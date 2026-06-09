import { $ } from './$';
import { bufferSourceToArrayBuffer, proto } from './utils';
import type { BufferSource } from './types';
import type {
	WasmModuleOpaque,
	WasmInstanceOpaque,
	WasmGlobalOpaque,
} from './internal';

export interface GlobalDescriptor<T extends ValueType = ValueType> {
	mutable?: boolean;
	value: T;
}

export interface MemoryDescriptor {
	initial: number;
	maximum?: number;
	shared?: boolean;
}

export interface ModuleExportDescriptor {
	kind: ImportExportKind;
	name: string;
}

export interface ModuleImportDescriptor {
	kind: ImportExportKind;
	module: string;
	name: string;
}

export interface TableDescriptor {
	element: TableKind;
	initial: number;
	maximum?: number;
}

export interface ValueTypeMap {
	anyfunc: Function;
	externref: any;
	f32: number;
	f64: number;
	i32: number;
	i64: bigint;
	v128: never;
}

export interface WebAssemblyInstantiatedSource {
	instance: Instance;
	module: Module;
}

export type ImportExportKind = 'function' | 'global' | 'memory' | 'table';
export type TableKind = 'anyfunc' | 'externref';
export type ExportValue = Function | Global | Memory | Table;
export type Exports = Record<string, ExportValue>;
export type ImportValue = ExportValue | number;
export type Imports = Record<string, ModuleImports>;
export type ModuleImports = Record<string, ImportValue>;
export type ValueType = keyof ValueTypeMap;

export class CompileError extends Error implements WebAssembly.CompileError {
	name = 'CompileError';
}
export class RuntimeError extends Error implements WebAssembly.RuntimeError {
	name = 'RuntimeError';
}
export class LinkError extends Error implements WebAssembly.LinkError {
	name = 'LinkError';
}

function toWasmError(e: unknown) {
	if (e && e instanceof Error && 'wasmError' in e) {
		switch (e.wasmError) {
			case 'CompileError':
				return new CompileError(e.message);
			case 'LinkError':
				return new LinkError(e.message);
			case 'RuntimeError':
				return new RuntimeError(e.message);
		}
	}
	return e;
}

interface GlobalInternals<T extends ValueType = ValueType> {
	descriptor: GlobalDescriptor<T>;
	value?: ValueTypeMap[T];
	opaque?: WasmGlobalOpaque;
}

const globalInternalsMap = new WeakMap<Global, GlobalInternals<any>>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Global) */
export class Global<T extends ValueType = ValueType>
	implements WebAssembly.Global
{
	constructor(descriptor: GlobalDescriptor<T>, value?: ValueTypeMap[T]) {
		globalInternalsMap.set(this, { descriptor, value });
	}

	/**
	 * The value contained inside the global variable — this can be used to directly set and get the global's value.
	 */
	get value(): ValueTypeMap[T] {
		const i = globalInternalsMap.get(this)!;
		return i.opaque ? $.wasmGlobalGet(i.opaque) : i.value;
	}

	set value(v: ValueTypeMap[T]) {
		const i = globalInternalsMap.get(this)!;
		if (i.opaque) {
			$.wasmGlobalSet(i.opaque, v);
		} else {
			i.value = v;
		}
	}

	/**
	 * Old-style method that returns the value contained inside the global variable.
	 */
	valueOf() {
		return this.value;
	}
}

function bindGlobal(g: Global, opaque = $.wasmNewGlobal()) {
	const i = globalInternalsMap.get(g);
	if (!i) throw new Error(`No internal state for Global`);
	i.opaque = opaque;
	return opaque;
}

function unwrapImports(importObject: Imports = {}) {
	return Object.entries(importObject).flatMap(([m, i]) =>
		Object.entries(i).map(([n, v]) => {
			let val;
			let i;
			let kind: ImportExportKind;
			if (typeof v === 'function') {
				kind = 'function';
				val = v;
			} else if (v instanceof Global) {
				kind = 'global';
				i = v.value;
				val = bindGlobal(v);
			} else if (v instanceof Memory) {
				kind = 'memory';
				val = v;
			} else {
				// TODO: Handle "table" type
				throw new LinkError(`Unsupported import type for "${m}.${n}"`);
			}
			return {
				module: m,
				name: n,
				kind,
				val,
				i,
			};
		}),
	);
}

