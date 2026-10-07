// UPSTREAM: OpenRA.Game/Player.cs @b6fc03f L27-337(逐语义重写;Shroud/
//          FrozenActorLayer 面随该批、Fluent 本地化随 Phase 6、Lua 随 Phase 8)
//          Verbatim-semantics rewrite (the Shroud/FrozenActorLayer faces land
//          with those batches, Fluent localization with Phase 6, Lua with
//          Phase 8).
//
// 机制对照 / Mechanism mapping:
//  - PlayerBitMask 位身份:上游 LongBitSet<PlayerBitMask>(Player.cs L37/91-93)
//    → core/long_bitset.hpp 的全量位集(分配器进程级;World ctor 的
//    LongBitSet<PlayerBitMask>.Reset() 锚点在 World 构造序保留)
//    The PlayerBitMask bit identity: upstream LongBitSet<PlayerBitMask> →
//    the full bit-set in core/long_bitset.hpp (the process-level allocator;
//    the World ctor's Reset() anchor keeps its place in the World
//    construction order).
//  - Faction/DisplayFaction(Info 类引用):Info 走值袋(无运行时类)→
//    解析后的 FactionInfoData 值(字段面 Name/InternalName/RandomFactionMembers/
//    Selectable/Side/Description;解析 = 生成记录的按名槽位读)
//    Faction/DisplayFaction (Info-class references): the Info rides the
//    value bag (no runtime class) → a parsed FactionInfoData value (the
//    field face; parsing = by-name slot reads over the generated record).
//  - PlayerActor 的构造怪癖保留(L204-211):先赋"未初始化 actor"再手动
//    Initialize(true),使 INotifyCreated.Created 查询 Player traits 不崩
//    The PlayerActor construction quirk kept (L204-211): assign the
//    uninitialized actor first, then Initialize(true) by hand, so
//    INotifyCreated.Created can query player traits without crashing.
//  - Shroud(L213)/FrozenActorLayer(L214):Trait<Shroud>() 的解析面随
//    Shroud 批接入 —— 部分覆盖装配面(COVERAGE 登记;上游缺 Shroud 时
//    此处即抛,消费面本批为空)
//    Shroud (L213) / FrozenActorLayer (L214): the resolution faces land
//    with the Shroud batch — the partial-coverage assembly face (in
//    COVERAGE; upstream throws here without Shroud, and this batch has no
//    consumers yet).
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
class FrozenActorLayer;  // L57 的懒解析面(the L57 lazy-resolve face)
class World;

/// Player.cs L35:WinState 与 L37 的 PlayerBitMask 标签权威定义在
/// trait_interfaces.hpp(ICrushable 的掩码面共用;见其头注)
/// The authoritative Player.cs L35 WinState + L37 PlayerBitMask tag
/// definitions live in trait_interfaces.hpp (shared with ICrushable's
/// mask face; see its header).

/// PowerState(Player.cs L28-33) | PowerState (Player.cs L28-33).
enum class PowerState : std::int32_t { Normal = 1, Low = 2, Critical = 4 };

/// FactionInfo 的解析值面(Player.cs L47/65 的 FactionInfo 引用承载;字段 =
/// Faction.cs 的六个只读字段,RandomFactionMembers 的 FrozenSet → 插入序
/// vector)
/// The parsed-value face of FactionInfo (carrying Player.cs L47/65's
/// FactionInfo references; the fields are Faction.cs's six readonly fields,
/// RandomFactionMembers' FrozenSet → an insertion-ordered vector).
struct FactionInfoData {
  std::string Name;                                 // Faction.cs L22
  std::string InternalName;                         // L25
  std::vector<std::string> RandomFactionMembers;    // L28
  std::string Side;                                 // L31
  std::string Description;                          // L35
  bool Selectable = true;                           // L37

  /// 生成记录值袋 → 解析值(FactionInfo 字段按名槽位读)
  /// The generated-record bag → the parsed value (by-name slot reads).
  static FactionInfoData Parse(const meta::RecordObject& rec_info);
};

