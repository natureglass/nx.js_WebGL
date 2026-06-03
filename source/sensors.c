// On-demand hardware sensor bindings for the Switch diagnostics page.
//
// Design notes:
//
//   - All services are lazy-initialized on first use and left initialized
//     for the rest of the process; the per-call cost is just an IPC round
//     trip. A page that never calls these functions pays nothing.
//
//   - Functions return native errors as JS exceptions; the JS wrapper
//     (Switch.diagnostics) catches them and maps to nice strings so the
//     page can show "unavailable" without crashing.
//
//   - The six-axis sensor + NFP need explicit start/stop because they
//     consume controller-side resources; the JS wrapper exposes them as
//     init/read/exit triplets and the page calls them on demand.

#include "error.h"
#include "sensors.h"

// ---------- Battery extras (psm) -----------------------------------------

static bool s_psm_inited = false;

static Result ensure_psm(void) {
	if (s_psm_inited) return 0;
	Result rc = psmInitialize();
	if (R_SUCCEEDED(rc)) s_psm_inited = true;
	return rc;
}

static JSValue nx_sensors_battery_info(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	Result rc = ensure_psm();
	if (R_FAILED(rc)) return nx_throw_libnx_error(ctx, rc, "psmInitialize()");

	PsmBatteryChargeInfoFields fields;
	rc = psmGetBatteryChargeInfoFields(&fields);
	if (R_FAILED(rc))
		return nx_throw_libnx_error(ctx, rc, "psmGetBatteryChargeInfoFields()");

	double age_pct = 0.0;
	(void)psmGetBatteryAgePercentage(&age_pct);
	double raw_charge = 0.0;
	(void)psmGetRawBatteryChargePercentage(&raw_charge);

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "voltageMv",
					  JS_NewUint32(ctx, fields.battery_charge_milli_voltage));
	JS_SetPropertyStr(ctx, obj, "temperatureC",
					  JS_NewFloat64(ctx, fields.temperature_celcius / 1000.0));
	JS_SetPropertyStr(ctx, obj, "agePercent", JS_NewFloat64(ctx, age_pct));
	JS_SetPropertyStr(ctx, obj, "chargePercent", JS_NewFloat64(ctx, raw_charge));
	JS_SetPropertyStr(ctx, obj, "charging",
					  JS_NewBool(ctx, fields.battery_charging));
	JS_SetPropertyStr(ctx, obj, "chargerType",
					  JS_NewUint32(ctx, (u32)fields.charger_type));
	JS_SetPropertyStr(
		ctx, obj, "inputCurrentLimitMa",
		JS_NewUint32(ctx, fields.input_current_limit));
	JS_SetPropertyStr(
		ctx, obj, "fastChargeCurrentLimitMa",
		JS_NewUint32(ctx, fields.fast_charge_current_limit));
	JS_SetPropertyStr(
		ctx, obj, "chargeVoltageLimitMv",
		JS_NewUint32(ctx, fields.charge_voltage_limit));
	return obj;
}

// ---------- Audio control (audctl + audout) ------------------------------

static bool s_audctl_inited = false;
static bool s_audout_inited = false;

static Result ensure_audctl(void) {
	if (s_audctl_inited) return 0;
	Result rc = audctlInitialize();
	if (R_SUCCEEDED(rc)) s_audctl_inited = true;
	return rc;
}

static Result ensure_audout(void) {
	if (s_audout_inited) return 0;
	Result rc = audoutInitialize();
	if (R_SUCCEEDED(rc)) s_audout_inited = true;
	return rc;
}

static JSValue nx_sensors_audio_info(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);

	// Master volume on the speaker target (0..100 typically).
	s32 vol_out = 0;
	bool vol_ok = false;
	if (R_SUCCEEDED(ensure_audctl())) {
		Result vrc = audctlGetTargetVolume(&vol_out, AudioTarget_Speaker);
		vol_ok = R_SUCCEEDED(vrc);
	}
	JS_SetPropertyStr(ctx, obj, "masterVolume",
					  vol_ok ? JS_NewInt32(ctx, vol_out) : JS_NULL);

	// Audio output state + first device name + headphones detection.
	// audoutListAudioOuts returns a packed buffer of NUL-terminated
	// device names. "AudioStereoJackOutput" indicates 3.5mm jack
	// (headphones); "AudioTvOutput" = dock HDMI; "AudioBuiltInSpeakerOutput"
	// = handheld speakers.
	JSValue devs = JS_NewArray(ctx);
	bool headphones = false;
	const char *primary_name = NULL;
	char namebuf[1024];
	if (R_SUCCEEDED(ensure_audout())) {
		u32 count = 0;
		Result rc =
			audoutListAudioOuts(namebuf, sizeof(namebuf) / 0x100, &count);
		if (R_SUCCEEDED(rc)) {
			const char *p = namebuf;
			for (u32 i = 0; i < count; i++) {
				size_t len = strnlen(p, 0x100);
				JS_SetPropertyUint32(ctx, devs, i,
									 JS_NewStringLen(ctx, p, len));
				if (strstr(p, "StereoJack")) headphones = true;
				if (i == 0) primary_name = p;
				p += 0x100;
			}
		}
	}
	JS_SetPropertyStr(ctx, obj, "devices", devs);
	JS_SetPropertyStr(ctx, obj, "headphonesConnected",
					  JS_NewBool(ctx, headphones));
	JS_SetPropertyStr(
		ctx, obj, "primaryDevice",
		primary_name ? JS_NewString(ctx, primary_name) : JS_NULL);
	return obj;
}

