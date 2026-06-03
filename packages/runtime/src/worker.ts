import { $ } from './$';

/**
 * Tier-0 Web Worker — see [[project-swb-web-workers-milestone]].
 *
 * Real multi-threaded Worker on a dedicated pthread with its own
 * JSRuntime+JSContext. Surface limited to bidirectional
 * `postMessage(string)`; no structured clone, no importScripts, no
 * timers inside the worker, no `nx.js` native APIs available to the
 * worker context.
 *
 * Spec deviations (Tier-0 only):
 *   - Constructor takes either a URL string or a `Blob` URL OR a raw
 *     source string. URLs are sync-read via `Switch.readFileSync` if
 *     they look like a path (sdmc:/, romfs:/, or no `://`); HTTPS not
 *     supported in Tier-0 — use Tier-1.
 *   - `e.data` is always a string. Number / object payloads must be
 *     `JSON.stringify`d by the caller, parsed by the receiver.
 *   - No `Transferable` (transfer parameter is ignored).
 *   - No `terminate()`-during-message-dispatch quiescence guarantee.
 */

interface MessageEventLike {
	data: string;
	type: 'message';
}
interface ErrorEventLike {
	message: string;
	type: 'error';
}

type MessageHandler = (e: MessageEventLike) => void;
type ErrorHandler = (e: ErrorEventLike) => void;

const workersByHandle = new Map<number, Worker>();

const KIND_DATA = 0;
const KIND_ERROR = 1;

/** Install the single dispatcher the C main-loop drain calls. */
$.workerSetDispatcher((handle: number, data: string, kind: number) => {
	const w = workersByHandle.get(handle);
	if (!w) return;
	if (kind === KIND_ERROR) w._dispatchError(new Error(data));
	else w._dispatchInbound(data);
});

/** Terminate + free every Worker tracked in JS. Called from the shell
 * on page navigation so workers from page A don't outlive their page
 * + leak QuickJS runtimes across the swb session. Synchronous —
 * blocks on each `pthread_join`. Workers receive no chance to clean
 * up; if they're holding external resources, that's their bug. */
export function terminateAllWorkers(): void {
	const all = Array.from(workersByHandle.values());
	for (const w of all) {
		try { w.terminate(); } catch (_) { /* swallow */ }
	}
}
(globalThis as unknown as { __terminateAllWorkers?: () => void }).__terminateAllWorkers = terminateAllWorkers;

/** Read a worker script source from a path or fall back to treating
 * the argument as inline source. Tier-0 only supports sync-readable
 * schemes — sdmc:/ , romfs:/ , or a relative path. */
function resolveWorkerSource(arg: string): string {
	// Heuristic: if the string contains a newline, semicolon, or
	// brace, it's almost certainly inline source. Otherwise try to
	// resolve as a path.
	if (/[\n;{}=]/.test(arg)) return arg;
	const sw = (globalThis as unknown as { Switch?: { readFileSync?: (p: string) => ArrayBuffer | Uint8Array } }).Switch;
	if (sw && typeof sw.readFileSync === 'function') {
		try {
			const buf = sw.readFileSync(arg);
			if (buf instanceof ArrayBuffer) {
				return new TextDecoder().decode(buf);
			}
			if (buf && (buf as Uint8Array).buffer) {
				return new TextDecoder().decode(buf as Uint8Array);
			}
		} catch (_) { /* fall through — treat as inline */ }
	}
	return arg;
}

export class Worker {
	#handle: number;
	#terminated = false;
	#onmessage: MessageHandler | null = null;
	#onerror: ErrorHandler | null = null;
	#messageListeners: Set<MessageHandler> = new Set();
	#errorListeners: Set<ErrorHandler> = new Set();

	constructor(scriptOrUrl: string, _options?: { name?: string; type?: 'classic' | 'module' }) {
		const source = resolveWorkerSource(scriptOrUrl);
		this.#handle = $.workerSpawn(source);
		workersByHandle.set(this.#handle, this);
	}

	get onmessage(): MessageHandler | null { return this.#onmessage; }
	set onmessage(fn: MessageHandler | null) { this.#onmessage = typeof fn === 'function' ? fn : null; }
	get onerror(): ErrorHandler | null { return this.#onerror; }
	set onerror(fn: ErrorHandler | null) { this.#onerror = typeof fn === 'function' ? fn : null; }

	postMessage(data: string, _transfer?: unknown[]): void {
		if (this.#terminated) return;
		// Spec note: postMessage allows any structured-cloneable value.
		// Tier-0 only supports strings; non-strings are stringified.
		const s = typeof data === 'string' ? data : String(data);
		$.workerPostToWorker(this.#handle, s);
	}

	terminate(): void {
		if (this.#terminated) return;
		this.#terminated = true;
		workersByHandle.delete(this.#handle);
		$.workerTerminate(this.#handle);
	}

	addEventListener(type: 'message', listener: MessageHandler): void;
	addEventListener(type: 'error', listener: ErrorHandler): void;
	addEventListener(type: string, listener: MessageHandler | ErrorHandler): void {
		if (type === 'message') this.#messageListeners.add(listener as MessageHandler);
		else if (type === 'error') this.#errorListeners.add(listener as ErrorHandler);
	}
	removeEventListener(type: 'message', listener: MessageHandler): void;
	removeEventListener(type: 'error', listener: ErrorHandler): void;
	removeEventListener(type: string, listener: MessageHandler | ErrorHandler): void {
		if (type === 'message') this.#messageListeners.delete(listener as MessageHandler);
		else if (type === 'error') this.#errorListeners.delete(listener as ErrorHandler);
	}

	/** Called by the global C-dispatcher per inbound message. */
	_dispatchInbound(data: string): void {
		const e: MessageEventLike = { data, type: 'message' };
		if (this.#onmessage) {
			try { this.#onmessage(e); }
			catch (err) { this._dispatchError(err); }
		}
		for (const fn of this.#messageListeners) {
			try { fn(e); }
			catch (err) { this._dispatchError(err); }
		}
	}

	_dispatchError(err: unknown): void {
		const msg = (err && (err as { message?: string }).message) || String(err);
		const e: ErrorEventLike = { message: msg, type: 'error' };
		if (this.#onerror) {
			try { this.#onerror(e); } catch (_) { /* swallow */ }
		}
		for (const fn of this.#errorListeners) {
			try { fn(e); } catch (_) { /* swallow */ }
		}
	}
}

(globalThis as unknown as { Worker: typeof Worker }).Worker = Worker;
