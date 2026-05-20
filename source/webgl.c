#include "webgl.h"
#include "webgl_egl.h"
#include "canvas.h"
#include "util.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#define GL_NO_ERROR 0
#define GL_NONE 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_DITHER 0x0BD0
#define GL_BLEND 0x0BE2
#define GL_FUNC_ADD 0x8006
#define GL_BLEND_EQUATION 0x8009
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_SCISSOR_TEST 0x0C11
#define GL_STENCIL_TEST 0x0B90
#define GL_STENCIL_CLEAR_VALUE 0x0B91
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96
#define GL_KEEP 0x1E00
#define GL_SCISSOR_BOX 0x0C10
#define GL_VIEWPORT 0x0BA2
#define GL_ALIASED_POINT_SIZE_RANGE 0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE 0x846E
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_RED_BITS 0x0D52
#define GL_GREEN_BITS 0x0D53
#define GL_BLUE_BITS 0x0D54
#define GL_ALPHA_BITS 0x0D55
#define GL_DEPTH_BITS 0x0D56
#define GL_STENCIL_BITS 0x0D57
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_FLIP_Y_WEBGL 0x9240
#define GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241
#define GL_UNPACK_COLORSPACE_CONVERSION_WEBGL 0x9243
#define GL_BROWSER_DEFAULT_WEBGL 0x9244
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#define GL_MAX_VARYING_VECTORS 0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_LOW_FLOAT 0x8DF0
#define GL_MEDIUM_FLOAT 0x8DF1
#define GL_HIGH_FLOAT 0x8DF2
#define GL_LOW_INT 0x8DF3
#define GL_MEDIUM_INT 0x8DF4
#define GL_HIGH_INT 0x8DF5
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_DELETE_STATUS 0x8B80
#define GL_SHADER_TYPE 0x8B4F
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_CUBE 0x8B60
#define GL_ARRAY_BUFFER 0x8892
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_RENDERBUFFER 0x8D41
#define GL_RENDERBUFFER_BINDING 0x8CA7
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH_STENCIL 0x84F9
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_UNSIGNED_INT_24_8_WEBGL 0x84FA
#define GL_STENCIL_INDEX8 0x8D48
#define GL_RGB 0x1907
#define GL_RGB565 0x8D62
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_RGBA 0x1908
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_COVERAGE 0x80A0

// gl.hint() pnames + values (milestone #16 added FRAGMENT_SHADER_DERIVATIVE
// for the OES_standard_derivatives extension; GENERATE_MIPMAP_HINT is the
// only other WebGL 1 pname per spec).
#define GL_DONT_CARE 0x1100
#define GL_FASTEST 0x1101
#define GL_NICEST 0x1102
#define GL_GENERATE_MIPMAP_HINT 0x8192
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES 0x8B8B

#define GL_CAP_BLEND (1u << 0)
#define GL_CAP_CULL_FACE (1u << 1)
#define GL_CAP_DEPTH_TEST (1u << 2)
#define GL_CAP_DITHER (1u << 3)
#define GL_CAP_POLYGON_OFFSET_FILL (1u << 4)
#define GL_CAP_SAMPLE_ALPHA_TO_COVERAGE (1u << 5)
#define GL_CAP_SAMPLE_COVERAGE (1u << 6)
#define GL_CAP_SCISSOR_TEST (1u << 7)
#define GL_CAP_STENCIL_TEST (1u << 8)

#define NX_WEBGL_MAX_VERTEX_ATTRIBS 8

typedef struct {
	bool enabled;
	int size;
	uint32_t type;
	bool normalized;
	int stride;
	int offset;
	JSValue buffer;
	// ANGLE_instanced_arrays / GLES3 divisor. 0 = per-vertex (default);
	// 1+ = advance once per N instances. Read by the passthrough dispatch
	// to detect that an instanced draw is in flight (any used attrib with
	// divisor>0 auto-promotes a non-instanced draw to passthrough too).
	uint32_t divisor;
} nx_webgl_vertex_attrib_t;

typedef struct {
	nx_canvas_t *canvas;
	JSValue canvas_value;
	double clear_color[4];
	double clear_depth;
	int32_t clear_stencil;
	bool color_mask[4];
	bool depth_mask;
	int viewport[4];
	int scissor_box[4];
	uint32_t depth_func;
	uint32_t enabled_caps;
	uint32_t blend_equation_rgb;
	uint32_t blend_equation_alpha;
	uint32_t blend_src;
	uint32_t blend_dst;
	uint32_t blend_src_alpha;
	uint32_t blend_dst_alpha;
	double blend_color[4];
	uint32_t cull_face;
	uint32_t front_face;
	double polygon_offset_factor;
	double polygon_offset_units;
	uint32_t stencil_mask;
	uint32_t stencil_func;
	int32_t stencil_ref;
	uint32_t stencil_value_mask;
	uint32_t stencil_fail;
	uint32_t stencil_zfail;
	uint32_t stencil_zpass;
	double line_width;
	JSValue current_program;
	JSValue array_buffer_binding;
	JSValue element_array_buffer_binding;
	JSValue texture_2d_binding;
	JSValue texture_cube_binding;
	// Currently bound user framebuffer / renderbuffer, JS_UNDEFINED if none
	// (default-FBO bind = JS_UNDEFINED). Bridge dispatch consults the EGL
	// backend's mirrored `current_user_framebuffer` field (set via
	// nx_webgl_egl_set_user_framebuffer). See [[bridge-fbo-support]].
	JSValue framebuffer_binding;
	JSValue renderbuffer_binding;
	uint32_t active_texture;
	nx_webgl_vertex_attrib_t vertex_attribs[NX_WEBGL_MAX_VERTEX_ATTRIBS];
	nx_webgl_egl_t *egl;
	bool bridge_clear_pending;
	uint32_t next_texture_id;
	uint32_t error;
	// gl.hint() pname state. Currently the only pname we accept is
	// `FRAGMENT_SHADER_DERIVATIVE_HINT_OES` (0x8B8B, milestone #16);
	// `GENERATE_MIPMAP_HINT` (0x8192) is also accepted but stored only.
	// Both default to GL_DONT_CARE (0x1100) per spec. Read back via
	// getParameter.
	uint32_t hint_fragment_shader_derivative;
	uint32_t hint_generate_mipmap;
} nx_webgl_context_t;

typedef struct {
	uint32_t type;
	char *source;
	char *info_log;
	uint32_t gles_handle;
	bool compile_status;
	bool gles_compile_attempted;
	bool deleted;
	// Set if the shader source contains `#pragma raw_passthrough` (anywhere
	// in the source — comment lines are accepted, since we don't strip
	// comments before scanning). When any attached shader has this flag,
	// the linked program is marked `raw_passthrough` and its draws bypass
	// the bridge's swap-in-hardcoded-program path; the user's native GLES
	// program runs directly. See [[nxjs-no-custom-fragment-shader]] for
	// the architectural background.
	bool raw_passthrough;
} nx_webgl_shader_t;

typedef struct {
	JSValue vertex_shader;
	JSValue fragment_shader;
	char *info_log;
	uint32_t gles_handle;
	float matrix4[16];
	float projection_matrix[16];
	float model_view_matrix[16];
	float color[4];
	float offset[2];
	float line_scale;
	float line_dash_size;
	float line_total_size;
	float fog_color[3];
	float fog_near;
	float fog_far;
	float light_direction[3];
	float light_color[3];
	float light_direction2[3];
	float light_color2[3];
	float ambient_light_color[3];
	float point_light_position[3];
	float point_light_color[3];
	float point_light_distance;
	float point_light_decay;
	float fog_density;
	float map_transform[9];
	float model_matrix[16];
	float sprite_center[2];
	float sprite_rotation;
	float point_size;
	float specular[3];
	float shininess;
	float emissive[3];
	int line_distance_attrib_index;
	int color_attrib_index;
	int position_attrib_index;
	int uv_attrib_index;
	int normal_attrib_index;
	// Set at linkProgram if any attached shader has `#pragma raw_passthrough`.
	// When true, draw dispatch bypasses the bridge's hardcoded-program swap
	// and runs the user's linked GLES program directly via native
	// glDrawArrays/glDrawElements. The user is responsible for vertex
	// transformation in their own GLSL. Buffer data reaches the GPU via
	// each `nx_webgl_buffer_t.gles_handle`; attribute pointers are forwarded
	// to native GLES at draw time. See [[bridge-raw-shader-passthrough]].
	bool raw_passthrough;
	bool has_matrix4;
	bool has_projection_matrix;
	bool has_model_view_matrix;
	bool has_color;
	bool has_offset;
	int sampler0;
	bool has_sampler0;
	bool has_line_scale;
	bool has_line_dash_size;
	bool has_line_total_size;
	bool has_fog_color;
	bool has_fog_near;
	bool has_fog_far;
	bool has_light_direction;
	bool has_light_color;
	bool has_light_direction2;
	bool has_light_color2;
	bool has_ambient_light_color;
	bool has_point_light_position;
	bool has_point_light_color;
	bool has_point_light_distance;
	bool has_point_light_decay;
	bool has_fog_density;
	bool has_map_transform;
	bool has_model_matrix;
	bool has_sprite_center;
	bool has_sprite_rotation;
	bool has_point_size;
	bool has_specular;
	bool has_shininess;
	bool has_emissive;
	bool has_line_distance_attrib_index;
	bool has_color_attrib_index;
	bool has_position_attrib_index;
	bool has_uv_attrib_index;
	bool has_normal_attrib_index;
	bool link_status;
	bool gles_link_attempted;
	bool deleted;
	// User-side bindAttribLocation calls are queued here and replayed
	// against the GLES program inside linkProgram (the GLES program
	// doesn't exist before link). Capped at NX_WEBGL_MAX_ATTRIB_BINDINGS;
	// past that the JS call sets GL_OUT_OF_MEMORY.
	struct {
		int location;
		char *name;  // strdup'd; freed on program delete
	} attrib_bindings[16];
	int attrib_binding_count;
} nx_webgl_program_t;

#define NX_WEBGL_MAX_ATTRIB_BINDINGS 16

typedef struct {
	uint32_t target;
	uint32_t usage;
	size_t size;
	uint8_t *data;
	bool deleted;
	// Native GLES buffer handle. 0 until first successful upload (lazy
	// allocate in bufferData). Used only by the raw-shader passthrough
	// draw path; bridge-mode draws read from the CPU-side `data` copy and
	// upload to their own dedicated VBOs each frame.
	uint32_t gles_handle;
} nx_webgl_buffer_t;

typedef struct {
	uint32_t target;
	uint32_t min_filter;
	uint32_t mag_filter;
	uint32_t wrap_s;
	uint32_t wrap_t;
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	int *alpha_min_x;
	int *alpha_max_x;
	uint32_t bridge_id;
	uint32_t revision;
	// Persistent native GLES texture handle. 0 until the texture is used as
	// an FBO color attachment (`framebufferTexture2D`) or `texImage2D` is
	// called with NULL data (storage-only allocation, the FBO-color-init
	// pattern). When non-zero, bridge dispatch binds this handle directly
	// instead of going through the per-draw `texture_cache` upload path.
	// Set + populated by webgl_egl helpers; the texture finalizer frees it.
	// See [[bridge-fbo-support]].
	uint32_t gles_handle;
	bool deleted;
} nx_webgl_texture_t;

// FBO + renderbuffer objects. Added for milestone #19 (webgl_postprocessing)
// so Three.js's `WebGLRenderTarget` can be backed by real native GLES FBOs
// and the bridge can be retargeted into them via
// `nx_webgl_egl_set_user_framebuffer`. See [[bridge-fbo-support]].
typedef struct {
	uint32_t handle;          // Native GLES FBO handle (0 = unallocated).
	int width;                // Derived from color attachment dims.
	int height;
	JSValue color_attachment;  // texture JSValue (dup'd) — kept alive while attached.
	JSValue depth_attachment;  // renderbuffer JSValue (dup'd).
	JSValue stencil_attachment;
	bool deleted;
} nx_webgl_framebuffer_t;

typedef struct {
	uint32_t handle;           // Native GLES RBO handle.
	uint32_t internal_format;  // GL_DEPTH_COMPONENT16, etc.
	int width;
	int height;
	bool deleted;
} nx_webgl_renderbuffer_t;

typedef struct {
	float x;
	float y;
} nx_webgl_vec2_t;

typedef struct {
	float x;
	float y;
	float z;
} nx_webgl_vec3_t;

typedef struct {
	nx_webgl_vec2_t p[3];
	nx_webgl_vec2_t uv[3];
	float z;
} nx_webgl_triangle_t;

typedef struct {
	nx_webgl_vec2_t p[4];
	nx_webgl_vec2_t uv[4];
} nx_webgl_textured_quad_t;

typedef enum {
	NX_WEBGL_UNIFORM_UNKNOWN,
	NX_WEBGL_UNIFORM_MATRIX4,
	NX_WEBGL_UNIFORM_PROJECTION_MATRIX,
	NX_WEBGL_UNIFORM_MODEL_VIEW_MATRIX,
	NX_WEBGL_UNIFORM_COLOR,
	NX_WEBGL_UNIFORM_OPACITY,
	NX_WEBGL_UNIFORM_OFFSET,
	NX_WEBGL_UNIFORM_SAMPLER,
	NX_WEBGL_UNIFORM_LINE_SCALE,
	NX_WEBGL_UNIFORM_LINE_DASH_SIZE,
	NX_WEBGL_UNIFORM_LINE_TOTAL_SIZE,
	NX_WEBGL_UNIFORM_FOG_COLOR,
	NX_WEBGL_UNIFORM_FOG_NEAR,
	NX_WEBGL_UNIFORM_FOG_FAR,
	NX_WEBGL_UNIFORM_LIGHT_DIRECTION,
	NX_WEBGL_UNIFORM_LIGHT_COLOR,
	NX_WEBGL_UNIFORM_AMBIENT_LIGHT_COLOR,
	NX_WEBGL_UNIFORM_POINT_LIGHT_POSITION,
	NX_WEBGL_UNIFORM_POINT_LIGHT_COLOR,
	NX_WEBGL_UNIFORM_POINT_LIGHT_DISTANCE,
	NX_WEBGL_UNIFORM_POINT_LIGHT_DECAY,
	NX_WEBGL_UNIFORM_LIGHT_DIRECTION_1,
	NX_WEBGL_UNIFORM_LIGHT_COLOR_1,
	NX_WEBGL_UNIFORM_FOG_DENSITY,
	NX_WEBGL_UNIFORM_MAP_TRANSFORM,
	NX_WEBGL_UNIFORM_MODEL_MATRIX,
	NX_WEBGL_UNIFORM_SPRITE_CENTER,
	NX_WEBGL_UNIFORM_SPRITE_ROTATION,
	NX_WEBGL_UNIFORM_POINT_SIZE,
	NX_WEBGL_UNIFORM_SPECULAR,
	NX_WEBGL_UNIFORM_SHININESS,
	NX_WEBGL_UNIFORM_EMISSIVE,
} nx_webgl_uniform_kind_t;

typedef struct {
	JSValue program;
	char *name;
	nx_webgl_uniform_kind_t kind;
	int location;
} nx_webgl_uniform_location_t;

typedef struct {
	const char *name;
	int size;
	uint32_t type;
} nx_webgl_active_info_t;

static JSClassID nx_webgl_context_class_id;
static JSClassID nx_webgl_shader_class_id;
static JSClassID nx_webgl_program_class_id;
static JSClassID nx_webgl_buffer_class_id;
static JSClassID nx_webgl_uniform_location_class_id;
static JSClassID nx_webgl_texture_class_id;
static JSClassID nx_webgl_framebuffer_class_id;
static JSClassID nx_webgl_renderbuffer_class_id;

// Fallback names returned by `gl.getActiveAttrib` / `gl.getActiveUniform`
// when bridge mode is NOT active (or no GLES program is linked yet). When
// bridge mode IS active with a real linked program, those functions
// forward to native GLES and return the actual shader's attributes /
// uniforms instead — see nx_webgl_get_active_attrib / nx_webgl_get_active_uniform.
// Pre-declaring optional features (multi-light, point-light, fog) in this
// fallback list would crash Three.js (it'd try to upload state for slots
// the actual program may not have), so keep this list lean.
static const nx_webgl_active_info_t active_attributes[] = {
	{"position", 1, GL_FLOAT_VEC3},
	{"color", 1, GL_FLOAT_VEC4},
	{"uv", 1, GL_FLOAT_VEC2},
	{"lineDistance", 1, GL_FLOAT},
};

static const nx_webgl_active_info_t active_uniforms[] = {
	{"projectionMatrix", 1, GL_FLOAT_MAT4},
	{"modelViewMatrix", 1, GL_FLOAT_MAT4},
	{"diffuse", 1, GL_FLOAT_VEC3},
	{"opacity", 1, GL_FLOAT},
	{"map", 1, GL_SAMPLER_2D},
	{"scale", 1, GL_FLOAT},
	{"dashSize", 1, GL_FLOAT},
	{"totalSize", 1, GL_FLOAT},
	{"u_matrix", 1, GL_FLOAT_MAT4},
	{"u_color", 1, GL_FLOAT_VEC4},
	{"u_offset", 1, GL_FLOAT_VEC2},
	{"u_texture", 1, GL_SAMPLER_2D},
};

static double clamp01(double value) {
	if (isnan(value) || value < 0.)
		return 0.;
	if (value > 1.)
		return 1.;
	return value;
}

static uint8_t to_u8(double value) {
	return (uint8_t)(clamp01(value) * 255. + 0.5);
}

static uint32_t cap_to_flag(uint32_t cap) {
	switch (cap) {
	case GL_BLEND:
		return GL_CAP_BLEND;
	case GL_CULL_FACE:
		return GL_CAP_CULL_FACE;
	case GL_DEPTH_TEST:
		return GL_CAP_DEPTH_TEST;
	case GL_DITHER:
		return GL_CAP_DITHER;
	case GL_POLYGON_OFFSET_FILL:
		return GL_CAP_POLYGON_OFFSET_FILL;
	case GL_SAMPLE_ALPHA_TO_COVERAGE:
		return GL_CAP_SAMPLE_ALPHA_TO_COVERAGE;
	case GL_SAMPLE_COVERAGE:
		return GL_CAP_SAMPLE_COVERAGE;
	case GL_SCISSOR_TEST:
		return GL_CAP_SCISSOR_TEST;
	case GL_STENCIL_TEST:
		return GL_CAP_STENCIL_TEST;
	default:
		return 0;
	}
}

static bool is_depth_func(uint32_t func) {
	return func == GL_NEVER || func == GL_LESS || func == GL_EQUAL ||
		   func == GL_LEQUAL || func == GL_GREATER || func == GL_NOTEQUAL ||
		   func == GL_GEQUAL || func == GL_ALWAYS;
}

static bool is_shader_type(uint32_t type) {
	return type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER;
}

static bool is_buffer_usage(uint32_t usage) {
	return usage == GL_STATIC_DRAW || usage == GL_DYNAMIC_DRAW ||
		   usage == GL_STREAM_DRAW;
}

static bool is_vertex_attrib_type(uint32_t type) {
	return type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == GL_SHORT ||
		   type == GL_UNSIGNED_SHORT || type == GL_FLOAT;
}

static bool is_blend_factor(uint32_t factor) {
	return factor == GL_ZERO || factor == GL_ONE || factor == GL_SRC_ALPHA ||
		   factor == GL_ONE_MINUS_SRC_ALPHA || factor == GL_DST_ALPHA ||
		   factor == GL_ONE_MINUS_DST_ALPHA || factor == GL_DST_COLOR ||
		   factor == GL_ONE_MINUS_DST_COLOR || factor == GL_SRC_COLOR ||
		   factor == GL_ONE_MINUS_SRC_COLOR ||
		   factor == GL_SRC_ALPHA_SATURATE;
}

static bool is_blend_equation(uint32_t equation) {
	return equation == GL_FUNC_ADD;
}

static bool is_cull_face_mode(uint32_t mode) {
	return mode == GL_FRONT || mode == GL_BACK || mode == GL_FRONT_AND_BACK;
}

static bool is_front_face_mode(uint32_t mode) {
	return mode == GL_CW || mode == GL_CCW;
}

static bool is_stencil_op(uint32_t op) {
	return op == GL_KEEP || op == GL_ZERO;
}

static bool is_texture_binding_target(uint32_t target) {
	return target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP;
}

static bool is_texture_image_target(uint32_t target) {
	return target == GL_TEXTURE_2D ||
		   (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
			target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
}

static JSValue *texture_binding_for_target(nx_webgl_context_t *context,
										   uint32_t target) {
	if (target == GL_TEXTURE_2D)
		return &context->texture_2d_binding;
	if (target == GL_TEXTURE_CUBE_MAP ||
		(target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
		 target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
		return &context->texture_cube_binding;
	return NULL;
}

static uint32_t texture_object_target_for_image_target(uint32_t target) {
	return target == GL_TEXTURE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
}

static nx_webgl_uniform_kind_t uniform_kind_for_name(const char *name) {
	if (strcmp(name, "projectionMatrix") == 0)
		return NX_WEBGL_UNIFORM_PROJECTION_MATRIX;
	if (strcmp(name, "modelViewMatrix") == 0)
		return NX_WEBGL_UNIFORM_MODEL_VIEW_MATRIX;
	if (strcmp(name, "u_matrix") == 0 || strcmp(name, "matrix") == 0 ||
		strcmp(name, "u_transform") == 0 || strcmp(name, "transform") == 0)
		return NX_WEBGL_UNIFORM_MATRIX4;
	if (strcmp(name, "u_color") == 0 || strcmp(name, "color") == 0 ||
		strcmp(name, "diffuse") == 0)
		return NX_WEBGL_UNIFORM_COLOR;
	if (strcmp(name, "opacity") == 0)
		return NX_WEBGL_UNIFORM_OPACITY;
	if (strcmp(name, "u_offset") == 0 || strcmp(name, "offset") == 0 ||
		strcmp(name, "translation") == 0)
		return NX_WEBGL_UNIFORM_OFFSET;
	if (strcmp(name, "u_texture") == 0 || strcmp(name, "texture") == 0 ||
		strcmp(name, "u_sampler") == 0 || strcmp(name, "sampler") == 0 ||
		strcmp(name, "map") == 0 ||
		// Three.js's WebGLBackground.js uses `t2D` for the 2D-Texture
		// background sampler (BackgroundShader). Without this, the bg
		// plane's sampler binding is dropped, the dispatcher's
		// `active_texture_for_program` returns NULL, and the bridge
		// renders the fullscreen quad as a solid stale `program->color`
		// instead of sampling the texture.
		strcmp(name, "t2D") == 0)
		return NX_WEBGL_UNIFORM_SAMPLER;
	if (strcmp(name, "scale") == 0)
		return NX_WEBGL_UNIFORM_LINE_SCALE;
	if (strcmp(name, "dashSize") == 0)
		return NX_WEBGL_UNIFORM_LINE_DASH_SIZE;
	if (strcmp(name, "totalSize") == 0)
		return NX_WEBGL_UNIFORM_LINE_TOTAL_SIZE;
	if (strcmp(name, "fogColor") == 0)
		return NX_WEBGL_UNIFORM_FOG_COLOR;
	if (strcmp(name, "fogNear") == 0)
		return NX_WEBGL_UNIFORM_FOG_NEAR;
	if (strcmp(name, "fogFar") == 0)
		return NX_WEBGL_UNIFORM_FOG_FAR;
	// Three.js's stock directional-light uniform names (single light only,
	// indexed-struct syntax). When `scene.add(new DirectionalLight(...))`
	// fires, Three.js's WebGLRenderer issues `gl.uniform3f` against these
	// exact names so the bridge program picks them up.
	if (strcmp(name, "directionalLights[0].direction") == 0)
		return NX_WEBGL_UNIFORM_LIGHT_DIRECTION;
	if (strcmp(name, "directionalLights[0].color") == 0)
		return NX_WEBGL_UNIFORM_LIGHT_COLOR;
	if (strcmp(name, "ambientLightColor") == 0)
		return NX_WEBGL_UNIFORM_AMBIENT_LIGHT_COLOR;
	// Three.js point-light stock uniform names (single light only). Position
	// is in view space when Three.js uploads it. distance=0 means infinite
	// range; decay=0 means no falloff (legacy "physically incorrect" mode).
	if (strcmp(name, "pointLights[0].position") == 0)
		return NX_WEBGL_UNIFORM_POINT_LIGHT_POSITION;
	if (strcmp(name, "pointLights[0].color") == 0)
		return NX_WEBGL_UNIFORM_POINT_LIGHT_COLOR;
	if (strcmp(name, "pointLights[0].distance") == 0)
		return NX_WEBGL_UNIFORM_POINT_LIGHT_DISTANCE;
	if (strcmp(name, "pointLights[0].decay") == 0)
		return NX_WEBGL_UNIFORM_POINT_LIGHT_DECAY;
	// Second directional light (max two for now — sufficient for scenes
	// with key + fill lights like misc_controls_orbit, but multi-light
	// support is still scope-limited).
	if (strcmp(name, "directionalLights[1].direction") == 0)
		return NX_WEBGL_UNIFORM_LIGHT_DIRECTION_1;
	if (strcmp(name, "directionalLights[1].color") == 0)
		return NX_WEBGL_UNIFORM_LIGHT_COLOR_1;
	// Three.js's FogExp2 uniform. When set, the bridge applies the
	// `1 - exp(-density² × depth²)` falloff instead of linear's
	// smoothstep(fogNear, fogFar, depth).
	if (strcmp(name, "fogDensity") == 0)
		return NX_WEBGL_UNIFORM_FOG_DENSITY;
	// Three.js's texture-transform mat3. Encodes texture.repeat / .offset /
	// .rotation / .center as a 3x3 affine. Three.js's MeshPhongMaterial /
	// MeshBasicMaterial use `mapTransform`; some older / lighter materials
	// use `uvTransform`. Both go to the same slot — the bridge's textured
	// vertex shader transforms `a_uv` through it before passing v_uv to
	// the fragment shader, which lets `texture.repeat.set(...)` /
	// `texture.offset.set(...)` actually take effect under the bridge.
	if (strcmp(name, "mapTransform") == 0 ||
		strcmp(name, "uvTransform") == 0)
		return NX_WEBGL_UNIFORM_MAP_TRANSFORM;
	// Three.js's `modelMatrix` is uploaded alongside `modelViewMatrix` for
	// sprite materials; the sprite vertex shader extracts world-space scale
	// from its column lengths. Our CPU-side sprite dispatch needs the same.
	if (strcmp(name, "modelMatrix") == 0)
		return NX_WEBGL_UNIFORM_MODEL_MATRIX;
	// SpriteMaterial-specific uniforms — together they signal "this draw is
	// a Three.js sprite" so the bridge can route through the sprite math
	// path (CPU-side screen-aligned quad expansion) instead of the generic
	// textured triangle path.
	if (strcmp(name, "center") == 0)
		return NX_WEBGL_UNIFORM_SPRITE_CENTER;
	if (strcmp(name, "rotation") == 0)
		return NX_WEBGL_UNIFORM_SPRITE_ROTATION;
	// Three.js's PointsMaterial uploads `size` (= material.size × pixelRatio)
	// alongside the existing `scale` uniform (= 0.5 × canvas height, for
	// `sizeAttenuation`). The bridge reads `size` for the point-quad extent;
	// `scale` is interpreted as line-scale OR attenuation-scale at the
	// dispatch site based on whether we're drawing lines or points.
	if (strcmp(name, "size") == 0)
		return NX_WEBGL_UNIFORM_POINT_SIZE;
	// Three.js's MeshPhongMaterial uploads `specular` (vec3 specular color,
	// default 0x111111 = 0.067,0.067,0.067) and `shininess` (float exponent,
	// default 30). The bridge adds an additive Blinn-Phong specular term to
	// the fragment shader lighting compose when both are bound.
	if (strcmp(name, "specular") == 0)
		return NX_WEBGL_UNIFORM_SPECULAR;
	if (strcmp(name, "shininess") == 0)
		return NX_WEBGL_UNIFORM_SHININESS;
	// Three.js's MeshLambert/Phong/Standard uploads `emissive` (vec3,
	// default 0x000000). Bridge adds it as an additive term at the end
	// of the fragment-shader compose so emissive materials self-illuminate
	// (visible even on unlit fragments / against black bg). Default zero
	// makes the additive a no-op when not set.
	if (strcmp(name, "emissive") == 0)
		return NX_WEBGL_UNIFORM_EMISSIVE;
	return NX_WEBGL_UNIFORM_UNKNOWN;
}

static int clamp_int(int value, int min_value, int max_value) {
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

static JSValue new_active_info(JSContext *ctx,
							   const nx_webgl_active_info_t *info) {
	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;
	JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, info->name),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewInt32(ctx, info->size),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "type",
							  JS_NewUint32(ctx, info->type), JS_PROP_C_W_E);
	return obj;
}

static float edge_function(nx_webgl_vec2_t a, nx_webgl_vec2_t b,
						   nx_webgl_vec2_t c) {
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

static inline bool scanline_edge_intersection(nx_webgl_vec2_t a,
											  nx_webgl_vec2_t b,
											  float y,
											  float *x_out) {
	if (a.y == b.y)
		return false;
	float min_y = fminf(a.y, b.y);
	float max_y = fmaxf(a.y, b.y);
	if (y < min_y || y > max_y)
		return false;
	float t = (y - a.y) / (b.y - a.y);
	if (t < 0.f || t > 1.f)
		return false;
	*x_out = a.x + (b.x - a.x) * t;
	return true;
}

static bool triangle_scanline_span(nx_webgl_vec2_t v0, nx_webgl_vec2_t v1,
								   nx_webgl_vec2_t v2, float y,
								   float *x_min_out,
								   float *x_max_out) {
	float xs[3];
	int count = 0;
	if (scanline_edge_intersection(v0, v1, y, &xs[count]))
		count++;
	if (scanline_edge_intersection(v1, v2, y, &xs[count]))
		count++;
	if (scanline_edge_intersection(v2, v0, y, &xs[count]))
		count++;
	if (count < 2)
		return false;

	float x_min = xs[0];
	float x_max = xs[0];
	for (int i = 1; i < count; i++) {
		if (xs[i] < x_min)
			x_min = xs[i];
		if (xs[i] > x_max)
			x_max = xs[i];
	}
	if (x_max < x_min)
		return false;
	*x_min_out = x_min;
	*x_max_out = x_max;
	return true;
}

static void replace_string(JSContext *ctx, char **target, const char *value) {
	if (*target) {
		js_free(ctx, *target);
		*target = NULL;
	}
	*target = js_strdup(ctx, value ? value : "");
}

static JSValue new_number_array(JSContext *ctx, const double *values,
								size_t count) {
	JSValue array = JS_NewArray(ctx);
	for (size_t i = 0; i < count; i++)
		JS_SetPropertyUint32(ctx, array, i, JS_NewFloat64(ctx, values[i]));
	return array;
}

static JSValue new_int_array(JSContext *ctx, const int *values, size_t count) {
	JSValue array = JS_NewArray(ctx);
	for (size_t i = 0; i < count; i++)
		JS_SetPropertyUint32(ctx, array, i, JS_NewInt32(ctx, values[i]));
	return array;
}

static nx_webgl_context_t *nx_get_webgl_context(JSContext *ctx,
												JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_webgl_context_class_id);
}

static nx_webgl_shader_t *nx_get_webgl_shader(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_shader_class_id);
}

static nx_webgl_program_t *nx_get_webgl_program(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_program_class_id);
}

static nx_webgl_buffer_t *nx_get_webgl_buffer(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_buffer_class_id);
}

static nx_webgl_uniform_location_t *
nx_get_webgl_uniform_location(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_uniform_location_class_id);
}

static nx_webgl_texture_t *nx_get_webgl_texture(JSValueConst obj) {
	return JS_GetOpaque(obj, nx_webgl_texture_class_id);
}

static JSValue nx_webgl_context_new(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);
	if (!canvas)
		return JS_EXCEPTION;

	nx_webgl_context_t *context = js_mallocz(ctx, sizeof(nx_webgl_context_t));
	if (!context)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_context_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		return obj;
	}

	context->canvas = canvas;
	context->canvas_value = JS_DupValue(ctx, argv[0]);
	context->viewport[0] = 0;
	context->viewport[1] = 0;
	context->viewport[2] = canvas->width;
	context->viewport[3] = canvas->height;
	context->scissor_box[0] = 0;
	context->scissor_box[1] = 0;
	context->scissor_box[2] = canvas->width;
	context->scissor_box[3] = canvas->height;
	context->clear_depth = 1.;
	context->clear_stencil = 0;
	for (int i = 0; i < 4; i++)
		context->color_mask[i] = true;
	context->depth_mask = true;
	context->depth_func = GL_LESS;
	context->enabled_caps = GL_CAP_DITHER;
	context->blend_equation_rgb = GL_FUNC_ADD;
	context->blend_equation_alpha = GL_FUNC_ADD;
	context->blend_src = GL_ONE;
	context->blend_dst = GL_ZERO;
	context->blend_src_alpha = GL_ONE;
	context->blend_dst_alpha = GL_ZERO;
	context->cull_face = GL_BACK;
	context->front_face = GL_CCW;
	context->stencil_mask = 0xffffffffu;
	context->stencil_func = GL_ALWAYS;
	context->stencil_ref = 0;
	context->stencil_value_mask = 0xffffffffu;
	context->stencil_fail = GL_KEEP;
	context->stencil_zfail = GL_KEEP;
	context->stencil_zpass = GL_KEEP;
	context->line_width = 1.;
	context->current_program = JS_UNDEFINED;
	context->array_buffer_binding = JS_UNDEFINED;
	context->element_array_buffer_binding = JS_UNDEFINED;
	context->texture_2d_binding = JS_UNDEFINED;
	context->texture_cube_binding = JS_UNDEFINED;
	context->framebuffer_binding = JS_UNDEFINED;
	context->renderbuffer_binding = JS_UNDEFINED;
	context->active_texture = GL_TEXTURE0;
	context->next_texture_id = 1;
	context->hint_fragment_shader_derivative = GL_DONT_CARE;
	context->hint_generate_mipmap = GL_DONT_CARE;
	for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++)
		context->vertex_attribs[i].buffer = JS_UNDEFINED;
	context->egl = nx_webgl_egl_create(ctx, canvas);

	JS_SetOpaque(obj, context);
	return obj;
}

static void finalizer_webgl_context(JSRuntime *rt, JSValue val) {
	nx_webgl_context_t *context =
		JS_GetOpaque(val, nx_webgl_context_class_id);
	if (context) {
		JS_FreeValueRT(rt, context->canvas_value);
		JS_FreeValueRT(rt, context->current_program);
		JS_FreeValueRT(rt, context->array_buffer_binding);
		JS_FreeValueRT(rt, context->element_array_buffer_binding);
		JS_FreeValueRT(rt, context->texture_2d_binding);
		JS_FreeValueRT(rt, context->texture_cube_binding);
		JS_FreeValueRT(rt, context->framebuffer_binding);
		JS_FreeValueRT(rt, context->renderbuffer_binding);
		for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++)
			JS_FreeValueRT(rt, context->vertex_attribs[i].buffer);
		nx_webgl_egl_destroy(rt, context->egl);
		js_free_rt(rt, context);
	}
}

static void finalizer_webgl_shader(JSRuntime *rt, JSValue val) {
	nx_webgl_shader_t *shader = JS_GetOpaque(val, nx_webgl_shader_class_id);
	if (shader) {
		js_free_rt(rt, shader->source);
		js_free_rt(rt, shader->info_log);
		js_free_rt(rt, shader);
	}
}

static void finalizer_webgl_program(JSRuntime *rt, JSValue val) {
	nx_webgl_program_t *program = JS_GetOpaque(val, nx_webgl_program_class_id);
	if (program) {
		JS_FreeValueRT(rt, program->vertex_shader);
		JS_FreeValueRT(rt, program->fragment_shader);
		js_free_rt(rt, program->info_log);
		for (int i = 0; i < program->attrib_binding_count; i++) {
			js_free_rt(rt, program->attrib_bindings[i].name);
		}
		js_free_rt(rt, program);
	}
}

static void finalizer_webgl_buffer(JSRuntime *rt, JSValue val) {
	nx_webgl_buffer_t *buffer = JS_GetOpaque(val, nx_webgl_buffer_class_id);
	if (buffer) {
		js_free_rt(rt, buffer->data);
		js_free_rt(rt, buffer);
	}
}

static void finalizer_webgl_uniform_location(JSRuntime *rt, JSValue val) {
	nx_webgl_uniform_location_t *location =
		JS_GetOpaque(val, nx_webgl_uniform_location_class_id);
	if (location) {
		JS_FreeValueRT(rt, location->program);
		js_free_rt(rt, location->name);
		js_free_rt(rt, location);
	}
}

static void finalizer_webgl_texture(JSRuntime *rt, JSValue val) {
	nx_webgl_texture_t *texture = JS_GetOpaque(val, nx_webgl_texture_class_id);
	if (texture) {
		// Persistent native GLES texture handle (FBO color attachments and
		// any texImage2D(NULL) call). The free happens here so the GPU
		// resource follows the JS object's lifetime.
		// NOTE: We don't have a context pointer in the finalizer; rely on
		// the egl-side delete to NOT need a current context. (glDeleteTextures
		// is documented as silently ignoring invalid handles, and in practice
		// our destroy paths happen with the bridge context current.)
		if (texture->gles_handle) {
			// Best-effort cleanup. We can't reach the egl backend from a
			// finalizer without storing a backend pointer on the texture
			// struct; rather than leak, swb's existing pattern is to delete
			// when the user explicitly calls gl.deleteTexture. Persistent
			// handles unused at finalize-time are minor leaks for the
			// lifetime of the EGL context.
			texture->gles_handle = 0;
		}
		js_free_rt(rt, texture->data);
		js_free_rt(rt, texture->alpha_min_x);
		js_free_rt(rt, texture->alpha_max_x);
		js_free_rt(rt, texture);
	}
}

static nx_webgl_framebuffer_t *nx_get_webgl_framebuffer(JSValueConst obj) {
	if (JS_IsUndefined(obj) || JS_IsNull(obj))
		return NULL;
	return JS_GetOpaque(obj, nx_webgl_framebuffer_class_id);
}

static nx_webgl_renderbuffer_t *nx_get_webgl_renderbuffer(JSValueConst obj) {
	if (JS_IsUndefined(obj) || JS_IsNull(obj))
		return NULL;
	return JS_GetOpaque(obj, nx_webgl_renderbuffer_class_id);
}

static void finalizer_webgl_framebuffer(JSRuntime *rt, JSValue val) {
	nx_webgl_framebuffer_t *fb =
		JS_GetOpaque(val, nx_webgl_framebuffer_class_id);
	if (fb) {
		JS_FreeValueRT(rt, fb->color_attachment);
		JS_FreeValueRT(rt, fb->depth_attachment);
		JS_FreeValueRT(rt, fb->stencil_attachment);
		js_free_rt(rt, fb);
	}
}

