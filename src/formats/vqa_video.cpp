// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VqaVideo.cs @7d57605(全文)+
// VqaLoader.cs:逐句照抄。异常面:三条消息逐字(Invalid vqa×2 /
// Unknown sub-chunk / Vqa uses unknown Subtype)+ 参数-less 的
// NotSupportedException/IndexOutOfRangeException 以 .NET 全名 + 默认消息
// 等价抛(仅坏数据可达)。Seek 越端:FileStream 允许 Position>Length 而
// 读取时抛;SpanReader 在 Seek 即抛 —— 时机差异不可观测于合法数据。
// [UPSTREAM continued] VqaVideo.cs in full plus VqaLoader.cs, copied
// statement by statement. The exception surface: four verbatim messages
// (the two Invalid-vqa texts / Unknown sub-chunk / Vqa uses unknown
// Subtype) with the parameterless NotSupportedException/
// IndexOutOfRangeException thrown as the .NET full name plus the default
// message (malformed data only). A Seek past end: FileStream allows
// Position>Length and throws at read time; SpanReader throws at the Seek —
// the timing difference is unobservable for valid data.
#include "formats/vqa_video.hpp"

#include "formats/ima_adpcm.hpp"
#include "formats/lcw.hpp"
#include "gfx/gfx_util.hpp"

namespace ora::fmt {
namespace {

bool TagIs(std::span<const std::byte> vec_tag, std::string_view str_ascii) {
  return vec_tag.size() == str_ascii.size() &&
         std::equal(str_ascii.begin(), str_ascii.end(), vec_tag.begin(),
                    [](char chr_c, std::byte byte_b) { return static_cast<std::byte>(chr_c) == byte_b; });
}

/// 上游 ReadASCII(4) 的 string 消费面(type[3] / 异常消息内插):字节按
/// char 逐位拼(含 '\0')。
/// The string consumers of upstream's ReadASCII(4) (type[3] / the exception
/// message interpolation): bytes assembled char by char (NULs included).
std::string TagStr(std::span<const std::byte> vec_tag) {
  auto str_out = std::string{};
  for (const auto byte_b : vec_tag)
    str_out.push_back(static_cast<char>(byte_b));
  return str_out;
}

/// 上游数组直写越界(IndexOutOfRangeException 默认消息)的等价抛点。
/// The equivalent throw of upstream's direct out-of-bounds array write
/// (IndexOutOfRangeException's default message).
std::byte At(std::span<const std::byte> vec, std::int64_t int8_i) {
  if (int8_i < 0 || int8_i >= static_cast<std::int64_t>(vec.size())) [[unlikely]]
    throw std::runtime_error("System.IndexOutOfRangeException: Index was outside the bounds of the array.");
  return vec[static_cast<std::size_t>(int8_i)];
}

void ThrowNotSupported() {
  throw std::runtime_error("System.NotSupportedException: Specified method is not supported.");
}

}  // namespace

bool VqaLoader::TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                              std::unique_ptr<IVideo>& out_video) const {
  out_video = nullptr;
  if (vec_file.empty())
    return false;
  if (!IsWestwoodVqa(vec_file))
    return false;

  out_video = std::make_unique<VqaVideo>(vec_file, b_use_frame_padding);
  return true;
}

bool IsWestwoodVqa(std::span<const std::byte> vec_file) {
  auto reader = SpanReader{vec_file};
  if (!TagIs(reader.ReadBytes(4), "FORM"))
    return false;

  if (reader.ReadUInt32() == 0)
    return false;

  return TagIs(reader.ReadBytes(4), "WVQA");
}

