#include "webgl_egl.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NXJS_HAS_EGL_GLES
#define NXJS_HAS_EGL_GLES 0
#endif

#if NXJS_HAS_EGL_GLES
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define NX_WEBGL_EGL_TEXTURE_CACHE_SIZE 32

// Soft limit for the per-program active-attribute mask used by the
// passthrough dispatch. Matches the implementation-wide
// NX_WEBGL_MAX_VERTEX_ATTRIBS (8) but kept self-contained here so
// webgl_egl.c doesn't have to pull in webgl.c's internal headers.
#define NX_WEBGL_MAX_VERTEX_ATTRIBS_LIMIT 16

typedef struct {
	uint32_t texture_id;
	uint32_t revision;
	int width;
	int height;
	uint32_t min_filter;
	uint32_t mag_filter;
	uint32_t wrap_s;
	uint32_t wrap_t;
	GLuint handle;
} nx_webgl_egl_texture_cache_entry_t;
#endif

struct nx_webgl_egl_s {
	bool built;
	bool available;
	int step;
	char status[192];
	double clear_color[4];
#if NXJS_HAS_EGL_GLES
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	EGLint major;
	EGLint minor;
	const char *vendor;
	const char *version;
	const char *renderer;
	void *native_window;
	bool bridge_enabled;
	bool spec_y_origin;
	int bridge_requested_width;
	int bridge_requested_height;
	int bridge_width;
	int bridge_height;
	GLuint bridge_texture;
GLuint bridge_framebuffer;
GLuint bridge_depth_renderbuffer;
GLuint bridge_color_program;
	GLuint bridge_color_vertex_shader;
	GLuint bridge_color_fragment_shader;
	GLuint bridge_texture_program;
	GLuint bridge_texture_vertex_shader;
	GLuint bridge_texture_fragment_shader;
	GLuint bridge_line_program;
	GLuint bridge_line_vertex_shader;
	GLuint bridge_line_fragment_shader;
	GLuint bridge_line_distance_buffer;
	GLuint bridge_line_color_buffer;
	GLuint bridge_triangle_color_buffer;
	// Shared by all three bridge programs (color, texture, line). Each draw
	// re-uploads to it before glDrawArrays, so cross-program reuse is safe.
	GLuint bridge_fog_depth_buffer;
	// Shared between bridge_color_program and bridge_texture_program for
	// per-vertex view-space normals (lighting). Lines don't use lighting.
	GLuint bridge_normal_buffer;
	// Shared between bridge_color_program and bridge_texture_program for
	// per-vertex view-space positions (point-light direction calc).
	GLuint bridge_view_position_buffer;
	GLint bridge_line_color_loc;
	GLint bridge_line_scale_loc;
	GLint bridge_line_dash_loc;
	GLint bridge_line_total_loc;
	GLint bridge_line_fog_enabled_loc;
	GLint bridge_line_fog_color_loc;
	GLint bridge_line_fog_near_loc;
	GLint bridge_line_fog_far_loc;
	GLint bridge_color_color_loc;
	GLint bridge_color_fog_enabled_loc;
	GLint bridge_color_fog_color_loc;
	GLint bridge_color_fog_near_loc;
	GLint bridge_color_fog_far_loc;
	GLint bridge_color_lighting_enabled_loc;
	GLint bridge_color_light_direction_loc;
	GLint bridge_color_light_color_loc;
	GLint bridge_color_ambient_light_color_loc;
	GLint bridge_color_point_light_enabled_loc;
	GLint bridge_color_point_light_position_loc;
	GLint bridge_color_point_light_color_loc;
	GLint bridge_color_point_light_distance_loc;
	GLint bridge_color_point_light_decay_loc;
	GLint bridge_color_light2_enabled_loc;
	GLint bridge_color_light_direction2_loc;
	GLint bridge_color_light_color2_loc;
	GLint bridge_color_fog_density_loc;
	GLint bridge_color_fog_exp2_enabled_loc;
	GLint bridge_texture_sampler_loc;
	GLint bridge_texture_fog_enabled_loc;
	GLint bridge_texture_fog_color_loc;
	GLint bridge_texture_fog_near_loc;
	GLint bridge_texture_fog_far_loc;
	GLint bridge_texture_lighting_enabled_loc;
	GLint bridge_texture_light_direction_loc;
	GLint bridge_texture_light_color_loc;
	GLint bridge_texture_ambient_light_color_loc;
	GLint bridge_texture_point_light_enabled_loc;
	GLint bridge_texture_point_light_position_loc;
	GLint bridge_texture_point_light_color_loc;
	GLint bridge_texture_point_light_distance_loc;
	GLint bridge_texture_point_light_decay_loc;
	GLint bridge_texture_light2_enabled_loc;
	GLint bridge_texture_light_direction2_loc;
	GLint bridge_texture_light_color2_loc;
	GLint bridge_texture_fog_density_loc;
	GLint bridge_texture_fog_exp2_enabled_loc;
	GLint bridge_texture_map_transform_loc;
	GLint bridge_texture_map_transform_enabled_loc;
	GLint bridge_texture_diffuse_loc;
	GLint bridge_texture_specular_enabled_loc;
	GLint bridge_texture_specular_loc;
	GLint bridge_texture_emissive_loc;
	GLint bridge_texture_shininess_loc;
	GLint bridge_color_specular_enabled_loc;
	GLint bridge_color_specular_loc;
	GLint bridge_color_emissive_loc;
	GLint bridge_color_shininess_loc;
	// HemisphereLight uniforms (milestone #21). Three.js's HemisphereLight
	// uploads `hemisphereLights[0].direction/skyColor/groundColor`; the
	// bridge composes `mix(ground, sky, 0.5 * dot(N, dir) + 0.5)` and adds
	// it to the ambient term (it's an ambient-class irradiance per
	// Three.js's `lights_fragment_begin`). Same applies to the texture
	// program below.
	GLint bridge_color_hemi_light_enabled_loc;
	GLint bridge_color_hemi_light_direction_loc;
	GLint bridge_color_hemi_light_sky_color_loc;
	GLint bridge_color_hemi_light_ground_color_loc;
	GLint bridge_texture_hemi_light_enabled_loc;
	GLint bridge_texture_hemi_light_direction_loc;
	GLint bridge_texture_hemi_light_sky_color_loc;
	GLint bridge_texture_hemi_light_ground_color_loc;
	// OES_standard_derivatives derivative-normals fallback (milestone #16).
	// When `lighting_enabled && !has_normals`, the dispatch sets
	// `u_useDerivativeNormals = 1` and the fragment shader computes
	// per-fragment normals via `normalize(cross(dFdx(v_viewPosition),
	// dFdy(v_viewPosition)))` instead of `normalize(v_normal)`. Closes
	// the [[bridge-flatshading-gap]] for `flatShading: true` materials
	// (Three.js's optimizer drops the `normal` attribute in that mode).
	GLint bridge_color_use_derivative_normals_loc;
	GLint bridge_texture_use_derivative_normals_loc;
	// SpotLight uniforms. Three.js uploads `spotLights[0].{position,direction,
	// color,distance,coneCos,penumbraCos,decay}`; the bridge programs sample
	// the cookie-less direct contribution via:
	//   L = normalize(position - v_viewPosition)
	//   spotEffect = smoothstep(coneCos, penumbraCos, dot(L, -direction))
	//   attenuation = spotEffect * (1.0 / pow(d, decay)) * (optional distance cutoff)
	// Both color and texture programs carry the same uniforms so opaque-mesh
	// (color path) and textured-mesh (texture path) materials light identically.
	// Cookie textures and shadow projection (spotLightMap[i], spotShadowMatrix[i])
	// are intentionally out of scope for this milestone — Three.js falls back
	// to a non-cookie spotlight when `spotLight.map = null`, which is what
	// webgl-lights-spotlight ships by default.
	GLint bridge_color_spot_light_enabled_loc;
	GLint bridge_color_spot_light_position_loc;
	GLint bridge_color_spot_light_direction_loc;
	GLint bridge_color_spot_light_color_loc;
	GLint bridge_color_spot_light_distance_loc;
	GLint bridge_color_spot_light_cone_cos_loc;
	GLint bridge_color_spot_light_penumbra_cos_loc;
	GLint bridge_color_spot_light_decay_loc;
	GLint bridge_texture_spot_light_enabled_loc;
	GLint bridge_texture_spot_light_position_loc;
	GLint bridge_texture_spot_light_direction_loc;
	GLint bridge_texture_spot_light_color_loc;
	GLint bridge_texture_spot_light_distance_loc;
	GLint bridge_texture_spot_light_cone_cos_loc;
	GLint bridge_texture_spot_light_penumbra_cos_loc;
	GLint bridge_texture_spot_light_decay_loc;
	// Temporary diagnostic: webgl.c dispatch sites write per-draw state
	// here so JS can read via gl.getBackendInfo().debugDispatchState.
	char debug_dispatch_state[512];
	bool bridge_pending_readback;
	// Set to true on each bridge clear (and on first bridge init) to signal
	// that the next textured-triangle draw is the "first textured draw of the
	// frame" — the GPU pipeline state hasn't fully validated yet, and the
	// first 1-2 primitives of that draw render with corrupted varying
	// interpolation (the entire face samples one near-white texel; previously
	// surfaced as the "one face white" bug on the Three.js r162 cube demo).
	// The textured-draw path consumes this flag by issuing a color/depth-
	// masked warmup drawArrays(0,3) ahead of the real draw — invisible, but
	// it absorbs the corrupted output. See [[bridge-first-textured-draw-lost]].
	bool bridge_pending_textured_warmup;
	// When false, suppress the automatic `flush_bridge_present` that
	// normally fires on `gl.clear` if pending=true. Clients that drive
	// readback explicitly via `gl.readPixels` (inline-canvas WebGL, the
	// canvas-runner in brewser) don't need it — the auto-
	// flush would do a redundant 1280×720 readback + write-to-screen
	// every frame, slow and visibly flashing on the first frame before
	// the page paint covers it. Defaults to true for back-compat with
	// the GpuCompositor pattern.
	bool bridge_auto_flush_enabled;
	// Tracks whether the auto-flush flag has been explicitly written by
	// either side (the initial default-on at first `set_bridge_enabled(true)`
	// counts, as does a later `setBridgeAutoFlush(...)`). Without this, a
	// re-call of `enableGpuBridgePrototype(true)` from a different page
	// can't distinguish "client opted out" from "never set" and silently
	// re-defaults to TRUE, undoing the swb runner's explicit
	// `setBridgeAutoFlush(false)`. The user-visible symptom is the
	// bridge FBO getting painted to the screen at (0,0) on every
	// `gl.clear`, which after navigating BACK from a page that called
	// `enableGpuBridgePrototype` shows up as a ghost copy of the new
	// page's WebGL canvas at the top-left.
	bool bridge_auto_flush_initialized;
	uint32_t bridge_last_draw_gl_error;
	GLuint bridge_vertex_buffer;
	uint8_t *bridge_readback;
	size_t bridge_readback_size;
	// Tessellation-fix scaffolding for the Tegra X1 TBR per-tile
	// interpolator coherency bug ([[threejs-cube-white-face]]). When
	// enabled, the bridge subdivides large screen-space triangles
	// (recursive midpoint, 4 children per level) before drawing.
	// `tessellation_scratch` is a persistent grow-only float buffer
	// reused each draw to hold the subdivided vertex stream
	// (interleaved position+UV, or position-only depending on the path).
	//
	// IMPORTANT: This workaround does NOT currently fix the bug — the
	// scaffolding works but midpoint-in-NDC subdivision produces
	// uniformly-sized sub-triangles that still trigger the rasterizer
	// artifact. See the big STATE comment above `tessellate_one_triangle`
	// in this file for the full investigation and the
	// clip-space-correct refactor it would take to actually fix.
	// Defaults to `false`; toggled via `gl.setTessellationFix(bool)`.
	bool tessellation_fix_enabled;
	float *tessellation_scratch;
	size_t tessellation_scratch_capacity_floats;
	nx_webgl_egl_texture_cache_entry_t texture_cache[NX_WEBGL_EGL_TEXTURE_CACHE_SIZE];
	// Milestone #15 probe: presence of GL_EXT_instanced_arrays in the GLES
	// extensions string + dlsym-style resolution of the three entry points
	// it adds. Filled in once at backend init (step 8 of the probe) and
	// surfaced through getBackendInfo() so a page can confirm hardware
	// support before the full instancing wiring lands. Function pointers
	// are typed `void *` here to keep the struct headers GLES2-only; the
	// dispatch path will cast through the typedef'd PFN types.
	bool ext_instanced_arrays_present;
	void *fn_vertex_attrib_divisor_ext;
	void *fn_draw_arrays_instanced_ext;
	void *fn_draw_elements_instanced_ext;
	// 2026-06-24 extension audit wave 1 — per-extension presence flags
	// probed from gl_extensions string at step 8 + entry-point loaders
	// resolved alongside. Used by webgl.c::nx_webgl_get_supported_extensions
	// to advertise only what the driver actually exposes, and by
	// nx_webgl_get_extension to gate the constants/methods stubs.
	bool has_anisotropic;            // GL_EXT_texture_filter_anisotropic
	bool has_clip_control;           // GL_EXT_clip_control
	bool has_depth_clamp;            // GL_EXT_depth_clamp
	bool has_polygon_offset_clamp;   // GL_EXT_polygon_offset_clamp
	bool has_texture_compression_bptc;
	bool has_texture_compression_rgtc;
	bool has_texture_compression_s3tc;     // any of EXT/DXT1/ANGLE-DXT3/5
	bool has_texture_compression_s3tc_srgb;
	bool has_texture_norm16;
	bool has_clip_cull_distance;     // GL_EXT_clip_cull_distance
	bool has_float_blend;            // GL_EXT_float_blend
	bool has_render_snorm;           // GL_EXT_render_snorm
	bool has_sample_variables;       // GL_OES_sample_variables
	bool has_shader_multisample_interpolation; // GL_OES_shader_multisample_interpolation
	bool has_parallel_shader_compile;          // GL_KHR_parallel_shader_compile
	bool has_multi_draw;             // GL_EXT_multi_draw_arrays
	bool has_draw_buffers_indexed;   // GL_OES/EXT_draw_buffers_indexed
	bool has_blend_func_extended;    // GL_EXT_blend_func_extended
	// Wave 2 compressed-texture extensions — driven by the same
	// compressedTexImage2D dispatch wired in wave 1.
	bool has_texture_compression_etc1; // GL_OES_compressed_ETC1_RGB8_texture
	bool has_texture_compression_etc;  // ES3 core (ETC2/EAC) — gated on webgl2_present
	bool has_texture_compression_astc; // GL_KHR_texture_compression_astc_ldr
	bool has_disjoint_timer_query;     // GL_EXT_disjoint_timer_query
	void *fn_query_counter_ext;        // glQueryCounter / glQueryCounterEXT
	// Wave 3 — v1 audit residuals. All effectively present on any GLES 3+
	// driver (these features were promoted to ES3 core), but we prefer the
	// explicit token when the driver advertises it.
	bool has_blend_minmax;        // GL_EXT_blend_minmax (or ES3 core)
	bool has_frag_depth;          // GL_EXT_frag_depth (or ES3 core gl_FragDepth)
	bool has_element_index_uint;  // GL_OES_element_index_uint (or ES3 core)
	bool has_fbo_render_mipmap;   // GL_OES_fbo_render_mipmap (or ES3 core)
	bool has_srgb;                // GL_EXT_sRGB (or ES3 core SRGB formats)
	bool has_ext_color_buffer_float; // GL_EXT_color_buffer_float
	// Entry-point loaders for the new extensions.
	void *fn_clip_control;                    // glClipControl[EXT]
	void *fn_polygon_offset_clamp_ext;        // glPolygonOffsetClampEXT
	void *fn_max_shader_compiler_threads_khr; // glMaxShaderCompilerThreadsKHR
	void *fn_multi_draw_arrays_ext;           // glMultiDrawArraysEXT
	void *fn_multi_draw_elements_ext;         // glMultiDrawElementsEXT
	void *fn_multi_draw_arrays_instanced_base_instance; // EXT variant; may be NULL
	void *fn_multi_draw_elements_instanced_base_vertex_base_instance; // EXT; may be NULL
	void *fn_enablei_ext;                     // glEnableiEXT
	void *fn_disablei_ext;                    // glDisableiEXT
	void *fn_blend_equationi_ext;             // glBlendEquationiEXT
	void *fn_blend_equation_separatei_ext;    // glBlendEquationSeparateiEXT
	void *fn_blend_funci_ext;                 // glBlendFunciEXT
	void *fn_blend_func_separatei_ext;        // glBlendFuncSeparateiEXT
	void *fn_bind_frag_data_location_ext;     // glBindFragDataLocationEXT
	void *fn_bind_frag_data_location_indexed_ext; // glBindFragDataLocationIndexedEXT
	void *fn_get_frag_data_index_ext;         // glGetFragDataIndexEXT
	void *fn_compressed_tex_image_2d;         // core glCompressedTexImage2D (for s3tc/bptc/rgtc wiring)
	void *fn_compressed_tex_sub_image_2d;     // core glCompressedTexSubImage2D
	// MSAA-config presence: true if eglChooseConfig granted a config with
	// EGL_SAMPLE_BUFFERS=1 + EGL_SAMPLES>=4. Surfaced via gl.getContextAttributes().antialias.
	bool egl_msaa_enabled;
	// Mesa-on-Citron gives us a GLES 3.2 context (even when we request ES2),
	// and ES 3.0+ requires an explicit VAO to be bound — the default VAO
	// (object 0) is reserved for ES 2.x and is not valid in ES 3 core.
	// Without a bound VAO, all vertex attribute state (incl. divisor) is
	// silently ignored. We create one VAO at init and bind it before every
	// passthrough draw. Resolved via eglGetProcAddress alongside the
	// instancing entry points; NULL on drivers that don't support VAOs.
	void *fn_gen_vertex_arrays;
	void *fn_bind_vertex_array;
	void *fn_delete_vertex_arrays;
	uint32_t passthrough_vao;
	// Full glGetString(GL_EXTENSIONS) string, surfaced via
	// gl.getBackendInfo().glExtensions so probe pages (com.natureglass.webglreport)
	// can audit which GL_* tokens the driver actually advertises. Mesa
	// Nouveau on Tegra reports ~80-150 tokens averaging ~30 chars each
	// (4-6 KB typical). 16 KB gives ~3x headroom; truncation would
	// silently hide tokens from the audit.
	char gl_extensions[16384];
	// User-supplied FBO currently bound via gl.bindFramebuffer(GL_FRAMEBUFFER,
	// fb). 0 means "no user FBO bound" — bridge dispatch falls back to its
	// own bridge_framebuffer + canvas-y top-down viewport convention +
	// readback-on-present path. Non-zero means subsequent bridge dispatch
	// draws into the user's FBO directly (skipping bridge_framebuffer + the
	// readback flag) using STANDARD GL bottom-up viewport / scissor coords
	// — Three.js's WebGLRenderer + EffectComposer flow uses GL-native
	// conventions inside FBOs and the next pass samples those textures with
	// standard 0..1 UVs, so any canvas-y flip would invert the chain.
	// Width/height come from the FBO's color attachment via
	// nx_webgl_egl_set_user_framebuffer; the bridge uses these for its
	// CPU-side perspective-divide viewport scaling.
	GLuint current_user_framebuffer;
	int current_user_framebuffer_width;
	int current_user_framebuffer_height;
	// User-bound VAO (gl.bindVertexArray). 0 means use the internal
	// passthrough_vao (legacy WebGL 1 path). Non-zero overrides
	// passthrough_vao at draw time.
	GLuint current_user_vao;
	// UBO indexed binding slot tracking. Mesa Nouveau resets these on
	// eglMakeCurrent (even when making the same context current — likely a
	// driver bug). The passthrough dispatch calls eglMakeCurrent at the
	// start of every draw, so we must snapshot user-set slot bindings via
	// bind_buffer_base / bind_buffer_range and re-apply them at the top of
	// every passthrough draw. Sized to match MAX_UNIFORM_BUFFER_BINDINGS
	// (84 per smoke test) with headroom. Index = binding slot.
	#define NX_WEBGL_MAX_UBO_BINDINGS 96
	GLuint ubo_indexed_bindings[NX_WEBGL_MAX_UBO_BINDINGS];
	// Range-binding offsets/sizes for bind_buffer_range. Offset=0 size=0
	// means "use bind_buffer_base semantics" (whole buffer).
	GLintptr ubo_indexed_offsets[NX_WEBGL_MAX_UBO_BINDINGS];
	GLsizeiptr ubo_indexed_sizes[NX_WEBGL_MAX_UBO_BINDINGS];
	// WebGL 2 (GLES 3) entry points resolved lazily via eglGetProcAddress
	// at probe step 8. NULL pointers mean "feature not available on this
	// driver" — JS-side wrappers set INVALID_OPERATION when missing.
	bool webgl2_present;
	void *fn_vertex_attrib_i_pointer;
	void *fn_vertex_attrib_i4i;
	void *fn_vertex_attrib_i4ui;
	void *fn_uniform1ui;
	void *fn_uniform2ui;
	void *fn_uniform3ui;
	void *fn_uniform4ui;
	void *fn_uniform1uiv;
	void *fn_uniform2uiv;
	void *fn_uniform3uiv;
	void *fn_uniform4uiv;
	void *fn_uniform_matrix2x3fv;
	void *fn_uniform_matrix3x2fv;
	void *fn_uniform_matrix2x4fv;
	void *fn_uniform_matrix4x2fv;
	void *fn_uniform_matrix3x4fv;
	void *fn_uniform_matrix4x3fv;
	void *fn_draw_buffers;
	void *fn_invalidate_framebuffer;
	void *fn_invalidate_sub_framebuffer;
	void *fn_blit_framebuffer;
	void *fn_read_buffer;
	void *fn_renderbuffer_storage_multisample;
	void *fn_framebuffer_texture_layer;
	void *fn_tex_image_3d;
	void *fn_tex_sub_image_3d;
	void *fn_copy_tex_sub_image_3d;
	void *fn_compressed_tex_image_3d;
	void *fn_compressed_tex_sub_image_3d;
	void *fn_tex_storage_2d;
	void *fn_tex_storage_3d;
	void *fn_clear_buffer_iv;
	void *fn_clear_buffer_uiv;
	void *fn_clear_buffer_fv;
	void *fn_clear_buffer_fi;
	void *fn_copy_buffer_sub_data;
	void *fn_get_buffer_sub_data;
	void *fn_bind_buffer_base;
	void *fn_bind_buffer_range;
	void *fn_get_uniform_block_index;
	void *fn_uniform_block_binding;
	void *fn_get_active_uniform_block_iv;
	void *fn_get_active_uniform_block_name;
	void *fn_get_active_uniforms_iv;
	void *fn_get_uniform_indices;
	void *fn_gen_samplers;
	void *fn_delete_samplers;
	void *fn_bind_sampler;
	void *fn_sampler_parameteri;
	void *fn_sampler_parameterf;
	void *fn_get_sampler_parameter_iv;
	void *fn_fence_sync;
	void *fn_delete_sync;
	void *fn_client_wait_sync;
	void *fn_wait_sync;
	void *fn_get_sync_iv;
	void *fn_gen_queries;
	void *fn_delete_queries;
	void *fn_begin_query;
	void *fn_end_query;
	void *fn_get_query_iv;
	void *fn_get_query_object_uiv;
	void *fn_gen_transform_feedbacks;
	void *fn_delete_transform_feedbacks;
	void *fn_bind_transform_feedback;
	void *fn_begin_transform_feedback;
	void *fn_end_transform_feedback;
	void *fn_pause_transform_feedback;
	void *fn_resume_transform_feedback;
	void *fn_transform_feedback_varyings;
	void *fn_get_transform_feedback_varying;
	void *fn_get_frag_data_location;
	void *fn_get_internal_format_iv;
#endif
};

static void define_bool(JSContext *ctx, JSValue obj, const char *name,
						bool value) {
	JS_DefinePropertyValueStr(ctx, obj, name, JS_NewBool(ctx, value),
							  JS_PROP_C_W);
}

static void define_string(JSContext *ctx, JSValue obj, const char *name,
						  const char *value) {
	JS_DefinePropertyValueStr(ctx, obj, name,
							  JS_NewString(ctx, value ? value : ""),
							  JS_PROP_C_W);
}

static void define_int(JSContext *ctx, JSValue obj, const char *name,
					   int value) {
	JS_DefinePropertyValueStr(ctx, obj, name, JS_NewInt32(ctx, value),
							  JS_PROP_C_W);
}

static void define_double(JSContext *ctx, JSValue obj, const char *name,
						  double value) {
	JS_DefinePropertyValueStr(ctx, obj, name, JS_NewFloat64(ctx, value),
							  JS_PROP_C_W);
}

static int clamp_int(int value, int min_value, int max_value) {
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

static JSValue make_triangle_result(JSContext *ctx, bool ok,
									const char *status,
									const uint8_t *pixel) {
	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;

	define_bool(ctx, obj, "ok", ok);
	define_string(ctx, obj, "status", status);
	define_int(ctx, obj, "red", pixel ? pixel[0] : 0);
	define_int(ctx, obj, "green", pixel ? pixel[1] : 0);
	define_int(ctx, obj, "blue", pixel ? pixel[2] : 0);
	define_int(ctx, obj, "alpha", pixel ? pixel[3] : 0);
	return obj;
}

static JSValue make_bridge_result(JSContext *ctx, bool ok,
								  const char *status, int width, int height,
								  int copied_pixels, const uint8_t *pixel) {
	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;

	define_bool(ctx, obj, "ok", ok);
	define_string(ctx, obj, "status", status);
	define_int(ctx, obj, "width", width);
	define_int(ctx, obj, "height", height);
	define_int(ctx, obj, "copiedPixels", copied_pixels);
	define_int(ctx, obj, "red", pixel ? pixel[0] : 0);
	define_int(ctx, obj, "green", pixel ? pixel[1] : 0);
	define_int(ctx, obj, "blue", pixel ? pixel[2] : 0);
	define_int(ctx, obj, "alpha", pixel ? pixel[3] : 0);
	return obj;
}

static JSValue make_bridge_benchmark_result(
	JSContext *ctx, bool ok, const char *status, int width, int height,
	int frame_count, int copied_pixels, double elapsed_ms,
	const uint8_t *pixel) {
	JSValue obj = make_bridge_result(ctx, ok, status, width, height,
									 copied_pixels, pixel);
	if (JS_IsException(obj))
		return obj;

	define_int(ctx, obj, "frameCount", frame_count);
	define_double(ctx, obj, "elapsedMs", elapsed_ms);
	define_double(ctx, obj, "averageFrameMs",
				  frame_count > 0 ? elapsed_ms / (double)frame_count : 0.);
	define_double(ctx, obj, "fps",
				  elapsed_ms > 0. ? (double)frame_count * 1000. / elapsed_ms
								  : 0.);
	return obj;
}

#if NXJS_HAS_EGL_GLES
static GLuint compile_triangle_shader(GLenum type, const char *source,
									  char *status, size_t status_size);

static void destroy_bridge_resources(nx_webgl_egl_t *backend) {
	if (!backend)
		return;
	if (backend->bridge_vertex_buffer) {
		glDeleteBuffers(1, &backend->bridge_vertex_buffer);
		backend->bridge_vertex_buffer = 0;
	}
	if (backend->bridge_color_program) {
		glDeleteProgram(backend->bridge_color_program);
		backend->bridge_color_program = 0;
	}
	if (backend->bridge_color_vertex_shader) {
		glDeleteShader(backend->bridge_color_vertex_shader);
		backend->bridge_color_vertex_shader = 0;
	}
	if (backend->bridge_color_fragment_shader) {
		glDeleteShader(backend->bridge_color_fragment_shader);
		backend->bridge_color_fragment_shader = 0;
	}
	if (backend->bridge_texture_program) {
		glDeleteProgram(backend->bridge_texture_program);
		backend->bridge_texture_program = 0;
	}
	if (backend->bridge_texture_vertex_shader) {
		glDeleteShader(backend->bridge_texture_vertex_shader);
		backend->bridge_texture_vertex_shader = 0;
	}
	if (backend->bridge_texture_fragment_shader) {
		glDeleteShader(backend->bridge_texture_fragment_shader);
		backend->bridge_texture_fragment_shader = 0;
	}
	if (backend->bridge_line_distance_buffer) {
		glDeleteBuffers(1, &backend->bridge_line_distance_buffer);
		backend->bridge_line_distance_buffer = 0;
	}
	if (backend->bridge_line_color_buffer) {
		glDeleteBuffers(1, &backend->bridge_line_color_buffer);
		backend->bridge_line_color_buffer = 0;
	}
	if (backend->bridge_triangle_color_buffer) {
		glDeleteBuffers(1, &backend->bridge_triangle_color_buffer);
		backend->bridge_triangle_color_buffer = 0;
	}
	if (backend->bridge_fog_depth_buffer) {
		glDeleteBuffers(1, &backend->bridge_fog_depth_buffer);
		backend->bridge_fog_depth_buffer = 0;
	}
	if (backend->bridge_normal_buffer) {
		glDeleteBuffers(1, &backend->bridge_normal_buffer);
		backend->bridge_normal_buffer = 0;
	}
	if (backend->bridge_view_position_buffer) {
		glDeleteBuffers(1, &backend->bridge_view_position_buffer);
		backend->bridge_view_position_buffer = 0;
	}
	if (backend->bridge_line_program) {
		glDeleteProgram(backend->bridge_line_program);
		backend->bridge_line_program = 0;
	}
	if (backend->bridge_line_vertex_shader) {
		glDeleteShader(backend->bridge_line_vertex_shader);
		backend->bridge_line_vertex_shader = 0;
	}
	if (backend->bridge_line_fragment_shader) {
		glDeleteShader(backend->bridge_line_fragment_shader);
		backend->bridge_line_fragment_shader = 0;
	}
	if (backend->bridge_framebuffer) {
		glDeleteFramebuffers(1, &backend->bridge_framebuffer);
		backend->bridge_framebuffer = 0;
	}
	if (backend->bridge_depth_renderbuffer) {
		glDeleteRenderbuffers(1, &backend->bridge_depth_renderbuffer);
		backend->bridge_depth_renderbuffer = 0;
	}
	if (backend->bridge_texture) {
		glDeleteTextures(1, &backend->bridge_texture);
		backend->bridge_texture = 0;
	}
	free(backend->bridge_readback);
	backend->bridge_readback = NULL;
	backend->bridge_readback_size = 0;
	backend->bridge_width = 0;
	backend->bridge_height = 0;
	backend->bridge_line_color_loc = -1;
	backend->bridge_line_scale_loc = -1;
	backend->bridge_line_dash_loc = -1;
	backend->bridge_line_total_loc = -1;
	backend->bridge_color_color_loc = -1;
	backend->bridge_texture_sampler_loc = -1;
	backend->bridge_pending_readback = false;
}

static void destroy_texture_cache(nx_webgl_egl_t *backend) {
	if (!backend)
		return;
	for (int i = 0; i < NX_WEBGL_EGL_TEXTURE_CACHE_SIZE; i++) {
		if (backend->texture_cache[i].handle) {
			glDeleteTextures(1, &backend->texture_cache[i].handle);
			memset(&backend->texture_cache[i], 0,
				   sizeof(backend->texture_cache[i]));
		}
	}
}

static bool ensure_bridge_resources(nx_webgl_egl_t *backend, int width,
									int height) {
	if (!backend || width <= 0 || height <= 0)
		return false;
	if (backend->bridge_framebuffer && backend->bridge_texture &&
		backend->bridge_readback && backend->bridge_width == width &&
		backend->bridge_height == height) {
		return true;
	}

	destroy_bridge_resources(backend);

	size_t readback_size = (size_t)width * (size_t)height * 4;
	backend->bridge_readback = malloc(readback_size);
	if (!backend->bridge_readback) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge: failed to allocate %zu byte readback buffer",
				 readback_size);
		return false;
	}
	memset(backend->bridge_readback, 0, readback_size);
	backend->bridge_readback_size = readback_size;

	glGenTextures(1, &backend->bridge_texture);
	glBindTexture(GL_TEXTURE_2D, backend->bridge_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, NULL);

	// 2026-06-08 ROUND 25: bridge FBO now uses DEPTH24_STENCIL8 (was
	// DEPTH_COMPONENT16). Without a stencil attachment, `gl.enable(STENCIL_TEST)`
	// + stencilFunc/Op/Mask have no buffer to operate on — Cocos cc.Mask,
	// Three.js clipping planes, and any portal/stencil-shadow engine produce
	// no clipping. Real browsers provide stencil by default. The depth
	// portion is still 24-bit (more precision than the prior 16-bit) at the
	// cost of 1 extra byte per pixel for stencil.
	glGenRenderbuffers(1, &backend->bridge_depth_renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, backend->bridge_depth_renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, 0x88F0 /* GL_DEPTH24_STENCIL8 */,
						  width, height);

	glGenFramebuffers(1, &backend->bridge_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, backend->bridge_texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
							  0x821A /* GL_DEPTH_STENCIL_ATTACHMENT */,
							  GL_RENDERBUFFER,
							  backend->bridge_depth_renderbuffer);
	GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge: framebuffer incomplete: 0x%x",
				 framebuffer_status);
		destroy_bridge_resources(backend);
		return false;
	}

	// Initialize the bridge color attachment to transparent black so the
	// first frame doesn't flash Tegra's uninitialized GPU-memory contents
	// (commonly a leftover red from previous framebuffer recycling). After
	// this clear, the bridge is empty until the page's first gl.clear() /
	// draw runs. See pvzge 2026-06-07 red-flash request.
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	backend->bridge_width = width;
	backend->bridge_height = height;
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge resources ready: %dx%d", width, height);
	return true;
}

static void copy_rgba_readback_to_canvas(nx_canvas_t *canvas, uint8_t *readback,
										 int width, int height, int dst_x,
										 int dst_y) {
	uint32_t *dst = (uint32_t *)canvas->data;
	for (int y = 0; y < height; y++) {
		int src_y = height - 1 - y;
		for (int x = 0; x < width; x++) {
			uint8_t *src = readback + ((size_t)src_y * width + x) * 4;
			uint32_t packed = ((uint32_t)src[3] << 24) |
							  ((uint32_t)src[0] << 16) |
							  ((uint32_t)src[1] << 8) | (uint32_t)src[2];
			dst[(size_t)(dst_y + y) * canvas->width + (dst_x + x)] = packed;
		}
	}
	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

static uint32_t packed_rgba_pixel(uint8_t *src) {
	return ((uint32_t)src[3] << 24) | ((uint32_t)src[0] << 16) |
		   ((uint32_t)src[1] << 8) | (uint32_t)src[2];
}

static void copy_rgba_readback_to_canvas_scaled(nx_canvas_t *canvas,
												uint8_t *readback,
												int width,
												int height) {
	if ((int)canvas->width == width && (int)canvas->height == height) {
		copy_rgba_readback_to_canvas(canvas, readback, width, height, 0, 0);
		return;
	}

	uint32_t *dst = (uint32_t *)canvas->data;
	int canvas_width = (int)canvas->width;
	int canvas_height = (int)canvas->height;
	if (canvas_width == width * 2 && canvas_height == height * 2) {
		for (int y = 0; y < height; y++) {
			int src_y = height - 1 - y;
			uint32_t *dst0 = dst + (size_t)(y * 2) * canvas_width;
			uint32_t *dst1 = dst0 + canvas_width;
			for (int x = 0; x < width; x++) {
				uint8_t *src = readback + ((size_t)src_y * width + x) * 4;
				uint32_t packed = packed_rgba_pixel(src);
				int dst_x = x * 2;
				dst0[dst_x] = packed;
				dst0[dst_x + 1] = packed;
				dst1[dst_x] = packed;
				dst1[dst_x + 1] = packed;
			}
		}
	} else {
		for (int y = 0; y < canvas_height; y++) {
			int scaled_y = (int)(((int64_t)y * height) / canvas_height);
			int src_y = height - 1 - scaled_y;
			uint32_t *dst_row = dst + (size_t)y * canvas_width;
			for (int x = 0; x < canvas_width; x++) {
				int src_x = (int)(((int64_t)x * width) / canvas_width);
				uint8_t *src = readback + ((size_t)src_y * width + src_x) * 4;
				dst_row[x] = packed_rgba_pixel(src);
			}
		}
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

static void bridge_render_size(nx_webgl_egl_t *backend, nx_canvas_t *canvas,
							   int *width, int *height) {
	int requested_width = backend->bridge_requested_width;
	int requested_height = backend->bridge_requested_height;
	if (requested_width <= 0 || requested_height <= 0) {
		*width = (int)canvas->width;
		*height = (int)canvas->height;
		return;
	}

	*width = requested_width < (int)canvas->width ? requested_width
												  : (int)canvas->width;
	*height = requested_height < (int)canvas->height ? requested_height
													 : (int)canvas->height;
	if (*width <= 0)
		*width = (int)canvas->width;
	if (*height <= 0)
		*height = (int)canvas->height;
}

// `spec_y=false` (the historical default): input rect.y is in canvas-y
// top-down convention; output y is flipped to GL-y bottom-up. Every
// production demo's coordinate system depends on this — see the bridge-
// Y-convention entry in REAL_GL_FAILURES.md.
// `spec_y=true` (the Option 2 opt-in for the conformance runner): input
// rect.y is already in spec/GL-y bottom-up convention; pass through with
// only scaling, no axis inversion. Scaling math is identical in both
// modes; only the output-y derivation differs.
static void bridge_scale_rect(nx_canvas_t *canvas, int render_width,
							  int render_height, const int *rect,
							  bool spec_y, int *x,
							  int *y, int *width, int *height) {
	int canvas_width = (int)canvas->width;
	int canvas_height = (int)canvas->height;
	int rx = rect ? rect[0] : 0;
	int ry = rect ? rect[1] : 0;
	int rw = rect ? rect[2] : canvas_width;
	int rh = rect ? rect[3] : canvas_height;
	int x0 = clamp_int(rx, 0, canvas_width);
	int y0 = clamp_int(ry, 0, canvas_height);
	int x1 = clamp_int(rx + rw, 0, canvas_width);
	int y1 = clamp_int(ry + rh, 0, canvas_height);
	if (x1 < x0)
		x1 = x0;
	if (y1 < y0)
		y1 = y0;

	int sx0 = (int)(((int64_t)x0 * render_width) / canvas_width);
	int sx1 = (int)(((int64_t)x1 * render_width + canvas_width - 1) /
					canvas_width);
	int sy0 = (int)(((int64_t)y0 * render_height) / canvas_height);
	int sy1 = (int)(((int64_t)y1 * render_height + canvas_height - 1) /
					canvas_height);
	*x = clamp_int(sx0, 0, render_width);
	*width = clamp_int(sx1, 0, render_width) - *x;
	if (spec_y) {
		// Pass-through: input rect.y is already GL bottom-up.
		*y = clamp_int(sy0, 0, render_height);
		*height = clamp_int(sy1, 0, render_height) - *y;
	} else {
		// Canvas-y top-down → GL-y bottom-up flip.
		*y = render_height - clamp_int(sy1, 0, render_height);
		*height = clamp_int(sy1, 0, render_height) -
				  clamp_int(sy0, 0, render_height);
	}
	if (*width < 0)
		*width = 0;
	if (*height < 0)
		*height = 0;
}

static void bridge_apply_viewport(nx_canvas_t *canvas, int render_width,
								  int render_height, const int *viewport,
								  bool spec_y) {
	int x;
	int y;
	int width;
	int height;
	bridge_scale_rect(canvas, render_width, render_height, viewport, spec_y,
					  &x, &y, &width, &height);
	glViewport(x, y, width, height);
}

static void bridge_apply_scissor(nx_canvas_t *canvas, int render_width,
								 int render_height, bool enabled,
								 const int *scissor_box, bool spec_y) {
	if (!enabled) {
		glDisable(GL_SCISSOR_TEST);
		return;
	}
	int x;
	int y;
	int width;
	int height;
	bridge_scale_rect(canvas, render_width, render_height, scissor_box, spec_y,
					  &x, &y, &width, &height);
	glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, width, height);
}

// Target framebuffer for the next bridge dispatch. Either the bridge's own
// FBO (sized to the canvas, top-down y) or a user FBO (sized to whatever
// Three.js asked for via WebGLRenderTarget, standard GL bottom-up y).
typedef struct {
	GLuint fbo;
	int width;
	int height;
	bool is_user_fbo;
} bridge_target_t;

// Resolve the current bridge target. If the user has bound a non-null FBO via
// gl.bindFramebuffer, return that; otherwise ensure_bridge_resources for the
// canvas-derived size and return the bridge's own FBO. Failure means we
// can't draw (callers should bail out).
static bool bridge_acquire_target(nx_webgl_egl_t *backend,
                                  nx_canvas_t *canvas,
                                  bridge_target_t *out) {
	if (!backend || !canvas || !out)
		return false;
	if (backend->current_user_framebuffer) {
		out->fbo = backend->current_user_framebuffer;
		out->width = backend->current_user_framebuffer_width > 0
			? backend->current_user_framebuffer_width
			: (int)canvas->width;
		out->height = backend->current_user_framebuffer_height > 0
			? backend->current_user_framebuffer_height
			: (int)canvas->height;
		out->is_user_fbo = true;
		return true;
	}
	int w = 0;
	int h = 0;
	bridge_render_size(backend, canvas, &w, &h);
	if (!ensure_bridge_resources(backend, w, h))
		return false;
	out->fbo = backend->bridge_framebuffer;
	out->width = w;
	out->height = h;
	out->is_user_fbo = false;
	return true;
}

// Bind the resolved target FBO and apply viewport + scissor in its native
// coordinate system. For user FBOs that's standard GL (origin bottom-left,
// viewport rect interpreted directly). For the bridge FBO we keep the
// canvas-y top-down convention via bridge_apply_viewport/scissor.
static void bridge_bind_target(const bridge_target_t *t,
                               nx_webgl_egl_t *backend,
                               nx_canvas_t *canvas,
                               const int *viewport,
                               bool scissor_enabled,
                               const int *scissor_box) {
	glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
	bool spec_y = backend && backend->spec_y_origin;
	if (t->is_user_fbo) {
		int rx = viewport ? viewport[0] : 0;
		int ry = viewport ? viewport[1] : 0;
		int rw = viewport ? viewport[2] : t->width;
		int rh = viewport ? viewport[3] : t->height;
		glViewport(rx, ry, rw, rh);
		if (scissor_enabled) {
			int sx = scissor_box ? scissor_box[0] : 0;
			int sy = scissor_box ? scissor_box[1] : 0;
			int sw = scissor_box ? scissor_box[2] : t->width;
			int sh = scissor_box ? scissor_box[3] : t->height;
			glEnable(GL_SCISSOR_TEST);
			glScissor(sx, sy, sw, sh);
		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	} else {
		bridge_apply_viewport(canvas, t->width, t->height, viewport, spec_y);
		bridge_apply_scissor(canvas, t->width, t->height, scissor_enabled,
		                     scissor_box, spec_y);
	}
}

// After a successful bridge dispatch, schedule the present-time readback ONLY
// if we drew into the bridge's own FBO. Draws into a user FBO leave the
// canvas untouched — the next pass will sample the FBO's color texture and
// the final OutputPass renders back to bridge_framebuffer (which DOES set
// this flag in the usual way).
static inline void bridge_mark_readback(nx_webgl_egl_t *backend,
                                        const bridge_target_t *t) {
	if (!t->is_user_fbo)
		backend->bridge_pending_readback = true;
}

static bool ensure_bridge_color_program(nx_webgl_egl_t *backend) {
	if (backend->bridge_color_program && backend->bridge_vertex_buffer &&
		backend->bridge_triangle_color_buffer &&
		backend->bridge_fog_depth_buffer &&
		backend->bridge_normal_buffer &&
		backend->bridge_view_position_buffer)
		return true;

	// Untextured triangle shader pair.
	// - Position: a_position (vec3 NDC at location 0). CPU already did the
	//   model-view-projection transform + perspective divide; the shader just
	//   passes the result through. Z is preserved so GL_DEPTH_TEST works.
	// - Per-vertex color: a_color (location 1), output = u_color * vec4(v_color, 1).
	//   Callers that don't bind a color buffer use glVertexAttrib3f(1, 1, 1, 1) so
	//   v_color defaults to white and output collapses to u_color.
	// - Linear fog: a_fogDepth (location 2) = view-space -mz (Three.js's
	//   vFogDepth). Fragment shader mixes in u_fogColor by smoothstep(near,
	//   far, depth) when u_fogEnabled > 0.5 — matching Three.js's `Fog`
	//   linear variant.
	// - Single directional light: a_normal (location 3) = view-space normal
	//   (CPU pre-transformed by `normalMatrix = inverseTranspose(modelView)`).
	//   Fragment shader applies Lambert diffuse `dot(N, L)` against
	//   `u_lightDirection` (view-space, toward light) plus u_ambientLightColor,
	//   modulated by u_lightColor when u_lightingEnabled > 0.5.
	// - Single point light: a_viewPosition (location 4) = per-vertex view-
	//   space position (CPU pre-transformed by modelView). Fragment shader
	//   computes per-fragment light direction `normalize(pointLightPosition
	//   - v_viewPosition)`, Lambert diffuse against it, optionally with
	//   distance + decay attenuation. Either or both light kinds can be
	//   enabled per frame (Three.js sends both sets of uniforms when both
	//   light types are in the scene). Multi-light not yet supported —
	//   only first directional + first point.
	static const char vertex_source[] =
		"attribute vec3 a_position;\n"
		"attribute vec3 a_color;\n"
		"attribute float a_fogDepth;\n"
		"attribute vec3 a_normal;\n"
		"attribute vec3 a_viewPosition;\n"
		"varying vec3 v_color;\n"
		"varying float v_fogDepth;\n"
		"varying vec3 v_normal;\n"
		"varying vec3 v_viewPosition;\n"
		"void main() {\n"
		"  v_color = a_color;\n"
		"  v_fogDepth = a_fogDepth;\n"
		"  v_normal = a_normal;\n"
		"  v_viewPosition = a_viewPosition;\n"
		"  gl_Position = vec4(a_position, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		// GL_OES_standard_derivatives needed for dFdx/dFdy in the derivative-
		// normals fallback (milestone #16). Tegra X1 (Maxwell GLES 3.2)
		// supports it; the bridge gates dispatch on the extension's runtime
		// availability through `u_useDerivativeNormals`. Default-off so
		// drivers without the extension are unaffected when the dispatch
		// keeps the uniform at 0 (which the static `0.0 > 0.5` branch the
		// optimizer folds away).
		"#extension GL_OES_standard_derivatives : enable\n"
		"precision mediump float;\n"
		"uniform vec4 u_color;\n"
		"uniform float u_fogEnabled;\n"
		"uniform float u_fogExp2Enabled;\n"
		"uniform vec3 u_fogColor;\n"
		"uniform float u_fogNear;\n"
		"uniform float u_fogFar;\n"
		"uniform float u_fogDensity;\n"
		"uniform float u_lightingEnabled;\n"
		"uniform vec3 u_lightDirection;\n"
		"uniform vec3 u_lightColor;\n"
		"uniform float u_light2Enabled;\n"
		"uniform vec3 u_lightDirection2;\n"
		"uniform vec3 u_lightColor2;\n"
		"uniform vec3 u_ambientLightColor;\n"
		"uniform float u_pointLightEnabled;\n"
		"uniform vec3 u_pointLightPosition;\n"
		"uniform vec3 u_pointLightColor;\n"
		"uniform float u_pointLightDistance;\n"
		"uniform float u_pointLightDecay;\n"
		"uniform float u_specularEnabled;\n"
		"uniform vec3 u_specular;\n"
		"uniform float u_shininess;\n"
		"uniform vec3 u_emissive;\n"
		"uniform float u_hemiLightEnabled;\n"
		"uniform vec3 u_hemiLightDirection;\n"
		"uniform vec3 u_hemiLightSkyColor;\n"
		"uniform vec3 u_hemiLightGroundColor;\n"
		"uniform float u_useDerivativeNormals;\n"
		// SpotLight (single light only). All vector uniforms are view-space.
		// `u_spotLightDirection` is the unit vector from light toward target;
		// the math negates it inside the cone test (`dot(L, -dir)`) so the
		// caller passes Three.js's raw `spotLights[0].direction` unchanged.
		"uniform float u_spotLightEnabled;\n"
		"uniform vec3 u_spotLightPosition;\n"
		"uniform vec3 u_spotLightDirection;\n"
		"uniform vec3 u_spotLightColor;\n"
		"uniform float u_spotLightDistance;\n"
		"uniform float u_spotLightConeCos;\n"
		"uniform float u_spotLightPenumbraCos;\n"
		"uniform float u_spotLightDecay;\n"
		"varying vec3 v_color;\n"
		"varying float v_fogDepth;\n"
		"varying vec3 v_normal;\n"
		"varying vec3 v_viewPosition;\n"
		"void main() {\n"
		"  vec4 base = u_color * vec4(v_color, 1.0);\n"
		"  if (u_lightingEnabled > 0.5) {\n"
		// Three.js's `flatShading: true` materials cause the GLES driver
		// to dead-code the `normal` attribute, so the bridge sees no
		// per-vertex normal. When the dispatch detects this it sets
		// u_useDerivativeNormals=1 and we derive the per-fragment normal
		// from view-position derivatives (constant within a triangle →
		// effectively flat-shaded, which matches Three.js's
		// `flatShading: true` visual intent).
		"    vec3 N;\n"
		"    if (u_useDerivativeNormals > 0.5) {\n"
		"      N = normalize(cross(dFdx(v_viewPosition), dFdy(v_viewPosition)));\n"
		"    } else {\n"
		"      N = normalize(v_normal);\n"
		"    }\n"
		"    vec3 V = normalize(-v_viewPosition);\n"
		"    vec3 diffuse = u_lightColor * max(dot(N, u_lightDirection), 0.0);\n"
		"    vec3 specular = vec3(0.0);\n"
		"    if (u_specularEnabled > 0.5) {\n"
		"      vec3 H = normalize(u_lightDirection + V);\n"
		"      specular += u_lightColor * pow(max(dot(N, H), 0.0), u_shininess);\n"
		"    }\n"
		"    if (u_light2Enabled > 0.5) {\n"
		"      diffuse += u_lightColor2 * max(dot(N, u_lightDirection2), 0.0);\n"
		"      if (u_specularEnabled > 0.5) {\n"
		"        vec3 H2 = normalize(u_lightDirection2 + V);\n"
		"        specular += u_lightColor2 * pow(max(dot(N, H2), 0.0), u_shininess);\n"
		"      }\n"
		"    }\n"
		"    if (u_pointLightEnabled > 0.5) {\n"
		"      vec3 lightVec = u_pointLightPosition - v_viewPosition;\n"
		"      float distance = length(lightVec);\n"
		"      vec3 L = lightVec / max(distance, 0.0001);\n"
		"      float NdotL = max(dot(N, L), 0.0);\n"
		"      float atten = 1.0;\n"
		"      if (u_pointLightDecay > 0.0)\n"
		"        atten = 1.0 / max(pow(distance, u_pointLightDecay), 1.0);\n"
		"      if (u_pointLightDistance > 0.0)\n"
		"        atten *= max(0.0, 1.0 - distance / u_pointLightDistance);\n"
		"      diffuse += u_pointLightColor * NdotL * atten;\n"
		"      if (u_specularEnabled > 0.5) {\n"
		"        vec3 Hp = normalize(L + V);\n"
		"        specular += u_pointLightColor * pow(max(dot(N, Hp), 0.0), u_shininess) * atten;\n"
		"      }\n"
		"    }\n"
		// SpotLight (single light only). Math mirrors Three.js's
		// lights_spot_fragment.glsl chunk: cone attenuation via
		// smoothstep(coneCos, penumbraCos, dot(L, dir)) gates the
		// Lambert NdotL contribution; distance attenuation matches the
		// point-light formula above. coneCos / penumbraCos are pre-
		// computed by Three.js's WebGLLights.setupLights() so the shader
		// stays branch-free on angle/penumbra.
		//
		// IMPORTANT: Three.js's `spotLights[0].direction` is computed as
		// `position - target` (i.e. it points FROM the target TOWARD the
		// light), NOT light→target. For a fragment under the cone, BOTH
		// `Ls` (fragment→light) AND `spotLight.direction` point roughly in
		// the same direction; their dot product is positive. No negation —
		// see Three.js's getSpotLightInfo() in lights_spot_pars_fragment.glsl
		// which does `dot(light.direction, spotLight.direction)` directly.
		//
		// NOTE: this code path is dormant for stock Three.js because
		// Three.js's shaders contain `#define SHADER_NAME` which trips the
		// raw_passthrough gate in webgl.c → draws bypass the bridge and run
		// Three.js's actual GLSL natively. Kept here for non-Three.js apps
		// that use the bridge directly.
		"    if (u_spotLightEnabled > 0.5) {\n"
		"      vec3 spotVec = u_spotLightPosition - v_viewPosition;\n"
		"      float spotDist = length(spotVec);\n"
		"      vec3 Ls = spotVec / max(spotDist, 0.0001);\n"
		"      float spotAngleCos = dot(Ls, u_spotLightDirection);\n"
		"      float spotEffect = smoothstep(u_spotLightConeCos, u_spotLightPenumbraCos, spotAngleCos);\n"
		"      if (spotEffect > 0.0) {\n"
		"        float spotAtten = 1.0;\n"
		"        if (u_spotLightDecay > 0.0)\n"
		"          spotAtten = 1.0 / max(pow(spotDist, u_spotLightDecay), 1.0);\n"
		"        if (u_spotLightDistance > 0.0)\n"
		"          spotAtten *= max(0.0, 1.0 - spotDist / u_spotLightDistance);\n"
		"        float spotNdotL = max(dot(N, Ls), 0.0);\n"
		"        diffuse += u_spotLightColor * spotNdotL * spotEffect * spotAtten;\n"
		"        if (u_specularEnabled > 0.5) {\n"
		"          vec3 Hs = normalize(Ls + V);\n"
		"          specular += u_spotLightColor * pow(max(dot(N, Hs), 0.0), u_shininess) * spotEffect * spotAtten;\n"
		"        }\n"
		"      }\n"
		"    }\n"
		// HemisphereLight (Three.js): an ambient-class irradiance that
		// blends sky and ground colors by the normal's projection onto
		// the up axis. Added to ambient (not to NdotL diffuse) to match
		// Three.js's `lights_fragment_begin.glsl.js`:
		//   irradiance += mix(ground, sky, 0.5 * dot(N, dir) + 0.5)
		"    vec3 hemiIrradiance = vec3(0.0);\n"
		"    if (u_hemiLightEnabled > 0.5) {\n"
		"      float hemiDotNL = dot(N, u_hemiLightDirection);\n"
		"      float hemiDiffuseWeight = 0.5 * hemiDotNL + 0.5;\n"
		"      hemiIrradiance = mix(u_hemiLightGroundColor, u_hemiLightSkyColor, hemiDiffuseWeight);\n"
		"    }\n"
		"    vec3 lit = base.rgb * (u_ambientLightColor + hemiIrradiance + diffuse) + u_specular * specular;\n"
		"    gl_FragColor = vec4(lit, base.a);\n"
		"  } else {\n"
		"    gl_FragColor = base;\n"
		"  }\n"
		// Self-illumination from `material.emissive` (Three.js MeshLambert/
		// Phong/Standard). Additive after lighting so it shows even in
		// unlit regions; default zero so non-emissive materials unaffected.
		// Applied BEFORE fog so emissive materials still fade with distance.
		"  gl_FragColor.rgb += u_emissive;\n"
		"  if (u_fogEnabled > 0.5) {\n"
		"    float f;\n"
		"    if (u_fogExp2Enabled > 0.5) {\n"
		"      f = 1.0 - exp(-u_fogDensity * u_fogDensity * v_fogDepth * v_fogDepth);\n"
		"    } else {\n"
		"      f = smoothstep(u_fogNear, u_fogFar, v_fogDepth);\n"
		"    }\n"
		"    gl_FragColor.rgb = mix(gl_FragColor.rgb, u_fogColor, clamp(f, 0.0, 1.0));\n"
		"  }\n"
		"}\n";

	if (!backend->bridge_color_vertex_shader) {
		backend->bridge_color_vertex_shader =
			compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
									backend->status, sizeof(backend->status));
		if (!backend->bridge_color_vertex_shader)
			return false;
	}
	if (!backend->bridge_color_fragment_shader) {
		backend->bridge_color_fragment_shader =
			compile_triangle_shader(GL_FRAGMENT_SHADER, fragment_source,
									backend->status, sizeof(backend->status));
		if (!backend->bridge_color_fragment_shader)
			return false;
	}

	if (!backend->bridge_color_program) {
		backend->bridge_color_program = glCreateProgram();
		if (!backend->bridge_color_program) {
			snprintf(backend->status, sizeof(backend->status),
					 "GPU bridge draw: glCreateProgram() failed: 0x%x",
					 glGetError());
			return false;
		}
		glAttachShader(backend->bridge_color_program,
					   backend->bridge_color_vertex_shader);
		glAttachShader(backend->bridge_color_program,
					   backend->bridge_color_fragment_shader);
		glBindAttribLocation(backend->bridge_color_program, 0, "a_position");
		glBindAttribLocation(backend->bridge_color_program, 1, "a_color");
		glBindAttribLocation(backend->bridge_color_program, 2, "a_fogDepth");
		glBindAttribLocation(backend->bridge_color_program, 3, "a_normal");
		glBindAttribLocation(backend->bridge_color_program, 4, "a_viewPosition");
		glLinkProgram(backend->bridge_color_program);

		GLint linked = GL_FALSE;
		glGetProgramiv(backend->bridge_color_program, GL_LINK_STATUS, &linked);
		if (!linked) {
			GLchar log[128];
			GLsizei log_length = 0;
			glGetProgramInfoLog(backend->bridge_color_program, sizeof(log),
								&log_length, log);
			snprintf(backend->status, sizeof(backend->status),
					 "GPU bridge draw: glLinkProgram() failed: %.*s",
					 (int)log_length, log);
			return false;
		}
		backend->bridge_color_color_loc =
			glGetUniformLocation(backend->bridge_color_program, "u_color");
		backend->bridge_color_fog_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogEnabled");
		backend->bridge_color_fog_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogColor");
		backend->bridge_color_fog_near_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogNear");
		backend->bridge_color_fog_far_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogFar");
		backend->bridge_color_lighting_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_lightingEnabled");
		backend->bridge_color_light_direction_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_lightDirection");
		backend->bridge_color_light_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_lightColor");
		backend->bridge_color_ambient_light_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_ambientLightColor");
		backend->bridge_color_point_light_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_pointLightEnabled");
		backend->bridge_color_point_light_position_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_pointLightPosition");
		backend->bridge_color_point_light_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_pointLightColor");
		backend->bridge_color_point_light_distance_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_pointLightDistance");
		backend->bridge_color_point_light_decay_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_pointLightDecay");
		backend->bridge_color_light2_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_light2Enabled");
		backend->bridge_color_light_direction2_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_lightDirection2");
		backend->bridge_color_light_color2_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_lightColor2");
		backend->bridge_color_fog_density_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogDensity");
		backend->bridge_color_fog_exp2_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_fogExp2Enabled");
		backend->bridge_color_specular_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_specularEnabled");
		backend->bridge_color_specular_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_specular");
		backend->bridge_color_shininess_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_shininess");
		backend->bridge_color_emissive_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_emissive");
		backend->bridge_color_hemi_light_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_hemiLightEnabled");
		backend->bridge_color_hemi_light_direction_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_hemiLightDirection");
		backend->bridge_color_hemi_light_sky_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_hemiLightSkyColor");
		backend->bridge_color_hemi_light_ground_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_hemiLightGroundColor");
		backend->bridge_color_use_derivative_normals_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_useDerivativeNormals");
		backend->bridge_color_spot_light_enabled_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightEnabled");
		backend->bridge_color_spot_light_position_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightPosition");
		backend->bridge_color_spot_light_direction_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightDirection");
		backend->bridge_color_spot_light_color_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightColor");
		backend->bridge_color_spot_light_distance_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightDistance");
		backend->bridge_color_spot_light_cone_cos_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightConeCos");
		backend->bridge_color_spot_light_penumbra_cos_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightPenumbraCos");
		backend->bridge_color_spot_light_decay_loc = glGetUniformLocation(
			backend->bridge_color_program, "u_spotLightDecay");
	}

	if (!backend->bridge_vertex_buffer)
		glGenBuffers(1, &backend->bridge_vertex_buffer);
	if (!backend->bridge_vertex_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: position glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_triangle_color_buffer)
		glGenBuffers(1, &backend->bridge_triangle_color_buffer);
	if (!backend->bridge_triangle_color_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: triangle color glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_fog_depth_buffer)
		glGenBuffers(1, &backend->bridge_fog_depth_buffer);
	if (!backend->bridge_fog_depth_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: fog depth glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_normal_buffer)
		glGenBuffers(1, &backend->bridge_normal_buffer);
	if (!backend->bridge_normal_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: normal glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_view_position_buffer)
		glGenBuffers(1, &backend->bridge_view_position_buffer);
	if (!backend->bridge_view_position_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: view position glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	return true;
}

static bool ensure_bridge_texture_program(nx_webgl_egl_t *backend) {
	if (backend->bridge_texture_program && backend->bridge_vertex_buffer &&
		backend->bridge_fog_depth_buffer &&
		backend->bridge_normal_buffer &&
		backend->bridge_view_position_buffer)
		return true;

	// Textured triangle shader pair.
	// - Position+UV: interleaved (5 floats per vertex) in
	//   `bridge_vertex_buffer`. a_position at location 0, a_uv at location 1.
	// - Linear fog: a_fogDepth (location 2) supplied from a separate
	//   `bridge_fog_depth_buffer`. See bridge_color_program for the same
	//   pattern. Fragment shader mixes in u_fogColor when u_fogEnabled > 0.5.
	// - Single directional light: a_normal (location 3) from a separate
	//   `bridge_normal_buffer`. CPU pre-transforms by normalMatrix
	//   (inverseTranspose of modelView). Fragment shader applies Lambert
	//   diffuse + ambient when u_lightingEnabled > 0.5. Matches Three.js's
	//   `MeshPhongMaterial({map: tex, shininess: 0})` shading.
	// - Single point light: a_viewPosition (location 4) for the per-fragment
	//   light direction calc. Same pattern as bridge_color_program.
	static const char vertex_source[] =
		"attribute vec3 a_position;\n"
		"attribute vec2 a_uv;\n"
		"attribute float a_fogDepth;\n"
		"attribute vec3 a_normal;\n"
		"attribute vec3 a_viewPosition;\n"
		"uniform mat3 u_mapTransform;\n"
		"uniform float u_mapTransformEnabled;\n"
		"varying vec2 v_uv;\n"
		"varying float v_fogDepth;\n"
		"varying vec3 v_normal;\n"
		"varying vec3 v_viewPosition;\n"
		"void main() {\n"
		"  if (u_mapTransformEnabled > 0.5) {\n"
		"    v_uv = (u_mapTransform * vec3(a_uv, 1.0)).xy;\n"
		"  } else {\n"
		"    v_uv = a_uv;\n"
		"  }\n"
		"  v_fogDepth = a_fogDepth;\n"
		"  v_normal = a_normal;\n"
		"  v_viewPosition = a_viewPosition;\n"
		"  gl_Position = vec4(a_position, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		// See bridge_color_program's derivative-normals comment for the
		// full rationale. Same extension + uniform + branch pattern.
		"#extension GL_OES_standard_derivatives : enable\n"
		"precision mediump float;\n"
		"uniform sampler2D u_texture;\n"
		"uniform vec3 u_diffuse;\n"
		"uniform float u_fogEnabled;\n"
		"uniform float u_fogExp2Enabled;\n"
		"uniform vec3 u_fogColor;\n"
		"uniform float u_fogNear;\n"
		"uniform float u_fogFar;\n"
		"uniform float u_fogDensity;\n"
		"uniform float u_lightingEnabled;\n"
		"uniform vec3 u_lightDirection;\n"
		"uniform vec3 u_lightColor;\n"
		"uniform float u_light2Enabled;\n"
		"uniform vec3 u_lightDirection2;\n"
		"uniform vec3 u_lightColor2;\n"
		"uniform vec3 u_ambientLightColor;\n"
		"uniform float u_pointLightEnabled;\n"
		"uniform vec3 u_pointLightPosition;\n"
		"uniform vec3 u_pointLightColor;\n"
		"uniform float u_pointLightDistance;\n"
		"uniform float u_pointLightDecay;\n"
		"uniform float u_specularEnabled;\n"
		"uniform vec3 u_specular;\n"
		"uniform float u_shininess;\n"
		"uniform vec3 u_emissive;\n"
		"uniform float u_hemiLightEnabled;\n"
		"uniform vec3 u_hemiLightDirection;\n"
		"uniform vec3 u_hemiLightSkyColor;\n"
		"uniform vec3 u_hemiLightGroundColor;\n"
		"uniform float u_useDerivativeNormals;\n"
		// SpotLight uniforms — mirrored to the color program (see comment
		// there for the cone+attenuation math rationale).
		"uniform float u_spotLightEnabled;\n"
		"uniform vec3 u_spotLightPosition;\n"
		"uniform vec3 u_spotLightDirection;\n"
		"uniform vec3 u_spotLightColor;\n"
		"uniform float u_spotLightDistance;\n"
		"uniform float u_spotLightConeCos;\n"
		"uniform float u_spotLightPenumbraCos;\n"
		"uniform float u_spotLightDecay;\n"
		"varying vec2 v_uv;\n"
		"varying float v_fogDepth;\n"
		"varying vec3 v_normal;\n"
		"varying vec3 v_viewPosition;\n"
		"void main() {\n"
		"  vec4 base = texture2D(u_texture, v_uv);\n"
		"  base.rgb *= u_diffuse;\n"
		"  if (u_lightingEnabled > 0.5) {\n"
		"    vec3 N;\n"
		"    if (u_useDerivativeNormals > 0.5) {\n"
		"      N = normalize(cross(dFdx(v_viewPosition), dFdy(v_viewPosition)));\n"
		"    } else {\n"
		"      N = normalize(v_normal);\n"
		"    }\n"
		"    vec3 V = normalize(-v_viewPosition);\n"
		"    vec3 diffuse = u_lightColor * max(dot(N, u_lightDirection), 0.0);\n"
		"    vec3 specular = vec3(0.0);\n"
		"    if (u_specularEnabled > 0.5) {\n"
		"      vec3 H = normalize(u_lightDirection + V);\n"
		"      specular += u_lightColor * pow(max(dot(N, H), 0.0), u_shininess);\n"
		"    }\n"
		"    if (u_light2Enabled > 0.5) {\n"
		"      diffuse += u_lightColor2 * max(dot(N, u_lightDirection2), 0.0);\n"
		"      if (u_specularEnabled > 0.5) {\n"
		"        vec3 H2 = normalize(u_lightDirection2 + V);\n"
		"        specular += u_lightColor2 * pow(max(dot(N, H2), 0.0), u_shininess);\n"
		"      }\n"
		"    }\n"
		"    if (u_pointLightEnabled > 0.5) {\n"
		"      vec3 lightVec = u_pointLightPosition - v_viewPosition;\n"
		"      float distance = length(lightVec);\n"
		"      vec3 L = lightVec / max(distance, 0.0001);\n"
		"      float NdotL = max(dot(N, L), 0.0);\n"
		"      float atten = 1.0;\n"
		"      if (u_pointLightDecay > 0.0)\n"
		"        atten = 1.0 / max(pow(distance, u_pointLightDecay), 1.0);\n"
		"      if (u_pointLightDistance > 0.0)\n"
		"        atten *= max(0.0, 1.0 - distance / u_pointLightDistance);\n"
		"      diffuse += u_pointLightColor * NdotL * atten;\n"
		"      if (u_specularEnabled > 0.5) {\n"
		"        vec3 Hp = normalize(L + V);\n"
		"        specular += u_pointLightColor * pow(max(dot(N, Hp), 0.0), u_shininess) * atten;\n"
		"      }\n"
		"    }\n"
		// SpotLight — same math as bridge_color_program's spot path
		// (see comment there for the no-negation convention).
		"    if (u_spotLightEnabled > 0.5) {\n"
		"      vec3 spotVec = u_spotLightPosition - v_viewPosition;\n"
		"      float spotDist = length(spotVec);\n"
		"      vec3 Ls = spotVec / max(spotDist, 0.0001);\n"
		"      float spotAngleCos = dot(Ls, u_spotLightDirection);\n"
		"      float spotEffect = smoothstep(u_spotLightConeCos, u_spotLightPenumbraCos, spotAngleCos);\n"
		"      if (spotEffect > 0.0) {\n"
		"        float spotAtten = 1.0;\n"
		"        if (u_spotLightDecay > 0.0)\n"
		"          spotAtten = 1.0 / max(pow(spotDist, u_spotLightDecay), 1.0);\n"
		"        if (u_spotLightDistance > 0.0)\n"
		"          spotAtten *= max(0.0, 1.0 - spotDist / u_spotLightDistance);\n"
		"        float spotNdotL = max(dot(N, Ls), 0.0);\n"
		"        diffuse += u_spotLightColor * spotNdotL * spotEffect * spotAtten;\n"
		"        if (u_specularEnabled > 0.5) {\n"
		"          vec3 Hs = normalize(Ls + V);\n"
		"          specular += u_spotLightColor * pow(max(dot(N, Hs), 0.0), u_shininess) * spotEffect * spotAtten;\n"
		"        }\n"
		"      }\n"
		"    }\n"
		// HemisphereLight (Three.js) — see bridge_color_program's
		// fragment shader for the full rationale. Ambient-class irradiance
		// blended from sky/ground by the normal's projection onto the
		// hemisphere direction.
		"    vec3 hemiIrradiance = vec3(0.0);\n"
		"    if (u_hemiLightEnabled > 0.5) {\n"
		"      float hemiDotNL = dot(N, u_hemiLightDirection);\n"
		"      float hemiDiffuseWeight = 0.5 * hemiDotNL + 0.5;\n"
		"      hemiIrradiance = mix(u_hemiLightGroundColor, u_hemiLightSkyColor, hemiDiffuseWeight);\n"
		"    }\n"
		"    vec3 lit = base.rgb * (u_ambientLightColor + hemiIrradiance + diffuse) + u_specular * specular;\n"
		"    gl_FragColor = vec4(lit, base.a);\n"
		"  } else {\n"
		"    gl_FragColor = base;\n"
		"  }\n"
		// Self-illumination from `material.emissive` (Three.js MeshLambert/
		// Phong/Standard). Additive after lighting so it shows even in
		// unlit regions; default zero so non-emissive materials unaffected.
		// Applied BEFORE fog so emissive materials still fade with distance.
		"  gl_FragColor.rgb += u_emissive;\n"
		"  if (u_fogEnabled > 0.5) {\n"
		"    float f;\n"
		"    if (u_fogExp2Enabled > 0.5) {\n"
		"      f = 1.0 - exp(-u_fogDensity * u_fogDensity * v_fogDepth * v_fogDepth);\n"
		"    } else {\n"
		"      f = smoothstep(u_fogNear, u_fogFar, v_fogDepth);\n"
		"    }\n"
		"    gl_FragColor.rgb = mix(gl_FragColor.rgb, u_fogColor, clamp(f, 0.0, 1.0));\n"
		"  }\n"
		"}\n";

	backend->bridge_texture_vertex_shader =
		compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
								backend->status, sizeof(backend->status));
	if (!backend->bridge_texture_vertex_shader)
		return false;
	backend->bridge_texture_fragment_shader =
		compile_triangle_shader(GL_FRAGMENT_SHADER, fragment_source,
								backend->status, sizeof(backend->status));
	if (!backend->bridge_texture_fragment_shader)
		return false;

	backend->bridge_texture_program = glCreateProgram();
	if (!backend->bridge_texture_program) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: glCreateProgram() failed: 0x%x",
				 glGetError());
		return false;
	}
	glAttachShader(backend->bridge_texture_program,
				   backend->bridge_texture_vertex_shader);
	glAttachShader(backend->bridge_texture_program,
				   backend->bridge_texture_fragment_shader);
	glBindAttribLocation(backend->bridge_texture_program, 0, "a_position");
	glBindAttribLocation(backend->bridge_texture_program, 1, "a_uv");
	glBindAttribLocation(backend->bridge_texture_program, 2, "a_fogDepth");
	glBindAttribLocation(backend->bridge_texture_program, 3, "a_normal");
	glBindAttribLocation(backend->bridge_texture_program, 4, "a_viewPosition");
	glLinkProgram(backend->bridge_texture_program);

	GLint linked = GL_FALSE;
	glGetProgramiv(backend->bridge_texture_program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLchar log[128];
		GLsizei log_length = 0;
		glGetProgramInfoLog(backend->bridge_texture_program, sizeof(log),
							&log_length, log);
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: glLinkProgram() failed: %.*s",
				 (int)log_length, log);
		return false;
	}
	backend->bridge_texture_sampler_loc =
		glGetUniformLocation(backend->bridge_texture_program, "u_texture");
	backend->bridge_texture_fog_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogEnabled");
	backend->bridge_texture_fog_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogColor");
	backend->bridge_texture_fog_near_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogNear");
	backend->bridge_texture_fog_far_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogFar");
	backend->bridge_texture_lighting_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_lightingEnabled");
	backend->bridge_texture_light_direction_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_lightDirection");
	backend->bridge_texture_light_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_lightColor");
	backend->bridge_texture_ambient_light_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_ambientLightColor");
	backend->bridge_texture_point_light_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_pointLightEnabled");
	backend->bridge_texture_point_light_position_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_pointLightPosition");
	backend->bridge_texture_point_light_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_pointLightColor");
	backend->bridge_texture_point_light_distance_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_pointLightDistance");
	backend->bridge_texture_point_light_decay_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_pointLightDecay");
	backend->bridge_texture_light2_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_light2Enabled");
	backend->bridge_texture_light_direction2_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_lightDirection2");
	backend->bridge_texture_light_color2_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_lightColor2");
	backend->bridge_texture_fog_density_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogDensity");
	backend->bridge_texture_fog_exp2_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_fogExp2Enabled");
	backend->bridge_texture_map_transform_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_mapTransform");
	backend->bridge_texture_map_transform_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_mapTransformEnabled");
	backend->bridge_texture_diffuse_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_diffuse");
	backend->bridge_texture_specular_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_specularEnabled");
	backend->bridge_texture_specular_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_specular");
	backend->bridge_texture_shininess_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_shininess");
	backend->bridge_texture_emissive_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_emissive");
	backend->bridge_texture_hemi_light_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_hemiLightEnabled");
	backend->bridge_texture_hemi_light_direction_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_hemiLightDirection");
	backend->bridge_texture_hemi_light_sky_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_hemiLightSkyColor");
	backend->bridge_texture_hemi_light_ground_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_hemiLightGroundColor");
	backend->bridge_texture_use_derivative_normals_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_useDerivativeNormals");
	backend->bridge_texture_spot_light_enabled_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightEnabled");
	backend->bridge_texture_spot_light_position_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightPosition");
	backend->bridge_texture_spot_light_direction_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightDirection");
	backend->bridge_texture_spot_light_color_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightColor");
	backend->bridge_texture_spot_light_distance_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightDistance");
	backend->bridge_texture_spot_light_cone_cos_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightConeCos");
	backend->bridge_texture_spot_light_penumbra_cos_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightPenumbraCos");
	backend->bridge_texture_spot_light_decay_loc = glGetUniformLocation(
		backend->bridge_texture_program, "u_spotLightDecay");

	if (!backend->bridge_vertex_buffer)
		glGenBuffers(1, &backend->bridge_vertex_buffer);
	if (!backend->bridge_vertex_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_fog_depth_buffer)
		glGenBuffers(1, &backend->bridge_fog_depth_buffer);
	if (!backend->bridge_fog_depth_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: fog depth glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_normal_buffer)
		glGenBuffers(1, &backend->bridge_normal_buffer);
	if (!backend->bridge_normal_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: normal glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_view_position_buffer)
		glGenBuffers(1, &backend->bridge_view_position_buffer);
	if (!backend->bridge_view_position_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: view position glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	return true;
}

static bool ensure_bridge_line_program(nx_webgl_egl_t *backend) {
	if (backend->bridge_line_program && backend->bridge_vertex_buffer &&
		backend->bridge_line_distance_buffer &&
		backend->bridge_line_color_buffer &&
		backend->bridge_fog_depth_buffer)
		return true;

	// Dashed/colored-line shader pair.
	// - Position: a_position (vec3 NDC at location 0). CPU did the MVP +
	//   perspective divide. Passing z through is required for wireframe
	//   overlays so back-facing edges depth-test out against front faces.
	// - LineDashedMaterial support: a_lineDistance (location 1), u_scale,
	//   u_dashSize, u_totalSize. u_totalSize == 0 means solid (no dash mask).
	// - Per-vertex color support (LineBasicMaterial vertexColors:true):
	//   a_color (location 2), output = u_color * vec4(v_color, 1.0). Callers
	//   that do not bind a color buffer use glVertexAttrib3f(2, 1, 1, 1) so
	//   v_color defaults to white and the output collapses to u_color.
	// - Linear fog: a_fogDepth (location 3). Fragment shader mixes in
	//   u_fogColor when u_fogEnabled > 0.5. See ensure_bridge_color_program
	//   for the same pattern.
	static const char vertex_source[] =
		"attribute vec3 a_position;\n"
		"attribute float a_lineDistance;\n"
		"attribute vec3 a_color;\n"
		"attribute float a_fogDepth;\n"
		"uniform float u_scale;\n"
		"varying float v_lineDistance;\n"
		"varying vec3 v_color;\n"
		"varying float v_fogDepth;\n"
		"void main() {\n"
		"  v_lineDistance = u_scale * a_lineDistance;\n"
		"  v_color = a_color;\n"
		"  v_fogDepth = a_fogDepth;\n"
		"  gl_Position = vec4(a_position, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"uniform vec4 u_color;\n"
		"uniform float u_dashSize;\n"
		"uniform float u_totalSize;\n"
		"uniform float u_fogEnabled;\n"
		"uniform vec3 u_fogColor;\n"
		"uniform float u_fogNear;\n"
		"uniform float u_fogFar;\n"
		"varying float v_lineDistance;\n"
		"varying vec3 v_color;\n"
		"varying float v_fogDepth;\n"
		"void main() {\n"
		"  if (u_totalSize > 0.0) {\n"
		"    if (mod(v_lineDistance, u_totalSize) > u_dashSize) discard;\n"
		"  }\n"
		"  gl_FragColor = u_color * vec4(v_color, 1.0);\n"
		"  if (u_fogEnabled > 0.5) {\n"
		"    float f = smoothstep(u_fogNear, u_fogFar, v_fogDepth);\n"
		"    gl_FragColor.rgb = mix(gl_FragColor.rgb, u_fogColor, f);\n"
		"  }\n"
		"}\n";

	if (!backend->bridge_line_vertex_shader)
		backend->bridge_line_vertex_shader =
			compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
									backend->status, sizeof(backend->status));
	if (!backend->bridge_line_vertex_shader)
		return false;
	if (!backend->bridge_line_fragment_shader)
		backend->bridge_line_fragment_shader =
			compile_triangle_shader(GL_FRAGMENT_SHADER, fragment_source,
									backend->status, sizeof(backend->status));
	if (!backend->bridge_line_fragment_shader)
		return false;

	if (!backend->bridge_line_program) {
		backend->bridge_line_program = glCreateProgram();
		if (!backend->bridge_line_program) {
			snprintf(backend->status, sizeof(backend->status),
					 "GPU bridge line draw: glCreateProgram() failed: 0x%x",
					 glGetError());
			return false;
		}
		glAttachShader(backend->bridge_line_program,
					   backend->bridge_line_vertex_shader);
		glAttachShader(backend->bridge_line_program,
					   backend->bridge_line_fragment_shader);
		glBindAttribLocation(backend->bridge_line_program, 0, "a_position");
		glBindAttribLocation(backend->bridge_line_program, 1,
							 "a_lineDistance");
		glBindAttribLocation(backend->bridge_line_program, 2, "a_color");
		glBindAttribLocation(backend->bridge_line_program, 3, "a_fogDepth");
		glLinkProgram(backend->bridge_line_program);

		GLint linked = GL_FALSE;
		glGetProgramiv(backend->bridge_line_program, GL_LINK_STATUS, &linked);
		if (!linked) {
			GLchar log[128];
			GLsizei log_length = 0;
			glGetProgramInfoLog(backend->bridge_line_program, sizeof(log),
								&log_length, log);
			snprintf(backend->status, sizeof(backend->status),
					 "GPU bridge line draw: glLinkProgram() failed: %.*s",
					 (int)log_length, log);
			return false;
		}
		backend->bridge_line_color_loc =
			glGetUniformLocation(backend->bridge_line_program, "u_color");
		backend->bridge_line_scale_loc =
			glGetUniformLocation(backend->bridge_line_program, "u_scale");
		backend->bridge_line_dash_loc =
			glGetUniformLocation(backend->bridge_line_program, "u_dashSize");
		backend->bridge_line_total_loc =
			glGetUniformLocation(backend->bridge_line_program, "u_totalSize");
		backend->bridge_line_fog_enabled_loc = glGetUniformLocation(
			backend->bridge_line_program, "u_fogEnabled");
		backend->bridge_line_fog_color_loc = glGetUniformLocation(
			backend->bridge_line_program, "u_fogColor");
		backend->bridge_line_fog_near_loc = glGetUniformLocation(
			backend->bridge_line_program, "u_fogNear");
		backend->bridge_line_fog_far_loc = glGetUniformLocation(
			backend->bridge_line_program, "u_fogFar");
	}

	if (!backend->bridge_vertex_buffer)
		glGenBuffers(1, &backend->bridge_vertex_buffer);
	if (!backend->bridge_vertex_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw: position glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_line_distance_buffer)
		glGenBuffers(1, &backend->bridge_line_distance_buffer);
	if (!backend->bridge_line_distance_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw: distance glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_line_color_buffer)
		glGenBuffers(1, &backend->bridge_line_color_buffer);
	if (!backend->bridge_line_color_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw: color glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	if (!backend->bridge_fog_depth_buffer)
		glGenBuffers(1, &backend->bridge_fog_depth_buffer);
	if (!backend->bridge_fog_depth_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw: fog depth glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	return true;
}

// Used by the bridge's per-draw texture cache (one level only). Mipmap
// minFilter variants collapse to NEAREST because the cache uploads level
// 0 only — sampling with LINEAR_MIPMAP_LINEAR on a non-mipmap-complete
// texture is undefined.
static GLenum bridge_texture_filter(uint32_t filter) {
	return filter == GL_LINEAR ? GL_LINEAR : GL_NEAREST;
}

// Used by the persistent-handle path. ALL minFilter variants flow
// through — when paired with `glGenerateMipmap` ([[bridge-raw-shader-passthrough]]
// helper added 2026-05-22 for milestone #24), mipmap sampling actually
// works. magFilter is still NEAREST/LINEAR only (GLES 2.0 spec).
#define GL_NEAREST_MIPMAP_NEAREST_LOCAL 0x2700
#define GL_LINEAR_MIPMAP_NEAREST_LOCAL 0x2701
#define GL_NEAREST_MIPMAP_LINEAR_LOCAL 0x2702
#define GL_LINEAR_MIPMAP_LINEAR_LOCAL 0x2703
static GLenum bridge_texture_filter_persistent(uint32_t filter) {
	switch (filter) {
	case GL_LINEAR:
	case GL_NEAREST_MIPMAP_NEAREST_LOCAL:
	case GL_LINEAR_MIPMAP_NEAREST_LOCAL:
	case GL_NEAREST_MIPMAP_LINEAR_LOCAL:
	case GL_LINEAR_MIPMAP_LINEAR_LOCAL:
		return (GLenum)filter;
	default:
		return GL_NEAREST;
	}
}

static GLenum bridge_texture_wrap(uint32_t wrap) {
	return wrap == GL_REPEAT ? GL_REPEAT : GL_CLAMP_TO_EDGE;
}

static GLuint ensure_bridge_cached_texture(nx_webgl_egl_t *backend,
										   uint32_t texture_id,
										   uint32_t revision,
										   int width,
										   int height,
										   const uint8_t *rgba,
										   uint32_t min_filter,
										   uint32_t mag_filter,
										   uint32_t wrap_s,
										   uint32_t wrap_t) {
	if (!backend || texture_id == 0 || revision == 0 || width <= 0 ||
		height <= 0 || !rgba)
		return 0;

	nx_webgl_egl_texture_cache_entry_t *slot = NULL;
	for (int i = 0; i < NX_WEBGL_EGL_TEXTURE_CACHE_SIZE; i++) {
		nx_webgl_egl_texture_cache_entry_t *entry = &backend->texture_cache[i];
		if (entry->texture_id == texture_id) {
			slot = entry;
			break;
		}
		if (!slot && entry->texture_id == 0)
			slot = entry;
	}
	if (!slot)
		slot = &backend->texture_cache[texture_id %
									   NX_WEBGL_EGL_TEXTURE_CACHE_SIZE];
	if (slot->texture_id != texture_id && slot->handle) {
		glDeleteTextures(1, &slot->handle);
		memset(slot, 0, sizeof(*slot));
	}
	if (!slot->handle)
		glGenTextures(1, &slot->handle);
	if (!slot->handle)
		return 0;

	glBindTexture(GL_TEXTURE_2D, slot->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					bridge_texture_filter(min_filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					bridge_texture_filter(mag_filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
					bridge_texture_wrap(wrap_s));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
					bridge_texture_wrap(wrap_t));

	// Clear any stale GL error left over from earlier bridge calls so
	// the post-glTexImage2D check below only reflects this upload —
	// otherwise an unrelated prior error gets misattributed to texture
	// upload (see [[bridge-stale-glerror]]).
	(void)glGetError();
	if (slot->texture_id != texture_id || slot->revision != revision ||
		slot->width != width || slot->height != height) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
					 GL_UNSIGNED_BYTE, rgba);
	} else if (slot->min_filter != min_filter || slot->mag_filter != mag_filter ||
			   slot->wrap_s != wrap_s || slot->wrap_t != wrap_t) {
		// Sampler state above is enough; texture pixels are already current.
	}

	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture upload failed: 0x%x", error);
		return 0;
	}

	slot->texture_id = texture_id;
	slot->revision = revision;
	slot->width = width;
	slot->height = height;
	slot->min_filter = min_filter;
	slot->mag_filter = mag_filter;
	slot->wrap_s = wrap_s;
	slot->wrap_t = wrap_t;
	return slot->handle;
}
#endif

nx_webgl_egl_t *nx_webgl_egl_create(JSContext *ctx, nx_canvas_t *canvas) {
	nx_webgl_egl_t *backend = js_mallocz(ctx, sizeof(nx_webgl_egl_t));
	if (!backend)
		return NULL;

	backend->clear_color[3] = 1.;

#if !NXJS_HAS_EGL_GLES
	backend->built = false;
	backend->available = false;
	snprintf(backend->status, sizeof(backend->status),
			 "EGL/OpenGL ES support was not built. Install switch EGL/GLESv2 "
			 "headers/libs and rebuild with NXJS_HAS_EGL_GLES=1.");
	(void)canvas;
	return backend;
#else
	backend->built = true;
	backend->display = EGL_NO_DISPLAY;
	backend->context = EGL_NO_CONTEXT;
	backend->surface = EGL_NO_SURFACE;
	snprintf(backend->status, sizeof(backend->status),
			 "EGL/OpenGL ES support was built; prototype not initialized.");
	(void)canvas;
	return backend;
#endif
}

static bool nx_webgl_egl_initialize(nx_webgl_egl_t *backend,
									nx_canvas_t *canvas) {
	if (!backend)
		return false;
	if (backend->available)
		return true;
	while (!backend->available) {
		int previous_step = backend->step;
		if (!nx_webgl_egl_probe_step(backend, canvas))
			return false;
		if (backend->step == previous_step)
			return false;
	}
	return true;
}

bool nx_webgl_egl_probe_step(nx_webgl_egl_t *backend, nx_canvas_t *canvas) {
	if (!backend)
		return false;
#if !NXJS_HAS_EGL_GLES
	(void)canvas;
	snprintf(backend->status, sizeof(backend->status),
			 "EGL/OpenGL ES support was not built.");
	return false;
#else
	// 4x MSAA preferred config — granted by Nouveau when available, makes
	// gl.getContextAttributes().antialias return true. Falls back to no-MSAA
	// RGBA8 if eglChooseConfig denies it (e.g. driver doesn't support
	// multisampled default FBOs on the surfaceless path).
	EGLint rgba8_msaa_attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SAMPLE_BUFFERS, 1,
		EGL_SAMPLES, 4,
		EGL_NONE,
	};
	EGLint rgba8_attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_NONE,
	};
	EGLint any_es2_attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};
	// Try ES 3 first (so glDrawArraysInstanced / glVertexAttribDivisor are
	// real ES3 core functions). Fall back to ES 2 if ES 3 context creation
	// fails. Mesa+Citron will give us ES 3.2 unconditionally; Tegra hardware
	// supports ES 3+. The previous "request ES 2" path was loading the ES3
	// instancing functions via eglGetProcAddress but Mesa returns no-op
	// stubs for ES3 entry points on an ES2-requested context — that's why
	// `bridge dbg = P+6x4` confirms instanced dispatch ran but only
	// instance 0 was visible in the milestone-#15 conformance test.
	EGLint context_attrs_es3[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE,
	};
	EGLint context_attrs_es2[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};

	if (backend->available) {
		snprintf(backend->status, sizeof(backend->status),
				 "EGL/OpenGL ES context already initialized");
		return true;
	}

	switch (backend->step) {
	case 0:
		snprintf(backend->status, sizeof(backend->status),
				 "step 1: calling eglGetDisplay()");
	backend->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (backend->display == EGL_NO_DISPLAY) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglGetDisplay() failed: 0x%x", eglGetError());
		return false;
	}
		backend->step = 1;
		snprintf(backend->status, sizeof(backend->status),
				 "step 1 ok: eglGetDisplay()");
		return true;

	case 1:
		snprintf(backend->status, sizeof(backend->status),
				 "step 2: calling eglInitialize()");
	if (!eglInitialize(backend->display, &backend->major, &backend->minor)) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglInitialize() failed: 0x%x", eglGetError());
		return false;
	}
		backend->step = 2;
		snprintf(backend->status, sizeof(backend->status),
				 "step 2 ok: eglInitialize() -> %d.%d", backend->major,
				 backend->minor);
		return true;

	case 2:
		snprintf(backend->status, sizeof(backend->status),
				 "step 3: calling eglBindAPI(EGL_OPENGL_ES_API)");
		if (!eglBindAPI(EGL_OPENGL_ES_API)) {
			snprintf(backend->status, sizeof(backend->status),
					 "eglBindAPI(EGL_OPENGL_ES_API) failed: 0x%x",
					 eglGetError());
			return false;
		}
		backend->step = 3;
		snprintf(backend->status, sizeof(backend->status),
				 "step 3 ok: eglBindAPI(EGL_OPENGL_ES_API)");
		return true;

	case 3:
		snprintf(backend->status, sizeof(backend->status),
				 "step 4: calling eglChooseConfig(surfaceless)");
	EGLint config_count = 0;
	const char *config_profile = "RGBA8 ES2 4xMSAA";
	bool config_ok = eglChooseConfig(backend->display, rgba8_msaa_attrs,
									 &backend->config, 1, &config_count);
	EGLint config_error = eglGetError();
	if (config_ok && config_count > 0) {
		backend->egl_msaa_enabled = true;
	} else {
		backend->egl_msaa_enabled = false;
		config_count = 0;
		config_profile = "RGBA8 ES2";
		config_ok = eglChooseConfig(backend->display, rgba8_attrs,
									&backend->config, 1, &config_count);
		config_error = eglGetError();
	}
	if (!config_ok || config_count == 0) {
		config_count = 0;
		config_profile = "any ES2";
		config_ok = eglChooseConfig(backend->display, any_es2_attrs,
									&backend->config, 1, &config_count);
		config_error = eglGetError();
	}
	if (!config_ok || config_count == 0) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglChooseConfig(surfaceless) failed: 0x%x, count=%d",
				 config_error, config_count);
		return false;
	}
		backend->step = 4;
		snprintf(backend->status, sizeof(backend->status),
				 "step 4 ok: eglChooseConfig(%s)", config_profile);
		return true;

	case 4:
		snprintf(backend->status, sizeof(backend->status),
				 "step 5: skipping EGL surface for surfaceless probe");
		backend->step = 5;
		snprintf(backend->status, sizeof(backend->status),
				 "step 5 ok: using EGL_NO_SURFACE path");
		return true;

	case 5:
		snprintf(backend->status, sizeof(backend->status),
				 "step 6: no EGL surface created");
		backend->surface = EGL_NO_SURFACE;
		backend->step = 6;
		snprintf(backend->status, sizeof(backend->status),
				 "step 6 ok: surface is EGL_NO_SURFACE");
		return true;

	case 6:
		snprintf(backend->status, sizeof(backend->status),
				 "step 7: calling eglCreateContext() ES3");
	backend->context = eglCreateContext(backend->display, backend->config,
										EGL_NO_CONTEXT, context_attrs_es3);
	if (backend->context == EGL_NO_CONTEXT) {
		// ES 3 not available — fall back to ES 2 (instancing won't work
		// natively, but at least basic rendering will).
		backend->context = eglCreateContext(backend->display, backend->config,
											EGL_NO_CONTEXT, context_attrs_es2);
	}
	if (backend->context == EGL_NO_CONTEXT) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglCreateContext() failed: 0x%x", eglGetError());
		return false;
	}
		backend->step = 7;
		snprintf(backend->status, sizeof(backend->status),
				 "step 7 ok: eglCreateContext()");
		return true;

	case 7:
		snprintf(backend->status, sizeof(backend->status),
				 "step 8: calling eglMakeCurrent(surfaceless)");
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglMakeCurrent(surfaceless) failed: 0x%x", eglGetError());
		return false;
	}
		backend->step = 8;
		snprintf(backend->status, sizeof(backend->status),
				 "step 8 ok: eglMakeCurrent(surfaceless)");
		return true;

	case 8:
		snprintf(backend->status, sizeof(backend->status),
				 "step 9: calling glGetString()");
	backend->vendor = (const char *)glGetString(GL_VENDOR);
	backend->version = (const char *)glGetString(GL_VERSION);
	backend->renderer = (const char *)glGetString(GL_RENDERER);

	// Milestone #15 probe: presence of GL_EXT_instanced_arrays and its
	// three entry points. Done here (once, at init) so getBackendInfo()
	// always reports a final answer. Token check looks for both
	// "GL_EXT_instanced_arrays" and the ANGLE-style alias the runtime
	// might forward; entry-point load uses eglGetProcAddress and falls
	// back through the core GLES3 names in case the driver exposes them
	// unsuffixed.
	{
		const GLubyte *exts = glGetString(GL_EXTENSIONS);
		if (exts) {
			snprintf(backend->gl_extensions, sizeof(backend->gl_extensions),
					 "%s", (const char *)exts);
			backend->ext_instanced_arrays_present =
				(strstr((const char *)exts, "GL_EXT_instanced_arrays") != NULL) ||
				(strstr((const char *)exts, "GL_ANGLE_instanced_arrays") != NULL) ||
				(strstr((const char *)exts, "GL_NV_draw_instanced") != NULL);
		} else {
			backend->gl_extensions[0] = '\0';
			backend->ext_instanced_arrays_present = false;
		}
		// Preference order: core GLES3 unsuffixed name first (since GL_VERSION
		// reports ES 3.2 on Mesa+Citron and Tegra is also GLES3-class), then
		// extension-suffixed fallbacks. Mesa returns NULL for the EXT variant
		// when EXT_instanced_arrays isn't advertised — that's correct — but
		// some other drivers (and at least one Tegra revision) return a
		// non-NULL stub that silently no-ops. Loading the core name first
		// avoids that whole class of bug.
		backend->fn_vertex_attrib_divisor_ext =
			(void *)eglGetProcAddress("glVertexAttribDivisor");
		if (!backend->fn_vertex_attrib_divisor_ext)
			backend->fn_vertex_attrib_divisor_ext =
				(void *)eglGetProcAddress("glVertexAttribDivisorEXT");
		if (!backend->fn_vertex_attrib_divisor_ext)
			backend->fn_vertex_attrib_divisor_ext =
				(void *)eglGetProcAddress("glVertexAttribDivisorANGLE");
		backend->fn_draw_arrays_instanced_ext =
			(void *)eglGetProcAddress("glDrawArraysInstanced");
		if (!backend->fn_draw_arrays_instanced_ext)
			backend->fn_draw_arrays_instanced_ext =
				(void *)eglGetProcAddress("glDrawArraysInstancedEXT");
		if (!backend->fn_draw_arrays_instanced_ext)
			backend->fn_draw_arrays_instanced_ext =
				(void *)eglGetProcAddress("glDrawArraysInstancedANGLE");
		if (!backend->fn_draw_arrays_instanced_ext)
			backend->fn_draw_arrays_instanced_ext =
				(void *)eglGetProcAddress("glDrawArraysInstancedNV");
		backend->fn_draw_elements_instanced_ext =
			(void *)eglGetProcAddress("glDrawElementsInstanced");
		if (!backend->fn_draw_elements_instanced_ext)
			backend->fn_draw_elements_instanced_ext =
				(void *)eglGetProcAddress("glDrawElementsInstancedEXT");
		if (!backend->fn_draw_elements_instanced_ext)
			backend->fn_draw_elements_instanced_ext =
				(void *)eglGetProcAddress("glDrawElementsInstancedANGLE");
		if (!backend->fn_draw_elements_instanced_ext)
			backend->fn_draw_elements_instanced_ext =
				(void *)eglGetProcAddress("glDrawElementsInstancedNV");
		// VAO functions for ES 3+ — required as soon as instancing is in
		// play because the passthrough dispatch needs SOMEWHERE to bind
		// the per-attrib divisor state. OES_vertex_array_object suffixes
		// for ES 2 compatibility.
		backend->fn_gen_vertex_arrays =
			(void *)eglGetProcAddress("glGenVertexArrays");
		if (!backend->fn_gen_vertex_arrays)
			backend->fn_gen_vertex_arrays =
				(void *)eglGetProcAddress("glGenVertexArraysOES");
		backend->fn_bind_vertex_array =
			(void *)eglGetProcAddress("glBindVertexArray");
		if (!backend->fn_bind_vertex_array)
			backend->fn_bind_vertex_array =
				(void *)eglGetProcAddress("glBindVertexArrayOES");
		backend->fn_delete_vertex_arrays =
			(void *)eglGetProcAddress("glDeleteVertexArrays");
		if (!backend->fn_delete_vertex_arrays)
			backend->fn_delete_vertex_arrays =
				(void *)eglGetProcAddress("glDeleteVertexArraysOES");
		// Generate the persistent passthrough VAO immediately, while the
		// context is current. All subsequent draws bind it before
		// touching vertex attribute state.
		if (backend->fn_gen_vertex_arrays && backend->fn_bind_vertex_array) {
			typedef void (*pfn_gen_vao_t)(GLsizei, GLuint *);
			pfn_gen_vao_t gen = (pfn_gen_vao_t)backend->fn_gen_vertex_arrays;
			GLuint vao = 0;
			gen(1, &vao);
			backend->passthrough_vao = vao;
		}

		// WebGL 2 / GLES 3.0 entry points. Resolved unconditionally; the
		// presence flag is true iff the core set we need for Three.js
		// (uint uniforms + integer vertex attribs + drawBuffers +
		// texImage3D + texStorage2D + bindBufferBase) all loaded.
		backend->fn_vertex_attrib_i_pointer =
			(void *)eglGetProcAddress("glVertexAttribIPointer");
		backend->fn_vertex_attrib_i4i =
			(void *)eglGetProcAddress("glVertexAttribI4i");
		backend->fn_vertex_attrib_i4ui =
			(void *)eglGetProcAddress("glVertexAttribI4ui");
		backend->fn_uniform1ui = (void *)eglGetProcAddress("glUniform1ui");
		backend->fn_uniform2ui = (void *)eglGetProcAddress("glUniform2ui");
		backend->fn_uniform3ui = (void *)eglGetProcAddress("glUniform3ui");
		backend->fn_uniform4ui = (void *)eglGetProcAddress("glUniform4ui");
		backend->fn_uniform1uiv = (void *)eglGetProcAddress("glUniform1uiv");
		backend->fn_uniform2uiv = (void *)eglGetProcAddress("glUniform2uiv");
		backend->fn_uniform3uiv = (void *)eglGetProcAddress("glUniform3uiv");
		backend->fn_uniform4uiv = (void *)eglGetProcAddress("glUniform4uiv");
		backend->fn_uniform_matrix2x3fv =
			(void *)eglGetProcAddress("glUniformMatrix2x3fv");
		backend->fn_uniform_matrix3x2fv =
			(void *)eglGetProcAddress("glUniformMatrix3x2fv");
		backend->fn_uniform_matrix2x4fv =
			(void *)eglGetProcAddress("glUniformMatrix2x4fv");
		backend->fn_uniform_matrix4x2fv =
			(void *)eglGetProcAddress("glUniformMatrix4x2fv");
		backend->fn_uniform_matrix3x4fv =
			(void *)eglGetProcAddress("glUniformMatrix3x4fv");
		backend->fn_uniform_matrix4x3fv =
			(void *)eglGetProcAddress("glUniformMatrix4x3fv");
		backend->fn_draw_buffers = (void *)eglGetProcAddress("glDrawBuffers");
		backend->fn_invalidate_framebuffer =
			(void *)eglGetProcAddress("glInvalidateFramebuffer");
		backend->fn_invalidate_sub_framebuffer =
			(void *)eglGetProcAddress("glInvalidateSubFramebuffer");
		backend->fn_blit_framebuffer =
			(void *)eglGetProcAddress("glBlitFramebuffer");
		backend->fn_read_buffer = (void *)eglGetProcAddress("glReadBuffer");
		backend->fn_renderbuffer_storage_multisample =
			(void *)eglGetProcAddress("glRenderbufferStorageMultisample");
		backend->fn_framebuffer_texture_layer =
			(void *)eglGetProcAddress("glFramebufferTextureLayer");
		backend->fn_tex_image_3d = (void *)eglGetProcAddress("glTexImage3D");
		backend->fn_tex_sub_image_3d =
			(void *)eglGetProcAddress("glTexSubImage3D");
		backend->fn_copy_tex_sub_image_3d =
			(void *)eglGetProcAddress("glCopyTexSubImage3D");
		backend->fn_compressed_tex_image_3d =
			(void *)eglGetProcAddress("glCompressedTexImage3D");
		backend->fn_compressed_tex_sub_image_3d =
			(void *)eglGetProcAddress("glCompressedTexSubImage3D");
		backend->fn_tex_storage_2d =
			(void *)eglGetProcAddress("glTexStorage2D");
		backend->fn_tex_storage_3d =
			(void *)eglGetProcAddress("glTexStorage3D");
		backend->fn_clear_buffer_iv =
			(void *)eglGetProcAddress("glClearBufferiv");
		backend->fn_clear_buffer_uiv =
			(void *)eglGetProcAddress("glClearBufferuiv");
		backend->fn_clear_buffer_fv =
			(void *)eglGetProcAddress("glClearBufferfv");
		backend->fn_clear_buffer_fi =
			(void *)eglGetProcAddress("glClearBufferfi");
		backend->fn_copy_buffer_sub_data =
			(void *)eglGetProcAddress("glCopyBufferSubData");
		backend->fn_get_buffer_sub_data =
			(void *)eglGetProcAddress("glGetBufferSubData");
		backend->fn_bind_buffer_base =
			(void *)eglGetProcAddress("glBindBufferBase");
		backend->fn_bind_buffer_range =
			(void *)eglGetProcAddress("glBindBufferRange");
		backend->fn_get_uniform_block_index =
			(void *)eglGetProcAddress("glGetUniformBlockIndex");
		backend->fn_uniform_block_binding =
			(void *)eglGetProcAddress("glUniformBlockBinding");
		backend->fn_get_active_uniform_block_iv =
			(void *)eglGetProcAddress("glGetActiveUniformBlockiv");
		backend->fn_get_active_uniform_block_name =
			(void *)eglGetProcAddress("glGetActiveUniformBlockName");
		backend->fn_get_active_uniforms_iv =
			(void *)eglGetProcAddress("glGetActiveUniformsiv");
		backend->fn_get_uniform_indices =
			(void *)eglGetProcAddress("glGetUniformIndices");
		backend->fn_gen_samplers = (void *)eglGetProcAddress("glGenSamplers");
		backend->fn_delete_samplers =
			(void *)eglGetProcAddress("glDeleteSamplers");
		backend->fn_bind_sampler = (void *)eglGetProcAddress("glBindSampler");
		backend->fn_sampler_parameteri =
			(void *)eglGetProcAddress("glSamplerParameteri");
		backend->fn_sampler_parameterf =
			(void *)eglGetProcAddress("glSamplerParameterf");
		backend->fn_get_sampler_parameter_iv =
			(void *)eglGetProcAddress("glGetSamplerParameteriv");
		backend->fn_fence_sync = (void *)eglGetProcAddress("glFenceSync");
		backend->fn_delete_sync = (void *)eglGetProcAddress("glDeleteSync");
		backend->fn_client_wait_sync =
			(void *)eglGetProcAddress("glClientWaitSync");
		backend->fn_wait_sync = (void *)eglGetProcAddress("glWaitSync");
		backend->fn_get_sync_iv = (void *)eglGetProcAddress("glGetSynciv");
		backend->fn_gen_queries = (void *)eglGetProcAddress("glGenQueries");
		backend->fn_delete_queries =
			(void *)eglGetProcAddress("glDeleteQueries");
		backend->fn_begin_query = (void *)eglGetProcAddress("glBeginQuery");
		backend->fn_end_query = (void *)eglGetProcAddress("glEndQuery");
		backend->fn_get_query_iv = (void *)eglGetProcAddress("glGetQueryiv");
		backend->fn_get_query_object_uiv =
			(void *)eglGetProcAddress("glGetQueryObjectuiv");
		backend->fn_gen_transform_feedbacks =
			(void *)eglGetProcAddress("glGenTransformFeedbacks");
		backend->fn_delete_transform_feedbacks =
			(void *)eglGetProcAddress("glDeleteTransformFeedbacks");
		backend->fn_bind_transform_feedback =
			(void *)eglGetProcAddress("glBindTransformFeedback");
		backend->fn_begin_transform_feedback =
			(void *)eglGetProcAddress("glBeginTransformFeedback");
		backend->fn_end_transform_feedback =
			(void *)eglGetProcAddress("glEndTransformFeedback");
		backend->fn_pause_transform_feedback =
			(void *)eglGetProcAddress("glPauseTransformFeedback");
		backend->fn_resume_transform_feedback =
			(void *)eglGetProcAddress("glResumeTransformFeedback");
		backend->fn_transform_feedback_varyings =
			(void *)eglGetProcAddress("glTransformFeedbackVaryings");
		backend->fn_get_transform_feedback_varying =
			(void *)eglGetProcAddress("glGetTransformFeedbackVarying");
		backend->fn_get_frag_data_location =
			(void *)eglGetProcAddress("glGetFragDataLocation");
		backend->fn_get_internal_format_iv =
			(void *)eglGetProcAddress("glGetInternalformativ");
		backend->webgl2_present =
			backend->fn_vertex_attrib_i_pointer && backend->fn_uniform1ui &&
			backend->fn_draw_buffers && backend->fn_tex_image_3d &&
			backend->fn_tex_storage_2d && backend->fn_bind_buffer_base;

		// 2026-06-24 extension audit wave 1 — probe per-extension presence
		// in the gl_extensions string captured above, and resolve the
		// entry points each one adds. NULL function pointers + false flag
		// = silently skip the advertise in nx_webgl_get_supported_extensions.
		// Token-presence convention: substring match on the GL_* name.
		{
			const char *exts = backend->gl_extensions;
			#define EXT_HAS(name) (exts && strstr(exts, name) != NULL)

			backend->has_anisotropic = EXT_HAS("GL_EXT_texture_filter_anisotropic");
			backend->has_clip_control = EXT_HAS("GL_EXT_clip_control");
			backend->has_depth_clamp = EXT_HAS("GL_EXT_depth_clamp");
			backend->has_polygon_offset_clamp = EXT_HAS("GL_EXT_polygon_offset_clamp");
			backend->has_texture_compression_bptc = EXT_HAS("GL_EXT_texture_compression_bptc");
			backend->has_texture_compression_rgtc = EXT_HAS("GL_EXT_texture_compression_rgtc");
			// WEBGL_compressed_texture_s3tc needs DXT1 + DXT3 + DXT5 simultaneously.
			// The Tegra/Mesa driver advertises DXT1 via GL_EXT_texture_compression_dxt1
			// or GL_EXT_texture_compression_s3tc (covers all four), and DXT3/5 via
			// the ANGLE-suffixed aliases. Accept any combination that covers all four.
			backend->has_texture_compression_s3tc =
				EXT_HAS("GL_EXT_texture_compression_s3tc") ||
				(EXT_HAS("GL_EXT_texture_compression_dxt1") &&
				 EXT_HAS("GL_ANGLE_texture_compression_dxt3") &&
				 EXT_HAS("GL_ANGLE_texture_compression_dxt5"));
			backend->has_texture_compression_s3tc_srgb =
				EXT_HAS("GL_EXT_texture_compression_s3tc_srgb") ||
				EXT_HAS("GL_NV_sRGB_formats");
			backend->has_texture_norm16 = EXT_HAS("GL_EXT_texture_norm16");
			backend->has_clip_cull_distance =
				EXT_HAS("GL_EXT_clip_cull_distance") ||
				EXT_HAS("GL_APPLE_clip_distance") ||
				EXT_HAS("GL_ANGLE_clip_cull_distance");
			backend->has_float_blend = EXT_HAS("GL_EXT_float_blend");
			backend->has_render_snorm = EXT_HAS("GL_EXT_render_snorm");
			backend->has_sample_variables =
				EXT_HAS("GL_OES_sample_variables") ||
				EXT_HAS("GL_EXT_sample_variables");
			backend->has_shader_multisample_interpolation =
				EXT_HAS("GL_OES_shader_multisample_interpolation") ||
				EXT_HAS("GL_EXT_shader_multisample_interpolation");
			backend->has_parallel_shader_compile = EXT_HAS("GL_KHR_parallel_shader_compile");
			backend->has_multi_draw =
				EXT_HAS("GL_EXT_multi_draw_arrays") ||
				EXT_HAS("GL_ANGLE_multi_draw");
			backend->has_draw_buffers_indexed =
				EXT_HAS("GL_OES_draw_buffers_indexed") ||
				EXT_HAS("GL_EXT_draw_buffers_indexed");
			backend->has_blend_func_extended = EXT_HAS("GL_EXT_blend_func_extended");
		// Wave 2 compressed-texture probes.
		backend->has_texture_compression_etc1 =
			EXT_HAS("GL_OES_compressed_ETC1_RGB8_texture") ||
			EXT_HAS("GL_EXT_compressed_ETC1_RGB8_sub_texture");
		// ETC2/EAC is ES3 core — present on any GLES 3+ context. The
		// extension token GL_ARB_ES3_compatibility isn't usually in the
		// GLES string; instead we infer from successful WebGL 2 setup.
		// `webgl2_present` is set just above in this block.
		backend->has_texture_compression_etc = backend->webgl2_present;
		backend->has_texture_compression_astc =
			EXT_HAS("GL_KHR_texture_compression_astc_ldr") ||
			EXT_HAS("GL_OES_texture_compression_astc");
		backend->has_disjoint_timer_query =
			EXT_HAS("GL_EXT_disjoint_timer_query");
		// Wave 3 — promote-to-ES3-core extensions. Driver-token first,
		// fall back to webgl2_present which is true iff ES3 entry points
		// loaded successfully.
		backend->has_blend_minmax =
			EXT_HAS("GL_EXT_blend_minmax") || backend->webgl2_present;
		backend->has_frag_depth =
			EXT_HAS("GL_EXT_frag_depth") || backend->webgl2_present;
		backend->has_element_index_uint =
			EXT_HAS("GL_OES_element_index_uint") || backend->webgl2_present;
		backend->has_fbo_render_mipmap =
			EXT_HAS("GL_OES_fbo_render_mipmap") || backend->webgl2_present;
		backend->has_srgb =
			EXT_HAS("GL_EXT_sRGB") || EXT_HAS("GL_EXT_texture_sRGB_decode") ||
			backend->webgl2_present;
		backend->has_ext_color_buffer_float =
			EXT_HAS("GL_EXT_color_buffer_float");
		backend->fn_query_counter_ext =
			(void *)eglGetProcAddress("glQueryCounterEXT");
		if (!backend->fn_query_counter_ext)
			backend->fn_query_counter_ext =
				(void *)eglGetProcAddress("glQueryCounter");

			// Resolve entry points. eglGetProcAddress returns NULL for tokens
			// the driver doesn't have, so we don't even need to gate by the
			// has_ flag here — the flag gates the advertise side.
			backend->fn_clip_control = (void *)eglGetProcAddress("glClipControl");
			if (!backend->fn_clip_control)
				backend->fn_clip_control = (void *)eglGetProcAddress("glClipControlEXT");
			backend->fn_polygon_offset_clamp_ext =
				(void *)eglGetProcAddress("glPolygonOffsetClampEXT");
			if (!backend->fn_polygon_offset_clamp_ext)
				backend->fn_polygon_offset_clamp_ext =
					(void *)eglGetProcAddress("glPolygonOffsetClamp");
			backend->fn_max_shader_compiler_threads_khr =
				(void *)eglGetProcAddress("glMaxShaderCompilerThreadsKHR");
			if (!backend->fn_max_shader_compiler_threads_khr)
				backend->fn_max_shader_compiler_threads_khr =
					(void *)eglGetProcAddress("glMaxShaderCompilerThreadsARB");
			backend->fn_multi_draw_arrays_ext =
				(void *)eglGetProcAddress("glMultiDrawArraysEXT");
			if (!backend->fn_multi_draw_arrays_ext)
				backend->fn_multi_draw_arrays_ext =
					(void *)eglGetProcAddress("glMultiDrawArrays");
			if (!backend->fn_multi_draw_arrays_ext)
				backend->fn_multi_draw_arrays_ext =
					(void *)eglGetProcAddress("glMultiDrawArraysANGLE");
			backend->fn_multi_draw_elements_ext =
				(void *)eglGetProcAddress("glMultiDrawElementsEXT");
			if (!backend->fn_multi_draw_elements_ext)
				backend->fn_multi_draw_elements_ext =
					(void *)eglGetProcAddress("glMultiDrawElements");
			if (!backend->fn_multi_draw_elements_ext)
				backend->fn_multi_draw_elements_ext =
					(void *)eglGetProcAddress("glMultiDrawElementsANGLE");
			backend->fn_multi_draw_arrays_instanced_base_instance =
				(void *)eglGetProcAddress("glMultiDrawArraysInstancedBaseInstanceEXT");
			backend->fn_multi_draw_elements_instanced_base_vertex_base_instance =
				(void *)eglGetProcAddress("glMultiDrawElementsInstancedBaseVertexBaseInstanceEXT");
			backend->fn_enablei_ext = (void *)eglGetProcAddress("glEnableiEXT");
			if (!backend->fn_enablei_ext)
				backend->fn_enablei_ext = (void *)eglGetProcAddress("glEnableiOES");
			if (!backend->fn_enablei_ext)
				backend->fn_enablei_ext = (void *)eglGetProcAddress("glEnablei");
			backend->fn_disablei_ext = (void *)eglGetProcAddress("glDisableiEXT");
			if (!backend->fn_disablei_ext)
				backend->fn_disablei_ext = (void *)eglGetProcAddress("glDisableiOES");
			if (!backend->fn_disablei_ext)
				backend->fn_disablei_ext = (void *)eglGetProcAddress("glDisablei");
			backend->fn_blend_equationi_ext = (void *)eglGetProcAddress("glBlendEquationiEXT");
			if (!backend->fn_blend_equationi_ext)
				backend->fn_blend_equationi_ext = (void *)eglGetProcAddress("glBlendEquationiOES");
			if (!backend->fn_blend_equationi_ext)
				backend->fn_blend_equationi_ext = (void *)eglGetProcAddress("glBlendEquationi");
			backend->fn_blend_equation_separatei_ext =
				(void *)eglGetProcAddress("glBlendEquationSeparateiEXT");
			if (!backend->fn_blend_equation_separatei_ext)
				backend->fn_blend_equation_separatei_ext =
					(void *)eglGetProcAddress("glBlendEquationSeparateiOES");
			if (!backend->fn_blend_equation_separatei_ext)
				backend->fn_blend_equation_separatei_ext =
					(void *)eglGetProcAddress("glBlendEquationSeparatei");
			backend->fn_blend_funci_ext = (void *)eglGetProcAddress("glBlendFunciEXT");
			if (!backend->fn_blend_funci_ext)
				backend->fn_blend_funci_ext = (void *)eglGetProcAddress("glBlendFunciOES");
			if (!backend->fn_blend_funci_ext)
				backend->fn_blend_funci_ext = (void *)eglGetProcAddress("glBlendFunci");
			backend->fn_blend_func_separatei_ext =
				(void *)eglGetProcAddress("glBlendFuncSeparateiEXT");
			if (!backend->fn_blend_func_separatei_ext)
				backend->fn_blend_func_separatei_ext =
					(void *)eglGetProcAddress("glBlendFuncSeparateiOES");
			if (!backend->fn_blend_func_separatei_ext)
				backend->fn_blend_func_separatei_ext =
					(void *)eglGetProcAddress("glBlendFuncSeparatei");
			backend->fn_bind_frag_data_location_ext =
				(void *)eglGetProcAddress("glBindFragDataLocationEXT");
			backend->fn_bind_frag_data_location_indexed_ext =
				(void *)eglGetProcAddress("glBindFragDataLocationIndexedEXT");
			backend->fn_get_frag_data_index_ext =
				(void *)eglGetProcAddress("glGetFragDataIndexEXT");
			// Core compressed-tex calls — already available in GLES 2, but the
			// existing bridge code only wired the 3D variants. Resolving here
			// gives the 2D path (used by all of s3tc / bptc / rgtc).
			backend->fn_compressed_tex_image_2d =
				(void *)eglGetProcAddress("glCompressedTexImage2D");
			backend->fn_compressed_tex_sub_image_2d =
				(void *)eglGetProcAddress("glCompressedTexSubImage2D");

			#undef EXT_HAS
		}
	}

	backend->available = true;
		backend->step = 9;
	snprintf(backend->status, sizeof(backend->status),
			 "EGL/OpenGL ES context initialized");
	(void)canvas;
	return true;

	default:
		snprintf(backend->status, sizeof(backend->status),
				 "EGL/OpenGL ES probe has no more initialization steps");
		return backend->available;
	}
#endif
}

void nx_webgl_egl_destroy(JSRuntime *rt, nx_webgl_egl_t *backend) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	if (backend->display != EGL_NO_DISPLAY) {
		if (backend->context != EGL_NO_CONTEXT) {
			eglMakeCurrent(backend->display, backend->surface,
						   backend->surface, backend->context);
			destroy_texture_cache(backend);
			destroy_bridge_resources(backend);
		}
		eglMakeCurrent(backend->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
					   EGL_NO_CONTEXT);
		if (backend->context != EGL_NO_CONTEXT)
			eglDestroyContext(backend->display, backend->context);
		if (backend->surface != EGL_NO_SURFACE)
			eglDestroySurface(backend->display, backend->surface);
		eglTerminate(backend->display);
	}
	free(backend->tessellation_scratch);
	backend->tessellation_scratch = NULL;
	backend->tessellation_scratch_capacity_floats = 0;
#endif
	js_free_rt(rt, backend);
}

bool nx_webgl_egl_reset_context(nx_webgl_egl_t *backend, nx_canvas_t *canvas) {
	if (!backend)
		return false;
#if NXJS_HAS_EGL_GLES
	// Reset is meaningful only after a successful first init. If the
	// backend never reached available=true, just re-run the initializer
	// from wherever it left off — it's idempotent up to first success.
	if (backend->display == EGL_NO_DISPLAY) {
		return nx_webgl_egl_initialize(backend, canvas);
	}
	// 1. Make the current context current so the destroy calls below
	//    delete the bridge resources from the right GLES name space.
	//    If the context is already gone (defensive), skip the deletes.
	if (backend->context != EGL_NO_CONTEXT) {
		eglMakeCurrent(backend->display, backend->surface, backend->surface,
		               backend->context);
		// 2. Tear down everything the bridge owns INSIDE the context.
		//    These mirror nx_webgl_egl_destroy's pre-context-destroy
		//    work — they free all `bridge_*` programs/buffers/textures
		//    plus the texture cache. After this the lazy ensure_*
		//    helpers will rebuild them on first use against the new
		//    context.
		destroy_texture_cache(backend);
		destroy_bridge_resources(backend);
		// Also drop the passthrough VAO created at step 8; it belongs
		// to the doomed context. probe_step regenerates it on re-init.
		if (backend->passthrough_vao && backend->fn_delete_vertex_arrays) {
			typedef void (*pfn_del_vao_t)(GLsizei, const GLuint *);
			pfn_del_vao_t del =
				(pfn_del_vao_t)backend->fn_delete_vertex_arrays;
			GLuint v = backend->passthrough_vao;
			del(1, &v);
		}
		backend->passthrough_vao = 0;
		// 3. Detach the context so eglDestroyContext can free it. EGL
		//    spec: a destroyed context is marked for deletion but only
		//    actually freed once it's not current — make_current(NO_*)
		//    first satisfies that.
		eglMakeCurrent(backend->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
		               EGL_NO_CONTEXT);
		eglDestroyContext(backend->display, backend->context);
		backend->context = EGL_NO_CONTEXT;
	}
	// 4. Destroy the surface too (no-op in surfaceless mode where
	//    surface is already EGL_NO_SURFACE, which is the path
	//    nx_webgl_egl on Switch takes per the probe_step config flow).
	if (backend->surface != EGL_NO_SURFACE) {
		eglDestroySurface(backend->display, backend->surface);
		backend->surface = EGL_NO_SURFACE;
	}
	// 5. Keep display + config alive (DO NOT eglTerminate). Restart the
	//    probe state machine at step 4 — that's the first step that
	//    runs AFTER eglChooseConfig (which already cached
	//    backend->config) and BEFORE surface/context creation. Walking
	//    the machine from there: step 4 → 5 (set surface=NO_SURFACE) →
	//    6 (eglCreateContext) → 7 (eglMakeCurrent) → 8 (resolve
	//    function pointers + extension probes) → done. All against the
	//    fresh context.
	backend->available = false;
	backend->step = 4;
	// 6. Function pointers (`fn_*` table) intentionally NOT cleared.
	//    Per EGL spec, eglGetProcAddress returns display-scoped function
	//    pointers that remain valid until the application terminates —
	//    they don't change across eglCreateContext/eglDestroyContext
	//    cycles on the same display. Re-running probe_step 8's
	//    `if (!backend->fn_X) backend->fn_X = eglGetProcAddress(...)`
	//    block would short-circuit, but that's fine: the cached value
	//    points to the same driver function the new context will use.
	//    Leaving them alone also avoids a 96-field-explicit-null block.
	// Stick the bridge_pending_textured_warmup flag back on so the next
	// bridge-mode draw runs the validation warmup that bridge_enabled
	// would also set after a fresh `set_bridge_enabled(true)`.
	backend->bridge_pending_textured_warmup = true;
	backend->bridge_auto_flush_initialized = false;
	// 7. Drive the state machine to completion. probe_step transitions
	//    one step per call; loop until available flips back on or until
	//    a step returns false (failure).
	while (!backend->available) {
		int previous_step = backend->step;
		if (!nx_webgl_egl_probe_step(backend, canvas))
			return false;
		if (backend->step == previous_step)
			return false;
	}
	return true;
#else
	(void)canvas;
	return false;
#endif
}

bool nx_webgl_egl_is_available(nx_webgl_egl_t *backend) {
	return backend && backend->available;
}

void nx_webgl_egl_set_clear_color(nx_webgl_egl_t *backend, double *color) {
	if (!backend)
		return;
	for (int i = 0; i < 4; i++)
		backend->clear_color[i] = color[i];
}

void nx_webgl_egl_set_bridge_enabled(nx_webgl_egl_t *backend, bool enabled) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	backend->bridge_enabled = enabled;
	if (enabled && !backend->bridge_auto_flush_initialized) {
		// First-time enable: default to true so existing apps
		// (GpuCompositor and any future drawArrays user that relies on
		// auto-present) work unchanged. Apps that drive their own
		// readback can opt out via `gl.setBridgeAutoFlush(false)`.
		//
		// Gated on `!bridge_auto_flush_initialized` (NOT the
		// `_enabled` flag itself) so a later re-call from a different
		// page can't undo a deliberate `setBridgeAutoFlush(false)` —
		// the C code can't tell "never set" from "explicitly false"
		// just from the boolean value, but it can tell from this
		// sticky one-shot init flag.
		backend->bridge_auto_flush_enabled = true;
		backend->bridge_auto_flush_initialized = true;
	}
	if (enabled) {
		// The first textured draw after bridge-enable also needs the
		// pipeline-state-validation warmup, in case the caller doesn't
		// gl.clear() before its first draw (see bridge_pending_textured_warmup).
		backend->bridge_pending_textured_warmup = true;
	}
	snprintf(backend->status, sizeof(backend->status),
			 enabled ? "GPU bridge render mode enabled"
					 : "GPU bridge render mode disabled");
#else
	(void)enabled;
#endif
}

void nx_webgl_egl_set_dispatch_debug(nx_webgl_egl_t *backend,
									 const char *label) {
	if (!backend)
		return;
	snprintf(backend->debug_dispatch_state, sizeof(backend->debug_dispatch_state),
			 "%s", label ? label : "");
}

// Append a short tag to the dispatch-debug ring. Used by the per-draw
// instrumentation to record which path each draw took: bridge success,
// bridge failed (with reason), software fallback, etc. Reset by
// `nx_webgl_egl_reset_dispatch_debug` once per frame (via gl.clear).
void nx_webgl_egl_append_dispatch_debug(nx_webgl_egl_t *backend,
										const char *tag) {
	if (!backend || !tag)
		return;
	size_t buf_size = sizeof(backend->debug_dispatch_state);
	size_t cur = strnlen(backend->debug_dispatch_state, buf_size);
	if (cur + 1 >= buf_size)
		return;
	size_t remaining = buf_size - cur;
	snprintf(backend->debug_dispatch_state + cur, remaining, "%s ", tag);
}

void nx_webgl_egl_reset_dispatch_debug(nx_webgl_egl_t *backend) {
	if (!backend)
		return;
	backend->debug_dispatch_state[0] = '\0';
}

void nx_webgl_egl_set_auto_flush(nx_webgl_egl_t *backend, bool enabled) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	backend->bridge_auto_flush_enabled = enabled;
	// Explicit write — the auto-default-to-true branch in
	// `set_bridge_enabled` should not override this on a later re-enable.
	backend->bridge_auto_flush_initialized = true;
	// If disabling while a pending flush exists, drop the flag too so
	// the next gl.clear doesn't try to flush anyway through other paths.
	if (!enabled)
		backend->bridge_pending_readback = false;
#else
	(void)enabled;
#endif
}

bool nx_webgl_egl_get_auto_flush(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->bridge_auto_flush_enabled;
#else
	(void)backend;
	return false;
#endif
}

void nx_webgl_egl_set_tessellation_fix(nx_webgl_egl_t *backend, bool enabled) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	backend->tessellation_fix_enabled = enabled;
#else
	(void)enabled;
#endif
}

bool nx_webgl_egl_get_tessellation_fix(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->tessellation_fix_enabled;
#else
	(void)backend;
	return false;
#endif
}

bool nx_webgl_egl_is_bridge_enabled(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->bridge_enabled;
#else
	(void)backend;
	return false;
#endif
}

// Option 2 / spec-y opt-in (2026-06-26). See header comment.
void nx_webgl_egl_set_spec_y_origin(nx_webgl_egl_t *backend, bool enabled) {
#if NXJS_HAS_EGL_GLES
	if (backend) backend->spec_y_origin = enabled;
#else
	(void)backend; (void)enabled;
#endif
}

bool nx_webgl_egl_get_spec_y_origin(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->spec_y_origin;
#else
	(void)backend;
	return false;
#endif
}

void nx_webgl_egl_uniform1f(nx_webgl_egl_t *backend, int location, float x) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform1f(location, x);
#endif
}

void nx_webgl_egl_uniform2f(nx_webgl_egl_t *backend, int location, float x, float y) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform2f(location, x, y);
#endif
}

void nx_webgl_egl_uniform3f(nx_webgl_egl_t *backend, int location, float x, float y, float z) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform3f(location, x, y, z);
#endif
}

void nx_webgl_egl_uniform4f(nx_webgl_egl_t *backend, int location, float x, float y, float z, float w) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform4f(location, x, y, z, w);
#endif
}

void nx_webgl_egl_uniform1i(nx_webgl_egl_t *backend, int location, int x) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform1i(location, x);
#endif
}

void nx_webgl_egl_uniform_matrix4fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniformMatrix4fv(location, 1, transpose ? GL_TRUE : GL_FALSE, value);
#endif
}

void nx_webgl_egl_uniform_matrix3fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniformMatrix3fv(location, 1, transpose ? GL_TRUE : GL_FALSE, value);
#endif
}

void nx_webgl_egl_uniform_matrix2fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniformMatrix2fv(location, 1, transpose ? GL_TRUE : GL_FALSE, value);
#endif
}

void nx_webgl_egl_uniform1fv(nx_webgl_egl_t *backend, int location, int count, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform1fv(location, count, value);
#endif
}

void nx_webgl_egl_uniform2fv(nx_webgl_egl_t *backend, int location, int count, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform2fv(location, count, value);
#endif
}

void nx_webgl_egl_uniform3fv(nx_webgl_egl_t *backend, int location, int count, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform3fv(location, count, value);
#endif
}

void nx_webgl_egl_uniform4fv(nx_webgl_egl_t *backend, int location, int count, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform4fv(location, count, value);
#endif
}

void nx_webgl_egl_uniform2i(nx_webgl_egl_t *backend, int location, int x, int y) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform2i(location, x, y);
#endif
}

void nx_webgl_egl_uniform3i(nx_webgl_egl_t *backend, int location, int x, int y, int z) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform3i(location, x, y, z);
#endif
}

void nx_webgl_egl_uniform4i(nx_webgl_egl_t *backend, int location, int x, int y, int z, int w) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform4i(location, x, y, z, w);
#endif
}

void nx_webgl_egl_uniform1iv(nx_webgl_egl_t *backend, int location, int count, const int *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform1iv(location, count, value);
#endif
}

void nx_webgl_egl_uniform2iv(nx_webgl_egl_t *backend, int location, int count, const int *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform2iv(location, count, value);
#endif
}

void nx_webgl_egl_uniform3iv(nx_webgl_egl_t *backend, int location, int count, const int *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform3iv(location, count, value);
#endif
}

void nx_webgl_egl_uniform4iv(nx_webgl_egl_t *backend, int location, int count, const int *value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || location < 0 || count <= 0 || !value)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUniform4iv(location, count, value);
#endif
}

uint32_t nx_webgl_egl_create_native_buffer(nx_webgl_egl_t *backend,
										   nx_canvas_t *canvas) {
#if NXJS_HAS_EGL_GLES
	if (!backend)
		return 0;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return 0;
	GLuint handle = 0;
	glGenBuffers(1, &handle);
	return (uint32_t)handle;
#else
	(void)backend;
	(void)canvas;
	return 0;
#endif
}

void nx_webgl_egl_delete_native_buffer(nx_webgl_egl_t *backend,
									   uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || handle == 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	GLuint h = (GLuint)handle;
	glDeleteBuffers(1, &h);
#else
	(void)backend;
	(void)handle;
#endif
}

void nx_webgl_egl_native_buffer_data(nx_webgl_egl_t *backend,
									 uint32_t handle, uint32_t target,
									 size_t size, const void *data,
									 uint32_t usage) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || handle == 0)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	// Allocator + upload. Disrupts the current ARRAY_BUFFER /
	// ELEMENT_ARRAY_BUFFER binding, but every bridge draw re-binds its own
	// VBOs before reading, so we don't need to save/restore here.
	glBindBuffer(target, (GLuint)handle);
	glBufferData(target, (GLsizeiptr)size, data, usage);
#else
	(void)backend;
	(void)handle;
	(void)target;
	(void)size;
	(void)data;
	(void)usage;
#endif
}

void nx_webgl_egl_native_buffer_sub_data(nx_webgl_egl_t *backend,
										 uint32_t handle, uint32_t target,
										 size_t offset, size_t size,
										 const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || handle == 0 || size == 0 || !data)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glBindBuffer(target, (GLuint)handle);
	glBufferSubData(target, (GLintptr)offset, (GLsizeiptr)size, data);
#else
	(void)backend;
	(void)handle;
	(void)target;
	(void)offset;
	(void)size;
	(void)data;
#endif
}

void nx_webgl_egl_use_native_program(nx_webgl_egl_t *backend,
									 uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	glUseProgram((GLuint)handle);
#else
	(void)backend;
	(void)handle;
#endif
}

// Raw-shader passthrough draw. Runs the user's linked GLES program against
// user-uploaded buffers, with vertex-attribute state synced to native GL.
// Bypasses the bridge's hardcoded-program swap so user GLSL (including
// custom uniforms, fwidth(), gl_FrontFacing, etc.) executes as written.
// See [[bridge-raw-shader-passthrough]] for the architectural background.
//
// Uniforms reach the program through the existing
// `nx_webgl_egl_uniform*` helpers — by the time the user calls
// `gl.drawXxx()`, their `gl.uniform*` setters have already uploaded values
// to `program_handle` (via the `gl.useProgram → glUseProgram` plumbing in
// `nx_webgl_use_program`).
const char *nx_webgl_egl_get_vendor(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend ? backend->vendor : NULL;
#else
	(void)backend;
	return NULL;
#endif
}

const char *nx_webgl_egl_get_renderer(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend ? backend->renderer : NULL;
#else
	(void)backend;
	return NULL;
#endif
}

bool nx_webgl_egl_has_instancing(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend &&
		   backend->fn_vertex_attrib_divisor_ext &&
		   backend->fn_draw_arrays_instanced_ext &&
		   backend->fn_draw_elements_instanced_ext;
#else
	(void)backend;
	return false;
#endif
}

bool nx_webgl_egl_draw_passthrough(
	nx_webgl_egl_t *backend,
	nx_canvas_t *canvas,
	uint32_t program_handle,
	uint32_t mode,
	bool indexed,
	int32_t first,
	int32_t count,
	uint32_t element_type,
	uint32_t element_offset,
	uint32_t element_buffer_handle,
	const nx_webgl_egl_passthrough_attrib_t *attribs,
	int attrib_count,
	int32_t instance_count,
	const int *viewport,
	bool blend,
	uint32_t blend_src, uint32_t blend_dst,
	uint32_t blend_src_alpha, uint32_t blend_dst_alpha,
	bool scissor_enabled,
	const int *scissor_box,
	bool depth_enabled,
	bool cull_enabled,
	uint32_t cull_face_mode,
	uint32_t front_face,
	bool stencil_enabled,
	uint32_t stencil_func, int32_t stencil_ref, uint32_t stencil_value_mask,
	uint32_t stencil_fail, uint32_t stencil_zfail, uint32_t stencil_zpass,
	uint32_t stencil_mask,
	const bool *color_mask) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)canvas; (void)program_handle; (void)mode;
	(void)indexed; (void)first; (void)count; (void)element_type;
	(void)element_offset; (void)element_buffer_handle; (void)attribs;
	(void)attrib_count; (void)instance_count; (void)viewport; (void)blend;
	(void)blend_src; (void)blend_dst; (void)blend_src_alpha;
	(void)blend_dst_alpha; (void)scissor_enabled; (void)scissor_box;
	(void)depth_enabled; (void)cull_enabled; (void)cull_face_mode;
	(void)front_face; (void)stencil_enabled; (void)stencil_func;
	(void)stencil_ref; (void)stencil_value_mask; (void)stencil_fail;
	(void)stencil_zfail; (void)stencil_zpass; (void)stencil_mask;
	(void)color_mask;
	return false;
#else
	// Function pointer typedefs for the runtime-loaded instancing entry
	// points. Both EXT and core-GLES3 variants share these prototypes.
	typedef void (*pfn_vertex_attrib_divisor_t)(GLuint index, GLuint divisor);
	typedef void (*pfn_draw_arrays_instanced_t)(GLenum mode, GLint first,
												 GLsizei count,
												 GLsizei instance_count);
	typedef void (*pfn_draw_elements_instanced_t)(GLenum mode, GLsizei count,
													GLenum type,
													const void *indices,
													GLsizei instance_count);

	// Instancing requested but driver lacks one of the entry points.
	if (instance_count > 0 && !nx_webgl_egl_has_instancing(backend)) {
		if (backend)
			nx_webgl_egl_append_dispatch_debug(backend, "P-noinst");
		return false;
	}
	if (!backend || !canvas || !program_handle || count <= 0 || !attribs)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		return false;
	}

	// (UBO re-apply moved closer to glDrawArrays — see just before draw)

	bridge_target_t target;
	if (!bridge_acquire_target(backend, canvas, &target))
		return false;

	bridge_bind_target(&target, backend, canvas, viewport, scissor_enabled, scissor_box);

	// Bind a VAO so vertex attribute state has somewhere to land. On
	// ES 3+ the default VAO 0 is reserved (no attribute state, all draws
	// no-op silently); on ES 2 with OES_vertex_array_object we get the
	// same plumbing for free. WebGL 2 users can bind their own VAO via
	// gl.bindVertexArray — when that's set, honor it (the user manages
	// their own attribute state). Otherwise fall back to the persistent
	// passthrough VAO so legacy WebGL 1 code that doesn't know about VAOs
	// keeps working.
	if (backend->fn_bind_vertex_array) {
		typedef void (*pfn_bind_vao_t)(GLuint);
		pfn_bind_vao_t bind = (pfn_bind_vao_t)backend->fn_bind_vertex_array;
		GLuint vao = backend->current_user_vao
		                 ? backend->current_user_vao
		                 : (GLuint)backend->passthrough_vao;
		if (vao)
			bind(vao);
	}

	glUseProgram((GLuint)program_handle);

	// Sync vertex-attribute state. Crucially, we MASK by the program's
	// active attributes — any context->vertex_attribs[] slot that isn't
	// referenced by the user's vertex shader gets force-disabled
	// regardless of what `enabled`/buffer state the JS side has tracked.
	//
	// Why: nx.js's WebGL context is shared across inline-canvas pages
	// (see [[swb-webgl-inline]]), so vertex-attrib enable bits leak from
	// the previous page's draws. If a stale slot is left enabled with
	// a no-longer-valid pointer/buffer, Tegra's GLES validator silently
	// skips the entire glDrawArrays — even when the active program
	// doesn't reference that slot. Disabling unused slots first
	// restores the spec-aligned "what the program needs is what gets
	// fed" contract.
	bool location_is_used[NX_WEBGL_MAX_VERTEX_ATTRIBS_LIMIT] = {false};
	{
		GLint active_attribs = 0;
		glGetProgramiv((GLuint)program_handle, GL_ACTIVE_ATTRIBUTES,
					   &active_attribs);
		GLint max_name_len = 64;
		glGetProgramiv((GLuint)program_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
					   &max_name_len);
		if (max_name_len <= 0 || max_name_len > 256) max_name_len = 256;
		char name[256];
		for (GLint i = 0; i < active_attribs; i++) {
			GLsizei wlen = 0;
			GLint size = 0;
			GLenum type = 0;
			glGetActiveAttrib((GLuint)program_handle, i,
							  sizeof(name), &wlen, &size, &type, name);
			GLint loc = glGetAttribLocation((GLuint)program_handle, name);
			if (loc < 0)
				continue;
			// Matrix and array attributes occupy multiple consecutive
			// locations. A `mat4` attribute (e.g. Three.js's
			// `instanceMatrix`) is bound as 4 vec4 attributes at
			// loc..loc+3; without expanding here, the per-instance rows
			// 2/3/4 get force-disabled and the shader reads garbage.
			int location_span = 1;
			switch (type) {
				case GL_FLOAT_MAT2: location_span = 2; break;
				case GL_FLOAT_MAT3: location_span = 3; break;
				case GL_FLOAT_MAT4: location_span = 4; break;
				default: location_span = 1; break;
			}
			// `size > 1` indicates an attribute array — each element
			// gets its own location run.
			if (size > 1) location_span *= size;
			for (int j = 0; j < location_span; j++) {
				int slot = loc + j;
				if (slot >= 0 && slot < attrib_count &&
					slot < NX_WEBGL_MAX_VERTEX_ATTRIBS_LIMIT) {
					location_is_used[slot] = true;
				}
			}
		}
	}

	pfn_vertex_attrib_divisor_t fn_divisor =
		(pfn_vertex_attrib_divisor_t)backend->fn_vertex_attrib_divisor_ext;

	// Diagnostic for instancing bug: record what divisor we ASKED for AND
	// what GL reports AFTER the set call. If the read-back doesn't match,
	// fn_divisor is a stub. If they match but instances still don't advance,
	// the bug is elsewhere (probably driver-side).
	char div_tag[128];
	int div_tag_len = 0;
	div_tag[0] = '\0';

	for (int i = 0; i < attrib_count; i++) {
		const nx_webgl_egl_passthrough_attrib_t *a = &attribs[i];
		if (!location_is_used[i]) {
			glDisableVertexAttribArray((GLuint)i);
			// Clear any stale divisor on disabled slots so a non-instanced
			// re-bind later doesn't inherit it (passthrough state survives
			// across draws on the shared screen GL context — see
			// [[swb-shared-gl-state-leak]]).
			if (fn_divisor)
				fn_divisor((GLuint)i, 0u);
			continue;
		}
		if (a->enabled && a->buffer_handle) {
			glBindBuffer(GL_ARRAY_BUFFER, (GLuint)a->buffer_handle);
			glEnableVertexAttribArray((GLuint)i);
			glVertexAttribPointer((GLuint)i,
								  a->size,
								  a->type,
								  a->normalized ? GL_TRUE : GL_FALSE,
								  a->stride,
								  (const void *)(uintptr_t)a->offset);
			// Apply the JS-tracked divisor when the driver supports it.
			// Always emit (even divisor 0) so stale divisor>0 state from a
			// prior instanced draw doesn't leak into a per-vertex attrib —
			// the shared screen GL context per [[swb-webgl-inline]] means
			// per-attrib state survives across pages until we re-set it.
			if (fn_divisor)
				fn_divisor((GLuint)i, a->divisor);
			// Drain any error the divisor call might have produced (some
			// drivers reject high indices); a stale error here would make
			// the post-draw glGetError() report it instead of the actual
			// draw status.
			(void)glGetError();
			// Read back the divisor we just set so the dispatch-debug tag
			// can report what native GL actually accepted. 0x88FE is
			// GL_VERTEX_ATTRIB_ARRAY_DIVISOR.
			GLint got_div = -1;
			glGetVertexAttribiv((GLuint)i, 0x88FE, &got_div);
			(void)glGetError();
			int n = snprintf(div_tag + div_tag_len, sizeof(div_tag) - div_tag_len,
							 "%d=%u/%d ", i, a->divisor, got_div);
			if (n > 0 && (size_t)(div_tag_len + n) < sizeof(div_tag))
				div_tag_len += n;
		} else {
			glDisableVertexAttribArray((GLuint)i);
			if (fn_divisor)
				fn_divisor((GLuint)i, 0u);
			(void)glGetError();
		}
	}

	if (blend) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(blend_src, blend_dst, blend_src_alpha,
							blend_dst_alpha);
	} else {
		glDisable(GL_BLEND);
	}

	if (depth_enabled) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glDepthRangef(0.f, 1.f);
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}

	if (cull_enabled) {
		GLenum gl_mode = GL_BACK;
		if (cull_face_mode == 0x0404u) gl_mode = GL_FRONT;
		else if (cull_face_mode == 0x0408u) gl_mode = GL_FRONT_AND_BACK;
		glEnable(GL_CULL_FACE);
		glCullFace(gl_mode);
	} else {
		glDisable(GL_CULL_FACE);
	}
	GLenum gl_front = GL_CCW;
	if (front_face == 0x0900u) gl_front = GL_CW;
	glFrontFace(gl_front);

	// 2026-06-08 ROUND 22: stencil + color-mask state. Without this
	// cc.Mask's stencil clipping doesn't work (text overflowing popups
	// stays visible). The stencil_* and color_mask values come from the
	// JS-side state cached in nx_webgl_context_t by gl.stencilFunc /
	// stencilOp / stencilMask / colorMask.
	if (stencil_enabled) {
		glEnable(GL_STENCIL_TEST);
		glStencilFunc((GLenum)stencil_func, stencil_ref, stencil_value_mask);
		glStencilOp((GLenum)stencil_fail, (GLenum)stencil_zfail,
					(GLenum)stencil_zpass);
		glStencilMask(stencil_mask);
	} else {
		glDisable(GL_STENCIL_TEST);
	}
	if (color_mask) {
		glColorMask(color_mask[0] ? GL_TRUE : GL_FALSE,
					color_mask[1] ? GL_TRUE : GL_FALSE,
					color_mask[2] ? GL_TRUE : GL_FALSE,
					color_mask[3] ? GL_TRUE : GL_FALSE);
	} else {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	(void)glGetError();  // drain stale errors

	{
		static int pre_n = 0;
		if (pre_n++ < 50) {
			GLint num_blocks = 0;
			GLint num_attribs = 0;
			GLint num_uniforms = 0;
			glGetProgramiv((GLuint)program_handle,
			               0x8A36 /* ACTIVE_UNIFORM_BLOCKS */, &num_blocks);
			glGetProgramiv((GLuint)program_handle, GL_ACTIVE_ATTRIBUTES,
			               &num_attribs);
			glGetProgramiv((GLuint)program_handle, GL_ACTIVE_UNIFORMS,
			               &num_uniforms);
			int bound_ubos = 0;
			char ubo_list[256] = {0};
			int written = 0;
			for (int i = 0; i < NX_WEBGL_MAX_UBO_BINDINGS; i++) {
				if (backend->ubo_indexed_bindings[i] != 0) {
					bound_ubos++;
					if (written < (int)sizeof(ubo_list) - 16) {
						written += snprintf(ubo_list + written,
						                     sizeof(ubo_list) - written,
						                     "%d:%u,", i,
						                     backend->ubo_indexed_bindings[i]);
					}
				}
			}
			int enabled_attribs = 0;
			char attr_list[256] = {0};
			int aw = 0;
			for (int i = 0; i < attrib_count; i++) {
				if (attribs[i].enabled) {
					enabled_attribs++;
					if (aw < (int)sizeof(attr_list) - 32) {
						aw += snprintf(attr_list + aw,
						                sizeof(attr_list) - aw,
						                "%d:buf%u/sz%d/of%d/st%d ",
						                i, attribs[i].buffer_handle,
						                attribs[i].size,
						                (int)attribs[i].offset,
						                attribs[i].stride);
					}
				}
			}
			fprintf(stderr,
				"[nxjs:pre-draw] n=%d prog=%u active_blocks=%d active_attrs=%d "
				"active_uniforms=%d bound_ubos=%d user_vao=%u\n"
				"  ubo_slots=%s\n"
				"  enabled_attrs(%d)=%s\n",
				pre_n, program_handle, num_blocks, num_attribs, num_uniforms,
				bound_ubos, backend->current_user_vao, ubo_list,
				enabled_attribs, attr_list);
			(void)glGetError();
		}
	}

	// Re-apply tracked UBO slot bindings + program block bindings right
	// before every draw. Mesa Nouveau on Citron has a deep driver bug
	// where the second-and-later draws of a program reading from a UBO
	// silently return zero — even when the slot bindings AND the program's
	// block-to-slot mappings are correctly established at API level. We
	// re-issue everything here as a best-effort defensive measure; the
	// actual fix lives in real Tegra hardware where this driver bug
	// doesn't exist. See [[mesa-nouveau-ubo-second-draw-bug]] memory.
	{
		typedef void (*pfn_base_t)(GLenum, GLuint, GLuint);
		typedef void (*pfn_range_t)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);
		pfn_base_t fn_base = (pfn_base_t)backend->fn_bind_buffer_base;
		pfn_range_t fn_range = (pfn_range_t)backend->fn_bind_buffer_range;
		if (fn_base) {
			for (int i = 0; i < NX_WEBGL_MAX_UBO_BINDINGS; i++) {
				GLuint buf = backend->ubo_indexed_bindings[i];
				if (buf == 0) continue;
				if (backend->ubo_indexed_sizes[i] > 0 && fn_range) {
					fn_range(0x8A11, (GLuint)i, buf,
					         backend->ubo_indexed_offsets[i],
					         backend->ubo_indexed_sizes[i]);
				} else {
					fn_base(0x8A11, (GLuint)i, buf);
				}
			}
			(void)glGetError();
		}
		typedef void (*pfn_ubb_t)(GLuint, GLuint, GLuint);
		typedef void (*pfn_gaubi_t)(GLuint, GLuint, GLenum, GLint *);
		pfn_ubb_t fn_ubb = (pfn_ubb_t)backend->fn_uniform_block_binding;
		pfn_gaubi_t fn_gaubi = (pfn_gaubi_t)backend->fn_get_active_uniform_block_iv;
		if (fn_ubb && fn_gaubi) {
			GLint num_blocks = 0;
			glGetProgramiv((GLuint)program_handle, 0x8A36 /* ACTIVE_UNIFORM_BLOCKS */,
			               &num_blocks);
			(void)glGetError();
			for (GLint b = 0; b < num_blocks && b < 16; b++) {
				GLint binding = 0;
				fn_gaubi((GLuint)program_handle, (GLuint)b,
				         0x8A3F /* UNIFORM_BLOCK_BINDING */, &binding);
				(void)glGetError();
				fn_ubb((GLuint)program_handle, (GLuint)b, (GLuint)binding);
			}
			(void)glGetError();
		}
	}

	if (indexed) {
		if (element_buffer_handle == 0) {
			// No native element buffer — the user called drawElements
			// before bufferData'ing their index buffer, or the buffer was
			// deleted. Pre-WebIDL nx.js would have INVALID_OPERATION'd
			// during validation; here we tag and bail rather than passing
			// offset as a client-side pointer (undefined behavior).
			nx_webgl_egl_append_dispatch_debug(backend, "P-noEAB");
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glDisable(GL_SCISSOR_TEST);
			return false;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)element_buffer_handle);
		if (instance_count > 0) {
			pfn_draw_elements_instanced_t fn =
				(pfn_draw_elements_instanced_t)backend->fn_draw_elements_instanced_ext;
			fn(mode, count, element_type,
			   (const void *)(uintptr_t)element_offset, instance_count);
		} else {
			glDrawElements(mode, count, element_type,
						   (const void *)(uintptr_t)element_offset);
		}
	} else {
		/* PMREM Tegra-compat: glFinish() before user-FBO draws as
		 * defense-in-depth (sync barrier). The PMREM crash root cause
		 * was the PMREMGGXConvolution FS body; addressed via shader
		 * replacement in nx_webgl_egl_compile_shader. */
		if (target.is_user_fbo) {
			glFinish();
		}
		if (instance_count > 0) {
			pfn_draw_arrays_instanced_t fn =
				(pfn_draw_arrays_instanced_t)backend->fn_draw_arrays_instanced_ext;
			fn(mode, first, count, instance_count);
		} else {
			glDrawArrays(mode, first, count);
		}
	}
	GLenum error = glGetError();
	backend->bridge_last_draw_gl_error = error;

	/* 2026-06-24 Fix G v3: per-draw rescue (v1) was REMOVED. The replacement
	 * rescues once per face transition from `nx_webgl_egl_set_user_framebuffer`
	 * via `cube_face_rescue_transition`. v1's per-draw copy-face-0->face-N
	 * overwrote face N storage on every draw, destroying earlier accumulated
	 * objects (CubeCamera multi-object renders showed "the same object
	 * reflected N times" because only the last-drawn object survived in each
	 * face). v3 snapshots/copies once per face transition, which lets all
	 * draws within a single face accumulate correctly into face-0 storage
	 * via Mesa's aliasing, then atomically copies the final result to face N
	 * at transition time. Both single-draw (CubemapFromEquirect) and multi-
	 * draw (CubeCamera) cases work. */

	// Clean up: leave the bridge's own state mostly untouched. The bridge
	// dispatch re-binds its own attribs/buffers/program before each draw,
	// so the dirty attribute state we leave behind doesn't affect bridge-
	// mode draws. We DO unbind buffers so subsequent gl.bufferData calls
	// don't accidentally mutate the user's last-bound buffer.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	// Unbind our passthrough VAO so bridge-mode draws (which rely on the
	// default attribute state, not our per-attrib divisor stash) start
	// clean. Bridge mode re-enables/re-binds attribs on every draw so VAO
	// 0 is the right destination for them.
	if (backend->fn_bind_vertex_array && backend->passthrough_vao) {
		typedef void (*pfn_bind_vao_t)(GLuint);
		pfn_bind_vao_t bind = (pfn_bind_vao_t)backend->fn_bind_vertex_array;
		bind(0);
	}

	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "passthrough draw GL error: 0x%x (mode=0x%x count=%d)",
				 error, mode, count);
		// Append a short tag to the dispatch-debug ring so the demo's
		// status canvas can show it.
		char tag[24];
		snprintf(tag, sizeof(tag), "P-gl(0x%x)", error);
		nx_webgl_egl_append_dispatch_debug(backend, tag);
		bridge_mark_readback(backend, &target);
		return false;
	}

	{
		char tag[24];
		if (instance_count > 0)
			snprintf(tag, sizeof(tag), "P+%dx%d", count, instance_count);
		else
			snprintf(tag, sizeof(tag), "P+%d", count);
		nx_webgl_egl_append_dispatch_debug(backend, tag);
		// Append the divisor diagnostic — "i=asked/got" per enabled+used
		// attrib. Lets us see if glVertexAttribDivisor took effect on
		// native GL (got matches asked) or was silently dropped.
		if (instance_count > 0 && div_tag[0]) {
			char wrapper[160];
			snprintf(wrapper, sizeof(wrapper), "[div %s]", div_tag);
			nx_webgl_egl_append_dispatch_debug(backend, wrapper);
		}
	}
	bridge_mark_readback(backend, &target);
	return true;
#endif
}

void nx_webgl_egl_set_bridge_resolution(nx_webgl_egl_t *backend, int width,
										int height) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	if (width <= 0 || height <= 0) {
		backend->bridge_requested_width = 0;
		backend->bridge_requested_height = 0;
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge resolution reset to canvas size");
		return;
	}
	backend->bridge_requested_width = width;
	backend->bridge_requested_height = height;
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge resolution requested: %dx%d", width, height);
#else
	(void)width;
	(void)height;
#endif
}

void nx_webgl_egl_delete_cached_texture(nx_webgl_egl_t *backend,
										uint32_t texture_id) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)texture_id;
#else
	if (!backend || texture_id == 0)
		return;
	for (int i = 0; i < NX_WEBGL_EGL_TEXTURE_CACHE_SIZE; i++) {
		nx_webgl_egl_texture_cache_entry_t *entry = &backend->texture_cache[i];
		if (entry->texture_id == texture_id) {
			if (entry->handle)
				glDeleteTextures(1, &entry->handle);
			memset(entry, 0, sizeof(*entry));
			return;
		}
	}
#endif
}

// ─────────────────────────────────────────────────────────────────────
// FBO / renderbuffer / persistent-texture native entry points (milestone
// #19, webgl_postprocessing). The bridge previously always rendered into
// `bridge_framebuffer`; supporting Three.js's `WebGLRenderTarget` requires
// (a) native FBOs/RBOs and persistent texture handles the user code can
// attach as color/depth, and (b) the dispatch-target retargeting via
// `bridge_acquire_target` above. These functions are the EGL-side shims
// JS-facing code in webgl.c calls.

uint32_t nx_webgl_egl_create_native_framebuffer(nx_webgl_egl_t *backend,
												 nx_canvas_t *canvas) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)canvas;
	return 0;
#else
	if (!backend || !canvas) return 0;
	if (!nx_webgl_egl_initialize(backend, canvas)) return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return 0;
	GLuint h = 0;
	glGenFramebuffers(1, &h);
	return (uint32_t)h;
#endif
}

void nx_webgl_egl_delete_native_framebuffer(nx_webgl_egl_t *backend,
                                             uint32_t handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle;
#else
	if (!backend || handle == 0) return;
	GLuint h = (GLuint)handle;
	// Unbind first if it's currently bound so subsequent draws don't keep
	// targeting a zombie FBO via the cached current_user_framebuffer field.
	if (backend->current_user_framebuffer == h) {
		backend->current_user_framebuffer = 0;
		backend->current_user_framebuffer_width = 0;
		backend->current_user_framebuffer_height = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	glDeleteFramebuffers(1, &h);
#endif
}

uint32_t nx_webgl_egl_create_native_renderbuffer(nx_webgl_egl_t *backend,
                                                  nx_canvas_t *canvas) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)canvas;
	return 0;
#else
	if (!backend || !canvas) return 0;
	if (!nx_webgl_egl_initialize(backend, canvas)) return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return 0;
	GLuint h = 0;
	glGenRenderbuffers(1, &h);
	return (uint32_t)h;
#endif
}

void nx_webgl_egl_delete_native_renderbuffer(nx_webgl_egl_t *backend,
                                              uint32_t handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle;
#else
	if (!backend || handle == 0) return;
	GLuint h = (GLuint)handle;
	glDeleteRenderbuffers(1, &h);
#endif
}

bool nx_webgl_egl_renderbuffer_storage(nx_webgl_egl_t *backend,
                                        uint32_t handle,
                                        uint32_t internalformat,
                                        int width, int height) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle; (void)internalformat;
	(void)width; (void)height;
	return false;
#else
	if (!backend || handle == 0 || width <= 0 || height <= 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)handle);
	(void)glGetError();
	glRenderbufferStorage(GL_RENDERBUFFER, (GLenum)internalformat, width,
	                      height);
	GLenum err = glGetError();
	return err == GL_NO_ERROR;
#endif
}

uint32_t nx_webgl_egl_create_persistent_texture(nx_webgl_egl_t *backend,
                                                 nx_canvas_t *canvas) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)canvas;
	return 0;
#else
	if (!backend || !canvas) return 0;
	if (!nx_webgl_egl_initialize(backend, canvas)) return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return 0;
	GLuint h = 0;
	glGenTextures(1, &h);
	return (uint32_t)h;
#endif
}

void nx_webgl_egl_delete_persistent_texture(nx_webgl_egl_t *backend,
                                             uint32_t handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle;
#else
	if (!backend || handle == 0) return;
	GLuint h = (GLuint)handle;
	glDeleteTextures(1, &h);
#endif
}

bool nx_webgl_egl_persistent_texture_image_2d(nx_webgl_egl_t *backend,
                                                uint32_t handle,
                                                int width, int height,
                                                uint32_t internalformat,
                                                uint32_t format,
                                                uint32_t type,
                                                const uint8_t *data,
                                                uint32_t min_filter,
                                                uint32_t mag_filter,
                                                uint32_t wrap_s,
                                                uint32_t wrap_t) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle; (void)width; (void)height;
	(void)internalformat; (void)format; (void)type; (void)data;
	(void)min_filter; (void)mag_filter; (void)wrap_s; (void)wrap_t;
	return false;
#else
	if (!backend || handle == 0 || width <= 0 || height <= 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindTexture(GL_TEXTURE_2D, (GLuint)handle);
	/* 2026-06-23 → 2026-06-24 (LIFTED): historically forced MIN/MAG=NEAREST
	 * for half-float / float textures because "LINEAR-filtered RGBA16F
	 * sampling crashes the driver in PMREM's prefilter pass". That
	 * diagnosis is now suspect — the actual crash root cause was the
	 * PMREMGGXConvolution FS body's uint/bitwise constructs (see
	 * [[reference-pmrem-tegra-compiler-workaround]]) which has since
	 * been worked around via FS replacement. The NEAREST-forcing
	 * caused real quality loss for HDR equirects (1k-2k textures alias
	 * badly with NEAREST). Trial-lifting in concert with the
	 * halfFloatDowngrade lift below — if PMREM regresses to driver
	 * crashes when HDR data enters the pipeline, restore both. */
	uint32_t min_filter_effective = min_filter;
	uint32_t mag_filter_effective = mag_filter;
	// Persistent textures may carry mipmap minFilter variants (set by JS
	// via tex_parameteri + generateMipmap). Pass them through to native
	// instead of collapsing — that's what makes real mipmap sampling
	// work. MAG_FILTER stays NEAREST/LINEAR per GLES spec.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                bridge_texture_filter_persistent(min_filter_effective));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
	                bridge_texture_filter(mag_filter_effective));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
	                bridge_texture_wrap(wrap_s));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
	                bridge_texture_wrap(wrap_t));
	(void)glGetError();
	// Promote unsized depth internalformats to their sized GLES3 equivalents.
	// Mesa-on-Tegra runs on a GLES3 context (per [[bridge-instancing-support]]),
	// and ES3 requires sized internalformats for depth+depth-stencil
	// textures. Three.js already passes the sized forms in its
	// WebGLTextures path, but external code (or older Three.js versions)
	// may pass the unsized form — auto-promote to avoid silent failure.
	GLint internal = (GLint)internalformat;
	if (format == 0x1902 /* GL_DEPTH_COMPONENT */) {
		if (internal == 0x1902 /* unsized */)
			internal = 0x81A5 /* GL_DEPTH_COMPONENT16 */;
	} else if (format == 0x84F9 /* GL_DEPTH_STENCIL */) {
		if (internal == 0x84F9 /* unsized */)
			internal = 0x88F0 /* GL_DEPTH24_STENCIL8 */;
	} else if (type == 0x1406 /* GL_FLOAT */) {
		// P2 (HDR): promote unsized RGBA / RGB to sized 32F variants for
		// FLOAT textures. ES3 requires sized internalformats for float
		// textures. Three.js's WebGL 1 path passes the unsized form;
		// auto-promote here.
		if (format == 0x1908 /* GL_RGBA */ && internal == 0x1908)
			internal = 0x8814 /* GL_RGBA32F */;
		else if (format == 0x1907 /* GL_RGB */ && internal == 0x1907)
			internal = 0x8815 /* GL_RGB32F */;
	} else if (type == 0x8D61 /* GL_HALF_FLOAT_OES */) {
		// Same for HALF_FLOAT_OES → RGBA16F / RGB16F. Also normalize the
		// type token: GLES3 native is GL_HALF_FLOAT (0x140B), which is what
		// the driver expects for the SIZED internal format path. The OES
		// token (0x8D61) only works with UNSIZED internal — but we just
		// promoted internal to sized, so swap the type too.
		if (format == 0x1908 /* GL_RGBA */ && internal == 0x1908)
			internal = 0x881A /* GL_RGBA16F */;
		else if (format == 0x1907 /* GL_RGB */ && internal == 0x1907)
			internal = 0x881B /* GL_RGB16F */;
	}
	// If we promoted to a sized FLOAT/HALF_FLOAT internalformat, the OES
	// type token must be swapped for the GLES3 core token, which the driver
	// requires for sized formats.
	GLenum native_type = (GLenum)type;
	if (type == 0x8D61 /* GL_HALF_FLOAT_OES */ &&
	    (internal == 0x881A /* RGBA16F */ || internal == 0x881B /* RGB16F */)) {
		native_type = 0x140B /* GL_HALF_FLOAT */;
	}
	// For UNSIGNED_INT_24_8_WEBGL the native enum is GL_UNSIGNED_INT_24_8
	// which has the same value (0x84FA). No remap needed.

	/* 2026-06-23 PMREM Tegra-compat downgrade: empirically Tegra GLES
	 * advertises EXT_color_buffer_half_float + OES_texture_half_float +
	 * OES_texture_half_float_linear but in practice cannot complete a
	 * sample+write cycle on a real RGBA16F / RGB16F texture — every
	 * PMREM prefilter draw that samples the equirect-to-cube RGBA16F
	 * result and writes the prefiltered RGBA16F result hard-crashes the
	 * driver. Downgrade NULL-source half-float / float texture
	 * allocations to RGBA8 / RGB8. The Three.js shader writes vec4
	 * floats either way; the framebuffer does the format conversion.
	 * HDR overshoot clamps to [0,1] — acceptable for the LDR equirect
	 * inputs we're using (HDR loaders aren't in libs/ anyway). Only
	 * downgrade NULL-source allocations (FBO RT init pattern). Data
	 * uploads pass through untouched so HDRLoader's RGBA16F image
	 * uploads (if ever added) preserve their data — they'd hit the
	 * same crash though, that's a separate problem.
	 *
	 * NOTE: webgl.c::nx_webgl_tex_image_2d allocates a `zero_buf` for
	 * null-source texImage2D so `data` is always non-null at this
	 * point. We can't distinguish FBO-RT allocation from real data
	 * upload here. Downgrade unconditionally — we have no HDRLoader
	 * in libs/ so no genuine HDR data flows through this path yet.
	 * If/when HDR loaders are added, this check needs to thread an
	 * is_null_source flag through. The data buffer's first N bytes
	 * (where N = w*h*bpp_halfFloat) gets interpreted as UByte instead
	 * of half-float; for the zero_buf case this is fine (zeros stay
	 * zeros). For genuine half-float pixel data this would corrupt;
	 * we accept that for now. */
	/* 2026-06-23 → 2026-06-24 (LIFTED): historically downgraded
	 * RGBA16F/32F → RGBA8 and RGB16F/32F → RGB8 unconditionally because
	 * "Tegra cannot complete a sample+write cycle on a real RGBA16F /
	 * RGB16F texture — every PMREM prefilter draw hard-crashes the
	 * driver". That was a misdiagnosis of the PMREMGGXConvolution FS
	 * uint/bitwise GLSL crash (now fixed via FS replacement,
	 * [[reference-pmrem-tegra-compiler-workaround]]). With the lift,
	 * HDR equirect → PMREM → IBL flows real half-float dynamic range
	 * through the prefilter; that's what gives MeshStandardMaterial
	 * its punchy specular highlights. The companion halfFloatFilter
	 * NEAREST forcing was lifted at the same time so Three.js's
	 * requested LINEAR filtering for HDR equirects is honored. The bg
	 * cube map still gets downgraded in
	 * nx_webgl_egl_persistent_cube_texture_image_2d (Fix G v3's UByte
	 * CPU-roundtrip assumes RGBA8 storage). */

	/* 2026-06-25 EXT_sRGB translation: Three.js's WebGL 1 path with
	 * `texture.colorSpace = SRGBColorSpace` uploads via the EXT_sRGB
	 * combo (internalformat == format == SRGB_EXT or SRGB_ALPHA_EXT,
	 * type=UByte). GLES3 requires (sized SRGB internalformat) + (unsized
	 * RGB/RGBA format). Translate so the driver does sRGB→linear decoding
	 * in the texture unit and Three.js's shader (which expects pre-decoded
	 * linear samples when EXT_sRGB is advertised) sees correct values.
	 * Phase 1.6 advertised EXT_sRGB but never wired the texImage2D path —
	 * surfaced 2026-06-25 as 4 demos rendering with black textures. */
	GLenum native_format = (GLenum)format;
	if (internal == 0x8C40 /* SRGB_EXT */) {
		internal = 0x8C41 /* SRGB8 */;
		native_format = GL_RGB;
	} else if (internal == 0x8C42 /* SRGB_ALPHA_EXT */) {
		internal = 0x8C43 /* SRGB8_ALPHA8 */;
		native_format = GL_RGBA;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0,
	             native_format, native_type, data);
	return glGetError() == GL_NO_ERROR;
#endif
}

#if NXJS_HAS_EGL_GLES
// Box-filter downsample of RGBA / RGB UByte image data: average 4 source
// pixels per channel into one destination pixel. Bounds-clamped on
// odd src dimensions so we never read past the source buffer; pow2
// inputs (the cubemap demos' 512×512 faces) hit the clean 2:1 case
// every level. Returns true if dst was filled; false on bad inputs.
static bool downsample_box_rgba_ubyte(const uint8_t *src, int src_w, int src_h,
                                      uint8_t *dst, int dst_w, int dst_h,
                                      int channels) {
	if (!src || !dst || src_w <= 0 || src_h <= 0 ||
	    dst_w <= 0 || dst_h <= 0 || channels <= 0 || channels > 4)
		return false;
	for (int y = 0; y < dst_h; y++) {
		int sy0 = y * 2;
		int sy1 = (sy0 + 1 < src_h) ? sy0 + 1 : sy0;
		for (int x = 0; x < dst_w; x++) {
			int sx0 = x * 2;
			int sx1 = (sx0 + 1 < src_w) ? sx0 + 1 : sx0;
			for (int c = 0; c < channels; c++) {
				int sum = (int)src[(sy0 * src_w + sx0) * channels + c] +
				          (int)src[(sy0 * src_w + sx1) * channels + c] +
				          (int)src[(sy1 * src_w + sx0) * channels + c] +
				          (int)src[(sy1 * src_w + sx1) * channels + c];
				dst[(y * dst_w + x) * channels + c] = (uint8_t)(sum / 4);
			}
		}
	}
	return true;
}
#endif

bool nx_webgl_egl_persistent_cube_texture_image_2d(nx_webgl_egl_t *backend,
                                                    uint32_t handle,
                                                    uint32_t face_target,
                                                    int width, int height,
                                                    uint32_t internalformat,
                                                    uint32_t format,
                                                    uint32_t type,
                                                    const uint8_t *data,
                                                    uint32_t min_filter,
                                                    uint32_t mag_filter,
                                                    uint32_t wrap_s,
                                                    uint32_t wrap_t) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle; (void)face_target;
	(void)width; (void)height;
	(void)internalformat; (void)format; (void)type; (void)data;
	(void)min_filter; (void)mag_filter; (void)wrap_s; (void)wrap_t;
	return false;
#else
	if (!backend || handle == 0 || width <= 0 || height <= 0) return false;
	if (face_target < 0x8515 /* GL_TEXTURE_CUBE_MAP_POSITIVE_X */ ||
	    face_target > 0x851A /* GL_TEXTURE_CUBE_MAP_NEGATIVE_Z */)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)handle);
	// Filter/wrap apply to the whole cube map; re-applying per face is
	// idempotent and saves the caller from sequencing this with the first
	// face upload. mip-aware MIN_FILTER stays here so post-upload sampling
	// at higher LODs hits the manually-emitted mipmap chain below.
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
	                bridge_texture_filter_persistent(min_filter));
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
	                bridge_texture_filter(mag_filter));
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
	                bridge_texture_wrap(wrap_s));
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
	                bridge_texture_wrap(wrap_t));
	(void)glGetError();
	// Use sized internal format for cube uploads. Mesa Nouveau is
	// stricter about this than the 2D path: discovered while bringing up
	// milestone #25 ([[swb-threejs-webgl-materials-cubemap]]).
	GLint internal = (GLint)internalformat;
	if (format == 0x1908 /* GL_RGBA */ && type == GL_UNSIGNED_BYTE &&
	    internal == 0x1908 /* unsized GL_RGBA */) {
		internal = 0x8058 /* GL_RGBA8 */;
	} else if (format == 0x1907 /* GL_RGB */ && type == GL_UNSIGNED_BYTE &&
	           internal == 0x1907 /* unsized GL_RGB */) {
		internal = 0x8051 /* GL_RGB8 */;
	}
	/* 2026-06-23 Tegra/Mesa cube-map FBO bug: when Three.js's
	 * WebGLCubeRenderTarget allocates a cube map with SRGB8_ALPHA8
	 * (0x8C43) internal format and uses it as a render target for
	 * CubemapFromEquirect (6 face draws into the cube), only the first
	 * face (POSITIVE_X) receives content — faces 2-6 silently produce
	 * (0,0,0,0). Verified via direct readback: equirect source is intact
	 * across all 6 draws, sampler bindings are identical, only the
	 * FBO/face attachment differs. Mesa Nouveau on Tegra appears to have
	 * a bug specific to multi-face writes into SRGB8_ALPHA8 cube maps.
	 * Downgrade SRGB8_ALPHA8 → RGBA8 to avoid the bug. Trade-off: the
	 * cube map stores linear values instead of sRGB-encoded values, so
	 * downstream sampling skips the sRGB→linear conversion. Three.js's
	 * background path will see slightly-dark colors but at least real
	 * content. */
	if (internal == 0x8C43 /* SRGB8_ALPHA8 */) {
		static int dg_n = 0;
		if (dg_n < 20) {
			dg_n++;
			fprintf(stderr,
				"[nxjs:cube-fix:srgbDowngrade] handle=%u face=0x%x SRGB8_ALPHA8\xe2\x86\x92RGBA8\n",
				(unsigned)handle, (unsigned)face_target);
			fflush(stderr);
		}
		internal = 0x8058 /* RGBA8 */;
	}
	/* 2026-06-24 Cube-map HDR downgrade: when Three.js's WebGLBackground
	 * runs CubemapFromEquirect on an HDR equirect (HalfFloatType source),
	 * the destination cube_map is allocated as RGBA16F to preserve range.
	 * That conflicts with Fix G v3's CPU-roundtrip rescue
	 * (cube_face_rescue_transition above) which assumes 4-byte RGBA UByte
	 * storage in glReadPixels + glTexSubImage2D — reading half-float
	 * storage as UByte gives garbage, so the rescue writes garbage to
	 * face N>0 and only POSITIVE_X shows correct content. Downgrade
	 * RGBA16F/32F → RGBA8 here so the bg cube is UByte-storage, the
	 * rescue's format assumption stays valid, and the background renders
	 * correctly on all 6 faces. The PMREM cube_uv (2D atlas, separate
	 * texture) keeps its half-float storage and gets the full HDR
	 * dynamic range for IBL — that's where the "shiny" comes from. The
	 * background loses HDR range but that's just a backdrop; the
	 * Mesa-Nouveau aliasing bug forces this trade. */
	if (internal == 0x881A /* RGBA16F */ ||
	    internal == 0x8814 /* RGBA32F */) {
		static int dg_n = 0;
		if (dg_n < 20) {
			dg_n++;
			fprintf(stderr,
				"[nxjs:cube-fix:hdrDowngrade] handle=%u face=0x%x RGBA16F/32F\xe2\x86\x92RGBA8 (FixG-compat)\n",
				(unsigned)handle, (unsigned)face_target);
			fflush(stderr);
		}
		internal = 0x8058 /* RGBA8 */;
		format = 0x1908 /* RGBA */;
		type = 0x1401 /* UNSIGNED_BYTE */;
	}
	/* 2026-06-23 Fix-attempt F: glTexStorage2D for NULL-data cube alloc.
	 * Probe data shows that mutable-storage cube maps allocated via 6
	 * per-face glTexImage2D(NULL) calls have a Mesa-Nouveau bug where
	 * ALL writes (clear, draw, blit) silently redirect to POSITIVE_X.
	 * Immutable storage is a different driver code path that may not
	 * exhibit the aliasing. We track per-handle immutable state in a
	 * static array; on first NULL-data call we switch to glTexStorage2D
	 * for the whole cube, then subsequent NULL-data calls are no-op'd.
	 * CPU-data uploads (CubeTextureLoader path) keep using glTexImage2D
	 * so face mipmap chain logic below stays valid for those callers. */
	static bool g_cube_immutable[256] = {0};
	bool used_immutable = false;
	GLenum err = GL_NO_ERROR;
	if (!data && handle < 256) {
		if (!g_cube_immutable[handle] && backend->fn_tex_storage_2d) {
			typedef void (*pfn_ts2d_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
			(void)glGetError();
			((pfn_ts2d_t)backend->fn_tex_storage_2d)(GL_TEXTURE_CUBE_MAP, 1,
				(GLenum)internal, (GLsizei)width, (GLsizei)height);
			err = glGetError();
			if (err == GL_NO_ERROR) {
				g_cube_immutable[handle] = true;
				used_immutable = true;
			}
			/* If glTexStorage2D failed, fall back to glTexImage2D. */
		} else if (g_cube_immutable[handle]) {
			/* Already immutable. NULL-data call is a no-op. */
			used_immutable = true;
			err = GL_NO_ERROR;
		}
	}
	if (!used_immutable) {
		glTexImage2D((GLenum)face_target, 0, internal, width, height,
		             0, (GLenum)format, (GLenum)type, data);
		err = glGetError();
	} else if (data && handle < 256 && g_cube_immutable[handle]) {
		/* Immutable cube with CPU data — use texSubImage2D for the upload. */
		(void)glGetError();
		glTexSubImage2D((GLenum)face_target, 0, 0, 0, width, height,
		                (GLenum)format, (GLenum)type, data);
		err = glGetError();
	}
	(void)used_immutable;
	if (err != GL_NO_ERROR) return false;

	// Software mipmap chain. Mesa Nouveau's `glGenerateMipmap(GL_TEXTURE_CUBE_MAP)`
	// silently no-ops even when the cube is complete and uses a sized
	// internalformat — confirmed during milestone #25 hw bring-up: with
	// driver-side generation, head materials (which sample at LOD ≥ 1 due
	// to curvature) read (0,0,0) while the background quad (LOD 0) works.
	// We sidestep the driver entirely by computing the box-filtered chain
	// CPU-side and uploading each level via glTexImage2D — deterministic
	// and works on every GLES driver. ~33 % memory overhead per face,
	// acceptable for cube textures.
	//
	// Only fires when caller actually supplied pixels AND the format is
	// one of our supported UByte tuples. NULL-data uploads (cube render
	// targets) skip — they're written into by FBO attachments, not
	// sampled with mipmaps in our current demos.
	if (data && type == GL_UNSIGNED_BYTE &&
	    (format == 0x1908 /* GL_RGBA */ || format == 0x1907 /* GL_RGB */)) {
		int channels = (format == 0x1907 /* GL_RGB */) ? 3 : 4;
		int w = width, h = height;
		int level = 1;
		const uint8_t *src = data;
		uint8_t *src_alloc = NULL;
		while (w > 1 || h > 1) {
			int new_w = w > 1 ? w / 2 : 1;
			int new_h = h > 1 ? h / 2 : 1;
			uint8_t *dst = (uint8_t *)malloc((size_t)new_w * (size_t)new_h *
			                                  (size_t)channels);
			if (!dst) {
				if (src_alloc) free(src_alloc);
				return false;
			}
			if (!downsample_box_rgba_ubyte(src, w, h, dst, new_w, new_h,
			                                channels)) {
				free(dst);
				if (src_alloc) free(src_alloc);
				return false;
			}
			(void)glGetError();
			glTexImage2D((GLenum)face_target, level, internal, new_w, new_h,
			             0, (GLenum)format, (GLenum)type, dst);
			err = glGetError();
			if (err != GL_NO_ERROR) {
				free(dst);
				if (src_alloc) free(src_alloc);
				return false;
			}
			// Free previous level's source buffer; promote dst to next
			// iteration's source (we keep ownership until the next
			// allocation succeeds or we exit the loop).
			if (src_alloc) free(src_alloc);
			src_alloc = dst;
			src = dst;
			w = new_w;
			h = new_h;
			level++;
		}
		if (src_alloc) free(src_alloc);
	}
	return true;
#endif
}

uint32_t nx_webgl_egl_check_framebuffer_status(nx_webgl_egl_t *backend,
                                                 uint32_t handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle;
	return 0;
#else
	if (!backend) return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return 0;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)handle);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	// Restore whichever FBO was bound so we don't surprise downstream
	// draws by changing target as a side effect of a status query.
	GLuint restore = backend->current_user_framebuffer
	                     ? backend->current_user_framebuffer
	                     : backend->bridge_framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, restore);
	return (uint32_t)status;
#endif
}

bool nx_webgl_egl_framebuffer_texture_2d(nx_webgl_egl_t *backend,
                                          uint32_t framebuffer_handle,
                                          uint32_t attachment,
                                          uint32_t textarget,
                                          uint32_t texture_handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)framebuffer_handle; (void)attachment;
	(void)textarget; (void)texture_handle;
	return false;
#else
	if (!backend || framebuffer_handle == 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)framebuffer_handle);
	(void)glGetError();
	/* Mesa-Nouveau cube-face workaround: pre-detach by calling
	 * glFramebufferTexture2D with tex=0 before the actual attach, and
	 * explicitly bind the cube map as current. Cheap and idempotent;
	 * the real cube-multi-face FBO write fix is Fix G (see post-draw
	 * rescue in nx_webgl_egl_dispatch_raw_pass_through). */
	bool is_cube_face = (textarget >= 0x8515 && textarget <= 0x851A);
	if (is_cube_face && texture_handle != 0) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum)attachment,
		                       (GLenum)textarget, 0, 0);
		(void)glGetError();
		glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)texture_handle);
		(void)glGetError();
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum)attachment,
	                       (GLenum)textarget, (GLuint)texture_handle, 0);
	GLenum err = glGetError();
	GLuint restore = backend->current_user_framebuffer
	                     ? backend->current_user_framebuffer
	                     : backend->bridge_framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, restore);
	return err == GL_NO_ERROR;
#endif
}

bool nx_webgl_egl_framebuffer_renderbuffer(nx_webgl_egl_t *backend,
                                            uint32_t framebuffer_handle,
                                            uint32_t attachment,
                                            uint32_t renderbuffer_handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)framebuffer_handle; (void)attachment;
	(void)renderbuffer_handle;
	return false;
#else
	if (!backend || framebuffer_handle == 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)framebuffer_handle);
	(void)glGetError();
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, (GLenum)attachment,
	                          GL_RENDERBUFFER, (GLuint)renderbuffer_handle);
	GLenum err = glGetError();
	GLuint restore = backend->current_user_framebuffer
	                     ? backend->current_user_framebuffer
	                     : backend->bridge_framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, restore);
	return err == GL_NO_ERROR;
#endif
}

/* 2026-06-24 Fix G v3: per-face-transition rescue for Mesa Nouveau cube-map
 * write aliasing. Called from `nx_webgl_egl_set_user_framebuffer` whenever
 * the bound user FBO changes. If the OLD FBO had a cube face attached as its
 * color attachment, this is the moment to rescue the previous face — all the
 * draws (and clear) that targeted the OLD FBO have completed (we already
 * called glFinish), and the NEW FBO's clear hasn't yet wiped face-0 storage.
 *
 * Per-face semantics:
 *   - Face 0 (POSITIVE_X): snapshot face-0 storage into per-cube backup 2D tex.
 *   - Face N>0: face-0 storage holds the aliased face-N content; copy it into
 *     face-N's real storage, then restore the backup into face-0's storage.
 *
 * Both `glCopyTexSubImage2D` and `glTexSubImage2D` writes to face N work
 * correctly on Mesa Nouveau — only FBO color writes (draw/clear) alias.
 *
 * State held in static arrays indexed by cube-texture handle (max 256). Not
 * freed on texture delete — acceptable lifetime leak, see memory entry
 * [[reference-mesa-cube-face-aliasing-rescue]]. */
#if NXJS_HAS_EGL_GLES
/* 2026-06-24 Fix G v3 Y-flip helper: glReadPixels reads from an FBO with
 * the OpenGL lower-left origin convention. glTexSubImage2D to a cube face
 * writes with the cube-face upper-left origin convention. Without an
 * explicit row reverse in between, every cube face Fix G writes lands
 * upside-down — which surfaced visibly the moment MeshStandardMaterial+
 * PMREM forced Three.js's WebGLBackground through the CubemapFromEquirect
 * path (older Phong-with-equirect demos sidestepped this because they
 * never triggered the cube-face rescue). 4 BPP fixed since the cube
 * downgrade above forces RGBA8 storage. */
static void cube_face_rescue_flip_y_rgba8(uint8_t *buf, int w, int h) {
	if (!buf || w <= 0 || h <= 0) return;
	size_t row_bytes = (size_t)w * 4;
	uint8_t *tmp = (uint8_t *)malloc(row_bytes);
	if (!tmp) return;
	for (int y = 0; y < h / 2; y++) {
		uint8_t *r0 = buf + (size_t)y * row_bytes;
		uint8_t *r1 = buf + (size_t)(h - 1 - y) * row_bytes;
		memcpy(tmp, r0, row_bytes);
		memcpy(r0, r1, row_bytes);
		memcpy(r1, tmp, row_bytes);
	}
	free(tmp);
}

static void cube_face_rescue_transition(GLuint old_fbo_handle,
                                         int old_width, int old_height) {
	/* Per-cube CPU staging buffers + size record. We use CPU round-trip
	 * (glReadPixels + glTexSubImage2D) for cube-face writes because
	 * glCopyTexSubImage2D writes to face N>0 do NOT reach the texture's
	 * sampler-visible storage on Mesa Nouveau (likely lands in a separate
	 * FBO color-buffer staging that's tied to face 0's storage via
	 * aliasing and never resolved back to the texture). glTexSubImage2D
	 * IS verified to land in the sampler-visible storage — that's how the
	 * mipmap-software-generation path in persistent_cube_texture_image_2d
	 * uploads face content, and the diagnostic probe we ran confirmed it
	 * shows up on the reflective sphere.
	 *
	 * Reads via glReadPixels from face 0 storage DO return the actual
	 * aliased content (verified via center-pixel readback). So the read
	 * side works correctly through any of: original FBO, scratch FBO with
	 * cube face attached, scratch FBO with 2D backup attached. Picked the
	 * scratch_fbo path here since we already build it for query. */
	static uint8_t *g_cube_backup_cpu[256] = {0};
	static int      g_cube_backup_size[256] = {0};

	if (old_fbo_handle == 0) return;
	if (old_width <= 0 || old_height <= 0) return;

	/* Save state we'll mutate. We're called from set_user_framebuffer so
	 * the GL context is current but no specific FBO is guaranteed bound. */
	GLint saved_read_fb = 0, saved_draw_fb = 0;
	GLint saved_active_tu = 0;
	GLint saved_tex2d = 0, saved_texcube = 0;
	glGetIntegerv(0x8CA8 /* GL_READ_FRAMEBUFFER_BINDING */, &saved_read_fb);
	glGetIntegerv(0x8CA6 /* GL_DRAW_FRAMEBUFFER_BINDING */, &saved_draw_fb);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active_tu);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_tex2d);
	glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &saved_texcube);

	/* Explicitly bind OLD FBO so the attachment query reports its color
	 * attachment (not whatever happened to be currently bound). */
	glBindFramebuffer(GL_FRAMEBUFFER, old_fbo_handle);

	GLint at_type = 0, at_obj = 0, at_face = 0;
	(void)glGetError();
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		0x8CD0 /* FBO_ATTACHMENT_OBJECT_TYPE */, &at_type);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		0x8CD1 /* FBO_ATTACHMENT_OBJECT_NAME */, &at_obj);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		0x8CD3 /* FBO_ATTACHMENT_TEXTURE_CUBE_MAP_FACE */, &at_face);
	(void)glGetError();

	bool is_cube = (at_type == GL_TEXTURE && at_obj > 0 &&
	                at_face >= 0x8515 && at_face <= 0x851A &&
	                at_obj < 256);
	if (!is_cube) {
		/* Restore state and exit — old FBO wasn't a cube-face attachment. */
		glActiveTexture((GLenum)saved_active_tu);
		glBindTexture(GL_TEXTURE_2D, (GLuint)saved_tex2d);
		glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)saved_texcube);
		glBindFramebuffer(0x8CA8, (GLuint)saved_read_fb);
		glBindFramebuffer(0x8CA9, (GLuint)saved_draw_fb);
		return;
	}

	GLuint cube_tex = (GLuint)at_obj;
	int face_idx = (int)(at_face - 0x8515);

	/* Cube face dimensions come from the OLD FBO's tracked dims (set by
	 * `bindFramebuffer` from the JS-side `nx_webgl_framebuffer_t.width/height`
	 * which were derived from the color attachment at attach time). This is
	 * the SOURCE OF TRUTH — viewport is unreliable since the bridge resets
	 * it on each draw and at hook time it may reflect the previous draw's
	 * dims rather than the current cube face. */
	int size = old_width > old_height ? old_width : old_height;

	/* Lazy-allocate per-cube CPU backup buffer (size from OLD FBO tracked
	 * dims — same for all faces of one cube). RGBA UByte = 4 bytes/pixel. */
	if (g_cube_backup_cpu[cube_tex] == NULL) {
		g_cube_backup_cpu[cube_tex] = (uint8_t *)malloc((size_t)size *
		                                                 (size_t)size * 4);
		g_cube_backup_size[cube_tex] = size;
		if (g_cube_backup_cpu[cube_tex] == NULL) {
			/* OOM — bail without rescue. Cube reflection will be wrong but
			 * the demo won't crash. */
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glActiveTexture((GLenum)saved_active_tu);
			glBindTexture(GL_TEXTURE_2D, (GLuint)saved_tex2d);
			glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)saved_texcube);
			glBindFramebuffer(0x8CA8, (GLuint)saved_read_fb);
			glBindFramebuffer(0x8CA9, (GLuint)saved_draw_fb);
			return;
		}
	}
	int backup_size = g_cube_backup_size[cube_tex];
	uint8_t *backup_cpu = g_cube_backup_cpu[cube_tex];
	(void)glGetError();

	/* Build a scratch READ FBO with face-0 of cube_tex attached. All
	 * face-0-storage reads (both snapshot and rescue step 1) go through
	 * this. Reads are confirmed to return the aliased content via memory
	 * entry's P6 probe + this session's center-pixel diagnostic. */
	GLuint scratch_fbo = 0;
	glGenFramebuffers(1, &scratch_fbo);
	glBindFramebuffer(0x8CA8 /* GL_READ_FRAMEBUFFER */, scratch_fbo);
	glFramebufferTexture2D(0x8CA8, GL_COLOR_ATTACHMENT0,
	                       0x8515 /* POSITIVE_X */, cube_tex, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if (face_idx == 0) {
		/* Face 0 just finished its draws. Snapshot face-0 storage into the
		 * per-cube CPU backup buffer for use by subsequent face N>0
		 * rescues this cycle. */
		(void)glGetError();
		glReadPixels(0, 0, backup_size, backup_size,
		             GL_RGBA, GL_UNSIGNED_BYTE, backup_cpu);
		(void)glGetError();
		cube_face_rescue_flip_y_rgba8(backup_cpu, backup_size, backup_size);
		static int g_n = 0;
		if (g_n < 3) {
			g_n++;
			fprintf(stderr,
				"[nxjs:cube-fix:G-snapshot] n=%d cubeTex=%u size=%d\n",
				g_n, cube_tex, backup_size);
			fflush(stderr);
		}
	} else {
		/* Face N>0 finished. Sequence:
		 *  1. glReadPixels face-0 storage (aliased face-N content) → CPU
		 *     stage buffer (allocated here, freed after use).
		 *  2. glTexSubImage2D backup_cpu → face-0 storage. Restores face 0
		 *     so face 0's draws survive the rest of the cycle.
		 *  3. glTexSubImage2D stage CPU buffer → face N storage. Lands in
		 *     sampler-visible storage (unlike glCopyTexSubImage2D which on
		 *     Mesa Nouveau writes to a non-sampler-visible color buffer for
		 *     cube face N>0 destinations). */
		/* Strategy split by cube size:
		 *  - LARGE (>=512): typically Three.js's CubemapFromEquirect bg
		 *    cube derived from an equirect's height (e.g. 1024 from a
		 *    2048×1024 equirect). Reads at face-0 storage DO capture the
		 *    aliased face-N content for these — write read-result to face
		 *    N so the cube samples correctly per direction. Produces a
		 *    proper 360° equirect background.
		 *  - SMALL (<512): typically a CubeCamera dynamic RT (e.g. 128).
		 *    Reads return mostly zeros + sparse junk; writing that
		 *    produces visible trails of historical sprite positions.
		 *    Write backup (face 0 view) to face N instead — sphere
		 *    reflects face-0-view from every direction, no trails. */
		size_t buf_bytes = (size_t)backup_size * (size_t)backup_size * 4;
		const uint8_t *face_n_src = backup_cpu;
		uint8_t *stage_cpu = NULL;
		if (backup_size >= 512) {
			stage_cpu = (uint8_t *)malloc(buf_bytes);
			if (stage_cpu) {
				(void)glGetError();
				glReadPixels(0, 0, backup_size, backup_size,
				             GL_RGBA, GL_UNSIGNED_BYTE, stage_cpu);
				(void)glGetError();
				cube_face_rescue_flip_y_rgba8(stage_cpu, backup_size, backup_size);
				face_n_src = stage_cpu;
			}
		}

		glBindTexture(GL_TEXTURE_CUBE_MAP, cube_tex);
		(void)glGetError();
		glTexSubImage2D(0x8515 /* POSITIVE_X */, 0, 0, 0,
		                backup_size, backup_size,
		                GL_RGBA, GL_UNSIGNED_BYTE, backup_cpu);
		(void)glGetError();
		/* 2026-06-24 cube-face-Y-pair swap: the global Y-flip applied to
		 * every glReadPixels above is geometrically correct for the four
		 * side faces (±X, ±Z) whose image-space "up" is world +Y, so
		 * inverting the rows matches the FBO-vs-texture origin
		 * convention difference. For the ±Y faces, image-space "up" is
		 * along ±Z (Three.js's per-face camera up vector flips), and a
		 * vertical row reverse on those images effectively turns the
		 * +Y image into -Y image content (and vice versa). To
		 * compensate, the rescue redirects POSITIVE_Y writes to
		 * NEGATIVE_Y and vice versa. */
		GLenum write_face = (GLenum)at_face;
		if (at_face == 0x8517 /* POSITIVE_Y */)       write_face = 0x8518 /* NEGATIVE_Y */;
		else if (at_face == 0x8518 /* NEGATIVE_Y */)  write_face = 0x8517 /* POSITIVE_Y */;
		glTexSubImage2D(write_face, 0, 0, 0,
		                backup_size, backup_size,
		                GL_RGBA, GL_UNSIGNED_BYTE, face_n_src);
		(void)glGetError();

		if (stage_cpu) free(stage_cpu);

		static int g_n = 0;
		if (g_n < 6) {
			g_n++;
			fprintf(stderr,
				"[nxjs:cube-fix:G-rescue] n=%d cubeTex=%u faceIdx=%d "
				"size=%d strategy=%s\n",
				g_n, cube_tex, face_idx, backup_size,
				backup_size >= 512 ? "read" : "backup");
			fflush(stderr);
		}
	}

	glDeleteFramebuffers(1, &scratch_fbo);

	/* Restore state we mutated */
	glActiveTexture((GLenum)saved_active_tu);
	glBindTexture(GL_TEXTURE_2D, (GLuint)saved_tex2d);
	glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)saved_texcube);
	glBindFramebuffer(0x8CA8, (GLuint)saved_read_fb);
	glBindFramebuffer(0x8CA9, (GLuint)saved_draw_fb);
}
#endif

void nx_webgl_egl_set_user_framebuffer(nx_webgl_egl_t *backend,
                                        uint32_t handle, int width,
                                        int height) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle; (void)width; (void)height;
#else
	if (!backend) return;
	// When transitioning AWAY from a user FBO (changing target or
	// returning to default), force a flush so writes to the previous
	// FBO's color and depth attachments are visible to subsequent
	// sample-from-texture operations. Tegra's tiled rasterizer can
	// otherwise leave depth writes in tile-local memory that the next
	// pass's `texture2D(depthTex, ...)` returns as 0. Spec says this
	// hazard should be handled implicitly but Mesa-on-Tegra doesn't.
	// Symptom: milestone #19.5's webgl_depth_texture demo rendered the
	// scene correctly into the color attachment but the depth-sampling
	// post-pass returned 0 everywhere → uniform white output.
	if (backend->current_user_framebuffer != 0 &&
	    backend->current_user_framebuffer != (GLuint)handle) {
		if (eglMakeCurrent(backend->display, backend->surface,
		                    backend->surface, backend->context)) {
			// glFlush() suffices for milestone #19.5's depth-texture sample,
			// but it's NOT enough for MRT color sampling on Mesa Nouveau:
			// after a draw to a 2-attachment FBO, sampling either attachment
			// in a subsequent pass returns (0,0,0,0) unless we force full
			// completion. glFinish() blocks until all prior commands actually
			// retire, including the per-tile resolve into the texture's
			// memory. This is the standard "MRT writeback → sample-as-texture"
			// hazard the GLES spec says drivers should handle implicitly.
			glFinish();
			/* 2026-06-24 Fix G v3: cube-face-aliasing rescue. Runs once per
			 * FBO transition so the multi-draw-per-face case (CubeCamera with
			 * N scene objects per face) works as well as the single-draw case
			 * (CubemapFromEquirect). See cube_face_rescue_transition above. */
			cube_face_rescue_transition(
				backend->current_user_framebuffer,
				backend->current_user_framebuffer_width,
				backend->current_user_framebuffer_height);
		}
	}
	// 2026-06-07 pvzge investigation: originally logged every transition
	// between user FBO and bridge default (handle 0 ↔ non-0). For apps
	// that bind FBOs every frame (Three.js shadow pass, post-processing,
	// MRT) this floods nxjs-debug.log with hundreds of lines per second
	// (the Rapier3d demo hit ~120 lines/sec, ~130 KB per minute). Rate-
	// limited to first 20 + every 500th so steady-state apps stay quiet
	// but Cocos-style anomaly debugging still gets a sample stream.
	{
		static int fbN = 0;
		++fbN;
		GLuint prev = backend->current_user_framebuffer;
		if (fbN <= 20 || (fbN % 500) == 0)
			fprintf(stderr,
				"[nxjs:set-user-fb] n=%d prev=%u next=%u size=%dx%d\n",
				fbN, (unsigned)prev, (unsigned)handle, width, height);
	}
	backend->current_user_framebuffer = (GLuint)handle;
	backend->current_user_framebuffer_width = width;
	backend->current_user_framebuffer_height = height;
#endif
}

void nx_webgl_egl_forward_active_texture(nx_webgl_egl_t *backend,
                                          uint32_t unit) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)unit;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glActiveTexture((GLenum)unit);
#endif
}

void nx_webgl_egl_forward_bind_texture(nx_webgl_egl_t *backend,
                                        uint32_t target, uint32_t handle) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)target; (void)handle;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glBindTexture((GLenum)target, (GLuint)handle);
#endif
}

// Bind the given persistent texture handle to GL_TEXTURE_2D and apply
// the parameter. Used by `nx_webgl_tex_parameteri` to forward filter/wrap
// changes to native GLES when the texture has been promoted to a
// persistent handle (so subsequent draws sample with the new params).
// See [[swb-threejs-webgl-materials-texture-filters]] milestone #24.
void nx_webgl_egl_texture_set_parameteri(nx_webgl_egl_t *backend,
                                          uint32_t target,
                                          uint32_t handle, uint32_t pname,
                                          uint32_t param) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)target; (void)handle; (void)pname; (void)param;
#else
	if (!backend || !handle) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glBindTexture((GLenum)target, (GLuint)handle);
	glTexParameteri((GLenum)target, (GLenum)pname, (GLint)param);
#endif
}

// Bind the given persistent texture handle to GL_TEXTURE_2D and call
// native glGenerateMipmap to fill levels 1..N from the level-0 texels.
// Caller must have already promoted the texture (`gles_handle != 0`)
// and uploaded the base level via `nx_webgl_egl_persistent_texture_image_2d`.
// Milestone #24.
void nx_webgl_egl_generate_mipmap(nx_webgl_egl_t *backend,
                                    uint32_t handle, uint32_t target) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)handle; (void)target;
#else
	if (!backend || !handle) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glBindTexture((GLenum)target, (GLuint)handle);
	glGenerateMipmap((GLenum)target);
#endif
}

void nx_webgl_egl_texture_set_parameterf(nx_webgl_egl_t *backend,
                                          uint32_t target,
                                          uint32_t handle, uint32_t pname,
                                          float param) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)target; (void)handle; (void)pname; (void)param;
#else
	if (!backend || !handle) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glBindTexture((GLenum)target, (GLuint)handle);
	glTexParameterf((GLenum)target, (GLenum)pname, (GLfloat)param);
#endif
}

// ============================================================================
// Method-binding pass (2026-06-26): finish / flush / vertexAttrib*f /
// getVertexAttrib / getUniform forwarders. Each is the standard
// eglMakeCurrent + native-call pattern.
// ============================================================================

void nx_webgl_egl_finish(nx_webgl_egl_t *backend) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glFinish();
#endif
}

void nx_webgl_egl_flush(nx_webgl_egl_t *backend) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glFlush();
#endif
}

void nx_webgl_egl_vertex_attrib_1f(nx_webgl_egl_t *backend, uint32_t index,
                                    float x) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)x;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glVertexAttrib1f((GLuint)index, (GLfloat)x);
#endif
}

void nx_webgl_egl_vertex_attrib_2f(nx_webgl_egl_t *backend, uint32_t index,
                                    float x, float y) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)x; (void)y;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glVertexAttrib2f((GLuint)index, (GLfloat)x, (GLfloat)y);
#endif
}

void nx_webgl_egl_vertex_attrib_3f(nx_webgl_egl_t *backend, uint32_t index,
                                    float x, float y, float z) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)x; (void)y; (void)z;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glVertexAttrib3f((GLuint)index, (GLfloat)x, (GLfloat)y, (GLfloat)z);
#endif
}

void nx_webgl_egl_vertex_attrib_4f(nx_webgl_egl_t *backend, uint32_t index,
                                    float x, float y, float z, float w) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)x; (void)y; (void)z; (void)w;
#else
	if (!backend) return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return;
	glVertexAttrib4f((GLuint)index, (GLfloat)x, (GLfloat)y, (GLfloat)z,
	                  (GLfloat)w);
#endif
}

bool nx_webgl_egl_get_vertex_attrib_fv(nx_webgl_egl_t *backend,
                                        uint32_t index, uint32_t pname,
                                        float *out) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)pname; (void)out;
	return false;
#else
	if (!backend || !out) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glGetVertexAttribfv((GLuint)index, (GLenum)pname, (GLfloat *)out);
	return true;
#endif
}

bool nx_webgl_egl_get_vertex_attrib_iv(nx_webgl_egl_t *backend,
                                        uint32_t index, uint32_t pname,
                                        int *out) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)index; (void)pname; (void)out;
	return false;
#else
	if (!backend || !out) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glGetVertexAttribiv((GLuint)index, (GLenum)pname, (GLint *)out);
	return true;
#endif
}

bool nx_webgl_egl_get_uniform_fv(nx_webgl_egl_t *backend,
                                  uint32_t program_handle, int location,
                                  float *out) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)program_handle; (void)location; (void)out;
	return false;
#else
	if (!backend || !program_handle || !out || location < 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glGetUniformfv((GLuint)program_handle, (GLint)location, (GLfloat *)out);
	return true;
#endif
}

bool nx_webgl_egl_get_uniform_iv(nx_webgl_egl_t *backend,
                                  uint32_t program_handle, int location,
                                  int *out) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)program_handle; (void)location; (void)out;
	return false;
#else
	if (!backend || !program_handle || !out || location < 0) return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glGetUniformiv((GLuint)program_handle, (GLint)location, (GLint *)out);
	return true;
#endif
}

bool nx_webgl_egl_read_user_fbo_pixels(nx_webgl_egl_t *backend,
                                        uint32_t fbo_handle,
                                        int x, int y, int width, int height,
                                        uint32_t format, uint32_t type,
                                        uint8_t *dst) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)fbo_handle;
	(void)x; (void)y; (void)width; (void)height;
	(void)format; (void)type; (void)dst;
	return false;
#else
	if (!backend || !dst || width <= 0 || height <= 0)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
	                    backend->context))
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo_handle);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	(void)glGetError();
	glReadPixels(x, y, width, height, (GLenum)format, (GLenum)type, dst);
	GLenum err = glGetError();
	// Note: we do NOT restore the previous FBO binding here. Subsequent
	// bridge dispatch consults `current_user_framebuffer` and re-binds
	// itself via bridge_bind_target, and the FBO we bound IS the user's
	// currently-bound FBO (per the caller's contract), so the binding is
	// still correct by construction.
	return err == GL_NO_ERROR;
#endif
}

uint32_t nx_webgl_egl_get_last_draw_gl_error(nx_webgl_egl_t *backend) {
	if (!backend)
		return 0;
#if NXJS_HAS_EGL_GLES
	return backend->bridge_last_draw_gl_error;
#else
	return 0;
#endif
}

bool nx_webgl_egl_has_pending_readback(nx_webgl_egl_t *backend) {
	if (!backend)
		return false;
#if NXJS_HAS_EGL_GLES
	return backend->bridge_pending_readback;
#else
	return false;
#endif
}

bool nx_webgl_egl_flush_bridge_present(nx_webgl_egl_t *backend,
									   nx_canvas_t *canvas) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	return false;
#else
	if (!backend || !backend->bridge_pending_readback)
		return false;
	if (!canvas || !canvas->data || canvas->width == 0 || canvas->height == 0)
		return false;
	if (!backend->bridge_framebuffer || !backend->bridge_readback)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge flush: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glReadPixels(0, 0, backend->bridge_width, backend->bridge_height, GL_RGBA,
				 GL_UNSIGNED_BYTE, backend->bridge_readback);
	GLenum error = glGetError();
	glFinish();
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge flush readback failed: 0x%x", error);
		backend->bridge_pending_readback = false;
		return false;
	}
	copy_rgba_readback_to_canvas_scaled(canvas, backend->bridge_readback,
										backend->bridge_width,
										backend->bridge_height);
	backend->bridge_pending_readback = false;
	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
	return true;
#endif
}

bool nx_webgl_egl_clear_bridge(nx_webgl_egl_t *backend, nx_canvas_t *canvas) {
	// Default clear: color+depth with 1.0 depth, stencil=0, no scissor.
	// Used by swb's compositor; not user-driven so keep upstream-compatible
	// defaults. JS-side gl.clear path threads the user's actual state
	// through nx_webgl_egl_clear_bridge_with_state directly.
	return nx_webgl_egl_clear_bridge_with_state(backend, canvas,
		0x4000 | 0x100 /* COLOR | DEPTH */, 1.0f, 0,
		false, NULL, false);
}

bool nx_webgl_egl_read_bridge_pixels(nx_webgl_egl_t *backend,
									  nx_canvas_t *canvas,
									  int x, int y, int width, int height,
									  uint32_t format, uint32_t type,
									  uint8_t *dst) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)canvas; (void)x; (void)y;
	(void)width; (void)height; (void)format; (void)type; (void)dst;
	return false;
#else
	(void)format;
	(void)type;
	if (!backend || !backend->bridge_enabled || !canvas || !dst)
		return false;
	if (width <= 0 || height <= 0)
		return false;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge readPixels: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}
	int render_width = 0, render_height = 0;
	bridge_render_size(backend, canvas, &render_width, &render_height);
	if (!ensure_bridge_resources(backend, render_width, render_height))
		return false;

	// Clip the requested rect to the FBO bounds. Any rows/cols outside
	// the FBO are zero-filled in `dst` — the caller's buffer must never
	// contain uninitialized bytes.
	int clip_x = x, clip_y = y, clip_w = width, clip_h = height;
	int dst_off_x = 0, dst_off_y = 0;
	if (clip_x < 0) {
		dst_off_x = -clip_x;
		clip_w += clip_x;
		clip_x = 0;
	}
	if (clip_y < 0) {
		dst_off_y = -clip_y;
		clip_h += clip_y;
		clip_y = 0;
	}
	if (clip_x + clip_w > render_width)
		clip_w = render_width - clip_x;
	if (clip_y + clip_h > render_height)
		clip_h = render_height - clip_y;
	const size_t row_bytes = (size_t)width * 4;
	memset(dst, 0, row_bytes * (size_t)height);

	if (clip_w <= 0 || clip_h <= 0)
		return true;

	// Defensive state: scissor must be off so reads aren't clipped to
	// whatever the last bridge draw set; color mask must be all-on (it
	// doesn't affect glReadPixels itself but we set it consistently with
	// the bridge's own reads). Read the clipped sub-rect directly into
	// the caller's buffer with the right destination row stride — when
	// the rect is the full FBO this is identical to the old full-FBO
	// path, but for typical inline-canvas usage (640×360 from a
	// 1280×720 FBO) it's a quarter as much data.
	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	(void)glGetError();
	uint8_t *clipped_start = NULL;
	size_t clipped_row_bytes = 0;
	size_t clipped_stride = 0;
	if (dst_off_x == 0 && clip_w == width) {
		// Common case: x-aligned. Read straight into dst at the correct
		// row offset — one glReadPixels call, no per-row copies.
		clipped_start = dst + (size_t)dst_off_y * row_bytes;
		clipped_row_bytes = (size_t)clip_w * 4;
		clipped_stride = row_bytes;
		glReadPixels(clip_x, clip_y, clip_w, clip_h, GL_RGBA,
		             GL_UNSIGNED_BYTE, clipped_start);
	} else {
		// x-clipped case: row stride differs between dst and the read
		// rect. Fall back to a row-by-row scratch via bridge_readback
		// (sized to fit the largest read; reuse it).
		const size_t scratch_bytes = (size_t)clip_w * (size_t)clip_h * 4;
		if (scratch_bytes <= backend->bridge_readback_size &&
		    backend->bridge_readback) {
			glReadPixels(clip_x, clip_y, clip_w, clip_h, GL_RGBA,
			             GL_UNSIGNED_BYTE, backend->bridge_readback);
			const size_t src_row_bytes = (size_t)clip_w * 4;
			for (int row = 0; row < clip_h; row++) {
				uint8_t *dst_row = dst + (size_t)(dst_off_y + row) * row_bytes
				                       + (size_t)dst_off_x * 4;
				const uint8_t *src_row = backend->bridge_readback +
				                         (size_t)row * src_row_bytes;
				memcpy(dst_row, src_row, src_row_bytes);
			}
			clipped_start = dst + (size_t)dst_off_y * row_bytes
			                  + (size_t)dst_off_x * 4;
			clipped_row_bytes = src_row_bytes;
			clipped_stride = row_bytes;
		}
	}
	GLenum err = glGetError();
	glFinish();
	if (err != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge readPixels failed: 0x%x", err);
		return false;
	}
	// In-place row reverse to convert glReadPixels's GL bottom-up rows
	// to canvas-y top-down — the convention nx.js's bridge already uses
	// for `gl.viewport` / `gl.scissor` x/y inputs. Doing this here
	// shaves ~95 ms/frame off the cube demo vs. the previous JS-side
	// `TypedArray.set(subarray)` row-flip loop in canvas-runner.ts.
	// Scratch is one row's worth (≤ 5 KB for the cube case).
	// Option 2 (2026-06-26): SKIP this reverse when spec_y_origin=true
	// — the caller wants standard GL bottom-up rows. This is the third
	// y-inversion site the conformance opt-in has to neutralize, after
	// bridge_scale_rect and the JS-side gl_y translation in
	// nx_webgl_read_pixels. Missing this is why the canvas-tex tests
	// didn't move under Option 2's first pass — the viewport gate +
	// JS-side gate aligned the draw and the requested rect, but this
	// internal flip re-mirrored the rows on the way back to JS.
	bool spec_y_skip_reverse = backend->spec_y_origin;
	if (!spec_y_skip_reverse &&
		clipped_start && clip_h > 1 && clipped_row_bytes > 0) {
		uint8_t *scratch_row = malloc(clipped_row_bytes);
		if (scratch_row) {
			for (int top = 0; top < clip_h / 2; top++) {
				int bot = clip_h - 1 - top;
				uint8_t *row_top = clipped_start + (size_t)top * clipped_stride;
				uint8_t *row_bot = clipped_start + (size_t)bot * clipped_stride;
				memcpy(scratch_row, row_top, clipped_row_bytes);
				memcpy(row_top, row_bot, clipped_row_bytes);
				memcpy(row_bot, scratch_row, clipped_row_bytes);
			}
			free(scratch_row);
		}
	}
	return true;
#endif
}

// Read a sub-rect of the bridge FBO directly into a destination
// `nx_canvas_t` (e.g., the screen canvas) at (dst_x, dst_y) with the
// usual Y-flip + RGBA→cairo-ARGB32 swizzle, marking the destination
// surface dirty. Skips the JS-visible Uint8ClampedArray buffer + a
// putImageData hop + a drawImage overlay — for brewser's
// animated inline-canvas WebGL path (Three.js cube), this collapses
// the per-frame readPixels + putImageData + drawImage(offscreen)
// chain into one glReadPixels + one C-level row copy.
//
// `src_x` / `src_y` are in GL bottom-up coordinates (the caller is
// responsible for any canvas-y top translation, same convention as
// the rest of this file). `dst_canvas` must be an ARGB32 image-surface
// canvas (anything created via `nx_canvas_new` etc.). Pixels outside
// the FBO's render rect or outside the destination canvas are
// silently skipped.
bool nx_webgl_egl_read_bridge_to_canvas_data(nx_webgl_egl_t *backend,
                                              int src_x, int src_y,
                                              int src_w, int src_h,
                                              nx_canvas_t *dst_canvas,
                                              int dst_x, int dst_y) {
#if !NXJS_HAS_EGL_GLES
	(void)backend; (void)src_x; (void)src_y; (void)src_w; (void)src_h;
	(void)dst_canvas; (void)dst_x; (void)dst_y;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !dst_canvas || !dst_canvas->data)
		return false;
	if (src_w <= 0 || src_h <= 0)
		return false;
	if (dst_canvas->width == 0 || dst_canvas->height == 0)
		return false;
	if (!nx_webgl_egl_initialize(backend, dst_canvas))
		return false;
	// Only call eglMakeCurrent if our context isn't already the one
	// bound to this thread. The call costs ~1 ms on Switch's driver
	// even when it's a no-op rebind; skipping when we know the context
	// is current saves that off every per-frame readback.
	if (eglGetCurrentContext() != backend->context) {
		if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
		                    backend->context)) {
			snprintf(backend->status, sizeof(backend->status),
			         "GPU bridge readToCanvas: eglMakeCurrent() failed: 0x%x",
			         eglGetError());
			return false;
		}
	}
	int render_width = 0, render_height = 0;
	bridge_render_size(backend, dst_canvas, &render_width, &render_height);
	if (!ensure_bridge_resources(backend, render_width, render_height))
		return false;

	// Clip src rect to bridge FBO bounds.
	int clip_x = src_x;
	int clip_y = src_y;
	int clip_w = src_w;
	int clip_h = src_h;
	int src_off_x = 0;
	int src_off_y = 0;
	if (clip_x < 0) {
		src_off_x = -clip_x;
		clip_w += clip_x;
		clip_x = 0;
	}
	if (clip_y < 0) {
		src_off_y = -clip_y;
		clip_h += clip_y;
		clip_y = 0;
	}
	if (clip_x + clip_w > render_width)
		clip_w = render_width - clip_x;
	if (clip_y + clip_h > render_height)
		clip_h = render_height - clip_y;
	if (clip_w <= 0 || clip_h <= 0)
		return true;

	// Clip the destination rect to the destination canvas bounds. We
	// shift `dst_x`/`dst_y` for the parts of the src rect that fell
	// outside the FBO above (src_off_x/y), so the dst position aligns
	// with the *clipped* src.
	int final_dst_x = dst_x + src_off_x;
	int final_dst_y = dst_y + src_off_y;
	int copy_w = clip_w;
	int copy_h = clip_h;
	int src_skip_x = 0;
	int src_skip_y = 0;
	if (final_dst_x < 0) {
		src_skip_x = -final_dst_x;
		copy_w -= src_skip_x;
		final_dst_x = 0;
	}
	if (final_dst_y < 0) {
		src_skip_y = -final_dst_y;
		copy_h -= src_skip_y;
		final_dst_y = 0;
	}
	if (final_dst_x + copy_w > (int)dst_canvas->width)
		copy_w = (int)dst_canvas->width - final_dst_x;
	if (final_dst_y + copy_h > (int)dst_canvas->height)
		copy_h = (int)dst_canvas->height - final_dst_y;
	if (copy_w <= 0 || copy_h <= 0)
		return true;

	// Read the clipped src rect into bridge_readback. Use the
	// already-allocated buffer (sized to fit the full bridge FBO, so
	// any sub-rect fits).
	size_t needed = (size_t)clip_w * (size_t)clip_h * 4;
	if (needed > backend->bridge_readback_size || !backend->bridge_readback)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	(void)glGetError();
	glReadPixels(clip_x, clip_y, clip_w, clip_h, GL_RGBA, GL_UNSIGNED_BYTE,
	             backend->bridge_readback);
	GLenum err = glGetError();
	glFinish();
	if (err != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
		         "GPU bridge readToCanvas readPixels failed: 0x%x", err);
		return false;
	}
	// 2026-06-07 pvzge investigation: log what C actually saw in the
	// bridge framebuffer at copyBridgeToCanvas time. JS-side readPixels
	// reports cyan (68,215,182) from frame ~1561 onward; if the C-side
	// readback agrees, the bridge FBO truly holds cyan (rules out
	// JS-side gl context divergence). If it differs, there's a JS vs C
	// FBO disagreement we need to chase.
	{
		static int readN = 0;
		++readN;
		if (readN <= 10 || (readN % 240) == 0) {
			const uint8_t *p = backend->bridge_readback;
			fprintf(stderr,
				"[nxjs:bridge-read] n=%d fbo=%u src=(%d,%d %dx%d) dst=(%d,%d) px[0,0]=(%u,%u,%u,%u)\n",
				readN, (unsigned)backend->bridge_framebuffer,
				clip_x, clip_y, clip_w, clip_h, final_dst_x, final_dst_y,
				(unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
		}
	}

	// Y-flip + RGBA→ARGB32(premul) swizzle while copying into dst.
	// Source rows are in GL bottom-up order; we map row r in dst to
	// row (clip_h - 1 - r - src_skip_y) in src (i.e. the GL bottom-up
	// row that visually corresponds to the top of the rect, plus any
	// rows we skipped above the dst canvas top edge).
	//
	// `glReadPixels` writes 4-byte tuples (R, G, B, A) at each pixel
	// offset; on little-endian (Switch is LE), reading as uint32 gives
	// bit layout 0xAABBGGRR (low byte = R). Cairo's ARGB32 little-
	// endian is 0xAARRGGBB. So the conversion is "swap R and B bytes"
	// — a single bit-shift expression in the common opaque-alpha case.
	// `bridge_readback` was malloc'd, so it's at least 8-byte aligned;
	// uint32 reads are safe.
	uint32_t *dst_pixels = (uint32_t *)dst_canvas->data;
	const int dst_stride_pixels = (int)dst_canvas->width;
	const int src_row_stride = clip_w * 4;
	for (int row = 0; row < copy_h; row++) {
		int src_row = clip_h - 1 - (row + src_skip_y);
		if (src_row < 0 || src_row >= clip_h)
			continue;
		const uint32_t *src_row_p = (const uint32_t *)(
		    backend->bridge_readback +
		    (size_t)src_row * src_row_stride +
		    (size_t)src_skip_x * 4);
		uint32_t *dst_row_p = dst_pixels +
		                      (size_t)(final_dst_y + row) * dst_stride_pixels +
		                      final_dst_x;
		for (int col = 0; col < copy_w; col++) {
			uint32_t rgba = *src_row_p++;
			uint32_t a = rgba >> 24;
			if (__builtin_expect(a == 255, 1)) {
				// Fast path: swap R and B in the uint32. Keep A and G
				// in place. Three.js's textured-cube render fills this
				// branch for every pixel (opaque clear + opaque mesh
				// material), so it dominates the wall-clock cost.
				*dst_row_p++ = (rgba & 0xff00ff00) |
				               ((rgba & 0x000000ff) << 16) |
				               ((rgba >> 16) & 0x000000ff);
			} else if (a == 0) {
				// Fully transparent source — leave the destination pixel
				// (page content already painted underneath) so a translucent
				// inline canvas reveals what's behind it (e.g. the audio
				// visualizer's transparent background showing the card grid).
				// Was: overwrite with 0 (opaque black).
				dst_row_p++;
			} else {
				// Partially transparent — src-over composite onto the
				// existing (page) pixel instead of overwriting, so content
				// behind the canvas shows through. dst is opaque ARGB32
				// (0xAARRGGBB); src (glReadPixels LE) is 0xAABBGGRR.
				uint32_t r = rgba & 0xff;
				uint32_t g = (rgba >> 8) & 0xff;
				uint32_t b = (rgba >> 16) & 0xff;
				uint32_t inv = 255 - a;
				uint32_t d = *dst_row_p;
				uint32_t dr = (d >> 16) & 0xff;
				uint32_t dg = (d >> 8) & 0xff;
				uint32_t db = d & 0xff;
				uint32_t orr = (r * a + dr * inv + 127) / 255;
				uint32_t og = (g * a + dg * inv + 127) / 255;
				uint32_t ob = (b * a + db * inv + 127) / 255;
				*dst_row_p++ = 0xff000000u | (orr << 16) | (og << 8) | ob;
			}
		}
	}
	if (dst_canvas->surface)
		cairo_surface_mark_dirty(dst_canvas->surface);
	return true;
#endif
}

bool nx_webgl_egl_clear_bridge_with_state(nx_webgl_egl_t *backend,
										  nx_canvas_t *canvas,
										  uint32_t mask,
										  float depth_value,
										  int32_t stencil_value,
										  bool scissor_enabled,
										  const int *scissor_box,
										  bool depth_enabled) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)mask;
	(void)depth_value;
	(void)stencil_value;
	(void)scissor_enabled;
	(void)scissor_box;
	(void)depth_enabled;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge clear: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	bridge_target_t target;
	if (!bridge_acquire_target(backend, canvas, &target))
		return false;
	int width = target.width;
	int height = target.height;
	// 2026-06-07 pvzge investigation: trace which FBO each JS gl.clear()
	// hits, with the actual clear_color the backend will write. JS-side
	// instrumentation in pvzge proves Cocos only ever submitted RED + a
	// dark pipeline-bg via `gl.clearColor`, but the bridge readback turns
	// CYAN (0x44d7b6) at frame ~1561 and stays cyan. If the cyan never
	// appears here either, the clear path is innocent and the cyan must
	// come from shader output / fragment writes. If a user FBO ID
	// becomes the predominant clear target around that point, the
	// page's draws+clears are landing on an intermediate FBO and the
	// bridge FBO's stale contents are what we keep reading.
	{
		static int clearN = 0;
		++clearN;
		if (clearN <= 10 || (clearN % 240) == 0)
			fprintf(stderr,
				"[nxjs:bridge-clear] n=%d mask=0x%x fbo=%u user=%d color=(%.3f,%.3f,%.3f,%.3f) depth=%.3f stencil=%d scissor=%d size=%dx%d\n",
				clearN, mask, (unsigned)target.fbo, target.is_user_fbo ? 1 : 0,
				backend->clear_color[0], backend->clear_color[1],
				backend->clear_color[2], backend->clear_color[3],
				depth_value, stencil_value, scissor_enabled ? 1 : 0,
				width, height);
	}

	(void)depth_enabled;
	glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
	glViewport(0, 0, width, height);
	if (target.is_user_fbo) {
		if (scissor_enabled) {
			int sx = scissor_box ? scissor_box[0] : 0;
			int sy = scissor_box ? scissor_box[1] : 0;
			int sw = scissor_box ? scissor_box[2] : width;
			int sh = scissor_box ? scissor_box[3] : height;
			glEnable(GL_SCISSOR_TEST);
			glScissor(sx, sy, sw, sh);
		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	} else {
		bridge_apply_scissor(canvas, width, height, scissor_enabled,
		                     scissor_box, backend->spec_y_origin);
	}
	// Defensively re-enable color writes; some callers (Three.js's
	// WebGLState) leave the color mask in a mode that would silently
	// turn glClear into a no-op.
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// Honor the user's clearColor / clearDepth / clearStencil state AND
	// the bit-mask argument to gl.clear. Pre-#19.5 this hardcoded
	// glClearDepthf(1.0) and glClear(COLOR|DEPTH) regardless of either —
	// works for Three.js's default render path (which always clears
	// both with depth=1.0) but breaks any code that wants a different
	// depth clear value or selective clears. Surfaced by milestone #19.5
	// hw bring-up.
	GLbitfield gl_mask = 0;
	if (mask & 0x4000 /* GL_COLOR_BUFFER_BIT */) {
		gl_mask |= GL_COLOR_BUFFER_BIT;
		glClearColor((GLfloat)backend->clear_color[0],
					 (GLfloat)backend->clear_color[1],
					 (GLfloat)backend->clear_color[2],
					 (GLfloat)backend->clear_color[3]);
	}
	if (mask & 0x100 /* GL_DEPTH_BUFFER_BIT */) {
		gl_mask |= GL_DEPTH_BUFFER_BIT;
		glClearDepthf(depth_value);
		glDepthMask(GL_TRUE);
	}
	if (mask & 0x400 /* GL_STENCIL_BUFFER_BIT */) {
		gl_mask |= GL_STENCIL_BUFFER_BIT;
		glClearStencil((GLint)stencil_value);
	}
	// Clear stale errors so the post-clear check only reflects this op
	// (see [[bridge-stale-glerror]]).
	(void)glGetError();
	if (gl_mask)
		glClear(gl_mask);
	glDisable(GL_SCISSOR_TEST);
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge clear failed: 0x%x", error);
		return false;
	}
	// Next textured draw is the first of the frame — the bridge needs a
	// pipeline-state-validation warmup before it (see field comment).
	// Only relevant when clearing the bridge's own FBO; user FBOs don't
	// suffer the warmup quirk (the bug is bridge-FBO-pipeline-specific).
	if (!target.is_user_fbo)
		backend->bridge_pending_textured_warmup = true;

	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge clear queued %dx%d -> %ux%u", width, height,
			 canvas->width, canvas->height);
	return true;
#endif
}

#if NXJS_HAS_EGL_GLES

/* ─────────────────────────────────────────────────────────────────────
 * TESSELLATION-FIX WORKAROUND — STATE AS OF 2026-05-17
 *
 * Original goal: a bridge-side workaround for the Tegra X1 rasterizer
 * "white face / dark face" bug on Three.js demos drawing BoxGeometry-
 * sized triangles via Citron (see the `threejs-cube-white-face`
 * memory). The bug manifests as: a face-sized triangle renders with
 * every fragment sampling one corner of the bound texture, painting
 * the whole face uniformly. JS-side manual tessellation
 * (`BoxGeometry(1,1,1,8,8,8)`) eliminates the bug, presumably because
 * no single triangle is "too big" anymore.
 *
 * The hypothesis was: subdivide large triangles bridge-side too, so
 * every demo Just Works without per-demo BoxGeometry tweaks.
 *
 * Implementation (the code below): recursive midpoint subdivision of
 * each input triangle in NDC space (post-perspective-divide, which is
 * the only space the bridge has). Threshold based on projected screen
 * area; recursion depth capped to bound the worst-case blowup.
 *
 *    !!! THIS WORKAROUND DOES NOT ACTUALLY FIX THE BUG. !!!
 *
 * Evidence: with `tessellation_fix_enabled = true` and the threshold
 * dialled all the way down to force the same 64× blowup that manual
 * `BoxGeometry(1,1,1,8,8,8)` uses, the wedge artifact still appears
 * on the cube demo. Same triangle count, same per-leaf area — yet
 * Three.js's grid produces a working cube and the bridge's
 * subdivision does not.
 *
 * Why the difference: Three.js generates grid vertices in OBJECT
 * space and then projects each one, so an angled face's far-edge
 * sub-triangles end up foreshortened (tiny in screen). The bridge
 * midpoints in NDC space (post-divide), producing UNIFORMLY-sized
 * sub-triangles across the face. Apparently the rasterizer bug is
 * sensitive to where in screen space the sub-triangles land, not
 * just their size. Three.js's foreshortened pattern dodges it; the
 * bridge's uniform pattern still triggers it.
 *
 * To actually fix this bridge-side, the bridge needs to subdivide in
 * pre-divide CLIP space — i.e. keep `w` through the pipeline so
 * midpoints are computed before perspective foreshortening. That's a
 * larger refactor: callers in `webgl.c::draw_*_textured_triangles_*`
 * currently do the perspective divide themselves and hand the bridge
 * NDC vertices. We'd need to push the divide down into the bridge so
 * it can subdivide first and then divide per leaf vertex.
 *
 * Operational state:
 *   - `tessellation_fix_enabled` defaults to `false` (see backend init).
 *   - brewser does NOT call `gl.setTessellationFix` — it
 *     uses JS-side `BoxGeometry(w,h,d,8,8,8)` tessellation instead,
 *     which is the only thing currently known to work.
 *   - The `gl.setTessellationFix(bool)` JS API is still exposed so a
 *     future revisitor (or an experiment) can flip it on without
 *     touching this code. Just understand: in the current
 *     implementation, flipping it on costs CPU per draw and does NOT
 *     produce the visual fix Three.js demos need.
 *
 * Why this code is still here (not deleted): the scaffolding —
 * scratch buffer, recursion helper, JS API, integration in both
 * textured and untextured bridge draw paths — is exactly what a
 * future clip-space-correct version would need. Keeping it
 * documented-but-dormant means the next attempt picks up the
 * infrastructure for free. Deleting and rebuilding from scratch
 * months later would be a waste.
 * ───────────────────────────────────────────────────────────────── */

/* Recursively splits a single triangle by midpoints into 4 children
 * until each child's projected screen area falls below `max_pixel_area`
 * (or `max_depth` is reached).
 *
 * Vertices are interleaved at `stride` floats each. The FIRST TWO
 * floats are interpreted as NDC x,y for the area test; the rest
 * (z, optional uv, optional rgb, etc.) are linearly interpolated as
 * opaque payload.
 *
 * Worst-case output per input triangle is `4^max_depth` sub-triangles
 * = `4^max_depth * 3` vertices = `4^max_depth * 3 * stride` floats.
 * For max_depth=4 that's a 256× blowup ceiling; the caller must
 * pre-size the output buffer accordingly. */
static void tessellate_one_triangle(
    const float *v0, const float *v1, const float *v2,
    int stride,
    int screen_w, int screen_h,
    float max_pixel_area,
    int max_depth,
    float *out_buf, int out_capacity_vertices, int *out_pos /* in/out */) {
	/* If we're full, drop further geometry rather than overflow. The
	 * caller pre-sized for the worst case so this should never fire,
	 * but defending against it keeps a misbehaving frame from
	 * trashing memory. */
	if (*out_pos + 3 > out_capacity_vertices)
		return;

	/* Pixel-area test in screen space. NDC range [-1,1] maps to
	 * [0,screen] so each NDC unit = screen/2 pixels. Twice the signed
	 * area of the triangle (no /2) is fine for comparison. */
	float dx01 = (v1[0] - v0[0]) * 0.5f * (float)screen_w;
	float dy01 = (v1[1] - v0[1]) * 0.5f * (float)screen_h;
	float dx02 = (v2[0] - v0[0]) * 0.5f * (float)screen_w;
	float dy02 = (v2[1] - v0[1]) * 0.5f * (float)screen_h;
	float double_area = dx01 * dy02 - dx02 * dy01;
	if (double_area < 0.f)
		double_area = -double_area;

	if (max_depth <= 0 || double_area <= max_pixel_area * 2.f) {
		/* Leaf: emit as-is. */
		float *dst = out_buf + (size_t)(*out_pos) * (size_t)stride;
		for (int k = 0; k < stride; k++) {
			dst[k] = v0[k];
			dst[stride + k] = v1[k];
			dst[2 * stride + k] = v2[k];
		}
		*out_pos += 3;
		return;
	}

	/* Midpoint each edge. Stride is bounded by the small list of bridge
	 * vertex layouts (3 for position-only, 5 for textured position+uv);
	 * a 16-float local scratch is plenty. */
	float m01[16];
	float m12[16];
	float m20[16];
	for (int k = 0; k < stride; k++) {
		m01[k] = (v0[k] + v1[k]) * 0.5f;
		m12[k] = (v1[k] + v2[k]) * 0.5f;
		m20[k] = (v2[k] + v0[k]) * 0.5f;
	}

	/* Four sub-triangles in CCW order matching the parent. */
	tessellate_one_triangle(v0, m01, m20, stride, screen_w, screen_h,
							max_pixel_area, max_depth - 1, out_buf,
							out_capacity_vertices, out_pos);
	tessellate_one_triangle(m01, v1, m12, stride, screen_w, screen_h,
							max_pixel_area, max_depth - 1, out_buf,
							out_capacity_vertices, out_pos);
	tessellate_one_triangle(m20, m12, v2, stride, screen_w, screen_h,
							max_pixel_area, max_depth - 1, out_buf,
							out_capacity_vertices, out_pos);
	tessellate_one_triangle(m01, m12, m20, stride, screen_w, screen_h,
							max_pixel_area, max_depth - 1, out_buf,
							out_capacity_vertices, out_pos);
}

/* Ensure tessellation_scratch is at least `needed_floats` floats. Grows
 * geometrically so repeated small bumps don't churn the heap.
 * Returns true if the buffer is sized; false on allocation failure
 * (in which case the caller should skip tessellation and draw the
 * original geometry). */
static bool ensure_tessellation_scratch(nx_webgl_egl_t *backend,
										size_t needed_floats) {
	if (backend->tessellation_scratch_capacity_floats >= needed_floats)
		return true;
	size_t cap = backend->tessellation_scratch_capacity_floats
				 ? backend->tessellation_scratch_capacity_floats
				 : 4096;
	while (cap < needed_floats)
		cap *= 2;
	float *new_buf = realloc(backend->tessellation_scratch,
							 cap * sizeof(float));
	if (!new_buf)
		return false;
	backend->tessellation_scratch = new_buf;
	backend->tessellation_scratch_capacity_floats = cap;
	return true;
}

/* Returns the worst-case vertex blowup factor at `max_depth`. */
static int tessellation_max_blowup(int max_depth) {
	/* Each level of subdivision turns one triangle into four, so
	 * `4^max_depth` triangles in the worst case. Each contributes 3
	 * vertices, but vertex_count already counts 3 per triangle, so
	 * the blowup factor for total vertex count is just `4^max_depth`. */
	int factor = 1;
	for (int i = 0; i < max_depth; i++)
		factor *= 4;
	return factor;
}

/* Subdivide an interleaved triangle-soup buffer. Returns the resulting
 * vertex count on success; on failure (allocation, no work to do, or
 * fix disabled) returns 0 and the caller falls back to the original
 * geometry.
 *
 *   stride: floats per vertex (3 for position-only, 5 for pos+uv).
 *   First two floats of each vertex are interpreted as NDC x,y for the
 *   area test; everything else is opaque payload that gets linearly
 *   interpolated. */
static int tessellate_textured_soup(nx_webgl_egl_t *backend,
									const float *src, int vertex_count,
									int stride, int screen_w, int screen_h) {
	if (!backend->tessellation_fix_enabled || !src || vertex_count < 3 ||
		vertex_count % 3 != 0 || screen_w <= 0 || screen_h <= 0)
		return 0;

	/* Threshold + depth tuning is moot in the current implementation
	 * because the workaround doesn't actually fix the bug (see the big
	 * comment block above the helpers). These values are kept at the
	 * size-based heuristic that *would* be reasonable if the underlying
	 * approach worked: ~16 hardware tiles per leaf, depth ceiling for
	 * pathological close-up cases. */
	const int max_depth = 4;
	const float max_pixel_area = 4096.f; /* ~16 tiles of 16×16 px */

	int blowup = tessellation_max_blowup(max_depth);
	size_t needed_floats = (size_t)vertex_count * (size_t)blowup *
						   (size_t)stride;
	if (!ensure_tessellation_scratch(backend, needed_floats))
		return 0;

	int out_pos = 0;
	int out_capacity_vertices = (int)((size_t)vertex_count * (size_t)blowup);
	for (int i = 0; i < vertex_count; i += 3) {
		const float *v0 = src + (size_t)(i + 0) * (size_t)stride;
		const float *v1 = src + (size_t)(i + 1) * (size_t)stride;
		const float *v2 = src + (size_t)(i + 2) * (size_t)stride;
		tessellate_one_triangle(v0, v1, v2, stride, screen_w, screen_h,
								max_pixel_area, max_depth,
								backend->tessellation_scratch,
								out_capacity_vertices, &out_pos);
	}
	return out_pos;
}

#endif /* NXJS_HAS_EGL_GLES */

bool nx_webgl_egl_draw_triangles_bridge(nx_webgl_egl_t *backend,
										nx_canvas_t *canvas,
										const float *clip_xyz,
										const float *vertex_colors,
										int vertex_count,
										const float *color,
										bool blend,
										uint32_t blend_src,
										uint32_t blend_dst,
										uint32_t blend_src_alpha,
										uint32_t blend_dst_alpha,
										const int *viewport,
										bool scissor_enabled,
										const int *scissor_box,
										bool depth_enabled,
										const float *fog_depth,
										bool fog_enabled,
										const float *fog_color,
										float fog_near,
										float fog_far,
										const float *normals,
										bool lighting_enabled,
										const float *light_direction,
										const float *light_color,
										const float *ambient_light_color,
										const float *view_positions,
										bool point_light_enabled,
										const float *point_light_position,
										const float *point_light_color,
										float point_light_distance,
										float point_light_decay,
										bool light2_enabled,
										const float *light_direction2,
										const float *light_color2,
										bool fog_exp2_enabled,
										float fog_density,
										bool cull_enabled,
										uint32_t cull_face_mode,
										bool specular_enabled,
										const float *specular_color,
										float shininess,
										const float *emissive_color,
										bool use_derivative_normals,
										bool hemi_light_enabled,
										const float *hemi_light_direction,
										const float *hemi_light_sky_color,
										const float *hemi_light_ground_color,
										const nx_webgl_egl_spot_light_t *spot_light) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)clip_xyz;
	(void)vertex_colors;
	(void)vertex_count;
	(void)color;
	(void)blend;
	(void)blend_src;
	(void)blend_dst;
	(void)blend_src_alpha;
	(void)blend_dst_alpha;
	(void)viewport;
	(void)scissor_enabled;
	(void)scissor_box;
	(void)depth_enabled;
	(void)fog_depth;
	(void)fog_enabled;
	(void)fog_color;
	(void)fog_near;
	(void)fog_far;
	(void)normals;
	(void)lighting_enabled;
	(void)light_direction;
	(void)light_color;
	(void)ambient_light_color;
	(void)view_positions;
	(void)point_light_enabled;
	(void)point_light_position;
	(void)point_light_color;
	(void)point_light_distance;
	(void)point_light_decay;
	(void)light2_enabled;
	(void)light_direction2;
	(void)light_color2;
	(void)fog_exp2_enabled;
	(void)fog_density;
	(void)cull_enabled;
	(void)cull_face_mode;
	(void)specular_enabled;
	(void)specular_color;
	(void)shininess;
	(void)emissive_color;
	(void)use_derivative_normals;
	(void)hemi_light_enabled;
	(void)hemi_light_direction;
	(void)hemi_light_sky_color;
	(void)hemi_light_ground_color;
	(void)spot_light;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0 || !clip_xyz ||
		vertex_count <= 0 || vertex_count % 3 != 0 || !color)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	bridge_target_t target;
	if (!bridge_acquire_target(backend, canvas, &target) ||
		!ensure_bridge_color_program(backend))
		return false;
	int width = target.width;
	int height = target.height;

	bridge_bind_target(&target, backend, canvas, viewport, scissor_enabled, scissor_box);
	// Honor Three.js's `gl.enable(GL_CULL_FACE)` + `gl.cullFace(...)` state
	// so single-sided materials (MeshPhongMaterial default
	// `side: FrontSide`) don't render their back-facing triangles. Without
	// this, the visible silhouette of solid extruded shapes shows
	// ambient-only back faces leaking through what should be only the
	// lit front cap, producing the "darker than expected" look.
	if (cull_enabled) {
		GLenum gl_mode = GL_BACK;
		if (cull_face_mode == 0x0404u) gl_mode = GL_FRONT;
		else if (cull_face_mode == 0x0408u) gl_mode = GL_FRONT_AND_BACK;
		glEnable(GL_CULL_FACE);
		glCullFace(gl_mode);
	} else {
		glDisable(GL_CULL_FACE);
	}
	glUseProgram(backend->bridge_color_program);
	if (backend->bridge_color_color_loc >= 0)
		glUniform4f(backend->bridge_color_color_loc, color[0], color[1],
					color[2], color[3]);
	bool fog_active = fog_enabled && fog_depth && fog_color;
	if (backend->bridge_color_fog_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_fog_enabled_loc,
					fog_active ? 1.f : 0.f);
	if (backend->bridge_color_fog_exp2_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_fog_exp2_enabled_loc,
					(fog_active && fog_exp2_enabled) ? 1.f : 0.f);
	if (fog_active) {
		if (backend->bridge_color_fog_color_loc >= 0)
			glUniform3f(backend->bridge_color_fog_color_loc, fog_color[0],
						fog_color[1], fog_color[2]);
		if (backend->bridge_color_fog_near_loc >= 0)
			glUniform1f(backend->bridge_color_fog_near_loc, fog_near);
		if (backend->bridge_color_fog_far_loc >= 0)
			glUniform1f(backend->bridge_color_fog_far_loc, fog_far);
		if (backend->bridge_color_fog_density_loc >= 0)
			glUniform1f(backend->bridge_color_fog_density_loc, fog_density);
	}
	// Lighting requires per-fragment normals — sourced from either the
	// `normals` buffer (per-vertex view-space normals, populated CPU-side)
	// or from view-position derivatives (`u_useDerivativeNormals` path,
	// milestone #16). The derivative path needs view-positions populated
	// regardless, which the dispatch already guarantees when lit.
	bool normals_available = normals != NULL || use_derivative_normals;
	bool light_active = lighting_enabled && normals_available &&
						light_direction && light_color && ambient_light_color;
	if (backend->bridge_color_lighting_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_lighting_enabled_loc,
					light_active ? 1.f : 0.f);
	if (backend->bridge_color_use_derivative_normals_loc >= 0)
		glUniform1f(backend->bridge_color_use_derivative_normals_loc,
					(light_active && use_derivative_normals) ? 1.f : 0.f);
	if (light_active) {
		if (backend->bridge_color_light_direction_loc >= 0)
			glUniform3f(backend->bridge_color_light_direction_loc,
						light_direction[0], light_direction[1],
						light_direction[2]);
		if (backend->bridge_color_light_color_loc >= 0)
			glUniform3f(backend->bridge_color_light_color_loc, light_color[0],
						light_color[1], light_color[2]);
		if (backend->bridge_color_ambient_light_color_loc >= 0)
			glUniform3f(backend->bridge_color_ambient_light_color_loc,
						ambient_light_color[0], ambient_light_color[1],
						ambient_light_color[2]);
	}
	bool light2_active = light_active && light2_enabled && light_direction2 &&
						 light_color2;
	if (backend->bridge_color_light2_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_light2_enabled_loc,
					light2_active ? 1.f : 0.f);
	if (light2_active) {
		if (backend->bridge_color_light_direction2_loc >= 0)
			glUniform3f(backend->bridge_color_light_direction2_loc,
						light_direction2[0], light_direction2[1],
						light_direction2[2]);
		if (backend->bridge_color_light_color2_loc >= 0)
			glUniform3f(backend->bridge_color_light_color2_loc,
						light_color2[0], light_color2[1], light_color2[2]);
	}
	// view_positions tracks whether `a_viewPosition` is populated. Used by
	// both the point-light path AND the new specular path — both need
	// per-vertex view-space positions.
	bool view_positions_active = light_active && view_positions != NULL;
	bool point_active = view_positions_active && point_light_enabled &&
						point_light_position && point_light_color;
	if (backend->bridge_color_point_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_point_light_enabled_loc,
					point_active ? 1.f : 0.f);
	if (point_active) {
		if (backend->bridge_color_point_light_position_loc >= 0)
			glUniform3f(backend->bridge_color_point_light_position_loc,
						point_light_position[0], point_light_position[1],
						point_light_position[2]);
		if (backend->bridge_color_point_light_color_loc >= 0)
			glUniform3f(backend->bridge_color_point_light_color_loc,
						point_light_color[0], point_light_color[1],
						point_light_color[2]);
		if (backend->bridge_color_point_light_distance_loc >= 0)
			glUniform1f(backend->bridge_color_point_light_distance_loc,
						point_light_distance);
		if (backend->bridge_color_point_light_decay_loc >= 0)
			glUniform1f(backend->bridge_color_point_light_decay_loc,
						point_light_decay);
	}
	// Blinn-Phong specular. Active when the program has both `specular`
	// (vec3) and `shininess` (float) bound AND view-positions are
	// available (V = normalize(-v_viewPosition) is needed for the halfway
	// vector). Three.js's MeshPhongMaterial uploads both — see
	// [[bridge-lighting-support]].
	bool specular_active = view_positions_active && specular_enabled &&
						   specular_color;
	if (backend->bridge_color_specular_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_specular_enabled_loc,
					specular_active ? 1.f : 0.f);
	if (specular_active) {
		if (backend->bridge_color_specular_loc >= 0)
			glUniform3f(backend->bridge_color_specular_loc, specular_color[0],
						specular_color[1], specular_color[2]);
		if (backend->bridge_color_shininess_loc >= 0)
			glUniform1f(backend->bridge_color_shininess_loc, shininess);
	}
	// Always upload emissive so a prior program's value can't leak. Default
	// zero is a no-op (additive term `gl_FragColor.rgb += u_emissive`).
	if (backend->bridge_color_emissive_loc >= 0) {
		if (emissive_color) {
			glUniform3f(backend->bridge_color_emissive_loc, emissive_color[0],
						emissive_color[1], emissive_color[2]);
		} else {
			glUniform3f(backend->bridge_color_emissive_loc, 0.f, 0.f, 0.f);
		}
	}
	// HemisphereLight (Three.js). Active when lit AND all three uniforms
	// were bound on the program. Always upload an enabled-flag so a prior
	// program's value can't leak; also always upload the three color/dir
	// uniforms (default zero) so the shader's mix() doesn't read stale
	// values when the program toggles between hemi-lit and non-hemi-lit
	// draws within a frame. See [[bridge-lighting-support]].
	bool hemi_active = light_active && hemi_light_enabled &&
					   hemi_light_direction && hemi_light_sky_color &&
					   hemi_light_ground_color;
	if (backend->bridge_color_hemi_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_hemi_light_enabled_loc,
					hemi_active ? 1.f : 0.f);
	if (hemi_active) {
		if (backend->bridge_color_hemi_light_direction_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_direction_loc,
						hemi_light_direction[0], hemi_light_direction[1],
						hemi_light_direction[2]);
		if (backend->bridge_color_hemi_light_sky_color_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_sky_color_loc,
						hemi_light_sky_color[0], hemi_light_sky_color[1],
						hemi_light_sky_color[2]);
		if (backend->bridge_color_hemi_light_ground_color_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_ground_color_loc,
						hemi_light_ground_color[0], hemi_light_ground_color[1],
						hemi_light_ground_color[2]);
	} else {
		if (backend->bridge_color_hemi_light_direction_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_direction_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_color_hemi_light_sky_color_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_sky_color_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_color_hemi_light_ground_color_loc >= 0)
			glUniform3f(backend->bridge_color_hemi_light_ground_color_loc,
						0.f, 0.f, 0.f);
	}
	// SpotLight. Active when lit, view-positions are populated (the cone
	// distance attenuation needs the per-fragment position), AND the caller
	// passed an enabled spot_light struct with valid vec3 pointers. Always
	// upload the enabled flag so a prior program's value can't leak; when
	// inactive, zero out the cone parameters too so smoothstep() doesn't
	// pick up stale uniforms on next draw.
	bool spot_active = view_positions_active && spot_light &&
					   spot_light->enabled && spot_light->position &&
					   spot_light->direction && spot_light->color;
	if (backend->bridge_color_spot_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_color_spot_light_enabled_loc,
					spot_active ? 1.f : 0.f);
	if (spot_active) {
		if (backend->bridge_color_spot_light_position_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_position_loc,
						spot_light->position[0], spot_light->position[1],
						spot_light->position[2]);
		if (backend->bridge_color_spot_light_direction_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_direction_loc,
						spot_light->direction[0], spot_light->direction[1],
						spot_light->direction[2]);
		if (backend->bridge_color_spot_light_color_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_color_loc,
						spot_light->color[0], spot_light->color[1],
						spot_light->color[2]);
		if (backend->bridge_color_spot_light_distance_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_distance_loc,
						spot_light->distance);
		if (backend->bridge_color_spot_light_cone_cos_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_cone_cos_loc,
						spot_light->cone_cos);
		if (backend->bridge_color_spot_light_penumbra_cos_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_penumbra_cos_loc,
						spot_light->penumbra_cos);
		if (backend->bridge_color_spot_light_decay_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_decay_loc,
						spot_light->decay);
	} else {
		// Zero out to avoid stale-uniform bleed-through. coneCos and
		// penumbraCos default to 1 (cos(0)) so smoothstep(1,1,x) returns 0
		// — no contribution even if the enabled flag were ever bypassed.
		if (backend->bridge_color_spot_light_position_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_position_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_color_spot_light_direction_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_direction_loc,
						0.f, 0.f, 1.f);
		if (backend->bridge_color_spot_light_color_loc >= 0)
			glUniform3f(backend->bridge_color_spot_light_color_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_color_spot_light_distance_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_distance_loc, 0.f);
		if (backend->bridge_color_spot_light_cone_cos_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_cone_cos_loc, 1.f);
		if (backend->bridge_color_spot_light_penumbra_cos_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_penumbra_cos_loc, 1.f);
		if (backend->bridge_color_spot_light_decay_loc >= 0)
			glUniform1f(backend->bridge_color_spot_light_decay_loc, 0.f);
	}
	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	bool has_vertex_colors = vertex_colors != NULL;
	if (has_vertex_colors) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_triangle_color_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), vertex_colors,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(1);
		glVertexAttrib3f(1, 1.f, 1.f, 1.f);
	}

	if (fog_active) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_fog_depth_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * sizeof(float), fog_depth,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(2);
		glVertexAttrib1f(2, 0.f);
	}

	// Only upload to the normal attribute when we actually have per-vertex
	// normals. Under derivative-normals (milestone #16) the buffer is NULL
	// and the fragment shader computes per-fragment normals from
	// `v_viewPosition` derivatives — the unused `a_normal` is harmless
	// (driver dead-codes it since v_normal isn't read on that branch).
	if (light_active && normals) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_normal_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), normals,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(3);
		glVertexAttrib3f(3, 0.f, 0.f, 1.f);
	}

	if (view_positions_active) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_view_position_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), view_positions,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(4);
		glVertexAttrib3f(4, 0.f, 0.f, 0.f);
	}

	if (blend) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(blend_src, blend_dst, blend_src_alpha, blend_dst_alpha);
	} else {
		glDisable(GL_BLEND);
	}
	if (depth_enabled) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glDepthRangef(0.f, 1.f);
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	// Viewport + scissor already applied by bridge_bind_target above.

	// Same "first draw of frame loses 1-2 primitives" workaround as the
	// textured path. See that block for iteration history. Position
	// attribute is 3 floats (no UVs in this path), so 18 floats total
	// (6 verts × 3 floats) for the off-screen 6-vert warmup.
	if (backend->bridge_pending_textured_warmup) {
		backend->bridge_pending_textured_warmup = false;
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
		static const float bridge_warmup_off_screen_xyz[18] = {
			10.f, 0.f, 0.f,
			11.f, 0.f, 0.f,
			10.f, 1.f, 0.f,
			11.f, 0.f, 0.f,
			11.f, 1.f, 0.f,
			10.f, 1.f, 0.f,
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(bridge_warmup_off_screen_xyz),
					 bridge_warmup_off_screen_xyz, GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		(void)glGetError();
	}

	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);

	// Tessellation-fix hook (untextured triangles, stride 3: x,y,z).
	// Same disclaimer as the textured path: off by default, current
	// implementation does NOT fix the rasterizer bug — see the STATE
	// comment block above `tessellate_one_triangle` for the full
	// rationale and what would actually work. Hook kept in place as
	// scaffolding for a future clip-space-correct re-implementation.
	//
	// Gated on `!has_vertex_colors` because the per-vertex-color case
	// uses two separate buffers (positions + colors in independent
	// glBufferData uploads); to subdivide it we'd have to interleave,
	// subdivide, then de-interleave. Per-vertex coloring is rare in
	// the demos we're porting so we skip it rather than complicate the
	// scaffolding. A future revisit could either fold colors into a
	// single interleaved buffer or extend `tessellate_textured_soup`
	// to subdivide N parallel streams.
	const float *draw_xyz = clip_xyz;
	int draw_vertex_count = vertex_count;
	if (backend->tessellation_fix_enabled && !has_vertex_colors) {
		int subdivided = tessellate_textured_soup(
			backend, clip_xyz, vertex_count, 3, width, height);
		if (subdivided >= 3) {
			draw_xyz = backend->tessellation_scratch;
			draw_vertex_count = subdivided;
		}
	}

	glBufferData(GL_ARRAY_BUFFER, (size_t)draw_vertex_count * 3 * sizeof(float),
				 draw_xyz, GL_STREAM_DRAW);
	// Clear any stale GL error from prior setup so glGetError after the draw
	// only reflects the draw itself, not unrelated state changes.
	(void)glGetError();
	glDrawArrays(GL_TRIANGLES, 0, draw_vertex_count);
	GLenum error = glGetError();
	backend->bridge_last_draw_gl_error = error;
	glDisableVertexAttribArray(0);
	if (has_vertex_colors)
		glDisableVertexAttribArray(1);
	if (fog_active)
		glDisableVertexAttribArray(2);
	if (light_active && normals)
		glDisableVertexAttribArray(3);
	if (point_active)
		glDisableVertexAttribArray(4);
	glDisable(GL_SCISSOR_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw failed: 0x%x (vc=%d)", error,
				 has_vertex_colors);
		return false;
	}

	bridge_mark_readback(backend, &target);
	snprintf(backend->status, sizeof(backend->status),
			 "vc=%d lit=%d L2=%d pt=%d fog=%d exp2=%d normPtr=%d",
			 vertex_count, light_active ? 1 : 0, light2_active ? 1 : 0,
			 point_active ? 1 : 0, fog_active ? 1 : 0,
			 fog_exp2_enabled ? 1 : 0, normals != NULL ? 1 : 0);
	return true;
#endif
}

bool nx_webgl_egl_draw_lines_bridge(nx_webgl_egl_t *backend,
									nx_canvas_t *canvas,
									const float *clip_xyz,
									const float *line_distance,
									const float *vertex_colors,
									int vertex_count,
									const float *color,
									float scale,
									float dash_size,
									float total_size,
									bool blend,
									uint32_t blend_src,
									uint32_t blend_dst,
									uint32_t blend_src_alpha,
									uint32_t blend_dst_alpha,
									const int *viewport,
									bool scissor_enabled,
									const int *scissor_box,
									bool depth_enabled,
									const float *fog_depth,
									bool fog_enabled,
									const float *fog_color,
									float fog_near,
									float fog_far) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)clip_xyz;
	(void)line_distance;
	(void)vertex_colors;
	(void)vertex_count;
	(void)color;
	(void)scale;
	(void)dash_size;
	(void)total_size;
	(void)blend;
	(void)blend_src;
	(void)blend_dst;
	(void)blend_src_alpha;
	(void)blend_dst_alpha;
	(void)viewport;
	(void)scissor_enabled;
	(void)scissor_box;
	(void)depth_enabled;
	(void)fog_depth;
	(void)fog_enabled;
	(void)fog_color;
	(void)fog_near;
	(void)fog_far;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0 || !clip_xyz ||
		vertex_count <= 0 || vertex_count % 2 != 0 || !color)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	bridge_target_t target;
	if (!bridge_acquire_target(backend, canvas, &target) ||
		!ensure_bridge_line_program(backend))
		return false;
	int width = target.width;
	int height = target.height;

	bridge_bind_target(&target, backend, canvas, viewport, scissor_enabled, scissor_box);
	glUseProgram(backend->bridge_line_program);
	if (backend->bridge_line_color_loc >= 0)
		glUniform4f(backend->bridge_line_color_loc, color[0], color[1],
					color[2], color[3]);
	if (backend->bridge_line_scale_loc >= 0)
		glUniform1f(backend->bridge_line_scale_loc, scale);
	if (backend->bridge_line_dash_loc >= 0)
		glUniform1f(backend->bridge_line_dash_loc, dash_size);
	if (backend->bridge_line_total_loc >= 0)
		glUniform1f(backend->bridge_line_total_loc, total_size);
	bool fog_active = fog_enabled && fog_depth && fog_color;
	if (backend->bridge_line_fog_enabled_loc >= 0)
		glUniform1f(backend->bridge_line_fog_enabled_loc,
					fog_active ? 1.f : 0.f);
	if (fog_active) {
		if (backend->bridge_line_fog_color_loc >= 0)
			glUniform3f(backend->bridge_line_fog_color_loc, fog_color[0],
						fog_color[1], fog_color[2]);
		if (backend->bridge_line_fog_near_loc >= 0)
			glUniform1f(backend->bridge_line_fog_near_loc, fog_near);
		if (backend->bridge_line_fog_far_loc >= 0)
			glUniform1f(backend->bridge_line_fog_far_loc, fog_far);
	}

	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, (size_t)vertex_count * 3 * sizeof(float),
				 clip_xyz, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	bool has_line_distance = line_distance != NULL;
	if (has_line_distance) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_line_distance_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * sizeof(float), line_distance,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(1);
		glVertexAttrib1f(1, 0.f);
	}

	bool has_vertex_colors = vertex_colors != NULL;
	if (has_vertex_colors) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_line_color_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), vertex_colors,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(2);
		glVertexAttrib3f(2, 1.f, 1.f, 1.f);
	}

	if (fog_active) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_fog_depth_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * sizeof(float), fog_depth,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(3);
		glVertexAttrib1f(3, 0.f);
	}

	if (blend) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(blend_src, blend_dst, blend_src_alpha, blend_dst_alpha);
	} else {
		glDisable(GL_BLEND);
	}
	if (depth_enabled) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		// Wireframe overlays sit at the same NDC z as their underlying
		// triangle edges. Disable depth writes so the line doesn't push a
		// fresh depth value into the buffer (which would then occlude further
		// front-facing triangles drawn in the same frame). Read-only depth
		// test still correctly culls back-facing lines.
		glDepthMask(GL_FALSE);
		glDepthRangef(0.f, 1.f);
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	// Viewport + scissor already applied by bridge_bind_target above.

	(void)glGetError();
	glDrawArrays(GL_LINES, 0, vertex_count);
	GLenum error = glGetError();
	backend->bridge_last_draw_gl_error = error;

	glDisableVertexAttribArray(0);
	if (has_line_distance)
		glDisableVertexAttribArray(1);
	if (has_vertex_colors)
		glDisableVertexAttribArray(2);
	if (fog_active)
		glDisableVertexAttribArray(3);
	glDisable(GL_SCISSOR_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge line draw failed: 0x%x", error);
		return false;
	}

	bridge_mark_readback(backend, &target);
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge queued %d line vertices at %dx%d -> %ux%u",
			 vertex_count, width, height, canvas->width, canvas->height);
	return true;
#endif
}

bool nx_webgl_egl_draw_textured_triangles_bridge(
	nx_webgl_egl_t *backend,
	nx_canvas_t *canvas,
	const float *clip_xyzuv,
	int vertex_count,
	uint32_t texture_id,
	uint32_t texture_revision,
	int texture_width,
	int texture_height,
	const uint8_t *texture_rgba,
	uint32_t texture_persistent_handle,
	uint32_t min_filter,
	uint32_t mag_filter,
	uint32_t wrap_s,
	uint32_t wrap_t,
	bool blend,
	uint32_t blend_src,
	uint32_t blend_dst,
	uint32_t blend_src_alpha,
	uint32_t blend_dst_alpha,
	const int *viewport,
	bool scissor_enabled,
	const int *scissor_box,
	bool depth_enabled,
	const float *fog_depth,
	bool fog_enabled,
	const float *fog_color,
	float fog_near,
	float fog_far,
	const float *normals,
	bool lighting_enabled,
	const float *light_direction,
	const float *light_color,
	const float *ambient_light_color,
	const float *view_positions,
	bool point_light_enabled,
	const float *point_light_position,
	const float *point_light_color,
	float point_light_distance,
	float point_light_decay,
	bool light2_enabled,
	const float *light_direction2,
	const float *light_color2,
	bool fog_exp2_enabled,
	float fog_density,
	const float *map_transform,
	bool has_map_transform,
	bool cull_enabled,
	uint32_t cull_face_mode,
	const float *diffuse_color,
	bool specular_enabled,
	const float *specular_color,
	float shininess,
	const float *emissive_color,
	bool use_derivative_normals,
	bool hemi_light_enabled,
	const float *hemi_light_direction,
	const float *hemi_light_sky_color,
	const float *hemi_light_ground_color,
	const nx_webgl_egl_spot_light_t *spot_light) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)clip_xyzuv;
	(void)vertex_count;
	(void)texture_id;
	(void)texture_revision;
	(void)texture_width;
	(void)texture_height;
	(void)texture_rgba;
	(void)texture_persistent_handle;
	(void)min_filter;
	(void)mag_filter;
	(void)wrap_s;
	(void)wrap_t;
	(void)blend;
	(void)blend_src;
	(void)blend_dst;
	(void)blend_src_alpha;
	(void)blend_dst_alpha;
	(void)viewport;
	(void)scissor_enabled;
	(void)scissor_box;
	(void)depth_enabled;
	(void)fog_depth;
	(void)fog_enabled;
	(void)fog_color;
	(void)fog_near;
	(void)fog_far;
	(void)normals;
	(void)lighting_enabled;
	(void)light_direction;
	(void)light_color;
	(void)ambient_light_color;
	(void)view_positions;
	(void)point_light_enabled;
	(void)point_light_position;
	(void)point_light_color;
	(void)point_light_distance;
	(void)point_light_decay;
	(void)light2_enabled;
	(void)light_direction2;
	(void)light_color2;
	(void)fog_exp2_enabled;
	(void)fog_density;
	(void)map_transform;
	(void)has_map_transform;
	(void)cull_enabled;
	(void)cull_face_mode;
	(void)diffuse_color;
	(void)specular_enabled;
	(void)specular_color;
	(void)shininess;
	(void)emissive_color;
	(void)use_derivative_normals;
	(void)hemi_light_enabled;
	(void)hemi_light_direction;
	(void)hemi_light_sky_color;
	(void)hemi_light_ground_color;
	(void)spot_light;
	return false;
#else
	// Persistent-handle textures (FBO color attachments) have no CPU `data` —
	// `texture_rgba` is NULL and `texture_id` may be 0 too. Treat that case as
	// valid: the dispatch will bind `texture_persistent_handle` directly,
	// skipping the per-draw `ensure_bridge_cached_texture` upload entirely.
	bool persistent_path = texture_persistent_handle != 0;
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0 || !clip_xyzuv ||
		vertex_count <= 0 || vertex_count % 3 != 0 ||
		texture_width <= 0 || texture_height <= 0) {
		nx_webgl_egl_append_dispatch_debug(backend, "T-args");
		return false;
	}
	if (!persistent_path && (!texture_rgba || texture_id == 0 ||
	                          texture_revision == 0)) {
		nx_webgl_egl_append_dispatch_debug(backend, "T-args");
		return false;
	}
	if (!nx_webgl_egl_initialize(backend, canvas)) {
		nx_webgl_egl_append_dispatch_debug(backend, "T-init");
		return false;
	}
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		nx_webgl_egl_append_dispatch_debug(backend, "T-mkcur");
		return false;
	}

	bridge_target_t target;
	if (!bridge_acquire_target(backend, canvas, &target) ||
		!ensure_bridge_texture_program(backend)) {
		nx_webgl_egl_append_dispatch_debug(backend, "T-rsrc");
		return false;
	}
	int width = target.width;
	int height = target.height;

	GLuint texture_handle;
	if (persistent_path) {
		// Persistent texture (FBO color attachment or createTexture+texImage2D
		// path). Apply sampler state directly to the user's GL texture each
		// draw so wrap/filter changes between bindings take effect.
		texture_handle = (GLuint)texture_persistent_handle;
		glBindTexture(GL_TEXTURE_2D, texture_handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                bridge_texture_filter(min_filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		                bridge_texture_filter(mag_filter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		                bridge_texture_wrap(wrap_s));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		                bridge_texture_wrap(wrap_t));
	} else {
		texture_handle = ensure_bridge_cached_texture(
			backend, texture_id, texture_revision, texture_width,
			texture_height, texture_rgba, min_filter, mag_filter, wrap_s,
			wrap_t);
		if (!texture_handle) {
			nx_webgl_egl_append_dispatch_debug(backend, "T-tex");
			return false;
		}
	}

	bridge_bind_target(&target, backend, canvas, viewport, scissor_enabled, scissor_box);
	// See cull_enabled comment in nx_webgl_egl_draw_triangles_bridge.
	if (cull_enabled) {
		GLenum gl_mode = GL_BACK;
		if (cull_face_mode == 0x0404u) gl_mode = GL_FRONT;
		else if (cull_face_mode == 0x0408u) gl_mode = GL_FRONT_AND_BACK;
		glEnable(GL_CULL_FACE);
		glCullFace(gl_mode);
	} else {
		glDisable(GL_CULL_FACE);
	}
	glUseProgram(backend->bridge_texture_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);
	if (backend->bridge_texture_sampler_loc >= 0)
		glUniform1i(backend->bridge_texture_sampler_loc, 0);
	// Apply Three.js's texture-transform mat3 (`texture.repeat` / `.offset`
	// / `.rotation` / `.center` baked into a 3x3 affine) to a_uv before
	// passing v_uv to the fragment shader. When not set, the shader uses
	// a_uv unchanged so callers that don't bind a transform are unaffected.
	if (backend->bridge_texture_map_transform_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_map_transform_enabled_loc,
					has_map_transform ? 1.f : 0.f);
	if (has_map_transform && map_transform &&
		backend->bridge_texture_map_transform_loc >= 0)
		glUniformMatrix3fv(backend->bridge_texture_map_transform_loc, 1, GL_FALSE,
						   map_transform);
	// Apply Three.js's `diffuse` / `u_color` uniform as a per-pixel multiplier
	// on the sampled texture color. Three.js's MeshBasicMaterial /
	// MeshLambertMaterial / MeshPhongMaterial all upload `diffuse` (vec3) and
	// the shader computes `diffuseColor *= map`. Default to identity (1,1,1)
	// when the caller doesn't bind a diffuse so existing demos that relied
	// on the un-tinted texture path are unaffected. Also fixes milestone #8's
	// per-sprite `material.color.setHSL` tint, which uploads to the same
	// `diffuse` uniform on SpriteMaterial.
	if (backend->bridge_texture_diffuse_loc >= 0) {
		if (diffuse_color) {
			glUniform3f(backend->bridge_texture_diffuse_loc, diffuse_color[0],
						diffuse_color[1], diffuse_color[2]);
		} else {
			glUniform3f(backend->bridge_texture_diffuse_loc, 1.f, 1.f, 1.f);
		}
	}
	bool fog_active = fog_enabled && fog_depth && fog_color;
	if (backend->bridge_texture_fog_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_fog_enabled_loc,
					fog_active ? 1.f : 0.f);
	if (backend->bridge_texture_fog_exp2_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_fog_exp2_enabled_loc,
					(fog_active && fog_exp2_enabled) ? 1.f : 0.f);
	if (fog_active) {
		if (backend->bridge_texture_fog_color_loc >= 0)
			glUniform3f(backend->bridge_texture_fog_color_loc, fog_color[0],
						fog_color[1], fog_color[2]);
		if (backend->bridge_texture_fog_near_loc >= 0)
			glUniform1f(backend->bridge_texture_fog_near_loc, fog_near);
		if (backend->bridge_texture_fog_far_loc >= 0)
			glUniform1f(backend->bridge_texture_fog_far_loc, fog_far);
		if (backend->bridge_texture_fog_density_loc >= 0)
			glUniform1f(backend->bridge_texture_fog_density_loc, fog_density);
	}
	// See color-path comment for the derivative-normals path. Same gating.
	bool normals_available = normals != NULL || use_derivative_normals;
	bool light_active = lighting_enabled && normals_available &&
						light_direction && light_color && ambient_light_color;
	if (backend->bridge_texture_lighting_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_lighting_enabled_loc,
					light_active ? 1.f : 0.f);
	if (backend->bridge_texture_use_derivative_normals_loc >= 0)
		glUniform1f(backend->bridge_texture_use_derivative_normals_loc,
					(light_active && use_derivative_normals) ? 1.f : 0.f);
	if (light_active) {
		if (backend->bridge_texture_light_direction_loc >= 0)
			glUniform3f(backend->bridge_texture_light_direction_loc,
						light_direction[0], light_direction[1],
						light_direction[2]);
		if (backend->bridge_texture_light_color_loc >= 0)
			glUniform3f(backend->bridge_texture_light_color_loc, light_color[0],
						light_color[1], light_color[2]);
		if (backend->bridge_texture_ambient_light_color_loc >= 0)
			glUniform3f(backend->bridge_texture_ambient_light_color_loc,
						ambient_light_color[0], ambient_light_color[1],
						ambient_light_color[2]);
	}
	bool light2_active = light_active && light2_enabled && light_direction2 &&
						 light_color2;
	if (backend->bridge_texture_light2_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_light2_enabled_loc,
					light2_active ? 1.f : 0.f);
	if (light2_active) {
		if (backend->bridge_texture_light_direction2_loc >= 0)
			glUniform3f(backend->bridge_texture_light_direction2_loc,
						light_direction2[0], light_direction2[1],
						light_direction2[2]);
		if (backend->bridge_texture_light_color2_loc >= 0)
			glUniform3f(backend->bridge_texture_light_color2_loc,
						light_color2[0], light_color2[1], light_color2[2]);
	}
	bool view_positions_active = light_active && view_positions != NULL;
	bool point_active = view_positions_active && point_light_enabled &&
						point_light_position && point_light_color;
	if (backend->bridge_texture_point_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_point_light_enabled_loc,
					point_active ? 1.f : 0.f);
	if (point_active) {
		if (backend->bridge_texture_point_light_position_loc >= 0)
			glUniform3f(backend->bridge_texture_point_light_position_loc,
						point_light_position[0], point_light_position[1],
						point_light_position[2]);
		if (backend->bridge_texture_point_light_color_loc >= 0)
			glUniform3f(backend->bridge_texture_point_light_color_loc,
						point_light_color[0], point_light_color[1],
						point_light_color[2]);
		if (backend->bridge_texture_point_light_distance_loc >= 0)
			glUniform1f(backend->bridge_texture_point_light_distance_loc,
						point_light_distance);
		if (backend->bridge_texture_point_light_decay_loc >= 0)
			glUniform1f(backend->bridge_texture_point_light_decay_loc,
						point_light_decay);
	}
	// Blinn-Phong specular (textured path) — same gating as the color path:
	// needs view-positions (for V) AND specular+shininess uniforms bound.
	bool specular_active = view_positions_active && specular_enabled &&
						   specular_color;
	if (backend->bridge_texture_specular_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_specular_enabled_loc,
					specular_active ? 1.f : 0.f);
	if (specular_active) {
		if (backend->bridge_texture_specular_loc >= 0)
			glUniform3f(backend->bridge_texture_specular_loc, specular_color[0],
						specular_color[1], specular_color[2]);
		if (backend->bridge_texture_shininess_loc >= 0)
			glUniform1f(backend->bridge_texture_shininess_loc, shininess);
	}
	// Always upload emissive so a prior program's value can't leak. Default
	// zero is a no-op (additive term `gl_FragColor.rgb += u_emissive`).
	if (backend->bridge_texture_emissive_loc >= 0) {
		if (emissive_color) {
			glUniform3f(backend->bridge_texture_emissive_loc, emissive_color[0],
						emissive_color[1], emissive_color[2]);
		} else {
			glUniform3f(backend->bridge_texture_emissive_loc, 0.f, 0.f, 0.f);
		}
	}
	// HemisphereLight (Three.js) — see color-program dispatch comment.
	bool hemi_active = light_active && hemi_light_enabled &&
					   hemi_light_direction && hemi_light_sky_color &&
					   hemi_light_ground_color;
	if (backend->bridge_texture_hemi_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_hemi_light_enabled_loc,
					hemi_active ? 1.f : 0.f);
	if (hemi_active) {
		if (backend->bridge_texture_hemi_light_direction_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_direction_loc,
						hemi_light_direction[0], hemi_light_direction[1],
						hemi_light_direction[2]);
		if (backend->bridge_texture_hemi_light_sky_color_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_sky_color_loc,
						hemi_light_sky_color[0], hemi_light_sky_color[1],
						hemi_light_sky_color[2]);
		if (backend->bridge_texture_hemi_light_ground_color_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_ground_color_loc,
						hemi_light_ground_color[0], hemi_light_ground_color[1],
						hemi_light_ground_color[2]);
	} else {
		if (backend->bridge_texture_hemi_light_direction_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_direction_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_texture_hemi_light_sky_color_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_sky_color_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_texture_hemi_light_ground_color_loc >= 0)
			glUniform3f(backend->bridge_texture_hemi_light_ground_color_loc,
						0.f, 0.f, 0.f);
	}
	// SpotLight (texture program) — mirrors the color-program dispatch.
	bool spot_active = view_positions_active && spot_light &&
					   spot_light->enabled && spot_light->position &&
					   spot_light->direction && spot_light->color;
	if (backend->bridge_texture_spot_light_enabled_loc >= 0)
		glUniform1f(backend->bridge_texture_spot_light_enabled_loc,
					spot_active ? 1.f : 0.f);
	if (spot_active) {
		if (backend->bridge_texture_spot_light_position_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_position_loc,
						spot_light->position[0], spot_light->position[1],
						spot_light->position[2]);
		if (backend->bridge_texture_spot_light_direction_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_direction_loc,
						spot_light->direction[0], spot_light->direction[1],
						spot_light->direction[2]);
		if (backend->bridge_texture_spot_light_color_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_color_loc,
						spot_light->color[0], spot_light->color[1],
						spot_light->color[2]);
		if (backend->bridge_texture_spot_light_distance_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_distance_loc,
						spot_light->distance);
		if (backend->bridge_texture_spot_light_cone_cos_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_cone_cos_loc,
						spot_light->cone_cos);
		if (backend->bridge_texture_spot_light_penumbra_cos_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_penumbra_cos_loc,
						spot_light->penumbra_cos);
		if (backend->bridge_texture_spot_light_decay_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_decay_loc,
						spot_light->decay);
	} else {
		if (backend->bridge_texture_spot_light_position_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_position_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_texture_spot_light_direction_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_direction_loc,
						0.f, 0.f, 1.f);
		if (backend->bridge_texture_spot_light_color_loc >= 0)
			glUniform3f(backend->bridge_texture_spot_light_color_loc,
						0.f, 0.f, 0.f);
		if (backend->bridge_texture_spot_light_distance_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_distance_loc, 0.f);
		if (backend->bridge_texture_spot_light_cone_cos_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_cone_cos_loc, 1.f);
		if (backend->bridge_texture_spot_light_penumbra_cos_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_penumbra_cos_loc, 1.f);
		if (backend->bridge_texture_spot_light_decay_loc >= 0)
			glUniform1f(backend->bridge_texture_spot_light_decay_loc, 0.f);
	}
	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5,
						  (const void *)(sizeof(float) * 3));
	if (fog_active) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_fog_depth_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * sizeof(float), fog_depth,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(2);
		glVertexAttrib1f(2, 0.f);
	}
	// See color-path comment: normals buffer skipped under derivative-normals.
	if (light_active && normals) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_normal_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), normals,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(3);
		glVertexAttrib3f(3, 0.f, 0.f, 1.f);
	}
	if (view_positions_active) {
		glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_view_position_buffer);
		glBufferData(GL_ARRAY_BUFFER,
					 (size_t)vertex_count * 3 * sizeof(float), view_positions,
					 GL_STREAM_DRAW);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, 0);
	} else {
		glDisableVertexAttribArray(4);
		glVertexAttrib3f(4, 0.f, 0.f, 0.f);
	}
	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	if (blend) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(blend_src, blend_dst, blend_src_alpha, blend_dst_alpha);
	} else {
		glDisable(GL_BLEND);
	}
	if (depth_enabled) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glDepthRangef(0.f, 1.f);
	} else {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	// Viewport + scissor already applied by bridge_bind_target above.

	// Workaround for the bridge "first draw of frame loses its first ~1-2
	// primitives" bug. The GPU pipeline state hasn't fully validated by the
	// time the first drawArrays after gl.clear() runs, and 1-2 primitives
	// emit corrupted output (textured: stuck on one bright texel; untextured:
	// no output at all).
	//
	// Iteration history:
	//   (1) Color-masked warmup — failed: drivers optimize away fully-masked
	//       draws (no output → no rasterization needed → state never
	//       validates), and the real draw is still effectively "first".
	//   (2) Degenerate (zero-area) warmup — failed: drivers also optimize
	//       away zero-area triangles before/during the rasterizer, so vertex
	//       shader execution may be skipped and state doesn't validate.
	//   (3) Off-screen 6-vertex (2 triangles) warmup — current: vertices at
	//       NDC x=10 are real, non-degenerate triangles. They go through the
	//       vertex shader (state validates), THEN the clip stage rejects
	//       them (entirely outside [-1,1] NDC volume). Rasterizer never
	//       sees them, no visible fragments emitted. 2 triangles absorbs
	//       up to 2 corrupted primitives.
	if (backend->bridge_pending_textured_warmup) {
		backend->bridge_pending_textured_warmup = false;
		// 6 vertices forming 2 triangles, all at NDC x in [10,11] —
		// entirely outside the [-1,1] NDC volume, clipped after vertex
		// shader runs but before rasterization. Layout: 5 floats per
		// vertex (x, y, z, u, v) matching the textured shader's attribs.
		static const float bridge_warmup_off_screen_xyzuv[30] = {
			10.f, 0.f, 0.f, 0.f, 0.f,
			11.f, 0.f, 0.f, 0.f, 0.f,
			10.f, 1.f, 0.f, 0.f, 0.f,
			11.f, 0.f, 0.f, 0.f, 0.f,
			11.f, 1.f, 0.f, 0.f, 0.f,
			10.f, 1.f, 0.f, 0.f, 0.f,
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(bridge_warmup_off_screen_xyzuv),
					 bridge_warmup_off_screen_xyzuv, GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		(void)glGetError();
	}

	// Tessellation-fix hook (textured triangles, stride 5: x,y,z,u,v).
	// Subdivides screen-large input triangles into smaller leaves
	// before the underlying glDrawArrays. Off by default and does NOT
	// fix the white-face/dark-face rasterizer bug in its current form
	// — see the big STATE comment block above `tessellate_one_triangle`
	// for why and what would actually work. Hook kept in place as
	// scaffolding for a future clip-space-correct re-implementation.
	const float *draw_xyzuv = clip_xyzuv;
	int draw_vertex_count = vertex_count;
	if (backend->tessellation_fix_enabled) {
		int subdivided = tessellate_textured_soup(
			backend, clip_xyzuv, vertex_count, 5, width, height);
		if (subdivided >= 3) {
			draw_xyzuv = backend->tessellation_scratch;
			draw_vertex_count = subdivided;
		}
	}

	glBufferData(GL_ARRAY_BUFFER, (size_t)draw_vertex_count * 5 * sizeof(float),
				 draw_xyzuv, GL_STREAM_DRAW);

	(void)glGetError();
	glDrawArrays(GL_TRIANGLES, 0, draw_vertex_count);
	GLenum error = glGetError();
	if (point_active)
		glDisableVertexAttribArray(4);
	if (light_active && normals)
		glDisableVertexAttribArray(3);
	if (fog_active)
		glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glDisable(GL_SCISSOR_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw failed: 0x%x", error);
		char tag[32];
		snprintf(tag, sizeof(tag), "T-gl(0x%x)", error);
		nx_webgl_egl_append_dispatch_debug(backend, tag);
		return false;
	}

	bridge_mark_readback(backend, &target);
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge queued textured %d vertices (in=%d) at %dx%d -> %ux%u",
			 draw_vertex_count, vertex_count, width, height,
			 canvas->width, canvas->height);
	{
		char tag[16];
		snprintf(tag, sizeof(tag), "T+%d", vertex_count / 3);
		nx_webgl_egl_append_dispatch_debug(backend, tag);
	}
	return true;
#endif
}

bool nx_webgl_egl_clear_prototype(nx_webgl_egl_t *backend,
								  nx_canvas_t *canvas) {
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
#if NXJS_HAS_EGL_GLES
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	GLuint texture = 0;
	GLuint framebuffer = 0;
	if (backend->surface == EGL_NO_SURFACE) {
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA,
					 GL_UNSIGNED_BYTE, NULL);
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							   GL_TEXTURE_2D, texture, 0);
		GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
			snprintf(backend->status, sizeof(backend->status),
					 "Offscreen GLES FBO incomplete: 0x%x",
					 framebuffer_status);
			glDeleteFramebuffers(1, &framebuffer);
			glDeleteTextures(1, &texture);
			return false;
		}
	}
	glClearColor((GLfloat)backend->clear_color[0],
				 (GLfloat)backend->clear_color[1],
				 (GLfloat)backend->clear_color[2],
				 (GLfloat)backend->clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);
	GLenum error = glGetError();
	glFinish();
	if (framebuffer)
		glDeleteFramebuffers(1, &framebuffer);
	if (texture)
		glDeleteTextures(1, &texture);
	snprintf(backend->status, sizeof(backend->status),
			 error == GL_NO_ERROR ? "Offscreen EGL/GLES clear completed"
								  : "Offscreen EGL/GLES clear failed: 0x%x",
			 error);
	return error == GL_NO_ERROR;
#else
	return false;
#endif
}

#if NXJS_HAS_EGL_GLES
static GLuint compile_triangle_shader(GLenum type, const char *source,
									  char *status, size_t status_size) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
		snprintf(status, status_size, "glCreateShader() failed: 0x%x",
				 glGetError());
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLchar log[128];
		GLsizei log_length = 0;
		glGetShaderInfoLog(shader, sizeof(log), &log_length, log);
		snprintf(status, status_size, "glCompileShader() failed: %.*s",
				 (int)log_length, log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}
#endif

JSValue nx_webgl_egl_triangle_readback(JSContext *ctx,
									   nx_webgl_egl_t *backend,
									   nx_canvas_t *canvas) {
	if (!nx_webgl_egl_initialize(backend, canvas))
		return make_triangle_result(ctx, false,
									backend ? backend->status
											: "EGL backend was not allocated",
									NULL);
#if !NXJS_HAS_EGL_GLES
	return make_triangle_result(ctx, false,
								"EGL/OpenGL ES support was not built.", NULL);
#else
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "triangle: eglMakeCurrent() failed: 0x%x", eglGetError());
		return make_triangle_result(ctx, false, backend->status, NULL);
	}

	static const char vertex_source[] =
		"attribute vec2 a_position;\n"
		"void main() {\n"
		"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"void main() {\n"
		"  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
		"}\n";
	static const GLfloat vertices[] = {
		-0.8f, -0.8f,
		0.8f, -0.8f,
		0.0f, 0.8f,
	};

	uint8_t pixel[4] = {0, 0, 0, 0};
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLuint program = 0;
	GLuint texture = 0;
	GLuint framebuffer = 0;
	GLuint vertex_buffer = 0;
	JSValue result = JS_UNDEFINED;

	vertex_shader = compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
											backend->status,
											sizeof(backend->status));
	if (!vertex_shader) {
		result = make_triangle_result(ctx, false, backend->status, pixel);
		goto cleanup;
	}

	fragment_shader = compile_triangle_shader(GL_FRAGMENT_SHADER,
											  fragment_source,
											  backend->status,
											  sizeof(backend->status));
	if (!fragment_shader) {
		result = make_triangle_result(ctx, false, backend->status, pixel);
		goto cleanup;
	}

	program = glCreateProgram();
	if (!program) {
		snprintf(backend->status, sizeof(backend->status),
				 "triangle: glCreateProgram() failed: 0x%x", glGetError());
		result = make_triangle_result(ctx, false, backend->status, pixel);
		goto cleanup;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glBindAttribLocation(program, 0, "a_position");
	glLinkProgram(program);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLchar log[128];
		GLsizei log_length = 0;
		glGetProgramInfoLog(program, sizeof(log), &log_length, log);
		snprintf(backend->status, sizeof(backend->status),
				 "triangle: glLinkProgram() failed: %.*s", (int)log_length,
				 log);
		result = make_triangle_result(ctx, false, backend->status, pixel);
		goto cleanup;
	}

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, texture, 0);
	GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		snprintf(backend->status, sizeof(backend->status),
				 "triangle: framebuffer incomplete: 0x%x",
				 framebuffer_status);
		result = make_triangle_result(ctx, false, backend->status, pixel);
		goto cleanup;
	}

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glViewport(0, 0, 32, 32);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(program);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

	GLenum error = glGetError();
	glFinish();
	bool ok = error == GL_NO_ERROR && pixel[0] >= 200 && pixel[1] <= 40 &&
			  pixel[2] <= 40 && pixel[3] >= 200;
	if (ok) {
		snprintf(backend->status, sizeof(backend->status),
				 "Offscreen GLES triangle readback completed: rgba(%u,%u,%u,%u)",
				 pixel[0], pixel[1], pixel[2], pixel[3]);
	} else {
		snprintf(backend->status, sizeof(backend->status),
				 "Offscreen GLES triangle readback failed: err=0x%x rgba(%u,%u,%u,%u)",
				 error, pixel[0], pixel[1], pixel[2], pixel[3]);
	}
	result = make_triangle_result(ctx, ok, backend->status, pixel);

cleanup:
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	if (vertex_buffer)
		glDeleteBuffers(1, &vertex_buffer);
	if (framebuffer)
		glDeleteFramebuffers(1, &framebuffer);
	if (texture)
		glDeleteTextures(1, &texture);
	if (program)
		glDeleteProgram(program);
	if (vertex_shader)
		glDeleteShader(vertex_shader);
	if (fragment_shader)
		glDeleteShader(fragment_shader);

	return result;
#endif
}

JSValue nx_webgl_egl_bridge_framebuffer(JSContext *ctx,
										nx_webgl_egl_t *backend,
										nx_canvas_t *canvas) {
	if (!canvas || !canvas->data || canvas->width == 0 || canvas->height == 0) {
		return make_bridge_result(ctx, false,
								  "bridge: canvas backing buffer is unavailable",
								  0, 0, 0, NULL);
	}
	if (!nx_webgl_egl_initialize(backend, canvas))
		return make_bridge_result(ctx, false,
								  backend ? backend->status
										  : "EGL backend was not allocated",
								  0, 0, 0, NULL);
#if !NXJS_HAS_EGL_GLES
	return make_bridge_result(ctx, false,
							  "EGL/OpenGL ES support was not built.", 0, 0, 0,
							  NULL);
#else
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge: eglMakeCurrent() failed: 0x%x", eglGetError());
		return make_bridge_result(ctx, false, backend->status, 0, 0, 0, NULL);
	}

	int width = canvas->width < 256 ? (int)canvas->width : 256;
	int height = canvas->height < 144 ? (int)canvas->height : 144;
	if (width <= 0 || height <= 0)
		return make_bridge_result(ctx, false,
								  "bridge: invalid canvas dimensions", 0, 0, 0,
								  NULL);

	static const char vertex_source[] =
		"attribute vec2 a_position;\n"
		"attribute vec3 a_color;\n"
		"varying vec3 v_color;\n"
		"void main() {\n"
		"  v_color = a_color;\n"
		"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"varying vec3 v_color;\n"
		"void main() {\n"
		"  gl_FragColor = vec4(v_color, 1.0);\n"
		"}\n";
	static const GLfloat vertices[] = {
		// x, y, r, g, b
		-0.9f, -0.8f, 1.f, 0.f, 0.f,
		0.9f, -0.8f, 0.f, 1.f, 0.f,
		0.0f, 0.9f, 0.f, 0.2f, 1.f,
	};

	size_t readback_size = (size_t)width * (size_t)height * 4;
	uint8_t *readback = js_malloc(ctx, readback_size);
	if (!readback)
		return JS_ThrowOutOfMemory(ctx);
	memset(readback, 0, readback_size);

	uint8_t sample[4] = {0, 0, 0, 0};
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLuint program = 0;
	GLuint texture = 0;
	GLuint framebuffer = 0;
	GLuint vertex_buffer = 0;
	JSValue result = JS_UNDEFINED;

	vertex_shader = compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
											backend->status,
											sizeof(backend->status));
	if (!vertex_shader) {
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	fragment_shader = compile_triangle_shader(GL_FRAGMENT_SHADER,
											  fragment_source,
											  backend->status,
											  sizeof(backend->status));
	if (!fragment_shader) {
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	program = glCreateProgram();
	if (!program) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge: glCreateProgram() failed: 0x%x", glGetError());
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glBindAttribLocation(program, 0, "a_position");
	glBindAttribLocation(program, 1, "a_color");
	glLinkProgram(program);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLchar log[128];
		GLsizei log_length = 0;
		glGetProgramInfoLog(program, sizeof(log), &log_length, log);
		snprintf(backend->status, sizeof(backend->status),
				 "bridge: glLinkProgram() failed: %.*s", (int)log_length,
				 log);
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, texture, 0);
	GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge: framebuffer incomplete: 0x%x",
				 framebuffer_status);
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(program);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5,
						  (const void *)(sizeof(GLfloat) * 2));
	glClearColor(0.02f, 0.08f, 0.14f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readback);

	GLenum error = glGetError();
	glFinish();
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge: glReadPixels/draw failed: 0x%x", error);
		result = make_bridge_result(ctx, false, backend->status, width, height,
									0, sample);
		goto cleanup;
	}

	int dst_x = ((int)canvas->width - width) / 2;
	int dst_y = ((int)canvas->height - height) / 2;
	if (dst_x < 0)
		dst_x = 0;
	if (dst_y < 0)
		dst_y = 0;
	uint32_t *dst = (uint32_t *)canvas->data;
	for (int y = 0; y < height; y++) {
		int src_y = height - 1 - y;
		for (int x = 0; x < width; x++) {
			uint8_t *src = readback + ((size_t)src_y * width + x) * 4;
			uint32_t packed = ((uint32_t)src[3] << 24) |
							  ((uint32_t)src[0] << 16) |
							  ((uint32_t)src[1] << 8) | (uint32_t)src[2];
			dst[(size_t)(dst_y + y) * canvas->width + (dst_x + x)] = packed;
		}
	}
	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);

	int sample_x = width / 2;
	int sample_y = height / 2;
	uint8_t *sample_src =
		readback + ((size_t)(height - 1 - sample_y) * width + sample_x) * 4;
	memcpy(sample, sample_src, sizeof(sample));
	snprintf(backend->status, sizeof(backend->status),
			 "Offscreen GLES bridge copied %dx%d (%d px), center rgba(%u,%u,%u,%u)",
			 width, height, width * height, sample[0], sample[1], sample[2],
			 sample[3]);
	result = make_bridge_result(ctx, true, backend->status, width, height,
								width * height, sample);

cleanup:
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	if (vertex_buffer)
		glDeleteBuffers(1, &vertex_buffer);
	if (framebuffer)
		glDeleteFramebuffers(1, &framebuffer);
	if (texture)
		glDeleteTextures(1, &texture);
	if (program)
		glDeleteProgram(program);
	if (vertex_shader)
		glDeleteShader(vertex_shader);
	if (fragment_shader)
		glDeleteShader(fragment_shader);
	js_free(ctx, readback);

	return result;
#endif
}

JSValue nx_webgl_egl_bridge_benchmark(JSContext *ctx,
									  nx_webgl_egl_t *backend,
									  nx_canvas_t *canvas,
									  int frame_count,
									  int requested_width,
									  int requested_height) {
	if (frame_count <= 0)
		frame_count = 120;
	if (frame_count > 600)
		frame_count = 600;

	if (!canvas || !canvas->data || canvas->width == 0 || canvas->height == 0) {
		return make_bridge_benchmark_result(
			ctx, false, "bridge benchmark: canvas backing buffer is unavailable",
			0, 0, frame_count, 0, 0., NULL);
	}
	if (!nx_webgl_egl_initialize(backend, canvas))
		return make_bridge_benchmark_result(
			ctx, false,
			backend ? backend->status : "EGL backend was not allocated", 0, 0,
			frame_count, 0, 0., NULL);
#if !NXJS_HAS_EGL_GLES
	return make_bridge_benchmark_result(
		ctx, false, "EGL/OpenGL ES support was not built.", 0, 0, frame_count,
		0, 0., NULL);
#else
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge benchmark: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return make_bridge_benchmark_result(ctx, false, backend->status, 0, 0,
											frame_count, 0, 0., NULL);
	}

	if (requested_width <= 0)
		requested_width = 256;
	if (requested_height <= 0)
		requested_height = 144;
	int width = (int)canvas->width < requested_width ? (int)canvas->width
													 : requested_width;
	int height = (int)canvas->height < requested_height ? (int)canvas->height
														: requested_height;
	if (width <= 0 || height <= 0) {
		return make_bridge_benchmark_result(
			ctx, false, "bridge benchmark: invalid canvas dimensions", 0, 0,
			frame_count, 0, 0., NULL);
	}

	static const char vertex_source[] =
		"attribute vec2 a_position;\n"
		"attribute vec3 a_color;\n"
		"varying vec3 v_color;\n"
		"void main() {\n"
		"  v_color = a_color;\n"
		"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"varying vec3 v_color;\n"
		"void main() {\n"
		"  gl_FragColor = vec4(v_color, 1.0);\n"
		"}\n";
	static const GLfloat vertices[] = {
		-0.9f, -0.8f, 1.f, 0.f, 0.f,
		0.9f, -0.8f, 0.f, 1.f, 0.f,
		0.0f, 0.9f, 0.f, 0.2f, 1.f,
	};

	size_t readback_size = (size_t)width * (size_t)height * 4;
	uint8_t *readback = js_malloc(ctx, readback_size);
	if (!readback)
		return JS_ThrowOutOfMemory(ctx);
	memset(readback, 0, readback_size);

	uint8_t sample[4] = {0, 0, 0, 0};
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLuint program = 0;
	GLuint texture = 0;
	GLuint framebuffer = 0;
	GLuint vertex_buffer = 0;
	JSValue result = JS_UNDEFINED;
	int total_copied_pixels = 0;
	double elapsed_ms = 0.;

	vertex_shader = compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
											backend->status,
											sizeof(backend->status));
	if (!vertex_shader) {
		result = make_bridge_benchmark_result(ctx, false, backend->status,
											  width, height, frame_count, 0,
											  0., sample);
		goto cleanup;
	}

	fragment_shader = compile_triangle_shader(GL_FRAGMENT_SHADER,
											  fragment_source,
											  backend->status,
											  sizeof(backend->status));
	if (!fragment_shader) {
		result = make_bridge_benchmark_result(ctx, false, backend->status,
											  width, height, frame_count, 0,
											  0., sample);
		goto cleanup;
	}

	program = glCreateProgram();
	if (!program) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge benchmark: glCreateProgram() failed: 0x%x",
				 glGetError());
		result = make_bridge_benchmark_result(ctx, false, backend->status,
											  width, height, frame_count, 0,
											  0., sample);
		goto cleanup;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glBindAttribLocation(program, 0, "a_position");
	glBindAttribLocation(program, 1, "a_color");
	glLinkProgram(program);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLchar log[128];
		GLsizei log_length = 0;
		glGetProgramInfoLog(program, sizeof(log), &log_length, log);
		snprintf(backend->status, sizeof(backend->status),
				 "bridge benchmark: glLinkProgram() failed: %.*s",
				 (int)log_length, log);
		result = make_bridge_benchmark_result(ctx, false, backend->status,
											  width, height, frame_count, 0,
											  0., sample);
		goto cleanup;
	}

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, texture, 0);
	GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		snprintf(backend->status, sizeof(backend->status),
				 "bridge benchmark: framebuffer incomplete: 0x%x",
				 framebuffer_status);
		result = make_bridge_benchmark_result(ctx, false, backend->status,
											  width, height, frame_count, 0,
											  0., sample);
		goto cleanup;
	}

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(program);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5,
						  (const void *)(sizeof(GLfloat) * 2));

	int dst_x = ((int)canvas->width - width) / 2;
	int dst_y = ((int)canvas->height - height) / 2;
	if (dst_x < 0)
		dst_x = 0;
	if (dst_y < 0)
		dst_y = 0;
	uint32_t *dst = (uint32_t *)canvas->data;

	u64 start_tick = armGetSystemTick();
	for (int frame = 0; frame < frame_count; frame++) {
		float pulse = (float)(frame % 60) / 59.f;
		glClearColor(0.02f + pulse * 0.12f, 0.08f, 0.14f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
					 readback);

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			snprintf(backend->status, sizeof(backend->status),
					 "bridge benchmark: frame %d failed: 0x%x", frame,
					 error);
			elapsed_ms =
				(double)armTicksToNs(armGetSystemTick() - start_tick) /
				1000000.;
			result = make_bridge_benchmark_result(
				ctx, false, backend->status, width, height, frame, 
				total_copied_pixels, elapsed_ms, sample);
			goto cleanup;
		}

		for (int y = 0; y < height; y++) {
			int src_y = height - 1 - y;
			for (int x = 0; x < width; x++) {
				uint8_t *src =
					readback + ((size_t)src_y * width + x) * 4;
				uint32_t packed = ((uint32_t)src[3] << 24) |
								  ((uint32_t)src[0] << 16) |
								  ((uint32_t)src[1] << 8) |
								  (uint32_t)src[2];
				dst[(size_t)(dst_y + y) * canvas->width + (dst_x + x)] =
					packed;
			}
		}
		total_copied_pixels += width * height;
		if (canvas->surface)
			cairo_surface_mark_dirty(canvas->surface);
	}
	glFinish();
	elapsed_ms =
		(double)armTicksToNs(armGetSystemTick() - start_tick) / 1000000.;

	int sample_x = width / 2;
	int sample_y = height / 2;
	uint8_t *sample_src =
		readback + ((size_t)(height - 1 - sample_y) * width + sample_x) * 4;
	memcpy(sample, sample_src, sizeof(sample));
	snprintf(backend->status, sizeof(backend->status),
			 "Bridge benchmark %d frames %dx%d: %.2f ms, %.2f fps",
			 frame_count, width, height, elapsed_ms,
			 elapsed_ms > 0. ? (double)frame_count * 1000. / elapsed_ms
							 : 0.);
	result = make_bridge_benchmark_result(ctx, true, backend->status, width,
										  height, frame_count,
										  total_copied_pixels, elapsed_ms,
										  sample);

cleanup:
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	if (vertex_buffer)
		glDeleteBuffers(1, &vertex_buffer);
	if (framebuffer)
		glDeleteFramebuffers(1, &framebuffer);
	if (texture)
		glDeleteTextures(1, &texture);
	if (program)
		glDeleteProgram(program);
	if (vertex_shader)
		glDeleteShader(vertex_shader);
	if (fragment_shader)
		glDeleteShader(fragment_shader);
	js_free(ctx, readback);

	return result;
#endif
}

JSValue nx_webgl_egl_get_backend_info(JSContext *ctx,
									  nx_webgl_egl_t *backend) {
	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;

	define_string(ctx, obj, "target", "EGL/OpenGL ES");
	define_bool(ctx, obj, "built", backend && backend->built);
	define_bool(ctx, obj, "available", backend && backend->available);
	define_string(ctx, obj, "status",
				  backend ? backend->status : "EGL backend was not allocated");
	define_int(ctx, obj, "probeStep", backend ? backend->step : 0);
	define_string(ctx, obj, "debugDispatchState",
				  backend ? backend->debug_dispatch_state : "");

#if NXJS_HAS_EGL_GLES
	define_int(ctx, obj, "eglMajor", backend ? backend->major : 0);
	define_int(ctx, obj, "eglMinor", backend ? backend->minor : 0);
	define_string(ctx, obj, "glVendor", backend ? backend->vendor : "");
	define_string(ctx, obj, "glVersion", backend ? backend->version : "");
	define_string(ctx, obj, "glRenderer", backend ? backend->renderer : "");
	define_int(ctx, obj, "bridgeRequestedWidth",
			   backend ? backend->bridge_requested_width : 0);
	define_int(ctx, obj, "bridgeRequestedHeight",
			   backend ? backend->bridge_requested_height : 0);
	define_int(ctx, obj, "bridgeRenderWidth",
			   backend ? backend->bridge_width : 0);
	define_int(ctx, obj, "bridgeRenderHeight",
			   backend ? backend->bridge_height : 0);
	define_bool(ctx, obj, "extInstancedArraysPresent",
				backend && backend->ext_instanced_arrays_present);
	define_bool(ctx, obj, "fnVertexAttribDivisor",
				backend && backend->fn_vertex_attrib_divisor_ext != NULL);
	define_bool(ctx, obj, "fnDrawArraysInstanced",
				backend && backend->fn_draw_arrays_instanced_ext != NULL);
	define_bool(ctx, obj, "fnDrawElementsInstanced",
				backend && backend->fn_draw_elements_instanced_ext != NULL);
	define_string(ctx, obj, "glExtensions",
				  backend ? backend->gl_extensions : "");
	// Bug-hunt: show the raw function pointer addresses + the address of
	// glDrawArrays for comparison. If glDrawArraysInstanced ===
	// glDrawArrays, eglGetProcAddress returned the wrong fn (silently
	// ignoring instance_count) and we need a different loading strategy.
	{
		char buf[64];
		void *pda = (void *)&glDrawArrays;
		void *pde = (void *)&glDrawElements;
		snprintf(buf, sizeof(buf), "%p", (void *)backend->fn_draw_arrays_instanced_ext);
		define_string(ctx, obj, "fnDrawArraysInstancedAddr", buf);
		snprintf(buf, sizeof(buf), "%p", pda);
		define_string(ctx, obj, "fnDrawArraysAddr", buf);
		snprintf(buf, sizeof(buf), "%p", (void *)backend->fn_draw_elements_instanced_ext);
		define_string(ctx, obj, "fnDrawElementsInstancedAddr", buf);
		snprintf(buf, sizeof(buf), "%p", pde);
		define_string(ctx, obj, "fnDrawElementsAddr", buf);
		snprintf(buf, sizeof(buf), "%p", (void *)backend->fn_vertex_attrib_divisor_ext);
		define_string(ctx, obj, "fnVertexAttribDivisorAddr", buf);
	}
#else
	define_int(ctx, obj, "eglMajor", 0);
	define_int(ctx, obj, "eglMinor", 0);
	define_string(ctx, obj, "glVendor", "");
	define_string(ctx, obj, "glVersion", "");
	define_string(ctx, obj, "glRenderer", "");
	define_int(ctx, obj, "bridgeRequestedWidth", 0);
	define_int(ctx, obj, "bridgeRequestedHeight", 0);
	define_int(ctx, obj, "bridgeRenderWidth", 0);
	define_int(ctx, obj, "bridgeRenderHeight", 0);
	define_bool(ctx, obj, "extInstancedArraysPresent", false);
	define_bool(ctx, obj, "fnVertexAttribDivisor", false);
	define_bool(ctx, obj, "fnDrawArraysInstanced", false);
	define_bool(ctx, obj, "fnDrawElementsInstanced", false);
	define_string(ctx, obj, "glExtensions", "");
#endif

	return obj;
}

bool nx_webgl_egl_compile_shader(nx_webgl_egl_t *backend, nx_canvas_t *canvas,
								 uint32_t shader_type, const char *source,
								 uint32_t *shader_handle,
								 bool *compile_status, char *info_log,
								 size_t info_log_size) {
	if (compile_status)
		*compile_status = false;
	if (info_log && info_log_size > 0)
		info_log[0] = '\0';
#if NXJS_HAS_EGL_GLES
	if (!backend || !source)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "shader compile: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	if (shader_handle && *shader_handle) {
		glDeleteShader((GLuint)*shader_handle);
		*shader_handle = 0;
	}

	GLuint handle = glCreateShader((GLenum)shader_type);
	if (!handle) {
		if (info_log && info_log_size > 0)
			snprintf(info_log, info_log_size, "glCreateShader failed: 0x%x",
					 glGetError());
		return true;
	}

	/* 2026-06-23 PMREM Tegra-compat rewrites — same-length string
	 * substitutions in PMREM's prefilter shader source applied before
	 * glShaderSource. Each fixes a different Tegra-incompatibility we've
	 * empirically observed crash glDrawArrays inside the prefilter pass:
	 *
	 *  (A) `GGX_SAMPLES 256` → `GGX_SAMPLES 1  `  (with trailing spaces
	 *      to preserve string length — same-length substitutions are
	 *      easier to reason about than length-changing ones). Caps
	 *      importance-sample loop iterations.
	 *  (B) `#define texture2DGradEXT textureGrad` → spaces. After this,
	 *      `#ifdef texture2DGradEXT` evaluates false and the cubeUV
	 *      sample falls back to plain `texture2D(envMap, uv)`. Tegra's
	 *      GLES compiler appears to produce incorrect GPU code for
	 *      `textureGrad(envMap, uv, vec2(0), vec2(0))` on RGBA16F
	 *      textures.
	 *  (C) `#define texture2DLodEXT textureLod` → spaces. Same defensive
	 *      removal; if any cubeUV path used the LodEXT variant we'd
	 *      hit the same hypothetical compiler issue.
	 *  (D) `roughness < 0.001` → `roughness < 9.999`  in PMREMGGXConvolution.
	 *      Roughness is in [0,1] so the comparison is now always true and
	 *      the GGX importance-sample loop is bypassed completely (single
	 *      `bilinearCubeUV` sample + early return). DIAGNOSTIC PROBE — if
	 *      draw #2 still crashes with this in place, the crash is NOT
	 *      shader semantics (loop/Hammersley/VNDF) but draw-call setup
	 *      (FBO bind / VAO / sampler-from-prev-FBO sync). If draw #2
	 *      survives, the loop body is the culprit.
	 *
	 * All same-length, scoped to PMREM's source pattern, and pass-through
	 * harmless for any non-PMREM shader.
	 */
	const char *patched_source = source;
	char *patched_owned = NULL;
	if (source) {
		const char *needle_a = "#define GGX_SAMPLES 256";
		const char *repl_a   = "#define GGX_SAMPLES 1  ";
		const char *needle_b = "#define texture2DGradEXT textureGrad";
		const char *repl_b   = "                                    ";
		const char *needle_c = "#define texture2DLodEXT textureLod";
		const char *repl_c   = "                                  ";
		const char *needle_d = "roughness < 0.001";
		const char *repl_d   = "roughness < 9.999";
		/* (E) Bypass the texture sample in the PMREM prefilter early-return
		 * path. Replace `bilinearCubeUV(envMap, N, mipInt)` (33 chars) with
		 * `vec3(1.0,0.5,0.25)               ` (33 chars). Magenta-ish
		 * constant — visually distinct if PMREM actually completes.
		 * Diagnostic: if draw #2 survives with this in place, the crash is
		 * the sampler chain (sampler binding, sampler-from-prev-FBO, or
		 * texture state). If draw #2 still crashes, the issue is something
		 * structural about the draw (VAO/program/attribute) that exists
		 * even without the sample. Lands on line 218 of the prefilter
		 * shader where the early-return path lives. */
		const char *needle_e = "bilinearCubeUV(envMap, N, mipInt)";
		const char *repl_e   = "vec3(1.0,0.5,0.25)               ";
		bool any_hit =
			(strstr(source, needle_a) != NULL) ||
			(strstr(source, needle_b) != NULL) ||
			(strstr(source, needle_c) != NULL) ||
			(strstr(source, needle_d) != NULL) ||
			(strstr(source, needle_e) != NULL);
		if (any_hit) {
			size_t source_len = strlen(source);
			patched_owned = (char *)malloc(source_len + 1);
			if (patched_owned) {
				memcpy(patched_owned, source, source_len + 1);
				char *hit;
				int n_a = 0, n_b = 0, n_c = 0, n_d = 0, n_e = 0;
				while ((hit = strstr(patched_owned, needle_a)) != NULL) {
					memcpy(hit, repl_a, strlen(repl_a)); n_a++;
				}
				while ((hit = strstr(patched_owned, needle_b)) != NULL) {
					memcpy(hit, repl_b, strlen(repl_b)); n_b++;
				}
				while ((hit = strstr(patched_owned, needle_c)) != NULL) {
					memcpy(hit, repl_c, strlen(repl_c)); n_c++;
				}
				while ((hit = strstr(patched_owned, needle_d)) != NULL) {
					memcpy(hit, repl_d, strlen(repl_d)); n_d++;
				}
				while ((hit = strstr(patched_owned, needle_e)) != NULL) {
					memcpy(hit, repl_e, strlen(repl_e)); n_e++;
				}
				patched_source = patched_owned;
				/* (J) Tegra-compat PMREM prefilter replacement: if this is
				 * the PMREMGGXConvolution fragment shader, substitute a
				 * hand-written minimal FS that omits the GGX importance-
				 * sample loop entirely. Tegra's GLSL compiler aborts
				 * glDrawArrays on the original even when the loop body
				 * is unreachable at runtime (early-return + sample-bypass
				 * proved the FS body presence alone is the trigger;
				 * minimal FS replacement made the draw survive).
				 *
				 * The replacement keeps the cubeUV math (getFace, getUV,
				 * bilinearCubeUV) so the prefilter pass actually produces
				 * usable output: a perfect-mirror downsample of the source
				 * envmap at each mip level (NO roughness-based blur).
				 * Materials referencing this PMREM result will look
				 * "sharp" at all roughness levels rather than progressively
				 * blurred — visually inferior to upstream but the only
				 * way to get PMREM through the Tegra/Mesa GLES pipeline
				 * without a driver abort. Better than nothing (raw
				 * equirect path) because cube_uv layout works with all
				 * Three.js materials including MeshStandardMaterial.
				 *
				 * Texel constants (CUBEUV_TEXEL_WIDTH/HEIGHT, CUBEUV_MAX_MIP)
				 * derived from Three.js's PMREM cube_uv layout: source
				 * texture is 1536×2048 = max-cube-face 256 (mip 0) + 6
				 * additional mip levels packed below, MAX_MIP=8. */
				int n_j = 0;
				if (shader_type == 0x8B30 /* GL_FRAGMENT_SHADER */ &&
				    strstr(patched_owned, "PMREMGGXConvolution") != NULL) {
					/* PMREMGGXConvolution replacement. Upstream uses
					 * 256-sample GGX VNDF importance sampling whose
					 * Hammersley sequence depends on uint+bitwise ops
					 * (radicalInverse_VdC) that Mesa Nouveau's GLSL
					 * compiler can't process even when unreachable —
					 * see [[reference-pmrem-tegra-compiler-workaround]].
					 *
					 * The replacement: 5-tap unrolled cross blur using
					 * a tangent/bitangent basis around the output normal.
					 * Each per-pass roughness scales the kernel radius;
					 * across PMREM's cumulative passes this produces a
					 * widening hemisphere coverage in the cube_uv mip
					 * chain. No uint, no bitwise, no early-return
					 * conditional, no function calls — every construct
					 * that crashed an earlier iteration during the
					 * 13-iteration bisect is excluded.
					 *
					 * CUBEUV_MAX_MIP / CUBEUV_TEXEL_WIDTH /
					 * CUBEUV_TEXEL_HEIGHT are PARSED from the source
					 * Three.js emits — hardcoding them only matches one
					 * cubeSize, which broke when the demo's equirect was
					 * sized to produce lodMax=9 instead of the
					 * hardcoded 8. UVs would land outside the texture
					 * and writes via multi-tap sampling crashed Mesa. */
					double max_mip = 8.0;
					double texel_w = 1.0 / 1536.0;
					double texel_h = 1.0 / 2048.0;
					{
						const char *p;
						p = strstr(patched_owned, "#define CUBEUV_MAX_MIP ");
						if (p) { p += strlen("#define CUBEUV_MAX_MIP "); max_mip = strtod(p, NULL); }
						p = strstr(patched_owned, "#define CUBEUV_TEXEL_WIDTH ");
						if (p) { p += strlen("#define CUBEUV_TEXEL_WIDTH "); texel_w = strtod(p, NULL); }
						p = strstr(patched_owned, "#define CUBEUV_TEXEL_HEIGHT ");
						if (p) { p += strlen("#define CUBEUV_TEXEL_HEIGHT "); texel_h = strtod(p, NULL); }
					}
					char *built = (char *)malloc(16384);
					if (built) {
						int written = snprintf(built, 16384,
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
							"  /* Sample 0 (N) — inline cube_uv math. */\n"
							"  { vec3 d = D0; float mi = mipInt; float face = getFace(d);\n"
							"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
							"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
							"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
							"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
							"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
							"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
							"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
							"    acc += texture(envMap, uv).rgb; }\n"
							"  /* Sample 1 (D1). */\n"
							"  { vec3 d = D1; float mi = mipInt; float face = getFace(d);\n"
							"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
							"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
							"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
							"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
							"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
							"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
							"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
							"    acc += texture(envMap, uv).rgb; }\n"
							"  /* Sample 2 (D2). */\n"
							"  { vec3 d = D2; float mi = mipInt; float face = getFace(d);\n"
							"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
							"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
							"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
							"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
							"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
							"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
							"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
							"    acc += texture(envMap, uv).rgb; }\n"
							"  /* Sample 3 (D3). */\n"
							"  { vec3 d = D3; float mi = mipInt; float face = getFace(d);\n"
							"    float fi = max(cubeUV_minMipLevel - mi, 0.0);\n"
							"    mi = max(mi, cubeUV_minMipLevel); float fs = exp2(mi);\n"
							"    vec2 uv = getUV(d, face) * (fs - 2.0) + 1.0;\n"
							"    if (face > 2.0) { uv.y += fs; face -= 3.0; }\n"
							"    uv.x += face * fs; uv.x += fi * 3.0 * cubeUV_minTileSize;\n"
							"    uv.y += 4.0 * (exp2(CUBEUV_MAX_MIP) - fs);\n"
							"    uv.x *= CUBEUV_TEXEL_WIDTH; uv.y *= CUBEUV_TEXEL_HEIGHT;\n"
							"    acc += texture(envMap, uv).rgb; }\n"
							"  /* Sample 4 (D4). */\n"
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
						if (written > 0 && written < 16384) {
							free(patched_owned);
							patched_owned = built;
							patched_source = patched_owned;
							n_j = 1;
						} else {
							free(built);
						}
					}
				}
				fprintf(stderr,
					"[nxjs:pmrem-fix] handle=%u rewrites: GGX=%d gradEXT=%d lodEXT=%d roughEarlyOut=%d sampleBypass=%d minimalFs=%d\n",
					(unsigned)handle, n_a, n_b, n_c, n_d, n_e, n_j);
				fflush(stderr);
			}
		}
	}
	const GLchar *sources[1] = {(const GLchar *)patched_source};
	glShaderSource(handle, 1, sources, NULL);
	glCompileShader(handle);

	GLint ok = GL_FALSE;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &ok);
	if (compile_status)
		*compile_status = ok == GL_TRUE;
	if (info_log && info_log_size > 0) {
		GLsizei written = 0;
		glGetShaderInfoLog(handle, (GLsizei)info_log_size, &written, info_log);
		info_log[info_log_size - 1] = '\0';
	}
	if (shader_handle)
		*shader_handle = (uint32_t)handle;
	if (patched_owned) free(patched_owned);
	return true;
#else
	(void)backend;
	(void)canvas;
	(void)shader_type;
	(void)source;
	(void)shader_handle;
	(void)info_log;
	(void)info_log_size;
	return false;
#endif
}

void nx_webgl_egl_delete_shader(nx_webgl_egl_t *backend,
								uint32_t shader_handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !shader_handle || !backend->available)
		return;
	if (eglMakeCurrent(backend->display, backend->surface, backend->surface,
					   backend->context))
		glDeleteShader((GLuint)shader_handle);
#else
	(void)backend;
	(void)shader_handle;
#endif
}

bool nx_webgl_egl_link_program(nx_webgl_egl_t *backend, nx_canvas_t *canvas,
							   uint32_t vertex_shader_handle,
							   uint32_t fragment_shader_handle,
							   const nx_webgl_attrib_binding_t *bindings,
							   int binding_count,
							   uint32_t *program_handle, bool *link_status,
							   char *info_log, size_t info_log_size) {
	if (link_status)
		*link_status = false;
	if (info_log && info_log_size > 0)
		info_log[0] = '\0';
#if NXJS_HAS_EGL_GLES
	if (!backend || !vertex_shader_handle || !fragment_shader_handle)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "program link: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	if (program_handle && *program_handle) {
		glDeleteProgram((GLuint)*program_handle);
		*program_handle = 0;
	}

	GLuint handle = glCreateProgram();
	if (!handle) {
		if (info_log && info_log_size > 0)
			snprintf(info_log, info_log_size, "glCreateProgram failed: 0x%x",
					 glGetError());
		return true;
	}

	glAttachShader(handle, (GLuint)vertex_shader_handle);
	glAttachShader(handle, (GLuint)fragment_shader_handle);
	// The runtime's bridge draw paths read the position attribute from
	// vertex_attribs[0], so pin "position" to location 0 in every linked
	// program. GLES linkers otherwise free-assign attribute locations, which
	// breaks any time another used attribute (e.g. "color" with vertexColors:
	// true) gets put at 0 instead. Other attribute names are tracked
	// per-program via getAttribLocation reflection (see color_attrib_index /
	// line_distance_attrib_index in webgl.c).
	glBindAttribLocation(handle, 0, "position");
	// Apply user-side bindAttribLocation calls collected program-side
	// in webgl.c. Later glBindAttribLocation calls override earlier
	// ones for the same name — which is what we want: explicit user
	// binds beat our default "position" pin if names collide.
	if (bindings && binding_count > 0) {
		for (int i = 0; i < binding_count; i++) {
			if (bindings[i].name) {
				glBindAttribLocation(handle, (GLuint)bindings[i].location,
									 bindings[i].name);
			}
		}
	}
	glLinkProgram(handle);

	GLint ok = GL_FALSE;
	glGetProgramiv(handle, GL_LINK_STATUS, &ok);
	bool link_ok = (ok == GL_TRUE);

	// WebGL conformance: when the user binds two attributes to the same
	// location via bindAttribLocation AND both attributes are active in
	// the linked program, the link must fail. The Tegra X1 GLES driver
	// is permissive and lets such aliased active attributes link, so we
	// enforce the rule JS-side. (When binding_count < 2 there can be no
	// aliasing among user binds; skip.)
	if (link_ok && bindings && binding_count >= 2) {
		GLint num_active = 0;
		glGetProgramiv(handle, GL_ACTIVE_ATTRIBUTES, &num_active);
		// Build an "is active" flag per binding by querying each name's
		// location through glGetAttribLocation — returns -1 if the name
		// isn't an active attribute. Much cheaper than enumerating all
		// active attributes (avoids the GetActiveAttrib loop).
		bool active[16];  // matches NX_WEBGL_MAX_ATTRIB_BINDINGS in webgl.c
		int n = binding_count < 16 ? binding_count : 16;
		for (int i = 0; i < n; i++) {
			active[i] = bindings[i].name &&
						glGetAttribLocation(handle, bindings[i].name) >= 0;
		}
		for (int i = 0; i < n && link_ok; i++) {
			if (!active[i]) continue;
			for (int j = i + 1; j < n; j++) {
				if (!active[j]) continue;
				if (bindings[i].location == bindings[j].location) {
					link_ok = false;
					if (info_log && info_log_size > 0) {
						snprintf(info_log, info_log_size,
							"active attributes '%s' and '%s' aliased to location %d",
							bindings[i].name, bindings[j].name,
							bindings[i].location);
					}
					break;
				}
			}
		}
	}

	if (link_status)
		*link_status = link_ok;
	if (link_ok && info_log && info_log_size > 0) {
		GLsizei written = 0;
		glGetProgramInfoLog(handle, (GLsizei)info_log_size, &written,
							info_log);
		info_log[info_log_size - 1] = '\0';
	}
	if (program_handle)
		*program_handle = (uint32_t)handle;
	return true;
#else
	(void)backend;
	(void)canvas;
	(void)vertex_shader_handle;
	(void)fragment_shader_handle;
	(void)program_handle;
	(void)info_log;
	(void)info_log_size;
	return false;
#endif
}

void nx_webgl_egl_delete_program(nx_webgl_egl_t *backend,
								 uint32_t program_handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available)
		return;
	if (eglMakeCurrent(backend->display, backend->surface, backend->surface,
					   backend->context))
		glDeleteProgram((GLuint)program_handle);
#else
	(void)backend;
	(void)program_handle;
#endif
}

int nx_webgl_egl_get_attrib_location(nx_webgl_egl_t *backend,
									uint32_t program_handle,
									const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available || !name)
		return -1;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return -1;
	return glGetAttribLocation((GLuint)program_handle, name);
#else
	(void)backend;
	(void)program_handle;
	(void)name;
	return -1;
#endif
}

int nx_webgl_egl_get_uniform_location(nx_webgl_egl_t *backend,
									 uint32_t program_handle,
									 const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available || !name)
		return -1;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return -1;
	return glGetUniformLocation((GLuint)program_handle, name);
#else
	(void)backend;
	(void)program_handle;
	(void)name;
	return -1;
#endif
}

bool nx_webgl_egl_get_active_attrib(nx_webgl_egl_t *backend,
									uint32_t program_handle,
									uint32_t index,
									char *name,
									size_t name_size,
									int *size,
									uint32_t *type) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available || !name ||
		name_size == 0)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	// Drain any stale GL error so the post-call glGetError() reflects ONLY
	// glGetActiveAttrib's result. See [[bridge-stale-glerror-trap]].
	(void)glGetError();
	name[0] = '\0';
	if (size) *size = 0;
	if (type) *type = 0;
	glGetActiveAttrib((GLuint)program_handle, index, (GLsizei)name_size, NULL,
					  (GLint *)size, (GLenum *)type, name);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend;
	(void)program_handle;
	(void)index;
	(void)name;
	(void)name_size;
	(void)size;
	(void)type;
	return false;
#endif
}

bool nx_webgl_egl_get_active_uniform(nx_webgl_egl_t *backend,
									 uint32_t program_handle,
									 uint32_t index,
									 char *name,
									 size_t name_size,
									 int *size,
									 uint32_t *type) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available || !name ||
		name_size == 0)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	// Drain any stale GL error so the post-call glGetError() reflects ONLY
	// glGetActiveUniform's result. See [[bridge-stale-glerror-trap]].
	(void)glGetError();
	name[0] = '\0';
	if (size) *size = 0;
	if (type) *type = 0;
	glGetActiveUniform((GLuint)program_handle, index, (GLsizei)name_size, NULL,
					   (GLint *)size, (GLenum *)type, name);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend;
	(void)program_handle;
	(void)index;
	(void)name;
	(void)name_size;
	(void)size;
	(void)type;
	return false;
#endif
}

bool nx_webgl_egl_get_program_iv(nx_webgl_egl_t *backend,
								 uint32_t program_handle,
								 uint32_t pname,
								 int *out_value) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !program_handle || !backend->available || !out_value)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	// Drain any stale GL error so the post-call glGetError() reflects ONLY
	// glGetProgramiv's result. See [[bridge-stale-glerror-trap]]. A stale
	// error here would make us falsely return false, and the JS-side
	// fallback returns countof(active_uniforms)=12 — Three.js then iterates
	// 12 slots, native returns empty name for slots that don't exist,
	// parseUniform's regex crashes on the empty name.
	(void)glGetError();
	GLint v = 0;
	glGetProgramiv((GLuint)program_handle, (GLenum)pname, &v);
	if (glGetError() != GL_NO_ERROR)
		return false;
	*out_value = (int)v;
	return true;
#else
	(void)backend;
	(void)program_handle;
	(void)pname;
	(void)out_value;
	return false;
#endif
}

// ============================================================================
// WebGL 2 (GLES 3) trampolines.
// ----------------------------------------------------------------------------
// Each function:
//  - Refuses if the backend isn't initialized / not available
//  - eglMakeCurrent's so calls land on our context
//  - Drains glGetError() FIRST per [[bridge-stale-glerror-trap]] when reporting
//    success/failure via the post-call error code
//  - Casts the resolved function pointer to a local typedef and trampolines
//
// Constants for ES3 enums used below (kept local so the header stays
// GLES2-only).
#if NXJS_HAS_EGL_GLES
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif
#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif
#ifndef GL_MAX_3D_TEXTURE_SIZE
#define GL_MAX_3D_TEXTURE_SIZE 0x8073
#endif
#ifndef GL_MAX_ARRAY_TEXTURE_LAYERS
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
#endif
#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS 0x8824
#endif
#ifndef GL_MAX_COLOR_ATTACHMENTS
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#endif
#ifndef GL_MAX_UNIFORM_BUFFER_BINDINGS
#define GL_MAX_UNIFORM_BUFFER_BINDINGS 0x8A2F
#endif
#ifndef GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#endif
#ifndef GL_MAX_VERTEX_UNIFORM_BLOCKS
#define GL_MAX_VERTEX_UNIFORM_BLOCKS 0x8A2B
#endif
#ifndef GL_MAX_FRAGMENT_UNIFORM_BLOCKS
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS 0x8A2D
#endif
#ifndef GL_MAX_COMBINED_UNIFORM_BLOCKS
#define GL_MAX_COMBINED_UNIFORM_BLOCKS 0x8A2E
#endif
#ifndef GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS 0x8C80
#endif
#ifndef GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C7A
#endif
#ifndef GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#endif
#ifndef GL_MAX_ELEMENT_INDEX
#define GL_MAX_ELEMENT_INDEX 0x8D6B
#endif
#ifndef GL_MAX_ELEMENTS_VERTICES
#define GL_MAX_ELEMENTS_VERTICES 0x80E8
#endif
#ifndef GL_MAX_ELEMENTS_INDICES
#define GL_MAX_ELEMENTS_INDICES 0x80E9
#endif
#ifndef GL_MAX_SERVER_WAIT_TIMEOUT
#define GL_MAX_SERVER_WAIT_TIMEOUT 0x9111
#endif
#ifndef GL_MAX_PROGRAM_TEXEL_OFFSET
#define GL_MAX_PROGRAM_TEXEL_OFFSET 0x8905
#endif
#ifndef GL_MIN_PROGRAM_TEXEL_OFFSET
#define GL_MIN_PROGRAM_TEXEL_OFFSET 0x8904
#endif
#ifndef GL_MAX_VARYING_COMPONENTS
#define GL_MAX_VARYING_COMPONENTS 0x8B4B
#endif
#ifndef GL_MAX_VERTEX_UNIFORM_COMPONENTS
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#endif
#ifndef GL_MAX_FRAGMENT_UNIFORM_COMPONENTS
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#endif
#ifndef GL_MAX_VERTEX_OUTPUT_COMPONENTS
#define GL_MAX_VERTEX_OUTPUT_COMPONENTS 0x9122
#endif
#ifndef GL_MAX_FRAGMENT_INPUT_COMPONENTS
#define GL_MAX_FRAGMENT_INPUT_COMPONENTS 0x9125
#endif
#ifndef GL_MAX_TEXTURE_LOD_BIAS
#define GL_MAX_TEXTURE_LOD_BIAS 0x84FD
#endif
#endif // NXJS_HAS_EGL_GLES

bool nx_webgl_egl_has_webgl2(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->webgl2_present;
#else
	(void)backend;
	return false;
#endif
}

bool nx_webgl_egl_ensure_initialized(nx_webgl_egl_t *backend,
                                      nx_canvas_t *canvas) {
	return nx_webgl_egl_initialize(backend, canvas);
}

bool nx_webgl_egl_persistent_texture_sub_image_2d(nx_webgl_egl_t *backend,
                                                    uint32_t handle,
                                                    int level,
                                                    int xoffset, int yoffset,
                                                    int width, int height,
                                                    uint32_t format,
                                                    uint32_t type,
                                                    const void *pixels) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || !handle)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	(void)glGetError(); /* swallow any pre-existing error */
	// On ES 3.0, GL_HALF_FLOAT_OES (0x8D61) is invalid as a sub-image type;
	// the native enum is GL_HALF_FLOAT (0x140B). Translate.
	GLenum native_type = (GLenum)type;
	if (native_type == 0x8D61)
		native_type = 0x140B;
	glBindTexture(GL_TEXTURE_2D, (GLuint)handle);
	(void)glGetError();
	glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, (GLsizei)width,
	                (GLsizei)height, (GLenum)format, native_type, pixels);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)handle; (void)level; (void)xoffset; (void)yoffset;
	(void)width; (void)height; (void)format; (void)type; (void)pixels;
	return false;
#endif
}

// VAOs ----------------------------------------------------------------------

uint32_t nx_webgl_egl_gen_vertex_array(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || !backend->fn_gen_vertex_arrays)
		return 0;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return 0;
	typedef void (*pfn_t)(GLsizei, GLuint *);
	pfn_t gen = (pfn_t)backend->fn_gen_vertex_arrays;
	GLuint h = 0;
	gen(1, &h);
	return (uint32_t)h;
#else
	(void)backend;
	return 0;
#endif
}

void nx_webgl_egl_delete_vertex_array(nx_webgl_egl_t *backend,
                                       uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->available || !handle ||
		!backend->fn_delete_vertex_arrays)
		return;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return;
	if (backend->current_user_vao == handle)
		backend->current_user_vao = 0;
	typedef void (*pfn_t)(GLsizei, const GLuint *);
	pfn_t del = (pfn_t)backend->fn_delete_vertex_arrays;
	GLuint h = (GLuint)handle;
	del(1, &h);
#else
	(void)backend;
	(void)handle;
#endif
}

void nx_webgl_egl_set_user_vao(nx_webgl_egl_t *backend, uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!backend)
		return;
	backend->current_user_vao = (GLuint)handle;
	// Push to native immediately so non-passthrough state-setting calls
	// (bindBuffer / vertexAttribPointer / enableVertexAttribArray) land
	// on the right VAO.
	if (backend->available && backend->fn_bind_vertex_array &&
		eglMakeCurrent(backend->display, backend->surface, backend->surface,
					   backend->context)) {
		typedef void (*pfn_t)(GLuint);
		pfn_t bind = (pfn_t)backend->fn_bind_vertex_array;
		bind((GLuint)handle);
	}
#else
	(void)backend;
	(void)handle;
#endif
}

uint32_t nx_webgl_egl_get_user_vao(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend ? (uint32_t)backend->current_user_vao : 0;
#else
	(void)backend;
	return 0;
#endif
}

// Helper: make-current + (void)glGetError() drain. Returns false on failure.
#if NXJS_HAS_EGL_GLES
static bool webgl2_make_current(nx_webgl_egl_t *backend) {
	if (!backend || !backend->available)
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	(void)glGetError();
	return true;
}
#endif

// drawBuffers / invalidate / blit / read / MSAA / texture layer ------------

void nx_webgl_egl_draw_buffers(nx_webgl_egl_t *backend, int n,
                                const uint32_t *bufs) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_draw_buffers)
		return;
	typedef void (*pfn_t)(GLsizei, const GLenum *);
	((pfn_t)backend->fn_draw_buffers)((GLsizei)n, (const GLenum *)bufs);
#else
	(void)backend; (void)n; (void)bufs;
#endif
}

void nx_webgl_egl_invalidate_framebuffer(nx_webgl_egl_t *backend,
                                          uint32_t target, int n,
                                          const uint32_t *attachments) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_invalidate_framebuffer)
		return;
	typedef void (*pfn_t)(GLenum, GLsizei, const GLenum *);
	((pfn_t)backend->fn_invalidate_framebuffer)((GLenum)target, (GLsizei)n,
	                                            (const GLenum *)attachments);
#else
	(void)backend; (void)target; (void)n; (void)attachments;
#endif
}

void nx_webgl_egl_invalidate_sub_framebuffer(nx_webgl_egl_t *backend,
                                              uint32_t target, int n,
                                              const uint32_t *attachments,
                                              int x, int y, int w, int h) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_invalidate_sub_framebuffer)
		return;
	typedef void (*pfn_t)(GLenum, GLsizei, const GLenum *, GLint, GLint,
	                       GLsizei, GLsizei);
	((pfn_t)backend->fn_invalidate_sub_framebuffer)(
		(GLenum)target, (GLsizei)n, (const GLenum *)attachments,
		x, y, (GLsizei)w, (GLsizei)h);
#else
	(void)backend; (void)target; (void)n; (void)attachments;
	(void)x; (void)y; (void)w; (void)h;
#endif
}

void nx_webgl_egl_blit_framebuffer(nx_webgl_egl_t *backend,
                                    int srcX0, int srcY0, int srcX1, int srcY1,
                                    int dstX0, int dstY0, int dstX1, int dstY1,
                                    uint32_t mask, uint32_t filter) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_blit_framebuffer)
		return;
	typedef void (*pfn_t)(GLint, GLint, GLint, GLint, GLint, GLint, GLint,
	                       GLint, GLbitfield, GLenum);
	((pfn_t)backend->fn_blit_framebuffer)(srcX0, srcY0, srcX1, srcY1,
	                                       dstX0, dstY0, dstX1, dstY1,
	                                       (GLbitfield)mask, (GLenum)filter);
#else
	(void)backend; (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1;
	(void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1;
	(void)mask; (void)filter;
#endif
}

void nx_webgl_egl_read_buffer(nx_webgl_egl_t *backend, uint32_t src) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_read_buffer)
		return;
	typedef void (*pfn_t)(GLenum);
	((pfn_t)backend->fn_read_buffer)((GLenum)src);
#else
	(void)backend; (void)src;
#endif
}

bool nx_webgl_egl_renderbuffer_storage_multisample(nx_webgl_egl_t *backend,
                                                    uint32_t handle,
                                                    int samples,
                                                    uint32_t internalformat,
                                                    int width, int height) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_renderbuffer_storage_multisample)
		return false;
	glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)handle);
	typedef void (*pfn_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
	((pfn_t)backend->fn_renderbuffer_storage_multisample)(
		GL_RENDERBUFFER, (GLsizei)samples, (GLenum)internalformat,
		(GLsizei)width, (GLsizei)height);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)handle; (void)samples; (void)internalformat;
	(void)width; (void)height;
	return false;
#endif
}

bool nx_webgl_egl_framebuffer_texture_layer(nx_webgl_egl_t *backend,
                                             uint32_t framebuffer_handle,
                                             uint32_t attachment,
                                             uint32_t texture_handle,
                                             int level, int layer) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_framebuffer_texture_layer)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)framebuffer_handle);
	typedef void (*pfn_t)(GLenum, GLenum, GLuint, GLint, GLint);
	((pfn_t)backend->fn_framebuffer_texture_layer)(
		GL_FRAMEBUFFER, (GLenum)attachment, (GLuint)texture_handle,
		(GLint)level, (GLint)layer);
	GLenum err = glGetError();
	// Restore previously-bound user FBO (which the bridge tracks). The
	// JS wrapper sets framebuffer_binding state separately.
	glBindFramebuffer(GL_FRAMEBUFFER, backend->current_user_framebuffer);
	return err == GL_NO_ERROR;
#else
	(void)backend; (void)framebuffer_handle; (void)attachment;
	(void)texture_handle; (void)level; (void)layer;
	return false;
#endif
}

// 3D texture upload + immutable storage ------------------------------------

bool nx_webgl_egl_tex_image_3d(nx_webgl_egl_t *backend,
                                uint32_t target, int level,
                                uint32_t internalformat,
                                int width, int height, int depth,
                                int border, uint32_t format, uint32_t type,
                                const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_tex_image_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei,
	                       GLint, GLenum, GLenum, const void *);
	((pfn_t)backend->fn_tex_image_3d)((GLenum)target, level,
	                                   (GLint)internalformat,
	                                   width, height, depth, border,
	                                   (GLenum)format, (GLenum)type, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)internalformat;
	(void)width; (void)height; (void)depth; (void)border;
	(void)format; (void)type; (void)data;
	return false;
#endif
}

bool nx_webgl_egl_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                    uint32_t target, int level,
                                    int xoff, int yoff, int zoff,
                                    int width, int height, int depth,
                                    uint32_t format, uint32_t type,
                                    const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_tex_sub_image_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLint, GLint, GLint, GLint, GLsizei,
	                       GLsizei, GLsizei, GLenum, GLenum, const void *);
	((pfn_t)backend->fn_tex_sub_image_3d)((GLenum)target, level,
	                                       xoff, yoff, zoff,
	                                       width, height, depth,
	                                       (GLenum)format, (GLenum)type, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)xoff; (void)yoff;
	(void)zoff; (void)width; (void)height; (void)depth;
	(void)format; (void)type; (void)data;
	return false;
#endif
}

bool nx_webgl_egl_copy_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                         uint32_t target, int level,
                                         int xoff, int yoff, int zoff,
                                         int x, int y, int w, int h) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_copy_tex_sub_image_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLint, GLint, GLint, GLint, GLint, GLint,
	                       GLsizei, GLsizei);
	((pfn_t)backend->fn_copy_tex_sub_image_3d)((GLenum)target, level,
	                                            xoff, yoff, zoff,
	                                            x, y, w, h);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)xoff; (void)yoff;
	(void)zoff; (void)x; (void)y; (void)w; (void)h;
	return false;
#endif
}

/* 2026-06-08 ROUND 38: copyTexImage2D + copyTexSubImage2D — core GLES2
 * (no eglGetProcAddress needed). Were missing from nxjs WebGL; Cocos's
 * multi-camera render-target pipeline (CameraLawn_RT → CameraLawn
 * composition) silently no-op'd the RT-to-texture copy → pvzge gameplay
 * area rendered black. Generic Web API behavior, every WebGL2 engine
 * using render targets needs these. */
bool nx_webgl_egl_copy_tex_image_2d(nx_webgl_egl_t *backend,
                                     uint32_t target, int level,
                                     uint32_t internalformat,
                                     int x, int y, int w, int h, int border) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend)) return false;
	glCopyTexImage2D((GLenum)target, level, (GLenum)internalformat,
	                  x, y, (GLsizei)w, (GLsizei)h, border);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)internalformat;
	(void)x; (void)y; (void)w; (void)h; (void)border;
	return false;
#endif
}

bool nx_webgl_egl_copy_tex_sub_image_2d(nx_webgl_egl_t *backend,
                                         uint32_t target, int level,
                                         int xoff, int yoff,
                                         int x, int y, int w, int h) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend)) return false;
	glCopyTexSubImage2D((GLenum)target, level, xoff, yoff,
	                     x, y, (GLsizei)w, (GLsizei)h);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)xoff; (void)yoff;
	(void)x; (void)y; (void)w; (void)h;
	return false;
#endif
}

bool nx_webgl_egl_compressed_tex_image_3d(nx_webgl_egl_t *backend,
                                           uint32_t target, int level,
                                           uint32_t internalformat,
                                           int width, int height, int depth,
                                           int border, size_t image_size,
                                           const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_compressed_tex_image_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei,
	                       GLint, GLsizei, const void *);
	((pfn_t)backend->fn_compressed_tex_image_3d)((GLenum)target, level,
	                                              (GLenum)internalformat,
	                                              width, height, depth, border,
	                                              (GLsizei)image_size, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)internalformat;
	(void)width; (void)height; (void)depth; (void)border;
	(void)image_size; (void)data;
	return false;
#endif
}

bool nx_webgl_egl_compressed_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                               uint32_t target, int level,
                                               int xoff, int yoff, int zoff,
                                               int width, int height, int depth,
                                               uint32_t format, size_t image_size,
                                               const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_compressed_tex_sub_image_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei,
	                       GLsizei, GLenum, GLsizei, const void *);
	((pfn_t)backend->fn_compressed_tex_sub_image_3d)(
		(GLenum)target, level, xoff, yoff, zoff,
		width, height, depth, (GLenum)format, (GLsizei)image_size, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)xoff; (void)yoff;
	(void)zoff; (void)width; (void)height; (void)depth;
	(void)format; (void)image_size; (void)data;
	return false;
#endif
}

bool nx_webgl_egl_tex_storage_2d(nx_webgl_egl_t *backend,
                                  uint32_t target, int levels,
                                  uint32_t internalformat,
                                  int width, int height) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_tex_storage_2d)
		return false;
	typedef void (*pfn_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
	((pfn_t)backend->fn_tex_storage_2d)((GLenum)target, (GLsizei)levels,
	                                     (GLenum)internalformat,
	                                     (GLsizei)width, (GLsizei)height);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)levels; (void)internalformat;
	(void)width; (void)height;
	return false;
#endif
}

bool nx_webgl_egl_tex_storage_3d(nx_webgl_egl_t *backend,
                                  uint32_t target, int levels,
                                  uint32_t internalformat,
                                  int width, int height, int depth) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_tex_storage_3d)
		return false;
	typedef void (*pfn_t)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
	((pfn_t)backend->fn_tex_storage_3d)((GLenum)target, (GLsizei)levels,
	                                     (GLenum)internalformat,
	                                     (GLsizei)width, (GLsizei)height,
	                                     (GLsizei)depth);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)levels; (void)internalformat;
	(void)width; (void)height; (void)depth;
	return false;
#endif
}

// clearBuffer family --------------------------------------------------------

void nx_webgl_egl_clear_buffer_iv(nx_webgl_egl_t *backend, uint32_t buffer,
                                   int drawbuffer, const int *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_clear_buffer_iv)
		return;
	typedef void (*pfn_t)(GLenum, GLint, const GLint *);
	((pfn_t)backend->fn_clear_buffer_iv)((GLenum)buffer, (GLint)drawbuffer,
	                                      value);
#else
	(void)backend; (void)buffer; (void)drawbuffer; (void)value;
#endif
}

void nx_webgl_egl_clear_buffer_uiv(nx_webgl_egl_t *backend, uint32_t buffer,
                                    int drawbuffer, const uint32_t *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_clear_buffer_uiv)
		return;
	typedef void (*pfn_t)(GLenum, GLint, const GLuint *);
	((pfn_t)backend->fn_clear_buffer_uiv)((GLenum)buffer, (GLint)drawbuffer,
	                                       (const GLuint *)value);
#else
	(void)backend; (void)buffer; (void)drawbuffer; (void)value;
#endif
}

void nx_webgl_egl_clear_buffer_fv(nx_webgl_egl_t *backend, uint32_t buffer,
                                   int drawbuffer, const float *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_clear_buffer_fv)
		return;
	typedef void (*pfn_t)(GLenum, GLint, const GLfloat *);
	((pfn_t)backend->fn_clear_buffer_fv)((GLenum)buffer, (GLint)drawbuffer,
	                                      value);
#else
	(void)backend; (void)buffer; (void)drawbuffer; (void)value;
#endif
}

void nx_webgl_egl_clear_buffer_fi(nx_webgl_egl_t *backend, uint32_t buffer,
                                   int drawbuffer, float depth, int stencil) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_clear_buffer_fi)
		return;
	typedef void (*pfn_t)(GLenum, GLint, GLfloat, GLint);
	((pfn_t)backend->fn_clear_buffer_fi)((GLenum)buffer, (GLint)drawbuffer,
	                                      depth, stencil);
#else
	(void)backend; (void)buffer; (void)drawbuffer; (void)depth; (void)stencil;
#endif
}

// Integer vertex attributes ------------------------------------------------

void nx_webgl_egl_vertex_attrib_i_pointer(nx_webgl_egl_t *backend,
                                           uint32_t index, int size,
                                           uint32_t type, int stride,
                                           int offset) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_vertex_attrib_i_pointer)
		return;
	typedef void (*pfn_t)(GLuint, GLint, GLenum, GLsizei, const void *);
	((pfn_t)backend->fn_vertex_attrib_i_pointer)((GLuint)index, size,
	                                              (GLenum)type,
	                                              (GLsizei)stride,
	                                              (const void *)(intptr_t)offset);
#else
	(void)backend; (void)index; (void)size; (void)type; (void)stride;
	(void)offset;
#endif
}

void nx_webgl_egl_vertex_attrib_i4i(nx_webgl_egl_t *backend, uint32_t index,
                                     int x, int y, int z, int w) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_vertex_attrib_i4i)
		return;
	typedef void (*pfn_t)(GLuint, GLint, GLint, GLint, GLint);
	((pfn_t)backend->fn_vertex_attrib_i4i)((GLuint)index, x, y, z, w);
#else
	(void)backend; (void)index; (void)x; (void)y; (void)z; (void)w;
#endif
}

void nx_webgl_egl_vertex_attrib_i4ui(nx_webgl_egl_t *backend, uint32_t index,
                                      uint32_t x, uint32_t y, uint32_t z,
                                      uint32_t w) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_vertex_attrib_i4ui)
		return;
	typedef void (*pfn_t)(GLuint, GLuint, GLuint, GLuint, GLuint);
	((pfn_t)backend->fn_vertex_attrib_i4ui)((GLuint)index, x, y, z, w);
#else
	(void)backend; (void)index; (void)x; (void)y; (void)z; (void)w;
#endif
}

// Uint + non-square matrix uniforms ----------------------------------------

void nx_webgl_egl_uniform1ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform1ui)
		return;
	typedef void (*pfn_t)(GLint, GLuint);
	((pfn_t)backend->fn_uniform1ui)(location, x);
#else
	(void)backend; (void)location; (void)x;
#endif
}

void nx_webgl_egl_uniform2ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform2ui)
		return;
	typedef void (*pfn_t)(GLint, GLuint, GLuint);
	((pfn_t)backend->fn_uniform2ui)(location, x, y);
#else
	(void)backend; (void)location; (void)x; (void)y;
#endif
}

void nx_webgl_egl_uniform3ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y, uint32_t z) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform3ui)
		return;
	typedef void (*pfn_t)(GLint, GLuint, GLuint, GLuint);
	((pfn_t)backend->fn_uniform3ui)(location, x, y, z);
#else
	(void)backend; (void)location; (void)x; (void)y; (void)z;
#endif
}

void nx_webgl_egl_uniform4ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform4ui)
		return;
	typedef void (*pfn_t)(GLint, GLuint, GLuint, GLuint, GLuint);
	((pfn_t)backend->fn_uniform4ui)(location, x, y, z, w);
#else
	(void)backend; (void)location; (void)x; (void)y; (void)z; (void)w;
#endif
}

void nx_webgl_egl_uniform1uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform1uiv)
		return;
	typedef void (*pfn_t)(GLint, GLsizei, const GLuint *);
	((pfn_t)backend->fn_uniform1uiv)(location, (GLsizei)count, value);
#else
	(void)backend; (void)location; (void)count; (void)value;
#endif
}

void nx_webgl_egl_uniform2uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform2uiv)
		return;
	typedef void (*pfn_t)(GLint, GLsizei, const GLuint *);
	((pfn_t)backend->fn_uniform2uiv)(location, (GLsizei)count, value);
#else
	(void)backend; (void)location; (void)count; (void)value;
#endif
}

void nx_webgl_egl_uniform3uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform3uiv)
		return;
	typedef void (*pfn_t)(GLint, GLsizei, const GLuint *);
	((pfn_t)backend->fn_uniform3uiv)(location, (GLsizei)count, value);
#else
	(void)backend; (void)location; (void)count; (void)value;
#endif
}

void nx_webgl_egl_uniform4uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform4uiv)
		return;
	typedef void (*pfn_t)(GLint, GLsizei, const GLuint *);
	((pfn_t)backend->fn_uniform4uiv)(location, (GLsizei)count, value);
#else
	(void)backend; (void)location; (void)count; (void)value;
#endif
}

#define NX_WEBGL2_MATRIX_NXM_IMPL(name, fnfield)                          \
void name(nx_webgl_egl_t *backend, int location, int count, bool transpose,\
           const float *value) {                                           \
	if (!webgl2_make_current(backend) || !backend->fnfield)                 \
		return;                                                             \
	typedef void (*pfn_t)(GLint, GLsizei, GLboolean, const GLfloat *);      \
	((pfn_t)backend->fnfield)(location, (GLsizei)count,                     \
	                          transpose ? GL_TRUE : GL_FALSE, value);       \
}

#if NXJS_HAS_EGL_GLES
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix2x3fv, fn_uniform_matrix2x3fv)
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix3x2fv, fn_uniform_matrix3x2fv)
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix2x4fv, fn_uniform_matrix2x4fv)
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix4x2fv, fn_uniform_matrix4x2fv)
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix3x4fv, fn_uniform_matrix3x4fv)
NX_WEBGL2_MATRIX_NXM_IMPL(nx_webgl_egl_uniform_matrix4x3fv, fn_uniform_matrix4x3fv)
#else
void nx_webgl_egl_uniform_matrix2x3fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
void nx_webgl_egl_uniform_matrix3x2fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
void nx_webgl_egl_uniform_matrix2x4fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
void nx_webgl_egl_uniform_matrix4x2fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
void nx_webgl_egl_uniform_matrix3x4fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
void nx_webgl_egl_uniform_matrix4x3fv(nx_webgl_egl_t *b, int l, int c, bool t, const float *v) { (void)b;(void)l;(void)c;(void)t;(void)v; }
#endif

// Buffer copy + readback ---------------------------------------------------

void nx_webgl_egl_copy_buffer_sub_data(nx_webgl_egl_t *backend,
                                        uint32_t read_target,
                                        uint32_t write_target,
                                        size_t read_offset, size_t write_offset,
                                        size_t size) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_copy_buffer_sub_data)
		return;
	typedef void (*pfn_t)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr);
	((pfn_t)backend->fn_copy_buffer_sub_data)((GLenum)read_target,
	                                           (GLenum)write_target,
	                                           (GLintptr)read_offset,
	                                           (GLintptr)write_offset,
	                                           (GLsizeiptr)size);
#else
	(void)backend; (void)read_target; (void)write_target; (void)read_offset;
	(void)write_offset; (void)size;
#endif
}

void nx_webgl_egl_get_buffer_sub_data(nx_webgl_egl_t *backend,
                                       uint32_t target, size_t offset,
                                       size_t size, void *dst) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_buffer_sub_data)
		return;
	typedef void (*pfn_t)(GLenum, GLintptr, GLsizeiptr, void *);
	((pfn_t)backend->fn_get_buffer_sub_data)((GLenum)target, (GLintptr)offset,
	                                          (GLsizeiptr)size, dst);
#else
	(void)backend; (void)target; (void)offset; (void)size; (void)dst;
#endif
}

// UBO surface --------------------------------------------------------------

void nx_webgl_egl_bind_buffer_base(nx_webgl_egl_t *backend, uint32_t target,
                                    uint32_t index, uint32_t buffer) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_bind_buffer_base)
		return;
	typedef void (*pfn_t)(GLenum, GLuint, GLuint);
	((pfn_t)backend->fn_bind_buffer_base)((GLenum)target, (GLuint)index,
	                                       (GLuint)buffer);
	// Track UBO slot bindings so we can re-apply them at the top of every
	// passthrough draw (Mesa Nouveau resets indexed buffer bindings on
	// eglMakeCurrent — see passthrough re-application loop).
	if (target == 0x8A11 /* GL_UNIFORM_BUFFER */ &&
	    index < NX_WEBGL_MAX_UBO_BINDINGS) {
		backend->ubo_indexed_bindings[index] = (GLuint)buffer;
		backend->ubo_indexed_offsets[index] = 0;
		backend->ubo_indexed_sizes[index] = 0;
	}
#else
	(void)backend; (void)target; (void)index; (void)buffer;
#endif
}

void nx_webgl_egl_bind_buffer_range(nx_webgl_egl_t *backend, uint32_t target,
                                     uint32_t index, uint32_t buffer,
                                     size_t offset, size_t size) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_bind_buffer_range)
		return;
	typedef void (*pfn_t)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr);
	((pfn_t)backend->fn_bind_buffer_range)((GLenum)target, (GLuint)index,
	                                        (GLuint)buffer, (GLintptr)offset,
	                                        (GLsizeiptr)size);
	if (target == 0x8A11 /* GL_UNIFORM_BUFFER */ &&
	    index < NX_WEBGL_MAX_UBO_BINDINGS) {
		backend->ubo_indexed_bindings[index] = (GLuint)buffer;
		backend->ubo_indexed_offsets[index] = (GLintptr)offset;
		backend->ubo_indexed_sizes[index] = (GLsizeiptr)size;
	}
#else
	(void)backend; (void)target; (void)index; (void)buffer; (void)offset;
	(void)size;
#endif
}

// (UBO binding re-apply is inlined at the top of nx_webgl_egl_draw_passthrough
// to avoid a static/extern forward-declaration dance.)

uint32_t nx_webgl_egl_get_uniform_block_index(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_uniform_block_index)
		return 0xFFFFFFFFu;
	typedef GLuint (*pfn_t)(GLuint, const GLchar *);
	return (uint32_t)((pfn_t)backend->fn_get_uniform_block_index)(
		(GLuint)program_handle, (const GLchar *)name);
#else
	(void)backend; (void)program_handle; (void)name;
	return 0xFFFFFFFFu;
#endif
}

void nx_webgl_egl_uniform_block_binding(nx_webgl_egl_t *backend,
                                         uint32_t program_handle,
                                         uint32_t block_index,
                                         uint32_t binding) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_uniform_block_binding)
		return;
	typedef void (*pfn_t)(GLuint, GLuint, GLuint);
	((pfn_t)backend->fn_uniform_block_binding)((GLuint)program_handle,
	                                            (GLuint)block_index,
	                                            (GLuint)binding);
#else
	(void)backend; (void)program_handle; (void)block_index; (void)binding;
#endif
}

bool nx_webgl_egl_get_active_uniform_block_iv(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                uint32_t block_index,
                                                uint32_t pname,
                                                int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_active_uniform_block_iv ||
		!out)
		return false;
	typedef void (*pfn_t)(GLuint, GLuint, GLenum, GLint *);
	((pfn_t)backend->fn_get_active_uniform_block_iv)((GLuint)program_handle,
	                                                  (GLuint)block_index,
	                                                  (GLenum)pname, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program_handle; (void)block_index; (void)pname;
	(void)out;
	return false;
#endif
}

bool nx_webgl_egl_get_active_uniform_block_name(nx_webgl_egl_t *backend,
                                                  uint32_t program_handle,
                                                  uint32_t block_index,
                                                  char *name, size_t name_size) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_get_active_uniform_block_name || !name || !name_size)
		return false;
	typedef void (*pfn_t)(GLuint, GLuint, GLsizei, GLsizei *, GLchar *);
	GLsizei written = 0;
	((pfn_t)backend->fn_get_active_uniform_block_name)((GLuint)program_handle,
	                                                    (GLuint)block_index,
	                                                    (GLsizei)name_size,
	                                                    &written, name);
	if (name_size > 0)
		name[name_size - 1] = '\0';
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program_handle; (void)block_index;
	(void)name; (void)name_size;
	return false;
#endif
}

bool nx_webgl_egl_get_active_uniforms_iv(nx_webgl_egl_t *backend,
                                          uint32_t program_handle,
                                          int count, const uint32_t *indices,
                                          uint32_t pname, int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_active_uniforms_iv ||
		!out || count <= 0)
		return false;
	typedef void (*pfn_t)(GLuint, GLsizei, const GLuint *, GLenum, GLint *);
	((pfn_t)backend->fn_get_active_uniforms_iv)((GLuint)program_handle,
	                                             (GLsizei)count,
	                                             (const GLuint *)indices,
	                                             (GLenum)pname, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program_handle; (void)count; (void)indices;
	(void)pname; (void)out;
	return false;
#endif
}

// Sampler objects ----------------------------------------------------------

uint32_t nx_webgl_egl_gen_sampler(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_gen_samplers)
		return 0;
	typedef void (*pfn_t)(GLsizei, GLuint *);
	GLuint h = 0;
	((pfn_t)backend->fn_gen_samplers)(1, &h);
	return (uint32_t)h;
#else
	(void)backend;
	return 0;
#endif
}

void nx_webgl_egl_delete_sampler(nx_webgl_egl_t *backend, uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_delete_samplers || !handle)
		return;
	typedef void (*pfn_t)(GLsizei, const GLuint *);
	GLuint h = (GLuint)handle;
	((pfn_t)backend->fn_delete_samplers)(1, &h);
#else
	(void)backend; (void)handle;
#endif
}

void nx_webgl_egl_bind_sampler(nx_webgl_egl_t *backend, uint32_t unit,
                                uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_bind_sampler)
		return;
	typedef void (*pfn_t)(GLuint, GLuint);
	((pfn_t)backend->fn_bind_sampler)((GLuint)unit, (GLuint)handle);
#else
	(void)backend; (void)unit; (void)handle;
#endif
}

void nx_webgl_egl_sampler_parameteri(nx_webgl_egl_t *backend, uint32_t handle,
                                      uint32_t pname, int param) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_sampler_parameteri)
		return;
	typedef void (*pfn_t)(GLuint, GLenum, GLint);
	((pfn_t)backend->fn_sampler_parameteri)((GLuint)handle, (GLenum)pname,
	                                         param);
#else
	(void)backend; (void)handle; (void)pname; (void)param;
#endif
}

void nx_webgl_egl_sampler_parameterf(nx_webgl_egl_t *backend, uint32_t handle,
                                      uint32_t pname, float param) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_sampler_parameterf)
		return;
	typedef void (*pfn_t)(GLuint, GLenum, GLfloat);
	((pfn_t)backend->fn_sampler_parameterf)((GLuint)handle, (GLenum)pname,
	                                         param);
#else
	(void)backend; (void)handle; (void)pname; (void)param;
#endif
}

bool nx_webgl_egl_get_sampler_parameter_iv(nx_webgl_egl_t *backend,
                                             uint32_t handle, uint32_t pname,
                                             int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_sampler_parameter_iv ||
		!out)
		return false;
	typedef void (*pfn_t)(GLuint, GLenum, GLint *);
	((pfn_t)backend->fn_get_sampler_parameter_iv)((GLuint)handle,
	                                                (GLenum)pname, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)handle; (void)pname; (void)out;
	return false;
#endif
}

// Sync objects -------------------------------------------------------------

void *nx_webgl_egl_fence_sync(nx_webgl_egl_t *backend, uint32_t condition,
                               uint32_t flags) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_fence_sync)
		return NULL;
	typedef void *(*pfn_t)(GLenum, GLbitfield);
	return ((pfn_t)backend->fn_fence_sync)((GLenum)condition,
	                                        (GLbitfield)flags);
#else
	(void)backend; (void)condition; (void)flags;
	return NULL;
#endif
}

void nx_webgl_egl_delete_sync(nx_webgl_egl_t *backend, void *sync) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_delete_sync || !sync)
		return;
	typedef void (*pfn_t)(void *);
	((pfn_t)backend->fn_delete_sync)(sync);
#else
	(void)backend; (void)sync;
#endif
}

uint32_t nx_webgl_egl_client_wait_sync(nx_webgl_egl_t *backend, void *sync,
                                         uint32_t flags, uint64_t timeout) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_client_wait_sync || !sync)
		return 0x911D; // WAIT_FAILED
	typedef GLenum (*pfn_t)(void *, GLbitfield, GLuint64);
	return (uint32_t)((pfn_t)backend->fn_client_wait_sync)(sync,
	                                                        (GLbitfield)flags,
	                                                        (GLuint64)timeout);
#else
	(void)backend; (void)sync; (void)flags; (void)timeout;
	return 0x911D;
#endif
}

void nx_webgl_egl_wait_sync(nx_webgl_egl_t *backend, void *sync, uint32_t flags,
                             uint64_t timeout) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_wait_sync || !sync)
		return;
	typedef void (*pfn_t)(void *, GLbitfield, GLuint64);
	((pfn_t)backend->fn_wait_sync)(sync, (GLbitfield)flags, (GLuint64)timeout);
#else
	(void)backend; (void)sync; (void)flags; (void)timeout;
#endif
}

bool nx_webgl_egl_get_sync_iv(nx_webgl_egl_t *backend, void *sync,
                                uint32_t pname, int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_sync_iv || !sync ||
		!out)
		return false;
	typedef void (*pfn_t)(void *, GLenum, GLsizei, GLsizei *, GLint *);
	GLsizei length = 0;
	((pfn_t)backend->fn_get_sync_iv)(sync, (GLenum)pname, 1, &length, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)sync; (void)pname; (void)out;
	return false;
#endif
}

// Query objects ------------------------------------------------------------

uint32_t nx_webgl_egl_gen_query(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_gen_queries)
		return 0;
	typedef void (*pfn_t)(GLsizei, GLuint *);
	GLuint h = 0;
	((pfn_t)backend->fn_gen_queries)(1, &h);
	return (uint32_t)h;
#else
	(void)backend;
	return 0;
#endif
}

void nx_webgl_egl_delete_query(nx_webgl_egl_t *backend, uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_delete_queries || !handle)
		return;
	typedef void (*pfn_t)(GLsizei, const GLuint *);
	GLuint h = (GLuint)handle;
	((pfn_t)backend->fn_delete_queries)(1, &h);
#else
	(void)backend; (void)handle;
#endif
}

void nx_webgl_egl_begin_query(nx_webgl_egl_t *backend, uint32_t target,
                               uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_begin_query)
		return;
	typedef void (*pfn_t)(GLenum, GLuint);
	((pfn_t)backend->fn_begin_query)((GLenum)target, (GLuint)handle);
#else
	(void)backend; (void)target; (void)handle;
#endif
}

void nx_webgl_egl_end_query(nx_webgl_egl_t *backend, uint32_t target) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_end_query)
		return;
	typedef void (*pfn_t)(GLenum);
	((pfn_t)backend->fn_end_query)((GLenum)target);
#else
	(void)backend; (void)target;
#endif
}

bool nx_webgl_egl_get_query_iv(nx_webgl_egl_t *backend, uint32_t target,
                                uint32_t pname, int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_query_iv || !out)
		return false;
	typedef void (*pfn_t)(GLenum, GLenum, GLint *);
	((pfn_t)backend->fn_get_query_iv)((GLenum)target, (GLenum)pname, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)pname; (void)out;
	return false;
#endif
}

bool nx_webgl_egl_get_query_object_uiv(nx_webgl_egl_t *backend, uint32_t handle,
                                         uint32_t pname, uint32_t *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_query_object_uiv ||
		!out)
		return false;
	typedef void (*pfn_t)(GLuint, GLenum, GLuint *);
	((pfn_t)backend->fn_get_query_object_uiv)((GLuint)handle, (GLenum)pname,
	                                           (GLuint *)out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)handle; (void)pname; (void)out;
	return false;
#endif
}

// Transform feedback -------------------------------------------------------

uint32_t nx_webgl_egl_gen_transform_feedback(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_gen_transform_feedbacks)
		return 0;
	typedef void (*pfn_t)(GLsizei, GLuint *);
	GLuint h = 0;
	((pfn_t)backend->fn_gen_transform_feedbacks)(1, &h);
	return (uint32_t)h;
#else
	(void)backend;
	return 0;
#endif
}

void nx_webgl_egl_delete_transform_feedback(nx_webgl_egl_t *backend,
                                              uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_delete_transform_feedbacks || !handle)
		return;
	typedef void (*pfn_t)(GLsizei, const GLuint *);
	GLuint h = (GLuint)handle;
	((pfn_t)backend->fn_delete_transform_feedbacks)(1, &h);
#else
	(void)backend; (void)handle;
#endif
}

void nx_webgl_egl_bind_transform_feedback(nx_webgl_egl_t *backend,
                                            uint32_t target, uint32_t handle) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_bind_transform_feedback)
		return;
	typedef void (*pfn_t)(GLenum, GLuint);
	((pfn_t)backend->fn_bind_transform_feedback)((GLenum)target,
	                                              (GLuint)handle);
#else
	(void)backend; (void)target; (void)handle;
#endif
}

void nx_webgl_egl_begin_transform_feedback(nx_webgl_egl_t *backend,
                                             uint32_t primitive_mode) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_begin_transform_feedback)
		return;
	typedef void (*pfn_t)(GLenum);
	((pfn_t)backend->fn_begin_transform_feedback)((GLenum)primitive_mode);
#else
	(void)backend; (void)primitive_mode;
#endif
}

void nx_webgl_egl_end_transform_feedback(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_end_transform_feedback)
		return;
	typedef void (*pfn_t)(void);
	((pfn_t)backend->fn_end_transform_feedback)();
#else
	(void)backend;
#endif
}

void nx_webgl_egl_pause_transform_feedback(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_pause_transform_feedback)
		return;
	typedef void (*pfn_t)(void);
	((pfn_t)backend->fn_pause_transform_feedback)();
#else
	(void)backend;
#endif
}

void nx_webgl_egl_resume_transform_feedback(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_resume_transform_feedback)
		return;
	typedef void (*pfn_t)(void);
	((pfn_t)backend->fn_resume_transform_feedback)();
#else
	(void)backend;
#endif
}

void nx_webgl_egl_transform_feedback_varyings(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                int count,
                                                const char *const *varyings,
                                                uint32_t buffer_mode) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_transform_feedback_varyings)
		return;
	typedef void (*pfn_t)(GLuint, GLsizei, const GLchar *const *, GLenum);
	((pfn_t)backend->fn_transform_feedback_varyings)((GLuint)program_handle,
	                                                  (GLsizei)count,
	                                                  (const GLchar *const *)varyings,
	                                                  (GLenum)buffer_mode);
#else
	(void)backend; (void)program_handle; (void)count; (void)varyings;
	(void)buffer_mode;
#endif
}

bool nx_webgl_egl_get_transform_feedback_varying(nx_webgl_egl_t *backend,
                                                   uint32_t program_handle,
                                                   uint32_t index,
                                                   char *name, size_t name_size,
                                                   int *size, uint32_t *type) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) ||
		!backend->fn_get_transform_feedback_varying || !name || !size || !type)
		return false;
	typedef void (*pfn_t)(GLuint, GLuint, GLsizei, GLsizei *, GLsizei *,
	                       GLenum *, GLchar *);
	GLsizei length = 0;
	GLsizei sz = 0;
	GLenum ty = 0;
	((pfn_t)backend->fn_get_transform_feedback_varying)((GLuint)program_handle,
	                                                     (GLuint)index,
	                                                     (GLsizei)name_size,
	                                                     &length, &sz, &ty, name);
	*size = (int)sz;
	*type = (uint32_t)ty;
	if (name_size > 0)
		name[name_size - 1] = '\0';
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program_handle; (void)index;
	(void)name; (void)name_size; (void)size; (void)type;
	return false;
#endif
}

// Misc ---------------------------------------------------------------------

int nx_webgl_egl_get_frag_data_location(nx_webgl_egl_t *backend,
                                          uint32_t program_handle,
                                          const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_frag_data_location)
		return -1;
	typedef GLint (*pfn_t)(GLuint, const GLchar *);
	return (int)((pfn_t)backend->fn_get_frag_data_location)(
		(GLuint)program_handle, (const GLchar *)name);
#else
	(void)backend; (void)program_handle; (void)name;
	return -1;
#endif
}

bool nx_webgl_egl_get_internal_format_iv(nx_webgl_egl_t *backend,
                                           uint32_t target,
                                           uint32_t internalformat,
                                           uint32_t pname, int buf_size,
                                           int *out) {
#if NXJS_HAS_EGL_GLES
	if (!webgl2_make_current(backend) || !backend->fn_get_internal_format_iv ||
		!out || buf_size <= 0)
		return false;
	typedef void (*pfn_t)(GLenum, GLenum, GLenum, GLsizei, GLint *);
	((pfn_t)backend->fn_get_internal_format_iv)((GLenum)target,
	                                             (GLenum)internalformat,
	                                             (GLenum)pname,
	                                             (GLsizei)buf_size, out);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)internalformat; (void)pname;
	(void)buf_size; (void)out;
	return false;
#endif
}

#if NXJS_HAS_EGL_GLES
static int webgl2_get_int(nx_webgl_egl_t *backend, GLenum pname) {
	if (!webgl2_make_current(backend))
		return 0;
	GLint v = 0;
	glGetIntegerv(pname, &v);
	if (glGetError() != GL_NO_ERROR)
		return 0;
	return (int)v;
}
#endif

int nx_webgl_egl_get_max_samples(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_SAMPLES);
#else
	(void)backend;
	return 0;
#endif
}
int nx_webgl_egl_get_max_3d_texture_size(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_3D_TEXTURE_SIZE);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_array_texture_layers(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_ARRAY_TEXTURE_LAYERS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_draw_buffers(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_DRAW_BUFFERS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_color_attachments(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_COLOR_ATTACHMENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_uniform_buffer_bindings(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_UNIFORM_BUFFER_BINDINGS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_uniform_buffer_offset_alignment(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
#else
	(void)backend; return 256;
#endif
}
int nx_webgl_egl_get_max_vertex_uniform_blocks(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_VERTEX_UNIFORM_BLOCKS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_fragment_uniform_blocks(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_FRAGMENT_UNIFORM_BLOCKS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_combined_uniform_blocks(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_COMBINED_UNIFORM_BLOCKS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_transform_feedback_separate_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_transform_feedback_interleaved_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_transform_feedback_separate_attribs(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_element_index(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_ELEMENT_INDEX);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_elements_vertices(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_ELEMENTS_VERTICES);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_elements_indices(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_ELEMENTS_INDICES);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_server_wait_timeout(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_SERVER_WAIT_TIMEOUT);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_program_texel_offset(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_PROGRAM_TEXEL_OFFSET);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_min_program_texel_offset(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MIN_PROGRAM_TEXEL_OFFSET);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_varying_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_VARYING_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_vertex_uniform_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_VERTEX_UNIFORM_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_fragment_uniform_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_vertex_output_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_VERTEX_OUTPUT_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_fragment_input_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_FRAGMENT_INPUT_COMPONENTS);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_texture_lod_bias(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, GL_MAX_TEXTURE_LOD_BIAS);
#else
	(void)backend; return 0;
#endif
}

// ============================================================================
// 2026-06-24 extension audit wave 1 — accessors + dispatch shims
// ============================================================================

// Generic int getter for limits queried through ES core glGetIntegerv that
// the bridge previously hardcoded. Mirrors webgl2_get_int but uses
// surfaceless makeCurrent so it works on both WebGL 1 and 2 contexts (ES
// core pnames are valid on both, GLES makes no distinction). Returns 0 if
// the backend isn't built.
static int egl_get_int_core(nx_webgl_egl_t *backend, GLenum pname) {
#if NXJS_HAS_EGL_GLES
	if (!backend) return 0;
	// Reuse webgl2_make_current; it's just an eglMakeCurrent on the EGL
	// backend's surfaceless config, valid for any ES version.
	if (!webgl2_make_current(backend)) return 0;
	GLint v = 0;
	glGetIntegerv(pname, &v);
	if (glGetError() != GL_NO_ERROR) return 0;
	return (int)v;
#else
	(void)backend; (void)pname; return 0;
#endif
}

int nx_webgl_egl_get_max_vertex_attribs_native(nx_webgl_egl_t *backend) {
	return egl_get_int_core(backend, 0x8869 /* GL_MAX_VERTEX_ATTRIBS */);
}
int nx_webgl_egl_get_max_texture_image_units_native(nx_webgl_egl_t *backend) {
	return egl_get_int_core(backend, 0x8872 /* GL_MAX_TEXTURE_IMAGE_UNITS */);
}
int nx_webgl_egl_get_max_combined_texture_image_units_native(nx_webgl_egl_t *backend) {
	return egl_get_int_core(backend, 0x8B4D /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS */);
}
int nx_webgl_egl_get_max_combined_vertex_uniform_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, 0x8A31 /* GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS */);
#else
	(void)backend; return 0;
#endif
}
int nx_webgl_egl_get_max_combined_fragment_uniform_components(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return webgl2_get_int(backend, 0x8A33 /* GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS */);
#else
	(void)backend; return 0;
#endif
}

float nx_webgl_egl_get_max_anisotropy(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->has_anisotropic) return 0.0f;
	if (!webgl2_make_current(backend)) return 0.0f;
	GLfloat v = 0.0f;
	glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &v);
	if (glGetError() != GL_NO_ERROR) return 0.0f;
	return (float)v;
#else
	(void)backend; return 0.0f;
#endif
}

void nx_webgl_egl_get_aliased_line_width_range_native(nx_webgl_egl_t *backend,
                                                       float out[2]) {
	out[0] = 1.0f; out[1] = 1.0f;
#if NXJS_HAS_EGL_GLES
	if (!backend || !webgl2_make_current(backend)) return;
	GLfloat vals[2] = {1.0f, 1.0f};
	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, vals);
	if (glGetError() == GL_NO_ERROR && vals[1] >= vals[0] && vals[1] >= 1.0f) {
		out[0] = (float)vals[0];
		out[1] = (float)vals[1];
	}
#endif
}

bool nx_webgl_egl_get_msaa_enabled(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->egl_msaa_enabled;
#else
	(void)backend; return false;
#endif
}

// Per-extension presence accessors (driven by the probe at step 8).
#define NX_HAS_GETTER(field) \
	bool nx_webgl_egl_##field(nx_webgl_egl_t *backend) { \
		return backend && backend->field; \
	}
NX_HAS_GETTER(has_anisotropic)
NX_HAS_GETTER(has_clip_control)
NX_HAS_GETTER(has_depth_clamp)
NX_HAS_GETTER(has_polygon_offset_clamp)
NX_HAS_GETTER(has_texture_compression_bptc)
NX_HAS_GETTER(has_texture_compression_rgtc)
NX_HAS_GETTER(has_texture_compression_s3tc)
NX_HAS_GETTER(has_texture_compression_s3tc_srgb)
NX_HAS_GETTER(has_texture_norm16)
NX_HAS_GETTER(has_clip_cull_distance)
NX_HAS_GETTER(has_float_blend)
NX_HAS_GETTER(has_render_snorm)
NX_HAS_GETTER(has_sample_variables)
NX_HAS_GETTER(has_shader_multisample_interpolation)
NX_HAS_GETTER(has_parallel_shader_compile)
NX_HAS_GETTER(has_multi_draw)
NX_HAS_GETTER(has_draw_buffers_indexed)
NX_HAS_GETTER(has_blend_func_extended)
NX_HAS_GETTER(has_texture_compression_etc1)
NX_HAS_GETTER(has_texture_compression_etc)
NX_HAS_GETTER(has_texture_compression_astc)
NX_HAS_GETTER(has_disjoint_timer_query)
NX_HAS_GETTER(has_blend_minmax)
NX_HAS_GETTER(has_frag_depth)
NX_HAS_GETTER(has_element_index_uint)
NX_HAS_GETTER(has_fbo_render_mipmap)
NX_HAS_GETTER(has_srgb)
NX_HAS_GETTER(has_ext_color_buffer_float)
#undef NX_HAS_GETTER

// VAO + drawBuffers entry-points are loaded unconditionally in step 8.
// The "has" check is just whether resolve succeeded — extensions for v1
// gate on these so the page only sees them when callable.
bool nx_webgl_egl_has_vertex_array_object(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->fn_gen_vertex_arrays &&
	       backend->fn_bind_vertex_array && backend->fn_delete_vertex_arrays;
#else
	(void)backend; return false;
#endif
}
bool nx_webgl_egl_has_draw_buffers(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	return backend && backend->fn_draw_buffers;
#else
	(void)backend; return false;
#endif
}

// EXT_disjoint_timer_query_webgl2 — record a GPU timestamp into the
// query's storage. Caller has already created the query via createQuery
// + has the handle. Target must be TIMESTAMP_EXT (0x8E28).
bool nx_webgl_egl_query_counter_ext(nx_webgl_egl_t *backend, uint32_t handle,
                                      uint32_t target) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_query_counter_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLenum);
	((pfn_t)backend->fn_query_counter_ext)((GLuint)handle, (GLenum)target);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)handle; (void)target; return false;
#endif
}

// GPU_DISJOINT_EXT — probe whether a disjoint operation (context loss,
// thermal throttle, eviction) happened since the last query. Reading the
// pname clears the flag. On Mesa Nouveau this typically returns false
// (no disjoint events on a stable surfaceless context).
bool nx_webgl_egl_get_gpu_disjoint(nx_webgl_egl_t *backend) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->has_disjoint_timer_query) return false;
	if (!webgl2_make_current(backend)) return false;
	GLint v = 0;
	glGetIntegerv(0x8FBB /* GPU_DISJOINT_EXT */, &v);
	// glGetError may flag INVALID_ENUM on drivers that don't accept the
	// pname despite advertising the extension — swallow it.
	(void)glGetError();
	return v != 0;
#else
	(void)backend; return false;
#endif
}

// EXT_clip_control dispatch.
bool nx_webgl_egl_clip_control(nx_webgl_egl_t *backend, uint32_t origin,
                                uint32_t depth) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_clip_control) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, GLenum);
	((pfn_t)backend->fn_clip_control)((GLenum)origin, (GLenum)depth);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)origin; (void)depth; return false;
#endif
}

// EXT_polygon_offset_clamp dispatch.
bool nx_webgl_egl_polygon_offset_clamp(nx_webgl_egl_t *backend, float factor,
                                        float units, float clamp) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_polygon_offset_clamp_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLfloat, GLfloat, GLfloat);
	((pfn_t)backend->fn_polygon_offset_clamp_ext)((GLfloat)factor, (GLfloat)units,
	                                                (GLfloat)clamp);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)factor; (void)units; (void)clamp; return false;
#endif
}

// KHR_parallel_shader_compile dispatch.
bool nx_webgl_egl_max_shader_compiler_threads_khr(nx_webgl_egl_t *backend,
                                                    uint32_t count) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_max_shader_compiler_threads_khr) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint);
	((pfn_t)backend->fn_max_shader_compiler_threads_khr)((GLuint)count);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)count; return false;
#endif
}

// WEBGL_multi_draw dispatch (EXT_multi_draw_arrays).
bool nx_webgl_egl_multi_draw_arrays(nx_webgl_egl_t *backend, uint32_t mode,
                                     const int *firsts, const int *counts,
                                     int drawcount) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_multi_draw_arrays_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, const GLint *, const GLsizei *, GLsizei);
	((pfn_t)backend->fn_multi_draw_arrays_ext)((GLenum)mode, (const GLint *)firsts,
	                                            (const GLsizei *)counts,
	                                            (GLsizei)drawcount);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)mode; (void)firsts; (void)counts; (void)drawcount;
	return false;
#endif
}

bool nx_webgl_egl_multi_draw_elements(nx_webgl_egl_t *backend, uint32_t mode,
                                       const int *counts, uint32_t type,
                                       const int *offsets_bytes, int drawcount) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_multi_draw_elements_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	// EXT_multi_draw_arrays uses const void *const *indices; we synthesize a
	// pointer array from byte-offsets so JS callers can pass element-buffer
	// offsets the WebGL way (drawElements semantics).
	const void *ptrs_stack[32];
	const void **ptrs = ptrs_stack;
	const void **ptrs_heap = NULL;
	if (drawcount > 32) {
		ptrs_heap = (const void **)malloc(sizeof(void *) * (size_t)drawcount);
		if (!ptrs_heap) return false;
		ptrs = ptrs_heap;
	}
	for (int i = 0; i < drawcount; i++) {
		ptrs[i] = (const void *)(uintptr_t)offsets_bytes[i];
	}
	typedef void (*pfn_t)(GLenum, const GLsizei *, GLenum, const void *const *,
	                       GLsizei);
	((pfn_t)backend->fn_multi_draw_elements_ext)((GLenum)mode,
	                                              (const GLsizei *)counts,
	                                              (GLenum)type, ptrs,
	                                              (GLsizei)drawcount);
	bool ok = glGetError() == GL_NO_ERROR;
	if (ptrs_heap) free(ptrs_heap);
	return ok;
#else
	(void)backend; (void)mode; (void)counts; (void)type;
	(void)offsets_bytes; (void)drawcount;
	return false;
#endif
}

// OES_draw_buffers_indexed dispatch.
bool nx_webgl_egl_enablei(nx_webgl_egl_t *backend, uint32_t target,
                           uint32_t index) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_enablei_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, GLuint);
	((pfn_t)backend->fn_enablei_ext)((GLenum)target, (GLuint)index);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)index; return false;
#endif
}
bool nx_webgl_egl_disablei(nx_webgl_egl_t *backend, uint32_t target,
                            uint32_t index) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_disablei_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, GLuint);
	((pfn_t)backend->fn_disablei_ext)((GLenum)target, (GLuint)index);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)index; return false;
#endif
}
bool nx_webgl_egl_blend_equationi(nx_webgl_egl_t *backend, uint32_t buf,
                                    uint32_t mode) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_blend_equationi_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLenum);
	((pfn_t)backend->fn_blend_equationi_ext)((GLuint)buf, (GLenum)mode);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)buf; (void)mode; return false;
#endif
}
bool nx_webgl_egl_blend_equation_separatei(nx_webgl_egl_t *backend,
                                             uint32_t buf, uint32_t modeRGB,
                                             uint32_t modeAlpha) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_blend_equation_separatei_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLenum, GLenum);
	((pfn_t)backend->fn_blend_equation_separatei_ext)((GLuint)buf, (GLenum)modeRGB,
	                                                    (GLenum)modeAlpha);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)buf; (void)modeRGB; (void)modeAlpha; return false;
#endif
}
bool nx_webgl_egl_blend_funci(nx_webgl_egl_t *backend, uint32_t buf,
                                uint32_t src, uint32_t dst) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_blend_funci_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLenum, GLenum);
	((pfn_t)backend->fn_blend_funci_ext)((GLuint)buf, (GLenum)src, (GLenum)dst);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)buf; (void)src; (void)dst; return false;
#endif
}
bool nx_webgl_egl_blend_func_separatei(nx_webgl_egl_t *backend, uint32_t buf,
                                         uint32_t srcRGB, uint32_t dstRGB,
                                         uint32_t srcAlpha, uint32_t dstAlpha) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_blend_func_separatei_ext) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLenum, GLenum, GLenum, GLenum);
	((pfn_t)backend->fn_blend_func_separatei_ext)(
		(GLuint)buf, (GLenum)srcRGB, (GLenum)dstRGB,
		(GLenum)srcAlpha, (GLenum)dstAlpha);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)buf; (void)srcRGB; (void)dstRGB;
	(void)srcAlpha; (void)dstAlpha; return false;
#endif
}

// WEBGL_blend_func_extended dispatch.
bool nx_webgl_egl_bind_frag_data_location(nx_webgl_egl_t *backend,
                                            uint32_t program, uint32_t color,
                                            const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_bind_frag_data_location_ext || !name) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLuint, const GLchar *);
	((pfn_t)backend->fn_bind_frag_data_location_ext)((GLuint)program, (GLuint)color,
	                                                   (const GLchar *)name);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program; (void)color; (void)name; return false;
#endif
}
bool nx_webgl_egl_bind_frag_data_location_indexed(nx_webgl_egl_t *backend,
                                                    uint32_t program,
                                                    uint32_t color,
                                                    uint32_t index,
                                                    const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_bind_frag_data_location_indexed_ext || !name)
		return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLuint, GLuint, GLuint, const GLchar *);
	((pfn_t)backend->fn_bind_frag_data_location_indexed_ext)(
		(GLuint)program, (GLuint)color, (GLuint)index, (const GLchar *)name);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)program; (void)color; (void)index; (void)name;
	return false;
#endif
}
int nx_webgl_egl_get_frag_data_index(nx_webgl_egl_t *backend, uint32_t program,
                                       const char *name) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_get_frag_data_index_ext || !name) return -1;
	if (!webgl2_make_current(backend)) return -1;
	typedef GLint (*pfn_t)(GLuint, const GLchar *);
	GLint idx = ((pfn_t)backend->fn_get_frag_data_index_ext)((GLuint)program,
	                                                          (const GLchar *)name);
	if (glGetError() != GL_NO_ERROR) return -1;
	return (int)idx;
#else
	(void)backend; (void)program; (void)name; return -1;
#endif
}

// Compressed texture 2D dispatch — wires up s3tc / s3tc_srgb / bptc / rgtc
// uploads to the native driver. The bridge previously only had 3D variants
// (compressedTexImage3D / SubImage3D) — the 2D path was a logging stub.
bool nx_webgl_egl_compressed_tex_image_2d(nx_webgl_egl_t *backend,
                                            uint32_t target, int level,
                                            uint32_t internalformat,
                                            int width, int height, int border,
                                            size_t image_size,
                                            const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_compressed_tex_image_2d) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint,
	                       GLsizei, const void *);
	((pfn_t)backend->fn_compressed_tex_image_2d)((GLenum)target, level,
	                                               (GLenum)internalformat,
	                                               width, height, border,
	                                               (GLsizei)image_size, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)internalformat;
	(void)width; (void)height; (void)border; (void)image_size; (void)data;
	return false;
#endif
}
bool nx_webgl_egl_compressed_tex_sub_image_2d(nx_webgl_egl_t *backend,
                                                uint32_t target, int level,
                                                int xoff, int yoff,
                                                int width, int height,
                                                uint32_t format,
                                                size_t image_size,
                                                const void *data) {
#if NXJS_HAS_EGL_GLES
	if (!backend || !backend->fn_compressed_tex_sub_image_2d) return false;
	if (!webgl2_make_current(backend)) return false;
	typedef void (*pfn_t)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum,
	                       GLsizei, const void *);
	((pfn_t)backend->fn_compressed_tex_sub_image_2d)((GLenum)target, level,
	                                                    xoff, yoff,
	                                                    width, height,
	                                                    (GLenum)format,
	                                                    (GLsizei)image_size, data);
	return glGetError() == GL_NO_ERROR;
#else
	(void)backend; (void)target; (void)level; (void)xoff; (void)yoff;
	(void)width; (void)height; (void)format; (void)image_size; (void)data;
	return false;
#endif
}
