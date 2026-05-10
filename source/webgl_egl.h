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
bool nx_webgl_egl_clear_bridge(nx_webgl_egl_t *backend, nx_canvas_t *canvas);
bool nx_webgl_egl_draw_triangles_bridge(nx_webgl_egl_t *backend,
										nx_canvas_t *canvas,
										const float *clip_xy,
										int vertex_count,
										const float *color,
										bool blend,
										uint32_t blend_src,
										uint32_t blend_dst);
bool nx_webgl_egl_draw_textured_triangles_bridge(
	nx_webgl_egl_t *backend,
	nx_canvas_t *canvas,
	const float *clip_uv,
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
	uint32_t blend_dst);
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
