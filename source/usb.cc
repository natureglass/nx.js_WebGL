#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <malloc.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <switch.h>

using namespace v8;

namespace {

// A USBDevice is now device-centric: one physical device (grouped by
// busID/deviceID) holding all of its interfaces. Each interface owns its own
// libnx interface session (acquired lazily on claim/open) and per-endpoint
// state. This mirrors the WebUSB model where `device.configurations[].
// interfaces[]` lists every interface of one physical device.

#define NX_USB_MAX_IFACES 16
#define NX_USB_EP 16

struct nx_usb_iface_t {
	bool present;
	UsbHsInterface inf; // descriptor (refreshed on selectAlternateInterface)
	bool acquired;      // usbHsAcquireUsbIf done
	UsbHsClientIfSession session;
	UsbHsClientEpSession in_eps[NX_USB_EP];
	UsbHsClientEpSession out_eps[NX_USB_EP];
	bool in_open[NX_USB_EP];
	bool out_open[NX_USB_EP];
	// Non-blocking bulk-IN read state: one in-flight URB per IN endpoint.
	bool in_reading[NX_USB_EP];
	void *in_buf[NX_USB_EP];
	u64 in_id_counter;
};

struct nx_usb_device_t {
	u32 busID;
	u32 deviceID;
	struct usb_device_descriptor device_desc;
	struct usb_config_descriptor config_desc;
	bool opened;
	int iface_count;
	nx_usb_iface_t ifaces[NX_USB_MAX_IFACES];
};

bool g_initialized = false;

// Hotplug: usb:hs loads a removal (state-change) event at init (autoclear=false),
// and we create an interface-available event for additions.
Event *g_state_change_evt = nullptr;
Event g_avail_evt = {0};
bool g_avail_evt_ok = false;

uint32_t get_u32_prop(Isolate *iso, Local<Object> obj, const char *name,
	                    bool *has) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	*has = false;
	if (!obj->Get(ctx, nx_str(iso, name)).ToLocal(&v) || v->IsUndefined() ||
	    v->IsNull()) {
		return 0;
	}
	*has = true;
	return v->Uint32Value(ctx).FromMaybe(0);
}

bool get_string_prop(Isolate *iso, Local<Object> obj, const char *name,
	                    Local<Value> *out) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (!obj->Get(ctx, nx_str(iso, name)).ToLocal(&v) || !v->IsString()) {
		char message[128];
		snprintf(message, sizeof(message), "USB control transfer setup.%s must be a string", name);
		nx_throw(iso, message);
		return false;
	}
	*out = v;
	return true;
}

// ---- device / interface lifecycle ---------------------------------------

void iface_close(nx_usb_iface_t *f) {
	for (int i = 0; i < NX_USB_EP; i++) {
		if (f->in_open[i]) {
			usbHsEpClose(&f->in_eps[i]);
			f->in_open[i] = false;
		}
		// Endpoint close aborts any in-flight URB; only then free its buffer.
		if (f->in_reading[i]) {
			free(f->in_buf[i]);
			f->in_buf[i] = nullptr;
			f->in_reading[i] = false;
		}
		if (f->out_open[i]) {
			usbHsEpClose(&f->out_eps[i]);
			f->out_open[i] = false;
		}
	}
	if (f->acquired) {
		usbHsIfClose(&f->session);
		f->acquired = false;
	}
}

void usb_device_close(nx_usb_device_t *dev) {
	if (!dev)
		return;
	for (int i = 0; i < dev->iface_count; i++) {
		if (dev->ifaces[i].present)
			iface_close(&dev->ifaces[i]);
	}
	dev->opened = false;
}

void usb_device_free(nx_usb_device_t *dev) {
	usb_device_close(dev);
	free(dev);
}

bool ensure_usb(Isolate *iso) {
	if (g_initialized)
		return true;
	Result rc = usbHsInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsInitialize");
		return false;
	}
	g_initialized = true;
	// Hotplug events (best-effort — hotplug just degrades to polling if absent).
	g_state_change_evt = usbHsGetInterfaceStateChangeEvent();
	UsbHsInterfaceFilter empty = {0};
	if (R_SUCCEEDED(usbHsCreateInterfaceAvailableEvent(&g_avail_evt, true, 0, &empty))) {
		g_avail_evt_ok = true;
	}
	return true;
}

nx_usb_device_t *unwrap_device(Isolate *iso, Local<Value> v) {
	nx_usb_device_t *dev = nx::Unwrap<nx_usb_device_t>(v);
	if (!dev) {
		nx_throw(iso, "Invalid USBDevice native handle");
		return nullptr;
	}
	return dev;
}

