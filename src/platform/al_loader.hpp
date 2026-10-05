// UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @b6fc03f L20-669(经
// OpenRA-OpenAL-CS 的 AL10/ALC10/ALC11/EFX 调用面)
// OPT-A6 同形态(docs/OPTIMIZATION_TRACKER.md):函数指针直连(LoadLibraryA +
// GetProcAddress 一次加载),上游 OpenRA-OpenAL-CS 的 P/Invoke 包装层消失。
// 常量与类型直接取自 third_party/OpenAL/include 的官方头(al.h/alc.h/efx.h),
// 不再手抄数值(与 GL 层的 gl_types 对照形态不同 —— OpenAL 有官方头可用)。
// Function pointers loaded once via LoadLibraryA + GetProcAddress (the
// OPT-A6 shape, docs/OPTIMIZATION_TRACKER.md); the upstream
// OpenRA-OpenAL-CS P/Invoke wrapper disappears. Constants and types come
// straight from the official headers in third_party/OpenAL/include
// (al.h/alc.h/efx.h) — no hand-copied values (unlike the GL layer's
// gl_types cross-check, OpenAL ships usable official headers).
#pragma once
import std;

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>  // AL_METERS_PER_UNIT(OpenAlSoundEngine.cs L345 的 EFX 调用)| AL_METERS_PER_UNIT (the EFX call of OpenAlSoundEngine.cs L345)

namespace ora::al {

// 声明一个 AL/ALC 入口:签名 typedef + inline 函数指针变量(API 原名保留,便于对照)
// Declares one AL/ALC entry point: signature typedef + inline function-pointer
// variable (API names kept verbatim for cross-referencing).
#define ORA_AL_DECL(name, sig) using PFN_##name = sig; inline PFN_##name name = nullptr;

// —— AL 入口(OpenAlSoundEngine.cs 全文的调用面)——
// —— AL entry points (the call surface of OpenAlSoundEngine.cs) ——
ORA_AL_DECL(alGetError, ALenum (*)(void))
ORA_AL_DECL(alGenSources, void (*)(ALsizei, ALuint*))
ORA_AL_DECL(alDeleteSources, void (*)(ALsizei, const ALuint*))
ORA_AL_DECL(alGenBuffers, void (*)(ALsizei, ALuint*))
ORA_AL_DECL(alDeleteBuffers, void (*)(ALsizei, const ALuint*))
ORA_AL_DECL(alBufferData, void (*)(ALuint, ALenum, const ALvoid*, ALsizei, ALsizei))
ORA_AL_DECL(alSourcef, void (*)(ALuint, ALenum, ALfloat))
ORA_AL_DECL(alSource3f, void (*)(ALuint, ALenum, ALfloat, ALfloat, ALfloat))
ORA_AL_DECL(alSourcei, void (*)(ALuint, ALenum, ALint))
ORA_AL_DECL(alSourceRewind, void (*)(ALuint))
ORA_AL_DECL(alSourcePlay, void (*)(ALuint))
ORA_AL_DECL(alSourcePause, void (*)(ALuint))
ORA_AL_DECL(alSourceStop, void (*)(ALuint))
ORA_AL_DECL(alGetSourcef, void (*)(ALuint, ALenum, ALfloat*))
ORA_AL_DECL(alGetSourcei, void (*)(ALuint, ALenum, ALint*))
ORA_AL_DECL(alListenerf, void (*)(ALenum, ALfloat))
ORA_AL_DECL(alListener3f, void (*)(ALenum, ALfloat, ALfloat, ALfloat))
ORA_AL_DECL(alListenerfv, void (*)(ALenum, const ALfloat*))

// —— ALC 入口(设备/上下文与枚举扩展)——
// —— ALC entry points (device/context and the enumeration extensions) ——
ORA_AL_DECL(alcOpenDevice, ALCdevice* (*)(const ALCchar*))
ORA_AL_DECL(alcCloseDevice, ALCboolean (*)(ALCdevice*))
ORA_AL_DECL(alcCreateContext, ALCcontext* (*)(ALCdevice*, const ALCint*))
ORA_AL_DECL(alcMakeContextCurrent, ALCboolean (*)(ALCcontext*))
ORA_AL_DECL(alcDestroyContext, void (*)(ALCcontext*))
ORA_AL_DECL(alcGetString, const ALCchar* (*)(ALCdevice*, ALCenum))
ORA_AL_DECL(alcIsExtensionPresent, ALCboolean (*)(ALCdevice*, const ALCchar*))

#undef ORA_AL_DECL

/// 加载结果:全部核心入口加载成功即 ok;缺失核心入口给出名字(致命)。
/// Load result: ok when every core entry loaded; a missing core entry reports
/// its name (fatal).
struct AlLoadResult {
  bool b_ok;
  const char* str_missing;
};

/// 经通用加载函数加载全部入口(Windows 实现:LoadLibraryA("OpenAL32.dll") +
/// GetProcAddress;测试可注入自定义 loader 以探测缺失符号)。
/// Loads every entry through the generic loader (the Windows implementation:
/// LoadLibraryA("OpenAL32.dll") + GetProcAddress; tests may inject a custom
/// loader to probe for missing symbols).
AlLoadResult LoadAl(void* (*fn_load)(const char* name));

/// 便捷入口:用 Windows 默认 loader 从 "OpenAL32.dll" 加载(可执行文件旁的
/// 随包 openal-soft 优先于系统路由)。
/// Convenience: load from "OpenAL32.dll" with the default Windows loader (the
/// bundled openal-soft next to the executable wins over the system router).
AlLoadResult LoadAlDefault();

}  // namespace ora::al
