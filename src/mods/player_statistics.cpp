// UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerStatistics.cs @1f594e320
//          L22-311 的实现面
//          The implementation face of L22-311.
#include "mods/player_statistics.hpp"

#include "core/int2.hpp"
#include "game/ruleset.hpp"
#include "mods/render_sprites.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

const game::ActorInfo* PlayerStatistics::FindActorInfoByNameLocal(
    const std::string& str_name) const {
  for (const auto& [name, info] : ptr_world_->Map().Rules().Actors())
    if (name == str_name)
      return info.get();
  return nullptr;
}

namespace {

std::vector<ProductionQueueInfoData> FindProductionQueueInfos(
    const game::ActorInfo& ai) {
  std::vector<ProductionQueueInfoData> vec_out;
  constexpr std::string_view sv_suffix{"ProductionQueueInfo"};
  for (const meta::RecordObject* rec_trait : ai.TraitsInConstructOrder()) {
    const std::string_view str_full =
        rec_trait->record_desc().str_full_name;
    if (str_full.size() >= sv_suffix.size() &&
        str_full.substr(str_full.size() - sv_suffix.size()) == sv_suffix)
      vec_out.push_back(ProductionQueueInfoData::Parse(*rec_trait));
  }
  return vec_out;
}

}  // namespace

ArmyUnit::ArmyUnit(const game::ActorInfo& actor_info, sim::Player& owner)
    : ptr_actor_info_{&actor_info} {
  for (const meta::RecordObject* rec_trait :
       actor_info.TraitsInConstructOrder()) {
    const std::string_view str_full =
        rec_trait->record_desc().str_full_name;
    if (str_full == std::string_view{"OpenRA.Mods.Common.Traits.BuildableInfo"})
      opt_buildable_info_ = BuildableInfoData::Parse(*rec_trait);
    else if (str_full ==
             std::string_view{"OpenRA.Mods.Common.Traits.TooltipInfo"}) {
      TooltipInfoData parsed = TooltipInfoData::Parse(*rec_trait);
      if (parsed.conditional.b_enabled_by_default)
        ptr_tooltip_info_ =
            std::make_unique<TooltipInfoData>(std::move(parsed));
    }
  }

  const meta::RecordObject* rec_rsi = nullptr;
  for (const meta::RecordObject* rec_trait :
       actor_info.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.Render.RenderSpritesInfo"}) {
      rec_rsi = rec_trait;
      break;
    }

  if (opt_buildable_info_.has_value() && rec_rsi != nullptr) {
    const BuildableInfoData& bi = *opt_buildable_info_;
    const RenderSpritesInfoData rsi =
        RenderSpritesInfoData::Parse(*rec_rsi);
    const std::string str_image =
        rsi.GetImage(actor_info, owner.Faction().InternalName);
    ptr_icon_ = std::make_unique<gfx::Animation>(
        RenderSprites::MakeAnimationDeps(owner.GetWorld()), str_image);
    ptr_icon_->Play(bi.str_icon);
    str_icon_palette_ = bi.str_icon_palette;
    b_icon_palette_is_player_palette_ = bi.b_icon_palette_is_player_palette;
    int4_build_palette_order_ = bi.int4_build_palette_order;

    int int4_queue_order = 0;
    bool b_first = true;
    for (const auto& [name, info] : owner.GetWorld().Map().Rules().Actors()) {
      for (const ProductionQueueInfoData& queue :
           FindProductionQueueInfos(*info)) {
        const bool b_in_queue = std::find(bi.vec_queue.begin(),
                                          bi.vec_queue.end(),
                                          queue.str_type) !=
                                bi.vec_queue.end();
        if (!b_in_queue)
          continue;

        if (b_first || queue.int4_display_order < int4_queue_order) {
          int4_queue_order = queue.int4_display_order;
          b_first = false;
        }
      }
    }

    int4_production_queue_order_ = int4_queue_order;
  }
}

PlayerStatisticsInfoData PlayerStatisticsInfoData::Parse(
    const meta::RecordObject&) {
  return {};
}

PlayerStatistics::PlayerStatistics(const ActorInitializer& init)
    : ptr_world_{&init.Self().world()},
      ptr_owner_{init.Self().Owner()} {}

void PlayerStatistics::Created(Actor& self) {
  ptr_resources_ = self.TraitOrDefault<PlayerResources>();
  ptr_experience_ = self.TraitOrDefault<PlayerExperience>();

  b_income_graph_disabled_ = ptr_resources_ == nullptr;
}

void PlayerStatistics::Tick(Actor& self) {
  int4_ticks_++;

  const int int4_timestep = self.world().Timestep();
  if (int4_ticks_ * int4_timestep >= 30000) {
    int4_ticks_ = 0;

    if (!b_army_graph_disabled_ &&
        (ArmyValue != 0 ||
         self.Owner()->PlayerWinState() == sim::WinState::Undefined))
      vec_army_samples_.push_back(ArmyValue);
    else
      b_army_graph_disabled_ = true;

    if (!b_income_graph_disabled_ &&
        (Income != 0 ||
         self.Owner()->PlayerWinState() == sim::WinState::Undefined))
      vec_income_samples_.push_back(Income);
    else
      b_income_graph_disabled_ = true;
  }

  if (ptr_resources_ == nullptr)
    return;

  const int int4_tick_delta = self.world().WorldTick() - int4_last_income_tick_;
  if (int4_tick_delta * int4_timestep >= 1000) {
    int4_last_income_tick_ = self.world().WorldTick();

    const int int4_last_earned =
        deque_earned_seconds_.size() > 59 ? deque_earned_seconds_.front() : 0;
    if (deque_earned_seconds_.size() > 59)
      deque_earned_seconds_.pop_front();
    int4_last_income_ = DisplayIncome = Income;
    Income = ptr_resources_->Earned - int4_last_earned;
    deque_earned_seconds_.push_back(ptr_resources_->Earned);
  } else {
    DisplayIncome =
        int2::Lerp(int4_last_income_, Income, int4_tick_delta * int4_timestep,
                   1000);
  }
}

