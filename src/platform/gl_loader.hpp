#pragma once
import std;

#include "platform/gl_types.hpp"

namespace ora::gl {

#define ORA_GL_DECL(name, sig) using PFN_##name = sig; inline PFN_##name name = nullptr;

ORA_GL_DECL(Enable, void(*)(GLenum))
ORA_GL_DECL(Disable, void(*)(GLenum))
ORA_GL_DECL(GetError, GLenum(*)(void))
ORA_GL_DECL(GetString, const GLchar*(*)(GLenum))
ORA_GL_DECL(GetStringi, const GLchar*(*)(GLenum, GLuint))
ORA_GL_DECL(PixelStorei, void(*)(GLenum, GLint))
ORA_GL_DECL(GetIntegerv, void(*)(GLenum, GLint*))

ORA_GL_DECL(DebugMessageCallback, void(*)(DEBUGPROC, const void*))
ORA_GL_DECL(DebugMessageInsert, void(*)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*))

ORA_GL_DECL(Flush, void(*)(void))
ORA_GL_DECL(Finish, void(*)(void))
ORA_GL_DECL(Viewport, void(*)(GLint, GLint, GLsizei, GLsizei))
ORA_GL_DECL(Clear, void(*)(GLbitfield))
ORA_GL_DECL(ClearColor, void(*)(GLfloat, GLfloat, GLfloat, GLfloat))

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

ORA_GL_DECL(BindFragDataLocation, void(*)(GLuint, GLuint, const GLchar*))

ORA_GL_DECL(Uniform1i, void(*)(GLint, GLint))
ORA_GL_DECL(Uniform1f, void(*)(GLint, GLfloat))
ORA_GL_DECL(Uniform2f, void(*)(GLint, GLfloat, GLfloat))
ORA_GL_DECL(Uniform3f, void(*)(GLint, GLfloat, GLfloat, GLfloat))
ORA_GL_DECL(Uniform1fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform2fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform3fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(Uniform4fv, void(*)(GLint, GLsizei, const GLfloat*))
ORA_GL_DECL(UniformMatrix4fv, void(*)(GLint, GLsizei, GLboolean, const GLfloat*))

ORA_GL_DECL(GenBuffers, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(BindBuffer, void(*)(GLenum, GLuint))
ORA_GL_DECL(BufferData, void(*)(GLenum, GLsizeiptr, const void*, GLenum))
ORA_GL_DECL(BufferSubData, void(*)(GLenum, GLintptr, GLsizeiptr, const void*))
ORA_GL_DECL(DeleteBuffers, void(*)(GLsizei, const GLuint*))

ORA_GL_DECL(BufferStorage, void(*)(GLenum, GLsizeiptr, const void*, GLbitfield))
ORA_GL_DECL(MapBufferRange, void* (*)(GLenum, GLintptr, GLsizeiptr, GLbitfield))
ORA_GL_DECL(FenceSync, GLsync (*)(GLenum, GLbitfield))
ORA_GL_DECL(ClientWaitSync, GLenum (*)(GLsync, GLbitfield, GLuint64))
ORA_GL_DECL(DeleteSync, void(*)(GLsync))

ORA_GL_DECL(VertexAttribPointer, void(*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
ORA_GL_DECL(VertexAttribIPointer, void(*)(GLuint, GLint, GLenum, GLsizei, const void*))
ORA_GL_DECL(EnableVertexAttribArray, void(*)(GLuint))
ORA_GL_DECL(DisableVertexAttribArray, void(*)(GLuint))
ORA_GL_DECL(GenVertexArrays, void(*)(GLsizei, GLuint*))
ORA_GL_DECL(DeleteVertexArrays, void(*)(GLsizei, const GLuint*))
ORA_GL_DECL(BindVertexArray, void(*)(GLuint))

ORA_GL_DECL(DrawArrays, void(*)(GLenum, GLint, GLsizei))
ORA_GL_DECL(DrawElements, void(*)(GLenum, GLsizei, GLenum, const void*))

ORA_GL_DECL(DrawElementsBaseVertex, void(*)(GLenum, GLsizei, GLenum, const void*, GLint))
ORA_GL_DECL(BlendEquation, void(*)(GLenum))
ORA_GL_DECL(BlendEquationSeparate, void(*)(GLenum, GLenum))
ORA_GL_DECL(BlendFunc, void(*)(GLenum, GLenum))
ORA_GL_DECL(DepthFunc, void(*)(GLenum))
ORA_GL_DECL(Scissor, void(*)(GLint, GLint, GLsizei, GLsizei))
ORA_GL_DECL(ReadPixels, void(*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))

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

ORA_GL_DECL(GetTexImage, void(*)(GLenum, GLint, GLenum, GLenum, void*))

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

struct GlLoadResult {
  bool b_ok;
  const char* str_missing;
};

GlLoadResult LoadGl(void* (*fn_load)(const char* name));

inline bool HasKHRDebug() { return DebugMessageCallback != nullptr; }

}
