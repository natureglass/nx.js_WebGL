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
	int bridge_requested_width;
	int bridge_requested_height;
	int bridge_width;
	int bridge_height;
	GLuint bridge_texture;
	GLuint bridge_framebuffer;
	GLuint bridge_color_program;
	GLuint bridge_color_vertex_shader;
	GLuint bridge_color_fragment_shader;
	GLuint bridge_texture_program;
	GLuint bridge_texture_vertex_shader;
	GLuint bridge_texture_fragment_shader;
	GLuint bridge_vertex_buffer;
	uint8_t *bridge_readback;
	size_t bridge_readback_size;
	nx_webgl_egl_texture_cache_entry_t texture_cache[NX_WEBGL_EGL_TEXTURE_CACHE_SIZE];
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
	if (backend->bridge_framebuffer) {
		glDeleteFramebuffers(1, &backend->bridge_framebuffer);
		backend->bridge_framebuffer = 0;
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

	glGenFramebuffers(1, &backend->bridge_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, backend->bridge_texture, 0);
	GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge: framebuffer incomplete: 0x%x",
				 framebuffer_status);
		destroy_bridge_resources(backend);
		return false;
	}

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

static bool ensure_bridge_color_program(nx_webgl_egl_t *backend) {
	if (backend->bridge_color_program && backend->bridge_vertex_buffer)
		return true;

	static const char vertex_source[] =
		"attribute vec2 a_position;\n"
		"void main() {\n"
		"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"uniform vec4 u_color;\n"
		"void main() {\n"
		"  gl_FragColor = u_color;\n"
		"}\n";

	backend->bridge_color_vertex_shader =
		compile_triangle_shader(GL_VERTEX_SHADER, vertex_source,
								backend->status, sizeof(backend->status));
	if (!backend->bridge_color_vertex_shader)
		return false;
	backend->bridge_color_fragment_shader =
		compile_triangle_shader(GL_FRAGMENT_SHADER, fragment_source,
								backend->status, sizeof(backend->status));
	if (!backend->bridge_color_fragment_shader)
		return false;

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

	glGenBuffers(1, &backend->bridge_vertex_buffer);
	if (!backend->bridge_vertex_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw: glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	return true;
}

static bool ensure_bridge_texture_program(nx_webgl_egl_t *backend) {
	if (backend->bridge_texture_program && backend->bridge_vertex_buffer)
		return true;

	static const char vertex_source[] =
		"attribute vec2 a_position;\n"
		"attribute vec2 a_uv;\n"
		"varying vec2 v_uv;\n"
		"void main() {\n"
		"  v_uv = a_uv;\n"
		"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
		"}\n";
	static const char fragment_source[] =
		"precision mediump float;\n"
		"uniform sampler2D u_texture;\n"
		"varying vec2 v_uv;\n"
		"void main() {\n"
		"  gl_FragColor = texture2D(u_texture, v_uv);\n"
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

	if (!backend->bridge_vertex_buffer)
		glGenBuffers(1, &backend->bridge_vertex_buffer);
	if (!backend->bridge_vertex_buffer) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: glGenBuffers() failed: 0x%x",
				 glGetError());
		return false;
	}
	return true;
}

static GLenum bridge_texture_filter(uint32_t filter) {
	return filter == GL_LINEAR ? GL_LINEAR : GL_NEAREST;
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
	EGLint context_attrs[] = {
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
	const char *config_profile = "RGBA8 ES2";
	bool config_ok = eglChooseConfig(backend->display, rgba8_attrs,
									 &backend->config, 1, &config_count);
	EGLint config_error = eglGetError();
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
				 "step 7: calling eglCreateContext()");
	backend->context = eglCreateContext(backend->display, backend->config,
										EGL_NO_CONTEXT, context_attrs);
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
#endif
	js_free_rt(rt, backend);
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
	snprintf(backend->status, sizeof(backend->status),
			 enabled ? "GPU bridge render mode enabled"
					 : "GPU bridge render mode disabled");
#else
	(void)enabled;
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

bool nx_webgl_egl_clear_bridge(nx_webgl_egl_t *backend, nx_canvas_t *canvas) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
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

	int width = 0;
	int height = 0;
	bridge_render_size(backend, canvas, &width, &height);
	if (!ensure_bridge_resources(backend, width, height))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glViewport(0, 0, width, height);
	glClearColor((GLfloat)backend->clear_color[0],
				 (GLfloat)backend->clear_color[1],
				 (GLfloat)backend->clear_color[2],
				 (GLfloat)backend->clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge clear failed: 0x%x", error);
		return false;
	}

	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge clear queued %dx%d -> %ux%u", width, height,
			 canvas->width, canvas->height);
	return true;
