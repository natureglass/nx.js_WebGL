import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import type { Screen } from '../screen';
import { assertInternalConstructor, createInternal, def, proto, stub } from '../utils';

interface WebGLRenderingContextInternal {
	canvas: Screen;
}

const _ = createInternal<WebGLRenderingContext, WebGLRenderingContextInternal>();

export type WebGLShader = object;
export type WebGLProgram = object;
export type WebGLBuffer = object;
export type WebGLUniformLocation = object;
export type WebGLTexture = object;

export interface WebGLActiveInfo {
	name: string;
	size: number;
	type: number;
}

export interface WebGLContextAttributes {
	alpha: boolean;
	depth: boolean;
	stencil: boolean;
	antialias: boolean;
	premultipliedAlpha: boolean;
	preserveDrawingBuffer: boolean;
	powerPreference?: string;
	failIfMajorPerformanceCaveat?: boolean;
}

export interface WebGLShaderPrecisionFormat {
	rangeMin: number;
	rangeMax: number;
	precision: number;
}

export interface WebGLBackendInfo {
	target: string;
	built: boolean;
	available: boolean;
	status: string;
	probeStep: number;
	eglMajor: number;
	eglMinor: number;
	glVendor: string;
	glVersion: string;
	glRenderer: string;
	bridgeRequestedWidth: number;
	bridgeRequestedHeight: number;
	bridgeRenderWidth: number;
	bridgeRenderHeight: number;
}

export interface WebGLGpuPrototypeResult {
	ok: boolean;
	status: string;
	width?: number;
	height?: number;
	copiedPixels?: number;
	frameCount?: number;
	elapsedMs?: number;
	averageFrameMs?: number;
	fps?: number;
	red: number;
	green: number;
	blue: number;
	alpha: number;
}

/**
 * Minimal native-backed WebGL 1.0 context.
 *
 * This first milestone only supports framebuffer clear operations. Shader,
 * program, buffer, texture, and framebuffer APIs are intentionally still stubs.
 */
