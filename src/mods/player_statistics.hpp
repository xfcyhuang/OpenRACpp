#pragma once
import std;

#include "game/actor_info.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/experience.hpp"
#include "mods/player_resources.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/tooltip.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

class PlayerStatistics;

class ArmyUnit {
 public:
  ArmyUnit(const game::ActorInfo& actor_info, sim::Player& owner);

  const game::ActorInfo* ActorInfo() const { return ptr_actor_info_; }
  const gfx::Animation* Icon() const { return ptr_icon_.get(); }
  const std::string& IconPalette() const { return str_icon_palette_; }
  bool IconPaletteIsPlayerPalette() const {
    return b_icon_palette_is_player_palette_;
  }
  int ProductionQueueOrder() const { return int4_production_queue_order_; }
  int BuildPaletteOrder() const { return int4_build_palette_order_; }
  const TooltipInfoData* TooltipInfo() const { return ptr_tooltip_info_.get(); }
  const std::optional<BuildableInfoData>& BuildableInfo() const {
    return opt_buildable_info_;
  }

  int Count = 0;

 private:
  const game::ActorInfo* ptr_actor_info_ = nullptr;
  std::unique_ptr<gfx::Animation> ptr_icon_;
  std::string str_icon_palette_;
  bool b_icon_palette_is_player_palette_ = false;
  int int4_production_queue_order_ = 0;
  int int4_build_palette_order_ = 9999;
  std::unique_ptr<TooltipInfoData> ptr_tooltip_info_;
  std::optional<BuildableInfoData> opt_buildable_info_;
};

struct PlayerStatisticsInfoData {
  static PlayerStatisticsInfoData Parse(const meta::RecordObject& rec_info);
};

class PlayerStatistics final : public TraitBase,
                               public sim::ITick,
                               public sim::IResolveOrder,
                               public sim::INotifyCreated,
                               public sim::IWorldLoaded,
                               public sim::INotifyWinStateChanged {
 public:
  explicit PlayerStatistics(const ActorInitializer& init);

  ORA_TRAIT_INTERFACES(PlayerStatistics,
                       OpenRA_Mods_Common_Traits_PlayerStatistics,
                       sim::ITick, sim::IResolveOrder, sim::INotifyCreated,
                       sim::IWorldLoaded, sim::INotifyWinStateChanged)

  int OrderCount = 0;
  std::optional<int> opt_defeat_tick;

  int GetExperience() const {
    return ptr_experience_ != nullptr ? ptr_experience_->Experience : 0;
  }

  std::vector<int>& IncomeSamples() { return vec_income_samples_; }
  const std::vector<int>& IncomeSamples() const {
    return vec_income_samples_;
  }
  int Income = 0;
  int DisplayIncome = 0;

  std::vector<int>& ArmySamples() { return vec_army_samples_; }
  const std::vector<int>& ArmySamples() const { return vec_army_samples_; }

  int KillsCost = 0;
  int DeathsCost = 0;

  int UnitsKilled = 0;
  int UnitsDead = 0;

  int BuildingsKilled = 0;
  int BuildingsDead = 0;

  int ArmyValue = 0;
  int AssetsValue = 0;

  ArmyUnit& Unit(const std::string& str_name) {
    const auto it = map_units_.find(str_name);
    if (it != map_units_.end())
      return *it->second;

    auto unit = std::make_unique<ArmyUnit>(*FindActorInfoByNameLocal(str_name),
                                           *ptr_owner_);
    const auto pair_inserted =
        map_units_.emplace(str_name, std::move(unit));
    return *pair_inserted.first->second;
  }
  const game::ActorInfo* FindActorInfoByNameLocal(
      const std::string& str_name) const;
  const std::map<std::string, std::unique_ptr<ArmyUnit>>& Units() const {
    return map_units_;
  }

  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;
  void OnPlayerWon(sim::Player&) override {}
  void OnPlayerLost(sim::Player& player) override {
    opt_defeat_tick = player.GetWorld().WorldTick();
  }

 private:
  sim::World* ptr_world_ = nullptr;
  sim::Player* ptr_owner_ = nullptr;
  PlayerResources* ptr_resources_ = nullptr;
  PlayerExperience* ptr_experience_ = nullptr;

  std::vector<int> vec_income_samples_;
  std::vector<int> vec_army_samples_;
  std::deque<int> deque_earned_seconds_;
  int int4_last_income_ = 0;
  int int4_last_income_tick_ = 0;
  int int4_ticks_ = 0;

  bool b_army_graph_disabled_ = false;
  bool b_income_graph_disabled_ = false;
  std::map<std::string, std::unique_ptr<ArmyUnit>> map_units_;
};

struct UpdatesPlayerStatisticsInfoData {
  bool b_add_to_army_value = false;
  bool b_add_to_assets_value = true;
  std::string str_override_actor;

  static UpdatesPlayerStatisticsInfoData Parse(
      const meta::RecordObject& rec_info);
};

class UpdatesPlayerStatistics final : public TraitBase,
                                      public sim::INotifyKilled,
                                      public sim::INotifyCreated,
                                      public sim::INotifyOwnerChanged,
                                      public sim::INotifyActorDisposing {
 public:
  UpdatesPlayerStatistics(const ActorInitializer& init,
                          UpdatesPlayerStatisticsInfoData info);

  ORA_TRAIT_INTERFACES(UpdatesPlayerStatistics,
                       OpenRA_Mods_Common_Traits_UpdatesPlayerStatistics,
                       sim::INotifyKilled, sim::INotifyCreated,
                       sim::INotifyOwnerChanged, sim::INotifyActorDisposing)

  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void Created(Actor& self) override;
  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void Disposing(Actor& self) override;

 private:
  UpdatesPlayerStatisticsInfoData info_;
  std::string str_actor_name_;
  int int4_cost_ = 0;
  PlayerStatistics* ptr_player_stats_ = nullptr;
  bool b_included_in_army_value_ = false;
  bool b_included_in_assets_value_ = false;
};

}
