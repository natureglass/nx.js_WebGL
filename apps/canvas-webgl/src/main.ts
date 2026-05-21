// nx.js standalone WebGL demo — Ocean Cube.
//
// All four shader programs (background sky/stars/planes, billboard moon,
// animated ocean grid, chrome cube with logo) opt into the bridge's
// raw-shader passthrough path via `#pragma raw_passthrough` so the
// custom GLSL runs end-to-end on native GLES. Required because the
// shader uniform/attribute names (uTime, uCamRight, uCubeYaw, etc.)
// aren't in nx.js's hardcoded allowlist.
//
// Right stick orbits the camera; left stick (push up/down) zooms.
// Touch + drag on the screen also orbits.

// nx.js's `screen` extends Canvas — it has getContext + addEventListener.
// The bundled @nx.js/runtime types aren't always resolvable in the IDE
// (they sit in the package's source not its build), so cast to `any`
// at the API boundary. esbuild ignores TS types when bundling.
const _screen: any = screen;
const W = screen.width;
const H = screen.height;

// IMPORTANT: nx.js's `nx_framebuffer_init` (which exits the boot console
// and creates the on-screen framebuffer) is invoked by getContext('2d')
// but NOT by getContext('webgl'). If we only ever ask for a WebGL
// context, the launching-screen-console never clears AND the WebGL
// bridge's output never reaches the visible screen. Trigger the
// framebuffer init by acquiring the 2D context once up front (we don't
// actually use it after this), then proceed with WebGL.
_screen.getContext('2d');

const gl: WebGLRenderingContext | null = _screen.getContext('webgl', {
	antialias: false,
	alpha: false,
	depth: true,
	stencil: false,
	premultipliedAlpha: false,
	preserveDrawingBuffer: false,
});
if (!gl) {
	throw new Error('WebGL is not available');
}

// Opt into the bridge's GPU-accelerated path. Without this the bridge
// falls back to software cairo rasterization which can't run our
// custom passthrough shaders.
if (typeof (gl as any).enableGpuBridgePrototype === 'function') {
	(gl as any).enableGpuBridgePrototype(true);
}

function compile(type: GLenum, src: string): WebGLShader {
	const s = gl!.createShader(type)!;
	gl!.shaderSource(s, src);
	gl!.compileShader(s);
	if (!gl!.getShaderParameter(s, gl!.COMPILE_STATUS)) {
		throw new Error('shader compile: ' + gl!.getShaderInfoLog(s));
	}
	return s;
}
function link(vs: string, fs: string): WebGLProgram {
	const p = gl!.createProgram()!;
	gl!.attachShader(p, compile(gl!.VERTEX_SHADER, vs));
	gl!.attachShader(p, compile(gl!.FRAGMENT_SHADER, fs));
	gl!.linkProgram(p);
	if (!gl!.getProgramParameter(p, gl!.LINK_STATUS)) {
		throw new Error('program link: ' + gl!.getProgramInfoLog(p));
	}
	return p;
}

// --- Shader sources ----------------------------------------------------
// All start with `#pragma raw_passthrough` so the bridge promotes the
// program to native-GLES dispatch — required because the custom uniform
// and attribute names aren't recognized by the bridge's allowlist.

const bgVS = `#pragma raw_passthrough
	precision mediump float;
	attribute vec2 aPos;
	varying vec2 vUv;
	void main() {
		vUv = aPos * 0.5 + 0.5;
		gl_Position = vec4(aPos, 0.0, 1.0);
	}
`;

