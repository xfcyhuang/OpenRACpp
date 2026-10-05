// UPSTREAM: OpenRA.Mods.Cnc/AudioLoaders/VocLoader.cs @b6fc03f L19-394
//          (voc_loader.hpp 的实现;头注的形态适配说明适用)
//          Implementation of voc_loader.hpp; the shape-adaptation notes of
//          the hpp header apply.
import std;
#include "formats/voc_loader.hpp"

namespace ora::fmt {

namespace {

/// VocFileHeader.Read(L64-72):20 ASCII + 三个 u16。
/// VocFileHeader.Read (L64-72): 20 ASCII bytes + three u16s.
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

  /// Description.StartsWith("Creative Voice File", Ordinal)。
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

/// CheckVocHeader(L114-127):四重校验,消息逐字(X = 大写无衬零十六进制)。
/// CheckVocHeader (L114-127): the four checks, messages verbatim (X =
/// uppercase unpadded hex).
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

/// GetSampleRateFromVocRate(L129-139)。
int GetSampleRateFromVocRate(int int4_voc_sample_rate) {
  if (int4_voc_sample_rate == 256)
    throw std::runtime_error("Invalid frequency divisor 256 in voc file");
  if (int4_voc_sample_rate == 0xa5 || int4_voc_sample_rate == 0xa6)
    return 11025;
  if (int4_voc_sample_rate == 0xd2 || int4_voc_sample_rate == 0xd3)
    return 22050;
  return static_cast<int>(1000000LL / (256LL - int4_voc_sample_rate));
}

/// Preload(L141-269)。
void Preload(SpanReader& reader_source, VocInfo& info) {
  std::vector<VocBlock> vec_block_list;
  info.int4_sample_rate = 0;
  info.int4_total_samples = 0;

  while (true) {
    VocBlock block_cur{};
    // 上游 try { ReadUInt8 } catch (EndOfStream) break —— 流尾净结束
    // Upstream's try { ReadUInt8 } catch (EndOfStream) break — the clean
    // ending at the stream tail.
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
      // Sound data | 声音数据
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

        // 上一块若为附加信息块(code 8),其采样率覆盖本块并移除该块
        // If the previous block is an extra-info block (code 8), its sample
        // rate overrides this block's and the block removes itself.
        if (!vec_block_list.empty()) {
          const VocBlock block_prev = vec_block_list.back();
          if (block_prev.int4_code == 8) {
            block_cur.block_sample.int4_rate = block_prev.block_sample.int4_rate;
            // List.Remove(结构体)删首个按值相等元素(与上游一致)
            // List.Remove(struct) deletes the first structurally equal
            // element (as upstream).
            const auto it_remove = std::ranges::find(vec_block_list, block_prev);
            if (it_remove != vec_block_list.end())
              vec_block_list.erase(it_remove);
          }
        }

        info.int4_sample_rate = std::max(info.int4_sample_rate, block_cur.block_sample.int4_rate);
        break;
      }

      // Silence | 静默
      case 3: {
        if (block_cur.int4_length != 3)
          throw std::runtime_error("Invalid silence block length in voc file");
        block_cur.block_sample.int8_offset = 0;
        block_cur.block_sample.int4_samples = reader_source.ReadUInt16() + 1;
        const auto int4_freq_div = reader_source.ReadUInt8();
        block_cur.block_sample.int4_rate = GetSampleRateFromVocRate(int4_freq_div);
        break;
      }

      // Repeat start | 循环开始
      case 6: {
        if (block_cur.int4_length != 2)
          throw std::runtime_error("Invalid repeat start block length in voc file");
        block_cur.block_loop.int4_count = reader_source.ReadUInt16() + 1;
        break;
      }

      // Repeat end | 循环结束
      case 7:
        break;

      // Extra info | 附加信息
      case 8: {
        if (block_cur.int4_length != 4)
          throw std::runtime_error("Invalid info block length in voc file");
        const std::int32_t int4_freq_div = reader_source.ReadUInt16();
        // 上游对 u16 读数检查 == 65536(恒假,不可达检查照抄)
        // Upstream checks a u16 read against 65536 (always false; the
        // unreachable check kept).
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

      // Sound data (New format) 及其它:上游 default 即抛
      // Sound data (New format) and the rest: upstream's default throws.
      case 9:
      default:
        throw std::runtime_error("Unhandled code in voc file");
    }

    if (int4_skip > 0)
      reader_source.Skip(int4_skip);
    vec_block_list.push_back(block_cur);
  }

  // 校验与总样点数统计(L256-266)
  // The validity check and the total-sample tally (L256-266).
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

}  // namespace

void LoadVoc(std::span<const std::byte> vec_file, VocInfo& info) {
  SpanReader reader_source{vec_file};
  CheckVocHeader(reader_source);
  Preload(reader_source, info);
}

std::vector<std::byte> DecodeVocPcm(std::span<const std::byte> vec_file, const VocInfo& info) {
  // Read/FillBuffer/UpdateBlockIfNeeded(L293-348)的物化等价:按列表序拼接
  // code 1/9 块的 [Offset, Offset+Samples) 区间。
  // The materialized equivalent of Read/FillBuffer/UpdateBlockIfNeeded
  // (L293-348): concatenates [Offset, Offset+Samples) per code 1/9 block in
  // list order.
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
    // Not a (supported) VOC | 非(受支持的)VOC
  }
  return false;
}

}  // namespace ora::fmt