#endif
}

bool nx_webgl_egl_draw_triangles_bridge(nx_webgl_egl_t *backend,
										nx_canvas_t *canvas,
										const float *clip_xy,
										int vertex_count,
										const float *color,
										bool blend,
										uint32_t blend_src,
										uint32_t blend_dst) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)clip_xy;
	(void)vertex_count;
	(void)color;
	(void)blend;
	(void)blend_src;
	(void)blend_dst;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0 || !clip_xy ||
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

	int width = 0;
	int height = 0;
	bridge_render_size(backend, canvas, &width, &height);
	if (!ensure_bridge_resources(backend, width, height) ||
		!ensure_bridge_color_program(backend))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glViewport(0, 0, width, height);
	glUseProgram(backend->bridge_color_program);
	GLint color_location =
		glGetUniformLocation(backend->bridge_color_program, "u_color");
	if (color_location >= 0)
		glUniform4f(color_location, color[0], color[1], color[2], color[3]);
	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, (size_t)vertex_count * 2 * sizeof(float),
				 clip_xy, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	if (blend) {
		glEnable(GL_BLEND);
		glBlendFunc(blend_src, blend_dst);
	} else {
		glDisable(GL_BLEND);
	}
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDrawArrays(GL_TRIANGLES, 0, vertex_count);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
				 backend->bridge_readback);
	GLenum error = glGetError();
	glFinish();
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge draw failed: 0x%x", error);
		return false;
	}

	copy_rgba_readback_to_canvas_scaled(canvas, backend->bridge_readback, width,
										height);
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge drew %d untextured vertices at %dx%d -> %ux%u",
			 vertex_count, width, height, canvas->width, canvas->height);
	return true;
#endif
}

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
	uint32_t blend_dst) {
#if !NXJS_HAS_EGL_GLES
	(void)backend;
	(void)canvas;
	(void)clip_uv;
	(void)vertex_count;
	(void)texture_id;
	(void)texture_revision;
	(void)texture_width;
	(void)texture_height;
	(void)texture_rgba;
	(void)min_filter;
	(void)mag_filter;
	(void)wrap_s;
	(void)wrap_t;
	(void)blend;
	(void)blend_src;
	(void)blend_dst;
	return false;
#else
	if (!backend || !backend->bridge_enabled || !canvas || !canvas->data ||
		canvas->width == 0 || canvas->height == 0 || !clip_uv ||
		vertex_count <= 0 || vertex_count % 3 != 0 || !texture_rgba ||
		texture_id == 0 || texture_revision == 0 || texture_width <= 0 ||
		texture_height <= 0)
		return false;
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw: eglMakeCurrent() failed: 0x%x",
				 eglGetError());
		return false;
	}

	int width = 0;
	int height = 0;
	bridge_render_size(backend, canvas, &width, &height);
	if (!ensure_bridge_resources(backend, width, height) ||
		!ensure_bridge_texture_program(backend))
		return false;

	GLuint texture_handle = ensure_bridge_cached_texture(
		backend, texture_id, texture_revision, texture_width, texture_height,
		texture_rgba, min_filter, mag_filter, wrap_s, wrap_t);
	if (!texture_handle)
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, backend->bridge_framebuffer);
	glViewport(0, 0, width, height);
	glUseProgram(backend->bridge_texture_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);
	GLint sampler_location =
		glGetUniformLocation(backend->bridge_texture_program, "u_texture");
	if (sampler_location >= 0)
		glUniform1i(sampler_location, 0);
	glBindBuffer(GL_ARRAY_BUFFER, backend->bridge_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, (size_t)vertex_count * 4 * sizeof(float),
				 clip_uv, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
						  (const void *)(sizeof(float) * 2));
	if (blend) {
		glEnable(GL_BLEND);
		glBlendFunc(blend_src, blend_dst);
	} else {
		glDisable(GL_BLEND);
	}
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDrawArrays(GL_TRIANGLES, 0, vertex_count);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
				 backend->bridge_readback);
	GLenum error = glGetError();
	glFinish();
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (error != GL_NO_ERROR) {
		snprintf(backend->status, sizeof(backend->status),
				 "GPU bridge texture draw failed: 0x%x", error);
		return false;
	}

	copy_rgba_readback_to_canvas_scaled(canvas, backend->bridge_readback, width,
										height);
	snprintf(backend->status, sizeof(backend->status),
			 "GPU bridge textured draw %d vertices at %dx%d -> %ux%u",
			 vertex_count, width, height, canvas->width, canvas->height);
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
#endif

	return obj;
}
