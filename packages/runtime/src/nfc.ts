import { $ } from './$';
import { DOMException } from './dom-exception';
import { getDeviceChooser } from './navigator/device-chooser';
import { Event } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import { setInterval, clearInterval, setTimeout } from './timers';
import { def } from './utils';

// Web NFC over libnx `nfc:user` (see nfc.cc).
//
// Scope: NDEF read/write on Type-2 (NTAG21x) tags, which is what Web NFC
// targets in practice. The reader lives in the right Joy-Con / Pro Controller,
// so a controller with NFC must be attached and the user taps a tag. The
// native layer does raw ISO14443-3A passthrough (READ 0x30 / WRITE 0xA2); the
// NDEF TLV + record (de)serialisation lives here.
//
// Testability: `scan()` resolves once permission is granted even when no NFC
// reader is present (e.g. under the Citron emulator) — it simply never sees a
// real tag. `globalThis.__nxjsSimulateNdefReading(serial, records)` injects a
// synthetic `reading` event into every active reader so the event + record
// parsing can be exercised without hardware.

const NfcDeviceState_TagFound = 2;
const NfcDeviceState_TagMounted = 4;

// NFC Forum URI record prefix table (index → prefix), RTD-URI 1.0.
const URI_PREFIXES = [
	'', 'http://www.', 'https://www.', 'http://', 'https://', 'tel:', 'mailto:',
	'ftp://anonymous:anonymous@', 'ftp://ftp.', 'ftps://', 'sftp://', 'smb://',
	'nfs://', 'ftp://', 'dav://', 'news:', 'telnet://', 'imap:', 'rtsp://',
	'urn:', 'pop:', 'sip:', 'sips:', 'tftp:', 'btspp://', 'btl2cap://',
	'btgoep://', 'tcpobex://', 'irdaobex://', 'file://', 'urn:epc:id:',
	'urn:epc:tag:', 'urn:epc:pat:', 'urn:epc:raw:', 'urn:epc:', 'urn:nfc:',
];

const utf8Decode = (b: Uint8Array): string => new TextDecoder().decode(b);
const utf8Encode = (s: string): Uint8Array => new TextEncoder().encode(s);

function toDataView(data: unknown): DataView {
	if (data instanceof DataView) return data;
	if (data instanceof ArrayBuffer) return new DataView(data);
	if (ArrayBuffer.isView(data)) {
		const v = data as ArrayBufferView;
		return new DataView(v.buffer, v.byteOffset, v.byteLength);
	}
	if (typeof data === 'string') {
		const u = utf8Encode(data);
		return new DataView(u.buffer, u.byteOffset, u.byteLength);
	}
	return new DataView(new ArrayBuffer(0));
}

function dvBytes(dv: DataView): Uint8Array {
	return new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
}

// --- NDEFRecord ------------------------------------------------------------

export interface NDEFRecordInit {
	recordType: string;
	mediaType?: string;
	id?: string;
	encoding?: string;
	lang?: string;
	data?: string | BufferSource;
}

/**
 * @see https://developer.mozilla.org/docs/Web/API/NDEFRecord
 */
export class NDEFRecord {
	readonly recordType: string;
	readonly mediaType?: string;
	readonly id?: string;
	readonly encoding?: string;
	readonly lang?: string;
	readonly data?: DataView;

	constructor(init: NDEFRecordInit) {
		if (!init || typeof init.recordType !== 'string') {
			throw new TypeError("NDEFRecord requires a 'recordType'.");
		}
		this.recordType = init.recordType;
		this.mediaType = init.mediaType;
		this.id = init.id;
		this.encoding = init.encoding;
		this.lang = init.lang;
		this.data = typeof init.data === 'undefined' ? undefined : toDataView(init.data);
	}
}
def(NDEFRecord);

