#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
class World;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::net {
struct Order;
}

namespace ora::mods {

struct ValuedInfoData {
  int int4_cost = 0;
  static ValuedInfoData Parse(const meta::RecordObject& rec_info);
};

class Valued final : public sim::TraitBase {
 public:

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Valued;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Valued>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct BuildableInfoData {
  std::vector<std::string> vec_prerequisites;
  std::vector<std::string> vec_queue;
  std::string str_build_at_production_type;
  int int4_build_limit = 0;
  std::string str_force_faction;
  std::string str_icon{"icon"};
  std::string str_icon_palette{"chrome"};
  bool b_icon_palette_is_player_palette = false;
  int int4_build_duration = -1;
  int int4_build_duration_modifier = 60;
  int int4_build_palette_order = 9999;
  std::string str_description;

  static BuildableInfoData Parse(const meta::RecordObject& rec_info);

  static std::string GetInitialFaction(const game::ActorInfo& ai,
                                       const std::string& default_faction);
};

class Buildable final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Buildable;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Buildable>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct ExitInfoData {
  WVec spawn_offset{0, 0, 0};
  CVec exit_cell{0, 0};
  std::optional<WAngle> opt_facing;
  std::vector<std::string> vec_production_types;
  int int4_exit_delay = 0;
  int int4_priority = 1;
  sim::ConditionalTraitData conditional;

  static ExitInfoData Parse(const meta::RecordObject& rec_info);
};

class Exit final : public sim::TraitBase,
                   public sim::ConditionalTraitCore<Exit>,
                   public sim::IObservesVariables,
                   public sim::INotifyCreated {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Exit;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Exit, sim::IObservesVariables,
                                 sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<Exit>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return ConditionalTraitCore<Exit>::CollectObservers();
  }

  void Created(sim::Actor& self) {
    ConditionalTraitCore<Exit>::CoreCreated(self);
  }

  explicit Exit(ExitInfoData info)
      : ConditionalTraitCore<Exit>{info.conditional},
        info_{std::move(info)} {}

  const ExitInfoData& Info() const { return info_; }

 private:
  ExitInfoData info_;
};

class Reservable final : public sim::TraitBase,
                         public sim::ITick,
                         public sim::INotifyOwnerChanged,
                         public sim::INotifySold,
                         public sim::INotifyActorDisposing,
                         public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      Reservable, OpenRA_Mods_Common_Traits_Reservable, sim::ITick,
      sim::INotifyOwnerChanged, sim::INotifySold,
      sim::INotifyActorDisposing, sim::INotifyCreated)

  void Tick(sim::Actor& self) override;
  void Created(sim::Actor& self) override {}
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void Selling(sim::Actor& self) override;
  void Sold(sim::Actor& self) override;
  void Disposing(sim::Actor& self) override;

  static bool IsReserved(const sim::Actor& a);

  static bool IsAvailableFor(sim::Actor& reservable,
                             sim::Actor* for_actor);

  void Reserve(sim::Actor* for_actor);

  void UnReserve();

 private:
  sim::Actor* p_reserved_for_ = nullptr;
};

struct RallyPointInfoData {
  std::vector<CVec> vec_path;

  static RallyPointInfoData Parse(const meta::RecordObject& rec_info);
};

class RallyPoint final : public sim::TraitBase,
                         public sim::IResolveOrder,
                         public sim::INotifyOwnerChanged,
                         public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      RallyPoint, OpenRA_Mods_Common_Traits_RallyPoint, sim::IResolveOrder,
      sim::INotifyOwnerChanged, sim::INotifyCreated)

  RallyPoint(sim::Actor& self, RallyPointInfoData info);

  void ResetPath(sim::Actor& self);

  void Created(sim::Actor& self) override {}
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;

  std::vector<CPos> Path;

  static bool IsForceSet(const ora::net::Order& order);

 private:
  RallyPointInfoData info_;
};

class TechTree;

struct ProvidesPrerequisiteInfoData {
  std::string str_prerequisite;
  std::vector<std::string> vec_requires_prerequisites;
  std::vector<std::string> vec_factions;
  bool b_reset_on_owner_change = false;
  sim::ConditionalTraitData conditional;

  static ProvidesPrerequisiteInfoData Parse(
      const meta::RecordObject& rec_info);
};

class ProvidesPrerequisite final : public sim::TraitBase,
                                   public sim::ConditionalTraitCore<
                                       ProvidesPrerequisite>,
                                   public sim::ITechTreePrerequisite,
                                   public sim::INotifyOwnerChanged,
                                   public sim::INotifyCreated,
                                   public sim::IObservesVariables {
 public:
  ORA_TRAIT_INTERFACES(
      ProvidesPrerequisite, OpenRA_Mods_Common_Traits_ProvidesPrerequisite,
      sim::ITechTreePrerequisite, sim::INotifyOwnerChanged,
      sim::INotifyCreated, sim::IObservesVariables)

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<
        ProvidesPrerequisite>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return ConditionalTraitCore<ProvidesPrerequisite>::
        CollectObservers();
  }

  ProvidesPrerequisite(sim::ActorInitializer& init,
                       ProvidesPrerequisiteInfoData info);

  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  std::vector<std::string> ProvidesPrerequisites()
      override;

  void Update();

 private:
  std::vector<std::string> vec_prerequisites_;
  bool b_enabled_ = false;
  TechTree* p_tech_tree_ = nullptr;
  std::string str_faction_;
  ProvidesPrerequisiteInfoData info_;
};

class TechTree final : public sim::TraitBase {
 public:

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_TechTree;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<TechTree>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  explicit TechTree(sim::ActorInitializer& init);

  void ActorChanged(sim::Actor& a);

  void Update();

  void Add(const std::string& key,
           const std::vector<std::string>& vec_prerequisites, int limit,
           sim::ITechTreeElement* tte);

  void Remove(const std::string& key);

  void RemoveElement(sim::ITechTreeElement* tte);

  bool HasPrerequisites(const std::vector<std::string>& vec_prerequisites);

  sim::Player* Owner() const { return p_owner_; }

 private:

  struct Watcher {
    std::string key;
    sim::ITechTreeElement* registered_by = nullptr;
    std::vector<std::string> vec_prerequisites;
    bool b_has_prerequisites = false;
    int int4_limit = 0;
    bool b_hidden = false;
    bool b_initialized = false;

    void Update(const std::map<std::string, int>& map_owned);
  };

  static std::map<std::string, int> GatherOwnedPrerequisites(
      sim::Player* player);

  std::vector<Watcher> vec_watchers_;
  sim::Player* p_owner_ = nullptr;
  sim::World* p_world_ = nullptr;
};

class DeveloperMode final : public sim::TraitBase, public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(DeveloperMode,
                       OpenRA_Mods_Common_Traits_DeveloperMode, sim::ISync)

  bool Enabled = false;

  bool FastCharge = false;
  bool AllTech = false;
  bool FastBuild = false;
  bool DisableShroud = false;
  bool UnlimitedPower = false;

  bool PathDebug = false;

  bool BuildAnywhere = false;
};

}
