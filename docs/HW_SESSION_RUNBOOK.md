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

**HW verdict** (fill in on hardware session):
- `[  ]` PASS with fallback disabled → Citron-only issue. Reword #52a to
  "Citron-emulator quirk"; fallback becomes defensive-only.
- `[  ]` FAIL with fallback disabled → Real Mesa Nouveau NV120 driver bug.
  Fallback remains load-bearing. Reword #52a to "driver-ceiling".
- `[  ]` UNEXPECTED (some other diagnostic) → attach the hardware log to the
  ledger entry and re-triage.

**HW session date / hardware model / verdict:** _pending_

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

**HW verdict** (fill in on hardware session):
- `[  ]` QUERY PASS strict — probe detail reads
  `spec-conformant (result > 0 for visible draw)` → Citron-only issue. Revert
  the probe relaxation and restore strict `QUERY_RESULT > 0` as the default
  in gl-probes.js. Reword #54 to "Citron-emulator quirk".
- `[  ]` QUERY FAIL strict — probe detail reads
  `Mesa Nouveau NV120 quirk — result=0 despite pixels drawn (#54)` →
  Real driver ceiling. Reword #54 to "driver-ceiling — no engine fix planned"
  and add to `[[reference-mesa-nouveau-layered-sampling-unsupported]]` family
  in workspace memory.

**HW session date / hardware model / verdict:** _pending_

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

## Archived (hardware-verified) — none yet
