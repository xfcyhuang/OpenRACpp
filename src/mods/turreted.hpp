#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class AttackTurreted;
class BodyOrientation;

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct TurretedInfoData {
  std::string str_turret{"primary"};
  WAngle angle_turn_speed{512};
  WAngle angle_initial_facing{0};
  int int4_realign_delay = 40;
  WVec vec_offset{0, 0, 0};
  int int4_editor_turret_facing_display_order = 4;
  sim::ConditionalTraitData conditional;

  static TurretedInfoData Parse(const meta::RecordObject& rec_info);

  static std::function<WAngle()> WorldFacingFromInit(
      ActorInitializer& init, std::string_view info_instance_name,
      WAngle default_facing);

  static std::function<WAngle()> WorldFacingFromInit(
      sim::TypeDictionary& dict, std::string_view info_instance_name,
      WAngle default_facing);

  std::function<WAngle()> LocalFacingFromInit(
      ActorInitializer& init,
      std::string_view info_instance_name) const;
};

class Turreted : public TraitBase,
                 public sim::ConditionalTraitCore<Turreted>,
                 public sim::IObservesVariables,
                 public sim::INotifyCreated,
                 public sim::ITick,
                 public sim::IDeathActorInitModifier,
                 public sim::IActorPreviewInitModifier,
                 public sim::ISync {
 public:
  Turreted(ActorInitializer& init, const TurretedInfoData& info,
           std::string str_info_instance_name);

  ORA_TRAIT_INTERFACES(Turreted, OpenRA_Mods_Common_Traits_Turreted,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::IDeathActorInitModifier,
                       sim::IActorPreviewInitModifier, sim::ISync)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Turreted>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Turreted>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  int QuantizedFacings() const { return int4_quantized_facings_; }
  void SetQuantizedFacings(int value) {
    if (value == 0)
      throw std::out_of_range(
          "Expected nonzero facings for turret. (Parameter 'value')\n"
          "Actual value was 0.");
    int4_quantized_facings_ = value;
  }

  WRot WorldOrientation() const;

  WRot LocalOrientation() const { return rot_local_orientation_; }
  void SetLocalOrientation(WRot value) { rot_local_orientation_ = value; }

  WVec Offset() const { return info_.vec_offset + vec_local_offset_; }
  const std::string& Name() const { return info_.str_turret; }

  void Tick(Actor& self) override;

  bool FaceTarget(Actor& self, const sim::Target& target);

  virtual bool HasAchievedDesiredFacing() const;

  WVec Position(Actor& self) const;

  void ModifyDeathActorInit(Actor& self,
                            sim::TypeDictionary& init) override;

  void ModifyActorPreviewInit(Actor& self,
                              sim::TypeDictionary& inits) override;

  void SetLocalOffset(WVec value) { vec_local_offset_ = value; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHookImpl(Actor& self);
  void TraitDisabledHook(Actor& self) { TraitDisabledHookImpl(self); }
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  const TurretedInfoData& Info() const { return info_; }

 protected:

  WAngle DesiredLocalFacing() const;

  void MoveTurret();

 private:
  friend class sim::ConditionalTraitCore<Turreted>;

  const TurretedInfoData info_;
  std::string str_info_instance_name_;

  AttackTurreted* p_attack_ = nullptr;
  sim::IFacing* p_facing_ = nullptr;
  BodyOrientation* p_body_ = nullptr;
  int int4_quantized_facings_ = 0;

  WVec vec_desired_direction_{0, 0, 0};
  int int4_realign_tick_ = 0;
  bool b_realign_desired_ = false;
  WRot rot_local_orientation_;
  WVec vec_local_offset_{0, 0, 0};
};

}