nx_usb_iface_t *iface_by_number(nx_usb_device_t *dev, uint32_t ifnum) {
	for (int i = 0; i < dev->iface_count; i++) {
		if (dev->ifaces[i].present &&
		    dev->ifaces[i].inf.inf.interface_desc.bInterfaceNumber == ifnum)
			return &dev->ifaces[i];
	}
	return nullptr;
}

// Acquire an interface's libnx session if not already acquired.
// Single attempt: usb:hs can transiently fail a re-acquire right after a close
// (it releases interfaces asynchronously). Rather than block the thread here,
// the retry/backoff is done in the JS layer (USBDevice.claimInterface) where it
// can yield frames and keep the UI responsive.
bool iface_acquire(Isolate *iso, nx_usb_iface_t *f) {
	if (f->acquired)
		return true;
	Result rc = usbHsAcquireUsbIf(&f->session, &f->inf);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsAcquireUsbIf");
		return false;
	}
	f->acquired = true;
	return true;
}

// A session usable for device/endpoint-recipient control transfers. Prefers an
// already-acquired interface; otherwise lazily acquires the first one (WebUSB
// allows control transfers to the default pipe after open(), before claim).
UsbHsClientIfSession *ctrl_session(Isolate *iso, nx_usb_device_t *dev) {
	for (int i = 0; i < dev->iface_count; i++) {
		if (dev->ifaces[i].present && dev->ifaces[i].acquired)
			return &dev->ifaces[i].session;
	}
	for (int i = 0; i < dev->iface_count; i++) {
		if (dev->ifaces[i].present) {
			if (!iface_acquire(iso, &dev->ifaces[i]))
				return nullptr;
			return &dev->ifaces[i].session;
		}
	}
	nx_throw(iso, "USB device has no interfaces");
	return nullptr;
}

// Find the acquired interface that owns endpoint `ep` in the given direction.
nx_usb_iface_t *iface_for_ep(nx_usb_device_t *dev, uint8_t ep, bool in) {
	for (int i = 0; i < dev->iface_count; i++) {
		nx_usb_iface_t *f = &dev->ifaces[i];
		if (!f->present || !f->acquired)
			continue;
		struct usb_endpoint_descriptor *descs =
		    in ? f->inf.inf.input_endpoint_descs : f->inf.inf.output_endpoint_descs;
		for (int j = 0; j < 15; j++) {
			if (descs[j].bLength >= USB_DT_ENDPOINT_SIZE &&
			    (descs[j].bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK) == ep)
				return f;
		}
	}
	return nullptr;
}

struct usb_endpoint_descriptor *find_endpoint(nx_usb_iface_t *f, uint8_t ep,
	                                            bool in) {
	struct usb_endpoint_descriptor *descs =
	    in ? f->inf.inf.input_endpoint_descs : f->inf.inf.output_endpoint_descs;
	for (int i = 0; i < 15; i++) {
		if (descs[i].bLength >= USB_DT_ENDPOINT_SIZE &&
		    (descs[i].bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK) == ep)
			return &descs[i];
	}
	return nullptr;
}

UsbHsClientEpSession *open_endpoint(Isolate *iso, nx_usb_iface_t *f, uint8_t ep,
	                                  bool in, uint32_t xfer_size) {
	if (ep >= NX_USB_EP) {
		nx_throw(iso, "Invalid USB endpoint number");
		return nullptr;
	}
	bool *open = in ? f->in_open : f->out_open;
	UsbHsClientEpSession *eps = in ? f->in_eps : f->out_eps;
	if (open[ep])
		return &eps[ep];
	struct usb_endpoint_descriptor *desc = find_endpoint(f, ep, in);
	if (!desc) {
		nx_throw(iso, "USB endpoint not found");
		return nullptr;
	}
	uint32_t max_xfer = xfer_size;
	if (max_xfer < desc->wMaxPacketSize)
		max_xfer = desc->wMaxPacketSize;
	if (max_xfer < 0x1000)
		max_xfer = 0x1000;
	Result rc = usbHsIfOpenUsbEp(&f->session, &eps[ep], 4, max_xfer, desc);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfOpenUsbEp");
		return nullptr;
	}
	open[ep] = true;
	return &eps[ep];
}

// Resolve <device, endpoint, direction> to <interface, ep session>, requiring a
// claimed interface that owns the endpoint.
nx_usb_iface_t *resolve_ep(Isolate *iso, nx_usb_device_t *dev, uint8_t ep,
	                         bool in, uint32_t xfer_size,
	                         UsbHsClientEpSession **out_ep) {
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return nullptr;
	}
	nx_usb_iface_t *f = iface_for_ep(dev, ep, in);
	if (!f) {
		nx_throw(iso, "USB endpoint not found on any claimed interface");
		return nullptr;
	}
	UsbHsClientEpSession *eps = open_endpoint(iso, f, ep, in, xfer_size);
	if (!eps)
		return nullptr;
	*out_ep = eps;
	return f;
}

