// UPSTREAM: OpenRA.Game/Traits/Player/FrozenActorLayer.cs @b6fc03f L19-386
//          全文逐语义重写(FrozenActor + FrozenActorLayer)
//          The whole of FrozenActorLayer.cs L19-386 (FrozenActor +
//          FrozenActorLayer).
//
// 机制对照 / Mechanism mapping:
//  - FrozenActor 的渲染缓存面(Renderables/ScreenBounds/MouseBounds 字段、
//    Flash、Render/HasRenderables):字段保留(渲染收集批写入);本批无写
//    入者 —— FrozenUnderFog/渲染收集随其后各批(COVERAGE 登记)
//    FrozenActor's render caches (the Renderables/ScreenBounds/MouseBounds
//    fields, Flash, Render/HasRenderables): fields kept (written by the
//    render-collection batch); no writer exists yet — FrozenUnderFog and
//    the render collection ride their later batches (registered in
//    COVERAGE).
//  - FrozenActorLayer 的 SpatiallyPartitioned<FrozenActor> → 通用模板的
//    指针实例化(SpatiallyPartitioned<FrozenActor*>;FrozenActor 由
//    FrozenActorLayer 的 world arena 构造并独占)
//    FrozenActorLayer's SpatiallyPartitioned<FrozenActor> → the generic
//    template's pointer instantiation (SpatiallyPartitioned<FrozenActor*>;
//    FrozenActors are constructed inside the layer's world arena and owned
//    there).
//  - ScreenMap 的 Cache<Player,…> 惰性缓存 → unordered_map<const Player*,
//    …> 惰性构造(screen_map.hpp 的冻结面本批落地)
//    ScreenMap's Cache<Player,…> lazy caches → unordered_map<const
//    Player*, …> lazily constructed (screen_map.hpp's frozen faces land
//    this batch).
//  - ITooltip/ITooltipInfo 的运行时面:接口 + 值袋承载(TooltipInfo 字段
//    面随具体 Tooltip trait 批);FirstEnabledTraitOrDefault = 首个非禁用
//    The ITooltip/ITooltipInfo runtime faces: the interfaces + the
//    value-bag carrier (TooltipInfo's field face rides the concrete
//    Tooltip trait batch); FirstEnabledTraitOrDefault = the first
//    non-disabled.
//  - Dictionary<uint, FrozenActor> → std::map<uint32_t, FrozenActor*>(
//    Tick 的遍历序进入 VisibilityHash/FrozenHash 的加法序 —— 上游
//    Dictionary 序在插入序稳定性下等价,而两哈希均为求和(交换律),
//    遍历序差异不可观测;map 的键序为确定性锚点)
//    Dictionary<uint, FrozenActor> → std::map<uint32_t, FrozenActor*>
//    (Tick's iteration order enters the addition order of
//    VisibilityHash/FrozenHash — upstream's Dictionary order equals
//    insertion order in practice, both hashes are sums (commutative), so
//    the traversal-order difference is unobservable; the map's key order
//    is the determinism anchor).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/polygon.hpp"
#include "core/rectangle.hpp"
#include "core/wpos.hpp"
#include "map/cell_region.hpp"
#include "sim/shroud.hpp"
#include "sim/spatially_partitioned.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::gfx {
class IRenderable;  // 渲染域前置(渲染缓存字段的元素类型;Phase 6 接线)
                    // The render-domain forward (the render-cache fields'
                    // element type; wired in Phase 6).
}

