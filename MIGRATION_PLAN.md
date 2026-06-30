# Brewser → nx.js v1 (V8+libuv+Skia) Migration Plan

## North star
Stop carrying a 750KB QuickJS-era fork of nx.js. Reduce nxjs-source to a THIN
extension patch on top of upstream V8 nx.js: a small native GL/EGL bridge + a few
main.cc init lines. Move WebGL *semantics* into brewser-runtime as engine-agnostic TS.
Track upstream by pulling, not by maintaining a divergent monolith.

## Proven foundations (do not re-litigate)
- FBO coexistence: GO. Single shared ES3 context; gl_state_save → webgl → restore →
  GrDirectContext::resetContext(); overlays via SkImages::BorrowTextureFrom.
  Measured 0.71% of 60Hz budget over 4,013 frames on Citron. Recipe is load-bearing.
- Skia is pinned to switch-skia 149-3 BECAUSE the recipe depends on Skia not assuming
  it owns persistent GL state. Do not bump without re-validating coexistence.
- Per-subsystem calls: webgl = C/hybrid (native bridge + TS semantics);
  video/audio/image = use upstream as-is (verify hwaccel/AV-sync later).
- Upstream has NO WebGL1 and NO WebGL extensions; both are ours to add (Step 2+).
- Module list is hardcoded in source/main.cc (no plugin hook) — extensions register
  via added nx_init_* lines there. "Zero fork" is impossible; thin patch is the goal.

## Repo / branch topology
- nxjs-source @ nxjs-extended  → QuickJS reference, READ-ONLY during migration
- nxjs-source-v8 @ v8-migration (from upstream v1.0.0-beta.5) → native work
- brewser-runtime-v8 @ v8-migration → TS shell + (later) WebGL semantics
- brewser @ main → consumes the built NRO via Makefile overlay; untouched until cutover

## STEP 1 — Brewser UI on V8 (WebGL null-stubbed)  ← CURRENT
Goal: prove the PLATFORM works on V8 before any WebGL migration. Shell renders via
Skia (Canvas 2D); WebGL returns null and does nothing.

Gates (all must pass):
- [x] 1.1 Worktrees created; nxjs-extended + both mains untouched & clean
          (nxjs-source-v8 @ v8-migration from v1.0.0-beta.5; brewser-runtime-v8 @
          v8-migration from main; brewser-v8 @ v8-migration from main — added in
          Step 1 to host the BREWSER_RUNTIME_DIR env-var seam in sync-runtime.mjs
          without touching brewser/main.)
- [x] 1.2 Stock upstream beta.5 NRO builds in nxjs-source-v8 — nxjs.nro = 56,513,145 B
- [x] 1.3 WebGL stub module: webgl.cc replaced with thin no-op preserving
          $.webglContextNew/$.webglInitClass and nx_webgl_active/present/exit
          symbols; no EGL/GLES includes; main.cc init line unchanged (already
          present from upstream). NRO = 56,349,305 B (−163,840 vs stock).
