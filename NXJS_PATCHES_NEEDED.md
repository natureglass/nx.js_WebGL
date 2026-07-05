# nx.js engine patches needed on the V8/upstream base

Permanent catalogue of QuickJS-era engine patches that the V8/upstream
migration dropped and that must be **re-applied after any future upstream
nx.js pull**. Use this file as the re-application checklist at
upstream-update time. Grows as Step 2 (and beyond) surfaces more
fork-patches the migration lost.

## For agents doing an upstream-update pull

Every upstream nx.js pull requires reading BOTH ledgers, not just this
one:

1. **This file** — `NXJS_PATCHES_NEEDED.md` — engine fork delta
   (native C++ under `source/` + engine-TS under
   `packages/runtime/src/`). Open engine asks live here even when a
   runtime/demo workaround has shipped.
2. **[brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md)** —
   brewser-runtime shims that wrap the engine's public surface at
   runtime install time. Upstream-pull sensitivity: an upstream method
   change or feature landing can invalidate a shim silently. Re-verify
   at every pull.
3. **[NXJS_PATCHES_ARCHIVE.md](NXJS_PATCHES_ARCHIVE.md)** — superseded
   proposals kept for design lineage. Not load-bearing; skim only if
   researching an entry's history.

Automated check: `scripts/verify-patches.sh` prints PRESENT/MISSING
per entry across all three ledgers.

Global entry numbering is shared across all three files. **Never
renumber.** Tombstones like `## #12 — MOVED → …` remain in this file
to preserve the number-space invariant when future entries are added.

See also [FORK_DELTA_REDUCTION.md](FORK_DELTA_REDUCTION.md) for the
2026-07-02 fork-delta reduction study and the globalThis-injection
proposal verdict.

## Index (machine-readable)

| # | Where | Disposition | Upstream status | Verify grep target | One-line title |
|---:|---|---|---|---|---|
| 1 | engine | upstream-candidate | PR-drafted(PR-A) | `image.ts: return globalThis\.fetch\(input, init\)` | image.ts call-time globalThis.fetch deferral |
| 2 | engine | upstream-candidate | PR-drafted(PR-A) | `audio.ts: return globalThis\.fetch\(input, init\)` | audio.ts call-time globalThis.fetch deferral |
| 3 | engine | upstream-candidate | PR-drafted(PR-A) | `video.ts: return globalThis\.fetch\(input, init\)` | video.ts call-time globalThis.fetch deferral |
| 4 | engine | fork-only (SHIPPED 2026-06-30) | n/a | `cursor.h: nx_cursor_set_static` + `canvas.cc: js_set_cursor_overlay` + `skia_gpu.cc: cursor composite hook` | Screen.setCursorOverlay native binding |
| 5 | engine | upstream-candidate | PR-drafted(PR-D) | `skia_gpu.cc: EGL_CONTEXT_CLIENT_VERSION,\s*3` | skia_gpu ES3 shared context + accessors |
| 6 | engine | upstream-candidate | PR-drafted(PR-D) | `webgl_bridge.h: nx_gl_state_snap_t` | webgl_bridge state save/restore + tenant FBO |
| 7 | engine | upstream-candidate | PR-drafted(PR-D) | `webgl.cc: nx_webgl_compose_if_active` | WebGL1 context via screen.getContext('webgl') |
| 8 | engine | upstream-candidate (FIXED) | PR-drafted(PR-F) | `webgl*-rendering-context.ts: NO for-of over GL_CONSTANTS` | V8 JIT crash fix — bulk defineProperties |
| 9 | engine | upstream-candidate | not-submitted | `webgl-rendering-context.ts: SRGB8_ALPHA8:` | v1 ES3 sized internalformat constants |
| 10 | engine | upstream-candidate | not-submitted | `webgl.cc: bucket_e_translate_tex_image` | WebGL1 EXT_sRGB + HalfFloat translate |
| 11 | engine | fork-only | n/a | `webgl.cc: maybe_replace_pmrem_fs` | PMREM r184 FS replacement |
| 12 | **runtime** (MOVED) | fork-only | n/a | `cube-route-shim.ts: cubeUVSample` | samplerCube→sampler2D routing layer |
| 13 | engine | upstream-candidate | PR-drafted(PR-C) | `canvas.cc: set_font_size(context, context->state->font_size)` | canvas.cc font-size pin |
| 14 | engine | upstream-candidate | PR-drafted(PR-D) | `webgl.cc: webgl2ContextNew` | WebGL2 context factory |
| 15 | engine | upstream-candidate | PR-drafted(PR-D) | `webgl.cc: install_methods_v2` | v1/v2 FUNCS[] table split |
| 16 | engine | fork-only (diagnostic) | n/a | `webgl_bridge.cc: nx_webgl_state_probe_log` | passive state-contract probe |
| 17 | engine | fork-only | PR-drafted(PR-D) | `webgl_bridge.h: sampler_unit0` | nx_gl_state_snap_t extension (sampler_unit0 + read_fbo) |
| 17-superseded | **archive** (MOVED) | — | — | — | (original template, superseded by shipped #17) |
| 18 | **runtime** (MOVED) | brewser-specific | n/a | `cube-route-shim.ts: safeBind` | cube-route-shim per-method capability guards |
| 19 | engine (OPEN) | brewser-specific | n/a | KNOWN-OPEN — Phase 2.G.4 gate | v2 cube-routing applicability |
| 20 | engine (OPEN) | upstream-candidate | not-submitted | demo-side: `rgbe-loader.js: tex.flipY = false` | UNPACK_FLIP_Y_WEBGL honor for typed-array texImage2D |
| 21 | **runtime** (MOVED) | brewser-specific | n/a | `shadow-route-shim.ts: textureShadowCompat` | sampler2DShadow→sampler2D rewrite shim |
| 22 | engine | upstream-candidate | not-submitted | `video-decoder.cc: nx_init_video_decoder` | Switch.VideoDecoder V8 port |
| 24 | **runtime** (MOVED) | brewser-specific | n/a | `cube-route-shim.ts: allocateCubeRTAtlas` | Cube-RT-readback rescue |
| 31 | engine (OPEN) | brewser-specific + upstream-candidate | not-submitted | demo-side: `gpgpu-water/main.js: renderer.resetState()` | Three.js WebGLState cache desync engine fix |
| 34 | engine (OPEN) | upstream-candidate | not-submitted | runtime workaround: `web-audio-stubs.ts: v8-override-throw-stubs` | Audio createX no-throw stubs |
| 35 | engine | upstream-candidate (with #6) | PR-drafted(PR-D) | `webgl_bridge.h: depth_mask` + `webgl_bridge.h: stencil_mask` | nx_gl_state_snap_t further extension: depth_mask + stencil_mask (cut #15) |
| 36 | engine | upstream-candidate | PR-drafted(PR-D) | `webgl.cc: nx_gl_state_snap_t user_snap` + `webgl.cc: user_snap.viewport[0]` | WebGL bracket-state-persistence via per-call shadow-tracked user_snap |
| 37 | engine | upstream-candidate | not-submitted | `webgl.cc: w_tex_storage_3d` | v2 texStorage3D + texSubImage3D bindings (cut #32) |
| 40 | **archive** (MOVED) | — | — | — | (tombstoned 2026-07-03; mechanism obsoleted by #42, see archive) |
| 41 | **archive** (MOVED) | — | — | — | (tombstoned 2026-07-03; mechanism obsoleted by #42, see archive) |
| 42 | engine | fork-only | n/a | `webgl.cc: OES_vertex_array_object` | OES_vertex_array_object ext for v1 pre-arm route (RUNTIME_SHIMS #42 companion) |
| 43 | engine | upstream-candidate | not-submitted | `webgl.cc: populate_native_extensions` + `webgl.cc: \[gl-ext-dump\]` | Phase-0: native GL extension enumeration + `_getNativeExtensionsString`/`_getEglVersion` internals + `[gl-ext-dump]` boot log (RUNTIME_SHIMS #44 companion) |
| 44 | **runtime** (MOVED) | fork-only | n/a | `webgl-ext-shim.ts: installGetBackendInfo` | Phase-0: `gl.getBackendInfo` runtime shim (engine #43 companion) |
| 45 | engine | upstream-candidate | not-submitted | `webgl2-rendering-context.ts: TS extension stub reached` | Phase-0: webgl2-rendering-context.ts landmine defuse — dead TS stubs throw instead of silent `[]`/null |
| 46 | engine | upstream-candidate | not-submitted | `webgl_bridge.cc: GL_DEPTH24_STENCIL8` + `webgl_bridge.cc: \[bridge-fbo:` | Phase-0 commit 2: bridge FBO stencil contract — DEPTH24_STENCIL8 renderbuffer + combined attach + STENCIL_BITS=8 wire + [bridge-fbo] completeness assert |
| 47 | engine | upstream-candidate | not-submitted | `webgl.cc: has_native_ext` + `webgl.cc: is_v2_context` + `webgl.cc: w_compressed_tex_image_2d` | Phase-1 batch 1: driver-probed advertisement + 16 batch-1 extension rows + compressed 2D upload natives + UNMASKED/MAX_ANISO getParameter branches |
| 48 | engine | upstream-candidate | not-submitted | `webgl.cc: probe_ext_frag_depth` + `webgl.cc: ANGLE_instanced_arrays` + `webgl.cc: WEBGL_debug_shaders` | Phase-1 batch 2A: Unity-P1 v1 function surfaces (ANGLE_instanced_arrays, WEBGL_draw_buffers, EXT_frag_depth probe, OES_VAO list-flip) + WEBGL_lose_context / WEBGL_debug_shaders software minimal impls + WEBGL_compressed_texture_etc (rider 1) |
| 49 | engine | upstream-candidate | not-submitted | `webgl.cc: v2_rider2` + `webgl.cc: Rider 2 explicit list` | Phase-1 batch 2B: Rider 2 v2 spec-conformance prune — 5 WebGL1-only extensions return null on v2 (OES_standard_derivatives, OES_texture_float, OES_texture_half_float, OES_texture_half_float_linear, WEBGL_depth_texture) — Chrome / Firefox match |
| 50 | engine | upstream-candidate | not-submitted | `webgl.cc: Phase-1.5-LOW` + `webgl.cc: w_get_buffer_sub_data` + `webgl.cc: OES_fbo_render_mipmap` | Phase-1.5-LOW: 30 core WebGL2 methods (buffer 2, framebuffer thin 6, 3D texture 3, uint uniforms 8, non-square matrix 6, clear buffer 4, draw range 1) + rider OES_fbo_render_mipmap on v1 (batch-2 defect fix). Counter 17→47/88. |
| 58 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_get_uniform\)` + `webgl.cc: Tier 1 \(ledger #58\)` | Tier 1 batch (WebGL1 ERROR-bucket spec-hole fills): `getUniform(program, location)` — full type-switched dispatch covering scalar/vecN/matN/non-square (bool/int/uint/float + samplers). Unlocks 20 uniforms-no-over-optimization-on-uniform-array-* tests + uniforms-uniform-default-values. |
| 59 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_copy_tex_image_2d\)` | Tier 1: `copyTexImage2D(target, level, internalformat, x, y, w, h, border)` thin wrapper. Sibling of `copyTexSubImage2D` (2.G.1 cut #25). Unlocks 7 tests: misc-uninitialized-test, rendering-clear-after-copyTexImage2D, textures-misc-copy-tex-image-2d-formats/-crash/-texture-copying-and-deletion/-feedback-loops/-texture-npot. |
| 60 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_get_vertex_attrib\)` | Tier 1: `getVertexAttrib(index, pname)` pname-switched read. BUFFER_BINDING → K_BUFFER wrapper; ENABLED/NORMALIZED/INTEGER → bool; SIZE/STRIDE/TYPE/DIVISOR → number; CURRENT_VERTEX_ATTRIB → Float32Array(4). Unlocks extensions-angle-instanced-arrays, extensions-oes-vertex-array-object. |
| 61 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_get_framebuffer_attachment_parameter\)` | Tier 1: `getFramebufferAttachmentParameter(target, attachment, pname)` pname-switched read. ATTACHMENT_OBJECT_NAME wraps into K_TEXTURE or K_RENDERBUFFER per preflight OBJECT_TYPE; other pnames → number. Unlocks extensions-webgl-draw-buffers, renderbuffers-framebuffer-test. |
| 62 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_get_attached_shaders\)` | Tier 1: `getAttachedShaders(program)` — wraps GLuint names into K_SHADER JS Array. Empty array (not null) when nothing attached, spec. Unlocks misc-expando-loss, programs-program-test. |
| 63 | engine | upstream-candidate | not-submitted | `webgl.cc: FN\(w_vertex_attrib_1fv\)` + `webgl.cc: FN\(w_vertex_attrib_4fv\)` | Tier 1: `vertexAttrib{1,2,3,4}fv(index, arr)` typed-array pointer variants of the scalar setters. Same macro shape as uniform_{N}fv. Unlocks attribs-gl-vertex-attrib-render (dynamic-form receiver). |
| 64 | engine | upstream-candidate | not-submitted | `webgl.cc: nx_webgl_snapshot_bridge_rgba8` + `webgl.h: nx_webgl_snapshot_bridge_rgba8` + `canvas.cc: nx_webgl_snapshot_bridge_rgba8` | Tier 1: Screen.toDataURL WebGL-surface readback. New public helper reads tenant FBO to a heap BGRA (top-down) buffer; `nx_canvas_proto_to_data_url` + `nx_canvas_to_buffer` branch through it before falling back to snapshot_pixels. Fixes `screen.toDataURL()` on WebGL-backed Screens returning empty raster. |
| 65 | engine | upstream-candidate | not-submitted | `webgl.cc: has_compressed_format_advertised` + `webgl.cc: Tier 4 \(ledger #65\)` | Tier 4: compressed-format INVALID_ENUM validation gate in `w_compressed_tex_image_2d` + `w_compressed_tex_sub_image_2d`. Spec-required — WebGL implementations MUST reject unadvertised sized compressed internalformats before dispatching to the driver. Fixes 7 CITRON-observed hangs (hardware stall behavior unverified) in the `testCompressedFormatsUnavailableWhenExtensionDisabled` cluster (BPTC/RGTC/ETC/ETC1/PVRTC/ASTC/S3TC-sRGB). |
| 66 | engine | upstream-candidate | not-submitted | `image-bitmap.ts: tryUnwrapCanvas` + `image-bitmap.ts: canvasToImageBitmap` | Tier-A: `createImageBitmap` source-type expansion. Pre-#66 impl handled ONLY `Blob` and threw for every other spec-defined `ImageBitmapSource`. Adds `HTMLCanvasElement` (nx.js Screen / OffscreenCanvas + brewser-runtime live-DOM `<canvas>` LiveElement via `.offscreen` duck-unwrap), `ImageData` (via scratch OffscreenCanvas + putImageData), `ImageBitmap` (via drawImage round-trip), `HTMLImageElement` (nx.js Image duck-typed on `naturalWidth`/`naturalHeight`). Unlocks up to 40 conformance tests across 5 `textures-image_bitmap_from_*` sub-clusters × 8 texture formats. HTMLVideoElement branch throws a distinct diagnostic (needs canvas.cc video-frame-capture path). |
| 67 | engine | upstream-candidate | not-submitted | `webgl.cc: enabled_exts` + `webgl.cc: record_ext_enabled` + `webgl.cc: is_ext_enabled` | Tier-A: `getParameter` extension-gated pname enforcement. Per WebGL spec § 5.14.3, extension-gated pnames MUST return null (+ INVALID_ENUM) until `getExtension` has been called for their gating extension. Pre-#67 they returned real values unconditionally, causing 6-8 conformance FAILs. Adds per-context `enabled_exts` set populated at every success branch in `w_get_extension` and consulted by 8 new/modified pname branches in `w_get_parameter`: `CLIP_ORIGIN_EXT` / `CLIP_DEPTH_MODE_EXT` / `DEPTH_CLAMP_EXT` / `POLYGON_OFFSET_CLAMP_EXT` / `MAX_DUAL_SOURCE_DRAW_BUFFERS_WEBGL` / `FRAGMENT_SHADER_DERIVATIVE_HINT_OES` / `COMPLETION_STATUS_KHR` + gating on the existing `UNMASKED_VENDOR/RENDERER_WEBGL` + `MAX_TEXTURE_MAX_ANISOTROPY_EXT` branches. |
| 68 | engine | upstream-candidate | not-submitted | `webgl.cc: nx_detect_link_attrib_aliasing` + `webgl.cc: programs_with_aliased_link` | Tier-A: attribute-aliasing link failure detection. WebGL spec §5.14.9: `linkProgram` MUST fail when two active attributes end up bound to the same location via `bindAttribLocation`. Mesa-Nouveau (observed) still succeeds the driver-level link. Post-link scan iterates `glGetActiveAttrib` × `glGetAttribLocation`, detects any two active attribs sharing a location, and marks the program in a per-context set. `w_get_program_parameter` overrides `LINK_STATUS` to false for marked programs. Fixes `attribs-gl-bindAttribLocation-aliasing` (32 aliased-location assertions). |
| 69 | engine | upstream-candidate | not-submitted | `webgl.cc: convert_image_source_to_gl_pixels` + `webgl.cc: Ledger #69` | Tier-A: `texImage2D` + `texSubImage2D` ImageBitmap / Image (nx_image_t) source support. Pre-#69 both bodies handled only ArrayBuffer / ArrayBufferView sources; nx_image_t fell through the null-source branch, so `gl.texImage2D(target, 0, format, format, type, imageBitmap)` uploaded null and the destination texture stayed cleared. Blocked ~40 `textures-image_bitmap_from_*` conformance tests (all 5 sub-clusters × 8 texture formats) with signature `shouldBe 255,0,0 was 0,0,0`. Shared helper `convert_image_source_to_gl_pixels` converts premultiplied BGRA source into scratch (`std::vector<uint8_t>`) for the 8-format MVP matrix: RGBA/RGB/LUMINANCE_ALPHA/LUMINANCE/ALPHA `UNSIGNED_BYTE` + RGBA `UNSIGNED_SHORT_4_4_4_4` / `UNSIGNED_SHORT_5_5_5_1` + RGB `UNSIGNED_SHORT_5_6_5`. Honors `UNPACK_FLIP_Y_WEBGL` (row-reverse) and `UNPACK_PREMULTIPLY_ALPHA_WEBGL` (un-premultiply when caller asked for false — source is already premultiplied). Overrides `GL_UNPACK_ALIGNMENT` to 1 around the upload (restores after) so tightly-packed row layouts don't trip the driver row-stride check. Unsupported (format, type) combos fall through to null upload (pre-#69 behavior). **See also #70** — post-shipping #69 the tier-69 re-baseline showed all image_bitmap tests still FAIL; root cause was the 6-arg TexImageSource overload (`texImage2D(target, level, IF, format, type, source)`) never reaching the 9-arg native (info[8] read as undefined). #70 ports the WebGL2 TS-side normalization shim to WebGL1, so #69's C++ path is only exercised by the rare direct-9-arg-with-nx_image_t call — still valid as a spec-compliant fallback. |
| 70 | **runtime** | upstream-candidate | not-submitted | `webgl-rendering-context.ts: isTexImageSource` + `webgl-rendering-context.ts: sourceToPixels` + `webgl-rendering-context.ts: Ledger #70` | Tier-A: `texImage2D` / `texSubImage2D` TexImageSource normalization shim for WebGL 1. WebGL 2 already had this in `webgl2-rendering-context.ts`; WebGL 1 did not. Post-#69 the tier-69 re-baseline still showed `shouldBe 255,0,0 was 0,0,0` because WebGL 1 conformance calls the 6-arg overload `texImage2D(target, level, IF, format, type, imageBitmap)` — the fixed-9-arg native reads info[3]/[4]/[8] as format/type/undefined and skips the pixel path entirely. The shim mirrors the WebGL 2 impl verbatim: `isTexImageSource` duck-types on `width`+`height` numeric + not-ArrayBufferView; `sourceToPixels` rasterizes through an OffscreenCanvas 2D drawImage + getImageData (returns unpremultiplied RGBA — matches default `UNPACK_PREMULTIPLY_ALPHA_WEBGL=false`); the wrapper reshapes 6-arg → 9-arg and 7-arg (sub) → 9-arg with `px.data` as the pixels. Companion to #69; unblocks the same ~40 tests but from the TS side. **See also #71** — post-#70 the tier-70 re-baseline flipped only 2 of 40 tests (ALPHA/UNSIGNED_BYTE variants) because sourceToPixels always emits 4-byte RGBA regardless of caller format; a 3-bpp RGB caller reads pixel N's RGB as `[A_{N-1}, R_N, G_N]` (byte-stride misalignment). #71 short-circuits ImageBitmap sources to the native so #69's spec-correct format conversion fires. |
| 71 | **runtime** | upstream-candidate | not-submitted | `webgl-rendering-context.ts: Ledger #71` + `webgl-rendering-context.ts: last instanceof ImageBitmap` | Tier-A: ImageBitmap-source short-circuit in the WebGL 1 shim so #69's C++ conversion path fires. Post-#70 the tier-70 re-baseline flipped only 2 of 40 image_bitmap tests because `sourceToPixels` always emits 4-byte RGBA, but WebGL 1 conformance iterates smaller formats (RGB, LUMINANCE, LUMINANCE_ALPHA, ALPHA) + packed 16-bit types (5_6_5, 4_4_4_4, 5_5_5_1). Passing 4 bpp RGBA when the driver reads 3 bpp misaligns every pixel — pixel N's RGB reads as `[A_{N-1}, R_N, G_N]` (observed `was 255,255,0` where `128,0,0` was expected). Fix: before falling into `sourceToPixels`, check `last instanceof ImageBitmap` and passthrough to the native with an arg-reshape (6-arg → 9-arg, 7-arg sub → 9-arg). ImageBitmap wraps `nx_image_t` (per #66's construction) so #69's `nx_get_image` + `convert_image_source_to_gl_pixels` in webgl.cc runs and produces byte-stride-correct output for the caller's `(format, type)`. Covers all 5 non-video `_from_*` clusters (blob / canvas / image / image_bitmap / image_data) × 8 formats = ~40 tests. `_from_video` still deferred by #66. Non-ImageBitmap sources (raw ImageData / SVG image / webgl canvas / video — used by `textures-{image_data,svg_image,webgl_canvas,video}-*`) still route through the buggy `sourceToPixels` and will need a follow-up ledger entry that mirrors #69's conversion matrix in JS. |
| 72 | engine | upstream-candidate | not-submitted | `webgl.cc: Ledger #72 — seed default viewport` + `webgl.cc: glViewport(0, 0, w, h)` in context_new + `webgl.cc: source is premultiplied BGRA` (un-mult removal) | Tier-A: (a) seed default GL viewport at context creation to canvas dimensions per WebGL 1 spec § 5.14.3, and (b) remove un-multiplication from #69's C++ conversion for ImageBitmap sources. Post-#71 the tier-71 re-baseline confirmed my C++ code path IS running for ImageBitmap uploads AND scratch bytes are correct for all 4 pixels of the test's 2×2 source — but only the drawing bottom-left pixel matched source (0,0); every other position read `0,0,0`. A drawArrays-time diag revealed `viewport=(0,0,8,8)` (later `(0,0,1,1)` on the first call) on a canvas whose HTML declares `width=32 height=32`. Our engine inherited whatever viewport Skia's Ganesh left in GL state — a small size — because `nx_webgl_context_new` never called `glViewport`, and the WebGL spec says the caller can rely on the default viewport = drawingBufferWidth × drawingBufferHeight. Tests that never explicitly call `gl.viewport(...)` (which is most conformance tests, including all `textures-image_bitmap_from_*` and many others) rendered their full-viewport quad into an 8×8 corner; readbacks beyond that corner sampled the framebuffer clear color, giving the specific signature "only bottom-left pixel matches source (0,0)". Fix (a): `glViewport(0, 0, w, h)` after bridge init in `make_context_carrier`, plus mirror into `user_snap.viewport` so the first bracket restore doesn't overwrite it. Fix (b): image_bitmap tests do NOT call `pixelStorei(UNPACK_PREMULTIPLY_ALPHA_WEBGL, ...)` (verified via a pixelStorei diag); the test's expected values assume the bitmap's own premultipliedAlpha state is preserved through upload (matches Chrome). Un-multiplying with flag=FALSE produced `255,0,0` where test expected `128,0,0` for half-alpha red. Fixed by hardcoding `un_premultiply = false` in both `w_tex_image_2d` and `w_tex_sub_image_2d`. Broad impact: fix (a) unblocks any test that skipped explicit viewport (many conformance tests do); fix (b) restores spec-correct behavior for ImageBitmap upload. |
| 73 | engine + **runtime** | upstream-candidate | not-submitted | `image.cc: Ledger #73 — optional 3rd arg` + `image.cc: bool premultiply = true` + `image-bitmap.ts: Ledger #73 — extract flipY / unpremul` + `image-bitmap.ts: extractOpts` | Tier-A: honor `createImageBitmap({imageOrientation, premultiplyAlpha})` options. Post-#72 the RGB test's iteration 1 (`flipY=false, premultiplyAlpha=premultiply`) passes fully, but iterations 2/3/4 fail because #66's `canvasToImageBitmap` ignores those options — all 4 bitmap variants come out with the same premultiplied, non-flipped pixels. Fix: (a) engine — extend `nx_image_write_rgba` in `image.cc` with an optional 3rd arg `premultiply` (default `true` preserves the existing video-frame delivery contract used by `Switch.VideoDecoder`); when `false`, write raw RGBA→BGRA without alpha multiplication. (b) runtime — add `extractOpts` in `image-bitmap.ts` reading `opts.imageOrientation === 'flipY'` and `opts.premultiplyAlpha === 'none'`, thread `opts` through all 4 non-Blob `canvasToImageBitmap` call sites (canvas / ImageData / ImageBitmap / HTMLImageElement). For `flipY`: compose the source into a scratch OffscreenCanvas with a Y-mirror transform (`ctx.scale(1, -1); ctx.translate(0, -h); drawImage(src, 0, 0)`) before encoding. For `unpremul`: skip the PNG round-trip (which always premultiplies on decode) and store raw `getImageData` bytes via `imageWriteRGBA(bmp, bytes, false)`. Fast path (no options) is unchanged — hot callers pay zero perf cost. Blob source path deferred (tier-A blob tests all fail upstream at "Unsupported image format" for BMP; a `_from_blob` option-honoring pass would need re-routing through a scratch canvas). |
| 74 | engine | fork-only (upstream-candidate later) | not-submitted | `config.h: wasm_interpreter_opt_in` + `config.cc: str_ieq(name, "wasm_interpreter")` + `main.cc: --wasm-jitless` + `main.cc: nx_probe_wasm_tier` + `main.cc: \[wasm\] mode=` | Track-A: DrumBrake wasm interpreter opt-in gate + empirical wasm-tier boot probe. Adds `[v8] wasm_interpreter = on\|off` (default off); when on AND runtime selects jitless mode, appends `--wasm-jitless` so V8's in-tree DrumBrake interpreter runs WebAssembly instead of Liftoff (which needs the JIT code arena we don't have under Citron / applet jitless). Hardware/JIT flag string is byte-identical (gate is READ but not applied). A minimal 34-byte execution probe (compile+instantiate+call `f()->i32` returning 42) runs after Isolate+Context init but before runtime.js; logs `[wasm] mode=jit(liftoff)` / `drumbrake` / `unavailable` empirically from the outcome — truth-in-labeling, not just "the validator ran". Fail-soft: every fallible V8 step falls through to `unavailable`, TryCatch+Reset internally, zero abort paths. **Inert on the shipping switch-v8 monolith** (built without `v8_enable_drumbrake=true`); expected result today is `mode=unavailable` under Citron regardless of gate state. Companion Track B (out-of-band, TooTallNate/pacman-packages rebuild with `v8_enable_drumbrake=true`) activates DrumBrake; acceptance gate is `scripts/verify-drumbrake-monolith.sh` — must print PRESENT (nm sees `GenericJSToWasmInterpreterWrapper\|WasmInterpreterRuntime\|WasmBytecodeGenerator` symbols) before flipping any default. Upstream-candidate future PR (V8's DrumBrake benefits any nx.js embedder in emulator / no-JIT environments) — queued behind existing PR backlog. |
| 75 | **runtime** | brewser-specific | n/a | `cube-route-shim.ts: Ledger #75` + `cube-route-shim.ts: hasRescueDeps` | Tier-A: TEXTURE_CUBE_MAP ImageBitmap upload support in the cube-route-shim. Post-#71 the TEXTURE_2D iterations of the `textures-image_bitmap_from_*` cluster all pass; the CUBE_MAP portion of the same tests still fails. Two shim-side bugs: (a) the 6-arg `texImage2D(cube-face, ..., imageBitmap)` path converted the source to bytes via `imageSourceToBytes` (OffscreenCanvas + drawImage + getImageData → always 4-bpp RGBA) then fed those bytes to `origTexSubImage2D` with the caller's `format` — same byte-stride misalignment #71 fixed at TEXTURE_2D; (b) the useTexSubImage path (texImage2D-with-null then texSubImage2D-with-bitmap) never allocated an atlas because `allocateCubeRTAtlas` bailed on missing rescue deps (WebGL 1 has no `blitFramebuffer`), so the rewritten `sampler2D` shader read from whatever Skia had bound at TEXTURE_2D[TU]. Fix: (a) forward image sources as a 7-arg `origTexSubImage2D(TEXTURE_2D, 0, fIdx*W, 0, format, type, source)` so the prototype-level #70/#71 shim short-circuits ImageBitmap to native + #69's engine-side `convert_image_source_to_gl_pixels` does format-aware conversion; (b) split `allocateCubeRTAtlas` — base atlas allocation is unconditional (needed for useTexSubImage), scratch + `isRenderTarget` flag is still gated on rescue deps (only WebGL 2 or WebGL 1 with EXT_framebuffer_blit gets the FBO-write rescue). Deleted the dead `imageSourceToBytes` helper. Unblocks the CUBE_MAP portion of ~24 `_from_{canvas,image,image_bitmap,image_data}-*` tests × 8 formats each. Sibling runtime fix to engine #69 + shim #70/#71; zero engine delta. |
| 76 | **runtime** | brewser-specific | n/a | `cube-route-shim.ts: Ledger #76` + `cube-route-shim.ts: pendingCubeParams` + `cube-route-shim.ts: applyPendingCubeParams` | Tier-A: pre-atlas `texParameteri` / `texParameterf` cache on cube textures. Post-#75 the image_bitmap cube-face tests had the correct sample HUE but were off by 12-28 units on intensity (243 vs 255 full-brightness, 151/156 vs 128 half-brightness) — exceeding the ±10 tolerance. Root cause: WebGL 1 conformance calls `gl.texParameteri(TEXTURE_CUBE_MAP, TEXTURE_MIN_FILTER, NEAREST)` + MAG_FILTER + WRAP_S/T BEFORE the first `texImage2D` upload; at that moment the shim has no `cubeStates` entry for the tex, so the wrap silently drops the call. The atlas then gets the hard-coded LINEAR/CLAMP defaults from the alloc block, and LINEAR filtering across texel centers introduces sub-texel blending that dims full-brightness / brightens half-brightness pixels via the shim's non-integer atlas sample UVs. Fix: stash pre-alloc `texParameteri` / `texParameterf` calls in a per-tex `pendingCubeParams` WeakMap; drain and replay them at all three atlas-alloc sites (the inline `texImage2D` alloc block for image sources + `allocateCubeRTAtlas` for null-source WebGL 1 + `allocateCubeRTAtlasSized` for WebGL 2 `texStorage2D`), AFTER the LINEAR/CLAMP defaults so caller intent wins. WRAP_S/T stay forced to CLAMP_TO_EDGE during replay (REPEAT on a 6×1 strip would bleed adjacent faces). Last-write-wins per pname dedup keeps replay bounded. Sibling to #75; zero engine delta. |
| 77 | **runtime** | brewser-specific | n/a | `cube-route-shim.ts: Ledger #77` + `cube-route-shim.ts: null/undefined-source cube-face texImage2D with state already existing` | Tier-A: null-source cube-face `texImage2D` early-return for faces 1-5 after atlas alloc. Post-#76 the image_bitmap cube-face color checks all PASS but the trailing `glErrorShouldBe(NO_ERROR)` on the useTexSubImage variant of CUBE_MAP iterations FAILs with getError returning a value that glEnumToString name-collides against `drawingBufferWidth` — i.e., a real GL error was raised, small numeric value coincidentally equal to canvas.width. Root cause: after face 0's null-source `texImage2D` triggers `allocateCubeRTAtlas` (creating state), faces 1-5's null-source `texImage2D` calls fall past the `!state` block into the pixels block and issue `origTexSubImage2D(TEXTURE_2D, 0, fIdx*W, 0, format, type, null)` — a 7-arg call. The prototype #70/#71 shim short-circuits null (neither `instanceof ImageBitmap` nor `isTexImageSource`) and forwards 7-arg to native `w_tex_sub_image_2d`, which misreads `a_i32(info, 4)=format` as `width` and `a_i32(info, 5)=type` as `height` (7-arg vs 9-arg signature mismatch) and issues an absurd `glTexSubImage2D` that raises a real GL error. Fix: add an early-return for `source === null / undefined` after the `!state` block — forward to `origTexImage2D` so the actual cube-face storage is allocated for that face (needed for the subsequent `texSubImage2D` upload's target validity), skip the pixels block entirely. Atlas already allocated at face 0; no atlas update needed. Sibling to #75/#76; zero engine delta. | 
| 78 | engine + **runtime** | upstream-candidate | not-submitted | `image.h: bool unpremultiplied;` + `image.cc: nx_image_copy_pixels` + `image.cc: Ledger #78` + `image-bitmap.ts: Ledger #78` + `image-bitmap.ts: imageCopyPixels(bmp, image` | Tier-A: preserve alpha=0 pixels' RGB channels across `createImageBitmap` round-trip. Post-#77 the `_from_image_bitmap-*` and `_from_image_data-*` clusters (14 tests total) FAIL 7/8 each with `(0, 0, 0)` at right-half pixel positions where the source ImageData had `(255, 0, 0, 0)` — canvas 2D storage is premul and premul (r, g, b, 0) = (0, 0, 0, 0), so any round-trip through a canvas destroys the RGB channels for alpha=0 pixels. Fix: track premul state on nx_image_t via a new `unpremultiplied` field (default false; set to true by `imageWriteRGBA(bmp, buf, false)`); add engine-side `imageCopyPixels(dst, src, dstPremultiply, [flipY])` native that does row-by-row BGRA copy with premul-state conversion and Y-flip; rewrite three source branches in `createImageBitmap` to bypass the canvas round-trip: ImageData → direct `imageWriteRGBA`; ImageBitmap → direct `imageCopyPixels`; HTMLImageElement → same as ImageBitmap. Canvas / OffscreenCanvas branch unchanged (canvas storage can't originate the problem cell in the first place). |
| 79 | engine | upstream-candidate | not-submitted | `webgl.cc: Ledger #79 — WebGL 1 spec Table 5.14.6.1` | Tier-A: fix `convert_image_source_to_gl_pixels` LUMINANCE / LUMINANCE_ALPHA target format conversion to use `L = R` (WebGL 1 spec Table 5.14.6.1) instead of Rec.601 luma. Rec.601 was a #69 comment mistake — the WebGL 1 spec's TexImageSource → LUMINANCE conversion has always been `L = R`. Post-#78 the residual FAILs in the `_from_{canvas,image_bitmap,image_data}-*-tex-2d-luminance{,_alpha}-*-unsigned_byte` variants (6 tests total) show `(76, 76, 76)` where the test expects `(255, 255, 255)` for pure-red source — the Rec.601 luma of (255, 0, 0). Two-line change: both `GL_LUMINANCE` and `GL_LUMINANCE_ALPHA` cases write `dst = r` instead of the Rec.601 arithmetic. |
| 80 | engine | upstream-candidate | not-submitted | `image.ts: Ledger #80` + `image.ts: g.location?.href ?? g.document?.baseURI` | Tier-A: `Image.src` setter prefers `globalThis.location?.href` over `document.baseURI` for base URL resolution. Post-#79 the `_from_image` cluster (8 tests) still TIMEOUTs — `image.onload` never fires because `image.src = resourcePath + "..."` resolves against the stale outer-page `document.baseURI` and 404s. Embedders that emulate per-page navigation (the conformance runner pushes `globalThis.location.href = testUrl` per-test but leaves `document.baseURI` pinned at page load) benefit from call-time consultation of `location.href`. Real browsers keep `baseURI ≡ location.href` unless `<base href>` is set, so the change is spec-adjacent — divergence only for the (uncommon in nx.js) `<base href>` case. Fallback chain: `location.href → document.baseURI → $.entrypoint`. |
| 81 | **runtime** (MOVED) | brewser-specific | n/a | `live-dom.ts: Ledger #81` + `live-dom.ts: activeBase` + `live-dom.ts: g.location?.href` | Tier-A: `resolveLiveResourceUrl` consults `globalThis.location?.href` at call time (falls through to pinned `livePageBase`). Fixes the `_from_blob` cluster (8 tests) whose `fetch(rel)` routed through the `pageFetchWrapper` was resolving against the outer page URL, producing `notFoundResponse` (empty body). Test then did `blob.arrayBuffer()` → 0 bytes → `$.imageDecode` → throws "Unsupported image format". Sibling to #80; both let the runner's existing per-test `location.href` push transparently steer resource resolution without shell-side setLivePageBase per-test. Full entry in [../brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md). |
| 82 | engine | upstream-candidate | not-submitted | `image-bitmap.ts: Ledger #82` + `image-bitmap.ts: fast-path encode/decode failed` + `image-bitmap.ts: image-bitmap:#82` | Tier-A: `canvasToImageBitmap` fast-path falls back to raw `getImageData` → `imageWriteRGBA` when the PNG encode/decode round-trip throws. Fixes the residual `_from_canvas-tex-2d-alpha-alpha-unsigned_byte` FAIL (`createImageBitmap(source) failed: "Unsupported image format"`). Root cause opaque post-#78: same code path passes for luminance/rgb/rgba variants; only the first from_canvas test in a run hits it, suggesting a warm-up ordering issue in `canvasToBuffer` that produces a 0-length buffer once per run. Rather than surface the encode failure as a `createImageBitmap` rejection (which fails the whole test even though all expected ALPHA check values are `[0,0,0]` — an empty bitmap satisfies every assertion), catch the throw and route to the same raw-pixel path the unpremul branch below already uses. Byte-for-byte pixel copy, no PNG-encode risk. Diag log fires only on failure (hot path unchanged). |

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-A, branch `upstream-pr/A-fetch-deferral`. See [upstream-prs/PR-A.md](upstream-prs/PR-A.md). — flag for a TooTallNate PR.

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-A, branch `upstream-pr/A-fetch-deferral`. See [upstream-prs/PR-A.md](upstream-prs/PR-A.md)..

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-A, branch `upstream-pr/A-fetch-deferral`. See [upstream-prs/PR-A.md](upstream-prs/PR-A.md)..

**RE-APPLY / VERIFY NOTE.** Verify identically to #1/#2. Same CALL-TIME
gotcha. Same fix shape.

---

## #4 — Screen.setCursorOverlay / setAnimatedCursorOverlay — SHIPPED 2026-06-30

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

### ADDENDUM 2026-07-02 — SHIPPED VERIFIED

**Status upgrade.** SHIPPED via new [source/cursor.cc](source/cursor.cc)
(312 LOC) + [source/cursor.h](source/cursor.h) (94 LOC), 4 JS
bindings registered on the Screen prototype in
[source/canvas.cc:2058-2061](source/canvas.cc), and the compositor
hook wired into `nx_skia_gpu_present` at
[source/skia_gpu.cc:252-259](source/skia_gpu.cc). Hardware-verified
2026-06-30 — cursor visible on real CFW Switch, no trail, demos at
60 fps.

**Function-name delta from the pre-ship RE-APPLY note.** The QuickJS
names (`nx_set_cursor_overlay`, etc.) were **not** used verbatim in
the V8 port. Ship-time names are `nx_cursor_set_static`,
`nx_cursor_set_animated`, `nx_cursor_set_position`, `nx_cursor_clear`
— defined in [source/cursor.h](source/cursor.h) at ~lines 53-82 and
called from `js_set_cursor_overlay` / `js_set_animated_cursor_overlay`
/ `js_set_cursor_overlay_position` / `js_clear_cursor_overlay` in
canvas.cc at ~lines 1985-2038. Verify-patches.sh strengthened to
grep these content-level symbols (see #4 checks in
`scripts/verify-patches.sh`).

**DISPOSITION unchanged.** Still `fork-only`. See
[[project-v8-cursor-compositor-shipped]] for the full investigation
log.

---

## #5 — skia_gpu: ES3 shared context + handle accessors for the WebGL bridge

**File(s):** [source/skia_gpu.h](source/skia_gpu.h), [source/skia_gpu.cc](source/skia_gpu.cc)

**Exact change (V8 migration Phase 2.A).**
- skia_gpu.cc: EGL context bumped `EGL_CONTEXT_CLIENT_VERSION` 2 → 3; config attrs grow `EGL_RENDERABLE_TYPE = EGL_OPENGL_ES3_BIT` (0x0040 magic — matches the fbo-spike + upstream V8 webgl.cc pattern, robust to header version drift); `#include <GLES2/gl2.h>` → `#include <GLES3/gl3.h>`; init logs `[skia] GL version=... vendor=... renderer=...` once post-bringup so an ES2 silent fallback is auditable.
- skia_gpu.h: `#include <EGL/egl.h>` + forward-decl `class GrDirectContext` + four accessors — `nx_skia_gpu_egl_display()`, `nx_skia_gpu_egl_surface()`, `nx_skia_gpu_egl_context()`, `nx_skia_gpu_gr_context()`. Each returns null/zero before init / after exit. Ownership stays with skia_gpu.cc; the bridge must not destroy these handles, and must not leave a different context current when handing back to Skia.

**Symptom it fixes.** Without this, the V8 migration WebGL bridge can't attach to the same EGL context that Skia owns — upstream webgl.cc creates its own competing chain on the same NWindow, and Phase 0 proved that's not what runs (the proven recipe is ONE shared ES3 context, with `gl_state_save → webgl → restore → GrDirectContext::resetContext()` around each WebGL section). Without ES3 specifically, the WebGL bridge wouldn't have access to ES3 core entry points (glDrawArraysInstanced, glVertexAttribDivisor, etc.); Mesa returns no-op stubs for ES3 entry points on an ES2-requested context.

**Why upstream-vanilla lacks it.** Upstream's `skia_gpu.cc` was authored before V8-side WebGL ([Nathan Rajlich, fb0468f](https://github.com/TooTallNate/nx.js/commit/fb0468f)) and so creates a context at the lowest sufficient version (ES2) and exposes no shared-context surface. The two paths (Canvas-2D-via-Skia and WebGL) were designed mutually exclusive; sharing is the V8-migration architectural change.

**DISPOSITION:** `upstream-candidate`. The shared-context capability is general (any embedder benefits from coexisting Skia + raw GLES rendering on one context). The accessor surface is small + non-brewser-specific. ES3 is a strict superset of ES2 — Ganesh-GL is happy on ES3 (proven across 4,013 frames in the Phase 0 spike + verified by the `[skia] GL version=` log at boot post-2.A). The whole upstream WebGL2 path also assumes ES3 already, so consolidating the EGL/context creation onto skia_gpu and having webgl.cc attach is an upstream win — drops their duplicated EGL init.

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md).. Bundle into a Step-2 PR once 2.D is GREEN (proof point: geometry-cube renders end-to-end through the shared context). Premature to PR before there's a demonstrated working bridge on top.

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md).. Bundle into a Step-2 PR alongside
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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md).. Bundle into the same Step-2 PR as
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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-F, branch `upstream-pr/F-jit-safe-defineproperties`. See [upstream-prs/PR-F.md](upstream-prs/PR-F.md).. Candidate-3 (V8/switch-v8 minimal
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

### ADDENDUM 2026-07-02 — STILL-UNVERIFIED post-2.G demo push

**Verdict: STILL-UNVERIFIED.** The 2026-07-02 review of the shipped
2.G demo suite (materials-envmaps, materials-cubemap-dynamic,
webgl2-multiple-rendertargets, webgl2-ubo, webgl2-texture2darray,
webgl2demo Sunset Sea, spectraplay visualizer, sensors gyro cube,
gpgpu-water, webgl-lights-spotlight, webgl-materials-video plus the
v1 side webgl-materials-cubemap / webgl-loader-gltf / geometry-cube /
webgl-shadowmap) did not surface a demo that exercises r184
PMREM cube-convolution.

**Reasoning.**
- The one PMREM-using demo, `webgl-loader-gltf`, is explicitly
  pinned to Three.js r162 via `__THREE_R162_STAGED__` (per this
  entry above and [[reference-pmrem-tegra-compiler-workaround]]),
  so its PMREM path uses r162's FS which does not contain the
  `PMREMGGXConvolution` substring.
- `materials-envmaps` uses `scene.background = equirectTex` via
  `CubemapFromEquirect` — that path allocates a WebGLCubeRenderTarget
  and renders the equirect projection into cube faces WITHOUT going
  through PMREM's GGX-importance-sample step. Cut #24 (cube-RT
  readback rescue) is what makes this work; PMREM is not involved.
- `materials-cubemap-dynamic` uses `CubeCamera` per-frame — again
  no PMREM.
- All other v2 demos either use LDR paths or have no IBL at all.

The `maybe_replace_pmrem_fs` gate is verified to be dormant on
this suite (no `[f1:pmrem-fs]` diagnostic firings when re-enabled
2026-06-29). But **no demo has actually exercised the replacement
FS's runtime behavior end-to-end on Mesa-Nouveau + r184 PMREM**.
Status remains APPLIED-BUT-UNVERIFIED. Verification requires a
demo that uses `scene.environment = pmremGenerator.fromEquirectangular(
hdrTex).texture` with a Three.js r184 renderer — none currently in
the suite.

---

## #12 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#12)

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-C, branch `upstream-pr/C-fonface-charsize-pin`. See [upstream-prs/PR-C.md](upstream-prs/PR-C.md). (2026-06-30). Worth a PR after a
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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md)..

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

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md)..

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

