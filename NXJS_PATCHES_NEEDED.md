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

## #13 — canvas.cc: pin set_font_size at start of fillText/strokeText/measureText — SHIPPED 2026-06-30

**File(s):** [source/canvas.cc](source/canvas.cc)

**Exact change.** Add `set_font_size(context, context->state->font_size);`
as the first statement (after argument parsing + early-return) in:

- `nx_canvas_context_2d_fill_text` (~line 1698, right after
  `double scale = 1., font_size = context->state->font_size;`)
- `nx_canvas_context_2d_stroke_text` (~line 1758, same position)
- `nx_canvas_context_2d_measure_text` (~line 1801, right before
  the `if (context->state->hb_font)` shaping block)

```cpp
// Pin the shared ft_face / hb_font scale to this ctx's font_size
// before shaping — re-apply per call, idempotent against cross-ctx
// state corruption via save/restore on a SHARED nx_font_face_t.
set_font_size(context, context->state->font_size);
```

The matching `set_font_size` definition (~line 333) is unchanged — it
runs `FT_Set_Char_Size(ft_face, 0, font_size * 64, 0, 0)` and
`hb_font_set_scale(hb_font, font_size * 64, font_size * 64)`.

**Symptom it fixes.** Inline 2D canvas text drawn via `ctx.fillText`
renders at the WRONG size (e.g., 10 px instead of 14 px) on any page
where the shell painter draws HTML text concurrently to the same screen
canvas. Most visible on `webgl-materials-cubemap`'s `#cube-status`
panel: "1 px smaller, tighter letter spacing, fuzzy" text on every
glyph; the FT_Face char_size is at 10 (the canvas-state default) while
`state->font_size` is 14. Affects any demo with a `<canvas>` driven by
a JS-side render loop that draws text via `'system-ui'` font, whenever
the brewser shell concurrently paints HTML text using
`ctx.save() / ctx.font / fillText / ctx.restore()`.

