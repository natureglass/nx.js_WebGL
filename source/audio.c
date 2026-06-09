#include "audio.h"
#include "async.h"
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <switch.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "vendor/dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "vendor/dr_wav.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include "vendor/stb_vorbis.c"
#pragma GCC diagnostic pop

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_NUM_VOICES 24
#define AUDIO_ALIGN 0x1000

// Per-video audio sub-allocation. Each slot fits one <video>'s wave-buf
// ring (matches video.c's AUDIO_TOTAL_BUF_BYTES = 4 × 10240 = 40960).
// 40960 = 10 × AUDIO_ALIGN so slot offsets stay page-aligned naturally.
// NUM_SLOTS = 8 caps concurrent audio-bearing videos; oversize fixtures
// degrade gracefully (extra videos return NULL from acquire and play
// without audio). 8 × 40960 = 327680 bytes, comfortably inside the 1 MB
// main mempool reserved below.
#define NX_VIDEO_AUDIO_SLOT_BYTES 0xA000
#define NX_VIDEO_AUDIO_NUM_SLOTS 8

static bool audio_initialized = false;
static AudioDriver audio_driver;
static bool voice_in_use[AUDIO_NUM_VOICES];
static void *audio_mempool_ptr = NULL;
static size_t audio_mempool_size = 0;
static int audio_mempool_id = -1;
static bool video_audio_slot_in_use[NX_VIDEO_AUDIO_NUM_SLOTS];

/* Per-voice wave buffer tracking */
static AudioDriverWaveBuf voice_wavebufs[AUDIO_NUM_VOICES];

/* Per-voice mempool tracking. Records the pool_id returned by
 * `audrvMemPoolAdd` inside `nx_audio_play`, so `nx_audio_release_voice`
 * can detach + remove it when the voice is freed. -1 = no pool registered.
 * Surgical re-add Step 2 (2026-06-03): cleanup ONLY at voice-free time;
 * `nx_audio_stop` still leaks per the original behavior to avoid the
 * driver-state corruption regression. */
static int voice_mempool_id[AUDIO_NUM_VOICES];

/* Slice-2b: serializes every audrv* call site. Recursive so nested
 * acquires (e.g. nx_audio_acquire_voice → audrvVoiceStop after a leak)
 * don't self-deadlock. Initialized via the standard recursive
 * attribute below. */
static pthread_mutex_t audio_driver_lock;
static bool audio_driver_lock_initialized = false;

static void ensure_audio_driver_lock(void) {
	if (audio_driver_lock_initialized) return;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&audio_driver_lock, &attr);
	pthread_mutexattr_destroy(&attr);
	audio_driver_lock_initialized = true;
}

void nx_audio_lock(void) {
	ensure_audio_driver_lock();
	pthread_mutex_lock(&audio_driver_lock);
}

void nx_audio_unlock(void) {
	pthread_mutex_unlock(&audio_driver_lock);
}

AudioDriver *nx_audio_get_driver(void) { return &audio_driver; }

static const AudioRendererConfig arConfig = {
	.output_rate     = AudioRendererOutputRate_48kHz,
	.num_voices      = AUDIO_NUM_VOICES,
	.num_effects     = 0,
	.num_sinks       = 1,
	.num_mix_objs    = 1,
	.num_mix_buffers = 2,
};

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

