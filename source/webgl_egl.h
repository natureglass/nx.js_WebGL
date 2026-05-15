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
void nx_webgl_egl_set_bridge_resolution(nx_webgl_egl_t *backend,
										int width,
										int height);
void nx_webgl_egl_delete_cached_texture(nx_webgl_egl_t *backend,
										uint32_t texture_id);
bool nx_webgl_egl_clear_bridge(nx_webgl_egl_t *backend, nx_canvas_t *canvas);
bool nx_webgl_egl_clear_bridge_with_state(nx_webgl_egl_t *backend,
  nx_canvas_t *canvas,
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
  const int *viewport,
  bool scissor_enabled,
  const int *scissor_box,
  bool depth_enabled);
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
  const int *viewport,
  bool scissor_enabled,
  const int *scissor_box,
  bool depth_enabled);
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
  uint32_t min_filter,
  uint32_t mag_filter,
  uint32_t wrap_s,
  uint32_t wrap_t,
  bool blend,
  uint32_t blend_src,
  uint32_t blend_dst,
  const int *viewport,
  bool scissor_enabled,
  const int *scissor_box,
  bool depth_enabled);
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
bool nx_webgl_egl_link_program(nx_webgl_egl_t *backend,
							   nx_canvas_t *canvas,
							   uint32_t vertex_shader_handle,
							   uint32_t fragment_shader_handle,
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
void nx_webgl_egl_uniform1f(nx_webgl_egl_t *backend, int location, float x);
void nx_webgl_egl_uniform2f(nx_webgl_egl_t *backend, int location, float x, float y);
void nx_webgl_egl_uniform3f(nx_webgl_egl_t *backend, int location, float x, float y, float z);
void nx_webgl_egl_uniform4f(nx_webgl_egl_t *backend, int location, float x, float y, float z, float w);
void nx_webgl_egl_uniform1i(nx_webgl_egl_t *backend, int location, int x);
void nx_webgl_egl_uniform_matrix4fv(nx_webgl_egl_t *backend, int location, bool transpose, const float *value);
