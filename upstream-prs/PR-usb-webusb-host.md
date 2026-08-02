# PR-usb-webusb-host — usable, conformant WebUSB host support

**Branch:** `upstream-pr/usb-webusb-host`
**Base:** `upstream/main`
**Draft status.** Not yet pushed, no PR opened. This entry captures the design +
justification so the PR can be assembled on demand.

> Consolidates three earlier draft entries (control-transfer-out, non-blocking
> read, WebUSB conformance) into one PR. They all touch the same three files and
> are really one feature — turning the partial WebUSB shim into a WebUSB host
> that actually works — so a single PR avoids stacking/rebasing and tells one
> coherent story. Each is preserved below as its own section.

## Files touched

```
 source/usb.cc                                  | rewritten (device-centric)
 packages/runtime/src/$.ts                      | new native binding types
 packages/runtime/src/navigator/usb.ts          | device model, EventTarget, methods
```

All additive at the JS API level except the device-model change (one `USBDevice`
now carries all of a device's interfaces — the conformant shape). Requires a
runtime rebuild (native `.nro` + regenerated TS types).

## Motivation

The shipped `navigator.usb` was a thin shim: it could enumerate and do bulk +
IN-control transfers, but it couldn't do the OUT control transfers real devices
need, it blocked the event loop on every read, and it modeled the bus wrong (one
`USBDevice` per interface). In practice that means a WebUSB serial monitor, HID
poller, or DFU tool couldn't be written against it. This PR closes those gaps as
far as libnx `usb:hs` allows, verified end-to-end on real hardware (see below).

---

## Part 1 — `controlTransferOut` (host→device control transfers)

The shim had `controlTransferIn` but not `controlTransferOut`, and the native
control path **hard-coded the IN direction bit** (`bmRequestType = USB_ENDPOINT_IN
| …`), so the entire OUT direction was unreachable — not even a zero-length OUT.

Added `nx_usb_control_transfer_out` (mirrors the IN function but with
`USB_ENDPOINT_OUT` and an optional `BufferSource` data stage; `undefined` data →
`wLength 0`), the `usbControlTransferOut` binding, and
`USBDevice.controlTransferOut(setup, data?)` → `USBOutTransferResult`.

This unblocks any device configured by an OUT control request: USB CDC-ACM
(`SET_LINE_CODING`, `SET_CONTROL_LINE_STATE` = DTR/RTS — CDC devices won't stream
until DTR is asserted), HID `SET_REPORT`, DFU `DFU_DNLOAD`, and countless vendor
control-out protocols.

## Part 2 — non-blocking `transferIn`

`transferIn` used the blocking `usbHsEpPostBuffer` (`eventWait(…, UINT64_MAX)`),
so on nx.js's single JS thread `await device.transferIn()` **froze the whole event
loop** (RAF, input, timers) until a packet arrived — and hung indefinitely on an
idle endpoint. A JS watchdog can't help; the thread is parked in the syscall.

Reimplemented on the async primitives libnx already exposes:
`usbHsEpPostBufferAsync` + `eventWait(evt, 0)` (non-blocking poll) +
`usbHsEpGetXferReport`. New `usbReadStart` posts one URB (idempotent per
endpoint, page-aligned DMA buffer stored on the device); `usbReadPoll` does a
zero-timeout wait and returns `undefined` while pending or the `ArrayBuffer` on
completion (ownership handed to V8; `count == 0` treated as still-pending so it's
autoclear-agnostic). The **public `transferIn` signature is unchanged** — it now
posts once then polls a frame at a time (`yieldToLoop`), so the UI stays at frame
rate while a read is outstanding and a silent endpoint no longer hangs the app.

## Part 3 — device-centric model (the headline conformance fix)

`usbHsQueryAvailableInterfaces` returns one `UsbHsInterface` per **interface**, so
the old binding wrapped each as its own `USBDevice` — a CDC-ACM device surfaced as
two `USBDevice` objects sharing a VID:PID, the opposite of WebUSB.

`UsbHsInterface` carries `busID` + `deviceID` (identical for every interface of
one device), so enumeration now **groups interfaces by (busID, deviceID)** into
one `nx_usb_device_t` holding an array of interfaces, each with its own lazily
acquired `UsbHsClientIfSession` and per-endpoint state. The JS descriptor lists
every interface under one device — the correct WebUSB shape.

Consequences, all handled:
- **`claimInterface(n)`** acquires *that* interface's session; **`releaseInterface(n)`**
  actually closes it (endpoints + session). Previously claim only validated the
  number and release was a no-op.
- **Transfers** resolve `<endpoint, direction>` to the owning *claimed* interface.
- **Control transfers** use the first acquired interface as the default control
  pipe (lazily acquiring one if none is claimed yet).

## Part 4 — new / fixed methods

- **`selectAlternateInterface(i, alt)`** — real `SET_INTERFACE` via
  `usbHsIfSetInterface`, closes the old alt's endpoints, refreshes the cached
  endpoint descriptors. Previously JS-only.
- **`clearHalt(dir, ep)`** — new. `CLEAR_FEATURE(ENDPOINT_HALT)` + closes the
  local endpoint session so the next transfer reopens it with a reset toggle.
- **`forget()`** — new. No permission store on Switch, so it maps to `close()`.
- **String descriptors** — the engine doesn't populate manufacturer/product/
  serial at enumeration (that needs acquiring every interface during a query,
  which is invasive). `open()` reads them via `GET_DESCRIPTOR` once a control
  pipe exists and fills the (now non-`readonly`) fields. Available after `open()`.
- **`isochronousTransferIn/Out`** — added but reject with `NotSupportedError`:
  libnx `usb:hs` has no isochronous primitive, and a fabricated one on
  `BatchBufferAsync` is untestable/error-prone. The method shape matches spec so
  feature-detection works.

## Part 5 — hotplug `connect` / `disconnect` events

`USB` now `extends EventTarget` and exposes `onconnect` / `ondisconnect` plus
`USBConnectionEvent` (with `device`). Backed by `usbHsGetInterfaceStateChangeEvent`
(removal) and `usbHsCreateInterfaceAvailableEvent` (arrival); new native
`usbHotplugCheck()` does a zero-timeout `eventWait` on both and reports whether
either fired. The JS layer runs a lightweight timer **only while a listener is
registered**; each tick calls `usbHotplugCheck()` and, only on a real change,
re-enumerates and diffs against the known set (keyed by busID:deviceID) to
dispatch events. The known set is seeded silently on first listen so pre-existing
devices don't fire a spurious `connect`. No listeners → no timer.

## Part 6 — enumeration completeness

`usbHsQueryAvailableInterfaces` **hides already-acquired interfaces**, so a
connected composite device would redraw as just its leftover interface during a
re-poll. Enumeration now uses `usbHsQueryAllInterfaces` (complete set, including
acquired; IDs come back as `-1`) and merges valid acquire IDs back in from the
available query. A device always shows all its interfaces; claiming an available
interface still works; interfaces held elsewhere display but throw cleanly on
claim.

## Part 7 — reconnect-race retry (found on hardware)

usb:hs releases interfaces asynchronously, so a rapid disconnect→reconnect can
transiently fail `usbHsAcquireUsbIf` with the interface still "in use" — and a
**data interface with a pending bulk-IN URB** releases slower (needs the URB to
abort on close). `USBDevice.claimInterface` retries with a **frame-yielding**
backoff (up to ~1.5 s, UI stays live) and only for the transient
`usbHsAcquireUsbIf` error (other errors throw at once). Deliberately *not* a
native `svcSleepThread` loop, which would block the render loop.

## Known remaining gaps (documented, not silently faked)

- **Transfer `status` is always `'ok'`** — libnx surfaces a `Result` we can't
  cleanly classify as stall/babble, so transfer errors reject instead of
  resolving with `status:'stall'`. `clearHalt` provides the recovery path.
- **`requestDevice`** has no chooser / permission model (Switch has no prompt UI);
  it returns the first filter match, same as the Bluetooth binding.
- **`selectConfiguration`** selects among exposed configs but issues no
  `SET_CONFIGURATION` (usb:hs owns configuration).
- **Isochronous** unsupported (no libnx primitive).

## Hardware validation

Verified on a real Switch (CFW) against an ESP32-C6 dev board over both USB-C
ports — the native USB-Serial-JTAG (`303a:1001`, 3-interface composite) and a
CH343 UART bridge (`1a86:55d3`, 2-interface). Confirmed: device-centric
enumeration + all-interface view, string descriptors, `claimInterface`,
`SET_LINE_CODING` + `SET_CONTROL_LINE_STATE` (DTR/RTS), non-blocking streaming,
bulk-OUT round trip (PING→PONG), hotplug attach, rapid same-millisecond
reconnect (9 cycles, 0 acquire failures), and clean release/close on exit.

## Downstream context

Consumed by the `com.natureglass.usbmonitor` homebrew app — a USB CDC-ACM serial
monitor for ESP32-class devices over the Switch USB-C port. Its connect flow is
`open` → `claimInterface` → `SET_LINE_CODING` → `SET_CONTROL_LINE_STATE(DTR|RTS)`
→ non-blocking bulk-IN read loop, finding the CDC data + comms interfaces within
one device object.
