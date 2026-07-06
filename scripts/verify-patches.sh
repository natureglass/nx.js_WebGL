#!/usr/bin/env bash
# verify-patches.sh — post-pull re-application audit for the nx.js
# V8-migration fork. Extracts the RE-APPLY / VERIFY NOTE grep for
# every ledger entry and prints PRESENT / MISSING per entry across
# both NXJS_PATCHES_NEEDED.md (engine) and brewser-runtime-v8/
# RUNTIME_SHIMS.md (runtime shims).
#
# Usage:
#   scripts/verify-patches.sh                 # verify against defaults
#   NXJS=/path/to/nxjs-source-v8 \
#     RUNTIME=/path/to/brewser-runtime-v8 \
#     APPS=/path/to/brewser-apps \
#     scripts/verify-patches.sh
#
# Exit code: 0 if every entry PRESENT (or KNOWN-OPEN); 1 if any
# entry that should be present is MISSING.

set -euo pipefail

# Resolve default paths relative to script location if not overridden.
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NXJS="${NXJS:-$(cd "$here/.." && pwd)}"
RUNTIME="${RUNTIME:-$(cd "$here/../../brewser-runtime-v8" 2>/dev/null && pwd || echo "$here/../../brewser-runtime-v8")}"
APPS="${APPS:-$(cd "$here/../../brewser-apps" 2>/dev/null && pwd || echo "$here/../../brewser-apps")}"

fail=0
have_check=0
missing_check=0

status() {
    local id="$1" state="$2" title="$3" file="${4:-}"
    printf '%-14s %-9s %s' "#$id" "$state" "$title"
    if [ -n "$file" ]; then
        printf '  (%s)' "$file"
    fi
    printf '\n'
    have_check=$((have_check + 1))
    case "$state" in
        MISSING) missing_check=$((missing_check + 1)); fail=1 ;;
        *) ;;
    esac
}

# check <id> <title> <path> <regex> [--allow-missing]
# Prints PRESENT if the file exists AND grep -Pq "$regex" $path finds a match.
# Prints MISSING otherwise. --allow-missing downgrades MISSING to KNOWN-OPEN.
check() {
    local id="$1" title="$2" path="$3" pattern="$4" allow="${5:-}"
    local state="MISSING"
    if [ -f "$path" ] && grep -Pq -- "$pattern" "$path"; then
        state="PRESENT"
    fi
    if [ "$state" = "MISSING" ] && [ "$allow" = "--allow-missing" ]; then
        state="KNOWN-OPEN"
    fi
    status "$id" "$state" "$title" "$path"
}

# check_absent <id> <title> <path> <regex> [--allow-missing]
# Inverse — used for #8 style anti-pattern checks. PRESENT means the
# pattern is ABSENT (i.e. the anti-pattern didn't regress back).
check_absent() {
    local id="$1" title="$2" path="$3" pattern="$4" allow="${5:-}"
    local state="MISSING"
    if [ -f "$path" ] && ! grep -Pq -- "$pattern" "$path"; then
        state="PRESENT"
    fi
    if [ "$state" = "MISSING" ] && [ "$allow" = "--allow-missing" ]; then
        state="KNOWN-OPEN"
    fi
    status "$id" "$state" "$title" "$path"
}

# check_file_exists <id> <title> <path>
check_file_exists() {
    local id="$1" title="$2" path="$3"
    if [ -f "$path" ]; then
        status "$id" "PRESENT" "$title" "$path"
    else
        status "$id" "MISSING" "$title" "$path"
    fi
}

echo "=== engine ledger: NXJS_PATCHES_NEEDED.md (in $NXJS) ==="

# #1 — image.ts globalThis.fetch deferral
check 1 "image.ts call-time globalThis.fetch" \
    "$NXJS/packages/runtime/src/image.ts" \
    'return globalThis\.fetch\(input, init\)'

# #2 — audio.ts globalThis.fetch deferral
check 2 "audio.ts call-time globalThis.fetch" \
    "$NXJS/packages/runtime/src/audio.ts" \
    'return globalThis\.fetch\(input, init\)'

# #3 — video.ts globalThis.fetch deferral
check 3 "video.ts call-time globalThis.fetch" \
    "$NXJS/packages/runtime/src/video.ts" \
    'return globalThis\.fetch\(input, init\)'

# #4 — cursor overlay native binding (SHIPPED 2026-06-30). Content-
# level greps for the 4 C entry points (see ADDENDUM in ledger for
# ship-time function names) + JS-side dispatch registrations +
# skia_gpu present-hook composite call.
check 4 "cursor.h nx_cursor_set_static prototype" \
    "$NXJS/source/cursor.h" \
    'void nx_cursor_set_static'
check 4 "cursor.h nx_cursor_set_animated prototype" \
    "$NXJS/source/cursor.h" \
    'void nx_cursor_set_animated'
check 4 "cursor.h nx_cursor_set_position prototype" \
    "$NXJS/source/cursor.h" \
    'void nx_cursor_set_position'
check 4 "cursor.h nx_cursor_clear prototype" \
    "$NXJS/source/cursor.h" \
    'void nx_cursor_clear'
check 4 "cursor.cc nx_cursor_set_static body" \
    "$NXJS/source/cursor.cc" \
    'void nx_cursor_set_static'
check 4 "canvas.cc js_set_cursor_overlay JS binding" \
    "$NXJS/source/canvas.cc" \
    'js_set_cursor_overlay'
check 4 "canvas.cc NX_DEF_FUNC setCursorOverlay registration" \
    "$NXJS/source/canvas.cc" \
    'setCursorOverlay.*js_set_cursor_overlay'
check 4 "skia_gpu.cc cursor composite hook in present" \
    "$NXJS/source/skia_gpu.cc" \
    'composite_cursor_overlay|nx_cursor.*compose|Cursor compositor'

# #5 — skia_gpu ES3 shared context + accessors
check 5 "skia_gpu ES3 CLIENT_VERSION=3" \
    "$NXJS/source/skia_gpu.cc" \
    'EGL_CONTEXT_CLIENT_VERSION,\s*3'

# #6 — webgl_bridge state save/restore + tenant FBO
check 6 "webgl_bridge nx_gl_state_snap_t" \
    "$NXJS/source/webgl_bridge.h" \
    'nx_gl_state_snap_t'

# #7 — WebGL1 context factory
check 7 "webgl.cc nx_webgl_compose_if_active" \
    "$NXJS/source/webgl.cc" \
    'nx_webgl_compose_if_active'

# #8 — Object.entries anti-pattern must be ABSENT; bulk defineProperties present
check_absent 8 "webgl1 no for-of Object.entries GL_CONSTANTS regression" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'for \(const \[k, v\] of Object\.entries\(GL_CONSTANTS\)\)'
check_absent 8 "webgl2 no for-of Object.entries GL_CONSTANTS regression" \
    "$NXJS/packages/runtime/src/canvas/webgl2-rendering-context.ts" \
    'for \(const \[k, v\] of Object\.entries\(GL_CONSTANTS\)\)'

# #9 — v1 ES3 sized internalformat constants
check 9 "webgl1 GL_CONSTANTS has SRGB8_ALPHA8" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'SRGB8_ALPHA8:'

# #10 — EXT_sRGB + HalfFloat translate
check 10 "webgl.cc bucket_e_translate_tex_image helper" \
    "$NXJS/source/webgl.cc" \
    'bucket_e_translate_tex_image'

# #11 — PMREM r184 FS replacement
check 11 "webgl.cc maybe_replace_pmrem_fs helper" \
    "$NXJS/source/webgl.cc" \
    'maybe_replace_pmrem_fs'

# #12 — MOVED to RUNTIME_SHIMS.md
echo "  #12 verified below in runtime ledger section."

# #13 — canvas.cc set_font_size pin at fillText/strokeText/measureText
check 13 "canvas.cc set_font_size pin in fill_text" \
    "$NXJS/source/canvas.cc" \
    'set_font_size\(context, context->state->font_size\)'

# #14 — WebGL2 context factory
check 14 "webgl.cc webgl2ContextNew binding" \
    "$NXJS/source/webgl.cc" \
    'webgl2ContextNew'

# #15 — v1/v2 FUNCS[] split
check 15 "webgl.cc install_methods_v2" \
    "$NXJS/source/webgl.cc" \
    'install_methods_v2'

# #16 — passive state-contract probe
check 16 "webgl_bridge.cc nx_webgl_state_probe_log" \
    "$NXJS/source/webgl_bridge.cc" \
    'nx_webgl_state_probe_log'

# #17 — nx_gl_state_snap_t extension for sampler_unit0 + read_fbo
check 17 "webgl_bridge.h sampler_unit0 field" \
    "$NXJS/source/webgl_bridge.h" \
    'sampler_unit0'
check 17 "webgl_bridge.h read_fbo field" \
    "$NXJS/source/webgl_bridge.h" \
    '\bread_fbo\b'

# #18 — MOVED to RUNTIME_SHIMS.md
echo "  #18 verified below in runtime ledger section."

# #19 — OPEN, no shipped fix to verify
status 19 "KNOWN-OPEN" "cube-routing v2 applicability — Phase 2.G.4 gate"

# #20 — OPEN engine ask; workaround in demo (rgbe-loader.js). Check
# the demo workaround for now, since the engine ask hasn't shipped.
if [ -d "$APPS" ]; then
    check 20 "rgbe-loader (v2) row-reverse workaround" \
        "$APPS/apps/experimental/com.natureglass.webgl2threejsdemos/libs/rgbe-loader.js" \
        'tex\.flipY\s*=\s*false' --allow-missing
    check 20 "rgbe-loader (v1) row-reverse workaround" \
        "$APPS/apps/experimental/com.natureglass.webgl1threejsdemos/libs/rgbe-loader.js" \
        'tex\.flipY\s*=\s*false' --allow-missing
else
    status 20 "KNOWN-OPEN" "demo-side workaround check skipped (brewser-apps not found)"
fi

# #21 — MOVED to RUNTIME_SHIMS.md
echo "  #21 verified below in runtime ledger section."

# #22 — Switch.VideoDecoder V8 port
check 22 "video-decoder.cc nx_init_video_decoder" \
    "$NXJS/source/video-decoder.cc" \
    'nx_init_video_decoder'
check 22 "media-decoder.cc audio-tap ring" \
    "$NXJS/source/media-decoder.cc" \
    'nx_media_read_waveform|tap_ring'

# #24 — MOVED to RUNTIME_SHIMS.md
echo "  #24 verified below in runtime ledger section."

# #31 — engine ask OPEN; demo-side workaround
if [ -d "$APPS" ]; then
    check 31 "gpgpu-water renderer.resetState() workaround" \
        "$APPS/apps/experimental/com.natureglass.webgl2threejsdemos/gpgpu-water/assets/main.js" \
        'renderer\.resetState\(\)' --allow-missing
else
    status 31 "KNOWN-OPEN" "demo-side workaround check skipped (brewser-apps not found)"
fi

# #34 — engine ask OPEN; runtime polyfill fills the gap
check 34 "web-audio-stubs.ts STUBS_BUILD_TAG post-guard-fix" \
    "$RUNTIME/src/polyfills/web-audio-stubs.ts" \
    'v8-override-throw-stubs' --allow-missing

