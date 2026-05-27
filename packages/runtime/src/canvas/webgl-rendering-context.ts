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

	readPixels(
		x: number,
		y: number,
		width: number,
		height: number,
		format: number,
		type: number,
		pixels: ArrayBufferView,
	): void {
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

export type WebGLVertexArrayObject = object;
export type WebGLQuery = object;
export type WebGLSampler = object;
export type WebGLSync = object;
export type WebGLTransformFeedback = object;

/**
 * WebGL 2 rendering context. Inheritance from {@link WebGLRenderingContext}
 * is established AFTER the class declaration via `Object.setPrototypeOf` so
 * we can use the same "construct native + return it" pattern as the parent
 * class without paying the cost of calling `super()` (which would allocate a
 * second `nx_webgl_context_t` we'd immediately throw away). QuickJS treats
 * derived-class constructors that don't call `super()` more strictly than
 * V8/SpiderMonkey — even when an object is explicitly returned, the result
 * came back as `null` to the caller (verified 2026-05-22 via the
 * webgl2-smoke direct probe). Making the class standalone and wiring up the
 * prototype chain manually sidesteps that. Three.js still sees
 * `gl.constructor.name === 'WebGL2RenderingContext'` and
 * `gl instanceof WebGL2RenderingContext` works.
 */
export class WebGL2RenderingContext {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		const canvas: Screen = arguments[1];
		const ctx = proto($.webgl2ContextNew(canvas), WebGL2RenderingContext);
		_w2.set(ctx, { canvas });
		return ctx;
	}

	// --- WebGL 2 constants (defined on the v2 prototype only) -------------

	declare readonly READ_BUFFER: number;
	declare readonly UNPACK_ROW_LENGTH: number;
	declare readonly UNPACK_SKIP_ROWS: number;
	declare readonly UNPACK_SKIP_PIXELS: number;
	declare readonly PACK_ROW_LENGTH: number;
	declare readonly PACK_SKIP_ROWS: number;
	declare readonly PACK_SKIP_PIXELS: number;
	declare readonly COLOR: number;
	declare readonly DEPTH: number;
	declare readonly STENCIL: number;
	declare readonly RED: number;
	declare readonly RGB8: number;
	declare readonly RGBA8: number;
	declare readonly RGB10_A2: number;
	declare readonly TEXTURE_BINDING_3D: number;
	declare readonly UNPACK_SKIP_IMAGES: number;
	declare readonly UNPACK_IMAGE_HEIGHT: number;
	declare readonly TEXTURE_3D: number;
	declare readonly TEXTURE_WRAP_R: number;
	declare readonly MAX_3D_TEXTURE_SIZE: number;
	declare readonly UNSIGNED_INT_2_10_10_10_REV: number;
	declare readonly MAX_ELEMENTS_VERTICES: number;
	declare readonly MAX_ELEMENTS_INDICES: number;
	declare readonly TEXTURE_MIN_LOD: number;
	declare readonly TEXTURE_MAX_LOD: number;
	declare readonly TEXTURE_BASE_LEVEL: number;
	declare readonly TEXTURE_MAX_LEVEL: number;
	declare readonly MIN: number;
	declare readonly MAX: number;
	declare readonly DEPTH_COMPONENT24: number;
	declare readonly MAX_TEXTURE_LOD_BIAS: number;
	declare readonly TEXTURE_COMPARE_MODE: number;
	declare readonly TEXTURE_COMPARE_FUNC: number;
	declare readonly CURRENT_QUERY: number;
	declare readonly QUERY_RESULT: number;
	declare readonly QUERY_RESULT_AVAILABLE: number;
	declare readonly STREAM_READ: number;
	declare readonly STREAM_COPY: number;
	declare readonly STATIC_READ: number;
	declare readonly STATIC_COPY: number;
	declare readonly DYNAMIC_READ: number;
	declare readonly DYNAMIC_COPY: number;
	declare readonly MAX_DRAW_BUFFERS: number;
	declare readonly DRAW_BUFFER0: number;
	declare readonly DRAW_BUFFER1: number;
	declare readonly DRAW_BUFFER2: number;
	declare readonly DRAW_BUFFER3: number;
	declare readonly DRAW_BUFFER4: number;
	declare readonly DRAW_BUFFER5: number;
	declare readonly DRAW_BUFFER6: number;
	declare readonly DRAW_BUFFER7: number;
	declare readonly DRAW_BUFFER8: number;
	declare readonly DRAW_BUFFER9: number;
	declare readonly DRAW_BUFFER10: number;
	declare readonly DRAW_BUFFER11: number;
	declare readonly DRAW_BUFFER12: number;
	declare readonly DRAW_BUFFER13: number;
	declare readonly DRAW_BUFFER14: number;
	declare readonly DRAW_BUFFER15: number;
	declare readonly MAX_FRAGMENT_UNIFORM_COMPONENTS: number;
	declare readonly MAX_VERTEX_UNIFORM_COMPONENTS: number;
	declare readonly SAMPLER_3D: number;
	declare readonly SAMPLER_2D_SHADOW: number;
	declare readonly FRAGMENT_SHADER_DERIVATIVE_HINT: number;
	declare readonly PIXEL_PACK_BUFFER: number;
	declare readonly PIXEL_UNPACK_BUFFER: number;
	declare readonly PIXEL_PACK_BUFFER_BINDING: number;
	declare readonly PIXEL_UNPACK_BUFFER_BINDING: number;
	declare readonly FLOAT_MAT2x3: number;
	declare readonly FLOAT_MAT2x4: number;
	declare readonly FLOAT_MAT3x2: number;
	declare readonly FLOAT_MAT3x4: number;
	declare readonly FLOAT_MAT4x2: number;
	declare readonly FLOAT_MAT4x3: number;
	declare readonly SRGB: number;
	declare readonly SRGB8: number;
	declare readonly SRGB8_ALPHA8: number;
	declare readonly COMPARE_REF_TO_TEXTURE: number;
	declare readonly RGBA32F: number;
	declare readonly RGB32F: number;
	declare readonly RGBA16F: number;
	declare readonly RGB16F: number;
	declare readonly VERTEX_ATTRIB_ARRAY_INTEGER: number;
	declare readonly MAX_ARRAY_TEXTURE_LAYERS: number;
	declare readonly MIN_PROGRAM_TEXEL_OFFSET: number;
	declare readonly MAX_PROGRAM_TEXEL_OFFSET: number;
	declare readonly MAX_VARYING_COMPONENTS: number;
	declare readonly TEXTURE_2D_ARRAY: number;
	declare readonly TEXTURE_BINDING_2D_ARRAY: number;
	declare readonly R11F_G11F_B10F: number;
	declare readonly UNSIGNED_INT_10F_11F_11F_REV: number;
	declare readonly RGB9_E5: number;
	declare readonly UNSIGNED_INT_5_9_9_9_REV: number;
	declare readonly TRANSFORM_FEEDBACK_BUFFER_MODE: number;
	declare readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: number;
	declare readonly TRANSFORM_FEEDBACK_VARYINGS: number;
	declare readonly TRANSFORM_FEEDBACK_BUFFER_START: number;
	declare readonly TRANSFORM_FEEDBACK_BUFFER_SIZE: number;
	declare readonly TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: number;
	declare readonly RASTERIZER_DISCARD: number;
	declare readonly MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: number;
	declare readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: number;
	declare readonly INTERLEAVED_ATTRIBS: number;
	declare readonly SEPARATE_ATTRIBS: number;
	declare readonly TRANSFORM_FEEDBACK_BUFFER: number;
	declare readonly TRANSFORM_FEEDBACK_BUFFER_BINDING: number;
	declare readonly RGBA32UI: number;
	declare readonly RGB32UI: number;
	declare readonly RGBA16UI: number;
	declare readonly RGB16UI: number;
	declare readonly RGBA8UI: number;
	declare readonly RGB8UI: number;
	declare readonly RGBA32I: number;
	declare readonly RGB32I: number;
	declare readonly RGBA16I: number;
	declare readonly RGB16I: number;
	declare readonly RGBA8I: number;
	declare readonly RGB8I: number;
	declare readonly RED_INTEGER: number;
	declare readonly RGB_INTEGER: number;
	declare readonly RGBA_INTEGER: number;
	declare readonly SAMPLER_2D_ARRAY: number;
	declare readonly SAMPLER_2D_ARRAY_SHADOW: number;
	declare readonly SAMPLER_CUBE_SHADOW: number;
	declare readonly UNSIGNED_INT_VEC2: number;
	declare readonly UNSIGNED_INT_VEC3: number;
	declare readonly UNSIGNED_INT_VEC4: number;
	declare readonly INT_SAMPLER_2D: number;
	declare readonly INT_SAMPLER_3D: number;
	declare readonly INT_SAMPLER_CUBE: number;
	declare readonly INT_SAMPLER_2D_ARRAY: number;
	declare readonly UNSIGNED_INT_SAMPLER_2D: number;
	declare readonly UNSIGNED_INT_SAMPLER_3D: number;
	declare readonly UNSIGNED_INT_SAMPLER_CUBE: number;
	declare readonly UNSIGNED_INT_SAMPLER_2D_ARRAY: number;
	declare readonly DEPTH_COMPONENT32F: number;
	declare readonly DEPTH32F_STENCIL8: number;
	declare readonly FLOAT_32_UNSIGNED_INT_24_8_REV: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_RED_SIZE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: number;
	declare readonly FRAMEBUFFER_DEFAULT: number;
	declare readonly UNSIGNED_INT_24_8: number;
	declare readonly DEPTH24_STENCIL8: number;
	declare readonly UNSIGNED_NORMALIZED: number;
	declare readonly DRAW_FRAMEBUFFER_BINDING: number;
	declare readonly READ_FRAMEBUFFER: number;
	declare readonly DRAW_FRAMEBUFFER: number;
	declare readonly READ_FRAMEBUFFER_BINDING: number;
	declare readonly RENDERBUFFER_SAMPLES: number;
	declare readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: number;
	declare readonly MAX_COLOR_ATTACHMENTS: number;
	declare readonly COLOR_ATTACHMENT1: number;
	declare readonly COLOR_ATTACHMENT2: number;
	declare readonly COLOR_ATTACHMENT3: number;
	declare readonly COLOR_ATTACHMENT4: number;
	declare readonly COLOR_ATTACHMENT5: number;
	declare readonly COLOR_ATTACHMENT6: number;
	declare readonly COLOR_ATTACHMENT7: number;
	declare readonly COLOR_ATTACHMENT8: number;
	declare readonly COLOR_ATTACHMENT9: number;
	declare readonly COLOR_ATTACHMENT10: number;
	declare readonly COLOR_ATTACHMENT11: number;
	declare readonly COLOR_ATTACHMENT12: number;
	declare readonly COLOR_ATTACHMENT13: number;
	declare readonly COLOR_ATTACHMENT14: number;
	declare readonly COLOR_ATTACHMENT15: number;
	declare readonly FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: number;
	declare readonly MAX_SAMPLES: number;
	declare readonly HALF_FLOAT: number;
	declare readonly RG: number;
	declare readonly RG_INTEGER: number;
	declare readonly R8: number;
	declare readonly RG8: number;
	declare readonly R16F: number;
	declare readonly R32F: number;
	declare readonly RG16F: number;
	declare readonly RG32F: number;
	declare readonly R8I: number;
	declare readonly R8UI: number;
	declare readonly R16I: number;
	declare readonly R16UI: number;
	declare readonly R32I: number;
	declare readonly R32UI: number;
	declare readonly RG8I: number;
	declare readonly RG8UI: number;
	declare readonly RG16I: number;
	declare readonly RG16UI: number;
	declare readonly RG32I: number;
	declare readonly RG32UI: number;
	declare readonly VERTEX_ARRAY_BINDING: number;
	declare readonly R8_SNORM: number;
	declare readonly RG8_SNORM: number;
	declare readonly RGB8_SNORM: number;
	declare readonly RGBA8_SNORM: number;
	declare readonly SIGNED_NORMALIZED: number;
	declare readonly COPY_READ_BUFFER: number;
	declare readonly COPY_WRITE_BUFFER: number;
	declare readonly COPY_READ_BUFFER_BINDING: number;
	declare readonly COPY_WRITE_BUFFER_BINDING: number;
	declare readonly UNIFORM_BUFFER: number;
	declare readonly UNIFORM_BUFFER_BINDING: number;
	declare readonly UNIFORM_BUFFER_START: number;
	declare readonly UNIFORM_BUFFER_SIZE: number;
	declare readonly MAX_VERTEX_UNIFORM_BLOCKS: number;
	declare readonly MAX_FRAGMENT_UNIFORM_BLOCKS: number;
	declare readonly MAX_COMBINED_UNIFORM_BLOCKS: number;
	declare readonly MAX_UNIFORM_BUFFER_BINDINGS: number;
	declare readonly MAX_UNIFORM_BLOCK_SIZE: number;
	declare readonly MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: number;
	declare readonly MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: number;
	declare readonly UNIFORM_BUFFER_OFFSET_ALIGNMENT: number;
	declare readonly ACTIVE_UNIFORM_BLOCKS: number;
	declare readonly UNIFORM_TYPE: number;
	declare readonly UNIFORM_SIZE: number;
	declare readonly UNIFORM_BLOCK_INDEX: number;
	declare readonly UNIFORM_OFFSET: number;
	declare readonly UNIFORM_ARRAY_STRIDE: number;
	declare readonly UNIFORM_MATRIX_STRIDE: number;
	declare readonly UNIFORM_IS_ROW_MAJOR: number;
	declare readonly UNIFORM_BLOCK_BINDING: number;
	declare readonly UNIFORM_BLOCK_DATA_SIZE: number;
	declare readonly UNIFORM_BLOCK_ACTIVE_UNIFORMS: number;
	declare readonly UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: number;
	declare readonly UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: number;
	declare readonly UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: number;
	declare readonly INVALID_INDEX: number;
	declare readonly MAX_VERTEX_OUTPUT_COMPONENTS: number;
	declare readonly MAX_FRAGMENT_INPUT_COMPONENTS: number;
	declare readonly MAX_SERVER_WAIT_TIMEOUT: number;
	declare readonly OBJECT_TYPE: number;
	declare readonly SYNC_CONDITION: number;
	declare readonly SYNC_STATUS: number;
	declare readonly SYNC_FLAGS: number;
	declare readonly SYNC_FENCE: number;
	declare readonly SYNC_GPU_COMMANDS_COMPLETE: number;
	declare readonly UNSIGNALED: number;
	declare readonly SIGNALED: number;
	declare readonly ALREADY_SIGNALED: number;
	declare readonly TIMEOUT_EXPIRED: number;
	declare readonly CONDITION_SATISFIED: number;
	declare readonly WAIT_FAILED: number;
	declare readonly SYNC_FLUSH_COMMANDS_BIT: number;
	declare readonly VERTEX_ATTRIB_ARRAY_DIVISOR: number;
	declare readonly ANY_SAMPLES_PASSED: number;
	declare readonly ANY_SAMPLES_PASSED_CONSERVATIVE: number;
	declare readonly SAMPLER_BINDING: number;
	declare readonly RGB10_A2UI: number;
	declare readonly INT_2_10_10_10_REV: number;
	declare readonly TRANSFORM_FEEDBACK: number;
	declare readonly TRANSFORM_FEEDBACK_PAUSED: number;
	declare readonly TRANSFORM_FEEDBACK_ACTIVE: number;
	declare readonly TRANSFORM_FEEDBACK_BINDING: number;
	declare readonly TEXTURE_IMMUTABLE_FORMAT: number;
	declare readonly MAX_ELEMENT_INDEX: number;
	declare readonly TEXTURE_IMMUTABLE_LEVELS: number;
	declare readonly TIMEOUT_IGNORED: number;
	declare readonly MAX_CLIENT_WAIT_TIMEOUT_WEBGL: number;

	// --- WebGL 2-only methods (all native-backed; stubs typed here) ------

	// Vertex array objects
	createVertexArray(): WebGLVertexArrayObject | null { stub(); }
	deleteVertexArray(vao: WebGLVertexArrayObject | null): void { stub(); }
	isVertexArray(vao: WebGLVertexArrayObject | null): boolean { stub(); }
	bindVertexArray(vao: WebGLVertexArrayObject | null): void { stub(); }

	// Native instancing (non-extension)
	drawArraysInstanced(mode: number, first: number, count: number, instanceCount: number): void { stub(); }
	drawElementsInstanced(mode: number, count: number, type: number, offset: number, instanceCount: number): void { stub(); }
	vertexAttribDivisor(index: number, divisor: number): void { stub(); }

	// Integer vertex attributes
	vertexAttribIPointer(index: number, size: number, type: number, stride: number, offset: number): void { stub(); }
	vertexAttribI4i(index: number, x: number, y: number, z: number, w: number): void { stub(); }
	vertexAttribI4ui(index: number, x: number, y: number, z: number, w: number): void { stub(); }
	vertexAttribI4iv(index: number, values: Int32Array | number[]): void { stub(); }
	vertexAttribI4uiv(index: number, values: Uint32Array | number[]): void { stub(); }

	// Unsigned-integer uniforms
	uniform1ui(location: WebGLUniformLocation | null, v0: number): void { stub(); }
	uniform2ui(location: WebGLUniformLocation | null, v0: number, v1: number): void { stub(); }
	uniform3ui(location: WebGLUniformLocation | null, v0: number, v1: number, v2: number): void { stub(); }
	uniform4ui(location: WebGLUniformLocation | null, v0: number, v1: number, v2: number, v3: number): void { stub(); }
	uniform1uiv(location: WebGLUniformLocation | null, values: Uint32Array | number[]): void { stub(); }
	uniform2uiv(location: WebGLUniformLocation | null, values: Uint32Array | number[]): void { stub(); }
	uniform3uiv(location: WebGLUniformLocation | null, values: Uint32Array | number[]): void { stub(); }
	uniform4uiv(location: WebGLUniformLocation | null, values: Uint32Array | number[]): void { stub(); }

	// Non-square matrix uniform setters
	uniformMatrix2x3fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }
	uniformMatrix3x2fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }
	uniformMatrix2x4fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }
	uniformMatrix4x2fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }
	uniformMatrix3x4fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }
	uniformMatrix4x3fv(location: WebGLUniformLocation | null, transpose: boolean, value: Float32Array | number[]): void { stub(); }

	// Multi-render targets / FBO ops
	drawBuffers(buffers: number[]): void { stub(); }
	invalidateFramebuffer(target: number, attachments: number[]): void { stub(); }
	invalidateSubFramebuffer(target: number, attachments: number[], x: number, y: number, width: number, height: number): void { stub(); }
	blitFramebuffer(srcX0: number, srcY0: number, srcX1: number, srcY1: number, dstX0: number, dstY0: number, dstX1: number, dstY1: number, mask: number, filter: number): void { stub(); }
	readBuffer(src: number): void { stub(); }
	renderbufferStorageMultisample(target: number, samples: number, internalformat: number, width: number, height: number): void { stub(); }
	framebufferTextureLayer(target: number, attachment: number, texture: WebGLTexture | null, level: number, layer: number): void { stub(); }
	getInternalformatParameter(target: number, internalformat: number, pname: number): Int32Array | null { stub(); }
	getFragDataLocation(program: WebGLProgram, name: string): number { stub(); }

	// 3D / 2D-array texture upload + immutable storage
	texImage3D(target: number, level: number, internalformat: number, width: number, height: number, depth: number, border: number, format: number, type: number, pixels: ArrayBufferView | ArrayBuffer | null): void { stub(); }
	texSubImage3D(target: number, level: number, xoffset: number, yoffset: number, zoffset: number, width: number, height: number, depth: number, format: number, type: number, pixels: ArrayBufferView | ArrayBuffer | null): void { stub(); }
	copyTexSubImage3D(target: number, level: number, xoffset: number, yoffset: number, zoffset: number, x: number, y: number, width: number, height: number): void { stub(); }
	compressedTexImage3D(target: number, level: number, internalformat: number, width: number, height: number, depth: number, border: number, srcData: ArrayBufferView | null): void { stub(); }
	compressedTexSubImage3D(target: number, level: number, xoffset: number, yoffset: number, zoffset: number, width: number, height: number, depth: number, format: number, srcData: ArrayBufferView | null): void { stub(); }
	texStorage2D(target: number, levels: number, internalformat: number, width: number, height: number): void { stub(); }
	texStorage3D(target: number, levels: number, internalformat: number, width: number, height: number, depth: number): void { stub(); }

	// clearBuffer family
	clearBufferiv(buffer: number, drawbuffer: number, values: Int32Array | number[]): void { stub(); }
	clearBufferuiv(buffer: number, drawbuffer: number, values: Uint32Array | number[]): void { stub(); }
	clearBufferfv(buffer: number, drawbuffer: number, values: Float32Array | number[]): void { stub(); }
	clearBufferfi(buffer: number, drawbuffer: number, depth: number, stencil: number): void { stub(); }

	// Buffer copy / readback
	copyBufferSubData(readTarget: number, writeTarget: number, readOffset: number, writeOffset: number, size: number): void { stub(); }
	getBufferSubData(target: number, srcByteOffset: number, dstBuffer: ArrayBufferView, dstOffset?: number, length?: number): void { stub(); }

	// Uniform-buffer objects
	bindBufferBase(target: number, index: number, buffer: WebGLBuffer | null): void { stub(); }
	bindBufferRange(target: number, index: number, buffer: WebGLBuffer | null, offset: number, size: number): void { stub(); }
	getUniformIndices(program: WebGLProgram, uniformNames: string[]): number[] | null { stub(); }
	getActiveUniforms(program: WebGLProgram, uniformIndices: number[], pname: number): unknown { stub(); }
	getUniformBlockIndex(program: WebGLProgram, uniformBlockName: string): number { stub(); }
	getActiveUniformBlockParameter(program: WebGLProgram, uniformBlockIndex: number, pname: number): unknown { stub(); }
	getActiveUniformBlockName(program: WebGLProgram, uniformBlockIndex: number): string | null { stub(); }
	uniformBlockBinding(program: WebGLProgram, uniformBlockIndex: number, uniformBlockBinding: number): void { stub(); }
	getIndexedParameter(target: number, index: number): unknown { stub(); }

	// Sampler objects
	createSampler(): WebGLSampler | null { stub(); }
	deleteSampler(sampler: WebGLSampler | null): void { stub(); }
	isSampler(sampler: WebGLSampler | null): boolean { stub(); }
	bindSampler(unit: number, sampler: WebGLSampler | null): void { stub(); }
	samplerParameteri(sampler: WebGLSampler, pname: number, param: number): void { stub(); }
	samplerParameterf(sampler: WebGLSampler, pname: number, param: number): void { stub(); }
	getSamplerParameter(sampler: WebGLSampler, pname: number): unknown { stub(); }

	// Sync objects
	fenceSync(condition: number, flags: number): WebGLSync | null { stub(); }
	isSync(sync: WebGLSync | null): boolean { stub(); }
	deleteSync(sync: WebGLSync | null): void { stub(); }
	clientWaitSync(sync: WebGLSync, flags: number, timeout: number): number { stub(); }
	waitSync(sync: WebGLSync, flags: number, timeout: number): void { stub(); }
	getSyncParameter(sync: WebGLSync, pname: number): unknown { stub(); }

	// Query objects
	createQuery(): WebGLQuery | null { stub(); }
	deleteQuery(query: WebGLQuery | null): void { stub(); }
	isQuery(query: WebGLQuery | null): boolean { stub(); }
	beginQuery(target: number, query: WebGLQuery): void { stub(); }
	endQuery(target: number): void { stub(); }
	getQuery(target: number, pname: number): WebGLQuery | null { stub(); }
	getQueryParameter(query: WebGLQuery, pname: number): unknown { stub(); }

	// Transform feedback
	createTransformFeedback(): WebGLTransformFeedback | null { stub(); }
	deleteTransformFeedback(tf: WebGLTransformFeedback | null): void { stub(); }
	isTransformFeedback(tf: WebGLTransformFeedback | null): boolean { stub(); }
	bindTransformFeedback(target: number, tf: WebGLTransformFeedback | null): void { stub(); }
	beginTransformFeedback(primitiveMode: number): void { stub(); }
	endTransformFeedback(): void { stub(); }
	transformFeedbackVaryings(program: WebGLProgram, varyings: string[], bufferMode: number): void { stub(); }
	getTransformFeedbackVarying(program: WebGLProgram, index: number): WebGLActiveInfo | null { stub(); }
	pauseTransformFeedback(): void { stub(); }
	resumeTransformFeedback(): void { stub(); }
}

interface WebGL2RenderingContextInternal {
	canvas: Screen;
}
const _w2 = createInternal<WebGL2RenderingContext, WebGL2RenderingContextInternal>();

// Wire the prototype chain so instances inherit every WebGL 1 method and
// constant from WebGLRenderingContext, AND so
// `gl instanceof WebGLRenderingContext` evaluates true (Three.js relies on
// this via the `gl instanceof WebGL2RenderingContext` style check, but
// also via the parent-class chain for cross-version helpers).
Object.setPrototypeOf(
	WebGL2RenderingContext.prototype,
	WebGLRenderingContext.prototype,
);
Object.setPrototypeOf(WebGL2RenderingContext, WebGLRenderingContext);

$.webgl2ContextInitClass(WebGL2RenderingContext);
def(WebGL2RenderingContext);
