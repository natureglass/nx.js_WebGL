#pragma once
#include "types.h"

typedef struct {
	HidNpadIdType id;
	PadState *pad;
} nx_gamepad_t;

typedef struct {
	nx_gamepad_t *gamepad;
	u64 mask;
	/* Original index this button was constructed for. Used by the
	 * getter to dispatch sentinel-mask buttons (Capture / HOME) to
	 * their applet-hook-backed transient flag instead of
	 * padGetButtons. */
	int idx;
} nx_gamepad_button_t;

nx_gamepad_t *nx_get_gamepad(JSContext *ctx, JSValueConst obj);

void nx_init_gamepad(JSContext *ctx, JSValueConst init_obj);
