# PR-sleep-resume-loop-fork — Fix crash/hang after wake-from-sleep for apps that use the libuv threadpool

**Branch:** `upstream-pr/sleep-resume-loop-fork`
**Base:** `upstream/main`
**Draft status.** Fix implemented and **confirmed on real hardware** (see
Testing). Not yet pushed / no PR opened; this entry captures the root-cause
analysis + minimal fix so the PR can be assembled on demand.

## What's in the commit

One file, additive:

```
 source/main.cc | ~40 +
 1 file changed, ~40 insertions(+)
```

No new libnx service dependency (`applet` is already initialized), no new link
dependency (`uv_loop_fork` is already in the linked libuv), no API/ABI change.

## Symptom

On real hardware, an nx.js app that performs any asynchronous I/O — `fetch()`,
DNS, `fs` reads, crypto, compression, image decode, anything routed through the
libuv threadpool — **crashes or hangs on the first such operation after the
console wakes from sleep.** Pure main-thread apps (a 2D-canvas clock, a WebGL
demo) are unaffected and survive sleep indefinitely. In practice this bites the
moment a user sleeps the console mid-session and then does anything network
(e.g. a "check for updates" button) after waking.

## Root cause

libuv's threadpool wakes the event loop when a worker finishes by calling
`uv_async_send()`, which `write()`s a byte to the loop's **async self-pipe**. On
most platforms that self-pipe is an `eventfd`/`pipe2`; the Switch (Horizon) port
has neither, so its `pipe()` is implemented as a **loopback TCP socketpair**
(`socket`/`bind`/`listen`/`connect`/`accept`). That socketpair is created once
at `uv_loop_init` time and lives for the whole process.

When the Switch sleeps, the system tears down the `bsdsocket` layer, which
**invalidates every socket fd — including the long-lived async self-pipe.** Fresh
sockets opened after wake work fine (that is why a post-resume DNS `getaddrinfo`
itself succeeds), but the boot-created self-pipe fd is now dead. So the sequence
after wake is:

1. App issues `fetch()` → a DNS resolve is queued to the threadpool.
2. A worker runs `getaddrinfo()` — succeeds (fresh socket).
3. The worker returns into libuv's `worker()` and calls `uv_async_send()` to
   notify the loop → `write()` to the **dead** self-pipe fd →
   `write()` fails unexpectedly → libuv `abort()` (or, depending on timing, a
   fault / a hang in the threadpool return path).

This exactly matches the observed behavior: only threadpool-using code paths
crash, they crash *after* the work itself completed, and it is 100%
reproducible on the first async op after each wake.

(Confirmed empirically: the crash always lands in libuv's threadpool `worker()`
post-work path; `libuv-horizon-port.o`'s `pipe()` is a loopback socketpair; and
the fix below makes it survive indefinitely across repeated sleep/wake cycles.)

## The fix

libuv already ships the exact tool for "the loop's kernel/socket state is no
longer valid, rebuild it": **`uv_loop_fork()`** (its POSIX use case is the child
after `fork()`). Its `uv__async_fork()` closes the old async fds and calls
`uv__make_pipe()` to create a **fresh** self-pipe, then re-registers it
(`uv__io_init_start`); `uv__io_fork()` rebuilds the backend poll set.

So the fix is: **on wake-from-sleep, call `uv_loop_fork(&loop)` on the loop
thread before any new async work can be queued.**

Wiring in `main.cc`:

- Register an `appletHook`. On `AppletHookType_OnResume` (or an
  `OnFocusState` transition back to `AppletFocusState_InFocus`), set a
  `g_need_loop_fork` flag. `appletSetRestartMessageEnabled(true)` is called so
  the Resume message is delivered.
- In the main loop, immediately after `appletMainLoop()` (which delivers the
  hook) and **before** `uv_run()` / the JS frame handler that could issue a new
  `fetch()`, if the flag is set, clear it and call `uv_loop_fork(&loop)`.

The hook only sets a flag; the fork runs on the loop thread (where
`uv_loop_fork` must be called). Idle threadpool workers are parked in the
threadpool's own condvar — not the loop — so recreating the loop's self-pipe
doesn't race them, and there is no in-flight async work at the resume instant
(the pre-sleep tasks completed before sleep).

```c
// globals
static volatile bool g_need_loop_fork = false;
static AppletHookCookie g_applet_hook_cookie;

static void nx_applet_hook_cb(AppletHookType hook, void *) {
    if (hook == AppletHookType_OnResume ||
        (hook == AppletHookType_OnFocusState &&
         appletGetFocusState() == AppletFocusState_InFocus))
        g_need_loop_fork = true;
}

// before the loop
appletSetRestartMessageEnabled(true);
appletHook(&g_applet_hook_cookie, nx_applet_hook_cb, nullptr);

// top of the loop, right after appletMainLoop()
if (g_need_loop_fork) {
    g_need_loop_fork = false;
    int frk = uv_loop_fork(&loop);
    if (frk != 0) { fprintf(stderr, "[nx] uv_loop_fork failed: %d\n", frk); fflush(stderr); }
}
```

## Compatibility

- **Zero cost when never sleeping.** The hook only flips a flag; `uv_loop_fork`
  runs at most once per wake.
- **Additive.** No existing name/behavior changes; apps that don't use async
  I/O are unaffected either way.
- **No new deps.** `uv_loop_fork` is already linked; `appletHook` /
  `appletSetRestartMessageEnabled` are libnx `applet` (always initialized).

## Testing

- **Confirmed on real hardware (application mode).** A minimal probe that
  `fetch()`es a URL every 3 s: before the fix, the first fetch after wake
  crashed 100% of the time (Atmosphère "instruction abort" in libuv's threadpool
  `worker()`, or a hang with no report); with the fix it survives **repeated**
  sleep/wake cycles — every post-wake fetch returns `200`, frame pacing stays at
  60 fps, no crash.
- Main-thread-only paths (2D canvas, WebGL) were verified to survive sleep both
  before and after (they never touch the threadpool), confirming the fix is
  scoped to the real cause and changes nothing for them.

## Remaining work before opening

- **Open connections are not re-established by this fix.** `uv_loop_fork`
  re-registers existing handles with the new backend, but a TCP/TLS connection
  that was open across sleep is dead regardless (the network dropped). Those
  surface as normal connection errors on next use, which the JS side already
  handles; only the *self-pipe* needed active recreation. Worth a sentence in
  the eventual PR description.
- **Consider whether the runtime should also emit a JS-visible signal on
  resume** (e.g. an event) so apps can proactively re-fetch/reconnect. Out of
  scope for this fix, which is purely "don't crash."
- **Docs.** A short note under `docs/` on sleep/resume behavior once the shape
  is reviewed.

## Downstream context

Downstream embedders that do network on the Switch (e.g. a browser shell doing
catalogue/version/telemetry fetches) hit this on the very first request after a
user wakes the console mid-session — the highest-visibility manifestation. This
fix makes that path survive transparently.