const bgFS = `#pragma raw_passthrough
	precision mediump float;
	uniform float uTime;
	uniform vec3 uCamRight;
	uniform vec3 uCamUp;
	uniform vec3 uCamForward;
	uniform float uAspect;
	uniform float uTanHalfFov;
	varying vec2 vUv;

	float hash(vec2 p) {
		return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
	}

	void main() {
		vec2 p = vUv * 2.0 - 1.0;
		p.x *= uAspect;
		vec3 rd = normalize(uCamForward + p.x * uTanHalfFov * uCamRight + p.y * uTanHalfFov * uCamUp);

		float y = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
		vec3 zenith = vec3(0.002, 0.005, 0.018);
		vec3 upper = vec3(0.010, 0.022, 0.060);
		vec3 horizon = vec3(0.032, 0.048, 0.105);
		vec3 seaLine = vec3(0.012, 0.018, 0.032);

		vec3 c = mix(horizon, upper, smoothstep(0.14, 0.62, y));
		c = mix(c, zenith, smoothstep(0.52, 1.0, y));
		c = mix(seaLine, c, smoothstep(0.02, 0.18, y));

		float horizonHaze = smoothstep(0.00, 0.12, y) * (1.0 - smoothstep(0.34, 0.62, y));
		c = mix(c, vec3(0.125, 0.145, 0.205), horizonHaze * 0.68);

		float starMask = smoothstep(0.12, 0.36, y);
		float lon = atan(rd.z, rd.x);
		float lat = asin(clamp(rd.y, -1.0, 1.0));
		vec2 skyUV = vec2(lon / 6.2831853 + 0.5, lat / 3.14159265 + 0.5);
		vec2 gv = skyUV * vec2(560.0, 300.0);
		vec2 cell = floor(gv);
		vec2 f = fract(gv) - 0.5;
		float h = hash(cell);
		float star = step(0.985, h);
		vec2 starPos = vec2(hash(cell + 1.3), hash(cell + 4.7)) - 0.5;
		float dist = length(f - starPos * 0.70);

		float phaseA = hash(cell + 13.1) * 6.2831853;
		float phaseB = hash(cell + 29.7) * 6.2831853;
		float speedA = 1.8 + hash(cell + 8.2) * 4.8;
		float speedB = 3.0 + hash(cell + 17.4) * 6.0;
		float waveA = 0.5 + 0.5 * sin(uTime * speedA + phaseA);
		float waveB = 0.5 + 0.5 * sin(uTime * speedB + phaseB);
		float blinkGate = smoothstep(0.58, 0.92, waveA);
		float flash = smoothstep(0.72, 1.0, hash(cell + floor(vec2(uTime * (1.10 + hash(cell + 5.6) * 2.0)))));
		float twinkle = clamp(0.06 + waveA * 0.34 + waveB * 0.18 + blinkGate * 0.38 + flash * 0.52, 0.0, 1.15);

		float spark = smoothstep(0.08, 0.0, dist) * star * twinkle * 1.18;
		float halo = smoothstep(0.18, 0.0, dist) * star * 0.42 * (0.22 + twinkle * 0.78);

		c += (vec3(0.92, 0.96, 1.0) * spark + vec3(0.56, 0.64, 0.92) * halo) * starMask * 1.20;

		vec3 planeWhite = vec3(1.00, 0.96, 0.78);
		vec3 planeRed = vec3(1.00, 0.035, 0.018);

		for (int i = 0; i < 20; i++) {
			float fi = float(i);
			float baseLon = -3.14159265 + (fi + 0.5) * (6.2831853 / 20.0);
			float planeLat = 0.018 + sin(fi * 2.17) * 0.006 + sin(uTime * 0.035 + fi) * 0.0012;
			float planeSpeed = 0.012 + fract(sin(fi * 19.13) * 43758.5453) * 0.010;
			float dirRand = fract(sin(fi * 53.71 + 1.23) * 43758.5453);
			float planeDir = mix(-1.0, 1.0, step(0.5, dirRand));
			float planeLon = mod(baseLon + uTime * planeSpeed * planeDir + 3.14159265, 6.2831853) - 3.14159265;

			float dLon = atan(sin(lon - planeLon), cos(lon - planeLon));
			float dLat = lat - planeLat;
			float planeD = length(vec2(dLon * cos(planeLat), dLat));

			float planeCore = smoothstep(0.00115, 0.0, planeD);

			float planePhase = mod(uTime + fi * 0.173, 2.0);
			float redBlinkA = smoothstep(0.00, 0.020, planePhase) * (1.0 - smoothstep(0.055, 0.085, planePhase));
			float redBlinkB = smoothstep(0.105, 0.125, planePhase) * (1.0 - smoothstep(0.160, 0.190, planePhase));
			float redBlink = clamp(redBlinkA + redBlinkB, 0.0, 1.0);
			float planeTwinkle = 0.92 + 0.08 * sin(uTime * (5.2 + fi * 0.07) + fi);
			vec3 planeColor = mix(planeWhite, planeRed, redBlink);

			c += planeColor * planeCore * 2.45 * planeTwinkle;
		}

		c = pow(c, vec3(0.4545));
		gl_FragColor = vec4(c, 1.0);
	}
`;

const moonVS = `#pragma raw_passthrough
	precision highp float;
	attribute vec2 aPos;
	uniform mat4 uProj, uView;
	uniform vec3 uMoonCenter;
	uniform vec3 uCamRight;
	uniform vec3 uCamUp;
	uniform float uMoonSize;
	varying vec2 vLocal;

	void main() {
		vLocal = aPos;
		vec3 world = uMoonCenter + (uCamRight * aPos.x + uCamUp * aPos.y) * uMoonSize;
		gl_Position = uProj * uView * vec4(world, 1.0);
	}
`;

const moonFS = `#pragma raw_passthrough
	precision mediump float;
	varying vec2 vLocal;

	float hash(vec2 p) {
		return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
	}

	float noise(vec2 p) {
		vec2 i = floor(p);
		vec2 f = fract(p);
		vec2 u = f * f * (3.0 - 2.0 * f);
		return mix(
			mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
			mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
			u.y
		);
	}

	float fbm(vec2 p) {
		float v = 0.0;
		float a = 0.5;
		mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
		for (int i = 0; i < 6; i++) {
			v += a * noise(p);
			p = m * p + 2.7;
			a *= 0.52;
		}
		return v;
	}

	void main() {
		float d = length(vLocal);
		if (d > 1.08) discard;

		float disc = smoothstep(0.72, 0.60, d);
		float core = smoothstep(0.18, 0.0, d);

		vec2 p = vLocal * 3.0;
		float n1 = fbm(p + vec2(0.4, -0.3));
		float n2 = fbm(p * 1.9 + vec2(-3.1, 1.7));
		float n3 = fbm(p * 3.2 + vec2(4.2, -2.6));
		float mottled = n1 * 0.45 + n2 * 0.35 + n3 * 0.20;

		float mariaA = smoothstep(0.44, 0.0, length(vLocal - vec2(-0.18, 0.06)));
		float mariaB = smoothstep(0.31, 0.0, length(vLocal - vec2(0.22, -0.12)));
		float mariaC = smoothstep(0.26, 0.0, length(vLocal - vec2(0.04, 0.24)));
		float mariaD = smoothstep(0.22, 0.0, length(vLocal - vec2(-0.28, -0.18)));
		float maria = mariaA * 0.70 + mariaB * 0.52 + mariaC * 0.35 + mariaD * 0.30;

		float crater1 = smoothstep(0.11, 0.03, abs(length(vLocal - vec2(0.16, 0.16)) - 0.06));
		float crater2 = smoothstep(0.10, 0.03, abs(length(vLocal - vec2(-0.10, -0.04)) - 0.05));
		float crater3 = smoothstep(0.09, 0.03, abs(length(vLocal - vec2(0.02, -0.22)) - 0.04));
		float craters = (crater1 + crater2 + crater3) * 0.12;

		float rim = smoothstep(0.90, 0.52, d) - smoothstep(1.0, 0.84, d);

		vec3 darkTone = vec3(0.34, 0.36, 0.41);
		vec3 midTone = vec3(0.84, 0.87, 0.93);
		vec3 lightTone = vec3(1.00, 1.00, 1.00);
		vec3 warmCore = vec3(1.00, 0.995, 0.97);

		float shade = clamp(0.56 + mottled * 0.34 - maria * 0.28 - craters * 0.46 + rim * 0.18, 0.0, 1.0);
		vec3 moon = mix(darkTone, midTone, smoothstep(0.06, 0.52, shade));
		moon = mix(moon, lightTone, smoothstep(0.48, 0.92, shade));
		moon = mix(moon, warmCore, core * 0.12);

		float haloRing = smoothstep(0.84, 0.70, d) - smoothstep(1.04, 0.86, d);
		float haloSoft = smoothstep(1.08, 0.70, d);
		float subtleGlow = smoothstep(1.08, 0.42, d) * (1.0 - smoothstep(0.88, 0.66, d));

		vec3 color = moon * disc;
		color += vec3(0.38, 0.60, 1.00) * haloRing * 0.55;
		color += vec3(0.22, 0.42, 0.90) * haloSoft * 0.12;
		color += vec3(0.58, 0.74, 1.00) * subtleGlow * 0.10;
		color = mix(color, vec3(0.78, 0.82, 0.92), 0.08);

		float alpha = max(disc * 0.96, haloRing * 0.28 + haloSoft * 0.055 + subtleGlow * 0.035);
		gl_FragColor = vec4(color, alpha);
	}
`;

