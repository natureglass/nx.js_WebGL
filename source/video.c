// video.c — Switch.VideoDecoder (slice 2b: video + audio + AV sync).
//
// Backs the switch-web-browser <video> element. Opens an mp4 (or any
// FFmpeg-supported container), decodes the video stream via libavcodec
// with NVTEGRA hardware acceleration when available, downloads frames to
// NV12 via av_hwframe_transfer_data, scales to RGBA via libswscale, and
// hands the most-recent frame to JS through Switch.videoDecoderNextFrame().
//
// Slice 2b adds audio decode + audrv playback + audio-master AV sync:
// the audio stream (typically AAC) is decoded on the same worker thread,
// resampled via libswresample to S16LE/48 kHz/stereo, and pushed into
// the shared audrv driver via per-decoder wave-buffer ring. JS-side
// `nextFrame()` paces video against `audrvVoiceGetPlayedSampleCount`
// instead of wall-clock so video and audio stay in sync. Files without
// an audio stream fall back to the slice-2a wall-clock pacing.

#include "video.h"
#include "async.h"
#include "audio.h"

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#define VIDEO_RING_SIZE 3
#define VIDEO_AVIO_BUF_SIZE 32768

// Slice-2b audio constants. Output is always S16LE / 48 kHz / stereo —
// the audrv renderer is configured for that rate (AudioRendererOutputRate_48kHz
// in audio.c) so we resample whatever the source codec gives us to match.
#define AUDIO_OUT_SAMPLE_RATE 48000
#define AUDIO_OUT_CHANNELS    2
#define AUDIO_OUT_SAMPLE_BYTES 2 // s16le = 2 bytes per sample per channel
#define AUDIO_OUT_FRAME_BYTES (AUDIO_OUT_CHANNELS * AUDIO_OUT_SAMPLE_BYTES)
// One wave buf = ~53 ms of audio at 48 kHz stereo s16 = 2560 frames =
// 10240 bytes. Sized so 4 wave bufs total 40960 bytes — naturally
// 0x1000-aligned, which audrv mempool sub-regions must be (sub-allocated
// via audio.c's nx_audio_acquire_video_buf — must match its
// NX_VIDEO_AUDIO_SLOT_BYTES = 0xA000 = 40960). With 4 wave bufs in flight
// we keep ~210 ms of audio buffered ahead of the play head — plenty of
// headroom for decode hiccups without piling on latency.
#define AUDIO_WAVE_BUF_FRAMES 2560
#define AUDIO_WAVE_BUF_BYTES (AUDIO_WAVE_BUF_FRAMES * AUDIO_OUT_FRAME_BYTES)
#define AUDIO_NUM_WAVE_BUFS  4
#define AUDIO_TOTAL_BUF_BYTES (AUDIO_NUM_WAVE_BUFS * AUDIO_WAVE_BUF_BYTES)
#define AUDIO_MEMPOOL_ALIGN 0x1000

// Audio packet queue (demuxer → audio decoder thread). Sized for ~680 ms of
// audio (32 × 21.33 ms per AAC packet @ 1024 samples/48 kHz). Audio packets
// are small; the queue exists so the demuxer can hand audio off without
// waiting for decode.
#define AUDIO_PKT_QUEUE_SIZE 32

// Video packet queue (demuxer → video decoder thread). Sized for ~1 s of
// video at 30 fps. Decouples av_read_frame from send_packet+drain+ring_push,
// so the demuxer never blocks on ring-full. Without this split the demuxer
// is tied to JS pop rate when the ring fills, which gates audio packet rate
// below realtime and starves the audrv voice — the bug that's been biting
// us across the last several iterations.
#define VIDEO_PKT_QUEUE_SIZE 30
// Compile-time assertion: AUDIO_TOTAL_BUF_BYTES must be a multiple of
// AUDIO_MEMPOOL_ALIGN so each video's sub-region inside the shared main
// mempool stays page-aligned (audrv requires this for DMA).
_Static_assert((AUDIO_TOTAL_BUF_BYTES % AUDIO_MEMPOOL_ALIGN) == 0,
	"AUDIO_TOTAL_BUF_BYTES must be 0x1000-aligned for audrv DMA");

// Shared NVTEGRA hw_device_ctx, lazily created on first decoder open.
// Survives for the process lifetime; the libnx Tegra channel underneath
// is cheap enough that re-creation per-decoder isn't worth the bookkeeping.
static AVBufferRef *g_hw_dev_ctx = NULL;
static bool g_hw_dev_attempted = false;
static pthread_mutex_t g_hw_dev_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	uint8_t *rgba;       // width*height*4, owned by the decoder
	int width, height;
	double pts_sec;
	bool valid;
	bool eos_marker;     // sentinel slot: pop returns null + ended=true
} nx_video_slot_t;

// Slice-2b: one wave buf in the per-decoder audrv ring. `data` points
// into v->audio_buf (the per-decoder mempool memory). `wb` is the libnx
// struct we hand to audrvVoiceAddWaveBuf — once added, audrv owns its
// lifecycle until wb.state transitions to Done, at which point we can
// reuse the slot.
typedef struct {
	AudioDriverWaveBuf wb;
	uint8_t *data;         // pointer into v->audio_buf
} nx_video_wave_buf_t;

typedef struct {
	// FFmpeg
	AVFormatContext *fmt_ctx;
	AVCodecContext *v_ctx;
	AVCodecContext *a_ctx;          // slice-2b, NULL if no audio stream
	AVBufferRef *hw_dev_ref;       // ref to g_hw_dev_ctx, or NULL if sw-only
	struct SwsContext *sws_ctx;
	enum AVPixelFormat sws_src_fmt;
	int sws_src_w, sws_src_h;
	struct SwrContext *swr_ctx;     // slice-2b, NULL if no audio stream

	// Input source
	FILE *src_fp;
	AVIOContext *avio_ctx;
	uint8_t *avio_buf;

	// Stream info
	int v_stream_idx;
	int a_stream_idx;               // slice-2b; -1 if no audio stream
	int width, height;
	double duration_sec;
	bool used_hw;
	bool used_audio;                // slice-2b; true iff a_ctx opened ok
	bool used_video;                // slice-2b followup #4: false when the
	                                // file has no video stream (audio-only
	                                // file in a <video> element). Demuxer
	                                // skips the video dispatch, no video
	                                // thread is spawned, sws is unused.
	bool muted;                     // slice-2b followup #5: when true,
	                                // audrvVoiceSetVolume(0) is called so
	                                // audio is silenced. Persists across
	                                // pause/resume and through seeks.
	bool no_audio;                  // slice-2b followup #6: when true at
	                                // construction, the audio stream is
	                                // ignored entirely — no avcodec
	                                // open, no swr_ctx, no audrvVoice
	                                // allocation, no audio thread spawn.
	                                // Used by the poster-frame preview
	                                // path so opening a video to grab
	                                // its first frame doesn't produce
	                                // any audrv state transitions at
	                                // all. Cannot be flipped at runtime
	                                // (would have to re-open).
	char *audio_error_msg;          // slice-2b; owned, first audio error wins
	bool loop;                      // slice-2b followup #3: <video loop>
	                                // — on EOF demuxer re-seeks to 0 + flushes
	                                // both decoder queues + resets audrv voice
	                                // instead of pushing the EOS marker.

	// Ring buffer (worker → JS)
	pthread_mutex_t lock;
	pthread_cond_t has_space;       // signaled by JS after pop
	pthread_cond_t has_data;        // signaled by worker after push
	nx_video_slot_t slots[VIDEO_RING_SIZE];
	int head;                       // next write index (worker)
	int tail;                       // next read index (JS)
	int count;
	// Bumped every time nx_video_ring_clear runs (loop wrap, seek). The
	// video thread captures this at packet-pop time and re-checks before
	// committing each decoded frame; if it has moved, the frame is from
	// an old play-through (or pre-seek position) and gets discarded
	// instead of polluting the freshly-cleared ring with stale pts that
	// JS pacing would interpret as "video way ahead of audio, wait."
	u32 ring_gen;

	// Control flags (mutex-protected via lock)
	bool paused;
	bool stop_requested;
	bool seek_requested;
	double seek_target_sec;
	bool ended;
	char *error_msg;                // owned

	// Pacing (read/written only by JS thread)
	bool clock_started;
	double first_pts_sec;
	u64 wall_start_ns;

	// Slice-2b audio playback state. The audio worker thread owns the audrv
	// wave-buffer ring (push side); the JS thread reads
	// `audrvVoiceGetPlayedSampleCount` for AV sync (pop side). All audrv
	// touches go through nx_audio_lock/unlock; the wave-buf array state
	// transitions are managed by audrv itself once a buf is added.
	int a_voice_id;                  // -1 if not allocated
	int a_mempool_id;                // -1 if not added
	uint8_t *audio_buf;              // mem-aligned, AUDIO_TOTAL_BUF_BYTES
	nx_video_wave_buf_t wave_bufs[AUDIO_NUM_WAVE_BUFS];
	int next_wave_buf;               // audio-worker's next-to-fill index
	bool audio_clock_started;        // true once first wave buf queued
	double first_audio_pts_sec;      // first audio packet's PTS
	// audrvVoiceGetPlayedSampleCount is CUMULATIVE since voice init —
	// audrvVoiceStop+Start (e.g. on seek + loop wrap) does NOT reset it.
	// Capture the count immediately before audrvVoiceStart so post-start
	// "samples played since this start" = (current_played - baseline).
	// First-ever start: baseline = 0. Loop wrap or seek: baseline = the
	// total of everything played up to that wrap.
	u64 audio_played_baseline;
	bool audio_voice_paused;         // mirror of audrvVoiceSetPaused state

	// Audio packet queue (demuxer → audio decoder). Producer = demuxer
	// (worker) thread; consumer = a_worker thread. Bounded ring of cloned
	// AVPacket pointers. Demuxer briefly blocks on push when full (or
	// returns early on stop/seek); audio thread blocks on pop when empty.
	pthread_mutex_t apkt_lock;
	pthread_cond_t apkt_has_data;
	pthread_cond_t apkt_has_space;
	AVPacket *apkt_ring[AUDIO_PKT_QUEUE_SIZE];
	// Parallel array: ring_gen captured at PUSH time. Stored per packet
	// so the consumer can compare against current ring_gen and discard
	// stale (pre-wrap) packets — capturing at pop time has a race where
	// a bump can happen between pop returning and the consumer reading
	// ring_gen, tagging a pre-wrap packet with a post-wrap gen.
	u32 apkt_gen[AUDIO_PKT_QUEUE_SIZE];
	int apkt_head;                   // next write index
	int apkt_tail;                   // next read index
	int apkt_count;
	bool apkt_flush_requested;       // signal to audio thread to drop all queued

	// Video packet queue (demuxer → video decoder). Same shape as apkt.
	// An empty (size=0, data=NULL) AVPacket pushed via vpkt_push_eos
	// signals end-of-stream — video thread sends NULL to the codec to
	// flush, then pushes the eos_marker slot to the ring.
	pthread_mutex_t vpkt_lock;
	pthread_cond_t vpkt_has_data;
	pthread_cond_t vpkt_has_space;
	AVPacket *vpkt_ring[VIDEO_PKT_QUEUE_SIZE];
	u32 vpkt_gen[VIDEO_PKT_QUEUE_SIZE];   // see apkt_gen comment
	int vpkt_head;
	int vpkt_tail;
	int vpkt_count;
	bool vpkt_flush_requested;

	// Worker threads
	pthread_t worker;                // demuxer
	bool worker_running;
	pthread_t a_worker;              // audio decoder
	bool a_worker_running;
	pthread_t v_worker;              // video decoder
	bool v_worker_running;
} nx_video_t;

