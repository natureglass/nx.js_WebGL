/**
 * Modified from `txiki.js` by Saúl Ibarra Corretgé <s@saghul.net>
 *  - https://github.com/saghul/txiki.js/blob/master/src/wasm.c
 *  - https://github.com/saghul/txiki.js/blob/master/src/js/polyfills/wasm.js
 */
#include "wasm.h"
#include "types.h"
#include <m3_env.h>

static M3Result nx_wasm_js_error = "JS error was thrown";

// https://webassembly.github.io/spec/js-api/index.html#towebassemblyvalue
static int nx__wasm_towebassemblyvalue(JSContext *ctx, JSValueConst val,
									   M3ValueType type, void *stack) {
	int r = 0;
	switch (type) {
	case c_m3Type_i32: {
		r = JS_ToInt32(ctx, (int32_t *)stack, val);
		break;
	};
	case c_m3Type_i64: {
		r = JS_ToInt64(ctx, (int64_t *)stack, val);
		break;
	};
	case c_m3Type_f32:
	case c_m3Type_f64: {
		r = JS_ToFloat64(ctx, (double *)stack, val);
		break;
	};
	case c_m3Type_none:
	case c_m3Type_unknown: {
		/* shrug */
		break;
	}
	}
	return r;
}

// https://webassembly.github.io/spec/js-api/index.html#tojsvalue
static JSValue nx__wasm_tojsvalue(JSContext *ctx, M3ValueType type,
								  const void *stack) {
	switch (type) {
	case c_m3Type_i32: {
		int32_t val = *(int32_t *)stack;
		return JS_NewInt32(ctx, val);
	}
	case c_m3Type_i64: {
		int64_t val = *(int64_t *)stack;
		if (val == (int32_t)val)
			return JS_NewInt32(ctx, (int32_t)val);
		else
			return JS_NewBigInt64(ctx, val);
	}
	case c_m3Type_f32: {
		float val = *(float *)stack;
		return JS_NewFloat64(ctx, (double)val);
	}
	case c_m3Type_f64: {
		double val = *(double *)stack;
		return JS_NewFloat64(ctx, val);
	}
	default:
		return JS_UNDEFINED;
	}
}

JSValue nx_throw_wasm_error(JSContext *ctx, const char *name, M3Result r) {
	JSValue obj = JS_NewError(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "message", JS_NewString(ctx, r),
							  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_DefinePropertyValueStr(ctx, obj, "wasmError", JS_NewString(ctx, name),
							  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	if (JS_IsException(obj))
		obj = JS_NULL;
	return JS_Throw(ctx, obj);
}

static JSClassID nx_wasm_memory_class_id;

typedef struct {
	IM3Memory mem;
	bool needs_free;
	int is_shared;
	JSValue cached_buffer;    // Cached ArrayBuffer for .buffer getter
	size_t cached_buffer_len; // Length when cached (invalidate on grow)
	void *cached_buffer_ptr;  // Underlying memory pointer at cache time
} nx_wasm_memory_t;

static nx_wasm_memory_t *nx_wasm_memory_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_wasm_memory_class_id);
}

static void finalizer_wasm_memory(JSRuntime *rt, JSValue val) {
	nx_wasm_memory_t *data = JS_GetOpaque(val, nx_wasm_memory_class_id);
	if (data) {
		if (!JS_IsUndefined(data->cached_buffer)) {
			JS_FreeValueRT(rt, data->cached_buffer);
		}
		if (data->needs_free && data->mem) {
			if (data->mem->mallocated) {
				m3_Free(data->mem->mallocated);
			}
			js_free_rt(rt, data->mem);
		}
		js_free_rt(rt, data);
	}
}

static JSValue nx_wasm_memory_new_(JSContext *ctx) {
	JSValue obj = JS_NewObjectClass(ctx, nx_wasm_memory_class_id);
	nx_wasm_memory_t *data = js_mallocz(ctx, sizeof(nx_wasm_memory_t));
	if (!data) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	data->cached_buffer = JS_UNDEFINED;
	data->cached_buffer_len = 0;
	JS_SetOpaque(obj, data);
	return obj;
}

static JSClassID nx_wasm_table_class_id;

/* `owned` distinguishes user-constructed tables (where the wrapper
 * holds its own malloc'd backing storage that the finalizer must
 * release) from module-exported tables (where `table` + `table_size`
 * point into the wasm3 module's own memory and the finalizer must
 * leave them alone). itch.io compat: Godot 4 + many Emscripten
 * builds construct a Table from JS to pass as a WASM import, so
 * the user-constructor path matters. */
typedef struct {
	IM3Function *table;
	u32 *table_size;
	bool owned;
} nx_wasm_table_t;

static nx_wasm_table_t *nx_wasm_table_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_wasm_table_class_id);
}

static void finalizer_wasm_table(JSRuntime *rt, JSValue val) {
	nx_wasm_table_t *data = JS_GetOpaque(val, nx_wasm_table_class_id);
	if (data) {
		if (data->owned) {
			if (data->table) js_free_rt(rt, data->table);
			if (data->table_size) js_free_rt(rt, data->table_size);
		}
		js_free_rt(rt, data);
	}
}

static JSValue nx_wasm_table_new_(JSContext *ctx) {
	JSValue obj = JS_NewObjectClass(ctx, nx_wasm_table_class_id);
	nx_wasm_table_t *data = js_mallocz(ctx, sizeof(nx_wasm_table_t));
	if (!data) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);
	return obj;
}

static JSClassID nx_wasm_exported_func_class_id;

typedef struct {
	IM3Function function;
	// Duplicated JSValue reference to the Instance JS object so that the
	// Instance (and therefore its underlying runtime/module/functions[]) cannot
	// be GC'd while any export wrapper is still reachable. Without this, the
	// Instance can be collected while JS code still holds a bound export
	// function, leaving `function` as a dangling pointer into freed memory.
	// Initialized to JS_UNDEFINED for the Table.get path that doesn't carry an
	// instance reference; in that case the table's lifecycle is managed
	// separately by the Table JS object.
	JSValue instance;
} nx_wasm_exported_func_t;

static nx_wasm_exported_func_t *nx_wasm_exported_func_get(JSContext *ctx,
														  JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_wasm_exported_func_class_id);
}

static void finalizer_wasm_exported_func(JSRuntime *rt, JSValue val) {
	nx_wasm_exported_func_t *data =
		JS_GetOpaque(val, nx_wasm_exported_func_class_id);
	if (data) {
		JS_FreeValueRT(rt, data->instance);
		js_free_rt(rt, data);
	}
}

