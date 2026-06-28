#pragma once
#include "types.h"
#include <png.h>
#include <webp/decode.h>

enum ImageFormat { FORMAT_PNG, FORMAT_JPEG, FORMAT_WEBP, FORMAT_GIF, FORMAT_ICO, FORMAT_SVG, FORMAT_UNKNOWN };

typedef struct {
	u32 width;
	u32 height;
	u8 *data;
	bool data_needs_js_free;
	cairo_surface_t *surface;
	enum ImageFormat format;
	/* Multi-frame animation (GIF). When `frame_count > 1`, `frames` is
	 * an array of `frame_count` pre-decoded BGRA premultiplied buffers
	 * each (width * height * 4) bytes long, and `frame_delays_ms` is the
	 * per-frame display duration in milliseconds. The active cairo
	 * surface keeps pointing at `data`; `nx_image_set_frame` copies the
	 * selected frame's bytes into `data` and marks the surface dirty,
	 * so callers paint with the standard `drawImage(img, …)` path.
	 * Single-frame and non-GIF images leave these zero / NULL. */
	u32 frame_count;
	u32 current_frame;
	u8 **frames;
	u16 *frame_delays_ms;
	u32 loop_count;  /* 0 = loop forever; per Netscape extension */
	/* ImageBitmap creation options baked at createImageBitmap time. The
	 * pixel buffer is ALWAYS stored as cairo BGRA-premultiplied (cairo
	 * surfaces have no other native format on little-endian); these
	 * flags carry the *logical* state the spec says the bitmap is in
	 * so the WebGL upload extractor can compute the effective flip /
	 * un-premultiply transform when combined with the WebGL pixelStorei
	 * UNPACK_FLIP_Y_WEBGL / UNPACK_PREMULTIPLY_ALPHA_WEBGL state.
	 *
	 * `bitmap_orientation_flip_y` = true when the bitmap was created
	 * with `imageOrientation: 'flipY'`. Default false.
	 *
	 * `bitmap_alpha_mode` is a tristate:
	 *   0 — UNSPECIFIED: raw Image / canvas / video source, OR
	 *       ImageBitmap created via createImageBitmap without options.
	 *       WebGL extractor uses legacy behavior (un-multiply when
	 *       UNPACK_PREMUL=false).
	 *   1 — STRAIGHT: createImageBitmap was given
	 *       `premultiplyAlpha: 'none'`. Behaves the same as legacy
	 *       (un-multiplies storage to deliver straight pixels).
	 *   2 — PREMULTIPLIED: createImageBitmap was given
	 *       `premultiplyAlpha: 'premultiply'` or `'default'` against an
	 *       image-like source. Storage is already premul; spec says
	 *       keep as premul regardless of UNPACK_PREMUL_WEBGL.
	 *
	 * Both fields default to 0/false. */
	bool bitmap_orientation_flip_y;
	uint8_t bitmap_alpha_mode;
	/* Original straight (non-premultiplied) RGBA bytes preserved for
	 * STRAIGHT-mode bitmaps (premultiplyAlpha:'none'). Optional; NULL
	 * when not populated. Allocated by `nx_image_write_rgba` when
	 * called with preserve_straight=true (e.g., from the
	 * createImageBitmap(ImageData, ...) polyfill branch). The cairo
	 * `data` field above stores BGRA premultiplied; for STRAIGHT-intent
	 * bitmaps the conversion is lossy at a=0 source pixels (premul of
	 * (255,0,0,0) is (0,0,0,0); the WebGL extractor's un-multiply path
	 * cannot recover the original color). The WebGL extractor checks
	 * `bitmap_alpha_mode == STRAIGHT && straight_data != NULL` and
	 * reads from this buffer instead of the lossy cairo buffer. Cloned
	 * by `nx_image_clone_from`. Freed by `close_image`. */
	u8 *straight_data;
} nx_image_t;

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj);

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
				int *height);

void nx_init_image(JSContext *ctx, JSValueConst init_obj);