- [x] 1.4 brewser-runtime-v8 typecheck PASS against linked V8 @nx.js/runtime
          1.0.0-beta.5; tsc emits dist/, largest single-file = live-overlay.js 254KB
          (well under V8's 2^29 string limit).
- [x] 1.5 QuickJS→V8 shell deltas: brewser-runtime grep finds ZERO Switch.version
          reads (cairo/pixman/quickjs/wasm3/v8/skia all comments only); webgl-shim
          already null-tolerant via existing diagnostics path; no polyfill/libuv
          deltas surfaced by typecheck. No code changes required.
- [x] 1.6 brewser.nro built via brewser-v8 → brewser-v8/brewser.nro = 66,763,898 B
          (V8+Skia+libuv embedded via --fat).
- [x] 1.7 GO-CRITERIA PASS on REAL CFW Switch HARDWARE: Brewser shell boots,
          UI renders, sustained session (tick=231+ across 4 s of captured render
          loop, [download-modal]/[updates-modal] wired, system CA bundle loaded).
          Few-seconds cold-load is normal V8 init.
          Open follow-up (NOT a Step-1 blocker): images don't display — fresh delta
          tracked under [[v8-step1-images-not-showing]]; defer to a focused round
          since it's a discrete bug, not an engine-integration question. Citron
          showed PHANTOM faults (Defect A: Sparkplug-OSR transition; Defect B:
          JIT-frame stack overflow) that DO NOT occur on hardware — see TESTING
          DISCIPLINE section below. Mitigated SINGLE-NRO via the auto-detector
          documented below — Citron now runs jitless and the shell stays alive on
          the emulator too (no phantom fault, slower than hardware but usable for
          iteration on non-JIT-heavy paths).

## What stays vs what was reverted (Step 1 outcome)

- KEPT (permanent engine changes):
  - SetStackLimit fix in nxjs-source-v8/source/main.cc (~1791-1817), deriving
    limit from threadGetSelf()->stack_mem + 256 KiB headroom. Pre-fix bug was
    real — V8's auto-detect StackGuard assumed a desktop-sized stack and never
    threw the catchable RangeError on the Switch's 1 MiB libnx main-thread
    stack. Load-bearing on hardware as well as emulator.
  - WebGL null stub in nxjs-source-v8/source/webgl.cc (the seam Step 2 grows
    into).
  - Extended_Pictographic codepoint range table in
    brewser-runtime-v8/src/scripts/emoji-atlas.ts. switch-v8 ships without ICU,
    so /\p{Extended_Pictographic}/u throws at parse-time. Real, hardware-affecting.
- REVERTED (Citron-only diagnostics that turned out unnecessary):
  - --regexp-interpret-all V8 flag (was Round-3 Citron-chase for a phantom
    regex-JIT fault that doesn't reproduce on hardware). Removing it restores
    perf for any path that uses regex JIT (CSS selectors, URL parsing).
  - Diagnostic disables of probeNetwork() + boot splash in
    brewser-v8/src/browser-shell.ts (Round-7 bisection; both were exonerated
    on Citron when the fault didn't change signature). Both restored.

## TESTING DISCIPLINE (learned in Step 1)

Citron is NOT authoritative for JIT-execute paths. It mis-emulates Sparkplug-OSR
/ freshly-emitted-JIT execution and produces phantom faults (Defect A: OSR
transition fault; Defect B: JIT-frame stack overflow) that DO NOT occur on
real Switch hardware. Citron is acceptable for non-JIT-heavy iteration, but any
path that exercises JIT tier-up — which includes ALL of Step 2's
WebGL-semantics-in-TS render loops — MUST be validated on real CFW hardware.
Hardware is the source of truth. Do not re-chase a Citron-only JIT fault as if
it were an engine bug.

Concrete from Step 1: breakout.nro built against our engine faulted ~7s into its
render loop on Citron with a function-prologue stp stack overflow. The same NRO
ran for MINUTES on real CFW Switch hardware with full JIT (Ignition + Sparkplug
+ Maglev + TurboFan), no fault. That refuted three rounds of investigation
treating the Citron faults as engine defects.

## SINGLE-NRO AUTO-DETECT (Step 1 tooling)

Resolution to the Citron-vs-hardware problem: a single NRO that detects at
boot which environment it's on and forces `--jitless` on Citron only. Replaces
the prior idea of shipping two NROs (brewser_jit / brewser_jitless) — abandoned
as a downgrade (operator can sideload the wrong one and silently run jitless on
hardware, which is the exact failure mode we want to avoid).

Detection signals (independent, structural; each robust for a different
reason — see [source/detect.cc](source/detect.cc) for the implementation +
inline rationale):
- **A — hbloader env-block absent.** `envIsNso() && loader_info_size==0 &&
  !envHasHeapOverride() && !envHasArgv()`. Citron's NRO loader doesn't
  populate the env block; hbloader does.
- **B — HOS core-3 reservation not enforced.** `svcGetInfo(CoreMask) & 0x8`.
  HOS reserves core 3 for system processes; Citron grants all four cores.
- **C — Secure-monitor config queries fail.** `splGetConfig(HardwareType)`
  returns non-zero on Citron (no SMC emulation); succeeds on hardware.

Vote: 2-of-3 = Citron. Score==2 emits a WARN log line naming the disagreed
signal so a drifted vector (Citron update closing a gap, hardware quirk) is
visible early. **Hardware-biased**: when in doubt, return false → full JIT.
Mis-flagging hardware-as-Citron is a silent recoverable perf cost;
mis-flagging Citron-as-hardware brings back unattributable phantom faults.
This bias direction is load-bearing — do not "fix" it.

Override precedence (highest wins):
1. `[v8] jit = on | off` in nxjs.ini → forces JIT mode; skips target detection
   entirely (the user has taken explicit responsibility for JIT).
2. `[v8] target = hardware | citron` in nxjs.ini → skips auto-detect. `target =
   hardware` lets the regime decide; `target = citron` forces `--jitless`.
3. Auto-detect (default; `[v8] target = auto` or unset).

Where the override file lives: `sdmc:/switch/nxjs-override.ini` is loaded LAST
(on top of the app-bundled `nxjs.ini` if any), so a single SD file can flip
the target setting for any fat NRO without rebuilding. Same path works on
both Citron and hardware (argv-based overrides don't — Citron doesn't populate
argv). Bundled-or-absent nxjs.ini path semantics are unchanged.

Vectors explicitly NOT used:
- `hos.is_atmosphere` — couples to CFW choice (a hardware user on ReiNX would
  false-positive Citron under this signal).
- `setsys.mii_author_id == "yuzu Default UID"` or `serial_number` prefix
  `YUZ` — spoofable identity strings; trivial to change in a Citron update.
- Firmware version / build hash — depends on the user updating either side.

Log line shape (greppable, single source of decision auditing):
```
[detect] target=citron (auto: A=1 B=1 C=1 score=3/3) -> mode=jitless
[detect] target=hardware (auto: A=0 B=0 C=0 score=0/3) -> mode=jit
[detect] target=hardware (override=hardware) -> mode=jit
[detect] target=citron (override=citron) -> mode=jitless
[detect] WARN auto score=2/3 (signal X disagreed) ...
```

## QUICKJS-ERA ENGINE PATCHES TO RE-APPLY ON V8

Tracked separately in **[NXJS_PATCHES_NEEDED.md](NXJS_PATCHES_NEEDED.md)** —
the single source of truth for re-application at upstream-pull time.

This file deliberately does NOT duplicate the list. Add new entries
directly to NXJS_PATCHES_NEEDED.md, following its DISPOSITION POLICY
header (prefer brewser-runtime fixes over engine edits; for engine fixes,
prefer upstreaming over fork-only edits). The list is expected to grow
during Step 2 as the WebGL-semantics-in-TS work surfaces more
runtime-calls-missing-engine-surface bugs.

Seeded entries as of Step 1 close:
- **#1** image.ts → call-time globalThis.fetch deferral (FIXED)
- **#2** audio.ts → same (FIXED)
- **#3** video.ts → same (FIXED, caught in the audit grep)
- **#4** Screen.setCursorOverlay / setAnimatedCursorOverlay (NOT YET FIXED;
  disposition likely brewser-specific — change the caller in
  page-mouse-forwarder.ts rather than add an engine binding)

## STEP 2 — WebGL on V8

PREREQUISITE: Step 2 render-path validation requires hardware checkpoints.
Do NOT gate Step 2 acceptance on Citron for JIT-exercising code (per the
TESTING DISCIPLINE section above). Citron is fine for non-JIT-heavy iteration
(builds, smoke tests of init paths) but every render-loop / tier-up-exercising
checkpoint must run on real CFW Switch hardware before it's considered
"passing."

### Architectural delta from upstream

Upstream v1.0.0-beta.5's webgl.cc (commit fb0468f, ~2,863 lines, 161 WebGL2
methods registered via a single method table, zero extensions, no WebGL1)
treats the screen NWindow as **WebGL-or-Skia exclusive** — webgl.cc creates
its own `EGLDisplay`/`EGLSurface`/`EGLContext` against `EGL_DEFAULT_DISPLAY`
on the same NWindow that `skia_gpu.cc` claims, and webgl.h states this
exclusivity plainly ("Mutually exclusive with the Canvas 2D screen paths").
That model is **incompatible** with the Phase 0 GO recipe (single shared
ES3 context, gl_state_save → WebGL → restore → grCtx->resetContext, overlays
via SkImages::BorrowTextureFrom — proven at 0.71% of 60 Hz over 4,013 frames).

Step 2's structural change: WebGL stops owning EGL, joins Skia's context, and
renders to an offscreen FBO that Skia composites. The fork's webgl_egl.c
(11,130 lines) carries the necessary shared-context plumbing + Tegra/Mesa
quirks and is the migration source; **the fork is the reference, upstream
webgl.cc is the bone structure to rebuild around.** The fork's webgl.c
(18,244 lines, conformance-stratum semantics + 40+ extensions) is mostly the
TS-side target — its responsibilities move to brewser-runtime over Step 2's
later phases.

Step 1 left behind a null stub at source/webgl.cc that preserves the symbol
seam (`nx_init_webgl`/`nx_webgl_active`/`nx_webgl_present`/`nx_webgl_exit`
plus the `$.webglContextNew`/`$.webglInitClass` JS init exports). Step 2
grows back into this stub; do NOT delete-and-reintroduce upstream's webgl.cc
or main.cc init lines — the stub IS the patch surface.

### Phase structure — gated, hardware-checkpointed

Each phase has a single GO criterion that has to PASS before the next phase
starts. Phases are deliberately small so a regression is bisectable.

#### Phase 2.A — Shared-context EGL ownership  [x] SHIPPED + Citron smoke GREEN 2026-06-28

Take EGL/GLES context creation OFF webgl.cc. The skia_gpu.cc init path
creates the one EGL context the whole engine uses; webgl.cc becomes a
*tenant* that asks skia_gpu for `eglMakeCurrent` (no-op when already
current — same context, same surface).

Concrete changes (2.A landed):
- `skia_gpu.cc` `EGL_CONTEXT_CLIENT_VERSION` bumped `2 → 3`. Config attrs
  grow `EGL_RENDERABLE_TYPE = EGL_OPENGL_ES3_BIT (0x0040)` — required, or
  eglChooseConfig may hand back an ES2-only config and eglCreateContext(ES3)
  then fails silently. `#include <GLES2/gl2.h>` → `#include <GLES3/gl3.h>`.
- One-shot `[skia] GL version=... vendor=... renderer=...` log line after
  `eglMakeCurrent` so a silent ES2 fallback is auditable. (If `GL version`
  starts with `2.` not `3.` something is wrong; if `Mesa` doesn't appear in
  the version string, the driver layer is wrong.)
- `skia_gpu.h` grows `#include <EGL/egl.h>` + `class GrDirectContext;` + four
  accessors:
  - `EGLDisplay nx_skia_gpu_egl_display(void);`
  - `EGLSurface nx_skia_gpu_egl_surface(void);`
  - `EGLContext nx_skia_gpu_egl_context(void);`
  - `GrDirectContext *nx_skia_gpu_gr_context(void);` (raw pointer, lifetime
    owned by skia_gpu — bridge must not addref or destroy)
- WebGL stub UNCHANGED — `webgl.cc` still returns null from `getContext`.
  2.A only exposes the accessors; 2.B grows the bridge into the stub.
- Catalogued as **NXJS_PATCHES_NEEDED.md #5** with disposition
  `upstream-candidate` (general capability: any embedder benefits from
  Skia exposing its EGL/GrContext handles so a tenant GLES renderer can
  share the context).

Skia 149-3 no-persistent-GL-state assumption preserved — 2.A does not
mutate Skia's GL-state expectations; it only widens the context version
(ES3 is a strict superset of ES2 — Ganesh-GL only calls ES2 entry points
+ GL_RGBA8 which is fine in both regimes) and exposes read-only handles.

**Citron smoke gate (2.A):** Citron is acceptable for 2.A — init-path
only, no JIT-heavy render loop, no WebGL rendering. **Hardware checkpoint
deferred to end-of-2.B** (shared context + offscreen FBO + Skia composite
proven together before 2.C/WebGL1 builds on top). Do not stack 2.A → 2.C
unproven on hardware.

GO criterion (2.A): brewser shell boots on Citron; Skia continues to
render correctly (toolbar, app cards, image logos all paint); the
`[skia] GL version=` log line shows ES3 (`GL_VERSION` second token starts
with `3.`); WebGL stub still resolves `getContext('webgl'/'webgl2')` to
null.

**RESULT (2026-06-28):** GREEN on Citron. Log shows
`[skia] GL version=OpenGL ES 3.2 Mesa 20.1.0-rc3 vendor=nouveau
renderer=NV120` (Mesa Nouveau gave us ES 3.2 unconditionally), Skia
surfaces both came up, shell rendered unbroken. First boot of the
2.A NRO mis-rendered black due to a stale-vendored emoji-atlas.js
parse-time SyntaxError throw — root-caused to `BREWSER_RUNTIME_DIR`
env not propagating from Make → `npm run sync-runtime` → child
node, fixed by re-vendoring manually. Engine ES3 bump itself was
never the regression. See [[reference-brewser-v8-sync-runtime-env-loss]]
for the recurrence tell.

Build state:
- `nxjs-source-v8/nxjs.nro` = 56,353,729 B (Step 1 was 56,353,401 B;
  +328 B, expected for the small new logging + accessor symbols).
- `brewser-v8/brewser.nro` = 66,763,274 B (Step 1 was 66,763,898 B;
  -624 B, within compression-variation noise).
- All Step 1 markers intact in the build artifacts (verified by `strings`:
  3 `[detect]` markers, `SetStackLimit` derivation at main.cc:1886-1890,
  Extended_Pictographic table in emoji-atlas.ts, image/audio/video
  fetch deferrals).
- All four new accessor symbols exported from the engine ELF.

Accessor surface that 2.B will attach to (locked in by this round):

| Accessor                          | Purpose for 2.B                                   |
|-----------------------------------|---------------------------------------------------|
| `nx_skia_gpu_egl_display()`       | `eglMakeCurrent(display, ...)` if needed          |
| `nx_skia_gpu_egl_surface()`       | draw/read surface arg for `eglMakeCurrent`        |
| `nx_skia_gpu_egl_context()`       | `eglGetCurrentContext()` equality check / restore |
| `nx_skia_gpu_gr_context()`        | `grCtx->resetContext()` at WebGL→Skia boundary    |

#### Phase 2.B — Bridge state-save/restore + offscreen FBO  [x] SHIPPED + HARDWARE GATE GREEN 2026-06-28

Port webgl_egl.c's `gl_state_save` / `gl_state_restore` (FBO, viewport,
program, VAO, ARRAY_BUFFER, active_tex, TEX_BINDING_2D, BLEND/DEPTH/CULL/
SCISSOR/STENCIL toggles, colormask, clear color, blend funcs) into the
new bridge file. Add a per-context offscreen color-attachment FBO + depth
attachment sized to the WebGL canvas; webgl.cc's "default framebuffer" is
THIS FBO, not the screen FBO 0. Add the Skia-side composite path that
calls `SkImages::BorrowTextureFrom(GR_GL_TEXTURE_2D, GL_RGBA8,
kBottomLeft)` + `drawImageRect` to blit the FBO into Skia each frame
where webgl has been touched.