void PlayerStatistics::ResolveOrder(Actor&, const net::Order& order) {
  if (order.str_order_string.starts_with("Dev"))
    return;

  OrderCount++;
}

void PlayerStatistics::WorldLoaded(sim::World&, gfx::WorldRenderer*) {
  if (!b_army_graph_disabled_)
    vec_army_samples_.push_back(ArmyValue);

  if (!b_income_graph_disabled_)
    vec_income_samples_.push_back(Income);
}

UpdatesPlayerStatisticsInfoData UpdatesPlayerStatisticsInfoData::Parse(
    const meta::RecordObject& rec_info) {
  UpdatesPlayerStatisticsInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "AddToArmyValue"))
    if (const auto* b = std::get_if<bool>(&gv->val))
      data.b_add_to_army_value = *b;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "AddToAssetsValue"))
    if (const auto* b = std::get_if<bool>(&gv->val))
      data.b_add_to_assets_value = *b;
  if (const auto v = sim::RecordFieldString(rec_info, "OverrideActor"))
    data.str_override_actor = std::string{*v};
  return data;
}

UpdatesPlayerStatistics::UpdatesPlayerStatistics(
    const ActorInitializer& init, UpdatesPlayerStatisticsInfoData info)
    : info_{std::move(info)} {
  Actor& self = init.Self();
  for (const meta::RecordObject* rec_trait :
       self.Info()->TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{"OpenRA.Mods.Common.Traits.ValuedInfo"}) {
      int4_cost_ = ValuedInfoData::Parse(*rec_trait).int4_cost;
      break;
    }

  ptr_player_stats_ =
      self.Owner()->PlayerActor()->Trait<PlayerStatistics>();
  str_actor_name_ = info_.str_override_actor.empty()
                        ? self.Info()->Name()
                        : info_.str_override_actor;
}

void UpdatesPlayerStatistics::Killed(Actor& self,
                                     const sim::AttackInfo& e) {
  if (self.Owner()->PlayerWinState() != sim::WinState::Undefined)
    return;

  if (b_included_in_army_value_) {
    ptr_player_stats_->ArmyValue -= int4_cost_;
    b_included_in_army_value_ = false;
    ptr_player_stats_->Unit(str_actor_name_).Count--;
  }

  if (b_included_in_assets_value_) {
    ptr_player_stats_->AssetsValue -= int4_cost_;
    b_included_in_assets_value_ = false;
  }

  ptr_player_stats_->DeathsCost += int4_cost_;

  if (e.Attacker == &self)
    return;

  PlayerStatistics* attacker_stats =
      e.Attacker->Owner()->PlayerActor()->Trait<PlayerStatistics>();
  const bool b_is_building = self.Info()->HasTraitInfoOfInterface(
      "OpenRA.Mods.Common.Traits.BuildingInfo");
  if (b_is_building) {
    if (!self.Owner()->NonCombatant())
      attacker_stats->BuildingsKilled++;

    ptr_player_stats_->BuildingsDead++;
  } else if (self.Info()->HasTraitInfoOfInterface(
                 "OpenRA.Mods.Common.Traits.IPositionableInfo")) {
    if (!self.Owner()->NonCombatant())
      attacker_stats->UnitsKilled++;

    ptr_player_stats_->UnitsDead++;
  }

  if (!self.Owner()->NonCombatant())
    attacker_stats->KillsCost += int4_cost_;
}

void UpdatesPlayerStatistics::Created(Actor&) {
  b_included_in_army_value_ = info_.b_add_to_army_value;
  if (b_included_in_army_value_) {
    ptr_player_stats_->ArmyValue += int4_cost_;
    ptr_player_stats_->Unit(str_actor_name_).Count++;
  }

  b_included_in_assets_value_ = info_.b_add_to_assets_value;
  if (b_included_in_assets_value_)
    ptr_player_stats_->AssetsValue += int4_cost_;
}

void UpdatesPlayerStatistics::OnOwnerChanged(Actor&, sim::Player&,
                                             sim::Player& new_owner) {
  PlayerStatistics* new_owner_stats =
      new_owner.PlayerActor()->Trait<PlayerStatistics>();
  if (b_included_in_army_value_) {
    ptr_player_stats_->ArmyValue -= int4_cost_;
    new_owner_stats->ArmyValue += int4_cost_;
    ptr_player_stats_->Unit(str_actor_name_).Count--;
    new_owner_stats->Unit(str_actor_name_).Count++;
  }

  if (b_included_in_assets_value_) {
    ptr_player_stats_->AssetsValue -= int4_cost_;
    new_owner_stats->AssetsValue += int4_cost_;
  }

  ptr_player_stats_ = new_owner_stats;
}

void UpdatesPlayerStatistics::Disposing(Actor&) {
  if (b_included_in_army_value_) {
    ptr_player_stats_->ArmyValue -= int4_cost_;
    b_included_in_army_value_ = false;
    ptr_player_stats_->Unit(str_actor_name_).Count--;
  }

  if (b_included_in_assets_value_) {
    ptr_player_stats_->AssetsValue -= int4_cost_;
    b_included_in_assets_value_ = false;
  }
}

}  // namespace ora::mods