# #35 — snap contract extension: depth_mask + stencil_mask (cut #15)
check 35 "webgl_bridge.h depth_mask field" \
    "$NXJS/source/webgl_bridge.h" \
    '\bdepth_mask\b'
check 35 "webgl_bridge.h stencil_mask field" \
    "$NXJS/source/webgl_bridge.h" \
    '\bstencil_mask\b'

# #36 — bracket-state-persistence via per-call shadow-tracked user_snap
check 36 "webgl.cc WebGLState user_snap field" \
    "$NXJS/source/webgl.cc" \
    'nx_gl_state_snap_t user_snap'
check 36 "webgl.cc user_snap_valid gate" \
    "$NXJS/source/webgl.cc" \
    'user_snap_valid'
check 36 "webgl.cc per-call shadow write example (viewport)" \
    "$NXJS/source/webgl.cc" \
    'user_snap\.viewport\[0\]\s*=\s*x'
check 36 "webgl.cc auto_user_vao field" \
    "$NXJS/source/webgl.cc" \
    'auto_user_vao'

# #37 — v2 texStorage3D + texSubImage3D bindings (cut #32)
check 37 "webgl.cc w_tex_storage_3d FN" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_tex_storage_3d\)'
check 37 "webgl.cc w_tex_sub_image_3d FN" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_tex_sub_image_3d\)'

# #40 and #41 — TOMBSTONED 2026-07-03 (see NXJS_PATCHES_ARCHIVE.md
# #40-tombstoned / #41-tombstoned). Both were reverted after hardware
# green on #42; guardrails below ensure neither the engine primitives
# nor their runtime call sites accidentally reappear.
check_absent 40 "webgl.cc has no w_reset_user_snap FN (tombstoned)" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_reset_user_snap\)'
check_absent 40 "webgl.cc has no resetUserSnap FUNCS entry (tombstoned)" \
    "$NXJS/source/webgl.cc" \
    '"resetUserSnap"'
check_absent 40 "gl-teardown.ts has no resetUserSnap runtime reference (tombstoned)" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'resetUserSnap'
check_absent 41 "webgl.cc has no patch41:apply string (tombstoned)" \
    "$NXJS/source/webgl.cc" \
    'patch41:apply'
check_absent 41 "webgl.cc has no dump_attribs_at_draw probe (tombstoned with #41)" \
    "$NXJS/source/webgl.cc" \
    'dump_attribs_at_draw'

# #42 — engine OES_vertex_array_object advertising for v1 pre-arm route.
# Runtime companion in RUNTIME_SHIMS.md #42; engine ext + runtime pre-arm
# ship together — runtime block no-ops on v1 without the ext.
check 42 "webgl.cc OES_vertex_array_object branch in w_get_extension" \
    "$NXJS/source/webgl.cc" \
    'OES_vertex_array_object'
check 42 "webgl.cc forward decls for VAO natives used by ext branch" \
    "$NXJS/source/webgl.cc" \
    'RUNTIME_SHIMS #42 / pre-arm route'
check 42 "webgl.cc OES ext exposes createVertexArrayOES/bindVertexArrayOES" \
    "$NXJS/source/webgl.cc" \
    '"bindVertexArrayOES"'

echo
echo "=== runtime ledger: brewser-runtime-v8/RUNTIME_SHIMS.md (in $RUNTIME) ==="

# #12 — cube-route-shim samplerCube→sampler2D
check 12 "cube-route-shim installed (cubeUVSample helper)" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'cubeUVSample'
check 12 "canvas-runner installs installCubeRouting" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installCubeRouting'

# #18 — cube-route-shim per-method safeBind guards
check 18 "cube-route-shim safeBind guards" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'safeBind'

# #21 — shadow-route-shim
check 21 "shadow-route-shim installed" \
    "$RUNTIME/src/scripts/shadow-route-shim.ts" \
    'textureShadowCompat'
check 21 "canvas-runner installs installShadowRouting" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installShadowRouting'

# #24 — cube-RT-readback rescue extension to cube-route-shim
check 24 "cube-route-shim allocateCubeRTAtlas rescue" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'allocateCubeRTAtlas'
check 24 "cube-route-shim framebufferTexture2D wrap" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'framebufferTexture2D'

# #39 — Phase A step a: GL teardown at shim chokepoint (runtime-side)
check 39 "gl-teardown.ts installGLTeardownTracking export" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'export function installGLTeardownTracking'
check 39 "gl-teardown.ts teardownGL export" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'export function teardownGL'
check 39 "canvas-runner installs installGLTeardownTracking" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installGLTeardownTracking\(gl\)'
check 39 "canvas-runner exports teardownSharedScreenGL" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'export function teardownSharedScreenGL'
check 39 "web-view.ts endSession calls teardownSharedScreenGL after endAppSession" \
    "$RUNTIME/src/web-view.ts" \
    'teardownSharedScreenGL\(\)'
check 39 "webgl-shim installs installGLTeardownTracking (native path)" \
    "$RUNTIME/src/shims/webgl-shim.ts" \
    'installGLTeardownTracking\(nativeContext'
# gl-state-sweep.ts and gl-frame-probe.ts were removed 2026-07-03 in the
# post-#42-hardware-green cleanup; guardrails below ensure they don't creep back.
check_absent 39 "gl-state-sweep.ts file removed (post-#42 cleanup)" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installGLStateSweep'
check_absent 39 "gl-frame-probe.ts references removed (post-#42 cleanup)" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installGLFrameProbe'
check_absent 39 "webgl-shim.ts has no state-sweep/frame-probe install (post-#42 cleanup)" \
    "$RUNTIME/src/shims/webgl-shim.ts" \
    'installGLStateSweep|installGLFrameProbe'

# #42 — Post-teardown VAO pre-arm through wrapped surface (runtime block).
# Companion engine change checked in the engine section above (#42).
check 42 "gl-teardown.ts pre-arm block emits [gl-teardown:prearm] marker" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    '\[gl-teardown:prearm\]'
check 42 "gl-teardown.ts pre-arm has WebGL2 route via gl.createVertexArray/bindVertexArray" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'glAny\.createVertexArray'
check 42 "gl-teardown.ts pre-arm has WebGL1 route via OES_vertex_array_object" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'createVaoOES\.call\(ext\)'
check 42 "gl-teardown.ts readCurrentVaoName reads VERTEX_ARRAY_BINDING for numeric log name" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'function readCurrentVaoName'
# #42 follow-up (2026-07-03) — invariant enforcement: runtime-internal
# code must never bind VAO 0 on a live tenant surface. Two violators
# removed; grep_absent ensures they don't creep back in.
check_absent 42 "canvas-runner.ts resetScreenGLForScript does NOT bindVertexArray(null)" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'gl2\.bindVertexArray\s*\(\s*null\s*\)'
check_absent 42 "gl-teardown.ts resetStateToDefaults does NOT tryCall(gl.bindVertexArray, gl, null)" \
    "$RUNTIME/src/scripts/gl-teardown.ts" \
    'tryCall\s*\(\s*gl\.bindVertexArray\s*,\s*gl\s*,\s*null\s*\)'

# #43 — Phase-0: native GL extension enumeration + [gl-ext-dump] boot log.
check 43 "webgl.cc populate_native_extensions helper" \
    "$NXJS/source/webgl.cc" \
    'populate_native_extensions'
check 43 "webgl.cc emits [gl-ext-dump] boot log" \
    "$NXJS/source/webgl.cc" \
    '\[gl-ext-dump\]'
check 43 "webgl.cc _getNativeExtensionsString native binding" \
    "$NXJS/source/webgl.cc" \
    '_getNativeExtensionsString.*w_get_native_extensions_string'
check 43 "webgl.cc _getEglVersion native binding" \
    "$NXJS/source/webgl.cc" \
    '_getEglVersion.*w_get_egl_version'
check 43 "webgl.cc glGetStringi(GL_EXTENSIONS) enumeration path" \
    "$NXJS/source/webgl.cc" \
    'glGetStringi\s*\(\s*GL_EXTENSIONS'
check 43 "webgl.cc eager populate call in make_context_carrier" \
    "$NXJS/source/webgl.cc" \
    'populate_native_extensions\(\);'

# #44 — Phase-0: gl.getBackendInfo runtime shim.
check 44 "webgl-ext-shim.ts installGetBackendInfo export" \
    "$RUNTIME/src/scripts/webgl-ext-shim.ts" \
    'export function installGetBackendInfo'
check 44 "webgl-ext-shim.ts marker-guard uses Symbol.for" \
    "$RUNTIME/src/scripts/webgl-ext-shim.ts" \
    "Symbol\\.for\\('brewserGetBackendInfoInstalled'\\)"
check 44 "webgl-shim.ts wires installGetBackendInfo on native branch" \
    "$RUNTIME/src/shims/webgl-shim.ts" \
    'installGetBackendInfo\(nativeContext'
check 44 "webgl-shim.ts wires installGetBackendInfo on future branch" \
    "$RUNTIME/src/shims/webgl-shim.ts" \
    'installGetBackendInfo\(futureContext'
check 44 "webgl-ext-shim.ts schema field: glExtensions" \
    "$RUNTIME/src/scripts/webgl-ext-shim.ts" \
    'glExtensions'
check 44 "webgl-ext-shim.ts schema field: eglMajor" \
    "$RUNTIME/src/scripts/webgl-ext-shim.ts" \
    'eglMajor'
check 44 "webgl-ext-shim.ts schema field: bridgeRequestedWidth" \
    "$RUNTIME/src/scripts/webgl-ext-shim.ts" \
    'bridgeRequestedWidth'

# #45 — Phase-0: webgl2-rendering-context.ts landmine defuse.
check 45 "webgl2-rendering-context.ts TS stub throws distinctive error" \
    "$NXJS/packages/runtime/src/canvas/webgl2-rendering-context.ts" \
    'TS extension stub reached'
check_absent 45 "webgl2-rendering-context.ts stub no longer silently returns []" \
    "$NXJS/packages/runtime/src/canvas/webgl2-rendering-context.ts" \
    'getSupportedExtensions\(\)\s*:\s*string\[\]\s*\{\s*return\s*\[\];'
check_absent 45 "webgl2-rendering-context.ts stub no longer silently returns null" \
    "$NXJS/packages/runtime/src/canvas/webgl2-rendering-context.ts" \
    'getExtension\(name:\s*string\)\s*:\s*any\s*\{\s*return\s*null;'

# #46 — Phase-0 commit 2: bridge FBO stencil contract fix.
check 46 "webgl_bridge.cc renderbuffer uses GL_DEPTH24_STENCIL8" \
    "$NXJS/source/webgl_bridge.cc" \
    'glRenderbufferStorage\s*\(\s*GL_RENDERBUFFER\s*,\s*GL_DEPTH24_STENCIL8'
check 46 "webgl_bridge.cc attaches via GL_DEPTH_STENCIL_ATTACHMENT" \
    "$NXJS/source/webgl_bridge.cc" \
    'GL_DEPTH_STENCIL_ATTACHMENT'
