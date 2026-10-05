// UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/PngSheetLoader.cs @b6fc03f L36-164
//          (png_sheet_loader.hpp 的实现;头注的形态适配说明适用)
//          Implementation of png_sheet_loader.hpp; the shape-adaptation
//          notes of the hpp header apply.
import std;
#include "formats/png_sheet_loader.hpp"
#include "formats/png.hpp"
#include "meta/field_loader.hpp"

namespace ora::fmt {

namespace {

/// PngSheetFrame(L39-45):可变数据袋帧。
/// PngSheetFrame (L39-45): a mutable data-bag frame.
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

/// RegionsFromFrames(L94-111):Frame[i] 手工区("x,y,w,h;offsetX,offsetY")。
/// RegionsFromFrames (L94-111): the manual Frame[i] regions ("x,y,w,h;
/// offsetX,offsetY").
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

    // 格式:x,y,width,height;offsetX,offsetY | Format: x,y,width,height;offsetX,offsetY
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

/// RegionsFromSlices(L113-162):FrameSize/FrameAmount/Offset 切片。
/// RegionsFromSlices (L113-162): the FrameSize/FrameAmount/Offset slicing.
void RegionsFromSlices(const Png& png_src, std::vector<Rectangle>& vec_regions,
                       std::vector<core::Vector2>& vec_offsets) {
  // 默认:整图一帧 | Default: the whole image as one frame.
  int2 int2_frame_size{png_src.Width(), png_src.Height()};
  std::int32_t int4_frame_amount = 1;

  const auto find_embedded = [&](std::string_view sv_key) -> const std::string* {
    const auto it_find = std::ranges::find_if(
        png_src.EmbeddedData(),
        [&](const std::pair<std::string, std::string>& kv) { return kv.first == sv_key; });
    return it_find == png_src.EmbeddedData().end() ? nullptr : &it_find->second;
  };

  if (const std::string* str_value = find_embedded("FrameSize")) {
    // 有 FrameSize 则用之,并取 FrameAmount 或按整除计算
    // With FrameSize given, take FrameAmount if present, else count how
    // many times it fits.
    int2_frame_size = meta::GetSizeValue("FrameSize", *str_value);
    if ((str_value = find_embedded("FrameAmount")) != nullptr)
      int4_frame_amount = meta::GetInt32Value("FrameAmount", *str_value);
    else
      int4_frame_amount = png_src.Width() / int2_frame_size.X *
                          (png_src.Height() / int2_frame_size.Y);
  } else if ((str_value = find_embedded("FrameAmount")) != nullptr) {
    // 否则按 FrameAmount 水平等分
    // Otherwise split horizontally by FrameAmount.
    int4_frame_amount = meta::GetInt32Value("FrameAmount", *str_value);
    int2_frame_size = int2{png_src.Width() / int4_frame_amount, png_src.Height()};
  }

  // 有 Offset 用其值,否则帧居中(上游注释)—— 值恒 Zero,语义 = 默认 Zero
  // An Offset value when present, else the frame centered (upstream
  // comment) — the value is always Zero, i.e. the Zero default.
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

}  // namespace

bool TryParsePngSheet(std::span<const std::byte> vec_file,
                      std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!Png::Verify(vec_file)) {
    vec_frames.clear();
    return false;
  }

  const Png png_src{vec_file};

  std::vector<Rectangle> vec_frame_regions;
  std::vector<core::Vector2> vec_frame_offsets;

  // 手工 Frame[i] 区优先于自动切片 | Manual Frame[i] regions beat the
  // auto-sliced ones.
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

}  // namespace ora::fmt
