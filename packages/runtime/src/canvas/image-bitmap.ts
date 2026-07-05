import { $ } from '../$';
import { Blob } from '../polyfills/blob';
import { assertInternalConstructor, def, proto } from '../utils';
import { ImageData } from './image-data';
import { OffscreenCanvas } from './offscreen-canvas';
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

// Ledger #73 — extract flipY / unpremul flags from createImageBitmap options
// (both call-shape variants: 2-arg with options, 6-arg with options at the
// tail). WebGL 1 conformance's image_bitmap tests iterate 4 bitmap variants
// via `{imageOrientation: "none"|"flipY", premultiplyAlpha: "premultiply"|
// "none"}`; each variant must produce distinct pixel bytes. Pre-#73 all
// four variants came out identical (premul, no flip) because these options
// were ignored, which made the "flipY=true" and "premultiplyAlpha=none"
// iterations fail even though iteration 1 (flipY=false, premultiply) hit
// PASS after #72 landed.
interface BitmapOpts {
	flipY: boolean;
	unpremul: boolean;
}
function extractOpts(
	optionsOrSx: ImageBitmapOptions | number | undefined,
	tailOptions: ImageBitmapOptions | undefined,
): BitmapOpts {
	let opts: ImageBitmapOptions | undefined;
	if (typeof optionsOrSx === 'object' && optionsOrSx !== null) {
		opts = optionsOrSx;
	} else if (typeof tailOptions === 'object' && tailOptions !== null) {
		opts = tailOptions;
	}
	return {
		flipY: opts?.imageOrientation === 'flipY',
		unpremul: opts?.premultiplyAlpha === 'none',
	};
}

// #66 — internal helper: encode a canvas (Screen / OffscreenCanvas / any
// nx_canvas_t) to PNG bytes then round-trip through $.imageDecode into a
// fresh ImageBitmap. Every non-Blob source path funnels through this at
// the tail; keeping it in one place makes the encode/decode contract easy
// to audit. #73 extends the signature to honor createImageBitmap options:
// flipY draws the source into a scratch canvas mirrored on Y before
// encoding; unpremul skips the PNG round-trip (which always premultiplies
// on decode) and stores raw getImageData bytes via imageWriteRGBA with
// premultiply=false.
async function canvasToImageBitmap(
	canvas: unknown,
	opts: BitmapOpts = { flipY: false, unpremul: false },
): Promise<ImageBitmap> {
	const src = canvas as OffscreenCanvas;
	// Fast path: no options → unchanged encode/decode round-trip. Keeps
	// hot paths (drawImage callers not requesting options) at zero perf
	// cost — no getImageData copy, no scratch canvas allocation.
	if (!opts.flipY && !opts.unpremul) {
		// Ledger #82 — canvasToBuffer('image/png') can occasionally return
		// a zero-length or otherwise undecodable buffer (observed as
		// `$.imageDecode` throwing "Unsupported image format" on the first
		// from_canvas conformance variant in a run; subsequent identical
		// tests pass, so the root cause is a warm-up ordering issue not
		// yet root-caused). Instead of surfacing that as a
		// createImageBitmap rejection — which fails the whole test even
		// when the caller's expected pixel values are all-zero and an
		// empty bitmap would satisfy every check — fall through to the
		// same raw getImageData → imageWriteRGBA path the unpremul branch
		// below already uses. Byte-for-byte pixel copy, no PNG-encode
		// risk. Diag log gives a future investigator a marker to grep for
		// in nxjs-debug.log without breaking the hot-path zero-cost
		// property (log only fires on failure).
		try {
			const buf = await $.canvasToBuffer(src, 'image/png');
			const bmp = proto($.imageNew(), ImageBitmap);
			await $.imageDecode(bmp, buf);
			return bmp;
		} catch (e) {
			console.debug(
				'[image-bitmap:#82] fast-path encode/decode failed, ' +
					'falling back to raw getImageData: ' +
					(e as { message?: string })?.message,
			);
			const w = src.width;
			const h = src.height;
			const sctx = src.getContext('2d');
			if (!sctx) {
				throw new Error(
					'Failed to acquire 2D context for fast-path fallback',
				);
			}
			const bytes = sctx.getImageData(0, 0, w, h).data;
			const bmp = proto($.imageNew(w, h), ImageBitmap);
			// getImageData returns unpremultiplied RGBA; write with
			// premultiply=true to match the fast-path's canvas 2D storage
			// contract (premultiplied). Preserves the invariant that
			// non-option callers get a premul-stored ImageBitmap.
			$.imageWriteRGBA(bmp, bytes.buffer, true);
			return bmp;
		}
	}
	// Options path. If flipY, compose the source into a scratch canvas
	// with a Y-mirror transform. Then either encode+decode (premul stays)
	// or read raw bytes and store via imageWriteRGBA(premultiply=false).
	const w = src.width;
	const h = src.height;
	let staging: OffscreenCanvas = src;
	if (opts.flipY) {
		staging = new OffscreenCanvas(w, h);
		const sctx = staging.getContext('2d');
		if (!sctx) {
			throw new Error('Failed to acquire 2D context for flipY staging');
		}
		sctx.save();
		sctx.scale(1, -1);
		sctx.translate(0, -h);
		sctx.drawImage(src, 0, 0);
		sctx.restore();
	}
	if (!opts.unpremul) {
		const buf = await $.canvasToBuffer(staging, 'image/png');
		const bmp = proto($.imageNew(), ImageBitmap);
		await $.imageDecode(bmp, buf);
		return bmp;
	}
	// Unpremul path: getImageData returns unpremultiplied RGBA; imageDecode
	// would re-premultiply, so route around it via imageWriteRGBA with
	// premultiply=false (Ledger #73's engine-side companion to this shim).
	const sctx = staging.getContext('2d');
	if (!sctx) {
		throw new Error('Failed to acquire 2D context for unpremul readback');
	}
	const bytes = sctx.getImageData(0, 0, w, h).data;
	const bmp = proto($.imageNew(w, h), ImageBitmap);
	$.imageWriteRGBA(bmp, bytes.buffer, false);
	return bmp;
}

