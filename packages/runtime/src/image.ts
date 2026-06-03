import { $ } from './$';
import { createInternal, def, proto } from './utils';
import { fetch } from './fetch/fetch';
import { URL } from './polyfills/url';
import { Event, ErrorEvent } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';

interface ImageInternal {
	complete: boolean;
	src?: URL;
}

const _ = createInternal<Image, ImageInternal>();

/**
 * The `Image` class is the spiritual equivalent of the [`HTMLImageElement`](https://developer.mozilla.org/docs/Web/API/HTMLImageElement)
 * class in web browsers. You can use it to load image data from the filesytem
 * or remote source over the network. Once loaded, the image may be drawn onto the screen
 * context or an offscreen canvas context using {@link CanvasRenderingContext2D.drawImage | `ctx.drawImage()`}.
 *
 * ### Supported Image Formats
 *
 *  - `jpg` - JPEG image data using [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)
 *  - `png` - PNG image data using [libpng](http://www.libpng.org/pub/png/libpng.html)
 *  - `webp` - WebP image data using [libpng](https://github.com/webmproject/libwebp)
 *  - `gif` - GIF image data using [gifdec](https://github.com/lecram/gifdec); animated GIFs
 *    are decoded into a multi-frame array and the host advances frames via
 *    {@link Image.frameCount}, {@link Image.frameDelay}, {@link Image.setFrame}.
 *
 * @example
 *
 * ```typescript
 * const ctx = screen.getContext('2d');
 *
 * const img = new Image();
 * img.addEventListener('load', () => {
 *   ctx.drawImage(img);
 * });
 * img.src = 'romfs:/logo.png';
 * ```
 */
export class Image extends EventTarget {
	declare onload: ((this: Image, ev: Event) => any) | null;
	declare onerror: ((this: Image, ev: ErrorEvent) => any) | null;
	declare decoding: 'async' | 'sync' | 'auto';
	declare isMap: boolean;
	declare loading: 'eager' | 'lazy';
	declare readonly width: number;
	declare readonly height: number;

	constructor() {
		super();
		const i = proto($.imageNew(), Image);
		i.onload = null;
		i.onerror = null;
		i.decoding = 'auto';
		i.isMap = false;
		i.loading = 'eager';
		_.set(i, { complete: true });
		return i;
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'load') {
			this.onload?.(event);
		} else if (event.type === 'error') {
			this.onerror?.(event as ErrorEvent);
		}
		return super.dispatchEvent(event);
	}

	get complete() {
		return _(this).complete;
	}

	get naturalWidth() {
		return this.width;
	}

	get naturalHeight() {
		return this.height;
	}

	get src() {
		return _(this).src?.href ?? '';
	}

	set src(val: string) {
		const url = new URL(val, $.entrypoint);
		const internal = _(this);
		internal.src = url;
		internal.complete = false;
		fetch(url)
			.then((res) => {
				if (!res.ok) {
					throw new Error(`Failed to load image: ${res.status}`);
				}
				return res.arrayBuffer();
			})
			.then((buf) => $.imageDecode(this, buf))
			.then(
				() => {
					internal.complete = true;
					// `dispatchEvent` has no try/catch around listeners
					// (see EventTarget impl); a sync throw inside an
					// `onload` callback escapes here and rejects the
					// chained promise → unhandled-rejection → text-fatal.
					// Common trigger: a consumer's `onload` patches a
					// LiveElement that was already removed from the DOM
					// between `src` assignment and decode completion
					// (e.g. `innerHTML = ''` ran in between). The catch
					// only suppresses the chain bookkeeping; sync
					// listener throws still propagate at their call site.
					try {
						this.dispatchEvent(new Event('load'));
					} catch (_e) {}
				},
				(error) => {
					internal.complete = false;
					try {
						this.dispatchEvent(new ErrorEvent('error', { error }));
					} catch (_e) {}
				},
			);
	}

	// Compat with HTML DOM interface
	className = '';
	get nodeType() {
		return 1;
	}
	get nodeName() {
		return 'IMG';
	}
	getAttribute(name: string): string | null {
		if (name === 'width') return String(this.width);
		if (name === 'height') return String(this.height);
		return null;
	}
	setAttribute(name: string, value: string | number) {}

	/**
	 * Number of frames in a multi-frame image (animated GIF). Returns 0
	 * for static images (PNG/JPEG/WebP/single-frame GIF). Used together
	 * with {@link frameDelay} and {@link setFrame} to drive animation
	 * from JavaScript.
	 */
	get frameCount(): number {
		return $.imageFrameCount(this);
	}

	/**
	 * The display time of the frame at `index`, in milliseconds. Throws
	 * `RangeError` if `index >= frameCount`. Returns 0 for static images.
	 */
	frameDelay(index: number): number {
		return $.imageFrameDelay(this, index);
	}

	/**
	 * Switch the cairo surface to render the frame at `index`. The next
	 * `drawImage(img, ...)` call paints that frame. No-op for static
	 * images; throws `RangeError` if `index >= frameCount`.
	 */
	setFrame(index: number): void {
		$.imageSetFrame(this, index);
	}
}
$.imageInit(Image);
def(Image);