**GO criterion (Phase 2.B):** stub still returns null to JS. Internal
self-test (compiled-in C-side smoke probe; gated out of release): clear
the offscreen FBO red, composite, see red rectangle on screen via Skia.
Skia 2D draws over the red rect render unchanged. Hardware boot still
sustains.

**RESULT (2026-06-28):** HARDWARE GATE GREEN on real CFW Switch.
1,096 frames of bracketed raw-GL with full JIT pipeline active
(Ignition + Sparkplug + Maglev + TurboFan), clean
`[webgl-bridge] exit ok` teardown, no flicker / no Skia
corruption / no FBO incomplete. Steady-state boundary cost
**112.23 µs/frame avg on hardware** (Phase 0 spike predicted
119 µs — within noise). Boundary scaled identically on Citron
(119–185 µs) and hardware (105–126 µs steady state), confirming
the recipe is target-agnostic. Bridge composite + 2D overlay + shell
rendered together cleanly. Clean exit order verified (SkImage
dropped before GL handles freed, per the catalog gotcha).

The state-save/restore contract for the bridge is now LOCKED IN at
[source/webgl_bridge.h](source/webgl_bridge.h)'s `nx_gl_state_snap_t`
+ `nx_gl_state_save`/`nx_gl_state_restore`. Phase 2.C and every
subsequent WebGL frame wraps itself in this bracket; do not
re-derive the state set.

