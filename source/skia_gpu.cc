#include "skia_gpu.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <time.h>

#include "cursor.h"
#include "fps.h"

// Boot timing anchor defined in main.cc; populated at the very start of
// main() so the [skia] (+Nms) log below can report the pre-Skia black
// ceiling without needing an extra IPC/getter layer. Pure logging hook;
// no behavior impact when not read.
extern uint64_t g_boot_t0_ns;

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkSurface.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"

namespace {

EGLDisplay s_dpy = nullptr;
EGLSurface s_surf = nullptr;
EGLContext s_ctx = nullptr;

sk_sp<GrDirectContext> s_gr;
// The EGL window's FBO 0, double-buffered (the present target).
sk_sp<SkSurface> s_fbo;
// A persistent, single render-target surface the canvas draws into. Canvas 2D
// semantics require ONE persistent surface (apps draw incrementally), but the
// FBO is double-buffered, so we composite: draw into s_canvas (persistent),
// then blit it to s_fbo and swap each frame.
sk_sp<SkSurface> s_canvas;
u32 s_w = 0, s_h = 0;

bool init_egl(NWindow *win) {
	s_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!s_dpy)
		return false;
	eglInitialize(s_dpy, nullptr, nullptr);
	eglBindAPI(EGL_OPENGL_ES_API);
	EGLConfig cfg;
	EGLint num = 0;
	// V8 migration Phase 2.A: bump context to ES3 so the WebGL bridge (Phase
	// 2.B+) can attach to this single shared context. EGL_RENDERABLE_TYPE =
	// EGL_OPENGL_ES3_BIT (0x0040) is REQUIRED — without it eglChooseConfig may
	// hand back an ES2-only config and eglCreateContext(ES3) then fails. Magic
	// number matches the fbo-spike + upstream webgl.cc pattern so this is
	// robust to header version drift.
	const EGLint attrs[] = {EGL_RENDERABLE_TYPE, 0x0040, // EGL_OPENGL_ES3_BIT
	                        EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8,
	                        EGL_BLUE_SIZE,  8, EGL_ALPHA_SIZE,   8,
	                        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
	                        EGL_NONE};
	eglChooseConfig(s_dpy, attrs, &cfg, 1, &num);
	if (!num) {
		fprintf(stderr, "[skia] eglChooseConfig: no ES3 config\n");
		fflush(stderr);
		return false;
	}
	s_surf = eglCreateWindowSurface(s_dpy, cfg, win, nullptr);
	if (!s_surf)
		return false;
	const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	s_ctx = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, ca);
	if (!s_ctx) {
		fprintf(stderr, "[skia] eglCreateContext (ES3) failed: 0x%x\n",
		        eglGetError());
		fflush(stderr);
		return false;
	}
	if (eglMakeCurrent(s_dpy, s_surf, s_surf, s_ctx) != EGL_TRUE) {
		// Without a current context nothing later (swap interval, GL interface,
		// Ganesh) can work; fail init so the caller falls back to raster.
		fprintf(stderr, "[skia] eglMakeCurrent failed: 0x%x\n", eglGetError());
		fflush(stderr);
		return false;
	}
	// Lock presentation to one buffer-swap per vblank (whatever the display's
	// refresh rate is). Without an explicit interval the driver default is
	// undefined. A failure here is non-fatal — presentation still works, just
	// with an undefined cadence (the previous behavior) — so log and continue.
	if (eglSwapInterval(s_dpy, 1) != EGL_TRUE) {
		fprintf(stderr, "[skia] eglSwapInterval(1) failed: 0x%x\n",
		        eglGetError());
		fflush(stderr);
	}
	// NOTE: the EGL window surface is double-buffered. The canvas draws into a
	// separate persistent surface (s_canvas) and we composite it into the back
	// buffer every present (see nx_skia_gpu_present), so double buffering does
	// not cause stale-frame flicker even though the driver doesn't support
	// EGL_SWAP_BEHAVIOR_PRESERVED.
	return true;
}

} // namespace

