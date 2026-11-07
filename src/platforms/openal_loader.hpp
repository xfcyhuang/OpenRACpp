#pragma once

import std;

namespace ora::platforms {

using pfn_alc_open_device = void* (*)(const char*);
using pfn_alc_create_context = void* (*)(void*, const std::int32_t*);
using pfn_alc_make_context_current = std::uint8_t (*)(void*);
using pfn_alc_destroy_context = void (*)(void*);
using pfn_alc_close_device = std::uint8_t (*)(void*);
using pfn_alc_get_string = const char* (*)(void*, std::int32_t);
using pfn_alc_is_extension_present = std::uint8_t (*)(void*, const char*);
using pfn_al_gen_sources = void (*)(std::int32_t, std::uint32_t*);
using pfn_al_delete_sources = void (*)(std::int32_t, const std::uint32_t*);
using pfn_al_gen_buffers = void (*)(std::int32_t, std::uint32_t*);
using pfn_al_delete_buffers = void (*)(std::int32_t, const std::uint32_t*);
using pfn_al_buffer_data =
    void (*)(std::uint32_t, std::int32_t, const void*, std::int32_t, std::int32_t);
using pfn_al_get_error = std::int32_t (*)(void);
using pfn_al_source_play = void (*)(std::uint32_t);
using pfn_al_source_pause = void (*)(std::uint32_t);
using pfn_al_source_stop = void (*)(std::uint32_t);
using pfn_al_source_rewind = void (*)(std::uint32_t);
using pfn_al_source_i = void (*)(std::uint32_t, std::int32_t, std::int32_t);
using pfn_al_source_f = void (*)(std::uint32_t, std::int32_t, float);
using pfn_al_source_3f = void (*)(std::uint32_t, std::int32_t, float, float, float);
using pfn_al_get_source_i = void (*)(std::uint32_t, std::int32_t, std::int32_t*);
using pfn_al_get_source_f = void (*)(std::uint32_t, std::int32_t, float*);
using pfn_al_listener_f = void (*)(std::int32_t, float);
using pfn_al_listener_3f = void (*)(std::int32_t, float, float, float);
using pfn_al_listener_fv = void (*)(std::int32_t, const float*);

struct openal_ops {
  pfn_alc_open_device fn_alc_open_device{};
  pfn_alc_create_context fn_alc_create_context{};
  pfn_alc_make_context_current fn_alc_make_context_current{};
  pfn_alc_destroy_context fn_alc_destroy_context{};
  pfn_alc_close_device fn_alc_close_device{};
  pfn_alc_get_string fn_alc_get_string{};
  pfn_alc_is_extension_present fn_alc_is_extension_present{};
  pfn_al_gen_sources fn_al_gen_sources{};
  pfn_al_delete_sources fn_al_delete_sources{};
  pfn_al_gen_buffers fn_al_gen_buffers{};
  pfn_al_delete_buffers fn_al_delete_buffers{};
  pfn_al_buffer_data fn_al_buffer_data{};
  pfn_al_get_error fn_al_get_error{};
  pfn_al_source_play fn_al_source_play{};
  pfn_al_source_pause fn_al_source_pause{};
  pfn_al_source_stop fn_al_source_stop{};
  pfn_al_source_rewind fn_al_source_rewind{};
  pfn_al_source_i fn_al_source_i{};
  pfn_al_source_f fn_al_source_f{};
  pfn_al_source_3f fn_al_source_3f{};
  pfn_al_get_source_i fn_al_get_source_i{};
  pfn_al_get_source_f fn_al_get_source_f{};
  pfn_al_listener_f fn_al_listener_f{};
  pfn_al_listener_3f fn_al_listener_3f{};
  pfn_al_listener_fv fn_al_listener_fv{};
};

void openal_probe_override(std::string_view str_path);

[[nodiscard]] std::expected<openal_ops, std::string> load_openal();

}
