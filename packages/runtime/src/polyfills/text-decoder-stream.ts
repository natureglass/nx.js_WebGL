import { def } from '../utils';
import { TransformStream } from 'web-streams-polyfill';
import { TextDecoder } from './text-decoder';

export interface TextDecoderStreamOptions {
	fatal?: boolean;
	ignoreBOM?: boolean;
}

/**
 * `TextDecoderStream` — the streaming counterpart to {@link TextDecoder}: a
 * `TransformStream` whose writable side accepts binary chunks and whose
 * readable side emits decoded strings. It's a standard Web API
 * (`ReadableStream.pipeThrough(new TextDecoderStream())`, and the common Web
 * Serial idiom `port.readable.pipeTo(new TextDecoderStream().writable)`).
 *
 * It was previously **absent** from the runtime, so `new TextDecoderStream()`
 * threw a `ReferenceError`. When that construction happens inside a
 * fire-and-forget async reader (as Web Serial apps typically write it), the
 * rejection is unhandled — and the runtime's unhandled-rejection handler halts
 * the frame loop, freezing the whole app right after the port opens.
 *
 * The underlying {@link TextDecoder} does not yet preserve partial multi-byte
 * sequences across `decode(..., {stream:true})` calls, so a code point split
 * across two chunks can still emit U+FFFD; that limitation lives in
 * `TextDecoder`, not here. ASCII / whole-line protocols (the Web Serial common
 * case) are unaffected.
 *
 * @see https://developer.mozilla.org/docs/Web/API/TextDecoderStream
 */
export class TextDecoderStream {
	#decoder: TextDecoder;
	#transform: TransformStream<BufferSource, string>;

	constructor(label: string = 'utf-8', options: TextDecoderStreamOptions = {}) {
		// Construct the decoder first: an invalid encoding label must throw a
		// RangeError from the constructor (matching the browser), before any
		// stream is created.
		const decoder = new TextDecoder(label, options);
		this.#decoder = decoder;
		this.#transform = new TransformStream<BufferSource, string>({
			transform(chunk, controller) {
				const text = decoder.decode(chunk, { stream: true });
				if (text) controller.enqueue(text);
			},
			flush(controller) {
				// Final call with no `stream` flag flushes any trailing state.
				const text = decoder.decode();
				if (text) controller.enqueue(text);
			},
		});
	}

	get encoding(): string {
		return this.#decoder.encoding;
	}
	get fatal(): boolean {
		return this.#decoder.fatal;
	}
	get ignoreBOM(): boolean {
		return this.#decoder.ignoreBOM;
	}
	get readable() {
		return this.#transform.readable;
	}
	get writable() {
		return this.#transform.writable;
	}
}
def(TextDecoderStream, 'TextDecoderStream');
