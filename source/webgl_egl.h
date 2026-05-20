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
 * in switch-web-browser demos. See the big STATE comment block in
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
  bool use_derivative_normals);
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
  bool use_derivative_normals);
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
	uint32_t front_face);

bool nx_webgl_egl_has_instancing(nx_webgl_egl_t *backend);

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
uint32_t nx_webgl_egl_check_framebuffer_status(nx_webgl_egl_t *backend,
                                                 uint32_t handle);
bool nx_webgl_egl_framebuffer_texture_2d(nx_webgl_egl_t *backend,
                                          uint32_t framebuffer_handle,
                                          uint32_t attachment,
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

// Read pixels from a user-bound FBO. Standard GL bottom-up convention
// (no y-flip — the user FBO doesn't share the bridge FBO's canvas-y
// top-down quirk). Restores binding to whatever was current. Used when
// `gl.readPixels` is called with a non-null `framebuffer_binding`.
bool nx_webgl_egl_read_user_fbo_pixels(nx_webgl_egl_t *backend,
                                        uint32_t fbo_handle,
                                        int x, int y, int width, int height,
                                        uint32_t format, uint32_t type,
                                        uint8_t *dst);
