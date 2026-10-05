// UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @7d57605 L12-238(类型/常量手抄段)
// GL 3.2 Core + GLES3 共用子集的类型与常量(上游 103 个常量全量收录,值与拼写逐一对照)。
// OPT-A6(docs/OPTIMIZATION_TRACKER.md):不引入 glGetError 轮询 —— 错误检查由
// KHR_debug 回调承担(Debug 构建),Release 构建零错误检查调用;此处仅定义类型与常量。
// GL 3.2 Core + GLES3 shared-subset types and constants (all 103 upstream
// constants, values and spellings cross-checked one by one).
// OPT-A6 (docs/OPTIMIZATION_TRACKER.md): no glGetError polling — error
// checking is carried by the KHR_debug callback (Debug builds) and is absent
// in Release builds; this header defines types and constants only.
#pragma once
import std;

namespace ora::gl {

// —— Khronos 标量类型(GL 1.1 语义,C 头等价物) ——
// —— Khronos scalar types (GL 1.1 semantics, C-header equivalents) ——
using GLenum = std::uint32_t;
using GLboolean = std::uint8_t;
using GLbitfield = std::uint32_t;
using GLint = std::int32_t;
using GLsizei = std::int32_t;
using GLuint = std::uint32_t;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

/// KHR_debug 回调签名(OpenGL.cs L209-215 的 DEBUGPROC 对应物)
/// KHR_debug callback signature (the DEBUGPROC counterpart of OpenGL.cs L209-215)
using DEBUGPROC = void (*)(GLenum source, GLenum type, GLuint id, GLenum severity,
                           GLsizei length, const GLchar* message, const void* user_param);

// —— 常量(上游 OpenGL.cs 逐条对照;按上游原拼写保留 GL_ 前缀) ——
// —— Constants (cross-checked against upstream OpenGL.cs; the GL_ prefix is
//    kept in the upstream spelling) ——
inline constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;
inline constexpr GLenum GL_ALPHA_TEST = 0x0BC0;
inline constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
inline constexpr GLenum GL_BGRA = 0x80E1;
inline constexpr GLenum GL_BLEND = 0x0BE2;
inline constexpr GLenum GL_BLEND_COLOR = 0x8005;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum GL_CLIENT_PIXEL_STORE_BIT = 0x0001;
inline constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
inline constexpr GLenum GL_COLOR_BUFFER_BIT = 0x4000;
inline constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
inline constexpr GLenum GL_CONTEXT_LOST = 0x0507;
inline constexpr GLenum GL_DEBUG_OUTPUT = 0x92E0;
inline constexpr GLenum GL_DEBUG_OUTPUT_SYNCHRONOUS = 0x8242;
inline constexpr GLenum GL_DEBUG_SEVERITY_HIGH = 0x9146;
inline constexpr GLenum GL_DEBUG_SEVERITY_LOW = 0x9148;
inline constexpr GLenum GL_DEBUG_SEVERITY_MEDIUM = 0x9147;
inline constexpr GLenum GL_DEBUG_SEVERITY_NOTIFICATION = 0x826B;
inline constexpr GLenum GL_DEBUG_SOURCE_API = 0x8246;
inline constexpr GLenum GL_DEBUG_SOURCE_APPLICATION = 0x824A;
inline constexpr GLenum GL_DEBUG_SOURCE_OTHER = 0x824B;
inline constexpr GLenum GL_DEBUG_SOURCE_SHADER_COMPILER = 0x8248;
inline constexpr GLenum GL_DEBUG_SOURCE_THIRD_PARTY = 0x8249;
inline constexpr GLenum GL_DEBUG_SOURCE_WINDOW_SYSTEM = 0x8247;
inline constexpr GLenum GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR = 0x824D;
inline constexpr GLenum GL_DEBUG_TYPE_ERROR = 0x824C;
inline constexpr GLenum GL_DEBUG_TYPE_MARKER = 0x8268;
inline constexpr GLenum GL_DEBUG_TYPE_OTHER = 0x8251;
inline constexpr GLenum GL_DEBUG_TYPE_PERFORMANCE = 0x8250;
inline constexpr GLenum GL_DEBUG_TYPE_POP_GROUP = 0x826A;
inline constexpr GLenum GL_DEBUG_TYPE_PORTABILITY = 0x824F;
inline constexpr GLenum GL_DEBUG_TYPE_PUSH_GROUP = 0x8269;
inline constexpr GLenum GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR = 0x824E;
inline constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
inline constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x0100;
inline constexpr GLenum GL_DEPTH_COMPONENT = 0x1902;
inline constexpr GLenum GL_DEPTH_COMPONENT16 = 0x81A5;
inline constexpr GLenum GL_DEPTH_TEST = 0x0B71;
inline constexpr GLenum GL_DST_ALPHA = 0x0304;
inline constexpr GLenum GL_DST_COLOR = 0x0306;
inline constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum GL_EXTENSIONS = 0x1F03;
inline constexpr GLenum GL_FLOAT = 0x1406;
inline constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
inline constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
inline constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;
inline constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
inline constexpr GLenum GL_FUNC_ADD = 0x8006;
inline constexpr GLenum GL_FUNC_REVERSE_SUBTRACT = 0x800B;
inline constexpr GLenum GL_FUNC_SUBTRACT = 0x800A;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_INVALID_ENUM = 0x0500;
inline constexpr GLenum GL_INVALID_FRAMEBUFFER_OPERATION = 0x0506;
inline constexpr GLenum GL_INVALID_OPERATION = 0x0502;
inline constexpr GLenum GL_INVALID_VALUE = 0x0501;
inline constexpr GLenum GL_LEQUAL = 0x0203;
inline constexpr GLenum GL_LINEAR = 0x2601;
inline constexpr GLenum GL_LINES = 0x0001;
inline constexpr GLenum GL_LINK_STATUS = 0x8B82;
inline constexpr GLenum GL_NEAREST = 0x2600;
inline constexpr GLenum GL_NUM_EXTENSIONS = 0x821D;
inline constexpr GLenum GL_ONE_MINUS_DST_ALPHA = 0x0305;
inline constexpr GLenum GL_ONE_MINUS_DST_COLOR = 0x0307;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
inline constexpr GLenum GL_ONE_MINUS_SRC_COLOR = 0x0301;
inline constexpr GLenum GL_OUT_OF_MEMORY = 0x0505;
inline constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
inline constexpr GLenum GL_PACK_ROW_LENGTH = 0x0D02;
inline constexpr GLenum GL_RENDERBUFFER = 0x8D41;
inline constexpr GLenum GL_RENDERER = 0x1F01;
inline constexpr GLenum GL_RGBA = 0x1908;
inline constexpr GLenum GL_RGBA16F = 0x881A;
inline constexpr GLenum GL_RGBA8 = 0x8058;
inline constexpr GLenum GL_SAMPLER_2D = 0x8B5E;
inline constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
inline constexpr GLenum GL_SHADING_LANGUAGE_VERSION = 0x8B8C;
inline constexpr GLenum GL_SRC_ALPHA = 0x0302;
inline constexpr GLenum GL_SRC_COLOR = 0x0300;
inline constexpr GLenum GL_STACK_OVERFLOW = 0x0503;
inline constexpr GLenum GL_STACK_UNDERFLOW = 0x0504;
inline constexpr GLenum GL_STATIC_DRAW = 0x88E4;
inline constexpr GLenum GL_STENCIL_BUFFER_BIT = 0x0400;
inline constexpr GLenum GL_STENCIL_TEST = 0x0B90;
inline constexpr GLenum GL_TABLE_TOO_LARGE = 0x8031;
inline constexpr GLenum GL_TEXTURE0 = 0x84C0;
inline constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
inline constexpr GLenum GL_TEXTURE_BASE_LEVEL = 0x813C;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
inline constexpr GLenum GL_TEXTURE_MAX_LEVEL = 0x813D;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum GL_TRIANGLES = 0x0004;
inline constexpr GLenum GL_UNPACK_ROW_LENGTH = 0x0CF2;
inline constexpr GLenum GL_UNPACK_SKIP_PIXELS = 0x0CF4;
inline constexpr GLenum GL_UNPACK_SKIP_ROWS = 0x0CF3;
inline constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum GL_UNSIGNED_INT = 0x1405;
inline constexpr GLenum GL_VENDOR = 0x1F00;
inline constexpr GLenum GL_VERSION = 0x1F02;
inline constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
inline constexpr GLenum GL_VIEWPORT = 0x0BA2;

inline constexpr GLboolean GL_FALSE = 0;
inline constexpr GLboolean GL_TRUE = 1;
inline constexpr GLenum GL_ZERO = 0;
inline constexpr GLenum GL_ONE = 1;
inline constexpr GLenum GL_TEXTURE1 = GL_TEXTURE0 + 1;
inline constexpr GLenum GL_TEXTURE2 = GL_TEXTURE0 + 2;
inline constexpr GLenum GL_TEXTURE3 = GL_TEXTURE0 + 3;
inline constexpr GLenum GL_TEXTURE4 = GL_TEXTURE0 + 4;
inline constexpr GLenum GL_TEXTURE5 = GL_TEXTURE0 + 5;
inline constexpr GLenum GL_TEXTURE6 = GL_TEXTURE0 + 6;
inline constexpr GLenum GL_TEXTURE7 = GL_TEXTURE0 + 7;
inline constexpr GLenum GL_BYTE = 0x1400;
inline constexpr GLenum GL_SHORT = 0x1402;
inline constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
inline constexpr GLenum GL_INT = 0x1404;
inline constexpr GLenum GL_RGB = 0x1907;
inline constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6;
inline constexpr GLenum GL_MAX_TEXTURE_SIZE = 0x0D33;

// —— 第四批新增:glBufferStorage 持久映射 + fence 同步 + blend/深度(上游经
// OpenTK/手写绑定使用;此处按 GL 3.2 Core 规范值收录) ——
// —— Fourth-batch additions: glBufferStorage persistent mapping + fence
//    synchronization + blend/depth (upstream uses these via OpenTK/handwritten
//    bindings; values per the GL 3.2 Core spec) ——

// glMapBufferRange / glBufferStorage 访问标志位 | range-map/storage access flags
inline constexpr GLbitfield GL_MAP_READ_BIT = 0x0001;
inline constexpr GLbitfield GL_MAP_WRITE_BIT = 0x0002;
inline constexpr GLbitfield GL_MAP_PERSISTENT_BIT = 0x0040;
inline constexpr GLbitfield GL_MAP_COHERENT_BIT = 0x0080;

// fence 同步对象 | fence sync objects
using GLsync = void*;
using GLuint64 = std::uint64_t;
inline constexpr GLenum GL_SYNC_GPU_COMMANDS_COMPLETE = 0x9117;  // 0x911D 是 GL_WAIT_FAILED,勿混 | 0x911D is GL_WAIT_FAILED — do not confuse
inline constexpr GLenum GL_ALREADY_SIGNALED = 0x911A;
inline constexpr GLenum GL_TIMEOUT_EXPIRED = 0x911B;
inline constexpr GLenum GL_CONDITION_SATISFIED = 0x911C;
inline constexpr GLuint64 GL_TIMEOUT_IGNORED = ~std::uint64_t{0};
inline constexpr GLbitfield GL_SYNC_FLUSH_COMMANDS_BIT = 0x00000001;

}  // namespace ora::gl
