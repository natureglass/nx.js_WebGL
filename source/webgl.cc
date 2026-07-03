/**
 * WebGL — Phase 2.C: WebGL1 context binding for inline canvases.
 *
 * Phase 2.A laid the shared ES3 context (Skia owns EGL). Phase 2.B realized
 * the coexistence primitive (state save/restore + tenant offscreen FBO +
 * SkImages composite, hardware-proven on Tegra). Phase 2.C grows this file
 * from a null stub into a real WebGL1 context factory: getContext('webgl')
 * returns a non-null object backed by raw GLES3 dispatches into the 2.B
 * tenant FBO, wrapped per-frame in the 2.B bracket.
 *
 * Architecture:
 *   - One module-global WebGLState (`st`). The state matches upstream V8's
 *     beta.5 pattern (commit fb0468f) — WebGL object types (Buffer, Texture,
 *     Program, ...) are wrapped objects holding the GL name; the TS side
 *     passes the JS classes to $.webglInitClass so freshly-minted objects
 *     get the right prototype (instanceof works).
 *   - Each method call lazily ENTERs the per-frame bracket: snapshots Skia's
 *     GL state via nx_gl_state_save, binds the tenant FBO. Subsequent calls
 *     in the same frame stay in the bracket.
 *   - main.cc's present hook calls nx_webgl_compose_if_active() BEFORE
 *     nx_skia_gpu_present(): if the bracket is open, exits it (restores
 *     state + grCtx->resetContext()) then asks the bridge to compose the
 *     FBO into Skia's persistent canvas surface (cheap no-op when the FBO
 *     wasn't touched).
 *   - WebGL-only pixel store flags (UNPACK_FLIP_Y_WEBGL, UNPACK_PREMULTIPLY)
 *     are tracked here; emulation at upload time is a 2.E task — for 2.C
 *     they're stored and otherwise ignored (geometry-cube sets them to
 *     defaults).
 *   - A synthetic error slot backs WebGL-level validation failures
 *     (e.g. invalid object handles); getError() drains it before glGetError.
 *
 * Phase 2.C IS NOT the full 220-method surface. We implement what
 * geometry-cube + Three.js's MeshBasic path empirically calls; 2.E is where
 * the bulk semantics move to brewser-runtime TS and the rest of the surface
 * grows in. Methods the demo doesn't call are not implemented — calling
 * them throws TypeError ("foo is not a function"), which the diagnostic
 * Proxy in webgl-shim.ts logs so the next iteration can add them.
 *
 * Phase 2.G adds WebGL2 (extends this class).
 */
#include "webgl.h"
#include "webgl_bridge.h"
#include "skia_gpu.h"
#include "error.h"
#include "wrap.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "include/gpu/ganesh/GrDirectContext.h"

using namespace v8;

// WebGL-specific enums (not part of GLES headers).
#define NX_GL_UNPACK_FLIP_Y_WEBGL                  0x9240
#define NX_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL       0x9241
#define NX_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL   0x9243
#define NX_GL_CONTEXT_LOST_WEBGL                   0x9242

// Common WebGL1 pname values returned from getParameter (subset).
#define NX_GL_MAX_VERTEX_ATTRIBS                   0x8869
#define NX_GL_MAX_TEXTURE_SIZE                     0x0D33
#define NX_GL_MAX_TEXTURE_IMAGE_UNITS              0x8872
#define NX_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS     0x8B4D
#define NX_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS       0x8B4C
#define NX_GL_MAX_FRAGMENT_UNIFORM_VECTORS         0x8DFD
#define NX_GL_MAX_VERTEX_UNIFORM_VECTORS           0x8DFB
#define NX_GL_MAX_VARYING_VECTORS                  0x8DFC
#define NX_GL_MAX_CUBE_MAP_TEXTURE_SIZE            0x851C
#define NX_GL_MAX_RENDERBUFFER_SIZE                0x84E8
#define NX_GL_MAX_VIEWPORT_DIMS                    0x0D3A
#define NX_GL_ALIASED_LINE_WIDTH_RANGE             0x846E
#define NX_GL_ALIASED_POINT_SIZE_RANGE             0x846D

namespace {

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

enum ObjKind : uint8_t {
	K_BUFFER,
	K_FRAMEBUFFER,
	K_PROGRAM,
	K_RENDERBUFFER,
	K_SHADER,
	K_TEXTURE,
	K_UNIFORM_LOCATION,
	K_ACTIVE_INFO,
	K_SHADER_PRECISION_FORMAT,
	// Phase 2.G.1 cut #3 — v2-only handle kinds. New entries MUST stay
	// BEFORE K_COUNT (which sizes the protos[] array). New entries are
	// stamped on v2 contexts by nx_webgl2_init_class's MAP[] extension.
	K_VERTEX_ARRAY_OBJECT,
	K_COUNT,
};

struct GLObj {
	uint32_t id;
	int32_t loc; // uniform location for K_UNIFORM_LOCATION
	uint8_t kind;
};

struct WebGLState {
	bool bracket_open = false;
	nx_gl_state_snap_t snap;
	GLenum synthetic_error = GL_NO_ERROR;
	// WebGL-only pixel store emulation state (stored only; 2.E does the work).
	bool unpack_flip_y = false;
	bool unpack_premultiply = false;
	int unpack_alignment = 4;
	int pack_alignment = 4;
	// Full user-side GL state snapshot for cross-bracket persistence.
	// Per the WebGL spec, all GL state a demo sets stays in effect for
	// subsequent calls until the demo explicitly changes it. Skia clobbers
	// GL between the runtime's `exit_bracket()` at compose time and the
	// demo's next `enter_bracket()`; without persistence tracking, a demo
	// that binds program / VAO / textures / viewport / blend func / etc.
	// ONCE at init (rather than per-frame) silently ends up rendering
	// under Skia's state on every subsequent frame. Three.js demos re-emit
	// state per material and accidentally survived the bug; raw-WebGL
	// demos like webgl2demo (Sunset Sea) and spectraplay's audio-reactive
	// visualizer expose it. Design: exit_bracket() saves the demo's live
	// state INTO `user_snap` BEFORE restoring Skia's snap. enter_bracket()
	// saves Skia's live state (as before, into `snap`) and then RESTORES
	// `user_snap` after the cut #15 defaults reset. valid flag gates the
	// first-ever enter_bracket where no user state has been captured yet
	// (cut #15's WebGL-defaults reset handles Three.js's initial cache
	// consistency; user_snap takes over from frame 2 onward).
	nx_gl_state_snap_t user_snap;
	bool user_snap_valid = false;
	// Auto-allocated VAO to hold user's attribute-enable / pointer state
	// for WebGL 1 demos (which never call bindVertexArray). Without this,
	// the demo's `enableVertexAttribArray` + `vertexAttribPointer` calls
	// operate on GL's default VAO (VAO 0), which Ganesh also uses for its
	// own rendering — Skia clobbers the demo's attribute setup between
	// frames, and drawArrays reads stale/wrong attribute pointers. Caught
	// while porting the sensors gyro cube. Allocated once at first
	// enter_bracket and bound before user's first GL call each frame so
	// demo's attribute state has a home Ganesh doesn't touch. WebGL 2
	// demos that call bindVertexArray explicitly override this via the
	// shadow (user_snap.vao) — their intent wins.
	GLuint auto_user_vao = 0;
	// Currently bound DRAW_FRAMEBUFFER as the JS sees it. 0 = "default" =
	// tenant FBO (the only difference from a browser WebGL where 0 = swap
	// chain back buffer). When the user binds a non-null framebuffer object,
	// we forward the FBO name directly.
	uint32_t bound_fbo_js = 0;
	// Bookkeeping for the per-frame "did we touch the FBO?" signal — set when
	// the bound target IS the default (tenant) FBO. The bridge dirties on
	// every draw/clear that lands in tenant; non-default-FBO writes don't.
	bool draw_into_default = true;
	// Drawing buffer dimensions (canvas w/h reported via drawingBufferWidth/
	// drawingBufferHeight, also drives default viewport / scissor).
	int width = 640;
	int height = 360;
	// Prototypes for the WebGL object classes (set by $.webglInitClass).
	Global<Object> protos[K_COUNT];
};

WebGLState *st = nullptr;

// Native GL extension cache. Populated once at first WebGL context
// creation via glGetStringi(GL_EXTENSIONS, i) over GL_NUM_EXTENSIONS —
// the ES3 path; the legacy glGetString(GL_EXTENSIONS) is deprecated
// for 3.x contexts and Mesa returns NULL for it there. Populate emits
// a one-shot [gl-ext-dump] boot log the next hardware session greps
// to resolve all bucket-B extension rows (s3tc, s3tc_srgb, astc,
// anisotropic, EXT_frag_depth, WEBGL_draw_buffers, EXT_shader_texture_lod,
// disjoint_timer_query, etc.) against the actual Mesa-Nouveau / Tegra
// X1 driver capability set. Introspection surface only — no
// advertisement changes to SUPPORTED[] at [webgl.cc:782] this phase;
// that is gated on the hardware dump per the phase-0 plan.
static std::vector<std::string> s_native_ext_list;
static std::string s_native_exts_joined;
static bool s_native_exts_populated = false;

static void populate_native_extensions() {
	if (s_native_exts_populated) return;
	s_native_exts_populated = true;
	// GL error hygiene (rider B) — drain any pre-existing error before the
	// enumeration so any residual we observe below is attributable to our
	// own calls, and drain at the end so downstream engine code (notably
	// the commit-2 [bridge-fbo] completeness assert) never sees a stale
	// error left by us.
	while (glGetError() != GL_NO_ERROR) { /* drain */ }
	GLint n = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	if (n < 0) n = 0;
	s_native_ext_list.reserve((size_t)n);
	for (GLint i = 0; i < n; i++) {
		const GLubyte *e = glGetStringi(GL_EXTENSIONS, (GLuint)i);
		if (e) s_native_ext_list.emplace_back((const char *)e);
	}
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr,
		        "[gl-ext-dump:err] enumeration left GL error 0x%x — draining\n",
		        (unsigned)err);
		while (glGetError() != GL_NO_ERROR) { /* drain */ }
	}
	std::sort(s_native_ext_list.begin(), s_native_ext_list.end());
	size_t total = 0;
	for (const auto &e : s_native_ext_list) total += e.size() + 1;
	s_native_exts_joined.reserve(total);
	for (size_t i = 0; i < s_native_ext_list.size(); i++) {
		if (i) s_native_exts_joined.push_back(' ');
		s_native_exts_joined += s_native_ext_list[i];
	}
	// [gl-ext-dump] one-shot — grep target for hardware capture.
	fprintf(stderr, "[gl-ext-dump] count=%d\n", (int)s_native_ext_list.size());
	for (const auto &e : s_native_ext_list) {
		fprintf(stderr, "[gl-ext-dump] %s\n", e.c_str());
	}
	fflush(stderr);
}

void free_gl_obj(GLObj *o) { delete o; }

Local<Object> new_gl_obj(Isolate *iso, uint8_t kind, GLuint id,
                         GLint loc = -1) {
	Local<Object> obj = nx::NewWrapped(iso);
	if (st && !st->protos[kind].IsEmpty()) {
		obj->SetPrototype(iso->GetCurrentContext(), st->protos[kind].Get(iso))
		    .Check();
	}
	GLObj *o = new GLObj{id, loc, kind};
	nx::Wrap<GLObj>(iso, obj, o, free_gl_obj);
	return obj;
}

GLObj *get_gl_obj(Local<Value> v) {
	if (v.IsEmpty() || !v->IsObject())
		return nullptr;
	return nx::Unwrap<GLObj>(v);
}

GLuint obj_id(Local<Value> v) {
	GLObj *o = get_gl_obj(v);
	return o ? o->id : 0;
}

GLint uniform_loc(Local<Value> v) {
	GLObj *o = get_gl_obj(v);
	return (o && o->kind == K_UNIFORM_LOCATION) ? o->loc : -1;
}

void record_error(GLenum err) {
	if (st && st->synthetic_error == GL_NO_ERROR)
		st->synthetic_error = err;
}

inline Local<Context> cur(Isolate *iso) { return iso->GetCurrentContext(); }

// ---------------------------------------------------------------------------
// Argument unwrap helpers
// ---------------------------------------------------------------------------

inline uint32_t a_u32(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->Uint32Value(cur(info.GetIsolate())).FromMaybe(0);
}
inline int32_t a_i32(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->Int32Value(cur(info.GetIsolate())).FromMaybe(0);
}
inline double a_f64(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->NumberValue(cur(info.GetIsolate())).FromMaybe(0.0);
}
inline float a_f32(const FunctionCallbackInfo<Value> &info, int i) {
	return (float)a_f64(info, i);
}
inline bool a_bool(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->BooleanValue(info.GetIsolate());
}
inline int64_t a_i64(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->IntegerValue(cur(info.GetIsolate())).FromMaybe(0);
}

// Returns raw bytes of any ArrayBufferView (honoring byteOffset).
uint8_t *view_bytes(Local<Value> v, size_t *len) {
	if (v.IsEmpty() || !v->IsArrayBufferView()) {
		if (len) *len = 0;
		return nullptr;
	}
	Local<ArrayBufferView> view = v.As<ArrayBufferView>();
	if (len) *len = view->ByteLength();
	return (uint8_t *)view->Buffer()->Data() + view->ByteOffset();
}

// Extract a contiguous f32 list from either Float32Array (zero-copy) or a
// plain JS array (copied into `tmp`).
bool f32_list(Isolate *iso, Local<Value> v, std::vector<float> &tmp,
              const float **out, size_t *n) {
	if (!v.IsEmpty() && v->IsFloat32Array()) {
		Local<Float32Array> ta = v.As<Float32Array>();
		*n = ta->Length();
		*out = (const float *)((uint8_t *)ta->Buffer()->Data() + ta->ByteOffset());
		return true;
	}
	if (!v.IsEmpty() && v->IsArray()) {
		Local<Array> arr = v.As<Array>();
		Local<Context> ctx = cur(iso);
		uint32_t len = arr->Length();
		tmp.resize(len);
		for (uint32_t i = 0; i < len; i++) {
			Local<Value> el;
			tmp[i] = 0.f;
			if (arr->Get(ctx, i).ToLocal(&el))
				tmp[i] = (float)el->NumberValue(ctx).FromMaybe(0.0);
		}
		*out = tmp.data();
		*n = tmp.size();
		return true;
	}
	return false;
}

