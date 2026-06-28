import { $ } from '../$';
import { Blob } from '../polyfills/blob';
import { Image } from '../image';
import { ImageData } from './image-data';
import { OffscreenCanvas } from './offscreen-canvas';
import { assertInternalConstructor, def, proto } from '../utils';
import type {
	ColorSpaceConversion,
	ImageOrientation,
	PremultiplyAlpha,
	ResizeQuality,
	ImageBitmapSource,
} from '../types';

/**
 * Represents a bitmap image which can be drawn to a `<canvas>` without undue latency.
 * It can be created from a variety of source objects using the
 * {@link createImageBitmap | `createImageBitmap()`} function.
 *
 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap
 */
export class ImageBitmap implements globalThis.ImageBitmap {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * Read-only property containing the height of the `ImageBitmap` in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/height
	 */
	declare height: number;

	/**
	 * Read-only property containing the width of the `ImageBitmap` in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/width
	 */
	declare width: number;

	/**
	 * Disposes of all graphical resources associated with the `ImageBitmap`.
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/close
	 */
	close(): void {
		$.imageClose(this);
	}
}
$.imageInit(ImageBitmap);
def(ImageBitmap);

export interface ImageBitmapOptions {
	colorSpaceConversion?: ColorSpaceConversion;
	imageOrientation?: ImageOrientation;
	premultiplyAlpha?: PremultiplyAlpha;
	resizeHeight?: number;
	resizeQuality?: ResizeQuality;
	resizeWidth?: number;
}

/**
 * Creates a bitmap from a given source, optionally cropped to contain only
 * a portion of that source. This function accepts a variety of different
 * image sources, and returns a `Promise` which resolves to an {@link ImageBitmap}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/createImageBitmap
 */
