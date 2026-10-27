#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_frame.hpp"
#include "gfx/texture.hpp"

namespace ora::gfx {

class Sheet {
 public:

  Sheet(SheetType kind_type, int2 int2_size, RenderThread* ptr_render = nullptr);

  Sheet(SheetType kind_type, Texture&& texture_owned);

  Sheet(SheetType kind_type, Texture& texture_external);

  Sheet(SheetType kind_type, std::span<const std::byte> vec_png_bytes,
        RenderThread* ptr_render = nullptr);

  Sheet(const Sheet&) = delete;
  Sheet& operator=(const Sheet&) = delete;
  Sheet(Sheet&&) = delete;
  Sheet& operator=(Sheet&&) = delete;

  std::span<std::byte> GetData();

  bool Buffered() const { return opt_data_.has_value() || (!opt_texture_.has_value() && ptr_texture_external_ == nullptr); }

  Texture& GetTexture();

  void CreateBuffer();

  void CommitBufferedData(Rectangle rect_region);

  void CommitBufferedData() { CommitBufferedData(Rectangle{0, 0, int2_size_.X, int2_size_.Y}); }

  void ReleaseBuffer();

  bool ReleaseBufferAndTryTransferTo(Sheet& sheet_destination);

  int2 Size() const { return int2_size_; }
  SheetType Type() const { return kind_type_; }

  const std::optional<Rectangle>& DirtyRegion() const { return rect_dirty_region_; }

 private:
  bool b_dirty_ = false;
  std::optional<Rectangle> rect_dirty_region_;
  bool b_release_buffer_on_commit_ = false;
  std::optional<Texture> opt_texture_;
  Texture* ptr_texture_external_ = nullptr;
  std::optional<std::vector<std::byte>> opt_data_;
  RenderThread* ptr_render_ = nullptr;
  int2 int2_size_{};
  SheetType kind_type_ = SheetType::Indexed;
};

class SheetOverflowException : public std::runtime_error {
 public:
  explicit SheetOverflowException(const std::string& str_message) : std::runtime_error(str_message) {}
};

class SheetBuilder {
 public:

  using AllocateSheetFn = std::function<std::unique_ptr<Sheet>()>;

  SheetBuilder(SheetType kind_type, std::int32_t int4_sheet_size, std::int32_t int4_margin = 1,
               RenderThread* ptr_render = nullptr);

  SheetBuilder(SheetType kind_type, AllocateSheetFn fn_allocate_sheet, std::int32_t int4_margin = 1)
      : kind_current_channel_{ChannelOf(kind_type)},
        kind_type_{kind_type},
        fn_allocate_sheet_{std::move(fn_allocate_sheet)},
        int4_margin_{int4_margin} {}

  static std::unique_ptr<Sheet> AllocateSheet(SheetType kind_type, std::int32_t int4_sheet_size,
                                              RenderThread* ptr_render = nullptr);

  static SheetType FrameTypeToSheetType(SpriteFrameType kind_frame_type);

  Sprite Add(const ISpriteFrame& frame, bool b_premultiplied = false) {
    return Add(frame.Data(), frame.Type(), frame.Size(), 0.0f,
               core::Vector3{frame.Offset().X, frame.Offset().Y, 0.0f}, b_premultiplied);
  }

  Sprite Add(std::span<const std::byte> vec_src, SpriteFrameType kind_frame_type, int2 int2_size,
             bool b_premultiplied = false) {
    return Add(vec_src, kind_frame_type, int2_size, 0.0f, core::Vector3{}, b_premultiplied);
  }

  Sprite Add(std::span<const std::byte> vec_src, SpriteFrameType kind_frame_type, int2 int2_size, float float_z_ramp,
             core::Vector3 v_sprite_offset, bool b_premultiplied = false);

  Sprite Allocate(int2 int2_image_size, float float_scale = 1.0f) {
    return Allocate(int2_image_size, 0.0f, core::Vector3{}, float_scale);
  }

