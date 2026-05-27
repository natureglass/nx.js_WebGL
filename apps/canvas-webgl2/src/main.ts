// nx.js standalone WebGL 2 demo — Sunset Sea Procedural ShaderToy Cube.
//
// GLSL ES 3.00. A single full-screen triangle (gl_VertexID + a const
// array, no vertex buffer) drives a procedural sunset sky (sun, halo,
// FBM clouds, stars) over an FBM-displaced sea with sun glitter, a
// chrome cube wearing the nx.js logo that floats on the waves, and a
// soft cube-on-sea shadow + reflection streak. CPU computes the cube's
// pose once per frame (yaw + wave roll + wave pitch).
//
// Both shaders open with `#pragma raw_passthrough` so the nx.js bridge
// promotes the linked program to native-GLES dispatch (the bridge's
// hardcoded color/texture programs don't know our custom uniforms or
// the gl_VertexID-driven attribute-less triangle).
//
// Right stick orbits the camera; touch-and-drag also orbits.

// nx.js's `screen` extends Canvas — it has getContext + addEventListener.
// The bundled @nx.js/runtime types aren't always resolvable in the IDE,
// so cast to `any` at the API boundary; esbuild ignores TS types when
// bundling.
const _screen: any = screen;
const W = screen.width;
const H = screen.height;

// IMPORTANT: nx.js's `nx_framebuffer_init` (which exits the boot console
// and creates the on-screen framebuffer) is invoked by getContext('2d')
// but NOT by getContext('webgl2'). If we only ever ask for a WebGL
// context, the launching-screen-console never clears AND the WebGL
// bridge's output never reaches the visible screen. Trigger the
// framebuffer init by acquiring the 2D context once up front (we don't
// actually use it after this), then proceed with WebGL 2.
_screen.getContext('2d');

const gl: WebGL2RenderingContext | null = _screen.getContext('webgl2', {
	antialias: false,
	alpha: false,
	depth: false,
	stencil: false,
	preserveDrawingBuffer: false,
});
if (!gl) {
	throw new Error('WebGL 2 is not available');
}

// Opt into the bridge's GPU-accelerated path. Without this the bridge
// falls back to software cairo rasterization which can't run our
// custom passthrough shaders.
if (typeof (gl as any).enableGpuBridgePrototype === 'function') {
	(gl as any).enableGpuBridgePrototype(true);
}

// ----- Shader sources -----------------------------------------------------
// `#pragma raw_passthrough` MUST sit immediately after `#version 300 es`
// so the bridge promotes the linked program to native-GLES dispatch
// instead of swapping in its own color/texture program (see the
// [[bridge-raw-shader-passthrough]] notes).

const vertexSource = `#version 300 es
#pragma raw_passthrough
precision highp float;
out vec2 vUv;
const vec2 POS[3] = vec2[3](
  vec2(-1.0, -1.0),
  vec2( 3.0, -1.0),
  vec2(-1.0,  3.0)
);
void main() {
  vec2 p = POS[gl_VertexID];
  vUv = p * 0.5 + 0.5;
  gl_Position = vec4(p, 0.0, 1.0);
}
`;

