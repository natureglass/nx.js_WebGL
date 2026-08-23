import { $, type USBNativeDevice } from '../$';
import { DOMException } from '../dom-exception';
import { getDeviceChooser } from './device-chooser';
import { INTERNAL_SYMBOL } from '../internal';
import type { BufferSource } from '../types';
import { assertInternalConstructor, def } from '../utils';
import { setTimeout, setInterval, clearInterval } from '../timers';
import { EventTarget } from '../polyfills/event-target';
import { Event } from '../polyfills/event';

type USBControlTransferType = 'standard' | 'class' | 'vendor';
type USBRequestType = USBControlTransferType | 'reserved';
type USBRecipient = 'device' | 'interface' | 'endpoint' | 'other';
type USBTransferStatus = 'ok' | 'stall' | 'babble';
type USBDirection = 'in' | 'out';
type USBEndpointType = 'bulk' | 'interrupt' | 'isochronous';

export interface USBDeviceFilter {
	vendorId?: number;
	productId?: number;
	classCode?: number;
	subclassCode?: number;
	protocolCode?: number;
	interfaceClass?: number;
	interfaceSubclass?: number;
	interfaceProtocol?: number;
}

export interface USBDeviceRequestOptions {
	filters?: USBDeviceFilter[];
}

export interface USBControlTransferParameters {
	requestType: USBRequestType;
	recipient: USBRecipient;
	request: number;
	value: number;
	index: number;
}

export interface USBInTransferResult {
	data?: DataView;
	status: USBTransferStatus;
}

export interface USBOutTransferResult {
	bytesWritten: number;
	status: USBTransferStatus;
}

interface USBEndpointDescriptor {
	endpointNumber: number;
	direction: USBDirection;
	type: USBEndpointType;
	packetSize: number;
}

interface USBAlternateInterfaceDescriptor {
	alternateSetting: number;
	interfaceClass: number;
	interfaceSubclass: number;
	interfaceProtocol: number;
	endpoints: USBEndpointDescriptor[];
}

interface USBInterfaceDescriptor {
	interfaceNumber: number;
	claimed?: boolean | number;
	alternates: USBAlternateInterfaceDescriptor[];
}

interface USBConfigurationDescriptor {
	configurationValue: number;
	interfaces: USBInterfaceDescriptor[];
}

interface USBNativeDescriptor extends USBNativeDevice {
	usbVersionMajor: number;
	usbVersionMinor: number;
	usbVersionSubminor: number;
	deviceClass: number;
	deviceSubclass: number;
	deviceProtocol: number;
	vendorId: number;
	productId: number;
	deviceVersionMajor: number;
	deviceVersionMinor: number;
	deviceVersionSubminor: number;
	manufacturerName?: string;
	productName?: string;
	serialNumber?: string;
	busId: number;
	deviceId: number;
	configurations: USBConfigurationDescriptor[];
}

interface USBDeviceState {
	native: USBNativeDescriptor;
	opened: boolean;
	configuration?: USBConfiguration;
}

const INTERNAL = INTERNAL_SYMBOL;
const _device = new WeakMap<USBDevice, USBDeviceState>();
let initialized = false;

function ensureInit() {
	if (initialized) return;
	$.usbInit();
	initialized = true;
	addEventListener('unload', () => {
		$.usbExit();
		initialized = false;
	});
}

function yieldToLoop() {
	return new Promise<void>((resolve) => setTimeout(resolve, 0));
}

function validateByte(name: string, value: number) {
	if (!Number.isInteger(value) || value < 0 || value > 0xff) {
		throw new TypeError(`${name} must be an integer in the range 0-255`);
	}
}

function validateWord(name: string, value: number) {
	if (!Number.isInteger(value) || value < 0 || value > 0xffff) {
		throw new TypeError(`${name} must be an integer in the range 0-65535`);
	}
}

function validateFilter(filter: USBDeviceFilter) {
	for (const key of [
		'vendorId',
		'productId',
		'classCode',
		'subclassCode',
		'protocolCode',
		'interfaceClass',
		'interfaceSubclass',
		'interfaceProtocol',
	] as const) {
		const value = filter[key];
		if (typeof value !== 'undefined') {
			validateWord(key, value);
		}
	}
}

