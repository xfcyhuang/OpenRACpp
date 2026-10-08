// UPSTREAM: OpenRA.Mods.Common/Traits/ProximityCapturableBase.cs @b6fc03f
//          L21-211 全文 + ProximityCapturable.cs L18-57 全文 +
//          ProximityCaptor.cs L17-28 全文
//          The whole of ProximityCapturableBase.cs L21-211 +
//          ProximityCapturable.cs L18-57 + ProximityCaptor.cs L17-28.
//
// 机制对照 / Mechanism mapping:
//  - RulesetLoaded 的 Player-actor ProximityCaptor 前置校验:工厂时点无
//    ruleset 面 → 装配面断言由测试/装配处承载(COVERAGE 登记)
//    RulesetLoaded's Player-actor ProximityCaptor precondition: the
//    factory face has no ruleset — the assembly-side assertion rides the
//    test/assembly site (registered in COVERAGE).
//  - Game.RunAfterTick(OnOwnerChanged 的恢复)→ 注入面(缺省 = 帧末任务
//    的近似一步;恢复时点差登记)
//    Game.RunAfterTick (OnOwnerChanged's re-arm) → the injection face (the
//    default approximates with the frame-end queue; the one-step timing
//    difference is registered).
//  - FlashTarget 视觉效果与 RangeCircle 注释圈随渲染装配批(空注释面)
//    The FlashTarget visual and the RangeCircle annotation ride the
//    render-assembly batch (an empty annotation face here).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// ProximityCaptorInfo 的解析面(ProximityCaptor.cs L21-25)
/// The parsed face of ProximityCaptorInfo (ProximityCaptor.cs L21-25).
struct ProximityCaptorInfoData {
  core::BitSet<sim::CaptureType> bitset_types;  // L24 Types([Require])

  static ProximityCaptorInfoData Parse(const meta::RecordObject& rec_info);
};

/// ProximityCaptor(ProximityCaptor.cs L27;纯标记 trait)
/// ProximityCaptor (ProximityCaptor.cs L27; the pure marker trait).
class ProximityCaptor final : public TraitBase {
 public:
  explicit ProximityCaptor(const ProximityCaptorInfoData& info)
      : info_{info} {}

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ProximityCaptor;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<ProximityCaptor>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const ProximityCaptorInfoData& Info() const { return info_; }

 private:
  ProximityCaptorInfoData info_;
};

/// ProximityCapturableBaseInfo 的解析面(ProximityCapturableBase.cs L23-38)
/// The parsed face of ProximityCapturableBaseInfo (L23-38).
struct ProximityCapturableBaseInfoData {
  core::BitSet<sim::CaptureType> bitset_captor_types;  // L25 默认四标签
  bool b_must_be_clear = false;                        // L28
  bool b_sticky = false;                               // L31
  bool b_permanent = false;                            // L34
  bool b_draw_decoration = true;                       // L37

  static ProximityCapturableBaseInfoData Parse(
      const meta::RecordObject& rec_info);

  /// L25 的默认四标签(BitSet 注册表的运行时解析 —— 位分配序随注册;
  /// 上游 BitSetAllocator 同为运行时首遇序)
  /// L25's default four tags (the BitSet registry's runtime resolution —
  /// the bit allocation follows the registration's first-appearance
  /// order, same as upstream's BitSetAllocator).
  static core::BitSet<sim::CaptureType> DefaultCaptorTypes();
};

/// ProximityCapturableBase(L51-210)+ ProximityCapturable(具体圆域触发)
/// ProximityCapturableBase (L51-210) + ProximityCapturable (the concrete
/// circle-domain trigger).
class ProximityCapturable final : public TraitBase,
                                  public sim::ITick,
                                  public sim::INotifyAddedToWorld,
                                  public sim::INotifyRemovedFromWorld,
                                  public sim::INotifyOwnerChanged,
                                  public sim::IRenderAnnotations {
 public:
  ProximityCapturable(const ActorInitializer& init,
                      const ProximityCapturableBaseInfoData& info_base,
                      WDist dist_range);

  ORA_TRAIT_INTERFACES(
      ProximityCapturable, OpenRA_Mods_Common_Traits_ProximityCapturable,
      sim::ITick, sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyOwnerChanged, sim::IRenderAnnotations)

  const sim::Player* OriginalOwner() const { return ptr_original_owner_; }
  bool Captured() const {
    return ptr_self_->Owner() != ptr_original_owner_;
  }

  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void Tick(Actor& self) override;
  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

  void RenderAnnotations(Actor& self, gfx::WorldRenderer& wr,
                         std::vector<gfx::RenderItem>& vec_out) override;
  bool SpatiallyPartitionable() const override { return false; }

  /// Game.RunAfterTick 的注入面(OnOwnerChanged 的 skipTriggerUpdate 复位;
  /// 缺省 = 帧末任务近似)
  /// The Game.RunAfterTick injection face (OnOwnerChanged's skipTriggerUpdate
  /// re-arm; the default approximates with the frame-end queue).
  static void SetRunAfterTickProvider(
      std::function<void(std::function<void()>)> fn_provider);

 private:
  // ———— ProximityCapturableBase 的四抽象(L71-74;圆域具体化)————
  // ———— ProximityCapturableBase's four abstracts (L71-74; the circle
  //      concretization) ————
  int CreateTrigger(Actor& self);
  void RemoveTrigger(Actor& self, int trigger);
  void TickInner(Actor& self);

  void ActorEntered(Actor& other);
  void ActorLeft(Actor& other);
  bool CanBeCapturedBy(Actor& a) const;
  void UpdateOwnership();
  void ChangeOwnership(Actor& captor);

  ProximityCapturableBaseInfoData info_;
  WDist dist_range_;  // ProximityCapturableInfo.L21 Range
  Actor* ptr_self_ = nullptr;
  sim::Player* ptr_original_owner_ = nullptr;
  std::vector<Actor*> vec_actors_in_range_;
  int int4_trigger_ = 0;
  WPos wpos_prev_position_{};
  bool b_skip_trigger_update_ = false;
};

}  // namespace ora::mods
