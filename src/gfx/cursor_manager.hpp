// UPSTREAM: OpenRA.Game/Graphics/CursorManager.cs @b6fc03f L20-317(逐方法)+
//           OpenRA.Game/ModData.cs @b6fc03f L135-149(ParseCursors 的装载面)
// 光标管理:cursors.yaml 的序列 → 帧切取(Start/Length 两上界检查,YamlException
// 文本逐字)→ 热点居中换算(hotspot = FromVector(f.Offset) - Hotspot -
// size/2)→ Indexed8 经 ConvertIndexedToBgra 解色 → SheetBuilder.Add(热点为
// 偏移)→ Bounds 并集 → 8 倍数 PaddedSize(三平台怪癖的上游注释照抄)。
// 硬件光标:每帧 paddingTL/BR 对齐后 CreateHardwareCursor(软光标共享同 sheet);
// Tick 的 3 tick 步进换帧 + CursorDouble 变更重建;Lock/Unlock 的相对鼠标
// 模式 + lockedPosition。
// 形态适配:
//   - Game.Settings.Graphics → 两 live 查询注入;Game.Renderer/Window →
//     Renderer*/Sdl2Window* 注入(空 = 上游 Utility 的 Game.Renderer == null
//     路径:不建硬件光标);Viewport.LastMousePos → 查询注入(Phase 5);
//   - ModData.Cursors 表 + IProvidesCursorPaletteInfo 调色板面 → 构造注入
//     (光标序列表 + 名字 → ImmutablePalette 解析器;调色板缓存内置);
//   - IHardwareCursor[] → optional&lt;Sdl2HardwareCursor&gt; 数组(上游 null 槽
//     = 建败/禁用);
//   - 上游 Dispose 的 GC 终结路径 → 析构。
// The cursor manager: cursors.yaml sequences → frame slicing (the Start/
// Length upper-bound checks with YamlException texts verbatim) → the
// hotspot-centering conversion (hotspot = FromVector(f.Offset) - Hotspot -
// size/2) → Indexed8 resolved through ConvertIndexedToBgra →
// SheetBuilder.Add (the hotspot as the offset) → the Bounds union → the
// multiple-of-8 PaddedSize (the upstream comment on the three platform
// quirks kept verbatim). Hardware cursors: per-frame paddingTL/BR alignment
// then CreateHardwareCursor (the software cursor shares the same sheet);
// Tick's 3-tick frame stepping + the CursorDouble rebuild; Lock/Unlock's
// relative mouse mode + lockedPosition. Shape adaptations: Game.Settings.
// Graphics → two live-query injections; Game.Renderer/Window → Renderer*/
// Sdl2Window* injections (null = upstream's Game.Renderer == null Utility
// path: no hardware cursors); Viewport.LastMousePos → a query injection
// (Phase 5); the ModData.Cursors table + the IProvidesCursorPaletteInfo
// palette face → constructor injection (the cursor-sequence table + a name →
// ImmutablePalette resolver; the palette cache built in); IHardwareCursor[]
// → an optional<Sdl2HardwareCursor> array (upstream's null slots =
// build-failure/disabled); upstream's Dispose-GC-finalizer path → the
// destructor.
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

