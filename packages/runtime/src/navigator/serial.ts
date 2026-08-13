import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { getDeviceChooser } from './device-chooser';
import { INTERNAL_SYMBOL as INTERNAL } from '../internal';
import { USBDevice } from './usb';
import { EventTarget } from '../polyfills/event-target';
import { Event } from '../polyfills/event';
import { assertInternalConstructor, def } from '../utils';

// Web Serial over USB CDC-ACM.
//
// The Switch has no general-purpose UART exposed to homebrew (the `uart:`
// service only reaches internal Joy-Con / Bluetooth / MCU ports), so a serial
// "port" here is a USB CDC-ACM device on the USB-C port — reached through the
// same `usb:hs` host stack WebUSB uses. Rather than re-implement the transfer
// machinery, each `SerialPort` drives a {@link USBDevice} (inheriting its
// non-blocking `transferIn`, claim-with-backoff, and control-transfer paths),
// speaking the CDC-ACM class protocol on top:
//
//   - SET_LINE_CODING (0x20)        — baud / data bits / parity / stop bits
//   - SET_CONTROL_LINE_STATE (0x22) — DTR / RTS
//   - SEND_BREAK (0x23)             — break signal
//
// ## Scope
//
// This covers standard **CDC-ACM** adapters — ESP32-S3/C3/C6 native USB,
// Arduino (16U2 / native), Raspberry Pi Pico, micro:bit, and anything that
// enumerates as USB class 0x02/0x0A. Vendor-specific USB-serial chips (FTDI,
// CP210x, CH340/CH341, PL2303) speak proprietary control protocols and are
// **not** handled yet — `open()` rejects them with `NotSupportedError`.

// --- CDC-ACM constants ----------------------------------------------------

const CDC_COMM_CLASS = 0x02; // Communications interface class
const CDC_ACM_SUBCLASS = 0x02; // Abstract Control Model
const CDC_DATA_CLASS = 0x0a; // CDC-Data interface class

const REQ_SET_LINE_CODING = 0x20;
const REQ_SET_CONTROL_LINE_STATE = 0x22;
const REQ_SEND_BREAK = 0x23;

// --- public option / info shapes ------------------------------------------

export type ParityType = 'none' | 'even' | 'odd';
export type FlowControlType = 'none' | 'hardware';

export interface SerialOptions {
	baudRate: number;
	dataBits?: number; // 7 | 8, default 8
	stopBits?: number; // 1 | 2, default 1
	parity?: ParityType; // default 'none'
	bufferSize?: number; // read chunk size, default 255
	flowControl?: FlowControlType; // default 'none'
}

export interface SerialPortInfo {
	usbVendorId?: number;
	usbProductId?: number;
}

export interface SerialPortFilter {
	usbVendorId?: number;
	usbProductId?: number;
}

export interface SerialPortRequestOptions {
	filters?: SerialPortFilter[];
}

export interface SerialInputSignals {
	dataCarrierDetect: boolean;
	clearToSend: boolean;
	ringIndicator: boolean;
	dataSetReady: boolean;
}

export interface SerialOutputSignals {
	dataTerminalReady?: boolean;
	requestToSend?: boolean;
	break?: boolean;
}

// Minimal view of the native descriptor returned by `$.usbGetDevices` — only
// the fields the serial layer reads (the full shape lives in usb.ts).
interface SerialNativeDevice {
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
			}[];
		}[];
	}[];
}

// --- usb init (shared native stack; idempotent) ---------------------------

let usbReady = false;
function ensureUsb() {
	if (usbReady) return;
	// `nx_usb_init` → `ensure_usb` is guarded by a native `g_initialized` flag,
	// so this is safe even when `navigator.usb` already initialised the stack.
	$.usbInit();
	usbReady = true;
	addEventListener('unload', () => {
		usbReady = false;
	});
}

function hex4(n: number): string {
	return (n >>> 0).toString(16).padStart(4, '0');
}

function deviceKey(n: SerialNativeDevice): string {
	return `${n.busId}:${n.deviceId}`;
}

function portName(n: SerialNativeDevice): string {
	return n.productName || `Serial port ${hex4(n.vendorId)}:${hex4(n.productId)}`;
}

/** True if the device advertises a CDC comm (ACM) or CDC-data interface. */
function isCdcLike(n: SerialNativeDevice): boolean {
	for (const config of n.configurations ?? []) {
		for (const iface of config.interfaces ?? []) {
			for (const alt of iface.alternates ?? []) {
				if (
					(alt.interfaceClass === CDC_COMM_CLASS &&
						alt.interfaceSubclass === CDC_ACM_SUBCLASS) ||
					alt.interfaceClass === CDC_DATA_CLASS
				) {
					return true;
				}
			}
		}
	}
	return false;
}