static JSValue nx_wasm_exported_func_new(JSContext *ctx, IM3Function func,
										 JSValueConst instance) {
	JSValue obj = JS_NewObjectClass(ctx, nx_wasm_exported_func_class_id);
	nx_wasm_exported_func_t *data =
		js_mallocz(ctx, sizeof(nx_wasm_exported_func_t));
	if (!data) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	data->function = func;
	data->instance = JS_DupValue(ctx, instance);
	JS_SetOpaque(obj, data);
	return obj;
}

static JSClassID nx_wasm_module_class_id;

typedef struct {
	IM3Module module;
	uint8_t *data;
	size_t size;
} nx_wasm_module_t;

static nx_wasm_module_t *nx_wasm_module_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_wasm_module_class_id);
}

static void finalizer_wasm_module(JSRuntime *rt, JSValue val) {
	nx_wasm_module_t *m = JS_GetOpaque(val, nx_wasm_module_class_id);
	if (m) {
		if (m->module)
			m3_FreeModule(m->module);
		// m->data is now an owned copy (see nx_wasm_new_module). Free it AFTER
		// m3_FreeModule since the M3Module's wasmStart/etc. pointers reference it.
		if (m->data)
			js_free_rt(rt, m->data);
		js_free_rt(rt, m);
	}
}

static JSClassID nx_wasm_global_class_id;

typedef struct {
	IM3Global global;
} nx_wasm_global_t;

static nx_wasm_global_t *nx_wasm_global_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_wasm_global_class_id);
}

static void finalizer_wasm_global(JSRuntime *rt, JSValue val) {
	nx_wasm_global_t *g = JS_GetOpaque(val, nx_wasm_global_class_id);
	if (g) {
		// Don't need to free `global` since the Runtime instance owns it
		g->global = NULL;
		js_free_rt(rt, g);
	}
}

static JSValue nx_wasm_new_global(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObjectClass(ctx, nx_wasm_global_class_id);
	nx_wasm_global_t *g = js_mallocz(ctx, sizeof(nx_wasm_global_t));
	if (!g) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}

	JS_SetOpaque(obj, g);

	// Gets defined during import / export instantiation
	g->global = NULL;

	return obj;
}

static JSValue nx_wasm_global_value_get(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_wasm_global_t *g = nx_wasm_global_get(ctx, argv[0]);
	if (!g)
		return JS_EXCEPTION;

	IM3Global global = g->global;
	if (!global) {
		// Not bound
		return JS_ThrowTypeError(ctx, "Global not defined");
	}

	M3TaggedValue val;
	M3Result r = m3_GetGlobal(global, &val);
	if (r) {
		return nx_throw_wasm_error(ctx, "LinkError", r);
	}

	return nx__wasm_tojsvalue(ctx, val.type, &val.value);
}

static JSValue nx_wasm_global_value_set(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_wasm_global_t *g = nx_wasm_global_get(ctx, argv[0]);
	if (!g)
		return JS_EXCEPTION;

	IM3Global global = g->global;
	if (!global) {
		// Not bound
		return JS_ThrowTypeError(ctx, "Global not defined");
	}

	if (nx__wasm_towebassemblyvalue(ctx, argv[1], global->type,
									&global->i32Value))
		return JS_EXCEPTION;

	return JS_UNDEFINED;
}

typedef struct {
	JSContext *ctx;
	JSValue func;
} nx_wasm_imported_func_t;

static JSClassID nx_wasm_instance_class_id;

typedef struct {
	IM3Runtime runtime;
	IM3Module module;
	bool loaded;
	nx_wasm_imported_func_t **imported_funcs;
	size_t num_imported_funcs;
	// v15 lifecycle fix: hold a strong JS ref to the parent Module JS value so
	// the Module (and therefore its owned m->data byte buffer) cannot be GC'd
	// while this Instance is still alive. Without this, JS code that drops the
	// Module reference (e.g. `const { instance } = WebAssembly.instantiate(...)`)
	// lets the Module finalizer run, frees m->data, and dangles every wasm
	// pointer in instance->module's functions[] — next lazy compile reads what
	// is now allocator free-list metadata.
	JSValue module_value;
} nx_wasm_instance_t;

// static nx_wasm_instance_t *nx_wasm_instance_get(JSContext *ctx, JSValueConst
// obj)
//{
//     return JS_GetOpaque2(ctx, obj, nx_wasm_instance_class_id);
// }

static void finalizer_wasm_instance(JSRuntime *rt, JSValue val) {
	nx_wasm_instance_t *i = JS_GetOpaque(val, nx_wasm_instance_class_id);
	if (i) {
		// Free JS references held by imported functions before the
		// runtime/module is torn down.
		if (i->imported_funcs) {
			for (size_t j = 0; j < i->num_imported_funcs; j++) {
				nx_wasm_imported_func_t *imported = i->imported_funcs[j];
				if (imported) {
					JS_FreeValueRT(rt, imported->func);
					js_free_rt(rt, imported);
				}
			}
			js_free_rt(rt, i->imported_funcs);
		}
		if (i->module) {
			// Free the module, only if it wasn't previously loaded.
			if (!i->loaded)
				m3_FreeModule(i->module);
		}
		if (i->runtime)
			m3_FreeRuntime(i->runtime);
		// Release the strong ref to the parent Module JS value (v15). Safe to
		// call on JS_UNDEFINED (the init value when construction failed early).
		JS_FreeValueRT(rt, i->module_value);
		js_free_rt(rt, i);
	}
}

static JSValue nx_wasm_new_module(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	if (nx_ctx->wasm_env == NULL) {
		nx_ctx->wasm_env = m3_NewEnvironment();
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_wasm_module_class_id);
	nx_wasm_module_t *m = js_mallocz(ctx, sizeof(nx_wasm_module_t));
	if (!m) {
		return JS_EXCEPTION;
	}

	JS_SetOpaque(obj, m);

	size_t size;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!buf) {
		return JS_EXCEPTION;
	}

	// Own a private copy of the bytes. m3_ParseModule does NOT copy — it stores
	// raw pointers into the buffer (module->wasmStart + each function's
	// func->wasm/wasmEnd) and re-reads them lazily during function-body compile.
	// JS-side ArrayBuffer mutation between parse and lazy compile (Cocos
	// Creator's bullet/spine loaders reuse/zero the buffer post-instantiation)
	// would otherwise corrupt what wasm3 reads.
	uint8_t *owned = js_malloc(ctx, size);
	if (!owned) {
		JS_FreeValue(ctx, obj);
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	memcpy(owned, buf, size);

	M3Result r = m3_ParseModule(nx_ctx->wasm_env, &m->module, owned, size);
	if (r) {
		js_free(ctx, owned);
		JS_FreeValue(ctx, obj);
		return nx_throw_wasm_error(ctx, "CompileError", r);
	}

	m->data = owned;
	m->size = size;

	return obj;
}

