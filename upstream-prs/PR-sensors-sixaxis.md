# PR-sensors-sixaxis — Six-axis (gyro + accelerometer) sensor bindings

**Branch:** `upstream-pr/sensors-sixaxis`
**Base:** `upstream/main`
**Draft status.** Not yet pushed, no PR opened. This entry captures
the design + justification so the PR can be assembled on demand.

## What's in the commit

Files added / touched (four files, all additive — nothing existing is
modified beyond one alphabetical insert in each of `main.cc` and
`packages/runtime/src/switch/index.ts`):

```
 source/sensors.h                                   |   4 +
 source/sensors.cc                                  | 145 ++++++++
 source/main.cc                                     |   2 +
 packages/runtime/src/$.ts                          |  11 +
 packages/runtime/src/switch/diagnostics.ts         |  46 +++
 packages/runtime/src/switch/index.ts               |   1 +
 6 files changed, 209 insertions(+)
```

## Motivation

Web APIs `DeviceOrientationEvent` and `DeviceMotionEvent` are the
standard surface every mobile-web page reaches for when it wants
IMU data (tilt-controls, orientation-following UI, AR/VR fallback
paths). Runtime-side polyfills can synthesize those events from any
per-frame IMU source, but they need a source. The Switch has one —
`hidGetSixAxisSensorStates` from libnx — and this is the smallest
JS-facing surface that exposes it.

Without native bindings, an embedder that wants deviceorientation
support has to either:
1. Fork the engine to add bindings (this PR), or
2. Skip the surface entirely (page-level `NO DATA` fallback), or
3. Route through gamepad polling — which the Gamepad API deliberately
   does NOT expose IMU on (spec has axes/buttons only), so this route
   requires a non-standard `Gamepad.gyro` extension that no consumer
   handles.

Path (1) is the only viable one for the widest class of pages.

## Design — six-axis subset only

The QuickJS branch of nx.js shipped a larger `sensors.c` (~360 LOC)
covering five sensor groups: battery extras (`psm`), audio output
state (`audctl`+`audout`), Wi-Fi RSSI (`wlaninf`), six-axis (`hid`),
and NFP/amiibo (`nfp:user`). This PR intentionally ports **only the
six-axis triplet** — the smallest surface that unblocks the primary
use case (deviceorientation/devicemotion polyfill in the embedder
runtime). Rationale:

- **Sensor scope should track consumer scope.** The other four groups
  each carry a specific service init/exit lifecycle, error surface,
  and enum contract that need review and testing on their own. Landing
  them together buries five independent decisions in one PR.
- **Six-axis is the sole engine dependency for the web IMU surface.**
  Every other group has a partial or full web analogue already:
  battery extras are read through `navigator.getBattery()` (nx.js
  provides that already); WLAN state is inferable from `nifm` +
  `navigator.onLine`; audio devices don't map to a mobile-web API
  meaningfully.
- **Room for follow-ups.** The same shape (module init in `main.cc`,
  handle guards, JS wrapper under `Switch.diagnostics.*`) extends
  naturally when a specific downstream need forces one of the other
  groups in.

The six-axis surface itself is:

```ts
Switch.diagnostics.sixAxis.start(): boolean
Switch.diagnostics.sixAxis.read(): { acceleration, angularVelocity, angle, samplingNumber, deltaTime } | null
Switch.diagnostics.sixAxis.stop(): void
```

`start()` picks up whatever pad is currently connected (Handheld →
Dual Joy-Con on No1 → Pro Controller on No1 fallback ladder — same
strategy as the QuickJS version). `read()` returns `null` when the
sensor isn't running OR the pad hasn't produced a fresh sample since
the last read; callers must treat both cases the same (idle
auto-rotate, "no data" tag). `stop()` releases the underlying
`HidSixAxisSensorHandle`s.

`hid` service init happens at process boot via libnx + `main.cc`; the
sensor bindings only guard the per-controller `hidStartSixAxisSensor`
/ `hidStopSixAxisSensor` handshake, so a page that never calls
`start()` pays zero.

## Compatibility

- **Zero risk of collision with an existing binding.** No previous
  `$.sensors*` names existed in v8; the PR only adds.
- **No new libnx service dependencies.** `hidGetSixAxisSensorStates`
  and its handle-management peers are already linked (libnx `hid` is
  always initialized on Switch).
- **API-shape compatible with QuickJS branch.** The read-object
  fields (`acceleration.{x,y,z}`, `angularVelocity.{x,y,z}`,
  `angle.{x,y,z}`, `samplingNumber`, `deltaTime`) match `sensors.c`
  in QuickJS byte-for-byte, so any embedder that had a QuickJS-era
  `Switch.diagnostics.sixAxis` consumer keeps working.

## Genericization already performed

The QuickJS `sensors.c` had a stale comment tying it to "the Switch
diagnostics page" (i.e. nx.js's demo apps directory). Rewritten as
"hardware diagnostics" so the module name doesn't imply a coupling to
a specific consumer.

Battery extras / audio / WLAN / NFP were dropped, per the "sensor
scope tracks consumer scope" reasoning above.

Field order in the read object matches spec on the wire (acceleration
first, angular velocity next, fused angle last) so a consumer walking
the object gets predictable field order.

## Remaining work before opening

- **Testing.** Would like to confirm behavior on real hardware
  (currently exercised on Citron for the emulator path — the sensor
  itself is stubbed there and returns `null`, which is the correct
  "no data" contract).
- **Documentation.** Add a `runtime/concepts/sensors.mdx` under `docs/`
  once the API stabilizes. Not blocking.
- **Types.** `$.sensorsSixAxisRead()`'s return-shape type in `$.ts`
  is inlined; consider promoting to a named `SixAxisReading` interface
  in `types.ts` alongside `NetworkInfo` etc.

## Downstream context

Downstream embedders (currently `@switch-web/runtime`) consume
`Switch.diagnostics.sixAxis.{start,read,stop}` to synthesize
`deviceorientation` and `devicemotion` events on the page's
`LiveWindow`. The polyfill starts the sensor lazily on the first
`addEventListener('deviceorientation'|'devicemotion')` and stops it
when the last such listener is removed — so a page that doesn't use
IMU sees no engine cost.
