// src/main.ts
var _screen = screen;
var W = screen.width;
var H = screen.height;
var ctx = _screen.getContext("2d");
ctx.fillStyle = "#000";
ctx.fillRect(0, 0, W, H);
var decoder = new Switch.VideoDecoder("romfs:/sample.mp4", { loop: true, hwAccel: false });
decoder.play();
var vw = decoder.width || W;
var vh = decoder.height || H;
var off = new OffscreenCanvas(vw, vh);
var offCtx = off.getContext("2d");
var screenAspect = W / H;
var videoAspect = vw / vh;
var dstW;
var dstH;
var dstX;
var dstY;
if (videoAspect > screenAspect) {
  dstW = W;
  dstH = Math.round(W / videoAspect);
  dstX = 0;
  dstY = Math.round((H - dstH) / 2);
} else {
  dstH = H;
  dstW = Math.round(H * videoAspect);
  dstX = Math.round((W - dstW) / 2);
  dstY = 0;
}
function frame() {
  requestAnimationFrame(frame);
  const f = decoder.nextFrame();
  if (!f || !f.data) return;
  const img = new ImageData(new Uint8ClampedArray(f.data), f.width, f.height);
  offCtx.putImageData(img, 0, 0);
  ctx.drawImage(off, dstX, dstY, dstW, dstH);
}
requestAnimationFrame(frame);
//# sourceMappingURL=main.js.map
