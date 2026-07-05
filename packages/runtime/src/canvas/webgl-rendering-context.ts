/**
 * WebGL 1 rendering context — Phase 2.C addition for the V8 migration.
 *
 * Companion to {@link WebGL2RenderingContext} for the inline-canvas WebGL 1
 * path that brewser-runtime + Three.js feature-detect against. The two
 * classes share the SAME helper-handle classes (WebGLBuffer / WebGLProgram /
 * etc.) — imported from `webgl2-rendering-context.ts` so `instanceof
 * WebGLBuffer` works regardless of which context produced the handle.
 *
 * The native methods are installed on the prototype by `$.webglInitClass()`
 * at runtime (see `source/webgl.cc` → `install_methods()`); the merged
 * interface below declares them as types-only so TypeScript callers see the
 * right signatures without paying for ~80 throwaway stub function objects
 * at boot.
 *
 * NOTE for 2.D+: the method list below is what the slice demo (geometry-cube)
 * empirically needs. Methods Three.js calls that the engine doesn't yet
 * implement will surface as `TypeError: foo is not a function` (or as
 * `UNDEFINED gl.foo` lines from the diagnostic Proxy in webgl-shim.ts).
 * That's the signal to add them in the next iteration.
 */
import { $ } from '../$';
import { def, proto, createInternal } from '../utils';
import { ImageBitmap } from './image-bitmap';
import { ImageData } from './image-data';
import { OffscreenCanvas } from './offscreen-canvas';
import {
	WebGLBuffer,
	WebGLFramebuffer,
	WebGLProgram,
	WebGLRenderbuffer,
	WebGLShader,
	WebGLTexture,
	WebGLUniformLocation,
	WebGLActiveInfo,
	WebGLShaderPrecisionFormat,
	type GLenum,
	type GLbitfield,
	type GLboolean,
	type GLint,
	type GLsizei,
	type GLintptr,
	type GLuint,
	type GLfloat,
	type GLclampf,
	type Float32List,
	type Int32List,
	type WebGLContextAttributes,
} from './webgl2-rendering-context';
import type { Screen } from '../screen';

const ILLEGAL = () => new TypeError('Illegal constructor');

const _ = createInternal<WebGLRenderingContext, { canvas: Screen }>();

