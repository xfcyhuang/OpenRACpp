#pragma once
import std;

#include "core/cvec.hpp"
#include "meta/variable_expression.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

struct PlugInfoData {
  std::string str_type;

  static PlugInfoData Parse(const meta::RecordObject& rec_info);
};

class Plug final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Plug;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts = ora::sim::MakeTraitUpcasts<Plug>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct PluggableInfoData {
  CVec cvec_offset;
  std::vector<std::pair<std::string, std::string>> vec_conditions;
  std::vector<std::pair<std::string, expr::BooleanExpression>> vec_requirements;
  std::string str_empty_option{"Empty"};

  static PluggableInfoData Parse(const meta::RecordObject& rec_info);
};

class Pluggable final : public sim::TraitBase,
                        public sim::IObservesVariables,
                        public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(Pluggable, OpenRA_Mods_Common_Traits_Pluggable,
                       sim::IObservesVariables, sim::INotifyCreated)

  Pluggable(sim::ActorInitializer& init, PluggableInfoData info,
            std::string str_instance_name);

  void Created(sim::Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() override;

  bool AcceptsPlug(std::string_view str_type) const;

  void EnablePlug(sim::Actor& self, std::string_view str_type);

  void DisablePlug(sim::Actor& self, std::string_view str_type);

  const PluggableInfoData& Info() const { return info_; }
  const std::string& Active() const { return str_active_; }

 private:
  PluggableInfoData info_;
  std::string str_instance_name_;
  std::string str_initial_plug_;
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;
  std::map<std::string, bool> map_plug_types_availability;
  std::string str_active_;
};

class PlugInit final : public sim::ValueActorInit<std::string> {
 public:
  ORA_INIT_TYPE_SELF_ONLY(PlugInit, OpenRA_Mods_Common_Traits_PlugInit)
  explicit PlugInit(std::string value, std::string str_instance_name = "")
      : ValueActorInit<std::string>(std::move(value),
                                    std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

}
