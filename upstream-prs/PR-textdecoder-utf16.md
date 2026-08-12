# PR-textdecoder-utf16 — Add UTF-16LE / UTF-16BE decoding to `TextDecoder`

**Branch:** `upstream-pr/textdecoder-utf16`
**Base:** `upstream/main`
**Local base:** `8981cc6`
**Draft status.** Implemented + confirmed on real hardware (unblocked Unity/Emscripten WebGL boot — see Testing). Captures the change so the PR can be assembled on demand.

## What's in the commit

```
 packages/runtime/src/polyfills/text-decoder.ts | ~55 +
 1 file changed
```

One file, additive. No new native binding.

## PR title (suggested)

`runtime: support UTF-16LE/UTF-16BE in TextDecoder`

## Motivation

`TextDecoder` today throws for any label other than `utf-8`/`utf8`:

```ts
if (typeof encoding === 'string' && encoding !== 'utf-8' && encoding !== 'utf8') {
	throw new TypeError('Only "utf-8" decoding is supported');
}
```

Emscripten-compiled runtimes need UTF-16. In particular, Unity's IL2CPP
marshals C# `System.String` (UTF-16) back to JS via
`new TextDecoder('utf-16le')` inside `UTF16ToString` during framework
init — so **every** Unity WebGL build (and any Emscripten module that
touches a UTF-16 string) throws `Only "utf-8" decoding is supported`
at load and never boots.

## The change

1. Constructor normalizes the label (trim + lowercase) and accepts the
   UTF-16 families in addition to UTF-8; anything else still throws
   (now a `RangeError`, matching browsers):

   ```ts
   const label = String(encoding).trim().toLowerCase();
   if (label === 'utf-8' || label === 'utf8' || label === 'unicode-1-1-utf-8') {
       this.encoding = 'utf-8';
   } else if (label === 'utf-16le' || label === 'utf-16' ||
              label === 'unicode' || label === 'csunicode' || label === 'unicodefeff') {
       this.encoding = 'utf-16le';
   } else if (label === 'utf-16be' || label === 'unicodefffe') {
       this.encoding = 'utf-16be';
   } else {
       throw new RangeError(`Failed to construct 'TextDecoder': The encoding label provided ('${encoding}') is invalid.`);
   }
   ```

2. `decode()` keeps the existing UTF-8 fast path untouched; a one-line
   branch routes UTF-16 to a new `decodeUtf16(bytes, bigEndian)` helper:

   ```ts
   if (this.encoding === 'utf-16le' || this.encoding === 'utf-16be') {
       return this.decodeUtf16(bytes, this.encoding === 'utf-16be');
   }
   ```

3. `decodeUtf16` reads 2 bytes per code unit (LE or BE), strips a leading
   BOM (U+FEFF) unless `ignoreBOM`, emits U+FFFD for a dangling odd byte
   (or throws when `fatal`), and builds the string with chunked
   `String.fromCharCode.apply` to stay under the argument-count cap on
   large inputs. Surrogate pairs pass through unchanged (they're already
   two code units in the byte stream).

## Behavior / correctness

Validated byte-for-byte against Node's (ada/ICU-backed) reference
`TextDecoder` across: ASCII, multi-byte BMP, non-BMP (emoji → surrogate
pairs), both endiannesses, BOM-strip, and odd-trailing-byte → U+FFFD.
No change to the UTF-8 path.

## Gotcha (worth calling out in review)

`String.fromCharCode.apply(null, units)` with a very large `units` array
overflows the argument count on some engines; the helper chunks at 0x8000
code units. The BOM check happens on the **decoded** code unit
(`unit === 0xFEFF`) so it's endianness-correct for both LE (`FF FE`) and
BE (`FE FF`) byte order.

## Testing

Real hardware (Switch, Tegra): with this change the Unity `2022.3.62f3-webgl2-debug`
build gets past the `new TextDecoder('utf-16le')` call in its framework
init that previously threw and aborted the load. Node-side unit parity
against the reference decoder (above).