// WebGL 1 numeric constants. Subset of WebGL 2's — only the ones a WebGL 1
// caller (Three.js + the demo) actually touches. Matches MDN's
// WebGLRenderingContext static constants page exactly so callers using
// gl.RGBA / gl.UNSIGNED_BYTE / etc. work without surprises.
//
// The full WebGL 2 constants live on WebGL2RenderingContext; if Three.js
// detects v1 context here, it won't touch v2-only enums.
const GL_CONSTANTS = {
	// ClearBufferMask
	DEPTH_BUFFER_BIT:               0x00000100,
	STENCIL_BUFFER_BIT:             0x00000400,
	COLOR_BUFFER_BIT:               0x00004000,
	// BeginMode
	POINTS:                         0x0000,
	LINES:                          0x0001,
	LINE_LOOP:                      0x0002,
	LINE_STRIP:                     0x0003,
	TRIANGLES:                      0x0004,
	TRIANGLE_STRIP:                 0x0005,
	TRIANGLE_FAN:                   0x0006,
	// BlendingFactorDest
	ZERO:                           0,
	ONE:                            1,
	SRC_COLOR:                      0x0300,
	ONE_MINUS_SRC_COLOR:            0x0301,
	SRC_ALPHA:                      0x0302,
	ONE_MINUS_SRC_ALPHA:            0x0303,
	DST_ALPHA:                      0x0304,
	ONE_MINUS_DST_ALPHA:            0x0305,
	// BlendingFactorSrc
	DST_COLOR:                      0x0306,
	ONE_MINUS_DST_COLOR:            0x0307,
	SRC_ALPHA_SATURATE:             0x0308,
	// BlendEquationSeparate
	FUNC_ADD:                       0x8006,
	BLEND_EQUATION:                 0x8009,
	BLEND_EQUATION_RGB:             0x8009,
	BLEND_EQUATION_ALPHA:           0x883D,
	FUNC_SUBTRACT:                  0x800A,
	FUNC_REVERSE_SUBTRACT:          0x800B,
	BLEND_DST_RGB:                  0x80C8,
	BLEND_SRC_RGB:                  0x80C9,
	BLEND_DST_ALPHA:                0x80CA,
	BLEND_SRC_ALPHA:                0x80CB,
	CONSTANT_COLOR:                 0x8001,
	ONE_MINUS_CONSTANT_COLOR:       0x8002,
	CONSTANT_ALPHA:                 0x8003,
	ONE_MINUS_CONSTANT_ALPHA:       0x8004,
	BLEND_COLOR:                    0x8005,
	// Buffer Objects
	ARRAY_BUFFER:                   0x8892,
	ELEMENT_ARRAY_BUFFER:           0x8893,
	ARRAY_BUFFER_BINDING:           0x8894,
	ELEMENT_ARRAY_BUFFER_BINDING:   0x8895,
	STREAM_DRAW:                    0x88E0,
	STATIC_DRAW:                    0x88E4,
	DYNAMIC_DRAW:                   0x88E8,
	BUFFER_SIZE:                    0x8764,
	BUFFER_USAGE:                   0x8765,
	CURRENT_VERTEX_ATTRIB:          0x8626,
	// CullFaceMode
	FRONT:                          0x0404,
	BACK:                           0x0405,
	FRONT_AND_BACK:                 0x0408,
	// EnableCap
	TEXTURE_2D:                     0x0DE1,
	CULL_FACE:                      0x0B44,
	BLEND:                          0x0BE2,
	DITHER:                         0x0BD0,
	STENCIL_TEST:                   0x0B90,
	DEPTH_TEST:                     0x0B71,
	SCISSOR_TEST:                   0x0C11,
	POLYGON_OFFSET_FILL:            0x8037,
	SAMPLE_ALPHA_TO_COVERAGE:       0x809E,
	SAMPLE_COVERAGE:                0x80A0,
	// ErrorCode
	NO_ERROR:                       0,
	INVALID_ENUM:                   0x0500,
	INVALID_VALUE:                  0x0501,
	INVALID_OPERATION:              0x0502,
	OUT_OF_MEMORY:                  0x0505,
	CONTEXT_LOST_WEBGL:             0x9242,
	// FrontFaceDirection
	CW:                             0x0900,
	CCW:                            0x0901,
	// GetPName
	LINE_WIDTH:                     0x0B21,
	ALIASED_POINT_SIZE_RANGE:       0x846D,
	ALIASED_LINE_WIDTH_RANGE:       0x846E,
	CULL_FACE_MODE:                 0x0B45,
	FRONT_FACE:                     0x0B46,
	DEPTH_RANGE:                    0x0B70,
	DEPTH_WRITEMASK:                0x0B72,
	DEPTH_CLEAR_VALUE:              0x0B73,
	DEPTH_FUNC:                     0x0B74,
	STENCIL_CLEAR_VALUE:            0x0B91,
	STENCIL_FUNC:                   0x0B92,
	STENCIL_FAIL:                   0x0B94,
	STENCIL_PASS_DEPTH_FAIL:        0x0B95,
	STENCIL_PASS_DEPTH_PASS:        0x0B96,
	STENCIL_REF:                    0x0B97,
	STENCIL_VALUE_MASK:             0x0B93,
	STENCIL_WRITEMASK:              0x0B98,
	STENCIL_BACK_FUNC:              0x8800,
	STENCIL_BACK_FAIL:              0x8801,
	STENCIL_BACK_PASS_DEPTH_FAIL:   0x8802,
	STENCIL_BACK_PASS_DEPTH_PASS:   0x8803,
	STENCIL_BACK_REF:               0x8CA3,
	STENCIL_BACK_VALUE_MASK:        0x8CA4,
	STENCIL_BACK_WRITEMASK:         0x8CA5,
	VIEWPORT:                       0x0BA2,
	SCISSOR_BOX:                    0x0C10,
	COLOR_CLEAR_VALUE:              0x0C22,
	COLOR_WRITEMASK:                0x0C23,
	UNPACK_ALIGNMENT:               0x0CF5,
	PACK_ALIGNMENT:                 0x0D05,
	MAX_TEXTURE_SIZE:               0x0D33,
	MAX_VIEWPORT_DIMS:              0x0D3A,
	SUBPIXEL_BITS:                  0x0D50,
	RED_BITS:                       0x0D52,
	GREEN_BITS:                     0x0D53,
	BLUE_BITS:                      0x0D54,
	ALPHA_BITS:                     0x0D55,
	DEPTH_BITS:                     0x0D56,
	STENCIL_BITS:                   0x0D57,
	POLYGON_OFFSET_UNITS:           0x2A00,
	POLYGON_OFFSET_FACTOR:          0x8038,
	TEXTURE_BINDING_2D:             0x8069,
	SAMPLE_BUFFERS:                 0x80A8,
	SAMPLES:                        0x80A9,
	SAMPLE_COVERAGE_VALUE:          0x80AA,
	SAMPLE_COVERAGE_INVERT:         0x80AB,
	// Compressed texture (just the enum; no real impl in 2.C)
	COMPRESSED_TEXTURE_FORMATS:     0x86A3,
	// HintMode
	DONT_CARE:                      0x1100,
	FASTEST:                        0x1101,
	NICEST:                         0x1102,
	GENERATE_MIPMAP_HINT:           0x8192,
	// DataType
	BYTE:                           0x1400,
	UNSIGNED_BYTE:                  0x1401,
	SHORT:                          0x1402,
	UNSIGNED_SHORT:                 0x1403,
	INT:                            0x1404,
	UNSIGNED_INT:                   0x1405,
	FLOAT:                          0x1406,
	// PixelFormat
	DEPTH_COMPONENT:                0x1902,
	ALPHA:                          0x1906,
	RGB:                            0x1907,
	RGBA:                           0x1908,
	LUMINANCE:                      0x1909,
	LUMINANCE_ALPHA:                0x190A,
	// PixelType
	UNSIGNED_SHORT_4_4_4_4:         0x8033,
	UNSIGNED_SHORT_5_5_5_1:         0x8034,
	UNSIGNED_SHORT_5_6_5:           0x8363,
	// Shaders
	FRAGMENT_SHADER:                0x8B30,
	VERTEX_SHADER:                  0x8B31,
	COMPILE_STATUS:                 0x8B81,
	DELETE_STATUS:                  0x8B80,
	LINK_STATUS:                    0x8B82,
	VALIDATE_STATUS:                0x8B83,
	ATTACHED_SHADERS:               0x8B85,
	ACTIVE_ATTRIBUTES:              0x8B89,
	ACTIVE_UNIFORMS:                0x8B86,
	MAX_VERTEX_ATTRIBS:             0x8869,
	MAX_VERTEX_UNIFORM_VECTORS:     0x8DFB,
	MAX_VARYING_VECTORS:            0x8DFC,
	MAX_COMBINED_TEXTURE_IMAGE_UNITS: 0x8B4D,
	MAX_VERTEX_TEXTURE_IMAGE_UNITS:   0x8B4C,
	MAX_TEXTURE_IMAGE_UNITS:          0x8872,
	MAX_FRAGMENT_UNIFORM_VECTORS:     0x8DFD,
	SHADER_TYPE:                    0x8B4F,
	SHADING_LANGUAGE_VERSION:       0x8B8C,
	CURRENT_PROGRAM:                0x8B8D,
	// Stencil ops
	NEVER:                          0x0200,
	LESS:                           0x0201,
	EQUAL:                          0x0202,
	LEQUAL:                         0x0203,
	GREATER:                        0x0204,
	NOTEQUAL:                       0x0205,
	GEQUAL:                         0x0206,
	ALWAYS:                         0x0207,
	KEEP:                           0x1E00,
	REPLACE:                        0x1E01,
	INCR:                           0x1E02,
	DECR:                           0x1E03,
	INVERT:                         0x150A,
	INCR_WRAP:                      0x8507,
	DECR_WRAP:                      0x8508,
	// StringName
	VENDOR:                         0x1F00,
	RENDERER:                       0x1F01,
	VERSION:                        0x1F02,
	// Texture filter / wrap
	NEAREST:                        0x2600,
	LINEAR:                         0x2601,
	NEAREST_MIPMAP_NEAREST:         0x2700,
	LINEAR_MIPMAP_NEAREST:          0x2701,
	NEAREST_MIPMAP_LINEAR:          0x2702,
	LINEAR_MIPMAP_LINEAR:           0x2703,
	TEXTURE_MAG_FILTER:             0x2800,
	TEXTURE_MIN_FILTER:             0x2801,
	TEXTURE_WRAP_S:                 0x2802,
	TEXTURE_WRAP_T:                 0x2803,
	TEXTURE:                        0x1702,
	TEXTURE_CUBE_MAP:               0x8513,
	TEXTURE_BINDING_CUBE_MAP:       0x8514,
	TEXTURE_CUBE_MAP_POSITIVE_X:    0x8515,
	TEXTURE_CUBE_MAP_NEGATIVE_X:    0x8516,
	TEXTURE_CUBE_MAP_POSITIVE_Y:    0x8517,
	TEXTURE_CUBE_MAP_NEGATIVE_Y:    0x8518,
	TEXTURE_CUBE_MAP_POSITIVE_Z:    0x8519,
	TEXTURE_CUBE_MAP_NEGATIVE_Z:    0x851A,
	MAX_CUBE_MAP_TEXTURE_SIZE:      0x851C,
	TEXTURE0:                       0x84C0,
	TEXTURE1:                       0x84C1,
	TEXTURE2:                       0x84C2,
	TEXTURE3:                       0x84C3,
	TEXTURE4:                       0x84C4,
	TEXTURE5:                       0x84C5,
	TEXTURE6:                       0x84C6,
	TEXTURE7:                       0x84C7,
	ACTIVE_TEXTURE:                 0x84E0,
	REPEAT:                         0x2901,
	CLAMP_TO_EDGE:                  0x812F,
	MIRRORED_REPEAT:                0x8370,
	// Uniform types
	FLOAT_VEC2:                     0x8B50,
	FLOAT_VEC3:                     0x8B51,
	FLOAT_VEC4:                     0x8B52,
	INT_VEC2:                       0x8B53,
	INT_VEC3:                       0x8B54,
	INT_VEC4:                       0x8B55,
	BOOL:                           0x8B56,
	BOOL_VEC2:                      0x8B57,
	BOOL_VEC3:                      0x8B58,
	BOOL_VEC4:                      0x8B59,
	FLOAT_MAT2:                     0x8B5A,
	FLOAT_MAT3:                     0x8B5B,
	FLOAT_MAT4:                     0x8B5C,
	SAMPLER_2D:                     0x8B5E,
	SAMPLER_CUBE:                   0x8B60,
	// Vertex attrib pname
	VERTEX_ATTRIB_ARRAY_ENABLED:    0x8622,
	VERTEX_ATTRIB_ARRAY_SIZE:       0x8623,
	VERTEX_ATTRIB_ARRAY_STRIDE:     0x8624,
	VERTEX_ATTRIB_ARRAY_TYPE:       0x8625,
	VERTEX_ATTRIB_ARRAY_NORMALIZED: 0x886A,
	VERTEX_ATTRIB_ARRAY_POINTER:    0x8645,
	VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: 0x889F,
	// Read-format
	IMPLEMENTATION_COLOR_READ_TYPE:   0x8B9A,
	IMPLEMENTATION_COLOR_READ_FORMAT: 0x8B9B,
	// Shader precision format query (gl.getShaderPrecisionFormat).
	LOW_FLOAT:                      0x8DF0,
	MEDIUM_FLOAT:                   0x8DF1,
	HIGH_FLOAT:                     0x8DF2,
	LOW_INT:                        0x8DF3,
	MEDIUM_INT:                     0x8DF4,
	HIGH_INT:                       0x8DF5,
	// Framebuffer
	FRAMEBUFFER:                    0x8D40,
	RENDERBUFFER:                   0x8D41,
	RGBA4:                          0x8056,
	RGB5_A1:                        0x8057,
	RGB565:                         0x8D62,
	DEPTH_COMPONENT16:              0x81A5,
	STENCIL_INDEX8:                 0x8D48,
	DEPTH_STENCIL:                  0x84F9,
	RENDERBUFFER_WIDTH:             0x8D42,
	RENDERBUFFER_HEIGHT:            0x8D43,
	RENDERBUFFER_INTERNAL_FORMAT:   0x8D44,
	RENDERBUFFER_RED_SIZE:          0x8D50,
	RENDERBUFFER_GREEN_SIZE:        0x8D51,
	RENDERBUFFER_BLUE_SIZE:         0x8D52,
	RENDERBUFFER_ALPHA_SIZE:        0x8D53,
	RENDERBUFFER_DEPTH_SIZE:        0x8D54,
	RENDERBUFFER_STENCIL_SIZE:      0x8D55,
	FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:           0x8CD0,
	FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:           0x8CD1,
	FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:         0x8CD2,
	FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: 0x8CD3,
	COLOR_ATTACHMENT0:              0x8CE0,
	DEPTH_ATTACHMENT:               0x8D00,
	STENCIL_ATTACHMENT:             0x8D20,
	DEPTH_STENCIL_ATTACHMENT:       0x821A,
	NONE:                           0,
	FRAMEBUFFER_COMPLETE:                       0x8CD5,
	FRAMEBUFFER_INCOMPLETE_ATTACHMENT:          0x8CD6,
	FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:  0x8CD7,
	FRAMEBUFFER_INCOMPLETE_DIMENSIONS:          0x8CD9,
	FRAMEBUFFER_UNSUPPORTED:                    0x8CDD,
	FRAMEBUFFER_BINDING:                        0x8CA6,
	RENDERBUFFER_BINDING:                       0x8CA7,
	MAX_RENDERBUFFER_SIZE:                      0x84E8,
	INVALID_FRAMEBUFFER_OPERATION:              0x0506,
	// ES3-core sized internalformats — exposed on the v1 context (deliberately
	// past the WebGL1 spec surface) because post-r150 Three.js's
	// `getInternalFormat()` reads them DIRECTLY off the gl context to pick a
	// sized internalformat for texImage2D, regardless of whether the context
	// is v1 or v2:
	//
	//     internalFormat = (transfer===SRGBTransfer) ? _gl.SRGB8_ALPHA8 : _gl.RGBA8;
	//     if (glType === _gl.HALF_FLOAT) internalFormat = _gl.RGBA16F;
	//     ...
	//
	// If these aren't on the gl object, `_gl.SRGB8_ALPHA8` is `undefined`,
	// `internalFormat` becomes `undefined`, texImage2D gets called with
	// `internalformat=undefined` (coerces to 0/NaN), the engine passes 0
	// to glTexImage2D, GL returns INVALID_ENUM, the texture never lands,
	// and the sampler returns 0 → every SRGB-colorSpace texture renders
	// black. This block makes Three.js's call compute a real enum that
	// the GLES3 driver accepts as a sized internalformat. Values are GLES3
	// registry-canonical (matches Mesa's gl3.h and Khronos glcorearb.h).
	// See NXJS_PATCHES_NEEDED.md #9.
	SRGB8_ALPHA8:                       0x8C43,
	SRGB8:                              0x8C41,
	RGBA8:                              0x8058,
	RGB8:                               0x8051,
	RGBA16F:                            0x881A,
	RGB16F:                             0x881B,
	R8:                                 0x8229,
	RG8:                                0x822B,
	R16F:                               0x822D,
	RG16F:                              0x822F,
	R32F:                               0x822E,
	RG32F:                              0x8230,
	RGBA32F:                            0x8814,
	RGB32F:                             0x8815,
	// Pixel storage (WebGL-only)
	UNPACK_FLIP_Y_WEBGL:               0x9240,
	UNPACK_PREMULTIPLY_ALPHA_WEBGL:    0x9241,
	UNPACK_COLORSPACE_CONVERSION_WEBGL: 0x9243,
	BROWSER_DEFAULT_WEBGL:             0x9244,
};

