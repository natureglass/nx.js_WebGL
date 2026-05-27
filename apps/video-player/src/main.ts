// nx.js standalone video player demo. Opens `romfs:/sample.mp4`,
// plays it fullscreen at 1280×720, and loops on EOF. Uses the
// `Switch.VideoDecoder` API (FFmpeg + Tegra NVDEC under the hood) to
// decode RGBA frames, then `putImageData` to an offscreen canvas at
// the video's native size and `drawImage` it scaled (letterboxed if
// the source aspect doesn't match 16:9) onto the screen.
//
// `loop: true` on the decoder options does the seek-to-0-on-EOF dance
// inside the decoder thread — no JS-side replay logic needed.

const _screen: any = screen;
const W = screen.width;
const H = screen.height;

const ctx = _screen.getContext('2d') as CanvasRenderingContext2D;
ctx.fillStyle = '#000';
ctx.fillRect(0, 0, W, H);

// `hwAccel: false` forces FFmpeg's CPU H.264 decoder. NVTEGRA
// hardware decode (the default) is broken on Citron — first few
// frames decode then `avcodec_send_packet` permanently fails for
// the decoder instance, but the audio thread keeps running (it
// doesn't touch NVDEC), so the symptom is "audio plays, screen
// stays black". See [[nvtegra-unreliable-on-citron]]. On real
// Switch silicon NVTEGRA should work; for this minimal demo we
// default to SW so it just works in both targets. The Switch's
// 4× Cortex-A57 @ ~1 GHz handles 320×240 and small-720p H.264
// comfortably; large 1080p sources will struggle without HW.
const decoder = new Switch.VideoDecoder('romfs:/sample.mp4', { loop: true, hwAccel: false });
decoder.play();

// `width` / `height` are populated synchronously by the constructor
// (probe + first-packet inspection happens inside `videoDecoderNew`).
// If the source is audio-only those are 0 — just fall back to screen
// dims so we at least don't divide by zero.
const vw = decoder.width || W;
const vh = decoder.height || H;

// Offscreen canvas at native video size. Each decoded frame is
// `putImageData`'d here; `drawImage` then scales it onto the screen.
// Avoids the cost of feeding the screen-sized scaler the wrong number
// of bytes — putImageData requires the image to match the canvas's
// pixel dimensions exactly.
const off = new OffscreenCanvas(vw, vh);
const offCtx = off.getContext('2d') as OffscreenCanvasRenderingContext2D;

// Fit-to-screen letterbox math. Computed once — neither the source
// nor the screen resizes during playback.
const screenAspect = W / H;
const videoAspect = vw / vh;
let dstW: number, dstH: number, dstX: number, dstY: number;
if (videoAspect > screenAspect) {
	// Source is wider than the screen — fit width, black bars top/bottom.
	dstW = W;
	dstH = Math.round(W / videoAspect);
	dstX = 0;
	dstY = Math.round((H - dstH) / 2);
} else {
	// Source is narrower than the screen — fit height, black bars on sides.
	dstH = H;
	dstW = Math.round(H * videoAspect);
	dstX = Math.round((W - dstW) / 2);
	dstY = 0;
}

function frame(): void {
	requestAnimationFrame(frame);
	const f = decoder.nextFrame();
	if (!f || !f.data) return;
	const img = new ImageData(new Uint8ClampedArray(f.data), f.width, f.height);
	offCtx.putImageData(img, 0, 0);
	ctx.drawImage(off as unknown as CanvasImageSource, dstX, dstY, dstW, dstH);
}
requestAnimationFrame(frame);
