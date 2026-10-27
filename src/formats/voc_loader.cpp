import std;
#include "formats/voc_loader.hpp"

namespace ora::fmt {

namespace {

struct VocFileHeader {
  std::array<char, 20> arr_description{};
  std::int32_t int4_datablock_offset = 0;
  std::int32_t int4_version = 0;
  std::int32_t int4_id = 0;

  static VocFileHeader Read(SpanReader& reader_source) {
    VocFileHeader vfh_ret;
    for (char& chr_c : vfh_ret.arr_description)
      chr_c = static_cast<char>(reader_source.ReadUInt8());
    vfh_ret.int4_datablock_offset = reader_source.ReadUInt16();
    vfh_ret.int4_version = reader_source.ReadUInt16();
    vfh_ret.int4_id = reader_source.ReadUInt16();
    return vfh_ret;
  }

  bool DescriptionIsCreativeVoiceFile() const {
    constexpr std::string_view kExpected = "Creative Voice File";
    if (kExpected.size() > arr_description.size())
      return false;
    for (std::size_t st_i{}; st_i < kExpected.size(); st_i++)
      if (arr_description[st_i] != kExpected[st_i])
        return false;
    return true;
  }
};

void CheckVocHeader(SpanReader& reader_source) {
  const VocFileHeader vfh = VocFileHeader::Read(reader_source);

  if (!vfh.DescriptionIsCreativeVoiceFile())
    throw std::runtime_error("Voc header description not recognized");
  if (vfh.int4_datablock_offset != 26)
    throw std::runtime_error("Voc header offset is wrong");
  if (vfh.int4_version < 0x0100 || vfh.int4_version >= 0x0200)
    throw std::runtime_error(std::format("Voc header version {:X} not supported", vfh.int4_version));
  if (vfh.int4_id != ~vfh.int4_version + 0x1234)
    throw std::runtime_error(std::format("Voc header id is bogus - expected: {:X} but value is : {:X}",
                                         ~vfh.int4_version + 0x1234, vfh.int4_id));
}

int GetSampleRateFromVocRate(int int4_voc_sample_rate) {
  if (int4_voc_sample_rate == 256)
    throw std::runtime_error("Invalid frequency divisor 256 in voc file");
  if (int4_voc_sample_rate == 0xa5 || int4_voc_sample_rate == 0xa6)
    return 11025;
  if (int4_voc_sample_rate == 0xd2 || int4_voc_sample_rate == 0xd3)
    return 22050;
  return static_cast<int>(1000000LL / (256LL - int4_voc_sample_rate));
}

void Preload(SpanReader& reader_source, VocInfo& info) {
  std::vector<VocBlock> vec_block_list;
  info.int4_sample_rate = 0;
  info.int4_total_samples = 0;

  while (true) {
    VocBlock block_cur{};

    if (reader_source.Position() >= reader_source.Length())
      break;
    block_cur.int4_code = reader_source.ReadUInt8();

    if (block_cur.int4_code == 0 || block_cur.int4_code > 9)
      break;

    block_cur.int4_length = reader_source.ReadUInt8();
    block_cur.int4_length |= reader_source.ReadUInt8() << 8;
    block_cur.int4_length |= reader_source.ReadUInt8() << 16;

    auto int4_skip = 0;
    switch (block_cur.int4_code) {

      case 1: {
        if (block_cur.int4_length < 2)
          throw std::runtime_error("Invalid sound data block length in voc file");
        const auto int4_freq_div = reader_source.ReadUInt8();
        block_cur.block_sample.int4_rate = GetSampleRateFromVocRate(int4_freq_div);
        const auto int4_codec = reader_source.ReadUInt8();
        if (int4_codec != 0)
          throw std::runtime_error("Unhandled codec used in voc file");
        int4_skip = block_cur.int4_length - 2;
        block_cur.block_sample.int4_samples = int4_skip;
        block_cur.block_sample.int8_offset = reader_source.Position();

        if (!vec_block_list.empty()) {
          const VocBlock block_prev = vec_block_list.back();
          if (block_prev.int4_code == 8) {
            block_cur.block_sample.int4_rate = block_prev.block_sample.int4_rate;

            const auto it_remove = std::ranges::find(vec_block_list, block_prev);
            if (it_remove != vec_block_list.end())
              vec_block_list.erase(it_remove);
          }
        }

        info.int4_sample_rate = std::max(info.int4_sample_rate, block_cur.block_sample.int4_rate);
        break;
      }

      case 3: {
        if (block_cur.int4_length != 3)
          throw std::runtime_error("Invalid silence block length in voc file");
        block_cur.block_sample.int8_offset = 0;
        block_cur.block_sample.int4_samples = reader_source.ReadUInt16() + 1;
        const auto int4_freq_div = reader_source.ReadUInt8();
        block_cur.block_sample.int4_rate = GetSampleRateFromVocRate(int4_freq_div);
        break;
      }

      case 6: {
        if (block_cur.int4_length != 2)
          throw std::runtime_error("Invalid repeat start block length in voc file");
        block_cur.block_loop.int4_count = reader_source.ReadUInt16() + 1;
        break;
      }

      case 7:
        break;

      case 8: {
        if (block_cur.int4_length != 4)
          throw std::runtime_error("Invalid info block length in voc file");
        const std::int32_t int4_freq_div = reader_source.ReadUInt16();

        if (int4_freq_div == 65536)
          throw std::runtime_error("Invalid frequency divisor 65536 in voc file");
        const auto int4_codec = reader_source.ReadUInt8();
        if (int4_codec != 0)
          throw std::runtime_error("Unhandled codec used in voc file");
        const auto int4_channels = reader_source.ReadUInt8() + 1;
        if (int4_channels != 1)
          throw std::runtime_error("Unhandled number of channels in voc file");
        block_cur.block_sample.int8_offset = 0;
        block_cur.block_sample.int4_samples = 0;
        block_cur.block_sample.int4_rate =
            static_cast<int>(256000000LL / (65536LL - int4_freq_div));
        break;
      }

      case 9:
      default:
        throw std::runtime_error("Unhandled code in voc file");
    }

    if (int4_skip > 0)
      reader_source.Skip(int4_skip);
    vec_block_list.push_back(block_cur);
  }

  for (const VocBlock& block_b : vec_block_list) {
    if (block_b.int4_code == 8)
      throw std::runtime_error("Unused block 8 in voc file");
    if (block_b.int4_code != 1 && block_b.int4_code != 9)
      continue;
    if (block_b.block_sample.int4_rate != info.int4_sample_rate)
      throw std::runtime_error("Voc file contains chunks with different sample rate");
    info.int4_total_samples += block_b.block_sample.int4_samples;
  }

  info.vec_blocks = std::move(vec_block_list);
  info.fp4_length_in_seconds =
      static_cast<float>(info.int4_total_samples) / static_cast<float>(info.int4_sample_rate);
}

}

void LoadVoc(std::span<const std::byte> vec_file, VocInfo& info) {
  SpanReader reader_source{vec_file};
  CheckVocHeader(reader_source);
  Preload(reader_source, info);
}

std::vector<std::byte> DecodeVocPcm(std::span<const std::byte> vec_file, const VocInfo& info) {

  std::vector<std::byte> vec_pcm;
  vec_pcm.reserve(static_cast<std::size_t>(info.int4_total_samples));
  for (const VocBlock& block_b : info.vec_blocks) {
    if (block_b.int4_code != 1 && block_b.int4_code != 9)
      continue;
    const std::int64_t int8_off = block_b.block_sample.int8_offset;
    const auto st_len = static_cast<std::size_t>(block_b.block_sample.int4_samples);
    if (int8_off < 0 || static_cast<std::uint64_t>(int8_off) + st_len > vec_file.size()) [[unlikely]]
      throw std::runtime_error("Attempted to read outside the source span.");
    const auto span_seg = vec_file.subspan(static_cast<std::size_t>(int8_off), st_len);
    vec_pcm.insert(vec_pcm.end(), span_seg.begin(), span_seg.end());
  }
  return vec_pcm;
}

bool TryParseVoc(std::span<const std::byte> vec_file, VocInfo& info, std::vector<std::byte>& vec_pcm) {
  try {
    LoadVoc(vec_file, info);
    vec_pcm = DecodeVocPcm(vec_file, info);
    return true;
  } catch (const std::exception&) {

  }
  return false;
}

}