/* 2026-06-08 ROUND 34/35: live-PCM-bytes accounting + budget cap with
 * silent-buffer fallback.
 *
 * Cocos (and Web Audio engines generally) pre-decode large mp3 banks
 * during scene transitions. Pvzge's gameplay-scene load decodes ~650
 * SFX mp3s; round 33 logs showed cumulative decoded PCM reaching
 * ~195 MB held in JS arraybuffer heap — enough to OOM or crash the
 * Switch app process. This budget caps the LIVE (not cumulative) PCM
 * memory: when total alive aligned PCM bytes would exceed the budget,
 * the decode returns a TINY SILENT buffer instead of the real PCM.
 *
 * Round 35 changed semantics from JS-error rejection to silent-buffer
 * fallback. Round 34 (32 MB + reject) made Cocos's loading scene hang
 * at 50% because the loader treats decodeAudioData rejection as a
 * fatal asset error and stalls the scene-load promise. Returning a
 * tiny silent buffer (one 4 KB page = ~46 ms of mono silence) makes
 * Cocos's AudioClip "succeed" → AssetManager promise resolves → scene
 * progresses. The SFX is silent for that one cue but gameplay
 * proceeds.
 *
 * `pcm_in_flight_bytes` is decremented by `pcm_arraybuffer_free`,
 * which is the GC finalizer registered on every real PCM-bearing
 * ArrayBuffer. All access is single-threaded (JS thread + GC, both
 * serialized by QuickJS). Decode WORK runs on a thread pool but the
 * memory accounting + budget check happens in
 * `decode_audio_after_work` which is on the JS thread.
 *
 * 96 MB chosen as a generous cap: well above the ~32 MB peak observed
 * in round 34 loading-scene attempt, well below the 195 MB that
 * crashed gameplay load in round 32. Expect zero silent fallbacks for
 * splash/UI, very few during gameplay scene load. */
#define PCM_BUDGET_BYTES (96ULL * 1024 * 1024)
static size_t pcm_in_flight_bytes = 0;

static void pcm_arraybuffer_free(JSRuntime *rt, void *opaque, void *ptr) {
	(void)rt;
	size_t sz = (size_t)(uintptr_t)opaque;
	if (pcm_in_flight_bytes >= sz) pcm_in_flight_bytes -= sz;
	else pcm_in_flight_bytes = 0;
	free(ptr);
}

/* ── Decode async data ── */

typedef struct {
	char *err_str;
	uint8_t *input;
	size_t input_size;
	const char *mime_type;
	/* Output */
	int16_t *pcm_data;
	uint32_t sample_rate;
	uint32_t channels;
	uint64_t total_samples;
	JSValue buffer_val;
} nx_decode_audio_async_t;

/* ── Decode work (runs on thread pool) ── */

static void decode_audio_work(nx_work_t *req) {
	nx_decode_audio_async_t *data = (nx_decode_audio_async_t *)req->data;

	if (strcmp(data->mime_type, "audio/mpeg") == 0 ||
		strcmp(data->mime_type, "audio/mp3") == 0) {
		drmp3_config cfg;
		drmp3_uint64 frame_count;
		drmp3_int16 *frames = drmp3_open_memory_and_read_pcm_frames_s16(
			data->input, data->input_size, &cfg, &frame_count, NULL);
		if (!frames) {
			data->err_str = "Failed to decode MP3";
			return;
		}
		data->pcm_data = frames;
		data->sample_rate = cfg.channels > 0 ? cfg.sampleRate : 44100;
		data->channels = cfg.channels;
		data->total_samples = frame_count;
	} else if (strcmp(data->mime_type, "audio/wav") == 0 ||
			   strcmp(data->mime_type, "audio/wave") == 0 ||
			   strcmp(data->mime_type, "audio/x-wav") == 0) {
		drwav wav;
		if (!drwav_init_memory(&wav, data->input, data->input_size, NULL)) {
			data->err_str = "Failed to decode WAV";
			return;
		}
		drwav_uint64 frame_count = wav.totalPCMFrameCount;
		int16_t *frames = malloc(frame_count * wav.channels * sizeof(int16_t));
		if (!frames) {
			drwav_uninit(&wav);
			data->err_str = "Out of memory decoding WAV";
			return;
		}
		drwav_read_pcm_frames_s16(&wav, frame_count, frames);
		data->pcm_data = frames;
		data->sample_rate = wav.sampleRate;
		data->channels = wav.channels;
		data->total_samples = frame_count;
		drwav_uninit(&wav);
	} else if (strcmp(data->mime_type, "audio/ogg") == 0 ||
			   strcmp(data->mime_type, "audio/vorbis") == 0) {
		int channels, sample_rate;
		short *output;
		int samples = stb_vorbis_decode_memory(
			data->input, (int)data->input_size, &channels, &sample_rate,
			&output);
		if (samples < 0) {
			data->err_str = "Failed to decode OGG Vorbis";
			return;
		}
		data->pcm_data = output;
		data->sample_rate = sample_rate;
		data->channels = channels;
		data->total_samples = samples;
	} else {
		data->err_str = "Unsupported audio MIME type";
	}
}

