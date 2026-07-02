import { $ } from '../$';
import { getSharedAudioContext } from '../audio';
import { ctxInternal, nodeInternal } from '../audio/internal';
import type { GainNode } from '../audio/gain-node';
import { proto } from '../utils';

// Per-decoder GainNode owned by the JS wrapper. Kept off the wrapped
// instance so we don't have to reason about property visibility on the
// `proto()`-produced object; the WeakMap also drops its reference
// naturally when the decoder is GC'd.
const gainByDecoder = new WeakMap<VideoDecoder, GainNode>();

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
		const inst = proto(
			$.videoDecoderNew(url, opts),
			VideoDecoder,
		) as VideoDecoder;
		// Cut #22b (2026-07-02): wire the audio stream into the shared
		// audio graph when the source has an audio track. Restores audio
		// playback for spectraplay's <audio> flow (routed through the
		// same videoDecoder path as <video>) and audio-bearing <video>.
		// Skipped for `noAudio: true` (poster-preview path) and for
		// sources without a usable audio stream.
		const wantAudio = !(opts && opts.noAudio) && inst.usedAudio;
		if (wantAudio) {
			try {
				const ctx = getSharedAudioContext();
				const stream = $.videoDecoderCreateAudioNode(
					inst,
					ctxInternal(ctx).handle,
				);
				if (stream) {
					const gain = ctx.createGain();
					gain.connect(ctx.destination);
					$.audioNodeConnect(
						stream,
						nodeInternal(gain).handle,
					);
					gainByDecoder.set(inst, gain);
				}
			} catch {
				// Audio-graph setup failure is non-fatal — video (if any)
				// still plays silently. The caller sees `audioError` /
				// `usedAudio=false` via the existing getters.
			}
		}
		return inst;
	}

	/** Stops the worker thread and releases all FFmpeg + libnx state. */
	close(): void {
		$.videoDecoderClose(this);
	}

	/** Starts (or resumes) decode + frame delivery. */
	play(): void {
		$.videoDecoderPlay(this);
	}

	/** Cut #22b (2026-07-02): freeze decode / audio output. `play()`
	 * resumes from the same position. */
	pause(): void {
		$.videoDecoderPause(this);
	}

	/** Cut #22b (2026-07-02): seek to `seconds` (clamped to the media
	 * duration). Audio + video state re-anchor on the next frame. */
	seek(seconds: number): void {
		$.videoDecoderSeek(this, Number.isFinite(seconds) ? seconds : 0);
	}

	/**
	 * Cut #22b (2026-07-02): sets the playback gain in the [0,1] range.
	 * Applied through the JS-side GainNode inserted between the stream
	 * source and the shared context's destination. `muted` overrides to 0
	 * for the effective output but the stored gain is preserved so
	 * `setMuted(false)` restores it.
	 */
	setVolume(value: number): void {
		const v = Number.isFinite(value) ? Math.max(0, Math.min(1, value)) : 1;
		$.videoDecoderSetVolume(this, v);
		const gain = gainByDecoder.get(this);
		if (gain) gain.gain.value = this.muted ? 0 : v;
	}

	/**
	 * Cut #22b (2026-07-02): zero-gain override on the JS-side GainNode
	 * (the stored `volume` is preserved).
	 */
	setMuted(muted: boolean): void {
		const m = !!muted;
		$.videoDecoderSetMuted(this, m);
		const gain = gainByDecoder.get(this);
		if (gain) gain.gain.value = m ? 0 : this.volume;
	}

	/**
	 * Cut #22b Stage 2 (2026-07-02): fill `out` with the last
	 * `out.length` mono samples in [-1, 1] from the audio tap. Returns
	 * `true` when the tap has enough samples (fresh decoder / no audio
	 * track → `false`).
	 */
	getWaveform(out: Float32Array): boolean {
		return $.videoDecoderGetWaveform(this, out);
	}

	/**
	 * Cut #22b Stage 2 (2026-07-02): fill `out` with FFT magnitude,
	 * bin-averaged across `out.length` bins from ~0..Nyquist, normalized
	 * to approx [0, 1]. Returns `true` when the tap has enough samples.
	 */
	getFrequencyData(out: Float32Array): boolean {
		return $.videoDecoderGetFrequencyData(this, out);
	}

	/**
	 * Cut #22b Stage 2 (2026-07-02): returns `[bass, mid, high]` RMS
	 * levels in ~[0, 1]. Empty array when audio hasn't accumulated the
	 * tap window yet.
	 */
	getAudioLevels(): number[] {
		return $.videoDecoderGetAudioLevels(this);
	}

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
