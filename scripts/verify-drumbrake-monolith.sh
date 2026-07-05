#!/usr/bin/env bash
# verify-drumbrake-monolith.sh — Track-B acceptance gate for
# NXJS_PATCHES_NEEDED.md #74 (DrumBrake wasm interpreter opt-in).
#
# Purpose. Track A (in-tree engine glue + boot probe) is inert until
# Track B (a switch-v8 pacman package rebuilt with
# v8_enable_drumbrake=true) is installed. Before flipping the default
# `[v8] wasm_interpreter` from off to on, or before trusting any
# `[wasm] mode=drumbrake` claim from the boot probe on a new machine,
# run THIS script to confirm the currently-installed monolith actually
# has the DrumBrake interpreter compiled in.
#
# Why not part of verify-patches.sh. That script verifies REPO CONTENT
# (source diffs present) and is deterministic against tracked files.
# This script verifies TOOLCHAIN STATE (which switch-v8 monolith the
# developer has pulled from pacman) and is machine-local. Mixing the
# two would false-positive verify-patches whenever a developer's local
# monolith is stale.
#
# Usage.
#   scripts/verify-drumbrake-monolith.sh
#   DEVKITPRO=/opt/devkitpro scripts/verify-drumbrake-monolith.sh
#
# Exit codes.
#   0 — PRESENT: DrumBrake symbols found. Safe to run `[v8]
#       wasm_interpreter = on` and expect `[wasm] mode=drumbrake` from
#       the boot probe under Citron / jitless launches.
#   1 — MISSING: DrumBrake symbols absent. The gate is inert; Citron
#       will report `mode=unavailable`. Rebuild switch-v8 upstream with
#       v8_enable_drumbrake=true and re-run.
#   2 — ERROR: cannot locate the monolith archive or nm binary.

set -euo pipefail

DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
LIB="$DEVKITPRO/portlibs/switch/lib/libv8_monolith.a"

if [ ! -f "$LIB" ]; then
    echo "ERROR: monolith not found at $LIB"
    echo "       (set DEVKITPRO if devkitPro is installed elsewhere)"
    exit 2
fi

# Prefer the aarch64 cross-nm (definitely handles the archive object
# format) but fall back to host nm if the toolchain nm isn't on this
# machine. Both should work on GNU ar archives; host nm is a compat
# fallback documented in the Phase-0 investigation (worked on
# devkitPro's Windows msys2 install where the aarch64-none-elf-nm is
# a symlink to host nm anyway).
NM="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm"
if [ ! -x "$NM" ]; then
    if ! command -v nm >/dev/null 2>&1; then
        echo "ERROR: no nm available (looked for $NM then PATH nm)"
        exit 2
    fi
    NM="nm"
fi

# DrumBrake ships several distinctive symbols under
# v8::internal::wasm::… — the arm64/x64 JS↔wasm interpreter wrapper
# builtins, the interpreter runtime + object classes, and the
# per-module bytecode generator that pre-lowers wasm to DrumBrake's
# internal dispatch format. Presence of ANY of these means the
# interpreter code was compiled in.
#
# CAUTION — do NOT match bare `WasmBytecode` or bare `WasmInterpreter`.
# The V8 devtools inspector API (compiled unconditionally) exports
# `getWasmBytecode` and other symbols that string-contain those tokens
# unrelated to the DrumBrake interpreter (false positives). The tokens
# below are DrumBrake-specific (verified against the shipping V8 15.0
# monolith which reports 0 matches — the expected "MISSING" outcome
# until Track B lands).
NEEDLE='GenericJSToWasmInterpreterWrapper|GenericWasmToJSInterpreterWrapper|WasmInterpreterRuntime|WasmInterpreterObject|WasmBytecodeGenerator'

HITS=$("$NM" --defined-only "$LIB" 2>/dev/null | grep -cE "$NEEDLE" || true)

if [ "$HITS" -gt 0 ]; then
    echo "PRESENT: DrumBrake symbols in $LIB ($HITS matches)"
    echo "         [v8] wasm_interpreter = on will select DrumBrake"
    echo "         under Citron / jitless launches."
    exit 0
fi

echo "MISSING: DrumBrake symbols in $LIB (0 matches for $NEEDLE)"
echo ""
echo "The shipping switch-v8 package was built without"
echo "v8_enable_drumbrake=true. Rebuild upstream and reinstall, then"
echo "re-run this script before flipping any wasm_interpreter default."
echo ""
echo "See NXJS_PATCHES_NEEDED.md #74 for the full Track-B activation"
echo "sequence."
exit 1
