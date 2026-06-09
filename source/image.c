#include "image.h"
#include "async.h"
#include "gifdec.h"
#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#include <webp/decode.h>

/* nanosvg + nanosvgrast: single-header SVG parser + rasterizer (MIT).
 * Defining the *_IMPLEMENTATION macros emits the function bodies into
 * this translation unit. We never call nsvgParseFromFile, but the
 * stdio dependency it pulls in is harmless. */
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "vendor/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "vendor/nanosvgrast.h"

static JSClassID nx_image_class_id;

typedef struct {
	int err;
	char *err_str;
	nx_image_t *image;
	JSValue image_val;
	JSValue buffer_val;
	uint8_t *input;
	size_t input_size;
} nx_decode_image_async_t;

struct buffer_state {
	uint8_t *ptr;
	size_t size;
};

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_image_class_id);
}

void close_image(JSRuntime *rt, nx_image_t *image) {
	if (image->surface) {
		cairo_surface_destroy(image->surface);
		image->surface = NULL;
	}

	if (image->data) {
		if (image->data_needs_js_free) {
			js_free_rt(rt, image->data);
		} else if (image->format == FORMAT_JPEG) {
			tjFree(image->data);
		} else {
			free(image->data);
		}
		image->data = NULL;
		image->data_needs_js_free = false;
	}

	/* Multi-frame (animated GIF) cleanup. `image->data` was a fresh
	 * "active framebuffer" memcpy'd from frames[current_frame]; the
	 * per-frame arrays own their own buffers and must be freed
	 * separately here. */
	if (image->frames) {
		for (u32 i = 0; i < image->frame_count; i++) {
			free(image->frames[i]);
		}
		free(image->frames);
		image->frames = NULL;
	}
	if (image->frame_delays_ms) {
		free(image->frame_delays_ms);
		image->frame_delays_ms = NULL;
	}
	image->frame_count = 0;
	image->current_frame = 0;
	image->loop_count = 0;

	image->width = image->height = 0;
}

void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	struct buffer_state *state = (struct buffer_state *)png_get_io_ptr(png_ptr);
	memcpy(data, state->ptr, length);
	state->ptr += length;
}

enum ImageFormat identify_image_format(uint8_t *data, size_t size) {
	if (size >= 8 && !memcmp(data, "\211PNG\r\n\032\n", 8)) {
		return FORMAT_PNG;
	} else if (size >= 2 && !memcmp(data, "\377\330", 2)) {
		return FORMAT_JPEG;
	} else if (size >= 12 && !memcmp(data, "RIFF", 4) &&
			   !memcmp(data + 8, "WEBP", 4)) {
		return FORMAT_WEBP;
	} else if (size >= 6 &&
			   (!memcmp(data, "GIF87a", 6) || !memcmp(data, "GIF89a", 6))) {
		return FORMAT_GIF;
	} else if (size >= 6 && data[0] == 0x00 && data[1] == 0x00 &&
			   data[2] == 0x01 && data[3] == 0x00 &&
			   /* image type 1 = ICO; type 2 = CUR. Reject CUR for now. */
			   (data[4] | (data[5] << 8)) > 0) {
		return FORMAT_ICO;
	}
	/* SVG: XML payload. Cheap scan after stripping BOM + whitespace; we
	 * look for "<svg" (case-insensitive) within the first 1KB so that
	 * "<?xml ...?> ... <svg ...>" payloads still match. */
	{
		size_t i = 0;
		if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
			i = 3;
		while (i < size && (data[i] == ' ' || data[i] == '\t' ||
							data[i] == '\n' || data[i] == '\r'))
			i++;
		if (i < size && data[i] == '<') {
			size_t scan_end = size < 1024 ? size : 1024;
			for (size_t k = i; k + 4 <= scan_end; k++) {
				if (data[k] == '<' &&
					(data[k + 1] == 's' || data[k + 1] == 'S') &&
					(data[k + 2] == 'v' || data[k + 2] == 'V') &&
					(data[k + 3] == 'g' || data[k + 3] == 'G')) {
					if (k + 4 == scan_end) return FORMAT_SVG;
					uint8_t c = data[k + 4];
					if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
						c == '>' || c == '/')
						return FORMAT_SVG;
				}
			}
		}
	}
	return FORMAT_UNKNOWN;
}

void premultiply_alpha(uint8_t *image_data, int width, int height) {
	for (int i = 0; i < width * height; ++i) {
		uint8_t *pixel = &image_data[i * 4];
		uint8_t alpha = pixel[3];
		pixel[0] = (pixel[0] * alpha) / 255;
		pixel[1] = (pixel[1] * alpha) / 255;
		pixel[2] = (pixel[2] * alpha) / 255;
	}
}

uint8_t *decode_png(uint8_t *input, size_t input_size, u32 *width,
					u32 *height) {
	png_structp png_ptr =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	struct buffer_state state = {input, input_size};
	png_set_read_fn(png_ptr, &state, user_read_data);

	png_read_info(png_ptr, info_ptr);

	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);

	png_set_bgr(png_ptr);
	png_set_expand(png_ptr);
	bool has_alpha =
		png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA;
	if (!has_alpha) {
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	}

	// Guard against overflow from untrusted PNG dimensions
	if (*width == 0 || *height == 0 ||
		*width > 16384 || *height > 16384 ||
		(size_t)(*width) > SIZE_MAX / 4 / (*height)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	uint8_t *image_data = malloc(4 * (size_t)(*width) * (*height));
	png_bytep *rows = malloc(sizeof(png_bytep) * (*height));
	if (!image_data || !rows) {
		free(image_data);
		free(rows);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}
	for (u32 i = 0; i < *height; ++i) {
		rows[i] = image_data + i * 4 * (*width);
	}

	png_read_image(png_ptr, rows);
	free(rows);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	if (has_alpha) {
		premultiply_alpha(image_data, *width, *height);
	}

	return image_data;
}

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
				int *height) {
	tjhandle handle = NULL;
	int subsamp, colorspace;
	int ret = -1;

	handle = tjInitDecompress();
	if (handle == NULL) {
		// printf("Error in tjInitDecompress(): %s\n", tjGetErrorStr());
		goto cleanup;
	}

	if (tjDecompressHeader3(handle, jpegBuf, jpegSize, width, height, &subsamp,
							&colorspace) == -1) {
		// printf("Error in tjDecompressHeader3(): %s\n", tjGetErrorStr());
		goto cleanup;
	}

	*output = tjAlloc((*width) * (*height) * tjPixelSize[TJPF_BGRA]);

	if (tjDecompress2(handle, jpegBuf, jpegSize, *output, *width, 0 /*pitch*/,
					  *height, TJPF_BGRA, TJFLAG_FASTDCT) == -1) {
		// printf("Error in tjDecompress2(): %s\n", tjGetErrorStr());
		tjFree(*output);
		*output = NULL;
		goto cleanup;
	}

	ret = 0;

cleanup:
	if (handle != NULL)
		tjDestroy(handle);

	return ret;
}