/* ── Decode after-work (runs on JS thread) ── */

static JSValue decode_audio_after_work(JSContext *ctx, nx_work_t *req) {
	nx_decode_audio_async_t *data = (nx_decode_audio_async_t *)req->data;

	JS_FreeValue(ctx, data->buffer_val);
	JS_FreeCString(ctx, data->mime_type);

	/* Keep ERROR log only — successful decodes happen 600+ times during
	 * gameplay scene load and don't need per-call logging now that the
	 * PCM budget is fielded. */
	if (data->err_str) {
		fprintf(stderr, "[nxjs:audio-decode-done] FAIL err=%s\n",
				data->err_str);
		fflush(stderr);
	}

	if (data->err_str) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, data->err_str),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	/* Create result object with ArrayBuffer for PCM data */
	size_t pcm_byte_size =
		data->total_samples * data->channels * sizeof(int16_t);

	/* Allocate page-aligned memory for audren mempool compatibility */
	size_t aligned_size = ALIGN_UP(pcm_byte_size, AUDIO_ALIGN);

	/* 2026-06-08 ROUND 34/35: enforce live-PCM budget with silent-buffer
	 * fallback. If granting this decode would push alive aligned PCM past
	 * PCM_BUDGET_BYTES, return a tiny 4 KB silent buffer instead of the
	 * real decoded PCM. Cocos's decodeAudioData promise resolves with a
	 * valid (but silent) AudioBuffer; AssetManager treats the AudioClip
	 * load as successful; the scene proceeds. The one affected SFX cue is
	 * silent. Round 34 rejected with JS error → Cocos's loader stalled
	 * the loading scene at 50%; silent fallback avoids that stall. */
	if (pcm_in_flight_bytes + aligned_size > PCM_BUDGET_BYTES) {
		fprintf(stderr,
				"[nxjs:audio-budget-silent] in_flight_mb=%.2f wanted_size=%zu budget_mb=%.2f sr=%u ch=%u\n",
				(double)pcm_in_flight_bytes / (1024.0 * 1024.0),
				aligned_size,
				(double)PCM_BUDGET_BYTES / (1024.0 * 1024.0),
				(unsigned)data->sample_rate, (unsigned)data->channels);
		fflush(stderr);
		if (data->pcm_data) free(data->pcm_data);

		/* Tiny silent buffer: one 4 KB page = 2048 int16 samples. At the
		 * original sample_rate/channels Cocos sees a brief (~40 ms mono)
		 * AudioBuffer. We do NOT count this against pcm_in_flight_bytes;
		 * if budget pressure is high, many small silent buffers staying
		 * unaccounted is acceptable (~4 KB each, hundreds before any
		 * meaningful pressure). Uses bare free as finalizer so it's not
		 * tracked. */
		void *silent_buf = memalign(AUDIO_ALIGN, AUDIO_ALIGN);
		if (!silent_buf) {
			return JS_ThrowInternalError(ctx, "OOM on silent fallback buffer");
		}
		memset(silent_buf, 0, AUDIO_ALIGN);
		armDCacheFlush(silent_buf, AUDIO_ALIGN);

		uint32_t safe_sr = data->sample_rate > 0 ? data->sample_rate : 44100;
		uint32_t safe_ch = data->channels > 0 ? data->channels : 1;
		uint64_t silent_samples = AUDIO_ALIGN / (sizeof(int16_t) * safe_ch);

		JSValue silent_ab = JS_NewArrayBuffer(
			ctx, (uint8_t *)silent_buf, AUDIO_ALIGN,
			(JSFreeArrayBufferDataFunc *)free, NULL, false);

		JSValue silent_result = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, silent_result, "pcmData", silent_ab);
		JS_SetPropertyStr(ctx, silent_result, "sampleRate",
						  JS_NewUint32(ctx, safe_sr));
		JS_SetPropertyStr(ctx, silent_result, "channels",
						  JS_NewUint32(ctx, safe_ch));
		JS_SetPropertyStr(ctx, silent_result, "samples",
						  JS_NewFloat64(ctx, (double)silent_samples));
		JS_SetPropertyStr(ctx, silent_result, "byteLength",
						  JS_NewFloat64(ctx, (double)AUDIO_ALIGN));
		return silent_result;
	}

	void *aligned_buf = memalign(AUDIO_ALIGN, aligned_size);
	if (!aligned_buf) {
		if (data->pcm_data) free(data->pcm_data);
		return JS_ThrowInternalError(ctx, "Failed to allocate aligned PCM buffer");
	}
	memcpy(aligned_buf, data->pcm_data, pcm_byte_size);
	memset((uint8_t *)aligned_buf + pcm_byte_size, 0,
		   aligned_size - pcm_byte_size);
	armDCacheFlush(aligned_buf, aligned_size);

	pcm_in_flight_bytes += aligned_size;

	/* 2026-06-08 ROUND 33: cumulative decoded-PCM memory accounting +
	 * round-34 live in_flight metric. Cumulative is monotonic (sums
	 * every decode ever); in_flight subtracts on GC, so it reflects
	 * real memory pressure. */
	{
		static uint64_t total_dec_bytes = 0;
		static uint64_t total_dec_count = 0;
		total_dec_bytes += aligned_size;
		total_dec_count++;
		if (total_dec_count <= 5 || total_dec_count % 50 == 0) {
			fprintf(stderr,
					"[nxjs:audio-pcm-mem] n=%llu bytes_this=%zu cumulative_mb=%.2f in_flight_mb=%.2f\n",
					(unsigned long long)total_dec_count,
					aligned_size,
					(double)total_dec_bytes / (1024.0 * 1024.0),
					(double)pcm_in_flight_bytes / (1024.0 * 1024.0));
			fflush(stderr);
		}
	}

	if (data->pcm_data) free(data->pcm_data);

	JSValue ab = JS_NewArrayBuffer(
		ctx, (uint8_t *)aligned_buf, aligned_size,
		pcm_arraybuffer_free, (void *)(uintptr_t)aligned_size, false);

	JSValue result = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, result, "pcmData", ab);
	JS_SetPropertyStr(ctx, result, "sampleRate",
					  JS_NewUint32(ctx, data->sample_rate));
	JS_SetPropertyStr(ctx, result, "channels",
					  JS_NewUint32(ctx, data->channels));
	JS_SetPropertyStr(ctx, result, "samples",
					  JS_NewFloat64(ctx, (double)data->total_samples));
	JS_SetPropertyStr(ctx, result, "byteLength",
					  JS_NewFloat64(ctx, (double)pcm_byte_size));

	return result;
}