const waterVS = `#pragma raw_passthrough
	precision highp float;
	attribute vec3 aPos;
	attribute vec2 aUv;
	uniform mat4 uProj, uView;
	uniform float uTime;
	varying vec3 vPos;
	varying vec2 vUv;
	varying float vHeight;

	float wave(vec2 p, vec2 d, float amp, float freq, float speed, float phase) {
		d = normalize(d);
		return sin(dot(p, d) * freq + uTime * speed + phase) * amp;
	}

	float waveField(vec2 p) {
		float h = 0.0;
		h += wave(p, vec2( 0.90,  0.24), 0.25, 0.54, 0.62, 0.0);
		h += wave(p, vec2( 0.27,  0.96), 0.18, 0.92, 0.84, 1.7);
		h += wave(p, vec2(-0.82,  0.43), 0.11, 1.62, 1.14, 2.4);
		h += wave(p, vec2( 0.66, -0.75), 0.055, 3.52, 1.75, 0.8);
		h += wave(p + vec2(sin(uTime * 0.07), cos(uTime * 0.08)) * 4.0, vec2(-0.15, 1.0), 0.024, 8.0, 2.35, 3.2);
		return h;
	}

	void main() {
		vec3 p = aPos;
		float h = waveField(p.xz);
		p.y += h;
		vPos = p;
		vUv = aUv;
		vHeight = h;
		gl_Position = uProj * uView * vec4(p, 1.0);
	}
`;

