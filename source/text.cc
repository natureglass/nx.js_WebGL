#include "error.h"
#include "types.h"
#include "util.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Native TextDecoder.decode(). Replaces the JS polyfill's per-byte
// `String.fromCharCode.apply(null, chunk)` UTF-8/UTF-16 path, which made V8
// allocate ~1 Local handle PER BYTE of the input. Decoding a multi-MB body that
// way grows V8's HandleScope block reserve by ~14 MiB per 1.9 MiB decoded, and
// V8 never returns those reserved blocks to the native heap -> a slow native
// OOM even though the JS heap stays flat (see project_brewser_native_oom_probe).
//
// Here the whole buffer is decoded in C++ into a SINGLE v8::String (one
// allocation, zero per-char handles), so no HandleScope growth occurs.
//
// The JS constructor (packages/runtime/src/polyfills/text-decoder.ts) already
// normalizes the WHATWG encoding label to exactly one of "utf-8", "utf-16le",
// or "utf-16be" and throws RangeError for anything else, so this binding only
// has to handle those three. It matches the polyfill's option semantics:
// `fatal` (throw instead of emitting U+FFFD) and `ignoreBOM` (keep the leading
// BOM instead of stripping it). `stream` is unsupported, exactly as before.

using namespace v8;

namespace {

// Strict WHATWG-conformant UTF-8 validation, used only for `fatal: true`. In
// non-fatal mode V8's own decoder performs standard U+FFFD substitution, so we
// hand the bytes straight to String::NewFromUtf8. Rejects truncated sequences,
// stray continuation bytes, invalid lead bytes, overlong encodings, UTF-16
// surrogate code points, and code points above U+10FFFF.
bool utf8_is_valid(const uint8_t *s, size_t n) {
	size_t i = 0;
	while (i < n) {
		uint8_t b = s[i];
		if (b < 0x80) { // ASCII
			i++;
			continue;
		}
		size_t extra;   // number of trailing continuation bytes
		uint32_t cp;    // accumulated code point
		uint32_t min;   // smallest code point legal for this length (overlong check)
		if ((b & 0xe0) == 0xc0) {
			extra = 1;
			cp = b & 0x1f;
			min = 0x80;
		} else if ((b & 0xf0) == 0xe0) {
			extra = 2;
			cp = b & 0x0f;
			min = 0x800;
		} else if ((b & 0xf8) == 0xf0) {
			extra = 3;
			cp = b & 0x07;
			min = 0x10000;
		} else {
			return false; // 0x80-0xBF stray continuation, or 0xF8-0xFF
		}
		if (i + extra >= n)
			return false; // truncated: not enough continuation bytes remain
		for (size_t k = 1; k <= extra; k++) {
			uint8_t c = s[i + k];
			if ((c & 0xc0) != 0x80)
				return false; // not a continuation byte
			cp = (cp << 6) | (c & 0x3f);
		}
		if (cp < min)
			return false; // overlong encoding
		if (cp > 0x10ffff)
			return false; // beyond Unicode
		if (cp >= 0xd800 && cp <= 0xdfff)
			return false; // surrogate half is not a valid UTF-8 scalar
		i += extra + 1;
	}
	return true;
}

// $.textDecode(bytes, encoding, fatal, ignoreBOM) -> string
void nx_text_decode(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();

	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[0]);
	if (!buf || size == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}

	String::Utf8Value enc(iso, info[1]);
	const char *encoding = *enc ? *enc : "utf-8";
	bool fatal = info.Length() > 2 && info[2]->BooleanValue(iso);
	bool ignore_bom = info.Length() > 3 && info[3]->BooleanValue(iso);

	if (!strcmp(encoding, "utf-16le") || !strcmp(encoding, "utf-16be")) {
		bool big_endian = !strcmp(encoding, "utf-16be");
		const uint8_t *data = buf;
		size_t len = size;

		// Strip a leading BOM (U+FEFF): FF FE for LE, FE FF for BE.
		if (!ignore_bom && len >= 2) {
			if (!big_endian && data[0] == 0xff && data[1] == 0xfe) {
				data += 2;
				len -= 2;
			} else if (big_endian && data[0] == 0xfe && data[1] == 0xff) {
				data += 2;
				len -= 2;
			}
		}

		size_t n_units = len / 2;
		bool odd = (len & 1) != 0;
		if (odd && fatal) {
			iso->ThrowException(
			    Exception::TypeError(nx_str(iso, "Invalid UTF-16 sequence")));
			return;
		}

		// Build a transient uint16_t array by reading bytes individually: the
		// view pointer may be unaligned on ARM, so a raw uint16_t* cast would be
		// UB. This C++ copy creates no V8 handles (unlike fromCharCode.apply).
		size_t out_units = n_units + (odd ? 1 : 0);
		uint16_t *units =
		    (uint16_t *)nx_alloc(iso, (out_units ? out_units : 1) * sizeof(uint16_t));
		if (!units)
			return; // nx_alloc scheduled a RangeError
		for (size_t j = 0; j < n_units; j++) {
			uint8_t lo_byte = data[2 * j];
			uint8_t hi_byte = data[2 * j + 1];
			units[j] = big_endian ? (uint16_t)((lo_byte << 8) | hi_byte)
			                      : (uint16_t)(lo_byte | (hi_byte << 8));
		}
		if (odd)
			units[n_units] = 0xfffd; // dangling odd byte -> replacement char

		Local<String> out;
		bool ok = String::NewFromTwoByte(iso, units, NewStringType::kNormal,
		                                 (int)out_units)
		              .ToLocal(&out);
		free(units);
		if (!ok) {
			iso->ThrowException(
			    Exception::RangeError(nx_str(iso, "Decoded string is too long")));
			return;
		}
		info.GetReturnValue().Set(out);
		return;
	}

	// UTF-8 (default). Strip a leading EF BB BF BOM unless ignoreBOM.
	const uint8_t *data = buf;
	size_t len = size;
	if (!ignore_bom && len >= 3 && data[0] == 0xef && data[1] == 0xbb &&
	    data[2] == 0xbf) {
		data += 3;
		len -= 3;
	}

	if (len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}

	if (fatal && !utf8_is_valid(data, len)) {
		iso->ThrowException(
		    Exception::TypeError(nx_str(iso, "Invalid UTF-8 sequence")));
		return;
	}

	if (len > (size_t)INT_MAX) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Decoded string is too long")));
		return;
	}

	Local<String> out;
	// Non-fatal: V8's decoder performs standard WHATWG U+FFFD substitution for
	// malformed input. Fatal input has already been validated above.
	if (!String::NewFromUtf8(iso, (const char *)data, NewStringType::kNormal,
	                         (int)len)
	         .ToLocal(&out)) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Decoded string is too long")));
		return;
	}
	info.GetReturnValue().Set(out);
}

} // namespace

void nx_init_text(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "textDecode", nx_text_decode);
}