static JSClassID nx_video_class_id;

// QuickJS's `JSFreeArrayBufferDataFunc` is `(JSRuntime *rt, void *opaque,
// void *ptr)` — three args. Bare `js_free` is `(JSContext *ctx, void *ptr)`
// — two args. Casting `js_free` directly into the callback slot leaves the
// real `ptr` in arg #3 (which `js_free` ignores), passes the JSRuntime as
// `ctx`, and passes the opaque (a JSContext we registered) as `ptr` — so
// js_free tries to free the JSContext, corrupting QuickJS's allocator
// freelist on first GC after the ArrayBuffer goes out of scope. This was
// the root cause of the slice-2a frame-2 hang: frame-1's ArrayBuffer
// outlived `bridge.createBitmapFromRGBA`, then `new Uint8ClampedArray(f.data)`
// on frame 2 triggered the GC threshold, finalizer ran with the broken
// signature, allocator state went corrupt, next allocation infinite-looped
// walking the corrupted freelist. Wrap js_free_rt in a proper 3-arg
// callback — same pattern as canvas.c's `js_free_array_buffer`.
static void nx_video_free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	js_free_rt(rt, ptr);
}

// --- helpers ---------------------------------------------------------------

static nx_video_t *nx_get_video(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_video_class_id);
}

static u64 nx_now_ns(void) {
	return armTicksToNs(armGetSystemTick());
}

static void nx_video_set_error(nx_video_t *v, const char *msg) {
	if (v->error_msg) return;     // first error wins
	v->error_msg = strdup(msg ? msg : "unknown");
}

static AVBufferRef *nx_get_hw_dev_ctx(void) {
	pthread_mutex_lock(&g_hw_dev_lock);
	if (!g_hw_dev_attempted) {
		g_hw_dev_attempted = true;
		AVBufferRef *ref = NULL;
		int rc = av_hwdevice_ctx_create(&ref, AV_HWDEVICE_TYPE_NVTEGRA,
		                                NULL, NULL, 0);
		if (rc >= 0) {
			g_hw_dev_ctx = ref;
		}
	}
	AVBufferRef *out = g_hw_dev_ctx ? av_buffer_ref(g_hw_dev_ctx) : NULL;
	pthread_mutex_unlock(&g_hw_dev_lock);
	return out;
}

// --- custom AVIO over stdio ------------------------------------------------
// FFmpeg's protocol layer doesn't know about sdmc:/romfs:/. We always wrap
// the input in our own callbacks. See feedback-ffmpeg-avio-callbacks memo.

static int nx_avio_read(void *opaque, uint8_t *buf, int buf_size) {
	FILE *fp = (FILE *)opaque;
	size_t n = fread(buf, 1, (size_t)buf_size, fp);
	if (n == 0) return feof(fp) ? AVERROR_EOF : AVERROR(EIO);
	return (int)n;
}

static int64_t nx_avio_seek(void *opaque, int64_t offset, int whence) {
	FILE *fp = (FILE *)opaque;
	if (whence == AVSEEK_SIZE) {
		long cur = ftell(fp);
		if (fseek(fp, 0, SEEK_END) != 0) return AVERROR(EIO);
		long end = ftell(fp);
		fseek(fp, cur, SEEK_SET);
		return (int64_t)end;
	}
	if (fseek(fp, (long)offset, whence) != 0) return AVERROR(EIO);
	return (int64_t)ftell(fp);
}

// --- hw decoder get_format callback ----------------------------------------

static enum AVPixelFormat nx_hw_get_format(AVCodecContext *ctx,
                                           const enum AVPixelFormat *pix_fmts) {
	(void)ctx;
	for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
		if (*p == AV_PIX_FMT_NVTEGRA) return *p;
	}
	return pix_fmts[0];
}

// --- frame ring ------------------------------------------------------------

static void nx_video_ring_clear(nx_video_t *v) {
	for (int i = 0; i < VIDEO_RING_SIZE; i++) {
		v->slots[i].valid = false;
		v->slots[i].eos_marker = false;
	}
	v->head = 0;
	v->tail = 0;
	v->count = 0;
	// Invalidate any in-flight frame writes from the video thread that
	// captured the previous generation. Also wake any writer blocked in
	// ring_acquire_write so it can observe count=0 and the new gen.
	v->ring_gen++;
	pthread_cond_broadcast(&v->has_space);
}

// Worker holds v->lock when calling. Blocks until ring has space OR stop.
// Returns the slot to fill, or NULL if shutdown.
static nx_video_slot_t *nx_video_ring_acquire_write(nx_video_t *v) {
	while (v->count >= VIDEO_RING_SIZE && !v->stop_requested) {
		pthread_cond_wait(&v->has_space, &v->lock);
	}
	if (v->stop_requested) return NULL;
	return &v->slots[v->head];
}

// Worker has finished writing v->slots[v->head]; advance + signal.
static void nx_video_ring_commit_write(nx_video_t *v) {
	v->head = (v->head + 1) % VIDEO_RING_SIZE;
	v->count++;
	pthread_cond_signal(&v->has_data);
}

// --- slice-2b audio setup + wave-buffer ring -------------------------------
//
// `nx_video_audio_open` is called from `nx_video_open_internal` after the
// audio stream has been located. It allocates the per-decoder wave-buffer
// mempool, sets up swresample, configures the audrv voice, and registers
// the mempool with the shared driver. Returns NULL on success or an
// error string on failure — failure is non-fatal, video plays without
// audio.
//
// The wave-buffer ring is a fixed-size array of AUDIO_NUM_WAVE_BUFS
// entries; the worker writes into `next_wave_buf` and audrv plays them
// in submission order. We poll wb.state to find free slots before
// writing; the audrv state machine is `Free → Waiting → Playing → Done`,
// and we treat both `Free` and `Done` as available to reuse.

// Forward decls — definitions further down.
static void *nx_video_audio_thread(void *arg);
static void *nx_video_video_thread(void *arg);
static bool nx_video_ensure_sws(nx_video_t *v, enum AVPixelFormat src_fmt,
                                int w, int h);

static void nx_video_audio_reset_wave_bufs(nx_video_t *v) {
	for (int i = 0; i < AUDIO_NUM_WAVE_BUFS; i++) {
		nx_video_wave_buf_t *slot = &v->wave_bufs[i];
		memset(&slot->wb, 0, sizeof(AudioDriverWaveBuf));
		slot->wb.data_raw = slot->data;
		slot->wb.size = AUDIO_WAVE_BUF_BYTES;
		slot->wb.start_sample_offset = 0;
		slot->wb.end_sample_offset = 0;
		slot->wb.is_looping = false;
		slot->wb.state = AudioDriverWaveBufState_Free;
	}
	v->next_wave_buf = 0;
}

