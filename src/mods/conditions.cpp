// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @b6fc03f(逐语句
//          重写;文件清单见 conditions.hpp 头注)
//          Statement-by-statement rewrite; the file list lives in
//          conditions.hpp's header note.
#include "mods/conditions.hpp"

#include "mods/external_condition.hpp"


#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "mods/move_activities.hpp"
#include "map/map.hpp"
#include "mods/armament.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "mods/power.hpp"
#include "mods/production_support.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

/// RandomOrDefault(IReadOnlyList, Random):空集合零消耗(与 warheads.cpp
/// 的 RandomOrDefaultOf 同语义;声音域 = LocalRandom)
/// RandomOrDefault (IReadOnlyList, Random): an empty list consumes
/// nothing (the same semantics as warheads.cpp's RandomOrDefaultOf; the
/// sound domain = LocalRandom).
std::string RandomOrDefaultOf(MersenneTwister& rng,
                              const std::vector<std::string>& vec_xs) {
  if (vec_xs.empty())
    return {};
  const std::int32_t index =
      rng.Next(0, static_cast<std::int32_t>(vec_xs.size()));
  return vec_xs[static_cast<std::size_t>(index)];
}

/// LowerInvariant 集合包含(GrantConditionOnProduction 的 Actors 键)
/// The LowerInvariant set containment (GrantConditionOnProduction's
/// Actors key).
std::string ToLower(std::string str_in) {
  for (char& c : str_in)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return str_in;
}

}  // namespace

// ———— GrantCondition ————

GrantConditionInfoData GrantConditionInfoData::Parse(
    const meta::RecordObject& rec) {
  GrantConditionInfoData data;
  if (const auto s = sim::RecordFieldString(rec, "Condition"))
    data.str_condition = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec, "GrantPermanently"))
    data.b_grant_permanently = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec);
  return data;
}