// Extract an i32 list from Int32Array / plain JS array.
bool i32_list(Isolate *iso, Local<Value> v, std::vector<int32_t> &tmp,
              const int32_t **out, size_t *n) {
	if (!v.IsEmpty() && v->IsInt32Array()) {
		Local<Int32Array> ta = v.As<Int32Array>();
		*n = ta->Length();
		*out = (const int32_t *)((uint8_t *)ta->Buffer()->Data() + ta->ByteOffset());
		return true;
	}
	if (!v.IsEmpty() && v->IsArray()) {
		Local<Array> arr = v.As<Array>();
		Local<Context> ctx = cur(iso);
		uint32_t len = arr->Length();
		tmp.resize(len);
		for (uint32_t i = 0; i < len; i++) {
			Local<Value> el;
			tmp[i] = 0;
			if (arr->Get(ctx, i).ToLocal(&el))
				tmp[i] = el->Int32Value(ctx).FromMaybe(0);
		}
		*out = tmp.data();
		*n = tmp.size();
		return true;
	}
	return false;
}

// UTF-8 string from JS, owned by caller (free with delete[]).
char *take_string(Isolate *iso, Local<Value> v) {
	if (v.IsEmpty() || !v->IsString()) {
		char *empty = new char[1];
		empty[0] = 0;
		return empty;
	}
	Local<String> s = v.As<String>();
	int len = s->Utf8Length(iso);
	char *buf = new char[len + 1];
	s->WriteUtf8(iso, buf, len);
	buf[len] = 0;
	return buf;
}

// ---------------------------------------------------------------------------
// Per-frame bracket — the 2.B contract
// ---------------------------------------------------------------------------

void enter_bracket() {
	if (!st) return;
	if (st->bracket_open) return;
	if (!nx_webgl_bridge_is_initialized()) return;
	nx_gl_state_save(&st->snap);
	// Bind the user-currently-bound FBO if they switched; otherwise default
	// (tenant FBO). Either way, our viewport / clear etc. go to the right
	// target without the user noticing the redirect.
	GLuint target_fbo = st->bound_fbo_js == 0
	                        ? nx_webgl_bridge_fbo_id()
	                        : st->bound_fbo_js;
	glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	// Phase 2.G.1 cut #15 — reset the WebGL default state that Skia's
	// Ganesh might have left in a different configuration. Three.js's
	// WebGLState cache initializes assuming these WebGL defaults; if
	// Skia left GL in a different state, Three.js's cache is out of sync
	// and cache short-circuits prevent Three.js from re-emitting the
	// state. Cut #15v log confirmed depth_mask+color_mask+clear_depth
	// are already correct at gl.clear time — meaning the depth CLEAR
	// works. The rejection must be at depth TEST time (depth_func!=LESS,
	// or depth_range inverted). Also reset cull/front-face and stencil
	// state for completeness — these are all cheap.
	glDepthMask(GL_TRUE);
	glStencilMask(0xFF);
	glDepthFunc(GL_LESS);
	glDepthRangef(0.0f, 1.0f);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	// Auto-allocate the persistence VAO if we haven't yet. Bind it so
	// user's attribute state (enableVertexAttribArray / vertexAttribPointer)
	// lives in a VAO Ganesh doesn't touch. See auto_user_vao field comment.
	if (st->auto_user_vao == 0) {
		glGenVertexArrays(1, &st->auto_user_vao);
	}
	glBindVertexArray(st->auto_user_vao);
	// Establish user_snap baseline on the very first enter (captures
	// Skia's initial state + cut #15 defaults + auto_user_vao just bound)
	// OR restore accumulated user state on subsequent enters. See
	// exit_bracket() comment for why we no longer save user_snap at exit —
	// Skia's 2D rendering has already clobbered GL state by the time
	// copyBridgeToScreen fires exit_bracket, so live state is not the
	// demo's intent. Instead, each state-modifying w_* setter updates the
	// relevant user_snap field directly, so user_snap always reflects
	// what the demo INTENDED (per WebGL spec, that's what should persist
	// across bracket boundaries).
	if (!st->user_snap_valid) {
		nx_gl_state_save(&st->user_snap);
		st->user_snap_valid = true;
	} else {
		nx_gl_state_restore(&st->user_snap);
	}
	st->bracket_open = true;
}

void exit_bracket() {
	if (!st || !st->bracket_open) return;
	// Do NOT save user_snap here: by the time exit_bracket fires (from
	// w_copy_bridge_to_canvas after Skia's paintLiveOverlay already ran,
	// or from nx_webgl_compose_if_active at present), the shared GL
	// context's live state is Skia-clobbered — it no longer reflects the
	// demo's intent. Instead, user_snap is updated INCREMENTALLY as each
	// state-modifying w_* setter fires, so it captures what the demo
	// actually asked for regardless of what Skia does behind the scenes.
	// The sensors-cube regression (webgl 1 raw cube, "renders broken")
	// pinned this: demo called gl.enable(DEPTH_TEST) at init; Skia's 2D
	// paints disabled DT for compositing; exit_bracket saved DT=0 as
	// "user state"; every subsequent frame's enter_bracket restored DT=0;
	// cube drew without depth test → back faces show through front faces.
	nx_gl_state_restore(&st->snap);
	GrDirectContext *gr = nx_skia_gpu_gr_context();
	if (gr) gr->resetContext();
	st->bracket_open = false;
}

inline void touch_fbo() {
	if (st && st->draw_into_default) nx_webgl_bridge_mark_fbo_dirty();
}

// ---------------------------------------------------------------------------
// Method implementations — the 2.C allowlist.
// Order matches upstream's table for easy diffing. Methods that the slice
// demo provably doesn't call are NOT here; calling them throws TypeError
// (caught + logged by the diagnostic Proxy). Each iteration adds whichever
// methods the proxy log reveals are missing.
// ---------------------------------------------------------------------------

#define FN(name) static void name(const FunctionCallbackInfo<Value> &info)

// ----- State / capability -----
// Shadow-tracking pattern: state-modifying w_* setters update
// st->user_snap.<field> after glCall so user's INTENT is captured
// regardless of Skia clobbers between bracket close/reopen. See
// enter_bracket / exit_bracket comments + [[reference-bracket-state-
// persistence-bug]] for the design rationale.
FN(w_viewport) {
	enter_bracket();
	const GLint x = a_i32(info, 0), y = a_i32(info, 1);
	const GLint w = a_i32(info, 2), h = a_i32(info, 3);
	glViewport(x, y, w, h);
	if (st) {
		st->user_snap.viewport[0] = x;
		st->user_snap.viewport[1] = y;
		st->user_snap.viewport[2] = w;
		st->user_snap.viewport[3] = h;
	}
}
FN(w_scissor) {
	enter_bracket();
	glScissor(a_i32(info, 0), a_i32(info, 1), a_i32(info, 2), a_i32(info, 3));
}
FN(w_enable) {
	enter_bracket();
	const GLenum cap = a_u32(info, 0);
	glEnable(cap);
	if (st) {
		switch (cap) {
			case GL_BLEND:        st->user_snap.blend        = GL_TRUE; break;
			case GL_DEPTH_TEST:   st->user_snap.depth_test   = GL_TRUE; break;
			case GL_CULL_FACE:    st->user_snap.cull         = GL_TRUE; break;
			case GL_SCISSOR_TEST: st->user_snap.scissor      = GL_TRUE; break;
			case GL_STENCIL_TEST: st->user_snap.stencil_test = GL_TRUE; break;
		}
	}
}
FN(w_disable) {
	enter_bracket();
	const GLenum cap = a_u32(info, 0);
	glDisable(cap);
	if (st) {
		switch (cap) {
			case GL_BLEND:        st->user_snap.blend        = GL_FALSE; break;
			case GL_DEPTH_TEST:   st->user_snap.depth_test   = GL_FALSE; break;
			case GL_CULL_FACE:    st->user_snap.cull         = GL_FALSE; break;
			case GL_SCISSOR_TEST: st->user_snap.scissor      = GL_FALSE; break;
			case GL_STENCIL_TEST: st->user_snap.stencil_test = GL_FALSE; break;
		}
	}
}
FN(w_is_enabled) {
	enter_bracket();
	GLboolean b = glIsEnabled(a_u32(info, 0));
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), b == GL_TRUE));
}
FN(w_depth_func) { enter_bracket(); glDepthFunc(a_u32(info, 0)); }
FN(w_depth_mask) {
	enter_bracket();
	const GLboolean m = a_bool(info, 0);
	glDepthMask(m);
	if (st) st->user_snap.depth_mask = m;
}
FN(w_depth_range) { enter_bracket(); glDepthRangef(a_f32(info, 0), a_f32(info, 1)); }
FN(w_cull_face) { enter_bracket(); glCullFace(a_u32(info, 0)); }
FN(w_front_face) { enter_bracket(); glFrontFace(a_u32(info, 0)); }
FN(w_blend_func) {
	enter_bracket();
	const GLenum s = a_u32(info, 0), d = a_u32(info, 1);
	glBlendFunc(s, d);
	if (st) {
		st->user_snap.blend_src_rgb = st->user_snap.blend_src_a = s;
		st->user_snap.blend_dst_rgb = st->user_snap.blend_dst_a = d;
	}
}
FN(w_blend_func_separate) {
	enter_bracket();
	const GLenum sRgb = a_u32(info, 0), dRgb = a_u32(info, 1);
	const GLenum sA = a_u32(info, 2), dA = a_u32(info, 3);
	glBlendFuncSeparate(sRgb, dRgb, sA, dA);
	if (st) {
		st->user_snap.blend_src_rgb = sRgb;
		st->user_snap.blend_dst_rgb = dRgb;
		st->user_snap.blend_src_a   = sA;
		st->user_snap.blend_dst_a   = dA;
	}
}
FN(w_blend_equation) { enter_bracket(); glBlendEquation(a_u32(info, 0)); }
FN(w_blend_equation_separate) {
	enter_bracket();
	glBlendEquationSeparate(a_u32(info, 0), a_u32(info, 1));
}
FN(w_blend_color) {
	enter_bracket();
	glBlendColor(a_f32(info, 0), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3));
}
FN(w_color_mask) {
	enter_bracket();
	const GLboolean r = a_bool(info, 0), g = a_bool(info, 1);
	const GLboolean b = a_bool(info, 2), a = a_bool(info, 3);
	glColorMask(r, g, b, a);
	if (st) {
		st->user_snap.color_mask[0] = r;
		st->user_snap.color_mask[1] = g;
		st->user_snap.color_mask[2] = b;
		st->user_snap.color_mask[3] = a;
	}
}
FN(w_stencil_func) {
	enter_bracket();
	glStencilFunc(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2));
}
FN(w_stencil_func_separate) {
	enter_bracket();
	glStencilFuncSeparate(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2),
	                      a_u32(info, 3));
}
FN(w_stencil_op) {
	enter_bracket();
	glStencilOp(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2));
}
FN(w_stencil_op_separate) {
	enter_bracket();
	glStencilOpSeparate(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                    a_u32(info, 3));
}
FN(w_stencil_mask) {
	enter_bracket();
	const GLuint m = a_u32(info, 0);
	glStencilMask(m);
	if (st) st->user_snap.stencil_mask = (GLint)m;
}
FN(w_stencil_mask_separate) {
	enter_bracket();
	glStencilMaskSeparate(a_u32(info, 0), a_u32(info, 1));
}
FN(w_polygon_offset) {
	enter_bracket();
	glPolygonOffset(a_f32(info, 0), a_f32(info, 1));
}
FN(w_sample_coverage) {
	enter_bracket();
	glSampleCoverage(a_f32(info, 0), a_bool(info, 1));
}
FN(w_line_width) { enter_bracket(); glLineWidth(a_f32(info, 0)); }
FN(w_hint) { enter_bracket(); glHint(a_u32(info, 0), a_u32(info, 1)); }

FN(w_clear) {
	enter_bracket();
	glClear(a_u32(info, 0));
	touch_fbo();
}
FN(w_clear_color) {
	enter_bracket();
	const GLfloat r = a_f32(info, 0), g = a_f32(info, 1);
	const GLfloat b = a_f32(info, 2), a = a_f32(info, 3);
	glClearColor(r, g, b, a);
	if (st) {
		st->user_snap.clear_color[0] = r;
		st->user_snap.clear_color[1] = g;
		st->user_snap.clear_color[2] = b;
		st->user_snap.clear_color[3] = a;
	}
}
FN(w_clear_depth) { enter_bracket(); glClearDepthf(a_f32(info, 0)); }
FN(w_clear_stencil) { enter_bracket(); glClearStencil(a_i32(info, 0)); }
FN(w_finish) { enter_bracket(); glFinish(); }
FN(w_flush) { enter_bracket(); glFlush(); }

FN(w_pixel_storei) {
	const GLenum pname = a_u32(info, 0);
	const GLint val = a_i32(info, 1);
	switch (pname) {
	case NX_GL_UNPACK_FLIP_Y_WEBGL:
		if (st) st->unpack_flip_y = (val != 0);
		return;
	case NX_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		if (st) st->unpack_premultiply = (val != 0);
		return;
	case NX_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		// Ignored; stored value irrelevant for the slice. 2.E can implement.
		return;
	case GL_UNPACK_ALIGNMENT:
		if (st) st->unpack_alignment = val;
		break;
	case GL_PACK_ALIGNMENT:
		if (st) st->pack_alignment = val;
		break;
	}
	enter_bracket();
	glPixelStorei(pname, val);
}

// ----- Query -----
FN(w_get_error) {
	enter_bracket();
	GLenum err = (st && st->synthetic_error != GL_NO_ERROR)
	                 ? st->synthetic_error
	                 : glGetError();
	if (st) st->synthetic_error = GL_NO_ERROR;
	info.GetReturnValue().Set(Uint32::NewFromUnsigned(info.GetIsolate(), err));
}

FN(w_get_parameter) {
	enter_bracket();
	Isolate *iso = info.GetIsolate();
	const GLenum pname = a_u32(info, 0);
	switch (pname) {
	case GL_VERSION:
	case GL_VENDOR:
	case GL_RENDERER:
	case GL_SHADING_LANGUAGE_VERSION: {
		const GLubyte *s = glGetString(pname);
		const char *cs = s ? (const char *)s : "";
		info.GetReturnValue().Set(
		    String::NewFromUtf8(iso, cs, NewStringType::kNormal)
		        .ToLocalChecked());
		return;
	}
	case GL_DEPTH_WRITEMASK:
	case GL_COLOR_WRITEMASK: {
		GLboolean v[4] = {0, 0, 0, 0};
		glGetBooleanv(pname, v);
		if (pname == GL_COLOR_WRITEMASK) {
			Local<Array> arr = Array::New(iso, 4);
			Local<Context> c = cur(iso);
			for (int i = 0; i < 4; i++)
				arr->Set(c, i, Boolean::New(iso, v[i] != 0)).Check();
			info.GetReturnValue().Set(arr);
		} else {
			info.GetReturnValue().Set(Boolean::New(iso, v[0] != 0));
		}
		return;
	}
	case GL_COLOR_CLEAR_VALUE:
	case GL_BLEND_COLOR:
	case GL_DEPTH_RANGE:
	case NX_GL_ALIASED_LINE_WIDTH_RANGE:
	case NX_GL_ALIASED_POINT_SIZE_RANGE: {
		GLfloat v[4] = {0, 0, 0, 0};
		glGetFloatv(pname, v);
		int n = (pname == GL_DEPTH_RANGE || pname == NX_GL_ALIASED_LINE_WIDTH_RANGE ||
		         pname == NX_GL_ALIASED_POINT_SIZE_RANGE) ? 2 : 4;
		Local<ArrayBuffer> ab = ArrayBuffer::New(iso, n * 4);
		memcpy(ab->Data(), v, n * 4);
		info.GetReturnValue().Set(Float32Array::New(ab, 0, n));
		return;
	}
	case NX_GL_MAX_VIEWPORT_DIMS: {
		GLint v[2] = {0, 0};
		glGetIntegerv(pname, v);
		Local<ArrayBuffer> ab = ArrayBuffer::New(iso, 8);
		memcpy(ab->Data(), v, 8);
		info.GetReturnValue().Set(Int32Array::New(ab, 0, 2));
		return;
	}
	default: {
		GLint v = 0;
		glGetIntegerv(pname, &v);
		info.GetReturnValue().Set(Int32::New(iso, v));
		return;
	}
	}
}