const waterFS = `#pragma raw_passthrough
	precision highp float;
	uniform float uTime;
	uniform vec3 uCam;
	uniform vec3 uCubeCenter;
	uniform float uCubeYaw;
	varying vec3 vPos;
	varying vec2 vUv;
	varying float vHeight;

	vec3 skyColor(vec3 rd) {
		float y = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
		vec3 horizon = vec3(0.032, 0.048, 0.105);
		vec3 upper = vec3(0.010, 0.022, 0.060);
		vec3 zenith = vec3(0.002, 0.005, 0.018);
		vec3 c = mix(horizon, upper, smoothstep(0.06, 0.48, y));
		c = mix(c, zenith, smoothstep(0.44, 1.0, y));
		return c;
	}

	float wave(vec2 p, vec2 d, float amp, float freq, float speed, float phase) {
		d = normalize(d);
		return sin(dot(p, d) * freq + uTime * speed + phase) * amp;
	}

	float waveField(vec2 p) {
		float h = 0.0;
		h += wave(p, vec2( 0.90,  0.24), 0.25, 0.54, 0.62, 0.0);
		h += wave(p, vec2( 0.27,  0.96), 0.18, 0.92, 0.84, 1.7);
		h += wave(p, vec2(-0.82,  0.43), 0.11, 1.62, 1.14, 2.4);
		h += wave(p, vec2( 0.66, -0.75), 0.055, 3.52, 1.75, 0.8);
		h += wave(p + vec2(sin(uTime * 0.07), cos(uTime * 0.08)) * 4.0, vec2(-0.15, 1.0), 0.024, 8.0, 2.35, 3.2);
		return h;
	}

	float hash(vec2 p) {
		return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
	}

	float noise(vec2 p) {
		vec2 i = floor(p);
		vec2 f = fract(p);
		vec2 u = f * f * (3.0 - 2.0 * f);
		return mix(
			mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
			mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
			u.y
		);
	}

	void main() {
		float e = 0.10;
		float hL = waveField(vPos.xz - vec2(e, 0.0));
		float hR = waveField(vPos.xz + vec2(e, 0.0));
		float hD = waveField(vPos.xz - vec2(0.0, e));
		float hU = waveField(vPos.xz + vec2(0.0, e));
		vec3 n = normalize(vec3(hL - hR, 0.24, hD - hU));

		float rip1 = sin(vPos.x * 18.0 + vPos.z * 9.0 + uTime * 2.8) * 0.010;
		float rip2 = sin(vPos.x * -11.0 + vPos.z * 22.0 + uTime * 3.9) * 0.008;
		n = normalize(n + vec3(rip1, 0.0, rip2));

		vec3 viewDir = normalize(uCam - vPos);
		vec3 moonDir = normalize(vec3(-26.0, 6.0, 42.0));
		vec3 refl = reflect(-viewDir, n);

		float ndv = max(dot(n, viewDir), 0.0);
		float fresnel = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);
		float diffuse = max(dot(n, moonDir), 0.0);
		float facingSun = max(dot(refl, moonDir), 0.0);
		float glint = pow(facingSun, 420.0) * 8.0 + pow(facingSun, 70.0) * 0.6;

		vec3 deepWater = vec3(0.001, 0.014, 0.070);
		vec3 midWater = vec3(0.010, 0.070, 0.220);
		vec3 crestTint = vec3(0.050, 0.170, 0.450);
		float hMix = smoothstep(-0.45, 0.55, vHeight);
		float distFade = smoothstep(18.0, 86.0, length(vPos.xz));
		vec3 body = mix(deepWater, midWater, hMix * 0.82);
		body = mix(body, crestTint, diffuse * 0.16 + fresnel * 0.08);
		body *= 0.60 + diffuse * 0.34;

		vec3 reflection = skyColor(refl);
		reflection += vec3(0.88, 0.93, 1.00) * pow(facingSun, 240.0) * 1.45;
		vec3 color = mix(body, reflection, fresnel * 0.90);
		color += vec3(0.88, 0.94, 1.00) * glint * 0.78;

		float slope = 1.0 - n.y;
		float crest = smoothstep(0.34, 0.64, vHeight + slope * 1.1);
		float softNoiseA = noise(vPos.xz * 1.35 + vec2(uTime * 0.055, -uTime * 0.040));
		float softNoiseB = noise(vPos.xz * 4.20 + vec2(-uTime * 0.140, uTime * 0.105));
		float rippleBreakup = 0.5 + 0.5 * sin(vPos.x * 5.8 + vPos.z * 2.3 + uTime * 1.65);
		float whiteStreaks = smoothstep(0.66, 0.92, softNoiseA * 0.50 + softNoiseB * 0.35 + rippleBreakup * 0.15);
		float foam = crest * whiteStreaks * 0.16;
		color = mix(color, vec3(0.92, 0.97, 1.0), foam);
		color += vec3(0.34, 0.46, 0.58) * foam * 0.20;

		vec2 cubeDelta = vPos.xz - uCubeCenter.xz;
		float cyaw = cos(uCubeYaw);
		float syaw = sin(uCubeYaw);
		vec2 cubeLocal = vec2(
			cyaw * cubeDelta.x - syaw * cubeDelta.y,
			syaw * cubeDelta.x + cyaw * cubeDelta.y
		);

		float underDist = length(cubeLocal);
		float underCore = exp(-underDist * underDist * 0.055);
		float underFade = 1.0 - smoothstep(4.20, 8.40, underDist);
		float underCenter = 1.0 - smoothstep(0.0, 3.10, underDist);
		float underWave = 0.76 + 0.24 * sin(uTime * 1.55 + vHeight * 8.0 + cubeLocal.x * 0.75 - cubeLocal.y * 0.55);
		float underCaustic = 0.70 + 0.30 * sin(cubeLocal.x * 3.0 + sin(cubeLocal.y * 2.0 + uTime * 0.7) + uTime * 1.2);
		float underwaterGlow = clamp((underCore * underFade + underCenter * 0.38) * underWave * underCaustic, 0.0, 1.0);

		vec2 boxD = abs(cubeLocal) - vec2(2.10 + vHeight * 0.045);
		float cubeBoxDist = length(max(boxD, 0.0)) + min(max(boxD.x, boxD.y), 0.0);
		float outsideDist = max(cubeBoxDist, 0.0);
		float contactCore = exp(-cubeBoxDist * cubeBoxDist * 30.0);
		float contactSpread = (1.0 - smoothstep(0.03, 0.72, outsideDist)) * smoothstep(-0.04, 0.22, cubeBoxDist);
		float contactFeather = 1.0 - smoothstep(0.34, 0.92, outsideDist);
		float contactLine = (contactCore * 0.62 + contactSpread * 0.46) * contactFeather;
		float contactOutside = smoothstep(-0.08, 0.13, cubeBoxDist);
		float contactWave = smoothstep(-0.18, 0.42, vHeight + slope * 1.45);
		float contactNoise = 0.64 + 0.36 * smoothstep(0.18, 0.90, hash(floor((cubeLocal + vec2(uTime * 0.34, uTime * 0.21)) * 20.0)));
		float contactRipple = 0.76 + 0.24 * sin(uTime * 7.6 + cubeLocal.x * 4.4 + cubeLocal.y * 3.7 + vHeight * 13.0);
		float contactFoam = contactLine * contactOutside * (0.46 + 0.54 * contactWave) * contactNoise * contactRipple;

		color = mix(color, vec3(0.02, 0.55, 0.78), underwaterGlow * 0.34);
		color += vec3(0.00, 0.34, 0.56) * underwaterGlow * 0.82;
		color += vec3(0.08, 0.70, 0.95) * underwaterGlow * underwaterGlow * 0.42;

		color = mix(color, vec3(0.98, 0.995, 1.0), contactFoam * 0.72);
		color += vec3(0.72, 0.88, 0.94) * contactFoam * 0.66;

		float seaHaze = smoothstep(24.0, 92.0, length(vPos.xz));
		seaHaze *= smoothstep(0.16, 1.0, 1.0 - max(viewDir.y, 0.0));
		color = mix(color, vec3(0.088, 0.118, 0.172), seaHaze * 0.62);

		color = mix(color, vec3(0.038, 0.080, 0.145), distFade * 0.34);
		color = color / (color + vec3(1.0));
		color = pow(color, vec3(0.4545));
		gl_FragColor = vec4(color, 1.0);
	}
`;

