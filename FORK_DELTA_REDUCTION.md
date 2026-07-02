# Fork-delta reduction study — nxjs-source-v8 vs upstream nx.js

Read-mostly investigation (Phases 1–4) + mechanical ledger reshuffle
(Phase 5). The report is the deliverable for Phases 1–4 and the
audit log for Phase 5(b). Only the five files enumerated in the
task brief were written; no engine or runtime code was edited.

**Investigation scope.** Measure how much our `v8-migration` branch
diverges from upstream nx.js, classify every entry in
NXJS_PATCHES_NEEDED.md by how it could be shrunk, and evaluate
the proposal to move engine patches to a brewser-runtime-side
`globalThis` injection layer.

**Investigation date.** 2026-07-02. Base ref captured below.

**Ground rules honored.**
- No renumbering of ledger entries.
- Investigation phases (1–4) touched no engine, runtime, or demo code.
- Phase 5 wrote exactly five files. Existing entries moved verbatim,
  including their RE-APPLY / VERIFY notes.
- Entry #19 and other in-flight Phase 2.G re-verify targets were
  not moved.
- The DISPOSITION POLICY section of NXJS_PATCHES_NEEDED.md was
  preserved unchanged and copied (not moved) into RUNTIME_SHIMS.md.

---

## Phase 1 — Real delta vs upstream

