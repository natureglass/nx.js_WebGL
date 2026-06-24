import type { PromiseState } from '@nx.js/inspect';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';
import type { WebGLRenderingContext } from './canvas/webgl-rendering-context';
import type { ImageBitmap } from './canvas/image-bitmap';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { OffscreenCanvasRenderingContext2D } from './canvas/offscreen-canvas-rendering-context-2d';
import type { Crypto, CryptoKey } from './crypto';
import type { DOMMatrix, DOMMatrixInit, DOMMatrixReadOnly } from './dommatrix';
import type { DOMPoint, DOMPointInit } from './dompoint';
import type { FontFace } from './font/font-face';
import type { Image } from './image';
import type {
	Callback,
	Keys,
	Opaque,
	RGBA,
	VibrationValues,
	WasmGlobalOpaque,
	WasmInstanceOpaque,
	WasmModuleOpaque,
} from './internal';
import type { BatteryManager } from './navigator/battery';
import type { Gamepad, GamepadButton } from './navigator/gamepad';
import type { VirtualKeyboard } from './navigator/virtual-keyboard';
import type { Touch } from './polyfills/event';
import type { URL, URLSearchParams } from './polyfills/url';
import type { Screen } from './screen';
import type {
	Album,
	AlbumFile,
	Application,
	FileSystem,
	IRSensor,
	MemoryUsage,
	NetworkInfo,
	Profile,
	ProfileUid,
	DirEntry,
	ReadFileOptions,
	SaveData,
	SaveDataCreationInfo,
	Service,
	Stats,
	Versions,
} from './switch';
import type { Server, TlsContextOpaque } from './tcp';
import type { Algorithm, BufferSource } from './types';
import type { DatagramSocket } from './udp';
import type { Memory, MemoryDescriptor, Table, TableDescriptor } from './wasm';
import type { Window } from './window';

type ClassOf<T> = {
	new (...args: any[]): T;
};

type FileHandle = Opaque<'FileHandle'>;
type CanvasGradientOpaque = Opaque<'CanvasGradientOpaque'>;
type CompressHandle = Opaque<'CompressHandle'>;
type DecompressHandle = Opaque<'DecompressHandle'>;
type SaveDataIterator = Opaque<'SaveDataIterator'>;
type URLSearchParamsIterator = Opaque<'URLSearchParamsIterator'>;

export interface Init {
	// account.c
	accountInitialize(): () => void;
	accountProfileInit(c: ClassOf<Profile>): void;
	accountCurrentProfile(): Profile | null;
	accountSelectProfile(): Profile | null;
	accountProfileNew(uid: ProfileUid): Profile;
	accountProfiles(): Profile[];

	// album.c
	capsaInitialize(): () => void;
	albumInit(c: ClassOf<Album>): void;
	albumFileInit(c: ClassOf<AlbumFile>): void;
	albumFileList(album: Album): AlbumFile[];

	// applet.c
	appletIlluminance(): number;
	appletGetAppletType(): number;
	appletGetOperationMode(): number;
	appletSetMediaPlaybackState(state: boolean): void;

	// battery.c
	batteryInit(): void;
	batteryInitClass(c: ClassOf<BatteryManager>): void;
	batteryExit(): void;

