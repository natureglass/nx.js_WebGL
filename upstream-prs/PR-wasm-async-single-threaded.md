# PR-wasm-async-single-threaded — Make async `WebAssembly.instantiate/compile` resolve on the single-threaded platform

**Branch:** `upstream-pr/wasm-async-single-threaded`
**Base:** `upstream/main`
**Local base:** `8981cc6`
**Draft status.** Implemented + confirmed on real hardware (Unity WebGL now compiles + instantiates its module and executes `_main`). This is the change that took Emscripten/Unity loads from "hang forever at instantiate" to "runs."

## What's in the commit

```
 packages/runtime/src/index.ts | ~40 +
 source/main.cc                |  ~1 +   (add --wasm-lazy-compilation to the JIT flag string)
 2 files changed
```

## PR title (suggested)

`runtime: re-express async WebAssembly on the sync constructors (+ default --wasm-lazy-compilation)`

## Motivation

nx.js runs V8 on `NewSingleThreadedDefaultPlatform()` (no worker threads —
the multi-threaded default's workers fault on Horizon's pthread/abseil
sync). Under that platform, V8's **async** `WebAssembly.compile` /
`WebAssembly.instantiate` (and the `*Streaming` variants) post their
compile work — and the promise-resolution completion — onto V8's task
runners that a threadless platform never drains, so the returned promise
**never resolves**. The event loop stays alive (libuv timers keep firing)
while the load sits pending forever.

This is exactly the path Emscripten/Unity/Godot use to load
(`WebAssembly.instantiateStreaming(fetch(codeUrl), imports)`), so no
Emscripten module can boot. The **synchronous** constructors
(`new WebAssembly.Module` / `new WebAssembly.Instance`) compile inline on
the calling thread and work fine (they're what a small `WebAssembly.Module`
smoke test uses).

Flag-level attempts do not help: `--no-wasm-async-compilation` is a no-op
(`--predictable`, already set, forces sync compilation), and
`--wasm-num-compilation-tasks=0` doesn't change the outcome — the stall is
the async **promise resolution**, which is delivered via a task the
threadless platform doesn't run.

## The change

Re-express the async WebAssembly API on top of the working synchronous
constructors, resolving through a **microtask** (which the main loop's
`PerformMicrotaskCheckpoint` does drain). In `index.ts`, in the branch
where WebAssembly is available (JIT on, code headroom > 0):

```ts
const M = WA.Module;
const I = WA.Instance;
const readBytes = (source: any): Promise<any> =>
	Promise.resolve(source).then((r: any) =>
		r && typeof r.arrayBuffer === 'function' ? r.arrayBuffer() : r);

WA.compile = (bytes: any) => Promise.resolve().then(() => new M(bytes));
WA.instantiate = (src: any, importObject?: any) =>
	Promise.resolve().then(() => {
		// instantiate(Module, imports) -> Instance;
		// instantiate(bytes,  imports) -> { module, instance }
		if (src instanceof M) return new I(src, importObject);
		const mod = new M(src);
		return { module: mod, instance: new I(mod, importObject) };
	});
WA.compileStreaming = (source: any) =>
	readBytes(source).then((bytes: any) => WA.compile(bytes));
WA.instantiateStreaming = (source: any, importObject?: any) =>
	readBytes(source).then((bytes: any) => WA.instantiate(bytes, importObject));
```

The compile is deferred into a `.then` so a compile/link error surfaces as
a promise **rejection** (matching spec) rather than a synchronous throw.
The existing reject-with-actionable-message branch (JIT off / no code
headroom) is unchanged — this is its `else`.

Paired change in `source/main.cc`: add `--wasm-lazy-compilation` to the
JIT-branch default V8 flag string. Because we're `--single-threaded`, an
eager compile of a large module (Unity/Emscripten modules are 10–60 MB)
runs entirely on the main thread and wedges the loop / exhausts the code
arena. Lazy keeps the synchronous `new Module` cheap (function bodies
compile on first call), which is what makes the sync-backed
`instantiate` above stay responsive on big modules.

```c
V8::SetFlagsFromString("--single-threaded --single-threaded-gc "
                       "--predictable --wasm-lazy-compilation");
```

## Behavior

- `WebAssembly.compile`/`instantiate`/`compileStreaming`/`instantiateStreaming`
  now resolve (or reject) reliably on the single-threaded platform.
- Return shapes match spec: `instantiate(Module, imports)` → `Instance`;
  `instantiate(bytes, imports)` → `{ module, instance }`.
- Errors reject the promise (spec-conformant) instead of throwing
  synchronously from the async entry point.
- Streaming variants are now provided (previously `instantiateStreaming`
  was often absent, forcing every embedder to polyfill it).

## Gotchas (worth calling out in review)

- Must capture `M = WA.Module` / `I = WA.Instance` **before** reassigning
  the async members, and use those captured refs (so an embedder that later
  wraps `WebAssembly.Module` doesn't recurse through the shim).
- `--wasm-lazy-compilation` is load-bearing for the shim on large modules;
  without it the synchronous `new Module` blocks the loop during an eager
  compile.

## Testing

Real hardware: Unity WebGL builds (2021.3 → 6000.4, WebGL1/2) now progress
`instantiateStreaming → arrayBuffer → new Module → new Instance` and
execute `_main` (Unity prints its `[UnityMemory] Configuration Parameters`
banner and enters its render loop), where previously the load hung
indefinitely at instantiate.
