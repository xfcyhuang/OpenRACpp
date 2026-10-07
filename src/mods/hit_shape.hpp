// UPSTREAM: OpenRA.Mods.Common/Traits/HitShape.cs @b6fc03f L21-177 全文
//          (逐语义重写;RenderDebug* 渲染面随 Phase 6)
//          The whole of HitShape.cs L21-177 (verbatim-semantics rewrite;
//          the RenderDebug* faces land with Phase 6).
//
// 机制对照 / Mechanism mapping:
//  - ArmorType 类型标签(Armor.cs L18)权威注册在 gen/bitsets_gen.cpp
//    (OpenRA.Mods.Common.Traits.ArmorType);标签类定义于此
//    The ArmorType type tag (Armor.cs L18) is authoritatively registered
//    in gen/bitsets_gen.cpp; the tag class is defined here.
//  - Info.Type(IHitShape)的 LoadShape 产物 = 值袋嵌套记录 → 本批的
//    mods::ParseHitShape(Initialize() 在解析尾 —— 上游 LoadShape L157
//    同点;loaders.cpp 注记的"Phase 5 行为面"即此)
//    Info.Type's (IHitShape) LoadShape product = the value-bag nested
//    record → this batch's mods::ParseHitShape (Initialize() at the parse
//    tail — the same point as upstream's LoadShape L157; loaders.cpp's
//    "Phase 5 behaviour surface" note lands here).
//  - Turreted trait 面未移植:turret == null 的上游分支(体朝向/体原点)
//    (COVERAGE 登记,同 Armament 的 Turreted 空集面)
//    The Turreted trait face is unported: upstream's turret == null
//    branches (the body orientation/origin) (registered in COVERAGE, the
//    same empty-set face as Armament's Turreted).
//  - TargetablePositions 的缓存键(上游 (TargetableCells, CenterPosition,
//    Orientation, TurretLocalOrientation, TurretOffset) 元组)→ 无 turret
//    面下的三字段简化键(TargetableCells/CenterPosition/Orientation;
//    turret 两元恒 null)
//    TargetablePositions's cache key (upstream's (TargetableCells,
//    CenterPosition, Orientation, TurretLocalOrientation, TurretOffset)
//    tuple) → the three-field simplified key under the turret-less face
//    (TargetableCells/CenterPosition/Orientation; the two turret members
//    stay null).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/body_orientation.hpp"
#include "mods/hit_shapes.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// Armor.cs L18:ArmorType 类型标签(位分配在 gen/bitsets_gen.cpp)
/// Armor.cs L18: the ArmorType type tag (bit allocation in
/// gen/bitsets_gen.cpp).
class ArmorType {};

/// HitShapeInfo 的解析面(L25-70)
/// The parsed face of HitShapeInfo (L25-70).
struct HitShapeInfoData {
  std::string str_turret;                  // L27 Turret
  std::vector<WVec> vec_targetable_offsets{WVec{0, 0, 0}};  // L30
  bool b_use_targetable_cells_offsets = false;              // L33
  core::BitSet<ArmorType> bitset_armor_types;               // L36
  std::unique_ptr<IHitShape> ptr_type;                      // L41 Type
  sim::ConditionalTraitData conditional;

  /// 值袋(含 LoadShape 嵌套记录)→ 解析值
  /// The value bag (carrying LoadShape's nested record) → the parsed
  /// value.
  static HitShapeInfoData Parse(const meta::RecordObject& rec_info);
};

/// HitShape(L72-177)
class HitShape final : public TraitBase,
                       public sim::ConditionalTraitCore<HitShape>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::ITargetablePositions {
 public:
  HitShape(ActorInitializer& init, HitShapeInfoData&& info);  // L86-90

  ORA_TRAIT_INTERFACES(HitShape, OpenRA_Mods_Common_Traits_HitShape,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITargetablePositions)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<HitShape>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<HitShape>::IsTraitDisabled();
  }

  /// Created(L92-98):targetableCells 解析 + 核的 Created 面
  /// Created (L92-98): the targetableCells resolve + the core's Created
  /// face.
  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  /// ITargetablePositions(L100-135;缓存键见头注)
  std::vector<WPos> TargetablePositions(Actor& self) override;

  /// L151-156:DistanceFromEdge(无 turret = 体原点/体朝向)
  WDist DistanceFromEdge(Actor& self, const WPos& pos);

  const HitShapeInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<HitShape>;

  /// L137-149:CalculateTargetableOffset(turret 面空集 → 体朝向单段)
  WVec CalculateTargetableOffset(Actor& self, const WVec& offset);

  /// L124-135:无缓存的物化体
  std::vector<WPos> MaterializeTargetablePositions(Actor& self);

  HitShapeInfoData info_;
  BodyOrientation* coords_ = nullptr;  // L74 orientation
  sim::ITargetableCells* targetable_cells_ = nullptr;  // L75

  /// 缓存键 + 产物(L78-84 的简化键;见头注)
  /// The cache key + product (L78-84's simplified key; see the header).
  struct CacheKey {
    // TargetableCells 为空 = null 键面(null 与空可区分:上游 targetableCells?
    // .TargetableCells() 的 null 分支)
    bool b_has_cells = false;
    std::vector<std::pair<CPos, sim::SubCell>> vec_targetable_cells;
    bool b_has_center = false;
    WPos center{};
    bool b_has_orientation = false;
    WRot orientation{WRot::None()};

    bool operator==(const CacheKey&) const = default;
  };
  CacheKey cache_input_;
  bool b_cache_valid_ = false;
  std::vector<WPos> vec_cached_targetable_positions_;
};

}  // namespace ora::mods
