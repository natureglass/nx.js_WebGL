#pragma once
#include <switch.h>

#include <EGL/egl.h>

#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"

class GrDirectContext;

// Phase 2.2 GPU backend: EGL + Skia Ganesh GL over the default NWindow. The
// screen canvas draws into a PERSISTENT offscreen GPU SkSurface (`s_canvas`);
// each present snapshots it and blits into the EGL window's double-buffered
// FBO 0, then eglSwapBuffers (see nx_skia_gpu_present). Offscreen canvases
// remain raster. If GPU bringup fails (e.g. Mesa starved for memory in applet
// mode), the caller falls back to the raster + libnx-framebuffer present path.
//
// V8 migration Phase 2.A: the EGL context is created at ES3 (was ES2) so the
// WebGL bridge phases (2.B+) can attach to this same context — single shared
// ES3 context per the Phase 0 spike (D:/Workspace/nxjs-upstream/fbo-spike).
// Ganesh-GL is happy on ES3 (a strict superset of ES2); the WebGL bridge gets
// the handles via the accessors below + a gl_state_save/restore +
// GrDirectContext::resetContext() protocol around each WebGL section.
//
// All functions must be called on the render (main) thread.

// Initialize EGL + a shared GrDirectContext + the screen draw surface at
// width x height. `samples` is the desired MSAA sample count (e.g. 4 in
// application mode, 2 in applet mode); if the multisampled surface can't be
// allocated it automatically retries with no MSAA.
//
// The screen canvas draws into a PERSISTENT offscreen surface;
// `nx_skia_gpu_present` snapshots+blits it into the double-buffered swapchain
// each frame, preserving prior-frame content (standard incremental-canvas
// semantics). The per-frame composite is cheap (measured ~6-8ms at 720p on
// Switch hardware) and is not a meaningful cost.
//
// `gpu_cache_mib` sets the Ganesh GPU resource-cache budget in MiB; 0 leaves
// Skia's own default (~96 MiB). Raise it for a texture-heavy app whose
// per-frame working set would otherwise thrash the cache.
//
// Returns the surface the screen canvas should draw into, or nullptr on
// failure (all partial EGL/Mesa state torn down). The surface is owned by
// this module; do not outlive nx_skia_gpu_screen_exit().
sk_sp<SkSurface> nx_skia_gpu_screen_init(u32 width, u32 height, int samples,
                                         u32 gpu_cache_mib);

// Flush + submit the GPU surface and eglSwapBuffers (present one frame).
void nx_skia_gpu_present(void);

// Tear down the GPU surface, GrDirectContext, and EGL. Idempotent.
void nx_skia_gpu_screen_exit(void);

// -----------------------------------------------------------------------------
// Shared-context accessors (V8 migration Phase 2.A)
//
// The Phase 0 spike (D:/Workspace/nxjs-upstream/fbo-spike) proved that Skia
// (Ganesh GL) and a WebGL-like raw GLES3 pass coexist on ONE EGL context when:
//   1. The context is created at ES3 (handled here, on this side of the seam).
//   2. The WebGL pass save/restores the GL state it touches and calls
//      GrDirectContext::resetContext() at the boundary so Ganesh rebinds its
//      cached program/VAO/state.
//   3. The WebGL pass renders to an offscreen FBO that Skia composites via
//      SkImages::BorrowTextureFrom (Phase 2.B work — NOT done here).
//
// These accessors give the WebGL bridge the handles it needs in 2.B onward.
// All return null/zero before nx_skia_gpu_screen_init() succeeds and after
// nx_skia_gpu_screen_exit().
//
// The bridge MUST NOT call eglDestroyContext / eglDestroySurface / eglTerminate
// on the returned handles — Skia owns lifetime. The bridge may call
// eglMakeCurrent(display, surface, surface, context) if it ever swaps the
// current context (the default is "always current," set once at init), and
// MUST NOT leave a different context current when handing back to Skia.

// EGL display the engine bound at init (EGL_DEFAULT_DISPLAY). Null before init
// / after exit.
EGLDisplay nx_skia_gpu_egl_display(void);

// EGL window surface on nwindowGetDefault(). The same surface Skia presents to
// via nx_skia_gpu_present().
EGLSurface nx_skia_gpu_egl_surface(void);

// The single shared ES3 EGL context. Already current on the render thread by
// the time the bridge sees it (we eglMakeCurrent once at init).
EGLContext nx_skia_gpu_egl_context(void);

// Skia's GrDirectContext. The bridge calls grCtx->resetContext() AFTER each
// WebGL section so Ganesh's GL-state cache invalidates and Skia re-binds its
// program/VAO/state on its next draw. Lifetime owned here; do not addref.
GrDirectContext *nx_skia_gpu_gr_context(void);

// The persistent canvas SkSurface — what the engine's render pipeline draws
// INTO each frame and what nx_skia_gpu_present() blits to the back buffer.
// Phase 2.B's WebGL↔Skia coexistence bridge composes its tenant-FBO color
// texture into THIS surface (between the JS render layer's draws and the
// present blit) so the FBO content appears on screen alongside the shell.
// Returns null before init / after exit.
SkSurface *nx_skia_gpu_canvas_surface(void);

// Release all cached GPU resources held by Skia's GrDirectContext (Ganesh
// atlas + glyph cache + unlocked textures + shader cache). Used by the WebGL
// bridge's resetSharedContext() to relieve cumulative Skia heap pressure that
// otherwise OOMs long-running test loops (the WebGL 1 conformance runner
// hitting a 217KB paint allocation failure around test #325). Skia lazily
// re-caches on next paint; no lifecycle side-effects. Idempotent; no-op if
// the GrContext is null.
void nx_skia_gpu_free_gpu_resources(void);
