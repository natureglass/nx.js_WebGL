#include "worker.h"
#include "error.h"
#include <pthread.h>
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Tier-0 Web Workers — see worker.h for the contract + scope.
 * ============================================================ */

#define WORKER_HEAP_LIMIT_BYTES ((size_t)16 * 1024 * 1024) /* 16 MB cap per worker */
#define WORKER_INBOUND_MAX 1024 /* backpressure: postMessage refuses if queue exceeds this */
#define WORKER_IDLE_WAIT_MS 50 /* worker sleep slice when idle waiting for messages */
#define WORKER_LOG_PATH_FMT "sdmc:/switch/brewser/logs/worker-%d.log"

/* Message kind discriminator. Lets the worker→main channel carry
 * normal `postMessage` payloads (KIND_DATA) and out-of-band error
 * notifications (KIND_ERROR — exception text from worker dispatch).
 * Main-side dispatcher reads `kind` and routes to onmessage vs
 * onerror accordingly. */
typedef enum {
	NX_WORKER_MSG_DATA = 0,
	NX_WORKER_MSG_ERROR = 1,
} nx_worker_msg_kind_t;

typedef struct nx_worker_msg {
	char *data;
	size_t len;
	nx_worker_msg_kind_t kind;
	/* Pass F: side-channel for postMessage transferable ArrayBuffers.
	 * Each `transfers[i]` is a process-wide malloc owning the bytes of
	 * the sender's i-th transferred AB (one memcpy sender→msg). On the
	 * receiver side, the dispatch path calls JS_NewArrayBuffer with
	 * `ab_free_func` so receiver's runtime gains ownership and the
	 * malloc gets freed via GC. After successful hand-off, the receive
	 * path NULLs the slot so msg destruction doesn't double-free.
	 * If the worker terminates mid-flight, `free_msg` walks the list
	 * and free()s any still-owned buffers. */
	void **transfers;
	size_t *transfer_sizes;
	int n_transfers;
	struct nx_worker_msg *next;
} nx_worker_msg_t;

typedef struct nx_worker {
	pthread_t thread;
	bool joined;

	JSRuntime *rt;
	JSContext *ctx; /* worker's own context */

	/* main → worker queue */
	pthread_mutex_t in_lock;
	pthread_cond_t in_cond;
	nx_worker_msg_t *in_head;
	nx_worker_msg_t *in_tail;
	int in_count;

	/* worker → main queue */
	pthread_mutex_t out_lock;
	nx_worker_msg_t *out_head;
	nx_worker_msg_t *out_tail;

	volatile bool terminating;

	char *source; /* owned; freed in destroy */
	int handle;
	FILE *log_fd;

	struct nx_worker *next;
} nx_worker_t;

/* Global state — single linked list, single dispatcher. */
static nx_worker_t *worker_list = NULL;
static pthread_mutex_t worker_list_lock = PTHREAD_MUTEX_INITIALIZER;
static int next_worker_handle = 1;
static JSContext *g_main_ctx = NULL; /* captured at init time */
static JSValue g_main_dispatch = JS_UNDEFINED; /* JS function (handle, data) */

/* ============================================================
 * Message queue helpers — caller holds the relevant mutex.
 * ============================================================ */

static nx_worker_msg_t *make_msg(const char *str, size_t len,
								 nx_worker_msg_kind_t kind) {
	nx_worker_msg_t *m = malloc(sizeof(nx_worker_msg_t));
	if (!m) return NULL;
	m->data = malloc(len + 1);
	if (!m->data) {
		free(m);
		return NULL;
	}
	memcpy(m->data, str, len);
	m->data[len] = '\0';
	m->len = len;
	m->kind = kind;
	m->transfers = NULL;
	m->transfer_sizes = NULL;
	m->n_transfers = 0;
	m->next = NULL;
	return m;
}

/* Snapshot a JS array of ArrayBuffers into the msg's transfer table.
 * One memcpy per AB (sender's QuickJS heap → process-wide malloc), then
 * JS_DetachArrayBuffer on each source so the sender can't reuse the
 * bytes after the call returns (spec-mandated). Returns 0 on success,
 * non-zero on failure (msg's transfer state is left clean either way).
 * Caller already validated `transfer_arr` is a JS array. */