function deviceFromNative(native: USBNativeDescriptor): USBDevice {
	return new USBDevice(INTERNAL, native);
}

/** Stable per-physical-device identity used by the hotplug diff. */
function deviceKey(native: USBNativeDescriptor): string {
	return `${native.busId}:${native.deviceId}`;
}

function hex4(n: number): string {
	return (n >>> 0).toString(16).padStart(4, '0');
}

/** Secondary chooser line: "vvvv:pppp · Manufacturer" (string descriptors are
 * usually empty until `open()`, so this is typically just the ids). */
function usbDetail(native: USBNativeDescriptor): string {
	const ids = `${hex4(native.vendorId)}:${hex4(native.productId)}`;
	return native.manufacturerName ? `${ids} · ${native.manufacturerName}` : ids;
}

/** A USB endpoint descriptor exposed by {@link USBAlternateInterface}. */
export class USBEndpoint {
	readonly endpointNumber: number;
	readonly direction: USBDirection;
	readonly type: USBEndpointType;
	readonly packetSize: number;

	constructor(_internal: symbol, desc: USBEndpointDescriptor) {
		assertInternalConstructor(arguments);
		this.endpointNumber = desc.endpointNumber;
		this.direction = desc.direction;
		this.type = desc.type;
		this.packetSize = desc.packetSize;
	}
}
def(USBEndpoint);

/** A USB alternate interface descriptor. */
export class USBAlternateInterface {
	readonly alternateSetting: number;
	readonly interfaceClass: number;
	readonly interfaceSubclass: number;
	readonly interfaceProtocol: number;
	readonly endpoints: USBEndpoint[];

	constructor(_internal: symbol, desc: USBAlternateInterfaceDescriptor) {
		assertInternalConstructor(arguments);
		this.alternateSetting = desc.alternateSetting;
		this.interfaceClass = desc.interfaceClass;
		this.interfaceSubclass = desc.interfaceSubclass;
		this.interfaceProtocol = desc.interfaceProtocol;
		this.endpoints = desc.endpoints.map((e) => new USBEndpoint(INTERNAL, e));
	}
}
def(USBAlternateInterface);

/** A USB interface descriptor. */
export class USBInterface {
	readonly interfaceNumber: number;
	claimed: boolean;
	readonly alternates: USBAlternateInterface[];
	alternate: USBAlternateInterface;

	constructor(_internal: symbol, desc: USBInterfaceDescriptor) {
		assertInternalConstructor(arguments);
		this.interfaceNumber = desc.interfaceNumber;
		this.claimed = !!desc.claimed;
		this.alternates = desc.alternates.map(
			(a) => new USBAlternateInterface(INTERNAL, a),
		);
		this.alternate = this.alternates[0];
	}
}
def(USBInterface);

/** A USB configuration descriptor. */
export class USBConfiguration {
	readonly configurationValue: number;
	readonly interfaces: USBInterface[];

	constructor(_internal: symbol, desc: USBConfigurationDescriptor) {
		assertInternalConstructor(arguments);
		this.configurationValue = desc.configurationValue;
		this.interfaces = desc.interfaces.map((i) => new USBInterface(INTERNAL, i));
	}
}
def(USBConfiguration);

/**
 * A physical USB device attached to the Switch's USB-C port, with all of its
 * interfaces exposed under {@link USBDevice.configurations}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/USBDevice
 */
export class USBDevice {
	readonly usbVersionMajor: number;
	readonly usbVersionMinor: number;
	readonly usbVersionSubminor: number;
	readonly deviceClass: number;
	readonly deviceSubclass: number;
	readonly deviceProtocol: number;
	readonly vendorId: number;
	readonly productId: number;
	readonly deviceVersionMajor: number;
	readonly deviceVersionMinor: number;
	readonly deviceVersionSubminor: number;
	// Not `readonly`: the engine doesn't populate string descriptors at
	// enumeration, so `open()` fills these in from GET_DESCRIPTOR reads.
	manufacturerName?: string;
	productName?: string;
	serialNumber?: string;
	readonly configurations: USBConfiguration[];