static const char *nx_video_audio_open(nx_video_t *v) {
	v->a_voice_id = -1;
	v->a_mempool_id = -1;
	v->audio_buf = NULL;
	v->swr_ctx = NULL;
	v->audio_clock_started = false;
	v->first_audio_pts_sec = 0.0;
	v->audio_voice_paused = false;

	if (v->a_stream_idx < 0 || !v->a_ctx) {
		return NULL; // no-op: no audio
	}

	if (!nx_audio_ensure_initialized()) {
		return "nx_audio_ensure_initialized failed (audren init)";
	}

	// Resampler: source params from a_ctx, dest is S16LE/48k/stereo.
	v->swr_ctx = swr_alloc();
	if (!v->swr_ctx) return "swr_alloc failed";

	AVChannelLayout out_layout;
	av_channel_layout_default(&out_layout, AUDIO_OUT_CHANNELS);
	AVChannelLayout in_layout;
	if (v->a_ctx->ch_layout.nb_channels > 0) {
		av_channel_layout_copy(&in_layout, &v->a_ctx->ch_layout);
	} else {
		av_channel_layout_default(&in_layout, v->a_ctx->ch_layout.nb_channels > 0
			? v->a_ctx->ch_layout.nb_channels : 2);
	}

	av_opt_set_chlayout(v->swr_ctx, "in_chlayout", &in_layout, 0);
	av_opt_set_int(v->swr_ctx, "in_sample_rate", v->a_ctx->sample_rate, 0);
	av_opt_set_sample_fmt(v->swr_ctx, "in_sample_fmt", v->a_ctx->sample_fmt, 0);
	av_opt_set_chlayout(v->swr_ctx, "out_chlayout", &out_layout, 0);
	av_opt_set_int(v->swr_ctx, "out_sample_rate", AUDIO_OUT_SAMPLE_RATE, 0);
	av_opt_set_sample_fmt(v->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	av_channel_layout_uninit(&in_layout);
	av_channel_layout_uninit(&out_layout);

	if (swr_init(v->swr_ctx) < 0) {
		return "swr_init failed";
	}

	// Per-decoder audio buffer. Sub-allocated from the shared main audrv
	// mempool (see audio.c's nx_audio_acquire_video_buf) so each video
	// doesn't burn an `audrvMemPoolAdd` slot — that capped pages at 1
	// video with audio under the previous design.
	v->audio_buf = nx_audio_acquire_video_buf(AUDIO_TOTAL_BUF_BYTES);
	if (!v->audio_buf) return "no free video audio slot";
	v->a_mempool_id = -1;  // sub-allocation lives in the main mempool;
	                       // nothing per-video to detach in destroy

	// Wire each wave-buf slot to its sub-region of audio_buf.
	for (int i = 0; i < AUDIO_NUM_WAVE_BUFS; i++) {
		v->wave_bufs[i].data = v->audio_buf + (size_t)i * AUDIO_WAVE_BUF_BYTES;
	}
	nx_video_audio_reset_wave_bufs(v);

	v->a_voice_id = nx_audio_acquire_voice();
	if (v->a_voice_id < 0) {
		// Release the slot before bailing or we leak it.
		nx_audio_release_video_buf(v->audio_buf);
		v->audio_buf = NULL;
		return "no free audrv voice";
	}

	nx_audio_lock();
	AudioDriver *drv = nx_audio_get_driver();
	bool voice_ok = audrvVoiceInit(drv, v->a_voice_id, AUDIO_OUT_CHANNELS,
	                               PcmFormat_Int16, AUDIO_OUT_SAMPLE_RATE);
	if (!voice_ok) {
		audrvUpdate(drv);
		nx_audio_unlock();
		nx_audio_release_voice(v->a_voice_id);
		v->a_voice_id = -1;
		nx_audio_release_video_buf(v->audio_buf);
		v->audio_buf = NULL;
		return "audrvVoiceInit failed";
	}
	audrvVoiceSetDestinationMix(drv, v->a_voice_id, AUDREN_FINAL_MIX_ID);
	audrvVoiceSetMixFactor(drv, v->a_voice_id, 1.0f, 0, 0);
	audrvVoiceSetMixFactor(drv, v->a_voice_id, 0.0f, 0, 1);
	audrvVoiceSetMixFactor(drv, v->a_voice_id, 0.0f, 1, 0);
	audrvVoiceSetMixFactor(drv, v->a_voice_id, 1.0f, 1, 1);
	audrvVoiceSetVolume(drv, v->a_voice_id, v->muted ? 0.0f : 1.0f);
	audrvUpdate(drv);
	nx_audio_unlock();

	// Spawn the audio decoder thread. It drains the audio packet queue
	// (fed by the main demuxer worker) and queues wave bufs into audrv.
	// Decoupling from the demuxer is what breaks the ring-full deadlock.
	if (pthread_create(&v->a_worker, NULL, nx_video_audio_thread, v) != 0) {
		nx_audio_release_voice(v->a_voice_id);
		v->a_voice_id = -1;
		nx_audio_release_video_buf(v->audio_buf);
		v->audio_buf = NULL;
		return "pthread_create audio thread failed";
	}
	v->a_worker_running = true;

	return NULL;
}

// Find the next wave-buf slot whose audrv state is Free or Done — that's
// the next one we can refill and queue. Returns NULL if all wave bufs
// are currently in flight (Waiting or Playing). Caller must hold
// nx_audio_lock since we read wb.state.
static nx_video_wave_buf_t *nx_video_audio_pick_free_wave_buf(nx_video_t *v) {
	for (int i = 0; i < AUDIO_NUM_WAVE_BUFS; i++) {
		int idx = (v->next_wave_buf + i) % AUDIO_NUM_WAVE_BUFS;
		AudioDriverWaveBufState st = v->wave_bufs[idx].wb.state;
		if (st == AudioDriverWaveBufState_Free ||
		    st == AudioDriverWaveBufState_Done) {
			v->next_wave_buf = (idx + 1) % AUDIO_NUM_WAVE_BUFS;
			return &v->wave_bufs[idx];
		}
	}
	return NULL;
}

// Decode one audio packet on the worker. The packet has already been
// matched to the audio stream by the dispatcher. Output samples are
// resampled to S16LE/48k/stereo and pushed to audrv as wave bufs.
// Returns 0 on success (including no-output cases like EAGAIN) and
// negative on fatal decode error.
static int nx_video_audio_decode_packet(nx_video_t *v, AVPacket *pkt,
                                        AVFrame *frame, u32 local_gen) {
	if (!v->a_ctx || !v->swr_ctx) {
		return 0;
	}
	int sr = avcodec_send_packet(v->a_ctx, pkt);
	if (sr < 0 && sr != AVERROR(EAGAIN) && sr != AVERROR_EOF) {
		return sr;
	}
	while (1) {
		int recv = avcodec_receive_frame(v->a_ctx, frame);
		if (recv == AVERROR(EAGAIN) || recv == AVERROR_EOF) break;
		if (recv < 0) {
			return recv;
		}

		// Stale-packet guard. MUST happen here, before nx_audio_lock
		// is acquired below — otherwise we'd be holding nx_audio_lock
		// while taking v->lock, while JS next_frame holds v->lock and
		// tries to take nx_audio_lock = classic lock-order inversion
		// deadlock. Doing the check up here keeps v->lock as the only
		// lock we ever hold during this comparison.
		pthread_mutex_lock(&v->lock);
		bool stale = (v->ring_gen != local_gen);
		pthread_mutex_unlock(&v->lock);
		if (stale) {
			av_frame_unref(frame);
			continue;
		}

		// Compute media-time PTS of this audio frame for AV-sync clock
		// anchor on the first one.
		AVRational tb = v->fmt_ctx->streams[v->a_stream_idx]->time_base;
		int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
		                  ? frame->best_effort_timestamp
		                  : frame->pts;
		double pts_sec = (pts == AV_NOPTS_VALUE)
		                     ? 0.0
		                     : (double)pts * av_q2d(tb);

		// Wait for a free wave buf. Worst case ~50 ms per slot * (N-1)
		// slots before next free — bounded sleep is fine. Worker also
		// checks stop_requested while sleeping. audrvUpdate must fire
		// each iteration: while we're stuck here, audrv has no other
		// heartbeat (the outer worker loop's audrvUpdate at the top of
		// while(1) doesn't iterate during this wait), so wave bufs would
		// sit Waiting forever and pick_free never finds a Done slot.
		nx_video_wave_buf_t *slot = NULL;
		for (int tries = 0; tries < 200; tries++) {
			nx_audio_lock();
			audrvUpdate(nx_audio_get_driver());
			slot = nx_video_audio_pick_free_wave_buf(v);
			if (slot) break;
			nx_audio_unlock();
			pthread_mutex_lock(&v->lock);
			bool stop = v->stop_requested || v->seek_requested;
			pthread_mutex_unlock(&v->lock);
			if (stop) return 0;
			svcSleepThread(5 * 1000000ULL); // 5 ms
		}
		if (!slot) {
			// Couldn't get a slot after ~1 s; bail without queuing.
			return 0;
		}
		// nx_audio_lock is currently held.

		// Convert. swr_convert wants pointers-to-channel-planes; for
		// interleaved S16 stereo there's one plane.
		uint8_t *dst_planes[1] = { slot->data };
		int out_samples = swr_convert(
			v->swr_ctx, dst_planes, AUDIO_WAVE_BUF_FRAMES,
			(const uint8_t **)frame->extended_data, frame->nb_samples);
		if (out_samples < 0) {
			nx_audio_unlock();
			return out_samples;
		}
		if (out_samples == 0) {
			nx_audio_unlock();
			av_frame_unref(frame);
			continue;
		}

		armDCacheFlush(slot->data,
			(size_t)out_samples * AUDIO_OUT_FRAME_BYTES);

		// Configure + submit the wave buf.
		memset(&slot->wb, 0, sizeof(AudioDriverWaveBuf));
		slot->wb.data_raw = slot->data;
		slot->wb.size = (size_t)out_samples * AUDIO_OUT_FRAME_BYTES;
		slot->wb.start_sample_offset = 0;
		slot->wb.end_sample_offset = (size_t)out_samples;
		slot->wb.is_looping = false;

		AudioDriver *drv = nx_audio_get_driver();

		// Late stale check, right before we commit to audrv. The early
		// check (before pick_free_wave_buf) can pass — gen=0 == 0 — but
		// then during the ~50ms pick_free_wave_buf retry, the demuxer
		// can run its loop branch (bump gen, reset audrv voice). At
		// that point the packet we're holding is now pre-wrap and its
		// pts must NOT anchor the post-wrap audio clock. We can't take
		// v->lock here (we hold nx_audio_lock — inversion with JS
		// next_frame). Atomic ACQUIRE-load pairs with the demuxer's
		// mutex-unlock release, making the bump visible.
		u32 cur_gen_now = __atomic_load_n(&v->ring_gen, __ATOMIC_ACQUIRE);
		if (cur_gen_now != local_gen) {
			nx_audio_unlock();
			av_frame_unref(frame);
			continue;
		}

		audrvVoiceAddWaveBuf(drv, v->a_voice_id, &slot->wb);
		if (!v->audio_clock_started) {
			// First buf queued — anchor the AV-sync clock here. Capture
			// the played-sample counter BEFORE audrvVoiceStart so any
			// cumulative count from previous play-throughs (e.g. on
			// <video loop> wrap, audrvVoiceStop doesn't zero the counter)
			// becomes the new zero point. `first_audio_pts_sec` then
			// maps the post-start delta back to media time.
			v->first_audio_pts_sec = pts_sec;
			v->audio_played_baseline =
				audrvVoiceGetPlayedSampleCount(drv, v->a_voice_id);
			audrvVoiceStart(drv, v->a_voice_id);
			v->audio_clock_started = true;
		}
		audrvUpdate(drv);
		nx_audio_unlock();

		av_frame_unref(frame);
	}
	return 0;
}

// --- audio packet queue + audio worker thread -----------------------------
//
// The demuxer (main worker) reads packets via av_read_frame and dispatches
// audio packets here. The audio worker thread pops them, calls
// nx_video_audio_decode_packet, and frees them. Decoupling means the audio
// thread keeps feeding audrv even when the main worker is blocked in
// nx_video_ring_acquire_write on a full video ring — the deadlock that
// kept biting us before this split.
//
// Caller protocol:
//   - push: demuxer clones the source AVPacket; on full ring it briefly
//     waits (with stop/seek escape) for space. Ownership transfers to
//     the queue.
//   - pop: audio worker takes ownership. On apkt_flush_requested it drains
//     the queue and clears the flag without decoding.

static void nx_video_apkt_queue_drain_locked(nx_video_t *v) {
	while (v->apkt_count > 0) {
		AVPacket *p = v->apkt_ring[v->apkt_tail];
		v->apkt_ring[v->apkt_tail] = NULL;
		v->apkt_tail = (v->apkt_tail + 1) % AUDIO_PKT_QUEUE_SIZE;
		v->apkt_count--;
		if (p) av_packet_free(&p);
	}
	v->apkt_head = 0;
	v->apkt_tail = 0;
}

// Push a cloned packet into the queue. Returns true on success, false on
// stop/seek (in which case the clone is freed here). Captures ring_gen at
// push time and stores it with the packet so a stale (pre-wrap) packet
// can be reliably distinguished by the consumer.
static bool nx_video_apkt_push(nx_video_t *v, AVPacket *src_pkt) {
	AVPacket *clone = av_packet_clone(src_pkt);
	if (!clone) return false;

	// Only the demuxer thread modifies ring_gen (via ring_clear in its
	// loop branch), and the demuxer is also the only push caller — so
	// the gen read here can't race with a bump. We still take v->lock
	// for memory-ordering safety on aarch64.
	pthread_mutex_lock(&v->lock);
	u32 gen_at_push = v->ring_gen;
	pthread_mutex_unlock(&v->lock);

	pthread_mutex_lock(&v->apkt_lock);
	while (v->apkt_count >= AUDIO_PKT_QUEUE_SIZE) {
		pthread_mutex_lock(&v->lock);
		bool bail = v->stop_requested || v->seek_requested ||
		            v->apkt_flush_requested;
		pthread_mutex_unlock(&v->lock);
		if (bail) {
			pthread_mutex_unlock(&v->apkt_lock);
			av_packet_free(&clone);
			return false;
		}
		pthread_cond_wait(&v->apkt_has_space, &v->apkt_lock);
	}
	v->apkt_ring[v->apkt_head] = clone;
	v->apkt_gen[v->apkt_head] = gen_at_push;
	v->apkt_head = (v->apkt_head + 1) % AUDIO_PKT_QUEUE_SIZE;
	v->apkt_count++;
	pthread_cond_signal(&v->apkt_has_data);
	pthread_mutex_unlock(&v->apkt_lock);
	return true;
}

// Pop one packet from the queue. Returns NULL on stop (final exit signal).
// On apkt_flush_requested, drains the queue and resets the audio codec /
// resampler before clearing the flag; then blocks again — caller loops.
// `*out_was_flush` (if non-NULL) is set true when a flush happened on this
// call. `*out_gen` (if non-NULL) is set to the ring_gen captured when this
// packet was pushed — caller uses it for stale-detection.
static AVPacket *nx_video_apkt_pop(nx_video_t *v, bool *out_was_flush, u32 *out_gen) {
	if (out_was_flush) *out_was_flush = false;
	if (out_gen) *out_gen = 0;
	pthread_mutex_lock(&v->apkt_lock);
	while (1) {
		if (v->apkt_flush_requested) {
			// Queue was already drained by the caller of
			// nx_video_apkt_request_flush (atomically with the flag
			// set). We only flush the codec + recycle swr here so any
			// post-flush packets the caller has since pushed survive.
			// Audio thread is the sole owner of a_ctx and swr_ctx
			// post-init, so it's safe to reset them here without an
			// extra lock.
			if (v->a_ctx) avcodec_flush_buffers(v->a_ctx);
			if (v->swr_ctx) {
				swr_close(v->swr_ctx);
				swr_init(v->swr_ctx);
			}
			v->apkt_flush_requested = false;
			if (out_was_flush) *out_was_flush = true;
			pthread_cond_broadcast(&v->apkt_has_space);
		}
		pthread_mutex_lock(&v->lock);
		bool stop = v->stop_requested;
		pthread_mutex_unlock(&v->lock);
		if (stop) {
			pthread_mutex_unlock(&v->apkt_lock);
			return NULL;
		}
		if (v->apkt_count > 0) {
			AVPacket *p = v->apkt_ring[v->apkt_tail];
			u32 pkt_gen = v->apkt_gen[v->apkt_tail];
			v->apkt_ring[v->apkt_tail] = NULL;
			v->apkt_tail = (v->apkt_tail + 1) % AUDIO_PKT_QUEUE_SIZE;
			v->apkt_count--;
			pthread_cond_signal(&v->apkt_has_space);
			pthread_mutex_unlock(&v->apkt_lock);
			if (out_gen) *out_gen = pkt_gen;
			return p;
		}
		pthread_cond_wait(&v->apkt_has_data, &v->apkt_lock);
	}
}

// Request the audio thread to discard everything currently queued AND
// reset its private AVFrame. Caller (JS seek path) holds v->lock briefly
// to set flags. The audio thread acts on apkt_flush_requested next time
// it loops.
static void nx_video_apkt_request_flush(nx_video_t *v) {
	pthread_mutex_lock(&v->apkt_lock);
	// Drain the queue HERE, atomically with the flag set. The audio
	// thread's flush handler then only flushes the codec — it MUST NOT
	// drop packets, because callers (demuxer loop branch, JS seek) push
	// fresh post-flush packets immediately after this returns, and any
	// race-window drain in the thread would drop them. (For audio,
	// dropping a packet is "only" an audible glitch; for the parallel
	// vpkt path, dropping the first post-seek keyframe wedges video
	// entirely until the next keyframe — fatal for single-GOP clips.)
	nx_video_apkt_queue_drain_locked(v);
	v->apkt_flush_requested = true;
	pthread_cond_broadcast(&v->apkt_has_data);
	pthread_cond_broadcast(&v->apkt_has_space);
	pthread_mutex_unlock(&v->apkt_lock);
}

static void *nx_video_audio_thread(void *arg) {
	nx_video_t *v = (nx_video_t *)arg;
	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		pthread_mutex_lock(&v->lock);
		if (!v->audio_error_msg) v->audio_error_msg = strdup("av_frame_alloc (audio thread) failed");
		pthread_mutex_unlock(&v->lock);
		return NULL;
	}

	while (1) {
		u32 local_gen = 0;
		AVPacket *pkt = nx_video_apkt_pop(v, NULL, &local_gen);
		if (!pkt) break;     // stop_requested

		// `local_gen` is the ring_gen at PUSH time. If a wrap/seek
		// happened between push and now, audio_decode_packet's
		// stale-check skips wave-buf queueing for this packet.
		int arc = nx_video_audio_decode_packet(v, pkt, frame, local_gen);
		if (arc < 0) {
			pthread_mutex_lock(&v->lock);
			if (!v->audio_error_msg)
				v->audio_error_msg = strdup("audio decode failed");
			pthread_mutex_unlock(&v->lock);
		}
		av_packet_free(&pkt);
	}

	// Audio-only mode: no video thread will push the end-of-stream
	// marker to the ring, so do it here on exit. JS sees ended=true
	// via nextFrame and stops polling; audrv keeps draining the
	// already-queued wave-bufs naturally in the background.
	if (!v->used_video) {
		pthread_mutex_lock(&v->lock);
		if (v->count < VIDEO_RING_SIZE) {
			nx_video_slot_t *s = &v->slots[v->head];
			s->valid = true;
			s->eos_marker = true;
			s->rgba = NULL;
			s->width = s->height = 0;
			nx_video_ring_commit_write(v);
		}
		v->ended = true;
		pthread_mutex_unlock(&v->lock);
	}

	av_frame_free(&frame);
	return NULL;
}