namespace ora::sim {

class FrozenActor;  // ICreatesFrozenActors 参数域(本文件后段定义)
                    // ICreatesFrozenActors' parameter domain (defined later in this file).

class Actor;
class Player;
class World;

/// FrozenActorLayer.cs L21-24:ICreatesFrozenActors(FrozenUnderFog 实现)
/// FrozenActorLayer.cs L21-24: ICreatesFrozenActors (implemented by
/// FrozenUnderFog).
class ICreatesFrozenActors {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreatesFrozenActors;
  virtual ~ICreatesFrozenActors() = default;
  virtual void OnVisibilityChanged(FrozenActor& frozen) = 0;
};

/// FrozenActorLayerInfo 的解析面(L28-34)
/// The parsed face of FrozenActorLayerInfo (L28-34).
struct FrozenActorLayerInfoData {
  int int4_bin_size = 10;  // L31 BinSize
};

/// ITooltip 消费的最小值面(ITooltipInfo 的 Name 承载;具体 trait 批展开)
/// The minimal value face consumed by ITooltip (ITooltipInfo's Name
/// carrier; expanded by the concrete-trait batch).
struct TooltipInfoValue {
  std::string str_name;
};

/// TraitsInterfaces.cs 的 ITooltipInfo(ITooltip.Info 的返回面)
/// TraitsInterfaces.cs's ITooltipInfo (the return face of ITooltip.Info).
class ITooltipInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITooltipInfo;
  virtual ~ITooltipInfo() = default;
  virtual const TooltipInfoValue& TooltipInfo() const = 0;
};

/// TraitsInterfaces.cs 的 ITooltip(首个非禁用者供 RefreshState)
/// TraitsInterfaces.cs's ITooltip (the first non-disabled one feeds
/// RefreshState).
class ITooltip {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITooltip;
  virtual ~ITooltip() = default;
  virtual const ITooltipInfo* TooltipInfo() const = 0;
  virtual Player* Owner() const = 0;
};

/// FrozenActor(FrozenActorLayer.cs L36-244;World arena 内构造)
/// FrozenActor (FrozenActorLayer.cs L36-244; constructed inside the World
/// arena).
class FrozenActor final {
 public:
  FrozenActor(Actor& actor, ICreatesFrozenActors& frozen_trait,
              std::vector<PPos> vec_footprint, Player& viewer,
              bool starts_revealed);

  std::vector<PPos> Footprint;             // L38
  const WPos CenterPosition;               // L39

  std::uint32_t ID() const;                // L116
  bool IsValid() const { return p_owner_ != nullptr; }  // L117
  const game::ActorInfo& Info() const;     // L118(背衬 actor 的 Info)
  Actor* ActorPtr() const;                 // L119(!IsDead ? actor : null)

  Player* Viewer() const { return p_viewer_; }       // L87
  Player* Owner() const { return p_owner_; }         // L45
  core::BitSet<TargetableType> TargetTypes() const {  // L46
    return bitset_target_types_;
  }
  const std::vector<WPos>& TargetablePositions() const {  // L47
    return vec_targetable_positions_;
  }

  const ITooltipInfo* TooltipInfo() const {  // L50
    return p_tooltip_info_;
  }
  Player* TooltipOwner() const { return p_tooltip_owner_; }  // L51

  int HP() const { return int4_hp_; }                     // L54
  enum DamageState DamageState() const {                  // L55(类型遮蔽
                                                          // 规避,见
                                                          // AttackInfo 注)
    return damage_state_;
  }

  // The Visible flag is tied directly to the actor visibility under the
  // fog.(上游 L60-67 注释全量照抄)
  bool Visible() const { return b_visible_; }  // L68
  bool Hidden() const { return b_hidden_; }    // L69

  bool Shrouded() const { return b_shrouded_; }        // L71
  bool NeedRenderables() const { return b_need_renderables_; }  // L72
  void SetNeedRenderables(bool b) { b_need_renderables_ = b; }
  bool UpdateVisibilityNextTick() const {  // L73
    return b_update_visibility_next_tick_;
  }
  void SetUpdateVisibilityNextTick(bool b) {
    b_update_visibility_next_tick_ = b;
  }

  /// 渲染缓存字段(L74-77;渲染收集批写入)—— 指针形态(NoRenderables/
  /// NoBounds 空态 = null)
  /// The render-cache fields (L74-77; written by the render-collection
  /// batch) — pointer-shaped (the NoRenderables/NoBounds empty state =
  /// null).
  const std::vector<gfx::IRenderable*>* Renderables = nullptr;
  const std::vector<Rectangle>* ScreenBoundsList = nullptr;
  Polygon MouseBounds{Polygon::Empty()};