	constructor(_internal: symbol, native: USBNativeDescriptor) {
		assertInternalConstructor(arguments);
		this.usbVersionMajor = native.usbVersionMajor;
		this.usbVersionMinor = native.usbVersionMinor;
		this.usbVersionSubminor = native.usbVersionSubminor;
		this.deviceClass = native.deviceClass;
		this.deviceSubclass = native.deviceSubclass;
		this.deviceProtocol = native.deviceProtocol;
		this.vendorId = native.vendorId;
		this.productId = native.productId;
		this.deviceVersionMajor = native.deviceVersionMajor;
		this.deviceVersionMinor = native.deviceVersionMinor;
		this.deviceVersionSubminor = native.deviceVersionSubminor;
		this.manufacturerName = native.manufacturerName;
		this.productName = native.productName;
		this.serialNumber = native.serialNumber;
		this.configurations = native.configurations.map(
			(c) => new USBConfiguration(INTERNAL, c),
		);
		_device.set(this, {
			native,
			opened: false,
			configuration: this.configurations[0],
		});
	}

	get opened() {
		return _device.get(this)?.opened ?? false;
	}

	get configuration() {
		return _device.get(this)?.configuration;
	}

	async open(): Promise<void> {
		const state = _device.get(this)!;
		if (!state.opened) {
			await yieldToLoop();
			$.usbDeviceOpen(state.native);
			state.opened = true;
			// Best-effort: the engine leaves string descriptors empty at
			// enumeration, so read them now that a control pipe is available.
			await this.#populateStrings();
		}
	}

