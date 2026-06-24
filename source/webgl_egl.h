#pragma once
#include "canvas.h"
#include "types.h"

typedef struct nx_webgl_egl_s nx_webgl_egl_t;

nx_webgl_egl_t *nx_webgl_egl_create(JSContext *ctx, nx_canvas_t *canvas);
void nx_webgl_egl_destroy(JSRuntime *rt, nx_webgl_egl_t *backend);
bool nx_webgl_egl_is_available(nx_webgl_egl_t *backend);
void nx_webgl_egl_set_clear_color(nx_webgl_egl_t *backend, double *color);
void nx_webgl_egl_set_bridge_enabled(nx_webgl_egl_t *backend, bool enabled);
bool nx_webgl_egl_is_bridge_enabled(nx_webgl_egl_t *backend);
void nx_webgl_egl_set_auto_flush(nx_webgl_egl_t *backend, bool enabled);
void nx_webgl_egl_set_dispatch_debug(nx_webgl_egl_t *backend, const char *label);
void nx_webgl_egl_append_dispatch_debug(nx_webgl_egl_t *backend, const char *tag);
void nx_webgl_egl_reset_dispatch_debug(nx_webgl_egl_t *backend);
bool nx_webgl_egl_get_auto_flush(nx_webgl_egl_t *backend);
/* Tessellation-fix toggle (off by default). When enabled, the bridge
 * subdivides large screen-space triangles into smaller sub-triangles
 * before drawing. The intent was to work around the Tegra X1 TBR
 * per-tile UV-interpolator coherency bug ([[threejs-cube-white-face]]).
 *
 * CURRENT STATE: the implementation works mechanically but does NOT
 * actually fix the bug — midpoint-in-NDC subdivision produces a
 * uniform sub-triangle pattern that still triggers the rasterizer
 * artifact. JS-side manual tessellation
 * (`THREE.BoxGeometry(w,h,d,8,8,8)`) is what currently fixes the bug
 * in brewser demos. See the big STATE comment block in
 * `webgl_egl.c` (above `tessellate_one_triangle`) for the
 * investigation and the clip-space-correct approach that would
 * actually work. The scaffolding is intentionally kept dormant for a
 * future revisit. */
void nx_webgl_egl_set_tessellation_fix(nx_webgl_egl_t *backend, bool enabled);
bool nx_webgl_egl_get_tessellation_fix(nx_webgl_egl_t *backend);
void nx_webgl_egl_set_bridge_resolution(nx_webgl_egl_t *backend,
										int width,
										int height);
void nx_webgl_egl_delete_cached_texture(nx_webgl_egl_t *backend,
										uint32_t texture_id);
bool nx_webgl_egl_clear_bridge(nx_webgl_egl_t *backend, nx_canvas_t *canvas);
bool nx_webgl_egl_read_bridge_pixels(nx_webgl_egl_t *backend,
									  nx_canvas_t *canvas,
									  int x, int y, int width, int height,
									  uint32_t format, uint32_t type,
									  uint8_t *dst);
bool nx_webgl_egl_read_bridge_to_canvas_data(nx_webgl_egl_t *backend,
                                              int src_x, int src_y,
                                              int src_w, int src_h,
                                              nx_canvas_t *dst_canvas,
                                              int dst_x, int dst_y);
bool nx_webgl_egl_clear_bridge_with_state(nx_webgl_egl_t *backend,
  nx_canvas_t *canvas,
  uint32_t mask,
  float depth_value,
  int32_t stencil_value,
  bool scissor_enabled,
  const int *scissor_box,
  bool depth_enabled);
bool nx_webgl_egl_has_pending_readback(nx_webgl_egl_t *backend);
bool nx_webgl_egl_flush_bridge_present(nx_webgl_egl_t *backend,
									   nx_canvas_t *canvas);
uint32_t nx_webgl_egl_get_last_draw_gl_error(nx_webgl_egl_t *backend);