function collectDevices(filters: SerialPortFilter[]): SerialNativeDevice[] {
	const list = filters.length ? filters : [{}];
	// With no filters, restrict to CDC-like devices so the picker isn't
	// cluttered with unrelated peripherals. With explicit vid/pid filters,
	// trust the caller and list every match (open() still validates CDC).
	const requireCdc = filters.length === 0;
	const seen = new Set<string>();
	const out: SerialNativeDevice[] = [];
	for (const f of list) {
		const usbFilter: { vendorId?: number; productId?: number } = {};
		if (typeof f.usbVendorId === 'number') usbFilter.vendorId = f.usbVendorId;
		if (typeof f.usbProductId === 'number') usbFilter.productId = f.usbProductId;
		for (const native of $.usbGetDevices(usbFilter) as unknown as SerialNativeDevice[]) {
			const k = deviceKey(native);
			if (seen.has(k)) continue;
			if (requireCdc && !isCdcLike(native)) continue;
			seen.add(k);
			out.push(native);
		}
	}
	return out;
}

// --- SerialPort -----------------------------------------------------------

interface CdcLayout {
	commInterface: number; // interface # for class control requests
	dataInterface: number; // interface # owning the bulk endpoints
	inEndpoint: number; // bulk IN
	outEndpoint: number; // bulk OUT
}

/** Locate the CDC bulk data path + control interface on an opened device. */
function resolveCdcLayout(device: USBDevice): CdcLayout | null {
	const config = device.configuration ?? device.configurations[0];
	if (!config) return null;
	let commNum: number | null = null;
	let dataIface: (typeof config.interfaces)[number] | null = null;
	for (const iface of config.interfaces) {
		const alt = iface.alternate ?? iface.alternates[0];
		if (!alt) continue;
		if (
			alt.interfaceClass === CDC_COMM_CLASS &&
			alt.interfaceSubclass === CDC_ACM_SUBCLASS
		) {
			commNum = iface.interfaceNumber;
		} else if (alt.interfaceClass === CDC_DATA_CLASS && !dataIface) {
			dataIface = iface;
		}
	}
	// Fallback: no tagged CDC-data interface — take the first interface that
	// exposes both a bulk IN and bulk OUT endpoint.
	if (!dataIface) {
		for (const iface of config.interfaces) {
			const alt = iface.alternate ?? iface.alternates[0];
			if (!alt) continue;
			const hasIn = alt.endpoints.some((e) => e.type === 'bulk' && e.direction === 'in');
			const hasOut = alt.endpoints.some((e) => e.type === 'bulk' && e.direction === 'out');
			if (hasIn && hasOut) {
				dataIface = iface;
				break;
			}
		}
	}
	if (!dataIface) return null;
	const alt = dataIface.alternate ?? dataIface.alternates[0];
	const inEp = alt.endpoints.find((e) => e.type === 'bulk' && e.direction === 'in');
	const outEp = alt.endpoints.find((e) => e.type === 'bulk' && e.direction === 'out');
	if (!inEp || !outEp) return null;
	return {
		commInterface: commNum ?? dataIface.interfaceNumber,
		dataInterface: dataIface.interfaceNumber,
		inEndpoint: inEp.endpointNumber,
		outEndpoint: outEp.endpointNumber,
	};
}

/**
 * A serial port backed by a USB CDC-ACM device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/SerialPort
 */
export class SerialPort extends EventTarget {
	#device: USBDevice;
	#info: SerialPortInfo;
	#open = false;
	#closing = false;
	#layout: CdcLayout | null = null;
	#readable: ReadableStream<Uint8Array> | null = null;
	#writable: WritableStream<Uint8Array> | null = null;
	#dtr = false;
	#rts = false;

	onconnect: ((this: SerialPort, ev: Event) => any) | null = null;
	ondisconnect: ((this: SerialPort, ev: Event) => any) | null = null;

