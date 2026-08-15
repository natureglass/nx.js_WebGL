import { $ } from '../$';
import { def } from '../utils';

export interface TextDecodeOptions {
	stream?: boolean;
}

/**
 * The `TextDecoder` interface represents a decoder for a specific text encoding.
 * The implementation in nx.js only supports `"utf-8"` decoding.
 *
 * If you need to decode binary data of a different encoding, consider importing
 * a more full-featured polyfill, such as [`@kayahr/text-encoding`](https://www.npmjs.com/package/@kayahr/text-encoding).
 *
 * Copyright: Apache License 2.0
 * @author Sam Thorogood
 * @see https://github.com/samthor/fast-text-encoding/blob/master/src/lowlevel.js
 */
export class TextDecoder implements globalThis.TextDecoder {
	encoding: string;
	fatal: boolean;
	ignoreBOM: boolean;
	constructor(
		encoding: string = 'utf-8',
		options?: { fatal?: boolean; ignoreBOM?: boolean },
	) {
		// WHATWG encoding labels are case-insensitive and whitespace-
		// trimmed. We support UTF-8 (the original nx.js capability) plus
		// UTF-16LE / UTF-16BE. UTF-16 matters for Emscripten-compiled
		// runtimes: Unity's IL2CPP marshals C# `System.String` (UTF-16)
		// back to JS via `new TextDecoder('utf-16le')` in `UTF16ToString`,
		// so without it a Unity WebGL build throws at framework init
		// ("Only utf-8 decoding is supported"). Anything else still
		// throws (RangeError, matching the browser).
		const label = String(encoding).trim().toLowerCase();
		if (label === 'utf-8' || label === 'utf8' || label === 'unicode-1-1-utf-8') {
			this.encoding = 'utf-8';
		} else if (
			label === 'utf-16le' || label === 'utf-16' ||
			label === 'unicode' || label === 'csunicode' || label === 'unicodefeff'
		) {
			this.encoding = 'utf-16le';
		} else if (label === 'utf-16be' || label === 'unicodefffe') {
			this.encoding = 'utf-16be';
		} else {
			throw new RangeError(
				`Failed to construct 'TextDecoder': The encoding label provided ('${encoding}') is invalid.`,
			);
		}
		this.fatal = options?.fatal ?? false;
		this.ignoreBOM = options?.ignoreBOM ?? false;
	}

	/** Decode a UTF-16 byte stream (little- or big-endian). Companion to
	 * the UTF-8 fast path in `decode()`; kept separate so the UTF-8 code
	 * is untouched. Strips a leading BOM (U+FEFF) unless `ignoreBOM`, and
	 * emits U+FFFD for a dangling odd byte (throws when `fatal`). Chunked
	 * `String.fromCharCode.apply` to stay under the argument-count cap on
	 * large inputs. */
	private decodeUtf16(bytes: Uint8Array, bigEndian: boolean): string {
		const len = bytes.length;
		let result = '';
		const CHUNK = 0x8000;
		const units: number[] = [];
		let checkBom = !this.ignoreBOM;
		let i = 0;
		for (; i + 1 < len; i += 2) {
			const unit = bigEndian
				? (bytes[i] << 8) | bytes[i + 1]
				: bytes[i] | (bytes[i + 1] << 8);
			if (checkBom) {
				checkBom = false;
				if (unit === 0xfeff) continue; // strip BOM
			}
			units.push(unit);
			if (units.length >= CHUNK) {
				result += String.fromCharCode.apply(null, units);
				units.length = 0;
			}
		}
		if (i < len) {
			// Odd trailing byte — incomplete code unit.
			if (this.fatal) throw new TypeError('Invalid UTF-16 sequence');
			units.push(0xfffd);
		}
		if (units.length) result += String.fromCharCode.apply(null, units);
		return result;
	}