static int msg_attach_transfers(JSContext *ctx, nx_worker_msg_t *m,
								JSValueConst transfer_arr) {
	JSValue len_val = JS_GetPropertyStr(ctx, transfer_arr, "length");
	int32_t n = 0;
	if (JS_ToInt32(ctx, &n, len_val) < 0) { JS_FreeValue(ctx, len_val); return -1; }
	JS_FreeValue(ctx, len_val);
	if (n <= 0) return 0;
	m->transfers = calloc((size_t)n, sizeof(void *));
	m->transfer_sizes = calloc((size_t)n, sizeof(size_t));
	if (!m->transfers || !m->transfer_sizes) {
		free(m->transfers); free(m->transfer_sizes);
		m->transfers = NULL; m->transfer_sizes = NULL;
		return -1;
	}
	for (int i = 0; i < n; i++) {
		JSValue ab = JS_GetPropertyUint32(ctx, transfer_arr, i);
		size_t ab_len = 0;
		uint8_t *ab_data = JS_GetArrayBuffer(ctx, &ab_len, ab);
		if (!ab_data) {
			JS_FreeValue(ctx, ab);
			/* free the partials we already grabbed */
			for (int j = 0; j < i; j++) free(m->transfers[j]);
			free(m->transfers); free(m->transfer_sizes);
			m->transfers = NULL; m->transfer_sizes = NULL;
			return -1;
		}
		void *copy = ab_len > 0 ? malloc(ab_len) : malloc(1);
		if (!copy) {
			JS_FreeValue(ctx, ab);
			for (int j = 0; j < i; j++) free(m->transfers[j]);
			free(m->transfers); free(m->transfer_sizes);
			m->transfers = NULL; m->transfer_sizes = NULL;
			return -1;
		}
		if (ab_len > 0) memcpy(copy, ab_data, ab_len);
		m->transfers[i] = copy;
		m->transfer_sizes[i] = ab_len;
		JS_DetachArrayBuffer(ctx, ab);
		JS_FreeValue(ctx, ab);
	}
	m->n_transfers = n;
	return 0;
}

static void free_msg(nx_worker_msg_t *m) {
	if (!m) return;
	free(m->data);
	if (m->transfers) {
		for (int i = 0; i < m->n_transfers; i++) {
			if (m->transfers[i]) free(m->transfers[i]);
		}
		free(m->transfers);
		free(m->transfer_sizes);
	}
	free(m);
}

/* JS_NewArrayBuffer free callback — runs when the receiver-side
 * ArrayBuffer is GC'd, returning the process-wide malloc to the heap. */
static void ab_free_func(JSRuntime *rt, void *opaque, void *ptr) {
	(void)rt; (void)opaque;
	free(ptr);
}

/* Build a JS array of ArrayBuffers for the receiver, taking ownership
 * of msg->transfers[i] (NULL'd after). The new ABs use ab_free_func so
 * the bytes are freed via receiver's GC, not our queue cleanup.
 * Returns JS_UNDEFINED when n_transfers is 0 (no allocation). */
static JSValue msg_build_transfer_array(JSContext *ctx, nx_worker_msg_t *m) {
	if (m->n_transfers <= 0) return JS_UNDEFINED;
	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < m->n_transfers; i++) {
		void *buf = m->transfers[i];
		size_t sz = m->transfer_sizes[i];
		JSValue ab;
		if (buf) {
			ab = JS_NewArrayBuffer(ctx, (uint8_t *)buf, sz,
									ab_free_func, NULL, false);
			m->transfers[i] = NULL; /* ownership handed to AB */
		} else {
			/* Already taken or NULL — emit a 0-length AB so indices stay aligned */
			ab = JS_NewArrayBufferCopy(ctx, (const uint8_t *)"", 0);
		}
		JS_SetPropertyUint32(ctx, arr, i, ab);
	}
	return arr;
}

static void enqueue_locked(nx_worker_msg_t **head, nx_worker_msg_t **tail,
						   nx_worker_msg_t *msg) {
	msg->next = NULL;
	if (*tail) {
		(*tail)->next = msg;
	} else {
		*head = msg;
	}
	*tail = msg;
}

static nx_worker_msg_t *drain_all_locked(nx_worker_msg_t **head,
										 nx_worker_msg_t **tail) {
	nx_worker_msg_t *out = *head;
	*head = NULL;
	*tail = NULL;
	return out;
}

/* ============================================================
 * Worker context bootstrap — runs ON the worker thread.
 * ============================================================ */

