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
} nx_image_t;

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj);

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
				int *height);

void nx_init_image(JSContext *ctx, JSValueConst init_obj);