// ---------- Wi-Fi info (wlaninf) -----------------------------------------

static bool s_wlaninf_inited = false;

static Result ensure_wlaninf(void) {
	if (s_wlaninf_inited) return 0;
	Result rc = wlaninfInitialize();
	if (R_SUCCEEDED(rc)) s_wlaninf_inited = true;
	return rc;
}

static JSValue nx_sensors_wlan_info(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);
	if (R_FAILED(ensure_wlaninf())) {
		JS_SetPropertyStr(ctx, obj, "available", JS_FALSE);
		return obj;
	}
	JS_SetPropertyStr(ctx, obj, "available", JS_TRUE);

	s32 rssi = 0;
	if (R_SUCCEEDED(wlaninfGetRSSI(&rssi))) {
		JS_SetPropertyStr(ctx, obj, "rssi", JS_NewInt32(ctx, rssi));
	} else {
		JS_SetPropertyStr(ctx, obj, "rssi", JS_NULL);
	}

	WlanInfState st = 0;
	if (R_SUCCEEDED(wlaninfGetState(&st))) {
		JS_SetPropertyStr(ctx, obj, "state", JS_NewUint32(ctx, (u32)st));
	} else {
		JS_SetPropertyStr(ctx, obj, "state", JS_NULL);
	}
	return obj;
}

// ---------- Six-axis sensor (hid: gyro / accel / fused angle) ------------

#define SIXAXIS_MAX_HANDLES 2
static HidSixAxisSensorHandle s_sixaxis_handles[SIXAXIS_MAX_HANDLES];
static s32 s_sixaxis_handle_count = 0;
static bool s_sixaxis_started = false;

// hid is initialized at process boot by libnx + main.c, so no ensure_hid()
// needed — just guard the per-controller start/stop.
static JSValue nx_sensors_sixaxis_start(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	if (s_sixaxis_started) return JS_NewBool(ctx, true);

	// Try handheld first (most common when running on the console itself).
	HidNpadIdType npad_id = HidNpadIdType_Handheld;
	HidNpadStyleTag style = HidNpadStyleTag_NpadHandheld;
	s32 total = 1;
	Result rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
										   style);
	if (R_FAILED(rc)) {
		// Fall back to docked dual Joy-Con on player 1.
		npad_id = HidNpadIdType_No1;
		style = HidNpadStyleTag_NpadJoyDual;
		total = 2;
		rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
										style);
	}
	if (R_FAILED(rc)) {
		// Last resort: Pro Controller on No1.
		npad_id = HidNpadIdType_No1;
		style = HidNpadStyleTag_NpadFullKey;
		total = 1;
		rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
										style);
	}
	if (R_FAILED(rc))
		return nx_throw_libnx_error(ctx, rc, "hidGetSixAxisSensorHandles()");

	s_sixaxis_handle_count = total;
	for (s32 i = 0; i < total; i++) {
		Result srt = hidStartSixAxisSensor(s_sixaxis_handles[i]);
		if (R_FAILED(srt)) {
			// Roll back any handles we already started.
			for (s32 j = 0; j < i; j++)
				hidStopSixAxisSensor(s_sixaxis_handles[j]);
			s_sixaxis_handle_count = 0;
			return nx_throw_libnx_error(ctx, srt, "hidStartSixAxisSensor()");
		}
	}
	s_sixaxis_started = true;
	return JS_NewBool(ctx, true);
}

static JSValue make_vec3(JSContext *ctx, HidVector v) {
	JSValue o = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, o, "x", JS_NewFloat64(ctx, v.x));
	JS_SetPropertyStr(ctx, o, "y", JS_NewFloat64(ctx, v.y));
	JS_SetPropertyStr(ctx, o, "z", JS_NewFloat64(ctx, v.z));
	return o;
}

static JSValue nx_sensors_sixaxis_read(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	if (!s_sixaxis_started || s_sixaxis_handle_count <= 0) {
		return JS_NULL;
	}
	HidSixAxisSensorState st = {0};
	size_t got =
		hidGetSixAxisSensorStates(s_sixaxis_handles[0], &st, 1);
	if (got == 0) return JS_NULL;
	JSValue o = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, o, "acceleration", make_vec3(ctx, st.acceleration));
	JS_SetPropertyStr(ctx, o, "angularVelocity",
					  make_vec3(ctx, st.angular_velocity));
	JS_SetPropertyStr(ctx, o, "angle", make_vec3(ctx, st.angle));
	JS_SetPropertyStr(ctx, o, "samplingNumber",
					  JS_NewBigUint64(ctx, st.sampling_number));
	JS_SetPropertyStr(ctx, o, "deltaTime",
					  JS_NewBigUint64(ctx, st.delta_time));
	return o;
}