**Accessor + primitive surface locked in for 2.C:**

| Surface (file: webgl_bridge.h)                | Role for 2.C+ WebGL bridge                                            |
|-----------------------------------------------|-----------------------------------------------------------------------|
| `struct nx_gl_state_snap_t`                   | State snapshot type. Hardware-proven set — see header for the rationale per field. |
| `nx_gl_state_save(snap *)`                    | Capture pre-WebGL GL state.                                          |
| `nx_gl_state_restore(snap *)`                 | Restore post-WebGL; caller MUST follow with `gr->resetContext()`.     |
| `nx_webgl_bridge_init(w, h)`                  | Lazy bringup of tenant FBO (caller chooses size).                    |
| `nx_webgl_bridge_exit()`                      | Drop SkImage FIRST, then GL handles. Idempotent.                     |
| `nx_webgl_bridge_is_initialized()`            | Gating predicate for compose calls.                                  |

`nx_webgl_bridge_compose_test()` is the 2.B test driver; 2.C replaces
it with the WebGL bridge proper — but the four other primitives
above are forever.

#### Phase 2.C — WebGL1 context exposed; minimal method surface  [x] SHIPPED + HARDWARE FUNCTIONAL-PASS (jitless) 2026-06-28

Port enough of upstream's webgl.cc method table + bringup to make
`screen.getContext('webgl')` (NOT `'webgl2'` yet) return a non-null
`WebGLRenderingContext`. WebGL1's enum constants differ from GLES3 at the
margins (no `UNIFORM_BUFFER`, etc.) — match WebGL1 spec, not GLES3 — and
its texture upload formats are tighter than WebGL2's. The TS side already
has a v1/v2 cache split in `webgl-shim.ts` and exposes both
`WebGLRenderingContext`/`WebGL2RenderingContext` constructors at first
context creation. The slice's bridge surface is whatever the slice demo
calls (see *First vertical slice*); upstream's 161-method table is NOT
ported wholesale yet.