/// Player(Player.cs L39-337) | Player (Player.cs L39-337).
class Player final {
 public:
  /// ctor(L152-228):client 空 = 地图玩家分支 | the ctor (L152-228); a
  /// null client is the map-player branch.
  Player(World& world, const net::SessionClient* client,
         const map::PlayerReference& pr, MersenneTwister& player_random);

  /// 测试构造(无世界面;sound_test 等的注入面 —— 同 WorldSimParams
  /// 先例,上游无此形态)
  /// The test ctor (no world face; the injection face of sound_test and
  /// friends — the WorldSimParams precedent; no upstream counterpart).
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

  Actor* PlayerActor() const { return p_player_actor_; }                  // L44
  const std::string& PlayerName() const { return str_player_name_; }      // L45
  const std::string& InternalName() const { return str_internal_name_; }  // L46
  const FactionInfoData& Faction() const { return faction_; }             // L47
  bool NonCombatant() const { return b_non_combatant_; }                  // L48
  bool Playable() const { return b_playable_; }                           // L49
  int ClientIndex() const { return int4_client_index_; }                  // L50
  const CPos& HomeLocation() const { return cpos_home_location_; }        // L51
  int Handicap() const { return int4_handicap_; }                         // L52
  const map::PlayerReference& PlayerReference() const { return pr_; }     // L53
  bool IsBot() const { return b_is_bot_; }                                // L54
  const std::string& BotType() const { return str_bot_type_; }            // L55

  /// L56/L57:Shroud/FrozenActorLayer 解析面(懒解析 = 上游 L213-214 的
  /// Owner.TraitOrDefault<T>;player actor 缺 trait 时 null,实现于
  /// player.cpp —— 模板查询需 World 完整类型)
  /// L56/L57: the Shroud/FrozenActorLayer resolution faces (the lazy
  /// resolve = upstream L213-214's Owner.TraitOrDefault<T>; null when the
  /// player actor lacks the trait, implemented in player.cpp — the
  /// template query needs the complete World type).
  Shroud* GetShroud() const;
  FrozenActorLayer* GetFrozenActorLayer() const;

  /// L59 的 color 私有字段 + L62 的 Color 关系色属性 | the private color
  /// (L59) + the relationship-color property (L62).
  const core::Color& Color() const { return color_; }
  void SetColor(core::Color c) { color_ = c; }
  /// GetColor(L274):关系色无关的原始色 | GetColor (L274): the raw color.
  static const core::Color& GetColor(const Player* p) { return p->color_; }

  const FactionInfoData& DisplayFaction() const { return display_faction_; }  // L65
  int SpawnPoint() const { return int4_spawn_point_; }                        // L68
  int DisplaySpawnPoint() const { return int4_display_spawn_point_; }         // L71

  WinState PlayerWinState() const { return win_state_; }                    // L73
  void SetWinState(WinState s) { win_state_ = s; }
  bool HasObjectives() const { return b_has_objectives_; }                  // L74
  void SetHasObjectives(bool b) { b_has_objectives_ = b; }

  /// Spectating(L77) | Spectating (L77).
  bool Spectating() const {
    return !b_in_mission_map_ &&
           (b_spectating_ || win_state_ != WinState::Undefined);
  }

  World& GetWorld() const { return *world_; }  // L79(测试构造面下可空 ——
                                               // 指针承载;上游 get;即此)
                                               // (nullable under the test
                                               // ctor; the pointer carries it.)

  /// PlayerMask 族(L91-93) | the mask family (L91-93).
  PlayerMaskSet PlayerMask;          // public 字段(上游同名)| the public field.
  PlayerMaskSet AlliedPlayersMask;   // L92
  PlayerMaskSet EnemyPlayersMask;    // L93

  /// UnlockedRenderPlayer(L95-104) | UnlockedRenderPlayer (L95-104).
  bool UnlockedRenderPlayer() const;

