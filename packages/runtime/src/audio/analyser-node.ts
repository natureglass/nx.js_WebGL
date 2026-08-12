import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioNode, type AudioNodeOptions } from './audio-node';
import { ctxInternal, NODE_TYPE_ANALYSER, nodeInternal } from './internal';
import type { BaseAudioContext } from './base-audio-context';

export interface AnalyserOptions extends AudioNodeOptions {
	fftSize?: number;
	maxDecibels?: number;
	minDecibels?: number;
	smoothingTimeConstant?: number;
}

const VALID_FFT_SIZES = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768];

// State lives in a WeakMap (like the other node subclasses) rather than in
// `#private` fields: a class with private fields is nominally distinct from
// the global `AnalyserNode` interface, which would make `createAnalyser()`'s
// return value fail to satisfy `BaseAudioContext` and cascade type errors.
const _ = createInternal<
	AnalyserNode,
	{
		fftSize: number;
		minDecibels: number;
		maxDecibels: number;
		smoothing: number;
		time: Float32Array; // native time-domain readback target
		smoothed: Float32Array; // smoothed linear magnitude (per spec)
		db: Float32Array; // dB spectrum scratch
	}
>();

/**
 * Provides real-time frequency- and time-domain analysis of the audio passing
 * through it. The node is a passthrough (its output equals its input); the
 * native graph copies each rendered output sample into a ring buffer that the
 * `getXTimeDomainData` methods read. Frequency-domain data is derived here in
 * JS by windowing + FFT of that time-domain window.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AnalyserNode
 */
export class AnalyserNode extends AudioNode implements globalThis.AnalyserNode {
	constructor(context: BaseAudioContext, options: AnalyserOptions = {}) {
		const handle = $.audioNodeNew(ctxInternal(context).handle, NODE_TYPE_ANALYSER);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 1,
			numberOfOutputs: 1,
			channelCount: options.channelCount ?? 2,
			channelCountMode: options.channelCountMode ?? 'max',
			channelInterpretation: options.channelInterpretation ?? 'speakers',
		});
		_.set(this, {
			fftSize: 2048,
			minDecibels: options.minDecibels ?? -100,
			maxDecibels: options.maxDecibels ?? -30,
			smoothing: options.smoothingTimeConstant ?? 0.8,
			time: new Float32Array(2048),
			smoothed: new Float32Array(1024),
			db: new Float32Array(1024),
		});
		if (options.fftSize !== undefined) this.fftSize = options.fftSize;
	}

	/**
	 * The size of the FFT used for frequency analysis (a power of two in
	 * [32, 32768]). Setting it resizes the time-domain window read by
	 * `getXTimeDomainData`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AnalyserNode/fftSize
	 */
	get fftSize(): number {
		return _(this).fftSize;
	}
	set fftSize(value: number) {
		if (!VALID_FFT_SIZES.includes(value)) {
			throw new DOMException(
				`The FFT size provided (${value}) is not a power of two between 32 and 32768`,
				'IndexSizeError',
			);
		}
		const i = _(this);
		i.fftSize = value;
		const bins = value >> 1;
		i.time = new Float32Array(value);
		i.smoothed = new Float32Array(bins);
		i.db = new Float32Array(bins);
	}

	get frequencyBinCount(): number {
		return _(this).fftSize >> 1;
	}

	get minDecibels(): number {
		return _(this).minDecibels;
	}
	set minDecibels(v: number) {
		_(this).minDecibels = v;
	}
	get maxDecibels(): number {
		return _(this).maxDecibels;
	}
	set maxDecibels(v: number) {
		_(this).maxDecibels = v;
	}
	get smoothingTimeConstant(): number {
		return _(this).smoothing;
	}
	set smoothingTimeConstant(v: number) {
		_(this).smoothing = v;
	}

	getFloatTimeDomainData(array: Float32Array): void {
		const i = _(this);
		readTime(this, i);
		const n = Math.min(array.length, i.fftSize);
		for (let k = 0; k < n; k++) array[k] = i.time[k];
	}

	getByteTimeDomainData(array: Uint8Array): void {
		const i = _(this);
		readTime(this, i);
		const n = Math.min(array.length, i.fftSize);
		for (let k = 0; k < n; k++) {
			const v = Math.round(128 + i.time[k] * 128);
			array[k] = v < 0 ? 0 : v > 255 ? 255 : v;
		}
	}

	getFloatFrequencyData(array: Float32Array): void {
		const i = _(this);
		const db = computeSpectrumDb(this, i);
		const n = Math.min(array.length, i.fftSize >> 1);
		for (let k = 0; k < n; k++) array[k] = db[k];
	}

	getByteFrequencyData(array: Uint8Array): void {
		const i = _(this);
		const db = computeSpectrumDb(this, i);
		const n = Math.min(array.length, i.fftSize >> 1);
		const min = i.minDecibels;
		const range = i.maxDecibels - min || 1;
		for (let k = 0; k < n; k++) {
			const v = Math.round((255 * (db[k] - min)) / range);
			array[k] = v < 0 ? 0 : v > 255 ? 255 : v;
		}
	}
}
def(AnalyserNode);