const cubeVS = `#pragma raw_passthrough
	precision highp float;
	attribute vec3 aPos;
	attribute vec3 aNormal;
	attribute vec2 aUv;
	uniform mat4 uProj, uView;
	uniform vec3 uCenter;
	uniform vec3 uTilt;
	uniform float uYaw;
	varying vec3 vWorld;
	varying vec3 vNormal;
	varying vec2 vUv;

	mat3 rotX(float a) {
		float c = cos(a), s = sin(a);
		return mat3(1.0,0.0,0.0, 0.0,c,s, 0.0,-s,c);
	}
	mat3 rotY(float a) {
		float c = cos(a), s = sin(a);
		return mat3(c,0.0,-s, 0.0,1.0,0.0, s,0.0,c);
	}
	mat3 rotZ(float a) {
		float c = cos(a), s = sin(a);
		return mat3(c,s,0.0, -s,c,0.0, 0.0,0.0,1.0);
	}

	void main() {
		mat3 r = rotY(uYaw) * rotZ(uTilt.z) * rotX(uTilt.x);
		vec3 p = r * aPos + uCenter;
		vec3 n = normalize(r * aNormal);
		vWorld = p;
		vNormal = n;
		vUv = aUv;
		gl_Position = uProj * uView * vec4(p, 1.0);
	}
`;

// Note: nx.js's UNPACK_FLIP_Y_WEBGL is a no-op, so the cube UV scheme
// (v=0 at face top) already maps row 0 of the source PNG to face top.
// No `1.0 - vUv.y` flip needed.
const cubeFS = `#pragma raw_passthrough
	precision highp float;
	uniform vec3 uCam;
	uniform sampler2D uLogo;
	varying vec3 vWorld;
	varying vec3 vNormal;
	varying vec2 vUv;

	vec3 skyColor(vec3 rd) {
		float y = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
		vec3 horizon = vec3(0.032, 0.048, 0.105);
		vec3 upper = vec3(0.010, 0.022, 0.060);
		vec3 zenith = vec3(0.002, 0.005, 0.018);
		vec3 c = mix(horizon, upper, smoothstep(0.06, 0.46, y));
		c = mix(c, zenith, smoothstep(0.46, 1.0, y));
		return c;
	}

	void main() {
		vec3 n = normalize(vNormal);
		vec3 viewDir = normalize(uCam - vWorld);
		vec3 moonDir = normalize(vec3(-26.0, 6.0, 42.0));
		vec3 refl = reflect(-viewDir, n);

		float ndv = max(dot(n, viewDir), 0.0);
		float fresnel = 0.08 + 0.92 * pow(1.0 - ndv, 4.5);
		float diffuse = max(dot(n, moonDir), 0.0);
		vec3 halfDir = normalize(moonDir + viewDir);
		float spec = pow(max(dot(n, halfDir), 0.0), 120.0) * 1.8;
		spec += pow(max(dot(refl, moonDir), 0.0), 160.0) * 0.8;

		vec4 logoTex = texture2D(uLogo, vUv);
		vec3 metalDark = vec3(0.05, 0.05, 0.06);
		vec3 metalLight = vec3(0.72, 0.76, 0.84);
		vec3 reflection = skyColor(refl);
		vec3 metal = mix(metalDark, metalLight, diffuse * 0.60 + 0.15);
		metal = mix(metal, reflection, fresnel * 0.55);

		float logoLum = dot(logoTex.rgb, vec3(0.299, 0.587, 0.114));
		float logoMask = smoothstep(0.05, 0.32, logoLum);

		vec3 litLogo = logoTex.rgb * (0.70 + diffuse * 0.44);
		vec3 darkBase = vec3(0.0);
		vec3 color = mix(darkBase, litLogo, logoMask);
		color = mix(color, metal, 0.12 * logoMask);
		color += reflection * fresnel * (0.10 * logoMask);
		color += vec3(0.92, 0.96, 1.00) * spec * (0.18 + 0.82 * logoMask);
		color += vec3(0.02, 0.03, 0.05) * (1.0 - diffuse) * (0.05 + 0.03 * logoMask);

		color = color / (color + vec3(1.0));
		color = pow(color, vec3(0.4545));
		gl_FragColor = vec4(color, 1.0);
	}
`;

const bgProg = link(bgVS, bgFS);
const moonProg = link(moonVS, moonFS);
const waterProg = link(waterVS, waterFS);
const cubeProg = link(cubeVS, cubeFS);

function attrib(prog: WebGLProgram, name: string, size: number, stride: number, offset: number) {
	const loc = gl!.getAttribLocation(prog, name);
	if (loc < 0) return;
	gl!.enableVertexAttribArray(loc);
	gl!.vertexAttribPointer(loc, size, gl!.FLOAT, false, stride, offset);
}