// SpotLight state passed alongside the bridge dispatch. When `enabled` is
// false (or the pointer is NULL), the bridge skips the spotlight contribution
// entirely. All vector fields are vec3 (3 floats) in the same coordinate
// space as the corresponding `point_light_*` args (view-space for position,
// already-unit world→view direction). `color` has Three.js's
// intensity*decay_scale already baked in (same convention as the existing
// directional/point light colors). `distance == 0` means "no cutoff distance"
// matching Three.js's SpotLight.distance semantics. `cone_cos` and
// `penumbra_cos` are pre-computed by Three.js's WebGLLights from
// `Math.cos(angle)` and `Math.cos(angle * (1 - penumbra))` respectively.
typedef struct {
  bool enabled;
  const float *position;       // vec3, view-space
  const float *direction;      // vec3, view-space, unit length, points from light toward target
  const float *color;          // vec3, linear RGB with intensity baked in
  float distance;              // cutoff distance; 0 = no cutoff
  float cone_cos;              // cos(SpotLight.angle)
  float penumbra_cos;          // cos(SpotLight.angle * (1 - SpotLight.penumbra))
  float decay;                 // physical decay exponent (typically 2 for inverse-square)
} nx_webgl_egl_spot_light_t;

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
  const nx_webgl_egl_spot_light_t *spot_light);
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
  float fog_far);
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
  // When non-zero, dispatch binds this native GLES texture handle directly
  // and skips the per-draw cache upload from `texture_rgba`. Set by webgl.c
  // for FBO-attached textures (no CPU-side `data`) — see [[bridge-fbo-support]].
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
  const nx_webgl_egl_spot_light_t *spot_light);
bool nx_webgl_egl_clear_prototype(nx_webgl_egl_t *backend,
								  nx_canvas_t *canvas);
bool nx_webgl_egl_probe_step(nx_webgl_egl_t *backend, nx_canvas_t *canvas);
JSValue nx_webgl_egl_triangle_readback(JSContext *ctx,
									   nx_webgl_egl_t *backend,
									   nx_canvas_t *canvas);
JSValue nx_webgl_egl_bridge_framebuffer(JSContext *ctx,
										nx_webgl_egl_t *backend,
										nx_canvas_t *canvas);
JSValue nx_webgl_egl_bridge_benchmark(JSContext *ctx,
									  nx_webgl_egl_t *backend,
									  nx_canvas_t *canvas,
									  int frame_count,
									  int requested_width,
									  int requested_height);
JSValue nx_webgl_egl_get_backend_info(JSContext *ctx, nx_webgl_egl_t *backend);
bool nx_webgl_egl_compile_shader(nx_webgl_egl_t *backend,
								 nx_canvas_t *canvas,
								 uint32_t shader_type,
								 const char *source,
								 uint32_t *shader_handle,
								 bool *compile_status,
								 char *info_log,
								 size_t info_log_size);
void nx_webgl_egl_delete_shader(nx_webgl_egl_t *backend,
								uint32_t shader_handle);
// Pre-link attribute bindings collected from user `bindAttribLocation`
// calls. Applied before glLinkProgram so the linker honors them. NULL or
// `binding_count == 0` is the legacy path.
typedef struct {
	int location;
	const char *name;
} nx_webgl_attrib_binding_t;

bool nx_webgl_egl_link_program(nx_webgl_egl_t *backend,
							   nx_canvas_t *canvas,
							   uint32_t vertex_shader_handle,
							   uint32_t fragment_shader_handle,
							   const nx_webgl_attrib_binding_t *bindings,
							   int binding_count,
							   uint32_t *program_handle,
							   bool *link_status,
							   char *info_log,
							   size_t info_log_size);
void nx_webgl_egl_delete_program(nx_webgl_egl_t *backend,
								 uint32_t program_handle);
int nx_webgl_egl_get_attrib_location(nx_webgl_egl_t *backend,
									uint32_t program_handle,
									const char *name);
int nx_webgl_egl_get_uniform_location(nx_webgl_egl_t *backend,
									 uint32_t program_handle,
									 const char *name);
bool nx_webgl_egl_get_active_attrib(nx_webgl_egl_t *backend,
									uint32_t program_handle,
									uint32_t index,
									char *name,
									size_t name_size,
									int *size,
									uint32_t *type);
bool nx_webgl_egl_get_active_uniform(nx_webgl_egl_t *backend,
									 uint32_t program_handle,
									 uint32_t index,
									 char *name,
									 size_t name_size,
									 int *size,
									 uint32_t *type);
bool nx_webgl_egl_get_program_iv(nx_webgl_egl_t *backend,
								 uint32_t program_handle,
								 uint32_t pname,
								 int *out_value);