**GO criterion (Phase 2.C):** `screen.getContext('webgl')` returns a
non-null object on Citron (init-path smoke test ONLY — no JIT-exercising
render loop yet).

**RESULT (2026-06-28):** **Hardware functional PASS (jitless).** Tegra
silicon runs the full 2.C bridge — engine boots, shell renders,
geometry-cube launches, `[webgl] context_new ok 1280x720`, bridge takes
ownership, demo runs sustained (1290+ ticks), bridge teardown clean.
Three.js custom GLSL shader (Monjori demo) compiles + animates at 60 fps
visibly on screen via the 2.B state save/restore + tenant FBO + Skia
compose pipeline. Mesa Nouveau driver behavior is identical to Citron,
confirming the bridge is hardware-validated.

**KNOWN ISSUE — V8 JIT regression on hardware:** the same code that runs
correctly on hardware-jitless dies silently during runtime.js evaluation
when V8 JIT (Sparkplug+Maglev+TurboFan) is enabled on Tegra. Engine
prints `[v8] max_heap=...` then terminates before reaching `[skia]` init.
NOT a bridge-execution bug (the bridge runs correctly when V8 reaches
it); a Tegra-specific V8 codegen issue introduced by 2.C's new code path
(most likely the `install_methods()` loop in source/webgl.cc creating
~190 V8 FunctionTemplate/Function objects across the v1+v2
$.webglInitClass calls, or the v1+v2 class-init `for...of Object.entries`
loops hitting a Sparkplug/Maglev tier-up edge case on aarch64).

Workaround during Step 2 development: drop `[v8] jit = off` into
`sdmc:/switch/nxjs-override.ini` on the Switch SD. Performance is lower
(Ignition-only interpreter) but functionally equivalent for visual-
correctness iteration. This is acceptable for 2.D work because V8 JIT
mode does not affect GL driver behavior — bridge state save/restore,
FBO compose, texture upload, etc. behave identically under JIT and
jitless. Catalogued as **NXJS_PATCHES_NEEDED.md #8**; investigation
deferred until 2.D's first hardware checkpoint (geometry-cube visual
correctness) ships, then we revisit before 2.E's bulk semantics work.

**Locked-in 2.C surface (committed allowlist):**
- Engine: `source/webgl.cc` grows from null stub (47 lines) into a real
  WebGL1 factory (873 lines) implementing ~95 methods. Reuses 2.B
  primitives (`nx_gl_state_save`/`restore`, tenant FBO, `SkImages`
  compose); per-frame bracket is lazy on first GL call, closed at
  `nx_webgl_compose_if_active` in main.cc's present hook.
- TS class: new file
  `packages/runtime/src/canvas/webgl-rendering-context.ts` exposes
  `WebGLRenderingContext` with ~210 GL constants + method-typing
  interface. `screen.getContext('webgl')` routes to `createWebGLContext`.
  `'webgl2'` is held null until Phase 2.G.
- Two fork-specific hooks (`enableGpuBridgePrototype`,
  `setBridgeAutoFlush`) return `true` as no-ops — required by
  brewser-runtime canvas-runner; drop in 2.E.
- 9 WebGL1 extension shims wired in `getExtension`
  (`EXT_blend_minmax`, `OES_element_index_uint`, `OES_standard_derivatives`,
  `OES_texture_float`/`_linear`, `OES_texture_half_float`/`_linear`,
  `EXT_sRGB`, `WEBGL_depth_texture`) — return objects with enum values
  numerically identical to ES3 core, so constants pass through to native
  GL transparently with no new engine GL plumbing needed.
- 4 cross-file Step-2 sibling fixes shipped in the same wave: image.ts /
  audio.ts / video.ts `src` setter switches to call-time
  `document.baseURI ?? $.entrypoint` (sibling of Step 1 #1's fetch
  deferral — relative URLs now resolve against the per-session
  brewser-runtime page URL); 2D-exclusivity check removed from the
  `'webgl'` branch in screen.ts so the brewser shell's existing 2D
  context doesn't block inline-canvas WebGL acquisition.

**Cube's current visual appearance (2.D starting diagnostic):**
- All demos render at SCREEN bottom-left, not at the inline canvas's
  CSS layout slot. Three.js calls `gl.viewport(0, 0, 640, 360)` →
  writes to GL FBO bottom-left → `kBottomLeft_GrSurfaceOrigin` SkImages
  compose lands at Skia screen bottom-left. **2.D needs the QuickJS-era
  fork's "draw FBO at canvas's CSS layout slot" routing — currently
  `enableGpuBridgePrototype(true)` is a no-op.**
