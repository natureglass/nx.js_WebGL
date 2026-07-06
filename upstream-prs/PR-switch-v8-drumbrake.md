# PR-switch-v8-drumbrake — EXPERIMENTAL opt-in DrumBrake variant of switch-v8

**Target upstream:** `TooTallNate/pacman-packages`
**Target branch:** `switch-v8`
**Local branch:** `drumbrake-enable` (in `/d/tmp/pacman-packages` clone of the upstream fork)
**Local commits:**
- `12972a2` — switch-v8: enable DrumBrake wasm interpreter (pkgrel=10, EXPERIMENTAL)
- `1c7f08a` — switch-v8: 0011 revised — s2s_RefArrayFill FATAL under PC-off (Amendment 5)
**Companion downstream ledger entry:** [`NXJS_PATCHES_NEEDED.md` #74 Track B](../NXJS_PATCHES_NEEDED.md#L3617) — Citron-scoped deferred outcome documented there.

## PR title (suggested)

> switch-v8: EXPERIMENTAL opt-in `v8_enable_drumbrake=true` variant (pkgrel=10, DO NOT SHIP AS DEFAULT — boot-hangs Citron, hardware untested)

## PR summary (put this at the top of the description; the boot-hang is the most important sentence for anyone triaging)

**⚠ EXPERIMENTAL, opt-in only, do not adopt as the default `switch-v8` pkgrel.** This PR wires `v8_enable_drumbrake=true` into the switch-v8 build so V8's in-tree wasm interpreter (DrumBrake) is available under `--jitless` / `--wasm-jitless`, targeting jitless embedders (Citron emulator, applet-mode launches, `[v8] jit = off` scenarios).

**Observed downstream (nx.js embedder, Citron emulator, latest nightly `0237a9b88`, gate off) as of this PR:** the resulting monolith installs and links cleanly, and `nm --defined-only libv8_monolith.a` finds all expected DrumBrake tokens (`GenericJSToWasmInterpreterWrapper`, `WasmInterpreterRuntime`, `WasmInterpreterObject`, `WasmBytecodeGenerator`, `GenericWasmToJSInterpreterWrapper`) — 3592 matches. However the engine **hangs inside `v8::Isolate::New`** at boot. The same source at the same revision boots cleanly under the previous `pkgrel=9` build (no DrumBrake compiled in). Hardware behaviour is *untested by choice* — the downstream project targets Citron only for jitless workflows; hardware runs full JIT.

This PR is intended as a **discoverable opt-in variant**, not a default upgrade. Downstreams that want to experiment can pull the `drumbrake-enable` branch or a specific `pkgrel=10` release tag; downstreams tracking `master`/`switch-v8` stay on `pkgrel=9` and are unaffected.

## Motivation

`v8_enable_drumbrake=true` enables V8's in-tree wasm interpreter, which — combined with `--jitless --wasm-jitless` at runtime — lets embedders run WebAssembly without a code arena. Concrete downstream want:

- **Emulator dev workflows:** Citron (arm64 dynarmic emulator) can't provide JIT, so nx.js falls back to `--jitless`. Under jitless, `libv8_monolith.a` (pkgrel ≤ 9) has no wasm implementation at all — `WebAssembly.compile` throws immediately. Every WASM app is untestable under emulator, forcing a full CFW-Switch round-trip for a class of quick iteration that shouldn't need it.
- **Applet-mode Switch launches** and any `[v8] jit = off` scenario have the same gap.

Enabling DrumBrake in the build fills that gap on paper. This PR is that build wiring. Runtime activation is still opt-in via `--wasm-jitless`, so hardware-JIT paths remain byte-identical to the current pkgrel=9 flag string.

## Changes

### Build config (`switch/v8/PKGBUILD`)

- `pkgrel`: 9 → 10.
- `_gn_args_jit` heredoc: adds `v8_enable_drumbrake = true` next to the existing `v8_enable_sparkplug` / `_turbofan` / `_maglev` block.
- `prepare()`: two `_apply` calls after the existing 0001–0009 series for the new patches below.

### Patch series (`switch/v8/patches/`)

Two patches added, both required to compile V8 15.0.243 with `v8_enable_drumbrake=true` on the switch-v8 target config (`target_os = "horizon"`, `v8_enable_pointer_compression = false`).

**`0010-drumbrake-horizon-whitelist.patch`** — extends `is_drumbrake_supported` in `gni/v8.gni`:

```
- The Wasm interpreter is currently supported only on arm64 and x64, on
- Windows, Linux, MacOS and tvOS.
+ switch-v8 (EXPERIMENTAL, upstream-unblessed): dropped the
+ `v8_enable_pointer_compression &&` conjunct and added
+ `target_os == "horizon"` to the OS whitelist. Horizon + PC-off combo is
+ NOT tested by V8 upstream. See companion patch 0011 for the single
+ compile-time PC assertion this bypass exposes.
  is_drumbrake_supported =
-     v8_enable_webassembly && v8_enable_pointer_compression &&
+     v8_enable_webassembly &&
      (v8_current_cpu == "x64" || v8_current_cpu == "arm64" ||
       v8_current_cpu == "riscv64") &&
      (target_os == "win" || target_os == "linux" || target_os == "mac" ||
-      target_os == "ios")
+      target_os == "ios" || target_os == "horizon")
```

**`0011-drumbrake-refarrayfill-pc-off-gate.patch`** — guards the sole PC-off compile break the C-pre static audit surfaced:

```
  INSTRUCTION_HANDLER_FUNC s2s_RefArrayFill(...) {
+ #ifdef V8_COMPRESS_POINTERS
      // DrumBrake currently only works with pointer compression.
      static_assert(COMPRESS_POINTERS_BOOL);
      [full upstream body, unchanged]
      NextOp();
+ #else   // V8_COMPRESS_POINTERS
+     // switch-v8 fork: array.fill on wasm-gc ref arrays is unsupported
+     // under pointer-compression-off. The handler must still exist because
+     // its address is odr-used by kInstructionTable, so we keep the
+     // signature and replace the body with a deterministic hard failure.
+     // Matches upstream's own `CHECK(false); // Not supported` in
+     // wasm-interpreter-runtime.cc:2418 for the ref-return marshalling path.
+     FATAL(
+         "DrumBrake: wasm-GC ref array.fill unsupported under PC-off "
+         "(switch-v8 #74)");
+ #endif  // V8_COMPRESS_POINTERS
  }
