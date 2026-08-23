import { def } from '../utils';
import { TransformStream } from 'web-streams-polyfill';
import { TextEncoder } from './text-encoder';

/**
 * `TextEncoderStream` — the streaming counterpart to {@link TextEncoder}: a
 * `TransformStream` whose writable side accepts strings and whose readable side
 * emits UTF-8 `Uint8Array` chunks. Added alongside {@link TextDecoderStream} so
 * the streaming text pair is complete (its absence would throw `ReferenceError`
 * the same way, halting the frame loop on an unhandled rejection).
 *
 * A high surrogate at a chunk boundary is held back and paired with the next
 * chunk's leading low surrogate, per the WHATWG encoding standard; a dangling
 * high surrogate at `flush` encodes as U+FFFD (delegated to `TextEncoder`).
 *
 * @see https://developer.mozilla.org/docs/Web/API/TextEncoderStream
 */
export class TextEncoderStream {
	#encoder: TextEncoder;
	#transform: TransformStream<string, Uint8Array>;

	constructor() {
		const encoder = new TextEncoder();
		this.#encoder = encoder;
		// Pending trailing high surrogate carried between chunks (closure state so
		// the transform/flush callbacks share it without private-field gymnastics).
		let pendingHighSurrogate: string | null = null;
		this.#transform = new TransformStream<string, Uint8Array>({
			transform(chunk, controller) {
				let input =
					(pendingHighSurrogate !== null ? pendingHighSurrogate : '') +
					String(chunk);
				pendingHighSurrogate = null;
				if (input.length) {
					const last = input.charCodeAt(input.length - 1);
					if (last >= 0xd800 && last <= 0xdbff) {
						// Defer the lone high surrogate to pair with the next chunk.
						pendingHighSurrogate = input.slice(-1);
						input = input.slice(0, -1);
					}
				}
				if (input) {
					const bytes = encoder.encode(input);
					if (bytes.length) controller.enqueue(bytes);
				}
			},
			flush(controller) {
				if (pendingHighSurrogate !== null) {
					// TextEncoder emits U+FFFD for a lone surrogate.
					const bytes = encoder.encode(pendingHighSurrogate);
					pendingHighSurrogate = null;
					if (bytes.length) controller.enqueue(bytes);
				}
			},
		});
	}

	get encoding(): string {
		return this.#encoder.encoding;
	}
	get readable() {
		return this.#transform.readable;
	}
	get writable() {
		return this.#transform.writable;
	}
}
def(TextEncoderStream, 'TextEncoderStream');
