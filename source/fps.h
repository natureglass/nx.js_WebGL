// Engine-side FPS overlay compositor. Sibling to cursor.cc — a small
// semi-transparent box with the current present-rate FPS, blended onto the
// EGL back-buffer at present time so it survives page navigation, every
// in-runtime app (including WebGL, whose content is read back into the same
// canvas surface), and stretches where JS is fully blocked.
//
//   JS side                         C side (this module)
//   ──────────                      ───────────────────
//   screen.setFpsOverlayEnabled ──► nx_fps_set_enabled
//                                   nx_fps_composite(SkSurface*)
//                                     ▲
//                                     │ called from nx_skia_gpu_present
//                                     │ AFTER the cursor composite, BEFORE
//                                     │ flush+submit+swap (drawn on top).
//
// The FPS number is measured C-side by counting presents over a 1-second
// window (armGetSystemTick), so it reflects the true vblank-locked present
// rate independent of how often JS repaints, and the readout refreshes once
// per second. Measurement runs on every present regardless of the enabled
// flag, so toggling the overlay on shows an at-most-1s-old value immediately.
//
// Text is drawn with a built-in 5x7 bitmap font (digits + "FPS") rather than
// an SkTypeface: the Switch has no system font manager (SkFontMgr_empty), so
// canvas text only renders once a font is loaded via CSS. A self-contained
// bitmap font keeps this overlay dependency-free and always renderable.

#pragma once
#include <stdbool.h>

class SkSurface;  // Skia forward decl — header-only consumers don't need the
                  // full SkSurface include.

// Show or hide the overlay. Cheap; flips a bool. Measurement continues either
// way so the first drawn frame after enabling is already accurate.
void nx_fps_set_enabled(bool enabled);

// Advance the present-rate measurement and, when enabled, composite the FPS
// box onto `target` (the EGL back-buffer SkSurface passed in by skia_gpu.cc's
// present hook). No-op draw when disabled or `target` is null.
void nx_fps_composite(SkSurface *target);
