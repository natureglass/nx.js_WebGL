#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <malloc.h>
#include <memory>
#include <string.h>
#include <switch.h>

using namespace v8;

// Web NFC backing over libnx `nfc:user`.
//
// The Switch's NFC reader lives in the right Joy-Con / Pro Controller. This
// module exposes just enough of `nfc:*` for NDEF read/write on Type-2 (NTAG)
// tags: initialise, enumerate the reader device, start/stop tag detection,
// poll the device state, read the detected tag's UID/type, and run raw
// commands through `nfcSendCommandByPassThrough` (Type-2 READ 0x30 / WRITE
// 0xA2). The NDEF TLV + record (de)serialisation happens in JS (nfc.ts).

namespace {

bool g_nfc_init = false;
bool g_have_device = false;
NfcDeviceHandle g_handle = {0};

// Re-enumerate NFC reader devices, caching the first one. The reader can come
// and go with the controller, so this is called before operations that need it.
void refresh_devices() {
	s32 total = 0;
	NfcDeviceHandle handles[8];
	Result rc = nfcListDevices(&total, handles, 8);
	if (R_SUCCEEDED(rc) && total > 0) {
		g_handle = handles[0];
		g_have_device = true;
	} else {
		g_have_device = false;
	}
}

// nfcInit(): initialise the service + enumerate devices. Returns true on
// success (service up). Does NOT throw when NFC is unsupported (e.g. the
// Citron emulator) — returns false so JS can report gracefully.
void nx_nfc_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!g_nfc_init) {
		Result rc = nfcInitialize(NfcServiceType_User);
		if (R_FAILED(rc)) {
			info.GetReturnValue().Set(Boolean::New(iso, false));
			return;
		}
		g_nfc_init = true;
	}
	refresh_devices();
	info.GetReturnValue().Set(Boolean::New(iso, true));
}

void nx_nfc_exit(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_nfc_init) {
		nfcExit();
		g_nfc_init = false;
		g_have_device = false;
	}
}

// nfcIsAvailable(): NFC enabled in system settings AND a reader present.
void nx_nfc_is_available(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	bool ok = false;
	if (g_nfc_init) {
		bool enabled = false;
		if (R_SUCCEEDED(nfcIsNfcEnabled(&enabled)) && enabled) {
			refresh_devices();
			ok = g_have_device;
		}
	}
	info.GetReturnValue().Set(Boolean::New(iso, ok));
}

void nx_nfc_start_detection(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	refresh_devices();
	if (!g_have_device) {
		nx_throw(iso, "No NFC reader is available (needs an NFC-capable controller).");
		return;
	}
	Result rc = nfcStartDetection(&g_handle, NfcProtocol_TypeA);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nfcStartDetection");
		return;
	}
}

void nx_nfc_stop_detection(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_have_device)
		nfcStopDetection(&g_handle);
}

// nfcGetState(): current NfcDeviceState (0..4), or -1 if unavailable.
// JS polls this: 2 = TagFound, 4 = TagMounted.
void nx_nfc_get_state(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!g_have_device) {
		info.GetReturnValue().Set(Integer::New(iso, -1));
		return;
	}
	NfcDeviceState st = NfcDeviceState_Initialized;
	Result rc = nfcGetDeviceState(&g_handle, &st);
	if (R_FAILED(rc)) {
		info.GetReturnValue().Set(Integer::New(iso, -1));
		return;
	}
	info.GetReturnValue().Set(Integer::New(iso, (int)st));
}

// nfcGetTagInfo(): { uid: ArrayBuffer, protocol, tagType } or undefined.
void nx_nfc_get_tag_info(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	if (!g_have_device)
		return;
	NfcTagInfo ti = {0};
	Result rc = nfcGetTagInfo(&g_handle, &ti);
	if (R_FAILED(rc))
		return;
	size_t uid_len = ti.uid.uid_length;
	if (uid_len > sizeof(ti.uid.uid))
		uid_len = sizeof(ti.uid.uid);
	void *buf = malloc(uid_len ? uid_len : 1);
	if (!buf) {
		nx_throw_oom(iso, uid_len);
		return;
	}
	memcpy(buf, ti.uid.uid, uid_len);
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, uid_len, [](void *p, size_t, void *) { free(p); }, nullptr);
	Local<Object> obj = Object::New(iso);
	obj->Set(ctx, nx_str(iso, "uid"), ArrayBuffer::New(iso, std::move(bs))).Check();
	obj->Set(ctx, nx_str(iso, "protocol"), Integer::NewFromUnsigned(iso, ti.protocol)).Check();
	obj->Set(ctx, nx_str(iso, "tagType"), Integer::NewFromUnsigned(iso, ti.tag_type)).Check();
	info.GetReturnValue().Set(obj);
}

void nx_nfc_keep_session(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_have_device)
		nfcKeepPassThroughSession(&g_handle);
}

void nx_nfc_release_session(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_have_device)
		nfcReleasePassThroughSession(&g_handle);
}

// nfcTransceive(cmd: BufferSource) -> ArrayBuffer reply. Raw ISO14443-3A
// command passthrough (e.g. Type-2 READ [0x30, page] -> 16 bytes).
void nx_nfc_transceive(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!g_nfc_init || !g_have_device) {
		nx_throw(iso, "NFC is not initialized or no tag is present.");
		return;
	}
	size_t cmd_len = 0;
	uint8_t *cmd = NX_GetBufferSource(iso, &cmd_len, info[0]);
	if (!cmd) {
		nx_throw(iso, "transceive command must be a BufferSource");
		return;
	}
	uint8_t reply[512];
	u64 out_size = 0;
	Result rc = nfcSendCommandByPassThrough(&g_handle, 500000000ULL, cmd, cmd_len,
	                                        reply, sizeof(reply), &out_size);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nfcSendCommandByPassThrough");
		return;
	}
	if (out_size > sizeof(reply))
		out_size = sizeof(reply);
	void *buf = malloc(out_size ? out_size : 1);
	if (!buf) {
		nx_throw_oom(iso, out_size);
		return;
	}
	memcpy(buf, reply, out_size);
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, out_size, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

} // namespace

void nx_init_nfc(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "nfcInit", nx_nfc_init);
	NX_SET_FUNC(init_obj, "nfcExit", nx_nfc_exit);
	NX_SET_FUNC(init_obj, "nfcIsAvailable", nx_nfc_is_available);
	NX_SET_FUNC(init_obj, "nfcStartDetection", nx_nfc_start_detection);
	NX_SET_FUNC(init_obj, "nfcStopDetection", nx_nfc_stop_detection);
	NX_SET_FUNC(init_obj, "nfcGetState", nx_nfc_get_state);
	NX_SET_FUNC(init_obj, "nfcGetTagInfo", nx_nfc_get_tag_info);
	NX_SET_FUNC(init_obj, "nfcKeepSession", nx_nfc_keep_session);
	NX_SET_FUNC(init_obj, "nfcReleaseSession", nx_nfc_release_session);
	NX_SET_FUNC(init_obj, "nfcTransceive", nx_nfc_transceive);
}