const fragmentSource = `#version 300 es
#pragma raw_passthrough
precision highp float;
in vec2 vUv;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform vec2 uLook;
uniform float uFov;
uniform float uMood;
uniform sampler2D uLogo;
uniform int uLogoReady;
// Cube pose precomputed once on the CPU per frame.
uniform vec3 uCubeCenter;
uniform mat3 uCubeR;
uniform mat3 uCubeInvR;

#define PI 3.141592653589793
#define FAR 900.0
const float CUBE_HALF_SIZE = 2.76;
const vec2 WAVE_DIR0 = vec2(0.8645072, 0.5026205);
const vec2 WAVE_DIR1 = vec2(-0.3401361, 0.9403762);
const vec2 WAVE_DIR2 = vec2(0.0995037, 0.9950372);
const vec2 WAVE_DIR3 = vec2(0.9615239, -0.2747211);
const vec3 SUN_DIR = vec3(0.0, 0.0846946, -0.9964070);

mat3 cameraBasis(vec3 ro, vec3 ta) {
  vec3 ww = normalize(ta - ro);
  vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
  vec3 vv = normalize(cross(uu, ww));
  return mat3(uu, vv, ww);
}

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  float a = hash12(i);
  float b = hash12(i + vec2(1.0, 0.0));
  float c = hash12(i + vec2(0.0, 1.0));
  float d = hash12(i + vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
  float f = 0.0;
  float a = 0.52;
  mat2 m = mat2(1.62, 1.18, -1.18, 1.62);
  for (int i = 0; i < 5; i++) {
    f += a * noise(p);
    p = m * p + 17.3;
    a *= 0.48;
  }
  return f;
}

float fbmLite(vec2 p) {
  float f = 0.0;
  float a = 0.52;
  mat2 m = mat2(1.62, 1.18, -1.18, 1.62);
  for (int i = 0; i < 3; i++) {
    f += a * noise(p);
    p = m * p + 17.3;
    a *= 0.48;
  }
  return f * 1.06;
}

float waveAnalytic(vec2 p) {
  float t = uTime;
  float h = 0.0;
  h += 0.38 * sin(dot(p, WAVE_DIR0) * 0.090 + t * 0.82);
  h += 0.22 * sin(dot(p, WAVE_DIR1) * 0.155 + t * 1.18);
  h += 0.13 * sin(dot(p, WAVE_DIR2) * 0.290 + t * 1.75);
  h += 0.08 * sin(dot(p, WAVE_DIR3) * 0.520 + t * 2.15);
  return h;
}

float waveHeight(vec2 p) {
  float h = waveAnalytic(p);
  h += (fbm(p * 0.075 + vec2(uTime * 0.035, -uTime * 0.018)) - 0.5) * 0.20;
  return h;
}

vec3 waveNormal(vec2 p) {
  float e = 0.85;
  float h = waveAnalytic(p);
  float hx = waveAnalytic(p + vec2(e, 0.0));
  float hz = waveAnalytic(p + vec2(0.0, e));
  return normalize(vec3(h - hx, e * 1.85, h - hz));
}

bool traceSea(vec3 ro, vec3 rd, out vec3 pos) {
  if (rd.y >= -0.002) return false;
  float invDown = 1.0 / max(-rd.y, 0.002);
  float t = ro.y * invDown;
  if (t <= 0.0 || t > FAR) return false;
  for (int i = 0; i < 5; i++) {
    vec3 p = ro + rd * t;
    float d = p.y - waveHeight(p.xz);
    t += d * invDown;
    t = clamp(t, 0.0, FAR);
  }
  pos = ro + rd * t;
  pos.y = waveHeight(pos.xz);
  return t > 0.0 && t < FAR;
}

bool traceCubePose(vec3 ro, vec3 rd, vec3 center, mat3 R, mat3 invR, out float tHit, out vec3 nWorld, out vec2 uv) {
  vec3 rlo = invR * (ro - center) / CUBE_HALF_SIZE;
  vec3 rld = invR * rd / CUBE_HALF_SIZE;
  vec3 invD = 1.0 / max(abs(rld), vec3(0.0001)) * sign(rld);
  vec3 t0 = (-vec3(1.0) - rlo) * invD;
  vec3 t1 = ( vec3(1.0) - rlo) * invD;
  vec3 tn3 = min(t0, t1);
  vec3 tf3 = max(t0, t1);
  float tn = max(max(tn3.x, tn3.y), tn3.z);
  float tf = min(min(tf3.x, tf3.y), tf3.z);
  if (tf < max(tn, 0.0)) return false;
  tHit = tn;
  if (tHit < 0.0) tHit = tf;
  if (tHit <= 0.0 || tHit > FAR) return false;
  vec3 lp = rlo + rld * tHit;
  vec3 an = abs(lp);
  vec3 nl;
  if (an.x > an.y && an.x > an.z) {
    nl = vec3(sign(lp.x), 0.0, 0.0);
    uv = lp.zy * vec2(sign(lp.x), 1.0) * 0.5 + 0.5;
  } else if (an.y > an.z) {
    nl = vec3(0.0, sign(lp.y), 0.0);
    uv = lp.xz * vec2(1.0, -sign(lp.y)) * 0.5 + 0.5;
  } else {
    nl = vec3(0.0, 0.0, sign(lp.z));
    uv = lp.xy * vec2(-sign(lp.z), 1.0) * 0.5 + 0.5;
  }
  nWorld = normalize(R * nl);
  return true;
}

vec3 skyColor(vec3 rd, vec3 sunDir) {
  float y = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
  float nearHorizon = pow(1.0 - clamp(abs(rd.y) * 2.2, 0.0, 1.0), 2.4);
  float lowSky = pow(1.0 - y, 2.0);
  float sunDot = max(dot(rd, sunDir), 0.0);
  vec3 nightTop = vec3(0.004, 0.014, 0.045);
  vec3 highBlue = vec3(0.018, 0.050, 0.115);
  vec3 lowerViolet = vec3(0.165, 0.105, 0.185);
  vec3 horizonRed = vec3(0.880, 0.175, 0.075);
  vec3 orangeBand = vec3(1.000, 0.390, 0.105);
  vec3 sky = mix(lowerViolet, highBlue, smoothstep(0.24, 0.70, y));
  sky = mix(sky, nightTop, smoothstep(0.66, 1.0, y));
  sky = mix(sky, horizonRed, nearHorizon * 0.78);
  sky += orangeBand * pow(nearHorizon, 3.2) * 0.55;
  sky += vec3(0.55, 0.05, 0.08) * pow(lowSky, 3.0) * 0.16;
  float sunCore = smoothstep(0.99505, 0.99875, sunDot);
  float sunDisk = smoothstep(0.9870, 0.9958, sunDot);
  float sunHalo = pow(sunDot, 12.0);
  float sunGlow = pow(sunDot, 120.0) * 3.6 + sunHalo * 0.82;
  sky += vec3(1.0, 0.55, 0.18) * sunGlow;
  sky = mix(sky, vec3(2.05, 1.78, 0.98), sunDisk * 0.98);
  sky = mix(sky, vec3(3.80, 3.45, 2.55), sunCore);
  return sky;
}

float cloudLayer(vec3 rd, float height, float scale, float speed, float softness) {
  if (rd.y <= 0.015) return 0.0;
  float t = height / rd.y;
  vec2 p = rd.xz * t * scale + vec2(uTime * speed, -uTime * speed * 0.28);
  float c = fbmLite(p) * 0.72 + fbmLite(p * 2.7 + vec2(14.1, -3.7)) * 0.28;
  c = smoothstep(softness, 1.0, c);
  return c * smoothstep(0.02, 0.23, rd.y) * smoothstep(0.86, 0.35, rd.y);
}

vec3 addStars(vec3 sky, vec3 rd, vec3 sunDir) {
  if (rd.y <= 0.08) return sky;
  float lum = dot(sky, vec3(0.299, 0.587, 0.114));
  if (lum >= 0.34) return sky;
  vec2 sp = vec2(atan(rd.x, -rd.z) / PI, rd.y) * vec2(135.0, 78.0);
  vec2 cell = floor(sp);
  vec2 local = fract(sp) - 0.5;
  float rnd = hash12(cell);
  float rare = step(0.955, rnd);
  vec2 off = vec2(hash12(cell + 21.7), hash12(cell + 47.2)) - 0.5;
  float d = length(local - off * 0.72);
  float size = mix(0.045, 0.105, hash12(cell + 9.4));
  float star = smoothstep(size, 0.0, d) * rare * (0.78 + 0.22 * sin(uTime * 1.8 + rnd * 80.0));
  float alt = smoothstep(0.10, 0.44, rd.y);
  float sunFade = 1.0 - smoothstep(0.93, 0.995, max(dot(rd, sunDir), 0.0));
  float dark = smoothstep(0.34, 0.10, lum);
  return sky + vec3(0.88, 0.93, 1.0) * 1.55 * star * alt * sunFade * dark;
}

vec3 addClouds(vec3 sky, vec3 rd, vec3 sunDir) {
  float c = clamp(
    cloudLayer(rd, 75.0, 0.0105, 0.0035, 0.515) * 0.95 +
    cloudLayer(rd, 155.0, 0.0060, -0.0020, 0.575) * 0.70,
    0.0,
    1.0
  );
  float lit = pow(max(dot(rd, sunDir), 0.0), 3.2);
  vec3 warm = vec3(1.0, 0.54, 0.27) * (0.26 + 1.35 * lit);
  vec3 cloudCol = mix(
    vec3(0.14, 0.18, 0.28),
    mix(vec3(0.50, 0.50, 0.55), warm, smoothstep(0.1, 0.9, lit)),
    0.45 + 0.55 * smoothstep(0.0, 0.55, rd.y)
  );
  c *= 1.0 - smoothstep(0.987, 0.9975, max(dot(rd, sunDir), 0.0)) * 0.70;
  return mix(sky, cloudCol, c * 0.56);
}

vec3 cubeColor(vec3 ro, vec3 rd, float tHit, vec3 n, vec2 uv, vec3 center, vec3 sunDir, vec3 sky) {
  vec3 pos = ro + rd * tHit;
  vec3 baseColor = vec3(0.045, 0.045, 0.055);
  vec3 faceColor = baseColor;
  if (uLogoReady != 0) {
    vec4 logoTex = texture(uLogo, clamp(uv, 0.003, 0.997));
    faceColor = mix(baseColor, logoTex.rgb, logoTex.a);
  }
  float ndotl = max(dot(n, sunDir), 0.0);
  float skyLight = 0.44 + 0.38 * max(n.y, 0.0);
  float rim = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
  float spec = pow(max(dot(reflect(-sunDir, n), -rd), 0.0), 72.0);
  vec3 col = faceColor * (vec3(0.18, 0.20, 0.28) * skyLight + vec3(1.0, 0.50, 0.20) * ndotl * 1.05);
  col += vec3(1.0, 0.86, 0.52) * spec * 0.36;
  col += sky * rim * 0.10;
  float d = length(pos - ro);
  float fog = 1.0 - exp(-d * 0.013);
  return mix(col, sky, fog * 0.28);
}

float cubeSeaShadow(vec3 seaPos, vec3 cubeCenter, vec3 sunDir) {
  vec2 lightPlanar = normalize(-sunDir.xz + vec2(0.0001));
  vec2 side = vec2(-lightPlanar.y, lightPlanar.x);
  float projectedLength = clamp((cubeCenter.y - seaPos.y) / max(sunDir.y, 0.035), 2.0, mix(13.0, 18.0, uMood));
  vec2 shadowCenter = cubeCenter.xz + lightPlanar * projectedLength * 0.46;
  vec2 d = seaPos.xz - shadowCenter;
  float along = dot(d, lightPlanar);
  float across = dot(d, side);
  float longSoft = exp(-(along * along) / mix(24.0, 26.0, uMood));
  float wideSoft = exp(-(across * across) / mix(3.6, 5.8, uMood));
  float contact = exp(-dot(seaPos.xz - cubeCenter.xz, seaPos.xz - cubeCenter.xz) / mix(3.1, 3.8, uMood));
  float broken = mix(0.74, 0.82, uMood) + mix(0.26, 0.18, uMood) * fbmLite(seaPos.xz * 1.05 + vec2(uTime * 0.18, -uTime * 0.07));
  float strength = mix(0.72, 1.05, uMood);
  return clamp(max(longSoft * wideSoft, contact * mix(0.72, 1.08, uMood)) * broken * strength, 0.0, 1.0);
}

vec3 cubeSeaReflectionStreak(vec3 seaPos, vec3 cubeCenter, out float mask) {
  vec2 towardCamera = normalize(vec2(0.0) - cubeCenter.xz + vec2(0.0001));
  vec2 side = vec2(-towardCamera.y, towardCamera.x);
  vec2 d = seaPos.xz - cubeCenter.xz;
  float along = dot(d, towardCamera);
  float across = dot(d, side);
  float endNear = mix(8.5, 11.0, uMood);
  float endFar = mix(13.0, 17.0, uMood);
  float forward = smoothstep(-0.25, 0.68, along) * (1.0 - smoothstep(endNear, endFar, along));
  float width = mix(mix(1.18, 0.42, clamp(along / 11.0, 0.0, 1.0)), mix(1.75, 0.34, clamp(along / 18.0, 0.0, 1.0)), uMood);
  float lateral = exp(-(across * across) / (width * width));
  float bands = 0.50 + 0.50 * sin(along * mix(5.2, 6.4, uMood) + fbmLite(seaPos.xz * 0.80 + uTime * 0.04) * 6.0);
  bands = smoothstep(mix(0.38, 0.50, uMood), 0.94, bands);
  float waveCut = 0.54 + 0.46 * fbmLite(seaPos.xz * 1.65 + vec2(uTime * 0.18, uTime * 0.06));
  float distanceFade = exp(-max(along, 0.0) * mix(0.11, 0.095, uMood));
  mask = clamp(forward * lateral * bands * waveCut * distanceFade, 0.0, 1.0);
  vec2 logoUv = vec2(fract(across * 0.22 + 0.5 + 0.05 * sin(along * 2.0)), fract(along * 0.12 + 0.2));
  vec3 logo = vec3(0.95, 0.78, 0.20);
  if (uLogoReady != 0) logo = texture(uLogo, logoUv).rgb;
  return mix(vec3(0.90, 0.54, 0.12), logo * vec3(1.10, 0.88, 0.40), mix(0.30, 0.52, uMood));
}

vec3 tonemap(vec3 c) {
  c = max(c, vec3(0.0));
  c = c * (1.0 + c * 0.055) / (1.0 + c);
  return pow(c, vec3(1.0 / 2.2));
}

vec3 render(vec2 fragCoord) {
  vec2 p = (fragCoord * 2.0 - uResolution.xy) / uResolution.y;
  float yaw = uLook.x;
  float pitch = uLook.y;
  vec3 ro = vec3(0.0, 2.75, 0.0);
  vec3 forward = normalize(vec3(sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch)));
  mat3 cam = cameraBasis(ro, ro + forward);
  vec3 rd = normalize(cam * vec3(p * uFov, 1.0));
  vec3 sunDir = SUN_DIR;
  vec3 sky = skyColor(rd, sunDir);
  sky = addStars(sky, rd, sunDir);
  sky = addClouds(sky, rd, sunDir);
  vec3 col = sky;
  vec3 cubeN;
  vec2 cubeUv;
  float cubeT = FAR;
  bool cubeHit = traceCubePose(ro, rd, uCubeCenter, uCubeR, uCubeInvR, cubeT, cubeN, cubeUv);
  vec3 seaPos;
  float seaDist = FAR;
  if (traceSea(ro, rd, seaPos)) {
    seaDist = length(seaPos - ro);
    vec3 n = waveNormal(seaPos.xz);
    vec3 refl = reflect(rd, n);
    vec3 reflectedSky = skyColor(refl, sunDir);
    reflectedSky = addClouds(reflectedSky, refl, sunDir);
    float ndotl = max(dot(n, sunDir), 0.0);
    float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
    fresnel = mix(0.025, 0.88, fresnel);
    float sunFacing = max(dot(reflect(-sunDir, n), -rd), 0.0);
    float sunSpec = pow(sunFacing, 720.0);
    float broadGlitter = pow(sunFacing, 78.0);
    float glitterMask = smoothstep(0.44, 0.83, fbmLite(seaPos.xz * 0.42 + vec2(uTime * 0.15, uTime * 0.04)));
    vec3 water = mix(vec3(0.006, 0.045, 0.095), vec3(0.015, 0.130, 0.225), 0.42 + 0.58 * max(dot(n, vec3(0.0, 1.0, 0.0)), 0.0));
    water = mix(water, vec3(0.04, 0.20, 0.31), ndotl * 0.18);
    water += vec3(0.015, 0.04, 0.06) * fbmLite(seaPos.xz * 0.11 - uTime * 0.04);
    vec3 sea = mix(water, reflectedSky, fresnel);
    float cubeShadow = cubeSeaShadow(seaPos, uCubeCenter, sunDir);
    sea *= 1.0 - cubeShadow * mix(0.54, 0.82, uMood);
    sea -= vec3(0.045, 0.034, 0.025) * cubeShadow * mix(0.58, 1.00, uMood);
    glitterMask *= 1.0 - cubeShadow * mix(0.72, 0.94, uMood);
    if (fresnel > 0.05) {
      vec3 reflN;
      vec2 reflUv;
      float reflT = FAR;
      bool reflHit = traceCubePose(seaPos + n * 0.10, refl, uCubeCenter, uCubeR, uCubeInvR, reflT, reflN, reflUv);
      if (reflHit) {
        vec3 cubeRefl = cubeColor(seaPos, refl, reflT, reflN, reflUv, uCubeCenter, sunDir, reflectedSky);
        float reflFade = exp(-reflT * mix(0.082, 0.052, uMood)) * smoothstep(0.05, 0.28, fresnel);
        float waveBreak = 0.68 + 0.32 * fbmLite(seaPos.xz * 1.20 + vec2(uTime * 0.11, uTime * 0.05));
        sea = mix(sea, cubeRefl, clamp(reflFade * waveBreak * mix(0.34, 0.36, uMood), 0.0, mix(0.28, 0.30, uMood)));
      }
    }
    float streakMask;
    vec3 streakCol = cubeSeaReflectionStreak(seaPos, uCubeCenter, streakMask);
    float closeWater = 1.0 - smoothstep(mix(12.0, 18.0, uMood), mix(26.0, 42.0, uMood), seaDist);
    sea = mix(sea, streakCol, clamp(streakMask * closeWater * (1.0 - cubeShadow * mix(0.55, 0.38, uMood)) * mix(0.18, 0.18, uMood), 0.0, mix(0.22, 0.22, uMood)));
    sea += vec3(1.0, 0.53, 0.20) * broadGlitter * glitterMask * 0.85;
    sea += vec3(1.0, 0.78, 0.42) * sunSpec * glitterMask * 2.3;
    float hGlow = exp(-abs(rd.y) * 95.0) * pow(max(dot(normalize(vec3(rd.x, 0.0, rd.z)), normalize(vec3(sunDir.x, 0.0, sunDir.z))), 0.0), 10.0);
    sea += vec3(0.95, 0.34, 0.12) * hGlow * 0.17;
    float fog = 1.0 - exp(-seaDist * 0.0105);
    vec3 fogCol = skyColor(normalize(vec3(rd.x, max(rd.y, 0.018), rd.z)), sunDir);
    col = mix(sea, fogCol, fog * 0.70);
  }
  if (cubeHit && cubeT < seaDist) {
    col = cubeColor(ro, rd, cubeT, cubeN, cubeUv, uCubeCenter, sunDir, sky);
  }
  vec2 q = fragCoord / uResolution.xy;
  float vignette = smoothstep(1.18, 0.24, length(q - 0.5));
  col *= 0.78 + 0.22 * vignette;
  vec3 softCol = col * 0.96 + vec3(0.008, 0.010, 0.014);
  vec3 cineCol = col * vec3(1.12, 1.04, 0.94) + vec3(0.012, 0.003, 0.000);
  cineCol = mix(vec3(dot(cineCol, vec3(0.299, 0.587, 0.114))), cineCol, 1.12);
  col = mix(softCol, cineCol, uMood);
  col += (hash12(fragCoord + fract(uTime) * 31.7) - 0.5) / 255.0;
  return tonemap(col);
}

void main() {
  // nx.js bridge maps gl.viewport(0, 0, w, h) to native GL rows
  // [screenH - h, screenH]. On the fullscreen 1280x720 canvas the offset
  // is zero, but driving fragCoord off vUv keeps the math
  // viewport-offset-independent in case the bridge ever lands a
  // non-fullscreen viewport.
  vec2 fragCoord = vUv * uResolution.xy;
  fragColor = vec4(render(fragCoord), 1.0);
}
`;

