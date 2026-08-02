// ---------------------------------------------------------------------------
// Minimal sleep / resume crash probe.
//
// Purpose: reproduce the "console sleeps (screen off), wake it, app shows for
// ~1s then crashes" bug with the smallest possible surface, so the ENGINE
// instrumentation (source/main.cc) can capture WHERE it dies. This app does
// nothing but continuously animate a 2D canvas via requestAnimationFrame,
// which keeps the engine presenting frames (eglSwapBuffers) every tick — the
// same present path the real apps use.
//
// What to watch, on-screen:
//   - frame counter: should climb smoothly (~60/s). If it FREEZES, the loop
//     stalled. If it jumps by a big number after wake, the loop resumed.
//   - dt / maxGap (ms): the biggest gap between two frames. A sleep shows up
//     as one huge dt right after wake.
//
// What to collect, on the SD card, after it crashes:
//   - sdmc:/switch/nxjs-crash.log   (written by the engine's new CPU-exception
//     handler the instant an in-process fault hits — has the faulting PC/LR)
//   - sdmc:/switch/nxjs-debug.log   (the engine heartbeat + per-frame trace;
//     the LAST line before the crash names the step that died)
//
// Repro steps: launch -> let it run a few seconds -> press POWER to sleep ->
// wait ~2s -> press POWER to wake -> observe the ~1s-then-crash.
// ---------------------------------------------------------------------------

const ctx = screen.getContext('2d');

let frame = 0;
let last = Date.now();
let maxGap = 0;
let lastGap = 0;

function draw() {
	frame++;
	const now = Date.now();
	const dt = now - last;
	last = now;
	lastGap = dt;
	if (dt > maxGap) maxGap = dt;

	const W = screen.width;
	const H = screen.height;

	// Full-clear each frame (exercises the canvas -> GPU present path).
	ctx.fillStyle = '#101820';
	ctx.fillRect(0, 0, W, H);

	// A bar that sweeps across the screen, so motion (or a freeze) is obvious
	// even without reading the numbers.
	const barW = 90;
	const x = (frame * 7) % (W + barW) - barW;
	ctx.fillStyle = '#65bc7b';
	ctx.fillRect(x, H - 60, barW, 60);

	ctx.fillStyle = '#ffffff';
	ctx.textAlign = 'center';

	ctx.font = '64px system-ui';
	ctx.textBaseline = 'middle';
	ctx.fillText(`frame ${frame}`, W / 2, H / 2 - 80);

	ctx.font = '30px system-ui';
	ctx.fillText(`dt ${lastGap}ms    maxGap ${maxGap}ms`, W / 2, H / 2);
	ctx.fillText(new Date().toLocaleTimeString(), W / 2, H / 2 + 50);

	ctx.font = '24px system-ui';
	ctx.fillStyle = '#8aa0b4';
	ctx.fillText(
		'Sleep (POWER) then wake to test resume. Logs: sdmc:/switch/nxjs-{debug,crash}.log',
		W / 2,
		H - 90,
	);

	// Secondary heartbeat to the debug log (the engine's native heartbeat is
	// the reliable one; this just confirms JS is still ticking).
	if (frame % 60 === 0) {
		console.log(`[sleep-test] frame=${frame} maxGap=${maxGap}ms`);
	}

	requestAnimationFrame(draw);
}

console.log('[sleep-test] starting rAF loop');
requestAnimationFrame(draw);