uint8_t *decode_webp(uint8_t *webp_data, size_t data_size, int *width,
					 int *height) {
	// Decode the WebP image data into a BGRA format
	uint8_t *bgra_data = WebPDecodeBGRA(webp_data, data_size, width, height);
	if (bgra_data == NULL) {
		// TODO: Handle error
		return NULL;
	}

	premultiply_alpha(bgra_data, *width, *height);

	return bgra_data;
}

/* ===========================================================================
 * SVG decoder (nanosvg + nanosvgrast).
 *
 * Parses the SVG payload and rasterizes at its intrinsic viewport size at
 * 96 DPI. nanosvg's `nsvgParse` mutates AND requires a NUL-terminated input
 * string, so we copy into a freshly malloc'd buffer first. nanosvgrast
 * writes non-premultiplied RGBA into the destination; we swizzle to
 * premultiplied BGRA in-place to match the rest of the pipeline (cairo
 * ARGB32 expects B,G,R,A in memory order on little-endian).
 *
 * Per-target-size rasterization (rasterizing at the eventual display size
 * to avoid Cairo bilinear downsampling) is a follow-up — the painter
 * currently downsamples this intrinsic-size buffer the same way it would
 * for a PNG.
 * =========================================================================== */

#define SVG_MAX_DIM 4096

static uint8_t *decode_svg(uint8_t *input, size_t input_size, u32 *out_w,
						   u32 *out_h) {
	if (input_size == 0) return NULL;
	char *copy = malloc(input_size + 1);
	if (!copy) return NULL;
	memcpy(copy, input, input_size);
	copy[input_size] = 0;

	NSVGimage *svg = nsvgParse(copy, "px", 96.0f);
	free(copy);
	if (!svg) return NULL;

	int w = (int)(svg->width + 0.5f);
	int h = (int)(svg->height + 0.5f);
	if (w <= 0 || h <= 0 || w > SVG_MAX_DIM || h > SVG_MAX_DIM) {
		nsvgDelete(svg);
		return NULL;
	}

	NSVGrasterizer *rast = nsvgCreateRasterizer();
	if (!rast) {
		nsvgDelete(svg);
		return NULL;
	}

	size_t stride = (size_t)w * 4;
	uint8_t *pixels = malloc(stride * (size_t)h);
	if (!pixels) {
		nsvgDeleteRasterizer(rast);
		nsvgDelete(svg);
		return NULL;
	}
	/* nsvgRasterize clears the destination internally (see nanosvgrast.h
	 * line ~1398), so no pre-memset needed. */
	nsvgRasterize(rast, svg, 0.0f, 0.0f, 1.0f, pixels, w, h, (int)stride);

	nsvgDeleteRasterizer(rast);
	nsvgDelete(svg);

	/* RGBA (non-premultiplied) → BGRA premultiplied, in-place. */
	const size_t pixel_count = (size_t)w * (size_t)h;
	for (size_t i = 0; i < pixel_count; i++) {
		uint8_t r = pixels[i * 4 + 0];
		uint8_t g = pixels[i * 4 + 1];
		uint8_t b = pixels[i * 4 + 2];
		uint8_t a = pixels[i * 4 + 3];
		if (a == 0) {
			pixels[i * 4 + 0] = 0;
			pixels[i * 4 + 1] = 0;
			pixels[i * 4 + 2] = 0;
		} else if (a == 255) {
			pixels[i * 4 + 0] = b;
			pixels[i * 4 + 1] = g;
			pixels[i * 4 + 2] = r;
		} else {
			pixels[i * 4 + 0] = (uint8_t)(((uint16_t)b * a) / 255);
			pixels[i * 4 + 1] = (uint8_t)(((uint16_t)g * a) / 255);
			pixels[i * 4 + 2] = (uint8_t)(((uint16_t)r * a) / 255);
		}
	}

	*out_w = (u32)w;
	*out_h = (u32)h;
	return pixels;
}

