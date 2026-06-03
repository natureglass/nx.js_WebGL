/**
 * Worker bootstrap script — emitted as a JS source string that runs in
 * BOTH the main JS context (eval'd at module init to obtain
 * `_scSerialize` / `_scDeserialize` for outbound postMessage encoding)
 * AND in every freshly-spawned worker context (prepended to the user's
 * worker source before `JS_Eval`).
 *
 * Single source of truth — no separate main-vs-worker copies that could
 * drift. The wire format is a TLV binary stream stored in an
 * ArrayBuffer; the C-side message queue carries the raw bytes opaquely.
 *
 * Scope (Tier-1 Pass A):
 *   primitives (undefined/null/bool/number/string), Date, ArrayBuffer,
 *   TypedArrays (all 9), DataView, Map, Set, Array, plain Object, Error.
 *
 * Out of scope (Tier-1.5+): BigInt, RegExp, Blob/File, ImageData,
 *   ImageBitmap, cyclic references, ArrayBuffer transfer optimisation.
 *   Throwing on these is preferable to silent data loss.
 *
 * Worker-side responsibilities also installed here:
 *   - `self` alias for globalThis
 *   - `self.postMessage(val)` → serialize → native `__postBytes`
 *   - `__handleInbound(rawBuf)` → deserialize → fire `self.onmessage`
 *   - `__handleError(msgStr)` reserved (worker rarely needs this)
 *
 * Identical code runs main-side; the main thread's worker.ts eval's
 * this string in a private scope and pulls the two functions out.
 */