/* ── Internal init (shared with source/video.c via audio.h) ── */

bool nx_audio_ensure_initialized(void) {
	nx_audio_lock();
	if (audio_initialized) {
		nx_audio_unlock();
		return true;
	}

	Result rc = audrenInitialize(&arConfig);
	if (R_FAILED(rc)) {
		nx_audio_unlock();
		return false;
	}

	// num_mempools: 2 is sufficient — slot 0 = the main 1 MB mempool
	// allocated below, which now ALSO hosts per-video audio sub-slots
	// (see nx_audio_acquire_video_buf); slot 1 = headroom for
	// nx_audio_play's per-call mempool. Per-video audio used to call its
	// own audrvMemPoolAdd, but that capped at 1 video (and bumping to 32
	// mempools froze Citron's audrv mixer with 6 concurrent voices, all
	// reading from distinct mempools — see [[nvtegra-unreliable-on-citron]]
	// for the Citron audio-renderer constraints). Sub-allocation inside
	// one shared mempool sidesteps both.
	rc = audrvCreate(&audio_driver, &arConfig, 2);
	if (R_FAILED(rc)) {
		audrenExit();
		nx_audio_unlock();
		return false;
	}

	/* Set up a default memory pool (1 MB, can grow later) */
	audio_mempool_size = ALIGN_UP(1024 * 1024, AUDIO_ALIGN);
	audio_mempool_ptr = memalign(AUDIO_ALIGN, audio_mempool_size);
	if (!audio_mempool_ptr) {
		audrvClose(&audio_driver);
		audrenExit();
		nx_audio_unlock();
		return false;
	}
	memset(audio_mempool_ptr, 0, audio_mempool_size);
	armDCacheFlush(audio_mempool_ptr, audio_mempool_size);

	audio_mempool_id =
		audrvMemPoolAdd(&audio_driver, audio_mempool_ptr, audio_mempool_size);
	audrvMemPoolAttach(&audio_driver, audio_mempool_id);

	/* Add device sink */
	static const u8 sink_channels[] = {0, 1};
	audrvDeviceSinkAdd(&audio_driver, AUDREN_DEFAULT_DEVICE_NAME, 2,
					   sink_channels);

	rc = audrenStartAudioRenderer();
	if (R_FAILED(rc)) {
		audrvClose(&audio_driver);
		audrenExit();
		nx_audio_unlock();
		return false;
	}

	audrvUpdate(&audio_driver);

	memset(voice_in_use, 0, sizeof(voice_in_use));
	memset(voice_wavebufs, 0, sizeof(voice_wavebufs));
	for (int i = 0; i < AUDIO_NUM_VOICES; i++) voice_mempool_id[i] = -1;
	audio_initialized = true;
	nx_audio_unlock();
	return true;
}

