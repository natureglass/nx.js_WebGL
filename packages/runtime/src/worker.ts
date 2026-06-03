import { $ } from './$';
import { WORKER_BOOTSTRAP_JS } from './worker-bootstrap';

/**
 * Tier-1 Pass A Web Worker — see [[project-swb-web-workers-milestone]].
 *
 * Structured clone shipped: `postMessage` accepts/returns primitives,
 * Date, ArrayBuffer, TypedArrays, DataView, Map, Set, Array, plain
 * Object, Error. Out of scope this pass: BigInt, RegExp, ArrayBuffer
 * transfer (zero-copy), cyclic refs.
 *
 * Tier-0 carryover: real OS thread per worker, own JSRuntime+JSContext,
 * 16 MB heap cap, bidirectional message queue, exception →
 * `worker.onerror`, navigation cleanup wired via `__terminateAllWorkers`.
 *
 * Wire: messages travel as ArrayBuffer bytes (TLV from
 * [./worker-bootstrap]). The C queue is opaque to the value's shape.
 */

interface MessageEventLike { data: unknown; type: 'message'; }
interface ErrorEventLike { message: string; type: 'error'; }
type MessageHandler = (e: MessageEventLike) => void;
type ErrorHandler = (e: ErrorEventLike) => void;

const workersByHandle = new Map<number, Worker>();

const KIND_DATA = 0;
const KIND_ERROR = 1;

// Evaluate the shared bootstrap module on the MAIN side so we can call
// the same serialize/deserialize functions here. We use indirect eval
// to keep the names off the module scope; the bootstrap assigns to
// `globalThis._scSerialize` / `_scDeserialize` which we then bind.
(0, eval)(WORKER_BOOTSTRAP_JS);
/** Pass F: serializer overloads. Without transferSet → plain
 * ArrayBuffer (legacy callers). With transferSet → `{ ab, transfers }`
 * pair where `transfers` is the side-channel list handed to the C
 * layer alongside the wire payload. */
type SerializerLegacy = (val: unknown) => ArrayBuffer;
type SerializerWithTransfer = (val: unknown, transferSet: Set<ArrayBuffer>) => { ab: ArrayBuffer; transfers: ArrayBuffer[] };
type Deserializer = (ab: ArrayBuffer, transferABs?: ArrayBuffer[]) => unknown;
const scSerialize: SerializerLegacy = (globalThis as unknown as { _scSerialize: SerializerLegacy })._scSerialize;
const scSerializeT: SerializerWithTransfer = (globalThis as unknown as { _scSerialize: SerializerWithTransfer })._scSerialize;
const scDeserialize: Deserializer = (globalThis as unknown as { _scDeserialize: Deserializer })._scDeserialize;

// Single C-side dispatcher entry. KIND_DATA carries an ArrayBuffer that
// we deserialize before firing `onmessage`; KIND_ERROR carries the
// exception's `toString()` and fires `onerror` directly. Internal
// envelopes (Pass E fetch proxy) are routed before user dispatch so
// the page's `worker.onmessage` never sees them. Pass F: the 4th arg
// is an optional array of receiver-owned ArrayBuffers built C-side
// from the transferred side-channel.
$.workerSetDispatcher((handle: number, value: unknown, kind: number, transferABs?: ArrayBuffer[]) => {
	const w = workersByHandle.get(handle);
	if (!w) return;
	if (kind === KIND_ERROR) {
		w._dispatchError(new Error(value as string));
		return;
	}
	let decoded: unknown;
	try { decoded = scDeserialize(value as ArrayBuffer, transferABs); }
	catch (err) { w._dispatchError(err); return; }
	if (decoded && typeof decoded === 'object'
			&& (decoded as { __nxInternal?: unknown }).__nxInternal === 'fetch-req') {
		handleWorkerFetchRequest(handle, decoded as WorkerFetchRequest);
		return;
	}
	w._dispatchInbound(decoded);
});

interface WorkerFetchRequest {
	__nxInternal: 'fetch-req';
	id: number;
	url: string;
	init: {
		method: string;
		headers: Array<[string, string]>;
		body: string | ArrayBuffer | ArrayBufferView | null;
	};
}

/** Post an internal envelope back to the worker, silently dropping if
 * the worker has been terminated mid-fetch. The worker's bootstrap
 * intercepts `fetch-resp` before firing user `onmessage`. */
function postInternalToWorker(handle: number, payload: object): void {
	if (!workersByHandle.has(handle)) return;
	try { $.workerPostToWorker(handle, scSerialize(payload)); }
	catch (_) { /* swallow — worker may have died between has() and post */ }
}

/** Main-thread fetch on behalf of a worker. Marshals the response back
 * via an internal envelope. Errors come through as `{ error: msg }`. */
