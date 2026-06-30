// Engine-side cursor compositor — see cursor.h for the architecture rationale
// + the JS-side contract.
//
// Implementation notes:
//
// - Alpha handling. JS hands us NON-PREMULTIPLIED RGBA (per ImageData /
//   `canvas.getImageData` spec; the runtime cursor registry's decoded
//   `CursorFrame.rgba` matches). Skia's `drawImage` with the default
//   `kSrcOver` blend mode expects PRE-MULTIPLIED source pixels (Skia
//   stores all raster surfaces premult internally). We premultiply once
//   at `nx_cursor_set_static` / `nx_cursor_set_animated` upload time, store
//   the premult bytes, and build the SkImage with `kPremul_SkAlphaType`.
//   Mismatch here was the exact failure mode that broke the runtime
//   getImageData/putImageData save-restore approach — getting it right
//   here in one place + once-per-asset is the load-bearing correctness
//   property.
//
// - SkImage cache. Per-frame SkImage is lazy-built on first
//   `nx_cursor_composite` for that frame, then cached. `Set` /
//   `Set_animated` allocate the cache slot but don't build images —
//   defers the cost to the first present where the cursor actually
//   shows. A 16-frame APNG with 64×64 frames builds ~16 SkImages of
//   ~16 KB each = ~256 KB of GPU-uploaded raster — bounded.
//
// - Lifetime. SkSurface (`target` parameter) is owned by skia_gpu.cc;
//   `nx_cursor_composite` takes a raw `SkSurface*` and never stores it.
//   The cached SkImage is owned via `sk_sp<SkImage>` (Skia's intrusive
//   refcount) — released by `nx_cursor_exit` BEFORE the GrDirectContext
//   goes away (called from `nx_skia_gpu_screen_exit` for that reason).
//   No cross-thread concerns: JS isolate + present loop both run on
//   the main thread, serialized.
//
// - Animation frame pick. Mirrors the QuickJS-era logic at
//   nxjs-source/source/main.c:631-649 — modulo the total animation
//   duration, walk delays_ms[] to find the slot. O(frame_count) per
//   present — fine for the ≤16 typical frame counts (APNG wait/progress
//   spinners we ship). `armGetSystemTick()` is the load-bearing
//   primitive: keeps spinning while JS is fully blocked.

#include "cursor.h"

#include <stdlib.h>
#include <string.h>
#include <switch.h>  // armGetSystemTick, armTicksToNs

#include "include/core/SkAlphaType.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorType.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkSurface.h"