int nx_audio_acquire_voice(void) {
	nx_audio_lock();
	for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
		if (!voice_in_use[i]) {
			voice_in_use[i] = true;
			nx_audio_unlock();
			return i;
		}
	}
	nx_audio_unlock();
	return -1;
}

void nx_audio_release_voice(int voice_id) {
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES) return;
	nx_audio_lock();
	if (audio_initialized) {
		audrvVoiceStop(&audio_driver, voice_id);
		audrvUpdate(&audio_driver);
		/* Tear down the per-call mempool added by nx_audio_play. Voice
		 * was just stopped + driver updated, so audrv no longer holds
		 * wave-buf references into this pool; detach + remove is safe.
		 * Intermediate audrvUpdate between voiceStop and detach is the
		 * cautious-extra-step that distinguishes this from the reverted
		 * 2026-06-02 Tier-1 pattern (which crashed all audio). Cleanup
		 * is gated to release_voice ONLY — `nx_audio_stop` still leaks
		 * per HEAD behavior; restoring its detach is Step 4. */
		if (voice_mempool_id[voice_id] >= 0) {
			audrvMemPoolDetach(&audio_driver, voice_mempool_id[voice_id]);
			audrvMemPoolRemove(&audio_driver, voice_mempool_id[voice_id]);
			voice_mempool_id[voice_id] = -1;
			audrvUpdate(&audio_driver);
		}
	}
	voice_in_use[voice_id] = false;
	nx_audio_unlock();
}

size_t nx_audio_video_slot_size(void) {
	return NX_VIDEO_AUDIO_SLOT_BYTES;
}

void *nx_audio_acquire_video_buf(size_t bytes) {
	if (bytes > NX_VIDEO_AUDIO_SLOT_BYTES) return NULL;
	if (!nx_audio_ensure_initialized()) return NULL;
	nx_audio_lock();
	for (int i = 0; i < NX_VIDEO_AUDIO_NUM_SLOTS; i++) {
		if (!video_audio_slot_in_use[i]) {
			video_audio_slot_in_use[i] = true;
			void *ptr = (uint8_t *)audio_mempool_ptr +
				(size_t)i * NX_VIDEO_AUDIO_SLOT_BYTES;
			memset(ptr, 0, NX_VIDEO_AUDIO_SLOT_BYTES);
			armDCacheFlush(ptr, NX_VIDEO_AUDIO_SLOT_BYTES);
			nx_audio_unlock();
			return ptr;
		}
	}
	nx_audio_unlock();
	return NULL;
}