void nx_webgl_egl_uniform1f(nx_webgl_egl_t *backend, int location, float x);
void nx_webgl_egl_uniform2f(nx_webgl_egl_t *backend, int location, float x, float y);
void nx_webgl_egl_uniform3f(nx_webgl_egl_t *backend, int location, float x, float y, float z);
void nx_webgl_egl_uniform4f(nx_webgl_egl_t *backend, int location, float x, float y, float z, float w);
void nx_webgl_egl_uniform1i(nx_webgl_egl_t *backend, int location, int x);
void nx_webgl_egl_uniform_matrix4fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value);
void nx_webgl_egl_uniform_matrix3fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value);
void nx_webgl_egl_uniform_matrix2fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value);
void nx_webgl_egl_uniform1fv(nx_webgl_egl_t *backend, int location, int count, const float *value);
void nx_webgl_egl_uniform2fv(nx_webgl_egl_t *backend, int location, int count, const float *value);
void nx_webgl_egl_uniform3fv(nx_webgl_egl_t *backend, int location, int count, const float *value);
void nx_webgl_egl_uniform4fv(nx_webgl_egl_t *backend, int location, int count, const float *value);
void nx_webgl_egl_uniform2i(nx_webgl_egl_t *backend, int location, int x, int y);
void nx_webgl_egl_uniform3i(nx_webgl_egl_t *backend, int location, int x, int y, int z);
void nx_webgl_egl_uniform4i(nx_webgl_egl_t *backend, int location, int x, int y, int z, int w);
void nx_webgl_egl_uniform1iv(nx_webgl_egl_t *backend, int location, int count, const int *value);
void nx_webgl_egl_uniform2iv(nx_webgl_egl_t *backend, int location, int count, const int *value);
void nx_webgl_egl_uniform3iv(nx_webgl_egl_t *backend, int location, int count, const int *value);
void nx_webgl_egl_uniform4iv(nx_webgl_egl_t *backend, int location, int count, const int *value);

// Native GL buffer plumbing. Used by the raw-shader passthrough draw path
// (see `program->raw_passthrough`), where user-uploaded vertex/index buffers
// must actually reach the GPU rather than only being cached CPU-side for
// the bridge's own per-draw re-upload. Bridge-mode draws continue to use
// the CPU-side `nx_webgl_buffer_t.data` and their own dedicated VBOs.
uint32_t nx_webgl_egl_create_native_buffer(nx_webgl_egl_t *backend,
										   nx_canvas_t *canvas);
void nx_webgl_egl_delete_native_buffer(nx_webgl_egl_t *backend,
									   uint32_t handle);
void nx_webgl_egl_native_buffer_data(nx_webgl_egl_t *backend,
									 uint32_t handle, uint32_t target,
									 size_t size, const void *data,
									 uint32_t usage);
void nx_webgl_egl_native_buffer_sub_data(nx_webgl_egl_t *backend,
										 uint32_t handle, uint32_t target,
										 size_t offset, size_t size,
										 const void *data);

// Bind a linked native program. Called from `gl.useProgram` so subsequent
// uniform uploads land on the right program. Bridge dispatch internally
// re-binds its own program; pass `handle == 0` to glUseProgram(0).
void nx_webgl_egl_use_native_program(nx_webgl_egl_t *backend, uint32_t handle);

// Raw-shader passthrough draw. Routes through the user's linked GLES
// program with the user's vertex-attribute and buffer state synced to
// native GL. Used only when `program->raw_passthrough` is set. Returns
// true on dispatch (whether or not the draw produced output); false on a
// setup error (eglMakeCurrent failure, missing FBO).
typedef struct {
	bool enabled;
	uint32_t buffer_handle;  // 0 if no buffer bound at vertexAttribPointer time
	int size;                // 1..4
	uint32_t type;           // GL_FLOAT / GL_UNSIGNED_BYTE / etc.
	bool normalized;
	int stride;
	int offset;
	uint32_t divisor;        // 0 = per-vertex; 1+ = per-instance (GLES3 / EXT_instanced_arrays)
} nx_webgl_egl_passthrough_attrib_t;