function perspective(fovy: number, aspect: number, near: number, far: number): Float32Array {
	const f = 1 / Math.tan(fovy / 2),
		nf = 1 / (near - far);
	return new Float32Array([
		f / aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (far + near) * nf, -1,
		0, 0, (2 * far * near) * nf, 0,
	]);
}
function nrm3(v: number[]) {
	const l = Math.hypot(v[0], v[1], v[2]) || 1;
	return [v[0] / l, v[1] / l, v[2] / l];
}
function cross(a: number[], b: number[]) {
	return [
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0],
	];
}
function lookAt(eye: number[], center: number[], up: number[]): Float32Array {
	const z = nrm3([eye[0] - center[0], eye[1] - center[1], eye[2] - center[2]]);
	const x = nrm3(cross(up, z));
	const y = cross(z, x);
	return new Float32Array([
		x[0], y[0], z[0], 0,
		x[1], y[1], z[1], 0,
		x[2], y[2], z[2], 0,
		-(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]),
		-(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]),
		-(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]),
		1,
	]);
}

function makeGrid(n: number, size: number) {
	const verts: number[] = [];
	const inds: number[] = [];
	for (let z = 0; z <= n; z++) {
		for (let x = 0; x <= n; x++) {
			const px = (x / n - 0.5) * size;
			const pz = (z / n - 0.5) * size;
			verts.push(px, 0, pz, x / n, z / n);
		}
	}
	for (let z = 0; z < n; z++) {
		for (let x = 0; x < n; x++) {
			const i = z * (n + 1) + x;
			inds.push(i, i + 1, i + n + 1, i + 1, i + n + 2, i + n + 1);
		}
	}
	return { verts: new Float32Array(verts), inds: new Uint16Array(inds) };
}

function makeCube(size: number) {
	const s = size * 0.5;
	return new Float32Array([
		// +Z
		-s,-s, s, 0,0,1, 0,1,   s,-s, s, 0,0,1, 1,1,   s, s, s, 0,0,1, 1,0,
		-s,-s, s, 0,0,1, 0,1,   s, s, s, 0,0,1, 1,0,  -s, s, s, 0,0,1, 0,0,
		// -Z
		 s,-s,-s, 0,0,-1, 0,1, -s,-s,-s, 0,0,-1, 1,1, -s, s,-s, 0,0,-1, 1,0,
		 s,-s,-s, 0,0,-1, 0,1, -s, s,-s, 0,0,-1, 1,0,  s, s,-s, 0,0,-1, 0,0,
		// +X
		 s,-s, s, 1,0,0, 0,1,   s,-s,-s, 1,0,0, 1,1,   s, s,-s, 1,0,0, 1,0,
		 s,-s, s, 1,0,0, 0,1,   s, s,-s, 1,0,0, 1,0,   s, s, s, 1,0,0, 0,0,
		// -X
		-s,-s,-s,-1,0,0, 0,1,  -s,-s, s,-1,0,0, 1,1,  -s, s, s,-1,0,0, 1,0,
		-s,-s,-s,-1,0,0, 0,1,  -s, s, s,-1,0,0, 1,0,  -s, s,-s,-1,0,0, 0,0,
		// +Y
		-s, s, s, 0,1,0, 0,1,   s, s, s, 0,1,0, 1,1,   s, s,-s, 0,1,0, 1,0,
		-s, s, s, 0,1,0, 0,1,   s, s,-s, 0,1,0, 1,0,  -s, s,-s, 0,1,0, 0,0,
		// -Y
		-s,-s,-s, 0,-1,0, 0,1,  s,-s,-s, 0,-1,0, 1,1,  s,-s, s, 0,-1,0, 1,0,
		-s,-s,-s, 0,-1,0, 0,1,  s,-s, s, 0,-1,0, 1,0, -s,-s, s, 0,-1,0, 0,0,
	]);
}

const grid = makeGrid(220, 170);
const waterVbo = gl.createBuffer()!;
gl.bindBuffer(gl.ARRAY_BUFFER, waterVbo);
gl.bufferData(gl.ARRAY_BUFFER, grid.verts, gl.STATIC_DRAW);
const waterIbo = gl.createBuffer()!;
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, waterIbo);
gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, grid.inds, gl.STATIC_DRAW);

const bgVbo = gl.createBuffer()!;
gl.bindBuffer(gl.ARRAY_BUFFER, bgVbo);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);

const moonVbo = gl.createBuffer()!;
gl.bindBuffer(gl.ARRAY_BUFFER, moonVbo);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);

const cubeVerts = makeCube(4.2);
const cubeVbo = gl.createBuffer()!;
gl.bindBuffer(gl.ARRAY_BUFFER, cubeVbo);
gl.bufferData(gl.ARRAY_BUFFER, cubeVerts, gl.STATIC_DRAW);

// Texture setup. Pre-allocate the persistent native handle via
// texImage2D(NULL) at 1024×1024 BEFORE uploading real data — otherwise
// `gl.bindTexture(cubeTex)` doesn't forward to native GL and the
// passthrough samples whatever was last bound (the bridge FBO's color
// attachment, causing each face to mirror the rendered scene).
const TEX_SIZE = 1024;
const cubeTex = gl.createTexture()!;
gl.bindTexture(gl.TEXTURE_2D, cubeTex);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, TEX_SIZE, TEX_SIZE, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

const logoImg = new Image();
logoImg.onload = () => {
	try {
		const off = new OffscreenCanvas(TEX_SIZE, TEX_SIZE);
		const tctx = off.getContext('2d')!;
		tctx.fillStyle = '#111111';
		tctx.fillRect(0, 0, TEX_SIZE, TEX_SIZE);
		const pad = 96;
		tctx.drawImage(logoImg, pad, pad, TEX_SIZE - pad * 2, TEX_SIZE - pad * 2);
		const imgData = tctx.getImageData(0, 0, TEX_SIZE, TEX_SIZE);
		gl!.bindTexture(gl!.TEXTURE_2D, cubeTex);
		gl!.texImage2D(
			gl!.TEXTURE_2D,
			0,
			gl!.RGBA,
			TEX_SIZE,
			TEX_SIZE,
			0,
			gl!.RGBA,
			gl!.UNSIGNED_BYTE,
			new Uint8Array(imgData.data.buffer),
		);
	} catch (e) {
		console.debug('[nxjs-webgl] texture upload failed:', e);
	}
};
logoImg.onerror = () => {
	console.debug('[nxjs-webgl] failed to load romfs:/logo.png');
};
logoImg.src = 'romfs:/logo.png';

