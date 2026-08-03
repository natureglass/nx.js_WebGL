# PR-A — Defer scheme resolution to `globalThis.fetch` in Image/Audio/Video

**Branch:** `upstream-pr/A-fetch-deferral`
**Base:** `upstream/main` (`34d2d03`)
**Local worktree:** `D:/tmp/pr-drafts/PR-A`
**Local commit:** `ce83520` — "runtime: defer scheme resolution to globalThis.fetch in image/audio/video"
**Retires ledger entries:** #1, #2, #3

## PR title (suggested)

`runtime: defer scheme resolution to globalThis.fetch in Image/Audio/Video`

## PR body

`Image.src`, `Audio.src`, and `Video.src` setters today do
`import { fetch } from './fetch/fetch'` at module init and capture
that fetch closure. `./fetch/fetch`'s scheme registry is fixed at
`http/https/blob/data/file/sdmc/romfs`.

Embedders that install a session-time `globalThis.fetch` wrapper to
extend the scheme registry (e.g. a custom `app://` scheme handler,
or a resource-loader that resolves paths against an in-memory
package) will find that `<img src="app://foo.png">` still rejects at
scheme lookup — the `Image.src` setter is calling the pre-wrapper
`./fetch/fetch`, not the embedder's `globalThis.fetch`.

Move the setter's fetch reference to a call-time `globalThis.fetch`
lookup. Each of the three modules gets a local:

```ts
function fetch(
    input: string | URL | Request,
    init?: RequestInit,
): Promise<Response> {
    return globalThis.fetch(input, init);
}
```

which replaces the previous `import { fetch } from './fetch/fetch'`.
The call sites in the setters are unchanged (`fetch(url).then(...)`).

## Behavior

- No change for embedders that don't override `globalThis.fetch` —
  the engine's global fetch is `./fetch/fetch`'s exported fetch by
  default, so the delegation chain terminates at the same
  implementation.
- Embedders that DO override `globalThis.fetch` now have their
  wrapper honored on `Image.src`/`Audio.src`/`Video.src` reads.

## Gotcha (worth calling out in review)

The lookup MUST be call-time, not import-time. An `import { fetch }
from './fetch/fetch'` line captures the export at module init —
before any embedder installs its wrapper. Replacing it with `const
fetch = globalThis.fetch;` at module top-level would freeze the
pre-wrapper fetch and reintroduce the bug in code that looks like
it should work. The function-body form guarantees the lookup
happens per-call.

## Diff summary

```
 packages/runtime/src/audio.ts | 14 +++++++++++++-
 packages/runtime/src/image.ts | 14 +++++++++++++-
 packages/runtime/src/video.ts | 15 ++++++++++++++-
 3 files changed, 40 insertions(+), 3 deletions(-)
```

Each file: replaces one `import { fetch } from './fetch/fetch';`
line with a ~12-line local function definition + explanatory
comment.

## Build / type-check status against upstream

The change is TS-only, three localized imports removed and one
local function per file added. Type-checking against upstream's
`packages/runtime/tsconfig.json` succeeds locally (the local
`fetch` function's signature matches the previous imported one).
No changes to any `.d.ts` public surface. No native code touched.

## Interaction with other PRs

- **PR-F** (JIT-safe defineProperties): independent files. No
  merge order preference.
- **PR-C** (canvas.cc font-size pin): independent files. No merge
  order preference.
- **PR-D** (Skia/WebGL coexistence): touches `screen.ts`,
  `webgl*.ts`, `webgl.cc`, `skia_gpu.{cc,h}`, new
  `webgl_bridge.{cc,h}`. No overlap with PR-A. No merge order
  preference.

## Downstream implication (for the fork's records)

Retires ledger entries #1/#2/#3 upon merge. Removes three
upstream-file deltas from the modified-upstream-file count
(image.ts, audio.ts, video.ts). Highest ratio of
(retired-entries × merge-likelihood) / effort of any pending PR.
