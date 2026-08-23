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

// --- CP210x (Silicon Labs) vendor protocol --------------------------------
const CP210X_VID = 0x10c4;
const CP210X_IFC_ENABLE = 0x00; // wValue: 1 = enable UART, 0 = disable
const CP210X_SET_LINE_CTL = 0x03; // wValue: (dataBits<<8)|(parity<<4)|stopBits
const CP210X_SET_BREAK = 0x05; // wValue: 1 = break on, 0 = off
const CP210X_SET_MHS = 0x07; // wValue: (mask<<8)|state; bit0 DTR, bit1 RTS
const CP210X_SET_BAUDRATE = 0x1e; // 4-byte LE baud in the data stage

// --- CH340 / CH341 (WCH) vendor protocol ----------------------------------
const CH340_VID = 0x1a86;
const CH341_READ_VERSION = 0x5f;
const CH341_WRITE_REG = 0x9a;
const CH341_SERIAL_INIT = 0xa1;
const CH341_MODEM_CTRL = 0xa4; // wValue = ~((DTR?0x20:0)|(RTS?0x40:0))

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
	// With no filters, restrict to serial-capable devices (CDC-ACM or a known
	// USB-UART bridge chip) so the picker isn't cluttered with unrelated
	// peripherals. With explicit vid/pid filters, trust the caller and list
	// every match (open() still validates the transport).
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
			if (requireCdc && !isCdcLike(native) && !isKnownBridge(native.vendorId)) continue;
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
	inPacketSize: number; // bulk IN wMaxPacketSize (read length must be a multiple)
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
		inPacketSize: inEp.packetSize,
	};
}

// ---------------------------------------------------------------------------
// USB-UART bridge chips (non-CDC): CP210x (Silicon Labs) and CH340/CH341 (WCH).
// These enumerate as a single vendor-specific interface (class 0xff) with a
// bulk IN/OUT pair and no CDC class descriptors, so they need a chip-specific
// open sequence. Data still flows over the bulk endpoints exactly like CDC —
// only the control requests (enable / baud / line format / DTR-RTS) differ.
// ---------------------------------------------------------------------------

interface SerialLayout {
	kind: 'cdc' | 'cp210x' | 'ch340';
	/** Interface the control requests target (CDC comm iface, or the bridge's
	 *  single vendor interface). */
	controlInterface: number;
	/** Interface owning the bulk data endpoints (claimed on open). */
	dataInterface: number;
	inEndpoint: number;
	outEndpoint: number;
	/** bulk IN wMaxPacketSize — read length must be rounded to a multiple. */
	inPacketSize: number;
}

interface SerialConfig {
	baudRate: number;
	dataBits: number;
	stopBits: number;
	parity: ParityType;
}

/** True for USB vendor ids we can drive with a bridge-chip serial backend. */
function isKnownBridge(vendorId: number): boolean {
	return vendorId === CP210X_VID || vendorId === CH340_VID;
}

/** First interface exposing both a bulk IN and a bulk OUT endpoint. */
function findBulkInterface(
	device: USBDevice,
): { iface: number; inEp: number; outEp: number; inPacket: number } | null {
	const config = device.configuration ?? device.configurations[0];
	if (!config) return null;
	for (const iface of config.interfaces) {
		const alt = iface.alternate ?? iface.alternates[0];
		if (!alt) continue;
		const inEp = alt.endpoints.find((e) => e.type === 'bulk' && e.direction === 'in');
		const outEp = alt.endpoints.find((e) => e.type === 'bulk' && e.direction === 'out');
		if (inEp && outEp) {
			return {
				iface: iface.interfaceNumber,
				inEp: inEp.endpointNumber,
				outEp: outEp.endpointNumber,
				inPacket: inEp.packetSize,
			};
		}
	}
	return null;
}

/** Pick the transport backend for an opened device: CDC-ACM first, then a
 *  known USB-UART bridge chip. Returns null for anything else. */
function resolveLayout(device: USBDevice): SerialLayout | null {
	const cdc = resolveCdcLayout(device);
	if (cdc) {
		return {
			kind: 'cdc',
			controlInterface: cdc.commInterface,
			dataInterface: cdc.dataInterface,
			inEndpoint: cdc.inEndpoint,
			outEndpoint: cdc.outEndpoint,
			inPacketSize: cdc.inPacketSize,
		};
	}
	const bulk = findBulkInterface(device);
	if (!bulk) return null;
	const kind: SerialLayout['kind'] | null =
		device.vendorId === CP210X_VID
			? 'cp210x'
			: device.vendorId === CH340_VID
				? 'ch340'
				: null;
	if (!kind) return null;
	return {
		kind,
		controlInterface: bulk.iface,
		dataInterface: bulk.iface,
		inEndpoint: bulk.inEp,
		outEndpoint: bulk.outEp,
		inPacketSize: bulk.inPacket,
	};
}