/// CursorManager(CursorManager.cs L20-317)。
class CursorManager {
 public:
  /// 构造依赖(上游从 Game/ModData 的全局与反射链取得)。
  /// The construction dependencies (what upstream pulls from the
  /// Game/ModData globals and reflection chains).
  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    std::span<const SpriteLoaderFn> vec_sprite_loaders;
    /// cursors.yaml 的解析表(名字序;ModData.ParseCursors 的注入等价)。
    /// The parsed cursors.yaml table (name order; the injected equivalent of
    /// ModData.ParseCursors).
    const std::vector<std::pair<std::string, CursorSequence>>* vec_cursor_sequences = nullptr;
    /// 名字 → 光标调色板(IProvidesCursorPaletteInfo 面;空函数 = 无调色板
    /// 定义,Indexed8 帧将在解色时按上游语义抛)。
    /// Name → cursor palette (the IProvidesCursorPaletteInfo face; an empty
    /// function = no palette definitions, and Indexed8 frames throw at
    /// conversion per upstream semantics).
    std::function<const ImmutablePalette*(const std::string&)> fn_resolve_palette;
    std::function<bool()> fn_hardware_cursors_disabled;  // GraphicSettings.DisableHardwareCursors(live)
    std::function<bool()> fn_cursor_double;              // GraphicSettings.CursorDouble(live)
    std::int32_t int4_cursor_sheet_size = 512;           // rc.CursorSheetSize
    platform::Sdl2Window* ptr_window = nullptr;          // Game.Renderer.Window(空 = Utility)
    Renderer* ptr_renderer = nullptr;                    // Game.Renderer(空 = Utility)
    std::function<int2()> fn_last_mouse_pos;             // Viewport.LastMousePos(Phase 5)
    RenderThread* ptr_render = nullptr;
  };

  explicit CursorManager(Deps deps);
  ~CursorManager();

  CursorManager(const CursorManager&) = delete;
  CursorManager& operator=(const CursorManager&) = delete;

  /// SetCursor(L157-166):空名/未知名 → 隐藏。
  /// SetCursor (L157-166): an empty/unknown name hides.
  void SetCursor(const std::string* ptr_cursor_name);

  /// Tick(L171-189):CursorDouble 变更重建 + 3 tick 换帧。
  /// Tick (L171-189): the CursorDouble rebuild + the 3-tick frame stepping.
  void Tick();

  /// Render(L203-227):软光标路径(硬件光标可用/已隐藏时 no-op)。
  /// Render (L203-227): the software-cursor path (a no-op when the hardware
  /// cursor is live or the cursor is hidden).
  void Render(Renderer& renderer_host);

  /// Lock/Unlock(L229-242):相对鼠标模式 + 锁定位。
  /// Lock/Unlock (L229-242): the relative mouse mode + the locked position.
  void Lock();
  void Unlock();

  /// ConvertIndexedToBgra(L244-273):索引帧解色(静态纯函数;上游异常文本
  /// 逐字)。
  /// ConvertIndexedToBgra (L244-273): the indexed-frame color resolve (a
  /// static pure function; upstream exception texts verbatim).
  static std::vector<std::byte> ConvertIndexedToBgra(const std::string& str_name,
                                                     const ISpriteFrame& frame_input,
                                                     const ImmutablePalette* ptr_palette);

  /// —— 测试面 —— / —— Test surface ——
  std::int32_t CursorCount() const { return static_cast<std::int32_t>(vec_cursors_.size()); }
  std::int32_t FrameIndexForTest() const { return int4_frame_; }
  bool IsLockedForTest() const { return b_is_locked_; }

 private:
  /// 内部光标表项(CursorManager.Cursor,L22-31)。
  /// The internal cursor entry (CursorManager.Cursor, L22-31).
  struct Cursor {
    std::string str_name;
    int2 int2_padded_size{};
    Rectangle rect_bounds{};
    std::int32_t int4_length = 0;  // 已入 sheet 的帧数 | frames added to the sheet
    std::vector<Sprite> vec_sprites;
    /// unique_ptr 形态 = 上游可空槽(Sdl2HardwareCursor 移动赋值删除,故不用
    /// optional)| the unique_ptr shape = upstream's nullable slots
    /// (Sdl2HardwareCursor deletes move-assignment, so no optional).
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
  std::vector<std::pair<std::string, Cursor>> vec_cursors_;  // 插入序 | insertion order
  SheetBuilder builder_sheet_;
  std::map<std::string, const ImmutablePalette*> map_palette_cache_;

  Cursor* ptr_cursor_ = nullptr;
  bool b_is_locked_ = false;
  int2 int2_locked_position_{};
  bool b_hardware_cursors_doubled_ = false;
  std::int32_t int4_frame_ = 0;
  std::int32_t int4_ticks_ = 0;
};

}  // namespace ora::gfx
