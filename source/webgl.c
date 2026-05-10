#include "webgl.h"
#include "webgl_egl.h"
#include "canvas.h"
#include "util.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_TRIANGLES 0x0004
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_DITHER 0x0BD0
#define GL_BLEND 0x0BE2
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_SCISSOR_TEST 0x0C11
#define GL_STENCIL_TEST 0x0B90
#define GL_VIEWPORT 0x0BA2
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_DELETE_STATUS 0x8B80
#define GL_SHADER_TYPE 0x8B4F
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_ARRAY_BUFFER 0x8892
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_RGBA 0x1908
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_COVERAGE 0x80A0

#define GL_CAP_BLEND (1u << 0)
#define GL_CAP_CULL_FACE (1u << 1)
#define GL_CAP_DEPTH_TEST (1u << 2)
#define GL_CAP_DITHER (1u << 3)
#define GL_CAP_POLYGON_OFFSET_FILL (1u << 4)
#define GL_CAP_SAMPLE_ALPHA_TO_COVERAGE (1u << 5)
#define GL_CAP_SAMPLE_COVERAGE (1u << 6)
#define GL_CAP_SCISSOR_TEST (1u << 7)
#define GL_CAP_STENCIL_TEST (1u << 8)

#define NX_WEBGL_MAX_VERTEX_ATTRIBS 8

typedef struct {
	bool enabled;
	int size;
	uint32_t type;
	bool normalized;
	int stride;
	int offset;
	JSValue buffer;
} nx_webgl_vertex_attrib_t;

typedef struct {
	nx_canvas_t *canvas;
	JSValue canvas_value;
	double clear_color[4];
	double clear_depth;
	int viewport[4];
	uint32_t depth_func;
	uint32_t enabled_caps;
	uint32_t blend_src;
	uint32_t blend_dst;
	JSValue current_program;
	JSValue array_buffer_binding;
	JSValue element_array_buffer_binding;
	JSValue texture_2d_binding;
	uint32_t active_texture;
	nx_webgl_vertex_attrib_t vertex_attribs[NX_WEBGL_MAX_VERTEX_ATTRIBS];
	nx_webgl_egl_t *egl;
	bool bridge_clear_pending;
	uint32_t next_texture_id;
	uint32_t error;
} nx_webgl_context_t;

typedef struct {
	uint32_t type;
	char *source;
	char *info_log;
	bool compile_status;
	bool deleted;
} nx_webgl_shader_t;

typedef struct {
	JSValue vertex_shader;
	JSValue fragment_shader;
	char *info_log;
	float matrix4[16];
	float color[4];
	float offset[2];
	bool has_matrix4;
	bool has_color;
	bool has_offset;
	int sampler0;
	bool has_sampler0;
	bool link_status;
	bool deleted;
} nx_webgl_program_t;

typedef struct {
	uint32_t target;
	uint32_t usage;
	size_t size;
	uint8_t *data;
	bool deleted;
} nx_webgl_buffer_t;

typedef struct {
	uint32_t target;
	uint32_t min_filter;
	uint32_t mag_filter;
	uint32_t wrap_s;
	uint32_t wrap_t;
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	int *alpha_min_x;
	int *alpha_max_x;
	uint32_t bridge_id;
	uint32_t revision;
	bool deleted;
} nx_webgl_texture_t;

typedef struct {
	float x;
	float y;
} nx_webgl_vec2_t;

typedef struct {
	nx_webgl_vec2_t p[4];
	nx_webgl_vec2_t uv[4];
} nx_webgl_textured_quad_t;

typedef enum {
	NX_WEBGL_UNIFORM_UNKNOWN,
	NX_WEBGL_UNIFORM_MATRIX4,
	NX_WEBGL_UNIFORM_COLOR,
	NX_WEBGL_UNIFORM_OFFSET,
	NX_WEBGL_UNIFORM_SAMPLER,
} nx_webgl_uniform_kind_t;

typedef struct {
	JSValue program;
	char *name;
	nx_webgl_uniform_kind_t kind;
} nx_webgl_uniform_location_t;

static JSClassID nx_webgl_context_class_id;
static JSClassID nx_webgl_shader_class_id;
static JSClassID nx_webgl_program_class_id;
static JSClassID nx_webgl_buffer_class_id;
static JSClassID nx_webgl_uniform_location_class_id;
static JSClassID nx_webgl_texture_class_id;

static double clamp01(double value) {
	if (isnan(value) || value < 0.)
		return 0.;
	if (value > 1.)
		return 1.;
	return value;
}

static uint8_t to_u8(double value) {
	return (uint8_t)(clamp01(value) * 255. + 0.5);
}

static uint32_t cap_to_flag(uint32_t cap) {
	switch (cap) {
	case GL_BLEND:
		return GL_CAP_BLEND;
	case GL_CULL_FACE:
		return GL_CAP_CULL_FACE;
	case GL_DEPTH_TEST:
		return GL_CAP_DEPTH_TEST;
	case GL_DITHER:
		return GL_CAP_DITHER;
	case GL_POLYGON_OFFSET_FILL:
		return GL_CAP_POLYGON_OFFSET_FILL;
	case GL_SAMPLE_ALPHA_TO_COVERAGE:
		return GL_CAP_SAMPLE_ALPHA_TO_COVERAGE;
	case GL_SAMPLE_COVERAGE:
		return GL_CAP_SAMPLE_COVERAGE;
	case GL_SCISSOR_TEST:
		return GL_CAP_SCISSOR_TEST;
	case GL_STENCIL_TEST:
		return GL_CAP_STENCIL_TEST;
	default:
		return 0;
	}
}

static bool is_depth_func(uint32_t func) {
	return func == GL_NEVER || func == GL_LESS || func == GL_EQUAL ||
		   func == GL_LEQUAL || func == GL_GREATER || func == GL_NOTEQUAL ||
		   func == GL_GEQUAL || func == GL_ALWAYS;
}

static bool is_shader_type(uint32_t type) {
	return type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER;
}

static bool is_buffer_usage(uint32_t usage) {
	return usage == GL_STATIC_DRAW || usage == GL_DYNAMIC_DRAW ||
		   usage == GL_STREAM_DRAW;
}

static bool is_vertex_attrib_type(uint32_t type) {
	return type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == GL_SHORT ||
		   type == GL_UNSIGNED_SHORT || type == GL_FLOAT;
}

static bool is_blend_factor(uint32_t factor) {
	return factor == GL_ZERO || factor == GL_ONE || factor == GL_SRC_ALPHA ||
		   factor == GL_ONE_MINUS_SRC_ALPHA;
}

static nx_webgl_uniform_kind_t uniform_kind_for_name(const char *name) {
	if (strcmp(name, "u_matrix") == 0 || strcmp(name, "matrix") == 0 ||
		strcmp(name, "u_transform") == 0 || strcmp(name, "transform") == 0)
		return NX_WEBGL_UNIFORM_MATRIX4;
	if (strcmp(name, "u_color") == 0 || strcmp(name, "color") == 0)
		return NX_WEBGL_UNIFORM_COLOR;
	if (strcmp(name, "u_offset") == 0 || strcmp(name, "offset") == 0 ||
		strcmp(name, "translation") == 0)
		return NX_WEBGL_UNIFORM_OFFSET;
	if (strcmp(name, "u_texture") == 0 || strcmp(name, "texture") == 0 ||
		strcmp(name, "u_sampler") == 0 || strcmp(name, "sampler") == 0)
		return NX_WEBGL_UNIFORM_SAMPLER;
	return NX_WEBGL_UNIFORM_UNKNOWN;
}

static int clamp_int(int value, int min_value, int max_value) {
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

static float edge_function(nx_webgl_vec2_t a, nx_webgl_vec2_t b,
						   nx_webgl_vec2_t c) {
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

static inline bool scanline_edge_intersection(nx_webgl_vec2_t a,
											  nx_webgl_vec2_t b,
											  float y,
											  float *x_out) {
	if (a.y == b.y)
		return false;
	float min_y = fminf(a.y, b.y);
	float max_y = fmaxf(a.y, b.y);
	if (y < min_y || y > max_y)
		return false;
	float t = (y - a.y) / (b.y - a.y);
	if (t < 0.f || t > 1.f)
		return false;
	*x_out = a.x + (b.x - a.x) * t;
	return true;
}

static bool triangle_scanline_span(nx_webgl_vec2_t v0, nx_webgl_vec2_t v1,
								   nx_webgl_vec2_t v2, float y,
								   float *x_min_out,
								   float *x_max_out) {
	float xs[3];
	int count = 0;
	if (scanline_edge_intersection(v0, v1, y, &xs[count]))
		count++;
	if (scanline_edge_intersection(v1, v2, y, &xs[count]))
		count++;
	if (scanline_edge_intersection(v2, v0, y, &xs[count]))
		count++;
	if (count < 2)
		return false;

	float x_min = xs[0];
	float x_max = xs[0];
	for (int i = 1; i < count; i++) {
		if (xs[i] < x_min)
			x_min = xs[i];
		if (xs[i] > x_max)
			x_max = xs[i];
	}
	if (x_max < x_min)
		return false;
	*x_min_out = x_min;
	*x_max_out = x_max;
	return true;
}

static void replace_string(JSContext *ctx, char **target, const char *value) {
	if (*target) {
		js_free(ctx, *target);
		*target = NULL;
	}
	*target = js_strdup(ctx, value ? value : "");
}

static JSValue new_number_array(JSContext *ctx, const double *values,
								size_t count) {
	JSValue array = JS_NewArray(ctx);
	for (size_t i = 0; i < count; i++)
		JS_SetPropertyUint32(ctx, array, i, JS_NewFloat64(ctx, values[i]));
	return array;
}

static JSValue new_int_array(JSContext *ctx, const int *values, size_t count) {
	JSValue array = JS_NewArray(ctx);
	for (size_t i = 0; i < count; i++)
		JS_SetPropertyUint32(ctx, array, i, JS_NewInt32(ctx, values[i]));
	return array;
}

static nx_webgl_context_t *nx_get_webgl_context(JSContext *ctx,
												JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_webgl_context_class_id);
}

static nx_webgl_shader_t *nx_get_webgl_shader(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_shader_class_id);
}

static nx_webgl_program_t *nx_get_webgl_program(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_program_class_id);
}

static nx_webgl_buffer_t *nx_get_webgl_buffer(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_buffer_class_id);
}

static nx_webgl_uniform_location_t *
nx_get_webgl_uniform_location(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_uniform_location_class_id);
}

static nx_webgl_texture_t *nx_get_webgl_texture(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_texture_class_id);
}

static JSValue nx_webgl_context_new(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);
	if (!canvas)
		return JS_EXCEPTION;

	nx_webgl_context_t *context = js_mallocz(ctx, sizeof(nx_webgl_context_t));
	if (!context)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_context_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		return obj;
	}

	context->canvas = canvas;
	context->canvas_value = JS_DupValue(ctx, argv[0]);
	context->viewport[0] = 0;
	context->viewport[1] = 0;
	context->viewport[2] = canvas->width;
	context->viewport[3] = canvas->height;
	context->clear_depth = 1.;
	context->depth_func = GL_LESS;
	context->enabled_caps = GL_CAP_DITHER;
	context->blend_src = GL_ONE;
	context->blend_dst = GL_ZERO;
	context->current_program = JS_UNDEFINED;
	context->array_buffer_binding = JS_UNDEFINED;
	context->element_array_buffer_binding = JS_UNDEFINED;
	context->texture_2d_binding = JS_UNDEFINED;
	context->active_texture = GL_TEXTURE0;
	context->next_texture_id = 1;
	for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++)
		context->vertex_attribs[i].buffer = JS_UNDEFINED;
	context->egl = nx_webgl_egl_create(ctx, canvas);

	JS_SetOpaque(obj, context);
	return obj;
}