VqaVideo::VqaVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding) : vec_file_(vec_file) {
  auto reader = SpanReader{vec_file_};

  // Decode FORM chunk
  if (!TagIs(reader.ReadBytes(4), "FORM"))
    throw std::runtime_error("System.IO.InvalidDataException: Invalid vqa (invalid FORM section)");
  reader.ReadUInt32();  // length

  if (!TagIs(reader.ReadBytes(8), "WVQAVQHD"))
    throw std::runtime_error("System.IO.InvalidDataException: Invalid vqa (not WVQAVQHD)");
  reader.ReadUInt32();  // length2

  reader.ReadUInt16();  // version
  uint4_video_flags_ = reader.ReadUInt16();
  uint2_frame_count_ = reader.ReadUInt16();
  uint2_width_ = reader.ReadUInt16();
  uint2_height_ = reader.ReadUInt16();

  uint2_block_width_ = reader.ReadUInt8();
  uint2_block_height_ = reader.ReadUInt8();
  uint1_framerate_ = reader.ReadUInt8();
  uint1_chunk_buffer_parts_ = reader.ReadUInt8();
  blocks_ = int2{uint2_width_ / uint2_block_width_, uint2_height_ / uint2_block_height_};

  uint2_num_colors_ = reader.ReadUInt16();
  reader.ReadUInt16();  // maxBlocks
  reader.ReadUInt16();  // unknown1
  reader.ReadUInt32();  // unknown2

  // Audio
  int4_sample_rate_ = reader.ReadUInt16();
  int4_audio_channels_ = reader.ReadUInt8();
  int4_sample_bits_ = reader.ReadUInt8();

  reader.ReadUInt32();  // unknown3
  reader.ReadUInt16();  // unknown4
  reader.ReadUInt32();  // maxCbfzSize, unreliable

  reader.ReadUInt32();  // unknown5

  if (IsHqVqa()) {
    vec_cbf_buffer_.assign(kMaxCbfzSize, std::byte{0});
    vec_cbf_heap_.assign(static_cast<std::size_t>(kMaxCbfzSize) * 3, std::byte{0});
    vec_orig_data_.assign(kMaxCbfzSize, std::byte{0});
  } else {
    const auto st_pixels = static_cast<std::size_t>(uint2_width_) * uint2_height_;
    vec_cbf_buffer_.assign(st_pixels, std::byte{0});
    vec_cbf_heap_.assign(st_pixels, std::byte{0});
    vec_cbp_.assign(st_pixels, std::byte{0});
    vec_orig_data_.assign(2 * static_cast<std::size_t>(blocks_.X) * blocks_.Y, std::byte{0});
  }

  vec_cbf_ = vec_cbf_heap_;
  vec_palette_bytes_.assign(static_cast<std::size_t>(uint2_num_colors_) * 4, std::byte{0});

  vec_file_buffer_.assign(kMaxCbfzSize, std::byte{0});

  auto vec_type = reader.ReadBytes(4);
  while (!TagIs(vec_type, "FINF")) {
    // Sub type is a file tag
    if (static_cast<char>(vec_type[3]) == 'F') {
      const auto int4_jmp = static_cast<std::int32_t>(std::byteswap(reader.ReadUInt32()));
      reader.Skip(int4_jmp);
      vec_type = reader.ReadBytes(4);
    } else
      throw std::runtime_error(std::format("System.NotSupportedException: Vqa uses unknown Subtype: {}",
                                           TagStr(vec_type)));
  }

  reader.ReadUInt16();  // length
  reader.ReadUInt16();  // unknown4

  // Frame offsets
  vec_offsets_.resize(uint2_frame_count_);
  for (std::size_t st_i = 0; st_i < vec_offsets_.size(); st_i++) {
    vec_offsets_[st_i] = reader.ReadUInt32();
    if (vec_offsets_[st_i] > 0x40000000)
      vec_offsets_[st_i] -= 0x40000000;
    vec_offsets_[st_i] <<= 1;
  }

  CollectAudioData();

  if (b_use_frame_padding) {
    const auto int4_frame_size = gfx::NextPowerOf2(std::max<int>(uint2_width_, uint2_height_));
    vec_current_frame_data_.assign(static_cast<std::size_t>(int4_frame_size) * int4_frame_size * 4, std::byte{0});
    uint2_total_frame_width_ = static_cast<std::uint16_t>(int4_frame_size);
  } else {
    vec_current_frame_data_.assign(static_cast<std::size_t>(uint2_width_) * uint2_height_ * 4, std::byte{0});
    uint2_total_frame_width_ = uint2_width_;
  }

  Reset();
}

void VqaVideo::Reset() {
  int4_current_frame_index_ = 0;
  int4_chunk_buffer_offset_ = 0;
  int4_current_chunk_buffer_ = 0;
  LoadFrame();
}