static void finalizer_webgl_renderbuffer(JSRuntime *rt, JSValue val) {
	nx_webgl_renderbuffer_t *rb =
		JS_GetOpaque(val, nx_webgl_renderbuffer_class_id);
	if (rb)
		js_free_rt(rt, rb);
}

static JSValue nx_webgl_get_drawing_buffer_width(JSContext *ctx,
												 JSValueConst this_val,
												 int argc,
												 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, context->canvas->width);
}

static JSValue nx_webgl_get_drawing_buffer_height(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, context->canvas->height);
}

static JSValue nx_webgl_get_context_attributes(JSContext *ctx,
											   JSValueConst this_val,
											   int argc,
											   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;
	JS_DefinePropertyValueStr(ctx, obj, "alpha", JS_NewBool(ctx, true),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "depth", JS_NewBool(ctx, true),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "stencil", JS_NewBool(ctx, false),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "antialias", JS_NewBool(ctx, false),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "premultipliedAlpha",
							  JS_NewBool(ctx, true), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "preserveDrawingBuffer",
							  JS_NewBool(ctx, false), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "powerPreference",
							  JS_NewString(ctx, "default"), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "failIfMajorPerformanceCaveat",
							  JS_NewBool(ctx, false), JS_PROP_C_W_E);
	return obj;
}

static JSValue nx_webgl_get_supported_extensions(JSContext *ctx,
												 JSValueConst this_val,
												 int argc,
												 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	JSValue arr = JS_NewArray(ctx);
	if (JS_IsException(arr))
		return arr;
	uint32_t idx = 0;
	if (context->egl && nx_webgl_egl_has_instancing(context->egl)) {
		JS_DefinePropertyValueUint32(ctx, arr, idx++,
			JS_NewString(ctx, "ANGLE_instanced_arrays"), JS_PROP_C_W_E);
	}
	// Milestone #16: OES_standard_derivatives drives the bridge's
	// derivative-normals fallback for `flatShading: true` materials AND
	// is callable directly via passthrough programs that compile-in
	// `#extension GL_OES_standard_derivatives : enable`. Tegra X1 supports
	// it via the GLES 3 core (`fwidth`/`dFdx`/`dFdy` are core in ES3, the
	// OES extension is the ES2 promotion-path token).
	JS_DefinePropertyValueUint32(ctx, arr, idx++,
		JS_NewString(ctx, "OES_standard_derivatives"), JS_PROP_C_W_E);
	// Milestone #19.5: WEBGL_depth_texture makes the FBO depth attachment
	// a sampleable sampler2D returning normalized depth in .x. Required
	// by Three.js's DepthTexture; gated by `renderer.extensions.has(...)`
	// in the upstream webgl_depth_texture demo. Tegra GLES supports it
	// natively (GLES3 has DEPTH_COMPONENT textures + OES_depth_texture
	// promotion is widely available).
	JS_DefinePropertyValueUint32(ctx, arr, idx++,
		JS_NewString(ctx, "WEBGL_depth_texture"), JS_PROP_C_W_E);
	// Sibling OES_depth_texture (the older spec). Three.js may probe
	// either name; advertising both removes ambiguity.
	JS_DefinePropertyValueUint32(ctx, arr, idx++,
		JS_NewString(ctx, "OES_depth_texture"), JS_PROP_C_W_E);
	return arr;
}

// Forward declarations of the gl-context-bound C functions the
// ANGLE_instanced_arrays extension wraps (defined below near
// nx_webgl_draw_elements).
static JSValue nx_webgl_draw_arrays_instanced(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv);
static JSValue nx_webgl_draw_elements_instanced(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv);
static JSValue nx_webgl_vertex_attrib_divisor(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv);

// JS_NewCFunctionData closures for the ANGLE_instanced_arrays extension
// methods. Each binds the gl context (in `func_data[0]`) so the wrapper can
// forward to the underlying gl-proto C functions with the right `this`.
static JSValue ext_inst_draw_arrays_w(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv,
									   int magic, JSValue *func_data) {
	(void)this_val; (void)magic;
	return nx_webgl_draw_arrays_instanced(ctx, func_data[0], argc, argv);
}
static JSValue ext_inst_draw_elements_w(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv,
										 int magic, JSValue *func_data) {
	(void)this_val; (void)magic;
	return nx_webgl_draw_elements_instanced(ctx, func_data[0], argc, argv);
}
static JSValue ext_inst_divisor_w(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv,
								   int magic, JSValue *func_data) {
	(void)this_val; (void)magic;
	return nx_webgl_vertex_attrib_divisor(ctx, func_data[0], argc, argv);
}

static JSValue nx_webgl_get_extension(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_EXCEPTION;

	// ANGLE_instanced_arrays — return an object with the three extension
	// methods bound to this gl context plus the VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE
	// constant. Only advertise it when the driver actually loaded the native
	// instancing entry points (probed at backend init). Three.js's
	// WebGLCapabilities does the lookup once at renderer construction; null
	// here drops it into the no-instancing fallback (no Mesh.InstancedMesh
	// support).
	if ((strcmp(name, "ANGLE_instanced_arrays") == 0) &&
		context->egl && nx_webgl_egl_has_instancing(context->egl)) {
		JS_FreeCString(ctx, name);
		JSValue ext = JS_NewObject(ctx);
		if (JS_IsException(ext))
			return ext;

		JSValue gl = JS_DupValue(ctx, this_val);
		JSValue m_da = JS_NewCFunctionData(ctx, ext_inst_draw_arrays_w, 4, 0,
											1, &gl);
		JSValue m_de = JS_NewCFunctionData(ctx, ext_inst_draw_elements_w, 5, 0,
											1, &gl);
		JSValue m_div = JS_NewCFunctionData(ctx, ext_inst_divisor_w, 2, 0,
											 1, &gl);
		JS_FreeValue(ctx, gl);

		JS_DefinePropertyValueStr(ctx, ext, "drawArraysInstancedANGLE",
								  m_da, JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, ext, "drawElementsInstancedANGLE",
								  m_de, JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, ext, "vertexAttribDivisorANGLE",
								  m_div, JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, ext,
								  "VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE",
								  JS_NewInt32(ctx, 0x88FE), JS_PROP_C_W_E);
		return ext;
	}

	// OES_standard_derivatives — no instance methods, just the hint pname
	// constant. Webby pages call getExtension to feature-detect before
	// using `#extension GL_OES_standard_derivatives : enable` in their
	// shaders. The bridge's flatShading-fallback path uses derivatives
	// internally regardless of whether the page opted in.
	if (strcmp(name, "OES_standard_derivatives") == 0) {
		JS_FreeCString(ctx, name);
		JSValue ext = JS_NewObject(ctx);
		if (JS_IsException(ext))
			return ext;
		JS_DefinePropertyValueStr(ctx, ext,
								  "FRAGMENT_SHADER_DERIVATIVE_HINT_OES",
								  JS_NewInt32(ctx, 0x8B8B), JS_PROP_C_W_E);
		return ext;
	}

	// WEBGL_depth_texture (and the older OES_depth_texture alias). The
	// extension's only client-visible surface is the UNSIGNED_INT_24_8_WEBGL
	// constant used as the `type` argument of `gl.texImage2D` for
	// DEPTH_STENCIL depth+stencil textures (the 24-bit-depth + 8-bit-stencil
	// packed format). DEPTH_COMPONENT alone with UNSIGNED_SHORT or
	// UNSIGNED_INT doesn't need the constant; the format/type accept-list
	// is widened unconditionally once `texImage2D` is asked for them. See
	// [[bridge-depth-texture-support]].
	if (strcmp(name, "WEBGL_depth_texture") == 0 ||
	    strcmp(name, "OES_depth_texture") == 0) {
		JS_FreeCString(ctx, name);
		JSValue ext = JS_NewObject(ctx);
		if (JS_IsException(ext))
			return ext;
		JS_DefinePropertyValueStr(ctx, ext, "UNSIGNED_INT_24_8_WEBGL",
								  JS_NewInt32(ctx, 0x84FA), JS_PROP_C_W_E);
		return ext;
	}

	JS_FreeCString(ctx, name);
	return JS_NULL;
}

static JSValue nx_webgl_get_shader_precision_format(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t shader_type;
	uint32_t precision_type;
	if (JS_ToUint32(ctx, &shader_type, argv[0]) ||
		JS_ToUint32(ctx, &precision_type, argv[1]))
		return JS_EXCEPTION;

	if (!is_shader_type(shader_type)) {
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}

	int range_min = 0;
	int range_max = 0;
	int precision = 0;
	switch (precision_type) {
	case GL_LOW_FLOAT:
		range_min = 8;
		range_max = 8;
		precision = 8;
		break;
	case GL_MEDIUM_FLOAT:
		range_min = 14;
		range_max = 14;
		precision = 10;
		break;
	case GL_HIGH_FLOAT:
		range_min = 127;
		range_max = 127;
		precision = 23;
		break;
	case GL_LOW_INT:
	case GL_MEDIUM_INT:
	case GL_HIGH_INT:
		range_min = 31;
		range_max = 30;
		precision = 0;
		break;
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}

	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;
	JS_DefinePropertyValueStr(ctx, obj, "rangeMin", JS_NewInt32(ctx, range_min),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "rangeMax", JS_NewInt32(ctx, range_max),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "precision",
							  JS_NewInt32(ctx, precision), JS_PROP_C_W_E);
	return obj;
}

static JSValue nx_webgl_clear_color(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	double values[4];
	for (int i = 0; i < 4; i++) {
		if (JS_ToFloat64(ctx, &values[i], argv[i]))
			return JS_EXCEPTION;
		context->clear_color[i] = clamp01(values[i]);
	}
	nx_webgl_egl_set_clear_color(context->egl, context->clear_color);

	return JS_UNDEFINED;
}

static void clear_canvas_software(nx_webgl_context_t *context) {
	nx_canvas_t *canvas = context->canvas;
	if (!canvas || !canvas->data || canvas->width == 0 || canvas->height == 0)
		return;

	uint8_t r = to_u8(context->clear_color[0]);
	uint8_t g = to_u8(context->clear_color[1]);
	uint8_t b = to_u8(context->clear_color[2]);
	uint8_t a = to_u8(context->clear_color[3]);

	uint32_t packed = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
					  ((uint32_t)g << 8) | (uint32_t)b;
	uint32_t *pixels = (uint32_t *)canvas->data;
	int min_x = 0;
	int min_y = 0;
	int max_x = (int)canvas->width;
	int max_y = (int)canvas->height;
	if ((context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0) {
		min_x = clamp_int(context->scissor_box[0], 0, (int)canvas->width);
		min_y = clamp_int(context->scissor_box[1], 0, (int)canvas->height);
		max_x = clamp_int(context->scissor_box[0] + context->scissor_box[2],
						  0, (int)canvas->width);
		max_y = clamp_int(context->scissor_box[1] + context->scissor_box[3],
						  0, (int)canvas->height);
	}
	for (int y = min_y; y < max_y; y++) {
		uint32_t *row = pixels + (size_t)y * canvas->width;
		for (int x = min_x; x < max_x; x++)
			row[x] = packed;
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

static void flush_pending_bridge_clear_to_software(nx_webgl_context_t *context) {
	if (!context)
		return;
	// If the GLES bridge accumulated draws this frame, flush them to the
	// canvas first so software fallbacks can compose on top instead of
	// clobbering bridge content. Gated on `auto_flush_enabled` — clients
	// driving their own readback (inline-canvas WebGL via
	// `gl.setBridgeAutoFlush(false)` + `gl.copyBridgeToCanvas`) DON'T
	// want the bridge to flush its FBO across the full screen canvas
	// here; that would smear bridge content over the page chrome that
	// sits outside the inline canvas's layout slot. When auto-flush is
	// off, we just clear the pending flag and let the client's own
	// readback handle presentation.
	if (context->egl && nx_webgl_egl_has_pending_readback(context->egl)) {
		nx_canvas_t *canvas = context->canvas;
		if (canvas && nx_webgl_egl_get_auto_flush(context->egl)) {
			nx_webgl_egl_flush_bridge_present(context->egl, canvas);
		}
		// Once the bridge content is on the canvas (or the client has
		// opted out of the flush), treat the canvas as already-cleared-
		// and-drawn so the upcoming software draw does not erase what
		// the bridge just produced.
		context->bridge_clear_pending = false;
		return;
	}
	if (!context->bridge_clear_pending)
		return;
	context->bridge_clear_pending = false;
	clear_canvas_software(context);
}

static JSValue nx_webgl_clear(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mask;
	if (JS_ToUint32(ctx, &mask, argv[0]))
		return JS_EXCEPTION;

	if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
				  GL_STENCIL_BUFFER_BIT)) != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if (mask == 0)
		return JS_UNDEFINED;

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	if (nx_webgl_egl_is_bridge_enabled(context->egl)) {
		// Reset per-frame dispatch-debug log. Each draw will append a
		// tag indicating which path it took (bridge success vs failure
		// with reason). Status canvas picks this up via getBackendInfo.
		nx_webgl_egl_reset_dispatch_debug(context->egl);
		// Flush any accumulated bridge draws from the previous frame so the
		// presented canvas shows their pixels (one readback per frame instead
		// of one per draw call). Skipped when the client opted out via
		// `gl.setBridgeAutoFlush(false)` — clients driving their own
		// readback don't need it and the extra 1280×720 glReadPixels +
		// canvas->data write is a per-frame stall plus a visible flash on
		// the first auto-flush before the page paint covers it.
		if (nx_webgl_egl_has_pending_readback(context->egl) &&
		    nx_webgl_egl_get_auto_flush(context->egl)) {
			nx_webgl_egl_flush_bridge_present(context->egl, canvas);
		}
		// Actually clear the GLES FBO so the new frame starts on a clean
		// surface, honoring the user's clearColor + clearDepth +
		// clearStencil + bit-mask state. Pre-#19.5 this hardcoded
		// glClearDepthf(1.0) and always cleared both color+depth —
		// surfaced as a real bug in webgl_depth_texture hw bring-up
		// where `gl.clearDepth(0.5)` had no effect.
		nx_webgl_egl_clear_bridge_with_state(
			context->egl, canvas,
			mask,
			(float)context->clear_depth,
			(int32_t)context->clear_stencil,
			(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
			context->scissor_box,
			(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0);
		// Mark canvas as still needing a software clear in case a software
		// fallback draw happens before any bridge draw this frame; the canvas
		// currently holds the previous frame's bridge readback.
		if (mask & GL_COLOR_BUFFER_BIT)
			context->bridge_clear_pending = true;
		return JS_UNDEFINED;
	}

	if ((mask & GL_COLOR_BUFFER_BIT) == 0)
		return JS_UNDEFINED;
	context->bridge_clear_pending = false;
	clear_canvas_software(context);

	return JS_UNDEFINED;
}

static JSValue nx_webgl_clear_depth(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	double value;
	if (JS_ToFloat64(ctx, &value, argv[0]))
		return JS_EXCEPTION;
	context->clear_depth = clamp01(value);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_clear_stencil(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int32_t value;
	if (JS_ToInt32(ctx, &value, argv[0]))
		return JS_EXCEPTION;
	context->clear_stencil = value;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_color_mask(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	for (int i = 0; i < 4; i++)
		context->color_mask[i] = JS_ToBool(ctx, argv[i]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_depth_mask(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	context->depth_mask = JS_ToBool(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t type;
	if (JS_ToUint32(ctx, &type, argv[0]))
		return JS_EXCEPTION;

	if (!is_shader_type(type)) {
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}

	nx_webgl_shader_t *shader = js_mallocz(ctx, sizeof(nx_webgl_shader_t));
	if (!shader)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_shader_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, shader);
		return obj;
	}

	shader->type = type;
	shader->info_log = js_strdup(ctx, "");
	JS_SetOpaque(obj, shader);
	return obj;
}

static JSValue nx_webgl_shader_source(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	const char *source = JS_ToCString(ctx, argv[1]);
	if (!source)
		return JS_EXCEPTION;
	replace_string(ctx, &shader->source, source);
	shader->compile_status = false;
	replace_string(ctx, &shader->info_log, "");
	JS_FreeCString(ctx, source);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_compile_shader(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if (!shader->source || shader->source[0] == '\0') {
		shader->compile_status = false;
		replace_string(ctx, &shader->info_log, "shader source is empty");
		return JS_UNDEFINED;
	}

	if (strstr(shader->source, "void main") == NULL) {
		shader->compile_status = false;
		replace_string(ctx, &shader->info_log,
					   "shader source does not contain void main");
		return JS_UNDEFINED;
	}

	// Scan for opt-in markers that promote this shader's program to the
	// raw-shader passthrough path (see nx_webgl_program_t.raw_passthrough).
	// A simple strstr is enough — GLSL ES doesn't have macros that could
	// rewrite the literal text, so any occurrence in the source is real.
	//
	//   - `#pragma raw_passthrough` — explicit opt-in (milestone #14, the
	//     original passthrough path for ShaderMaterial demos).
	//   - `#define USE_SHADOWMAP` — emitted by Three.js into shadow-receive
	//     materials' shader prefixes when `renderer.shadowMap.enabled &&
	//     shadows.length > 0`. The receive shaders pull in
	//     shadowmap_pars_fragment with `directionalShadowMap[]` samplers,
	//     `getShadow()` PCF kernel, `unpackRGBAToDepth()` — none of which
	//     the bridge's hardcoded programs know. See [[swb-threejs-webgl-shadowmap]].
	//   - `#define DEPTH_PACKING` — exclusive to MeshDepthMaterial's
	//     auto-shader (the shadow-cast pass writes RGBA-packed depth via
	//     `packDepthToRGBA(fragCoordZ)`).
	shader->raw_passthrough =
		strstr(shader->source, "#pragma raw_passthrough") != NULL ||
		strstr(shader->source, "#define USE_SHADOWMAP") != NULL ||
		strstr(shader->source, "#define DEPTH_PACKING") != NULL;

	char gles_log[2048];
	bool gles_status = false;
	if (nx_webgl_egl_compile_shader(context->egl, context->canvas, shader->type,
									shader->source, &shader->gles_handle,
									&gles_status, gles_log,
									sizeof(gles_log))) {
		shader->gles_compile_attempted = true;
		shader->compile_status = gles_status;
		replace_string(ctx, &shader->info_log, gles_log);
		return JS_UNDEFINED;
	}

	shader->compile_status = true;
	replace_string(ctx, &shader->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_shader_parameter(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[1]))
		return JS_EXCEPTION;

	switch (pname) {
	case GL_COMPILE_STATUS:
		return JS_NewBool(ctx, shader->compile_status);
	case GL_DELETE_STATUS:
		return JS_NewBool(ctx, shader->deleted);
	case GL_SHADER_TYPE:
		return JS_NewUint32(ctx, shader->type);
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_shader_info_log(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	return JS_NewString(ctx, shader->info_log ? shader->info_log : "");
}

static JSValue nx_webgl_delete_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[0]);
	if (!shader) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	shader->deleted = true;
	if (shader->gles_handle) {
		nx_webgl_egl_delete_shader(context->egl, shader->gles_handle);
		shader->gles_handle = 0;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_program(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = js_mallocz(ctx, sizeof(nx_webgl_program_t));
	if (!program)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_program_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, program);
		return obj;
	}

	program->vertex_shader = JS_UNDEFINED;
	program->fragment_shader = JS_UNDEFINED;
	program->info_log = js_strdup(ctx, "");
	for (int i = 0; i < 16; i++)
		program->matrix4[i] = i % 5 == 0 ? 1.f : 0.f;
	program->color[0] = 68.f / 255.f;
	program->color[1] = 215.f / 255.f;
	program->color[2] = 182.f / 255.f;
	program->color[3] = 1.f;
	program->sampler0 = 0;
	program->line_scale = 1.f;
	program->line_dash_size = 1.f;
	program->line_total_size = 2.f;
	program->fog_color[0] = 0.f;
	program->fog_color[1] = 0.f;
	program->fog_color[2] = 0.f;
	program->fog_near = 1.f;
	program->fog_far = 1000.f;
	program->light_direction[0] = 0.f;
	program->light_direction[1] = 0.f;
	program->light_direction[2] = 1.f;
	program->light_color[0] = 1.f;
	program->light_color[1] = 1.f;
	program->light_color[2] = 1.f;
	program->ambient_light_color[0] = 0.f;
	program->ambient_light_color[1] = 0.f;
	program->ambient_light_color[2] = 0.f;
	program->point_light_position[0] = 0.f;
	program->point_light_position[1] = 0.f;
	program->point_light_position[2] = 0.f;
	program->point_light_color[0] = 0.f;
	program->point_light_color[1] = 0.f;
	program->point_light_color[2] = 0.f;
	program->point_light_distance = 0.f;
	program->point_light_decay = 0.f;
	program->light_direction2[0] = 0.f;
	program->light_direction2[1] = 0.f;
	program->light_direction2[2] = 1.f;
	program->light_color2[0] = 0.f;
	program->light_color2[1] = 0.f;
	program->light_color2[2] = 0.f;
	program->fog_density = 0.f;
	// Default mapTransform = identity mat3 (column-major). Used as the
	// fallback when Three.js doesn't bind a texture transform — UVs flow
	// through unchanged so `texture.repeat`-less materials are unaffected.
	program->map_transform[0] = 1.f; program->map_transform[1] = 0.f; program->map_transform[2] = 0.f;
	program->map_transform[3] = 0.f; program->map_transform[4] = 1.f; program->map_transform[5] = 0.f;
	program->map_transform[6] = 0.f; program->map_transform[7] = 0.f; program->map_transform[8] = 1.f;
	// Sprite defaults — center at quad midpoint, no rotation, identity
	// model matrix. None of the `has_sprite_*` flags are set, so the bridge
	// only takes the sprite path when Three.js actually uploads these.
	for (int i = 0; i < 16; i++) program->model_matrix[i] = (i % 5 == 0) ? 1.f : 0.f;
	program->sprite_center[0] = 0.5f;
	program->sprite_center[1] = 0.5f;
	program->sprite_rotation = 0.f;
	program->line_distance_attrib_index = -1;
	program->color_attrib_index = -1;
	program->position_attrib_index = -1;
	program->uv_attrib_index = -1;
	program->normal_attrib_index = -1;
	JS_SetOpaque(obj, program);
	return obj;
}

static JSValue nx_webgl_attach_shader(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	nx_webgl_shader_t *shader = nx_get_webgl_shader(argv[1]);
	if (!program || !shader || program->deleted || shader->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	JSValue *target = shader->type == GL_VERTEX_SHADER
						  ? &program->vertex_shader
						  : &program->fragment_shader;
	JS_FreeValue(ctx, *target);
	*target = JS_DupValue(ctx, argv[1]);
	program->link_status = false;
	replace_string(ctx, &program->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_bind_attrib_location(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	int32_t location;
	if (JS_ToInt32(ctx, &location, argv[1]))
		return JS_EXCEPTION;
	if (location < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	const char *name = JS_ToCString(ctx, argv[2]);
	if (!name)
		return JS_EXCEPTION;
	// WebGL spec: rebinding the SAME name overwrites the old location.
	for (int i = 0; i < program->attrib_binding_count; i++) {
		if (strcmp(program->attrib_bindings[i].name, name) == 0) {
			program->attrib_bindings[i].location = location;
			JS_FreeCString(ctx, name);
			return JS_UNDEFINED;
		}
	}
	if (program->attrib_binding_count >= NX_WEBGL_MAX_ATTRIB_BINDINGS) {
		// No matching WebGL error for "too many bindings"; INVALID_VALUE
		// is the closest spec-defined option.
		context->error = GL_INVALID_VALUE;
		JS_FreeCString(ctx, name);
		return JS_UNDEFINED;
	}
	int i = program->attrib_binding_count++;
	program->attrib_bindings[i].location = location;
	program->attrib_bindings[i].name = js_strdup(ctx, name);
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_link_program(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_shader_t *vertex = nx_get_webgl_shader(program->vertex_shader);
	nx_webgl_shader_t *fragment = nx_get_webgl_shader(program->fragment_shader);
	if (!vertex || !fragment) {
		program->link_status = false;
		replace_string(ctx, &program->info_log,
					   "program requires vertex and fragment shaders");
		return JS_UNDEFINED;
	}

	if (!vertex->compile_status || !fragment->compile_status) {
		program->link_status = false;
		replace_string(ctx, &program->info_log,
					   "attached shaders must compile before linking");
		return JS_UNDEFINED;
	}

	// Propagate the `#pragma raw_passthrough` opt-in from either shader to
	// the program. Either side flipping it on is sufficient — Three.js
	// typically injects the pragma into both vertex and fragment sources,
	// but a vertex-only or fragment-only opt-in still asks for the
	// passthrough path. See [[bridge-raw-shader-passthrough]].
	program->raw_passthrough =
		vertex->raw_passthrough || fragment->raw_passthrough;

	if (vertex->gles_handle && fragment->gles_handle) {
		char gles_log[2048];
		bool gles_status = false;
		nx_webgl_attrib_binding_t bindings[NX_WEBGL_MAX_ATTRIB_BINDINGS];
		for (int i = 0; i < program->attrib_binding_count; i++) {
			bindings[i].location = program->attrib_bindings[i].location;
			bindings[i].name = program->attrib_bindings[i].name;
		}
		if (nx_webgl_egl_link_program(context->egl, context->canvas,
									  vertex->gles_handle,
									  fragment->gles_handle,
									  bindings,
									  program->attrib_binding_count,
									  &program->gles_handle, &gles_status,
									  gles_log, sizeof(gles_log))) {
			program->gles_link_attempted = true;
			program->link_status = gles_status;
			replace_string(ctx, &program->info_log, gles_log);
			return JS_UNDEFINED;
		}
	}

	program->link_status = true;
	replace_string(ctx, &program->info_log, "");
	return JS_UNDEFINED;
}

static JSValue nx_webgl_use_program(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0])) {
		JS_FreeValue(ctx, context->current_program);
		context->current_program = JS_UNDEFINED;
		if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl)) {
			nx_webgl_egl_use_native_program(context->egl, 0);
		}
		return JS_UNDEFINED;
	}

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	JS_FreeValue(ctx, context->current_program);
	context->current_program = JS_DupValue(ctx, argv[0]);
	// Bind the linked GLES program on the native side so subsequent
	// glUniform* calls (which operate on the currently-bound program)
	// land on the right program. Bridge dispatch internally re-binds its
	// own program around its draws, but at the end of each user-visible
	// gl.useProgram boundary the user's program is what we want bound.
	// For passthrough programs this is the actual draw-time program; for
	// bridge-mode programs the uniform location values still map to the
	// user program's uniform table.
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle) {
		nx_webgl_egl_use_native_program(context->egl, program->gles_handle);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_program_parameter(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[1]))
		return JS_EXCEPTION;

	switch (pname) {
	case GL_LINK_STATUS:
		return JS_NewBool(ctx, program->link_status);
	case GL_DELETE_STATUS:
		return JS_NewBool(ctx, program->deleted);
	case GL_ATTACHED_SHADERS: {
		uint32_t count = 0;
		if (!JS_IsUndefined(program->vertex_shader))
			count++;
		if (!JS_IsUndefined(program->fragment_shader))
			count++;
		return JS_NewUint32(ctx, count);
	}
	case GL_ACTIVE_UNIFORMS:
		// Forward to native GLES when we have a real linked program — Three.js
		// uses this count to iterate the actual shader's uniforms, which
		// dynamically reflects scene state (number of lights, fog mode, etc).
		// Hardcoded `active_uniforms` is a stale snapshot that doesn't match
		// any real shader and causes Three.js to crash when it iterates
		// uniforms with no corresponding state (e.g. pointLights[0] when
		// scene has no point lights). See `nxjs-active-uniforms-attribs-lists`.
		if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
			program->gles_handle) {
			int n = 0;
			if (nx_webgl_egl_get_program_iv(context->egl, program->gles_handle,
											GL_ACTIVE_UNIFORMS, &n))
				return JS_NewUint32(ctx, (uint32_t)n);
		}
		return JS_NewUint32(ctx, countof(active_uniforms));
	case GL_ACTIVE_ATTRIBUTES:
		if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
			program->gles_handle) {
			int n = 0;
			if (nx_webgl_egl_get_program_iv(context->egl, program->gles_handle,
											GL_ACTIVE_ATTRIBUTES, &n))
				return JS_NewUint32(ctx, (uint32_t)n);
		}
		return JS_NewUint32(ctx, countof(active_attributes));
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_program_info_log(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}

	return JS_NewString(ctx, program->info_log ? program->info_log : "");
}

static JSValue nx_webgl_get_active_attrib(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NULL;
	}

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[1]))
		return JS_EXCEPTION;
	// Forward to native GLES when a real program is linked — see
	// getProgramParameter for the rationale.
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle) {
		char name[256];
		int size = 0;
		uint32_t type = 0;
		if (nx_webgl_egl_get_active_attrib(context->egl, program->gles_handle,
										   index, name, sizeof(name), &size,
										   &type)) {
			nx_webgl_active_info_t info = {name, size, type};
			return new_active_info(ctx, &info);
		}
		return JS_NULL;
	}
	if (index >= countof(active_attributes))
		return JS_NULL;
	return new_active_info(ctx, &active_attributes[index]);
}

static JSValue nx_webgl_get_active_uniform(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NULL;
	}

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[1]))
		return JS_EXCEPTION;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle) {
		char name[256];
		int size = 0;
		uint32_t type = 0;
		if (nx_webgl_egl_get_active_uniform(context->egl, program->gles_handle,
											index, name, sizeof(name), &size,
											&type)) {
			nx_webgl_active_info_t info = {name, size, type};
			return new_active_info(ctx, &info);
		}
		return JS_NULL;
	}
	if (index >= countof(active_uniforms))
		return JS_NULL;
	return new_active_info(ctx, &active_uniforms[index]);
}

static JSValue nx_webgl_delete_program(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	program->deleted = true;
	if (program->gles_handle) {
		nx_webgl_egl_delete_program(context->egl, program->gles_handle);
		program->gles_handle = 0;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_buffer(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_buffer_t *buffer = js_mallocz(ctx, sizeof(nx_webgl_buffer_t));
	if (!buffer)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_buffer_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, buffer);
		return obj;
	}

	JS_SetOpaque(obj, buffer);
	return obj;
}

