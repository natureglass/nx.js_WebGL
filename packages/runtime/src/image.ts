import { $ } from './$';
import { createInternal, def, proto } from './utils';
import { URL } from './polyfills/url';

// Late-bound (call-time) globalThis.fetch wrapper. This module DOES NOT
// import `./fetch/fetch` directly: embedders (e.g. brewser-runtime's
// BrowserResourceLoader for `brewser://` URLs) install their own
// `globalThis.fetch` wrapper at app-session start to extend the set of
// supported schemes beyond this package's built-in
// http/https/blob/data/file/sdmc/romfs handlers. Recapitulates the
// QuickJS-era patch that the V8 migration dropped — see
// nxjs-source-v8/MIGRATION_PLAN.md "QuickJS-era engine patches to re-apply
// on V8" catalog.
//
// CRITICAL: late-bound. The function body calls `globalThis.fetch` at
// CALL TIME, not at module init. An import-time capture
// (`const fetch = globalThis.fetch`) would freeze the pre-wrapper engine
// fetch (this module loads at engine boot, embedders install the wrapper
// at session start) and the deferral would do nothing. Any future
// re-application of this style of patch must preserve the call-time
// lookup.
function fetch(
	input: string | URL | Request,
	init?: RequestInit,
): Promise<Response> {
	return globalThis.fetch(input, init);
}
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
		// Ledger #80 — prefer `globalThis.location?.href` over
		// `document.baseURI`. Embedders that emulate per-page navigation by
		// pushing a fresh `location.href` (e.g. the webgl-conformance runner
		// evaluating each test HTML in-place) leave `document.baseURI` pinned
		// at the outer page URL, so relative `image.src` values were
		// resolving against the wrong base — the from_image cluster's
		// `image.src = resourcePath + "..."` fetches 404'd and `onload`
		// never fired (TIMEOUT). `location.href` is the more responsive
		// signal here: real browsers keep `baseURI ≡ location.href` unless
		// `<base href>` is set, so preferring location.href diverges only
		// for the (uncommon in nx.js) `<base href>` case. Falls back to
		// `document.baseURI` then `$.entrypoint`.
		const g = globalThis as {
			location?: { href?: string };
			document?: { baseURI?: string };
		};
		const baseUrl =
			g.location?.href ?? g.document?.baseURI ?? $.entrypoint;
		const url = new URL(val, baseUrl);
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
					this.dispatchEvent(new Event('load'));
				},
				(error) => {
					internal.complete = false;
					this.dispatchEvent(new ErrorEvent('error', { error }));
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
}
$.imageInit(Image);
def(Image);