// `instance_count <= 0` means non-instanced (calls glDrawArrays / glDrawElements).
// `instance_count > 0` requires the backend's instancing entry points to be
// loaded (see ext_instanced_arrays_present probe); dispatches via
// glDrawArraysInstanced* / glDrawElementsInstanced*. Returns false if
// instancing was requested but not supported.
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
	/* 2026-06-08 ROUND 22: stencil + color-mask forwarding. Previously
	   `gl.stencilFunc / stencilOp / stencilMask / colorMask` were stored
	   on the JS context but never reached native GL, so cc.Mask's stencil
	   clip never worked (overflow rendered outside the mask shape). */
	bool stencil_enabled,
	uint32_t stencil_func, int32_t stencil_ref, uint32_t stencil_value_mask,
	uint32_t stencil_fail, uint32_t stencil_zfail, uint32_t stencil_zpass,
	uint32_t stencil_mask,
	const bool *color_mask /* 4 bools: R,G,B,A */);

bool nx_webgl_egl_has_instancing(nx_webgl_egl_t *backend);

/* Native `glGetString(GL_VENDOR)` / `glGetString(GL_RENDERER)` strings
 * captured at backend init. NULL if the backend isn't available yet or
 * the GLES driver returned NULL. Surfaced via `WEBGL_debug_renderer_info`'s
 * `UNMASKED_VENDOR_WEBGL` / `UNMASKED_RENDERER_WEBGL` pnames so diagnostic
 * pages (jQuery's WebGL Report, Three.js's `WebGLDebugInfo`) can show the
 * actual driver instead of the masked `"nx.js"` brand. */
const char *nx_webgl_egl_get_vendor(nx_webgl_egl_t *backend);
const char *nx_webgl_egl_get_renderer(nx_webgl_egl_t *backend);

// FBO / RBO / persistent-texture native entry points. Used by the JS-facing
// glue in webgl.c to back `gl.createFramebuffer` / `framebufferTexture2D`
// / etc. Bridge dispatch consults `current_user_framebuffer` (set via
// set_user_framebuffer) on each draw — non-zero retargets the dispatch
// into the user's FBO with standard GL bottom-up viewport convention and
// skips the present-time readback. See bridge_acquire_target / bridge_bind_target
// in webgl_egl.c for the retargeting logic. Added 2026-05-20 for milestone
// #19 (webgl_postprocessing).
uint32_t nx_webgl_egl_create_native_framebuffer(nx_webgl_egl_t *backend,
                                                 nx_canvas_t *canvas);
void nx_webgl_egl_delete_native_framebuffer(nx_webgl_egl_t *backend,
                                             uint32_t handle);
uint32_t nx_webgl_egl_create_native_renderbuffer(nx_webgl_egl_t *backend,
                                                  nx_canvas_t *canvas);
void nx_webgl_egl_delete_native_renderbuffer(nx_webgl_egl_t *backend,
                                              uint32_t handle);
bool nx_webgl_egl_renderbuffer_storage(nx_webgl_egl_t *backend,
                                        uint32_t handle,
                                        uint32_t internalformat,
                                        int width, int height);
uint32_t nx_webgl_egl_create_persistent_texture(nx_webgl_egl_t *backend,
                                                 nx_canvas_t *canvas);
void nx_webgl_egl_delete_persistent_texture(nx_webgl_egl_t *backend,
                                             uint32_t handle);
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
                                                uint32_t wrap_t);
// Upload one face of a TEXTURE_CUBE_MAP persistent texture. `face_target`
// must be one of GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z. Filter/wrap
// parameters apply to GL_TEXTURE_CUBE_MAP and are idempotent across faces.
// Used by the cube branch of `texImage2D` ([[swb-threejs-webgl-materials-cubemap]],
// milestone #25). `data` may be NULL for cube render-target init.
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
                                                    uint32_t wrap_t);
uint32_t nx_webgl_egl_check_framebuffer_status(nx_webgl_egl_t *backend,
                                                 uint32_t handle);
bool nx_webgl_egl_framebuffer_texture_2d(nx_webgl_egl_t *backend,
                                          uint32_t framebuffer_handle,
                                          uint32_t attachment,
                                          uint32_t textarget,
                                          uint32_t texture_handle);
bool nx_webgl_egl_framebuffer_renderbuffer(nx_webgl_egl_t *backend,
                                            uint32_t framebuffer_handle,
                                            uint32_t attachment,
                                            uint32_t renderbuffer_handle);
void nx_webgl_egl_set_user_framebuffer(nx_webgl_egl_t *backend,
                                        uint32_t handle, int width,
                                        int height);