**Upstream base ref.** `34d2d03f7ddd63b7c4f279f85d3e15d3de33d307`
(upstream/main tagged/aliased Version Packages (beta) commit;
current `git merge-base upstream/main v8-migration`). Upstream/main
has since moved two commits ahead (`27c56a1` fix + `2826e05` cleanup);
neither is currently in our tree. The metric below therefore
UNDER-counts what an upstream pull would require reconciling by
whatever those two commits changed (small — one 25-line fix + a
staging cleanup, spot-checked to not touch any files we've modified).

**Command that produced the numbers.**
`git diff --stat 34d2d03..v8-migration` and its `--numstat` sibling.
Adds/removes are line-level, not byte-level.

### Bucket (a) — NEW fork-owned files (12 files, 6,325 LOC)

| File | LOC | Category | Rebase pain |
|---|---:|---|---|
| MIGRATION_PLAN.md | 934 | doc | none |
| NXJS_PATCHES_NEEDED.md | 2,503 | doc | none (this study reduces it) |
| packages/runtime/src/canvas/webgl-rendering-context.ts | 614 | engine-TS | none |
| packages/runtime/src/switch/video-decoder.ts | 138 | engine-TS | none |
| source/cursor.cc | 312 | native C++ | none |
| source/cursor.h | 94 | native C++ | none |
| source/detect.cc | 83 | native C++ | none |
| source/detect.h | 26 | native C++ | none |
| source/video-decoder.cc | 317 | native C++ | none |
| source/video-decoder.h | 4 | native C++ | none |
| source/webgl_bridge.cc | 1,020 | native C++ | none |
| source/webgl_bridge.h | 280 | native C++ | none |

New-file additions do not create rebase conflicts under normal
circumstances; they only require re-wiring if upstream adds a file
at the same path (currently none of these paths exist upstream).
They inflate maintenance surface but do not block pulls.

### Bucket (b) — MODIFIED upstream files (15 files, +2,818 / −2,552)

**This is the rebase-pain metric.**

| File | +add | −del | Notes |
|---|---:|---:|---|
| packages/runtime/src/$.ts | 33 | 1 | binding type-decls for our added natives |
| packages/runtime/src/audio.ts | 19 | 2 | #2 globalThis.fetch deferral |
| packages/runtime/src/canvas/webgl2-rendering-context.ts | 41 | 6 | #8 fix + $.webgl2 factory |
| packages/runtime/src/image.ts | 33 | 2 | #1 globalThis.fetch deferral |
| packages/runtime/src/screen.ts | 52 | 11 | #7 v1 route + #14 v2 route |
| packages/runtime/src/switch/index.ts | 1 | 0 | export VideoDecoder |
| packages/runtime/src/video.ts | 19 | 2 | #3 globalThis.fetch deferral |
| source/canvas.cc | 144 | 0 | #4 cursor bindings + #13 font-size pin |
| source/config.cc | 63 | 0 | webgl_test_fbo + webgl_state_probe flags |
| source/config.h | 41 | 0 | same |
| source/main.cc | 174 | 0 | wiring: cursor, bridge, video-decoder, detect |
| source/skia_gpu.cc | 93 | 7 | #5 ES3 shared context |
| source/skia_gpu.h | 58 | 0 | #5 handle accessors |
| source/webgl.cc | 2,037 | 2,521 | net −484: we DROPPED upstream's parallel-EGL WebGL2 chain and replaced with our shared-context ~95-method v1 + empty-v2 install |
| source/webgl.h | 10 | 0 | nx_webgl_compose_if_active decl |

**Where the pain is concentrated.**
1. `source/webgl.cc` (net −484 lines but with a 2,037/2,521 churn
   pattern — the file has been effectively rewritten). Any upstream
   change to webgl.cc will conflict badly.
2. `source/main.cc` (+174 lines of wiring), `source/canvas.cc`
   (+144 lines), and the two `source/skia_gpu.*` (+151 lines
   combined). Each adds hooks that will need to be re-wired at every
   upstream drift of these files.
3. Seven `packages/runtime/src/*.ts` files with small deltas each.
   Individually cheap; collectively meaningful because upstream is
   most active in the runtime TS layer.

**Delta summary.**
- 15 upstream files touched (rebase-pain surface).
- 12 new files (maintenance surface, no rebase pain).
- ~9.1 KLOC added, ~2.5 KLOC removed. Doc-only new files account for
  3.4 KLOC of the additions.
- **The primary reduction target is the modified-upstream-file
  count.** Getting `image.ts`, `audio.ts`, `video.ts`, and the two
  runtime TS `webgl*-rendering-context.ts` files to zero would drop
  the count from 15 to 10 and eliminate the runtime-TS churn class.

---

## Phase 2 — Per-entry classification

Every entry in NXJS_PATCHES_NEEDED.md, one row. Columns:

- **#** — entry number (never renumbered).
- **Delta type** — where the currently-shipped fix physically lives:
  `native-C++` (source/\*.cc), `engine-TS` (packages/runtime/src/\*.ts),
  `runtime-side-only` (brewser-runtime-v8/\*), `demo-side` (brewser-apps/\*),
  `none` (open, unshipped), `hybrid` (multiple layers).
- **Injectable via globalThis?** — could a brewser-runtime install-time
  patch replace it. `yes` / `partial` / `no` + one-line justify.
- **Best reduction path** — one of `UPSTREAM-PR`, `MOVE-TO-RUNTIME`,
  `FORK-FILE-ISOLATE` (keep in fork but confined to a new file that
  doesn't touch upstream files), `KEEP-AS-IS`, `ALREADY-ZERO-DELTA`.
- **Migration cost** — S / M / L.
- **Risk flags** — HOT-PATH / INIT-ORDER / CONTRACT / OPEN.

| # | Title (short) | Disposition | Delta type | Injectable? | Best path | Cost | Risk |
|---:|---|---|---|---|---|---|---|
| 1 | image.ts fetch deferral | upstream-candidate | engine-TS | **yes** — replace `Image.prototype.src` setter at runtime install | UPSTREAM-PR | S | INIT-ORDER (runtime can inject anytime — engine loads Image before runtime, but the setter lookup is per-use) |
| 2 | audio.ts fetch deferral | upstream-candidate | engine-TS | **yes** — same shape as #1 | UPSTREAM-PR | S | (as #1) |
| 3 | video.ts fetch deferral | upstream-candidate | engine-TS | **yes** — same shape as #1 | UPSTREAM-PR | S | (as #1) |
| 4 | Screen.setCursorOverlay native binding | fork-only | native-C++ | **no** — needs C-side composite pass hooked into engine's present chain, no JS-level surface can supply it | KEEP-AS-IS (already FORK-FILE-ISOLATE via new `source/cursor.{cc,h}`; the wiring in `main.cc`/`canvas.cc` is minimal) | L | HOT-PATH (per-frame composite), CONTRACT (owns `display_buffer`) |
| 5 | Skia_gpu ES3 shared context + accessors | upstream-candidate | native-C++ | **no** — modifies EGL context creation in engine boot | UPSTREAM-PR (bundle with #6) | M | INIT-ORDER (must exist before Skia init), CONTRACT (engine boot contract) |
| 6 | webgl_bridge state save/restore + tenant FBO | upstream-candidate | native-C++ | **no** — the save/restore primitive IS a C-only GL contract; there is no equivalent JS API | UPSTREAM-PR (bundle with #5) | L | HOT-PATH (per-frame around every WebGL call), CONTRACT (2.B locked contract) |
| 7 | WebGL1 context via getContext('webgl') | upstream-candidate (with caveats) | hybrid: native-C++ + engine-TS | **no** for the C++ dispatch surface; TS shim already in engine-TS layer | UPSTREAM-PR after 2.G stabilizes | L | HOT-PATH, INIT-ORDER, CONTRACT |
| 8 | V8 JIT crash — Object.entries fix | upstream-candidate | engine-TS | **partial** — runtime could patch the class after load, but the FIX must precede constants installation which happens at module-body scope of packages/runtime/src/canvas/webgl*-rendering-context.ts. Runtime patch would apply too late unless it replaces the class entirely | UPSTREAM-PR (zero C++ delta; pure TS pattern change; likely uncontroversial) | S | INIT-ORDER (fix runs at engine-TS module body, before runtime loads — a `globalThis` injection at runtime install cannot pre-empt the module body) |
| 9 | v1 ES3 sized internalformat constants | upstream-candidate | engine-TS | **partial** — a runtime shim could add missing GL_CONSTANTS entries at install-time via `Object.defineProperties(WebGLRenderingContext.prototype, …)`, but MUST use the same #8-safe bulk shape | UPSTREAM-PR (bundle with #10) | S | INIT-ORDER (Three.js reads `gl.SRGB8_ALPHA8` at first WebGL touch — must land before the first WebGL context method call) |
| 10 | WebGL1 EXT_sRGB + HalfFloat translate | upstream-candidate | native-C++ | **partial** — a runtime wrapping of `gl.texImage2D` / `texSubImage2D` could rewrite (internalformat, format, type) tuples client-side. Costs a per-tex-upload JS hop but functionally covers the ES3 mismatch | UPSTREAM-PR (bundle with #9); or MOVE-TO-RUNTIME as fallback | M native / S runtime | HOT-PATH (texImage2D called once per texture, not per frame — safe to wrap) |
| 11 | PMREM r184 FS replacement | fork-only | native-C++ | **partial** — could wrap `gl.shaderSource` runtime-side and do the substitution there; would parallel cube-route-shim's pattern. BUT the QuickJS-era logic is a hand-tuned Mesa-Nouveau compiler workaround; migrating to TS costs 120 lines and a Mesa driver check that today lives correctly in C | FORK-FILE-ISOLATE (native fix stays; consider MOVE-TO-RUNTIME parallel to #12 if an r184 PMREM demo lands) | M | HOT-PATH (per shader-source call, but shader compile is one-shot per material), CONTRACT (Tegra-specific) |
| 12 | samplerCube→sampler2D routing shim | fork-only | runtime-side-only | **already** — this IS the runtime injection pattern the study is asking about | ALREADY-ZERO-DELTA — no engine delta whatsoever | 0 (shipped) | HOT-PATH (per shader-source call; shim work is stable at compile time) |
| 13 | canvas.cc font-size pin | upstream-candidate | native-C++ | **no** — the bug is FT_Face char_size state corruption, a C-level shared-object issue with no JS surface | UPSTREAM-PR | S | HOT-PATH (per fillText/strokeText/measureText — but the cost is one FT_Set_Char_Size cache-hit, sub-microsecond) |
| 14 | WebGL2 context factory | upstream-candidate | hybrid: native-C++ + engine-TS | **no** for the C++ factory; the TS wrapper is the mint point | UPSTREAM-PR (bundle with #15) | M | CONTRACT (Phase 2.G table-split shape) |
| 15 | v1/v2 method-table split | upstream-candidate | native-C++ | **no** — install_methods runs at engine bring-up | UPSTREAM-PR (bundle with #14) | S | CONTRACT (JIT-safety discipline from #8) |
| 16 | Passive state-contract probe | fork-only diagnostic | native-C++ | **no** — reads GL state at C-level bridge hooks | KEEP-AS-IS (diagnostic; leave in fork) | 0 | (diagnostic only, never in hot path unless flag on) |
| 17 | nx_gl_state_snap_t extension (sampler_unit0 + read_fbo) | fork-only | native-C++ | **no** — extends the 2.B save/restore contract | KEEP-AS-IS | 0 (shipped) | HOT-PATH (per bridge frame), CONTRACT (2.B contract extension) |
| 17-superseded | (archive marker) | — | none | n/a | ARCHIVE | 0 | — |
| 18 | cube-route-shim per-method guards | brewser-specific | runtime-side-only | **already** | ALREADY-ZERO-DELTA | 0 | — |
| 19 | Cube-routing v2 applicability | brewser-specific | runtime-side-only (OPEN) | **already** — but OPEN, tagged 2.G.4 re-verify | KEEP-AS-IS pending Phase 2.G.4 (flagged as ambiguous, defaults to STAYS per rule) | 0 | OPEN (Phase 2.G.4 gate) |
| 20 | UNPACK_FLIP_Y_WEBGL honor | upstream-candidate (engine ask OPEN) | native-C++ ask + demo-side workaround shipped | **partial** — a runtime wrapping of `gl.texImage2D` could reverse-row before uploading | UPSTREAM-PR (engine fix); demo-side workaround is a stopgap in brewser-apps | M | HOT-PATH (per texImage2D — but only fires on flipY=true+typed-array uploads, rare per frame), OPEN |
| 21 | sampler2DShadow rewrite shim | brewser-specific | runtime-side-only | **already** | ALREADY-ZERO-DELTA | 0 | HOT-PATH (per shader-source, one-shot) |
| 22 | Switch.VideoDecoder V8 port | upstream-candidate | hybrid: native-C++ + engine-TS | **no** — needs native FFmpeg + audrv bindings | UPSTREAM-PR (VideoDecoder is a documented nx.js API; parity restoration) | L | HOT-PATH (per-frame nextFrame), INIT-ORDER (Switch namespace at engine bring-up) |
| 24 | Cube-RT-readback rescue | brewser-specific | runtime-side-only | **already** | ALREADY-ZERO-DELTA | 0 | HOT-PATH (per framebufferTexture2D — rare) |
| 31 | Three.js WebGLState cache desync | brewser-specific workaround + upstream-candidate engine fix | demo-side workaround + OPEN engine ask | **partial** for engine ask (one of the proposed designs is a `nx.__afterExitBracket` seam — exactly the runtime-hook shape). The workaround itself lives in demo code | UPSTREAM-PR (engine seam), or MOVE-TO-RUNTIME (runtime enumerates live contexts and resets on bridge exit) | M | HOT-PATH (per bridge exit), OPEN, CONTRACT (bridge lifecycle) |
| 34 | Audio createX throw-stubs | upstream-candidate (engine ask OPEN) | engine-TS ask + runtime polyfill shipped | **already** for the workaround — polyfill IS a `globalThis` injection at brewser-runtime install; the engine ask is the upstream cure | UPSTREAM-PR (engine ask); polyfill is the runtime workaround while upstream is broken | S | INIT-ORDER (polyfill must run before app code calls `createMediaElementSource`), OPEN |

### Ambiguous classifications, flagged per task rule

- **#19** — pure runtime-side, but OPEN and 2.G.4-tagged. Rule says
  ambiguous entries STAY. Additionally, the task brief explicitly
  forbids moving #19. STAYS.
- **#20** — engine ask is upstream-candidate; workaround lives in
  demo-side rgbe-loader.js (not brewser-runtime). Entry captures
  the engine ask, not the workaround. STAYS.
- **#31** — engine ask exists (deferred); workaround lives in
  brewser-apps demo file. Entry captures the engine ask. STAYS.
- **#34** — engine ask exists; workaround lives in brewser-runtime
  polyfill. The entry text spans both. STAYS with cross-ref to
  RUNTIME_SHIMS.md pointing at the polyfill detail.

### Risk-flag summary

- **HOT-PATH-blocking-a-move**: #6, #7, #17 — save/restore or
  per-frame bracket. MOVE-TO-RUNTIME disqualified.
- **INIT-ORDER-blocking-a-move**: #8, #9, #22, #34 (polyfill) — the
  fix has to exist before something else runs. Injection *can* work
  for #34 (polyfill installs before app code); #8/#9 must precede
  engine-TS module bodies, ruling injection out for them.
- **CONTRACT-holding-in-fork**: #6, #7, #14, #15, #17 — these
  formalize the bridge/coexistence contract. UPSTREAM-PR the
  contract, don't outsource it to runtime.
- **OPEN**: #19, #20, #31, #34.

---

## Phase 3 — Evaluate the injection proposal

The proposal: a brewser-runtime "engine augmentation layer" that
patches the engine's exported classes at runtime-install time,
retiring entries that today ship as edits to engine files.

### Design (paper only)

**Single install point.** A new module
`brewser-runtime-v8/src/polyfills/engine-augmentation.ts`, imported
at the top of `brewser-runtime-v8/src/index.ts` before
`web-audio-stubs`, before `canvas-runner`, before any other engine
touch.

**Ordering constraint.** Must run:
1. After the engine runtime bundle has finished evaluating (so the
   classes exist).
2. Before any application code touches those classes.
3. Before `web-audio-stubs.ts` if it's the same phase (to avoid
   patching a polyfilled surface).
4. Before `canvas-runner.ts::installCubeRouting` (so the shim wraps
   the engine surface that the augmentation exposed, not an
   unmodified engine surface).

**Idempotency + guard rules (learned from #34's polyfill bug).**

The #34 polyfill had `if (typeof proto[name] === 'function') return;`
as a guard — intended to skip overriding real implementations, but
throw-stubs *are* functions, so the polyfill silently no-op'd
against the very functions it existed to patch. Applied lessons:

- **Do NOT gate on `typeof === 'function'`** — stubs count. Instead,
  gate on a marker: `if (proto[name][BREWSER_AUGMENTED]) return;`
  where the marker is set only by our augmentation. This way
  re-running the augmentation is a no-op, and a legitimate upstream
  implementation is detectable by its absence of the marker (we can
  choose to defer to it or override anyway per method).
- **Do NOT gate on "does the method exist"** — engine `undefined`
  methods are the exact case we want to augment for missing bindings.
- **Every override records the pre-augmentation shape** (attach a
  `__brewserOriginal` back-ref) so a future refactor can un-hook.
- **Log-once install banner** at level `[engine-aug]` so a future
  regression tell exists.
- **Never patch prototypes for classes not yet loaded** — if
  `WebGLRenderingContext` isn't defined at augmentation time,
  augment lazily via a `getContext` wrap. This is exactly the shape
  #34's polyfill missed and cube-route-shim uses successfully.

### Entries the injection layer WOULD retire

| # | Retire? | How |
|---|---|---|
| 1 | yes | override `Image.prototype.__lookupSetter__('src')` to call `globalThis.fetch` — replaces the engine's `import { fetch }`-frozen closure |
| 2 | yes | same shape as #1 for `Audio.prototype.src` |
| 3 | yes | same shape as #1 for `Video.prototype.src` |
| 34 | **already retired by the runtime-side polyfill** (though the entry captures the engine ask, which stays OPEN as an upstream candidate) |
| 9 | partial | can add missing constants at install time via bulk `Object.defineProperties`. But needs the #8-safe shape or reintroduces the JIT crash. Also races with engine-TS module-body reads |
| 20 | partial | wrap `WebGLRenderingContext.prototype.texImage2D` and reverse-row before dispatch. Costs one function-call hop per texImage2D — acceptable |

### Entries the injection layer CANNOT touch

| # | Why not |
|---|---|
| 4 | native compositor over `display_buffer`, no JS surface |
| 5 | EGL context creation runs before any JS |
| 6 | GL state save/restore is a C-level primitive |
| 7 | WebGL context factory is native |
| 8 | fix must precede engine-TS module-body constants installation; injection at brewser-runtime load happens after that |
| 10 | translate could be runtime-side, but the C helpers are already applied atomically per WebGL call — moving them to JS trades a compiled 30-line function for a per-call V8 stack roundtrip. UPSTREAM-PR wins |
| 11 | Mesa-Nouveau workaround with hand-tuned constants parse; can be runtime-side but has no reduction win — moves LOC from source/ to brewser-runtime/, doesn't reduce fork delta of any type |
| 13 | FT_Face is a native shared-object, no JS surface for cross-context invalidation |
| 14, 15 | native factory + install_methods, no JS surface |
| 16, 17 | GL state contract, C-only |
| 22 | needs `nx_media_*` FFmpeg bindings, native only |
| 31 | needs a bridge-lifecycle hook that doesn't exist yet; the engine ask includes such a hook as an option |

### Alternative: upstream extension-points

Instead of injecting on top of the current engine surface, PR upstream
to add extension seams that our fork can register into with zero
downstream churn:

- **Scheme registry hook**: expose `$.registerFetchScheme(name, handler)`
  or make Image/Audio/Video use `globalThis.fetch` — retires #1, #2, #3
  as a class. **Effect**: three edits to engine-TS files become one
  runtime-side call. Zero engine fork delta.
- **After-frame hook for bridge lifecycle**: expose `$.onAfterFrame(cb)`
  or `$.onWebGLBracketExit(cb)` — retires the mechanism half of #31
  and makes state-contract extensions upstream-friendly. Combined
  with the existing coexistence primitives this makes the whole 2.B
  bracket much easier to upstream.
- **GL_CONSTANTS extension seam**: expose a way for embedders to
  supply extra GL constants without patching the engine-TS constants
  loop — retires #9's engine-TS delta AND provides an
  extension-friendly WebGL context for other embedders that need
  post-WebGL1 constants.
- **PixelStore hook** on texImage2D: expose a texImage2D pre-dispatch
  callback or make `UNPACK_FLIP_Y_WEBGL` a first-class engine flag —
  retires #20.

### Recommendation

**Adopt narrowly for #1/#2/#3/#20/#34, prefer extension-point
upstreaming for the class.** Concretely:

1. **File the extension-point PRs first** — scheme registry, GL
   constants seam, pixel-store honor. If TooTallNate accepts any of
   them, that entry drops to zero engine delta permanently without
   needing our injection layer.
2. **While the PRs are in flight (or if rejected)**, ship the
   engine-augmentation module in brewser-runtime-v8 that retires
   #1/#2/#3 today. That drops three engine-TS files off the modified
   list (image.ts, audio.ts, video.ts → total ~-73 additions across
   three upstream files) and moves the fix to the layer where it
   BELONGS by our own DISPOSITION POLICY (which prefers
   brewser-runtime over engine edits).
3. **Do not attempt to inject** #8, #9, #10, #11, #13, #22, #17, #14,
   #15, #6, #5, #7 — either INIT-ORDER, HOT-PATH, or CONTRACT
   forbids it, and the reduction win is small or negative.
4. **Cross-cutting rule going forward.** Any future engine ask
   should get a mandatory "can this be an extension point instead"
   review before landing as an engine edit. This survives the pull
   cycle better than any injection layer.

Verdict: `adopt narrowly` (retire #1/#2/#3 immediately in
brewser-runtime), `pursue extension-point upstreaming` (as the
strategic path for #1/#2/#3, #9, #20, #31 as a class).

**Reject** the wholesale-migration form of the proposal — moving
engine-C++ patches to JS in bulk trades LOC-in-source-tree for
per-frame JS overhead, and the entries that would benefit most
(hot-path bridge, contract-holding save/restore, native
compositor) cannot be moved regardless.

---

## Phase 4 — Upstream PR batch plan

All upstream-candidate entries are `not-submitted`. Ordering by
(likelihood × entries retired ÷ effort). Format each PR to
frame as general-embedder benefit; strip brewser-specific
scaffolding before submitting.

### PR-A — Image/Audio/Video call-time fetch deferral (top-1)

- **Entries covered.** #1, #2, #3.
- **Files touched (upstream).** `packages/runtime/src/image.ts`,
  `audio.ts`, `video.ts`. Small, ~10 lines each.
- **Framing.** "Allow embedders to extend the scheme registry via
  `globalThis.fetch`. The `Image.src`/`Audio.src`/`Video.src` setters
  today capture `./fetch/fetch` at module-import time, freezing the
  fetch implementation before any embedder installs its extended
  wrapper. Move the lookup to call-time."
- **Reason it's likely to land.** Bug-shaped (silent scheme rejection
  for embedder-registered schemes); one-line change per file; no
  behavior change for existing consumers.
- **Genericization required.** None — the change is already generic;
  our fork's brewser-runtime code isn't mentioned.
- **Effort.** S. **Retires.** 3.

### PR-B — WebGL1 v1 constants + EXT_sRGB/HalfFloat translate (top-2)

- **Entries covered.** #9, #10.
- **Files touched.** `source/webgl.cc` (translate helpers),
  `packages/runtime/src/canvas/webgl-rendering-context.ts` (constants
  additions).
- **Framing.** "Add ES3-core sized internalformat constants +
  translate WebGL1 EXT_sRGB unsized enums + normalize
  HALF_FLOAT_OES→HALF_FLOAT so post-r150 Three.js works on a v1
  context."
- **Reason it's likely to land.** Three.js is common; the WebGL1
  spec-conformant translate applies to every embedder.
- **Genericization required.** Ensure `packages/runtime/src/canvas/
  webgl-rendering-context.ts` isn't ours-only. Upstream nx.js does
  not currently expose v1; PR either lands the v1 context surface
  in the process, or targets a future upstream v1 branch.
- **Effort.** M. **Retires.** 2. **Depends on.** #7 or an
  upstream WebGL1 path — PR discussion likely.

### PR-C — canvas.cc: pin FT_Face char_size per text-op (top-3)

- **Entries covered.** #13.
- **Files touched.** `source/canvas.cc`.
- **Framing.** "Two `OffscreenCanvas` contexts sharing a `FontFace`
  can silently corrupt each other's rendered font size via the
  shared `FT_Face.char_size`. Re-pin at the start of
  fillText/strokeText/measureText — negligible cost, prevents the
  cross-context corruption."
- **Reason it's likely to land.** Minimum-repro trivially extractable
  (per the entry — two OffscreenCanvas, one save/font/fillText/
  restore, other fillText at 10 px). Bug is spec-visible.
- **Genericization required.** None.
- **Effort.** S. **Retires.** 1.

### PR-D — skia_gpu ES3 + webgl_bridge coexistence primitive

- **Entries covered.** #5, #6, #7, #14, #15.
- **Files touched.** `source/skia_gpu.{cc,h}`, new
  `source/webgl_bridge.{cc,h}`, `source/webgl.cc` (v1 surface),
  `packages/runtime/src/canvas/webgl2-rendering-context.ts` +
  `webgl-rendering-context.ts` (v1/v2 factories),
  `packages/runtime/src/screen.ts` (routes).
- **Framing.** "Coexisting Skia + WebGL on a single EGL/GLES3 context.
  Adds the load-bearing state save/restore primitive as a public
  contract; adds tenant offscreen FBO + Skia composite via
  `SkImages::BorrowTextureFrom`; adds v1/v2 context factories."
- **Reason it might land.** Architectural improvement over upstream's
  "WebGL OR Skia" model; solves cases upstream can't (WebGL over a
  Skia-driven canvas).
- **Reason it might not.** Large architectural PR; the FUNCS[] split
  discipline is nontrivial; upstream's WebGL is currently v2-only.
- **Genericization required.** Strip `enableGpuBridgePrototype` /
  `setBridgeAutoFlush` no-op hooks (they exist only for
  canvas-runner.ts back-compat).
- **Effort.** L. **Retires.** 5. **Depends on.** Phase 2.G hardware
  gate green.

### PR-E — Switch.VideoDecoder V8 restoration

- **Entries covered.** #22.
- **Files touched.** New `source/video-decoder.{cc,h}`, wiring in
  `source/main.cc`, `packages/runtime/src/switch/video-decoder.ts`,
  `packages/runtime/src/switch/index.ts`, `packages/runtime/src/$.ts`.
- **Framing.** "Restore the QuickJS-era-1.0.0-beta.5 public
  `Switch.VideoDecoder` API on the V8 fork. Thin V8 bindings over
  the already-ported `nx_media_*` portable pipeline."
- **Reason it's likely to land.** API-parity restoration, not new
  design. Upstream previously shipped this surface.
- **Effort.** L (the code is written; framing + PR review is the
  cost). **Retires.** 1 with a large-LOC-per-entry win.

### PR-F — engine-TS JIT-safe defineProperties pattern

- **Entries covered.** #8.
- **Files touched.** `packages/runtime/src/canvas/webgl-rendering-
  context.ts`, `webgl2-rendering-context.ts`.
- **Framing.** "V8/aarch64 JIT tier-up crashes on tight `for
  (const [k, v] of Object.entries(BIG_OBJ))` + `Object.defineProperty`
  loops at scale. Bulk `Object.defineProperties(target, descriptors)`
  produces byte-identical property shapes with a predictable install
  loop V8 handles cleanly."
- **Reason it's likely to land.** No behavior change; robustness
  improvement.
- **Effort.** S. **Retires.** 1.

### PR-G — Audio createX no-throw stubs

- **Entries covered.** #34.
- **Files touched.** `packages/runtime/src/audio/base-audio-context.ts`,
  `audio-context.ts`.
- **Framing.** "Throwing from `createXxx` after `new AudioContext()`
  has succeeded leaves the context half-init (audrv voice held,
  never closed). Return a benign passthrough Gain node or wire
  minimal impls."
- **Reason it's likely to land.** Bug-shaped; simple diff.
- **Effort.** S. **Retires.** 1.

### PR-H — UNPACK_FLIP_Y_WEBGL honor (later)

- **Entries covered.** #20 (engine ask).
- **Deferred until.** Compensation-sweep of the codebase completes
  (blast-radius risk called out in the entry).
- **Framing.** "Honor the WebGL `UNPACK_FLIP_Y_WEBGL` pixel-store
  flag for typed-array texImage2D uploads."
- **Effort.** M. **Retires.** 1.

### PR-I — WebGLState cache-desync engine seam (later)

- **Entries covered.** #31 (engine ask).
- **Deferred until.** Blast-radius sweep of ping-pong demos names
  the concrete engine-side design.
- **Effort.** M. **Retires.** 1.

### Top-3 to file first

Based on `(likelihood × retirement) ÷ effort`:

1. **PR-A** (Image/Audio/Video fetch deferral) — 3 entries × high
   likelihood ÷ small effort. Best ratio in the batch.
2. **PR-F** (JIT-safe defineProperties) — 1 entry × very-high
   likelihood (robustness fix, no behavior change) ÷ small effort.
3. **PR-C** (canvas.cc font-size pin) — 1 entry × high likelihood ÷
   small effort, with a trivial extractable repro.

**Rationale.** These three ship the same day, remove the most
rebase-pain per hour spent, and each is a small enough PR that
review is short. They also seed the relationship for the larger
PR-B / PR-D that follow.

---

## Phase 5(b) — Ledger split log

Mechanical moves per the task rules. One global number space
preserved across the three files; no renumbering.

### STAYS in NXJS_PATCHES_NEEDED.md (engine-only + open engine asks)

- **#1, #2, #3** — engine-TS deltas in image.ts / audio.ts / video.ts.
- **#4** — native cursor binding (DEFERRED, still an open engine ask).
- **#5** — skia_gpu ES3 + accessors (native).
- **#6** — webgl_bridge state save/restore + tenant FBO (native).
- **#7** — WebGL1 context factory (native + engine-TS).
- **#8** — JIT-safe defineProperties (engine-TS).
- **#9** — v1 ES3 sized constants (engine-TS).
- **#10** — WebGL1 EXT_sRGB + HalfFloat translate (native).
- **#11** — PMREM r184 FS replacement (native, fork-only).
- **#13** — canvas.cc font-size pin (native).
- **#14** — WebGL2 context factory (native + engine-TS).
- **#15** — v1/v2 FUNCS[] split (native).
- **#16** — passive state probe (native, fork-only diagnostic).
- **#16-ACTIVE** — active probe design (sub-section of #16).
- **#17** — nx_gl_state_snap_t extension (native).
- **#19** — v2 cube-routing applicability. RUNTIME-SIDE but OPEN
  and 2.G.4-tagged. Task brief forbids moving; classified as
  ambiguous per the STAYS-default rule. Cross-ref added.
- **#20** — engine ask OPEN. Demo-side workaround in brewser-apps
  (NOT brewser-runtime). Entry captures the engine ask. STAYS.
- **#22** — VideoDecoder V8 port (native + engine-TS).
- **#31** — engine ask OPEN. Workaround is demo-side. Cross-ref
  added.
- **#34** — engine ask OPEN. Runtime polyfill fills the gap; the
  entry's engine-side ask stays. Cross-ref added pointing to
  RUNTIME_SHIMS.md #34.

### MOVED to brewser-runtime-v8/RUNTIME_SHIMS.md (verbatim)

- **#12** — samplerCube→sampler2D routing layer. Runtime-side only
  (`brewser-runtime-v8/src/scripts/cube-route-shim.ts`). Zero engine
  delta.
- **#18** — cube-route-shim per-method capability guards. Runtime
  refinement to #12.
- **#21** — sampler2DShadow rewrite shim.
  `brewser-runtime-v8/src/scripts/shadow-route-shim.ts`. Zero engine
  delta.
- **#24** — Cube-RT-readback rescue. Extension to
  `cube-route-shim.ts`. Zero engine delta.

### MOVED to NXJS_PATCHES_ARCHIVE.md

- **#17-superseded** — original PROPOSED template for #17. Superseded
  by the SHIPPED #17 entry. Archived verbatim for historical
  reference.

### Tombstones left behind

Each moved entry has a one-line tombstone in the engine ledger:
`## #N — MOVED → <path> (#N)`. Tombstones preserve the
one-global-number-space invariant so future entries can be numbered
starting at #35 without conflict.

### Cross-references added

- Engine ledger #31 → RUNTIME_SHIMS.md (workaround detail).
- Engine ledger #34 → RUNTIME_SHIMS.md #34 polyfill entry.

### DISPOSITION POLICY

Preserved unchanged in NXJS_PATCHES_NEEDED.md. Copied (not moved)
into the RUNTIME_SHIMS.md header per task rules.

---

## Recommendation — ordered set of actions

The goal is to reduce **modified-upstream-file count** (currently 15)
with the least effort. Ordered:

1. **File PR-A + PR-F + PR-C today.** These are the highest-ratio
   PRs. Retires #1, #2, #3, #8, #13 upstream — after merge, five
   entries drop off; three of them (image.ts, audio.ts, video.ts)
   also drop three files from the modified-upstream list.
   **Effect: 15 → 12 modified upstream files.**

2. **Ship the brewser-runtime `engine-augmentation` module for
   #1/#2/#3 in the interim.** Even if PR-A takes months, we can
   move the fix out of engine-TS today. Costs a small `Object.
   defineProperty(Image.prototype, 'src', …)`-style shim that
   defers to globalThis.fetch. This unblocks the upstream pull
   pain independently of PR-A landing.
   **Effect on rebase pain: three engine-TS files stop drifting.**

3. **Batch PR-F + PR-G small PRs** as fast follow-ups. Retires #8
   and #34 (engine ask). Small diffs, likely straightforward review.

4. **Prepare and file PR-B (v1 constants + translate).** Medium
   effort; retires #9 + #10. Engine-side webgl.cc churn is the
   fork's biggest rebase target, so any of that file we can move
   upstream is high-leverage.

5. **Prepare PR-D (coexistence primitive) once Phase 2.G is
   hardware-green.** This is the architectural upstream — retires
   #5/#6/#7/#14/#15 as a class if accepted. Even if only #5/#6
   land, we get the bridge primitive upstreamed and future pulls
   don't fight over `skia_gpu.cc`.

6. **Prepare PR-E (VideoDecoder restoration).** Parity restoration
   framing; likely straightforward review even at L effort.

7. **Defer PR-H (#20) and PR-I (#31) engine asks** until the
   blast-radius sweeps their entries call for are done. Their
   workarounds are shipping in demo/runtime code today; they don't
   block anything.

**Never** attempt to migrate #4, #11, #13, #16, #17 out of the
native fork — HOT-PATH / CONTRACT / driver-specificity make them
correct where they are.

### Verdict on the globalThis injection proposal

**Adopt narrowly** (retire #1/#2/#3 in brewser-runtime today; the
polyfill for #34 already exists) — **in service of upstream
extension-point PRs** (PR-A, and the future PRs that expose
extension seams). The injection layer is a stopgap that unblocks
rebase pain while the upstream PRs are in flight; it is not a
substitute for upstreaming, because:

- The best-case-injection-eligible entries (#1/#2/#3) are also the
  ones most likely to land upstream cleanly. Doing both in parallel
  is cheap.
- The remaining entries (#8, #9, #10, #11, #13, #14, #15, #22, native
  bridge) cannot be moved at all, so the injection layer would only
  handle a small tail regardless. The reduction from moving 3 of ~24
  entries is a ~15% dent in modified-upstream-file count, not the
  architectural win the proposal implicitly promises.
- The alternative — upstream extension-points — CHANGES the shape
  of the problem for future patches, not just today's list. If
  TooTallNate accepts a `registerFetchScheme` or `onBracketExit`
  hook, that eliminates a whole class of future fork-only edits,
  not just the three currently on the list.

**Reject** the "move everything to runtime" reading of the proposal.
The C++/JS boundary was chosen well; wholesale migration trades
compiled hot-path code for per-frame JS overhead and doesn't reduce
the entries that actually cause the most rebase pain.