// CPU mirror of the wave field so the cube floats on the water.
function waveAt(x: number, z: number, t: number): number {
	const norm = (a: number, b: number): [number, number] => {
		const l = Math.hypot(a, b);
		return [a / l, b / l];
	};
	const d = (ax: number, az: number, dir: [number, number]) => ax * dir[0] + az * dir[1];
	let h = 0;
	h += Math.sin(d(x, z, norm(0.90, 0.24)) * 0.54 + t * 0.62 + 0.0) * 0.25;
	h += Math.sin(d(x, z, norm(0.27, 0.96)) * 0.92 + t * 0.84 + 1.7) * 0.18;
	h += Math.sin(d(x, z, norm(-0.82, 0.43)) * 1.62 + t * 1.14 + 2.4) * 0.11;
	h += Math.sin(d(x, z, norm(0.66, -0.75)) * 3.52 + t * 1.75 + 0.8) * 0.055;
	h += Math.sin(d(x + Math.sin(t * 0.07) * 4.0, z + Math.cos(t * 0.08) * 4.0, norm(-0.15, 1.0)) * 8.0 + t * 2.35 + 3.2) * 0.024;
	return h;
}
function slopeAt(x: number, z: number, t: number) {
	const e = 0.16;
	const hL = waveAt(x - e, z, t);
	const hR = waveAt(x + e, z, t);
	const hD = waveAt(x, z - e, t);
	const hU = waveAt(x, z + e, t);
	return { dx: (hR - hL) / (2 * e), dz: (hU - hD) / (2 * e) };
}

// Camera state.
const cam = {
	yaw: 2.77,
	pitch: -0.12,
	dist: 17.5,
};

// Touch input — nx.js native way. The screen IS the canvas; touch
// events dispatch on it directly (see `apps/touchscreen` example).
let activeTouchId: number | null = null;
let dragActive = false;
let lastX = 0;
let lastY = 0;

function startDrag(x: number, y: number) {
	dragActive = true;
	lastX = x;
	lastY = y;
}
function moveDrag(x: number, y: number) {
	if (!dragActive) return;
	const dx = x - lastX;
	const dy = y - lastY;
	lastX = x;
	lastY = y;
	cam.yaw += dx * 0.006;
	cam.pitch = Math.max(-0.75, Math.min(-0.05, cam.pitch + dy * 0.005));
}
function endDrag() {
	dragActive = false;
	activeTouchId = null;
}

_screen.addEventListener('touchstart', (e: TouchEvent) => {
	if (activeTouchId !== null) return;
	const t = e.changedTouches[0];
	if (!t) return;
	activeTouchId = t.identifier;
	startDrag(t.clientX, t.clientY);
});
_screen.addEventListener('touchmove', (e: TouchEvent) => {
	if (activeTouchId === null) return;
	for (let i = 0; i < e.changedTouches.length; i++) {
		const t = e.changedTouches[i];
		if (t.identifier === activeTouchId) {
			moveDrag(t.clientX, t.clientY);
			return;
		}
	}
});
_screen.addEventListener('touchend', (e: TouchEvent) => {
	if (activeTouchId === null) return;
	for (let i = 0; i < e.changedTouches.length; i++) {
		if (e.changedTouches[i].identifier === activeTouchId) {
			endDrag();
			return;
		}
	}
});

// Gamepad polling. Right stick orbits, left-stick Y zooms.
function pollGamepad(dt: number) {
	const pads = navigator.getGamepads();
	const gp = pads && pads[0];
	if (!gp) return;
	const dead = 0.12;
	// Right stick = orbit. Standard Gamepad API axis 2 = right-X,
	// axis 3 = right-Y.
	const rx = gp.axes[2] || 0;
	const ry = gp.axes[3] || 0;
	if (Math.abs(rx) > dead) cam.yaw += rx * dt * 1.6;
	if (Math.abs(ry) > dead) {
		cam.pitch = Math.max(-0.75, Math.min(-0.05, cam.pitch + ry * dt * 0.9));
	}
	// Left stick Y = zoom. Push UP (negative Y) → zoom in.
	const ly = gp.axes[1] || 0;
	if (Math.abs(ly) > dead) {
		cam.dist = Math.max(9, Math.min(28, cam.dist + ly * dt * 12));
	}
}

gl.enable(gl.DEPTH_TEST);
gl.disable(gl.CULL_FACE);

let lastMs = 0;

