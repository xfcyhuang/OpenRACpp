// UPSTREAM: OpenRA.Mods.Common/Traits/Player/MissionObjectives.cs 实现部分
//          The implementation half.
#include "mods/mission_objectives.hpp"

#include "mods/cash_trickler.hpp"
#include "mods/changes_health.hpp"
#include "mods/gps_watcher.hpp"
#include "mods/immobile.hpp"
#include "mods/scaredy_cat.hpp"
#include "sim/actor_init.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/player.hpp"
#include "sim/sync_hash.hpp"
#include "sim/world.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

// ———— MissionObjectivesInfoData::Parse(L23-47 的字段面)————
// ———— MissionObjectivesInfoData::Parse (the L23-47 field face) ————

MissionObjectivesInfoData MissionObjectivesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  MissionObjectivesInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "Cooperative"))
    data.b_cooperative = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "EarlyGameOver"))
    data.b_early_game_over = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "GameOverDelay"))
    data.int4_game_over_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldString(rec_info, "WinNotification"))
    data.str_win_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "WinTextNotification"))
    data.str_win_text_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "LoseNotification"))
    data.str_loss_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "LoseTextNotification"))
    data.str_loss_text_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "LeaveNotification"))
    data.str_leave_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "LeaveTextNotification"))
    data.str_leave_text_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "MissionType"))
    data.str_mission_type = std::string{*v};

  return data;
}

MissionObjectives::MissionObjectives(sim::Player& player,
                                     MissionObjectivesInfoData&& info)
    : p_player_{&player}, info_{std::move(info)} {}

int MissionObjectives::Add(sim::Player& player,
                           std::string str_description,
                           std::string str_type, bool b_required,
                           bool b_inhibit_announcement) {
  const int int4_new_id = static_cast<int>(vec_objectives_.size());
  vec_objectives_.emplace_back(std::move(str_description),
                               std::move(str_type), b_required);

  player.SetHasObjectives(true);
  for (auto& [token, fn_handler] : vec_objective_added_)
    if (fn_handler)
      fn_handler(player, b_inhibit_announcement);

  for (auto* inou :
       player.PlayerActor()->TraitsImplementing<sim::INotifyObjectivesUpdated>())
    inou->OnObjectiveAdded(player, int4_new_id);

  return int4_new_id;
}

std::size_t MissionObjectives::AddObjectiveAddedHandler(
    ObjectiveAddedFn fn_handler) {
  const std::size_t n_token = n_next_token_++;
  vec_objective_added_.emplace_back(n_token, std::move(fn_handler));
  return n_token;
}

void MissionObjectives::RemoveObjectiveAddedHandler(std::size_t n_token) {
  for (auto& [token, fn_handler] : vec_objective_added_)
    if (token == n_token) {
      fn_handler = nullptr;
      return;
    }
}

// ———— [VerifySync] 哈希注册({ObjectivesHash};第十二批 G2 补面)————
// ———— The [VerifySync] hash registration ({ObjectivesHash}; the
//      batch-12 G2 missing face) ————

int MissionObjectivesSyncHash(const sim::ISync* s) {
  const auto* objectives = static_cast<const MissionObjectives*>(s);
  return sim::sync::CombineSyncHash(0, objectives->ObjectivesHash());
}

const bool b_mission_objectives_sync_registered = [] {
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.MissionObjectives",
      &MissionObjectivesSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_mission_objectives_sync_registered_anchor =
    &b_mission_objectives_sync_registered;

void RegisterCommonTraitsBatch12Objectives() {
  auto& registry = sim::TraitRegistry::Instance();

  registry.Register(
      "MissionObjectivesInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        MissionObjectivesInfoData data =
            MissionObjectivesInfoData::Parse(rec_info);
        return arena.Create<MissionObjectives>(*init.Self().Owner(),
                                               std::move(data));
      });

  registry.Register(
      "GpsWatcherInfo",
      [](const meta::RecordObject&, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<GpsWatcher>();
      });

  registry.Register(
      "CashTricklerInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<CashTrickler>(
            CashTricklerInfoData::Parse(rec_info));
      });

  registry.Register(
      "ChangesHealthInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<ChangesHealth>(
            ChangesHealthInfoData::Parse(rec_info));
      });

  registry.Register(
      "ImmobileInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<Immobile>(init,
                                      ImmobileInfoData::Parse(rec_info));
      });

  registry.Register(
      "ScaredyCatInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<ScaredyCat>(
            ScaredyCatInfoData::Parse(rec_info));
      });
}

}  // namespace ora::mods