m3ApiRawFunction(nx_wasm_imported_func) {
	IM3Function func = _ctx->function;
	IM3FuncType funcType = func->funcType;
	nx_wasm_imported_func_t *js = _ctx->userdata;

	uint64_t *retValAddr = _sp;
	_sp += funcType->numRets;

	// Map the WASM arguments to JS values
	JSValue args[funcType->numArgs];
	for (int i = 0; i < funcType->numArgs; i++) {
		u8 type = funcType->types[funcType->numRets + i];
		args[i] = nx__wasm_tojsvalue(js->ctx, type, _sp);
		_sp++;
	}

	// Invoke the JavaScript user function
	JSValue ret_val =
		JS_Call(js->ctx, js->func, JS_NULL, funcType->numArgs, args);
	if (JS_IsException(ret_val)) {
		JS_FreeValue(js->ctx, ret_val);
		return nx_wasm_js_error;
	}

	// Map the JS return value to WASM
	if (funcType->numRets > 0) {
		if (nx__wasm_towebassemblyvalue(js->ctx, ret_val, funcType->types[0],
										retValAddr)) {
			JS_FreeValue(js->ctx, ret_val);
			return nx_wasm_js_error;
		}
		// TODO: handle multi-return values when JS returns an Array?
	}

	JS_FreeValue(js->ctx, ret_val);
	m3ApiSuccess();
}

static JSValue find_matching_import(JSContext *ctx, M3ImportInfo *info,
									JSValue imports_array,
									size_t imports_array_length) {
	for (size_t i = 0; i < imports_array_length; i++) {
		JSValue entry = JS_GetPropertyUint32(ctx, imports_array, i);

		JSValue module_val = JS_GetPropertyStr(ctx, entry, "module");
		const char *module_name = JS_ToCString(ctx, module_val);
		JS_FreeValue(ctx, module_val);
		if (strcmp(info->moduleUtf8, module_name) != 0) {
			JS_FreeCString(ctx, module_name);
			JS_FreeValue(ctx, entry);
			continue;
		}

		JSValue name_val = JS_GetPropertyStr(ctx, entry, "name");
		const char *field_name = JS_ToCString(ctx, name_val);
		JS_FreeValue(ctx, name_val);
		if (strcmp(info->fieldUtf8, field_name) != 0) {
			JS_FreeCString(ctx, module_name);
			JS_FreeCString(ctx, field_name);
			JS_FreeValue(ctx, entry);
			continue;
		}

		JS_FreeCString(ctx, module_name);
		JS_FreeCString(ctx, field_name);
		return entry;
	}

	return JS_UNDEFINED;
}