/* ===========================================================================
 * ICO decoder.
 *
 * ICO (Microsoft icon container) is a tiny header pointing at one or more
 * entries; each entry is either an embedded PNG or a "BMP-shaped" DIB.
 * Spec: https://en.wikipedia.org/wiki/ICO_(file_format)
 *
 * Layout:
 *   ICONDIR        (6 bytes)            reserved=0, type=1 (ICO) / 2 (CUR), n
 *   ICONDIRENTRY[] (16 bytes each)      w, h, palette, planes, bpp, size, off
 *   payload[]                           PNG bytes OR BMP-without-fileheader
 *
 * Each direntry's W/H byte is 0 for "256". Payload at `off` of length `size`.
 *
 * For the BMP-shaped entry the payload is:
 *   BITMAPINFOHEADER (40 bytes; or rarely BITMAPV5HEADER 124 bytes)
 *   [color table]   (only present when bpp <= 8)
 *   XOR pixel data  (rows bottom-up, dword-aligned)
 *   AND mask data   (1 bit per pixel, rows bottom-up, dword-aligned)
 *
 * The DIB header's `height` is DOUBLED (XOR rows + AND rows) so the actual
 * image height is height/2. The AND mask supplies transparency when the
 * pixel data doesn't already have an alpha channel — we follow the Windows
 * convention: AND=1 → transparent, AND=0 → opaque. For 32-bit BMPs we trust
 * the embedded alpha first and only fall back to AND when every alpha byte
 * is zero (common in poorly-authored .ico files that left the alpha bytes
 * at 0 expecting the AND mask to do the work).
 *
 * Entry-selection strategy: prefer the largest-area entry; tie-break on bpp.
 * This matches how every modern browser picks favicons. We don't honor any
 * requested size hint here — caller scales the surface as needed.
 *
 * Returns a malloc'd premultiplied BGRA buffer on success (caller frees);
 * NULL on any error (truncated file, unsupported bpp, etc).
 * =========================================================================== */

#define ICO_MAX_DIM 1024

/* Read little-endian uint16/uint32 from `p` without alignment assumptions. */
static inline uint16_t ico_u16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t ico_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Decode a DIB-shaped (BMP-style) ICO entry payload starting at `payload`
 * of `payload_size` bytes. Writes width/height into outputs and returns a
 * malloc'd premultiplied BGRA buffer (or NULL). */
static uint8_t *decode_ico_dib(const uint8_t *payload, size_t payload_size,
							   u32 *out_w, u32 *out_h) {
	if (payload_size < 40) return NULL;
	uint32_t header_size = ico_u32(payload);
	/* Accept BITMAPINFOHEADER (40) and BITMAPV4HEADER (108) / V5HEADER (124).
	 * Anything weirder (e.g. OS/2 BITMAPCOREHEADER, 12 bytes) is rare in
	 * ICO and we punt. */
	if (header_size < 40 || header_size > 124 || header_size > payload_size) {
		return NULL;
	}
	int32_t  dib_w  = (int32_t)ico_u32(payload + 4);
	int32_t  dib_h  = (int32_t)ico_u32(payload + 8);
	uint16_t planes = ico_u16(payload + 12);
	uint16_t bpp    = ico_u16(payload + 14);
	uint32_t compression = ico_u32(payload + 16);

	/* DIB height is XOR_rows + AND_rows. */
	if (dib_w <= 0 || dib_h <= 0 || planes != 1) return NULL;
	if ((dib_h & 1) != 0) return NULL;
	uint32_t w = (uint32_t)dib_w;
	uint32_t h = (uint32_t)dib_h / 2;
	if (w == 0 || h == 0 || w > ICO_MAX_DIM || h > ICO_MAX_DIM) return NULL;
	/* BI_RGB (0) only. Drop BI_BITFIELDS / RLE — rare in favicons. */
	if (compression != 0) return NULL;
	/* Support the bpps that real-world favicons actually use. 1 / 4 / 8 are
	 * paletted with the color table immediately after the header. 24 / 32
	 * are direct RGB(A). */
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) {
		return NULL;
	}

	/* Color table for paletted entries. Each palette entry is 4 bytes BGRA
	 * (alpha byte is ignored — always treated as opaque). Number of entries
	 * is read from the header at offset 32 ("biClrUsed"); 0 means 2^bpp. */
	uint32_t pal_count = 0;
	const uint8_t *palette = NULL;
	if (bpp <= 8) {
		pal_count = ico_u32(payload + 32);
		if (pal_count == 0) pal_count = 1u << bpp;
		if (pal_count > 256) return NULL;
		if (header_size + pal_count * 4 > payload_size) return NULL;
		palette = payload + header_size;
	}

	/* XOR pixel data follows the (optional) palette. Rows are bottom-up
	 * and padded to a 4-byte boundary. */
	size_t xor_offset = header_size + (size_t)pal_count * 4;
	size_t xor_row_bits = (size_t)w * bpp;
	size_t xor_row = ((xor_row_bits + 31) / 32) * 4;  /* dword-aligned bytes */
	size_t xor_total = xor_row * h;
	if (xor_offset > payload_size || xor_total > payload_size - xor_offset) {
		return NULL;
	}
	const uint8_t *xor_data = payload + xor_offset;

	/* AND mask immediately follows XOR data. 1 bit per pixel, dword-aligned
	 * rows, also bottom-up. May be absent in malformed files; we tolerate
	 * that by treating "no AND data" as fully opaque. */
	size_t and_row = (((size_t)w + 31) / 32) * 4;
	size_t and_offset = xor_offset + xor_total;
	size_t and_total = and_row * h;
	int have_and = (and_offset + and_total <= payload_size);
	const uint8_t *and_data = have_and ? payload + and_offset : NULL;

	uint8_t *out = malloc((size_t)w * h * 4);
	if (!out) return NULL;

	/* For 32-bit, decide once up-front whether to trust the embedded alpha
	 * or fall back to the AND mask. Pre-scan: any non-zero alpha byte means
	 * the author actually populated alpha, so use it. */
	int trust_embedded_alpha = 0;
	if (bpp == 32) {
		for (size_t y = 0; y < h && !trust_embedded_alpha; y++) {
			const uint8_t *src = xor_data + (size_t)y * xor_row;
			for (size_t x = 0; x < w; x++) {
				if (src[x * 4 + 3] != 0) { trust_embedded_alpha = 1; break; }
			}
		}
	}

	for (uint32_t y = 0; y < h; y++) {
		/* Flip bottom-up → top-down on write. */
		uint8_t *dst = out + (size_t)(h - 1 - y) * w * 4;
		const uint8_t *xor_row_ptr = xor_data + (size_t)y * xor_row;
		const uint8_t *and_row_ptr = have_and ? and_data + (size_t)y * and_row : NULL;
		for (uint32_t x = 0; x < w; x++) {
			uint8_t b = 0, g = 0, r = 0, a = 0xFF;
			if (bpp == 32) {
				b = xor_row_ptr[x * 4 + 0];
				g = xor_row_ptr[x * 4 + 1];
				r = xor_row_ptr[x * 4 + 2];
				a = trust_embedded_alpha ? xor_row_ptr[x * 4 + 3] : 0xFF;
			} else if (bpp == 24) {
				b = xor_row_ptr[x * 3 + 0];
				g = xor_row_ptr[x * 3 + 1];
				r = xor_row_ptr[x * 3 + 2];
			} else if (bpp == 8) {
				uint8_t idx = xor_row_ptr[x];
				if (idx >= pal_count) idx = 0;
				b = palette[idx * 4 + 0];
				g = palette[idx * 4 + 1];
				r = palette[idx * 4 + 2];
			} else if (bpp == 4) {
				uint8_t byte = xor_row_ptr[x >> 1];
				uint8_t idx = (x & 1) ? (byte & 0x0F) : (byte >> 4);
				if (idx >= pal_count) idx = 0;
				b = palette[idx * 4 + 0];
				g = palette[idx * 4 + 1];
				r = palette[idx * 4 + 2];
			} else { /* bpp == 1 */
				uint8_t byte = xor_row_ptr[x >> 3];
				uint8_t idx = (byte >> (7 - (x & 7))) & 1;
				if (idx >= pal_count) idx = 0;
				b = palette[idx * 4 + 0];
				g = palette[idx * 4 + 1];
				r = palette[idx * 4 + 2];
			}
			/* AND mask: 1 bit per pixel, MSB-first within each byte.
			 * Bit set = transparent. Only consulted when we're NOT trusting
			 * embedded 32-bit alpha. */
			if (bpp != 32 || !trust_embedded_alpha) {
				if (have_and) {
					uint8_t mb = and_row_ptr[x >> 3];
					if ((mb >> (7 - (x & 7))) & 1) a = 0;
				}
			}
			/* Premultiply on the way out so the surface matches the rest of
			 * the pipeline (PNG/JPEG/WebP paths all premultiply). */
			dst[x * 4 + 0] = (uint8_t)(((uint16_t)b * a) / 255);
			dst[x * 4 + 1] = (uint8_t)(((uint16_t)g * a) / 255);
			dst[x * 4 + 2] = (uint8_t)(((uint16_t)r * a) / 255);
			dst[x * 4 + 3] = a;
		}
	}
	*out_w = w;
	*out_h = h;
	return out;
}

