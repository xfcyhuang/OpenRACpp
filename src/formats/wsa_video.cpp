#include "formats/wsa_video.hpp"

#include "formats/lcw.hpp"
#include "formats/xor_delta.hpp"
#include "gfx/gfx_util.hpp"

namespace ora::fmt {

bool WsaLoader::TryParseVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding,
                              std::unique_ptr<IVideo>& out_video) const {
  out_video = nullptr;
  if (vec_file.empty())
    return false;
  if (!IsWsa(vec_file))
    return false;

  out_video = std::make_unique<WsaVideo>(vec_file, b_use_frame_padding);
  return true;
}

bool IsWsa(std::span<const std::byte> vec_file) {
  auto reader = SpanReader{vec_file};

  const auto uint2_frames = reader.ReadUInt16();
  if (uint2_frames <= 1)
    return false;

  reader.ReadUInt16();
  reader.ReadUInt16();
  const auto uint2_width = reader.ReadUInt16();
  const auto uint2_height = reader.ReadUInt16();

  if (static_cast<int>(uint2_width) <= 0 || static_cast<int>(uint2_height) <= 0)
    return false;

  reader.ReadUInt16();

  const auto uint2_flags = reader.ReadUInt16();

  auto vec_offsets = std::vector<std::uint32_t>(static_cast<std::size_t>(uint2_frames) + 2);
  for (auto& uint4_offset : vec_offsets)
    uint4_offset = reader.ReadUInt32();

  if (uint2_flags == 1) {
    reader.ReadBytes(768);
    for (auto& uint4_offset : vec_offsets)
      uint4_offset += 768;
  }

  return reader.Length() == vec_offsets.back();
}

WsaVideo::WsaVideo(std::span<const std::byte> vec_file, bool b_use_frame_padding) : vec_file_(vec_file) {
  auto reader = SpanReader{vec_file_};

  uint2_frame_count_ = reader.ReadUInt16();

  reader.ReadUInt16();
  reader.ReadUInt16();

  uint2_width_ = reader.ReadUInt16();
  uint2_height_ = reader.ReadUInt16();

  reader.ReadUInt16();
  const auto uint2_flags = reader.ReadUInt16();

  vec_frame_offsets_.resize(static_cast<std::size_t>(uint2_frame_count_) + 2);
  for (auto& uint4_offset : vec_frame_offsets_)
    uint4_offset = reader.ReadUInt32();

  if (uint2_flags == 1) {
    vec_palette_bytes_.assign(1024, std::byte{0});
    for (std::size_t st_i = 0; st_i < vec_palette_bytes_.size();) {
      auto uint1_r = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);
      auto uint1_g = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);
      auto uint1_b = static_cast<std::uint8_t>(reader.ReadUInt8() << 2);

      uint1_r |= static_cast<std::uint8_t>(uint1_r >> 6);
      uint1_g |= static_cast<std::uint8_t>(uint1_g >> 6);
      uint1_b |= static_cast<std::uint8_t>(uint1_b >> 6);

      vec_palette_bytes_[st_i++] = static_cast<std::byte>(uint1_b);
      vec_palette_bytes_[st_i++] = static_cast<std::byte>(uint1_g);
      vec_palette_bytes_[st_i++] = static_cast<std::byte>(uint1_r);
      vec_palette_bytes_[st_i++] = std::byte{255};
    }

    for (auto& uint4_offset : vec_frame_offsets_)
      uint4_offset += 768;
  }

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

void WsaVideo::Reset() {
  int4_current_frame_index_ = 0;
  b_has_previous_ = false;
  vec_previous_indices_.clear();
  LoadFrame();
}

void WsaVideo::AdvanceFrame() {
  vec_previous_indices_ = vec_current_indices_;
  b_has_previous_ = true;
  int4_current_frame_index_++;
  LoadFrame();
}

void WsaVideo::LoadFrame() {
  if (int4_current_frame_index_ >= uint2_frame_count_)
    return;

  auto reader = SpanReader{vec_file_};
  reader.Seek(vec_frame_offsets_[static_cast<std::size_t>(int4_current_frame_index_)]);

  const auto uint4_data_length =
      vec_frame_offsets_[static_cast<std::size_t>(int4_current_frame_index_) + 1] -
      vec_frame_offsets_[static_cast<std::size_t>(int4_current_frame_index_)];

  const auto vec_raw_data = reader.ReadBytes(static_cast<std::int32_t>(uint4_data_length));

  auto vec_intermediate = std::vector<std::byte>{};
  vec_intermediate.assign(static_cast<std::size_t>(uint2_width_) * uint2_height_, std::byte{0});

  lcw::DecodeInto(vec_raw_data, vec_intermediate);

  vec_current_indices_.assign(static_cast<std::size_t>(uint2_width_) * uint2_height_, std::byte{0});
  if (!b_has_previous_)
    std::ranges::fill(vec_current_indices_, std::byte{0});
  else
    vec_current_indices_.assign(vec_previous_indices_.begin(), vec_previous_indices_.end());

  xor_delta::DecodeInto(vec_intermediate, vec_current_indices_, 0);

  if (vec_palette_bytes_.empty()) [[unlikely]]

    throw std::runtime_error("System.NullReferenceException: Object reference not set to an instance of an object.");

  auto st_c = std::size_t{0};
  auto int4_position = 0;
  for (auto int4_y = 0; int4_y < uint2_height_; int4_y++) {
    for (auto int4_x = 0; int4_x < uint2_width_; int4_x++) {
      const auto uint4_color_index = static_cast<std::uint8_t>(vec_current_indices_[st_c++]);
      vec_current_frame_data_[int4_position++] = vec_palette_bytes_[uint4_color_index * 4];
      vec_current_frame_data_[int4_position++] = vec_palette_bytes_[uint4_color_index * 4 + 1];
      vec_current_frame_data_[int4_position++] = vec_palette_bytes_[uint4_color_index * 4 + 2];
      vec_current_frame_data_[int4_position++] = vec_palette_bytes_[uint4_color_index * 4 + 3];
    }

    int4_position = (int4_y + 1) * uint2_total_frame_width_ * 4;
  }
}

}