static JSValue nx_wasm_new_instance(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	JSValue opaque = JS_NewObjectClass(ctx, nx_wasm_instance_class_id);
	nx_wasm_instance_t *instance = js_mallocz(ctx, sizeof(nx_wasm_instance_t));
	if (!instance) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	// Init JS-value-typed fields so the finalizer can free safely if any of the
	// fallible setup below bails. js_mallocz gives zeroed bytes which decode as
	// JS_MKVAL(JS_TAG_INT, 0) — safe to JS_FreeValue but not semantically right.
	instance->module_value = JS_UNDEFINED;

	JS_SetOpaque(opaque, instance);

	nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
	// v15 lifecycle fix: keep the Module JS value reachable for as long as this
	// Instance is alive. Module owns m->data (the wasm bytes); instance->module
	// stores raw pointers into that buffer via wasmStart/wasmEnd + per-function
	// wasm/wasmEnd, and re-reads them lazily during function-body compile.
	instance->module_value = JS_DupValue(ctx, argv[0]);

	M3Result r =
		m3_ParseModule(nx_ctx->wasm_env, &instance->module, m->data, m->size);

	/* Create a runtime per module to avoid symbol clash. */
	IM3Runtime runtime =
		m3_NewRuntime(nx_ctx->wasm_env, /* TODO: adjust */ 512 * 1024, NULL);
	if (!runtime) {
		JS_FreeValue(ctx, opaque);
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	instance->runtime = runtime;

	JSValue imports_array = argv[1];
	uint32_t imports_array_length;
	JSValue imports_len_val =
		JS_GetPropertyStr(ctx, imports_array, "length");
	if (JS_ToUint32(ctx, &imports_array_length, imports_len_val)) {
		JS_FreeValue(ctx, imports_len_val);
		JS_FreeValue(ctx, opaque);
		return JS_EXCEPTION;
	}
	JS_FreeValue(ctx, imports_len_val);

	/* When the WASM module declares the memory as an import, we need to "map"
	   the provided `WebAssembly.Memory` data into the runtime here, before
	   loading the module. */
	if (instance->module->memoryImported) {
		M3ImportInfo *import = &instance->module->memoryImport;
		JSValue matching_import = find_matching_import(
			ctx, import, imports_array, imports_array_length);
		if (JS_IsUndefined(matching_import)) {
			JS_FreeValue(ctx, opaque);
			JS_ThrowTypeError(ctx, "Missing import memory \"%s.%s\"",
							  import->moduleUtf8, import->fieldUtf8);
			return JS_EXCEPTION;
		}

		JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");
		nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, v);

		memcpy(&runtime->memory, data->mem, sizeof(M3Memory));
		runtime->memory.mallocated->runtime = runtime;
		runtime->memory.mallocated->maxStack =
			(m3slot_t *)runtime->stack + runtime->numStackSlots;

		// TODO: what if "numPages" or "maxPages" conflict?

		if (data->needs_free) {
			js_free(ctx, data->mem);
		}
		data->mem = &runtime->memory;
		data->needs_free = false;

		JS_FreeValue(ctx, v);
		JS_FreeValue(ctx, matching_import);
	}

	r = m3_LoadModule(runtime, instance->module);
	if (r) {
		JS_FreeValue(ctx, opaque);
		return nx_throw_wasm_error(ctx, "LinkError", r);
	}

	// Process the provided "imports" into the runtime,
	// instantiate the defined "exports" from the runtime
	JSValue exports_array = JS_NewArray(ctx);
	size_t exports_index = 0;

	for (size_t i = 0; i < instance->module->numFunctions; ++i) {
		IM3Function f = &instance->module->functions[i];
		if (f->import.moduleUtf8 && f->import.fieldUtf8) {
			// Imported `Function`
			JSValue matching_import = find_matching_import(
				ctx, &f->import, imports_array, imports_array_length);
			if (JS_IsUndefined(matching_import)) {
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				JS_ThrowTypeError(ctx, "Missing import function \"%s.%s\"",
								  f->import.moduleUtf8, f->import.fieldUtf8);
				return JS_EXCEPTION;
			}

			JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");
			if (JS_IsFunction(ctx, v)) {
				nx_wasm_imported_func_t *js =
					js_malloc(ctx, sizeof(nx_wasm_imported_func_t));
				if (!js) {
					JS_FreeValue(ctx, v);
					JS_FreeValue(ctx, matching_import);
					JS_FreeValue(ctx, exports_array);
					JS_FreeValue(ctx, opaque);
					JS_ThrowOutOfMemory(ctx);
					return JS_EXCEPTION;
				}
				js->ctx = ctx;

				js->func = JS_DupValue(ctx, v);
				// js->func = v;

				M3Result r = m3_LinkRawFunctionEx(
					instance->module, f->import.moduleUtf8, f->import.fieldUtf8,
					NULL, nx_wasm_imported_func, js);
				if (r) {
					JS_FreeValue(ctx, v);
					JS_FreeValue(ctx, matching_import);
					JS_FreeValue(ctx, js->func);
					js_free(ctx, js);
					JS_FreeValue(ctx, exports_array);
					JS_FreeValue(ctx, opaque);
					return nx_throw_wasm_error(ctx, "LinkError", r);
				}

				// Track the imported func so it can be freed in the finalizer
				nx_wasm_imported_func_t **new_arr = js_realloc(
					ctx, instance->imported_funcs,
					(instance->num_imported_funcs + 1) *
						sizeof(nx_wasm_imported_func_t *));
				if (!new_arr) {
					JS_FreeValue(ctx, v);
					JS_FreeValue(ctx, matching_import);
					JS_FreeValue(ctx, exports_array);
					JS_FreeValue(ctx, opaque);
					JS_FreeValue(ctx, js->func);
					js_free(ctx, js);
					JS_ThrowOutOfMemory(ctx);
					return JS_EXCEPTION;
				}
				instance->imported_funcs = new_arr;
				instance->imported_funcs[instance->num_imported_funcs++] = js;
			}

			JS_FreeValue(ctx, v);
			JS_FreeValue(ctx, matching_import);
		} else if (f->numNames > 0) {
			// Exported `Function` — pass the Instance opaque so the JS handle
			// keeps the Instance (and therefore its runtime / module /
			// functions[]) alive for as long as the export wrapper is reachable.
			//
			// wasm-bindgen-style bundles (e.g. Rapier) emit MULTIPLE export
			// names that all alias the same underlying function. wasm3's parser
			// records up to d_m3MaxDuplicateFunctionImpl names per function
			// (16 — see m3_config.h). We must surface ALL of them as separate
			// exports so JS code that calls e.g. `A.rawvector_y` and
			// `A.rawrotation_y` (both bound to the same WASM function via
			// signature dedup) gets a function each.
			JSValue val = nx_wasm_exported_func_new(ctx, f, opaque);
			if (JS_IsException(val)) {
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				return JS_EXCEPTION;
			}

			for (u16 ni = 0; ni < f->numNames; ni++) {
				cstr_t name = f->names[ni];
				if (!name) continue;
				JSValue item = JS_NewObject(ctx);
				JS_DefinePropertyValueStr(ctx, item, "kind",
										  JS_NewString(ctx, "function"),
										  JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, item, "name",
										  JS_NewString(ctx, name),
										  JS_PROP_C_W_E);
				// All export entries for one underlying function share the same
				// callable value. Dup so each export holds its own reference.
				JS_DefinePropertyValueStr(ctx, item, "val", JS_DupValue(ctx, val),
										  JS_PROP_C_W_E);
				JS_DefinePropertyValueUint32(ctx, exports_array,
											 exports_index++, item,
											 JS_PROP_C_W_E);
			}
			// Release our own reference; the dups inside the loop keep it alive.
			JS_FreeValue(ctx, val);
		}
	}

	for (size_t i = 0; i < instance->module->numGlobals; i++) {
		const IM3Global g = &instance->module->globals[i];
		if (g->imported) {
			// Imported `Global`
			JSValue matching_import = find_matching_import(
				ctx, &g->import, imports_array, imports_array_length);
			if (JS_IsUndefined(matching_import)) {
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				JS_ThrowTypeError(ctx, "Missing import global \"%s.%s\"",
								  g->import.moduleUtf8, g->import.fieldUtf8);
				return JS_EXCEPTION;
			}

			JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");

			// TODO: handle "val" being a Number

			nx_wasm_global_t *nx_g = nx_wasm_global_get(ctx, v);
			nx_g->global = g;

			JSValue initial_value =
				JS_GetPropertyStr(ctx, matching_import, "i");
			JS_FreeValue(ctx, v);

			if (nx__wasm_towebassemblyvalue(ctx, initial_value, g->type,
											&g->i32Value)) {
				JS_FreeValue(ctx, initial_value);
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				return JS_EXCEPTION;
			}

			JS_FreeValue(ctx, initial_value);
		} else if (g->name) {
			// Exported `Global`
			JSValue op = nx_wasm_new_global(ctx, JS_UNDEFINED, 0, NULL);
			if (JS_IsException(op)) {
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				return JS_EXCEPTION;
			}
			nx_wasm_global_t *nx_g = nx_wasm_global_get(ctx, op);
			if (!nx_g) {
				JS_FreeValue(ctx, exports_array);
				JS_FreeValue(ctx, opaque);
				return JS_EXCEPTION;
			}
			nx_g->global = g;

			JSValue item = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(
				ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(
				ctx, item, "name", JS_NewString(ctx, g->name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "val", op, JS_PROP_C_W_E);
			// TODO: value ("i32")
			// TODO: mutable (true, false)
			JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++,
										 item, JS_PROP_C_W_E);
		}
	}

	if (instance->module->memoryExportName) {
		// Exported `Memory`
		JSValue val = nx_wasm_memory_new_(ctx);
		if (JS_IsException(val)) {
			JS_FreeValue(ctx, exports_array);
			JS_FreeValue(ctx, opaque);
			return JS_EXCEPTION;
		}

		nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, val);
		data->mem = &runtime->memory;
		data->needs_free = false;

		JSValue item = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, item, "kind",
								  JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "name",
			JS_NewString(ctx, instance->module->memoryExportName),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, item, "val", val, JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item,
									 JS_PROP_C_W_E);
	}

	if (instance->module->table0ExportName) {
		// Exported `Table`
		JSValue val = nx_wasm_table_new_(ctx);
		if (JS_IsException(val)) {
			JS_FreeValue(ctx, exports_array);
			JS_FreeValue(ctx, opaque);
			return JS_EXCEPTION;
		}

		nx_wasm_table_t *data = nx_wasm_table_get(ctx, val);
		data->table = instance->module->table0;
		data->table_size = &instance->module->table0Size;

		JSValue item = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "table"),
								  JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "name",
			JS_NewString(ctx, instance->module->table0ExportName),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, item, "val", val, JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item,
									 JS_PROP_C_W_E);
	}

	instance->loaded = true;

	JSValue rtn = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, rtn, 0, opaque);
	JS_SetPropertyUint32(ctx, rtn, 1, exports_array);
	return rtn;
}