// vendor control-transfer shorthands (recipient defaults to the interface)
function vOut(
	device: USBDevice,
	request: number,
	value: number,
	index: number,
	data?: BufferSource,
	recipient: 'interface' | 'device' = 'interface',
) {
	return device.controlTransferOut(
		{ requestType: 'vendor', recipient, request, value, index },
		data,
	);
}
function vIn(
	device: USBDevice,
	request: number,
	value: number,
	index: number,
	length: number,
	recipient: 'interface' | 'device' = 'interface',
) {
	return device.controlTransferIn(
		{ requestType: 'vendor', recipient, request, value, index },
		length,
	);
}

/** Encode data/parity/stop into each chip's line-control word. */
function lineCtlBits(
	dataBits: number,
	parity: ParityType,
	stopBits: number,
): { cp210x: number; ch341: number } {
	const cp210x =
		(dataBits << 8) |
		((parity === 'odd' ? 1 : parity === 'even' ? 2 : 0) << 4) |
		(stopBits === 2 ? 2 : 0);
	// CH341 LCR: enable RX (0x80) + TX (0x40) + word length (CS5..CS8 = 0..3).
	let ch341 = 0x80 | 0x40 | ((Math.max(5, Math.min(8, dataBits)) - 5) & 0x03);
	if (parity !== 'none') {
		ch341 |= 0x08;
		if (parity === 'even') ch341 |= 0x10;
	}
	if (stopBits === 2) ch341 |= 0x04;
	return { cp210x, ch341 };
}

/** Enable the CP210x UART and set baud / line format. */
async function configureCp210x(
	device: USBDevice,
	L: SerialLayout,
	o: SerialConfig,
): Promise<void> {
	const idx = L.controlInterface;
	await vOut(device, CP210X_IFC_ENABLE, 0x0001, idx);
	const baud = new Uint8Array(4);
	new DataView(baud.buffer).setUint32(0, o.baudRate, true);
	await vOut(device, CP210X_SET_BAUDRATE, 0x0000, idx, baud);
	await vOut(device, CP210X_SET_LINE_CTL, lineCtlBits(o.dataBits, o.parity, o.stopBits).cp210x, idx);
}

/** CH341 baud registers (older ch341.c algorithm — accurate for the standard
 *  rates 9600..921600). Returns the prescaler/divisor register writes. */
function ch340BaudRegs(baud: number): { prescaler: number; divisor: number } {
	let factor = Math.floor(1532620800 / baud);
	let div = 3;
	while (factor > 0xfff0 && div > 0) {
		factor = Math.floor(factor / 8);
		div--;
	}
	if (factor > 0xfff0) {
		throw new DOMException('Unsupported CH340 baud rate.', 'NotSupportedError');
	}
	factor = 0x10000 - factor;
	return { prescaler: (factor & 0xff00) | div, divisor: factor & 0xff };
}

/** Initialise a CH340/CH341 and set baud / line format. All CH34x control
 *  requests are device-recipient (protocol per Linux ch341.c). NOTE: not yet
 *  exercised on real hardware — verify against a physical CH340 board. */