/**
 * The WebGL 1 rendering context for an inline canvas (and for
 * `screen.getContext('webgl')`), backed by raw GLES3 dispatches into the 2.B
 * tenant offscreen FBO.
 *
 * Acquire via `canvas.getContext('webgl')` or
 * `canvas.getContext('experimental-webgl')`.
 *
 * @see https://developer.mozilla.org/docs/Web/API/WebGLRenderingContext
 */
export class WebGLRenderingContext {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}

	/** The {@link Screen | `screen`} canvas this context draws to. */
	get canvas(): Screen {
		return _(this).canvas;
	}

	/** Drawing buffer width in pixels (matches the tenant FBO size). */
	declare readonly drawingBufferWidth: GLsizei;

	/** Drawing buffer height in pixels (matches the tenant FBO size). */
	declare readonly drawingBufferHeight: GLsizei;
}

// Native method bindings — installed on the prototype by $.webglInitClass().
// Declared as merged-interface types to keep the JS class slim.
export interface WebGLRenderingContext {
	getContextAttributes(): WebGLContextAttributes;
	isContextLost(): boolean;
	getSupportedExtensions(): string[];
	getExtension(name: string): any;
	getParameter(pname: GLenum): any;
	getError(): GLenum;
	getShaderPrecisionFormat(shadertype: GLenum, precisiontype: GLenum): WebGLShaderPrecisionFormat | null;

