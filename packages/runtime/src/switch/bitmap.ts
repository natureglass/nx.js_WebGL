import { $ } from '../$';
import { ImageBitmap } from '../canvas/image-bitmap';
import { proto } from '../utils';

/**
 * Allocate an `ImageBitmap` of the given size and copy raw RGBA8 pixel bytes
 * into its backing buffer with the BGRA-premultiplied swizzle Skia expects.
 * The returned bitmap can be drawn via `ctx.drawImage(bitmap, x, y)` like
 * any other bitmap source — and, unlike a raw `putImageData` blit, is
 * scaled by `ctx.drawImage(bitmap, sx, sy, sw, sh, dx, dy, dw, dh)`.
 *
 * Intended for sources that produce raw pixel data per frame (decoded
 * video, generated buffers, etc.). Allocating once and refreshing pixels
 * via {@link writeRGBAToBitmap} avoids the per-frame allocation cost of
 * recreating the bitmap.
 *
 * `bytes` must contain at least `width * height * 4` bytes in R, G, B, A
 * byte order.
 *
 * @example
 * ```ts
 * const frame = dec.nextFrame();
 * if (frame?.data) {
 *   const bmp = Switch.createBitmapFromRGBA(frame.data, frame.width, frame.height);
 *   ctx.drawImage(bmp, 0, 0);
 * }
 * ```
 */
export function createBitmapFromRGBA(
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
	width: number,
	height: number,
): ImageBitmap {
	const bmp = proto($.imageNew(width, height), ImageBitmap);
	$.imageWriteRGBA(bmp, bytes);
	return bmp;
}

/**
 * Overwrite the pixels of an existing `ImageBitmap` (one that was previously
 * created via {@link createBitmapFromRGBA}) with fresh RGBA8 bytes. The
 * bitmap's dimensions are unchanged; `bytes` must contain at least
 * `bitmap.width * bitmap.height * 4` bytes.
 *
 * Use this in tight per-frame loops to avoid reallocating the bitmap's
 * backing buffer every frame.
 */
export function writeRGBAToBitmap(
	bitmap: ImageBitmap,
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
): void {
	$.imageWriteRGBA(bitmap, bytes);
}

/**
 * Fix D (2026-09-05): BGRA counterparts to {@link createBitmapFromRGBA} /
 * {@link writeRGBAToBitmap} for sources that ALREADY produce BGRA in Skia's
 * ARGB32 memory order (decoded video frames from `Switch.VideoDecoder`'s
 * `nextFrame(true)`). `imageWriteBGRA` is a straight memcpy — no per-pixel
 * R↔B swap — so a decoded frame reaches the screen without the
 * BGRA→RGBA→BGRA double swizzle the RGBA path performs each frame.
 *
 * `bytes` must contain at least `width * height * 4` bytes in B, G, R, A
 * byte order (opaque, alpha 255).
 */
export function createBitmapFromBGRA(
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
	width: number,
	height: number,
): ImageBitmap {
	const bmp = proto($.imageNew(width, height), ImageBitmap);
	$.imageWriteBGRA(bmp, bytes);
	return bmp;
}

export function writeBGRAToBitmap(
	bitmap: ImageBitmap,
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
): void {
	$.imageWriteBGRA(bitmap, bytes);
}

/**
 * 2026-09-06 — planar-I420 counterparts for decoded video frames delivered by
 * `Switch.VideoDecoder`'s `nextFrame()` when the decoder was opened with
 * `{ yuv: true }`. `bytes` is contiguous Y|U|V (1.5 B/px). The bitmap is
 * marked YUV so `ctx.drawImage(bitmap, …)` builds a GPU YUVA image and does
 * YUV→RGB in the shader — uploading ~2.6× less per frame than BGRA, which is
 * the dominant per-frame cost on the Switch's Mesa-nouveau GL.
 *
 * The backing `imageNew(width, height)` allocation (width*height*4) comfortably
 * holds the 1.5*width*height I420, so per-frame refresh via
 * {@link writeYUVToBitmap} never reallocates.
 *
 * `colorSpace`: 0=Rec709 limited, 1=Rec601 limited, 2=full/JPEG, 3=Rec709 full.
 */
export function createBitmapFromYUV(
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
	width: number,
	height: number,
	colorSpace?: number,
): ImageBitmap {
	const bmp = proto($.imageNew(width, height), ImageBitmap);
	$.imageWriteYUV(bmp, bytes, width, height, colorSpace);
	return bmp;
}

export function writeYUVToBitmap(
	bitmap: ImageBitmap,
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
	width: number,
	height: number,
	colorSpace?: number,
): void {
	$.imageWriteYUV(bitmap, bytes, width, height, colorSpace);
}

/**
 * 2026-09-06 — set the GPU present vsync divisor: `1` = 60 Hz (default),
 * `2` = 30 Hz. The shell pins this to 2 while a fullscreen video is the sole
 * thing on screen so 30 fps content gets a clean, judder-free 1:1 cadence
 * (each frame shown for exactly two refreshes / one 30 Hz tick) instead of the
 * uneven ~40 fps the free-running loop yields. No-op on the raster fallback.
 * Exposed on `Switch` via `switch/index.ts`'s `export * from './bitmap'`.
 */
export function gfxSetSwapInterval(interval: number): void {
	$.gfxSetSwapInterval(interval);
}
