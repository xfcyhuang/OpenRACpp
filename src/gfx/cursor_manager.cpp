// UPSTREAM: OpenRA.Game/Graphics/CursorManager.cs @b6fc03f L20-317
//          (cursor_manager.hpp 的实现;头注的形态适配说明适用)
//          Implementation of cursor_manager.hpp; the shape-adaptation notes
//          of the hpp header apply.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "gfx/cursor_manager.hpp"
#include "gfx/renderer.hpp"

namespace ora::gfx {

CursorManager::CursorManager(Deps deps)
    : deps_{deps},
      builder_sheet_{SheetType::BGRA, deps.int4_cursor_sheet_size, 1, deps.ptr_render} {
  FrameCache cache_frames{*deps_.ptr_file_system, deps_.vec_sprite_loaders};

  for (const auto& [str_key, seq_cursor] : *deps_.vec_cursor_sequences) {
    const std::vector<std::unique_ptr<ISpriteFrame>>& vec_cursor_sprites = cache_frames[seq_cursor.Src()];
    const std::int32_t int4_length =
        seq_cursor.Length().has_value()
            ? *seq_cursor.Length()
            : static_cast<std::int32_t>(vec_cursor_sprites.size()) - seq_cursor.Start();

    if (seq_cursor.Start() > static_cast<std::int32_t>(vec_cursor_sprites.size()))
      throw std::runtime_error(std::format(
          "Cursor {}: Start is greater than the length of the sprite sequence.", str_key));

    if (int4_length > static_cast<std::int32_t>(vec_cursor_sprites.size()))
      throw std::runtime_error(std::format(
          "Cursor {}: Length is greater than the length of the sprite sequence.",
          seq_cursor.Name()));

    // Skip(Start).Take(length)
    std::vector<const ISpriteFrame*> vec_frames;
    for (std::int32_t int4_i{seq_cursor.Start()};
         int4_i < seq_cursor.Start() + int4_length &&
         int4_i < static_cast<std::int32_t>(vec_cursor_sprites.size());
         int4_i++)
      vec_frames.push_back(vec_cursor_sprites[static_cast<std::size_t>(int4_i)].get());

    const ImmutablePalette* ptr_palette = nullptr;
    if (!seq_cursor.Palette().empty()) {
      const auto it_cached = map_palette_cache_.find(seq_cursor.Palette());
      if (it_cached != map_palette_cache_.end())
        ptr_palette = it_cached->second;
      else {
        ptr_palette = deps_.fn_resolve_palette ? deps_.fn_resolve_palette(seq_cursor.Palette())
                                               : nullptr;
        map_palette_cache_.emplace(seq_cursor.Palette(), ptr_palette);
      }
    }

    Cursor cursor_c;
    cursor_c.str_name = str_key;
    cursor_c.rect_bounds = Rectangle::FromLTRB(0, 0, 1, 1);
    cursor_c.vec_sprites.resize(vec_frames.size());
    cursor_c.vec_ptr_cursors.resize(vec_frames.size());

    // 硬件光标有若干平台特异 bug/限制。逐帧加衬垫以减少边角情形:
    //  - 热点在帧界内(SDL 强制)
    //  - 同序列所有帧同尺寸(macOS 10.15 需要)
    //  - 帧尺寸为 8 的倍数(Windows 需要)
    // Hardware cursors have a number of odd platform-specific
    // bugs/limitations. Reduce the number of edge cases by padding the
    // individual frames such that:
    //  - the hotspot is inside the frame bounds (enforced by SDL)
    //  - all frames within a sequence have the same size (needed for macOS
    //    10.15)
    //  - the frame size is a multiple of 8 (needed for Windows)
    for (const ISpriteFrame* ptr_frame : vec_frames) {
      // 热点相对帧中心声明 | The hotspot is specified relative to the
      // frame's center.
      const int2 int2_hotspot =
          int2::FromVector(ptr_frame->Offset()) - seq_cursor.Hotspot() - ptr_frame->Size() / 2;

      // 索引数据解为真彩色 | Resolve indexed data to real colours.
      std::span<const std::byte> vec_data = ptr_frame->Data();
      SpriteFrameType kind_type = ptr_frame->Type();
      std::vector<std::byte> vec_converted;
      if (kind_type == SpriteFrameType::Indexed8) {
        vec_converted = ConvertIndexedToBgra(str_key, *ptr_frame, ptr_palette);
        vec_data = vec_converted;
        kind_type = SpriteFrameType::Bgra32;
      }

      cursor_c.vec_sprites[static_cast<std::size_t>(cursor_c.int4_length++)] =
          builder_sheet_.Add(vec_data, kind_type, ptr_frame->Size(), 0.0f,
                             core::Vector3{static_cast<float>(int2_hotspot.X),
                                           static_cast<float>(int2_hotspot.Y), 0.0f});

      // Bounds 相对热点 | Bounds relative to the hotspot.
      cursor_c.rect_bounds = Rectangle::Union(
          cursor_c.rect_bounds,
          Rectangle{int2_hotspot.X, int2_hotspot.Y, ptr_frame->Size().X, ptr_frame->Size().Y});
    }

    // 右下加衬至 8 倍数 | Pad the bottom-right edge to a multiple of 8.
    cursor_c.int2_padded_size =
        8 * int2{(cursor_c.rect_bounds.Width + 7) / 8, (cursor_c.rect_bounds.Height + 7) / 8};

    // 上游 cursors.Add 的重复键 ArgumentException 等价抛
    // The equivalent throw of upstream cursors.Add's duplicate-key
    // ArgumentException.
    if (std::ranges::any_of(vec_cursors_,
                            [&](const auto& kv) { return kv.first == str_key; }))
      throw std::runtime_error("An item with the same key has already been added.");
    vec_cursors_.emplace_back(str_key, std::move(cursor_c));
  }

  // Game.Renderer != null 时才建硬件光标(上游 Utility 面)
  // Hardware cursors only when Game.Renderer != null (upstream's Utility
  // face).
  if (deps_.ptr_renderer != nullptr && deps_.ptr_window != nullptr) {
    CreateOrUpdateHardwareCursors();
    Update();
  }
}

CursorManager::~CursorManager() {
  ClearHardwareCursors();
  vec_cursors_.clear();
}

void CursorManager::CreateOrUpdateHardwareCursors() {
  if (deps_.fn_hardware_cursors_disabled && deps_.fn_hardware_cursors_disabled())
    return;

  // 先释放既有光标,防原生资源泄漏(上游注释)
  // Dispose any existing cursors to avoid leaking native resources (the
  // upstream comment).
  ClearHardwareCursors();

  for (auto& [str_key, cursor_template] : vec_cursors_) {
    for (std::size_t st_i{}; st_i < cursor_template.vec_sprites.size(); st_i++) {
      cursor_template.vec_ptr_cursors[st_i].reset();

      // 计算帧在序列 Bounds 内的衬垫位置
      // Calculate the padding to position the frame within sequenceBounds.
      const Sprite& sprite_frame = cursor_template.vec_sprites[st_i];
      const int2 int2_padding_tl =
          -(cursor_template.rect_bounds.TopLeft() - int2::FromVector(core::Vector2{
                                                       sprite_frame.vec_offset.X,
                                                       sprite_frame.vec_offset.Y}));
      const int2 int2_padding_br = cursor_template.int2_padded_size -
                                   int2{sprite_frame.Bounds.Width, sprite_frame.Bounds.Height} -
                                   int2_padding_tl;

      auto ptr_hardware_cursor = CreateHardwareCursor(
          str_key, sprite_frame, int2_padding_tl, int2_padding_br,
          -cursor_template.rect_bounds.TopLeft());
      if (ptr_hardware_cursor != nullptr && ptr_hardware_cursor->Cursor() != nullptr)
        cursor_template.vec_ptr_cursors[st_i] = std::move(ptr_hardware_cursor);
      else {
        std::println("Failed to initialize hardware cursor for {}.", cursor_template.str_name);
        std::println(stderr, "Failed to initialize hardware cursor for {}.",
                     cursor_template.str_name);
      }
    }
  }

  if (builder_sheet_.Current() != nullptr)
    builder_sheet_.Current()->ReleaseBuffer();

  b_hardware_cursors_doubled_ = deps_.fn_cursor_double && deps_.fn_cursor_double();
}

void CursorManager::SetCursor(const std::string* ptr_cursor_name) {
  const Cursor* ptr_previous = ptr_cursor_;
  if ((ptr_cursor_name == nullptr && ptr_previous == nullptr) ||
      (ptr_previous != nullptr && ptr_cursor_name != nullptr &&
       *ptr_cursor_name == ptr_previous->str_name))
    return;

  ptr_cursor_ = nullptr;
  if (ptr_cursor_name != nullptr) {
    for (auto& [str_key, cursor_c] : vec_cursors_) {
      if (str_key == *ptr_cursor_name) {
        ptr_cursor_ = &cursor_c;
        break;
      }
    }
  }

  Update();
}

void CursorManager::Tick() {
  if (deps_.fn_cursor_double &&
      b_hardware_cursors_doubled_ != deps_.fn_cursor_double()) {
    CreateOrUpdateHardwareCursors();
    Update();
  }

  if (ptr_cursor_ == nullptr || ptr_cursor_->vec_ptr_cursors.size() == 1)
    return;

  if (++int4_ticks_ > 2) {
    int4_ticks_ -= 2;
    int4_frame_++;

    Update();
  }
}

void CursorManager::Update() {
  if (ptr_cursor_ != nullptr &&
      int4_frame_ >= static_cast<std::int32_t>(ptr_cursor_->vec_ptr_cursors.size()))
    int4_frame_ %= static_cast<std::int32_t>(ptr_cursor_->vec_ptr_cursors.size());

  const platform::Sdl2HardwareCursor* ptr_hardware_cursor =
      ptr_cursor_ != nullptr &&
              ptr_cursor_->vec_ptr_cursors[static_cast<std::size_t>(int4_frame_)] != nullptr
          ? ptr_cursor_->vec_ptr_cursors[static_cast<std::size_t>(int4_frame_)].get()
          : nullptr;
  // Utility 面(无窗口):上游 Game.Renderer == null 时不会走到 Update
  // (构造器门槛),SetCursor 亦不该触窗口 —— 此处静默跳过
  // The Utility face (no window): upstream never reaches Update with
  // Game.Renderer == null (the constructor gate), so SetCursor must not
  // touch a window either — skipped silently here.
  if (deps_.ptr_window == nullptr)
    return;
  if (ptr_hardware_cursor == nullptr || b_is_locked_)
    deps_.ptr_window->SetHardwareCursor(nullptr);
  else
    deps_.ptr_window->SetHardwareCursor(ptr_hardware_cursor);
}

void CursorManager::Render(Renderer& renderer_host) {
  // 光标隐藏 | The cursor is hidden.
  if (ptr_cursor_ == nullptr)
    return;

  // 硬件光标启用 | The hardware cursor is live.
  if (!b_is_locked_ &&
      ptr_cursor_->vec_ptr_cursors[static_cast<std::size_t>(
                                       int4_frame_ % ptr_cursor_->int4_length)] != nullptr)
    return;

  // 软件绘制光标 | Render the cursor in software.
  const bool b_double_cursor = deps_.fn_cursor_double && deps_.fn_cursor_double();
  const Sprite& sprite_cursor =
      ptr_cursor_->vec_sprites[static_cast<std::size_t>(int4_frame_ % ptr_cursor_->int4_length)];
  float fp4_cursor_scale = b_double_cursor ? 2.0f : 1.0f;

  // 光标按窗口原生坐标绘制;应用与硬件光标相同的缩放规则
  // The cursor is rendered in native window coordinates; the same scaling
  // rules as the hardware cursor apply.
  if (deps_.ptr_window->NativeWindowScale() > 1.5f)
    fp4_cursor_scale *= 2.0f;

  const int2 int2_mouse_pos = b_is_locked_ ? int2_locked_position_
                                  : deps_.fn_last_mouse_pos
                                      ? deps_.fn_last_mouse_pos()
                                      : int2{0, 0};
  renderer_host.UIRgbaSpriteRenderer().DrawSprite(
      sprite_cursor, core::Vector3{static_cast<float>(int2_mouse_pos.X),
                                    static_cast<float>(int2_mouse_pos.Y), 0.0f},
      fp4_cursor_scale / renderer_host.EffectiveWindowScale());
}

void CursorManager::Lock() {
  int2_locked_position_ =
      deps_.fn_last_mouse_pos ? deps_.fn_last_mouse_pos() : int2{0, 0};
  deps_.ptr_window->SetRelativeMouseMode(true);
  b_is_locked_ = true;
  Update();
}

void CursorManager::Unlock() {
  deps_.ptr_window->SetRelativeMouseMode(false);
  b_is_locked_ = false;
  Update();
}

std::vector<std::byte> CursorManager::ConvertIndexedToBgra(const std::string& str_name,
                                                           const ISpriteFrame& frame_input,
                                                           const ImmutablePalette* ptr_palette) {
  if (frame_input.Type() != SpriteFrameType::Indexed8)
    throw std::runtime_error(
        "ConvertIndexedToBgra requires input frames to be indexed. (Parameter 'frame')");

  // 所有调色板必须显式引用,即使内嵌于精灵中
  // All palettes must be explicitly referenced, even if they are embedded
  // in the sprite.
  if (ptr_palette == nullptr)
    throw std::runtime_error(std::format(
        "Cursor sequence `{}` attempted to load an indexed sprite but does not define Palette",
        str_name));

  const std::int32_t int4_width = frame_input.Size().X;
  const std::int32_t int4_height = frame_input.Size().Y;

  if (int4_width == 0 || int4_height == 0)
    return {};

  std::vector<std::byte> vec_data(4 * static_cast<std::size_t>(int4_width) *
                                  static_cast<std::size_t>(int4_height));
  const auto store_u32 = [&vec_data](std::size_t st_index, std::uint32_t uint4_v) {
    vec_data[st_index] = static_cast<std::byte>(uint4_v & 0xFF);
    vec_data[st_index + 1] = static_cast<std::byte>((uint4_v >> 8) & 0xFF);
    vec_data[st_index + 2] = static_cast<std::byte>((uint4_v >> 16) & 0xFF);
    vec_data[st_index + 3] = static_cast<std::byte>((uint4_v >> 24) & 0xFF);
  };

  for (std::int32_t int4_j{}; int4_j < int4_height; int4_j++)
    for (std::int32_t int4_i{}; int4_i < int4_width; int4_i++)
      store_u32(static_cast<std::size_t>(int4_j) * static_cast<std::size_t>(int4_width) +
                    static_cast<std::size_t>(int4_i),
                ptr_palette->At(static_cast<std::uint8_t>(
                    frame_input.Data()[static_cast<std::size_t>(int4_j) * int4_width + int4_i])));

  return vec_data;
}

std::unique_ptr<platform::Sdl2HardwareCursor> CursorManager::CreateHardwareCursor(
    const std::string& str_name, const Sprite& sprite_data, int2 int2_padding_tl,
    int2 int2_padding_br, int2 int2_hotspot) {
  const int2 int2_size{sprite_data.Bounds.Width, sprite_data.Bounds.Height};
  const std::int32_t int4_src_stride = sprite_data.ptr_sheet->Size().X;
  std::span<std::byte> vec_src_data = sprite_data.ptr_sheet->GetData();
  const std::int32_t int4_new_width = int2_padding_tl.X + int2_size.X + int2_padding_br.X;
  const std::int32_t int4_new_height = int2_padding_tl.Y + int2_size.Y + int2_padding_br.Y;
  std::vector<std::uint8_t> vec_rgba_data(4 * static_cast<std::size_t>(int4_new_width) *
                                          static_cast<std::size_t>(int4_new_height));

  for (std::int32_t int4_j{}; int4_j < int2_size.Y; int4_j++) {
    for (std::int32_t int4_i{}; int4_i < int2_size.X; int4_i++) {
      const std::size_t st_src =
          4 * (static_cast<std::size_t>(int4_j + sprite_data.Bounds.Top()) * int4_src_stride +
               sprite_data.Bounds.Left() + int4_i);
      const std::size_t st_dest = 4 * (static_cast<std::size_t>(int4_j + int2_padding_tl.Y) *
                                           static_cast<std::size_t>(int4_new_width) +
                                       int4_i + int2_padding_tl.X);
      for (std::size_t st_k{}; st_k < 4; st_k++)
        vec_rgba_data[st_dest + st_k] = static_cast<std::uint8_t>(vec_src_data[st_src + st_k]);
    }
  }

  std::optional<platform::Sdl2HardwareCursor> opt_cursor = deps_.ptr_window->CreateHardwareCursor(
      str_name, int4_new_width, int4_new_height, vec_rgba_data, int2_hotspot,
      deps_.fn_cursor_double && deps_.fn_cursor_double());
  if (!opt_cursor.has_value() || opt_cursor->Cursor() == nullptr)
    return nullptr;  // 上游 null 槽(建败;窗口层已记日志)| upstream's null slot (build failure; the window layer already logged)
  return std::make_unique<platform::Sdl2HardwareCursor>(std::move(*opt_cursor));
}

void CursorManager::ClearHardwareCursors() {
  for (auto& [str_key_ignored, cursor_c] : vec_cursors_) {
    for (std::unique_ptr<platform::Sdl2HardwareCursor>& ptr_cursor : cursor_c.vec_ptr_cursors)
      ptr_cursor.reset();
  }
}

}  // namespace ora::gfx