	// State
	viewport(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
	scissor(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
	enable(cap: GLenum): void;
	disable(cap: GLenum): void;
	isEnabled(cap: GLenum): GLboolean;
	depthFunc(func: GLenum): void;
	depthMask(flag: GLboolean): void;
	depthRange(zNear: GLclampf, zFar: GLclampf): void;
	cullFace(mode: GLenum): void;
	frontFace(mode: GLenum): void;
	blendFunc(sfactor: GLenum, dfactor: GLenum): void;
	blendFuncSeparate(srcRGB: GLenum, dstRGB: GLenum, srcAlpha: GLenum, dstAlpha: GLenum): void;
	blendEquation(mode: GLenum): void;
	blendEquationSeparate(modeRGB: GLenum, modeAlpha: GLenum): void;
	blendColor(r: GLclampf, g: GLclampf, b: GLclampf, a: GLclampf): void;
	colorMask(r: GLboolean, g: GLboolean, b: GLboolean, a: GLboolean): void;
	stencilFunc(func: GLenum, ref: GLint, mask: GLuint): void;
	stencilFuncSeparate(face: GLenum, func: GLenum, ref: GLint, mask: GLuint): void;
	stencilOp(fail: GLenum, zfail: GLenum, zpass: GLenum): void;
	stencilOpSeparate(face: GLenum, fail: GLenum, zfail: GLenum, zpass: GLenum): void;
	stencilMask(mask: GLuint): void;
	stencilMaskSeparate(face: GLenum, mask: GLuint): void;
	polygonOffset(factor: GLfloat, units: GLfloat): void;
	sampleCoverage(value: GLclampf, invert: GLboolean): void;
	lineWidth(width: GLfloat): void;
	hint(target: GLenum, mode: GLenum): void;
	clear(mask: GLbitfield): void;
	clearColor(r: GLclampf, g: GLclampf, b: GLclampf, a: GLclampf): void;
	clearDepth(depth: GLclampf): void;
	clearStencil(s: GLint): void;
	finish(): void;
	flush(): void;
	pixelStorei(pname: GLenum, param: GLint | GLboolean): void;

	// Shader / program
	createShader(type: GLenum): WebGLShader | null;
	deleteShader(shader: WebGLShader | null): void;
	isShader(shader: WebGLShader | null): GLboolean;
	shaderSource(shader: WebGLShader, source: string): void;
	compileShader(shader: WebGLShader): void;
	getShaderParameter(shader: WebGLShader, pname: GLenum): any;
	getShaderInfoLog(shader: WebGLShader): string;
	getShaderSource(shader: WebGLShader): string;
	createProgram(): WebGLProgram | null;
	deleteProgram(program: WebGLProgram | null): void;
	isProgram(program: WebGLProgram | null): GLboolean;
	attachShader(program: WebGLProgram, shader: WebGLShader): void;
	detachShader(program: WebGLProgram, shader: WebGLShader): void;
	linkProgram(program: WebGLProgram): void;
	validateProgram(program: WebGLProgram): void;
	useProgram(program: WebGLProgram | null): void;
	getProgramParameter(program: WebGLProgram, pname: GLenum): any;
	getProgramInfoLog(program: WebGLProgram): string;
	getAttribLocation(program: WebGLProgram, name: string): GLint;
	getUniformLocation(program: WebGLProgram, name: string): WebGLUniformLocation | null;
	bindAttribLocation(program: WebGLProgram, index: GLuint, name: string): void;
	getActiveAttrib(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null;
	getActiveUniform(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null;

	// Buffer
	createBuffer(): WebGLBuffer | null;
	deleteBuffer(buffer: WebGLBuffer | null): void;
	isBuffer(buffer: WebGLBuffer | null): GLboolean;
	bindBuffer(target: GLenum, buffer: WebGLBuffer | null): void;
	bufferData(target: GLenum, size: GLsizei | ArrayBufferView | ArrayBuffer | null, usage: GLenum): void;
	bufferSubData(target: GLenum, offset: GLintptr, data: ArrayBufferView | ArrayBuffer): void;

	// Vertex attribs
	enableVertexAttribArray(index: GLuint): void;
	disableVertexAttribArray(index: GLuint): void;
	vertexAttribPointer(index: GLuint, size: GLint, type: GLenum, normalized: GLboolean, stride: GLsizei, offset: GLintptr): void;
	vertexAttrib1f(index: GLuint, x: GLfloat): void;
	vertexAttrib2f(index: GLuint, x: GLfloat, y: GLfloat): void;
	vertexAttrib3f(index: GLuint, x: GLfloat, y: GLfloat, z: GLfloat): void;
	vertexAttrib4f(index: GLuint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void;

	// Uniforms
	uniform1f(location: WebGLUniformLocation | null, x: GLfloat): void;
	uniform2f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat): void;
	uniform3f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat): void;
	uniform4f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void;
	uniform1i(location: WebGLUniformLocation | null, x: GLint): void;
	uniform2i(location: WebGLUniformLocation | null, x: GLint, y: GLint): void;
	uniform3i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint): void;
	uniform4i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint, w: GLint): void;
	uniform1fv(location: WebGLUniformLocation | null, value: Float32List): void;
	uniform2fv(location: WebGLUniformLocation | null, value: Float32List): void;
	uniform3fv(location: WebGLUniformLocation | null, value: Float32List): void;
	uniform4fv(location: WebGLUniformLocation | null, value: Float32List): void;
	uniform1iv(location: WebGLUniformLocation | null, value: Int32List): void;
	uniform2iv(location: WebGLUniformLocation | null, value: Int32List): void;
	uniform3iv(location: WebGLUniformLocation | null, value: Int32List): void;
	uniform4iv(location: WebGLUniformLocation | null, value: Int32List): void;
	uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;
	uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;
	uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;

	// Texture
	createTexture(): WebGLTexture | null;
	deleteTexture(texture: WebGLTexture | null): void;
	isTexture(texture: WebGLTexture | null): GLboolean;
	bindTexture(target: GLenum, texture: WebGLTexture | null): void;
	activeTexture(unit: GLenum): void;
	texParameteri(target: GLenum, pname: GLenum, param: GLint): void;
	texParameterf(target: GLenum, pname: GLenum, param: GLfloat): void;
	generateMipmap(target: GLenum): void;
	texImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;

	// Framebuffer / Renderbuffer
	createFramebuffer(): WebGLFramebuffer | null;
	deleteFramebuffer(framebuffer: WebGLFramebuffer | null): void;
	isFramebuffer(framebuffer: WebGLFramebuffer | null): GLboolean;
	bindFramebuffer(target: GLenum, framebuffer: WebGLFramebuffer | null): void;
	framebufferTexture2D(target: GLenum, attachment: GLenum, textarget: GLenum, texture: WebGLTexture | null, level: GLint): void;
	framebufferRenderbuffer(target: GLenum, attachment: GLenum, renderbuffertarget: GLenum, renderbuffer: WebGLRenderbuffer | null): void;
	checkFramebufferStatus(target: GLenum): GLenum;
	createRenderbuffer(): WebGLRenderbuffer | null;
	deleteRenderbuffer(renderbuffer: WebGLRenderbuffer | null): void;
	isRenderbuffer(renderbuffer: WebGLRenderbuffer | null): GLboolean;
	bindRenderbuffer(target: GLenum, renderbuffer: WebGLRenderbuffer | null): void;
	renderbufferStorage(target: GLenum, internalformat: GLenum, width: GLsizei, height: GLsizei): void;

	// Draw
	drawArrays(mode: GLenum, first: GLint, count: GLsizei): void;
	drawElements(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr): void;

	// Readback
	readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView): void;