static JSValue nx_sensors_sixaxis_stop(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	if (!s_sixaxis_started) return JS_UNDEFINED;
	for (s32 i = 0; i < s_sixaxis_handle_count; i++)
		hidStopSixAxisSensor(s_sixaxis_handles[i]);
	s_sixaxis_handle_count = 0;
	s_sixaxis_started = false;
	return JS_UNDEFINED;
}

// ---------- NFC (nfp:user, amiibo) ---------------------------------------

#define NFP_MAX_HANDLES 2
static NfcDeviceHandle s_nfp_handles[NFP_MAX_HANDLES];
static s32 s_nfp_handle_count = 0;
static bool s_nfp_inited = false;
static bool s_nfp_detecting = false;

static JSValue nx_sensors_nfp_start(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	if (s_nfp_detecting) return JS_NewBool(ctx, true);

	if (!s_nfp_inited) {
		Result rc = nfpInitialize(NfpServiceType_User);
		if (R_FAILED(rc))
			return nx_throw_libnx_error(ctx, rc, "nfpInitialize()");
		s_nfp_inited = true;
	}

	s32 total = 0;
	Result rc = nfpListDevices(&total, s_nfp_handles, NFP_MAX_HANDLES);
	if (R_FAILED(rc))
		return nx_throw_libnx_error(ctx, rc, "nfpListDevices()");
	s_nfp_handle_count = total;
	if (total == 0) return JS_NewBool(ctx, false);

	for (s32 i = 0; i < total; i++) {
		Result srt = nfpStartDetection(&s_nfp_handles[i]);
		if (R_FAILED(srt)) {
			for (s32 j = 0; j < i; j++)
				nfpStopDetection(&s_nfp_handles[j]);
			s_nfp_handle_count = 0;
			return nx_throw_libnx_error(ctx, srt, "nfpStartDetection()");
		}
	}
	s_nfp_detecting = true;
	return JS_NewBool(ctx, true);
}

static JSValue nx_sensors_nfp_poll(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	JSValue arr = JS_NewArray(ctx);
	if (!s_nfp_detecting) return arr;
	for (s32 i = 0; i < s_nfp_handle_count; i++) {
		NfpDeviceState state = NfpDeviceState_Unavailable;
		Result rc = nfpGetDeviceState(&s_nfp_handles[i], &state);
		JSValue entry = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, entry, "index", JS_NewInt32(ctx, i));
		JS_SetPropertyStr(ctx, entry, "state",
						  R_SUCCEEDED(rc) ? JS_NewUint32(ctx, (u32)state)
										  : JS_NULL);
		if (R_SUCCEEDED(rc) && state == NfpDeviceState_TagFound) {
			NfpTagInfo info = {0};
			if (R_SUCCEEDED(nfpGetTagInfo(&s_nfp_handles[i], &info))) {
				JSValue uuid = JS_NewArrayBufferCopy(
					ctx, info.uid.uid, info.uid.uid_length);
				JS_SetPropertyStr(ctx, entry, "uuid", uuid);
				JS_SetPropertyStr(ctx, entry, "protocol",
								  JS_NewUint32(ctx, (u32)info.protocol));
				JS_SetPropertyStr(ctx, entry, "tagType",
								  JS_NewUint32(ctx, (u32)info.tag_type));
			}
		}
		JS_SetPropertyUint32(ctx, arr, (u32)i, entry);
	}
	return arr;
}

static JSValue nx_sensors_nfp_stop(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	if (!s_nfp_detecting) return JS_UNDEFINED;
	for (s32 i = 0; i < s_nfp_handle_count; i++)
		nfpStopDetection(&s_nfp_handles[i]);
	s_nfp_handle_count = 0;
	s_nfp_detecting = false;
	return JS_UNDEFINED;
}

// ---------- Registration -------------------------------------------------

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("sensorsBatteryInfo", 0, nx_sensors_battery_info),
	JS_CFUNC_DEF("sensorsAudioInfo", 0, nx_sensors_audio_info),
	JS_CFUNC_DEF("sensorsWlanInfo", 0, nx_sensors_wlan_info),
	JS_CFUNC_DEF("sensorsSixAxisStart", 0, nx_sensors_sixaxis_start),
	JS_CFUNC_DEF("sensorsSixAxisRead", 0, nx_sensors_sixaxis_read),
	JS_CFUNC_DEF("sensorsSixAxisStop", 0, nx_sensors_sixaxis_stop),
	JS_CFUNC_DEF("sensorsNfpStart", 0, nx_sensors_nfp_start),
	JS_CFUNC_DEF("sensorsNfpPoll", 0, nx_sensors_nfp_poll),
	JS_CFUNC_DEF("sensorsNfpStop", 0, nx_sensors_nfp_stop),
};

void nx_init_sensors(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
