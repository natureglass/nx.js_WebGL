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

# #4 — cursor overlay native binding (DEFERRED per ledger)
# Ledger says "DEFERRED, NEEDS ENGINE BINDING" — but the cursor
# files DID land per bucket (a). Presence check:
check_file_exists 4 "cursor.cc native compositor" "$NXJS/source/cursor.cc"

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

echo
echo "=== summary ==="
echo "checks:  $have_check"
echo "MISSING: $missing_check"

if [ "$fail" -eq 0 ]; then
    echo "OK: every applicable patch is PRESENT (open engine asks reported as KNOWN-OPEN)."
    exit 0
else
    echo "FAIL: one or more patches MISSING. See PRESENT / MISSING per entry above."
    exit 1
fi
