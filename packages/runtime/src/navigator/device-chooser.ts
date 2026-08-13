// Device-chooser seam — the piece that lets the hardware Web APIs
// (WebUSB, Web Bluetooth, and, in later phases, WebSerial / WebHID /
// Web MIDI / Web NFC) present a device-selection or permission-grant UI
// instead of silently binding the first matching device.
//
// The engine's `requestDevice()` / `requestPort()` flows call
// `getDeviceChooser()`. When a host application (the brewser shell) has
// registered a chooser, the flow enumerates its candidates, hands them to
// the chooser, and resolves with whatever the user picked. When *no* chooser
// is registered — e.g. a bare nx.js homebrew app with no shell — the flows
// fall back to their historical first-match behaviour, so nothing that works
// today regresses.
//
// ## Cross-bundle bridge
//
// The brewser shell is bundled separately from the runtime that ships inside
// the NRO (esbuild inlines its own copy of any `@nx.js/runtime` module it
// imports), so the shell cannot reach this module's state through a normal
// import — a `setDeviceChooser` it imported would mutate a *different* module
// instance than the one `requestDevice()` reads. The existing polyfills solve
// the same problem by mutating the global `navigator`; we mirror that here by
// publishing the registration setter on `globalThis.__nxjsSetDeviceChooser`.
// The shell calls that global once at boot; the live chooser reference lives
// only in this module (read via `getDeviceChooser()`), so app JS can't swap
// it by overwriting a plain data global.

/** Which hardware API is asking for a device. */
export type DeviceChooserKind =
	| 'usb'
	| 'bluetooth'
	| 'serial'
	| 'hid'
	| 'midi'
	| 'nfc';

/** A single selectable device presented to the user by the chooser. */
export interface DeviceChooserCandidate {
	/** Opaque, stable id the chooser returns to identify the choice. The
	 * calling flow maps it back to the underlying device handle. */
	id: string;
	/** Primary display label (product name, or a vendor:product fallback). */
	name: string;
	/** Optional secondary line (ids, address, connection state, …). */
	detail?: string;
}

export interface DeviceChooserRequest {
	kind: DeviceChooserKind;
	/**
	 * `'select'` — the user picks one of `candidates` (WebUSB / Web Bluetooth
	 * / WebSerial / WebHID device pickers).
	 * `'confirm'` — a single yes/no permission grant (Web MIDI's
	 * `requestMIDIAccess`, Web NFC's `scan`), where `candidates[0]` describes
	 * what is being granted.
	 */
	mode: 'select' | 'confirm';
	candidates: DeviceChooserCandidate[];
	/** Optional heading the UI may show above the list / prompt. */
	title?: string;
}

/**
 * Resolves with the chosen candidate's `id` (select mode) or a non-null
 * sentinel (confirm mode → granted), or `null` when the user cancels / denies.
 */
export type DeviceChooser = (
	req: DeviceChooserRequest,
) => Promise<string | null>;

let chooser: DeviceChooser | null = null;

/** Register (or clear, with `null`) the host device chooser. */
export function setDeviceChooser(fn: DeviceChooser | null): void {
	chooser = typeof fn === 'function' ? fn : null;
}

/** The currently-registered chooser, or `null` for first-match fallback. */
export function getDeviceChooser(): DeviceChooser | null {
	return chooser;
}

// Publish the setter for the separately-bundled shell to call once at boot.
// Non-enumerable + non-writable so it reads like an internal engine hook and
// can't be trivially shadowed; `configurable: true` keeps re-eval (HMR / test
// re-import) from throwing.
Object.defineProperty(globalThis, '__nxjsSetDeviceChooser', {
	value: setDeviceChooser,
	writable: false,
	enumerable: false,
	configurable: true,
});

// Debug/testing aid: invoke the currently-registered chooser directly with a
// synthetic request. Lets a host with no real device backend — e.g. the Citron
// emulator, which doesn't emulate usb:hs host mode or BLE scanning — exercise
// the full chooser UI + permission gate with fake candidates. Resolves to the
// chosen id (or null when the user cancels / no chooser is registered). The
// permission gate still applies (it lives in the host chooser), so this can't
// bypass consent — it only stands in for real device enumeration.
Object.defineProperty(globalThis, '__nxjsDeviceChooserProbe', {
	value: (req: DeviceChooserRequest): Promise<string | null> => {
		const fn = getDeviceChooser();
		return fn ? fn(req) : Promise.resolve(null);
	},
	writable: false,
	enumerable: false,
	configurable: true,
});