	async #populateStrings(): Promise<void> {
		if (this.manufacturerName && this.productName && this.serialNumber) return;
		try {
			const dd = await this.controlTransferIn(
				{ requestType: 'standard', recipient: 'device', request: 0x06, value: 0x0100, index: 0 },
				18,
			);
			const dv = dd.data;
			if (!dv || dv.byteLength < 18) return;
			const read = async (index: number): Promise<string | undefined> => {
				if (!index) return undefined;
				const r = await this.controlTransferIn(
					{ requestType: 'standard', recipient: 'device', request: 0x06, value: 0x0300 | index, index: 0x0409 },
					255,
				);
				const v = r.data;
				if (!v || v.byteLength < 2) return undefined;
				const len = v.getUint8(0);
				let out = '';
				for (let i = 2; i + 1 < len && i + 1 < v.byteLength; i += 2) {
					out += String.fromCharCode(v.getUint16(i, true));
				}
				return out || undefined;
			};
			if (!this.manufacturerName) this.manufacturerName = await read(dv.getUint8(14));
			if (!this.productName) this.productName = await read(dv.getUint8(15));
			if (!this.serialNumber) this.serialNumber = await read(dv.getUint8(16));
		} catch {
			// best-effort; leave any unread strings undefined
		}
	}

	async close(): Promise<void> {
		const state = _device.get(this)!;
		if (state.opened) {
			await yieldToLoop();
			$.usbDeviceClose(state.native);
			state.opened = false;
			for (const config of this.configurations) {
				for (const iface of config.interfaces) iface.claimed = false;
			}
		}
	}

	/** No persistent permission store on Switch; `forget()` closes the device. */
	async forget(): Promise<void> {
		await this.close();
	}

	async selectConfiguration(configurationValue: number): Promise<void> {
		const state = _device.get(this)!;
		const config = this.configurations.find(
			(c) => c.configurationValue === configurationValue,
		);
		if (!config) {
			throw new DOMException('The selected configuration does not exist.', 'NotFoundError');
		}
		state.configuration = config;
	}

	async claimInterface(interfaceNumber: number): Promise<void> {
		const state = _device.get(this)!;
		if (!state.opened) {
			throw new DOMException('The device must be opened first.', 'InvalidStateError');
		}
		// usb:hs releases interfaces asynchronously, so a re-acquire right after a
		// close (rapid disconnect → reconnect) can transiently fail with the
		// interface still "in use" — and a data interface with a pending bulk-IN
		// URB can take a while to release. Retry with frame-yielding backoff
		// (keeps RAF/input alive) for up to ~1.5s before surfacing the error.
		// Only the transient acquire race is retried; other failures throw at once.
		let lastErr: unknown;
		for (let attempt = 0; attempt < 20; attempt++) {
			await yieldToLoop();
			try {
				$.usbClaimInterface(state.native, interfaceNumber);
				const iface = state.configuration?.interfaces.find(
					(i) => i.interfaceNumber === interfaceNumber,
				);
				if (iface) iface.claimed = true;
				return;
			} catch (err) {
				const msg = err instanceof Error ? err.message : String(err);
				if (!msg.includes('usbHsAcquireUsbIf')) throw err;
				lastErr = err;
				await new Promise<void>((resolve) => setTimeout(resolve, 75));
			}
		}
		throw lastErr;
	}

	async releaseInterface(interfaceNumber: number): Promise<void> {
		const state = _device.get(this)!;
		await yieldToLoop();
		$.usbReleaseInterface(state.native, interfaceNumber);
		const iface = state.configuration?.interfaces.find(
			(i) => i.interfaceNumber === interfaceNumber,
		);
		if (iface) iface.claimed = false;
	}

	async selectAlternateInterface(
		interfaceNumber: number,
		alternateSetting: number,
	): Promise<void> {
		const state = _device.get(this)!;
		const iface = state.configuration?.interfaces.find(
			(i) => i.interfaceNumber === interfaceNumber,
		);
		const alternate = iface?.alternates.find(
			(a) => a.alternateSetting === alternateSetting,
		);
		if (!iface || !alternate) {
			throw new DOMException('The selected alternate interface does not exist.', 'NotFoundError');
		}
		await yieldToLoop();
		$.usbSelectAlternateInterface(state.native, interfaceNumber, alternateSetting);
		iface.alternate = alternate;
	}

	async transferIn(
		endpointNumber: number,
		length: number,
	): Promise<USBInTransferResult> {
		validateByte('endpointNumber', endpointNumber);
		validateWord('length', length);
		const state = _device.get(this)!;
		// Non-blocking: post the URB once, then poll it a frame at a time so the
		// JS event loop (RAF, input, timers) keeps running while data is pending.
		// `usbReadStart` is idempotent, so re-entry after an earlier yield is safe.
		$.usbReadStart(state.native, endpointNumber, length);
		for (;;) {
			await yieldToLoop();
			const buffer = $.usbReadPoll(state.native, endpointNumber);
			if (buffer !== undefined) {
				return { data: new DataView(buffer), status: 'ok' };
			}
		}
	}

	async transferOut(
		endpointNumber: number,
		data: BufferSource,
	): Promise<USBOutTransferResult> {
		validateByte('endpointNumber', endpointNumber);
		const state = _device.get(this)!;
		// Non-blocking: post the OUT URB once, then poll it a frame at a time so the
		// JS event loop (RAF, input, timers) keeps running while the transfer is in
		// flight. The synchronous alternative ($.usbTransferOut → usbHsEpPostBuffer)
		// waits U64_MAX, so a device that stalls its OUT endpoint would freeze the
		// whole runtime — mirror transferIn's async post/poll instead.
		$.usbWriteStart(state.native, endpointNumber, data);
		for (;;) {
			await yieldToLoop();
			const bytesWritten = $.usbWritePoll(state.native, endpointNumber);
			if (bytesWritten !== undefined) {
				return { bytesWritten, status: 'ok' };
			}
		}
	}

	async controlTransferIn(
		setup: USBControlTransferParameters,
		length: number,
	): Promise<USBInTransferResult> {
		if (!['standard', 'class', 'vendor', 'reserved'].includes(setup.requestType)) {
			throw new TypeError('Invalid USB requestType');
		}
		if (!['device', 'interface', 'endpoint', 'other'].includes(setup.recipient)) {
			throw new TypeError('Invalid USB recipient');
		}
		validateByte('request', setup.request);
		validateWord('value', setup.value);
		validateWord('index', setup.index);
		validateWord('length', length);
		const state = _device.get(this)!;
		await yieldToLoop();
		const buffer = $.usbControlTransferIn(state.native, setup, length);
		return { data: new DataView(buffer), status: 'ok' };
	}

	async controlTransferOut(
		setup: USBControlTransferParameters,
		data?: BufferSource,
	): Promise<USBOutTransferResult> {
		if (!['standard', 'class', 'vendor', 'reserved'].includes(setup.requestType)) {
			throw new TypeError('Invalid USB requestType');
		}
		if (!['device', 'interface', 'endpoint', 'other'].includes(setup.recipient)) {
			throw new TypeError('Invalid USB recipient');
		}
		validateByte('request', setup.request);
		validateWord('value', setup.value);
		validateWord('index', setup.index);
		const state = _device.get(this)!;
		await yieldToLoop();
		return {
			bytesWritten: $.usbControlTransferOut(state.native, setup, data),
			status: 'ok',
		};
	}

	/** Clear a halted (stalled) bulk/interrupt endpoint and reset its data toggle. */
	async clearHalt(direction: USBDirection, endpointNumber: number): Promise<void> {
		if (direction !== 'in' && direction !== 'out') {
			throw new TypeError("direction must be 'in' or 'out'");
		}
		validateByte('endpointNumber', endpointNumber);
		const state = _device.get(this)!;
		await yieldToLoop();
		$.usbClearHalt(state.native, direction === 'in', endpointNumber);
	}

	async isochronousTransferIn(
		_endpointNumber: number,
		_packetLengths: number[],
	): Promise<never> {
		throw new DOMException(
			'Isochronous transfers are not supported by the nx.js usb:hs backend.',
			'NotSupportedError',
		);
	}

	async isochronousTransferOut(
		_endpointNumber: number,
		_data: BufferSource,
		_packetLengths: number[],
	): Promise<never> {
		throw new DOMException(
			'Isochronous transfers are not supported by the nx.js usb:hs backend.',
			'NotSupportedError',
		);
	}

	async reset(): Promise<void> {
		const state = _device.get(this)!;
		await yieldToLoop();
		$.usbResetDevice(state.native);
	}
}
def(USBDevice);

