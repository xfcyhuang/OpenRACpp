// UPSTREAM: OpenRA.Game/Graphics/Sheet.cs @b6fc03f L19-196 +
//           OpenRA.Game/Graphics/SheetBuilder.cs @b6fc03f L34-175(实现)
// Implementations of Sheet/SheetBuilder (headers in sheet.hpp carry the
// full UPSTREAM anchors and deviation notes).
#include "gfx/sheet.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"

namespace ora::gfx {

// ———— Sheet ————

Sheet::Sheet(SheetType kind_type, int2 int2_size, RenderThread* ptr_render)
    : ptr_render_{ptr_render}, int2_size_{int2_size}, kind_type_{kind_type} {}

Sheet::Sheet(SheetType kind_type, Texture&& texture_owned)
    : opt_texture_{std::move(texture_owned)},
      int2_size_{opt_texture_->Width(), opt_texture_->Height()},
      kind_type_{kind_type} {}

Sheet::Sheet(SheetType kind_type, Texture& texture_external)
    : ptr_texture_external_{&texture_external},
      int2_size_{texture_external.Width(), texture_external.Height()},
      kind_type_{kind_type} {}

Sheet::Sheet(SheetType kind_type, std::span<const std::byte> vec_png_bytes, RenderThread* ptr_render)
    : ptr_render_{ptr_render}, kind_type_{kind_type} {
  // Sheet(Stream)(L57-66):Png 解码 → 全幅 data → FastCopyIntoSprite →
  // ReleaseBuffer。上游注释照抄(仅 ChromeProvider 的 BGRA 面使用)。
  // Sheet(Stream) (L57-66): Png decode → the full-extent data →
  // FastCopyIntoSprite → ReleaseBuffer. Upstream comment kept (only
  // ChromeProvider's BGRA face uses it).
  const fmt::Png png_loading{vec_png_bytes};
  int2_size_ = int2{png_loading.Width(), png_loading.Height()};
  opt_data_ = std::vector<std::byte>(4 * static_cast<std::size_t>(int2_size_.X) *
                                    static_cast<std::size_t>(int2_size_.Y));
  FastCopyIntoSprite(Sprite{*this, Rectangle{0, 0, int2_size_.X, int2_size_.Y}, TextureChannel::Red},
                     png_loading);

  ReleaseBuffer();
}

std::span<std::byte> Sheet::GetData() {
  CreateBuffer();
  return std::span<std::byte>{*opt_data_};
}

Texture& Sheet::GetTexture() {
  if (ptr_texture_external_ != nullptr)
    return *ptr_texture_external_;  // 非拥有形态:无惰性建/无脏上传 | the non-owning form: no lazy create, no dirty upload

  if (!opt_texture_.has_value()) {
    assert(ptr_render_ != nullptr);  // 纯数据模式无 GPU 侧 | no GPU side in the data-only mode
    opt_texture_.emplace(*ptr_render_);
    b_dirty_ = true;
  }

  if (opt_data_.has_value() && b_dirty_) {
    const auto& vec_data = *opt_data_;
    // If size of texture does not match the sheet size, the texture needs
    // to be initialized. (Sheet.cs L74-79)
    if (rect_dirty_region_.has_value() && opt_texture_->Width() == int2_size_.X &&
        opt_texture_->Height() == int2_size_.Y &&
        (rect_dirty_region_->Width != int2_size_.X || rect_dirty_region_->Height != int2_size_.Y)) {
      const auto& rect_region = *rect_dirty_region_;
      opt_texture_->SetSubData(vec_data, rect_region.X, rect_region.Y, rect_region.Width, rect_region.Height);
    } else {
      opt_texture_->SetData(vec_data, int2_size_.X, int2_size_.Y);
    }

    rect_dirty_region_.reset();
    b_dirty_ = false;
    if (b_release_buffer_on_commit_)
      opt_data_.reset();  // 上游 data = null | upstream's data = null
  }

  return *opt_texture_;
}

void Sheet::CreateBuffer() {
  if (opt_data_.has_value())
    return;

  if (opt_texture_.has_value())
    opt_data_ = opt_texture_->GetData();  // GPU 读回(Sheet.cs L130)| read back (Sheet.cs L130)
  else if (ptr_texture_external_ != nullptr)
    opt_data_ = ptr_texture_external_->GetData();
  else
    opt_data_ = std::vector<std::byte>(static_cast<std::size_t>(4) * int2_size_.X * int2_size_.Y);

  b_release_buffer_on_commit_ = false;
}

void Sheet::CommitBufferedData(Rectangle rect_region) {
  if (!Buffered())
    throw std::runtime_error(
        "This sheet is unbuffered. You cannot call CommitBufferedData on an unbuffered sheet. "
        "If you need to completely replace the texture data you should set data into the texture directly. "
        "If you need to make only small changes to the texture data consider creating a buffered sheet instead.");

  if (!rect_dirty_region_.has_value())
    rect_dirty_region_ = rect_region;
  else
    rect_dirty_region_ = Rectangle::Union(*rect_dirty_region_, rect_region);

  b_dirty_ = true;
}

void Sheet::ReleaseBuffer() {
  if (!Buffered())
    return;

  b_dirty_ = true;
  b_release_buffer_on_commit_ = true;

  // Commit data from the buffer to the texture, allowing the buffer to be
  // released and reclaimed by GC. (Sheet.cs L164-166;上游判 Game.Renderer)
  // (Sheet.cs L164-166; upstream tests Game.Renderer)
  if (ptr_render_ != nullptr)
    GetTexture();
}

bool Sheet::ReleaseBufferAndTryTransferTo(Sheet& sheet_destination) {
  if (!(int2_size_ == sheet_destination.int2_size_))
    throw std::invalid_argument("Destination sheet does not have the same size");

  // 上游先取局部 buffer 引用再 ReleaseBuffer(引用语义);上传后本方 data
  // 置空、局部引用仍活。此处记录"曾有数据"即可 —— 上游转移出去的内容在
  // Array.Clear 后同为全零(见函数头注记:重新分配 = 行为等价)。
  // Upstream grabs the local buffer reference before ReleaseBuffer
  // (reference semantics); after the commit its own data goes null while
  // the local reference stays alive. Recording "had data" suffices here —
  // the transferred content is all-zero after Array.Clear either way (see
  // the header note: re-allocation is behavior-equivalent).
  const bool b_had_data = opt_data_.has_value();

  ReleaseBuffer();

  // We aren't committing data to the GPU, so let's not delete our data.
  // (Sheet.cs L177-179;上游判 Game.Renderer == null)
  // (Sheet.cs L177-179; upstream tests Game.Renderer == null)
  if (ptr_render_ == nullptr)
    return false;

  // Only transfer if the destination has no data that would be lost by
  // overwriting. (Sheet.cs L181-189)
  if (b_had_data && !sheet_destination.opt_data_.has_value() && !sheet_destination.opt_texture_.has_value()) {
    sheet_destination.opt_data_ =
        std::vector<std::byte>(static_cast<std::size_t>(4) * int2_size_.X * int2_size_.Y, std::byte{0});
    sheet_destination.b_release_buffer_on_commit_ = false;
    return true;
  }

  return false;
}

// ———— SheetBuilder ————

std::unique_ptr<Sheet> SheetBuilder::AllocateSheet(SheetType kind_type, std::int32_t int4_sheet_size,
                                                   RenderThread* ptr_render) {
  return std::make_unique<Sheet>(kind_type, int2{int4_sheet_size, int4_sheet_size}, ptr_render);
}

SheetType SheetBuilder::FrameTypeToSheetType(SpriteFrameType kind_frame_type) {
  switch (kind_frame_type) {
    case SpriteFrameType::Indexed8:
      return SheetType::Indexed;

    // Util.FastCopyIntoChannel will automatically convert these to BGRA
    // (SheetBuilder.cs L59-64 上注释)
    case SpriteFrameType::Bgra32:
    case SpriteFrameType::Bgr24:
    case SpriteFrameType::Rgba32:
    case SpriteFrameType::Rgb24:
      return SheetType::BGRA;
    default:
      throw std::runtime_error("Unknown SpriteFrameType " + std::to_string(static_cast<int>(kind_frame_type)));
  }
}

SheetBuilder::SheetBuilder(SheetType kind_type, std::int32_t int4_sheet_size, std::int32_t int4_margin,
                           RenderThread* ptr_render)
    : kind_current_channel_{ChannelOf(kind_type)},
      kind_type_{kind_type},
      int4_margin_{int4_margin} {
  fn_allocate_sheet_ = [kind_type, int4_sheet_size, ptr_render] {
    return AllocateSheet(kind_type, int4_sheet_size, ptr_render);
  };
}

Sprite SheetBuilder::Add(std::span<const std::byte> vec_src, SpriteFrameType kind_frame_type, int2 int2_size,
                         float float_z_ramp, core::Vector3 v_sprite_offset, bool b_premultiplied) {
  if (ptr_current_ == nullptr) {
    vec_sheets_.push_back(fn_allocate_sheet_());
    ptr_current_ = vec_sheets_.back().get();
  }

  // Don't bother allocating empty sprites (SheetBuilder.cs L94-96)
  if (int2_size.X == 0 || int2_size.Y == 0)
    return Sprite(*ptr_current_, Rectangle::Empty(), 0.0f, v_sprite_offset, kind_current_channel_,
                  BlendMode::Alpha);

  auto sprite_rect = Allocate(int2_size, float_z_ramp, v_sprite_offset);
  FastCopyIntoChannel(sprite_rect, vec_src, kind_frame_type, b_premultiplied);
  ptr_current_->CommitBufferedData(sprite_rect.Bounds);
  return sprite_rect;
}

std::optional<TextureChannel> SheetBuilder::NextChannel(TextureChannel kind_channel) const {
  const auto int4_next_channel =
      static_cast<std::int32_t>(kind_channel) + static_cast<std::int32_t>(kind_type_);
  if (int4_next_channel > static_cast<std::int32_t>(TextureChannel::Alpha))
    return std::nullopt;

  return static_cast<TextureChannel>(int4_next_channel);
}

Sprite SheetBuilder::Allocate(int2 int2_image_size, float float_z_ramp, core::Vector3 v_sprite_offset,
                              float float_scale) {
  if (ptr_current_ == nullptr) {
    vec_sheets_.push_back(fn_allocate_sheet_());
    ptr_current_ = vec_sheets_.back().get();
  }

  if (int2_image_size.X + int2_p_.X + int4_margin_ > ptr_current_->Size().X) {
    int2_p_ = int2{0, int2_p_.Y + int4_row_height_ + int4_margin_};
    int4_row_height_ = int2_image_size.Y;
  }

  if (int2_image_size.Y > int4_row_height_)
    int4_row_height_ = int2_image_size.Y;

  if (int2_p_.Y + int2_image_size.Y + int4_margin_ > ptr_current_->Size().Y) {
    auto opt_next = NextChannel(kind_current_channel_);
    if (!opt_next.has_value()) {
      Sheet* ptr_previous = ptr_current_;
      auto sheet_new = fn_allocate_sheet_();
      ptr_current_ = sheet_new.get();

      // Reuse the backing buffer between sheets where possible.
      // This avoids allocating additional buffers which the GC must clean
      // up. (SheetBuilder.cs L147-149)
      ptr_previous->ReleaseBufferAndTryTransferTo(*ptr_current_);

      vec_sheets_.push_back(std::move(sheet_new));
      kind_current_channel_ = ChannelOf(kind_type_);
    } else {
      kind_current_channel_ = *opt_next;
    }

    int4_row_height_ = int2_image_size.Y;
    int2_p_ = int2{};
  }

  auto sprite_rect = Sprite(*ptr_current_,
                            Rectangle{int2_p_.X + int4_margin_, int2_p_.Y + int4_margin_, int2_image_size.X,
                                            int2_image_size.Y},
                            float_z_ramp, v_sprite_offset, kind_current_channel_, BlendMode::Alpha, float_scale);
  int2_p_ = int2_p_ + int2{int2_image_size.X + int4_margin_, 0};

  return sprite_rect;
}

}  // namespace ora::gfx