function handleWorkerFetchRequest(handle: number, req: WorkerFetchRequest): void {
	const id = req.id;
	const mainFetch = (globalThis as unknown as {
		fetch: (u: string, init?: { method?: string; headers?: unknown; body?: unknown }) => Promise<{
			status: number; statusText: string; ok: boolean; url: string;
			headers: { forEach: (cb: (v: string, k: string) => void) => void };
			arrayBuffer: () => Promise<ArrayBuffer>;
		}>;
	}).fetch;
	const init: { method: string; headers: Array<[string, string]>; body?: unknown } = {
		method: req.init.method,
		headers: req.init.headers,
	};
	if (req.init.body != null) init.body = req.init.body;
	let p: Promise<unknown>;
	try { p = mainFetch(req.url, init as { method?: string; headers?: unknown; body?: unknown }); }
	catch (err) {
		postInternalToWorker(handle, { __nxInternal: 'fetch-resp', id, error: (err as Error)?.message || String(err) });
		return;
	}
	p.then(async (r) => {
		const resp = r as {
			status: number; statusText: string; ok: boolean; url: string;
			headers: { forEach: (cb: (v: string, k: string) => void) => void };
			arrayBuffer: () => Promise<ArrayBuffer>;
		};
		const body = await resp.arrayBuffer();
		const headers: Array<[string, string]> = [];
		resp.headers.forEach((v, k) => { headers.push([k, v]); });
		postInternalToWorker(handle, {
			__nxInternal: 'fetch-resp', id,
			status: resp.status, statusText: resp.statusText, ok: resp.ok,
			url: resp.url, headers, body,
		});
	}).catch((err) => {
		const msg = (err && (err as Error).message) || String(err);
		postInternalToWorker(handle, { __nxInternal: 'fetch-resp', id, error: msg });
	});
}

/** Terminate + free every active worker. Called from the swb shell on
 * page navigation. Synchronous — blocks per `pthread_join`. */
export function terminateAllWorkers(): void {
	const all = Array.from(workersByHandle.values());
	for (const w of all) {
		try { w.terminate(); } catch (_) { /* swallow */ }
	}
}
(globalThis as unknown as { __terminateAllWorkers?: () => void }).__terminateAllWorkers = terminateAllWorkers;

/**
 * Resolve the ctor argument to a JS source string.
 *
 * - `sdmc:/`, `romfs:/` → sync read via `Switch.readFileSync` (returns
 *   string immediately, ctor stays sync).
 * - `http://`, `https://`, `brewser://`, `data:` → async `fetch`
 *   (returns a Promise; ctor enters pending state until it resolves).
 * - No scheme + JS structural chars → treat as inline source.
 * - No scheme + no structural chars → ambiguous, treat as inline source
 *   anyway (preserves Tier-0 behaviour for short literal scripts).
 *
 * Scheme check runs FIRST so a URL like `?v=1` (contains `=`) doesn't
 * get misclassified as inline JS.
 */
function resolveWorkerSource(arg: string): string | Promise<string> {
	const schemeMatch = arg.match(/^([a-zA-Z][a-zA-Z0-9+.\-]*):/);
	if (schemeMatch) {
		const scheme = schemeMatch[1].toLowerCase();
		if (scheme === 'sdmc' || scheme === 'romfs') {
			const sw = (globalThis as unknown as { Switch?: { readFileSync?: (p: string) => ArrayBuffer | Uint8Array } }).Switch;
			if (sw && typeof sw.readFileSync === 'function') {
				try {
					const buf = sw.readFileSync(arg);
					if (buf instanceof ArrayBuffer) return new TextDecoder().decode(buf);
					if (buf && (buf as Uint8Array).buffer) return new TextDecoder().decode(buf as Uint8Array);
				} catch (e) {
					throw new Error('Worker: cannot read ' + arg + ': ' + ((e as Error).message || e));
				}
			}
			throw new Error('Worker: Switch.readFileSync unavailable for ' + arg);
		}
		if (scheme === 'http' || scheme === 'https' || scheme === 'brewser' || scheme === 'data') {
			return (globalThis as unknown as { fetch: (u: string) => Promise<{ ok: boolean; status: number; text: () => Promise<string> }> })
				.fetch(arg)
				.then((r) => {
					if (!r.ok) throw new Error('Worker URL fetch failed: ' + r.status + ' for ' + arg);
					return r.text();
				});
		}
		// Unknown scheme — fall through to inline-source treatment
	}
	return arg;
}

export class Worker {
	/** -1 while the constructor's async URL fetch is in flight (Pass D);
	 * positive once the worker's pthread is alive. `terminate` before
	 * the fetch resolves clears `#terminated` and the spawn never happens. */
	#handle: number;
	#terminated = false;
	#onmessage: MessageHandler | null = null;
	#onerror: ErrorHandler | null = null;
	#messageListeners: Set<MessageHandler> = new Set();
	#errorListeners: Set<ErrorHandler> = new Set();
	/** Buffer for `postMessage` calls that happen between ctor and the
	 * URL fetch resolving. Drained in source-arrival order via the same
	 * `workerPostToWorker` path once `#handle` is real. Null after that
	 * drain — postMessage takes the direct path. Mirrors browser
	 * behaviour: messages queued pre-spawn are delivered post-spawn.
	 * Pass F: each entry is `{ data, transfer? }` so the transfer list
	 * survives the buffer + drain cycle. */
	#pendingMessages: Array<{ data: unknown; transfer?: ArrayBuffer[] }> | null = null;