/* `__postBytes(ArrayBuffer, transferABs?)` — worker-side outbound. The
 * worker's bootstrap installs `self.postMessage = (val, transfer) =>
 * __postBytes(...)` so user code calls postMessage(any, transfer?) and
 * we shovel the serialised bytes through. Pass A: bytes carried
 * opaquely. Pass F: optional `transferABs` JS array of ArrayBuffers
 * gets its bytes attached as a side-channel + detached on the worker
 * side. */
static JSValue worker_self_post(JSContext *ctx, JSValueConst this_val,
								int argc, JSValueConst *argv) {
	nx_worker_t *w = JS_GetContextOpaque(ctx);
	if (!w) return JS_ThrowInternalError(ctx, "no worker context");
	if (argc < 1) return JS_ThrowTypeError(ctx, "__postBytes requires ArrayBuffer");
	size_t len = 0;
	uint8_t *src = JS_GetArrayBuffer(ctx, &len, argv[0]);
	if (!src) return JS_ThrowTypeError(ctx, "__postBytes expects an ArrayBuffer");
	nx_worker_msg_t *m = make_msg((const char *)src, len, NX_WORKER_MSG_DATA);
	if (!m) return JS_ThrowOutOfMemory(ctx);
	if (argc >= 2 && JS_IsArray(argv[1])) {
		if (msg_attach_transfers(ctx, m, argv[1]) != 0) {
			free_msg(m);
			return JS_ThrowInternalError(ctx, "__postBytes: failed to attach transfers");
		}
	}
	pthread_mutex_lock(&w->out_lock);
	enqueue_locked(&w->out_head, &w->out_tail, m);
	pthread_mutex_unlock(&w->out_lock);
	return JS_UNDEFINED;
}

/* `console.log(...args)` — worker-side. Writes a single line to the
 * per-worker log file with all args stringified + space-joined. */
static JSValue worker_console_log(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_worker_t *w = JS_GetContextOpaque(ctx);
	if (!w || !w->log_fd) return JS_UNDEFINED;
	for (int i = 0; i < argc; i++) {
		const char *s = JS_ToCString(ctx, argv[i]);
		if (s) {
			if (i > 0) fputc(' ', w->log_fd);
			fputs(s, w->log_fd);
			JS_FreeCString(ctx, s);
		}
	}
	fputc('\n', w->log_fd);
	fflush(w->log_fd);
	return JS_UNDEFINED;
}

/* `close()` — worker calls this to signal self-termination. Sets the
 * terminating flag; the loop notices on its next iteration. */
static JSValue worker_close(JSContext *ctx, JSValueConst this_val,
							int argc, JSValueConst *argv) {
	nx_worker_t *w = JS_GetContextOpaque(ctx);
	if (w) w->terminating = true;
	return JS_UNDEFINED;
}

/* `__workerReadFile(path)` — worker-side sync file read for
 * `importScripts`. Tier-1 Pass C: restricted to sdmc:/ + romfs:/
 * (devkitPro libc has these mounted for direct fopen). http(s):// path
 * deferred to fetch-proxy (Pass E). Returns the file content as a JS
 * string (decoded as UTF-8 — caller's problem if the file is binary).
 * Throws on any I/O failure. */
static JSValue worker_read_file(JSContext *ctx, JSValueConst this_val,
								int argc, JSValueConst *argv) {
	if (argc < 1) return JS_ThrowTypeError(ctx, "__workerReadFile(path)");
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path) return JS_EXCEPTION;
	if (strncmp(path, "sdmc:/", 6) != 0 && strncmp(path, "romfs:/", 7) != 0) {
		JSValue err = JS_ThrowTypeError(ctx,
			"importScripts: only sdmc:/ and romfs:/ paths supported in Tier-1");
		JS_FreeCString(ctx, path);
		return err;
	}
	FILE *f = fopen(path, "rb");
	if (!f) {
		JSValue err = JS_ThrowReferenceError(ctx, "importScripts: cannot open %s", path);
		JS_FreeCString(ctx, path);
		return err;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		JS_FreeCString(ctx, path);
		return JS_ThrowInternalError(ctx, "importScripts: seek failed");
	}
	long size = ftell(f);
	if (size < 0) {
		fclose(f);
		JS_FreeCString(ctx, path);
		return JS_ThrowInternalError(ctx, "importScripts: ftell failed");
	}
	fseek(f, 0, SEEK_SET);
	char *buf = malloc((size_t)size + 1);
	if (!buf) {
		fclose(f);
		JS_FreeCString(ctx, path);
		return JS_ThrowOutOfMemory(ctx);
	}
	size_t got = fread(buf, 1, (size_t)size, f);
	fclose(f);
	buf[got] = '\0';
	JSValue result = JS_NewStringLen(ctx, buf, got);
	free(buf);
	JS_FreeCString(ctx, path);
	return result;
}

