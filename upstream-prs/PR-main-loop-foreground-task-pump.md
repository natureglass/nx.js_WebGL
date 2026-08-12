# PR-main-loop-foreground-task-pump — Pump V8's foreground task queue in the main loop

**Branch:** `upstream-pr/main-loop-foreground-task-pump`
**Base:** `upstream/main`
**Local base:** `8981cc6`
**Draft status.** Implemented; correctness/robustness fix. Kept intentionally
separate from the WebAssembly PR because it's a general engine-loop change,
not wasm-specific.

## What's in the commit

```
 source/main.cc | ~6 +
 1 file changed
```

## PR title (suggested)

`main: drain V8 foreground platform tasks each loop iteration`

## Motivation

The main loop advances two of the three work sources V8 can queue:

```c
uv_run(&loop, UV_RUN_NOWAIT);       // libuv: sockets, fs, dns, timers, threadpool afters
iso->PerformMicrotaskCheckpoint();  // V8 microtasks (promise reactions)
```

It never pumps the third: V8's **foreground task queue**.
`NewSingleThreadedDefaultPlatform()` has no worker threads, so V8 runs its
"background" work (async compilation jobs, some GC/finalization tasks, and
their foreground completion tasks) on the foreground task runner, which is
drained only by `v8::platform::PumpMessageLoop(platform, isolate, ...)`.
With nothing pumping it, any V8 feature that posts a foreground task to make
progress can stall silently while the loop otherwise looks healthy (timers
still fire).

## The change

Drain the foreground queue right after `uv_run` and before the microtask
checkpoint (so a task that resolves a promise gets its reactions serviced
the same turn):

```c
uv_run(&loop, UV_RUN_NOWAIT);
// Drain V8 foreground platform tasks. Single-threaded default platform has
// no worker threads, so V8 runs "background" work on the foreground task
// runner; nothing else pumps it. kDoNotWait never blocks.
while (v8::platform::PumpMessageLoop(
           platform.get(), iso,
           v8::platform::MessageLoopBehavior::kDoNotWait)) {
}
iso->PerformMicrotaskCheckpoint();
```

`platform` is the existing `std::unique_ptr<Platform>` local; note the
local shadows the `v8::platform` namespace, hence the fully-qualified
`v8::platform::PumpMessageLoop`.

## Behavior

- Foreground platform tasks now make progress every loop iteration.
- `kDoNotWait` means the pump never blocks; when the queue is empty it's a
  cheap no-op, so steady-state cost is negligible.
- No ordering change to libuv or microtasks beyond adding the drain between
  them.

## Scope / honesty for review

This is the correct embedder pattern (`d8`/Node pump the message loop), and
it's a prerequisite for any V8 feature that relies on foreground-task
delivery on this platform. In the current codebase its observable impact is
limited — the WebAssembly async-load stall that motivated the investigation
turned out to be fixed at the JS layer (see
`PR-wasm-async-single-threaded`), and most other async work goes through
microtasks or libuv. It's included because draining the foreground queue is
simply correct, harmless (empty-queue no-op), and forward-looking.

## Testing

Real hardware: no regressions observed across the Unity WebGL suite with the
pump in place; loop timing (heartbeat cadence, input, paint) unchanged.
