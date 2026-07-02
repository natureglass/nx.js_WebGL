// Switch.VideoDecoder native bindings (Phase 2.G.1 cut #22).
//
// Minimal port of the QuickJS-era Switch.VideoDecoder API onto the V8 fork's
// portable nx_media_* pipeline. Unlike the (much larger) QuickJS video.c
// which owns its own decode threads, packet rings, and audrv voice
// bookkeeping, this file is a thin V8-binding wrapper over nx_media_open /
// nx_media_present — which the fork's Video class already uses.
//
// Scope for cut #22: enough to unblock webgl-materials-video (the only v2
// demo needing Switch.VideoDecoder). Specifically:
//   * synchronous open (blocks briefly on nx_media_open — fine for local
//     sdmc files),
//   * play(),
//   * nextFrame() returning `{data: ArrayBuffer, width, height, pts, ended}`
//     with BGRA→RGBA swizzle (nx_media outputs BGRA per media-decoder.h;
//     the demo's DataTexture uses THREE.RGBAFormat / UnsignedByteType),
//   * getters the demo reads: width, height, duration, error, ended,
//     paused (stub false — nx_media doesn't expose paused state getter),
//     usedVideo, usedAudio, usedHw (stub false — nx_media hides
//     hw/sw decision), muted/volume/audioTime/audioError (stubs — audio
//     integration deferred; the demo uses `noAudio: true` so this path
//     is exercised without an audio voice).
//
// Deferred (not needed by webgl-materials-video, ports if future demos
// require them):
//   * seek, pause, close-and-reopen cycles
//   * setMuted/setVolume + audio-graph attach
//   * getAudioLevels/getFrequencyData/getWaveform (audio-visualizer surface
//     — the QuickJS impl carries ~600 lines of audrv played-sample-count
//     wave-buffer bookkeeping; only useful once an audio path lands).

#include "video-decoder.h"
#include "audio.h"
#include "audio-graph.h"
#include "error.h"
#include "media-decoder.h"
#include "types.h"
#include "wrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace v8;

namespace {

struct nx_video_decoder_t {
	nx_media_t *media = nullptr;
	// Stream-source node attached to the media (cut #22b, 2026-07-02).
	// Released strictly AFTER nx_media_destroy joins the decode thread, so
	// the producer can never touch a freed node. NULL when the source has
	// no audio track OR `noAudio: true` was passed at construction.
	nx_audio_node *audio_node = nullptr;
	uint8_t *frame_buf = nullptr; // BGRA — nx_media_present output, w*h*4
	int width = 0;
	int height = 0;
	double duration = 0.0;
	bool eos_sent = false; // one-shot end-of-stream sentinel latch
	bool closed = false;
	// Cut #22b: mirrors nx_media's playing state at the JS boundary so
	// the `paused` getter can report it without adding an accessor to
	// media-decoder.h. Starts paused (nx_media_open leaves the clock
	// stopped until nx_media_play).
	bool paused = true;
	// Audio state (JS-side wraps in a GainNode too — the C state is the
	// source of truth for the `muted` / `volume` getters).
	bool muted = false;
	float volume = 1.0f;
};

void free_video_decoder(nx_video_decoder_t *d) {
	if (d->media) {
		nx_media_destroy(d->media); // joins decode thread first
		d->media = nullptr;
	}
	if (d->audio_node) {
		// Safe to release now — the producer thread is gone.
		nx_audio_node_release(d->audio_node);
		d->audio_node = nullptr;
	}
	free(d->frame_buf);
	d->frame_buf = nullptr;
	delete d;
}

nx_video_decoder_t *get_decoder(Isolate *iso, Local<Value> val) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(val);
	if (!d) nx_throw(iso, "expected VideoDecoder handle");
	return d;
}