// --- video packet queue + video worker thread ------------------------------
//
// Mirrors the apkt queue. Demuxer pushes raw video AVPackets; video worker
// pops, send_packet's, drains frames, pushes them to the ring. EOS is
// represented by an empty (size=0, data=NULL) AVPacket clone — see
// nx_video_vpkt_push_eos.

static void nx_video_vpkt_queue_drain_locked(nx_video_t *v) {
	while (v->vpkt_count > 0) {
		AVPacket *p = v->vpkt_ring[v->vpkt_tail];
		v->vpkt_ring[v->vpkt_tail] = NULL;
		v->vpkt_tail = (v->vpkt_tail + 1) % VIDEO_PKT_QUEUE_SIZE;
		v->vpkt_count--;
		if (p) av_packet_free(&p);
	}
	v->vpkt_head = 0;
	v->vpkt_tail = 0;
}

static bool nx_video_vpkt_push(nx_video_t *v, AVPacket *src_pkt) {
	AVPacket *clone = av_packet_clone(src_pkt);
	if (!clone) return false;

	pthread_mutex_lock(&v->lock);
	u32 gen_at_push = v->ring_gen;
	pthread_mutex_unlock(&v->lock);

	pthread_mutex_lock(&v->vpkt_lock);
	while (v->vpkt_count >= VIDEO_PKT_QUEUE_SIZE) {
		pthread_mutex_lock(&v->lock);
		bool bail = v->stop_requested || v->seek_requested ||
		            v->vpkt_flush_requested;
		pthread_mutex_unlock(&v->lock);
		if (bail) {
			pthread_mutex_unlock(&v->vpkt_lock);
			av_packet_free(&clone);
			return false;
		}
		pthread_cond_wait(&v->vpkt_has_space, &v->vpkt_lock);
	}
	v->vpkt_ring[v->vpkt_head] = clone;
	v->vpkt_gen[v->vpkt_head] = gen_at_push;
	v->vpkt_head = (v->vpkt_head + 1) % VIDEO_PKT_QUEUE_SIZE;
	v->vpkt_count++;
	pthread_cond_signal(&v->vpkt_has_data);
	pthread_mutex_unlock(&v->vpkt_lock);
	return true;
}

// Push an EOS sentinel: an empty AVPacket (size=0, data=NULL). The video
// thread treats this as "send NULL to codec, drain final frames, push the
// ring eos_marker slot, exit-or-idle".
static bool nx_video_vpkt_push_eos(nx_video_t *v) {
	AVPacket *eos = av_packet_alloc();
	if (!eos) return false;
	// eos->data == NULL, eos->size == 0 already (av_packet_alloc zeroes).

	pthread_mutex_lock(&v->vpkt_lock);
	while (v->vpkt_count >= VIDEO_PKT_QUEUE_SIZE) {
		pthread_mutex_lock(&v->lock);
		bool bail = v->stop_requested || v->seek_requested;
		pthread_mutex_unlock(&v->lock);
		if (bail) {
			pthread_mutex_unlock(&v->vpkt_lock);
			av_packet_free(&eos);
			return false;
		}
		pthread_cond_wait(&v->vpkt_has_space, &v->vpkt_lock);
	}
	v->vpkt_ring[v->vpkt_head] = eos;
	v->vpkt_head = (v->vpkt_head + 1) % VIDEO_PKT_QUEUE_SIZE;
	v->vpkt_count++;
	pthread_cond_signal(&v->vpkt_has_data);
	pthread_mutex_unlock(&v->vpkt_lock);
	return true;
}

