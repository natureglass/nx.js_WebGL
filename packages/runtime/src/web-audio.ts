import { $ } from './$';
import { ensureAudioInit, ensureUpdateLoop } from './audio';
import { EventTarget } from './polyfills/event-target';
import { Event } from './polyfills/event';
import { clearTimeout, setTimeout } from './timers';
import { def } from './utils';

/**
 * Tier-1 Web Audio API polyfill. Wraps the existing `$.audio*` native
 * bindings (24-voice audrv mixer, MP3/WAV/OGG decoders) in the standard
 * AudioContext / AudioBuffer / Source / Gain / Oscillator surface so
 * itch.io-style HTML5 games run unmodified.
 *
 * Supported:
 *   - `AudioContext` (running/closed states, `currentTime`, `destination`, `sampleRate`)
 *   - `AudioBuffer` (decoded PCM, channel data accessors)
 *   - `AudioBufferSourceNode` (`start`/`stop`/`loop`/`playbackRate`/`onended`)
 *   - `GainNode` (`gain.value` propagates to the source's voice immediately)
 *   - `OscillatorNode` (sine/square/sawtooth/triangle, generated as a
 *     looping PCM buffer; `frequency.value`/`type` honored before start)
 *   - `decodeAudioData(buffer, success?, error?)`
 *
 * Not yet supported (Tier 2+):
 *   - `AudioParam` scheduling methods (`setValueAtTime`, ramps) — `value`
 *     setter applies immediately
 *   - Sample-accurate `start(when)` — `when` is honored relative to
 *     `currentTime` via `setTimeout` (millisecond granularity, not
 *     sample-accurate)
 *   - `loopStart` / `loopEnd` — `loop` is whole-buffer only
 *   - `AnalyserNode`, `PannerNode`, `BiquadFilterNode`, etc.
 *   - `AudioWorklet`
 *
 * Voice budget: the underlying audrv mixer has 24 voices shared across
 * all audio APIs in the process (Web Audio + `Audio` element + `<video>`
 * audio). A `AudioBufferSourceNode.start()` allocates a voice; `stop()`
 * or end-of-playback frees it.
 */

const NATIVE_SAMPLE_RATE = 48000;

const sineTable = (() => {
	// Generate one cycle of each waveform shape at the native sample
	// rate, sized so it loops cleanly at common game-tone frequencies.
	// At 48kHz a 1-second buffer is the worst case; we generate per
	// oscillator on demand to honor the actual frequency value.
	return null;
})();

function generateWaveform(
	type: OscillatorType,
	frequency: number,
	durationSeconds: number,
	sampleRate: number,
): { pcm: Int16Array; alignedBuffer: ArrayBuffer } {
	const sampleCount = Math.max(1, Math.floor(durationSeconds * sampleRate));
	// Allocate via the native page-aligned allocator so the resulting
	// PCM can be handed straight to `$.audioPlay` (audrvMemPoolAdd
	// rejects non-4KB-aligned pointers).
	const alignedBuffer = $.audioAllocPCM(sampleCount * 2);
	const out = new Int16Array(alignedBuffer, 0, sampleCount);
	const phaseInc = (2 * Math.PI * frequency) / sampleRate;
	let phase = 0;
	const amp = 0x7000; // ~0.87 of full scale — leaves headroom for mixing
	switch (type) {
		case 'square':
			for (let i = 0; i < sampleCount; i++) {
				out[i] = (phase % (2 * Math.PI)) < Math.PI ? amp : -amp;
				phase += phaseInc;
			}
			break;
		case 'sawtooth':
			for (let i = 0; i < sampleCount; i++) {
				const p = (phase % (2 * Math.PI)) / Math.PI - 1; // -1..1
				out[i] = (p * amp) | 0;
				phase += phaseInc;
			}
			break;
		case 'triangle':
			for (let i = 0; i < sampleCount; i++) {
				const p = (phase % (2 * Math.PI)) / Math.PI; // 0..2
				const t = p < 1 ? p * 2 - 1 : 3 - p * 2; // -1..1
				out[i] = (t * amp) | 0;
				phase += phaseInc;
			}
			break;
		case 'sine':
		default:
			for (let i = 0; i < sampleCount; i++) {
				out[i] = (Math.sin(phase) * amp) | 0;
				phase += phaseInc;
			}
			break;
	}
	return { pcm: out, alignedBuffer };
}