function compile(type: GLenum, src: string, label: string): WebGLShader {
	const s = gl!.createShader(type)!;
	gl!.shaderSource(s, src);
	gl!.compileShader(s);
	if (!gl!.getShaderParameter(s, gl!.COMPILE_STATUS)) {
		throw new Error(label + ' compile: ' + gl!.getShaderInfoLog(s));
	}
	return s;
}

const vs = compile(gl.VERTEX_SHADER, vertexSource, 'vs');
const fs = compile(gl.FRAGMENT_SHADER, fragmentSource, 'fs');
const program = gl.createProgram()!;
gl.attachShader(program, vs);
gl.attachShader(program, fs);
gl.linkProgram(program);
if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
	throw new Error('link: ' + gl.getProgramInfoLog(program));
}
gl.deleteShader(vs);
gl.deleteShader(fs);
gl.useProgram(program);

// gl_VertexID drives a const-array fullscreen triangle in the vertex
// shader — no attribute data needed. A VAO is still required by GLSL ES
// 3.00 even when there are no enabled attribs.
const vao = gl.createVertexArray()!;
gl.bindVertexArray(vao);

// nx.js's drawArrays gate requires attribute 0 to be enabled with type
// FLOAT and size >= 2 BEFORE the raw-passthrough dispatch — the gate
// was designed for the bridge's hardcoded `a_position`-bound color/
// texture programs. Our shader uses gl_VertexID + a const-array
// triangle and never reads from a vertex attribute, but the gate would
// set GL_INVALID_OPERATION (0x502) every frame without ever dispatching
// the draw. Bind a tiny dummy buffer + enable attr 0 to satisfy the
// gate; the shader ignores whatever data sits there.
const dummyBuf = gl.createBuffer()!;
gl.bindBuffer(gl.ARRAY_BUFFER, dummyBuf);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0, 0, 0, 0, 0, 0]), gl.STATIC_DRAW);
gl.enableVertexAttribArray(0);
gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