// ---- descriptor -> JS ----------------------------------------------------

void set_u32(Isolate *iso, Local<Object> obj, const char *name, uint32_t value) {
	Local<Context> ctx = iso->GetCurrentContext();
	obj->Set(ctx, nx_str(iso, name), Integer::NewFromUnsigned(iso, value)).Check();
}

Local<Object> endpoint_to_object(Isolate *iso,
	                               const struct usb_endpoint_descriptor *desc) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> obj = Object::New(iso);
	set_u32(iso, obj, "endpointNumber",
	        desc->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
	obj->Set(ctx, nx_str(iso, "direction"),
	         nx_str(iso, (desc->bEndpointAddress & USB_ENDPOINT_IN) ? "in" : "out"))
	    .Check();
	const char *type = "bulk";
	switch (desc->bmAttributes & USB_TRANSFER_TYPE_MASK) {
	case USB_TRANSFER_TYPE_ISOCHRONOUS:
		type = "isochronous";
		break;
	case USB_TRANSFER_TYPE_INTERRUPT:
		type = "interrupt";
		break;
	}
	obj->Set(ctx, nx_str(iso, "type"), nx_str(iso, type)).Check();
	set_u32(iso, obj, "packetSize", desc->wMaxPacketSize);
	return obj;
}

Local<Object> interface_to_object(Isolate *iso, const UsbHsInterface *inf) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> alt = Object::New(iso);
	const struct usb_interface_descriptor *desc = &inf->inf.interface_desc;
	set_u32(iso, alt, "alternateSetting", desc->bAlternateSetting);
	set_u32(iso, alt, "interfaceClass", desc->bInterfaceClass);
	set_u32(iso, alt, "interfaceSubclass", desc->bInterfaceSubClass);
	set_u32(iso, alt, "interfaceProtocol", desc->bInterfaceProtocol);
	Local<Array> eps = Array::New(iso);
	uint32_t ep_idx = 0;
	// A populated endpoint descriptor is >= 7 bytes; unused slots are all-zero
	// (bLength 0). Use `>=` not `==`: USB-Audio/MIDIStreaming endpoints use the
	// 9-byte audio endpoint descriptor (adds bRefresh + bSynchAddress), so an
	// `== USB_DT_ENDPOINT_SIZE` check would drop every MIDI bulk endpoint and the
	// interface would surface with no endpoints (no MIDI in/out ports).
	for (int i = 0; i < 15; i++) {
		const struct usb_endpoint_descriptor *in = &inf->inf.input_endpoint_descs[i];
		if (in->bLength >= USB_DT_ENDPOINT_SIZE)
			eps->Set(ctx, ep_idx++, endpoint_to_object(iso, in)).Check();
		const struct usb_endpoint_descriptor *out = &inf->inf.output_endpoint_descs[i];
		if (out->bLength >= USB_DT_ENDPOINT_SIZE)
			eps->Set(ctx, ep_idx++, endpoint_to_object(iso, out)).Check();
	}
	alt->Set(ctx, nx_str(iso, "endpoints"), eps).Check();

	Local<Object> iface = Object::New(iso);
	set_u32(iso, iface, "interfaceNumber", desc->bInterfaceNumber);
	set_u32(iso, iface, "claimed", 0);
	Local<Array> alternates = Array::New(iso, 1);
	alternates->Set(ctx, 0, alt).Check();
	iface->Set(ctx, nx_str(iso, "alternates"), alternates).Check();
	return iface;
}