static JSValue nx_wasm_module_imports(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
	if (!m)
		return JS_EXCEPTION;

	JSValue imports = JS_NewArray(ctx);
	if (JS_IsException(imports))
		return imports;

	size_t index = 0;
	for (size_t i = 0; i < m->module->numFunctions; ++i) {
		IM3Function f = &m->module->functions[i];
		if (f->import.moduleUtf8 && f->import.fieldUtf8) {
			JSValue item = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(ctx, item, "kind",
									  JS_NewString(ctx, "function"),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "module",
									  JS_NewString(ctx, f->import.moduleUtf8),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "name",
									  JS_NewString(ctx, f->import.fieldUtf8),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueUint32(ctx, imports, index++, item,
										 JS_PROP_C_W_E);
		}
	}

	for (size_t i = 0; i < m->module->numGlobals; i++) {
		IM3Global g = &m->module->globals[i];
		if (g->imported && g->import.moduleUtf8 && g->import.fieldUtf8) {
			JSValue item = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(
				ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "module",
									  JS_NewString(ctx, g->import.moduleUtf8),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "name",
									  JS_NewString(ctx, g->import.fieldUtf8),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueUint32(ctx, imports, index++, item,
										 JS_PROP_C_W_E);
		}
	}

	if (m->module->memoryImported) {
		JSValue item = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, item, "kind",
								  JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "module",
			JS_NewString(ctx, m->module->memoryImport.moduleUtf8),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "name",
			JS_NewString(ctx, m->module->memoryImport.fieldUtf8),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, imports, index++, item,
									 JS_PROP_C_W_E);
	}

	// TODO: "table" import types (wasm3 doesn't currently support)

	return imports;
}

static JSValue nx_wasm_module_exports(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
	if (!m)
		return JS_EXCEPTION;

	JSValue exports = JS_NewArray(ctx);
	if (JS_IsException(exports))
		return exports;

	size_t index = 0;
	for (size_t i = 0; i < m->module->numFunctions; ++i) {
		IM3Function f = &m->module->functions[i];
		// Emit ONE descriptor per stored alias (see Instance.exports loop
		// above and m3_parse.c for the alias list).
		for (u16 ni = 0; ni < f->numNames; ni++) {
			cstr_t name = f->names[ni];
			if (!name) continue;
			JSValue item = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(ctx, item, "kind",
									  JS_NewString(ctx, "function"),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, item, "name",
									  JS_NewString(ctx, name),
									  JS_PROP_C_W_E);
			JS_DefinePropertyValueUint32(ctx, exports, index++, item,
										 JS_PROP_C_W_E);
		}
	}

	for (size_t i = 0; i < m->module->numGlobals; ++i) {
		IM3Global g = &m->module->globals[i];
		if (!g->imported && g->name) {
			JSValue item = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(
				ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(
				ctx, item, "name", JS_NewString(ctx, g->name), JS_PROP_C_W_E);
			JS_DefinePropertyValueUint32(ctx, exports, index++, item,
										 JS_PROP_C_W_E);
		}
	}

	if (!m->module->memoryImported && m->module->memoryExportName) {
		JSValue item = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, item, "kind",
								  JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "name", JS_NewString(ctx, m->module->memoryExportName),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, exports, index++, item,
									 JS_PROP_C_W_E);
	}

	if (m->module->table0ExportName) {
		JSValue item = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "table"),
								  JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(
			ctx, item, "name", JS_NewString(ctx, m->module->table0ExportName),
			JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, exports, index++, item,
									 JS_PROP_C_W_E);
	}

	return exports;
}

static JSValue nx_wasm_call_func(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_wasm_exported_func_t *data = nx_wasm_exported_func_get(ctx, argv[0]);
	if (!data) {
		return JS_EXCEPTION;
	}

	IM3Function func = data->function;
	if (!func) {
		return nx_throw_wasm_error(ctx, "RuntimeError",
								   "Missing function reference");
	}

	M3Result r = m3Err_none;
	if (!func->compiled) {
		r = CompileFunction(func);
	}
	if (r) {
		return nx_throw_wasm_error(ctx, "RuntimeError", r);
	}

	int nargs = m3_GetArgCount(func);
	if (nargs == 0) {
		r = m3_Call(func, 0, NULL);
	} else {
		// Convert each JS argument to the WASM-declared type. The previous
		// implementation stringified every argument via JS_ToCString +
		// m3_CallArgv (strtoul/strtod), which silently turned every JS
		// boolean into 0 (because strtoul("true",NULL,10) = 0) and dropped
		// float precision in some cases. wasm-bindgen bundles (Rapier,
		// Emscripten exports) call functions with lots of bool flags + mixed
		// int/float args; the bool-stringification bug made every flag false
		// in WASM, including RigidBodyDesc.enabled / translationsEnabled* /
		// canSleep, which manifested as "physics bodies don't fall under
		// gravity". Typed conversion + m3_Call fixes the underlying issue.
		uint64_t valbuf[nargs];
		const void *argptrs[nargs];
		for (int i = 0; i < nargs; i++) {
			argptrs[i] = &valbuf[i];
			JSValueConst v = argv[i + 1];
			M3ValueType type = m3_GetArgType(func, i);
			switch (type) {
			case c_m3Type_i32: {
				int32_t iv;
				if (JS_ToInt32(ctx, &iv, v)) {
					return JS_EXCEPTION;
				}
				*(int32_t *)&valbuf[i] = iv;
				break;
			}
			case c_m3Type_i64: {
				int64_t iv;
				if (JS_ToInt64(ctx, &iv, v)) {
					return JS_EXCEPTION;
				}
				*(int64_t *)&valbuf[i] = iv;
				break;
			}
			case c_m3Type_f32: {
				double dv;
				if (JS_ToFloat64(ctx, &dv, v)) {
					return JS_EXCEPTION;
				}
				*(float *)&valbuf[i] = (float)dv;
				break;
			}
			case c_m3Type_f64: {
				double dv;
				if (JS_ToFloat64(ctx, &dv, v)) {
					return JS_EXCEPTION;
				}
				*(double *)&valbuf[i] = dv;
				break;
			}
			default:
				return nx_throw_wasm_error(ctx, "RuntimeError",
										   "unknown argument type");
			}
		}
		r = m3_Call(func, nargs, argptrs);
	}

	if (r) {
		if (r == nx_wasm_js_error) {
			// If a JavaScript error was returned then that means an
			// imported function threw an error, so re-throw here
			return JS_EXCEPTION;
		} else {
			return nx_throw_wasm_error(ctx, "RuntimeError", r);
		}
	}

	int ret_count = m3_GetRetCount(func);
	if (ret_count == 0) {
		return JS_UNDEFINED;
	}

	uint64_t valbuff[ret_count];
	const void *valptrs[ret_count];
	memset(valbuff, 0, sizeof(valbuff));
	for (int i = 0; i < ret_count; i++) {
		valptrs[i] = &valbuff[i];
	}

	r = m3_GetResults(func, ret_count, valptrs);
	if (r)
		return nx_throw_wasm_error(ctx, "RuntimeError", r);

	if (ret_count == 1) {
		return nx__wasm_tojsvalue(ctx, m3_GetRetType(func, 0), valptrs[0]);
	} else {
		JSValue rets = JS_NewArray(ctx);
		for (int i = 0; i < ret_count; i++) {
			JS_SetPropertyUint32(
				ctx, rets, i,
				nx__wasm_tojsvalue(ctx, m3_GetRetType(func, i), valptrs[i]));
		}
		return rets;
	}
}

