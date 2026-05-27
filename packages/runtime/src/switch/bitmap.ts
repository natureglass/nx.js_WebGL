import { $ } from '../$';
import { ImageBitmap } from '../canvas/image-bitmap';
import { proto } from '../utils';

/**
 * Allocate an `ImageBitmap` of the given size and copy raw RGBA8 pixel
 * bytes into its backing buffer with the BGRA-premultiplied swizzle Cairo
 * expects. The returned bitmap can be drawn via
 * `ctx.drawImage(bitmap, x, y)` like any other bitmap source.
 *
 * Intended for sources that produce raw pixel data per frame (decoded
 * video, generated buffers, etc.). Allocating once and refreshing pixels
 * via {@link writeRGBAToBitmap} avoids the per-frame allocation cost of
 * recreating the bitmap.
 *
 * `bytes` must contain at least `width * height * 4` bytes in
 * R, G, B, A byte order. Accepts an `ArrayBuffer` (e.g. from
 * `Switch.VideoDecoder.nextFrame().data`) or any `TypedArray` view onto
 * one (`Uint8Array` / `Uint8ClampedArray` / etc.).
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
 * Overwrite the pixels of an existing `ImageBitmap` (one that was
 * previously created via {@link createBitmapFromRGBA}) with fresh RGBA8
 * bytes. The bitmap's dimensions are unchanged; `bytes` must contain at
 * least `bitmap.width * bitmap.height * 4` bytes.
 *
 * Use this in tight per-frame loops to avoid reallocating the bitmap's
 * backing buffer and Cairo surface every frame.
 */
export function writeRGBAToBitmap(
	bitmap: ImageBitmap,
	bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
): void {
	$.imageWriteRGBA(bitmap, bytes);
}