static AVPacket *nx_video_vpkt_pop(nx_video_t *v, bool *out_was_flush, u32 *out_gen) {
	if (out_was_flush) *out_was_flush = false;
	if (out_gen) *out_gen = 0;
	pthread_mutex_lock(&v->vpkt_lock);
	while (1) {
		if (v->vpkt_flush_requested) {
			// Queue was already drained by the caller of
			// nx_video_vpkt_request_flush. We only flush the codec
			// here so any post-flush packets the caller has since
			// pushed (including the all-important post-seek keyframe)
			// survive. v_ctx is single-threaded — only this thread
			// touches it post-init.
			if (v->v_ctx) avcodec_flush_buffers(v->v_ctx);
			v->vpkt_flush_requested = false;
			if (out_was_flush) *out_was_flush = true;
			pthread_cond_broadcast(&v->vpkt_has_space);
		}
		pthread_mutex_lock(&v->lock);
		bool stop = v->stop_requested;
		pthread_mutex_unlock(&v->lock);
		if (stop) {
			pthread_mutex_unlock(&v->vpkt_lock);
			return NULL;
		}
		if (v->vpkt_count > 0) {
			AVPacket *p = v->vpkt_ring[v->vpkt_tail];
			u32 pkt_gen = v->vpkt_gen[v->vpkt_tail];
			v->vpkt_ring[v->vpkt_tail] = NULL;
			v->vpkt_tail = (v->vpkt_tail + 1) % VIDEO_PKT_QUEUE_SIZE;
			v->vpkt_count--;
			pthread_cond_signal(&v->vpkt_has_space);
			pthread_mutex_unlock(&v->vpkt_lock);
			if (out_gen) *out_gen = pkt_gen;
			return p;
		}
		pthread_cond_wait(&v->vpkt_has_data, &v->vpkt_lock);
	}
}

static void nx_video_vpkt_request_flush(nx_video_t *v) {
	pthread_mutex_lock(&v->vpkt_lock);
	// See nx_video_apkt_request_flush for the rationale — caller drains
	// queue atomically with the flag, thread's handler flushes codec
	// only. Critical for video: if the thread were to drain post-seek
	// packets, the first post-seek KEYFRAME could be dropped, and
	// without a keyframe the codec can't decode subsequent P/B frames
	// until the next keyframe (which for single-GOP clips like
	// test.mp4 doesn't come until end-of-file).
	nx_video_vpkt_queue_drain_locked(v);
	v->vpkt_flush_requested = true;
	pthread_cond_broadcast(&v->vpkt_has_data);
	pthread_cond_broadcast(&v->vpkt_has_space);
	pthread_mutex_unlock(&v->vpkt_lock);
}

static void *nx_video_video_thread(void *arg) {
	nx_video_t *v = (nx_video_t *)arg;
	AVFrame *frame = av_frame_alloc();
	AVFrame *sw_frame = av_frame_alloc();
	if (!frame || !sw_frame) {
		pthread_mutex_lock(&v->lock);
		nx_video_set_error(v, "av_frame_alloc failed (video thread)");
		pthread_mutex_unlock(&v->lock);
		goto out;
	}

	while (1) {
		bool was_flush = false;
		u32 local_gen = 0;
		AVPacket *pkt = nx_video_vpkt_pop(v, &was_flush, &local_gen);
		(void)was_flush;
		if (!pkt) {
			break;
		}

		// `local_gen` is the ring_gen captured at PUSH time (stored
		// with the packet). Compared to current ring_gen at commit
		// time: mismatch = packet was pushed before a wrap/seek, so
		// its decoded frame is stale and must not pollute the
		// post-wrap ring with high-pts content that JS pacing would
		// never deliver.

		bool is_eos = (pkt->data == NULL && pkt->size == 0);
		int sr = avcodec_send_packet(v->v_ctx, is_eos ? NULL : pkt);
		av_packet_free(&pkt);
		if (sr < 0 && sr != AVERROR(EAGAIN) && sr != AVERROR_EOF) {
			pthread_mutex_lock(&v->lock);
			nx_video_set_error(v, "avcodec_send_packet failed (video thread)");
			pthread_mutex_unlock(&v->lock);
			break;
		}

		while (1) {
			int recv = avcodec_receive_frame(v->v_ctx, frame);
			if (recv == AVERROR(EAGAIN)) break;
			if (recv == AVERROR_EOF) {
				pthread_mutex_lock(&v->lock);
				// Push an EOS marker so JS sees end-of-stream.
				if (v->count < VIDEO_RING_SIZE) {
					nx_video_slot_t *s = &v->slots[v->head];
					s->valid = true;
					s->eos_marker = true;
					s->rgba = NULL;
					s->width = s->height = 0;
					nx_video_ring_commit_write(v);
				}
				v->ended = true;
				pthread_mutex_unlock(&v->lock);
				goto out;
			}
			if (recv < 0) {
				pthread_mutex_lock(&v->lock);
				nx_video_set_error(v, "avcodec_receive_frame failed");
				pthread_mutex_unlock(&v->lock);
				goto out;
			}

			AVFrame *src = frame;
			if (frame->format == AV_PIX_FMT_NVTEGRA) {
				av_frame_unref(sw_frame);
				int t = av_hwframe_transfer_data(sw_frame, frame, 0);
				if (t < 0) {
					pthread_mutex_lock(&v->lock);
					nx_video_set_error(v, "av_hwframe_transfer_data failed");
					pthread_mutex_unlock(&v->lock);
					goto out;
				}
				src = sw_frame;
			}

			if (!nx_video_ensure_sws(v, (enum AVPixelFormat)src->format,
			                         src->width, src->height)) {
				pthread_mutex_lock(&v->lock);
				nx_video_set_error(v, "sws_getContext failed");
				pthread_mutex_unlock(&v->lock);
				goto out;
			}

			pthread_mutex_lock(&v->lock);
			nx_video_slot_t *slot = nx_video_ring_acquire_write(v);
			if (!slot) {
				// stop_requested fired while waiting for ring space.
				// This commonly happens at end-of-stream: demuxer
				// pushed vpkt_eos + set stop_requested, but the ring
				// is still full of frames that JS hasn't popped (the
				// audio-clock pacing freezes once audrv drains, so
				// late frames hang). Without flipping v->ended here,
				// JS pacing's drain-mode bypass never engages and the
				// ring stays stuck → currentPts caps at the last
				// successfully popped frame's PTS. Flip ended so JS
				// can drain the rest and reach the EOS sentinel.
				v->ended = true;
				pthread_cond_broadcast(&v->has_data);
				pthread_mutex_unlock(&v->lock);
				av_frame_unref(frame);
				av_frame_unref(sw_frame);
				goto out;
			}
			// Stale-frame guard. We hold v->lock here so the comparison
			// is atomic with whatever might have just bumped ring_gen
			// (only nx_video_ring_clear bumps it, and it also runs under
			// v->lock). Mismatch → discard, jump back to the outer loop
			// where the next packet pop refreshes local_gen + sees the
			// vpkt_flush flag if the demuxer requested one.
			if (v->ring_gen != local_gen) {
				pthread_mutex_unlock(&v->lock);
				av_frame_unref(frame);
				av_frame_unref(sw_frame);
				break;
			}
			if (!slot->rgba ||
			    slot->width != v->width || slot->height != v->height) {
				free(slot->rgba);
				slot->rgba = malloc((size_t)v->width * v->height * 4);
				slot->width = v->width;
				slot->height = v->height;
			}
			if (!slot->rgba) {
				nx_video_set_error(v, "out of memory for frame buffer");
				pthread_mutex_unlock(&v->lock);
				goto out;
			}
			uint8_t *dst[4] = { slot->rgba, NULL, NULL, NULL };
			int dst_linesize[4] = { v->width * 4, 0, 0, 0 };
			sws_scale(v->sws_ctx,
			          (const uint8_t * const *)src->data, src->linesize,
			          0, src->height, dst, dst_linesize);

			AVRational tb = v->fmt_ctx->streams[v->v_stream_idx]->time_base;
			int64_t pts = src->best_effort_timestamp != AV_NOPTS_VALUE
			                  ? src->best_effort_timestamp
			                  : src->pts;
			slot->pts_sec = (pts == AV_NOPTS_VALUE)
			                    ? 0.0
			                    : (double)pts * av_q2d(tb);
			slot->valid = true;
			slot->eos_marker = false;
			nx_video_ring_commit_write(v);
			pthread_mutex_unlock(&v->lock);

			av_frame_unref(frame);
			av_frame_unref(sw_frame);
		}
	}

out:
	if (frame) av_frame_free(&frame);
	if (sw_frame) av_frame_free(&sw_frame);
	return NULL;
}

// --- swscale management ----------------------------------------------------
// We create the SwsContext lazily once we see the first decoded frame's
// real (sw) pixel format. For HW frames that's the post-transfer NV12.
// For SW frames it's whatever the codec produces (usually YUV420P).

static bool nx_video_ensure_sws(nx_video_t *v, enum AVPixelFormat src_fmt,
                                int w, int h) {
	if (v->sws_ctx && v->sws_src_fmt == src_fmt &&
	    v->sws_src_w == w && v->sws_src_h == h) {
		return true;
	}
	if (v->sws_ctx) {
		sws_freeContext(v->sws_ctx);
		v->sws_ctx = NULL;
	}
	v->sws_ctx = sws_getContext(w, h, src_fmt,
	                            w, h, AV_PIX_FMT_RGBA,
	                            SWS_BILINEAR, NULL, NULL, NULL);
	if (!v->sws_ctx) return false;
	v->sws_src_fmt = src_fmt;
	v->sws_src_w = w;
	v->sws_src_h = h;
	return true;
}

// --- demuxer worker --------------------------------------------------------
//
// Three-thread architecture (post-2026-05-27): this thread is now purely
// the demuxer. It only does `av_read_frame` and dispatches packets to the
// apkt or vpkt queue. Audio decode runs in `nx_video_audio_thread`; video
// decode + ring push runs in `nx_video_video_thread`. Decoupling the
// demuxer from the video decoder is what finally breaks the rate-mismatch
// deadlock: when the video ring fills, the video thread blocks in
// nx_video_ring_acquire_write, but the demuxer keeps reading packets and
// the audio thread keeps feeding audrv — so the audio clock advances, JS
// pops, and the video thread unblocks naturally.

