import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioNode, type AudioNodeOptions } from './audio-node';
import { createAudioParam } from './audio-param';
import { ctxInternal, nodeInternal, NODE_TYPE_DYNAMICS_COMPRESSOR } from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface DynamicsCompressorOptions extends AudioNodeOptions {
	attack?: number;
	knee?: number;
	ratio?: number;
	release?: number;
	threshold?: number;
}

interface DynamicsCompressorInternal {
	threshold: AudioParam;
	knee: AudioParam;
	ratio: AudioParam;
	attack: AudioParam;
	release: AudioParam;
}

const _ = createInternal<DynamicsCompressorNode, DynamicsCompressorInternal>();

/**
 * An {@link AudioNode} which applies dynamics compression — lowering the
 * volume of the loudest parts of the signal to even out level. nx.js
 * implements a feed-forward peak compressor with a soft knee and attack /
 * release smoothing.
 *
 * > [!NOTE]
 * > nx.js does not apply automatic makeup gain (the output is only ever
 * > attenuated) and evaluates the five parameters once per render quantum
 * > (k-rate).
 *
 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode
 */
export class DynamicsCompressorNode
	extends AudioNode
	implements globalThis.DynamicsCompressorNode
{
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/DynamicsCompressorNode
	 */
	constructor(
		context: BaseAudioContext,
		options: DynamicsCompressorOptions = {},
	) {
		const handle = $.audioNodeNew(
			ctxInternal(context).handle,
			NODE_TYPE_DYNAMICS_COMPRESSOR,
		);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 1,
			numberOfOutputs: 1,
			channelCount: options.channelCount ?? 2,
			channelCountMode: options.channelCountMode ?? 'clamped-max',
			channelInterpretation: options.channelInterpretation ?? 'speakers',
		});
		const i: DynamicsCompressorInternal = {
			threshold: createAudioParam(this, handle, 0, {
				defaultValue: -24,
				minValue: -100,
				maxValue: 0,
				automationRate: 'k-rate',
			}),
			knee: createAudioParam(this, handle, 1, {
				defaultValue: 30,
				minValue: 0,
				maxValue: 40,
				automationRate: 'k-rate',
			}),
			ratio: createAudioParam(this, handle, 2, {
				defaultValue: 12,
				minValue: 1,
				maxValue: 20,
				automationRate: 'k-rate',
			}),
			attack: createAudioParam(this, handle, 3, {
				defaultValue: 0.003,
				minValue: 0,
				maxValue: 1,
				automationRate: 'k-rate',
			}),
			release: createAudioParam(this, handle, 4, {
				defaultValue: 0.25,
				minValue: 0,
				maxValue: 1,
				automationRate: 'k-rate',
			}),
		};
		if (typeof options.threshold === 'number')
			i.threshold.value = options.threshold;
		if (typeof options.knee === 'number') i.knee.value = options.knee;
		if (typeof options.ratio === 'number') i.ratio.value = options.ratio;
		if (typeof options.attack === 'number') i.attack.value = options.attack;
		if (typeof options.release === 'number')
			i.release.value = options.release;
		_.set(this, i);
	}

	/**
	 * The decibel value above which compression starts (a k-rate
	 * {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/threshold
	 */
	get threshold(): AudioParam {
		return _(this).threshold;
	}

	/**
	 * The decibel range above the threshold where the transition to compression
	 * is smoothed (a k-rate {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/knee
	 */
	get knee(): AudioParam {
		return _(this).knee;
	}

	/**
	 * The amount of input change, in dB, needed for a 1 dB output change (a
	 * k-rate {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/ratio
	 */
	get ratio(): AudioParam {
		return _(this).ratio;
	}

	/**
	 * The time, in seconds, to reduce the gain by 10 dB (a k-rate
	 * {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/attack
	 */
	get attack(): AudioParam {
		return _(this).attack;
	}

	/**
	 * The time, in seconds, to increase the gain by 10 dB (a k-rate
	 * {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/release
	 */
	get release(): AudioParam {
		return _(this).release;
	}

	/**
	 * The current gain reduction applied by the compressor, in decibels
	 * (a read-only value, `<= 0`).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DynamicsCompressorNode/reduction
	 */
	get reduction(): number {
		return $.audioCompressorReduction(nodeInternal(this).handle);
	}
}
def(DynamicsCompressorNode);
