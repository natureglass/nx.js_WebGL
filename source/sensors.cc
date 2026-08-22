// Hardware sensor bindings. Currently exposes the six-axis (gyro +
// accelerometer + fused-angle) sensor as three functions:
//
//   $.sensorsSixAxisStart() : boolean
//   $.sensorsSixAxisRead()  : { acceleration, angularVelocity, angle,
//                               direction, ... } | null
//   $.sensorsSixAxisStop()  : undefined
//
// `direction` is Nintendo's fused 3x3 orientation matrix
// (HidSixAxisSensorState.direction) serialized row-major as a flat
// 9-element array [m00,m01,m02, m10,m11,m12, m20,m21,m22]. Unlike the
// accel vector (gravity-referenced tilt only) this carries a full
// orientation including a *relative* yaw the deviceorientation polyfill
// uses to recover `alpha`. There is no magnetometer, so yaw is
// origin-arbitrary and drifts — see device-orientation.ts.
//
// hid is initialized at process boot by libnx + main.cc, so no ensure_hid()
// is needed — we only guard per-controller start/stop of the sensor itself.
// A page that never calls sensorsSixAxisStart() pays nothing.
//
// The QuickJS version of nx.js exposed a larger `sensors.c` module including
// battery extras (psm), audio control (audctl+audout), WLAN RSSI (wlaninf)
// and NFP/amiibo. Those were dropped from this V8 port — the only surface
// currently needed by @switch-web/runtime's `deviceorientation` /
// `devicemotion` polyfill is the six-axis triplet. The other groups can be
// added incrementally with the same shape when needed.

#include "error.h"
#include "sensors.h"

using namespace v8;

namespace {

#define SIXAXIS_MAX_HANDLES 2
static HidSixAxisSensorHandle s_sixaxis_handles[SIXAXIS_MAX_HANDLES];
static s32 s_sixaxis_handle_count = 0;
static bool s_sixaxis_started = false;

// Attempt to obtain sensor handles for the current pad configuration. Tries
// Handheld first (most common when running on the console itself), then
// falls back to docked dual Joy-Con on player 1, then to Pro Controller on
// No1. Sets s_sixaxis_handles + s_sixaxis_handle_count on success; returns
// the libnx Result of the successful call, or the last-tried Result on
// failure.
static Result nx_sixaxis_acquire_handles(void) {
	HidNpadIdType npad_id = HidNpadIdType_Handheld;
	HidNpadStyleTag style = HidNpadStyleTag_NpadHandheld;
	s32 total = 1;
	Result rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
	                                        style);
	if (R_FAILED(rc)) {
		npad_id = HidNpadIdType_No1;
		style = HidNpadStyleTag_NpadJoyDual;
		total = 2;
		rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
		                                style);
	}
	if (R_FAILED(rc)) {
		npad_id = HidNpadIdType_No1;
		style = HidNpadStyleTag_NpadFullKey;
		total = 1;
		rc = hidGetSixAxisSensorHandles(s_sixaxis_handles, total, npad_id,
		                                style);
	}
	if (R_SUCCEEDED(rc)) s_sixaxis_handle_count = total;
	return rc;
}

void nx_sensors_sixaxis_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (s_sixaxis_started) {
		info.GetReturnValue().Set(Boolean::New(iso, true));
		return;
	}

	Result rc = nx_sixaxis_acquire_handles();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "hidGetSixAxisSensorHandles");
		return;
	}

	for (s32 i = 0; i < s_sixaxis_handle_count; i++) {
		Result srt = hidStartSixAxisSensor(s_sixaxis_handles[i]);
		if (R_FAILED(srt)) {
			// Roll back any handles we already started.
			for (s32 j = 0; j < i; j++)
				hidStopSixAxisSensor(s_sixaxis_handles[j]);
			s_sixaxis_handle_count = 0;
			nx_throw_libnx_error(iso, srt, "hidStartSixAxisSensor");
			return;
		}
	}
	s_sixaxis_started = true;
	info.GetReturnValue().Set(Boolean::New(iso, true));
}

static Local<Object> make_vec3(Isolate *iso, Local<Context> ctx, HidVector v) {
	Local<Object> o = Object::New(iso);
	o->Set(ctx, nx_str(iso, "x"), Number::New(iso, v.x)).Check();
	o->Set(ctx, nx_str(iso, "y"), Number::New(iso, v.y)).Check();
	o->Set(ctx, nx_str(iso, "z"), Number::New(iso, v.z)).Check();
	return o;
}

void nx_sensors_sixaxis_read(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!s_sixaxis_started || s_sixaxis_handle_count <= 0) {
		info.GetReturnValue().SetNull();
		return;
	}
	HidSixAxisSensorState st = {};
	size_t got = hidGetSixAxisSensorStates(s_sixaxis_handles[0], &st, 1);
	if (got == 0) {
		info.GetReturnValue().SetNull();
		return;
	}
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> o = Object::New(iso);
	o->Set(ctx, nx_str(iso, "acceleration"), make_vec3(iso, ctx, st.acceleration))
	    .Check();
	o->Set(ctx, nx_str(iso, "angularVelocity"),
	       make_vec3(iso, ctx, st.angular_velocity))
	    .Check();
	o->Set(ctx, nx_str(iso, "angle"), make_vec3(iso, ctx, st.angle)).Check();
	// Fused 3x3 orientation matrix, row-major flat [m00..m22]. `float
	// direction[3][3]` is contiguous row-major in C, so &d[0][0] walks
	// m00,m01,m02,m10,... in order.
	Local<Array> dir = Array::New(iso, 9);
	const float *dm = &st.direction.direction[0][0];
	for (int k = 0; k < 9; k++)
		dir->Set(ctx, k, Number::New(iso, dm[k])).Check();
	o->Set(ctx, nx_str(iso, "direction"), dir).Check();
	o->Set(ctx, nx_str(iso, "samplingNumber"),
	       BigInt::NewFromUnsigned(iso, st.sampling_number))
	    .Check();
	o->Set(ctx, nx_str(iso, "deltaTime"),
	       BigInt::NewFromUnsigned(iso, st.delta_time))
	    .Check();
	info.GetReturnValue().Set(o);
}

void nx_sensors_sixaxis_stop(const FunctionCallbackInfo<Value> &info) {
	if (!s_sixaxis_started) return;
	for (s32 i = 0; i < s_sixaxis_handle_count; i++)
		hidStopSixAxisSensor(s_sixaxis_handles[i]);
	s_sixaxis_handle_count = 0;
	s_sixaxis_started = false;
}

} // namespace

void nx_init_sensors(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "sensorsSixAxisStart", nx_sensors_sixaxis_start);
	NX_SET_FUNC(init_obj, "sensorsSixAxisRead", nx_sensors_sixaxis_read);
	NX_SET_FUNC(init_obj, "sensorsSixAxisStop", nx_sensors_sixaxis_stop);
}
