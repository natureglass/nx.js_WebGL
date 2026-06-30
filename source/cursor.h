// Engine-side cursor compositor (V8 migration re-port of QuickJS-era
// `composite_cursor_overlay`, NXJS_PATCHES_NEEDED.md #4).
//
// Architecture:
//
//   JS side                       C side (this module)
//   ──────────                    ───────────────────
//   screen.setCursorOverlay   ──► nx_cursor_set_static
//   screen.setAnimated...     ──► nx_cursor_set_animated
//   screen.setCursorOverlay   ──► nx_cursor_set_position
//     Position
//   screen.clearCursorOverlay ──► nx_cursor_clear
//                                 nx_cursor_composite(SkSurface*)
//                                   ▲
//                                   │ called from nx_skia_gpu_present
//                                   │ AFTER canvas blit, BEFORE
//                                   │ flush+submit+swap
//
// Cursor pixels are blitted onto the EGL back-buffer SkSurface only.
// The persistent canvas surface (s_canvas in skia_gpu.cc) NEVER has
// cursor pixels written into it, so:
//   - No cursor trail (next frame's blit gives a clean canvas).
//   - No interference with the canvas fast path's blitCacheUnder
//     mechanism (the stale-snapshot fuzz the runtime fallback's
//     forced slow path exposed).
//   - Animated cursors (wait/progress spinners) advance via
//     armGetSystemTick() inside `nx_cursor_composite` — runs every
//     present, independent of JS being blocked on sync chunks like
//     navigateTo's grid build.
//
// The runtime fallback in brewser-runtime-v8/src/input/page-mouse-
// forwarder.ts auto-defers to this engine path the moment the four
// JS bindings exist on the Screen prototype (its
// `probeNativeCursorOverlay` feature-detect flips a sticky bool
// after the first detection). No runtime-side changes required to
// flip the feature on.

#pragma once
#include <stdbool.h>
#include <stdint.h>

class SkSurface;  // Skia forward decl — header-only consumers don't need the
                  // full SkSurface include.

// Replace any prior cursor with a static (single-frame) overlay.
//
//   x, y         — top-left position on the screen (clipped at composite time)
//   rgba         — width*height*4 bytes, NON-PREMULTIPLIED RGBA (per ImageData
//                  / canvas.getImageData spec); copied into our owned buffer
//                  (no caller lifetime dependency); premultiplied at copy time
//                  for SkPremul_AlphaType compatibility with Skia's drawImage.
//   width,height — cursor bitmap dimensions in pixels.
void nx_cursor_set_static(int32_t x, int32_t y,
                          const uint8_t *rgba,
                          uint32_t width, uint32_t height);

// Replace any prior cursor with an animated overlay.
//
//   packed_rgba    — frame_count frames back-to-back, each (width*height*4)
//                    bytes, non-premultiplied. Layout matches the runtime
//                    cursor registry's `CursorAsset.packedRgba` so the JS
//                    side can hand the buffer over zero-rearrangement.
//   frame_count    — 1..64 frames. Larger values are rejected (cursor cleared)
//                    as a sanity bound on memory use.
//   delays_ms      — frame_count u16 per-frame display durations.
//
// Composite picks the current frame from `armGetSystemTick()` elapsed-ms
// modulo the sum of delays_ms, walking delays_ms to find the index — same
// O(frame_count) math as the QuickJS-era compositor.
void nx_cursor_set_animated(int32_t x, int32_t y,
                            const uint8_t *packed_rgba,
                            uint32_t width, uint32_t height,
                            uint32_t frame_count,
                            const uint16_t *delays_ms);

// Cheap position update. No RGBA upload, no SkImage rebuild. Called per
// cursor-move tick from the runtime cursor sync; common case.
void nx_cursor_set_position(int32_t x, int32_t y);

// Disable the cursor. Subsequent `nx_cursor_composite` calls no-op until the
// next `nx_cursor_set_*`. Releases the frame storage + cached SkImages.
void nx_cursor_clear(void);

// Composite the current cursor frame onto `target` (the EGL back-buffer
// SkSurface passed in by skia_gpu.cc's present hook). No-op when cursor is
// cleared or `target` is null. Picks the current animated frame from
// `armGetSystemTick()` so the animation advances at present rate independent
// of JS state. Cheap on idle frames (constant-time check + one drawImage).
void nx_cursor_composite(SkSurface *target);

// Teardown — frees frame storage + cached SkImages. Called from
// nx_skia_gpu_screen_exit so SkImage handles release BEFORE the
// GrDirectContext they were built on goes away.
void nx_cursor_exit(void);