  void RefreshState();    // L121-140
  void RefreshHidden();   // L142-153
  void Tick();            // L155-162
  void Invalidate();      // L195-198(Owner = null)
  void Flash(core::Color color, float alpha);  // L200-206
  void Flash(core::Color tint);                // L208-214(FLASH_TINT 形态)

 private:
  /// L164-193:UpdateVisibility(footprint 上的 CellVisibility 归并)
  void UpdateVisibility();

  Actor& actor_;                        // L40
  ICreatesFrozenActors& frozen_trait_;  // L41
  Shroud& shroud_;                      // L42
  std::vector<WPos> vec_targetable_positions_;  // L43

  Player* p_viewer_ = nullptr;   // L87
  Player* p_owner_ = nullptr;    // L45(Invalidate 置 null)
  core::BitSet<TargetableType> bitset_target_types_;  // L46
  const ITooltipInfo* p_tooltip_info_ = nullptr;      // L50
  Player* p_tooltip_owner_ = nullptr;                 // L51
  std::vector<ITooltip*> vec_tooltips_;               // L52
  IHealth* p_health_ = nullptr;                       // L56
  int int4_hp_ = 0;                                   // L54
  enum DamageState damage_state_ = DamageState::Undamaged;  // L55
  std::vector<IVisibilityModifier*> vec_visibility_modifiers_;  // L58

  bool b_visible_ = true;   // L68
  bool b_hidden_ = false;   // L69
  bool b_shrouded_ = false;  // L71
  bool b_need_renderables_ = false;    // L72
  bool b_update_visibility_next_tick_ = false;  // L73

  int int4_flash_ticks_ = 0;  // L82
};

/// FrozenActorLayer(FrozenActorLayer.cs L246-385;player actor 上的 trait)
/// FrozenActorLayer (FrozenActorLayer.cs L246-385; the trait on the player
/// actor).
class FrozenActorLayer final : public TraitBase, public ITick, public ISync {
 public:
  FrozenActorLayer(Actor& self, const FrozenActorLayerInfoData& info);

  ORA_TRAIT_INTERFACES(FrozenActorLayer, OpenRA_Traits_FrozenActorLayer,
                       ITick, ISync)

  /// [VerifySync] 双成员(L248-252)
  int VisibilityHash = 0;
  int FrozenHash = 0;

  void Tick(Actor& self) override;  // L314-341

  void Add(FrozenActor* fa);     // L277-282
  void Remove(FrozenActor* fa);  // L284-289

  /// L356-362:FromID(缺 = nullptr) | L356-362: FromID (missing →
  /// nullptr).
  FrozenActor* FromID(std::uint32_t id);

  /// L364-370:FrozenActorsInRegion | L364-370: FrozenActorsInRegion.
  std::vector<FrozenActor*> FrozenActorsInRegion(
      const map::CellRegion& region, bool only_visible = true);

  /// L372-384:FrozenActorsInCircle | L372-384: FrozenActorsInCircle.
  std::vector<FrozenActor*> FrozenActorsInCircle(World& world,
                                                 const WPos& origin,
                                                 const WDist& r,
                                                 bool only_visible = true);

  /// Render 面(L343-353)随渲染批(ScreenMap 冻结面已就位;此处不引入
  /// WorldRenderer 依赖)
  /// The Render face (L343-353) rides the render batch (ScreenMap's frozen
  /// faces are in place; no WorldRenderer dependency introduced here).

 private:
  /// L291-312:FootprintBounds(静态;min/max 走 else-if 形态照抄)
  static Rectangle FootprintBounds(const FrozenActor& fa);

  int int4_bin_size_;
  World& world_;
  Player* p_owner_ = nullptr;
  std::map<std::uint32_t, FrozenActor*> map_frozen_actors_by_id_;
  SpatiallyPartitioned<FrozenActor*> partitioned_frozen_actors_;
};

}  // namespace ora::sim