// videoDecoderNew(url: string, opts?: object) → handle
void nx_video_decoder_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	if (info.Length() < 1 || !info[0]->IsString()) {
		nx_throw(iso, "videoDecoderNew(url, opts?) — url string required");
		return;
	}
	String::Utf8Value url_val(iso, info[0]);
	const char *url = *url_val ? *url_val : "";

	bool want_loop = false;
	if (info.Length() >= 2 && info[1]->IsObject()) {
		Local<Object> opts = info[1].As<Object>();
		Local<Value> loop_val;
		if (opts->Get(ctx, nx_str(iso, "loop")).ToLocal(&loop_val) &&
		    loop_val->IsBoolean()) {
			want_loop = loop_val->BooleanValue(iso);
		}
		// hwAccel, muted, noAudio: consumed silently. nx_media picks
		// hw/sw internally; audio path not wired in cut #22.
	}

	char err_buf[256] = {};
	nx_media_t *media = nx_media_open(url, nullptr, 0, nullptr,
	                                   err_buf, sizeof(err_buf));
	if (!media) {
		char msg[512];
		snprintf(msg, sizeof(msg), "VideoDecoder open: %s", err_buf);
		nx_throw(iso, msg);
		return;
	}

	int w = nx_media_width(media);
	int h = nx_media_height(media);
	double duration = nx_media_duration(media);

	uint8_t *buf = nullptr;
	if (nx_media_has_video(media) && w > 0 && h > 0) {
		buf = (uint8_t *)nx_alloc(iso, (size_t)w * h * 4);
		if (!buf) {
			// nx_alloc scheduled the OOM exception on the isolate.
			nx_media_destroy(media);
			return;
		}
		memset(buf, 0, (size_t)w * h * 4);
	}

	if (want_loop) nx_media_set_loop(media, true);

	nx_video_decoder_t *d = new nx_video_decoder_t();
	d->media = media;
	d->frame_buf = buf;
	d->width = w;
	d->height = h;
	d->duration = duration;

	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_video_decoder_t>(iso, obj, d, free_video_decoder);
	info.GetReturnValue().Set(obj);
}

// videoDecoderPlay(dec) → undefined
void nx_video_decoder_play(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (d && d->media) {
		nx_media_play(d->media);
		d->paused = false;
	}
}

// videoDecoderPause(dec) → undefined (cut #22b: was deferred in cut #22).
void nx_video_decoder_pause(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (d && d->media) {
		nx_media_pause(d->media);
		d->paused = true;
	}
}

// videoDecoderSeek(dec, seconds) → undefined (cut #22b).
void nx_video_decoder_seek(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || !d->media) return;
	double t = 0;
	if (!info[1]->NumberValue(iso->GetCurrentContext()).To(&t)) t = 0;
	if (!(t >= 0)) t = 0;
	nx_media_seek(d->media, t);
}

// videoDecoderClose(dec) → undefined
void nx_video_decoder_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || d->closed) return;
	d->closed = true;
	if (d->media) {
		nx_media_destroy(d->media); // joins decode thread first
		d->media = nullptr;
	}
	if (d->audio_node) {
		nx_audio_node_release(d->audio_node);
		d->audio_node = nullptr;
	}
	free(d->frame_buf);
	d->frame_buf = nullptr;
}

// videoDecoderCreateAudioNode(dec, audioCtxHandle) -> stream node handle | null
//
// Attaches a STREAM_SOURCE node to the media's audio track and hands its
// handle to the JS wrapper (mirrors nx_video_create_audio_node for the
// Video element). Returns null when the source has no audio, when audio
// was already wired, or when the media handle is torn down.
//
// The returned wrapper has NO finalizer: node ownership is the decoder's
// (released in free_video_decoder AFTER the decode thread joins). The JS
// side keeps the returned wrapper alive as long as it holds the decoder
// (via a GainNode reference stored on the JS class).
void nx_video_decoder_create_audio_node(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d) return;
	nx_audio_ctx_t *ctx = nx::Unwrap<nx_audio_ctx_t>(info[1]);
	if (!ctx) {
		nx_throw(iso, "expected AudioContext handle");
		return;
	}
	if (!d->media || !nx_media_has_audio(d->media) || d->audio_node) {
		info.GetReturnValue().SetNull();
		return;
	}
	nx_audio_node *node =
	    nx_audio_node_create(ctx->graph, NX_AUDIO_NODE_STREAM_SOURCE);
	d->audio_node = node;
	nx_media_set_audio_node(d->media, node, ctx->graph->sample_rate);
	Local<Object> obj = nx::NewWrapped(iso);
	obj->SetAlignedPointerInInternalField(0, node,
	                                      kEmbedderDataTypeTagDefault);
	info.GetReturnValue().Set(obj);
}