const U = {
	res: gl.getUniformLocation(program, 'uResolution'),
	time: gl.getUniformLocation(program, 'uTime'),
	look: gl.getUniformLocation(program, 'uLook'),
	fov: gl.getUniformLocation(program, 'uFov'),
	mood: gl.getUniformLocation(program, 'uMood'),
	logo: gl.getUniformLocation(program, 'uLogo'),
	logoReady: gl.getUniformLocation(program, 'uLogoReady'),
	cubeCenter: gl.getUniformLocation(program, 'uCubeCenter'),
	cubeR: gl.getUniformLocation(program, 'uCubeR'),
	cubeInvR: gl.getUniformLocation(program, 'uCubeInvR'),
};

// ----- Logo texture -----
// 1x1 placeholder so the sampler stays valid until the PNG arrives.
// Pre-allocate the persistent native texture handle BEFORE the async
// PNG decode lands — otherwise the later upload only populates nx.js's
// CPU-side cache and the bridge won't have a native handle to bind.
const TEX_SIZE = 1024;
const logoTexture = gl.createTexture()!;
gl.activeTexture(gl.TEXTURE0);
gl.bindTexture(gl.TEXTURE_2D, logoTexture);
gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
	new Uint8Array([220, 230, 255, 255]));
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
gl.uniform1i(U.logo, 0);