type AnalyserState = ReturnType<typeof _>;

function readTime(node: AnalyserNode, i: AnalyserState): void {
	if (i.time.length !== i.fftSize) i.time = new Float32Array(i.fftSize);
	$.audioAnalyserFloatTimeData(nodeInternal(node).handle, i.time);
}

/** FFT the current (windowed) time-domain window and return the smoothed
 * magnitude spectrum in dB (length = frequencyBinCount). */
function computeSpectrumDb(node: AnalyserNode, i: AnalyserState): Float32Array {
	readTime(node, i);
	const N = i.fftSize;
	const bins = N >> 1;
	const re = new Float32Array(N);
	const im = new Float32Array(N);
	// Blackman window (WebAudio's analysis window) over the time data.
	const a0 = 0.42, a1 = 0.5, a2 = 0.08;
	const denom = N - 1 || 1;
	for (let k = 0; k < N; k++) {
		const w =
			a0 - a1 * Math.cos((2 * Math.PI * k) / denom) + a2 * Math.cos((4 * Math.PI * k) / denom);
		re[k] = i.time[k] * w;
	}
	fftRadix2(re, im);
	const smooth = i.smoothing;
	const smoothed = i.smoothed;
	const db = i.db;
	for (let k = 0; k < bins; k++) {
		// Spec normalises the FFT magnitude by fftSize.
		const mag = Math.hypot(re[k], im[k]) / N;
		// Temporal smoothing against the previous frame's linear magnitude.
		const cur = smooth * smoothed[k] + (1 - smooth) * mag;
		smoothed[k] = cur;
		db[k] = 20 * Math.log10(cur > 1e-10 ? cur : 1e-10);
	}
	return db;
}

/** In-place iterative radix-2 FFT (decimation-in-time). `re.length` must be a
 * power of two; `im` starts all-zero for a real input signal. */
function fftRadix2(re: Float32Array, im: Float32Array): void {
	const n = re.length;
	// Bit-reversal permutation.
	for (let i = 1, j = 0; i < n; i++) {
		let bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) {
			const tr = re[i];
			re[i] = re[j];
			re[j] = tr;
			const ti = im[i];
			im[i] = im[j];
			im[j] = ti;
		}
	}
	for (let len = 2; len <= n; len <<= 1) {
		const half = len >> 1;
		const ang = (-2 * Math.PI) / len;
		const wRe = Math.cos(ang);
		const wIm = Math.sin(ang);
		for (let i = 0; i < n; i += len) {
			let curRe = 1;
			let curIm = 0;
			for (let k = 0; k < half; k++) {
				const aRe = re[i + k];
				const aIm = im[i + k];
				const bRe = re[i + k + half];
				const bIm = im[i + k + half];
				const tRe = bRe * curRe - bIm * curIm;
				const tIm = bRe * curIm + bIm * curRe;
				re[i + k] = aRe + tRe;
				im[i + k] = aIm + tIm;
				re[i + k + half] = aRe - tRe;
				im[i + k + half] = aIm - tIm;
				const nRe = curRe * wRe - curIm * wIm;
				curIm = curRe * wIm + curIm * wRe;
				curRe = nRe;
			}
		}
	}
}
