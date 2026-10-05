// UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @b6fc03f(加载面 =
// OpenRA-OpenAL-CS 的 DllImport 表;此处为其 C++ 直连等价物)
// 加载器实现:Windows LoadLibraryA + GetProcAddress 表驱动(gl_loader 同款;
// DLL 句柄进程级常驻,与上下文生命周期解耦 —— OpenAL 规范允许 alcShutdown 后
// 复用函数指针,本项目不调用 alcShutdown)。
// Loader implementation: table-driven Windows LoadLibraryA + GetProcAddress
// (the same arrangement as gl_loader; the DLL handle stays process-lifetime,
// decoupled from the context lifetime — the OpenAL spec allows reusing the
// function pointers after context destruction, and we never call alcShutdown).
import std;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/al_loader.hpp"

namespace ora::al {

namespace {

HMODULE hmodule_al_ = nullptr;  // OpenAL32.dll(进程级;不显式卸载)| OpenAL32.dll (process-lifetime; never explicitly freed)

/// 默认 loader:首次调用打开 DLL,后续复用。
/// Default loader: opens the DLL on first use, reuses it afterwards.
void* DefaultAlLoad(const char* str_name) {
  if (hmodule_al_ == nullptr) {
    // 可执行文件旁的随包 openal-soft 优先(Windows 默认搜索序:应用目录在
    // 系统目录之前)。
    // The bundled openal-soft next to the executable wins (Windows default
    // search order: the application directory precedes the system directory).
    hmodule_al_ = LoadLibraryA("OpenAL32.dll");
  }
  if (hmodule_al_ == nullptr)
    return nullptr;
  return reinterpret_cast<void*>(GetProcAddress(hmodule_al_, str_name));
}

/// 表驱动加载(ORA_AL_DECL 名字串与声明严格一致)。
/// Table-driven loading (the names match the ORA_AL_DECL declarations exactly).
struct AlEntry {
  const char* str_name;
  void** pptr_fn;
};

}  // namespace

AlLoadResult LoadAl(void* (*fn_load)(const char* name)) {
  const AlEntry arr_entries[] = {
      {"alGetError", reinterpret_cast<void**>(&alGetError)},
      {"alGenSources", reinterpret_cast<void**>(&alGenSources)},
      {"alDeleteSources", reinterpret_cast<void**>(&alDeleteSources)},
      {"alGenBuffers", reinterpret_cast<void**>(&alGenBuffers)},
      {"alDeleteBuffers", reinterpret_cast<void**>(&alDeleteBuffers)},
      {"alBufferData", reinterpret_cast<void**>(&alBufferData)},
      {"alSourcef", reinterpret_cast<void**>(&alSourcef)},
      {"alSource3f", reinterpret_cast<void**>(&alSource3f)},
      {"alSourcei", reinterpret_cast<void**>(&alSourcei)},
      {"alSourceRewind", reinterpret_cast<void**>(&alSourceRewind)},
      {"alSourcePlay", reinterpret_cast<void**>(&alSourcePlay)},
      {"alSourcePause", reinterpret_cast<void**>(&alSourcePause)},
      {"alSourceStop", reinterpret_cast<void**>(&alSourceStop)},
      {"alGetSourcef", reinterpret_cast<void**>(&alGetSourcef)},
      {"alGetSourcei", reinterpret_cast<void**>(&alGetSourcei)},
      {"alListenerf", reinterpret_cast<void**>(&alListenerf)},
      {"alListener3f", reinterpret_cast<void**>(&alListener3f)},
      {"alListenerfv", reinterpret_cast<void**>(&alListenerfv)},
      {"alcOpenDevice", reinterpret_cast<void**>(&alcOpenDevice)},
      {"alcCloseDevice", reinterpret_cast<void**>(&alcCloseDevice)},
      {"alcCreateContext", reinterpret_cast<void**>(&alcCreateContext)},
      {"alcMakeContextCurrent", reinterpret_cast<void**>(&alcMakeContextCurrent)},
      {"alcDestroyContext", reinterpret_cast<void**>(&alcDestroyContext)},
      {"alcGetString", reinterpret_cast<void**>(&alcGetString)},
      {"alcIsExtensionPresent", reinterpret_cast<void**>(&alcIsExtensionPresent)},
  };

  for (const AlEntry& entry : arr_entries) {
    *entry.pptr_fn = fn_load(entry.str_name);
    if (*entry.pptr_fn == nullptr)
      return {false, entry.str_name};
  }
  return {true, nullptr};
}

AlLoadResult LoadAlDefault() {
  return LoadAl(&DefaultAlLoad);
}

}  // namespace ora::al