export class WebGLRenderingContext {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		const canvas: Screen = arguments[1];
		const ctx = proto($.webglContextNew(canvas), WebGLRenderingContext);
		_.set(ctx, { canvas });
		return ctx;
	}

	get canvas() {
		return _(this).canvas;
	}

	declare readonly drawingBufferWidth: number;
	declare readonly drawingBufferHeight: number;

	declare readonly NO_ERROR: number;
	declare readonly INVALID_ENUM: number;
	declare readonly INVALID_VALUE: number;
	declare readonly INVALID_OPERATION: number;
	declare readonly POINTS: number;
	declare readonly LINES: number;
	declare readonly LINE_LOOP: number;
	declare readonly LINE_STRIP: number;
	declare readonly TRIANGLES: number;
	declare readonly ZERO: number;
	declare readonly ONE: number;
	declare readonly NEVER: number;
	declare readonly LESS: number;
	declare readonly EQUAL: number;
	declare readonly LEQUAL: number;
	declare readonly GREATER: number;
	declare readonly NOTEQUAL: number;
	declare readonly GEQUAL: number;
	declare readonly ALWAYS: number;
	declare readonly DEPTH_BUFFER_BIT: number;
	declare readonly STENCIL_BUFFER_BIT: number;
	declare readonly COLOR_BUFFER_BIT: number;
	declare readonly CULL_FACE: number;
	declare readonly CULL_FACE_MODE: number;
	declare readonly FRONT_FACE: number;
	declare readonly FRONT: number;
	declare readonly BACK: number;
	declare readonly FRONT_AND_BACK: number;
	declare readonly CW: number;
	declare readonly CCW: number;
	declare readonly DEPTH_TEST: number;
	declare readonly DITHER: number;
	declare readonly BLEND: number;
	declare readonly FUNC_ADD: number;
	declare readonly BLEND_EQUATION: number;
	declare readonly BLEND_EQUATION_RGB: number;
	declare readonly BLEND_EQUATION_ALPHA: number;
	declare readonly BLEND_SRC_RGB: number;
	declare readonly BLEND_DST_RGB: number;
	declare readonly BLEND_SRC_ALPHA: number;
	declare readonly BLEND_DST_ALPHA: number;
	declare readonly SCISSOR_TEST: number;
	declare readonly SCISSOR_BOX: number;
	declare readonly STENCIL_TEST: number;
	declare readonly STENCIL_CLEAR_VALUE: number;
	declare readonly STENCIL_WRITEMASK: number;
	declare readonly STENCIL_FUNC: number;
	declare readonly STENCIL_REF: number;
	declare readonly STENCIL_VALUE_MASK: number;
	declare readonly STENCIL_FAIL: number;
	declare readonly STENCIL_PASS_DEPTH_FAIL: number;
	declare readonly STENCIL_PASS_DEPTH_PASS: number;
	declare readonly KEEP: number;
	declare readonly VIEWPORT: number;
	declare readonly ALIASED_POINT_SIZE_RANGE: number;
	declare readonly ALIASED_LINE_WIDTH_RANGE: number;
	declare readonly DEPTH_CLEAR_VALUE: number;
	declare readonly DEPTH_FUNC: number;
	declare readonly DEPTH_WRITEMASK: number;
	declare readonly COLOR_CLEAR_VALUE: number;
	declare readonly COLOR_WRITEMASK: number;
	declare readonly POLYGON_OFFSET_FACTOR: number;
	declare readonly POLYGON_OFFSET_UNITS: number;
	declare readonly RED_BITS: number;
	declare readonly GREEN_BITS: number;
	declare readonly BLUE_BITS: number;
	declare readonly ALPHA_BITS: number;
	declare readonly DEPTH_BITS: number;
	declare readonly STENCIL_BITS: number;
	declare readonly UNPACK_ALIGNMENT: number;
	declare readonly PACK_ALIGNMENT: number;
	declare readonly VENDOR: number;
	declare readonly RENDERER: number;
	declare readonly VERSION: number;
	declare readonly VERTEX_SHADER: number;
	declare readonly FRAGMENT_SHADER: number;
	declare readonly LOW_FLOAT: number;
	declare readonly MEDIUM_FLOAT: number;
	declare readonly HIGH_FLOAT: number;
	declare readonly LOW_INT: number;
	declare readonly MEDIUM_INT: number;
	declare readonly HIGH_INT: number;
	declare readonly COMPILE_STATUS: number;
	declare readonly LINK_STATUS: number;
	declare readonly DELETE_STATUS: number;
	declare readonly SHADER_TYPE: number;
	declare readonly ATTACHED_SHADERS: number;
	declare readonly CURRENT_PROGRAM: number;
	declare readonly ACTIVE_UNIFORMS: number;
	declare readonly ACTIVE_ATTRIBUTES: number;
	declare readonly ACTIVE_TEXTURE: number;
	declare readonly BYTE: number;
	declare readonly UNSIGNED_BYTE: number;
	declare readonly SHORT: number;
	declare readonly UNSIGNED_SHORT: number;
	declare readonly INT: number;
	declare readonly UNSIGNED_INT: number;
	declare readonly FLOAT: number;
	declare readonly FLOAT_VEC2: number;
	declare readonly FLOAT_VEC4: number;
	declare readonly FLOAT_MAT4: number;
	declare readonly SAMPLER_2D: number;
	declare readonly ARRAY_BUFFER: number;
	declare readonly ARRAY_BUFFER_BINDING: number;
	declare readonly ELEMENT_ARRAY_BUFFER: number;
	declare readonly ELEMENT_ARRAY_BUFFER_BINDING: number;
	declare readonly FRAMEBUFFER: number;
	declare readonly FRAMEBUFFER_BINDING: number;
	declare readonly RENDERBUFFER_BINDING: number;
	declare readonly BUFFER_SIZE: number;
	declare readonly BUFFER_USAGE: number;
	declare readonly STREAM_DRAW: number;
	declare readonly STATIC_DRAW: number;
	declare readonly DYNAMIC_DRAW: number;
	declare readonly TEXTURE_2D: number;
	declare readonly TEXTURE_BINDING_2D: number;
	declare readonly TEXTURE_CUBE_MAP: number;
	declare readonly TEXTURE_BINDING_CUBE_MAP: number;
	declare readonly TEXTURE_CUBE_MAP_POSITIVE_X: number;
	declare readonly TEXTURE_CUBE_MAP_NEGATIVE_X: number;
	declare readonly TEXTURE_CUBE_MAP_POSITIVE_Y: number;
	declare readonly TEXTURE_CUBE_MAP_NEGATIVE_Y: number;
	declare readonly TEXTURE_CUBE_MAP_POSITIVE_Z: number;
	declare readonly TEXTURE_CUBE_MAP_NEGATIVE_Z: number;
	declare readonly TEXTURE0: number;
	declare readonly TEXTURE_MIN_FILTER: number;
	declare readonly TEXTURE_MAG_FILTER: number;
	declare readonly TEXTURE_WRAP_S: number;
	declare readonly TEXTURE_WRAP_T: number;
	declare readonly NEAREST: number;
	declare readonly LINEAR: number;
	declare readonly CLAMP_TO_EDGE: number;
	declare readonly REPEAT: number;
	declare readonly RGBA: number;
	declare readonly SRC_COLOR: number;
	declare readonly ONE_MINUS_SRC_COLOR: number;
	declare readonly SRC_ALPHA: number;
	declare readonly ONE_MINUS_SRC_ALPHA: number;
	declare readonly DST_ALPHA: number;
	declare readonly ONE_MINUS_DST_ALPHA: number;
	declare readonly DST_COLOR: number;
	declare readonly ONE_MINUS_DST_COLOR: number;
	declare readonly SRC_ALPHA_SATURATE: number;
	declare readonly MAX_TEXTURE_SIZE: number;
	declare readonly MAX_VIEWPORT_DIMS: number;
	declare readonly MAX_VERTEX_ATTRIBS: number;
	declare readonly MAX_TEXTURE_IMAGE_UNITS: number;
	declare readonly MAX_VERTEX_TEXTURE_IMAGE_UNITS: number;
	declare readonly MAX_COMBINED_TEXTURE_IMAGE_UNITS: number;
	declare readonly MAX_CUBE_MAP_TEXTURE_SIZE: number;
	declare readonly MAX_RENDERBUFFER_SIZE: number;
	declare readonly MAX_VERTEX_UNIFORM_VECTORS: number;
	declare readonly MAX_FRAGMENT_UNIFORM_VECTORS: number;
	declare readonly MAX_VARYING_VECTORS: number;
	declare readonly SHADING_LANGUAGE_VERSION: number;
	declare readonly POLYGON_OFFSET_FILL: number;
	declare readonly SAMPLE_ALPHA_TO_COVERAGE: number;
	declare readonly SAMPLE_COVERAGE: number;

	getContextAttributes(): WebGLContextAttributes {
		stub();
	}

	getSupportedExtensions(): string[] {
		stub();
	}

	getExtension(name: string): unknown | null {
		stub();
	}

	getShaderPrecisionFormat(
		shaderType: number,
		precisionType: number,
	): WebGLShaderPrecisionFormat | null {
		stub();
	}

	clearColor(red: number, green: number, blue: number, alpha: number): void {
		stub();
	}

	clearDepth(depth: number): void {
		stub();
	}

	clearStencil(stencil: number): void {
		stub();
	}

	clear(mask: number): void {
		stub();
	}

	createShader(type: number): WebGLShader | null {
		stub();
	}

	shaderSource(shader: WebGLShader, source: string): void {
		stub();
	}

	compileShader(shader: WebGLShader): void {
		stub();
	}

	getShaderParameter(shader: WebGLShader, pname: number): unknown {
		stub();
	}

	getShaderInfoLog(shader: WebGLShader): string | null {
		stub();
	}

	deleteShader(shader: WebGLShader | null): void {
		stub();
	}

	createProgram(): WebGLProgram | null {
		stub();
	}

	attachShader(program: WebGLProgram, shader: WebGLShader): void {
		stub();
	}

	linkProgram(program: WebGLProgram): void {
		stub();
	}

	useProgram(program: WebGLProgram | null): void {
		stub();
	}

	getProgramParameter(program: WebGLProgram, pname: number): unknown {
		stub();
	}

	getProgramInfoLog(program: WebGLProgram): string | null {
		stub();
	}

	getActiveAttrib(program: WebGLProgram, index: number): WebGLActiveInfo | null {
		stub();
	}

	getActiveUniform(program: WebGLProgram, index: number): WebGLActiveInfo | null {
		stub();
	}

	deleteProgram(program: WebGLProgram | null): void {
		stub();
	}

	createBuffer(): WebGLBuffer | null {
		stub();
	}

	bindBuffer(target: number, buffer: WebGLBuffer | null): void {
		stub();
	}

	bufferData(target: number, data: ArrayBufferView | ArrayBuffer | number, usage: number): void {
		stub();
	}

	bufferSubData(target: number, offset: number, data: ArrayBufferView | ArrayBuffer): void {
		stub();
	}

	deleteBuffer(buffer: WebGLBuffer | null): void {
		stub();
	}

	createTexture(): WebGLTexture | null {
		stub();
	}

	activeTexture(texture: number): void {
		stub();
	}

	bindTexture(target: number, texture: WebGLTexture | null): void {
		stub();
	}

	texImage2D(
		target: number,
		level: number,
		internalformat: number,
		width: number,
		height: number,
		border: number,
		format: number,
		type: number,
		pixels: ArrayBufferView | ArrayBuffer | null,
	): void {
		stub();
	}

	texSubImage2D(
		target: number,
		level: number,
		xoffset: number,
		yoffset: number,
		width: number,
		height: number,
		format: number,
		type: number,
		pixels: ArrayBufferView | ArrayBuffer,
	): void {
		stub();
	}

	texParameteri(target: number, pname: number, param: number): void {
		stub();
	}

	deleteTexture(texture: WebGLTexture | null): void {
		stub();
	}

	getUniformLocation(program: WebGLProgram, name: string): WebGLUniformLocation | null {
		stub();
	}

	uniform2f(location: WebGLUniformLocation | null, x: number, y: number): void {
		stub();
	}

	uniform1i(location: WebGLUniformLocation | null, x: number): void {
		stub();
	}

	uniform4f(
		location: WebGLUniformLocation | null,
		x: number,
		y: number,
		z: number,
		w: number,
	): void {
		stub();
	}

	uniformMatrix4fv(
		location: WebGLUniformLocation | null,
		transpose: boolean,
		value: Float32Array | number[],
	): void {
		stub();
	}

	getAttribLocation(program: WebGLProgram, name: string): number {
		stub();
	}

	enableVertexAttribArray(index: number): void {
		stub();
	}

	disableVertexAttribArray(index: number): void {
		stub();
	}

	vertexAttribPointer(
		index: number,
		size: number,
		type: number,
		normalized: boolean,
		stride: number,
		offset: number,
	): void {
		stub();
	}

	drawArrays(mode: number, first: number, count: number): void {
		stub();
	}

	drawElements(mode: number, count: number, type: number, offset: number): void {
		stub();
	}

	enable(cap: number): void {
		stub();
	}

	disable(cap: number): void {
		stub();
	}

	depthFunc(func: number): void {
		stub();
	}

	depthMask(flag: boolean): void {
		stub();
	}

	colorMask(red: boolean, green: boolean, blue: boolean, alpha: boolean): void {
		stub();
	}

	blendEquation(mode: number): void {
		stub();
	}

	blendFunc(sfactor: number, dfactor: number): void {
		stub();
	}

	blendFuncSeparate(
		srcRGB: number,
		dstRGB: number,
		srcAlpha: number,
		dstAlpha: number,
	): void {
		stub();
	}

	blendColor(red: number, green: number, blue: number, alpha: number): void {
		stub();
	}

	cullFace(mode: number): void {
		stub();
	}

	frontFace(mode: number): void {
		stub();
	}

	polygonOffset(factor: number, units: number): void {
		stub();
	}

	stencilMask(mask: number): void {
		stub();
	}

	stencilFunc(func: number, ref: number, mask: number): void {
		stub();
	}

	stencilOp(fail: number, zfail: number, zpass: number): void {
		stub();
	}

	bindFramebuffer(target: number, framebuffer: null): void {
		stub();
	}

	lineWidth(width: number): void {
		stub();
	}

	viewport(x: number, y: number, width: number, height: number): void {
		stub();
	}

	scissor(x: number, y: number, width: number, height: number): void {
		stub();
	}

	getParameter(pname: number): unknown {
		stub();
	}

	getError(): number {
		stub();
	}

	getBackendInfo(): WebGLBackendInfo {
		stub();
	}

	clearGpuPrototype(): boolean {
		stub();
	}

	probeGpuPrototypeStep(): boolean {
		stub();
	}

	triangleGpuPrototype(): WebGLGpuPrototypeResult {
		stub();
	}

	bridgeGpuPrototype(): WebGLGpuPrototypeResult {
		stub();
	}

	bridgeGpuBenchmarkPrototype(
		frameCount?: number,
		width?: number,
		height?: number,
	): WebGLGpuPrototypeResult {
		stub();
	}

	enableGpuBridgePrototype(enabled?: boolean): boolean {
		stub();
	}

	setGpuBridgeResolutionPrototype(width?: number, height?: number): boolean {
		stub();
	}
}
$.webglContextInitClass(WebGLRenderingContext);
def(WebGLRenderingContext);