- Monjori custom fragment shader works (animates at 60 fps).
- Geometry-cube starts black — likely related to ELEMENT_ARRAY_BUFFER
  binding NOT being in `nx_gl_state_snap_t`'s save set OR multiple
  texture unit bindings not all in the snap. **2.D should check the
  state-save contract first — that's the most likely culprit since
  Monjori shader (uses `drawArrays`, single texture) works fine.**
- Materials Cubemap demo: 2 of 3 heads black. Three.js cubemap path
  hits Mesa Nouveau's `samplerCube` returns-vec4(0) driver limit
  (see [[reference-mesa-nouveau-layered-sampling-unsupported]]).
  **Phase 2.F** (Tegra/Mesa quirks), not 2.C/2.D.
- Geometries demo all textures black: likely
  [[reference-brewser-v1-black-texture-demos]]: my texImage2D
  doesn't yet widen the accept-list for SRGB internalformats.
  **Phase 2.F.**

#### Phase 2.D — FIRST VERTICAL SLICE: demo renders end-to-end on hardware

The integrating phase. See *First vertical slice* below for the precise
slice contents + acceptance.

**GO criterion (Phase 2.D):** chosen demo renders on real CFW Switch
hardware, sustained ≥ 60 s, brewser shell HTML overlay visible on top, no
regressions in Step 1 paths.

#### Phase 2.E — WebGL semantics bulk-moved to brewser-runtime TS

Once the slice is GREEN, lift the fork's webgl.c-side responsibilities
into brewser-runtime TS:
- Extension surface (40+ wrappers — `EXT_sRGB`, `WEBGL_depth_texture`,
  `OES_*`, `WEBGL_compressed_texture_*`, `EXT_color_buffer_*`, etc.)
- Handle lifetime (WebGLBuffer/Program/Shader/Texture/Framebuffer/
  Renderbuffer/Sampler/Sync/VAO/TransformFeedback objects + their
  finalizer rules)
- Validation + WebGL synthetic error queue (drained-then-OR with
  `glGetError`)
- `getParameter` exposure (the parts that aren't trivially direct GL
  reads — `MAX_TEXTURE_IMAGE_UNITS` etc. ARE direct; the WebGL-specific
  parameters are not)
- UNPACK_FLIP_Y_WEBGL / UNPACK_PREMULTIPLY_ALPHA_WEBGL emulation
- The shader-name → bridge program allowlist machinery (carrying over
  the `enableGpuBridgePrototype` API surface, even if implementation is
  no-op in the new bridge for now)

**GO criterion (Phase 2.E):** the v1 webgl1threejsdemos set's
*black-texture-free* demos render on hardware (Three.js typical surface:
MeshBasicMaterial + texture + simple geometries). The known black-texture
quartet (webgl-geometries / webgl-loader-gltf / webgl-sprites /
webgl-materials-blending) is deferred to Phase 2.F.

#### Phase 2.F — Tegra/Mesa quirks migrated bridge-side

Apply the recon-table quirk fixes. Each gets a hardware checkpoint of its
specific demo. Order roughly by load-bearing-ness for the v1 demo set:

1. **EXT_sRGB texImage2D accept-list widening** + `SRGB_EXT` /
   `SRGB_ALPHA_EXT` → `SRGB8` / `SRGB8_ALPHA8` translation
   ([[reference-brewser-v1-black-texture-demos]]). Fixes the
   black-texture quartet.
2. **PMREMGGXConvolution FS replacement** + cube-face FBO aliasing
   rescue + sampler2DShadow `COMPARE_MODE` replay
   ([[reference-pmrem-tegra-compiler-workaround]],
   [[reference-mesa-cube-face-aliasing-rescue]],
   [[reference-brewser-threejs-spotlight-shadow-engine-fix]]).
3. **dfgLUT RG16F texSubImage2D accept-list** + PMREM HALF_FLOAT (0x140B)
   accept-list ([[reference-dfglut-rg16f-accept-list-fix]],
   [[reference-pmrem-halffloat-accept-list-fix]]).
4. **Mesa Nouveau layered-sampling driver-limit gating**
   ([[reference-mesa-nouveau-layered-sampling-unsupported]]) — conformance
   gating, NOT a fix; the slice acknowledges sampler3D/2DArray/Cube
   limitations and avoids the affected demos.

**GO criterion (Phase 2.F):** the full v1 webgl1threejsdemos set that
worked on the QuickJS-era fork works on V8 to the same level of fidelity
(degraded paths flagged with `[nxjs:*-fix]` markers as today).

**Status (2026-06-29):**
- F.1 — PMREM intermediate render-target path — [x] SHIPPED + Citron
  verified (`webgl-loader-gltf`: HDR equirect background + helmet IBL).
  Hardware-pending: yes (Citron is the same Mesa version, but JIT codegen
  + per-frame allocator behavior differ; folded into the consolidated
  hardware pass at Bucket F close).
