import { $ } from '../$';

/**
 * Hardware diagnostics sensors — battery extras, six-axis (gyro/accel),
 * audio output state, Wi-Fi signal, and NFC (amiibo) scan.
 *
 * All calls are synchronous, lazy-init the underlying libnx service on first
 * use, and have no continuous cost: a page that doesn't poll pays nothing
 * (including any other page in the same process — the bindings stay quiescent
 * until called).
 *
 * Sensors with explicit lifecycle (six-axis, NFP) expose `*Start()` /
 * `*Read()` or `*Poll()` / `*Stop()` triplets — call Start before polling and
 * Stop when leaving the page so controller-side resources are released.
 *
 * @example
 * // One-shot reading (no setup needed)
 * const bat = Switch.diagnostics.battery();
 * console.log(bat.voltageMv, bat.temperatureC);
 *
 * // Polling loop
 * Switch.diagnostics.sixAxis.start();
 * const tick = () => {
 *   const s = Switch.diagnostics.sixAxis.read();
 *   if (s) updateCube(s.angle.x, s.angle.y, s.angle.z);
 *   requestAnimationFrame(tick);
 * };
 * tick();
 * // ...later, on page leave:
 * Switch.diagnostics.sixAxis.stop();
 */
export const diagnostics = {
	/** Battery details (voltage, temperature, charge%, age%, charging state).
	 * Requires firmware 17.0.0+ for the full struct; older firmware throws. */
	battery() {
		return $.sensorsBatteryInfo();
	},

	/** Audio output state (master volume, list of available output device
	 * names, whether headphones are detected). */
	audio() {
		return $.sensorsAudioInfo();
	},

	/** Wi-Fi signal info (RSSI in dBm, connection state). `available: false`
	 * if `wlaninf` couldn't be initialized (e.g. in some emulators). */
	wlan() {
		return $.sensorsWlanInfo();
	},

	/** Six-axis sensor (gyro + accelerometer + fused angle). Call `start()`
	 * once, `read()` per frame to get the latest sample, `stop()` when done.
	 * `read()` returns `null` when the sensor isn't running or has no sample
	 * yet. */
	sixAxis: {
		start(): boolean {
			return $.sensorsSixAxisStart();
		},
		read() {
			return $.sensorsSixAxisRead();
		},
		stop(): void {
			$.sensorsSixAxisStop();
		},
	},

	/** NFC reader (amiibo detection). `start()` lists devices and begins
	 * detection on each; `poll()` returns one entry per device with its
	 * current `state` (NfpDeviceState enum). `state === 2` = TagFound, in
	 * which case `uuid` is populated. Use `stop()` when leaving the page. */
	nfp: {
		start(): boolean {
			return $.sensorsNfpStart();
		},
		poll() {
			return $.sensorsNfpPoll();
		},
		stop(): void {
			$.sensorsNfpStop();
		},
	},
};

/**
 * NfpDeviceState enum values returned from `Switch.diagnostics.nfp.poll()`.
 *
 * - `Initialized` (0): device ready, not yet searching.
 * - `SearchingForTag` (1): actively scanning.
 * - `TagFound` (2): a tag is present; `uuid` will be populated.
 * - `TagRemoved` (3): a previously-found tag was removed.
 * - `TagMounted` (4): the tag has been mounted for read/write.
 * - `Unavailable` (5): device disconnected or not present.
 */
export const NfpDeviceState = {
	Initialized: 0,
	SearchingForTag: 1,
	TagFound: 2,
	TagRemoved: 3,
	TagMounted: 4,
	Unavailable: 5,
} as const;

/**
 * WlanInfState enum values returned from `Switch.diagnostics.wlan().state`.
 */
export const WlanInfState = {
	NotConnected: 0,
	Connecting: 1,
	Connected: 2,
} as const;
