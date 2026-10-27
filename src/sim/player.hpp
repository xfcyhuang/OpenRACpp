#pragma once
import std;

#include "core/color.hpp"
#include "core/long_bitset.hpp"
#include "map/map_players.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {

class Actor;
class Shroud;
class FrozenActorLayer;
class World;

enum class PowerState : std::int32_t { Normal = 1, Low = 2, Critical = 4 };

struct FactionInfoData {
  std::string Name;
  std::string InternalName;
  std::vector<std::string> RandomFactionMembers;
  std::string Side;
  std::string Description;
  bool Selectable = true;

  static FactionInfoData Parse(const meta::RecordObject& rec_info);
};

class Player final {
 public:

  Player(World& world, const net::SessionClient* client,
         const map::PlayerReference& pr, MersenneTwister& player_random);

  Player(std::string str_player_name, std::string str_internal_name,
         int client_index, bool playable = true)
      : str_player_name_{std::move(str_player_name)},
        str_internal_name_{std::move(str_internal_name)},
        int4_client_index_{client_index},
        b_playable_{playable} {
    const std::string* name = &str_internal_name_;
    PlayerMask = PlayerMaskSet{std::span<const std::string>{name, 1}};
  }

  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  Actor* PlayerActor() const { return p_player_actor_; }
  const std::string& PlayerName() const { return str_player_name_; }
  const std::string& InternalName() const { return str_internal_name_; }
  const FactionInfoData& Faction() const { return faction_; }
  bool NonCombatant() const { return b_non_combatant_; }
  bool Playable() const { return b_playable_; }
  int ClientIndex() const { return int4_client_index_; }
  const CPos& HomeLocation() const { return cpos_home_location_; }
  int Handicap() const { return int4_handicap_; }
  const map::PlayerReference& PlayerReference() const { return pr_; }
  bool IsBot() const { return b_is_bot_; }
  const std::string& BotType() const { return str_bot_type_; }

  Shroud* GetShroud() const;
  FrozenActorLayer* GetFrozenActorLayer() const;

  const core::Color& Color() const { return color_; }
  void SetColor(core::Color c) { color_ = c; }

  static const core::Color& GetColor(const Player* p) { return p->color_; }

  const FactionInfoData& DisplayFaction() const { return display_faction_; }
  int SpawnPoint() const { return int4_spawn_point_; }
  int DisplaySpawnPoint() const { return int4_display_spawn_point_; }

  WinState PlayerWinState() const { return win_state_; }
  void SetWinState(WinState s) { win_state_ = s; }
  bool HasObjectives() const { return b_has_objectives_; }
  void SetHasObjectives(bool b) { b_has_objectives_ = b; }

  bool Spectating() const {
    return !b_in_mission_map_ &&
           (b_spectating_ || win_state_ != WinState::Undefined);
  }

  World& GetWorld() const { return *world_; }

  World* world_ptr() const { return world_; }

  PlayerMaskSet PlayerMask;
  PlayerMaskSet AlliedPlayersMask;
  PlayerMaskSet EnemyPlayersMask;

  bool UnlockedRenderPlayer() const;

  const std::string& ResolvedPlayerName();

  static FactionInfoData ResolveFaction(
      const std::string& str_faction_name,
      std::span<const FactionInfoData> faction_infos,
      MersenneTwister& player_random, bool require_selectable = true);

  std::string ToString();

  PlayerRelationship RelationshipWith(const Player* other) const;

  bool IsAlliedWith(const Player* p) const {
    return RelationshipWith(p) == PlayerRelationship::Ally;
  }

  void PlayerDisconnected(Player& p);

  const std::vector<IUnlocksRenderPlayer*>& UnlockRenderPlayer() const {
    return vec_unlock_render_player_;
  }
  const std::vector<INotifyPlayerDisconnected*>& NotifyDisconnected() const {
    return vec_notify_disconnected_;
  }

 private:

  std::string ResolvePlayerName();

  World* world_ = nullptr;

  Actor* p_player_actor_ = nullptr;
  std::string str_player_name_;
  std::string str_internal_name_;
  FactionInfoData faction_;
  bool b_non_combatant_ = false;
  bool b_playable_ = true;
  int int4_client_index_ = 0;
  CPos cpos_home_location_{CPos::Zero()};
  int int4_handicap_ = 0;
  map::PlayerReference pr_;
  bool b_is_bot_ = false;
  std::string str_bot_type_;
  mutable Shroud* p_shroud_ = nullptr;

  mutable class FrozenActorLayer* p_frozen_actor_layer_ =
      nullptr;
  core::Color color_{core::Color::FromArgbRaw(0xFF4B4B4B)};
  FactionInfoData display_faction_;
  int int4_spawn_point_ = 0;
  int int4_display_spawn_point_ = 0;
  WinState win_state_ = WinState::Undefined;
  bool b_has_objectives_ = false;
  bool b_in_mission_map_ = false;
  bool b_spectating_ = false;

  std::vector<IUnlocksRenderPlayer*> vec_unlock_render_player_;
  std::vector<INotifyPlayerDisconnected*> vec_notify_disconnected_;
  std::vector<const meta::RecordObject*> vec_bot_infos_;
  std::string str_resolved_player_name_;
};

}

namespace ora::sim::sync {

inline int HashPlayer(const Player* p) {
  if (p != nullptr && p->PlayerActor() != nullptr)
    return static_cast<int>(p->PlayerActor()->ActorID() << 16) * 0x567;
  return 0;
}

}
