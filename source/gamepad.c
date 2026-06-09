#include "gamepad.h"

static JSClassID nx_gamepad_class_id;
static JSClassID nx_gamepad_button_class_id;

/* Sentinel mask for buttons whose state isn't carried by padGetButtons —
 * specifically the Capture button (delivered via applet messages) and
 * the HOME button (typically intercepted by hbmenu / the OS). The
 * gamepad-button getter checks for this sentinel and dispatches to
 * the per-button "transient pressed" flag instead of masking
 * padGetButtons. */
#define NX_BTN_MASK_NONE ((u64)-1)

/* Button layout exposed to JS via gamepad.buttons[i]. Indices 0-15 are
 * the standard Web Gamepad layout; 16-19 add the sideways/single-joycon
 * SL/SR buttons (libnx delivers them through padGetButtons, so they
 * use real masks); 20 is the Capture button (delivered via
 * `AppletHookType_OnCaptureButtonShortPressed`); 21 is reserved for
 * the HOME button (intercepted by hbmenu in NRO context — exposed but
 * always reads as released). */
static u64 standard_button_masks[] = {
	HidNpadButton_B,       HidNpadButton_A,       HidNpadButton_Y,
	HidNpadButton_X,       HidNpadButton_L,       HidNpadButton_R,
	HidNpadButton_ZL,      HidNpadButton_ZR,      HidNpadButton_Minus,
	HidNpadButton_Plus,    HidNpadButton_StickL,  HidNpadButton_StickR,
	HidNpadButton_Up,      HidNpadButton_Down,    HidNpadButton_Left,
	HidNpadButton_Right,
	/* 16-19: side-joycon SL/SR (used in sideways / single-joycon mode) */
	HidNpadButton_LeftSL,  HidNpadButton_LeftSR,
	HidNpadButton_RightSL, HidNpadButton_RightSR,
	/* 20: Capture button — sentinel; serviced by capture_just_pressed */
	NX_BTN_MASK_NONE,
	/* 21: HOME button — sentinel; reserved (never fires under hbmenu) */
	NX_BTN_MASK_NONE,
};

#define NX_GAMEPAD_BUTTON_COUNT \
	((int)(sizeof(standard_button_masks) / sizeof(standard_button_masks[0])))

/* --- Capture button plumbing ---------------------------------------
 *
 * libnx exposes the Capture button via the applet message system
 * (`AppletHookType_OnCaptureButtonShortPressed`) rather than through
 * padGetButtons — the button is a system-level shortcut, not a
 * standard HID pad bit. We register a hook at gamepad-init time; the
 * callback raises a transient flag that the gamepad-button getter
 * reads and clears. Net effect from JS: pressing Capture causes
 * `gamepad.buttons[20].pressed` to return true on the FIRST read
 * after the press, then false again — which lines up with the
 * rising-edge detection both the mouse forwarder and the shell
 * shortcut router use.
 *
 * HOME is NOT plumbed today because hbmenu / HBL typically intercepts
 * it to return to the homebrew menu before our hook would run. Index
 * 21 stays exposed as a sentinel so JS code reading
 * `buttons.length === 22` doesn't trip up; it just always reads
 * false. A future wiring path would call
 * `hidsysAcquireHomeButtonEventHandle` and listen on a thread, with
 * fallback if the OS denies the handle. */
static bool capture_just_pressed = false;
static AppletHookCookie capture_hook_cookie;
static bool capture_hook_registered = false;

static void nx_gamepad_applet_hook(AppletHookType hook, void *param) {
	(void)param;
	if (hook == AppletHookType_OnCaptureButtonShortPressed) {
		capture_just_pressed = true;
	}
}

nx_gamepad_t *nx_get_gamepad(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_gamepad_class_id);
}

nx_gamepad_button_t *nx_get_gamepad_button(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_gamepad_button_class_id);
}

static void finalizer_gamepad(JSRuntime *rt, JSValue val) {
	nx_gamepad_t *gamepad = JS_GetOpaque(val, nx_gamepad_class_id);
	// nx_context_t *nx_ctx = JS_GetRuntimeOpaque(rt);
	if (gamepad) {
		// nx_ctx->pads[gamepad->id] = NULL;
		js_free_rt(rt, gamepad);
	}
}

static void finalizer_gamepad_button(JSRuntime *rt, JSValue val) {
	nx_gamepad_button_t *gamepad_button =
		JS_GetOpaque(val, nx_gamepad_button_class_id);
	if (gamepad_button) {
		js_free_rt(rt, gamepad_button);
	}
}

static JSValue nx_gamepad_new(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	u32 id;
	if (JS_ToUint32(ctx, &id, argv[0])) {
		return JS_EXCEPTION;
	}

	nx_gamepad_t *gamepad = js_mallocz(ctx, sizeof(nx_gamepad_t));
	if (!gamepad) {
		return JS_EXCEPTION;
	}

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	gamepad->id = id;
	gamepad->pad = &nx_ctx->pads[id];

	JSValue obj = JS_NewObjectClass(ctx, nx_gamepad_class_id);
	JS_SetOpaque(obj, gamepad);
	return obj;
}

static JSValue nx_gamepad_button_new(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, argv[0]);

	u32 index;
	if (JS_ToUint32(ctx, &index, argv[1])) {
		return JS_EXCEPTION;
	}
	if ((int)index >= NX_GAMEPAD_BUTTON_COUNT) {
		return JS_ThrowRangeError(ctx,
			"gamepad button index %u out of range (max %d)",
			index, NX_GAMEPAD_BUTTON_COUNT - 1);
	}

	nx_gamepad_button_t *gamepad_button =
		js_mallocz(ctx, sizeof(nx_gamepad_button_t));
	if (!gamepad_button) {
		return JS_EXCEPTION;
	}

	gamepad_button->gamepad = gamepad;
	gamepad_button->mask = standard_button_masks[index];
	gamepad_button->idx = (int)index;

	JSValue obj = JS_NewObjectClass(ctx, nx_gamepad_button_class_id);
	JS_SetOpaque(obj, gamepad_button);
	return obj;
}

