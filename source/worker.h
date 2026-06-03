#pragma once
#include "types.h"

/* Tier-0 Web Workers — real multi-threaded Worker on top of a fresh
 * JSRuntime + JSContext per worker, running on a dedicated pthread.
 * Surface limited to bidirectional `postMessage(string)`; no structured
 * clone, no importScripts, no fetch, no timers — see
 * [[project-swb-web-workers-milestone]] for the staged scope.
 *
 * Invariants (DO NOT BREAK):
 *   1. Each worker owns its own `JSRuntime`. NEVER share JSValues across
 *      workers/main — the heaps are independent.
 *   2. `nx_ctx_t` (the main-thread singleton) is touched ONLY by the
 *      main thread. Worker threads must not call audrv/video/canvas/GPU
 *      or any Switch.* service.
 *   3. Message queues are guarded by per-worker mutexes. The worker
 *      thread sleeps on `in_cond` when idle; main signals it on
 *      postMessage + terminate. The main thread drains the outbound
 *      queue from its own event-loop tick — never blocks.
 *   4. `terminate()` blocks until the worker thread has joined. No
 *      detached threads — leaks runtimes across navigation otherwise.
 */

/* Register the main-side `$` bindings used by `Worker` on the main JS
 * context: `workerSpawn`, `workerPostToWorker`, `workerTerminate`,
 * `workerSetDispatcher`. Called once at startup from main.c. */
void nx_init_worker(JSContext *ctx, JSValueConst init_obj);

/* Drain every active worker's outbound message queue and dispatch each
 * message into the main JS side. Called from the main event loop each
 * tick — cheap when no workers exist. */
void nx_process_workers(JSContext *ctx);

/* Terminate + free every active worker. Called on app shutdown. */
void nx_shutdown_workers(void);