// Forward user-side gl.activeTexture / gl.bindTexture to native GLES.
// Bridge-mode dispatch already binds its own texture state per draw, so
// these forwards are a no-op for bridge-only demos; they matter for the
// raw-shader passthrough path ([[bridge-raw-shader-passthrough]]) where
// the user's program samples user-bound textures directly (e.g. Three.js's
// EffectComposer ShaderPass sampling the prior pass's FBO color texture).
void nx_webgl_egl_forward_active_texture(nx_webgl_egl_t *backend,
                                          uint32_t unit);
void nx_webgl_egl_forward_bind_texture(nx_webgl_egl_t *backend,
                                        uint32_t target, uint32_t handle);

// Apply a glTexParameteri to a persistent texture handle. Used when the
// JS-side `gl.texParameteri` call lands on a texture that's already
// been promoted to a native GLES handle — the change needs to reach
// native so subsequent draws sample with the new filter/wrap. Milestone
// #24 ([[swb-threejs-webgl-materials-texture-filters]]).
void nx_webgl_egl_texture_set_parameteri(nx_webgl_egl_t *backend,
                                          uint32_t target,
                                          uint32_t handle, uint32_t pname,
                                          uint32_t param);
// Generate mipmaps on a persistent texture handle. Caller must have
// promoted the texture and uploaded the level-0 texels first. Milestone
// #24.
void nx_webgl_egl_generate_mipmap(nx_webgl_egl_t *backend,
                                    uint32_t handle, uint32_t target);

// Read pixels from a user-bound FBO. Standard GL bottom-up convention
// (no y-flip — the user FBO doesn't share the bridge FBO's canvas-y
// top-down quirk). Restores binding to whatever was current. Used when
// `gl.readPixels` is called with a non-null `framebuffer_binding`.
bool nx_webgl_egl_read_user_fbo_pixels(nx_webgl_egl_t *backend,
                                        uint32_t fbo_handle,
                                        int x, int y, int width, int height,
                                        uint32_t format, uint32_t type,
                                        uint8_t *dst);

// ============================================================================
// WebGL 2 (ES 3.0) native trampolines
// ============================================================================
// All of these resolve their underlying entry point lazily via
// `eglGetProcAddress` at backend init (alongside the existing instancing /
// VAO probes). `nx_webgl_egl_has_webgl2(backend)` reports whether the core
// set required by Three.js (instancing + VAOs + uint uniforms + drawBuffers +
// vertex-array integer pointer) loaded; finer-grained queries follow.

bool nx_webgl_egl_has_webgl2(nx_webgl_egl_t *backend);

// Ensure the EGL/GLES backend is fully initialized (probe loop run to
// completion). Caller-driven version of the lazy init that the existing
// bridge dispatch paths trigger internally — needed when a JS entry point
// like `gl.getParameter(MAX_SAMPLES)` calls a native GLES helper directly
// without going through the bridge first. Returns true if available
// (already initialized or just finished), false if the probe failed.
bool nx_webgl_egl_ensure_initialized(nx_webgl_egl_t *backend,
                                      nx_canvas_t *canvas);

// Forward a glTexSubImage2D to a persistent native texture handle. Used by
// `gl.texSubImage2D` after `gl.texStorage2D` allocated immutable storage
// (Three.js's WebGL 2 texture upload pattern). The persistent handle is
// already bound at the JS-side `bindTexture` so we don't re-bind here —
// just make-current and forward.
bool nx_webgl_egl_persistent_texture_sub_image_2d(nx_webgl_egl_t *backend,
                                                    uint32_t handle,
                                                    int level,
                                                    int xoffset, int yoffset,
                                                    int width, int height,
                                                    uint32_t format,
                                                    uint32_t type,
                                                    const void *pixels);

// VAOs — exposed as user-managed bindings (in addition to the existing
// passthrough_vao). `set_user_vao(handle)` is what `gl.bindVertexArray`
// records; the passthrough dispatch consults it and binds the user's VAO
// when non-zero, else the passthrough_vao. `0` means "bind the passthrough
// VAO" (i.e. the WebGL 1 path).
uint32_t nx_webgl_egl_gen_vertex_array(nx_webgl_egl_t *backend);
void nx_webgl_egl_delete_vertex_array(nx_webgl_egl_t *backend, uint32_t handle);
void nx_webgl_egl_set_user_vao(nx_webgl_egl_t *backend, uint32_t handle);
uint32_t nx_webgl_egl_get_user_vao(nx_webgl_egl_t *backend);