/** Hidden property attached to bound exported-function wrappers so
 * `Table.set` can recover the raw `nx_wasm_exported_func_t` opaque to
 * hand to the C side. Without this, `Table.set` only worked when the
 * caller passed a raw opaque directly (rare in user code — Instance
 * exports are always bound wrappers). */
const RAW_WASM_FUNC = Symbol('rawWasmFunc');

function wrapExports(ex: any[]): Exports {
	const e: Exports = Object.create(null);
	for (const v of ex) {
		if (v.kind === 'function') {
			const fn = callFunc.bind(null, v.val);
			Object.defineProperty(fn, 'name', { value: v.name });
			// Tag with the raw opaque so Table.set can unwrap it.
			Object.defineProperty(fn, RAW_WASM_FUNC, { value: v.val });
			e[v.name] = fn;
		} else if (v.kind === 'global') {
			const g = new Global({ value: v.value, mutable: v.mutable });
			bindGlobal(g, v.val);
			e[v.name] = g;
		} else if (v.kind === 'memory') {
			e[v.name] = proto(v.val, Memory);
		} else if (v.kind === 'table') {
			e[v.name] = proto(v.val, Table);
		} else {
			throw new LinkError(`Unsupported export type "${v.kind}"`);
		}
	}
	return Object.freeze(e);
}

interface InstanceInternals {
	module: Module;
	opaque: WasmInstanceOpaque;
}

const instanceInternalsMap = new WeakMap<Instance, InstanceInternals>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Instance) */
export class Instance implements WebAssembly.Instance {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Instance/exports) */
	readonly exports: Exports;

	constructor(moduleObject: Module, importObject?: Imports) {
		const modInternal = moduleInternalsMap.get(moduleObject);
		if (!modInternal) throw new Error(`No internal state for Module`);
		const [opaque, exp] = $.wasmNewInstance(
			modInternal.opaque,
			unwrapImports(importObject),
		);
		instanceInternalsMap.set(this, { module: moduleObject, opaque });
		this.exports = wrapExports(exp);
	}
}

function callFunc(
	func: any, // exported func
	...args: unknown[]
): unknown {
	try {
		return $.wasmCallFunc(func, ...args);
	} catch (err: unknown) {
		throw toWasmError(err);
	}
}

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory)
 */
export class Memory implements WebAssembly.Memory {
	constructor(descriptor: MemoryDescriptor) {
		return proto($.wasmMemNew(descriptor), Memory);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/buffer) */
	declare readonly buffer: ArrayBuffer;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/grow) */
	// @ts-expect-error This is a native function
	grow(delta: number): number {}
}
$.wasmInitMemory(Memory);

interface ModuleInternals {
	buffer: ArrayBuffer;
	opaque: WasmModuleOpaque;
}