void VqaVideo::CollectAudioData() {
  auto reader = SpanReader{vec_file_};
  auto vec_audio1 = std::vector<std::byte>{};  // left channel / mono
  auto vec_audio2 = std::vector<std::byte>{};  // right channel
  auto int4_adpcm_index = 0;
  auto b_compressed = false;
  for (std::size_t st_i = 0; st_i < vec_offsets_.size(); st_i++) {
    reader.Seek(vec_offsets_[st_i]);
    const auto int8_end = st_i < vec_offsets_.size() - 1
                              ? static_cast<std::int64_t>(vec_offsets_[st_i + 1])
                              : reader.Length();

    while (reader.Position() < int8_end) {
      auto vec_type = reader.ReadBytes(4);
      if (TagIs(vec_type, "SN2J")) {
        const auto int4_jmp = static_cast<std::int32_t>(std::byteswap(reader.ReadUInt32()));
        reader.Skip(int4_jmp);
        vec_type = reader.ReadBytes(4);
      }

      const auto int4_length = static_cast<std::int32_t>(std::byteswap(reader.ReadUInt32()));

      if (TagIs(vec_type, "SND0") || TagIs(vec_type, "SND2")) {
        if (int4_audio_channels_ == 0)
          ThrowNotSupported();
        else if (int4_audio_channels_ == 1) {
          const auto vec_raw_audio = reader.ReadBytes(int4_length);
          vec_audio1.insert(vec_audio1.end(), vec_raw_audio.begin(), vec_raw_audio.end());
        } else {
          auto vec_raw_audio = reader.ReadBytes(int4_length / 2);
          vec_audio1.insert(vec_audio1.end(), vec_raw_audio.begin(), vec_raw_audio.end());
          vec_raw_audio = reader.ReadBytes(int4_length / 2);
          vec_audio2.insert(vec_audio2.end(), vec_raw_audio.begin(), vec_raw_audio.end());
          if (int4_length % 2 != 0)
            reader.Skip(2);
        }

        b_compressed = TagIs(vec_type, "SND2");
      } else {
        if (static_cast<std::int64_t>(int4_length) + reader.Position() > reader.Length())
          throw std::runtime_error(std::format("System.NotSupportedException: Vqa uses unknown Subtype: {}",
                                               TagStr(vec_type)));
        reader.Skip(int4_length);
      }

      // Chunks are aligned on even bytes; advance by a byte if the next one is null
      if (reader.Peek() == 0)
        reader.ReadUInt8();
    }
  }

  // L228-237 的 GetAudioData 局部函数:未压缩直用;SND2 全量走 IMA
  // ADPCM(index 跨声道各自持久)。
  // The GetAudioData local of L228-237: uncompressed passes straight
  // through; SND2 runs the whole buffer through IMA ADPCM (index persisting
  // per channel).
  const auto get_audio_data = [&int4_adpcm_index](bool b_comp, std::vector<std::byte>& vec_audio) {
    if (!b_comp)
      return vec_audio;

    auto vec_result = std::vector<std::byte>(vec_audio.size() * 4, std::byte{0});
    ima_adpcm::LoadImaAdpcmSound(vec_audio, int4_adpcm_index, vec_result);
    return vec_result;
  };

  if (int4_audio_channels_ == 1) {
    vec_audio_data_ = get_audio_data(b_compressed, vec_audio1);
  } else {
    int4_adpcm_index = 0;
    auto vec_left_data = get_audio_data(b_compressed, vec_audio1);
    int4_adpcm_index = 0;
    auto vec_right_data = get_audio_data(b_compressed, vec_audio2);

    vec_audio_data_.assign(vec_right_data.size() + vec_left_data.size(), std::byte{0});
    auto st_right = std::size_t{0};
    auto st_left = std::size_t{0};
    for (std::size_t st_i = 0; st_i < vec_audio_data_.size();) {
      vec_audio_data_[st_i++] = At(vec_left_data, static_cast<std::int64_t>(st_left++));
      vec_audio_data_[st_i++] = At(vec_left_data, static_cast<std::int64_t>(st_left++));
      vec_audio_data_[st_i++] = At(vec_right_data, static_cast<std::int64_t>(st_right++));
      vec_audio_data_[st_i++] = At(vec_right_data, static_cast<std::int64_t>(st_right++));
    }
  }

  b_has_audio_ = !vec_audio_data_.empty();
}

void VqaVideo::AdvanceFrame() {
  int4_current_frame_index_++;
  LoadFrame();
}