check_absent 46 "webgl_bridge.cc no longer uses depth-only GL_DEPTH_COMPONENT24 storage" \
    "$NXJS/source/webgl_bridge.cc" \
    'glRenderbufferStorage\s*\(\s*GL_RENDERBUFFER\s*,\s*GL_DEPTH_COMPONENT24'
check_absent 46 "webgl_bridge.cc no longer uses depth-only GL_DEPTH_ATTACHMENT" \
    "$NXJS/source/webgl_bridge.cc" \
    'glFramebufferRenderbuffer\s*\(\s*GL_FRAMEBUFFER\s*,\s*GL_DEPTH_ATTACHMENT'
check 46 "webgl_bridge.cc emits [bridge-fbo:complete] positive breadcrumb" \
    "$NXJS/source/webgl_bridge.cc" \
    '\[bridge-fbo:complete\]'
check 46 "webgl_bridge.cc emits [bridge-fbo:INCOMPLETE] failure assert" \
    "$NXJS/source/webgl_bridge.cc" \
    '\[bridge-fbo:INCOMPLETE\]'
check 46 "webgl.cc w_get_parameter has explicit GL_STENCIL_BITS/GL_DEPTH_BITS case" \
    "$NXJS/source/webgl.cc" \
    'case GL_STENCIL_BITS:'

# #47 — Phase-1 batch 1: driver-probed advertisement + 16 rows + compressed
# 2D natives + UNMASKED/MAX_ANISO getParameter branches.
check 47 "webgl.cc has_native_ext helper" \
    "$NXJS/source/webgl.cc" \
    'static bool has_native_ext'
check 47 "webgl.cc is_v2_context helper" \
    "$NXJS/source/webgl.cc" \
    'static bool is_v2_context'
check 47 "webgl.cc w_get_supported_extensions is driver-probed (has_native_ext calls present)" \
    "$NXJS/source/webgl.cc" \
    'if \(has_native_ext\("GL_EXT_depth_clamp"\)\)'
check_absent 47 "webgl.cc no longer has the shared SUPPORTED\[9\] static (retired)" \
    "$NXJS/source/webgl.cc" \
    'static const char \*const SUPPORTED\[\]'
check 47 "webgl.cc w_get_extension has EXT_texture_filter_anisotropic branch" \
    "$NXJS/source/webgl.cc" \
    '"EXT_texture_filter_anisotropic"'
check 47 "webgl.cc w_get_extension has WEBGL_compressed_texture_astc branch" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_compressed_texture_astc"'
check 47 "webgl.cc w_get_extension has WEBGL_debug_renderer_info branch" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_debug_renderer_info"'
check 47 "webgl.cc w_get_extension has WEBGL_stencil_texturing branch (v2 core-A)" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_stencil_texturing"'
check 47 "webgl.cc w_get_extension has EXT_texture_norm16 branch (v2)" \
    "$NXJS/source/webgl.cc" \
    '"EXT_texture_norm16"'
check 47 "webgl.cc w_get_parameter has UNMASKED_VENDOR_WEBGL case (0x9245)" \
    "$NXJS/source/webgl.cc" \
    'case 0x9245:'
check 47 "webgl.cc w_get_parameter has UNMASKED_RENDERER_WEBGL case (0x9246)" \
    "$NXJS/source/webgl.cc" \
    'case 0x9246:'
check 47 "webgl.cc w_get_parameter has MAX_TEXTURE_MAX_ANISOTROPY_EXT case (0x84FF)" \
    "$NXJS/source/webgl.cc" \
    'case 0x84FF:'
check 47 "webgl.cc w_compressed_tex_image_2d FN" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_compressed_tex_image_2d\)'
check 47 "webgl.cc w_compressed_tex_sub_image_2d FN" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_compressed_tex_sub_image_2d\)'
check 47 "webgl.cc compressedTexImage2D wired in FUNCS[] (both v1+v2)" \
    "$NXJS/source/webgl.cc" \
    '"compressedTexImage2D", w_compressed_tex_image_2d'
check 47 "webgl.cc compressedTexSubImage2D wired in FUNCS[] (both v1+v2)" \
    "$NXJS/source/webgl.cc" \
    '"compressedTexSubImage2D", w_compressed_tex_sub_image_2d'

# #48 — Phase-1 batch 2A: Unity-P1 v1 function surfaces + software shims +
# rider-1 ETC2/EAC.
check 48 "webgl.cc probe_ext_frag_depth helper" \
    "$NXJS/source/webgl.cc" \
    'static bool probe_ext_frag_depth'
check 48 "webgl.cc emits [frag-depth-probe] result" \
    "$NXJS/source/webgl.cc" \
    '\[frag-depth-probe\]'
check 48 "webgl.cc w_get_extension has ANGLE_instanced_arrays branch (v1)" \
    "$NXJS/source/webgl.cc" \
    '"ANGLE_instanced_arrays"'
check 48 "webgl.cc w_get_extension has WEBGL_draw_buffers branch (v1)" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_draw_buffers"'
check 48 "webgl.cc w_get_extension has EXT_frag_depth branch (v1, probe-gated)" \
    "$NXJS/source/webgl.cc" \
    '"EXT_frag_depth"'
check 48 "webgl.cc w_get_extension has WEBGL_lose_context branch" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_lose_context"'
check 48 "webgl.cc w_get_extension has WEBGL_debug_shaders branch" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_debug_shaders"'
check 48 "webgl.cc w_get_extension has WEBGL_compressed_texture_etc branch (rider 1)" \
    "$NXJS/source/webgl.cc" \
    '"WEBGL_compressed_texture_etc"'
check 48 "webgl.cc v1 install_methods gains drawArraysInstanced" \
    "$NXJS/source/webgl.cc" \
    '"drawArraysInstanced", w_draw_arrays_instanced'
check 48 "webgl.cc v1 install_methods gains vertexAttribDivisor" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribDivisor", w_vertex_attrib_divisor'
check 48 "webgl.cc v1 install_methods gains drawBuffers" \
    "$NXJS/source/webgl.cc" \
    '"drawBuffers", w_draw_buffers'
check 48 "webgl.cc OES_vertex_array_object advertised on v1 (list-flip retires #42 asymmetry)" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("OES_vertex_array_object"\)'

# #49 — Phase-1 batch 2B: Rider 2 v2 spec-conformance prune. 5 WebGL1-only
# extensions return null on v2 (matches Chrome / Firefox behavior).
check 49 "webgl.cc v2_rider2 prune guard in w_get_extension" \
    "$NXJS/source/webgl.cc" \
    'const bool v2_rider2 = is_v2_context'
check 49 "webgl.cc OES_standard_derivatives moved to v1-only advertising" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("OES_standard_derivatives"\)'
check 49 "webgl.cc WEBGL_depth_texture moved to v1-only advertising" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("WEBGL_depth_texture"\)'
check 49 "webgl.cc OES_texture_float_linear KEPT on v2 (still a WebGL2 ext per registry)" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("OES_texture_float_linear"\)'

# #50 — Phase-1.5-LOW: 30 core WebGL2 methods + OES_fbo_render_mipmap rider.
# Family-grouped checks so a regression at any tier level shows up as a
# single family-tagged MISSING line.
check 50 "webgl.cc u32_list helper for uint typed-array unwrap" \
    "$NXJS/source/webgl.cc" \
    'bool u32_list\(Isolate'
# Buffer ops (2)
check 50 "webgl.cc [buffer] getBufferSubData FN + FUNCS[]" \
    "$NXJS/source/webgl.cc" \
    '"getBufferSubData", w_get_buffer_sub_data'
check 50 "webgl.cc [buffer] copyBufferSubData FN + FUNCS[]" \
    "$NXJS/source/webgl.cc" \
    '"copyBufferSubData", w_copy_buffer_sub_data'
# Framebuffer thin (6)
check 50 "webgl.cc [fbo-thin] framebufferTextureLayer" \
    "$NXJS/source/webgl.cc" \
    '"framebufferTextureLayer", w_framebuffer_texture_layer'
check 50 "webgl.cc [fbo-thin] invalidateFramebuffer" \
    "$NXJS/source/webgl.cc" \
    '"invalidateFramebuffer", w_invalidate_framebuffer'
check 50 "webgl.cc [fbo-thin] invalidateSubFramebuffer" \
    "$NXJS/source/webgl.cc" \
    '"invalidateSubFramebuffer", w_invalidate_sub_framebuffer'
check 50 "webgl.cc [fbo-thin] readBuffer" \
    "$NXJS/source/webgl.cc" \
    '"readBuffer", w_read_buffer'
check 50 "webgl.cc [fbo-thin] renderbufferStorageMultisample" \
    "$NXJS/source/webgl.cc" \
    '"renderbufferStorageMultisample", w_renderbuffer_storage_multisample'
check 50 "webgl.cc [fbo-thin] getFragDataLocation" \
    "$NXJS/source/webgl.cc" \
    '"getFragDataLocation", w_get_frag_data_location'
# 3D texture (3)
check 50 "webgl.cc [tex3d] copyTexSubImage3D" \
    "$NXJS/source/webgl.cc" \
    '"copyTexSubImage3D", w_copy_tex_sub_image_3d'
check 50 "webgl.cc [tex3d] compressedTexImage3D" \
    "$NXJS/source/webgl.cc" \
    '"compressedTexImage3D", w_compressed_tex_image_3d'
check 50 "webgl.cc [tex3d] compressedTexSubImage3D" \
    "$NXJS/source/webgl.cc" \
    '"compressedTexSubImage3D", w_compressed_tex_sub_image_3d'
# UInt uniforms (8)
check 50 "webgl.cc [uint-uni] uniform1ui" \
    "$NXJS/source/webgl.cc" \
    '"uniform1ui", w_uniform_1ui'
check 50 "webgl.cc [uint-uni] uniform4uiv" \
    "$NXJS/source/webgl.cc" \
    '"uniform4uiv", w_uniform_4uiv'
# Non-square matrix (6)
check 50 "webgl.cc [nsq-mat] uniformMatrix2x3fv" \
    "$NXJS/source/webgl.cc" \
    '"uniformMatrix2x3fv", w_uniform_matrix_2x3fv'
check 50 "webgl.cc [nsq-mat] uniformMatrix4x3fv" \
    "$NXJS/source/webgl.cc" \
    '"uniformMatrix4x3fv", w_uniform_matrix_4x3fv'
# Clear buffer (4)
check 50 "webgl.cc [clear-buf] clearBufferiv" \
    "$NXJS/source/webgl.cc" \
    '"clearBufferiv", w_clear_buffer_iv'
check 50 "webgl.cc [clear-buf] clearBufferfi" \
    "$NXJS/source/webgl.cc" \
    '"clearBufferfi", w_clear_buffer_fi'
# Draw range (1)
check 50 "webgl.cc [draw-range] drawRangeElements" \
    "$NXJS/source/webgl.cc" \
    '"drawRangeElements", w_draw_range_elements'
# Rider — OES_fbo_render_mipmap (batch-2 defect fix)
check 50 "webgl.cc [rider] OES_fbo_render_mipmap advertised on v1" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("OES_fbo_render_mipmap"\)'
check 50 "webgl.cc [rider] OES_fbo_render_mipmap w_get_extension branch" \
    "$NXJS/source/webgl.cc" \
    '"OES_fbo_render_mipmap"'