// drawBuffers (WebGL 2 native — replaces WEBGL_draw_buffers extension).
void nx_webgl_egl_draw_buffers(nx_webgl_egl_t *backend, int n,
                                const uint32_t *bufs);

// invalidateFramebuffer / invalidateSubFramebuffer.
void nx_webgl_egl_invalidate_framebuffer(nx_webgl_egl_t *backend,
                                          uint32_t target, int n,
                                          const uint32_t *attachments);
void nx_webgl_egl_invalidate_sub_framebuffer(nx_webgl_egl_t *backend,
                                              uint32_t target, int n,
                                              const uint32_t *attachments,
                                              int x, int y, int w, int h);

// blitFramebuffer / readBuffer / renderbufferStorageMultisample /
// framebufferTextureLayer.
void nx_webgl_egl_blit_framebuffer(nx_webgl_egl_t *backend,
                                    int srcX0, int srcY0, int srcX1, int srcY1,
                                    int dstX0, int dstY0, int dstX1, int dstY1,
                                    uint32_t mask, uint32_t filter);
void nx_webgl_egl_read_buffer(nx_webgl_egl_t *backend, uint32_t src);
bool nx_webgl_egl_renderbuffer_storage_multisample(nx_webgl_egl_t *backend,
                                                    uint32_t handle,
                                                    int samples,
                                                    uint32_t internalformat,
                                                    int width, int height);
bool nx_webgl_egl_framebuffer_texture_layer(nx_webgl_egl_t *backend,
                                             uint32_t framebuffer_handle,
                                             uint32_t attachment,
                                             uint32_t texture_handle,
                                             int level, int layer);

// 3D / array texture upload + immutable storage.
bool nx_webgl_egl_tex_image_3d(nx_webgl_egl_t *backend,
                                uint32_t target, int level,
                                uint32_t internalformat,
                                int width, int height, int depth,
                                int border, uint32_t format, uint32_t type,
                                const void *data);
bool nx_webgl_egl_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                    uint32_t target, int level,
                                    int xoff, int yoff, int zoff,
                                    int width, int height, int depth,
                                    uint32_t format, uint32_t type,
                                    const void *data);
bool nx_webgl_egl_copy_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                         uint32_t target, int level,
                                         int xoff, int yoff, int zoff,
                                         int x, int y, int w, int h);
bool nx_webgl_egl_copy_tex_image_2d(nx_webgl_egl_t *backend,
                                     uint32_t target, int level,
                                     uint32_t internalformat,
                                     int x, int y, int w, int h, int border);
bool nx_webgl_egl_copy_tex_sub_image_2d(nx_webgl_egl_t *backend,
                                         uint32_t target, int level,
                                         int xoff, int yoff,
                                         int x, int y, int w, int h);
bool nx_webgl_egl_compressed_tex_image_3d(nx_webgl_egl_t *backend,
                                           uint32_t target, int level,
                                           uint32_t internalformat,
                                           int width, int height, int depth,
                                           int border, size_t image_size,
                                           const void *data);
bool nx_webgl_egl_compressed_tex_sub_image_3d(nx_webgl_egl_t *backend,
                                               uint32_t target, int level,
                                               int xoff, int yoff, int zoff,
                                               int width, int height, int depth,
                                               uint32_t format, size_t image_size,
                                               const void *data);
bool nx_webgl_egl_tex_storage_2d(nx_webgl_egl_t *backend,
                                  uint32_t target, int levels,
                                  uint32_t internalformat,
                                  int width, int height);
bool nx_webgl_egl_tex_storage_3d(nx_webgl_egl_t *backend,
                                  uint32_t target, int levels,
                                  uint32_t internalformat,
                                  int width, int height, int depth);

// clearBuffer family.
void nx_webgl_egl_clear_buffer_iv(nx_webgl_egl_t *backend,
                                   uint32_t buffer, int drawbuffer,
                                   const int *value);
void nx_webgl_egl_clear_buffer_uiv(nx_webgl_egl_t *backend,
                                    uint32_t buffer, int drawbuffer,
                                    const uint32_t *value);