void VqaVideo::LoadFrame() {
  if (int4_current_frame_index_ >= uint2_frame_count_)
    return;

  // Seek to the start of the frame
  auto reader = SpanReader{vec_file_};
  reader.Seek(vec_offsets_[static_cast<std::size_t>(int4_current_frame_index_)]);
  const auto int8_end = int4_current_frame_index_ < static_cast<std::int32_t>(vec_offsets_.size()) - 1
                            ? static_cast<std::int64_t>(vec_offsets_[static_cast<std::size_t>(
                                      int4_current_frame_index_ + 1)])
                            : reader.Length();

  while (reader.Position() < int8_end) {
    auto vec_type = reader.ReadBytes(4);
    std::uint32_t uint4_length;
    if (TagIs(vec_type, "SN2J")) {
      const auto int4_jmp = static_cast<std::int32_t>(std::byteswap(reader.ReadUInt32()));
      reader.Skip(int4_jmp);
      vec_type = reader.ReadBytes(4);
      if (TagIs(vec_type, "SND2")) {
        uint4_length = std::byteswap(reader.ReadUInt32());
        reader.Skip(static_cast<std::int64_t>(uint4_length));
        vec_type = reader.ReadBytes(4);
      } else
        ThrowNotSupported();
    }

    uint4_length = std::byteswap(reader.ReadUInt32());

    if (TagIs(vec_type, "VQFR"))
      DecodeVQFR(reader);
    else if (TagIs(vec_type, std::string_view{"\0VQF", 4})) {
      reader.ReadUInt8();
      DecodeVQFR(reader);
    } else if (TagIs(vec_type, "VQFL"))
      DecodeVQFR(reader, "VQFL");
    else
      // Don't parse sound here.
      reader.Skip(static_cast<std::int64_t>(uint4_length));

    // Chunks are aligned on even bytes; advance by a byte if the next one is null
    if (reader.Peek() == 0)
      reader.ReadUInt8();
  }

  // Now that the frame data has been loaded (in the relevant private fields), decode it into CurrentFrameData.
  DecodeFrameData();
}

