#include "memory.h"

static JSValue nx_memory_usage(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	JSRuntime *rt = JS_GetRuntime(ctx);
	JSMemoryUsage stats;
	JS_ComputeMemoryUsage(rt, &stats);

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "mallocSize",
					  JS_NewInt64(ctx, stats.malloc_size));
	JS_SetPropertyStr(ctx, obj, "mallocLimit",
					  JS_NewInt64(ctx, stats.malloc_limit));
	JS_SetPropertyStr(ctx, obj, "memoryUsedSize",
					  JS_NewInt64(ctx, stats.memory_used_size));
	JS_SetPropertyStr(ctx, obj, "mallocCount",
					  JS_NewInt64(ctx, stats.malloc_count));
	JS_SetPropertyStr(ctx, obj, "memoryUsedCount",
					  JS_NewInt64(ctx, stats.memory_used_count));
	JS_SetPropertyStr(ctx, obj, "atomCount",
					  JS_NewInt64(ctx, stats.atom_count));
	JS_SetPropertyStr(ctx, obj, "atomSize",
					  JS_NewInt64(ctx, stats.atom_size));
	JS_SetPropertyStr(ctx, obj, "strCount",
					  JS_NewInt64(ctx, stats.str_count));
	JS_SetPropertyStr(ctx, obj, "strSize",
					  JS_NewInt64(ctx, stats.str_size));
	JS_SetPropertyStr(ctx, obj, "objCount",
					  JS_NewInt64(ctx, stats.obj_count));
	JS_SetPropertyStr(ctx, obj, "objSize",
					  JS_NewInt64(ctx, stats.obj_size));
	JS_SetPropertyStr(ctx, obj, "propCount",
					  JS_NewInt64(ctx, stats.prop_count));
	JS_SetPropertyStr(ctx, obj, "propSize",
					  JS_NewInt64(ctx, stats.prop_size));
	JS_SetPropertyStr(ctx, obj, "shapeCount",
					  JS_NewInt64(ctx, stats.shape_count));
	JS_SetPropertyStr(ctx, obj, "shapeSize",
					  JS_NewInt64(ctx, stats.shape_size));
	JS_SetPropertyStr(ctx, obj, "jsFuncCount",
					  JS_NewInt64(ctx, stats.js_func_count));
	JS_SetPropertyStr(ctx, obj, "jsFuncSize",
					  JS_NewInt64(ctx, stats.js_func_size));
	JS_SetPropertyStr(ctx, obj, "jsFuncCodeSize",
					  JS_NewInt64(ctx, stats.js_func_code_size));
	JS_SetPropertyStr(ctx, obj, "jsFuncPc2lineCount",
					  JS_NewInt64(ctx, stats.js_func_pc2line_count));
	JS_SetPropertyStr(ctx, obj, "jsFuncPc2lineSize",
					  JS_NewInt64(ctx, stats.js_func_pc2line_size));
	JS_SetPropertyStr(ctx, obj, "cFuncCount",
					  JS_NewInt64(ctx, stats.c_func_count));
	JS_SetPropertyStr(ctx, obj, "arrayCount",
					  JS_NewInt64(ctx, stats.array_count));
	JS_SetPropertyStr(ctx, obj, "fastArrayCount",
					  JS_NewInt64(ctx, stats.fast_array_count));
	JS_SetPropertyStr(ctx, obj, "fastArrayElements",
					  JS_NewInt64(ctx, stats.fast_array_elements));
	JS_SetPropertyStr(ctx, obj, "binaryObjectCount",
					  JS_NewInt64(ctx, stats.binary_object_count));
	JS_SetPropertyStr(ctx, obj, "binaryObjectSize",
					  JS_NewInt64(ctx, stats.binary_object_size));
	return obj;
}

static JSValue nx_run_gc(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	JSRuntime *rt = JS_GetRuntime(ctx);
	JS_RunGC(rt);
	// QuickJS's `js_trigger_gc` sets the next auto-GC threshold to
	// `live_heap + (live_heap >> 1)` — fine on x86 where a GC pass on a
	// 7-10 MB heap is ~10ms, but on Switch hardware the same walk takes
	// 5+ seconds (cache patterns + slower memory). The default 50%
	// regrowth turns into a 3-5 MB garbage tolerance at warm steady
	// state, so when auto-GC eventually fires it has a huge mark phase
	// and the whole page freezes for 5s — visible to the user as the
	// rapier-physics demo locking up after ~30s of steady-state play
	// (data chain in [[reference-rapier-gc-pause-investigation]]).
	//
	// After a forced GC, cap the next-trigger threshold to live_heap +
	// 4 MB so subsequent auto-fires don't accidentally land inside an
	// app-level allocation burst (e.g. addBody creating a Mesh +
	// Material + 5-6 wbindgen calls + scene mutations would punch
	// through a 1 MB cap mid-call, triggering auto-GC inside the burst
	// — which we observed as a death spiral that re-froze the
	// rapier-physics demo when its JS-side heartbeat was 5s instead of
	// 1s). 4 MB is comfortably larger than any one realistic addBody
	// burst (~10-100 KB) and still small enough that auto-GC fires
	// promptly when garbage really accumulates. The JS-side
	// `setInterval(Switch.gc, ...)` heartbeat still flattens the
	// pause-time distribution; this cap just gives the heartbeat the
	// headroom to run at a humane cadence (5s) without spurious
	// in-burst auto-GCs.
	JSMemoryUsage stats;
	JS_ComputeMemoryUsage(rt, &stats);
	size_t capped = (size_t)stats.malloc_size + (4 * 1024 * 1024);
	JS_SetGCThreshold(rt, capped);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("memoryUsage", 0, nx_memory_usage),
	JS_CFUNC_DEF("gc", 0, nx_run_gc),
};

void nx_init_memory(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
