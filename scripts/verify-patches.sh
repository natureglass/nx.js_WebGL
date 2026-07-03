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
