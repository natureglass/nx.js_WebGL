#include <errno.h>
#include <inttypes.h>
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pixman.h>
#include <switch.h>
#include <turbojpeg.h>
#include <unistd.h>

#include "account.h"
#include "album.h"
#include "applet.h"
#include "async.h"
#include "audio.h"
#include "battery.h"
#include "canvas.h"
#include "compression.h"
#include "crypto.h"
#include "dns.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "fs.h"
#include "fsdev.h"
#include "gamepad.h"
#include "image.h"
#include "irs.h"
#include "memory.h"
#include "nifm.h"
#include "sensors.h"
#include "ns.h"
#include "poll.h"
#include "service.h"
#include "software-keyboard.h"
#include "tcp.h"
#include "tls.h"
#include "types.h"
#include "udp.h"
#include "uint8array.h"
#include "url.h"
#include "util.h"
#include "video.h"
#include "wasm.h"
#include "web.h"
#include "webgl.h"
#include "window.h"
#include "worker.h"

/* Centralised log path so all diagnostic output lands beside the
 * brewser profile's other logs. NOTE: this hardcodes the
 * brewser profile path into the SHARED nxjs runtime — any
 * other consumer of nxjs.nro will also write its debug log here. If
 * a different consumer needs its own location, factor this into a
 * runtime-settable path (e.g. via env or argv). */
#define LOG_FILENAME "sdmc:/switch/brewser/logs/nxjs-debug.log"

// Defined in runtime.c
extern const uint32_t qjsc_runtime_size;
extern const uint8_t qjsc_runtime[];

// 2026-06-07 ROUND 6: QuickJS interrupt-handler-based JS execution watchdog.
// The pvzge inGameScene freeze is in pure-JS code that doesn't call any
// of the methods we wrap from the page side, so JS-side instrumentation
// can't see what's running. This C-side handler is called by QuickJS
// every ~10000 bytecode opcodes during JS execution. Throttled to log
// once per ~1 second of wall time. Pulls `__pvzge_lastWrappedCall` from
// JS globals (a string updated by the page-side freeze-hooks wraps on
// every entry) to surface the most recent named JS activity. Pattern:
//   - counter still growing across log lines => JS is in a tight bytecode
//     loop, lastCall names the most recent wrapped fn entered before it
//   - counter stops growing => JS is blocked in native code (less likely)
//   - lastCall string === '(none)' or stale => the hung code never enters
//     any wrapped fn; expand wraps or accept that JS bytecode is the hang
static uint64_t nx_js_int_counter = 0;
static u64 nx_js_int_last_log_ns = 0;
static int nx_js_int_in_handler = 0;
static int nx_js_interrupt_handler(JSRuntime *rt, void *opaque) {
	(void)rt;
	if (nx_js_int_in_handler) return 0; // re-entry guard
	nx_js_int_counter++;
	// Cheap throttle: only check wall time every 256 interrupts (was 4096
	// in round 6 — too coarse, missed activity right before the freeze).
	if ((nx_js_int_counter & 0xFF) != 0) return 0;
	u64 now_ns = armTicksToNs(armGetSystemTick());
	if (nx_js_int_last_log_ns == 0) {
		nx_js_int_last_log_ns = now_ns;
		return 0;
	}
	u64 delta_ns = now_ns - nx_js_int_last_log_ns;
	// 2026-06-08 ROUND 45: raised from 50ms → 5s now that the freeze
	// investigation is closed and pvzge runs full gameplay. Keep the
	// probe as a low-cost liveness signal (so we can still detect a
	// hang in future games) but stop spamming the log 20×/sec.
	if (delta_ns < 5000ULL * 1000 * 1000) return 0; // < 5s
	nx_js_int_in_handler = 1;
	JSContext *ctx = (JSContext *)opaque;
	const char *last_call_str = NULL;
	JSValue glob = JS_GetGlobalObject(ctx);
	JSValue v = JS_GetPropertyStr(ctx, glob, "__pvzge_lastWrappedCall");
	if (JS_IsString(v)) {
		last_call_str = JS_ToCString(ctx, v);
	}
	fprintf(stderr,
			"[nxjs:js-interrupt] count=%llu wall_delta_ms=%llu lastCall=%s\n",
			(unsigned long long)nx_js_int_counter,
			(unsigned long long)(delta_ns / 1000000ULL),
			last_call_str ? last_call_str : "<unset>");
	fflush(stderr);
	if (last_call_str) JS_FreeCString(ctx, last_call_str);
	JS_FreeValue(ctx, v);
	JS_FreeValue(ctx, glob);
	nx_js_int_last_log_ns = now_ns;
	nx_js_int_in_handler = 0;
	return 0;
}

// 2026-06-17 boot-hang investigation (Citron intermittent black-screen-before-
// splash). Wall-clock-from-entry probe so we can bisect WHICH stage between
// libnx services init, JS bytecode load, JS module evaluation, and first
// framebufferInit hangs on the emulator. Cheap (fprintf+fflush per stage
// boundary; ~20 lines total per boot) so safe to leave in until the
// repro is understood. Macro logs ms since first call.
static u64 nx_boot_t0 = 0;
#define NX_BOOT_LOG(label) do { \
	u64 _t = armTicksToNs(armGetSystemTick()); \
	if (nx_boot_t0 == 0) nx_boot_t0 = _t; \
	fprintf(stderr, "[nxjs:boot] +%6llums %s\n", \
			(unsigned long long)((_t - nx_boot_t0) / 1000000ULL), label); \
	fflush(stderr); \
} while (0)

// Text renderer
static PrintConsole *print_console = NULL;

// Framebuffer renderer
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;
static u32 js_fb_width = 0;
static u32 js_fb_height = 0;

// 2026-06-17 Citron splash continuity: the decoded loading.jpg bytes are
// kept alive past `nx_render_loading_image` so `nx_framebuffer_init` can
// blit them into the new canvas->data buffer when the JS side first
// allocates a canvas. Without this, the first main-loop present after
// JS init copies canvas->data (zero-filled = black) into the FB,
// flashing away the C-side splash within a frame or two on warm-cache
// boots. Owned by C, allocated via tjAlloc (must use tjFree), one-shot
// (consumed + freed on first framebuffer_init that finds a non-null
// pointer). BGRA, no row padding (stride = w*4).
static uint8_t *s_loading_splash_bgra = NULL;
static int s_loading_splash_w = 0;
static int s_loading_splash_h = 0;

// Display buffer: page pixels with the cursor overlay composited on top.
// The Switch framebuffer memcpy reads from THIS buffer, not directly from
// `js_framebuffer` (= canvas->data). This keeps the cursor from ever
// landing in canvas->data — fixing the "ImageData save/restore corrupts
// page pixels" class of bug the JS-side dirty-rect approach hit. When
// no cursor is enabled the buffer is unused and we memcpy js_framebuffer
// directly. Sized to match js_framebuffer in `nx_framebuffer_init`.
static uint8_t *display_buffer = NULL;
static size_t display_buffer_size = 0;

// Cursor overlay state. JS sets the bitmap once (or whenever sprite size
// changes) and just updates x/y per cursor tick. The C-side memcpy step
// reads this every frame.
static bool cursor_overlay_enabled = false;
static int cursor_overlay_x = 0;
static int cursor_overlay_y = 0;
static int cursor_overlay_w = 0;
static int cursor_overlay_h = 0;
// Non-premultiplied RGBA8 bitmap, sized cursor_overlay_w * cursor_overlay_h * 4.
// Owned by C; (re)allocated whenever bitmap dimensions change.
static uint8_t *cursor_overlay_rgba = NULL;
static size_t cursor_overlay_rgba_size = 0;