sk_sp<SkSurface> nx_skia_gpu_screen_init(u32 width, u32 height, int samples,
                                         u32 gpu_cache_mib) {
	if (!init_egl(nwindowGetDefault())) {
		fprintf(stderr, "[skia] EGL init failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	// Smoke confirmation of the ES3 bringup. ES3 advertises GL_VERSION starting
	// with "OpenGL ES 3" (e.g. "OpenGL ES 3.2 Mesa ..."); if Mesa silently gave
	// us ES2 the second token reads "2." and the WebGL bridge in 2.B would
	// later fail to find ES3 entry points. Logged once for the auditor;
	// Ganesh-GL init below is the load-bearing check.
	const GLubyte *gl_version = glGetString(GL_VERSION);
	const GLubyte *gl_vendor = glGetString(GL_VENDOR);
	const GLubyte *gl_renderer = glGetString(GL_RENDERER);
	fprintf(stderr, "[skia] GL version=%s vendor=%s renderer=%s\n",
	        gl_version ? (const char *)gl_version : "(null)",
	        gl_vendor ? (const char *)gl_vendor : "(null)",
	        gl_renderer ? (const char *)gl_renderer : "(null)");
	fflush(stderr);

	SkGraphics::Init();
	auto iface = GrGLInterfaces::MakeEGL();
	s_gr = GrDirectContexts::MakeGL(iface);
	if (!s_gr) {
		fprintf(stderr, "[skia] GrDirectContext failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	// Ganesh's default GPU resource cache budget is only ~96 MiB. A
	// texture-heavy app whose per-frame working set exceeds that (e.g. a
	// full-screen 2D game drawing many large atlases + baked tilemap layers,
	// each up to a few MiB of GPU texture) thrashes the cache: textures still
	// needed next frame get evicted LRU and re-uploaded. When the caller
	// requests a larger budget (gpu_cache_mib > 0; see the regime-gated
	// resolution in main.cc + the [renderer] gpu_cache config), raise it so
	// the working set stays resident. 0 leaves Skia's default untouched
	// (correct for tight-RAM applet mode, where a big cache would starve Mesa).
	if (gpu_cache_mib > 0) {
		const size_t oldBytes = s_gr->getResourceCacheLimit();
		const size_t newBytes = (size_t)gpu_cache_mib * 1024u * 1024u;
		s_gr->setResourceCacheLimit(newBytes);
		fprintf(stderr, "[skia] GPU resource cache limit %zu MiB -> %zu MiB\n",
		        oldBytes / (1024 * 1024), newBytes / (1024 * 1024));
		fflush(stderr);
	}
	GrGLFramebufferInfo fbi;
	fbi.fFBOID = 0;
	fbi.fFormat = 0x8058; // GL_RGBA8
	auto rt = GrBackendRenderTargets::MakeGL((int)width, (int)height, 0, 8, fbi);
	s_fbo = SkSurfaces::WrapBackendRenderTarget(
	    s_gr.get(), rt, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
	    nullptr, nullptr);
	if (!s_fbo) {
		fprintf(stderr, "[skia] WrapBackendRenderTarget failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	// Persistent canvas surface (BGRA to match the raster/canvas pixel model
	// and getImageData readback path). Prefer MSAA: Ganesh's coverage-based
	// antialiased path renderer silently drops some large/complex concave fills
	// (e.g. the Ghostscript-tiger mouth) on a non-multisampled target. MSAA
	// rasterizes fills without that renderer and resolves smooth edges, so
	// complex antialiased fills render correctly. MSAA costs samples*WxH*4
	// bytes, which may not fit in applet mode (~137 MiB) -> retry with fewer
	// samples, finally non-MSAA, rather than failing (a failed init would tear
	// down EGL and fall back to the raster path).
	SkImageInfo info = SkImageInfo::Make((int)width, (int)height,
	                                     kBGRA_8888_SkColorType,
	                                     kPremul_SkAlphaType);
	int try_samples[] = {samples, 2, 0};
	for (int si = 0; si < 3 && !s_canvas; si++) {
		int s = try_samples[si];
		if (si > 0 && s >= samples)
			continue;  // don't retry an equal/higher count
		s_canvas = SkSurfaces::RenderTarget(s_gr.get(), skgpu::Budgeted::kNo,
		                                    info, s, kBottomLeft_GrSurfaceOrigin,
		                                    nullptr);
		if (s_canvas) {
			fprintf(stderr, "[skia] canvas surface MSAA=%dx ready\n", s);
			fflush(stderr);
		}
	}
	if (!s_canvas) {
		fprintf(stderr, "[skia] canvas RenderTarget failed (all sample counts)\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	s_w = width;
	s_h = height;
	{
		// Boot-splash residual diagnostic: report ms since main()'s t0.
		// This delta is T_Skia — the pre-Skia black ceiling that no JS-
		// side splash can cover (no GPU surface exists before this
		// line). The splash hoist closes the post-Skia gap; T_Skia is
		// what's left and the number the user uses to decide whether
		// to design a C-side framebuffer-splash-before-Skia patch.
		uint64_t _ms = 0;
		if (g_boot_t0_ns != 0) {
			struct timespec _ts;
			clock_gettime(CLOCK_MONOTONIC, &_ts);
			uint64_t _now = (uint64_t)_ts.tv_sec * 1000000000ull +
			                (uint64_t)_ts.tv_nsec;
			_ms = (_now - g_boot_t0_ns) / 1000000ull;
		}
		fprintf(stderr,
		        "[skia] GPU screen surface %ux%u ready (+%llums since t0)\n",
		        width, height, (unsigned long long)_ms);
		fflush(stderr);
	}
	return s_canvas;
}

void nx_skia_gpu_present(void) {
	if (!s_canvas || !s_fbo || !s_gr)
		return;
	// Composite: snapshot the persistent canvas surface and blit it into the
	// EGL back buffer (FBO 0), then swap. The persistent surface always holds
	// the full current canvas content, so every presented buffer is correct
	// regardless of double buffering or incremental drawing.
	sk_sp<SkImage> img = s_canvas->makeImageSnapshot();
	if (img) {
		SkCanvas *c = s_fbo->getCanvas();
		// Use kSrc (source-replace), NOT the default kSrcOver: the EGL back
		// buffer is double-buffered and retains stale content (the previous
		// frame, or uninitialized garbage). With kSrcOver, any transparent /
		// partially-transparent pixel in the canvas surface would blend the
		// stale destination through, making output look additive across
		// frames. kSrc copies the surface verbatim — including alpha — so each
		// present fully replaces the back buffer, matching the CPU raster path
		// (which memcpy()s the pixels straight into the framebuffer).
		SkPaint paint;
		paint.setBlendMode(SkBlendMode::kSrc);
		c->drawImage(img, 0, 0, SkSamplingOptions(), &paint);
	}
	// Cursor compositor (re-port of QuickJS-era composite_cursor_overlay,
	// NXJS_PATCHES_NEEDED.md #4). Blends the current cursor SkImage onto the
	// EGL back-buffer ONLY — s_canvas is never touched, so the next frame's
	// canvas blit above gives a clean surface and the cursor doesn't trail.
	// Animated cursors advance via armGetSystemTick() inside the call, so
	// the wait/progress spinner stays smooth even when JS is fully blocked
	// on a synchronous chunk (e.g. navigateTo's grid build). No-op when no
	// JS code has pushed an overlay via screen.setCursorOverlay et al.
	nx_cursor_composite(s_fbo.get());
	// FPS overlay (fps.cc). Drawn AFTER the cursor so the readout sits on top
	// of everything, and — like the cursor — only onto the EGL back-buffer, so
	// the persistent canvas surface stays clean (no trail). Measures the
	// present rate every call; no-op draw unless screen.setFpsOverlayEnabled(true).
	nx_fps_composite(s_fbo.get());
	s_gr->flush(s_fbo.get());
	s_gr->submit();
	eglSwapBuffers(s_dpy, s_surf);
}

void nx_skia_gpu_set_swap_interval(int interval) {
	// Frame-pacing control (2026-09-06): eglSwapInterval(2) makes each
	// eglSwapBuffers wait 2 vsync periods (→ 30 Hz on a 60 Hz panel), which
	// gives 30 fps video a clean 1:1 cadence (each frame shown once, evenly)
	// instead of the juddery ~40 fps the free-running loop produces when the
	// per-frame cost straddles the 16.7 ms vsync boundary. The shell toggles
	// this to 2 while a fullscreen video is the sole content on screen and
	// back to 1 (60 Hz) otherwise. No-op on the raster fallback (no EGL).
	if (s_dpy)
		eglSwapInterval(s_dpy, interval);
}

void nx_skia_gpu_screen_exit(void) {
	// Release any cached cursor SkImages BEFORE the GrDirectContext goes
	// away — the SkImage handles reference GPU-uploaded raster textures
	// that the context owns. Free order matters: cursor → canvas → fbo →
	// gr_context. Idempotent (safe to call again at teardown).
	nx_cursor_exit();
	s_canvas.reset();
	s_fbo.reset();
	if (s_gr) {
		SkGraphics::PurgeAllCaches();
		s_gr.reset();
	}
	// Idempotent: also serves as the cleanup path for a failed init, and main()
	// calls it again at teardown. Null each handle after destroying it.
	if (s_dpy) {
		eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (s_ctx) {
			eglDestroyContext(s_dpy, s_ctx);
			s_ctx = nullptr;
		}
		if (s_surf) {
			eglDestroySurface(s_dpy, s_surf);
			s_surf = nullptr;
		}
		eglTerminate(s_dpy);
		s_dpy = nullptr;
	}
}

// -----------------------------------------------------------------------------
// Shared-context accessors (V8 migration Phase 2.A).
//
// Phase 2.A only EXPOSES these — the WebGL stub does not yet call them. Phase
// 2.B grows the bridge into the stub and attaches via these handles per the
// fbo-spike recipe (gl_state_save → webgl → restore → grCtx->resetContext).
//
// All four return null before init or after exit; treat that as "GPU path not
// available" the same way the rest of the engine treats nx_skia_gpu_screen_init
// returning nullptr (caller falls back to raster).

EGLDisplay nx_skia_gpu_egl_display(void) { return s_dpy; }
EGLSurface nx_skia_gpu_egl_surface(void) { return s_surf; }
EGLContext nx_skia_gpu_egl_context(void) { return s_ctx; }
GrDirectContext *nx_skia_gpu_gr_context(void) { return s_gr.get(); }
SkSurface *nx_skia_gpu_canvas_surface(void) { return s_canvas.get(); }

void nx_skia_gpu_free_gpu_resources(void) {
	if (s_gr) s_gr->freeGpuResources();
}
