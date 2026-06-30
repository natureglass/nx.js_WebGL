#include "webgl_bridge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkFont.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSamplingOptions.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

namespace {

bool s_initialized = false;
int s_fbo_w = 0;
int s_fbo_h = 0;

// Tenant GL handles. Owned by webgl_bridge; freed in nx_webgl_bridge_exit().
GLuint s_fbo = 0;
GLuint s_color_tex = 0;
GLuint s_depth_rb = 0;

// Phase 2.C ownership flag. When true (WebGL context exists), the 2.B test
// driver yields its render step; compose path is shared.
bool s_webgl_owned = false;

// Per-frame dirty flag: set when the tenant FBO has been written to since the
// last compose, cleared by nx_webgl_bridge_compose. Drops the SkImage draw on
// idle frames (no WebGL traffic + no test_fbo).
bool s_fbo_dirty = false;

// Auto-flush gate: when true (default), nx_webgl_bridge_compose stomps the
// dirty FBO onto Skia's canvas surface every frame. When false, the runtime
// is opting to drive its own per-canvas readback (canvas-runner.ts reads
// gl.readPixels into each <canvas>'s OffscreenCanvas, which Skia paints at
// the canvas's CSS layout slot) and the engine compose MUST stay out of the
// way or it overwrites the correctly-placed DOM. The 2.B test-FBO smoke path
// never calls setBridgeAutoFlush so its default-true behavior is preserved.
// Toggled by gl.setBridgeAutoFlush(bool) via webgl.cc::w_set_bridge_auto_flush.
bool s_auto_flush = true;

// 2.B test program. Compiled once at init; not WebGL — just enough hand-
// written GL to land a visible draw in the FBO that 2.D can read back.
GLuint s_test_prog = 0;
GLuint s_test_vbo = 0;
GLuint s_test_vao = 0;
GLint s_test_u_color = -1;

// Compose path: SkImage wrapping the tenant FBO color texture, allocated
// once and reused every frame. Re-borrowed if the FBO changes.
sk_sp<SkImage> s_compose_image;

// Animation state for the 2.B test draw + perf metering.
uint64_t s_t_start_ns = 0;
uint64_t s_frame_count = 0;
double s_boundary_us_accum = 0.0;

uint64_t ts_ns() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

GLuint compile_shader(GLenum type, const char *src) {
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, &src, nullptr);
	glCompileShader(sh);
	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		GLsizei n = 0;
		glGetShaderInfoLog(sh, sizeof(log), &n, log);
		fprintf(stderr, "[webgl-bridge] shader compile fail: %.*s\n", (int)n,
		        log);
		fflush(stderr);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

bool create_fbo(int w, int h) {
	glGenTextures(1, &s_color_tex);
	glBindTexture(GL_TEXTURE_2D, s_color_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenRenderbuffers(1, &s_depth_rb);
	glBindRenderbuffer(GL_RENDERBUFFER, s_depth_rb);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

	glGenFramebuffers(1, &s_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       s_color_tex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                          GL_RENDERBUFFER, s_depth_rb);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	// Unbind BEFORE checking status so the failure path leaves Skia's FBO 0
	// current rather than our incomplete tenant.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[webgl-bridge] FBO incomplete: 0x%x\n", status);
		fflush(stderr);
		return false;
	}
	return true;
}

bool create_test_program(void) {
	// Solid-color triangle program. Uses ES 3 `#version 300 es` because the
	// shared context is ES3 (Phase 2.A) — that matches the spike. The 2D
	// vertex layout makes the triangle visible regardless of MVP setup.
	const char *vs = "#version 300 es\n"
	                 "layout(location=0) in vec2 a_pos;\n"
	                 "void main(){ gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
	const char *fs = "#version 300 es\n"
	                 "precision mediump float;\n"
	                 "uniform vec4 u_color;\n"
	                 "out vec4 outColor;\n"
	                 "void main(){ outColor = u_color; }\n";
	GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
	if (!v) return false;
	GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
	if (!f) { glDeleteShader(v); return false; }
	s_test_prog = glCreateProgram();
	glAttachShader(s_test_prog, v);
	glAttachShader(s_test_prog, f);
	glLinkProgram(s_test_prog);
	GLint ok = 0;
	glGetProgramiv(s_test_prog, GL_LINK_STATUS, &ok);
	glDeleteShader(v);
	glDeleteShader(f);
	if (!ok) {
		char log[1024];
		GLsizei n = 0;
		glGetProgramInfoLog(s_test_prog, sizeof(log), &n, log);
		fprintf(stderr, "[webgl-bridge] program link fail: %.*s\n", (int)n,
		        log);
		fflush(stderr);
		glDeleteProgram(s_test_prog);
		s_test_prog = 0;
		return false;
	}
	s_test_u_color = glGetUniformLocation(s_test_prog, "u_color");

	// Triangle in clip space. Big enough to mostly fill the FBO while
	// leaving a tinted border (the clear color) so we can confirm BOTH the
	// glClear AND the draw made it through compositing.
	const float verts[] = {-0.7f, -0.7f, 0.7f, -0.7f, 0.0f, 0.8f};
	glGenVertexArrays(1, &s_test_vao);
	glBindVertexArray(s_test_vao);
	glGenBuffers(1, &s_test_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, s_test_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return true;
}

// Render an animated clear-+-triangle into the tenant FBO. Caller has already
// captured GL state via nx_gl_state_save(); we leave state mutated but the
// caller's restore handles it.
void render_test_into_fbo(float t) {
	glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
	glViewport(0, 0, s_fbo_w, s_fbo_h);
	// Animate clear color so a stuck-frame is obvious (constant fill = stale
	// frame; cycling = present is actually advancing).
	float r = 0.05f + 0.05f * sinf(t * 0.7f);
	float g = 0.10f + 0.05f * cosf(t * 0.9f);
	float b = 0.25f + 0.05f * sinf(t * 1.1f);
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Disable Skia's typical enables so our minimal draw isn't accidentally
	// dependent on inherited state. The save snap captured what to restore.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glUseProgram(s_test_prog);
	// Triangle color cycles too — magenta-ish, contrasts with the dark clear.
	float tr = 0.7f + 0.3f * sinf(t * 2.0f);
	float tg = 0.2f + 0.2f * cosf(t * 1.4f);
	float tb = 0.8f;
	glUniform4f(s_test_u_color, tr, tg, tb, 1.0f);
	glBindVertexArray(s_test_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void wrap_compose_image(void) {
	// Borrow the tenant FBO color texture as an SkImage. Borrowing means
	// Skia does NOT own the GL handle — webgl_bridge keeps owning it,
	// must outlive any Skia draw that references the image, and frees it
	// in nx_webgl_bridge_exit BEFORE the GrDirectContext goes away.
	GrGLTextureInfo tinfo;
	tinfo.fTarget = GL_TEXTURE_2D;
	tinfo.fID = s_color_tex;
	tinfo.fFormat = 0x8058; // GL_RGBA8
	GrBackendTexture btex = GrBackendTextures::MakeGL(s_fbo_w, s_fbo_h,
	                                                  skgpu::Mipmapped::kNo,
	                                                  tinfo);
	GrDirectContext *gr = nx_skia_gpu_gr_context();
	if (!gr) {
		s_compose_image.reset();
		return;
	}
	s_compose_image = SkImages::BorrowTextureFrom(
	    gr, btex, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
	    kPremul_SkAlphaType, nullptr);
	if (!s_compose_image) {
		fprintf(stderr, "[webgl-bridge] SkImages::BorrowTextureFrom failed\n");
		fflush(stderr);
	}
}

} // namespace

// ---------------------------------------------------------------------------
// State save/restore primitive (public — re-used by 2.C+ WebGL bridge).
// ---------------------------------------------------------------------------

void nx_gl_state_save(nx_gl_state_snap_t *s) {
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s->fbo);
	glGetIntegerv(GL_VIEWPORT, s->viewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &s->vao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &s->active_tex);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->tex2d_binding);
	s->blend        = glIsEnabled(GL_BLEND);
	s->depth_test   = glIsEnabled(GL_DEPTH_TEST);
	s->cull         = glIsEnabled(GL_CULL_FACE);
	s->scissor      = glIsEnabled(GL_SCISSOR_TEST);
	s->stencil_test = glIsEnabled(GL_STENCIL_TEST);
	glGetBooleanv(GL_COLOR_WRITEMASK, s->color_mask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, s->clear_color);
	glGetIntegerv(GL_BLEND_SRC_RGB, &s->blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &s->blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &s->blend_src_a);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &s->blend_dst_a);
}