/* Decode an ICO container. Picks the best entry (largest area, then bpp),
 * delegates to decode_png when the entry is a PNG-in-ICO, otherwise to the
 * DIB path above. */
static uint8_t *decode_ico(uint8_t *input, size_t input_size,
						   u32 *out_w, u32 *out_h) {
	if (input_size < 6) return NULL;
	uint16_t reserved = ico_u16(input);
	uint16_t type     = ico_u16(input + 2);
	uint16_t count    = ico_u16(input + 4);
	if (reserved != 0 || type != 1 || count == 0) return NULL;
	if ((size_t)6 + (size_t)count * 16 > input_size) return NULL;

	/* Pick best entry: largest area, then highest bpp. */
	int best = -1;
	uint32_t best_area = 0;
	uint16_t best_bpp = 0;
	for (uint16_t i = 0; i < count; i++) {
		const uint8_t *e = input + 6 + (size_t)i * 16;
		uint32_t w = e[0] == 0 ? 256 : e[0];
		uint32_t h = e[1] == 0 ? 256 : e[1];
		uint16_t bpp = ico_u16(e + 6);
		uint32_t size = ico_u32(e + 8);
		uint32_t off  = ico_u32(e + 12);
		if (off == 0 || size == 0) continue;
		if ((size_t)off + size > input_size) continue;
		uint32_t area = w * h;
		if (area > best_area || (area == best_area && bpp > best_bpp)) {
			best = i;
			best_area = area;
			best_bpp = bpp;
		}
	}
	if (best < 0) return NULL;

	const uint8_t *e = input + 6 + (size_t)best * 16;
	uint32_t size = ico_u32(e + 8);
	uint32_t off  = ico_u32(e + 12);
	uint8_t *payload = input + off;

	/* PNG-in-ICO is signaled by the PNG magic in the payload (Vista+
	 * convention). Reuse the existing PNG path. */
	if (size >= 8 && !memcmp(payload, "\211PNG\r\n\032\n", 8)) {
		return decode_png(payload, size, out_w, out_h);
	}
	return decode_ico_dib(payload, size, out_w, out_h);
}

/* Hard ceilings to avoid runaway allocations on adversarial inputs. */
#define GIF_MAX_DIM 16384
#define GIF_MAX_FRAMES 1024
/* GIF delay is 1/100 s; a value of 0 is "as fast as possible" in spec
 * but most browsers clamp to ~20 ms to avoid runaway CPU. */
#define GIF_MIN_DELAY_MS 20

/* Free the temporary array of full-canvas BGRA frame buffers used
 * during decode_gif's first pass. Indexed [0, n). */
static void free_full_frames(uint8_t **full, u32 n) {
	if (!full) return;
	for (u32 i = 0; i < n; i++) free(full[i]);
	free(full);
}

