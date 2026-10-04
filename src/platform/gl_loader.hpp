// UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @7d57605 L240-669(801 行手写绑定段)
// OPT-A6(docs/OPTIMIZATION_TRACKER.md):函数指针直连(经 SDL_GL_GetProcAddress 一次加载),
// 无委托双重装箱、无每次调用的 CheckGLError 轮询 —— Debug 构建注册 KHR_debug 回调,
// Release 构建零错误检查。KHR_debug 为可选扩展(缺失不致命)。
// Function pointers loaded once via SDL_GL_GetProcAddress — no double-boxed
// delegates and no per-call CheckGLError polling (OPT-A6,
// docs/OPTIMIZATION_TRACKER.md): Debug builds register a KHR_debug callback;
// Release builds do no error checking at all. KHR_debug is optional (a missing
// extension is not fatal).
#pragma once
import std;

#include "platform/gl_types.hpp"

namespace ora::gl {

// 声明一个 GL 入口:typedef 签名 + inline 函数指针变量(GL API 原名保留,便于对照)
// Declares one GL entry point: signature typedef + inline function-pointer
// variable (GL API names kept verbatim for cross-referencing).
#define ORA_GL_DECL(name, sig) using PFN_##name = sig; inline PFN_##name name = nullptr;

// —— 状态与查询(OpenGL.cs L513-518,640-644)——
// —— State & queries (OpenGL.cs L513-518, 640-644) ——
ORA_GL_DECL(Enable, void(*)(GLenum))
ORA_GL_DECL(Disable, void(*)(GLenum))
ORA_GL_DECL(GetError, GLenum(*)(void))
ORA_GL_DECL(GetString, const GLchar*(*)(GLenum))
ORA_GL_DECL(GetStringi, const GLchar*(*)(GLenum, GLuint))
ORA_GL_DECL(PixelStorei, void(*)(GLenum, GLint))
ORA_GL_DECL(GetIntegerv, void(*)(GLenum, GLint*))

// —— KHR_debug(可选;OpenGL.cs L555-562,带后缀探测)——
// —— KHR_debug (optional; OpenGL.cs L555-562, suffix probing) ——
ORA_GL_DECL(DebugMessageCallback, void(*)(DEBUGPROC, const void*))
ORA_GL_DECL(DebugMessageInsert, void(*)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*))

// —— 清屏/视口(OpenGL.cs L579-583)——
// —— Clear/viewport (OpenGL.cs L579-583) ——
ORA_GL_DECL(Flush, void(*)(void))
ORA_GL_DECL(Finish, void(*)(void))
ORA_GL_DECL(Viewport, void(*)(GLint, GLint, GLsizei, GLsizei))
ORA_GL_DECL(Clear, void(*)(GLbitfield))
ORA_GL_DECL(ClearColor, void(*)(GLfloat, GLfloat, GLfloat, GLfloat))

// —— 着色器/程序(OpenGL.cs L584-605)——
// —— Shaders/programs (OpenGL.cs L584-605) ——
ORA_GL_DECL(CreateProgram, GLuint(*)(void))
ORA_GL_DECL(UseProgram, void(*)(GLuint))
ORA_GL_DECL(GetProgramiv, void(*)(GLuint, GLenum, GLint*))
ORA_GL_DECL(CreateShader, GLuint(*)(GLenum))
ORA_GL_DECL(ShaderSource, void(*)(GLuint, GLsizei, const GLchar* const*, const GLint*))
ORA_GL_DECL(CompileShader, void(*)(GLuint))
ORA_GL_DECL(GetShaderiv, void(*)(GLuint, GLenum, GLint*))
ORA_GL_DECL(AttachShader, void(*)(GLuint, GLuint))
ORA_GL_DECL(GetShaderInfoLog, void(*)(GLuint, GLsizei, GLsizei*, GLchar*))
ORA_GL_DECL(LinkProgram, void(*)(GLuint))
ORA_GL_DECL(DeleteProgram, void(*)(GLuint))
ORA_GL_DECL(DeleteShader, void(*)(GLuint))
ORA_GL_DECL(GetProgramInfoLog, void(*)(GLuint, GLsizei, GLsizei*, GLchar*))
ORA_GL_DECL(GetUniformLocation, GLint(*)(GLuint, const GLchar*))ORA_GL_DECL(GetActiveUniform, void(*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*))
ORA_GL_DECL(BindAttribLocation, void(*)(GLuint, GLuint, const GLchar*))
// 桌面 GL 专属(OpenGL.cs L635-639) | Desktop-GL only (OpenGL.cs L635-639)
ORA_GL_DECL(BindFragDataLocation, void(*)(GLuint, GLuint, const GLchar*))