void nx_console_init(nx_context_t *nx_ctx) {
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	if (print_console == NULL) {
		print_console = consoleInit(NULL);
	}
}

void nx_console_exit() {
	if (print_console != NULL) {
		consoleExit(print_console);
		print_console = NULL;
	}
}

static JSValue nx_framebuffer_init(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	NX_BOOT_LOG("nx_framebuffer_init enter");
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_console_exit();
	if (win == NULL) {
		// Retrieve the default window
		win = nwindowGetDefault();
	}
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
	}
	u32 width, height;
	nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);
	if (!canvas)
		return JS_EXCEPTION;
	width = canvas->width;
	height = canvas->height;
	js_framebuffer = canvas->data;
	js_fb_width = width;
	js_fb_height = height;
	// Continue the C-side splash: if `nx_render_loading_image` stashed a
	// decoded loading.jpg, blit it into canvas->data with center/letterbox
	// so the first main-loop present after this call shows the splash
	// instead of zeroed-black canvas backing. One-shot — consumed + freed
	// here so subsequent canvas resizes don't re-paint the splash.
	if (s_loading_splash_bgra && canvas->data) {
		const int fb_w = (int)width;
		const int fb_h = (int)height;
		const int img_w = s_loading_splash_w;
		const int img_h = s_loading_splash_h;
		const int copy_w = img_w < fb_w ? img_w : fb_w;
		const int copy_h = img_h < fb_h ? img_h : fb_h;
		const int off_x = (fb_w - copy_w) / 2;
		const int off_y = (fb_h - copy_h) / 2;
		const int src_off_x = img_w > fb_w ? (img_w - fb_w) / 2 : 0;
		const int src_off_y = img_h > fb_h ? (img_h - fb_h) / 2 : 0;
		const int src_stride = img_w * 4;
		const int dst_stride = fb_w * 4;
		uint8_t *dst = canvas->data;
		// canvas->data was just js_mallocz'd → already zero (black border).
		for (int y = 0; y < copy_h; y++) {
			const uint8_t *src_row = s_loading_splash_bgra +
				(size_t)(y + src_off_y) * src_stride + (size_t)src_off_x * 4;
			uint8_t *dst_row = dst +
				(size_t)(y + off_y) * dst_stride + (size_t)off_x * 4;
			memcpy(dst_row, src_row, (size_t)copy_w * 4);
		}
		tjFree(s_loading_splash_bgra);
		s_loading_splash_bgra = NULL;
		s_loading_splash_w = 0;
		s_loading_splash_h = 0;
		NX_BOOT_LOG("nx_framebuffer_init: splash blitted into canvas->data");
	}
	// Allocate / resize the display buffer alongside the page framebuffer
	// so the per-frame composite step always has a destination sized to
	// match js_framebuffer.
	size_t needed = (size_t)width * (size_t)height * 4;
	if (needed != display_buffer_size) {
		free(display_buffer);
		display_buffer = malloc(needed);
		display_buffer_size = display_buffer ? needed : 0;
	}
	NX_BOOT_LOG("nx_framebuffer_init: framebufferCreate pre");
	framebuffer = malloc(sizeof(Framebuffer));
	framebufferCreate(framebuffer, win, width, height, PIXEL_FORMAT_BGRA_8888,
					  2);
	framebufferMakeLinear(framebuffer);
	NX_BOOT_LOG("nx_framebuffer_init exit ok");
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
	return JS_UNDEFINED;
}

void nx_framebuffer_exit() {
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
		framebuffer = NULL;
		js_framebuffer = NULL;
	}
	if (display_buffer != NULL) {
		free(display_buffer);
		display_buffer = NULL;
		display_buffer_size = 0;
	}
	if (cursor_overlay_rgba != NULL) {
		free(cursor_overlay_rgba);
		cursor_overlay_rgba = NULL;
		cursor_overlay_rgba_size = 0;
	}
	cursor_overlay_enabled = false;
	cursor_overlay_w = 0;
	cursor_overlay_h = 0;
}

// ---- Cursor overlay -------------------------------------------------
//
// The cursor is composited into `display_buffer` at present time, NOT
// painted into canvas->data. This is the C-side replacement for the JS
// dirty-rect save/restore approach, which corrupted page pixels when the
// underlying surface was partially transparent (premultiplied 0 → memcpy
// → opaque black on the Switch framebuffer).
//
// JS hands us a non-premultiplied RGBA bitmap once (and re-sends it
// whenever the sprite changes shape). Position-only updates use the
// cheaper `setCursorOverlayPosition` to avoid copying the bitmap each
// time the joycon stick moves the cursor by one pixel.
//
// The composite math: src is non-premult RGBA; dst is premult ARGB32 in
// little-endian byte order (BGRA in memory) since cairo image surfaces
// match that layout on the Switch's LE arch. We src-over composite onto
// dst, producing premult BGRA — same format the screen FB expects.