FN(w_get_extension) {
	Isolate *iso = info.GetIsolate();
	if (info.Length() < 1 || !info[0]->IsString()) return;
	String::Utf8Value name_utf8(iso, info[0]);
	const char *name = *name_utf8;
	if (!name) return;
	Local<Context> c = cur(iso);
	auto make_obj_with = [&](std::initializer_list<std::pair<const char *, uint32_t>> kvs) {
		Local<Object> o = Object::New(iso);
		for (const auto &kv : kvs) {
			o->Set(c, String::NewFromUtf8(iso, kv.first).ToLocalChecked(),
			       Uint32::NewFromUnsigned(iso, kv.second)).Check();
		}
		info.GetReturnValue().Set(o);
	};
	// WebGL1 extensions whose enum constants are numerically identical to
	// ES3 core enums — returning an object exposes the constants Three.js
	// queries; subsequent gl.<method>(EXT_CONST) calls reach native ES3
	// (which already implements the underlying capability) transparently.
	// No new engine GL plumbing needed.
	if (strcmp(name, "EXT_blend_minmax") == 0) {
		make_obj_with({{"MIN_EXT", 0x8007}, {"MAX_EXT", 0x8008}});
		return;
	}
	if (strcmp(name, "OES_element_index_uint") == 0) {
		// No enum values — empty object is the standard signature. Indicates
		// "you may use gl.UNSIGNED_INT (0x1405) with drawElements", which
		// ES3 supports natively.
		info.GetReturnValue().Set(Object::New(iso));
		return;
	}
	if (strcmp(name, "OES_standard_derivatives") == 0) {
		make_obj_with({{"FRAGMENT_SHADER_DERIVATIVE_HINT_OES", 0x8B8B}});
		return;
	}
	if (strcmp(name, "OES_texture_float") == 0 ||
	    strcmp(name, "OES_texture_float_linear") == 0 ||
	    strcmp(name, "OES_texture_half_float_linear") == 0) {
		info.GetReturnValue().Set(Object::New(iso));
		return;
	}
	if (strcmp(name, "OES_texture_half_float") == 0) {
		// HALF_FLOAT_OES = 0x8D61 in WebGL1's extension, vs 0x140B (HALF_FLOAT)
		// in ES3 core. Three.js v1 code paths use the OES value. ES3 actually
		// accepts BOTH (the driver treats 0x8D61 + 0x140B as aliases on Mesa
		// Nouveau — confirmed in the fork's webgl_egl.c handling).
		make_obj_with({{"HALF_FLOAT_OES", 0x8D61}});
		return;
	}
	if (strcmp(name, "EXT_sRGB") == 0) {
		// Three.js sets texture.colorSpace = SRGB and queries this extension
		// on WebGL1 to know whether to use SRGB_EXT vs SRGB. The constants
		// map cleanly to ES3 (SRGB8 / SRGB8_ALPHA8 internalformats are core).
		make_obj_with({{"SRGB_EXT", 0x8C40},
		                {"SRGB_ALPHA_EXT", 0x8C42},
		                {"SRGB8_ALPHA8_EXT", 0x8C43},
		                {"FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT", 0x8210}});
		return;
	}
	if (strcmp(name, "WEBGL_depth_texture") == 0) {
		make_obj_with({{"UNSIGNED_INT_24_8_WEBGL", 0x84FA}});
		return;
	}
	// Everything else: not advertised yet. Return null (the spec value for
	// "extension not supported"). 2.E will widen this list as the slice
	// demos hit it.
	info.GetReturnValue().SetNull();
}
FN(w_get_supported_extensions) {
	Isolate *iso = info.GetIsolate();
	// Advertise the extensions that w_get_extension hands back. Three.js's
	// WebGLCapabilities checks getSupportedExtensions().indexOf(name) before
	// calling getExtension on a few of them.
	static const char *const SUPPORTED[] = {
	    "EXT_blend_minmax",
	    "OES_element_index_uint",
	    "OES_standard_derivatives",
	    "OES_texture_float",
	    "OES_texture_float_linear",
	    "OES_texture_half_float",
	    "OES_texture_half_float_linear",
	    "EXT_sRGB",
	    "WEBGL_depth_texture",
	};
	const int N = (int)(sizeof(SUPPORTED) / sizeof(SUPPORTED[0]));
	Local<Array> arr = Array::New(iso, N);
	Local<Context> c = cur(iso);
	for (int i = 0; i < N; i++) {
		arr->Set(c, i,
		         String::NewFromUtf8(iso, SUPPORTED[i]).ToLocalChecked()).Check();
	}
	info.GetReturnValue().Set(arr);
}
// Phase-0 gap fill — restore native GL extension visibility the QuickJS-
// era engine exposed via `gl.getBackendInfo().glExtensions` and which the
// V8 migration dropped. Internal-only native (leading underscore names
// signal "shim consumers only, not spec surface"). Returns the joined
// space-separated native GL extension string (sorted alphabetically) so
// the brewser-runtime getBackendInfo shim can pass it through unchanged
// to com.natureglass.webglreport, which splits it back into tokens at
// [webglreport.js:199]. See WEBGL_EXTENSION_GAP.md §Broken-introspection A
// and phase-0 ledger entry.
FN(w_get_native_extensions_string) {
	populate_native_extensions();
	Isolate *iso = info.GetIsolate();
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, s_native_exts_joined.c_str(),
	                        NewStringType::kNormal,
	                        (int)s_native_exts_joined.size())
	        .ToLocalChecked());
}

// Phase-0 gap fill — EGL version string for the getBackendInfo shim's
// `eglMajor`/`eglMinor` fields (matches pre-migration schema at
// [nxjs-source/source/webgl_egl.c:8433-8509]). Uses eglQueryString
// (idempotent, no re-init) on the display Skia already brought up.
// Returned form: "major.minor" (e.g. "1.5"); trailing vendor blob per
// EGL spec is stripped so the shim's parse is a single split-on-dot.
// Empty string means the display isn't up yet (never happens at
// getContext time; guard is defensive).
FN(w_get_egl_version) {
	Isolate *iso = info.GetIsolate();
	EGLDisplay dpy = nx_skia_gpu_egl_display();
	const char *raw = dpy ? eglQueryString(dpy, EGL_VERSION) : nullptr;
	std::string out;
	if (raw) {
		const char *end = raw;
		while (*end && *end != ' ' && *end != '\t') end++;
		out.assign(raw, (size_t)(end - raw));
	}
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, out.c_str(), NewStringType::kNormal,
	                        (int)out.size())
	        .ToLocalChecked());
}

FN(w_is_context_lost) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), false));
}
FN(w_get_context_attributes) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = cur(iso);
	Local<Object> o = Object::New(iso);
	auto setb = [&](const char *k, bool v) {
		o->Set(c, String::NewFromUtf8(iso, k).ToLocalChecked(),
		       Boolean::New(iso, v)).Check();
	};
	setb("alpha", true);
	setb("antialias", false);
	setb("depth", true);
	setb("stencil", true);
	setb("premultipliedAlpha", true);
	setb("preserveDrawingBuffer", false);
	setb("desynchronized", false);
	setb("failIfMajorPerformanceCaveat", false);
	o->Set(c, String::NewFromUtf8(iso, "powerPreference").ToLocalChecked(),
	       String::NewFromUtf8(iso, "default").ToLocalChecked()).Check();
	info.GetReturnValue().Set(o);
}

FN(w_get_shader_precision_format) {
	Isolate *iso = info.GetIsolate();
	GLint range[2] = {0, 0};
	GLint precision = 0;
	glGetShaderPrecisionFormat(a_u32(info, 0), a_u32(info, 1), range, &precision);
	Local<Object> obj = new_gl_obj(iso, K_SHADER_PRECISION_FORMAT, 0);
	Local<Context> c = cur(iso);
	obj->Set(c, String::NewFromUtf8(iso, "rangeMin").ToLocalChecked(),
	         Int32::New(iso, range[0])).Check();
	obj->Set(c, String::NewFromUtf8(iso, "rangeMax").ToLocalChecked(),
	         Int32::New(iso, range[1])).Check();
	obj->Set(c, String::NewFromUtf8(iso, "precision").ToLocalChecked(),
	         Int32::New(iso, precision)).Check();
	info.GetReturnValue().Set(obj);
}

// ----- Shader -----
FN(w_create_shader) {
	enter_bracket();
	GLuint s = glCreateShader(a_u32(info, 0));
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_SHADER, s));
}
FN(w_delete_shader) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteShader(id);
}
FN(w_is_shader) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsShader(obj_id(info[0])) == GL_TRUE));
}

// Phase 2.F.1: PMREMGGXConvolution FS replacement. Mesa-Nouveau on Tegra X1
// aborts glDrawArrays when the original Three.js r184 PMREM convolution FS
// (uint + bitwise Hammersley radicalInverse_VdC in the GGX importance-sample
// loop) is compiled, even when the loop is statically unreachable. The
// replacement keeps the cubeUV math (getFace, getUV, inline bilinearCubeUV)
// but uses a 5-tap unrolled cross blur using a tangent/bitangent basis. No
// uint, no bitwise, no for, no sin/cos, no early-return conditional — every
// construct that crashed an iteration during the QuickJS-era 13-iteration
// bisect is excluded. Verbatim re-port from
// nxjs-source/source/webgl_egl.c:8629-8800 per
// [[reference-pmrem-tegra-compiler-workaround]].
//
// Per-pass roughness scales the kernel radius; across PMREM's cumulative
// passes this produces a widening hemisphere coverage in the cube_uv mip
// chain — visually inferior to upstream's roughness-based GGX blur (perfect-
// mirror-ish look at all roughness levels) but the only way to get PMREM
// through Tegra/Mesa GLES without driver abort.
//
// Texel constants (CUBEUV_MAX_MIP / CUBEUV_TEXEL_WIDTH / CUBEUV_TEXEL_HEIGHT)
// are PARSED from the Three.js-emitted source — hardcoding them only matched
// cubeSize=256 (lodMax=8); for 2048-wide equirects → cubeSize=512 → lodMax=9
// the hardcoded constants put UVs outside the texture and multi-tap writes
// crashed Mesa.
//
// Gate: shader_type == GL_FRAGMENT_SHADER AND source contains
// "PMREMGGXConvolution". Both conditions must hold; the substring guard
// alone is r184-PMREM-specific (Three.js r162's PMREM FS doesn't contain
// this name) and the type guard makes accidental match in a vertex shader
// (extremely unlikely) safe.
//
// Returns a heap-allocated replacement source (caller delete[]) when the
// substitution fires, or nullptr when the source should be passed through
// unchanged.
static char *maybe_replace_pmrem_fs(GLuint shader, const char *src) {
	GLint shader_type = 0;
	glGetShaderiv(shader, GL_SHADER_TYPE, &shader_type);
	if (shader_type != GL_FRAGMENT_SHADER) return nullptr;
	if (strstr(src, "PMREMGGXConvolution") == nullptr) return nullptr;

	double max_mip = 8.0;
	double texel_w = 1.0 / 1536.0;
	double texel_h = 1.0 / 2048.0;
	if (const char *p = strstr(src, "#define CUBEUV_MAX_MIP ")) {
		max_mip = strtod(p + strlen("#define CUBEUV_MAX_MIP "), nullptr);
	}
	if (const char *p = strstr(src, "#define CUBEUV_TEXEL_WIDTH ")) {
		texel_w = strtod(p + strlen("#define CUBEUV_TEXEL_WIDTH "), nullptr);
	}
	if (const char *p = strstr(src, "#define CUBEUV_TEXEL_HEIGHT ")) {
		texel_h = strtod(p + strlen("#define CUBEUV_TEXEL_HEIGHT "), nullptr);
	}

	char *built = new char[16384];
	const int n = snprintf(built, 16384,
		"#version 300 es\n"
		"precision highp float;\n"
		"precision highp sampler2D;\n"
		"in vec3 vOutputDirection;\n"
		"layout(location = 0) out highp vec4 pc_fragColor;\n"
		"uniform sampler2D envMap;\n"
		"uniform float roughness;\n"
		"uniform float mipInt;\n"
		"#define cubeUV_minMipLevel 4.0\n"
		"#define cubeUV_minTileSize 16.0\n"
		"#define CUBEUV_TEXEL_WIDTH %.10f\n"
		"#define CUBEUV_TEXEL_HEIGHT %.10f\n"
		"#define CUBEUV_MAX_MIP %.4f\n"
		"float getFace(vec3 d) {\n"
		"  vec3 a = abs(d);\n"
		"  if (a.x > a.z) {\n"
		"    if (a.x > a.y) return d.x > 0.0 ? 0.0 : 3.0;\n"
		"    return d.y > 0.0 ? 1.0 : 4.0;\n"
		"  }\n"
		"  if (a.z > a.y) return d.z > 0.0 ? 2.0 : 5.0;\n"
		"  return d.y > 0.0 ? 1.0 : 4.0;\n"
		"}\n"
		"vec2 getUV(vec3 d, float f) {\n"
		"  vec2 uv;\n"
		"  if (f == 0.0) uv = vec2(d.z, d.y) / abs(d.x);\n"
		"  else if (f == 1.0) uv = vec2(-d.x, -d.z) / abs(d.y);\n"
		"  else if (f == 2.0) uv = vec2(-d.x, d.y) / abs(d.z);\n"
		"  else if (f == 3.0) uv = vec2(-d.z, d.y) / abs(d.x);\n"
		"  else if (f == 4.0) uv = vec2(-d.x, d.z) / abs(d.y);\n"
		"  else uv = vec2(d.x, d.y) / abs(d.z);\n"
		"  return 0.5 * (uv + 1.0);\n"
		"}\n"
		"void main() {\n"
		"  vec3 N = normalize(vOutputDirection);\n"
		"  vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);\n"
		"  vec3 T = normalize(cross(up, N));\n"
		"  vec3 B = cross(N, T);\n"
		"  float blur = roughness * 5.0;\n"
		"  vec3 D0 = N;\n"
		"  vec3 D1 = normalize(N + T * blur);\n"
		"  vec3 D2 = normalize(N - T * blur);\n"
		"  vec3 D3 = normalize(N + B * blur);\n"
		"  vec3 D4 = normalize(N - B * blur);\n"
		"  vec3 acc = vec3(0.0);\n"
		"  { vec3 d = D0; float mi = mipInt; float face = getFace(d);\n"
		"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
		"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
		"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
		"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
		"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
		"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
		"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
		"    acc += texture(envMap, uv).rgb; }\n"
		"  { vec3 d = D1; float mi = mipInt; float face = getFace(d);\n"
		"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
		"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
		"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
		"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
		"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
		"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
		"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
		"    acc += texture(envMap, uv).rgb; }\n"
		"  { vec3 d = D2; float mi = mipInt; float face = getFace(d);\n"
		"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
		"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
		"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
		"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
		"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
		"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
		"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
		"    acc += texture(envMap, uv).rgb; }\n"
		"  { vec3 d = D3; float mi = mipInt; float face = getFace(d);\n"
		"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
		"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
		"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
		"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
		"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
		"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
		"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
		"    acc += texture(envMap, uv).rgb; }\n"
		"  { vec3 d = D4; float mi = mipInt; float face = getFace(d);\n"
		"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
		"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
		"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
		"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
		"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
		"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
		"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
		"    acc += texture(envMap, uv).rgb; }\n"
		"  pc_fragColor = vec4(acc * 0.2, 1.0);\n"
		"}\n",
		texel_w, texel_h, max_mip);
	if (n <= 0 || n >= 16384) {
		delete[] built;
		return nullptr;
	}
	return built;
}

