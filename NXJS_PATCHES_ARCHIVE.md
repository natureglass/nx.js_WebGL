# nx.js engine patches — ARCHIVE

Superseded ledger entries preserved for historical reference. When an
entry in [NXJS_PATCHES_NEEDED.md](NXJS_PATCHES_NEEDED.md) is replaced
by a newer entry that captures the shipped shape (rather than a
proposed template), the original PROPOSED text lands here so the
design lineage isn't lost.

Entries in this file do NOT need re-application, verification, or
tracking. They document paths considered and paths shipped, and are
useful when a future regression or design discussion needs the
original template that a shipped entry evolved from.

Global entry numbering is shared across
[NXJS_PATCHES_NEEDED.md](NXJS_PATCHES_NEEDED.md),
[../brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md),
and this file. Never renumber.

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

## #40-tombstoned — user_snap reset primitive for cross-demo teardown (Phase A step-a follow-on) — SHIPPED 2026-07-03, TOMBSTONED 2026-07-03

**Tombstoned** (this cleanup pass, alongside #41-tombstoned) after hardware green on the [runtime pre-arm fix](../brewser-runtime-v8/RUNTIME_SHIMS.md#42) confirmed the pre-arm is the load-bearing mechanism and the `resetUserSnap` engine primitive is no longer load-bearing. Engine `FN(w_reset_user_snap)`, both `resetUserSnap` FUNCS[] entries, and the runtime `if (false)` call block in `gl-teardown.ts` all reverted in the same cleanup. Full original text below for design-lineage.

**File(s):** [source/webgl.cc](source/webgl.cc) — `FN(js_webgl_reset_user_snap)` + `NX_SET_FUNC(init_obj, "webglResetUserSnap", ...)` in `nx_init_webgl`.

**Root cause.** Patch #36 (bracket-state persistence) shipped `user_snap` as a per-call shadow of user WebGL state, plus a `user_snap_valid` boolean that gates the FIRST-EVER enter_bracket's init path vs subsequent restore paths ([webgl.cc:315-369](source/webgl.cc#L315)). Once true, `user_snap_valid` stays true for the process lifetime — no reset primitive existed. This was fine in the single-demo regime patch #36 shipped for, but breaks under Phase A step (a)'s multi-demo teardown model:

- Demo A runs. First enter_bracket: `user_snap_valid=false` → init path captures `user_snap.vao=auto_user_vao` (the patch #36 attribute-state VAO). Sets `user_snap_valid=true`.
- Demo A exits. brewser-runtime-v8's `gl-teardown.ts::teardownGL` calls `gl.bindVertexArray(null)` + `gl.bindTexture(TEXTURE_2D, null)` + `gl.stencilMask(0xFFFFFFFF)` etc. via the wrapped w_* setters. Each shadow-writes into `user_snap` (per patch #36's contract): `user_snap.vao=0`, `tex2d_binding=0`, `stencil_mask=-1`.
- Demo B launches. First enter_bracket: `user_snap_valid` is STILL TRUE (never reset) → RESTORE path → `nx_gl_state_restore(&user_snap)` → binds VAO 0 (default), leaves TU0 empty, sets stencil mask to -1. `auto_user_vao` binding from enter_bracket line 351 is IMMEDIATELY overwritten by the restore at line 366.
- Demo B's attribute state lives on the default VAO that Skia touches → Skia clobbers between frames → geometry misreads → visual break.

**Fresh-boot vs post-bloom sweep diff (2026-07-03) established the mechanism.** Every diffed field (`bind.VERTEX_ARRAY`, `TU[0].TEXTURE_2D`, `stencil.*_MASK`) matched values my teardown had shadow-written into user_snap. Full diff in [RUNTIME_SHIMS.md #39](../brewser-runtime-v8/RUNTIME_SHIMS.md#39).

**Fix (shipped).** Expose a single-line reset primitive on the `$` bridge:

```cpp
FN(js_webgl_reset_user_snap) {
    if (st) st->user_snap_valid = false;
}
```

Registered as `$.webglResetUserSnap`. Called runtime-side as `globalThis.$.webglResetUserSnap?.()` — the engine binding lives on `$` (the init_obj bridge), NOT on the nx.js runtime's `Switch` module namespace, which is a separate TS module attached at [nxjs-extended/packages/runtime/src/index.ts:94](../nxjs-extended/packages/runtime/src/index.ts#L94) via `def(Switch, 'Switch')`. First shipping attempt (2026-07-03) incorrectly called `Switch.webglResetUserSnap` → silent no-op because `Switch` doesn't mirror engine additions to `$`. Fixed same-day; see fix note below. brewser-runtime-v8's `teardownGL` calls it as the very last step of session teardown, so the NEXT demo's first enter_bracket takes the init path — captures whatever's in GL AT THAT MOMENT (auto_user_vao just bound at [webgl.cc:351](source/webgl.cc#L351), plus Skia's current defaults after the cut #15 reset at lines 337-344) as user_snap. Identical to fresh-boot behavior.

**FIX NOTE (2026-07-03, iteration 3 — reachability fixed but mechanism theory rejected).** Initial ship (iter 1) used `Switch.webglResetUserSnap` — no-op because `Switch` is a separate TS module namespace, not a `$` mirror. Iter 2 used `globalThis.$.webglResetUserSnap` — ALSO no-op because nx.js's runtime bundle DELETES `globalThis.$` at end of its boot ([packages/runtime/src/$.ts:932](../nxjs-extended/packages/runtime/src/$.ts#L932): `delete (globalThis as any).$;`). Confirmed via `[patch40:runtime] dollar-typeof=undefined fn-typeof=no-dollar`. Iter 3: installed `resetUserSnap` as a WebGL METHOD via the FUNCS[] table (same pattern as existing `enableGpuBridgePrototype` / `setBridgeAutoFlush` hooks). Sentinels confirmed the call fires correctly (`[patch40:engine] resetUserSnap fired st=<ptr> pre_valid=1`).

**DISABLED 2026-07-03 (mechanism theory rejected).** Iter 3 was reachable and fired, but the sweep at sensors' first-draw showed IDENTICAL values to pre-#40 broken state, AND sensors regressed from "broken cube" to "no cube at all" (screenshot confirmed). Analysis:

- (1) My teardown's OWN `resetStateToDefaults` w_* setters (blendFunc, viewport, etc.) trigger `enter_bracket` whose init path saves the CURRENT GL state AT THAT MOMENT — which is Skia's leftover from before teardown, not the spec defaults my teardown is about to write. Then my subsequent w_* setters shadow-write user_snap with poisoned values (vao=0 from bindVertexArray(null), tex2d_binding=0 from bindTexture(null), stencil_mask=-1 from stencilMask(0xFFFFFFFF)) — same as pre-#40.
- (2) `resetUserSnap` flips `user_snap_valid=false` AT END of teardown. Sensors' first w_* call takes init path, saving current GL as user_snap. Current GL after Skia composes = Skia's leftover with `auto_user_vao` re-bound by enter_bracket line 351. Theoretically user_snap.vao=auto_user_vao, but sweep shows vao=0 — the init save isn't producing the expected value, OR subsequent state re-poisons it. Root cause not isolated.
- (3) Empirically WORSE than pre-#40: sensors' broken box became "no cube at all". Init-path capture of Skia-leftover state is worse than restore of teardown-poisoned state for sensors' rendering.

**Runtime call is now GATED FALSE** in gl-teardown.ts (`if (false as boolean) { fn?.call(gl); }`). The engine primitive remains available on both v1 and v2 WebGL context prototypes for future re-enable when we have a viable mechanism. The `bracket + user_snap` machinery is exactly what Phase B (per-demo EGL contexts) retires — the real fix ships with step (c) full context isolation, not with a deeper user_snap hack.

**Recurrence risk for accidental re-enable.** If a future edit removes the `if (false)` gate without re-verifying the mechanism, sensors' rendering will regress from "broken cube" to "no cube". Recurrence tell: `[patch40:engine]` lines appear in log AND `bind.VERTEX_ARRAY=0` in sensors' sweep AND user reports "no cube visible".

**Why upstream-vanilla lacks it.** Patch #36 is fork-only (Tegra/Mesa-Nouveau bracket-state persistence for the WebGL↔Skia coexistence model). #40 extends that primitive with a small session-boundary API — meaningful only in embedders that host multiple GL sessions per process. nx.js has no built-in session concept; the runtime layer synthesizes it. This entry documents the additional API the fork requires.

**DISPOSITION:** `upstream-candidate` — companion to #36. Any embedder that hosts multi-session apps benefits from being able to reset the bracket baseline at session transitions. Bundle with PR-D alongside #36/#17/#35.

**UPSTREAM STATUS:** `not-submitted`. Companion to patches #6/#17/#35/#36 (all engine bracket-persistence family).

**Sequencing.** Ships as the load-bearing engine primitive for Phase A step (a) (see [RUNTIME_SHIMS.md #39](../brewser-runtime-v8/RUNTIME_SHIMS.md#39)). The runtime call is a single `Switch.webglResetUserSnap?.()` at the end of `teardownGL`, guarded so a stale nxjs.nro (without this binding) doesn't break teardown. Step (b) bundle refactor and step (c) per-demo V8 contexts don't require or interact with this primitive; #40 is pure step-(a) plumbing.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `webglResetUserSnap`. Recurrence tell: bloom→sensors (or any WebGL2 demo → WebGL1 demo → same pair) regresses to broken-box class on the second demo, and `[gl-sweep:v1-first]` diff between fresh-boot and post-bloom runs shows `bind.VERTEX_ARRAY`, `TU[0].TEXTURE_2D`, and/or `stencil.*_MASK` diverging with post-bloom values matching whatever `teardownGL::resetStateToDefaults` writes — this entry regressed.

---

---

## #41-tombstoned — VAO apply-path null→auto_user_vao stopgap — SHIPPED 2026-07-03, DISABLED same-day, TOMBSTONED 2026-07-03

**Tombstoned** (this cleanup pass, alongside #40-tombstoned) — the engine `if ((false) && ...)` dead branch in `enter_bracket` fully removed after hardware green on the runtime pre-arm fix ([RUNTIME_SHIMS.md #42](../brewser-runtime-v8/RUNTIME_SHIMS.md#42)) rendered the mechanism obsolete. Full original text below for design-lineage.

## #41 — VAO apply-path null→auto_user_vao stopgap — SHIPPED 2026-07-03, DISABLED same-day (mechanism theory rejected on hardware)

**Status:** the runtime pre-arm at [RUNTIME_SHIMS.md #42](../brewser-runtime-v8/RUNTIME_SHIMS.md#42) is the load-bearing fix. The engine block below is retained behind an `if (false)` gate so the shipping toolchain and `verify-patches.sh` can still parse the pattern; full revert + tombstone will land in the same cleanup pass as [#40](#40) after hardware green.

**Hardware evidence that killed the theory (2026-07-03 postbloom run).** Instrumented `[patch41:apply]` fired 431 times per session. First fire at [hw-sensors-postbloom.log:6453](sdmc:/hw-sensors-postbloom.log) landed AFTER the tenant's own d=1 draw had already succeeded correctly on VAO 0 (log line 6451: `d=1 vao=0 tu0=0 a0[e=1,b=10,sz=3,st=32,off=0] a1[e=1,b=10,...,off=12] a2[e=1,b=10,...,off=24] a3[e=0]`). Every subsequent draw was on VAO 2 (auto_user_vao) with a0..a3 all `e=0` — empty VAO because sensor's raw-WebGL1 `enableVertexAttribArray`+`vertexAttribPointer` calls had shadow-configured VAO 0, not VAO 2. Alex-visual on hardware: **"Nothing rendered"** across the whole session. The stopgap STRANDED a legitimately-configured tenant VAO by forcibly swapping to an empty one at the first restore boundary.

**Root cause reconsidered.** `user_snap.vao == 0` has two distinct meanings the swap can't distinguish:
1. Skia clobbered the tenant VAO handle (the failure mode #41 was written for). Auto_user_vao substitution is correct here.
2. The outgoing demo's teardown called `bindVertexArray(null)` legitimately AND the incoming demo's raw-WebGL default-VAO attribute setup landed on VAO 0 as its persistent state. Auto_user_vao substitution STRANDS those attribs.

Postbloom→sensors is case 2. The fresh-boot audit only exercised case 1's timing (Skia had run, tenant hadn't); postbloom exercises case 2's timing (tenant setup already landed on VAO 0 by the time restore runs).

**Fix that shipped instead.** Runtime pre-arm at end of `teardownGL`: create + bind a fresh non-zero VAO through the wrapped surface, so `user_snap.vao` is ALWAYS a valid tenant VAO by construction, restore binds something Skia doesn't touch, and the tenant's attribs land on it naturally. See [RUNTIME_SHIMS.md #42](../brewser-runtime-v8/RUNTIME_SHIMS.md#42) for the mechanism + why we don't add an exemption list for `auto_user_vao` (invariant beats enumeration).

---

**Original (2026-07-03) design description below — kept for historical context; the engine block is `if (false)`-gated.**

**File(s):** [source/webgl.cc](source/webgl.cc) — 5-line insert in `enter_bracket` right after `nx_gl_state_restore(&st->user_snap)` at [webgl.cc:366](source/webgl.cc#L366).

**Root cause.** Asymmetry in the bridge-null→bridge-default translation. FBO handles the null-input case at BOTH the shadow-write path (`w_bind_framebuffer` at [webgl.cc:1744](source/webgl.cc#L1744) — `GLuint actual = (fbo == 0) ? nx_webgl_bridge_fbo_id() : fbo;` writes tenant, not 0, into `user_snap.fbo`) AND the apply path (`enter_bracket` line 322-326 — `st->bound_fbo_js == 0 ? tenant : bound`). VAO handles it ONLY at the apply path's UNCONDITIONAL bind at [webgl.cc:351](source/webgl.cc#L351) (`glBindVertexArray(st->auto_user_vao)`), which is IMMEDIATELY UNDONE 15 lines below by `nx_gl_state_restore` at [webgl_bridge.cc:635](source/webgl_bridge.cc#L635) when `user_snap.vao=0` — the restore `glBindVertexArray((GLuint)s->vao)` overwrites the auto_user_vao bind with default VAO 0.

**Localization: who writes `user_snap.vao=0`?**
- **Direct writer**: `w_bind_vertex_array` at [webgl.cc:1549](source/webgl.cc#L1549) via `st->user_snap.vao = (GLint)v` when JS caller passes null. brewser-runtime-v8's `gl-teardown.ts::resetStateToDefaults` calls `gl.bindVertexArray(null)` explicitly — this is the sole direct writer we've observed on hardware.
- **Indirect via delete**: `w_delete_vertex_array` at [webgl.cc:1537](source/webgl.cc#L1537) does NOT shadow-write `user_snap.vao`. Per GL spec, deleting the currently-bound VAO auto-reverts GL state to 0, but `user_snap.vao` retains the deleted name (dangling). At restore time `glBindVertexArray(deleted_name)` errors with `INVALID_OPERATION` and leaves the current binding untouched — which is `auto_user_vao` from line 351. So the delete-currently-bound path doesn't hit the same bug (dangling ID != 0). Apply-path fix still catches direct-null case correctly.

**Fix (apply path).** Alex-authorized minimal engine stopgap. Placed at the APPLY path (not the write path) so any future writer that sets `user_snap.vao=0` is also caught — e.g., a future edit that adds shadow-tracking to `w_delete_vertex_array`:

```cpp
} else {
    nx_gl_state_restore(&st->user_snap);
    // Patch #41 — VAO apply-path stopgap.
    if (st->user_snap.vao == 0 && st->auto_user_vao != 0) {
        glBindVertexArray(st->auto_user_vao);
        fprintf(stderr, "[patch41:apply] vao=0→auto_user_vao=%u\n",
                (unsigned)st->auto_user_vao);
        fflush(stderr);
    }
}
```

Instrumentation `[patch41:apply]` fires every time the mapping activates. Once verified stable on hardware and no other failure modes remain, the fprintf lines can be removed.

**Companion trace instrumentation (temporary, this build only).** For Alex's single hardware session diagnostic, always-on printfs added:
- `[tu-trace:active_tex] unit=0x...` at every `w_active_texture`
- `[tu-trace:bind_tex_2d] name=... at_active=0x... shadow=<0|1>` at every `w_bind_texture(TEXTURE_2D)`
- `[vao-trace:bind] name=...` at every `w_bind_vertex_array`
- `[bracket-trace:apply] active_tex=0x... tex2d_binding=... vao=... valid_pre=...` at end of every enter_bracket
- `[draw-attribs] d=N vao=... tu0=... act=0x... a0[...] a1[...] a2[...] a3[...] err=0x...` at draws #1/2/3/10 per-context (counter resets at context_new). Uses engine-side `glGetVertexAttribiv` to work around the missing JS `getVertexAttrib` binding (documented as a real upstream gap; not implementing full JS API in this cut).

**Why upstream-vanilla lacks it.** The auto_user_vao mechanism is patch #36 (fork-only, Tegra/Mesa-Nouveau + Skia-Ganesh coexistence). #41 is an asymmetry cleanup on that mechanism — meaningful only in embedders that host the WebGL↔Skia bracket contract from #36.

**DISPOSITION:** `fork-only`, DISABLED. Phase-B (per-demo EGL contexts) retires the bracket + auto_user_vao machinery entirely; #41's `if (false)` branch disappears with it. Full revert bundled with the same cleanup that tombstones #40 after hardware green on #42.

**UPSTREAM STATUS:** `n/a`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `patch41:apply`. The string must appear in a dead branch guarded by `if ((false) && ...)`; if any future edit strips the `false &&` and re-enables the swap, sensors' post-bloom rendering regresses from "correct cube" back to "nothing rendered" (the swap will again strand tenant attribs on VAO 0). Recurrence tell if that happens: `[patch41:apply]` log lines fire in a session AND the tenant's cube disappears after the first frame in bloom→sensors. Also verify `st->auto_user_vao` still exists as a WebGLState field ([webgl.cc:150](source/webgl.cc#L150)); if patch #36's auto_user_vao machinery is refactored, the dead branch's `st->auto_user_vao != 0` reference needs the same rename or removal.

---