static JSValue nx_wasm_memory_new(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	JSValue obj = nx_wasm_memory_new_(ctx);
	if (JS_IsException(obj))
		return JS_EXCEPTION;

	nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, obj);
	if (!data)
		return JS_EXCEPTION;

	IM3Memory mem = js_mallocz(ctx, sizeof(M3Memory));
	data->mem = mem;
	data->needs_free = true;
	data->is_shared = JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "shared"));
	if (data->is_shared == -1)
		return JS_EXCEPTION;

	u32 initial;
	if (JS_ToUint32(ctx, &initial, JS_GetPropertyStr(ctx, argv[0], "initial")))
		return JS_EXCEPTION;

	u32 maxPages;
	if (JS_ToUint32(ctx, &maxPages, JS_GetPropertyStr(ctx, argv[0], "maximum")))
		return JS_EXCEPTION;

	mem->numPages = initial;
	mem->maxPages = maxPages ? maxPages : 65536;

	size_t numBytes = d_m3DefaultMemPageSize * initial;
	size_t numPreviousBytes = 0;
	void *newMem = m3_Realloc("Wasm Linear Memory", mem->mallocated, numBytes,
							  numPreviousBytes);
	mem->mallocated = (M3MemoryHeader *)newMem;
	mem->mallocated->length = numBytes;

	// `runtime` and `maxStack` get set during import

	return obj;
}

// `Memory#buffer` getter function
static JSValue nx_wasm_memory_buffer_get(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, this_val);
	if (!data) {
		return JS_EXCEPTION;
	}

	IM3Memory mem = data->mem;
	if (!mem) {
		JS_ThrowTypeError(ctx, "Memory not set");
		return JS_EXCEPTION;
	}

	M3MemoryHeader *mallocated = mem->mallocated;
	if (!mallocated) {
		JS_ThrowTypeError(ctx, "Memory not allocated");
		return JS_EXCEPTION;
	}

	size_t size = mallocated->length;
	uint8_t *memory = m3MemData(mallocated);

	// Return cached buffer if BOTH size and underlying pointer are unchanged.
	// Previously checked only size — but wasm3's m3_ResizeMemory does
	// realloc(), which can move the underlying buffer to a NEW address even
	// when the size grew by N pages from a smaller previous value (the
	// general allocator behaviour). The stale ArrayBuffer would still
	// hand out reads from the freed old pointer; for wasm-bindgen libraries
	// like Rapier (which round-trip handles through linear-memory output
	// slots) this manifests as the JS side reading garbage f64 values that
	// then push internal handle-maps into multi-GB array growth and OOM.
	// See [[rapier-wasm-memory-grow-detach]] for the trip wire.
	if (!JS_IsUndefined(data->cached_buffer) &&
		data->cached_buffer_len == size &&
		data->cached_buffer_ptr == memory) {
		return JS_DupValue(ctx, data->cached_buffer);
	}

	// Invalidate old cache. Detach FIRST so any DataView / TypedArray still
	// holding a reference reads .detached === true on next access instead of
	// silently reading freed memory.
	if (!JS_IsUndefined(data->cached_buffer)) {
		JS_DetachArrayBuffer(ctx, data->cached_buffer);
		JS_FreeValue(ctx, data->cached_buffer);
	}

	JSValue buf =
		JS_NewArrayBuffer(ctx, memory, size, NULL, NULL, data->is_shared);
	if (JS_IsException(buf)) {
		data->cached_buffer = JS_UNDEFINED;
		data->cached_buffer_ptr = NULL;
		return JS_EXCEPTION;
	}

	// Cache the buffer, size, AND pointer
	data->cached_buffer = JS_DupValue(ctx, buf);
	data->cached_buffer_len = size;
	data->cached_buffer_ptr = memory;
	return buf;
}

// `Memory#grow()` function
static JSValue nx_wasm_memory_grow(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;

	IM3Memory memory = data->mem;
	if (!memory) {
		JS_ThrowTypeError(ctx, "Memory not set");
		return JS_EXCEPTION;
	}

	M3MemoryHeader *mallocated = memory->mallocated;
	if (!mallocated) {
		JS_ThrowTypeError(ctx, "Memory not allocated");
		return JS_EXCEPTION;
	}

	i32 numPagesToGrow;
	if (JS_ToInt32(ctx, &numPagesToGrow, argv[0]))
		return JS_EXCEPTION;

	if (numPagesToGrow < 0) {
		JS_ThrowTypeError(
			ctx, "WebAssembly.Memory.grow(): Argument 0 must be non-negative");
		return JS_EXCEPTION;
	}

	JSValue prevSize = JS_NewUint32(ctx, memory->numPages);

	if (numPagesToGrow > 0) {
		u32 requiredPages = memory->numPages + numPagesToGrow;

		if (requiredPages > memory->maxPages) {
			return nx_throw_wasm_error(ctx, "RuntimeError",
									   "Memory.grow would exceed maximum");
		}

		IM3Runtime runtime = m3MemRuntime(mallocated);
		if (runtime) {
			M3Result r = ResizeMemory(runtime, requiredPages);
			if (r)
				return nx_throw_wasm_error(ctx, "RuntimeError", r);
		} else {
			// Standalone Memory (not bound to an instance) — grow directly
			size_t numPreviousBytes = mallocated->length;
			size_t numBytes = d_m3DefaultMemPageSize * requiredPages;
			void *newMem = m3_Realloc("Wasm Linear Memory", mallocated,
									  numBytes, numPreviousBytes);
			if (!newMem) {
				JS_ThrowOutOfMemory(ctx);
				return JS_EXCEPTION;
			}
			memory->mallocated = (M3MemoryHeader *)newMem;
			memory->mallocated->length = numBytes;
			memory->numPages = requiredPages;
		}
	}

	return prevSize;
}