static JSValue nx_gamepad_get_axes(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	JSValue arr = JS_NewArray(ctx);
	HidAnalogStickState analog_stick_l = padGetStickPos(gamepad->pad, 0);
	HidAnalogStickState analog_stick_r = padGetStickPos(gamepad->pad, 1);
	JS_SetPropertyUint32(
		ctx, arr, 0, JS_NewFloat64(ctx, (double)analog_stick_l.x / 32768.0));
	JS_SetPropertyUint32(
		ctx, arr, 1, JS_NewFloat64(ctx, (double)-analog_stick_l.y / 32768.0));
	JS_SetPropertyUint32(
		ctx, arr, 2, JS_NewFloat64(ctx, (double)analog_stick_r.x / 32768.0));
	JS_SetPropertyUint32(
		ctx, arr, 3, JS_NewFloat64(ctx, (double)-analog_stick_r.y / 32768.0));
	return arr;
}

static JSValue nx_gamepad_get_id(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	// nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewString(ctx, "");
}

static JSValue nx_gamepad_get_raw_buttons(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewBigUint64(ctx, padGetButtons(gamepad->pad));
}

static JSValue nx_gamepad_get_device_type(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewUint32(ctx, hidGetNpadDeviceType(gamepad->id));
}

static JSValue nx_gamepad_get_style_set(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewUint32(ctx, padGetStyleSet(gamepad->pad));
}

static JSValue nx_gamepad_get_index(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewUint32(ctx, gamepad->id);
}

static JSValue nx_gamepad_get_connected(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
	return JS_NewBool(ctx, padIsConnected(gamepad->pad));
}

/* Resolve the "is this button currently pressed?" state for one
 * gamepad button. Honours the sentinel mask used by Capture (idx 20)
 * and HOME (idx 21): for those, padGetButtons doesn't carry the bit
 * and we read the applet-hook-backed transient flag instead. The
 * Capture read is destructive — once consumed, the flag clears, so
 * the rising-edge detection in JS only fires once per physical press. */
static bool nx_gamepad_button_resolve_pressed(nx_gamepad_button_t *gb) {
	if (gb->mask == NX_BTN_MASK_NONE) {
		if (gb->idx == 20) {
			bool was = capture_just_pressed;
			capture_just_pressed = false;
			return was;
		}
		/* idx 21 = HOME — not currently wired (hbmenu intercepts). */
		return false;
	}
	u64 kDown = padGetButtons(gb->gamepad->pad);
	return (kDown & gb->mask) != 0;
}

static JSValue nx_gamepad_button_get_pressed(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
	return JS_NewBool(ctx, nx_gamepad_button_resolve_pressed(gamepad_button));
}

static JSValue nx_gamepad_button_get_touched(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
	return JS_NewBool(ctx, nx_gamepad_button_resolve_pressed(gamepad_button));
}

static JSValue nx_gamepad_button_get_value(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
	return JS_NewUint32(ctx, nx_gamepad_button_resolve_pressed(gamepad_button) ? 1 : 0);
}

static JSValue nx_gamepad_init_class(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "axes", nx_gamepad_get_axes);
	NX_DEF_GET(proto, "id", nx_gamepad_get_id);
	NX_DEF_GET(proto, "index", nx_gamepad_get_index);
	NX_DEF_GET(proto, "connected", nx_gamepad_get_connected);

	// Non-standard
	NX_DEF_GET(proto, "deviceType", nx_gamepad_get_device_type);
	NX_DEF_GET(proto, "rawButtons", nx_gamepad_get_raw_buttons);
	NX_DEF_GET(proto, "styleSet", nx_gamepad_get_style_set);

	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_gamepad_button_init_class(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "pressed", nx_gamepad_button_get_pressed);
	NX_DEF_GET(proto, "touched", nx_gamepad_button_get_touched);
	NX_DEF_GET(proto, "value", nx_gamepad_button_get_value);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("gamepadInit", 1, nx_gamepad_init_class),
	JS_CFUNC_DEF("gamepadNew", 1, nx_gamepad_new),
	JS_CFUNC_DEF("gamepadButtonInit", 1, nx_gamepad_button_init_class),
	JS_CFUNC_DEF("gamepadButtonNew", 1, nx_gamepad_button_new),
};

void nx_init_gamepad(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_gamepad_class_id);
	JSClassDef gamepad_class = {
		"Gamepad",
		.finalizer = finalizer_gamepad,
	};
	JS_NewClass(rt, nx_gamepad_class_id, &gamepad_class);

	JS_NewClassID(rt, &nx_gamepad_button_class_id);
	JSClassDef gamepad_button_class = {
		"GamepadButton",
		.finalizer = finalizer_gamepad_button,
	};
	JS_NewClass(rt, nx_gamepad_button_class_id, &gamepad_button_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));

	/* Subscribe to applet messages so the Capture button (index 20)
	 * propagates into the gamepad-button state. Idempotent — guarded
	 * by `capture_hook_registered`. The hook is process-wide and
	 * survives across contexts; we never unsubscribe (the cookie is
	 * static).
	 */
	if (!capture_hook_registered) {
		appletHook(&capture_hook_cookie, nx_gamepad_applet_hook, NULL);
		capture_hook_registered = true;
	}
}
