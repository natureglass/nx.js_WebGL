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