**Why upstream-vanilla lacks it.** nx.js uses a 1:1 mapping from JS
`FontFace` to the C-side `nx_font_face_t` (`packages/runtime/src/font/
font-face.ts:40` builds the C struct once per JS FontFace construction;
[source/font.cc:33-110](source/font.cc#L33-L110) allocates the
FreeType `FT_Face` and HarfBuzz `hb_font` inside it). All 2D contexts
that resolve to the SAME `FontFace` via
[`findFont`](packages/runtime/src/font/font-face-set.ts#L96-L116)
end up with `state->ft_face` and `state->hb_font` pointing at the
SAME shared FreeType / HarfBuzz objects.

The state stack save/restore preserves `state->font_size` (a value)
but the `FT_Face`'s `char_size` is **device-global**: any
`set_font_size` call (via `set_font` setter OR via the implicit
`set_font_size(state->font_size)` at the end of
`nx_canvas_context_2d_restore`, [canvas.cc:1822](source/canvas.cc#L1822))
mutates the shared FT/HB state in place — visible to every other ctx
holding the same `nx_font_face_t`.

The brewser shell painter sequence is:
1. `ctx.save()` — push state copy
2. `ctx.font = '${size}px system-ui'` — `set_font` updates state.font_face
   (possibly to a different `FontFace` if `findFont` returns a different
   match) + `set_font_size(size)` on the inner state's ft_face
3. `fillText(...)` — draws at `size`
4. `ctx.restore()` — pops; **calls `set_font_size(outer.font_size)`**
   where `outer.font_face` is the FIRST-registered `'system-ui'`
   FontFace and `outer.font_size = 10` (the canvas-state default,
   from `init_state_defaults`). Mutates the shared FT_Face's
   char_size to 10.

Cube-status's `renderStatus` runs every 200 ms via `setInterval`. Its
`ctx.font = '14px system-ui'` is no-op'd by the JS-side setter's
early-return (`if (this.font === v) return;` at canvas-rendering-
context-2d.ts:91) on every call after the first — because the ctx's
`state.font_string` is still `'14px system-ui'`. So `set_font_size(14)`
does NOT get called again, and the next `fillText` reads the shared
FT_Face at the size the shell left behind: **10**.

**DISPOSITION:** `upstream-candidate`. General engine correctness —
any nx.js embedder with concurrent 2D contexts sharing a system-ui
FontFace hits this. A wider, structural fix would give each
`nx_canvas_context_2d_t` its own `FT_Face` + `hb_font` clones rather
than sharing the FontFace's instances, but that's a larger refactor
with allocation-rate + memory implications; the per-text-op
`set_font_size` re-pin is the minimal, surgical fix.

**UPSTREAM STATUS:** `not-submitted` (2026-06-30). Worth a PR after a
minimal repro is reduced — likely "two `OffscreenCanvas` instances,
both set `ctx.font = '14px system-ui'`, one calls save/font('20px')/
fillText/restore, the other does fillText and gets text at 10 px."
Repro is trivial to extract from the user-reported case.

**Cost.** One `FT_Set_Char_Size` + one `hb_font_set_scale` per
`fillText` / `strokeText` / `measureText` call — sub-microsecond on
the cached FT_Face (no font-data parse). Negligible vs the
`SkCanvas::drawGlyphs` call that follows.

**Diagnostic toolkit that found the bug** (removed after ship; re-add
during future regression hunts):

1. **In `fill_text` (engine):** every-Nth-call log of `state.font_size`,
   `ft->size->metrics.x_ppem`, `hb_font_get_scale()` values, and the
   `font_face` pointer. Surfaces ANY mismatch between `state.font_size`
   and the actual FT/HB scale (`v8 fft` in our session's logs).
2. **In `set_font_size` (engine):** every-call log of `font_face`
   pointer + new size. With (1), pairs to identify WHO is calling
   `set_font_size` on which `nx_font_face_t` (`v9 set_font_size`).
3. **In `overlayLiveAnimatedCanvases` (brewser-runtime, scoped to
   `el.id === 'cube-status'`):** log `box.x/y/w/h`, `screenX/Y`, and a
   `getImageData(x, y, 1, 1)` sample at a known text position
   (`fuzz-diag cube-status tick`).
4. **Horizontal pixel strip (same scope):** read 30 contiguous pixels
   across known glyphs, classify each by red channel into
   `.` (bg) / `:` (AA edge) / `#` (text body), join into a string. Lets
   you eyeball actual glyph widths in the offscreen — caught the
   ~70 % scale that confirmed the offscreen content (not the
   compositing) was wrong (`fuzz-diag cube-status strip`).

**Recurrence tells:**

- Any inline 2D canvas's `fillText` text appears smaller than its
  `state.font_size` would imply, ON PAGES WHERE THE SHELL CONCURRENTLY
  PAINTS HTML TEXT WITH `'system-ui'`.
- Engine-level fillText probe (#1 above) shows `state.size=N` but
  `ft.x_ppem` < N for the affected font_face.
- `set_font_size` trace (#2 above) shows alternating `ff=` pointers
  with one of them repeatedly getting set to 10 right after every
  shell `ctx.restore()` call — the classic cross-context corruption
  signature.

**Cross-references:**

- [[project-nxjs-canvas-shared-ft-face-fix]] — full investigation log
  (false starts: Math.round on dst coords, putImageData isolation,
  setSubpixel(false), single-source pixel path, double-draw
  elimination — ALL refuted before the trace caught the actual cause).
- [[project-v8-cursor-compositor-shipped]] — the cursor work surfaced
  this bug because its restored canCanvasFastPath + paintCursorOverlay
  hook added the `ctx.save() / set font / fillText / ctx.restore()`
  pattern to the per-frame shell paint chain. The bug itself
  PRE-EXISTED the cursor work (anywhere a shell does save/restore
  around font changes triggers it); cursor work just made it
  reproduce visibly on a hot path.

---

## #14 — WebGL2 context factory + screen.getContext('webgl2') wiring (Phase 2.G.0) — SHIPPED 2026-06-30

**File(s):**
[source/webgl.cc](source/webgl.cc) (nx_webgl2_context_new, nx_webgl2_init_class, make_context_carrier helper, NX_SET_FUNC registrations in nx_init_webgl);
[packages/runtime/src/canvas/webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts) (createWebGL2Context now calls `$.webgl2ContextNew`; the class install now calls `$.webgl2InitClass`);
[packages/runtime/src/screen.ts](packages/runtime/src/screen.ts) (`getContext('webgl2')` branch flipped from `return null` to mint a v2 context via createWebGL2Context, mirroring the v1 branch's 2D-coexistence rule).

**Exact change (Phase 2.G.0).** Add two new engine bindings beside the
existing v1 pair (`$.webglContextNew` + `$.webglInitClass`):
- `$.webgl2ContextNew(canvas)` → wraps an internal `make_context_carrier`
  helper with `is_v2=true`. Shares engine `WebGLState` with v1 (one
  process, one bridge, one tenant FBO); the v2 wrapper diverges from v1
  only by an additional own-property `__webgl2 = true` for future engine-
  side dispatchers in 2.G.1+ that need to branch on context kind
  (getParameter pname tables, extension allowlists, etc.).
- `$.webgl2InitClass(WebGL2RenderingContext, { handle map })` → calls a
  SEPARATE `install_methods_v2` (currently an EMPTY FUNCS[] table) on the
  v2 prototype, and additively populates the shared K_* handle-prototype
  carriers without overwriting v1's entries.

Runtime side: `createWebGL2Context` now calls `$.webgl2ContextNew` (not
`$.webglContextNew`); the install at the bottom of
`webgl2-rendering-context.ts` now calls `$.webgl2InitClass` (not
`$.webglInitClass`). `screen.getContext('webgl2')` mirrors the `'webgl'`
branch's behavior — mints a v2 context on first call, caches it in
`ScreenInternal.contextWebGL2`, returns the cached value on subsequent
calls, allows 2D-coexistence on the same canvas, excludes only the other
WebGL family.

**Why this change.** Upstream V8 nx.js v1.0.0-beta.5's webgl.cc exposes
neither v1 nor v2 from `screen.getContext` — Phase 2.C grew the v1 path
by adding `$.webglContextNew` + `$.webglInitClass` and flipping
`screen.getContext('webgl')`. The v2 path was left null at 2.C (see #7's
deliberate-null rationale) because returning a v2 context with only v1
methods routes Three.js into its v2 codepath via `gl.constructor.name`
detection and immediately throws on the first v2-only call. Phase 2.G.0
ships the structural piece: a non-null v2 context with the CORRECT
prototype shape (387 v2 constants + empty method table) and the SEPARATE
factory + init symbols, so 2.G.1 can grow methods into the v2 path
without re-litigating the structural question and without touching v1.

**Symptom it fixes.** Before 2.G.0: `screen.getContext('webgl2')` returns
`null`; Three.js's WebGL2-detection (`new WebGLRenderer({ canvas })`
internal probe) fails, demos either fail entirely or downgrade to v1. After
2.G.0: `screen.getContext('webgl2') instanceof WebGL2RenderingContext`
returns true; the v2 detection succeeds. Calling any v2 method (or any
v1 method on the v2 context, since v2's FUNCS[] is empty in 2.G.0) throws
`TypeError: X is not a function` — EXPECTED until 2.G.1.

**Why upstream-vanilla lacks it.** Upstream beta.5 ships a single 161-
method WebGL2 table backed by the legacy "WebGL or Skia, not both" EGL-
ownership model (see MIGRATION_PLAN.md "Architectural delta from
upstream"). Our shared-context model + 2.B state save/restore + 2.C
v1-with-empty-extension-table approach is structurally different, so the
v2 introduction has to follow the same separate-binding-symbol pattern
the v1 path uses.

**DISPOSITION:** `upstream-candidate`. Any embedder running the V8 nx.js
inside a Skia-bridged shared-context architecture benefits from the
separate v1/v2 factory + init binding split. File a PR after 2.G is
hardware-verified end-to-end.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** After upstream pull, `grep` engine
`source/webgl.cc` for `webgl2ContextNew` and `webgl2InitClass`. If absent,
re-apply the 4-symbol diff (helper `make_context_carrier`, two new public
functions, two new `NX_SET_FUNC` registrations in `nx_init_webgl`).
Re-apply the TS-side `createWebGL2Context` swap to `$.webgl2ContextNew` /
`$.webgl2InitClass`. Re-apply the screen.ts:152-162 flip.

**GOTCHA: v1 install path must stay byte-identical.** The v1 install
path was hardware-verified clean (Phase 2.C hardware gate, full JIT after
#8 fix). The 2.G.0 work is ADDITIVE only — `install_methods()` and
`nx_webgl_init_class` are untouched. A regression that "fixes" v2 by
sharing code with v1 (a single install_methods that selects by class) is
the wrong shape; see the table-split rationale in #15.

---

## #15 — WebGL v1/v2 method-table split: separate FUNCS[] tables (Phase 2.G.0 shape decision)

**File(s):**
[source/webgl.cc](source/webgl.cc) (`install_methods()` for v1 + new
`install_methods_v2()` — separate static-storage Spec FUNCS[] tables;
multi-paragraph rationale block above `install_methods()`).

**Exact change (Phase 2.G.0).** The v2 method install path uses its OWN
`install_methods_v2(iso, proto)` function with its OWN `static const Spec
FUNCS[]` table. NOT a single `install_methods(iso, proto, bool is_v2)`
that selects a subset by flag, NOT a single FUNCS[] with per-entry
`V1_BIT/V2_BIT` flags.

**Why this shape.**

1. **JIT safety against #8 lesson.** #8's root cause was a TS-side
   `Object.defineProperty` pattern that compiled into a V8/aarch64 Tegra
   JIT codegen issue (1160 per-key calls in `for (const [k,v] of
   Object.entries(GL_CONSTANTS))` loops); the fix was bulk
   `Object.defineProperties` per target. That fix discipline is "do not
   gate behavior at install time; keep the install loop shape
   predictable." Separate tables EXTEND that discipline to the engine-
   side method install: a single FUNCS[] with select-subset-at-install
   logic would feed conditional FunctionTemplate::New calls into V8's
   prototype hidden class, growing the surface area for a new JIT
   codegen edge case. Separate tables present V8 with two independent,
   straightforward install paths.

2. **v1 install path is hardware-verified clean.** The 95-entry v1
   FUNCS[] + install loop compiled-and-JIT'd clean on real Tegra
   (Phase 2.C hardware gate, jit=on default post-#8). Touching that loop
   to add selection logic is a regression risk that 2.G.0 cannot afford
   (the table-split shape MUST land before 2.G.1's webgl2-ubo slice can
   sign off). Independent tables = v1 code path unchanged = v1 hardware
   verification carries through.

3. **9 v2-only extensions need v2-only registration.** EXT_disjoint_
   timer_query_webgl2, EXT_texture_norm16, WEBGL_clip_cull_distance,
   EXT_float_blend, EXT_render_snorm, OES_sample_variables, OES_draw_
   buffers_indexed, WEBGL_blend_func_extended, WEBGL_compressed_texture_
   etc — all gated by `is_webgl2=true` in the QuickJS-era fork. A shared
   table would have to gate them at install time; separate tables make
   them v2-only by location.

4. **Three.js detection contract.** Three.js detects v1 vs v2 via
   `gl.constructor.name === 'WebGL{2}RenderingContext'`. Independent
   prototype chains with independent method sets match this contract.
   No shared dispatch surface.

5. **Bisectability.** If a future hardware regression appears on v2 but
   not v1, it's attributable to the v2 install path (Spec entries OR
   install-loop interaction with the table size). With separate tables
   the regression is bisectable by toggling individual v2 entries off;
   with a shared table the bisection has to disambiguate "is it the
   entry or the gate".

**Cost.** When 2.G.1+ ports the ~95 v2-only methods AND the ~95
v1-shared methods (WebGL2 IS-A WebGL1), the v2 FUNCS[] grows to ~190
entries. The shared method entries point at the SAME C++ impl
functions (the `w_useProgram` etc. impls do not change); only the Spec
entry duplicates. Code-size impact is ~95 extra Spec entries in the
.cc file. Negligible.

**Symptom it fixes.** This is a structural choice, not a bug fix. The
alternative shape (shared FUNCS[] with selection) is what we are NOT
doing. Recurrence tell: a future contributor sees the duplicate Spec
entries between FUNCS[] (v1) and FUNCS_V2[] (v2) at 2.G.1+ and refactors
to dedup → reintroduces the gate, regressing the JIT-safety discipline
silently. If a future regression flips `jit = off` to be a workaround
again, FIRST check whether the table-split shape got refactored out.

**Why upstream-vanilla lacks it.** Upstream beta.5 has a single ~161-
method table that registers WebGL2 only; no v1 path exists in upstream
at all, so the question of split-shape never arose there.

**DISPOSITION:** `upstream-candidate`. Any embedder splitting v1/v2 into
separate exposed surfaces benefits. Bundle into the same PR as #14.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** After upstream pull, look for
`install_methods_v2` next to `install_methods` in webgl.cc. If only one
exists, re-apply the split. **GOTCHA: do not refactor to dedup.** The
duplication between v1 FUNCS[] and v2 FUNCS[] is intentional per the
JIT-safety discipline above. The rationale block above
`install_methods()` documents this in detail; do not condense it on
"cleanup" passes.

---

## #16 — WebGL state-contract probe (Phase 2.G.0 — read-only, gated by `[webgl] state_probe = true`) — SHIPPED 2026-06-30

**File(s):**
[source/config.h](source/config.h) (`webgl_state_probe` bool field);
[source/config.cc](source/config.cc) (`[webgl] state_probe = true|false`
parse + default-false init);
[source/webgl_bridge.h](source/webgl_bridge.h) (probe enable/log
declarations + multi-paragraph header comment on the four candidate
bindings);
[source/webgl_bridge.cc](source/webgl_bridge.cc) (`s_state_probe_on`
gate, `s_probe_compose_n` frame counter, `probe_sample_this_frame`,
`nx_webgl_state_probe_enable`, `nx_webgl_state_probe_log` with the four
candidate reads + GL_ACTIVE_TEXTURE defensive restore; hook calls at
`nx_webgl_bridge_init` end, `nx_webgl_bridge_compose` enter/exit on
frames 1/60/600, `nx_webgl_bridge_exit` start);
[source/main.cc](source/main.cc) (`nx_webgl_state_probe_enable(ctx->
config.webgl_state_probe)` before bridge init).

**Exact change (Phase 2.G.0).** Before extending the FROZEN 2.B
`nx_gl_state_snap_t` even once, ship a READ-ONLY hardware probe that
queries four candidate GL bindings at known hook points and logs them.
The user reads the log on real CFW hardware; bindings whose values
remain constant across all tags do NOT need to be added to the snap;
bindings that mutate between `init` / `compose-pre` / `compose-post`
tags are leakage candidates and become inputs to the proposed batched
snap extension (#17 — proposed, awaiting probe results + sign-off).

Four candidates probed:
1. **UBO indexed bindings** — `GL_UNIFORM_BUFFER_BINDING` (base) +
   indexed slots 0..3 via `glGetIntegeri_v`.
2. **Sampler-unit-0 binding** — `GL_SAMPLER_BINDING` with active unit 0
   (the only unit Ganesh-GL is documented to touch).
3. **Read framebuffer separate from draw** — `GL_READ_FRAMEBUFFER_BINDING`
   alongside the already-saved `GL_DRAW_FRAMEBUFFER_BINDING`.
4. **Transform feedback + rasterizer-discard** — `GL_TRANSFORM_FEEDBACK_
   BINDING` + `glIsEnabled(GL_RASTERIZER_DISCARD)`.

Output format (one line per hook fire):
```
[webgl-bridge:probe] tag=<init|compose-pre|compose-post|exit>
  frame=<n> ubo_base=<id> ubo0=<id> ubo1=<id> ubo2=<id> ubo3=<id>
  sampler0=<id> read_fbo=<id> draw_fbo=<id> tf=<id> rast_disc=<0|1>
```

**Probe call sites (all auto-fire when probe enabled):**
- `init` — end of `nx_webgl_bridge_init` (baseline before any WebGL or
  composite traffic)
- `compose-pre` + `compose-post` (or `compose-post-{noflush,skip,clean}`)
  — start/end of `nx_webgl_bridge_compose` on frames 1, 60, 600 (1s and
  10s of compose-path activity, plus the very first frame for an
  immediate signal)
- `exit` — start of `nx_webgl_bridge_exit`

The probe is read-only: only `glGetIntegerv`, `glGetIntegeri_v`,
`glIsEnabled`, and a defensive `glActiveTexture` save/restore for the
sampler-unit-0 query. It does NOT mutate any state Skia or WebGL cares
about. Safe to enable on any production run; only the log volume grows.

**Why this change.** The QuickJS-era fork carried per-VAO attribute
state save/restore (webgl.c:14820-14872) and additional EGL-side
bookkeeping that implicitly serviced these four bindings; the V8
2.B `nx_gl_state_snap_t` deliberately stopped at the 18 fields the
Phase 0 fbo-spike empirically proved load-bearing on Citron + hardware.
Phase 2.G's webgl2-ubo slice (2.G.1) introduces UBO indexed binding
traffic; webgl2-multiple-rendertargets (2.G.2) introduces read/draw FB
split; gpgpu-water (2.G.7) stresses every binding in the list. Before
extending the snap (irreversible — every snap extension is hardware-
revalidated), get empirical evidence for which bindings actually leak
under our specific shared-context EGL architecture. The shared context
may make some of the QuickJS-era assumptions moot.

**Symptom it fixes.** Pre-probe: snap extension is a guess (extend
based on the QuickJS-era impl's assumptions about a different EGL
architecture). Post-probe: snap extension is bounded to bindings that
empirically leak on the V8 substrate. Saves rework + hardware passes.

**Why upstream-vanilla lacks it.** No state-contract probe; upstream
has no bridge architecture.

**DISPOSITION:** `fork-only`. Diagnostic-only feature specific to our
shared-context WebGL↔Skia coexistence model. Can be removed once Phase
2.G hardware-revalidates the extended snap; left in source as a
diagnostic re-enable for future regressions.

**UPSTREAM STATUS:** `n/a` (fork-only diagnostic).

**RE-APPLY / VERIFY NOTE.** After upstream pull, grep for
`nx_webgl_state_probe_log` in source/webgl_bridge.cc. If absent,
re-apply the probe (config flag + 5 hook points). If absent and the
batched snap extension (#17) has already shipped, the probe can stay
removed unless a new state-contract question arises.

**USAGE.** Drop into `sdmc:/switch/nxjs-override.ini`:
```ini
[webgl]
state_probe = true
```
Boot brewser → trigger a WebGL2 path. Read the log; a binding whose
value differs between adjacent tags is a passive-leak candidate.

**LIMITATION of the read-only probe.** The passive form catches leaks
that show up as VALUES Skia leaves bound — but a binding that Skia
restores to its OWN expected state (which may be a default-zero) after
its frame would read identically at `compose-pre` and `compose-post`
even if Ganesh touched it mid-frame. To detect that class of leak the
ACTIVE probe (see below) is required: SET the binding to a known
non-default value BEFORE crossing Skia, then read back AFTER, and
diff. Active probe spec lives in the design block at the bottom of
this entry; implementation deferred to next-session-after-#16-passive-
data.

---

### #16-ACTIVE — Active state-leak probe SPEC (Phase 2.G.0 — DESIGN, NOT IMPLEMENTED)

**STATUS: DESIGN ONLY, NOT SHIPPED.** This is the spec for a
follow-on probe pass after the passive #16 data is reviewed. Active
probe code is not yet written; this block IS the spec for writing it.

**Why active in addition to passive.** Shared-context EGL was the root
cause across multiple prior bugs (cube-face aliasing, save/restore
discipline). Skia's Ganesh-GL backend has its own state cache that
assumes certain bindings stay where Ganesh put them between draws.
There are FOUR distinct mutation patterns to detect:

| Pattern | Passive-probe behavior | Active-probe behavior |
|---|---|---|
| Skia LEAVES binding mutated | catches | catches |
| Skia mutates + RESTORES (to its own expected state) | misses | catches if our SET differs from Skia's expected |
| Binding is read-only / never touched | catches no leak | catches no leak |
| Binding semantics broken by shared context | misses | catches |

The user's instruction: "shared-context EGL may make some moot or
change their shape vs. QuickJS-era assumptions" — active probe
distinguishes these cases.

**Spec format per candidate.** Each probe = SET, CROSS, READ-BACK,
INTERPRET. The CROSS is exactly ONE Skia frame (the engine present
hook → `nx_skia_gpu_present` issues whatever GL traffic Ganesh-GL
needs to paint the current canvas surface and swap buffers).

**Active probe sequence (universal):**
1. Acquire shared GL context (the bridge runs in it already; no
   eglMakeCurrent needed if probe runs from inside the existing per-
   frame compose path).
2. SET: bind a known non-default value for the candidate. Detail
   per-candidate below. ALSO: snapshot what Skia/Ganesh's expected
   state was just before our SET (via `glGetIntegerv`) — call this
   `pre`.
3. RECORD: `glGetIntegerv` immediately after SET, confirm it stuck —
   call this `set`. (Catches the case where the GL driver refused our
   SET — INVALID_VALUE etc. — so we don't read a false leak result.)
4. CROSS: yield to the engine's normal present flow. The bridge's
   `compose-post` hook fires AFTER Skia has presented one frame.
5. READ-BACK: at `compose-post` hook, `glGetIntegerv` again — call
   this `post`.
6. RESTORE: re-bind the candidate to `pre` to leave GL state as we
   found it. Critical or we corrupt subsequent frames.
7. INTERPRET (one log line per candidate per probe pass):
   - `post == set` → Skia did NOT touch it. **Moot under shared
     context.** Does not need snapshotting.
   - `post == pre`  → Skia mutated AND restored. **Snap MUST cover it**
     — but only if `pre != set` (i.e. Skia's restore target differs
     from our SET, indicating Ganesh has a specific expected value
     that conflicts with our WebGL traffic).
   - `post != set && post != pre` → Skia mutated to a NEW value
     and left it. **Snap MUST cover it.**
   - `post == set && pre != set` → Skia restored OUR value (Skia's
     internal save/restore already covers this binding). Boundary
     between moot and needs-snapshotting depends on whether Skia's
     internal save/restore is something we can rely on across Skia
     versions; conservative interpretation = needs snapshotting.

**Per-candidate detail.**

#### (a) UBO indexed bindings

- **SET:** `glGenBuffers(1, &probeUBO); glBindBuffer(GL_UNIFORM_BUFFER,
  probeUBO); glBufferData(GL_UNIFORM_BUFFER, 256, NULL,
  GL_DYNAMIC_DRAW); glBindBufferRange(GL_UNIFORM_BUFFER, /*index=*/3,
  probeUBO, 0, 256);`. Slot 3 chosen because Ganesh-GL is documented
  to use slots 0..2 for its own UBOs (Skia tess UBOs); 3 is the first
  slot Three.js's webgl2-ubo demo would actually contend for (it uses
  slots 0 + 1 for ViewData/LightingData).
- **READ:** `glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 3, &v)`.
  Also probe slot 0 for the Ganesh-touched case.
- **INTERPRET:** Slot 3 mutation on its own = active Ganesh meddling
  beyond its documented slots → snap MUST cover all `MAX_UNIFORM_BUFFER_
  BINDINGS` slots (probe `MAX_UNIFORM_BUFFER_BINDINGS` via getParameter
  once at init for sizing). Slot 3 stable + slot 0 mutated = Ganesh
  stays in its documented lane → snap covers slots 0..N where N is the
  highest slot Ganesh touches per the probe.
- **CLEANUP:** `glDeleteBuffers(1, &probeUBO);` after RESTORE.

#### (b) Sampler-unit-0 binding

- **SET:** `glGenSamplers(1, &probeSamp); glSamplerParameteri(probeSamp,
  GL_TEXTURE_MIN_FILTER, GL_NEAREST); glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, probeSamp);`. NEAREST filter deliberately
  different from Ganesh's default (LINEAR) to catch a "Ganesh set
  filter via texParameteri" leak too.
- **READ:** `glGetIntegerv(GL_SAMPLER_BINDING, &v)` with unit 0 active.
- **INTERPRET:** Mutated = snap MUST cover `sampler_unit0`. Unmutated
  + filter unchanged in probeSamp queries = moot.
- **CLEANUP:** `glBindSampler(0, 0); glDeleteSamplers(1, &probeSamp);`.

#### (c) READ_FRAMEBUFFER (separate from DRAW)

- **SET:** create a probe FBO, bind ONLY to GL_READ_FRAMEBUFFER:
  `glGenFramebuffers(1, &probeRFB); glBindFramebuffer(GL_READ_FRAMEBUFFER,
  probeRFB);`. DRAW_FRAMEBUFFER intentionally untouched to test the
  split: existing 2.B snap saves `fbo` via `GL_DRAW_FRAMEBUFFER_
  BINDING`, restores via `glBindFramebuffer(GL_FRAMEBUFFER, ...)`
  (which sets BOTH targets). Hypothesis: Skia binds the same FBO to
  both targets on its draws → READ side is restored implicitly.
- **READ:** `glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &v)`.
- **INTERPRET:** `post == probeRFB` → Skia didn't touch READ side →
  moot. `post == 0` (default FB) → Skia explicitly unbound READ → snap
  MUST cover. `post == draw_fbo_skia_uses` → Skia bound the same FBO
  to both targets (current hypothesis) → snap MUST cover OR the
  existing restore via `GL_FRAMEBUFFER` is sufficient (test by checking
  whether 2.B's existing restore at webgl_bridge.cc:266 — `glBind-
  Framebuffer(GL_FRAMEBUFFER, (GLuint)s->fbo)` — already covers it).
- **CLEANUP:** `glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); glDelete-
  Framebuffers(1, &probeRFB);`.

#### (d) Transform feedback binding + rasterizer-discard

- **SET:** `glGenTransformFeedbacks(1, &probeTF); glBindTransformFeedback(
  GL_TRANSFORM_FEEDBACK, probeTF); glEnable(GL_RASTERIZER_DISCARD);`.
- **READ:** `glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &tfV);
  GLboolean rdV = glIsEnabled(GL_RASTERIZER_DISCARD);`.
- **INTERPRET:** Ganesh-GL is not documented to use transform feedback
  at all. **STRONGLY EXPECTED MOOT.** If either mutates, that's a
  surprise — file it loudly. The likely outcome here is that
  RASTERIZER_DISCARD passively REMAINS enabled into the next frame
  (Skia doesn't know to disable it) → "post == set" → moot for SKIA
  but **a Three.js DRAW after our enable would silently produce no
  fragments**. The latter is a WebGL→Skia AND WebGL→WebGL concern,
  not the snap's job — it's user-context discipline. Still: if active
  probe shows RASTERIZER_DISCARD stays enabled, the snap should cover
  it for defense in depth (cheap — one `glIsEnabled` + conditional
  enable/disable).
- **CLEANUP:** `glDisable(GL_RASTERIZER_DISCARD); glBindTransformFeed-
  back(GL_TRANSFORM_FEEDBACK, 0); glDeleteTransformFeedbacks(1,
  &probeTF);`.

**Implementation skeleton (next session).** Same gate as the passive
probe (`webgl_state_probe = true`), but a separate config flag
`webgl_state_probe_active = true` opts into the active sequence. The
active probe runs ONCE per launch (not per-frame — the SET introduces
GL traffic that would otherwise mask the leak signal). Hook point:
the FIRST `nx_webgl_bridge_compose` call where the bridge is
initialized and Skia has rendered at least one frame (i.e. `s_probe_
compose_n == 1` slot). One log line per candidate. After all 4
candidates probe + cleanup, set a `s_active_probe_done` flag and
short-circuit subsequent calls.

**Output format (one block per launch):**
```
[webgl-bridge:probe-active] candidate=ubo_slot3 pre=0 set=42 post=42 verdict=moot
[webgl-bridge:probe-active] candidate=ubo_slot0 pre=0 set=43 post=0 verdict=NEEDS_SNAP_LEAVE
[webgl-bridge:probe-active] candidate=sampler_unit0 pre=0 set=51 post=51 verdict=moot
[webgl-bridge:probe-active] candidate=read_fbo pre=0 set=44 post=0 verdict=NEEDS_SNAP_LEAVE
[webgl-bridge:probe-active] candidate=tf pre=0 set=45 post=45 verdict=moot
[webgl-bridge:probe-active] candidate=rast_disc pre=0 set=1 post=1 verdict=moot-but-defense-warranted
[webgl-bridge:probe-active] SUMMARY needs_snap={ubo_slot0,read_fbo} moot={ubo_slot3,sampler_unit0,tf,rast_disc}
```

The SUMMARY line directly drives #17's cut: needs_snap set → fields
added to `nx_gl_state_snap_t`; moot set → fields NOT added.

**Citron caveat.** The active probe runs deterministically on Citron
(host AMD Vulkan translation), but Skia's GL traffic differs from
real Tegra Mesa-Nouveau (different GL extension set → different
Ganesh codepaths). The active probe is hardware-required. Citron can
be a sanity check that the probe code itself doesn't crash, but
verdict lines from Citron are NOT authoritative.

---

## #17 — nx_gl_state_snap_t extension for sampler_unit0 + read_fbo (Phase 2.G.1) — SHIPPED 2026-07-01

**STATUS: SHIPPED.** Extension driven by the authoritative #16-ACTIVE
probe SUMMARY line captured on real Switch hardware (auto-detect said
`target=hardware ... -> mode=jit`, WASM headroom 64 MiB, full JIT
tier-up). Hardware verdict:

```
[webgl-bridge:probe-active] candidate=ubo_slot3 pre=0 set=1 post=1 verdict=moot
[webgl-bridge:probe-active] candidate=ubo_slot0 pre=0 post=0 verdict=moot-passive
[webgl-bridge:probe-active] candidate=sampler_unit0 pre=0 set=1 post=2 verdict=NEEDS_SNAP_LEAVE
[webgl-bridge:probe-active] candidate=read_fbo pre=0 set=1 post=0 verdict=NEEDS_SNAP_MUTATE_RESTORE
[webgl-bridge:probe-active] candidate=tf pre=0 set=1 post=1 verdict=moot
[webgl-bridge:probe-active] candidate=rast_disc pre=0 set=1 post=1 verdict=moot
[webgl-bridge:probe-active] SUMMARY needs_snap={sampler_unit0,read_fbo}
```

**File(s) (SHIPPED 2026-07-01):**
[source/webgl_bridge.h](source/webgl_bridge.h) — `struct nx_gl_state_snap_t` extended with `GLint sampler_unit0` + `GLint read_fbo` fields + header comment updated with #17 rationale + verdict interpretation
[source/webgl_bridge.cc](source/webgl_bridge.cc) — `nx_gl_state_save` reads `GL_SAMPLER_BINDING` on transiently-active unit 0 and `GL_READ_FRAMEBUFFER_BINDING`; `nx_gl_state_restore` binds them back in the correct order (READ_FRAMEBUFFER re-bind AFTER the `glBindFramebuffer(GL_FRAMEBUFFER, draw_fbo)` call because the latter binds BOTH targets — only re-fire the READ bind if `read_fbo != draw_fbo`; skips the redundant call for the common case)

**Moot fields (NOT added, per probe SUMMARY):**
- `ubo_slot3`, `ubo_slot0` — Ganesh-GL doesn't touch UBO indexed bindings on this hardware; slot 3 stayed at our SET value, slot 0 stayed at pre-value 0. Save/restore would be dead code.
- `tf`, `rast_disc` — Ganesh doesn't use transform feedback or toggle rasterizer discard. `rast_disc` has a passive concern (WebGL draws after a user's enable produce no fragments — but that's user-context discipline, not the snap's job).

**Symptom it fixes (predicted, hardware verification pending).** Two v2 demos gated on this snap extension per the queue:
- `webgl2-multiple-rendertargets` — Three.js's MRT setup binds read/draw FBOs separately for glReadBuffer/glDrawBuffers coordination. Without `read_fbo` snap coverage, Skia's read=0 assumption after WebGL's split-bind produces silently-wrong readback (or the MRT tail cosmetic artifact per [[project-v8-migration-phase2g1-more-demos]]).
- `gpgpu-water` — heavy sampler_unit0 use (compute-like ping-pong with textureLookup + specific sampler objects per pass). Without `sampler_unit0` snap coverage, Ganesh's post-Three.js frame samples the WebGL demo's leftover sampler object, producing corrupt Skia paint until the next unit-0 rebind.

**Why upstream-vanilla lacks it.** No bridge; no state contract to save/restore.

**DISPOSITION:** `fork-only`. State-contract discipline specific to our shared-context WebGL↔Skia coexistence model.

**UPSTREAM STATUS:** `n/a` (fork-only).

**RE-APPLY / VERIFY NOTE.** If an upstream V8-fork pull loses these fields, grep `source/webgl_bridge.h` for `sampler_unit0`. Missing → re-apply save+restore per the SHIPPED code above. Recurrence tell: after a snap regression, WebGL2 MRT demos show cosmetic bleed at the compose boundary; gpgpu demos render wrong pixels one frame after each Skia paint.

**Followups.**
- If a future v2 demo lands on hardware and shows corrupt Skia paint AFTER working WebGL, that's a state-leak the probe didn't catch (either at a different active_texture unit than 0, or in a probe-moot binding whose semantics changed under a new demo). Re-enable the active probe with an extended candidate list — the shim's SET/READ/RESTORE pattern is now proven, just add candidates to `nx_active_probe_state_t`.
- `rast_disc` passive concern (moot for snap, but WebGL draws after a demo-enable produce no fragments): if a future demo enables `GL_RASTERIZER_DISCARD` and then does draws expected to land on Skia's paint, look here.

---

## #17-superseded — original PROPOSED template (superseded by SHIPPED entry above)

**File(s) (planned):**
[source/webgl_bridge.h](source/webgl_bridge.h) (`struct nx_gl_state_snap_t`
+ FROZEN-contract header comment update);
[source/webgl_bridge.cc](source/webgl_bridge.cc) (`nx_gl_state_save` /
`nx_gl_state_restore` body extensions).

### Template (apply ONLY the `#ifdef`-gated members the probe verdict elects)

```c
/* In webgl_bridge.h, REPLACING the existing struct:                       */

/* Phase 2.G.0 — snap extension feature flags. Each defaults OFF; the
 * user-signed-off probe-driven set defines exactly which flip on. The
 * flag set is the LEGAL CUT of the extension; flags not flipped do not
 * grow the struct, do not cost the save/restore roundtrip, and do not
 * change the FROZEN 2.B contract's footprint.
 *
 * Probe-driven flags (set per #16-ACTIVE SUMMARY's needs_snap list):
 *   NX_SNAP_UBO_INDEXED         — UBO indexed bindings 0..N
 *   NX_SNAP_SAMPLER_UNIT0       — sampler-unit-0 binding
 *   NX_SNAP_READ_FBO            — READ_FRAMEBUFFER separate from DRAW
 *   NX_SNAP_TF_BINDING          — transform feedback object binding
 *   NX_SNAP_RASTERIZER_DISCARD  — RASTERIZER_DISCARD enable
 */

/* Slot count for the UBO indexed array. Sized from MAX_UNIFORM_BUFFER_
 * BINDINGS at engine init AT MOST; or hardcoded to the highest probed-
 * leaky slot + 1 (whichever is smaller). Three.js v2 r182 webgl2-ubo
 * uses slots 0+1; pad to 4 for headroom unless probe finds Ganesh
 * touches beyond. */
#ifndef NX_UBO_SAVE_SLOTS
#define NX_UBO_SAVE_SLOTS 4
#endif

struct nx_gl_state_snap_t {
    /* === Existing 18 fields preserved EXACTLY (do not reorder, do
     *     not retype, do not rename — the 2.B FROZEN contract). === */
    GLint fbo;
    GLint viewport[4];
    GLint program;
    GLint vao;
    GLint array_buffer;
    GLint active_tex;
    GLint tex2d_binding;
    GLboolean blend;
    GLboolean depth_test;
    GLboolean cull;
    GLboolean scissor;
    GLboolean stencil_test;
    GLboolean color_mask[4];
    GLfloat clear_color[4];
    GLint blend_src_rgb;
    GLint blend_dst_rgb;
    GLint blend_src_a;
    GLint blend_dst_a;

    /* === Phase 2.G.0 batched extension. Each member is gated on its
     *     probe-driven feature flag. Members whose flag is off cost
     *     zero (no struct growth, no save/restore GL roundtrip). === */

#ifdef NX_SNAP_UBO_INDEXED
    GLint ubo_base;                       /* GL_UNIFORM_BUFFER_BINDING */
    GLint ubo_indexed[NX_UBO_SAVE_SLOTS]; /* glGetIntegeri_v slots 0..N */
#endif

#ifdef NX_SNAP_SAMPLER_UNIT0
    GLint sampler_unit0;                  /* GL_SAMPLER_BINDING @ TU 0 */
#endif

#ifdef NX_SNAP_READ_FBO
    GLint read_fbo;                       /* GL_READ_FRAMEBUFFER_BINDING */
#endif

#ifdef NX_SNAP_TF_BINDING
    GLint tf_binding;                     /* GL_TRANSFORM_FEEDBACK_BINDING */
#endif

#ifdef NX_SNAP_RASTERIZER_DISCARD
    GLboolean rasterizer_discard;         /* glIsEnabled(GL_RASTERIZER_DISCARD) */
#endif
};

/* In webgl_bridge.cc, nx_gl_state_save body — APPEND to existing reads: */

void nx_gl_state_save(nx_gl_state_snap_t *s) {
    /* ... existing 18 saves preserved EXACTLY ... */

#ifdef NX_SNAP_UBO_INDEXED
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &s->ubo_base);
    for (int i = 0; i < NX_UBO_SAVE_SLOTS; i++) {
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, i, &s->ubo_indexed[i]);
    }
#endif

#ifdef NX_SNAP_SAMPLER_UNIT0
    {
        GLint prev_active;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_SAMPLER_BINDING, &s->sampler_unit0);
        glActiveTexture((GLenum)prev_active);
    }
#endif

#ifdef NX_SNAP_READ_FBO
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s->read_fbo);
#endif

#ifdef NX_SNAP_TF_BINDING
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &s->tf_binding);
#endif

#ifdef NX_SNAP_RASTERIZER_DISCARD
    s->rasterizer_discard = glIsEnabled(GL_RASTERIZER_DISCARD);
#endif
}

/* In webgl_bridge.cc, nx_gl_state_restore body — APPEND to existing
 * writes BEFORE the existing trailing glBlendFuncSeparate (the order
 * matters: existing fields are restored first, then the extension): */

void nx_gl_state_restore(const nx_gl_state_snap_t *s) {
    /* ... existing 18 restores preserved EXACTLY ... */

#ifdef NX_SNAP_UBO_INDEXED
    glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)s->ubo_base);
    for (int i = 0; i < NX_UBO_SAVE_SLOTS; i++) {
        /* glBindBufferBase is the right restore call when range/offset
         * isn't tracked. The probe data tells us whether we ALSO need to
         * track offset+size: if probe shows a non-0 binding, also
         * snapshot GL_UNIFORM_BUFFER_START[i] and GL_UNIFORM_BUFFER_SIZE
         * [i], and restore via glBindBufferRange instead. Final-final
         * decision deferred to probe output. */
        glBindBufferBase(GL_UNIFORM_BUFFER, i, (GLuint)s->ubo_indexed[i]);
    }
#endif

#ifdef NX_SNAP_SAMPLER_UNIT0
    {
        GLint prev_active;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
        glActiveTexture(GL_TEXTURE0);
        glBindSampler(0, (GLuint)s->sampler_unit0);
        glActiveTexture((GLenum)prev_active);
    }
#endif

#ifdef NX_SNAP_READ_FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)s->read_fbo);
#endif

#ifdef NX_SNAP_TF_BINDING
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, (GLuint)s->tf_binding);
#endif

#ifdef NX_SNAP_RASTERIZER_DISCARD
    if (s->rasterizer_discard) glEnable(GL_RASTERIZER_DISCARD);
    else                       glDisable(GL_RASTERIZER_DISCARD);
#endif
}
```

### Cut-decision rule (parameterized on #16-ACTIVE SUMMARY)

For each candidate verdict in the active probe's SUMMARY line:

| Verdict (#16-ACTIVE) | Action on #17 cut |
|---|---|
| `moot` | Do NOT define the corresponding `NX_SNAP_<X>` flag. Field stays out; save/restore unchanged. |
| `NEEDS_SNAP_LEAVE` (post != set, Skia left it mutated) | DEFINE the flag. Field + save/restore lines fire. |
| `NEEDS_SNAP_RESTORE` (post == pre != set, Skia restored to its own pre-state) | DEFINE the flag. Same code path; Skia's restore not relied on. |
| `moot-but-defense-warranted` | DEFINE if the defense cost (one query + one conditional restore per frame) is judged worthwhile vs the risk. Default yes for RASTERIZER_DISCARD. |

### Hardware re-validation checklist (post-extension)

1. Boot brewser-v8 on real CFW Switch with `NX_SNAP_<X>` defines per probe.
2. v1 demo regression set: `geometry-cube`, `webgl-materials-cubemap`,
   `webgl-loader-gltf`, `webgl-shadowmap`, the v1 black-texture quartet
   (post-#10 SRGB fix) — all must render unchanged.
3. Sustained ≥ 60 s. With `state_probe = true`, the `[webgl-bridge:probe]`
   lines (#16 passive) should show our snapshotted values correctly
   restored across frames.
4. NO new flicker / no Skia corruption / no FBO incomplete.

**Why a single batched extension (not N increments).** Each snap
extension is hardware-revalidated (the snap is FROZEN; touching it
risks unmasking a Ganesh state-cache assumption). Batching all
probe-positive fields into one extension means **one** hardware pass
re-verifies the contract vs N if added incrementally. Hardware passes
are the load-bearing cost.

**Symptom it fixes.** Phase 2.G.X demos (webgl2-ubo at 2.G.1,
webgl2-multiple-rendertargets at 2.G.2, gpgpu-water at 2.G.7) would
exhibit "Skia renders garbage after WebGL2 draws" symptoms on
hardware if any of the candidate bindings leak across the WebGL→Skia
handoff and the snap doesn't cover them. The parameterized template +
probe-driven cut means we extend by the MINIMUM correct set, not by
guesswork.

**DISPOSITION:** `upstream-candidate`. Engine-correctness: the
snap-set should cover all leakable state under shared-context EGL.
The `#ifdef`-gated shape is for documentation + reversibility; can be
cleaned up to unconditional members once the cut is final and the
gates are no longer needed for audit.

**UPSTREAM STATUS:** `not-submitted` (template only).

**RE-APPLY / VERIFY NOTE.** Wait for #16 + #16-ACTIVE probe data +
user sign-off. Apply the template with the gate set per SUMMARY.
Hardware-revalidate per the checklist above. Update webgl_bridge.h's
FROZEN-contract header comment to enumerate the new fields + reasons
(copy the relevant "INTERPRET" bullet from the #16-ACTIVE per-
candidate spec into the header doc for each landed field).

---

## #18 — cube-route-shim per-method capability guards (Phase 2.G.0 — runtime-side, unblocks empty/partial v2 from canvas-runner) — SHIPPED 2026-06-30

**File(s):**
[brewser-runtime-v8/src/scripts/cube-route-shim.ts](../brewser-runtime-v8/src/scripts/cube-route-shim.ts)
(`installCubeRouting` — `safeBind` helper introduced; every wrap
`target.X = function(...)` site wrapped in `if (origDep1 && origDep2
&& ...) { ... } else if (diagOn()) { console.debug('[f2a:guard]',
'skip wrap X') }`).

**Exact change (Phase 2.G.0).** Replace the 14 unconditional
`gl.X.bind(gl)` calls at the top of `installCubeRouting` with a
`safeBind(name)` helper that returns `null` when the named method is
missing on `gl`. Then wrap each of the 12 `target.X = function ...`
assignments in an `if (origDep1 && origDep2 && ...)` gate listing
EVERY `origX` ref the wrapper invokes at runtime. When any required
ref is null, the wrap is silently skipped; the raw `gl.X` (whether
present or missing) is preserved unchanged.

**Method-dependency map (every wrap → its required-orig set):**

| Wrap | Required origs |
|---|---|
| `activeTexture` | `origActiveTexture` |
| `bindTexture` | `origBindTexture` |
| `texImage2D` | `origTexImage2D`, `origBindTexture`, `origTexParameteri`, `origTexSubImage2D`, `gl.createTexture` (runtime check) |
| `texSubImage2D` | `origTexSubImage2D`, `origBindTexture` |
| `texParameteri` | `origTexParameteri`, `origBindTexture` |
| `texParameterf` | `origTexParameterf`, `origBindTexture` |
| `generateMipmap` | `origGenerateMipmap` |
| `createShader` | `origCreateShader` |
| `shaderSource` | `origShaderSource` |
| `attachShader` | `origAttachShader` |
| `getActiveUniform` | `origGetActiveUniform` |
| `compileShader` | `origCompileShader`, `origGetShaderParameter`, `origGetShaderInfoLog` (all 3 diag-only) |

**Symptom it fixes.** Before guard: `screen.getContext('webgl2')` on
a Phase 2.G.0 empty v2 context succeeded engine-side (`[webgl2]
context_new ok ...` in log), but `canvas-runner.ts::getSharedScreenGL2`
then called `installCubeRouting(gl)` whose unconditional
`gl.activeTexture.bind(gl)` at line 132 dereferenced `undefined.bind`
→ `TypeError` → caught at `getSharedScreenGL2`'s try/catch → returned
`null` → Three.js's `WebGL.isWebGL2Available()` saw null on its probe
canvas → reported **"WebGL 2 not supported"**.

After guard: empty v2 → every wrap's deps are null → every wrap is
silently skipped → `installCubeRouting` returns cleanly →
`getSharedScreenGL2` returns the empty v2 → Three.js's probe sees a
valid `WebGL2RenderingContext` → `isWebGL2Available()` passes.
**Calling any GL method on the empty context throws `TypeError`
later** — that's correct 2.G.0 behavior (the demo's first GL call
fails, but the WebGL2 capability gate passes).

After 2.G.1 binds SOME but not all v2 methods (partial population),
the guard fires per-wrap: wraps whose deps are now bound install
normally; wraps whose deps are still missing skip silently. Every
state from empty → full is correct.

**Hooks are method-set DISJOINT, no ordering dependency.**
`installBridgeDirtyHooks` wraps `drawArrays`/`drawElements`/`clear`
and is ALREADY per-method-guarded (canvas-runner.ts:795 `if (typeof
orig !== 'function') return`). `installCubeRouting` wraps the
texture/shader set. No method is wrapped by both. Install order is
indifferent.

**Open question deliberately NOT settled here.** cube-route-shim is
the v1-era Bucket F.2a #12 rescue for the Mesa-Nouveau samplerCube
driver limit. v2 cube routing applicability is a Phase 2.G.4 re-
verify — see #19. The guard makes cube-route-shim NON-CRASHING on
v2; it does NOT decide whether v2 SHOULD have cube routing at all.
If 2.G.4 determines v2 should not, gate the `installCubeRouting`
CALL at canvas-runner.ts:361, not the function itself.

**Why upstream-vanilla lacks it.** cube-route-shim is fork-only
runtime code (#12). The guard is a fork-only refinement of fork-only
code.

**DISPOSITION:** `brewser-specific`. Runtime-side; fixes a
brewser-runtime-v8 / nx.js V8 fork integration corner. Stays in the
runtime fork.

**UPSTREAM STATUS:** `n/a`.

**RE-APPLY / VERIFY NOTE.** After upstream pull (or
brewser-runtime-v8 refactor), grep `cube-route-shim.ts` for `safeBind`.
If absent, re-apply the guard pattern: replace the 14 `.bind(gl)`
calls with `safeBind` and wrap each `target.X = function` in an
`if (origDeps)` gate. Diagnostic `[f2a:guard]` log lines surface
every skipped wrap when `__f2aDiag` is on.

**Recurrence tell.** If a future contributor reverts to unconditional
`.bind(gl)` because the codebase "looks cleaner without the if-blocks",
v2 instantiation re-breaks on 2.G.0 (empty) AND on any 2.G.X partial
state where the contributor adds a method to cube-route-shim's
dependencies without verifying it's bound by 2.G.X's method-table cut.

---

## #24 — Cube-RT-readback rescue (runtime shim; Phase 2.G.1 cut #24) — SHIPPED 2026-07-01, HARDWARE-VERIFY PENDING

**File(s):**
- MODIFIED [brewser-runtime-v8/src/scripts/cube-route-shim.ts](../brewser-runtime-v8/src/scripts/cube-route-shim.ts) (~150 lines added)

**Root cause.** cube-route-shim (#12) atlases USER cube uploads (`texImage2D(POSITIVE_X+i, ..., image)`) into a 2D strip. WebGLCubeRenderTarget-populated cubes (Three.js's `CubemapFromEquirect` for `scene.background = equirectTex` + `CubeCamera` for `materials-cubemap-dynamic`) bypass that path: face storage is allocated via nullally-source `texImage2D(POSITIVE_X+i, ..., null)`, then the FBO writes to face N via `framebufferTexture2D`. The shim previously skipped null uploads entirely, so no atlas was allocated. Also, Mesa-Nouveau's driver silently aliases FBO writes to face N>0 → face 0 storage anyway ([[reference-mesa-cube-face-aliasing-rescue]]), so even if the shim COULD sample the raw cube, it would sample garbage. Result before this cut: the shim's `bindTexture(CUBE_MAP)` re-bind leaves TEXTURE_2D at whatever was last there (Skia glyph atlas / DOM compose surface), and the rewritten sampler2D envMap samples HTML content as the skybox.

**Fix (runtime, per-context).** In `installCubeRouting`:
1. Extend `CubeState` with `scratchTex?`, `scratchAllocated?`, `rtInternalformat?`, `rtFormat?`, `rtType?`, `isRenderTarget?`.
2. Add `fboCubeStates: WeakMap<WebGLFramebuffer, {cubeTex, faceIdx, hasContent}>` + `currentDrawFBO: WebGLFramebuffer | null`.
3. New helper `allocateCubeRTAtlas(cubeTex, w, h, intl, fmt, type)` — allocates 6×w × h atlas + w × h scratch, both LINEAR/CLAMP_TO_EDGE; sets `isRenderTarget = true`.
4. New helpers `flushCubeFaceToAtlas(cubeTex, faceIdx)` (bind atlas as TEXTURE_2D + `copyTexSubImage2D` from current FBO at `faceIdx * faceW` offset) and `flushPendingCubeFace(fb)` (idempotent flush of the FBO's pending face).
5. Existing `texImage2D` null-source path: after forwarding to engine, if `source === null && w >= 8 && h >= 8`, call `allocateCubeRTAtlas`.
6. NEW wrap: `framebufferTexture2D(FRAMEBUFFER/DRAW_FRAMEBUFFER, ..., POSITIVE_X+i, cubeTex, level)` → if the cube is RT-marked with a scratch, flush any pending face on the same FBO with a different face, then redirect to `framebufferTexture2D(fbTarget, attachment, TEXTURE_2D, cubeState.scratchTex, level)` + record `fboCubeStates[fbo] = {cubeTex, i, hasContent: true}`.
7. NEW wrap: `bindFramebuffer(target, fb)` — if switching away from `currentDrawFBO` and it has a pending cube face, flush BEFORE switching (`copyTexSubImage2D` reads from the CURRENT framebuffer, so it must still be bound).

**Why this bypasses the Mesa-Nouveau alias bug.** No cube-face writes ever happen. All FBO writes hit `TEXTURE_2D + scratchTex`, which is a normal 2D attachment that the driver handles correctly. The scratch → atlas copy is a `TEXTURE_2D → TEXTURE_2D` `copyTexSubImage2D`, which the QuickJS-era rescue confirmed reaches sampler-visible storage (`glCopyTexSubImage2D` was broken only for cube-face writes).

**Depth attachment.** Three.js allocates a face-size 2D depth renderbuffer for cube RTs and shares it across the 6 face passes. Since scratchTex is also face-size, `FRAMEBUFFER_COMPLETE` after our redirect. No wrap of `framebufferRenderbuffer` needed.

**Sampling path.** Unchanged — existing `bindTexture(TEXTURE_CUBE_MAP, cubeTex)` wrap redirects TEXTURE_2D[activeTU] to the atlas, existing shader rewrite converts samplerCube reads to `cubeUVSample(sampler2D, dir)` on the atlas.

**DISPOSITION:** `brewser-specific` (runtime-side). Zero engine delta.

**UPSTREAM STATUS:** `n/a`.

**Unlocks (after hardware verify).**
- `scene.background = equirectTex` natural path (materials-envmaps + gpgpu-water) — replaces cut #17 / cut #23 pmremRT.texture workarounds. Slight HDR range preserved on Citron; on Mesa-Nouveau limited to LDR because Three.js's `CubemapFromEquirect` uses RGBA8 internal by default.
- `materials-cubemap-dynamic` (CubeCamera per-frame cube renders) — first demo that CAN'T be worked around by pmremRT.texture (per-frame content), so this is the load-bearing unlock.

**Not covered (yet).**
- `framebufferTextureLayer(target, attachment, cubeTex, level, layer)` — WebGL2's alternate cube-face attachment API. Not currently seen in Three.js r184's cube RT path (uses framebufferTexture2D), but any demo that goes through it will bypass our redirect. Add a parallel wrap if a demo trips this.
- HDR (RGBA16F) cube RTs — `copyTexSubImage2D` should handle the internal format transparently, but not exercised by current v2 demos (Mesa-Nouveau's PMREM path forces LDR downgrade upstream). Verify separately if HDR cube RT demo lands.

**RE-APPLY / VERIFY NOTE.**

*To verify still needed*: revert cut #17 in materials-envmaps to `scene.background = equirectTex`; the demo should render the equirect skybox without HTML/Skia bleed. If HTML shows again, rescue regressed — grep `cube-route-shim.ts` for `allocateCubeRTAtlas`, `flushPendingCubeFace`, `framebufferTexture2D` (should have TWO occurrences: the wrap install + the origBindFramebuffer/origFramebufferTexture2D safeBind block).

*Recurrence tell*: any demo using CubeCamera / WebGLCubeRenderTarget showing solid-color, HTML, or Skia glyph atlas content on any cube surface. First check that the shim's diag markers fire (`__f2aDiag = true` → `[f2a:rt-atlas-alloc]`, `[f2a:rt-redirect]`, `[f2a:rt-flush]` should all appear in nxjs-debug.log; missing = the wrap install skipped due to a safeBind guard fail).

---

## #19 — Cube-routing applicability under WebGL2 — OPEN, Phase 2.G.4 re-verify required

**STATUS: OPEN.** The guard in #18 makes `installCubeRouting` non-
crashing on v2 contexts. It deliberately does NOT settle whether
cube-route-shim is the right behavior for v2 at all.

**Background.** cube-route-shim (#12 — SHIPPED 2026-06-29) is the
Bucket F.2a runtime-side rescue for Tegra/Mesa-Nouveau's samplerCube
driver limit ([[reference-mesa-nouveau-layered-sampling-unsupported]]).
It rewrites GLSL `samplerCube` declarations + `textureCube` calls in
v1 (and currently v2, by canvas-runner.ts:361 + 1505 installing it
unconditionally) shaders into `sampler2D` + 6×1 strip atlas reads.

**The Phase 2.G.4 question.** Does v2 need the SAME shim, a DIFFERENT
shim, or NO shim?

Arguments FOR re-using the v1 shim on v2:
- The Mesa-Nouveau samplerCube driver limit applies regardless of
  WebGL version (it's a GLES3 driver layer issue, not a WebGL spec
  issue).
- Three.js's v2 codepath still emits `samplerCube` uniforms in its
  GLSL (the WebGL2 backend produces GLSL ES 300 which uses
  `samplerCube` and `texture(samplerCube, vec3)` calls — the shim
  already handles the GLSL ES 300 branch at cube-route-shim.ts:647).
- Re-using means zero new code.

Arguments AGAINST re-using as-is:
- The v1 shim was VERIFIED on v1 demos (`webgl-materials-cubemap`
  green on Citron + hardware) — v2 demos may have subtly different
  Three.js codepaths (e.g. `WebGLRenderer.outputColorSpace`, `lib.glsl
  preludes` for v2) that need different rewriting.
- The shim wraps `texImage2D` to atlas-route cube faces — but v2 also
  has `texImage3D` / `texSubImage3D`. If a v2 demo uploads cube faces
  via 3D texture API, the shim misses them. (Likely not the case for
  Three.js's CubeTexture path which uses 2D, but worth verifying
  per-demo.)
- v2 has `samplerCubeShadow` (cube depth sampling). NOT in v1. Shim
  has no path for it.
- The 2.G.0 method-table for v2 is currently empty; even when 2.G.1
  starts populating it, the cube-shim's depended-on methods (texImage2D,
  bindTexture, shaderSource, etc.) need to be bound BEFORE the shim's
  wraps become functional. This is naturally handled by #18's guards,
  but the question of whether ANY v2 method gets cube-routed is
  separate.

**Required validation (Phase 2.G.4 work).** Run each of:
- `materials-cubemap-dynamic` — CubeCamera dynamic cubemap; FBO writes
  to cube faces. Shim's `texImage2D` hook handles uploads, but FBO
  writes go via `framebufferTexture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X,
  ...)` which the shim does NOT wrap.
- `materials-envmaps` — static envmap usage. Closest to v1
  `webgl-materials-cubemap` the shim was verified on.
- `webgl-loader-gltf` (v1 cross-demo regression check post-#18 —
  PMREM path with cube-uv envMap that the shim's dual-decl gate
  handles).

For each, run with `__f2aDiag = true` and observe whether `[f2a:*]`
markers fire as expected and the demo renders correctly.

**Open subquestion.** If v2 cube-routing IS needed but currently
breaks something, the patch hierarchy is:
1. Gate the `installCubeRouting(gl)` CALL at canvas-runner.ts:361 +
   1505 on a separate v1-vs-v2 check (probe `gl.constructor.name` or
   check for v2-only methods).
2. Add v2-specific wraps (e.g., `framebufferTexture2D` + cube-face
   target → 2D atlas attachment) inside the shim, gated by v2
   detection.
3. Write a parallel `installCubeRouting2(gl)` for v2 that diverges
   structurally from v1's shim. Worst case; only if v2 needs are
   fundamentally different.

**DISPOSITION:** `brewser-specific` (runtime-side).

**UPSTREAM STATUS:** `n/a`.

**RESOLUTION TIMING.** Block this open question on Phase 2.G.4
(materials-cubemap-dynamic + materials-envmaps demo passes). Do NOT
attempt to resolve in 2.G.1 (webgl2-ubo slice — no cube usage) or
2.G.2 (webgl2-multiple-rendertargets — no cube usage).

---

## #20 — UNPACK_FLIP_Y_WEBGL honor for typed-array texImage2D uploads — OPEN, Phase 2.G.4+ engine fix deferred

**STATUS: OPEN.** Current workaround in RGBELoader (both v1 + v2 libs) pre-flips row data client-side; engine-side fix would let ALL DataTexture/typed-array uploads with `flipY=true` behave per spec.

**File(s) (workaround, SHIPPED 2026-07-01, Phase 2.G.1 cut #18):**
- [brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/libs/rgbe-loader.js](../brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/libs/rgbe-loader.js) (~line 213 — row reverse before DataTexture construction + `tex.flipY = false`)
- [brewser-apps/apps/experimental/com.natureglass.webgl1threejsdemos/libs/rgbe-loader.js](../brewser-apps/apps/experimental/com.natureglass.webgl1threejsdemos/libs/rgbe-loader.js) (identical patch — same code modulo `__THREE_R184_STAGED__` vs `__THREE_R162_STAGED__`)

**File(s) (engine fix target):** [source/webgl.cc](source/webgl.cc) — `w_tex_image_2d` (line ~1252) and `w_tex_sub_image_2d` (line ~1274).

**Root cause.** Engine's [source/webgl.cc:116](source/webgl.cc#L116) records `st->unpack_flip_y = (val != 0)` in `w_pixel_storei` for `UNPACK_FLIP_Y_WEBGL` (0x9240), but the flag is NEVER read elsewhere. `w_tex_image_2d` / `w_tex_sub_image_2d` pass the ArrayBuffer bytes straight to `glTexImage2D` / `glTexSubImage2D`. Class comment on line 115 explicitly acknowledges the gap: "WebGL-only pixel store emulation state (stored only; 2.E does the work)" — 2.E did NOT do the work.

**Symptom it causes.** Any DataTexture with `flipY=true` (Three.js default) + typed-array data source ends up in GPU memory in file-top-to-bottom order (byte offset 0 = file row 0 = image top). But OpenGL sampling convention treats byte offset 0 = image bottom → sampling with v=1 reads the file bottom row instead of the file top row → image sampled upside-down. Manifests visibly on any content with vertical orientation (skyboxes, portraits, panoramas). Materials-envmaps + webgl-loader-gltf HDR skyboxes were both upside-down before cut #18.

**Engine fix (deferred).** In `w_tex_image_2d` / `w_tex_sub_image_2d`, when `st->unpack_flip_y && pixels != nullptr && width > 0 && height > 1`, compute `bytes_per_pixel(format, type)`, `row_bytes = align_up(width * bpp, st->unpack_alignment)`, allocate temp buffer, memcpy rows in reverse order, pass temp to `glTexImage2D`, free after. Format+type→bpp helper needed:
- Format channels: RED/ALPHA/LUMINANCE/DEPTH → 1; RG/LUMINANCE_ALPHA/DEPTH_STENCIL → 2; RGB/SRGB → 3; RGBA/SRGB_ALPHA → 4.
- Type bytes: UBYTE/BYTE → 1; USHORT/SHORT/HALF_FLOAT/HALF_FLOAT_OES → 2; UINT/INT/FLOAT → 4.
- Packed types (5_6_5, 4_4_4_4, 5_5_5_1) → fixed 2 bpp. 2_10_10_10_REV, 10F_11F_11F_REV, 5_9_9_9_REV, 24_8 → fixed 4 bpp.

**WHY DEFERRED (blast radius).** Multiple currently-working textures may depend on "flag ignored" as compensation:
1. Textures uploaded via cube-route-shim's OffscreenCanvas→getImageData path (already returns top-down bytes; if flipY comes through as true, we'd double-compensate).
2. Textures where the demo pre-flipped data (any custom loader that already did the flip and set flipY=false OR left flipY=true assuming no-op).
3. Any Three.js DataArrayTexture / Data3DTexture / DataTexture path where the WebGL 2 upload code sets `pixelStorei(UNPACK_FLIP_Y_WEBGL, texture.flipY)` unconditionally.

Ship alongside a systematic sweep of DataTexture uploads with `flipY=true` — verify none rely on the compensation — before enabling engine-side.

**DISPOSITION:** `upstream-candidate` (once shipped). Engine correctness matches browser behavior.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.**

*To verify the workaround is still needed* after any V8-engine upstream pull: grep `source/webgl.cc` for `unpack_flip_y`. If the flag is only set (in `w_pixel_storei`) and never read from `w_tex_image_2d` / `w_tex_sub_image_2d`, workaround still needed.

*To upgrade to the engine fix.* When ready to ship engine-side, drop the client-side pre-flip loops in both rgbe-loader.js files (restore `tex.flipY = true`), implement engine-side flip per the recipe above, and sweep the rest of the codebase for latent compensating pre-flips.

---

## #21 — sampler2DShadow → sampler2D runtime shader-rewrite shim (Phase 2.G.1 cut #21) — SHIPPED 2026-07-01

**File(s):**
- NEW [brewser-runtime-v8/src/scripts/shadow-route-shim.ts](../brewser-runtime-v8/src/scripts/shadow-route-shim.ts) (~230 lines)
- MODIFIED [brewser-runtime-v8/src/scripts/canvas-runner.ts](../brewser-runtime-v8/src/scripts/canvas-runner.ts) — import + `installShadowRouting(gl)` after `installCubeRouting(gl)` at both `getSharedScreenGL()` and `getSharedScreenGL2()` sites.

**Root cause.** Mesa-Nouveau on Tegra X1 has no working `sampler2DShadow` hardware-compare path. Cut #19 diag rounds 1-3 (nxjs-source-v8/source/webgl.cc temporary instrumentation, reverted after cut #21 landed) proved:
1. `glTexParameteri(TEXTURE_2D, TEXTURE_COMPARE_MODE, COMPARE_REF_TO_TEXTURE)` lands correctly on the depth texture (err=0x0, `bound_tex` matches the FBO-attached tex).
2. Compare mode SURVIVES `glTexStorage2D` and `glFramebufferTexture2D` unchanged (`post_mode=0x884E`).
3. Shadow FBO is `FRAMEBUFFER_COMPLETE` (0x8CD5).
4. Shadow render pass executes every frame — one `glClear(COLOR|DEPTH|STENCIL)` + one `glDrawElements` (index count matches the shadow-caster geometry) with `dm=1 cd=1.000 err=0x0`.

Yet `sampler2DShadow` returns 0 in the main pass → `directLight.color *= 0` → SpotLight (and any shadow-gated light) invisible. Cut #20 (fallback from `glTexStorage2D` to `glTexImage2D(NULL)` for depth internalformats) also failed. Joins the driver-limit family in [[reference-mesa-nouveau-layered-sampling-unsupported]] alongside sampler3D / sampler2DArray / samplerCube.

**Fix approach.** Wrap `gl.shaderSource` to intercept Three.js's shadow-map shaders:
1. Scan `\bsampler2DShadow\s+(\w+)` to collect per-shader shadow-sampler identifier set (matches uniform decls AND function-parameter decls in the same pass — Three.js's `getShadow` takes `sampler2DShadow shadowMap` as a param).
2. Global replace `\bsampler2DShadow\b` → `sampler2D` (safe against `samplerCubeShadow` — different token; also rewrites `precision X sampler2DShadow;` decls without a follow-up identifier).
3. For each collected identifier, rewrite `texture(IDENT[?], ...)` → `textureShadowCompat(IDENT[?], ...)` (identifier-scoped — does NOT touch texture() calls on regular sampler2D uniforms).
4. Inject helper: `highp float textureShadowCompat(highp sampler2D _depths, highp vec3 _uvz) { highp float _stored = texture(_depths, _uvz.xy).r; return step(_uvz.z, _stored); }`. Matches Three.js's default `LessEqualCompare` semantics (`shadow.map.depthTexture.compareFunction = LessEqualCompare` → sampler2DShadow returns 1.0 when `ref <= sample_depth`; `step(ref, depth)` returns 1.0 when `ref <= depth`).

Also wraps `gl.texParameteri` to force `TEXTURE_COMPARE_MODE=NONE` on TEXTURE_2D targets — sampling a depth texture as sampler2D with compare mode ON is undefined behavior per ES 3.0 spec; forcing NONE guarantees raw depth-value reads.

**CRITICAL LESSON re-learned from cut #7 (cube-route-shim's cubeUVSample helper).** The helper MUST have explicit `highp` qualifiers on return type, sampler2D param, vec3 param, AND local `_stored`. Mesa-Nouveau's GLSL ES 3.00 compiler is strict — the file-scope `precision highp float;` in Three.js's prelude does NOT propagate to function signatures. First-round attempt without them produced silent shader-compile failures → EVERY material with shadows fell back to nothing rendered → canvas completely black (not just missing spotlight, TOTAL black including non-shadow geometry). Recurrence tell: if a future runtime-shim shader-rewrite adds a helper that returns black-screen behavior on Mesa-Nouveau, check for missing `highp` on the helper decl.

**Symptom it fixes.** webgl-lights-spotlight (v2) — spotlight cone renders correctly on floor, TorusKnot casts proper PCF shadow into the cookie-lit pool. Also fixes any future v2 demo using DirectionalLight/SpotLight with `castShadow=true` + PCF shadow map.

**DEFERRED.** `samplerCubeShadow` (point light shadows). No current v2 demo exercises point light shadows. Parallel rewrite would follow the same shape — collect `samplerCubeShadow IDENT`, rewrite type + calls, inject helper that samples the cube face + does step compare. Requires coordination with cube-route-shim's samplerCube→sampler2D + atlas layer (or its own atlas of some kind). Estimated ~100 additional lines.

**DISPOSITION:** `brewser-specific` (runtime-side). Mesa-Nouveau driver-limit workaround; upstream Three.js would not accept a rewrite that hides sampler2DShadow.

**UPSTREAM STATUS:** `n/a`.

**RE-APPLY / VERIFY NOTE.** If a brewser-runtime-v8 upstream pull loses this shim, webgl-lights-spotlight (or any v2 shadow-map demo) regresses to invisible spotlight / ambient-only lighting. Recurrence tell: `[shadow-shim:install]` line missing from `nxjs-debug.log` after context creation. Re-apply by restoring `shadow-route-shim.ts` from git history and re-adding the `installShadowRouting(gl)` calls in canvas-runner.ts after both `installCubeRouting(gl)` sites.

---

## #22 — Switch.VideoDecoder minimal V8 port (Phase 2.G.1 cut #22) — SHIPPED 2026-07-01

**File(s):**
- NEW [source/video-decoder.h](source/video-decoder.h) (~5 lines — forward decl of `nx_init_video_decoder`)
- NEW [source/video-decoder.cc](source/video-decoder.cc) (~280 lines — V8 bindings over `nx_media_*`)
- MODIFIED [source/main.cc](source/main.cc) — `NX_MODULE(video_decoder)` forward decl + `nx_init_video_decoder(iso, init_obj)` call in `build_init_object` right after `nx_init_video`
- NEW [packages/runtime/src/switch/video-decoder.ts](packages/runtime/src/switch/video-decoder.ts) (~130 lines — trimmed copy of QuickJS's TS wrapper, with pause/seek/setMuted/setVolume/getAudioLevels/getFrequencyData/getWaveform REMOVED since the V8 minimal port doesn't wire them)
- MODIFIED [packages/runtime/src/switch/index.ts](packages/runtime/src/switch/index.ts) — added `export * from './video-decoder';`
- MODIFIED [packages/runtime/src/$.ts](packages/runtime/src/$.ts) — added type declarations for the 5 `videoDecoder*` native bindings

**Root cause.** QuickJS-era engine exposed `Switch.VideoDecoder` via a ~2000-line `source/video.c` that owned its own decode threads, packet rings, and audrv voice bookkeeping. V8 fork's `source/video.cc` (348 lines) implements only the `Video` element (for drawImage integration) and skipped the VideoDecoder API entirely — the underlying `nx_media_*` portable pipeline in `source/media-decoder.cc` was ported but not wrapped as VideoDecoder. Demo webgl-materials-video throws `Switch.VideoDecoder unavailable — rebuild nxjs.nro with video support`.

**Scope.** Cut #22 implements the MINIMUM VideoDecoder surface the demo needs, layered as a thin V8 binding over the existing `nx_media_*` API (which is a strict superset of what the QuickJS pipeline provided per-embedder). Implemented: `videoDecoderNew` (synchronous open via `nx_media_open` on main thread — fine for local sdmc files), `videoDecoderPlay` (→ `nx_media_play`), `videoDecoderClose` (→ `nx_media_destroy`), `videoDecoderNextFrame` (→ `nx_media_present` + BGRA→RGBA swizzle-copy into a fresh ArrayBuffer + wrap as `{data, width, height, pts, ended}` object), `videoDecoderInit` (installs prototype getters). Getters wired: `width`, `height`, `duration`, `error`, `ended`, `usedVideo`, `usedAudio`. Stub getters (return sane defaults, no C bindings): `paused` (false), `usedHw` (false — `nx_media` hides hw/sw decision), `muted` (false), `volume` (1.0), `audioTime` (0.0), `audioError` (null).

**BGRA→RGBA swizzle.** `nx_media_present` writes BGRA to the caller's buffer (per [source/media-decoder.h:74-77](source/media-decoder.h#L74) — "caller-owned width*height*4 BGRA buffer"). The demo consumes `frame.data` as a `THREE.DataTexture(bytes, w, h, THREE.RGBAFormat, THREE.UnsignedByteType)` — expects RGBA byte order. Per-frame copy loop swaps `dst[i]=src[i+2]; dst[i+2]=src[i]` for the R↔B swap; G and A stay. Cost: ~500 KB/s at Sintel's ~480×204 @ 24 fps — trivial.

**DEFERRED (not in cut #22, add when a future demo needs them).**
- `pause()` / `seek()` — `nx_media_pause` / `nx_media_seek` exist in media-decoder.h, just need bindings.
- `setMuted()` / `setVolume()` / `getAudioLevels()` / `getFrequencyData()` / `getWaveform()` — audio-graph attach + audrv played-sample-count bookkeeping. Significant — the QuickJS impl carries ~600 lines for the visualizer surface (audrv played-sample counter, wave-buffer state, per-band RMS accumulation). Only useful once an audio path lands.
- `usedHw` proper reading — requires `nx_media_used_hw()` accessor in media-decoder.h (currently doesn't exist; the hw/sw decision is buried in the decode thread state).
- `muted` / `volume` proper — requires audio-graph attach path first.
- `audioTime` proper — requires `nx_media_audio_time()` accessor.

**hwAccel option.** Cut #22 accepts `opts.hwAccel` but silently ignores it — `nx_media_open` internally attempts hw first and falls back to sw without exposing which was used. Demo's `usedHw` stub returns false, matching the "software" readback the demo shows. If a future demo needs the actual hw/sw choice, add `nx_media_used_hw()` to media-decoder.h and wire the getter properly.

**Symptom it fixes.** webgl-materials-video (v2) — "Switch.VideoDecoder unavailable" error gone; Sintel trailer plays across the 14×7 cube mosaic; nextFrame() returns fresh frames at the container's frame rate.

**DISPOSITION:** `upstream-candidate`. Switch.VideoDecoder is a public nx.js API surface (was in QuickJS-era 1.0.0-beta.5 releases); the V8 fork should ship it for API parity. The ~280-line V8 binding over the existing portable `nx_media_*` pipeline is a clean upstream addition — no dependencies on brewser-specific code.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.**

*To verify the port is still present* after any V8-engine upstream pull: grep `source/video-decoder.cc` for `nx_init_video_decoder`. If the file is missing, upstream didn't accept our port yet AND lost our fork addition — re-apply.

*Recurrence tell.* Demo throws `Switch.VideoDecoder unavailable — rebuild nxjs.nro with video support` from [webgl-materials-video/assets/main.js:89](../brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/webgl-materials-video/assets/main.js#L89). The `typeof Switch.VideoDecoder !== 'function'` guard proves the class isn't exposed by the runtime.

*Format contract.* `nx_media_present` writes BGRA; if a future demo's Three.js path uses `THREE.BGRAFormat` (unlikely — Three.js doesn't have that), the R↔B swizzle in `nx_video_decoder_next_frame` should be dropped. Otherwise keep the swizzle — most GPU consumers (WebGL RGBA texture uploads, canvas ImageData) expect RGBA.

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
