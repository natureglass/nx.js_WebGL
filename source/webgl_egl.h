#pragma once
#include "canvas.h"
#include "types.h"

typedef struct nx_webgl_egl_s nx_webgl_egl_t;

nx_webgl_egl_t *nx_webgl_egl_create(JSContext *ctx, nx_canvas_t *canvas);
void nx_webgl_egl_destroy(JSRuntime *rt, nx_webgl_egl_t *backend);
bool nx_webgl_egl_is_available(nx_webgl_egl_t *backend);
void nx_webgl_egl_set_clear_color(nx_webgl_egl_t *backend, double *color);
bool nx_webgl_egl_clear_prototype(nx_webgl_egl_t *backend,
								  nx_canvas_t *canvas);
JSValue nx_webgl_egl_get_backend_info(JSContext *ctx, nx_webgl_egl_t *backend);
