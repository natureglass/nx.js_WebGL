/*
 * gifdec — minimal GIF89a decoder (lecram/gifdec, public domain).
 *
 * Vendored 2026-05-31 for nx.js. Single addition vs upstream: a
 * memory-backed open path. Switch builds receive image data as an
 * already-decoded ArrayBuffer from the runtime fetch (libcurl) — there
 * is no temp filesystem to write through — so the in-memory reader is
 * how the nx.js image pipeline calls into this library.
 *
 * Upstream:        https://github.com/lecram/gifdec
 * Upstream license: public domain (no warranty)
 */
#ifndef GIFDEC_H
#define GIFDEC_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gd_Palette {
	int size;
	uint8_t colors[0x100 * 3];
} gd_Palette;

typedef struct gd_GCE {
	uint16_t delay;
	uint8_t tindex;
	uint8_t disposal;
	int input;
	int transparency;
} gd_GCE;

typedef struct gd_GIF {
	/* File backing. -1 when the GIF was opened from memory; see `mem`. */
	int fd;
	/* Memory backing. NULL when file-backed. When non-NULL, `read`/
	 * `lseek` calls inside gifdec.c are routed through `gd_read` /
	 * `gd_seek`, which operate on `[mem, mem + mem_size)` with a
	 * `mem_pos` cursor instead of hitting the filesystem. */
	const uint8_t *mem;
	size_t mem_size;
	size_t mem_pos;
	off_t anim_start;
	uint16_t width, height;
	uint16_t depth;
	uint16_t loop_count;
	gd_GCE gce;
	gd_Palette *palette;
	gd_Palette lct, gct;
	void (*plain_text)(
		struct gd_GIF *gif, uint16_t tx, uint16_t ty,
		uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
		uint8_t fg, uint8_t bg
	);
	void (*comment)(struct gd_GIF *gif);
	void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
	uint16_t fx, fy, fw, fh;
	uint8_t bgindex;
	uint8_t *canvas, *frame;
} gd_GIF;

gd_GIF *gd_open_gif(const char *fname);
/** Memory-backed open: parse a GIF from `[buf, buf + size)`. The buffer
 * must remain valid for the lifetime of the returned `gd_GIF *` —
 * gifdec does not copy it. */
gd_GIF *gd_open_gif_mem(const uint8_t *buf, size_t size);
int gd_get_frame(gd_GIF *gif);
void gd_render_frame(gd_GIF *gif, uint8_t *buffer);
int gd_is_bgcolor(gd_GIF *gif, uint8_t color[3]);
void gd_rewind(gd_GIF *gif);
void gd_close_gif(gd_GIF *gif);

#ifdef __cplusplus
}
#endif

#endif /* GIFDEC_H */
