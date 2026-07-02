# PR-D — Skia/WebGL shared-context coexistence primitive

**Branch:** `upstream-pr/D-skia-webgl-coexistence`
**Base:** `upstream/main` (`34d2d03`)
**Local worktree:** `D:/tmp/pr-drafts/PR-D`
**Local commit:** `40fc1e9` — "engine: Skia/WebGL shared-context coexistence primitive"
**Retires ledger entries:** #5, #6, #7, #14, #15 (primary), #17, #35, #36 (state-contract discipline that travels with the primitive)

## Draft status

This is a **draft-only** primitive extraction, not a merge-ready
PR. The commit contains the primitive as it exists on the
downstream v8-migration branch with fork-specific runtime hooks
stripped (see "Genericization already performed" below). Further
work is expected before opening the upstream PR — see
"Remaining work before opening" below.

**No push to any remote. No PR opened.**

## What's in the commit

Files touched:

```
 packages/runtime/src/$.ts                          |   50 +-
 packages/runtime/src/canvas/webgl-rendering-context.ts  |  611 +++
 packages/runtime/src/canvas/webgl2-rendering-context.ts |   47 +-
 packages/runtime/src/screen.ts                     |   63 +-
 source/config.cc                                   |   63 +
 source/config.h                                    |   41 +
 source/skia_gpu.cc                                 |  100 +-
 source/skia_gpu.h                                  |   58 +
 source/webgl.cc                                    | 4653 +++++++++-----------
 source/webgl.h                                     |   10 +
 source/webgl_bridge.cc                             | 1020 +++++
 source/webgl_bridge.h                              |  280 ++
 12 files changed, 4447 insertions(+), 2549 deletions(-)
```

Core pieces:

