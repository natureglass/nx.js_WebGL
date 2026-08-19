import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { getDeviceChooser } from './device-chooser';
import { INTERNAL_SYMBOL as INTERNAL } from '../internal';
import { USBDevice } from './usb';
import { EventTarget } from '../polyfills/event-target';
import { Event } from '../polyfills/event';
import { setTimeout } from '../timers';
import { assertInternalConstructor, def } from '../utils';

// Web MIDI over USB-MIDI (class-compliant MIDIStreaming).
//
// A USB-MIDI device exposes an Audio-class (0x01) MIDIStreaming (subclass
// 0x03) interface carrying bulk IN/OUT endpoints. MIDI travels as 32-bit
// "USB-MIDI Event Packets": [ (cableNumber<<4)|CIN, midi0, midi1, midi2 ],
// where CIN (Code Index Number) identifies the message. This layer drives the
// device through {@link USBDevice} (same as WebSerial / WebHID) and codecs
// between packets and MIDI byte messages.
//
// Unlike the device pickers, `requestMIDIAccess()` is a one-shot *permission*
// grant (browsers prompt once, then expose every device), so it routes through
// the chooser in `confirm` mode. Access is granted to all connected USB-MIDI
// devices; one input + one output port is exposed per device (cable 0). Multi-
// jack devices expose only cable 0 for now (enumerating embedded jacks needs
// the class-specific MS descriptors, not yet parsed).

const AUDIO_CLASS = 0x01;
const MIDISTREAMING_SUBCLASS = 0x03;

// MIDI byte count carried by each CIN (index = CIN 0x0..0xF).
const CIN_LENGTH = [0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1];

export interface MIDIOptions {
	sysex?: boolean;
	software?: boolean;
}

interface MidiNativeDevice {
	busId: number;
	deviceId: number;
	vendorId: number;
	productId: number;
	productName?: string;
	manufacturerName?: string;
	configurations: {
		interfaces: {
			interfaceNumber: number;
			alternates: {
				interfaceClass: number;
				interfaceSubclass: number;
				endpoints: { endpointNumber: number; direction: string; type: string; packetSize: number }[];
			}[];
		}[];
	}[];
}

interface MidiHandle {
	usb: USBDevice;
	inEndpoint: number | null;
	inPacketSize: number;
	outEndpoint: number | null;
}

interface OpenMidiEntry {
	usb: USBDevice;
	native: MidiNativeDevice;
	input: MIDIInput | null;
	output: MIDIOutput | null;
	sysex: boolean;
}

// Cache of open MIDI devices, keyed by `${busId}:${deviceId}`, kept across
// requestMIDIAccess calls. This makes requestMIDIAccess idempotent — a repeat
// call returns the SAME live ports instead of re-claiming an interface this
// process already holds. It also covers app switches: the usb:hs session and
// this module are process-global (switching brewser apps doesn't exit the NRO),
// so a MIDIStreaming interface opened by a prior app instance stays acquired;
// re-acquiring it fails (usbHsAcquireUsbIf → "in use", module 140 desc 301) and
// the claim retry can't clear it because *we* are the holder. Reusing the cached
// entry sidesteps the re-claim entirely.
const openMidi = new Map<string, OpenMidiEntry>();

function midiKey(native: MidiNativeDevice): string {
	return `${native.busId}:${native.deviceId}`;
}

/** Close + release one cached device (aborts its bulk-IN URB, frees the iface). */
function closeMidiEntry(entry: OpenMidiEntry) {
	try {
		$.usbDeviceClose(entry.native as unknown as Parameters<typeof $.usbDeviceClose>[0]);
	} catch {
		// best-effort; a stale/disconnected handle may already be gone
	}
}

/** Release every cached device — used on full NRO exit (`unload`). */
function closeAllMidiDevices() {
	for (const entry of openMidi.values()) closeMidiEntry(entry);
	openMidi.clear();
}