// VQA Frame
void VqaVideo::DecodeVQFR(SpanReader& reader, std::string_view str_parent_type) {
  // The CBP chunks each contain 1/8th of the full lookup table
  // Annoyingly, the complete table is not applied until the frame
  // *after* the one that contains the 8th chunk.
  // Do we have a set of partial lookup tables ready to apply?
  if (int4_current_chunk_buffer_ == uint1_chunk_buffer_parts_ && uint1_chunk_buffer_parts_ != 0) {
    if (!b_cbp_is_compressed_) {
      vec_cbf_heap_ = vec_cbp_;
      vec_cbf_ = vec_cbf_heap_;
    } else
      lcw::DecodeInto(vec_cbp_, vec_cbf_heap_);

    int4_chunk_buffer_offset_ = int4_current_chunk_buffer_ = 0;
  }

  while (true) {
    // Chunks are aligned on even bytes; may be padded with a single null
    if (reader.Peek() == 0)
      reader.ReadUInt8();
    auto vec_type = reader.ReadBytes(4);
    const auto int4_subchunk_length = static_cast<std::int32_t>(std::byteswap(reader.ReadUInt32()));

    if (TagIs(vec_type, "CBFZ")) {
      // Full frame-modifier
      const auto b_decode_mode = reader.Peek() == 0;
      reader.ReadInto(vec_file_buffer_, 0, static_cast<std::size_t>(int4_subchunk_length));
      std::ranges::fill(vec_cbf_, std::byte{0});
      std::ranges::fill(vec_cbf_buffer_, std::byte{0});
      const auto int4_decode_count =
          lcw::DecodeInto(vec_file_buffer_, vec_cbf_buffer_, b_decode_mode ? 1 : 0, b_decode_mode);
      if ((uint4_video_flags_ & 0x10) == 16) {
        auto st_p = std::size_t{0};
        for (auto int4_i = 0; int4_i < int4_decode_count; int4_i += 2) {
          const auto uint2_packed = static_cast<std::uint16_t>(
              (static_cast<std::uint16_t>(static_cast<std::uint8_t>(At(vec_cbf_buffer_, int4_i + 1))) << 8) |
              static_cast<std::uint16_t>(static_cast<std::uint8_t>(At(vec_cbf_buffer_, int4_i))));
          /* 15      bit      0
             0rrrrrgg gggbbbbb
             HI byte  LO byte*/
          vec_cbf_heap_[st_p++] = static_cast<std::byte>((uint2_packed & 0x7C00) >> 7);
          vec_cbf_heap_[st_p++] = static_cast<std::byte>((uint2_packed & 0x3E0) >> 2);
          vec_cbf_heap_[st_p++] = static_cast<std::byte>((uint2_packed & 0x1f) << 3);
        }

        vec_cbf_ = vec_cbf_heap_;
      } else
        vec_cbf_ = vec_cbf_buffer_;

      if (str_parent_type == "VQFL")
        return;
    } else if (TagIs(vec_type, "CBF0")) {
      const auto vec_read = reader.ReadBytes(int4_subchunk_length);
      vec_cbf_heap_.assign(vec_read.begin(), vec_read.end());
      vec_cbf_ = vec_cbf_heap_;
    } else if (TagIs(vec_type, "CBP0") || TagIs(vec_type, "CBPZ")) {
      // frame-modifier chunk
      if (vec_cbp_.empty()) [[unlikely]]
        // HQ 上游此处 cbp 为 null(NRE 等价抛;仅坏数据可达)。
        // cbp is null here upstream on HQ (the NRE's equivalent throw;
        // malformed data only).
        throw std::runtime_error(
            "System.NullReferenceException: Object reference not set to an instance of an object.");
      reader.ReadInto(vec_cbp_, static_cast<std::size_t>(int4_chunk_buffer_offset_),
                      static_cast<std::size_t>(int4_subchunk_length));
      int4_chunk_buffer_offset_ += int4_subchunk_length;
      int4_current_chunk_buffer_++;
      b_cbp_is_compressed_ = TagIs(vec_type, "CBPZ");
    } else if (TagIs(vec_type, "CPL0")) {
      // Palette
      for (auto int4_i = 0; int4_i < uint2_num_colors_; int4_i++) {
        const auto uint1_r = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);
        const auto uint1_g = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);
        const auto uint1_b = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);
        vec_palette_bytes_[static_cast<std::size_t>(int4_i) * 4] = static_cast<std::byte>(uint1_b);
        vec_palette_bytes_[static_cast<std::size_t>(int4_i) * 4 + 1] = static_cast<std::byte>(uint1_g);
        vec_palette_bytes_[static_cast<std::size_t>(int4_i) * 4 + 2] = static_cast<std::byte>(uint1_r);
        vec_palette_bytes_[static_cast<std::size_t>(int4_i) * 4 + 3] = std::byte{255};
      }
    } else if (TagIs(vec_type, "VPTZ")) {
      // Frame data
      lcw::DecodeInto(reader.ReadBytes(int4_subchunk_length), vec_orig_data_);

      // This is the last subchunk
      return;
    } else if (TagIs(vec_type, "VPRZ")) {
      std::ranges::fill(vec_orig_data_, std::byte{0});
      reader.ReadInto(vec_file_buffer_, 0, static_cast<std::size_t>(int4_subchunk_length));
      if (vec_file_buffer_[0] != std::byte{0})
        int4_vtpr_size_ = lcw::DecodeInto(vec_file_buffer_, vec_orig_data_);
      else
        lcw::DecodeInto(vec_file_buffer_, vec_orig_data_, 1, true);
      return;
    } else if (TagIs(vec_type, "VPTR")) {
      std::ranges::fill(vec_orig_data_, std::byte{0});
      reader.ReadInto(vec_orig_data_, 0, static_cast<std::size_t>(int4_subchunk_length));
      int4_vtpr_size_ = int4_subchunk_length;
      return;
    } else
      throw std::runtime_error(
          std::format("System.IO.InvalidDataException: Unknown sub-chunk {}", TagStr(vec_type)));
  }
}

