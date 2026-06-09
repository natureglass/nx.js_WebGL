import { $ } from './$';
import {
	assertInternalConstructor,
	createInternal,
	def,
	normalizeImageMime,
	proto,
	stub,
} from './utils';
import { Blob } from './polyfills/blob';
import { EventTarget } from './polyfills/event-target';
import { INTERNAL_SYMBOL } from './internal';
import { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';
import {
	WebGLRenderingContext,
	WebGL2RenderingContext,
} from './canvas/webgl-rendering-context';
import { initTouchscreen } from './touchscreen';
import type { TouchEvent } from './polyfills/event';

interface ScreenInternal {
	context2d?: CanvasRenderingContext2D;
	webgl?: WebGLRenderingContext;
	webgl2?: WebGL2RenderingContext;
}

const _ = createInternal<Screen, ScreenInternal>();

export class Screen extends EventTarget implements globalThis.Screen {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		const c = proto($.canvasNew(1280, 720), Screen);
		_.set(c, {});
		return c;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/availWidth) */
	get availWidth() {
		return this.width;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/availHeight) */
	get availHeight() {
		return this.height;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/colorDepth) */
	get colorDepth() {
		return 24;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/orientation) */
	get orientation(): ScreenOrientation {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/pixelDepth) */
	get pixelDepth() {
		return 24;
	}

	/**
	 * The width of the screen in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Screen/width
	 */
	declare readonly width: number;

	/**
	 * The height of the screen in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Screen/height
	 */
	declare readonly height: number;

	getContext(contextId: '2d'): CanvasRenderingContext2D;
	getContext(contextId: 'webgl' | 'experimental-webgl'): WebGLRenderingContext;
	getContext(contextId: 'webgl2'): WebGL2RenderingContext;
	getContext(contextId: string): CanvasRenderingContext2D | WebGLRenderingContext | WebGL2RenderingContext | null {
		if (
			contextId !== '2d' &&
			contextId !== 'webgl' &&
			contextId !== 'experimental-webgl' &&
			contextId !== 'webgl2'
		) {
			return null;
		}

		const i = _(this);
		if (contextId === 'webgl2') {
			// The Screen canvas is the shared underlying surface
			// (see [[swb-webgl-inline]]) — different inline canvases on
			// the page may ask for different context kinds. Both v1 and
			// v2 instances wrap the SAME native EGL/GLES context; the
			// only difference is the JS class identity (which gates
			// Three.js's `gl.constructor.name === 'WebGL2RenderingContext'`
			// detection). The spec's "one kind per canvas" rule applies
			// to the inline canvas — enforced there by the runner — not
			// at this layer.
			if (!i.webgl2) {
				i.webgl2 = new WebGL2RenderingContext(
					// @ts-expect-error Internal constructor
					INTERNAL_SYMBOL,
					this,
				);
			}
			return i.webgl2;
		}
		if (contextId === 'webgl' || contextId === 'experimental-webgl') {
			if (!i.webgl) {
				i.webgl = new WebGLRenderingContext(
					// @ts-expect-error Internal constructor
					INTERNAL_SYMBOL,
					this,
				);
			}
			return i.webgl;
		}

		if (!i.context2d) {
			i.context2d = new CanvasRenderingContext2D(
				// @ts-expect-error Internal constructor
				INTERNAL_SYMBOL,
				this,
			);

			$.framebufferInit(this);
		}

		return i.context2d;
	}

	// @ts-expect-error
	addEventListener(
		type: 'touchstart' | 'touchmove' | 'touchend',
		listener: (ev: TouchEvent) => any,
		options?: boolean | AddEventListenerOptions,
	): void;
	addEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | AddEventListenerOptions,
	): void;
	addEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: boolean | AddEventListenerOptions,
	): void {
		if (type === 'touchstart' || type === 'touchmove' || type === 'touchend') {
			initTouchscreen();
		}
		super.addEventListener(type, callback, options);
	}

	/**
	 * Creates a {@link Blob} object representing the image contained on the screen.
	 *
	 * @example
	 *
	 * ```typescript
	 * screen.toBlob(blob => {
	 *   blob.arrayBuffer().then(buffer => {
	 *     Switch.writeFileSync('out.png', buffer);
	 *   });
	 * });
	 * ```
	 *
	 * @param callback A callback function with the resulting {@link Blob} object as a single argument. `null` may be passed if the image cannot be created for any reason.
	 * @param type A string indicating the image format. The default type is `image/png`. This image format will be also used if the specified type is not supported.
	 * @param quality A number between `0` and `1` indicating the image quality to be used when creating images using file formats that support lossy compression (such as `image/jpeg`). A user agent will use its default quality value if this option is not specified, or if the number is outside the allowed range.
	 */
	toBlob(
		callback: (blob: Blob | null) => void,
		type = 'image/png',
		quality = 0.92,
	) {
		$.canvasToBuffer(this, type, quality).then((buf: ArrayBuffer) => {
			callback(new Blob([buf], { type: normalizeImageMime(type) }));
		});
	}

	/**
	 * Returns a `data:` URL containing a representation of the image in the format specified by the type parameter.
	 *
	 * @example
	 *
	 * ```typescript
	 * const url = screen.toDataURL();
	 * fetch(url)
	 *   .then(res => res.arrayBuffer())
	 *   .then(buffer => {
	 *     Switch.writeFileSync('out.png', buffer);
	 *   });
	 * ```
	 *
	 * @param type A string indicating the image format. The default type is `image/png`. This image format will be also used if the specified type is not supported.
	 * @param quality A number between `0` and `1` indicating the image quality to be used when creating images using file formats that support lossy compression (such as `image/jpeg`). The default quality value will be used if this option is not specified, or if the number is outside the allowed range.
	 * @see https://developer.mozilla.org/docs/Web/API/HTMLCanvasElement/toDataURL
	 */
	toDataURL(type = 'image/png', quality = 0.92): string {
		stub();
	}

	/**
	 * Register a cursor bitmap that the engine composites onto the
	 * display buffer at present time. The cursor visual NEVER lands in
	 * the page's canvas->data — this is the engine-side replacement for
	 * the JS dirty-rect save/restore approach.
	 *
	 * @param x cursor top-left x in framebuffer pixels.
	 * @param y cursor top-left y in framebuffer pixels.
	 * @param rgba Non-premultiplied RGBA8 bitmap, w*h*4 bytes. Standard
	 *   `ImageData.data` layout. Passing a smaller buffer throws.
	 * @param w bitmap width in pixels.
	 * @param h bitmap height in pixels.
	 */
	setCursorOverlay(
		x: number,
		y: number,
		rgba: ArrayBuffer | ArrayBufferView,
		w: number,
		h: number,
	): void {
		$.setCursorOverlay(x, y, rgba, w, h);
	}

	/**
	 * Cheap position-only update for the cursor overlay. Avoids re-copying
	 * the bitmap when the user has only moved the cursor by a pixel.
	 */
	setCursorOverlayPosition(x: number, y: number): void {
		$.setCursorOverlayPosition(x, y);
	}

	/** Disable the cursor overlay until the next `setCursorOverlay` call. */
	clearCursorOverlay(): void {
		$.clearCursorOverlay();
	}

	// Compat with HTML DOM interface
	className = '';
	get nodeType() {
		return 1;
	}
	get nodeName() {
		return 'CANVAS';
	}
	get offsetWidth() {
		return this.width;
	}
	get offsetHeight() {
		return this.height;
	}
	get offsetTop() {
		return 0;
	}
	get offsetLeft() {
		return 0;
	}
	getAttribute(name: string): string | null {
		if (name === 'width') return String(this.width);
		if (name === 'height') return String(this.height);
		return null;
	}
	setAttribute(name: string, value: string | number) {}
}
$.canvasInitClass(Screen);
def(Screen);

// @ts-expect-error Internal constructor
export var screen = new Screen(INTERNAL_SYMBOL);
def(screen, 'screen');