```

Neither patch changes upstream V8 defaults; both are inert unless a downstream sets `v8_enable_drumbrake=true` at build time.

Horizon patches 0001–0009 untouched.

## C-pre static audit — full findings (2026-07-05)

Full audit was performed against `v8/v8@15.0.243` DrumBrake sources before touching any GN. The 11-file audit tree is at [`src/wasm/interpreter/`](https://github.com/v8/v8/tree/15.0.243/src/wasm/interpreter). Result:

- **Compile-time PC gates**: exactly 1. `wasm-interpreter.cc:6788` `static_assert(COMPRESS_POINTERS_BOOL)` inside `Handlers<false>::s2s_RefArrayFill`. Instantiated via `kInstructionTable` at `wasm-interpreter.cc:7282` (`#define V(name) Handlers<false>::name, FOREACH_NO_BOUNDSCHECK_INSTR_HANDLER(V)`). Upstream comment on that line: `// DrumBrake currently only works with pointer compression.` — policy statement, guarded by patch 0011.
- **Runtime PC gates**: exactly 1, LEFT IN PLACE. `wasm-interpreter-runtime.cc:2418-2424` — inside the ref-return marshalling switch, `#ifdef V8_COMPRESS_POINTERS ... #else CHECK(false); // Not supported`. Fires only when a wasm function returns a `funcref`/`externref`. Downstream target apps (Unity WebGL, older itch.io HTML5 games) do not exercise this path.
- **`interpreter-builtins-arm64.cc`** — uses `LoadTaggedField` (PC-adaptive macro-assembler helper), `LSL kTaggedSizeLog2` (auto-adjusts under PC-off), `#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE` correctly no-ops under PC-off. All slot arithmetic goes through `kTaggedSize` / `kSystemPointerSize` constants — no bare 4/8 offsets.
- **Slot-layout in DrumBrake object accessors** (`wasm-interpreter-objects-inl.h`, `wasm-interpreter-objects.h/cc`, `wasm-interpreter-inl.h`, `wasm-interpreter-runtime.h`, `wasm-interpreter-runtime-inl.h`, `instruction-handlers.h`, `wasm-interpreter-simd.cc`) — no `V8_COMPRESS_POINTERS` / `COMPRESS_POINTERS_BOOL` / `static_assert(kTaggedSize`, no raw offset arithmetic, no compressed-slot type usage.
- **`InitInstructionTableOnce(Isolate*)`** at `wasm-interpreter.cc:703` is guarded by `!V8_DRUMBRAKE_BOUNDS_CHECKS`. Under this target (`v8_drumbrake_bounds_checks = true` — set by the `!(is_win||is_linux||is_mac||is_ios)` disjunct at BUILD.gn:140) it is not compiled at all. Not a factor.