/* Install the C-level worker-side bindings that the JS bootstrap
 * builds on top of. The JS bootstrap (prepended to user source by
 * worker.ts) finds `__postBytes` here and wraps it into
 * `self.postMessage = val => __postBytes(_scSerialize(val))`, then
 * defines `__handleInbound(rawBuf)` for the C dispatcher to call.
 * `console.log/warn/error` writes to the per-worker log file; `close`
 * sets the terminating flag. */
static void worker_bootstrap_globals(JSContext *ctx) {
	JSValue global = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global, "__postBytes",
					  JS_NewCFunction(ctx, worker_self_post, "__postBytes", 1));
	JS_SetPropertyStr(ctx, global, "__workerReadFile",
					  JS_NewCFunction(ctx, worker_read_file, "__workerReadFile", 1));
	JS_SetPropertyStr(ctx, global, "close",
					  JS_NewCFunction(ctx, worker_close, "close", 0));
	JSValue console = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, console, "log",
					  JS_NewCFunction(ctx, worker_console_log, "log", 1));
	JS_SetPropertyStr(ctx, console, "error",
					  JS_NewCFunction(ctx, worker_console_log, "error", 1));
	JS_SetPropertyStr(ctx, console, "warn",
					  JS_NewCFunction(ctx, worker_console_log, "warn", 1));
	JS_SetPropertyStr(ctx, global, "console", console);
	JS_FreeValue(ctx, global);
}

/* Fire `globalThis.__handleInbound(ArrayBuffer, transferABs?)` if the
 * bootstrap registered one. The bootstrap layer deserialises the bytes
 * and fires the user's `self.onmessage({ data: value })`. Pass F:
 * transferred buffers attached to `msg` are wrapped into ArrayBuffers
 * owned by THIS context (receiver) — `msg->transfers[i]` is NULL'd as
 * ownership moves to JS, so queue cleanup won't double-free. Exceptions
 * from either side route to main via an ERROR message on the outbound
 * queue so `worker.onerror` fires. */
static void worker_dispatch_inbound(JSContext *ctx, nx_worker_msg_t *msg) {
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue handler = JS_GetPropertyStr(ctx, global, "__handleInbound");
	if (JS_IsFunction(ctx, handler)) {
		JSValue raw = JS_NewArrayBufferCopy(ctx, (const uint8_t *)msg->data, msg->len);
		JSValue transfer_arr = msg_build_transfer_array(ctx, msg);
		JSValueConst args[2] = {raw, transfer_arr};
		int n_args = JS_IsUndefined(transfer_arr) ? 1 : 2;
		JSValue ret = JS_Call(ctx, handler, global, n_args, args);
		JS_FreeValue(ctx, raw);
		if (!JS_IsUndefined(transfer_arr)) JS_FreeValue(ctx, transfer_arr);
		if (JS_IsException(ret)) {
			nx_worker_t *w = JS_GetContextOpaque(ctx);
			JSValue exc = JS_GetException(ctx);
			const char *s = JS_ToCString(ctx, exc);
			if (s && w) {
				if (w->log_fd) {
					fprintf(w->log_fd, "[worker] onmessage threw: %s\n", s);
					fflush(w->log_fd);
				}
				nx_worker_msg_t *errm = make_msg(s, strlen(s),
												  NX_WORKER_MSG_ERROR);
				if (errm) {
					pthread_mutex_lock(&w->out_lock);
					enqueue_locked(&w->out_head, &w->out_tail, errm);
					pthread_mutex_unlock(&w->out_lock);
				}
			}
			if (s) JS_FreeCString(ctx, s);
			JS_FreeValue(ctx, exc);
		}
		JS_FreeValue(ctx, ret);
	}
	JS_FreeValue(ctx, handler);
	JS_FreeValue(ctx, global);
}

/* Call the bootstrap-installed `globalThis.__runDueTimers()` which
 * fires any setTimeout/setInterval callbacks whose deadline has
 * passed, then returns the number of ms until the next-due timer
 * (or WORKER_IDLE_WAIT_MS if no timers are queued). The C event loop
 * uses the returned value as its cond_timedwait duration so workers
 * with active timers wake on time, while idle workers still sleep
 * the full 50 ms. */