let usbReady = false;
function ensureUsb() {
	if (usbReady) return;
	$.usbInit();
	usbReady = true;
	// `unload` only fires on full NRO exit (not on a brewser app switch), so this
	// is a best-effort cleanup for process teardown; cross-app-switch reuse is
	// handled by the `openMidi` cache in requestMIDIAccess itself.
	addEventListener('unload', () => {
		closeAllMidiDevices();
		usbReady = false;
	});
}

function hex4(n: number): string {
	return (n >>> 0).toString(16).padStart(4, '0');
}

function isMidiDevice(n: MidiNativeDevice): boolean {
	for (const config of n.configurations ?? []) {
		for (const iface of config.interfaces ?? []) {
			for (const alt of iface.alternates ?? []) {
				if (alt.interfaceClass === AUDIO_CLASS && alt.interfaceSubclass === MIDISTREAMING_SUBCLASS) {
					return true;
				}
			}
		}
	}
	return false;
}

/** Encode a stream of MIDI bytes into concatenated 32-bit USB-MIDI packets. */
function encodeToUsbMidi(data: Uint8Array): Uint8Array {
	const out: number[] = [];
	let i = 0;
	while (i < data.length) {
		const status = data[i];
		if (status >= 0xf0) {
			if (status === 0xf0) {
				// SysEx — consume through the terminating 0xF7 (or end of data).
				let j = i + 1;
				while (j < data.length && data[j] !== 0xf7) j++;
				const end = j < data.length ? j : data.length - 1;
				let k = i;
				while (end - k + 1 > 3) {
					out.push(0x04, data[k], data[k + 1], data[k + 2]);
					k += 3;
				}
				const rem = end - k + 1;
				const endCin = rem === 1 ? 0x05 : rem === 2 ? 0x06 : 0x07;
				out.push(endCin, data[k] ?? 0, data[k + 1] ?? 0, data[k + 2] ?? 0);
				i = end + 1;
				continue;
			}
			if (status === 0xf1 || status === 0xf3) {
				out.push(0x02, status, data[i + 1] ?? 0, 0);
				i += 2;
				continue;
			}
			if (status === 0xf2) {
				out.push(0x03, status, data[i + 1] ?? 0, data[i + 2] ?? 0);
				i += 3;
				continue;
			}
			// Single-byte system (0xF4/0xF5/0xF6, real-time 0xF8..0xFF).
			out.push(0x0f, status, 0, 0);
			i += 1;
			continue;
		}
		if (status < 0x80) {
			// Data byte with no running status we track — skip to resync.
			i += 1;
			continue;
		}
		// Channel voice/mode message.
		const cin = status >> 4;
		const len = CIN_LENGTH[cin];
		if (len === 2) {
			out.push(cin, status, data[i + 1] ?? 0, 0);
			i += 2;
		} else {
			out.push(cin, status, data[i + 1] ?? 0, data[i + 2] ?? 0);
			i += 3;
		}
	}
	return new Uint8Array(out);
}

// --- MIDIMessageEvent ------------------------------------------------------

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIMessageEvent
 */
export class MIDIMessageEvent extends Event {
	readonly data: Uint8Array;

	constructor(type: string, init?: { data?: Uint8Array }) {
		super(type);
		this.data = init?.data ?? new Uint8Array(0);
	}
}
def(MIDIMessageEvent);

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIConnectionEvent
 */
export class MIDIConnectionEvent extends Event {
	readonly port: MIDIPort;

	constructor(type: string, init: { port: MIDIPort }) {
		super(type);
		this.port = init.port;
	}
}
def(MIDIConnectionEvent);

// --- MIDIPort --------------------------------------------------------------

type PortType = 'input' | 'output';
type PortConnection = 'open' | 'closed' | 'pending';
type PortState = 'connected' | 'disconnected';

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIPort
 */
