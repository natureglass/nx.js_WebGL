import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioNode, type AudioNodeOptions } from './audio-node';
import { createAudioParam } from './audio-param';
import { ctxInternal, NODE_TYPE_DELAY } from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface DelayOptions extends AudioNodeOptions {
	maxDelayTime?: number;
	delayTime?: number;
}

const _ = createInternal<DelayNode, { delayTime: AudioParam }>();

/**
 * An {@link AudioNode} which delays its input by a (possibly automated) amount
 * of time, up to `maxDelayTime`. Because the delay line reads already-written
 * (past) samples, a `DelayNode` may legally sit inside a feedback cycle
 * (`delay → gain → delay`) to build echo effects.
 *
 * > [!NOTE]
 * > nx.js evaluates `delayTime` once per render quantum (k-rate) and floors it
 * > at one render quantum (~2.7 ms at 48 kHz) — the Web Audio minimum for a
 * > delay used in a cycle.
 *
 * @see https://developer.mozilla.org/docs/Web/API/DelayNode
 */
export class DelayNode extends AudioNode implements globalThis.DelayNode {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/DelayNode/DelayNode
	 */
	constructor(context: BaseAudioContext, options: DelayOptions = {}) {
		const maxDelayTime = options.maxDelayTime ?? 1;
		if (!(maxDelayTime > 0 && maxDelayTime < 180)) {
			throw new DOMException(
				`The max delay time provided (${maxDelayTime}) is outside the range (0, 180)`,
				'NotSupportedError',
			);
		}
		const handle = $.audioNodeNew(
			ctxInternal(context).handle,
			NODE_TYPE_DELAY,
			maxDelayTime,
		);
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
		const delayTime = createAudioParam(this, handle, 0, {
			defaultValue: 0,
			minValue: 0,
			maxValue: maxDelayTime,
		});
		if (typeof options.delayTime === 'number') {
			delayTime.value = options.delayTime;
		}
		_.set(this, { delayTime });
	}

	/**
	 * The amount of delay to apply, in seconds (an a-rate {@link AudioParam},
	 * evaluated k-rate in nx.js), capped at the node's `maxDelayTime`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/DelayNode/delayTime
	 */
	get delayTime(): AudioParam {
		return _(this).delayTime;
	}
}
def(DelayNode);
