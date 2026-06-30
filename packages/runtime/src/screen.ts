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
import { type WebGL2RenderingContext } from './canvas/webgl2-rendering-context';
import {
	createWebGLContext,
	type WebGLRenderingContext,
} from './canvas/webgl-rendering-context';
import { initTouchscreen } from './touchscreen';
import type { TouchEvent } from './polyfills/event';
import {
	isAppOwningScreen,
	markAppOwnsScreen,
	registerConsoleScreen,
} from './console-screen';

interface ScreenInternal {
	context2d?: CanvasRenderingContext2D;
	contextWebGL?: WebGLRenderingContext;
	contextWebGL2?: WebGL2RenderingContext;
}

const _ = createInternal<Screen, ScreenInternal>();

// Internal hook so the console present can get the screen's 2D context without
// the public getContext()'s "app owns screen" side effect.
const CONSOLE_SCREEN_CTX = Symbol('consoleScreenCtx');

// Acquire the screen's 2D context + put the screen into canvas (framebuffer)
// mode. Shared by the public `getContext('2d')` and the console present.
//
// NOTE: this MUST be a free function (keyed off the `_` WeakMap), NOT a
// `#private` method. The `Screen` constructor returns a substitute native
// canvas object (`proto($.canvasNew(...), Screen)`), so private class members
// are never installed on the actual `screen` instance — calling a `#method` on
// it throws "Receiver must be an instance of class Screen".
function ensureContext(self: Screen): CanvasRenderingContext2D {
	const i = _(self);
	if (!i.context2d) {
		i.context2d = new CanvasRenderingContext2D(
			// @ts-expect-error Internal constructor
			INTERNAL_SYMBOL,
			self,
		);
		$.framebufferInit(self);
	}
	return i.context2d;
}

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
	getContext(contextId: 'webgl' | 'experimental-webgl'): WebGLRenderingContext | null;
	getContext(contextId: 'webgl2'): WebGL2RenderingContext | null;
	getContext(contextId: string): null;
	getContext(
		contextId: string,
	): CanvasRenderingContext2D | WebGLRenderingContext | WebGL2RenderingContext | null {
		const i = _(this);
		if (contextId === '2d') {
			// The screen may only have one context kind (per the HTML canvas
			// spec): once a WebGL context exists, '2d' returns null.
			if (i.contextWebGL || i.contextWebGL2) return null;
			// User code acquiring the screen context takes over rendering
			// from the default console present (see console-screen.ts).
			markAppOwnsScreen();
			return ensureContext(this);
		}
		// Phase 2.C: WebGL 1 path. Renders into the bridge's tenant offscreen
		// FBO (Skia composites the result into the persistent canvas surface
		// before present). Matches the WebGL 1 spec — `experimental-webgl` is
		// the legacy alias the spec mandates we still honor.
		//
		// IMPORTANT: this branch does NOT enforce v2's "2D-exclusive" check —
		// brewser's screen always carries the shell's 2D context, and inline
		// canvas WebGL demos depend on coexistence: 2D draws the shell UI
		// into the persistent canvas surface, WebGL draws into the bridge
		// tenant FBO, and the present-time compose stack writes WebGL on top
		// of 2D into the same surface before swap. Forbidding co-acquisition
		// (as the v2 branch below does) kills inline-canvas WebGL for every
		// brewser app (canvas-runner.ts → getSharedScreenGL → here).
		if (contextId === 'webgl' || contextId === 'experimental-webgl') {
			if (!i.contextWebGL) {
				// Only exclude the other WebGL family on the same canvas;
				// 2D coexists.
				if (i.contextWebGL2) return null;
				const ctx = createWebGLContext(this);
				if (!ctx) return null;
				i.contextWebGL = ctx;
			}
			return i.contextWebGL;
		}
		if (contextId === 'webgl2') {
			// Phase 2.C: 'webgl2' is deliberately null. The engine binding for
			// the WebGL 2 surface (vertexAttribIPointer, drawElementsInstanced,
			// getBufferSubData, ...) is Phase 2.G work. Three.js detects WebGL
			// 2 via `gl.constructor.name === 'WebGL2RenderingContext'`, so
			// returning a v2 instance with only v1 methods would silently route
			// Three.js into its v2 code path and throw on the first v2 call.
			// Better: stay null + force the v1 path (which is what the slice
			// demo `geometry-cube` deliberately uses).
			return null;
		}
		return null;
	}

	/**
	 * @ignore
	 * Internal: used by the console present to draw onto the screen WITHOUT
	 * marking the app as owning the screen.
	 */
	[CONSOLE_SCREEN_CTX](): CanvasRenderingContext2D {
		return ensureContext(this);
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

// Let the console present blit onto the screen (without claiming app ownership)
// and wire touch-drag scrollback.
registerConsoleScreen(() => (screen as any)[CONSOLE_SCREEN_CTX](), screen);
def(screen, 'screen');
