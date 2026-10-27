#pragma once
import std;

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>

namespace ora::al {

#define ORA_AL_DECL(name, sig) using PFN_##name = sig; inline PFN_##name name = nullptr;

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

ORA_AL_DECL(alcOpenDevice, ALCdevice* (*)(const ALCchar*))
ORA_AL_DECL(alcCloseDevice, ALCboolean (*)(ALCdevice*))
ORA_AL_DECL(alcCreateContext, ALCcontext* (*)(ALCdevice*, const ALCint*))
ORA_AL_DECL(alcMakeContextCurrent, ALCboolean (*)(ALCcontext*))
ORA_AL_DECL(alcDestroyContext, void (*)(ALCcontext*))
ORA_AL_DECL(alcGetString, const ALCchar* (*)(ALCdevice*, ALCenum))
ORA_AL_DECL(alcIsExtensionPresent, ALCboolean (*)(ALCdevice*, const ALCchar*))

#undef ORA_AL_DECL

struct AlLoadResult {
  bool b_ok;
  const char* str_missing;
};

AlLoadResult LoadAl(void* (*fn_load)(const char* name));

AlLoadResult LoadAlDefault();

}
