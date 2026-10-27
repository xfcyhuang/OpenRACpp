#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/cursor_sequence.hpp"
#include "gfx/palette.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite_loader.hpp"
#include "platform/sdl2_hardware_cursor.hpp"

namespace ora::gfx {

class Renderer;
class WorldRenderer;

class CursorManager {
 public:

  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    std::span<const SpriteLoaderFn> vec_sprite_loaders;

    const std::vector<std::pair<std::string, CursorSequence>>* vec_cursor_sequences = nullptr;

    std::function<const ImmutablePalette*(const std::string&)> fn_resolve_palette;
    std::function<bool()> fn_hardware_cursors_disabled;
    std::function<bool()> fn_cursor_double;
    std::int32_t int4_cursor_sheet_size = 512;
    platform::Sdl2Window* ptr_window = nullptr;
    Renderer* ptr_renderer = nullptr;
    std::function<int2()> fn_last_mouse_pos;
    RenderThread* ptr_render = nullptr;
  };

  explicit CursorManager(Deps deps);
  ~CursorManager();

  CursorManager(const CursorManager&) = delete;
  CursorManager& operator=(const CursorManager&) = delete;

  void SetCursor(const std::string* ptr_cursor_name);

  void Tick();

  void Render(Renderer& renderer_host);

  void Lock();
  void Unlock();

  static std::vector<std::byte> ConvertIndexedToBgra(const std::string& str_name,
                                                     const ISpriteFrame& frame_input,
                                                     const ImmutablePalette* ptr_palette);

  std::int32_t CursorCount() const { return static_cast<std::int32_t>(vec_cursors_.size()); }
  std::int32_t FrameIndexForTest() const { return int4_frame_; }
  bool IsLockedForTest() const { return b_is_locked_; }

 private:

  struct Cursor {
    std::string str_name;
    int2 int2_padded_size{};
    Rectangle rect_bounds{};
    std::int32_t int4_length = 0;
    std::vector<Sprite> vec_sprites;

    std::vector<std::unique_ptr<platform::Sdl2HardwareCursor>> vec_ptr_cursors;
  };

  void CreateOrUpdateHardwareCursors();
  void Update();
  std::unique_ptr<platform::Sdl2HardwareCursor> CreateHardwareCursor(const std::string& str_name,
                                                                      const Sprite& sprite_data,
                                                                      int2 int2_padding_tl,
                                                                      int2 int2_padding_br,
                                                                      int2 int2_hotspot);
  void ClearHardwareCursors();

  Deps deps_;
  std::vector<std::pair<std::string, Cursor>> vec_cursors_;
  SheetBuilder builder_sheet_;
  std::map<std::string, const ImmutablePalette*> map_palette_cache_;

  Cursor* ptr_cursor_ = nullptr;
  bool b_is_locked_ = false;
  int2 int2_locked_position_{};
  bool b_hardware_cursors_doubled_ = false;
  std::int32_t int4_frame_ = 0;
  std::int32_t int4_ticks_ = 0;
};

}