static uint64_t worker_call_run_due_timers(JSContext *ctx, nx_worker_t *w) {
	uint64_t ms = WORKER_IDLE_WAIT_MS;
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue fn = JS_GetPropertyStr(ctx, global, "__runDueTimers");
	if (JS_IsFunction(ctx, fn)) {
		JSValue ret = JS_Call(ctx, fn, global, 0, NULL);
		if (JS_IsException(ret)) {
			JSValue exc = JS_GetException(ctx);
			if (w && w->log_fd) {
				const char *s = JS_ToCString(ctx, exc);
				if (s) {
					fprintf(w->log_fd, "[worker] __runDueTimers threw: %s\n", s);
					fflush(w->log_fd);
					JS_FreeCString(ctx, s);
				}
			}
			JS_FreeValue(ctx, exc);
		} else {
			int32_t r;
			if (JS_ToInt32(ctx, &r, ret) == 0 && r >= 0) {
				ms = (uint64_t)r;
				if (ms > WORKER_IDLE_WAIT_MS) ms = WORKER_IDLE_WAIT_MS;
			}
		}
		JS_FreeValue(ctx, ret);
	}
	JS_FreeValue(ctx, fn);
	JS_FreeValue(ctx, global);
	return ms;
}

/* Run all pending microtasks (Promise reactions) on the worker context.
 * Mirrors `nx_process_pending_jobs` from main.c. */
static void worker_drain_microtasks(JSContext *ctx, nx_worker_t *w) {
	JSContext *jctx = NULL;
	for (;;) {
		int rc = JS_ExecutePendingJob(JS_GetRuntime(ctx), &jctx);
		if (rc == 0) break;
		if (rc < 0 && w->log_fd) {
			JSValue exc = JS_GetException(jctx);
			const char *s = JS_ToCString(jctx, exc);
			if (s) {
				fprintf(w->log_fd, "[worker] microtask threw: %s\n", s);
				fflush(w->log_fd);
				JS_FreeCString(jctx, s);
			}
			JS_FreeValue(jctx, exc);
		}
	}
}

/* ============================================================
 * The worker thread entry point.
 * ============================================================ */

static void *worker_thread_main(void *arg) {
	nx_worker_t *w = (nx_worker_t *)arg;

	/* Per-worker log file. Best-effort: if fopen fails the worker still
	 * runs, console.log just becomes a no-op. */
	char log_path[128];
	snprintf(log_path, sizeof(log_path), WORKER_LOG_PATH_FMT, w->handle);
	w->log_fd = fopen(log_path, "w");
	if (w->log_fd) {
		fprintf(w->log_fd, "[worker %d] thread started\n", w->handle);
		fflush(w->log_fd);
	}

	/* QuickJS setup — runtime + context owned by this thread alone. */
	w->rt = JS_NewRuntime();
	if (!w->rt) goto cleanup;
	JS_SetMemoryLimit(w->rt, WORKER_HEAP_LIMIT_BYTES);
	w->ctx = JS_NewContext(w->rt);
	if (!w->ctx) goto cleanup;
	JS_SetContextOpaque(w->ctx, w);

	worker_bootstrap_globals(w->ctx);

	/* Evaluate the user source. Failure logged; loop still runs so the
	 * worker can report the error via postMessage if it wishes — but in
	 * Tier-0 the worker source must succeed for `onmessage` to be set
	 * at all, so a parse error effectively makes the worker inert. */
	JSValue eval_ret = JS_Eval(w->ctx, w->source, strlen(w->source),
								"worker.js", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(eval_ret) && w->log_fd) {
		JSValue exc = JS_GetException(w->ctx);
		const char *s = JS_ToCString(w->ctx, exc);
		if (s) {
			fprintf(w->log_fd, "[worker %d] eval threw: %s\n", w->handle, s);
			fflush(w->log_fd);
			JS_FreeCString(w->ctx, s);
		}
		JS_FreeValue(w->ctx, exc);
	}
	JS_FreeValue(w->ctx, eval_ret);

	/* Drain any microtasks the initial eval queued. */
	worker_drain_microtasks(w->ctx, w);

	/* Main worker loop. Each iteration:
	 *   1. Drain any inbound message batch + fire onmessage + microtasks
	 *   2. Run any setTimeout/setInterval callbacks whose deadline passed,
	 *      get back the ms-until-next-deadline (capped at WORKER_IDLE_WAIT_MS)
	 *   3. Sleep on the cond var until next message, terminate, or that
	 *      deadline — whichever comes first */
	while (!w->terminating) {
		pthread_mutex_lock(&w->in_lock);
		nx_worker_msg_t *batch = drain_all_locked(&w->in_head, &w->in_tail);
		w->in_count = 0;
		pthread_mutex_unlock(&w->in_lock);

		while (batch) {
			nx_worker_msg_t *next = batch->next;
			worker_dispatch_inbound(w->ctx, batch);
			worker_drain_microtasks(w->ctx, w);
			free_msg(batch);
			batch = next;
			if (w->terminating) {
				while (batch) {
					nx_worker_msg_t *n2 = batch->next;
					free_msg(batch);
					batch = n2;
				}
				break;
			}
		}
		if (w->terminating) break;

		uint64_t ms_to_next = worker_call_run_due_timers(w->ctx, w);
		worker_drain_microtasks(w->ctx, w);
		if (w->terminating) break;

		pthread_mutex_lock(&w->in_lock);
		if (!w->in_head && !w->terminating && ms_to_next > 0) {
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += (long)ms_to_next * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec += ts.tv_nsec / 1000000000L;
				ts.tv_nsec %= 1000000000L;
			}
			pthread_cond_timedwait(&w->in_cond, &w->in_lock, &ts);
		}
		pthread_mutex_unlock(&w->in_lock);
	}

	if (w->log_fd) {
		fprintf(w->log_fd, "[worker %d] thread exiting cleanly\n", w->handle);
		fflush(w->log_fd);
	}