void nx_audio_release_video_buf(void *ptr) {
	if (!ptr || !audio_mempool_ptr) return;
	nx_audio_lock();
	size_t offset = (size_t)((uint8_t *)ptr - (uint8_t *)audio_mempool_ptr);
	if (offset % NX_VIDEO_AUDIO_SLOT_BYTES == 0) {
		size_t idx = offset / NX_VIDEO_AUDIO_SLOT_BYTES;
		if (idx < NX_VIDEO_AUDIO_NUM_SLOTS) {
			video_audio_slot_in_use[idx] = false;
		}
	}
	nx_audio_unlock();
}

/* ── Native JS functions ── */

static JSValue nx_audio_init(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (!nx_audio_ensure_initialized()) {
		return JS_ThrowInternalError(ctx, "audio renderer init failed");
	}
	return JS_UNDEFINED;
}

static JSValue nx_audio_exit(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	nx_audio_lock();
	if (!audio_initialized) {
		nx_audio_unlock();
		return JS_UNDEFINED;
	}

	for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
		if (voice_in_use[i]) {
			audrvVoiceStop(&audio_driver, i);
		}
	}
	audrvUpdate(&audio_driver);

	if (audio_mempool_id >= 0) {
		audrvMemPoolDetach(&audio_driver, audio_mempool_id);
		audrvMemPoolRemove(&audio_driver, audio_mempool_id);
	}
	audrvClose(&audio_driver);
	audrenExit();

	if (audio_mempool_ptr) {
		free(audio_mempool_ptr);
		audio_mempool_ptr = NULL;
	}
	memset(video_audio_slot_in_use, 0, sizeof(video_audio_slot_in_use));

	audio_initialized = false;
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_decode(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decode_audio_async_t);
	data->buffer_val = JS_DupValue(ctx, argv[0]);
	data->input = JS_GetArrayBuffer(ctx, &data->input_size, data->buffer_val);
	data->mime_type = JS_ToCString(ctx, argv[1]);
	if (!data->mime_type) {
		free(data);
		free(req);
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, decode_audio_work,
						  decode_audio_after_work);
}