// videoDecoderSetVolume(dec, value: number) — clamps to [0, 1]. The JS
// wrapper also applies this through its GainNode; the C side stores it so
// that the `volume` getter reads the last-set value.
void nx_video_decoder_set_volume(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d) return;
	double v = 1.0;
	if (!info[1]->NumberValue(iso->GetCurrentContext()).To(&v)) v = 1.0;
	if (!(v >= 0)) v = 0;
	if (v > 1) v = 1;
	d->volume = (float)v;
}

// videoDecoderSetMuted(dec, muted: boolean) — JS wrapper zeroes its
// GainNode's gain when muted; the C state feeds the `muted` getter.
void nx_video_decoder_set_muted(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d) return;
	d->muted = info[1]->BooleanValue(iso);
}

// Cut #22b Stage 2 (2026-07-02): visualizer surface.
//
// Each accessor takes a Float32Array output buffer (the caller pre-sizes
// it — spectraplay uses 256 for waveform, 128 for spectrum) and returns
// true when it was filled. Returning false lets the caller fall back to
// a synthetic waveform (spectraplay does this by design).

// Extract the caller's Float32Array output pointer + element count.
// Returns NULL and no exception on non-Float32Array (caller returns
// false to the JS side, matching the try/catch pattern in
// brewser-runtime's videoGetFrequencyData/videoGetWaveform).
float *unwrap_f32(const FunctionCallbackInfo<Value> &info, uint32_t *out_len) {
	if (!info[1]->IsFloat32Array()) return nullptr;
	Local<Float32Array> ta = info[1].As<Float32Array>();
	std::shared_ptr<BackingStore> bs = ta->Buffer()->GetBackingStore();
	*out_len = (uint32_t)(ta->ByteLength() / sizeof(float));
	return reinterpret_cast<float *>(
	    static_cast<uint8_t *>(bs->Data()) + ta->ByteOffset());
}

// videoDecoderGetWaveform(dec, out: Float32Array) → boolean
void nx_video_decoder_get_waveform(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || !d->media) {
		info.GetReturnValue().Set(False(iso));
		return;
	}
	uint32_t n = 0;
	float *out = unwrap_f32(info, &n);
	if (!out) {
		info.GetReturnValue().Set(False(iso));
		return;
	}
	bool ok = nx_media_read_waveform(d->media, out, n);
	info.GetReturnValue().Set(Boolean::New(iso, ok));
}

// videoDecoderGetFrequencyData(dec, out: Float32Array) → boolean
void nx_video_decoder_get_frequency_data(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || !d->media) {
		info.GetReturnValue().Set(False(iso));
		return;
	}
	uint32_t n = 0;
	float *out = unwrap_f32(info, &n);
	if (!out) {
		info.GetReturnValue().Set(False(iso));
		return;
	}
	bool ok = nx_media_read_spectrum(d->media, out, n);
	info.GetReturnValue().Set(Boolean::New(iso, ok));
}

// videoDecoderGetAudioLevels(dec) → number[] (bass, mid, high) — [] when
// audio hasn't accumulated a tap window yet.
void nx_video_decoder_get_audio_levels(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || !d->media) {
		info.GetReturnValue().Set(Array::New(iso, 0));
		return;
	}
	float bands[3] = {0, 0, 0};
	uint32_t n = nx_media_read_audio_levels(d->media, bands, 3);
	Local<Array> arr = Array::New(iso, (int)n);
	for (uint32_t i = 0; i < n; i++) {
		arr->Set(ctx, i, Number::New(iso, (double)bands[i])).Check();
	}
	info.GetReturnValue().Set(arr);
}