cleanup:
	if (w->ctx) {
		JS_FreeContext(w->ctx);
		w->ctx = NULL;
	}
	if (w->rt) {
		JS_FreeRuntime(w->rt);
		w->rt = NULL;
	}
	if (w->log_fd) {
		fclose(w->log_fd);
		w->log_fd = NULL;
	}
	return NULL;
}

/* ============================================================
 * Main-thread API — find / list-management / lifecycle.
 * ============================================================ */

static nx_worker_t *find_worker_locked(int handle) {
	nx_worker_t *w = worker_list;
	while (w) {
		if (w->handle == handle) return w;
		w = w->next;
	}
	return NULL;
}

static void remove_from_list_locked(nx_worker_t *target) {
	nx_worker_t **pp = &worker_list;
	while (*pp) {
		if (*pp == target) {
			*pp = target->next;
			target->next = NULL;
			return;
		}
		pp = &(*pp)->next;
	}
}

static void destroy_worker(nx_worker_t *w) {
	/* Caller must have already joined the thread. */
	pthread_mutex_lock(&w->in_lock);
	nx_worker_msg_t *m = drain_all_locked(&w->in_head, &w->in_tail);
	pthread_mutex_unlock(&w->in_lock);
	while (m) { nx_worker_msg_t *n = m->next; free_msg(m); m = n; }
	pthread_mutex_lock(&w->out_lock);
	m = drain_all_locked(&w->out_head, &w->out_tail);
	pthread_mutex_unlock(&w->out_lock);
	while (m) { nx_worker_msg_t *n = m->next; free_msg(m); m = n; }
	pthread_mutex_destroy(&w->in_lock);
	pthread_cond_destroy(&w->in_cond);
	pthread_mutex_destroy(&w->out_lock);
	free(w->source);
	free(w);
}

/* `$.workerSpawn(sourceString)` → returns int handle, or throws. */
static JSValue js_worker_spawn(JSContext *ctx, JSValueConst this_val,
							   int argc, JSValueConst *argv) {
	if (argc < 1) return JS_ThrowTypeError(ctx, "workerSpawn requires source");
	size_t src_len = 0;
	const char *src = JS_ToCStringLen(ctx, &src_len, argv[0]);
	if (!src) return JS_EXCEPTION;

	nx_worker_t *w = calloc(1, sizeof(nx_worker_t));
	if (!w) {
		JS_FreeCString(ctx, src);
		return JS_ThrowOutOfMemory(ctx);
	}
	w->source = malloc(src_len + 1);
	if (!w->source) {
		free(w);
		JS_FreeCString(ctx, src);
		return JS_ThrowOutOfMemory(ctx);
	}
	memcpy(w->source, src, src_len);
	w->source[src_len] = '\0';
	JS_FreeCString(ctx, src);

	pthread_mutex_init(&w->in_lock, NULL);
	pthread_cond_init(&w->in_cond, NULL);
	pthread_mutex_init(&w->out_lock, NULL);

	pthread_mutex_lock(&worker_list_lock);
	w->handle = next_worker_handle++;
	w->next = worker_list;
	worker_list = w;
	pthread_mutex_unlock(&worker_list_lock);

	int rc = pthread_create(&w->thread, NULL, worker_thread_main, w);
	if (rc != 0) {
		pthread_mutex_lock(&worker_list_lock);
		remove_from_list_locked(w);
		pthread_mutex_unlock(&worker_list_lock);
		destroy_worker(w);
		return JS_ThrowInternalError(ctx, "pthread_create failed: %d", rc);
	}
	return JS_NewInt32(ctx, w->handle);
}