const moduleInternalsMap = new WeakMap<Module, ModuleInternals>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module) */
export class Module implements WebAssembly.Module {
	constructor(bytes: BufferSource) {
		const buffer = bufferSourceToArrayBuffer(bytes);
		moduleInternalsMap.set(this, {
			// Hold a reference to the bytes to prevent garbage collection
			buffer,
			opaque: $.wasmNewModule(buffer),
		});
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/customSections) */
	static customSections(
		moduleObject: Module,
		sectionName: string,
	): ArrayBuffer[] {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/exports) */
	static exports(moduleObject: Module): ModuleExportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return $.wasmModuleExports(i.opaque);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/imports) */
	static imports(moduleObject: Module): ModuleImportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return $.wasmModuleImports(i.opaque);
	}
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table) */
export class Table implements WebAssembly.Table {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/length) */
	declare readonly length: number;

	constructor(descriptor: TableDescriptor, value?: any) {
		// C side allocates owned backing storage + returns a wrapped
		// JSValue with the Table class-id + opaque set; `proto()` then
		// installs Table.prototype so `instanceof Table` works and the
		// C-installed `length` getter resolves. Mirrors the Memory
		// pattern at L227. Tier-1 ignores the optional `value` (slot
		// initializer) since the C ctor already zero-fills.
		return proto($.wasmTableNew(descriptor), Table);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/get) */
	get(index: number): any {
		// Only function refs are supported for now
		const fn = $.wasmTableGet(this, index);
		return callFunc.bind(null, fn);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/grow) */
	grow(delta: number, value?: any): number {
		return $.wasmTableGrow(this, delta);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/set)
	 *
	 * Tier-2 extension: accepts a raw JS function as `value`. nxjs
	 * synthesizes a tiny wasm wrapper module (one import + one passthrough
	 * export) so the JS function becomes a normal wasm exported function
	 * that wasm3 can dispatch through the table.
	 *
	 * Signature inference order:
	 *   1. `value.__wasmSig` string (Emscripten format: 'v'/'i'/'I'/'f'/'d')
	 *   2. Optional third positional argument (same format)
	 *   3. Fallback: `'i'` return + N×`'i'` args from `value.length` (Emscripten
	 *      addFunction's most common shape)
	 */
	set(index: number, value?: any, signature?: string): void {
		if (typeof value === 'function') {
			const raw = (value as any)[RAW_WASM_FUNC];
			if (raw !== undefined) {
				$.wasmTableSet(this, index, raw);
				return;
			}
			const sig: string =
				(value as any).__wasmSig ??
				signature ??
				('i' + 'i'.repeat(value.length));
			const wrapped = convertJsFunctionToWasm(value, sig);
			$.wasmTableSet(this, index, (wrapped as any)[RAW_WASM_FUNC]);
			return;
		}
		$.wasmTableSet(this, index, value);
	}
}
$.wasmInitTable(Table);

/**
 * Tier-2 trampoline: wraps a raw JS function as a callable wasm
 * exported function. Mirrors what browsers' internal
 * `convertJsFunctionToWasm` (and Emscripten's `addFunction`) does — we
 * synthesize a tiny one-import / one-export wasm module on the fly,
 * instantiate it with `func` bound to the import, and return the
 * exported wrapper.
 *
 * Signature is an Emscripten-style string where the FIRST char is the
 * return type and remaining chars are arg types: `v`=void / `i`=i32 /
 * `I`=i64 / `f`=f32 / `d`=f64. Examples: `"v"` (void→void), `"vi"`
 * (void→i32), `"ii"` (i32→i32), `"iiii"` (i32→i32,i32,i32).
 *
 * Returned function is a bound caller (same shape as
 * `instance.exports.<name>`) and is tagged with `RAW_WASM_FUNC` so
 * `Table.set` recognises it.
 */
function convertJsFunctionToWasm(func: Function, sig: string): Function {
	if (typeof sig !== 'string' || sig.length === 0) {
		throw new TypeError(
			'convertJsFunctionToWasm: signature must be a non-empty string',
		);
	}
	const bytes = buildTrampolineWasmBytes(sig);
	const mod = new Module(bytes);
	const inst = new Instance(mod, { e: { f: func as any } });
	const exported = inst.exports.f as Function;
	if (!exported) {
		throw new Error('convertJsFunctionToWasm: synthesized module missing "f" export');
	}
	return exported;
}

/** Map Emscripten sig char → wasm value type byte. Throws on unknown
 * char so we don't silently emit malformed wasm. */
function sigCharToValType(c: string): number {
	switch (c) {
		case 'i': return 0x7f; // i32
		case 'I': return 0x7e; // i64
		case 'f': return 0x7d; // f32
		case 'd': return 0x7c; // f64
		default:
			throw new TypeError(
				`convertJsFunctionToWasm: unknown signature char '${c}' (use 'v'/'i'/'I'/'f'/'d')`,
			);
	}
}

/** Build the minimal valid wasm module bytes for a trampoline of the
 * given signature. Layout:
 *
 *   - magic + version
 *   - type section:    [func(args) → ret]
 *   - import section:  "e"."f" (func type 0)
 *   - func section:    one func of type 0
 *   - export section:  "f" → func index 1 (0 is the import, 1 is ours)
 *   - code section:    local.get 0..N; call 0; end
 */
