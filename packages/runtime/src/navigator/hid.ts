import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { getDeviceChooser } from './device-chooser';
import { INTERNAL_SYMBOL as INTERNAL } from '../internal';
import { USBDevice } from './usb';
import { EventTarget } from '../polyfills/event-target';
import { Event } from '../polyfills/event';
import { setTimeout } from '../timers';
import { assertInternalConstructor, def } from '../utils';
import type { BufferSource } from '../types';

// WebHID over USB.
//
// A HID device here is a USB device exposing a HID-class interface (class
// 0x03) on the Switch's USB-C port, driven — like WebSerial — on top of the
// existing {@link USBDevice} transfer machinery:
//
//   - Input reports  ← interrupt IN endpoint (polled, dispatched as
//                       `inputreport` events)
//   - Output reports → interrupt OUT endpoint, or SET_REPORT (0x09) on EP0
//   - Feature reports ↔ GET_REPORT (0x01) / SET_REPORT (0x09) on EP0
//
// The HID report descriptor (GET_DESCRIPTOR type 0x22) is fetched and given a
// compact parse — enough to know whether report IDs are in use (so input
// reports split correctly) and to populate `HIDDevice.collections` with each
// top-level collection's usagePage/usage and the report IDs per report type.
// Per-item detail inside a report is not expanded (`items` is left empty).

const HID_CLASS = 0x03;

const REQ_GET_DESCRIPTOR = 0x06;
const REQ_GET_REPORT = 0x01;
const REQ_SET_REPORT = 0x09;
const DESC_TYPE_REPORT = 0x22;
const REPORT_TYPE_OUTPUT = 0x02;
const REPORT_TYPE_FEATURE = 0x03;

// --- public shapes --------------------------------------------------------

export interface HIDDeviceFilter {
	vendorId?: number;
	productId?: number;
	usagePage?: number;
	usage?: number;
}

export interface HIDDeviceRequestOptions {
	filters: HIDDeviceFilter[];
}

export interface HIDReportItem {
	// Per-item detail is not parsed in this implementation.
}

export interface HIDReportInfo {
	reportId: number;
	items: HIDReportItem[];
}

export interface HIDCollectionInfo {
	usagePage: number;
	usage: number;
	type: number;
	children: HIDCollectionInfo[];
	inputReports: HIDReportInfo[];
	outputReports: HIDReportInfo[];
	featureReports: HIDReportInfo[];
}

interface HIDNativeDevice {
	busId: number;
	deviceId: number;
	vendorId: number;
	productId: number;
	productName?: string;
	manufacturerName?: string;
	configurations: {
		interfaces: {
			interfaceNumber: number;
			alternates: { interfaceClass: number }[];
		}[];
	}[];
}

// --- usb init (shared native stack; idempotent) ---------------------------

let usbReady = false;
function ensureUsb() {
	if (usbReady) return;
	$.usbInit();
	usbReady = true;
	addEventListener('unload', () => {
		usbReady = false;
	});
}

function hex4(n: number): string {
	return (n >>> 0).toString(16).padStart(4, '0');
}

function deviceKey(n: HIDNativeDevice): string {
	return `${n.busId}:${n.deviceId}`;
}

function isHidLike(n: HIDNativeDevice): boolean {
	for (const config of n.configurations ?? []) {
		for (const iface of config.interfaces ?? []) {
			for (const alt of iface.alternates ?? []) {
				if (alt.interfaceClass === HID_CLASS) return true;
			}
		}
	}
	return false;
}

function toU8(data: BufferSource): Uint8Array {
	if (data instanceof Uint8Array) return data;
	if (data instanceof ArrayBuffer) return new Uint8Array(data);
	const view = data as ArrayBufferView;
	return new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
}

// --- report descriptor parse ----------------------------------------------

interface AccReport {
	reportId: number;
	bits: number;
}
interface AccCollection {
	usagePage: number;
	usage: number;
	type: number;
	inputReports: AccReport[];
	outputReports: AccReport[];
	featureReports: AccReport[];
	children: AccCollection[];
}
interface ParsedDescriptor {
	usesReportIds: boolean;
	collections: HIDCollectionInfo[];
}

function accReport(arr: AccReport[], id: number): AccReport {
	let r = arr.find((x) => x.reportId === id);
	if (!r) {
		r = { reportId: id, bits: 0 };
		arr.push(r);
	}
	return r;
}

/**
 * Compact HID report-descriptor parse. Extracts top-level collections
 * (usagePage/usage/type) and, per report type, the report IDs seen with their
 * accumulated byte length. Sufficient for report-ID-aware I/O; individual
 * report items are not expanded.
 */
