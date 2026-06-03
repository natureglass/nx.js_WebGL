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
	m->next = NULL;
	return m;
}

static void free_msg(nx_worker_msg_t *m) {
	if (!m) return;
	free(m->data);
	free(m);
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

/* `self.postMessage(str)` — worker-side. */
static JSValue worker_self_post(JSContext *ctx, JSValueConst this_val,
								int argc, JSValueConst *argv) {
	nx_worker_t *w = JS_GetContextOpaque(ctx);
	if (!w) return JS_ThrowInternalError(ctx, "no worker context");
	if (argc < 1) return JS_ThrowTypeError(ctx, "postMessage requires 1 argument");
	size_t len = 0;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);
	if (!str) return JS_EXCEPTION;
	nx_worker_msg_t *m = make_msg(str, len, NX_WORKER_MSG_DATA);
	JS_FreeCString(ctx, str);
	if (!m) return JS_ThrowOutOfMemory(ctx);
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

/* Install `self`, `globalThis.postMessage`, `globalThis.close`,
 * `globalThis.console.log`, and an empty `globalThis.onmessage`
 * property. Called once before the worker user code is evaluated. */
static void worker_bootstrap_globals(JSContext *ctx) {
	JSValue global = JS_GetGlobalObject(ctx);
	/* postMessage */
	JS_SetPropertyStr(ctx, global, "postMessage",
					  JS_NewCFunction(ctx, worker_self_post, "postMessage", 1));
	/* close */
	JS_SetPropertyStr(ctx, global, "close",
					  JS_NewCFunction(ctx, worker_close, "close", 0));
	/* self === globalThis */
	JS_SetPropertyStr(ctx, global, "self", JS_DupValue(ctx, global));
	/* console.log */
	JSValue console = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, console, "log",
					  JS_NewCFunction(ctx, worker_console_log, "log", 1));
	JS_SetPropertyStr(ctx, console, "error",
					  JS_NewCFunction(ctx, worker_console_log, "error", 1));
	JS_SetPropertyStr(ctx, console, "warn",
					  JS_NewCFunction(ctx, worker_console_log, "warn", 1));
	JS_SetPropertyStr(ctx, global, "console", console);
	/* onmessage placeholder */
	JS_SetPropertyStr(ctx, global, "onmessage", JS_NULL);
	JS_FreeValue(ctx, global);
}

/* Fire `globalThis.onmessage({ data: str })` if the user installed a
 * handler. Synthesizes a tiny event-like object — no full Event class
 * needed for Tier-0. Exceptions logged to worker log; loop continues. */
static void worker_dispatch_inbound(JSContext *ctx, const char *data,
									size_t len) {
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue handler = JS_GetPropertyStr(ctx, global, "onmessage");
	if (JS_IsFunction(ctx, handler)) {
		JSValue evt = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, evt, "data", JS_NewStringLen(ctx, data, len));
		JSValueConst args[1] = {evt};
		JSValue ret = JS_Call(ctx, handler, global, 1, args);
		if (JS_IsException(ret)) {
			nx_worker_t *w = JS_GetContextOpaque(ctx);
			JSValue exc = JS_GetException(ctx);
			const char *s = JS_ToCString(ctx, exc);
			if (s && w) {
				if (w->log_fd) {
					fprintf(w->log_fd, "[worker] onmessage threw: %s\n", s);
					fflush(w->log_fd);
				}
				/* Surface to the main thread as an error event. Routed via
				 * the outbound queue with kind=ERROR; main dispatcher fires
				 * `worker.onerror({ message: s })`. */
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
		JS_FreeValue(ctx, evt);
	}
	JS_FreeValue(ctx, handler);
	JS_FreeValue(ctx, global);
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

	/* Main worker loop. */
	while (!w->terminating) {
		/* Drain inbound. */
		pthread_mutex_lock(&w->in_lock);
		if (!w->in_head && !w->terminating) {
			/* Wait for either a message or terminate signal. Bounded
			 * wait so terminate doesn't depend on incoming traffic. */
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += (long)WORKER_IDLE_WAIT_MS * 1000000L;
			if (ts.tv_nsec >= 1000000000L) {
				ts.tv_sec += ts.tv_nsec / 1000000000L;
				ts.tv_nsec %= 1000000000L;
			}
			pthread_cond_timedwait(&w->in_cond, &w->in_lock, &ts);
		}
		nx_worker_msg_t *batch = drain_all_locked(&w->in_head, &w->in_tail);
		w->in_count = 0;
		pthread_mutex_unlock(&w->in_lock);

		while (batch) {
			nx_worker_msg_t *next = batch->next;
			worker_dispatch_inbound(w->ctx, batch->data, batch->len);
			worker_drain_microtasks(w->ctx, w);
			free_msg(batch);
			batch = next;
			if (w->terminating) {
				/* Drop any remaining queued messages on terminate. */
				while (batch) {
					nx_worker_msg_t *n2 = batch->next;
					free_msg(batch);
					batch = n2;
				}
				break;
			}
		}
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

/* `$.workerPostToWorker(handle, str)` → queues a message for the worker. */
static JSValue js_worker_post(JSContext *ctx, JSValueConst this_val,
							  int argc, JSValueConst *argv) {
	int handle;
	if (argc < 2) return JS_ThrowTypeError(ctx, "workerPostToWorker(handle, str)");
	if (JS_ToInt32(ctx, &handle, argv[0])) return JS_EXCEPTION;
	size_t len = 0;
	const char *str = JS_ToCStringLen(ctx, &len, argv[1]);
	if (!str) return JS_EXCEPTION;

	pthread_mutex_lock(&worker_list_lock);
	nx_worker_t *w = find_worker_locked(handle);
	pthread_mutex_unlock(&worker_list_lock);
	if (!w) {
		JS_FreeCString(ctx, str);
		return JS_FALSE; /* silently drop — worker already gone */
	}
	nx_worker_msg_t *m = make_msg(str, len, NX_WORKER_MSG_DATA);
	JS_FreeCString(ctx, str);
	if (!m) return JS_ThrowOutOfMemory(ctx);
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
 * `fn(handle: number, data: string): void`. Called once at runtime
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
			JSValueConst args[3] = {
				JS_NewInt32(ctx, w->handle),
				JS_NewStringLen(ctx, batch->data, batch->len),
				JS_NewInt32(ctx, (int)batch->kind),
			};
			JSValue ret = JS_Call(ctx, g_main_dispatch, JS_UNDEFINED, 3, args);
			JS_FreeValue(ctx, (JSValue)args[0]);
			JS_FreeValue(ctx, (JSValue)args[1]);
			JS_FreeValue(ctx, (JSValue)args[2]);
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
