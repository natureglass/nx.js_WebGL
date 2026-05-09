#include "webgl_egl.h"
#include <stdio.h>

#ifndef NXJS_HAS_EGL_GLES
#define NXJS_HAS_EGL_GLES 0
#endif

#if NXJS_HAS_EGL_GLES
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

struct nx_webgl_egl_s {
	bool built;
	bool available;
	bool attempted;
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
	if (backend->attempted)
		return false;
	backend->attempted = true;

#if !NXJS_HAS_EGL_GLES
	(void)canvas;
	return false;
#else
	EGLint attrs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE,
	};
	EGLint context_attrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};

	backend->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (backend->display == EGL_NO_DISPLAY) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglGetDisplay() failed: 0x%x", eglGetError());
		return false;
	}

	if (!eglInitialize(backend->display, &backend->major, &backend->minor)) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglInitialize() failed: 0x%x", eglGetError());
		return false;
	}

	EGLint config_count = 0;
	if (!eglChooseConfig(backend->display, attrs, &backend->config, 1,
						 &config_count) ||
		config_count == 0) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglChooseConfig() failed: 0x%x", eglGetError());
		return false;
	}

	backend->surface = eglCreateWindowSurface(
		backend->display, backend->config,
		(EGLNativeWindowType)nwindowGetDefault(), NULL);
	if (backend->surface == EGL_NO_SURFACE) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglCreateWindowSurface() failed: 0x%x", eglGetError());
		return false;
	}

	backend->context = eglCreateContext(backend->display, backend->config,
										EGL_NO_CONTEXT, context_attrs);
	if (backend->context == EGL_NO_CONTEXT) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglCreateContext() failed: 0x%x", eglGetError());
		return false;
	}

	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context)) {
		snprintf(backend->status, sizeof(backend->status),
				 "eglMakeCurrent() failed: 0x%x", eglGetError());
		return false;
	}

	backend->vendor = (const char *)glGetString(GL_VENDOR);
	backend->version = (const char *)glGetString(GL_VERSION);
	backend->renderer = (const char *)glGetString(GL_RENDERER);
	backend->available = true;
	snprintf(backend->status, sizeof(backend->status),
			 "EGL/OpenGL ES context initialized");
	(void)canvas;
	return true;
#endif
}

void nx_webgl_egl_destroy(JSRuntime *rt, nx_webgl_egl_t *backend) {
	if (!backend)
		return;
#if NXJS_HAS_EGL_GLES
	if (backend->display != EGL_NO_DISPLAY) {
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

bool nx_webgl_egl_clear_prototype(nx_webgl_egl_t *backend,
								  nx_canvas_t *canvas) {
	if (!nx_webgl_egl_initialize(backend, canvas))
		return false;
#if NXJS_HAS_EGL_GLES
	if (!eglMakeCurrent(backend->display, backend->surface, backend->surface,
						backend->context))
		return false;
	glClearColor((GLfloat)backend->clear_color[0],
				 (GLfloat)backend->clear_color[1],
				 (GLfloat)backend->clear_color[2],
				 (GLfloat)backend->clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);
	return eglSwapBuffers(backend->display, backend->surface) == EGL_TRUE;
#else
	return false;
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

#if NXJS_HAS_EGL_GLES
	define_int(ctx, obj, "eglMajor", backend ? backend->major : 0);
	define_int(ctx, obj, "eglMinor", backend ? backend->minor : 0);
	define_string(ctx, obj, "glVendor", backend ? backend->vendor : "");
	define_string(ctx, obj, "glVersion", backend ? backend->version : "");
	define_string(ctx, obj, "glRenderer", backend ? backend->renderer : "");
#else
	define_int(ctx, obj, "eglMajor", 0);
	define_int(ctx, obj, "eglMinor", 0);
	define_string(ctx, obj, "glVendor", "");
	define_string(ctx, obj, "glVersion", "");
	define_string(ctx, obj, "glRenderer", "");
#endif

	return obj;
}
