# PR-F — Use `Object.defineProperties` bulk shape for WebGL2 constants install

**Branch:** `upstream-pr/F-jit-safe-defineproperties`
**Base:** `upstream/main` (`34d2d03`)
**Local worktree:** `D:/tmp/pr-drafts/PR-F`
**Local commit:** `f8bf7ff` — "runtime/webgl2: use Object.defineProperties bulk shape for GL_CONSTANTS install"
**Retires ledger entries:** #8

## PR title (suggested)

`runtime/webgl2: use Object.defineProperties bulk shape for GL_CONSTANTS install`

## PR body

The current constants install at the bottom of
`packages/runtime/src/canvas/webgl2-rendering-context.ts` uses a
per-key loop:

```ts
for (const [k, v] of Object.entries(GL_CONSTANTS)) {
    Object.defineProperty(WebGL2RenderingContext, k, { value: v });
    Object.defineProperty(WebGL2RenderingContext.prototype, k, { value: v });
}
```

with ~370 constants × 2 targets = ~740 `defineProperty` calls at
module-body scope, back-to-back.

We've observed this pattern trip V8/aarch64 JIT tier-up
(Sparkplug/Maglev) at boot in a downstream deployment (running V8
on Switch Tegra X1 silicon under libnx). Symptom: engine boots
past V8 init, then dies silently during runtime.js evaluation
before any application code runs — the crash is async with respect
to the interpreter because tier-up compilation is decoupled from
interpreter execution. Ignition-only (with JIT disabled) works
fine.

Bulk `Object.defineProperties(target, descs)` builds the descriptor
map first, then installs all properties in a single call per
target. Byte-identical property shape:
`writable=enumerable=configurable=false` when only `value` is set,
which matches the previous per-key defineProperty behavior. V8
handles the single bulk call cleanly.

## Diff

```ts
{
    const keys = Object.keys(GL_CONSTANTS);
    const descs: PropertyDescriptorMap = {};
    for (let i = 0; i < keys.length; i++) {
        const k = keys[i];
        descs[k] = { value: (GL_CONSTANTS as Record<string, number>)[k] };
    }
    Object.defineProperties(WebGL2RenderingContext, descs);
    Object.defineProperties(WebGL2RenderingContext.prototype, descs);
}
```

Wrapped in a block scope to keep `keys`/`descs` from leaking as
module-level bindings.

## Behavior

- No change to any consumer. Instances and the class still expose
  the constants as own-properties with the same descriptor
  attributes (`writable=enumerable=configurable=false`,
  `value=<GLenum>`).
- Downstream reads (`gl.TRIANGLES`, `gl.VERTEX_SHADER`, etc.)
  observe the same shape they did before.

## Framing

The upstream V8 codegen bug on aarch64 with tight defineProperty
loops at this scale is not fixed by this PR — this PR sidesteps
the bad codegen path by structuring the install as a single call.
The bug is worth reporting to V8 upstream (a minimal repro is a
module-body function running `for (const [k,v] of Object.entries(
BIG_OBJ))` calling `Object.defineProperty(target, k, {value: v})`
1000+ times, on aarch64 with full JIT). Independent of that, the
bulk-defineProperties shape is a robustness improvement for any
V8 embedder that runs runtime bundles with large constant tables.

## Diff summary

```
 packages/runtime/src/canvas/webgl2-rendering-context.ts | 20 +++++++++++++++-----
 1 file changed, 17 insertions(+), 3 deletions(-)
```

## Build / type-check status

TS-only change; single file. Type-checks against upstream's
tsconfig locally. No behavior change; no test surface change.

## Interaction with other PRs

- **PR-A/PR-C**: independent files. No merge order preference.
- **PR-D** (Skia/WebGL coexistence): PR-D expands the runtime's
  WebGL surface by adding a v1 context and reusing the same
  constants-install pattern. **PR-F should merge before PR-D** so
  PR-D can inherit the bulk-defineProperties shape rather than
  reintroducing the per-key loop. If PR-F merges first, PR-D's
  v1 context install path follows the same bulk shape by
  imitation.

## Downstream implication

Retires ledger entry #8. Downstream had shipped the same fix in
the fork; on merge, that fork-only edit rejoins upstream.