function buildTrampolineWasmBytes(sig: string): Uint8Array {
	const retChar = sig[0];
	const argsChars = sig.slice(1);
	const argTypes = [...argsChars].map(sigCharToValType);
	const retTypes: number[] = retChar === 'v' ? [] : [sigCharToValType(retChar)];

	const out: number[] = [];

	// helpers
	const u8 = (b: number) => out.push(b & 0xff);
	const uleb = (n: number, dst: number[] = out) => {
		do {
			let byte = n & 0x7f;
			n >>>= 7;
			if (n !== 0) byte |= 0x80;
			dst.push(byte);
		} while (n !== 0);
	};
	const section = (id: number, payload: number[]) => {
		u8(id);
		uleb(payload.length);
		for (const b of payload) u8(b);
	};
	const ulebTo = (n: number, dst: number[]) => {
		do {
			let byte = n & 0x7f;
			n >>>= 7;
			if (n !== 0) byte |= 0x80;
			dst.push(byte);
		} while (n !== 0);
	};

	// magic + version
	for (const b of [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]) u8(b);

	// type section (id=1): 1 type, func, numArgs, args, numRets, rets
	{
		const p: number[] = [];
		ulebTo(1, p); // 1 type
		p.push(0x60); // func type
		ulebTo(argTypes.length, p);
		for (const t of argTypes) p.push(t);
		ulebTo(retTypes.length, p);
		for (const t of retTypes) p.push(t);
		section(0x01, p);
	}

	// import section (id=2): 1 import, "e" / "f", kind=func, type idx 0
	{
		const p: number[] = [];
		ulebTo(1, p);
		ulebTo(1, p); p.push(0x65); // "e"
		ulebTo(1, p); p.push(0x66); // "f"
		p.push(0x00); // kind = func
		ulebTo(0, p); // type idx 0
		section(0x02, p);
	}

	// function section (id=3): 1 func of type 0
	{
		const p: number[] = [];
		ulebTo(1, p);
		ulebTo(0, p);
		section(0x03, p);
	}

	// export section (id=7): 1 export, "f", kind=func, idx=1 (0 is the import)
	{
		const p: number[] = [];
		ulebTo(1, p);
		ulebTo(1, p); p.push(0x66); // "f"
		p.push(0x00); // kind = func
		ulebTo(1, p);
		section(0x07, p);
	}

	// code section (id=10): 1 body
	{
		// body bytes
		const body: number[] = [];
		ulebTo(0, body); // 0 local decl groups
		for (let i = 0; i < argTypes.length; i++) {
			body.push(0x20); // local.get
			ulebTo(i, body);
		}
		body.push(0x10); // call
		ulebTo(0, body); // func idx 0 (the imported JS fn)
		body.push(0x0b); // end

		const p: number[] = [];
		ulebTo(1, p); // 1 body
		ulebTo(body.length, p);
		for (const b of body) p.push(b);
		section(0x0a, p);
	}

	return new Uint8Array(out);
}

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compile)
 */
export async function compile(bytes: BufferSource): Promise<Module> {
	// TODO: run this on the thread pool?
	return new Module(bytes);
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compileStreaming) */
export async function compileStreaming(
	source: Response | PromiseLike<Response>,
): Promise<Module> {
	const res = await source;
	if (!res.ok) {
		// TODO: throw error?
	}
	const buf = await res.arrayBuffer();
	return compile(buf);
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/instantiate) */
export function instantiate(
	bytes: BufferSource,
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource>;
export function instantiate(
	moduleObject: Module,
	importObject?: Imports,
): Promise<Instance>;
export async function instantiate(
	bytes: BufferSource | Module,
	importObject?: Imports,
) {
	if (bytes instanceof Module) {
		return new Instance(bytes, importObject);
	}
	const m = await compile(bytes);

	// TODO: run this on the thread pool?
	// maybe not, because we need to interact with JS here?
	const instance = new Instance(m, importObject);

	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/instantiateStreaming) */
export async function instantiateStreaming(
	source: Response | PromiseLike<Response>,
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource> {
	const m = await compileStreaming(source);
	const instance = await instantiate(m, importObject);
	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/validate) */
export function validate(bytes: BufferSource): boolean {
	const buffer = bufferSourceToArrayBuffer(bytes);
	return $.wasmValidate(buffer);
}