async function configureCh340(
	device: USBDevice,
	_L: SerialLayout,
	o: SerialConfig,
): Promise<void> {
	await vIn(device, CH341_READ_VERSION, 0, 0, 2, 'device').catch(() => {});
	await vOut(device, CH341_SERIAL_INIT, 0, 0, undefined, 'device');
	const { prescaler, divisor } = ch340BaudRegs(o.baudRate);
	await vOut(device, CH341_WRITE_REG, 0x1312, prescaler, undefined, 'device');
	await vOut(device, CH341_WRITE_REG, 0x0f2c, divisor, undefined, 'device');
	await vOut(device, CH341_WRITE_REG, 0x2518, lineCtlBits(o.dataBits, o.parity, o.stopBits).ch341, undefined, 'device');
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
	#layout: SerialLayout | null = null;
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
		const layout = resolveLayout(device);
		if (!layout) {
			await device.close().catch(() => {});
			throw new DOMException(
				'This USB device is not a supported serial adapter. Only CDC-ACM, CP210x, and CH340/CH341 USB-UART chips are handled (FTDI, PL2303 not yet).',
				'NotSupportedError',
			);
		}
		this.#layout = layout;

		const cfg: SerialConfig = { baudRate: options.baudRate, dataBits, stopBits, parity };
		if (layout.kind === 'cdc') {
			// Claim the data interface (owns the bulk pipes); best-effort claim the
			// comm interface so the OS doesn't hold it (control transfers ride EP0
			// and lazily acquire an interface if none is claimed, so a failure here
			// is non-fatal).
			if (layout.controlInterface !== layout.dataInterface) {
				await device.claimInterface(layout.controlInterface).catch(() => {});
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
					{ requestType: 'class', recipient: 'interface', request: REQ_SET_LINE_CODING, value: 0, index: layout.controlInterface },
					coding,
				)
				.catch(() => {});
		} else {
			// USB-UART bridge chip: claim its single vendor interface, then run the
			// chip-specific enable + baud + line-format sequence.
			await device.claimInterface(layout.dataInterface);
			if (layout.kind === 'cp210x') await configureCp210x(device, layout, cfg);
			else await configureCh340(device, layout, cfg);
		}

		// Assert DTR + RTS on open: many devices (ESP32 native USB, Arduino, and
		// bridge-chip boards) only stream once a terminal signals it's present.
		// Apps can override via setSignals().
		this.#dtr = true;
		this.#rts = true;
		await this.#writeControlLineState().catch(() => {});

		this.#setupStreams(bufferSize);
		this.#closing = false;
		this.#open = true;
	}

	async #writeControlLineState(): Promise<void> {
		const L = this.#layout;
		if (!L) return;
		const dtr = this.#dtr;
		const rts = this.#rts;
		if (L.kind === 'cdc') {
			await this.#device.controlTransferOut({
				requestType: 'class',
				recipient: 'interface',
				request: REQ_SET_CONTROL_LINE_STATE,
				value: (dtr ? 0x01 : 0) | (rts ? 0x02 : 0),
				index: L.controlInterface,
			});
		} else if (L.kind === 'cp210x') {
			// SET_MHS: high byte is the write mask (DTR+RTS), low byte the state.
			const state = (dtr ? 0x01 : 0) | (rts ? 0x02 : 0);
			await vOut(this.#device, CP210X_SET_MHS, (0x03 << 8) | state, L.controlInterface);
		} else {
			// CH341 MODEM_CTRL takes the inverted DTR(0x20)/RTS(0x40) bits.
			const control = (dtr ? 0x20 : 0) | (rts ? 0x40 : 0);
			await vOut(this.#device, CH341_MODEM_CTRL, ~control & 0xff, 0, undefined, 'device');
		}
	}

	#setupStreams(bufferSize: number): void {
		const device = this.#device;
		const inEp = this.#layout!.inEndpoint;
		const outEp = this.#layout!.outEndpoint;
		// A bulk IN transfer length MUST be a whole multiple of the endpoint's
		// max packet size — otherwise the host controller overflows the moment a
		// full packet arrives (module 140 "usb bulk IN transfer failed"). Round
		// the requested buffer up to the next multiple (at least one packet).
		const maxPacket = this.#layout!.inPacketSize || 64;
		let readLen = Math.max(maxPacket, Math.ceil(bufferSize / maxPacket) * maxPacket);
		// Cap the per-transfer read length to a modest, browser-like size. WebSerial
		// `bufferSize` is the size of the internal read buffer, NOT the USB transfer
		// length — Chrome reads in small chunks and loops. A huge bufferSize (e.g.
		// 65536) must NOT become a giant per-URB read: every poll would allocate a
		// DMA buffer that big, and it also has to fit WebUSB's 16-bit length word
		// (transferIn() rejects > 0xffff, which would throw on the first pull and
		// kill the read stream). One frame-sized read per device write is plenty;
		// anything larger is just wasted allocation and setup.
		const READ_CAP = 4096;
		const maxReadLen = Math.max(maxPacket, Math.floor(READ_CAP / maxPacket) * maxPacket);
		if (readLen > maxReadLen) readLen = maxReadLen;

		this.#readable = new ReadableStream<Uint8Array>({
			pull: async (controller) => {
				if (this.#closing) {
					try { controller.close(); } catch {}
					return;
				}
				try {
					const result = await device.transferIn(inEp, readLen);
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
			const L = this.#layout;
			if (L.kind === 'cdc') {
				await this.#device
					.controlTransferOut({
						requestType: 'class',
						recipient: 'interface',
						request: REQ_SEND_BREAK,
						value: signals.break ? 0xffff : 0x0000,
						index: L.controlInterface,
					})
					.catch(() => {});
			} else if (L.kind === 'cp210x') {
				await vOut(this.#device, CP210X_SET_BREAK, signals.break ? 0x0001 : 0x0000, L.controlInterface).catch(() => {});
			}
			// CH340: break signalling not implemented — ignore.
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