/**
 * Fired at {@link USB} when a device is attached (`connect`) or detached
 * (`disconnect`).
 *
 * @see https://developer.mozilla.org/docs/Web/API/USBConnectionEvent
 */
export class USBConnectionEvent extends Event {
	readonly device: USBDevice;

	constructor(type: string, eventInitDict: { device: USBDevice }) {
		super(type);
		this.device = eventInitDict.device;
	}
}
def(USBConnectionEvent);

const HOTPLUG_POLL_MS = 500;

/** Entry point for WebUSB functionality, available as `navigator.usb`. */
export class USB extends EventTarget {
	#onconnect: ((this: USB, ev: USBConnectionEvent) => any) | null = null;
	#ondisconnect: ((this: USB, ev: USBConnectionEvent) => any) | null = null;
	#hotplugListeners = 0;
	#pollTimer: number | null = null;
	#known = new Map<string, USBDevice>();

	constructor(_internal: symbol) {
		super();
		assertInternalConstructor(arguments);
	}

	async getDevices(): Promise<USBDevice[]> {
		ensureInit();
		return $.usbGetDevices().map((native) =>
			deviceFromNative(native as USBNativeDescriptor),
		);
	}

	async requestDevice(options: USBDeviceRequestOptions = {}): Promise<USBDevice> {
		ensureInit();
		const filters = options.filters ?? [{}];
		if (!Array.isArray(filters)) {
			throw new TypeError('USB device filters must be an array');
		}
		// Enumerate every device matching ANY filter, deduped by physical
		// device (bus:device) so a composite device isn't offered twice.
		const seen = new Set<string>();
		const matches: USBNativeDescriptor[] = [];
		for (const filter of filters) {
			validateFilter(filter);
			for (const native of $.usbGetDevices(filter) as USBNativeDescriptor[]) {
				const key = deviceKey(native);
				if (seen.has(key)) continue;
				seen.add(key);
				matches.push(native);
			}
		}
		if (matches.length === 0) {
			throw new DOMException('No USB devices matching the filters were found.', 'NotFoundError');
		}
		const chooser = getDeviceChooser();
		if (!chooser) {
			// No host picker registered (bare nx.js): preserve the historical
			// first-match behaviour so standalone apps don't regress.
			return deviceFromNative(matches[0]);
		}
		const chosenId = await chooser({
			kind: 'usb',
			mode: 'select',
			title: 'Select a USB device',
			candidates: matches.map((native) => ({
				id: deviceKey(native),
				name:
					native.productName ||
					`USB device ${hex4(native.vendorId)}:${hex4(native.productId)}`,
				detail: usbDetail(native),
			})),
		});
		if (chosenId == null) {
			throw new DOMException('No device was selected.', 'NotFoundError');
		}
		const chosen = matches.find((native) => deviceKey(native) === chosenId);
		if (!chosen) {
			throw new DOMException('The selected device is no longer available.', 'NotFoundError');
		}
		return deviceFromNative(chosen);
	}

