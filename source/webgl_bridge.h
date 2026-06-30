#pragma once
//
// V8 migration Step 2 — WebGL ↔ Skia coexistence primitive.
//
// Phase 2.B realizes the Phase 0 fbo-spike recipe as proper engine code on top
// of Phase 2.A's shared ES3 context (Skia owns EGL; bridge attaches via
// nx_skia_gpu_egl_* accessors). The bridge does NOT own EGL and does NOT
// create JS-visible WebGL classes — that's Phase 2.C+. What it owns:
//
//   1. A tenant offscreen FBO (color texture + depth/stencil renderbuffer)
//      created on Skia's shared ES3 context.
//   2. The load-bearing per-frame BOUNDARY discipline that any future WebGL
//      pass MUST wrap itself in to coexist with Skia:
//
//         nx_gl_state_save(&snap);
//         /* ...raw GL into the tenant FBO... */
//         nx_gl_state_restore(&snap);
//         nx_skia_gpu_gr_context()->resetContext();
//
//      Skipping the resetContext() leaves Ganesh believing its cached
//      program/VAO/etc. are still current — next Skia draw renders garbage.
//      Skipping any element of the save/restore set leaves Skia operating on
//      mis-bound state — symptom is intermittent / shape-dependent visual
//      corruption, not a clean crash. The state list mirrors the spike's
//      contract; see [[v8-migration-phase0-go]] for provenance.
//   3. A 2.B-only test compose path that drives the FBO with hand-written GL
//      and SkImages::BorrowTextureFrom-composites the FBO color texture +
//      a 2D overlay into the passed Skia surface. Gated by the
//      `[webgl] test_fbo = true` config flag so the shell renders normally
//      when off. This is the smoke + hardware checkpoint for Phase 2.B,
//      NOT the production path; Phase 2.C+ replaces it with the WebGL
//      bridge proper.
//
// Lifetime: nx_webgl_bridge_init() must be called AFTER nx_skia_gpu_screen_init
// (the bridge depends on the shared GL context + GrDirectContext being live).
// nx_webgl_bridge_exit() must be called BEFORE nx_skia_gpu_screen_exit().
//

#include "skia_gpu.h"

#include <GLES3/gl3.h>

#include "include/core/SkSurface.h"

// State snapshot captured at the WebGL→Skia handoff. The set was empirically
// validated in the fbo-spike (4,013 frames on Citron). Each field is required
// for a specific reason:
//
//   fbo            — Skia binds different FBOs all the time; restoring the
//                    DRAW_FRAMEBUFFER_BINDING is essential or Skia next draws
//                    into our tenant FBO instead of its own surface target.
//   viewport       — same: Skia sets per-surface, tenant sets per-FBO.
//   program        — Ganesh's program cache + state cache assume the bound
//                    program is the one it set; leaving ours bound silently
//                    breaks the next Ganesh draw.
//   vao            — likewise; Ganesh tracks bound VAO for vertex-attrib state.
//   array_buffer   — for the GL-pre-VAO-era path Ganesh sometimes hits.
//   active_tex     — bridges that change it (texture-unit selector) WILL
//                    corrupt Ganesh's per-unit binding state otherwise.
//   tex2d_binding  — texture-unit 0 binding, since Ganesh assumes it.
//   blend / depth_test / cull / scissor / stencil_test — Ganesh cares about
//                    every one of these; an enable left "wrong" silently
//                    drops fragments.
//   color_mask     — leaving a channel masked silently produces black/no-
//                    alpha output in subsequent Skia draws.
//   clear_color    — Ganesh sets per-clear; if we leave ours and Ganesh's
//                    next code path is "assume already correct" we'll
//                    glClear to the wrong color.
//   blend_src/dst  — full glBlendFuncSeparate state.
//
// NOT in the set (deliberately):
//   - DEPTH_FUNC / DEPTH_WRITEMASK — Ganesh's per-draw setup resets these.
//   - PIXEL_PACK/UNPACK alignment — irrelevant for the compose path.
//   - STENCIL ops/mask — Ganesh resets these per-draw too, and the spike
//     proved STENCIL_TEST enable + glDisable on restore is sufficient.
//   - polygon_offset / line_width / point_size — Ganesh sets these per-draw.
//   - active SAMPLER bindings — none used yet.
//
// If 2.C/2.D surfaces "Skia renders garbage after WebGL draws" symptoms on
// hardware, the FIRST place to look is this list — a missing entry under a
// new code path is the most likely cause.
struct nx_gl_state_snap_t {
	GLint fbo;
	GLint viewport[4];
	GLint program;
	GLint vao;
	GLint array_buffer;
	GLint active_tex;
	GLint tex2d_binding;
	GLboolean blend;
	GLboolean depth_test;
	GLboolean cull;
	GLboolean scissor;
	GLboolean stencil_test;
	GLboolean color_mask[4];
	GLfloat clear_color[4];
	GLint blend_src_rgb;
	GLint blend_dst_rgb;
	GLint blend_src_a;
	GLint blend_dst_a;
};

