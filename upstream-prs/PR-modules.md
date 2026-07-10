# PR-modules — Page-level ES modules: importmap + prefetched-source registry + JS-callable bindings

**Branch:** `upstream-pr/modules-page-level`
**Base:** `upstream/main`
**Retires ledger entries:** #105

## PR title (suggested)

`module: expose page-level ES module execution — importmap + prefetched-source registry + JS-callable moduleRun`

## PR body

nx.js has had a full V8-backed ES module system in `source/module.cc` since
the V8 migration. It's used by `nx_run_entry_module` for the runtime's own
bundle: `ScriptCompiler::CompileModule` + `Module::InstantiateModule` +
`Module::Evaluate`, with `SetHostInitializeImportMetaObjectCallback` +
`SetHostImportModuleDynamicallyCallback` wired for `import.meta.url` and
dynamic `import()`. Top-level await is chained. Cycles resolve. It works.

The gap: this machinery is **not reachable from JS**. Embedders that want
to execute a page-shaped `<script type="module">` — either because they're
rendering HTML pages, or because they want to run arbitrary user-supplied
module code at runtime, or because they're building a REPL, or because
they need to run a test file that imports fixtures — currently can't. They
have to either:

1. Ship a userland module loader (SystemJS, RequireJS, etc.) — ~30 KB, its
   own resolver, its own compile pipeline running on top of `eval` — and
   forfeit V8's native module semantics, its compile cache, its module
   identity guarantees, and this file's dynamic-import integration.
2. Pre-bundle everything into a single entrypoint and reboot the isolate
   for every module they want to run.

Neither is great. This PR closes the gap with a minimal surface: four
JS-callable functions on the `$` bridge object, one resolver extension
that adds importmap fallback for bare specifiers, and one alternate source
lookup path for URL schemes the engine has no fopen access to.

### The new surface

Attached both to the `$` init object (for `packages/runtime` internal use)
and to a durable `globalThis.nxjsPageModules` namespace (for downstream
embedders, since `$` is captured + deleted at nx.js runtime init):

```ts
// Register an importmap for a page scope. Merges on repeat calls (last
// write wins per specifier). Silently no-ops on malformed JSON.
$.moduleSetImportmap(pageBase: string, mapJson: string): void
globalThis.nxjsPageModules.setImportmap(pageBase, mapJson)

// Register source text for a URL. The resolver consults this map BEFORE
// falling through to fopen. Lets an embedder execute modules over URL
// schemes the engine can't reach directly (http(s)://, custom schemes)
// as long as the embedder's own fetch() reached them first.
$.moduleSetSource(url: string, source: string): void
globalThis.nxjsPageModules.setSource(url, source)

// Compile + instantiate + evaluate `source` as a module identified by
// `url`, resolving bare specifiers via `pageBase`'s importmap. Returns
// a Promise mirroring the evaluation promise: fulfills with the module
// namespace, rejects on any compile/instantiate/evaluate failure,
// chained through top-level await.
$.moduleRun(source: string, url: string, pageBase: string): Promise<any>
globalThis.nxjsPageModules.run(source, url, pageBase)

// Purge everything tagged with this page scope (importmap, module cache,
// prefetched sources, page-base tagging). Call on page navigation.
$.moduleClearPage(pageBase: string): void
globalThis.nxjsPageModules.clearPage(pageBase)
```

`nxjsPageModules` is registered as `DontEnum | DontDelete` so it stays
out of `for…in` / `Object.keys(globalThis)`, is non-deletable, but
remains writable in case an embedder wants to wrap or proxy it. The
runtime never touches this global.

### Usage sketch

```js
const pageBase = 'brewser://apps/foo/index.html';

// From the HTML: <script type="importmap">{"imports":{"three":"./assets/three.module.js"}}</script>
$.moduleSetImportmap(pageBase, importmapJsonText);

// Walk the entry module's static imports (regex or ESTree scan),
// fetch each URL via fetch(), register with the engine.
for (const [url, source] of await prefetchGraph(entryUrl, pageBase)) {
  $.moduleSetSource(url, source);
}

// From the HTML: <script type="module">import * as THREE from 'three'; ...</script>
await $.moduleRun(inlineSource, `${pageBase}#inline-0`, pageBase);

