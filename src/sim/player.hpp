// UPSTREAM: OpenRA.Game/Player.cs @b6fc03f L27-337(仿真核心子集;Shroud/
//          FrozenActorLayer/FactionInfo/Scripting 面 Phase 5/8 落地)
//          The sim-core subset of Player.cs (Shroud/FrozenActorLayer/
//          FactionInfo/Scripting surfaces land in Phase 5/8).
//
// 机制对照 / Mechanism mapping:
//  - SyncHash/Order 反序列化所需最小面:PlayerActor/PlayerName/InternalName/
//    WinState/UnlockedRenderPlayer/PlayerMask;完整构造(ModData 玩家创建链)
//    Phase 5 随 ICreatePlayers 落地
//    The minimal surface needed by SyncHash / order deserialization:
//    PlayerActor/PlayerName/InternalName/WinState/UnlockedRenderPlayer/
//    PlayerMask; the full construction (the ModData player-creation chain)
//    lands with ICreatePlayers in Phase 5.
//  - UnlockedRenderPlayer(L95-104):IUnlocksRenderPlayer trait 族 + 战局
//    判定;Phase 3 恒走 WinState 分支(trait 面空)—— COVERAGE 登记
//    UnlockedRenderPlayer (L95-104): the IUnlocksRenderPlayer trait family
//    plus the outcome check; Phase 3 always takes the WinState branch (the
//    trait face is empty) — registered in COVERAGE.
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;
class World;

/// PlayerBitMask 类型标签(Player.cs L37;LongBitSet 槽位 —— Phase 5 全量
/// LongBitSet 实现接入,本期以 uint64 承载 ≤64 玩家)
/// The PlayerBitMask type tag (Player.cs L37; the LongBitSet slot — the
/// full LongBitSet lands in Phase 5, carried as uint64 for ≤64 players
/// meanwhile).
class PlayerBitMask {};

/// Player(Player.cs L39-337;仿真核心子集)
/// Player (Player.cs L39-337; the sim-core subset).
class Player final {
 public:
  Player(Actor* player_actor, std::string str_player_name,
         std::string str_internal_name, int client_index,
         bool b_playable = true)
      : p_player_actor_(player_actor),
        str_player_name_(std::move(str_player_name)),
        str_internal_name_(std::move(str_internal_name)),
        int4_client_index_(client_index),
        b_playable_(b_playable) {}

  Actor* PlayerActor() const { return p_player_actor_; }
  const std::string& PlayerName() const { return str_player_name_; }
  const std::string& InternalName() const { return str_internal_name_; }
  int ClientIndex() const { return int4_client_index_; }
  bool Playable() const { return b_playable_; }

  WinState PlayerWinState() const { return win_state_; }
  void SetWinState(WinState s) { win_state_ = s; }

  /// L95-104(见头注偏离)
  bool UnlockedRenderPlayer() const {
    return win_state_ != WinState::Undefined && !b_in_mission_map_;
  }

  std::uint64_t PlayerMask() const { return uint8_player_mask_; }
  void SetPlayerMask(std::uint64_t m) { uint8_player_mask_ = m; }

 private:
  Actor* p_player_actor_ = nullptr;
  std::string str_player_name_;
  std::string str_internal_name_;
  int int4_client_index_ = 0;
  bool b_playable_ = true;
  bool b_in_mission_map_ = false;  // inMissionMap(任务地图锁定;Phase 5 地图链)
  WinState win_state_ = WinState::Undefined;
  std::uint64_t uint8_player_mask_ = 0;
};

}  // namespace ora::sim

namespace ora::sim::sync {

/// HashPlayer(Sync.cs L131-136)
inline int HashPlayer(const Player* p) {
  if (p != nullptr && p->PlayerActor() != nullptr)
    return static_cast<int>(p->PlayerActor()->ActorID() << 16) * 0x567;
  return 0;
}

}  // namespace ora::sim::sync