static void finalizer_webgl_context(JSRuntime *rt, JSValue val) {
	nx_webgl_context_t *context =
		JS_GetOpaque(val, nx_webgl_context_class_id);
	if (context) {
		JS_FreeValueRT(rt, context->canvas_value);
		JS_FreeValueRT(rt, context->current_program);
		JS_FreeValueRT(rt, context->array_buffer_binding);
		JS_FreeValueRT(rt, context->element_array_buffer_binding);
		JS_FreeValueRT(rt, context->texture_2d_binding);
		for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++)
			JS_FreeValueRT(rt, context->vertex_attribs[i].buffer);
		nx_webgl_egl_destroy(rt, context->egl);
		js_free_rt(rt, context);
	}
}

static void finalizer_webgl_shader(JSRuntime *rt, JSValue val) {
	nx_webgl_shader_t *shader = JS_GetOpaque(val, nx_webgl_shader_class_id);
	if (shader) {
		js_free_rt(rt, shader->source);
		js_free_rt(rt, shader->info_log);
		js_free_rt(rt, shader);
	}
}

static void finalizer_webgl_program(JSRuntime *rt, JSValue val) {
	nx_webgl_program_t *program = JS_GetOpaque(val, nx_webgl_program_class_id);
	if (program) {
		JS_FreeValueRT(rt, program->vertex_shader);
		JS_FreeValueRT(rt, program->fragment_shader);
		js_free_rt(rt, program->info_log);
		js_free_rt(rt, program);
	}
}

static void finalizer_webgl_buffer(JSRuntime *rt, JSValue val) {
	nx_webgl_buffer_t *buffer = JS_GetOpaque(val, nx_webgl_buffer_class_id);
	if (buffer) {
		js_free_rt(rt, buffer->data);
		js_free_rt(rt, buffer);
	}
}

static void finalizer_webgl_uniform_location(JSRuntime *rt, JSValue val) {
	nx_webgl_uniform_location_t *location =
		JS_GetOpaque(val, nx_webgl_uniform_location_class_id);
	if (location) {
		JS_FreeValueRT(rt, location->program);
		js_free_rt(rt, location->name);
		js_free_rt(rt, location);
	}
}

static void finalizer_webgl_texture(JSRuntime *rt, JSValue val) {
	nx_webgl_texture_t *texture = JS_GetOpaque(val, nx_webgl_texture_class_id);
	if (texture) {
		js_free_rt(rt, texture->data);
		js_free_rt(rt, texture->alpha_min_x);
		js_free_rt(rt, texture->alpha_max_x);
		js_free_rt(rt, texture);
	}
}

static JSValue nx_webgl_get_drawing_buffer_width(JSContext *ctx,
												 JSValueConst this_val,
												 int argc,
												 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, context->canvas->width);
}

static JSValue nx_webgl_get_drawing_buffer_height(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, context->canvas->height);
}

static JSValue nx_webgl_clear_color(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	double values[4];
	for (int i = 0; i < 4; i++) {
		if (JS_ToFloat64(ctx, &values[i], argv[i]))
			return JS_EXCEPTION;
		context->clear_color[i] = clamp01(values[i]);
	}
	nx_webgl_egl_set_clear_color(context->egl, context->clear_color);

	return JS_UNDEFINED;
}

static void clear_canvas_software(nx_webgl_context_t *context) {
	nx_canvas_t *canvas = context->canvas;
	if (!canvas || !canvas->data || canvas->width == 0 || canvas->height == 0)
		return;

	uint8_t r = to_u8(context->clear_color[0]);
	uint8_t g = to_u8(context->clear_color[1]);
	uint8_t b = to_u8(context->clear_color[2]);
	uint8_t a = to_u8(context->clear_color[3]);

	uint32_t packed = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
					  ((uint32_t)g << 8) | (uint32_t)b;
	size_t pixel_count = (size_t)canvas->width * canvas->height;
	uint32_t *pixels = (uint32_t *)canvas->data;
	for (size_t i = 0; i < pixel_count; i++)
		pixels[i] = packed;

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

static void flush_pending_bridge_clear_to_software(nx_webgl_context_t *context) {
	if (!context || !context->bridge_clear_pending)
		return;
	context->bridge_clear_pending = false;
	clear_canvas_software(context);
}

static JSValue nx_webgl_clear(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mask;
	if (JS_ToUint32(ctx, &mask, argv[0]))
		return JS_EXCEPTION;

	if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
				  GL_STENCIL_BUFFER_BIT)) != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if ((mask & GL_COLOR_BUFFER_BIT) == 0)
		return JS_UNDEFINED;

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	if (nx_webgl_egl_is_bridge_enabled(context->egl) &&
		nx_webgl_egl_clear_bridge(context->egl, canvas)) {
		context->bridge_clear_pending = true;
		return JS_UNDEFINED;
	}

	context->bridge_clear_pending = false;
	clear_canvas_software(context);

	return JS_UNDEFINED;
}

static JSValue nx_webgl_clear_depth(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	double value;
	if (JS_ToFloat64(ctx, &value, argv[0]))
		return JS_EXCEPTION;
	context->clear_depth = clamp01(value);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t type;
	if (JS_ToUint32(ctx, &type, argv[0]))
		return JS_EXCEPTION;

	if (!is_shader_type(type)) {
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}

	nx_webgl_shader_t *shader = js_mallocz(ctx, sizeof(nx_webgl_shader_t));
	if (!shader)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_shader_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, shader);
		return obj;
	}

	shader->type = type;
	shader->info_log = js_strdup(ctx, "");
	JS_SetOpaque(obj, shader);
	return obj;
}