	// ---- hotplug (connect / disconnect) ----------------------------------

	get onconnect() {
		return this.#onconnect;
	}
	set onconnect(cb: ((this: USB, ev: USBConnectionEvent) => any) | null) {
		if (this.#onconnect) {
			super.removeEventListener('connect', this.#onconnect as any);
			this.#hotplugListeners--;
		}
		this.#onconnect = cb;
		if (cb) {
			super.addEventListener('connect', cb as any);
			this.#hotplugListeners++;
			this.#startHotplug();
		} else {
			this.#maybeStopHotplug();
		}
	}

	get ondisconnect() {
		return this.#ondisconnect;
	}
	set ondisconnect(cb: ((this: USB, ev: USBConnectionEvent) => any) | null) {
		if (this.#ondisconnect) {
			super.removeEventListener('disconnect', this.#ondisconnect as any);
			this.#hotplugListeners--;
		}
		this.#ondisconnect = cb;
		if (cb) {
			super.addEventListener('disconnect', cb as any);
			this.#hotplugListeners++;
			this.#startHotplug();
		} else {
			this.#maybeStopHotplug();
		}
	}

	addEventListener(type: string, callback: any, options?: any): void {
		super.addEventListener(type, callback, options);
		if (type === 'connect' || type === 'disconnect') {
			this.#hotplugListeners++;
			this.#startHotplug();
		}
	}

	removeEventListener(type: string, callback: any, options?: any): void {
		super.removeEventListener(type, callback, options);
		if ((type === 'connect' || type === 'disconnect') && this.#hotplugListeners > 0) {
			this.#hotplugListeners--;
			this.#maybeStopHotplug();
		}
	}

	#startHotplug() {
		if (this.#pollTimer !== null) return;
		ensureInit();
		// Seed the known-device set silently so pre-existing devices don't fire
		// a spurious `connect` when listening starts.
		this.#known = new Map();
		for (const native of $.usbGetDevices() as USBNativeDescriptor[]) {
			this.#known.set(deviceKey(native), deviceFromNative(native));
		}
		this.#pollTimer = setInterval(() => this.#poll(), HOTPLUG_POLL_MS);
	}

	#maybeStopHotplug() {
		if (this.#hotplugListeners > 0 || this.#onconnect || this.#ondisconnect) return;
		if (this.#pollTimer !== null) {
			clearInterval(this.#pollTimer);
			this.#pollTimer = null;
		}
	}

	#poll() {
		// The native check gates re-enumeration on an actual hotplug event.
		if (!$.usbHotplugCheck()) return;
		const seen = new Map<string, USBDevice>();
		for (const native of $.usbGetDevices() as USBNativeDescriptor[]) {
			const key = deviceKey(native);
			let dev = this.#known.get(key);
			if (!dev) {
				dev = deviceFromNative(native);
				this.dispatchEvent(new USBConnectionEvent('connect', { device: dev }));
			}
			seen.set(key, dev);
		}
		for (const [key, dev] of this.#known) {
			if (!seen.has(key)) {
				this.dispatchEvent(new USBConnectionEvent('disconnect', { device: dev }));
			}
		}
		this.#known = seen;
	}
}
def(USB);

/** The shared USB instance, also exposed as `navigator.usb`. */
export const usb = new USB(INTERNAL);