Verdict at audit time: **RISKY** but patchable — one static_assert to guard, no bulk rewrite required. `gn gen` passes with the two patches applied. `ninja v8_monolith` completes without additional errors (only the pre-existing `webgl.cc:2661` sign-compare warning shows in downstream builds).

## Test evidence — where it works and where it does not

**Local static audit passes** — see above.

**Fork CI build reproducible** — `on: push` in `.github/workflows/docker-image.yml` triggered a fresh Docker Buildx run against the `drumbrake-enable` branch; artifact `switch-v8-15.0.243-10-any.pkg.tar.zst` extracted from `ghcr.io/<user-lowercased>/pacman-packages:drumbrake-enable`, SHA256 `27a37df93b489b1485808b483ee47e691144cb4485fb8c67894145711f1b422d`. Pre-Track-B baseline pull (control push of unmodified `switch-v8` to the same fork) went green first; that seed shaved the drumbrake-enable rebuild to ~30 minutes.

**Downstream install:** `dkp-pacman -U switch-v8-15.0.243-10-any.pkg.tar.zst` clean; net upgrade +3.37 MiB. `pacman -Q switch-v8` → `switch-v8 15.0.243-10`.

**Downstream acceptance gate:** downstream ships a `verify-drumbrake-monolith.sh` script that greps `nm --defined-only libv8_monolith.a` for a narrow DrumBrake-specific needle. **Result: PRESENT (3592 matches, exit 0).** Narrow needle deliberately avoids false-positives via the V8 devtools inspector API (which unconditionally exports `getWasmBytecode`).

**Downstream engine link:** clean, no new warnings.

**Downstream boot under Citron (nightly `0237a9b88`, latest at time of test), gate off:**

```
[detect] target=citron (auto: A=1 B=1 C=1 score=3/3) -> mode=jitless
[v8] mem_total=3285 MiB free=3 MiB regime=application -> mode=jitless (Ignition only)
[v8-trace] before V8::Initialize
[v8-trace] after V8::Initialize
[v8] max_heap=512 MiB (arena=1024 MiB free=3 MiB)
[v8-trace] before Isolate::New
                                    ← hang; `[v8-trace] after Isolate::New` never fires.
```

The `[v8-trace]` lines are temporary downstream `fprintf(stderr, "...")` calls added around the four boundary points to localize the hang. The hang is inside `v8::Isolate::New(create_params)` — not `V8::Initialize`, not the runtime configuration between them.

**Cheap Citron-side discriminators, no rebuild:**

- **CPU accuracy = Accurate** (default Auto → Accurate): Citron itself crashes, no log created. Worse failure mode. Reverted.
- **Multicore emulation = Off** (default On → Off): byte-identical baseline hang, same last line, same signature.

**Static audit of the V8 core Isolate::New path** (not the DrumBrake interpreter sources — those already audited above):