namespace {

struct CursorState {
	bool enabled = false;
	int32_t x = 0;
	int32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	// Frame storage. frame_count == 1 for static cursors; > 1 for
	// animated. Each entry in frame_rgba_premult is a malloc'd
	// (width * height * 4) byte buffer of PRE-MULTIPLIED RGBA. The
	// parallel frame_images array caches the SkImage built from each
	// frame on first composite (null until then).
	uint32_t frame_count = 0;
	uint8_t **frame_rgba_premult = nullptr;
	uint16_t *frame_delays_ms = nullptr;  // [frame_count]; null for static
	uint32_t total_anim_ms = 0;            // 0 for static
	uint64_t anim_start_tick = 0;          // armGetSystemTick at set_animated
	sk_sp<SkImage> *frame_images = nullptr;  // [frame_count]; null entries
	                                          // are lazy-built on first composite
};

CursorState s_state;

// Premultiply non-premult RGBA pixels into a freshly-allocated buffer.
// Returns NULL on alloc failure (caller treats as upload failure).
//
// Edge cases per spec:
//   a == 255 → identical (fast memcpy path).
//   a == 0   → all channels zero (transparent black, the only valid premult
//              representation of fully-transparent).
//   else     → R' = round(R * a / 255), etc. The +127 / 255 rounding matches
//              the convention Skia uses internally; off-by-one drift would
//              show up as faint cursor-edge banding against high-contrast
//              backgrounds.
uint8_t *premultiply_copy(const uint8_t *src, uint32_t pixel_count) {
	const size_t bytes = (size_t)pixel_count * 4;
	uint8_t *dst = (uint8_t *)malloc(bytes);
	if (!dst) return nullptr;
	for (uint32_t i = 0; i < pixel_count; i++) {
		const uint32_t a = src[i * 4 + 3];
		if (a == 255) {
			dst[i * 4 + 0] = src[i * 4 + 0];
			dst[i * 4 + 1] = src[i * 4 + 1];
			dst[i * 4 + 2] = src[i * 4 + 2];
			dst[i * 4 + 3] = 255;
		} else if (a == 0) {
			dst[i * 4 + 0] = 0;
			dst[i * 4 + 1] = 0;
			dst[i * 4 + 2] = 0;
			dst[i * 4 + 3] = 0;
		} else {
			dst[i * 4 + 0] =
			    (uint8_t)(((uint32_t)src[i * 4 + 0] * a + 127) / 255);
			dst[i * 4 + 1] =
			    (uint8_t)(((uint32_t)src[i * 4 + 1] * a + 127) / 255);
			dst[i * 4 + 2] =
			    (uint8_t)(((uint32_t)src[i * 4 + 2] * a + 127) / 255);
			dst[i * 4 + 3] = (uint8_t)a;
		}
	}
	return dst;
}

void free_frame_storage(void) {
	if (s_state.frame_rgba_premult) {
		for (uint32_t i = 0; i < s_state.frame_count; i++) {
			free(s_state.frame_rgba_premult[i]);
		}
		free(s_state.frame_rgba_premult);
		s_state.frame_rgba_premult = nullptr;
	}
	if (s_state.frame_delays_ms) {
		free(s_state.frame_delays_ms);
		s_state.frame_delays_ms = nullptr;
	}
	if (s_state.frame_images) {
		// sk_sp destructors release the GPU-uploaded raster handles.
		delete[] s_state.frame_images;
		s_state.frame_images = nullptr;
	}
}

// Pick the current frame index from armGetSystemTick. Mirrors QuickJS-era
// nxjs-source/source/main.c::composite_cursor_overlay frame-pick block.
// Static cursors fast-path to 0.
uint32_t pick_current_frame(void) {
	if (s_state.frame_count <= 1 || !s_state.frame_delays_ms ||
	    s_state.total_anim_ms == 0) {
		return 0;
	}
	const uint64_t elapsed_ns =
	    armTicksToNs(armGetSystemTick() - s_state.anim_start_tick);
	const uint32_t elapsed_ms = (uint32_t)(elapsed_ns / 1000000ull);
	const uint32_t t = elapsed_ms % s_state.total_anim_ms;
	uint32_t acc = 0;
	for (uint32_t i = 0; i < s_state.frame_count; i++) {
		acc += s_state.frame_delays_ms[i];
		if (t < acc) return i;
	}
	return s_state.frame_count - 1;
}

// Build and cache the SkImage for `frame_idx` on demand. Returns null
// on out-of-bounds or allocation failure (composite no-ops).
sk_sp<SkImage> get_or_build_frame_image(uint32_t frame_idx) {
	if (!s_state.frame_images || !s_state.frame_rgba_premult ||
	    frame_idx >= s_state.frame_count) {
		return nullptr;
	}
	if (s_state.frame_images[frame_idx]) {
		return s_state.frame_images[frame_idx];
	}
	if (!s_state.frame_rgba_premult[frame_idx]) {
		return nullptr;
	}
	SkImageInfo info = SkImageInfo::Make(
	    (int)s_state.width, (int)s_state.height,
	    kRGBA_8888_SkColorType, kPremul_SkAlphaType);
	SkPixmap pm(info, s_state.frame_rgba_premult[frame_idx],
	            (size_t)s_state.width * 4);
	// RasterFromPixmapCopy makes Skia own its own copy of the bytes so
	// the SkImage's lifetime is independent of our `frame_rgba_premult`
	// buffer. Avoids a use-after-free if we ever rebuild the state mid-
	// composite (we don't today, but the copy is a one-time cost and
	// keeps the lifetime story simple).
	s_state.frame_images[frame_idx] = SkImages::RasterFromPixmapCopy(pm);
	return s_state.frame_images[frame_idx];
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API — see cursor.h for the contract.
// ---------------------------------------------------------------------------

void nx_cursor_set_static(int32_t x, int32_t y,
                          const uint8_t *rgba,
                          uint32_t width, uint32_t height) {
	if (!rgba || width == 0 || height == 0) {
		nx_cursor_clear();
		return;
	}
	free_frame_storage();
	s_state.x = x;
	s_state.y = y;
	s_state.width = width;
	s_state.height = height;
	s_state.frame_count = 1;
	s_state.total_anim_ms = 0;
	s_state.anim_start_tick = 0;
	s_state.frame_rgba_premult = (uint8_t **)malloc(sizeof(uint8_t *));
	if (!s_state.frame_rgba_premult) {
		s_state.enabled = false;
		s_state.frame_count = 0;
		return;
	}
	s_state.frame_rgba_premult[0] = premultiply_copy(rgba, width * height);
	s_state.frame_images = new sk_sp<SkImage>[1];
	s_state.enabled = (s_state.frame_rgba_premult[0] != nullptr);
}

void nx_cursor_set_animated(int32_t x, int32_t y,
                            const uint8_t *packed_rgba,
                            uint32_t width, uint32_t height,
                            uint32_t frame_count,
                            const uint16_t *delays_ms) {
	if (!packed_rgba || !delays_ms || width == 0 || height == 0 ||
	    frame_count == 0 || frame_count > 64) {
		// frame_count > 64 is a sanity bound — the cursor registry ships
		// APNGs in the ~16-frame range; >64 is almost certainly bogus
		// input. Reject by clearing so a misbehaving JS caller doesn't
		// allocate hundreds of MB of frame storage.
		nx_cursor_clear();
		return;
	}
	free_frame_storage();
	s_state.x = x;
	s_state.y = y;
	s_state.width = width;
	s_state.height = height;
	s_state.frame_count = frame_count;
	s_state.frame_rgba_premult =
	    (uint8_t **)malloc(sizeof(uint8_t *) * frame_count);
	s_state.frame_delays_ms =
	    (uint16_t *)malloc(sizeof(uint16_t) * frame_count);
	if (!s_state.frame_rgba_premult || !s_state.frame_delays_ms) {
		free_frame_storage();
		s_state.enabled = false;
		s_state.frame_count = 0;
		return;
	}
	memcpy(s_state.frame_delays_ms, delays_ms,
	       sizeof(uint16_t) * frame_count);
	s_state.total_anim_ms = 0;
	for (uint32_t i = 0; i < frame_count; i++) {
		s_state.total_anim_ms += delays_ms[i];
	}
	if (s_state.total_anim_ms == 0) {
		// Defensive: a delays array of all zeros would loop the modulo at
		// composite time. Bump to 1 ms so we deterministically pick
		// frame 0 every present (effectively static).
		s_state.total_anim_ms = 1;
	}
	const size_t frame_bytes = (size_t)width * (size_t)height * 4;
	// Zero-init the parallel SkImage cache.
	s_state.frame_images = new sk_sp<SkImage>[frame_count];
	// Build the premult buffers up-front (SkImage stays lazy).
	for (uint32_t i = 0; i < frame_count; i++) {
		s_state.frame_rgba_premult[i] =
		    premultiply_copy(packed_rgba + i * frame_bytes, width * height);
	}
	s_state.anim_start_tick = armGetSystemTick();
	// Enabled iff frame 0 uploaded successfully (the minimum to render
	// anything). Per-frame premult failures show as nulls in
	// frame_rgba_premult — composite handles by returning null SkImage
	// and skipping the draw for that frame.
	s_state.enabled = (s_state.frame_rgba_premult[0] != nullptr);
}

void nx_cursor_set_position(int32_t x, int32_t y) {
	s_state.x = x;
	s_state.y = y;
}

void nx_cursor_clear(void) {
	free_frame_storage();
	s_state.enabled = false;
	s_state.frame_count = 0;
	s_state.width = 0;
	s_state.height = 0;
	s_state.total_anim_ms = 0;
	s_state.anim_start_tick = 0;
}

void nx_cursor_composite(SkSurface *target) {
	if (!s_state.enabled || !target) return;
	if (s_state.width == 0 || s_state.height == 0) return;
	const uint32_t frame_idx = pick_current_frame();
	sk_sp<SkImage> img = get_or_build_frame_image(frame_idx);
	if (!img) return;
	SkCanvas *c = target->getCanvas();
	if (!c) return;
	SkPaint paint;
	paint.setBlendMode(SkBlendMode::kSrcOver);
	// drawImage clips automatically against the target surface bounds,
	// so a cursor whose top-left x/y puts part of the bitmap off-screen
	// is handled correctly with no manual clamping.
	c->drawImage(img, (float)s_state.x, (float)s_state.y,
	             SkSamplingOptions(), &paint);
}

void nx_cursor_exit(void) {
	nx_cursor_clear();
}