let logoReady = false;
const logoImage = new Image();
logoImage.onload = () => {
	try {
		const off = new OffscreenCanvas(TEX_SIZE, TEX_SIZE);
		const tctx = off.getContext('2d')!;
		tctx.fillStyle = '#111111';
		tctx.fillRect(0, 0, TEX_SIZE, TEX_SIZE);
		const pad = 96;
		// Vertically flip before upload — GL textures sample
		// origin-at-bottom-left but `UNPACK_FLIP_Y_WEBGL` is a no-op in
		// nx.js's bridge. Flipping in the 2D canvas (translate + scale
		// 1, -1) means the bytes that getImageData returns are already
		// in the orientation the sampler expects.
		tctx.save();
		tctx.translate(0, TEX_SIZE);
		tctx.scale(1, -1);
		tctx.drawImage(logoImage as any, pad, pad, TEX_SIZE - pad * 2, TEX_SIZE - pad * 2);
		tctx.restore();
		const imgData = tctx.getImageData(0, 0, TEX_SIZE, TEX_SIZE);
		gl!.activeTexture(gl!.TEXTURE0);
		gl!.bindTexture(gl!.TEXTURE_2D, logoTexture);
		gl!.texImage2D(gl!.TEXTURE_2D, 0, gl!.RGBA, TEX_SIZE, TEX_SIZE, 0,
			gl!.RGBA, gl!.UNSIGNED_BYTE, new Uint8Array(imgData.data.buffer));
		gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_MIN_FILTER, gl!.LINEAR);
		gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_MAG_FILTER, gl!.LINEAR);
		gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_S, gl!.CLAMP_TO_EDGE);
		gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_T, gl!.CLAMP_TO_EDGE);
		logoReady = true;
	} catch (e) {
		console.debug('[canvas-webgl2] logo upload failed:', e);
	}
};
logoImage.onerror = () => {
	console.debug('[canvas-webgl2] failed to load romfs:/logo.png');
};
(logoImage as any).src = 'romfs:/logo.png';