FN(w_shader_source) {
	GLuint s = obj_id(info[0]);
	char *src = take_string(info.GetIsolate(), info[1]);
	char *pmrem_repl = maybe_replace_pmrem_fs(s, src);
	const char *p = pmrem_repl ? pmrem_repl : src;
	glShaderSource(s, 1, &p, nullptr);
	delete[] src;
	if (pmrem_repl) delete[] pmrem_repl;
}
FN(w_compile_shader) { enter_bracket(); glCompileShader(obj_id(info[0])); }
FN(w_get_shader_parameter) {
	GLuint s = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	GLint v = 0;
	glGetShaderiv(s, pname, &v);
	if (pname == GL_DELETE_STATUS || pname == GL_COMPILE_STATUS) {
		info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), v != 0));
	} else {
		info.GetReturnValue().Set(Int32::New(info.GetIsolate(), v));
	}
}
FN(w_get_shader_info_log) {
	GLuint s = obj_id(info[0]);
	GLint len = 0;
	glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> buf(len > 0 ? len : 1);
	GLsizei written = 0;
	if (len > 0) glGetShaderInfoLog(s, len, &written, buf.data());
	info.GetReturnValue().Set(
	    String::NewFromUtf8(info.GetIsolate(),
	                        buf.data(), NewStringType::kNormal,
	                        written).ToLocalChecked());
}
FN(w_get_shader_source) {
	GLuint s = obj_id(info[0]);
	GLint len = 0;
	glGetShaderiv(s, GL_SHADER_SOURCE_LENGTH, &len);
	std::vector<char> buf(len > 0 ? len : 1);
	GLsizei written = 0;
	if (len > 0) glGetShaderSource(s, len, &written, buf.data());
	info.GetReturnValue().Set(
	    String::NewFromUtf8(info.GetIsolate(),
	                        buf.data(), NewStringType::kNormal,
	                        written).ToLocalChecked());
}

// ----- Program -----
FN(w_create_program) {
	GLuint p = glCreateProgram();
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_PROGRAM, p));
}
FN(w_delete_program) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteProgram(id);
}
FN(w_is_program) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsProgram(obj_id(info[0])) == GL_TRUE));
}
FN(w_attach_shader) {
	glAttachShader(obj_id(info[0]), obj_id(info[1]));
}
FN(w_detach_shader) {
	glDetachShader(obj_id(info[0]), obj_id(info[1]));
}
FN(w_link_program) { enter_bracket(); glLinkProgram(obj_id(info[0])); }
FN(w_validate_program) { enter_bracket(); glValidateProgram(obj_id(info[0])); }
FN(w_use_program) {
	enter_bracket();
	const GLuint p = obj_id(info[0]);
	glUseProgram(p);
	if (st) st->user_snap.program = (GLint)p;
}
FN(w_get_program_parameter) {
	GLuint p = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	GLint v = 0;
	glGetProgramiv(p, pname, &v);
	if (pname == GL_DELETE_STATUS || pname == GL_LINK_STATUS ||
	    pname == GL_VALIDATE_STATUS) {
		info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), v != 0));
	} else {
		info.GetReturnValue().Set(Int32::New(info.GetIsolate(), v));
	}
}
FN(w_get_program_info_log) {
	GLuint p = obj_id(info[0]);
	GLint len = 0;
	glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> buf(len > 0 ? len : 1);
	GLsizei written = 0;
	if (len > 0) glGetProgramInfoLog(p, len, &written, buf.data());
	info.GetReturnValue().Set(
	    String::NewFromUtf8(info.GetIsolate(),
	                        buf.data(), NewStringType::kNormal,
	                        written).ToLocalChecked());
}
FN(w_get_attrib_location) {
	GLuint p = obj_id(info[0]);
	char *name = take_string(info.GetIsolate(), info[1]);
	GLint loc = glGetAttribLocation(p, name);
	delete[] name;
	info.GetReturnValue().Set(Int32::New(info.GetIsolate(), loc));
}
FN(w_get_uniform_location) {
	GLuint p = obj_id(info[0]);
	char *name = take_string(info.GetIsolate(), info[1]);
	GLint loc = glGetUniformLocation(p, name);
	delete[] name;
	if (loc < 0) {
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(
	    new_gl_obj(info.GetIsolate(), K_UNIFORM_LOCATION, 0, loc));
}
FN(w_bind_attrib_location) {
	GLuint p = obj_id(info[0]);
	GLuint idx = a_u32(info, 1);
	char *name = take_string(info.GetIsolate(), info[2]);
	glBindAttribLocation(p, idx, name);
	delete[] name;
}
FN(w_get_active_attrib) {
	enter_bracket();
	Isolate *iso = info.GetIsolate();
	GLuint p = obj_id(info[0]);
	GLuint idx = a_u32(info, 1);
	char name[256];
	GLsizei nlen = 0;
	GLint size = 0;
	GLenum type = 0;
	glGetActiveAttrib(p, idx, sizeof(name), &nlen, &size, &type, name);
	Local<Object> obj = new_gl_obj(iso, K_ACTIVE_INFO, 0);
	Local<Context> c = cur(iso);
	obj->Set(c, String::NewFromUtf8(iso, "name").ToLocalChecked(),
	         String::NewFromUtf8(iso, name, NewStringType::kNormal,
	                              nlen).ToLocalChecked()).Check();
	obj->Set(c, String::NewFromUtf8(iso, "size").ToLocalChecked(),
	         Int32::New(iso, size)).Check();
	obj->Set(c, String::NewFromUtf8(iso, "type").ToLocalChecked(),
	         Uint32::NewFromUnsigned(iso, type)).Check();
	info.GetReturnValue().Set(obj);
}
FN(w_get_active_uniform) {
	enter_bracket();
	Isolate *iso = info.GetIsolate();
	GLuint p = obj_id(info[0]);
	GLuint idx = a_u32(info, 1);
	char name[256];
	GLsizei nlen = 0;
	GLint size = 0;
	GLenum type = 0;
	glGetActiveUniform(p, idx, sizeof(name), &nlen, &size, &type, name);
	Local<Object> obj = new_gl_obj(iso, K_ACTIVE_INFO, 0);
	Local<Context> c = cur(iso);
	obj->Set(c, String::NewFromUtf8(iso, "name").ToLocalChecked(),
	         String::NewFromUtf8(iso, name, NewStringType::kNormal,
	                              nlen).ToLocalChecked()).Check();
	obj->Set(c, String::NewFromUtf8(iso, "size").ToLocalChecked(),
	         Int32::New(iso, size)).Check();
	obj->Set(c, String::NewFromUtf8(iso, "type").ToLocalChecked(),
	         Uint32::NewFromUnsigned(iso, type)).Check();
	info.GetReturnValue().Set(obj);
}

// ----- Buffer -----
FN(w_create_buffer) {
	GLuint b = 0;
	glGenBuffers(1, &b);
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_BUFFER, b));
}
FN(w_delete_buffer) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteBuffers(1, &id);
}
FN(w_is_buffer) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsBuffer(obj_id(info[0])) == GL_TRUE));
}
FN(w_bind_buffer) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLuint buf = obj_id(info[1]);
	glBindBuffer(target, buf);
	if (st && target == GL_ARRAY_BUFFER) st->user_snap.array_buffer = (GLint)buf;
}
FN(w_buffer_data) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLenum usage = a_u32(info, 2);
	if (info[1]->IsNumber()) {
		// (target, size, usage) — allocate uninitialized storage.
		const int64_t size = a_i64(info, 1);
		glBufferData(target, (GLsizeiptr)size, nullptr, usage);
		return;
	}
	size_t len = 0;
	uint8_t *p = view_bytes(info[1], &len);
	if (info[1]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[1].As<ArrayBuffer>();
		p = (uint8_t *)ab->Data();
		len = ab->ByteLength();
	}
	glBufferData(target, (GLsizeiptr)len, p, usage);
}
FN(w_buffer_sub_data) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLintptr offset = (GLintptr)a_i64(info, 1);
	size_t len = 0;
	uint8_t *p = view_bytes(info[2], &len);
	if (info[2]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[2].As<ArrayBuffer>();
		p = (uint8_t *)ab->Data();
		len = ab->ByteLength();
	}
	if (p) glBufferSubData(target, offset, (GLsizeiptr)len, p);
}

// ----- Vertex attrib -----
FN(w_enable_vertex_attrib_array) {
	enter_bracket();
	glEnableVertexAttribArray(a_u32(info, 0));
}
FN(w_disable_vertex_attrib_array) {
	enter_bracket();
	glDisableVertexAttribArray(a_u32(info, 0));
}
FN(w_vertex_attrib_pointer) {
	enter_bracket();
	glVertexAttribPointer(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                      a_bool(info, 3) ? GL_TRUE : GL_FALSE,
	                      a_i32(info, 4),
	                      (const void *)(intptr_t)a_i64(info, 5));
}
FN(w_vertex_attrib_1f) { enter_bracket(); glVertexAttrib1f(a_u32(info, 0), a_f32(info, 1)); }
FN(w_vertex_attrib_2f) { enter_bracket(); glVertexAttrib2f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2)); }
FN(w_vertex_attrib_3f) { enter_bracket(); glVertexAttrib3f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3)); }
FN(w_vertex_attrib_4f) { enter_bracket(); glVertexAttrib4f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3), a_f32(info, 4)); }

// ----- Uniform -----
FN(w_uniform_1f) { enter_bracket(); glUniform1f(uniform_loc(info[0]), a_f32(info, 1)); }
FN(w_uniform_2f) { enter_bracket(); glUniform2f(uniform_loc(info[0]), a_f32(info, 1), a_f32(info, 2)); }
FN(w_uniform_3f) { enter_bracket(); glUniform3f(uniform_loc(info[0]), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3)); }
FN(w_uniform_4f) { enter_bracket(); glUniform4f(uniform_loc(info[0]), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3), a_f32(info, 4)); }
FN(w_uniform_1i) { enter_bracket(); glUniform1i(uniform_loc(info[0]), a_i32(info, 1)); }
FN(w_uniform_2i) { enter_bracket(); glUniform2i(uniform_loc(info[0]), a_i32(info, 1), a_i32(info, 2)); }
FN(w_uniform_3i) { enter_bracket(); glUniform3i(uniform_loc(info[0]), a_i32(info, 1), a_i32(info, 2), a_i32(info, 3)); }
FN(w_uniform_4i) { enter_bracket(); glUniform4i(uniform_loc(info[0]), a_i32(info, 1), a_i32(info, 2), a_i32(info, 3), a_i32(info, 4)); }

#define UNI_FV(N) \
FN(w_uniform_##N##fv) { \
	enter_bracket(); \
	std::vector<float> tmp; \
	const float *p = nullptr; \
	size_t n = 0; \
	if (!f32_list(info.GetIsolate(), info[1], tmp, &p, &n)) return; \
	glUniform##N##fv(uniform_loc(info[0]), (GLsizei)(n / N), p); \
}
UNI_FV(1)
UNI_FV(2)
UNI_FV(3)
UNI_FV(4)
#undef UNI_FV

#define UNI_IV(N) \
FN(w_uniform_##N##iv) { \
	enter_bracket(); \
	std::vector<int32_t> tmp; \
	const int32_t *p = nullptr; \
	size_t n = 0; \
	if (!i32_list(info.GetIsolate(), info[1], tmp, &p, &n)) return; \
	glUniform##N##iv(uniform_loc(info[0]), (GLsizei)(n / N), p); \
}
UNI_IV(1)
UNI_IV(2)
UNI_IV(3)
UNI_IV(4)
#undef UNI_IV

#define UNI_MAT_FV(N) \
FN(w_uniform_matrix_##N##fv) { \
	enter_bracket(); \
	std::vector<float> tmp; \
	const float *p = nullptr; \
	size_t n = 0; \
	if (!f32_list(info.GetIsolate(), info[2], tmp, &p, &n)) return; \
	const GLsizei count = (GLsizei)(n / (N * N)); \
	glUniformMatrix##N##fv(uniform_loc(info[0]), count, \
	                        a_bool(info, 1) ? GL_TRUE : GL_FALSE, p); \
}
UNI_MAT_FV(2)
UNI_MAT_FV(3)
UNI_MAT_FV(4)
#undef UNI_MAT_FV

