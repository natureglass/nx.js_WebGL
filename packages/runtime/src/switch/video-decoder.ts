import { $ } from '../$';
import { proto } from '../utils';

export interface VideoDecoderOptions {
	/** Try the NVTEGRA hardware decoder first (default: true). Falls
	 * back to software decode when the platform doesn't expose it. */
	hwAccel?: boolean;
	/** Loop indefinitely: on EOF the demuxer re-seeks to 0 instead of
	 * pushing the end-of-stream marker, audrv voice + queues reset,
	 * playback continues. Default: false. Mirrors HTML `<video loop>`. */
	loop?: boolean;
	/** Start the decoder muted (audrv voice volume = 0). Distinct from
	 * `noAudio` in that the audio voice IS initialized — just at zero
	 * volume — so a subsequent `setMuted(false)` can immediately resume
	 * audio without re-opening. Default: false. */
	muted?: boolean;
	/** Skip audio open entirely: no audrv voice, no swresample, no
	 * audio thread. Cannot be flipped at runtime; close + re-open the
	 * decoder if audio is needed later. Used by the poster-preview
	 * pass to extract a video's first frame without producing any
	 * audrv state transitions. Default: false. */
	noAudio?: boolean;
}

export interface VideoFrameData {
	/** RGBA8 pixel buffer, width*height*4 bytes. `null` when delivering
	 * the end-of-stream marker. */
	data: ArrayBuffer | null;
	width: number;
	height: number;
	/** Presentation timestamp in seconds. */
	pts: number;
	/** True for the single end-of-stream sentinel. */
	ended: boolean;
}

/**
 * Hardware-accelerated video decoder backed by FFmpeg + Tegra NVDEC.
 * Opens a media file (anything libavformat can demux — mp4, mkv, webm),
 * decodes the video stream on a worker thread, and exposes frames as
 * RGBA pixel buffers consumable from `OffscreenCanvas.putImageData`.
 *
 * Slice 2a: video-only. Audio path lands in slice 2b.
 *
 * @example
 * ```ts
 * const dec = new Switch.VideoDecoder('sdmc:/movie.mp4');
 * dec.play();
 *
 * function frameTick() {
 *   const f = dec.nextFrame();
 *   if (f && f.data) {
 *     const img = new ImageData(new Uint8ClampedArray(f.data), f.width, f.height);
 *     canvas2d.putImageData(img, 0, 0);
 *   }
 *   requestAnimationFrame(frameTick);
 * }
 * requestAnimationFrame(frameTick);
 * ```
 */
export class VideoDecoder {
	/** Intrinsic frame width as reported by the codec, in pixels. */
	declare readonly width: number;
	/** Intrinsic frame height as reported by the codec, in pixels. */
	declare readonly height: number;
	/** Container duration in seconds (0 if unknown). */
	declare readonly duration: number;
	/** True when decode is paused (no frames pushed into the ring). */
	declare readonly paused: boolean;
	/** True once the last frame has been delivered. */
	declare readonly ended: boolean;
	/** True if NVTEGRA hw-accel is in use. */
	declare readonly usedHw: boolean;
	/** Slice 2b followup #4: true when the source has a usable video
	 * stream. False for audio-only files (mp3, m4a, wav, flac, ogg,
	 * opus) opened in a `<video>` element — the decoder runs the audio
	 * pipeline only, `nextFrame()` returns `null` until the audio
	 * finishes, then delivers one `{ data: null, ended: true }` sentinel.
	 * Width and height are 0 in this mode. */
	declare readonly usedVideo: boolean;
	/** Slice 2b: true once the audio stream has been located, opened,
	 * and wired up to the audrv driver. False when the file has no
	 * audio stream OR audio setup failed (see {@link audioError}). When
	 * false the video plays without audio and `nextFrame()` paces video
	 * against wall-clock instead of audio-master clock. */
	declare readonly usedAudio: boolean;
	/** Slice 2b: first audio-side error encountered during open or
	 * decode, or `null`. Examples: "no decoder for audio codec",
	 * "swr_init failed", "audrvMemPoolAdd failed". Non-fatal — video
	 * still plays without audio when this is non-null. */
	declare readonly audioError: string | null;
	/** First decoder error encountered, or `null`. */
	declare readonly error: string | null;
	/** Slice 2b followup #5: true when audio is silenced. Driven by
	 * {@link setMuted}; setting this calls `audrvVoiceSetVolume(0|1)`
	 * on the audio voice. Persists across pause/resume + seeks. */
	declare readonly muted: boolean;
	/** Slice 2c: linear playback gain 0.0..1.0. Driven by {@link setVolume};
	 * `muted` overrides to 0 but the stored gain is preserved. */
	declare readonly volume: number;
	/** Slice 2c: accurate audio playback position in seconds, from the
	 * audrv played-sample counter (the A/V-sync master clock). Tracks
	 * playback for audio-only sources where there are no video-frame PTS to
	 * advance the cursor. 0 before audio starts / for files with no audio. */
	declare readonly audioTime: number;

	constructor(url: string, opts?: VideoDecoderOptions) {
		return proto($.videoDecoderNew(url, opts), VideoDecoder);
	}

	/** Stops the worker thread and releases all FFmpeg + libnx state. */
	close(): void {
		$.videoDecoderClose(this);
	}

	/** Starts (or resumes) decode + frame delivery. */
	play(): void {
		$.videoDecoderPlay(this);
	}

	// Cut #22 (V8 port, 2026-07-01): pause / seek / setMuted / setVolume /
	// getAudioLevels / getFrequencyData / getWaveform are NOT wired in the
	// V8 minimal port — no v2 demo currently exercises them. Add them when a
	// future demo needs them; the C-side plumbing (nx_media_pause / _seek
	// exists in media-decoder.h; audio-graph attachment for muted/volume/
	// visualizer surface is bigger). Attempting to call these methods will
	// throw at the `$` layer.

	/**
	 * Returns the next decoded frame if one is available + due for
	 * presentation. Returns `null` when the decoder is paused or the next
	 * frame isn't yet due.
	 */
	nextFrame(): VideoFrameData | null {
		return $.videoDecoderNextFrame(this);
	}
}
$.videoDecoderInit(VideoDecoder);