- F.2a — samplerCube → sampler2D routing layer — **[x] SHIPPED + Citron
  + hardware VERIFIED** (`webgl-materials-cubemap`: 3 Walt heads under
  Royal Castle skybox + clean reflections, AND `webgl-loader-gltf`:
  helmet IBL + HDR skybox visible side-by-side after the F.1
  regression was hunted down). Runtime-side
  (`brewser-runtime-v8/src/scripts/cube-route-shim.ts`); zero engine
  fork delta (nxjs.nro IDENTICAL to F.1 close at 56,458,217 B
  throughout F.2a). See NXJS_PATCHES_NEEDED.md #12 for the full
  implementation arc — initially shipped at 5 distinct issues (route
  + shader-rewrite + getActiveUniform type-fake + OffscreenCanvas
  image-source conversion for the engine's 9-arg-only `texSubImage2D`
  gap + per-frame `resetState`), then 2 MORE surfaced when F.1's
  `webgl-loader-gltf` demo regressed to matte helmet + black skybox
  after F.2a went in: a safer `bindTexture` hook (forward CUBE_MAP +
  conditional 2D atlas bind + atlas-alloc gated to non-typed-array
  uploads — fixes Three.js's `_emptyCubeTexture` 1×6-placeholder
  atlas clobbering legit 2D bindings like the PMREM cubeUV envMap
  atlas), and a dual-declaration `envMap` gate (`envmap_common_pars`
  and `BackgroundShader` declare `samplerCube envMap` + `sampler2D
  envMap` in #ifdef branches; only fake the cube routing when the
  source contains `#define ENVMAP_TYPE_CUBE\b(?!_)` to detect that
  the cube branch is the live one — fixes hijacking PMREM-cubeUV
  envMap into a broken `setTextureCube`/`uploadCubeTexture` path).
  Both demos verified rendering correctly side-by-side on Citron +
  on hardware-jit (hardware confirmed `[detect] target=hardware
  ... -> mode=jit` + clean `[f2a:install]` + Three.js demos
  rendering with full IBL + cubemap atlas paths).