/* Decode a GIF buffer into `image`. Two-stage:
 *
 *   1. Walk every frame, compositing against a running RGB canvas
 *      (gifdec's gd_render_frame) and a parallel single-byte alpha
 *      canvas (this function). Per-pixel alpha is propagated through
 *      gifdec's disposal methods 0/1/2; method 3 (restore-to-previous)
 *      is left as a no-op to match upstream gifdec. Each frame is
 *      saved as a full-canvas BGRA premultiplied buffer.
 *
 *   2. Compute the union of every frame's image-descriptor rectangle
 *      and crop every buffer down to that rectangle. The cropped
 *      size becomes `image->width / image->height`, so
 *      `naturalWidth/naturalHeight` on the JS side match the actual
 *      content rect (not the often-larger logical-screen rect that
 *      sits inside the GIF header).
 *
 * Returns 0 on success, -1 on any failure (with `image->data` left
 * NULL so the caller surfaces the generic "Image decode was not
 * initialized" error). */
static int decode_gif(uint8_t *input, size_t input_size, nx_image_t *image) {
	gd_GIF *gif = gd_open_gif_mem(input, input_size);
	if (!gif) return -1;
	if (gif->width == 0 || gif->height == 0 ||
		gif->width > GIF_MAX_DIM || gif->height > GIF_MAX_DIM) {
		gd_close_gif(gif);
		return -1;
	}

	const u32 cw = gif->width;
	const u32 ch = gif->height;
	const size_t full_stride = (size_t)cw * ch * 4;

	uint8_t *rgb_buf = malloc((size_t)cw * ch * 3);
	/* Per-pixel alpha for the carried canvas. Initial state: every
	 * pixel transparent — pixels become opaque (0xFF) only when a
	 * frame writes a non-transparent palette index there. This is the
	 * right default for the typical "transparent GIF over a coloured
	 * page" use case; a fully-opaque GIF still ends up with alpha=0xFF
	 * everywhere because every pixel gets written by some frame. */
	uint8_t *alpha_canvas = calloc((size_t)cw * ch, 1);
	if (!rgb_buf || !alpha_canvas) {
		free(rgb_buf);
		free(alpha_canvas);
		gd_close_gif(gif);
		return -1;
	}

	uint8_t **full_frames = NULL;
	uint16_t *delays = NULL;
	u32 cap = 0, n = 0;

	/* Union rect of every frame's image-descriptor extent. Initialized
	 * inverted so the first frame seeds it. */
	u32 ext_x0 = cw, ext_y0 = ch, ext_x1 = 0, ext_y1 = 0;

	/* Disposal-of-previous-frame state. Applied at the top of each
	 * loop iteration (mirroring gifdec's `dispose()` which runs at the
	 * top of gd_get_frame). */
	int has_pending = 0;
	int pend_disp = 0;
	u16 pend_fx = 0, pend_fy = 0, pend_fw = 0, pend_fh = 0;
	/* Tracks whether ANY frame so far flagged transparency. Used to
	 * decide what "background" alpha to use on disposal-2 (restore-
	 * to-background): transparent if the GIF is alpha-aware,
	 * otherwise opaque so a fully-opaque GIF doesn't get a one-frame
	 * transparent gap. */
	int has_seen_transparency = 0;

	int got;
	while ((got = gd_get_frame(gif)) == 1) {
		if (n >= GIF_MAX_FRAMES) break;

		if (has_pending) {
			if (pend_disp == 2) {
				const uint8_t bg_alpha = has_seen_transparency ? 0 : 0xFF;
				u32 y1 = (u32)pend_fy + pend_fh;
				u32 x1 = (u32)pend_fx + pend_fw;
				if (y1 > ch) y1 = ch;
				if (x1 > cw) x1 = cw;
				for (u32 y = pend_fy; y < y1; y++) {
					for (u32 x = pend_fx; x < x1; x++) {
						alpha_canvas[y * cw + x] = bg_alpha;
					}
				}
			}
			/* Disposals 0/1/3: alpha_canvas remains as-is. 0/1 are
			 * "don't dispose" / "leave as-is" — the frame's non-
			 * transparent pixels were already baked into the alpha
			 * canvas after the previous render. 3 is "restore to
			 * previous" which gifdec doesn't implement either. */
			has_pending = 0;
		}

		gd_render_frame(gif, rgb_buf);

		if (gif->gce.transparency) has_seen_transparency = 1;

		/* Composite current frame's non-transparent pixels into the
		 * alpha canvas at the frame's image-descriptor rect. Pixels
		 * marked as transparent inherit the carried alpha. */
		{
			u32 y1 = (u32)gif->fy + gif->fh;
			u32 x1 = (u32)gif->fx + gif->fw;
			if (y1 > ch) y1 = ch;
			if (x1 > cw) x1 = cw;
			const int trans = gif->gce.transparency;
			const uint8_t tindex = gif->gce.tindex;
			for (u32 y = gif->fy; y < y1; y++) {
				for (u32 x = gif->fx; x < x1; x++) {
					size_t idx = (size_t)y * cw + x;
					uint8_t pal_idx = gif->frame[idx];
					if (!trans || pal_idx != tindex) {
						alpha_canvas[idx] = 0xFF;
					}
				}
			}
		}

		if (n == cap) {
			u32 new_cap = cap ? cap * 2 : 8;
			uint8_t **nf = realloc(full_frames, new_cap * sizeof(*nf));
			uint16_t *nd = realloc(delays, new_cap * sizeof(*nd));
			if (!nf || !nd) {
				if (nf) full_frames = nf;
				if (nd) delays = nd;
				goto fail;
			}
			full_frames = nf;
			delays = nd;
			cap = new_cap;
		}

		uint8_t *bgra = malloc(full_stride);
		if (!bgra) goto fail;
		/* RGB+alpha → premultiplied BGRA. Alpha is 0 or 0xFF only, so
		 * premultiplication is a select between "all zeros" and "pass
		 * through". */
		{
			size_t pixels = (size_t)cw * ch;
			for (size_t i = 0; i < pixels; i++) {
				uint8_t a = alpha_canvas[i];
				if (a == 0) {
					bgra[i * 4 + 0] = 0;
					bgra[i * 4 + 1] = 0;
					bgra[i * 4 + 2] = 0;
					bgra[i * 4 + 3] = 0;
				} else {
					bgra[i * 4 + 0] = rgb_buf[i * 3 + 2]; /* B */
					bgra[i * 4 + 1] = rgb_buf[i * 3 + 1]; /* G */
					bgra[i * 4 + 2] = rgb_buf[i * 3 + 0]; /* R */
					bgra[i * 4 + 3] = a;
				}
			}
		}
		full_frames[n] = bgra;

		u32 ms = (u32)gif->gce.delay * 10;
		if (ms < GIF_MIN_DELAY_MS) ms = GIF_MIN_DELAY_MS;
		delays[n] = (uint16_t)(ms > 65535 ? 65535 : ms);

		/* Update union extent with this frame's rect. */
		{
			u32 fx1 = (u32)gif->fx + gif->fw;
			u32 fy1 = (u32)gif->fy + gif->fh;
			if (fx1 > cw) fx1 = cw;
			if (fy1 > ch) fy1 = ch;
			if ((u32)gif->fx < ext_x0) ext_x0 = gif->fx;
			if ((u32)gif->fy < ext_y0) ext_y0 = gif->fy;
			if (fx1 > ext_x1) ext_x1 = fx1;
			if (fy1 > ext_y1) ext_y1 = fy1;
		}

		pend_disp = gif->gce.disposal;
		pend_fx = gif->fx;
		pend_fy = gif->fy;
		pend_fw = gif->fw;
		pend_fh = gif->fh;
		has_pending = 1;

		n++;
	}
	free(rgb_buf);
	rgb_buf = NULL;
	free(alpha_canvas);
	alpha_canvas = NULL;

	if (n == 0 || ext_x1 <= ext_x0 || ext_y1 <= ext_y0) goto fail;

	/* Stage 2 — crop. */
	const u32 ew = ext_x1 - ext_x0;
	const u32 eh = ext_y1 - ext_y0;
	const size_t crop_stride = (size_t)ew * eh * 4;
	const size_t crop_row = (size_t)ew * 4;

	uint8_t **cropped = malloc((size_t)n * sizeof(*cropped));
	if (!cropped) goto fail;
	for (u32 f = 0; f < n; f++) cropped[f] = NULL;
	for (u32 f = 0; f < n; f++) {
		cropped[f] = malloc(crop_stride);
		if (!cropped[f]) {
			free_full_frames(cropped, n);
			cropped = NULL;
			goto fail;
		}
		for (u32 y = 0; y < eh; y++) {
			memcpy(
				cropped[f] + (size_t)y * crop_row,
				full_frames[f] + ((size_t)(ext_y0 + y) * cw + ext_x0) * 4,
				crop_row);
		}
		free(full_frames[f]);
		full_frames[f] = NULL;
	}
	free(full_frames);
	full_frames = NULL;

	image->width = ew;
	image->height = eh;
	image->frame_count = n;
	image->loop_count = gif->loop_count;
	image->frames = cropped;
	image->frame_delays_ms = delays;
	image->current_frame = 0;

	image->data = malloc(crop_stride);
	if (!image->data) {
		free_full_frames(cropped, n);
		image->frames = NULL;
		free(delays);
		image->frame_delays_ms = NULL;
		image->frame_count = 0;
		gd_close_gif(gif);
		return -1;
	}
	memcpy(image->data, cropped[0], crop_stride);

	gd_close_gif(gif);
	return 0;

fail:
	free(rgb_buf);
	free(alpha_canvas);
	free_full_frames(full_frames, n);
	free(delays);
	gd_close_gif(gif);
	return -1;
}