  /// ResolvedPlayerName(L107-114;bot 枚举名的 Fluent 面随 Phase 6 ——
  /// 原文名直返为 COVERAGE 登记偏离)
  /// ResolvedPlayerName (L107-114; the enumerated-bot-name Fluent face
  /// lands with Phase 6 — returning the raw name is the registered
  /// COVERAGE deviation).
  const std::string& ResolvedPlayerName();

  /// ResolveFaction(L116-137) | ResolveFaction (L116-137).
  static FactionInfoData ResolveFaction(
      const std::string& str_faction_name,
      std::span<const FactionInfoData> faction_infos,
      MersenneTwister& player_random, bool require_selectable = true);

  /// ToString(L230-233) | ToString (L230-233).
  std::string ToString();

  /// RelationshipWith(L249-265) | RelationshipWith (L249-265).
  PlayerRelationship RelationshipWith(const Player* other) const;
  /// IsAlliedWith(L268-271) | IsAlliedWith (L268-271).
  bool IsAlliedWith(const Player* p) const {
    return RelationshipWith(p) == PlayerRelationship::Ally;
  }

  /// PlayerDisconnected(L302-306) | PlayerDisconnected (L302-306).
  void PlayerDisconnected(Player& p);

  /// L83-84 的构造期缓存 | the construction-time caches (L83-84).
  const std::vector<IUnlocksRenderPlayer*>& UnlockRenderPlayer() const {
    return vec_unlock_render_player_;
  }
  const std::vector<INotifyPlayerDisconnected*>& NotifyDisconnected() const {
    return vec_notify_disconnected_;
  }

 private:
  /// ResolvePlayerName(L235-247) | ResolvePlayerName (L235-247).
  std::string ResolvePlayerName();

  World* world_ = nullptr;  // 引用语义(测试构造面 = null)| reference
                            // semantics (null under the test ctor).
  Actor* p_player_actor_ = nullptr;   // L44
  std::string str_player_name_;       // L45
  std::string str_internal_name_;     // L46
  FactionInfoData faction_;           // L47
  bool b_non_combatant_ = false;      // L48
  bool b_playable_ = true;            // L49
  int int4_client_index_ = 0;         // L50
  CPos cpos_home_location_{CPos::Zero()};  // L51
  int int4_handicap_ = 0;             // L52
  map::PlayerReference pr_;           // L53
  bool b_is_bot_ = false;             // L54
  std::string str_bot_type_;          // L55(null 承载为空串 | null as "")
  mutable Shroud* p_shroud_ = nullptr;  // L56(懒解析缓存;const 查询
                                         // 首解析 —— mutable | the
                                         // lazy-resolve cache, first
                                         // resolved under a const query)
  mutable class FrozenActorLayer* p_frozen_actor_layer_ =
      nullptr;  // L57(同上 | ditto)
  core::Color color_{core::Color::FromArgbRaw(0xFF4B4B4B)};  // L59
  FactionInfoData display_faction_;   // L65
  int int4_spawn_point_ = 0;          // L68
  int int4_display_spawn_point_ = 0;  // L71
  WinState win_state_ = WinState::Undefined;  // L73
  bool b_has_objectives_ = false;     // L74
  bool b_in_mission_map_ = false;     // L81
  bool b_spectating_ = false;         // L82

  std::vector<IUnlocksRenderPlayer*> vec_unlock_render_player_;          // L83
  std::vector<INotifyPlayerDisconnected*> vec_notify_disconnected_;      // L84
  std::vector<const meta::RecordObject*> vec_bot_infos_;                 // L86
  std::string str_resolved_player_name_;                                 // L87
};

}  // namespace ora::sim

namespace ora::sim::sync {

/// HashPlayer(Sync.cs L131-136) | HashPlayer (Sync.cs L131-136).
inline int HashPlayer(const Player* p) {
  if (p != nullptr && p->PlayerActor() != nullptr)
    return static_cast<int>(p->PlayerActor()->ActorID() << 16) * 0x567;
  return 0;
}

}  // namespace ora::sim::sync