// Build the JS descriptor for a device from its grouped interfaces.
void set_device_descriptor(Isolate *iso, Local<Object> obj,
	                         nx_usb_device_t *dev) {
	Local<Context> ctx = iso->GetCurrentContext();
	const struct usb_device_descriptor *d = &dev->device_desc;
	set_u32(iso, obj, "usbVersionMajor", (d->bcdUSB >> 8) & 0xff);
	set_u32(iso, obj, "usbVersionMinor", (d->bcdUSB >> 4) & 0x0f);
	set_u32(iso, obj, "usbVersionSubminor", d->bcdUSB & 0x0f);
	set_u32(iso, obj, "deviceClass", d->bDeviceClass);
	set_u32(iso, obj, "deviceSubclass", d->bDeviceSubClass);
	set_u32(iso, obj, "deviceProtocol", d->bDeviceProtocol);
	set_u32(iso, obj, "vendorId", d->idVendor);
	set_u32(iso, obj, "productId", d->idProduct);
	set_u32(iso, obj, "deviceVersionMajor", (d->bcdDevice >> 8) & 0xff);
	set_u32(iso, obj, "deviceVersionMinor", (d->bcdDevice >> 4) & 0x0f);
	set_u32(iso, obj, "deviceVersionSubminor", d->bcdDevice & 0x0f);
	// Non-standard but stable identity for the JS hotplug map.
	set_u32(iso, obj, "busId", dev->busID);
	set_u32(iso, obj, "deviceId", dev->deviceID);

	Local<Object> config = Object::New(iso);
	set_u32(iso, config, "configurationValue", dev->config_desc.bConfigurationValue);
	Local<Array> interfaces = Array::New(iso, dev->iface_count);
	for (int i = 0; i < dev->iface_count; i++) {
		interfaces->Set(ctx, i, interface_to_object(iso, &dev->ifaces[i].inf)).Check();
	}
	config->Set(ctx, nx_str(iso, "interfaces"), interfaces).Check();
	Local<Array> configurations = Array::New(iso, 1);
	configurations->Set(ctx, 0, config).Check();
	obj->Set(ctx, nx_str(iso, "configurations"), configurations).Check();
}

// ---- entry points --------------------------------------------------------

void nx_usb_init(const FunctionCallbackInfo<Value> &info) {
	ensure_usb(info.GetIsolate());
}

void nx_usb_exit(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	if (g_initialized) {
		if (g_avail_evt_ok) {
			usbHsDestroyInterfaceAvailableEvent(&g_avail_evt, 0);
			g_avail_evt_ok = false;
		}
		g_state_change_evt = nullptr;
		usbHsExit();
		g_initialized = false;
	}
}

void build_filter(Isolate *iso, Local<Value> arg, UsbHsInterfaceFilter *filter) {
	if (!arg->IsObject())
		return;
	Local<Object> f = arg.As<Object>();
	bool has = false;
	uint32_t v = get_u32_prop(iso, f, "vendorId", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_idVendor; filter->idVendor = (u16)v; }
	v = get_u32_prop(iso, f, "productId", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_idProduct; filter->idProduct = (u16)v; }
	v = get_u32_prop(iso, f, "classCode", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bDeviceClass; filter->bDeviceClass = (u8)v; }
	v = get_u32_prop(iso, f, "subclassCode", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bDeviceSubClass; filter->bDeviceSubClass = (u8)v; }
	v = get_u32_prop(iso, f, "protocolCode", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bDeviceProtocol; filter->bDeviceProtocol = (u8)v; }
	v = get_u32_prop(iso, f, "interfaceClass", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bInterfaceClass; filter->bInterfaceClass = (u8)v; }
	v = get_u32_prop(iso, f, "interfaceSubclass", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bInterfaceSubClass; filter->bInterfaceSubClass = (u8)v; }
	v = get_u32_prop(iso, f, "interfaceProtocol", &has);
	if (has) { filter->Flags |= UsbHsInterfaceFilterFlags_bInterfaceProtocol; filter->bInterfaceProtocol = (u8)v; }
}

