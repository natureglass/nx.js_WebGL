#pragma once
#include "types.h"

// Defined by each runtime entrypoint (device main.cc / host test main.cc);
// switches the screen to the libnx text console (no-op on the host).
void nx_console_init(nx_context_t *nx_ctx);

// ---------------------------------------------------------------------------
// ES module loading (static `import` + filesystem `await import()`).
//
// Shared by both the device runtime (source/main.cc) and the host test binary
// (packages/runtime/test/src/main.cc) so the two never drift. Specifiers are
// resolved as URLs (via `ada`) against the importing module's URL and read
// synchronously with read_file()/fopen — so entrypoint / filesystem imports
// use mounted devoptab schemes (romfs:, sdmc:, nxjs:, file:) and reject bare
// specifiers. Page-level modules (see nx_module_bindings below) additionally
// consult a per-page importmap and may source their body from a runtime-
// supplied prefetch registry rather than fopen.
// ---------------------------------------------------------------------------

// Register the host module callbacks on the isolate:
//   - SetHostInitializeImportMetaObjectCallback (import.meta.url / .main)
//   - SetHostImportModuleDynamicallyCallback   (filesystem `import()`)
// Call once, after Isolate::New.
void nx_init_modules(v8::Isolate *iso);

// Register the page-module JS surface on `init_obj` (the `$` init object):
//   - moduleSetImportmap(pageBase: string, mapJson: string): void
//   - moduleSetSource(url: string, source: string): void
//   - moduleRun(source: string, url: string, pageBase: string): Promise<any>
//   - moduleClearPage(pageBase: string): void
// The embedder is expected to walk static imports on the JS side, prefetch
// each dep's source via its own fetch (`moduleSetSource`), then call
// `moduleRun` on the entry. `moduleClearPage` purges the page's state on
// navigation. See BINDINGS.md §page-modules and NXJS_PATCHES_NEEDED.md #105.
void nx_module_bindings(v8::Isolate *iso, v8::Local<v8::Object> init_obj);

// Compile + instantiate + evaluate `src` (length `len`) as the entrypoint ES
// module, recorded under URL `name` (its ScriptOrigin resource name and
// import.meta.url base). Resolves static + dynamic imports against the
// filesystem. Returns false on failure (the error is reported via
// nx_emit_error_event). Handles top-level await: a rejected async graph is
// surfaced via the error path rather than becoming a silent unhandled
// rejection.
bool nx_run_entry_module(v8::Isolate *iso, v8::Local<v8::Context> context,
                         const char *src, size_t len, const char *name);

// Release retained module handles (call before disposing the isolate).
void nx_modules_teardown();

// ---------------------------------------------------------------------------
// V8 bytecode code cache (boot-time compile skip).
//
// Cold-compiling the two big boot scripts — the embedded runtime.js (~1.2 MB)
// and the app entry module main.js (~1.9 MB) — costs ~1 s on hardware (V8 runs
// single-threaded/--predictable, so parse+bytecode-gen blocks the boot thread).
// These wrappers cache the compiled bytecode to sdmc and consume it on later
// boots, skipping the parse. V8's CachedData self-validates against source + V8
// version + flags, so a stale cache is transparently rejected and re-produced;
// the cache filename is additionally keyed by source length + a content hash so
// distinct bundles never alias one file. Any I/O or cache miss falls back to a
// normal cold compile — the cache can never wedge boot. `tag` names the cache
// file; `src`/`len` are the raw source bytes (for keying).
v8::MaybeLocal<v8::Script> nx_compile_script_cached(
    v8::Isolate *iso, v8::Local<v8::Context> ctx, v8::Local<v8::String> source,
    v8::ScriptOrigin &origin, const char *tag, const char *src, size_t len);
v8::MaybeLocal<v8::Module> nx_compile_module_cached(
    v8::Isolate *iso, v8::Local<v8::String> source, v8::ScriptOrigin &origin,
    const char *tag, const char *src, size_t len);