	// canvas.c
	canvasNew(width: number, height: number): Screen | OffscreenCanvas;
	canvasToBuffer(
		canvas: Screen | OffscreenCanvas,
		type?: string,
		quality?: number,
	): Promise<ArrayBuffer>;
	canvasInitClass(c: ClassOf<Screen | OffscreenCanvas>): void;
	canvasContext2dNew(c: Screen): CanvasRenderingContext2D;
	canvasContext2dNew(c: OffscreenCanvas): OffscreenCanvasRenderingContext2D;
	canvasContext2dInitClass(
		c: ClassOf<CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D>,
	): void;
	webglContextNew(c: Screen): WebGLRenderingContext;
	webglContextInitClass(c: ClassOf<WebGLRenderingContext>): void;
	canvasContext2dGetImageData(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		sx: number,
		sy: number,
		sw: number,
		sh: number,
	): ArrayBuffer;
	canvasContext2dGetTransform(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): number[];
	canvasContext2dGetFont(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): string;
	canvasContext2dSetFont(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		font: FontFace,
		size: number,
		fontString: string,
	): number[];
	canvasContext2dGetFillStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): RGBA;
	canvasContext2dSetFillStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];
	canvasContext2dGetStrokeStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): RGBA;
	canvasContext2dSetStrokeStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];
	canvasContext2dSetFillStyleGradient(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		gradient: CanvasGradientOpaque,
	): void;
	canvasContext2dSetStrokeStyleGradient(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		gradient: CanvasGradientOpaque,
	): void;
	canvasContext2dGetShadowColor(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): RGBA;
	canvasContext2dSetShadowColor(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): void;
	canvasGradientNewLinear(
		x0: number,
		y0: number,
		x1: number,
		y1: number,
	): CanvasGradientOpaque;
	canvasGradientNewRadial(
		x0: number,
		y0: number,
		r0: number,
		x1: number,
		y1: number,
		r1: number,
	): CanvasGradientOpaque;
	canvasGradientInitClass(c: any): void;
	canvasGradientAddColorStop(
		gradient: any,
		offset: number,
		r: number,
		g: number,
		b: number,
		a: number,
	): void;

	// compression.c
	compressNew(format: string): CompressHandle;
	compressWrite(
		handle: CompressHandle,
		buf: BufferSource,
	): Promise<ArrayBuffer>;
	compressFlush(handle: CompressHandle): Promise<ArrayBuffer | null>;
	decompressNew(format: string): DecompressHandle;
	decompressWrite(
		handle: DecompressHandle,
		buf: BufferSource,
	): Promise<ArrayBuffer>;
	decompressFlush(handle: DecompressHandle): Promise<ArrayBuffer | null>;

	// crypto.c
	cryptoKeyNew(
		algorithm: Algorithm,
		key: ArrayBuffer,
		extractable: boolean,
		keyUsages: KeyUsage[],
	): CryptoKey<any>;
	cryptoInit(c: ClassOf<Crypto>): void;
	cryptoKeyInit(c: ClassOf<CryptoKey<any>>): void;
	cryptoEncrypt(
		algorithm: Algorithm,
		key: CryptoKey<any>,
		data: BufferSource,
	): Promise<ArrayBuffer>;
	cryptoDecrypt(
		algorithm: Algorithm,
		key: CryptoKey<any>,
		data: BufferSource,
	): Promise<ArrayBuffer>;
	cryptoSign(
		algorithm: Algorithm,
		key: CryptoKey<any>,
		data: BufferSource,
	): Promise<ArrayBuffer>;
	cryptoVerify(
		algorithm: Algorithm,
		key: CryptoKey<any>,
		signature: BufferSource,
		data: BufferSource,
	): Promise<boolean>;
	cryptoExportKey(format: string, key: CryptoKey<any>): ArrayBuffer;
	cryptoGenerateKeyEc(namedCurve: string): [ArrayBuffer, ArrayBuffer];
	cryptoKeyNewEcPrivate(
		algorithm: any,
		privateKey: ArrayBuffer,
		publicKey: ArrayBuffer,
		extractable: boolean,
		usages: string[],
	): any;
	cryptoDeriveBits(
		algorithm: any,
		baseKey: CryptoKey<any>,
		length: number,
	): Promise<ArrayBuffer>;
	cryptoDigest(algorithm: string, buf: BufferSource): Promise<ArrayBuffer>;
	cryptoGenerateKeyRsa(
		modulusLength: number,
		publicExponent: number,
	): Promise<ArrayBuffer[]>;
	cryptoKeyNewRsa(
		algoName: string,
		hashName: string,
		type: string,
		n: ArrayBuffer,
		e: ArrayBuffer,
		d: ArrayBuffer | null,
		p: ArrayBuffer | null,
		q: ArrayBuffer | null,
		extractable: boolean,
		usages: string[],
	): CryptoKey<any>;
	cryptoRsaExportComponents(key: CryptoKey<any>): ArrayBuffer[];
	cryptoExportKeyPkcs8(key: CryptoKey<any>): ArrayBuffer;
	cryptoExportKeySpki(key: CryptoKey<any>): ArrayBuffer;
	cryptoImportKeyPkcs8Spki(
		format: string,
		data: BufferSource,
		algoName: string,
		paramName: string,
		extractable: boolean,
		usages: string[],
	): CryptoKey<any>;
	cryptoEcExportPublicRaw(key: CryptoKey<any>): ArrayBuffer;
	sha256Hex(str: string): string;

	// dommatrix.c
	dommatrixNew(values?: number[]): DOMMatrix | DOMMatrixReadOnly;
	dommatrixFromMatrix(init?: DOMMatrixInit): DOMMatrix | DOMMatrixReadOnly;
	dommatrixROInitClass(c: ClassOf<DOMMatrixReadOnly>): void;
	dommatrixInitClass(c: ClassOf<DOMMatrix>): void;
	dommatrixTransformPoint(m: DOMMatrixReadOnly, p: DOMPointInit): DOMPoint;

	// dns.c
	dnsResolve(hostname: string): Promise<string[]>;

	// error.c
	onError(fn: (err: any) => number): void;
	onUnhandledRejection(
		fn: (promise: Promise<unknown>, reason: any) => number,
	): void;

	// font.c
	fontFaceNew(data: ArrayBuffer): FontFace;
	getSystemFont(type: number): ArrayBuffer;

	// fs.c
	fclose(f: FileHandle): Promise<void>;
	fopen(path: string, mode: string, startOffset?: number): Promise<FileHandle>;
	fread(f: FileHandle, buf: ArrayBuffer): Promise<number | null>;
	fwrite(f: FileHandle, data: ArrayBuffer): Promise<void>;
	fsCreateBigFile(path: string): void;
	mkdir(path: string, mode: number): Promise<number>;
	mkdirSync(path: string, mode: number): number;
	openDir(path: string): Promise<object>;
	readDirNext(handle: object): Promise<DirEntry | null>;
	closeDir(handle: object): Promise<void>;
	readDirSync(path: string): string[] | null;
	readFile(path: string, opts?: ReadFileOptions): Promise<ArrayBuffer | null>;
	readFileSync(path: string, opts?: ReadFileOptions): ArrayBuffer | null;
	remove(path: string): Promise<void>;
	removeSync(path: string): void;
	rename(path: string, dest: string): Promise<void>;
	renameSync(path: string, dest: string): void;
	stat(path: string): Promise<Stats | null>;
	statSync(path: string): Stats | null;
	writeFile(path: string, data: ArrayBuffer): Promise<void>;
	writeFileSync(path: string, data: ArrayBuffer): void;
	appendFileSync(path: string, data: ArrayBuffer): void;

	// fsdev.c
	fsInit(c: ClassOf<FileSystem>): void;
	fsMount(fs: FileSystem, name: string): void;
	fsOpenBis(id: number): FileSystem;
	fsOpenSdmc(): FileSystem;
	fsOpenWithId(
		titleId: bigint,
		type: number,
		path: string,
		attributes: number,
	): FileSystem;
	saveDataInit(c: ClassOf<SaveData>): void;
	saveDataCreateSync(info: SaveDataCreationInfo, nacp?: ArrayBuffer): void;
	saveDataMount(saveData: SaveData, name: string): void;
	fsOpenSaveDataInfoReader(saveDataSpaceId: number): SaveDataIterator | null;
	fsSaveDataInfoReaderNext(iterator: SaveDataIterator): SaveData | null;

	// gamepad.c
	gamepadInit(c: ClassOf<Gamepad>): void;
	gamepadNew(index: number): Gamepad;
	gamepadButtonInit(c: ClassOf<GamepadButton>): void;
	gamepadButtonNew(gamepad: Gamepad, index: number): void;

	// image.c
	imageInit(c: ClassOf<Image | ImageBitmap>): void;
	imageNew(width?: number, height?: number): Image | ImageBitmap;
	imageDecode(img: Image | ImageBitmap, data: ArrayBuffer): Promise<void>;
	imageClose(img: ImageBitmap): void;
	imageWriteRGBA(
		img: Image | ImageBitmap,
		bytes: ArrayBuffer | Uint8Array | Uint8ClampedArray,
	): void;
	/** Animation (multi-frame GIF) accessors. `imageFrameCount` returns
	 * 0 for static images and the frame total otherwise; `imageFrameDelay`
	 * returns the per-frame display time in milliseconds; `imageSetFrame`
	 * swaps which frame the cairo surface paints from. */
	imageFrameCount(img: Image | ImageBitmap): number;
	imageFrameDelay(img: Image | ImageBitmap, index: number): number;
	imageSetFrame(img: Image | ImageBitmap, index: number): void;

	// video.c
	videoDecoderInit(c: any): void;
	videoDecoderNew(url: string, opts?: { hwAccel?: boolean }): any;
	videoDecoderClose(v: any): void;
	videoDecoderPlay(v: any): void;
	videoDecoderPause(v: any): void;
	videoDecoderSeek(v: any, seconds: number): void;
	videoDecoderNextFrame(v: any): {
		data: ArrayBuffer | null;
		width: number;
		height: number;
		pts: number;
		ended: boolean;
	} | null;
	videoDecoderSetMuted(v: any, muted: boolean): void;
	videoDecoderSetVolume(v: any, volume: number): void;
	videoDecoderGetAudioLevels(v: any): number[];
	videoDecoderGetFrequencyData(v: any, out: Float32Array): boolean;
	videoDecoderGetWaveform(v: any, out: Float32Array): boolean;

	// irs.c
	irsInit(): () => void;
	irsSensorNew(image: ImageBitmap, color: RGBA): IRSensor;
	irsSensorStart(s: IRSensor): void;
	irsSensorStop(s: IRSensor): void;
	irsSensorUpdate(s: IRSensor): boolean;

	// memory.c
	memoryUsage(): MemoryUsage;
	gc(): void;

	// main.c
	argv: string[];
	entrypoint: string;
	version: Versions;
	exit(): never;
	cwd(): string;
	chdir(dir: string): void;
	print(v: string): void;
	printErr(v: string): void;
	getInternalPromiseState(p: Promise<unknown>): [PromiseState, unknown];
	getenv(name: string): string | undefined;
	setenv(name: string, value: string): void;
	unsetenv(name: string): void;
	envToObject(): Record<string, string>;
	onFrame(fn: (plusDown: boolean) => void): void;
	onExit(fn: () => void): void;
	framebufferInit(screen: Screen): void;
	// Cursor overlay (engine-side composite, see `composite_cursor_overlay`
	// in source/main.c). The cursor visual lives in `display_buffer`, NOT
	// in canvas->data, so it can never corrupt page pixels.
	setCursorOverlay(
		x: number,
		y: number,
		rgba: ArrayBuffer | ArrayBufferView,
		w: number,
		h: number,
	): void;
	setAnimatedCursorOverlay(
		x: number,
		y: number,
		packedRgba: ArrayBuffer | ArrayBufferView,
		w: number,
		h: number,
		frameCount: number,
		frameDelaysMs: ArrayBufferView,
	): void;
	setCursorOverlayPosition(x: number, y: number): void;
	clearCursorOverlay(): void;
	hidInitializeTouchScreen(): void;
	hidGetTouchScreenStates(): Touch[] | undefined;
	hidInitializeKeyboard(): void;
	hidInitializeVibrationDevices(): void;
	hidGetKeyboardStates(): Keys;
	hidSendVibrationValues(v: VibrationValues): void;

	// nifm.c
	nifmInitialize(): () => void;
	networkInfo(): NetworkInfo;

	// sensors.c
	sensorsBatteryInfo(): {
		voltageMv: number;
		temperatureC: number;
		agePercent: number;
		chargePercent: number;
		charging: boolean;
		chargerType: number;
		inputCurrentLimitMa: number;
		fastChargeCurrentLimitMa: number;
		chargeVoltageLimitMv: number;
	};
	sensorsAudioInfo(): {
		masterVolume: number | null;
		devices: string[];
		headphonesConnected: boolean;
		primaryDevice: string | null;
	};
	sensorsWlanInfo(): {
		available: boolean;
		rssi: number | null;
		state: number | null;
	};
	sensorsSixAxisStart(): boolean;
	sensorsSixAxisRead(): {
		acceleration: { x: number; y: number; z: number };
		angularVelocity: { x: number; y: number; z: number };
		angle: { x: number; y: number; z: number };
		samplingNumber: bigint;
		deltaTime: bigint;
	} | null;
	sensorsSixAxisStop(): void;
	sensorsNfpStart(): boolean;
	sensorsNfpPoll(): {
		index: number;
		state: number | null;
		uuid?: ArrayBuffer;
		protocol?: number;
	}[];
	sensorsNfpStop(): void;

	// ns.c
	nsInitialize(): () => void;
	nsAppInit(c: ClassOf<Application>): void;
	nsAppNew(id: string | bigint | ArrayBuffer | null): Application;
	nsAppNext(index: number): bigint | null;

	// service.c
	serviceInit(c: ClassOf<Service>): () => void;
	serviceNew(name?: string): Service;

	// software-keyboard.c
	swkbdCreate(fns: {
		onCancel: (this: VirtualKeyboard) => void;
		onChange: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number,
			dicStartCursorPos: number,
			dicEndCursorPos: number,
		) => void;
		onSubmit: (this: VirtualKeyboard, str: string) => void;
		onCursorMove: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number,
		) => void;
	}): VirtualKeyboard;
	swkbdSetCursorPos(s: VirtualKeyboard, cursorPos: number): void;
	swkbdSetInputText(s: VirtualKeyboard, value: string): void;
	swkbdShow(s: VirtualKeyboard): [number, number, number, number];
	swkbdHide(s: VirtualKeyboard): void;
	swkbdUpdate(this: VirtualKeyboard): void;

	// web.c
	webAppletNew(): any;
	webAppletStart(applet: any, url: string, options: Record<string, any>): void;
	webAppletAppear(applet: any): boolean;
	webAppletSendMessage(applet: any, msg: string): boolean;
	webAppletPollMessages(applet: any): string[];
	webAppletRequestExit(applet: any): void;
	webAppletClose(applet: any): void;
	webAppletIsRunning(applet: any): boolean;
	webAppletGetMode(applet: any): string;

	// tcp.c
	connect(cb: Callback<number>, ip: string, port: number): void;
	write(cb: Callback<number>, fd: number, data: ArrayBuffer): void;
	read(cb: Callback<number>, fd: number, buffer: ArrayBuffer): void;
	close(fd: number): void;
	tcpServerInit(c: any): void;
	tcpServerNew(
		ip: string,
		port: number,
		onAccept: (fd: number) => void,
	): Server;

	// udp.c
	udpInit(c: any): void;
	udpNew(
		ip: string,
		port: number,
		onRecv: (
			err: Error | null,
			data?: ArrayBuffer,
			remoteIp?: string,
			remotePort?: number,
		) => void,
	): DatagramSocket;
	udpSend(
		cb: Callback<number>,
		fd: number,
		data: ArrayBuffer,
		ip: string,
		port: number,
	): void;

	// tls.c
	tlsHandshake(
		cb: Callback<TlsContextOpaque>,
		fd: number,
		hostname: string,
		rejectUnauthorized: boolean,
	): void;
	tlsWrite(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		data: ArrayBuffer,
	): void;
	tlsRead(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		buffer: ArrayBuffer,
	): void;

	// url.c
	urlInit(c: ClassOf<URL>): void;
	urlNew(url: string | URL, base?: string | URL): URL;
	urlSearchInit(c: ClassOf<URLSearchParams>): void;
	urlSearchNew(input: string, url?: URL): URLSearchParams;
	urlSearchIterator(
		params: URLSearchParams,
		type: number,
	): URLSearchParamsIterator;
	urlSearchIteratorNext(it: URLSearchParamsIterator): any;

	// wasm.c
	wasmCallFunc(f: any, ...args: unknown[]): unknown;
	wasmMemNew(descriptor: MemoryDescriptor): Memory;
	wasmTableNew(descriptor: TableDescriptor): Table;
	wasmTableGet(t: any, i: number): Memory;
	wasmTableSet(t: any, i: number, v: any): void;
	wasmTableGrow(t: any, delta: number): number;
	wasmInitMemory(c: any): void;
	wasmInitTable(c: any): void;
	wasmNewModule(b: ArrayBuffer): WasmModuleOpaque;
	wasmNewInstance(
		m: WasmModuleOpaque,
		imports: any[],
	): [WasmInstanceOpaque, any[]];
	wasmNewGlobal(): WasmGlobalOpaque;
	wasmModuleExports(m: WasmModuleOpaque): any[];
	wasmModuleImports(m: WasmModuleOpaque): any[];
	wasmGlobalGet(g: WasmGlobalOpaque): any;
	wasmGlobalSet(g: WasmGlobalOpaque, v: any): void;
	wasmValidate(bytes: ArrayBuffer): boolean;

	// audio.c
	audioInit(): void;
	audioExit(): void;
	audioDecode(
		buffer: ArrayBuffer,
		mimeType: string,
	): Promise<{
		pcmData: ArrayBuffer;
		sampleRate: number;
		channels: number;
		samples: number;
		byteLength: number;
	}>;
	audioPlay(
		pcmData: ArrayBuffer,
		voiceId: number,
		volume: number,
		loop: boolean,
		sampleRate: number,
		channels: number,
		totalSamples: number,
	): void;
	audioStop(voiceId: number): void;
	audioPause(voiceId: number, paused: boolean): void;
	audioSetVolume(voiceId: number, volume: number): void;
	audioSetPitch(voiceId: number, pitch: number): void;
	audioUpdate(): void;
	audioGetPlayedSamples(voiceId: number): number;
	audioAllocVoice(): number;
	audioFreeVoice(voiceId: number): void;
	audioIsPlaying(voiceId: number): boolean;
	/** Allocate a page-aligned PCM ArrayBuffer (4KB alignment) suitable
	 * for passing to `audioPlay`. JS-allocated ArrayBuffers from
	 * `new Int16Array(n).buffer` are NOT page-aligned and `audrvMemPoolAdd`
	 * rejects them; use this allocator for any PCM that the runtime
	 * generates rather than decodes (Oscillator waveforms, createBuffer). */
	audioAllocPCM(byteLength: number): ArrayBuffer;

	// worker.c — Web Workers (see [[project-swb-web-workers-milestone]])
	/** Spawn a real OS-thread Web Worker with a fresh JSRuntime+JSContext.
	 * Returns an integer handle used by the other worker* APIs. Throws
	 * on `pthread_create` failure. The source must include the
	 * structured-clone bootstrap (worker.ts prepends it automatically). */
	workerSpawn(source: string): number;
	/** Post a serialised structured-clone payload (ArrayBuffer) into the
	 * worker's inbound queue. Returns true on success, false if the
	 * handle is unknown. Throws if the inbound queue is full. Pass F:
	 * optional `transfers` is a list of ArrayBuffers whose bytes are
	 * carried via the C-side side-channel and whose sources are detached
	 * by the C layer after attachment. */
	workerPostToWorker(handle: number, bytes: ArrayBuffer, transfers?: ArrayBuffer[]): boolean;
	/** Block until the worker's pthread joins, then free its runtime.
	 * Returns true if a worker was found+terminated, false otherwise. */
	workerTerminate(handle: number): boolean;
	/** Register the JS dispatcher that the C main-loop drain calls
	 * once per inbound message: `(handle, value, kind, transferABs?) =>
	 * void`. When `kind === 0` (DATA), `value` is an ArrayBuffer carrying
	 * a structured-clone payload (worker.ts deserialises before firing
	 * onmessage); `transferABs`, when present, is a JS array of
	 * receiver-owned ArrayBuffers built C-side from the Pass F
	 * side-channel — TAG_TRANSFERRED_AB tags in the payload resolve to
	 * entries here. When `kind === 1` (ERROR), `value` is the exception's
	 * `toString()` string and `transferABs` is undefined. Installed once
	 * at runtime init by worker.ts. */
	workerSetDispatcher(fn: (handle: number, value: unknown, kind: number, transferABs?: ArrayBuffer[]) => void): void;

	// uint8array.c
	uint8arrayInit(c: typeof Uint8Array): void;

	// window.c
	windowInit(c: Window): void;

	applyPath: any;
}

export const $: Init = (globalThis as any).$;
delete (globalThis as any).$;
