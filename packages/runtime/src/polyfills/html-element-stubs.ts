import { def } from '../utils';

/**
 * Stub `HTML*Element` classes registered on globalThis as empty class
 * constructors. Many game engines (Cocos Creator + plenty of Emscripten
 * builds) include unguarded `obj instanceof HTMLCanvasElement` /
 * `instanceof HTMLImageElement` / etc. checks for asset-source type
 * detection. In a headless / non-DOM environment those names would
 * resolve to `undefined` and the bare reference throws a ReferenceError
 * before the instanceof check even runs.
 *
 * The stubs are empty classes whose `instanceof` always returns false
 * against nx.js's `LiveElement` / `OffscreenCanvas` / etc., which is
 * spec-correct (those are not real DOM HTML elements). Engines then
 * take the alternate (non-DOM) code path.
 *
 * Inheritance is mostly flat: `Node` and `Element` are at the base,
 * everything else extends `HTMLElement` extends `Element` extends
 * `Node`. The chain matters in case engines walk the prototype chain
 * during their detection.
 */

class Node {}
class Element extends Node {}
class HTMLElement extends Element {}

class HTMLCanvasElement extends HTMLElement {}
class HTMLImageElement extends HTMLElement {}
class HTMLVideoElement extends HTMLElement {}
class HTMLAudioElement extends HTMLElement {}
class HTMLMediaElement extends HTMLElement {}
class HTMLDivElement extends HTMLElement {}
class HTMLSpanElement extends HTMLElement {}
class HTMLInputElement extends HTMLElement {}
class HTMLButtonElement extends HTMLElement {}
class HTMLAnchorElement extends HTMLElement {}
class HTMLIFrameElement extends HTMLElement {}
class HTMLScriptElement extends HTMLElement {}

def(Node, 'Node');
def(Element, 'Element');
def(HTMLElement, 'HTMLElement');
def(HTMLCanvasElement, 'HTMLCanvasElement');
def(HTMLImageElement, 'HTMLImageElement');
def(HTMLVideoElement, 'HTMLVideoElement');
def(HTMLAudioElement, 'HTMLAudioElement');
def(HTMLMediaElement, 'HTMLMediaElement');
def(HTMLDivElement, 'HTMLDivElement');
def(HTMLSpanElement, 'HTMLSpanElement');
def(HTMLInputElement, 'HTMLInputElement');
def(HTMLButtonElement, 'HTMLButtonElement');
def(HTMLAnchorElement, 'HTMLAnchorElement');
def(HTMLIFrameElement, 'HTMLIFrameElement');
def(HTMLScriptElement, 'HTMLScriptElement');

export {
	Node,
	Element,
	HTMLElement,
	HTMLCanvasElement,
	HTMLImageElement,
	HTMLVideoElement,
	HTMLAudioElement,
	HTMLMediaElement,
	HTMLDivElement,
	HTMLSpanElement,
	HTMLInputElement,
	HTMLButtonElement,
	HTMLAnchorElement,
	HTMLIFrameElement,
	HTMLScriptElement,
};
