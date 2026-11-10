import std;

#include "game/miniyaml.hpp"
#include "game/music_info.hpp"
#include "game/sound/sound.hpp"
#include "game/sound_info.hpp"
#include "mods/common/wav_loader.hpp"
#include "platforms/sound_engine.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_AUDIO_GOLDEN_DIR
#define ORA_AUDIO_GOLDEN_DIR "tests/golden/audio"
#endif

namespace {

void push_ascii(std::vector<std::uint8_t>& vec_bytes, std::string_view str_tag) {
  for (const char chr_c : str_tag) vec_bytes.push_back(static_cast<std::uint8_t>(chr_c));
}

void push_u32(std::vector<std::uint8_t>& vec_bytes, std::uint32_t u4_value) {
  vec_bytes.push_back(static_cast<std::uint8_t>(u4_value & 0xffu));
  vec_bytes.push_back(static_cast<std::uint8_t>((u4_value >> 8) & 0xffu));
  vec_bytes.push_back(static_cast<std::uint8_t>((u4_value >> 16) & 0xffu));
  vec_bytes.push_back(static_cast<std::uint8_t>((u4_value >> 24) & 0xffu));
}

void push_i16(std::vector<std::uint8_t>& vec_bytes, std::int16_t int4_value) {
  const std::uint16_t u4_value{static_cast<std::uint16_t>(int4_value)};
  vec_bytes.push_back(static_cast<std::uint8_t>(u4_value & 0xffu));
  vec_bytes.push_back(static_cast<std::uint8_t>((u4_value >> 8) & 0xffu));
}

void push_i32(std::vector<std::uint8_t>& vec_bytes, std::int32_t int4_value) {
  push_u32(vec_bytes, static_cast<std::uint32_t>(int4_value));
}

void push_fmt_chunk(std::vector<std::uint8_t>& vec_bytes, std::int16_t int4_format,
                    std::int16_t int4_channels, std::int32_t int4_rate,
                    std::int16_t int4_block_align, std::int16_t int4_bits) {
  push_ascii(vec_bytes, "fmt ");
  push_u32(vec_bytes, 16u);
  push_i16(vec_bytes, int4_format);
  push_i16(vec_bytes, int4_channels);
  push_i32(vec_bytes, int4_rate);
  push_i32(vec_bytes, int4_rate * int4_channels * int4_block_align);
  push_i16(vec_bytes, int4_block_align);
  push_i16(vec_bytes, int4_bits);
}

void patch_riff_size(std::vector<std::uint8_t>& vec_bytes) {
  const std::uint32_t u4_size{static_cast<std::uint32_t>(vec_bytes.size() - 8u)};
  vec_bytes[4] = static_cast<std::uint8_t>(u4_size & 0xffu);
  vec_bytes[5] = static_cast<std::uint8_t>((u4_size >> 8) & 0xffu);
  vec_bytes[6] = static_cast<std::uint8_t>((u4_size >> 16) & 0xffu);
  vec_bytes[7] = static_cast<std::uint8_t>((u4_size >> 24) & 0xffu);
}

std::vector<std::uint8_t> make_pcm16_mono_wav() {
  std::vector<std::uint8_t> vec_bytes;
  push_ascii(vec_bytes, "RIFF");
  push_u32(vec_bytes, 0u);
  push_ascii(vec_bytes, "WAVE");
  push_fmt_chunk(vec_bytes, 1, 1, 8000, 2, 16);
  push_ascii(vec_bytes, "data");
  push_u32(vec_bytes, 8u);
  push_i16(vec_bytes, 0);
  push_i16(vec_bytes, 1);
  push_i16(vec_bytes, -1);
  push_i16(vec_bytes, 32767);
  patch_riff_size(vec_bytes);
  return vec_bytes;
}

std::int32_t pcm_i16_at(const std::vector<std::uint8_t>& vec_pcm, std::size_t sz_index) {
  const std::uint16_t u4_lo{static_cast<std::uint16_t>(vec_pcm[sz_index * 2u])};
  const std::uint16_t u4_hi{static_cast<std::uint16_t>(vec_pcm[sz_index * 2u + 1u])};
  return static_cast<std::int16_t>(u4_lo | (u4_hi << 8));
}

std::int64_t pcm_sum_i16(const std::vector<std::uint8_t>& vec_pcm) {
  std::int64_t int4_sum{0};
  for (std::size_t sz_i{0}; sz_i + 1u < vec_pcm.size(); sz_i += 2u)
    int4_sum += pcm_i16_at(vec_pcm, sz_i / 2u);
  return int4_sum;
}

std::vector<std::int32_t> pcm_head8_i16(const std::vector<std::uint8_t>& vec_pcm) {
  std::vector<std::int32_t> vec_head;
  for (std::size_t sz_i{0}; sz_i < 8u && sz_i * 2u + 1u < vec_pcm.size(); ++sz_i)
    vec_head.push_back(pcm_i16_at(vec_pcm, sz_i));
  return vec_head;
}

std::optional<std::string> read_file_bytes(const std::string& str_path) {
  std::ifstream fs_file{str_path, std::ios::binary};
  if (!fs_file) return std::nullopt;
  std::string str_text;
  fs_file.seekg(0, std::ios::end);
  const std::streampos pos_end{fs_file.tellg()};
  str_text.resize(static_cast<std::size_t>(pos_end));
  fs_file.seekg(0, std::ios::beg);
  fs_file.read(str_text.data(), static_cast<std::streamsize>(str_text.size()));
  return str_text;
}

std::string wav_stats_line(std::string_view str_name,
                           const ora::mods::common::decoded_sound& snd) {
  std::int64_t int4_sum{0};
  for (std::size_t sz_i{0}; sz_i + 1u < snd.vec_pcm.size(); sz_i += 2u)
    int4_sum += pcm_i16_at(snd.vec_pcm, sz_i / 2u);
  std::string str_head;
  char buf_chars[32]{};
  for (std::size_t sz_i{0}; sz_i < 8u && sz_i * 2u + 1u < snd.vec_pcm.size(); ++sz_i) {
    const std::int32_t int4_sample{pcm_i16_at(snd.vec_pcm, sz_i)};
    const auto res_chars{std::to_chars(buf_chars, buf_chars + 32u, int4_sample)};
    if (!str_head.empty()) str_head += ",";
    str_head.append(buf_chars, res_chars.ptr);
  }
  return std::format("{}: ch={} bits={} rate={} lenSec={:.6f} pcmBytes={} sum={} head8={}",
                     str_name, snd.int4_channels, snd.int4_sample_bits, snd.int4_sample_rate,
                     static_cast<double>(snd.f4_length_in_seconds), snd.vec_pcm.size(), int4_sum,
                     str_head);
}

std::string fmt_float_r(float f4_value) {
  char buf_chars[32]{};
  const auto res_chars{std::to_chars(buf_chars, buf_chars + 32u, f4_value)};
  return std::string{buf_chars, res_chars.ptr};
}

std::string join_csv(const std::vector<std::string>& vec_items) {
  std::string str_out;
  for (const std::string& str_item : vec_items) {
    if (!str_out.empty()) str_out += ",";
    str_out += str_item;
  }
  return str_out;
}

std::string audio_sound_info_dump(
    const std::vector<std::pair<std::string, ora::game::sound_info>>& vec_infos) {
  std::vector<std::size_t> vec_order(vec_infos.size());
  for (std::size_t sz_i{0}; sz_i < vec_infos.size(); ++sz_i) vec_order[sz_i] = sz_i;
  std::sort(vec_order.begin(), vec_order.end(),
            [&](std::size_t sz_a, std::size_t sz_b) {
              return vec_infos[sz_a].first < vec_infos[sz_b].first;
            });
  std::string str_out;
  for (const std::size_t sz_idx : vec_order) {
    const auto& [str_key, info] = vec_infos[sz_idx];
    str_out += std::format("type {}\n", str_key);
    str_out += std::format("  default_variant {}\n", info.str_default_variant);
    str_out += std::format("  default_prefix {}\n", info.str_default_prefix);
    for (const auto& [str_k, vec_clips] : info.map_variants)
      str_out += std::format("  variants {}={}\n", str_k, join_csv(vec_clips));
    for (const auto& [str_k, vec_clips] : info.map_prefixes)
      str_out += std::format("  prefixes {}={}\n", str_k, join_csv(vec_clips));
    if (!info.set_disable_variants.empty()) {
      std::string str_csv;
      for (const auto& str_item : info.set_disable_variants) {
        if (!str_csv.empty()) str_csv += ",";
        str_csv += str_item;
      }
      str_out += std::format("  disable_variants {}\n", str_csv);
    }
    if (!info.set_disable_prefixes.empty()) {
      std::string str_csv;
      for (const auto& str_item : info.set_disable_prefixes) {
        if (!str_csv.empty()) str_csv += ",";
        str_csv += str_item;
      }
      str_out += std::format("  disable_prefixes {}\n", str_csv);
    }
    for (const auto& [str_k, vec_clips] : info.map_voices)
      str_out += std::format("  voices {}={}|vol={}|int=DoNotPlay\n", str_k, join_csv(vec_clips),
                             fmt_float_r(1.0f));
    for (const auto& [str_k, pool] : info.map_notification_pools) {
      std::string str_kind{"DoNotPlay"};
      if (pool.kind == ora::game::interrupt_type::interrupt) str_kind = "Interrupt";
      if (pool.kind == ora::game::interrupt_type::overlap) str_kind = "Overlap";
      str_out += std::format("  notifications {}={}|vol={}|int={}\n", str_k,
                             join_csv(pool.vec_clips), fmt_float_r(pool.f4_volume_modifier),
                             str_kind);
    }
  }
  return str_out;
}

std::string audio_music_dump(
    const std::vector<std::pair<std::string, ora::game::music_info>>& vec_tracks) {
  std::vector<std::size_t> vec_order(vec_tracks.size());
  for (std::size_t sz_i{0}; sz_i < vec_tracks.size(); ++sz_i) vec_order[sz_i] = sz_i;
  std::sort(vec_order.begin(), vec_order.end(),
            [&](std::size_t sz_a, std::size_t sz_b) {
              return vec_tracks[sz_a].first < vec_tracks[sz_b].first;
            });
  std::string str_out;
  for (const std::size_t sz_idx : vec_order) {
    const auto& [str_key, track] = vec_tracks[sz_idx];
    str_out += std::format("track {}|{}|{}|hidden={}|vol={}\n", str_key, track.str_filename,
                           track.str_title, track.b_hidden ? "True" : "False",
                           fmt_float_r(track.f4_volume_modifier));
  }
  return str_out;
}

std::vector<std::string> golden_data_lines(const std::string& str_text) {
  std::vector<std::string> vec_lines;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t idx_nl{str_text.find('\n', sz_pos)};
    const std::size_t sz_end{idx_nl == std::string::npos ? str_text.size() : idx_nl};
    if (sz_end > sz_pos && str_text[sz_pos] != '#')
      vec_lines.emplace_back(str_text.substr(sz_pos, sz_end - sz_pos));
    if (idx_nl == std::string::npos) break;
    sz_pos = idx_nl + 1;
  }
  return vec_lines;
}

}