function parseReportDescriptor(bytes: Uint8Array): ParsedDescriptor {
	let usagePage = 0;
	let reportSize = 0;
	let reportCount = 0;
	let reportId = 0;
	let usesReportIds = false;
	let localUsages: number[] = [];
	const top: AccCollection[] = [];
	const stack: AccCollection[] = [];
	const currentTop = (): AccCollection | null =>
		stack.length ? stack[0] : top.length ? top[top.length - 1] : null;

	let i = 0;
	while (i < bytes.length) {
		const prefix = bytes[i++];
		if (prefix === 0xfe) {
			// Long item: [0xFE][dataSize][tag]<data...>
			const size = bytes[i] ?? 0;
			i += 2 + size;
			continue;
		}
		const bSize = prefix & 0x03;
		const dataLen = bSize === 3 ? 4 : bSize;
		const bType = (prefix >> 2) & 0x03;
		const bTag = (prefix >> 4) & 0x0f;
		let data = 0;
		for (let b = 0; b < dataLen; b++) data |= (bytes[i + b] ?? 0) << (8 * b);
		data >>>= 0;
		i += dataLen;

		if (bType === 0) {
			// Main
			if (bTag === 0x0a) {
				// Collection
				const col: AccCollection = {
					usagePage,
					usage: localUsages[0] ?? 0,
					type: data,
					inputReports: [],
					outputReports: [],
					featureReports: [],
					children: [],
				};
				if (stack.length) stack[stack.length - 1].children.push(col);
				else top.push(col);
				stack.push(col);
			} else if (bTag === 0x0c) {
				// End Collection
				stack.pop();
			} else if (bTag === 0x08 || bTag === 0x09 || bTag === 0x0b) {
				// Input / Output / Feature
				const col = currentTop();
				if (col) {
					const arr =
						bTag === 0x08
							? col.inputReports
							: bTag === 0x09
								? col.outputReports
								: col.featureReports;
					accReport(arr, reportId).bits += reportSize * reportCount;
				}
			}
			localUsages = [];
		} else if (bType === 1) {
			// Global
			if (bTag === 0x0) usagePage = data;
			else if (bTag === 0x7) reportSize = data;
			else if (bTag === 0x8) {
				reportId = data;
				usesReportIds = true;
			} else if (bTag === 0x9) reportCount = data;
		} else if (bType === 2) {
			// Local
			if (bTag === 0x0) localUsages.push(data);
		}
	}

	const finalize = (col: AccCollection): HIDCollectionInfo => {
		const reports = (rs: AccReport[]): HIDReportInfo[] =>
			rs.map((r) => ({ reportId: r.reportId, items: [] }));
		return {
			usagePage: col.usagePage,
			usage: col.usage,
			type: col.type,
			inputReports: reports(col.inputReports),
			outputReports: reports(col.outputReports),
			featureReports: reports(col.featureReports),
			children: col.children.map(finalize),
		};
	};
	return { usesReportIds, collections: top.map(finalize) };
}

// --- HIDInputReportEvent ---------------------------------------------------

/**
 * Fired at a {@link HIDDevice} when an input report arrives.
 *
 * @see https://developer.mozilla.org/docs/Web/API/HIDInputReportEvent
 */
export class HIDInputReportEvent extends Event {
	readonly device: HIDDevice;
	readonly reportId: number;
	readonly data: DataView;

	constructor(type: string, init: { device: HIDDevice; reportId: number; data: DataView }) {
		super(type);
		this.device = init.device;
		this.reportId = init.reportId;
		this.data = init.data;
	}
}
def(HIDInputReportEvent);

// --- HIDDevice -------------------------------------------------------------

/**
 * A USB HID device, as returned by
 * {@link HID.requestDevice | `navigator.hid.requestDevice()`}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice
 */
export class HIDDevice extends EventTarget {
	#device: USBDevice;
	#opened = false;
	#closing = false;
	#hidInterface = 0;
	#inEndpoint: number | null = null;
	#outEndpoint: number | null = null;
	#inPacketSize = 64;
	#usesReportIds = false;
	#collections: HIDCollectionInfo[] = [];

	readonly vendorId: number;
	readonly productId: number;
	readonly productName: string;

	oninputreport: ((this: HIDDevice, ev: HIDInputReportEvent) => any) | null = null;

	constructor(_internal: symbol, device?: USBDevice) {
		super();
		assertInternalConstructor(arguments);
		this.#device = device!;
		this.vendorId = device!.vendorId;
		this.productId = device!.productId;
		this.productName = device!.productName ?? '';
	}

	get opened(): boolean {
		return this.#opened;
	}