void nx_decode_image_do(nx_work_t *req) {
	nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;
	// 2026-06-07 pvzge texture-upload investigation: log every decode
	// with input buffer size + first 8 magic bytes + the format the
	// identifier picked + dimensions after decode. This narrows whether
	// (a) input bytes never arrived (size=0), (b) decoder didn't
	// recognize the magic (format=UNKNOWN), or (c) decoder recognized
	// but produced 0x0 dims (decoder rejected the body). Throttle: first
	// 10 + every 25th to keep log volume sane across the ~75 pvzge image
	// loads per launch.
	static int decodeN = 0;
	++decodeN;
	bool should_log = (decodeN <= 10) || (decodeN % 25 == 0);
	if (should_log) {
		const uint8_t *p = (const uint8_t *)data->input;
		uint8_t b[8] = {0};
		size_t blen = data->input_size < 8 ? data->input_size : 8;
		if (p)
			memcpy(b, p, blen);
		fprintf(stderr,
			"[nxjs:decode-in] n=%d size=%zu magic=%02x%02x%02x%02x%02x%02x%02x%02x\n",
			decodeN, data->input_size,
			b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
	}
	data->image->format = identify_image_format(data->input, data->input_size);
	if (data->image->format == FORMAT_PNG) {
		data->image->data =
			decode_png(data->input, data->input_size, &data->image->width,
					   &data->image->height);
	} else if (data->image->format == FORMAT_JPEG) {
		if (decode_jpeg(data->input, data->input_size, &data->image->data,
						(int *)&data->image->width,
						(int *)&data->image->height)) {
			data->err_str = tjGetErrorStr();
			if (should_log)
				fprintf(stderr, "[nxjs:decode-out] n=%d FAIL jpeg err=%s\n",
					decodeN, data->err_str);
			return;
		}
	} else if (data->image->format == FORMAT_WEBP) {
		data->image->data = decode_webp(data->input, data->input_size,
										(int *)&data->image->width,
										(int *)&data->image->height);
	} else if (data->image->format == FORMAT_GIF) {
		/* decode_gif fills in width/height/frames/data on the image
		 * directly; leave the rest of nx_decode_image_do's surface-
		 * build path untouched. */
		if (decode_gif(data->input, data->input_size, data->image) != 0) {
			data->err_str = "Failed to decode GIF";
			if (should_log)
				fprintf(stderr, "[nxjs:decode-out] n=%d FAIL gif\n", decodeN);
			return;
		}
	} else if (data->image->format == FORMAT_ICO) {
		data->image->data =
			decode_ico(data->input, data->input_size, &data->image->width,
					   &data->image->height);
	} else if (data->image->format == FORMAT_SVG) {
		data->image->data =
			decode_svg(data->input, data->input_size, &data->image->width,
					   &data->image->height);
	} else {
		data->err_str = "Unsupported image format";
		if (should_log)
			fprintf(stderr, "[nxjs:decode-out] n=%d FAIL unsupported format=%d\n",
				decodeN, (int)data->image->format);
		return;
	}
	if (data->image->data == NULL) {
		data->err_str = "Image decode was not initialized";
		if (should_log)
			fprintf(stderr, "[nxjs:decode-out] n=%d FAIL data=NULL format=%d w=%u h=%u\n",
				decodeN, (int)data->image->format,
				data->image->width, data->image->height);
		return;
	}
	if (should_log)
		fprintf(stderr,
			"[nxjs:decode-out] n=%d OK format=%d w=%u h=%u\n",
			decodeN, (int)data->image->format,
			data->image->width, data->image->height);
	data->image->surface = cairo_image_surface_create_for_data(
		data->image->data, CAIRO_FORMAT_ARGB32, data->image->width,
		data->image->height, data->image->width * 4);
}

JSValue nx_decode_image_cb(JSContext *ctx, nx_work_t *req) {
	nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		JS_FreeValue(ctx, data->image_val);
		JS_FreeValue(ctx, data->buffer_val);
		return JS_Throw(ctx, err);
	} else if (data->err_str) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, data->err_str),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		JS_FreeValue(ctx, data->image_val);
		JS_FreeValue(ctx, data->buffer_val);
		return JS_Throw(ctx, err);
	}

	JS_FreeValue(ctx, data->image_val);
	JS_FreeValue(ctx, data->buffer_val);
	return JS_UNDEFINED;
}