  Sprite Allocate(int2 int2_image_size, float float_z_ramp, core::Vector3 v_sprite_offset, float float_scale = 1.0f);

  Sheet* Current() const { return ptr_current_; }
  TextureChannel CurrentChannel() const { return kind_current_channel_; }

  const std::vector<std::unique_ptr<Sheet>>& AllSheets() const { return vec_sheets_; }

 private:
  static TextureChannel ChannelOf(SheetType kind_type) {
    return kind_type == SheetType::Indexed ? TextureChannel::Red : TextureChannel::RGBA;
  }

  std::optional<TextureChannel> NextChannel(TextureChannel kind_channel) const;

  TextureChannel kind_current_channel_ = TextureChannel::Red;
  SheetType kind_type_ = SheetType::Indexed;
  std::vector<std::unique_ptr<Sheet>> vec_sheets_;
  AllocateSheetFn fn_allocate_sheet_;
  std::int32_t int4_margin_ = 1;
  std::int32_t int4_row_height_ = 0;
  int2 int2_p_{};
  Sheet* ptr_current_ = nullptr;
};

inline Sprite::Sprite(Sheet& sheet, Rectangle bounds, TextureChannel channel, float scale)
    : Sprite(sheet, bounds, 0.0f, core::Vector3{}, channel, BlendMode::Alpha, scale) {}

inline Sprite::Sprite(Sheet& sheet, Rectangle bounds, float z_ramp, core::Vector3 offset, TextureChannel channel,
                      BlendMode blend_mode, float scale)
    : Bounds{bounds},
      ptr_sheet{&sheet},
      kind_blend{blend_mode},
      kind_channel{channel},
      float_z_ramp{z_ramp},
      vec_size{scale * static_cast<float>(bounds.Width), scale * static_cast<float>(bounds.Height),
               scale * bounds.Height * z_ramp},
      vec_offset{offset} {

  constexpr float kInset = 1.0f / 128.0f;
  const int2 int2_size_sheet = ptr_sheet->Size();
  const auto float_left_bound = static_cast<float>(std::min(Bounds.Left(), Bounds.Right()));
  const auto float_top_bound = static_cast<float>(std::min(Bounds.Top(), Bounds.Bottom()));
  const auto float_right_bound = static_cast<float>(std::max(Bounds.Left(), Bounds.Right()));
  const auto float_bottom_bound = static_cast<float>(std::max(Bounds.Top(), Bounds.Bottom()));
  float_left = (float_left_bound + kInset) / int2_size_sheet.X;
  float_top = (float_top_bound + kInset) / int2_size_sheet.Y;
  float_right = (float_right_bound - kInset) / int2_size_sheet.X;
  float_bottom = (float_bottom_bound - kInset) / int2_size_sheet.Y;
}

inline SpriteWithSecondaryData::SpriteWithSecondaryData(const Sprite& sprite, Sheet& secondary_sheet,
                                                        Rectangle secondary_bounds, TextureChannel secondary_channel)
    : Sprite{sprite} {

  ptr_secondary_sheet = &secondary_sheet;
  SecondaryBounds = secondary_bounds;
  kind_secondary_channel = secondary_channel;
  b_secondary = true;
  const int2 int2_size_sheet = ptr_sheet->Size();
  float_secondary_left =
      static_cast<float>(std::min(SecondaryBounds.Left(), SecondaryBounds.Right())) / int2_size_sheet.X;
  float_secondary_top =
      static_cast<float>(std::min(SecondaryBounds.Top(), SecondaryBounds.Bottom())) / int2_size_sheet.Y;
  float_secondary_right =
      static_cast<float>(std::max(SecondaryBounds.Left(), SecondaryBounds.Right())) / int2_size_sheet.X;
  float_secondary_bottom =
      static_cast<float>(std::max(SecondaryBounds.Top(), SecondaryBounds.Bottom())) / int2_size_sheet.Y;
}

}