// `Table#get()` function
static JSValue nx_wasm_table_get_fn(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_wasm_table_t *data = nx_wasm_table_get(ctx, argv[0]);
	if (!data)
		return JS_EXCEPTION;

	u32 index;
	if (JS_ToUint32(ctx, &index, argv[1]))
		return JS_EXCEPTION;

	u32 size = *data->table_size;

	if (index >= size) {
		JS_ThrowRangeError(ctx,
						   "WebAssembly.Table.get(): invalid index %u into "
						   "funcref table of size %u",
						   index, size);
		return JS_EXCEPTION;
	}

	IM3Function func = data->table[index];
	if (!func) {
		return JS_NULL;
	}

	// TODO(lifecycle): the Table JS object doesn't currently hold an Instance
	// reference, so we have no way to keep the underlying module alive from
	// here. Passing JS_UNDEFINED preserves prior behavior. A future fix should
	// make nx_wasm_table_t hold a JS reference to the Instance, then pass that
	// here so wrapped Table.get'd functions don't dangle when the Instance is
	// GC'd while still in use.
	return nx_wasm_exported_func_new(ctx, func, JS_UNDEFINED);
}

// `Table#length` getter function
static JSValue nx_wasm_table_length_get(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_wasm_table_t *data = nx_wasm_table_get(ctx, this_val);
	if (!data) {
		return JS_EXCEPTION;
	}

	if (!data->table_size) {
		JS_ThrowTypeError(ctx, "Table size not set");
		return JS_EXCEPTION;
	}

	return JS_NewUint32(ctx, *data->table_size);
}

/* `new WebAssembly.Table(descriptor)` — JS-side constructor binding.
 * Allocates owned backing storage (array of `initial` NULL function
 * pointers + a u32 size cell), wraps them in a Table JSValue, and
 * returns it. JS wrapper sets the prototype via `proto()` so
 * `instanceof Table` works + prototype methods are reachable. */
static JSValue nx_wasm_table_new(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	if (argc < 1 || !JS_IsObject(argv[0])) {
		return JS_ThrowTypeError(
			ctx, "WebAssembly.Table(): descriptor must be an object");
	}

	/* descriptor.element — must be 'anyfunc' (legacy) or 'funcref'
	 * (modern) — accept both spellings since the spec aliases them.
	 * Other reference types (externref) are Tier-2 / not supported. */
	JSValue elem_val = JS_GetPropertyStr(ctx, argv[0], "element");
	if (JS_IsString(elem_val)) {
		const char *elem = JS_ToCString(ctx, elem_val);
		bool ok = elem &&
				  (strcmp(elem, "anyfunc") == 0 || strcmp(elem, "funcref") == 0);
		if (elem) JS_FreeCString(ctx, elem);
		if (!ok) {
			JS_FreeValue(ctx, elem_val);
			return JS_ThrowTypeError(
				ctx,
				"WebAssembly.Table(): only 'anyfunc' / 'funcref' element types "
				"are supported");
		}
	} else if (!JS_IsUndefined(elem_val)) {
		JS_FreeValue(ctx, elem_val);
		return JS_ThrowTypeError(
			ctx, "WebAssembly.Table(): descriptor.element must be a string");
	}
	JS_FreeValue(ctx, elem_val);

	/* descriptor.initial — required u32 */
	JSValue initial_val = JS_GetPropertyStr(ctx, argv[0], "initial");
	if (JS_IsUndefined(initial_val)) {
		JS_FreeValue(ctx, initial_val);
		return JS_ThrowTypeError(
			ctx, "WebAssembly.Table(): descriptor.initial is required");
	}
	u32 initial;
	int rc = JS_ToUint32(ctx, &initial, initial_val);
	JS_FreeValue(ctx, initial_val);
	if (rc) return JS_EXCEPTION;

	/* descriptor.maximum is accepted but not enforced in Tier-1 (we
	 * don't cap growth). Real itch.io games that pass a maximum just
	 * use it as guidance; the spec only requires checks if grow would
	 * exceed it. Defer enforcement to Tier-2 if a game actually needs it. */

	JSValue obj = nx_wasm_table_new_(ctx);
	if (JS_IsException(obj)) return JS_EXCEPTION;
	nx_wasm_table_t *data = nx_wasm_table_get(ctx, obj);
	if (!data) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}

	/* Use a 1-slot floor for the malloc so `initial == 0` doesn't trip
	 * js_mallocz's "0 bytes -> NULL is success" edge — we want to
	 * distinguish allocation failure from a legitimately empty table. */
	u32 alloc_count = initial > 0 ? initial : 1;
	data->table = js_mallocz(ctx, alloc_count * sizeof(IM3Function));
	data->table_size = js_mallocz(ctx, sizeof(u32));
	if (!data->table || !data->table_size) {
		/* Finalizer cleans up partial alloc once owned=true is set. */
		data->owned = true;
		JS_FreeValue(ctx, obj);
		return JS_ThrowOutOfMemory(ctx);
	}
	*data->table_size = initial;
	data->owned = true;
	return obj;
}

/* `Table#set(index, value)` — assign a function ref at index.
 * Tier-1 accepts `null` (clears slot) or an exported function (the
 * usual case when wiring imports). A user-supplied JS function would
 * need a trampoline to bridge into wasm3 — defer to Tier-2 if a real
 * game requires it. */
static JSValue nx_wasm_table_set_fn(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_wasm_table_t *data = nx_wasm_table_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;

	u32 index;
	if (JS_ToUint32(ctx, &index, argv[1])) return JS_EXCEPTION;

	if (index >= *data->table_size) {
		JS_ThrowRangeError(ctx,
						   "WebAssembly.Table.set(): index %u out of range for "
						   "table of size %u",
						   index, *data->table_size);
		return JS_EXCEPTION;
	}

	if (JS_IsNull(argv[2]) || JS_IsUndefined(argv[2])) {
		data->table[index] = NULL;
		return JS_UNDEFINED;
	}

	nx_wasm_exported_func_t *func_data =
		JS_GetOpaque2(ctx, argv[2], nx_wasm_exported_func_class_id);
	if (!func_data) {
		return JS_ThrowTypeError(
			ctx,
			"WebAssembly.Table.set(): value must be null or an exported "
			"function");
	}
	data->table[index] = func_data->function;
	return JS_UNDEFINED;
}