// Construct a record from parsed on-tag fields.
function recordFromTag(tnf: number, typeBytes: Uint8Array, idBytes: Uint8Array | null, payload: Uint8Array): NDEFRecord {
	const typeStr = utf8Decode(typeBytes);
	const id = idBytes && idBytes.length ? utf8Decode(idBytes) : undefined;
	if (tnf === 0x01 && typeStr === 'T') {
		// Text: [status][lang][text]; status bit7 = UTF-16, bits0-5 = lang len.
		const status = payload[0] ?? 0;
		const langLen = status & 0x3f;
		const encoding = status & 0x80 ? 'utf-16' : 'utf-8';
		const lang = utf8Decode(payload.slice(1, 1 + langLen));
		const text = payload.slice(1 + langLen);
		return new NDEFRecord({ recordType: 'text', encoding, lang, id, data: text });
	}
	if (tnf === 0x01 && typeStr === 'U') {
		const prefix = URI_PREFIXES[payload[0] ?? 0] ?? '';
		const url = prefix + utf8Decode(payload.slice(1));
		return new NDEFRecord({ recordType: 'url', id, data: url });
	}
	if (tnf === 0x02) return new NDEFRecord({ recordType: 'mime', mediaType: typeStr, id, data: payload });
	if (tnf === 0x03) return new NDEFRecord({ recordType: 'absolute-url', id, data: payload });
	if (tnf === 0x04) return new NDEFRecord({ recordType: typeStr, id, data: payload }); // external
	if (tnf === 0x00) return new NDEFRecord({ recordType: 'empty' });
	if (tnf === 0x01) return new NDEFRecord({ recordType: typeStr, id, data: payload }); // other well-known
	return new NDEFRecord({ recordType: 'unknown', id, data: payload });
}

// --- NDEFMessage -----------------------------------------------------------

export interface NDEFMessageInit {
	records: NDEFRecordInit[];
}

/**
 * @see https://developer.mozilla.org/docs/Web/API/NDEFMessage
 */
export class NDEFMessage {
	readonly records: NDEFRecord[];

	constructor(init: NDEFMessageInit | { records: NDEFRecord[] }) {
		const recs = init?.records ?? [];
		this.records = recs.map((r) => (r instanceof NDEFRecord ? r : new NDEFRecord(r)));
	}
}
def(NDEFMessage);

// --- NDEF byte codec -------------------------------------------------------