// ----- Camera + input state -----
let yaw = 0.0;
let pitch = -0.035;
let targetYaw = 0.0;
let targetPitch = -0.035;
let fov = 1.06;
const targetFov = 1.06;
const mood = 1.0;
const clamp = (x: number, a: number, b: number) => Math.max(a, Math.min(b, x));

// Drag-to-orbit. Mirrors the canvas-webgl pattern (single-touch only,
// identifier-tracked so a second finger doesn't hijack the drag).
let dragActive = false;
let activeTouchId: number | null = null;
let lastX = 0, lastY = 0;
function startDrag(x: number, y: number): void { dragActive = true; lastX = x; lastY = y; }
function moveDrag(x: number, y: number): void {
	if (!dragActive) return;
	const dx = x - lastX;
	const dy = y - lastY;
	lastX = x;
	lastY = y;
	targetYaw = clamp(targetYaw + dx * 0.0032, -0.78, 0.78);
	targetPitch = clamp(targetPitch - dy * 0.0024, -0.22, 0.18);
}
function endDrag(): void { dragActive = false; activeTouchId = null; }

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

// Gamepad right-stick = orbit (standard mapping: axis 2 = right-X,
// axis 3 = right-Y).
function pollGamepad(dt: number): void {
	const pads = navigator.getGamepads();
	const gp = pads && pads[0];
	if (!gp) return;
	const dead = 0.12;
	const rx = gp.axes[2] || 0;
	const ry = gp.axes[3] || 0;
	if (Math.abs(rx) > dead) targetYaw = clamp(targetYaw + rx * dt * 1.6, -0.78, 0.78);
	if (Math.abs(ry) > dead) targetPitch = clamp(targetPitch - ry * dt * 0.6, -0.22, 0.18);
}