// —— uniform 直连(OPT-A6:location 整数参数,无字符串查找)——
// —— Direct uniforms (OPT-A6: integer-location parameters, no string lookups) ——
ORA_GL_DECL(Uniform1i, void(*)(GLint, GLint))
ORA_GL_DECL(Uniform1f, void(*)(GLint, GLfloat))
ORA_GL_DECL(Uniform2f, void(*)(GLint, GLfloat, GLfloat))
ORA_GL_DECL(Uniform3f, void(*)(GLint, GLfloat, GLfloat, GLfloat))
ORA_GL_DECL(Uniform1fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform2fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform3fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform4fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(UniformMatrix4fv, void(*)(GLint, GLsizei, GLboolean, const GLfloat*))

// —— 缓冲对象(OpenGL.cs L606-615)——
// —— Buffer objects (OpenGL.cs L606-615) ——
ORA_GL_DECL(GenBuffers, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(BindBuffer, void(*)(GLenum, GLuint))
ORA_GL_DECL(BufferData, void(*)(GLenum, GLsizeiptr, const void*, GLenum))
ORA_GL_DECL(BufferSubData, void(*)(GLenum, GLintptr, GLsizeiptr, const void*))
ORA_GL_DECL(DeleteBuffers, void(*)(GLsizei, const GLuint*))

// —— 顶点数组(OpenGL.cs L611-615,646-647)——
// —— Vertex arrays (OpenGL.cs L611-615, 646-647) ——
ORA_GL_DECL(VertexAttribPointer, void(*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
ORA_GL_DECL(VertexAttribIPointer, void(*)(GLuint, GLint, GLenum, GLsizei, const void*))
ORA_GL_DECL(EnableVertexAttribArray, void(*)(GLuint))
ORA_GL_DECL(DisableVertexAttribArray, void(*)(GLuint))
ORA_GL_DECL(GenVertexArrays, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(BindVertexArray, void(*)(GLuint))

// —— 绘制与混合/深度/裁剪(OpenGL.cs L616-622)——
// —— Drawing & blend/depth/scissor (OpenGL.cs L616-622) ——
ORA_GL_DECL(DrawArrays, void(*)(GLenum, GLint, GLsizei))
ORA_GL_DECL(DrawElements, void(*)(GLenum, GLsizei, GLenum, const void*))
ORA_GL_DECL(BlendEquation, void(*)(GLenum))
ORA_GL_DECL(BlendEquationSeparate, void(*)(GLenum, GLenum))
ORA_GL_DECL(BlendFunc, void(*)(GLenum, GLenum))
ORA_GL_DECL(DepthFunc, void(*)(GLenum))
ORA_GL_DECL(Scissor, void(*)(GLint, GLint, GLsizei, GLsizei))
ORA_GL_DECL(ReadPixels, void(*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))

// —— 纹理(OpenGL.cs L623-632,637)——
// —— Textures (OpenGL.cs L623-632, 637) ——
ORA_GL_DECL(GenTextures, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(DeleteTextures, void(*)(GLsizei, const GLuint*))
ORA_GL_DECL(IsTexture, GLboolean(*)(GLuint))
ORA_GL_DECL(BindTexture, void(*)(GLenum, GLuint))
ORA_GL_DECL(ActiveTexture, void(*)(GLenum))
ORA_GL_DECL(TexImage2D, void(*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))
ORA_GL_DECL(TexSubImage2D, void(*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))
ORA_GL_DECL(CopyTexImage2D, void(*)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint))
ORA_GL_DECL(TexParameteri, void(*)(GLenum, GLenum, GLint))
ORA_GL_DECL(TexParameterf, void(*)(GLenum, GLenum, GLfloat))
// 桌面 GL 专属(OpenGL.cs L637) | Desktop-GL only (OpenGL.cs L637)
ORA_GL_DECL(GetTexImage, void(*)(GLenum, GLint, GLenum, GLenum, void*))

// —— 帧缓冲(OpenGL.cs L648-657)——
// —— Framebuffers (OpenGL.cs L648-657) ——
ORA_GL_DECL(GenFramebuffers, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(BindFramebuffer, void(*)(GLenum, GLuint))
ORA_GL_DECL(FramebufferTexture2D, void(*)(GLenum, GLenum, GLenum, GLuint, GLint))
ORA_GL_DECL(DeleteFramebuffers, void(*)(GLsizei, const GLuint*))
ORA_GL_DECL(GenRenderbuffers, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(BindRenderbuffer, void(*)(GLenum, GLuint))
ORA_GL_DECL(RenderbufferStorage, void(*)(GLenum, GLenum, GLsizei, GLsizei))
ORA_GL_DECL(DeleteRenderbuffers, void(*)(GLsizei, const GLuint*))
ORA_GL_DECL(FramebufferRenderbuffer, void(*)(GLenum, GLenum, GLenum, GLuint))
ORA_GL_DECL(CheckFramebufferStatus, GLenum(*)(GLenum))

#undef ORA_GL_DECL

/// 加载结果:全部核心入口加载成功即 ok;缺失核心入口给出名字(致命)。
/// Load result: ok when every core entry loaded; a missing core entry reports
/// its name (fatal).
struct GlLoadResult {
  bool b_ok;
  const char* str_missing;
};

/// 经平台加载函数(如 SDL_GL_GetProcAddress)加载全部核心入口。
/// Loads all core entry points through the platform loader (e.g. SDL_GL_GetProcAddress).
GlLoadResult LoadGl(void* (*fn_load)(const char* name));

/// KHR_debug 是否可用(Debug 构建注册回调前查询)。
/// Whether KHR_debug is available (query before registering the callback in Debug builds).
inline bool HasKHRDebug() { return DebugMessageCallback != nullptr; }

}  // namespace ora::gl