// ----- Texture -----
FN(w_create_texture) {
	GLuint t = 0;
	glGenTextures(1, &t);
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_TEXTURE, t));
}
FN(w_delete_texture) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteTextures(1, &id);
}
FN(w_is_texture) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsTexture(obj_id(info[0])) == GL_TRUE));
}
FN(w_bind_texture) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLuint tex = obj_id(info[1]);
	glBindTexture(target, tex);
	// Track TU0 TEXTURE_2D binding to match nx_gl_state_snap_t coverage.
	if (st && target == GL_TEXTURE_2D &&
		st->user_snap.active_tex == (GLint)GL_TEXTURE0) {
		st->user_snap.tex2d_binding = (GLint)tex;
	}
}
FN(w_active_texture) {
	enter_bracket();
	const GLenum unit = a_u32(info, 0);
	glActiveTexture(unit);
	if (st) st->user_snap.active_tex = (GLint)unit;
}
FN(w_tex_parameteri) {
	enter_bracket();
	glTexParameteri(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2));
}
FN(w_tex_parameterf) {
	enter_bracket();
	glTexParameterf(a_u32(info, 0), a_u32(info, 1), a_f32(info, 2));
}
FN(w_generate_mipmap) {
	enter_bracket();
	glGenerateMipmap(a_u32(info, 0));
}
// texImage2D — buffer-source overload only for 2.C
//   (target, level, internalformat, width, height, border, format, type, pixels)
//
// Bucket E translate (re-applies dropped fork patches; see references in
// auto-memory):
//   * WebGL1 EXT_sRGB unsized formats → ES3 sized internalformat + unsized
//     format. Three.js's WebGL1 path uses SRGB_EXT/SRGB_ALPHA_EXT (per the
//     EXT_sRGB spec, which requires internalformat==format). ES3-core does
//     not recognize 0x8C40/0x8C42 as internalformats and returns
//     GL_INVALID_ENUM — confirmed empirically via the bucket-e:texImage2D
//     diagnostic (err=0x0500 on 0x8C42 calls; 0x0000 on RGBA calls). Reapplies
//     [[reference-brewser-v1-black-texture-demos]].
//   * WebGL1 unsized internalformat + HALF_FLOAT/FLOAT type → ES3 sized
//     internalformat (RGBA→RGBA16F/32F, RGB→RGB16F/32F, RG→RG16F/32F,
//     R→R16F/32F). ES3 requires a sized internalformat for HALF_FLOAT/FLOAT
//     types — unsized RGBA + HALF_FLOAT is INVALID_OPERATION in ES3. This is
//     a precondition for Bucket F PMREM (cube→2D atlas intermediates upload
//     half-float). Reapplies [[reference-dfglut-rg16f-accept-list-fix]] as a
//     translate (the V8 engine has no accept-list to widen — the V8 analog
//     is the unsized→sized translation).
// Both translations are guarded by `internalformat == format` so we only
// touch the WebGL1-style ES2 call shape; if a caller already passes a sized
// internalformat, we leave it alone.
static inline void bucket_e_translate_tex_image(
    GLint *internalformat, GLenum *format, GLenum *type) {
	if (*internalformat == *format) {
		if (*internalformat == 0x8C42 /* SRGB_ALPHA_EXT */) {
			*internalformat = 0x8C43; /* SRGB8_ALPHA8 */
			*format         = 0x1908; /* GL_RGBA */
			return;
		}
		if (*internalformat == 0x8C40 /* SRGB_EXT */) {
			*internalformat = 0x8C41; /* SRGB8 */
			*format         = 0x1907; /* GL_RGB */
			return;
		}
		if (*type == 0x140B /* HALF_FLOAT */ ||
		    *type == 0x8D61 /* HALF_FLOAT_OES */) {
			switch (*internalformat) {
			case 0x1908: *internalformat = 0x881A; break; /* RGBA→RGBA16F */
			case 0x1907: *internalformat = 0x881B; break; /* RGB→RGB16F */
			case 0x8227: *internalformat = 0x822F; break; /* RG→RG16F */
			case 0x1903: *internalformat = 0x822D; break; /* RED→R16F */
			}
		}
		if (*type == 0x1406 /* GL_FLOAT */) {
			switch (*internalformat) {
			case 0x1908: *internalformat = 0x8814; break; /* RGBA→RGBA32F */
			case 0x1907: *internalformat = 0x8815; break; /* RGB→RGB32F */
			case 0x8227: *internalformat = 0x8230; break; /* RG→RG32F */
			case 0x1903: *internalformat = 0x822E; break; /* RED→R32F */
			}
		}
	}
	// F.1: Normalize HALF_FLOAT_OES (WebGL1 OES extension token, 0x8D61) to
	// HALF_FLOAT (ES3-core token, 0x140B) when paired with a sized half-float
	// internalformat (RGBA16F/RGB16F/RG16F/R16F). The QuickJS-era engine
	// comment that said "Mesa Nouveau treats 0x8D61 and 0x140B as aliases"
	// is true for the WebGL1 ES2-style unsized call (internalformat==format
	// unsized), but ES3 spec REQUIRES type==HALF_FLOAT for sized half-float
	// internalformats — pairing HALF_FLOAT_OES with sized internalformat
	// returns INVALID_OPERATION on Mesa-Nouveau-on-Citron (confirmed via F.1
	// `[f1:texImage2D-hf]` diagnostic on webgl-loader-gltf's r162 PMREM
	// RGBA16F intermediate uploads). Three.js r162's PMREM passes already-
	// sized RGBA16F + HALF_FLOAT_OES; this normalization makes that valid.
	if (*type == 0x8D61 /* HALF_FLOAT_OES */) {
		switch (*internalformat) {
		case 0x881A /* RGBA16F */:
		case 0x881B /* RGB16F */:
		case 0x822F /* RG16F */:
		case 0x822D /* R16F */:
			*type = 0x140B; /* HALF_FLOAT */
			break;
		}
	}
}

// texSubImage2D has no internalformat parameter — storage was allocated by a
// prior texImage2D/texStorage2D. The sub-upload's format should be unsized
// (the storage carries the sizing). The only translation needed mirrors the
// SRGB format-side normalization (in case Three.js passes SRGB_ALPHA_EXT as
// the format here too, mirroring the texImage2D pattern).
static inline void bucket_e_translate_tex_sub_image(
    GLenum *format, GLenum *type) {
	if (*format == 0x8C42 /* SRGB_ALPHA_EXT */) {
		*format = 0x1908; /* GL_RGBA */
	} else if (*format == 0x8C40 /* SRGB_EXT */) {
		*format = 0x1907; /* GL_RGB */
	}
	// F.1: Normalize HALF_FLOAT_OES → HALF_FLOAT. texSubImage2D uploads
	// against storage that was allocated by a prior texImage2D or
	// texStorage2D — if the storage was sized half-float (RGBA16F/etc.),
	// the sub-upload must use the ES3-core HALF_FLOAT token, not the
	// WebGL1 OES alias. Safe to normalize unconditionally: HALF_FLOAT is
	// the ES3-canonical token and Mesa accepts it for any half-float
	// storage shape; the unsized-WebGL1 case still works post-normalize.
	if (*type == 0x8D61 /* HALF_FLOAT_OES */) {
		*type = 0x140B; /* HALF_FLOAT */
	}
}

FN(w_tex_image_2d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLint level = a_i32(info, 1);
	GLint internalformat = a_i32(info, 2);
	const GLsizei width = a_i32(info, 3);
	const GLsizei height = a_i32(info, 4);
	const GLint border = a_i32(info, 5);
	GLenum format = a_u32(info, 6);
	GLenum type = a_u32(info, 7);
	size_t len = 0;
	void *pixels = view_bytes(info[8], &len);
	if (info[8]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[8].As<ArrayBuffer>();
		pixels = ab->Data();
		len = ab->ByteLength();
	}
	if (info[8]->IsNullOrUndefined()) pixels = nullptr;
	bucket_e_translate_tex_image(&internalformat, &format, &type);
	glTexImage2D(target, level, internalformat, width, height, border, format,
	             type, pixels);
}
FN(w_tex_sub_image_2d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLint level = a_i32(info, 1);
	const GLint xoff = a_i32(info, 2);
	const GLint yoff = a_i32(info, 3);
	const GLsizei width = a_i32(info, 4);
	const GLsizei height = a_i32(info, 5);
	GLenum format = a_u32(info, 6);
	GLenum type = a_u32(info, 7);
	size_t len = 0;
	void *pixels = view_bytes(info[8], &len);
	if (info[8]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[8].As<ArrayBuffer>();
		pixels = ab->Data();
		len = ab->ByteLength();
	}
	bucket_e_translate_tex_sub_image(&format, &type);
	glTexSubImage2D(target, level, xoff, yoff, width, height, format, type,
	                pixels);
}

// Phase 2.G.1 cut #25 (2026-07-01) — bound because cube-route-shim's
// cube-RT-readback rescue needs it to blit scratch → atlas at face
// offset (per NXJS_PATCHES_NEEDED.md #24). Never bound in QuickJS-era
// either; not load-bearing for any current v1/v2 demo OTHER than the
// runtime rescue's flush path, so this is a pure additive spec-hole
// close. Same signature as glCopyTexSubImage2D core-GLES2 — reads from
// current READ_FRAMEBUFFER (or FRAMEBUFFER on ES2) at (x,y) w×h and
// writes to the currently-bound target texture at (xoffset,yoffset).
FN(w_copy_tex_sub_image_2d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLint level = a_i32(info, 1);
	const GLint xoff = a_i32(info, 2);
	const GLint yoff = a_i32(info, 3);
	const GLint x = a_i32(info, 4);
	const GLint y = a_i32(info, 5);
	const GLsizei width = a_i32(info, 6);
	const GLsizei height = a_i32(info, 7);
	glCopyTexSubImage2D(target, level, xoff, yoff, x, y, width, height);
}

// Phase 2.G.1 cut #27 (2026-07-01) — cube-route-shim needs blitFramebuffer
// to Y-flip the scratch → atlas copy in one GPU-side pass (avoids CPU
// readPixels/texSubImage2D roundtrip and its per-format bpp mapping). WebGL2
// only (glBlitFramebuffer is core GLES3, not ES2). Passed directly through
// to the GLES3 entrypoint; Mesa-Nouveau supports Y-flip via reversed dst
// rectangle (dstY0 > dstY1). Signature matches WebGL2 spec 1:1.
FN(w_blit_framebuffer) {
	enter_bracket();
	const GLint sx0 = a_i32(info, 0);
	const GLint sy0 = a_i32(info, 1);
	const GLint sx1 = a_i32(info, 2);
	const GLint sy1 = a_i32(info, 3);
	const GLint dx0 = a_i32(info, 4);
	const GLint dy0 = a_i32(info, 5);
	const GLint dx1 = a_i32(info, 6);
	const GLint dy1 = a_i32(info, 7);
	const GLbitfield mask = a_u32(info, 8);
	const GLenum filter = a_u32(info, 9);
	glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
}

// Phase 2.G.1 cut #3 — v2-only method impls. Grouped before texImage3D to
// keep all v2-only additions in one contiguous block of the file.
//
// VAO (cut #3a) — Three.js v2 uses VAOs unconditionally for every Mesh.
// Four spec methods: create / delete / is / bind. Direct GLES3 passthroughs;
// the K_VERTEX_ARRAY_OBJECT handle kind (declared above K_COUNT) carries
// the GL name through the JS object via the existing GLObj wrapper.
FN(w_create_vertex_array) {
	GLuint v = 0;
	glGenVertexArrays(1, &v);
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(),
	                                      K_VERTEX_ARRAY_OBJECT, v));
}
FN(w_delete_vertex_array) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteVertexArrays(1, &id);
}
FN(w_is_vertex_array) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsVertexArray(obj_id(info[0])) == GL_TRUE));
}
FN(w_bind_vertex_array) {
	enter_bracket();
	const GLuint v = obj_id(info[0]);
	glBindVertexArray(v);
	if (st) st->user_snap.vao = (GLint)v;
}

// UBO core (cut #3b) — minimum surface for Three.js's UBO setup: index
// query + binding-point assignment + buffer-to-binding bind. The webgl2-ubo
// demo calls this pair after THREE.UniformsGroup creation:
//   const idx = gl.getUniformBlockIndex(program, 'ViewData');
//   gl.uniformBlockBinding(program, idx, 0);
//   gl.bindBufferBase(GL_UNIFORM_BUFFER, 0, viewDataBuffer);
// `bindBufferRange` is the offset+size variant; bound in case Three.js's
// uniformsGroup uses it for sub-region UBO uploads (it doesn't by default
// in r184 but cheap to ship together).
FN(w_bind_buffer_base) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLuint index = a_u32(info, 1);
	const GLuint buffer = obj_id(info[2]);
	glBindBufferBase(target, index, buffer);
}
FN(w_bind_buffer_range) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLuint index = a_u32(info, 1);
	const GLuint buffer = obj_id(info[2]);
	const GLintptr offset = (GLintptr)a_i64(info, 3);
	const GLsizeiptr size = (GLsizeiptr)a_i64(info, 4);
	glBindBufferRange(target, index, buffer, offset, size);
}
FN(w_get_uniform_block_index) {
	Isolate *iso = info.GetIsolate();
	const GLuint prog = obj_id(info[0]);
	String::Utf8Value name(iso, info[1]);
	const char *cn = *name ? *name : "";
	GLuint idx = glGetUniformBlockIndex(prog, cn);
	info.GetReturnValue().Set(Uint32::NewFromUnsigned(iso, idx));
}
FN(w_uniform_block_binding) {
	enter_bracket();
	const GLuint prog = obj_id(info[0]);
	const GLuint blockIndex = a_u32(info, 1);
	const GLuint blockBinding = a_u32(info, 2);
	glUniformBlockBinding(prog, blockIndex, blockBinding);
}

// texStorage2D (cut #3c) — Three.js v2 prefers immutable texture storage;
// `texStorage2D` allocates ALL mip levels at once and freezes the dimensions.
// Subsequent uploads must go through texSubImage2D. Five-arg passthrough.
FN(w_tex_storage_2d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLsizei levels = a_i32(info, 1);
	const GLenum internalformat = a_u32(info, 2);
	const GLsizei width = a_i32(info, 3);
	const GLsizei height = a_i32(info, 4);
	glTexStorage2D(target, levels, internalformat, width, height);
}

// Instanced drawing (cut #4) — Three.js's InstancedMesh path uses
// drawElementsInstanced + vertexAttribDivisor for per-instance attribute
// stepping. drawArraysInstanced is the unindexed sibling, shipped together.
// All three are direct GLES3 passthroughs. Both draw impls use touch_fbo()
// (defined above) to mark the bridge FBO dirty when drawing into the default
// target — matches the v1 w_draw_arrays / w_draw_elements pattern.
FN(w_draw_arrays_instanced) {
	enter_bracket();
	glDrawArraysInstanced(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                      a_i32(info, 3));
	touch_fbo();
}
FN(w_draw_elements_instanced) {
	enter_bracket();
	glDrawElementsInstanced(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                        (const void *)(intptr_t)a_i64(info, 3),
	                        a_i32(info, 4));
	touch_fbo();
}
FN(w_vertex_attrib_divisor) {
	enter_bracket();
	glVertexAttribDivisor(a_u32(info, 0), a_u32(info, 1));
}