export const WORKER_BOOTSTRAP_JS = `
(function (g) {
'use strict';

// TextEncoder / TextDecoder — minimal UTF-8 implementations. The worker
// context starts from a bare QuickJS runtime that does NOT include
// these; nx.js's main runtime polyfills them but we can't import the
// runtime bundle into a worker context. Install only if missing so we
// don't shadow native versions on the main side (where this same
// bootstrap is eval'd to bind _scSerialize / _scDeserialize). UTF-8
// only — no encoding labels, no fatal mode, no streaming.
if (typeof g.TextEncoder !== 'function') {
  g.TextEncoder = function TextEncoder() { this.encoding = 'utf-8'; };
  g.TextEncoder.prototype.encode = function (input) {
    if (!input) return new Uint8Array(0);
    var pos = 0, len = input.length, at = 0;
    var target = new Uint8Array(Math.max(32, len * 3));
    while (pos < len) {
      var value = input.charCodeAt(pos++);
      if (value >= 0xd800 && value <= 0xdbff && pos < len) {
        var extra = input.charCodeAt(pos);
        if ((extra & 0xfc00) === 0xdc00) {
          ++pos;
          value = ((value & 0x3ff) << 10) + (extra & 0x3ff) + 0x10000;
        } else value = 0xfffd;
      } else if (value >= 0xdc00 && value <= 0xdfff) value = 0xfffd;
      if (value < 0x80) { target[at++] = value; continue; }
      else if (value < 0x800) { target[at++] = (value >>> 6) | 0xc0; }
      else if (value < 0x10000) {
        target[at++] = (value >>> 12) | 0xe0;
        target[at++] = ((value >>> 6) & 0x3f) | 0x80;
      } else {
        target[at++] = (value >>> 18) | 0xf0;
        target[at++] = ((value >>> 12) & 0x3f) | 0x80;
        target[at++] = ((value >>> 6) & 0x3f) | 0x80;
      }
      target[at++] = (value & 0x3f) | 0x80;
    }
    return target.slice(0, at);
  };
}
if (typeof g.TextDecoder !== 'function') {
  g.TextDecoder = function TextDecoder(label) { this.encoding = (label || 'utf-8'); };
  g.TextDecoder.prototype.decode = function (input) {
    if (!input) return '';
    var u = input instanceof Uint8Array
      ? input
      : (input.buffer ? new Uint8Array(input.buffer, input.byteOffset || 0, input.byteLength)
                      : new Uint8Array(input));
    var out = '', i = 0, n = u.length;
    while (i < n) {
      var b1 = u[i++];
      if (b1 < 0x80) { out += String.fromCharCode(b1); continue; }
      var b2, b3, b4, cp;
      if ((b1 & 0xe0) === 0xc0) { b2 = u[i++]; cp = ((b1 & 0x1f) << 6) | (b2 & 0x3f); }
      else if ((b1 & 0xf0) === 0xe0) {
        b2 = u[i++]; b3 = u[i++];
        cp = ((b1 & 0x0f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
      } else if ((b1 & 0xf8) === 0xf0) {
        b2 = u[i++]; b3 = u[i++]; b4 = u[i++];
        cp = ((b1 & 0x07) << 18) | ((b2 & 0x3f) << 12) | ((b3 & 0x3f) << 6) | (b4 & 0x3f);
      } else { cp = 0xfffd; }
      if (cp > 0xffff) {
        cp -= 0x10000;
        out += String.fromCharCode(0xd800 + (cp >>> 10), 0xdc00 + (cp & 0x3ff));
      } else out += String.fromCharCode(cp);
    }
    return out;
  };
}

var TAG_UNDEFINED = 0x01, TAG_NULL = 0x02, TAG_FALSE = 0x03, TAG_TRUE = 0x04;
var TAG_NUMBER = 0x05, TAG_STRING = 0x06;
var TAG_ARRAY = 0x07, TAG_OBJECT = 0x08;
var TAG_DATE = 0x09;
var TAG_ARRAYBUFFER = 0x0A;
var TAG_UINT8ARRAY = 0x0B, TAG_INT8ARRAY = 0x0C, TAG_UINT8CLAMPEDARRAY = 0x0D;
var TAG_UINT16ARRAY = 0x0E, TAG_INT16ARRAY = 0x0F;
var TAG_UINT32ARRAY = 0x10, TAG_INT32ARRAY = 0x11;
var TAG_FLOAT32ARRAY = 0x12, TAG_FLOAT64ARRAY = 0x13;
var TAG_MAP = 0x14, TAG_SET = 0x15;
var TAG_ERROR = 0x16, TAG_DATAVIEW = 0x17;
// Pass F: transferred ArrayBuffer — wire payload is just u32 index
// into the side-channel transfer table. Bytes travel via msg->transfers
// (C-side), not inline.
var TAG_TRANSFERRED_AB = 0x18;

function Writer() {
  this.buf = new Uint8Array(256);
  this.view = new DataView(this.buf.buffer);
  this.pos = 0;
}
Writer.prototype.grow = function (n) {
  if (this.pos + n <= this.buf.length) return;
  var next = this.buf.length;
  while (next < this.pos + n) next *= 2;
  var nb = new Uint8Array(next);
  nb.set(this.buf.subarray(0, this.pos));
  this.buf = nb;
  this.view = new DataView(nb.buffer);
};
Writer.prototype.u8 = function (v) { this.grow(1); this.buf[this.pos++] = v & 0xff; };
Writer.prototype.u32 = function (v) { this.grow(4); this.view.setUint32(this.pos, v >>> 0, true); this.pos += 4; };
Writer.prototype.f64 = function (v) { this.grow(8); this.view.setFloat64(this.pos, v, true); this.pos += 8; };
Writer.prototype.bytes = function (src) {
  this.grow(src.length);
  this.buf.set(src, this.pos);
  this.pos += src.length;
};
Writer.prototype.str = function (s) {
  var enc = new TextEncoder();
  var b = enc.encode(s);
  this.u32(b.length);
  this.bytes(b);
};
Writer.prototype.done = function () { return this.buf.buffer.slice(0, this.pos); };

function Reader(ab) {
  this.buf = new Uint8Array(ab);
  this.view = new DataView(ab);
  this.pos = 0;
}
Reader.prototype.u8 = function () { return this.buf[this.pos++]; };
Reader.prototype.u32 = function () { var v = this.view.getUint32(this.pos, true); this.pos += 4; return v; };
Reader.prototype.f64 = function () { var v = this.view.getFloat64(this.pos, true); this.pos += 8; return v; };
Reader.prototype.bytes = function (n) {
  // Slice copies — needed because the receiver may detach this buffer
  // (deserialize creates new ArrayBuffers that the caller owns).
  var out = this.buf.slice(this.pos, this.pos + n);
  this.pos += n;
  return out;
};
Reader.prototype.str = function () {
  var n = this.u32();
  var dec = new TextDecoder();
  var s = dec.decode(this.buf.subarray(this.pos, this.pos + n));
  this.pos += n;
  return s;
};

function write(w, v) {
  if (v === undefined) { w.u8(TAG_UNDEFINED); return; }
  if (v === null) { w.u8(TAG_NULL); return; }
  if (v === false) { w.u8(TAG_FALSE); return; }
  if (v === true) { w.u8(TAG_TRUE); return; }
  var t = typeof v;
  if (t === 'number') { w.u8(TAG_NUMBER); w.f64(v); return; }
  if (t === 'string') { w.u8(TAG_STRING); w.str(v); return; }
  if (v instanceof Date) { w.u8(TAG_DATE); w.f64(v.getTime()); return; }
  if (v instanceof ArrayBuffer) {
    // Pass F: if this AB is in the current transfer list, emit a
    // TAG_TRANSFERRED_AB tag with the side-channel index instead of
    // inline bytes. The C layer carries the bytes via msg->transfers.
    if (w.transferSet && w.transferSet.has(v)) {
      var idx = w.transferList.indexOf(v);
      if (idx < 0) { idx = w.transferList.length; w.transferList.push(v); }
      w.u8(TAG_TRANSFERRED_AB); w.u32(idx);
      return;
    }
    w.u8(TAG_ARRAYBUFFER); w.u32(v.byteLength);
    w.bytes(new Uint8Array(v));
    return;
  }
  if (ArrayBuffer.isView(v)) {
    var tag;
    if (v instanceof DataView) tag = TAG_DATAVIEW;
    else if (v instanceof Uint8Array) tag = TAG_UINT8ARRAY;
    else if (v instanceof Int8Array) tag = TAG_INT8ARRAY;
    else if (typeof Uint8ClampedArray !== 'undefined' && v instanceof Uint8ClampedArray) tag = TAG_UINT8CLAMPEDARRAY;
    else if (v instanceof Uint16Array) tag = TAG_UINT16ARRAY;
    else if (v instanceof Int16Array) tag = TAG_INT16ARRAY;
    else if (v instanceof Uint32Array) tag = TAG_UINT32ARRAY;
    else if (v instanceof Int32Array) tag = TAG_INT32ARRAY;
    else if (v instanceof Float32Array) tag = TAG_FLOAT32ARRAY;
    else if (v instanceof Float64Array) tag = TAG_FLOAT64ARRAY;
    else throw new Error('unsupported typed-array kind in structured clone');
    w.u8(tag); w.u32(v.byteLength);
    w.bytes(new Uint8Array(v.buffer, v.byteOffset, v.byteLength));
    return;
  }
  if (v instanceof Map) {
    w.u8(TAG_MAP); w.u32(v.size);
    var it = v.entries(); var e;
    while (!(e = it.next()).done) { write(w, e.value[0]); write(w, e.value[1]); }
    return;
  }
  if (v instanceof Set) {
    w.u8(TAG_SET); w.u32(v.size);
    var it2 = v.values(); var e2;
    while (!(e2 = it2.next()).done) write(w, e2.value);
    return;
  }
  if (Array.isArray(v)) {
    w.u8(TAG_ARRAY); w.u32(v.length);
    for (var i = 0; i < v.length; i++) write(w, v[i]);
    return;
  }
  if (v instanceof Error) {
    w.u8(TAG_ERROR);
    w.str(v.name || 'Error');
    w.str(v.message || '');
    return;
  }
  if (t === 'object') {
    var keys = Object.keys(v);
    w.u8(TAG_OBJECT); w.u32(keys.length);
    for (var j = 0; j < keys.length; j++) { w.str(keys[j]); write(w, v[keys[j]]); }
    return;
  }
  throw new Error('cannot structured-clone value of type ' + t);
}

function read(r) {
  var tag = r.u8();
  switch (tag) {
    case TAG_UNDEFINED: return undefined;
    case TAG_NULL: return null;
    case TAG_FALSE: return false;
    case TAG_TRUE: return true;
    case TAG_NUMBER: return r.f64();
    case TAG_STRING: return r.str();
    case TAG_DATE: return new Date(r.f64());
    case TAG_ARRAYBUFFER: {
      var len = r.u32();
      return r.bytes(len).buffer;
    }
    case TAG_UINT8ARRAY:
    case TAG_INT8ARRAY:
    case TAG_UINT8CLAMPEDARRAY:
    case TAG_UINT16ARRAY:
    case TAG_INT16ARRAY:
    case TAG_UINT32ARRAY:
    case TAG_INT32ARRAY:
    case TAG_FLOAT32ARRAY:
    case TAG_FLOAT64ARRAY: {
      var byteLen = r.u32();
      var ab = r.bytes(byteLen).buffer;
      switch (tag) {
        case TAG_UINT8ARRAY: return new Uint8Array(ab);
        case TAG_INT8ARRAY: return new Int8Array(ab);
        case TAG_UINT8CLAMPEDARRAY: return new Uint8ClampedArray(ab);
        case TAG_UINT16ARRAY: return new Uint16Array(ab);
        case TAG_INT16ARRAY: return new Int16Array(ab);
        case TAG_UINT32ARRAY: return new Uint32Array(ab);
        case TAG_INT32ARRAY: return new Int32Array(ab);
        case TAG_FLOAT32ARRAY: return new Float32Array(ab);
        case TAG_FLOAT64ARRAY: return new Float64Array(ab);
      }
      return null;
    }
    case TAG_DATAVIEW: {
      var dvLen = r.u32();
      return new DataView(r.bytes(dvLen).buffer);
    }
    case TAG_TRANSFERRED_AB: {
      var tIdx = r.u32();
      if (!r.transferABs || tIdx >= r.transferABs.length) {
        throw new Error('structured-clone: TAG_TRANSFERRED_AB index ' + tIdx + ' out of range');
      }
      return r.transferABs[tIdx];
    }
    case TAG_MAP: {
      var nm = r.u32(); var m = new Map();
      for (var i = 0; i < nm; i++) { var k = read(r); var v = read(r); m.set(k, v); }
      return m;
    }
    case TAG_SET: {
      var ns = r.u32(); var s = new Set();
      for (var i2 = 0; i2 < ns; i2++) s.add(read(r));
      return s;
    }
    case TAG_ARRAY: {
      var na = r.u32(); var a = new Array(na);
      for (var i3 = 0; i3 < na; i3++) a[i3] = read(r);
      return a;
    }
    case TAG_OBJECT: {
      var no = r.u32(); var o = {};
      for (var i4 = 0; i4 < no; i4++) { var key = r.str(); o[key] = read(r); }
      return o;
    }
    case TAG_ERROR: {
      var name = r.str(); var msg = r.str();
      var e = new Error(msg); e.name = name;
      return e;
    }
    default: throw new Error('structured-clone: unknown tag ' + tag);
  }
}

// Pass F: optional second arg \`transferSet\` (a Set of ArrayBuffers that
// the caller wants transferred). When present, returns { ab, transfers }
// where \`transfers\` is the list of ABs to hand to the C layer in the
// side-channel. Without it, returns just the wire ArrayBuffer for full
// back-compat with Pass A callers.
g._scSerialize = function (val, transferSet) {
  var w = new Writer();
  if (transferSet) {
    w.transferSet = transferSet;
    w.transferList = [];
  } else {
    w.transferSet = null;
    w.transferList = null;
  }
  write(w, val);
  if (transferSet) {
    return { ab: w.done(), transfers: w.transferList };
  }
  return w.done();
};
// Pass F: optional \`transferABs\` is a JS array of receiver-owned
// ArrayBuffers built C-side from the side-channel. TAG_TRANSFERRED_AB
// tags resolve to entries in this array.
g._scDeserialize = function (ab, transferABs) {
  var r = new Reader(ab);
  r.transferABs = transferABs || null;
  return read(r);
};

// === Worker-side glue (only takes effect inside the worker context). ===
// The presence of \`__postBytes\` (registered natively from worker.c on
// the worker's JSContext) is what tells us we're INSIDE a worker.
if (typeof g.__postBytes === 'function') {
  g.self = g;
  // Pass F: optional \`transfer\` is an array of ArrayBuffers to hand off
  // by ownership. Each is bytes-copied to a process-wide malloc on the
  // C side, then detached so this context can't reuse them. The C side
  // receives mainAB + transferABs[] and routes the bytes via side-channel.
  g.postMessage = function (val, transfer) {
    if (transfer && transfer.length) {
      var set = new Set();
      for (var i = 0; i < transfer.length; i++) {
        var t = transfer[i];
        if (!(t instanceof ArrayBuffer)) {
          throw new TypeError('postMessage: transfer items must be ArrayBuffer (Tier-1)');
        }
        set.add(t);
      }
      var out = g._scSerialize(val, set);
      g.__postBytes(out.ab, out.transfers);
    } else {
      g.__postBytes(g._scSerialize(val));
    }
  };
  g.self.postMessage = g.postMessage;

  // ----- fetch proxy (Pass E) — must be declared BEFORE __handleInbound
  // so the handler can route fetch-resp envelopes to _pendingFetches.
  // Wire envelope uses \`__nxInternal\` as the top-level discriminator;
  // user payloads with a nested __nxInternal property are unaffected
  // because the check is on the OUTER object only. -----
  var _nextFetchId = 1;
  var _pendingFetches = Object.create(null);

  // Headers-on-worker — case-insensitive, minimal surface. Wire format
  // is an array of [k, v] pairs (Headers itself isn't structured-cloneable
  // since it's a host class).
  function WorkerHeaders(pairs) {
    this._map = Object.create(null);
    if (pairs) for (var i = 0; i < pairs.length; i++) {
      this._map[String(pairs[i][0]).toLowerCase()] = String(pairs[i][1]);
    }
  }
  WorkerHeaders.prototype.get = function (name) {
    var v = this._map[String(name).toLowerCase()];
    return v === undefined ? null : v;
  };
  WorkerHeaders.prototype.has = function (name) {
    return Object.prototype.hasOwnProperty.call(this._map, String(name).toLowerCase());
  };
  WorkerHeaders.prototype.forEach = function (fn) {
    for (var k in this._map) fn(this._map[k], k, this);
  };

  // Response-on-worker — idempotent body reads (no detach/stream
  // semantics in Tier-1; calling .text() twice returns the same string).
  function WorkerResponse(init) {
    this.status = init.status;
    this.statusText = init.statusText || '';
    this.ok = !!init.ok;
    this.url = init.url || '';
    this.headers = new WorkerHeaders(init.headers || []);
    this._body = init.body || new ArrayBuffer(0);
  }
  WorkerResponse.prototype.arrayBuffer = function () {
    return Promise.resolve(this._body);
  };
  WorkerResponse.prototype.text = function () {
    var self = this;
    return Promise.resolve().then(function () {
      return new TextDecoder().decode(new Uint8Array(self._body));
    });
  };
  WorkerResponse.prototype.json = function () {
    return this.text().then(function (s) { return JSON.parse(s); });
  };

  // Normalize \`init.headers\` from Headers-like / plain object / array
  // forms into the [[k, v]] wire format. Returns [] if absent.
  function _normalizeHeaders(h) {
    if (!h) return [];
    if (Array.isArray(h)) {
      var out = [];
      for (var i = 0; i < h.length; i++) out.push([String(h[i][0]), String(h[i][1])]);
      return out;
    }
    if (typeof h.forEach === 'function') {
      var out2 = [];
      h.forEach(function (v, k) { out2.push([String(k), String(v)]); });
      return out2;
    }
    if (typeof h === 'object') {
      var out3 = [];
      for (var k in h) if (Object.prototype.hasOwnProperty.call(h, k)) {
        out3.push([String(k), String(h[k])]);
      }
      return out3;
    }
    return [];
  }

  g.fetch = function (url, init) {
    var u = String(url);
    var rawInit = init || {};
    var method = rawInit.method ? String(rawInit.method) : 'GET';
    var headers = _normalizeHeaders(rawInit.headers);
    var body = rawInit.body;
    // Tier-1: pass through primitives + ArrayBuffer/views; reject the
    // body shapes we don't yet support so they don't silently turn into
    // empty bodies on the main side.
    if (body != null
        && typeof body !== 'string'
        && !(body instanceof ArrayBuffer)
        && !(body && body.buffer instanceof ArrayBuffer)) {
      return Promise.reject(new TypeError('Worker fetch: unsupported body type (Tier-1 supports string + ArrayBuffer + TypedArray)'));
    }
    var id = _nextFetchId++;
    return new Promise(function (resolve, reject) {
      _pendingFetches[id] = { resolve: resolve, reject: reject };
      try {
        g.__postBytes(g._scSerialize({
          __nxInternal: 'fetch-req',
          id: id,
          url: u,
          init: { method: method, headers: headers, body: body == null ? null : body },
        }));
      } catch (e) {
        delete _pendingFetches[id];
        reject(e);
      }
    });
  };

  // The native side calls this per inbound message; \`raw\` is the
  // serialised ArrayBuffer carried over the queue. Pass F:
  // \`transferABs\` (optional 2nd arg) is the side-channel transferred
  // ArrayBuffers. Internal envelopes (fetch-resp from Pass E) are
  // intercepted BEFORE firing user onmessage so worker code never sees
  // them.
  g.__handleInbound = function (raw, transferABs) {
    var data = g._scDeserialize(raw, transferABs);
    if (data && data.__nxInternal === 'fetch-resp') {
      var entry = _pendingFetches[data.id];
      if (!entry) return;
      delete _pendingFetches[data.id];
      if (data.error) { entry.reject(new Error(data.error)); return; }
      try {
        entry.resolve(new WorkerResponse({
          status: data.status, statusText: data.statusText, ok: data.ok,
          url: data.url, headers: data.headers, body: data.body,
        }));
      } catch (e) { entry.reject(e); }
      return;
    }
    var fn = g.onmessage;
    if (typeof fn === 'function') {
      fn({ data: data, type: 'message' });
    }
  };

  // Worker-only debug helper: returns count of inflight fetches so the
  // Pass E fixture can verify no leaks after parallel fetches resolve.
  g.__pendingFetchCount = function () {
    var n = 0;
    for (var k in _pendingFetches) n++;
    return n;
  };

  // ----- Timers (Pass B) -----
  // The C event loop calls \`__runDueTimers\` once per iteration. It
  // fires due callbacks and returns ms-until-next, so the cond_timedwait
  // sleeps for at most that long. Linear-insert queue: fine while N is
  // small. Each entry: { id, fn, args, deadline (ms abs from Date.now),
  // interval (0 for one-shot) }.
  var _timers = [];
  var _nextTimerId = 1;
  // Currently-firing timer's id and a cancel flag. __runDueTimers
  // already shift()'d the timer before invoking its callback, so
  // clearInterval(id) called FROM inside the callback wouldn't find
  // it in _timers and the unconditional re-insert below would silently
  // resurrect the cancelled interval. Track it explicitly. 0 = no
  // timer running. Real-HW bug: setInterval that "should have stopped"
  // kept firing after clearInterval inside its own callback.
  var _runningTimerId = 0;
  var _runningTimerCancelled = false;

  function _insertTimer(t) {
    for (var i = 0; i < _timers.length; i++) {
      if (_timers[i].deadline > t.deadline) {
        _timers.splice(i, 0, t);
        return;
      }
    }
    _timers.push(t);
  }

  g.setTimeout = function (fn, delay /*, ...args */) {
    if (typeof fn !== 'function') {
      // Spec-compliant edge case: string callbacks should be eval'd. We
      // don't support that — coerce to no-op rather than throw, so
      // pages don't crash on broken setTimeout("...") calls.
      return 0;
    }
    var d = (+delay) | 0; if (d < 0) d = 0;
    var args = [];
    for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
    var id = _nextTimerId++;
    _insertTimer({ id: id, fn: fn, args: args, deadline: Date.now() + d, interval: 0 });
    return id;
  };
  g.setInterval = function (fn, delay /*, ...args */) {
    if (typeof fn !== 'function') return 0;
    var d = (+delay) | 0; if (d < 1) d = 1; // intervals min 1ms per spec
    var args = [];
    for (var i = 2; i < arguments.length; i++) args.push(arguments[i]);
    var id = _nextTimerId++;
    _insertTimer({ id: id, fn: fn, args: args, deadline: Date.now() + d, interval: d });
    return id;
  };
  g.clearTimeout = g.clearInterval = function (id) {
    // Special case: cancelling the timer currently being executed —
    // it's already been shift()'d out of _timers, so splice won't
    // find it. Flag it so the post-callback re-insert is skipped.
    if (id === _runningTimerId) { _runningTimerCancelled = true; return; }
    for (var i = 0; i < _timers.length; i++) {
      if (_timers[i].id === id) { _timers.splice(i, 1); return; }
    }
  };

  // Native side calls this once per event-loop iteration. Fires due
  // callbacks; returns ms until next deadline (or a large sentinel
  // when none — C caps at WORKER_IDLE_WAIT_MS so 60000 just means
  // "no specific deadline, use the default idle wait").
  g.__runDueTimers = function () {
    if (_timers.length === 0) return 60000;
    var now = Date.now();
    while (_timers.length > 0 && _timers[0].deadline <= now) {
      var t = _timers.shift();
      _runningTimerId = t.id;
      _runningTimerCancelled = false;
      try { t.fn.apply(null, t.args); }
      catch (e) {
        try {
          var msg = (e && e.message) ? e.message : String(e);
          g.__postBytes(g._scSerialize(msg));
          if (g.console && typeof g.console.error === 'function') g.console.error('[timer]', msg);
        } catch (_) {}
      }
      var wasCancelled = _runningTimerCancelled;
      _runningTimerId = 0;
      _runningTimerCancelled = false;
      if (!wasCancelled && t.interval > 0) {
        t.deadline = now + t.interval;
        _insertTimer(t);
      }
    }
    if (_timers.length === 0) return 60000;
    var delta = _timers[0].deadline - Date.now();
    return delta < 0 ? 0 : delta;
  };

  // ----- performance.now (Pass B) -----
  // Date.now() resolution; sub-ms unavailable from QuickJS without a
  // separate clock binding. Good enough for typical "elapsed since X"
  // measurements; not good enough for audio-grade timing.
  var _perfStart = Date.now();
  g.performance = {
    now: function () { return Date.now() - _perfStart; },
    timeOrigin: _perfStart,
  };

  // ----- queueMicrotask (Pass B) -----
  // Promise.resolve().then is already a microtask in QuickJS; this is
  // just the standard alias.
  g.queueMicrotask = function (fn) {
    if (typeof fn !== 'function') throw new TypeError('queueMicrotask: not a function');
    Promise.resolve().then(fn);
  };

  // ----- atob / btoa (Pass B) -----
  var _b64chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  g.btoa = function (s) {
    s = String(s);
    var out = '', i = 0, n = s.length;
    while (i < n) {
      var b1 = s.charCodeAt(i++);
      if (b1 > 0xff) throw new Error("btoa: string contains characters outside Latin1");
      var b2 = i < n ? s.charCodeAt(i++) : -1;
      if (b2 > 0xff) throw new Error("btoa: string contains characters outside Latin1");
      var b3 = i < n ? s.charCodeAt(i++) : -1;
      if (b3 > 0xff) throw new Error("btoa: string contains characters outside Latin1");
      out += _b64chars[b1 >> 2];
      out += _b64chars[((b1 & 0x3) << 4) | (b2 < 0 ? 0 : (b2 >> 4))];
      out += b2 < 0 ? '=' : _b64chars[((b2 & 0xf) << 2) | (b3 < 0 ? 0 : (b3 >> 6))];
      out += b3 < 0 ? '=' : _b64chars[b3 & 0x3f];
    }
    return out;
  };
  g.atob = function (s) {
    s = String(s).replace(/[\\t\\n\\f\\r ]/g, '');
    while (s.length > 0 && s.charAt(s.length - 1) === '=') s = s.substring(0, s.length - 1);
    if (s.length % 4 === 1) throw new Error('atob: invalid base64');
    var out = '', i = 0, n = s.length;
    while (i < n) {
      var c1 = _b64chars.indexOf(s.charAt(i++));
      var c2 = i < n ? _b64chars.indexOf(s.charAt(i++)) : -1;
      var c3 = i < n ? _b64chars.indexOf(s.charAt(i++)) : -1;
      var c4 = i < n ? _b64chars.indexOf(s.charAt(i++)) : -1;
      if (c1 < 0 || c2 < 0) throw new Error('atob: invalid base64 char');
      out += String.fromCharCode((c1 << 2) | (c2 >> 4));
      if (c3 < 0) break;
      out += String.fromCharCode(((c2 & 0xf) << 4) | (c3 >> 2));
      if (c4 < 0) break;
      out += String.fromCharCode(((c3 & 0x3) << 6) | c4);
    }
    return out;
  };

  // ----- importScripts (Pass C) -----
  // Sync sequential fetch + eval. Tier-1: sdmc:/ + romfs:/ via the
  // native __workerReadFile binding (registered from worker.c). HTTP(S)
  // and brewser:// schemes need the fetch proxy — Pass E.
  g.importScripts = function () {
    for (var i = 0; i < arguments.length; i++) {
      var url = String(arguments[i]);
      var src = g.__workerReadFile(url); // throws on failure
      // Indirect eval = runs in global scope, so \`function foo() {...}\`
      // and \`var x = ...\` land on \`self\`.
      (0, eval)(src);
    }
  };
}
})(globalThis);
`;