// #66 — helper: attempt to unwrap a canvas-like source down to a raw nx.js
// canvas (OffscreenCanvas / Screen) so `$.canvasToBuffer` accepts it.
//
// Handles three shapes:
//   1. Already an nx.js OffscreenCanvas -> return as-is.
//   2. Duck-typed as a live-DOM `<canvas>` LiveElement (brewser-runtime):
//      `tagName === 'CANVAS'` + `.offscreen` field lazily allocated on
//      `getContext('2d')`. We call `getContext('2d')` to force the
//      allocation, then return `.offscreen`.
//   3. Duck-typed convertToBlob-carrying (fallback) -> null so the caller
//      falls through to the Blob-route below.
//
// Returns null when nothing matches; the caller then tries other paths.
function tryUnwrapCanvas(image: unknown): OffscreenCanvas | null {
	if (image instanceof OffscreenCanvas) return image;
	if (
		image &&
		typeof image === 'object' &&
		(image as { tagName?: string }).tagName === 'CANVAS' &&
		typeof (image as { getContext?: unknown }).getContext === 'function'
	) {
		// Force the offscreen backing to exist (brewser-runtime's LiveElement
		// creates it lazily on first getContext call).
		try {
			(image as { getContext: (k: string) => unknown }).getContext('2d');
		} catch (_) {
			/* fall through */
		}
		const off = (image as { offscreen?: unknown }).offscreen;
		if (off instanceof OffscreenCanvas) return off;
	}
	return null;
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
	_optionsOrSx?: ImageBitmapOptions | number,
	_sy?: number,
	_sw?: number,
	_sh?: number,
	_options?: ImageBitmapOptions,
): Promise<ImageBitmap> {
	// Ledger #73 — extract flipY / unpremul from the caller's options and
	// thread them through the source-type branches below. Each branch that
	// ultimately calls canvasToImageBitmap now passes `opts` so the helper
	// can apply the transform (flipY at composite time, unpremul via the
	// raw imageWriteRGBA path). Sub-rect (sx/sy/sw/sh) still ignored —
	// deferred until a test needs it. See canvasToImageBitmap for the
	// per-option implementation strategy.
	const opts = extractOpts(_optionsOrSx, _options);
	// #66 — WebGL 1 conformance surfaced 40 tests (5 source-type clusters ×
	// 8 texture formats) that FAIL because the pre-#66 impl only handled
	// Blob and threw for every other spec-defined ImageBitmapSource. The
	// branches below cover the full ImageBitmapSource union:
	//   Blob             — existing decode path
	//   ImageData        — putImageData onto a scratch OffscreenCanvas, encode
	//   ImageBitmap      — drawImage onto a scratch canvas, encode
	//   HTMLImageElement — drawImage onto a scratch canvas, encode
	//   HTMLCanvasElement / OffscreenCanvas — encode source canvas directly
	//     (also covers brewser-runtime's live-DOM `<canvas>` LiveElement via
	//     duck-typed `.offscreen` unwrap in tryUnwrapCanvas)
	//   HTMLVideoElement — NOT supported here; drawImage's C++ impl doesn't
	//     accept video sources yet, so we throw a distinct error rather
	//     than silently producing an empty bitmap.

	// 1. Blob source — decode buffer directly. Fast path (no options)
	//    returns the decoded ImageBitmap as-is; options path routes
	//    through imageCopyPixels for premul-state conversion + Y-flip.
	//    Ledger #83 — pre-#83 the Blob branch ignored opts entirely,
	//    so `_from_blob` cluster iterations with `premultiplyAlpha:
	//    "none"` (alpha=0.5 red at 128,0,0 sampled as 255,0,0 expected
	//    → actual 128,0,0 was PREMULTIPLIED not un-multiplied) and
	//    `imageOrientation: "flipY"` (top pixel expected red, got
	//    green from the un-flipped source) FAIL post-#81b, once URL
	//    resolution was fixed and the PNG bytes actually arrived.
	//    The fix mirrors the ImageBitmap / HTMLImageElement branches
	//    below (both added by #78): decode into a temp, then
	//    imageCopyPixels(dst, tmp, dstPremultiply, flipY) does the
	//    BGRA-byte copy with premul conversion + optional Y-flip.
	if (image instanceof Blob) {
		const buf = await image.arrayBuffer();
		const decoded = proto($.imageNew(), ImageBitmap);
		await $.imageDecode(decoded, buf);
		if (!opts.flipY && !opts.unpremul) {
			return decoded;
		}
		const w = decoded.width;
		const h = decoded.height;
		const bmp = proto($.imageNew(w, h), ImageBitmap);
		$.imageCopyPixels(bmp, decoded, !opts.unpremul, opts.flipY);
		return bmp;
	}

	// 2. Canvas-like source — nx.js OffscreenCanvas / Screen / live-DOM
	//    `<canvas>` LiveElement. Encode the source canvas directly via
	//    $.canvasToBuffer (which is what OffscreenCanvas.convertToBlob and
	//    Screen.toBlob both use internally), then decode.
	const unwrappedCanvas = tryUnwrapCanvas(image);
	if (unwrappedCanvas) {
		return canvasToImageBitmap(unwrappedCanvas, opts);
	}

	// 3. ImageData source — Ledger #78. Bypass the canvas round-trip and
	//    write pixels straight to the bitmap via imageWriteRGBA (premultiply
	//    flag = !opts.unpremul). The pre-#78 path went through OffscreenCanvas
	//    + putImageData + getImageData, which zeroes RGB channels of any
	//    alpha=0 pixel — canvas 2D storage is premul and (r, g, b, 0)
	//    premul = (0, 0, 0, 0). WebGL 1 image_bitmap conformance's
	//    `_from_image_data-*` tests uses ImageData with (255, 0, 0, 0) at
	//    tr / br positions and expects them preserved through the
	//    `premultiplyAlpha: "none"` round-trip; the canvas path failed
	//    those checks. flipY is handled by pre-flipping the ImageData
	//    bytes in JS (small Uint8ClampedArray → Uint8Array copy).
	if (image instanceof ImageData) {
		if (!(image.width > 0 && image.height > 0)) {
			throw new Error('ImageData source has zero dimensions');
		}
		const w = image.width;
		const h = image.height;
		const bmp = proto($.imageNew(w, h), ImageBitmap);
		let bytes: Uint8Array | Uint8ClampedArray = image.data;
		if (opts.flipY) {
			const flipped = new Uint8Array(w * h * 4);
			const stride = w * 4;
			for (let y = 0; y < h; y++) {
				const srcOffset = (h - 1 - y) * stride;
				const dstOffset = y * stride;
				for (let x = 0; x < stride; x++) {
					flipped[dstOffset + x] = image.data[srcOffset + x];
				}
			}
			bytes = flipped;
		}
		$.imageWriteRGBA(bmp, bytes, !opts.unpremul);
		return bmp;
	}

	// 4. Draw-able source — HTMLImageElement (nx.js Image), ImageBitmap,
	//    OR anything else with numeric width/height that the canvas.cc
	//    drawImage native accepts. We route through a scratch OffscreenCanvas
	//    of the source's intrinsic size so the round-trip preserves the
	//    caller's dimensions verbatim.
	//
	//    We check the ImageBitmap branch here (rather than instanceof-ing
	//    it above alongside Blob) because drawImage(bitmap, 0, 0) is the
	//    natural copy path: rebuilding the pixel bytes off the source is
	//    what canvas.cc's SkImage cache already does for every draw.
	// Ledger #78 — ImageBitmap source. Bypass the drawImage-onto-canvas
	// round-trip via imageCopyPixels (engine-side BGRA byte copy with
	// premul-state conversion + optional Y-flip). The pre-#78 canvas path
	// destroyed alpha=0 pixels' RGB channels for the same reason the
	// ImageData branch above described. WebGL 1 image_bitmap conformance's
	// `_from_image_bitmap-*` tests use a source ImageBitmap that was
	// itself created from an ImageData with alpha=0 pixels (via the fixed
	// ImageData branch), so this branch also needs to preserve those.
	if (image instanceof ImageBitmap) {
		const w = image.width;
		const h = image.height;
		if (!(w > 0 && h > 0)) {
			throw new Error('ImageBitmap source has zero dimensions');
		}
		const bmp = proto($.imageNew(w, h), ImageBitmap);
		$.imageCopyPixels(bmp, image, !opts.unpremul, opts.flipY);
		return bmp;
	}

	// 5. HTMLImageElement (nx.js Image). Duck-type on `naturalWidth` +
	//    `naturalHeight` to avoid a hard `Image` import (Image drags the
	//    fetch polyfills in, keeping this branch import-light matters
	//    since createImageBitmap can be called from Worker-style contexts
	//    where those aren't loaded).
	const asImage = image as {
		naturalWidth?: number;
		naturalHeight?: number;
		width?: number;
		height?: number;
	};
	if (
		typeof asImage.naturalWidth === 'number' &&
		typeof asImage.naturalHeight === 'number'
	) {
		const w = asImage.naturalWidth || asImage.width || 0;
		const h = asImage.naturalHeight || asImage.height || 0;
		if (!(w > 0 && h > 0)) {
			throw new Error('Image source has zero dimensions (still loading?)');
		}
		// Ledger #78 — nx.js Image is nx_image_t under the hood; the
		// engine-side imageCopyPixels reads its BGRA data + premul flag
		// directly. Bypasses the same canvas round-trip data-loss as the
		// ImageBitmap branch above.
		const bmp = proto($.imageNew(w, h), ImageBitmap);
		$.imageCopyPixels(bmp, image, !opts.unpremul, opts.flipY);
		return bmp;
	}

	// 6. HTMLVideoElement — deliberately not supported yet. drawImage's
	//    C++ impl doesn't accept video sources (only nx_image_t and
	//    nx_canvas_t), so a canvas round-trip would just throw at the
	//    drawImage call with a confusing "Image or Canvas expected"
	//    message. Report the actual gap here instead.
	const asVideo = image as { videoWidth?: number };
	if (typeof asVideo.videoWidth === 'number') {
		throw new Error(
			'createImageBitmap: HTMLVideoElement source not yet supported ' +
				'(needs canvas.cc drawImage video-frame-capture path)',
		);
	}

	throw new Error(
		`Unsupported image source: ${(image as { constructor?: { name?: string } })
			?.constructor?.name ?? typeof image}`,
	);
}
def(createImageBitmap);