static void *nx_video_worker(void *arg) {
	nx_video_t *v = (nx_video_t *)arg;
	AVPacket *pkt = av_packet_alloc();
	if (!pkt) {
		pthread_mutex_lock(&v->lock);
		nx_video_set_error(v, "av_packet_alloc failed");
		pthread_mutex_unlock(&v->lock);
		goto out;
	}

	while (1) {
		pthread_mutex_lock(&v->lock);
		while (v->paused && !v->stop_requested && !v->seek_requested) {
			pthread_cond_wait(&v->has_space, &v->lock);
		}
		if (v->stop_requested) {
			pthread_mutex_unlock(&v->lock);
			break;
		}
		if (v->seek_requested) {
			double target = v->seek_target_sec;
			v->seek_requested = false;
			// Ring clear happens on the JS seek path under v->lock already,
			// but re-clear here to cover the case where the video worker
			// pushed a frame after the JS pre-seek clear but before
			// vpkt_flush took effect.
			nx_video_ring_clear(v);
			v->ended = false;
			pthread_mutex_unlock(&v->lock);

			int64_t ts = (int64_t)(target * AV_TIME_BASE);
			av_seek_frame(v->fmt_ctx, -1, ts, AVSEEK_FLAG_BACKWARD);
			// Audio thread + video thread each flush their own codec when
			// they observe their queue's flush flag (set by JS seek path
			// before we got here).
			continue;
		}
		pthread_mutex_unlock(&v->lock);

		// Drive audrv state forward at demuxer cadence too. JS next_frame
		// kicks it at JS-tick rate; the audio worker kicks it during
		// pick_free_wave_buf retries. This extra kick is cheap insurance.
		if (v->used_audio && v->a_voice_id >= 0) {
			nx_audio_lock();
			audrvUpdate(nx_audio_get_driver());
			nx_audio_unlock();
		}

		int rr = av_read_frame(v->fmt_ctx, pkt);
		if (rr == AVERROR_EOF) {
			if (v->loop) {
				// <video loop>: re-seek to start instead of ending.
				// Mirrors what JS-side nx_video_seek does — clear the
				// ring + reset audrv voice + request flushes on both
				// decoder queues so each thread re-flushes its codec
				// next pop. We do NOT push the EOS sentinel; both
				// decoder threads keep running and pick up the
				// post-seek packets.
				pthread_mutex_lock(&v->lock);
				nx_video_ring_clear(v);
				v->ended = false;
				v->clock_started = false;
				pthread_mutex_unlock(&v->lock);
				if (v->used_audio && v->a_voice_id >= 0) {
					nx_audio_lock();
					AudioDriver *drv = nx_audio_get_driver();
					audrvVoiceStop(drv, v->a_voice_id);
					audrvUpdate(drv);
					nx_video_audio_reset_wave_bufs(v);
					v->audio_clock_started = false;
					v->audio_voice_paused = false;
					nx_audio_unlock();
					nx_video_apkt_request_flush(v);
				}
				nx_video_vpkt_request_flush(v);
				(void)av_seek_frame(v->fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
				continue;
			}
			// Non-loop: signal EOS to the video thread; audio just
			// stops queueing. Audio-only files (no video thread)
			// instead get an EOS marker pushed by the audio thread
			// when it drains and exits (see nx_video_audio_thread).
			if (v->used_video) {
				(void)nx_video_vpkt_push_eos(v);
			}
			pthread_mutex_lock(&v->lock);
			v->stop_requested = true;     // demuxer is done
			pthread_mutex_unlock(&v->lock);
			break;
		}
		if (rr < 0) {
			pthread_mutex_lock(&v->lock);
			nx_video_set_error(v, "av_read_frame error");
			v->stop_requested = true;
			pthread_mutex_unlock(&v->lock);
			break;
		}

		// Dispatch to the appropriate decoder queue. Non-video / non-audio
		// packets (subtitles, data) are discarded.
		if (pkt->stream_index == v->v_stream_idx) {
			(void)nx_video_vpkt_push(v, pkt);
		} else if (v->used_audio && pkt->stream_index == v->a_stream_idx) {
			(void)nx_video_apkt_push(v, pkt);
		}
		av_packet_unref(pkt);
	}

out:
	if (pkt) av_packet_free(&pkt);
	pthread_mutex_lock(&v->lock);
	pthread_cond_broadcast(&v->has_data);
	pthread_mutex_unlock(&v->lock);
	return NULL;
}

// --- open / close ----------------------------------------------------------

static const char *nx_video_open_internal(nx_video_t *v, const char *path,
                                          bool want_hw) {
	v->src_fp = fopen(path, "rb");
	if (!v->src_fp) return "fopen failed (file missing?)";

	v->avio_buf = av_malloc(VIDEO_AVIO_BUF_SIZE);
	if (!v->avio_buf) return "av_malloc avio buf";
	v->avio_ctx = avio_alloc_context(v->avio_buf, VIDEO_AVIO_BUF_SIZE, 0,
	                                 v->src_fp, nx_avio_read, NULL,
	                                 nx_avio_seek);
	if (!v->avio_ctx) return "avio_alloc_context";

	v->fmt_ctx = avformat_alloc_context();
	if (!v->fmt_ctx) return "avformat_alloc_context";
	v->fmt_ctx->pb = v->avio_ctx;
	v->fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

	int rc = avformat_open_input(&v->fmt_ctx, NULL, NULL, NULL);
	if (rc < 0) return "avformat_open_input failed";
	rc = avformat_find_stream_info(v->fmt_ctx, NULL);
	if (rc < 0) return "avformat_find_stream_info failed";

	v->v_stream_idx = av_find_best_stream(v->fmt_ctx, AVMEDIA_TYPE_VIDEO,
	                                      -1, -1, NULL, 0);
	if (v->v_stream_idx >= 0) {
		AVStream *st = v->fmt_ctx->streams[v->v_stream_idx];
		const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec) return "no decoder for video codec";

		v->v_ctx = avcodec_alloc_context3(dec);
		if (!v->v_ctx) return "avcodec_alloc_context3";
		avcodec_parameters_to_context(v->v_ctx, st->codecpar);

		if (want_hw) {
			AVBufferRef *hw_ref = nx_get_hw_dev_ctx();
			if (hw_ref) {
				v->hw_dev_ref = hw_ref;
				v->v_ctx->hw_device_ctx = av_buffer_ref(hw_ref);
				v->v_ctx->get_format = nx_hw_get_format;
				v->used_hw = true;
			}
		}

		rc = avcodec_open2(v->v_ctx, dec, NULL);
		if (rc < 0) return "avcodec_open2 failed";

		v->width = v->v_ctx->width;
		v->height = v->v_ctx->height;
		v->used_video = true;
	}
	// Whether we have video or not, fmt_ctx->duration is set by
	// avformat_find_stream_info for audio-only files just the same.
	if (v->fmt_ctx->duration > 0) {
		v->duration_sec = (double)v->fmt_ctx->duration / (double)AV_TIME_BASE;
	}

	// Slice 2b: locate + open the audio stream, set up swresample +
	// audrv voice. Non-fatal: if anything fails we record the reason in
	// v->audio_error_msg and continue with video-only playback (slice
	// 2a wall-clock pacing).
	// Slice 2b followup #6: noAudio short-circuit — the caller (poster
	// preview pass) wants the video stream only, with zero audrv state
	// transitions on construction.
	if (v->no_audio) {
		return v->used_video ? NULL : "no playable video or audio stream";
	}
	v->a_stream_idx = av_find_best_stream(v->fmt_ctx, AVMEDIA_TYPE_AUDIO,
	                                       -1, v->v_stream_idx, NULL, 0);
	if (v->a_stream_idx >= 0) {
		AVStream *a_st = v->fmt_ctx->streams[v->a_stream_idx];
		const AVCodec *a_dec = avcodec_find_decoder(a_st->codecpar->codec_id);
		if (!a_dec) {
			v->audio_error_msg = strdup("no decoder for audio codec");
			v->a_stream_idx = -1;
		} else {
			v->a_ctx = avcodec_alloc_context3(a_dec);
			if (!v->a_ctx) {
				v->audio_error_msg = strdup("avcodec_alloc_context3 (audio) failed");
				v->a_stream_idx = -1;
			} else if (avcodec_parameters_to_context(v->a_ctx, a_st->codecpar) < 0
			           || avcodec_open2(v->a_ctx, a_dec, NULL) < 0) {
				v->audio_error_msg = strdup("avcodec_open2 (audio) failed");
				avcodec_free_context(&v->a_ctx);
				v->a_stream_idx = -1;
			} else {
				const char *aerr = nx_video_audio_open(v);
				if (aerr) {
					v->audio_error_msg = strdup(aerr);
					if (v->swr_ctx) swr_free(&v->swr_ctx);
					// nx_video_audio_open is responsible for releasing
					// its own audio_buf + voice on error paths. We just
					// drop the remaining FFmpeg side-state here.
					avcodec_free_context(&v->a_ctx);
					v->a_stream_idx = -1;
				} else {
					v->used_audio = true;
				}
			}
		}
	}
	// At least one playable stream is required. A file with neither
	// usable video nor usable audio (subtitles-only, encrypted, etc.)
	// can't drive a <video> element — surface that as an open error.
	if (!v->used_video && !v->used_audio) {
		return "no playable video or audio stream";
	}
	return NULL;
}