void nx_usb_get_devices(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	if (!ensure_usb(iso))
		return;

	UsbHsInterfaceFilter filter = {0};
	build_filter(iso, info[0], &filter);

	// Enumerate the COMPLETE set with usbHsQueryAllInterfaces so a device always
	// shows all of its interfaces — even ones currently acquired (which
	// usbHsQueryAvailableInterfaces hides, so a connected composite device would
	// otherwise redraw as just its leftover interface). QueryAll sets ID=-1, so
	// we separately fetch the acquirable set and copy each valid ID/struct back
	// in; interfaces missing from `avail` are held elsewhere and can't be
	// re-acquired (claim throws cleanly), but still display correctly.
	UsbHsInterface interfaces[64];
	s32 total = 0;
	Result rc = usbHsQueryAllInterfaces(&filter, interfaces, sizeof(interfaces), &total);
	if (R_FAILED(rc)) {
		// Fall back to the available-only query on firmwares where QueryAll fails.
		rc = usbHsQueryAvailableInterfaces(&filter, interfaces, sizeof(interfaces), &total);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "usbHsQueryAllInterfaces");
			return;
		}
	}
	if (total > 64)
		total = 64;

	UsbHsInterface avail[64];
	s32 total_avail = 0;
	if (R_FAILED(usbHsQueryAvailableInterfaces(&filter, avail, sizeof(avail), &total_avail)))
		total_avail = 0;
	if (total_avail > 64)
		total_avail = 64;

	// Group interfaces by physical device (busID + deviceID).
	nx_usb_device_t *devs[64];
	int dev_count = 0;
	for (s32 i = 0; i < total; i++) {
		UsbHsInterface *itf = &interfaces[i];
		// Prefer the acquirable copy (valid ID) when this interface is available.
		for (s32 a = 0; a < total_avail; a++) {
			if (avail[a].busID == itf->busID && avail[a].deviceID == itf->deviceID &&
			    avail[a].inf.interface_desc.bInterfaceNumber ==
			        itf->inf.interface_desc.bInterfaceNumber) {
				itf = &avail[a];
				break;
			}
		}
		nx_usb_device_t *dev = nullptr;
		for (int k = 0; k < dev_count; k++) {
			if (devs[k]->busID == itf->busID && devs[k]->deviceID == itf->deviceID) {
				dev = devs[k];
				break;
			}
		}
		if (!dev) {
			dev = (nx_usb_device_t *)calloc(1, sizeof(*dev));
			if (!dev) {
				for (int k = 0; k < dev_count; k++) usb_device_free(devs[k]);
				nx_throw_oom(iso, sizeof(*dev));
				return;
			}
			dev->busID = itf->busID;
			dev->deviceID = itf->deviceID;
			memcpy(&dev->device_desc, &itf->device_desc, sizeof(dev->device_desc));
			memcpy(&dev->config_desc, &itf->config_desc, sizeof(dev->config_desc));
			devs[dev_count++] = dev;
		}
		if (dev->iface_count < NX_USB_MAX_IFACES) {
			nx_usb_iface_t *f = &dev->ifaces[dev->iface_count++];
			f->present = true;
			f->inf = *itf;
		}
	}

	Local<Array> arr = Array::New(iso, dev_count);
	for (int k = 0; k < dev_count; k++) {
		Local<Object> obj = nx::NewWrapped(iso);
		nx::Wrap(iso, obj, devs[k], usb_device_free);
		set_device_descriptor(iso, obj, devs[k]);
		arr->Set(ctx, k, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

// Returns 1 if a hotplug event fired since the last check (or if events are
// unavailable, forcing a re-enumeration), else 0.
void nx_usb_hotplug_check(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!g_initialized) {
		info.GetReturnValue().Set(false);
		return;
	}
	bool changed = false;
	if (g_state_change_evt) {
		if (R_SUCCEEDED(eventWait(g_state_change_evt, 0))) {
			changed = true;
			eventClear(g_state_change_evt); // autoclear=false
		}
	}
	if (g_avail_evt_ok) {
		if (R_SUCCEEDED(eventWait(&g_avail_evt, 0)))
			changed = true; // autoclear=true
	}
	if (!g_state_change_evt && !g_avail_evt_ok)
		changed = true; // no event support → always re-check
	(void)iso;
	info.GetReturnValue().Set(changed);
}

void nx_usb_device_open(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	// No libnx "open device" primitive; interfaces are acquired on claim. We
	// mark the device open and best-effort acquire the first interface so that
	// control transfers / string reads work before an explicit claimInterface.
	if (!dev->opened) {
		if (dev->iface_count > 0) {
			nx_usb_iface_t *f0 = &dev->ifaces[0];
			if (R_SUCCEEDED(usbHsAcquireUsbIf(&f0->session, &f0->inf)))
				f0->acquired = true;
		}
		dev->opened = true;
	}
}

void nx_usb_device_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	usb_device_close(dev);
}

void nx_usb_claim_interface(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	uint32_t ifnum = info[1]->Uint32Value(ctx).FromMaybe(0);
	nx_usb_iface_t *f = iface_by_number(dev, ifnum);
	if (!f) {
		nx_throw(iso, "USB interface not found on this device");
		return;
	}
	iface_acquire(iso, f); // throws on failure
}

void nx_usb_release_interface(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t ifnum = info[1]->Uint32Value(ctx).FromMaybe(0);
	nx_usb_iface_t *f = iface_by_number(dev, ifnum);
	if (f)
		iface_close(f); // closes endpoints + interface session
}

