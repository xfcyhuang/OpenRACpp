import std;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/al_loader.hpp"

namespace ora::al {

namespace {

HMODULE hmodule_al_ = nullptr;

void* DefaultAlLoad(const char* str_name) {
  if (hmodule_al_ == nullptr) {

    hmodule_al_ = LoadLibraryA("OpenAL32.dll");
  }
  if (hmodule_al_ == nullptr)
    return nullptr;
  return reinterpret_cast<void*>(GetProcAddress(hmodule_al_, str_name));
}

struct AlEntry {
  const char* str_name;
  void** pptr_fn;
};

}

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

}