static void nx_video_destroy(JSRuntime *rt, nx_video_t *v) {
	if (!v) return;

	if (v->worker_running || v->a_worker_running || v->v_worker_running) {
		pthread_mutex_lock(&v->lock);
		v->stop_requested = true;
		pthread_cond_broadcast(&v->has_space);
		pthread_cond_broadcast(&v->has_data);
		pthread_mutex_unlock(&v->lock);
		// Wake all queue conds — any of the three threads may be parked
		// in a cond_wait (demuxer in apkt/vpkt_push, audio worker in
		// apkt_pop, video worker in vpkt_pop). Without these broadcasts
		// the joins below would hang.
		pthread_mutex_lock(&v->apkt_lock);
		pthread_cond_broadcast(&v->apkt_has_data);
		pthread_cond_broadcast(&v->apkt_has_space);
		pthread_mutex_unlock(&v->apkt_lock);
		pthread_mutex_lock(&v->vpkt_lock);
		pthread_cond_broadcast(&v->vpkt_has_data);
		pthread_cond_broadcast(&v->vpkt_has_space);
		pthread_mutex_unlock(&v->vpkt_lock);

		if (v->worker_running) {
			pthread_join(v->worker, NULL);
			v->worker_running = false;
		}
		// Video worker must exit BEFORE we free the ring/v_ctx/etc.
		if (v->v_worker_running) {
			pthread_join(v->v_worker, NULL);
			v->v_worker_running = false;
		}
		// Audio worker must exit BEFORE we tear down audrv voice / mempool
		// / swr / a_ctx below, because its decode path reads all of those.
		if (v->a_worker_running) {
			pthread_join(v->a_worker, NULL);
			v->a_worker_running = false;
		}
	}

	// Drain any AVPackets still in either queue (workers may have exited
	// with packets remaining if stop fired while the queue was non-empty).
	pthread_mutex_lock(&v->apkt_lock);
	nx_video_apkt_queue_drain_locked(v);
	pthread_mutex_unlock(&v->apkt_lock);
	pthread_mutex_lock(&v->vpkt_lock);
	nx_video_vpkt_queue_drain_locked(v);
	pthread_mutex_unlock(&v->vpkt_lock);

	for (int i = 0; i < VIDEO_RING_SIZE; i++) {
		free(v->slots[i].rgba);
	}

	// Slice 2b audio teardown — must run before sws_freeContext etc so
	// the voice is stopped before its wave-buf-backing region is released.
	// 2026-05-27: per-video audrv mempool is gone (we sub-allocate from
	// the shared main mempool now — see audio.c's nx_audio_acquire_video_buf),
	// so no per-video MemPoolDetach/Remove is needed; just stop the
	// voice and release the slot.
	if (v->a_voice_id >= 0) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		audrvVoiceStop(drv, v->a_voice_id);
		audrvUpdate(drv);
		nx_audio_unlock();
		nx_audio_release_voice(v->a_voice_id);
		v->a_voice_id = -1;
	}
	if (v->audio_buf) {
		nx_audio_release_video_buf(v->audio_buf);
		v->audio_buf = NULL;
	}
	if (v->swr_ctx) swr_free(&v->swr_ctx);
	if (v->a_ctx) avcodec_free_context(&v->a_ctx);
	free(v->audio_error_msg);

	if (v->sws_ctx) sws_freeContext(v->sws_ctx);
	if (v->v_ctx) avcodec_free_context(&v->v_ctx);
	if (v->fmt_ctx) avformat_close_input(&v->fmt_ctx);
	if (v->avio_ctx) {
		av_freep(&v->avio_ctx->buffer);
		avio_context_free(&v->avio_ctx);
	}
	if (v->src_fp) fclose(v->src_fp);
	if (v->hw_dev_ref) av_buffer_unref(&v->hw_dev_ref);
	free(v->error_msg);

	pthread_mutex_destroy(&v->lock);
	pthread_cond_destroy(&v->has_space);
	pthread_cond_destroy(&v->has_data);
	pthread_mutex_destroy(&v->apkt_lock);
	pthread_cond_destroy(&v->apkt_has_data);
	pthread_cond_destroy(&v->apkt_has_space);
	pthread_mutex_destroy(&v->vpkt_lock);
	pthread_cond_destroy(&v->vpkt_has_data);
	pthread_cond_destroy(&v->vpkt_has_space);
	js_free_rt(rt, v);
}

static void nx_video_finalizer(JSRuntime *rt, JSValue val) {
	nx_video_t *v = JS_GetOpaque(val, nx_video_class_id);
	nx_video_destroy(rt, v);
}

// --- JS-callable functions -------------------------------------------------

static JSValue nx_video_new(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
	if (argc < 1) return JS_ThrowTypeError(ctx, "videoDecoderNew(url, opts?)");

	const char *url = JS_ToCString(ctx, argv[0]);
	if (!url) return JS_EXCEPTION;
	bool want_hw = true;
	bool want_loop = false;
	bool want_muted = false;
	bool want_no_audio = false;
	if (argc >= 2 && JS_IsObject(argv[1])) {
		JSValue hw = JS_GetPropertyStr(ctx, argv[1], "hwAccel");
		if (JS_IsBool(hw)) want_hw = JS_ToBool(ctx, hw);
		JS_FreeValue(ctx, hw);
		JSValue loop = JS_GetPropertyStr(ctx, argv[1], "loop");
		if (JS_IsBool(loop)) want_loop = JS_ToBool(ctx, loop);
		JS_FreeValue(ctx, loop);
		JSValue muted = JS_GetPropertyStr(ctx, argv[1], "muted");
		if (JS_IsBool(muted)) want_muted = JS_ToBool(ctx, muted);
		JS_FreeValue(ctx, muted);
		JSValue noaudio = JS_GetPropertyStr(ctx, argv[1], "noAudio");
		if (JS_IsBool(noaudio)) want_no_audio = JS_ToBool(ctx, noaudio);
		JS_FreeValue(ctx, noaudio);
	}

	nx_video_t *v = js_mallocz(ctx, sizeof(nx_video_t));
	if (!v) { JS_FreeCString(ctx, url); return JS_EXCEPTION; }

	pthread_mutex_init(&v->lock, NULL);
	pthread_cond_init(&v->has_space, NULL);
	pthread_cond_init(&v->has_data, NULL);
	pthread_mutex_init(&v->apkt_lock, NULL);
	pthread_cond_init(&v->apkt_has_data, NULL);
	pthread_cond_init(&v->apkt_has_space, NULL);
	pthread_mutex_init(&v->vpkt_lock, NULL);
	pthread_cond_init(&v->vpkt_has_data, NULL);
	pthread_cond_init(&v->vpkt_has_space, NULL);
	v->paused = true;     // start paused; JS calls play() to roll
	v->loop = want_loop;
	v->muted = want_muted;
	v->no_audio = want_no_audio;
	v->a_stream_idx = -1;
	v->a_voice_id = -1;
	v->a_mempool_id = -1;

	const char *err = nx_video_open_internal(v, url, want_hw);
	JS_FreeCString(ctx, url);
	if (err) {
		JSValue ex = JS_ThrowInternalError(ctx, "VideoDecoder open: %s", err);
		nx_video_destroy(JS_GetRuntime(ctx), v);
		return ex;
	}

	// Skip the video worker for audio-only files. The demuxer dispatch
	// already gates by used_video, so no vpkt traffic would reach the
	// thread; and on EOF the audio thread is the one that pushes the
	// ring eos_marker.
	if (v->used_video) {
		if (pthread_create(&v->v_worker, NULL, nx_video_video_thread, v) != 0) {
			JSValue ex = JS_ThrowInternalError(ctx, "pthread_create video thread failed");
			nx_video_destroy(JS_GetRuntime(ctx), v);
			return ex;
		}
		v->v_worker_running = true;
	}

	if (pthread_create(&v->worker, NULL, nx_video_worker, v) != 0) {
		JSValue ex = JS_ThrowInternalError(ctx, "pthread_create demuxer failed");
		nx_video_destroy(JS_GetRuntime(ctx), v);
		return ex;
	}
	v->worker_running = true;

	JSValue obj = JS_NewObjectClass(ctx, nx_video_class_id);
	if (JS_IsException(obj)) {
		nx_video_destroy(JS_GetRuntime(ctx), v);
		return obj;
	}
	JS_SetOpaque(obj, v);
	return obj;
}

static JSValue nx_video_close(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_UNDEFINED;     // already finalized

	pthread_mutex_lock(&v->lock);
	v->stop_requested = true;
	pthread_cond_broadcast(&v->has_space);
	pthread_cond_broadcast(&v->has_data);
	pthread_mutex_unlock(&v->lock);
	pthread_mutex_lock(&v->apkt_lock);
	pthread_cond_broadcast(&v->apkt_has_data);
	pthread_cond_broadcast(&v->apkt_has_space);
	pthread_mutex_unlock(&v->apkt_lock);
	pthread_mutex_lock(&v->vpkt_lock);
	pthread_cond_broadcast(&v->vpkt_has_data);
	pthread_cond_broadcast(&v->vpkt_has_space);
	pthread_mutex_unlock(&v->vpkt_lock);
	if (v->worker_running) {
		pthread_join(v->worker, NULL);
		v->worker_running = false;
	}
	if (v->v_worker_running) {
		pthread_join(v->v_worker, NULL);
		v->v_worker_running = false;
	}
	if (v->a_worker_running) {
		pthread_join(v->a_worker, NULL);
		v->a_worker_running = false;
	}
	// FFmpeg contexts kept alive until finalizer for property reads.
	return JS_UNDEFINED;
}

static JSValue nx_video_play(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_EXCEPTION;
	pthread_mutex_lock(&v->lock);
	v->paused = false;
	pthread_cond_broadcast(&v->has_space);
	pthread_mutex_unlock(&v->lock);
	// Slice 2b: unpause the audrv voice too if we have one.
	if (v->used_audio && v->a_voice_id >= 0 && v->audio_voice_paused) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		audrvVoiceSetPaused(drv, v->a_voice_id, false);
		audrvUpdate(drv);
		v->audio_voice_paused = false;
		nx_audio_unlock();
	}
	return JS_UNDEFINED;
}

static JSValue nx_video_pause(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_EXCEPTION;
	pthread_mutex_lock(&v->lock);
	v->paused = true;
	pthread_mutex_unlock(&v->lock);
	// Slice 2b: halt audrv voice playback so audio doesn't drain ahead
	// of the (stalled) video. audrvVoiceSetPaused freezes the played-
	// sample counter, which is what JS-side AV-sync gating needs.
	if (v->used_audio && v->a_voice_id >= 0 && !v->audio_voice_paused) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		audrvVoiceSetPaused(drv, v->a_voice_id, true);
		audrvUpdate(drv);
		v->audio_voice_paused = true;
		nx_audio_unlock();
	}
	return JS_UNDEFINED;
}

static JSValue nx_video_seek(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_EXCEPTION;
	double t;
	if (JS_ToFloat64(ctx, &t, argv[1])) return JS_EXCEPTION;
	pthread_mutex_lock(&v->lock);
	v->seek_requested = true;
	v->seek_target_sec = t;
	v->clock_started = false;     // reset wall-clock pacing
	// Clear the ring while we still hold v->lock — keeps JS from popping a
	// pre-seek frame between this call and the demuxer noticing the seek.
	nx_video_ring_clear(v);
	v->ended = false;
	pthread_cond_broadcast(&v->has_space);
	pthread_mutex_unlock(&v->lock);
	// Signal both decoder threads to flush. The demuxer will observe
	// seek_requested on its next loop iteration and call av_seek_frame.
	// Each decoder thread sees its own queue's flush flag inside pop,
	// drains pre-seek packets, and flushes its codec/resampler.
	nx_video_vpkt_request_flush(v);
	// Slice 2b: also reset audrv voice + audio_clock_started so the next
	// queued wave-buf re-anchors first_audio_pts_sec.
	if (v->used_audio && v->a_voice_id >= 0) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		audrvVoiceStop(drv, v->a_voice_id);
		audrvUpdate(drv);
		nx_video_audio_reset_wave_bufs(v);
		v->audio_clock_started = false;
		v->audio_voice_paused = false;
		nx_audio_unlock();
		nx_video_apkt_request_flush(v);
	}
	return JS_UNDEFINED;
}