export type AudioContextState = 'suspended' | 'running' | 'closed';
export type OscillatorType = 'sine' | 'square' | 'sawtooth' | 'triangle' | 'custom';

/**
 * `AudioParam` — scheduled audio parameter. Tier-1 implements only the
 * `value` setter (applies immediately to the connected voice if any);
 * the scheduling methods are stubs that snap to the target value at
 * the requested time via `setTimeout`. Real sample-accurate ramps are
 * out of scope.
 */
export class AudioParam {
	#value: number;
	#defaultValue: number;
	#onChange: ((v: number) => void) | null = null;

	constructor(defaultValue: number, onChange?: (v: number) => void) {
		this.#value = defaultValue;
		this.#defaultValue = defaultValue;
		this.#onChange = onChange ?? null;
	}

	get value(): number { return this.#value; }
	set value(v: number) {
		this.#value = v;
		if (this.#onChange) this.#onChange(v);
	}
	get defaultValue(): number { return this.#defaultValue; }
	get minValue(): number { return -3.4028235e38; }
	get maxValue(): number { return 3.4028235e38; }

	setValueAtTime(value: number, startTime: number): AudioParam {
		const ctx = currentContext;
		const delay = ctx ? Math.max(0, (startTime - ctx.currentTime) * 1000) : 0;
		if (delay <= 0) { this.value = value; return this; }
		setTimeout(() => { this.value = value; }, delay);
		return this;
	}
	linearRampToValueAtTime(value: number, endTime: number): AudioParam {
		return this.setValueAtTime(value, endTime);
	}
	exponentialRampToValueAtTime(value: number, endTime: number): AudioParam {
		return this.setValueAtTime(value, endTime);
	}
	setTargetAtTime(target: number, _startTime: number, _timeConstant: number): AudioParam {
		this.value = target;
		return this;
	}
	cancelScheduledValues(_startTime: number): AudioParam { return this; }
	cancelAndHoldAtTime(_cancelTime: number): AudioParam { return this; }
}

/** Base class for all audio graph nodes. `connect`/`disconnect` track
 * the downstream node graph; the actual signal path is realised by
 * voice gain multiplication at `start()` time. */
export class AudioNode extends EventTarget {
	readonly context: AudioContext;
	readonly numberOfInputs: number = 1;
	readonly numberOfOutputs: number = 1;
	readonly channelCount: number = 2;
	readonly channelCountMode: 'max' | 'clamped-max' | 'explicit' = 'max';
	readonly channelInterpretation: 'speakers' | 'discrete' = 'speakers';
	/** Downstream nodes in the graph (Tier-1: only used to walk the
	 * chain when a source asks "what's my final gain to destination"). */
	_outputs: AudioNode[] = [];

	constructor(context: AudioContext) { super(); this.context = context; }

	connect(destination: AudioNode, _output?: number, _input?: number): AudioNode {
		if (!destination) throw new TypeError('connect: destination required');
		if (this._outputs.indexOf(destination) < 0) this._outputs.push(destination);
		return destination;
	}
	disconnect(destination?: AudioNode, _output?: number, _input?: number): void {
		if (!destination) { this._outputs.length = 0; return; }
		const idx = this._outputs.indexOf(destination);
		if (idx >= 0) this._outputs.splice(idx, 1);
	}

	/** Walk the connected chain to the destination and multiply every
	 * GainNode's current value along the way. Source nodes call this
	 * just before `$.audioPlay` to compute the effective voice gain. */
	_computeChainGain(): number {
		let total = 1;
		const visited = new Set<AudioNode>();
		const walk = (n: AudioNode) => {
			if (visited.has(n)) return;
			visited.add(n);
			if (n instanceof GainNode) total *= n.gain.value;
			for (const next of n._outputs) walk(next);
		};
		for (const next of this._outputs) walk(next);
		return Math.max(0, Math.min(1, total));
	}
}

/** Singleton terminal node — `ctx.destination`. No real wiring needed:
 * every voice already routes to the audrv final mix. */
export class AudioDestinationNode extends AudioNode {
	readonly maxChannelCount: number = 2;
	constructor(context: AudioContext) {
		super(context);
		(this as any).channelCount = 2;
	}
}

export class AudioBuffer {
	readonly sampleRate: number;
	readonly length: number;
	readonly numberOfChannels: number;
	/** Interleaved Int16 PCM as returned by `$.audioDecode`, page-aligned
	 * so audrv mempool can ingest it directly. */
	_pcmInt16: ArrayBuffer;
	/** Lazily-allocated per-channel Float32 views for `getChannelData`. */
	#channelData: Float32Array[] | null = null;

	constructor(args: {
		sampleRate: number;
		length: number;
		numberOfChannels: number;
		pcmInt16: ArrayBuffer;
	}) {
		this.sampleRate = args.sampleRate;
		this.length = args.length;
		this.numberOfChannels = args.numberOfChannels;
		this._pcmInt16 = args.pcmInt16;
	}

	get duration(): number { return this.length / this.sampleRate; }

	getChannelData(channel: number): Float32Array {
		if (channel < 0 || channel >= this.numberOfChannels) {
			throw new RangeError(`channel ${channel} out of range`);
		}
		if (!this.#channelData) {
			this.#channelData = new Array(this.numberOfChannels);
			const view = new Int16Array(this._pcmInt16);
			for (let c = 0; c < this.numberOfChannels; c++) {
				const buf = new Float32Array(this.length);
				for (let i = 0; i < this.length; i++) {
					buf[i] = view[i * this.numberOfChannels + c] / 32768;
				}
				this.#channelData[c] = buf;
			}
		}
		return this.#channelData[channel];
	}

	copyFromChannel(destination: Float32Array, channelNumber: number, startInChannel = 0): void {
		const src = this.getChannelData(channelNumber);
		destination.set(src.subarray(startInChannel, startInChannel + destination.length));
	}

	copyToChannel(source: Float32Array, channelNumber: number, startInChannel = 0): void {
		if (channelNumber < 0 || channelNumber >= this.numberOfChannels) {
			throw new RangeError(`channel ${channelNumber} out of range`);
		}
		const view = new Int16Array(this._pcmInt16);
		for (let i = 0; i < source.length; i++) {
			const v = Math.max(-1, Math.min(1, source[i]));
			view[(startInChannel + i) * this.numberOfChannels + channelNumber] = (v * 32767) | 0;
		}
		this.#channelData = null; // invalidate cache
	}
}

export class AudioBufferSourceNode extends AudioNode {
	buffer: AudioBuffer | null = null;
	loop = false;
	loopStart = 0;
	loopEnd = 0;
	readonly playbackRate: AudioParam;
	readonly detune: AudioParam;
	onended: ((this: AudioBufferSourceNode, ev: Event) => any) | null = null;
	#voiceId = -1;
	#started = false;
	#ended = false;
	#endTimer: ReturnType<typeof setTimeout> | null = null;

	constructor(context: AudioContext) {
		super(context);
		this.playbackRate = new AudioParam(1.0, (v) => {
			if (this.#voiceId >= 0) $.audioSetPitch(this.#voiceId, v);
		});
		this.detune = new AudioParam(0);
	}

	start(when = 0, offset = 0, _duration?: number): void {
		if (this.#started) {
			throw new DOMException('source already started', 'InvalidStateError');
		}
		this.#started = true;
		const delay = Math.max(0, (when - this.context.currentTime) * 1000);
		const fire = () => {
			if (this.#ended || !this.buffer) return;
			ensureAudioInit();
			ensureUpdateLoop();
			const v = $.audioAllocVoice() as number;
			if (v < 0) {
				this.#ended = true;
				this._fireEnded();
				return;
			}
			this.#voiceId = v;
			const buf = this.buffer;
			const gain = this._computeChainGain();
			$.audioPlay(
				buf._pcmInt16,
				v,
				gain,
				this.loop,
				buf.sampleRate,
				buf.numberOfChannels,
				buf.length,
			);
			if (this.playbackRate.value !== 1) $.audioSetPitch(v, this.playbackRate.value);
			// Track this source on the context so connected GainNodes
			// can find it for live-volume updates.
			this.context._registerSource(this);
			if (!this.loop) {
				const durMs = Math.max(0, (buf.length / buf.sampleRate - offset) * 1000 / Math.max(0.001, this.playbackRate.value));
				this.#endTimer = setTimeout(() => {
					this.#ended = true;
					this._free();
					this._fireEnded();
				}, durMs);
			}
		};
		if (delay <= 0) fire();
		else setTimeout(fire, delay);
	}

	stop(when = 0): void {
		const delay = Math.max(0, (when - this.context.currentTime) * 1000);
		const doStop = () => {
			if (this.#ended) return;
			this.#ended = true;
			this._free();
			this._fireEnded();
		};
		if (delay <= 0) doStop();
		else setTimeout(doStop, delay);
	}

	_currentVoiceId(): number { return this.#voiceId; }

	_applyVoiceGain(): void {
		if (this.#voiceId >= 0) {
			$.audioSetVolume(this.#voiceId, this._computeChainGain());
		}
	}

	_free(): void {
		if (this.#endTimer) { clearTimeout(this.#endTimer); this.#endTimer = null; }
		if (this.#voiceId >= 0) {
			try { $.audioStop(this.#voiceId); } catch (_) { /* ignore */ }
			try { $.audioFreeVoice(this.#voiceId); } catch (_) { /* ignore */ }
			this.#voiceId = -1;
		}
		this.context._unregisterSource(this);
	}

	_fireEnded(): void {
		const ev = new Event('ended');
		if (typeof this.onended === 'function') this.onended.call(this, ev);
		this.dispatchEvent(ev);
	}
}

export class GainNode extends AudioNode {
	readonly gain: AudioParam;

	constructor(context: AudioContext, options?: { gain?: number }) {
		super(context);
		this.gain = new AudioParam(options?.gain ?? 1.0, () => this._propagateGain());
	}

	/** When `gain.value` changes, walk all currently-playing sources
	 * that route through THIS node and recompute their voice volume. */
	_propagateGain(): void {
		for (const src of this.context._activeSources) {
			// Cheap check: does src's chain include this gain? Walk
			// from src and look for `this`.
			if (this._sourceReachesMe(src)) src._applyVoiceGain();
		}
	}

	_sourceReachesMe(src: AudioNode): boolean {
		const seen = new Set<AudioNode>();
		const walk = (n: AudioNode): boolean => {
			if (n === this) return true;
			if (seen.has(n)) return false;
			seen.add(n);
			for (const next of n._outputs) if (walk(next)) return true;
			return false;
		};
		return walk(src);
	}
}

export class OscillatorNode extends AudioNode {
	type: OscillatorType = 'sine';
	readonly frequency: AudioParam;
	readonly detune: AudioParam;
	onended: ((this: OscillatorNode, ev: Event) => any) | null = null;
	#source: AudioBufferSourceNode | null = null;
	#started = false;

	constructor(context: AudioContext) {
		super(context);
		this.frequency = new AudioParam(440);
		this.detune = new AudioParam(0);
	}

	start(when = 0): void {
		if (this.#started) throw new DOMException('oscillator already started', 'InvalidStateError');
		this.#started = true;
		// Generate a 1-second waveform buffer at the chosen frequency
		// and loop it. Detune is ignored in Tier-1. PCM is page-aligned
		// via $.audioAllocPCM so audrvMemPoolAdd accepts it.
		const { pcm, alignedBuffer } = generateWaveform(this.type, this.frequency.value, 1.0, NATIVE_SAMPLE_RATE);
		const buffer = new AudioBuffer({
			sampleRate: NATIVE_SAMPLE_RATE,
			length: pcm.length,
			numberOfChannels: 1,
			pcmInt16: alignedBuffer,
		});
		this.#source = new AudioBufferSourceNode(this.context);
		this.#source.buffer = buffer;
		this.#source.loop = true;
		// Splice the source into our outgoing graph so chain-gain works.
		for (const out of this._outputs) this.#source.connect(out);
		this.#source.onended = (ev) => {
			if (typeof this.onended === 'function') this.onended.call(this, ev);
		};
		this.#source.start(when);
	}

	stop(when = 0): void {
		if (this.#source) this.#source.stop(when);
	}
}

let currentContext: AudioContext | null = null;

export class AudioContext extends EventTarget {
	readonly destination: AudioDestinationNode;
	readonly sampleRate: number = NATIVE_SAMPLE_RATE;
	readonly baseLatency: number = 0.02;
	readonly outputLatency: number = 0.02;
	#state: AudioContextState = 'suspended';
	#startTime = 0;
	#suspendOffset = 0;
	#suspendedAt = 0;
	/** All BufferSourceNodes currently holding a voice; used by
	 * GainNode propagation to find affected sources. */
	_activeSources = new Set<AudioBufferSourceNode>();

	constructor(_options?: { sampleRate?: number; latencyHint?: any }) {
		super();
		ensureAudioInit();
		ensureUpdateLoop();
		this.destination = new AudioDestinationNode(this);
		this.#startTime = performance.now();
		this.#state = 'running';
		if (!currentContext) currentContext = this;
	}

	get state(): AudioContextState { return this.#state; }
	get currentTime(): number {
		if (this.#state === 'closed') return 0;
		if (this.#state === 'suspended') return this.#suspendOffset / 1000;
		return ((performance.now() - this.#startTime) + this.#suspendOffset) / 1000;
	}

	createBuffer(numberOfChannels: number, length: number, sampleRate: number): AudioBuffer {
		// Allocate via $.audioAllocPCM so the buffer is page-aligned
		// (4KB) and `audrvMemPoolAdd` will accept it on play. The native
		// allocator also zeros + dcache-flushes the region.
		const byteLength = numberOfChannels * length * 2;
		const pcmAligned = $.audioAllocPCM(byteLength);
		return new AudioBuffer({
			sampleRate,
			length,
			numberOfChannels,
			pcmInt16: pcmAligned,
		});
	}

	createBufferSource(): AudioBufferSourceNode { return new AudioBufferSourceNode(this); }
	createGain(): GainNode { return new GainNode(this); }
	createOscillator(): OscillatorNode { return new OscillatorNode(this); }

	decodeAudioData(
		buffer: ArrayBuffer,
		successCallback?: (buf: AudioBuffer) => void,
		errorCallback?: (err: any) => void,
	): Promise<AudioBuffer> {
		// We don't know the source mime up front. Try MP3 → WAV → OGG
		// by header sniff so any of the three formats round-trips.
		const view = new Uint8Array(buffer);
		let mime = 'audio/mpeg';
		if (view.length >= 4 && view[0] === 0x52 && view[1] === 0x49 && view[2] === 0x46 && view[3] === 0x46) {
			mime = 'audio/wav'; // "RIFF"
		} else if (view.length >= 4 && view[0] === 0x4F && view[1] === 0x67 && view[2] === 0x67 && view[3] === 0x53) {
			mime = 'audio/ogg'; // "OggS"
		} else if (view.length >= 3 && view[0] === 0x49 && view[1] === 0x44 && view[2] === 0x33) {
			mime = 'audio/mpeg'; // "ID3"
		}
		const p = ($.audioDecode(buffer, mime) as Promise<any>).then((decoded) => {
			const ab = new AudioBuffer({
				sampleRate: decoded.sampleRate,
				length: decoded.samples,
				numberOfChannels: decoded.channels,
				pcmInt16: decoded.pcmData,
			});
			if (successCallback) try { successCallback(ab); } catch (_) {}
			return ab;
		}, (err) => {
			if (errorCallback) try { errorCallback(err); } catch (_) {}
			throw err;
		});
		return p;
	}

	async suspend(): Promise<void> {
		if (this.#state !== 'running') return;
		this.#state = 'suspended';
		this.#suspendOffset += performance.now() - this.#startTime;
		this.#suspendedAt = performance.now();
		for (const src of this._activeSources) {
			const v = src._currentVoiceId();
			if (v >= 0) $.audioPause(v, true);
		}
	}

	async resume(): Promise<void> {
		if (this.#state !== 'suspended') return;
		this.#state = 'running';
		this.#startTime = performance.now();
		for (const src of this._activeSources) {
			const v = src._currentVoiceId();
			if (v >= 0) $.audioPause(v, false);
		}
	}

	async close(): Promise<void> {
		for (const src of Array.from(this._activeSources)) src._free();
		this._activeSources.clear();
		this.#state = 'closed';
		if (currentContext === this) currentContext = null;
	}

	_registerSource(s: AudioBufferSourceNode): void { this._activeSources.add(s); }
	_unregisterSource(s: AudioBufferSourceNode): void { this._activeSources.delete(s); }
}

def(AudioContext);
def(AudioContext, 'webkitAudioContext');
def(AudioNode);
def(AudioDestinationNode);
def(AudioBuffer);
def(AudioBufferSourceNode);
def(GainNode);
def(OscillatorNode);
def(AudioParam);