static JSValue nx_webgl_shader_source(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	const char *source = JS_ToCString(ctx, argv[1]);
	if (!source)
		return JS_EXCEPTION;
	replace_string(ctx, &shader->source, source);
	shader->compile_status = false;
	replace_string(ctx, &shader->info_log, "");
	JS_FreeCString(ctx, source);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_compile_shader(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if (!shader->source || shader->source[0] == '\0') {
		shader->compile_status = false;
		replace_string(ctx, &shader->info_log, "shader source is empty");
		return JS_UNDEFINED;
	}

	if (strstr(shader->source, "void main") == NULL) {
		shader->compile_status = false;
		replace_string(ctx, &shader->info_log,
					   "shader source does not contain void main");
		return JS_UNDEFINED;
	}

	shader->compile_status = true;
	replace_string(ctx, &shader->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_shader_parameter(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[1]))
		return JS_EXCEPTION;

	switch (pname) {
	case GL_COMPILE_STATUS:
		return JS_NewBool(ctx, shader->compile_status);
	case GL_DELETE_STATUS:
		return JS_NewBool(ctx, shader->deleted);
	case GL_SHADER_TYPE:
		return JS_NewUint32(ctx, shader->type);
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_shader_info_log(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	return JS_NewString(ctx, shader->info_log ? shader->info_log : "");
}

static JSValue nx_webgl_delete_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	shader->deleted = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_program(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = js_mallocz(ctx, sizeof(nx_webgl_program_t));
	if (!program)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_program_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, program);
		return obj;
	}

	program->vertex_shader = JS_UNDEFINED;
	program->fragment_shader = JS_UNDEFINED;
	program->info_log = js_strdup(ctx, "");
	for (int i = 0; i < 16; i++)
		program->matrix4[i] = i % 5 == 0 ? 1.f : 0.f;
	program->color[0] = 68.f / 255.f;
	program->color[1] = 215.f / 255.f;
	program->color[2] = 182.f / 255.f;
	program->color[3] = 1.f;
	program->sampler0 = 0;
	JS_SetOpaque(obj, program);
	return obj;
}

static JSValue nx_webgl_attach_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[1]);
	if (!program || !shader || program->deleted || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	JSValue *target = shader->type == GL_VERTEX_SHADER
						  ? &program->vertex_shader
						  : &program->fragment_shader;
	JS_FreeValue(ctx, *target);
	*target = JS_DupValue(ctx, argv[1]);
	program->link_status = false;
	replace_string(ctx, &program->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_link_program(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_shader_t *vertex = nx_get_webgl_shader(program->vertex_shader);
	nx_webgl_shader_t *fragment = nx_get_webgl_shader(program->fragment_shader);
	if (!vertex || !fragment) {
		program->link_status = false;
		replace_string(ctx, &program->info_log,
					   "program requires vertex and fragment shaders");
		return JS_UNDEFINED;
	}

	if (!vertex->compile_status || !fragment->compile_status) {
		program->link_status = false;
		replace_string(ctx, &program->info_log,
					   "attached shaders must compile before linking");
		return JS_UNDEFINED;
	}

	program->link_status = true;
	replace_string(ctx, &program->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_use_program(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0])) {
		JS_FreeValue(ctx, context->current_program);
		context->current_program = JS_UNDEFINED;
		return JS_UNDEFINED;
	}

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	JS_FreeValue(ctx, context->current_program);
	context->current_program = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_program_parameter(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[1]))
		return JS_EXCEPTION;

	switch (pname) {
	case GL_LINK_STATUS:
		return JS_NewBool(ctx, program->link_status);
	case GL_DELETE_STATUS:
		return JS_NewBool(ctx, program->deleted);
	case GL_ATTACHED_SHADERS: {
		uint32_t count = 0;
		if (!JS_IsUndefined(program->vertex_shader))
			count++;
		if (!JS_IsUndefined(program->fragment_shader))
			count++;
		return JS_NewUint32(ctx, count);
	}
	case GL_ACTIVE_UNIFORMS:
		return JS_NewUint32(ctx, 3);
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_program_info_log(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	return JS_NewString(ctx, program->info_log ? program->info_log : "");
}

static JSValue nx_webgl_delete_program(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	program->deleted = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_buffer(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_buffer_t *buffer = js_mallocz(ctx, sizeof(nx_webgl_buffer_t));
	if (!buffer)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_buffer_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, buffer);
		return obj;
	}

	JS_SetOpaque(obj, buffer);
	return obj;
}

static JSValue nx_webgl_bind_buffer(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (JS_IsNull(argv[1])) {
		JSValue *binding = target == GL_ARRAY_BUFFER
							   ? &context->array_buffer_binding
							   : &context->element_array_buffer_binding;
		JS_FreeValue(ctx, *binding);
		*binding = JS_UNDEFINED;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(argv[1]);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (buffer->target != 0 && buffer->target != target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	buffer->target = target;
	JSValue *binding = target == GL_ARRAY_BUFFER
						   ? &context->array_buffer_binding
						   : &context->element_array_buffer_binding;
	JS_FreeValue(ctx, *binding);
	*binding = JS_DupValue(ctx, argv[1]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_buffer_data(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	uint32_t usage;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToUint32(ctx, &usage, argv[2]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (!is_buffer_usage(usage)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(target == GL_ARRAY_BUFFER
								? context->array_buffer_binding
								: context->element_array_buffer_binding);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	size_t size = 0;
	uint8_t *source = NULL;
	if (JS_IsNumber(argv[1])) {
		uint32_t requested_size;
		if (JS_ToUint32(ctx, &requested_size, argv[1]))
			return JS_EXCEPTION;
		size = requested_size;
	} else {
		source = NX_GetBufferSource(ctx, &size, argv[1]);
		if (!source) {
			context->error = GL_INVALID_VALUE;
			return JS_UNDEFINED;
		}
	}

	if (size == 0) {
		js_free(ctx, buffer->data);
		buffer->data = NULL;
	} else if (buffer->data && buffer->size == size) {
		if (source)
			memcpy(buffer->data, source, size);
		else
			memset(buffer->data, 0, size);
	} else {
		uint8_t *copy = js_malloc(ctx, size);
		if (!copy)
			return JS_EXCEPTION;
		if (source)
			memcpy(copy, source, size);
		else
			memset(copy, 0, size);

		js_free(ctx, buffer->data);
		buffer->data = copy;
	}
	buffer->size = size;
	buffer->usage = usage;
	buffer->target = target;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_delete_buffer(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(argv[0]);
	if (!buffer) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	buffer->deleted = true;
	if (nx_get_webgl_buffer(context->array_buffer_binding) == buffer) {
		JS_FreeValue(ctx, context->array_buffer_binding);
		context->array_buffer_binding = JS_UNDEFINED;
	}
	if (nx_get_webgl_buffer(context->element_array_buffer_binding) == buffer) {
		JS_FreeValue(ctx, context->element_array_buffer_binding);
		context->element_array_buffer_binding = JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_texture_t *texture = js_mallocz(ctx, sizeof(nx_webgl_texture_t));
	if (!texture)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_texture_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, texture);
		return obj;
	}

	texture->min_filter = GL_NEAREST;
	texture->mag_filter = GL_NEAREST;
	texture->wrap_s = GL_CLAMP_TO_EDGE;
	texture->wrap_t = GL_CLAMP_TO_EDGE;
	texture->bridge_id = context->next_texture_id++;
	if (texture->bridge_id == 0)
		texture->bridge_id = context->next_texture_id++;
	JS_SetOpaque(obj, texture);
	return obj;
}

static JSValue nx_webgl_active_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t texture;
	if (JS_ToUint32(ctx, &texture, argv[0]))
		return JS_EXCEPTION;
	if (texture != GL_TEXTURE0) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->active_texture = texture;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_bind_texture(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_TEXTURE_2D) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (JS_IsNull(argv[1])) {
		JS_FreeValue(ctx, context->texture_2d_binding);
		context->texture_2d_binding = JS_UNDEFINED;
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture = nx_get_webgl_texture(argv[1]);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (texture->target != 0 && texture->target != target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	texture->target = target;
	JS_FreeValue(ctx, context->texture_2d_binding);
	context->texture_2d_binding = JS_DupValue(ctx, argv[1]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_tex_parameteri(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	uint32_t pname;
	uint32_t param;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToUint32(ctx, &pname, argv[1]) ||
		JS_ToUint32(ctx, &param, argv[2]))
		return JS_EXCEPTION;
	if (target != GL_TEXTURE_2D) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture =
		nx_get_webgl_texture(context->texture_2d_binding);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	switch (pname) {
	case GL_TEXTURE_MIN_FILTER:
	case GL_TEXTURE_MAG_FILTER:
		if (param != GL_NEAREST && param != GL_LINEAR) {
			context->error = GL_INVALID_ENUM;
			return JS_UNDEFINED;
		}
		if (pname == GL_TEXTURE_MIN_FILTER)
			texture->min_filter = param;
		else
			texture->mag_filter = param;
		return JS_UNDEFINED;
	case GL_TEXTURE_WRAP_S:
	case GL_TEXTURE_WRAP_T:
		if (param != GL_CLAMP_TO_EDGE && param != GL_REPEAT) {
			context->error = GL_INVALID_ENUM;
			return JS_UNDEFINED;
		}
		if (pname == GL_TEXTURE_WRAP_S)
			texture->wrap_s = param;
		else
			texture->wrap_t = param;
		return JS_UNDEFINED;
	default:
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
}

static JSValue nx_webgl_tex_image_2d(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	int32_t level;
	uint32_t internal_format;
	int32_t width;
	int32_t height;
	int32_t border;
	uint32_t format;
	uint32_t type;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToInt32(ctx, &level, argv[1]) ||
		JS_ToUint32(ctx, &internal_format, argv[2]) ||
		JS_ToInt32(ctx, &width, argv[3]) || JS_ToInt32(ctx, &height, argv[4]) ||
		JS_ToInt32(ctx, &border, argv[5]) || JS_ToUint32(ctx, &format, argv[6]) ||
		JS_ToUint32(ctx, &type, argv[7]))
		return JS_EXCEPTION;

	if (target != GL_TEXTURE_2D || internal_format != GL_RGBA ||
		format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (level != 0 || width <= 0 || height <= 0 || border != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture =
		nx_get_webgl_texture(context->texture_2d_binding);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	size_t byte_length = 0;
	uint8_t *source = NULL;
	if (!JS_IsNull(argv[8]))
		source = NX_GetBufferSource(ctx, &byte_length, argv[8]);
	size_t expected = (size_t)width * (size_t)height * 4;
	if (!source || byte_length < expected) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	uint8_t *copy = js_malloc(ctx, expected);
	if (!copy)
		return JS_EXCEPTION;
	memcpy(copy, source, expected);

	int *alpha_min_x = js_malloc(ctx, sizeof(int) * (size_t)height);
	int *alpha_max_x = js_malloc(ctx, sizeof(int) * (size_t)height);
	if (!alpha_min_x || !alpha_max_x) {
		js_free(ctx, copy);
		js_free(ctx, alpha_min_x);
		js_free(ctx, alpha_max_x);
		return JS_EXCEPTION;
	}
	for (int y = 0; y < height; y++) {
		int min_x = width;
		int max_x = -1;
		for (int x = 0; x < width; x++) {
			uint8_t alpha = copy[((size_t)y * (size_t)width + (size_t)x) * 4 + 3];
			if (alpha != 0) {
				if (min_x == width)
					min_x = x;
				max_x = x;
			}
		}
		alpha_min_x[y] = min_x;
		alpha_max_x[y] = max_x;
	}

	js_free(ctx, texture->data);
	js_free(ctx, texture->alpha_min_x);
	js_free(ctx, texture->alpha_max_x);
	texture->data = copy;
	texture->alpha_min_x = alpha_min_x;
	texture->alpha_max_x = alpha_max_x;
	texture->width = width;
	texture->height = height;
	texture->target = target;
	texture->revision++;
	if (texture->revision == 0)
		texture->revision = 1;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_delete_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_texture_t *texture = nx_get_webgl_texture(argv[0]);
	if (!texture) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	texture->deleted = true;
	if (nx_get_webgl_texture(context->texture_2d_binding) == texture) {
		JS_FreeValue(ctx, context->texture_2d_binding);
		context->texture_2d_binding = JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_uniform_location(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NULL;
	}

	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name)
		return JS_EXCEPTION;

	nx_webgl_uniform_kind_t kind = uniform_kind_for_name(name);
	if (kind == NX_WEBGL_UNIFORM_UNKNOWN) {
		JS_FreeCString(ctx, name);
		return JS_NULL;
	}

	nx_webgl_uniform_location_t *location =
		js_mallocz(ctx, sizeof(nx_webgl_uniform_location_t));
	if (!location) {
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_uniform_location_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, location);
		JS_FreeCString(ctx, name);
		return obj;
	}

	location->program = JS_DupValue(ctx, argv[0]);
	location->name = js_strdup(ctx, name);
	location->kind = kind;
	JS_SetOpaque(obj, location);
	JS_FreeCString(ctx, name);
	return obj;
}

static bool get_uniform_program(nx_webgl_context_t *context,
								nx_webgl_uniform_location_t *location,
								nx_webgl_program_t **out_program) {
	if (!location)
		return false;
	nx_webgl_program_t *program = nx_get_webgl_program(location->program);
	if (!program || program->deleted || !program->link_status)
		return false;
	*out_program = program;
	return true;
}

static JSValue nx_webgl_uniform2f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (location->kind != NX_WEBGL_UNIFORM_OFFSET) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	double x;
	double y;
	if (JS_ToFloat64(ctx, &x, argv[1]) || JS_ToFloat64(ctx, &y, argv[2]))
		return JS_EXCEPTION;
	program->offset[0] = (float)x;
	program->offset[1] = (float)y;
	program->has_offset = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform4f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (location->kind != NX_WEBGL_UNIFORM_COLOR) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	for (int i = 0; i < 4; i++) {
		double value;
		if (JS_ToFloat64(ctx, &value, argv[i + 1]))
			return JS_EXCEPTION;
		program->color[i] = (float)clamp01(value);
	}
	program->has_color = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform1i(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (location->kind != NX_WEBGL_UNIFORM_SAMPLER) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	int32_t value;
	if (JS_ToInt32(ctx, &value, argv[1]))
		return JS_EXCEPTION;
	if (value != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	program->sampler0 = value;
	program->has_sampler0 = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform_matrix4fv(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	int transpose = JS_ToBool(ctx, argv[1]);
	if (transpose < 0)
		return JS_EXCEPTION;
	if (transpose) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (location->kind != NX_WEBGL_UNIFORM_MATRIX4) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	size_t byte_length = 0;
	uint8_t *source = NX_GetBufferSource(ctx, &byte_length, argv[2]);
	if (!source || byte_length < sizeof(float) * 16) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	memcpy(program->matrix4, source, sizeof(float) * 16);
	program->has_matrix4 = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_attrib_location(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NewInt32(ctx, -1);
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NewInt32(ctx, -1);
	}

	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name)
		return JS_EXCEPTION;

	int32_t location = -1;
	if (strcmp(name, "position") == 0 || strcmp(name, "a_position") == 0)
		location = 0;
	else if (strcmp(name, "color") == 0 || strcmp(name, "a_color") == 0)
		location = 1;
	else if (strcmp(name, "texcoord") == 0 || strcmp(name, "texCoord") == 0 ||
			 strcmp(name, "uv") == 0 || strcmp(name, "a_uv") == 0)
		location = 2;

	JS_FreeCString(ctx, name);
	return JS_NewInt32(ctx, location);
}

static JSValue nx_webgl_enable_vertex_attrib_array(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[0]))
		return JS_EXCEPTION;
	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	context->vertex_attribs[index].enabled = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_disable_vertex_attrib_array(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[0]))
		return JS_EXCEPTION;
	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	context->vertex_attribs[index].enabled = false;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_vertex_attrib_pointer(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	uint32_t size;
	uint32_t type;
	int normalized;
	int32_t stride;
	int32_t offset;
	if (JS_ToUint32(ctx, &index, argv[0]) ||
		JS_ToUint32(ctx, &size, argv[1]) || JS_ToUint32(ctx, &type, argv[2]) ||
		JS_ToBool(ctx, argv[3]) < 0 || JS_ToInt32(ctx, &stride, argv[4]) ||
		JS_ToInt32(ctx, &offset, argv[5]))
		return JS_EXCEPTION;
	normalized = JS_ToBool(ctx, argv[3]);

	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS || size < 1 || size > 4 ||
		stride < 0 || offset < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (!is_vertex_attrib_type(type)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(context->array_buffer_binding);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_vertex_attrib_t *attrib = &context->vertex_attribs[index];
	attrib->size = size;
	attrib->type = type;
	attrib->normalized = normalized != 0;
	attrib->stride = stride;
	attrib->offset = offset;
	JS_FreeValue(ctx, attrib->buffer);
	attrib->buffer = JS_DupValue(ctx, context->array_buffer_binding);
	return JS_UNDEFINED;
}

static bool read_attrib_vec2(nx_webgl_context_t *context,
							 nx_webgl_vertex_attrib_t *attrib,
							 int vertex_index, nx_webgl_vec2_t *out) {
	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(attrib->buffer);
	if (!buffer || buffer->deleted || !buffer->data)
		return false;

	int stride = attrib->stride == 0 ? attrib->size * (int)sizeof(float)
									 : attrib->stride;
	size_t offset = (size_t)attrib->offset + (size_t)vertex_index * stride;
	if (offset + sizeof(float) * 2 > buffer->size)
		return false;

	memcpy(&out->x, buffer->data + offset, sizeof(float));
	memcpy(&out->y, buffer->data + offset + sizeof(float), sizeof(float));
	return true;
}

static nx_webgl_vec2_t clip_to_pixel(nx_webgl_context_t *context,
									 nx_webgl_vec2_t clip) {
	float vx = (float)context->viewport[0];
	float vy = (float)context->viewport[1];
	float vw = (float)context->viewport[2];
	float vh = (float)context->viewport[3];
	return (nx_webgl_vec2_t){
		vx + (clip.x * 0.5f + 0.5f) * vw,
		vy + (0.5f - clip.y * 0.5f) * vh,
	};
}

static nx_webgl_vec2_t transform_position(nx_webgl_program_t *program,
										  nx_webgl_vec2_t position) {
	nx_webgl_vec2_t out = position;
	if (program->has_matrix4) {
		float x = position.x;
		float y = position.y;
		float *m = program->matrix4;
		float tx = m[0] * x + m[4] * y + m[12];
		float ty = m[1] * x + m[5] * y + m[13];
		float tw = m[3] * x + m[7] * y + m[15];
		if (tw != 0.f) {
			tx /= tw;
			ty /= tw;
		}
		out.x = tx;
		out.y = ty;
	}
	if (program->has_offset) {
		out.x += program->offset[0];
		out.y += program->offset[1];
	}
	return out;
}

static uint32_t program_color(nx_webgl_program_t *program) {
	float *color = program->has_color ? program->color : (float[4]){
		68.f / 255.f,
		215.f / 255.f,
		182.f / 255.f,
		1.f,
	};
	uint8_t r = to_u8(color[0]);
	uint8_t g = to_u8(color[1]);
	uint8_t b = to_u8(color[2]);
	uint8_t a = to_u8(color[3]);
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) |
		   (uint32_t)b;
}

static nx_webgl_texture_t *active_texture_for_program(nx_webgl_context_t *context,
													  nx_webgl_program_t *program) {
	if (!program->has_sampler0 || program->sampler0 != 0)
		return NULL;
	nx_webgl_texture_t *texture =
		nx_get_webgl_texture(context->texture_2d_binding);
	if (!texture || texture->deleted || !texture->data || texture->width == 0 ||
		texture->height == 0)
		return NULL;
	return texture;
}

static inline int texture_coord_to_int(float value, uint32_t wrap_mode,
									   uint32_t size) {
	if (wrap_mode == GL_REPEAT) {
		value = value - floorf(value);
		if (value < 0.f)
			value += 1.f;
	} else {
		if (value < 0.f)
			value = 0.f;
		else if (value > 1.f)
			value = 1.f;
	}

	int coord = (int)(value * (float)size);
	if (coord < 0)
		return 0;
	if (coord >= (int)size)
		return (int)size - 1;
	return coord;
}

static inline bool texture_pixel_is_transparent(nx_webgl_texture_t *texture,
												int x, int y) {
	return texture->alpha_min_x && texture->alpha_max_x &&
		   (x < texture->alpha_min_x[y] || x > texture->alpha_max_x[y]);
}

static inline uint32_t texture_pixel_rgba(nx_webgl_texture_t *texture, int x,
										  int y) {
	if (x < 0)
		x = 0;
	else if (x >= (int)texture->width)
		x = texture->width - 1;
	if (y < 0)
		y = 0;
	else if (y >= (int)texture->height)
		y = texture->height - 1;
	if (texture_pixel_is_transparent(texture, x, y))
		return 0;
	uint8_t *pixel = texture->data + ((size_t)y * texture->width + x) * 4;
	return ((uint32_t)pixel[3] << 24) | ((uint32_t)pixel[0] << 16) |
		   ((uint32_t)pixel[1] << 8) | (uint32_t)pixel[2];
}

static inline uint32_t texture_pixel_rgba_unchecked(nx_webgl_texture_t *texture,
													int x, int y) {
	if (texture_pixel_is_transparent(texture, x, y))
		return 0;
	uint8_t *pixel = texture->data + ((size_t)y * texture->width + x) * 4;
	return ((uint32_t)pixel[3] << 24) | ((uint32_t)pixel[0] << 16) |
		   ((uint32_t)pixel[1] << 8) | (uint32_t)pixel[2];
}

static inline uint32_t sample_texture_fast(nx_webgl_texture_t *texture, float u,
										   float v) {
	int x = texture_coord_to_int(u, texture->wrap_s, texture->width);
	int y = texture_coord_to_int(v, texture->wrap_t, texture->height);
	return texture_pixel_rgba(texture, x, y);
}

static inline uint32_t sample_texture_clamp_edge_fast(
	nx_webgl_texture_t *texture, float u, float v) {
	if (u < 0.f)
		u = 0.f;
	else if (u > 1.f)
		u = 1.f;
	if (v < 0.f)
		v = 0.f;
	else if (v > 1.f)
		v = 1.f;

	int x = (int)(u * (float)texture->width);
	int y = (int)(v * (float)texture->height);
	if (x >= (int)texture->width)
		x = (int)texture->width - 1;
	if (y >= (int)texture->height)
		y = (int)texture->height - 1;
	return texture_pixel_rgba_unchecked(texture, x, y);
}

static inline uint32_t blend_src_alpha_over(uint32_t dst, uint32_t src) {
	uint32_t src_a = (src >> 24) & 0xff;
	if (src_a == 0)
		return dst;
	if (src_a == 255)
		return src;
	uint8_t src_b = src & 0xff;
	uint8_t src_g = (src >> 8) & 0xff;
	uint8_t src_r = (src >> 16) & 0xff;
	uint8_t dst_b = dst & 0xff;
	uint8_t dst_g = (dst >> 8) & 0xff;
	uint8_t dst_r = (dst >> 16) & 0xff;
	uint8_t dst_a = (dst >> 24) & 0xff;
	uint32_t inv_a = 255 - src_a;
	uint8_t out_b = (uint8_t)((src_b * src_a + dst_b * inv_a + 127) / 255);
	uint8_t out_g = (uint8_t)((src_g * src_a + dst_g * inv_a + 127) / 255);
	uint8_t out_r = (uint8_t)((src_r * src_a + dst_r * inv_a + 127) / 255);
	uint8_t out_a = (uint8_t)(src_a + (dst_a * inv_a + 127) / 255);
	return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
		   ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

static inline uint32_t blend_pixel(uint32_t dst, uint32_t src,
								   uint32_t src_factor,
								   uint32_t dst_factor) {
	if (src_factor == GL_SRC_ALPHA && dst_factor == GL_ONE_MINUS_SRC_ALPHA)
		return blend_src_alpha_over(dst, src);
	if (src_factor == GL_ONE && dst_factor == GL_ZERO)
		return src;
	if (src_factor == GL_ZERO && dst_factor == GL_ONE)
		return dst;

	uint8_t src_b = src & 0xff;
	uint8_t src_g = (src >> 8) & 0xff;
	uint8_t src_r = (src >> 16) & 0xff;
	uint8_t src_a = (src >> 24) & 0xff;
	uint8_t dst_b = dst & 0xff;
	uint8_t dst_g = (dst >> 8) & 0xff;
	uint8_t dst_r = (dst >> 16) & 0xff;
	uint8_t dst_a = (dst >> 24) & 0xff;
	float sf = src_factor == GL_ONE	   ? 1.f
			   : src_factor == GL_ZERO ? 0.f
			   : src_factor == GL_SRC_ALPHA
				   ? (float)src_a / 255.f
				   : 1.f - (float)src_a / 255.f;
	float df = dst_factor == GL_ONE	   ? 1.f
			   : dst_factor == GL_ZERO ? 0.f
			   : dst_factor == GL_SRC_ALPHA
				   ? (float)src_a / 255.f
				   : 1.f - (float)src_a / 255.f;
	uint8_t out_b = (uint8_t)clamp_int((int)(src_b * sf + dst_b * df + 0.5f),
									   0, 255);
	uint8_t out_g = (uint8_t)clamp_int((int)(src_g * sf + dst_g * df + 0.5f),
									   0, 255);
	uint8_t out_r = (uint8_t)clamp_int((int)(src_r * sf + dst_r * df + 0.5f),
									   0, 255);
	uint8_t out_a = (uint8_t)clamp_int((int)(src_a * sf + dst_a * df + 0.5f),
									   0, 255);
	return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
		   ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

static bool assign_axis_aligned_quad_corners(nx_webgl_vec2_t *p,
											 nx_webgl_vec2_t *uv,
											 nx_webgl_vec2_t *corner_uv,
											 float *min_x_out,
											 float *max_x_out,
											 float *min_y_out,
											 float *max_y_out) {
	float min_x = fminf(fminf(p[0].x, p[1].x), fminf(p[2].x, p[3].x));
	float max_x = fmaxf(fmaxf(p[0].x, p[1].x), fmaxf(p[2].x, p[3].x));
	float min_y = fminf(fminf(p[0].y, p[1].y), fminf(p[2].y, p[3].y));
	float max_y = fmaxf(fmaxf(p[0].y, p[1].y), fmaxf(p[2].y, p[3].y));
	if (max_x - min_x < 1.f || max_y - min_y < 1.f)
		return false;

	bool assigned[4] = {false, false, false, false};
	for (int i = 0; i < 4; i++) {
		bool left = fabsf(p[i].x - min_x) <= 1.f;
		bool right = fabsf(p[i].x - max_x) <= 1.f;
		bool top = fabsf(p[i].y - min_y) <= 1.f;
		bool bottom = fabsf(p[i].y - max_y) <= 1.f;
		int corner = -1;
		if (left && top)
			corner = 0;
		else if (right && top)
			corner = 1;
		else if (right && bottom)
			corner = 2;
		else if (left && bottom)
			corner = 3;
		if (corner < 0 || assigned[corner])
			return false;
		assigned[corner] = true;
		corner_uv[corner] = uv[i];
	}

	*min_x_out = min_x;
	*max_x_out = max_x;
	*min_y_out = min_y;
	*max_y_out = max_y;
	return true;
}

static bool load_indexed_quad(nx_webgl_context_t *context,
							  nx_webgl_program_t *program,
							  nx_webgl_vertex_attrib_t *position,
							  nx_webgl_vertex_attrib_t *texcoord,
							  uint16_t *indices, int quad_offset,
							  nx_webgl_vec2_t *p, nx_webgl_vec2_t *uv) {
	uint16_t i0 = indices[quad_offset + 0];
	uint16_t i1 = indices[quad_offset + 1];
	uint16_t i2 = indices[quad_offset + 2];
	uint16_t i3 = indices[quad_offset + 5];
	if (indices[quad_offset + 3] != i0 || indices[quad_offset + 4] != i2)
		return false;

	uint16_t vertex_indices[4] = {i0, i1, i2, i3};
	for (int i = 0; i < 4; i++) {
		nx_webgl_vec2_t clip;
		if (!read_attrib_vec2(context, position, vertex_indices[i], &clip) ||
			!read_attrib_vec2(context, texcoord, vertex_indices[i], &uv[i]))
			return false;
		clip = transform_position(program, clip);
		p[i] = clip_to_pixel(context, clip);
	}
	return true;
}

static void rasterize_axis_aligned_textured_quad(
	nx_canvas_t *canvas, nx_webgl_texture_t *texture, nx_webgl_vec2_t *p,
	nx_webgl_vec2_t *uv, bool blend, uint32_t blend_src, uint32_t blend_dst) {
	nx_webgl_vec2_t corner_uv[4];
	float min_xf;
	float max_xf;
	float min_yf;
	float max_yf;
	if (!assign_axis_aligned_quad_corners(p, uv, corner_uv, &min_xf, &max_xf,
										  &min_yf, &max_yf))
		return;

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf) - 1, 0,
						  (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf) - 1, 0,
						  (int)canvas->height - 1);
	if (max_x < min_x || max_y < min_y)
		return;

	float width = max_xf - min_xf;
	float height = max_yf - min_yf;
	float inv_width = 1.f / width;
	float inv_height = 1.f / height;
	uint32_t *pixels = (uint32_t *)canvas->data;
	bool src_over = blend && blend_src == GL_SRC_ALPHA &&
					blend_dst == GL_ONE_MINUS_SRC_ALPHA;
	bool replace = !blend || (blend_src == GL_ONE && blend_dst == GL_ZERO);
	bool clamp_edge = texture->wrap_s == GL_CLAMP_TO_EDGE &&
					  texture->wrap_t == GL_CLAMP_TO_EDGE;

	for (int y = min_y; y <= max_y; y++) {
		float ty = ((float)y + 0.5f - min_yf) * inv_height;
		if (ty < 0.f)
			ty = 0.f;
		else if (ty > 1.f)
			ty = 1.f;
		nx_webgl_vec2_t left_uv = {
			corner_uv[0].x + (corner_uv[3].x - corner_uv[0].x) * ty,
			corner_uv[0].y + (corner_uv[3].y - corner_uv[0].y) * ty,
		};
		nx_webgl_vec2_t right_uv = {
			corner_uv[1].x + (corner_uv[2].x - corner_uv[1].x) * ty,
			corner_uv[1].y + (corner_uv[2].y - corner_uv[1].y) * ty,
		};

		int row_min_x = min_x;
		int row_max_x = max_x;
		bool row_constant_v = fabsf(left_uv.y - right_uv.y) <= 0.0001f;
		if (src_over && row_constant_v && texture->alpha_min_x &&
			texture->alpha_max_x) {
			int tex_y = texture_coord_to_int(left_uv.y, texture->wrap_t,
											 texture->height);
			int alpha_min = texture->alpha_min_x[tex_y];
			int alpha_max = texture->alpha_max_x[tex_y];
			if (alpha_max < alpha_min)
				continue;
			float u_span = right_uv.x - left_uv.x;
			if (texture->wrap_s == GL_CLAMP_TO_EDGE && fabsf(u_span) > 0.0001f) {
				float alpha_min_u = (float)alpha_min / (float)texture->width;
				float alpha_max_u =
					(float)(alpha_max + 1) / (float)texture->width;
				float x0 = min_xf + (alpha_min_u - left_uv.x) / u_span * width;
				float x1 = min_xf + (alpha_max_u - left_uv.x) / u_span * width;
				if (x1 < x0) {
					float tmp = x0;
					x0 = x1;
					x1 = tmp;
				}
				int clip_min_x = (int)floorf(x0);
				int clip_max_x = (int)ceilf(x1) - 1;
				if (clip_max_x < row_min_x || clip_min_x > row_max_x)
					continue;
				row_min_x = clamp_int(clip_min_x, row_min_x, row_max_x);
				row_max_x = clamp_int(clip_max_x, row_min_x, row_max_x);
			}
		}

		uint32_t *row_pixels = pixels + (size_t)y * canvas->width;
		float u_span = right_uv.x - left_uv.x;
		float v_span = right_uv.y - left_uv.y;
		float tx = ((float)row_min_x + 0.5f - min_xf) * inv_width;
		if (tx < 0.f)
			tx = 0.f;
		else if (tx > 1.f)
			tx = 1.f;
		float u = left_uv.x + u_span * tx;
		float v = left_uv.y + v_span * tx;
		float u_step = u_span * inv_width;
		float v_step = v_span * inv_width;
		for (int x = row_min_x; x <= row_max_x; x++) {
			uint32_t src = clamp_edge
							   ? sample_texture_clamp_edge_fast(texture, u, v)
							   : sample_texture_fast(texture, u, v);
			u += u_step;
			v += v_step;
			if (replace) {
				row_pixels[x] = src;
			} else if (src_over) {
				uint32_t alpha = src >> 24;
				if (alpha == 0)
					continue;
				if (alpha == 255)
					row_pixels[x] = src;
				else
					row_pixels[x] = blend_src_alpha_over(row_pixels[x], src);
			} else {
				row_pixels[x] =
					blend_pixel(row_pixels[x], src, blend_src, blend_dst);
			}
		}
	}
}

static bool draw_axis_aligned_textured_quads(
	JSContext *ctx, nx_webgl_context_t *context, nx_webgl_program_t *program,
	nx_webgl_vertex_attrib_t *position, nx_webgl_vertex_attrib_t *texcoord,
	nx_webgl_texture_t *texture, uint16_t *indices, int count, bool blend) {
	if (count % 6 != 0)
		return false;

	int quad_count = count / 6;
	nx_webgl_textured_quad_t stack_quads[128];
	nx_webgl_textured_quad_t *quads = stack_quads;
	if (quad_count > (int)countof(stack_quads)) {
		quads = js_malloc(ctx,
						  sizeof(nx_webgl_textured_quad_t) * (size_t)quad_count);
		if (!quads)
			return false;
	}

	for (int quad_index = 0; quad_index < quad_count; quad_index++) {
		nx_webgl_textured_quad_t *quad = &quads[quad_index];
		nx_webgl_vec2_t corner_uv[4];
		float min_x;
		float max_x;
		float min_y;
		float max_y;
		if (!load_indexed_quad(context, program, position, texcoord, indices,
							   quad_index * 6, quad->p, quad->uv) ||
			!assign_axis_aligned_quad_corners(quad->p, quad->uv, corner_uv,
											  &min_x, &max_x, &min_y, &max_y)) {
			if (quads != stack_quads)
				js_free(ctx, quads);
			return false;
		}
	}

	for (int quad_index = 0; quad_index < quad_count; quad_index++) {
		nx_webgl_textured_quad_t *quad = &quads[quad_index];
		rasterize_axis_aligned_textured_quad(
			context->canvas, texture, quad->p, quad->uv, blend, context->blend_src,
			context->blend_dst);
	}

	if (quads != stack_quads)
		js_free(ctx, quads);
	return true;
}

static bool draw_indexed_textured_quads_bridge(
	JSContext *ctx, nx_webgl_context_t *context, nx_webgl_program_t *program,
	nx_webgl_vertex_attrib_t *position, nx_webgl_vertex_attrib_t *texcoord,
	nx_webgl_texture_t *texture, uint16_t *indices, int count, bool blend) {
	if (!nx_webgl_egl_is_bridge_enabled(context->egl) || !texture ||
		!texture->data || texture->deleted || texture->revision == 0 ||
		texture->bridge_id == 0 || count <= 0 || count % 6 != 0)
		return false;

	int quad_count = count / 6;
	int vertex_count = quad_count * 6;
	float *clip_uv =
		js_malloc(ctx, (size_t)vertex_count * 4 * sizeof(float));
	if (!clip_uv)
		return false;

	bool loaded = true;
	for (int quad = 0; quad < quad_count; quad++) {
		int offset = quad * 6;
		uint16_t i0 = indices[offset + 0];
		uint16_t i1 = indices[offset + 1];
		uint16_t i2 = indices[offset + 2];
		uint16_t i3 = indices[offset + 5];
		if (indices[offset + 3] != i0 || indices[offset + 4] != i2) {
			loaded = false;
			break;
		}
		uint16_t expanded[6] = {i0, i1, i2, i0, i2, i3};
		for (int i = 0; i < 6; i++) {
			nx_webgl_vec2_t clip;
			nx_webgl_vec2_t uv;
			if (!read_attrib_vec2(context, position, expanded[i], &clip) ||
				!read_attrib_vec2(context, texcoord, expanded[i], &uv)) {
				loaded = false;
				break;
			}
			clip = transform_position(program, clip);
			int out = (quad * 6 + i) * 4;
			clip_uv[out + 0] = clip.x;
			clip_uv[out + 1] = clip.y;
			clip_uv[out + 2] = uv.x;
			clip_uv[out + 3] = uv.y;
		}
		if (!loaded)
			break;
	}

	bool drew = false;
	if (loaded) {
		drew = nx_webgl_egl_draw_textured_triangles_bridge(
			context->egl, context->canvas, clip_uv, vertex_count,
			texture->bridge_id, texture->revision, (int)texture->width,
			(int)texture->height, texture->data, texture->min_filter,
			texture->mag_filter, texture->wrap_s, texture->wrap_t, blend,
			context->blend_src, context->blend_dst);
	}
	js_free(ctx, clip_uv);
	return drew;
}

static void rasterize_triangle(nx_canvas_t *canvas, nx_webgl_vec2_t v0,
							   nx_webgl_vec2_t v1, nx_webgl_vec2_t v2,
							   uint32_t color, bool blend,
							   uint32_t blend_src, uint32_t blend_dst) {
	float area = edge_function(v0, v1, v2);
	if (area == 0.f)
		return;

	float min_xf = fminf(v0.x, fminf(v1.x, v2.x));
	float max_xf = fmaxf(v0.x, fmaxf(v1.x, v2.x));
	float min_yf = fminf(v0.y, fminf(v1.y, v2.y));
	float max_yf = fmaxf(v0.y, fmaxf(v1.y, v2.y));

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf), 0, (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf), 0, (int)canvas->height - 1);
	uint32_t *pixels = (uint32_t *)canvas->data;

	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			nx_webgl_vec2_t p = {(float)x + 0.5f, (float)y + 0.5f};
			float w0 = edge_function(v1, v2, p);
			float w1 = edge_function(v2, v0, p);
			float w2 = edge_function(v0, v1, p);
			if ((w0 >= 0.f && w1 >= 0.f && w2 >= 0.f) ||
				(w0 <= 0.f && w1 <= 0.f && w2 <= 0.f)) {
				size_t pixel_index = (size_t)y * canvas->width + x;
				pixels[pixel_index] = blend
										  ? blend_pixel(pixels[pixel_index],
														color, blend_src,
														blend_dst)
										  : color;
			}
		}
	}
}

static void rasterize_triangle_textured(nx_canvas_t *canvas,
										nx_webgl_vec2_t v0,
										nx_webgl_vec2_t v1,
										nx_webgl_vec2_t v2,
										nx_webgl_vec2_t uv0,
										nx_webgl_vec2_t uv1,
										nx_webgl_vec2_t uv2,
										nx_webgl_texture_t *texture,
										bool blend, uint32_t blend_src,
										uint32_t blend_dst) {
	float area = edge_function(v0, v1, v2);
	if (area == 0.f)
		return;
	float inv_area = 1.f / area;

	float min_xf = fminf(v0.x, fminf(v1.x, v2.x));
	float max_xf = fmaxf(v0.x, fmaxf(v1.x, v2.x));
	float min_yf = fminf(v0.y, fminf(v1.y, v2.y));
	float max_yf = fmaxf(v0.y, fmaxf(v1.y, v2.y));

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf), 0, (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf), 0, (int)canvas->height - 1);
	uint32_t *pixels = (uint32_t *)canvas->data;
	bool src_over = blend && blend_src == GL_SRC_ALPHA &&
					blend_dst == GL_ONE_MINUS_SRC_ALPHA;
	bool replace = !blend || (blend_src == GL_ONE && blend_dst == GL_ZERO);
	bool clamp_edge = texture->wrap_s == GL_CLAMP_TO_EDGE &&
					  texture->wrap_t == GL_CLAMP_TO_EDGE;

	float w0_dx = v2.y - v1.y;
	float w1_dx = v0.y - v2.y;
	float w2_dx = v1.y - v0.y;

	float u_dx = (uv0.x * w0_dx + uv1.x * w1_dx + uv2.x * w2_dx) *
				 inv_area;
	float v_dx = (uv0.y * w0_dx + uv1.y * w1_dx + uv2.y * w2_dx) *
				 inv_area;

	for (int y = min_y; y <= max_y; y++) {
		float scan_y = (float)y + 0.5f;
		float span_min_x;
		float span_max_x;
		if (!triangle_scanline_span(v0, v1, v2, scan_y, &span_min_x,
									&span_max_x))
			continue;

		int row_min_x =
			clamp_int((int)ceilf(span_min_x - 0.5f), min_x, max_x);
		int row_max_x =
			clamp_int((int)floorf(span_max_x - 0.5f), min_x, max_x);
		if (row_max_x < row_min_x)
			continue;

		nx_webgl_vec2_t row_start = {(float)row_min_x + 0.5f, scan_y};
		float row_w0 = edge_function(v1, v2, row_start);
		float row_w1 = edge_function(v2, v0, row_start);
		float row_w2 = edge_function(v0, v1, row_start);
		float row_u = (uv0.x * row_w0 + uv1.x * row_w1 + uv2.x * row_w2) *
					  inv_area;
		float row_v = (uv0.y * row_w0 + uv1.y * row_w1 + uv2.y * row_w2) *
					  inv_area;
		uint32_t *row_pixels = pixels + (size_t)y * canvas->width;

		float u = row_u;
		float v = row_v;
		for (int x = row_min_x; x <= row_max_x; x++) {
			uint32_t src =
				clamp_edge ? sample_texture_clamp_edge_fast(texture, u, v)
						   : sample_texture_fast(texture, u, v);
			if (replace) {
				row_pixels[x] = src;
			} else if (src_over) {
				uint32_t alpha = src >> 24;
				if (alpha == 0)
					goto next_pixel;
				if (alpha == 255) {
					row_pixels[x] = src;
				} else {
					row_pixels[x] = blend_src_alpha_over(row_pixels[x], src);
				}
			} else {
				row_pixels[x] =
					blend_pixel(row_pixels[x], src, blend_src, blend_dst);
			}
next_pixel:
			u += u_dx;
			v += v_dx;
		}
	}
}

static JSValue nx_webgl_draw_arrays(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t first;
	int32_t count;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &first, argv[1]) ||
		JS_ToInt32(ctx, &count, argv[2]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (first < 0 || count < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_vertex_attrib_t *position = &context->vertex_attribs[0];
	if (!position->enabled || position->type != GL_FLOAT || position->size < 2) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	nx_webgl_texture_t *texture = active_texture_for_program(context, program);
	nx_webgl_vertex_attrib_t *texcoord = &context->vertex_attribs[2];
	bool use_texture = texture && texcoord->enabled && texcoord->type == GL_FLOAT &&
					   texcoord->size >= 2;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint32_t color = program_color(program);
	int triangle_count = count / 3;
	if (triangle_count == 0)
		return JS_UNDEFINED;

	if (nx_webgl_egl_is_bridge_enabled(context->egl) && !use_texture) {
		int vertex_count = triangle_count * 3;
		float *clip_xy =
			js_malloc(ctx, (size_t)vertex_count * 2 * sizeof(float));
		if (clip_xy) {
			bool loaded = true;
			for (int i = 0; i < vertex_count; i++) {
				int vertex_index = first + i;
				nx_webgl_vec2_t clip;
				if (!read_attrib_vec2(context, position, vertex_index, &clip)) {
					loaded = false;
					break;
				}
				clip = transform_position(program, clip);
				clip_xy[i * 2 + 0] = clip.x;
				clip_xy[i * 2 + 1] = clip.y;
			}
			if (!loaded) {
				js_free(ctx, clip_xy);
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			float fallback_color[4] = {
				68.f / 255.f,
				215.f / 255.f,
				182.f / 255.f,
				1.f,
			};
			float *gpu_color = program->has_color ? program->color
												  : fallback_color;
			bool drew = nx_webgl_egl_draw_triangles_bridge(
				context->egl, canvas, clip_xy, vertex_count, gpu_color, blend,
				context->blend_src, context->blend_dst);
			js_free(ctx, clip_xy);
			if (drew) {
				context->bridge_clear_pending = false;
				return JS_UNDEFINED;
			}
		}
	}

	flush_pending_bridge_clear_to_software(context);

	for (int triangle = 0; triangle < triangle_count; triangle++) {
		nx_webgl_vec2_t clip[3];
		nx_webgl_vec2_t uv[3];
		for (int i = 0; i < 3; i++) {
			int vertex_index = first + triangle * 3 + i;
			if (!read_attrib_vec2(context, position, vertex_index, &clip[i])) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			if (use_texture &&
				!read_attrib_vec2(context, texcoord, vertex_index, &uv[i])) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			clip[i] = transform_position(program, clip[i]);
		}

		nx_webgl_vec2_t p0 = clip_to_pixel(context, clip[0]);
		nx_webgl_vec2_t p1 = clip_to_pixel(context, clip[1]);
		nx_webgl_vec2_t p2 = clip_to_pixel(context, clip[2]);
		if (use_texture)
			rasterize_triangle_textured(canvas, p0, p1, p2, uv[0], uv[1],
										uv[2], texture, blend,
										context->blend_src,
										context->blend_dst);
		else
			rasterize_triangle(canvas, p0, p1, p2, color, blend,
							   context->blend_src, context->blend_dst);
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);

	return JS_UNDEFINED;
}

static JSValue nx_webgl_draw_elements(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t count;
	uint32_t type;
	int32_t offset;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &count, argv[1]) ||
		JS_ToUint32(ctx, &type, argv[2]) || JS_ToInt32(ctx, &offset, argv[3]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (type != GL_UNSIGNED_SHORT) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (count < 0 || offset < 0 || offset % (int)sizeof(uint16_t) != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_vertex_attrib_t *position = &context->vertex_attribs[0];
	if (!position->enabled || position->type != GL_FLOAT || position->size < 2) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *element_buffer =
		nx_get_webgl_buffer(context->element_array_buffer_binding);
	if (!element_buffer || element_buffer->deleted || !element_buffer->data) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if ((size_t)offset + (size_t)count * sizeof(uint16_t) >
		element_buffer->size) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	nx_webgl_texture_t *texture = active_texture_for_program(context, program);
	nx_webgl_vertex_attrib_t *texcoord = &context->vertex_attribs[2];
	bool use_texture = texture && texcoord->enabled && texcoord->type == GL_FLOAT &&
					   texcoord->size >= 2;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint16_t *indices = (uint16_t *)(element_buffer->data + offset);
	uint32_t color = program_color(program);
	if (use_texture &&
		draw_indexed_textured_quads_bridge(ctx, context, program, position,
										   texcoord, texture, indices, count,
										   blend)) {
		context->bridge_clear_pending = false;
		return JS_UNDEFINED;
	}
	flush_pending_bridge_clear_to_software(context);
	if (use_texture &&
		draw_axis_aligned_textured_quads(ctx, context, program, position, texcoord,
										 texture, indices, count, blend)) {
		if (canvas->surface)
			cairo_surface_mark_dirty(canvas->surface);
		return JS_UNDEFINED;
	}

	int triangle_count = count / 3;
	for (int triangle = 0; triangle < triangle_count; triangle++) {
		nx_webgl_vec2_t clip[3];
		nx_webgl_vec2_t uv[3];
		for (int i = 0; i < 3; i++) {
			int vertex_index = indices[triangle * 3 + i];
			if (!read_attrib_vec2(context, position, vertex_index, &clip[i])) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			if (use_texture &&
				!read_attrib_vec2(context, texcoord, vertex_index, &uv[i])) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			clip[i] = transform_position(program, clip[i]);
		}

		nx_webgl_vec2_t p0 = clip_to_pixel(context, clip[0]);
		nx_webgl_vec2_t p1 = clip_to_pixel(context, clip[1]);
		nx_webgl_vec2_t p2 = clip_to_pixel(context, clip[2]);
		if (use_texture)
			rasterize_triangle_textured(canvas, p0, p1, p2, uv[0], uv[1],
										uv[2], texture, blend,
										context->blend_src,
										context->blend_dst);
		else
			rasterize_triangle(canvas, p0, p1, p2, color, blend,
							   context->blend_src, context->blend_dst);
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);

	return JS_UNDEFINED;
}

static JSValue nx_webgl_enable_disable(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv,
									   bool enabled) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t cap;
	if (JS_ToUint32(ctx, &cap, argv[0]))
		return JS_EXCEPTION;

	uint32_t flag = cap_to_flag(cap);
	if (flag == 0) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (enabled)
		context->enabled_caps |= flag;
	else
		context->enabled_caps &= ~flag;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_enable(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	return nx_webgl_enable_disable(ctx, this_val, argc, argv, true);
}

static JSValue nx_webgl_disable(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	return nx_webgl_enable_disable(ctx, this_val, argc, argv, false);
}

static JSValue nx_webgl_depth_func(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t func;
	if (JS_ToUint32(ctx, &func, argv[0]))
		return JS_EXCEPTION;

	if (!is_depth_func(func)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	context->depth_func = func;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_blend_func(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t src;
	uint32_t dst;
	if (JS_ToUint32(ctx, &src, argv[0]) || JS_ToUint32(ctx, &dst, argv[1]))
		return JS_EXCEPTION;
	if (!is_blend_factor(src) || !is_blend_factor(dst)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	context->blend_src = src;
	context->blend_dst = dst;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_viewport(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	int32_t values[4];
	for (int i = 0; i < 4; i++) {
		if (JS_ToInt32(ctx, &values[i], argv[i]))
			return JS_EXCEPTION;
	}

	if (values[2] < 0 || values[3] < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	for (int i = 0; i < 4; i++) {
		context->viewport[i] = values[i];
	}

	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_parameter(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[0]))
		return JS_EXCEPTION;

	uint32_t cap_flag = cap_to_flag(pname);
	if (cap_flag != 0)
		return JS_NewBool(ctx, (context->enabled_caps & cap_flag) != 0);

	switch (pname) {
	case GL_COLOR_CLEAR_VALUE:
		return new_number_array(ctx, context->clear_color, 4);
	case GL_DEPTH_CLEAR_VALUE:
		return JS_NewFloat64(ctx, context->clear_depth);
	case GL_DEPTH_FUNC:
		return JS_NewUint32(ctx, context->depth_func);
	case GL_BLEND_SRC_RGB:
	case GL_BLEND_SRC_ALPHA:
		return JS_NewUint32(ctx, context->blend_src);
	case GL_BLEND_DST_RGB:
	case GL_BLEND_DST_ALPHA:
		return JS_NewUint32(ctx, context->blend_dst);
	case GL_VIEWPORT:
		return new_int_array(ctx, context->viewport, 4);
	case GL_VENDOR:
		return JS_NewString(ctx, "nx.js");
	case GL_RENDERER:
		return JS_NewString(ctx, "nx.js framebuffer WebGL skeleton");
	case GL_VERSION:
		return JS_NewString(ctx, "WebGL 1.0 (nx.js experimental)");
	case GL_SHADING_LANGUAGE_VERSION:
		return JS_NewString(ctx, "WebGL GLSL ES 1.0 (nx.js experimental)");
	case GL_CURRENT_PROGRAM:
		if (JS_IsUndefined(context->current_program))
			return JS_NULL;
		return JS_DupValue(ctx, context->current_program);
	case GL_ARRAY_BUFFER_BINDING:
		if (JS_IsUndefined(context->array_buffer_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->array_buffer_binding);
	case GL_ELEMENT_ARRAY_BUFFER_BINDING:
		if (JS_IsUndefined(context->element_array_buffer_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->element_array_buffer_binding);
	case GL_TEXTURE_BINDING_2D:
		if (JS_IsUndefined(context->texture_2d_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->texture_2d_binding);
	case GL_ACTIVE_TEXTURE:
		return JS_NewUint32(ctx, context->active_texture);
	case GL_MAX_TEXTURE_SIZE:
	case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
	case GL_MAX_RENDERBUFFER_SIZE:
		return JS_NewUint32(ctx, 4096);
	case GL_MAX_VIEWPORT_DIMS: {
		int dims[2] = {4096, 4096};
		return new_int_array(ctx, dims, 2);
	}
	case GL_MAX_VERTEX_ATTRIBS:
		return JS_NewUint32(ctx, 8);
	case GL_MAX_TEXTURE_IMAGE_UNITS:
	case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
		return JS_NewUint32(ctx, 8);
	case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
		return JS_NewUint32(ctx, 0);
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_error(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t error = context->error;
	context->error = GL_NO_ERROR;
	return JS_NewUint32(ctx, error);
}

static JSValue nx_webgl_get_backend_info(JSContext *ctx,
										 JSValueConst this_val, int argc,
										 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_get_backend_info(ctx, context->egl);
}

static JSValue nx_webgl_clear_gpu_prototype(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewBool(
		ctx, nx_webgl_egl_clear_prototype(context->egl, context->canvas));
}

static JSValue nx_webgl_probe_gpu_prototype_step(JSContext *ctx,
												 JSValueConst this_val,
												 int argc,
												 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewBool(
		ctx, nx_webgl_egl_probe_step(context->egl, context->canvas));
}

static JSValue nx_webgl_triangle_gpu_prototype(JSContext *ctx,
											   JSValueConst this_val,
											   int argc,
											   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_triangle_readback(ctx, context->egl, context->canvas);
}

static JSValue nx_webgl_bridge_gpu_prototype(JSContext *ctx,
											 JSValueConst this_val,
											 int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_bridge_framebuffer(ctx, context->egl, context->canvas);
}

static JSValue nx_webgl_bridge_gpu_benchmark_prototype(JSContext *ctx,
													   JSValueConst this_val,
													   int argc,
													   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int frame_count = 120;
	int width = 256;
	int height = 144;
	if (argc > 0 && !JS_IsUndefined(argv[0]) &&
		JS_ToInt32(ctx, &frame_count, argv[0]))
		return JS_EXCEPTION;
	if (argc > 1 && !JS_IsUndefined(argv[1]) &&
		JS_ToInt32(ctx, &width, argv[1]))
		return JS_EXCEPTION;
	if (argc > 2 && !JS_IsUndefined(argv[2]) &&
		JS_ToInt32(ctx, &height, argv[2]))
		return JS_EXCEPTION;
	return nx_webgl_egl_bridge_benchmark(ctx, context->egl, context->canvas,
										 frame_count, width, height);
}

static JSValue nx_webgl_enable_gpu_bridge_prototype(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	bool enabled = true;
	if (argc > 0 && !JS_IsUndefined(argv[0]))
		enabled = JS_ToBool(ctx, argv[0]);
	nx_webgl_egl_set_bridge_enabled(context->egl, enabled);
	if (!enabled)
		context->bridge_clear_pending = false;
	return JS_NewBool(ctx, nx_webgl_egl_is_bridge_enabled(context->egl));
}

static JSValue nx_webgl_set_gpu_bridge_resolution_prototype(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int32_t width = 0;
	int32_t height = 0;
	if (argc > 0 && !JS_IsUndefined(argv[0]) &&
		JS_ToInt32(ctx, &width, argv[0]))
		return JS_EXCEPTION;
	if (argc > 1 && !JS_IsUndefined(argv[1]) &&
		JS_ToInt32(ctx, &height, argv[1]))
		return JS_EXCEPTION;
	nx_webgl_egl_set_bridge_resolution(context->egl, width, height);
	context->bridge_clear_pending = false;
	return JS_NewBool(ctx, true);
}

static void define_constant(JSContext *ctx, JSValueConst obj, const char *name,
							int32_t value) {
	JS_DefinePropertyValueStr(ctx, obj, name, JS_NewInt32(ctx, value),
							  JS_PROP_CONFIGURABLE);
}

static JSValue nx_webgl_context_init_class(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");

	NX_DEF_GET(proto, "drawingBufferWidth", nx_webgl_get_drawing_buffer_width);
	NX_DEF_GET(proto, "drawingBufferHeight",
			   nx_webgl_get_drawing_buffer_height);

	NX_DEF_FUNC(proto, "clearColor", nx_webgl_clear_color, 4);
	NX_DEF_FUNC(proto, "clearDepth", nx_webgl_clear_depth, 1);
	NX_DEF_FUNC(proto, "clear", nx_webgl_clear, 1);
	NX_DEF_FUNC(proto, "createShader", nx_webgl_create_shader, 1);
	NX_DEF_FUNC(proto, "shaderSource", nx_webgl_shader_source, 2);
	NX_DEF_FUNC(proto, "compileShader", nx_webgl_compile_shader, 1);
	NX_DEF_FUNC(proto, "getShaderParameter", nx_webgl_get_shader_parameter, 2);
	NX_DEF_FUNC(proto, "getShaderInfoLog", nx_webgl_get_shader_info_log, 1);
	NX_DEF_FUNC(proto, "deleteShader", nx_webgl_delete_shader, 1);
	NX_DEF_FUNC(proto, "createProgram", nx_webgl_create_program, 0);
	NX_DEF_FUNC(proto, "attachShader", nx_webgl_attach_shader, 2);
	NX_DEF_FUNC(proto, "linkProgram", nx_webgl_link_program, 1);
	NX_DEF_FUNC(proto, "useProgram", nx_webgl_use_program, 1);
	NX_DEF_FUNC(proto, "getProgramParameter", nx_webgl_get_program_parameter,
				2);
	NX_DEF_FUNC(proto, "getProgramInfoLog", nx_webgl_get_program_info_log, 1);
	NX_DEF_FUNC(proto, "deleteProgram", nx_webgl_delete_program, 1);
	NX_DEF_FUNC(proto, "createBuffer", nx_webgl_create_buffer, 0);
	NX_DEF_FUNC(proto, "bindBuffer", nx_webgl_bind_buffer, 2);
	NX_DEF_FUNC(proto, "bufferData", nx_webgl_buffer_data, 3);
	NX_DEF_FUNC(proto, "deleteBuffer", nx_webgl_delete_buffer, 1);
	NX_DEF_FUNC(proto, "createTexture", nx_webgl_create_texture, 0);
	NX_DEF_FUNC(proto, "activeTexture", nx_webgl_active_texture, 1);
	NX_DEF_FUNC(proto, "bindTexture", nx_webgl_bind_texture, 2);
	NX_DEF_FUNC(proto, "texImage2D", nx_webgl_tex_image_2d, 9);
	NX_DEF_FUNC(proto, "texParameteri", nx_webgl_tex_parameteri, 3);
	NX_DEF_FUNC(proto, "deleteTexture", nx_webgl_delete_texture, 1);
	NX_DEF_FUNC(proto, "getUniformLocation", nx_webgl_get_uniform_location, 2);
	NX_DEF_FUNC(proto, "uniform2f", nx_webgl_uniform2f, 3);
	NX_DEF_FUNC(proto, "uniform4f", nx_webgl_uniform4f, 5);
	NX_DEF_FUNC(proto, "uniform1i", nx_webgl_uniform1i, 2);
	NX_DEF_FUNC(proto, "uniformMatrix4fv", nx_webgl_uniform_matrix4fv, 3);
	NX_DEF_FUNC(proto, "getAttribLocation", nx_webgl_get_attrib_location, 2);
	NX_DEF_FUNC(proto, "enableVertexAttribArray",
				nx_webgl_enable_vertex_attrib_array, 1);
	NX_DEF_FUNC(proto, "disableVertexAttribArray",
				nx_webgl_disable_vertex_attrib_array, 1);
	NX_DEF_FUNC(proto, "vertexAttribPointer",
				nx_webgl_vertex_attrib_pointer, 6);
	NX_DEF_FUNC(proto, "drawArrays", nx_webgl_draw_arrays, 3);
	NX_DEF_FUNC(proto, "drawElements", nx_webgl_draw_elements, 4);
	NX_DEF_FUNC(proto, "enable", nx_webgl_enable, 1);
	NX_DEF_FUNC(proto, "disable", nx_webgl_disable, 1);
	NX_DEF_FUNC(proto, "depthFunc", nx_webgl_depth_func, 1);
	NX_DEF_FUNC(proto, "blendFunc", nx_webgl_blend_func, 2);
	NX_DEF_FUNC(proto, "viewport", nx_webgl_viewport, 4);
	NX_DEF_FUNC(proto, "getParameter", nx_webgl_get_parameter, 1);
	NX_DEF_FUNC(proto, "getError", nx_webgl_get_error, 0);
	NX_DEF_FUNC(proto, "getBackendInfo", nx_webgl_get_backend_info, 0);
	NX_DEF_FUNC(proto, "clearGpuPrototype", nx_webgl_clear_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "probeGpuPrototypeStep",
				nx_webgl_probe_gpu_prototype_step, 0);
	NX_DEF_FUNC(proto, "triangleGpuPrototype",
				nx_webgl_triangle_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "bridgeGpuPrototype",
				nx_webgl_bridge_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "bridgeGpuBenchmarkPrototype",
				nx_webgl_bridge_gpu_benchmark_prototype, 3);
	NX_DEF_FUNC(proto, "enableGpuBridgePrototype",
				nx_webgl_enable_gpu_bridge_prototype, 1);
	NX_DEF_FUNC(proto, "setGpuBridgeResolutionPrototype",
				nx_webgl_set_gpu_bridge_resolution_prototype, 2);

	define_constant(ctx, proto, "NO_ERROR", GL_NO_ERROR);
	define_constant(ctx, proto, "INVALID_ENUM", GL_INVALID_ENUM);
	define_constant(ctx, proto, "INVALID_VALUE", GL_INVALID_VALUE);
	define_constant(ctx, proto, "INVALID_OPERATION", GL_INVALID_OPERATION);
	define_constant(ctx, proto, "TRIANGLES", GL_TRIANGLES);
	define_constant(ctx, proto, "ZERO", GL_ZERO);
	define_constant(ctx, proto, "ONE", GL_ONE);
	define_constant(ctx, proto, "NEVER", GL_NEVER);
	define_constant(ctx, proto, "LESS", GL_LESS);
	define_constant(ctx, proto, "EQUAL", GL_EQUAL);
	define_constant(ctx, proto, "LEQUAL", GL_LEQUAL);
	define_constant(ctx, proto, "GREATER", GL_GREATER);
	define_constant(ctx, proto, "NOTEQUAL", GL_NOTEQUAL);
	define_constant(ctx, proto, "GEQUAL", GL_GEQUAL);
	define_constant(ctx, proto, "ALWAYS", GL_ALWAYS);
	define_constant(ctx, proto, "DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT);
	define_constant(ctx, proto, "STENCIL_BUFFER_BIT", GL_STENCIL_BUFFER_BIT);
	define_constant(ctx, proto, "COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT);
	define_constant(ctx, proto, "CULL_FACE", GL_CULL_FACE);
	define_constant(ctx, proto, "DEPTH_TEST", GL_DEPTH_TEST);
	define_constant(ctx, proto, "DITHER", GL_DITHER);
	define_constant(ctx, proto, "BLEND", GL_BLEND);
	define_constant(ctx, proto, "BLEND_SRC_RGB", GL_BLEND_SRC_RGB);
	define_constant(ctx, proto, "BLEND_DST_RGB", GL_BLEND_DST_RGB);
	define_constant(ctx, proto, "BLEND_SRC_ALPHA", GL_BLEND_SRC_ALPHA);
	define_constant(ctx, proto, "BLEND_DST_ALPHA", GL_BLEND_DST_ALPHA);
	define_constant(ctx, proto, "SCISSOR_TEST", GL_SCISSOR_TEST);
	define_constant(ctx, proto, "STENCIL_TEST", GL_STENCIL_TEST);
	define_constant(ctx, proto, "VIEWPORT", GL_VIEWPORT);
	define_constant(ctx, proto, "DEPTH_CLEAR_VALUE", GL_DEPTH_CLEAR_VALUE);
	define_constant(ctx, proto, "DEPTH_FUNC", GL_DEPTH_FUNC);
	define_constant(ctx, proto, "COLOR_CLEAR_VALUE", GL_COLOR_CLEAR_VALUE);
	define_constant(ctx, proto, "VENDOR", GL_VENDOR);
	define_constant(ctx, proto, "RENDERER", GL_RENDERER);
	define_constant(ctx, proto, "VERSION", GL_VERSION);
	define_constant(ctx, proto, "VERTEX_SHADER", GL_VERTEX_SHADER);
	define_constant(ctx, proto, "FRAGMENT_SHADER", GL_FRAGMENT_SHADER);
	define_constant(ctx, proto, "COMPILE_STATUS", GL_COMPILE_STATUS);
	define_constant(ctx, proto, "LINK_STATUS", GL_LINK_STATUS);
	define_constant(ctx, proto, "DELETE_STATUS", GL_DELETE_STATUS);
	define_constant(ctx, proto, "SHADER_TYPE", GL_SHADER_TYPE);
	define_constant(ctx, proto, "ATTACHED_SHADERS", GL_ATTACHED_SHADERS);
	define_constant(ctx, proto, "CURRENT_PROGRAM", GL_CURRENT_PROGRAM);
	define_constant(ctx, proto, "ACTIVE_UNIFORMS", GL_ACTIVE_UNIFORMS);
	define_constant(ctx, proto, "BYTE", GL_BYTE);
	define_constant(ctx, proto, "UNSIGNED_BYTE", GL_UNSIGNED_BYTE);
	define_constant(ctx, proto, "SHORT", GL_SHORT);
	define_constant(ctx, proto, "UNSIGNED_SHORT", GL_UNSIGNED_SHORT);
	define_constant(ctx, proto, "INT", GL_INT);
	define_constant(ctx, proto, "UNSIGNED_INT", GL_UNSIGNED_INT);
	define_constant(ctx, proto, "FLOAT", GL_FLOAT);
	define_constant(ctx, proto, "ARRAY_BUFFER", GL_ARRAY_BUFFER);
	define_constant(ctx, proto, "ARRAY_BUFFER_BINDING", GL_ARRAY_BUFFER_BINDING);
	define_constant(ctx, proto, "ELEMENT_ARRAY_BUFFER", GL_ELEMENT_ARRAY_BUFFER);
	define_constant(ctx, proto, "ELEMENT_ARRAY_BUFFER_BINDING",
					GL_ELEMENT_ARRAY_BUFFER_BINDING);
	define_constant(ctx, proto, "BUFFER_SIZE", GL_BUFFER_SIZE);
	define_constant(ctx, proto, "BUFFER_USAGE", GL_BUFFER_USAGE);
	define_constant(ctx, proto, "STREAM_DRAW", GL_STREAM_DRAW);
	define_constant(ctx, proto, "STATIC_DRAW", GL_STATIC_DRAW);
	define_constant(ctx, proto, "DYNAMIC_DRAW", GL_DYNAMIC_DRAW);
	define_constant(ctx, proto, "TEXTURE_2D", GL_TEXTURE_2D);
	define_constant(ctx, proto, "TEXTURE_BINDING_2D", GL_TEXTURE_BINDING_2D);
	define_constant(ctx, proto, "TEXTURE0", GL_TEXTURE0);
	define_constant(ctx, proto, "ACTIVE_TEXTURE", GL_ACTIVE_TEXTURE);
	define_constant(ctx, proto, "TEXTURE_MIN_FILTER", GL_TEXTURE_MIN_FILTER);
	define_constant(ctx, proto, "TEXTURE_MAG_FILTER", GL_TEXTURE_MAG_FILTER);
	define_constant(ctx, proto, "TEXTURE_WRAP_S", GL_TEXTURE_WRAP_S);
	define_constant(ctx, proto, "TEXTURE_WRAP_T", GL_TEXTURE_WRAP_T);
	define_constant(ctx, proto, "NEAREST", GL_NEAREST);
	define_constant(ctx, proto, "LINEAR", GL_LINEAR);
	define_constant(ctx, proto, "CLAMP_TO_EDGE", GL_CLAMP_TO_EDGE);
	define_constant(ctx, proto, "REPEAT", GL_REPEAT);
	define_constant(ctx, proto, "RGBA", GL_RGBA);
	define_constant(ctx, proto, "SRC_ALPHA", GL_SRC_ALPHA);
	define_constant(ctx, proto, "ONE_MINUS_SRC_ALPHA",
					GL_ONE_MINUS_SRC_ALPHA);
	define_constant(ctx, proto, "MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
	define_constant(ctx, proto, "MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS);
	define_constant(ctx, proto, "MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS);
	define_constant(ctx, proto, "MAX_TEXTURE_IMAGE_UNITS",
					GL_MAX_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_VERTEX_TEXTURE_IMAGE_UNITS",
					GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_COMBINED_TEXTURE_IMAGE_UNITS",
					GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_CUBE_MAP_TEXTURE_SIZE",
					GL_MAX_CUBE_MAP_TEXTURE_SIZE);
	define_constant(ctx, proto, "MAX_RENDERBUFFER_SIZE",
					GL_MAX_RENDERBUFFER_SIZE);
	define_constant(ctx, proto, "SHADING_LANGUAGE_VERSION",
					GL_SHADING_LANGUAGE_VERSION);
	define_constant(ctx, proto, "POLYGON_OFFSET_FILL", GL_POLYGON_OFFSET_FILL);
	define_constant(ctx, proto, "SAMPLE_ALPHA_TO_COVERAGE",
					GL_SAMPLE_ALPHA_TO_COVERAGE);
	define_constant(ctx, proto, "SAMPLE_COVERAGE", GL_SAMPLE_COVERAGE);

	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry init_function_list[] = {
	JS_CFUNC_DEF("webglContextNew", 0, nx_webgl_context_new),
	JS_CFUNC_DEF("webglContextInitClass", 0, nx_webgl_context_init_class),
};

void nx_init_webgl(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_webgl_context_class_id);
	JSClassDef webgl_context_class = {
		"nx_webgl_context_t",
		.finalizer = finalizer_webgl_context,
	};
	JS_NewClass(rt, nx_webgl_context_class_id, &webgl_context_class);

	JS_NewClassID(rt, &nx_webgl_shader_class_id);
	JSClassDef webgl_shader_class = {
		"nx_webgl_shader_t",
		.finalizer = finalizer_webgl_shader,
	};
	JS_NewClass(rt, nx_webgl_shader_class_id, &webgl_shader_class);

	JS_NewClassID(rt, &nx_webgl_program_class_id);
	JSClassDef webgl_program_class = {
		"nx_webgl_program_t",
		.finalizer = finalizer_webgl_program,
	};
	JS_NewClass(rt, nx_webgl_program_class_id, &webgl_program_class);

	JS_NewClassID(rt, &nx_webgl_buffer_class_id);
	JSClassDef webgl_buffer_class = {
		"nx_webgl_buffer_t",
		.finalizer = finalizer_webgl_buffer,
	};
	JS_NewClass(rt, nx_webgl_buffer_class_id, &webgl_buffer_class);

	JS_NewClassID(rt, &nx_webgl_uniform_location_class_id);
	JSClassDef webgl_uniform_location_class = {
		"nx_webgl_uniform_location_t",
		.finalizer = finalizer_webgl_uniform_location,
	};
	JS_NewClass(rt, nx_webgl_uniform_location_class_id,
				&webgl_uniform_location_class);

	JS_NewClassID(rt, &nx_webgl_texture_class_id);
	JSClassDef webgl_texture_class = {
		"nx_webgl_texture_t",
		.finalizer = finalizer_webgl_texture,
	};
	JS_NewClass(rt, nx_webgl_texture_class_id, &webgl_texture_class);

	JS_SetPropertyFunctionList(ctx, init_obj, init_function_list,
							   countof(init_function_list));
}