// On navigation:
$.moduleClearPage(pageBase);
```

The engine does not fetch. The engine does not walk imports pre-instantiate.
The engine's contract is only: given an importmap + a source registry + an
entry, run V8's real module machinery under spec-conformant semantics. This
split keeps the C++ delta small, keeps async I/O on the JS side where
`fetch()` already lives, and preserves the engine's compile cache and module
identity for cross-graph deduplication.

## What this preserves

- **Entrypoint module flow is unchanged.** `nx_run_entry_module` still passes
  no page scope. `load_module` still falls through to `fopen` when there's
  no prefetched source. `resolve_specifier_with_map` degrades to
  `resolve_specifier` when the page scope is empty. Every existing embedder
  that only loads a single entrypoint sees zero behavioral change.
- **Dynamic `import()` from a page module inherits the page scope.** The
  existing `dynamic_import_callback` was extended with the same
  page-base-aware resolver + prefetch check. `import('three')` from an
  inline module works the same as `import * as THREE from 'three'`.
- **`import.meta.url` is populated for page modules.** They go through the
  same `register_module` path; `init_import_meta` reads their URL from
  `g_module_urls` unchanged.
- **Cycles resolve.** `register_module` runs before `InstantiateModule`,
  matching the existing pattern.
- **Top-level await is awaited by the caller.** `moduleRun` chains the
  evaluation promise into the returned Promise via `.then(() => ns)` when
  pending.

## What's intentionally not in this PR

- **`scopes` section of importmaps.** Parser accepts and ignores. Common
  use cases (single-page demos, most public importmaps) only touch
  `imports`. Adding scoped resolution is `~15 LOC` of nested-map lookup;
  happy to fold in if reviewers want it in this PR.
- **`v8::SyntheticModule` for host-provided modules.** Would slot in as
  `moduleSetSynthetic(url, exportsObject)` for `import { X } from 'nx:foo'`
  patterns. `~50 LOC`; leaving for a follow-up PR because it's a distinct
  capability with its own review surface (export-name discovery, evaluation
  callback shape).
- **Async C++→JS fetch callback.** The JS-drives-fetch design is a
  deliberate choice — it keeps the engine synchronous, avoids a new
  cross-language async boundary, and reuses the embedder's already-working
  `fetch()`. If a future use case genuinely needs the engine to initiate a
  load (dynamic import of a URL not pre-scanned), a JS-side registered
  fetcher wrapped through `nx_queue_async` is the natural extension.

## Diff summary

```
 source/module.cc | ~+290 LOC  (state + resolver ext + 4 bindings + durable global + teardown)
 source/module.h  |    +6 LOC  (declaration + updated header block comment)
 source/main.cc   |    +1 LOC  (nx_module_bindings call in build_init_object)
 3 files changed
```

No new source files. No Makefile changes. No new dependencies (`ada` was
already in use; `v8::JSON::Parse` is standard V8). No changes to the
existing entrypoint-module contract or to any other binding.

## Build / test status

Compiles clean against V8 monolith. The four new functions register with
the existing `NX_SET_FUNC` macro pattern; teardown is called from the
existing `nx_modules_teardown` shutdown path. Entrypoint modules
(pre-existing test coverage) work byte-identically — the `page_base`
parameter to `load_module` defaults empty, exercising the pre-existing
codepath.

## Interaction with other PRs

- **PR-A** (fetch deferral in image/audio/video), **PR-C** (canvas.cc
  font-size pin), **PR-F** (JIT-safe defineProperties): all independent
  files. No merge order preference.
- **PR-D** (Skia/WebGL coexistence): independent. No overlap.

## Downstream implication

Retires ledger entry #105 upon merge. Unlocks stock (unmodified)
Three.js demos in Brewser without shipping a patched Three.js copy or a
demo-side bridge shim. Same primitive supports any embedder wanting to
render browser-shaped ES module content or drive user-supplied module
code from JS.