- F.2b — SRGB cube downgrade + cube-face-aliasing rescue — **[~] SKIPPED
  (N/A for the gate demo).** Both rescues target specific artifacts that
  did NOT manifest in `webgl-materials-cubemap`: the SRGB cube downgrade
  is for cube textures uploaded with sized `SRGB8_ALPHA8` internalformat
  (demo's path uses unsized `RGBA`), and the cube-face FBO aliasing
  rescue is for cube textures rendered to via FBO writes (CubeCamera /
  CubemapFromEquirect / WebGLCubeRenderTarget — demo uses static
  per-face Image uploads, no FBO writes). Re-port becomes load-bearing
  if/when a demo using those code paths surfaces; the recipes in
  [[reference-pmrem-tegra-compiler-workaround]] Bug 3 and
  [[reference-mesa-cube-face-aliasing-rescue]] are the documented
  references for that future round. Skip-if-artifact-absent per the
  F.2 spec.

**Bucket F closed (modulo hardware pass).** F.1 + F.2a together cover
PMREM intermediate-RT paths (HDR / IBL / scene.environment) and
direct user-CubeTexture paths (skybox + envmap reflection / refraction)
on Tegra Mesa-Nouveau. Remaining cube-related demos that DO use FBO
writes or HDR cube allocation will trigger the F.2b rescues if they
exhibit the respective artifacts.

#### Phase 2.G — WebGL2 + thin-patch structure consolidated

`screen.getContext('webgl2')` returns a non-null context backed by the
SAME shared GL context (WebGL2 = WebGL1 class extended; existing TS
prototype chain wiring in `WebGL2RenderingContext` already exposes the
WebGL1 surface to v2 callers). Conformance test surface (the
~480 tex-3d-* tests + 2D_ARRAY + cube tests acknowledged gated by Mesa
limit in [[reference-mesa-nouveau-layered-sampling-unsupported]]) re-runs
where applicable. Fork-delta against upstream consolidated into the
fewest-files-touched shape we can manage; entries in
NXJS_PATCHES_NEEDED.md are tightened up with the final disposition.

**GO criterion (Phase 2.G):** WebGL2 demo set (`webgl2threejsdemos`)
parity with the QuickJS-era fork on hardware. Final patch catalog
reflects only fork-only quirks and not-yet-upstreamed
engine-correctness fixes.

### First vertical slice — Phase 2.D acceptance contents

**Chosen demo:**
`apps/experimental/com.natureglass.webgl1threejsdemos/geometry-cube`
(224 lines, Three.js MeshBasicMaterial + DataTexture + spinning cube,
black `setClearColor`, antialias=false, calls `renderer.resetState()`
per-frame).

**Why this demo:**
- It's an existing brewser-apps demo that's known-good on the QuickJS-era
  fork, so a regression on V8 is unambiguous.
- Smaller WebGL surface than full Three.js IBL/PMREM demos: no IBL, no
  shadows, no PMREM, no postprocessing, no instancing. Three.js for
  MeshBasicMaterial calls roughly: `createShader/shaderSource/compileShader/
  getShaderParameter/getShaderInfoLog`, `createProgram/attachShader/
  linkProgram/getProgramParameter/getProgramInfoLog/useProgram`,
  `getAttribLocation/getUniformLocation`, `createBuffer/bindBuffer/
  bufferData`, `vertexAttribPointer/enableVertexAttribArray`,
  `createTexture/bindTexture/texImage2D/texParameteri/activeTexture`,
  `uniform1i/uniform3fv/uniformMatrix4fv`, `enable(DEPTH_TEST)/depthFunc/
  clearColor/clear/viewport`, `drawElements`, `getParameter`, `getError`.
  ~30 distinct methods (well under upstream's 161-entry table), plus
  whatever Three.js capability probing hits.
- DataTexture upload path side-steps the existing texImage2D-image-source
  limitation (still a TODO at the runtime layer; the slice doesn't have
  to solve it).
- The cube rotates → JIT tier-up DOES exercise here (per-frame requestAnimationFrame
  callback into the Three.js render loop becomes a hot path).

**Minimum bridge surface (Phase 2.C derivation):** the slice doesn't try to
predict Three.js's exact GL call set — bring up the bridge with the
shader/program/buffer/texture/uniform/draw methods listed above, run on
Citron, capture missing-method `glproxy` errors from the existing diag
Proxy in webgl-shim.ts (`__brewserGLProxyDebug = true`), iterate. Commit
the **resulting** allowlist (not a predicted one) as the
post-Phase-2.D bridge surface. Do NOT port upstream's full 161-method
table eagerly — that's Phase 2.G's job.

**Minimum TS semantics (in brewser-runtime):**
- Bridge surface returns plain JS wrappers (`{ id, kind }`) for object
  handles; the `instanceof WebGLBuffer` chain wires through to the
  prototype carriers exposed by `$.webglInitClass`.
- A synthetic-error slot for "drain before glGetError" — even MeshBasic
  exercises `getError` post-`resetState()`.
- `UNPACK_FLIP_Y_WEBGL` honored (Three.js sets/unsets it around
  DataTexture upload).
- `enableGpuBridgePrototype(true)` — silently no-op (matches v1 demo's
  call site at `webgl1demo/assets/main.js:72` and the Three.js demos that
  don't call it at all).

**Minimum Brewser overlay:** the shell's existing HTML status panel +
menu (no app-specific overlay). The cursor overlay is deferred per
NXJS_PATCHES_NEEDED.md #4 (will appear invisible during the slice; not a
slice blocker). Overlay composes ON TOP of the WebGL surface via the
Phase 2.B Skia path.

### Hardware checkpoint per slice

Per the TESTING DISCIPLINE section: Citron is acceptable for init-path
smoke (Phase 2.A through Phase 2.C GO can be Citron-only; jitless mode
already covers the Citron Sparkplug-OSR / JIT-frame-stack phantom faults).
**Phase 2.D must complete on real CFW Switch hardware.** The exact
checkpoint script:

1. Build engine + runtime, deploy to hardware via SD (or whichever path
   the user prefers; see [[reference-brewser-make-chains-nxjs]] for the
   integrated build).
2. Boot brewser → confirm shell still works (Step 1 regression check —
   shell home renders, app list visible, no boot-time JS exceptions).
3. Launch `geometry-cube` → confirm: (a) renderer.resetState() doesn't
   throw, (b) cube is visible, (c) texture is visible (UV mapping +
   colorspace conversion non-fatal, not zeroed), (d) cube rotates
   smoothly, (e) sustained ≥ 60 seconds (JIT tier-up + Maglev/TurboFan
   path exercised through the render loop; phantom-fault probes in
   [[v8-migration-phase1-hardware]] reproduce within 10 s of launch if
   present).
4. Failure-on-hardware-but-not-Citron = engine bug, investigate.
   Failure-on-Citron-but-not-hardware = phantom-fault category, leave a
   `[detect]` log line in the trace and DO NOT chase per testing
   discipline.

### Predicted dropped-patches surface (Phase 2.E onward)

The migration is likely to surface these as
`runtime-calls-missing-engine-surface` bugs (the
NXJS_PATCHES_NEEDED.md pattern):

- `enableGpuBridgePrototype` runtime hook — fork-specific; can ship as a
  no-op runtime shim or as a fork-only engine binding depending on
  whether the bridge re-grows a passthrough allowlist.
- `gl.canvas` property on the context object — Three.js reads this for
  size queries (`gl.canvas.width / .height`). Upstream wrappers do NOT
  expose it. Likely add a TS-side property in webgl-shim.ts via
  `Object.defineProperty` on the returned context, NOT an engine edit.
- `OES_vertex_array_object` / `OES_element_index_uint` / `WEBGL_depth_texture`
  extension fallbacks for v1 — Three.js's WebGLRenderer probes for
  these and downgrades silently. The slice's geometry-cube doesn't NEED
  them (Three.js falls back to indexed UNSIGNED_SHORT + no VAO), but the
  broader v1 demo set will.
- `getExtension('XYZ')` returning `null` for an extension whose enum
  constants Three.js DOES use → INVALID_ENUM at later draw → silently
  black rendering. The fork advertised these by name AND wrapped the
  enums; the V8 migration must do the same.
- Three.js's `renderer.resetState()` contract per-frame in the slice
  (already in the slice demo) — exposes whether GL state survives the
  WebGL↔Skia boundary. If it does NOT, we've got a state-restore bug.
- The `[[reference-*]]` Tegra/Mesa quirks in memory (PMREM / cube-face /
  sampler2DShadow / dfgLUT / SRGB / HALF_FLOAT) will reappear in Phase
  2.F as the v1 demo set is brought up; these are EXPECTED, not
  surprises, and the memory entries are the migration recipes.
- `Uint8Array → ArrayBuffer` coercion at `texImage2D(buffer-source)` —
  upstream's `view_bytes` helper handles both, but the WebGL1 path
  upstream LACKS may have an off-by-one with `byteOffset`. Verify.
- `console.warn/log/error/info` silencer pattern
  ([[console-error-switches-render-mode]]) — already standard in v1
  demos; only worth calling out if the V8 bridge starts logging to
  stderr from a path the engine routes back through `$.print`.

## STEP 3+ — video/audio/image verification, cutover, upstream-tracking discipline
(TBD after Step 2)