/** Parse an NDEF message byte array into records. */
function parseNdefMessage(buf: Uint8Array): NDEFRecord[] {
	const records: NDEFRecord[] = [];
	let i = 0;
	while (i < buf.length) {
		const header = buf[i++];
		const tnf = header & 0x07;
		const il = (header & 0x08) !== 0;
		const sr = (header & 0x10) !== 0;
		const me = (header & 0x40) !== 0;
		const typeLen = buf[i++];
		let payloadLen: number;
		if (sr) {
			payloadLen = buf[i++];
		} else {
			payloadLen = ((buf[i] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | buf[i + 3]) >>> 0;
			i += 4;
		}
		const idLen = il ? buf[i++] : 0;
		const type = buf.slice(i, i + typeLen);
		i += typeLen;
		const id = idLen ? buf.slice(i, i + idLen) : null;
		i += idLen;
		const payload = buf.slice(i, i + payloadLen);
		i += payloadLen;
		records.push(recordFromTag(tnf, type, id, payload));
		if (me) break;
	}
	return records;
}

function encodeRecord(rec: NDEFRecord, first: boolean, last: boolean): number[] {
	let tnf = 0x05; // unknown
	let typeBytes: number[] = [];
	let payload: number[] = [];
	const rt = rec.recordType;
	const bodyBytes = (): number[] => (rec.data ? Array.from(dvBytes(rec.data)) : []);
	const bodyText = (): string => (rec.data ? utf8Decode(dvBytes(rec.data)) : '');

	if (rt === 'text') {
		tnf = 0x01;
		typeBytes = [0x54]; // 'T'
		const lang = rec.lang || 'en';
		const langBytes = Array.from(utf8Encode(lang));
		payload = [langBytes.length & 0x3f, ...langBytes, ...Array.from(utf8Encode(bodyText()))];
	} else if (rt === 'url') {
		tnf = 0x01;
		typeBytes = [0x55]; // 'U'
		const url = bodyText();
		let idx = 0;
		for (let k = 1; k < URI_PREFIXES.length; k++) {
			const p = URI_PREFIXES[k];
			if (p && url.startsWith(p) && p.length > (URI_PREFIXES[idx]?.length ?? 0)) idx = k;
		}
		const rest = idx > 0 ? url.slice(URI_PREFIXES[idx].length) : url;
		payload = [idx, ...Array.from(utf8Encode(rest))];
	} else if (rt === 'mime') {
		tnf = 0x02;
		typeBytes = Array.from(utf8Encode(rec.mediaType || 'application/octet-stream'));
		payload = bodyBytes();
	} else if (rt === 'absolute-url') {
		tnf = 0x03;
		typeBytes = Array.from(utf8Encode(bodyText()));
	} else if (rt === 'empty') {
		tnf = 0x00;
	} else {
		// External type (e.g. "example.com:foo") or opaque.
		tnf = 0x04;
		typeBytes = Array.from(utf8Encode(rt));
		payload = bodyBytes();
	}

	const sr = payload.length < 256;
	const header = tnf | (first ? 0x80 : 0) | (last ? 0x40 : 0) | (sr ? 0x10 : 0);
	const out = [header, typeBytes.length];
	if (sr) out.push(payload.length);
	else out.push((payload.length >>> 24) & 0xff, (payload.length >>> 16) & 0xff, (payload.length >>> 8) & 0xff, payload.length & 0xff);
	out.push(...typeBytes, ...payload);
	return out;
}

function encodeNdefMessage(records: NDEFRecord[]): Uint8Array {
	const out: number[] = [];
	if (records.length === 0) {
		// A single empty record.
		out.push(...encodeRecord(new NDEFRecord({ recordType: 'empty' }), true, true));
	} else {
		records.forEach((r, i) => out.push(...encodeRecord(r, i === 0, i === records.length - 1)));
	}
	return new Uint8Array(out);
}

// --- tag read / write (Type-2 passthrough) ---------------------------------

function serialFromUid(uid: ArrayBuffer): string {
	const b = new Uint8Array(uid);
	return Array.from(b).map((x) => x.toString(16).padStart(2, '0')).join(':');
}

/** Incremental page reader — Type-2 READ [0x30, page] returns 16 bytes. */
function makeTagReader() {
	let buf = new Uint8Array(0);
	let nextPage = 4;
	const ensure = (need: number): boolean => {
		while (buf.length < need && nextPage <= 230) {
			let block: Uint8Array;
			try {
				block = new Uint8Array($.nfcTransceive(new Uint8Array([0x30, nextPage])));
			} catch {
				break;
			}
			if (block.length === 0) break;
			const merged = new Uint8Array(buf.length + block.length);
			merged.set(buf);
			merged.set(block, buf.length);
			buf = merged;
			nextPage += 4;
		}
		return buf.length >= need;
	};
	return {
		get: (i: number): number => (ensure(i + 1) ? buf[i] : 0),
		slice: (a: number, b: number): Uint8Array => (ensure(b) ? buf.slice(a, b) : buf.slice(a)),
	};
}

/** Read the tag's first NDEF-message TLV and parse it into records. */
function readTagRecords(): NDEFRecord[] {
	$.nfcKeepSession();
	try {
		const r = makeTagReader();
		let i = 0;
		for (let guard = 0; guard < 64; guard++) {
			const t = r.get(i);
			if (t === 0x00) { i += 1; continue; } // NULL TLV
			if (t === 0xfe || t === 0x00) break; // terminator
			if (t === 0x03) {
				let len = r.get(i + 1);
				let vstart = i + 2;
				if (len === 0xff) {
					len = (r.get(i + 2) << 8) | r.get(i + 3);
					vstart = i + 4;
				}
				if (len === 0) return [];
				return parseNdefMessage(r.slice(vstart, vstart + len));
			}
			// Other TLV (lock/memory control): [type][len][value]
			const len = r.get(i + 1);
			i += 2 + len;
		}
		return [];
	} finally {
		$.nfcReleaseSession();
	}
}

function writeTagRecords(records: NDEFRecord[]): void {
	const message = encodeNdefMessage(records);
	const tlv: number[] = [];
	if (message.length < 0xff) tlv.push(0x03, message.length);
	else tlv.push(0x03, 0xff, (message.length >> 8) & 0xff, message.length & 0xff);
	tlv.push(...Array.from(message), 0xfe); // NDEF TLV + terminator
	while (tlv.length % 4 !== 0) tlv.push(0x00);
	$.nfcKeepSession();
	try {
		for (let p = 0; p * 4 < tlv.length; p++) {
			const page = 4 + p;
			// Type-2 WRITE: [0xA2, page, d0, d1, d2, d3]
			$.nfcTransceive(new Uint8Array([0xa2, page, tlv[p * 4] ?? 0, tlv[p * 4 + 1] ?? 0, tlv[p * 4 + 2] ?? 0, tlv[p * 4 + 3] ?? 0]));
		}
	} finally {
		$.nfcReleaseSession();
	}
}

// --- NDEFReadingEvent ------------------------------------------------------

/**
 * @see https://developer.mozilla.org/docs/Web/API/NDEFReadingEvent
 */
export class NDEFReadingEvent extends Event {
	readonly serialNumber: string;
	readonly message: NDEFMessage;

	constructor(type: string, init: { serialNumber?: string; message: NDEFMessage }) {
		super(type);
		this.serialNumber = init.serialNumber ?? '';
		this.message = init.message;
	}
}
def(NDEFReadingEvent);

// --- shared detection poll -------------------------------------------------

let usbHardwareReady: boolean | null = null;
const activeReaders = new Set<NDEFReader>();
let pollTimer: ReturnType<typeof setInterval> | null = null;
let lastSerial: string | null = null;

function dispatchReading(serialNumber: string, records: NDEFRecord[]): void {
	const message = new NDEFMessage({ records });
	for (const reader of [...activeReaders]) {
		reader.dispatchEvent(new NDEFReadingEvent('reading', { serialNumber, message }));
	}
}

function pollTick(): void {
	let state = -1;
	try {
		state = $.nfcGetState();
	} catch {
		return;
	}
	if (state === NfcDeviceState_TagFound || state === NfcDeviceState_TagMounted) {
		const info = $.nfcGetTagInfo();
		if (!info) return;
		const serial = serialFromUid(info.uid);
		if (serial === lastSerial) return; // already delivered this tag
		lastSerial = serial;
		try {
			dispatchReading(serial, readTagRecords());
		} catch {
			for (const reader of [...activeReaders]) {
				reader.dispatchEvent(new Event('readingerror'));
			}
		}
	} else {
		// Tag removed — allow the next tap to be delivered.
		lastSerial = null;
	}
}

function startPollingIfPossible(): void {
	if (pollTimer !== null) return;
	if (usbHardwareReady === null) usbHardwareReady = safeNfcInit();
	if (!usbHardwareReady) return; // no reader (e.g. Citron) — sim-only
	try {
		$.nfcStartDetection();
	} catch {
		return;
	}
	lastSerial = null;
	pollTimer = setInterval(pollTick, 250);
}

function stopPollingIfIdle(): void {
	if (activeReaders.size > 0) return;
	if (pollTimer !== null) {
		clearInterval(pollTimer);
		pollTimer = null;
	}
	try {
		$.nfcStopDetection();
	} catch {}
	lastSerial = null;
}

function safeNfcInit(): boolean {
	try {
		return $.nfcInit();
	} catch {
		return false;
	}
}

// --- NDEFReader ------------------------------------------------------------

export interface NDEFScanOptions {
	signal?: AbortSignal;
}
export interface NDEFWriteOptions {
	overwrite?: boolean;
	signal?: AbortSignal;
}

/**
 * Entry point for the Web NFC API.
 *
 * @see https://developer.mozilla.org/docs/Web/API/NDEFReader
 */
export class NDEFReader extends EventTarget {
	onreading: ((this: NDEFReader, ev: NDEFReadingEvent) => any) | null = null;
	onreadingerror: ((this: NDEFReader, ev: Event) => any) | null = null;

	#scanning = false;

	async #grant(kind: string): Promise<void> {
		const chooser = getDeviceChooser();
		if (!chooser) return; // bare nx.js — auto-grant
		const granted = await chooser({
			kind: 'nfc',
			mode: 'confirm',
			title: kind === 'write' ? 'Allow writing to NFC tags?' : 'Allow reading NFC tags?',
			candidates: [{ id: '__grant__', name: 'Allow NFC access' }],
		});
		if (granted == null) {
			throw new DOMException('NFC permission was denied.', 'NotAllowedError');
		}
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/NDEFReader/scan
	 */
	async scan(options: NDEFScanOptions = {}): Promise<void> {
		if (options.signal?.aborted) {
			throw new DOMException('The scan was aborted.', 'AbortError');
		}
		await this.#grant('read');
		if (!this.#scanning) {
			this.#scanning = true;
			activeReaders.add(this);
			startPollingIfPossible();
		}
		options.signal?.addEventListener('abort', () => {
			this.#scanning = false;
			activeReaders.delete(this);
			stopPollingIfIdle();
		});
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/NDEFReader/write
	 */
	async write(message: string | BufferSource | NDEFMessageInit, options: NDEFWriteOptions = {}): Promise<void> {
		if (options.signal?.aborted) {
			throw new DOMException('The write was aborted.', 'AbortError');
		}
		await this.#grant('write');
		if (usbHardwareReady === null) usbHardwareReady = safeNfcInit();
		if (!usbHardwareReady || !$.nfcIsAvailable()) {
			throw new DOMException('No NFC reader is available.', 'NotSupportedError');
		}
		const records = normalizeWriteMessage(message);
		// Ensure detection is running so a tag can be present, then wait briefly
		// for a tag before writing.
		const startedHere = pollTimer === null;
		try {
			$.nfcStartDetection();
		} catch {}
		const deadline = Date.now() + 10000;
		let wrote = false;
		while (Date.now() < deadline) {
			let state = -1;
			try {
				state = $.nfcGetState();
			} catch {}
			if (state === NfcDeviceState_TagFound || state === NfcDeviceState_TagMounted) {
				writeTagRecords(records);
				wrote = true;
				break;
			}
			await new Promise((r) => setTimeout(r, 200));
		}
		if (startedHere && pollTimer === null) {
			try {
				$.nfcStopDetection();
			} catch {}
		}
		if (!wrote) {
			throw new DOMException('No NFC tag was presented before the timeout.', 'TimeoutError');
		}
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'reading') this.onreading?.(event as NDEFReadingEvent);
		else if (event.type === 'readingerror') this.onreadingerror?.(event);
		return super.dispatchEvent(event);
	}
}
def(NDEFReader);

function normalizeWriteMessage(message: string | BufferSource | NDEFMessageInit): NDEFRecord[] {
	if (typeof message === 'string') {
		return [new NDEFRecord({ recordType: 'text', data: message })];
	}
	if (message instanceof ArrayBuffer || ArrayBuffer.isView(message)) {
		return [new NDEFRecord({ recordType: 'mime', mediaType: 'application/octet-stream', data: message as BufferSource })];
	}
	const init = message as NDEFMessageInit;
	return (init.records ?? []).map((r) => new NDEFRecord(r));
}

// Citron / no-hardware testing aid: inject a synthetic `reading` into every
// active reader. `records` is an array of NDEFRecordInit.
Object.defineProperty(globalThis, '__nxjsSimulateNdefReading', {
	value: (serialNumber: string, records: NDEFRecordInit[] = []): void => {
		dispatchReading(serialNumber || '04:sim:00', records.map((r) => new NDEFRecord(r)));
	},
	writable: false,
	enumerable: false,
	configurable: true,
});