// drawBuffers (cut #5) — Three.js v2's WebGLState calls
// `gl.drawBuffers([gl.BACK])` on the very first render's bindFramebuffer
// transition (defaultDrawbuffers starts [] so the BACK-set is unconditional
// on the first frame; subsequent frames are gated by needsUpdate). Without
// this method, calling it through the demo's Proxy resolves to `undefined`
// → TypeError "drawBuffers is not a function" → which Three.js's `state`
// function does NOT wrap in try/catch (unlike texStorage2D/texImage2D/etc.
// which ARE wrapped), so the throw propagates up out of renderer.render()
// and aborts the render path; subsequent frames continue ticking the
// animate loop (animation tween + fps counter), masking it. Symptom:
// canvas shows the clear color only (the gl.clear that runs BEFORE
// state.drawBuffers in the frame), no geometry rasterizes, no JS error
// surfaces because the Three.js try/catch upstream isn't on this path.
// GLES3 signature: glDrawBuffers(GLsizei n, const GLenum *bufs).
FN(w_draw_buffers) {
	enter_bracket();
	std::vector<int32_t> tmp;
	const int32_t *p = nullptr;
	size_t n = 0;
	if (!i32_list(info.GetIsolate(), info[0], tmp, &p, &n)) return;
	glDrawBuffers((GLsizei)n, (const GLenum *)p);
}

// Phase 2.G.1 cut #2 — texImage3D for WebGL2. Three.js's v2 path calls this
// at WebGLRenderer init time to create 1×1 placeholder textures for the
// default-bound TEXTURE_3D / TEXTURE_2D_ARRAY samplers (the v2 analog of v1's
// _emptyCubeTexture). Without this binding the demo throws before reaching
// its scene setup. Signature: (target, level, internalformat, width, height,
// depth, border, format, type, pixels). Mirrors w_tex_image_2d but with
// `depth` between height and border. No bucket-E format widening yet — the
// Three.js call site uses canonical RGBA+UByte which Mesa Nouveau accepts
// directly; format-widening for v2 3D-texture uploads is a future cut if/
// when a demo surfaces a probe-rejected format.
FN(w_tex_image_3d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLint level = a_i32(info, 1);
	GLint internalformat = a_i32(info, 2);
	const GLsizei width = a_i32(info, 3);
	const GLsizei height = a_i32(info, 4);
	const GLsizei depth = a_i32(info, 5);
	const GLint border = a_i32(info, 6);
	GLenum format = a_u32(info, 7);
	GLenum type = a_u32(info, 8);
	size_t len = 0;
	void *pixels = view_bytes(info[9], &len);
	if (info[9]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[9].As<ArrayBuffer>();
		pixels = ab->Data();
		len = ab->ByteLength();
	}
	if (info[9]->IsNullOrUndefined()) pixels = nullptr;
	glTexImage3D(target, level, internalformat, width, height, depth, border,
	             format, type, pixels);
}

// Phase 2.G.1 cut #32 (2026-07-01) — bound because Three.js r184's WebGL2
// backend unconditionally calls state.texStorage3D + state.texSubImage3D
// for DataArrayTexture/Data3DTexture uploads (WebGLTextures.js:1174/1190/
// 1198). Both wrappers try/catch and silently swallow "gl.texStorage3D is
// not a function" errors, so without these bindings the array-texture
// storage is never allocated → sampler2DArray/sampler3D reads return
// vec4(0) → webgl2-texture2darray renders black on both Citron and
// hardware. Direct passthrough — no format massaging required (GLES3
// spec matches WebGL2 spec 1:1 for these entry points).
FN(w_tex_storage_3d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLsizei levels = a_i32(info, 1);
	const GLenum internalformat = a_u32(info, 2);
	const GLsizei width = a_i32(info, 3);
	const GLsizei height = a_i32(info, 4);
	const GLsizei depth = a_i32(info, 5);
	glTexStorage3D(target, levels, internalformat, width, height, depth);
}

FN(w_tex_sub_image_3d) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	const GLint level = a_i32(info, 1);
	const GLint xoff = a_i32(info, 2);
	const GLint yoff = a_i32(info, 3);
	const GLint zoff = a_i32(info, 4);
	const GLsizei width = a_i32(info, 5);
	const GLsizei height = a_i32(info, 6);
	const GLsizei depth = a_i32(info, 7);
	const GLenum format = a_u32(info, 8);
	const GLenum type = a_u32(info, 9);
	size_t len = 0;
	void *pixels = view_bytes(info[10], &len);
	if (info[10]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[10].As<ArrayBuffer>();
		pixels = ab->Data();
		len = ab->ByteLength();
	}
	if (info[10]->IsNullOrUndefined()) pixels = nullptr;
	glTexSubImage3D(target, level, xoff, yoff, zoff, width, height, depth,
	                format, type, pixels);
}

// ----- Framebuffer / Renderbuffer -----
FN(w_create_framebuffer) {
	GLuint f = 0;
	glGenFramebuffers(1, &f);
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_FRAMEBUFFER, f));
}
FN(w_delete_framebuffer) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteFramebuffers(1, &id);
}
FN(w_is_framebuffer) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsFramebuffer(obj_id(info[0])) == GL_TRUE));
}
FN(w_bind_framebuffer) {
	enter_bracket();
	const GLenum target = a_u32(info, 0);
	GLuint fbo = obj_id(info[1]);
	if (st) {
		st->bound_fbo_js = fbo;
		st->draw_into_default = (fbo == 0);
	}
	// JS sees null/0 as "default" framebuffer; we redirect to tenant FBO.
	GLuint actual = (fbo == 0) ? nx_webgl_bridge_fbo_id() : fbo;
	glBindFramebuffer(target, actual);
	if (st) {
		// GL_FRAMEBUFFER binds both draw and read; the other two target
		// only one endpoint.
		if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
			st->user_snap.fbo = (GLint)actual;
		}
		if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) {
			st->user_snap.read_fbo = (GLint)actual;
		}
	}
}
FN(w_framebuffer_texture_2d) {
	enter_bracket();
	glFramebufferTexture2D(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                       obj_id(info[3]), a_i32(info, 4));
}
FN(w_framebuffer_renderbuffer) {
	enter_bracket();
	glFramebufferRenderbuffer(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                          obj_id(info[3]));
}
FN(w_check_framebuffer_status) {
	enter_bracket();
	info.GetReturnValue().Set(Uint32::NewFromUnsigned(info.GetIsolate(),
	    glCheckFramebufferStatus(a_u32(info, 0))));
}
FN(w_create_renderbuffer) {
	GLuint r = 0;
	glGenRenderbuffers(1, &r);
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_RENDERBUFFER, r));
}
FN(w_delete_renderbuffer) {
	GLuint id = obj_id(info[0]);
	if (id) glDeleteRenderbuffers(1, &id);
}
FN(w_is_renderbuffer) {
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(),
	    glIsRenderbuffer(obj_id(info[0])) == GL_TRUE));
}
FN(w_bind_renderbuffer) {
	enter_bracket();
	glBindRenderbuffer(a_u32(info, 0), obj_id(info[1]));
}
FN(w_renderbuffer_storage) {
	enter_bracket();
	glRenderbufferStorage(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2),
	                      a_i32(info, 3));
}

// ----- Draw -----
FN(w_draw_arrays) {
	enter_bracket();
	glDrawArrays(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2));
	touch_fbo();
}
FN(w_draw_elements) {
	enter_bracket();
	glDrawElements(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	               (const void *)(intptr_t)a_i64(info, 3));
	touch_fbo();
}

// ----- Readback -----
FN(w_read_pixels) {
	enter_bracket();
	const GLint x = a_i32(info, 0);
	const GLint y = a_i32(info, 1);
	const GLsizei width = a_i32(info, 2);
	const GLsizei height = a_i32(info, 3);
	const GLenum format = a_u32(info, 4);
	const GLenum type = a_u32(info, 5);
	size_t len = 0;
	void *pixels = view_bytes(info[6], &len);
	if (pixels) glReadPixels(x, y, width, height, format, type, pixels);
}

// ----- Fork-specific hooks required by brewser-runtime canvas-runner -----
FN(w_enable_gpu_bridge_prototype) {
	// Canvas-runner refuses to use the context if this method is absent
	// (see brewser-runtime canvas-runner.ts:278). The QuickJS-era fork's
	// bridge had this as a runtime toggle; the V8-migration bridge is always
	// on (the only path that exists), so this is a no-op that returns true.
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), true));
}
FN(w_set_bridge_auto_flush) {
	// brewser-runtime canvas-runner.ts calls gl.setBridgeAutoFlush(false) to
	// tell the engine "I drive my own per-canvas paint via copyBridgeToCanvas
	// at each <canvas>'s CSS layout slot — do NOT auto-stomp the whole FBO
	// onto the screen on top of that". Forward the bool to the bridge;
	// default-true (the no-arg / non-bool call) preserves the 2.B test-FBO
	// smoke path that never calls this.
	Isolate *iso = info.GetIsolate();
	bool v = true;
	if (info.Length() >= 1)
		v = info[0]->BooleanValue(iso);
	nx_webgl_bridge_set_auto_flush(v);
	info.GetReturnValue().Set(Boolean::New(iso, true));
}

FN(w_copy_bridge_to_canvas) {
	// gl.copyBridgeToCanvas(srcX, srcY, srcW, srcH, dst_canvas, dstX, dstY)
	//   → bool ok
	//
	// Engine-side half of the runtime's per-canvas paint path. The runtime
	// (brewser-runtime canvas-runner.ts → overlayLiveAnimatedCanvases)
	// computes each inline <canvas>'s CSS layout slot and calls this to blit
	// that sub-rect of the shared bridge FBO onto the destination canvas
	// surface at the slot's screen-coord position. Pair with
	// setBridgeAutoFlush(false) so the engine's whole-FBO compose path stays
	// out of the way.
	//
	// Phase 2.D scope: dst_canvas is ignored — the only destination the
	// runtime passes is screen (nxScreen()), and the bridge composes onto
	// the persistent canvas surface from nx_skia_gpu_canvas_surface(). When
	// Phase 2.E / 2.G needs per-OffscreenCanvas destinations, extract the
	// dst_canvas's Skia surface from info[4].
	Isolate *iso = info.GetIsolate();
	if (info.Length() < 7) {
		info.GetReturnValue().Set(Boolean::New(iso, false));
		return;
	}
	const int sx = a_i32(info, 0);
	const int sy = a_i32(info, 1);
	const int sw = a_i32(info, 2);
	const int sh = a_i32(info, 3);
	const int dx = a_i32(info, 5);
	const int dy = a_i32(info, 6);

	// Close any open per-frame WebGL bracket so Skia draws against its own
	// cached GL state, not the WebGL pass's (FBO/viewport/program/etc. saved
	// in st->snap). Same discipline nx_webgl_compose_if_active uses at
	// present time.
	if (st && st->bracket_open) exit_bracket();

	SkSurface *target = nx_skia_gpu_canvas_surface();
	if (!target) {
		info.GetReturnValue().Set(Boolean::New(iso, false));
		return;
	}
	const bool ok = nx_webgl_bridge_compose_rect(target, sx, sy, sw, sh, dx, dy);
	info.GetReturnValue().Set(Boolean::New(iso, ok));
}

// ---------------------------------------------------------------------------
// Class init: receive prototype carriers + install methods on them.
//
// Phase 2.G.0 — table-split shape DECISION: separate FUNCS[] tables for v1 and
// v2 (this file holds the v1 table; install_methods_v2 below holds the v2
// table). Rationale, against the JIT-safety lesson from NXJS_PATCHES_NEEDED.md
// #8:
//
//   1. The v1 install path is hardware-verified clean (Phase 2.C hardware
//      gate, jit=on default re-enabled by #8 fix). Leaving the v1 FUNCS[]
//      table + install loop UNCHANGED means the JS shape that JIT-tier'd up
//      successfully on Tegra is preserved byte-for-byte. A separate v2
//      table cannot disturb v1's compiled code.
//   2. The v2 install path is its OWN predictable JS shape: same install
//      loop pattern (C++ for-each over a static-storage FUNCS[] with
//      proto->Set + FunctionTemplate::New + GetFunction). The hardware
//      verification step for 2.G.0 is "does this NEW shape JIT-compile
//      clean too?" — and if it doesn't, the regression is bisectable
//      because v1 is untouched.
//   3. Shape mutation discipline. The #8 fix was about a TS-side
//      Object.defineProperty pattern that compiled into a JIT codegen
//      issue; that fix replaced ~1160 per-key calls with bulk
//      Object.defineProperties on each target. v1 and v2 both already
//      use that fixed install shape for constants (webgl-rendering-
//      context.ts:577-585 and webgl2-rendering-context.ts:1102-1110).
//      The METHOD install at install_methods() is engine-side, runs once
//      at boot, NOT TS-JIT-hot — but the same discipline applies: keep
//      shapes predictable, do not gate behavior at install time. Separate
//      tables avoid a "select v1 vs v2 subset at install time" branch
//      that could feed inconsistent function objects into V8's hidden
//      class for the prototype.
//   4. The 9 v2-only extensions (EXT_disjoint_timer_query_webgl2,
//      EXT_texture_norm16, WEBGL_clip_cull_distance, EXT_float_blend,
//      EXT_render_snorm, OES_sample_variables, OES_draw_buffers_indexed,
//      WEBGL_blend_func_extended, WEBGL_compressed_texture_etc) belong
//      in v2's table only; a shared table would have to gate them
//      at runtime, growing the binding surface.
//   5. Three.js detects v1/v2 via gl.constructor.name === 'WebGL{2}
//      RenderingContext' — independent prototype chains match the
//      runtime contract.
//
// COST: when 2.G.1+ ports the ~95 v2-only methods plus duplicates the ~95
// v1-shared methods into the v2 table, the v2 FUNCS[] will be ~190 entries.
// The duplicate v1 entries point at the SAME C++ impl functions; no code
// duplication, only Spec-entry duplication.
//
// For 2.G.0 the v2 table is EMPTY (no methods bound). The v2 prototype
// scaffolding is correct (the bulk-defineProperties constants install on
// the TS side already populates 387 v2 constants on the prototype). Method
// calls on a v2 instance throw `TypeError: X is not a function`.
// ---------------------------------------------------------------------------

