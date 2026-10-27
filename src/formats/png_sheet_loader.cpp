import std;
#include "formats/png_sheet_loader.hpp"
#include "formats/png.hpp"
#include "meta/field_loader.hpp"

namespace ora::fmt {

namespace {

class PngSheetFrame : public gfx::ISpriteFrame {
 public:
  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vec2_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return false; }

  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Rgba32;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vec2_offset_{};
  std::vector<std::byte> vec_data_;
};

void RegionsFromFrames(const Png& png_src, std::vector<Rectangle>& vec_regions,
                       std::vector<core::Vector2>& vec_offsets) {
  const Rectangle rect_png{0, 0, png_src.Width(), png_src.Height()};

  for (std::int32_t int4_i{0};; int4_i++) {
    const std::string str_key = std::format("Frame[{}]", int4_i);
    const auto it_find = std::ranges::find_if(
        png_src.EmbeddedData(),
        [&](const std::pair<std::string, std::string>& kv) { return kv.first == str_key; });
    if (it_find == png_src.EmbeddedData().end())
      break;
    const std::string& str_frame = it_find->second;

    const std::size_t st_semi = str_frame.find(';');
    if (st_semi == std::string::npos)
      throw std::runtime_error(
          "System.IndexOutOfRangeException: Index was outside the bounds of the array.");
    const Rectangle rect_region = meta::GetRectangleValue("Region", str_frame.substr(0, st_semi));
    if (!rect_png.Contains(rect_region))
      throw std::runtime_error(std::format("Invalid frame regions {},{},{},{} defined.",
                                           rect_region.X, rect_region.Y, rect_region.Width,
                                           rect_region.Height));

    vec_regions.push_back(rect_region);
    vec_offsets.push_back(meta::GetVector2Value("Offset", str_frame.substr(st_semi + 1)));
  }
}

void RegionsFromSlices(const Png& png_src, std::vector<Rectangle>& vec_regions,
                       std::vector<core::Vector2>& vec_offsets) {

  int2 int2_frame_size{png_src.Width(), png_src.Height()};
  std::int32_t int4_frame_amount = 1;

  const auto find_embedded = [&](std::string_view sv_key) -> const std::string* {
    const auto it_find = std::ranges::find_if(
        png_src.EmbeddedData(),
        [&](const std::pair<std::string, std::string>& kv) { return kv.first == sv_key; });
    return it_find == png_src.EmbeddedData().end() ? nullptr : &it_find->second;
  };

  if (const std::string* str_value = find_embedded("FrameSize")) {

    int2_frame_size = meta::GetSizeValue("FrameSize", *str_value);
    if ((str_value = find_embedded("FrameAmount")) != nullptr)
      int4_frame_amount = meta::GetInt32Value("FrameAmount", *str_value);
    else
      int4_frame_amount = png_src.Width() / int2_frame_size.X *
                          (png_src.Height() / int2_frame_size.Y);
  } else if ((str_value = find_embedded("FrameAmount")) != nullptr) {

    int4_frame_amount = meta::GetInt32Value("FrameAmount", *str_value);
    int2_frame_size = int2{png_src.Width() / int4_frame_amount, png_src.Height()};
  }

  core::Vector2 vec2_offset{};
  if (const std::string* str_value = find_embedded("Offset"))
    vec2_offset = meta::GetVector2Value("Offset", *str_value);

  const std::int32_t int4_frames_per_row = png_src.Width() / int2_frame_size.X;
  const std::int32_t int4_rows = (int4_frame_amount + int4_frames_per_row - 1) / int4_frames_per_row;
  if (png_src.Width() < int2_frame_size.X * int4_frame_amount / int4_rows ||
      png_src.Height() < int2_frame_size.Y * int4_rows)
    throw std::runtime_error(std::format("Invalid frame size {},{} and frame amount {} defined.",
                                         int2_frame_size.X, int2_frame_size.Y, int4_frame_amount));

  vec_regions.reserve(static_cast<std::size_t>(int4_frame_amount));
  for (std::int32_t int4_i{}; int4_i < int4_frame_amount; int4_i++) {
    const auto int4_x = int4_i % int4_frames_per_row * int2_frame_size.X;
    const auto int4_y = int4_i / int4_frames_per_row * int2_frame_size.Y;
    vec_regions.emplace_back(int4_x, int4_y, int2_frame_size.X, int2_frame_size.Y);
    vec_offsets.push_back(vec2_offset);
  }
}

}

bool TryParsePngSheet(std::span<const std::byte> vec_file,
                      std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!Png::Verify(vec_file)) {
    vec_frames.clear();
    return false;
  }

  const Png png_src{vec_file};

  std::vector<Rectangle> vec_frame_regions;
  std::vector<core::Vector2> vec_frame_offsets;

  const bool b_has_frame_keys = std::ranges::any_of(png_src.EmbeddedData(), [](const auto& kv) {
    return kv.first.starts_with("Frame[");
  });
  if (b_has_frame_keys)
    RegionsFromFrames(png_src, vec_frame_regions, vec_frame_offsets);
  else
    RegionsFromSlices(png_src, vec_frame_regions, vec_frame_offsets);

  vec_frames.clear();
  vec_frames.reserve(vec_frame_regions.size());
  const std::int32_t int4_stride = png_src.PixelStride();
  for (std::size_t st_i{}; st_i < vec_frame_regions.size(); st_i++) {
    const Rectangle& rect_region = vec_frame_regions[st_i];
    auto frame_next = std::make_unique<PngSheetFrame>();
    const std::int64_t int8_frame_start =
        static_cast<std::int64_t>(rect_region.X) + static_cast<std::int64_t>(rect_region.Y) * png_src.Width();
    frame_next->int2_size_ = int2{rect_region.Width, rect_region.Height};
    frame_next->int2_frame_size_ = frame_next->int2_size_;
    frame_next->vec2_offset_ = vec_frame_offsets[st_i];
    frame_next->type_frame_ = png_src.Type();
    frame_next->vec_data_.assign(static_cast<std::size_t>(rect_region.Width) * rect_region.Height * int4_stride,
                                 std::byte{0});

    for (std::int32_t int4_y{}; int4_y < frame_next->int2_size_.Y; int4_y++) {
      const std::size_t st_src = static_cast<std::size_t>((int8_frame_start + int4_y * png_src.Width())) *
                                 static_cast<std::size_t>(int4_stride);
      const std::size_t st_dst = static_cast<std::size_t>(int4_y) * frame_next->int2_size_.X *
                                 static_cast<std::size_t>(int4_stride);
      const std::size_t st_count = static_cast<std::size_t>(frame_next->int2_size_.X) *
                                   static_cast<std::size_t>(int4_stride);
      std::ranges::copy_n(png_src.Data().begin() + static_cast<std::ptrdiff_t>(st_src),
                          static_cast<std::ptrdiff_t>(st_count),
                          frame_next->vec_data_.begin() + static_cast<std::ptrdiff_t>(st_dst));
    }
    vec_frames.push_back(std::move(frame_next));
  }

  return true;
}

}