export class MIDIPort extends EventTarget {
	readonly id: string;
	readonly manufacturer: string;
	readonly name: string;
	readonly version: string;
	readonly type: PortType;

	protected handle: MidiHandle;
	protected _connection: PortConnection = 'closed';
	protected _state: PortState = 'connected';

	onstatechange: ((this: MIDIPort, ev: MIDIConnectionEvent) => any) | null = null;

	constructor(
		_internal: symbol,
		id?: string,
		name?: string,
		manufacturer?: string,
		type?: PortType,
		handle?: MidiHandle,
	) {
		super();
		assertInternalConstructor(arguments);
		this.id = id!;
		this.name = name ?? '';
		this.manufacturer = manufacturer ?? '';
		this.version = '1.0';
		this.type = type!;
		this.handle = handle!;
	}

	get connection(): PortConnection {
		return this._connection;
	}
	get state(): PortState {
		return this._state;
	}

	async open(): Promise<this> {
		this._connection = 'open';
		return this;
	}

	async close(): Promise<this> {
		this._connection = 'closed';
		return this;
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'statechange') {
			this.onstatechange?.(event as MIDIConnectionEvent);
		}
		return super.dispatchEvent(event);
	}
}
def(MIDIPort);

// --- MIDIInput -------------------------------------------------------------

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIInput
 */
export class MIDIInput extends MIDIPort {
	#onmidimessage: ((this: MIDIInput, ev: MIDIMessageEvent) => any) | null = null;
	#looping = false;
	#sysex: number[] | null = null;
	#sysexEnabled: boolean;

	constructor(
		_internal: symbol,
		id?: string,
		name?: string,
		manufacturer?: string,
		handle?: MidiHandle,
		sysexEnabled?: boolean,
	) {
		// @ts-expect-error forwarded internal args
		super(_internal, id, name, manufacturer, 'input', handle);
		this.#sysexEnabled = sysexEnabled === true;
	}

	get onmidimessage() {
		return this.#onmidimessage;
	}
	set onmidimessage(cb: ((this: MIDIInput, ev: MIDIMessageEvent) => any) | null) {
		this.#onmidimessage = cb;
		// Setting a handler implicitly opens the port (per spec).
		if (cb) void this.open();
	}

	async open(): Promise<this> {
		if (this._connection === 'open') return this;
		this._connection = 'open';
		if (this.handle.inEndpoint !== null && !this.#looping) {
			this.#looping = true;
			this.#readLoop();
		}
		return this;
	}

	async close(): Promise<this> {
		this._connection = 'closed';
		return this;
	}

	#emit(bytes: number[]): void {
		const ev = new MIDIMessageEvent('midimessage', { data: new Uint8Array(bytes) });
		this.dispatchEvent(ev);
	}

	/** Decode one 32-bit USB-MIDI packet, emitting any completed message. */
	#decodePacket(p0: number, p1: number, p2: number, p3: number): void {
		const cin = p0 & 0x0f;
		if (cin === 0x04) {
			// SysEx start/continue.
			this.#sysex = (this.#sysex ?? []).concat([p1, p2, p3]);
			return;
		}
		if (cin === 0x05 || cin === 0x06 || cin === 0x07) {
			const n = cin - 4; // 1, 2 or 3 trailing bytes
			const tail = [p1, p2, p3].slice(0, n);
			if (this.#sysex) {
				const msg = this.#sysex.concat(tail);
				this.#sysex = null;
				if (this.#sysexEnabled) this.#emit(msg);
				return;
			}
			// Not mid-SysEx: single/short system-common message.
			this.#emit(tail);
			return;
		}
		const len = CIN_LENGTH[cin];
		if (len === 0) return;
		this.#emit([p1, p2, p3].slice(0, len));
	}

	#readLoop(): void {
		const usb = this.handle.usb;
		const inEp = this.handle.inEndpoint!;
		const size = this.handle.inPacketSize || 64;
		const pump = async (): Promise<void> => {
			if (this._connection !== 'open') {
				this.#looping = false;
				return;
			}
			try {
				const res = await usb.transferIn(inEp, size);
				if (this._connection !== 'open') {
					this.#looping = false;
					return;
				}
				const d = res.data;
				if (d) {
					for (let off = 0; off + 4 <= d.byteLength; off += 4) {
						this.#decodePacket(
							d.getUint8(off),
							d.getUint8(off + 1),
							d.getUint8(off + 2),
							d.getUint8(off + 3),
						);
					}
				}
				setTimeout(pump, 0);
			} catch {
				this.#looping = false;
			}
		};
		void pump();
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'midimessage') {
			this.#onmidimessage?.(event as MIDIMessageEvent);
		}
		return super.dispatchEvent(event);
	}
}
def(MIDIInput);