	// brewser-specific hooks (canvas-runner expects these — no-op in V8 bridge).
	enableGpuBridgePrototype(enabled: boolean): boolean;
	setBridgeAutoFlush(enabled: boolean): boolean;
}

// Numeric GL constants live on both the class and every instance.
// Built as ONE bulk Object.defineProperties call per target (instead of
// per-key Object.defineProperty in a for-of-destructuring loop over
// Object.entries) to avoid a V8/aarch64 Tegra JIT codegen issue that
// crashed the engine when ~1160 such installs ran across the v1+v2
// module bodies — see NXJS_PATCHES_NEEDED.md #8.
export interface WebGLRenderingContext extends Readonly<typeof GL_CONSTANTS> {}
{
	const keys = Object.keys(GL_CONSTANTS);
	const descs: PropertyDescriptorMap = {};
	for (let i = 0; i < keys.length; i++) {
		const k = keys[i];
		descs[k] = { value: (GL_CONSTANTS as Record<string, number>)[k] };
	}
	Object.defineProperties(WebGLRenderingContext, descs);
	Object.defineProperties(WebGLRenderingContext.prototype, descs);
}
def(WebGLRenderingContext);

$.webglInitClass(WebGLRenderingContext, {
	WebGLBuffer,
	WebGLFramebuffer,
	WebGLProgram,
	WebGLRenderbuffer,
	WebGLShader,
	WebGLTexture,
	WebGLUniformLocation,
	WebGLActiveInfo,
	WebGLShaderPrecisionFormat,
});