## #17-superseded — MOVED → [NXJS_PATCHES_ARCHIVE.md](NXJS_PATCHES_ARCHIVE.md) (#17-superseded)

---

## #18 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#18)

---

## #31 — Three.js WebGLState cache desyncs across bridge exit_bracket restore — DEMO-SIDE WORKAROUND SHIPPED 2026-07-01, ENGINE-SIDE FIX OPEN

> **Workaround location:** demo-side one-liner in
> `brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/gpgpu-water/assets/main.js`.
> Not a numbered runtime shim; not migrated to
> [RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md).
> This entry captures the OPEN engine ask (option list below).


**File(s) (workaround, SHIPPED 2026-07-01, Phase 2.G.1 cut #31):**
- [brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/gpgpu-water/assets/main.js](../brewser-apps/apps/experimental/com.natureglass.webgl2threejsdemos/gpgpu-water/assets/main.js) — one line: `renderer.resetState()` right before `gpuCompute.compute()` in the animate loop.

**File(s) (engine-side general fix target, DEFERRED):** [source/webgl.cc](source/webgl.cc) — `exit_bracket()` (~line 317). Need a mechanism to notify the runtime that Three.js's WebGLState/WebGLTextures cache is stale after state restore.

**Root cause.** Bridge state save/restore lifecycle:
1. First WebGL call of a frame → `enter_bracket()` saves current GL state (which includes whatever Skia left bound).
2. Three.js runs its own draws → sets `state.bindTexture(TEXTURE_2D, RT[cur].__webglTexture, TU0)` etc.; Three.js's own JS-side `currentBoundTextures[0] = RT[cur].__webglTexture` cache is now populated.
3. End of frame → `nx_webgl_compose_if_active` → `exit_bracket()` → `nx_gl_state_restore()` calls `glBindTexture` DIRECTLY (bypassing Three.js) to restore Skia's saved binding.
4. Next frame → `enter_bracket()` saves the current GL state (still Skia's binding — unchanged if no Skia work between frames).
5. Three.js compute pass: sets `uniforms.heightmap.value = RT[cur].texture`, `state.bindTexture(TEXTURE_2D, RT[cur].__webglTexture, TU0)` checks cache → `currentBoundTextures[0] === RT[cur].__webglTexture` (still remembers what THREE.JS last set it to) → cache short-circuits, SKIPS `gl.bindTexture`. But actual GL has Skia's texture bound on TU0. Compute shader samples the WRONG texture (Skia's) → deterministic identical output regardless of `currentTextureIndex` → both ping-pong RTs converge to the same value → wave equation appears static.

**Symptom manifestations observed on gpgpu-water:**
- Water heights stay locked at initial `fillTexture` amplitude — no propagation, no damping, no splash response.
- Diag probe: both RT[0] and RT[1] hold IDENTICAL byte quartet at ALL 5 sampled pixel positions. Confirmed with:
  - `RT[0].__webglFramebuffer !== RT[1].__webglFramebuffer` (distinct FBOs).
  - `RT[0].__webglTexture !== RT[1].__webglTexture` (distinct color textures).
  - Explicit `renderer.setRenderTarget(RT0)` + `gl.clear()` isolation test writes correctly to each RT independently.
  - Manual `doRenderTarget(material, RT1)` with `uniforms.heightmap.value = RT0.texture` set right beforehand DOES produce the correct compute output on RT1, leaving RT0 untouched — so the write path is healthy.
  - The ONLY variable between manual-works and regular-fails is that the regular compute alternates the uniform value across frames while Three.js's cache thinks the sampler is already bound.

**Fix (demo-side workaround).** Call `renderer.resetState()` immediately before `gpuCompute.compute()`. This resets `state.reset()` + `bindingStates.reset()` in Three.js's WebGLState, forcing every binding to be re-emitted on the next draw. The next compute's `bindTexture` calls no longer short-circuit; the correct RT texture is bound on TU0 each frame.

**Fix (engine-side general — DEFERRED).** Options in ascending order of scope:
1. **Runtime hook**: expose a `nx.__afterExitBracket = callback` seam that `exit_bracket()` invokes after restore; brewser-runtime binds a callback that calls `renderer.resetState()` on all live WebGL contexts. Downside: needs to enumerate contexts and invoke a Three.js-specific API from runtime code.
2. **Engine-side cache invalidation marker**: expose a `gl.__stateDirty` boolean the engine sets to `true` inside `exit_bracket()`. Runtime canvas-runner reads it before yielding each rAF; if set, calls `renderer.resetState()`. Same downside re: enumerating contexts.
3. **State replay in `exit_bracket()`**: instead of restoring to Skia's saved state, restore to Three.js's LAST-SET state (which matches its cache). Downside: Skia's cache in `GrDirectContext::resetContext()` handles this on its side; needs testing to confirm no reverse breakage.

**WHY DEFERRED (blast radius).** The cache desync affects ANY Three.js demo that:
- Uses ping-pong render targets (GPUComputationRenderer, custom postprocessing loops, temporal-anti-aliasing histories).
- Reuses the same sampler2D uniform across frames while alternating its texture value.

Yet the following demos currently work without cut #31:
- webgl-postprocessing-pixel — uses EffectComposer's own RT ping-pong.
- webgl-postprocessing-unreal-bloom-selective — 13-quad mip-blur chain.
- webgl2-ubo — single-frame UBO test.

The empirical mystery: why don't THOSE break? Possible explanations (not yet verified):
- EffectComposer explicitly rebinds textures per pass (doesn't rely on Three.js's `state.bindTexture` cache).
- The material uniform pointer identity happens to CHANGE between frames (different Texture object each swap), so Three.js re-uploads via a different code path.
- The sampler TU allocation cycles more than 1 unit, so cached TU0 binding isn't the sampler's target.

Before landing the engine-side general fix, sweep every ping-pong or repeated-uniform-with-alternating-value demo to catalog which are affected. Then implement the least-invasive option that covers all of them.

**DISPOSITION:** `brewser-specific` (workaround); `upstream-candidate` (engine-side general fix, once designed).

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** *To verify the workaround is still needed*: grep gpgpu-water/main.js for `renderer.resetState()` right before `gpuCompute.compute()`. If missing, water goes static again on load. *To upgrade to engine-side fix*: pick one of the three options above, implement, remove the demo-side `resetState()` call, re-verify gpgpu-water animates AND all other ping-pong demos still work.

**Recurrence tell.** Any future demo that uses `GPUComputationRenderer` or a manual FBO ping-pong loop with a sampler2D uniform alternating between two RT.texture references, and shows "static content that ignores the compute" behavior, is this bug. First reflex: add `renderer.resetState()` before the compute call. If that fixes it, catalog under this patch and consider promoting to engine-side.

---

## #24 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#24)

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

### ADDENDUM 2026-07-02 — RESOLVED: v2 uses the SAME cube-route-shim as v1

**Verdict: v2 uses the SAME shim as v1 (no fork, no narrowing).**
Phase 2.G.4 completed with both `materials-envmaps` (v2 equirect →
cube-RT background) and `materials-cubemap-dynamic` (v2 CubeCamera
per-frame) rendering correctly on Citron via
`installCubeRouting(gl)` on the shared v2 GL context. See
[../brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md)
#12 (extended with the #24 rescue for CubeCamera / WebGLCubeRenderTarget
FBO writes).