function draw(ms: number) {
	requestAnimationFrame(draw);
	const t = ms * 0.001;
	const dt = lastMs > 0 ? Math.min(0.1, (ms - lastMs) * 0.001) : 0.016;
	lastMs = ms;

	pollGamepad(dt);

	gl!.viewport(0, 0, W, H);

	const proj = perspective((45 * Math.PI) / 180, W / H, 0.1, 500.0);
	const cy = Math.cos(cam.pitch);
	const sy = Math.sin(cam.pitch);
	const eye = [
		Math.sin(cam.yaw) * cam.dist * cy,
		4.4 - sy * cam.dist * 0.32,
		Math.cos(cam.yaw) * cam.dist * cy,
	];
	const view = lookAt(eye, [0, 0.2, 0], [0, 1, 0]);

	gl!.clearColor(0.02, 0.04, 0.075, 1);
	gl!.clear(gl!.COLOR_BUFFER_BIT | gl!.DEPTH_BUFFER_BIT);

	// --- Background sky/stars (no depth test).
	gl!.disable(gl!.DEPTH_TEST);
	gl!.useProgram(bgProg);
	gl!.uniform1f(gl!.getUniformLocation(bgProg, 'uTime'), t);
	const target = [0, 0.2, 0];
	const bgForward = nrm3([target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]]);
	const bgRight = nrm3(cross(bgForward, [0, 1, 0]));
	const bgUp = nrm3(cross(bgRight, bgForward));
	const tanHalfFov = Math.tan(((45 * Math.PI) / 180) * 0.5);
	gl!.uniform3fv(gl!.getUniformLocation(bgProg, 'uCamRight'), new Float32Array(bgRight));
	gl!.uniform3fv(gl!.getUniformLocation(bgProg, 'uCamUp'), new Float32Array(bgUp));
	gl!.uniform3fv(gl!.getUniformLocation(bgProg, 'uCamForward'), new Float32Array(bgForward));
	gl!.uniform1f(gl!.getUniformLocation(bgProg, 'uAspect'), W / H);
	gl!.uniform1f(gl!.getUniformLocation(bgProg, 'uTanHalfFov'), tanHalfFov);
	gl!.bindBuffer(gl!.ARRAY_BUFFER, bgVbo);
	attrib(bgProg, 'aPos', 2, 8, 0);
	gl!.drawArrays(gl!.TRIANGLE_STRIP, 0, 4);

	// --- Moon billboard (blended over sky).
	gl!.enable(gl!.BLEND);
	gl!.blendFunc(gl!.SRC_ALPHA, gl!.ONE_MINUS_SRC_ALPHA);
	gl!.useProgram(moonProg);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(moonProg, 'uProj'), false, proj);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(moonProg, 'uView'), false, view);
	gl!.uniform3fv(gl!.getUniformLocation(moonProg, 'uMoonCenter'), new Float32Array([-78.0, 34.0, 126.0]));
	gl!.uniform3fv(gl!.getUniformLocation(moonProg, 'uCamRight'), new Float32Array(bgRight));
	gl!.uniform3fv(gl!.getUniformLocation(moonProg, 'uCamUp'), new Float32Array(bgUp));
	gl!.uniform1f(gl!.getUniformLocation(moonProg, 'uMoonSize'), 24.0);
	gl!.bindBuffer(gl!.ARRAY_BUFFER, moonVbo);
	attrib(moonProg, 'aPos', 2, 8, 0);
	gl!.drawArrays(gl!.TRIANGLE_STRIP, 0, 4);
	gl!.disable(gl!.BLEND);

	// --- Water + cube.
	gl!.enable(gl!.DEPTH_TEST);
	const cubeX = 0.0;
	const cubeZ = -0.2;
	const surf = waveAt(cubeX, cubeZ, t);
	const slope = slopeAt(cubeX, cubeZ, t);
	const cubeCenter = [cubeX, surf * 0.55 + 0.70, cubeZ];
	const cubeTilt = [slope.dz * 0.42, 0.0, -slope.dx * 0.42];
	const cubeYaw = t * 0.10;

	gl!.useProgram(waterProg);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(waterProg, 'uProj'), false, proj);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(waterProg, 'uView'), false, view);
	gl!.uniform1f(gl!.getUniformLocation(waterProg, 'uTime'), t);
	gl!.uniform3fv(gl!.getUniformLocation(waterProg, 'uCam'), new Float32Array(eye));
	gl!.uniform3fv(gl!.getUniformLocation(waterProg, 'uCubeCenter'), new Float32Array(cubeCenter));
	gl!.uniform1f(gl!.getUniformLocation(waterProg, 'uCubeYaw'), cubeYaw);
	gl!.bindBuffer(gl!.ARRAY_BUFFER, waterVbo);
	gl!.bindBuffer(gl!.ELEMENT_ARRAY_BUFFER, waterIbo);
	attrib(waterProg, 'aPos', 3, 20, 0);
	attrib(waterProg, 'aUv', 2, 20, 12);
	gl!.drawElements(gl!.TRIANGLES, grid.inds.length, gl!.UNSIGNED_SHORT, 0);

	gl!.useProgram(cubeProg);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(cubeProg, 'uProj'), false, proj);
	gl!.uniformMatrix4fv(gl!.getUniformLocation(cubeProg, 'uView'), false, view);
	gl!.uniform3fv(gl!.getUniformLocation(cubeProg, 'uCam'), new Float32Array(eye));
	gl!.uniform3fv(gl!.getUniformLocation(cubeProg, 'uCenter'), new Float32Array(cubeCenter));
	gl!.uniform3fv(gl!.getUniformLocation(cubeProg, 'uTilt'), new Float32Array(cubeTilt));
	gl!.uniform1f(gl!.getUniformLocation(cubeProg, 'uYaw'), cubeYaw);
	gl!.activeTexture(gl!.TEXTURE0);
	gl!.bindTexture(gl!.TEXTURE_2D, cubeTex);
	gl!.uniform1i(gl!.getUniformLocation(cubeProg, 'uLogo'), 0);
	gl!.bindBuffer(gl!.ARRAY_BUFFER, cubeVbo);
	attrib(cubeProg, 'aPos', 3, 32, 0);
	attrib(cubeProg, 'aNormal', 3, 32, 12);
	attrib(cubeProg, 'aUv', 2, 32, 24);
	gl!.drawArrays(gl!.TRIANGLES, 0, cubeVerts.length / 8);
}

requestAnimationFrame(draw);
