# nx.js engine patches needed on the V8/upstream base

Permanent catalogue of QuickJS-era engine patches that the V8/upstream
migration dropped and that must be **re-applied after any future upstream
nx.js pull**. Use this file as the re-application checklist at
upstream-update time. Grows as Step 2 (and beyond) surfaces more
fork-patches the migration lost.

## DISPOSITION POLICY

Prefer fixes in **brewser-runtime** over engine edits. For general
**engine-correctness** fixes, prefer **UPSTREAMING** (PR to TooTallNate)
so the entry can drop to zero fork-delta on the next pull. Engine edits
that stay in the fork are the last resort and should be minimized — this
file tracks that debt.

Disposition values:
- **brewser-specific** — the bug is specific to how brewser embeds nx.js;
  the right fix is in brewser-runtime or brewser-v8, NOT in the engine.
  Entries with this disposition should be migrated out of the fork (fix
  the caller in brewser-runtime) and then deleted from this list.
- **upstream-candidate** — general engine-correctness fix that any
  embedder benefits from. File a PR; track UPSTREAM STATUS. Drop the
  entry when merged-in.
- **fork-only** — Switch-/Tegra-/brewser-product-specific behavior that
  upstream would not accept. Keep in the fork; document in detail so
  it survives an upstream pull.

UPSTREAM STATUS values:
- `not-submitted` — known good fix, no PR open yet.
- `PR-open(#N)` — PR is in flight; link the PR number.
- `merged-in(vX)` — landed in upstream at version X; check whether the
  next pull obsoletes this entry.
- `n/a` — not applicable (brewser-specific / fork-only disposition).

---

## #1 — image.ts: call-time globalThis.fetch deferral

**File(s):** [packages/runtime/src/image.ts](packages/runtime/src/image.ts)

**Exact change.** Remove `import { fetch } from './fetch/fetch';`.
Replace with a local function that resolves `globalThis.fetch` per call:

```ts
function fetch(
    input: string | URL | Request,
    init?: RequestInit,
): Promise<Response> {
    return globalThis.fetch(input, init);
}
```

Place anywhere in the file before the `Image.src` setter at the call
site `fetch(url)`. Keep the existing `import { URL } from './polyfills/
url';` — `new URL(...)` is still used to construct the request URL.

**Symptom it fixes.** `<img src="brewser://...">` (and any
embedder-extended scheme) renders blank. The engine's `Image.src` setter
calls the package-local `./fetch/fetch`, whose scheme registry only
contains `http: https: blob: data: file: sdmc: romfs:`. Any other
scheme rejects synchronously at scheme lookup
([fetch.ts:481-484](packages/runtime/src/fetch/fetch.ts#L481)) with
`Error: scheme '<x>' not supported` before reaching the embedder's
`globalThis.fetch` wrapper (e.g. brewser-runtime's `BrowserResourceLoader`
that handles `brewser://`). User-visible: app-card logos on home + apps
pages blank, modal logo placeholder blank.

**Why upstream-vanilla lacks it.** The QuickJS-era fork patched
`image.ts` to use `globalThis.fetch` (see the historical reference
`[[reference-nxjs-image-audio-page-url-base]]` cited in
[brewser-runtime-v8/src/scripts/live-dom.ts:184-196](../brewser-runtime-v8/src/scripts/live-dom.ts)).
The V8 migration brought vanilla upstream nxjs 1.0.0-beta.5 and dropped
that patch silently — the upstream Image still does
`import { fetch } from './fetch/fetch'`, which is the wrong choice for
any embedder that registers schemes.

**DISPOSITION:** `upstream-candidate`. This is general engine-
correctness — the architecture explicitly allows embedders to extend
schemes via `globalThis.fetch` (see `installRuntimeFetch` design in
brewser-runtime-v8/src/resources/runtime-fetch.ts), so the engine's own
`Image` should honor that. No reason for upstream not to take it.

**UPSTREAM STATUS:** `not-submitted` — flag for a TooTallNate PR.

**RE-APPLY / VERIFY NOTE.**

*To verify the patch is still needed* after an upstream pull: search
the engine's `packages/runtime/src/image.ts` for the line
`import { fetch } from './fetch/fetch'`. If present, the patch is still
needed. If absent (replaced by a `globalThis.fetch` lookup), the
upstream caught up — verify the new shape is call-time (not
import-time), drop this entry.

*To re-apply.* The change shown above. **GOTCHA: must be CALL-TIME.**
This module loads at engine boot, before any embedder installs its
session-time `globalThis.fetch` wrapper. An import-time capture
(`const fetch = globalThis.fetch;` at module top-level) would freeze the
pre-wrapper engine fetch, and the deferral would silently do nothing —
the bug appears fixed in code review but reappears at runtime. The
function-body form above guarantees the lookup happens per-call.

---

## #2 — audio.ts: call-time globalThis.fetch deferral

**File(s):** [packages/runtime/src/audio.ts](packages/runtime/src/audio.ts)

**Exact change.** Identical pattern to #1. Remove
`import { fetch } from './fetch/fetch';` and add the same local
`function fetch(...) { return globalThis.fetch(input, init); }` near
the top of the file. Single call site at `Audio.load()`'s
`fetch(url).then(...)`.

**Symptom it fixes.** `<audio src="brewser://...">` and analogous
`brewser://` audio loads fail the same way as images. Not directly
observed in this round's boots because the only audio that loaded was
`click.wav` via `sdmc:/` (which the local fetch handles) — but any
brewser://-rooted audio asset (e.g. notification sound from an app's
own folder) would silently fail to load.

**Why upstream-vanilla lacks it.** Same story as image.ts — the
QuickJS-era fork patched both image.ts AND audio.ts (the historical
reference is named "image-audio-page-url-base" for that reason); V8
migration dropped both patches.

**DISPOSITION:** `upstream-candidate`. Same engine-correctness reason
as #1; ideally bundle both into a single upstream PR.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Verify identically to #1 (grep image.ts +
audio.ts for the local import). Same CALL-TIME gotcha applies. Same
fix shape.

---

## #3 — video.ts: call-time globalThis.fetch deferral

**File(s):** [packages/runtime/src/video.ts](packages/runtime/src/video.ts)

**Exact change.** Same pattern as #1/#2. Local late-bound wrapper
function replacing the `./fetch/fetch` import.

**Symptom it fixes.** `<video src="brewser://...">` fails to load. Like
audio, only manifest-style URLs trigger this — video.ts already
short-circuits its `FILE_SCHEMES` (romfs/sdmc/file/nxjs) to a direct
native `$.videoLoad` and bypasses fetch entirely; the fetch branch
only fires for `http/https/blob/data` schemes and now (with the fix)
embedder-extended schemes including `brewser://`.

**Why upstream-vanilla lacks it.** Almost certainly part of the same
historical fork patch as image/audio; this round added it for
completeness (the audit grep for `./fetch/fetch` importers caught it).

**DISPOSITION:** `upstream-candidate`. Bundle into the same upstream
PR as #1 + #2.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Verify identically to #1/#2. Same CALL-TIME
gotcha. Same fix shape.

---

## #4 — Screen.setCursorOverlay / setAnimatedCursorOverlay — DEFERRED, NEEDS ENGINE BINDING

**File(s):** Native engine — port from QuickJS-era nx.js fork at
`D:/Workspace/nxjs-source/source/main.c` lines ~180-694 (state +
`nx_set_cursor_overlay`, `nx_set_animated_cursor_overlay`,
`nx_set_cursor_overlay_position`, `nx_clear_cursor_overlay`,
`composite_cursor_overlay`). Runtime-side TS wrapper already lives in
[brewser-runtime-v8/src/graphics/screen.ts](../brewser-runtime-v8/src/graphics/screen.ts)
(typed surface) and is called from
[brewser-runtime-v8/src/input/page-mouse-forwarder.ts](../brewser-runtime-v8/src/input/page-mouse-forwarder.ts).
The fork registers methods on the `Screen` class JS prototype — the V8
equivalent attaches to the same Screen prototype reachable via
`globalThis.screen`.