// Returns: null if no frame currently due (paused / not started / not-yet-time)
// or { data: ArrayBuffer, width, height, pts, ended: bool }.
// `ended: true` is delivered with data: null exactly once when EOS reached.
static JSValue nx_video_next_frame(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_NULL;

	pthread_mutex_lock(&v->lock);

	if (v->paused) {
		pthread_mutex_unlock(&v->lock);
		return JS_NULL;
	}

	// Drive audrv state forward on every pacing call, even when no
	// video frame is ready. Without this, JS returns early on count==0
	// (which happens whenever the worker is briefly blocked, e.g.
	// waiting for a wave-buf slot or doing decode), audrv stops getting
	// a heartbeat, played-sample count freezes, and AV sync deadlocks.
	// Lock order: v->lock → nx_audio_lock (matches the use_audio_clock
	// branch below and the worker path).
	if (v->used_audio && v->a_voice_id >= 0) {
		nx_audio_lock();
		audrvUpdate(nx_audio_get_driver());
		nx_audio_unlock();
	}

	if (v->count == 0) {
		pthread_mutex_unlock(&v->lock);
		return JS_NULL;
	}

	nx_video_slot_t *slot = &v->slots[v->tail];

	if (slot->eos_marker) {
		v->tail = (v->tail + 1) % VIDEO_RING_SIZE;
		v->count--;
		slot->valid = false;
		slot->eos_marker = false;
		pthread_cond_broadcast(&v->has_space);
		pthread_mutex_unlock(&v->lock);
		JSValue obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, obj, "data", JS_NULL);
		JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, v->width));
		JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, v->height));
		JS_SetPropertyStr(ctx, obj, "pts", JS_NewFloat64(ctx, 0.0));
		JS_SetPropertyStr(ctx, obj, "ended", JS_TRUE);
		return obj;
	}

	// Pacing: when audio is the master clock (slice 2b), compare frame
	// PTS against the audrv played-sample count converted to seconds.
	// When there's no audio (file has no audio stream OR audio decode
	// failed), fall back to slice-2a wall-clock pacing.
	double pts = slot->pts_sec;
	int w = slot->width;
	int h = slot->height;
	uint8_t *src_rgba = slot->rgba;
	size_t src_bytes = (size_t)w * h * 4;

	bool use_audio_clock = v->used_audio && v->a_voice_id >= 0
	                       && v->audio_clock_started;
	double frame_offset; // seconds: positive = frame is ahead of clock
	if (use_audio_clock) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		// audrvUpdate already fired above the count==0 early-return on
		// this call, so read the played counter directly.
		u64 played_raw = audrvVoiceGetPlayedSampleCount(drv, v->a_voice_id);
		// Dynamic re-baseline. Two audrv behaviors are observed in the
		// wild for audrvVoiceStop+Start (e.g. on <video loop> wrap or
		// JS seek):
		//   (a) Counter is preserved across Stop+Start (cumulative since
		//       voice init). Baseline captured before Start = last
		//       cumulative value; subsequent reads grow above it.
		//   (b) Counter is reset to 0 at Start. Old baseline is now
		//       larger than current — that's our signal to re-baseline.
		// Adjusting here covers both cases without needing to know which
		// audrv we're on. Held under nx_audio_lock so the audio worker
		// (which also writes baseline at first-wave-buf time) can't race.
		if (played_raw < v->audio_played_baseline) {
			v->audio_played_baseline = played_raw;
		}
		u64 played = played_raw - v->audio_played_baseline;
		nx_audio_unlock();
		double audio_pos_sec = (double)played / (double)AUDIO_OUT_SAMPLE_RATE;
		double media_pos = v->first_audio_pts_sec + audio_pos_sec;
		frame_offset = pts - media_pos;
	} else {
		if (!v->clock_started) {
			v->clock_started = true;
			v->first_pts_sec = pts;
			v->wall_start_ns = nx_now_ns();
			frame_offset = 0.0; // deliver immediately
		} else {
			double elapsed_sec =
				(double)(nx_now_ns() - v->wall_start_ns) / 1e9;
			frame_offset = (pts - v->first_pts_sec) - elapsed_sec;
		}
	}
	// Drain-mode bypass (slice-2b followup #7): once the video thread
	// has signalled v->ended (i.e. it pushed the ring eos_marker after
	// codec EOF), any frames left in the ring with PTS ahead of the
	// audio clock are END-OF-STREAM leftovers. Without this bypass
	// they'd hold indefinitely (audio_pos_sec stops advancing once the
	// audrv voice drains), the JS-side currentPts gets stuck at the
	// last successfully-popped frame's PTS, and the EOS marker behind
	// them is never reached. Skip the not-yet-due gate when v->ended
	// is set so the remaining frames + EOS drain in a few ticks.
	if (frame_offset > 0.005 && !v->ended) {
		// Not due yet — hold the slot and tell JS to come back later.
		pthread_mutex_unlock(&v->lock);
		return JS_NULL;
	}
	// Drop late frames (>50 ms behind the master clock) so we catch
	// back up when decode falls behind. Pop the slot without delivering
	// it, signal the worker so it can push the next one, and recurse.
	// 50 ms ≈ 1 frame at 24 fps / 1.5 frames at 30 fps — generous
	// enough to absorb hiccups without dropping under normal load.
	if (use_audio_clock && frame_offset < -0.050) {
		v->tail = (v->tail + 1) % VIDEO_RING_SIZE;
		v->count--;
		slot->valid = false;
		pthread_cond_broadcast(&v->has_space);
		pthread_mutex_unlock(&v->lock);
		// Recurse to look at the next frame. Bounded by ring depth.
		return nx_video_next_frame(ctx, this_val, argc, argv);
	}

	// Pop this slot, copy pixels out under the lock (worker is also
	// reading/writing slot->rgba in parallel; copying here is the
	// synchronization point).
	uint8_t *copy = js_malloc(ctx, src_bytes);
	if (!copy) {
		pthread_mutex_unlock(&v->lock);
		return JS_EXCEPTION;
	}
	memcpy(copy, src_rgba, src_bytes);

	v->tail = (v->tail + 1) % VIDEO_RING_SIZE;
	v->count--;
	slot->valid = false;
	pthread_cond_broadcast(&v->has_space);
	pthread_mutex_unlock(&v->lock);

	JSValue ab = JS_NewArrayBuffer(ctx, copy, src_bytes,
	                               nx_video_free_array_buffer, NULL,
	                               false);
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "data", ab);
	JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, h));
	JS_SetPropertyStr(ctx, obj, "pts", JS_NewFloat64(ctx, pts));
	JS_SetPropertyStr(ctx, obj, "ended", JS_FALSE);
	return obj;
}

// Property getters on VideoDecoder.prototype ---------------------------------

static JSValue nx_video_get_width(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewInt32(ctx, v->width);
}
static JSValue nx_video_get_height(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewInt32(ctx, v->height);
}
static JSValue nx_video_get_duration(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewFloat64(ctx, v->duration_sec);
}
static JSValue nx_video_get_paused(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	pthread_mutex_lock(&v->lock);
	bool p = v->paused;
	pthread_mutex_unlock(&v->lock);
	return JS_NewBool(ctx, p);
}
static JSValue nx_video_get_ended(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	pthread_mutex_lock(&v->lock);
	bool e = v->ended && v->count == 0;
	pthread_mutex_unlock(&v->lock);
	return JS_NewBool(ctx, e);
}
static JSValue nx_video_get_used_hw(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewBool(ctx, v->used_hw);
}
static JSValue nx_video_get_used_audio(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewBool(ctx, v->used_audio);
}
static JSValue nx_video_get_used_video(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewBool(ctx, v->used_video);
}
static JSValue nx_video_get_muted(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_EXCEPTION;
	return JS_NewBool(ctx, v->muted);
}
static JSValue nx_video_set_muted(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
	if (argc < 2) return JS_ThrowTypeError(ctx, "videoDecoderSetMuted(handle, muted)");
	nx_video_t *v = nx_get_video(ctx, argv[0]);
	if (!v) return JS_EXCEPTION;
	int muted = JS_ToBool(ctx, argv[1]);
	if (muted < 0) return JS_EXCEPTION;
	v->muted = muted != 0;
	if (v->used_audio && v->a_voice_id >= 0) {
		nx_audio_lock();
		AudioDriver *drv = nx_audio_get_driver();
		audrvVoiceSetVolume(drv, v->a_voice_id, v->muted ? 0.0f : 1.0f);
		audrvUpdate(drv);
		nx_audio_unlock();
	}
	return JS_UNDEFINED;
}
static JSValue nx_video_get_audio_error(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_NULL;
	pthread_mutex_lock(&v->lock);
	JSValue out = v->audio_error_msg
	                  ? JS_NewString(ctx, v->audio_error_msg)
	                  : JS_NULL;
	pthread_mutex_unlock(&v->lock);
	return out;
}
static JSValue nx_video_get_error(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
	nx_video_t *v = nx_get_video(ctx, this_val);
	if (!v) return JS_NULL;
	pthread_mutex_lock(&v->lock);
	JSValue out = v->error_msg ? JS_NewString(ctx, v->error_msg) : JS_NULL;
	pthread_mutex_unlock(&v->lock);
	return out;
}

static JSValue nx_video_init_class(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "width", nx_video_get_width);
	NX_DEF_GET(proto, "height", nx_video_get_height);
	NX_DEF_GET(proto, "duration", nx_video_get_duration);
	NX_DEF_GET(proto, "paused", nx_video_get_paused);
	NX_DEF_GET(proto, "ended", nx_video_get_ended);
	NX_DEF_GET(proto, "usedHw", nx_video_get_used_hw);
	NX_DEF_GET(proto, "usedAudio", nx_video_get_used_audio);
	NX_DEF_GET(proto, "usedVideo", nx_video_get_used_video);
	NX_DEF_GET(proto, "muted", nx_video_get_muted);
	NX_DEF_GET(proto, "audioError", nx_video_get_audio_error);
	NX_DEF_GET(proto, "error", nx_video_get_error);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("videoDecoderInit", 1, nx_video_init_class),
	JS_CFUNC_DEF("videoDecoderNew", 2, nx_video_new),
	JS_CFUNC_DEF("videoDecoderClose", 1, nx_video_close),
	JS_CFUNC_DEF("videoDecoderPlay", 1, nx_video_play),
	JS_CFUNC_DEF("videoDecoderPause", 1, nx_video_pause),
	JS_CFUNC_DEF("videoDecoderSeek", 2, nx_video_seek),
	JS_CFUNC_DEF("videoDecoderNextFrame", 1, nx_video_next_frame),
	JS_CFUNC_DEF("videoDecoderSetMuted", 2, nx_video_set_muted),
};

void nx_init_video(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);
	JS_NewClassID(rt, &nx_video_class_id);
	JSClassDef video_class = {
		"VideoDecoder",
		.finalizer = nx_video_finalizer,
	};
	JS_NewClass(rt, nx_video_class_id, &video_class);
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
	                           countof(function_list));
}
