# PR-body-arraybuffer-inmemory-fastpath — Return in-memory `Body` bytes directly from `arrayBuffer()`

**Branch:** `upstream-pr/body-arraybuffer-inmemory-fastpath`
**Base:** `upstream/main`
**Local base:** `8981cc6`
**Draft status.** Implemented + confirmed on real hardware (unblocked large-body reads that previously hung — see Testing).

## What's in the commit

```
 packages/runtime/src/fetch/body.ts | ~25 +
 1 file changed
```

## PR title (suggested)

`runtime: fast-path Body.arrayBuffer() for in-memory bodies (fixes hang reading large in-memory responses)`

## Motivation

When a `Response`/`Request` body is constructed from an in-memory source
(a `string`, `BufferSource`/`ArrayBuffer`/typed array, or
`URLSearchParams`), `Body` wraps the bytes in a `ReadableStream` via
`asyncIteratorToStream(arrayBufferIterator(...))`. `arrayBuffer()` (and
therefore `text()`/`json()`/`blob()`, which all funnel through it) then
reads that stream back out chunk-by-chunk with `reader.read()`.

On a single-threaded platform (no worker threads), the native
`ReadableStream`'s read/pull scheduling **stalls when the stream yields a
single large chunk** — `reader.read()`'s promise never resolves. Small
bodies (a few KB) complete fine, so the failure is size-dependent and
easy to miss. The concrete symptom: an Emscripten/Unity app that fetches
a multi-MB `code.wasm` / `.data` via `fetch(url).then(r => r.arrayBuffer())`
hangs forever at `arrayBuffer()` — the event loop stays alive (timers keep
firing) while the body promise is stuck. This blocked every large local
asset read.

Independently of the stall, round-tripping already-in-memory bytes through
a stream is pure overhead.

## The change

Keep the raw bytes on the `Body` when it's built from an in-memory source,
and return them directly from `arrayBuffer()`:

```ts
export abstract class Body implements globalThis.Body {
	body: ReadableStream<Uint8Array> | null;
	...
	#rawBody: ArrayBuffer | undefined;   // NEW
```

Set `#rawBody` in the three in-memory constructor branches (string,
`URLSearchParams`, and the `BufferSource` `else` branch) alongside the
existing `this.body = asyncIteratorToStream(...)`:

```ts
this.#rawBody = encoded.buffer;   // string / URLSearchParams
...
this.#rawBody = ab;               // BufferSource
```

Short-circuit in `arrayBuffer()` after the `bodyUsed`/null checks and
before the stream read loop:

```ts
if (this.#rawBody) {
	this.bodyUsed = true;
	return this.#rawBody;
}
```

The `body` `ReadableStream` is still exposed on `.body` for callers that
consume it directly; only the buffered accessors take the fast path.
Blob/ReadableStream/FormData bodies (no `#rawBody`) keep the stream path
unchanged.

## Behavior

- `arrayBuffer()`/`text()`/`json()`/`blob()` on an in-memory body return
  without touching the ReadableStream — correct result, no size-dependent
  stall, and less work.
- `bodyUsed` still flips to `true` (so a second read throws), and
  `resp.body` (the stream) is unchanged for stream consumers.
- Real network/streamed bodies (no `#rawBody`) are unaffected.

## Note for maintainers

The fast path sidesteps — but does not fix — the underlying native
`ReadableStream` large-single-chunk stall on the single-threaded platform.
That stall is worth a separate look (it would still bite a caller who reads
`resp.body` directly on a large in-memory response), but the vast majority
of body consumption goes through `arrayBuffer()`/`text()`/`json()`, which
this covers.

## Testing

Real hardware: an Emscripten/Unity WebGL build's `fetch(codeUrl).then(r =>
r.arrayBuffer())` on a 9.3 MB `code.wasm` previously hung at
`arrayBuffer()` indefinitely; with this change it returns immediately and
the module proceeds to instantiate. Small-body paths (JSON config, etc.)
unchanged.
