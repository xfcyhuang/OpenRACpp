// UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @7d57605 L506-669(Bind<T> 加载段)
// 表驱动单次加载;KHR_debug 双候选探测(无后缀 core 4.3 / KHR / ARB 后缀),
// 对应上游 OpenGL.cs L550-562 的后缀拼接探测。
// Table-driven one-shot loading; KHR_debug probes two candidates (the
// suffixless core-4.3 form and the KHR/ARB-suffixed forms), mirroring the
// suffix concatenation of upstream OpenGL.cs L550-562.
import std;

#include "platform/gl_loader.hpp"

namespace ora::gl {

namespace {

template <typename T>
struct Entry {
  const char* str_name;
  T* ptr_slot;
};

/// 逐条加载;核心入口缺失返回其名字(致命),KHR_debug 例外(可选)。
/// Loads entry by entry; a missing core entry reports its name (fatal),
/// with KHR_debug the exception (optional).
template <typename T>
const char* LoadOne(void* (*fn_load)(const char*), const Entry<T>& entry) {
  *entry.ptr_slot = reinterpret_cast<T*>(fn_load(entry.str_name));
  return *entry.ptr_slot == nullptr ? entry.str_name : nullptr;
}

}  // namespace

GlLoadResult LoadGl(void* (*fn_load)(const char* name)) {
  const char* str_missing = nullptr;

#define ORA_GL_LOAD(fn)                                                     \
  do {                                                                      \
    if (str_missing == nullptr) {                                           \
      fn = reinterpret_cast<decltype(fn)>(fn_load("gl" #fn));               \
      if (fn == nullptr)                                                    \
        str_missing = "gl" #fn;                                             \
    }                                                                       \
  } while (0)

  ORA_GL_LOAD(Enable);
  ORA_GL_LOAD(Disable);
  ORA_GL_LOAD(GetError);
  ORA_GL_LOAD(GetString);
  ORA_GL_LOAD(GetStringi);
  ORA_GL_LOAD(PixelStorei);
  ORA_GL_LOAD(GetIntegerv);
  ORA_GL_LOAD(Flush);
  ORA_GL_LOAD(Finish);
  ORA_GL_LOAD(Viewport);
  ORA_GL_LOAD(Clear);
  ORA_GL_LOAD(ClearColor);
  ORA_GL_LOAD(CreateProgram);
  ORA_GL_LOAD(UseProgram);
  ORA_GL_LOAD(GetProgramiv);
  ORA_GL_LOAD(CreateShader);
  ORA_GL_LOAD(ShaderSource);
  ORA_GL_LOAD(CompileShader);
  ORA_GL_LOAD(GetShaderiv);
  ORA_GL_LOAD(AttachShader);
  ORA_GL_LOAD(GetShaderInfoLog);
  ORA_GL_LOAD(LinkProgram);
  ORA_GL_LOAD(DeleteProgram);
  ORA_GL_LOAD(DeleteShader);
  ORA_GL_LOAD(GetProgramInfoLog);
  ORA_GL_LOAD(GetUniformLocation);
  ORA_GL_LOAD(GetActiveUniform);
  ORA_GL_LOAD(BindAttribLocation);
  ORA_GL_LOAD(Uniform1i);
  ORA_GL_LOAD(Uniform1f);
  ORA_GL_LOAD(Uniform2f);
  ORA_GL_LOAD(Uniform3f);
  ORA_GL_LOAD(Uniform1fv);
  ORA_GL_LOAD(Uniform2fv);
  ORA_GL_LOAD(Uniform3fv);
  ORA_GL_LOAD(Uniform4fv);
  ORA_GL_LOAD(UniformMatrix4fv);
  ORA_GL_LOAD(GenBuffers);
  ORA_GL_LOAD(BindBuffer);
  ORA_GL_LOAD(BufferData);
  ORA_GL_LOAD(BufferSubData);
  ORA_GL_LOAD(DeleteBuffers);
  ORA_GL_LOAD(BufferStorage);
  ORA_GL_LOAD(MapBufferRange);
  ORA_GL_LOAD(FenceSync);
  ORA_GL_LOAD(ClientWaitSync);
  ORA_GL_LOAD(DeleteSync);
  ORA_GL_LOAD(VertexAttribPointer);
  ORA_GL_LOAD(VertexAttribIPointer);
  ORA_GL_LOAD(EnableVertexAttribArray);
  ORA_GL_LOAD(DisableVertexAttribArray);
  ORA_GL_LOAD(GenVertexArrays);
  ORA_GL_LOAD(DeleteVertexArrays);
  ORA_GL_LOAD(BindVertexArray);
  ORA_GL_LOAD(DrawArrays);
  ORA_GL_LOAD(DrawElements);
  ORA_GL_LOAD(DrawElementsBaseVertex);
  ORA_GL_LOAD(BlendEquation);
  ORA_GL_LOAD(BlendEquationSeparate);
  ORA_GL_LOAD(BlendFunc);
  ORA_GL_LOAD(DepthFunc);
  ORA_GL_LOAD(Scissor);
  ORA_GL_LOAD(ReadPixels);
  ORA_GL_LOAD(GenTextures);
  ORA_GL_LOAD(DeleteTextures);
  ORA_GL_LOAD(IsTexture);
  ORA_GL_LOAD(BindTexture);
  ORA_GL_LOAD(ActiveTexture);
  ORA_GL_LOAD(TexImage2D);
  ORA_GL_LOAD(TexSubImage2D);
  ORA_GL_LOAD(CopyTexImage2D);
  ORA_GL_LOAD(TexParameteri);
  ORA_GL_LOAD(TexParameterf);
  ORA_GL_LOAD(GenFramebuffers);
  ORA_GL_LOAD(BindFramebuffer);
  ORA_GL_LOAD(FramebufferTexture2D);
  ORA_GL_LOAD(DeleteFramebuffers);
  ORA_GL_LOAD(GenRenderbuffers);
  ORA_GL_LOAD(BindRenderbuffer);
  ORA_GL_LOAD(RenderbufferStorage);
  ORA_GL_LOAD(DeleteRenderbuffers);
  ORA_GL_LOAD(FramebufferRenderbuffer);
  ORA_GL_LOAD(CheckFramebufferStatus);

#undef ORA_GL_LOAD

  // 桌面 GL 专属入口:缺失不算致命(GLES3 profile 无此二者)
  // Desktop-GL-only entries: their absence is not fatal (absent under GLES3).
  GetTexImage = reinterpret_cast<decltype(GetTexImage)>(fn_load("glGetTexImage"));
  BindFragDataLocation = reinterpret_cast<decltype(BindFragDataLocation)>(fn_load("glBindFragDataLocation"));

  // KHR_debug 可选探测(OpenGL.cs L555-562 的后缀序列:无后缀 → KHR → ARB)
  // Optional KHR_debug probing (the suffix sequence of OpenGL.cs L555-562:
  // suffixless → KHR → ARB).
  if (auto* ptr = fn_load("glDebugMessageCallback"); ptr != nullptr)
    DebugMessageCallback = reinterpret_cast<decltype(DebugMessageCallback)>(ptr);
  else
    for (const char* str_suffix : {"KHR", "ARB"}) {
      char str_name[48];
      const auto result_fmt = std::format_to_n(str_name, std::size(str_name) - 1, "glDebugMessageCallback{}", str_suffix);
      *result_fmt.out = '\0';
      if (auto* ptr_suffixed = fn_load(str_name); ptr_suffixed != nullptr) {
        DebugMessageCallback = reinterpret_cast<decltype(DebugMessageCallback)>(ptr_suffixed);
        break;
      }
    }
  if (auto* ptr = fn_load("glDebugMessageInsert"); ptr != nullptr)
    DebugMessageInsert = reinterpret_cast<decltype(DebugMessageInsert)>(ptr);

  return GlLoadResult{str_missing == nullptr, str_missing};
}

}  // namespace ora::gl