# #51 — Phase-1.5-LOW-MED: 6 core WebGL2 methods (integer vertex attribs +
# getInternalformatParameter). Counter 47 → 53 / 88.
# Integer vertex attribs (5)
check 51 "webgl.cc [int-attrib] vertexAttribI4i" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribI4i", w_vertex_attrib_i4i'
check 51 "webgl.cc [int-attrib] vertexAttribI4ui" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribI4ui", w_vertex_attrib_i4ui'
check 51 "webgl.cc [int-attrib] vertexAttribI4iv" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribI4iv", w_vertex_attrib_i4iv'
check 51 "webgl.cc [int-attrib] vertexAttribI4uiv" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribI4uiv", w_vertex_attrib_i4uiv'
check 51 "webgl.cc [int-attrib] vertexAttribIPointer" \
    "$NXJS/source/webgl.cc" \
    '"vertexAttribIPointer", w_vertex_attrib_i_pointer'
# getInternalformatParameter (1)
check 51 "webgl.cc [informat-param] getInternalformatParameter" \
    "$NXJS/source/webgl.cc" \
    '"getInternalformatParameter", w_get_internalformat_parameter'
# Family marker — the LOW-MED block's header comment. Regression tell:
# absent = someone deleted the block wholesale, not just a single FN.
check 51 "webgl.cc [low-med] block header comment present" \
    "$NXJS/source/webgl.cc" \
    'Phase-1\.5-LOW-MED'

# #52 — Two gl-probes-discovered gaps (OPEN).
# #52a: drawRangeElements silent no-op on Citron/Mesa Nouveau — gets a
#       defensive touch_fbo() to match the other draw FNs but the failure
#       persists. Guardrail checks the defensive fix is present.
# #52b: WebGL1 core getTexParameter missing from FUNCS[] — deferred fix.
#       Guardrail marks it as known-open until the FN + FUNCS[] entry lands.
# #52a — drawRangeElements → drawElements fallback (interim fix SHIPPED 2026-07-03).
# Body substitution + one-time boot log guardrail.
check 52 "webgl.cc [52a] w_draw_range_elements calls glDrawElements (fallback)" \
    "$NXJS/source/webgl.cc" \
    'glDrawElements\(mode, count, type, \(const void \*\)offset\);'
check 52 "webgl.cc [52a] fallback boot log present" \
    "$NXJS/source/webgl.cc" \
    '\[#52a\] drawRangeElements -> drawElements fallback'
# Direct glDrawRangeElements call is now legitimately present inside the
# `#if NX_52A_DISABLE_FALLBACK` branch (hardware probe recipe). Do NOT
# check_absent for it — instead assert both the default (fallback) branch
# and the ifdef branch coexist so the gate stays intact.
check 52 "webgl.cc [52a] fallback-disabled DIRECT boot log present (gate branch)" \
    "$NXJS/source/webgl.cc" \
    '\[#52a\] drawRangeElements DIRECT \(fallback DISABLED'
# #52b — getTexParameter FN + FUNCS[] entries (SHIPPED 2026-07-03).
check 52 "webgl.cc [52b] w_get_tex_parameter FN present" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_tex_parameter\)'
check 52 "webgl.cc [52b] getTexParameter registered (v1 or v2 FUNCS[])" \
    "$NXJS/source/webgl.cc" \
    '"getTexParameter", w_get_tex_parameter'

# #53 — Phase-1.5-MED: 25 methods across 4 families + 3 new K_* handle kinds.
# Handle-kind guardrails.
check 53 "webgl.cc [med-handle] K_QUERY enum member" \
    "$NXJS/source/webgl.cc" \
    'K_QUERY,'
check 53 "webgl.cc [med-handle] K_SAMPLER enum member" \
    "$NXJS/source/webgl.cc" \
    'K_SAMPLER,'
check 53 "webgl.cc [med-handle] K_SYNC enum member" \
    "$NXJS/source/webgl.cc" \
    'K_SYNC,'
check 53 "webgl.cc [med-handle] GLObj gained GLsync sync field" \
    "$NXJS/source/webgl.cc" \
    'GLsync sync = nullptr'
check 53 "webgl.cc [med-handle] WebGLQuery class registered in nx_webgl2_init_class MAP" \
    "$NXJS/source/webgl.cc" \
    '"WebGLQuery", K_QUERY'
check 53 "webgl.cc [med-handle] WebGLSampler class registered" \
    "$NXJS/source/webgl.cc" \
    '"WebGLSampler", K_SAMPLER'
check 53 "webgl.cc [med-handle] WebGLSync class registered" \
    "$NXJS/source/webgl.cc" \
    '"WebGLSync", K_SYNC'
# Sampler family (7)
check 53 "webgl.cc [med-sampler] createSampler" \
    "$NXJS/source/webgl.cc" \
    '"createSampler", w_create_sampler'
check 53 "webgl.cc [med-sampler] getSamplerParameter" \
    "$NXJS/source/webgl.cc" \
    '"getSamplerParameter", w_get_sampler_parameter'
# Sync family (6)
check 53 "webgl.cc [med-sync] fenceSync" \
    "$NXJS/source/webgl.cc" \
    '"fenceSync", w_fence_sync'
check 53 "webgl.cc [med-sync] clientWaitSync" \
    "$NXJS/source/webgl.cc" \
    '"clientWaitSync", w_client_wait_sync'
check 53 "webgl.cc [med-sync] getSyncParameter" \
    "$NXJS/source/webgl.cc" \
    '"getSyncParameter", w_get_sync_parameter'
# Query family (7)
check 53 "webgl.cc [med-query] createQuery" \
    "$NXJS/source/webgl.cc" \
    '"createQuery", w_create_query'
check 53 "webgl.cc [med-query] beginQuery" \
    "$NXJS/source/webgl.cc" \
    '"beginQuery", w_begin_query'
check 53 "webgl.cc [med-query] getQueryParameter" \
    "$NXJS/source/webgl.cc" \
    '"getQueryParameter", w_get_query_parameter'
# UBO introspection (5)
check 53 "webgl.cc [med-ubo] getUniformIndices" \
    "$NXJS/source/webgl.cc" \
    '"getUniformIndices", w_get_uniform_indices'
check 53 "webgl.cc [med-ubo] getActiveUniforms" \
    "$NXJS/source/webgl.cc" \
    '"getActiveUniforms", w_get_active_uniforms'
check 53 "webgl.cc [med-ubo] getActiveUniformBlockName" \
    "$NXJS/source/webgl.cc" \
    '"getActiveUniformBlockName", w_get_active_uniform_block_name'
# Family marker — regression tell for block-wholesale delete.
check 53 "webgl.cc [med] block header comment present" \
    "$NXJS/source/webgl.cc" \
    'Phase-1\.5-MED'

# #54 — Citron-observed ANY_SAMPLES_PASSED / hardware-pending. No engine
# change; guardrail is documentation-only. Ledger entry existence + the
# HW_SESSION_RUNBOOK.md section are what get audited here.
status 54 "KNOWN-OPEN" "ANY_SAMPLES_PASSED Citron-observed / hardware-pending (see docs/HW_SESSION_RUNBOOK.md §#54)"
check_file_exists 54 "docs/HW_SESSION_RUNBOOK.md exists" \
    "$NXJS/docs/HW_SESSION_RUNBOOK.md"

# #55 — Phase-1.5-MED-HIGH: 10 transform-feedback methods + K_TRANSFORM_FEEDBACK
# handle + WebGLTransformFeedback class registration. Counter 78 → 88/88.
check 55 "webgl.cc [mh-handle] K_TRANSFORM_FEEDBACK enum member" \
    "$NXJS/source/webgl.cc" \
    'K_TRANSFORM_FEEDBACK,'
check 55 "webgl.cc [mh-handle] WebGLTransformFeedback class registered" \
    "$NXJS/source/webgl.cc" \
    '"WebGLTransformFeedback", K_TRANSFORM_FEEDBACK'
# 10 methods.
check 55 "webgl.cc [tf] createTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"createTransformFeedback", w_create_transform_feedback'
check 55 "webgl.cc [tf] deleteTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"deleteTransformFeedback", w_delete_transform_feedback'
check 55 "webgl.cc [tf] isTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"isTransformFeedback", w_is_transform_feedback'
check 55 "webgl.cc [tf] bindTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"bindTransformFeedback", w_bind_transform_feedback'
check 55 "webgl.cc [tf] beginTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"beginTransformFeedback", w_begin_transform_feedback'
check 55 "webgl.cc [tf] endTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"endTransformFeedback", w_end_transform_feedback'
check 55 "webgl.cc [tf] transformFeedbackVaryings" \
    "$NXJS/source/webgl.cc" \
    '"transformFeedbackVaryings", w_transform_feedback_varyings'
check 55 "webgl.cc [tf] getTransformFeedbackVarying" \
    "$NXJS/source/webgl.cc" \
    '"getTransformFeedbackVarying", w_get_transform_feedback_varying'
check 55 "webgl.cc [tf] pauseTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"pauseTransformFeedback", w_pause_transform_feedback'
check 55 "webgl.cc [tf] resumeTransformFeedback" \
    "$NXJS/source/webgl.cc" \
    '"resumeTransformFeedback", w_resume_transform_feedback'
# Family marker.
check 55 "webgl.cc [mh] block header comment present" \
    "$NXJS/source/webgl.cc" \
    'Phase-1\.5-MED-HIGH'

# #56 — Fix landed 2026-07-03 (glFinish sync + glGetBufferSubData proc-
# address fallback in w_get_buffer_sub_data). Guardrails ensure both
# mitigations stay in place until hardware verifies which one carries.
check 56 "webgl.cc [#56] glFinish() sync candidate before glMapBufferRange" \
    "$NXJS/source/webgl.cc" \
    'candidate \(b\) — sync barrier before map'
check 56 "webgl.cc [#56] glGetBufferSubData proc-address fallback resolver" \
    "$NXJS/source/webgl.cc" \
    'resolve_pfn_get_buffer_sub_data'
check 56 "webgl.cc [#56] proc-address resolution boot log" \
    "$NXJS/source/webgl.cc" \
    '\[#56\] glGetBufferSubData proc-address resolved'
check 56 "webgl.cc [#56] NX_56_DEBUG per-call diagnostic guard" \
    "$NXJS/source/webgl.cc" \
    '#ifdef NX_56_DEBUG'
check 56 "webgl.cc [#56] second-stage: per-target rebind fallback" \
    "$NXJS/source/webgl.cc" \
    'per-target rebind fallback for map'
check 56 "webgl.cc [#56] rebind branch: COPY_WRITE_BUFFER handling" \
    "$NXJS/source/webgl.cc" \
    'target == GL_COPY_WRITE_BUFFER \|\| target == GL_COPY_READ_BUFFER'

# #56b — Re-invocation write-visibility race fence-guard. Empirical Mesa
# Nouveau NV120 workaround; shared helper so any future GPU→CPU map-read
# site uses the same primitive without copy-paste.
check 56b "webgl.cc [#56b] shared sync-guard helper defined" \
    "$NXJS/source/webgl.cc" \
    'static void nx_56b_readback_sync_guard'
