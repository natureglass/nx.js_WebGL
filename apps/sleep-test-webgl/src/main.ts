// ---------------------------------------------------------------------------
// Minimal WebGL sleep/resume crash probe.
//
// The 2D-canvas probe SURVIVED sleep/resume, so the crash is specific to the
// WebGL path (source/webgl.cc). WebGL here renders into a bridge "tenant FBO"
// on Skia's shared GL context, so two things are required, in order:
//   1. bring up the Skia-GPU screen FIRST — getContext('2d') does this via
//      $.framebufferInit(); without it getContext('webgl'*) refuses with
//      "[webgl] context_new refused: skia_gpu not ready".
//   2. THEN acquire the WebGL context.
//
// This uses WebGL 1 (getContext('webgl')) because this engine build is
// "Phase 2.G.0": WebGL2 exists shape-only (methods throw), while WebGL1 is
// fully implemented AND shares the identical bridge / tenant-FBO / present
// path that Brewser's WebGL2 uses — so it exercises the same resume-sensitive
// GL state with code that is guaranteed to actually run.
//
// It does real GL work every frame (clear + a shader-drawn spinning triangle),
// which is what a resume-invalidated GL context would fault on. The engine
// instrumentation (already in nxjs.nro) traces the frame-handler + present and
// the CPU-exception handler catches an in-process fault -> nxjs-crash.log.
//
// Requires APPLICATION MODE (full memory). Repro: launch (hold R on a game to
// enter app mode, same as Brewser) -> spinning triangle -> POWER to sleep ->
// wait ~2s -> POWER to wake -> watch for the ~1s-then-crash. Collect
// sdmc:/switch/nxjs-crash.log + sdmc:/switch/nxjs-debug.log.
// ---------------------------------------------------------------------------

// 1) Bring up the Skia-GPU screen first (required before any WebGL context).
const ctx2d = screen.getContext('2d');
ctx2d.fillStyle = '#0a0f14';
ctx2d.fillRect(0, 0, screen.width, screen.height);

// 2) Now the bridge can attach. Use WebGL 1 (fully implemented in this build).
const gl = screen.getContext('webgl');

if (!gl) {
	console.log('[sleep-test-webgl] getContext("webgl") returned null.');
	console.log('');
	console.log('WebGL needs APPLICATION MODE (same as Brewser). Launch this the');
	console.log('way you launch Brewser: hold R while opening a game so hbmenu');
	console.log('takes over in application mode, then relaunch this NRO.');
	console.log('');
	console.log('Press + to exit.');
} else {
	console.printErr(`[sleep-test-webgl] WebGL ok: ${gl.getParameter(gl.VERSION)}\n`);

	const vsrc = `
attribute vec2 a_pos;
uniform float u_angle;
void main() {
  float c = cos(u_angle), s = sin(u_angle);
  vec2 p = vec2(a_pos.x * c - a_pos.y * s, a_pos.x * s + a_pos.y * c);
  gl_Position = vec4(p, 0.0, 1.0);
}`;

	const fsrc = `
precision mediump float;
uniform float u_t;
void main() {
  gl_FragColor = vec4(0.4 + 0.4 * sin(u_t), 0.55, 0.4 + 0.4 * cos(u_t), 1.0);
}`;

	function compile(type: number, src: string): WebGLShader {
		const sh = gl!.createShader(type)!;
		gl!.shaderSource(sh, src);
		gl!.compileShader(sh);
		if (!gl!.getShaderParameter(sh, gl!.COMPILE_STATUS)) {
			console.printErr('[sleep-test-webgl] shader error: ' + gl!.getShaderInfoLog(sh) + '\n');
		}
		return sh;
	}

	const prog = gl.createProgram()!;
	gl.attachShader(prog, compile(gl.VERTEX_SHADER, vsrc));
	gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fsrc));
	gl.linkProgram(prog);
	if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
		console.printErr('[sleep-test-webgl] link error: ' + gl.getProgramInfoLog(prog) + '\n');
	}
	gl.useProgram(prog);

	const buf = gl.createBuffer();
	gl.bindBuffer(gl.ARRAY_BUFFER, buf);
	// prettier-ignore
	gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
		0.0,  0.6,
		-0.5, -0.4,
		0.5, -0.4,
	]), gl.STATIC_DRAW);
	const loc = gl.getAttribLocation(prog, 'a_pos');
	gl.enableVertexAttribArray(loc);
	gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);

	const uAngle = gl.getUniformLocation(prog, 'u_angle');
	const uT = gl.getUniformLocation(prog, 'u_t');

	let frame = 0;
	function draw() {
		frame++;
		const t = frame / 60;
		gl!.clearColor(0.06, 0.09, 0.13, 1.0);
		gl!.clear(gl!.COLOR_BUFFER_BIT);
		gl!.uniform1f(uAngle, t);
		gl!.uniform1f(uT, t);
		gl!.drawArrays(gl!.TRIANGLES, 0, 3);

		if (frame % 60 === 0) console.printErr('[sleep-test-webgl] frame=' + frame + '\n');
		requestAnimationFrame(draw);
	}
	requestAnimationFrame(draw);
}