JSValue nx_image_decode(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decode_image_async_t);
	data->image = nx_get_image(ctx, argv[0]);
	data->image_val = JS_DupValue(ctx, argv[0]);
	data->buffer_val = JS_DupValue(ctx, argv[1]);
	data->input = JS_GetArrayBuffer(ctx, &data->input_size, data->buffer_val);
	return nx_queue_async(ctx, req, nx_decode_image_do, nx_decode_image_cb);
}

JSValue nx_image_new(JSContext *ctx, JSValueConst this_val, int argc,
					 JSValueConst *argv) {
	JSValue img = JS_NewObjectClass(ctx, nx_image_class_id);
	if (JS_IsException(img)) {
		return img;
	}
	nx_image_t *data = js_mallocz(ctx, sizeof(nx_image_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	JS_SetOpaque(img, data);
	if (argc == 2) {
		if (JS_ToUint32(ctx, &data->width, argv[0]) ||
			JS_ToUint32(ctx, &data->height, argv[1])) {
			return JS_EXCEPTION;
		}
		// Overflow check before allocation
		if (data->width == 0 || data->height == 0 ||
			data->width > SIZE_MAX / 4 ||
			(size_t)data->height > SIZE_MAX / ((size_t)data->width * 4)) {
			return JS_ThrowRangeError(ctx, "Image dimensions too large");
		}
		// Width and height were specified, so allocate a backing store to use
		data->data = js_mallocz(ctx, (size_t)data->width * data->height * 4);
		data->data_needs_js_free = true;
		if (!data->data) {
			return JS_EXCEPTION;
		}
		data->surface = cairo_image_surface_create_for_data(
			data->data, CAIRO_FORMAT_ARGB32, data->width, data->height,
			data->width * 4);
	}
	return img;
}

JSValue nx_image_close(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) {
		return JS_EXCEPTION;
	}
	close_image(JS_GetRuntime(ctx), image);
	return JS_UNDEFINED;
}

// Copies width*height*4 RGBA bytes from `argv[1]` (ArrayBuffer or any
// TypedArray view onto one) into the Image's backing buffer with the
// RGBA→BGRA-premultiplied swizzle Cairo expects (CAIRO_FORMAT_ARGB32
// stores B,G,R,A in memory order on little-endian). The Image must
// have been constructed with explicit (width, height) so it has a
// backing buffer + cairo surface — see `nx_image_new` for the
// allocation path.
//
// Used by brewser's <video> frame delivery to feed decoded
// FFmpeg RGBA frames into an Image that can be drawn via the standard
// drawImage(img, x, y) cairo paint path, sidestepping the second-call
// putImageData hang we hit during slice 2a. See live-video.ts for the
// caller side and feedback_nxjs_putimagedata_screen_ctx_hangs_on_second_call
// for the bug we're avoiding.
JSValue nx_image_write_rgba(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) {
		return JS_ThrowTypeError(ctx, "imageWriteRGBA: first arg must be an Image");
	}
	if (!image->data || !image->surface) {
		return JS_ThrowTypeError(
			ctx,
			"imageWriteRGBA: image has no backing buffer — construct with new ImageBitmap-ish helper that allocates width/height up-front");
	}

	// Accept either a raw ArrayBuffer or any TypedArray view onto one.
	// Try raw ArrayBuffer first (this is what `Switch.VideoDecoder.nextFrame().data`
	// returns directly, so it's the common case). Fall through to the
	// TypedArray path for Uint8Array / Uint8ClampedArray callers.
	size_t src_size = 0;
	uint8_t *src = JS_GetArrayBuffer(ctx, &src_size, argv[1]);
	if (!src) {
		size_t src_offset = 0, src_length = 0, bytes_per_element = 0;
		JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[1], &src_offset,
											&src_length, &bytes_per_element);
		if (JS_IsException(ab)) {
			// Clear the typed-array exception so we can throw our own
			// clearer message. JS_GetException returns the thrown value
			// owned by the caller — free it explicitly.
			JSValue thrown = JS_GetException(ctx);
			JS_FreeValue(ctx, thrown);
			return JS_ThrowTypeError(
				ctx,
				"imageWriteRGBA: second arg must be ArrayBuffer or TypedArray");
		}
		src = JS_GetArrayBuffer(ctx, &src_size, ab);
		JS_FreeValue(ctx, ab);
		if (!src) {
			return JS_ThrowTypeError(
				ctx,
				"imageWriteRGBA: TypedArray's backing ArrayBuffer is unavailable");
		}
		src += src_offset;
		src_size = src_length;
	}

	size_t expected = (size_t)image->width * image->height * 4;
	if (src_size < expected) {
		return JS_ThrowRangeError(
			ctx, "imageWriteRGBA: buffer size %zu < expected %zu (%ux%u*4)",
			src_size, expected, image->width, image->height);
	}

	uint8_t *dst = image->data;
	size_t pixels = (size_t)image->width * image->height;
	for (size_t i = 0; i < pixels; i++) {
		uint8_t r = src[i * 4 + 0];
		uint8_t g = src[i * 4 + 1];
		uint8_t b = src[i * 4 + 2];
		uint8_t a = src[i * 4 + 3];
		if (a == 0) {
			dst[i * 4 + 0] = 0;
			dst[i * 4 + 1] = 0;
			dst[i * 4 + 2] = 0;
			dst[i * 4 + 3] = 0;
		} else if (a == 255) {
			dst[i * 4 + 0] = b;
			dst[i * 4 + 1] = g;
			dst[i * 4 + 2] = r;
			dst[i * 4 + 3] = a;
		} else {
			dst[i * 4 + 0] = (uint8_t)((b * a) / 255);
			dst[i * 4 + 1] = (uint8_t)((g * a) / 255);
			dst[i * 4 + 2] = (uint8_t)((r * a) / 255);
			dst[i * 4 + 3] = a;
		}
	}

	cairo_surface_mark_dirty(image->surface);
	return JS_UNDEFINED;
}