// ----- CPU-side cube pose, mirroring the GLSL cubePose() exactly -----
const CUBE_HALF_SIZE = 2.76;
const ANCHOR_X = -8.2;
const ANCHOR_Z = -10.5;
const WAVE_DIRS: ReadonlyArray<readonly [number, number]> = [
	[ 0.8645072,  0.5026205],
	[-0.3401361,  0.9403762],
	[ 0.0995037,  0.9950372],
	[ 0.9615239, -0.2747211],
];
function hash12CPU(x: number, y: number): number {
	let px = (x * 0.1031) % 1; if (px < 0) px += 1;
	let py = (y * 0.1031) % 1; if (py < 0) py += 1;
	let pz = (x * 0.1031) % 1; if (pz < 0) pz += 1;
	const d = px * (py + 33.33) + py * (pz + 33.33) + pz * (px + 33.33);
	px += d; py += d; pz += d;
	let r = ((px + py) * pz) % 1;
	if (r < 0) r += 1;
	return r;
}
function noiseCPU(x: number, y: number): number {
	const ix = Math.floor(x), iy = Math.floor(y);
	const fx = x - ix, fy = y - iy;
	const ux = fx * fx * (3 - 2 * fx);
	const uy = fy * fy * (3 - 2 * fy);
	const a = hash12CPU(ix, iy);
	const b = hash12CPU(ix + 1, iy);
	const c = hash12CPU(ix, iy + 1);
	const d = hash12CPU(ix + 1, iy + 1);
	return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uy;
}
function fbmCPU(x: number, y: number): number {
	let f = 0, a = 0.52, px = x, py = y;
	for (let i = 0; i < 5; i++) {
		f += a * noiseCPU(px, py);
		const nx = 1.62 * px + 1.18 * py + 17.3;
		const ny = -1.18 * px + 1.62 * py + 17.3;
		px = nx; py = ny;
		a *= 0.48;
	}
	return f;
}
function waveHeightCPU(x: number, z: number, t: number): number {
	let h = 0;
	h += 0.38 * Math.sin((x * WAVE_DIRS[0][0] + z * WAVE_DIRS[0][1]) * 0.090 + t * 0.82);
	h += 0.22 * Math.sin((x * WAVE_DIRS[1][0] + z * WAVE_DIRS[1][1]) * 0.155 + t * 1.18);
	h += 0.13 * Math.sin((x * WAVE_DIRS[2][0] + z * WAVE_DIRS[2][1]) * 0.290 + t * 1.75);
	h += 0.08 * Math.sin((x * WAVE_DIRS[3][0] + z * WAVE_DIRS[3][1]) * 0.520 + t * 2.15);
	h += (fbmCPU(x * 0.075 + t * 0.035, z * 0.075 - t * 0.018) - 0.5) * 0.20;
	return h;
}
const cubeCenterArr = new Float32Array(3);
const cubeRArr = new Float32Array(9);
const cubeInvRArr = new Float32Array(9);
const tmpA = new Float32Array(9);
const rY = new Float32Array(9);
const rX = new Float32Array(9);
const rZ = new Float32Array(9);
function mat3Mul(out: Float32Array, a: Float32Array, b: Float32Array): void {
	for (let c = 0; c < 3; c++) {
		for (let r = 0; r < 3; r++) {
			out[c * 3 + r] =
				a[0 * 3 + r] * b[c * 3 + 0] +
				a[1 * 3 + r] * b[c * 3 + 1] +
				a[2 * 3 + r] * b[c * 3 + 2];
		}
	}
}
function mat3Transpose(out: Float32Array, m: Float32Array): void {
	out[0] = m[0]; out[1] = m[3]; out[2] = m[6];
	out[3] = m[1]; out[4] = m[4]; out[5] = m[7];
	out[6] = m[2]; out[7] = m[5]; out[8] = m[8];
}
function rotYCPU(out: Float32Array, a: number): void {
	const s = Math.sin(a), c = Math.cos(a);
	out[0] = c; out[1] = 0; out[2] = s;
	out[3] = 0; out[4] = 1; out[5] = 0;
	out[6] = -s; out[7] = 0; out[8] = c;
}
function rotXCPU(out: Float32Array, a: number): void {
	const s = Math.sin(a), c = Math.cos(a);
	out[0] = 1; out[1] = 0; out[2] = 0;
	out[3] = 0; out[4] = c; out[5] = -s;
	out[6] = 0; out[7] = s; out[8] = c;
}
function rotZCPU(out: Float32Array, a: number): void {
	const s = Math.sin(a), c = Math.cos(a);
	out[0] = c; out[1] = -s; out[2] = 0;
	out[3] = s; out[4] = c;  out[5] = 0;
	out[6] = 0; out[7] = 0;  out[8] = 1;
}
function updateCubePose(t: number): void {
	const seaBob = waveHeightCPU(ANCHOR_X, ANCHOR_Z, t);
	cubeCenterArr[0] = ANCHOR_X;
	cubeCenterArr[1] = seaBob + CUBE_HALF_SIZE * 0.33 + Math.sin(t * 0.72) * 0.10;
	cubeCenterArr[2] = ANCHOR_Z;
	const waveRoll = waveHeightCPU(ANCHOR_X + 2.0, ANCHOR_Z, t) - waveHeightCPU(ANCHOR_X - 2.0, ANCHOR_Z, t);
	const wavePitch = waveHeightCPU(ANCHOR_X, ANCHOR_Z + 2.0, t) - waveHeightCPU(ANCHOR_X, ANCHOR_Z - 2.0, t);
	rotYCPU(rY, t * 0.22);
	rotXCPU(rX, wavePitch * 0.22 + Math.sin(t * 0.52) * 0.05);
	rotZCPU(rZ, -waveRoll * 0.22);
	mat3Mul(tmpA, rY, rX);
	mat3Mul(cubeRArr, tmpA, rZ);
	mat3Transpose(cubeInvRArr, cubeRArr);
}