	/**
	 * Decodes a BufferSource into a string using the specified encoding.
	 * If no input is provided, an empty string is returned.
	 *
	 * **Note:** Currently the `stream` option is not supported.
	 * // TODO: Implement `stream` option to preserve incomplete multi-byte sequences across calls.
	 *
	 * @param input The BufferSource to decode.
	 * @param options The options for decoding.
	 * @returns The decoded string.
	 */
	decode(input?: BufferSource, options?: TextDecodeOptions): string {
		if (!input) return '';
		let bytes;
		if (input instanceof ArrayBuffer) {
			bytes = new Uint8Array(input);
		} else {
			bytes = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
		}

		// Native fast path: decode the whole buffer into ONE string in C++.
		// The JS decoders below build the output via `String.fromCharCode.apply`
		// over the input, which makes V8 allocate ~1 Local handle PER BYTE; on a
		// multi-MB body that grows V8's HandleScope block reserve by ~14 MiB per
		// 1.9 MiB decoded and never returns it to the native heap -> eventual
		// native OOM even while the JS heap stays flat (see
		// project_brewser_native_oom_probe.md). The native binding produces a
		// single string with no per-byte handles. Guarded by a capability check
		// so an older engine without the binding still works via the JS fallback
		// below. A `fatal` decode error is thrown as a TypeError from native and
		// propagates unchanged (no catch-and-fallback — that would mask it).
		if (typeof $.textDecode === 'function') {
			return $.textDecode(bytes, this.encoding, this.fatal, this.ignoreBOM);
		}

		// UTF-16 takes a dedicated path; the rest of this method is the
		// original UTF-8 decoder, untouched.
		if (this.encoding === 'utf-16le' || this.encoding === 'utf-16be') {
			return this.decodeUtf16(bytes, this.encoding === 'utf-16be');
		}

		var inputIndex = 0;

		// Create a working buffer for UTF-16 code points, but don't generate one
		// which is too large for small input sizes. UTF-8 to UCS-16 conversion is
		// going to be at most 1:1, if all code points are ASCII. The other extreme
		// is 4-byte UTF-8, which results in two UCS-16 points, but this is still 50%
		// fewer entries in the output.
		var pendingSize = Math.min(256 * 256, bytes.length + 1);
		var pending = new Uint16Array(pendingSize);
		var chunks = [];
		var pendingIndex = 0;
		var isFirstChunk = true;

		for (;;) {
			var more = inputIndex < bytes.length;

			// If there's no more data or there'd be no room for two UTF-16 values,
			// create a chunk. This isn't done at the end by simply slicing the data
			// into equal sized chunks as we might hit a surrogate pair.
			if (!more || pendingIndex >= pendingSize - 1) {
				// nb. .apply and friends are *really slow*. Low-hanging fruit is to
				// expand this to literally pass pending[0], pending[1], ... etc, but
				// the output code expands pretty fast in this case.
				// These extra vars get compiled out: they're just to make TS happy.
				// Turns out you can pass an ArrayLike to .apply().
				var subarray = pending.subarray(0, pendingIndex);
				// @ts-expect-error
				var chunk = String.fromCharCode.apply(null, subarray);

				// Strip BOM from the beginning of the output if ignoreBOM is false (default)
				if (
					isFirstChunk &&
					!this.ignoreBOM &&
					chunk.length > 0 &&
					chunk.charCodeAt(0) === 0xfeff
				) {
					chunk = chunk.slice(1);
				}
				isFirstChunk = false;

				chunks.push(chunk);

				if (!more) {
					return chunks.join('');
				}

				// Move the buffer forward and create another chunk.
				bytes = bytes.subarray(inputIndex);
				inputIndex = 0;
				pendingIndex = 0;
			}

			var byte1 = bytes[inputIndex++];
			if ((byte1 & 0x80) === 0) {
				// 1-byte or null
				pending[pendingIndex++] = byte1;
			} else if ((byte1 & 0xe0) === 0xc0) {
				// 2-byte
				var byte2 = bytes[inputIndex++];
				if (byte2 === undefined || (byte2 & 0xc0) !== 0x80) {
					if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
					pending[pendingIndex++] = 0xfffd;
					if (byte2 !== undefined) inputIndex--;
				} else {
					pending[pendingIndex++] = ((byte1 & 0x1f) << 6) | (byte2 & 0x3f);
				}
			} else if ((byte1 & 0xf0) === 0xe0) {
				// 3-byte
				var byte2 = bytes[inputIndex++];
				if (byte2 === undefined || (byte2 & 0xc0) !== 0x80) {
					if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
					pending[pendingIndex++] = 0xfffd;
					if (byte2 !== undefined) inputIndex--;
				} else {
					var byte3 = bytes[inputIndex++];
					if (byte3 === undefined || (byte3 & 0xc0) !== 0x80) {
						if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
						pending[pendingIndex++] = 0xfffd;
						if (byte3 !== undefined) inputIndex--;
					} else {
						pending[pendingIndex++] =
							((byte1 & 0x0f) << 12) | ((byte2 & 0x3f) << 6) | (byte3 & 0x3f);
					}
				}
			} else if ((byte1 & 0xf8) === 0xf0) {
				// 4-byte
				var byte2 = bytes[inputIndex++];
				if (byte2 === undefined || (byte2 & 0xc0) !== 0x80) {
					if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
					pending[pendingIndex++] = 0xfffd;
					if (byte2 !== undefined) inputIndex--;
				} else {
					var byte3 = bytes[inputIndex++];
					if (byte3 === undefined || (byte3 & 0xc0) !== 0x80) {
						if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
						pending[pendingIndex++] = 0xfffd;
						if (byte3 !== undefined) inputIndex--;
					} else {
						var byte4 = bytes[inputIndex++];
						if (byte4 === undefined || (byte4 & 0xc0) !== 0x80) {
							if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
							pending[pendingIndex++] = 0xfffd;
							if (byte4 !== undefined) inputIndex--;
						} else {
							// this can be > 0xffff, so possibly generate surrogates
							var codepoint =
								((byte1 & 0x07) << 0x12) |
								((byte2 & 0x3f) << 0x0c) |
								((byte3 & 0x3f) << 0x06) |
								(byte4 & 0x3f);
							if (codepoint > 0xffff) {
								codepoint -= 0x10000;
								pending[pendingIndex++] = ((codepoint >>> 10) & 0x3ff) | 0xd800;
								codepoint = 0xdc00 | (codepoint & 0x3ff);
							}
							pending[pendingIndex++] = codepoint;
						}
					}
				}
			} else {
				// invalid initial byte
				if (this.fatal) throw new TypeError('Invalid UTF-8 sequence');
				pending[pendingIndex++] = 0xfffd;
			}
		}
	}
}
def(TextDecoder, 'TextDecoder');

export const decoder = new TextDecoder();