void GrantCondition::TraitEnabledHook(sim::Actor& self) {
  // L33-37
  if (int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(info_.str_condition);
}

void GrantCondition::TraitDisabledHook(sim::Actor& self) {
  // L39-46
  if (info_.b_grant_permanently ||
      int4_condition_token_ == sim::Actor::InvalidConditionToken)
    return;
  int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

// ———— GrantConditionOnTileSet ————

void GrantConditionOnTileSet::Created(sim::Actor& self) {
  // L38-44
  for (const std::string& tile_set : vec_tile_sets_)
    if (tile_set == self.world().Map().Tileset()) {
      self.GrantCondition(str_condition_);
      break;
    }
}

// ———— GrantRandomCondition ————

void GrantRandomCondition::Created(sim::Actor& self) {
  // L41-45
  if (vec_conditions_.empty())
    return;

  // ImmutableArrayExtensions.Random(SharedRandom):Next(0, count) 一次消耗
  // ImmutableArrayExtensions.Random (SharedRandom): one Next(0, count)
  // consumption.
  const std::int32_t index = self.world().SharedRandom().Next(
      0, static_cast<std::int32_t>(vec_conditions_.size()));
  self.GrantCondition(
      vec_conditions_[static_cast<std::size_t>(index)]);
}

// ———— GrantConditionWhileAiming ————

void GrantConditionWhileAiming::StartedAiming(sim::Actor& self,
                                             sim::TraitBase*) {
  // L36-40
  if (int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionWhileAiming::StoppedAiming(sim::Actor& self,
                                              sim::TraitBase*) {
  // L42-46
  if (int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

// ———— GrantConditionOnDamageState ————

void GrantConditionOnDamageState::Created(sim::Actor& self) {
  // L46-49
  GrantConditionOnValidDamageState(self);
}

void GrantConditionOnDamageState::GrantConditionOnValidDamageState(
    sim::Actor& self) {
  // L51-58
  const sim::DamageState state =
      self.Trait<sim::IHealth>()->DamageState();
  if ((static_cast<std::int32_t>(valid_damage_states_) &
       static_cast<std::int32_t>(state)) !=
          static_cast<std::int32_t>(state) ||
      int4_condition_token_ != sim::Actor::InvalidConditionToken)
    return;

  int4_condition_token_ = self.GrantCondition(str_condition_);

  // Game.CosmeticRandom 的 LocalRandom 域(上游 not-synced;消耗保留)
  // Game.CosmeticRandom's LocalRandom domain (upstream not-synced; the
  // consumption is kept).
  RandomOrDefaultOf(self.world().LocalRandom(), vec_enabled_sounds_);
}

void GrantConditionOnDamageState::DamageStateChanged(
    sim::Actor& self, const sim::AttackInfo& e) {
  // L60-81
  const bool granted =
      int4_condition_token_ != sim::Actor::InvalidConditionToken;
  if (granted && b_grant_permanently_)
    return;

  const auto has_flag = [](sim::DamageState set, sim::DamageState f) {
    return (static_cast<std::int32_t>(set) &
            static_cast<std::int32_t>(f)) == static_cast<std::int32_t>(f);
  };

  if (!granted && !has_flag(valid_damage_states_, e.previous_damage_state))
    GrantConditionOnValidDamageState(self);
  else if (granted && !has_flag(valid_damage_states_, e.damage_state) &&
           has_flag(valid_damage_states_, e.previous_damage_state)) {
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
    RandomOrDefaultOf(self.world().LocalRandom(), vec_disabled_sounds_);
  }
}

// ———— GrantConditionOnHealth ————

void GrantConditionOnHealth::Validate(int health_max_hp) const {
  // RulesetLoaded L56-62(文本逐字)
  if (health_max_hp < int4_min_hp_)
    throw yaml::YamlException(
        "Minimum HP (" + std::to_string(int4_min_hp_) +
        ") for GrantConditionOnHealth can't be more than actor's Maximum "
        "HP (" +
        std::to_string(health_max_hp) + ")");
}

void GrantConditionOnHealth::Created(sim::Actor& self) {
  // ctor L58 + Created L65-68
  const sim::IHealth* health = self.Trait<sim::IHealth>();
  int4_effective_max_hp_ =
      int4_max_hp_ > 0 ? int4_max_hp_ : health->MaxHP();
  GrantConditionOnValidHealth(self);
}

void GrantConditionOnHealth::GrantConditionOnValidHealth(sim::Actor& self) {
  // L70-78
  const int hp = self.Trait<sim::IHealth>()->HP();
  if (int4_min_hp_ > hp || int4_effective_max_hp_ < hp ||
      int4_condition_token_ != sim::Actor::InvalidConditionToken)
    return;

  int4_condition_token_ = self.GrantCondition(str_condition_);
  RandomOrDefaultOf(self.world().LocalRandom(), vec_enabled_sounds_);
}

void GrantConditionOnHealth::Damaged(sim::Actor& self,
                                     const sim::AttackInfo&) {
  // L81-99
  const bool granted =
      int4_condition_token_ != sim::Actor::InvalidConditionToken;
  if (granted && b_grant_permanently_)
    return;

  const int hp = self.Trait<sim::IHealth>()->HP();
  if (!granted)
    GrantConditionOnValidHealth(self);
  else if (int4_min_hp_ > hp || int4_effective_max_hp_ < hp) {
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
    RandomOrDefaultOf(self.world().LocalRandom(), vec_disabled_sounds_);
  }
}

// ———— GrantConditionOnTerrain ————

void GrantConditionOnTerrain::Tick(sim::Actor& self) {
  // L50-71
  const CPos cell = self.Location();
  if (!self.world().Map().Contains(cell))
    return;

  // 地形类型可在 actor 不动时变化(上游注释)
  // The terrain type may change between ticks without the actor moving
  // (upstream comment).
  const map::Map& map = self.world().Map();
  std::string current_terrain = map.GetTerrainInfo(cell).Type;
  if (cell.Layer() != 0) {
    // terrainTypes[World.GetCustomMovementLayers()[layer].
    // GetTerrainIndex(cell)].Type(自定义层索引)
    // terrainTypes[World.GetCustomMovementLayers()[layer].
    // GetTerrainIndex(cell)].Type (the custom-layer index).
    auto layers = self.world().CustomMovementLayers();
    if (cell.Layer() < layers.size()) {
      const std::uint8_t terrain_index =
          layers[cell.Layer()]->GetTerrainIndex(cell);
      const auto vec_terrain_types =
          map.Rules().TerrainInfo().TerrainTypes();
      if (terrain_index < vec_terrain_types.size())
        current_terrain = vec_terrain_types[terrain_index].Type;
    }
  }

  const bool wants_granted = std::find(vec_terrain_types_.begin(),
                                        vec_terrain_types_.end(),
                                        current_terrain) !=
                             vec_terrain_types_.end();
  if (current_terrain != str_cached_terrain_) {
    if (wants_granted &&
        int4_condition_token_ == sim::Actor::InvalidConditionToken)
      int4_condition_token_ = self.GrantCondition(str_condition_);
    else if (!wants_granted &&
             int4_condition_token_ != sim::Actor::InvalidConditionToken)
      int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
  }

  str_cached_terrain_ = current_terrain;
}

// ———— GrantConditionOnBotOwner ————

void GrantConditionOnBotOwner::Created(sim::Actor& self) {
  // L39-43
  if (self.Owner()->IsBot() &&
      std::find(vec_bots_.begin(), vec_bots_.end(), self.Owner()->BotType()) !=
          vec_bots_.end())
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionOnBotOwner::OnOwnerChanged(sim::Actor& self,
                                              sim::Player&,
                                              sim::Player& new_owner) {
  // L45-53
  if (int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);

  if (std::find(vec_bots_.begin(), vec_bots_.end(), new_owner.BotType()) !=
      vec_bots_.end())
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

// ———— GrantConditionOnCombatantOwner ————

void GrantConditionOnCombatantOwner::Created(sim::Actor& self) {
  // L39-43
  if (!self.Owner()->NonCombatant())
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionOnCombatantOwner::OnOwnerChanged(
    sim::Actor& self, sim::Player&, sim::Player& new_owner) {
  // L45-54
  if (int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);

  if (!new_owner.NonCombatant())
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

// ———— GrantConditionOnPlayerResources ————

void GrantConditionOnPlayerResources::Created(sim::Actor& self) {
  // L43-46
  ptr_player_resources_ =
      self.Owner()->PlayerActor()->Trait<PlayerResources>();
}

void GrantConditionOnPlayerResources::OnOwnerChanged(sim::Actor&,
                                                     sim::Player&,
                                                     sim::Player& new_owner) {
  // L48-51
  ptr_player_resources_ =
      new_owner.PlayerActor()->Trait<PlayerResources>();
}

void GrantConditionOnPlayerResources::Tick(sim::Actor& self) {
  // L53-66
  if (str_condition_.empty())
    return;

  const bool enabled = ptr_player_resources_->Resources >
                       int4_threshold_;
  if (enabled && int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
  else if (!enabled &&
           int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

// ———— GrantConditionOnFaction ————

GrantConditionOnFaction::GrantConditionOnFaction(
    sim::ActorInitializer& init, std::string str_condition,
    std::vector<std::string> vec_factions, bool b_reset_on_owner_change,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      vec_factions_{std::move(vec_factions)},
      b_reset_on_owner_change_{b_reset_on_owner_change} {
  // L36-38:FactionInit 缺省 = Owner.Faction.InternalName
  // L36-38: FactionInit's fallback = Owner.Faction.InternalName.
  str_faction_ = init.GetValue<sim::FactionInit, std::string>(
      std::string_view{},
      init.Self().Owner()->Faction().InternalName);
}

GrantConditionOnFaction::~GrantConditionOnFaction() = default;

void GrantConditionOnFaction::OnOwnerChanged(sim::Actor& self,
                                             sim::Player&,
                                             sim::Player& new_owner) {
  // L43-52:基类钩子的直呼形态(上游调 protected TraitDisabled/
  // TraitEnabled —— C++ 侧经核的同一变换)
  // L43-52: the direct-call shape of the base hooks (upstream invokes
  // the protected TraitDisabled/TraitEnabled — the same transitions via
  // the core on the C++ side).
  if (b_reset_on_owner_change_ &&
      str_faction_ != new_owner.Faction().InternalName) {
    str_faction_ = new_owner.Faction().InternalName;
    TraitDisabledHook(self);
    TraitEnabledHook(self);
  }
}

void GrantConditionOnFaction::TraitEnabledHook(sim::Actor& self) {
  // L54-57
  if (int4_condition_token_ == sim::Actor::InvalidConditionToken &&
      std::find(vec_factions_.begin(), vec_factions_.end(), str_faction_) !=
          vec_factions_.end())
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionOnFaction::TraitDisabledHook(sim::Actor& self) {
  // L59-64
  if (int4_condition_token_ == sim::Actor::InvalidConditionToken)
    return;
  int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

// ———— GrantConditionOnPowerState ————

GrantConditionOnPowerState::GrantConditionOnPowerState(
    std::string str_condition, std::int32_t int4_valid_power_states,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      int4_valid_power_states_{int4_valid_power_states} {}

GrantConditionOnPowerState::~GrantConditionOnPowerState() = default;

void GrantConditionOnPowerState::Created(sim::Actor& self) {
  // L38-43
  ptr_player_power_ = self.Owner()->PlayerActor()->Trait<PowerManager>();
  Update(self);
}

void GrantConditionOnPowerState::OnOwnerChanged(sim::Actor& self,
                                                sim::Player&,
                                                sim::Player& new_owner) {
  // L81-88
  ptr_player_power_ = new_owner.PlayerActor()->Trait<PowerManager>();
  Update(self);
}

void GrantConditionOnPowerState::PowerLevelChanged(sim::Actor& self) {
  // L76-79
  Update(self);
}

void GrantConditionOnPowerState::Update(sim::Actor& self) {
  // L60-74:PowerState 枚举 Normal/Low/Critical = 1/2/4(上游位域)
  // L60-74: the PowerState enum Normal/Low/Critical = 1/2/4 (the
  // upstream bit domain).
  const std::int32_t state_bit =
      static_cast<std::int32_t>(ptr_player_power_->GetPowerState());
  b_valid_power_state_ =
      !ConditionalTraitCore::IsTraitDisabled() &&
      (int4_valid_power_states_ & state_bit) == state_bit;

  if (b_valid_power_state_ &&
      int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
  else if (!b_valid_power_state_ &&
           int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

// ———— GrantConditionOnMovement ————

GrantConditionOnMovement::GrantConditionOnMovement(
    sim::Actor& self, std::string str_condition,
    sim::MovementType valid_movement_types,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      valid_movement_types_{valid_movement_types} {
  ptr_movement_ = self.Trait<sim::IMove>();
}

GrantConditionOnMovement::~GrantConditionOnMovement() = default;

void GrantConditionOnMovement::MovementTypeChanged(
    sim::Actor& self, sim::MovementType types) {
  // L44-47
  UpdateCondition(self, types);
}

void GrantConditionOnMovement::UpdateCondition(sim::Actor& self,
                                               sim::MovementType types) {
  // L31-41
  const bool valid_movement =
      !ConditionalTraitCore::IsTraitDisabled() &&
      (static_cast<std::int32_t>(types) &
       static_cast<std::int32_t>(valid_movement_types_)) != 0;

  if (!valid_movement &&
      int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
  else if (valid_movement &&
           int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionOnMovement::TraitEnabledHook(sim::Actor& self) {
  // L49-52
  UpdateCondition(self, ptr_movement_->CurrentMovementTypes());
}

void GrantConditionOnMovement::TraitDisabledHook(sim::Actor& self) {
  // L54-57
  UpdateCondition(self, ptr_movement_->CurrentMovementTypes());
}

// ———— GrantConditionOnAttack ————

GrantConditionOnAttack::GrantConditionOnAttack(InfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {}

GrantConditionOnAttack::~GrantConditionOnAttack() = default;

void GrantConditionOnAttack::GrantInstance(sim::Actor& self) {
  // L55-60
  if (info_.str_condition.empty())
    return;

  vec_tokens_.push_back(self.GrantCondition(info_.str_condition));
}

void GrantConditionOnAttack::RevokeInstance(sim::Actor& self,
                                            bool revoke_all) {
  // L62-74
  int4_shots_fired_ = 0;

  if (vec_tokens_.empty())
    return;

  if (!revoke_all)
    self.RevokeCondition(vec_tokens_.back()), vec_tokens_.pop_back();
  else
    while (!vec_tokens_.empty())
      self.RevokeCondition(vec_tokens_.back()), vec_tokens_.pop_back();
}

bool GrantConditionOnAttack::TargetChanged(const sim::Target& last_target,
                                           const sim::Target& target) {
  // L77-106
  if (last_target.Type() == sim::TargetType::FrozenActor &&
      target.Type() == sim::TargetType::Actor &&
      last_target.FrozenActorPtr->ActorPtr() == target.ActorPtr)
    return false;

  if (last_target.Type() == sim::TargetType::Actor &&
      target.Type() == sim::TargetType::FrozenActor &&
      target.FrozenActorPtr->ActorPtr() == last_target.ActorPtr)
    return false;

  if (last_target.Type() != target.Type())
    return true;

  if (last_target.Type() == sim::TargetType::Actor &&
      target.Type() == sim::TargetType::Actor &&
      last_target.ActorPtr != target.ActorPtr)
    return true;

  if (last_target.Type() == sim::TargetType::FrozenActor &&
      target.Type() == sim::TargetType::FrozenActor &&
      last_target.FrozenActorPtr != target.FrozenActorPtr)
    return true;

  if (last_target.Type() == sim::TargetType::Terrain &&
      target.Type() == sim::TargetType::Terrain &&
      last_target.CenterPosition() != target.CenterPosition())
    return true;

  return false;
}

void GrantConditionOnAttack::Tick(sim::Actor& self) {
  // L68-75
  if (!vec_tokens_.empty() && --int4_cooldown_ == 0) {
    int4_cooldown_ = info_.int4_revoke_delay;
    RevokeInstance(self, info_.b_revoke_all);
  }
}

void GrantConditionOnAttack::Attacking(sim::Actor& self,
                                       const sim::Target& target,
                                       Armament& armament,
                                       const Barrel&) {
  // L108-163
  if (ConditionalTraitCore::IsTraitDisabled() ||
      ConditionalTraitCore::IsTraitPaused())
    return;

  const std::string str_armament_name(armament.InfoData().str_name);
  if (std::find(info_.vec_armament_names.begin(),
                info_.vec_armament_names.end(),
                str_armament_name) == info_.vec_armament_names.end())
    return;

  if (info_.b_revoke_on_new_target) {
    if (TargetChanged(target_last_, target))
      RevokeInstance(self, info_.b_revoke_all);
    target_last_ = target;
  }

  int4_cooldown_ = info_.int4_revoke_delay;

  if (!info_.b_is_cyclic &&
      static_cast<int>(vec_tokens_.size()) >= info_.int4_maximum_instances)
    return;

  int4_shots_fired_++;
  const int required_shots =
      static_cast<int>(vec_tokens_.size()) <
              static_cast<int>(info_.vec_required_shots_per_instance.size())
          ? info_.vec_required_shots_per_instance[vec_tokens_.size()]
          : info_.vec_required_shots_per_instance.back();

  if (int4_shots_fired_ >= required_shots) {
    if (info_.b_is_cyclic &&
        static_cast<int>(vec_tokens_.size()) ==
            info_.int4_maximum_instances)
      RevokeInstance(self, true);
    else
      GrantInstance(self);

    int4_shots_fired_ = 0;
  }
}

void GrantConditionOnAttack::TraitDisabledHook(sim::Actor& self) {
  // L107(Label 下的一行)| the single line under the label.
  RevokeInstance(self, true);
}

// ———— GrantConditionOnProduction ————

void GrantConditionOnProduction::UnitProduced(sim::Actor& self,
                                              sim::Actor& other,
                                              CPos) {
  // L52-61:Actors 键的小写化包含面
  // L52-61: the lowercased containment of the Actors key.
  if (!vec_actors_.empty()) {
    const std::string str_produced = ToLower(other.Info()->Name());
    bool matched = false;
    for (const std::string& actor_key : vec_actors_)
      if (ToLower(actor_key) == str_produced) {
        matched = true;
        break;
      }
    if (!matched)
      return;
  }

  if (int4_token_ == sim::Actor::InvalidConditionToken)
    int4_token_ = self.GrantCondition(str_condition_);

  Ticks = int4_duration_;
}

void GrantConditionOnProduction::Tick(sim::Actor& self) {
  // L63-67
  if (int4_duration_ >= 0 &&
      int4_token_ != sim::Actor::InvalidConditionToken && --Ticks < 0)
    int4_token_ = self.RevokeCondition(int4_token_);
}

float GrantConditionOnProduction::GetValue() {
  // L69-76
  if (!b_show_selection_bar_ || int4_duration_ < 0 ||
      int4_token_ == sim::Actor::InvalidConditionToken)
    return 0.f;

  return static_cast<float>(Ticks) / static_cast<float>(int4_duration_);
}

// ———— GrantConditionOnPrerequisite ————

void GrantConditionOnPrerequisite::Created(sim::Actor& self) {
  // L42-45
  ptr_global_manager_ = self.Owner()->PlayerActor()
                            ->Trait<GrantConditionOnPrerequisiteManager>();
}

void GrantConditionOnPrerequisite::AddedToWorld(sim::Actor& self) {
  // L47-50
  if (!vec_prerequisites_.empty())
    ptr_global_manager_->Register(&self, this, vec_prerequisites_);
}

void GrantConditionOnPrerequisite::RemovedFromWorld(sim::Actor& self) {
  // L52-55
  if (!vec_prerequisites_.empty())
    ptr_global_manager_->Unregister(&self, this, vec_prerequisites_);
}

void GrantConditionOnPrerequisite::OnOwnerChanged(sim::Actor&,
                                                  sim::Player&,
                                                  sim::Player& new_owner) {
  // L57-60
  ptr_global_manager_ = new_owner.PlayerActor()
                            ->Trait<GrantConditionOnPrerequisiteManager>();
}

void GrantConditionOnPrerequisite::PrerequisitesUpdated(sim::Actor& self,
                                                        bool available) {
  // L62-79
  if (available == b_was_available_)
    return;

  if (available && int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
  else if (!available &&
           int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);

  b_was_available_ = available;
}

// ———— GrantConditionOnPrerequisiteManager ————

GrantConditionOnPrerequisiteManager::GrantConditionOnPrerequisiteManager(
    sim::ActorInitializer& init) {
  ptr_tech_tree_ = init.Self().Trait<TechTree>();
}

std::string GrantConditionOnPrerequisiteManager::MakeKey(
    const std::vector<std::string>& vec_prerequisites) {
  // L32-36:string.Join("_", prerequisites.Order())
  // L32-36: string.Join("_", prerequisites.Order()).
  std::vector<std::string> vec_sorted = vec_prerequisites;
  std::sort(vec_sorted.begin(), vec_sorted.end());
  std::string key = "condition_";
  for (std::size_t i = 0; i < vec_sorted.size(); i++) {
    if (i != 0)
      key += "_";
    key += vec_sorted[i];
  }
  return key;
}

void GrantConditionOnPrerequisiteManager::Register(
    sim::Actor* actor, GrantConditionOnPrerequisite* u,
    const std::vector<std::string>& vec_prerequisites) {
  // L40-54
  const std::string key = MakeKey(vec_prerequisites);
  auto it = map_upgradables_.find(key);
  if (it == map_upgradables_.end()) {
    it = map_upgradables_.emplace(key, std::vector<Entry>{}).first;
    ptr_tech_tree_->Add(key, vec_prerequisites, 0, this);
  }

  it->second.push_back(Entry{actor, u});

  // 即时报当前状态 | report the current state immediately.
  u->PrerequisitesUpdated(*actor,
                          ptr_tech_tree_->HasPrerequisites(vec_prerequisites));
}

void GrantConditionOnPrerequisiteManager::Unregister(
    sim::Actor* actor, GrantConditionOnPrerequisite* u,
    const std::vector<std::string>& vec_prerequisites) {
  // L56-66
  const std::string key = MakeKey(vec_prerequisites);
  auto it = map_upgradables_.find(key);
  if (it == map_upgradables_.end())
    return;

  std::vector<Entry>& list = it->second;
  for (std::size_t i = 0; i < list.size();) {
    if (list[i].actor == actor && list[i].upgrade == u)
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
    else
      i++;
  }
  if (list.empty()) {
    map_upgradables_.erase(it);
    ptr_tech_tree_->Remove(key);
  }
}

void GrantConditionOnPrerequisiteManager::PrerequisitesAvailable(
    const std::string& key) {
  // L68-75
  auto it = map_upgradables_.find(key);
  if (it == map_upgradables_.end())
    return;

  for (const Entry& entry : it->second)
    entry.upgrade->PrerequisitesUpdated(*entry.actor, true);
}

void GrantConditionOnPrerequisiteManager::PrerequisitesUnavailable(
    const std::string& key) {
  // L77-84
  auto it = map_upgradables_.find(key);
  if (it == map_upgradables_.end())
    return;

  for (const Entry& entry : it->second)
    entry.upgrade->PrerequisitesUpdated(*entry.actor, false);
}

// ———— SpreadsCondition ————

SpreadsCondition::SpreadsCondition(int probability, WDist range,
                                   std::string spread_condition, int delay,
                                   sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      int4_probability_{probability},
      range_{range},
      str_spread_condition_{std::move(spread_condition)},
      int4_delay_config_{delay} {
  int4_delay_ = delay;
}

SpreadsCondition::~SpreadsCondition() = default;

void SpreadsCondition::Tick(sim::Actor& self) {
  // L42-67
  if (ConditionalTraitCore::IsTraitDisabled())
    return;

  if (int4_delay_-- > 0)
    return;

  int4_delay_ = int4_delay_config_;

  if (self.world().SharedRandom().Next(100) > int4_probability_)
    return;

  // FindActorsInCircle(...).Where(HasTrait<SpreadsCondition>) 的稳定序
  // (ActorID 升序 = FindActorsInCircle 的输出序)
  // The stable order of FindActorsInCircle(...).Where(HasTrait<
  // SpreadsCondition>) (the ActorID-ascending output order of
  // FindActorsInCircle).
  std::vector<sim::Actor*> vec_candidates;
  for (sim::Actor* a :
       self.world().FindActorsInCircle(self.CenterPosition(), range_))
    if (a->TraitOrDefault<SpreadsCondition>() != nullptr)
      vec_candidates.push_back(a);

  if (vec_candidates.empty())
    return;

  // RandomOrDefault(SharedRandom):Next(0, count) 一次消耗
  // RandomOrDefault (SharedRandom): one Next(0, count) consumption.
  const std::int32_t index = self.world().SharedRandom().Next(
      0, static_cast<std::int32_t>(vec_candidates.size()));
  vec_candidates[static_cast<std::size_t>(index)]->GrantCondition(
      str_spread_condition_);
}

// ———— GrantExternalConditionToProduced ————

GrantExternalConditionToProduced::GrantExternalConditionToProduced(
    std::string str_condition, int duration,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      int4_duration_{duration} {}

GrantExternalConditionToProduced::~GrantExternalConditionToProduced() =
    default;

void GrantExternalConditionToProduced::UnitProduced(sim::Actor& self,
                                                    sim::Actor& other,
                                                    CPos) {
  // L37-47(ExternalCondition 于 external_condition.hpp;此处经注册表的
  // 按名转发避免头依赖 —— Trait<ExternalCondition> 的具名查询)
  // L37-47 (ExternalCondition lives in external_condition.hpp; routed
  // through the registry's by-name face to avoid the header dependency —
  // the named Trait<ExternalCondition> query).
  if (ConditionalTraitCore::IsTraitDisabled() || other.IsDead())
    return;

  GrantExternalConditionByName(other, self, str_condition_, int4_duration_);
}

// ———— GrantExternalConditionToCrusher ————

GrantExternalConditionToCrusher::GrantExternalConditionToCrusher(
    std::string str_condition, int duration,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      int4_duration_{duration} {}

GrantExternalConditionToCrusher::~GrantExternalConditionToCrusher() =
    default;

void GrantExternalConditionToCrusher::OnCrush(
    sim::Actor& self, sim::Actor& crusher,
    const core::BitSet<sim::CrushClass>&) {
  // L31-40
  if (ConditionalTraitCore::IsTraitDisabled())
    return;

  GrantExternalConditionByName(crusher, self, str_condition_, int4_duration_);
}

// ———— ToggleConditionOnOrder ————

ToggleConditionOnOrder::ToggleConditionOnOrder(
    std::string str_condition, std::string str_order_name,
    sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      str_condition_{std::move(str_condition)},
      str_order_name_{std::move(str_order_name)} {}

ToggleConditionOnOrder::~ToggleConditionOnOrder() = default;

void ToggleConditionOnOrder::SetCondition(sim::Actor& self, bool granted) {
  // L56-84(声音/通知面省略;token 语义逐字)
  // L56-84 (the sound/notification faces omitted; the token semantics
  // verbatim).
  if (granted && int4_condition_token_ == sim::Actor::InvalidConditionToken) {
    int4_condition_token_ = self.GrantCondition(str_condition_);
  } else if (!granted &&
             int4_condition_token_ != sim::Actor::InvalidConditionToken) {
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
  }
}

void ToggleConditionOnOrder::ResolveOrder(sim::Actor& self,
                                          const net::Order& order) {
  // L104-111
  if (!ConditionalTraitCore::IsTraitDisabled() &&
      !ConditionalTraitCore::IsTraitPaused() &&
      order.str_order_string == str_order_name_) {
    Enabled = !Enabled;
    SetCondition(self, Enabled);
  }
}

void ToggleConditionOnOrder::TraitDisabledHook(sim::Actor& self) {
  // L113-116:禁用即复位
  // L113-116: disabling resets.
  Enabled = false;
  SetCondition(self, false);
}

void ToggleConditionOnOrder::TraitPausedHook(sim::Actor& self) {
  // L118-121:暂停撤条件不复位 enabled
  // L118-121: pausing revokes without resetting enabled.
  SetCondition(self, false);
}

void ToggleConditionOnOrder::TraitResumedHook(sim::Actor& self) {
  // L123-126:恢复还原先前状态
  // L123-126: resuming restores the previous state.
  SetCondition(self, Enabled);
}

// ———— GrantConditionOnClientDock / OnHostDock ————

void GrantConditionOnClientDock::Docked(sim::Actor& self, sim::Actor& host) {
  // L42-58
  const bool host_matches =
      vec_dock_host_names_.empty() ||
      std::find(vec_dock_host_names_.begin(), vec_dock_host_names_.end(),
                host.Info()->Name()) != vec_dock_host_names_.end();
  if (!str_condition_.empty() && host_matches &&
      int4_token_ == sim::Actor::InvalidConditionToken) {
    if (int4_delayed_token_ == sim::Actor::InvalidConditionToken)
      int4_token_ = self.GrantCondition(str_condition_);
    else {
      int4_token_ = int4_delayed_token_;
      int4_delayed_token_ = sim::Actor::InvalidConditionToken;
    }
  }
}

void GrantConditionOnClientDock::Undocked(sim::Actor& self, sim::Actor&) {
  // L60-74
  if (int4_token_ == sim::Actor::InvalidConditionToken ||
      int4_after_dock_duration_ < 0)
    return;
  if (int4_after_dock_duration_ == 0)
    int4_token_ = self.RevokeCondition(int4_token_);
  else {
    int4_delayed_token_ = int4_token_;
    int4_token_ = sim::Actor::InvalidConditionToken;
    Duration = int4_after_dock_duration_;
  }
}

void GrantConditionOnClientDock::Tick(sim::Actor& self) {
  // L76-79
  if (int4_delayed_token_ != sim::Actor::InvalidConditionToken &&
      --Duration <= 0)
    int4_delayed_token_ = self.RevokeCondition(int4_delayed_token_);
}

void GrantConditionOnHostDock::Docked(sim::Actor& self, sim::Actor& client) {
  // L42-58
  const bool client_matches =
      vec_dock_client_names_.empty() ||
      std::find(vec_dock_client_names_.begin(), vec_dock_client_names_.end(),
                client.Info()->Name()) != vec_dock_client_names_.end();
  if (!str_condition_.empty() && client_matches &&
      int4_token_ == sim::Actor::InvalidConditionToken) {
    if (int4_delayed_token_ == sim::Actor::InvalidConditionToken)
      int4_token_ = self.GrantCondition(str_condition_);
    else {
      int4_token_ = int4_delayed_token_;
      int4_delayed_token_ = sim::Actor::InvalidConditionToken;
    }
  }
}

void GrantConditionOnHostDock::Undocked(sim::Actor& self, sim::Actor&) {
  // L60-74
  if (int4_token_ == sim::Actor::InvalidConditionToken ||
      int4_after_dock_duration_ < 0)
    return;
  if (int4_after_dock_duration_ == 0)
    int4_token_ = self.RevokeCondition(int4_token_);
  else {
    int4_delayed_token_ = int4_token_;
    int4_token_ = sim::Actor::InvalidConditionToken;
    Duration = int4_after_dock_duration_;
  }
}

void GrantConditionOnHostDock::Tick(sim::Actor& self) {
  // L76-79
  if (int4_delayed_token_ != sim::Actor::InvalidConditionToken &&
      --Duration <= 0)
    int4_delayed_token_ = self.RevokeCondition(int4_delayed_token_);
}

// ———— GrantConditionOnDeploy ————

GrantConditionOnDeploy::GrantConditionOnDeploy(sim::ActorInitializer& init,
                                               InfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  // L114-118
  ptr_self_ = &init.Self();
  b_check_terrain_type_ = !info_.vec_allowed_terrain_types.empty();
  // L118:GetValue<DeployStateInit, DeployState>(Undeployed)
  // L118: GetValue<DeployStateInit, DeployState>(Undeployed).
  if (const sim::DeployStateInit* deploy_init =
          init.GetOrDefault<sim::DeployStateInit>())
    deploy_state_ = static_cast<DeployState>(deploy_init->Value());
}

GrantConditionOnDeploy::~GrantConditionOnDeploy() = default;

void GrantConditionOnDeploy::Created(sim::Actor& self) {
  // L120-149:notify 集恒空(渲染批)→ 动画分支即完成分支
  // L120-149: the notify set is permanently empty (the render batch) →
  // the animation branch is the completion branch.
  ConditionalTraitCore::CoreCreated(self);

  if (info_.opt_facing.has_value() &&
      deploy_state_ != DeployState::Undeployed) {
    sim::IFacing* facing = self.TraitOrDefault<sim::IFacing>();
    if (facing != nullptr)
      facing->SetFacing(*info_.opt_facing);
  }

  switch (deploy_state_) {
    case DeployState::Undeployed:
      OnUndeployCompleted();
      break;
    case DeployState::Deploying:
      DeployInner(true);
      break;
    case DeployState::Deployed:
      OnDeployCompleted();
      break;
    case DeployState::Undeploying:
      UndeployInner(true);
      break;
  }
}

sim::Activity* GrantConditionOnDeploy::WrapMove(
    sim::Activity* move_inner) {
  // L151-160:UndeployOnMove → DeployForGrantedCondition(moving=true) 后
  // 链内层(活动对象 WorldArena 分配)
  // L151-160: UndeployOnMove → DeployForGrantedCondition(moving=true)
  // chaining the inner move (activities allocated in the WorldArena).
  if (!info_.b_undeploy_on_move)
    return move_inner;

  sim::Activity* activity = ptr_self_->world().Arena().Create<
      DeployForGrantedCondition>(*ptr_self_, this, true);
  activity->Queue(move_inner);
  return activity;
}

bool GrantConditionOnDeploy::TryLockForPickup(sim::Actor& self,
                                              sim::Actor&) {
  // L162-172
  if (!info_.b_undeploy_on_pickup ||
      deploy_state_ == DeployState::Undeployed ||
      ConditionalTraitCore::IsTraitDisabled())
    return true;

  if (deploy_state_ == DeployState::Deployed &&
      !ConditionalTraitCore::IsTraitPaused())
    Undeploy();

  return false;
}

net::Order GrantConditionOnDeploy::IssueDeployOrder(sim::Actor& self,
                                                    bool queued) {
  // L222-225
  return net::Order{"GrantConditionOnDeploy", &self, queued};
}

bool GrantConditionOnDeploy::CanIssueDeployOrder(sim::Actor& self,
                                                 bool queued) {
  // L227-250
  if (ConditionalTraitCore::IsTraitPaused() ||
      ConditionalTraitCore::IsTraitDisabled() || self.IsDead() ||
      self.Disposed())
    return false;

  if (queued || !info_.b_smart_deploy ||
      deploy_state_ == DeployState::Undeployed ||
      deploy_state_ == DeployState::Undeploying)
    return true;

  // 选择集内全部同款未部署才允许 SmartDeploy(上游语义)
  // SmartDeploy allows only when every same-trait actor in the selection
  // is undeployed (upstream semantics).
  for (sim::Actor* actor : self.world().Selection()->Actors()) {
    if (actor == &self || actor->IsDead() || !actor->IsInWorld())
      continue;

    bool blocks = false;
    for (GrantConditionOnDeploy* d :
         actor->TraitsImplementing<GrantConditionOnDeploy>())
      if (!d->IsTraitPaused() && !d->IsTraitDisabled() &&
          (d->GetDeployState() == DeployState::Undeployed ||
           d->GetDeployState() == DeployState::Undeploying))
        blocks = true;
    if (blocks)
      return false;
  }

  return true;
}

void GrantConditionOnDeploy::ResolveOrder(sim::Actor& self,
                                          const net::Order& order) {
  // L252-260
  if (ConditionalTraitCore::IsTraitDisabled() ||
      ConditionalTraitCore::IsTraitPaused())
    return;

  if (order.str_order_string != "GrantConditionOnDeploy")
    return;

  self.QueueActivity(
      order.b_queued,
      self.world().Arena().Create<DeployForGrantedCondition>(self, this,
                                                             false));
}

void GrantConditionOnDeploy::FinishedDeploy(sim::Actor&) {
  // L291-294
  OnDeployCompleted();
}

void GrantConditionOnDeploy::FinishedUndeploy(sim::Actor&) {
  // L296-299
  OnUndeployCompleted();
}

bool GrantConditionOnDeploy::CanDeploy() {
  // L262-266
  if (ConditionalTraitCore::IsTraitPaused() ||
      ConditionalTraitCore::IsTraitDisabled())
    return false;

  return IsValidTerrain(ptr_self_->Location()) ||
         deploy_state_ == DeployState::Deployed;
}

bool GrantConditionOnDeploy::IsValidTerrain(CPos location) {
  // L262-274
  return IsValidTerrainType(location) && IsValidRampType(location);
}

bool GrantConditionOnDeploy::IsValidTerrainType(CPos location) {
  // L275-285
  if (!ptr_self_->world().Map().Contains(location))
    return false;

  if (!b_check_terrain_type_)
    return true;

  const std::string str_terrain_type =
      ptr_self_->world().Map().GetTerrainInfo(location).Type;
  return std::find(info_.vec_allowed_terrain_types.begin(),
                   info_.vec_allowed_terrain_types.end(),
                   str_terrain_type) !=
         info_.vec_allowed_terrain_types.end();
}

bool GrantConditionOnDeploy::IsValidRampType(CPos location) {
  // L287-291
  if (info_.b_can_deploy_on_ramps)
    return true;

  const map::Map& map = ptr_self_->world().Map();
  return !map.Ramp().Contains(location) || map.Ramp().Get(location) == 0;
}

void GrantConditionOnDeploy::DeployInner(bool init) {
  // L277-302
  if (!init && deploy_state_ != DeployState::Undeployed)
    return;

  if (!IsValidTerrain(ptr_self_->Location()))
    return;

  // DeploySounds 的 LocalRandom 消耗保留(声音面省略)
  // The DeploySounds LocalRandom consumption is kept (the sound face
  // omitted).
  if (!info_.vec_deploy_sounds.empty())
    RandomOrDefaultOf(ptr_self_->world().LocalRandom(),
                      info_.vec_deploy_sounds);

  if (!init)
    OnDeployStarted();

  // notify 恒空 → 直接完成(上游 L293-294 真分支)
  // notify permanently empty → immediate completion (upstream's live
  // L293-294 branch).
  OnDeployCompleted();
}

void GrantConditionOnDeploy::UndeployInner(bool init) {
  // L303-326
  if (!init && deploy_state_ != DeployState::Deployed)
    return;

  if (!info_.vec_undeploy_sounds.empty())
    RandomOrDefaultOf(ptr_self_->world().LocalRandom(),
                      info_.vec_undeploy_sounds);

  if (!init)
    OnUndeployStarted();

  OnUndeployCompleted();
}

void GrantConditionOnDeploy::OnDeployStarted() {
  // L323-330
  if (int4_undeployed_token_ != sim::Actor::InvalidConditionToken)
    int4_undeployed_token_ =
        ptr_self_->RevokeCondition(int4_undeployed_token_);

  deploy_state_ = DeployState::Deploying;
}

void GrantConditionOnDeploy::OnDeployCompleted() {
  // L332-339
  if (int4_deployed_token_ == sim::Actor::InvalidConditionToken)
    int4_deployed_token_ =
        ptr_self_->GrantCondition(info_.str_deployed_condition);

  deploy_state_ = DeployState::Deployed;
}

void GrantConditionOnDeploy::OnUndeployStarted() {
  // L341-348:上游 UndeployStarted 置 Deploying(quirk 保真)
  // L341-348: upstream's UndeployStarted sets Deploying (the quirk is
  // kept).
  if (int4_deployed_token_ != sim::Actor::InvalidConditionToken)
    int4_deployed_token_ =
        ptr_self_->RevokeCondition(int4_deployed_token_);

  deploy_state_ = DeployState::Deploying;
}

void GrantConditionOnDeploy::OnUndeployCompleted() {
  // L350-356
  if (int4_undeployed_token_ == sim::Actor::InvalidConditionToken)
    int4_undeployed_token_ =
        ptr_self_->GrantCondition(info_.str_undeployed_condition);

  deploy_state_ = DeployState::Undeployed;
}

// ———— DeployForGrantedCondition / DeployInner / ToggleChargedCondition ————

DeployForGrantedCondition::DeployForGrantedCondition(
    sim::Actor& self, GrantConditionOnDeploy* deploy, bool moving)
    : ptr_deploy_{deploy}, b_moving_{moving} {
  // L26-31
  b_can_turn_ = self.Info()->HasTraitInfoOfInterface(
      "OpenRA.Traits.IFacingInfo");
}

void DeployForGrantedCondition::OnFirstRun(sim::Actor& self) {
  // L33-37:转向内联为 Turn 活动(WorldArena)
  // L33-37: the turn enqueues a Turn activity (WorldArena).
  if (ptr_deploy_->GetDeployState() == DeployState::Undeployed &&
      ptr_deploy_->Info().opt_facing.has_value() && b_can_turn_ &&
      !b_moving_)
    QueueChild(self.world().Arena().Create<activities::Turn>(
        self, *ptr_deploy_->Info().opt_facing));
}

bool DeployForGrantedCondition::Tick(sim::Actor& self) {
  // L39-51
  if (IsCanceling() ||
      (ptr_deploy_->GetDeployState() != DeployState::Deployed && b_moving_))
    return true;

  QueueChild(self.world().Arena().Create<DeployInner>(ptr_deploy_));
  return true;
}

DeployInner::DeployInner(GrantConditionOnDeploy* deployment)
    : ptr_deployment_{deployment} {
  // L67-73:部署动画一旦开始必须完成(不可中断)
  // L67-73: once the deploy animation starts it must finish (not
  // interruptible).
  b_is_interruptible_ = false;
}

bool DeployInner::Tick(sim::Actor&) {
  // L75-87
  if (ptr_deployment_->GetDeployState() == DeployState::Deploying ||
      ptr_deployment_->GetDeployState() == DeployState::Undeploying)
    return false;

  if (b_initiated_)
    return true;

  if (ptr_deployment_->GetDeployState() == DeployState::Undeployed)
    ptr_deployment_->Deploy();
  else
    ptr_deployment_->Undeploy();

  b_initiated_ = true;
  return false;
}

void ToggleChargedCondition::OnFirstRun(sim::Actor& self) {
  // L232-236
  if (ptr_toggle_->CanToggle())
    ptr_toggle_->ToggleState(self);
}

// ———— GrantChargedConditionOnToggle ————

void GrantChargedConditionOnToggle::InfoData::Validate() const {
  // RulesetLoaded L92-101(文本逐字)
  if (int4_charge_duration < 1)
    throw yaml::YamlException("ChargeDuration cannot be lower than 1.");
  if (int4_condition_duration < 1)
    throw yaml::YamlException("ConditionDuration cannot be lower than 1.");
}

GrantChargedConditionOnToggle::GrantChargedConditionOnToggle(InfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  // ctor L126-139
  ChargeTick =
      info_.int4_initial_charge < 0 ||
              info_.int4_initial_charge >= info_.int4_charge_duration
          ? info_.int4_charge_duration
          : info_.int4_initial_charge;

  int4_charge_threshold_ =
      info_.int4_charge_threshhold < 0 ||
              info_.int4_charge_threshhold > info_.int4_charge_duration
          ? info_.int4_charge_duration
          : info_.int4_charge_threshhold;
  int4_activated_charge_threshold_ =
      int4_charge_threshold_ * info_.int4_condition_duration /
      info_.int4_charge_duration;
}

GrantChargedConditionOnToggle::~GrantChargedConditionOnToggle() = default;

void GrantChargedConditionOnToggle::TraitDisabledHook(sim::Actor& self) {
  // L140-153
  if (b_is_active_)
    Deactivate(self);

  ChargeTick =
      info_.int4_initial_charge < 0 ||
              info_.int4_initial_charge > info_.int4_charge_duration
          ? info_.int4_charge_duration
          : info_.int4_initial_charge;
  if (int4_charged_token_ != sim::Actor::InvalidConditionToken)
    int4_charged_token_ = self.RevokeCondition(int4_charged_token_);
}

net::Order GrantChargedConditionOnToggle::IssueDeployOrder(sim::Actor& self,
                                                           bool queued) {
  // L161-164
  return net::Order{"ActivateCondition", &self, queued};
}

bool GrantChargedConditionOnToggle::CanIssueDeployOrder(sim::Actor&,
                                                        bool queued) {
  // L166
  return queued || CanToggle();
}

void GrantChargedConditionOnToggle::ResolveOrder(sim::Actor& self,
                                                 const net::Order& order) {
  // L173-183
  if (order.str_order_string != "ActivateCondition")
    return;

  if (order.b_queued || info_.b_cancels_current_activity)
    self.QueueActivity(order.b_queued,
                       self.world().Arena().Create<ToggleChargedCondition>(
                           self, this));
  else if (CanToggle())
    ToggleState(self);
}

bool GrantChargedConditionOnToggle::CanToggle() const {
  // L185
  return !ConditionalTraitCore::IsTraitDisabled() &&
         !ConditionalTraitCore::IsTraitPaused() &&
         ((!b_is_active_ && ChargeTick >= int4_charge_threshold_) ||
          (b_is_active_ && info_.b_can_cancel_condition));
}

void GrantChargedConditionOnToggle::ToggleState(sim::Actor& self) {
  // L187-201
  if (b_is_active_) {
    // 保留未用充电的比例
    // Keep the percentage of the unused charge.
    ChargeTick = ChargeTick * info_.int4_charge_duration /
                 info_.int4_condition_duration;
    Deactivate(self);
  } else {
    // 非满充激活时按比例扣减激活时长
    // If activated without full charge, subtract from the activated
    // duration.
    ChargeTick = ChargeTick * info_.int4_condition_duration /
                 info_.int4_charge_duration;
    Activate(self);
  }
}

void GrantChargedConditionOnToggle::Activate(sim::Actor& self) {
  // L203-212(声音面省略)
  if (int4_activated_token_ == sim::Actor::InvalidConditionToken)
    int4_activated_token_ =
        self.GrantCondition(info_.str_activated_condition);

  b_is_active_ = true;
}

void GrantChargedConditionOnToggle::Deactivate(sim::Actor& self) {
  // L214-222(声音面省略)
  if (int4_activated_token_ != sim::Actor::InvalidConditionToken)
    int4_activated_token_ = self.RevokeCondition(int4_activated_token_);

  b_is_active_ = false;
}

void GrantChargedConditionOnToggle::Tick(sim::Actor& self) {
  // L205-238
  if (ConditionalTraitCore::IsTraitDisabled() ||
      ConditionalTraitCore::IsTraitPaused())
    return;

  if (b_is_active_) {
    if (ChargeTick > 0)
      ChargeTick--;
    else
      Deactivate(self);
  } else {
    if (ChargeTick < info_.int4_charge_duration)
      ChargeTick++;
  }

  if (!info_.str_charged_condition.empty()) {
    if (ChargeTick < (b_is_active_ ? int4_activated_charge_threshold_
                                   : int4_charge_threshold_)) {
      if (int4_charged_token_ != sim::Actor::InvalidConditionToken)
        int4_charged_token_ = self.RevokeCondition(int4_charged_token_);
    } else {
      if (int4_charged_token_ == sim::Actor::InvalidConditionToken)
        int4_charged_token_ =
            self.GrantCondition(info_.str_charged_condition);
    }
  }
}

float GrantChargedConditionOnToggle::GetValue() {
  // L240-247
  if (ConditionalTraitCore::IsTraitDisabled())
    return 0.f;

  return b_is_active_
             ? static_cast<float>(ChargeTick) /
                   static_cast<float>(info_.int4_condition_duration)
             : static_cast<float>(ChargeTick) /
                   static_cast<float>(info_.int4_charge_duration);
}

core::Color GrantChargedConditionOnToggle::GetColor() {
  // L248
  return b_is_active_ ? info_.color_activated : info_.color_deactivated;
}

bool GrantChargedConditionOnToggle::DisplayWhenEmpty() const {
  // L249
  return info_.b_display_bar_when_empty;
}

// ———— GrantConditionOnLayer 族 ————

GrantConditionOnLayer::GrantConditionOnLayer(std::string str_condition,
                                             std::uint8_t valid_layer,
                                             sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional},
      uint1_valid_layer_type_{valid_layer},
      str_condition_{std::move(str_condition)} {}

GrantConditionOnLayer::~GrantConditionOnLayer() = default;

void GrantConditionOnLayer::CustomLayerChanged(sim::Actor& self,
                                               std::uint8_t old_layer,
                                               std::uint8_t new_layer) {
  // L30-33
  UpdateConditions(self, old_layer, new_layer);
}

void GrantConditionOnLayer::UpdateConditions(sim::Actor& self,
                                             std::uint8_t old_layer,
                                             std::uint8_t new_layer) {
  // L35-44
  if (new_layer == uint1_valid_layer_type_ &&
      old_layer != uint1_valid_layer_type_ &&
      int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
  else if (new_layer != uint1_valid_layer_type_ &&
           old_layer == uint1_valid_layer_type_ &&
           int4_condition_token_ != sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

void GrantConditionOnLayer::TraitEnabledHook(sim::Actor& self) {
  // L46-50
  if (self.Location().Layer() == uint1_valid_layer_type_ &&
      int4_condition_token_ == sim::Actor::InvalidConditionToken)
    int4_condition_token_ = self.GrantCondition(str_condition_);
}

void GrantConditionOnLayer::TraitDisabledHook(sim::Actor& self) {
  // L52-56
  if (int4_condition_token_ == sim::Actor::InvalidConditionToken)
    return;

  int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

}  // namespace ora::mods