void VqaVideo::DecodeFrameData() {
  if (IsHqVqa()) {
    /* The VP?? chunks of the video file contains an array of instructions for
     * how the blocks of the finished frame will be filled with color data blocks
     * contained in the CBF? chunks.
     */
    auto int4_p = 0;
    for (auto int4_y = 0; int4_y < blocks_.Y;) {
      for (auto int4_x = 0; int4_x < blocks_.X;) {
        if (int4_y >= blocks_.Y)
          break;

        // The first 3 bits of the short determine the type of instruction with the rest being one or two parameters.
        auto int4_val = static_cast<std::int32_t>(At(vec_orig_data_, int4_p++));
        int4_val |= static_cast<std::int32_t>(At(vec_orig_data_, int4_p++)) << 8;
        const auto int4_para_a = int4_val & 0x1fff;
        const auto int4_para_b1 = int4_val & 0xFF;
        const auto int4_para_b2 = (((int4_val / 256) & 0x1f) + 1) * 2;
        switch (int4_val >> 13) {
          case 0:
            int4_x += int4_para_a;
            break;
          case 1:
            WriteBlock(int4_para_b1, int4_para_b2, int4_x, int4_y);
            break;
          case 2:
            WriteBlock(int4_para_b1, 1, int4_x, int4_y);
            for (auto int4_i = 0; int4_i < int4_para_b2; int4_i++)
              WriteBlock(static_cast<std::uint8_t>(At(vec_orig_data_, int4_p++)), 1, int4_x, int4_y);
            break;
          case 3:
            WriteBlock(int4_para_a, 1, int4_x, int4_y);
            break;
          case 5:
            WriteBlock(int4_para_a, static_cast<std::uint8_t>(At(vec_orig_data_, int4_p++)), int4_x, int4_y);
            break;
          default:
            ThrowNotSupported();
        }
      }

      int4_y++;
    }

    if (int4_p != int4_vtpr_size_)
      throw std::runtime_error("System.IndexOutOfRangeException: Index was outside the bounds of the array.");
  } else {
    for (auto int4_y = 0; int4_y < blocks_.Y; int4_y++) {
      for (auto int4_x = 0; int4_x < blocks_.X; int4_x++) {
        const auto uint1_px = static_cast<std::uint8_t>(vec_orig_data_[int4_x + int4_y * blocks_.X]);
        const auto uint1_mod = static_cast<std::uint8_t>(vec_orig_data_[int4_x + (int4_y + blocks_.Y) * blocks_.X]);
        for (auto int4_j = 0; int4_j < uint2_block_height_; int4_j++) {
          for (auto int4_i = 0; int4_i < uint2_block_width_; int4_i++) {
            const auto int4_cbfi = (uint1_mod * 256 + uint1_px) * 8 + int4_j * uint2_block_width_ + int4_i;
            const auto uint4_color_index = uint1_mod == 0x0f ? uint1_px
                                                             : static_cast<std::uint8_t>(At(vec_cbf_, int4_cbfi));

            const auto int4_pixel_x = int4_x * uint2_block_width_ + int4_i;
            const auto int4_pixel_y = int4_y * uint2_block_height_ + int4_j;
            const auto int4_pos = int4_pixel_y * uint2_total_frame_width_ + int4_pixel_x;
            vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4] =
                At(vec_palette_bytes_, static_cast<std::int64_t>(uint4_color_index) * 4);
            vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 1] =
                At(vec_palette_bytes_, static_cast<std::int64_t>(uint4_color_index) * 4 + 1);
            vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 2] =
                At(vec_palette_bytes_, static_cast<std::int64_t>(uint4_color_index) * 4 + 2);
            vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 3] =
                At(vec_palette_bytes_, static_cast<std::int64_t>(uint4_color_index) * 4 + 3);
          }
        }
      }
    }
  }
}

void VqaVideo::WriteBlock(int int4_block_number, int int4_count, int& int4_x, int& int4_y) {
  for (auto int4_i = 0; int4_i < int4_count; int4_i++) {
    const auto int4_offset = int4_block_number * uint2_block_height_ * uint2_block_width_ * 3;
    for (auto int4_by = 0; int4_by < uint2_block_height_; int4_by++)
      for (auto int4_bx = 0; int4_bx < uint2_block_width_; int4_bx++) {
        const auto int4_p = (int4_bx + int4_by * uint2_block_width_) * 3;

        const auto int4_pixel_x = int4_x * uint2_block_width_ + int4_bx;
        const auto int4_pixel_y = int4_y * uint2_block_height_ + int4_by;
        const auto int4_pos = int4_pixel_y * uint2_total_frame_width_ + int4_pixel_x;
        vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4] =
            At(vec_cbf_, static_cast<std::int64_t>(int4_offset + int4_p + 2));
        vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 1] =
            At(vec_cbf_, static_cast<std::int64_t>(int4_offset + int4_p + 1));
        vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 2] =
            At(vec_cbf_, static_cast<std::int64_t>(int4_offset + int4_p));
        vec_current_frame_data_[static_cast<std::size_t>(int4_pos) * 4 + 3] = std::byte{255};
      }

    int4_x++;
    if (int4_x >= blocks_.X) {
      int4_x = 0;
      int4_y++;
      if (int4_y >= blocks_.Y && int4_i != int4_count - 1)
        throw std::runtime_error("System.IndexOutOfRangeException: Index was outside the bounds of the array.");
    }
  }
}

}  // namespace ora::fmt