static JSValue nx_audio_play(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (!audio_initialized) {
		return JS_ThrowInternalError(ctx, "Audio not initialized");
	}

	size_t pcm_size;
	uint8_t *pcm_data = JS_GetArrayBuffer(ctx, &pcm_size, argv[0]);
	if (!pcm_data)
		return JS_EXCEPTION;

	int voice_id;
	if (JS_ToInt32(ctx, &voice_id, argv[1]))
		return JS_EXCEPTION;
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES) {
		return JS_ThrowRangeError(ctx, "Invalid voice ID");
	}

	double volume;
	if (JS_ToFloat64(ctx, &volume, argv[2]))
		return JS_EXCEPTION;

	int loop = JS_ToBool(ctx, argv[3]);
	if (loop == -1)
		return JS_EXCEPTION;

	int32_t sample_rate;
	if (JS_ToInt32(ctx, &sample_rate, argv[4]))
		return JS_EXCEPTION;

	int32_t channels;
	if (JS_ToInt32(ctx, &channels, argv[5]))
		return JS_EXCEPTION;

	uint64_t total_samples;
	double samples_dbl;
	if (JS_ToFloat64(ctx, &samples_dbl, argv[6]))
		return JS_EXCEPTION;
	total_samples = (uint64_t)samples_dbl;

	/* 2026-06-08 ROUND 26: probe every audio play. Logs voice, format,
	   loop, volume so we can correlate which audio source actually reaches
	   audrv. Pvzge user reports background music silent / SFX work — this
	   probe + nx_audio_alloc_voice probe pinpoints where music fails. */
	{
		static uint64_t play_n = 0;
		play_n++;
		fprintf(stderr,
				"[nxjs:audio-play] n=%llu voice=%d vol=%.3f loop=%d sr=%d "
				"ch=%d samples=%llu pcm_size=%zu\n",
				(unsigned long long)play_n, voice_id, volume, loop,
				(int)sample_rate, (int)channels,
				(unsigned long long)total_samples, pcm_size);
		fflush(stderr);
	}

	nx_audio_lock();
	/* Add the PCM buffer as a mempool */
	size_t aligned_size = ALIGN_UP(pcm_size, AUDIO_ALIGN);
	int pool_id = audrvMemPoolAdd(&audio_driver, pcm_data, aligned_size);
	/* 2026-06-08 ROUND 33: probe audrv mempool exhaustion. Logs return value
	 * + count of voices still holding mempool slots. Pvzge crash during
	 * gameplay scene load may be audio-mempool-exhaust related. */
	{
		int active_mempools = 0;
		for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
			if (voice_mempool_id[i] >= 0) active_mempools++;
		}
		fprintf(stderr,
				"[nxjs:audio-mempool] add ret=%d active_voices_with_mp=%d pcm_bytes=%zu\n",
				pool_id, active_mempools, aligned_size);
		fflush(stderr);
	}
	if (pool_id < 0) {
		nx_audio_unlock();
		return JS_ThrowInternalError(ctx, "Failed to add audio mempool");
	}
	audrvMemPoolAttach(&audio_driver, pool_id);
	voice_mempool_id[voice_id] = pool_id;

	/* Initialize voice */
	audrvVoiceInit(&audio_driver, voice_id, channels, PcmFormat_Int16,
				   sample_rate);
	audrvVoiceSetDestinationMix(&audio_driver, voice_id, AUDREN_FINAL_MIX_ID);
	if (channels == 1) {
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 1);
	} else {
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 0.0f, 0, 1);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 0.0f, 1, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 1, 1);
	}
	audrvVoiceSetVolume(&audio_driver, voice_id, (float)volume);

	/* Set up wave buffer */
	AudioDriverWaveBuf *wavebuf = &voice_wavebufs[voice_id];
	memset(wavebuf, 0, sizeof(AudioDriverWaveBuf));
	wavebuf->data_raw = pcm_data;
	wavebuf->size = pcm_size;
	wavebuf->start_sample_offset = 0;
	wavebuf->end_sample_offset = total_samples;
	wavebuf->is_looping = loop;

	audrvVoiceAddWaveBuf(&audio_driver, voice_id, wavebuf);
	audrvVoiceStart(&audio_driver, voice_id);
	audrvUpdate(&audio_driver);
	nx_audio_unlock();

	return JS_UNDEFINED;
}