	get collections(): HIDCollectionInfo[] {
		return this.#collections;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice/open
	 */
	async open(): Promise<void> {
		if (this.#opened) return;
		const device = this.#device;
		await device.open();
		const config = device.configuration ?? device.configurations[0];
		let hidIface: (typeof config.interfaces)[number] | undefined;
		for (const iface of config.interfaces) {
			const alt = iface.alternate ?? iface.alternates[0];
			if (alt && alt.interfaceClass === HID_CLASS) {
				hidIface = iface;
				break;
			}
		}
		if (!hidIface) {
			await device.close().catch(() => {});
			throw new DOMException('This USB device has no HID interface.', 'NotSupportedError');
		}
		this.#hidInterface = hidIface.interfaceNumber;
		const alt = hidIface.alternate ?? hidIface.alternates[0];
		for (const ep of alt.endpoints) {
			if (ep.type !== 'interrupt') continue;
			if (ep.direction === 'in' && this.#inEndpoint === null) {
				this.#inEndpoint = ep.endpointNumber;
				this.#inPacketSize = ep.packetSize || 64;
			} else if (ep.direction === 'out' && this.#outEndpoint === null) {
				this.#outEndpoint = ep.endpointNumber;
			}
		}
		await device.claimInterface(this.#hidInterface);

		// Fetch + parse the report descriptor (best-effort). Request a generous
		// length; the device returns min(requested, actual).
		try {
			const res = await device.controlTransferIn(
				{ requestType: 'standard', recipient: 'interface', request: REQ_GET_DESCRIPTOR, value: (DESC_TYPE_REPORT << 8) | 0, index: this.#hidInterface },
				4096,
			);
			if (res.data && res.data.byteLength > 0) {
				const bytes = new Uint8Array(
					res.data.buffer.slice(res.data.byteOffset, res.data.byteOffset + res.data.byteLength),
				);
				const parsed = parseReportDescriptor(bytes);
				this.#usesReportIds = parsed.usesReportIds;
				this.#collections = parsed.collections;
			}
		} catch {
			// Descriptor unavailable — proceed with no report IDs / empty
			// collections; raw report I/O still works.
		}

		this.#closing = false;
		this.#opened = true;
		if (this.#inEndpoint !== null) this.#startInputLoop();
	}

	#startInputLoop(): void {
		const device = this.#device;
		const inEp = this.#inEndpoint!;
		const pump = async (): Promise<void> => {
			if (this.#closing || !this.#opened) return;
			try {
				const res = await device.transferIn(inEp, this.#inPacketSize);
				if (this.#closing) return;
				const d = res.data;
				if (d && d.byteLength > 0) {
					const bytes = new Uint8Array(
						d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength),
					);
					let reportId = 0;
					let data: DataView;
					if (this.#usesReportIds && bytes.length > 0) {
						reportId = bytes[0];
						data = new DataView(bytes.buffer, 1);
					} else {
						data = new DataView(bytes.buffer);
					}
					const ev = new HIDInputReportEvent('inputreport', { device: this, reportId, data });
					this.dispatchEvent(ev);
				}
				setTimeout(pump, 0);
			} catch {
				// Endpoint aborted (close) or transient error — stop the loop.
			}
		};
		void pump();
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice/sendReport
	 */
	async sendReport(reportId: number, data: BufferSource): Promise<void> {
		if (!this.#opened) {
			throw new DOMException('The device must be opened first.', 'InvalidStateError');
		}
		const bytes = toU8(data);
		if (this.#outEndpoint !== null) {
			let payload = bytes;
			if (reportId !== 0) {
				payload = new Uint8Array(bytes.length + 1);
				payload[0] = reportId & 0xff;
				payload.set(bytes, 1);
			}
			await this.#device.transferOut(this.#outEndpoint, payload);
			return;
		}
		await this.#device.controlTransferOut(
			{ requestType: 'class', recipient: 'interface', request: REQ_SET_REPORT, value: (REPORT_TYPE_OUTPUT << 8) | (reportId & 0xff), index: this.#hidInterface },
			bytes,
		);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice/sendFeatureReport
	 */
	async sendFeatureReport(reportId: number, data: BufferSource): Promise<void> {
		if (!this.#opened) {
			throw new DOMException('The device must be opened first.', 'InvalidStateError');
		}
		await this.#device.controlTransferOut(
			{ requestType: 'class', recipient: 'interface', request: REQ_SET_REPORT, value: (REPORT_TYPE_FEATURE << 8) | (reportId & 0xff), index: this.#hidInterface },
			toU8(data),
		);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice/receiveFeatureReport
	 */
	async receiveFeatureReport(reportId: number): Promise<DataView> {
		if (!this.#opened) {
			throw new DOMException('The device must be opened first.', 'InvalidStateError');
		}
		const res = await this.#device.controlTransferIn(
			{ requestType: 'class', recipient: 'interface', request: REQ_GET_REPORT, value: (REPORT_TYPE_FEATURE << 8) | (reportId & 0xff), index: this.#hidInterface },
			64,
		);
		const raw = res.data
			? new Uint8Array(res.data.buffer.slice(res.data.byteOffset, res.data.byteOffset + res.data.byteLength))
			: new Uint8Array(0);
		// WebHID prepends the report ID as byte 0.
		const out = new Uint8Array(raw.length + 1);
		out[0] = reportId & 0xff;
		out.set(raw, 1);
		return new DataView(out.buffer);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HIDDevice/close
	 */
	async close(): Promise<void> {
		this.#closing = true;
		try {
			await this.#device.close();
		} catch {}
		this.#opened = false;
	}

	/** No persistent permission store on Switch; `forget()` closes the device. */
	async forget(): Promise<void> {
		await this.close();
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'inputreport') {
			this.oninputreport?.(event as HIDInputReportEvent);
		}
		return super.dispatchEvent(event);
	}
}
def(HIDDevice);

// --- HID (navigator.hid) ---------------------------------------------------

function collectDevices(filters: HIDDeviceFilter[]): HIDNativeDevice[] {
	const list = filters.length ? filters : [{}];
	const requireHid = filters.length === 0;
	const seen = new Set<string>();
	const out: HIDNativeDevice[] = [];
	for (const f of list) {
		const usbFilter: { vendorId?: number; productId?: number } = {};
		if (typeof f.vendorId === 'number') usbFilter.vendorId = f.vendorId;
		if (typeof f.productId === 'number') usbFilter.productId = f.productId;
		for (const native of $.usbGetDevices(usbFilter) as unknown as HIDNativeDevice[]) {
			const k = deviceKey(native);
			if (seen.has(k)) continue;
			// usagePage/usage filtering would require the report descriptor
			// (only available after open), so it isn't applied at enumeration.
			if (!isHidLike(native)) {
				if (requireHid || !(typeof f.vendorId === 'number' || typeof f.productId === 'number')) continue;
			}
			seen.add(k);
			out.push(native);
		}
	}
	return out;
}

const grantedDevices: HIDDevice[] = [];

/**
 * Entry point for the WebHID API, available as `navigator.hid`.
 *
 * @see https://developer.mozilla.org/docs/Web/API/HID
 */
export class HID extends EventTarget {
	onconnect: ((this: HID, ev: Event) => any) | null = null;
	ondisconnect: ((this: HID, ev: Event) => any) | null = null;

	constructor(_internal: symbol) {
		super();
		assertInternalConstructor(arguments);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/HID/getDevices
	 */
	async getDevices(): Promise<HIDDevice[]> {
		return grantedDevices.slice();
	}

	/**
	 * Prompts the user to select HID devices. Returns an array (per spec),
	 * containing the single chosen device — or empty if cancelled.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/HID/requestDevice
	 */
	async requestDevice(options: HIDDeviceRequestOptions): Promise<HIDDevice[]> {
		ensureUsb();
		const filters = options?.filters ?? [];
		if (!Array.isArray(filters)) {
			throw new TypeError('HID device filters must be an array');
		}
		const devices = collectDevices(filters);
		if (devices.length === 0) return [];
		const chooser = getDeviceChooser();
		let chosen: HIDNativeDevice | undefined;
		if (!chooser) {
			chosen = devices[0];
		} else {
			const chosenId = await chooser({
				kind: 'hid',
				mode: 'select',
				title: 'Select an HID device',
				candidates: devices.map((n) => ({
					id: deviceKey(n),
					name: n.productName || `HID device ${hex4(n.vendorId)}:${hex4(n.productId)}`,
					detail: `${hex4(n.vendorId)}:${hex4(n.productId)}`,
				})),
			});
			if (chosenId == null) return [];
			chosen = devices.find((n) => deviceKey(n) === chosenId);
			if (!chosen) return [];
		}
		const usb = new USBDevice(INTERNAL, chosen as unknown as ConstructorParameters<typeof USBDevice>[1]);
		const device = new HIDDevice(INTERNAL, usb);
		grantedDevices.push(device);
		return [device];
	}
}
def(HID);

/** The shared `HID` instance, also exposed as `navigator.hid`. */
// @ts-expect-error internal constructor
export const hid = new HID(INTERNAL);