/* `Table#grow(delta)` — extend size. Returns the previous size per
 * spec. Only user-owned tables can grow — module-exported tables
 * share storage with wasm3 and growing would invalidate pointers
 * the runtime holds. */
static JSValue nx_wasm_table_grow_fn(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_wasm_table_t *data = nx_wasm_table_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;

	if (!data->owned) {
		return JS_ThrowTypeError(
			ctx, "WebAssembly.Table.grow(): cannot grow a module-owned table");
	}

	u32 delta;
	if (JS_ToUint32(ctx, &delta, argv[1])) return JS_EXCEPTION;

	u32 old_size = *data->table_size;
	u32 new_size = old_size + delta;
	if (new_size < old_size) {
		return JS_ThrowRangeError(ctx,
								  "WebAssembly.Table.grow(): size overflow");
	}

	if (new_size > 0) {
		IM3Function *new_table =
			js_realloc(ctx, data->table, new_size * sizeof(IM3Function));
		if (!new_table) return JS_ThrowOutOfMemory(ctx);
		for (u32 i = old_size; i < new_size; i++) new_table[i] = NULL;
		data->table = new_table;
	}
	*data->table_size = new_size;
	return JS_NewUint32(ctx, old_size);
}

/* Initialize the `Memory` class */
static JSValue nx_wasm_init_memory_class(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "buffer", nx_wasm_memory_buffer_get);
	NX_DEF_FUNC(proto, "grow", nx_wasm_memory_grow, 1);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

/* Initialize the `Table` class */
static JSValue nx_wasm_init_table_class(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	// NX_DEF_FUNC(proto, "get", nx_wasm_table_get_fn, 1);
	NX_DEF_GET(proto, "length", nx_wasm_table_length_get);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_wasm_validate(JSContext *ctx, JSValueConst this_val,
							   int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	if (nx_ctx->wasm_env == NULL) {
		nx_ctx->wasm_env = m3_NewEnvironment();
	}

	size_t size;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!buf) {
		return JS_FALSE;
	}

	IM3Module module = NULL;
	M3Result r = m3_ParseModule(nx_ctx->wasm_env, &module, buf, size);
	if (r) {
		return JS_FALSE;
	}

	// Successfully parsed — free the module and return true
	m3_FreeModule(module);
	return JS_TRUE;
}

static const JSCFunctionListEntry init_function_list[] = {
	JS_CFUNC_DEF("wasmCallFunc", 1, nx_wasm_call_func),
	JS_CFUNC_DEF("wasmMemNew", 1, nx_wasm_memory_new),
	JS_CFUNC_DEF("wasmTableNew", 1, nx_wasm_table_new),
	JS_CFUNC_DEF("wasmTableGet", 2, nx_wasm_table_get_fn),
	JS_CFUNC_DEF("wasmTableSet", 3, nx_wasm_table_set_fn),
	JS_CFUNC_DEF("wasmTableGrow", 2, nx_wasm_table_grow_fn),
	JS_CFUNC_DEF("wasmInitMemory", 1, nx_wasm_init_memory_class),
	JS_CFUNC_DEF("wasmInitTable", 1, nx_wasm_init_table_class),

	JS_CFUNC_DEF("wasmNewModule", 1, nx_wasm_new_module),
	JS_CFUNC_DEF("wasmNewInstance", 1, nx_wasm_new_instance),
	JS_CFUNC_DEF("wasmNewGlobal", 1, nx_wasm_new_global),
	JS_CFUNC_DEF("wasmModuleExports", 1, nx_wasm_module_exports),
	JS_CFUNC_DEF("wasmModuleImports", 1, nx_wasm_module_imports),
	JS_CFUNC_DEF("wasmGlobalGet", 1, nx_wasm_global_value_get),
	JS_CFUNC_DEF("wasmGlobalSet", 1, nx_wasm_global_value_set),
	JS_CFUNC_DEF("wasmValidate", 1, nx_wasm_validate),
};

void nx_init_wasm(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	/* WebAssembly.Global */
	JS_NewClassID(rt, &nx_wasm_global_class_id);
	JSClassDef nx_wasm_global_class = {
		"WebAssembly.Global",
		.finalizer = finalizer_wasm_global,
	};
	JS_NewClass(rt, nx_wasm_global_class_id, &nx_wasm_global_class);

	/* WebAssembly.Memory */
	JS_NewClassID(rt, &nx_wasm_memory_class_id);
	JSClassDef nx_wasm_memory_class = {
		"WebAssembly.Memory",
		.finalizer = finalizer_wasm_memory,
	};
	JS_NewClass(rt, nx_wasm_memory_class_id, &nx_wasm_memory_class);

	/* WebAssembly.Table */
	JS_NewClassID(rt, &nx_wasm_table_class_id);
	JSClassDef nx_wasm_table_class = {
		"WebAssembly.Table",
		.finalizer = finalizer_wasm_table,
	};
	JS_NewClass(rt, nx_wasm_table_class_id, &nx_wasm_table_class);

	/* WebAssembly.Function */
	JS_NewClassID(rt, &nx_wasm_exported_func_class_id);
	JSClassDef nx_wasm_exported_func_class = {
		"WebAssembly.Function",
		.finalizer = finalizer_wasm_exported_func,
	};
	JS_NewClass(rt, nx_wasm_exported_func_class_id,
				&nx_wasm_exported_func_class);

	/* WebAssembly.Module */
	JS_NewClassID(rt, &nx_wasm_module_class_id);
	JSClassDef nx_wasm_module_class = {
		"WebAssembly.Module",
		.finalizer = finalizer_wasm_module,
	};
	JS_NewClass(rt, nx_wasm_module_class_id, &nx_wasm_module_class);

	/* WebAssembly.Instance */
	JS_NewClassID(rt, &nx_wasm_instance_class_id);
	JSClassDef nx_wasm_instance_class = {
		"WebAssembly.Instance",
		.finalizer = finalizer_wasm_instance,
	};
	JS_NewClass(rt, nx_wasm_instance_class_id, &nx_wasm_instance_class);

	JS_SetPropertyFunctionList(ctx, init_obj, init_function_list,
							   countof(init_function_list));
}