int main() {
  {
    std::vector<std::uint8_t> vec_bad{'R', 'I', 'F', 'X', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    check(!ora::mods::common::wav_try_parse(vec_bad).has_value(), "non-RIFF rejected");
  }
  {
    std::vector<std::uint8_t> vec_bad{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', '2'};
    check(!ora::mods::common::wav_try_parse(vec_bad).has_value(), "non-WAVE form type rejected");
  }
  {
    const std::vector<std::uint8_t> vec_bytes{make_pcm16_mono_wav()};
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "pcm16 mono wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->int4_channels == 1, "pcm16 mono channels");
      check(opt_snd->int4_sample_bits == 16, "pcm16 mono bits");
      check(opt_snd->int4_sample_rate == 8000, "pcm16 mono rate");
      check(opt_snd->vec_pcm.size() == 8u, "pcm16 mono pcm byte count");
      check(pcm_sum_i16(opt_snd->vec_pcm) == 32767, "pcm16 mono sum");
      const std::vector<std::int32_t> vec_head{pcm_head8_i16(opt_snd->vec_pcm)};
      check(vec_head.size() == 4u && vec_head[0] == 0 && vec_head[1] == 1 && vec_head[2] == -1 &&
                vec_head[3] == 32767,
            "pcm16 mono head samples");
      check(std::fabs(opt_snd->f4_length_in_seconds - 0.00325f) < 1e-6f,
            "pcm16 mono length from file length");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes{make_pcm16_mono_wav()};
    for (std::size_t sz_i{0}; sz_i < 10u; ++sz_i) vec_bytes.push_back(0xABu);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "trailing junk wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->vec_pcm.size() == 8u, "trailing junk ignored in pcm");
      check(std::fabs(opt_snd->f4_length_in_seconds - 0.003875f) < 1e-6f,
            "length uses whole file including trailing junk");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 1, 1, 8000, 2, 16);
    push_ascii(vec_bytes, "LIST");
    push_u32(vec_bytes, 3u);
    push_ascii(vec_bytes, "INF");
    vec_bytes.push_back(0x00u);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 8u);
    push_i16(vec_bytes, 5);
    push_i16(vec_bytes, 6);
    push_i16(vec_bytes, 7);
    push_i16(vec_bytes, 8);
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "odd-size LIST chunk with pad byte skipped");
    if (opt_snd.has_value()) {
      check(opt_snd->vec_pcm.size() == 8u, "data decoded after odd chunk alignment");
      check(pcm_sum_i16(opt_snd->vec_pcm) == 26, "aligned data sum");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 2, 1, 22050, 1024, 4);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 4u);
    push_u32(vec_bytes, 0u);
    patch_riff_size(vec_bytes);
    check(!ora::mods::common::wav_try_parse(vec_bytes).has_value(),
          "MS-ADPCM compression rejected");
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 85, 1, 22050, 1024, 4);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 4u);
    push_u32(vec_bytes, 0u);
    patch_riff_size(vec_bytes);
    check(!ora::mods::common::wav_try_parse(vec_bytes).has_value(),
          "unknown compression type rejected");
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 1, 6, 22050, 12, 16);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 4u);
    push_u32(vec_bytes, 0u);
    patch_riff_size(vec_bytes);
    check(!ora::mods::common::wav_try_parse(vec_bytes).has_value(),
          "six channel wav rejected");
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 1, 1, 8000, 2, 16);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 100u);
    push_i16(vec_bytes, 42);
    push_i16(vec_bytes, 43);
    push_i16(vec_bytes, 44);
    push_i16(vec_bytes, 45);
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "defective data chunk size clamps to file end");
    if (opt_snd.has_value()) {
      check(opt_snd->vec_pcm.size() == 8u, "clamped data byte count");
      check(pcm_sum_i16(opt_snd->vec_pcm) == 174, "clamped data sum");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 1, 1, 8000, 1, 8);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 4u);
    vec_bytes.push_back(200u);
    vec_bytes.push_back(0u);
    vec_bytes.push_back(127u);
    vec_bytes.push_back(255u);
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "pcm8 wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->int4_sample_bits == 8, "pcm8 bits passthrough");
      check(opt_snd->vec_pcm.size() == 4u, "pcm8 byte count");
      check(opt_snd->vec_pcm[0] == 200u && opt_snd->vec_pcm[1] == 0u &&
                opt_snd->vec_pcm[2] == 127u && opt_snd->vec_pcm[3] == 255u,
            "pcm8 unsigned bytes passthrough");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 17, 1, 22050, 1024, 4);
    push_ascii(vec_bytes, "fact");
    push_u32(vec_bytes, 4u);
    push_i32(vec_bytes, 6);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 1024u);
    push_i16(vec_bytes, 100);
    vec_bytes.push_back(5u);
    vec_bytes.push_back(0u);
    for (std::size_t sz_i{0}; sz_i < 1020u; ++sz_i) vec_bytes.push_back(0u);
    vec_bytes[60] = 0x07u;
    vec_bytes[61] = 0x23u;
    vec_bytes[62] = 0x45u;
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "ima adpcm wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->int4_sample_bits == 16, "ima reports 16 bit output");
      check(opt_snd->vec_pcm.size() == 12u, "fact chunk drives output size");
      const std::vector<std::uint8_t> vec_expected{100, 0, 122, 0, 125, 0, 144, 0, 156, 0, 181, 0};
      check(opt_snd->vec_pcm == vec_expected, "ima decode hand-worked samples");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 17, 1, 44100, 1024, 4);
    push_ascii(vec_bytes, "fact");
    push_u32(vec_bytes, 4u);
    push_i32(vec_bytes, 3000);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 2048u);
    push_i16(vec_bytes, 10);
    vec_bytes.push_back(0u);
    vec_bytes.push_back(0u);
    for (std::size_t sz_i{0}; sz_i < 1020u; ++sz_i) vec_bytes.push_back(0u);
    push_i16(vec_bytes, -10);
    vec_bytes.push_back(0u);
    vec_bytes.push_back(0u);
    for (std::size_t sz_i{0}; sz_i < 1020u; ++sz_i) vec_bytes.push_back(0u);
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "two block ima wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->vec_pcm.size() == 6000u, "two block output ends mid second block");
      bool b_all_match{true};
      for (std::size_t sz_i{0}; sz_i < 2041u; ++sz_i)
        if (pcm_i16_at(opt_snd->vec_pcm, sz_i) != 10) b_all_match = false;
      for (std::size_t sz_i{2041u}; sz_i * 2u < opt_snd->vec_pcm.size(); ++sz_i)
        if (pcm_i16_at(opt_snd->vec_pcm, sz_i) != -10) b_all_match = false;
      check(b_all_match, "block header reseeds predictor per block");
    }
  }
  {
    std::vector<std::uint8_t> vec_bytes;
    push_ascii(vec_bytes, "RIFF");
    push_u32(vec_bytes, 0u);
    push_ascii(vec_bytes, "WAVE");
    push_fmt_chunk(vec_bytes, 17, 2, 22050, 16, 4);
    push_ascii(vec_bytes, "fact");
    push_u32(vec_bytes, 4u);
    push_i32(vec_bytes, 9);
    push_ascii(vec_bytes, "data");
    push_u32(vec_bytes, 16u);
    push_i16(vec_bytes, 100);
    vec_bytes.push_back(0u);
    vec_bytes.push_back(0u);
    push_i16(vec_bytes, 200);
    vec_bytes.push_back(0u);
    vec_bytes.push_back(0u);
    for (std::size_t sz_i{0}; sz_i < 8u; ++sz_i) vec_bytes.push_back(0u);
    patch_riff_size(vec_bytes);
    const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
    check(opt_snd.has_value(), "stereo ima wav parses");
    if (opt_snd.has_value()) {
      check(opt_snd->vec_pcm.size() == 36u, "stereo output size");
      bool b_all_match{true};
      for (std::size_t sz_i{0}; sz_i * 4u + 3u < opt_snd->vec_pcm.size(); ++sz_i) {
        if (pcm_i16_at(opt_snd->vec_pcm, sz_i * 2u) != 100) b_all_match = false;
        if (pcm_i16_at(opt_snd->vec_pcm, sz_i * 2u + 1u) != 200) b_all_match = false;
      }
      check(b_all_match, "stereo interleave left right alternation");
    }
  }
  {
    const std::string str_wav_dir{std::string{ORA_AUDIO_GOLDEN_DIR} + "/wav"};
    const auto opt_stats{read_file_bytes(str_wav_dir + "/stats.txt")};
    check(opt_stats.has_value(), "golden wav stats readable");
    if (opt_stats.has_value()) {
      std::vector<std::string> vec_golden;
      std::size_t sz_pos{0};
      const std::string& str_stats{*opt_stats};
      while (sz_pos < str_stats.size()) {
        const std::size_t idx_nl{str_stats.find('\n', sz_pos)};
        if (idx_nl == std::string::npos) {
          vec_golden.emplace_back(str_stats.substr(sz_pos));
          break;
        }
        vec_golden.emplace_back(str_stats.substr(sz_pos, idx_nl - sz_pos));
        sz_pos = idx_nl + 1;
      }
      std::size_t sz_line{0};
      for (const std::string_view str_name :
           {"CHAT1REVERSE.WAV", "DEVASTATORMELTDOWN.WAV", "ORNIBOMB.WAV",
            "RELEASESARDAUKARS.WAV"}) {
        const auto opt_bytes{read_file_bytes(str_wav_dir + "/" + std::string{str_name})};
        check(opt_bytes.has_value(), std::format("{} corpus readable", str_name));
        if (!opt_bytes.has_value()) continue;
        std::vector<std::uint8_t> vec_bytes{opt_bytes->begin(), opt_bytes->end()};
        const auto opt_snd{ora::mods::common::wav_try_parse(vec_bytes)};
        check(opt_snd.has_value(), std::format("{} decodes", str_name));
        if (!opt_snd.has_value()) continue;
        const std::vector<std::string> vec_golden{golden_data_lines(*opt_stats)};
        check(sz_line < vec_golden.size(), std::format("{} golden line present", str_name));
        if (sz_line >= vec_golden.size()) continue;
        check(wav_stats_line(str_name, *opt_snd) == vec_golden[sz_line],
              std::format("{} stats line matches oracle", str_name));
        ++sz_line;
      }
      check(sz_line == 4u, "four corpus anchors compared");
    }
  }
  {
    namespace ns = ora::game;
    auto res_parse{ora::game::miniyaml_from_string("Speech:\n\tNotifications:\n\t\tEmpty: \n", "t", true)};
    check(res_parse.has_value(), "synthetic yaml parses");
    if (res_parse.has_value()) {
      const auto res_info{ns::sound_info_from_yaml((*res_parse)[0])};
      check(res_info.has_value(), "sound info from empty notifications parses");
      if (res_info.has_value()) {
        check(res_info->str_default_variant == ".aud", "default variant is .aud");
        check(res_info->str_default_prefix.empty(), "default prefix empty");
        check(res_info->map_notification_pools.contains("Empty"), "empty notification pool exists");
        check(res_info->map_notification_pools.at("Empty").vec_clips.empty(),
              "empty clips from blank value");
        check(res_info->map_notification_pools.at("Empty").f4_volume_modifier == 1.0f,
              "default volume modifier 1");
        check(res_info->map_notification_pools.at("Empty").kind ==
                  ns::interrupt_type::do_not_play,
              "default interrupt do not play");
        check(res_info->map_voice_pools.empty(), "no voice pools without voices node");
      }
    }
  }
  {
    namespace ns = ora::game;
    const std::string str_yaml{
        "Sounds:\n\tNotifications:\n\t\tCashTickUp:\n\t\t\tVolumeModifier: 0.33\n\t\t\t"
        "InterruptType: Overlap\n\t\tChatLine: rabeep1\n\t\t\tInterruptType: interrupt\n\t\t"
        "RadarUp: radaron2\n"};
    auto res_parse{ora::game::miniyaml_from_string(str_yaml, "t", true)};
    check(res_parse.has_value(), "params yaml parses");
    if (res_parse.has_value()) {
      const auto res_info{ns::sound_info_from_yaml((*res_parse)[0])};
      check(res_info.has_value(), "sound info with pool params parses");
      if (res_info.has_value()) {
        check(res_info->map_notification_pools.size() == 3u, "three notification pools");
        check(res_info->map_notification_pools.at("CashTickUp").f4_volume_modifier == 0.33f,
              "volume modifier parsed as float");
        check(res_info->map_notification_pools.at("CashTickUp").kind ==
                  ns::interrupt_type::overlap,
              "interrupt type overlap");
        check(res_info->map_notification_pools.at("ChatLine").kind == ns::interrupt_type::interrupt,
              "interrupt type case insensitive");
        check(res_info->map_notification_pools.at("RadarUp").vec_clips.size() == 1u &&
                  res_info->map_notification_pools.at("RadarUp").vec_clips[0] == "radaron2",
              "single clip pool");
      }
    }
  }
  {
    namespace ns = ora::game;
    const std::string str_yaml{
        "GenericVoice:\n\tVariants:\n\t\tallies: .v01,.v03\n\t\tsoviet: .r01,.r03\n\tVoices:\n\t\t"
        "Select: await1,ready,report1,yessir1\n\t\tAction: ackno, affirm1 ,,\n\t"
        "DisableVariants: Die, Burned, Zapped\n\tDefaultVariant: .wav\n"};
    auto res_parse{ora::game::miniyaml_from_string(str_yaml, "t", true)};
    check(res_parse.has_value(), "voices yaml parses");
    if (res_parse.has_value()) {
      const auto res_info{ns::sound_info_from_yaml((*res_parse)[0])};
      check(res_info.has_value(), "sound info voices parses");
      if (res_info.has_value()) {
        check(res_info->map_variants.size() == 2u, "two variant factions");
        check(res_info->map_variants.at("allies").size() == 2u &&
                  res_info->map_variants.at("allies")[0] == ".v01" &&
                  res_info->map_variants.at("allies")[1] == ".v03",
              "variant clips split and trimmed");
        check(res_info->map_voices.at("Action").size() == 2u &&
                  res_info->map_voices.at("Action")[0] == "ackno" &&
                  res_info->map_voices.at("Action")[1] == "affirm1",
              "csv empty entries removed and trimmed");
        check(res_info->map_voice_pools.at("Select").f4_volume_modifier == 1.0f,
              "voice pool volume fixed 1");
        check(res_info->map_voice_pools.at("Select").kind == ns::interrupt_type::do_not_play,
              "voice pool interrupt fixed do not play");
        check(res_info->set_disable_variants.size() == 3u &&
                  res_info->set_disable_variants.contains("Die") &&
                  res_info->set_disable_variants.contains("Burned") &&
                  res_info->set_disable_variants.contains("Zapped"),
              "disable variants csv to set");
        check(res_info->str_default_variant == ".wav", "default variant overridden");
        check(res_info->map_notification_pools.empty(), "no notification pools for voices type");
      }
    }
  }
  {
    namespace ns = ora::game;
    const std::string str_yaml{
        "Types:\n\tVoices:\n\t\tSelect: a\n\t\tSelect: b\n\tUnknownField: x\n"};
    auto res_parse{ora::game::miniyaml_from_string(str_yaml, "t", true)};
    check(res_parse.has_value(), "dup yaml parses");
    if (res_parse.has_value()) {
      const auto res_info{ns::sound_info_from_yaml((*res_parse)[0])};
      check(!res_info.has_value(), "duplicate voice pool key rejected");
    }
  }
  {
    namespace ns = ora::game;
    check(ns::parse_float_value("0.33").has_value() &&
              *ns::parse_float_value("0.33") == 0.33f,
          "float parse plain");
    check(ns::parse_float_value("50%").has_value() &&
              std::fabs(*ns::parse_float_value("50%") - 0.5f) < 1e-6f,
          "float parse percent");
    check(!ns::parse_float_value("abc").has_value(), "float parse rejects garbage");
    check(ns::parse_interrupt_type("Overlap") == ns::interrupt_type::overlap,
          "enum parse overlap");
    check(ns::parse_interrupt_type("DONOTPLAY") == ns::interrupt_type::do_not_play,
          "enum parse case insensitive");
    check(ns::parse_interrupt_type("1") == ns::interrupt_type::interrupt,
          "enum parse numeric");
    check(!ns::parse_interrupt_type("junk").has_value(), "enum parse rejects junk");
    const std::vector<std::string> vec_csv{ns::parse_string_csv(" a ,,b,c ")};
    check(vec_csv.size() == 3u && vec_csv[0] == "a" && vec_csv[1] == "b" && vec_csv[2] == "c",
          "csv trims and drops empties");
    check(ns::parse_string_csv("").empty(), "csv empty value empty array");
  }
  {
    namespace ns = ora::game;
    ora::game::support::MersenneTwister mt_rng{ora::game::support::make_mersenne_twister(1234)};
    ns::sound_pool pool_empty{};
    const auto opt_none{ns::sound_pool_get_next(pool_empty, mt_rng)};
    check(!opt_none.has_value(), "empty pool returns null");
    check(mt_rng.int4_total_count == 0, "empty pool consumes no rng");

    ns::sound_pool pool{};
    pool.vec_clips = {"a", "b", "c"};
    std::vector<std::string> vec_drawn;
    for (std::size_t sz_i{0}; sz_i < 3u; ++sz_i) {
      const auto opt_clip{ns::sound_pool_get_next(pool, mt_rng)};
      check(opt_clip.has_value(), "pool draw returns clip");
      if (opt_clip.has_value()) vec_drawn.push_back(*opt_clip);
    }
    check(mt_rng.int4_total_count == 2, "size one draw consumes nothing");
    std::sort(vec_drawn.begin(), vec_drawn.end());
    check(vec_drawn.size() == 3u && vec_drawn[0] == "a" && vec_drawn[1] == "b" &&
              vec_drawn[2] == "c",
          "all clips drawn before refill");
    const auto opt_refill{ns::sound_pool_get_next(pool, mt_rng)};
    check(opt_refill.has_value(), "pool refills after exhaust");
    check(mt_rng.int4_total_count == 3, "refill draw consumes rng");
  }
  {
    namespace ns = ora::game;
    const std::string str_yaml{
        "await_r: Afterlife (Await)\n\tVolumeModifier: 0.7\nintro: Intro\n\tHidden: true\n\t"
        "VolumeModifier: 0.9\n2nd_hand: The Second Hand\n"};
    auto res_parse{ora::game::miniyaml_from_string(str_yaml, "t", true)};
    check(res_parse.has_value(), "music yaml parses");
    if (res_parse.has_value()) {
      const auto res_info{ns::music_info_from_yaml("await_r", (*res_parse)[0])};
      check(res_info.has_value(), "music info parses");
      if (res_info.has_value()) {
        check(res_info->str_title == "Afterlife (Await)", "music title from node value");
        check(!res_info->b_hidden, "music hidden default false");
        check(res_info->f4_volume_modifier == 0.7f, "music volume modifier");
        check(res_info->str_filename == "await_r.aud", "music filename from key plus aud");
      }
      const auto res_hidden{ns::music_info_from_yaml("INTRO", (*res_parse)[1])};
      check(res_hidden.has_value(), "hidden music parses");
      if (res_hidden.has_value()) {
        check(res_hidden->b_hidden, "music hidden true");
        check(res_hidden->f4_volume_modifier == 0.9f, "hidden music volume");
      }
      const std::string str_ext_yaml{
          "tune: T\n\tExtension: wav\n\tFilename: custom-name\n"};
      auto res_ext{ora::game::miniyaml_from_string(str_ext_yaml, "t", true)};
      check(res_ext.has_value(), "extension yaml parses");
      if (res_ext.has_value()) {
        const auto res_info{ns::music_info_from_yaml("tune", (*res_ext)[0])};
        check(res_info.has_value() && res_info->str_filename == "custom-name.wav",
              "music extension and filename override");
      }
      check(res_info.has_value() && res_info->b_exists == false && res_info->int4_length == 0,
            "music exists and length default false zero");
    }
  }
  {
    const std::string str_audio_dir{ORA_AUDIO_GOLDEN_DIR};
    auto res_notifications{ora::game::load_sound_infos(str_audio_dir + "/yaml/notifications.yaml")};
    check(res_notifications.has_value(), "ra notifications yaml loads");
    if (res_notifications.has_value()) {
      check(res_notifications->size() == 2u, "notifications has two types");
      bool b_found_speech{false};
      std::size_t sz_speech_pools{0};
      for (const auto& [str_key, info] : *res_notifications) {
        if (str_key == "speech") {
          b_found_speech = true;
          sz_speech_pools = info.map_notification_pools.size();
        }
      }
      check(b_found_speech, "speech type lowered");
      check(sz_speech_pools == 113u, "speech has 113 notification pools");
      const auto opt_golden{read_file_bytes(str_audio_dir + "/notifications.txt")};
      check(opt_golden.has_value(), "golden notifications dump readable");
      if (opt_golden.has_value()) {
        const std::vector<std::string> vec_golden{golden_data_lines(*opt_golden)};
        const std::string str_produced{audio_sound_info_dump(*res_notifications)};
        std::vector<std::string> vec_produced;
        std::size_t sz_pos{0};
        while (sz_pos < str_produced.size()) {
          const std::size_t idx_nl{str_produced.find('\n', sz_pos)};
          const std::size_t sz_end{idx_nl == std::string::npos ? str_produced.size() : idx_nl};
          vec_produced.emplace_back(str_produced.substr(sz_pos, sz_end - sz_pos));
          if (idx_nl == std::string::npos) break;
          sz_pos = idx_nl + 1;
        }
        check(vec_produced == vec_golden, "notifications dump matches oracle line by line");
      }
    }
  }
  {
    const std::string str_audio_dir{ORA_AUDIO_GOLDEN_DIR};
    auto res_voices{ora::game::load_sound_infos(str_audio_dir + "/yaml/voices.yaml")};
    check(res_voices.has_value(), "ra voices yaml loads");
    if (res_voices.has_value()) {
      check(res_voices->size() == 15u, "voices has 15 types");
      const auto opt_golden{read_file_bytes(str_audio_dir + "/voices.txt")};
      check(opt_golden.has_value(), "golden voices dump readable");
      if (opt_golden.has_value()) {
        const std::vector<std::string> vec_golden{golden_data_lines(*opt_golden)};
        const std::string str_produced{audio_sound_info_dump(*res_voices)};
        std::vector<std::string> vec_produced;
        std::size_t sz_pos{0};
        while (sz_pos < str_produced.size()) {
          const std::size_t idx_nl{str_produced.find('\n', sz_pos)};
          const std::size_t sz_end{idx_nl == std::string::npos ? str_produced.size() : idx_nl};
          vec_produced.emplace_back(str_produced.substr(sz_pos, sz_end - sz_pos));
          if (idx_nl == std::string::npos) break;
          sz_pos = idx_nl + 1;
        }
        check(vec_produced == vec_golden, "voices dump matches oracle line by line");
      }
    }
  }
  {
    const std::string str_audio_dir{ORA_AUDIO_GOLDEN_DIR};
    auto res_music{ora::game::load_music_infos(str_audio_dir + "/yaml/music.yaml")};
    check(res_music.has_value(), "ra music yaml loads");
    if (res_music.has_value()) {
      check(res_music->size() == 40u, "music has 40 tracks");
      bool b_intro_hidden{false};
      for (const auto& [str_key, track] : *res_music)
        if (str_key == "intro" && track.b_hidden) b_intro_hidden = true;
      check(b_intro_hidden, "intro track hidden");
      const auto opt_golden{read_file_bytes(str_audio_dir + "/music.txt")};
      check(opt_golden.has_value(), "golden music dump readable");
      if (opt_golden.has_value()) {
        const std::vector<std::string> vec_golden{golden_data_lines(*opt_golden)};
        const std::string str_produced{audio_music_dump(*res_music)};
        std::vector<std::string> vec_produced;
        std::size_t sz_pos{0};
        while (sz_pos < str_produced.size()) {
          const std::size_t idx_nl{str_produced.find('\n', sz_pos)};
          const std::size_t sz_end{idx_nl == std::string::npos ? str_produced.size() : idx_nl};
          vec_produced.emplace_back(str_produced.substr(sz_pos, sz_end - sz_pos));
          if (idx_nl == std::string::npos) break;
          sz_pos = idx_nl + 1;
        }
        check(vec_produced == vec_golden, "music dump matches oracle line by line");
      }
    }
  }
  {
    namespace pl = ora::platforms;
    pl::openal_probe_override("ora-audio-test-no-such-dir/soft_oal.dll");
    pl::sound_engine engine{pl::make_sound_engine("Null Device")};
    check(pl::engine_dummy(engine), "openal load failure falls back to dummy");
    const std::vector<pl::sound_device> vec_devices{pl::engine_available_devices(engine)};
    check(vec_devices.size() == 1u && !vec_devices[0].opt_device.has_value() &&
              vec_devices[0].str_label == "No Sound Output",
          "dummy devices single no sound output");
    const pl::sound_source src{
        pl::engine_add_sound_source_from_memory(engine, {}, 1, 16, 22050)};
    check(!src.b_valid, "dummy source is null source");
    const std::optional<pl::sound> opt_snd{
        pl::engine_play_2d(engine, src, false, false, pl::wpos{}, 1.0f, false, 0)};
    check(opt_snd.has_value() && opt_snd->ptr_state == nullptr,
          "dummy play returns null sound not nullopt");
    check(!pl::sound_complete(engine, *opt_snd), "null sound never completes");
    const std::optional<float> opt_seek{pl::sound_seek_position(engine, *opt_snd)};
    check(opt_seek.has_value() && *opt_seek == 0.0f, "null sound seek position zero");
    check(pl::engine_volume_get(engine) == 0.0f, "dummy engine volume reads zero");
    pl::engine_volume_set(engine, 0.5f);
    check(pl::engine_volume_get(engine) == 0.0f, "dummy engine volume set no-op");
    pl::engine_stop_sound(engine, &*opt_snd);
    pl::engine_stop_all_sounds(engine);
    pl::engine_set_all_sounds_paused(engine, true);
    pl::engine_set_listener_position(engine, pl::wpos{1, 2, 3});
    pl::engine_dispose(engine);
    check(engine.ptr_state == nullptr, "dummy dispose clears state");
    pl::openal_probe_override("");
  }
  {
    namespace pl = ora::platforms;
    pl::openal_probe_override("ora-audio-test-no-such-dir/soft_oal.dll");
    ora::game::sound_facade facade{ora::game::make_sound_facade("Null Device", false)};
    check(pl::engine_dummy(facade.engine), "facade falls back to dummy engine");
    check(ora::game::sound_facade_tick(facade) == ora::game::sound_tick_event::k_none,
          "facade tick without music is no-op");
    check(!facade.b_music_playing, "facade tick without music keeps flag clear");
    ora::game::sound_facade facade_muted{ora::game::make_sound_facade("Null Device", true)};
    check(pl::engine_volume_get(facade_muted.engine) == 0.0f, "facade mute zeroes volume");
    pl::openal_probe_override("");
  }
  return ora::test::test_exit_code("audio");
}