	constructor(_internal: symbol, device?: USBDevice) {
		super();
		assertInternalConstructor(arguments);
		this.#device = device!;
		this.#info = {
			usbVendorId: device!.vendorId,
			usbProductId: device!.productId,
		};
	}

	get readable(): ReadableStream<Uint8Array> | null {
		return this.#readable;
	}

	get writable(): WritableStream<Uint8Array> | null {
		return this.#writable;
	}

	get connected(): boolean {
		return this.#open && !this.#closing;
	}

	getInfo(): SerialPortInfo {
		return { ...this.#info };
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/SerialPort/open
	 */
	async open(options: SerialOptions): Promise<void> {
		if (this.#open) {
			throw new DOMException('The port is already open.', 'InvalidStateError');
		}
		if (!options || typeof options.baudRate !== 'number' || options.baudRate <= 0) {
			throw new TypeError("Failed to execute 'open' on 'SerialPort': required member baudRate must be a positive number.");
		}
		const dataBits = options.dataBits ?? 8;
		const stopBits = options.stopBits ?? 1;
		const parity = options.parity ?? 'none';
		const bufferSize = options.bufferSize ?? 255;

		const device = this.#device;
		await device.open();
		const layout = resolveCdcLayout(device);
		if (!layout) {
			await device.close().catch(() => {});
			throw new DOMException(
				'This USB device is not a CDC-ACM serial port. Vendor-specific adapters (FTDI, CP210x, CH340, PL2303) are not yet supported.',
				'NotSupportedError',
			);
		}
		this.#layout = layout;

		// Claim the data interface (owns the bulk pipes); best-effort claim the
		// comm interface so the OS doesn't hold it (control transfers ride EP0
		// and lazily acquire an interface if none is claimed, so a failure here
		// is non-fatal).
		if (layout.commInterface !== layout.dataInterface) {
			await device.claimInterface(layout.commInterface).catch(() => {});
		}
		await device.claimInterface(layout.dataInterface);

		// SET_LINE_CODING — 7-byte payload. Native-USB CDC devices often ignore
		// baud (their link is USB, not a real UART), so tolerate a STALL here.
		const coding = new Uint8Array(7);
		const dv = new DataView(coding.buffer);
		dv.setUint32(0, options.baudRate, true);
		dv.setUint8(4, stopBits === 2 ? 2 : 0); // 0 = 1 stop bit, 2 = 2 stop bits
		dv.setUint8(5, parity === 'odd' ? 1 : parity === 'even' ? 2 : 0);
		dv.setUint8(6, dataBits);
		await device
			.controlTransferOut(
				{ requestType: 'class', recipient: 'interface', request: REQ_SET_LINE_CODING, value: 0, index: layout.commInterface },
				coding,
			)
			.catch(() => {});

		// Assert DTR + RTS on open: many CDC devices (ESP32 native USB, Arduino)
		// only stream once a terminal signals it's present. Apps can override
		// via setSignals().
		this.#dtr = true;
		this.#rts = true;
		await this.#writeControlLineState().catch(() => {});

		this.#setupStreams(bufferSize);
		this.#closing = false;
		this.#open = true;
	}

	async #writeControlLineState(): Promise<void> {
		if (!this.#layout) return;
		const value = (this.#dtr ? 0x01 : 0) | (this.#rts ? 0x02 : 0);
		await this.#device.controlTransferOut({
			requestType: 'class',
			recipient: 'interface',
			request: REQ_SET_CONTROL_LINE_STATE,
			value,
			index: this.#layout.commInterface,
		});
	}

	#setupStreams(bufferSize: number): void {
		const device = this.#device;
		const inEp = this.#layout!.inEndpoint;
		const outEp = this.#layout!.outEndpoint;

		this.#readable = new ReadableStream<Uint8Array>({
			pull: async (controller) => {
				if (this.#closing) {
					try { controller.close(); } catch {}
					return;
				}
				try {
					const result = await device.transferIn(inEp, bufferSize);
					if (this.#closing) {
						try { controller.close(); } catch {}
						return;
					}
					const d = result.data;
					if (d && d.byteLength > 0) {
						// Copy out — the poll buffer is single-shot but the consumer
						// may retain the chunk past the next read.
						controller.enqueue(
							new Uint8Array(d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength)),
						);
					}
				} catch (err) {
					if (this.#closing) {
						try { controller.close(); } catch {}
					} else {
						controller.error(err as Error);
					}
				}
			},
		});

		this.#writable = new WritableStream<Uint8Array>({
			write: async (chunk) => {
				const u8 =
					chunk instanceof Uint8Array
						? chunk
						: new Uint8Array(chunk as unknown as ArrayBufferLike);
				await device.transferOut(outEp, u8);
			},
		});
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/SerialPort/setSignals
	 */
	async setSignals(signals: SerialOutputSignals = {}): Promise<void> {
		if (!this.#open) {
			throw new DOMException('The port is not open.', 'InvalidStateError');
		}
		if (typeof signals.dataTerminalReady === 'boolean') this.#dtr = signals.dataTerminalReady;
		if (typeof signals.requestToSend === 'boolean') this.#rts = signals.requestToSend;
		await this.#writeControlLineState().catch(() => {});
		if (typeof signals.break === 'boolean' && this.#layout) {
			await this.#device
				.controlTransferOut({
					requestType: 'class',
					recipient: 'interface',
					request: REQ_SEND_BREAK,
					value: signals.break ? 0xffff : 0x0000,
					index: this.#layout.commInterface,
				})
				.catch(() => {});
		}
	}

	/**
	 * Best-effort input signal snapshot. The CDC SERIAL_STATE notification
	 * (which carries DCD/DSR/RI/CTS) is not yet polled from the comm
	 * interface's interrupt endpoint, so these read as `false`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/SerialPort/getSignals
	 */
	async getSignals(): Promise<SerialInputSignals> {
		if (!this.#open) {
			throw new DOMException('The port is not open.', 'InvalidStateError');
		}
		return {
			dataCarrierDetect: false,
			clearToSend: false,
			ringIndicator: false,
			dataSetReady: false,
		};
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/SerialPort/close
	 */
	async close(): Promise<void> {
		this.#closing = true;
		try {
			if (this.#readable && !this.#readable.locked) await this.#readable.cancel();
		} catch {}
		try {
			if (this.#writable && !this.#writable.locked) await this.#writable.abort();
		} catch {}
		// Closing the device aborts any in-flight bulk-IN URB, which unblocks a
		// pending read (its poll throws → the stream closes via #closing).
		try {
			await this.#device.close();
		} catch {}
		this.#readable = null;
		this.#writable = null;
		this.#open = false;
	}

	/** No persistent permission store on Switch; `forget()` closes the port. */
	async forget(): Promise<void> {
		await this.close();
	}
}
def(SerialPort);

// --- Serial (navigator.serial) --------------------------------------------

/** Ports handed out this session (no persistent grant store on Switch). */
const grantedPorts: SerialPort[] = [];

/**
 * Entry point for the Web Serial API, available as `navigator.serial`.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Serial
 */
export class Serial extends EventTarget {
	onconnect: ((this: Serial, ev: Event) => any) | null = null;
	ondisconnect: ((this: Serial, ev: Event) => any) | null = null;

	constructor(_internal: symbol) {
		super();
		assertInternalConstructor(arguments);
	}

	/**
	 * Returns the ports granted this session. The Switch has no persistent
	 * permission store, so this reflects ports obtained via
	 * {@link Serial.requestPort} in the current run rather than across runs.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Serial/getPorts
	 */
	async getPorts(): Promise<SerialPort[]> {
		return grantedPorts.slice();
	}

	/**
	 * Prompts the user to select a serial port. When a host chooser is
	 * registered (the brewser shell), a picker lists the matching CDC-ACM
	 * devices; otherwise the first match is returned (bare nx.js).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Serial/requestPort
	 */
	async requestPort(options: SerialPortRequestOptions = {}): Promise<SerialPort> {
		ensureUsb();
		const filters = options.filters ?? [];
		if (!Array.isArray(filters)) {
			throw new TypeError('Serial port filters must be an array');
		}
		const devices = collectDevices(filters);
		if (devices.length === 0) {
			throw new DOMException('No serial ports found.', 'NotFoundError');
		}
		const chooser = getDeviceChooser();
		let chosen: SerialNativeDevice | undefined;
		if (!chooser) {
			chosen = devices[0];
		} else {
			const chosenId = await chooser({
				kind: 'serial',
				mode: 'select',
				title: 'Select a serial port',
				candidates: devices.map((n) => ({
					id: deviceKey(n),
					name: portName(n),
					detail: `${hex4(n.vendorId)}:${hex4(n.productId)}`,
				})),
			});
			if (chosenId == null) {
				throw new DOMException('No port was selected.', 'NotFoundError');
			}
			chosen = devices.find((n) => deviceKey(n) === chosenId);
			if (!chosen) {
				throw new DOMException('The selected port is no longer available.', 'NotFoundError');
			}
		}
		const device = new USBDevice(INTERNAL, chosen as unknown as ConstructorParameters<typeof USBDevice>[1]);
		const port = new SerialPort(INTERNAL, device);
		grantedPorts.push(port);
		return port;
	}
}
def(Serial);

/** The shared `Serial` instance, also exposed as `navigator.serial`. */
// @ts-expect-error internal constructor
export const serial = new Serial(INTERNAL);
