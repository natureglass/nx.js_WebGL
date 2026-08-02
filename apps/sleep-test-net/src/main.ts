// ---------------------------------------------------------------------------
// Minimal NETWORK sleep/resume crash probe.
//
// Both the 2D-canvas and WebGL probes SURVIVED sleep/resume. The user reports
// the real Brewser crash fires "as soon as I click Check for Updates" after a
// wake (idle shell, no media) — i.e. on a network fetch. When the Switch
// sleeps, Wi-Fi drops; on wake it re-associates, and a fetch (DNS -> TLS ->
// socket) issued while the network stack is half-recovered / holding stale
// sockets is a textbook post-resume hard fault.
//
// This probe does exactly that: it fetches Brewser's REAL "Check for Updates"
// URL on a 3s timer, so a fetch always fires shortly after you wake. The
// engine's CPU-exception handler (already in nxjs.nro) catches an in-process
// fault -> sdmc:/switch/nxjs-crash.log; console.printErr() timestamps each
// fetch into sdmc:/switch/nxjs-debug.log so the LAST "[net] fetch #N ..." line
// before the crash correlates with the resume marker.
//
// Needs Wi-Fi configured (application mode not required, but launch it the same
// way as the others is fine). Repro: launch -> a few fetches succeed -> POWER
// to sleep -> wait ~2s -> POWER to wake -> the next auto-fetch (<=3s) should
// crash. Collect sdmc:/switch/nxjs-crash.log + sdmc:/switch/nxjs-debug.log.
// ---------------------------------------------------------------------------

// Brewser's actual Check-for-Updates versions endpoint (reliable GitHub host).
const URL = 'https://raw.githubusercontent.com/natureglass/Brewser/main/versions.json';

const ctx = screen.getContext('2d');

let frame = 0;
let last = Date.now();
let maxGap = 0;
let fetchN = 0;
let inFlight = false;
let lastStatus = '(none yet)';
let nextFetchAt = Date.now() + 1500;

async function doFetch() {
	if (inFlight) return;
	inFlight = true;
	const n = ++fetchN;
	const t0 = Date.now();
	lastStatus = `#${n} fetching...`;
	console.printErr(`[net] fetch #${n} START\n`);
	try {
		const res = await fetch(URL);
		const text = await res.text();
		const ms = Date.now() - t0;
		lastStatus = `#${n} OK ${res.status} ${text.length}b ${ms}ms`;
		console.printErr(`[net] fetch #${n} OK status=${res.status} len=${text.length} ${ms}ms\n`);
	} catch (e) {
		const ms = Date.now() - t0;
		const msg = (e as Error)?.message ?? String(e);
		lastStatus = `#${n} ERROR ${msg} ${ms}ms`;
		console.printErr(`[net] fetch #${n} ERROR ${msg} ${ms}ms\n`);
	} finally {
		inFlight = false;
	}
}

function draw() {
	frame++;
	const now = Date.now();
	const dt = now - last;
	last = now;
	if (dt > maxGap) maxGap = dt;

	if (now >= nextFetchAt && !inFlight) {
		nextFetchAt = now + 3000;
		void doFetch();
	}

	const W = screen.width;
	const H = screen.height;
	ctx.fillStyle = '#101820';
	ctx.fillRect(0, 0, W, H);

	ctx.fillStyle = '#ffffff';
	ctx.textAlign = 'center';
	ctx.font = '38px system-ui';
	ctx.textBaseline = 'middle';
	ctx.fillText('network sleep/resume probe', W / 2, 90);

	ctx.font = '30px system-ui';
	ctx.fillText(`fetches: ${fetchN}`, W / 2, H / 2 - 60);
	ctx.fillStyle = lastStatus.includes('ERROR') ? '#e0a030' : '#65bc7b';
	ctx.fillText(lastStatus, W / 2, H / 2);
	ctx.fillStyle = '#8aa0b4';
	ctx.font = '24px system-ui';
	ctx.fillText(`frame ${frame}   dt ${dt}ms   maxGap ${maxGap}ms`, W / 2, H / 2 + 60);
	ctx.fillText('Sleep (POWER), wake, wait ~3s for the next fetch.', W / 2, H - 90);

	requestAnimationFrame(draw);
}

console.printErr('[net] sleep-test-net starting; auto-fetch every 3s\n');
requestAnimationFrame(draw);