static JSValue nx_set_cursor_overlay(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	if (argc < 5) {
		fprintf(stderr, "[nxjs:cursor] setCursorOverlay BAD argc=%d\n", argc);
		return JS_ThrowTypeError(ctx,
			"setCursorOverlay: expected (x, y, rgbaArrayBuffer, w, h)");
	}
	int x, y, w, h;
	if (JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1]) ||
		JS_ToInt32(ctx, &w, argv[3]) || JS_ToInt32(ctx, &h, argv[4])) {
		fprintf(stderr, "[nxjs:cursor] setCursorOverlay BAD int args\n");
		return JS_EXCEPTION;
	}
	if (w <= 0 || h <= 0) {
		// Treat zero-sized as a disable.
		fprintf(stderr, "[nxjs:cursor] setCursorOverlay disable (w=%d h=%d)\n", w, h);
		cursor_overlay_enabled = false;
		return JS_UNDEFINED;
	}

	// Resolve the bitmap source. Prefer the TypedArray path first because
	// ImageData.data is a Uint8ClampedArray (a TypedArray, not a raw
	// ArrayBuffer) — `JS_GetArrayBuffer(view)` returns NULL on a
	// TypedArray, but it's clearer to ask for the typed-array buffer
	// directly when the caller hands us a view. Falls back to direct
	// ArrayBuffer when the caller passes one already.
	size_t src_offset = 0, src_size = 0;
	uint8_t *src = NULL;
	size_t bpe = 0;
	JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[2], &src_offset,
	                                    &src_size, &bpe);
	if (!JS_IsException(ab)) {
		size_t ab_size = 0;
		uint8_t *ab_ptr = JS_GetArrayBuffer(ctx, &ab_size, ab);
		JS_FreeValue(ctx, ab);
		if (ab_ptr) {
			src = ab_ptr + src_offset;
		}
	} else {
		// Not a TypedArray; clear the pending exception and try ArrayBuffer.
		JS_FreeValue(ctx, JS_GetException(ctx));
		src = JS_GetArrayBuffer(ctx, &src_size, argv[2]);
		src_offset = 0;
	}
	if (!src) {
		fprintf(stderr, "[nxjs:cursor] setCursorOverlay rgba arg is not a Uint8Array/ArrayBuffer\n");
		return JS_ThrowTypeError(ctx,
			"setCursorOverlay: rgba arg is not a Uint8Array/ArrayBuffer");
	}
	size_t needed = (size_t)w * (size_t)h * 4;
	if (src_size < needed) {
		fprintf(stderr, "[nxjs:cursor] setCursorOverlay rgba too small needed=%zu got=%zu\n",
		        needed, src_size);
		return JS_ThrowRangeError(ctx,
			"setCursorOverlay: rgba buffer too small (need %zu, have %zu)",
			needed, src_size);
	}

	if (needed != cursor_overlay_rgba_size) {
		uint8_t *new_buf = realloc(cursor_overlay_rgba, needed);
		if (!new_buf) {
			return JS_ThrowOutOfMemory(ctx);
		}
		cursor_overlay_rgba = new_buf;
		cursor_overlay_rgba_size = needed;
	}
	memcpy(cursor_overlay_rgba, src, needed);
	cursor_overlay_x = x;
	cursor_overlay_y = y;
	cursor_overlay_w = w;
	cursor_overlay_h = h;
	cursor_overlay_enabled = true;
	// Sample a non-corner pixel so we can confirm the bitmap arrived
	// non-empty (default arrow pixel at ~(5,8) should be white-ish).
	{
		static int n = 0;
		++n;
		if (n <= 4) {
			size_t i = (size_t)8 * (size_t)w * 4 + 5 * 4;
			if (i + 3 < needed) {
				fprintf(stderr, "[nxjs:cursor] setCursorOverlay ok n=%d xy=(%d,%d) wh=%dx%d "
				        "sample[5,8]=(%u,%u,%u,%u)\n",
				        n, x, y, w, h,
				        cursor_overlay_rgba[i], cursor_overlay_rgba[i+1],
				        cursor_overlay_rgba[i+2], cursor_overlay_rgba[i+3]);
			}
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_set_cursor_overlay_position(JSContext *ctx,
											  JSValueConst this_val,
											  int argc, JSValueConst *argv) {
	if (argc < 2) {
		return JS_ThrowTypeError(ctx,
			"setCursorOverlayPosition: expected (x, y)");
	}
	int x, y;
	if (JS_ToInt32(ctx, &x, argv[0]) || JS_ToInt32(ctx, &y, argv[1])) {
		return JS_EXCEPTION;
	}
	cursor_overlay_x = x;
	cursor_overlay_y = y;
	{
		static int n = 0;
		++n;
		if (n <= 4 || (n % 60) == 0) {
			fprintf(stderr, "[nxjs:cursor] setCursorOverlayPosition n=%d xy=(%d,%d) enabled=%d\n",
			        n, x, y, (int)cursor_overlay_enabled);
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_clear_cursor_overlay(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	cursor_overlay_enabled = false;
	return JS_UNDEFINED;
}

// Composites the cursor bitmap onto `display_buffer` at (cursor_overlay_x,
// cursor_overlay_y), clamped to the framebuffer. Assumes
// `display_buffer` already holds a fresh copy of `js_framebuffer`.
static void composite_cursor_overlay(void) {
	if (!cursor_overlay_enabled || !cursor_overlay_rgba ||
		cursor_overlay_w <= 0 || cursor_overlay_h <= 0) {
		return;
	}
	int x0 = cursor_overlay_x;
	int y0 = cursor_overlay_y;
	int w = cursor_overlay_w;
	int h = cursor_overlay_h;
	int src_off_x = 0;
	int src_off_y = 0;
	if (x0 < 0) { src_off_x = -x0; w += x0; x0 = 0; }
	if (y0 < 0) { src_off_y = -y0; h += y0; y0 = 0; }
	if (x0 + w > (int)js_fb_width) w = (int)js_fb_width - x0;
	if (y0 + h > (int)js_fb_height) h = (int)js_fb_height - y0;
	if (w <= 0 || h <= 0) return;

	const int src_stride = cursor_overlay_w * 4;
	const int dst_stride = (int)js_fb_width * 4;
	for (int row = 0; row < h; row++) {
		const uint8_t *s = cursor_overlay_rgba +
		                   (src_off_y + row) * src_stride +
		                   src_off_x * 4;
		uint8_t *d = display_buffer + (y0 + row) * dst_stride + x0 * 4;
		for (int col = 0; col < w; col++) {
			uint8_t sr = s[0], sg = s[1], sb = s[2], sa = s[3];
			if (sa == 0) {
				// transparent — keep destination
			} else if (sa == 255) {
				// opaque — overwrite. dst is BGRA in memory.
				d[0] = sb;
				d[1] = sg;
				d[2] = sr;
				d[3] = 255;
			} else {
				// src-over: out = src*sa + dst*(1-sa). dst is premult BGRA.
				uint32_t inv = 255 - sa;
				uint32_t db = d[0], dg = d[1], dr = d[2], da = d[3];
				d[0] = (uint8_t)((sb * sa + db * inv + 127) / 255);
				d[1] = (uint8_t)((sg * sa + dg * inv + 127) / 255);
				d[2] = (uint8_t)((sr * sa + dr * inv + 127) / 255);
				d[3] = (uint8_t)((255u * sa + da * inv + 127) / 255);
			}
			s += 4;
			d += 4;
		}
	}
}

uint8_t *read_file(const char *filename, size_t *out_size) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = malloc(size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}

	size_t result = fread(buffer, 1, size, file);
	fclose(file);

	if (result != size) {
		free(buffer);
		return NULL;
	}

	*out_size = size;

	// NULL terminate the buffer, to work around bug in QuickJS
	// eval where doesn't respect the provided buffer size
	buffer[size] = '\0';

	return buffer;
}

bool delete_if_empty(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		return false;
	}

	// Seek to the end of the file
	fseek(file, 0, SEEK_END);

	// Get the file size
	long size = ftell(file);

	// Close the file
	fclose(file);

	// If the file is empty, delete it
	if (size == 0) {
		if (remove(filename) == 0) {
			return true;
		} else {
			return false;
		}
	}

	return true;
}

static int is_running = 1;

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	is_running = 0;
	return JS_UNDEFINED;
}

// Function to cleanly exit the main event loop (for use by other modules)
void nx_exit_event_loop(void) {
	is_running = 0;
}

static SocketInitConfig const s_socketInitConfig = {
	.tcp_tx_buf_size = 1 * 1024 * 1024,
	.tcp_rx_buf_size = 1 * 1024 * 1024,
	.tcp_tx_buf_max_size = 4 * 1024 * 1024,
	.tcp_rx_buf_max_size = 4 * 1024 * 1024,

	.udp_tx_buf_size = 0x2400,
	.udp_rx_buf_size = 0xA500,

	.sb_efficiency = 8,

	.num_bsd_sessions = 3,
	.bsd_service_type = BsdServiceType_Auto,
};

// 2026-06-17 lazy `socketInitialize` (Citron boot-hang fix).
// `socketInitialize` was previously called eagerly from `main()` BEFORE
// `freopen` of the log file. On Citron the BSD service emulation
// intermittently stalls during the buffer reservation (10MB + sb_efficiency=8
// + 3 sessions), leaving boot frozen on a black-frame "splash" that
// `nx_render_loading_image` had just presented. Launching a lighter NRO
// first warmed up Citron's BSD path so the brewser relaunch found the
// service primed — a workaround, not a fix.
//
// Lazy approach: the home page is local (`brewser://`), the constructor
// wires up loaders but doesn't fire any network requests, and
// `probeNetwork()` is explicitly fire-and-forget. So we can defer
// `socketInitialize` until the first JS-side BSD entry point
// (`tcp.connect`, `tcp.tcpServerInit/New`, `udp.udpInit/New`,
// `dns.dnsResolve`). On real HW the call returns in <1ms; on Citron the
// stall — if it still happens — now happens AFTER the brewser-logo
// splash has painted, so the user sees a splash with a brief delay
// rather than a black screen of unknown duration.
//
// Idempotent. Returns true on success / when already initialized;
// false if `socketInitialize` failed (and stamps an `[nxjs:net]` line
// in the debug log). Call sites should JS_ThrowInternalError on false.
static bool s_socket_initialized = false;
bool nx_ensure_socket_initialized(void) {
	if (s_socket_initialized) return true;
	u64 _t_pre = armTicksToNs(armGetSystemTick());
	fprintf(stderr, "[nxjs:net] socketInitialize pre (lazy)\n");
	fflush(stderr);
	Result rc = socketInitialize(&s_socketInitConfig);
	u64 _t_post = armTicksToNs(armGetSystemTick());
	if (R_FAILED(rc)) {
		fprintf(stderr,
		        "[nxjs:net] socketInitialize FAILED rc=0x%x after %llums\n",
		        rc, (unsigned long long)((_t_post - _t_pre) / 1000000ULL));
		fflush(stderr);
		return false;
	}
	fprintf(stderr, "[nxjs:net] socketInitialize ok in %llums\n",
	        (unsigned long long)((_t_post - _t_pre) / 1000000ULL));
	fflush(stderr);
	s_socket_initialized = true;
	return true;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (nx_ctx->rendering_mode != NX_RENDERING_MODE_CONSOLE) {
		nx_framebuffer_exit();
		nx_console_init(nx_ctx);
	}
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str) return JS_EXCEPTION;
	printf("%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_print_err(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str) return JS_EXCEPTION;
	fprintf(stderr, "%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_cwd(JSContext *ctx, JSValueConst this_val, int argc,
					  JSValueConst *argv) {
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		char final_path[1036];

		// Emulators such as Ryujinx don't have
		// the "sdmc:" prefix, so add it
		if (cwd[0] == '/') {
			snprintf(final_path, sizeof(final_path), "sdmc:%s", cwd);
		} else {
			snprintf(final_path, sizeof(final_path), "%s", cwd);
		}

		// Ensure the path ends with a trailing slash
		// so that it works nicely with `new URL()`
		size_t len = strlen(final_path);
		if (len > 0 && final_path[len - 1] != '/') {
			final_path[len] = '/';
			final_path[len + 1] = '\0';
		}

		return JS_NewString(ctx, final_path);
	}
	return JS_UNDEFINED;
}

static JSValue js_chdir(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	const char *dir = JS_ToCString(ctx, argv[0]);
	if (chdir(dir) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), dir);
		JS_FreeCString(ctx, dir);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, dir);
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_touch_screen(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	hidInitializeTouchScreen();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_keyboard(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	hidInitializeKeyboard();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_vibration_devices(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	Result rc = hidInitializeVibrationDevices(
		nx_ctx->vibration_device_handles, 2,
		// TODO: handle No1 gamepad
		HidNpadIdType_Handheld, HidNpadStyleSet_NpadStandard);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(
			ctx, "hidInitializeVibrationDevices() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_send_vibration_values(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	HidVibrationValue VibrationValues[2];
	JSValue low_amp_value = JS_GetPropertyStr(ctx, argv[0], "lowAmp");
	JSValue low_freq_value = JS_GetPropertyStr(ctx, argv[0], "lowFreq");
	JSValue high_amp_value = JS_GetPropertyStr(ctx, argv[0], "highAmp");
	JSValue high_freq_value = JS_GetPropertyStr(ctx, argv[0], "highFreq");
	double low_amp, low_freq, high_amp, high_freq;
	int err = JS_ToFloat64(ctx, &low_amp, low_amp_value) ||
			  JS_ToFloat64(ctx, &low_freq, low_freq_value) ||
			  JS_ToFloat64(ctx, &high_amp, high_amp_value) ||
			  JS_ToFloat64(ctx, &high_freq, high_freq_value);
	JS_FreeValue(ctx, low_amp_value);
	JS_FreeValue(ctx, low_freq_value);
	JS_FreeValue(ctx, high_amp_value);
	JS_FreeValue(ctx, high_freq_value);
	if (err) {
		return JS_EXCEPTION;
	}
	VibrationValues[0].freq_low = low_freq;
	VibrationValues[0].amp_low = low_amp;
	VibrationValues[0].freq_high = high_freq;
	VibrationValues[0].amp_high = high_amp;
	memcpy(&VibrationValues[1], &VibrationValues[0], sizeof(HidVibrationValue));

	Result rc = hidSendVibrationValues(nx_ctx->vibration_device_handles,
									   VibrationValues, 2);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "hidSendVibrationValues() returned 0x%x",
							  rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_get_touch_screen_states(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	HidTouchScreenState state = {0};
	hidGetTouchScreenStates(&state, 1);
	if (state.count == 0) {
		return JS_UNDEFINED;
	}
	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < state.count; i++) {
		JSValue touch = JS_NewObject(ctx);
		JSValue x = JS_NewInt32(ctx, state.touches[i].x);
		JSValue y = JS_NewInt32(ctx, state.touches[i].y);
		JS_SetPropertyUint32(ctx, arr, i, touch);
		JS_SetPropertyStr(ctx, touch, "identifier",
						  JS_NewInt32(ctx, state.touches[i].finger_id));
		JS_SetPropertyStr(ctx, touch, "clientX", x);
		JS_SetPropertyStr(ctx, touch, "clientY", y);
		JS_SetPropertyStr(ctx, touch, "screenX", x);
		JS_SetPropertyStr(ctx, touch, "screenY", y);
		JS_SetPropertyStr(
			ctx, touch, "radiusX",
			JS_NewFloat64(ctx, (double)state.touches[i].diameter_x / 2.0));
		JS_SetPropertyStr(
			ctx, touch, "radiusY",
			JS_NewFloat64(ctx, (double)state.touches[i].diameter_y / 2.0));
		JS_SetPropertyStr(ctx, touch, "rotationAngle",
						  JS_NewInt32(ctx, state.touches[i].rotation_angle));
	}
	return arr;
}

static JSValue js_hid_get_keyboard_states(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	HidKeyboardState state = {0};
	hidGetKeyboardStates(&state, 1);
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "modifiers",
					  JS_NewBigUint64(ctx, state.modifiers));
	for (int i = 0; i < 4; i++) {
		JS_SetPropertyUint32(ctx, obj, i, JS_NewBigUint64(ctx, state.keys[i]));
	}
	return obj;
}

static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	char *value = getenv(name);
	if (value == NULL) {
		if (errno) {
			JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
			JS_FreeCString(ctx, name);
			return JS_EXCEPTION;
		}
		JS_FreeCString(ctx, name);
		return JS_UNDEFINED;
	}
	JS_FreeCString(ctx, name);
	return JS_NewString(ctx, value);
}

static JSValue js_setenv(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	const char *value = JS_ToCString(ctx, argv[1]);
	if (setenv(name, value, 1) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s=%s", strerror(errno), name, value);
		JS_FreeCString(ctx, name);
		JS_FreeCString(ctx, value);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue js_unsetenv(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	if (unsetenv(name) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue js_env_to_object(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	JSValue env = JS_NewObject(ctx);

	// Get the environment variables from the operating system
	extern char **environ;
	char **envp = environ;
	while (*envp) {
		// Split each environment variable into a key-value pair
		char *key = strdup(*envp);
		char *eq = strchr(key, '=');
		if (eq) {
			*eq = '\0';
			char *value = eq + 1;

			JS_SetPropertyStr(ctx, env, key, JS_NewString(ctx, value));
		}
		free(key);
		envp++;
	}

	return env;
}

// Returns the internal state of a Promise instance.
static JSValue js_get_internal_promise_state(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	JSPromiseStateEnum state = JS_PromiseState(ctx, argv[0]);
	JSValue arr = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, arr, 0, JS_NewUint32(ctx, state));
	if (state > JS_PROMISE_PENDING) {
		JS_SetPropertyUint32(ctx, arr, 1, JS_PromiseResult(ctx, argv[0]));
	} else {
		JS_SetPropertyUint32(ctx, arr, 1, JS_NULL);
	}
	return arr;
}

static JSValue nx_set_frame_handler(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->frame_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_set_exit_handler(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->exit_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_version_get_ams(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	if (!hosversionIsAtmosphere()) {
		return JS_UNDEFINED;
	}

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (!nx_ctx->spl_initialized) {
		nx_ctx->spl_initialized = true;
		splInitialize();
	}

	u64 packed_version;
	Result rc = splGetConfig((SplConfigItem)65000, &packed_version);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc,
									"splGetConfig(ExosphereApiVersion)");
	}
	u8 major_version = (packed_version >> 56) & 0xFF;
	u8 minor_version = (packed_version >> 48) & 0xFF;
	u8 micro_version = (packed_version >> 40) & 0xFF;
	char version_str[12];
	snprintf(version_str, 12, "%u.%u.%u", major_version, minor_version,
			 micro_version);
	return JS_NewString(ctx, version_str);
}

static JSValue nx_version_get_emummc(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	if (!hosversionIsAtmosphere()) {
		return JS_UNDEFINED;
	}

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (!nx_ctx->spl_initialized) {
		nx_ctx->spl_initialized = true;
		splInitialize();
	}

	u64 is_emummc;
	Result rc = splGetConfig((SplConfigItem)65007, &is_emummc);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc,
									"splGetConfig(ExosphereEmummcType)");
	}
	return JS_NewBool(ctx, is_emummc ? true : false);
}

int nx_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
							  const char *url, bool is_main) {
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JSValue meta_obj = JS_GetImportMeta(ctx, m);
	if (JS_IsException(meta_obj))
		return -1;
	JS_DefinePropertyValueStr(ctx, meta_obj, "url", JS_NewString(ctx, url),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, meta_obj, "main", JS_NewBool(ctx, is_main),
							  JS_PROP_C_W_E);
	JS_FreeValue(ctx, meta_obj);
	return 0;
}

void nx_process_pending_jobs(JSContext *ctx, nx_context_t *nx_ctx,
							 JSRuntime *rt) {
	JSContext *ctx1;
	int err;
	// Don't allow an infinite number of pending jobs
	// to allow the UI to update periodically. The number
	// of iterations was chosen arbitrarily - maybe could
	// be optimized by using a timer instead of a fixed
	// number of iterations.
	for (u8 i = 0; i < 20; i++) {
		err = JS_ExecutePendingJob(rt, &ctx1);
		if (err <= 0) {
			if (err < 0) {
				nx_emit_error_event(ctx1);
			}
			break;
		}
	}

	if (!JS_IsUndefined(nx_ctx->unhandled_rejected_promise)) {
		nx_emit_unhandled_rejection_event(ctx);
	}
}

void nx_render_loading_image(nx_context_t *nx_ctx, const char *nro_path) {
	// Check if there is a `loading.jpg` file on the RomFS
	// and render that to the screen if present. Path is the brewser-shell
	// assets directory (moved from romfs root 2026-06-17 to keep
	// shell-owned assets co-located).
	size_t loading_image_size;
	const char *loading_image_path = "romfs:/shell/assets/loading.jpg";
	uint8_t *loading_image = (uint8_t *)read_file(loading_image_path, &loading_image_size);
	if (!loading_image && nro_path) {
		// RomFS loading_image image not found.
		// Try to load from SD card, relative to the path of the NRO.
		char *temp_loading_image_path = strdup(nro_path);
		if (temp_loading_image_path) {
			replace_file_extension(temp_loading_image_path, ".jpg");
			loading_image = (uint8_t *)read_file(temp_loading_image_path, &loading_image_size);
			free(temp_loading_image_path);
		}
	}
	if (loading_image == NULL) {
		// 2026-06-07: no loading.jpg shipped. Without this, the first frame
		// presented after framebuffer-create shows uninit Tegra GPU memory
		// (commonly leftover red from a previous app's buffers, easily
		// confused for an engine bug). Allocate the framebuffer just long
		// enough to push a single solid-black frame, then tear it down so
		// the normal init path can take ownership cleanly.
		NX_BOOT_LOG("loading_image NULL path: nwindowGetDefault");
		win = nwindowGetDefault();
		int blank_width = 1280;
		int blank_height = 720;
		NX_BOOT_LOG("loading_image NULL path: framebufferCreate pre");
		framebuffer = malloc(sizeof(Framebuffer));
		if (framebuffer) {
			framebufferCreate(framebuffer, win, blank_width, blank_height,
			                  PIXEL_FORMAT_BGRA_8888, 2);
			framebufferMakeLinear(framebuffer);
			NX_BOOT_LOG("loading_image NULL path: framebufferCreate ok");
			// Switch framebufferCreate sets up a double-buffer swapchain.
			// A single present leaves the OTHER buffer holding uninit Tegra
			// GPU memory (leftover red from a previous app), which surfaces
			// on the next vsync after the JS runtime initializes its own
			// framebuffer. Present TWO cleared frames so BOTH back-buffers
			// are zeroed before tearing down.
			for (int i = 0; i < 2; i++) {
				u32 stride;
				u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
				if (framebuf) {
					memset(framebuf, 0, (size_t)blank_height * stride);
				}
				framebufferEnd(framebuffer);
				if (i == 0) NX_BOOT_LOG("loading_image NULL path: present 1");
				else NX_BOOT_LOG("loading_image NULL path: present 2");
			}
			NX_BOOT_LOG("loading_image NULL path: framebufferClose pre");
			framebufferClose(framebuffer);
			free(framebuffer);
			framebuffer = NULL;
			NX_BOOT_LOG("loading_image NULL path: framebufferClose ok");
		}
		return;
	}
	if (loading_image != NULL) {
		NX_BOOT_LOG("nx_render_loading_image: jpeg decode + present");
		win = nwindowGetDefault();
		const int fb_w = 1280;
		const int fb_h = 720;
		int img_w = fb_w;
		int img_h = fb_h;
		js_framebuffer = NULL;
		framebuffer = malloc(sizeof(Framebuffer));
		framebufferCreate(framebuffer, win, fb_w, fb_h, PIXEL_FORMAT_BGRA_8888, 2);
		framebufferMakeLinear(framebuffer);

		if (decode_jpeg(loading_image, loading_image_size, &js_framebuffer,
		                &img_w, &img_h) == 0 && js_framebuffer != NULL) {
			// Center + letterbox the decoded image into the 1280×720 FB,
			// honouring stride. The original code `memcpy`'d the entire
			// decoded buffer flat into the FB, which sheared any image
			// that wasn't exactly 1280×720 (row width ≠ stride width).
			u32 stride;
			u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
			if (framebuf) {
				// Black background fill (one row at a time so stride padding
				// stays black even when the image is smaller).
				memset(framebuf, 0, (size_t)fb_h * stride);
				// Clip image dims to FB and center.
				int copy_w = img_w < fb_w ? img_w : fb_w;
				int copy_h = img_h < fb_h ? img_h : fb_h;
				int off_x = (fb_w - copy_w) / 2;
				int off_y = (fb_h - copy_h) / 2;
				// Source side may be wider than the FB; advance into the
				// source row by the left-side crop offset (negative letterbox
				// case = image bigger than FB on a dimension).
				int src_off_x = img_w > fb_w ? (img_w - fb_w) / 2 : 0;
				int src_off_y = img_h > fb_h ? (img_h - fb_h) / 2 : 0;
				const int src_stride = img_w * 4;
				for (int y = 0; y < copy_h; y++) {
					const u8 *src_row =
						js_framebuffer + (size_t)(y + src_off_y) * src_stride +
						(size_t)src_off_x * 4;
					u8 *dst_row =
						framebuf + (size_t)(y + off_y) * stride +
						(size_t)off_x * 4;
					memcpy(dst_row, src_row, (size_t)copy_w * 4);
				}
			}
			framebufferEnd(framebuffer);
			NX_BOOT_LOG("nx_render_loading_image: present ok");
			// Stash the decoded splash so `nx_framebuffer_init` can
			// pre-fill the JS canvas backing buffer with the same image
			// — otherwise the first main-loop present after JS init
			// flashes black over the splash on warm-cache boots.
			s_loading_splash_bgra = js_framebuffer;
			s_loading_splash_w = img_w;
			s_loading_splash_h = img_h;
			js_framebuffer = NULL;  // ownership transferred to s_loading_splash_*
		} else {
			NX_BOOT_LOG("nx_render_loading_image: decode failed (skipped)");
			tjFree(js_framebuffer);
			js_framebuffer = NULL;
		}

		free(loading_image);

		framebufferClose(framebuffer);
		free(framebuffer);
		framebuffer = NULL;
	}
}

// Main program entrypoint
int main(int argc, char *argv[]) {
	Result rc;

	// 2026-06-17 boot-hang investigation: open the log FIRST so the boot
	// probes captured below survive a hang in any of the libnx init steps
	// (romfs / socket / pl). Existing call site (after plInitialize) moved
	// here so NX_BOOT_LOG output lands in the file from `+0ms` onward.
	// Original semantics preserved: same path, same "w" mode, same fd kept
	// for `fclose` at shutdown.
	FILE *debug_fd = freopen(LOG_FILENAME, "w", stderr);
	NX_BOOT_LOG("entry");

	nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
	memset(nx_ctx, 0, sizeof(nx_context_t));

	NX_BOOT_LOG("romfsInit pre");
	rc = romfsInit();
	if (R_FAILED(rc)) {
		diagAbortWithResult(rc);
	}
	NX_BOOT_LOG("romfsInit ok");

	NX_BOOT_LOG("nx_render_loading_image pre");
	nx_render_loading_image(nx_ctx, argc > 0 ? argv[0] : NULL);
	NX_BOOT_LOG("nx_render_loading_image ok");

	// 2026-06-17: `socketInitialize` is now lazy (see
	// `nx_ensure_socket_initialized` above). On Citron the eager call
	// here intermittently hung during BSD service buffer reservation,
	// freezing boot on a black-frame placeholder before the brewser-logo
	// splash painted. JS-side network entry points (tcp.connect, udp.*,
	// dns.dnsResolve) gate on the lazy ensure helper.
	NX_BOOT_LOG("socketInitialize: deferred to first network use");

	NX_BOOT_LOG("plInitialize pre");
	rc = plInitialize(PlServiceType_User);
	if (R_FAILED(rc)) {
		diagAbortWithResult(rc);
	}
	NX_BOOT_LOG("plInitialize ok");

	NX_BOOT_LOG("JS_NewRuntime pre");
	JSRuntime *rt = JS_NewRuntime();
	JSContext *ctx = JS_NewContext(rt);
	NX_BOOT_LOG("JS_NewRuntime+JS_NewContext ok");

	// 2026-06-07 ROUND 6: install JS execution watchdog (see nx_js_interrupt_handler
	// near top of this file for full docs). Logs `[nxjs:js-interrupt]` lines to
	// nxjs-debug.log once per ~1s while JS bytecode is executing, so the pvzge
	// inGameScene freeze becomes observable from C side even when no JS-side
	// wrap fires.
	JS_SetInterruptHandler(rt, nx_js_interrupt_handler, ctx);

	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;
	nx_ctx->thpool = thpool_init(4);
	nx_ctx->frame_handler = JS_UNDEFINED;
	nx_ctx->exit_handler = JS_UNDEFINED;
	nx_ctx->error_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejection_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejected_promise = JS_UNDEFINED;
	pthread_mutex_init(&(nx_ctx->async_done_mutex), NULL);
	JS_SetContextOpaque(ctx, nx_ctx);
	JS_SetRuntimeOpaque(rt, nx_ctx);
	JS_SetHostPromiseRejectionTracker(rt, nx_promise_rejection_handler, ctx);

	padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);
	padInitializeDefault(&nx_ctx->pads[0]);
	padInitialize(&nx_ctx->pads[1], HidNpadIdType_No2);
	padInitialize(&nx_ctx->pads[2], HidNpadIdType_No3);
	padInitialize(&nx_ctx->pads[3], HidNpadIdType_No4);
	padInitialize(&nx_ctx->pads[4], HidNpadIdType_No5);
	padInitialize(&nx_ctx->pads[5], HidNpadIdType_No6);
	padInitialize(&nx_ctx->pads[6], HidNpadIdType_No7);
	padInitialize(&nx_ctx->pads[7], HidNpadIdType_No8);

	// First try the `main.jsc` file on the RomFS, which should
	// contain the bytecode entrypoint compiled with `qjsc -b`
	size_t user_code_size;
	bool user_path_needs_free = false;
	bool user_code_is_bytecode = true;
	char *user_code_path = "romfs:/main.jsc";
	NX_BOOT_LOG("read_file(romfs:/main.jsc) pre");
	char *user_code = (char *)read_file(user_code_path, &user_code_size);
	{
		char _msg[96];
		snprintf(_msg, sizeof(_msg), "read_file(romfs:/main.jsc) ok size=%zu",
				 user_code ? user_code_size : 0);
		NX_BOOT_LOG(_msg);
	}

	if (user_code == NULL && errno == ENOENT) {
		// If no `main.jsc`, then try the `main.js` file on the RomFS
		user_code_is_bytecode = false;
		user_code_path = "romfs:/main.js";
		user_code = (char *)read_file(user_code_path, &user_code_size);
	}

	if (user_code == NULL && errno == ENOENT && argc > 0) {
		// If no `main.js`, then try the `.js` file with the matching name
		// as the `.nro` file on the SD card
		user_path_needs_free = true;
		user_code_path = strdup(argv[0]);
		if (user_code_path) {
			replace_file_extension(user_code_path, ".js");
			user_code = (char *)read_file(user_code_path, &user_code_size);
		}
	}

	if (user_code == NULL) {
		nx_console_init(nx_ctx);
		printf("%s: %s\n", strerror(errno), user_code_path);
		if (user_path_needs_free) {
			free(user_code_path);
		}
		nx_ctx->had_error = 1;
		goto main_loop;
	}

	// The internal `$` object contains native functions that are wrapped in the
	// JS runtime
	NX_BOOT_LOG("nx_init_* block pre");
	JSValue global_obj = JS_GetGlobalObject(ctx);
	nx_ctx->init_obj = JS_NewObject(ctx);
	nx_init_account(ctx, nx_ctx->init_obj);
	nx_init_album(ctx, nx_ctx->init_obj);
	nx_init_applet(ctx, nx_ctx->init_obj);
	nx_init_audio(ctx, nx_ctx->init_obj);
	nx_init_battery(ctx, nx_ctx->init_obj);
	nx_init_canvas(ctx, nx_ctx->init_obj);
	nx_init_compression(ctx, nx_ctx->init_obj);
	nx_init_crypto(ctx, nx_ctx->init_obj);
	nx_init_dns(ctx, nx_ctx->init_obj);
	nx_init_dommatrix(ctx, nx_ctx->init_obj);
	nx_init_error(ctx, nx_ctx->init_obj);
	nx_init_font(ctx, nx_ctx->init_obj);
	nx_init_fs(ctx, nx_ctx->init_obj);
	nx_init_fsdev(ctx, nx_ctx->init_obj);
	nx_init_gamepad(ctx, nx_ctx->init_obj);
	nx_init_image(ctx, nx_ctx->init_obj);
	nx_init_irs(ctx, nx_ctx->init_obj);
	nx_init_memory(ctx, nx_ctx->init_obj);
	nx_init_nifm(ctx, nx_ctx->init_obj);
	nx_init_sensors(ctx, nx_ctx->init_obj);
	nx_init_ns(ctx, nx_ctx->init_obj);
	nx_init_service(ctx, nx_ctx->init_obj);
	nx_init_tcp(ctx, nx_ctx->init_obj);
	nx_init_tls(ctx, nx_ctx->init_obj);
	nx_init_udp(ctx, nx_ctx->init_obj);
	nx_init_uint8array(ctx, nx_ctx->init_obj);
	nx_init_url(ctx, nx_ctx->init_obj);
	nx_init_swkbd(ctx, nx_ctx->init_obj);
	nx_init_video(ctx, nx_ctx->init_obj);
	nx_init_wasm(ctx, nx_ctx->init_obj);
	nx_init_web(ctx, nx_ctx->init_obj);
	nx_init_webgl(ctx, nx_ctx->init_obj);
	nx_init_window(ctx, nx_ctx->init_obj);
	nx_init_worker(ctx, nx_ctx->init_obj);
	const JSCFunctionListEntry init_function_list[] = {
		JS_CFUNC_DEF("exit", 0, js_exit),
		JS_CFUNC_DEF("cwd", 0, js_cwd),
		JS_CFUNC_DEF("chdir", 1, js_chdir),
		JS_CFUNC_DEF("print", 1, js_print),
		JS_CFUNC_DEF("printErr", 1, js_print_err),
		JS_CFUNC_DEF("getInternalPromiseState", 1,
					 js_get_internal_promise_state),
		JS_CFUNC_DEF("hidInitializeTouchScreen", 0,
					 js_hid_initialize_touch_screen),
		JS_CFUNC_DEF("hidGetTouchScreenStates", 0,
					 js_hid_get_touch_screen_states),

		// env vars
		JS_CFUNC_DEF("getenv", 1, js_getenv),
		JS_CFUNC_DEF("setenv", 2, js_setenv),
		JS_CFUNC_DEF("unsetenv", 1, js_unsetenv),
		JS_CFUNC_DEF("envToObject", 0, js_env_to_object),

		JS_CFUNC_DEF("onExit", 1, nx_set_exit_handler),
		JS_CFUNC_DEF("onFrame", 1, nx_set_frame_handler),

		// framebuffer renderer
		JS_CFUNC_DEF("framebufferInit", 1, nx_framebuffer_init),

		// cursor overlay (composited into the display buffer at present
		// time so the cursor visual never lands in canvas->data — see
		// `composite_cursor_overlay`).
		JS_CFUNC_DEF("setCursorOverlay", 5, nx_set_cursor_overlay),
		JS_CFUNC_DEF("setCursorOverlayPosition", 2, nx_set_cursor_overlay_position),
		JS_CFUNC_DEF("clearCursorOverlay", 0, nx_clear_cursor_overlay),

		// hid
		JS_CFUNC_DEF("hidInitializeKeyboard", 0, js_hid_initialize_keyboard),
		JS_CFUNC_DEF("hidInitializeVibrationDevices", 0,
					 js_hid_initialize_vibration_devices),
		JS_CFUNC_DEF("hidGetKeyboardStates", 0, js_hid_get_keyboard_states),
		JS_CFUNC_DEF("hidSendVibrationValues", 0, js_hid_send_vibration_values),
	};
	JS_SetPropertyFunctionList(ctx, nx_ctx->init_obj, init_function_list,
							   countof(init_function_list));
	NX_BOOT_LOG("nx_init_* block ok");

	// `Switch.version`
	JSAtom atom;
	char version_str[12];
	JSValue version_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, version_obj, "ada", JS_NewString(ctx, "2.9.2"));
	NX_DEF_GET_(version_obj, "ams", nx_version_get_ams, JS_PROP_C_W_E);
	JS_SetPropertyStr(ctx, version_obj, "cairo",
					  JS_NewString(ctx, cairo_version_string()));
	NX_DEF_GET_(version_obj, "emummc", nx_version_get_emummc, JS_PROP_C_W_E);
	JS_SetPropertyStr(ctx, version_obj, "freetype2",
					  JS_NewString(ctx, FREETYPE_VERSION_STR));
	JS_SetPropertyStr(ctx, version_obj, "harfbuzz",
					  JS_NewString(ctx, HB_VERSION_STRING));
	u32 hos_version = hosversionGet();
	snprintf(version_str, 12, "%d.%d.%d", HOSVER_MAJOR(hos_version),
			 HOSVER_MINOR(hos_version), HOSVER_MICRO(hos_version));
	JS_SetPropertyStr(ctx, version_obj, "hos", JS_NewString(ctx, version_str));
	JS_SetPropertyStr(ctx, version_obj, "libnx",
					  JS_NewString(ctx, LIBNX_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "mbedtls",
					  JS_NewString(ctx, MBEDTLS_VERSION_STRING));
	JS_SetPropertyStr(ctx, version_obj, "nxjs",
					  JS_NewString(ctx, NXJS_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "pixman",
					  JS_NewString(ctx, pixman_version_string()));
	JS_SetPropertyStr(ctx, version_obj, "png",
					  JS_NewString(ctx, PNG_LIBPNG_VER_STRING));
	JS_SetPropertyStr(ctx, version_obj, "quickjs",
					  JS_NewString(ctx, JS_GetVersion()));
	JS_SetPropertyStr(ctx, version_obj, "turbojpeg",
					  JS_NewString(ctx, LIBTURBOJPEG_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "wasm3", JS_NewString(ctx, M3_VERSION));
	const int webp_version = WebPGetDecoderVersion();
	snprintf(version_str, 12, "%d.%d.%d", (webp_version >> 16) & 0xFF,
			 (webp_version >> 8) & 0xFF, webp_version & 0xFF);
	JS_SetPropertyStr(ctx, version_obj, "webp", JS_NewString(ctx, version_str));
	JS_SetPropertyStr(ctx, version_obj, "zlib",
					  JS_NewString(ctx, zlibVersion()));
	JS_SetPropertyStr(ctx, version_obj, "zstd",
					  JS_NewString(ctx, ZSTD_versionString()));
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "version", version_obj);

	// `Switch.entrypoint`
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "entrypoint",
					  JS_NewString(ctx, user_code_path));

	// `Switch.argv`
	JSValue argv_array = JS_NewArray(ctx);
	for (int i = 0; i < argc; i++) {
		JS_SetPropertyUint32(ctx, argv_array, i, JS_NewString(ctx, argv[i]));
	}
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "argv", argv_array);

	JS_SetPropertyStr(ctx, global_obj, "$", nx_ctx->init_obj);

	// Initialize runtime
	NX_BOOT_LOG("qjsc_runtime eval pre");
	JSValue runtime_init_func, runtime_init_result;
	runtime_init_func = JS_ReadObject(ctx, qjsc_runtime, qjsc_runtime_size,
									  JS_READ_OBJ_BYTECODE);
	if (JS_IsException(runtime_init_func)) {
		print_js_error(ctx);
		nx_ctx->had_error = 1;
		goto main_loop;
	}
	runtime_init_result = JS_EvalFunction(ctx, runtime_init_func);
	if (JS_IsException(runtime_init_result)) {
		nx_console_init(nx_ctx);
		printf("Runtime initialization failed\n");
		print_js_error(ctx);
		nx_ctx->had_error = 1;
		goto main_loop;
	}
	JS_FreeValue(ctx, runtime_init_result);
	NX_BOOT_LOG("qjsc_runtime eval ok");

	// Run the user code
	NX_BOOT_LOG("main.jsc eval pre");
	JSValue user_code_result;
	if (user_code_is_bytecode) {
		user_code_result = JS_ReadObject(ctx, (const u8 *)user_code,
										 user_code_size, JS_READ_OBJ_BYTECODE);
		if (JS_IsException(user_code_result)) {
			nx_emit_error_event(ctx);
		} else {
			nx_module_set_import_meta(ctx, user_code_result, user_code_path,
									  true);
			user_code_result = JS_EvalFunction(ctx, user_code_result);
			if (JS_IsException(user_code_result)) {
				nx_emit_error_event(ctx);
			}
		}
	} else {
		user_code_result =
			JS_Eval(ctx, user_code, user_code_size, user_code_path,
					JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
		if (JS_IsException(user_code_result)) {
			nx_emit_error_event(ctx);
		} else {
			nx_module_set_import_meta(ctx, user_code_result, user_code_path,
									  true);
			user_code_result = JS_EvalFunction(ctx, user_code_result);
			if (JS_IsException(user_code_result)) {
				nx_emit_error_event(ctx);
			}
		}
	}
	JS_FreeValue(ctx, user_code_result);
	free(user_code);
	if (user_path_needs_free) {
		free(user_code_path);
	}
	NX_BOOT_LOG("main.jsc eval ok");

main_loop:
	NX_BOOT_LOG("main_loop enter");
	while (appletMainLoop()) {
		if (!nx_ctx->had_error) {
			// Check if any file descriptors have reported activity
			nx_poll(&nx_ctx->poll);
		}

		if (!nx_ctx->had_error) {
			// Check if any thread pool tasks have completed
			nx_process_async(ctx, nx_ctx);
		}

		if (!nx_ctx->had_error) {
			// Drain outbound message queues from any active Web Workers
			// and dispatch each into the main JS side. Cheap when no
			// workers exist. See [[project-swb-web-workers-milestone]].
			nx_process_workers(ctx);
		}

		if (!nx_ctx->had_error) {
			// Process any Promises that need to be fulfilled
			nx_process_pending_jobs(ctx, nx_ctx, rt);
		}

		// Update controller pad states
		padUpdate(&nx_ctx->pads[0]);
		padUpdate(&nx_ctx->pads[1]);
		padUpdate(&nx_ctx->pads[2]);
		padUpdate(&nx_ctx->pads[3]);
		padUpdate(&nx_ctx->pads[4]);
		padUpdate(&nx_ctx->pads[5]);
		padUpdate(&nx_ctx->pads[6]);
		padUpdate(&nx_ctx->pads[7]);

		u64 kDown = padGetButtonsDown(&nx_ctx->pads[0]);
		bool plusDown = kDown & HidNpadButton_Plus;

		if (nx_ctx->had_error) {
			if (plusDown) {
				// When an initialization or unhandled error occurs,
				// wait until the user presses "+" to fully exit so
				// the user has a chance to read the error message.
				break;
			}
		} else {
			// Call frame handler
			JSValueConst args[] = {JS_NewBool(ctx, plusDown)};
			JSValue ret_val =
				JS_Call(ctx, nx_ctx->frame_handler, JS_NULL, 1, args);

			if (JS_IsException(ret_val)) {
				nx_emit_error_event(ctx);
			}
			JS_FreeValue(ctx, ret_val);

			if (!is_running) {
				// `Switch.exit()` was called
				break;
			}
		}

		if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
			// Update the console, sending a new frame to the display
			consoleUpdate(print_console);
		} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
			// Copy the JS framebuffer to the current Switch buffer.
			// When a cursor overlay is registered, composite it through
			// `display_buffer` so the cursor visual never persists into
			// canvas->data.
			u32 stride;
			u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
			size_t total = (size_t)js_fb_width * (size_t)js_fb_height * 4;
			static int present_n = 0;
			++present_n;
			if (present_n == 1) NX_BOOT_LOG("CANVAS-mode present 1 (post splash)");
			bool composited = false;
			if (cursor_overlay_enabled && display_buffer &&
				display_buffer_size >= total) {
				memcpy(display_buffer, js_framebuffer, total);
				composite_cursor_overlay();
				memcpy(framebuf, display_buffer, total);
				composited = true;
			} else {
				memcpy(framebuf, js_framebuffer, total);
			}
			// Sample a pixel near the cursor so we can prove the composite
			// actually wrote something to the FB at the cursor location.
			if (present_n <= 6 || (present_n % 240) == 0) {
				int sx = cursor_overlay_x;
				int sy = cursor_overlay_y + 8;
				int sxc = sx + 5;
				if (sx >= 0 && sy >= 0 &&
				    sx < (int)js_fb_width && sy < (int)js_fb_height) {
					u8 *p = framebuf + (size_t)sy * (size_t)js_fb_width * 4 +
					        (size_t)sxc * 4;
					fprintf(stderr,
					        "[nxjs:cursor] present n=%d composited=%d enabled=%d "
					        "xy=(%d,%d) wh=%dx%d fb[%d,%d]=(%u,%u,%u,%u)\n",
					        present_n, (int)composited, (int)cursor_overlay_enabled,
					        cursor_overlay_x, cursor_overlay_y,
					        cursor_overlay_w, cursor_overlay_h,
					        sxc, sy, p[0], p[1], p[2], p[3]);
				}
			}
			framebufferEnd(framebuffer);
		}
	}

	thpool_destroy(nx_ctx->thpool);

	// Call exit handler
	JSValue ret_val = JS_Call(ctx, nx_ctx->exit_handler, JS_NULL, 0, NULL);
	JS_FreeValue(ctx, ret_val);

	if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
		nx_console_exit();
	} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
		nx_framebuffer_exit();
	}

	fclose(debug_fd);
	FILE *leaks_fd = freopen(LOG_FILENAME, "a", stdout);

	JS_FreeValue(ctx, global_obj);
	JS_FreeValue(ctx, nx_ctx->frame_handler);
	JS_FreeValue(ctx, nx_ctx->exit_handler);
	JS_FreeValue(ctx, nx_ctx->error_handler);
	JS_FreeValue(ctx, nx_ctx->unhandled_rejection_handler);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	if (nx_ctx->wasm_env) {
		m3_FreeEnvironment(nx_ctx->wasm_env);
	}
	if (nx_ctx->ft_library) {
		FT_Done_FreeType(nx_ctx->ft_library);
	}

	if (nx_ctx->mbedtls_initialized) {
		mbedtls_ctr_drbg_free(&nx_ctx->ctr_drbg);
		mbedtls_entropy_free(&nx_ctx->entropy);
	}
	if (nx_ctx->ca_certs_loaded) {
		mbedtls_x509_crt_free(&nx_ctx->ca_chain);
	}

	if (nx_ctx->spl_initialized) {
		splExit();
	}

	free(nx_ctx);

	plExit();
	romfsExit();
	// Match lazy init — only call socketExit when we actually initialized.
	if (s_socket_initialized) socketExit();

	fflush(leaks_fd);
	fclose(leaks_fd);

	/* If nothing was written to the debug log file, then delete it */
	delete_if_empty(LOG_FILENAME);

	return 0;
}