export function createImageBitmap(
	image: ImageBitmapSource,
	options?: ImageBitmapOptions,
): Promise<ImageBitmap>;
export function createImageBitmap(
	image: ImageBitmapSource,
	sx: number,
	sy: number,
	sw: number,
	sh: number,
	options?: ImageBitmapOptions,
): Promise<ImageBitmap>;
export async function createImageBitmap(
	image: ImageBitmapSource,
	optionsOrSx?: ImageBitmapOptions | number,
	sy?: number,
	sw?: number,
	sh?: number,
	options?: ImageBitmapOptions,
): Promise<ImageBitmap> {
	// Resolve the ImageBitmapOptions arg from either overload position.
	const opts: ImageBitmapOptions | undefined =
		typeof optionsOrSx === 'object' ? optionsOrSx : options;
	const applyOptions = (bm: ImageBitmap): ImageBitmap => {
		// Map options → ($.imageSetBitmapOptions's 3-arg surface):
		//   imageOrientation === 'flipY' → flipY=true
		//   premultiplyAlpha === 'none' → alphaMode=1 (STRAIGHT)
		//   premultiplyAlpha === 'premultiply'|'default' → alphaMode=2 (PREMULTIPLIED)
		//   No opts supplied → alphaMode=0 (UNSPECIFIED, legacy behavior)
		// When the test calls createImageBitmap WITHOUT options the bitmap
		// retains UNSPECIFIED mode (alphaMode=0), preserving the historical
		// extractor behavior for callers that never adopted the options.
		if (!opts) return bm;
		const flipY = opts.imageOrientation === 'flipY';
		let alphaMode = 0;
		if (opts.premultiplyAlpha === 'none') alphaMode = 1;
		else if (opts.premultiplyAlpha === 'premultiply'
			|| opts.premultiplyAlpha === 'default') alphaMode = 2;
		$.imageSetBitmapOptions(bm, flipY, alphaMode);
		return bm;
	};
	if (image instanceof Blob) {
		const buf = await image.arrayBuffer();
		const img = proto($.imageNew(), ImageBitmap);
		await $.imageDecode(img, buf);
		return applyOptions(img);
	}
	// Canvas duck-type: any source with a spec-style `convertToBlob` (nxjs
	// OffscreenCanvas, swb LiveElement canvas, HTMLCanvasElement) is encoded
	// to a Blob first, then decoded via the Blob path. Tier-1 — no native
	// canvas→ImageBitmap fast path yet; cost is one PNG encode + decode.
	if (image && typeof (image as { convertToBlob?: unknown }).convertToBlob === 'function') {
		const blob = await (image as { convertToBlob: () => Promise<Blob> }).convertToBlob();
		const buf = await blob.arrayBuffer();
		const img = proto($.imageNew(), ImageBitmap);
		await $.imageDecode(img, buf);
		return applyOptions(img);
	}
	// 2026-06-26 Layer 2: Image / ImageBitmap / ImageData source extensions
	// for the conformance `image_bitmap_from_image`, `from_image_bitmap`,
	// and `from_image_data` test clusters.
	//
	// 2026-06-27 Layer 2 NATIVE FAST PATH: bypass the OffscreenCanvas →
	// drawImage/putImageData → convertToBlob (PNG encode) → arrayBuffer →
	// $.imageDecode (PNG decode) roundtrip for the three source types whose
	// pixel data is already in canonical RGBA-or-cairo-BGRA-premul form.
	// Removes a ~264KB PNG encode + decode per call (measured: ~150-200ms
	// per createImageBitmap on 257×257 sources), restoring per-test
	// runtime below the conformance runner's 320ms grace window. Byte-
	// equivalence to the prior roundtrip path: identical for the
	// conformance tests' canonical {0, 64, 128, 192, 255} alpha values
	// (both paths round-trip cleanly through 8-bit premul math), within
	// ±1 channel for unusual alphas (test tolerance is 10). See
	// REAL_GL_FAILURES.md "Layer-2 fast path byte-equivalence" subsection
	// for the full analysis. KEEP the convertToBlob roundtrip for canvas/
	// OffscreenCanvas sources further up (they don't expose a clean RGBA
	// buffer cheaply; that path is empirically exonerated via the WebGL1
	// image_bitmap_from_canvas verdicts).
	if (image instanceof Image || image instanceof ImageBitmap) {
		// Try native clone first. Returns null when the source hasn't
		// realized its pixel buffer (decode-in-flight, or constructed
		// via $.imageNew() with no dims), in which case fall back to the
		// roundtrip so the polyfill remains spec-permissible.
		const cloned = $.imageCloneFrom(image as Image | ImageBitmap);
		if (cloned) {
			return applyOptions(proto(cloned, ImageBitmap));
		}
		// Fallback: the source isn't realized yet — go through the
		// convertToBlob roundtrip. This path is unchanged from the
		// 2026-06-26 polyfill landing.
		const w = (image as { width: number }).width;
		const h = (image as { height: number }).height;
		const oc = new OffscreenCanvas(w, h);
		const ctx = oc.getContext('2d');
		(ctx as unknown as { drawImage: (i: unknown, x: number, y: number) => void })
			.drawImage(image, 0, 0);
		const blob = await oc.convertToBlob();
		const buf = await blob.arrayBuffer();
		const img = proto($.imageNew(), ImageBitmap);
		await $.imageDecode(img, buf);
		return applyOptions(img);
	}
	if (image instanceof ImageData) {
		// Fast path: allocate the ImageBitmap with the source's dims so
		// the C-side `$.imageNew(w, h)` gives us a backing buffer + cairo
		// surface, then push the source's straight-RGBA8 bytes through
		// `$.imageWriteRGBA` which already does the straight → BGRA
		// premultiplied swizzle (the same conversion the prior roundtrip
		// achieved via putImageData → convertToBlob → decode, just
		// without the PNG-encode round-trip cost).
		//
		// `preserveStraight=true`: also stash the original straight bytes
		// so the WebGL extractor can deliver lossless STRAIGHT pixels for
		// `premultiplyAlpha:'none'` bitmaps. Without this, source pixels
		// with a=0 round-trip through premul-of-(r,g,b,0)=(0,0,0,0) and
		// can't be recovered to (r,g,b) — the conformance from_image_data
		// and from_image_bitmap rgba-ubyte tests use exactly this pattern
		// (alpha=0 right column) and required the preserved bytes to
		// match the spec's `premultiplyAlpha:'none'` semantic.
		const bm = proto($.imageNew(image.width, image.height), ImageBitmap);
		$.imageWriteRGBA(bm, image.data, true);
		return applyOptions(bm);
	}
	throw new Error(`Unsupported image source: ${image.constructor.name}`);
}
def(createImageBitmap);