// videoDecoderNextFrame(dec) → {data, width, height, pts, ended} | null
void nx_video_decoder_next_frame(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_video_decoder_t *d = get_decoder(iso, info[0]);
	if (!d || !d->media) return; // undefined → JS null via the wrapper

	// End-of-stream: emit one sentinel, then keep returning undefined.
	if (nx_media_ended(d->media)) {
		if (d->eos_sent) return;
		d->eos_sent = true;
		Local<Object> result = Object::New(iso);
		result->Set(ctx, nx_str(iso, "data"), Null(iso)).Check();
		result->Set(ctx, nx_str(iso, "width"), Integer::New(iso, d->width))
		    .Check();
		result->Set(ctx, nx_str(iso, "height"), Integer::New(iso, d->height))
		    .Check();
		result->Set(ctx, nx_str(iso, "pts"), Number::New(iso, 0.0)).Check();
		result->Set(ctx, nx_str(iso, "ended"), True(iso)).Check();
		info.GetReturnValue().Set(result);
		return;
	}

	if (!d->frame_buf) return; // audio-only source; nothing to present

	if (!nx_media_present(d->media, &d->frame_buf)) {
		// Media clock says no new frame is due yet.
		return;
	}

	// Have a fresh frame in d->frame_buf (BGRA order). Allocate an
	// ArrayBuffer and swizzle B↔R while copying — the demo hands the
	// bytes to a THREE.DataTexture using RGBAFormat/UnsignedByteType.
	// Cost: ~500 KB/s per frame at Sintel's ~480×204 @ 24 fps — trivial.
	size_t byte_count = (size_t)d->width * d->height * 4;
	std::unique_ptr<BackingStore> bs =
	    ArrayBuffer::NewBackingStore(iso, byte_count);
	uint8_t *dst = (uint8_t *)bs->Data();
	const uint8_t *src = d->frame_buf;
	for (size_t i = 0; i < byte_count; i += 4) {
		dst[i] = src[i + 2];     // R ← B
		dst[i + 1] = src[i + 1]; // G
		dst[i + 2] = src[i];     // B ← R
		dst[i + 3] = src[i + 3]; // A
	}
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, std::move(bs));

	Local<Object> result = Object::New(iso);
	result->Set(ctx, nx_str(iso, "data"), ab).Check();
	result->Set(ctx, nx_str(iso, "width"), Integer::New(iso, d->width)).Check();
	result->Set(ctx, nx_str(iso, "height"), Integer::New(iso, d->height))
	    .Check();
	result->Set(ctx, nx_str(iso, "pts"),
	            Number::New(iso, nx_media_current_time(d->media)))
	    .Check();
	result->Set(ctx, nx_str(iso, "ended"), False(iso)).Check();
	info.GetReturnValue().Set(result);
}

// Prototype getters. `this` is the JS VideoDecoder instance (wrapped).
void nx_vd_get_width(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	info.GetReturnValue().Set(Integer::New(info.GetIsolate(), d ? d->width : 0));
}
void nx_vd_get_height(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	info.GetReturnValue().Set(Integer::New(info.GetIsolate(),
	                                        d ? d->height : 0));
}
void nx_vd_get_duration(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	info.GetReturnValue().Set(Number::New(info.GetIsolate(),
	                                       d ? d->duration : 0.0));
}
void nx_vd_get_error(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	if (!d || !d->media) {
		info.GetReturnValue().SetNull();
		return;
	}
	const char *err = nx_media_error(d->media);
	if (!err) {
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(nx_str_lossy(iso, err));
}
void nx_vd_get_ended(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	bool v = d && d->media && nx_media_ended(d->media);
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), v));
}
void nx_vd_get_has_video(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	bool v = d && d->media && nx_media_has_video(d->media);
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), v));
}
void nx_vd_get_has_audio(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	bool v = d && d->media && nx_media_has_audio(d->media);
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), v));
}
// Deferred (stubs): the demo reads them but doesn't act on them.
// Cut #22b: track paused state at the JS boundary (set in play/pause
// bindings). Backs the seek-bar's is-playing indicator + spectraplay's
// visualizer scale (which reads `audio.paused`). Fresh decoders report
// `true` (not yet started); after nx_media_ended is reached we don't
// flip it — an ended decoder is effectively paused for UI purposes.
void nx_vd_get_paused(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	bool paused = true;
	if (d) {
		paused = d->paused ||
		         (d->media && nx_media_ended(d->media));
	}
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), paused));
}
void nx_vd_get_used_hw(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(False(info.GetIsolate()));
}
// Cut #22b (2026-07-02): read the current audio state from the decoder;
// `audioTime` slaves to the audio clock when audio is wired (else 0.0).
void nx_vd_get_muted(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), d && d->muted));
}
void nx_vd_get_volume(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	info.GetReturnValue().Set(
	    Number::New(info.GetIsolate(), d ? (double)d->volume : 1.0));
}
void nx_vd_get_audio_time(const FunctionCallbackInfo<Value> &info) {
	nx_video_decoder_t *d = nx::Unwrap<nx_video_decoder_t>(info.This());
	double t = (d && d->media && d->audio_node)
	               ? nx_media_current_time(d->media)
	               : 0.0;
	info.GetReturnValue().Set(Number::New(info.GetIsolate(), t));
}
void nx_vd_get_audio_error(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().SetNull();
}