// Capture / restore the GL state contract. The caller owns the snap struct.
// gr->resetContext() is NOT called here — the caller does it after restore
// so the choice (per-section vs batched) is explicit at the call site.
void nx_gl_state_save(nx_gl_state_snap_t *s);
void nx_gl_state_restore(const nx_gl_state_snap_t *s);

// Initialize the tenant FBO + (optionally) the 2.B test program / VAO on
// Skia's shared ES3 context. Must be called AFTER nx_skia_gpu_screen_init.
// Returns false if FBO/program creation failed (caller treats the bridge as
// unavailable and renders normally). `fbo_w`/`fbo_h` is the offscreen size —
// 640×360 for the 2.B smoke probe and the 2.C slice demo.
bool nx_webgl_bridge_init(int fbo_w, int fbo_h);

// Tear down the FBO + program. Idempotent. Must be called BEFORE
// nx_skia_gpu_screen_exit so the GL handles are still valid when freed.
void nx_webgl_bridge_exit(void);

// True between init success and exit.
bool nx_webgl_bridge_is_initialized(void);

// ---- Phase 2.C accessors (used by webgl.cc to render INTO the tenant FBO)
// The WebGL context binds this FBO as its "default framebuffer" — every WebGL
// draw lands in this offscreen target, then the engine present hook calls
// nx_webgl_bridge_compose() to blit it onto Skia's canvas surface. Returns 0
// when bridge isn't initialized.
GLuint nx_webgl_bridge_fbo_id(void);

// FBO dimensions (the WebGL "drawing buffer" w/h). Returns 0,0 when not init.
void nx_webgl_bridge_fbo_size(int *out_w, int *out_h);

// Mark the FBO as touched this frame. Called by webgl.cc whenever a draw /
// clear lands in the tenant FBO. The compose path checks this flag and skips
// the SkImage draw on frames the bridge wasn't touched (cheap no-op when JS
// didn't call into WebGL).
void nx_webgl_bridge_mark_fbo_dirty(void);

// Compose the tenant FBO color texture as an SkImage into `target` (Skia's
// persistent canvas surface). No-op when not initialized OR when the FBO
// hasn't been marked dirty since the last compose. Clears the dirty flag
// after composing. NO overlay drawn — this is the production WebGL→Skia
// compose path, not the 2.B test driver.
void nx_webgl_bridge_compose(SkSurface *target);

// Phase 2.B smoke driver — STILL AVAILABLE behind [webgl] test_fbo = true.
// Renders a hand-written animated triangle into the tenant FBO (state-save
// bracketed + resetContext), then composites the FBO color texture as an
// SkImage into `target` at the test rect plus a 2D overlay (banner + frame
// counter + boundary microseconds) on top. No-op when the bridge isn't
// initialized OR when a WebGL context is actively driving the FBO (the test
// driver yields to real WebGL traffic to avoid clobbering tenant content).
// Idempotent against being called once per frame.
void nx_webgl_bridge_compose_test(SkSurface *target);

// Signal that a WebGL context is now the owner of the tenant FBO — disables
// the 2.B test driver's GL render-into-FBO step (compose_test still composes
// what's there). Phase 2.C call site: nx_webgl_context_new. The flag is
// process-wide because there's one bridge + one tenant FBO.
void nx_webgl_bridge_set_webgl_owned(bool owned);
bool nx_webgl_bridge_is_webgl_owned(void);

// Phase 2.D: gate `nx_webgl_bridge_compose` so the runtime can suppress the
// engine's whole-FBO auto-stomp onto Skia's canvas surface. brewser-runtime's
// canvas-runner.ts drives its own per-canvas paint by calling the parameterized
// compose helper below at the canvas's CSS layout slot, and calls
// gl.setBridgeAutoFlush(false) at getSharedScreenGL time so the engine stops
// auto-stomping the whole FBO on top of those placements. Default is TRUE
// (preserves the 2.B test_fbo smoke path + any non-runtime caller that
// doesn't set it). The brewser runtime sets FALSE on context acquisition.
void nx_webgl_bridge_set_auto_flush(bool v);

// Phase 2.D: parameterized sub-rect blit of the tenant FBO onto `target` Skia
// surface. The engine-side half of `gl.copyBridgeToCanvas(srcX, srcY, srcW,
// srcH, dst_canvas, dstX, dstY)`. The runtime expresses src coords in
// canvas/top-down convention (matching the QuickJS-era contract); this helper
// translates to the kBottomLeft-origin SkImage's coords internally so a request
// for src (0, 0, w, h) reads where Three.js gl.viewport(0, 0, w, h) actually
// writes (GL bottom-left of the FBO). dst rect is at (dst_x, dst_y) with the
// same width/height as src. Caller (webgl.cc::w_copy_bridge_to_canvas) MUST
// exit any open per-frame WebGL bracket first so Skia draws against its own
// GL state, not the WebGL pass's. Returns false if the bridge isn't initialized
// or args are degenerate.
bool nx_webgl_bridge_compose_rect(SkSurface *target,
                                  int src_x, int src_y, int src_w, int src_h,
                                  int dst_x, int dst_y);