void nx_usb_select_alternate_interface(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t ifnum = info[1]->Uint32Value(ctx).FromMaybe(0);
	uint32_t alt = info[2]->Uint32Value(ctx).FromMaybe(0);
	nx_usb_iface_t *f = iface_by_number(dev, ifnum);
	if (!f) {
		nx_throw(iso, "USB interface not found on this device");
		return;
	}
	if (!iface_acquire(iso, f))
		return;
	// Endpoints belong to the current alt setting; close them before switching.
	for (int i = 0; i < NX_USB_EP; i++) {
		if (f->in_open[i]) { usbHsEpClose(&f->in_eps[i]); f->in_open[i] = false; }
		if (f->in_reading[i]) { free(f->in_buf[i]); f->in_buf[i] = nullptr; f->in_reading[i] = false; }
		if (f->out_open[i]) { usbHsEpClose(&f->out_eps[i]); f->out_open[i] = false; }
	}
	UsbHsInterfaceInfo newinf;
	Result rc = usbHsIfSetInterface(&f->session, &newinf, (u8)alt);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfSetInterface");
		return;
	}
	// Refresh the cached interface info so endpoint descriptors match the alt.
	f->inf.inf = newinf;
}

// ---- transfers -----------------------------------------------------------

void nx_usb_transfer_out(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	size_t len = 0;
	uint8_t *src = NX_GetBufferSource(iso, &len, info[2]);
	if (!src) {
		nx_throw(iso, "transferOut data must be a BufferSource");
		return;
	}
	if (len > 0xff0000) {
		nx_throw(iso, "USB transfer is too large");
		return;
	}
	UsbHsClientEpSession *ep = nullptr;
	if (!resolve_ep(iso, dev, (uint8_t)endpoint, false, (uint32_t)len, &ep))
		return;
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	memcpy(buf, src, len);
	u32 transferred = 0;
	Result rc = usbHsEpPostBuffer(ep, buf, (u32)len, &transferred);
	free(buf);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsEpPostBuffer");
		return;
	}
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, transferred));
}

void nx_usb_transfer_in(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	uint32_t len = info[2]->Uint32Value(ctx).FromMaybe(0);
	if (len > 0xff0000) {
		nx_throw(iso, "USB transfer is too large");
		return;
	}
	UsbHsClientEpSession *ep = nullptr;
	if (!resolve_ep(iso, dev, (uint8_t)endpoint, true, len, &ep))
		return;
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	u32 transferred = 0;
	Result rc = usbHsEpPostBuffer(ep, buf, len, &transferred);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsEpPostBuffer");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, transferred, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

// Non-blocking bulk-IN: post the URB (idempotent), reap later with read_poll.
void nx_usb_read_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	uint32_t len = info[2]->Uint32Value(ctx).FromMaybe(0);
	if (len > 0xff0000) {
		nx_throw(iso, "USB transfer is too large");
		return;
	}
	if (endpoint >= NX_USB_EP) {
		nx_throw(iso, "Invalid USB endpoint number");
		return;
	}
	nx_usb_iface_t *f = iface_for_ep(dev, (uint8_t)endpoint, true);
	if (dev->opened && f && f->in_reading[endpoint])
		return; // already in flight — idempotent
	UsbHsClientEpSession *ep = nullptr;
	f = resolve_ep(iso, dev, (uint8_t)endpoint, true, len, &ep);
	if (!f)
		return;
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	u64 id = f->in_id_counter++;
	u32 xferId = 0;
	Result rc = usbHsEpPostBufferAsync(ep, buf, len, id, &xferId);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsEpPostBufferAsync");
		return;
	}
	f->in_buf[endpoint] = buf;
	f->in_reading[endpoint] = true;
}