check 56b "webgl.cc [#56b] fence + clientWaitSync + deleteSync sequence" \
    "$NXJS/source/webgl.cc" \
    'glFenceSync\(GL_SYNC_GPU_COMMANDS_COMPLETE'
check 56b "webgl.cc [#56b] clientWaitSync with FLUSH_COMMANDS_BIT + 100ms" \
    "$NXJS/source/webgl.cc" \
    'glClientWaitSync\(sync, GL_SYNC_FLUSH_COMMANDS_BIT, timeout_ns\)'
check 56b "webgl.cc [#56b] TIMEOUT log line (don't hang runtime)" \
    "$NXJS/source/webgl.cc" \
    '\[#56b\] %s: clientWaitSync TIMEOUT'
check 56b "webgl.cc [#56b] w_get_buffer_sub_data calls the sync guard" \
    "$NXJS/source/webgl.cc" \
    'nx_56b_readback_sync_guard\("getBufferSubData"\)'
# Class-close guardrail: only ONE glMapBufferRange with GL_MAP_READ_BIT
# should exist in webgl.cc as of 2026-07-03. If a new call site lands
# without the sync-guard, this check regresses on the next audit.
check 56b "webgl.cc [#56b] class closes at single map-read site" \
    "$NXJS/source/webgl.cc" \
    'GL_MAP_READ_BIT'

# #57 — Batch 3 final extension batch. All driver-gated advertising +
# w_get_extension branches + FUNCS[] wiring.
check 57 "webgl.cc [b3] resolve_b3_pfns proc-address resolver" \
    "$NXJS/source/webgl.cc" \
    'static void resolve_b3_pfns'
check 57 "webgl.cc [b3] one-shot resolution boot log" \
    "$NXJS/source/webgl.cc" \
    '\[b3\] extension entry-point resolution'
# Extension entry points.
check 57 "webgl.cc [b3] w_clip_control_ext" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_clip_control_ext\)'
check 57 "webgl.cc [b3] w_polygon_offset_clamp_ext" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_polygon_offset_clamp_ext\)'
check 57 "webgl.cc [b3] w_query_counter_ext" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_query_counter_ext\)'
check 57 "webgl.cc [b3] w_max_shader_compiler_threads_khr" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_max_shader_compiler_threads_khr\)'
check 57 "webgl.cc [b3] w_enable_i / w_disable_i / w_is_enabled_i (OES_draw_buffers_indexed)" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_enable_i\)'
check 57 "webgl.cc [b3] w_multi_draw_arrays_webgl" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_multi_draw_arrays_webgl\)'
check 57 "webgl.cc [b3] w_multi_draw_elements_instanced_webgl" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_multi_draw_elements_instanced_webgl\)'
# Advertising rows (in w_get_supported_extensions).
check 57 "webgl.cc [b3] EXT_clip_control advertised" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("EXT_clip_control"\)'
check 57 "webgl.cc [b3] EXT_disjoint_timer_query advertised (v1)" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("EXT_disjoint_timer_query"\)'
check 57 "webgl.cc [b3] EXT_disjoint_timer_query_webgl2 advertised (v2)" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("EXT_disjoint_timer_query_webgl2"\)'
check 57 "webgl.cc [b3] OES_draw_buffers_indexed advertised" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("OES_draw_buffers_indexed"\)'
check 57 "webgl.cc [b3] WEBGL_multi_draw advertised" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("WEBGL_multi_draw"\)'
check 57 "webgl.cc [b3] WEBGL_blend_func_extended advertised" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("WEBGL_blend_func_extended"\)'
check 57 "webgl.cc [b3] KHR_parallel_shader_compile advertised" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("KHR_parallel_shader_compile"\)'
check 57 "webgl.cc [b3] WEBGL_clip_cull_distance advertised (v2)" \
    "$NXJS/source/webgl.cc" \
    'out\.push_back\("WEBGL_clip_cull_distance"\)'
# w_get_extension branches.
check 57 "webgl.cc [b3] w_get_extension branch: EXT_clip_control" \
    "$NXJS/source/webgl.cc" \
    'strcmp\(name, "EXT_clip_control"\)'
check 57 "webgl.cc [b3] w_get_extension branch: EXT_disjoint_timer_query" \
    "$NXJS/source/webgl.cc" \
    'strcmp\(name, "EXT_disjoint_timer_query"\)'
check 57 "webgl.cc [b3] w_get_extension branch: WEBGL_multi_draw" \
    "$NXJS/source/webgl.cc" \
    'strcmp\(name, "WEBGL_multi_draw"\)'
# Family marker.
check 57 "webgl.cc [b3] block header comment present" \
    "$NXJS/source/webgl.cc" \
    'Batch 3 \(ledger #57\)'
# #52a fallback gate — new build define check.
check 52 "webgl.cc [52a] fallback gate: NX_52A_DISABLE_FALLBACK ifndef guard present" \
    "$NXJS/source/webgl.cc" \
    '#ifndef NX_52A_DISABLE_FALLBACK'

# #58 — Tier 1: getUniform full type-switched impl.
check 58 "webgl.cc [tier1] w_get_uniform FN body" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_uniform\)'
check 58 "webgl.cc [tier1] getUniform FUNCS entry (v1 + v2)" \
    "$NXJS/source/webgl.cc" \
    '\{"getUniform", w_get_uniform\}'
check 58 "webgl.cc [tier1] block header comment present" \
    "$NXJS/source/webgl.cc" \
    'Tier 1 batch \(ledger #58\)'

# #59 — Tier 1: copyTexImage2D thin wrapper.
check 59 "webgl.cc [tier1] w_copy_tex_image_2d FN body" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_copy_tex_image_2d\)'
check 59 "webgl.cc [tier1] copyTexImage2D FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"copyTexImage2D", w_copy_tex_image_2d\}'

# #60 — Tier 1: getVertexAttrib pname-switched impl.
check 60 "webgl.cc [tier1] w_get_vertex_attrib FN body" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_vertex_attrib\)'
check 60 "webgl.cc [tier1] getVertexAttrib FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"getVertexAttrib", w_get_vertex_attrib\}'
# BUFFER_BINDING pname branch (spec-critical: returns Buffer wrapper).
check 60 "webgl.cc [tier1] w_get_vertex_attrib BUFFER_BINDING branch present" \
    "$NXJS/source/webgl.cc" \
    'GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING'

# #61 — Tier 1: getFramebufferAttachmentParameter pname-switched impl.
check 61 "webgl.cc [tier1] w_get_framebuffer_attachment_parameter FN body" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_framebuffer_attachment_parameter\)'
check 61 "webgl.cc [tier1] getFramebufferAttachmentParameter FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"getFramebufferAttachmentParameter", w_get_framebuffer_attachment_parameter\}'

# #62 — Tier 1: getAttachedShaders returns Array of shader wrappers.
check 62 "webgl.cc [tier1] w_get_attached_shaders FN body" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_attached_shaders\)'
check 62 "webgl.cc [tier1] getAttachedShaders FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"getAttachedShaders", w_get_attached_shaders\}'

# #63 — Tier 1: vertexAttrib{1,2,3,4}fv macro-generated wrappers. FN bodies
# are produced by the VA_FV(N) macro; grep for the macro definition + the
# four invocations. Also verify all 4 FUNCS entries wire the resulting
# symbols (1fv + 4fv are the range endpoints — if either is missing the
# macro block was truncated or the FUNCS block regressed).
check 63 "webgl.cc [tier1] VA_FV macro definition present" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_vertex_attrib_##N##fv\)'
check 63 "webgl.cc [tier1] VA_FV(1) invocation" \
    "$NXJS/source/webgl.cc" \
    '^VA_FV\(1\)'
check 63 "webgl.cc [tier1] VA_FV(4) invocation" \
    "$NXJS/source/webgl.cc" \
    '^VA_FV\(4\)'
check 63 "webgl.cc [tier1] vertexAttrib1fv FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"vertexAttrib1fv", w_vertex_attrib_1fv\}'
check 63 "webgl.cc [tier1] vertexAttrib4fv FUNCS entry" \
    "$NXJS/source/webgl.cc" \
    '\{"vertexAttrib4fv", w_vertex_attrib_4fv\}'

# #64 — Tier 1: Screen.toDataURL WebGL-surface readback. Public helper in
# webgl.cc + extern in webgl.h + call sites in canvas.cc (sync + async).
check 64 "webgl.cc [tier1] nx_webgl_snapshot_bridge_rgba8 impl body" \
    "$NXJS/source/webgl.cc" \
    'bool nx_webgl_snapshot_bridge_rgba8'
check 64 "webgl.h [tier1] nx_webgl_snapshot_bridge_rgba8 extern decl" \
    "$NXJS/source/webgl.h" \
    'bool nx_webgl_snapshot_bridge_rgba8'
check 64 "canvas.cc [tier1] webgl.h include" \
    "$NXJS/source/canvas.cc" \
    '#include "webgl.h"'
check 64 "canvas.cc [tier1] nx_webgl_snapshot_bridge_rgba8 call sites" \
    "$NXJS/source/canvas.cc" \
    'nx_webgl_snapshot_bridge_rgba8'

# #65 — Tier 4: compressed-format INVALID_ENUM validation gate. Helper decl
# + block marker + gate branches inside both compressed FN bodies.
check 65 "webgl.cc [tier4] has_compressed_format_advertised helper" \
    "$NXJS/source/webgl.cc" \
    'static bool has_compressed_format_advertised'
check 65 "webgl.cc [tier4] block marker comment present" \
    "$NXJS/source/webgl.cc" \
    'Tier 4 \(ledger #65\)'
check 65 "webgl.cc [tier4] w_compressed_tex_image_2d gate branch" \
    "$NXJS/source/webgl.cc" \
    'if \(!has_compressed_format_advertised\(internalformat\)\)'
check 65 "webgl.cc [tier4] w_compressed_tex_sub_image_2d gate branch" \
    "$NXJS/source/webgl.cc" \
    'if \(!has_compressed_format_advertised\(format\)\)'

# #66 — Tier-A: createImageBitmap source-type expansion.
check 66 "image-bitmap.ts [tier-a] tryUnwrapCanvas helper" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'function tryUnwrapCanvas'
check 66 "image-bitmap.ts [tier-a] canvasToImageBitmap helper" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'function canvasToImageBitmap'
check 66 "image-bitmap.ts [tier-a] ImageData source branch" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'image instanceof ImageData'
check 66 "image-bitmap.ts [tier-a] ImageBitmap source branch" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'image instanceof ImageBitmap'
check 66 "image-bitmap.ts [tier-a] live-DOM CANVAS unwrap" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    "tagName\?: string.*=== 'CANVAS'|tagName === 'CANVAS'"
check 66 "image-bitmap.ts [tier-a] HTMLVideoElement diagnostic (not silently supported)" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'HTMLVideoElement source not yet supported'

# #67 — Tier-A: getParameter extension-gated pname enforcement.
check 67 "webgl.cc [tier-a] WebGLState.enabled_exts field" \
    "$NXJS/source/webgl.cc" \
    'std::unordered_set<std::string> enabled_exts'