	constructor(scriptOrUrl: string, _options?: { name?: string; type?: 'classic' | 'module' }) {
		const resolved = resolveWorkerSource(scriptOrUrl);
		if (typeof resolved === 'string') {
			// Sync path — inline source or sdmc:/romfs: sync read
			this.#handle = $.workerSpawn(WORKER_BOOTSTRAP_JS + '\n;\n' + resolved);
			workersByHandle.set(this.#handle, this);
		} else {
			// Async URL path — defer spawn until fetch resolves
			this.#handle = -1;
			this.#pendingMessages = [];
			resolved.then(
				(src) => {
					if (this.#terminated) return;
					try {
						this.#handle = $.workerSpawn(WORKER_BOOTSTRAP_JS + '\n;\n' + src);
						workersByHandle.set(this.#handle, this);
						const buffered = this.#pendingMessages || [];
						this.#pendingMessages = null;
						for (const m of buffered) {
							try { this._postBytesNow(m.data, m.transfer); }
							catch (err) { this._dispatchError(err); }
						}
					} catch (err) { this._dispatchError(err); }
				},
				(err) => {
					if (this.#terminated) return;
					this._dispatchError(err);
				},
			);
		}
	}

	get onmessage(): MessageHandler | null { return this.#onmessage; }
	set onmessage(fn: MessageHandler | null) { this.#onmessage = typeof fn === 'function' ? fn : null; }
	get onerror(): ErrorHandler | null { return this.#onerror; }
	set onerror(fn: ErrorHandler | null) { this.#onerror = typeof fn === 'function' ? fn : null; }

	postMessage(data: unknown, transfer?: ArrayBuffer[]): void {
		if (this.#terminated) return;
		if (this.#handle < 0) {
			// Still fetching the URL — buffer for replay post-spawn.
			if (this.#pendingMessages) this.#pendingMessages.push({ data, transfer });
			return;
		}
		this._postBytesNow(data, transfer);
	}

	/** Internal: serialize + post to the C queue. Handles the Pass F
	 * transfer path: validates each entry is an ArrayBuffer, calls the
	 * transfer-aware serializer, and passes the side-channel through to
	 * `workerPostToWorker`. The C layer detaches each transferred AB
	 * after attaching its bytes. */
	_postBytesNow(data: unknown, transfer?: ArrayBuffer[]): void {
		if (transfer && transfer.length > 0) {
			const set = new Set<ArrayBuffer>();
			for (const t of transfer) {
				if (!(t instanceof ArrayBuffer)) {
					throw new TypeError('postMessage: transfer items must be ArrayBuffer (Tier-1)');
				}
				set.add(t);
			}
			let pair: { ab: ArrayBuffer; transfers: ArrayBuffer[] };
			try { pair = scSerializeT(data, set); }
			catch (err) { throw new Error('postMessage: ' + ((err as Error)?.message || String(err))); }
			$.workerPostToWorker(this.#handle, pair.ab, pair.transfers);
			return;
		}
		let bytes: ArrayBuffer;
		try { bytes = scSerialize(data); }
		catch (err) { throw new Error('postMessage: ' + ((err as Error)?.message || String(err))); }
		$.workerPostToWorker(this.#handle, bytes);
	}

	terminate(): void {
		if (this.#terminated) return;
		this.#terminated = true;
		this.#pendingMessages = null;
		if (this.#handle >= 0) {
			workersByHandle.delete(this.#handle);
			$.workerTerminate(this.#handle);
		}
		// If handle is still -1, the fetch.then guard above will see
		// `#terminated` and skip the spawn.
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

	_dispatchInbound(data: unknown): void {
		const e: MessageEventLike = { data, type: 'message' };
		if (this.#onmessage) { try { this.#onmessage(e); } catch (err) { this._dispatchError(err); } }
		for (const fn of this.#messageListeners) {
			try { fn(e); } catch (err) { this._dispatchError(err); }
		}
	}

	_dispatchError(err: unknown): void {
		const msg = (err && (err as { message?: string }).message) || String(err);
		const e: ErrorEventLike = { message: msg, type: 'error' };
		if (this.#onerror) { try { this.#onerror(e); } catch (_) { /* swallow */ } }
		for (const fn of this.#errorListeners) { try { fn(e); } catch (_) { /* swallow */ } }
	}
}

(globalThis as unknown as { Worker: typeof Worker }).Worker = Worker;