// ---------------------------------------------------------------------------
// Ledger #70 — TexImageSource normalization shim for WebGL 1.
// Mirrors the WebGL 2 shim in webgl2-rendering-context.ts (~line 1204). The
// native texImage2D / texSubImage2D bindings only handle ArrayBufferView /
// PBO-offset pixel data (fixed 9-arg / 10-arg signatures). WebGL 1 spec's
// 6-arg TexImageSource overloads — `texImage2D(target, level, IF, format,
// type, source)` and `texSubImage2D(target, level, xoff, yoff, format, type,
// source)` — arrive at the native with the ImageBitmap in info[5], NOT
// info[8], so `pixels` reads as undefined and the texture stays cleared.
// Convert the source to raw RGBA bytes via an OffscreenCanvas round-trip
// and reshape the arg list to the 9-arg form before dispatching. #69 (the
// engine-side nx_image_t handler) remains a spec-compliance fallback for
// direct 9-arg calls that pass an nx_image_t as the pixels arg.
// ---------------------------------------------------------------------------

function isTexImageSource(v: any): boolean {
	return (
		v !== null &&
		typeof v === 'object' &&
		!ArrayBuffer.isView(v) &&
		typeof v.width === 'number' &&
		typeof v.height === 'number'
	);
}

function sourceToPixels(src: any): {
	data: Uint8Array;
	width: number;
	height: number;
} {
	let id: ImageData;
	if (src instanceof ImageData) {
		id = src;
	} else {
		// Rasterize through an offscreen 2D canvas; getImageData() returns
		// non-premultiplied RGBA, which is exactly what GL expects with the
		// default UNPACK_PREMULTIPLY_ALPHA_WEBGL = false.
		const c = new OffscreenCanvas(src.width, src.height);
		const ctx = c.getContext('2d');
		ctx.drawImage(src, 0, 0);
		id = ctx.getImageData(0, 0, src.width, src.height) as ImageData;
	}
	return {
		data: new Uint8Array(
			id.data.buffer,
			id.data.byteOffset,
			id.data.byteLength,
		),
		width: id.width,
		height: id.height,
	};
}