/* =========================================================================
 * Animation API — used by hosts that want to drive multi-frame images
 * (animated GIFs) themselves. The pattern is:
 *
 *   const n = Switch.imageFrameCount(img);   // 0 = not animated
 *   for (let i = 0; i < n; i++) {
 *     const delayMs = Switch.imageFrameDelay(img, i);
 *     setTimeout(() => { Switch.imageSetFrame(img, i); }, ...);
 *   }
 *
 * `imageSetFrame` does a single memcpy from the chosen frame into the
 * image's active framebuffer and marks the cairo surface dirty, so the
 * next `drawImage(img)` paints the new frame.
 * ========================================================================= */

static JSValue nx_image_frame_count(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) return JS_EXCEPTION;
	return JS_NewUint32(ctx, image->frame_count);
}

static JSValue nx_image_frame_delay(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) return JS_EXCEPTION;
	uint32_t idx = 0;
	if (JS_ToUint32(ctx, &idx, argv[1])) return JS_EXCEPTION;
	if (image->frame_count == 0 || !image->frame_delays_ms) {
		return JS_NewUint32(ctx, 0);
	}
	if (idx >= image->frame_count) {
		return JS_ThrowRangeError(ctx,
			"imageFrameDelay: frame index %u >= frame count %u",
			idx, image->frame_count);
	}
	return JS_NewUint32(ctx, image->frame_delays_ms[idx]);
}

static JSValue nx_image_set_frame(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) return JS_EXCEPTION;
	uint32_t idx = 0;
	if (JS_ToUint32(ctx, &idx, argv[1])) return JS_EXCEPTION;
	if (image->frame_count == 0 || !image->frames || !image->data) {
		return JS_UNDEFINED;  /* no-op for static images */
	}
	if (idx >= image->frame_count) {
		return JS_ThrowRangeError(ctx,
			"imageSetFrame: frame index %u >= frame count %u",
			idx, image->frame_count);
	}
	if (idx == image->current_frame) return JS_UNDEFINED;
	const size_t bgra_stride = (size_t)image->width * image->height * 4;
	memcpy(image->data, image->frames[idx], bgra_stride);
	image->current_frame = idx;
	if (image->surface) {
		cairo_surface_mark_dirty(image->surface);
	}
	return JS_UNDEFINED;
}

static void finalizer_image(JSRuntime *rt, JSValue val) {
	nx_image_t *image = JS_GetOpaque(val, nx_image_class_id);
	if (image) {
		close_image(rt, image);
		js_free_rt(rt, image);
	}
}

static JSValue nx_image_get_width(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, this_val);
	if (!image)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, image->width);
}

static JSValue nx_image_get_height(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, this_val);
	if (!image)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, image->height);
}

static JSValue nx_image_init_class(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "width", nx_image_get_width);
	NX_DEF_GET(proto, "height", nx_image_get_height);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("imageInit", 0, nx_image_init_class),
	JS_CFUNC_DEF("imageNew", 0, nx_image_new),
	JS_CFUNC_DEF("imageDecode", 0, nx_image_decode),
	JS_CFUNC_DEF("imageClose", 0, nx_image_close),
	JS_CFUNC_DEF("imageWriteRGBA", 2, nx_image_write_rgba),
	JS_CFUNC_DEF("imageFrameCount", 1, nx_image_frame_count),
	JS_CFUNC_DEF("imageFrameDelay", 2, nx_image_frame_delay),
	JS_CFUNC_DEF("imageSetFrame", 2, nx_image_set_frame),
};

void nx_init_image(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_image_class_id);
	JSClassDef image_class = {
		"Image",
		.finalizer = finalizer_image,
	};
	JS_NewClass(rt, nx_image_class_id, &image_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
