# PR-C — Pin FT_Face char_size at start of fillText/strokeText/measureText

**Branch:** `upstream-pr/C-fonface-charsize-pin`
**Base:** `upstream/main` (`34d2d03`)
**Local worktree:** `D:/tmp/pr-drafts/PR-C`
**Local commit:** `d150865` — "canvas: pin FT_Face char_size at start of fillText/strokeText/measureText"
**Retires ledger entries:** #13

## PR title (suggested)

`canvas: pin FT_Face char_size at start of fillText/strokeText/measureText`

## PR body

`FontFace` construction allocates one `FT_Face` + one `hb_font` per
JS `FontFace` object (see `source/font.cc`). All 2D canvas contexts
that resolve to the same `FontFace` via `findFont` (in
`packages/runtime/src/font/font-face-set.ts`) end up sharing those
FreeType and HarfBuzz instances via their `state->ft_face` /
`state->hb_font` pointers.

That sharing has a subtle correctness bug in cross-context
save/restore. The `state->font_size` value is per-context
(preserved by save/restore); but `FT_Face.char_size` is
device-global. Any `set_font_size` call — including the implicit
one at the end of `nx_canvas_context_2d_restore` — mutates the
shared FT_Face's char_size in place, visible to every other
context holding the same FontFace.

Concrete scenario:

1. Context B: `ctx.font = '14px system-ui'`. Setter calls
   `set_font_size(14)`. FT_Face.char_size = 14 × 64.
2. Context A: `ctx.save()`. Pushes a state copy.
3. Context A: `ctx.font = '20px system-ui'`. Setter finds the same
   FontFace, calls `set_font_size(20)`. FT_Face.char_size = 20 × 64.
4. Context A: `ctx.fillText(...)`. Draws at 20 px.
5. Context A: `ctx.restore()`. Pops the state; at the bottom,
   restores `set_font_size(outer.font_size)` — where `outer.font_size
   = 10` (the canvas-state default). FT_Face.char_size = 10 × 64.
6. Context B: `ctx.font = '14px system-ui'` — the JS-side setter
   short-circuits because `this.font === '14px system-ui'` (font
   string unchanged). No `set_font_size` fires.
7. Context B: `ctx.fillText(...)`. Reads glyph metrics from the
   shared FT_Face with char_size = 10 (Context A's leftover).
   Renders at 10 px instead of 14.

The specific consumer that hit this in production is a
`setInterval`-driven `<canvas>` in one embedder's shell painter,
concurrent with a `save`/`restore` in a different context. Both
contexts default to `'14px system-ui'` and both should draw at 14;
the second consistently renders at 10 after the first `restore`
fires.

## Fix

Add one line at the start of each of the three text-op functions:

```cpp
set_font_size(context, context->state->font_size);
```

Idempotent against the state's font_size (calls `FT_Set_Char_Size`
on a cached face; sub-microsecond) — restores the per-context
scale to the FT_Face immediately before shaping/measurement, so
whatever the last cross-context leftover was is overwritten.

## Diff

```
 source/canvas.cc | 13 +++++++++++++
 1 file changed, 13 insertions(+)
```

One re-pin call added at three sites:
- `nx_canvas_context_2d_fill_text` (right after argument parsing)
- `nx_canvas_context_2d_stroke_text` (right after argument parsing)
- `nx_canvas_context_2d_measure_text` (right after the hb_font
  guard)

## Minimum repro

```js
const a = new OffscreenCanvas(200, 50).getContext('2d');
const b = new OffscreenCanvas(200, 50).getContext('2d');
a.font = '14px system-ui';
b.font = '14px system-ui';

// A does a save/font-change/restore
a.save();
a.font = '20px system-ui';
a.fillText('A', 10, 30);
a.restore();

// B should draw at 14, but reads FT_Face char_size = 10 (canvas
// default) left behind by A's restore.
b.fillText('B', 10, 30);
```

Before this PR: B's "B" renders visibly smaller than A's "A" —
about 10-px tall instead of 14. After: same visual size as A's,
matching the state's font_size.

## Cost

One `FT_Set_Char_Size` + one `hb_font_set_scale` per fillText /
strokeText / measureText. Both are cache hits on any face that's
been used at least once — sub-microsecond in practice, negligible
next to the `SkCanvas::drawGlyphs` call that follows.

## Diff summary

```
 source/canvas.cc | 13 +++++++++++++
 1 file changed, 13 insertions(+)
```

## Build / type-check status

Native C++ change; single file. Compiles against upstream
`source/canvas.cc` locally. No public-API delta.

## Interaction with other PRs

- **PR-A / PR-F**: independent files. No merge order preference.
- **PR-D** (Skia/WebGL coexistence): independent — PR-D touches
  `webgl.cc` / `webgl_bridge.cc` / `skia_gpu.cc` and only
  `canvas.cc` for the cursor overlay wiring, which is a
  fork-specific addition. If PR-D lands first, PR-C applies
  cleanly on top.

## Downstream implication

Retires ledger entry #13. Downstream ships the fix in the fork
today; on merge that fork-only edit rejoins upstream.