- `src/execution/isolate.cc` has 4 `V8_ENABLE_DRUMBRAKE` references, ALL in `CallSiteBuilder::*` (stack-trace assembly) which runs after isolate creation. Not on the hang path.
- `src/wasm/wasm-engine.cc`: `WasmEngine::InitializeOncePerProcess`'s DrumBrake branch is gated on `#ifdef V8_ENABLE_DRUMBRAKE` AND `if (v8_flags.wasm_jitless)`. Under gate-off the runtime flag is false → this branch does not run.

**Root cause is therefore an IMPLICIT effect of the `V8_ENABLE_DRUMBRAKE` define** — most plausibly:

- Changed mksnapshot output (extra DrumBrake wasm-interpreter builtins carried in the snapshot with arm64 relocations that don't resolve under our `switch-v8` `ExternalReferenceTable`), OR
- Broader `ExternalReferenceTable` layout with an entry Horizon-specific stubs elsewhere, OR
- Isolate builtin-deserialization corner case specific to the `V8_TARGET_OS_HORIZON` branch (added via the existing Horizon patches 0005 / 0006 in this same repo).

Confirming any of these requires either a full V8 core deep-dive OR a `V8::Initialize` / `Isolate::Init` internal-trace rebuild through fork CI. Not on the roadmap here; downstream project scope explicitly excludes hardware verification for jitless WASM, and Citron-only investigation has reached the cheap-discriminator limit.

## What downstreams should do

**If you're on the current pkgrel=9 and nothing prompts you to change:** keep tracking `switch-v8`. This PR does not affect you. Wait for a future pkgrel that has the boot hang resolved before opting in.

**If you have a Track-B use case (jitless WASM under emulator or applet mode):** try `pkgrel=10` from the `drumbrake-enable` branch and see if your Citron / emulator / launch-environment combination survives `Isolate::New`. If it does, please report the exact Citron/emulator version + config settings — every additional data point narrows the root cause.

**If you can localize inside `Isolate::New`:** trace fprintfs around the sub-steps in your isolate init call chain (V8's `Isolate::Init`, `SetupIsolateDelegate::SetupBuiltins`, snapshot deserialization) and post the localization. Downstream will help review.

## Watch items for future V8 bumps

1. **`gni/v8.gni:is_drumbrake_supported`** at each future V8 tag. If upstream widens the OS whitelist or drops the pointer-compression conjunct, patch 0010 becomes redundant / smaller and Track B may unblock spontaneously.
2. **`src/wasm/interpreter/wasm-interpreter.cc`** — audit for new `static_assert(COMPRESS_POINTERS_BOOL)` or equivalent PC-off compile gates before each V8 bump. Patch 0011 covers the one at v15.0.243; new ones added at later tags will fail to compile until guarded similarly.
3. **`src/wasm/wasm-engine.cc`** and **`src/execution/isolate.cc`** — new `V8_ENABLE_DRUMBRAKE` branches that reach the `Isolate::New` path may resolve OR worsen the boot hang. Retest after every V8 bump before republishing pkgrel=10 as a viable variant.

## Risks / non-goals

- **Root cause not localized past `Isolate::New`.** Merging this variant does not fix the boot hang under Citron.
- **Hardware behaviour not tested.** Downstream jitless-on-hardware is out of scope for the project that produced this PR (hardware runs full JIT). Any hardware regression is unknown territory.
- **Wasm-gc `array.fill` on ref-typed arrays hard-fails** under PC-off (see patch 0011). Not a change in observable behaviour — pkgrel=9 has no wasm interpreter at all, and pkgrel=10 with the boot hang has no wasm at all either. Only relevant once the boot hang is resolved.
- **Ref-returning wasm calls hit an upstream `CHECK(false); // Not supported`** at `wasm-interpreter-runtime.cc:2418` under PC-off. Left in place; upstream limitation, not this PR's.
- **Not for `pkgrel=9→10` default rollup.** If this branch is not adopted as-experimental-only and downstreams silently pull the new default, most Citron users boot-hang without recourse. Downstream policy is that pkgrel=10 stays parallel-track behind a manual opt-in until either the boot hang resolves or downstreams accept the risk.

## Retires ledger entries

None in the downstream ledger. Companion downstream note is a permanent deferred entry recording the boot-hang finding; this PR is what downstream can point at when re-testing under future V8 or Citron releases.
