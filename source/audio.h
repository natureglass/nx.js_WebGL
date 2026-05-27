#pragma once
#include "types.h"
#include <switch.h>

void nx_init_audio(JSContext *ctx, JSValueConst init_obj);

// Slice-2b additions for sharing the audrv stack with source/video.c
// (the <video> element's audio path). Same driver, same voice pool —
// the audio renderer service only allows one driver per process, so
// VideoDecoder + Switch.Audio MUST share.
//
// All four functions are thread-safe via an internal mutex (see audio.c).
// The decode worker thread + the JS main thread both touch the driver
// (worker enqueues wave buffers, main thread reads played-sample count
// for AV sync) — wrap any audrv* call site with nx_audio_lock /
// nx_audio_unlock or call through these helpers.
bool nx_audio_ensure_initialized(void);
AudioDriver *nx_audio_get_driver(void);
int nx_audio_acquire_voice(void);
void nx_audio_release_voice(int voice_id);
void nx_audio_lock(void);
void nx_audio_unlock(void);

// Slice-2b multi-video audio (2026-05-27). Sub-allocates fixed-size
// slots inside the existing main audio mempool, so each <video>'s
// wave-buf ring lives within a single audrv-registered mempool instead
// of each video calling its own `audrvMemPoolAdd`. The old per-video
// mempool approach hit `num_mempools=2` after one video (need to leave
// the second mempool slot free for `nx_audio_play`'s per-call mempool).
// Bumping `num_mempools` to 32 worked for registration but Citron's
// audrv mixer froze with multiple concurrent voices reading from
// distinct mempools — sub-allocation sidesteps both issues.
//
// `bytes` must be ≤ `nx_audio_video_slot_size()`; oversize requests
// return NULL. Slot count is fixed at build time
// (`NX_VIDEO_AUDIO_NUM_SLOTS` in audio.c) — when all slots are taken,
// returns NULL and the caller falls back to video-only (no audio).
// Slot memory is zeroed + dcache-flushed before return.
void *nx_audio_acquire_video_buf(size_t bytes);
void nx_audio_release_video_buf(void *ptr);
size_t nx_audio_video_slot_size(void);
