import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { createAudioParam } from './audio-param';
import {
	AudioScheduledSourceNode,
	trackActiveSource,
} from './audio-scheduled-source-node';
import {
	ctxInternal,
	nodeInternal,
	NODE_TYPE_OSCILLATOR,
	OSCILLATOR_TYPE_SAWTOOTH,
	OSCILLATOR_TYPE_SINE,
	OSCILLATOR_TYPE_SQUARE,
	OSCILLATOR_TYPE_TRIANGLE,
} from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface OscillatorOptions {
	type?: OscillatorType;
	frequency?: number;
	detune?: number;
	periodicWave?: PeriodicWave;
}

interface OscillatorInternal {
	started: boolean;
	type: OscillatorType;
	frequency: AudioParam;
	detune: AudioParam;
}

const _ = createInternal<OscillatorNode, OscillatorInternal>();

const WAVE_CODE: Partial<Record<OscillatorType, number>> = {
	sine: OSCILLATOR_TYPE_SINE,
	square: OSCILLATOR_TYPE_SQUARE,
	sawtooth: OSCILLATOR_TYPE_SAWTOOTH,
	triangle: OSCILLATOR_TYPE_TRIANGLE,
};

function checkScheduleArg(name: string, paramName: string, v: number) {
	if (!Number.isFinite(v)) {
		throw new TypeError(
			`Failed to execute '${name}' on 'OscillatorNode': The provided ${paramName} is non-finite.`,
		);
	}
	if (v < 0) {
		throw new RangeError(
			`Failed to execute '${name}' on 'OscillatorNode': The ${paramName} provided (${v}) cannot be negative.`,
		);
	}
}

/**
 * An audio source generating a periodic waveform (sine, square, sawtooth or
 * triangle). Used to build tones, chords and simple synths on top of the
 * Web Audio API.
 *
 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode
 */
export class OscillatorNode
	extends AudioScheduledSourceNode
	implements globalThis.OscillatorNode
{
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/OscillatorNode
	 */
	constructor(context: BaseAudioContext, options: OscillatorOptions = {}) {
		const handle = $.audioNodeNew(
			ctxInternal(context).handle,
			NODE_TYPE_OSCILLATOR,
		);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 0,
			numberOfOutputs: 1,
			channelCount: 2,
			channelCountMode: 'max',
			channelInterpretation: 'speakers',
		});
		_.set(this, {
			started: false,
			type: 'sine',
			frequency: createAudioParam(this, handle, 0, {
				defaultValue: 440,
			}),
			detune: createAudioParam(this, handle, 1, {
				defaultValue: 0,
			}),
		});
		const i = _(this);
		if (typeof options.frequency === 'number') {
			i.frequency.value = options.frequency;
		}
		if (typeof options.detune === 'number') {
			i.detune.value = options.detune;
		}
		if (options.type) {
			this.type = options.type;
		}
		if (options.periodicWave) {
			throw new Error(
				"OscillatorNode 'periodicWave' is not yet supported in nx.js",
			);
		}
	}

	/**
	 * The oscillator's periodic waveform. nx.js supports `'sine'`, `'square'`,
	 * `'sawtooth'` and `'triangle'`; `'custom'` (via `setPeriodicWave`) is
	 * not yet implemented.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/type
	 */
	get type(): OscillatorType {
		return _(this).type;
	}

	set type(v: OscillatorType) {
		if (v === 'custom') {
			throw new DOMException(
				"Failed to set the 'type' property on 'OscillatorNode': cannot set type directly to 'custom'.",
				'InvalidStateError',
			);
		}
		const code = WAVE_CODE[v];
		if (code === undefined) {
			throw new TypeError(
				`Failed to set the 'type' property on 'OscillatorNode': The provided value '${v}' is not a valid enum value of type OscillatorType.`,
			);
		}
		$.audioOscillatorSetType(nodeInternal(this).handle, code);
		_(this).type = v;
	}

	/**
	 * The frequency of the oscillation in hertz (a-rate {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/frequency
	 */
	get frequency(): AudioParam {
		return _(this).frequency;
	}

	/**
	 * Detuning of the oscillator, in cents (a-rate {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/detune
	 */
	get detune(): AudioParam {
		return _(this).detune;
	}

	/**
	 * Schedules the oscillator to begin playback.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/start
	 */
	start(when = 0): void {
		checkScheduleArg('start', 'start time', when);
		const i = _(this);
		if (i.started) {
			throw new DOMException(
				"Failed to execute 'start' on 'OscillatorNode': cannot call start more than once.",
				'InvalidStateError',
			);
		}
		i.started = true;
		$.audioSourceStart(nodeInternal(this).handle, when, 0, -1);
		trackActiveSource(this);
	}

	/**
	 * Schedules the oscillator to stop.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OscillatorNode/stop
	 */
	stop(when = 0): void {
		checkScheduleArg('stop', 'stop time', when);
		const i = _(this);
		if (!i.started) {
			throw new DOMException(
				"Failed to execute 'stop' on 'OscillatorNode': cannot call stop without calling start first.",
				'InvalidStateError',
			);
		}
		$.audioSourceStop(nodeInternal(this).handle, when);
	}

	setPeriodicWave(_periodicWave: PeriodicWave): void {
		throw new Error(
			"OscillatorNode 'setPeriodicWave' is not yet supported in nx.js",
		);
	}
}
def(OscillatorNode);
