// UPSTREAM: OpenRA.Game/Traits/Player/Shroud.cs @b6fc03f L19-514(全文逐语义
//          重写)
//          The whole of Shroud.cs L19-514 (verbatim-semantics rewrite).
//
// 机制对照 / Mechanism mapping:
//  - ShroudInfo 的 ILobbyOptions 面(lobby UI)随 Phase 6/7;字段进值袋
//    (gen 表),本文件只承载运行时语义。fogEnabled/ExploreMapEnabled 的
//    OptionOrDefault 消费面在 Created(L146-157)
//    ShroudInfo's ILobbyOptions face (the lobby UI) lands with Phase 6/7;
//    the fields ride the value bag (the gen tables) while this file carries
//    the runtime semantics alone. The OptionOrDefault consumption of
//    fogEnabled/ExploreMapEnabled sits in Created (L146-157).
//  - 事件 OnShroudChanged(Action<PPos>) → 回调 vector(FrozenActorLayer
//    订阅;+= 的多播语义保持)
//    The OnShroudChanged event (Action<PPos>) → a callback vector (the
//    FrozenActorLayer subscription; += multicasting kept).
//  - Dictionary<object, ShroudSource> → std::map<const void*, ShroudSource>
//    (键 = trait 指针;上游 object 引用相等 → 指针相等,序仅影响遍历而非
//    语义 —— sources 只做 TryAdd/Remove 单键操作)
//    Dictionary<object, ShroudSource> → std::map<const void*, ShroudSource>
//    (key = the trait pointer; upstream's reference equality → pointer
//    equality; the order affects iteration only, and sources sees single-key
//    TryAdd/Remove operations alone).
//  - Tick 的 touched 向量化搜索(span.IndexOf(true))→ span 线性扫描
//    (memchr 级;非 hot 域,行为等价)
//    Tick's vectorized touched search (span.IndexOf(true)) → a linear span
//    scan (memchr-grade; not a hot domain here, behaviour-equal).
//  - 上游 resolvedType 构造默认 0 = Shroud:ProjectedCellLayer<ShroudCellType>
//    零初始化即上游 new ProjectedCellLayer 的 default(T) 填充
//    Upstream's resolvedType defaults to 0 = Shroud on construction: the
//    zero-initialized ProjectedCellLayer<ShroudCellType> equals upstream's
//    default(T)-filled new ProjectedCellLayer.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

/// Shroud.cs L74:SourceType
enum class ShroudSourceType : std::uint8_t {
  PassiveVisibility = 0,
  Shroud = 1,
  Visibility = 2,
};

/// Shroud.cs L82-83:CellVisibility([Flags];Visible 不是 Explored 的超集)
/// Shroud.cs L82-83: CellVisibility ([Flags]; Visible is not a superset of
/// Explored).
enum class CellVisibility : std::uint8_t {
  Hidden = 0x0,
  Explored = 0x1,
  Visible = 0x2,
};

inline bool HasCellVisibility(CellVisibility state, CellVisibility flag) {
  return (static_cast<std::uint8_t>(state) &
          static_cast<std::uint8_t>(flag)) != 0;
}

/// ShroudInfo 的解析面(L19-70 的加载字段;lobby UI 面随 Phase 6/7)
/// The parsed face of ShroudInfo (the loadable fields of L19-70; the lobby
/// UI face lands with Phase 6/7).
struct ShroudInfoData {
  bool b_fog_checkbox_enabled = true;      // L30 FogCheckboxEnabled
  bool b_explored_map_checkbox_enabled = false;  // L50
};

