module;
#include <windows.h>
module ora.platforms;

import std;

namespace ora::platforms {

namespace {

std::string& override_path() {
  static std::string str_path;
  return str_path;
}

std::wstring utf8_to_wide(std::string_view str_text) {
  if (str_text.empty()) return std::wstring{};
  const int int4_len{MultiByteToWideChar(CP_UTF8, 0, str_text.data(),
                                         static_cast<int>(str_text.size()), nullptr, 0)};
  std::wstring str_out(static_cast<std::size_t>(int4_len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str_text.data(), static_cast<int>(str_text.size()),
                      str_out.data(), int4_len);
  return str_out;
}

std::string exe_directory() {
  wchar_t chr_buffer[MAX_PATH]{};
  const DWORD u4_len{GetModuleFileNameW(nullptr, chr_buffer, MAX_PATH)};
  if (u4_len == 0 || u4_len >= MAX_PATH) return std::string{};
  const std::filesystem::path path_exe{chr_buffer};
  return path_exe.parent_path().string();
}

bool try_load_library(const std::string& str_path, HMODULE& ptr_module) {
  const std::wstring str_wide{utf8_to_wide(str_path)};
  ptr_module = LoadLibraryW(str_wide.c_str());
  return ptr_module != nullptr;
}

void* resolve_symbol(HMODULE ptr_module, const char* chr_name) {
  return reinterpret_cast<void*>(GetProcAddress(ptr_module, chr_name));
}

}

void openal_probe_override(std::string_view str_path) {
  override_path() = std::string{str_path};
}

std::expected<openal_ops, std::string> load_openal() {
  std::vector<std::string> vec_candidates;
  if (!override_path().empty()) {
    vec_candidates.push_back(override_path());
  } else {
    const std::string str_exe_dir{exe_directory()};
    if (!str_exe_dir.empty()) {
      vec_candidates.push_back(str_exe_dir + "/soft_oal.dll");
      vec_candidates.push_back(str_exe_dir + "/openal32.dll");
    }
    vec_candidates.push_back("soft_oal.dll");
    vec_candidates.push_back("openal32.dll");
  }

  for (const std::string& str_candidate : vec_candidates) {
    HMODULE ptr_module{nullptr};
    if (!try_load_library(str_candidate, ptr_module)) continue;

    openal_ops ops{};
    bool b_all_resolved{true};
    const char* chr_names[] = {"alcOpenDevice",
                               "alcCreateContext",
                               "alcMakeContextCurrent",
                               "alcDestroyContext",
                               "alcCloseDevice",
                               "alcGetString",
                               "alcIsExtensionPresent",
                               "alGenSources",
                               "alDeleteSources",
                               "alGenBuffers",
                               "alDeleteBuffers",
                               "alBufferData",
                               "alGetError",
                               "alSourcePlay",
                               "alSourcePause",
                               "alSourceStop",
                               "alSourceRewind",
                               "alSourcei",
                               "alSourcef",
                               "alSource3f",
                               "alGetSourcei",
                               "alGetSourcef",
                               "alListenerf",
                               "alListener3f",
                               "alListenerfv"};
    void** ptr_targets[] = {reinterpret_cast<void**>(&ops.fn_alc_open_device),
                            reinterpret_cast<void**>(&ops.fn_alc_create_context),
                            reinterpret_cast<void**>(&ops.fn_alc_make_context_current),
                            reinterpret_cast<void**>(&ops.fn_alc_destroy_context),
                            reinterpret_cast<void**>(&ops.fn_alc_close_device),
                            reinterpret_cast<void**>(&ops.fn_alc_get_string),
                            reinterpret_cast<void**>(&ops.fn_alc_is_extension_present),
                            reinterpret_cast<void**>(&ops.fn_al_gen_sources),
                            reinterpret_cast<void**>(&ops.fn_al_delete_sources),
                            reinterpret_cast<void**>(&ops.fn_al_gen_buffers),
                            reinterpret_cast<void**>(&ops.fn_al_delete_buffers),
                            reinterpret_cast<void**>(&ops.fn_al_buffer_data),
                            reinterpret_cast<void**>(&ops.fn_al_get_error),
                            reinterpret_cast<void**>(&ops.fn_al_source_play),
                            reinterpret_cast<void**>(&ops.fn_al_source_pause),
                            reinterpret_cast<void**>(&ops.fn_al_source_stop),
                            reinterpret_cast<void**>(&ops.fn_al_source_rewind),
                            reinterpret_cast<void**>(&ops.fn_al_source_i),
                            reinterpret_cast<void**>(&ops.fn_al_source_f),
                            reinterpret_cast<void**>(&ops.fn_al_source_3f),
                            reinterpret_cast<void**>(&ops.fn_al_get_source_i),
                            reinterpret_cast<void**>(&ops.fn_al_get_source_f),
                            reinterpret_cast<void**>(&ops.fn_al_listener_f),
                            reinterpret_cast<void**>(&ops.fn_al_listener_3f),
                            reinterpret_cast<void**>(&ops.fn_al_listener_fv)};
    for (std::size_t sz_i{0}; sz_i < 25u; ++sz_i) {
      *ptr_targets[sz_i] = resolve_symbol(ptr_module, chr_names[sz_i]);
      if (*ptr_targets[sz_i] == nullptr) b_all_resolved = false;
    }

    if (!b_all_resolved) {
      FreeLibrary(ptr_module);
      continue;
    }

    return ops;
  }

  return std::unexpected{std::string{"OpenAL native library not found (soft_oal.dll/openal32.dll)"}};
}

}