// ----- Constant uniforms + frame loop -----
gl.viewport(0, 0, W, H);
gl.uniform1f(U.mood, mood);
gl.uniform2f(U.res, W, H);
// The bridge's auto-present path runs from inside `gl.clear()`: it
// detects `bridge_pending_readback` from the previous frame and blits
// the bridge FBO to the screen canvas surface BEFORE clearing the new
// frame. A draw-only loop (no clear) accumulates pending readbacks
// forever and the screen stays black — exactly what would happen with
// a fullscreen shader that "covers everything" and skips the clear.
// Set a black clear color once; per-frame clear below triggers present.
gl.clearColor(0, 0, 0, 1);

const start = performance.now();
let lastNow = start;

function frame(now: number): void {
	requestAnimationFrame(frame);
	const dt = Math.min(0.05, Math.max(0.001, (now - lastNow) * 0.001));
	lastNow = now;
	pollGamepad(dt);
	yaw += (targetYaw - yaw) * 0.085;
	pitch += (targetPitch - pitch) * 0.085;
	fov += (targetFov - fov) * 0.075;

	const tSec = (now - start) * 0.001;
	updateCubePose(tSec);

	gl!.uniform1f(U.time, tSec);
	gl!.uniform2f(U.look, yaw, pitch);
	gl!.uniform1f(U.fov, fov);
	gl!.uniform1i(U.logoReady, logoReady ? 1 : 0);
	gl!.uniform3fv(U.cubeCenter, cubeCenterArr);
	gl!.uniformMatrix3fv(U.cubeR, false, cubeRArr);
	gl!.uniformMatrix3fv(U.cubeInvR, false, cubeInvRArr);

	// Frame-start clear. Cosmetically a no-op (the fullscreen triangle
	// covers every pixel) but it's the trigger for the bridge's
	// `flush_bridge_present` path that blits the previous frame's FBO
	// to the on-screen canvas surface. Without this the loop runs at
	// 60 fps but the screen stays black.
	gl!.clear(gl!.COLOR_BUFFER_BIT);

	// Clear stale error before draw so any post-draw error reflects only
	// this frame (matches the bridge's stale-glerror handling).
	gl!.getError();
	gl!.drawArrays(gl!.TRIANGLES, 0, 3);
}
requestAnimationFrame(frame);