static JSValue nx_webgl_bind_buffer(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (JS_IsNull(argv[1])) {
		JSValue *binding = target == GL_ARRAY_BUFFER
							   ? &context->array_buffer_binding
							   : &context->element_array_buffer_binding;
		JS_FreeValue(ctx, *binding);
		*binding = JS_UNDEFINED;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(argv[1]);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (buffer->target != 0 && buffer->target != target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	buffer->target = target;
	JSValue *binding = target == GL_ARRAY_BUFFER
						   ? &context->array_buffer_binding
						   : &context->element_array_buffer_binding;
	JS_FreeValue(ctx, *binding);
	*binding = JS_DupValue(ctx, argv[1]);
	return JS_UNDEFINED;
}


static JSValue nx_webgl_buffer_data(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	uint32_t usage;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToUint32(ctx, &usage, argv[2]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (!is_buffer_usage(usage)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(target == GL_ARRAY_BUFFER
								? context->array_buffer_binding
								: context->element_array_buffer_binding);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	// WebIDL overload resolution for `bufferData(GLenum target,
	// [BufferDataSource? data | GLsizeiptr size], GLenum usage)`:
	//
	//   - null / undefined  -> BufferDataSource overload (nullable);
	//     WebGL spec override flags INVALID_VALUE.
	//   - ArrayBuffer / ArrayBufferView -> BufferDataSource overload.
	//   - Anything else -> GLsizeiptr overload: ToNumber-coerce per
	//     ECMAScript, truncate toward zero, reject negative as
	//     INVALID_VALUE, reject > 4GB as OUT_OF_MEMORY.
	//
	// JS_ToInt64 handles ToNumber for strings (numeric parse), arrays
	// (toString -> single-element parses, multi-element -> NaN -> 0),
	// and plain objects (ToPrimitive -> NaN -> 0). Matches Khronos's
	// buffer-data-and-buffer-sub-data WebIDL overload assertions.
	size_t size = 0;
	uint8_t *source = NULL;
	if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	source = NX_GetBufferSource(ctx, &size, argv[1]);
	if (!source) {
		int64_t requested_signed;
		if (JS_ToInt64(ctx, &requested_signed, argv[1]))
			return JS_EXCEPTION;
		if (requested_signed < 0) {
			context->error = GL_INVALID_VALUE;
			return JS_UNDEFINED;
		}
		if ((uint64_t)requested_signed > 0xFFFFFFFFu) {
			context->error = GL_OUT_OF_MEMORY;
			return JS_UNDEFINED;
		}
		size = (size_t)requested_signed;
	}

	if (size == 0) {
		js_free(ctx, buffer->data);
		buffer->data = NULL;
	} else if (buffer->data && buffer->size == size) {
		if (source)
			memcpy(buffer->data, source, size);
		else
			memset(buffer->data, 0, size);
	} else {
		uint8_t *copy = js_malloc(ctx, size);
		if (!copy)
			return JS_EXCEPTION;
		if (source)
			memcpy(copy, source, size);
		else
			memset(copy, 0, size);

		js_free(ctx, buffer->data);
		buffer->data = copy;
	}
	buffer->size = size;
	buffer->usage = usage;
	buffer->target = target;

	// Also mirror the upload to a native GLES buffer so the raw-shader
	// passthrough draw path can bind it directly. Bridge-mode draws still
	// read from `buffer->data` and ignore the native handle. Lazy-create
	// on first upload; reuse the handle on subsequent bufferData calls.
	// We intentionally do this AFTER the CPU copy so bridge-mode behavior
	// is unaffected if the native upload fails or the EGL backend isn't
	// available.
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl)) {
		if (!buffer->gles_handle) {
			buffer->gles_handle =
				nx_webgl_egl_create_native_buffer(context->egl, context->canvas);
		}
		if (buffer->gles_handle) {
			nx_webgl_egl_native_buffer_data(context->egl, buffer->gles_handle,
											target, size,
											size > 0 ? buffer->data : NULL,
											usage);
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_buffer_sub_data(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	int32_t offset;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToInt32(ctx, &offset, argv[1]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (offset < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(target == GL_ARRAY_BUFFER
								? context->array_buffer_binding
								: context->element_array_buffer_binding);
	if (!buffer || buffer->deleted || buffer->target != target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	// Per the WebGL/WebIDL spec, `bufferSubData(target, offset, data)`'s
	// `data` argument is a non-nullable `BufferSource`. WebIDL type
	// conversion fails synchronously with TypeError if `data` is null,
	// undefined, or any other non-BufferSource value. We reproduce that
	// behavior here so callers can rely on try/catch instead of needing
	// to poll getError(), and so we don't leak INVALID_VALUE into the
	// subsequent error queue from a programmer-error call site.
	size_t byte_length = 0;
	if (JS_IsNull(argv[2]) || JS_IsUndefined(argv[2])) {
		return JS_ThrowTypeError(ctx,
			"bufferSubData: 'data' argument must be a BufferSource (got %s)",
			JS_IsNull(argv[2]) ? "null" : "undefined");
	}
	uint8_t *source = NX_GetBufferSource(ctx, &byte_length, argv[2]);
	if (!source) {
		return JS_ThrowTypeError(ctx,
			"bufferSubData: 'data' argument must be an ArrayBuffer or ArrayBufferView");
	}

	size_t byte_offset = (size_t)offset;
	if (byte_offset > buffer->size || byte_length > buffer->size - byte_offset) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (byte_length > 0) {
		if (!buffer->data) {
			context->error = GL_INVALID_OPERATION;
			return JS_UNDEFINED;
		}
		memcpy(buffer->data + byte_offset, source, byte_length);

		// Mirror to the native GL buffer for the passthrough path. Same
		// rationale as bufferData. No-op when no handle was allocated yet
		// (the buffer was never bufferData'd, which is a programmer error
		// already caught by the !data check above).
		if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
			buffer->gles_handle) {
			nx_webgl_egl_native_buffer_sub_data(context->egl,
												buffer->gles_handle,
												target,
												byte_offset, byte_length,
												source);
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_delete_buffer(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(argv[0]);
	if (!buffer) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	buffer->deleted = true;
	if (buffer->gles_handle && context->egl) {
		nx_webgl_egl_delete_native_buffer(context->egl, buffer->gles_handle);
		buffer->gles_handle = 0;
	}
	if (nx_get_webgl_buffer(context->array_buffer_binding) == buffer) {
		JS_FreeValue(ctx, context->array_buffer_binding);
		context->array_buffer_binding = JS_UNDEFINED;
	}
	if (nx_get_webgl_buffer(context->element_array_buffer_binding) == buffer) {
		JS_FreeValue(ctx, context->element_array_buffer_binding);
		context->element_array_buffer_binding = JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_buffer_parameter(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	uint32_t pname;
	if (JS_ToUint32(ctx, &target, argv[0]) ||
		JS_ToUint32(ctx, &pname, argv[1]))
		return JS_EXCEPTION;
	if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
	if (pname != GL_BUFFER_SIZE && pname != GL_BUFFER_USAGE) {
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(target == GL_ARRAY_BUFFER
								? context->array_buffer_binding
								: context->element_array_buffer_binding);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_NULL;
	}
	if (pname == GL_BUFFER_SIZE)
		return JS_NewInt64(ctx, (int64_t)buffer->size);
	return JS_NewUint32(ctx, buffer->usage);
}

static JSValue nx_webgl_create_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_texture_t *texture = js_mallocz(ctx, sizeof(nx_webgl_texture_t));
	if (!texture)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_texture_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, texture);
		return obj;
	}

	texture->min_filter = GL_NEAREST;
	texture->mag_filter = GL_NEAREST;
	texture->wrap_s = GL_CLAMP_TO_EDGE;
	texture->wrap_t = GL_CLAMP_TO_EDGE;
	texture->bridge_id = context->next_texture_id++;
	if (texture->bridge_id == 0)
		texture->bridge_id = context->next_texture_id++;
	JS_SetOpaque(obj, texture);
	return obj;
}

static JSValue nx_webgl_active_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t texture;
	if (JS_ToUint32(ctx, &texture, argv[0]))
		return JS_EXCEPTION;
	if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + 8) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->active_texture = texture;
	// Mirror to native GL so that subsequent gl.bindTexture calls (also
	// forwarded) land on the right unit when the raw-shader passthrough
	// path samples a user-bound texture. Bridge-mode dispatch resets the
	// active unit to GL_TEXTURE0 for its own sampling, so this forwarding
	// is purely about keeping native state aligned with user state at
	// passthrough-dispatch time. See [[bridge-fbo-support]] +
	// [[bridge-raw-shader-passthrough]].
	nx_webgl_egl_forward_active_texture(context->egl, texture);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_bind_texture(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (!is_texture_binding_target(target)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	JSValue *binding = texture_binding_for_target(context, target);

	if (JS_IsNull(argv[1])) {
		JS_FreeValue(ctx, *binding);
		*binding = JS_UNDEFINED;
		// Mirror unbind to native so passthrough doesn't see a stale binding.
		nx_webgl_egl_forward_bind_texture(context->egl, target, 0);
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture = nx_get_webgl_texture(argv[1]);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (texture->target != 0 && texture->target != target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	texture->target = target;
	JS_FreeValue(ctx, *binding);
	*binding = JS_DupValue(ctx, argv[1]);
	// Forward to native GL when the texture has a persistent handle (FBO
	// color attachment or any texImage2D-allocated texture), so the
	// raw-shader passthrough path samples the right texture. Textures
	// without a persistent handle (legacy cache-only path) don't have a
	// native handle to bind — bridge dispatch manages those internally.
	if (texture->gles_handle)
		nx_webgl_egl_forward_bind_texture(context->egl, target,
		                                   texture->gles_handle);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_tex_parameteri(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	uint32_t pname;
	uint32_t param;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToUint32(ctx, &pname, argv[1]) ||
		JS_ToUint32(ctx, &param, argv[2]))
		return JS_EXCEPTION;
	if (!is_texture_binding_target(target)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	JSValue *binding = texture_binding_for_target(context, target);
	nx_webgl_texture_t *texture = nx_get_webgl_texture(*binding);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	switch (pname) {
	case GL_TEXTURE_MIN_FILTER:
	case GL_TEXTURE_MAG_FILTER:
		if (param != GL_NEAREST && param != GL_LINEAR) {
			context->error = GL_INVALID_ENUM;
			return JS_UNDEFINED;
		}
		if (pname == GL_TEXTURE_MIN_FILTER)
			texture->min_filter = param;
		else
			texture->mag_filter = param;
		return JS_UNDEFINED;
	case GL_TEXTURE_WRAP_S:
	case GL_TEXTURE_WRAP_T:
		if (param != GL_CLAMP_TO_EDGE && param != GL_REPEAT) {
			context->error = GL_INVALID_ENUM;
			return JS_UNDEFINED;
		}
		if (pname == GL_TEXTURE_WRAP_S)
			texture->wrap_s = param;
		else
			texture->wrap_t = param;
		return JS_UNDEFINED;
	default:
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
}

static void update_texture_alpha_rows(nx_webgl_texture_t *texture, int start_y,
									  int row_count) {
	if (!texture || !texture->data || !texture->alpha_min_x ||
		!texture->alpha_max_x)
		return;
	int end_y = start_y + row_count;
	if (start_y < 0)
		start_y = 0;
	if (end_y > (int)texture->height)
		end_y = (int)texture->height;
	for (int y = start_y; y < end_y; y++) {
		int min_x = (int)texture->width;
		int max_x = -1;
		for (int x = 0; x < (int)texture->width; x++) {
			uint8_t alpha =
				texture->data[((size_t)y * texture->width + (size_t)x) * 4 + 3];
			if (alpha != 0) {
				if (min_x == (int)texture->width)
					min_x = x;
				max_x = x;
			}
		}
		texture->alpha_min_x[y] = min_x;
		texture->alpha_max_x[y] = max_x;
	}
}

static JSValue nx_webgl_tex_image_2d(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	int32_t level;
	uint32_t internal_format;
	int32_t width;
	int32_t height;
	int32_t border;
	uint32_t format;
	uint32_t type;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToInt32(ctx, &level, argv[1]) ||
		JS_ToUint32(ctx, &internal_format, argv[2]) ||
		JS_ToInt32(ctx, &width, argv[3]) || JS_ToInt32(ctx, &height, argv[4]) ||
		JS_ToInt32(ctx, &border, argv[5]) || JS_ToUint32(ctx, &format, argv[6]) ||
		JS_ToUint32(ctx, &type, argv[7]))
		return JS_EXCEPTION;

	if (!is_texture_image_target(target)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	// Accept-list for (internalformat, format, type) tuples. WebGL 1 spec
	// requires internalformat == format for unsized formats. Milestone #19.5
	// widens this beyond the original RGBA+UNSIGNED_BYTE to also accept the
	// depth-texture combos that WEBGL_depth_texture / OES_depth_texture
	// enable. The dispatch path forwards (format, type) straight to native
	// glTexImage2D via persistent_texture_image_2d for the NULL-source case
	// (the FBO-attachment pattern Three.js's DepthTexture uses).
	bool is_rgba_unorm = (internal_format == GL_RGBA &&
	                      format == GL_RGBA && type == GL_UNSIGNED_BYTE);
	// Accept both unsized (GL_DEPTH_COMPONENT) and sized
	// (GL_DEPTH_COMPONENT16/24/32F) internalformats with format =
	// GL_DEPTH_COMPONENT. Three.js's WebGLTextures always passes the
	// SIZED form (`_gl.DEPTH_COMPONENT16` for WebGL 1, `_gl.DEPTH_COMPONENT24`
	// for WebGL 2), so rejecting that case was the bug behind the
	// "white canvas" / unwritten-depth-attachment symptom in milestone
	// #19.5 hw bring-up.
	bool is_depth_internal = (internal_format == GL_DEPTH_COMPONENT ||
	                          internal_format == GL_DEPTH_COMPONENT16 ||
	                          internal_format == GL_DEPTH_COMPONENT24 ||
	                          internal_format == GL_DEPTH_COMPONENT32F);
	bool is_depth = (is_depth_internal && format == GL_DEPTH_COMPONENT &&
	                 (type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT));
	bool is_depth_stencil_internal = (internal_format == GL_DEPTH_STENCIL ||
	                                   internal_format == GL_DEPTH24_STENCIL8);
	bool is_depth_stencil = (is_depth_stencil_internal &&
	                         format == GL_DEPTH_STENCIL &&
	                         type == GL_UNSIGNED_INT_24_8_WEBGL);
	if (!is_rgba_unorm && !is_depth && !is_depth_stencil) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (level != 0 || width <= 0 || height <= 0 || border != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	JSValue *binding = texture_binding_for_target(context, target);
	nx_webgl_texture_t *texture = nx_get_webgl_texture(*binding);
	if (!texture || texture->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	uint32_t texture_target = texture_object_target_for_image_target(target);
	if (texture->target != texture_target) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	size_t byte_length = 0;
	uint8_t *source = NULL;
	bool null_source = JS_IsNull(argv[8]) || JS_IsUndefined(argv[8]);
	if (!null_source)
		source = NX_GetBufferSource(ctx, &byte_length, argv[8]);
	size_t expected = (size_t)width * (size_t)height * 4;
	// Depth and depth-stencil textures may only be allocated with NULL data
	// per the WEBGL_depth_texture spec (no client-side pixel upload). Reject
	// non-null with INVALID_OPERATION to match the Khronos conformance
	// behavior; Three.js's DepthTexture path always passes NULL anyway.
	if (!null_source && (is_depth || is_depth_stencil)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (!null_source && (!source || byte_length < expected)) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if (texture_target == GL_TEXTURE_CUBE_MAP) {
		texture->width = width;
		texture->height = height;
		texture->revision++;
		if (texture->revision == 0)
			texture->revision = 1;
		return JS_UNDEFINED;
	}

	// NULL-source path (the FBO color-attachment init pattern Three.js's
	// WebGLRenderTarget uses). Allocate or refresh a persistent native GLES
	// texture so subsequent framebufferTexture2D can attach it AND the
	// bridge can sample it (after pass-1 writes pixels into it). No CPU-side
	// data buffer needed — sampling goes through `gles_handle` per
	// [[bridge-fbo-support]].
	if (null_source) {
		js_free(ctx, texture->data);
		js_free(ctx, texture->alpha_min_x);
		js_free(ctx, texture->alpha_max_x);
		texture->data = NULL;
		texture->alpha_min_x = NULL;
		texture->alpha_max_x = NULL;
		texture->width = width;
		texture->height = height;
		texture->target = target;
		if (texture->gles_handle == 0)
			texture->gles_handle = nx_webgl_egl_create_persistent_texture(
				context->egl, context->canvas);
		if (texture->gles_handle == 0) {
			context->error = GL_OUT_OF_MEMORY;
			return JS_UNDEFINED;
		}
		if (!nx_webgl_egl_persistent_texture_image_2d(
		        context->egl, texture->gles_handle, width, height,
		        internal_format, format, type, NULL,
		        texture->min_filter, texture->mag_filter,
		        texture->wrap_s, texture->wrap_t)) {
			context->error = GL_INVALID_OPERATION;
			return JS_UNDEFINED;
		}
		texture->revision++;
		if (texture->revision == 0)
			texture->revision = 1;
		return JS_UNDEFINED;
	}

	uint8_t *copy = js_malloc(ctx, expected);
	if (!copy)
		return JS_EXCEPTION;
	memcpy(copy, source, expected);

	int *alpha_min_x = js_malloc(ctx, sizeof(int) * (size_t)height);
	int *alpha_max_x = js_malloc(ctx, sizeof(int) * (size_t)height);
	if (!alpha_min_x || !alpha_max_x) {
		js_free(ctx, copy);
		js_free(ctx, alpha_min_x);
		js_free(ctx, alpha_max_x);
		return JS_EXCEPTION;
	}

	js_free(ctx, texture->data);
	js_free(ctx, texture->alpha_min_x);
	js_free(ctx, texture->alpha_max_x);
	texture->data = copy;
	texture->alpha_min_x = alpha_min_x;
	texture->alpha_max_x = alpha_max_x;
	texture->width = width;
	texture->height = height;
	texture->target = target;
	update_texture_alpha_rows(texture, 0, height);
	texture->revision++;
	if (texture->revision == 0)
		texture->revision = 1;
	// If the texture already has a persistent GLES handle (it was used as
	// an FBO attachment before, or NULL-source'd earlier), keep it in sync
	// with this CPU upload so subsequent FBO renders into it would see the
	// new texels — also the case where Three.js re-uploads texture data
	// after the WebGLRenderTarget has been created.
	if (texture->gles_handle != 0) {
		(void)nx_webgl_egl_persistent_texture_image_2d(
		    context->egl, texture->gles_handle, width, height,
		    internal_format, format, type, copy,
		    texture->min_filter, texture->mag_filter,
		    texture->wrap_s, texture->wrap_t);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_tex_sub_image_2d(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	int32_t level;
	int32_t xoffset;
	int32_t yoffset;
	int32_t width;
	int32_t height;
	uint32_t format;
	uint32_t type;
	if (JS_ToUint32(ctx, &target, argv[0]) || JS_ToInt32(ctx, &level, argv[1]) ||
		JS_ToInt32(ctx, &xoffset, argv[2]) ||
		JS_ToInt32(ctx, &yoffset, argv[3]) ||
		JS_ToInt32(ctx, &width, argv[4]) || JS_ToInt32(ctx, &height, argv[5]) ||
		JS_ToUint32(ctx, &format, argv[6]) || JS_ToUint32(ctx, &type, argv[7]))
		return JS_EXCEPTION;

	if (target != GL_TEXTURE_2D || format != GL_RGBA ||
		type != GL_UNSIGNED_BYTE) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (level != 0 || xoffset < 0 || yoffset < 0 || width < 0 || height < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture =
		nx_get_webgl_texture(context->texture_2d_binding);
	if (!texture || texture->deleted || !texture->data) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (xoffset + width > (int32_t)texture->width ||
		yoffset + height > (int32_t)texture->height) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (width == 0 || height == 0)
		return JS_UNDEFINED;

	size_t byte_length = 0;
	uint8_t *source = NX_GetBufferSource(ctx, &byte_length, argv[8]);
	size_t expected = (size_t)width * (size_t)height * 4;
	if (!source || byte_length < expected) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	for (int y = 0; y < height; y++) {
		uint8_t *dst =
			texture->data + (((size_t)yoffset + (size_t)y) * texture->width +
							 (size_t)xoffset) *
								4;
		memcpy(dst, source + (size_t)y * (size_t)width * 4,
			   (size_t)width * 4);
	}
	update_texture_alpha_rows(texture, yoffset, height);
	texture->revision++;
	if (texture->revision == 0)
		texture->revision = 1;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_pixel_storei(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t pname;
	int32_t param;
	if (JS_ToUint32(ctx, &pname, argv[0]) || JS_ToInt32(ctx, &param, argv[1]))
		return JS_EXCEPTION;

	switch (pname) {
	case GL_UNPACK_ALIGNMENT:
	case GL_PACK_ALIGNMENT:
		if (param != 1 && param != 2 && param != 4 && param != 8) {
			context->error = GL_INVALID_VALUE;
			return JS_UNDEFINED;
		}
		return JS_UNDEFINED;
	case GL_UNPACK_FLIP_Y_WEBGL:
	case GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		return JS_UNDEFINED;
	case GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		if ((uint32_t)param != GL_NONE &&
			(uint32_t)param != GL_BROWSER_DEFAULT_WEBGL) {
			context->error = GL_INVALID_VALUE;
			return JS_UNDEFINED;
		}
		return JS_UNDEFINED;
	default:
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
}

static JSValue nx_webgl_generate_mipmap(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (!is_texture_binding_target(target)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_delete_texture(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_texture_t *texture = nx_get_webgl_texture(argv[0]);
	if (!texture) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	texture->deleted = true;
	nx_webgl_egl_delete_cached_texture(context->egl, texture->bridge_id);
	if (texture->gles_handle) {
		nx_webgl_egl_delete_persistent_texture(context->egl,
		                                        texture->gles_handle);
		texture->gles_handle = 0;
	}
	if (nx_get_webgl_texture(context->texture_2d_binding) == texture) {
		JS_FreeValue(ctx, context->texture_2d_binding);
		context->texture_2d_binding = JS_UNDEFINED;
	}
	if (nx_get_webgl_texture(context->texture_cube_binding) == texture) {
		JS_FreeValue(ctx, context->texture_cube_binding);
		context->texture_cube_binding = JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_uniform_location(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NULL;
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NULL;
	}

	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name)
		return JS_EXCEPTION;

	nx_webgl_uniform_kind_t kind = uniform_kind_for_name(name);

	// Always try to populate the native GLES location when bridge is
	// active and the program has a real linked handle. Previously
	// `location->location` was left as the js_mallocz default (0) which
	// is wrong for any uniform not actually at GLES slot 0 — but the
	// bridge demos didn't notice because they use bridge-side state
	// stashing for rendering and the per-Three.js-program uniform
	// uploads are dead writes anyway.
	//
	// For tests with custom shader uniforms (no allowlist entry,
	// kind=UNKNOWN), we MUST forward to native GLES so the location is
	// addressable; otherwise getUniformLocation returns null and the
	// test can't proceed. Without this, raw WebGL conformance tests
	// using their own shader uniforms always fail at lookup time.
	int native_location = -1;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle) {
		native_location = nx_webgl_egl_get_uniform_location(
			context->egl, program->gles_handle, name);
	}

	// If neither the allowlist nor native GLES knows the name, return
	// null per WebGL spec for an absent uniform.
	if (kind == NX_WEBGL_UNIFORM_UNKNOWN && native_location < 0) {
		JS_FreeCString(ctx, name);
		return JS_NULL;
	}

	nx_webgl_uniform_location_t *location =
		js_mallocz(ctx, sizeof(nx_webgl_uniform_location_t));
	if (!location) {
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_uniform_location_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, location);
		JS_FreeCString(ctx, name);
		return obj;
	}

	location->program = JS_DupValue(ctx, argv[0]);
	location->name = js_strdup(ctx, name);
	location->kind = kind;
	location->location = native_location;  // -1 when not in native program
	JS_SetOpaque(obj, location);
	JS_FreeCString(ctx, name);
	return obj;
}

static bool get_uniform_program(nx_webgl_context_t *context,
								nx_webgl_uniform_location_t *location,
								nx_webgl_program_t **out_program) {
	if (!location)
		return false;
	nx_webgl_program_t *program = nx_get_webgl_program(location->program);
	if (!program || program->deleted || !program->link_status)
		return false;
	// WebGL spec: setting a uniform requires the location's program to
	// match the currently-bound program (gl.useProgram). Otherwise
	// INVALID_OPERATION. Callers treat a `false` return as "set the
	// error and abort"; signal cross-program-mismatch the same way.
	nx_webgl_program_t *current =
		nx_get_webgl_program(context->current_program);
	if (current != program)
		return false;
	*out_program = program;
	return true;
}

static JSValue nx_webgl_uniform2f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	double x;
	double y;
	if (JS_ToFloat64(ctx, &x, argv[1]) || JS_ToFloat64(ctx, &y, argv[2]))
		return JS_EXCEPTION;

	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform2f(context->egl, location->location, (float)x, (float)y);
	}
	if (location->kind == NX_WEBGL_UNIFORM_OFFSET) {
		program->offset[0] = (float)x;
		program->offset[1] = (float)y;
		program->has_offset = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_SPRITE_CENTER) {
		program->sprite_center[0] = (float)x;
		program->sprite_center[1] = (float)y;
		program->has_sprite_center = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform1f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	double value;
	if (JS_ToFloat64(ctx, &value, argv[1]))
		return JS_EXCEPTION;

	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform1f(context->egl, location->location, (float)value);
	}
	if (location->kind == NX_WEBGL_UNIFORM_OPACITY) {
		program->color[3] = (float)clamp01(value);
		program->has_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LINE_SCALE) {
		program->line_scale = (float)value;
		program->has_line_scale = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LINE_DASH_SIZE) {
		program->line_dash_size = (float)value;
		program->has_line_dash_size = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LINE_TOTAL_SIZE) {
		program->line_total_size = (float)value;
		program->has_line_total_size = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_FOG_NEAR) {
		program->fog_near = (float)value;
		program->has_fog_near = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_FOG_FAR) {
		program->fog_far = (float)value;
		program->has_fog_far = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_DISTANCE) {
		program->point_light_distance = (float)value;
		program->has_point_light_distance = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_DECAY) {
		program->point_light_decay = (float)value;
		program->has_point_light_decay = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_FOG_DENSITY) {
		program->fog_density = (float)value;
		program->has_fog_density = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_SPRITE_ROTATION) {
		program->sprite_rotation = (float)value;
		program->has_sprite_rotation = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_SIZE) {
		program->point_size = (float)value;
		program->has_point_size = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_SHININESS) {
		program->shininess = (float)value;
		program->has_shininess = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform3f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	float values[3];
	for (int i = 0; i < 3; i++) {
		double value;
		if (JS_ToFloat64(ctx, &value, argv[i + 1]))
			return JS_EXCEPTION;
		values[i] = (float)value;
	}

	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform3f(context->egl, location->location, values[0], values[1], values[2]);
	}
	if (location->kind == NX_WEBGL_UNIFORM_COLOR) {
		for (int i = 0; i < 3; i++) {
			program->color[i] = clamp01(values[i]);
		}
		if (!program->has_color)
			program->color[3] = 1.f;
		program->has_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_FOG_COLOR) {
		program->fog_color[0] = clamp01(values[0]);
		program->fog_color[1] = clamp01(values[1]);
		program->fog_color[2] = clamp01(values[2]);
		program->has_fog_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_DIRECTION) {
		// Three.js sends the view-space direction already; no clamp here
		// since direction components are signed unit-vector axes.
		program->light_direction[0] = values[0];
		program->light_direction[1] = values[1];
		program->light_direction[2] = values[2];
		program->has_light_direction = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_COLOR) {
		// Light color = baseColor × intensity from Three.js, in linear
		// space. Intensity values like `new DirectionalLight(0xfff, 3)`
		// yield channel values > 1 so we don't clamp here either.
		program->light_color[0] = values[0];
		program->light_color[1] = values[1];
		program->light_color[2] = values[2];
		program->has_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_AMBIENT_LIGHT_COLOR) {
		program->ambient_light_color[0] = values[0];
		program->ambient_light_color[1] = values[1];
		program->ambient_light_color[2] = values[2];
		program->has_ambient_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_POSITION) {
		// View-space position (Three.js does the world→view conversion
		// before upload). No clamp — positions are signed and can be any
		// magnitude.
		program->point_light_position[0] = values[0];
		program->point_light_position[1] = values[1];
		program->point_light_position[2] = values[2];
		program->has_point_light_position = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_COLOR) {
		program->point_light_color[0] = values[0];
		program->point_light_color[1] = values[1];
		program->point_light_color[2] = values[2];
		program->has_point_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_DIRECTION_1) {
		program->light_direction2[0] = values[0];
		program->light_direction2[1] = values[1];
		program->light_direction2[2] = values[2];
		program->has_light_direction2 = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_COLOR_1) {
		program->light_color2[0] = values[0];
		program->light_color2[1] = values[1];
		program->light_color2[2] = values[2];
		program->has_light_color2 = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_SPECULAR) {
		program->specular[0] = (float)clamp01(values[0]);
		program->specular[1] = (float)clamp01(values[1]);
		program->specular[2] = (float)clamp01(values[2]);
		program->has_specular = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_EMISSIVE) {
		program->emissive[0] = (float)clamp01(values[0]);
		program->emissive[1] = (float)clamp01(values[1]);
		program->emissive[2] = (float)clamp01(values[2]);
		program->has_emissive = true;
	}
	return JS_UNDEFINED;
}

// Forward declarations — definitions live further down with the other
// uniform-setter helpers.
static const float *uniform_array_or_buffer_floats(
	JSContext *ctx, JSValueConst obj, int min_elements, float *fallback,
	int fallback_capacity, int *out_count);
static const int32_t *uniform_array_or_buffer_ints(
	JSContext *ctx, JSValueConst obj, int min_elements, int32_t *fallback,
	int fallback_capacity, int *out_count);

static JSValue nx_webgl_uniform3fv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	// Accept either TypedArray/ArrayBuffer (the common case) OR a plain
	// JS Array — Three.js's WebGLLights uses `[r, g, b]` arrays for
	// `state.ambient` (`uniforms.ambientLightColor.value`), so without
	// the Array fallback ambient never reaches the bridge (and the call
	// silently sets GL_INVALID_VALUE = 0x501).
	float scratch[16];
	int count = 0;
	const float *source = uniform_array_or_buffer_floats(
		ctx, argv[1], 3, scratch, (int)(sizeof(scratch) / sizeof(scratch[0])),
		&count);
	if (!source || count <= 0 || count % 3 != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	// Forward to native GLES when bridge is active and the program has a
	// real linked program — needed for tests using custom shader
	// uniforms (unknown kind) to not silently drop their data.
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform3fv(context->egl, location->location,
								count / 3, source);
	}
	if (location->kind == NX_WEBGL_UNIFORM_COLOR) {
		memcpy(program->color, source, sizeof(float) * 3);
		if (!program->has_color)
			program->color[3] = 1.f;
		program->has_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_FOG_COLOR) {
		memcpy(program->fog_color, source, sizeof(float) * 3);
		program->has_fog_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_DIRECTION) {
		memcpy(program->light_direction, source, sizeof(float) * 3);
		program->has_light_direction = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_COLOR) {
		memcpy(program->light_color, source, sizeof(float) * 3);
		program->has_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_AMBIENT_LIGHT_COLOR) {
		memcpy(program->ambient_light_color, source, sizeof(float) * 3);
		program->has_ambient_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_POSITION) {
		memcpy(program->point_light_position, source, sizeof(float) * 3);
		program->has_point_light_position = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_POINT_LIGHT_COLOR) {
		memcpy(program->point_light_color, source, sizeof(float) * 3);
		program->has_point_light_color = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_DIRECTION_1) {
		memcpy(program->light_direction2, source, sizeof(float) * 3);
		program->has_light_direction2 = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_LIGHT_COLOR_1) {
		memcpy(program->light_color2, source, sizeof(float) * 3);
		program->has_light_color2 = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_SPECULAR) {
		program->specular[0] = (float)clamp01(source[0]);
		program->specular[1] = (float)clamp01(source[1]);
		program->specular[2] = (float)clamp01(source[2]);
		program->has_specular = true;
	} else if (location->kind == NX_WEBGL_UNIFORM_EMISSIVE) {
		program->emissive[0] = (float)clamp01(source[0]);
		program->emissive[1] = (float)clamp01(source[1]);
		program->emissive[2] = (float)clamp01(source[2]);
		program->has_emissive = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform4f(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	float values[4];
	for (int i = 0; i < 4; i++) {
		double value;
		if (JS_ToFloat64(ctx, &value, argv[i + 1]))
			return JS_EXCEPTION;
		values[i] = (float)value;
	}

	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform4f(context->egl, location->location, values[0], values[1], values[2], values[3]);
	}
	if (location->kind == NX_WEBGL_UNIFORM_COLOR) {
		for (int i = 0; i < 4; i++) {
			program->color[i] = clamp01(values[i]);
		}
		program->has_color = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform1i(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	int32_t value;
	if (JS_ToInt32(ctx, &value, argv[1]))
		return JS_EXCEPTION;

	// Forward to native GLES whenever the bridge has a real linked program
	// and the uniform has a valid native location. Required for
	// raw-shader passthrough programs (Three.js's auto-shaders) which
	// have many int/bool/sampler uniforms (e.g. `bool receiveShadow`,
	// `directionalShadowMap[N]` samplers bound to non-zero slots).
	// See [[nxjs-uniform1i-fix]] memory for the milestone-#20 backstory.
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform1i(context->egl, location->location, value);
	}

	// Bridge-mode legacy stashing: only the bridge's hardcoded sampler at
	// texture unit 0 is honored. Non-zero sampler slots reach native via
	// the forward above; the bridge dispatch's texture binding ignores them.
	if (location->kind == NX_WEBGL_UNIFORM_SAMPLER && value == 0) {
		program->sampler0 = value;
		program->has_sampler0 = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform_matrix4fv(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]))
		return JS_UNDEFINED;

	int transpose = JS_ToBool(ctx, argv[1]);
	if (transpose < 0)
		return JS_EXCEPTION;
	if (transpose) {
		// WebGL 1: transpose must be false.
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	nx_webgl_uniform_location_t *location =
		nx_get_webgl_uniform_location(argv[0]);
	nx_webgl_program_t *program = NULL;
	if (!get_uniform_program(context, location, &program)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	// Accept TypedArray/ArrayBuffer OR plain JS Array (Three.js's
	// `state.ambient` pattern); reject sizes that aren't a positive
	// multiple of 16 (the spec requires an integer number of mat4s).
	float scratch[16];
	int count = 0;
	const float *source = uniform_array_or_buffer_floats(
		ctx, argv[2], 16, scratch,
		(int)(sizeof(scratch) / sizeof(scratch[0])), &count);
	if (!source || count <= 0 || count % 16 != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform_matrix4fv(context->egl, location->location, false, source);
	}
	if (location->kind == NX_WEBGL_UNIFORM_MATRIX4 ||
		location->kind == NX_WEBGL_UNIFORM_PROJECTION_MATRIX ||
		location->kind == NX_WEBGL_UNIFORM_MODEL_VIEW_MATRIX) {
		if (location->kind == NX_WEBGL_UNIFORM_PROJECTION_MATRIX) {
			memcpy(program->projection_matrix, source, sizeof(float) * 16);
			program->has_projection_matrix = true;
		} else if (location->kind == NX_WEBGL_UNIFORM_MODEL_VIEW_MATRIX) {
			memcpy(program->model_view_matrix, source, sizeof(float) * 16);
			program->has_model_view_matrix = true;
		} else {
			memcpy(program->matrix4, source, sizeof(float) * 16);
			program->has_matrix4 = true;
		}
	} else if (location->kind == NX_WEBGL_UNIFORM_MODEL_MATRIX) {
		memcpy(program->model_matrix, source, sizeof(float) * 16);
		program->has_model_matrix = true;
	}
	return JS_UNDEFINED;
}

// Extract a float buffer from either a TypedArray/ArrayBuffer (preferred)
// or a plain JS Array. Three.js's WebGLLights stores some uniform values
// as plain `[r, g, b]` arrays (e.g. `state.ambient`), so uniform*v setters
// that only accept buffer sources silently fail with GL_INVALID_VALUE and
// the value never reaches the bridge. Returns the buffer pointer or NULL
// on failure; on success writes the element count to *out_count.
//
// `fallback` is scratch storage for the Array path. `fallback_capacity`
// is its element capacity. If the Array has more elements than capacity,
// returns NULL (caller should size scratch for the worst case).
static const float *uniform_array_or_buffer_floats(
	JSContext *ctx, JSValueConst obj, int min_elements, float *fallback,
	int fallback_capacity, int *out_count) {
	size_t byte_length = 0;
	uint8_t *src = NX_GetBufferSource(ctx, &byte_length, obj);
	if (src && byte_length >= (size_t)min_elements * sizeof(float)) {
		*out_count = (int)(byte_length / sizeof(float));
		return (const float *)src;
	}
	if (!JS_IsObject(obj))
		return NULL;
	JSValue len_val = JS_GetPropertyStr(ctx, obj, "length");
	uint32_t len = 0;
	int ok = !JS_ToUint32(ctx, &len, len_val);
	JS_FreeValue(ctx, len_val);
	if (!ok || (int)len < min_elements || (int)len > fallback_capacity)
		return NULL;
	for (uint32_t i = 0; i < len; i++) {
		JSValue elem = JS_GetPropertyUint32(ctx, obj, i);
		double v = 0.0;
		int conv_failed = JS_ToFloat64(ctx, &v, elem);
		JS_FreeValue(ctx, elem);
		if (conv_failed)
			return NULL;
		fallback[i] = (float)v;
	}
	*out_count = (int)len;
	return fallback;
}

// Same as above but for int32 uniform-array setters (uniform*iv family).
static const int32_t *uniform_array_or_buffer_ints(
	JSContext *ctx, JSValueConst obj, int min_elements, int32_t *fallback,
	int fallback_capacity, int *out_count) {
	size_t byte_length = 0;
	uint8_t *src = NX_GetBufferSource(ctx, &byte_length, obj);
	if (src && byte_length >= (size_t)min_elements * sizeof(int32_t)) {
		*out_count = (int)(byte_length / sizeof(int32_t));
		return (const int32_t *)src;
	}
	if (!JS_IsObject(obj))
		return NULL;
	JSValue len_val = JS_GetPropertyStr(ctx, obj, "length");
	uint32_t len = 0;
	int ok = !JS_ToUint32(ctx, &len, len_val);
	JS_FreeValue(ctx, len_val);
	if (!ok || (int)len < min_elements || (int)len > fallback_capacity)
		return NULL;
	for (uint32_t i = 0; i < len; i++) {
		JSValue elem = JS_GetPropertyUint32(ctx, obj, i);
		int32_t v = 0;
		int conv_failed = JS_ToInt32(ctx, &v, elem);
		JS_FreeValue(ctx, elem);
		if (conv_failed)
			return NULL;
		fallback[i] = v;
	}
	*out_count = (int)len;
	return fallback;
}

// Forwarders for the rest of the WebGL1 uniform-setter family. Three.js's
// WebGLUniforms.upload() picks a setter per uniform TYPE returned by
// gl.getActiveUniform — now that getActiveUniform forwards to native GLES
// (see nxjs-active-uniforms-attribs-lists memory), Three.js sees every
// uniform the linked shader declares and calls the matching setter. Any
// setter we don't expose throws "not a function" mid-render. These
// forwarders cover the common Phong/Lambert uniform types (mat3 for
// normalMatrix, ivec/float-array variants for misc uniforms).
//
// None of the names that currently land on these setters are in
// uniform_kind_for_name's allowlist, so getUniformLocation returns null
// and the early-null-check usually short-circuits. The native-GLES
// forward branch is kept for future demos that legitimately upload to
// an allowlisted uniform via these methods.

static bool uniform_setter_common(JSContext *ctx, JSValueConst this_val,
								  JSValueConst location_arg,
								  nx_webgl_context_t **out_context,
								  nx_webgl_uniform_location_t **out_location,
								  nx_webgl_program_t **out_program) {
	*out_context = nx_get_webgl_context(ctx, this_val);
	if (!*out_context)
		return false;
	if (JS_IsNull(location_arg)) {
		*out_location = NULL;
		*out_program = NULL;
		return true;
	}
	*out_location = nx_get_webgl_uniform_location(location_arg);
	if (!get_uniform_program(*out_context, *out_location, out_program)) {
		(*out_context)->error = GL_INVALID_OPERATION;
		return false;
	}
	return true;
}

// Shared body for the three uniformMatrix*fv setters. Validates:
//   - transpose flag (WebGL 1 rejects transpose=true with INVALID_VALUE)
//   - array source is a TypedArray/ArrayBuffer OR a plain JS Array
//   - element count is a positive multiple of `components` (e.g. 9 for mat3)
// Then forwards to native GLES through `backend_call`. Returns JS_UNDEFINED.
static JSValue uniform_matrix_fv_common(
	JSContext *ctx, JSValueConst this_val, JSValueConst *argv, int components,
	void (*backend_call)(nx_webgl_egl_t *, int, bool, const float *)) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	int transpose = JS_ToBool(ctx, argv[1]);
	if (transpose < 0)
		return JS_EXCEPTION;
	if (transpose) {
		// WebGL 1: transpose must be false. WebGL 2 allows true but the
		// nx.js bridge is WebGL-1-only today.
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (!location)
		return JS_UNDEFINED;
	float scratch[16];
	int count = 0;
	const float *source = uniform_array_or_buffer_floats(
		ctx, argv[2], components, scratch,
		(int)(sizeof(scratch) / sizeof(scratch[0])), &count);
	if (!source) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	// WebGL spec: length must be a positive multiple of `components`
	// (i.e. an integer number of matrices).
	if (count <= 0 || count % components != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		backend_call(context->egl, location->location, false, source);
	}
	// Bridge-side stash for known mat3 uniform kinds. Mirrors the mat4
	// stashing in nx_webgl_uniform_matrix4fv for projection/modelView/u_matrix.
	if (components == 9 &&
		location->kind == NX_WEBGL_UNIFORM_MAP_TRANSFORM) {
		memcpy(program->map_transform, source, sizeof(float) * 9);
		program->has_map_transform = true;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform_matrix3fv(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	return uniform_matrix_fv_common(ctx, this_val, argv, 9,
									nx_webgl_egl_uniform_matrix3fv);
}

static JSValue nx_webgl_uniform_matrix2fv(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	return uniform_matrix_fv_common(ctx, this_val, argv, 4,
									nx_webgl_egl_uniform_matrix2fv);
}

static JSValue uniform_fv_common(JSContext *ctx, JSValueConst this_val, int argc,
								 JSValueConst *argv, int components,
								 void (*backend_call)(nx_webgl_egl_t *, int,
													  int, const float *)) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	if (!location)
		return JS_UNDEFINED;
	// Bound the on-stack scratch buffer. 64 floats covers vec4 arrays of
	// length 16 (e.g. lightProbe SH coefficients are 9 vec3s = 27 floats);
	// callers passing larger arrays must use a TypedArray, not a JS Array.
	float scratch[64];
	int count = 0;
	const float *source = uniform_array_or_buffer_floats(
		ctx, argv[1], components, scratch,
		(int)(sizeof(scratch) / sizeof(scratch[0])), &count);
	if (!source) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	int element_count = count / components;
	if (element_count <= 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		backend_call(context->egl, location->location, element_count, source);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform1fv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_fv_common(ctx, this_val, argc, argv, 1, nx_webgl_egl_uniform1fv);
}

static JSValue nx_webgl_uniform2fv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_fv_common(ctx, this_val, argc, argv, 2, nx_webgl_egl_uniform2fv);
}

static JSValue nx_webgl_uniform4fv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_fv_common(ctx, this_val, argc, argv, 4, nx_webgl_egl_uniform4fv);
}

static JSValue nx_webgl_uniform2i(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	int32_t x, y;
	if (JS_ToInt32(ctx, &x, argv[1]) || JS_ToInt32(ctx, &y, argv[2]))
		return JS_EXCEPTION;
	if (!location)
		return JS_UNDEFINED;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform2i(context->egl, location->location, x, y);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform3i(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	int32_t x, y, z;
	if (JS_ToInt32(ctx, &x, argv[1]) || JS_ToInt32(ctx, &y, argv[2]) ||
		JS_ToInt32(ctx, &z, argv[3]))
		return JS_EXCEPTION;
	if (!location)
		return JS_UNDEFINED;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform3i(context->egl, location->location, x, y, z);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform4i(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	int32_t x, y, z, w;
	if (JS_ToInt32(ctx, &x, argv[1]) || JS_ToInt32(ctx, &y, argv[2]) ||
		JS_ToInt32(ctx, &z, argv[3]) || JS_ToInt32(ctx, &w, argv[4]))
		return JS_EXCEPTION;
	if (!location)
		return JS_UNDEFINED;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		nx_webgl_egl_uniform4i(context->egl, location->location, x, y, z, w);
	}
	return JS_UNDEFINED;
}

static JSValue uniform_iv_common(JSContext *ctx, JSValueConst this_val, int argc,
								 JSValueConst *argv, int components,
								 void (*backend_call)(nx_webgl_egl_t *, int,
													  int, const int *)) {
	nx_webgl_context_t *context;
	nx_webgl_uniform_location_t *location;
	nx_webgl_program_t *program;
	if (!uniform_setter_common(ctx, this_val, argv[0], &context, &location, &program))
		return context ? JS_UNDEFINED : JS_EXCEPTION;
	if (!location)
		return JS_UNDEFINED;
	int32_t scratch[64];
	int count = 0;
	const int32_t *source = uniform_array_or_buffer_ints(
		ctx, argv[1], components, scratch,
		(int)(sizeof(scratch) / sizeof(scratch[0])), &count);
	if (!source) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	int element_count = count / components;
	if (element_count <= 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle && location->location >= 0) {
		backend_call(context->egl, location->location, element_count,
					 (const int *)source);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_uniform1iv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_iv_common(ctx, this_val, argc, argv, 1, nx_webgl_egl_uniform1iv);
}

static JSValue nx_webgl_uniform2iv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_iv_common(ctx, this_val, argc, argv, 2, nx_webgl_egl_uniform2iv);
}

static JSValue nx_webgl_uniform3iv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_iv_common(ctx, this_val, argc, argv, 3, nx_webgl_egl_uniform3iv);
}

static JSValue nx_webgl_uniform4iv(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	return uniform_iv_common(ctx, this_val, argc, argv, 4, nx_webgl_egl_uniform4iv);
}

static JSValue nx_webgl_get_attrib_location(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	nx_webgl_program_t *program = nx_get_webgl_program(argv[0]);
	if (!program || program->deleted) {
		context->error = GL_INVALID_VALUE;
		return JS_NewInt32(ctx, -1);
	}
	if (!program->link_status) {
		context->error = GL_INVALID_OPERATION;
		return JS_NewInt32(ctx, -1);
	}

	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name)
		return JS_EXCEPTION;

	int32_t location = -1;
	if (context->egl && nx_webgl_egl_is_bridge_enabled(context->egl) &&
		program->gles_handle) {
		location = nx_webgl_egl_get_attrib_location(context->egl,
													program->gles_handle, name);
	} else {
		if (strcmp(name, "position") == 0 || strcmp(name, "a_position") == 0)
			location = 0;
		else if (strcmp(name, "color") == 0 || strcmp(name, "a_color") == 0)
			location = 1;
		else if (strcmp(name, "texcoord") == 0 || strcmp(name, "texCoord") == 0 ||
				 strcmp(name, "uv") == 0 || strcmp(name, "a_uv") == 0)
			location = 2;
		else if (strcmp(name, "lineDistance") == 0 ||
				 strcmp(name, "a_lineDistance") == 0)
			location = 3;
		else if (strcmp(name, "normal") == 0 || strcmp(name, "a_normal") == 0)
			location = 4;
	}

	if (location >= 0 && location < NX_WEBGL_MAX_VERTEX_ATTRIBS &&
		(strcmp(name, "lineDistance") == 0 ||
		 strcmp(name, "a_lineDistance") == 0)) {
		program->line_distance_attrib_index = location;
		program->has_line_distance_attrib_index = true;
	}
	if (location >= 0 && location < NX_WEBGL_MAX_VERTEX_ATTRIBS &&
		(strcmp(name, "color") == 0 || strcmp(name, "a_color") == 0)) {
		program->color_attrib_index = location;
		program->has_color_attrib_index = true;
	}
	if (location >= 0 && location < NX_WEBGL_MAX_VERTEX_ATTRIBS &&
		(strcmp(name, "position") == 0 || strcmp(name, "a_position") == 0)) {
		program->position_attrib_index = location;
		program->has_position_attrib_index = true;
	}
	if (location >= 0 && location < NX_WEBGL_MAX_VERTEX_ATTRIBS &&
		(strcmp(name, "uv") == 0 || strcmp(name, "a_uv") == 0 ||
		 strcmp(name, "texcoord") == 0 || strcmp(name, "texCoord") == 0)) {
		program->uv_attrib_index = location;
		program->has_uv_attrib_index = true;
	}
	if (location >= 0 && location < NX_WEBGL_MAX_VERTEX_ATTRIBS &&
		(strcmp(name, "normal") == 0 || strcmp(name, "a_normal") == 0)) {
		program->normal_attrib_index = location;
		program->has_normal_attrib_index = true;
	}

	JS_FreeCString(ctx, name);
	return JS_NewInt32(ctx, location);
}

static JSValue nx_webgl_enable_vertex_attrib_array(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[0]))
		return JS_EXCEPTION;
	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	context->vertex_attribs[index].enabled = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_disable_vertex_attrib_array(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	if (JS_ToUint32(ctx, &index, argv[0]))
		return JS_EXCEPTION;
	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	context->vertex_attribs[index].enabled = false;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_vertex_attrib_pointer(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	uint32_t size;
	uint32_t type;
	int normalized;
	int32_t stride;
	int32_t offset;
	if (JS_ToUint32(ctx, &index, argv[0]) ||
		JS_ToUint32(ctx, &size, argv[1]) || JS_ToUint32(ctx, &type, argv[2]) ||
		JS_ToBool(ctx, argv[3]) < 0 || JS_ToInt32(ctx, &stride, argv[4]) ||
		JS_ToInt32(ctx, &offset, argv[5]))
		return JS_EXCEPTION;
	normalized = JS_ToBool(ctx, argv[3]);

	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS || size < 1 || size > 4 ||
		stride < 0 || offset < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (!is_vertex_attrib_type(type)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *buffer =
		nx_get_webgl_buffer(context->array_buffer_binding);
	if (!buffer || buffer->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_vertex_attrib_t *attrib = &context->vertex_attribs[index];
	attrib->size = size;
	attrib->type = type;
	attrib->normalized = normalized != 0;
	attrib->stride = stride;
	attrib->offset = offset;
	JS_FreeValue(ctx, attrib->buffer);
	attrib->buffer = JS_DupValue(ctx, context->array_buffer_binding);
	return JS_UNDEFINED;
}

static bool read_attrib_vec2(nx_webgl_context_t *context,
							 nx_webgl_vertex_attrib_t *attrib,
							 int vertex_index, nx_webgl_vec2_t *out) {
	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(attrib->buffer);
	if (!buffer || buffer->deleted || !buffer->data)
		return false;

	int stride = attrib->stride == 0 ? attrib->size * (int)sizeof(float)
									 : attrib->stride;
	size_t offset = (size_t)attrib->offset + (size_t)vertex_index * stride;
	if (offset + sizeof(float) * 2 > buffer->size)
		return false;

	memcpy(&out->x, buffer->data + offset, sizeof(float));
	memcpy(&out->y, buffer->data + offset + sizeof(float), sizeof(float));
	return true;
}

static bool read_attrib_vec3(nx_webgl_context_t *context,
							 nx_webgl_vertex_attrib_t *attrib,
							 int vertex_index, nx_webgl_vec3_t *out) {
	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(attrib->buffer);
	if (!buffer || buffer->deleted || !buffer->data)
		return false;

	int stride = attrib->stride == 0 ? attrib->size * (int)sizeof(float)
									 : attrib->stride;
	size_t offset = (size_t)attrib->offset + (size_t)vertex_index * stride;
	if (offset + sizeof(float) * 2 > buffer->size)
		return false;

	memcpy(&out->x, buffer->data + offset, sizeof(float));
	memcpy(&out->y, buffer->data + offset + sizeof(float), sizeof(float));
	out->z = 0.f;
	if (attrib->size >= 3 && offset + sizeof(float) * 3 <= buffer->size)
		memcpy(&out->z, buffer->data + offset + sizeof(float) * 2,
			   sizeof(float));
	return true;
}

static bool read_attrib_float(nx_webgl_context_t *context,
							  nx_webgl_vertex_attrib_t *attrib,
							  int vertex_index, float *out) {
	nx_webgl_buffer_t *buffer = nx_get_webgl_buffer(attrib->buffer);
	if (!buffer || buffer->deleted || !buffer->data)
		return false;

	int stride = attrib->stride == 0 ? attrib->size * (int)sizeof(float)
									 : attrib->stride;
	size_t offset = (size_t)attrib->offset + (size_t)vertex_index * stride;
	if (offset + sizeof(float) > buffer->size)
		return false;

	memcpy(out, buffer->data + offset, sizeof(float));
	return true;
}

static nx_webgl_vec2_t clip_to_pixel(nx_webgl_context_t *context,
									 nx_webgl_vec2_t clip) {
	float vx = (float)context->viewport[0];
	float vy = (float)context->viewport[1];
	float vw = (float)context->viewport[2];
	float vh = (float)context->viewport[3];
	return (nx_webgl_vec2_t){
		vx + (clip.x * 0.5f + 0.5f) * vw,
		vy + (0.5f - clip.y * 0.5f) * vh,
	};
}

static nx_webgl_vec3_t transform_position3_depth(nx_webgl_program_t *program,
												 nx_webgl_vec3_t position) {
	nx_webgl_vec3_t out = position;
	if (program->has_projection_matrix && program->has_model_view_matrix) {
		float *mv = program->model_view_matrix;
		float x = position.x;
		float y = position.y;
		float z = position.z;
		float mx = mv[0] * x + mv[4] * y + mv[8] * z + mv[12];
		float my = mv[1] * x + mv[5] * y + mv[9] * z + mv[13];
		float mz = mv[2] * x + mv[6] * y + mv[10] * z + mv[14];
		float mw = mv[3] * x + mv[7] * y + mv[11] * z + mv[15];

		float *p = program->projection_matrix;
		float tx = p[0] * mx + p[4] * my + p[8] * mz + p[12] * mw;
		float ty = p[1] * mx + p[5] * my + p[9] * mz + p[13] * mw;
		float tz = p[2] * mx + p[6] * my + p[10] * mz + p[14] * mw;
		float tw = p[3] * mx + p[7] * my + p[11] * mz + p[15] * mw;
		if (tw != 0.f) {
			tx /= tw;
			ty /= tw;
			tz /= tw;
		}
		out.x = tx;
		out.y = ty;
		out.z = tz;
	} else if (program->has_matrix4) {
		float x = position.x;
		float y = position.y;
		float z = position.z;
		float *m = program->matrix4;
		float tx = m[0] * x + m[4] * y + m[8] * z + m[12];
		float ty = m[1] * x + m[5] * y + m[9] * z + m[13];
		float tz = m[2] * x + m[6] * y + m[10] * z + m[14];
		float tw = m[3] * x + m[7] * y + m[11] * z + m[15];
		if (tw != 0.f) {
			tx /= tw;
			ty /= tw;
			tz /= tw;
		}
		out.x = tx;
		out.y = ty;
		out.z = tz;
	}
	if (program->has_offset) {
		out.x += program->offset[0];
		out.y += program->offset[1];
	}
	return out;
}

// Computes view-space normal for a single vertex by multiplying the input
// normal by Three.js's `normalMatrix` = transpose(inverse(upper-left 3x3
// of modelView)). Uses the cofactor identity (cofactor = inverseTranspose
// × det) and skips the determinant divide because the bridge fragment
// shader normalizes the result anyway. Returns the input normal unchanged
// when no model-view matrix is present (e.g. matrix4-only fixed-pipeline
// path), which is fine for any demo that doesn't use lighting.
static nx_webgl_vec3_t compute_view_space_normal(nx_webgl_program_t *program,
												  nx_webgl_vec3_t normal) {
	if (!program->has_model_view_matrix)
		return normal;
	const float *mv = program->model_view_matrix;
	// Upper-left 3x3 of the column-major modelView. Elements mv[3], mv[7],
	// mv[11] (the translation row of column-major) are ignored.
	float a = mv[0], b = mv[4], c = mv[8];
	float d = mv[1], e = mv[5], f = mv[9];
	float g = mv[2], h = mv[6], k = mv[10];
	// Cofactor matrix entries (signed minors).
	float c00 = e * k - f * h;
	float c01 = -(d * k - f * g);
	float c02 = d * h - e * g;
	float c10 = -(b * k - c * h);
	float c11 = a * k - c * g;
	float c12 = -(a * h - b * g);
	float c20 = b * f - c * e;
	float c21 = -(a * f - c * d);
	float c22 = a * e - b * d;
	nx_webgl_vec3_t out;
	out.x = c00 * normal.x + c01 * normal.y + c02 * normal.z;
	out.y = c10 * normal.x + c11 * normal.y + c12 * normal.z;
	out.z = c20 * normal.x + c21 * normal.y + c22 * normal.z;
	return out;
}

// Computes view-space position for a single vertex via `mvPosition.xyz`.
// Used by point-light dispatch to feed per-fragment light direction
// (`pointLightPosition - vViewPosition` in view space). For the matrix4-
// only branch we don't have a separate modelView matrix — return the
// input position as a best-effort fallback (point lighting will be
// approximate or off).
static nx_webgl_vec3_t compute_view_space_position(
	nx_webgl_program_t *program, nx_webgl_vec3_t position) {
	if (!program->has_model_view_matrix)
		return position;
	const float *mv = program->model_view_matrix;
	nx_webgl_vec3_t out;
	out.x = mv[0] * position.x + mv[4] * position.y + mv[8] * position.z + mv[12];
	out.y = mv[1] * position.x + mv[5] * position.y + mv[9] * position.z + mv[13];
	out.z = mv[2] * position.x + mv[6] * position.y + mv[10] * position.z + mv[14];
	return out;
}

// Computes view-space fog depth (Three.js's `vFogDepth = -mvPosition.z`) for
// a single vertex. Used by the bridge dispatch sites to feed fog data per
// vertex to the bridge programs (which mix in `u_fogColor` per the
// scene.fog uniforms). For the `has_matrix4` branch we don't have a
// separate modelView matrix, so fog depth is undefined — return 0 which
// will saturate to no-fog with sensible near/far values.
static float compute_fog_depth(nx_webgl_program_t *program,
							   nx_webgl_vec3_t position) {
	if (program->has_projection_matrix && program->has_model_view_matrix) {
		float *mv = program->model_view_matrix;
		float mz = mv[2] * position.x + mv[6] * position.y +
				   mv[10] * position.z + mv[14];
		return -mz;
	}
	return 0.f;
}

static nx_webgl_vec2_t transform_position3(nx_webgl_program_t *program,
										   nx_webgl_vec3_t position) {
	nx_webgl_vec3_t out = transform_position3_depth(program, position);
	return (nx_webgl_vec2_t){out.x, out.y};
}

static uint32_t program_color(nx_webgl_program_t *program) {
	float *color = program->has_color ? program->color : (float[4]){
		68.f / 255.f,
		215.f / 255.f,
		182.f / 255.f,
		1.f,
	};
	uint8_t r = to_u8(color[0]);
	uint8_t g = to_u8(color[1]);
	uint8_t b = to_u8(color[2]);
	uint8_t a = to_u8(color[3]);
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) |
		   (uint32_t)b;
}

static int compare_triangle_depth_desc(const void *a, const void *b) {
	const nx_webgl_triangle_t *ta = (const nx_webgl_triangle_t *)a;
	const nx_webgl_triangle_t *tb = (const nx_webgl_triangle_t *)b;
	if (ta->z < tb->z)
		return 1;
	if (ta->z > tb->z)
		return -1;
	return 0;
}

static nx_webgl_texture_t *active_texture_for_program(nx_webgl_context_t *context,
													  nx_webgl_program_t *program) {
	if (!program->has_sampler0 || program->sampler0 != 0)
		return NULL;
	nx_webgl_texture_t *texture =
		nx_get_webgl_texture(context->texture_2d_binding);
	if (!texture || texture->deleted || !texture->data || texture->width == 0 ||
		texture->height == 0)
		return NULL;
	if (texture->width == 1 && texture->height == 1 && texture->data[0] == 0 &&
		texture->data[1] == 0 && texture->data[2] == 0 &&
		texture->data[3] == 0)
		return NULL;
	return texture;
}

static inline int texture_coord_to_int(float value, uint32_t wrap_mode,
									   uint32_t size) {
	if (wrap_mode == GL_REPEAT) {
		value = value - floorf(value);
		if (value < 0.f)
			value += 1.f;
	} else {
		if (value < 0.f)
			value = 0.f;
		else if (value > 1.f)
			value = 1.f;
	}

	int coord = (int)(value * (float)size);
	if (coord < 0)
		return 0;
	if (coord >= (int)size)
		return (int)size - 1;
	return coord;
}

static inline bool texture_pixel_is_transparent(nx_webgl_texture_t *texture,
												int x, int y) {
	return texture->alpha_min_x && texture->alpha_max_x &&
		   (x < texture->alpha_min_x[y] || x > texture->alpha_max_x[y]);
}

static inline uint32_t texture_pixel_rgba(nx_webgl_texture_t *texture, int x,
										  int y) {
	if (x < 0)
		x = 0;
	else if (x >= (int)texture->width)
		x = texture->width - 1;
	if (y < 0)
		y = 0;
	else if (y >= (int)texture->height)
		y = texture->height - 1;
	if (texture_pixel_is_transparent(texture, x, y))
		return 0;
	uint8_t *pixel = texture->data + ((size_t)y * texture->width + x) * 4;
	return ((uint32_t)pixel[3] << 24) | ((uint32_t)pixel[0] << 16) |
		   ((uint32_t)pixel[1] << 8) | (uint32_t)pixel[2];
}

static inline uint32_t texture_pixel_rgba_unchecked(nx_webgl_texture_t *texture,
													int x, int y) {
	if (texture_pixel_is_transparent(texture, x, y))
		return 0;
	uint8_t *pixel = texture->data + ((size_t)y * texture->width + x) * 4;
	return ((uint32_t)pixel[3] << 24) | ((uint32_t)pixel[0] << 16) |
		   ((uint32_t)pixel[1] << 8) | (uint32_t)pixel[2];
}

static inline uint32_t sample_texture_fast(nx_webgl_texture_t *texture, float u,
										   float v) {
	int x = texture_coord_to_int(u, texture->wrap_s, texture->width);
	int y = texture_coord_to_int(v, texture->wrap_t, texture->height);
	return texture_pixel_rgba(texture, x, y);
}

static inline uint32_t sample_texture_clamp_edge_fast(
	nx_webgl_texture_t *texture, float u, float v) {
	if (u < 0.f)
		u = 0.f;
	else if (u > 1.f)
		u = 1.f;
	if (v < 0.f)
		v = 0.f;
	else if (v > 1.f)
		v = 1.f;

	int x = (int)(u * (float)texture->width);
	int y = (int)(v * (float)texture->height);
	if (x >= (int)texture->width)
		x = (int)texture->width - 1;
	if (y >= (int)texture->height)
		y = (int)texture->height - 1;
	return texture_pixel_rgba_unchecked(texture, x, y);
}

static inline uint32_t blend_src_alpha_over(uint32_t dst, uint32_t src) {
	uint32_t src_a = (src >> 24) & 0xff;
	if (src_a == 0)
		return dst;
	if (src_a == 255)
		return src;
	uint8_t src_b = src & 0xff;
	uint8_t src_g = (src >> 8) & 0xff;
	uint8_t src_r = (src >> 16) & 0xff;
	uint8_t dst_b = dst & 0xff;
	uint8_t dst_g = (dst >> 8) & 0xff;
	uint8_t dst_r = (dst >> 16) & 0xff;
	uint8_t dst_a = (dst >> 24) & 0xff;
	uint32_t inv_a = 255 - src_a;
	uint8_t out_b = (uint8_t)((src_b * src_a + dst_b * inv_a + 127) / 255);
	uint8_t out_g = (uint8_t)((src_g * src_a + dst_g * inv_a + 127) / 255);
	uint8_t out_r = (uint8_t)((src_r * src_a + dst_r * inv_a + 127) / 255);
	uint8_t out_a = (uint8_t)(src_a + (dst_a * inv_a + 127) / 255);
	return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
		   ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

static inline uint32_t blend_pixel(uint32_t dst, uint32_t src,
								   uint32_t src_factor,
								   uint32_t dst_factor) {
	if (src_factor == GL_SRC_ALPHA && dst_factor == GL_ONE_MINUS_SRC_ALPHA)
		return blend_src_alpha_over(dst, src);
	if (src_factor == GL_ONE && dst_factor == GL_ZERO)
		return src;
	if (src_factor == GL_ZERO && dst_factor == GL_ONE)
		return dst;

	uint8_t src_b = src & 0xff;
	uint8_t src_g = (src >> 8) & 0xff;
	uint8_t src_r = (src >> 16) & 0xff;
	uint8_t src_a = (src >> 24) & 0xff;
	uint8_t dst_b = dst & 0xff;
	uint8_t dst_g = (dst >> 8) & 0xff;
	uint8_t dst_r = (dst >> 16) & 0xff;
	uint8_t dst_a = (dst >> 24) & 0xff;
	int sat = src_a < 255 - dst_a ? src_a : 255 - dst_a;
#define BLEND_FACTOR_VALUE(factor, src_c, dst_c)                               \
	((factor) == GL_ONE				  ? 255                                \
	 : (factor) == GL_ZERO			  ? 0                                  \
	 : (factor) == GL_SRC_ALPHA		  ? src_a                              \
	 : (factor) == GL_ONE_MINUS_SRC_ALPHA ? 255 - src_a                     \
	 : (factor) == GL_DST_ALPHA		  ? dst_a                              \
	 : (factor) == GL_ONE_MINUS_DST_ALPHA ? 255 - dst_a                     \
	 : (factor) == GL_SRC_COLOR		  ? (src_c)                            \
	 : (factor) == GL_ONE_MINUS_SRC_COLOR ? 255 - (src_c)                   \
	 : (factor) == GL_DST_COLOR		  ? (dst_c)                            \
	 : (factor) == GL_ONE_MINUS_DST_COLOR ? 255 - (dst_c)                   \
	 : (factor) == GL_SRC_ALPHA_SATURATE ? sat                              \
									  : 255)
#define BLEND_CHANNEL(src_c, dst_c)                                            \
	(uint8_t)clamp_int(                                                       \
		((src_c) * BLEND_FACTOR_VALUE(src_factor, (src_c), (dst_c)) +         \
		 (dst_c) * BLEND_FACTOR_VALUE(dst_factor, (src_c), (dst_c)) + 127) /  \
			255,                                                               \
		0, 255)
	uint8_t out_b = BLEND_CHANNEL(src_b, dst_b);
	uint8_t out_g = BLEND_CHANNEL(src_g, dst_g);
	uint8_t out_r = BLEND_CHANNEL(src_r, dst_r);
	uint8_t out_a = BLEND_CHANNEL(src_a, dst_a);
#undef BLEND_CHANNEL
#undef BLEND_FACTOR_VALUE
	return ((uint32_t)out_a << 24) | ((uint32_t)out_r << 16) |
		   ((uint32_t)out_g << 8) | (uint32_t)out_b;
}

static bool assign_axis_aligned_quad_corners(nx_webgl_vec2_t *p,
											 nx_webgl_vec2_t *uv,
											 nx_webgl_vec2_t *corner_uv,
											 float *min_x_out,
											 float *max_x_out,
											 float *min_y_out,
											 float *max_y_out) {
	float min_x = fminf(fminf(p[0].x, p[1].x), fminf(p[2].x, p[3].x));
	float max_x = fmaxf(fmaxf(p[0].x, p[1].x), fmaxf(p[2].x, p[3].x));
	float min_y = fminf(fminf(p[0].y, p[1].y), fminf(p[2].y, p[3].y));
	float max_y = fmaxf(fmaxf(p[0].y, p[1].y), fmaxf(p[2].y, p[3].y));
	if (max_x - min_x < 1.f || max_y - min_y < 1.f)
		return false;

	bool assigned[4] = {false, false, false, false};
	for (int i = 0; i < 4; i++) {
		bool left = fabsf(p[i].x - min_x) <= 1.f;
		bool right = fabsf(p[i].x - max_x) <= 1.f;
		bool top = fabsf(p[i].y - min_y) <= 1.f;
		bool bottom = fabsf(p[i].y - max_y) <= 1.f;
		int corner = -1;
		if (left && top)
			corner = 0;
		else if (right && top)
			corner = 1;
		else if (right && bottom)
			corner = 2;
		else if (left && bottom)
			corner = 3;
		if (corner < 0 || assigned[corner])
			return false;
		assigned[corner] = true;
		corner_uv[corner] = uv[i];
	}

	*min_x_out = min_x;
	*max_x_out = max_x;
	*min_y_out = min_y;
	*max_y_out = max_y;
	return true;
}

static bool load_indexed_quad(nx_webgl_context_t *context,
							  nx_webgl_program_t *program,
							  nx_webgl_vertex_attrib_t *position,
							  nx_webgl_vertex_attrib_t *texcoord,
							  uint16_t *indices, int quad_offset,
							  nx_webgl_vec2_t *p, nx_webgl_vec2_t *uv) {
	uint16_t i0 = indices[quad_offset + 0];
	uint16_t i1 = indices[quad_offset + 1];
	uint16_t i2 = indices[quad_offset + 2];
	uint16_t i3 = indices[quad_offset + 5];
	if (indices[quad_offset + 3] != i0 || indices[quad_offset + 4] != i2)
		return false;

	uint16_t vertex_indices[4] = {i0, i1, i2, i3};
	for (int i = 0; i < 4; i++) {
		nx_webgl_vec3_t position3;
		if (!read_attrib_vec3(context, position, vertex_indices[i], &position3) ||
			!read_attrib_vec2(context, texcoord, vertex_indices[i], &uv[i]))
			return false;
		nx_webgl_vec2_t clip = transform_position3(program, position3);
		p[i] = clip_to_pixel(context, clip);
	}
	return true;
}

static void rasterize_axis_aligned_textured_quad(
	nx_canvas_t *canvas, nx_webgl_texture_t *texture, nx_webgl_vec2_t *p,
	nx_webgl_vec2_t *uv, bool blend, uint32_t blend_src, uint32_t blend_dst) {
	nx_webgl_vec2_t corner_uv[4];
	float min_xf;
	float max_xf;
	float min_yf;
	float max_yf;
	if (!assign_axis_aligned_quad_corners(p, uv, corner_uv, &min_xf, &max_xf,
										  &min_yf, &max_yf))
		return;

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf) - 1, 0,
						  (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf) - 1, 0,
						  (int)canvas->height - 1);
	if (max_x < min_x || max_y < min_y)
		return;

	float width = max_xf - min_xf;
	float height = max_yf - min_yf;
	float inv_width = 1.f / width;
	float inv_height = 1.f / height;
	uint32_t *pixels = (uint32_t *)canvas->data;
	bool src_over = blend && blend_src == GL_SRC_ALPHA &&
					blend_dst == GL_ONE_MINUS_SRC_ALPHA;
	bool replace = !blend || (blend_src == GL_ONE && blend_dst == GL_ZERO);
	bool clamp_edge = texture->wrap_s == GL_CLAMP_TO_EDGE &&
					  texture->wrap_t == GL_CLAMP_TO_EDGE;

	for (int y = min_y; y <= max_y; y++) {
		float ty = ((float)y + 0.5f - min_yf) * inv_height;
		if (ty < 0.f)
			ty = 0.f;
		else if (ty > 1.f)
			ty = 1.f;
		nx_webgl_vec2_t left_uv = {
			corner_uv[0].x + (corner_uv[3].x - corner_uv[0].x) * ty,
			corner_uv[0].y + (corner_uv[3].y - corner_uv[0].y) * ty,
		};
		nx_webgl_vec2_t right_uv = {
			corner_uv[1].x + (corner_uv[2].x - corner_uv[1].x) * ty,
			corner_uv[1].y + (corner_uv[2].y - corner_uv[1].y) * ty,
		};

		int row_min_x = min_x;
		int row_max_x = max_x;
		bool row_constant_v = fabsf(left_uv.y - right_uv.y) <= 0.0001f;
		if (src_over && row_constant_v && texture->alpha_min_x &&
			texture->alpha_max_x) {
			int tex_y = texture_coord_to_int(left_uv.y, texture->wrap_t,
											 texture->height);
			int alpha_min = texture->alpha_min_x[tex_y];
			int alpha_max = texture->alpha_max_x[tex_y];
			if (alpha_max < alpha_min)
				continue;
			float u_span = right_uv.x - left_uv.x;
			if (texture->wrap_s == GL_CLAMP_TO_EDGE && fabsf(u_span) > 0.0001f) {
				float alpha_min_u = (float)alpha_min / (float)texture->width;
				float alpha_max_u =
					(float)(alpha_max + 1) / (float)texture->width;
				float x0 = min_xf + (alpha_min_u - left_uv.x) / u_span * width;
				float x1 = min_xf + (alpha_max_u - left_uv.x) / u_span * width;
				if (x1 < x0) {
					float tmp = x0;
					x0 = x1;
					x1 = tmp;
				}
				int clip_min_x = (int)floorf(x0);
				int clip_max_x = (int)ceilf(x1) - 1;
				if (clip_max_x < row_min_x || clip_min_x > row_max_x)
					continue;
				row_min_x = clamp_int(clip_min_x, row_min_x, row_max_x);
				row_max_x = clamp_int(clip_max_x, row_min_x, row_max_x);
			}
		}

		uint32_t *row_pixels = pixels + (size_t)y * canvas->width;
		float u_span = right_uv.x - left_uv.x;
		float v_span = right_uv.y - left_uv.y;
		float tx = ((float)row_min_x + 0.5f - min_xf) * inv_width;
		if (tx < 0.f)
			tx = 0.f;
		else if (tx > 1.f)
			tx = 1.f;
		float u = left_uv.x + u_span * tx;
		float v = left_uv.y + v_span * tx;
		float u_step = u_span * inv_width;
		float v_step = v_span * inv_width;
		for (int x = row_min_x; x <= row_max_x; x++) {
			uint32_t src = clamp_edge
							   ? sample_texture_clamp_edge_fast(texture, u, v)
							   : sample_texture_fast(texture, u, v);
			u += u_step;
			v += v_step;
			if (replace) {
				row_pixels[x] = src;
			} else if (src_over) {
				uint32_t alpha = src >> 24;
				if (alpha == 0)
					continue;
				if (alpha == 255)
					row_pixels[x] = src;
				else
					row_pixels[x] = blend_src_alpha_over(row_pixels[x], src);
			} else {
				row_pixels[x] =
					blend_pixel(row_pixels[x], src, blend_src, blend_dst);
			}
		}
	}
}

static bool draw_axis_aligned_textured_quads(
	JSContext *ctx, nx_webgl_context_t *context, nx_webgl_program_t *program,
	nx_webgl_vertex_attrib_t *position, nx_webgl_vertex_attrib_t *texcoord,
	nx_webgl_texture_t *texture, uint16_t *indices, int count, bool blend) {
	if (count % 6 != 0)
		return false;

	int quad_count = count / 6;
	nx_webgl_textured_quad_t stack_quads[128];
	nx_webgl_textured_quad_t *quads = stack_quads;
	if (quad_count > (int)countof(stack_quads)) {
		quads = js_malloc(ctx,
						  sizeof(nx_webgl_textured_quad_t) * (size_t)quad_count);
		if (!quads)
			return false;
	}

	for (int quad_index = 0; quad_index < quad_count; quad_index++) {
		nx_webgl_textured_quad_t *quad = &quads[quad_index];
		nx_webgl_vec2_t corner_uv[4];
		float min_x;
		float max_x;
		float min_y;
		float max_y;
		if (!load_indexed_quad(context, program, position, texcoord, indices,
							   quad_index * 6, quad->p, quad->uv) ||
			!assign_axis_aligned_quad_corners(quad->p, quad->uv, corner_uv,
											  &min_x, &max_x, &min_y, &max_y)) {
			if (quads != stack_quads)
				js_free(ctx, quads);
			return false;
		}
	}

	for (int quad_index = 0; quad_index < quad_count; quad_index++) {
		nx_webgl_textured_quad_t *quad = &quads[quad_index];
		rasterize_axis_aligned_textured_quad(
			context->canvas, texture, quad->p, quad->uv, blend, context->blend_src,
			context->blend_dst);
	}

	if (quads != stack_quads)
		js_free(ctx, quads);
	return true;
}

// Three.js's SpriteMaterial vertex shader does view-aligned-quad expansion
// per sprite — pulls modelView origin to view space, extracts world scale
// from modelMatrix column lengths, adds a rotated/scaled quad-corner offset,
// then projects. Our bridge_texture_program just expects pre-projected NDC
// vertices, so this helper does the sprite math CPU-side and routes through
// the existing textured-triangle bridge. Triggered by Three.js uploading the
// `center` + `rotation` uniforms — see uniform_kind_for_name.
static bool draw_sprite_bridge(JSContext *ctx, nx_webgl_context_t *context,
							   nx_webgl_program_t *program,
							   nx_webgl_vertex_attrib_t *position,
							   nx_webgl_vertex_attrib_t *texcoord,
							   nx_webgl_texture_t *texture,
							   uint16_t *indices, int count, bool blend) {
	if (!nx_webgl_egl_is_bridge_enabled(context->egl) || !texture ||
		!texture->data || texture->deleted || texture->revision == 0 ||
		texture->bridge_id == 0 || count <= 0 || count % 3 != 0)
		return false;
	if (!program->has_model_view_matrix || !program->has_projection_matrix)
		return false;

	int vertex_count = count;
	// Sprites draw a single quad (6 indices). Bound at 12 vertices (2
	// quads worth) so the stack buffer covers everything Three.js ever
	// sends through this path. Skip js_malloc per draw — there are
	// 200+ sprite draws per frame and the alloc overhead adds up.
	float clip_xyzuv_stack[12 * 5];
	if (vertex_count > 12) return false;
	float *clip_xyzuv = clip_xyzuv_stack;
	(void)ctx;

	const float *mv = program->model_view_matrix;
	const float *mm = program->has_model_matrix ? program->model_matrix : NULL;
	const float *pm = program->projection_matrix;
	// Sprite center in view space = modelViewMatrix * (0, 0, 0, 1) = the
	// translation column of the column-major matrix.
	float mvX = mv[12];
	float mvY = mv[13];
	float mvZ = mv[14];
	// World-space scale from modelMatrix columns 0 and 1 (the X and Y axes).
	// Fall back to (1, 1) when modelMatrix wasn't uploaded.
	float scaleX = 1.f;
	float scaleY = 1.f;
	if (mm) {
		scaleX = sqrtf(mm[0] * mm[0] + mm[1] * mm[1] + mm[2] * mm[2]);
		scaleY = sqrtf(mm[4] * mm[4] + mm[5] * mm[5] + mm[6] * mm[6]);
	}
	float cx = program->has_sprite_center ? program->sprite_center[0] : 0.5f;
	float cy = program->has_sprite_center ? program->sprite_center[1] : 0.5f;
	float rot = program->has_sprite_rotation ? program->sprite_rotation : 0.f;
	float cosR = cosf(rot);
	float sinR = sinf(rot);

	bool loaded = true;
	for (int i = 0; i < vertex_count; i++) {
		int idx = indices[i];
		nx_webgl_vec3_t pos3;
		nx_webgl_vec2_t uv;
		if (!read_attrib_vec3(context, position, idx, &pos3) ||
			!read_attrib_vec2(context, texcoord, idx, &uv)) {
			loaded = false;
			break;
		}
		float alignedX = (pos3.x - (cx - 0.5f)) * scaleX;
		float alignedY = (pos3.y - (cy - 0.5f)) * scaleY;
		float rotX = cosR * alignedX - sinR * alignedY;
		float rotY = sinR * alignedX + cosR * alignedY;
		float vx = mvX + rotX;
		float vy = mvY + rotY;
		float vz = mvZ;
		float cxw = pm[0] * vx + pm[4] * vy + pm[8] * vz + pm[12];
		float cyw = pm[1] * vx + pm[5] * vy + pm[9] * vz + pm[13];
		float czw = pm[2] * vx + pm[6] * vy + pm[10] * vz + pm[14];
		float cww = pm[3] * vx + pm[7] * vy + pm[11] * vz + pm[15];
		if (cww != 0.f) {
			cxw /= cww;
			cyw /= cww;
			czw /= cww;
		}
		int out = i * 5;
		clip_xyzuv[out + 0] = cxw;
		clip_xyzuv[out + 1] = cyw;
		clip_xyzuv[out + 2] = czw;
		clip_xyzuv[out + 3] = uv.x;
		clip_xyzuv[out + 4] = uv.y;
	}

	bool drew = false;
	if (loaded) {
		float zero3[3] = {0.f, 0.f, 0.f};
		drew = nx_webgl_egl_draw_textured_triangles_bridge(
			context->egl, context->canvas, clip_xyzuv, vertex_count,
			texture->bridge_id, texture->revision, (int)texture->width,
			(int)texture->height, texture->data, texture->gles_handle,
			texture->min_filter,
			texture->mag_filter, texture->wrap_s, texture->wrap_t, blend,
			context->blend_src, context->blend_dst, context->blend_src_alpha, context->blend_dst_alpha, context->viewport,
			(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
			context->scissor_box,
			(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
			NULL, false, zero3, 0.f, 0.f,  // fog disabled — sprites usually transparent
			NULL, false, zero3, zero3, zero3,
			NULL, false, zero3, zero3, 0.f, 0.f,
			false, zero3, zero3,
			false, 0.f,
			program->map_transform, program->has_map_transform,
			false, 0u,  // cull face DISABLED for sprites — sprite quads are always camera-facing, no winding concerns
			program->has_color ? program->color : NULL,
			false, NULL, 0.f, NULL,
			false);  // specular + emissive + derivative-normals DISABLED for sprites
	}
	return drew;
}

static bool draw_indexed_textured_triangles_bridge(
	JSContext *ctx, nx_webgl_context_t *context, nx_webgl_program_t *program,
	nx_webgl_vertex_attrib_t *position, nx_webgl_vertex_attrib_t *texcoord,
	nx_webgl_texture_t *texture, uint16_t *indices, int count, bool blend) {
	if (!nx_webgl_egl_is_bridge_enabled(context->egl) || !texture ||
		!texture->data || texture->deleted || texture->revision == 0 ||
		texture->bridge_id == 0 || count <= 0 || count % 3 != 0)
		return false;

	int vertex_count = count;
	bool fog_enabled = program->has_fog_color &&
					   ((program->has_fog_near && program->has_fog_far) ||
						program->has_fog_density);
	int normal_index = program->has_normal_attrib_index
						   ? program->normal_attrib_index
						   : 4;
	if (normal_index < 0 || normal_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		normal_index = 4;
	nx_webgl_vertex_attrib_t *normal_attr =
		&context->vertex_attribs[normal_index];
	bool has_normals = normal_attr->enabled &&
					   normal_attr->type == GL_FLOAT &&
					   normal_attr->size >= 3;
	bool has_directional = program->has_light_direction &&
						   program->has_light_color;
	bool has_point_light = program->has_point_light_position &&
						   program->has_point_light_color;
	// Lighting wants per-fragment normals; sourced from either the
	// per-vertex `a_normal` buffer OR (milestone #16) the bridge fragment
	// shader's view-position derivatives via OES_standard_derivatives.
	// The derivative path activates when lighting uniforms are bound but
	// the program has no `normal` attribute (Three.js's `flatShading: true`
	// optimizer drops it — [[bridge-flatshading-gap]]).
	bool has_lighting_uniforms = has_directional || has_point_light;
	bool use_derivative_normals = has_lighting_uniforms && !has_normals;
	bool lighting_enabled = has_lighting_uniforms;
	float *clip_xyzuv =
		js_malloc(ctx, (size_t)vertex_count * 5 * sizeof(float));
	if (!clip_xyzuv)
		return false;
	float *fog_depth_data = NULL;
	if (fog_enabled) {
		fog_depth_data =
			js_malloc(ctx, (size_t)vertex_count * sizeof(float));
		if (!fog_depth_data) {
			js_free(ctx, clip_xyzuv);
			return false;
		}
	}
	float *normal_data = NULL;
	float *view_position_data = NULL;
	if (lighting_enabled) {
		// Only allocate the per-vertex normal buffer when we actually have
		// `a_normal` to read from; derivative-normals computes N entirely
		// from `v_viewPosition` in the fragment shader.
		if (has_normals) {
			normal_data =
				js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
			if (!normal_data) {
				js_free(ctx, clip_xyzuv);
				js_free(ctx, fog_depth_data);
				return false;
			}
		}
		// Always populate view-position when lit. Point-light dispatch
		// needs it; specular (post-#11) needs it; derivative-normals
		// (milestone #16) needs it; and the bridge fragment shader's
		// `V = normalize(-v_viewPosition)` would otherwise pick up
		// garbage when light is on but no point light is in scene.
		view_position_data =
			js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
		if (!view_position_data) {
			js_free(ctx, clip_xyzuv);
			js_free(ctx, fog_depth_data);
			js_free(ctx, normal_data);
			return false;
		}
	}

	// Per-vertex "behind camera" flag. The bridge has no GPU near-plane
	// clipping (Three.js / GLES rely on GL's clip-space machinery, which
	// our CPU-side perspective divide bypasses). Vertices with pre-divide
	// `tw <= eps` lie at-or-behind the near plane; after the divide their
	// NDC values are reflected through the origin and the rasterizer
	// connects them to in-frustum vertices producing huge stretched
	// triangles — exactly the [[bridge-no-near-clip]] failure mode.
	// We compute tw inline (one extra dot product of the projection
	// matrix's row 3 with the model-view-transformed position), flag the
	// vertex, and post-process triangles to collapse any triangle touching
	// a behind-camera vertex to zero area. Costs ~7 muls/vertex for the
	// extra `tw` calc + an O(N) sweep at the end; small relative to the
	// dispatch.
	uint8_t *vertex_behind = NULL;
	bool have_clip_check = program->has_projection_matrix &&
						   program->has_model_view_matrix;
	if (have_clip_check) {
		vertex_behind = js_malloc(ctx, (size_t)vertex_count);
		if (!vertex_behind) {
			js_free(ctx, clip_xyzuv);
			js_free(ctx, fog_depth_data);
			js_free(ctx, normal_data);
			js_free(ctx, view_position_data);
			return false;
		}
	}
	bool loaded = true;
	for (int i = 0; i < vertex_count; i++) {
		nx_webgl_vec3_t position3;
		nx_webgl_vec2_t uv;
		if (!read_attrib_vec3(context, position, indices[i], &position3) ||
			!read_attrib_vec2(context, texcoord, indices[i], &uv)) {
			loaded = false;
			break;
		}
		nx_webgl_vec3_t clip = transform_position3_depth(program, position3);
		int out = i * 5;
		clip_xyzuv[out + 0] = clip.x;
		clip_xyzuv[out + 1] = clip.y;
		clip_xyzuv[out + 2] = clip.z;
		clip_xyzuv[out + 3] = uv.x;
		clip_xyzuv[out + 4] = uv.y;
		if (vertex_behind) {
			const float *mv = program->model_view_matrix;
			const float *p = program->projection_matrix;
			float mx = mv[0] * position3.x + mv[4] * position3.y +
					   mv[8] * position3.z + mv[12];
			float my = mv[1] * position3.x + mv[5] * position3.y +
					   mv[9] * position3.z + mv[13];
			float mz = mv[2] * position3.x + mv[6] * position3.y +
					   mv[10] * position3.z + mv[14];
			float mw = mv[3] * position3.x + mv[7] * position3.y +
					   mv[11] * position3.z + mv[15];
			float tw = p[3] * mx + p[7] * my + p[11] * mz + p[15] * mw;
			vertex_behind[i] = (tw <= 1e-4f) ? 1 : 0;
		}
		if (fog_depth_data)
			fog_depth_data[i] = compute_fog_depth(program, position3);
		if (normal_data) {
			nx_webgl_vec3_t n;
			if (!read_attrib_vec3(context, normal_attr, indices[i], &n)) {
				loaded = false;
				break;
			}
			nx_webgl_vec3_t vn = compute_view_space_normal(program, n);
			normal_data[i * 3 + 0] = vn.x;
			normal_data[i * 3 + 1] = vn.y;
			normal_data[i * 3 + 2] = vn.z;
		}
		if (view_position_data) {
			nx_webgl_vec3_t vp =
				compute_view_space_position(program, position3);
			view_position_data[i * 3 + 0] = vp.x;
			view_position_data[i * 3 + 1] = vp.y;
			view_position_data[i * 3 + 2] = vp.z;
		}
	}
	// Collapse triangles that touch any at-or-behind-near-plane vertex.
	if (loaded && vertex_behind) {
		for (int tri = 0; tri + 2 < vertex_count; tri += 3) {
			if (vertex_behind[tri] || vertex_behind[tri + 1] ||
				vertex_behind[tri + 2]) {
				for (int k = 0; k < 3; k++) {
					int out = (tri + k) * 5;
					clip_xyzuv[out + 0] = 0.f;
					clip_xyzuv[out + 1] = 0.f;
					clip_xyzuv[out + 2] = 0.f;
				}
			}
		}
	}

	bool drew = false;
	if (loaded) {
		float zero3[3] = {0.f, 0.f, 0.f};
		bool has_directional2 = program->has_light_direction2 &&
								program->has_light_color2;
		drew = nx_webgl_egl_draw_textured_triangles_bridge(
			context->egl, context->canvas, clip_xyzuv, vertex_count,
			texture->bridge_id, texture->revision, (int)texture->width,
			(int)texture->height, texture->data, texture->gles_handle,
			texture->min_filter,
			texture->mag_filter, texture->wrap_s, texture->wrap_t, blend,
			context->blend_src, context->blend_dst, context->blend_src_alpha, context->blend_dst_alpha, context->viewport,
			(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
			context->scissor_box,
			(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
			fog_enabled ? fog_depth_data : NULL,
			fog_enabled, program->fog_color, program->fog_near,
			program->fog_far,
			(lighting_enabled && has_normals) ? normal_data : NULL,
			lighting_enabled,
			has_directional ? program->light_direction : zero3,
			has_directional ? program->light_color : zero3,
			program->has_ambient_light_color
				? program->ambient_light_color
				: zero3,
			lighting_enabled ? view_position_data : NULL,
			has_point_light, program->point_light_position,
			program->point_light_color, program->point_light_distance,
			program->point_light_decay,
			has_directional2,
			has_directional2 ? program->light_direction2 : zero3,
			has_directional2 ? program->light_color2 : zero3,
			program->has_fog_density, program->fog_density,
			program->map_transform, program->has_map_transform,
			(context->enabled_caps & GL_CAP_CULL_FACE) != 0,
			context->cull_face,
			program->has_color ? program->color : NULL,
			program->has_specular && program->has_shininess,
			program->has_specular ? program->specular : NULL,
			program->has_shininess ? program->shininess : 30.f,
			program->has_emissive ? program->emissive : NULL,
			use_derivative_normals);
	}
	js_free(ctx, clip_xyzuv);
	js_free(ctx, fog_depth_data);
	js_free(ctx, normal_data);
	js_free(ctx, view_position_data);
	js_free(ctx, vertex_behind);
	return drew;
}

static bool draw_indexed_triangles_bridge(
	JSContext *ctx, nx_webgl_context_t *context, nx_webgl_program_t *program,
	nx_webgl_vertex_attrib_t *position, uint16_t *indices, int count,
	uint32_t color, bool blend) {
	if (!nx_webgl_egl_is_bridge_enabled(context->egl) || count <= 0 ||
		count % 3 != 0)
		return false;

	int vertex_count = count;
	int color_index = program->has_color_attrib_index
						  ? program->color_attrib_index
						  : 1;
	if (color_index < 0 || color_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		color_index = 1;
	nx_webgl_vertex_attrib_t *color_attr =
		&context->vertex_attribs[color_index];
	bool has_vertex_color = color_attr->enabled &&
							color_attr->type == GL_FLOAT &&
							color_attr->size >= 3;

	bool fog_enabled = program->has_fog_color &&
					   ((program->has_fog_near && program->has_fog_far) ||
						program->has_fog_density);
	int normal_index = program->has_normal_attrib_index
						   ? program->normal_attrib_index
						   : 4;
	if (normal_index < 0 || normal_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		normal_index = 4;
	nx_webgl_vertex_attrib_t *normal_attr =
		&context->vertex_attribs[normal_index];
	bool has_normals = normal_attr->enabled &&
					   normal_attr->type == GL_FLOAT &&
					   normal_attr->size >= 3;
	bool has_directional = program->has_light_direction &&
						   program->has_light_color;
	bool has_point_light = program->has_point_light_position &&
						   program->has_point_light_color;
	// See draw_indexed_textured_triangles_bridge for the derivative-normals
	// gating rationale (milestone #16).
	bool has_lighting_uniforms = has_directional || has_point_light;
	bool use_derivative_normals = has_lighting_uniforms && !has_normals;
	bool lighting_enabled = has_lighting_uniforms;
	float *clip_xyz =
		js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
	if (!clip_xyz)
		return false;
	float *vertex_color_data = NULL;
	if (has_vertex_color) {
		vertex_color_data =
			js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
		if (!vertex_color_data) {
			js_free(ctx, clip_xyz);
			return false;
		}
	}
	float *fog_depth_data = NULL;
	if (fog_enabled) {
		fog_depth_data =
			js_malloc(ctx, (size_t)vertex_count * sizeof(float));
		if (!fog_depth_data) {
			js_free(ctx, clip_xyz);
			js_free(ctx, vertex_color_data);
			return false;
		}
	}
	float *normal_data = NULL;
	float *view_position_data = NULL;
	if (lighting_enabled) {
		if (has_normals) {
			normal_data =
				js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
			if (!normal_data) {
				js_free(ctx, clip_xyz);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				return false;
			}
		}
		// Always populate view-position when lit (point-light, specular,
		// derivative-normals all need it).
		view_position_data =
			js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
		if (!view_position_data) {
			js_free(ctx, clip_xyz);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
			js_free(ctx, normal_data);
			return false;
		}
	}

	bool loaded = true;
	for (int i = 0; i < vertex_count; i++) {
		nx_webgl_vec3_t position3;
		if (!read_attrib_vec3(context, position, indices[i], &position3)) {
			loaded = false;
			break;
		}
		nx_webgl_vec3_t clip = transform_position3_depth(program, position3);
		clip_xyz[i * 3 + 0] = clip.x;
		clip_xyz[i * 3 + 1] = clip.y;
		clip_xyz[i * 3 + 2] = clip.z;
		if (fog_depth_data)
			fog_depth_data[i] = compute_fog_depth(program, position3);
		if (has_vertex_color) {
			nx_webgl_vec3_t col;
			if (!read_attrib_vec3(context, color_attr, indices[i], &col)) {
				loaded = false;
				break;
			}
			vertex_color_data[i * 3 + 0] = col.x;
			vertex_color_data[i * 3 + 1] = col.y;
			vertex_color_data[i * 3 + 2] = col.z;
		}
		if (normal_data) {
			nx_webgl_vec3_t n;
			if (!read_attrib_vec3(context, normal_attr, indices[i], &n)) {
				loaded = false;
				break;
			}
			nx_webgl_vec3_t vn = compute_view_space_normal(program, n);
			normal_data[i * 3 + 0] = vn.x;
			normal_data[i * 3 + 1] = vn.y;
			normal_data[i * 3 + 2] = vn.z;
		}
		if (view_position_data) {
			nx_webgl_vec3_t vp =
				compute_view_space_position(program, position3);
			view_position_data[i * 3 + 0] = vp.x;
			view_position_data[i * 3 + 1] = vp.y;
			view_position_data[i * 3 + 2] = vp.z;
		}
	}

	bool drew = false;
	if (loaded) {
		float gpu_color[4] = {
			(float)((color >> 16) & 0xff) / 255.f,
			(float)((color >> 8) & 0xff) / 255.f,
			(float)(color & 0xff) / 255.f,
			(float)((color >> 24) & 0xff) / 255.f,
		};
		float zero3[3] = {0.f, 0.f, 0.f};
		bool has_directional2 = program->has_light_direction2 &&
								program->has_light_color2;
		{
			// DIAGNOSTIC: dump dispatch state so JS can read via
			// gl.getBackendInfo().debugDispatchState.
			char dbg[512];
			snprintf(dbg, sizeof(dbg),
					 "hLD=%d hLC=%d hAL=%d hLD2=%d hLC2=%d hC=%d | "
					 "C=%.2f,%.2f,%.2f,%.2f | "
					 "L1=%.2f,%.2f,%.2f | L2=%.2f,%.2f,%.2f | "
					 "A=%.2f,%.2f,%.2f | LD1=%.2f,%.2f,%.2f | "
					 "LD2=%.2f,%.2f,%.2f | hd2=%d gpu=%02x%02x%02x%02x",
					 program->has_light_direction, program->has_light_color,
					 program->has_ambient_light_color,
					 program->has_light_direction2, program->has_light_color2,
					 program->has_color,
					 program->color[0], program->color[1],
					 program->color[2], program->color[3],
					 program->light_color[0], program->light_color[1],
					 program->light_color[2],
					 program->light_color2[0], program->light_color2[1],
					 program->light_color2[2],
					 program->ambient_light_color[0],
					 program->ambient_light_color[1],
					 program->ambient_light_color[2],
					 program->light_direction[0], program->light_direction[1],
					 program->light_direction[2],
					 program->light_direction2[0], program->light_direction2[1],
					 program->light_direction2[2],
					 has_directional2,
					 (color >> 16) & 0xff, (color >> 8) & 0xff,
					 color & 0xff, (color >> 24) & 0xff);
			nx_webgl_egl_set_dispatch_debug(context->egl, dbg);
		}
		drew = nx_webgl_egl_draw_triangles_bridge(
			context->egl, context->canvas, clip_xyz,
			has_vertex_color ? vertex_color_data : NULL, vertex_count,
			gpu_color, blend, context->blend_src, context->blend_dst,
			context->blend_src_alpha, context->blend_dst_alpha,
			context->viewport,
			(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
			context->scissor_box,
			(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
			fog_enabled ? fog_depth_data : NULL,
			fog_enabled, program->fog_color, program->fog_near,
			program->fog_far,
			(lighting_enabled && has_normals) ? normal_data : NULL,
			lighting_enabled,
			has_directional ? program->light_direction : zero3,
			has_directional ? program->light_color : zero3,
			program->has_ambient_light_color
				? program->ambient_light_color
				: zero3,
			lighting_enabled ? view_position_data : NULL,
			has_point_light, program->point_light_position,
			program->point_light_color, program->point_light_distance,
			program->point_light_decay,
			has_directional2,
			has_directional2 ? program->light_direction2 : zero3,
			has_directional2 ? program->light_color2 : zero3,
			program->has_fog_density, program->fog_density,
			(context->enabled_caps & GL_CAP_CULL_FACE) != 0,
			context->cull_face,
			program->has_specular && program->has_shininess,
			program->has_specular ? program->specular : NULL,
			program->has_shininess ? program->shininess : 30.f,
			program->has_emissive ? program->emissive : NULL,
			use_derivative_normals);
	}
	js_free(ctx, clip_xyz);
	js_free(ctx, vertex_color_data);
	js_free(ctx, fog_depth_data);
	js_free(ctx, normal_data);
	js_free(ctx, view_position_data);
	return drew;
}

// Software DDA line rasterizer.
// Dashes are applied when dashed is true: a pixel is drawn only when
//   fmod(scale * lineDistance, total_size) <= dash_size.
// lineDistance is linearly interpolated between endpoints.
// vc0/vc1 are optional per-vertex RGB colors; when colored == true the per-pixel
// color is the lerp of vc0/vc1 modulated by uniform_color (0..1).
static void rasterize_line(nx_webgl_context_t *context,
						   nx_canvas_t *canvas, nx_webgl_vec2_t p0,
						   nx_webgl_vec2_t p1, float d0, float d1,
						   bool dashed, float scale, float dash_size,
						   float total_size, uint32_t color, bool blend,
						   uint32_t blend_src, uint32_t blend_dst,
						   bool colored, const float *vc0, const float *vc1,
						   const float *uniform_color) {
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return;

	int clip_min_x = 0;
	int clip_min_y = 0;
	int clip_max_x = (int)canvas->width - 1;
	int clip_max_y = (int)canvas->height - 1;
	if ((context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0) {
		clip_min_x = clamp_int(context->scissor_box[0], 0,
							   (int)canvas->width - 1);
		clip_min_y = clamp_int(context->scissor_box[1], 0,
							   (int)canvas->height - 1);
		clip_max_x = clamp_int(context->scissor_box[0] +
								   context->scissor_box[2] - 1,
							   0, (int)canvas->width - 1);
		clip_max_y = clamp_int(context->scissor_box[1] +
								   context->scissor_box[3] - 1,
							   0, (int)canvas->height - 1);
	}
	if (clip_max_x < clip_min_x || clip_max_y < clip_min_y)
		return;

	float dx = p1.x - p0.x;
	float dy = p1.y - p0.y;
	float adx = fabsf(dx);
	float ady = fabsf(dy);
	float steps_f = adx > ady ? adx : ady;
	int steps = (int)ceilf(steps_f);
	uint32_t *pixels = (uint32_t *)canvas->data;
	int width = (int)canvas->width;
	float uniform_a = uniform_color ? uniform_color[3] : 1.f;
	float uniform_r = uniform_color ? uniform_color[0] : 1.f;
	float uniform_g = uniform_color ? uniform_color[1] : 1.f;
	float uniform_b = uniform_color ? uniform_color[2] : 1.f;

	if (steps <= 0) {
		int xi = (int)floorf(p0.x);
		int yi = (int)floorf(p0.y);
		if (xi < clip_min_x || xi > clip_max_x || yi < clip_min_y ||
			yi > clip_max_y)
			return;
		if (dashed && total_size > 0.f) {
			float fd = fmodf(scale * d0, total_size);
			if (fd < 0.f)
				fd += total_size;
			if (fd > dash_size)
				return;
		}
		uint32_t pixel_color = color;
		if (colored && vc0) {
			uint8_t r = to_u8(vc0[0] * uniform_r);
			uint8_t g = to_u8(vc0[1] * uniform_g);
			uint8_t b = to_u8(vc0[2] * uniform_b);
			uint8_t a = to_u8(uniform_a);
			pixel_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
						  ((uint32_t)g << 8) | (uint32_t)b;
		}
		size_t pi = (size_t)yi * canvas->width + xi;
		pixels[pi] = blend ? blend_pixel(pixels[pi], pixel_color, blend_src,
										 blend_dst)
						   : pixel_color;
		return;
	}

	float inv_steps = 1.f / (float)steps;
	float step_x = dx * inv_steps;
	float step_y = dy * inv_steps;
	float step_d = (d1 - d0) * inv_steps;
	float fx = p0.x;
	float fy = p0.y;
	float fd = d0;
	float inv_total = (dashed && total_size > 0.f) ? (1.f / total_size) : 0.f;
	float cr0 = 0.f, cg0 = 0.f, cb0 = 0.f, dcr = 0.f, dcg = 0.f, dcb = 0.f;
	if (colored && vc0 && vc1) {
		cr0 = vc0[0];
		cg0 = vc0[1];
		cb0 = vc0[2];
		dcr = (vc1[0] - vc0[0]) * inv_steps;
		dcg = (vc1[1] - vc0[1]) * inv_steps;
		dcb = (vc1[2] - vc0[2]) * inv_steps;
	}

	for (int i = 0; i <= steps; i++) {
		int xi = (int)floorf(fx + 0.5f);
		int yi = (int)floorf(fy + 0.5f);
		if (xi >= clip_min_x && xi <= clip_max_x && yi >= clip_min_y &&
			yi <= clip_max_y) {
			bool draw = true;
			if (dashed && total_size > 0.f) {
				float td = scale * fd;
				float m = td - floorf(td * inv_total) * total_size;
				if (m > dash_size)
					draw = false;
			}
			if (draw) {
				uint32_t pixel_color = color;
				if (colored && vc0 && vc1) {
					uint8_t r = to_u8(cr0 * uniform_r);
					uint8_t g = to_u8(cg0 * uniform_g);
					uint8_t b = to_u8(cb0 * uniform_b);
					uint8_t a = to_u8(uniform_a);
					pixel_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
								  ((uint32_t)g << 8) | (uint32_t)b;
				}
				size_t pi = (size_t)yi * width + xi;
				pixels[pi] = blend ? blend_pixel(pixels[pi], pixel_color,
												 blend_src, blend_dst)
								   : pixel_color;
			}
		}
		fx += step_x;
		fy += step_y;
		fd += step_d;
		cr0 += dcr;
		cg0 += dcg;
		cb0 += dcb;
	}
}

static void rasterize_triangle(nx_canvas_t *canvas, nx_webgl_vec2_t v0,
							   nx_webgl_vec2_t v1, nx_webgl_vec2_t v2,
							   uint32_t color, bool blend,
							   uint32_t blend_src, uint32_t blend_dst) {
	float area = edge_function(v0, v1, v2);
	if (area == 0.f)
		return;

	float min_xf = fminf(v0.x, fminf(v1.x, v2.x));
	float max_xf = fmaxf(v0.x, fmaxf(v1.x, v2.x));
	float min_yf = fminf(v0.y, fminf(v1.y, v2.y));
	float max_yf = fmaxf(v0.y, fmaxf(v1.y, v2.y));

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf), 0, (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf), 0, (int)canvas->height - 1);
	uint32_t *pixels = (uint32_t *)canvas->data;

	for (int y = min_y; y <= max_y; y++) {
		for (int x = min_x; x <= max_x; x++) {
			nx_webgl_vec2_t p = {(float)x + 0.5f, (float)y + 0.5f};
			float w0 = edge_function(v1, v2, p);
			float w1 = edge_function(v2, v0, p);
			float w2 = edge_function(v0, v1, p);
			if ((w0 >= 0.f && w1 >= 0.f && w2 >= 0.f) ||
				(w0 <= 0.f && w1 <= 0.f && w2 <= 0.f)) {
				size_t pixel_index = (size_t)y * canvas->width + x;
				pixels[pixel_index] = blend
										  ? blend_pixel(pixels[pixel_index],
														color, blend_src,
														blend_dst)
										  : color;
			}
		}
	}
}

static void rasterize_triangle_textured(nx_canvas_t *canvas,
										nx_webgl_vec2_t v0,
										nx_webgl_vec2_t v1,
										nx_webgl_vec2_t v2,
										nx_webgl_vec2_t uv0,
										nx_webgl_vec2_t uv1,
										nx_webgl_vec2_t uv2,
										nx_webgl_texture_t *texture,
										bool blend, uint32_t blend_src,
										uint32_t blend_dst) {
	float area = edge_function(v0, v1, v2);
	if (area == 0.f)
		return;
	float inv_area = 1.f / area;

	float min_xf = fminf(v0.x, fminf(v1.x, v2.x));
	float max_xf = fmaxf(v0.x, fmaxf(v1.x, v2.x));
	float min_yf = fminf(v0.y, fminf(v1.y, v2.y));
	float max_yf = fmaxf(v0.y, fmaxf(v1.y, v2.y));

	int min_x = clamp_int((int)floorf(min_xf), 0, (int)canvas->width - 1);
	int max_x = clamp_int((int)ceilf(max_xf), 0, (int)canvas->width - 1);
	int min_y = clamp_int((int)floorf(min_yf), 0, (int)canvas->height - 1);
	int max_y = clamp_int((int)ceilf(max_yf), 0, (int)canvas->height - 1);
	uint32_t *pixels = (uint32_t *)canvas->data;
	bool src_over = blend && blend_src == GL_SRC_ALPHA &&
					blend_dst == GL_ONE_MINUS_SRC_ALPHA;
	bool replace = !blend || (blend_src == GL_ONE && blend_dst == GL_ZERO);
	bool clamp_edge = texture->wrap_s == GL_CLAMP_TO_EDGE &&
					  texture->wrap_t == GL_CLAMP_TO_EDGE;

	float w0_dx = v2.y - v1.y;
	float w1_dx = v0.y - v2.y;
	float w2_dx = v1.y - v0.y;

	float u_dx = (uv0.x * w0_dx + uv1.x * w1_dx + uv2.x * w2_dx) *
				 inv_area;
	float v_dx = (uv0.y * w0_dx + uv1.y * w1_dx + uv2.y * w2_dx) *
				 inv_area;

	for (int y = min_y; y <= max_y; y++) {
		float scan_y = (float)y + 0.5f;
		float span_min_x;
		float span_max_x;
		if (!triangle_scanline_span(v0, v1, v2, scan_y, &span_min_x,
									&span_max_x))
			continue;

		int row_min_x =
			clamp_int((int)ceilf(span_min_x - 0.5f), min_x, max_x);
		int row_max_x =
			clamp_int((int)floorf(span_max_x - 0.5f), min_x, max_x);
		if (row_max_x < row_min_x)
			continue;

		nx_webgl_vec2_t row_start = {(float)row_min_x + 0.5f, scan_y};
		float row_w0 = edge_function(v1, v2, row_start);
		float row_w1 = edge_function(v2, v0, row_start);
		float row_w2 = edge_function(v0, v1, row_start);
		float row_u = (uv0.x * row_w0 + uv1.x * row_w1 + uv2.x * row_w2) *
					  inv_area;
		float row_v = (uv0.y * row_w0 + uv1.y * row_w1 + uv2.y * row_w2) *
					  inv_area;
		uint32_t *row_pixels = pixels + (size_t)y * canvas->width;

		float u = row_u;
		float v = row_v;
		for (int x = row_min_x; x <= row_max_x; x++) {
			uint32_t src =
				clamp_edge ? sample_texture_clamp_edge_fast(texture, u, v)
						   : sample_texture_fast(texture, u, v);
			if (replace) {
				row_pixels[x] = src;
			} else if (src_over) {
				uint32_t alpha = src >> 24;
				if (alpha == 0)
					goto next_pixel;
				if (alpha == 255) {
					row_pixels[x] = src;
				} else {
					row_pixels[x] = blend_src_alpha_over(row_pixels[x], src);
				}
			} else {
				row_pixels[x] =
					blend_pixel(row_pixels[x], src, blend_src, blend_dst);
			}
next_pixel:
			u += u_dx;
			v += v_dx;
		}
	}
}

// Build a flat LINES-pair vertex list from any line primitive mode.
// Returns the number of LINES-pair vertices written, or -1 on read failure.
// clip_xy: 2 floats per vertex (clip-space x/y).
// line_distance: 1 float per vertex when has_line_distance == true.
// vertex_colors: 3 floats per vertex when has_vertex_color == true.
// When indices != NULL the primitive walk reads through the index buffer
// (drawElements path); otherwise it uses sequential indices first..first+count
// (drawArrays path).
static int expand_lines_to_pairs(nx_webgl_context_t *context,
								 nx_webgl_program_t *program,
								 nx_webgl_vertex_attrib_t *position,
								 nx_webgl_vertex_attrib_t *line_distance_attr,
								 bool has_line_distance,
								 nx_webgl_vertex_attrib_t *color_attr,
								 bool has_vertex_color, uint32_t mode,
								 int32_t first, int32_t count,
								 const uint16_t *indices, float *clip_xyz,
								 float *line_distance, float *vertex_colors,
								 float *fog_depth) {
	int segment_count = 0;
	if (mode == GL_LINES)
		segment_count = count / 2;
	else if (mode == GL_LINE_STRIP)
		segment_count = count > 1 ? count - 1 : 0;
	else if (mode == GL_LINE_LOOP)
		segment_count = count > 1 ? count : 0;
	if (segment_count <= 0)
		return 0;

	int out_vertex = 0;
	for (int s = 0; s < segment_count; s++) {
		int i0;
		int i1;
		if (mode == GL_LINES) {
			i0 = indices ? (int)indices[s * 2]
						 : first + s * 2;
			i1 = indices ? (int)indices[s * 2 + 1]
						 : i0 + 1;
		} else if (mode == GL_LINE_STRIP) {
			i0 = indices ? (int)indices[s]
						 : first + s;
			i1 = indices ? (int)indices[s + 1]
						 : i0 + 1;
		} else {
			int s_next = (s + 1) % count;
			i0 = indices ? (int)indices[s]
						 : first + s;
			i1 = indices ? (int)indices[s_next]
						 : first + s_next;
		}

		nx_webgl_vec3_t pos0;
		nx_webgl_vec3_t pos1;
		if (!read_attrib_vec3(context, position, i0, &pos0) ||
			!read_attrib_vec3(context, position, i1, &pos1))
			return -1;
		nx_webgl_vec3_t c0 = transform_position3_depth(program, pos0);
		nx_webgl_vec3_t c1 = transform_position3_depth(program, pos1);
		if ((c0.z < -1.f && c1.z < -1.f) || (c0.z > 1.f && c1.z > 1.f))
			continue;

		clip_xyz[out_vertex * 3 + 0] = c0.x;
		clip_xyz[out_vertex * 3 + 1] = c0.y;
		clip_xyz[out_vertex * 3 + 2] = c0.z;
		clip_xyz[(out_vertex + 1) * 3 + 0] = c1.x;
		clip_xyz[(out_vertex + 1) * 3 + 1] = c1.y;
		clip_xyz[(out_vertex + 1) * 3 + 2] = c1.z;
		if (fog_depth) {
			fog_depth[out_vertex] = compute_fog_depth(program, pos0);
			fog_depth[out_vertex + 1] = compute_fog_depth(program, pos1);
		}
		if (has_line_distance && line_distance) {
			float d0 = 0.f;
			float d1 = 0.f;
			if (!read_attrib_float(context, line_distance_attr, i0, &d0) ||
				!read_attrib_float(context, line_distance_attr, i1, &d1))
				return -1;
			line_distance[out_vertex] = d0;
			line_distance[out_vertex + 1] = d1;
		}
		if (has_vertex_color && vertex_colors) {
			nx_webgl_vec3_t col0;
			nx_webgl_vec3_t col1;
			if (!read_attrib_vec3(context, color_attr, i0, &col0) ||
				!read_attrib_vec3(context, color_attr, i1, &col1))
				return -1;
			vertex_colors[out_vertex * 3 + 0] = col0.x;
			vertex_colors[out_vertex * 3 + 1] = col0.y;
			vertex_colors[out_vertex * 3 + 2] = col0.z;
			vertex_colors[(out_vertex + 1) * 3 + 0] = col1.x;
			vertex_colors[(out_vertex + 1) * 3 + 1] = col1.y;
			vertex_colors[(out_vertex + 1) * 3 + 2] = col1.z;
		}
		out_vertex += 2;
	}
	return out_vertex;
}

static void draw_arrays_lines(JSContext *ctx, nx_webgl_context_t *context,
							  nx_webgl_program_t *program, uint32_t mode,
							  int32_t first, int32_t count) {
	int position_index = program->has_position_attrib_index
							 ? program->position_attrib_index
							 : 0;
	if (position_index < 0 || position_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		position_index = 0;
	nx_webgl_vertex_attrib_t *position =
		&context->vertex_attribs[position_index];
	int line_distance_index = program->has_line_distance_attrib_index
								  ? program->line_distance_attrib_index
								  : 3;
	if (line_distance_index < 0 ||
		line_distance_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		line_distance_index = 3;
	nx_webgl_vertex_attrib_t *line_distance_attr =
		&context->vertex_attribs[line_distance_index];
	bool has_line_distance = line_distance_attr->enabled &&
							 line_distance_attr->type == GL_FLOAT;

	int color_index = program->has_color_attrib_index
						  ? program->color_attrib_index
						  : 1;
	if (color_index < 0 || color_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		color_index = 1;
	nx_webgl_vertex_attrib_t *color_attr =
		&context->vertex_attribs[color_index];
	bool has_vertex_color = color_attr->enabled &&
							color_attr->type == GL_FLOAT &&
							color_attr->size >= 3;

	bool dashed = program->has_line_total_size && program->line_total_size > 0.f &&
				  program->has_line_dash_size && has_line_distance;
	float scale = program->has_line_scale ? program->line_scale : 1.f;
	float dash_size = program->has_line_dash_size ? program->line_dash_size
												  : 1.f;
	float total_size = dashed && program->has_line_total_size
						   ? program->line_total_size
						   : 0.f;

	nx_canvas_t *canvas = context->canvas;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint32_t color = program_color(program);
	float fallback_color[4] = {
		68.f / 255.f,
		215.f / 255.f,
		182.f / 255.f,
		1.f,
	};
	float *uniform_color = program->has_color ? program->color
											  : fallback_color;

	int max_segment_count = 0;
	if (mode == GL_LINES)
		max_segment_count = count / 2;
	else if (mode == GL_LINE_STRIP)
		max_segment_count = count > 1 ? count - 1 : 0;
	else if (mode == GL_LINE_LOOP)
		max_segment_count = count > 1 ? count : 0;
	if (max_segment_count <= 0)
		return;

	int max_vertex_count = max_segment_count * 2;

	// Lines path supports linear Fog only (no exp2 — bridge_line_program
	// hasn't been extended for it). FogExp2 scenes will render lines
	// without fog rather than with wrong defaults.
	bool fog_enabled = program->has_fog_color && program->has_fog_near &&
					   program->has_fog_far;

	// Try GPU bridge first when enabled.
	if (nx_webgl_egl_is_bridge_enabled(context->egl)) {
		float *clip_xyz =
			js_malloc(ctx, (size_t)max_vertex_count * 3 * sizeof(float));
		float *line_distance_data = NULL;
		float *vertex_color_data = NULL;
		float *fog_depth_data = NULL;
		if (clip_xyz && has_line_distance) {
			line_distance_data =
				js_malloc(ctx, (size_t)max_vertex_count * sizeof(float));
		}
		if (clip_xyz && has_vertex_color) {
			vertex_color_data =
				js_malloc(ctx,
						  (size_t)max_vertex_count * 3 * sizeof(float));
		}
		if (clip_xyz && fog_enabled) {
			fog_depth_data =
				js_malloc(ctx, (size_t)max_vertex_count * sizeof(float));
		}
		if (clip_xyz && (!has_line_distance || line_distance_data) &&
			(!has_vertex_color || vertex_color_data) &&
			(!fog_enabled || fog_depth_data)) {
			int written = expand_lines_to_pairs(
				context, program, position, line_distance_attr,
				has_line_distance, color_attr, has_vertex_color, mode, first,
				count, NULL, clip_xyz, line_distance_data, vertex_color_data,
				fog_depth_data);
			if (written < 0) {
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				context->error = GL_INVALID_OPERATION;
				return;
			}
			if (written > 0) {
				bool drew = nx_webgl_egl_draw_lines_bridge(
					context->egl, canvas, clip_xyz,
					dashed ? line_distance_data : NULL,
					has_vertex_color ? vertex_color_data : NULL, written,
					uniform_color, scale, dash_size, total_size, blend,
					context->blend_src, context->blend_dst, context->blend_src_alpha, context->blend_dst_alpha, context->viewport,
					(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
					context->scissor_box,
					(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
					fog_enabled ? fog_depth_data : NULL,
					fog_enabled, program->fog_color, program->fog_near,
					program->fog_far);
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				if (drew) {
					context->bridge_clear_pending = false;
					return;
				}
			} else {
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				return;
			}
		} else {
			js_free(ctx, clip_xyz);
			js_free(ctx, line_distance_data);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
		}
	}

	// Software fallback path.
	flush_pending_bridge_clear_to_software(context);

	for (int s = 0; s < max_segment_count; s++) {
		int i0;
		int i1;
		if (mode == GL_LINES) {
			i0 = first + s * 2;
			i1 = i0 + 1;
		} else if (mode == GL_LINE_STRIP) {
			i0 = first + s;
			i1 = i0 + 1;
		} else {
			i0 = first + s;
			i1 = first + ((s + 1) % count);
		}

		nx_webgl_vec3_t pos0;
		nx_webgl_vec3_t pos1;
		if (!read_attrib_vec3(context, position, i0, &pos0) ||
			!read_attrib_vec3(context, position, i1, &pos1)) {
			context->error = GL_INVALID_OPERATION;
			return;
		}
		nx_webgl_vec3_t c0 = transform_position3_depth(program, pos0);
		nx_webgl_vec3_t c1 = transform_position3_depth(program, pos1);
		if ((c0.z < -1.f && c1.z < -1.f) || (c0.z > 1.f && c1.z > 1.f))
			continue;
		nx_webgl_vec2_t p0 = clip_to_pixel(
			context, (nx_webgl_vec2_t){c0.x, c0.y});
		nx_webgl_vec2_t p1 = clip_to_pixel(
			context, (nx_webgl_vec2_t){c1.x, c1.y});

		float d0 = 0.f;
		float d1 = 0.f;
		bool sw_dashed = dashed;
		if (sw_dashed) {
			if (!read_attrib_float(context, line_distance_attr, i0, &d0) ||
				!read_attrib_float(context, line_distance_attr, i1, &d1)) {
				sw_dashed = false;
			}
		}

		nx_webgl_vec3_t col0 = {1.f, 1.f, 1.f};
		nx_webgl_vec3_t col1 = {1.f, 1.f, 1.f};
		bool sw_colored = has_vertex_color;
		if (sw_colored) {
			if (!read_attrib_vec3(context, color_attr, i0, &col0) ||
				!read_attrib_vec3(context, color_attr, i1, &col1)) {
				sw_colored = false;
			}
		}

		float vc0[3] = {col0.x, col0.y, col0.z};
		float vc1[3] = {col1.x, col1.y, col1.z};
		rasterize_line(context, canvas, p0, p1, d0, d1, sw_dashed, scale,
					   dash_size, total_size, color, blend,
					   context->blend_src, context->blend_dst, sw_colored,
					   sw_colored ? vc0 : NULL, sw_colored ? vc1 : NULL,
					   uniform_color);
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

// Indexed-line dispatch used by gl.drawElements(LINES/LINE_STRIP/LINE_LOOP).
// Mirrors draw_arrays_lines but reads vertex indices from the bound element
// array buffer (Three.js's wireframe path generates this index buffer).
static void draw_elements_lines(JSContext *ctx, nx_webgl_context_t *context,
								nx_webgl_program_t *program,
								nx_webgl_vertex_attrib_t *position,
								uint32_t mode, const uint16_t *indices,
								int count) {
	int line_distance_index = program->has_line_distance_attrib_index
								  ? program->line_distance_attrib_index
								  : 3;
	if (line_distance_index < 0 ||
		line_distance_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		line_distance_index = 3;
	nx_webgl_vertex_attrib_t *line_distance_attr =
		&context->vertex_attribs[line_distance_index];
	bool has_line_distance = line_distance_attr->enabled &&
							 line_distance_attr->type == GL_FLOAT;

	int color_index = program->has_color_attrib_index
						  ? program->color_attrib_index
						  : 1;
	if (color_index < 0 || color_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		color_index = 1;
	nx_webgl_vertex_attrib_t *color_attr =
		&context->vertex_attribs[color_index];
	bool has_vertex_color = color_attr->enabled &&
							color_attr->type == GL_FLOAT &&
							color_attr->size >= 3;

	bool dashed = program->has_line_total_size && program->line_total_size > 0.f &&
				  program->has_line_dash_size && has_line_distance;
	float scale = program->has_line_scale ? program->line_scale : 1.f;
	float dash_size = program->has_line_dash_size ? program->line_dash_size
												  : 1.f;
	float total_size = dashed && program->has_line_total_size
						   ? program->line_total_size
						   : 0.f;

	nx_canvas_t *canvas = context->canvas;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint32_t color = program_color(program);
	float fallback_color[4] = {
		68.f / 255.f,
		215.f / 255.f,
		182.f / 255.f,
		1.f,
	};
	float *uniform_color = program->has_color ? program->color
											  : fallback_color;

	int max_segment_count = 0;
	if (mode == GL_LINES)
		max_segment_count = count / 2;
	else if (mode == GL_LINE_STRIP)
		max_segment_count = count > 1 ? count - 1 : 0;
	else if (mode == GL_LINE_LOOP)
		max_segment_count = count > 1 ? count : 0;
	if (max_segment_count <= 0)
		return;

	int max_vertex_count = max_segment_count * 2;

	// See draw_arrays_lines for why this stays linear-only.
	bool fog_enabled = program->has_fog_color && program->has_fog_near &&
					   program->has_fog_far;

	if (nx_webgl_egl_is_bridge_enabled(context->egl)) {
		float *clip_xyz =
			js_malloc(ctx, (size_t)max_vertex_count * 3 * sizeof(float));
		float *line_distance_data = NULL;
		float *vertex_color_data = NULL;
		float *fog_depth_data = NULL;
		if (clip_xyz && has_line_distance) {
			line_distance_data =
				js_malloc(ctx, (size_t)max_vertex_count * sizeof(float));
		}
		if (clip_xyz && has_vertex_color) {
			vertex_color_data =
				js_malloc(ctx,
						  (size_t)max_vertex_count * 3 * sizeof(float));
		}
		if (clip_xyz && fog_enabled) {
			fog_depth_data =
				js_malloc(ctx, (size_t)max_vertex_count * sizeof(float));
		}
		if (clip_xyz && (!has_line_distance || line_distance_data) &&
			(!has_vertex_color || vertex_color_data) &&
			(!fog_enabled || fog_depth_data)) {
			int written = expand_lines_to_pairs(
				context, program, position, line_distance_attr,
				has_line_distance, color_attr, has_vertex_color, mode,
				0, count, indices, clip_xyz, line_distance_data,
				vertex_color_data, fog_depth_data);
			if (written < 0) {
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				context->error = GL_INVALID_OPERATION;
				return;
			}
			if (written > 0) {
				bool drew = nx_webgl_egl_draw_lines_bridge(
					context->egl, canvas, clip_xyz,
					dashed ? line_distance_data : NULL,
					has_vertex_color ? vertex_color_data : NULL, written,
					uniform_color, scale, dash_size, total_size, blend,
					context->blend_src, context->blend_dst, context->blend_src_alpha, context->blend_dst_alpha, context->viewport,
					(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
					context->scissor_box,
					(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
					fog_enabled ? fog_depth_data : NULL,
					fog_enabled, program->fog_color, program->fog_near,
					program->fog_far);
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				if (drew) {
					context->bridge_clear_pending = false;
					return;
				}
			} else {
				js_free(ctx, clip_xyz);
				js_free(ctx, line_distance_data);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				return;
			}
		} else {
			js_free(ctx, clip_xyz);
			js_free(ctx, line_distance_data);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
		}
	}

	// Software fallback.
	flush_pending_bridge_clear_to_software(context);

	for (int s = 0; s < max_segment_count; s++) {
		int i0;
		int i1;
		if (mode == GL_LINES) {
			i0 = indices[s * 2];
			i1 = indices[s * 2 + 1];
		} else if (mode == GL_LINE_STRIP) {
			i0 = indices[s];
			i1 = indices[s + 1];
		} else {
			int s_next = (s + 1) % count;
			i0 = indices[s];
			i1 = indices[s_next];
		}

		nx_webgl_vec3_t pos0;
		nx_webgl_vec3_t pos1;
		if (!read_attrib_vec3(context, position, i0, &pos0) ||
			!read_attrib_vec3(context, position, i1, &pos1)) {
			context->error = GL_INVALID_OPERATION;
			return;
		}
		nx_webgl_vec3_t c0 = transform_position3_depth(program, pos0);
		nx_webgl_vec3_t c1 = transform_position3_depth(program, pos1);
		if ((c0.z < -1.f && c1.z < -1.f) || (c0.z > 1.f && c1.z > 1.f))
			continue;
		nx_webgl_vec2_t p0 = clip_to_pixel(
			context, (nx_webgl_vec2_t){c0.x, c0.y});
		nx_webgl_vec2_t p1 = clip_to_pixel(
			context, (nx_webgl_vec2_t){c1.x, c1.y});

		float d0 = 0.f;
		float d1 = 0.f;
		bool sw_dashed = dashed;
		if (sw_dashed) {
			if (!read_attrib_float(context, line_distance_attr, i0, &d0) ||
				!read_attrib_float(context, line_distance_attr, i1, &d1)) {
				sw_dashed = false;
			}
		}

		nx_webgl_vec3_t col0 = {1.f, 1.f, 1.f};
		nx_webgl_vec3_t col1 = {1.f, 1.f, 1.f};
		bool sw_colored = has_vertex_color;
		if (sw_colored) {
			if (!read_attrib_vec3(context, color_attr, i0, &col0) ||
				!read_attrib_vec3(context, color_attr, i1, &col1)) {
				sw_colored = false;
			}
		}

		float vc0[3] = {col0.x, col0.y, col0.z};
		float vc1[3] = {col1.x, col1.y, col1.z};
		rasterize_line(context, canvas, p0, p1, d0, d1, sw_dashed, scale,
					   dash_size, total_size, color, blend,
					   context->blend_src, context->blend_dst, sw_colored,
					   sw_colored ? vc0 : NULL, sw_colored ? vc1 : NULL,
					   uniform_color);
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

// Expand GL_POINTS to screen-aligned quads in NDC space and dispatch
// through the bridge triangle path. Each input point becomes 6 vertices
// (two triangles) sized so the output appears as a small square ~2 pixels
// wide in the bound viewport. Required because GLES2 GL_POINTS is a
// separate primitive type the bridge doesn't natively support, and
// Three.js's PointsMaterial / particle systems issue drawArrays(POINTS,
// ...). The expanded quads route through bridge_color_program so vertex
// colors and linear / exp2 fog still work; lighting is N/A since
// PointsMaterial doesn't ship normals.
static void draw_arrays_points(JSContext *ctx, nx_webgl_context_t *context,
							   nx_webgl_program_t *program,
							   int32_t first, int32_t count) {
	if (count <= 0)
		return;

	int position_index = program->has_position_attrib_index
							 ? program->position_attrib_index
							 : 0;
	if (position_index < 0 || position_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		position_index = 0;
	nx_webgl_vertex_attrib_t *position =
		&context->vertex_attribs[position_index];
	if (!position->enabled || position->type != GL_FLOAT || position->size < 2) {
		context->error = GL_INVALID_OPERATION;
		return;
	}

	int color_index = program->has_color_attrib_index
						  ? program->color_attrib_index
						  : 1;
	if (color_index < 0 || color_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		color_index = 1;
	nx_webgl_vertex_attrib_t *color_attr =
		&context->vertex_attribs[color_index];
	bool has_vertex_color = color_attr->enabled &&
							color_attr->type == GL_FLOAT &&
							color_attr->size >= 3;

	nx_canvas_t *canvas = context->canvas;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	bool fog_enabled = program->has_fog_color &&
					   ((program->has_fog_near && program->has_fog_far) ||
						program->has_fog_density);
	float fallback_color[4] = {
		68.f / 255.f,
		215.f / 255.f,
		182.f / 255.f,
		1.f,
	};
	float *gpu_color = program->has_color ? program->color : fallback_color;

	// Resolve the bound texture for textured point sprites. NULL when no
	// texture or no sampler uniform — falls through to the untextured
	// color path. Three.js's PointsMaterial({map, ...}) sets up `map` →
	// program->sampler0 which active_texture_for_program then resolves.
	nx_webgl_texture_t *texture = active_texture_for_program(context, program);
	bool use_texture = texture != NULL;

	// Quad half-extent in pixels. Three.js's PointsMaterial uploads
	// `size` (= material.size × pixelRatio) which we recognized as
	// NX_WEBGL_UNIFORM_POINT_SIZE. When `size` is bound we use it as the
	// quad full width; otherwise default to 2 px (legacy milestone #6
	// behavior — small dots for starfields without PointsMaterial.size).
	float base_size_px = program->has_point_size ? program->point_size : 2.f;
	float vw = context->viewport[2] > 0 ? (float)context->viewport[2] : 640.f;
	float vh = context->viewport[3] > 0 ? (float)context->viewport[3] : 360.f;
	// `size` is the FULL on-screen width of the point quad; half-extent
	// is therefore size/2 pixels. NDC width = 2; NDC half-extent =
	// (size_px/2) × (2/vw) = size_px/vw.
	float base_half_x_ndc = base_size_px / vw;
	float base_half_y_ndc = base_size_px / vh;

	// Size attenuation: Three.js's PointsMaterial shader does
	// `gl_PointSize *= (scale / -mvPosition.z)` when sizeAttenuation is
	// true. `scale` is uploaded as `0.5 × canvas.height` (see Three.js
	// r162 source ~line 28029). The bridge can't distinguish "attenuation
	// on" from "attenuation off" via uniform values alone, so we apply
	// the multiply whenever both `size` AND `scale` are bound — matches
	// Three.js's default-on behavior. Demos that want fixed-size points
	// can omit one of the uniforms; the bridge falls back to the base
	// size.
	bool size_attenuation = program->has_point_size && program->has_line_scale;
	float attenuation_scale = program->has_line_scale ? program->line_scale
													  : 1.f;

	// Two triangles forming a quad. Vertex order: TL, BR, BL, TL, TR, BR.
	static const float quad_dx[6] = {-1.f, +1.f, -1.f, -1.f, +1.f, +1.f};
	static const float quad_dy[6] = {+1.f, -1.f, -1.f, +1.f, +1.f, -1.f};
	// Matching UV layout for textured points. (u, v) = ((dx+1)/2, (1-dy)/2)
	// so TL of quad samples (0, 0), BR samples (1, 1).
	static const float quad_u[6] = {0.f, 1.f, 0.f, 0.f, 1.f, 1.f};
	static const float quad_v[6] = {0.f, 1.f, 1.f, 0.f, 0.f, 1.f};

	if (nx_webgl_egl_is_bridge_enabled(context->egl)) {
		int max_vertex_count = count * 6;
		float *clip_xyz = NULL;
		float *clip_xyzuv = NULL;
		if (use_texture)
			clip_xyzuv =
				js_malloc(ctx, (size_t)max_vertex_count * 5 * sizeof(float));
		else
			clip_xyz =
				js_malloc(ctx, (size_t)max_vertex_count * 3 * sizeof(float));
		float *vertex_color_data = NULL;
		float *fog_depth_data = NULL;
		if ((clip_xyz || clip_xyzuv) && has_vertex_color && !use_texture) {
			// Per-vertex color only flows through the untextured triangle
			// bridge. The textured path takes a single diffuse uniform.
			vertex_color_data =
				js_malloc(ctx, (size_t)max_vertex_count * 3 * sizeof(float));
		}
		if ((clip_xyz || clip_xyzuv) && fog_enabled) {
			fog_depth_data =
				js_malloc(ctx, (size_t)max_vertex_count * sizeof(float));
		}
		bool buffers_ok = (use_texture ? (clip_xyzuv != NULL)
									   : (clip_xyz != NULL)) &&
						  (!has_vertex_color || use_texture ||
						   vertex_color_data) &&
						  (!fog_enabled || fog_depth_data);
		if (buffers_ok) {
			bool loaded = true;
			int written_points = 0;
			for (int p = 0; p < count; p++) {
				int vertex_index = first + p;
				nx_webgl_vec3_t pos;
				if (!read_attrib_vec3(context, position, vertex_index, &pos)) {
					loaded = false;
					break;
				}
				nx_webgl_vec3_t ndc = transform_position3_depth(program, pos);
				// Per-point half-extent in NDC. For size attenuation we
				// scale by `scale / -mv_z` — Three.js's exact formula.
				// Mirror compute_view_space_position to get mv_z, then
				// apply.
				float half_x = base_half_x_ndc;
				float half_y = base_half_y_ndc;
				if (size_attenuation && program->has_model_view_matrix) {
					const float *mv = program->model_view_matrix;
					float mv_z = mv[2] * pos.x + mv[6] * pos.y +
								 mv[10] * pos.z + mv[14];
					if (mv_z < -0.0001f) {
						float k = attenuation_scale / -mv_z;
						half_x *= k;
						half_y *= k;
					}
				}
				// Cull points fully outside the NDC cube (with the per-
				// point margin for the quad expansion). Off-screen points
				// pay no bandwidth + bridge upload cost.
				if (ndc.z < -1.f || ndc.z > 1.f)
					continue;
				if (ndc.x < -1.f - half_x || ndc.x > 1.f + half_x)
					continue;
				if (ndc.y < -1.f - half_y || ndc.y > 1.f + half_y)
					continue;

				nx_webgl_vec3_t col = {1.f, 1.f, 1.f};
				if (has_vertex_color && !use_texture) {
					if (!read_attrib_vec3(context, color_attr, vertex_index,
										  &col)) {
						loaded = false;
						break;
					}
				}
				float fog_depth = fog_enabled ? compute_fog_depth(program, pos)
											  : 0.f;
				int base = written_points * 6;
				for (int v = 0; v < 6; v++) {
					float vx = ndc.x + quad_dx[v] * half_x;
					float vy = ndc.y + quad_dy[v] * half_y;
					if (use_texture) {
						clip_xyzuv[(base + v) * 5 + 0] = vx;
						clip_xyzuv[(base + v) * 5 + 1] = vy;
						clip_xyzuv[(base + v) * 5 + 2] = ndc.z;
						clip_xyzuv[(base + v) * 5 + 3] = quad_u[v];
						clip_xyzuv[(base + v) * 5 + 4] = quad_v[v];
					} else {
						clip_xyz[(base + v) * 3 + 0] = vx;
						clip_xyz[(base + v) * 3 + 1] = vy;
						clip_xyz[(base + v) * 3 + 2] = ndc.z;
						if (vertex_color_data) {
							vertex_color_data[(base + v) * 3 + 0] = col.x;
							vertex_color_data[(base + v) * 3 + 1] = col.y;
							vertex_color_data[(base + v) * 3 + 2] = col.z;
						}
					}
					if (fog_depth_data)
						fog_depth_data[base + v] = fog_depth;
				}
				written_points++;
			}
			if (!loaded) {
				js_free(ctx, clip_xyz);
				js_free(ctx, clip_xyzuv);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				context->error = GL_INVALID_OPERATION;
				return;
			}
			int vertex_count_out = written_points * 6;
			if (vertex_count_out == 0) {
				js_free(ctx, clip_xyz);
				js_free(ctx, clip_xyzuv);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				return;
			}
			float zero3[3] = {0.f, 0.f, 0.f};
			bool drew = false;
			if (use_texture) {
				drew = nx_webgl_egl_draw_textured_triangles_bridge(
					context->egl, canvas, clip_xyzuv, vertex_count_out,
					texture->bridge_id, texture->revision,
					(int)texture->width, (int)texture->height, texture->data,
					texture->gles_handle,
					texture->min_filter, texture->mag_filter, texture->wrap_s,
					texture->wrap_t, blend, context->blend_src,
					context->blend_dst, context->blend_src_alpha,
					context->blend_dst_alpha, context->viewport,
					(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
					context->scissor_box,
					(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
					fog_enabled ? fog_depth_data : NULL,
					fog_enabled, program->fog_color, program->fog_near,
					program->fog_far,
					NULL, false, zero3, zero3, zero3,
					NULL, false, zero3, zero3, 0.f, 0.f,
					false, zero3, zero3,
					program->has_fog_density, program->fog_density,
					NULL, false,  // map_transform — points don't use it
					false, 0u,    // cull face — N/A for screen-aligned points
					program->has_color ? program->color : NULL,
					false, NULL, 0.f, NULL, false);  // specular + emissive + derivative-normals DISABLED for points
			} else {
				drew = nx_webgl_egl_draw_triangles_bridge(
					context->egl, canvas, clip_xyz,
					has_vertex_color ? vertex_color_data : NULL,
					vertex_count_out, gpu_color, blend, context->blend_src,
					context->blend_dst, context->blend_src_alpha,
					context->blend_dst_alpha, context->viewport,
					(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
					context->scissor_box,
					(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
					fog_enabled ? fog_depth_data : NULL,
					fog_enabled, program->fog_color, program->fog_near,
					program->fog_far,
					NULL, false, zero3, zero3, zero3,
					NULL, false, zero3, zero3, 0.f, 0.f,
					false, zero3, zero3,
					program->has_fog_density, program->fog_density,
					(context->enabled_caps & GL_CAP_CULL_FACE) != 0,
					context->cull_face,
					false, NULL, 0.f, NULL, false);  // specular + emissive + derivative-normals DISABLED for points
			}
			js_free(ctx, clip_xyz);
			js_free(ctx, clip_xyzuv);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
			if (drew) {
				context->bridge_clear_pending = false;
				return;
			}
		} else {
			js_free(ctx, clip_xyz);
			js_free(ctx, clip_xyzuv);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
		}
	}

	// Software fallback: plot a single pixel per point. Adequate for
	// debug builds where the bridge is disabled; the bridge path above
	// is the production path.
	flush_pending_bridge_clear_to_software(context);
	uint32_t color_u32 = program_color(program);
	uint32_t *pixels = (uint32_t *)canvas->data;
	for (int p = 0; p < count; p++) {
		nx_webgl_vec3_t pos;
		if (!read_attrib_vec3(context, position, first + p, &pos)) {
			context->error = GL_INVALID_OPERATION;
			return;
		}
		nx_webgl_vec3_t ndc = transform_position3_depth(program, pos);
		if (ndc.z < -1.f || ndc.z > 1.f)
			continue;
		nx_webgl_vec2_t px =
			clip_to_pixel(context, (nx_webgl_vec2_t){ndc.x, ndc.y});
		int ix = (int)px.x;
		int iy = (int)px.y;
		if (ix < 0 || ix >= canvas->width || iy < 0 || iy >= canvas->height)
			continue;
		pixels[iy * canvas->width + ix] = color_u32;
	}
	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);
}

// Dispatch a draw through the raw-shader passthrough path when one of three
// gates fires: (1) the bound program was marked via `#pragma raw_passthrough`,
// (2) `instance_count > 0` was supplied via an `ANGLE_instanced_arrays`
// draw call (instancing always needs the native program), or (3) any enabled
// attrib has divisor > 0 (the program is mid-instancing — Three.js's pattern
// is to set divisors once and leave them between draws). Bypasses the
// bridge's hardcoded-program swap and runs the user's linked GLES program
// directly. Returns true if the draw was dispatched (success or GL error
// inside the passthrough path); false to fall through to bridge-mode
// dispatch. See [[bridge-raw-shader-passthrough]].
static bool try_draw_passthrough(nx_webgl_context_t *context,
								  nx_webgl_program_t *program,
								  uint32_t mode, bool indexed,
								  int32_t first, int32_t count,
								  uint32_t element_type,
								  uint32_t element_offset,
								  uint32_t element_buffer_handle,
								  int32_t instance_count) {
	if (!program || !program->gles_handle)
		return false;
	if (!context->egl || !nx_webgl_egl_is_bridge_enabled(context->egl))
		return false;

	bool any_divisor = false;
	for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++) {
		if (context->vertex_attribs[i].enabled &&
			context->vertex_attribs[i].divisor > 0u) {
			any_divisor = true;
			break;
		}
	}
	// Gate: explicit pragma, explicit instanced draw, or sticky divisor on
	// any enabled attrib (Three.js's instancing pattern).
	if (!program->raw_passthrough && instance_count <= 0 && !any_divisor)
		return false;

	nx_webgl_egl_passthrough_attrib_t attribs[NX_WEBGL_MAX_VERTEX_ATTRIBS];
	for (int i = 0; i < NX_WEBGL_MAX_VERTEX_ATTRIBS; i++) {
		nx_webgl_vertex_attrib_t *src = &context->vertex_attribs[i];
		attribs[i].enabled = src->enabled;
		nx_webgl_buffer_t *buf = nx_get_webgl_buffer(src->buffer);
		attribs[i].buffer_handle =
			(buf && !buf->deleted) ? buf->gles_handle : 0u;
		attribs[i].size = src->size;
		attribs[i].type = src->type;
		attribs[i].normalized = src->normalized;
		attribs[i].stride = src->stride;
		attribs[i].offset = src->offset;
		attribs[i].divisor = src->divisor;
	}

	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	bool scissor_enabled = (context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0;
	bool depth_enabled = (context->enabled_caps & GL_CAP_DEPTH_TEST) != 0;
	bool cull_enabled = (context->enabled_caps & GL_CAP_CULL_FACE) != 0;

	bool ok = nx_webgl_egl_draw_passthrough(
		context->egl, context->canvas, program->gles_handle, mode,
		indexed, first, count, element_type, element_offset,
		element_buffer_handle, attribs, NX_WEBGL_MAX_VERTEX_ATTRIBS,
		instance_count, context->viewport, blend,
		context->blend_src, context->blend_dst,
		context->blend_src_alpha, context->blend_dst_alpha,
		scissor_enabled, context->scissor_box, depth_enabled,
		cull_enabled, context->cull_face, context->front_face);
	// An instanced draw that failed setup (e.g. missing native instancing
	// fn pointers) should propagate up so the JS caller can set INVALID_OP.
	// The non-instanced fallback case still returns "dispatched" because the
	// raw_passthrough / divisor gate already committed us to native dispatch.
	if (!ok && instance_count > 0)
		return false;
	context->bridge_clear_pending = false;
	return true;
}

static JSValue nx_webgl_draw_arrays(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t first;
	int32_t count;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &first, argv[1]) ||
		JS_ToInt32(ctx, &count, argv[2]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES && mode != GL_LINES && mode != GL_LINE_STRIP &&
		mode != GL_LINE_LOOP && mode != GL_POINTS) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (first < 0 || count < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	int position_index = program->has_position_attrib_index
							 ? program->position_attrib_index
							 : 0;
	if (position_index < 0 || position_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		position_index = 0;
	nx_webgl_vertex_attrib_t *position =
		&context->vertex_attribs[position_index];
	if (!position->enabled || position->type != GL_FLOAT || position->size < 2) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	// Raw-shader passthrough opt-in via `#pragma raw_passthrough` in either
	// attached shader. Runs the user's linked program directly; bridge swap
	// and CPU-side transformation are skipped. See [[nxjs-no-custom-fragment-shader]]
	// for what this unblocks.
	if (try_draw_passthrough(context, program, mode, false, first, count,
							  0, 0, 0, 0)) {
		return JS_UNDEFINED;
	}

	if (mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) {
		draw_arrays_lines(ctx, context, program, mode, first, count);
		return JS_UNDEFINED;
	}

	if (mode == GL_POINTS) {
		draw_arrays_points(ctx, context, program, first, count);
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture = active_texture_for_program(context, program);
	int uv_index = program->has_uv_attrib_index ? program->uv_attrib_index : 2;
	if (uv_index < 0 || uv_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		uv_index = 2;
	nx_webgl_vertex_attrib_t *texcoord = &context->vertex_attribs[uv_index];
	bool use_texture = texture && texcoord->enabled && texcoord->type == GL_FLOAT &&
					   texcoord->size >= 2;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint32_t color = program_color(program);
	int triangle_count = count / 3;
	if (triangle_count == 0)
		return JS_UNDEFINED;

	// Textured drawArrays: synthesize sequential indices and route
	// through the existing indexed-textured bridge dispatch. Required
	// for Three.js's PolyhedronGeometry-derived shapes (Tetra/Octa/Icos)
	// which Three.js builds as non-indexed (each face copies vertices so
	// UVs and normals can differ at face boundaries). Without this,
	// such shapes with MeshPhongMaterial({map: tex}) hit the software
	// fallback and leak pixels onto canvas->data at canvas-space coords.
	if (nx_webgl_egl_is_bridge_enabled(context->egl) && use_texture) {
		int vertex_count = triangle_count * 3;
		uint16_t *fake_indices =
			js_malloc(ctx, (size_t)vertex_count * sizeof(uint16_t));
		if (fake_indices) {
			int first_clamped = first < 0 ? 0 : first;
			bool indices_fit = true;
			for (int i = 0; i < vertex_count; i++) {
				int idx = first_clamped + i;
				if (idx < 0 || idx > 0xFFFF) {
					indices_fit = false;
					break;
				}
				fake_indices[i] = (uint16_t)idx;
			}
			if (indices_fit &&
				draw_indexed_textured_triangles_bridge(
					ctx, context, program, position, texcoord, texture,
					fake_indices, vertex_count, blend)) {
				js_free(ctx, fake_indices);
				context->bridge_clear_pending = false;
				return JS_UNDEFINED;
			}
			js_free(ctx, fake_indices);
		}
		// Bridge failed — when client manages its own readback,
		// don't fall through to software (see nx_webgl_draw_elements
		// gate). Matches the textured drawElements failure path.
		if (!nx_webgl_egl_get_auto_flush(context->egl)) {
			nx_webgl_egl_append_dispatch_debug(context->egl, "AT-BAIL");
			return JS_UNDEFINED;
		}
	}

	if (nx_webgl_egl_is_bridge_enabled(context->egl) && !use_texture) {
		int vertex_count = triangle_count * 3;
		int color_index = program->has_color_attrib_index
							  ? program->color_attrib_index
							  : 1;
		if (color_index < 0 || color_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
			color_index = 1;
		nx_webgl_vertex_attrib_t *color_attr =
			&context->vertex_attribs[color_index];
		bool has_vertex_color = color_attr->enabled &&
								color_attr->type == GL_FLOAT &&
								color_attr->size >= 3;
		bool fog_enabled = program->has_fog_color &&
						   ((program->has_fog_near && program->has_fog_far) ||
							program->has_fog_density);
		int normal_index = program->has_normal_attrib_index
							   ? program->normal_attrib_index
							   : 4;
		if (normal_index < 0 || normal_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
			normal_index = 4;
		nx_webgl_vertex_attrib_t *normal_attr =
			&context->vertex_attribs[normal_index];
		bool has_normals = normal_attr->enabled &&
						   normal_attr->type == GL_FLOAT &&
						   normal_attr->size >= 3;
		bool has_directional = program->has_light_direction &&
							   program->has_light_color;
		bool has_point_light = program->has_point_light_position &&
							   program->has_point_light_color;
		// See draw_indexed_textured_triangles_bridge for derivative-normals
		// (milestone #16) gating rationale.
		bool has_lighting_uniforms = has_directional || has_point_light;
		bool use_derivative_normals = has_lighting_uniforms && !has_normals;
		bool lighting_enabled = has_lighting_uniforms;

		float *clip_xyz =
			js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
		float *vertex_color_data = NULL;
		float *fog_depth_data = NULL;
		float *normal_data = NULL;
		float *view_position_data = NULL;
		if (clip_xyz && has_vertex_color) {
			vertex_color_data =
				js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
		}
		if (clip_xyz && fog_enabled) {
			fog_depth_data =
				js_malloc(ctx, (size_t)vertex_count * sizeof(float));
		}
		if (clip_xyz && lighting_enabled) {
			if (has_normals) {
				normal_data =
					js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
			}
			// Always allocate view-position when lit — needed by both the
			// point-light dispatch and the Blinn-Phong specular / derivative-
			// normals paths.
			if (!has_normals || normal_data) {
				view_position_data =
					js_malloc(ctx, (size_t)vertex_count * 3 * sizeof(float));
			}
		}
		if (clip_xyz && (!has_vertex_color || vertex_color_data) &&
			(!fog_enabled || fog_depth_data) &&
			(!(lighting_enabled && has_normals) || normal_data) &&
			(!lighting_enabled || view_position_data)) {
			bool loaded = true;
			for (int i = 0; i < vertex_count; i++) {
				int vertex_index = first + i;
				nx_webgl_vec3_t position3;
				if (!read_attrib_vec3(context, position, vertex_index,
									  &position3)) {
					loaded = false;
					break;
				}
				nx_webgl_vec3_t clip =
					transform_position3_depth(program, position3);
				clip_xyz[i * 3 + 0] = clip.x;
				clip_xyz[i * 3 + 1] = clip.y;
				clip_xyz[i * 3 + 2] = clip.z;
				if (fog_depth_data)
					fog_depth_data[i] = compute_fog_depth(program, position3);
				if (has_vertex_color) {
					nx_webgl_vec3_t col;
					if (!read_attrib_vec3(context, color_attr, vertex_index,
										  &col)) {
						loaded = false;
						break;
					}
					vertex_color_data[i * 3 + 0] = col.x;
					vertex_color_data[i * 3 + 1] = col.y;
					vertex_color_data[i * 3 + 2] = col.z;
				}
				if (normal_data) {
					nx_webgl_vec3_t n;
					if (!read_attrib_vec3(context, normal_attr, vertex_index,
										  &n)) {
						loaded = false;
						break;
					}
					nx_webgl_vec3_t vn = compute_view_space_normal(program, n);
					normal_data[i * 3 + 0] = vn.x;
					normal_data[i * 3 + 1] = vn.y;
					normal_data[i * 3 + 2] = vn.z;
				}
				if (view_position_data) {
					nx_webgl_vec3_t vp =
						compute_view_space_position(program, position3);
					view_position_data[i * 3 + 0] = vp.x;
					view_position_data[i * 3 + 1] = vp.y;
					view_position_data[i * 3 + 2] = vp.z;
				}
			}
			if (!loaded) {
				js_free(ctx, clip_xyz);
				js_free(ctx, vertex_color_data);
				js_free(ctx, fog_depth_data);
				js_free(ctx, normal_data);
				js_free(ctx, view_position_data);
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			float fallback_color[4] = {
				68.f / 255.f,
				215.f / 255.f,
				182.f / 255.f,
				1.f,
			};
			float *gpu_color = program->has_color ? program->color
												  : fallback_color;
			float zero3[3] = {0.f, 0.f, 0.f};
			bool has_directional2 = program->has_light_direction2 &&
									program->has_light_color2;
			bool drew = nx_webgl_egl_draw_triangles_bridge(
				context->egl, canvas, clip_xyz,
				has_vertex_color ? vertex_color_data : NULL, vertex_count,
				gpu_color, blend, context->blend_src, context->blend_dst,
				context->blend_src_alpha, context->blend_dst_alpha,
				context->viewport,
				(context->enabled_caps & GL_CAP_SCISSOR_TEST) != 0,
				context->scissor_box,
				(context->enabled_caps & GL_CAP_DEPTH_TEST) != 0,
				fog_enabled ? fog_depth_data : NULL,
				fog_enabled, program->fog_color, program->fog_near,
				program->fog_far,
				(lighting_enabled && has_normals) ? normal_data : NULL,
				lighting_enabled,
				has_directional ? program->light_direction : zero3,
				has_directional ? program->light_color : zero3,
				program->has_ambient_light_color
					? program->ambient_light_color
					: zero3,
				lighting_enabled ? view_position_data : NULL,
				has_point_light, program->point_light_position,
				program->point_light_color, program->point_light_distance,
				program->point_light_decay,
				has_directional2,
				has_directional2 ? program->light_direction2 : zero3,
				has_directional2 ? program->light_color2 : zero3,
				program->has_fog_density, program->fog_density,
			(context->enabled_caps & GL_CAP_CULL_FACE) != 0,
			context->cull_face,
			program->has_specular && program->has_shininess,
			program->has_specular ? program->specular : NULL,
			program->has_shininess ? program->shininess : 30.f,
			program->has_emissive ? program->emissive : NULL,
			use_derivative_normals);
			js_free(ctx, clip_xyz);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
			js_free(ctx, normal_data);
			js_free(ctx, view_position_data);
			if (drew) {
				context->bridge_clear_pending = false;
				return JS_UNDEFINED;
			}
		} else {
			js_free(ctx, clip_xyz);
			js_free(ctx, vertex_color_data);
			js_free(ctx, fog_depth_data);
			js_free(ctx, normal_data);
			js_free(ctx, view_position_data);
		}
	}

	flush_pending_bridge_clear_to_software(context);

	for (int triangle = 0; triangle < triangle_count; triangle++) {
		nx_webgl_vec2_t clip[3];
		nx_webgl_vec2_t uv[3];
		for (int i = 0; i < 3; i++) {
			int vertex_index = first + triangle * 3 + i;
			nx_webgl_vec3_t position3;
			if (!read_attrib_vec3(context, position, vertex_index,
								  &position3)) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			if (use_texture &&
				!read_attrib_vec2(context, texcoord, vertex_index, &uv[i])) {
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			clip[i] = transform_position3(program, position3);
		}

		nx_webgl_vec2_t p0 = clip_to_pixel(context, clip[0]);
		nx_webgl_vec2_t p1 = clip_to_pixel(context, clip[1]);
		nx_webgl_vec2_t p2 = clip_to_pixel(context, clip[2]);
		if (use_texture)
			rasterize_triangle_textured(canvas, p0, p1, p2, uv[0], uv[1],
										uv[2], texture, blend,
										context->blend_src,
										context->blend_dst);
		else
			rasterize_triangle(canvas, p0, p1, p2, color, blend,
							   context->blend_src, context->blend_dst);
	}

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);

	return JS_UNDEFINED;
}

static JSValue nx_webgl_draw_elements(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t count;
	uint32_t type;
	int32_t offset;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &count, argv[1]) ||
		JS_ToUint32(ctx, &type, argv[2]) || JS_ToInt32(ctx, &offset, argv[3]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES && mode != GL_LINES && mode != GL_LINE_STRIP &&
		mode != GL_LINE_LOOP) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_BYTE) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	size_t index_size = (type == GL_UNSIGNED_BYTE) ? 1 : 2;
	if (count < 0 || offset < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	// WebGL 1.0 spec 6.4: offset not aligned to index size returns
	// INVALID_OPERATION (not INVALID_VALUE).
	if ((size_t)offset % index_size != 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	int position_index = program->has_position_attrib_index
							 ? program->position_attrib_index
							 : 0;
	if (position_index < 0 || position_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		position_index = 0;
	nx_webgl_vertex_attrib_t *position =
		&context->vertex_attribs[position_index];
	if (!position->enabled || position->type != GL_FLOAT || position->size < 2) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *element_buffer =
		nx_get_webgl_buffer(context->element_array_buffer_binding);
	if (!element_buffer || element_buffer->deleted || !element_buffer->data) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if ((size_t)offset + (size_t)count * index_size >
		element_buffer->size) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	// Raw-shader passthrough opt-in via `#pragma raw_passthrough` in either
	// attached shader. Native glDrawElements reads the user's element
	// buffer directly (no JS-side index extraction). See
	// [[bridge-raw-shader-passthrough]].
	if (try_draw_passthrough(context, program, mode, true, 0, count,
							  type, (uint32_t)offset,
							  element_buffer->gles_handle, 0)) {
		return JS_UNDEFINED;
	}

	// All downstream dispatch paths take `uint16_t *indices`. When the
	// caller used `gl.UNSIGNED_BYTE`, upconvert the uint8 indices into
	// a temporary uint16 buffer first; free at every exit path below.
	uint16_t *indices;
	uint16_t *owned_indices = NULL;
	if (type == GL_UNSIGNED_BYTE) {
		owned_indices = js_malloc(ctx, (size_t)count * sizeof(uint16_t));
		if (!owned_indices)
			return JS_EXCEPTION;
		const uint8_t *src = element_buffer->data + offset;
		for (int i = 0; i < count; i++)
			owned_indices[i] = (uint16_t)src[i];
		indices = owned_indices;
	} else {
		indices = (uint16_t *)(element_buffer->data + offset);
	}

	if (mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) {
		draw_elements_lines(ctx, context, program, position, mode, indices,
							count);
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}

	nx_webgl_texture_t *texture = active_texture_for_program(context, program);
	int uv_index = program->has_uv_attrib_index ? program->uv_attrib_index : 2;
	if (uv_index < 0 || uv_index >= NX_WEBGL_MAX_VERTEX_ATTRIBS)
		uv_index = 2;
	nx_webgl_vertex_attrib_t *texcoord = &context->vertex_attribs[uv_index];
	bool use_texture = texture && texcoord->enabled && texcoord->type == GL_FLOAT &&
					   texcoord->size >= 2;
	bool blend = (context->enabled_caps & GL_CAP_BLEND) != 0;
	uint32_t color = program_color(program);
	if (!use_texture &&
		draw_indexed_triangles_bridge(ctx, context, program, position, indices,
									  count, color, blend)) {
		context->bridge_clear_pending = false;
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}
	// Sprite detection: Three.js's SpriteMaterial uploads `center` (vec2)
	// and `rotation` (float) — uniforms no other material uses. When both
	// are present, the bridge can't use its generic CPU-side MVP path
	// (sprite vertices are screen-aligned around the sprite's view-space
	// origin, not Three.js's standard MVP transform). Route to the
	// dedicated sprite math path instead. Falls through to the generic
	// textured path if the sprite path declines (e.g. no modelMatrix).
	if (use_texture && program->has_sprite_center &&
		program->has_sprite_rotation &&
		draw_sprite_bridge(ctx, context, program, position, texcoord,
						   texture, indices, count, blend)) {
		context->bridge_clear_pending = false;
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}
	if (use_texture &&
		draw_indexed_textured_triangles_bridge(ctx, context, program, position,
											   texcoord, texture, indices,
											   count, blend)) {
		context->bridge_clear_pending = false;
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}
	if (!use_texture)
		nx_webgl_egl_append_dispatch_debug(context->egl, "noTex");
	// Bridge dispatch failed (or wasn't applicable). When the client has
	// opted out of bridge auto-flush (inline-canvas WebGL via
	// `gl.setBridgeAutoFlush(false)`), it expects the bridge to manage
	// the FBO and the client to handle screen presentation via
	// `gl.copyBridgeToCanvas`. The software fallback below writes to
	// `canvas->data` — which IS the screen canvas backing buffer in
	// that mode, NOT the inline canvas slot. Those pixels land at
	// (0,0) of the screen and visually leak as "shapes outside the
	// canvas" overlapping the page chrome. Bail out instead so the
	// draw is lost-but-clean for the frame.
	if (context->egl && !nx_webgl_egl_get_auto_flush(context->egl)) {
		nx_webgl_egl_append_dispatch_debug(context->egl, "BAIL");
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}
	flush_pending_bridge_clear_to_software(context);
	if (use_texture &&
		draw_axis_aligned_textured_quads(ctx, context, program, position, texcoord,
										 texture, indices, count, blend)) {
		if (canvas->surface)
			cairo_surface_mark_dirty(canvas->surface);
		if (owned_indices)
			js_free(ctx, owned_indices);
		return JS_UNDEFINED;
	}

	int triangle_count = count / 3;
	bool sort_depth = (context->enabled_caps & GL_CAP_DEPTH_TEST) != 0 &&
					  triangle_count > 1;
	nx_webgl_triangle_t stack_triangles[64];
	nx_webgl_triangle_t *triangles = stack_triangles;
	if (sort_depth && triangle_count > (int)countof(stack_triangles)) {
		triangles = js_malloc(ctx, (size_t)triangle_count *
									   sizeof(nx_webgl_triangle_t));
		if (!triangles) {
			if (owned_indices)
				js_free(ctx, owned_indices);
			return JS_EXCEPTION;
		}
	}

	for (int triangle = 0; triangle < triangle_count; triangle++) {
		nx_webgl_triangle_t local_triangle;
		for (int i = 0; i < 3; i++) {
			int vertex_index = indices[triangle * 3 + i];
			nx_webgl_vec3_t position3;
			if (!read_attrib_vec3(context, position, vertex_index,
								  &position3)) {
				if (triangles != stack_triangles)
					js_free(ctx, triangles);
				if (owned_indices)
					js_free(ctx, owned_indices);
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			if (use_texture &&
				!read_attrib_vec2(context, texcoord, vertex_index,
								  &local_triangle.uv[i])) {
				if (triangles != stack_triangles)
					js_free(ctx, triangles);
				if (owned_indices)
					js_free(ctx, owned_indices);
				context->error = GL_INVALID_OPERATION;
				return JS_UNDEFINED;
			}
			nx_webgl_vec3_t clip =
				transform_position3_depth(program, position3);
			local_triangle.p[i] =
				clip_to_pixel(context, (nx_webgl_vec2_t){clip.x, clip.y});
			if (!use_texture)
				local_triangle.uv[i] = (nx_webgl_vec2_t){0.f, 0.f};
			if (i == 0)
				local_triangle.z = clip.z;
			else
				local_triangle.z += clip.z;
		}
		local_triangle.z /= 3.f;
		if (sort_depth)
			triangles[triangle] = local_triangle;
		else if (use_texture)
			rasterize_triangle_textured(
				canvas, local_triangle.p[0], local_triangle.p[1],
				local_triangle.p[2], local_triangle.uv[0],
				local_triangle.uv[1], local_triangle.uv[2], texture, blend,
				context->blend_src, context->blend_dst);
		else
			rasterize_triangle(canvas, local_triangle.p[0], local_triangle.p[1],
							   local_triangle.p[2], color, blend,
							   context->blend_src, context->blend_dst);
	}

	if (sort_depth) {
		qsort(triangles, (size_t)triangle_count, sizeof(nx_webgl_triangle_t),
			  compare_triangle_depth_desc);
		for (int triangle = 0; triangle < triangle_count; triangle++) {
			nx_webgl_triangle_t *sorted_triangle = &triangles[triangle];
			if (use_texture)
				rasterize_triangle_textured(
					canvas, sorted_triangle->p[0], sorted_triangle->p[1],
					sorted_triangle->p[2], sorted_triangle->uv[0],
					sorted_triangle->uv[1], sorted_triangle->uv[2], texture,
					blend, context->blend_src, context->blend_dst);
			else
				rasterize_triangle(canvas, sorted_triangle->p[0],
								   sorted_triangle->p[1],
								   sorted_triangle->p[2], color, blend,
								   context->blend_src, context->blend_dst);
		}
	}
	if (triangles != stack_triangles)
		js_free(ctx, triangles);
	if (owned_indices)
		js_free(ctx, owned_indices);

	if (canvas->surface)
		cairo_surface_mark_dirty(canvas->surface);

	return JS_UNDEFINED;
}

// ANGLE_instanced_arrays — vertexAttribDivisorANGLE(index, divisor).
// Stores per-attrib divisor in `context->vertex_attribs[index].divisor`;
// the passthrough dispatch consumes it at draw time via the native
// `glVertexAttribDivisor*` entry point resolved at backend init.
static JSValue nx_webgl_vertex_attrib_divisor(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t index;
	uint32_t divisor;
	if (JS_ToUint32(ctx, &index, argv[0]) ||
		JS_ToUint32(ctx, &divisor, argv[1]))
		return JS_EXCEPTION;
	if (index >= NX_WEBGL_MAX_VERTEX_ATTRIBS) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	context->vertex_attribs[index].divisor = divisor;
	return JS_UNDEFINED;
}

// ANGLE_instanced_arrays — drawArraysInstancedANGLE(mode, first, count, instanceCount).
// Always dispatches via the passthrough path; instancing is not supported on
// the bridge's hardcoded programs. Returns INVALID_OPERATION if native
// instancing entry points aren't loaded (probe at backend init).
static JSValue nx_webgl_draw_arrays_instanced(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t first;
	int32_t count;
	int32_t instance_count;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &first, argv[1]) ||
		JS_ToInt32(ctx, &count, argv[2]) ||
		JS_ToInt32(ctx, &instance_count, argv[3]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES && mode != GL_LINES && mode != GL_LINE_STRIP &&
		mode != GL_LINE_LOOP && mode != GL_POINTS) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (first < 0 || count < 0 || instance_count < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if (count == 0 || instance_count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	if (!try_draw_passthrough(context, program, mode, false, first, count,
							  0, 0, 0, instance_count)) {
		// Native instancing not available — bridge can't emulate.
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

// ANGLE_instanced_arrays — drawElementsInstancedANGLE(mode, count, type, offset, instanceCount).
// Same dispatch rules as drawArraysInstancedANGLE.
static JSValue nx_webgl_draw_elements_instanced(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t mode;
	int32_t count;
	uint32_t type;
	int32_t offset;
	int32_t instance_count;
	if (JS_ToUint32(ctx, &mode, argv[0]) || JS_ToInt32(ctx, &count, argv[1]) ||
		JS_ToUint32(ctx, &type, argv[2]) || JS_ToInt32(ctx, &offset, argv[3]) ||
		JS_ToInt32(ctx, &instance_count, argv[4]))
		return JS_EXCEPTION;

	if (mode != GL_TRIANGLES && mode != GL_LINES && mode != GL_LINE_STRIP &&
		mode != GL_LINE_LOOP) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_BYTE &&
		type != GL_UNSIGNED_INT) {
		// Native GL3 supports UNSIGNED_INT directly; nx.js doesn't expose
		// the OES_element_index_uint extension, but accept it here in case
		// Three.js feature-detects via getExtension first.
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	size_t index_size = (type == GL_UNSIGNED_BYTE) ? 1
						: (type == GL_UNSIGNED_INT) ? 4
													: 2;
	if (count < 0 || offset < 0 || instance_count < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	if ((size_t)offset % index_size != 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (count == 0 || instance_count == 0)
		return JS_UNDEFINED;

	nx_webgl_program_t *program = nx_get_webgl_program(context->current_program);
	if (!program || !program->link_status || program->deleted) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_webgl_buffer_t *element_buffer =
		nx_get_webgl_buffer(context->element_array_buffer_binding);
	if (!element_buffer || element_buffer->deleted || !element_buffer->data) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if ((size_t)offset + (size_t)count * index_size > element_buffer->size) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	nx_canvas_t *canvas = context->canvas;
	if (!canvas->data || canvas->width == 0 || canvas->height == 0)
		return JS_UNDEFINED;

	if (!try_draw_passthrough(context, program, mode, true, 0, count,
							  type, (uint32_t)offset,
							  element_buffer->gles_handle,
							  instance_count)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_enable_disable(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv,
									   bool enabled) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t cap;
	if (JS_ToUint32(ctx, &cap, argv[0]))
		return JS_EXCEPTION;

	uint32_t flag = cap_to_flag(cap);
	if (flag == 0) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (enabled)
		context->enabled_caps |= flag;
	else
		context->enabled_caps &= ~flag;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_enable(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	return nx_webgl_enable_disable(ctx, this_val, argc, argv, true);
}

static JSValue nx_webgl_disable(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	return nx_webgl_enable_disable(ctx, this_val, argc, argv, false);
}

static JSValue nx_webgl_is_enabled(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t cap;
	if (JS_ToUint32(ctx, &cap, argv[0]))
		return JS_EXCEPTION;
	uint32_t flag = cap_to_flag(cap);
	if (flag == 0) {
		context->error = GL_INVALID_ENUM;
		return JS_NewBool(ctx, false);
	}
	return JS_NewBool(ctx, (context->enabled_caps & flag) != 0);
}

static JSValue nx_webgl_depth_func(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t func;
	if (JS_ToUint32(ctx, &func, argv[0]))
		return JS_EXCEPTION;

	if (!is_depth_func(func)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	context->depth_func = func;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_blend_func(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t src;
	uint32_t dst;
	if (JS_ToUint32(ctx, &src, argv[0]) || JS_ToUint32(ctx, &dst, argv[1]))
		return JS_EXCEPTION;
	if (!is_blend_factor(src) || !is_blend_factor(dst)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	context->blend_src = src;
	context->blend_dst = dst;
	context->blend_src_alpha = src;
	context->blend_dst_alpha = dst;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_blend_equation(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t equation;
	if (JS_ToUint32(ctx, &equation, argv[0]))
		return JS_EXCEPTION;
	if (!is_blend_equation(equation)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->blend_equation_rgb = equation;
	context->blend_equation_alpha = equation;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_blend_func_separate(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t src_rgb;
	uint32_t dst_rgb;
	uint32_t src_alpha;
	uint32_t dst_alpha;
	if (JS_ToUint32(ctx, &src_rgb, argv[0]) ||
		JS_ToUint32(ctx, &dst_rgb, argv[1]) ||
		JS_ToUint32(ctx, &src_alpha, argv[2]) ||
		JS_ToUint32(ctx, &dst_alpha, argv[3]))
		return JS_EXCEPTION;
	if (!is_blend_factor(src_rgb) || !is_blend_factor(dst_rgb) ||
		!is_blend_factor(src_alpha) || !is_blend_factor(dst_alpha)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->blend_src = src_rgb;
	context->blend_dst = dst_rgb;
	context->blend_src_alpha = src_alpha;
	context->blend_dst_alpha = dst_alpha;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_blend_color(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	for (int i = 0; i < 4; i++) {
		double value;
		if (JS_ToFloat64(ctx, &value, argv[i]))
			return JS_EXCEPTION;
		context->blend_color[i] = clamp01(value);
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_cull_face(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t mode;
	if (JS_ToUint32(ctx, &mode, argv[0]))
		return JS_EXCEPTION;
	if (!is_cull_face_mode(mode)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->cull_face = mode;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_front_face(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t mode;
	if (JS_ToUint32(ctx, &mode, argv[0]))
		return JS_EXCEPTION;
	if (!is_front_face_mode(mode)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->front_face = mode;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_polygon_offset(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_ToFloat64(ctx, &context->polygon_offset_factor, argv[0]) ||
		JS_ToFloat64(ctx, &context->polygon_offset_units, argv[1]))
		return JS_EXCEPTION;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_stencil_mask(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_ToUint32(ctx, &context->stencil_mask, argv[0]))
		return JS_EXCEPTION;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_stencil_func(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t func;
	uint32_t mask;
	int32_t ref;
	if (JS_ToUint32(ctx, &func, argv[0]) || JS_ToInt32(ctx, &ref, argv[1]) ||
		JS_ToUint32(ctx, &mask, argv[2]))
		return JS_EXCEPTION;
	if (!is_depth_func(func)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->stencil_func = func;
	context->stencil_ref = ref;
	context->stencil_value_mask = mask;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_stencil_op(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t fail;
	uint32_t zfail;
	uint32_t zpass;
	if (JS_ToUint32(ctx, &fail, argv[0]) || JS_ToUint32(ctx, &zfail, argv[1]) ||
		JS_ToUint32(ctx, &zpass, argv[2]))
		return JS_EXCEPTION;
	if (!is_stencil_op(fail) || !is_stencil_op(zfail) ||
		!is_stencil_op(zpass)) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	context->stencil_fail = fail;
	context->stencil_zfail = zfail;
	context->stencil_zpass = zpass;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_framebuffer(JSContext *ctx,
                                            JSValueConst this_val, int argc,
                                            JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	nx_webgl_framebuffer_t *fb = js_mallocz(ctx, sizeof(*fb));
	if (!fb)
		return JS_EXCEPTION;
	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_framebuffer_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, fb);
		return obj;
	}
	fb->color_attachment = JS_UNDEFINED;
	fb->depth_attachment = JS_UNDEFINED;
	fb->stencil_attachment = JS_UNDEFINED;
	fb->handle = nx_webgl_egl_create_native_framebuffer(context->egl,
	                                                    context->canvas);
	if (fb->handle == 0) {
		js_free(ctx, fb);
		JS_FreeValue(ctx, obj);
		context->error = GL_OUT_OF_MEMORY;
		return JS_NULL;
	}
	JS_SetOpaque(obj, fb);
	return obj;
}

static JSValue nx_webgl_delete_framebuffer(JSContext *ctx,
                                            JSValueConst this_val, int argc,
                                            JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0]))
		return JS_UNDEFINED;
	nx_webgl_framebuffer_t *fb = nx_get_webgl_framebuffer(argv[0]);
	if (!fb || fb->deleted)
		return JS_UNDEFINED;
	if (nx_get_webgl_framebuffer(context->framebuffer_binding) == fb) {
		JS_FreeValue(ctx, context->framebuffer_binding);
		context->framebuffer_binding = JS_UNDEFINED;
		// Bridge also clears its mirrored binding so subsequent draws
		// don't target a deleted FBO.
		nx_webgl_egl_set_user_framebuffer(context->egl, 0, 0, 0);
	}
	if (fb->handle) {
		nx_webgl_egl_delete_native_framebuffer(context->egl, fb->handle);
		fb->handle = 0;
	}
	fb->deleted = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_is_framebuffer(JSContext *ctx,
                                        JSValueConst this_val, int argc,
                                        JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	if (argc < 1)
		return JS_NewBool(ctx, false);
	nx_webgl_framebuffer_t *fb = nx_get_webgl_framebuffer(argv[0]);
	return JS_NewBool(ctx, fb && !fb->deleted && fb->handle != 0);
}

static JSValue nx_webgl_bind_framebuffer(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_FRAMEBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
		JS_FreeValue(ctx, context->framebuffer_binding);
		context->framebuffer_binding = JS_UNDEFINED;
		nx_webgl_egl_set_user_framebuffer(context->egl, 0, 0, 0);
		return JS_UNDEFINED;
	}
	nx_webgl_framebuffer_t *fb = nx_get_webgl_framebuffer(argv[1]);
	if (!fb || fb->deleted || fb->handle == 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	JS_FreeValue(ctx, context->framebuffer_binding);
	context->framebuffer_binding = JS_DupValue(ctx, argv[1]);
	nx_webgl_egl_set_user_framebuffer(context->egl, fb->handle, fb->width,
	                                   fb->height);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_check_framebuffer_status(JSContext *ctx,
                                                  JSValueConst this_val,
                                                  int argc,
                                                  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_FRAMEBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_NewUint32(ctx, 0);
	}
	nx_webgl_framebuffer_t *fb =
		nx_get_webgl_framebuffer(context->framebuffer_binding);
	if (!fb || fb->handle == 0) {
		// Default framebuffer (bridge_framebuffer) is always complete.
		return JS_NewUint32(ctx, GL_FRAMEBUFFER_COMPLETE);
	}
	uint32_t status = nx_webgl_egl_check_framebuffer_status(context->egl,
	                                                         fb->handle);
	return JS_NewUint32(ctx, status);
}

static JSValue nx_webgl_framebuffer_texture_2d(JSContext *ctx,
                                                JSValueConst this_val,
                                                int argc,
                                                JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target, attachment, textarget;
	int32_t level;
	if (JS_ToUint32(ctx, &target, argv[0]) ||
	    JS_ToUint32(ctx, &attachment, argv[1]) ||
	    JS_ToUint32(ctx, &textarget, argv[2]) ||
	    JS_ToInt32(ctx, &level, argv[4]))
		return JS_EXCEPTION;
	if (target != GL_FRAMEBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (textarget != GL_TEXTURE_2D) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (level != 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	nx_webgl_framebuffer_t *fb =
		nx_get_webgl_framebuffer(context->framebuffer_binding);
	if (!fb || fb->handle == 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	uint32_t tex_handle = 0;
	JSValue *slot = NULL;
	if (attachment == GL_COLOR_ATTACHMENT0)
		slot = &fb->color_attachment;
	else if (attachment == GL_DEPTH_ATTACHMENT)
		slot = &fb->depth_attachment;
	else if (attachment == GL_STENCIL_ATTACHMENT)
		slot = &fb->stencil_attachment;
	else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
		// Combined depth+stencil attachment — Three.js's DepthStencilFormat
		// path uses this. Store in depth slot (the stencil slot stays
		// available for stencil-only configs); both halves of the
		// underlying texture come from the single binding.
		slot = &fb->depth_attachment;
	else {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (JS_IsNull(argv[3]) || JS_IsUndefined(argv[3])) {
		// Detach.
		if (slot) {
			JS_FreeValue(ctx, *slot);
			*slot = JS_UNDEFINED;
		}
	} else {
		nx_webgl_texture_t *texture = nx_get_webgl_texture(argv[3]);
		if (!texture || texture->deleted) {
			context->error = GL_INVALID_OPERATION;
			return JS_UNDEFINED;
		}
		// Ensure the texture has a persistent native handle. Three.js
		// usually calls texImage2D(NULL) before framebufferTexture2D, which
		// allocates the handle; this is a safety net for the reverse order.
		if (texture->gles_handle == 0) {
			texture->gles_handle =
				nx_webgl_egl_create_persistent_texture(context->egl,
				                                        context->canvas);
			if (texture->gles_handle == 0) {
				context->error = GL_OUT_OF_MEMORY;
				return JS_UNDEFINED;
			}
			// Allocate empty storage at the recorded dims (or 1×1 if none).
			int w = texture->width > 0 ? (int)texture->width : 1;
			int h = texture->height > 0 ? (int)texture->height : 1;
			(void)nx_webgl_egl_persistent_texture_image_2d(
			    context->egl, texture->gles_handle, w, h, GL_RGBA,
			    GL_RGBA, GL_UNSIGNED_BYTE, NULL, texture->min_filter,
			    texture->mag_filter, texture->wrap_s, texture->wrap_t);
			texture->width = w;
			texture->height = h;
		}
		tex_handle = texture->gles_handle;
		if (slot) {
			JS_FreeValue(ctx, *slot);
			*slot = JS_DupValue(ctx, argv[3]);
		}
		// Update the FBO's dims from the color attachment's dims. Three.js
		// asks for matching color/depth dims, so updating from color is
		// sufficient for the bridge's viewport scaling.
		if (attachment == GL_COLOR_ATTACHMENT0) {
			fb->width = (int)texture->width;
			fb->height = (int)texture->height;
			// If this FBO is currently bound, push the new dims to the
			// bridge so the next dispatch's viewport scales right.
			if (nx_get_webgl_framebuffer(context->framebuffer_binding) == fb)
				nx_webgl_egl_set_user_framebuffer(context->egl, fb->handle,
				                                   fb->width, fb->height);
		}
	}

	if (!nx_webgl_egl_framebuffer_texture_2d(context->egl, fb->handle,
	                                          attachment, tex_handle)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_framebuffer_renderbuffer(JSContext *ctx,
                                                  JSValueConst this_val,
                                                  int argc,
                                                  JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target, attachment, rbtarget;
	if (JS_ToUint32(ctx, &target, argv[0]) ||
	    JS_ToUint32(ctx, &attachment, argv[1]) ||
	    JS_ToUint32(ctx, &rbtarget, argv[2]))
		return JS_EXCEPTION;
	if (target != GL_FRAMEBUFFER || rbtarget != GL_RENDERBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	nx_webgl_framebuffer_t *fb =
		nx_get_webgl_framebuffer(context->framebuffer_binding);
	if (!fb || fb->handle == 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}

	uint32_t rb_handle = 0;
	JSValue *slot = NULL;
	if (attachment == GL_COLOR_ATTACHMENT0)
		slot = &fb->color_attachment;
	else if (attachment == GL_DEPTH_ATTACHMENT)
		slot = &fb->depth_attachment;
	else if (attachment == GL_STENCIL_ATTACHMENT)
		slot = &fb->stencil_attachment;
	else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
		slot = &fb->depth_attachment;
	else {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}

	if (JS_IsNull(argv[3]) || JS_IsUndefined(argv[3])) {
		if (slot) {
			JS_FreeValue(ctx, *slot);
			*slot = JS_UNDEFINED;
		}
	} else {
		nx_webgl_renderbuffer_t *rb = nx_get_webgl_renderbuffer(argv[3]);
		if (!rb || rb->deleted || rb->handle == 0) {
			context->error = GL_INVALID_OPERATION;
			return JS_UNDEFINED;
		}
		rb_handle = rb->handle;
		if (slot) {
			JS_FreeValue(ctx, *slot);
			*slot = JS_DupValue(ctx, argv[3]);
		}
	}

	if (!nx_webgl_egl_framebuffer_renderbuffer(context->egl, fb->handle,
	                                            attachment, rb_handle)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_create_renderbuffer(JSContext *ctx,
                                             JSValueConst this_val, int argc,
                                             JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	nx_webgl_renderbuffer_t *rb = js_mallocz(ctx, sizeof(*rb));
	if (!rb)
		return JS_EXCEPTION;
	JSValue obj = JS_NewObjectClass(ctx, nx_webgl_renderbuffer_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, rb);
		return obj;
	}
	rb->handle = nx_webgl_egl_create_native_renderbuffer(context->egl,
	                                                     context->canvas);
	if (rb->handle == 0) {
		js_free(ctx, rb);
		JS_FreeValue(ctx, obj);
		context->error = GL_OUT_OF_MEMORY;
		return JS_NULL;
	}
	JS_SetOpaque(obj, rb);
	return obj;
}

static JSValue nx_webgl_delete_renderbuffer(JSContext *ctx,
                                             JSValueConst this_val, int argc,
                                             JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0]))
		return JS_UNDEFINED;
	nx_webgl_renderbuffer_t *rb = nx_get_webgl_renderbuffer(argv[0]);
	if (!rb || rb->deleted)
		return JS_UNDEFINED;
	if (nx_get_webgl_renderbuffer(context->renderbuffer_binding) == rb) {
		JS_FreeValue(ctx, context->renderbuffer_binding);
		context->renderbuffer_binding = JS_UNDEFINED;
	}
	if (rb->handle) {
		nx_webgl_egl_delete_native_renderbuffer(context->egl, rb->handle);
		rb->handle = 0;
	}
	rb->deleted = true;
	return JS_UNDEFINED;
}

static JSValue nx_webgl_is_renderbuffer(JSContext *ctx,
                                         JSValueConst this_val, int argc,
                                         JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	if (argc < 1)
		return JS_NewBool(ctx, false);
	nx_webgl_renderbuffer_t *rb = nx_get_webgl_renderbuffer(argv[0]);
	return JS_NewBool(ctx, rb && !rb->deleted && rb->handle != 0);
}

static JSValue nx_webgl_bind_renderbuffer(JSContext *ctx,
                                           JSValueConst this_val, int argc,
                                           JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target;
	if (JS_ToUint32(ctx, &target, argv[0]))
		return JS_EXCEPTION;
	if (target != GL_RENDERBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1])) {
		JS_FreeValue(ctx, context->renderbuffer_binding);
		context->renderbuffer_binding = JS_UNDEFINED;
		return JS_UNDEFINED;
	}
	nx_webgl_renderbuffer_t *rb = nx_get_webgl_renderbuffer(argv[1]);
	if (!rb || rb->deleted || rb->handle == 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	JS_FreeValue(ctx, context->renderbuffer_binding);
	context->renderbuffer_binding = JS_DupValue(ctx, argv[1]);
	return JS_UNDEFINED;
}

static JSValue nx_webgl_renderbuffer_storage(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target, internalformat;
	int32_t width, height;
	if (JS_ToUint32(ctx, &target, argv[0]) ||
	    JS_ToUint32(ctx, &internalformat, argv[1]) ||
	    JS_ToInt32(ctx, &width, argv[2]) ||
	    JS_ToInt32(ctx, &height, argv[3]))
		return JS_EXCEPTION;
	if (target != GL_RENDERBUFFER) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	if (width <= 0 || height <= 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	nx_webgl_renderbuffer_t *rb =
		nx_get_webgl_renderbuffer(context->renderbuffer_binding);
	if (!rb || rb->handle == 0) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (!nx_webgl_egl_renderbuffer_storage(context->egl, rb->handle,
	                                        internalformat, width, height)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	rb->internal_format = internalformat;
	rb->width = width;
	rb->height = height;
	return JS_UNDEFINED;
}

// gl.readPixels(x, y, width, height, format, type, pixels)
// Reads from the currently-bound framebuffer (the bridge FBO when the
// GPU bridge is enabled) into a JS-side typed array. Only RGBA + UNSIGNED_BYTE
// is supported — the only format/type pair WebGL 1 mandates and the one the
// bridge FBO uses natively. The destination buffer must be at least
// width * height * 4 bytes.
//
// Y coordinate convention: canvas-y top-down (matches nx.js's bridge
// `gl.viewport` and `gl.scissor` quirk — see `bridge_scale_rect` in
// `webgl_egl.c`). The internal translation to GL's bottom-up
// `glReadPixels` is done here so that a script that calls
// `gl.viewport(0, 0, w, h); gl.drawArrays(...); gl.readPixels(x, y, 1, 1, ...)`
// reads from the same pixel it would on a real WebGL canvas where
// the canvas IS the framebuffer.
//
// Returned bytes are still in GL row order (bottom row of the read
// rect comes first). Callers that need top-down rows must Y-flip
// while copying.
static JSValue nx_webgl_read_pixels(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int32_t x, y, width, height;
	uint32_t format, type;
	if (JS_ToInt32(ctx, &x, argv[0]) ||
		JS_ToInt32(ctx, &y, argv[1]) ||
		JS_ToInt32(ctx, &width, argv[2]) ||
		JS_ToInt32(ctx, &height, argv[3]) ||
		JS_ToUint32(ctx, &format, argv[4]) ||
		JS_ToUint32(ctx, &type, argv[5]))
		return JS_EXCEPTION;
	if (format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	if (width <= 0 || height <= 0)
		return JS_UNDEFINED;
	size_t buffer_size = 0;
	uint8_t *dst = NX_GetBufferSource(ctx, &buffer_size, argv[6]);
	if (!dst) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	size_t needed = (size_t)width * (size_t)height * 4;
	if (buffer_size < needed) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	// User-FBO path (milestone #19): when the user has bound a non-null
	// framebuffer via gl.bindFramebuffer, read directly from it using
	// STANDARD GL convention (origin bottom-left, y not translated). The
	// canvas-y top-down quirk applies only to the bridge's own FBO, where
	// canvas-y inputs need flipping to GL-y for glReadPixels.
	nx_webgl_framebuffer_t *bound_fb =
		nx_get_webgl_framebuffer(context->framebuffer_binding);
	if (bound_fb && !bound_fb->deleted && bound_fb->handle != 0) {
		if (!nx_webgl_egl_read_user_fbo_pixels(context->egl, bound_fb->handle,
		                                        x, y, width, height,
		                                        format, type, dst)) {
			context->error = GL_INVALID_OPERATION;
			return JS_UNDEFINED;
		}
		return JS_UNDEFINED;
	}
	// Default-FBO (bridge_framebuffer) path: keep canvas-y top-down convention.
	int32_t canvas_height = (int32_t)context->canvas->height;
	int32_t gl_y = canvas_height - y - height;
	if (!nx_webgl_egl_read_bridge_pixels(context->egl, context->canvas,
										  x, gl_y, width, height,
										  format, type, dst)) {
		context->error = GL_INVALID_OPERATION;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue nx_webgl_line_width(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	double width;
	if (JS_ToFloat64(ctx, &width, argv[0]))
		return JS_EXCEPTION;
	if (width <= 0. || isnan(width)) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}
	context->line_width = width;
	return JS_UNDEFINED;
}

// WebGL 1 `gl.hint(target, mode)`. Accepts GENERATE_MIPMAP_HINT (always
// present) and FRAGMENT_SHADER_DERIVATIVE_HINT_OES (only when
// OES_standard_derivatives is exposed via getExtension). Mode must be
// FASTEST, NICEST, or DONT_CARE. The stored value is the bridge's
// preference — the actual native GLES hint isn't forwarded (Tegra ignores
// it for the bridge's fragment shaders) but it's queryable via
// getParameter so Khronos conformance tests pass.
static JSValue nx_webgl_hint(JSContext *ctx, JSValueConst this_val,
							  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	uint32_t target, mode;
	if (JS_ToUint32(ctx, &target, argv[0]) ||
		JS_ToUint32(ctx, &mode, argv[1]))
		return JS_EXCEPTION;
	if (mode != GL_FASTEST && mode != GL_NICEST && mode != GL_DONT_CARE) {
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
	switch (target) {
	case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
		context->hint_fragment_shader_derivative = mode;
		return JS_UNDEFINED;
	case GL_GENERATE_MIPMAP_HINT:
		context->hint_generate_mipmap = mode;
		return JS_UNDEFINED;
	default:
		context->error = GL_INVALID_ENUM;
		return JS_UNDEFINED;
	}
}

static JSValue nx_webgl_viewport(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	int32_t values[4];
	for (int i = 0; i < 4; i++) {
		if (JS_ToInt32(ctx, &values[i], argv[i]))
			return JS_EXCEPTION;
	}

	if (values[2] < 0 || values[3] < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	for (int i = 0; i < 4; i++) {
		context->viewport[i] = values[i];
	}

	return JS_UNDEFINED;
}

static JSValue nx_webgl_scissor(JSContext *ctx, JSValueConst this_val,
								int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	int32_t values[4];
	for (int i = 0; i < 4; i++) {
		if (JS_ToInt32(ctx, &values[i], argv[i]))
			return JS_EXCEPTION;
	}

	if (values[2] < 0 || values[3] < 0) {
		context->error = GL_INVALID_VALUE;
		return JS_UNDEFINED;
	}

	for (int i = 0; i < 4; i++)
		context->scissor_box[i] = values[i];

	return JS_UNDEFINED;
}

static JSValue nx_webgl_get_parameter(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t pname;
	if (JS_ToUint32(ctx, &pname, argv[0]))
		return JS_EXCEPTION;

	uint32_t cap_flag = cap_to_flag(pname);
	if (cap_flag != 0)
		return JS_NewBool(ctx, (context->enabled_caps & cap_flag) != 0);

	switch (pname) {
	case GL_COLOR_CLEAR_VALUE:
		return new_number_array(ctx, context->clear_color, 4);
	case GL_COLOR_WRITEMASK: {
		JSValue array = JS_NewArray(ctx);
		for (size_t i = 0; i < 4; i++)
			JS_SetPropertyUint32(ctx, array, i,
								 JS_NewBool(ctx, context->color_mask[i]));
		return array;
	}
	case GL_DEPTH_CLEAR_VALUE:
		return JS_NewFloat64(ctx, context->clear_depth);
	case GL_STENCIL_CLEAR_VALUE:
		return JS_NewInt32(ctx, context->clear_stencil);
	case GL_DEPTH_WRITEMASK:
		return JS_NewBool(ctx, context->depth_mask);
	case GL_DEPTH_FUNC:
		return JS_NewUint32(ctx, context->depth_func);
	case GL_BLEND_EQUATION_RGB:
		return JS_NewUint32(ctx, context->blend_equation_rgb);
	case GL_BLEND_EQUATION_ALPHA:
		return JS_NewUint32(ctx, context->blend_equation_alpha);
	case GL_BLEND_SRC_RGB:
		return JS_NewUint32(ctx, context->blend_src);
	case GL_BLEND_DST_RGB:
		return JS_NewUint32(ctx, context->blend_dst);
	case GL_BLEND_SRC_ALPHA:
		return JS_NewUint32(ctx, context->blend_src_alpha);
	case GL_BLEND_DST_ALPHA:
		return JS_NewUint32(ctx, context->blend_dst_alpha);
	case GL_CULL_FACE_MODE:
		return JS_NewUint32(ctx, context->cull_face);
	case GL_FRONT_FACE:
		return JS_NewUint32(ctx, context->front_face);
	case GL_POLYGON_OFFSET_FACTOR:
		return JS_NewFloat64(ctx, context->polygon_offset_factor);
	case GL_POLYGON_OFFSET_UNITS:
		return JS_NewFloat64(ctx, context->polygon_offset_units);
	case GL_STENCIL_WRITEMASK:
		return JS_NewUint32(ctx, context->stencil_mask);
	case GL_STENCIL_FUNC:
		return JS_NewUint32(ctx, context->stencil_func);
	case GL_STENCIL_REF:
		return JS_NewInt32(ctx, context->stencil_ref);
	case GL_STENCIL_VALUE_MASK:
		return JS_NewUint32(ctx, context->stencil_value_mask);
	case GL_STENCIL_FAIL:
		return JS_NewUint32(ctx, context->stencil_fail);
	case GL_STENCIL_PASS_DEPTH_FAIL:
		return JS_NewUint32(ctx, context->stencil_zfail);
	case GL_STENCIL_PASS_DEPTH_PASS:
		return JS_NewUint32(ctx, context->stencil_zpass);
	case GL_VIEWPORT:
		return new_int_array(ctx, context->viewport, 4);
	case GL_SCISSOR_BOX:
		return new_int_array(ctx, context->scissor_box, 4);
	case GL_ALIASED_POINT_SIZE_RANGE:
	case GL_ALIASED_LINE_WIDTH_RANGE: {
		double range[2] = {1., 1.};
		return new_number_array(ctx, range, 2);
	}
	case GL_VENDOR:
		return JS_NewString(ctx, "nx.js");
	case GL_RENDERER:
		return JS_NewString(ctx, "nx.js framebuffer WebGL skeleton");
	case GL_VERSION:
		return JS_NewString(ctx, "WebGL 1.0 (nx.js experimental)");
	case GL_SHADING_LANGUAGE_VERSION:
		return JS_NewString(ctx, "WebGL GLSL ES 1.0 (nx.js experimental)");
	case GL_CURRENT_PROGRAM:
		if (JS_IsUndefined(context->current_program))
			return JS_NULL;
		return JS_DupValue(ctx, context->current_program);
	case GL_ARRAY_BUFFER_BINDING:
		if (JS_IsUndefined(context->array_buffer_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->array_buffer_binding);
	case GL_ELEMENT_ARRAY_BUFFER_BINDING:
		if (JS_IsUndefined(context->element_array_buffer_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->element_array_buffer_binding);
	case GL_TEXTURE_BINDING_2D:
		if (JS_IsUndefined(context->texture_2d_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->texture_2d_binding);
	case GL_TEXTURE_BINDING_CUBE_MAP:
		if (JS_IsUndefined(context->texture_cube_binding))
			return JS_NULL;
		return JS_DupValue(ctx, context->texture_cube_binding);
	case GL_FRAMEBUFFER_BINDING:
	case GL_RENDERBUFFER_BINDING:
		return JS_NULL;
	case GL_ACTIVE_TEXTURE:
		return JS_NewUint32(ctx, context->active_texture);
	case GL_UNPACK_ALIGNMENT:
	case GL_PACK_ALIGNMENT:
		return JS_NewUint32(ctx, 4);
	case GL_RED_BITS:
	case GL_GREEN_BITS:
	case GL_BLUE_BITS:
	case GL_ALPHA_BITS:
		return JS_NewUint32(ctx, 8);
	case GL_DEPTH_BITS:
		return JS_NewUint32(ctx, 24);
	case GL_STENCIL_BITS:
		return JS_NewUint32(ctx, 0);
	case GL_MAX_TEXTURE_SIZE:
	case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
	case GL_MAX_RENDERBUFFER_SIZE:
		return JS_NewUint32(ctx, 4096);
	case GL_MAX_VIEWPORT_DIMS: {
		int dims[2] = {4096, 4096};
		return new_int_array(ctx, dims, 2);
	}
	case GL_MAX_VERTEX_ATTRIBS:
		return JS_NewUint32(ctx, 8);
	case GL_MAX_TEXTURE_IMAGE_UNITS:
	case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
		return JS_NewUint32(ctx, 8);
	case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
		return JS_NewUint32(ctx, 0);
	case GL_MAX_VERTEX_UNIFORM_VECTORS:
		return JS_NewUint32(ctx, 256);
	case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
		return JS_NewUint32(ctx, 224);
	case GL_MAX_VARYING_VECTORS:
		return JS_NewUint32(ctx, 8);
	case GL_GENERATE_MIPMAP_HINT:
		return JS_NewUint32(ctx, context->hint_generate_mipmap);
	case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
		return JS_NewUint32(ctx, context->hint_fragment_shader_derivative);
	default:
		context->error = GL_INVALID_ENUM;
		return JS_NULL;
	}
}

static JSValue nx_webgl_get_error(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;

	uint32_t error = context->error;
	context->error = GL_NO_ERROR;
	return JS_NewUint32(ctx, error);
}

static JSValue nx_webgl_get_backend_info(JSContext *ctx,
										 JSValueConst this_val, int argc,
										 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_get_backend_info(ctx, context->egl);
}

static JSValue nx_webgl_clear_gpu_prototype(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewBool(
		ctx, nx_webgl_egl_clear_prototype(context->egl, context->canvas));
}

static JSValue nx_webgl_probe_gpu_prototype_step(JSContext *ctx,
												 JSValueConst this_val,
												 int argc,
												 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return JS_NewBool(
		ctx, nx_webgl_egl_probe_step(context->egl, context->canvas));
}

static JSValue nx_webgl_triangle_gpu_prototype(JSContext *ctx,
											   JSValueConst this_val,
											   int argc,
											   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_triangle_readback(ctx, context->egl, context->canvas);
}

static JSValue nx_webgl_bridge_gpu_prototype(JSContext *ctx,
											 JSValueConst this_val,
											 int argc,
											 JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	return nx_webgl_egl_bridge_framebuffer(ctx, context->egl, context->canvas);
}

static JSValue nx_webgl_bridge_gpu_benchmark_prototype(JSContext *ctx,
													   JSValueConst this_val,
													   int argc,
													   JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int frame_count = 120;
	int width = 256;
	int height = 144;
	if (argc > 0 && !JS_IsUndefined(argv[0]) &&
		JS_ToInt32(ctx, &frame_count, argv[0]))
		return JS_EXCEPTION;
	if (argc > 1 && !JS_IsUndefined(argv[1]) &&
		JS_ToInt32(ctx, &width, argv[1]))
		return JS_EXCEPTION;
	if (argc > 2 && !JS_IsUndefined(argv[2]) &&
		JS_ToInt32(ctx, &height, argv[2]))
		return JS_EXCEPTION;
	return nx_webgl_egl_bridge_benchmark(ctx, context->egl, context->canvas,
										 frame_count, width, height);
}

static JSValue nx_webgl_enable_gpu_bridge_prototype(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	bool enabled = true;
	if (argc > 0 && !JS_IsUndefined(argv[0]))
		enabled = JS_ToBool(ctx, argv[0]);
	nx_webgl_egl_set_bridge_enabled(context->egl, enabled);
	if (!enabled)
		context->bridge_clear_pending = false;
	return JS_NewBool(ctx, nx_webgl_egl_is_bridge_enabled(context->egl));
}

// gl.setBridgeAutoFlush(bool) — disable the automatic bridge→canvas
// readback that fires on `gl.clear` when there's a pending bridge draw.
// Clients that drive readback themselves via `gl.readPixels` (e.g.
// switch-web-browser's inline-canvas WebGL canvas-runner) should call
// this with `false` once after enabling the bridge — saves a 1280×720
// glReadPixels per frame and avoids a visible flash on the first
// auto-flush.
static JSValue nx_webgl_set_bridge_auto_flush(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc,
                                              JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	bool enabled = true;
	if (argc > 0 && !JS_IsUndefined(argv[0]))
		enabled = JS_ToBool(ctx, argv[0]);
	nx_webgl_egl_set_auto_flush(context->egl, enabled);
	return JS_NewBool(ctx, enabled);
}

// gl.setTessellationFix(bool) — toggle bridge-side midpoint
// subdivision of large screen-space triangles. Off by default.
//
// Intended as a workaround for the Tegra X1 TBR per-tile UV-
// interpolator coherency bug ([[threejs-cube-white-face]]) — but the
// current implementation does NOT actually fix the bug because
// midpoint-in-NDC subdivision produces uniform sub-triangle patterns
// the rasterizer still trips on. switch-web-browser does NOT call
// this; it uses JS-side `BoxGeometry(w,h,d,8,8,8)` tessellation
// instead, which is the only working approach today. The API and
// underlying scaffolding (recursion helper, scratch buffer, hooks in
// both bridge draw paths) are kept dormant in case someone returns
// to do the clip-space-correct version. See the big STATE comment
// in `nxjs-source/source/webgl_egl.c` (above
// `tessellate_one_triangle`) for the investigation and the refactor
// that would actually fix things.
static JSValue nx_webgl_set_tessellation_fix(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc,
                                             JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	bool enabled = false;
	if (argc > 0 && !JS_IsUndefined(argv[0]))
		enabled = JS_ToBool(ctx, argv[0]);
	nx_webgl_egl_set_tessellation_fix(context->egl, enabled);
	return JS_NewBool(ctx, nx_webgl_egl_get_tessellation_fix(context->egl));
}

// gl.copyBridgeToCanvas(srcX, srcY, srcW, srcH, dstCanvas, dstX, dstY)
// Reads a sub-rect of the bridge FBO directly into `dstCanvas` (a
// `Screen` or `OffscreenCanvas`) at (dstX, dstY) with Y-flip + premul
// swizzle, in one C-level row copy. Skips the JS-visible buffer +
// putImageData + drawImage(offscreen) sequence — for animated inline-
// canvas WebGL (Three.js cube etc.), this is ~7 ms/frame faster than
// the explicit-readback+overlay path.
//
// Takes the canvas directly (not a 2D context) because 2D contexts are
// commonly Proxy-wrapped (e.g. by `installBrowserShim` in
// `switch-web-runtime` for CSS color normalisation) and the Proxy's
// class id no longer matches nx.js's canvas-context class id, so
// `JS_GetOpaque` can't reach the underlying C struct. Canvas objects
// aren't wrapped — `nxScreen()` returns the raw Screen.
//
// `srcX`/`srcY` follow nxjs's canvas-y top-down convention (matching
// `gl.viewport` / `gl.scissor` x/y inputs). Returns `true` on success.
static JSValue nx_webgl_copy_bridge_to_canvas(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc,
                                              JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	if (argc < 7)
		return JS_NewBool(ctx, false);
	int32_t src_x, src_y, src_w, src_h, dst_x, dst_y;
	if (JS_ToInt32(ctx, &src_x, argv[0]) ||
	    JS_ToInt32(ctx, &src_y, argv[1]) ||
	    JS_ToInt32(ctx, &src_w, argv[2]) ||
	    JS_ToInt32(ctx, &src_h, argv[3]) ||
	    JS_ToInt32(ctx, &dst_x, argv[5]) ||
	    JS_ToInt32(ctx, &dst_y, argv[6]))
		return JS_EXCEPTION;
	nx_canvas_t *dst_canvas = nx_get_canvas(ctx, argv[4]);
	if (!dst_canvas) {
		// Clear any TypeError from JS_GetOpaque2 — we treat type
		// mismatch as a soft fail.
		JS_FreeValue(ctx, JS_GetException(ctx));
		return JS_NewBool(ctx, false);
	}
	// Translate canvas-y top src origin to GL bottom-up. The bridge
	// FBO height matches `context->canvas->height` (the canvas the
	// WebGL context is bound to — i.e., the screen canvas).
	int32_t bridge_h = (int32_t)context->canvas->height;
	int32_t gl_y = bridge_h - src_y - src_h;
	bool ok = nx_webgl_egl_read_bridge_to_canvas_data(context->egl,
	                                                   src_x, gl_y,
	                                                   src_w, src_h,
	                                                   dst_canvas,
	                                                   dst_x, dst_y);
	return JS_NewBool(ctx, ok);
}

static JSValue nx_webgl_set_gpu_bridge_resolution_prototype(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	nx_webgl_context_t *context = nx_get_webgl_context(ctx, this_val);
	if (!context)
		return JS_EXCEPTION;
	int32_t width = 0;
	int32_t height = 0;
	if (argc > 0 && !JS_IsUndefined(argv[0]) &&
		JS_ToInt32(ctx, &width, argv[0]))
		return JS_EXCEPTION;
	if (argc > 1 && !JS_IsUndefined(argv[1]) &&
		JS_ToInt32(ctx, &height, argv[1]))
		return JS_EXCEPTION;
	nx_webgl_egl_set_bridge_resolution(context->egl, width, height);
	context->bridge_clear_pending = false;
	return JS_NewBool(ctx, true);
}

static void define_constant(JSContext *ctx, JSValueConst obj, const char *name,
							int32_t value) {
	JS_DefinePropertyValueStr(ctx, obj, name, JS_NewInt32(ctx, value),
							  JS_PROP_CONFIGURABLE);
}

static JSValue nx_webgl_context_init_class(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");

	NX_DEF_GET(proto, "drawingBufferWidth", nx_webgl_get_drawing_buffer_width);
	NX_DEF_GET(proto, "drawingBufferHeight",
			   nx_webgl_get_drawing_buffer_height);

	NX_DEF_FUNC(proto, "getContextAttributes",
				nx_webgl_get_context_attributes, 0);
	NX_DEF_FUNC(proto, "getSupportedExtensions",
				nx_webgl_get_supported_extensions, 0);
	NX_DEF_FUNC(proto, "getExtension", nx_webgl_get_extension, 1);
	NX_DEF_FUNC(proto, "getShaderPrecisionFormat",
				nx_webgl_get_shader_precision_format, 2);
	NX_DEF_FUNC(proto, "clearColor", nx_webgl_clear_color, 4);
	NX_DEF_FUNC(proto, "clearDepth", nx_webgl_clear_depth, 1);
	NX_DEF_FUNC(proto, "clearStencil", nx_webgl_clear_stencil, 1);
	NX_DEF_FUNC(proto, "clear", nx_webgl_clear, 1);
	NX_DEF_FUNC(proto, "createShader", nx_webgl_create_shader, 1);
	NX_DEF_FUNC(proto, "shaderSource", nx_webgl_shader_source, 2);
	NX_DEF_FUNC(proto, "compileShader", nx_webgl_compile_shader, 1);
	NX_DEF_FUNC(proto, "getShaderParameter", nx_webgl_get_shader_parameter, 2);
	NX_DEF_FUNC(proto, "getShaderInfoLog", nx_webgl_get_shader_info_log, 1);
	NX_DEF_FUNC(proto, "deleteShader", nx_webgl_delete_shader, 1);
	NX_DEF_FUNC(proto, "createProgram", nx_webgl_create_program, 0);
	NX_DEF_FUNC(proto, "attachShader", nx_webgl_attach_shader, 2);
	NX_DEF_FUNC(proto, "bindAttribLocation", nx_webgl_bind_attrib_location, 3);
	NX_DEF_FUNC(proto, "linkProgram", nx_webgl_link_program, 1);
	NX_DEF_FUNC(proto, "useProgram", nx_webgl_use_program, 1);
	NX_DEF_FUNC(proto, "getProgramParameter", nx_webgl_get_program_parameter,
				2);
	NX_DEF_FUNC(proto, "getProgramInfoLog", nx_webgl_get_program_info_log, 1);
	NX_DEF_FUNC(proto, "getActiveAttrib", nx_webgl_get_active_attrib, 2);
	NX_DEF_FUNC(proto, "getActiveUniform", nx_webgl_get_active_uniform, 2);
	NX_DEF_FUNC(proto, "deleteProgram", nx_webgl_delete_program, 1);
	NX_DEF_FUNC(proto, "createBuffer", nx_webgl_create_buffer, 0);
	NX_DEF_FUNC(proto, "bindBuffer", nx_webgl_bind_buffer, 2);
	NX_DEF_FUNC(proto, "bufferData", nx_webgl_buffer_data, 3);
	NX_DEF_FUNC(proto, "bufferSubData", nx_webgl_buffer_sub_data, 3);
	NX_DEF_FUNC(proto, "getBufferParameter", nx_webgl_get_buffer_parameter, 2);
	NX_DEF_FUNC(proto, "deleteBuffer", nx_webgl_delete_buffer, 1);
	NX_DEF_FUNC(proto, "createTexture", nx_webgl_create_texture, 0);
	NX_DEF_FUNC(proto, "activeTexture", nx_webgl_active_texture, 1);
	NX_DEF_FUNC(proto, "bindTexture", nx_webgl_bind_texture, 2);
	NX_DEF_FUNC(proto, "texImage2D", nx_webgl_tex_image_2d, 9);
	NX_DEF_FUNC(proto, "texSubImage2D", nx_webgl_tex_sub_image_2d, 9);
	NX_DEF_FUNC(proto, "pixelStorei", nx_webgl_pixel_storei, 2);
	NX_DEF_FUNC(proto, "generateMipmap", nx_webgl_generate_mipmap, 1);
	NX_DEF_FUNC(proto, "texParameteri", nx_webgl_tex_parameteri, 3);
	NX_DEF_FUNC(proto, "deleteTexture", nx_webgl_delete_texture, 1);
	NX_DEF_FUNC(proto, "getUniformLocation", nx_webgl_get_uniform_location, 2);
	NX_DEF_FUNC(proto, "uniform2f", nx_webgl_uniform2f, 3);
	NX_DEF_FUNC(proto, "uniform1f", nx_webgl_uniform1f, 2);
	NX_DEF_FUNC(proto, "uniform3f", nx_webgl_uniform3f, 4);
	NX_DEF_FUNC(proto, "uniform3fv", nx_webgl_uniform3fv, 2);
	NX_DEF_FUNC(proto, "uniform4f", nx_webgl_uniform4f, 5);
	NX_DEF_FUNC(proto, "uniform1i", nx_webgl_uniform1i, 2);
	NX_DEF_FUNC(proto, "uniformMatrix4fv", nx_webgl_uniform_matrix4fv, 3);
	NX_DEF_FUNC(proto, "uniformMatrix3fv", nx_webgl_uniform_matrix3fv, 3);
	NX_DEF_FUNC(proto, "uniformMatrix2fv", nx_webgl_uniform_matrix2fv, 3);
	NX_DEF_FUNC(proto, "uniform1fv", nx_webgl_uniform1fv, 2);
	NX_DEF_FUNC(proto, "uniform2fv", nx_webgl_uniform2fv, 2);
	NX_DEF_FUNC(proto, "uniform4fv", nx_webgl_uniform4fv, 2);
	NX_DEF_FUNC(proto, "uniform2i", nx_webgl_uniform2i, 3);
	NX_DEF_FUNC(proto, "uniform3i", nx_webgl_uniform3i, 4);
	NX_DEF_FUNC(proto, "uniform4i", nx_webgl_uniform4i, 5);
	NX_DEF_FUNC(proto, "uniform1iv", nx_webgl_uniform1iv, 2);
	NX_DEF_FUNC(proto, "uniform2iv", nx_webgl_uniform2iv, 2);
	NX_DEF_FUNC(proto, "uniform3iv", nx_webgl_uniform3iv, 2);
	NX_DEF_FUNC(proto, "uniform4iv", nx_webgl_uniform4iv, 2);
	NX_DEF_FUNC(proto, "getAttribLocation", nx_webgl_get_attrib_location, 2);
	NX_DEF_FUNC(proto, "enableVertexAttribArray",
				nx_webgl_enable_vertex_attrib_array, 1);
	NX_DEF_FUNC(proto, "disableVertexAttribArray",
				nx_webgl_disable_vertex_attrib_array, 1);
	NX_DEF_FUNC(proto, "vertexAttribPointer",
				nx_webgl_vertex_attrib_pointer, 6);
	NX_DEF_FUNC(proto, "drawArrays", nx_webgl_draw_arrays, 3);
	NX_DEF_FUNC(proto, "drawElements", nx_webgl_draw_elements, 4);
	NX_DEF_FUNC(proto, "enable", nx_webgl_enable, 1);
	NX_DEF_FUNC(proto, "isEnabled", nx_webgl_is_enabled, 1);
	NX_DEF_FUNC(proto, "disable", nx_webgl_disable, 1);
	NX_DEF_FUNC(proto, "depthFunc", nx_webgl_depth_func, 1);
	NX_DEF_FUNC(proto, "depthMask", nx_webgl_depth_mask, 1);
	NX_DEF_FUNC(proto, "colorMask", nx_webgl_color_mask, 4);
	NX_DEF_FUNC(proto, "blendEquation", nx_webgl_blend_equation, 1);
	NX_DEF_FUNC(proto, "blendFunc", nx_webgl_blend_func, 2);
	NX_DEF_FUNC(proto, "blendFuncSeparate", nx_webgl_blend_func_separate, 4);
	NX_DEF_FUNC(proto, "blendColor", nx_webgl_blend_color, 4);
	NX_DEF_FUNC(proto, "cullFace", nx_webgl_cull_face, 1);
	NX_DEF_FUNC(proto, "frontFace", nx_webgl_front_face, 1);
	NX_DEF_FUNC(proto, "polygonOffset", nx_webgl_polygon_offset, 2);
	NX_DEF_FUNC(proto, "stencilMask", nx_webgl_stencil_mask, 1);
	NX_DEF_FUNC(proto, "stencilFunc", nx_webgl_stencil_func, 3);
	NX_DEF_FUNC(proto, "stencilOp", nx_webgl_stencil_op, 3);
	NX_DEF_FUNC(proto, "bindFramebuffer", nx_webgl_bind_framebuffer, 2);
	NX_DEF_FUNC(proto, "createFramebuffer", nx_webgl_create_framebuffer, 0);
	NX_DEF_FUNC(proto, "deleteFramebuffer", nx_webgl_delete_framebuffer, 1);
	NX_DEF_FUNC(proto, "isFramebuffer", nx_webgl_is_framebuffer, 1);
	NX_DEF_FUNC(proto, "checkFramebufferStatus",
	            nx_webgl_check_framebuffer_status, 1);
	NX_DEF_FUNC(proto, "framebufferTexture2D",
	            nx_webgl_framebuffer_texture_2d, 5);
	NX_DEF_FUNC(proto, "framebufferRenderbuffer",
	            nx_webgl_framebuffer_renderbuffer, 4);
	NX_DEF_FUNC(proto, "createRenderbuffer", nx_webgl_create_renderbuffer, 0);
	NX_DEF_FUNC(proto, "deleteRenderbuffer", nx_webgl_delete_renderbuffer, 1);
	NX_DEF_FUNC(proto, "isRenderbuffer", nx_webgl_is_renderbuffer, 1);
	NX_DEF_FUNC(proto, "bindRenderbuffer", nx_webgl_bind_renderbuffer, 2);
	NX_DEF_FUNC(proto, "renderbufferStorage",
	            nx_webgl_renderbuffer_storage, 4);
	NX_DEF_FUNC(proto, "readPixels", nx_webgl_read_pixels, 7);
	NX_DEF_FUNC(proto, "lineWidth", nx_webgl_line_width, 1);
	NX_DEF_FUNC(proto, "hint", nx_webgl_hint, 2);
	NX_DEF_FUNC(proto, "viewport", nx_webgl_viewport, 4);
	NX_DEF_FUNC(proto, "scissor", nx_webgl_scissor, 4);
	NX_DEF_FUNC(proto, "getParameter", nx_webgl_get_parameter, 1);
	NX_DEF_FUNC(proto, "getError", nx_webgl_get_error, 0);
	NX_DEF_FUNC(proto, "getBackendInfo", nx_webgl_get_backend_info, 0);
	NX_DEF_FUNC(proto, "clearGpuPrototype", nx_webgl_clear_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "probeGpuPrototypeStep",
				nx_webgl_probe_gpu_prototype_step, 0);
	NX_DEF_FUNC(proto, "triangleGpuPrototype",
				nx_webgl_triangle_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "bridgeGpuPrototype",
				nx_webgl_bridge_gpu_prototype, 0);
	NX_DEF_FUNC(proto, "bridgeGpuBenchmarkPrototype",
				nx_webgl_bridge_gpu_benchmark_prototype, 3);
	NX_DEF_FUNC(proto, "enableGpuBridgePrototype",
				nx_webgl_enable_gpu_bridge_prototype, 1);
	NX_DEF_FUNC(proto, "setBridgeAutoFlush",
				nx_webgl_set_bridge_auto_flush, 1);
	NX_DEF_FUNC(proto, "setTessellationFix",
				nx_webgl_set_tessellation_fix, 1);
	NX_DEF_FUNC(proto, "copyBridgeToCanvas",
				nx_webgl_copy_bridge_to_canvas, 7);
	NX_DEF_FUNC(proto, "setGpuBridgeResolutionPrototype",
				nx_webgl_set_gpu_bridge_resolution_prototype, 2);

	define_constant(ctx, proto, "NO_ERROR", GL_NO_ERROR);
	define_constant(ctx, proto, "INVALID_ENUM", GL_INVALID_ENUM);
	define_constant(ctx, proto, "INVALID_VALUE", GL_INVALID_VALUE);
	define_constant(ctx, proto, "INVALID_OPERATION", GL_INVALID_OPERATION);
	define_constant(ctx, proto, "POINTS", GL_POINTS);
	define_constant(ctx, proto, "LINES", GL_LINES);
	define_constant(ctx, proto, "LINE_LOOP", GL_LINE_LOOP);
	define_constant(ctx, proto, "LINE_STRIP", GL_LINE_STRIP);
	define_constant(ctx, proto, "TRIANGLES", GL_TRIANGLES);
	define_constant(ctx, proto, "ZERO", GL_ZERO);
	define_constant(ctx, proto, "ONE", GL_ONE);
	define_constant(ctx, proto, "NEVER", GL_NEVER);
	define_constant(ctx, proto, "LESS", GL_LESS);
	define_constant(ctx, proto, "EQUAL", GL_EQUAL);
	define_constant(ctx, proto, "LEQUAL", GL_LEQUAL);
	define_constant(ctx, proto, "GREATER", GL_GREATER);
	define_constant(ctx, proto, "NOTEQUAL", GL_NOTEQUAL);
	define_constant(ctx, proto, "GEQUAL", GL_GEQUAL);
	define_constant(ctx, proto, "ALWAYS", GL_ALWAYS);
	define_constant(ctx, proto, "DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT);
	define_constant(ctx, proto, "STENCIL_BUFFER_BIT", GL_STENCIL_BUFFER_BIT);
	define_constant(ctx, proto, "COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT);
	define_constant(ctx, proto, "NONE", GL_NONE);
	define_constant(ctx, proto, "CULL_FACE", GL_CULL_FACE);
	define_constant(ctx, proto, "CULL_FACE_MODE", GL_CULL_FACE_MODE);
	define_constant(ctx, proto, "FRONT_FACE", GL_FRONT_FACE);
	define_constant(ctx, proto, "FRONT", GL_FRONT);
	define_constant(ctx, proto, "BACK", GL_BACK);
	define_constant(ctx, proto, "FRONT_AND_BACK", GL_FRONT_AND_BACK);
	define_constant(ctx, proto, "CW", GL_CW);
	define_constant(ctx, proto, "CCW", GL_CCW);
	define_constant(ctx, proto, "DEPTH_TEST", GL_DEPTH_TEST);
	define_constant(ctx, proto, "DITHER", GL_DITHER);
	define_constant(ctx, proto, "BLEND", GL_BLEND);
	define_constant(ctx, proto, "FUNC_ADD", GL_FUNC_ADD);
	define_constant(ctx, proto, "BLEND_EQUATION", GL_BLEND_EQUATION);
	define_constant(ctx, proto, "BLEND_EQUATION_RGB", GL_BLEND_EQUATION_RGB);
	define_constant(ctx, proto, "BLEND_EQUATION_ALPHA",
					GL_BLEND_EQUATION_ALPHA);
	define_constant(ctx, proto, "BLEND_SRC_RGB", GL_BLEND_SRC_RGB);
	define_constant(ctx, proto, "BLEND_DST_RGB", GL_BLEND_DST_RGB);
	define_constant(ctx, proto, "BLEND_SRC_ALPHA", GL_BLEND_SRC_ALPHA);
	define_constant(ctx, proto, "BLEND_DST_ALPHA", GL_BLEND_DST_ALPHA);
	define_constant(ctx, proto, "SCISSOR_TEST", GL_SCISSOR_TEST);
	define_constant(ctx, proto, "SCISSOR_BOX", GL_SCISSOR_BOX);
	define_constant(ctx, proto, "STENCIL_TEST", GL_STENCIL_TEST);
	define_constant(ctx, proto, "STENCIL_CLEAR_VALUE",
					GL_STENCIL_CLEAR_VALUE);
	define_constant(ctx, proto, "STENCIL_WRITEMASK", GL_STENCIL_WRITEMASK);
	define_constant(ctx, proto, "STENCIL_FUNC", GL_STENCIL_FUNC);
	define_constant(ctx, proto, "STENCIL_REF", GL_STENCIL_REF);
	define_constant(ctx, proto, "STENCIL_VALUE_MASK", GL_STENCIL_VALUE_MASK);
	define_constant(ctx, proto, "STENCIL_FAIL", GL_STENCIL_FAIL);
	define_constant(ctx, proto, "STENCIL_PASS_DEPTH_FAIL",
					GL_STENCIL_PASS_DEPTH_FAIL);
	define_constant(ctx, proto, "STENCIL_PASS_DEPTH_PASS",
					GL_STENCIL_PASS_DEPTH_PASS);
	define_constant(ctx, proto, "KEEP", GL_KEEP);
	define_constant(ctx, proto, "VIEWPORT", GL_VIEWPORT);
	define_constant(ctx, proto, "ALIASED_POINT_SIZE_RANGE",
					GL_ALIASED_POINT_SIZE_RANGE);
	define_constant(ctx, proto, "ALIASED_LINE_WIDTH_RANGE",
					GL_ALIASED_LINE_WIDTH_RANGE);
	define_constant(ctx, proto, "DEPTH_CLEAR_VALUE", GL_DEPTH_CLEAR_VALUE);
	define_constant(ctx, proto, "DEPTH_FUNC", GL_DEPTH_FUNC);
	define_constant(ctx, proto, "DEPTH_WRITEMASK", GL_DEPTH_WRITEMASK);
	define_constant(ctx, proto, "COLOR_CLEAR_VALUE", GL_COLOR_CLEAR_VALUE);
	define_constant(ctx, proto, "COLOR_WRITEMASK", GL_COLOR_WRITEMASK);
	define_constant(ctx, proto, "POLYGON_OFFSET_FACTOR",
					GL_POLYGON_OFFSET_FACTOR);
	define_constant(ctx, proto, "POLYGON_OFFSET_UNITS",
					GL_POLYGON_OFFSET_UNITS);
	define_constant(ctx, proto, "RED_BITS", GL_RED_BITS);
	define_constant(ctx, proto, "GREEN_BITS", GL_GREEN_BITS);
	define_constant(ctx, proto, "BLUE_BITS", GL_BLUE_BITS);
	define_constant(ctx, proto, "ALPHA_BITS", GL_ALPHA_BITS);
	define_constant(ctx, proto, "DEPTH_BITS", GL_DEPTH_BITS);
	define_constant(ctx, proto, "STENCIL_BITS", GL_STENCIL_BITS);
	define_constant(ctx, proto, "UNPACK_ALIGNMENT", GL_UNPACK_ALIGNMENT);
	define_constant(ctx, proto, "UNPACK_FLIP_Y_WEBGL",
					GL_UNPACK_FLIP_Y_WEBGL);
	define_constant(ctx, proto, "UNPACK_PREMULTIPLY_ALPHA_WEBGL",
					GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL);
	define_constant(ctx, proto, "UNPACK_COLORSPACE_CONVERSION_WEBGL",
					GL_UNPACK_COLORSPACE_CONVERSION_WEBGL);
	define_constant(ctx, proto, "BROWSER_DEFAULT_WEBGL",
					GL_BROWSER_DEFAULT_WEBGL);
	define_constant(ctx, proto, "PACK_ALIGNMENT", GL_PACK_ALIGNMENT);
	define_constant(ctx, proto, "VENDOR", GL_VENDOR);
	define_constant(ctx, proto, "RENDERER", GL_RENDERER);
	define_constant(ctx, proto, "VERSION", GL_VERSION);
	// gl.hint() (milestone #16).
	define_constant(ctx, proto, "DONT_CARE", GL_DONT_CARE);
	define_constant(ctx, proto, "FASTEST", GL_FASTEST);
	define_constant(ctx, proto, "NICEST", GL_NICEST);
	define_constant(ctx, proto, "GENERATE_MIPMAP_HINT", GL_GENERATE_MIPMAP_HINT);
	define_constant(ctx, proto, "FRAGMENT_SHADER_DERIVATIVE_HINT_OES",
					GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
	define_constant(ctx, proto, "VERTEX_SHADER", GL_VERTEX_SHADER);
	define_constant(ctx, proto, "FRAGMENT_SHADER", GL_FRAGMENT_SHADER);
	define_constant(ctx, proto, "LOW_FLOAT", GL_LOW_FLOAT);
	define_constant(ctx, proto, "MEDIUM_FLOAT", GL_MEDIUM_FLOAT);
	define_constant(ctx, proto, "HIGH_FLOAT", GL_HIGH_FLOAT);
	define_constant(ctx, proto, "LOW_INT", GL_LOW_INT);
	define_constant(ctx, proto, "MEDIUM_INT", GL_MEDIUM_INT);
	define_constant(ctx, proto, "HIGH_INT", GL_HIGH_INT);
	define_constant(ctx, proto, "COMPILE_STATUS", GL_COMPILE_STATUS);
	define_constant(ctx, proto, "LINK_STATUS", GL_LINK_STATUS);
	define_constant(ctx, proto, "DELETE_STATUS", GL_DELETE_STATUS);
	define_constant(ctx, proto, "SHADER_TYPE", GL_SHADER_TYPE);
	define_constant(ctx, proto, "ATTACHED_SHADERS", GL_ATTACHED_SHADERS);
	define_constant(ctx, proto, "CURRENT_PROGRAM", GL_CURRENT_PROGRAM);
	define_constant(ctx, proto, "ACTIVE_UNIFORMS", GL_ACTIVE_UNIFORMS);
	define_constant(ctx, proto, "ACTIVE_ATTRIBUTES", GL_ACTIVE_ATTRIBUTES);
	define_constant(ctx, proto, "BYTE", GL_BYTE);
	define_constant(ctx, proto, "UNSIGNED_BYTE", GL_UNSIGNED_BYTE);
	define_constant(ctx, proto, "SHORT", GL_SHORT);
	define_constant(ctx, proto, "UNSIGNED_SHORT", GL_UNSIGNED_SHORT);
	define_constant(ctx, proto, "INT", GL_INT);
	define_constant(ctx, proto, "UNSIGNED_INT", GL_UNSIGNED_INT);
	define_constant(ctx, proto, "FLOAT", GL_FLOAT);
	define_constant(ctx, proto, "FLOAT_VEC2", GL_FLOAT_VEC2);
	define_constant(ctx, proto, "FLOAT_VEC3", GL_FLOAT_VEC3);
	define_constant(ctx, proto, "FLOAT_VEC4", GL_FLOAT_VEC4);
	define_constant(ctx, proto, "FLOAT_MAT2", GL_FLOAT_MAT2);
	define_constant(ctx, proto, "FLOAT_MAT3", GL_FLOAT_MAT3);
	define_constant(ctx, proto, "FLOAT_MAT4", GL_FLOAT_MAT4);
	define_constant(ctx, proto, "INT_VEC2", GL_INT_VEC2);
	define_constant(ctx, proto, "INT_VEC3", GL_INT_VEC3);
	define_constant(ctx, proto, "INT_VEC4", GL_INT_VEC4);
	define_constant(ctx, proto, "BOOL", GL_BOOL);
	define_constant(ctx, proto, "BOOL_VEC2", GL_BOOL_VEC2);
	define_constant(ctx, proto, "BOOL_VEC3", GL_BOOL_VEC3);
	define_constant(ctx, proto, "BOOL_VEC4", GL_BOOL_VEC4);
	define_constant(ctx, proto, "SAMPLER_2D", GL_SAMPLER_2D);
	define_constant(ctx, proto, "SAMPLER_CUBE", GL_SAMPLER_CUBE);
	define_constant(ctx, proto, "ARRAY_BUFFER", GL_ARRAY_BUFFER);
	define_constant(ctx, proto, "ARRAY_BUFFER_BINDING", GL_ARRAY_BUFFER_BINDING);
	define_constant(ctx, proto, "ELEMENT_ARRAY_BUFFER", GL_ELEMENT_ARRAY_BUFFER);
	define_constant(ctx, proto, "ELEMENT_ARRAY_BUFFER_BINDING",
					GL_ELEMENT_ARRAY_BUFFER_BINDING);
	define_constant(ctx, proto, "FRAMEBUFFER", GL_FRAMEBUFFER);
	define_constant(ctx, proto, "FRAMEBUFFER_BINDING", GL_FRAMEBUFFER_BINDING);
	define_constant(ctx, proto, "RENDERBUFFER", GL_RENDERBUFFER);
	define_constant(ctx, proto, "RENDERBUFFER_BINDING",
					GL_RENDERBUFFER_BINDING);
	define_constant(ctx, proto, "COLOR_ATTACHMENT0", GL_COLOR_ATTACHMENT0);
	define_constant(ctx, proto, "DEPTH_ATTACHMENT", GL_DEPTH_ATTACHMENT);
	define_constant(ctx, proto, "STENCIL_ATTACHMENT", GL_STENCIL_ATTACHMENT);
	define_constant(ctx, proto, "DEPTH_STENCIL_ATTACHMENT",
					GL_DEPTH_STENCIL_ATTACHMENT);
	define_constant(ctx, proto, "FRAMEBUFFER_COMPLETE",
					GL_FRAMEBUFFER_COMPLETE);
	define_constant(ctx, proto, "FRAMEBUFFER_INCOMPLETE_ATTACHMENT",
					GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
	define_constant(ctx, proto, "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT",
					GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
	define_constant(ctx, proto, "FRAMEBUFFER_INCOMPLETE_DIMENSIONS",
					GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
	define_constant(ctx, proto, "FRAMEBUFFER_UNSUPPORTED",
					GL_FRAMEBUFFER_UNSUPPORTED);
	define_constant(ctx, proto, "DEPTH_COMPONENT", GL_DEPTH_COMPONENT);
	define_constant(ctx, proto, "DEPTH_COMPONENT16", GL_DEPTH_COMPONENT16);
	define_constant(ctx, proto, "DEPTH_COMPONENT24", GL_DEPTH_COMPONENT24);
	define_constant(ctx, proto, "DEPTH_STENCIL", GL_DEPTH_STENCIL);
	define_constant(ctx, proto, "DEPTH24_STENCIL8", GL_DEPTH24_STENCIL8);
	define_constant(ctx, proto, "UNSIGNED_INT_24_8_WEBGL",
					GL_UNSIGNED_INT_24_8_WEBGL);
	define_constant(ctx, proto, "STENCIL_INDEX8", GL_STENCIL_INDEX8);
	define_constant(ctx, proto, "RGB", GL_RGB);
	define_constant(ctx, proto, "RGB565", GL_RGB565);
	define_constant(ctx, proto, "RGBA4", GL_RGBA4);
	define_constant(ctx, proto, "RGB5_A1", GL_RGB5_A1);
	define_constant(ctx, proto, "BUFFER_SIZE", GL_BUFFER_SIZE);
	define_constant(ctx, proto, "BUFFER_USAGE", GL_BUFFER_USAGE);
	define_constant(ctx, proto, "STREAM_DRAW", GL_STREAM_DRAW);
	define_constant(ctx, proto, "STATIC_DRAW", GL_STATIC_DRAW);
	define_constant(ctx, proto, "DYNAMIC_DRAW", GL_DYNAMIC_DRAW);
	define_constant(ctx, proto, "TEXTURE_2D", GL_TEXTURE_2D);
	define_constant(ctx, proto, "TEXTURE_BINDING_2D", GL_TEXTURE_BINDING_2D);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP", GL_TEXTURE_CUBE_MAP);
	define_constant(ctx, proto, "TEXTURE_BINDING_CUBE_MAP",
					GL_TEXTURE_BINDING_CUBE_MAP);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_POSITIVE_X",
					GL_TEXTURE_CUBE_MAP_POSITIVE_X);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_NEGATIVE_X",
					GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_POSITIVE_Y",
					GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_NEGATIVE_Y",
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_POSITIVE_Z",
					GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
	define_constant(ctx, proto, "TEXTURE_CUBE_MAP_NEGATIVE_Z",
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
	define_constant(ctx, proto, "TEXTURE0", GL_TEXTURE0);
	define_constant(ctx, proto, "ACTIVE_TEXTURE", GL_ACTIVE_TEXTURE);
	define_constant(ctx, proto, "TEXTURE_MIN_FILTER", GL_TEXTURE_MIN_FILTER);
	define_constant(ctx, proto, "TEXTURE_MAG_FILTER", GL_TEXTURE_MAG_FILTER);
	define_constant(ctx, proto, "TEXTURE_WRAP_S", GL_TEXTURE_WRAP_S);
	define_constant(ctx, proto, "TEXTURE_WRAP_T", GL_TEXTURE_WRAP_T);
	define_constant(ctx, proto, "NEAREST", GL_NEAREST);
	define_constant(ctx, proto, "LINEAR", GL_LINEAR);
	define_constant(ctx, proto, "CLAMP_TO_EDGE", GL_CLAMP_TO_EDGE);
	define_constant(ctx, proto, "REPEAT", GL_REPEAT);
	define_constant(ctx, proto, "RGBA", GL_RGBA);
	define_constant(ctx, proto, "SRC_COLOR", GL_SRC_COLOR);
	define_constant(ctx, proto, "ONE_MINUS_SRC_COLOR", GL_ONE_MINUS_SRC_COLOR);
	define_constant(ctx, proto, "SRC_ALPHA", GL_SRC_ALPHA);
	define_constant(ctx, proto, "ONE_MINUS_SRC_ALPHA",
					GL_ONE_MINUS_SRC_ALPHA);
	define_constant(ctx, proto, "DST_ALPHA", GL_DST_ALPHA);
	define_constant(ctx, proto, "ONE_MINUS_DST_ALPHA", GL_ONE_MINUS_DST_ALPHA);
	define_constant(ctx, proto, "DST_COLOR", GL_DST_COLOR);
	define_constant(ctx, proto, "ONE_MINUS_DST_COLOR", GL_ONE_MINUS_DST_COLOR);
	define_constant(ctx, proto, "SRC_ALPHA_SATURATE", GL_SRC_ALPHA_SATURATE);
	define_constant(ctx, proto, "MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
	define_constant(ctx, proto, "MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS);
	define_constant(ctx, proto, "MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS);
	define_constant(ctx, proto, "MAX_TEXTURE_IMAGE_UNITS",
					GL_MAX_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_VERTEX_TEXTURE_IMAGE_UNITS",
					GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_COMBINED_TEXTURE_IMAGE_UNITS",
					GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	define_constant(ctx, proto, "MAX_CUBE_MAP_TEXTURE_SIZE",
					GL_MAX_CUBE_MAP_TEXTURE_SIZE);
	define_constant(ctx, proto, "MAX_RENDERBUFFER_SIZE",
					GL_MAX_RENDERBUFFER_SIZE);
	define_constant(ctx, proto, "MAX_VERTEX_UNIFORM_VECTORS",
					GL_MAX_VERTEX_UNIFORM_VECTORS);
	define_constant(ctx, proto, "MAX_FRAGMENT_UNIFORM_VECTORS",
					GL_MAX_FRAGMENT_UNIFORM_VECTORS);
	define_constant(ctx, proto, "MAX_VARYING_VECTORS",
					GL_MAX_VARYING_VECTORS);
	define_constant(ctx, proto, "SHADING_LANGUAGE_VERSION",
					GL_SHADING_LANGUAGE_VERSION);
	define_constant(ctx, proto, "POLYGON_OFFSET_FILL", GL_POLYGON_OFFSET_FILL);
	define_constant(ctx, proto, "SAMPLE_ALPHA_TO_COVERAGE",
					GL_SAMPLE_ALPHA_TO_COVERAGE);
	define_constant(ctx, proto, "SAMPLE_COVERAGE", GL_SAMPLE_COVERAGE);

	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry init_function_list[] = {
	JS_CFUNC_DEF("webglContextNew", 0, nx_webgl_context_new),
	JS_CFUNC_DEF("webglContextInitClass", 0, nx_webgl_context_init_class),
};

void nx_init_webgl(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_webgl_context_class_id);
	JSClassDef webgl_context_class = {
		"nx_webgl_context_t",
		.finalizer = finalizer_webgl_context,
	};
	JS_NewClass(rt, nx_webgl_context_class_id, &webgl_context_class);

	JS_NewClassID(rt, &nx_webgl_shader_class_id);
	JSClassDef webgl_shader_class = {
		"nx_webgl_shader_t",
		.finalizer = finalizer_webgl_shader,
	};
	JS_NewClass(rt, nx_webgl_shader_class_id, &webgl_shader_class);

	JS_NewClassID(rt, &nx_webgl_program_class_id);
	JSClassDef webgl_program_class = {
		"nx_webgl_program_t",
		.finalizer = finalizer_webgl_program,
	};
	JS_NewClass(rt, nx_webgl_program_class_id, &webgl_program_class);

	JS_NewClassID(rt, &nx_webgl_buffer_class_id);
	JSClassDef webgl_buffer_class = {
		"nx_webgl_buffer_t",
		.finalizer = finalizer_webgl_buffer,
	};
	JS_NewClass(rt, nx_webgl_buffer_class_id, &webgl_buffer_class);

	JS_NewClassID(rt, &nx_webgl_uniform_location_class_id);
	JSClassDef webgl_uniform_location_class = {
		"nx_webgl_uniform_location_t",
		.finalizer = finalizer_webgl_uniform_location,
	};
	JS_NewClass(rt, nx_webgl_uniform_location_class_id,
				&webgl_uniform_location_class);

	JS_NewClassID(rt, &nx_webgl_texture_class_id);
	JSClassDef webgl_texture_class = {
		"nx_webgl_texture_t",
		.finalizer = finalizer_webgl_texture,
	};
	JS_NewClass(rt, nx_webgl_texture_class_id, &webgl_texture_class);

	JS_NewClassID(rt, &nx_webgl_framebuffer_class_id);
	JSClassDef webgl_framebuffer_class = {
		"nx_webgl_framebuffer_t",
		.finalizer = finalizer_webgl_framebuffer,
	};
	JS_NewClass(rt, nx_webgl_framebuffer_class_id, &webgl_framebuffer_class);

	JS_NewClassID(rt, &nx_webgl_renderbuffer_class_id);
	JSClassDef webgl_renderbuffer_class = {
		"nx_webgl_renderbuffer_t",
		.finalizer = finalizer_webgl_renderbuffer,
	};
	JS_NewClass(rt, nx_webgl_renderbuffer_class_id, &webgl_renderbuffer_class);

	JS_SetPropertyFunctionList(ctx, init_obj, init_function_list,
							   countof(init_function_list));
}
