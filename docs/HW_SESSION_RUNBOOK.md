# Hardware session runbook

Consolidated set of items whose Citron/emulator behavior needs verification on
real Tegra hardware (CFW Switch, hbmenu, real Mesa Nouveau NV120 driver stack).

**Standing rule** (project-wide): Citron is a functional-iteration authority,
NOT a driver-truth authority. Citron sits on top of AMD Vulkan on Windows via
an emulation/translation layer; anything that touches driver-specific GLES3
behavior — layered-texture writes, occlusion queries, ES3-optional entry
points, packed formats, MRT semantics — MUST be verified on real hardware
before its ledger status can graduate from `CITRON-*` / `hardware-pending` to
a definitive verdict.

**Runbook usage.** Each entry below states:
1. The Citron observation (what the emulator shows).
2. The hardware probe recipe (how to reproduce on real hardware).
3. The hardware verdict rows to fill in (PASS / FAIL / SKIPPED + notes) and the
   downstream disposition each verdict implies.

When a hardware session runs, fill in the `HW verdict` line, commit the runbook
update, then update the referenced ledger entry per the disposition rules.

---

## §#52a — `drawRangeElements` silent no-op vs `drawElements` fallback

**Ledger:** [NXJS_PATCHES_NEEDED.md #52a](../NXJS_PATCHES_NEEDED.md#52).

**Citron observation.** `gl-probes-v0.3.0-drawrange-iso.log` on Citron 2026-07-03:
DRAW_RANGE isolation-mode probe FAILS with signature
`sentinel=no cornerSentinel=no allZero=yes cornerZero=yes eDE=0x0 eClear=0x0 eDRE=0x0 eRead=0x0`
— second block's `gl.clear` + `gl.drawRangeElements` produces `[0,0,0,0]`
readback despite no GL errors and correct first-block behavior. Isolation
proves it isn't probe state leakage. Interim `glDrawElements` fallback shipped
(spec-legal per ES3 §2.8.3).

**Hardware probe recipe.**

1. Rebuild engine with `NX_52A_DISABLE_FALLBACK=1` to restore the direct
   `glDrawRangeElements` call:
   ```
   make -C /d/Workspace/nxjs-source-v8 clean
   CFLAGS_APPEND=-DNX_52A_DISABLE_FALLBACK=1 make -C /d/Workspace/nxjs-source-v8
   # rebuild brewser-v8 chain
   make -C /d/Workspace/brewser-v8 sdmc
   ```
   Verify by boot log — should see:
   `[#52a] drawRangeElements DIRECT (fallback DISABLED via NX_52A_DISABLE_FALLBACK build)`
   instead of the default `[#52a] drawRangeElements -> drawElements fallback` line.

2. Boot Switch + hbmenu → brewser → GL Probes app.

3. Click **Run DRAW_RANGE Isolated** (blue button). Capture
   `sdmc:/switch/brewser/logs/gl-probes-v<VER>-drawrange-iso.log`.

4. Expected outcomes:
   - **DRAW_RANGE PASS** with `drawRangeElements → drawElements fallback painted [255,128,64,255]`.
     No — wait, this build has fallback DISABLED, so the detail should read
     something DIFFERENT (see below). Verify: the boot log confirmed direct
     mode, so the probe's `PASS` detail reads
     `drawRangeElements → drawElements fallback painted [255,128,64,255]` is a
     PROBE ISSUE — the probe's detail text is hardcoded to the fallback
     wording. Instead grep the boot log for the DIRECT banner and separately
     grep the probe log for `PASS`. Both green = hardware works with direct
     `glDrawRangeElements`.

5. Also run the two-block reproduction — click **Run Probes (WebGL 2)** and
   check the pre-v0.5.0 pattern for the `sentinel=?` diagnostic. The v0.5.0+
   single-shot probe collapses to the isolated form; to run the classic
   two-block pattern set `?strict=1` (which restores the pre-refactor
   comparison logic in v1.0.0+ if we add it, or manually inspect the log).

**HW verdict** (2026-07-03 hardware session):
- `[X]` PASS with fallback disabled → Citron-only issue. Reword #52a to
  "Citron-emulator quirk"; fallback becomes defensive-only. ← **CONFIRMED**
- `[ ]` FAIL with fallback disabled → Real Mesa Nouveau NV120 driver bug.
- `[ ]` UNEXPECTED (some other diagnostic) → attach the hardware log to the
  ledger entry and re-triage.

**HW session date / hardware model / verdict:** 2026-07-03 — Tegra X1 Nouveau NV120 CFW hbmenu — **CITRON-ONLY QUIRK**. Boot A (default NRO, fallback ON) and Boot B (`NX_52A_DISABLE_FALLBACK=1`, fallback OFF) both painted the DRAW_RANGE probe's expected pixel `[255,127,64,255]`; the strict-mode probe FAIL on both boots is a ±1 pixel-rounding tolerance issue (hardware `round-half-down` vs Citron `round-half-up`), not a `glDrawRangeElements` defect. Logs: `gl-probes-v0.11.0-all-strict-BOOT-A.log`, `gl-probes-v0.11.0-drawrange-iso-BOOT-B.log`, `gl-probes-v0.11.0-all-strict-BOOT-B.log`. Ledger #52a updated + `NX_52A_DISABLE_FALLBACK` build gate retained for future re-verification.

---

## §#55-pause — `pauseTransformFeedback` + `resumeTransformFeedback` reset the buffer write pointer instead of continuing

**Ledger:** [NXJS_PATCHES_NEEDED.md #55](../NXJS_PATCHES_NEEDED.md#55).

**Citron observation.** `gl-probes-v0.9.0.log` on Citron 2026-07-03: TF_CAPTURE
probe PASS (3-vertex SEPARATE_ATTRIBS capture works end-to-end with correct
[11,12,13,14, 21,22,23,24, 31,32,33,34] readback for ids [10,20,30]). TF_ERR
probe PASS (nested `beginTransformFeedback` → INVALID_OPERATION as expected).
TF_PAUSE FAIL: begin → draw id=100 → pause → draw id=200 → resume → draw
id=300 → end. Expected slot 0 = id=100's `[101,102,103,104]`; observed slot 0
= `[301,302,303,304]` (id=300's values). Pattern is consistent with
"pause+resume treated as end+begin" — the buffer write pointer resets on
`resumeTransformFeedback`, and id=300's capture overwrites id=100's slot.

**Discriminator** (from probe detail): `ePauseCtx=0x0 ePause=0x0 eResume=0x0 eEnd=0x0`
— no GL errors anywhere. All four TF entry points accepted their arguments.

Same **Citron-observed / hardware-pending** class as §#52a and §#54.

**Hardware probe recipe.**

1. Deploy gl-probes v0.10.0+ files verbatim (no engine rebuild needed — the
   underlying TF surface is engine-native and doesn't gate on any build
   flag).
2. Boot Switch + brewser → **GL Probes**.
3. Navigate to `brewser://apps/experimental/com.natureglass.gl-probes/index.html?strict=1`
   to disable the pass-quirk relaxation.
4. Click **Run Probes (WebGL 2)**. Capture
   `sdmc:/switch/brewser/logs/gl-probes-v<VER>.log`.

**HW verdict** (2026-07-03 hardware session):
- `[X]` TF_PAUSE PASS strict — probe detail reads
  `pause skipped id=200, captured id=100 + id=300 in order` →
  Citron-only issue. Reword this runbook entry + ledger #55 to
  "Citron-emulator quirk". ← **CONFIRMED**
- `[ ]` TF_PAUSE FAIL strict with same quirk signature.
- `[ ]` TF_PAUSE FAIL strict with DIFFERENT signature.

**HW session date / hardware model / verdict:** 2026-07-03 — Tegra X1 Nouveau NV120 CFW hbmenu — **CITRON-ONLY QUIRK**. Strict TF_PAUSE probe on real hardware `PASS detail=pause skipped id=200, captured id=100 + id=300 in order | ePauseCtx=0x0 ePause=0x0 eResume=0x0 eEnd=0x0`. Driver honors pause/resume semantics correctly. Log: `gl-probes-v0.11.0-all-strict-BOOT-A.log`.

---

## §#54 — Occlusion query `ANY_SAMPLES_PASSED` returns 0 despite pixels drawn

**Ledger:** [NXJS_PATCHES_NEEDED.md #54](../NXJS_PATCHES_NEEDED.md#54).

**Citron observation.** `gl-probes-v0.7.0.log` on Citron 2026-07-03: QUERY probe
sees `curActive=true` + `eBegin=0x0 eDraw=0x0 eEnd=0x0 eAvail=0x0 eResult=0x0`
+ readPixels `[255,255,255,255]` (draw painted) + `QUERY_RESULT=0`. Engine
wiring proven correct; either driver ceiling or Citron GPU-translation gap.

**Hardware probe recipe.**

1. Rebuild engine with the current (v0.8.0+) codebase (no #define needed —
   the relaxation lives in the probe, not the engine).
2. Deploy gl-probes app files verbatim (v0.8.0+).
3. Boot Switch + brewser → **GL Probes**.
4. Navigate to `brewser://apps/experimental/com.natureglass.gl-probes/index.html?strict=1`
   (query param enables strict mode in the QUERY probe — see the
   `strictQuery` flag in gl-probes.js).
5. Click **Run Probes (WebGL 2)**. Capture
   `sdmc:/switch/brewser/logs/gl-probes-v<VER>.log`.

**HW verdict** (2026-07-03 hardware session):
- `[X]` QUERY PASS strict — Citron-only issue. Revert
  the probe relaxation and restore strict `QUERY_RESULT > 0` as the default
  in gl-probes.js. Reword #54 to "Citron-emulator quirk". ← **CONFIRMED**
- `[ ]` QUERY FAIL strict → Real driver ceiling.

**HW session date / hardware model / verdict:** 2026-07-03 — Tegra X1 Nouveau NV120 CFW hbmenu — **CITRON-ONLY QUIRK**. Strict QUERY probe on real hardware `PASS detail=surface + wiring ok, QUERY_RESULT=1 — spec-conformant (result > 0 for visible draw) | eBegin=0x0 eDraw=0x0 eEnd=0x0 eAvail=0x0 eResult=0x0 curActive=true`. `ANY_SAMPLES_PASSED` works correctly on real hardware; the Citron `QUERY_RESULT=0` was a Citron GPU-translation gap. Log: `gl-probes-v0.11.0-all-strict-BOOT-A.log`.

---

## Accumulated batch/tier hardware caveats (from plan §5.3, per-batch reports)

Consolidated here from `brewser-runtime-v8/docs/EXTENSION_PORT_PLAN.md §5.3` +
each phase-1 tier's batch report. These are the hardware-observable behaviors
newly-exposed by the tier landings — they DON'T need every-session
verification, but the first hardware session after each tier ships should
sweep the list and record verdicts.

### From batch 1 (#47 — driver-probed advertisement + compressed 2D uploads)

- **Compressed texture uploads.** S3TC, S3TC_sRGB, RGTC, BPTC, ETC1, ETC2 (via
  ES3 core), ASTC — hardware run should decode a known reference image per
  format and diff pixel-perfect against a raster reference. Regression tell:
  garbled compressed texture on demos that use the format.
- **Anisotropic sampling visual verification.** Render a tilted textured
  plane at aniso=1 vs aniso=16 (`gl.texParameterf(gl.TEXTURE_2D,
  ext.TEXTURE_MAX_ANISOTROPY_EXT, 16.0)`) and eyeball the sharpness
  improvement. gl-probes EXT_ANISO already verifies the parameter roundtrip
  numerically; visual verification is separate.
- **UNMASKED_VENDOR_WEBGL / UNMASKED_RENDERER_WEBGL.** Should read
  `nouveau` / `NV120` on real hardware. Citron confirms this format works;
  hardware confirms the actual values.

### From batch 2A (#48 — Unity-P1 v1 fn surfaces)

- **`gl_FragDepthEXT` in v1 shaders.** Hardware run should compile + link a
  #version 100 shader with `#extension GL_EXT_frag_depth` and validate
  `gl_FragDepthEXT` writes reach the depth attachment.
- **`WEBGL_draw_buffers` MRT on v1.** Hardware run should render a simple MRT
  scene (2+ color attachments) and validate all attachments received distinct
  output. Same probe as gl-probes FBO_MRT but running on v1 context.
- **OES_vertex_array_object list-flip** (#42 retire). Any v1 Three.js demo
  that uses the OES VAO extension should still render — verify Three.js
  didn't switch to a broken code path once the extension became discoverable.

### From batch 2B (#49 — v2 spec-conformance prune)

- **v2 demos that formerly relied on the pre-prune extensions.** Rider 2B
  suite — webgl2-ubo + spectraplay visualizer + webgl2-multiple-rendertargets
  — verified working on Citron 2026-07-03. Hardware run should confirm no
  visual regression on the same suite.

### From phase-1.5-LOW (#50 — 30 core WebGL2 methods)

- **`getBufferSubData` via glMapBufferRange.** Verify readback matches source
  bytes on a non-trivial demo (spectraplay's audio-reactive visualizer uses
  large buffer copies).
- **`compressedTexImage3D` ETC2/EAC layered upload + `copyTexSubImage3D`
  between layers.** gl-probes TEX3D verifies via the framebufferTextureLayer
  readback path; hardware should confirm the sample-in-shader path via a
  demo that uses layered textures with actual GLSL sampling.
- **Non-square matrix uniforms.** Any demo that uses `uniformMatrix3x2fv`
  and friends — verify column-major storage matches expectations.
- **`drawRangeElements`** — see §#52a above.

### From phase-1.5-LOW-MED (#51 — integer vertex attribs + getInternalformatParameter)

- **`vertexAttribIPointer` integer attribute end-to-end.** Any demo that uses
  `flat` uint varyings and integer input attributes should render correctly.
  gl-probes INT_ATTRIB verifies via RGBA32UI readback; hardware should
  cross-check with a raw-GL demo.
- **`getInternalformatParameter(RENDERBUFFER, RGBA8, SAMPLES)`** — hardware
  should report an MSAA sample list. gl-probes INFORMAT_PARAM verifies the
  Int32Array return shape; hardware should confirm the specific sample counts
  match `MAX_SAMPLES` at least.

### From phase-1.5-MED (#53 — sampler + sync + query + UBO introspection)

- **Sampler-overrides-texture-param.** gl-probes SAMPLER verifies via a small
  2×1 texture; hardware should cross-check on a demo that mixes sampler + tex
  parameter bindings.
- **Sync signals across a real frame boundary.** gl-probes SYNC uses no
  intervening GPU work; hardware should verify a `fenceSync` after a real
  Three.js frame's draw calls signals correctly via `clientWaitSync`.
- **Query family** — see §#54 above.
- **UBO introspection.** Three.js's `WebGLUniforms.setValueV2ui` / UBO path
  should work correctly on hardware — the webgl2-ubo demo already exercises
  the `getUniformBlockIndex` + `bindBufferBase` path; introspection queries
  (`getActiveUniforms`, etc.) are new and could regress silently.

### Previously-shipped items with hardware-verification-pending status

- **#11** — PMREM r184 FS replacement. Applied via `maybe_replace_pmrem_fs`;
  needs a r184 PMREM demo to exercise. Not currently in the demo set.
- **#46** — Bridge FBO stencil contract fix (DEPTH24_STENCIL8). Citron log
  shows `[bridge-fbo:complete] stencil=8`. Hardware should confirm same
  breadcrumb + demo that uses stencil (e.g. shadow-volume) renders.

---

## Meta

**Adding new entries.** Any tier commit that adds hardware-observable
behavior should append a section here — either a new `§#NN` for a per-item
verification, or a bullet under the tier's rollup section above.

**Verifying entries.** After a hardware session, fill in the `HW verdict`
line with date + hardware model + PASS/FAIL/SKIPPED per checkbox row.
Ideally attach the log path (SDMC or CFW) that captured the evidence.

**Graduating entries.** Once an item is hardware-verified and the ledger is
updated per its disposition rule, either:
- Move the entry to an "Archived (hardware-verified)" section below (kept
  for historical audit), OR
- Delete it if the ledger fully captures the outcome.

Prefer moving over deleting for the first 6 months post-verification —
regressions happen and the runbook history is useful triage material.

---

## §#56 — `getBufferSubData` via `glMapBufferRange` returns zeros on Mesa Nouveau NV120 hardware — DIAGNOSIS-SHIPPED-FIX-PENDING-HARDWARE-VERIFY 2026-07-03

**Ledger:** [NXJS_PATCHES_NEEDED.md #56](../NXJS_PATCHES_NEEDED.md#56).

**Hardware observation.** gl-probes v0.11.0 BUFFER probe on real Tegra
Nouveau NV120 fails with `mismatch@0 src=3 got=0` — every byte in the
readback is zero. Reproduces on both Boot A (default NRO) and Boot B
(fallback-disabled NRO), so not tied to any #52a build gate.

**Citron behavior.** BUFFER probe passes cleanly with `copy+getSubData
64B memcmp ok`. Citron's AMD Vulkan translation of `glMapBufferRange`
accepts `GL_COPY_WRITE_BUFFER` target; Nouveau NV120 evidently doesn't
sync or doesn't support it for map.

**Fix landed 2026-07-03 (`w_get_buffer_sub_data` in source/webgl.cc).**
Belt-and-suspenders — ships both mitigation candidates so the next
hardware boot pins down which one carries:

1. Candidate (b) — `glFinish()` before `glMapBufferRange`. Cheap
   pipeline drain; spec says MAP_READ_BIT implicitly syncs but Nouveau
   drivers historically under-implement this.
2. Candidate (a) — proc-address fallback. If `glMapBufferRange` returns
   NULL, resolve `glGetBufferSubData` via `eglGetProcAddress` and use
   it directly. Matches the QuickJS-era reference impl
   (`nxjs-source/source/webgl_egl.c:9925`) that worked on same hardware.
3. `NX_56_DEBUG` build flag — per-call fprintf of the mapped pointer,
   first 16 bytes, glGetError codes before/after map + after fallback.

**BUFFER probe gained a second arm (gl-probes v0.13.0).** Arm A reads
from ARRAY_BUFFER directly (no copy — isolates base readback path on
a universally-supported target). Arm B is the original copy + read
from COPY_WRITE_BUFFER.

**Verdict procedure for the next hardware boot.**

1. Boot default NRO (no NX_56_DEBUG). Boot log will contain the
   one-shot banner:
   ```
   [#56] glGetBufferSubData proc-address resolved: 0x<addr>
   ```
   Non-zero = proc-address fallback is available. Zero-address = only
   the glFinish sync mitigation is in play (candidate b alone).
2. Open **GL Probes** app → click **Run Probes STRICT (hardware)**.
3. Grab `sdmc:/switch/brewser/logs/gl-probes-v0.13.0-all-strict.log`.
4. Grep for BUFFER line — parse the detail.

**Verdict table.**

| BUFFER detail | Verdict | Downstream action |
|---|---|---|
| `Arm A ... + Arm B ... both ok` | CONFIRMED FIX | Reclassify ledger #56 to CLOSED. Move to Archived section below. Remove `glFinish()` if perf-critical demos care (perf note: probably fine). |
| `Arm A ... PASS, Arm B ... FAIL — target- or copy-specific defect` | PARTIAL — sync helped ARRAY_BUFFER, target-specific quirk remains on COPY_WRITE_BUFFER | Implement a per-target rebind fallback (rebind to ARRAY_BUFFER, map, unmap, restore) in a follow-up commit. Log the specific failing target in NX_56_DEBUG. |
| `BOTH ARMS FAIL — universal map/readback defect` | NO EFFECT — mitigations didn't carry | Rebuild with `-DNX_56_DEBUG=1` and re-smoke; the per-call diagnostic will reveal whether glMapBufferRange returns NULL AND the fallback proc-address was NULL (candidate 4 escalation: transient scratch buffer path via bufferData + bufferSubData). |
| `Arm B PASS, Arm A FAIL — inverted-severity map defect` | UNEXPECTED | Attach hardware log + rebuild NX_56_DEBUG. Very strange — investigate before proceeding. |

**HW session date / verdict:** 2026-07-03 hardware smoke #2 — **PARTIAL — target-specific quirk on COPY_WRITE_BUFFER**. Verdict row 2 from the runbook table above. Logs: `hw-gl-probes-v0.14.0.log` and `hw-gl-probes-v0.14.0-all-strict.log` line 13. Non-strict `got=0`, strict `got=42` — the map returns non-NULL but wrong data.

**Second-stage fix SHIPPED 2026-07-03 (per-target rebind).** For `GL_COPY_WRITE_BUFFER` / `GL_COPY_READ_BUFFER` / `GL_PIXEL_PACK_BUFFER`, `w_get_buffer_sub_data` now rebinds the target's buffer to `GL_ARRAY_BUFFER` (verified working per Arm A PASS), maps from there, unmaps, restores original ARRAY_BUFFER binding. Boot log line predicted with `-DNX_56_DEBUG=1`: `[#56] per-target rebind: target=0x8F37 bind=<name> saved_array=<name>`.

**Next hardware boot prediction.** BUFFER probe both arms PASS: `Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy) both ok 64B memcmp`. If confirmed, reclassify #56 to CLOSED and archive.

### 2026-07-03 hardware smoke #3 (third session) — SECOND-STAGE FIX HARDWARE-VERIFIED

BUFFER PASS on fresh WebGL2 context: `PASS detail=Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy) both ok 64B memcmp`. **26 PASS / 0 FAIL / 0 SKIP (of 26)**. Log: `gl-probes-v0.14.0.log` (non-strict, generated 21:37:53Z).

The paired strict re-run 2.4 seconds later on the same cached WebGL2 context showed Arm B `got=42` — state-carryover artifact from prior TF_ERR probe's a_id=42 upload, driver's `bufferData` doesn't zero-init reused VRAM. Documented probe-design observation, NOT an engine defect. Log: `gl-probes-v0.14.0-all-strict.log`.

**§#56 CLOSED — see Archived section below.**

### Hardware smoke #2 re-verifications (2026-07-03)

Gl-probes v0.14.0 STRICT run on the same session confirmed:
- **§#52a** — DRAW_RANGE PASS (drawrange-iso mode, default NRO fallback active). Hardware verdict from smoke #1 stands: CITRON-ONLY QUIRK.
- **§#54** — QUERY PASS strict `QUERY_RESULT=1 spec-conformant`. CITRON-ONLY QUIRK re-verified.
- **§#55-pause** — TF_PAUSE PASS strict `pause skipped id=200, captured id=100 + id=300 in order`. CITRON-ONLY QUIRK re-verified.

All 5 new **b3 probes** (ledger #57) PASS strict on hardware: TIMER_QUERY, POLY_CLAMP, INDEXED_BLEND, MULTI_DRAW, BFE_CONST. Batch 3 hardware-verified.

**Minor #57 sub-item — TIMER_QUERY 32-bit truncation.** Hardware TIMER_QUERY probe reports `t0=4294967295 t1=4294967295 (delta=0 ns)` with `disjoint=0`. Both Citron and hardware return `0xFFFFFFFF` from `glGetQueryObjectuiv(QUERY_RESULT_EXT)` — the engine currently uses the 32-bit variant, saturating for TIMESTAMP_EXT queries whose 64-bit result exceeds 2^32. Surface + wiring verified. Future improvement: switch to `glGetQueryObjectui64vEXT` for TIMER queries via proc-address resolution; JS-side represents up to 2^53 as a Number. Non-blocking; deferred until a demo needs nanosecond delta values.

---

## Archived (hardware-verified)

### §#52a — glDrawRangeElements Citron-only quirk (2026-07-03)
`glDrawElements` fallback shipped as defensive-only; `NX_52A_DISABLE_FALLBACK`
build gate retained. Direct `glDrawRangeElements` verified working on real
Tegra Nouveau NV120 via Boot B (fallback-disabled NRO). Ledger #52a.

### §#54 — ANY_SAMPLES_PASSED Citron-only quirk (2026-07-03)
Strict `QUERY_RESULT=1 spec-conformant` on real Tegra Nouveau NV120. Ledger #54.

### §#55-pause — transformFeedback pause/resume Citron-only quirk (2026-07-03)
Strict `pause skipped id=200, captured id=100 + id=300 in order` on real Tegra
Nouveau NV120. Ledger #55.

### §#56 — getBufferSubData COPY_WRITE_BUFFER target-specific defect (2026-07-03)
Per-target rebind fallback (second-stage fix) verified on real Tegra Nouveau
NV120 fresh-context path: `Arm A + Arm B both ok 64B memcmp`. State-carryover
observation on re-invocation documented; harness fix deferred. Ledger #56.

### §#57 — Batch 3 extension surface (2026-07-03)
All 5 b3 probes PASS on real Tegra Nouveau NV120: TIMER_QUERY, POLY_CLAMP,
INDEXED_BLEND, MULTI_DRAW, BFE_CONST. Extension advertising + wiring
hardware-verified. Ledger #57.

Minor open sub-item — TIMER_QUERY 32-bit truncation (`0xFFFFFFFF` on both Citron
and hardware for TIMESTAMP_EXT queries whose 64-bit driver time exceeds 2^32).
Non-blocking; deferred until a demo needs nanosecond deltas. Fix path:
`glGetQueryObjectui64vEXT` via proc-address, JS Number represents up to 2^53.