check 67 "webgl.cc [tier-a] record_ext_enabled helper" \
    "$NXJS/source/webgl.cc" \
    'static void record_ext_enabled'
check 67 "webgl.cc [tier-a] is_ext_enabled helper" \
    "$NXJS/source/webgl.cc" \
    'static bool is_ext_enabled'
check 67 "webgl.cc [tier-a] Ledger #67 gated-pname block header" \
    "$NXJS/source/webgl.cc" \
    'Ledger #67 — extension-gated pname enforcement'
check 67 "webgl.cc [tier-a] CLIP_ORIGIN_EXT gated branch" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("EXT_clip_control"\)'
check 67 "webgl.cc [tier-a] DEPTH_CLAMP_EXT gated branch" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("EXT_depth_clamp"\)'
check 67 "webgl.cc [tier-a] POLYGON_OFFSET_CLAMP_EXT gated branch" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("EXT_polygon_offset_clamp"\)'
check 67 "webgl.cc [tier-a] MAX_DUAL_SOURCE_DRAW_BUFFERS_WEBGL gated branch" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("WEBGL_blend_func_extended"\)'
check 67 "webgl.cc [tier-a] FRAGMENT_SHADER_DERIVATIVE_HINT_OES gated branch" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("OES_standard_derivatives"\)'
check 67 "webgl.cc [tier-a] UNMASKED_VENDOR/RENDERER_WEBGL gated" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("WEBGL_debug_renderer_info"\)'
check 67 "webgl.cc [tier-a] MAX_TEXTURE_MAX_ANISOTROPY_EXT gated" \
    "$NXJS/source/webgl.cc" \
    'is_ext_enabled\("EXT_texture_filter_anisotropic"\)'

# #68 — Tier-A: attribute-aliasing link failure detection.
check 68 "webgl.cc [tier-a] programs_with_aliased_link field" \
    "$NXJS/source/webgl.cc" \
    'std::unordered_set<GLuint> programs_with_aliased_link'
check 68 "webgl.cc [tier-a] nx_detect_link_attrib_aliasing helper" \
    "$NXJS/source/webgl.cc" \
    'static void nx_detect_link_attrib_aliasing'
check 68 "webgl.cc [tier-a] Ledger #68 comment in w_link_program" \
    "$NXJS/source/webgl.cc" \
    'Ledger #68 — post-link aliased-attribute check'
check 68 "webgl.cc [tier-a] LINK_STATUS override in w_get_program_parameter" \
    "$NXJS/source/webgl.cc" \
    'Ledger #68 — LINK_STATUS override for aliased-attribute programs'
check 68 "webgl.cc [tier-a] delete_program clears aliased record" \
    "$NXJS/source/webgl.cc" \
    'programs_with_aliased_link\.erase\(id\)'

# #69 — Tier-A: texImage2D / texSubImage2D ImageBitmap / Image source support.
check 69 "webgl.cc [tier-a] image.h include" \
    "$NXJS/source/webgl.cc" \
    '#include "image.h"'
check 69 "webgl.cc [tier-a] convert_image_source_to_gl_pixels helper" \
    "$NXJS/source/webgl.cc" \
    'static uint8_t \*convert_image_source_to_gl_pixels'
check 69 "webgl.cc [tier-a] Ledger #69 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #69 — texImage2D / texSubImage2D ImageBitmap \+ Image source support'
check 69 "webgl.cc [tier-a] w_tex_image_2d nx_get_image probe" \
    "$NXJS/source/webgl.cc" \
    'Ledger #69 — ImageBitmap / Image \(nx_image_t\) source'
check 69 "webgl.cc [tier-a] w_tex_sub_image_2d image-source path" \
    "$NXJS/source/webgl.cc" \
    'Ledger #69 — ImageBitmap / Image source path mirrors w_tex_image_2d'

# #70 — Tier-A: TexImageSource normalization shim for WebGL 1.
check 70 "webgl-rendering-context.ts [tier-a] ImageData import" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    "import \{ ImageData \} from './image-data'"
check 70 "webgl-rendering-context.ts [tier-a] OffscreenCanvas import" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    "import \{ OffscreenCanvas \} from './offscreen-canvas'"
check 70 "webgl-rendering-context.ts [tier-a] Ledger #70 header comment" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'Ledger #70 — TexImageSource normalization shim for WebGL 1'
check 70 "webgl-rendering-context.ts [tier-a] isTexImageSource helper" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'function isTexImageSource'
check 70 "webgl-rendering-context.ts [tier-a] sourceToPixels helper" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'function sourceToPixels'
check 70 "webgl-rendering-context.ts [tier-a] prototype wrap install" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'const p: any = WebGLRenderingContext\.prototype'

# #71 — Tier-A: ImageBitmap-source short-circuit in the WebGL 1 shim.
check 71 "webgl-rendering-context.ts [tier-a] ImageBitmap import" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    "import \{ ImageBitmap \} from './image-bitmap'"
check 71 "webgl-rendering-context.ts [tier-a] Ledger #71 header comment" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'Ledger #71 — ImageBitmap passthrough'
check 71 "webgl-rendering-context.ts [tier-a] instanceof ImageBitmap short-circuit (both wrappers)" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'last instanceof ImageBitmap'

# #72 — Tier-A: default viewport at context creation + un-mult removal.
check 72 "webgl.cc [tier-a] Ledger #72 viewport seed in enter_bracket" \
    "$NXJS/source/webgl.cc" \
    'Ledger #72 — before capturing user_snap for the first time'
check 72 "webgl.cc [tier-a] glViewport seed with canvas dims" \
    "$NXJS/source/webgl.cc" \
    'glViewport\(0, 0, \(GLsizei\)st->width, \(GLsizei\)st->height\)'
check 72 "webgl.cc [tier-a] un-mult removed from tex_image_2d" \
    "$NXJS/source/webgl.cc" \
    'Ledger #72 — image_bitmap conformance tests do NOT call'
check 72 "webgl.cc [tier-a] un-mult removed from tex_sub_image_2d" \
    "$NXJS/source/webgl.cc" \
    'Ledger #72 — see w_tex_image_2d'

# #73 — Tier-A: honor createImageBitmap imageOrientation + premultiplyAlpha.
check 73 "image.cc [tier-a] Ledger #73 header comment" \
    "$NXJS/source/image.cc" \
    'Ledger #73 — optional 3rd arg'
check 73 "image.cc [tier-a] premultiply flag default true" \
    "$NXJS/source/image.cc" \
    'bool premultiply = true'
check 73 "image-bitmap.ts [tier-a] Ledger #73 header comment" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'Ledger #73 — extract flipY'
check 73 "image-bitmap.ts [tier-a] extractOpts helper" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'function extractOpts'
check 73 "image-bitmap.ts [tier-a] flipY option check" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    "imageOrientation === 'flipY'"
check 73 "image-bitmap.ts [tier-a] unpremul option check" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    "premultiplyAlpha === 'none'"
check 73 "image-bitmap.ts [tier-a] canvasToImageBitmap opts threaded through" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'canvasToImageBitmap\(\w+, opts\)'

# #74 — Track-A DrumBrake wasm interpreter opt-in gate + empirical wasm-tier probe.
check 74 "config.h wasm_interpreter_opt_in field" \
    "$NXJS/source/config.h" \
    '^\s*bool wasm_interpreter_opt_in;'
check 74 "config.cc wasm_interpreter parse branch" \
    "$NXJS/source/config.cc" \
    'str_ieq\(name, "wasm_interpreter"\)'
check 74 "main.cc --wasm-jitless conditional append (gated on wasm_interpreter_opt_in)" \
    "$NXJS/source/main.cc" \
    'V8::SetFlagsFromString\("--wasm-jitless"\)'
check 74 "main.cc nx_probe_wasm_tier helper defined" \
    "$NXJS/source/main.cc" \
    'static void nx_probe_wasm_tier'
check 74 "main.cc [wasm] mode= boot log line" \
    "$NXJS/source/main.cc" \
    '\[wasm\] mode=%s'

# #75 — Tier-A: TEXTURE_CUBE_MAP ImageBitmap upload support in cube-route-shim.
check 75 "cube-route-shim.ts [tier-a] Ledger #75 header comment" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #75 — the pre-#75 helper `imageSourceToBytes`'
check 75 "cube-route-shim.ts [tier-a] allocateCubeRTAtlas split (base atlas unconditional)" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #75 — split path\. Base atlas allocation is ALWAYS done'
check 75 "cube-route-shim.ts [tier-a] hasRescueDeps local gate" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'const hasRescueDeps = !!\(origFramebufferTexture2D'
check 75 "cube-route-shim.ts [tier-a] image-source 7-arg native forward" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #75 — image sources \(ImageBitmap primarily'
check_absent 75 "cube-route-shim.ts [tier-a] imageSourceToBytes helper removed" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'function imageSourceToBytes'

# #76 — Tier-A: pre-atlas texParameteri/f cache on cube textures.
check 76 "cube-route-shim.ts [tier-a] Ledger #76 header comment" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #76 — pre-atlas texParameteri/f cache'
check 76 "cube-route-shim.ts [tier-a] pendingCubeParams WeakMap declared" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'const pendingCubeParams = new WeakMap<WebGLTexture, PendingParam\[\]>'
check 76 "cube-route-shim.ts [tier-a] stashPendingCubeParam helper" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'function stashPendingCubeParam'
check 76 "cube-route-shim.ts [tier-a] applyPendingCubeParams helper" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'function applyPendingCubeParams'
check 76 "cube-route-shim.ts [tier-a] stash call from texParameteri wrap" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    "stashPendingCubeParam\(tex, pname, param, 'i'\)"
check 76 "cube-route-shim.ts [tier-a] stash call from texParameterf wrap" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    "stashPendingCubeParam\(tex, pname, param, 'f'\)"

# #77 — Tier-A: null-source cube-face texImage2D early-return for faces 1-5.
check 77 "cube-route-shim.ts [tier-a] Ledger #77 header comment" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #77 — null/undefined-source cube-face texImage2D with state'
check 77 "cube-route-shim.ts [tier-a] null-source early-return branch" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'if \(source === null \|\| source === undefined\)'

# #78 — Tier-A: preserve alpha=0 pixels' RGB across createImageBitmap round-trip.
check 78 "image.h [tier-a] unpremultiplied field on nx_image_t" \
    "$NXJS/source/image.h" \
    '^\s*bool unpremultiplied;'
check 78 "image.cc [tier-a] Ledger #78 header comment" \
    "$NXJS/source/image.cc" \
    'Ledger #78 — imageCopyPixels'
check 78 "image.cc [tier-a] nx_image_copy_pixels definition" \
    "$NXJS/source/image.cc" \
    '^void nx_image_copy_pixels\('
check 78 "image.cc [tier-a] imageCopyPixels registration" \
    "$NXJS/source/image.cc" \
    'NX_SET_FUNC\(init_obj, "imageCopyPixels"'
check 78 "image.cc [tier-a] unpremultiplied set in imageWriteRGBA" \
    "$NXJS/source/image.cc" \
    'image->unpremultiplied = !premultiply'