/* `$.workerPostToWorker(handle, ArrayBuffer, transferABs?)` → queues a
 * serialised structured-clone payload for the worker. The bytes are
 * opaque to the C layer; the worker bootstrap's `__handleInbound`
 * deserialises. Pass F: optional `transferABs` array carries main-side
 * ArrayBuffers whose bytes are attached as a side-channel + detached
 * on the main side. */
static JSValue js_worker_post(JSContext *ctx, JSValueConst this_val,
							  int argc, JSValueConst *argv) {
	int handle;
	if (argc < 2) return JS_ThrowTypeError(ctx, "workerPostToWorker(handle, ArrayBuffer)");
	if (JS_ToInt32(ctx, &handle, argv[0])) return JS_EXCEPTION;
	size_t len = 0;
	uint8_t *src = JS_GetArrayBuffer(ctx, &len, argv[1]);
	if (!src) return JS_ThrowTypeError(ctx, "workerPostToWorker expects ArrayBuffer");

	pthread_mutex_lock(&worker_list_lock);
	nx_worker_t *w = find_worker_locked(handle);
	pthread_mutex_unlock(&worker_list_lock);
	if (!w) return JS_FALSE; /* silently drop — worker already gone */
	nx_worker_msg_t *m = make_msg((const char *)src, len, NX_WORKER_MSG_DATA);
	if (!m) return JS_ThrowOutOfMemory(ctx);
	if (argc >= 3 && JS_IsArray(argv[2])) {
		if (msg_attach_transfers(ctx, m, argv[2]) != 0) {
			free_msg(m);
			return JS_ThrowInternalError(ctx, "workerPostToWorker: failed to attach transfers");
		}
	}
	pthread_mutex_lock(&w->in_lock);
	if (w->in_count >= WORKER_INBOUND_MAX) {
		pthread_mutex_unlock(&w->in_lock);
		free_msg(m);
		return JS_ThrowInternalError(ctx, "worker inbound queue full");
	}
	enqueue_locked(&w->in_head, &w->in_tail, m);
	w->in_count++;
	pthread_cond_signal(&w->in_cond);
	pthread_mutex_unlock(&w->in_lock);
	return JS_TRUE;
}

/* `$.workerTerminate(handle)` → blocks until the worker thread joins,
 * then frees. Subsequent ops on the handle silently no-op. */
static JSValue js_worker_terminate(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	int handle;
	if (argc < 1) return JS_ThrowTypeError(ctx, "workerTerminate(handle)");
	if (JS_ToInt32(ctx, &handle, argv[0])) return JS_EXCEPTION;

	pthread_mutex_lock(&worker_list_lock);
	nx_worker_t *w = find_worker_locked(handle);
	if (w) remove_from_list_locked(w);
	pthread_mutex_unlock(&worker_list_lock);
	if (!w) return JS_FALSE;

	/* Signal the worker to exit + wake it from cond_wait. */
	pthread_mutex_lock(&w->in_lock);
	w->terminating = true;
	pthread_cond_signal(&w->in_cond);
	pthread_mutex_unlock(&w->in_lock);

	pthread_join(w->thread, NULL);
	w->joined = true;
	destroy_worker(w);
	return JS_TRUE;
}

/* `$.workerSetDispatcher(fn)` — installs the JS function that
 * `nx_process_workers` calls per drained message. Signature:
 * `fn(handle: number, value: ArrayBuffer|string, kind: number,
 *     transferABs?: ArrayBuffer[]): void`. Called once at runtime
 * init from worker.ts. */