void nx_gl_state_restore(const nx_gl_state_snap_t *s) {
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)s->fbo);
	glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	glUseProgram((GLuint)s->program);
	glBindVertexArray((GLuint)s->vao);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)s->array_buffer);
	glActiveTexture((GLenum)s->active_tex);
	glBindTexture(GL_TEXTURE_2D, (GLuint)s->tex2d_binding);
	if (s->blend)        glEnable(GL_BLEND);        else glDisable(GL_BLEND);
	if (s->depth_test)   glEnable(GL_DEPTH_TEST);   else glDisable(GL_DEPTH_TEST);
	if (s->cull)         glEnable(GL_CULL_FACE);    else glDisable(GL_CULL_FACE);
	if (s->scissor)      glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	if (s->stencil_test) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
	glColorMask(s->color_mask[0], s->color_mask[1], s->color_mask[2],
	            s->color_mask[3]);
	glClearColor(s->clear_color[0], s->clear_color[1], s->clear_color[2],
	             s->clear_color[3]);
	glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_a,
	                    s->blend_dst_a);
}

// ---------------------------------------------------------------------------
// Lifetime + 2.B test compose path.
// ---------------------------------------------------------------------------

bool nx_webgl_bridge_init(int fbo_w, int fbo_h) {
	if (s_initialized) return true;
	if (!nx_skia_gpu_egl_context() || !nx_skia_gpu_gr_context()) {
		fprintf(stderr, "[webgl-bridge] init refused: skia_gpu not ready\n");
		fflush(stderr);
		return false;
	}
	if (fbo_w <= 0 || fbo_h <= 0) {
		fprintf(stderr, "[webgl-bridge] init refused: bad fbo size %dx%d\n",
		        fbo_w, fbo_h);
		fflush(stderr);
		return false;
	}
	s_fbo_w = fbo_w;
	s_fbo_h = fbo_h;

	// We're being called from the engine init path — the shared context is
	// already current per skia_gpu's eglMakeCurrent. Save Skia's pre-init GL
	// state so our FBO creation can't leave anything mis-bound when it
	// returns to the engine's render loop.
	nx_gl_state_snap_t snap;
	nx_gl_state_save(&snap);

	if (!create_fbo(fbo_w, fbo_h)) {
		nx_gl_state_restore(&snap);
		nx_skia_gpu_gr_context()->resetContext();
		return false;
	}
	if (!create_test_program()) {
		// FBO created but program failed — clean it up so we don't leak.
		if (s_fbo) { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
		if (s_color_tex) { glDeleteTextures(1, &s_color_tex); s_color_tex = 0; }
		if (s_depth_rb) { glDeleteRenderbuffers(1, &s_depth_rb); s_depth_rb = 0; }
		nx_gl_state_restore(&snap);
		nx_skia_gpu_gr_context()->resetContext();
		return false;
	}

	wrap_compose_image();
	if (!s_compose_image) {
		// Composite wrapper failed — still safe to render INTO the FBO, but
		// 2.B's smoke gate would have nothing to show. Treat as init failure.
		glDeleteFramebuffers(1, &s_fbo); s_fbo = 0;
		glDeleteTextures(1, &s_color_tex); s_color_tex = 0;
		glDeleteRenderbuffers(1, &s_depth_rb); s_depth_rb = 0;
		glDeleteProgram(s_test_prog); s_test_prog = 0;
		glDeleteBuffers(1, &s_test_vbo); s_test_vbo = 0;
		glDeleteVertexArrays(1, &s_test_vao); s_test_vao = 0;
		nx_gl_state_restore(&snap);
		nx_skia_gpu_gr_context()->resetContext();
		return false;
	}

	nx_gl_state_restore(&snap);
	nx_skia_gpu_gr_context()->resetContext();

	s_t_start_ns = ts_ns();
	s_frame_count = 0;
	s_boundary_us_accum = 0.0;
	s_initialized = true;

	fprintf(stderr,
	        "[webgl-bridge] init ok fbo=%dx%d color_tex=%u depth_rb=%u fbo_id=%u\n",
	        fbo_w, fbo_h, s_color_tex, s_depth_rb, s_fbo);
	fflush(stderr);
	return true;
}

void nx_webgl_bridge_exit(void) {
	if (!s_initialized) return;
	// Drop the Skia-side SkImage FIRST so the GrDirectContext stops
	// referencing the tenant color texture, THEN free the GL handles.
	s_compose_image.reset();
	if (s_test_vao)  { glDeleteVertexArrays(1, &s_test_vao); s_test_vao = 0; }
	if (s_test_vbo)  { glDeleteBuffers(1, &s_test_vbo); s_test_vbo = 0; }
	if (s_test_prog) { glDeleteProgram(s_test_prog); s_test_prog = 0; }
	if (s_fbo)       { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
	if (s_color_tex) { glDeleteTextures(1, &s_color_tex); s_color_tex = 0; }
	if (s_depth_rb)  { glDeleteRenderbuffers(1, &s_depth_rb); s_depth_rb = 0; }
	s_initialized = false;
	s_fbo_dirty = false;
	s_webgl_owned = false;
	fprintf(stderr, "[webgl-bridge] exit ok frames=%llu avg_boundary=%.2fus\n",
	        (unsigned long long)s_frame_count,
	        s_frame_count > 0 ? s_boundary_us_accum / (double)s_frame_count
	                          : 0.0);
	fflush(stderr);
}

bool nx_webgl_bridge_is_initialized(void) { return s_initialized; }

GLuint nx_webgl_bridge_fbo_id(void) {
	return s_initialized ? s_fbo : 0;
}

void nx_webgl_bridge_fbo_size(int *out_w, int *out_h) {
	if (out_w) *out_w = s_initialized ? s_fbo_w : 0;
	if (out_h) *out_h = s_initialized ? s_fbo_h : 0;
}

void nx_webgl_bridge_mark_fbo_dirty(void) {
	if (s_initialized) s_fbo_dirty = true;
}

void nx_webgl_bridge_set_webgl_owned(bool owned) {
	s_webgl_owned = owned;
	if (owned) {
		fprintf(stderr, "[webgl-bridge] owner=webgl (test driver yields)\n");
		fflush(stderr);
	}
}

bool nx_webgl_bridge_is_webgl_owned(void) { return s_webgl_owned; }

void nx_webgl_bridge_set_auto_flush(bool v) { s_auto_flush = v; }

// Phase 2.D: parameterized sub-rect blit of the tenant FBO onto a destination
// Skia surface. Lets the runtime (brewser-runtime canvas-runner) compose each
// inline <canvas>'s region of the shared FBO at its CSS layout slot — the
// engine-side half of gl.copyBridgeToCanvas. The runtime expresses src
// coordinates in canvas/top-down convention (matching the QuickJS-era
// nx_webgl_copy_bridge_to_canvas contract); we translate to the kBottomLeft
// SkImage's coord system so a request for "top-left box.w x box.h" reads
// the GL viewport's bottom-left output (where Three.js gl.viewport(0,0,w,h)
// actually writes). Does NOT touch s_fbo_dirty — the runtime drives its own
// dirty tracking on this path.
bool nx_webgl_bridge_compose_rect(SkSurface *target,
                                  int src_x, int src_y, int src_w, int src_h,
                                  int dst_x, int dst_y) {
	if (!s_initialized || !target || !s_compose_image) return false;
	if (src_w <= 0 || src_h <= 0) return false;
	SkCanvas *c = target->getCanvas();
	if (!c) return false;
	const int skia_sy = s_fbo_h - src_y - src_h;
	SkRect src = SkRect::MakeXYWH((float)src_x, (float)skia_sy,
	                              (float)src_w, (float)src_h);
	SkRect dst = SkRect::MakeXYWH((float)dst_x, (float)dst_y,
	                              (float)src_w, (float)src_h);
	SkPaint paint;
	c->drawImageRect(s_compose_image.get(), src, dst,
	                 SkSamplingOptions(), &paint,
	                 SkCanvas::kStrict_SrcRectConstraint);
	return true;
}

// Production WebGL → Skia compose path. Cheap no-op on idle frames (no dirty,
// no compose call). Mirrors compose_test but without the test triangle / overlay
// chrome — the WebGL context drives the FBO contents itself.
//
// Gated by s_auto_flush: when the runtime has called gl.setBridgeAutoFlush(false)
// (canvas-runner.ts:302-309 does this in getSharedScreenGL after enabling the
// bridge), the engine MUST NOT compose the FBO to Skia — the runtime drives
// per-canvas readback into each <canvas>'s OffscreenCanvas itself and Skia
// paints the offscreens at the canvases' CSS layout slots. Auto-flushing on
// top of that overwrites the correctly-placed DOM with the whole 1280×720
// FBO at screen origin — the "demos render at screen bottom-left with shell
// stomped" symptom that 2.D fixes. Gate placed BEFORE the dirty check so a
// dirty FBO under auto-flush=false stays unflushed (the runtime's readback
// path consumes it instead).
void nx_webgl_bridge_compose(SkSurface *target) {
	if (!s_auto_flush) return;
	if (!s_initialized || !target || !s_compose_image) return;
	if (!s_fbo_dirty) return;

	SkCanvas *c = target->getCanvas();
	if (!c) return;
	const int target_w = c->imageInfo().width();
	const int target_h = c->imageInfo().height();
	const float dx = (float)((target_w - s_fbo_w) / 2);
	const float dy = (float)((target_h - s_fbo_h) / 2);
	SkPaint paint;
	c->drawImageRect(s_compose_image.get(),
	                 SkRect::MakeXYWH(dx, dy, (float)s_fbo_w, (float)s_fbo_h),
	                 SkSamplingOptions(), &paint);
	s_fbo_dirty = false;
}

void nx_webgl_bridge_compose_test(SkSurface *target) {
	if (!s_initialized || !target || !s_compose_image) return;

	// Phase 2.C: when a WebGL context owns the FBO, the test driver yields its
	// "render triangle into FBO" step (WebGL is responsible for contents) but
	// still allows the compose path to run via nx_webgl_bridge_compose, called
	// from main.cc's present hook. So this function is effectively a no-op when
	// WebGL-owned — the WebGL bridge has its own compose call site.
	if (s_webgl_owned) return;

	const double t = (double)(ts_ns() - s_t_start_ns) / 1.0e9;

	// ===== Bracketed raw-GL section (the load-bearing primitive 2.C wraps) =====
	const uint64_t b0 = ts_ns();
	nx_gl_state_snap_t snap;
	nx_gl_state_save(&snap);

	render_test_into_fbo((float)t);
	s_fbo_dirty = true;  // test driver touched the FBO; let compose run

	nx_gl_state_restore(&snap);
	GrDirectContext *gr = nx_skia_gpu_gr_context();
	if (gr) gr->resetContext();
	const uint64_t b1 = ts_ns();
	const double boundary_us = (double)(b1 - b0) / 1000.0;
	s_boundary_us_accum += boundary_us;
	s_frame_count++;

	// ===== Skia compose phase =====
	SkCanvas *c = target->getCanvas();
	if (!c) return;
	// Target canvas is the persistent screen canvas — DO NOT clear it; we
	// only paint additively on top of whatever the shell drew so the smoke
	// test doesn't wipe shell content (coexistence proof requires SHELL
	// drawing to remain visible alongside our composite).

	// Composite the tenant FBO color texture at a known rect. Centered-ish
	// on a 1280×720 surface (the engine's default). 320,180 + 640,360 lands
	// it in the middle with a margin so the shell underneath is visible at
	// the edges.
	const int target_w = c->imageInfo().width();
	const int target_h = c->imageInfo().height();
	const float dx = (float)((target_w - s_fbo_w) / 2);
	const float dy = (float)((target_h - s_fbo_h) / 2);

	SkPaint compose_paint;
	c->drawImageRect(s_compose_image.get(),
	                 SkRect::MakeXYWH(dx, dy, (float)s_fbo_w, (float)s_fbo_h),
	                 SkSamplingOptions(), &compose_paint);

	// Yellow border around the FBO region so its bounds are unambiguous in a
	// screenshot.
	SkPaint border;
	border.setStyle(SkPaint::kStroke_Style);
	border.setStrokeWidth(3);
	border.setColor(SK_ColorYELLOW);
	c->drawRect(SkRect::MakeXYWH(dx - 2.f, dy - 2.f,
	                             (float)s_fbo_w + 4.f, (float)s_fbo_h + 4.f),
	            border);

	// 2D overlay BANNER on top of the composite. Drawn AFTER drawImageRect
	// so it lands above the FBO contents — proves draw order survives the
	// WebGL→Skia handoff.
	SkPaint banner;
	banner.setColor(SkColorSetARGB(180, 0, 200, 100));
	c->drawRect(SkRect::MakeXYWH(40, 40, 700, 64), banner);
	SkPaint banner_border;
	banner_border.setStyle(SkPaint::kStroke_Style);
	banner_border.setStrokeWidth(2);
	banner_border.setColor(SK_ColorYELLOW);
	c->drawRect(SkRect::MakeXYWH(40, 40, 700, 64), banner_border);

	SkFont font;
	font.setSize(22);
	SkPaint text;
	text.setColor(SK_ColorWHITE);
	c->drawString("Phase 2.B coexistence: WebGL FBO + Skia overlay",
	              56, 80, font, text);

	char hud[160];
	snprintf(hud, sizeof(hud),
	         "frame %llu  boundary %.1fus avg",
	         (unsigned long long)s_frame_count,
	         s_boundary_us_accum / (double)s_frame_count);
	SkPaint hud_text;
	hud_text.setColor(SK_ColorWHITE);
	c->drawString(hud, 56, target_h - 32.f, font, hud_text);

	// Drop a `[webgl-bridge]` line periodically so the log carries proof the
	// bracket ran (and the boundary-cost number) for the smoke gate.
	if (s_frame_count == 1 || s_frame_count == 60 || s_frame_count == 600 ||
	    (s_frame_count % 3600) == 0) {
		fprintf(stderr,
		        "[webgl-bridge] frame=%llu boundary_us=%.2f avg_boundary_us=%.2f\n",
		        (unsigned long long)s_frame_count, boundary_us,
		        s_boundary_us_accum / (double)s_frame_count);
		fflush(stderr);
	}
}