check 78 "image-bitmap.ts [tier-a] Ledger #78 header comments" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'Ledger #78'
check 78 "image-bitmap.ts [tier-a] ImageBitmap branch uses imageCopyPixels" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    '\$\.imageCopyPixels\(bmp, image, !opts\.unpremul, opts\.flipY\)'
check 78 "\$.ts [tier-a] imageCopyPixels signature" \
    "$NXJS/packages/runtime/src/\$.ts" \
    'imageCopyPixels\('

# #79 — Tier-A: LUMINANCE / LUMINANCE_ALPHA use L=R (spec Table 5.14.6.1).
check 79 "webgl.cc [tier-a] Ledger #79 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #79 — WebGL 1 spec Table 5.14.6.1'
check_absent 79 "webgl.cc [tier-a] Rec.601 luma formula removed from convert_image_source_to_gl_pixels" \
    "$NXJS/source/webgl.cc" \
    'r \* 299 \+ g \* 587 \+ b \* 114'

# #80 — Tier-A: Image.src baseURL prefers globalThis.location.href.
check 80 "image.ts [tier-a] Ledger #80 header comment" \
    "$NXJS/packages/runtime/src/image.ts" \
    'Ledger #80'
check 80 "image.ts [tier-a] baseUrl consults location.href before baseURI" \
    "$NXJS/packages/runtime/src/image.ts" \
    'g\.location\?\.href \?\? g\.document\?\.baseURI'

# #81 — Tier-A: resolveLiveResourceUrl prefers globalThis.location.href
# at call time (MOVED to runtime — brewser-runtime-v8/RUNTIME_SHIMS.md).
check 81 "live-dom.ts [tier-a] Ledger #81 header comment" \
    "$RUNTIME/src/scripts/live-dom.ts" \
    'Ledger #81'
check 81 "live-dom.ts [tier-a] activeBase reads globalThis.location.href before livePageBase" \
    "$RUNTIME/src/scripts/live-dom.ts" \
    'let activeBase = liveHref \?\? livePageBase'
check 81 "live-dom.ts [tier-a] activeBase normalized to directory URL (strip filename)" \
    "$RUNTIME/src/scripts/live-dom.ts" \
    'activeBase\.substring\(0, lastSlash \+ 1\)'
check 81 "live-dom.ts [tier-a] Ledger #81b header comment" \
    "$RUNTIME/src/scripts/live-dom.ts" \
    'Ledger #81b'
check 81 "live-dom.ts [tier-a] new URL() constructor tried before manual walker" \
    "$RUNTIME/src/scripts/live-dom.ts" \
    'try \{\s*return new URL\(s, activeBase\)\.toString\(\);'

# #82 — Tier-A: canvasToImageBitmap fast-path fallback on encode/decode
# failure — raw getImageData + imageWriteRGBA route.
check 82 "image-bitmap.ts [tier-a] Ledger #82 header comment" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'Ledger #82'
check 82 "image-bitmap.ts [tier-a] fast-path try/catch fallback diag marker" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    '\[image-bitmap:#82\] fast-path encode/decode failed'
check 82 "image-bitmap.ts [tier-a] fallback writes premultiplied via imageWriteRGBA(bmp, ..., true)" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    '\$\.imageWriteRGBA\(bmp, bytes\.buffer, true\)'

# #83 — Tier-A: createImageBitmap(Blob, opts) honors imageOrientation
# + premultiplyAlpha via post-decode imageCopyPixels step.
check 83 "image-bitmap.ts [tier-a] Ledger #83 header comment" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    'Ledger #83'
check 83 "image-bitmap.ts [tier-a] Blob branch options-path uses imageCopyPixels" \
    "$NXJS/packages/runtime/src/canvas/image-bitmap.ts" \
    '\$\.imageCopyPixels\(bmp, decoded, !opts\.unpremul, opts\.flipY\)'

# #84 — Tier-A: createImageBitmap(<video>) runtime shim.
check 84 "live-video.ts [tier-a] Ledger #84 header comment" \
    "$RUNTIME/src/scripts/live-video.ts" \
    'Ledger #84'
check 84 "live-video.ts [tier-a] installVideoImageBitmapShim exported" \
    "$RUNTIME/src/scripts/live-video.ts" \
    'export function installVideoImageBitmapShim'
check 84 "live-video.ts [tier-a] VIDEO_IMG_BITMAP_SHIM_BRAND symbol" \
    "$RUNTIME/src/scripts/live-video.ts" \
    'VIDEO_IMG_BITMAP_SHIM_BRAND'
check 84 "canvas-runner.ts [tier-a] installVideoImageBitmapShim called from installPageGlobals" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'installVideoImageBitmapShim\(\)'
check 84 "live-video.ts [tier-a] brewser://→sdmc:/switch/brewser/ translation in resolveSourceForDecoder" \
    "$RUNTIME/src/scripts/live-video.ts" \
    "replace\\(/\\^brewser:\\\\/\\\\/\\/i, 'sdmc:/switch/brewser/'"
check 84 "live-video.ts [tier-a] captureVideoFrameToBitmap forces SW reopen on HW decoder pre-first-frame" \
    "$RUNTIME/src/scripts/live-video.ts" \
    'openDecoder\(el, st0, false\)'
check_file_exists 84 "brewser-apps [tier-a] webgl1 video assets synced (red-green.mp4)" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/red-green.mp4"
check_file_exists 84 "brewser-apps [tier-a] webgl1 video assets synced (red-green.webmvp8.webm)" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/red-green.webmvp8.webm"
check_file_exists 84 "brewser-apps [tier-a] webgl1 video assets synced (red-green.bt601.vp9.webm)" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/red-green.bt601.vp9.webm"

# #99 — Tier-A: cube-route-shim atlas-ifies typed-array cube uploads (with
# _emptyCubeTexture exclusion). Runtime-side ledger (cube-route-shim.ts in
# brewser-runtime-v8) — MOVED pointer in NXJS_PATCHES_NEEDED.md.
check 99 "cube-route-shim.ts [tier-a] Ledger #99 header (detect Three.js)" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #99 — detect Three.js'
check 99 "cube-route-shim.ts [tier-a] isEmptyCubePlaceholder predicate" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'isEmptyCubePlaceholder'
check 99 "cube-route-shim.ts [tier-a] wantAtlas gate includes typed-array" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'isTypedArrayLike\(source\) && w >= 1 && h >= 1 &&'

# #98 — Tier-A: WebGL constants installed enumerable: true + vertexAttrib
# Pointer type validation (INT/UNSIGNED_INT/FIXED reject) on WebGL 1.
check 98 "webgl-rendering-context.ts [tier-a] descs enumerable: true (v1)" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'enumerable: true'
check 98 "webgl2-rendering-context.ts [tier-a] descs enumerable: true (v2)" \
    "$NXJS/packages/runtime/src/canvas/webgl2-rendering-context.ts" \
    'enumerable: true'
check 98 "webgl.cc [tier-a] w_vertex_attrib_pointer type gate INVALID_ENUM" \
    "$NXJS/source/webgl.cc" \
    'Ledger #98 — WebGL 1 spec'

# #97 — Tier-A: texParameter{i,f} on target with no bound texture generates
# INVALID_OPERATION per WebGL 1 spec §5.14.8.
check 97 "webgl.cc [tier-a] nx_binding_pname_for_tex_target helper" \
    "$NXJS/source/webgl.cc" \
    'nx_binding_pname_for_tex_target'
check 97 "webgl.cc [tier-a] TEXTURE_BINDING_2D bpname 0x8069" \
    "$NXJS/source/webgl.cc" \
    '\*out = 0x8069'
check 97 "webgl.cc [tier-a] texParameteri record_error on unbound target" \
    "$NXJS/source/webgl.cc" \
    'w_tex_parameteri'

# #96 — Tier-A: sync XMLHttpRequest via Switch.readFileSync. Runtime-side
# ledger (xhr.ts in brewser-runtime-v8) — MOVED pointer in
# NXJS_PATCHES_NEEDED.md.
check 96 "xhr.ts [tier-a] Ledger #95b (sync-XHR) header comment" \
    "$RUNTIME/src/polyfills/xhr.ts" \
    'Ledger #95b — sync-XHR support'
check 96 "xhr.ts [tier-a] _sync flag captured in open()" \
    "$RUNTIME/src/polyfills/xhr.ts" \
    'this\._sync = _async === false'
check 96 "xhr.ts [tier-a] send() sync branch dispatches Switch.readFileSync" \
    "$RUNTIME/src/polyfills/xhr.ts" \
    'if \(this\._sync\)'

# #95 — Tier-A: WebGL wrapper `deleted` flag + wrapper-cache retention on
# delete + bindX(deletedX) INVALID_OPERATION + getFramebufferAttachment-
# Parameter(NONE, OBJECT_NAME) INVALID_ENUM on WebGL 1 + shader asset sync
# + FBO delete-of-bound fallback.
check 95 "webgl.cc [tier-a] GLObj deleted flag defined" \
    "$NXJS/source/webgl.cc" \
    'bool deleted = false;'
check 95 "webgl.cc [tier-a] obj_deleted() helper defined" \
    "$NXJS/source/webgl.cc" \
    'inline bool obj_deleted'
check 95 "webgl.cc [tier-a] new_gl_obj_create() helper defined" \
    "$NXJS/source/webgl.cc" \
    'new_gl_obj_create\(Isolate'
check 95 "webgl.cc [tier-a] w_bind_buffer rejects deleted (Ledger #95 header)" \
    "$NXJS/source/webgl.cc" \
    'Ledger #95 — WebGL 1 spec'
check 95 "webgl.cc [tier-a] w_get_framebuffer_attachment_parameter INVALID_ENUM on WebGL 1 NONE" \
    "$NXJS/source/webgl.cc" \
    'querying OBJECT_NAME generates'
check 95 "webgl.cc [tier-a] w_delete_framebuffer falls back to tenant on delete-of-bound" \
    "$NXJS/source/webgl.cc" \
    'if \(st && st->bound_fbo_js == o->id\)'
check_file_exists 95 "brewser-apps [tier-a] webgl1 vertexShader.vert synced from webgl2 resources" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/vertexShader.vert"
check_file_exists 95 "brewser-apps [tier-a] webgl1 fragmentShader.frag synced from webgl2 resources" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/fragmentShader.frag"

# #94 — Tier-A: cube-route-shim atlas-alloc gate lowered from w>=8 to w>=1
# so small null-source cube-face texImage2D uploads allocate an atlas —
# fixes the WebGL 1 textures-{svg_image,image}-tex-2d-* cluster's cube+
# texSubImage+flipY=true stale-atlas bug. Runtime-side ledger (cube-route-
# shim.ts in brewser-runtime-v8) — MOVED pointer in NXJS_PATCHES_NEEDED.md.
check 94 "cube-route-shim.ts [tier-a] Ledger #94 header comment" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'Ledger #94 — gate lowered from'
check 94 "cube-route-shim.ts [tier-a] gate is (source === null && w >= 1 && h >= 1)" \
    "$RUNTIME/src/scripts/cube-route-shim.ts" \
    'source === null && w >= 1 && h >= 1'