// videoDecoderInit(ClassCtor) — TS wrapper calls this once at module load
// to install prototype getters onto Switch.VideoDecoder.prototype.
void nx_video_decoder_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	if (info.Length() < 1 || !info[0]->IsObject()) {
		nx_throw(iso, "videoDecoderInit(ClassCtor) — object required");
		return;
	}
	Local<Value> proto_val;
	if (!info[0].As<Object>()
	         ->Get(ctx, nx_str(iso, "prototype"))
	         .ToLocal(&proto_val)) {
		return;
	}
	if (!proto_val->IsObject()) return;
	Local<Object> proto = proto_val.As<Object>();
	NX_DEF_GET(proto, "width", nx_vd_get_width);
	NX_DEF_GET(proto, "height", nx_vd_get_height);
	NX_DEF_GET(proto, "duration", nx_vd_get_duration);
	NX_DEF_GET(proto, "error", nx_vd_get_error);
	NX_DEF_GET(proto, "ended", nx_vd_get_ended);
	NX_DEF_GET(proto, "usedVideo", nx_vd_get_has_video);
	NX_DEF_GET(proto, "usedAudio", nx_vd_get_has_audio);
	NX_DEF_GET(proto, "paused", nx_vd_get_paused);
	NX_DEF_GET(proto, "usedHw", nx_vd_get_used_hw);
	NX_DEF_GET(proto, "muted", nx_vd_get_muted);
	NX_DEF_GET(proto, "volume", nx_vd_get_volume);
	NX_DEF_GET(proto, "audioTime", nx_vd_get_audio_time);
	NX_DEF_GET(proto, "audioError", nx_vd_get_audio_error);
}

} // namespace

void nx_init_video_decoder(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "videoDecoderInit", nx_video_decoder_init_class);
	NX_SET_FUNC(init_obj, "videoDecoderNew", nx_video_decoder_new);
	NX_SET_FUNC(init_obj, "videoDecoderPlay", nx_video_decoder_play);
	NX_SET_FUNC(init_obj, "videoDecoderPause", nx_video_decoder_pause);
	NX_SET_FUNC(init_obj, "videoDecoderSeek", nx_video_decoder_seek);
	NX_SET_FUNC(init_obj, "videoDecoderClose", nx_video_decoder_close);
	NX_SET_FUNC(init_obj, "videoDecoderNextFrame", nx_video_decoder_next_frame);
	// Cut #22b: audio-graph attach + volume/mute (spectraplay MP3 fix).
	NX_SET_FUNC(init_obj, "videoDecoderCreateAudioNode",
	            nx_video_decoder_create_audio_node);
	NX_SET_FUNC(init_obj, "videoDecoderSetVolume",
	            nx_video_decoder_set_volume);
	NX_SET_FUNC(init_obj, "videoDecoderSetMuted",
	            nx_video_decoder_set_muted);
	// Cut #22b Stage 2: visualizer surface (spectraplay's spectrum + wave).
	NX_SET_FUNC(init_obj, "videoDecoderGetWaveform",
	            nx_video_decoder_get_waveform);
	NX_SET_FUNC(init_obj, "videoDecoderGetFrequencyData",
	            nx_video_decoder_get_frequency_data);
	NX_SET_FUNC(init_obj, "videoDecoderGetAudioLevels",
	            nx_video_decoder_get_audio_levels);
}
