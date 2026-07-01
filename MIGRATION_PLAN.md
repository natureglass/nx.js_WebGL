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

#### Phase 2.G — WebGL2 RE-PLATFORM (port working QuickJS-era impl onto V8 substrate)

**Framing.** WebGL2 is NOT greenfield. The QuickJS-era nxjs-extended branch
ran the full Three.js r182 `com.natureglass.webgl2threejsdemos` set —
webgl2-ubo, webgl2-multiple-rendertargets, webgl2-shaders-sky,
gpgpu-water, instancing-dynamic, materials-cubemap-dynamic,
materials-envmaps, webgl-lights-spotlight, webgl-materials-video,
webgl-postprocessing-pixel, webgl-postprocessing-unreal-bloom-selective.
That impl is the REFERENCE and TARGET. 2.G is a re-platform of working
code onto the new substrate (shared ES3 context from 2.A, frozen state
contract from 2.B, V8 binding shape from #8, the SRGB/HalfFloat translates
from #10, the cube routing layer from #12). Treat it like Buckets E/F
but at WebGL2 scale.

**The diff (inventory citations).**

| Surface | QuickJS-era (nxjs-source @ nxjs-extended) | V8-migration current (nxjs-source-v8) | Delta |
|---------|------------------------------------------|---------------------------------------|-------|
| Engine WebGL C/C++ | source/webgl.c (18,244 lines) + webgl_egl.c (11,130 lines) | source/webgl.cc (1,701 lines, ~95 v1 methods) + webgl_bridge.cc (565 lines) | **~10×** more surface to re-platform on the engine side |
| Methods registered | ~190 (full v1 + v2 spec) | 95 (v1 only) | **~95 WebGL2-only methods missing** |
| Runtime v2 context | working, dispatch wired | TS class exists (webgl2-rendering-context.ts, 1,246 lines, 387 constants); `screen.getContext('webgl2')` **deliberately returns null** (screen.ts:152-162) | TS scaffold present, engine wiring + bridge missing |
| Constants | 387 v2 constants in v2 TS GL_CONSTANTS | 387 already in v2 TS (lines 201-752); 236 in v1 + 14 ES3 sized internalformats (#9) | TS constant table essentially done; engine table needs adds |
| Extensions | 9 v2-only shims (EXT_disjoint_timer_query_webgl2, EXT_texture_norm16, WEBGL_clip_cull_distance, EXT_float_blend, EXT_render_snorm, OES_sample_variables, OES_draw_buffers_indexed, WEBGL_blend_func_extended, WEBGL_compressed_texture_etc) | 0 v2-only shims | 9 shims to re-port |
| State save/restore | 18-field `nx_gl_state_snap_t` + VAO per-handle attrib snapshot (vao_save/restore_state at webgl.c:14820-14872) | 18-field `nx_gl_state_snap_t` (webgl_bridge.h:82-101); no VAO attrib snapshot, no UBO snapshot, no read-FB split snapshot | Contract extension required — see "State-contract" below |
| Tegra/Mesa quirks | PMREM cube routing (#12 ported), SRGB/HalfFloat (#10 ported), cube-face aliasing rescue, dfgLUT RG16F accept-list, MRT integer-readback gate, half-float NEAREST forcing | #10, #11, #12 shipped (v1 paths); v2 paths untested under the same patches | Most quirks already shipped via #10/#11/#12 — load-bearing for v2 too |

**Authoritative inventory citations (read these before any 2.G code):**

QuickJS-era reference (nxjs-source @ nxjs-extended, READ-ONLY):
- VAO: webgl.c:14756-14905 (create/delete/is/bindVertexArray + per-VAO state save/restore at 14820-14872)
- Samplers: webgl.c:16764-16834 (create/delete/is/bind/parameteri/parameterf)
- Transform Feedback: webgl.c:14141-14195 (transformFeedbackVaryings), 17856-17864 (handle bindings), webgl_egl.c:437-442 (begin/end/pause/resume wrappers)
- Queries: webgl.c:16972-17007 (create/delete/is/begin/end), 3125 (getQueryParameter via EXT_disjoint_timer_query_webgl2)
- Sync: webgl.c:16858-16968 (fenceSync/isSync/deleteSync/clientWaitSync/waitSync/getSyncParameter)
- UBO: webgl.c:16472-16710 (getUniformBlockIndex/getActiveUniformBlockParameter/getActiveUniformBlockName/uniformBlockBinding/bindBufferRange/bindBufferBase), 17812-17815 (bindBufferBase/getActiveUniforms/getUniformIndices)
- 3D textures: webgl.c:15666-15920 (texImage3D + texSubImage3D probes), 17785-17799 (registrations: texImage3D/texSubImage3D/copyTexSubImage3D/compressed*3D/texStorage2D/texStorage3D)
- Instanced/multi: webgl.c:12377-12495 (vertexAttribDivisor + drawArraysInstanced + drawElementsInstanced), 17772 (drawBuffers + 16-slot dispatch 2665-2700)
- Read/Draw FB split + clearBuffer: webgl.c:17772-17805 (drawBuffers/invalidate/invalidateSub/blitFramebuffer/readBuffer/framebufferTextureLayer/clearBuffer{i,ui,f,fi})
- Extended uniforms: webgl.c:17742-17769 (uniform[Matrix]NxMfv + uniformN[ui][v] + vertexAttribI* + vertexAttribIPointer + vertexAttribDivisor)
- WebGL2 constants table: webgl.c:17895-18133 (~240 v2 enums)
- Tegra/Mesa quirks: webgl.c:5203-5212 (half-float NEAREST), 6504-6638 (cube format widening), 13546-13690 (readPixels Phase 1c widening), webgl_egl.c:1999-2382 (extended H-B probe for sampler3D/2DArray/Cube)

V8 current (nxjs-source-v8, WRITE):
- Method table: webgl.cc:1436-1570 (95 methods, `install_methods()` / `FUNCS[]`)
- v1 constants: webgl-rendering-context.ts:60-247 (236, includes #9 ES3 adds at 132-144)
- v2 constants: webgl2-rendering-context.ts:201-752 (387, complete)
- State snap: webgl_bridge.h:82-101 (struct), webgl_bridge.cc:245-283 (save/restore body) — **FROZEN contract; extensions require hardware re-validation**
- v2 context class: webgl2-rendering-context.ts:766-1110 (carrier + handle classes + bulk `defineProperties` install — Patch #8 shape preserved)
- v2 getContext stub: screen.ts:152-162 — currently returns `null`

### Categorized delta — clean re-apply vs needs rework

**Bucket G.A — Near-mechanical re-apply onto V8 bindings (same logic, new binding macro):**
- VAO methods (create/delete/is/bind/+ vertex attrib I-pointer)
- Sampler objects (create/delete/is/bind/parameteri/parameterf)
- Sync objects (fenceSync/isSync/deleteSync/clientWaitSync/waitSync/getSyncParameter)
- Query objects (handles only; begin/end/getQueryParameter has a Tegra interaction — see G.D)
- Extended uniforms (uniformMatrix2x3fv ... uniformMatrix4x3fv, uniform[1-4]ui[v])
- VertexAttribI* + vertexAttribIPointer + vertexAttribDivisor
- 3D texture entry points STRUCTURALLY (texImage3D/texSubImage3D/texStorage3D/copyTexSubImage3D/compressed*3D) — the bindings re-platform clean; SAMPLING is the Tegra blocker, not the upload path
- drawBuffers/drawArraysInstanced/drawElementsInstanced
- bindBufferBase/bindBufferRange/uniformBlockBinding/getUniformBlockIndex/getActiveUniformBlockParameter/getActiveUniformBlockName/getActiveUniforms/getUniformIndices
- blitFramebuffer/framebufferTextureLayer/readBuffer
- invalidateFramebuffer/invalidateSubFramebuffer
- renderbufferStorageMultisample (already covered by ES3 in shared context)
- copyBufferSubData/getBufferSubData/getFragDataLocation
- clearBuffer{iv,uiv,fv,fi}
- getInternalformatParameter/getIndexedParameter
- WebGL2 pixelStorei pnames (PACK/UNPACK_ROW_LENGTH etc.) — extend existing v1 pixelStorei
- WebGL2 constants in engine getParameter dispatch (TEXTURE_3D, UNIFORM_BUFFER, TRANSFORM_FEEDBACK_BUFFER, DRAW_BUFFER0..N etc.) — table extension
- 9 v2-only extension shims (advertise + numeric-passthrough — same shape as the v1 extensions in 2.C; only EXT_texture_norm16, WEBGL_compressed_texture_etc, EXT_float_blend, EXT_render_snorm need any engine-side capability gate beyond a string in the supported list)

**Bucket G.B — Needs rework for the V8 substrate (depended on QuickJS/Cairo/old-context specifics):**
- v2 context creation: in QuickJS this was a 7-line "call v1 init then flip `is_webgl2=true`" (webgl.c:1664-1673). In V8 the current `nx_webgl_context_new` (webgl.cc:1617-1671) creates a carrier; v2 needs its own factory `nx_webgl2_context_new` registered as `$.webgl2ContextNew` and bound on the v2 prototype via `$.webglInitClass` v2 slot. Mechanical but the V8 carrier-and-proto shape (#8 constraint) differs from QuickJS's class macros.
- screen.getContext('webgl2') wiring (screen.ts:152-162): currently returns `null` by design. Flipping to the real factory needs the engine `$.webgl2ContextNew` symbol to exist first.
- Bridge state save: needs CONTRACT EXTENSION — see G.C. The QuickJS-era impl carries the additional state save naturally (it's not in nx_gl_state_snap_t — it's per-VAO handle state at webgl.c:14820-14872 + EGL-side bookkeeping). Re-platforming = decide what new fields go in `nx_gl_state_snap_t` vs what stays in per-handle attrib snapshots.
- `is_webgl2` capability gating: 8+ branch points in QuickJS-era at webgl.c:2120/2125/2131/2135/2139/2143/2159/2164/2176 (extension is_webgl2 gates). The V8 v1/v2 contexts are separate classes (independent prototype chains) per the current webgl2-rendering-context.ts:766 — so a single boolean flag isn't the right shape. Either: (i) duplicate the FUNCS[] table for v2 with the v2-only adds, or (ii) keep a single function set and bind only v1-relevant entries on the v1 prototype. Decision is structural.
- Three.js material/lighting uniform bridge: QuickJS-era carried bridge uniforms for lights/fog/spot lights/point lights/material props (webgl.c:920-1400). The V8 bridge is much thinner — the cube-route-shim.ts approach (#12) suggests these moved to runtime-TS. Audit which of the QuickJS-era bridge uniforms are now obsolete vs still load-bearing for v2 demos that bypass the cube-route-shim path.

**Bucket G.C — State-contract extensions WebGL2 forces (the load-bearing risk).**

The 2.B `nx_gl_state_snap_t` (webgl_bridge.h:82-101, 18 fields) is FROZEN and proven on 4,013 frames + production hardware. WebGL2 touches state outside it. Each extension needs a hardware re-validation pass.

| WebGL2 feature | State outside current snap | Risk | Mitigation candidate |
|---|---|---|---|
| VAO | bound VAO + per-attrib divisor + per-attrib integer-type + ELEMENT_ARRAY_BUFFER_BINDING (currently ALREADY in snap per black-cube fix); GL_VERTEX_ARRAY_BINDING IS in snap (field `vao`) | Medium — `vao` field IS in snap (webgl_bridge.h line 248), but per-attrib integer-type is per-VAO storage, not Skia-touched. Skia uses GL_VERTEX_ARRAY=0 (no VAO bound) — restore-to-0 already covers it. | None — snap is sufficient; VAO state IS per-handle, restored on bindVertexArray |
| UBO indexed bindings | GL_UNIFORM_BUFFER_BINDING + GL_UNIFORM_BUFFER_BINDING[i] for each binding point; UNIFORM_BUFFER_START/SIZE indexed | High — Ganesh-GL may not touch UBO, but verifying that requires a hardware test. If Ganesh doesn't touch UBO, save = 0; if it does, save = up to MAX_UNIFORM_BUFFER_BINDINGS slots | Probe first: query `GL_UNIFORM_BUFFER_BINDING[i]` before/after Skia frame; if always 0, snap stays the same. If touched, add `ubo_bindings[MAX_UNIFORM_BUFFER_BINDINGS]` to snap. |
| Transform feedback | TRANSFORM_FEEDBACK_BUFFER_BINDING, current TF object, TF varyings, RASTERIZER_DISCARD enable | High — TF is rare in render-frame work; Ganesh doesn't use it. Save = current TF binding + RASTERIZER_DISCARD enable (must be disabled before Skia draws) | Add `tf_binding` (handle) + `rasterizer_discard` (bool) to snap; restore both. Hardware-verify nothing else needed. |
| Sampler objects | per-texture-unit bound sampler (NUM_TEXTURE_UNITS slots) | High — Ganesh-GL uses tex unit 0; if a sampler object is bound there, Ganesh's `texParameteri` may have no effect. Critical to save unit-0 sampler binding | Extend snap with `sampler_unit0` field (or all-units if MAX is small); restore to 0 around Skia |
| Read/Draw FB split | READ_FRAMEBUFFER_BINDING (DRAW_FRAMEBUFFER_BINDING is already in snap as `fbo`) | Medium — Skia binds the same FBO to both targets when calling glBindFramebuffer(GL_FRAMEBUFFER, x). Saving READ separately matters for blitFramebuffer / readPixels mid-frame | Extend snap with `read_fbo` field; restore separately |
| Pixel pack/unpack | PACK_ROW_LENGTH/SKIP_*, UNPACK_ROW_LENGTH/SKIP_*/IMAGE_HEIGHT (8 fields total) | Low-Medium — Three.js sets/unsets per-upload; Ganesh resets per-upload. Skia may not touch. Verify | Probe first; if untouched by Skia, snap stays the same. If touched, add 8 ints to snap. |
| Bound query | CURRENT_QUERY for each target | Low — Queries are async; bracketed begin/end inside a single WebGL frame; Ganesh doesn't use queries | Don't extend snap; document that user must close any begun query before next compose |
| Active TF | TRANSFORM_FEEDBACK_BINDING (object) + ACTIVE flag | Low — bracketed begin/end within a single user frame; Ganesh doesn't use | Don't extend snap; document close-before-compose contract |

**Snap-extension hardware passes required (one each):**
1. UBO indexed bindings probe (no code change; just measure)
2. Sampler-unit-0 binding restore (snap field + hardware re-verify of unchanged demos)
3. Read-FB separate restore (snap field + hardware re-verify)
4. Transform-feedback + rasterizer-discard restore (snap field + TF demo if any exists)

Each is bisectable; do them serially, not in parallel.

**Bucket G.D — Tegra/Mesa driver-limit workarounds for WebGL2.**

These are quirks already discovered + worked around in the QuickJS era. Per [[reference-mesa-nouveau-layered-sampling-unsupported]] (confirmed via H-B probe at boot) Tegra Mesa-Nouveau cannot SAMPLE from sampler3D / sampler2DArray / samplerCube — these silently return vec4(0).

| Workaround | Status post-V8 | Disposition for 2.G |
|---|---|---|
| samplerCube → sampler2D cube-uv 2D atlas routing | Patch #12 SHIPPED (cube-route-shim.ts); v1-verified, v2 untested | Re-verify on v2 demos (materials-envmaps, materials-cubemap-dynamic); load-bearing |
| PMREM intermediate render-target (sized RGBA16F + HALF_FLOAT normalization) | Patch #10 + #11 SHIPPED; v1-verified | Re-verify on v2 PMREM paths (webgl-postprocessing-unreal-bloom-selective uses bloom which is PMREM-adjacent) |
| dfgLUT RG16F texSubImage2D accept-list | Engine SHIPPED in v8-migration webgl.cc accept-list (per memory [[reference-dfglut-rg16f-accept-list-fix]]) | Verify still in V8 source; if not, re-port to webgl.cc accept-list |
| Cube-face FBO aliasing rescue (CubeCamera dynamic cubemap) | Skipped in F.2b as N/A for `webgl-materials-cubemap`; load-bearing for `materials-cubemap-dynamic` (FBO writes to cube faces) | RE-PORT per [[reference-mesa-cube-face-aliasing-rescue]] when materials-cubemap-dynamic comes up — Phase 2.G.4 |
| Half-float MIN/MAG=NEAREST forcing on Tegra (webgl.c:5203-5212) | NOT ported to v8-migration | Re-port to engine accept-list; load-bearing for any half-float texture |
| sampler3D / sampler2DArray driver block | Driver limit, no workaround; gates webgl2-texture2darray demo + ~480 tex-3d conformance tests | DO NOT attempt to fix; explicitly gate the demo out of 2.G acceptance |
| MRT integer-format readback (Phase 1c, webgl.c:13546-13690) | NOT ported (v1 doesn't exercise) | Re-port readPixels format gate widening; load-bearing for webgl2-multiple-rendertargets if it reads back, and for conformance |
| Three.js r182 PMREM compatibility (sampler3D + texStorage3D + render path) | Partially ported via #10 + #11; v1 unaffected | Verify on r182 v2 demos (materials-envmaps) — likely needs #12's cube-route-shim because direct sampler3D fails |
| WebGL2 antialias=false MSAA fallback (Mesa surfaceless default-FB limit) | Already-known v1 constraint (no engine fix); Three.js demos use post-process MSAA via renderbufferStorageMultisample which IS available | Document only; not a blocker |

**Becomes-obsolete on the new substrate (do NOT re-port):**
- Cairo black-trail compositor workarounds (Skia composite replaces Cairo; was the precondition for the cursor compositor)
- Cocos2D-specific UBO-block-unbound workaround at webgl.c:4506-4510 (we don't run Cocos; brewser/Three.js is the only consumer)
- QuickJS class-macro install (V8 uses bulk-defineProperties per #8)
- Generation-stamped resource recreation (webgl.c:35-128 audit table) — Three.js doesn't trigger context loss on brewser; if needed later, port as standalone

### First slice — UNAMBIGUOUS GATE for 2.G

**Slice demo:** `D:/Workspace/brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/webgl2-ubo/`

**Why this slice (the unambiguous gate):**
- It was working on the QuickJS-era nxjs-extended fork (per user confirmation that the whole `com.natureglass.webgl2threejsdemos` set ran pre-migration). Same demo, same scene, same Three.js version. Result must match.
- It exercises the CANONICAL WebGL2-only feature: Uniform Buffer Objects via `THREE.UniformsGroup`. Calls bindBufferRange + `layout(std140)` uniform blocks + uniformBlockBinding + getUniformBlockIndex + getActiveUniformBlockParameter. None of these work on a v1 context — if the demo renders, the UBO path is correct.
- Uses ONLY sampler2D (143KB crate.png). No layered samplers → not blocked by [[reference-mesa-nouveau-layered-sampling-unsupported]].
- Status panel (canvas-2D overlay) gives a visual pass/fail — extension list + WebGL2 version string + FPS counter render alongside the scene.
- 200 alternating tetrahedron/box meshes share two UBO bindings (ViewData + LightingData) — enough state to stress the UBO indexed-binding restore contract.

**Minimum v2 surface for the slice (predicted; capture actual via `__brewserGLProxyDebug` proxy):**
- Engine: `$.webgl2ContextNew` factory + `$.webglInitClass` v2 binding pass (~95 v1 methods reused + ~25 v2-only adds for THIS slice)
- v2-only methods minimally required: createVertexArray, bindVertexArray, deleteVertexArray (Three.js v2 path uses VAOs unconditionally), bindBufferBase, bindBufferRange, getUniformBlockIndex, uniformBlockBinding, getActiveUniformBlockParameter, getActiveUniformBlockName, getActiveUniforms, drawArraysInstanced (Three.js may use even without explicit instancing), drawBuffers, blitFramebuffer (Three.js v2 framebuffer copy path), clearBufferfv (Three.js may call instead of clear()), texStorage2D (already present per Bucket E shipping), readBuffer, framebufferTextureLayer (Three.js IBL — likely unused in UBO demo)
- screen.ts:152-162 flipped from `return null` to `createWebGL2Context(this)`
- Bridge: state contract extension for UBO indexed bindings if the probe finds Skia/Ganesh touches them (probe FIRST, change snap only if necessary)
- Runtime: existing `webgl2-rendering-context.ts` (1,246 lines) prototype install runs against the now-non-null engine context

**Unambiguous gate:** webgl2-ubo renders the 200 alternating tetra+box scene with the crate texture on hardware, status panel shows the WebGL2 string + UBO extension confirmed, sustained ≥ 60 s. SAME visual as on the QuickJS-era fork. **Same demos, same result = done.**

### Phase 2.G structure — slice-first, then expand demo by demo

Each phase = one demo's GO. Demos are hardware-verified before the next phase begins.

**Phase 2.G.0 — Pre-slice: engine v2 context factory + screen.getContext('webgl2') wiring**
- Add `nx_webgl2_context_new` to webgl.cc (mirrors `nx_webgl_context_new` shape, sets a v2-distinguishing field on the carrier)
- Register `$.webgl2ContextNew` JS init export
- Flip screen.ts:152-162 from `return null` to `createWebGL2Context(this)`
- Add `createWebGL2Context` to webgl2-rendering-context.ts (parallel to webgl1's at webgl-rendering-context.ts:606)
- GO criterion: `screen.getContext('webgl2')` returns a non-null object on Citron; Three.js detects it via `gl.constructor.name === 'WebGL2RenderingContext'`
- NO method implementations yet; calls will throw `TypeError: X is not a function`

**Phase 2.G.1 — First-slice bridge surface (webgl2-ubo)  [x] SHIPPED + CITRON VERIFIED 2026-06-30**
- v1 95-method FUNCS[] copied verbatim into install_methods_v2 (cut #1)
- v2-only adds shipped via iterative diag-proxy capture, NOT predicted:
  - cut #2: `texImage3D` — Three.js v2 init creates 1×1 placeholder textures for default-bound TEXTURE_3D/2D_ARRAY samplers
  - cut #3: 9 v2 methods batched after the texImage3D round established the cadence — `createVertexArray`/`deleteVertexArray`/`isVertexArray`/`bindVertexArray` (new K_VERTEX_ARRAY_OBJECT handle kind), `bindBufferBase`/`bindBufferRange`/`getUniformBlockIndex`/`uniformBlockBinding`, `texStorage2D`
- All adds are direct GLES3 passthroughs (no Mesa Nouveau format-widening probes required — Three.js's UBO path uses canonical formats)
- Bridge state contract probe: NOT YET RUN (still gated as #16-ACTIVE design; not blocking — UBO-only paths didn't surface "Skia renders garbage after WebGL2 draws"). Snap extension #17 still PROPOSED.
- **GO criterion met on CITRON**: webgl2-ubo renders the 200 alternating tetrahedra/crate-textured box scene; status panel reports `THREE r184 loaded: yes`, `demo error: (none)`, `scene ready: yes`; per-mesh color variation from shared UBO visible; extensions advertised match v1 path.
- **Hardware-JIT pass still PENDING** — Citron is jitless, doesn't exercise aarch64 JIT codegen path. Per testing discipline, a real CFW Switch boot is required before 2.G.1 is hardware-verified.
- 2.G.1 ships 11 distinct v2-only engine bindings + 1 new handle kind. The webgl2-ubo slice is the proof-of-life: if this works on hardware (which it should — same codegen path as v1 demos that already pass hardware-JIT), the remaining 2.G.x demos are incremental.

**Phase 2.G.2 — webgl2-multiple-rendertargets (MRT + draw_buffers + read/draw FB split)**
- Adds: drawBuffers (already needed in G.1; verify), framebufferTextureLayer if 2D-Array-based MRT (NOT in this demo — uses 2D textures), readBuffer, renderbufferStorageMultisample
- State contract: read/draw FB split MUST be in snap by now (saved separately)
- GO criterion: G-buffer demo renders on hardware (color + normal attachments visible, post-process composition correct), sustained ≥ 60 s

**Phase 2.G.3 — webgl2-shaders-sky + instancing-dynamic**
- Sky: minimal v2 (mostly GLSL 3.0 + uniforms; no UBO/MRT). Should pass with G.1 surface.
- Instancing-dynamic: drawArraysInstanced/drawElementsInstanced + vertexAttribDivisor — likely already covered in G.1; just verify.
- GO criterion: both render on hardware

**Phase 2.G.4 — materials-cubemap-dynamic + materials-envmaps (Tegra cube quirks at v2)**
- Re-verify Patch #12 (cube-route-shim.ts) under v2 context
- Re-port [[reference-mesa-cube-face-aliasing-rescue]] (CubeCamera dynamic cubemap; SKIPPED in F.2b as N/A for static cubemap, now load-bearing)
- GO criterion: both demos render on hardware (degraded paths flagged with `[nxjs:cube-fix:*]` markers per F.2a precedent)

**Phase 2.G.5 — webgl-lights-spotlight (shadow maps)**
- Spotlight shadow uses sampler2DShadow + COMPARE_MODE/COMPARE_FUNC — single texture, NOT sampler2DArrayShadow
- Re-port [[reference-brewser-threejs-spotlight-shadow-engine-fix]] if it was a fork patch (cite NXJS_PATCHES_NEEDED.md when entered)
- GO criterion: spotlight shadow renders on hardware

**Phase 2.G.6 — webgl-postprocessing-pixel + webgl-postprocessing-unreal-bloom-selective**
- Postprocessing exercises FBO ping-pong + multiple shader passes
- UnrealBloom uses PMREM-adjacent paths (selective bloom layer compositing) — re-verify #10 + #11
- GO criterion: both demos render on hardware

**Phase 2.G.7 — gpgpu-water (transform feedback / FBO-as-compute)**
- gpgpu-water uses GPGPU pattern (FBO ping-pong, NOT transform feedback in Three.js) — this is the biggest stress test for state contract
- State contract: read/draw FB split + sampler unit + half-float texture support all stressed
- If the demo uses transform feedback objects directly: re-port TF state save (Bucket G.C row)
- GO criterion: gpgpu-water renders on hardware

**Phase 2.G.8 — webgl-materials-video (HTMLVideoElement texImage2D)**
- Re-port the HTMLVideoElement → texImage2D path if it was an engine fork patch; otherwise this is a runtime-side (image source) integration
- GO criterion: video texture animates on hardware

**Phase 2.G.9 — physics-rapier-basic (WASM physics, mostly CPU)**
- Likely tests instancing + WASM. If G.1-G.3 cover the GL surface, this should pass.
- GO criterion: physics simulation renders + physics WASM ticks on hardware

**Phase 2.G.X — Acknowledged out-of-scope:**
- webgl2-texture2darray (sampler2DArray direct sample — Mesa-Nouveau driver block per [[reference-mesa-nouveau-layered-sampling-unsupported]])
- ~480 tex-3d-* conformance tests (same driver block)
- 8 cube-direct-sample conformance tests (same driver block; PMREM cube-uv routing in #12 is for env maps only, not for raw sampler-test conformance)

### Catalog of dropped WebGL2 fork patches (NXJS_PATCHES_NEEDED.md adds)

Each dropped fork patch becomes a numbered entry in NXJS_PATCHES_NEEDED.md when first re-applied; preview list below.

| # (planned) | Patch | Status pre-2.G | Files | Disposition |
|---|---|---|---|---|
| #13 | v2 context factory + screen.getContext('webgl2') wiring | DROPPED on upstream pull | source/webgl.cc, packages/runtime/src/canvas/webgl2-rendering-context.ts, packages/runtime/src/screen.ts | upstream-candidate (any embedder needs functional v2 context) |
| #14 | WebGL2 engine method allowlist (~95 v2-only adds + 9 v2-only extensions) | DROPPED | source/webgl.cc (FUNCS[] table + impl bodies) | upstream-candidate (engine-correctness) |
| #15 | Bridge state contract extension: read-FB + sampler-unit-0 + UBO indexed (conditional on probe) + TF + rasterizer_discard | NEW (2.B contract didn't cover) | source/webgl_bridge.h, source/webgl_bridge.cc | upstream-candidate (engine-correctness; coexistence-required) |
| #16 | Per-VAO attribute snapshot save/restore on bindVertexArray | DROPPED (webgl.c:14820-14872 in QuickJS-era) | source/webgl.cc (VAO save/restore impl) | upstream-candidate (WebGL spec compliance) |
| #17 | Tegra half-float MIN/MAG=NEAREST forcing | DROPPED (webgl.c:5203-5212) | source/webgl.cc texParameter / texImage paths | fork-only (Tegra-specific) |
| #18 | Cube-face FBO aliasing rescue (CubeCamera) | DROPPED + SKIPPED F.2b | source/webgl.cc + per [[reference-mesa-cube-face-aliasing-rescue]] | fork-only (Tegra Mesa-Nouveau) |
| #19 | readPixels Phase 1c format-widening (RGBA/RGB/RG/RED × UByte/Float/Half + Int formats) | DROPPED (webgl.c:13546-13690) | source/webgl.cc readPixels accept-list | upstream-candidate |
| #20 | dfgLUT RG16F texSubImage2D accept-list (verify in V8 source vs re-port) | Memory says shipped per [[reference-dfglut-rg16f-accept-list-fix]] | source/webgl.cc texSubImage2D accept-list | upstream-candidate (already shipped — verify-and-document) |
| #21 | Three.js bridge uniforms for lights/materials/fog (if still load-bearing) | DROPPED (webgl.c:920-1400 in QuickJS-era; may be obsolete with cube-route-shim) | TBD on demo-by-demo basis | fork-only or obsolete |

Numbers above are PLANNED placeholders; actual numbering is the next-available slot at re-application time (see NXJS_PATCHES_NEEDED.md DISPOSITION POLICY).

**GO criterion (Phase 2.G, overall):** webgl2-ubo, webgl2-multiple-rendertargets, webgl2-shaders-sky, gpgpu-water, instancing-dynamic, materials-cubemap-dynamic, materials-envmaps, webgl-lights-spotlight, webgl-materials-video, webgl-postprocessing-pixel, webgl-postprocessing-unreal-bloom-selective ALL render on hardware to the same fidelity as on the QuickJS-era nxjs-extended fork. webgl2-texture2darray is gated out (Tegra Mesa-Nouveau driver limit, acknowledged out-of-scope). NXJS_PATCHES_NEEDED.md grows by ~9 entries (#13-#21 above, exact count TBD on re-application).

### Honest scope read

**How big is 2.G really?** Bigger than 2.D (geometry-cube slice) but structurally similar in shape: re-platform proven code onto the new substrate, demo by demo. The size delta vs 2.C is ~95 v2-only methods + 9 extensions + state-contract extension + a Tegra quirk re-port pass. Most v2 methods re-platform cleanly (Bucket G.A, ~75% of the delta). The hard parts are:
1. The state-contract extension (Bucket G.C) — 4 hardware-validated additions to `nx_gl_state_snap_t`. Each is small, but each needs a hardware boot. Estimate 4-6 hardware sessions for state contract alone.
2. The Tegra cube-face FBO aliasing rescue (Bucket G.D row 4) — load-bearing for materials-cubemap-dynamic; the recipe is documented in [[reference-mesa-cube-face-aliasing-rescue]] (per-transition CPU-roundtrip rescue v3) — non-trivial re-port.
3. Re-verifying the existing #10 + #11 + #12 patches under v2 context (they were v1-verified). If they "just work" under v2: cheap. If v2 unmasks a new code path: a focused round each.

Best-case (everything clean): 3-4 weeks of focused work, ~8 hardware sessions.
Realistic (one or two demos need a focused-round each like 2.F.2a): 6-8 weeks, ~15 hardware sessions.

**Is 2.G the right next thing, or do WebGL1 gaps gate it?** Mostly NO — the v1 demo set is well-covered after 2.F. The few remaining v1 gaps ([[reference-brewser-v1-black-texture-demos]] resolved per memory; cube-camera dynamic per F.2b deferred) are demo-specific and don't gate v2. The v8-migration is on the cusp of the WebGL2 work being unblocked.

**The strong position:** the gate is unambiguous. The demo set ran pre-migration. Each demo's pass/fail is binary, visually testable, and matches a pre-migration screenshot. That's a much stronger gate than "implement the spec correctly" — we're chasing visual parity with a known-good run, demo by demo. The QuickJS-era impl is the answer key.

**Risks:**
- Tegra Mesa-Nouveau may have driver limits we didn't hit on v1 paths (esp. UBO indexed bindings, integer-attribute draws, MSAA multisample renderbuffer at large sample counts). Each surfaces as a focused round; the cube-route-shim precedent (runtime-side workaround) is the template.
- The state contract extension is irreversible once landed (it's the 2.B FROZEN contract). Each addition needs production-hardware verification, not Citron.
- V8 JIT regressions (the kind that #8 hunted down) may re-emerge with new bulk-method-install shapes for v2. Memory's recurrence tell ([[project-v8-migration-8-root-caused]]) applies: any future TS regression of `for (const [k,v] of Object.entries(GL_CONSTANTS))` pattern brings the crash back silently on hardware-JIT only.

**Stop-and-sign-off gates inside 2.G:**
- After 2.G.0: confirm wiring works on Citron before touching engine methods
- After 2.G.1 (webgl2-ubo): the slice is the proof-of-life. If this works on hardware, the rest is incremental.
- After 2.G.4 (materials-cubemap-dynamic): if the cube-face FBO aliasing rescue doesn't re-port cleanly, we've hit a substrate divergence — re-scope.
- After 2.G.7 (gpgpu-water): if state contract holds through 1000+ FBO ping-pongs, 2.G's hardest substrate question is answered.

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