void nx_usb_read_poll(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	uint32_t endpoint = info[1]->Uint32Value(ctx).FromMaybe(0);
	if (endpoint >= NX_USB_EP)
		return;
	nx_usb_iface_t *f = iface_for_ep(dev, (uint8_t)endpoint, true);
	if (!f || !f->in_reading[endpoint])
		return; // nothing pending → undefined
	UsbHsClientEpSession *ep = &f->in_eps[endpoint];
	Result rc = eventWait(&ep->eventXfer, 0);
	if (R_FAILED(rc)) {
		if (R_VALUE(rc) == KERNELRESULT(TimedOut))
			return; // still in flight
		void *buf = f->in_buf[endpoint];
		f->in_reading[endpoint] = false;
		f->in_buf[endpoint] = nullptr;
		free(buf);
		nx_throw_libnx_error(iso, rc, "eventWait(eventXfer)");
		return;
	}
	UsbHsXferReport report = {0};
	u32 count = 0;
	rc = usbHsEpGetXferReport(ep, &report, 1, &count);
	if (R_FAILED(rc)) {
		void *buf = f->in_buf[endpoint];
		f->in_reading[endpoint] = false;
		f->in_buf[endpoint] = nullptr;
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsEpGetXferReport");
		return;
	}
	if (count == 0)
		return; // spurious wake; keep pending state
	void *buf = f->in_buf[endpoint];
	f->in_reading[endpoint] = false;
	f->in_buf[endpoint] = nullptr;
	if (R_FAILED(report.res)) {
		free(buf);
		nx_throw_libnx_error(iso, report.res, "usb bulk IN transfer");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, report.transferredSize, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

// ---- control transfers ---------------------------------------------------

uint8_t request_type_bits(const char *s) {
	if (strcmp(s, "class") == 0)
		return USB_REQUEST_TYPE_CLASS;
	if (strcmp(s, "vendor") == 0)
		return USB_REQUEST_TYPE_VENDOR;
	if (strcmp(s, "reserved") == 0)
		return USB_REQUEST_TYPE_RESERVED;
	return USB_REQUEST_TYPE_STANDARD;
}

uint8_t recipient_bits(const char *s) {
	if (strcmp(s, "interface") == 0)
		return USB_RECIPIENT_INTERFACE;
	if (strcmp(s, "endpoint") == 0)
		return USB_RECIPIENT_ENDPOINT;
	if (strcmp(s, "other") == 0)
		return USB_RECIPIENT_OTHER;
	return USB_RECIPIENT_DEVICE;
}

void nx_usb_control_transfer_in(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	if (!info[1]->IsObject()) {
		nx_throw(iso, "controlTransferIn setup must be an object");
		return;
	}
	Local<Object> setup = info[1].As<Object>();
	Local<Value> request_type_val, recipient_val;
	if (!get_string_prop(iso, setup, "requestType", &request_type_val) ||
	    !get_string_prop(iso, setup, "recipient", &recipient_val))
		return;
	String::Utf8Value request_type(iso, request_type_val);
	String::Utf8Value recipient(iso, recipient_val);
	bool has = false;
	uint32_t request = get_u32_prop(iso, setup, "request", &has);
	uint32_t value = get_u32_prop(iso, setup, "value", &has);
	uint32_t index = get_u32_prop(iso, setup, "index", &has);
	uint32_t len = info[2]->Uint32Value(ctx).FromMaybe(0);
	if (len > 0xffff) {
		nx_throw(iso, "USB control transfer length is too large");
		return;
	}
	UsbHsClientIfSession *s = ctrl_session(iso, dev);
	if (!s)
		return;
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	u8 bmRequestType = USB_ENDPOINT_IN |
	    request_type_bits(*request_type ? *request_type : "standard") |
	    recipient_bits(*recipient ? *recipient : "device");
	u32 transferred = 0;
	Result rc = usbHsIfCtrlXfer(s, bmRequestType, (u8)request, (u16)value,
	                            (u16)index, (u16)len, buf, &transferred);
	if (R_FAILED(rc)) {
		free(buf);
		nx_throw_libnx_error(iso, rc, "usbHsIfCtrlXfer");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, transferred, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

void nx_usb_control_transfer_out(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	if (!info[1]->IsObject()) {
		nx_throw(iso, "controlTransferOut setup must be an object");
		return;
	}
	Local<Object> setup = info[1].As<Object>();
	Local<Value> request_type_val, recipient_val;
	if (!get_string_prop(iso, setup, "requestType", &request_type_val) ||
	    !get_string_prop(iso, setup, "recipient", &recipient_val))
		return;
	String::Utf8Value request_type(iso, request_type_val);
	String::Utf8Value recipient(iso, recipient_val);
	bool has = false;
	uint32_t request = get_u32_prop(iso, setup, "request", &has);
	uint32_t value = get_u32_prop(iso, setup, "value", &has);
	uint32_t index = get_u32_prop(iso, setup, "index", &has);

	// Optional data stage (zero-length for SET_CONTROL_LINE_STATE etc.).
	size_t len = 0;
	uint8_t *src = nullptr;
	if (!info[2]->IsUndefined() && !info[2]->IsNull()) {
		src = NX_GetBufferSource(iso, &len, info[2]);
		if (!src) {
			nx_throw(iso, "controlTransferOut data must be a BufferSource");
			return;
		}
	}
	if (len > 0xffff) {
		nx_throw(iso, "USB control transfer length is too large");
		return;
	}
	UsbHsClientIfSession *s = ctrl_session(iso, dev);
	if (!s)
		return;
	size_t alloc_size = (len + 0xfff) & ~((size_t)0xfff);
	if (alloc_size == 0)
		alloc_size = 0x1000;
	void *buf = memalign(0x1000, alloc_size);
	if (!buf) {
		nx_throw_oom(iso, alloc_size);
		return;
	}
	memset(buf, 0, alloc_size);
	if (src && len > 0)
		memcpy(buf, src, len);
	u8 bmRequestType = USB_ENDPOINT_OUT |
	    request_type_bits(*request_type ? *request_type : "standard") |
	    recipient_bits(*recipient ? *recipient : "device");
	u32 transferred = 0;
	Result rc = usbHsIfCtrlXfer(s, bmRequestType, (u8)request, (u16)value,
	                            (u16)index, (u16)len, buf, &transferred);
	free(buf);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfCtrlXfer");
		return;
	}
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, transferred));
}

// clearHalt: CLEAR_FEATURE(ENDPOINT_HALT) on the endpoint, then close the local
// endpoint session so the next transfer reopens it with a reset data toggle.
void nx_usb_clear_halt(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	bool in = info[1]->BooleanValue(iso);
	uint32_t endpoint = info[2]->Uint32Value(ctx).FromMaybe(0);
	if (endpoint >= NX_USB_EP) {
		nx_throw(iso, "Invalid USB endpoint number");
		return;
	}
	nx_usb_iface_t *f = iface_for_ep(dev, (uint8_t)endpoint, in);
	if (!f) {
		nx_throw(iso, "USB endpoint not found on any claimed interface");
		return;
	}
	u16 wIndex = (u16)(endpoint | (in ? USB_ENDPOINT_IN : 0));
	u8 bmRequestType = (u8)USB_ENDPOINT_OUT | (u8)USB_REQUEST_TYPE_STANDARD |
	                   (u8)USB_RECIPIENT_ENDPOINT;
	u32 transferred = 0;
	Result rc = usbHsIfCtrlXfer(&f->session, bmRequestType,
	                            0x01 /* CLEAR_FEATURE */, 0x00 /* ENDPOINT_HALT */,
	                            wIndex, 0, nullptr, &transferred);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "usbHsIfCtrlXfer(CLEAR_FEATURE)");
		return;
	}
	// Reset the host-side endpoint (and data toggle) by closing the session.
	if (in) {
		if (f->in_reading[endpoint]) {
			free(f->in_buf[endpoint]);
			f->in_buf[endpoint] = nullptr;
			f->in_reading[endpoint] = false;
		}
		if (f->in_open[endpoint]) {
			usbHsEpClose(&f->in_eps[endpoint]);
			f->in_open[endpoint] = false;
		}
	} else {
		if (f->out_open[endpoint]) {
			usbHsEpClose(&f->out_eps[endpoint]);
			f->out_open[endpoint] = false;
		}
	}
}