/// Shroud(Shroud.cs L72-513) | Shroud (Shroud.cs L72-513).
class Shroud final : public TraitBase,
                     public INotifyCreated,
                     public ITick,
                     public ISync {
 public:
  Shroud(Actor& self, const ShroudInfoData& info);

  ORA_TRAIT_INTERFACES(Shroud, OpenRA_Traits_Shroud, INotifyCreated, ITick,
                       ISync)

  /// L76 | L76.
  int RevealedCells() const { return int4_revealed_cells_; }

  /// L77-118:disabled([VerifySync] 唯一成员)+ disabledChanged
  /// L77-118: disabled ([VerifySync]'s sole member) + disabledChanged.
  bool Disabled() const { return b_disabled_; }
  void SetDisabled(bool value);

  bool FogEnabled() const { return !Disabled() && b_fog_enabled_; }  // L121
  bool ExploreMapEnabled() const { return b_explore_map_enabled_; }  // L122

  /// L124:Hash(Tick 内更新;非 [VerifySync] —— 上游同此)
  /// L124: Hash (updated inside Tick; not [VerifySync] — as upstream).
  int Hash() const { return int4_hash_; }

  /// L75:OnShroudChanged +=(多播) | L75: OnShroudChanged += (multicast).
  void AddShroudChangedCallback(std::function<void(PPos)> fn) {
    vec_on_shroud_changed_.push_back(std::move(fn));
  }

  void Created(Actor& self) override;  // L146-157(INotifyCreated)
  void Tick(Actor& self) override;     // L159-205

  /// L245-267:ProjectedCellsInRange(map, pos, minRange, maxRange,
  /// maxHeightDelta)
  static std::vector<PPos> ProjectedCellsInRange(const map::Map& map,
                                                 const WPos& pos,
                                                 const WDist& min_range,
                                                 const WDist& max_range,
                                                 int max_height_delta = -1);

  /// L269-272:ProjectedCellsInRange(map, cell, range, maxHeightDelta)
  static std::vector<PPos> ProjectedCellsInRange(const map::Map& map,
                                                 const CPos& cell,
                                                 const WDist& range,
                                                 int max_height_delta = -1);

  /// L274-305:AddSource(重复键 = "Attempting to add duplicate shroud
  /// source" 逐字)
  void AddSource(const void* key, ShroudSourceType type,
                 std::vector<PPos> vec_projected_cells);

  /// L307-334:RemoveSource(缺键静默) | L307-334: RemoveSource (a missing
  /// key is silent).
  void RemoveSource(const void* key);

  /// L336-351:ExploreProjectedCells | L336-351: ExploreProjectedCells.
  void ExploreProjectedCells(const std::vector<PPos>& vec_cells);

  /// L353-368:Explore(另一 Shroud 的探索并集;bounds 不符抛)
  void Explore(const Shroud& s);

  /// L370-382:ExploreAll | L370-382: ExploreAll.
  void ExploreAll();

  /// L384-394:ResetExploration | L384-394: ResetExploration.
  void ResetExploration();

  /// L396-424:IsExplored 族 | L396-424: the IsExplored family.
  bool IsExplored(const WPos& pos) const;
  bool IsExplored(const CPos& cell) const;
  bool IsExplored(const MPos& uv) const;
  bool IsExplored(const PPos& puv) const;

  /// L426-452:IsVisible 族 | L426-452: the IsVisible family.
  bool IsVisible(const WPos& pos) const;
  bool IsVisible(const CPos& cell) const;
  bool IsVisible(const MPos& uv) const;
  bool IsVisible(const PPos& puv) const;

  /// L454-459:Contains(explored 层域判定) | L454-459: Contains (the
  /// explored-layer domain check).
  bool Contains(const PPos& uv) const;

  /// L461-512:GetVisibility(pos / puv)
  /// L461-512: GetVisibility (pos / puv).
  CellVisibility GetVisibility(const WPos& pos) const;
  CellVisibility GetVisibility(const PPos& puv) const;

 private:
  /// L79:ShroudSource(SourceType, PPos[]) | L79: ShroudSource (SourceType,
  /// PPos[]).
  struct ShroudSource {
    ShroudSourceType type;
    std::vector<PPos> vec_projected_cells;
  };

  /// L78:ShroudCellType : byte(内部;L209/219 用序比较)
  /// L78: ShroudCellType : byte (internal; compared by order at L209/219).
  enum class ShroudCellType : std::uint8_t { Shroud = 0, Fog = 1, Visible = 2 };

  /// L207-243:UpdateCell(index, self)
  void UpdateCell(int index, Actor& self);

  ShroudInfoData info_;
  map::Map& map_;

  std::map<const void*, ShroudSource> map_sources_;  // L89

  // L92-96:各源类型的逐格计数 / L92-96: the per-cell counts by source type.
  map::ProjectedCellLayer<std::int16_t> layer_passive_visible_count_;
  map::ProjectedCellLayer<std::int16_t> layer_visible_count_;
  map::ProjectedCellLayer<std::int16_t> layer_generated_shroud_count_;
  map::ProjectedCellLayer<bool> layer_explored_;
  map::ProjectedCellLayer<bool> layer_touched_;
  bool b_any_cell_touched_ = true;  // L97

  map::ProjectedCellLayer<ShroudCellType> layer_resolved_type_;  // L100

  bool b_disabled_changed_ = false;  // L102
  bool b_disabled_ = false;          // L105([VerifySync])
  bool b_fog_enabled_ = false;       // L120
  bool b_explore_map_enabled_ = false;  // L122
  int int4_revealed_cells_ = 0;      // L76
  int int4_hash_ = 0;                // L124

  bool b_shroud_generation_enabled_ = false;  // L127
  bool b_passive_visibility_enabled_ = false;  // L128

  std::vector<std::function<void(PPos)>> vec_on_shroud_changed_;  // L75
};

}  // namespace ora::sim