static JSValue js_worker_set_dispatcher(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
		return JS_ThrowTypeError(ctx, "workerSetDispatcher requires function");
	}
	if (!JS_IsUndefined(g_main_dispatch)) JS_FreeValue(ctx, g_main_dispatch);
	g_main_dispatch = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

/* ============================================================
 * Main event-loop drain.
 * ============================================================ */

void nx_process_workers(JSContext *ctx) {
	if (JS_IsUndefined(g_main_dispatch)) return;
	/* Walk the list under the list lock to find workers with pending
	 * outbound messages, but drain each worker's queue with only its
	 * own out_lock held (so we don't block other postMessage calls).
	 * Snapshot the worker pointers first; the worker_list_lock is
	 * short. */
	pthread_mutex_lock(&worker_list_lock);
	int count = 0;
	for (nx_worker_t *w = worker_list; w; w = w->next) count++;
	if (count == 0) {
		pthread_mutex_unlock(&worker_list_lock);
		return;
	}
	nx_worker_t **snapshot = malloc(sizeof(nx_worker_t *) * count);
	if (!snapshot) {
		pthread_mutex_unlock(&worker_list_lock);
		return;
	}
	int i = 0;
	for (nx_worker_t *w = worker_list; w && i < count; w = w->next) {
		snapshot[i++] = w;
	}
	pthread_mutex_unlock(&worker_list_lock);

	for (int j = 0; j < count; j++) {
		nx_worker_t *w = snapshot[j];
		pthread_mutex_lock(&w->out_lock);
		nx_worker_msg_t *batch = drain_all_locked(&w->out_head, &w->out_tail);
		pthread_mutex_unlock(&w->out_lock);
		while (batch) {
			nx_worker_msg_t *next = batch->next;
			JSValue val;
			JSValue transfer_arr = JS_UNDEFINED;
			if (batch->kind == NX_WORKER_MSG_DATA) {
				/* Carry the structured-clone bytes into JS as an
				 * ArrayBuffer; worker.ts deserialises before onmessage.
				 * Pass F: side-channel transfers move ownership of
				 * msg->transfers[i] into receiver-owned ABs. */
				val = JS_NewArrayBufferCopy(ctx, (const uint8_t *)batch->data, batch->len);
				transfer_arr = msg_build_transfer_array(ctx, batch);
			} else {
				/* ERROR payload is the exception's toString(); stays
				 * as a JS string for `worker.onerror({ message })`. */
				val = JS_NewStringLen(ctx, batch->data, batch->len);
			}
			JSValueConst args[4] = {
				JS_NewInt32(ctx, w->handle),
				val,
				JS_NewInt32(ctx, (int)batch->kind),
				transfer_arr,
			};
			JSValue ret = JS_Call(ctx, g_main_dispatch, JS_UNDEFINED, 4, args);
			JS_FreeValue(ctx, (JSValue)args[0]);
			JS_FreeValue(ctx, (JSValue)args[1]);
			JS_FreeValue(ctx, (JSValue)args[2]);
			if (!JS_IsUndefined(transfer_arr)) JS_FreeValue(ctx, transfer_arr);
			if (JS_IsException(ret)) nx_emit_error_event(ctx);
			JS_FreeValue(ctx, ret);
			free_msg(batch);
			batch = next;
		}
	}
	free(snapshot);
}

void nx_shutdown_workers(void) {
	pthread_mutex_lock(&worker_list_lock);
	nx_worker_t *w = worker_list;
	worker_list = NULL;
	pthread_mutex_unlock(&worker_list_lock);
	while (w) {
		nx_worker_t *next = w->next;
		pthread_mutex_lock(&w->in_lock);
		w->terminating = true;
		pthread_cond_signal(&w->in_cond);
		pthread_mutex_unlock(&w->in_lock);
		pthread_join(w->thread, NULL);
		destroy_worker(w);
		w = next;
	}
}

/* ============================================================
 * Init — registers main-side bindings on the `$` init object.
 * ============================================================ */

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("workerSpawn", 1, js_worker_spawn),
	JS_CFUNC_DEF("workerPostToWorker", 2, js_worker_post),
	JS_CFUNC_DEF("workerTerminate", 1, js_worker_terminate),
	JS_CFUNC_DEF("workerSetDispatcher", 1, js_worker_set_dispatcher),
};

void nx_init_worker(JSContext *ctx, JSValueConst init_obj) {
	g_main_ctx = ctx;
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