**Symptom it fixes.** `TypeError: screen2.setCursorOverlay is not a
function` flood in nxjs-debug.log on every boot (both Citron and
hardware). **NOT just diagnostic noise** — the cursor is INVISIBLE
when the user engages it. There is no soft-cursor fallback on the
runtime side: the brewser-runtime code in
`page-mouse-forwarder.ts::syncCursorOverlay()` draws into a private
`OffscreenCanvas`, then hands the bitmap to `screen.setCursorOverlay`
for the engine to composite onto `display_buffer` at present time.
The OffscreenCanvas itself is never composited anywhere; if
`setCursorOverlay` throws, the surrounding try/catch swallows it and
the cursor is not rendered at all. The earlier catalog entry's claim
("the soft cursor already renders fine without them — the throws are
visible only in the log, not in the UI") was incorrect — verified
2026-06-28 by re-reading both the runtime call site and the
QuickJS-era engine implementation.

**Why upstream-vanilla lacks it.** The fork added a native Screen
binding (a libnx software-cursor compositor that BGRA src-over blends
the cursor bitmap onto `display_buffer` after the page renders) that
upstream nxjs doesn't have. V8 migration brought vanilla upstream
Screen which doesn't expose `setCursorOverlay` /
`setAnimatedCursorOverlay` / `setCursorOverlayPosition` /
`clearCursorOverlay`.

**DISPOSITION:** `fork-only`. The cursor compositor is
brewser-/Switch-product-specific: it relies on the engine owning the
`display_buffer` and running its own composite pass after the page is
done rendering, which is a brewser-shaped embedding pattern. Upstream
nx.js wouldn't take it because most nx.js apps don't have a
mouse-driven UI at all. Two non-fork alternatives were considered and
rejected:
- **Runtime-side feature-detect + no-op.** Removes the TypeError
  flood but leaves the cursor invisible — a UX regression, not a
  fix. Not acceptable.
- **Runtime-side soft-cursor on the page canvas / DOM overlay.** Was
  the pre-fork approach in QuickJS times. Caused the "cursor leaves a
  black trail" regression (noted at
  [page-mouse-forwarder.ts:1090-1094](../brewser-runtime-v8/src/input/page-mouse-forwarder.ts#L1090-L1094))
  in regions where Cairo's underlying data was premultiplied-zero. On
  V8 + Skia rather than QuickJS + Cairo this *may* not reproduce, but
  the work to verify + the additional DOM/canvas mechanism is
  comparable in size to porting the C compositor, with more surface
  area for new bugs.

**UPSTREAM STATUS:** `n/a` (fork-only).

**DEFERRED** to a dedicated round (not in scope for the Step 1 →
Step 2 transition). When picked up:

**RE-APPLY / PORT NOTE.** The QuickJS-era implementation in
`nxjs-source/source/main.c` is largely self-contained and JS-ABI-free
inside `composite_cursor_overlay`; the four JS entry points only need
their `JSValue`/`JSContext` ↔ V8 plumbing rewritten. The composite
function itself reads from C buffers, writes to `display_buffer`,
uses `armGetSystemTick()` for the animation frame pick — no JS engine
contact at all. Wire the composite call from the engine's per-frame
present hook (same place the QuickJS fork called it: after
`js_framebuffer` → `display_buffer` copy, before the swap). The
runtime TS surface in `brewser-runtime-v8/src/graphics/screen.ts`
already matches the four-method signature; no runtime-side change
needed once the binding is in place.

---

## #5 — skia_gpu: ES3 shared context + handle accessors for the WebGL bridge

**File(s):** [source/skia_gpu.h](source/skia_gpu.h), [source/skia_gpu.cc](source/skia_gpu.cc)

**Exact change (V8 migration Phase 2.A).**
- skia_gpu.cc: EGL context bumped `EGL_CONTEXT_CLIENT_VERSION` 2 → 3; config attrs grow `EGL_RENDERABLE_TYPE = EGL_OPENGL_ES3_BIT` (0x0040 magic — matches the fbo-spike + upstream V8 webgl.cc pattern, robust to header version drift); `#include <GLES2/gl2.h>` → `#include <GLES3/gl3.h>`; init logs `[skia] GL version=... vendor=... renderer=...` once post-bringup so an ES2 silent fallback is auditable.
- skia_gpu.h: `#include <EGL/egl.h>` + forward-decl `class GrDirectContext` + four accessors — `nx_skia_gpu_egl_display()`, `nx_skia_gpu_egl_surface()`, `nx_skia_gpu_egl_context()`, `nx_skia_gpu_gr_context()`. Each returns null/zero before init / after exit. Ownership stays with skia_gpu.cc; the bridge must not destroy these handles, and must not leave a different context current when handing back to Skia.

**Symptom it fixes.** Without this, the V8 migration WebGL bridge can't attach to the same EGL context that Skia owns — upstream webgl.cc creates its own competing chain on the same NWindow, and Phase 0 proved that's not what runs (the proven recipe is ONE shared ES3 context, with `gl_state_save → webgl → restore → GrDirectContext::resetContext()` around each WebGL section). Without ES3 specifically, the WebGL bridge wouldn't have access to ES3 core entry points (glDrawArraysInstanced, glVertexAttribDivisor, etc.); Mesa returns no-op stubs for ES3 entry points on an ES2-requested context.

**Why upstream-vanilla lacks it.** Upstream's `skia_gpu.cc` was authored before V8-side WebGL ([Nathan Rajlich, fb0468f](https://github.com/TooTallNate/nx.js/commit/fb0468f)) and so creates a context at the lowest sufficient version (ES2) and exposes no shared-context surface. The two paths (Canvas-2D-via-Skia and WebGL) were designed mutually exclusive; sharing is the V8-migration architectural change.

**DISPOSITION:** `upstream-candidate`. The shared-context capability is general (any embedder benefits from coexisting Skia + raw GLES rendering on one context). The accessor surface is small + non-brewser-specific. ES3 is a strict superset of ES2 — Ganesh-GL is happy on ES3 (proven across 4,013 frames in the Phase 0 spike + verified by the `[skia] GL version=` log at boot post-2.A). The whole upstream WebGL2 path also assumes ES3 already, so consolidating the EGL/context creation onto skia_gpu and having webgl.cc attach is an upstream win — drops their duplicated EGL init.

**UPSTREAM STATUS:** `not-submitted`. Bundle into a Step-2 PR once 2.D is GREEN (proof point: geometry-cube renders end-to-end through the shared context). Premature to PR before there's a demonstrated working bridge on top.

**RE-APPLY / VERIFY NOTE.**

*To verify the patch is still needed* after an upstream pull: search `skia_gpu.cc` for `EGL_CONTEXT_CLIENT_VERSION,\s*3`. If absent (still 2), the patch is needed. If present (upstream caught up), check whether the accessors `nx_skia_gpu_egl_{display,surface,context}` + `nx_skia_gpu_gr_context` are exposed in `skia_gpu.h`; if not, the accessor half of this entry is still needed.

*To re-apply.* Change shape is exactly as above. **GOTCHA: `EGL_RENDERABLE_TYPE = EGL_OPENGL_ES3_BIT` is REQUIRED in the config attribs.** Without it eglChooseConfig may hand back an ES2-only config and the subsequent eglCreateContext(ES3) fails — *or worse, Mesa silently grants ES2 and returns no-op stubs for ES3 entry points*. The `[skia] GL version=` log makes this auditable; verify the second token starts with `3.` not `2.`.

---

## #6 — webgl_bridge: state save/restore + tenant FBO + Skia composite (Phase 2.B coexistence primitive)

**File(s):** [source/webgl_bridge.h](source/webgl_bridge.h),
[source/webgl_bridge.cc](source/webgl_bridge.cc) (NEW), plus the
config + main.cc wiring described below.

**Exact change (V8 migration Phase 2.B).**

New `source/webgl_bridge.h`/`.cc`. Three public surfaces:

1. **The GL state save/restore primitive** (the load-bearing contract any
   tenant GLES renderer sharing Skia's context MUST wrap itself in):
   - `struct nx_gl_state_snap_t` carrying the empirically-validated set
     from the Phase 0 fbo-spike: `DRAW_FRAMEBUFFER_BINDING`, `VIEWPORT`,
     `CURRENT_PROGRAM`, `VERTEX_ARRAY_BINDING`, `ARRAY_BUFFER_BINDING`,
     `ACTIVE_TEXTURE`, `TEXTURE_BINDING_2D`, `BLEND`/`DEPTH_TEST`/
     `CULL_FACE`/`SCISSOR_TEST`/`STENCIL_TEST` enables, `COLOR_WRITEMASK`,
     `COLOR_CLEAR_VALUE`, `BLEND_SRC_RGB`/`DST_RGB`/`SRC_ALPHA`/`DST_ALPHA`.
   - `void nx_gl_state_save(nx_gl_state_snap_t *)` /
     `void nx_gl_state_restore(const nx_gl_state_snap_t *)`. The caller
     calls `nx_skia_gpu_gr_context()->resetContext()` AFTER restore so
     Ganesh re-syncs its cached state. Skipping that = Skia next-draw
     garbage. Skipping any save/restore entry = intermittent corruption
     (not a clean crash). The contract is locked in here; 2.C/2.D wrap
     every WebGL frame in this bracket without rederivation.

2. **Tenant offscreen FBO lifecycle** — `nx_webgl_bridge_init(fbo_w, fbo_h)`
   allocates a color texture (`GL_RGBA8`, `LINEAR`/`CLAMP_TO_EDGE`) + depth
   renderbuffer (`GL_DEPTH_COMPONENT24`) + FBO on Skia's shared ES3 context
   via the 2.A `nx_skia_gpu_egl_*` accessors. Wraps the color texture as
   an `SkImage` via `SkImages::BorrowTextureFrom` (once, reused per
   frame) so Skia can composite the FBO contents without copying.
   `nx_webgl_bridge_exit()` drops the SkImage FIRST then frees the GL
   handles; idempotent.

3. **Phase 2.B test compose path** — `nx_webgl_bridge_compose_test(SkSurface*)`
   renders an animated clear+triangle into the tenant FBO (state-save
   bracketed + `resetContext()`), then `drawImageRect`s the SkImage at
   the center of the target plus a 2D banner overlay + frame/boundary HUD
   on top. Gated by `[webgl] test_fbo = true` config flag (defaults
   false). Replaced by the WebGL bridge proper in Phase 2.C+; the save/
   restore primitive above is what survives.

Wiring in [source/main.cc](source/main.cc):
- Include `webgl_bridge.h`.
- After successful `nx_skia_gpu_screen_init` + `nx_canvas_set_gpu_surface`
  — and only if `ctx->config.webgl_test_fbo` — call
  `nx_webgl_bridge_init(640, 360)`.
- In the GPU canvas present branch, immediately before
  `nx_skia_gpu_present()`, call `nx_webgl_bridge_compose_test(
  nx_skia_gpu_canvas_surface())` (no-op when bridge isn't initialized).
- At each `nx_skia_gpu_screen_exit()` site, call `nx_webgl_bridge_exit()`
  FIRST so GL handles free while the shared context is still valid
  (idempotent; safe to call when uninitialized).

Wiring in [source/skia_gpu.h](source/skia_gpu.h) / `skia_gpu.cc`:
- New accessor `SkSurface *nx_skia_gpu_canvas_surface(void)` returning
  the persistent canvas SkSurface so the bridge can compose INTO the
  same surface the engine present blits to the back buffer.

Wiring in [source/config.h](source/config.h) / `config.cc`:
- `nx_config_t::webgl_test_fbo` (bool, default false), parsed under
  `[webgl] test_fbo`. Accepts the engine-wide boolean spellings
  (`true|false|on|off|1|0|yes|no`).

**Symptom it fixes.** Phase 2.B without this entry would have nothing to
re-apply at upstream pull time. The state save/restore set is the
load-bearing primitive 2.C+ wraps every WebGL frame in to coexist with
Skia. Skipping the resetContext() = next Skia draw renders garbage;
skipping any save/restore entry = intermittent visual corruption
shape-dependent on which GL state the new code path touched. The set
was empirically validated in the 4,013-frame fbo-spike and locked in
here.

**Why upstream-vanilla lacks it.** Upstream V8 webgl.cc creates a
parallel EGL chain (mutually exclusive with Skia per its own header
note); no coexistence primitive needed, none provided. This entry is
the design-pattern shift Step 2 makes.

**DISPOSITION:** `upstream-candidate`. The save/restore primitive +
SkImages::BorrowTextureFrom composite of a tenant FBO is GENERAL —
any embedder running both Ganesh-GL and a raw GLES renderer benefits.
Pairs with #5 (the 2.A accessors); ideally bundle both into the same
upstream PR. The `[webgl] test_fbo` config flag is Phase 2.B
scaffolding and would be DROPPED from any upstream submission (it's
the smoke driver, not the primitive).

**UPSTREAM STATUS:** `not-submitted`. Bundle into a Step-2 PR alongside
#5 once 2.D is GREEN.

**RE-APPLY / VERIFY NOTE.**

*To verify the patch is still needed* after an upstream pull: check
whether `source/webgl_bridge.h`/`.cc` survived. Look for
`nx_gl_state_snap_t` + the `nx_gl_state_save`/`restore` symbols in
`nxjs.elf`'s symbol table (`strings | grep nx_gl_state_save`).

*To re-apply.* Patch as described above. **GOTCHA: dropping the SkImage
in nx_webgl_bridge_exit BEFORE the GL handles.** Doing it in the
opposite order leaves Ganesh's GrBackendTexture pointing at a deleted
GL texture name; the next Skia frame's draw asserts inside Ganesh's
texture-validation path (or worse, reads garbage if asserts compile
out).

**Recurrence tells** (if 2.C ships and the bridge starts producing
"Skia renders garbage after WebGL draws" on a hardware-only pattern,
the FIRST place to look is whether the save/restore set lost an entry
under a new code path — the symptom is shape-dependent, intermittent
corruption, NOT a clean crash; check whether any GL state newly
touched by the new path is in `nx_gl_state_snap_t`).

---

## #7 — WebGL1 context exposed via screen.getContext('webgl') + minimal method surface

**File(s):** [source/webgl.cc](source/webgl.cc) (replaces the Step 1 null
stub), [source/webgl.h](source/webgl.h) (new `nx_webgl_compose_if_active`),
[source/webgl_bridge.h](source/webgl_bridge.h)/`.cc` (new accessors:
`nx_webgl_bridge_fbo_id` / `_fbo_size` / `_mark_fbo_dirty` /
`_compose` / `_set_webgl_owned` / `_is_webgl_owned`),
[source/main.cc](source/main.cc) (per-frame present hook calls
`nx_webgl_compose_if_active` before `nx_skia_gpu_present`),
[packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
(new TS class + GL constants + factory),
[packages/runtime/src/screen.ts](packages/runtime/src/screen.ts)
(routes `'webgl'`/`'experimental-webgl'` to `createWebGLContext`,
gates `'webgl2'` to null until Phase 2.G),
[packages/runtime/src/$.ts](packages/runtime/src/$.ts) (widens
`$.webglContextNew` / `$.webglInitClass` to accept either v1/v2).

**Exact change (V8 migration Phase 2.C).**

- Engine: `screen.getContext('webgl')` returns a non-null
  `WebGLRenderingContext` backed by raw GLES3 dispatches into the 2.B
  tenant offscreen FBO. Each method lazily ENTERs the per-frame 2.B
  bracket (state save + bind tenant FBO); the present hook EXITs the
  bracket (restore + `grCtx->resetContext()`) and composes the FBO into
  Skia's persistent canvas surface via `SkImages::BorrowTextureFrom`.
- ~95 WebGL 1 methods implemented as thin glXxx wrappers (state /
  query / shader / program / buffer / vertex attrib / uniform /
  texture / framebuffer / renderbuffer / draw / readPixels). Phase 2.E
  bulk-moves semantics + validation to brewser-runtime TS; 2.C is just
  enough plumbing to drive geometry-cube without crashing.
- `'webgl2'` is deliberately null until 2.G — returning a non-null v2
  context with only v1 methods would silently route Three.js into its
  v2 code path (which detects via `gl.constructor.name`) and throw on
  the first v2 call.
- Two fork-specific hooks (`enableGpuBridgePrototype`,
  `setBridgeAutoFlush`) return `true` as no-ops; without them
  brewser-runtime canvas-runner.ts refuses to use the context (see
  [canvas-runner.ts:275-292](../brewser-runtime-v8/src/scripts/canvas-runner.ts)).
  These were runtime toggles in the QuickJS-era fork; the V8 bridge is
  always-on, so they exist only for canvas-runner compatibility. Drop
  the canvas-runner check + delete these in 2.E.

**Symptom it fixes.** Step 1 left `getContext('webgl')` returning null
across the board, blocking the slice demo (and any WebGL app) entirely.
2.C is the engine surface that makes WebGL 1 contexts available; without
it, 2.D (visual correctness) has nothing to render against.

**Why upstream-vanilla lacks it.** Upstream V8 webgl.cc (commit fb0468f)
ships only WebGL 2, with its own EGL chain on the default NWindow. The
shared-context recipe needed by 2.A + 2.B requires a different
architectural layout (tenant FBO + bracket), which upstream doesn't
have. Once 2.E/2.G consolidate, this is the entry that's the candidate
for upstream — but it depends on #5 and #6 first.

**DISPOSITION:** `upstream-candidate` (with caveats). The shared-context
WebGL 1 path + bracket integration is general (any embedder benefits).
The two fork-specific hooks (`enableGpuBridgePrototype`,
`setBridgeAutoFlush`) would NOT be upstreamed — they're scaffolding
for canvas-runner.ts compatibility and would be deleted before any
upstream PR (after canvas-runner drops the check, planned for 2.E).

**UPSTREAM STATUS:** `not-submitted`. Bundle into the same Step-2 PR as
#5 + #6 once 2.D is GREEN. Drop the fork-specific hooks first.

**RE-APPLY / VERIFY NOTE.**

*To verify the patch is still needed* after an upstream pull: grep
`source/webgl.cc` for `nx_webgl_compose_if_active`. If absent, 2.C
hasn't been re-applied. Check for `enableGpuBridgePrototype` to verify
the canvas-runner hooks survived.

*To re-apply.* The change is significant (~900 lines new in webgl.cc +
~530 lines new in webgl-rendering-context.ts + ~100 lines of bridge
accessors). Source of truth: this repo's `v8-migration` branch. Don't
hand-reapply; cherry-pick the relevant commit(s).

**RECURRENCE TELLS / KNOWN-MISSING for 2.D iteration:**

- `[webgl] context_new ok WxH` log line at boot when geometry-cube is
  launched = factory fires. Absence = the path didn't reach webgl.cc
  (canvas-runner's screen.getContext probe failed or the SkiaGPU init
  isn't ready).
- `TypeError: gl.X is not a function` in nxjs-debug.log = 2.C's method
  surface is missing X. The diagnostic Proxy in
  brewser-runtime-v8/src/shims/webgl-shim.ts (gated by
  `__brewserGLProxyDebug`) also logs `UNDEFINED gl.X` for any property
  access against an undefined method. Add the missing method in
  `install_methods()` per-iteration.
- A first-cut launch is expected to either crash with TypeError on a
  missing method OR render the cube wrong (black / untextured /
  unlit). Wrong-rendering is 2.D's gate, not 2.C's — only TypeErrors
  block 2.C.

---

## #8 — V8 JIT crashes hardware boot when WebGL classes are init'd (Phase 2.C regression) — FIXED 2026-06-28

**File(s):**
[packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
+ [packages/runtime/src/canvas/webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts) —
the two module-body `for (const [k, v] of Object.entries(GL_CONSTANTS))`
loops that each do `Object.defineProperty(class, k, …)` +
`Object.defineProperty(class.prototype, k, …)` for ~210 (v1) + ~370
(v2) numeric GL constants. The combined ~1160 bulk property installs
in tight for-of-with-destructuring loops are the sole trigger of the
V8-JIT-on-Tegra crash.

**Symptom.** Tegra hardware boot with V8 JIT enabled (Sparkplug+Maglev+
TurboFan) dies silently during runtime.js evaluation, BEFORE `[skia]`
init. Engine prints the V8 init lines (`[v8] code arena ...`,
`[v8] mem_total=...`, `[v8] max_heap=...`) then terminates with no
fatal output, no segfault, no further log. Workaround: drop
`[v8] jit = off` into `sdmc:/switch/nxjs-override.ini` on the Switch
SD — engine boots cleanly under Ignition-only and runs the full 2.C
bridge functionally on Tegra silicon.

**Bisect evidence (2026-06-28 hardware-jit-on spike, 5 rounds):**

| Run | constants loop | $.webglInitClass | install_methods cap | Outcome |
|---|---|---|---|---|
| 1.0 | on | on | -1 (all 244) | Crash |
| 1.1 | on | on | **0** | Crash (same crash point) → install_methods body EXONERATED |
| 2.1 | on | off | -1 | Crash (logs truncated; constants loop still on) |
| 2.2 | **off** | off | -1 | **Clean boot** through to `[skia]` + full app |
| 3.A | **off** | **on** | -1 | **Clean boot** → engine $.webglInitClass call EXONERATED |

3.A is the decisive run: both `$.webglInitClass` calls executed in
full (244 FunctionTemplate registrations across v1+v2), only the TS
constants loops were skipped, and hardware booted clean to skia +
ran the shell + reached the cube demo. The pattern that triggers the
JIT crash is exclusively the
`for (const [k,v] of Object.entries(GL_CONSTANTS))` +
`Object.defineProperty` × ~1160 in tight loops at module-body scope.

**Crash mechanics (inferred from bisect):** V8 JIT (likely Sparkplug
or Maglev) attempts to tier up the module-body function containing
the for-of loop and either crashes during codegen or installs code
that crashes on execution. The crash is async w.r.t. the interpreter
(which is why runs 1.0/1.1 *appeared* to crash AFTER the unrelated
`install_methods` work — tier-up compilation is decoupled from
interpreter execution; the constants loop schedules the bad compile
and unrelated work buys wall-clock for the compile thread to crash
the process).

**Why upstream-vanilla lacks it.** Upstream V8 nx.js ships WebGL2 only
(one constants loop, ~370 entries × 2 defineProperty = ~740 installs),
which apparently stays under whatever threshold tips Tegra V8 JIT into
the bad codegen path. Our 2.C ships v1 + v2 = ~1160 installs in two
consecutive module-body loops, which trips it.

**DISPOSITION:** **upstream-candidate.** Candidate-1 TS rewrite landed
2026-06-28 in both `packages/runtime/src/canvas/webgl-rendering-context.ts`
and `webgl2-rendering-context.ts`. Pure TS pattern change in the engine's
runtime bundle — ZERO C++ delta. Any embedder using the engine's WebGL
runtime benefits; bundle into the same Step-2 upstream PR as #5/#6/#7.

**Classification:** technically an (A) V8-aarch64 codegen bug, mitigated
as (B) by changing our TS pattern. The JS pattern itself is standard
and idiomatic — millions of programs use `Object.entries` + for-of —
but at this scale on Tegra's V8 port it crashes the JIT. We've stopped
triggering it; the underlying V8 codegen bug still exists.

**UPSTREAM STATUS:** `not-submitted`. Candidate-3 (V8/switch-v8 minimal
repro) remains a worthwhile low-priority follow-up — we've stopped
triggering the V8 bug, not fixed it. Minimal repro facts: a module-body
function running `for (const [k, v] of Object.entries(obj))` calling
`Object.defineProperty(target, k, {value: v})` ~1000+ times across two
consecutive class-prototype installs on aarch64 with full JIT crashes
silently during async tier-up. Two consecutive ~500-entry loops are
sufficient (v1 ~210 + v2 ~370); single ~370-entry loop (upstream V8
nx.js with WebGL2 only) doesn't trigger.

**FIX SHAPE (applied).** Both
[packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
and [webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts)
now use:

```ts
{
    const keys = Object.keys(GL_CONSTANTS);
    const descs: PropertyDescriptorMap = {};
    for (let i = 0; i < keys.length; i++) {
        const k = keys[i];
        descs[k] = { value: (GL_CONSTANTS as Record<string, number>)[k] };
    }
    Object.defineProperties(<Class>, descs);
    Object.defineProperties(<Class>.prototype, descs);
}
```

Replaces (DO NOT regress to):

```ts
for (const [k, v] of Object.entries(GL_CONSTANTS)) {
    Object.defineProperty(<Class>, k, { value: v });
    Object.defineProperty(<Class>.prototype, k, { value: v });
}
```

Property attributes are byte-identical (only `value` set on each
descriptor → `writable`/`enumerable`/`configurable` all default to
`false`, matching the original).

**HARDWARE GATE PROOF (2026-06-28):** brewser booted on real CFW Switch
hardware with `[detect] target=hardware ... -> mode=jit` + full
`Ignition+Sparkplug+Maglev+TurboFan` tier stack, no override ini. Log
progressed past the previously-fatal `[v8] max_heap=` → `[skia] GL
version=OpenGL ES 3.2 Mesa 20.1.0-rc3 vendor=nouveau renderer=NV120`
zone, shell came up, WebGL bridge initialized
(`[webgl-bridge] init ok fbo=1280x720`), `getContext('webgl')` returned
non-null (`[webgl] context_new ok 1280x720`), Three.js threejsdemos
loaded (geometry-cube + webgl-materials-blending visible in image
probes), sustained ~1400 mouse-fwd ticks (~25 s) of continuous
operation, clean teardown (`[webgl-bridge] exit ok`). Constants
regression check passes implicitly — Three.js's module-init touches
`gl.VERTEX_SHADER`/`gl.TRIANGLES`/`gl.FLOAT`/etc. immediately; if any
were missing or wrong it would throw inside the constructor.

**WORKAROUND STATUS:** `[v8] jit = off` in `sdmc:/switch/nxjs-override.ini`
is **NO LONGER REQUIRED.** Default hardware behavior (auto-detect → JIT)
now boots cleanly. The override remains available for diagnostic
purposes (e.g. comparing JIT vs jitless perf) but is no longer the
standing answer.

**RE-APPLY / VERIFY NOTE.**

*To verify this fix is still in place* after any future runtime
rebundle / upstream pull: grep
[packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
+ [webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts)
for `for (const [k, v] of Object.entries(GL_CONSTANTS))`. If present,
the regression is back — the bug returns silently as a hardware-JIT
boot crash but Citron (jitless) and hardware-jitless will both still
appear to work. The fix re-applies as the block shown above.

**Recurrence tells:**
- Hardware boot dies between `[v8] max_heap=` and `[skia]` lines → the
  TS fix regressed (e.g. an upstream pull restored the for-of pattern).
  Re-apply per the fix-shape block above.
- Citron always works because it forces jitless via the auto-detector
  (so Citron success does NOT prove the fix is in place — check
  hardware).
- The underlying V8 codegen bug is NOT fixed upstream; if anything
  ever again writes a module-body `for (const [k,v] of
  Object.entries(BIG_OBJ))` + `Object.defineProperty` × hundreds at
  hardware-JIT scope, expect a similar silent crash. Prefer
  `Object.defineProperties(target, descriptors)` for any future bulk
  property install.

---

## #9 — v1 WebGLRenderingContext missing ES3 sized internalformat constants — SHIPPED 2026-06-29

**File(s):** [packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
(`GL_CONSTANTS` map).

**Symptom.** Three.js code paths post-r150 read sized internalformat
enums DIRECTLY off the gl context inside `getInternalFormat()`:
```js
internalFormat = (transfer === SRGBTransfer) ? _gl.SRGB8_ALPHA8 : _gl.RGBA8;
if (glType === _gl.HALF_FLOAT) internalFormat = _gl.RGBA16F;
```
The v1 `GL_CONSTANTS` map was originally "WebGL1 subset only" and
omitted these ES3-core enums; reading them returns `undefined`, which
coerces to NaN/0 by the time it reaches the engine's `texImage2D`. Most
of the failing demos happen NOT to take this exact path (the SRGB-2D
demos that motivated #10 take the EXT_sRGB path instead, which uses
`SRGB_ALPHA_EXT = 0x8C42`), but PMREM render-target / dfgLUT / various
floating-point texture paths DO take this path, and missing constants
there silently break those uploads.

**The fix (shipped).** Added 14 ES3-core sized internalformat enums to
the v1 `GL_CONSTANTS` map (between the framebuffer block and the
WebGL-only pixel-storage block):
`SRGB8_ALPHA8 0x8C43`, `SRGB8 0x8C41`, `RGBA8 0x8058`, `RGB8 0x8051`,
`RGBA16F 0x881A`, `RGB16F 0x881B`, `R8 0x8229`, `RG8 0x822B`,
`R16F 0x822D`, `RG16F 0x822F`, `R32F 0x822E`, `RG32F 0x8230`,
`RGBA32F 0x8814`, `RGB32F 0x8815`. Values are GLES3-registry-canonical
(verified against Khronos `glcorearb.h` / Mesa `gl3.h`).

**Why upstream-vanilla lacks it.** Upstream V8 nx.js's v1 surface (if it
even exposes one — V8 nx.js is WebGL2-only out of the box) is a strict
WebGL1 spec subset. Three.js's WebGL1 reaching for ES3 constants is a
post-WebGL1-spec design choice; the engine has to play along.

**DISPOSITION:** `upstream-candidate`. Any embedder running Three.js
post-r150 on a WebGL1 context hits the same gap.

**UPSTREAM STATUS:** `not-submitted`. Bundle into the same Step-2 PR as
#5/#6/#7/#10.

**Install mechanism note.** Adding entries re-runs the GL_CONSTANTS
installation loop. That loop MUST use the #8-safe bulk
`Object.defineProperties(target, descriptors)` shape (the for-of-over-
Object.entries pattern was the JIT-crash trigger). Verified shape is
intact in `runtime.js` (4 `Object.defineProperties(WebGL…)` calls; 0
for-of-over-Object.entries).

**RE-APPLY / VERIFY NOTE.** Grep
[packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts)
for `SRGB8_ALPHA8:` in the `GL_CONSTANTS` block. If absent after an
upstream pull, re-add the 14 entries.

---

## #10 — WebGL1 EXT_sRGB unsized & half-float unsized internalformats → ES3 sized translate + HALF_FLOAT_OES → HALF_FLOAT type normalization — SHIPPED 2026-06-29 (SRGB + HalfFloat both VERIFIED)

**File(s):** [source/webgl.cc](source/webgl.cc) (`w_tex_image_2d`,
`w_tex_sub_image_2d`, plus two `bucket_e_translate_*` helpers
immediately above them).

**Symptom (SRGB half — VERIFIED on hardware-and-Citron).** Three.js v1
texture uploads with `texture.colorSpace = SRGBColorSpace` fail
silently as black textures. Diagnostic capture (the now-reverted
`[bucket-e:texImage2D]` markers) showed:
- Failing call: `internalformat=0x8C42 (SRGB_ALPHA_EXT) format=0x8C42
  type=0x1401 (UByte) 256x256 → err=0x0500 (INVALID_ENUM)`.
- Working call: `internalformat=0x1908 (RGBA) format=0x1908 type=0x1401
  → err=0x0000` (Three.js's 1×1 empty default textures).
The WebGL1 EXT_sRGB spec requires `internalformat == format` and uses
unsized enums `SRGB_EXT 0x8C40` / `SRGB_ALPHA_EXT 0x8C42`. ES3-core
texImage2D does not recognize these as internalformats (ES3 uses
sized `SRGB8 0x8C41` / `SRGB8_ALPHA8 0x8C43` with format `GL_RGB` /
`GL_RGBA`). Engine passthrough hands the WebGL1 enums straight to
glTexImage2D → INVALID_ENUM → texture stays empty → sampler returns 0.
This is the V8-fork manifestation of the QuickJS-era issue catalogued
in [[reference-brewser-v1-black-texture-demos]] (same enums, same
error, different bridge architecture).

**Symptom (HalfFloat/Float half — VERIFIED 2026-06-29 via F.1 gate on
webgl-loader-gltf's r162 PMREM intermediate uploads).** Three.js r162
PMREM allocates RGBA16F intermediate render targets for the cube_uv
2D atlas. Two distinct issues had to be fixed for it to work, surfaced
sequentially by the F.1 `[f1:texImage2D-hf]` glGetError diagnostic:

1. **Unsized → sized internalformat translation** (the original #10
   half-float shape). When Three.js passes an unsized half-float combo
   (e.g. `texImage2D(target, 0, RGBA, w, h, 0, RGBA, HALF_FLOAT, data)`
   with `internalformat == format == RGBA`), ES3 requires a sized
   internalformat. The translate widens RGBA→RGBA16F / RGB→RGB16F /
   RG→RG16F / R→R16F (and 32F equivalents for GL_FLOAT type).

2. **`HALF_FLOAT_OES (0x8D61)` → `HALF_FLOAT (0x140B)` type
   normalization for sized half-float internalformats.** Three.js r162
   in particular passes an ALREADY-sized internalformat
   (`RGBA16F (0x881A)`) paired with the WebGL1 OES extension type token
   `HALF_FLOAT_OES (0x8D61)`. ES3 accepts `HALF_FLOAT_OES` ONLY with
   unsized internalformats; pairing it with sized returns
   `INVALID_OPERATION (0x0502)`. The QuickJS-era engine comment that
   said "Mesa treats 0x8D61 and 0x140B as aliases" is true for the
   unsized ES2-style case but NOT for sized internalformats. Fix: when
   `type == HALF_FLOAT_OES` AND `internalformat ∈ {RGBA16F, RGB16F,
   RG16F, R16F}`, normalize `type → HALF_FLOAT`. For texSubImage2D
   the normalization is unconditional (sub-uploads land into storage
   that was sized at allocation time; HALF_FLOAT is always the
   ES3-canonical token to pair with sized storage).

The first half (unsized→sized internalformat translate) shipped with
Bucket E. The second half (type normalization) was added during F.1
after the diagnostic exposed the issue. Both are cross-referenced to
[[reference-pmrem-halffloat-accept-list-fix]] and
[[reference-dfglut-rg16f-accept-list-fix]] — the QuickJS-era engine
also normalized type/format/internalformat combos through its
accept-list + EGL-layer translation; the V8 engine has no accept-list,
so the analog is this combined translate.

**Verification (Citron, 2026-06-29):** webgl-loader-gltf demo renders
the HDR equirect background AND the helmet shows IBL reflections. The
F.1 `[f1:texImage2D-hf]` diagnostic confirmed:
- Pre-fix: `internalformat=0x881A format=0x1908 type=0x8D61 ... err=0x0502 (INVALID_OPERATION)` × 3 calls.
- Post-fix: `internalformat=0x881A format=0x1908 type=0x140B ... err=0x0000` × 3 calls. Type normalized; Mesa-Nouveau accepts.
The same Mesa version (20.1.0-rc3) runs on hardware, so the fix is
expected to apply identically there.

**The fix (shipped).** Two helpers in webgl.cc, both signatures take
all three of internalformat/format/type by pointer so the type
normalization (added in F.1) can mutate the type alongside the
format/internalformat translation:

```cpp
// internalformat==format guard: only touch the WebGL1 ES2 call shape
// for the SRGB and unsized→sized half-float/float cases.
static inline void bucket_e_translate_tex_image(
    GLint *internalformat, GLenum *format, GLenum *type) {
    if (*internalformat == *format) {
        // SRGB unsized → ES3 sized + unsized format pairing
        if (*internalformat == 0x8C42) { *internalformat = 0x8C43;
                                         *format = 0x1908; return; }
        if (*internalformat == 0x8C40) { *internalformat = 0x8C41;
                                         *format = 0x1907; return; }
        // HALF_FLOAT / HALF_FLOAT_OES unsized → ES3 sized (RGBA→RGBA16F, etc.)
        if (*type == 0x140B || *type == 0x8D61) { /* widen internalformat */ }
        // FLOAT unsized → ES3 sized 32F
        if (*type == 0x1406) { /* widen internalformat */ }
    }
    // F.1: HALF_FLOAT_OES → HALF_FLOAT normalization for sized half-float
    // internalformats. Fires whenever the type is the WebGL1 OES alias AND
    // the (possibly post-translate) internalformat is one of the sized
    // half-float formats — covers BOTH the (a) Three.js path that passes
    // already-sized internalformat with OES type (r162 PMREM) and (b) the
    // path that needed the unsized→sized widen above (its post-widen
    // internalformat now matches one of the sized cases).
    if (*type == 0x8D61) {
        switch (*internalformat) {
        case 0x881A /* RGBA16F */: case 0x881B /* RGB16F */:
        case 0x822F /* RG16F */:   case 0x822D /* R16F */:
            *type = 0x140B; /* HALF_FLOAT */
            break;
        }
    }
}

static inline void bucket_e_translate_tex_sub_image(
    GLenum *format, GLenum *type) {
    if (*format == 0x8C42) *format = 0x1908; // RGBA
    else if (*format == 0x8C40) *format = 0x1907; // RGB
    // F.1: unconditional OES→core HALF_FLOAT normalization. Sub-uploads
    // land into storage that was sized at allocation time; HALF_FLOAT is
    // always the ES3-canonical token to pair with sized storage. Safe to
    // normalize because Mesa accepts HALF_FLOAT with both sized and
    // unsized backing storage.
    if (*type == 0x8D61) *type = 0x140B;
}
```

Called from `w_tex_image_2d` and `w_tex_sub_image_2d` immediately
before the native GL call. The `internalformat == format` guard keeps
the translation tight — calls that already pass a sized internalformat
(the post-r150 Three.js path that needs #9's constants) flow through
unchanged.

**Why upstream-vanilla lacks it.** Upstream V8 nx.js is WebGL2-only;
the WebGL1 EXT_sRGB translation is specific to v1 contexts which
upstream doesn't expose.

**DISPOSITION:** `upstream-candidate`. Pairs with #9 — together they
make v1 + post-r150 Three.js render textures correctly without driver
quirks. Same Step-2 PR.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep
[source/webgl.cc](source/webgl.cc) for `bucket_e_translate_tex_image`.
If absent after an upstream pull, re-port both helpers and their call
sites (one line each in `w_tex_image_2d` / `w_tex_sub_image_2d`, right
before the native GL call). The `internalformat == format` guard MUST
stay — without it, the translate would catch combos that aren't EXT_sRGB
spec-conformant and break paths that currently work.

**Recurrence tells:**
- v1 Three.js demo with `texture.colorSpace = SRGBColorSpace` renders
  black → the SRGB half regressed. Re-add diagnostic glGetError logging
  around glTexImage2D (the now-reverted `[bucket-e:texImage2D]` pattern)
  to confirm `internalformat=0x8C42 ... err=0x0500` is back.
- Three.js PMREM-using demo (webgl-loader-gltf or any
  scene.environment-via-HDR-equirect) renders black background / matte
  helmet → the HalfFloat half of #10 regressed. Re-add the F.1
  `[f1:texImage2D-hf]` diagnostic; check whether the post-translate type
  is 0x140B (good) or 0x8D61 (normalization regressed) and whether err
  is 0x0000 (good) or 0x0502 (INVALID_OPERATION = sized+OES still being
  passed to GL).
- Direct samplerCube cubemap demo (`webgl-materials-cubemap`) renders
  with 2 of 3 heads black AFTER #10/#11 are in place → that's the
  separate Bucket F.2 issue (Mesa-Nouveau samplerCube driver limit +
  dropped cube→2D atlas routing fork patches), NOT a #10/#11 regression.

---

## #11 — PMREM r184 PMREMGGXConvolution FS replacement — SHIPPED 2026-06-29 (APPLIED-BUT-UNVERIFIED-PENDING-r184-PMREM-DEMO)

**File(s):** [source/webgl.cc](source/webgl.cc) (`maybe_replace_pmrem_fs`
helper above `w_shader_source`, called from `w_shader_source`).

**Symptom (Three.js r184 PMREM only).** Mesa-Nouveau on Tegra X1 (and
likely Citron, given the demo's own comment) aborts `glDrawArrays`
when the original Three.js r184 PMREMGGXConvolution fragment shader is
compiled, EVEN WHEN the GGX importance-sample loop body is statically
unreachable at runtime. The QuickJS-era fork hunted this through 13
iterations and conclusively isolated the FS BODY PRESENCE alone (not
runtime execution) as the trigger — see
[[reference-pmrem-tegra-compiler-workaround]]. Three.js r162's PMREM
doesn't contain the offending FS, which is why
`webgl-loader-gltf` (the only PMREM demo in the current set) is
specifically pinned to r162 via `__THREE_R162_STAGED__`.

**Why upstream-vanilla lacks it.** This is a Tegra/Mesa-Nouveau GLSL
compiler bug. Three.js doesn't know about it. The QuickJS-era fork
intercepted shaderSource and substituted a hand-written minimal FS
that omits all the constructs that crashed an iteration (no uint, no
bitwise, no for, no sin/cos, no early-return conditional).

**The fix (shipped).** Verbatim re-port from QuickJS-era
[webgl_egl.c::nx_webgl_egl_compile_shader](D:/Workspace/nxjs-source/source/webgl_egl.c#L8629-L8800)
into V8's [webgl.cc::maybe_replace_pmrem_fs](source/webgl.cc) +
called from `w_shader_source`. Gated by
`shader_type == GL_FRAGMENT_SHADER` AND
`strstr(src, "PMREMGGXConvolution") != NULL` — both conditions must
hold. Parses `#define CUBEUV_TEXEL_WIDTH/HEIGHT/MAX_MIP` from the
Three.js-emitted source (hardcoding would only match cubeSize=256
and break for cubeSize=512 with lodMax=9). Replacement FS is a 5-tap
unrolled cross blur using a tangent/bitangent basis around the output
normal; per-pass roughness scales the kernel radius; across PMREM's
cumulative passes this produces a widening hemisphere coverage in the
cube_uv mip chain. Visually inferior to upstream's GGX importance-
sample blur (sharper edges across all roughness levels) but the only
way to get r184 PMREM through Mesa-Nouveau without a driver abort.

**DISPOSITION:** `fork-only` (Tegra/Mesa-Nouveau-specific GLSL
compiler workaround).

**UPSTREAM STATUS:** `n/a` (driver-specific).

**Status: APPLIED-BUT-UNVERIFIED-PENDING-r184-PMREM-DEMO.** F.1 added
the FS replacement code AND a temporary `[f1:pmrem-fs]` diagnostic that
counts how many times it fires. On `webgl-loader-gltf` (r162 PMREM)
the diagnostic confirmed it fires 0 times (as expected — r162's PMREM
FS doesn't contain "PMREMGGXConvolution"). The replacement code is
proactively in place for when an r184 PMREM demo lands; active
verification will happen then. No current demo exercises r184 PMREM.

**RE-APPLY / VERIFY NOTE.** Grep
[source/webgl.cc](source/webgl.cc) for `maybe_replace_pmrem_fs`. If
absent after an upstream pull, re-port (~120 lines of C code +
embedded GLSL template; the exact source lives in QuickJS-era
`webgl_egl.c` lines ~8629-8800).

**Recurrence tells:**
- Adding an r184 PMREM demo causes the engine to crash during shader
  compile → check the gate (FRAGMENT_SHADER + "PMREMGGXConvolution"
  substring) is matching correctly.
- An r184 PMREM demo runs but the PMREM output is wrong (UVs outside
  texture / writes crash Mesa) → the constants parser
  (`#define CUBEUV_TEXEL_WIDTH/HEIGHT/MAX_MIP`) may have regressed;
  check it picks up Three.js's emitted values rather than falling back
  to the hardcoded 1536/2048/8 defaults.

---

## #12 — samplerCube → sampler2D routing layer (Bucket F.2a) — SHIPPED 2026-06-29 (RUNTIME-SIDE; VERIFIED on Citron)

**File(s):** [brewser-runtime-v8/src/scripts/cube-route-shim.ts](../brewser-runtime-v8/src/scripts/cube-route-shim.ts)
(NEW, ~310 lines) + 3-line wire-up in
[brewser-runtime-v8/src/scripts/canvas-runner.ts](../brewser-runtime-v8/src/scripts/canvas-runner.ts)
(import + two `installCubeRouting(gl)` call sites, in `getSharedScreenGL`
and `getSharedScreenGL2`, immediately after the existing
`installBridgeDirtyHooks(gl)`).

**Symptom.** Mesa-Nouveau on Tegra silently returns `vec4(0)` from any
direct `samplerCube` sampling (see
[[reference-mesa-nouveau-layered-sampling-unsupported]]). Demos that use
stock Three.js cube samplers — `MeshLambertMaterial { envMap: cubeTex }`,
`MeshBasicMaterial` reflective variants, `scene.background = cubeTex` —
render with the env-mapped heads black and the skybox absent. F.1
unblocked the PMREM path (Three.js's own 2D-atlas conversion for IBL);
F.2a unblocks USER cube textures by performing the same atlas conversion
runtime-side for any cube texture Three.js asks the GL to upload.

**Why runtime, not engine.** The QuickJS-era engine NEVER had a general
samplerCube routing layer — the cube_uv code in
[QuickJS webgl_egl.c:8629-8800](D:/Workspace/nxjs-source/source/webgl_egl.c#L8629)
is the PMREMGGXConvolution FS replacement (already ported as #11), which
assumes its input is already a `sampler2D` atlas built by Three.js's own
PMREM. Cube demos like `webgl-materials-cubemap` (Lambert + envMap +
`scene.background = cubeTex`) bypass PMREM entirely. The pre-V8 fork was
broken on these demos too — production cubemap demos that worked were
the ones that explicitly routed through PMREM (per
[[reference-mesa-nouveau-layered-sampling-unsupported]]: "the working
cube demos … ALL use sampler2D under the hood"). So F.2a is NEW work
regardless of layer; runtime placement was chosen because (a) zero
engine fork delta per the disposition policy, (b) the shared GL context
has one clean chokepoint (`getSharedScreenGL`), (c) the same
monkey-patch pattern is already in production via
`installBridgeDirtyHooks`, (d) Three.js's GLSL is templated and stable
enough for identifier-tracked substring rewriting.

**The fix (applied).** Three interceptions wrap the shared GL context:

1. **Texture upload reroute.** `gl.texImage2D(TEXTURE_CUBE_MAP_POSITIVE_X +
   i, ..., image)` is intercepted: per-cube state allocates a
   companion 2D atlas texture sized `(faceW * 6, faceH)` and uses
   `gl.texSubImage2D(TEXTURE_2D, 0, i * faceW, 0, ...)` to land each
   face into its slot. Per-cube state is keyed by the native cube
   `WebGLTexture` handle via `WeakMap`. `texSubImage2D` against cube
   face targets is similarly rerouted. `gl.texParameteri(TEXTURE_CUBE_MAP,
   ...)` is forwarded to the atlas texture (with `WRAP_S`/`WRAP_T` forced
   to `CLAMP_TO_EDGE` so the strip layout doesn't bleed inter-face).
   `gl.generateMipmap(TEXTURE_CUBE_MAP)` is silently skipped (atlas mips
   would bleed at face boundaries).

2. **Bind reroute.** `gl.bindTexture(TEXTURE_CUBE_MAP, tex)` rebinds the
   atlas as `gl.bindTexture(TEXTURE_2D, atlasTex)` at the current active
   TU, so the rewritten FS's `sampler2D` reads from the atlas. Active-TU
   tracking via wrapped `gl.activeTexture`.

3. **Shader rewrite.** `gl.shaderSource(handle, src)` rewrites GLSL:
   - `uniform [precision]? samplerCube IDENT` → `uniform [precision]? sampler2D IDENT` (identifier captured per shader).
   - For each captured identifier `IDENT`: `textureCube(IDENT, V)`,
     `textureCubeLod(IDENT, V, L)`, `textureCubeLodEXT`,
     `textureCubeGrad`, `textureCubeGradEXT`, and (for GLSL ES 300)
     `texture(IDENT, V)`, `textureLod(IDENT, V, L)`, `textureProj(IDENT,
     V)` are rewritten to `cubeUVSample(IDENT, V)`. Identifier-scoped
     so `sampler2D` calls keep their original `texture`/`textureLod`.
   - Injects a `cubeUVSample(sampler2D, vec3) → vec4` helper at the top
     of the shader (after preamble: `#version`, `#extension`, `#define`,
     `precision`, `//` comments, blank lines). Helper does standard cube-
     face selection (max-axis test), per-face UV projection, and reads
     `texture2D(atlas, vec2((face + uv.x) / 6.0, uv.y))`. Uses only
     `texture2D` (WebGL1-safe; GLSL ES 300 prelude aliases this).

**Layout choice: 6×1 horizontal strip.** Three.js's PMREM cube_uv format
encodes a packed mip pyramid in a 1536×2048 atlas — appropriate for IBL
convolution but heavy for an LDR background cube. The strip layout
avoids per-mip math and accepts 0.5 px of edge bleed at LINEAR filtering
(CLAMP_TO_EDGE prevents wrap-around). Sufficient for backgrounds /
reflection envmaps; revisit if cubeUV mipped sampling becomes load-
bearing for a future demo.

**Identifier scoping rationale.** GLSL ES 300's `texture(s, v)` is
overloaded by sampler type. A blanket `texture(.*, .*)` rewrite would
break every `sampler2D` call in the same shader. By capturing the
specific identifiers declared as `samplerCube` and only rewriting calls
referencing those identifiers, sampler2D paths stay untouched.

**Why upstream-vanilla lacks it.** Driver-/platform-specific workaround
for the Mesa-Nouveau layered-sampling limit on Tegra X1. Upstream
nx.js / V8 / Three.js have no reason to ship a cube→2D atlas reroute.

**DISPOSITION:** `fork-only` (Tegra/Mesa-Nouveau-specific workaround,
runtime-side instead of engine — same product-specific category as
#11). Lives in brewser-runtime-v8, not nxjs-source-v8. No engine delta.

**UPSTREAM STATUS:** `n/a` (driver-specific).

**STATUS: SHIPPED + VERIFIED on Citron 2026-06-29.** `webgl-materials-
cubemap` renders all 3 Walt heads (pure reflection, refraction through
the center head, yellow-tint mixed reflection on the right) under the
Royal Castle skybox with clean reflections. No visible artifacts → F.2b
rescues (SRGB cube downgrade, cube-face FBO aliasing) were SKIPPED per
the "skip-if-artifact-absent" rule. Build state: brewser.nro =
66,893,946 B; nxjs.nro = **56,458,217 B IDENTICAL to F.1 close** — zero
engine delta confirmed throughout F.2a. Hardware-pending: F.2a's
visible result is Citron-confirmed; consolidated hardware pass at
Bucket F close. Driver-specific defect (Mesa-Nouveau samplerCube
returns vec4(0)) is identical on Citron and hardware silicon per
[[reference-mesa-nouveau-layered-sampling-unsupported]], so a Citron
PASS for the route + atlas + shader rewrite path is strong evidence
the fix transfers.

**Implementation arc (7 independent issues had to be solved in
sequence — initially shipped at 5, the next 2 surfaced when the
F.2a + F.1 demos were exercised together).** Documenting the cascade
so a future regression can be attributed to the right layer:

1. **The route at all.** Without the cube-route-shim, `samplerCube`
   uniforms return `vec4(0)` per the Mesa-Nouveau layered-sampling
   limit. Three.js's PMREM bypasses this via 2D atlas (F.1); F.2a does
   the same for USER cube textures.

2. **The shaderSource override actually sticks.** Tracked via
   `gl.shaderSource !== origShaderSource` self-check at install time
   (since reverted, but the pattern is the lift). Confirmed via the
   `[f2a:install]` log line which IS unconditional in shipped code.

3. **Three.js sees the rewritten uniform as samplerCube, not sampler2D.**
   The shader rewrite replaces `samplerCube envMap` with `sampler2D
   envMap` so the cube sampling can route through a 2D atlas. BUT Three
   .js queries `gl.getActiveUniform()` once at program init and caches
   the resulting type → setter function mapping. If it sees
   SAMPLER_2D, it wires `setValueT1`/`setTexture2D` — which tries to
   upload the demo's `CubeTexture` (whose `.image` is a 6-element
   array, not a single image source) via the 2D path, silently fails,
   and the cube faces are never uploaded. The actual demo runs to
   completion (renders at 60fps, `OBJ loaded: yes`, no demo error)
   while the cube uniform stays bound to Three.js's internal
   `_emptyCubeTexture` (a 1×1 white placeholder), giving the
   pre-F.2a-identical "2-of-3 black heads + black skybox" symptom.

   **Fix:** intercept `gl.getActiveUniform()` and fake `SAMPLER_2D
   (0x8B5E)` back to `SAMPLER_CUBE (0x8B60)` for any uniform name that
   was originally declared as `samplerCube` (tracked per-program via
   the WeakMap chain `shader → identifiers → attachShader → program`).
   With the fake in place, Three.js wires `setValueT6`/`setTextureCube`
   / `uploadCubeTexture`, which uses `gl.texImage2D(CUBE_FACE_POSITIVE_X
   + i, ...)` for face uploads — which the cube-route-shim's existing
   intercepts correctly route to the 2D atlas.

4. **The engine accepts the cube-face image uploads.** nx.js's
   `w_tex_image_2d` and `w_tex_sub_image_2d` **only support the 9-arg
   (target, level, internalformat, w, h, border, format, type,
   pixels) variant with raw pixel bytes** — they do NOT accept the
   6-arg `texImage2D(target, level, internalformat, format, type,
   source)` variant with `HTMLImageElement` / `ImageBitmap` /
   `HTMLCanvasElement` source. The engine reads `info[3]..info[8]`
   regardless of `info.Length()` and treats undefined slots as
   numeric-0, producing a malformed glTexImage2D call and
   `GL_INVALID_ENUM (0x500)`. The cube-route-shim allocates the atlas
   with the 9-arg null-pixels form (works) but Three.js's per-face
   uploads pass `cubeImage[i]` as a 6-arg image-source call, which my
   first shim implementation forwarded as a 7-arg `texSubImage2D
   (target, level, x, y, format, type, source)` — also rejected by
   the engine. Diagnosed via per-call `gl.getError()` check (now
   reverted) showing `err=0x500` for every face upload.

   **Fix:** runtime-side conversion via `OffscreenCanvas` + 2D
   `getImageData`. The shim's `imageSourceToBytes(src, w, h)` helper
   creates a (w × h) OffscreenCanvas, draws the image source into it,
   reads back pixels via `ctx.getImageData(0, 0, w, h).data`
   (`Uint8ClampedArray` — qualifies as `ArrayBufferView`), then calls
   the engine's 9-arg `texSubImage2D` with the bytes. `OffscreenCanvas
   + 2D + drawImage(Image)` and `getImageData` are supported by nx.js
   v8 (the brewser shell uses them extensively, and engine line
   1247's `IsNullOrUndefined()` check guards the typed-pixels path).

   **Note (engine-fix candidate, NOT addressed here):** the lack of
   image-source `texImage2D` / `texSubImage2D` is a general engine
   gap — any demo doing `gl.texImage2D(TEXTURE_2D, 0, RGBA, RGBA,
   UByte, img)` directly (not just cube paths) would also silently
   fail. Currently masked because demos use `DataTexture` (typed-
   array path, works) or because the silent failure manifests as
   "black texture" which is the same symptom as many other bugs.
   Filing a separate engine-side image-source upload patch could be
   a future round; for F.2a's scope, the runtime-side per-cube
   conversion suffices.

5. **`renderer.resetState()` per frame + Three.js state cache.** Per
   the demo's idiom, `renderer.resetState()` resets Three.js's
   `WebGLState` cache each frame so the bind tracking starts fresh.
   This means the cube-route-shim's `bindTexture(CUBE_MAP, cubeTex)
   → bindTexture(TEXTURE_2D, atlas)` re-routing fires per-frame
   (not just on first use), which is what keeps the atlas bound to
   the correct active TU through the per-material uniform upload
   path. If a future demo skips `resetState()`, the atlas could stay
   bound across frames OR could be unbound by other 2D texture
   binds — the routing still works because the next setTextureCube
   re-binds, but per-frame `resetState()` is the cheapest way to
   keep the state path simple.

6. **Safer bindTexture(CUBE_MAP) — preserve CUBE_MAP state +
   conditional atlas bind.** The initial shim version did
   `origBindTexture(TEXTURE_2D, atlas-OR-null)` UNCONDITIONALLY for
   every CUBE_MAP bind, including binds of textures that didn't have
   an atlas (Three.js's `_emptyCubeTexture` placeholder, PMREM
   intermediates, anything pre-first-image-upload). This stomped
   legitimate 2D texture bindings at the same TU — most visibly
   `webgl-loader-gltf`'s `MeshStandardMaterial` cubeUV envMap atlas
   (a sampler2D texture from PMREM) got overwritten with the empty
   cube's 1×6-white placeholder atlas every frame, regressing F.1's
   helmet IBL + skybox to matte/black.
   **Fix:** the shim now ALWAYS forwards CUBE_MAP binds to the
   actual `TEXTURE_CUBE_MAP` target (so the GL CUBE_MAP state stays
   correct for any pure-samplerCube shader that wasn't caught by
   the rewrite), and ONLY touches `TEXTURE_2D[activeTU]` when the
   bound cube tex has an allocated atlas (`state.atlasAllocated`).
   ALSO gates the atlas-allocation path itself to source-is-image
   uploads (typed-array placeholders like Three.js's empty cube go
   through the engine's native 9-arg path unchanged — no atlas
   created for them, no bind reroute).

7. **`samplerCube` ↔ `sampler2D envMap` dual-declaration gating
   (envMap-specifically).** Three.js's shader chunks
   `envmap_common_pars_fragment` + the BackgroundShader declare BOTH
   `uniform samplerCube envMap;` (under `#ifdef ENVMAP_TYPE_CUBE`)
   AND `uniform sampler2D envMap;` (under `#else` / `#elif
   ENVMAP_TYPE_CUBE_UV`) for the SAME uniform name. The active
   branch is selected at compile time by the material's `#define
   ENVMAP_TYPE_CUBE` / `#define ENVMAP_TYPE_CUBE_UV`. The naive
   "rewrite every `samplerCube IDENT` to `sampler2D IDENT` and fake
   `getActiveUniform()` to SAMPLER_CUBE for IDENT" approach hijacks
   Three.js's setTexture2D path for the LIVE cubeUV sampler2D
   envMap — Three.js then calls `setTextureCube` with a 2D atlas
   value, `uploadCubeTexture` bails (`texture.image.length !== 6`),
   the texture is never properly bound, helmet renders matte.
   **Fix:** before rewriting, scan the source for `sampler2D
   IDENT` counterparts. Classify each `samplerCube IDENT` as
   - SOLO (no sampler2D IDENT in source): always rewrite + fake.
     (BackgroundCubeMaterial's `tCube`; cube-only shaders.)
   - DUAL (sampler2D IDENT also in source): only rewrite + fake
     when the source contains `#define ENVMAP_TYPE_CUBE\b(?!_)`
     (cube branch is the live one). When `#define ENVMAP_TYPE_CUBE_UV`
     is present, leave the dual ident alone — Three.js takes its
     native sampler2D-cubeUV upload path. Currently only `envMap`
     is dual-declared in stock Three.js shader chunks; future-proof
     handling for other dual idents conservatively skips (better
     to leave a cube sampler reading vec4(0) than hijack a live
     sampler2D one). The full webgl-loader-gltf path was restored
     by this gate without giving up the webgl-materials-cubemap
     route — both demos now render correctly side-by-side.

**RE-APPLY / VERIFY NOTE.** Grep
[brewser-runtime-v8/src/scripts/cube-route-shim.ts](../brewser-runtime-v8/src/scripts/cube-route-shim.ts)
for `cubeUVSample`. If absent after a brewser-runtime pull / rebase,
re-port the file + the two `installCubeRouting(gl)` call sites in
canvas-runner.ts. Verify the install fires by checking `gl[Symbol.for('brewserCubeRouteInstalled')] === true` after `getSharedScreenGL`.

**Recurrence tells:**
- `webgl-materials-cubemap` regresses to 2-of-3 black heads + no skybox
  → cube-route-shim regressed; check `import { installCubeRouting }`
  still present in canvas-runner.ts and the two call sites are intact.
  The `[f2a:install]` log line (unconditional, one-shot per session) is
  the first thing to check in nxjs-debug.log — its absence means the
  shim isn't even being installed.
- 2-of-3 black heads + skybox black BUT `[f2a:install]` IS present →
  one of the deeper layers regressed. Set `globalThis.__f2aDiag = true`
  before the demo runs (e.g., via DevTools or a top-level `<script>`)
  to get the per-call markers. Then check in order:
  (a) `[f2a:shader-rewrite]` with `cubeUniforms=envMap` — confirms the
  GLSL rewrite fires + captures the cube identifier. Absent = the
  shaderSource override didn't stick, or the source's `samplerCube`
  pattern doesn't match the regex.
  (b) `[f2a:uniform-fake-cube]` with `origType=0x8b5e` `fakedTo=0x8b60`
  — confirms the getActiveUniform fake fires on the rewritten uniform.
  Absent = the per-shader-to-per-program identifier propagation broke
  (attachShader hook regressed, or the program-keyed WeakMap miss).
  (c) `[f2a:atlas-alloc]` with `faceW=512 faceH=512` (or whatever the
  cube source size is) AFTER the demo's images load — confirms the
  cube upload reaches the texImage2D hook with the real image size.
  Absent (or only the `faceW=1 faceH=1 srcCtor=Uint8Array` empty-cube
  placeholder alloc) = Three.js is wired to setTexture2D (uniform-fake
  regressed) or `cube tex.needsUpdate` isn't firing in the demo.
- Any non-cube Three.js demo regresses to black textures or scrambled
  geometry → an interception is mis-forwarding. Suspect the
  `texImage2D` / `bindTexture` rewriters; specifically, ensure
  non-cube targets are passed through to original methods unchanged
  (the `isCubeFaceTarget(target)` early-bail is the gate).
- A non-cube `sampler2D` uniform in a shader gets its calls rewritten
  (e.g., `texture(diffuseMap, vUv)` becomes `cubeUVSample(...)`) →
  identifier scoping regressed; the `cubeIdents` capture from
  `uniform samplerCube IDENT` declarations is failing or the per-
  identifier regex is matching the wrong identifier.
- Atlas size errors: shader compile fails on `cubeUVSample` (e.g.,
  redefinition or missing precision) → the helper-injection insertion
  point landed inside the shader body instead of the preamble. Check
  the `for (let i = 0; i < lines.length; i++)` loop in
  `rewriteCubeShader` — should break at the first non-preamble line.
- Visible inter-face seams on the cube background → strip layout's
  LINEAR-filter edge bleed at face boundaries. Mitigate by adding
  per-face padding (1-px texel border replicated from adjacent face
  edge pixels) in the atlas allocation; F.2b refinement (currently
  NOT shipped because no visible seams were observed on the
  webgl-materials-cubemap gate at 512-px-per-face).
- `webgl-materials-cubemap` works but some OTHER demo with an image-
  source `texImage2D` regresses → engine still lacks the 6-arg image-
  source variant. The cube-route-shim's `imageSourceToBytes` only
  converts CUBE-target uploads (because that's where the shim
  intercepts). 2D image-source uploads on demo code paths would
  separately need either an engine-side fix or a broader 2D-path
  shim — out of scope for F.2a.

**Cross-references:**
- [[reference-mesa-nouveau-layered-sampling-unsupported]] — the
  hardware reality F.2a works around.
- [[reference-pmrem-tegra-compiler-workaround]] — Bug 3 (SRGB cube
  downgrade) was companion-tracked here pre-V8; if F.2b lands the SRGB
  cube downgrade rescue, it joins this entry as a sub-bullet.
- [[reference-mesa-cube-face-aliasing-rescue]] — engine-side cube-face
  FBO aliasing rescue. F.2a routes USER cube textures around the
  driver via 2D atlas; rescue is only needed if cube textures are
  written via FBO (e.g., CubeCamera dynamic cubemap). F.2b may re-port
  this if `webgl-materials-cubemap-dynamic` becomes a verification
  target.

---

## Expected growth during Step 2

Step 2 (WebGL semantics to TS) is expected to surface more fork-patches
of this shape: brewser-runtime calls an engine surface the QuickJS fork
provided but upstream V8 doesn't ship. The recognizable pattern:

- A runtime-side call throws `TypeError: ... is not a function`, OR
- A runtime-side fetch / load returns 403 / not-supported / blank for a
  URL shape the embedder explicitly handles via a registered loader, OR
- A native binding referenced by `$.someName` is undefined.

Each new occurrence gets its own numbered entry here with the same
fields. Resist the temptation to fix in the engine: ask first whether
the runtime caller can change, file an upstream PR if it can't, and
only add a fork-only engine edit as last resort.