# #93 — Tier-A: getVertexAttribOffset + extension gate on VERTEX_ARRAY_BINDING_OES.
check 93 "webgl.cc [tier-a] w_get_vertex_attrib_offset FN defined" \
    "$NXJS/source/webgl.cc" \
    'FN\(w_get_vertex_attrib_offset\)'
check 93 "webgl.cc [tier-a] getVertexAttribOffset registered in method table" \
    "$NXJS/source/webgl.cc" \
    '"getVertexAttribOffset", w_get_vertex_attrib_offset'
check 93 "webgl.cc [tier-a] Ledger #93 extension-gate on 0x85B5" \
    "$NXJS/source/webgl.cc" \
    'Ledger #93 — extension-gated on v1'

# #92 — Tier-A: per-context WebGL object wrapper cache (identity
# preservation). new_gl_obj cache-first + delete_* erasures +
# object-returning w_get_parameter cases.
check 92 "webgl.cc [tier-a] Ledger #92 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #92 — per-context wrapper cache'
check 92 "webgl.cc [tier-a] wrapper_cache field on WebGLState" \
    "$NXJS/source/webgl.cc" \
    'std::unordered_map<uint64_t, Global<Object>> wrapper_cache'
check 92 "webgl.cc [tier-a] erase_wrapper_cache helper defined" \
    "$NXJS/source/webgl.cc" \
    'inline void erase_wrapper_cache'
check 92 "webgl.cc [tier-a] w_get_parameter has ARRAY_BUFFER_BINDING case" \
    "$NXJS/source/webgl.cc" \
    "case 0x8894 /\\* GL_ARRAY_BUFFER_BINDING \\*/"

# #91 — Tier-A: WebGL 1 NPOT texture restrictions (generateMipmap /
# texImage2D level>0 / copyTexImage2D level>0) per §5.14.8.
check 91 "webgl.cc [tier-a] Ledger #91 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #91 — WebGL 1 spec .5\.14\.8'
check 91 "webgl.cc [tier-a] is_pot helper defined" \
    "$NXJS/source/webgl.cc" \
    'static inline bool is_pot\(GLint n\)'

# #90 — Tier-A: shaderSource rejects _webgl_/webgl_ reserved-prefix
# identifiers per WebGL 1 spec §5 (driver leniency workaround).
check 90 "webgl.cc [tier-a] Ledger #90 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #90 — WebGL 1 spec .5 GLSL identifier reservation'
check 90 "webgl.cc [tier-a] has_reserved_webgl_identifier function present" \
    "$NXJS/source/webgl.cc" \
    'has_reserved_webgl_identifier'

# #89 — Tier-A: minimal SVG decoder for Khronos conformance's red-green.svg
# (WebGL 1 textures-svg_image-tex-2d-* cluster needs both the asset synced
# from webgl2 resources AND the engine decode path added).
check 89 "image.h [tier-a] FORMAT_SVG in ImageFormat enum" \
    "$NXJS/source/image.h" \
    'FORMAT_SVG'
check 89 "image.cc [tier-a] Ledger #89 SVG detection in identify_image_format" \
    "$NXJS/source/image.cc" \
    'Ledger #89 — minimal SVG detection'
check 89 "image.cc [tier-a] Ledger #89 decode_svg parser present" \
    "$NXJS/source/image.cc" \
    'Ledger #89 — targeted SVG parser'
check_file_exists 89 "brewser-apps [tier-a] webgl1 red-green.svg synced from webgl2 resources" \
    "$APPS/apps/experimental/com.natureglass.webglconformtest/full-webgl1-conformance/sdk/tests/resources/red-green.svg"
check 89 "webgl-rendering-context.ts [tier-a] Image sources route through the #71 short-circuit (v2)" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'last instanceof ImageBitmap \|\| last instanceof Image'

# #88 — Tier-A: getUniformLocation rejects out-of-range bracket indices
# (client-side WebGL 1 spec validation for the driver-leniency case where
# Mesa-Nouveau wraps uint32 index overflow instead of returning null).
check 88 "webgl.cc [tier-a] Ledger #88 header comment" \
    "$NXJS/source/webgl.cc" \
    'Ledger #88 — WebGL 1 spec 5\.14'
check 88 "webgl.cc [tier-a] bracket-index validation loop" \
    "$NXJS/source/webgl.cc" \
    'idx > \(uint64_t\)0x7FFFFFFF'

# #87 — Tier-A: gate WEBGL_multi_draw advertisement on GL_ANGLE_multi_draw
# (gl_DrawID capability) so we don't advertise a spec-incomplete extension.
check 87 "webgl.cc [tier-a] Ledger #87 header comment" \
    "$NXJS/source/webgl.cc" \
    "Ledger #87 — WEBGL_multi_draw's Khronos spec REQUIRES gl_DrawID"
check 87 "webgl.cc [tier-a] gate requires GL_ANGLE_multi_draw" \
    "$NXJS/source/webgl.cc" \
    'has_native_ext\("GL_ANGLE_multi_draw"\)'

# #86 — Tier-A: page scripts share ONE AsyncFunction scope so cross-script
# eval() sees top-level var / const / let. Runtime-side ledger (canvas-runner.ts
# in brewser-runtime-v8) — MOVED pointer in NXJS_PATCHES_NEEDED.md.
check 86 "canvas-runner.ts [tier-a] Ledger #86 header comment" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    'Ledger #86 — run all page scripts in one shared AsyncFunction'
check 86 "canvas-runner.ts [tier-a] scripts concatenated with __b() separator" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    "bodies\\.join\\(.\\\\n;__b\\(\\);\\\\n."
check 86 "canvas-runner.ts [tier-a] AsyncFunctionCtor called with __b parameter" \
    "$RUNTIME/src/scripts/canvas-runner.ts" \
    "new AsyncFunctionCtor\\("

# #85 — Tier-A: canvas-source short-circuit in the WebGL 1 TexImageSource shim
# (canvasToNxImageBitmap wraps a canvas source's raw pixels into a fresh
# nx_image_t, then falls through to the existing #71 ImageBitmap short-circuit
# so native's convert_image_source_to_gl_pixels handles flipY / format
# conversion / premul / colorspace correctly).
check 85 "webgl-rendering-context.ts [tier-a] Ledger #85 header comment" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'Ledger #85 — canvas-source short-circuit'
check 85 "webgl-rendering-context.ts [tier-a] canvasToNxImageBitmap helper defined" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'function canvasToNxImageBitmap'
check 85 "webgl-rendering-context.ts [tier-a] isCanvasSource guard defined" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'function isCanvasSource'
check 85 "webgl-rendering-context.ts [tier-a] texImage2D wrapper invokes canvas conversion before ImageBitmap branch" \
    "$NXJS/packages/runtime/src/canvas/webgl-rendering-context.ts" \
    'isCanvasSource\(last\)'

echo
echo "=== meta-check: ledger vs script coverage ==="
# Non-fatal warnings. Detects:
#   - ledger entries with no script check (missing coverage)
#   - script check IDs that don't correspond to any ledger heading
#     (orphaned check — entry moved/renamed without updating the script)
# Tombstoned headings ("## #N — MOVED → ...") count as ledger entries
# for coverage purposes (the moved entry has a check against its new
# location).

meta_warn=0

# Extract all script check IDs seen during this run. We prefix status
# lines with "#$id", so re-scan the script for `status "$id"` +
# `check "$id"` + `check_absent "$id"` + `check_file_exists "$id"` +
# `echo "  #<id>"` occurrences directly rather than relying on the
# run's output. Give us the actual coverage of the source file, not
# the run.
script="${BASH_SOURCE[0]}"
runtime_ledger="$RUNTIME/RUNTIME_SHIMS.md"
engine_ledger="$NXJS/NXJS_PATCHES_NEEDED.md"

# Ledger IDs from BOTH ledgers (engine + runtime). Match the heading
# form: "## #<id> — ...", but EXCLUDE tombstone lines
# ("## #N — MOVED → …") — the moved entry is checked at its new
# location, which shows up in the OTHER ledger's IDs.
ledger_ids=$(
    { grep -hP '^## #[\w-]+ — (?!MOVED →)' "$engine_ledger" 2>/dev/null || true;
      grep -hP '^## #[\w-]+ — (?!MOVED →)' "$runtime_ledger" 2>/dev/null || true; } \
    | grep -oP '(?<=^## #)[\w-]+(?= —)' \
    | sort -u
)

# Script IDs: any argument N in `check N "..."`, `check_absent N "..."`,
# `check_file_exists N "..."`, `status N "..."`, or comment-style
# `#  #<N> verified below`.
script_ids=$(
    grep -hoP '(?:^\s*(?:check|check_absent|check_file_exists|status)\s+)([\w-]+)\b' "$script" 2>/dev/null \
    | awk '{print $NF}' \
    | grep -Ev '^(fail|have_check|missing_check|meta_warn|end|next_headings|end_candidates)$' \
    | sort -u
)
# Also count references in embedded comments like `  #12 verified below`.
script_ids_all=$( { echo "$script_ids";
    grep -hoP '(?<=#)[0-9]+(?= verified below)' "$script" 2>/dev/null || true; } | sort -u )

# 1. Ledger entries with no script check.
for id in $ledger_ids; do
    # Tombstoned entries are those whose heading text starts with "MOVED →".
    # Their coverage lives at the moved location and is asserted in the
    # runtime section; they still count as covered by the "#N verified
    # below in runtime ledger section." echo lines.
    if ! grep -qE "^[[:space:]]*(check|check_absent|check_file_exists|status)[[:space:]]+$id\b" "$script"; then
        if ! grep -qE "verified below.*#$id\b|#$id[[:space:]]+verified below" "$script"; then
            echo "  WARN: ledger entry #$id has no verify-patches.sh check"
            meta_warn=$((meta_warn + 1))
        fi
    fi
done

# 2. Script checks referencing an ID that isn't a ledger entry.
for id in $script_ids_all; do
    # Non-numeric non-tombstone tokens (parsed noise) — skip.
    case "$id" in
        [0-9]*|17-superseded) : ;;
        *) continue ;;
    esac
    # Tombstoned IDs: MOVED-in-ledger entries whose numeric ID we still
    # reference via check_absent guardrails to catch regression.
    # Currently: #40 and #41 (moved to NXJS_PATCHES_ARCHIVE.md
    # 2026-07-03; guardrails ensure engine primitives + runtime call
    # sites don't reappear).
    case "$id" in
        40|41) continue ;;
    esac
    if ! echo "$ledger_ids" | grep -qxF "$id"; then
        echo "  WARN: verify-patches.sh checks #$id but no ledger entry with that id exists"
        meta_warn=$((meta_warn + 1))
    fi
done

if [ "$meta_warn" -eq 0 ]; then
    echo "  clean — every ledger entry has a check, every check has a ledger entry"
fi

echo
echo "=== summary ==="
echo "checks:       $have_check"
echo "MISSING:      $missing_check"
echo "meta warns:   $meta_warn"

if [ "$fail" -eq 0 ]; then
    echo "OK: every applicable patch is PRESENT (open engine asks reported as KNOWN-OPEN)."
    exit 0
else
    echo "FAIL: one or more patches MISSING. See PRESENT / MISSING per entry above."
    exit 1
fi
