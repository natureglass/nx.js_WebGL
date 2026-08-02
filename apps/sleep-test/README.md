# sleep-test — nx.js sleep/resume crash probe

Minimal app to reproduce and diagnose the **"console sleeps (screen off), wake
it, the app shows for ~1s then crashes"** bug on real Switch hardware.

It does nothing but animate a 2D canvas in a `requestAnimationFrame` loop, which
keeps the engine presenting a frame (`eglSwapBuffers`) every tick — the same
present path real apps use. The **engine** (`source/main.cc`) is where the
diagnostics live:

- a **CPU-exception handler** (`__libnx_exception_handler`) that dumps the
  faulting PC / LR / fault-address / registers the instant an in-process fault
  hits — this is what makes a crash that previously "logged nothing" visible;
- a **per-frame trace** armed the moment we resume (via the applet `OnResume`
  hook and a wall-clock gap detector), `fflush`ed every line so the **last line
  before the crash names the exact step that died**;
- an **applet-lifecycle log** of the focus / resume / operation-mode transitions
  around a sleep.

## Build

```sh
# 1) engine (produces the instrumented ../../nxjs.nro) — devkitPro msys2 bash:
#    cd /d/Workspace/nxjs-source-v8 && make
# 2) this app (from repo root):
pnpm --filter sleep-test build
pnpm --filter sleep-test nro --fat   # self-contained sleep-test.nro
```

## Test on hardware

1. Copy `sleep-test.nro` to the SD card (e.g. `sdmc:/switch/`), launch it.
2. Let it run a few seconds — the `frame` counter should climb smoothly.
3. Press **POWER** to sleep (screen off), wait ~2s, press **POWER** to wake.
4. Observe the ~1s-then-crash.

## Collect after the crash

- `sdmc:/switch/nxjs-crash.log` — the faulting PC/LR/registers (if the fault was
  in-process). Present ⇒ the crash is a CPU fault in our process.
- `sdmc:/switch/nxjs-debug.log` — the heartbeat + per-frame trace. Look at the
  **last `[fN] ...` line**: e.g. `pre skia_gpu_present (eglSwapBuffers)` with no
  matching `post` ⇒ it died inside the GPU present/swap. Also note the
  `[applet] hook=...` sequence around the sleep.

Send both logs back.