1. **skia_gpu ES3 + accessors (retires #5).** Skia's EGL context
   creation moves from CLIENT_VERSION=2 to CLIENT_VERSION=3 with
   RENDERABLE_TYPE=ES3_BIT. Four public accessors expose the shared
   EGL display/surface/context + the `GrDirectContext*` so callers
   that need to talk to the same context can. Ganesh-GL is happy
   on ES3 (superset of ES2), and having ES3 means the WebGL side
   gets ES3 core entry points (glDrawArraysInstanced etc.) via the
   same context.

2. **webgl_bridge (retires #6).** New `source/webgl_bridge.{h,cc}`
   (~1300 LOC combined). Three surfaces:
   - **GL state save/restore contract** (`nx_gl_state_snap_t` + two
     functions). ~20 fields empirically validated across a
     4013-frame Phase-0 spike + subsequent hardware runs. Frozen
     contract — extension has to be justified per field. Callers
     bracket every WebGL section as:
     ```
     nx_gl_state_save(&s);
     /* WebGL calls */
     nx_gl_state_restore(&s);
     nx_skia_gpu_gr_context()->resetContext();
     ```
   - **Tenant offscreen FBO lifecycle** — allocates GL_RGBA8 color
     tex + GL_DEPTH_COMPONENT24 depth on the shared context, wraps
     the color tex as an SkImage via `SkImages::BorrowTextureFrom`
     so Skia can composite it back into its canvas surface without
     copying.
   - **Composite path** — draws the SkImage back into Skia's
     persistent canvas surface via `drawImageRect`.

3. **WebGL1 method surface (retires #7).** `screen.getContext('webgl')`
   returns a real WebGL1 context alongside upstream's existing
   `screen.getContext('webgl2')`. ~95 methods implemented as thin
   glXxx wrappers into the tenant FBO. Each method calls the shared
   `enter_bracket()` (state save + FBO bind) prelude; the frame's
   present hook calls `exit_bracket()` (restore + resetContext + Skia
   composite).

4. **WebGL2 factory (retires #14, #15).** `screen.getContext('webgl2')`
   is wired to a separate `install_methods_v2` with its own FUNCS[]
   table. The v1/v2 split (rather than a single table + is_v2 flag)
   is a deliberate JIT-safety discipline — see PR-F for the parallel
   TS-side pattern lesson (the two changes reinforce each other).

5. **State-contract extensions travel with the primitive** (retires
   #17, #35, #36). The `nx_gl_state_snap_t` includes
   `sampler_unit0` + `read_fbo` (hardware-probe-driven, per the
   `#16-ACTIVE` probe verdict), `depth_mask` + `stencil_mask` (cut
   #15 — Three.js's WebGLState cache assumes depth_mask=TRUE, Skia
   leaves at FALSE), plus the per-call shadow-tracked `user_snap`
   in `WebGLState` (cut #14 — restores user-intended persistent GL
   bindings across bracket close/reopen cycles). See downstream
   NXJS_PATCHES_NEEDED.md #17/#35/#36 for the individual
   evidence-based rationale.

## PR title (suggested)

`engine: Skia/WebGL shared-context coexistence primitive`

## PR body (suggested)

nx.js today gives Skia and the WebGL2 context each their own EGL
chain on the default window and treats them as mutually exclusive
(the WebGL2 backend acquires the window, Skia doesn't run
concurrently). This PR replaces that with a shared-context model:
one EGL/GLES3 context hosts both Ganesh-GL (Skia) and the
WebGL(2) renderer, with a small state save/restore contract
bracketing each WebGL section.

**Why.** Any nx.js app that wants to render 2D UI over a WebGL
scene — an HTML shell around a WebGL demo, a live sensor overlay
on a 3D scene, a debug HUD — needs 2D and 3D on the same
framebuffer. Under the current mutually-exclusive design, that's
impossible without ping-ponging the GL context, which is expensive
and glitch-prone. The shared-context primitive lets both draw into
the same backbuffer via one clean handoff per frame.

**The save/restore contract is the load-bearing invariant.** The
~20-field `nx_gl_state_snap_t` was empirically validated across a
4013-frame headless spike + subsequent hardware runs. Skipping any
field or omitting the trailing `GrDirectContext::resetContext()`
call = intermittent Skia corruption on the next frame. See
`source/webgl_bridge.h` for the full field list and per-field
rationale.

**Validation.** Verified across a curated 13-demo Three.js r184
WebGL2 suite (cubemaps, shadow maps, UBOs, MRT, GPGPU, video
textures) on Tegra X1 hardware with V8 JIT enabled (Ignition +
Sparkplug + Maglev + TurboFan tier stack, default JIT-on). This is
**NOT** a claim of full Three.js compatibility or full WebGL2
conformance — the suite is a curated set of ~13 demos that
exercise the primitive's contract across representative code
paths, not a systematic conformance run.

## Genericization already performed on this branch

- Removed fork-specific `enableGpuBridgePrototype` and
  `setBridgeAutoFlush` engine methods (both were no-op back-compat
  hooks for a specific downstream runtime; upstream doesn't need
  them). Their FUNCS[] entries in both v1 and v2 method tables
  removed; their FN bodies removed.
- Removed the corresponding TS type declarations from
  `WebGLRenderingContext` interface.
- Kept `copyBridgeToCanvas` — that IS a general capability
  (per-region composite of the bridge FBO onto Skia's canvas
  surface), useful for any embedder driving overlay-style paint
  layouts.

## Remaining work before opening the PR

Non-trivial items the drafter (this session) did not complete:

1. **Comment sanitation.** ~10 comments in the primitive files
   still reference "brewser-runtime canvas-runner.ts" or similar
   downstream code by name. Each should be re-worded to describe
   the API's general purpose without naming a specific consumer.
   `grep -rn brewser source/webgl.cc source/webgl_bridge.{cc,h}
   source/skia_gpu.{cc,h} packages/runtime/src/screen.ts` finds
   them.

2. **Standalone build check.** Downstream builds against libnx (a
   Switch/homebrew SDK) as part of the same tree. The primitive
   itself does NOT depend on libnx — only EGL / GLES3 / Skia /
   V8 — but the surrounding engine wiring in `source/main.cc` uses
   libnx elsewhere. Upstream's build recipe likely already handles
   the libnx dependency (upstream's nx.js targets Switch too); the
   PR should be verified to compile cleanly on upstream's build
   config. If upstream builds a subset without libnx (e.g. for
   host testing), the primitive should NOT force libnx presence.
   Recommend a feature-flag `NX_WEBGL_BRIDGE_ENABLED` for
   embedders that don't need the coexistence path — default ON
   for existing WebGL2-using downstreams, gate any bridge code
   inside `#ifdef NX_WEBGL_BRIDGE_ENABLED`.

3. **`source/main.cc` wiring.** The bridge init/exit hooks in
   `source/main.cc` were NOT included in this commit (touching
   `main.cc` risks conflicting with upstream's own recent
   `main.cc` changes and would need per-line review). Downstream
   pattern:
   ```c
   // After nx_skia_gpu_screen_init + nx_canvas_set_gpu_surface:
   nx_webgl_bridge_init(1280, 720);

   // In the GPU present branch, before nx_skia_gpu_present:
   nx_webgl_compose_if_active(nx_skia_gpu_canvas_surface());

   // At each screen_exit site, FIRST:
   nx_webgl_bridge_exit();
   ```
   Upstream reviewers should be alerted this wiring must be added.

4. **Diagnostic gates.** Downstream ships `webgl_test_fbo` and
   `webgl_state_probe` config flags (defaults off) that enable
   the Phase-2.B smoke-test compose path and the Phase-2.G.0
   passive state-contract probe respectively (see
   `source/config.{cc,h}`). Both are diagnostic and could be
   omitted from the upstream PR or gated on
   `NX_WEBGL_BRIDGE_DEBUG`. Left in the branch for reviewer
   awareness; recommend upstream strips them or gates them.

5. **Fork-specific include(s).** `source/webgl.cc` may still
   reference `<switch.h>` or other libnx headers in places
   irrelevant to the bridge. Standalone build verification would
   reveal these. If upstream's main line already handles them,
   nothing to do.

## Compilation status of what's in the branch

Not built end-to-end from this session because the coexistence
primitive shares source files with downstream's engine tree and a
full build requires the surrounding devkitPro/libnx toolchain and
a driver-target selection. The primitive itself has no
dependencies beyond EGL / GLES3 / Skia / V8 — all of which
upstream nx.js already links. The following categorization is
based on file-level inspection, not a build attempt:

| Layer | Compiles standalone against upstream? | Notes |
|---|---|---|
| `source/skia_gpu.{cc,h}` | Yes | EGL + Skia dependencies unchanged |
| `source/webgl_bridge.{cc,h}` | Yes | Uses EGL / GLES3 / Skia only |
| `source/webgl.cc` bridge integration | Yes | Uses V8 + GLES3 |
| `source/webgl.cc` `w_copy_bridge_to_canvas` | Yes | Uses Skia + V8 |
| `source/config.{cc,h}` | Yes | Standalone INI parser + a couple bool fields |
| `packages/runtime/src/canvas/webgl-rendering-context.ts` | Yes | Pure TS, uses `$.webglContextNew` / `$.webglInitClass` — both symbols added in `packages/runtime/src/$.ts` on this branch |
| `packages/runtime/src/canvas/webgl2-rendering-context.ts` | Yes | Wraps `$.webgl2ContextNew` / `$.webgl2InitClass` |
| `packages/runtime/src/screen.ts` | Yes | Routes `'webgl'`/`'webgl2'` to the two factory functions |
| **`source/main.cc` wiring** | **NOT included** | See remaining-work item 3 |

## Interaction with other PRs

- **PR-A** (Image/Audio/Video fetch deferral): independent.
- **PR-C** (canvas.cc font-size pin): independent — PR-C's
  `source/canvas.cc` diff is orthogonal to PR-D's `source/`
  changes.
- **PR-F** (JIT-safe defineProperties): **recommended merge order
  is F → D**. The v1 constants install path in
  `packages/runtime/src/canvas/webgl-rendering-context.ts` uses
  the bulk `Object.defineProperties(target, descs)` pattern; if
  PR-F lands first, PR-D's v1 install is consistent with the
  patched v2 install. If PR-D lands first, upstream reviewers
  should see the primitive already using the JIT-safe pattern and
  PR-F becomes trivial (single file, one existing loop location
  to swap).
- **PR-E** (Switch.VideoDecoder restoration, future): independent —
  video decoder is a separate surface from the primitive.
- **PR-B** (WebGL1 v1 constants + EXT_sRGB/HalfFloat translate,
  future): DEPENDS on PR-D. PR-B's constants live in the v1
  context surface that PR-D introduces; PR-B's `source/webgl.cc`
  translate helpers live in the WebGL surface that PR-D
  introduces. PR-B must merge after PR-D.

## Downstream implication (for the fork's records)

On merge of PR-D:
- Retires ledger entries #5, #6, #7, #14, #15, #17, #35, #36.
- Reduces modified-upstream-file count by ~8 (source/skia_gpu.{cc,h},
  source/webgl.cc, source/webgl.h, source/config.{cc,h}, and the
  four TS files touched). Only `source/main.cc` remains modified
  in the fork post-merge (for the small wiring diff that PR-D
  deliberately did not include).
- Sets up PR-B to be feasible upstream (currently PR-B can't
  land because upstream doesn't expose a WebGL1 context at all).

## Explicit non-claims

- **Not a WebGL2 conformance claim.** The 13-demo Tegra hardware
  suite exercises a specific subset of paths.
- **Not a Three.js compatibility claim.** Three.js r184 was the
  tested vintage; older/newer versions may exercise paths not
  covered.
- **Not a full-suite Skia+WebGL interop claim.** Ganesh-GL's
  behavior across other Skia versions is not exhaustively tested.
- **Not a claim that the primitive is minimal.** ~1300 LOC of
  webgl_bridge + a substantial webgl.cc surface; upstream may want
  to slice this into smaller PRs (e.g. ES3+accessors alone, then
  webgl_bridge, then v1 surface). If so, the ordering that makes
  each PR reviewable in isolation is: (1) skia_gpu ES3+accessors,
  (2) webgl_bridge + state contract, (3) v1 context surface, (4)
  v2 factory split. Each earlier slice unblocks the next.
