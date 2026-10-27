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

struct ProximityCaptorInfoData {
  core::BitSet<sim::CaptureType> bitset_types;

  static ProximityCaptorInfoData Parse(const meta::RecordObject& rec_info);
};

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

struct ProximityCapturableBaseInfoData {
  core::BitSet<sim::CaptureType> bitset_captor_types;
  bool b_must_be_clear = false;
  bool b_sticky = false;
  bool b_permanent = false;
  bool b_draw_decoration = true;

  static ProximityCapturableBaseInfoData Parse(
      const meta::RecordObject& rec_info);

  static core::BitSet<sim::CaptureType> DefaultCaptorTypes();
};

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

  static void SetRunAfterTickProvider(
      std::function<void(std::function<void()>)> fn_provider);

 private:

  int CreateTrigger(Actor& self);
  void RemoveTrigger(Actor& self, int trigger);
  void TickInner(Actor& self);

  void ActorEntered(Actor& other);
  void ActorLeft(Actor& other);
  bool CanBeCapturedBy(Actor& a) const;
  void UpdateOwnership();
  void ChangeOwnership(Actor& captor);

  ProximityCapturableBaseInfoData info_;
  WDist dist_range_;
  Actor* ptr_self_ = nullptr;
  sim::Player* ptr_original_owner_ = nullptr;
  std::vector<Actor*> vec_actors_in_range_;
  int int4_trigger_ = 0;
  WPos wpos_prev_position_{};
  bool b_skip_trigger_update_ = false;
};

}