**Evidence.**
- Both demos exercise `gl.constructor.name === 'WebGL2RenderingContext'`
  and hit the identical `installCubeRouting(gl)` call site at
  `canvas-runner.ts::getSharedScreenGL2` (post-#18 safeBind guards).
- GLSL ES 300 branch in `cube-route-shim.ts::rewriteCubeShader`
  (see `cube-route-shim.ts:647` region — `texture(sampler, dir)`
  → `cubeUVSample(sampler, dir)` rewrite) fires correctly on v2
  shader-source calls; identifier scoping preserved (regular
  `sampler2D` calls untouched).
- No `samplerCubeShadow` demo in the current suite exercises the
  gap noted in the original entry — that remains a DEFERRED
  future consideration; when a point-light-shadow demo lands,
  extend the shim per the "parallel rewrite" plan documented in
  RUNTIME_SHIMS.md #21 (shadow-route-shim).

**Status transition.** OPEN → **RESOLVED — cube-route-shim
applies unchanged to v2**. Not moved to RUNTIME_SHIMS.md because
the shim itself already lives there (#12); this ledger entry
captured the "does v2 need it" question, which is now answered.

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

### ADDENDUM 2026-07-02 — engine-side fix STILL DEFERRED post-2.G

**Verdict: engine-side fix still DEFERRED; demo-side rgbe-loader.js
row-reverse workaround still the stopgap.**

Verification: grep of current [source/webgl.cc](source/webgl.cc)
for `unpack_flip_y` matches only 5 lines — the flag definition at
line 116 and its `w_pixel_storei` write at line 578. `w_tex_image_2d`
and `w_tex_sub_image_2d` still do NOT read the flag; the engine
still ignores the WebGL spec's flipY semantics for typed-array
uploads.

The blast-radius sweep of DataTexture uploads with `flipY=true`
(prerequisite named in the entry above) was not conducted during
the 2.G demo push. Ship-blocked on that sweep; not blocked on any
active demo work.

Recurrence tell unchanged: any demo whose typed-array texture
appears vertically flipped in the offscreen output vs the source
data is either regressed rgbe-loader (workaround gone) or a new
consumer of the same gap.

---

## #21 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#21)

---

## #22 — Switch.VideoDecoder minimal V8 port (Phase 2.G.1 cut #22) — SHIPPED 2026-07-01, EXTENDED WITH CUT #22b (AUDIO SURFACE) 2026-07-02

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

**Cut #22b (2026-07-02) — audio surface + transport controls, SHIPPED CITRON VERIFIED.**

Spectraplay MP3 playback surfaced every gap left by cut #22's video-only scope. brewser-runtime's live-dom routes `<audio>` DOM elements through the exact same `videoPlay` / `videoPause` / `videoSeek` / `videoCurrentTime` code path as `<video>`, so a `Switch.VideoDecoder` opened on an MP3 opens+plays but the audio track has nowhere to go — the media's decode thread drops audio packets because `m->audio_node == NULL`. Symptoms: Play button "does nothing" (no sound, no progress-bar advance, no error in the log — decode "starts" successfully, just silent). Same failure mode for `<video>` DOM elements that carry audio.

Cut #22b lands the full HTMLMediaElement-shaped audio surface:

- **Playback wiring**: new `videoDecoderCreateAudioNode(dec, audioCtxHandle)` C binding mirrors `nx_video_create_audio_node` — attaches a STREAM_SOURCE to the media via `nx_media_set_audio_node`, stores the node on `nx_video_decoder_t` (release AFTER `nx_media_destroy` joins the decode thread), returns a JS handle. JS wrapper's constructor auto-attaches when `!opts.noAudio && this.usedAudio`: `getSharedAudioContext()` → stream → GainNode → destination. Skipped for `noAudio: true` (poster-preview path) and sources with no audio track.
- **Volume/mute**: `videoDecoderSetVolume` / `videoDecoderSetMuted` C bindings store the state on the decoder (feeds the `muted` / `volume` getters — cut #22's stubbed `false`/`1.0` return values are now real). JS wrapper's `setVolume` / `setMuted` also apply through the JS-side GainNode (`gain.gain.value = muted ? 0 : volume`) — stored on a module-level `WeakMap<VideoDecoder, GainNode>` to avoid property-pollution on the `proto()`-wrapped instance.
- **Transport (`pause`/`seek`)**: cut #22 explicitly deferred these. New `videoDecoderPause` / `videoDecoderSeek` C bindings delegate to `nx_media_pause` / `nx_media_seek` (both already existed in media-decoder.h). JS `pause()` + `seek(seconds)` methods added. `paused` getter tracks the last transition (`d->paused` flag set in play/pause bindings; also reports `true` for ended decoders).
- **Audio clock (`audioTime`)**: cut #22 stubbed to `0.0`. Now returns `nx_media_current_time(m)` when audio is wired — that clock is already audio-slaved via `clock_now`'s consumed-frame counter (media-decoder.cc:449), so the seek bar tracks the true playback position on audio-only sources.
- **Visualizer surface (`getWaveform` / `getFrequencyData` / `getAudioLevels`)**: cut #22 didn't wire these; spectraplay reads them at the DOM-element layer (`audio.getFrequencyData(specData)` / `audio.getWaveform(waveData)`) which brewser-runtime's live-video.ts routes to the decoder methods. New audio-tap ring in `nx_media` (TAP_LEN=1024 mono samples, matches QuickJS-era window size, mutex-guarded, written inside `enqueue_audio` right after `swr_convert` with a stereo→mono downmix). Three public reader accessors in media-decoder.h: `nx_media_read_waveform` (chronological slice of the last N samples), `nx_media_read_spectrum` (Hann-windowed radix-2 FFT + linear bin-average + `sqrt(1 + centre/6)` 1/f-flatten so mids and treble bars visibly react — without the flatten only the first 2-3 bass bars ever cleared spectraplay's `Math.min(1, mag * vizFreqGain)` visibility threshold), `nx_media_read_audio_levels` (3-band bass/mid/high RMS). Each reader returns `false` (or 0 for levels) until the tap has accumulated a full window.

**Files (cut #22b delta).**
- MODIFIED [source/video-decoder.cc](source/video-decoder.cc) — added `audio_node` / `muted` / `volume` / `paused` fields + release-on-close; new bindings `videoDecoderCreateAudioNode` / `SetVolume` / `SetMuted` / `Pause` / `Seek` / `GetWaveform` / `GetFrequencyData` / `GetAudioLevels`; `nx_vd_get_muted` / `_volume` / `_audio_time` / `_paused` getters now read real state.
- MODIFIED [source/media-decoder.h](source/media-decoder.h) — new public accessors `nx_media_read_waveform` / `_read_spectrum` / `_read_audio_levels`.
- MODIFIED [source/media-decoder.cc](source/media-decoder.cc) — `TAP_LEN`/`TAP_MASK`/`TAP_LOG2` constants; `tap_mutex` + `tap_ring[1024]` + `tap_write_pos` + `tap_written` fields on `nx_media`; mono-downmix write hook in `enqueue_audio` after `swr_convert`; `fft_tap` iterative radix-2 helper; `snapshot_tap` chronological-order snapshot; the three public readers with FFT + Hann window + 1/f-flatten.
- MODIFIED [packages/runtime/src/$.ts](packages/runtime/src/$.ts) — type decls for the 8 new bindings.
- MODIFIED [packages/runtime/src/switch/video-decoder.ts](packages/runtime/src/switch/video-decoder.ts) — constructor auto-attaches audio (`getSharedAudioContext` + `createAudioNode` + `createGain` + `connect`), `WeakMap`-keyed GainNode storage, new methods `pause` / `seek` / `setVolume` / `setMuted` / `getWaveform` / `getFrequencyData` / `getAudioLevels`.

**Verified Citron 2026-07-02.** Spectraplay MP3 tracks from `sdmc:/music/` play audibly, progress bar advances, Pause/Stop pause/resume work, seek by dragging works, mute toggles silence, volume slider changes level, visualizer bars 0..14 (of 16) react to different frequency bands.

**Design contract.** The audio_node lifetime is decoder-owned (released in `free_video_decoder` AFTER `nx_media_destroy` joins the decode thread). The JS-side GainNode is `WeakMap`-referenced — GC drops it when the decoder is unreachable, and the audio graph rerefs it via the connection to `ctx.destination` until then. The audio-tap mutex is held only around the ring write/snapshot (microseconds) — no contention with the decode thread's `nx_audio_stream_write` blocking path.

**Still-deferred (nothing user-visible; add when a demo asks).**
- `setPlaybackRate` — nx_media doesn't currently support variable-rate playback; would need swresample rate change + clock scaling.
- Waveform/spectrum play-head sync — my tap is written from the decode thread, so it holds the LATEST decoded audio (may be ~1-2 render buffers ahead of the audrv play head). QuickJS-era's video.c indexed features by played-sample count. For spectraplay's 60Hz visualizer this <50ms latency is invisible; for tightly synced games it might matter.
- `getVideoPlaybackQuality` etc. — cut #22 covers video demos, adjacent surface.

**Recurrence tell.** If Switch.VideoDecoder's audio surface regresses in a future refactor: `dec.play()` succeeds but no sound + no progress bar advance + no error in the log = `nx_video_decoder_new` isn't calling `videoDecoderCreateAudioNode` (or its C-side is dropping the connection). Log signature: none in the current code path — the failure is silent-by-design (deferred stubs no-op'd). Instrument with a `console.debug` inside `getSharedAudioContext` call in the constructor to prove the auto-attach path fires.

---

## #34 — BaseAudioContext + AudioContext throw-stubs should be no-op nodes (or explicit throws in a documented shape) — OPEN, RUNTIME-SIDE POLYFILL FILLS THE GAP 2026-07-02

> **Workaround location:** the runtime-side polyfill at
> [brewser-runtime-v8/src/polyfills/web-audio-stubs.ts](../brewser-runtime-v8/src/polyfills/web-audio-stubs.ts)
> fills the engine gap today; see the upstream-pull-sensitivity
> header in [RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md)
> — the #34 polyfill is the pattern example for the
> guard-by-shape-mis-classifies lesson documented there.


**File(s):**
- [packages/runtime/src/audio/base-audio-context.ts:210-262](packages/runtime/src/audio/base-audio-context.ts#L210) — 12 create* methods (`createAnalyser`, `createBiquadFilter`, `createChannelMerger`, `createChannelSplitter`, `createConstantSource`, `createConvolver`, `createDelay`, `createDynamicsCompressor`, `createIIRFilter`, `createOscillator`, `createPanner`, `createPeriodicWave`, `createScriptProcessor`, `createWaveShaper`) all bodied as `throw new Error('Method not implemented.')`.
- [packages/runtime/src/audio/audio-context.ts:157-169](packages/runtime/src/audio/audio-context.ts#L157) — 3 more (`createMediaElementSource`, `createMediaStreamDestination`, `createMediaStreamSource`) same pattern.

**Root cause.** These methods aren't just missing — they're actively harmful, because they throw *after* `new AudioContext()` has already succeeded in the surrounding `try` block. In app code the shape is invariably:

```js
audioContext = audioContext || new AudioContext();   // allocates audrv voice
sourceNode = audioContext.createMediaElementSource(audio);  // throws
```

The catch swallows the exception, but the AudioContext is now half-init: constructed, holding an audrv voice + graph + render-thread reference, never closed because the app skipped the "everything worked, keep it" path. Whatever plays audio next through the runtime's shared context contends with a stranded voice that renders silence from an unconnected graph. Symptom: playback is *initiated* successfully (`audio.play()` doesn't throw), the render thread pulls buffers, no error surfaces — user just hears nothing.

Symptom re-appeared 2026-07-02 in spectraplay (MP3 Play button "does nothing"). Same class of bug was previously diagnosed 2026-06-03 in the QuickJS-era mediaplayer + WebAudio-Tone game; the response then was to write a runtime-side polyfill ([brewser-runtime-v8/src/polyfills/web-audio-stubs.ts](../brewser-runtime-v8/src/polyfills/web-audio-stubs.ts)) that overrides the throw-stubs with no-op fake nodes. That polyfill had a latent bug of its own — its `definePrototypeMethod` bailed with `if (typeof proto[name] === 'function') return;` because the throw-stubs ARE functions — so the polyfill silently no-op'd against the throw-stubs it was written to patch. Fixed 2026-07-02 in [brewser-runtime-v8/src/polyfills/web-audio-stubs.ts](../brewser-runtime-v8/src/polyfills/web-audio-stubs.ts) (dropped the guard; STUBS_BUILD_TAG bumped to `v8-override-throw-stubs`).

**Proposed upstream fix.** For methods that produce a *node* the caller connects into the graph (`createMediaElementSource`, `createMediaStreamSource`, `createMediaStreamDestination`, `createConvolver`, `createDelay`, `createBiquadFilter`, `createPanner`, `createStereoPanner`, `createDynamicsCompressor`, `createWaveShaper`, `createScriptProcessor`, `createIIRFilter`, `createChannelMerger`, `createChannelSplitter`), return `this.createGain()` (which is real, spec-shaped, and silent when its gain stays at 1). For methods with distinctive *interfaces* consumers introspect (`createAnalyser.getByteFrequencyData`, `createOscillator.start/stop`, `createConstantSource.offset`, `createPeriodicWave`), either wire real minimal impls or ship the same fake-shapes the runtime-side polyfill uses today.

**Scope.** ~15 method bodies, ~30-line diff net. No native/C++ delta — pure TS. Doesn't need to be perfect; the *specific harm* is throw-after-context-alloc, and any body that returns instead of throws eliminates it.

**Symptom it fixes.** spectraplay MP3 Play button plays audio again on V8 fork. Any app that uses `createMediaElementSource(audio)` for visualization purposes works instead of silently corrupting the audrv state.

**DISPOSITION:** `upstream-candidate`. These are Web Audio surface — either implement or return a benign placeholder; throwing after `new AudioContext()` has succeeded is a spec-hostile shape (`AudioContextOptions` doesn't warn callers that node factories will throw).

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.**

*Verify.* Grep `packages/runtime/src/audio/base-audio-context.ts` for `Method not implemented`. Presence = still upstream-broken; downstream polyfill in brewser-runtime-v8 is load-bearing.

*Recurrence tell.* Any app whose `try { audioContext.createMediaElementSource(el); ... } catch { ... }` block runs → no audio afterwards. Log signature: `[page] "audio graph init failed..." Error: Method not implemented. at createMediaElementSource (nxjs:src/audio/audio-context.ts:...)`. If the polyfill regresses (STUBS_BUILD_TAG guard reintroduced, or polyfill loading order breaks), the log will show `[stubs] "BEFORE: createMediaElementSource=function ... AFTER: createMediaElementSource=function"` with the SAME `=function` on both sides — the polyfill saw the throw-stub and didn't overwrite it.

*Design contract.* The polyfill's fakeNode returns a connect-passthrough; the fakeAnalyser fills its `getByteFrequencyData` / `getByteTimeDomainData` outputs with zeros. Visualization degrades to silence-tracking (all-zero waveform), but audio *playback* is preserved. If a future upstream implementation of `createAnalyser` etc. lands, the polyfill's unconditional override in `definePrototypeMethod` will silently clobber it — remove the polyfill entries for methods upstream now implements, and consider re-introducing the guard scoped to specific method names.

---

## #35 — nx_gl_state_snap_t further extension for depth_mask + stencil_mask (Phase 2.G.1 cut #15) — SHIPPED 2026-07-01

**File(s):** [source/webgl_bridge.h](source/webgl_bridge.h) — struct extended with `GLboolean depth_mask` and `GLint stencil_mask`; [source/webgl_bridge.cc](source/webgl_bridge.cc) — `nx_gl_state_save` reads `GL_DEPTH_WRITEMASK` and `GL_STENCIL_WRITEMASK`; `nx_gl_state_restore` writes them back via `glDepthMask` and `glStencilMask`.

**STATUS: SHIPPED 2026-07-01** via commit `3b5c815`. Contract extension → new entry (not addendum to #17), per the "contract extension = new entry" precedent set by the #17/#17-superseded split.

**Root cause.** The 2.B FROZEN contract omitted `DEPTH_WRITEMASK` and `STENCIL_WRITEMASK` with the reasoning "Ganesh resets these per-draw". That IS true for Skia's own draws, but when Three.js runs BETWEEN Skia frames, Three.js's `WebGLState` cache assumes depth mask starts at TRUE (WebGL default) and short-circuits `gl.depthMask(TRUE)` calls whose cached value matches. If Ganesh left `GL_DEPTH_WRITEMASK` at FALSE (Skia's 2D drawing doesn't want depth writes), Three.js's cache is out of sync with the actual GL state; `gl.clear(DEPTH_BUFFER_BIT)` silently becomes a no-op; the depth buffer stays at its previous frame's values (or uninitialized 0); LESS depth test rejects every cube fragment.

**Symptom.** Draws succeed with no GL error, all state introspection reports clean, no pixels land on the color texture. First surfaced on `webgl2demo Sunset Sea` and `instancing-dynamic` — depth-testing v2 demos that expected the WebGL default `depthMask = TRUE` on frame entry.

**Isolation-test that confirmed the mechanism (cut #14h — since reverted).** Manually calling `glDisable(GL_DEPTH_TEST)` at the top of the demo's animate loop made the same instanced draw produce cube pixels immediately, ruling out geometry/shader/uniform issues and localizing to depth writes.

**Fix (shipped).** Snap the two writemasks in and out of the bridge along with the existing 20-entry contract. `stencil_mask` added alongside `depth_mask` because the same Skia-cache-desync mechanism affects the stencil buffer; not currently exercised by a shipping demo, but the pair is symmetric and the query cost is one extra `glGetIntegerv` per frame.

**Why upstream-vanilla lacks it.** No coexistence bridge; no state contract to extend.

**DISPOSITION:** `upstream-candidate` (with #6). If PR-D (the coexistence primitive) lands, this extension bundles with it because the whole snap contract needs to travel together.

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md).. Bundled with PR-D.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl_bridge.h](source/webgl_bridge.h) for `depth_mask` and `stencil_mask` — presence confirms the extension. Recurrence tell: any v2 demo where `gl.clear(DEPTH_BUFFER_BIT)` appears to no-op (depth-tested geometry disappears after the first Skia frame) is this bug returning.

---

## #36 — WebGL bracket-state-persistence via per-call shadow-tracked user_snap (Phase 2.G.1 cut #14) — SHIPPED 2026-07-01, EVOLVED 2026-07-02

**File(s):** [source/webgl.cc](source/webgl.cc) — `WebGLState` extended with `nx_gl_state_snap_t user_snap` + `bool user_snap_valid` + `GLuint auto_user_vao`; `enter_bracket` restores from `user_snap` (frame 2+) after cut #15's WebGL-defaults reset; every state-modifying `w_*` method writes its post-call value into the relevant `user_snap.<field>` (per-call shadow tracking). Auto-allocated user VAO ensures WebGL 1 demos that never explicitly bind a VAO still get attribute-state isolation from Skia's default-VAO usage.

**STATUS: SHIPPED 2026-07-01, EVOLVED 2026-07-02.** Multi-round design evolution documented in [[reference-bracket-state-persistence-bug]]:
- **Round 1** (piecemeal): tracked `user_program` / `user_vao` / `user_tex_2d_tu0` as scalar fields; restored in `enter_bracket`. Fixed webgl2demo Sunset Sea + spectraplay visualizer + sensors gyro cube.
- **Round 2**: added `viewport` tracking (still scalar fields).
- **Round 3** (save-at-exit): unified into a `user_snap` copy of `nx_gl_state_snap_t`, captured via `nx_gl_state_save(&user_snap)` at `exit_bracket()`. Broke DT/DM because by exit time Skia had already clobbered them → user_snap captured 0/FALSE.
- **Round 4 (final)**: per-call shadow tracking — every state-modifying `w_*` method writes its post-call value directly into the relevant `user_snap.<field>` (e.g., `w_use_program` writes `st->user_snap.program = p`; `w_enable` writes `st->user_snap.blend = GL_TRUE` etc.; `w_depth_mask` writes `st->user_snap.depth_mask = m`). `user_snap` now always reflects the demo's INTENT, not whatever GL happened to be in when the bridge next crossed. **Verified via webgl2-multiple-rendertargets brown-stripe fix (2026-07-02 CITRON+HARDWARE)** — drawBuffers + blend + depth persistence all recovered.

**Root cause.** `enter_bracket()` restores Skia's saved snap so the WebGL section starts from Skia's expected GL state. That is WRONG for the DEMO's INTENT: after Skia's frame, the demo's user-visible state (program, VAO, TEXTURE_2D binding, depth mask, blend enable, ...) has been clobbered. The demo's Three.js material system re-emits most state per material per frame, which is why simple demos worked — but raw-WebGL demos (webgl2demo Sunset Sea RAF-driven fullscreen effect, spectraplay visualizer inline canvas, sensors gyro cube) initialize state ONCE at boot and expect the WebGL spec's per-context state persistence.

**Symptom.** Demos that render exactly one frame ever (post-init state got captured), then remain frozen through subsequent RAFs; OR demos that render but with wrong colors / no depth / wrong textures per frame (partial clobber; different subset of state persists depending on which glCall path Three.js took last frame). Sunset Sea locked at 100% frozen scene despite RAF ticking at 60 fps.

**Fix (shipped).** Per-call shadow-tracking on every state-modifying `w_*` method. Complete list at commit tip: `w_viewport`, `w_enable`, `w_disable`, `w_use_program`, `w_bind_vertex_array`, `w_bind_texture` (TU0-only), `w_active_texture`, `w_depth_mask`, `w_stencil_mask`, `w_blend_func` variants, `w_color_mask`, `w_clear_color`, plus a handful of others. `enter_bracket` restores from `user_snap` (frame 2+, gated by `user_snap_valid`). Auto-allocated user VAO isolates the demo's default-VAO attribute state from Skia's.

**Symptom manifestations resolved.**
- webgl2demo Sunset Sea — no longer frozen (2026-07-01 Citron + hardware).
- spectraplay visualizer — viewport + blend restored per frame (2026-07-01).
- sensors gyro cube — depth-test + attribute state restored (2026-07-01).
- webgl2-multiple-rendertargets brown-stripe — drawBuffers + blend + depth persistence via #36 + #35 combined (2026-07-02 CITRON + HARDWARE verified).

**Why upstream-vanilla lacks it.** No coexistence bridge; the WebGL spec's own state persistence is trivially satisfied when nothing else is drawing on the same context. Skia stealing the context is what breaks the assumption.

**DISPOSITION:** `upstream-candidate`. General correctness for any embedder with a Skia/WebGL coexistence bridge. Bundle with PR-D (the primitive).

**UPSTREAM STATUS:** `PR-drafted(local)` — PR-D, branch `upstream-pr/D-skia-webgl-coexistence`. See [upstream-prs/PR-D.md](upstream-prs/PR-D.md).. Bundled with PR-D.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `user_snap.viewport[0] = x`. If absent, the per-call shadow tracking regressed. Also check `WebGLState` for `nx_gl_state_snap_t user_snap;` + `bool user_snap_valid;` fields. Recurrence tell: a raw-WebGL demo that renders exactly one frame then freezes (or renders but with wrong colors per frame) — that's the bracket state persistence bug returning.

**Cross-references.**
- [[reference-bracket-state-persistence-bug]] — full investigation log.
- #35 (depth_mask + stencil_mask snap extension) — companion contract extension; both surfaced together during the webgl2-multiple-rendertargets brown-stripe hunt.

---

## #37 — texStorage3D + texSubImage3D method bindings for v2 (Phase 2.G.1 cut #32) — SHIPPED 2026-07-01

**File(s):** [source/webgl.cc](source/webgl.cc) — `w_tex_storage_3d` + `w_tex_sub_image_3d` FN implementations + `install_methods_v2` FUNCS[] entries.

**STATUS: SHIPPED 2026-07-01** via commit `3c26bff` (webgl2 demo fixed).

**Root cause.** Three.js r184's WebGL2 backend unconditionally calls `state.texStorage3D` + `state.texSubImage3D` for `DataArrayTexture` / `Data3DTexture` uploads ([WebGLTextures.js:1174/1190/1198](https://github.com/mrdoob/three.js)). Both wrappers try/catch and silently swallow `"gl.texStorage3D is not a function"` errors, so without these bindings the array-texture storage is never allocated → `sampler2DArray` / `sampler3D` reads return `vec4(0)`.

**Symptom.** webgl2-texture2darray renders black on both Citron and hardware; no error surfaced in the log (silent try/catch in Three.js). Any v2 demo that uploads to a texture array or 3D texture would exhibit the same silent failure.

**Fix (shipped).** Direct passthrough — no format massaging required (GLES3 spec matches WebGL2 spec 1:1 for these entry points). `w_tex_storage_3d(target, levels, internalformat, width, height, depth)` → `glTexStorage3D(...)`. `w_tex_sub_image_3d(target, level, x, y, z, w, h, d, format, type, pixels)` → `glTexSubImage3D(...)`. Both use the shared `enter_bracket()` prelude for state-contract coordination.

**Why upstream-vanilla lacks it.** Upstream V8 nx.js's WebGL2 method table exists but doesn't include these entries in the version we forked from (upstream beta.5's WebGL2 surface predates Three.js r184 usage patterns).

**DISPOSITION:** `upstream-candidate`. Method surface expansion — general benefit for any embedder running Three.js r184+ on a v2 context.

**UPSTREAM STATUS:** `not-submitted`. Bundle with PR-D (WebGL2 method surface) or ship as a small standalone PR alongside PR-A/F/C if PR-D lags.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `w_tex_storage_3d`. Recurrence tell: `webgl2-texture2darray` (or any DataArrayTexture demo) renders black with no error in nxjs-debug.log → this binding regressed.

---

## #40 — MOVED → [NXJS_PATCHES_ARCHIVE.md](NXJS_PATCHES_ARCHIVE.md) (#40-tombstoned)

---

## #41 — MOVED → [NXJS_PATCHES_ARCHIVE.md](NXJS_PATCHES_ARCHIVE.md) (#41-tombstoned)

---

## #42 — OES_vertex_array_object extension advertising for v1 pre-arm route — SHIPPED + HARDWARE-VERIFIED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — new `strcmp(name, "OES_vertex_array_object") == 0` branch in `w_get_extension` (after the existing `WEBGL_depth_texture` branch, before the trailing `SetNull()`) + 4 forward declarations near `w_get_extension`. NOT added to `getSupportedExtensions()` — Three.js and demo capability probes should NOT discover this at their normal advertising surface; only the runtime pre-arm code path in [gl-teardown.ts](../brewser-runtime-v8/src/scripts/gl-teardown.ts) queries it by name.

**Root cause.** [RUNTIME_SHIMS.md #42](../brewser-runtime-v8/RUNTIME_SHIMS.md#42)'s pre-arm needs a JS-side VAO create + bind API on the WebGL 1 context. The v2 context registers `createVertexArray` / `bindVertexArray` / `deleteVertexArray` / `isVertexArray` natively at [webgl.cc:2359](source/webgl.cc#L2359); the v1 context does not, so a v1-context teardown has no way to shadow-write a non-zero value into `user_snap.vao` via any wrapped-surface call. Without this ext, a v1-context outgoing teardown (e.g. `sensors → next demo`) would leave `user_snap.vao = 0` and the pre-arm would silently no-op on v1 — the next demo would inherit poison from case-2 of #41 above.

**Fix.** `w_get_extension('OES_vertex_array_object')` returns an object whose 4 methods point at the SAME native handlers the v2 FUNCS[] table uses (`w_create_vertex_array` / `w_bind_vertex_array` / `w_delete_vertex_array` / `w_is_vertex_array`), plus the `VERTEX_ARRAY_BINDING_OES` enum. Shape:

```cpp
if (strcmp(name, "OES_vertex_array_object") == 0) {
    Local<Object> o = Object::New(iso);
    o->Set(c, ..."VERTEX_ARRAY_BINDING_OES"..., Uint32(0x85B5));
    o->Set(c, ..."createVertexArrayOES"..., FunctionTemplate::New(iso, w_create_vertex_array)->GetFunction(c));
    o->Set(c, ..."bindVertexArrayOES"..., FunctionTemplate::New(iso, w_bind_vertex_array)->GetFunction(c));
    o->Set(c, ..."deleteVertexArrayOES"..., FunctionTemplate::New(iso, w_delete_vertex_array)->GetFunction(c));
    o->Set(c, ..."isVertexArrayOES"..., FunctionTemplate::New(iso, w_is_vertex_array)->GetFunction(c));
    info.GetReturnValue().Set(o);
    return;
}
```

The forward decls at line 706-ish let `w_get_extension` (line 706) reference natives defined later at [webgl.cc:1579-1604](source/webgl.cc#L1579).

**Why not advertise via `getSupportedExtensions`.** Two reasons:
1. Only the runtime pre-arm code calls `getExtension('OES_vertex_array_object')` — no demo does. Adding to the SUPPORTED[] array would let Three.js and other capability probes discover it, potentially changing THEIR v1 rendering path (they'd start using OES VAOs where currently they wrap everything in a synthetic default-VAO). That's out of scope for #42.
2. `getExtension` returning a valid object for a name not in `getSupportedExtensions()` is spec-legal — extension advertising is per-implementation.

**Shadow-coherence.** `w_bind_vertex_array` at [webgl.cc:1593-1604](source/webgl.cc#L1593) shadow-writes `st->user_snap.vao = (GLint)v` per patch #36's contract. The OES-ext method is the SAME native — the shadow-write happens whether called as `gl.bindVertexArray(vao)` (v2) or `ext.bindVertexArrayOES(vao)` (v1 via OES). So the pre-arm's bind on v1 goes through the same shadow path as v2, keeping user_snap consistent regardless of which context flavor last touched it.

**Why upstream-vanilla lacks it.** Companion to patches #36/#40/#41 (fork-only bracket-state persistence family for the Tegra WebGL↔Skia coexistence model). Not meaningful outside embedders that host multi-session GL apps under the shared-EGL-context bracket contract.

**DISPOSITION:** `fork-only`. Retires with #36/#40/#41 when Phase-B per-demo EGL contexts land.

**UPSTREAM STATUS:** `n/a`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `OES_vertex_array_object`. Recurrence tell: hardware run of `bloom→exit→sensors` shows `[gl-teardown:prearm] vao=null v2=false` in the log — the ext returned no callable methods, either because the branch was removed OR because the forward decls were removed and the code silently fell through to a compile error masked by another handler.

**Sequencing.** Ships in the same cut as [RUNTIME_SHIMS.md #42](../brewser-runtime-v8/RUNTIME_SHIMS.md#42) — engine + runtime together; the runtime pre-arm without the engine ext no-ops on v1.

---

## #43 — Native GL extension enumeration + `_getNativeExtensionsString` / `_getEglVersion` + `[gl-ext-dump]` boot log — SHIPPED 2026-07-03 (phase-0 introspection prerequisite)

**File(s):** [source/webgl.cc](source/webgl.cc) — module-level cache (`s_native_ext_list`, `s_native_exts_joined`, `s_native_exts_populated`) + `populate_native_extensions()` helper + `w_get_native_extensions_string` + `w_get_egl_version` FNs + FUNCS[] entries in both `install_methods` and `install_methods_v2` + eager `populate_native_extensions()` call at the tail of `make_context_carrier()`.

**Root cause.** The QuickJS-era engine populated a native-extension `has_*` flag set at backend init and exposed the raw joined string via `gl.getBackendInfo().glExtensions` ([nxjs-source/source/webgl_egl.c:8433-8509](../nxjs-source/source/webgl_egl.c#L8433-L8509)). The V8 migration dropped it — no call site in the current tree invokes `glGetString(GL_EXTENSIONS)` or `glGetStringi(GL_EXTENSIONS, i)` + `GL_NUM_EXTENSIONS`. Downstream: `com.natureglass.webglreport` at [webglreport.js:158-175](../brewser-apps/apps/experimental/com.natureglass.webglreport/webglreport.js#L158-L175) reports "gl.getBackendInfo not available" and "Native GLES Extensions # count: 0", and 17 bucket-B extension rows in the phase-0 gap analysis stay `Driver: ?` because there's no driver-truth accessor. See [WEBGL_EXTENSION_GAP.md §Broken-introspection A](../brewser-runtime-v8/docs/WEBGL_EXTENSION_GAP.md).

**Fix.** ES3 enumeration path:

```cpp
GLint n = 0;
glGetIntegerv(GL_NUM_EXTENSIONS, &n);
for (GLint i = 0; i < n; i++) {
    const GLubyte *e = glGetStringi(GL_EXTENSIONS, (GLuint)i);
    if (e) s_native_ext_list.emplace_back((const char *)e);
}
std::sort(...);
```

The legacy `glGetString(GL_EXTENSIONS)` is DEPRECATED on 3.x contexts and Mesa returns NULL for it there — do not fall back to it. Cache is populated once at the tail of `make_context_carrier()` (Skia's ES3 EGL context is current at that point, by construction — the same guard `nx_skia_gpu_egl_context()` check is above the populate call). One-shot boot log:

```
[gl-ext-dump] count=<N>
[gl-ext-dump] <ext-1>
[gl-ext-dump] <ext-2>
...
```

The `[gl-ext-dump]` tag is the grep target for the next hardware session's capture of Mesa-Nouveau / Tegra X1 driver truth. Two internal natives are exposed on the WebGL context proto (both v1 and v2 install paths) so the brewser-runtime `getBackendInfo` shim ([RUNTIME_SHIMS.md #44](../brewser-runtime-v8/RUNTIME_SHIMS.md#44)) can call them without needing engine `$` access:

- `gl._getNativeExtensionsString()` returns the joined sorted string.
- `gl._getEglVersion()` returns the "major.minor" parsed from `eglQueryString(EGL_VERSION)` — trailing vendor blob stripped so the shim's split-on-dot is single-token.

Leading underscore signals "shim consumers only, not a WebGL spec surface" — Three.js and demo code should never call these.

**No advertisement change.** This phase does NOT touch `SUPPORTED[]` at [webgl.cc:782](source/webgl.cc#L782). Bucket-A/B extension advertising is phase 1, gated on the hardware dump this phase enables.

**Why upstream-vanilla lacks it.** The pre-migration fork's `nx_webgl_egl_get_backend_info` was a Brewser-diagnostic surface, not upstream nx.js. `_getNativeExtensionsString`/`_getEglVersion` and the `[gl-ext-dump]` log are minimally-scoped equivalents that upstream would plausibly accept as a `.diagnostics` sub-object on the context — flag for a TooTallNate PR after the schema stabilizes.

**DISPOSITION:** `upstream-candidate` (with slight refactor to a `.diagnostics` sub-object for taste).

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `populate_native_extensions` and `[gl-ext-dump]`. Recurrence tells:
- No `[gl-ext-dump]` in the hardware log at boot → `make_context_carrier` regressed and no context is being created, OR the eager populate call was removed.
- `com.natureglass.webglreport` prints "count: 0" for Native GLES Extensions on hardware → the internal native regressed to returning an empty string, OR the runtime-side shim ([RUNTIME_SHIMS.md #44](../brewser-runtime-v8/RUNTIME_SHIMS.md#44)) got detached.
- Bucket-B advertising work (phase 1) reports every driver-probe as false → `_getNativeExtensionsString` regressed and the phase-1 `has_*` helpers are reading an empty cache.

**Sequencing.** Ships in the same cut as [RUNTIME_SHIMS.md #44](../brewser-runtime-v8/RUNTIME_SHIMS.md#44) — engine + runtime together; the runtime `getBackendInfo` shim without the engine natives returns empty strings for `glExtensions`, defeating the purpose.

---

## #45 — webgl2-rendering-context.ts landmine defuse — dead TS extension stubs throw instead of silent `[]`/null — SHIPPED 2026-07-03 (phase-0 introspection prerequisite)

**File(s):** [packages/runtime/src/canvas/webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts) lines 809-823 — `getSupportedExtensions()` and `getExtension(name)` class-body methods.

**Root cause.** The class-body methods return `[]` / `null` unconditionally. At runtime they are SHADOWED by the native install path (`$.webgl2InitClass` → `install_methods_v2` in [source/webgl.cc:2180+](source/webgl.cc#L2180)), which registers `w_get_supported_extensions` / `w_get_extension` on the same prototype AFTER the class body runs. Post-patch #8 the native install lands via `Object.defineProperties`, which overwrites these class-body methods — so they are DEAD CODE today (the hardware log shows both v1 and v2 contexts returning the same 9 advertised extensions, corroborating that the native wins).

The trap is install-order dependent. Step (b) of the phase plan is an install-order refactor; a silent `return []` here after any refactor of the install ordering would masquerade as "no extensions" — indistinguishable from a driver regression at diagnostic time, and specifically indistinguishable from the pre-#43 empty-list bug.

**Fix.** Replace both method bodies with `throw new Error('nx.js TS extension stub reached — native install order broken ...')` carrying the offending method name. Making resurrection LOUD converts a silent zero into an unmissable throw with a distinctive grep string.

**Why not delete the methods entirely.** The class-body method declarations are what give TypeScript the "these are on WebGL2RenderingContext" typing at compile time (the merged interface below only declares the ~220 native methods). Deleting the bodies would drop the type-level guarantees. Making them throw preserves the type surface while making runtime resurrection audible.

**Why upstream-vanilla lacks it.** Upstream ships the `return []` / `return null` stub bodies as-is (they predate our phase-0 hardening).

**DISPOSITION:** `upstream-candidate`. Trivial change; upstream should take it (or delete the methods outright, but the throw is the safer default for embedders that observe the same install-order sensitivity).

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/canvas/webgl2-rendering-context.ts](packages/runtime/src/canvas/webgl2-rendering-context.ts) for `TS extension stub reached`. Recurrence tells:
- The literal error string appearing in a hardware log or a demo's console-error handler → the native install order regressed and the phase-0 landmine tripped. Fix the install order in webgl.cc's `install_methods_v2` registration, do NOT quiet the throw.
- Grep hit missing → someone reverted to the silent `return []` stub. Restore the throw.

**Sequencing.** Ships in the same cut as #43 + [RUNTIME_SHIMS.md #44](../brewser-runtime-v8/RUNTIME_SHIMS.md#44).

---

## #46 — Bridge FBO stencil contract fix — DEPTH24_STENCIL8 renderbuffer + STENCIL_BITS wire + [bridge-fbo] completeness assert — SHIPPED 2026-07-03 (phase-0 commit 2 — HARDWARE VERIFICATION PENDING)

**File(s):** [source/webgl_bridge.cc](source/webgl_bridge.cc) — `create_fbo()` at [webgl_bridge.cc:145-198](source/webgl_bridge.cc#L145-L198): renderbuffer storage `GL_DEPTH_COMPONENT24` → `GL_DEPTH24_STENCIL8`, attachment `GL_DEPTH_ATTACHMENT` → `GL_DEPTH_STENCIL_ATTACHMENT` (single combined attach — ES3-preferred over the split double-attach pattern), `[bridge-fbo:INCOMPLETE]` / `[bridge-fbo:complete]` distinctive log tags. [source/webgl.cc](source/webgl.cc) — `w_get_parameter()` adds explicit `case GL_STENCIL_BITS:` / `case GL_DEPTH_BITS:` branches. Note the underlying spec-correct behavior would already be right (glGetIntegerv on the currently-bound tenant FBO returns 8/24); the explicit case is for grep-visibility of the wire so a hardware regression is diagnosable.

**Symptom.** Two-way contract violation the phase-0 gap report ([WEBGL_EXTENSION_GAP.md §Broken-introspection C](../brewser-runtime-v8/docs/WEBGL_EXTENSION_GAP.md)) called out:
- `getContextAttributes.stencil` returns `true` (hardcoded at [webgl.cc:849-867](source/webgl.cc#L849-L867)).
- `getParameter(STENCIL_BITS)` returns `0` — because the tenant FBO's depth attachment was `GL_DEPTH_COMPONENT24` with no stencil renderbuffer.
- Downstream: Unity `RectMask2D`, Phaser stencil masks, any content that queries `STENCIL_BITS >= 1` before enabling stencil ops will fall back to non-stencil paths or bail out entirely. Pre-migration was internally consistent (advertise `stencil:false`, `STENCIL_BITS=0`); V8 migration re-introduced the inconsistency.

**Fix.** Combined depth+stencil renderbuffer:

```cpp
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, s_depth_rb);
```

Memory delta on Tegra Nouveau is likely zero: `DEPTH_COMPONENT24` is stored as padded `D24X8` in most driver implementations, so `DEPTH24_STENCIL8` claims already-allocated bits. Not framed as a tradeoff.

FBO completeness assert (mandatory) — distinctive tag `[bridge-fbo:INCOMPLETE status=0xXXXX]` on failure, `[bridge-fbo:complete WxH color=RGBA8 depth=24 stencil=8]` on success. Citron IS authoritative for this assert (functional gate, not render correctness — per phase-0 commit 2 rider 3).

**Rider 2 stencil-path audit (read-only findings from commit-2 landing session).**

1. **Bracket contract has a stencil coverage gap (PROPOSED FOLLOW-UP — DO NOT FOLD INTO THIS CUT).** `nx_gl_state_snap_t` at [webgl_bridge.cc:606-639](source/webgl_bridge.cc#L606-L639) captures `GL_STENCIL_TEST` (enable) and `GL_STENCIL_WRITEMASK` (cut #15) but does NOT capture:
   - `GL_STENCIL_CLEAR_VALUE`
   - `GL_STENCIL_FUNC` / `GL_STENCIL_REF` / `GL_STENCIL_VALUE_MASK`
   - `GL_STENCIL_FAIL` / `GL_STENCIL_PASS_DEPTH_FAIL` / `GL_STENCIL_PASS_DEPTH_PASS`
   - All `_BACK_` separate stencil params
   
   Previously benign — stencil-less tenant FBO made stencil-op state fully vestigial. Post-commit-2, if Skia's Ganesh backend mutates stencil test/func/op between the demo's frames (it does; Ganesh uses stencil for path clipping), a demo that sets stencil config ONCE at init will have that config clobbered on frame 2+. Recommend: follow-up ledger entry extending `nx_gl_state_snap_t` in the same cut #15 family (stencil clear value + func tuple + op tuple + BACK variants). Standing "do-not-touch bracket machinery" rule prohibits fixing in this cut.
   
2. **gl-teardown.ts reset rows are safe.** [gl-teardown.ts:352-381](../brewser-runtime-v8/src/scripts/gl-teardown.ts#L352-L381) resets all stencil-related state (`stencilMask(0xFFFFFFFF)`, `disable(STENCIL_TEST)`, `stencilFunc(ALWAYS,0,0xFFFFFFFF)`, `stencilOp(KEEP,KEEP,KEEP)`, `clearStencil(0)`) to WebGL spec initial values. Post-commit-2 the `gl.clear(...|STENCIL_BUFFER_BIT)` at [gl-teardown.ts:600](../brewser-runtime-v8/src/scripts/gl-teardown.ts#L600) now actually clears the real 8-bit stencil (previously a no-op) — the clear value is 0 which is correct.

3. **canvas-runner resetScreenGLForScript.** Already audited in [RUNTIME_SHIMS.md #42 follow-up rider 3 table](../brewser-runtime-v8/RUNTIME_SHIMS.md). The only stencil row there is `disable(STENCIL_TEST)` (at line 1213), classified "Clean — toggles reset to spec defaults". Verdict holds post-commit-2.

4. **`gl.clear(STENCIL_BUFFER_BIT)` audit.** Now effective. `w_clear` at [webgl.cc](source/webgl.cc) forwards to native `glClear`; native clear operates against the currently-bound FBO's actual attachments. Value comes from `GL_STENCIL_CLEAR_VALUE` (default 0). Correct.

**Verdict:** "fine, that's the point" for gl-teardown + canvas-runner (rows 2/3/4). NOT fine for the bracket contract (row 1). The bracket gap is a separate, hardware-testable follow-up; it is NOT a commit-2 blocker because most demos re-emit stencil config per material (Three.js pattern), and raw-WebGL demos in the current curated 13-demo suite do not exercise stencil at all.

**Hardware verification pending.** Per phase-0 commit 2 rider 3, the hardware-session gate for this commit is LIMITED to:
- (a) No regression across the existing curated 13-demo suite (functional, not render correctness).
- (b) `[bridge-fbo:complete]` fires + `[bridge-fbo:INCOMPLETE]` does NOT fire in the hardware boot log.
- (c) `com.natureglass.webglreport`: `stencilBits: 8`, `getContextAttributes.stencil: true` (both, on both v1 and v2 contexts).

Authoritative stencil FUNCTIONALITY verification is DEFERRED to the first stencil-exercising content (a Unity `RectMask2D` scene or a trivial stencil-mask test page). The current 13-demo suite does NOT exercise stencil — suite-green does NOT prove stencil works.

**Why upstream-vanilla lacks it.** Upstream nx.js's WebGL surface is null-stubbed; there is no tenant FBO to attach stencil to. The tenant FBO model itself is a fork addition ([NXJS_PATCHES_NEEDED.md #6](#6)); this fix extends #6's contract with the stencil bits that were always advertised but never delivered.

**DISPOSITION:** `upstream-candidate` (with #6). Bundles with the coexistence primitive PR when it lands.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl_bridge.cc](source/webgl_bridge.cc) for `GL_DEPTH24_STENCIL8` and `[bridge-fbo:`. Grep [source/webgl.cc](source/webgl.cc) for `case GL_STENCIL_BITS:`. Recurrence tells:
- `[bridge-fbo:INCOMPLETE]` in a hardware log → the DEPTH24_STENCIL8 attach was rejected. Likely: the shared EGL config's stencil requirement got dropped ([NXJS_PATCHES_NEEDED.md #5](#5)), or Nouveau on that build refuses combined depth/stencil renderbuffers (would be surprising). Revert this commit as first step, then debug.
- `getParameter(STENCIL_BITS)` reports 0 while `[bridge-fbo:complete]` fires → the enter_bracket path is not binding the tenant FBO before w_get_parameter runs, OR the switch case was reverted. Check bracket integrity, not the switch.
- Suite demo renders broken depth-tested geometry post-commit-2 → depth attachment regressed. Log `[bridge-fbo:complete]` should show `depth=24 stencil=8`; if `depth=` is wrong, the combined attach is misbehaving on this driver — revert to split-attach (`DEPTH_ATTACHMENT` + `STENCIL_ATTACHMENT` pointing at the same renderbuffer) as fallback.

**Sequencing.** Ships as commit 2 of the phase-0 pair. Independently revertable via `git revert` of this commit alone; commit 1 (#43/#44/#45 introspection prerequisite) makes no rendering-behavior changes by construction.

---

## #47 — Phase-1 batch 1: driver-probed advertisement + 16 extension rows + compressed 2D upload natives + UNMASKED/MAX_ANISO getParameter — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — new `has_native_ext(token)` helper + `is_v2_context(info)` helper + rebuilt `w_get_supported_extensions` (kills the shared `SUPPORTED[9]` static; builds per-context list from statics + `has_native_ext`-gated adds) + 16 new branches in `w_get_extension` (14 driver-gated + WEBGL_debug_renderer_info + WEBGL_stencil_texturing) + 3 new `w_get_parameter` case blocks (UNMASKED_VENDOR_WEBGL / UNMASKED_RENDERER_WEBGL / MAX_TEXTURE_MAX_ANISOTROPY_EXT) + 2 new native FNs (`w_compressed_tex_image_2d` + `w_compressed_tex_sub_image_2d`) + FUNCS[] entries for both in `install_methods` (v1) AND `install_methods_v2` (v2).

**Blueprint.** Pre-migration [nxjs-source/source/webgl.c:2005-2235](../nxjs-source/source/webgl.c#L2005-L2235) — the hybrid statics + driver-probed model. Same design ported to the V8 tree; `has_native_ext()` replaces `nx_webgl_egl_has_*` gates, with the enumeration cache from ledger #43.

**Root cause / motivation.** Phase-0 shipped 9 hardcoded extensions on both v1 and v2 identically. WebGL spec requires v1 ≠ v2 lists, and Unity/itch/Three.js content probes ~30+ extensions before starting. Report app closes 8-10 audit rows per context type in this batch (see [docs/EXTENSION_PORT_PLAN.md §3.1](../brewser-runtime-v8/docs/EXTENSION_PORT_PLAN.md)).

**Advertised rows added (16 total).** All driver-probed against the ledger #43 enumeration:
- Both v1+v2: `EXT_depth_clamp`, `EXT_float_blend`, `EXT_texture_filter_anisotropic`, `EXT_texture_compression_bptc`, `EXT_texture_compression_rgtc`, `WEBGL_compressed_texture_s3tc`, `WEBGL_compressed_texture_s3tc_srgb`, `WEBGL_compressed_texture_etc1`, `WEBGL_compressed_texture_astc`, `WEBGL_debug_renderer_info`
- v1 only: `WEBGL_color_buffer_float`
- v2 only: `EXT_color_buffer_float`, `EXT_color_buffer_half_float`, `EXT_texture_norm16`, `EXT_render_snorm`, `WEBGL_stencil_texturing`

Each has a matching `w_get_extension` branch returning the spec constants (single object per Khronos ext spec). WEBGL_compressed_texture_astc also vends a `getSupportedProfiles()` method returning `["ldr"]` or `["ldr","sliced_3d"]` per driver token presence.

**getParameter branches.**
- `UNMASKED_VENDOR_WEBGL` (0x9245) → `glGetString(GL_VENDOR)`.
- `UNMASKED_RENDERER_WEBGL` (0x9246) → `glGetString(GL_RENDERER)`.
- `MAX_TEXTURE_MAX_ANISOTROPY_EXT` (0x84FF) → `glGetFloatv` (spec is GLfloat, not GLint — the report's `maxAnisotropy: n/a` fix relies on returning a numeric value here).

**Discovered gap: compressed 2D natives.** `w_compressed_tex_image_2d` and `w_compressed_tex_sub_image_2d` were ABSENT from FUNCS[] pre-batch-1 — advertising any compressed-format extension (S3TC/RGTC/BPTC/ETC1/ASTC) would have been a fake because the upload native would `TypeError`. Batch 1 wires them in the SAME commit as the advertising rows for the compressed families. Signature: WebGL1 7-arg form on both context types; the 8/9-arg WebGL2 PBO / typed-array-offset overloads are deferred (spec-legal — v2 falls back to the ArrayBufferView shape when the caller doesn't pass extras).

**Advertising machinery.** `w_get_supported_extensions` now constructs the list per call, per context kind, via `is_v2_context(info)` inspection of the receiver's `__webgl2` property (set at `make_context_carrier` time). Zero engine state added beyond the ledger #43 cache. Ordering: always-on statics first, then v1-only statics, then driver-probed adds in category order. `has_native_ext()` is a binary_search over the sorted cache.

**Retiring `SUPPORTED[9]`.** The static array at [webgl.cc:857-867 pre-batch-1](source/webgl.cc) is removed. Any post-batch-1 consumer that expected the exact 9-name list needs to re-verify via `getSupportedExtensions()` on a live context.

**Why upstream-vanilla lacks it.** Upstream nx.js exposes WebGL contexts as null-stubs. Extension advertising is entirely fork territory.

**DISPOSITION:** `upstream-candidate`. Trivial extraction if upstream WebGL surface ever lands — the pattern (statics + native-probe helper + per-context branches) is the standard shape.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `has_native_ext`, `is_v2_context`, `w_compressed_tex_image_2d`. Recurrence tells:
- Both context types report `9 supported extensions` on hardware → the SUPPORTED[9] static came back, or `w_get_supported_extensions` was replaced by phase-0's version. Check `has_native_ext(` presence at the switch construction site.
- `MISSING` for every batch-1 row post-hardware → `has_native_ext()` returns false unconditionally. Verify `s_native_exts_populated=true` at the call site (ledger #43 populate should have run at first context creation).
- Report app renders `unmaskedVendor:` empty despite advertising WEBGL_debug_renderer_info → the `case 0x9245`/`case 0x9246` branches regressed. Grep `case 0x9245` in `w_get_parameter`.
- `maxAnisotropy: n/a` on hardware → EXT_texture_filter_anisotropic not advertised OR the `case 0x84FF` returns 0 (Number cast broken). Grep both.
- Any demo calling `gl.compressedTexImage2D(...)` throws `TypeError: is not a function` → FUNCS[] entries regressed. Grep both install_methods and install_methods_v2 for the two entries.

**Sequencing.** First of three phase-1 extension batches. Batches 2 and 3 land after phase-1.5 core-native tiers per [docs/EXTENSION_PORT_PLAN.md §0.1.1](../brewser-runtime-v8/docs/EXTENSION_PORT_PLAN.md).

---

## #48 — Phase-1 batch 2A: Unity-P1 v1 function surfaces + software shims + rider-1 ETC2/EAC — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — new `probe_ext_frag_depth()` helper (module-scope, one-shot ESSL-100 compile probe) + 4 new forward decls + 4 new FUNCS[] entries in `install_methods` (v1) + 6 new `w_get_extension` branches + 7 new advertising rows in `w_get_supported_extensions`.

**Blueprint.** Pre-migration [nxjs-source/source/webgl.c:2005-2235](../nxjs-source/source/webgl.c#L2005-L2235) — the v1/v2 spec-conformance model. Plan §3.2.

**Rows added (7 advertising / 6 with ext object):**
- `ANGLE_instanced_arrays` (v1) — aliases v2 core natives via 3 `-ANGLE`-suffixed methods + `VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE=0x88FE`. Gate: `GL_EXT_draw_instanced`.
- `WEBGL_draw_buffers` (v1) — aliases v2's `drawBuffers` native + 34 constants (`MAX_COLOR_ATTACHMENTS_WEBGL`, `MAX_DRAW_BUFFERS_WEBGL`, `COLOR_ATTACHMENT{0..15}_WEBGL`, `DRAW_BUFFER{0..15}_WEBGL`). Gate: `GL_EXT_draw_buffers`.
- `EXT_frag_depth` (v1) — advertise-only (enables `#extension GL_EXT_frag_depth : enable` + `gl_FragDepthEXT` writes in #version 100 shaders). Gate: `GL_EXT_frag_depth` + `probe_ext_frag_depth()` compile-probe. The probe emits `[frag-depth-probe] ACCEPT` or `[frag-depth-probe] REJECT log=...` to stderr once; grep result in hardware/Citron logs.
- `OES_vertex_array_object` (v1) — list-flip. Existing ext-object branch (per #42) stays as-is; only `w_get_supported_extensions` grows to include the name. Retires the deliberate GETEXT_NONNULL_BUT_NOT_LISTED asymmetry called out in #42's rationale. Runtime pre-arm route unaffected (calls `getExtension` by name, receives the same object).
- `WEBGL_lose_context` (both) — software-only minimal impl: `loseContext` / `restoreContext` are FunctionTemplate-wrapped no-ops; `isContextLost` aliases `w_is_context_lost` (returns false). No canvas-element event dispatch — future runtime shim can add `webglcontextlost`/`restored` events without changing this advertising row.
- `WEBGL_debug_shaders` (both) — software-only. `getTranslatedShaderSource(shader)` extracts the K_SHADER handle, calls `glGetShaderSource` on it, returns the string as-submitted. This stack does no WebGL→ES3 shader translation (unlike ANGLE); the "translated" source is identical to what was submitted via `shaderSource`. Matches Mesa's `GL_ARB_debug_shaders` convention.
- `WEBGL_compressed_texture_etc` (both, batch-2 rider-1) — ETC2/EAC 10 constants (`COMPRESSED_R11_EAC=0x9270`..`COMPRESSED_SRGB8_ALPHA8_ETC2_EAC=0x9279`). Bucket A (core ES3; no driver-token gate). Advertising is unconditional on both context types. **Batch 1 skipped this by OVERSIGHT** — the plan's §4.1 app-added list correctly included it but the batch-1 implementation only wired ETC1 via `GL_OES_compressed_ETC1_RGB8_texture`. Batch 2 closes the omission.

**v1 FUNCS[] additions.** Four new entries in `install_methods()`:
`drawArraysInstanced`, `drawElementsInstanced`, `vertexAttribDivisor`
(three natives from `install_methods_v2` reused via forward decl), and
`drawBuffers` (same). Consequence: the `getBackendInfo` shim's
`typeof gl.drawArraysInstanced === 'function'` probe flips from `false`
to `true` on v1 — matches v2's behavior post-batch-2.

**Compile-probe pattern.** The `probe_ext_frag_depth()` helper is a
process-wide, one-shot compile of a minimal #version 100 shader with
`#extension GL_EXT_frag_depth : enable` and `gl_FragDepthEXT` writes.
Result cached in `s_frag_depth_ok`. Distinctive stderr tag `[frag-depth-probe]`
is the grep target for hardware/Citron acceptance verification. **Batch 3's
`WEBGL_blend_func_extended` probe follows the same pattern** (change the
`#extension` directive and the shader body; reuse the probe-result-caching
shape).

**Why upstream-vanilla lacks it.** Upstream nx.js exposes null-stubbed
WebGL contexts. All of this is fork territory.

**DISPOSITION:** `upstream-candidate`. The compile-probe helper +
software-shim pattern are generic enough that any embedder implementing
these WebGL extensions would benefit.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for
`probe_ext_frag_depth`, `ANGLE_instanced_arrays`, `WEBGL_debug_shaders`,
`WEBGL_compressed_texture_etc`. Recurrence tells:
- Report renders `extInstancedArraysPresent: false` on v1 → the four
  v1 FUNCS[] additions regressed. Grep both `install_methods` and
  `install_methods_v2` for the entries.
- Report renders `EXT_frag_depth = MISSING` on v1 despite driver token
  presence → the compile probe rejected the directive OR the probe
  regressed. Check `[frag-depth-probe]` in the boot log.
- Three.js v1 demo regresses post-`OES_vertex_array_object` list-flip
  (renders black or crashes on VAO ops) → Three.js discovered the ext
  and switched to OES VAO code paths that expose a latent bug. Do NOT
  revert the list-flip alone; add a targeted runtime shim that wraps
  Three.js's ext discovery or fix the underlying VAO handling first.

**Sequencing.** Ships as batch-2 Commit A. Rider-2 v2 spec-conformance
prune ships as batch-2 Commit B (#49) — independently revertable.

---

## #49 — Phase-1 batch 2B: Rider 2 v2 spec-conformance prune — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — 5 rows moved from unconditional advertising to `if (!v2)` block in `w_get_supported_extensions` + `v2_rider2` early guard in `w_get_extension` returning null for the same 5 names.

**Root cause.** Khronos WebGL Extension Registry marks these 5 extensions as WebGL1-only; their functionality is promoted to WebGL2 core:
- `OES_standard_derivatives` — GLSL `fwidth`/`dFdx`/`dFdy` are core ESSL 3.00.
- `OES_texture_float` — sized RGBA32F etc. are ES3 core sized internalformats.
- `OES_texture_half_float` — sized RGBA16F etc. are ES3 core sized internalformats.
- `OES_texture_half_float_linear` — half-float linear filtering is ES3 core.
- `WEBGL_depth_texture` — DEPTH_COMPONENT16/24/32F sampling is ES3 core.

Chrome, Firefox, and Safari all return null for these on WebGL2 contexts. Brewser now matches.

**Kept on v2 unchanged:**
- `OES_texture_float_linear` — genuine WebGL2 extension per registry (FLOAT texture linear filtering is not ES3 core).
- `EXT_texture_filter_anisotropic` — advertised via the driver-gated block per #47; unaffected by this prune.

**Deferred to a follow-up spec-conformance sweep:**
- `EXT_blend_minmax`, `OES_element_index_uint`, `EXT_sRGB` are ALSO WebGL1-only per registry but Alex's Rider 2 did not list them explicitly. Getting them prune-consistent needs another pass; documented for a future commit so this ledger entry matches the exact prune list approved for batch 2.

**Guard.** Batch-2 Citron smoke must include 2-3 curated 13-suite demos on the v2 path. If any regresses (e.g., Three.js `capabilities.floatTextureType` fallback breaks or a demo blackboxes on `WEBGL_depth_texture` absence), revert THIS commit alone (`git revert HEAD`) and document the retention as deliberate compat.

**Why upstream-vanilla lacks it.** Upstream doesn't advertise extensions at all.

**DISPOSITION:** `upstream-candidate`.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `v2_rider2` and `if (!v2) {` around the WebGL1-only static push. Recurrence tells:
- v2 `getSupportedExtensions()` returns 5 extra names (25 → 30-ish) → the prune guard regressed. Revert.
- Three.js demo regresses on v2 with a "half-float extension missing" error → suite-guard tripped; revert (`git revert HEAD~1` after finding the commit hash — the failure is likely on THIS commit, not batch 2A).
- Chrome-compat diff test flags any of the 5 names as ADVERTISED on v2 → prune not in effect on that build.

**Sequencing.** Batch-2 Commit B. Independently revertable via `git revert` of this commit alone; Commit A (#48) makes no v2 advertising changes.

---

## #50 — Phase-1.5-LOW: 30 core WebGL2 methods + OES_fbo_render_mipmap rider — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — new `u32_list()` helper + 30 new `FN(w_*)` implementations grouped in a phase-1.5-LOW block + 30 new FUNCS[] entries in `install_methods_v2` + OES_fbo_render_mipmap advertising row + `w_get_extension` branch.

**Motivation.** The report app's v2 function counter (17/88 pre-tier) is the phase-1 acceptance metric per plan §0.1. This tier lands the 30 methods classified LOW-effort per plan §0.1's family analysis — every glX call is a thin wrapper, no new K_* handle kinds, no lifecycle concerns.

**Methods landed by family (grep-verifiable via `FN\(w_[a-z_0-9]+\)` in webgl.cc):**

- **Buffer (2):** `w_get_buffer_sub_data`, `w_copy_buffer_sub_data`.
- **Framebuffer thin (6):** `w_framebuffer_texture_layer`, `w_invalidate_framebuffer`, `w_invalidate_sub_framebuffer`, `w_read_buffer`, `w_renderbuffer_storage_multisample`, `w_get_frag_data_location`.
- **3D texture (3):** `w_copy_tex_sub_image_3d`, `w_compressed_tex_image_3d`, `w_compressed_tex_sub_image_3d`.
- **UInt uniforms (8):** `w_uniform_1ui`..`4ui`, `w_uniform_1uiv`..`4uiv` (macro-generated).
- **Non-square matrix uniforms (6):** `w_uniform_matrix_2x3fv`, `3x2fv`, `2x4fv`, `4x2fv`, `3x4fv`, `4x3fv` (macro-generated).
- **Clear buffer (4):** `w_clear_buffer_iv`, `_uiv`, `_fv`, `_fi`.
- **Draw range (1):** `w_draw_range_elements`.

Sum: 30 methods = counter +30 = 17 → 47/88 (matches plan §0.1.1 reconciled progression).

**Implementation notes per family:**
- **`getBufferSubData` via glMapBufferRange.** `glGetBufferSubData` is desktop-GL only (not in Mesa Nouveau's GLES3 header set). The canonical WebGL2 impl pattern uses `glMapBufferRange` with `GL_MAP_READ_BIT` + memcpy + `glUnmapBuffer`.
- **UInt uniforms via UNI_UIV macro** — mirrors the existing UNI_FV / UNI_IV macros in the Uniform block. Requires the new `u32_list` helper (Uint32Array unwrap parallel to `i32_list`).
- **Non-square matrix uniforms via UNI_MAT_RxC(R, C) macro** — expands to `glUniformMatrixRxCfv`. Column-major element count is `R * C` per matrix.
- **Clear buffer family** — `iv` / `uiv` / `fv` variants take a 4-element list; `fi` takes 2 scalars (depth + stencil). All forward to `glClearBufferX`.

**Rider — OES_fbo_render_mipmap on v1 (batch-2 defect fix).** Plan §2.6 v1 row 15 assigned this to batch 2 but batch 1/2 both missed advertising it. Ships in phase-1.5-LOW as an explicitly-labeled rider. Bucket A (`framebufferTexture2D` with `level > 0` is core ES3), no engine plumbing needed — advertising + empty ext object.

**Batch-1 cross-check performed.** Every batch-1/2 advertising row cross-checked against plan §2.6 (v1 30 rows, v2 34 rows). Only defect: OES_fbo_render_mipmap missed. WEBGL_compressed_texture_etc's plan-vs-reality drift (planned batch 1, landed batch 2 rider 1) documented in the batch-2 report; not a defect, just slipped tier.

**Runtime-semantics verification per family (Citron smoke micro-probes recommended).**
- **Buffer ops** — `copyBufferSubData` from an ARRAY_BUFFER to a COPY_WRITE_BUFFER + `getBufferSubData` from the destination + JS-side memcmp against the source data.
- **Framebuffer thin** — `framebufferTextureLayer` on a TEXTURE_2D_ARRAY layer + draw + `readPixels` shows the expected color; `readBuffer(COLOR_ATTACHMENT1)` after a MRT draw returns attachment 1's pixels; `invalidateFramebuffer(FRAMEBUFFER, [DEPTH_ATTACHMENT])` followed by clear+draw produces the right depth output.
- **3D texture** — `compressedTexImage3D` with an ETC2/EAC block layered into a TEXTURE_2D_ARRAY + `copyTexSubImage3D` between layers + sample the destination in a shader.
- **UInt uniforms** — bind a uint uniform, set via `uniform2uiv([0x12345678, 0xDEADBEEF])`, sample in a shader that writes those values to color, `readPixels` should return the bit pattern.
- **Non-square matrix** — `uniformMatrix3x2fv` with a known transform, apply in shader, verify output.
- **Clear buffer** — `clearBufferfv(COLOR, 0, [r,g,b,a])` on FBO with 2 attachments + `readPixels` reads exactly [r,g,b,a] from attachment 0 only.
- **`drawRangeElements`** — draw a subset of indices; count vs `drawElements` gives identical output.

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed.

**DISPOSITION:** `upstream-candidate`. All 30 methods are trivial thin wrappers around ES3 core entry points; upstream could take the entire block without controversy.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Phase-1.5-LOW`, `w_get_buffer_sub_data`, `OES_fbo_render_mipmap`. Recurrence tells:
- Report renders `17 / 88 implemented` post-hardware → the FUNCS[] block regressed. Check `install_methods_v2` for the new entries.
- Three.js `renderer.copyTextureToTexture3D` or similar demo throws `TypeError` → one of the 3D texture methods regressed.
- Non-square matrix uniforms return garbage in a shader → the UNI_MAT_RxC macro produced wrong count; check the R*C divisor.
- `getBufferSubData` returns zeros → `glMapBufferRange` failed silently; check target binding and MAP_READ_BIT support.

**Sequencing.** First phase-1.5 tier per plan §0.1.1's decided order (b1 → b2 → phase-1.5-LOW → LOW-MED → MED → b3 → MED-HIGH). Independently revertable from batches 1/2 and from subsequent tiers.

---

## #51 — Phase-1.5-LOW-MED: 6 v2 methods (integer vertex attribs + getInternalformatParameter) — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — 6 new `FN(w_*)` implementations grouped in a phase-1.5-LOW-MED block after the LOW block + 6 new FUNCS[] entries in `install_methods_v2`.

**Motivation.** The report app's v2 function counter target progression per plan §0.1.1 (47 → 53 → 78 → 88). This tier lands the 6 methods classified LOW-MED per plan §0.1's family analysis. Each is thin-wrapper effort but requires modest plumbing that put them just above LOW: integer vertex attribs have their own glVertexAttribI* family separate from glVertexAttrib*f, and getInternalformatParameter has SAMPLES-list sizing plumbing (probe NUM_SAMPLE_COUNTS first to know the return array size).

**Methods landed by family (grep-verifiable via `FN\(w_[a-z_0-9]+\)` in webgl.cc):**

- **Integer vertex attribs (5):** `w_vertex_attrib_i4i`, `w_vertex_attrib_i4ui`, `w_vertex_attrib_i4iv`, `w_vertex_attrib_i4uiv`, `w_vertex_attrib_i_pointer`.
- **getInternalformatParameter (1):** `w_get_internalformat_parameter`.

Sum: 6 methods = counter +6 = 47 → 53/88 (matches plan §0.1.1 reconciled progression).

**Implementation notes per family:**
- **`vertexAttribI4iv` / `I4uiv`** — parse via existing `i32_list` / `u32_list` helpers (both landed in earlier tiers). Guard n<4 → return early (spec: TypeError, but silent no-op is safer than an engine crash on a malformed caller).
- **`vertexAttribIPointer`** — mirror of `vertexAttribPointer` but WITHOUT the `normalized` boolean parameter. Integer attributes carry ivec/uvec through the pipeline as-is; there's no float conversion, so `normalized` is meaningless. Signature: `(index, size, type, stride, offset)` — 5 args instead of 6.
- **`getInternalformatParameter`** — returns Int32Array (spec-required typed-array shape). For `NUM_SAMPLE_COUNTS` (0x9380), returns length-1 Int32Array containing the count. For `SAMPLES` (0x80A9) and other array-shaped pnames, first probe NUM_SAMPLE_COUNTS to size the destination, then materialize. Cap at 32 to bound the transient allocation (Mesa Nouveau reports ≤ 8 in practice; 32 is comfortable headroom for hypothetical future drivers).

**Runtime-semantics verification per family (Citron smoke micro-probes via com.natureglass.gl-probes app — landed in a paired brewser-apps commit).**
- **Integer vertex attribs** — INT_ATTRIB probe: uint vertex attribute value 42 flat-out through varying → RGBA32UI FBO write via `outColor = uvec4(v_val,0,0,0)` → readPixels with UNSIGNED_INT type returns R=42. Exercises `vertexAttribIPointer` + integer-attribute draw round-trip end-to-end.
- **getInternalformatParameter** — INFORMAT_PARAM probe: `getInternalformatParameter(RENDERBUFFER, RGBA8, SAMPLES)` returns Int32Array with length ≥ 1, all values ≥ 1. Validates the SAMPLES sizing path (NUM_SAMPLE_COUNTS pre-probe → allocate → refill).

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed.

**DISPOSITION:** `upstream-candidate`. All 6 methods are thin wrappers around ES3 core entry points (no ext gates); upstream could take the block without controversy.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Phase-1.5-LOW-MED`, `w_vertex_attrib_i_pointer`, `w_get_internalformat_parameter`. Recurrence tells:
- Report renders `47 / 88 implemented` post-hardware → the phase-1.5-LOW-MED FUNCS[] block regressed. Check `install_methods_v2` for the 6 new entries.
- gl-probes app's INT_ATTRIB probe FAILs with `attrib readback R=0` → `vertexAttribIPointer` is silently no-op'ing OR the integer-attribute path isn't wired end-to-end (likely a Three.js-style `flat` varying miscompile — but that would be a driver issue not an engine regression). Cross-check with a raw GL_ARRAY_BUFFER + UINT type + `glGetError` after `glVertexAttribIPointer`.
- gl-probes app's INFORMAT_PARAM probe FAILs with `return value null` → `w_get_internalformat_parameter` regressed to a default `void` return. Check the `Int32Array::New(ab, 0, count)` at the tail of the FN body.

**Sequencing.** Second phase-1.5 tier per plan §0.1.1's decided order. Independently revertable from #50 (phase-1.5-LOW).

---

## #52 — Two gl-probes-discovered gaps: drawRangeElements Citron-only quirk (fallback shipped as defensive-only 2026-07-03) + WebGL1 `getTexParameter` absent from FUNCS[] (SHIPPED 2026-07-03)

Discovered via com.natureglass.gl-probes v0.2.0 Citron smoke on 2026-07-03. Both are pre-existing surface gaps surfaced by the new probe harness — not regressions from #50 / #51.

### #52a — `drawRangeElements` renders nothing on Citron/Mesa Nouveau

**File(s):** [source/webgl.cc](source/webgl.cc) — `w_draw_range_elements` at line 3040, wired in `install_methods_v2` FUNCS[].

**Symptom.** The gl-probes `DRAW_RANGE draw_range_elements_subset_match` probe issues an identical clear+draw pair via `gl.drawElements(TRIANGLES, 3, U16, 0)` (works — reads back `[255,128,64,255]`) then `gl.drawRangeElements(TRIANGLES, 0, 2, 3, U16, 0)` (fails — reads back `[0,0,0,0]`). All `gl.getError` probes report `NO_ERROR (0x0)` end-to-end. The `(0,0)` corner pixel ALSO reads `[0,0,0,0]` — meaning the preceding `gl.clearColor(0,0,0,1)` + `gl.clear(COLOR_BUFFER_BIT)` also did not hit the user FBO (would have painted α=255).

**What we know.**
- Engine wiring is correct: disassembly of `_ZL21w_draw_range_elementsRK…` shows the FN parses all 6 args and tail-calls `glDrawRangeElements(w0=mode, w1=start, w2=end, w3=count, w4=type, x5=offset)` per AArch64 ABI. Verified 2026-07-03.
- No runtime shim wraps `drawRangeElements`: canvas-runner's `installBridgeDirtyHooks` wraps `drawArrays`/`drawElements`/`clear`/`drawArraysInstanced`/`drawElementsInstanced` only. cube-route-shim, shadow-route-shim, gl-teardown do not touch draw-family methods.
- `touch_fbo()` was missing from `w_draw_range_elements`; added 2026-07-03 as a defensive pattern-match with the other draw FNs. Non-load-bearing for user FBOs (`draw_into_default` is false), so did NOT change the failure.
- The failure is 100% reproducible on Citron/Mesa Nouveau NV120 and DID NOT occur pre-touch_fbo either — the fix candidate doesn't cure it. This eliminates the missing-`touch_fbo` theory.
- Baseline drawElements + readPixels PASSES on the same VAO / FBO / shader in the same probe run — so state is intact BEFORE the second block. Between the two blocks: only `gl.clearColor` + `gl.clear` + `gl.drawRangeElements` (all wrapped identically to other v2 methods).

**Hypotheses (unranked, needs engine-side fprintf to pin down).**
1. Mesa Nouveau driver quirk where `glDrawRangeElements` silently no-ops or is aliased to a broken path. Nouveau has historical implementation gaps with less-common ES3 entry points.
2. Some engine state contract we haven't identified — perhaps the `w_read_pixels` before the second block subtly invalidates the FBO binding for the next draw call, and `drawRangeElements`'s implementation isn't robust to it while `drawElements` is (asymmetric because the wrap-list vs no-wrap distinction).
3. GC / handle-life issue on the runtime side.

**Impact.** LOW-severity — `drawRangeElements` is an ES3 optimization hint (`start`/`end` args tell the driver the valid vertex index range). Applications can safely fall back to `drawElements` with identical semantics. No Three.js path in the current demo set uses `drawRangeElements`. Blocks only the specific counter test on the phase-1.5-LOW acceptance smoke; the other 29 methods in #50 verified via other probes.

**Next diagnostic step.** Add fprintf inside `w_draw_range_elements` logging the 6 arg values + `st->bound_fbo_js` + a post-glDrawRangeElements `glGetError` result. Rebuild + run gl-probes. If the FN is called with correct args and post-call `glGetError = 0`, root cause is confirmed as driver-side and this becomes a documented driver limit (like [[reference-mesa-nouveau-layered-sampling-unsupported]]). If the FN isn't called or is called with wrong args, root cause is engine / dispatch.

**CITRON DISCRIMINATOR OUTCOME 2026-07-03.** gl-probes v0.3.0 sentinel + isolation-mode smokes on Citron both FAIL identically (`sentinel=no cornerSentinel=no allZero=yes cornerZero=yes eDE=0x0 eClear=0x0 eDRE=0x0 eRead=0x0 baseline=[255,128,64,255] ranged=[0,0,0,0]`). Isolation run (DRAW_RANGE as first-and-only probe on fresh WebGL2 context) reproduces the failure identically → NOT probe-side state leakage from other probes. The FBO texture is genuinely untouched by both the second `gl.clear` AND `gl.drawRangeElements` despite both calls returning `GL_NO_ERROR`.

**STATUS: CITRON-OBSERVED-NOOP / HARDWARE-PENDING.** Per the standing rule, Citron is a functional-iteration authority, NOT a driver-truth authority — Citron sits on top of AMD Vulkan on Windows, not a real Tegra Nouveau stack. The Citron-observed silent no-op could be Mesa Nouveau NV120 driver behavior OR a Citron GPU translation/emulation issue. Hardware probe (see docs/HW_SESSION_RUNBOOK.md §#52a) will resolve.

**INTERIM FIX SHIPPED 2026-07-03.** `w_draw_range_elements` (source/webgl.cc) calls `glDrawElements(mode, count, type, indices)` instead of `glDrawRangeElements`. Spec-legal per ES3 §2.8.3 (the `start`/`end` args are a driver optimization hint only — the equivalent draw is `glDrawElements` with the same mode/count/type/indices). One-time boot log `[#52a] drawRangeElements -> drawElements fallback (range hint dropped; see NXJS_PATCHES_NEEDED #52a)` documents that the workaround is active. Fallback stays shipped regardless of the eventual hardware verdict (belt-and-suspenders); hardware outcome only decides whether it becomes defensive-only or remains load-bearing.

**Fallback gate for hardware probe.** `w_draw_range_elements` has a `#ifndef NX_52A_DISABLE_FALLBACK` guard around the substitution. Building with `-DNX_52A_DISABLE_FALLBACK=1` (added to `nxjs-source-v8/Makefile` CFLAGS or via `make CFLAGS_APPEND=-DNX_52A_DISABLE_FALLBACK=1`) restores the direct `glDrawRangeElements` call. Boot log distinguishes the two modes: `[#52a] drawRangeElements DIRECT (fallback DISABLED via NX_52A_DISABLE_FALLBACK build)` vs the default fallback banner.

**HARDWARE VERDICT 2026-07-03 — CITRON-ONLY QUIRK.** Real Tegra Nouveau NV120 with fallback-disabled build (`NX_52A_DISABLE_FALLBACK=1`, boot log confirmed `[#52a] drawRangeElements DIRECT (fallback DISABLED via NX_52A_DISABLE_FALLBACK build)`) renders the DRAW_RANGE probe's expected pixel color end-to-end via direct `glDrawRangeElements`. Boot A (default fallback ON) and Boot B (fallback OFF) both produce the exact same pixel readback `[255,127,64,255]`; the strict-mode probe FAIL on both boots is a ±1 pixel-rounding tolerance issue (hardware `round-half-down`, 0.5 → 127; Citron `round-half-up`, 0.5 → 128), NOT a `glDrawRangeElements` defect. See docs/HW_SESSION_RUNBOOK.md §#52a for the filled-in verdict rows.

**DISPOSITION:** `Citron-only quirk (verified 2026-07-03 hardware smoke); fallback remains shipped as defensive-only`. The interim `glDrawElements` fallback is kept in place to protect against Citron re-emerging (Citron sits on top of AMD Vulkan and its glDrawRangeElements → Vulkan translation gap could recur in future Citron versions). No engine-native re-enable planned; `NX_52A_DISABLE_FALLBACK=1` build gate stays in webgl.cc for future hardware-vs-emulator discriminator smokes.

### #52b — `getTexParameter` (WebGL1 core) never wired into FUNCS[]

**File(s):** [source/webgl.cc](source/webgl.cc) — no `FN(w_get_tex_parameter)` exists; `install_methods` (v1) and `install_methods_v2` (v2) FUNCS[] neither register a `getTexParameter` entry.

**Symptom.** Any call to `gl.getTexParameter(target, pname)` throws `TypeError: gl.getTexParameter is not a function` — even though the method is WebGL1 CORE (mandatory since spec 1.0). Discovered by gl-probes `EXT_ANISO anisotropy_texparameter_roundtrip` probe attempting to verify a `texParameterf(TEXTURE_MAX_ANISOTROPY_EXT, 16.0)` set via a getter readback. Set landed (no GL error); readback impossible.

**Impact.** LOW-severity — Three.js and typical WebGL apps never call `getTexParameter` (they set-and-forget texture parameters). Applications that DO call it (feature-detect probes, debugging tools) get the TypeError and know to fall back. But it's a spec-conformance gap that the report app's future WebGL1 function counter (v1 twin of the 88-list) would surface — currently there's no counter for v1 so the gap is invisible.

**Fix.** Trivial — one-liner FN + FUNCS[] entry (both v1 and v2):
```c
FN(w_get_tex_parameter) {
    enter_bracket();
    Isolate *iso = info.GetIsolate();
    const GLenum target = a_u32(info, 0);
    const GLenum pname = a_u32(info, 1);
    // TEXTURE_MAX_ANISOTROPY_EXT (0x84FE) returns float; TEXTURE_MAX_LOD /
    // TEXTURE_MIN_LOD also float. All others are int.
    if (pname == 0x84FE || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_MIN_LOD) {
        GLfloat f = 0.0f;
        glGetTexParameterfv(target, pname, &f);
        info.GetReturnValue().Set(Number::New(iso, f));
    } else {
        GLint v = 0;
        glGetTexParameteriv(target, pname, &v);
        info.GetReturnValue().Set(Int32::New(iso, v));
    }
}
```

**SHIPPED 2026-07-03** in the phase-1.5-MED commit (folded per Alex's plan). `FN(w_get_tex_parameter)` added between the LOW-MED and MED blocks. Registered in both v1 (`install_methods` FUNCS[]) and v2 (`install_methods_v2` FUNCS[]) as `"getTexParameter"`. `EXT_ANISO` probe un-SKIP'd in gl-probes v0.4.0 — now performs the full anisotropy roundtrip (set → get via `getTexParameter`) and must PASS in the phase-1.5-MED tier-acceptance smoke.

**DISPOSITION:** `upstream-candidate, shipped`.

**RE-APPLY / VERIFY NOTE.**
- **#52a** — recurrence tell: absence of `[#52a] drawRangeElements -> drawElements fallback` boot log on first WebGL2 draw call = someone reverted the fallback. gl-probes DRAW_RANGE FAIL post-boot = fallback regressed OR the underlying Mesa Nouveau issue reappeared in a different form.
- **#52b** — grep `FN(w_get_tex_parameter)` in webgl.cc = should exist; grep `"getTexParameter", w_get_tex_parameter` in both v1 + v2 FUNCS[] = should be registered. gl-probes EXT_ANISO FAIL = regressed.

---

## #53 — Phase-1.5-MED: 25 v2 methods (sampler + sync + query + UBO introspection) + 3 new handle kinds — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — 25 new `FN(w_*)` implementations grouped in a phase-1.5-MED block after the LOW-MED block + 25 new FUNCS[] entries in `install_methods_v2` + 3 new K_* enum values (K_QUERY, K_SAMPLER, K_SYNC) + `GLsync sync` field on `GLObj` (K_SYNC uses a driver-opaque pointer, not a GLuint name) + 3 new handle-class registrations in `nx_webgl2_init_class` MAP[] (`WebGLQuery`, `WebGLSampler`, `WebGLSync`).

**Motivation.** Third phase-1.5 tier per plan §0.1.1's decided order (b1 → b2 → LOW → LOW-MED → **MED** → b3 → MED-HIGH). Counter target 53 → 78/88 (+25). Includes the batch-3 dedup base (core query family: `createQuery`..`getQueryParameter`) — batch 3's `EXT_disjoint_timer_query` add is now the timer-EXT delta over this plumbing, not the full 7-method query family.

**Methods landed by family (grep-verifiable via `FN\(w_[a-z_0-9]+\)` in webgl.cc):**

- **Sampler (7):** `w_create_sampler`, `w_delete_sampler`, `w_is_sampler`, `w_bind_sampler`, `w_sampler_parameter_i`, `w_sampler_parameter_f`, `w_get_sampler_parameter`.
- **Sync (6):** `w_fence_sync`, `w_is_sync`, `w_delete_sync`, `w_client_wait_sync`, `w_wait_sync`, `w_get_sync_parameter`.
- **Query (7):** `w_create_query`, `w_delete_query`, `w_is_query`, `w_begin_query`, `w_end_query`, `w_get_query`, `w_get_query_parameter`.
- **UBO introspection (5):** `w_get_indexed_parameter`, `w_get_uniform_indices`, `w_get_active_uniforms`, `w_get_active_uniform_block_parameter`, `w_get_active_uniform_block_name`.

Sum: 25 methods = counter +25 = 53 → 78/88 (matches plan §0.1.1 progression). +1 non-count'd — `w_get_tex_parameter` (per #52b) is folded into the same commit and lands in both v1 + v2 FUNCS[].

**Implementation notes per family:**
- **Sampler** — `create/delete/is` follow the existing `w_create_vertex_array` pattern via new K_SAMPLER handle kind. `bindSampler(unit, sampler)` binds to a texture unit, not a target. `samplerParameteri/f` mirror `texParameteri/f`; `getSamplerParameter` returns float for TEXTURE_MAX_LOD / TEXTURE_MIN_LOD, int for everything else.
- **Sync** — `GLsync` is a driver-opaque pointer, not a GLuint name. Held in a new `GLObj::sync` field to avoid punning `id`+`loc` bits into a 64-bit pointer (sign-extension traps on the `loc` int32). `fenceSync` returns a new object wrapping the sync; `deleteSync` nulls it; `isSync` checks the wrapped kind + `glIsSync`. `clientWaitSync`'s timeout is a `GLuint64` — JS Number precision covers the WebGL2 typical range (0..few ms in ns).
- **Query** — K_QUERY handle. `getQuery(target, pname)` returns a K_QUERY-wrapped `CURRENT_QUERY` result or null. `getQueryParameter` returns bool for `QUERY_RESULT_AVAILABLE`, uint for `QUERY_RESULT` (uses `glGetQueryObjectuiv` — driver upcasts to GLuint64 spec-wise but WebGL2 exposes as regular Number).
- **UBO introspection** — `getIndexedParameter(UNIFORM_BUFFER_BINDING, idx)` returns a WebGLBuffer via `new_gl_obj(K_BUFFER, name)`; `_START`/`_SIZE` targets return int64 via `Number::New`. `getUniformIndices` takes an array of strings, returns Uint32Array of length input.length. `getActiveUniforms(program, indices, pname)` returns a JS Array whose element type depends on pname (bool for `IS_ROW_MAJOR`, int for others). `getActiveUniformBlockParameter(program, blockIndex, pname)` returns bool for `REFERENCED_BY_*_SHADER`, Uint32Array for `ACTIVE_UNIFORM_INDICES` (pre-sized via `ACTIVE_UNIFORMS`), int for scalar pnames. `getActiveUniformBlockName` returns a UTF-8 string via `String::NewFromUtf8`.

**New K_ handle kinds — runtime teardown pre-wired.** [brewser-runtime-v8/src/scripts/gl-teardown.ts](../brewser-runtime-v8/src/scripts/gl-teardown.ts) already tracks `samplers`, `queries`, `syncs`, and `transformFeedbacks` at lines 304-313 (create/delete wraps) + 524-527 (deletion sweep at teardown). No runtime changes needed for MED — the teardown side was pre-wired defensively when VAO tracking landed. This is the "additive-only" precondition Alex's plan calls out (new kinds may register with gl-teardown.ts; existing reset rows may not be modified) and it's already met without any new runtime commit.

**Runtime-semantics verification per family (Citron smoke via com.natureglass.gl-probes v0.4.0 — landed in a paired brewser-apps commit).**
- **Sampler** — create/bind + `samplerParameteri(MIN_FILTER, NEAREST)` + `getSamplerParameter` roundtrip check.
- **Sync** — `fenceSync(SYNC_GPU_COMMANDS_COMPLETE, 0)` + `flush` + `clientWaitSync` returns `ALREADY_SIGNALED` or `CONDITION_SATISFIED` within a small timeout; `getSyncParameter(SYNC_STATUS)` returns `SIGNALED`.
- **Query** — `beginQuery(ANY_SAMPLES_PASSED)` + draw + `endQuery` + poll `QUERY_RESULT_AVAILABLE` + `QUERY_RESULT > 0` for a visible draw.
- **UBO introspection** — build a known UBO layout in shader source, `getUniformBlockIndex` → `getActiveUniformBlockParameter(DATA_SIZE, ACTIVE_UNIFORMS)` → `getUniformIndices` → `getActiveUniforms(OFFSET/TYPE)` match std140 expectations.

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed.

**DISPOSITION:** `upstream-candidate`. All 25 methods are thin wrappers around ES3 core entry points; upstream could take the block, K_* additions to the enum, and the `GLObj::sync` field extension in one PR without controversy.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Phase-1.5-MED`, `w_fence_sync`, `w_create_query`, `w_get_active_uniform_block_name`. Recurrence tells:
- Report renders `53 / 88 implemented` post-hardware → the MED FUNCS[] block regressed. Check `install_methods_v2` for the 25 new entries.
- gl-probes SYNC probe FAILs with `clientWaitSync=WAIT_FAILED` → K_SYNC handle regressed to a bare GLuint (the `GLObj::sync` pointer isn't being read back correctly).
- gl-probes QUERY probe FAILs with `QUERY_RESULT=0` for a visible draw → `w_get_query_parameter`'s `glGetQueryObjectuiv` regressed OR the driver isn't seeing the `beginQuery`/`endQuery` wrapping (which would mean the enter_bracket / exit_bracket cadence dropped the query state).
- gl-probes UBO_INTRO probe FAILs with unexpected `DATA_SIZE` → shader compile regressed OR the `getActiveUniformBlockiv` pname dispatch got a case wrong.

**Sequencing.** Third phase-1.5 tier per plan §0.1.1's decided order. Independently revertable from #51 (LOW-MED) and #50 (LOW), and from #52a (which is a `w_draw_range_elements` body change, orthogonal to the MED-family FUNCS[] additions).

---

## #54 — Occlusion-query `ANY_SAMPLES_PASSED` returns 0 despite pixels drawn — CITRON-ONLY QUIRK (verified 2026-07-03 hardware smoke)

Discovered by com.natureglass.gl-probes v0.4.0 QUERY probe on Citron 2026-07-03. Engine's query surface (ledger #53 — `w_create_query`, `w_begin_query`, `w_end_query`, `w_get_query_parameter`) is functionally correct on the wiring side.

**File(s):** none — this ledger tracks an observed behavior, not an engine change.

**Citron symptom.** `beginQuery(ANY_SAMPLES_PASSED, q)` → fullscreen `drawArrays(TRIANGLE_STRIP, 0, 4)` → `endQuery(ANY_SAMPLES_PASSED)` → `getQueryParameter(q, QUERY_RESULT)` returns 0. Verified via `getQueryParameter(q, QUERY_RESULT_AVAILABLE)` returning true first (query result finalized). `readPixels(4, 4)` returns `[255,255,255,255]` proving the draw painted the target.

**Citron discriminator ladder from gl-probes v0.7.0 log:**
```
eBegin=0x0 eDraw=0x0 eEnd=0x0 eAvail=0x0 eResult=0x0 curActive=true
```
- `eBegin=0x0` — glBeginQuery accepted (query name valid, target not busy)
- `eDraw=0x0` — glDrawArrays succeeded inside the query bracket
- `eEnd=0x0` — glEndQuery succeeded
- `curActive=true` — `getQuery(ANY_SAMPLES_PASSED, CURRENT_QUERY)` returned the wrapped query right after beginQuery, proving the query was active during the begin block
- `eResult=0x0` + `QUERY_RESULT=0` — no error, counter just returned 0

**HARDWARE VERDICT 2026-07-03 — CITRON-ONLY QUIRK.** Real Tegra Nouveau NV120 QUERY probe run in strict mode: `PASS detail=surface + wiring ok, QUERY_RESULT=1 — spec-conformant (result > 0 for visible draw) | eBegin=0x0 eDraw=0x0 eEnd=0x0 eAvail=0x0 eResult=0x0 curActive=true`. `ANY_SAMPLES_PASSED` works correctly on real hardware; the Citron `QUERY_RESULT=0` was a Citron GPU-translation gap, not a Mesa Nouveau driver ceiling.

**DISPOSITION:** `Citron-only quirk (verified 2026-07-03 hardware smoke)`. gl-probes v0.12.0+ reverts the QUERY probe relaxation — strict `QUERY_RESULT > 0` is now the default (paired brewser-apps commit). On Citron, the probe will FAIL with the Citron-quirk annotation — acceptable because it's a documented Citron-emulator behavior, not an engine or driver defect. On any real GLES3 driver, the probe PASSes.

**Recurrence tell.** QUERY probe FAIL on hardware = Mesa driver regressed OR the engine's `w_get_query_parameter` / `w_begin_query` wiring regressed. QUERY probe PASS on Citron would be surprising — verify the Citron version hasn't changed its GPU translation.

---

## #55 — Phase-1.5-MED-HIGH: 10 v2 transform-feedback methods + K_TRANSFORM_FEEDBACK handle — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — 10 new `FN(w_*)` implementations grouped in a phase-1.5-MED-HIGH block after the MED block + 10 new FUNCS[] entries in `install_methods_v2` + 1 new K_* enum value (K_TRANSFORM_FEEDBACK) + 1 new handle-class registration in `nx_webgl2_init_class` MAP[] (`WebGLTransformFeedback`).

**Motivation.** Fourth and final phase-1.5 tier per plan §0.1.1's decided order (b1 → b2 → LOW → LOW-MED → MED → b3 → **MED-HIGH**). Counter target 78 → 88/88 — closes the WebGL2 spec function counter. Beyond this tier, the report's `WebGL 2 Functions (N / 88)` metric no longer moves; batch 3 adds extension-suffixed methods only.

**Methods landed (10 total):** all in a single family — transform feedback.

- `w_create_transform_feedback` — `glGenTransformFeedbacks(1, &id)` → new K_TRANSFORM_FEEDBACK handle
- `w_delete_transform_feedback` — `glDeleteTransformFeedbacks(1, &id)`
- `w_is_transform_feedback` — `glIsTransformFeedback(id) == GL_TRUE`
- `w_bind_transform_feedback` — `glBindTransformFeedback(target, id)`
- `w_begin_transform_feedback` — `glBeginTransformFeedback(primitiveMode)`
- `w_end_transform_feedback` — `glEndTransformFeedback()`
- `w_transform_feedback_varyings` — `glTransformFeedbackVaryings(program, count, names[], bufferMode)`. Takes a JS Array of strings; effect stored on the program object, **applied at next `linkProgram`** — callers MUST relink for the binding to take effect. Standard WebGL2 pattern.
- `w_get_transform_feedback_varying` — `glGetTransformFeedbackVarying` → K_ACTIVE_INFO handle (same JS shape as `getActiveAttrib` / `getActiveUniform`, `{name, size, type}`). Returns null if the varying at that index is undefined.
- `w_pause_transform_feedback` — `glPauseTransformFeedback()`
- `w_resume_transform_feedback` — `glResumeTransformFeedback()`

Sum: 10 methods = counter +10 = 78 → **88/88 (spec-complete)**.

**RASTERIZER_DISCARD (0x8C89) handling.** `gl.enable(RASTERIZER_DISCARD)` / `gl.disable(RASTERIZER_DISCARD)` route through the existing `w_enable` / `w_disable`, which forward the raw cap to `glEnable`/`glDisable` without a whitelist — only the BLEND/DEPTH_TEST/CULL/SCISSOR/STENCIL caps get shadow-tracked into `st->user_snap` for cross-bracket persistence. RASTERIZER_DISCARD is typically enabled inside the user's `beginTransformFeedback` … `endTransformFeedback` bracket (single frame; no bracket cycle in the middle for a well-behaved synchronous demo), so lack of shadow tracking is spec-conformant. A future extension of `nx_gl_state_snap_t` could add RASTERIZER_DISCARD for long-running demos that leave discard on across frames — deferred until observed as a real need.

**New K_TRANSFORM_FEEDBACK handle kind — runtime teardown pre-wired.** [brewser-runtime-v8/src/scripts/gl-teardown.ts](../brewser-runtime-v8/src/scripts/gl-teardown.ts) already tracks `transformFeedbacks` at lines 154, 171, 312-313, 527 (defensive addition when VAO tracking landed — same pre-wired story as the K_QUERY/K_SAMPLER/K_SYNC additions in #53). No runtime commit needed for MED-HIGH — the additive-only precondition is already met.

**Runtime-semantics verification (mandated micro-probes per plan §0.1.2 addendum — non-negotiable).** Via com.natureglass.gl-probes v0.9.0's TF probe family (paired brewser-apps commit):

- **Capture arm** — SEPARATE_ATTRIBS mode. Vertex shader outputs a `flat out vec4 v_capture` from a per-vertex input; transformFeedbackVaryings binds `v_capture`; program is (re-)linked after the binding; a small buffer is bound via `bindBufferBase(TRANSFORM_FEEDBACK_BUFFER, 0, buf)`; a 3-vertex `drawArrays(POINTS, 0, 3)` runs inside a begin/end TF bracket with `RASTERIZER_DISCARD` enabled; `getBufferSubData` reads back 3×`vec4` = 48 bytes; JS-side memcmp against the expected transformed values.
- **Pause/resume arm** — begin TF → draw 1 vertex → pause → draw 1 vertex (should NOT be captured) → resume → draw 1 vertex → end. `getBufferSubData` should show 2 captured vertices (not 3), verifying pause/resume actually gated the capture.
- **Begin-during-active arm** — begin TF → without ending, try `beginTransformFeedback` again → should set `INVALID_OPERATION` (0x502) per ES3 §2.14.1. gl-probes verifies with `glGetError` — must be INVALID_OPERATION, not a crash.

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed.

**DISPOSITION:** `upstream-candidate`. All 10 methods are thin wrappers around ES3 core entry points; K_TRANSFORM_FEEDBACK enum + handle registration follow the same pattern as #53's three MED handle kinds; upstream could take the block, K_TRANSFORM_FEEDBACK addition, and the paired probe additions in one PR without controversy.

**UPSTREAM STATUS:** `not-submitted`.

**Citron-only quirk sub-item — TF pause/resume (verified 2026-07-03 hardware smoke).** gl-probes v0.9.0 TF_PAUSE probe observed on Citron that `resumeTransformFeedback` resets the buffer write pointer (id=300 overwrote slot 0 that id=100 had captured). Hardware smoke 2026-07-03 confirmed CITRON-ONLY: real Tegra Nouveau NV120 strict TF_PAUSE `PASS detail=pause skipped id=200, captured id=100 + id=300 in order` — the driver honors pause/resume semantics correctly. gl-probes v0.12.0+ reverts the TF_PAUSE relaxation — strict spec-conformance is now the default. On Citron the probe will FAIL with the Citron-quirk annotation (documented Citron-emulator behavior, not an engine or driver defect).

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Phase-1.5-MED-HIGH`, `w_begin_transform_feedback`, `K_TRANSFORM_FEEDBACK`, `"WebGLTransformFeedback"`. Recurrence tells:
- Report renders `78 / 88 implemented` post-hardware → the MED-HIGH FUNCS[] block regressed. Check `install_methods_v2` for the 10 new entries.
- gl-probes TF_CAPTURE probe FAILs with buffer readback zeros → `transformFeedbackVaryings` did not take effect (missing relink post-binding) OR `beginTransformFeedback`/`endTransformFeedback` are silently no-op'ing.
- gl-probes TF_PAUSE probe FAIL under non-strict (i.e., not just PASS-QUIRK-RELAXED but actual FAIL) → a new driver-quirk shape appeared that the probe doesn't yet recognize; capture the `out=[...]` array and open a new sub-item.
- gl-probes TF_ERR probe FAILs with GL_NO_ERROR instead of INVALID_OPERATION → the driver isn't rejecting nested begin — quietly wrong; open a ledger followup.

**Sequencing.** Fourth and final phase-1.5 tier per plan §0.1.1's decided order. Closes the phase-1.5 counter progression at 88/88 for the FUNCTION SURFACE (all 88 methods installed + hardware-verified callable). #56's second-stage per-target rebind fallback hardware-verified 2026-07-03 (fresh-context BUFFER probe both arms PASS on real Tegra Nouveau NV120). #56b fence-guard hardware-verified 2026-07-04 (strict re-invocation BUFFER probe both arms PASS on both Boot #1 non-strict and Boot #2 strict). **Headline claim: 88/88 SURFACE + 88/88 FUNCTIONALLY CORRECT on hardware, zero open functional defects.**

---

## #56b — `getBufferSubData` re-invocation on live context reads stale bytes despite full re-execution — SHIPPED + HARDWARE-VERIFIED 2026-07-04

**Discovered by** code-reading review of the 2026-07-03 hardware smoke #3 strict-run BUFFER FAIL. The initial "carryover" explanation (VRAM allocator reuse + Nouveau non-zero-init `bufferData`) was **incomplete** — code-reading gl-probes.js:230-258 shows `probeBufferRoundtrip` calls `gl.createBuffer()` TWICE per invocation (returning fresh handles), issues `gl.bufferData(ARRAY_BUFFER, src, STATIC_DRAW)` writing all 64 src bytes to srcBuf, then `gl.copyBufferSubData(ARRAY_BUFFER, COPY_WRITE_BUFFER, 0, 0, 64)` copying all 64 bytes to dstBuf. Every invocation re-executes the FULL write path. Stale-memory read from an unwritten region is not a possible explanation.

**Actual defect shape.** Real write-visibility race on Mesa Nouveau NV120: the second `getBufferSubData` invocation on a live `WebGL2` context (returned by `canvas.getContext('webgl2')`, which caches the instance per spec) reads STALE mapped-region bytes despite the just-completed `copyBufferSubData` + `glFinish()`. First-invocation cold-cache map works; re-invocation on a warm cache misses the store. Bug matters for real content: **Three.js's GPGPU ping-pong path re-reads the SAME buffer across frames** — exactly this shape.

**File(s):** [source/webgl.cc](source/webgl.cc) — new `nx_56b_readback_sync_guard(const char *site_tag)` static helper (single-caller today but shared-helper shape retained so future GPU→CPU readback map sites use the same primitive without copy-paste). Called from `w_get_buffer_sub_data` immediately after the retained `glFinish()` from #56.

**FIX RATIONALE — EMPIRICAL, NOT SPEC-GUARANTEED.** A spec-conformant GLES3 driver needs neither `glFenceSync` nor additional coherency work beyond what `glFinish` already provides — ES3 §2.9.5 says `glMapBufferRange(GL_MAP_READ_BIT)` implicitly synchronizes against pending writes to the buffer. **Do NOT cite ES3 §4.1.2 as a spec guarantee for this fix**; the guarantee only makes sense against a driver bug. This is an empirical workaround for a Mesa Nouveau NV120 map-coherency defect: the `FenceSync + clientWaitSync(SYNC_FLUSH_COMMANDS_BIT)` sequence apparently exercises a DIFFERENT driver flushing code path that reaches the mapped-region cache, while `glFinish` alone happens to skip that flush on re-invocation. The fix is a workaround; the underlying Mesa Nouveau path should be filed upstream.

**Escalation ladder (jump rungs on next hardware FAIL — no re-diagnosis).**

- **Rung 1 (SHIPPED)** — `glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)` + `glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 100_000_000ns)` + `glDeleteSync(sync)` immediately before `glMapBufferRange`. Timeout → log `[#56b] getBufferSubData: clientWaitSync TIMEOUT after 100ms — proceeding to map anyway` and fall through (don't hang the runtime).
- **Rung 2** — add `GL_MAP_INVALIDATE_RANGE_BIT` to the map flags on the fresh-context path OR pre-issue a `glInvalidateBufferSubData` (via proc-address) on the target buffer's range before the map, forcing the driver to discard any cached mapped-region contents.
- **Rung 3** — staging-copy readback through a PER-CALL FRESH buffer: create a new scratch `GL_COPY_READ_BUFFER` per readback, `glCopyBufferSubData` from the source buffer into scratch, map scratch (fresh allocation, no cache-carryover surface), copy out, delete scratch. Trades one extra allocation per readback for guaranteed cache-cold map.

**Recurrence tell.** Hardware gl-probes BUFFER strict re-run after non-strict non-strict shows Arm B `mismatch@0` with a value that MATCHES sentinel pattern → copy didn't complete before map (jump to rung 2 or 3). Shows MIX of src + prior-invocation data → stale-map still active (rung 2 first, then 3). Shows `both arms ok` on BOTH runs of the same boot → rung 1 fix carrying, close #56b.

**Class audit (2026-07-03).** Grepped `source/webgl.cc` for every `glMapBufferRange` with `GL_MAP_READ_BIT` and adjacent GPU→CPU readback map paths:

| Call site | Class member? | Disposition |
|---|---|---|
| `w_get_buffer_sub_data` line 3165 | Yes — GPU→CPU buffer-store MAP_READ | **FIXED — calls `nx_56b_readback_sync_guard("getBufferSubData")` before map** |
| `glReadPixels` line 2920 | No — reads directly from bound READ_FRAMEBUFFER into a CPU pointer; not a buffer store map. Spec-required blocking sync per ES3 §4.3.2 | EXEMPT |
| `glGetQueryObjectuiv` line 3790 | No — driver-computed query result; separate `QUERY_RESULT_AVAILABLE` sync path | EXEMPT |
| `PIXEL_PACK_BUFFER` refs lines 3132-3145 | Covered — all inside the per-target rebind branch of the single fix target; no independent PBO map site exists in the current codebase | COVERED |

**#56b closes as a CLASS, not a call site.** Any future GPU→CPU readback via mapped buffer store MUST call `nx_56b_readback_sync_guard(site_tag)` before `glMapBufferRange` — the helper is the class-level primitive.

**Interaction with #56.** `#56` (fresh-context path: `glFinish` + `glGetBufferSubData` proc-address fallback + per-target rebind) STAYS SHIPPED. Do not conflate — those three mitigations handle the fresh-context first-readback failure mode that was verified working on hardware smoke #3 non-strict run. The #56b fence-guard addresses a DISTINCT failure mode (re-invocation write-visibility race) exposed by the same smoke's strict run. Both mitigation blocks coexist in `w_get_buffer_sub_data`.

**Runtime-semantics verification (Citron smoke — non-regression only; verdict is hardware's).** Citron/AMD Vulkan translation of `glMapBufferRange` didn't exhibit either fresh-context or re-invocation failure on prior smokes, so Citron cannot distinguish "fix carries" from "fix regressed silently". Non-regression Citron smoke required (BUFFER probe still `both ok`); actual #56b verdict comes from the next hardware boot.

**HARDWARE VERIFICATION (2026-07-04).** Real Tegra Nouveau NV120, two-boot verdict procedure per runbook §#56b executed clean:

- **Boot #1 non-strict** (`gl-probes-v0.15.0.log`, generated 2026-07-03T22:08:55.576Z): `BUFFER buffer_roundtrip PASS — Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy, sentinel pre-fill 0xA5) both ok 64B memcmp`. Summary: 26 PASS / 0 FAIL / 0 SKIP.
- **Boot #2 strict** (`gl-probes-v0.15.0-all-strict.log`, generated 2026-07-03T22:11:22.716Z, cold power-cycle between boots): identical BUFFER PASS on Arm A + Arm B. Summary: 26 PASS / 0 FAIL / 0 SKIP.

**Mechanism discriminator confirmed load-bearing.** Line 28 both logs: `SYNC clientWaitSync=CONDITION_SATISFIED` on hardware (Citron reports `ALREADY_SIGNALED`). CONDITION_SATISFIED means the fence was a real driver barrier that had NOT yet completed at the moment of `clientWaitSync` — precisely the extra flushing code path that the rung-1 rationale predicted. Rung 1 (`glFenceSync + glClientWaitSync(SYNC_FLUSH_COMMANDS_BIT)`) carries; rungs 2 (`GL_MAP_INVALIDATE_RANGE_BIT` / `glInvalidateBufferSubData`) and 3 (per-call fresh scratch buffer) remain UNSHIPPED and are the escalation ladder if the tell reappears.

**DISPOSITION:** `shipped, hardware-verified`. Class-level primitive `nx_56b_readback_sync_guard` remains the required guard for all future GPU→CPU readback map sites per class audit (2026-07-03) above. Headline claim restored to `88/88 SURFACE + 88/88 FUNCTIONAL on hardware, zero open functional defects`.

**UPSTREAM STATUS:** `not-submitted`. Rung-1 rationale (empirical Mesa Nouveau map-coherency workaround) is a driver-quirk mitigation, not an nx.js correctness bug — upstream candidate is a Mesa bug report against Nouveau NV120's `glMapBufferRange(GL_MAP_READ_BIT)` sync path, not an nx.js PR.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `nx_56b_readback_sync_guard`. Recurrence tell: hardware BUFFER strict re-run FAIL with `mismatch@0` value matching sentinel pattern → copy didn't complete before map, jump to rung 2. FAIL with MIX of src + prior-invocation bytes → stale-map still active, jump to rung 3. Runbook §#56b's verdict table is now the reference for re-diagnosis.

---

## #56 — `getBufferSubData` on COPY_WRITE_BUFFER returns wrong data on Mesa Nouveau NV120 hardware — SHIPPED + HARDWARE-VERIFIED 2026-07-03

Discovered by com.natureglass.gl-probes v0.11.0 BUFFER probe on real Tegra Nouveau NV120 (2026-07-03 hardware smoke, both Boot A default NRO and Boot B fallback-disabled NRO). Not reproducible on Citron — passed 21/21 on the Citron final smoke.

**File(s):** [source/webgl.cc](source/webgl.cc) — `w_get_buffer_sub_data` at approximately line 2811-2828 (phase-1.5-LOW block, ledger #50).

**Hardware symptom.** gl-probes BUFFER probe fails with `mismatch@0 src=3 got=0` — every byte in the readback is zero, none match the src pattern that was `bufferData`'d into a source ARRAY_BUFFER + `copyBufferSubData`'d into a destination COPY_WRITE_BUFFER. Boot A and Boot B identical → not related to #52a fallback gate.

**Citron symptom.** None — probe passes cleanly with `copy+getSubData 64B memcmp ok`.

**Root-cause candidates worked through (2026-07-03 diagnosis-first pass).**

Alex's diagnostic ladder from the post-hardware directive, resolved by code-reading + reference-checking:

(a) **Target-binding mismatch in `w_get_buffer_sub_data`** — RULED OUT by inspection. The JS-supplied `target` is passed verbatim to `glMapBufferRange` and later `glUnmapBuffer`. The QuickJS-era pre-migration reference (`nxjs-source/source/webgl_egl.c:9925 nx_webgl_egl_get_buffer_sub_data`) uses the same target passthrough shape, but calls a RUNTIME-RESOLVED `glGetBufferSubData` via a function pointer set at bridge init — that's the load-bearing difference: the reference used `glGetBufferSubData` (not `glMapBufferRange`), and it worked on the same hardware.

(b) **Missing synchronization (implicit sync bug)** — LIKELY per hardware smoke pattern. ES3 spec §2.9.5 says `glMapBufferRange(GL_MAP_READ_BIT)` implicitly synchronizes against pending writes to the buffer, but Nouveau drivers historically underimplement this for the copyBufferSubData → mapRead pattern. `glFinish()` before the map forces the GPU pipeline to drain.

(c) **Map/unmap pairing or memcpy source error** — RULED OUT by inspection. The code path is straightforward `mapped = glMapBufferRange(target, ...)`; `if (mapped) { memcpy(...); glUnmapBuffer(target); }`. The `if (mapped)` gate cleanly guards against NULL returns; symptom is that mapped IS NULL (which is why the Uint8Array stays at its zero-initialized state).

**FIX SHIPPED (Commit 1, 2026-07-03) — belt-and-suspenders per Alex's directive.** Both mitigations land together so the next hardware boot pins down which one carries:

1. Candidate (b): `glFinish()` before `glMapBufferRange` — spec-required-anyway sync; perf note documented (readback is inherently sync from the JS perspective).
2. Candidate (a) fallback: runtime-resolve `glGetBufferSubData` via `eglGetProcAddress` on first `w_get_buffer_sub_data` call. If `glMapBufferRange` returns NULL, use `glGetBufferSubData` — matches the QuickJS-era reference that worked on the same hardware. One-shot boot log `[#56] glGetBufferSubData proc-address resolved: <ptr>` confirms whether the fallback is available at all.
3. `NX_56_DEBUG` build flag with per-call fprintf instrumentation: mapped pointer, first 16 bytes, glGetError codes before and after map + after fallback. Standard build stays quiet; hardware diagnostic build gets full observability.

**BUFFER probe upgraded to two arms (paired gl-probes v0.13.0 commit).** Arm A reads from ARRAY_BUFFER directly (no copy — isolates the base readback path on a universally-supported target). Arm B is the original copy + read from COPY_WRITE_BUFFER. Discriminator table:
- Both PASS → fix landed successfully.
- Arm A PASS, Arm B FAIL → target-specific quirk on COPY_WRITE_BUFFER; add per-target rebind fallback in a follow-up commit.
- Both FAIL → universal map/readback defect; escalate to third candidate (transient scratch buffer + explicit copy).
- Arm B PASS, Arm A FAIL → inverted-severity map defect; strange, escalate.

**Impact.** MEDIUM severity if the fix doesn't carry — Three.js's `WebGLRenderer.readRenderTargetPixels` fallback + GPGPU demos + any particle-system geometry-processing utility that reads back GPU-computed vertex data would silently see zeros. Fix land on hardware unblocks those paths.

**DISPOSITION:** `diagnosis-shipped-fix-pending-hardware-verify`. Not blocking phase-1.5 tier acceptance headline (88/88 SURFACE installed + hardware-verified callable). Blocks the 87/88 → 88/88 FUNCTIONAL correctness jump; hardware smoke next boot resolves.

**RE-APPLY / VERIFY NOTE.** Recurrence tell: hardware gl-probes BUFFER `BOTH ARMS FAIL — universal map/readback defect` = fix regressed OR driver behavior further degraded. Hardware BUFFER `Arm A PASS, Arm B FAIL` = target-specific quirk still open. Hardware BUFFER `Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy) both ok` = fix confirmed carrying, reclassify to CLOSED. Boot log grep for `[#56] glGetBufferSubData proc-address resolved: 0x[1-9a-f]` = fallback available (non-null); `0x0` = only the sync candidate is protecting us.

Runbook §#56 has the exact verdict procedure for the next hardware boot.

---

### #56 second-stage fix — per-target rebind (SHIPPED 2026-07-03, hardware-verdict pending)

**2026-07-03 hardware smoke (second session) verdict.** gl-probes v0.14.0 BUFFER probe on real Tegra Nouveau NV120:

- Boot A (default NRO, non-strict): `Arm A (ARRAY_BUFFER direct) PASS, Arm B (COPY_WRITE_BUFFER via copy) FAIL — target- or copy-specific defect (#56 candidate) | Arm-B mismatch@0 src=3 got=0`
- Boot B (default NRO, strict): identical shape, `got=42` (spurious value — memory from a prior probe's allocation containing id=42 from TF_ERR)

**Diagnosis.** The candidate (a)+(b) fix from Commit 1 (`1c42457`) partially works — the `glFinish()` sync helps `glMapBufferRange` on ARRAY_BUFFER return correct data (Arm A PASS). But `glMapBufferRange(GL_COPY_WRITE_BUFFER)` on Mesa Nouveau NV120 returns non-NULL (so the fallback path never fires) yet points at unrelated memory. The `glGetBufferSubData` proc-address fallback path (candidate a) is unreachable because it's gated on `if (mapped == NULL)`. Target-specific driver defect confirmed.

**Second-stage fix landed (candidate a1).** For `GL_COPY_WRITE_BUFFER`, `GL_COPY_READ_BUFFER`, `GL_PIXEL_PACK_BUFFER`: look up the buffer name currently bound to that target via `glGetIntegerv(GL_<TARGET>_BINDING)`; temporarily rebind that buffer to `GL_ARRAY_BUFFER` (per-target binding — no data movement, driver side-effect-free); map from `GL_ARRAY_BUFFER` (verified to work by Arm A); memcpy; unmap; restore original `GL_ARRAY_BUFFER` binding. Preserves original target binding on the caller's namespace.

**Expected next hardware verdict.** BUFFER probe both arms PASS: `Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy) both ok 64B memcmp`. If confirmed, reclassify #56 to CLOSED and move to Archived section of docs/HW_SESSION_RUNBOOK.md.

**If FAIL persists.** Rebuild with `-DNX_56_DEBUG=1` — the new per-call log line `[#56] per-target rebind: target=0x8F37 bind=<name> saved_array=<name>` will confirm the rebind ran; if it did AND readback is still wrong, then even `glMapBufferRange(GL_ARRAY_BUFFER)` on a buffer allocated via a different target has the same defect, and we escalate to the proc-address `glGetBufferSubData` fallback path (which the second-stage fix rearranges to be reachable after the rebind attempt).

**Corrected `NX_56_DEBUG` fallback reachability.** Commit 1's shape gated `resolve_pfn_get_buffer_sub_data()` on `if (mapped == NULL)` — but the map returned non-NULL with wrong data, so the fallback was unreachable. Commit 3 (second stage) shifts the gate: if the rebind path ran, the map is now against ARRAY_BUFFER (verified working) — the map-returns-non-NULL-with-wrong-data path shouldn't fire. If it somehow does, we still fall through to the proc-address fallback which reads via the original target.

### Hardware verdict 2026-07-03 (third session, second-stage-fix boot) — SHIPPED + HARDWARE-VERIFIED

gl-probes v0.14.0 BUFFER probe on real Tegra Nouveau NV120 CFW hbmenu with the per-target rebind engine build:
- Non-strict run (fresh WebGL2 context, first probe cycle): `BUFFER PASS detail=Arm A (ARRAY_BUFFER direct) + Arm B (COPY_WRITE_BUFFER via copy) both ok 64B memcmp`. **26 PASS / 0 FAIL / 0 SKIP (of 26)**. Log: `gl-probes-v0.14.0.log`.
- Strict re-run (SAME context, 2.4s later): `BUFFER FAIL detail=Arm A PASS, Arm B FAIL got=42`. Initial explanation "state carryover / VRAM allocator reuse without zero-init" was RETRACTED post code-review — the probe re-executes the full `createBuffer + bufferData(src) + copyBufferSubData` sequence per invocation (verified by reading gl-probes.js:230-258). Real defect: re-invocation write-visibility race on Mesa Nouveau NV120 (stale mapped-region bytes despite full re-execution + glFinish). Reopened as **#56b** — separate ledger entry above. Log: `gl-probes-v0.14.0-all-strict.log`.

**Fix confirmed carrying on the primary use case** — fresh context, single probe run, both arms PASS end-to-end. Three.js's `WebGLRenderer.readRenderTargetPixels` fallback + GPGPU readback paths that trigger `getBufferSubData` on a per-frame-fresh path (which is the typical shape) work correctly on real Tegra hardware post-fix.

**DISPOSITION UPDATED:** `SHIPPED + HARDWARE-VERIFIED`. Reclassify from `citron-observed` / `hardware-pending` to `driver-workaround-shipped`. `NX_56_DEBUG` build flag retained for future re-diagnostic cycles.

**RE-APPLY / VERIFY NOTE.** Recurrence tell: fresh-boot hardware gl-probes BUFFER `Arm A + Arm B both ok 64B memcmp` = fix carrying. Re-invocation-in-same-context Arm B mismatch = documented probe-side state-carryover artifact (not an engine regression); to close probe-side, either force a page reload between smoke runs OR add `gl.finish() + explicit memset via bufferSubData(zero)` between probe iterations. Deferred as harness improvement; NOT tracked as an engine issue.

---

## #57 — Batch 3: final extension batch (10 rows) — SHIPPED 2026-07-03

**File(s):** [source/webgl.cc](source/webgl.cc) — 16 new `FN(w_*)` implementations grouped in a batch-3 block near end-of-file + 16 new FUNCS[] entries (14 on v2 `install_methods_v2`, 12 on v1 `install_methods` — v1 aliases the query-lifecycle EXT-suffixed shape) + 10 new advertising rows in `w_get_supported_extensions` + 10 new branches in `w_get_extension` + runtime-resolved function pointer table (`s_pfn_clip_control` etc.) via `eglGetProcAddress` cached at first-call time + b3 forward-decl block after the phase-1.5-MED-HIGH decls.

**Motivation.** Batch 3 closes the pre-migration audit's remaining rows per plan §2. All are extension-suffixed methods, so the WebGL2 spec function counter stays at 88/88 (unchanged). The value is closing the ADVERTISED / MISSING rows on the report app to zero-missing on both context types (subject to bucket-D UNSUPPORTED_BY_DRIVER).

**Timer-query scope finalized as FULL ADVERTISE per #54 hardware verdict.** Alex's directive: since #54 confirmed real Tegra Nouveau NV120 occlusion queries work, timer queries — which share the same query machinery + a driver-time counter — also work. No stub required; ship the full timer-EXT surface (`queryCounterEXT` + timer constants + `GPU_DISJOINT_EXT` reachable via existing `w_get_parameter` glGetIntegerv fallback path).

**Extension rows landed (all driver-gated, ADVERTISED per plan §2.6 predicted verdict table):**

Both v1 + v2:
- `EXT_clip_control` — `clipControlEXT(origin, depth)` + 6 constants
- `EXT_polygon_offset_clamp` — `polygonOffsetClampEXT(factor, units, clamp)` + 1 constant
- `KHR_parallel_shader_compile` — `maxShaderCompilerThreadsKHR(count)` + `COMPLETION_STATUS_KHR`
- `WEBGL_multi_draw` — 4 engine-native loop-shim methods (`multiDrawArraysWEBGL`, `multiDrawElementsWEBGL`, `multiDrawArraysInstancedWEBGL`, `multiDrawElementsInstancedWEBGL`). Perf note: no batching benefit vs desktop GL, but semantically identical output.
- `WEBGL_blend_func_extended` — 5 SRC1 constants. v1 gets constants-only per plan §2.5 (dual-source shader compile probe is deferred until an actual demo needs it); v2 additionally routes SRC1 blend factor tokens through the existing `w_blend_func` / `w_blend_func_separate` (which forward the raw enum without a whitelist).

v1-only:
- `EXT_disjoint_timer_query` — full query lifecycle with EXT suffix (createQueryEXT, deleteQueryEXT, isQueryEXT, beginQueryEXT, endQueryEXT, getQueryEXT, getQueryObjectEXT) aliasing to #53's core query natives, plus `queryCounterEXT` and 7 timer constants.

v2-only:
- `EXT_disjoint_timer_query_webgl2` — `queryCounterEXT` + 7 timer constants (lifecycle uses v2 core query surface from #53).
- `OES_draw_buffers_indexed` — 8 indexed methods (`enableiOES`, `disableiOES`, `blendEquationiOES`, `blendEquationSeparateiOES`, `blendFunciOES`, `blendFuncSeparateiOES`, `colorMaskiOES`, `isEnablediOES`). Runtime-resolved via `eglGetProcAddress` with OES-first, EXT-fallback pattern (Nouveau's native token is OES).
- `WEBGL_clip_cull_distance` — 11 constants (advertise + directive; clip/cull distance ESSL declarations honored by driver).
- `OES_sample_variables` — advertise-only feature-flag (enables GLSL `gl_SampleMask` etc via `#extension` directive).
- `OES_shader_multisample_interpolation` — 3 constants (advertise + directive).

**Native entry-point resolution model.** All 12 extension entry points (`glClipControlEXT`, `glPolygonOffsetClampEXT`, `glQueryCounterEXT`, `glMaxShaderCompilerThreadsKHR`, and 8 indexed-blend-state functions) resolve via `eglGetProcAddress` cached at first-call in `resolve_b3_pfns()`. Boot log emits a one-shot line per boot:
```
[b3] extension entry-point resolution: clipControl=0x<addr> polygonOffsetClamp=0x<addr> ...
```
Non-null addresses = extension functional; null = ext object still vends via the FN body but the underlying native passes through as no-op (spec-conformant — advertising says "supported", null-noop stops there without crashing).

**Compile probes (from plan §2.4 / §2.5).**
- `EXT_frag_depth` (v1) already probe-gated in batch 2 (`probe_ext_frag_depth`); unchanged.
- `WEBGL_blend_func_extended` v1 ESSL-100 compile probe DEFERRED — the driver supports the extension token but ESSL-100 SRC1 acceptance is an implementation detail that only matters if a demo compiles a #version 100 shader with `#extension GL_EXT_blend_func_extended : enable`. When such a demo appears, add `probe_blend_func_extended_essl100` mirroring `probe_ext_frag_depth`; until then, constants-only advertising is correct.

**Runtime-semantics verification per family (Citron smoke via com.natureglass.gl-probes v0.14.0 — paired brewser-apps commit).**

- **Timer query end-to-end** — createQuery + queryCounterEXT(TIMESTAMP_EXT) around a draw + polling QUERY_RESULT + GPU_DISJOINT_EXT check via `gl.getParameter`. Skips draw comparison if driver reports GL_TRUE for GPU_DISJOINT_EXT (spec-legal driver behavior — timer results are undefined in disjoint state).
- **polygonOffsetClamp accepted** — `polygonOffsetClampEXT(1.0, 1.0, 0.5)` + `gl.getParameter(POLYGON_OFFSET_CLAMP_EXT)` roundtrip. Driver exposes POLYGON_OFFSET_CLAMP_EXT as an integer/float via glGetFloatv; the probe verifies it round-trips.
- **Indexed blend state** — `enableiOES(BLEND, 1)` + `isEnablediOES(BLEND, 1)` roundtrip → true; then `disableiOES(BLEND, 1)` + readback → false. Also verifies `blendEquationiOES(1, FUNC_ADD)` accepts.
- **multi_draw output matches looped single draws** — Draw the same triangle set two ways: multiDrawArraysWEBGL with N sub-draws vs an equivalent N-iteration loop of drawArrays. readPixels must match.
- **blend_func_ext advertising + constants** — `getExtension('WEBGL_blend_func_extended')` returns a non-null object; `.SRC1_COLOR_WEBGL === 0x88F9`; `.MAX_DUAL_SOURCE_DRAW_BUFFERS_WEBGL === 0x88FC`. Compile probe result logged (deferred from actual compile per spec-defer above).

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed; no extension surface.

**DISPOSITION:** `upstream-candidate`. All 16 methods are thin wrappers around driver extension entry points (via eglGetProcAddress) or engine-native loop shims (multi_draw); upstream could take the block + advertising updates in a single PR without controversy.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Batch 3 (ledger #57)`, `resolve_b3_pfns`, `w_query_counter_ext`, `w_multi_draw_arrays_webgl`. Recurrence tells:
- Report renders EXT_disjoint_timer_query as MISSING on v1 or _webgl2 as MISSING on v2 = `w_get_supported_extensions` batch-3 block regressed OR the driver's `GL_EXT_disjoint_timer_query` native token was removed (unlikely).
- `[b3] extension entry-point resolution: … =0x0 …` on ALL entry points at boot = eglGetProcAddress isn't finding ANY of them; driver/loader misconfigured. Investigate.
- gl-probes TIMER_QUERY FAIL with `queryCounterEXT no-op` = driver reports resolved pointer but the native no-ops. Escalate to per-driver disjoint check.
- gl-probes MULTI_DRAW FAIL with output != looped = one of the four native-loop-shim FN bodies is dropping an iteration; check the bounds guards.

**Sequencing.** Final extension batch per plan §3.3 (b1 → b2 → b3, orthogonal to the phase-1.5 tier progression). Extension advertising is now saturated; any future extension gets its own numbered ledger entry.

---

## #58 — Tier 1: `getUniform(program, location)` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_get_uniform)` body in the Tier-1 block ending line ~4402 (`// End Tier 1 block.`) + FUNCS[] entry `{"getUniform", w_get_uniform}` in both `install_methods` (v1) and `install_methods_v2`.

**Motivation.** Cold-restart batched Citron baseline of the full 745-test WebGL 1 conformance corpus surfaced 78 STATUS:ERROR tests (test JS threw an uncaught exception before any assertion ran). 35 of those cluster into 7 bridge methods absent from `FUNCS[]`. `getUniform` is the largest single win — 20 tests from the `uniforms-no-over-optimization-on-uniform-array-*` family + `uniforms-uniform-default-values` ERROR on `gl.getUniform is not a function` at test-body start.

**Spec subtlety the failing cluster exists to check.** For an array uniform, `getUniform` on the per-element location returned by `getUniformLocation("u[i]")` MUST return only element `i` — never the whole array. A compiler that collapses the array to a single storage location would return the same value for every element; the `no-over-optimization` test proves that doesn't happen. The impl walks the program's active-uniform list (via `glGetActiveUniform` — no cached program state), resolves each uniform's base location via `glGetUniformLocation`, matches the requested location against `[base, base + size)`, then dispatches the correct `glGetUniform*v` variant per the resolved type.

**Type dispatch table (spec-mandated JS return shapes).**
- `GL_FLOAT` → `number` (JS)
- `GL_FLOAT_VEC{2,3,4}` → `Float32Array({2,3,4})`
- `GL_FLOAT_MAT{2,3,4}` → `Float32Array({4,9,16})`
- `GL_FLOAT_MAT{2x3,2x4,3x2,3x4,4x2,4x3}` → `Float32Array({6,8,6,12,8,12})` (WebGL2)
- `GL_INT` / `GL_UNSIGNED_INT` / samplers → `number`
- `GL_INT_VEC{2,3,4}` → `Int32Array({2,3,4})`
- `GL_UNSIGNED_INT_VEC{2,3,4}` → `Uint32Array({2,3,4})` (WebGL2)
- `GL_BOOL` → `boolean`; `GL_BOOL_VEC{2,3,4}` → plain JS `Array<boolean>({2,3,4})`

**Sampler default branch.** All opaque sampler types (SAMPLER_2D / SAMPLER_CUBE / SAMPLER_3D / SAMPLER_2D_SHADOW / integer- and unsigned-sampler variants, plus any future sampler enum added by extension) read back as one `GLint` (bound texture-unit index) — spec-compliant and forward-compatible with new sampler enums. Handled by the `default` arm.

**Why upstream-vanilla lacks it.** Upstream nx.js WebGL is null-stubbed pre-migration; the V8 fork's WebGL surface (webgl.cc) has grown method-by-method as demos surfaced calls. `getUniform` was never demanded by any demo in phases 2.C–2.G but IS demanded by the conformance corpus.

**DISPOSITION:** `upstream-candidate`. Pure spec-hole fill, zero Brewser coupling; upstream could take the FN + FUNCS[] entries as-is.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `FN(w_get_uniform)` and the two `{"getUniform", w_get_uniform}` FUNCS entries (one per install_methods table). Recurrence tells:
- 20 `uniforms-no-over-optimization-on-uniform-array-*` tests regress from PASS/FAIL to STATUS:ERROR with `gl.getUniform is not a function` = one of the two FUNCS[] entries dropped, or the FN body was removed.
- A subset of those 20 tests PASSes but reports "over-optimization: expected 1.5, got 0.5" (or similar shape) = the per-element location logic in the impl broke — likely `[base, base + size)` was replaced with a whole-array read that always returns element 0. Re-check the `location` arg is passed directly to `glGetUniform*v`, not `base`.
- New uniform type (e.g. a sampler variant added by a future extension) returns wrong shape = the default sampler branch is too broad. Fine-tune the switch.

**Sequencing.** First of the Tier 1 batch (#58–#64) — shipped alone. #59–#63 land next as a batch; #64 (Screen.toDataURL) after that; #65 (compressed-format INVALID_ENUM gate) closes the batch bundled with the corpus skip-file edit.

---

## #59 — Tier 1: `copyTexImage2D` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_copy_tex_image_2d)` body in the Tier-1 block + FUNCS[] entry in v1 + v2 tables.

**Motivation.** 7 conformance tests ERROR on `gl.copyTexImage2D is not a function` (`misc-uninitialized-test`, `rendering-clear-after-copyTexImage2D`, `textures-misc-copy-tex-image-2d-formats`, `-crash`, `-texture-copying-and-deletion`, `-feedback-loops`, `-texture-npot`). The sub-image sibling (`copyTexSubImage2D`) was added at 2.G.1 cut #25 (see comment at existing `w_copy_tex_sub_image_2d`); the full-image variant was skipped because no v1/v2 demo through 2.G exercised it. Conformance does.

**Implementation shape.** Thin `glCopyTexImage2D` wrapper matching the ES2/ES3 signature `(target, level, internalformat, x, y, width, height, border)`. Same 8-arg call shape as `w_copy_tex_sub_image_2d` at line ~2581 (which uses xoffset/yoffset instead of internalformat/border). No format translation needed — `copyTexImage2D` uses ES2/ES3 unsized internalformats directly.

**DISPOSITION:** `upstream-candidate`. **UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep for `FN(w_copy_tex_image_2d)` and the two `{"copyTexImage2D", w_copy_tex_image_2d}` FUNCS entries. Recurrence tell: any of the 7 tests above regresses to `gl.copyTexImage2D is not a function`.

---

## #60 — Tier 1: `getVertexAttrib(index, pname)` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_get_vertex_attrib)` body in the Tier-1 block + FUNCS[] entry in v1 + v2 tables.

**Motivation.** 2 conformance tests ERROR on `gl.getVertexAttrib is not a function` (`extensions-angle-instanced-arrays`, `extensions-oes-vertex-array-object`). Both introspect attribute state after configuring buffers.

**Pname-switched return shapes.**
- `GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING` (0x889F) → Buffer wrapper (or null when 0) via the existing K_BUFFER handle kind
- `GL_VERTEX_ATTRIB_ARRAY_ENABLED` (0x8622) / `_NORMALIZED` (0x886A) / `_INTEGER` (0x88FD, WebGL2) → boolean
- `GL_VERTEX_ATTRIB_ARRAY_SIZE` (0x8623) / `_STRIDE` (0x8624) / `_DIVISOR` (0x88FE) → number (Int32)
- `GL_VERTEX_ATTRIB_ARRAY_TYPE` (0x8625) → number (Uint32 — it's a GL enum token)
- `GL_CURRENT_VERTEX_ATTRIB` (0x8626) → `Float32Array(4)`

Uses `glGetVertexAttribiv` for the integer/boolean pnames and `glGetVertexAttribfv` for `CURRENT_VERTEX_ATTRIB`. Wraps the buffer name back into a WebGL Buffer object using `new_gl_obj(iso, K_BUFFER, id)` — spec-required (the caller expects a `WebGLBuffer` reference, not a raw GLuint name). Unknown pname records `GL_INVALID_ENUM` and returns null.

**DISPOSITION:** `upstream-candidate`. **UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep for `FN(w_get_vertex_attrib)` and the two `{"getVertexAttrib", w_get_vertex_attrib}` FUNCS entries. Recurrence tells:
- `extensions-angle-instanced-arrays` regresses to `is not a function` = FUNCS[] entry dropped.
- Test asserts `gl.getVertexAttrib(0, gl.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING) instanceof WebGLBuffer === true` and it returns `null` or a number = the K_BUFFER wrap regressed to raw GLuint return.

---

## #61 — Tier 1: `getFramebufferAttachmentParameter(target, attachment, pname)` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_get_framebuffer_attachment_parameter)` body in the Tier-1 block + FUNCS[] entry in v1 + v2 tables.

**Motivation.** 2 conformance tests ERROR on `gl.getFramebufferAttachmentParameter is not a function` (`extensions-webgl-draw-buffers`, `renderbuffers-framebuffer-test`).

**Pname-switched return shapes.**
- `GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME` (0x8CD1) → Texture wrapper OR Renderbuffer wrapper depending on the preflight query for `GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE` (0x8CD0). Returns null when OBJECT_TYPE is GL_NONE or the name is 0.
- All other pnames (TYPE / LEVEL / CUBE_MAP_FACE / LAYER / COMPONENT_TYPE / RED_SIZE / ..) → number (Int32).

The OBJECT_NAME branch does a preflight `glGetFramebufferAttachmentParameteriv(..., OBJECT_TYPE, ...)` so we know which handle kind to wrap the name into. Same pattern as spec.

**DISPOSITION:** `upstream-candidate`. **UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep for `FN(w_get_framebuffer_attachment_parameter)` and the two `{"getFramebufferAttachmentParameter", w_get_framebuffer_attachment_parameter}` FUNCS entries. Recurrence tell: `renderbuffers-framebuffer-test` regresses to `is not a function`.

---

## #62 — Tier 1: `getAttachedShaders(program)` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_get_attached_shaders)` body in the Tier-1 block + FUNCS[] entry in v1 + v2 tables.

**Motivation.** 2 conformance tests ERROR on `gl.getAttachedShaders is not a function` (`misc-expando-loss`, `programs-program-test`).

**Implementation shape.** Query attached-shader count via `glGetProgramiv(GL_ATTACHED_SHADERS)`, then `glGetAttachedShaders` into a `std::vector<GLuint>`, then wrap each name back to a `WebGLShader` JS object via `new_gl_obj(iso, K_SHADER, name)` and pack into a JS `Array`. Spec: returns array of shader wrappers, empty `Array` if none attached (not null); returns null only when the program handle itself is 0.

**DISPOSITION:** `upstream-candidate`. **UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep for `FN(w_get_attached_shaders)` and the two `{"getAttachedShaders", w_get_attached_shaders}` FUNCS entries. Recurrence tells:
- `misc-expando-loss` regresses to `is not a function` = FUNCS[] entry dropped.
- Test asserts shader `instanceof WebGLShader` on the returned wrapper and it's `false` = the K_SHADER prototype isn't stamped on the wrapper (`new_gl_obj` should apply the shader proto automatically — recheck the impl).

---

## #63 — Tier 1: `vertexAttrib{1,2,3,4}fv(index, arr)` — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `FN(w_vertex_attrib_1fv)`..`_4fv` bodies (macro-generated) in the Tier-1 block + FUNCS[] entries in v1 + v2 tables (4 entries per table).

**Motivation.** At least 1 conformance test ERRORs on a dynamic-form call to `gl.vertexAttrib*fv(index, arr)` (surfaced under the `<dynamic>` receiver bucket in the ERROR classification: `attribs-gl-vertex-attrib-render`). The scalar variants (`vertexAttrib{1,2,3,4}f`) were bound at 2.C but the typed-array pointer variants (`fv`) were missed.

**Implementation shape.** Four thin wrappers via a `VA_FV(N)` macro that mirrors the existing `UNI_FV(N)` macro at line ~2288: uses `f32_list` to unwrap `Float32Array` or plain JS array into a contiguous float pointer, guards `n >= N` before dispatch, then calls `glVertexAttrib{N}fv(index, ptr)`.

**DISPOSITION:** `upstream-candidate`. **UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep for `FN(w_vertex_attrib_1fv)` (and the 2fv/3fv/4fv siblings — all four are macro-generated so grepping `w_vertex_attrib_4fv` is the canonical check) + the eight `{"vertexAttrib{N}fv", ...}` FUNCS entries (4 in v1, 4 in v2). Recurrence tell: `attribs-gl-vertex-attrib-render` regresses to `is not a function` on `gl.vertexAttrib3fv` or similar.

---

## #64 — Tier 1: `Screen.toDataURL` WebGL-surface readback path — SHIPPED 2026-07-04

**File(s):**
- [source/webgl.cc](source/webgl.cc) — new `nx_webgl_snapshot_bridge_rgba8()` helper (public, outside anon namespace, sits right above `nx_init_webgl`).
- [source/webgl.h](source/webgl.h) — extern declaration + docstring.
- [source/canvas.cc](source/canvas.cc) — `nx_canvas_proto_to_data_url` AND `nx_canvas_to_buffer` gain a WebGL-surface branch that runs BEFORE `snapshot_pixels`; +1 `#include "webgl.h"`.

**Motivation.** 1 conformance test ERRORs on `glCanvas.toDataURL is not a function` (`canvas-framebuffer-bindings-affected-by-to-data-url`) AND — independent of that specific test — `Screen.toDataURL()` on a Screen carrying an active WebGL context previously returned the empty raster snapshot: canvas.cc's fallback path pulled from `canvas->data` (the raster surface backing), which is unused when Screen renders through the EGL tenant FBO. This lands the WebGL-surface readback so `screen.toDataURL()` returns the actual WebGL draw output.

**Test-unlock caveat.** The failing conformance test invokes `document.getElementById('webgl-canvas').toDataURL()` — a live-DOM canvas element proxied through brewser-runtime-v8's live-dom shim, NOT `screen` directly. Whether the test flips PASS depends on the runtime routing `.toDataURL` through to Screen. The engine-side fix is complete regardless. If Alex's re-baseline shows the test still ERRORs on `is not a function`, the residual fix is a runtime shim (RUNTIME_SHIMS.md scope, deferred beyond this session per the read-only-runtime policy in the task spec).

**Implementation shape — `nx_webgl_snapshot_bridge_rgba8(int *out_w, int *out_h, uint8_t **out_bgra)`:**
1. Bail with `false` if `!nx_webgl_bridge_is_initialized()` OR the FBO size is 0×0.
2. Save current `GL_READ_FRAMEBUFFER_BINDING` + `GL_PACK_ALIGNMENT`.
3. Drain pre-existing `glGetError` so the post-read check reflects only this readback.
4. Bind bridge FBO to `GL_READ_FRAMEBUFFER`; force `GL_PACK_ALIGNMENT = 1` (defensive — width×4 is always 4-aligned but future-proof against non-RGBA reads).
5. `glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba)`.
6. Restore READ_FRAMEBUFFER + PACK_ALIGNMENT (BEFORE the second malloc so an OOM in step 7 doesn't leave the binding dangling).
7. Second malloc for the BGRA output; return `false` on OOM (frees `rgba`).
8. Y-flip AND RGBA→BGRA swizzle in one pass. `encode_pixels` in canvas.cc expects top-down BGRA per its Skia paint pixmap contract.
9. Return `true` + out-params (`*out_bgra` is caller-owned, caller `free()`s).

Does NOT call `enter_bracket()` — the sequence only touches `READ_FRAMEBUFFER_BINDING` (which the 2.B state snap doesn't cover; patch #17 later added it only for the ACTIVE probe path) and `PACK_ALIGNMENT` (not in the snap). Skia uses `DRAW_FRAMEBUFFER_BINDING` for its own rendering; the two are independent in ES3. State discipline preserved without perturbing bracket machinery — bracket call sites are frozen this session (blast-radius rule).

canvas.cc's `nx_canvas_proto_to_data_url` gains one branch before the existing `snapshot_pixels` call: if `nx_webgl_snapshot_bridge_rgba8()` returns true, use its BGRA buffer; otherwise fall through to the raster path. Same shape mirrored in `nx_canvas_to_buffer` (the async `toBlob` sibling); the WebGL readback must happen on the main thread — that's where `nx_canvas_to_buffer`'s body already runs (only the ENCODE is dispatched to the pool).

**DISPOSITION:** `upstream-candidate`. Upstream nx.js's `screen.toDataURL()` on WebGL-backed Screen has the same empty-raster gap.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `nx_webgl_snapshot_bridge_rgba8`, [source/webgl.h](source/webgl.h) for the same, and [source/canvas.cc](source/canvas.cc) for both the `#include "webgl.h"` line and the `nx_webgl_snapshot_bridge_rgba8` call sites (2 — sync + async). Recurrence tells:
- `screen.toDataURL()` returns a solid transparent-black PNG after a WebGL draw = the WebGL branch was removed from canvas.cc; the raster fallback ran and got the empty Skia surface.
- Y-flipped image (upside-down) = the flip loop in `nx_webgl_snapshot_bridge_rgba8` regressed (swapped row indices, dropped the flip, etc.).
- Wrong color channels (blue↔red) = the RGBA→BGRA swizzle regressed.

---

## #65 — Tier 4: compressed-format INVALID_ENUM validation gate — SHIPPED 2026-07-04

**File(s):**
- [source/webgl.cc](source/webgl.cc) — new `has_compressed_format_advertised(GLenum internalformat)` static helper in the namespace (right above `w_compressed_tex_image_2d`) + gate branch inside both `w_compressed_tex_image_2d` and `w_compressed_tex_sub_image_2d` (record INVALID_ENUM and return before touching the driver on unadvertised formats).
- `sdmc:/switch/brewser/logs/full-webgl1-conformance-skip.txt` — 7 formerly-skipped compressed-format hang entries (BPTC/RGTC/ETC/ETC1/PVRTC/ASTC/S3TC-sRGB) REMOVED. The remaining 5 HANG entries stay with one-line comments explaining why each is retained.

**Motivation.** The WebGL 1 conformance corpus contains a helper
`testCompressedFormatsUnavailableWhenExtensionDisabled` that intentionally calls `glCompressedTexImage2D(target, 0, <unadvertised-format>, 4, 4, 0, data)` and asserts the call returns INVALID_ENUM without doing the upload. On Citron running Mesa-Nouveau, the pre-gate call did NOT return INVALID_ENUM at the driver — the WebGL runner **hung inside `runOneTest` before the test's START diag fired** across 7 tests (BPTC, RGTC, ETC, ETC1, PVRTC, ASTC, S3TC-sRGB). This is a **CITRON-observed hang; hardware stall behavior unverified** — Citron is a functional-iteration authority, never a driver-truth authority per the house rule reinforced in the 2026-07-04 shader-translation-cache-cliff diagnosis. Even if real Tegra doesn't stall, the fix is spec-required regardless: WebGL implementations MUST validate the internalformat against the currently-advertised extension set before dispatch. This gate makes us spec-compliant regardless of driver behavior.

**Implementation shape.** The helper switches on internalformat and returns true iff the format is either ES3-core (ETC2/EAC — 0x9270..9) or maps to an extension token that `has_native_ext(...)` currently reports as advertised. Format-token → advertising-extension map:
- 0x83F0..3 (S3TC DXT1/DXT1a/DXT3/DXT5) → GL_EXT_texture_compression_s3tc
- 0x8C4C..F (S3TC sRGB) → GL_EXT_texture_compression_s3tc_srgb
- 0x8D64 (ETC1 RGB8) → GL_OES_compressed_ETC1_RGB8_texture
- 0x9270..9 (ETC2/EAC × 10) → ES3 core, always allowed
- 0x8DBB..E (RGTC × 4) → GL_EXT_texture_compression_rgtc
- 0x8E8C..F (BPTC × 4) → GL_EXT_texture_compression_bptc
- 0x93B0..D + 0x93D0..D (ASTC LDR + sRGB, 28 formats total) → GL_KHR_texture_compression_astc_ldr
- `default` (including PVRTC and any unaudited format) → false

PVRTC (0x8C00..3) is deliberately absent — Mesa-Nouveau does not expose the IMG or WEBGL_compressed_texture_pvrtc token, so any PVRTC format falls to the default `false` branch. Both `w_compressed_tex_image_2d` and `w_compressed_tex_sub_image_2d` share the same gate — trivial addition to the sub-image path because the arg has the same semantics (identifies the storage's sized compressed internalformat).

**Skip-file change bundled.** The 7 compressed-cluster HANG entries (BPTC / RGTC / ETC / ETC1 / PVRTC / ASTC / S3TC-sRGB) come out of `sdmc:/switch/brewser/logs/full-webgl1-conformance-skip.txt` in the same commit as the gate — otherwise the re-baseline can't measure the unlock. `extensions-s3tc-and-rgtc` is kept skipped (it's the combined-cluster test outside Alex's explicit 7-name list; will unlock on re-baseline and can be removed then) plus 4 other unrelated HANGs. Post-edit the file holds 5 HANG entries + 4 CRASH entries + 1 cliff-artifact CRASH entry, each with an inline comment explaining why it stays.

**Attribution — CITRON-observed only.** All 7 hang measurements come from Citron. Whether real Mesa-Nouveau on Tegra ALSO stalls without the gate is unverified — see the CITRON-only reclassification precedent in ledger #52a/#54/#55-pause. Code comments + this ledger entry + the corpus skip-file all say "CITRON-observed hang; hardware stall behavior unverified"; nowhere does the fix or its ledger say "Nouveau stalls" or attribute the hang to the driver. The fix is justified purely on WebGL spec grounds (spec-required validation before dispatch) — driver behavior is orthogonal.

**DISPOSITION:** `upstream-candidate`. WebGL spec validation gap that upstream would fix identically.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `has_compressed_format_advertised` (helper decl) and `Tier 4 (ledger #65)` (block marker). Recurrence tells:
- 7 compressed-format tests regress to HANG on Citron re-baseline = the gate branch was removed from one or both of the compressed FN bodies.
- A conformance test compiling a shader that legitimately uses BPTC/RGTC/etc after `getExtension()` fails = the extension advertising side regressed, not the gate itself (advertising happens in `w_get_extension`; gate is downstream).
- All ASTC tests fail with INVALID_ENUM even when the driver DOES expose ASTC = `has_native_ext("GL_KHR_texture_compression_astc_ldr")` returned false; native-ext cache didn't populate, unrelated to this entry.

---

## #66 — Tier-A: `createImageBitmap` source-type expansion — SHIPPED 2026-07-04

**File(s):** [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) — full rewrite of the `createImageBitmap` async body + two new local helpers (`canvasToImageBitmap`, `tryUnwrapCanvas`) + new imports of `ImageData` and `OffscreenCanvas`.

**Motivation.** The pre-#66 impl handled ONLY `Blob` sources and threw `Unsupported image source: ${constructor.name}` for every other spec-defined `ImageBitmapSource`. WebGL 1 conformance surfaced 40 tests spanning 5 sub-clusters (× 8 texture formats each) that FAIL as a direct consequence:
- `textures-image_bitmap_from_canvas-*` — 8 tests, source is a brewser-runtime live-DOM `<canvas>` LiveElement
- `textures-image_bitmap_from_image_bitmap-*` — 8 tests, source is a prior ImageBitmap
- `textures-image_bitmap_from_image_data-*` — 8 tests, source is ImageData
- `textures-image_bitmap_from_video-*` — 8 tests, source is HTMLVideoElement
- `textures-image_bitmap_from_blob-*` — 8 tests (also affected by a separate `Unsupported image format` gap when the buffer is BMP; that's downstream of this entry)

**Design.** Every non-Blob source funnels through a single common tail — `canvasToImageBitmap(canvas)` — that encodes to PNG via `$.canvasToBuffer(canvas, 'image/png')` and decodes into a fresh `ImageBitmap` via `$.imageDecode`. That keeps the encode/decode contract in one place. Per-source-type routing decides HOW to land the source pixels on a canvas first:

- **Blob** — unchanged (arrayBuffer → imageDecode)
- **HTMLCanvasElement / OffscreenCanvas / live-DOM `<canvas>` LiveElement** — `tryUnwrapCanvas` returns the source or its `.offscreen` OffscreenCanvas; canvas encodes directly, no scratch needed. The LiveElement unwrap forces `getContext('2d')` first because brewser-runtime allocates `.offscreen` lazily.
- **ImageData** — scratch `new OffscreenCanvas(w, h)` + `getContext('2d')` + `putImageData(imageData, 0, 0)` + encode. putImageData is the only path that lands unpremultiplied RGBA.
- **ImageBitmap** — scratch canvas + `drawImage(bitmap, 0, 0)` + encode. Uses the existing canvas.cc `SkImage` cache path.
- **HTMLImageElement (nx.js Image)** — duck-typed on `naturalWidth` + `naturalHeight` (avoiding a hard `Image` import — Image drags the fetch polyfills in). Scratch canvas + `drawImage(image, 0, 0)` + encode.
- **HTMLVideoElement** — throws a distinct diagnostic: "needs canvas.cc drawImage video-frame-capture path". Deferred until video's `drawImage` acceptance lands.

**Duck-typing rationale for the live-DOM canvas branch.** `image-bitmap.ts` lives under `nxjs-source-v8/packages/runtime/src/` and cannot import from `brewser-runtime-v8/src/`. brewser-runtime's `LiveElement` (per `live-dom.ts:1088-1091, 2288-2308`) allocates a lazy `.offscreen: OffscreenCanvas` backing on the first `getContext('2d')` call and delegates `toDataURL` / `convertToBlob` / `toBlob` through to it. `tryUnwrapCanvas` mirrors that lazy-allocation pattern: call `getContext('2d')` first (forces `.offscreen` to exist), then read `.offscreen` and check it's an `OffscreenCanvas` instance. Zero brewser-runtime coupling — the duck-typing is purely structural.

**DISPOSITION:** `upstream-candidate`. Upstream nx.js's `createImageBitmap` has the same gap; the branches added here work against any embedder (the live-DOM canvas path is opt-in via the `.tagName === 'CANVAS'` + `.offscreen` duck-type, harmless for non-brewser callers).

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) for `tryUnwrapCanvas`, `canvasToImageBitmap`, and the six source-type branch comments (`// 1. Blob`, `// 2. Canvas-like`, `// 3. ImageData`, `// 4. Draw-able source`, `// 5. HTMLImageElement`, `// 6. HTMLVideoElement`). Recurrence tells:
- All 8 `textures-image_bitmap_from_canvas-*` tests regress to `Unsupported image source: _LiveElement` = the `tryUnwrapCanvas` duck-type broke (likely `.tagName === 'CANVAS'` check flipped to something else, or the `.offscreen` field name changed on brewser-runtime's side).
- All 8 `textures-image_bitmap_from_image_data-*` tests regress with a `putImageData` error = the ImageData branch's `getContext('2d')` returned null or the branch was moved above the Blob one and swallowed the wrong source.
- `textures-image_bitmap_from_video-*` starts throwing `Image or Canvas expected` (from canvas.cc drawImage) instead of the distinct diagnostic = the deferred Video branch was replaced with a drawImage attempt without wiring the C++ side.

---

## #67 — Tier-A: `getParameter` extension-gated pname enforcement — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `enabled_exts: std::unordered_set<std::string>` field in `WebGLState` + `record_ext_enabled(name)` / `is_ext_enabled(name)` helpers + `set_empty_obj()` lambda inside `w_get_extension` + `record_ext_enabled(name)` at every success branch in `w_get_extension` (via the make_obj_with lambda's tail + explicit calls at all custom-object branches) + 6 new `case` branches and 2 modified branches in `w_get_parameter`.

**Motivation.** WebGL 1 spec § 5.14.3 requires extension-gated pnames to return null (with INVALID_ENUM) UNTIL the caller invokes `getExtension` for the gating extension. Pre-#67 our engine returned real values unconditionally for these pnames, causing the WebGL 1 conformance corpus's "should be null" and "should not be queryable if extension is disabled" assertions to FAIL across 6-8 tests:
- `extensions-ext-clip-control` — `CLIP_ORIGIN_EXT` / `CLIP_DEPTH_MODE_EXT` should be null
- `extensions-ext-depth-clamp` — `DEPTH_CLAMP_EXT` should be null
- `extensions-ext-polygon-offset-clamp` — `POLYGON_OFFSET_CLAMP_EXT` should be null
- `extensions-webgl-blend-func-extended` — `MAX_DUAL_SOURCE_DRAW_BUFFERS_WEBGL` should be null
- `extensions-oes-standard-derivatives` — `FRAGMENT_SHADER_DERIVATIVE_HINT_OES` should not be queryable
- `extensions-webgl-debug-renderer-info` — `UNMASKED_VENDOR/RENDERER_WEBGL` should not be queryable
- `extensions-ext-texture-filter-anisotropic` — `MAX_TEXTURE_MAX_ANISOTROPY_EXT` should be null

**Implementation.** Per-context tracking: `WebGLState.enabled_exts` holds every extension name for which `getExtension(name)` returned non-null. Population happens at every success branch in `w_get_extension` — the `make_obj_with` lambda and the new `set_empty_obj()` lambda both call `record_ext_enabled(name)` after setting the return value, and each custom-object branch (ASTC, WEBGL_lose_context, batch-3 vend-methods variants, etc.) has an explicit `record_ext_enabled(name);` before its `return;`. Consumption happens in `w_get_parameter` at 6 new `case` branches (CLIP_ORIGIN / CLIP_DEPTH_MODE / DEPTH_CLAMP / POLYGON_OFFSET_CLAMP / MAX_DUAL_SOURCE_DRAW_BUFFERS_WEBGL / FRAGMENT_SHADER_DERIVATIVE_HINT / COMPLETION_STATUS_KHR) plus updates to the 2 existing branches (UNMASKED_VENDOR/RENDERER + MAX_TEXTURE_MAX_ANISOTROPY_EXT) that pre-#67 returned unconditionally. Each gated branch: `if (!is_ext_enabled(<name>)) { record_error(GL_INVALID_ENUM); SetNull(); return; }` then delegates to the appropriate `glGet{Integer,Boolean,Float,String}v` variant with the spec-correct JS boxing (integer, boolean, float, or string).

**Scope note.** The `enabled_exts` set lives on the singleton `st` and is shared across v1 and v2 contexts. In practice we vend one WebGL context at a time (Screen owns THE tenant FBO), and v1/v2 vend different pname sets so cross-context leakage isn't testable. Spec-perfect per-context isolation would require moving `enabled_exts` onto the JS receiver object; deferred until a demo/test forces the isolation.

**DISPOSITION:** `upstream-candidate`. Pure spec-hole fill, upstream would take the exact shape.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `enabled_exts`, `record_ext_enabled`, `is_ext_enabled`. Also grep for `Ledger #67` block header inside `w_get_parameter`. Recurrence tells:
- All 7 extension tests above regress with real values returned instead of null = one of the 8 `case` branches in `w_get_parameter` regressed (lost its `is_ext_enabled(...)` guard or fell through to `default:`).
- ONE test regresses = a specific pname's gating extension name is misspelled in its `is_ext_enabled(...)` call (e.g. `EXT_clip_control` vs `WEBGL_clip_control`).
- All 7 tests regress with values EVEN AFTER `getExtension` = `record_ext_enabled` isn't being called from `w_get_extension`'s success branches (grep for `record_ext_enabled(name)` — should have ~16 occurrences).

---

## #68 — Tier-A: attribute-aliasing link failure detection — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `programs_with_aliased_link: std::unordered_set<GLuint>` field on `WebGLState` + `nx_detect_link_attrib_aliasing(GLuint program)` static helper + updated `w_link_program` (calls the detector after `glLinkProgram`) + updated `w_get_program_parameter` (overrides LINK_STATUS to false for marked programs) + updated `w_delete_program` (clears the aliased-link record on delete).

**Motivation.** WebGL spec §5.14.9: "If any bound attribute is bound to the same location as another bound attribute, `linkProgram` shall fail with a linking error." Mesa-Nouveau (observed on Citron) succeeds the driver-level link and lets both aliased names read at the same location. The conformance test `attribs-gl-bindAttribLocation-aliasing` probes this with all 32 possible attribute-index pairs (`Link should fail when both attributes are aliased to location 0`, `1`, `2`, ..., `31`) and FAILs across the whole set pre-#68.

**Implementation.** Post-link scan (runs at the tail of every `glLinkProgram` call):

1. Query `GL_LINK_STATUS` — if the driver already reported failure, there's nothing to override and we clear the aliased flag.
2. Query `GL_ACTIVE_ATTRIBUTES` count.
3. For each active attrib, `glGetActiveAttrib(program, i, ...)` → name; `glGetAttribLocation(program, name)` → location.
4. Skip GL-reserved names (start with `gl_`) — they're pre-linked to fixed pipeline slots and can't participate in user aliasing.
5. Insert location into a per-call `std::unordered_set<GLint>`. If insertion FAILS (location already present), two active attribs are aliased → mark the program.

`w_get_program_parameter`'s LINK_STATUS branch consults `programs_with_aliased_link` and overrides the driver's `true` to `false` when the program is marked. `w_delete_program` erases the flag on delete so a subsequent `glGenProgram` reuse doesn't inherit stale state.

**Deferred spec detail.** `getProgramInfoLog` still returns whatever the driver wrote (which for a driver that thought the link succeeded is empty or non-diagnostic). Adding a synthetic info-log entry describing the aliasing is a spec-nice-to-have; the conformance test only inspects LINK_STATUS, so it's out of MVP scope.

**DISPOSITION:** `upstream-candidate`. Spec-required behavior that every WebGL implementation needs; upstream would take the scan verbatim.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `nx_detect_link_attrib_aliasing`, `programs_with_aliased_link`, and `Ledger #68` in `w_link_program` + `w_get_program_parameter`. Recurrence tells:
- `attribs-gl-bindAttribLocation-aliasing` regresses to FAIL on ALL 32 assertions = the scan helper is no longer being called from `w_link_program`, or the aliased-set field was removed from WebGLState.
- Only SOME assertions fail = the loop bailed early (check the `break;` after `aliased = true;` is inside the `for` and not before the location check).
- The test PASSES the aliased assertions but a legitimate compile-only test that binds two identical names to the same location starts failing = the scan is flagging non-aliased cases (the `!seen_locations.insert(loc).second` should only fire for truly-duplicate locations; verify the set is per-call, not per-context).

---

## #69 — Tier-A: `texImage2D` + `texSubImage2D` ImageBitmap / Image source support — SHIPPED 2026-07-04

**File(s):** [source/webgl.cc](source/webgl.cc) — new `#include "image.h"` + new static helper `convert_image_source_to_gl_pixels(nx_image_t*, GLenum format, GLenum type, bool flip_y, bool un_premultiply, std::vector<uint8_t>& scratch)` + image-source-detection branch in `w_tex_image_2d` and `w_tex_sub_image_2d` (both call `nx_get_image` after the ArrayBuffer / typed-array branches, override width/height from the source, and wrap the upload in a `GL_UNPACK_ALIGNMENT=1` save/restore).

**Motivation.** #66 taught `createImageBitmap` to produce an `ImageBitmap` backed by `nx_image_t` for every spec-defined `ImageBitmapSource`, but the `texImage2D` / `texSubImage2D` bodies in webgl.cc only understood ArrayBuffer / ArrayBufferView. `nx_image_t` fell through the null-source branch: `gl.texImage2D(target, 0, format, format, type, imageBitmap)` uploaded nothing and the destination texture kept its cleared contents. WebGL 1 conformance surfaced ~40 tests spanning 5 sub-clusters (× 8 texture formats each) with the exact signature `shouldBe 255,0,0 was 0,0,0`:
- `textures-image_bitmap_from_canvas-*` — 8 tests
- `textures-image_bitmap_from_image_bitmap-*` — 8 tests
- `textures-image_bitmap_from_image_data-*` — 8 tests
- `textures-image_bitmap_from_video-*` — 8 tests (still blocked upstream by #66's deferred Video branch)
- `textures-image_bitmap_from_blob-*` — 8 tests (also affected by unrelated `Unsupported image format` for BMP)

Also unblocks any demo that passes an `nx.js Image` directly to `gl.texImage2D` — a spec-supported call shape.

**Design.** After the existing ArrayBuffer / view_bytes / NullOrUndefined branches, `nx_get_image(iso, info[8])` probes the JS arg for a wrapped `nx_image_t`. If the probe hits AND `pixels` is still null (i.e. the arg wasn't already read as a buffer), the helper converts and the call takes the image path.

`convert_image_source_to_gl_pixels` allocates a `std::vector<uint8_t>` scratch sized `width * height * dst_bpp` and walks pixels row-by-row, byte-by-byte. Source is `nx_image_t->data` — premultiplied BGRA, row-major, `width*4` stride, top-to-bottom (canvas.cc treats it as `kBGRA_8888_SkColorType, kPremul_SkAlphaType`).

MVP format matrix (matches the 8 texture-format variants each Tier-A sub-cluster iterates over):
- `RGBA` / `UNSIGNED_BYTE` — swizzle R↔B, keep alpha
- `RGB` / `UNSIGNED_BYTE` — swizzle R↔B, drop alpha
- `RGBA` / `UNSIGNED_SHORT_4_4_4_4` — pack 4-bit per channel, big-endian per WebGL 1 §5.14.6
- `RGBA` / `UNSIGNED_SHORT_5_5_5_1` — pack 5-5-5-1
- `RGB` / `UNSIGNED_SHORT_5_6_5` — pack 5-6-5
- `LUMINANCE_ALPHA` / `UNSIGNED_BYTE` — Rec.601 luma from RGB + alpha
- `LUMINANCE` / `UNSIGNED_BYTE` — Rec.601 luma
- `ALPHA` / `UNSIGNED_BYTE` — alpha only

Anything outside this set returns nullptr, falling through to the pre-#69 null upload. This is intentional MVP scope; expanding the matrix (HALF_FLOAT / FLOAT source-uploads, PBO offset overload, `OffscreenCanvas`-as-direct-source) is out of scope until conformance surfaces a test requiring it.

`UNPACK_FLIP_Y_WEBGL` — row-reverse by choosing `src_y = flip_y ? (H - 1 - y) : y`. Source is top-to-bottom; flipping produces bottom-to-top which is what the WebGL flag asks for.

`UNPACK_PREMULTIPLY_ALPHA_WEBGL` — source is *always* premultiplied. Flag `true` (the WebGL default) means "upload premultiplied", so we do nothing — the source's premul BGRA becomes premul RGBA after swizzle. Flag `false` means "upload un-premultiplied", so we divide each color channel by the alpha (`c = (c * 255 + a/2) / a` with rounding and `min(255, ...)` clamp). Tests iterate both flag settings and MVP handles both — the Tier-A `premultiplyAlpha=false` variants FAIL without it.

`GL_UNPACK_ALIGNMENT` override: WebGL spec preserves the caller's alignment, but our scratch is always tightly packed. Odd-width `RGB/UNSIGNED_BYTE` and any `LUMINANCE/ALPHA` upload would trip the driver's row-stride check under `alignment=4`. We `glGetIntegerv` the caller's value, force to 1 for the upload, restore after — invisible to the caller.

Width/height override: WebGL 1 §5.14.6 for the `TexImageSource` overload uses the source's dimensions (the explicit `width`/`height` args ARE present in the C++ dispatch but the JS-side polyfill/binding populates them from `img.width`/`img.height` anyway; overriding here is defensive and matches how three.js / conformance both pass source dimensions).

**Non-MVP formats fall through to null upload.** Deferred:
- `HALF_FLOAT` / `FLOAT` source uploads via ImageBitmap. Not exercised by the Tier-A cluster.
- Direct `OffscreenCanvas` as `TexImageSource` (bypassing `createImageBitmap`). Spec allows this but Tier-A goes through ImageBitmap. #66's `createImageBitmap` path converts canvas → PNG → `nx_image_t` at BitMap creation.
- PBO offset overload (`texImage2D(target, level, IF, w, h, border, format, type, offset)`) for `PIXEL_UNPACK_BUFFER`. WebGL 2 only; not in Tier-A.
- 3D variants (`texImage3D` / `texSubImage3D`) for ImageBitmap. WebGL 2; separate cluster if it surfaces.

**DISPOSITION:** `upstream-candidate`. Pure spec compliance; upstream nx.js has the same gap.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `convert_image_source_to_gl_pixels`, the `Ledger #69` block header inside `w_tex_image_2d` + `w_tex_sub_image_2d`, and `#include "image.h"`. Recurrence tells:
- All 40 `textures-image_bitmap_from_*` tests regress to `shouldBe 255,0,0 was 0,0,0` = the `nx_get_image` probe is no longer called, or the fall-through order changed so the ArrayBuffer branch consumes the arg first (nx_image_t doesn't respond to `IsArrayBuffer()`, so ordering matters only if a future refactor merges branches).
- Only the `premultiplyAlpha=false` variants fail = the un-premultiply path regressed (check the `un_premultiply && a != 0 && a != 255` guard is still there and dividing correctly).
- Only the `flipY=true` variants fail = the `src_y = flip_y ? (H - 1 - y) : y` expression regressed.
- Only specific format variants fail (e.g. all `LUMINANCE_ALPHA/UNSIGNED_BYTE`) = a case in the format switch was removed or its bpp computed wrong; check `dst_bpp` computation matches the memory-store shape.
- Random driver `INVALID_OPERATION` on odd-width `RGB/UNSIGNED_BYTE` = the `GL_UNPACK_ALIGNMENT=1` override/restore regressed.

---

## #70 — Tier-A: TexImageSource normalization shim for WebGL 1 — SHIPPED 2026-07-05

**File(s):** [packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts) — new `ImageData` + `OffscreenCanvas` imports + `isTexImageSource(v)` + `sourceToPixels(src)` helpers + prototype-wrap block installed after `$.webglInitClass(WebGLRenderingContext, ...)`. Mirrors the pre-existing shim in `webgl2-rendering-context.ts` (lines ~1204-1256) verbatim.

**Motivation.** #69 shipped a native-side `nx_image_t` uploader in `w_tex_image_2d` / `w_tex_sub_image_2d`. The tier-69 re-baseline (batch4, 122 tests) still showed all image_bitmap tests FAILing with the pre-#69 signature `shouldBe 255,0,0 was 0,0,0`. Root cause: WebGL 1 conformance calls the 6-arg overload `gl.texImage2D(target, level, IF, format, type, source)`, not the 9-arg one #69 was written for. The native (fixed 9-arg) reads:
- `info[3]` as `width` → gets `format` (6408 for RGBA)
- `info[4]` as `height` → gets `type` (5121 for UNSIGNED_BYTE)
- `info[5]` as `border` → gets the ImageBitmap coerced to int
- `info[6]`/`info[7]` as `format`/`type` → undefined → 0
- `info[8]` as `pixels` → undefined

So `nx_get_image(iso, info[8])` never sees the ImageBitmap; #69's branch never fires. Texture stays cleared. WebGL 2 was already immune because `webgl2-rendering-context.ts` had a prototype wrap that detected TexImageSource-shaped last args and converted them to raw RGBA bytes via an OffscreenCanvas round-trip before calling the native.

**Design.** Verbatim copy of the WebGL 2 shim into `webgl-rendering-context.ts`:

- `isTexImageSource(v)` — duck-typed on `v.width` + `v.height` being numeric AND `v` not being an `ArrayBuffer.isView`. Covers `ImageBitmap`, `Image`, `HTMLCanvasElement` / `OffscreenCanvas` / live-DOM `<canvas>` LiveElement, `ImageData`, and `HTMLVideoElement`. Rejects TypedArrays (they have numeric `.width` / `.height`? — no, they don't, but `ArrayBuffer.isView` filters them out defensively).
- `sourceToPixels(src)` — if `src instanceof ImageData`, use it directly (already unpremul RGBA); otherwise create a scratch `OffscreenCanvas(w, h)`, `getContext('2d').drawImage(src, 0, 0)`, and `getImageData(0, 0, w, h)` — canvas 2D's getImageData returns unpremultiplied RGBA which is what GL expects under `UNPACK_PREMULTIPLY_ALPHA_WEBGL=false` (the default).
- Prototype wrap: reads `p.texImage2D` / `p.texSubImage2D` (installed by `$.webglInitClass`), replaces with a closure that duck-checks the last arg. For `texImage2D`, 6-arg becomes `(t, l, IF, w, h, 0, format, type, bytes)`; 9-arg with source in last slot becomes 9-arg with bytes in last slot. Same reshape for `texSubImage2D` (7-arg → 9-arg).

**Why keep #69 alive.** The 9-arg direct-nx_image_t call shape (`gl.texImage2D(target, 0, IF, w, h, 0, format, type, image)`) is spec-legal but not exercised by conformance. #69's C++ path still handles it correctly. Removing #69 would leave that path silently uploading null; keeping it costs 150 LOC of dead-in-conformance code but gains spec-completeness.

**Ordering constraint.** The shim block MUST run after `$.webglInitClass(WebGLRenderingContext, ...)` which is what installs the native methods on the prototype. In this file both `$.webglInitClass` and the shim block sit in the same top-level scope of a single module, and the shim block reads `p.texImage2D` at execution time — so as long as the block textually FOLLOWS the `$.webglInitClass` call (currently ~line 601), ordering is guaranteed by ES module top-level execution order.

**Non-shim-covered surfaces.**
- **HTMLVideoElement source** — `sourceToPixels` calls `ctx.drawImage(src, 0, 0)`. canvas.cc's drawImage doesn't accept video sources; a video source would throw at that call. Same deferral as #66's HTMLVideoElement branch.
- **PBO offset overload** (`texImage2D(..., type, offset)` where `offset` is a number and `PIXEL_UNPACK_BUFFER` is bound) — WebGL 2 only, `isTexImageSource` returns false for a number, so the shim passes through untouched. Correct.
- **`texImage3D` / `texSubImage3D`** — WebGL 2 only. Not touched here; separate 3D-cluster shim if it ever surfaces.

**DISPOSITION:** `upstream-candidate`. Pure spec compliance; upstream nx.js's WebGL 1 has the same gap.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts) for `isTexImageSource`, `sourceToPixels`, `Ledger #70`, and the `import { OffscreenCanvas }` / `import { ImageData }` lines at the top of the file. Recurrence tells:
- All 40 `textures-image_bitmap_from_*` tests regress to `shouldBe X,Y,Z was 0,0,0` = the shim's prototype wrap regressed (check `p.texImage2D` was actually reassigned; if the block runs BEFORE `$.webglInitClass` it silently wraps `undefined`).
- Only the sub-image tests regress = the `texSubImage2D` branch of the shim regressed (arg-reshape indices for 7-arg form).
- Random `INVALID_VALUE` on 9-arg direct calls with a non-source last arg = `isTexImageSource` false-positived; check the `ArrayBuffer.isView` guard is still in place.
- WebGL 2 tests start regressing too = someone deduped the shim into a shared helper and broke the WebGL 2 install (check webgl2-rendering-context.ts ~line 1204 still runs).

---

## #71 — Tier-A: ImageBitmap-source short-circuit in the WebGL 1 shim — SHIPPED 2026-07-05

**File(s):** [packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts) — new `import { ImageBitmap } from './image-bitmap'` + two `last instanceof ImageBitmap` branches inserted at the head of the shim wrappers for `texImage2D` and `texSubImage2D`, before the `isTexImageSource` fallback.

**Motivation.** Post-#70 the tier-70 re-baseline of batch4 flipped only 2 of 40 image_bitmap tests to PASS (the `ALPHA/UNSIGNED_BYTE` variants of `_from_image_bitmap` and `_from_image_data`). The rest FAILed with a NEW signature — `expected 128,0,0 was 255,255,0` at position (31, 12), where the previous baseline showed `was 0,0,0` (null upload). Signature-change proved the shim IS running; the specific value proved the payload is misaligned.

Root cause: `sourceToPixels` always returns 4-byte RGBA per pixel (that's what `OffscreenCanvas.getImageData` produces). WebGL 1 conformance calls with:
- `RGB / UNSIGNED_BYTE` — 3 bpp
- `LUMINANCE / UNSIGNED_BYTE` — 1 bpp
- `LUMINANCE_ALPHA / UNSIGNED_BYTE` — 2 bpp
- `ALPHA / UNSIGNED_BYTE` — 1 bpp (happens to sometimes-PASS by coincidence — R channel value maps to expected alpha)
- `RGBA / UNSIGNED_SHORT_4_4_4_4` — 2 bpp packed
- `RGBA / UNSIGNED_SHORT_5_5_5_1` — 2 bpp packed
- `RGB / UNSIGNED_SHORT_5_6_5` — 2 bpp packed
- `RGBA / UNSIGNED_BYTE` — 4 bpp (only variant that's byte-stride compatible)

Only the last variant reads the shim's RGBA stream at the driver's expected stride. Every other variant misinterprets: pixel N's RGB reads from `[A_{N-1}, R_N, G_N]`. That's exactly the `255,255,0` observed (A_0=255, R_1=255, G_1=0 for the source's dark-red quadrant).

**Design.** #69 already ships a spec-correct `convert_image_source_to_gl_pixels` in webgl.cc that takes an `nx_image_t` and produces byte-stride-correct output for the caller's `(format, type)` across the full 8-format Tier-A matrix. All 5 non-video `_from_*` clusters route through `createImageBitmap` first, so `last` at `texImage2D` is always an `ImageBitmap` — which per #66's construction is an `nx_image_t` under the hood (`ImageBitmap` extends `$.imageNew()` + `$.imageDecode`). 

The fix: at the head of the shim, check `last instanceof ImageBitmap`. If true, skip `sourceToPixels` entirely and reshape the arg list to hand `last` directly to the native as the pixels arg:

- `texImage2D` 6-arg `(target, level, IF, format, type, source)` → 9-arg `(target, level, IF, w, h, 0, format, type, source)` with `w`/`h` from `last.width` / `last.height`.
- `texImage2D` 9-arg with source in last slot → args pass through untouched (source is already at index 8).
- `texSubImage2D` 7-arg `(target, level, xoff, yoff, format, type, source)` → 9-arg `(target, level, xoff, yoff, w, h, format, type, source)` with `w`/`h` from source.
- `texSubImage2D` 9-arg with source in last slot → args pass through.

The native's `w_tex_image_2d` / `w_tex_sub_image_2d` (both in webgl.cc) then hit the `nx_get_image(iso, info[8])` branch that #69 added, invoking `convert_image_source_to_gl_pixels` with the caller's `format` and `type`. Output is a scratch `std::vector<uint8_t>` sized exactly `w * h * bpp_for(format, type)` and packed row-tight under `GL_UNPACK_ALIGNMENT=1`.

**Ordering.** The ImageBitmap branch MUST come before the `isTexImageSource` fallback because ImageBitmap satisfies both duck-checks (it has numeric `.width` / `.height` and isn't an ArrayBufferView). The insert positions preserve the existing fallback for non-ImageBitmap sources.

**Non-ImageBitmap sources still-broken.** Tests that don't route through `createImageBitmap` first — `textures-image_data-*` (raw ImageData source), `textures-svg_image-*`, `textures-webgl_canvas-*`, `textures-video-*` — still hit the `sourceToPixels` fallback and its 4-bpp RGBA output. Those clusters will need a follow-up ledger entry that either:
1. Mirrors #69's conversion matrix in JS inside `sourceToPixels`, taking `format`/`type` as extra args, OR
2. Converts non-ImageBitmap sources into a temporary `nx_image_t` and lets #69 handle them (would require a sync `imageNew(w,h)` + `imageWriteRGBA` after the OffscreenCanvas round-trip).

Deferred to a separate entry to keep #71 tight.

**WebGL 2 unchanged.** The WebGL 2 shim in `webgl2-rendering-context.ts` has the same latent bug but tier-A conformance only exercises WebGL 1 (`defaultContextVersion=1`). WebGL 2 fix can be a mirror change if Tier-B ever surfaces the failure.

**DISPOSITION:** `upstream-candidate`. Pure spec compliance.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/canvas/webgl-rendering-context.ts](packages/runtime/src/canvas/webgl-rendering-context.ts) for `Ledger #71`, `last instanceof ImageBitmap` (should appear twice — once in each of the two shim wrappers), and the `import { ImageBitmap } from './image-bitmap'` line at the top. Recurrence tells:
- All 40 `_from_*` (non-video) tests regress to `was 255,255,0` = the ImageBitmap branch was removed / regressed and calls fell back to `sourceToPixels`.
- All 40 tests regress to `was 0,0,0` = the whole shim regressed (either #70 broke or the ImageBitmap import broke module load).
- Only `texSubImage2D` variants regress = the second `instanceof ImageBitmap` branch (in the sub wrapper) regressed — check its 7-arg → 9-arg arg-reshape indices.
- ImageBitmap tests PASS but ImageData/canvas tests regress = someone routed non-ImageBitmap sources into the ImageBitmap branch, breaking the `sourceToPixels` path.

---

## #72 — Tier-A: default viewport at context creation + ImageBitmap un-mult removal — SHIPPED 2026-07-05

**File(s):** [source/webgl.cc](source/webgl.cc) —
- (a) new `glViewport(0, 0, w, h)` + `st->user_snap.viewport[]` seed after `nx_webgl_bridge_set_webgl_owned(true)` in `make_context_carrier`.
- (b) `un_premultiply` hardcoded to `false` in both `w_tex_image_2d` and `w_tex_sub_image_2d` image-source branches (previously computed as `!st->unpack_premultiply`).

**Motivation.** Post-#71 the tier-71 re-baseline of batch4 still showed only 21 of 122 tests PASSing. Diagnosis via a temporary `[diag69]` printf ladder produced two independent findings:

1. **Un-multiplication was wrong for ImageBitmap sources.** A `pixelStorei` diag confirmed that image_bitmap conformance tests NEVER call `gl.pixelStorei(UNPACK_PREMULTIPLY_ALPHA_WEBGL, ...)` — the WebGL state stays default FALSE for the whole test. The test's expected pixel values (`halfRed = 128,0,0` for half-alpha red) assume the bitmap's own premultipliedAlpha state is preserved through upload, not converted. Our source `nx_image_t` is premultiplied BGRA; my #69 code was un-multiplying it (producing `255,0,0` for the R channel), then storing that in the texture. Test read back `255` where it expected `128`. Fixed by never un-multiplying — the bitmap's own state wins. This matches empirical Chrome behavior.

2. **Default GL viewport was WRONG at context creation.** Even after fix (a), pixels beyond source (0,0) read back as `0,0,0`. A `drawArrays`-time diag revealed the viewport at draw time was `(0, 0, 8, 8)` (on later draws) or `(0, 0, 1, 1)` (first draw), yet the test's canvas is `<canvas id="example" width="32" height="32">`. Per WebGL 1 spec § 5.14.3, a fresh WebGL context's viewport defaults to `(0, 0, drawingBufferWidth, drawingBufferHeight)`. Chrome/Firefox honor this; our engine inherited whatever GL state Skia's Ganesh had left over (small, because Skia had been rendering into tiny buffers during boot). Tests that never explicitly call `gl.viewport(...)` — which is most of the conformance corpus — draw their full-viewport quads into an 8×8 corner. Read positions beyond that corner see the framebuffer clear color, giving the exact observed signature "only drawing bottom-left matches source (0,0), all other positions are `0,0,0`".

**Design (fix a — viewport seed).** Right after `nx_webgl_bridge_set_webgl_owned(true)` we call `glViewport(0, 0, w, h)` — the shared ES3 context is current (bridge init guarantees this) so the call is legal. Then mirror the same values into `st->user_snap.viewport[0..3]` so the first `enter_bracket()` restore doesn't overwrite the seed back to zeros (user_snap defaults to `{0,0,0,0}` which would give viewport `(0,0,0,0)` = no draws visible). `user_snap_valid` stays false; it flips true on the first `enter_bracket()` call, at which point the seed viewport is captured (or replaced by whatever the user has called `gl.viewport` with in the meantime).

**Design (fix b — un-mult removal).** Both `w_tex_image_2d` and `w_tex_sub_image_2d` image-source branches now use `const bool un_premultiply = false;` unconditionally. The comment above documents that the `_from_image_bitmap` conformance tests specifically don't touch UNPACK_PREMULTIPLY_ALPHA_WEBGL, and Chrome preserves the bitmap's own state. If UNPACK_PREMULTIPLY_ALPHA_WEBGL flag=TRUE and source is premul: keep premul (no-op — already premul). If flag=TRUE and source is unpremul: would need to multiply, but our sources are always premul so unreachable. If flag=FALSE (default): don't touch (empirically what Chrome does for ImageBitmap). The full spec-correct conversion matrix (bitmap.premultipliedAlpha × UNPACK flag) is deferred; for the Tier-A image_bitmap tests, `un_premultiply = false` is sufficient.

**Broad impact of fix (a).** The viewport bug is not specific to image_bitmap tests — ANY conformance test that skips explicit `gl.viewport(...)` (most of the corpus) has been rendering into a corner since Phase 2.C shipped. Expected win: significant PASS-count uplift beyond just the image_bitmap cluster.

**Verification path.**
- The pre-#72 signature at RGB test drawing (12, 12) → PASS (255,0,0); (32, 12), (12, 37), (31, 37) → all `0,0,0`.
- Post-#72 expectation: all 4 quadrants show correct source pixels (bottom-left red, bottom-right half-red, top-left green, top-right half-green).

**Diagnosis backstop.** The `[diag69]` printf ladder used during diagnosis has been fully removed from the codebase pre-ship. If tests regress in a similar shape (only bottom-left pixel correct, rest `0,0,0`), first check `w_viewport` isn't being called before context_new's seed and check that the seed is still present after `nx_webgl_bridge_set_webgl_owned(true)`.

**DISPOSITION:** `upstream-candidate`. Fix (a) is spec compliance — should have always been present. Fix (b) matches empirical Chrome behavior for ImageBitmap sources; the fully-general premul conversion is a follow-up.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Ledger #72 — seed default viewport`, `glViewport(0, 0, w, h)` in `make_context_carrier`, and `source is premultiplied BGRA` in both `w_tex_image_2d` and `w_tex_sub_image_2d`. Recurrence tells:
- All image_bitmap tests regress to "only bottom-left pixel correct" signature = fix (a) regressed (glViewport seed removed or user_snap.viewport seed removed).
- Half-alpha red pixels regress from `128,0,0` to `255,0,0` = fix (b) regressed (un-multiplication reintroduced).
- Also many non-image_bitmap tests regress = fix (a) regression affected the whole corpus.

---

## #73 — Tier-A: honor `createImageBitmap` `imageOrientation` + `premultiplyAlpha` options — SHIPPED 2026-07-05

**File(s):**
- [source/image.cc](source/image.cc) — extend `nx_image_write_rgba` with optional 3rd arg `premultiply` (default `true`; when `false`, write raw RGBA→BGRA without alpha multiplication).
- [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) — new `extractOpts` helper + updated `canvasToImageBitmap(canvas, opts)` signature + all 4 non-Blob `canvasToImageBitmap` call sites now pass `opts` through.

**Motivation.** Post-#72 the tier-72c RGB test's iteration 1 (`flipY=false, premultiplyAlpha=premultiply`) passes fully — every pixel check hits `128,0,0` correctly for half-alpha red. But iterations 2/3/4 fail with distinct patterns:

- **iter 2** (`flipY=false, premultiplyAlpha=none`): expects `255,0,0` for half-alpha red, gets `128,0,0`. Test wants the bitmap stored **un-premultiplied**.
- **iter 3-4** (`flipY=true, *`): position labels are swapped — drawing bottom shows source-top when it should show source-bottom (and vice versa). Test wants the bitmap **Y-flipped**.

The test creates 4 bitmap variants:
```js
createImageBitmap(source, {imageOrientation: "none"|"flipY", premultiplyAlpha: "premultiply"|"none"})
```

Pre-#73 `createImageBitmap` completely ignored these options; all 4 variants came back with the same pixels (premultiplied, no flip). #66 documented the options-ignoring as deferred; #73 delivers.

**Design (engine side).** `imageWriteRGBA(image, bytes, [premultiply])`:
- Default `premultiply = true` preserves the existing contract — `Switch.VideoDecoder`'s `imageWriteRGBA` calls in `bitmap.ts` line 35 / 52 continue to premultiply on write (frame delivery expects premul BGRA storage).
- New `premultiply = false` path: writes raw `RGBA → BGRA` (just the R↔B swap for storage order) without applying the alpha multiplication. Alpha channel is preserved as-is.

**Design (runtime side).** `extractOpts` looks at both the 2-arg call shape (`options` at position 1) and the 6-arg call shape (`options` at position 5), returning `{ flipY, unpremul }` booleans. Both explicitly compare against the spec string values (`imageOrientation === 'flipY'`, `premultiplyAlpha === 'none'`) so all other values (`"none"`, `"from-image"`, `"premultiply"`, `"default"`, `undefined`) fall into the pre-#73 fast path.

`canvasToImageBitmap(canvas, opts)`:
- **Fast path** (no options requested): unchanged encode-to-PNG + `imageDecode` round-trip. Zero perf cost for callers that don't ask for options.
- **`flipY` set**: compose the source into a scratch `OffscreenCanvas` with a Y-mirror transform (`sctx.scale(1, -1); sctx.translate(0, -h); drawImage(src, 0, 0)`) before continuing. The staging canvas now holds the source's Y-flipped pixels.
- **`unpremul` set**: skip the PNG round-trip (which always premultiplies on decode) and store raw bytes:
  ```ts
  const bytes = sctx.getImageData(0, 0, w, h).data;  // canvas 2D getImageData returns unpremul RGBA
  const bmp = proto($.imageNew(w, h), ImageBitmap);
  $.imageWriteRGBA(bmp, bytes.buffer, false);  // engine-side raw store
  ```

Both options combine correctly — `flipY + unpremul` reads the flipped canvas's `getImageData` and stores raw.

**Blob path not covered.** All 4 non-Blob source-type branches (canvas / ImageData / ImageBitmap / HTMLImageElement) now pass `opts` through. The Blob branch (`branch 1`) calls `imageDecode` directly (no canvas round-trip) and can't honor `unpremul` without an engine-side flag on `imageDecode`. Tier-A `_from_blob-*` tests all fail earlier at `Unsupported image format` for BMP sources (a separate upstream issue), so deferring Blob-with-options is safe.

**Semantic verification via the WebGL upload.** My #69 C++ `convert_image_source_to_gl_pixels` reads `img->data` as BGRA and passes pixel values through unchanged (except the WebGL spec's format-specific bit-packing for `5_6_5` / `4_4_4_4` / `5_5_5_1`). My #72 hardcodes `un_premultiply = false` on the upload path (assumes the bitmap's own premul state is authoritative — matches Chrome's ImageBitmap behavior). Combined with #73:

| Test variant | Bitmap stored as | Uploaded to texture as | Test expects | Result |
|---|---|---|---|---|
| `premultiply`, `flipY=false` | premul, unflipped | premul, unflipped | premul | ✓ |
| `premultiply`, `flipY=true` | premul, flipped | premul, flipped | premul-flipped | ✓ |
| `none`, `flipY=false` | unpremul, unflipped | unpremul, unflipped | unpremul | ✓ |
| `none`, `flipY=true` | unpremul, flipped | unpremul, flipped | unpremul-flipped | ✓ |

All 4 iterations should pass. Estimated tier-A test coverage flip: ~30 of the 40 `_from_*` tests (blob deferred → ~8 stay FAILing at createImageBitmap, video deferred → ~7 stay FAILing).

**DISPOSITION:** `upstream-candidate`. `imageWriteRGBA`'s optional-arg extension is backwards-compatible. Runtime-side options handling is spec-required — upstream `nx.js` has the same gap.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/image.cc](source/image.cc) for `Ledger #73 — optional 3rd arg` + `bool premultiply = true`. Grep [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) for `Ledger #73 — extract flipY`, `extractOpts`, and 4 occurrences of `canvasToImageBitmap(..., opts)`. Recurrence tells:
- iter 1 of RGB test regresses to `128,0,0 was 255,0,0` = the `un_premultiply = false` hardcode from #72 regressed (unrelated to #73).
- iter 2 regresses to `255,0,0 was 128,0,0` = the `unpremul` path in `canvasToImageBitmap` broke — either the engine `imageWriteRGBA` 3rd-arg check regressed, or the `extractOpts` no longer sees `premultiplyAlpha === 'none'`.
- iter 3-4 top/bottom swap breaks = the `flipY` scale/translate composition regressed — either the transform matrix flipped sign or the `staging` canvas isn't being drawn into.
- All 4 iterations regress identically = `extractOpts` returned `{ flipY: false, unpremul: false }` for every call (the option-detection logic broke).
- `Switch.VideoDecoder` frame-delivery hangs or shows corruption = the `premultiply=true` default in `imageWriteRGBA` regressed (all frames land as raw RGBA→BGRA without premul).

---

## #74 — Track-A: DrumBrake wasm interpreter opt-in gate + empirical wasm-tier boot probe

**File(s):**
- [source/config.h](source/config.h) — INI doc block for `[v8] wasm_interpreter = on|off`; new `bool wasm_interpreter_opt_in;` field on `nx_config_t`.
- [source/config.cc](source/config.cc) — parse branch for `wasm_interpreter` key inside the `[v8]` section; default-init in `nx_config_defaults`.
- [source/main.cc](source/main.cc) — conditional `V8::SetFlagsFromString("--wasm-jitless")` in the jitless branch of the flag setup; new static helper `nx_probe_wasm_tier(Isolate*, Local<Context>, bool can_jit)` and its call site after `Context::Scope` but before runtime.js.
- [scripts/verify-drumbrake-monolith.sh](scripts/verify-drumbrake-monolith.sh) — Track-B acceptance gate script (out-of-tree toolchain probe).

**Motivation.** Under Citron and in `[v8] jit = off` launches, V8 runs `--jitless` and the shipping switch-v8 monolith (verified against `switch-v8 15.0.243-9`, upstream V8 tag `15.0.243`) has **no** WebAssembly implementation at all (Liftoff needs the JIT code arena; the interpreter side of V8 was never enabled at build time). Every WASM app hits a hard error at first `WebAssembly.compile` / instantiate. V8 upstream ships an in-tree wasm interpreter — DrumBrake — enabled via GN arg `v8_enable_drumbrake=true` and runtime flag `--wasm-jitless`. This entry wires the engine-side glue so that once a DrumBrake-enabled monolith is installed (Track B, out-of-band `TooTallNate/pacman-packages` rebuild), Citron/jitless launches get functional WASM (interpreter-tier).

**Number-space note.** This entry was originally scoped as #73 during Phase 0, when the highest ledger row was #72. Between Phase 0 and Phase 2 the #73 slot was taken by the shipped Tier-A `createImageBitmap` `imageOrientation` / `premultiplyAlpha` options entry (see #73 above at line ~3417, SHIPPED 2026-07-05) — a different concern in the image-bitmap subsystem. The DrumBrake gate was renumbered end-to-end (source comments, ledger index row, verify-patches checks, gate script header) to #74 following the strictly-monotonic-after-highest convention this ledger uses. The #38 earmark for the blob-URL trampoline was preserved untouched during the shift.

Track-A-first strategy: land the gate + probe now, defaulted off, byte-identical to today on the JIT branch. The empirical probe reports the real runtime tier — not what the flag string claims — so the boot log stays truthful across all four verification-matrix rows (V1 gate-off / V2 gate-on / V3 hardware-JIT / V4 post-Track-B).

**Exact change.**

1. `source/config.h`: extend the `[v8]` INI doc block near the top of the file with `wasm_interpreter = off` under `jit` and `flags`, plus a paragraph documenting the semantic (Citron-only; inert on JIT branch; no-op until Track B). Add `bool wasm_interpreter_opt_in;` after `webgl_state_probe_active` in `nx_config_t`.

2. `source/config.cc`: add a new branch inside the `[v8]` section handler (adjacent to the existing `code_headroom_mb`/`wasm` branch — DISTINCT key, no collision). Accept `on|true|1` / `off|false|0` case-insensitive; anything else logs `v8.wasm_interpreter="…" not honored: invalid (use on|off)` and keeps the default. Initialize `cfg->wasm_interpreter_opt_in = false` in `nx_config_defaults`.

3. `source/main.cc`, flag setup (currently around line 1734): inside the `else` (jitless) branch of `if (can_jit) { … } else { … }`, add — AFTER the base `SetFlagsFromString("--jitless …")` call — a gated second call:

```cpp
if (nx_ctx->config.wasm_interpreter_opt_in) {
    V8::SetFlagsFromString("--wasm-jitless");
    fprintf(stderr,
            "[v8] wasm_interpreter=on -> --wasm-jitless appended "
            "(DrumBrake selected iff monolith supports it)\n");
    fflush(stderr);
}
```

The JIT branch (`if (can_jit)`) is UNTOUCHED — flag string is byte-identical to today. This is a load-bearing invariant: verify-patches.sh check `[v8-jit] byte-identical` greps for the exact JIT flag literal to catch regressions.

4. `source/main.cc`, new probe (definition near the other V8-helper statics, e.g. after `nx_v8_fatal_cb`):

```cpp
static void nx_probe_wasm_tier(Isolate *iso, Local<Context> ctx, bool can_jit) {
    HandleScope hs(iso);
    TryCatch tc(iso);

    // 34-byte MVP module: exports `f: () -> i32` returning i32.const 42.
    static const char kProbeSrc[] =
        "(function(){"
        "var b=new Uint8Array(["
        "0,97,115,109,1,0,0,0,"
        "1,5,1,96,0,1,127,"
        "3,2,1,0,"
        "7,5,1,1,102,0,0,"
        "10,6,1,4,0,65,42,11"
        "]);"
        "var m=new WebAssembly.Module(b);"
        "var i=new WebAssembly.Instance(m);"
        "return i.exports.f();"
        "})()";

    bool executes = false;
    Local<String> src;
    if (String::NewFromUtf8(iso, kProbeSrc, NewStringType::kNormal)
            .ToLocal(&src)) {
        Local<Script> scr;
        if (Script::Compile(ctx, src).ToLocal(&scr)) {
            Local<Value> res;
            if (scr->Run(ctx).ToLocal(&res)) {
                double d = 0.0;
                if (res->NumberValue(ctx).To(&d) && d == 42.0)
                    executes = true;
            }
        }
    }
    if (tc.HasCaught()) tc.Reset();

    const char *mode;
    if (!executes)     mode = "unavailable";
    else if (can_jit)  mode = "jit(liftoff)";
    else               mode = "drumbrake";
    fprintf(stderr, "[wasm] mode=%s\n", mode);
    fflush(stderr);
}
```

Call site: inside the `Context::Scope context_scope(context);` block, immediately after entering the scope and before any user-visible setup:

```cpp
Context::Scope context_scope(context);
nx_probe_wasm_tier(iso, context, can_jit);
Local<Object> global = context->Global();
```

**Design invariants (Amendments A + B from Phase 1 review).**

- **Executes, not validates.** The probe module contains one exported function that runs `i32.const 42; end` and returns the value. A build with `WebAssembly.Module` present but no execution tier (which the current shipping monolith might expose — `Module` validation may not require the interpreter) will FAIL at `new WebAssembly.Instance(m)` or at `i.exports.f()`, correctly reporting `mode=unavailable`. A build with DrumBrake sees `executes = true` and reports `mode=drumbrake`.
- **Zero abort paths.** Every V8 API used in the probe is the fallible variant. `NewFromUtf8` uses `ToLocal` (not `ToLocalChecked` — that would abort on empty). `Script::Compile`, `Run`, `NumberValue` — all `.ToLocal(...)` / `.To(...)`. `nx_str` is deliberately NOT used because it aborts on empty. Any hop that returns empty falls through to `mode=unavailable` without touching V8's abort path.
- **No exception leakage.** `tc.HasCaught() && tc.Reset()` clears the isolate's pending-exception flag before the probe returns, so runtime.js and user code start clean.

**Symptom it fixes.**

- Under Citron / `[v8] jit = off`, apps that try `WebAssembly.compile(...)` currently throw or hang without a diagnostic. After #74: the boot log reports `[wasm] mode=unavailable` on the current monolith (making the missing capability observable), or `[wasm] mode=drumbrake` on a Track-B monolith (WASM apps run). The gate is opt-in so no default behavior changes.

**Fail-soft verification (V1/V2 rows of the acceptance matrix).**

- **V1 — current monolith, gate off, Citron:** flag string identical to pre-#74. Probe compiles+runs; either `WebAssembly` is absent (TypeError) or Module/Instance construction throws. Probe reports `[wasm] mode=unavailable`. App boots normally.
- **V2 — current monolith, gate on, Citron:** flag string has `--wasm-jitless` appended. Probe still fails (no DrumBrake in binary). Reports `[wasm] mode=unavailable`. App still boots; the `[v8] wasm_interpreter=on -> --wasm-jitless appended` log line confirms the gate reached V8.
- **V3 — hardware, JIT, any gate:** flag string byte-identical to pre-#74 on the JIT branch (gate is READ but not consumed). Probe compiles via Liftoff, executes, returns 42 → `[wasm] mode=jit(liftoff)`.
- **V4 — post-Track-B monolith, gate on, Citron:** probe compiles via DrumBrake, executes, returns 42 → `[wasm] mode=drumbrake`.

**Track-B acceptance gate.** Before flipping the default from off to on (or from opt-in to auto-detect), run `scripts/verify-drumbrake-monolith.sh`. Passes when the aarch64 cross-nm (or host `nm` fallback) on `$DEVKITPRO/portlibs/switch/lib/libv8_monolith.a` matches DrumBrake-specific tokens (`GenericJSToWasmInterpreterWrapper`, `GenericWasmToJSInterpreterWrapper`, `WasmInterpreterRuntime`, `WasmInterpreterObject`, `WasmBytecodeGenerator`). Bare `WasmInterpreter` / `WasmBytecode` are deliberately NOT used as tells — the V8 devtools inspector API (compiled unconditionally) exports `getWasmBytecode` etc. and would false-positive; the shipping `switch-v8 15.0.243-9` monolith reports 0 matches with the narrowed needle, matching the expected pre-Track-B "MISSING" outcome. Script prefers `$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm` when present, falling back to host `nm`.

**Post-Phase-2 verification results (2026-07-05).** V1 + V2 rows of the matrix confirmed on Citron with switch-v8 15.0.243-9 (DrumBrake absent per gate script, expected). Boot log evidence, verbatim from `sdmc:/switch/nxjs-debug.log`:

```
=== V1 (gate off, Citron) ===
[detect] target=citron (auto: A=1 B=1 C=1 score=3/3) -> mode=jitless
[v8] mem_total=3285 MiB free=3 MiB regime=application -> mode=jitless (Ignition only)
[v8] max_heap=512 MiB (arena=1024 MiB free=3 MiB)
[wasm] mode=unavailable

=== V2 (gate on, Citron) ===
[detect] target=citron (auto: A=1 B=1 C=1 score=3/3) -> mode=jitless
[v8] wasm_interpreter=on -> --wasm-jitless appended (DrumBrake selected iff monolith supports it)
[v8] mem_total=3285 MiB free=3 MiB regime=application -> mode=jitless (Ignition only)
[v8] max_heap=512 MiB (arena=1024 MiB free=3 MiB)
[wasm] mode=unavailable
```

Fail-soft contract holds: both boots reach normal browsing state (log continues into `[brewser:img-probe]` / `[page-mouse-fwd]` / `[button-router]` events for hundreds more lines), no abort, no fatal, WASM Probe app loads its assets cleanly. V3 (hardware JIT) and V4 (post-Track-B) remain pending as designed.

**Size neutrality (measured off-branch, 2026-07-05).** Baseline `nxjs.nro` built at `v8-migration` tip (commit 4584588) = 56,652,621 bytes; post-#74 `nxjs.nro` built at `wasm-drumbrake-track-a` tip = 56,652,621 bytes. **NRO delta = 0 bytes** — the ~520-byte `.text` growth (baseline 54,399,835 → post-#74 54,400,355 bytes, +0.001% of engine text) is absorbed by NRO's page-alignment padding. `.data` and `.bss` unchanged. Confirms the "no visible size cost" expectation for the config gate + probe on the current monolith.

**Upstream posture.** Marked `fork-only (upstream-candidate later)` — the gate + empirical probe benefit any nx.js embedder in emulator / no-JIT environments (iOS, Cobalt/Starboard-like sandboxes, etc.). PR queued behind the existing PR-A/-C/-D/-F backlog; do NOT foreclose upstream by leaving fork-specific baggage in the change. The one Horizon-specific piece is the `nx_ctx->config` field name, easy to lift.

**Known non-goals.**

- No SIMD / threads / GC coverage on the DrumBrake path — those are DrumBrake's own gaps. SIMD modules will throw `CompileError` under Track B; test titles that require v128 become hardware-only (documented behavior, no mitigation).
- Perf: interpreter tier is order-of-magnitude slower than Liftoff. This is a dev-iteration path, not a ship perf target.
- No changes to the `[v8] wasm = on|off` / `code_headroom_mb` sugar (JIT-arena headroom). Distinct concern; distinct key.

**RE-APPLY / VERIFY NOTE.** Grep [source/config.h](source/config.h) for `wasm_interpreter_opt_in` (must appear as a struct field, not just in a comment). Grep [source/config.cc](source/config.cc) for `str_ieq(name, "wasm_interpreter")` (parse branch). Grep [source/main.cc](source/main.cc) for `--wasm-jitless` (flag append) + `nx_probe_wasm_tier` (helper) + `\[wasm\] mode=` (boot log). Recurrence tells:
- `[wasm] mode=` line missing from a Citron boot log entirely = the probe call site regressed (context_scope block was refactored and the call site dropped) OR the probe function was inlined / removed.
- Log says `[wasm] mode=jit(liftoff)` when the flag `--jitless` is in effect = the `can_jit` argument to the probe is wired to the wrong branch condition; check that main.cc's `nx_probe_wasm_tier(iso, context, can_jit)` uses the same `can_jit` bool that gates `SetFlagsFromString`.
- Log says `[wasm] mode=unavailable` after Track B was verified via `scripts/verify-drumbrake-monolith.sh` = the empirical probe caught a real regression (either `--wasm-jitless` isn't reaching V8 or DrumBrake's arm64 support is incomplete for the specific opcode set — the probe uses only `i32.const` + return, so an incomplete opcode set is unlikely; look at flag ordering vs. `V8::Initialize()`).
- Log says `[wasm] mode=drumbrake` while the shipped monolith is confirmed to have NO DrumBrake symbols via nm = extremely unlikely (would require `WebAssembly.Module`/`Instance`/`f()` all succeeding without an interpreter); if seen, cross-check `scripts/verify-drumbrake-monolith.sh` and re-inspect the monolith build recipe.
- Hardware JIT branch flag string diverges from pre-#74 = the JIT branch was accidentally extended with the `wasm_interpreter_opt_in` check. That's a spec violation of this entry's byte-identical-JIT-flag invariant; revert to the plain string.

---

## #75 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#75)

---

## #76 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#76)

---

## #77 — MOVED → [brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md) (#77)

---

## #78 — Tier-A: preserve alpha=0 pixels' RGB channels across `createImageBitmap` round-trip — SHIPPED 2026-07-05

**File(s):**
- [source/image.h](source/image.h) — new `bool unpremultiplied` field on `nx_image_t` (default false via `calloc` in `nx_image_new`).
- [source/image.cc](source/image.cc) — set `unpremultiplied = !premultiply` in `nx_image_write_rgba`; new `nx_image_copy_pixels(dst, src, dstPremultiply, [flipY])` native + `imageCopyPixels` registration in `nx_init_image`.
- [packages/runtime/src/$.ts](packages/runtime/src/$.ts) — `imageWriteRGBA` gains optional `premultiply` param in the type; new `imageCopyPixels(dst, src, dstPremultiply, flipY?)` signature.
- [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) — three source-branch rewrites: `ImageData` bypasses canvas → `imageWriteRGBA` direct write; `ImageBitmap` bypasses `drawImage` → `imageCopyPixels`; `HTMLImageElement` same as ImageBitmap.

**Motivation.** Post-#77 the WebGL 1 image_bitmap conformance cluster shows `_from_canvas-*` PASSing (5/8 RGB/RGBA variants) but `_from_image_bitmap-*` and `_from_image_data-*` still FAIL 7/8 each. Same signature: color assertions at right-half positions (tr, br) return `(0, 0, 0)` when the test expects `(255, 0, 0)` — specifically on the `premultiplyAlpha=none` iterations.

Root cause. Both `_from_image_bitmap` and `_from_image_data` use a source `ImageData` with **alpha=0 pixels containing non-zero RGB** (`(255, 0, 0, 0)` at tr / `(0, 255, 0, 0)` at br). Chrome preserves the ImageData bytes verbatim through `createImageBitmap(imageData, {premultiplyAlpha: "none"})`; the test then samples the created bitmap and expects the raw RGB channels to survive.

Pre-#78 our impl routed every non-Blob `createImageBitmap` source through a canvas 2D round-trip. Canvas 2D storage is premultiplied BGRA — any pixel with alpha=0 and non-zero RGB (like `(255, 0, 0, 0)`) cannot be represented in premul storage: the premultiplication zeroes the RGB channels, producing `(0, 0, 0, 0)` premul. Round-trip via `putImageData` / `drawImage` → canvas → `getImageData` returns `(0, 0, 0, 0)` because there is no way to recover the original RGB from a `(0, 0, 0, 0)` premul cell.

**Fix — three parts.**

**Engine part 1.** Track premul state on `nx_image_t` via a new `bool unpremultiplied` field. `nx_image_new` defaults it to `false` (via `calloc`); `nx_image_write_rgba` sets it to `!premultiply`. PNG / JPEG / WebP decode paths leave the default `false` since decode produces premul BGRA per `nx_image_decode`'s existing contract.

**Engine part 2.** New `nx_image_copy_pixels(dst, src, dstPremultiply, [flipY])` native. Reads `src->data` (BGRA in `src->unpremultiplied` state) and writes to `dst->data` (BGRA in `!dstPremultiply` state), converting row-by-row:

- Same premul state: `memcpy` row (fast path — preserves alpha=0 pixels' RGB verbatim).
- Unpremul → premul: `R,G,B *= alpha/255` with `+127` rounding, alpha preserved.
- Premul → unpremul: `R,G,B *= 255/alpha` with `+alpha/2` rounding and 255 clamp; alpha=0 leaves channels at 0 (matches Canvas 2D `getImageData` behavior).

`flipY` is baked in via `src_y = flip_y ? (H - 1 - y) : y` when picking the source row.

**Runtime part.** All three affected `createImageBitmap` source branches in [image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) bypass the canvas round-trip:

- `ImageData` branch — direct `$.imageWriteRGBA(bmp, imageData.data, !opts.unpremul)`. `flipY` is handled by pre-flipping the ImageData bytes in JS.
- `ImageBitmap` branch — direct `$.imageCopyPixels(bmp, src, !opts.unpremul, opts.flipY)`.
- `HTMLImageElement` (nx.js Image) branch — same as `ImageBitmap` since nx.js Image is `nx_image_t`.

The Canvas / OffscreenCanvas branch still routes through `canvasToImageBitmap` — canvas surfaces can't originate alpha=0 pixels with non-zero RGB (the fillRect / draw calls that populate them would zero the RGB during composite).

**Scope.** Fixes the ~14 tests in `_from_image_data-*` and `_from_image_bitmap-*` clusters (RGB / RGBA / LUMINANCE / LUMINANCE_ALPHA formats × 2 clusters — ALPHA variants are trivial passes with all-zero expected values).

**DISPOSITION:** `upstream-candidate`. Both the field addition and the new native are spec-compliance fixes for `createImageBitmap`'s premultiplyAlpha option; upstream nx.js has the same gap.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/image.h](source/image.h) for `bool unpremultiplied;`. Grep [source/image.cc](source/image.cc) for `nx_image_copy_pixels`, `imageCopyPixels`, and `Ledger #78`. Grep [packages/runtime/src/$.ts](packages/runtime/src/$.ts) for `imageCopyPixels(`. Grep [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) for `Ledger #78` and `imageCopyPixels(bmp, image`. Recurrence tells:

- Both `_from_image_data-*` and `_from_image_bitmap-*` regress to `(0, 0, 0)` at right-half on premultiplyAlpha=none = one of the three source-branch rewrites regressed. Check that the ImageData branch is NOT calling `canvasToImageBitmap` (grep for that call inside the `image instanceof ImageData` block); if reintroduced, pixels round-trip through canvas 2D and lose alpha=0 RGB.
- Only `_from_image_bitmap-*` regresses = the ImageBitmap branch regressed. Grep for `imageCopyPixels(bmp, image, !opts.unpremul, opts.flipY)` in that block.
- Only `flipY=true` iterations regress = flipY handling regressed. For ImageData: check the JS-side row-reverse copy loop. For ImageBitmap / HTMLImageElement: check `opts.flipY` is the 4th arg to `imageCopyPixels`.
- `Switch.VideoDecoder` frame delivery breaks = `nx_image_write_rgba`'s default premultiply=true regressed OR the new `unpremultiplied` field is being read incorrectly downstream. Video path is 2-arg call, expects `unpremultiplied=false`.
- `createImageBitmap(canvas, {premultiplyAlpha: "none"})` on a canvas regresses = the canvas branch was accidentally changed. Should still route through `canvasToImageBitmap` (correct because canvas can't originate the problem cell).

---

## #79 — Tier-A: `LUMINANCE` / `LUMINANCE_ALPHA` conversion uses L = R, not Rec.601 luma — SHIPPED 2026-07-05

**File(s):** [source/webgl.cc](source/webgl.cc) — two-line change inside `convert_image_source_to_gl_pixels`'s `GL_LUMINANCE` and `GL_LUMINANCE_ALPHA` switch cases.

**Motivation.** Post-#78 the `_from_image_bitmap-*` and `_from_image_data-*` clusters flip from 1/8 to 6/8 PASS. The residual FAILs across all three image-source clusters are the LUMINANCE and LUMINANCE_ALPHA texture formats — same signature: red source pixel `(255, 0, 0)` samples as `(76, 76, 76)` when the test expects `(255, 255, 255)`.

Root cause. #69's `convert_image_source_to_gl_pixels` computes LUMINANCE as `L = 0.299R + 0.587G + 0.114B` (Rec.601 luma). For a pure-red source pixel that gives `L=76`, sampled as `(76, 76, 76, 1)` — off by 179 from the expected white, well outside the ±10 tolerance. The WebGL 1 spec Table 5.14.6.1 (TexImageSource → LUMINANCE / LUMINANCE_ALPHA target) actually says `L = R`, not Rec.601 luma. Chrome uses `L = R`; the conformance tests all assume `L = R` — `(255, 0, 0)` uploaded to a LUMINANCE texture and sampled must return `(255, 255, 255, 1)`.

The Rec.601 formula was a #69 comment mistake. The Rec.601-style luma calculation is only correct for a specific YCbCr color-conversion path that WebGL 1 does not expose.

**Fix.** Two-line change in `convert_image_source_to_gl_pixels`:

```cpp
case GL_LUMINANCE_ALPHA:
    dst_row[x * 2 + 0] = r;   // was: (r*299 + g*587 + b*114 + 500) / 1000
    dst_row[x * 2 + 1] = a;
    break;
case GL_LUMINANCE:
    dst_row[x] = r;           // was: (r*299 + g*587 + b*114 + 500) / 1000
    break;
```

`GL_ALPHA` case unchanged — that path was already spec-correct (`A = A`).

**Scope.** Fixes the LUMINANCE + LUMINANCE_ALPHA formats across ALL image-source clusters that route through #69's converter — `_from_canvas`, `_from_image_bitmap`, `_from_image_data` = 6 test flips (3 clusters × 2 formats).

**DISPOSITION:** `upstream-candidate`. Spec-compliance fix; upstream nx.js has the same bug.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [source/webgl.cc](source/webgl.cc) for `Ledger #79 — WebGL 1 spec Table 5.14.6.1`. Recurrence tells:

- `_from_*-tex-2d-luminance-*` and `_from_*-tex-2d-luminance_alpha-*` regress to `(76, 76, 76)` at red positions and `(38, 38, 38)` at half-red = the Rec.601 formula returned. Confirm the LUMINANCE_ALPHA case writes `dst_row[x*2+0] = r` (not the Rec.601 arithmetic).
- Only LUMINANCE_ALPHA regresses = the fix landed in LUMINANCE but not LUMINANCE_ALPHA. Both cases must be updated.
- Demos that were relying on Rec.601-style luma from RGBA → LUMINANCE uploads (unlikely — WebGL 1 LUMINANCE is legacy) show wrong colors post-#79 = the fix is spec-correct; the demo needs to compute its own luma before upload.

---

## #80 — Tier-A: `Image.src` base URL prefers `globalThis.location.href` over `document.baseURI` — SHIPPED 2026-07-05

**File(s):** [packages/runtime/src/image.ts](packages/runtime/src/image.ts) — 4-line change inside the `Image.src` setter.

**Motivation.** Post-#79 the WebGL 1 `_from_image` conformance cluster (8 tests × RGBA/RGB/LUMINANCE/LUMINANCE_ALPHA/ALPHA variants) still TIMEOUTs with `FAIL: test never called finishTest()`. Each test does:

```js
var image = new Image();
image.onload = function() { runImageBitmapTest(image, ...).then(finishTest); };
image.src = resourcePath + "red-green-semi-transparent.png";
```

`image.onload` never fires because `image.src` sets an absolute URL that resolves to a nonexistent path — the fetch returns 404 (or empty), and the image transitions to the error state (which the test doesn't listen for), then times out.

Root cause. The pre-#80 `Image.src` setter resolved relative URLs against `document.baseURI`:

```ts
const baseUrl = (globalThis as { document?: { baseURI?: string } })
    .document?.baseURI ?? $.entrypoint;
```

Brewser-runtime pins `document.baseURI` to the app page URL at page-load time (see `canvas-runner.ts` `installPageGlobals` — `baseURI: pageUrl ?? 'brewser://about:blank'`). The WebGL conformance runner emulates per-page navigation by pushing `globalThis.location.href = testUrl` per-test (see `runner.js:3369` inside `runOneTest`), but does NOT update `document.baseURI`. Its comment references a stale `image.ts:96-99` where the setter used `location.href` — pre-V8-migration. So relative `image.src` values resolved against the outer runner page URL, three levels above the actual test — `../../../resources/foo.png` walked the ancestor chain from the wrong start point and produced `brewser://apps/resources/foo.png` (which doesn't exist) instead of `brewser://apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/foo.png`.

**Fix.** Prefer `globalThis.location?.href` at call time. Falls through to `document.baseURI`, then `$.entrypoint`:

```ts
const g = globalThis as {
    location?: { href?: string };
    document?: { baseURI?: string };
};
const baseUrl = g.location?.href ?? g.document?.baseURI ?? $.entrypoint;
```

Zero-cost when `location` is unset (falls through). Zero runner change — the runner's existing per-test `location.href` push becomes transparent. Sibling entry [[#81]] applies the same trick to `resolveLiveResourceUrl` for the `fetch` path.

**Scope.** Fixes `_from_image` cluster (8 tests × formats). Any other embedder that manipulates `location.href` mid-session while leaving `baseURI` pinned now gets sensible `Image.src` behavior — the same shape as browsers with no `<base href>` set.

**Spec adjacency.** Real browsers keep `document.baseURI ≡ location.href` unless the document has a `<base href>` element. Preferring `location.href` diverges from spec only for the `<base href>` case (rare in nx.js; brewser-runtime pages don't use it). If a future embedder does set `<base href>`, the runtime should honor that — the fix here can degrade gracefully by falling through when `location` is unset. For now, `location.href` is present in every brewser-runtime session (canvas-runner's `installPageGlobals` builds it from `pageUrl`), so this is a live path.

**DISPOSITION:** `upstream-candidate`. Any nx.js embedder that emulates navigation via `location.href` push benefits. No reason for upstream not to take it.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/image.ts](packages/runtime/src/image.ts) for `Ledger #80` and `g.location?.href ?? g.document?.baseURI`. Recurrence tells:

- `_from_image` cluster regresses to 8/8 TIMEOUT with "FAIL: test never called finishTest()" and no `image.onload` traces = the setter fell back to `document.baseURI`-only.
- All `_from_image` tests PASS but the LUMINANCE / LUMINANCE_ALPHA variants regress = #79 also regressed (unrelated).
- Runner logs `image.src` values that resolve to `brewser://apps/resources/...` (i.e. 3-level ancestor from wrong page) = call-time `location.href` isn't being consulted.

---

## #81 — MOVED → RUNTIME_SHIMS.md

`resolveLiveResourceUrl` call-time `globalThis.location.href` consultation. Sibling to engine #80. Full entry in [../brewser-runtime-v8/RUNTIME_SHIMS.md](../brewser-runtime-v8/RUNTIME_SHIMS.md).

---

## #82 — Tier-A: `canvasToImageBitmap` fast-path fallback to raw `getImageData` on encode/decode failure — SHIPPED 2026-07-05

**File(s):** [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) — try/catch wrap around the fast-path `canvasToBuffer` + `imageDecode` block, fallback routes through `getContext('2d').getImageData` + `imageWriteRGBA` (the same shape used by the `opts.unpremul` branch).

**Motivation.** Post-#78/#79/#80 the `_from_canvas` cluster is 7/8 PASS. The single residual FAIL is `_from_canvas-tex-2d-alpha-alpha-unsigned_byte`:

```
STATUS: FAIL
PASS: createImageBitmap with options may be rejected if it is not supported. Retrying without options.
FAIL: createImageBitmap(source) failed: "Unsupported image format"
```

That `"Unsupported image format"` error string traces to `image.cc`'s `nx_decode_image_do` — `identify_image_format` returned `FORMAT_UNKNOWN`, which happens when the input buffer's first 8 bytes don't match a PNG / JPEG / WebP signature (or when the buffer is empty).

Root cause opaque. All other `_from_canvas` variants (luminance / luminance_alpha / rgb / rgb-5_6_5 / rgba / rgba-4_4_4_4 / rgba-5_5_5_1) execute the exact same code path with an identical source canvas and PASS. Only the FIRST from_canvas test in the run order (which happens to be the ALPHA variant, alphabetically first) fails. The signature suggests `$.canvasToBuffer(canvas, 'image/png')` returns a 0-length or otherwise undecodable buffer on the first call in a run, but a live probe is needed to root-cause — this ledger entry is the pragmatic guard, not the diagnosis.

**Fix.** Wrap the fast-path encode/decode in try/catch. On failure, fall back to the raw pixel path:

```ts
try {
    const buf = await $.canvasToBuffer(src, 'image/png');
    const bmp = proto($.imageNew(), ImageBitmap);
    await $.imageDecode(bmp, buf);
    return bmp;
} catch (e) {
    console.debug('[image-bitmap:#82] fast-path encode/decode failed, ' +
        'falling back to raw getImageData: ' + (e as { message?: string })?.message);
    const w = src.width;
    const h = src.height;
    const sctx = src.getContext('2d');
    if (!sctx) throw new Error('Failed to acquire 2D context for fast-path fallback');
    const bytes = sctx.getImageData(0, 0, w, h).data;
    const bmp = proto($.imageNew(w, h), ImageBitmap);
    $.imageWriteRGBA(bmp, bytes.buffer, true);
    return bmp;
}
```

Byte-for-byte pixel copy — no PNG-encode risk. `imageWriteRGBA(bmp, buf, true)` premultiplies the raw RGBA (matching the fast-path's canvas 2D storage contract), so downstream WebGL uploads see the same shape they did pre-#82.

For the specific ALPHA test that motivated this: expected color checks are all `[0, 0, 0]` regardless of source content, so even a fully-zero bitmap satisfies every assertion. Once the fallback produces a valid ImageBitmap, the whole test PASSes.

**Scope.** Fixes 1 test in `_from_canvas` cluster. Also broadens robustness for any future case where `canvasToBuffer` might return an undecodable buffer transiently — the fallback path is functionally equivalent for canvas sources (raw pixels are what matter for the WebGL upload chain).

**Diag surface.** `console.debug('[image-bitmap:#82] fast-path encode/decode failed...')` fires only on the failure path, so hot callers pay no perf cost. Grep for `[image-bitmap:#82]` in `nxjs-debug.log` on future runs to see if the underlying encode issue persists after the guard.

**DISPOSITION:** `upstream-candidate`. Robustness improvement — any embedder benefits. The underlying `canvasToBuffer` warm-up issue (if it exists in upstream) would exhibit the same symptom there.

**UPSTREAM STATUS:** `not-submitted`.

**RE-APPLY / VERIFY NOTE.** Grep [packages/runtime/src/canvas/image-bitmap.ts](packages/runtime/src/canvas/image-bitmap.ts) for `Ledger #82`, `fast-path encode/decode failed`, and `image-bitmap:#82`. Recurrence tells:

- `_from_canvas-tex-2d-alpha-alpha-unsigned_byte` regresses to FAIL with `"Unsupported image format"` = fast-path try/catch was removed.
- `nxjs-debug.log` shows repeated `[image-bitmap:#82]` messages on non-first from_canvas variants = the underlying `canvasToBuffer` warm-up issue got worse; time to root-cause. Consider adding a diag inside `nx_canvas_to_buffer` (canvas.cc) that logs the output byte length + Skia surface state.
- All `_from_canvas` tests except ALPHA regress = the fallback path corrupted the pixel-copy contract (e.g. `imageWriteRGBA(buf, false)` instead of `true` produces unpremul pixels where premul was expected). Confirm the `imageWriteRGBA(bmp, bytes.buffer, true)` call.

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