void nx_webgl_egl_clear_buffer_fv(nx_webgl_egl_t *backend,
                                   uint32_t buffer, int drawbuffer,
                                   const float *value);
void nx_webgl_egl_clear_buffer_fi(nx_webgl_egl_t *backend,
                                   uint32_t buffer, int drawbuffer,
                                   float depth, int stencil);

// Integer vertex attributes.
void nx_webgl_egl_vertex_attrib_i_pointer(nx_webgl_egl_t *backend,
                                           uint32_t index, int size,
                                           uint32_t type, int stride,
                                           int offset);
void nx_webgl_egl_vertex_attrib_i4i(nx_webgl_egl_t *backend, uint32_t index,
                                     int x, int y, int z, int w);
void nx_webgl_egl_vertex_attrib_i4ui(nx_webgl_egl_t *backend, uint32_t index,
                                      uint32_t x, uint32_t y, uint32_t z,
                                      uint32_t w);

// Unsigned-integer + non-square matrix uniforms (ES 3 only — applied to the
// currently bound GLES program, mirroring the existing uniform1f/etc. shape).
void nx_webgl_egl_uniform1ui(nx_webgl_egl_t *backend, int location, uint32_t x);
void nx_webgl_egl_uniform2ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y);
void nx_webgl_egl_uniform3ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y, uint32_t z);
void nx_webgl_egl_uniform4ui(nx_webgl_egl_t *backend, int location,
                              uint32_t x, uint32_t y, uint32_t z, uint32_t w);
void nx_webgl_egl_uniform1uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value);
void nx_webgl_egl_uniform2uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value);
void nx_webgl_egl_uniform3uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value);
void nx_webgl_egl_uniform4uiv(nx_webgl_egl_t *backend, int location,
                               int count, const uint32_t *value);
void nx_webgl_egl_uniform_matrix2x3fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);
void nx_webgl_egl_uniform_matrix3x2fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);
void nx_webgl_egl_uniform_matrix2x4fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);
void nx_webgl_egl_uniform_matrix4x2fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);
void nx_webgl_egl_uniform_matrix3x4fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);
void nx_webgl_egl_uniform_matrix4x3fv(nx_webgl_egl_t *backend, int location,
                                       int count, bool transpose,
                                       const float *value);

// Buffer copy / readback.
void nx_webgl_egl_copy_buffer_sub_data(nx_webgl_egl_t *backend,
                                        uint32_t read_target,
                                        uint32_t write_target,
                                        size_t read_offset, size_t write_offset,
                                        size_t size);
void nx_webgl_egl_get_buffer_sub_data(nx_webgl_egl_t *backend,
                                       uint32_t target, size_t offset,
                                       size_t size, void *dst);

// UBOs.
void nx_webgl_egl_bind_buffer_base(nx_webgl_egl_t *backend, uint32_t target,
                                    uint32_t index, uint32_t buffer);
void nx_webgl_egl_bind_buffer_range(nx_webgl_egl_t *backend, uint32_t target,
                                     uint32_t index, uint32_t buffer,
                                     size_t offset, size_t size);
uint32_t nx_webgl_egl_get_uniform_block_index(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                const char *name);
void nx_webgl_egl_uniform_block_binding(nx_webgl_egl_t *backend,
                                         uint32_t program_handle,
                                         uint32_t block_index,
                                         uint32_t binding);
bool nx_webgl_egl_get_active_uniform_block_iv(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                uint32_t block_index,
                                                uint32_t pname,
                                                int *out);
bool nx_webgl_egl_get_active_uniform_block_name(nx_webgl_egl_t *backend,
                                                  uint32_t program_handle,
                                                  uint32_t block_index,
                                                  char *name, size_t name_size);
bool nx_webgl_egl_get_active_uniforms_iv(nx_webgl_egl_t *backend,
                                          uint32_t program_handle,
                                          int count, const uint32_t *indices,
                                          uint32_t pname, int *out);

// Sampler objects.
uint32_t nx_webgl_egl_gen_sampler(nx_webgl_egl_t *backend);
void nx_webgl_egl_delete_sampler(nx_webgl_egl_t *backend, uint32_t handle);
void nx_webgl_egl_bind_sampler(nx_webgl_egl_t *backend, uint32_t unit,
                                uint32_t handle);
void nx_webgl_egl_sampler_parameteri(nx_webgl_egl_t *backend, uint32_t handle,
                                      uint32_t pname, int param);
