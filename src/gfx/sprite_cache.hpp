// UPSTREAM: OpenRA.Game/Graphics/SpriteCache.cs @b6fc03f L21-183(全文逐语义)
// 精灵预留/物化两段流水:ReserveSprites 记账(token 制,同文件聚合),Load
// Reservations 按文件加载帧 → (帧索引越界检查,消息逐字含 SourceLocation
// 的 "Name:Line" 形态)→ pendingResolve 按 **帧高稳定排序**(sheet 行高打包
// 的装箱优化,上游注释照抄)→ (文件名, 帧索引, 预乘, AdjustFrame) 四元组
// 去重的 spriteCache 逐项入 SheetBuilder。ResolveSprites 以 token 取回(已
// 取/未记 → InvalidOperationException 逐字;文件缺失 → FileNotFoundException
// 文本)。LoadScreen 进度回调为 Phase 6 面(no-op)。
// 形态适配:
//   - Stream/ISpriteLoader[] → span + SpriteLoaderFn 链(sprite_loader.hpp);
//   - Dictionary&lt;SheetType, SheetBuilder&gt; → 两成员(Indexed/BGRA;上游键集
//     就是这两枚);
//   - reservationsByFilename/spriteReservations 的 Dictionary 迭代序 = 插入
//     序,以 vector&lt;pair&gt; 保序(等高稳定排序的平局结果与上游一致);
//   - AdjustFrame 委托返回的帧对象由调用方保证存活至 LoadReservations 结束
//     (上游靠 GC;消费点仅在 Add 读取 Size/Type/Data);
//   - TrimExcess 的容量收缩无 C++ 对应物(行为等价,内存面不同)。
// The two-stage sprite pipeline: ReserveSprites books (token-based, grouped
// per file), then LoadReservations loads frames per file → (the frame-index
// bounds check, message verbatim including SourceLocation's "Name:Line"
// form) → pendingResolve stably sorted by **frame height** (the sheet-packing
// optimization over the row height, the upstream comment verbatim) → the
// spriteCache deduplicated by (filename, frame index, premultiplied,
// AdjustFrame) feeding the SheetBuilder item by item. ResolveSprites hands
// the sprites back per token (already-reserved/never-reserved → the
// InvalidOperationException verbatim; a missing file → the
// FileNotFoundException text). The LoadScreen progress callback is a Phase 6
// face (no-op). Shape adaptations:
//   - Stream/ISpriteLoader[] → the span + SpriteLoaderFn chain
//     (sprite_loader.hpp);
//   - Dictionary<SheetType, SheetBuilder> → two members (Indexed/BGRA; the
//     upstream key set is exactly these two);
//   - the Dictionary iteration order of reservationsByFilename/
//     spriteReservations = insertion order, preserved as vector<pair> (the
//     height-stable sort breaks ties as upstream does);
//   - the frames returned by the AdjustFrame delegate stay alive until
//     LoadReservations ends by caller guarantee (upstream leans on the GC;
//     the consumption points only read Size/Type/Data at Add);
//   - TrimExcess has no C++ counterpart (behavior-equal, memory differs).
#pragma once
import std;

#include "fs/file_system.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

/// AdjustFrame(L21):帧修饰委托(可返回原帧或包装帧)。
/// AdjustFrame (L21): the frame-adjusting delegate (may return the input or
/// a wrapper).
using AdjustFrameFn = std::function<const ISpriteFrame*(const ISpriteFrame& frame_input,
                                                         std::int32_t int4_index,
                                                         std::int32_t int4_total)>;

/// SpriteCache(L23-183)。
class SpriteCache {
 public:
  /// 上游 (fileSystem, loaders, bgraSheetSize, indexedSheetSize[, margins])。
  /// Upstream (fileSystem, loaders, bgraSheetSize, indexedSheetSize[, margins]).
  SpriteCache(fs::FileSystem& file_system, std::span<const SpriteLoaderFn> vec_loaders,
              std::int32_t int4_bgra_sheet_size, std::int32_t int4_indexed_sheet_size,
              std::int32_t int4_bgra_margin = 1, std::int32_t int4_indexed_margin = 1,
              RenderThread* ptr_render = nullptr);

  /// ReserveSprites(L53-59):token 记账(frames 空 = 上游 null = 全帧)。
  /// ReserveSprites (L53-59): token booking (an absent frames list =
  /// upstream's null = all frames).
  std::int32_t ReserveSprites(const std::string& str_filename,
                              std::optional<std::vector<std::int32_t>> vec_frames,
                              yaml::SourceLocation location_source,
                              AdjustFrameFn fn_adjust_frame = nullptr, bool b_premultiplied = false);

  /// LoadFramesUncached(L77-80):绕过预留直读(字节随帧共持)。
  /// LoadFramesUncached (L77-80): a direct read bypassing the reservations
  /// (the bytes co-held with the frames).
  std::optional<FramesSource> LoadFramesUncached(const std::string& str_filename);

  /// LoadReservations(L82-160):全量物化(modData 参数仅驱动 LoadScreen 进度,
  /// Phase 6 no-op,故省略)。
  /// LoadReservations (L82-160): materializes everything (the modData
  /// parameter only drives the LoadScreen progress — a Phase 6 no-op — so it
  /// is dropped).
  void LoadReservations();

  /// ResolveSprites(L162-173):取走(二次取/未记抛;缺文件抛 FileNotFoundException
  /// 文本)。
  /// ResolveSprites (L162-173): takes the sprites away (a second take /
  /// never-reserved throws; a missing file throws the FileNotFoundException
  /// text).
  std::vector<Sprite> ResolveSprites(std::int32_t int4_token);

  /// MissingFiles(L175):去重后的 (文件名, 位置) 表。
  /// MissingFiles (L175): the deduplicated (filename, location) list.
  std::vector<std::pair<std::string, yaml::SourceLocation>> MissingFiles() const;

  const SheetBuilder& BuilderOf(SheetType kind_type) const;
  SheetBuilder& BuilderOf(SheetType kind_type);

 private:
  struct Reservation {
    std::optional<std::vector<std::int32_t>> vec_frames;  // nullopt = 上游 null | upstream's null
    yaml::SourceLocation location_source{};
    AdjustFrameFn fn_adjust_frame;
    bool b_premultiplied = false;
  };

  fs::FileSystem& file_system_;
  std::vector<SpriteLoaderFn> vec_loaders_;
  SheetBuilder builder_indexed_;
  SheetBuilder builder_bgra_;

  std::vector<std::pair<std::int32_t, Reservation>> vec_sprite_reservations_;  // 插入序 | insertion order
  std::vector<std::pair<std::string, std::vector<std::int32_t>>> vec_reservations_by_filename_;

  std::map<std::int32_t, std::optional<std::vector<Sprite>>> map_resolved_sprites_;
  std::map<std::int32_t, std::pair<std::string, yaml::SourceLocation>> map_missing_files_;

  std::int32_t int4_next_reservation_token_ = 1;
};

}  // namespace ora::gfx