static void install_methods(Isolate *iso, Local<Object> proto) {
	struct Spec { const char *name; FunctionCallback fn; };
	static const Spec FUNCS[] = {
	    {"viewport", w_viewport},
	    {"scissor", w_scissor},
	    {"enable", w_enable},
	    {"disable", w_disable},
	    {"isEnabled", w_is_enabled},
	    {"depthFunc", w_depth_func},
	    {"depthMask", w_depth_mask},
	    {"depthRange", w_depth_range},
	    {"cullFace", w_cull_face},
	    {"frontFace", w_front_face},
	    {"blendFunc", w_blend_func},
	    {"blendFuncSeparate", w_blend_func_separate},
	    {"blendEquation", w_blend_equation},
	    {"blendEquationSeparate", w_blend_equation_separate},
	    {"blendColor", w_blend_color},
	    {"colorMask", w_color_mask},
	    {"stencilFunc", w_stencil_func},
	    {"stencilFuncSeparate", w_stencil_func_separate},
	    {"stencilOp", w_stencil_op},
	    {"stencilOpSeparate", w_stencil_op_separate},
	    {"stencilMask", w_stencil_mask},
	    {"stencilMaskSeparate", w_stencil_mask_separate},
	    {"polygonOffset", w_polygon_offset},
	    {"sampleCoverage", w_sample_coverage},
	    {"lineWidth", w_line_width},
	    {"hint", w_hint},
	    {"clear", w_clear},
	    {"clearColor", w_clear_color},
	    {"clearDepth", w_clear_depth},
	    {"clearStencil", w_clear_stencil},
	    {"finish", w_finish},
	    {"flush", w_flush},
	    {"pixelStorei", w_pixel_storei},
	    {"getError", w_get_error},
	    {"getParameter", w_get_parameter},
	    {"getExtension", w_get_extension},
	    {"getSupportedExtensions", w_get_supported_extensions},
	    // Phase-0 gap fill — internal natives for the runtime's
	    // getBackendInfo shim. Leading underscore signals "shim
	    // consumers only, not a WebGL spec surface". See
	    // populate_native_extensions() rationale + phase-0 ledger entry.
	    {"_getNativeExtensionsString", w_get_native_extensions_string},
	    {"_getEglVersion", w_get_egl_version},
	    {"isContextLost", w_is_context_lost},
	    {"getContextAttributes", w_get_context_attributes},
	    {"getShaderPrecisionFormat", w_get_shader_precision_format},
	    {"createShader", w_create_shader},
	    {"deleteShader", w_delete_shader},
	    {"isShader", w_is_shader},
	    {"shaderSource", w_shader_source},
	    {"compileShader", w_compile_shader},
	    {"getShaderParameter", w_get_shader_parameter},
	    {"getShaderInfoLog", w_get_shader_info_log},
	    {"getShaderSource", w_get_shader_source},
	    {"createProgram", w_create_program},
	    {"deleteProgram", w_delete_program},
	    {"isProgram", w_is_program},
	    {"attachShader", w_attach_shader},
	    {"detachShader", w_detach_shader},
	    {"linkProgram", w_link_program},
	    {"validateProgram", w_validate_program},
	    {"useProgram", w_use_program},
	    {"getProgramParameter", w_get_program_parameter},
	    {"getProgramInfoLog", w_get_program_info_log},
	    {"getAttribLocation", w_get_attrib_location},
	    {"getUniformLocation", w_get_uniform_location},
	    {"bindAttribLocation", w_bind_attrib_location},
	    {"getActiveAttrib", w_get_active_attrib},
	    {"getActiveUniform", w_get_active_uniform},
	    {"createBuffer", w_create_buffer},
	    {"deleteBuffer", w_delete_buffer},
	    {"isBuffer", w_is_buffer},
	    {"bindBuffer", w_bind_buffer},
	    {"bufferData", w_buffer_data},
	    {"bufferSubData", w_buffer_sub_data},
	    {"enableVertexAttribArray", w_enable_vertex_attrib_array},
	    {"disableVertexAttribArray", w_disable_vertex_attrib_array},
	    {"vertexAttribPointer", w_vertex_attrib_pointer},
	    {"vertexAttrib1f", w_vertex_attrib_1f},
	    {"vertexAttrib2f", w_vertex_attrib_2f},
	    {"vertexAttrib3f", w_vertex_attrib_3f},
	    {"vertexAttrib4f", w_vertex_attrib_4f},
	    {"uniform1f", w_uniform_1f},
	    {"uniform2f", w_uniform_2f},
	    {"uniform3f", w_uniform_3f},
	    {"uniform4f", w_uniform_4f},
	    {"uniform1i", w_uniform_1i},
	    {"uniform2i", w_uniform_2i},
	    {"uniform3i", w_uniform_3i},
	    {"uniform4i", w_uniform_4i},
	    {"uniform1fv", w_uniform_1fv},
	    {"uniform2fv", w_uniform_2fv},
	    {"uniform3fv", w_uniform_3fv},
	    {"uniform4fv", w_uniform_4fv},
	    {"uniform1iv", w_uniform_1iv},
	    {"uniform2iv", w_uniform_2iv},
	    {"uniform3iv", w_uniform_3iv},
	    {"uniform4iv", w_uniform_4iv},
	    {"uniformMatrix2fv", w_uniform_matrix_2fv},
	    {"uniformMatrix3fv", w_uniform_matrix_3fv},
	    {"uniformMatrix4fv", w_uniform_matrix_4fv},
	    {"createTexture", w_create_texture},
	    {"deleteTexture", w_delete_texture},
	    {"isTexture", w_is_texture},
	    {"bindTexture", w_bind_texture},
	    {"activeTexture", w_active_texture},
	    {"texParameteri", w_tex_parameteri},
	    {"texParameterf", w_tex_parameterf},
	    {"generateMipmap", w_generate_mipmap},
	    {"texImage2D", w_tex_image_2d},
	    {"texSubImage2D", w_tex_sub_image_2d},
	    {"copyTexSubImage2D", w_copy_tex_sub_image_2d},
	    {"createFramebuffer", w_create_framebuffer},
	    {"deleteFramebuffer", w_delete_framebuffer},
	    {"isFramebuffer", w_is_framebuffer},
	    {"bindFramebuffer", w_bind_framebuffer},
	    {"framebufferTexture2D", w_framebuffer_texture_2d},
	    {"framebufferRenderbuffer", w_framebuffer_renderbuffer},
	    {"checkFramebufferStatus", w_check_framebuffer_status},
	    {"createRenderbuffer", w_create_renderbuffer},
	    {"deleteRenderbuffer", w_delete_renderbuffer},
	    {"isRenderbuffer", w_is_renderbuffer},
	    {"bindRenderbuffer", w_bind_renderbuffer},
	    {"renderbufferStorage", w_renderbuffer_storage},
	    {"drawArrays", w_draw_arrays},
	    {"drawElements", w_draw_elements},
	    {"readPixels", w_read_pixels},
	    // Fork-specific hooks (canvas-runner expects them).
	    {"enableGpuBridgePrototype", w_enable_gpu_bridge_prototype},
	    {"setBridgeAutoFlush", w_set_bridge_auto_flush},
	    {"copyBridgeToCanvas", w_copy_bridge_to_canvas},
	};
	Local<Context> ctx = iso->GetCurrentContext();
	for (const auto &s : FUNCS) {
		proto->Set(ctx, nx_str(iso, s.name),
		           FunctionTemplate::New(iso, s.fn)
		               ->GetFunction(ctx).ToLocalChecked()).Check();
	}
}

// Phase 2.G.1 cut #1 — v2 method table = v1 95-method base (verbatim copy
// of install_methods's FUNCS[]). All w_* impl functions operate on the
// process-wide WebGLState `st` which is identical between v1 and v2 (one
// bridge, one tenant FBO, one shared ES3 context per Phase 2.A), so binding
// the same impl functions on the v2 prototype is semantically correct —
// each call goes through the same lazy bracket enter / Skia composite
// exit machinery a v1 call would. Entries duplicate the Spec rows; the
// underlying C++ functions are SHARED (no impl duplication).
//
// 2.G.1 cut #2+ will add v2-only methods (VAO, UBO, sync, queries,
// texStorage2D, ...) at the bottom of this table as the webgl2-ubo slice's
// diag-proxy reports them. The intentional code duplication of the Spec
// rows is the table-split-shape discipline from NXJS_PATCHES_NEEDED.md #15
// — DO NOT refactor to dedup; refactoring re-introduces the JIT-safety
// risk per the rationale block above install_methods().
static void install_methods_v2(Isolate *iso, Local<Object> proto) {
	struct Spec { const char *name; FunctionCallback fn; };
	static const Spec FUNCS[] = {
	    {"viewport", w_viewport},
	    {"scissor", w_scissor},
	    {"enable", w_enable},
	    {"disable", w_disable},
	    {"isEnabled", w_is_enabled},
	    {"depthFunc", w_depth_func},
	    {"depthMask", w_depth_mask},
	    {"depthRange", w_depth_range},
	    {"cullFace", w_cull_face},
	    {"frontFace", w_front_face},
	    {"blendFunc", w_blend_func},
	    {"blendFuncSeparate", w_blend_func_separate},
	    {"blendEquation", w_blend_equation},
	    {"blendEquationSeparate", w_blend_equation_separate},
	    {"blendColor", w_blend_color},
	    {"colorMask", w_color_mask},
	    {"stencilFunc", w_stencil_func},
	    {"stencilFuncSeparate", w_stencil_func_separate},
	    {"stencilOp", w_stencil_op},
	    {"stencilOpSeparate", w_stencil_op_separate},
	    {"stencilMask", w_stencil_mask},
	    {"stencilMaskSeparate", w_stencil_mask_separate},
	    {"polygonOffset", w_polygon_offset},
	    {"sampleCoverage", w_sample_coverage},
	    {"lineWidth", w_line_width},
	    {"hint", w_hint},
	    {"clear", w_clear},
	    {"clearColor", w_clear_color},
	    {"clearDepth", w_clear_depth},
	    {"clearStencil", w_clear_stencil},
	    {"finish", w_finish},
	    {"flush", w_flush},
	    {"pixelStorei", w_pixel_storei},
	    {"getError", w_get_error},
	    {"getParameter", w_get_parameter},
	    {"getExtension", w_get_extension},
	    {"getSupportedExtensions", w_get_supported_extensions},
	    // Phase-0 gap fill — internal natives for the runtime's
	    // getBackendInfo shim. Leading underscore signals "shim
	    // consumers only, not a WebGL spec surface". See
	    // populate_native_extensions() rationale + phase-0 ledger entry.
	    {"_getNativeExtensionsString", w_get_native_extensions_string},
	    {"_getEglVersion", w_get_egl_version},
	    {"isContextLost", w_is_context_lost},
	    {"getContextAttributes", w_get_context_attributes},
	    {"getShaderPrecisionFormat", w_get_shader_precision_format},
	    {"createShader", w_create_shader},
	    {"deleteShader", w_delete_shader},
	    {"isShader", w_is_shader},
	    {"shaderSource", w_shader_source},
	    {"compileShader", w_compile_shader},
	    {"getShaderParameter", w_get_shader_parameter},
	    {"getShaderInfoLog", w_get_shader_info_log},
	    {"getShaderSource", w_get_shader_source},
	    {"createProgram", w_create_program},
	    {"deleteProgram", w_delete_program},
	    {"isProgram", w_is_program},
	    {"attachShader", w_attach_shader},
	    {"detachShader", w_detach_shader},
	    {"linkProgram", w_link_program},
	    {"validateProgram", w_validate_program},
	    {"useProgram", w_use_program},
	    {"getProgramParameter", w_get_program_parameter},
	    {"getProgramInfoLog", w_get_program_info_log},
	    {"getAttribLocation", w_get_attrib_location},
	    {"getUniformLocation", w_get_uniform_location},
	    {"bindAttribLocation", w_bind_attrib_location},
	    {"getActiveAttrib", w_get_active_attrib},
	    {"getActiveUniform", w_get_active_uniform},
	    {"createBuffer", w_create_buffer},
	    {"deleteBuffer", w_delete_buffer},
	    {"isBuffer", w_is_buffer},
	    {"bindBuffer", w_bind_buffer},
	    {"bufferData", w_buffer_data},
	    {"bufferSubData", w_buffer_sub_data},
	    {"enableVertexAttribArray", w_enable_vertex_attrib_array},
	    {"disableVertexAttribArray", w_disable_vertex_attrib_array},
	    {"vertexAttribPointer", w_vertex_attrib_pointer},
	    {"vertexAttrib1f", w_vertex_attrib_1f},
	    {"vertexAttrib2f", w_vertex_attrib_2f},
	    {"vertexAttrib3f", w_vertex_attrib_3f},
	    {"vertexAttrib4f", w_vertex_attrib_4f},
	    {"uniform1f", w_uniform_1f},
	    {"uniform2f", w_uniform_2f},
	    {"uniform3f", w_uniform_3f},
	    {"uniform4f", w_uniform_4f},
	    {"uniform1i", w_uniform_1i},
	    {"uniform2i", w_uniform_2i},
	    {"uniform3i", w_uniform_3i},
	    {"uniform4i", w_uniform_4i},
	    {"uniform1fv", w_uniform_1fv},
	    {"uniform2fv", w_uniform_2fv},
	    {"uniform3fv", w_uniform_3fv},
	    {"uniform4fv", w_uniform_4fv},
	    {"uniform1iv", w_uniform_1iv},
	    {"uniform2iv", w_uniform_2iv},
	    {"uniform3iv", w_uniform_3iv},
	    {"uniform4iv", w_uniform_4iv},
	    {"uniformMatrix2fv", w_uniform_matrix_2fv},
	    {"uniformMatrix3fv", w_uniform_matrix_3fv},
	    {"uniformMatrix4fv", w_uniform_matrix_4fv},
	    {"createTexture", w_create_texture},
	    {"deleteTexture", w_delete_texture},
	    {"isTexture", w_is_texture},
	    {"bindTexture", w_bind_texture},
	    {"activeTexture", w_active_texture},
	    {"texParameteri", w_tex_parameteri},
	    {"texParameterf", w_tex_parameterf},
	    {"generateMipmap", w_generate_mipmap},
	    {"texImage2D", w_tex_image_2d},
	    {"texSubImage2D", w_tex_sub_image_2d},
	    {"copyTexSubImage2D", w_copy_tex_sub_image_2d},
	    {"createFramebuffer", w_create_framebuffer},
	    {"deleteFramebuffer", w_delete_framebuffer},
	    {"isFramebuffer", w_is_framebuffer},
	    {"bindFramebuffer", w_bind_framebuffer},
	    {"framebufferTexture2D", w_framebuffer_texture_2d},
	    {"framebufferRenderbuffer", w_framebuffer_renderbuffer},
	    {"checkFramebufferStatus", w_check_framebuffer_status},
	    {"createRenderbuffer", w_create_renderbuffer},
	    {"deleteRenderbuffer", w_delete_renderbuffer},
	    {"isRenderbuffer", w_is_renderbuffer},
	    {"bindRenderbuffer", w_bind_renderbuffer},
	    {"renderbufferStorage", w_renderbuffer_storage},
	    {"drawArrays", w_draw_arrays},
	    {"drawElements", w_draw_elements},
	    {"readPixels", w_read_pixels},
	    // Fork-specific hooks (canvas-runner expects them).
	    {"enableGpuBridgePrototype", w_enable_gpu_bridge_prototype},
	    {"setBridgeAutoFlush", w_set_bridge_auto_flush},
	    {"copyBridgeToCanvas", w_copy_bridge_to_canvas},

	    // ---- v2-only adds (Phase 2.G.1 cut #2+) ----
	    // cut #2 (2026-06-30) — texImage3D: Three.js v2 WebGLRenderer init
	    // creates 1×1 placeholder textures for default-bound TEXTURE_3D /
	    // TEXTURE_2D_ARRAY samplers (v2 analog of v1's _emptyCubeTexture).
	    {"texImage3D", w_tex_image_3d},
	    // cut #32 (2026-07-01) — Three.js r184 WebGL2 uses these for every
	    // DataArrayTexture/Data3DTexture upload; without them, sampler2DArray
	    // reads return vec4(0) → webgl2-texture2darray renders black.
	    {"texStorage3D", w_tex_storage_3d},
	    {"texSubImage3D", w_tex_sub_image_3d},

	    // cut #3 (2026-06-30) — VAO + UBO core + texStorage2D. Three.js v2
	    // uses VAOs unconditionally for every Mesh, and the webgl2-ubo demo
	    // exercises the UBO surface directly. texStorage2D is Three.js v2's
	    // preferred immutable-allocation path for textures.
	    {"createVertexArray", w_create_vertex_array},
	    {"deleteVertexArray", w_delete_vertex_array},
	    {"isVertexArray", w_is_vertex_array},
	    {"bindVertexArray", w_bind_vertex_array},
	    {"bindBufferBase", w_bind_buffer_base},
	    {"bindBufferRange", w_bind_buffer_range},
	    {"getUniformBlockIndex", w_get_uniform_block_index},
	    {"uniformBlockBinding", w_uniform_block_binding},
	    {"texStorage2D", w_tex_storage_2d},

	    // cut #27 (2026-07-01) — blitFramebuffer for cube-route-shim's
	    // Y-flipped scratch → atlas copy in the cube-RT-readback rescue.
	    // WebGL2-only (glBlitFramebuffer is core GLES3, no ES2 equivalent).
	    {"blitFramebuffer", w_blit_framebuffer},

	    // cut #4 (2026-06-30) — instanced-drawing trio. Three.js's
	    // InstancedMesh path uses drawElementsInstanced + vertexAttribDivisor
	    // for per-instance attribute stepping; drawArraysInstanced is the
	    // unindexed sibling.
	    {"drawArraysInstanced", w_draw_arrays_instanced},
	    {"drawElementsInstanced", w_draw_elements_instanced},
	    {"vertexAttribDivisor", w_vertex_attrib_divisor},

	    // cut #5 (2026-06-30) — drawBuffers. Three.js v2 WebGLState calls
	    // gl.drawBuffers([gl.BACK]) on first-render bindFramebuffer transition
	    // (un-try/catch'd in the upstream state.drawBuffers function);
	    // silently aborts the render path when undefined. instancing-dynamic
	    // surfaced this as "render fps stays 49 + clear color shows + nothing
	    // else draws + demo error stays null" — gl.clear runs FIRST in the
	    // frame so the clear color reaches the FBO; the throw from
	    // state.drawBuffers after that loses everything downstream, but
	    // Three.js's logging path (`error(...)`) was muted by the demo's
	    // console.error override, so no surfaced error. See drawBuffers impl
	    // comment above for the full chain.
	    {"drawBuffers", w_draw_buffers},
	    // Add sync / queries / etc. here as the diag-proxy reports them.
	};
	Local<Context> ctx = iso->GetCurrentContext();
	for (const auto &s : FUNCS) {
		if (!s.name || !s.fn) continue;
		proto->Set(ctx, nx_str(iso, s.name),
		           FunctionTemplate::New(iso, s.fn)
		               ->GetFunction(ctx).ToLocalChecked()).Check();
	}
}

