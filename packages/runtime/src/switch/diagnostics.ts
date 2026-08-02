import { $ } from '../$';

/**
 * Hardware diagnostics — currently exposes the six-axis (gyro +
 * accelerometer + fused angle) sensor. Additional sensor groups
 * (battery extras, WLAN RSSI, audio output state, NFC/amiibo) exist
 * in the QuickJS branch's `sensors.c` and can be ported the same way
 * when needed.
 *
 * `sixAxis` has an explicit start/stop lifecycle because the underlying
 * `hidGetSixAxisSensorHandles` consumes controller-side resources; call
 * `start()` before `read()` and `stop()` when done. `read()` returns
 * `null` when the sensor isn't running or the pad hasn't produced a
 * fresh sample yet.
 *
 * @example
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
	/** Six-axis sensor (gyro + accelerometer + fused angle). Call `start()`
	 * once, `read()` per frame for the latest sample, `stop()` when done.
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
};