void nx_webgl_egl_sampler_parameterf(nx_webgl_egl_t *backend, uint32_t handle,
                                      uint32_t pname, float param);
bool nx_webgl_egl_get_sampler_parameter_iv(nx_webgl_egl_t *backend,
                                             uint32_t handle, uint32_t pname,
                                             int *out);

// Sync objects.
void *nx_webgl_egl_fence_sync(nx_webgl_egl_t *backend, uint32_t condition,
                               uint32_t flags);
void nx_webgl_egl_delete_sync(nx_webgl_egl_t *backend, void *sync);
uint32_t nx_webgl_egl_client_wait_sync(nx_webgl_egl_t *backend, void *sync,
                                         uint32_t flags, uint64_t timeout);
void nx_webgl_egl_wait_sync(nx_webgl_egl_t *backend, void *sync,
                             uint32_t flags, uint64_t timeout);
bool nx_webgl_egl_get_sync_iv(nx_webgl_egl_t *backend, void *sync,
                                uint32_t pname, int *out);

// Query objects.
uint32_t nx_webgl_egl_gen_query(nx_webgl_egl_t *backend);
void nx_webgl_egl_delete_query(nx_webgl_egl_t *backend, uint32_t handle);
void nx_webgl_egl_begin_query(nx_webgl_egl_t *backend, uint32_t target,
                               uint32_t handle);
void nx_webgl_egl_end_query(nx_webgl_egl_t *backend, uint32_t target);
bool nx_webgl_egl_get_query_iv(nx_webgl_egl_t *backend, uint32_t target,
                                uint32_t pname, int *out);
bool nx_webgl_egl_get_query_object_uiv(nx_webgl_egl_t *backend,
                                         uint32_t handle, uint32_t pname,
                                         uint32_t *out);

// Transform feedback.
uint32_t nx_webgl_egl_gen_transform_feedback(nx_webgl_egl_t *backend);
void nx_webgl_egl_delete_transform_feedback(nx_webgl_egl_t *backend,
                                              uint32_t handle);
void nx_webgl_egl_bind_transform_feedback(nx_webgl_egl_t *backend,
                                            uint32_t target, uint32_t handle);
void nx_webgl_egl_begin_transform_feedback(nx_webgl_egl_t *backend,
                                             uint32_t primitive_mode);
void nx_webgl_egl_end_transform_feedback(nx_webgl_egl_t *backend);
void nx_webgl_egl_pause_transform_feedback(nx_webgl_egl_t *backend);
void nx_webgl_egl_resume_transform_feedback(nx_webgl_egl_t *backend);
void nx_webgl_egl_transform_feedback_varyings(nx_webgl_egl_t *backend,
                                                uint32_t program_handle,
                                                int count,
                                                const char *const *varyings,
                                                uint32_t buffer_mode);
bool nx_webgl_egl_get_transform_feedback_varying(nx_webgl_egl_t *backend,
                                                   uint32_t program_handle,
                                                   uint32_t index,
                                                   char *name, size_t name_size,
                                                   int *size, uint32_t *type);

// Misc.
int nx_webgl_egl_get_frag_data_location(nx_webgl_egl_t *backend,
                                          uint32_t program_handle,
                                          const char *name);
bool nx_webgl_egl_get_internal_format_iv(nx_webgl_egl_t *backend,
                                           uint32_t target,
                                           uint32_t internalformat,
                                           uint32_t pname, int buf_size,
                                           int *out);
int nx_webgl_egl_get_max_samples(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_3d_texture_size(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_array_texture_layers(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_draw_buffers(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_color_attachments(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_uniform_buffer_bindings(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_uniform_buffer_offset_alignment(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_vertex_uniform_blocks(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_fragment_uniform_blocks(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_combined_uniform_blocks(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_transform_feedback_separate_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_transform_feedback_interleaved_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_transform_feedback_separate_attribs(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_element_index(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_elements_vertices(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_elements_indices(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_server_wait_timeout(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_program_texel_offset(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_min_program_texel_offset(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_varying_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_vertex_uniform_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_fragment_uniform_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_vertex_output_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_fragment_input_components(nx_webgl_egl_t *backend);
int nx_webgl_egl_get_max_texture_lod_bias(nx_webgl_egl_t *backend);