// --- MIDIOutput ------------------------------------------------------------

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIOutput
 */
export class MIDIOutput extends MIDIPort {
	constructor(
		_internal: symbol,
		id?: string,
		name?: string,
		manufacturer?: string,
		handle?: MidiHandle,
	) {
		// @ts-expect-error forwarded internal args
		super(_internal, id, name, manufacturer, 'output', handle);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/MIDIOutput/send
	 */
	send(data: Uint8Array | number[], _timestamp?: number): void {
		if (this.handle.outEndpoint === null) return;
		const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
		const packets = encodeToUsbMidi(bytes);
		if (packets.length === 0) return;
		this._connection = 'open';
		// Fire-and-forget: send() is synchronous in the spec.
		void this.handle.usb.transferOut(this.handle.outEndpoint, packets).catch(() => {});
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/MIDIOutput/clear
	 */
	clear(): void {
		// No queued messages are buffered here, so nothing to flush.
	}
}
def(MIDIOutput);

// --- MIDIAccess ------------------------------------------------------------

/**
 * @see https://developer.mozilla.org/docs/Web/API/MIDIAccess
 */
export class MIDIAccess extends EventTarget {
	readonly inputs: Map<string, MIDIInput>;
	readonly outputs: Map<string, MIDIOutput>;
	readonly sysexEnabled: boolean;

	onstatechange: ((this: MIDIAccess, ev: MIDIConnectionEvent) => any) | null = null;

	constructor(
		_internal: symbol,
		inputs?: Map<string, MIDIInput>,
		outputs?: Map<string, MIDIOutput>,
		sysexEnabled?: boolean,
	) {
		super();
		assertInternalConstructor(arguments);
		this.inputs = inputs ?? new Map();
		this.outputs = outputs ?? new Map();
		this.sysexEnabled = sysexEnabled === true;
	}
}
def(MIDIAccess);

// --- requestMIDIAccess -----------------------------------------------------

// Ensure a connected MIDI device is open and cached. Reuses an already-open
// entry (idempotent — no re-claim of an interface we already hold) unless the
// requested sysex mode differs, in which case it is reopened so the input's
// SysEx gating matches. Best-effort: a device that fails to open/claim is
// skipped, not fatal.
async function ensureMidiOpen(native: MidiNativeDevice, sysex: boolean): Promise<void> {
	const key = midiKey(native);
	const existing = openMidi.get(key);
	if (existing) {
		if (existing.sysex === sysex) return; // reuse the live ports as-is
		// SysEx permission changed → reopen with the new setting.
		closeMidiEntry(existing);
		openMidi.delete(key);
	}
	const usb = new USBDevice(INTERNAL, native as unknown as ConstructorParameters<typeof USBDevice>[1]);
	try {
		await usb.open();
		const config = usb.configuration ?? usb.configurations[0];
		let iface: (typeof config.interfaces)[number] | undefined;
		for (const cand of config.interfaces) {
			const alt = cand.alternate ?? cand.alternates[0];
			if (alt && alt.interfaceClass === AUDIO_CLASS && alt.interfaceSubclass === MIDISTREAMING_SUBCLASS) {
				iface = cand;
				break;
			}
		}
		if (!iface) {
			await usb.close().catch(() => {});
			return;
		}
		const alt = iface.alternate ?? iface.alternates[0];
		let inEp: number | null = null;
		let inSize = 64;
		let outEp: number | null = null;
		for (const ep of alt.endpoints) {
			if (ep.type !== 'bulk') continue;
			if (ep.direction === 'in' && inEp === null) {
				inEp = ep.endpointNumber;
				inSize = ep.packetSize || 64;
			} else if (ep.direction === 'out' && outEp === null) {
				outEp = ep.endpointNumber;
			}
		}
		await usb.claimInterface(iface.interfaceNumber);
		const handle: MidiHandle = { usb, inEndpoint: inEp, inPacketSize: inSize, outEndpoint: outEp };
		const name = native.productName || `USB MIDI ${hex4(native.vendorId)}:${hex4(native.productId)}`;
		const maker = native.manufacturerName ?? '';
		let input: MIDIInput | null = null;
		let output: MIDIOutput | null = null;
		if (inEp !== null) {
			// @ts-expect-error internal constructor
			input = new MIDIInput(INTERNAL, `${key}:in`, name, maker, handle, sysex);
		}
		if (outEp !== null) {
			// @ts-expect-error internal constructor
			output = new MIDIOutput(INTERNAL, `${key}:out`, name, maker, handle);
		}
		openMidi.set(key, { usb, native, input, output, sysex });
	} catch {
		await usb.close().catch(() => {});
	}
}

/**
 * Requests access to the system's USB-MIDI devices.
 *
 * When a host chooser is registered (the brewser shell), the user is shown a
 * one-shot permission prompt; on approval, every connected USB-MIDI device is
 * exposed. Without a chooser (bare nx.js), access is granted directly.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Navigator/requestMIDIAccess
 */
export async function requestMIDIAccess(options: MIDIOptions = {}): Promise<MIDIAccess> {
	ensureUsb();
	const sysex = options.sysex === true;
	const chooser = getDeviceChooser();
	if (chooser) {
		const granted = await chooser({
			kind: 'midi',
			mode: 'confirm',
			title: sysex ? 'Allow MIDI access (with system-exclusive)?' : 'Allow MIDI access?',
			candidates: [{ id: '__grant__', name: sysex ? 'Allow MIDI (incl. SysEx)' : 'Allow MIDI devices' }],
		});
		if (granted == null) {
			throw new DOMException('MIDI access request was denied.', 'SecurityError');
		}
	}
	// Reconcile the cache against the currently-connected devices. This is what
	// makes requestMIDIAccess idempotent AND survives brewser app switches: a
	// device still connected from a prior call/app is REUSED (no re-claim of an
	// interface we already hold); one that has been unplugged is closed so its
	// interface is freed.
	const present = ($.usbGetDevices() as unknown as MidiNativeDevice[]).filter(isMidiDevice);
	const presentKeys = new Set(present.map(midiKey));
	for (const [key, entry] of openMidi) {
		if (!presentKeys.has(key)) {
			closeMidiEntry(entry);
			openMidi.delete(key);
		}
	}
	// Open new devices / reopen on a sysex change; already-open ones are reused.
	await Promise.all(present.map((n) => ensureMidiOpen(n, sysex)));
	// Build the access maps from the cache so `access.inputs`/`outputs` are ready
	// when the caller inspects them right after `await requestMIDIAccess()`.
	const inputs = new Map<string, MIDIInput>();
	const outputs = new Map<string, MIDIOutput>();
	for (const [key, entry] of openMidi) {
		if (entry.input) inputs.set(`${key}:in`, entry.input);
		if (entry.output) outputs.set(`${key}:out`, entry.output);
	}
	// @ts-expect-error internal constructor
	return new MIDIAccess(INTERNAL, inputs, outputs, sysex);
}