{
	const p: any = WebGLRenderingContext.prototype;
	const nativeTexImage2D = p.texImage2D;
	const nativeTexSubImage2D = p.texSubImage2D;
	if (typeof nativeTexImage2D === 'function') {
		p.texImage2D = function (...args: any[]) {
			const last = args[args.length - 1];
			// Ledger #71 — ImageBitmap passthrough. sourceToPixels always
			// produces 4-byte RGBA per pixel, but the caller may request a
			// smaller format (RGB, LUMINANCE, ALPHA, LUMINANCE_ALPHA) or
			// packed 16-bit type (5_6_5, 4_4_4_4, 5_5_5_1). Passing 4 bpp
			// RGBA when the driver reads 3 bpp misaligns every pixel
			// (observed as `was 255,255,0` where the R of pixel N leaks
			// into the RGB of pixel N-1). Route ImageBitmap sources
			// directly to the native so #69's `convert_image_source_to_gl_
			// pixels` in webgl.cc does the byte-stride-correct conversion
			// via `nx_get_image`. ImageBitmap is nx_image_t under the hood
			// (per #66's construction).
			if (last instanceof ImageBitmap) {
				if (args.length === 6) {
					// (target, level, internalformat, format, type, source)
					// → (target, level, IF, w, h, 0, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						last.width,
						last.height,
						0,
						args[3],
						args[4],
						last,
					];
				}
				// 9-arg: source is already at args[8]; args pass through.
				return nativeTexImage2D.apply(this, args);
			}
			if (isTexImageSource(last)) {
				const px = sourceToPixels(last);
				if (args.length === 6) {
					// (target, level, internalformat, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						px.width,
						px.height,
						0,
						args[3],
						args[4],
						px.data,
					];
				} else {
					// (..., width, height, border, format, type, source)
					args[args.length - 1] = px.data;
				}
			}
			return nativeTexImage2D.apply(this, args);
		};
		p.texSubImage2D = function (...args: any[]) {
			const last = args[args.length - 1];
			// Ledger #71 — same short-circuit as texImage2D above. See there
			// for rationale.
			if (last instanceof ImageBitmap) {
				if (args.length === 7) {
					// (target, level, xoff, yoff, format, type, source)
					// → (target, level, xoff, yoff, w, h, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						args[3],
						last.width,
						last.height,
						args[4],
						args[5],
						last,
					];
				}
				return nativeTexSubImage2D.apply(this, args);
			}
			if (isTexImageSource(last)) {
				const px = sourceToPixels(last);
				if (args.length === 7) {
					// (target, level, xoffset, yoffset, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						args[3],
						px.width,
						px.height,
						args[4],
						args[5],
						px.data,
					];
				} else {
					// (..., width, height, format, type, source)
					args[args.length - 1] = px.data;
				}
			}
			return nativeTexSubImage2D.apply(this, args);
		};
	}
}

/**
 * @ignore
 * Internal factory used by `Screen#getContext('webgl')`. Returns `null`
 * when the GL context could not be created (Skia GPU path not up, or the
 * tenant FBO init failed).
 */
export function createWebGLContext(
	canvas: Screen,
): WebGLRenderingContext | null {
	const c = $.webglContextNew(canvas);
	if (!c) return null;
	const ctx = proto(c, WebGLRenderingContext);
	_.set(ctx, { canvas });
	return ctx;
}
