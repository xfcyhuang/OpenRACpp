// UPSTREAM: OpenRA.Game/Graphics/SequenceSet.cs @b6fc03f L19-119(全文逐语义)
// 序列集:sequences.yaml 的引擎侧容器。ISpriteSequence = 单条序列的查询面
// (mods 侧 ClassicSpriteSequence 家族实现,Phase 5 移植;本批留注入面);
// ISpriteSequenceLoader = 解析器(modData.SpriteSequenceLoader,manifest 的
// SpriteSequenceFormat 名分派 —— 同为 Phase 5 面)。
// 形态适配:
//   - 构造依赖(上游自 ModData 取 Manifest.Sequences/SpriteLoaders/
//     RendererConstants 两 sheet 尺寸)→ Deps 注入;
//   - images 的嵌套 Dictionary(插入序)→ vector&lt;pair&gt; 两级保序(GetSequence
//     的错误文本逐字;LoadSprites 的遍历序与上游一致);
//   - PerfTimer 保留为名字注释(计时面 Phase 6);
//   - ^ 前缀跳过 = ActorInfo.AbstractActorPrefix。
// The sequence set: the engine-side container of sequences.yaml.
// ISpriteSequence = the single-sequence query face (the mods-side
// ClassicSpriteSequence family implements it, ported in Phase 5; this batch
// lands the injection surface); ISpriteSequenceLoader = the parser
// (modData.SpriteSequenceLoader, dispatched by the manifest's
// SpriteSequenceFormat name — equally a Phase 5 face). Shape adaptations:
//   - the construction dependencies (upstream pulls Manifest.Sequences/
//     SpriteLoaders/the two RendererConstants sheet sizes from ModData) →
//     the Deps injection;
//   - the nested insertion-ordered Dictionary of images → a two-level
//     order-preserving vector<pair> (GetSequence's error texts verbatim;
//     LoadSprites' traversal order as upstream);
//   - the PerfTimer stays as a name comment (the timing face is Phase 6);
//   - the ^-prefix skip = ActorInfo.AbstractActorPrefix.
#pragma once
import std;

#include "core/wangle.hpp"
#include "fs/file_system.hpp"
#include "gfx/sprite_cache.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

class SpriteCache;

/// ISpriteSequence(SequenceSet.cs L19-36)。GetShadow 的上游 null = C++ 空
/// Sprite(无 sheet 指针;Animation 的 shadow 判空走 ptr_sheet == nullptr)。
/// ISpriteSequence (SequenceSet.cs L19-36). Upstream's null GetShadow = the
/// empty C++ Sprite (no sheet pointer; Animation's shadow-null test reads
/// ptr_sheet == nullptr).
struct ISpriteSequence {
  virtual std::string_view Name() const = 0;
  virtual std::int32_t Length() const = 0;
  virtual std::int32_t Facings() const = 0;
  virtual std::int32_t Tick() const = 0;
  virtual std::int32_t ZOffset() const = 0;
  virtual std::int32_t ShadowZOffset() const = 0;
  virtual Rectangle Bounds() const = 0;
  virtual bool IgnoreWorldTint() const = 0;
  virtual float Scale() const = 0;
  virtual void ResolveSprites(SpriteCache& cache_sprites) = 0;
  virtual Sprite GetSprite(std::int32_t int4_frame) = 0;
  virtual Sprite GetSprite(std::int32_t int4_frame, WAngle wangle_facing) = 0;
  virtual std::pair<Sprite, WAngle> GetSpriteWithRotation(std::int32_t int4_frame,
                                                          WAngle wangle_facing) = 0;
  virtual Sprite GetShadow(std::int32_t int4_frame, WAngle wangle_facing) = 0;
  virtual float GetAlpha(std::int32_t int4_frame) = 0;
  virtual ~ISpriteSequence() = default;
};

/// ISpriteSequenceLoader(SequenceSet.cs L38-41;modData/tileSet 经实现方构造
/// 注入 —— Phase 5 的 mods 解析器批)。
/// ISpriteSequenceLoader (SequenceSet.cs L38-41; modData/tileSet arrive via
/// the implementation's construction — the Phase 5 mods-parser batch).
struct ISpriteSequenceLoader {
  /// 返回 (序列名 → 序列) 的插入序表。
  /// Returns the insertion-ordered (sequence name → sequence) table.
  virtual std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> ParseSequences(
      SpriteCache& cache_sprites, const std::string& str_tile_set, const yaml::MiniYamlNode& node_image) = 0;
  virtual ~ISpriteSequenceLoader() = default;
};

/// SequenceSet(SequenceSet.cs L43-118)。
class SequenceSet {
 public:
  /// 构造依赖(上游经 ModData 反射链取得的等价物;additionalSequences =
  /// 地图附加序列节点,可空)。
  /// The construction dependencies (the equivalents of what upstream pulls
  /// through the ModData reflection chain; additionalSequences = the map's
  /// extra-sequences node, nullable).
  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    std::span<const SpriteLoaderFn> vec_sprite_loaders;
    const std::vector<std::string>* vec_sequence_files = nullptr;  // Manifest.Sequences
    std::int32_t int4_bgra_sheet_size = 2048;                      // rc.SequenceBgraSheetSize
    std::int32_t int4_indexed_sheet_size = 2048;                   // rc.SequenceIndexedSheetSize
    RenderThread* ptr_render = nullptr;
  };

  SequenceSet(Deps deps, ISpriteSequenceLoader& loader_sequences, std::string str_tile_set,
              const yaml::MiniYaml* yaml_additional_sequences = nullptr);

  /// GetSequence(L61-69):两级缺失的错误文本逐字。
  /// GetSequence (L61-69): the two-level missing error texts verbatim.
  /// const 方法亦可返回可变引用(unique_ptr 点位经 const 指针仍可变)。
  /// A const method may still return a mutable reference (a unique_ptr's
  /// pointee stays mutable through a const pointer).
  ISpriteSequence& GetSequence(const std::string& str_image,
                               const std::string& str_sequence) const;

  /// Images(L72)。
  std::vector<std::string> Images() const;

  /// HasSequence(L74-80):image 缺失仍抛(上游语义)。
  /// HasSequence (L74-80): a missing image still throws (upstream
  /// semantics).
  bool HasSequence(const std::string& str_image, const std::string& str_sequence) const;

  /// Sequences(L82-88)。
  std::vector<std::string> Sequences(const std::string& str_image) const;

  /// LoadSprites(L106-112):预留物化 + 逐序列 ResolveSprites。
  /// LoadSprites (L106-112): reservation materialization + per-sequence
  /// ResolveSprites.
  void LoadSprites();

  SpriteCache& CacheRef() { return *ptr_sprite_cache_; }
  const SpriteCache& CacheRef() const { return *ptr_sprite_cache_; }
  const std::string& TileSet() const { return str_tile_set_; }

 private:
  /// images 键查找(线性;null = 未定义)。
  /// The images-key lookup (linear; null = undefined).
  const std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>>* FindImagesEntry(
      const std::string& str_image) const;

  std::string str_tile_set_;
  ISpriteSequenceLoader* ptr_loader_;
  yaml::StringPool pool_yaml_;
  std::vector<yaml::MiniYamlNode> vec_nodes_;
  // image → (序列名 → 序列),两级插入序 | image → (sequence name →
  // sequence), two-level insertion order.
  std::vector<std::pair<std::string,
                        std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>>>>
      vec_images_;
  std::unique_ptr<SpriteCache> ptr_sprite_cache_;
};

}  // namespace ora::gfx
