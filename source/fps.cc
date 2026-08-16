// Engine-side FPS overlay compositor — see fps.h for the contract/architecture.
//
// Measurement: an exponential moving average of the inter-present interval
// (armGetSystemTick deltas). EMA keeps the displayed integer stable without a
// separate "refresh every N ms" gate. The first present after boot just seeds
// the previous tick (no dt yet).
//
// Draw: a semi-transparent rounded box at the top-left with the FPS integer
// followed by " FPS", rendered from a built-in 5x7 bitmap font (no SkTypeface
// dependency — see fps.h). Blended onto the passed-in back-buffer SkSurface
// only; the persistent canvas surface is never touched (same discipline as the
// cursor compositor), so there's no trail and no interference with the canvas
// fast path.

#include "fps.h"

#include <stdint.h>
#include <stdio.h>
#include <switch.h>  // armGetSystemTick, armTicksToNs

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"

namespace {

struct FpsState {
	bool enabled = false;
	bool started = false;
	uint64_t window_start_tick = 0;  // armGetSystemTick at the window's start
	uint32_t frames = 0;             // presents counted in the current window
	int display_fps = 0;             // value computed at the last 1s boundary
};

FpsState s_state;

// --- 5x7 bitmap font -------------------------------------------------------
// Each glyph is 7 rows; the low 5 bits of each byte are the columns
// (bit 4 = leftmost). Only the characters the overlay needs are defined:
// digits 0-9, 'F', 'P', 'S', and space. Anything else renders as blank.
const uint8_t GLYPH_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
};
const uint8_t GLYPH_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
const uint8_t GLYPH_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
const uint8_t GLYPH_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};

// Return the 7-row glyph for `ch`, or nullptr for blank (space / unknown).
const uint8_t *glyph_for(char ch) {
	if (ch >= '0' && ch <= '9') return GLYPH_DIGITS[ch - '0'];
	if (ch == 'F') return GLYPH_F;
	if (ch == 'P') return GLYPH_P;
	if (ch == 'S') return GLYPH_S;
	return nullptr;  // space + anything else
}

// Draw one glyph at (x, y) with `px`-sized square pixels.
void draw_glyph(SkCanvas *c, char ch, float x, float y, float px,
                const SkPaint &paint) {
	const uint8_t *g = glyph_for(ch);
	if (!g) return;
	for (int row = 0; row < 7; row++) {
		const uint8_t bits = g[row];
		for (int col = 0; col < 5; col++) {
			if (bits & (1 << (4 - col))) {
				c->drawRect(
				    SkRect::MakeXYWH(x + col * px, y + row * px, px, px),
				    paint);
			}
		}
	}
}

}  // namespace

void nx_fps_set_enabled(bool enabled) { s_state.enabled = enabled; }

void nx_fps_composite(SkSurface *target) {
	// --- Advance the measurement (runs every present, enabled or not) ------
	// Count presents over a 1-second window and recompute the displayed number
	// only when the window closes, so the readout refreshes once per second
	// rather than changing every frame. Measuring even while disabled means
	// enabling the overlay shows an at-most-1s-old value immediately.
	const uint64_t now = armGetSystemTick();
	if (!s_state.started) {
		s_state.started = true;
		s_state.window_start_tick = now;
		s_state.frames = 0;
	}
	s_state.frames++;
	const double elapsed_ms =
	    (double)armTicksToNs(now - s_state.window_start_tick) / 1.0e6;
	if (elapsed_ms >= 1000.0) {
		s_state.display_fps =
		    (int)(s_state.frames * 1000.0 / elapsed_ms + 0.5);
		s_state.window_start_tick = now;
		s_state.frames = 0;
	}

	if (!s_state.enabled || !target) return;

	SkCanvas *c = target->getCanvas();
	if (!c) return;

	// --- Displayed integer (last full-second measurement) ------------------
	int fps = s_state.display_fps;
	if (fps < 0) fps = 0;
	if (fps > 999) fps = 999;

	// "<n> FPS" — at most "999 FPS" = 7 chars.
	char text[8];
	const int n = snprintf(text, sizeof(text), "%d FPS", fps);
	const int len = (n < 0) ? 0 : (n < (int)sizeof(text) - 1 ? n
	                                                          : (int)sizeof(text) - 1);

	// --- Layout (bottom-left) ----------------------------------------------
	const float px = 3.0f;         // bitmap-pixel size
	const float glyph_w = 5 * px;  // 15
	const float glyph_h = 7 * px;  // 21
	const float advance = glyph_w + px;  // one blank column between glyphs
	const float pad = 6.0f;
	const float margin = 8.0f;
	const float text_w = len > 0 ? len * glyph_w + (len - 1) * px : 0.0f;
	const float box_w = text_w + 2 * pad;
	const float box_h = glyph_h + 2 * pad;
	const float box_x = margin;
	const float box_y = (float)target->height() - box_h - margin;

	// --- Box (semi-transparent black, rounded) -----------------------------
	SkPaint box;
	box.setAntiAlias(true);
	box.setColor(SkColorSetARGB(150, 0, 0, 0));
	c->drawRoundRect(SkRect::MakeXYWH(box_x, box_y, box_w, box_h), 5.0f, 5.0f,
	                 box);

	// --- Text (opaque bright green — classic overlay, reads over any bg) ---
	SkPaint ink;
	ink.setAntiAlias(false);  // crisp blocky pixels
	ink.setColor(SkColorSetARGB(255, 120, 230, 120));
	float gx = box_x + pad;
	const float gy = box_y + pad;
	for (int i = 0; i < len; i++) {
		draw_glyph(c, text[i], gx, gy, px, ink);
		gx += advance;
	}
}