static JSValue nx_audio_stop(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	int voice_id;
	if (JS_ToInt32(ctx, &voice_id, argv[0]))
		return JS_EXCEPTION;
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES)
		return JS_ThrowRangeError(ctx, "Invalid voice ID");
	nx_audio_lock();
	if (audio_initialized) {
		audrvVoiceStop(&audio_driver, voice_id);
		audrvUpdate(&audio_driver);
	}
	voice_in_use[voice_id] = false;
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_pause(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	int paused = JS_ToBool(ctx, argv[1]);
	if (paused == -1)
		return JS_EXCEPTION;
	nx_audio_lock();
	if (audio_initialized) {
		audrvVoiceSetPaused(&audio_driver, voice_id, paused);
		audrvUpdate(&audio_driver);
	}
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_set_volume(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	double volume;
	JS_ToFloat64(ctx, &volume, argv[1]);
	nx_audio_lock();
	if (audio_initialized) {
		audrvVoiceSetVolume(&audio_driver, voice_id, (float)volume);
		audrvUpdate(&audio_driver);
	}
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_set_pitch(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	double pitch;
	JS_ToFloat64(ctx, &pitch, argv[1]);
	nx_audio_lock();
	if (audio_initialized) {
		audrvVoiceSetPitch(&audio_driver, voice_id, (float)pitch);
		audrvUpdate(&audio_driver);
	}
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_update(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	nx_audio_lock();
	if (audio_initialized) audrvUpdate(&audio_driver);
	nx_audio_unlock();
	return JS_UNDEFINED;
}

static JSValue nx_audio_get_played_samples(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	u64 count = 0;
	nx_audio_lock();
	if (audio_initialized) count = audrvVoiceGetPlayedSampleCount(&audio_driver, voice_id);
	nx_audio_unlock();
	return JS_NewFloat64(ctx, (double)count);
}

static JSValue nx_audio_alloc_voice(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	int v = nx_audio_acquire_voice();
	/* 2026-06-08 ROUND 26: probe voice allocation. If music never gets a
	   voice, allocator is exhausted. Throttled to first 100 + every 25th. */
	{
		static uint64_t alloc_n = 0;
		alloc_n++;
		if (alloc_n <= 100 || (alloc_n % 25) == 0) {
			fprintf(stderr, "[nxjs:audio-alloc-voice] n=%llu voice=%d\n",
					(unsigned long long)alloc_n, v);
			fflush(stderr);
		}
	}
	if (v < 0) return JS_ThrowInternalError(ctx, "No free audio voices");
	return JS_NewInt32(ctx, v);
}

static JSValue nx_audio_free_voice(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	nx_audio_release_voice(voice_id);
	return JS_UNDEFINED;
}

static JSValue nx_audio_is_playing(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	bool playing = false;
	nx_audio_lock();
	if (audio_initialized) {
		AudioDriverWaveBuf *wb = &voice_wavebufs[voice_id];
		playing = wb->state == AudioDriverWaveBufState_Playing;
	}
	nx_audio_unlock();
	return JS_NewBool(ctx, playing);
}

/* Page-aligned PCM allocator for the Web Audio polyfill. JS-allocated
 * ArrayBuffers are not 4KB-aligned, which makes audrvMemPoolAdd reject
 * them. Returns an ArrayBuffer backed by memalign(AUDIO_ALIGN, …) with
 * `free` as the GC callback. Pure allocator — does NOT touch the audrv
 * mempool table. Step 1 of the surgical re-add 2026-06-03. */
static JSValue nx_audio_alloc_pcm(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	int64_t bytes;
	if (JS_ToInt64(ctx, &bytes, argv[0])) return JS_EXCEPTION;
	if (bytes <= 0) return JS_ThrowRangeError(ctx, "byteLength must be > 0");
	size_t aligned_size = ALIGN_UP((size_t)bytes, AUDIO_ALIGN);
	void *buf = memalign(AUDIO_ALIGN, aligned_size);
	if (!buf) return JS_ThrowOutOfMemory(ctx);
	memset(buf, 0, aligned_size);
	armDCacheFlush(buf, aligned_size);
	return JS_NewArrayBuffer(ctx, (uint8_t *)buf, aligned_size,
							 (JSFreeArrayBufferDataFunc *)free, NULL, false);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("audioInit", 0, nx_audio_init),
	JS_CFUNC_DEF("audioExit", 0, nx_audio_exit),
	JS_CFUNC_DEF("audioDecode", 2, nx_audio_decode),
	JS_CFUNC_DEF("audioPlay", 7, nx_audio_play),
	JS_CFUNC_DEF("audioStop", 1, nx_audio_stop),
	JS_CFUNC_DEF("audioPause", 2, nx_audio_pause),
	JS_CFUNC_DEF("audioSetVolume", 2, nx_audio_set_volume),
	JS_CFUNC_DEF("audioSetPitch", 2, nx_audio_set_pitch),
	JS_CFUNC_DEF("audioUpdate", 0, nx_audio_update),
	JS_CFUNC_DEF("audioGetPlayedSamples", 1, nx_audio_get_played_samples),
	JS_CFUNC_DEF("audioAllocVoice", 0, nx_audio_alloc_voice),
	JS_CFUNC_DEF("audioFreeVoice", 1, nx_audio_free_voice),
	JS_CFUNC_DEF("audioIsPlaying", 1, nx_audio_is_playing),
	JS_CFUNC_DEF("audioAllocPCM", 1, nx_audio_alloc_pcm),
};

void nx_init_audio(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
