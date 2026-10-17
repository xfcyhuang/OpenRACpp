// UPSTREAM: OpenRA.Mods.Common/Traits/Plug.cs @e180e3225 L15-24 +
//          Traits/Pluggable.cs L15-160
//          Plug.cs L15-24 + Pluggable.cs L15-160.
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
  std::string str_type;  // L20([FieldLoader.Require])

  static PlugInfoData Parse(const meta::RecordObject& rec_info);
};

/// Plug(L23;空运行时类)
/// Plug (L23; the empty runtime class).
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
  CVec cvec_offset;                                     // L23
  std::vector<std::pair<std::string, std::string>> vec_conditions;  // L28
  std::vector<std::pair<std::string, expr::BooleanExpression>> vec_requirements;  // L33
  std::string str_empty_option{"Empty"};                // L42

  static PluggableInfoData Parse(const meta::RecordObject& rec_info);
};

/// Pluggable(L82-153;IEditorActorOptions 的编辑器面不入运行时 —— lint
/// 域,COVERAGE 登记)
/// Pluggable (L82-153; the IEditorActorOptions editor face stays out of
/// the runtime — lint domain, registered in COVERAGE).
class Pluggable final : public sim::TraitBase,
                        public sim::IObservesVariables,
                        public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(Pluggable, OpenRA_Mods_Common_Traits_Pluggable,
                       sim::IObservesVariables, sim::INotifyCreated)

  Pluggable(sim::ActorInitializer& init, PluggableInfoData info,
            std::string str_instance_name);

  void Created(sim::Actor& self) override;  // L106-110

  std::vector<sim::VariableObserver> GetVariableObservers() override;  // L146-152

  /// L112-121 | L112-121.
  bool AcceptsPlug(std::string_view str_type) const;

  /// L123-133 | L123-133.
  void EnablePlug(sim::Actor& self, std::string_view str_type);

  /// L135-144 | L135-144.
  void DisablePlug(sim::Actor& self, std::string_view str_type);

  const PluggableInfoData& Info() const { return info_; }
  const std::string& Active() const { return str_active_; }

 private:
  PluggableInfoData info_;
  std::string str_instance_name_;
  std::string str_initial_plug_;                    // L87
  int int4_condition_token_ = sim::Actor::InvalidConditionToken;  // L88
  std::map<std::string, bool> map_plug_types_availability;        // L88
  std::string str_active_;                          // L90
};

/// PlugInit(L155-159;info 实例名形)
/// PlugInit (L155-159; the info-instance-name form).
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

}  // namespace ora::mods