// $.webglInitClass(WebGLRenderingContext, {WebGLBuffer, WebGLProgram, ...})
//   — receives the JS class + the helper-class map; stashes prototype
//   carriers for new_gl_obj() to use, and installs the WebGL methods on
//   the class's prototype.
void nx_webgl_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!st) st = new WebGLState();
	if (info.Length() < 1 || !info[0]->IsFunction()) return;
	Local<Function> cls = info[0].As<Function>();
	Local<Context> ctx = cur(iso);
	Local<Value> proto_v;
	if (!cls->Get(ctx, nx_str(iso, "prototype")).ToLocal(&proto_v)) return;
	if (!proto_v->IsObject()) return;
	Local<Object> proto = proto_v.As<Object>();
	install_methods(iso, proto);

	if (info.Length() < 2 || !info[1]->IsObject()) return;
	Local<Object> classes = info[1].As<Object>();
	struct KV { const char *name; ObjKind kind; };
	static const KV MAP[] = {
	    {"WebGLBuffer", K_BUFFER},
	    {"WebGLFramebuffer", K_FRAMEBUFFER},
	    {"WebGLProgram", K_PROGRAM},
	    {"WebGLRenderbuffer", K_RENDERBUFFER},
	    {"WebGLShader", K_SHADER},
	    {"WebGLTexture", K_TEXTURE},
	    {"WebGLUniformLocation", K_UNIFORM_LOCATION},
	    {"WebGLActiveInfo", K_ACTIVE_INFO},
	    {"WebGLShaderPrecisionFormat", K_SHADER_PRECISION_FORMAT},
	};
	for (const auto &kv : MAP) {
		Local<Value> jc;
		if (!classes->Get(ctx, nx_str(iso, kv.name)).ToLocal(&jc)) continue;
		if (!jc->IsFunction()) continue;
		Local<Value> p;
		if (!jc.As<Function>()->Get(ctx, nx_str(iso, "prototype")).ToLocal(&p))
			continue;
		if (!p->IsObject()) continue;
		st->protos[kv.kind].Reset(iso, p.As<Object>());
	}
}

// Phase 2.G.0 — $.webgl2InitClass(WebGL2RenderingContext, { handle map })
// receives the JS WebGL2RenderingContext class + the helper-class map;
// stashes prototype carriers (additive — v2 handle set is a superset of v1,
// shares the same K_* slots) and installs the v2 method table on the class's
// prototype. Separate symbol from nx_webgl_init_class to keep the v1 install
// path byte-identical to its hardware-verified shape (see install_methods()'s
// JIT-safety rationale block).
void nx_webgl2_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!st) st = new WebGLState();
	if (info.Length() < 1 || !info[0]->IsFunction()) return;
	Local<Function> cls = info[0].As<Function>();
	Local<Context> ctx = cur(iso);
	Local<Value> proto_v;
	if (!cls->Get(ctx, nx_str(iso, "prototype")).ToLocal(&proto_v)) return;
	if (!proto_v->IsObject()) return;
	Local<Object> proto = proto_v.As<Object>();
	install_methods_v2(iso, proto);

	// Handle class map. v2 adds v2-only handle kinds (WebGLVertexArrayObject
	// cut #3; WebGLQuery, WebGLSampler, WebGLSync, WebGLTransformFeedback
	// in future cuts as the slice's diag-proxy reports each create*).
	if (info.Length() < 2 || !info[1]->IsObject()) return;
	Local<Object> classes = info[1].As<Object>();
	struct KV { const char *name; ObjKind kind; };
	static const KV MAP[] = {
	    {"WebGLBuffer", K_BUFFER},
	    {"WebGLFramebuffer", K_FRAMEBUFFER},
	    {"WebGLProgram", K_PROGRAM},
	    {"WebGLRenderbuffer", K_RENDERBUFFER},
	    {"WebGLShader", K_SHADER},
	    {"WebGLTexture", K_TEXTURE},
	    {"WebGLUniformLocation", K_UNIFORM_LOCATION},
	    {"WebGLActiveInfo", K_ACTIVE_INFO},
	    {"WebGLShaderPrecisionFormat", K_SHADER_PRECISION_FORMAT},
	    // v2-only handle kinds (cut #3+):
	    {"WebGLVertexArrayObject", K_VERTEX_ARRAY_OBJECT},
	};
	for (const auto &kv : MAP) {
		Local<Value> jc;
		if (!classes->Get(ctx, nx_str(iso, kv.name)).ToLocal(&jc)) continue;
		if (!jc->IsFunction()) continue;
		Local<Value> p;
		if (!jc.As<Function>()->Get(ctx, nx_str(iso, "prototype")).ToLocal(&p))
			continue;
		if (!p->IsObject()) continue;
		// Additive-safe: WebGLBuffer / WebGLFramebuffer / etc. are EXPORTED
		// from webgl2-rendering-context.ts and IMPORTED by v1's
		// webgl-rendering-context.ts at line 24-46 — one class per kind,
		// shared symbol. Module evaluation order is v2 first (v1 depends on
		// it for the handle classes), v1 second. So when this runs at v2's
		// init, the slot is still empty; when v1's init runs afterward, it
		// unconditionally Reset()s the same slot to the SAME prototype
		// pointer. Net effect: identical prototype stashed regardless of
		// order. The IsEmpty() check here is a defensive no-op against
		// future re-orderings (e.g. if v1 is ever decoupled from v2's
		// handle exports and runs first).
		if (st->protos[kv.kind].IsEmpty()) {
			st->protos[kv.kind].Reset(iso, p.As<Object>());
		}
	}
	fprintf(stderr, "[webgl2] init_class ok (empty v2 method table; "
	                "shape verification only)\n");
	fflush(stderr);
}

// Internal shared helper for both v1 and v2 context factories. Returns an
// empty Local<Object>() on failure (caller sets info return to undefined ->
// TS createWebGL*Context returns null). `is_v2` tags the carrier so engine-
// side dispatchers added in 2.G.1+ can branch on context kind.
static Local<Object> make_context_carrier(Isolate *iso,
                                          const FunctionCallbackInfo<Value> &info,
                                          bool is_v2) {
	if (!st) st = new WebGLState();

	// Skia must be up — without the shared ES3 context + GrDirectContext,
	// the bridge can't init. Caller (TS) treats this as "no GL available".
	if (!nx_skia_gpu_egl_context() || !nx_skia_gpu_gr_context()) {
		fprintf(stderr, "[webgl%s] context_new refused: skia_gpu not ready\n",
		        is_v2 ? "2" : "");
		fflush(stderr);
		return Local<Object>();
	}

	// Read canvas dimensions if a canvas was passed.
	int w = st->width, h = st->height;
	if (info.Length() >= 1 && info[0]->IsObject()) {
		Local<Object> canvas = info[0].As<Object>();
		Local<Context> ctx = cur(iso);
		Local<Value> vw, vh;
		if (canvas->Get(ctx, nx_str(iso, "width")).ToLocal(&vw) &&
		    vw->IsNumber())
			w = vw->Int32Value(ctx).FromMaybe(w);
		if (canvas->Get(ctx, nx_str(iso, "height")).ToLocal(&vh) &&
		    vh->IsNumber())
			h = vh->Int32Value(ctx).FromMaybe(h);
	}
	if (w <= 0) w = 640;
	if (h <= 0) h = 360;
	st->width = w;
	st->height = h;

	// Bring up the tenant FBO lazily (if 2.B's test_fbo opt-in hasn't
	// already done so). The bridge is idempotent on init.
	if (!nx_webgl_bridge_is_initialized()) {
		if (!nx_webgl_bridge_init(w, h)) {
			fprintf(stderr,
			        "[webgl%s] context_new refused: bridge_init failed\n",
			        is_v2 ? "2" : "");
			fflush(stderr);
			return Local<Object>();
		}
	}
	nx_webgl_bridge_set_webgl_owned(true);

	// Phase-0 — populate the native GL extension cache and emit the
	// [gl-ext-dump] one-shot boot log. Bridge init above guarantees the
	// shared ES3 EGL context is current; glGetIntegerv(GL_NUM_EXTENSIONS)
	// + glGetStringi(GL_EXTENSIONS, i) is legal at this point. First
	// context creation fires the dump; subsequent creations are cheap
	// no-ops via the `s_native_exts_populated` guard.
	populate_native_extensions();

	// Mint the context carrier object. The TS factory sets its prototype to
	// WebGL{2}RenderingContext, so install_methods{,_v2} having populated the
	// prototype is what makes instance methods reachable.
	Local<Object> ctx_obj = nx::NewWrapped(iso);
	Local<Context> jctx = cur(iso);
	ctx_obj->Set(jctx, nx_str(iso, "drawingBufferWidth"),
	             Int32::New(iso, w)).Check();
	ctx_obj->Set(jctx, nx_str(iso, "drawingBufferHeight"),
	             Int32::New(iso, h)).Check();
	if (is_v2) {
		ctx_obj->Set(jctx, nx_str(iso, "__webgl2"),
		             Boolean::New(iso, true)).Check();
	}
	fprintf(stderr, "[webgl%s] context_new ok %dx%d%s\n",
	        is_v2 ? "2" : "", w, h,
	        is_v2 ? " (v2 wrapper, empty methods — 2.G.0)" : "");
	fflush(stderr);
	return ctx_obj;
}

// $.webglContextNew(canvas) — main factory. Returns a wrapped object that
// the TS side adds the WebGLRenderingContext prototype to. Returns undefined
// on failure (TS returns null from getContext).
void nx_webgl_context_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> obj = make_context_carrier(iso, info, /*is_v2=*/false);
	if (obj.IsEmpty()) return;
	info.GetReturnValue().Set(obj);
}

// Phase 2.G.0 — $.webgl2ContextNew(canvas). Separate factory symbol from v1.
// Currently shares engine state (WebGLState `st` is process-wide) with v1 —
// there is no per-context state divergence in 2.G.0. The separate symbol is
// the load-bearing structural choice: createWebGL2Context calls this,
// createWebGLContext calls nx_webgl_context_new, and the install paths via
// $.webgl{2}InitClass are wholly distinct. Returns undefined on failure.
void nx_webgl2_context_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> obj = make_context_carrier(iso, info, /*is_v2=*/true);
	if (obj.IsEmpty()) return;
	info.GetReturnValue().Set(obj);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API used by main.cc — the present-time integration.
// ---------------------------------------------------------------------------

// True only when this file owns the screen (legacy: WebGL2-screen path; in
// 2.C we render INSIDE Skia's canvas, so this stays false — the per-frame
// compose hook below is what 2.C uses, not the WebGL-owns-NWindow path).
bool nx_webgl_active(void) { return false; }
void nx_webgl_present(void) {}
void nx_webgl_exit(void) {}

// Called from main.cc's GPU canvas present branch, BEFORE nx_skia_gpu_present.
// Closes the per-frame bracket if it's open (restores Skia's GL state +
// resetContext), then asks the bridge to composite the tenant FBO into
// Skia's persistent canvas surface. The bridge's compose is dirty-gated, so
// the call is cheap on frames where WebGL didn't draw.
void nx_webgl_compose_if_active(SkSurface *target) {
	if (!st) return;
	if (st->bracket_open) exit_bracket();
	if (target) nx_webgl_bridge_compose(target);
}

void nx_init_webgl(v8::Isolate *iso, v8::Local<v8::Object> init_obj) {
	NX_SET_FUNC(init_obj, "webglContextNew", nx_webgl_context_new);
	NX_SET_FUNC(init_obj, "webglInitClass", nx_webgl_init_class);
	// Phase 2.G.0 — separate v2 factory + init pair. Empty method table
	// for 2.G.0 (shape verification only); methods land in 2.G.1.
	NX_SET_FUNC(init_obj, "webgl2ContextNew", nx_webgl2_context_new);
	NX_SET_FUNC(init_obj, "webgl2InitClass", nx_webgl2_init_class);
}