void nx_usb_reset_device(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_usb_device_t *dev = unwrap_device(iso, info[0]);
	if (!dev)
		return;
	if (!dev->opened) {
		nx_throw(iso, "USB device is not open");
		return;
	}
	UsbHsClientIfSession *s = ctrl_session(iso, dev);
	if (!s)
		return;
	Result rc = usbHsIfResetDevice(s);
	if (R_FAILED(rc))
		nx_throw_libnx_error(iso, rc, "usbHsIfResetDevice");
}

} // namespace

void nx_init_usb(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "usbInit", nx_usb_init);
	NX_SET_FUNC(init_obj, "usbExit", nx_usb_exit);
	NX_SET_FUNC(init_obj, "usbGetDevices", nx_usb_get_devices);
	NX_SET_FUNC(init_obj, "usbHotplugCheck", nx_usb_hotplug_check);
	NX_SET_FUNC(init_obj, "usbDeviceOpen", nx_usb_device_open);
	NX_SET_FUNC(init_obj, "usbDeviceClose", nx_usb_device_close);
	NX_SET_FUNC(init_obj, "usbClaimInterface", nx_usb_claim_interface);
	NX_SET_FUNC(init_obj, "usbReleaseInterface", nx_usb_release_interface);
	NX_SET_FUNC(init_obj, "usbSelectAlternateInterface", nx_usb_select_alternate_interface);
	NX_SET_FUNC(init_obj, "usbTransferIn", nx_usb_transfer_in);
	NX_SET_FUNC(init_obj, "usbReadStart", nx_usb_read_start);
	NX_SET_FUNC(init_obj, "usbReadPoll", nx_usb_read_poll);
	NX_SET_FUNC(init_obj, "usbTransferOut", nx_usb_transfer_out);
	NX_SET_FUNC(init_obj, "usbControlTransferIn", nx_usb_control_transfer_in);
	NX_SET_FUNC(init_obj, "usbControlTransferOut", nx_usb_control_transfer_out);
	NX_SET_FUNC(init_obj, "usbClearHalt", nx_usb_clear_halt);
	NX_SET_FUNC(init_obj, "usbResetDevice", nx_usb_reset_device);
}
