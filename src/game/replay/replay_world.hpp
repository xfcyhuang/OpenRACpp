#pragma once

import std;

#include "game/support/mersenne_twister.hpp"

namespace ora::game::replay {

// —— ore-lord 黄金世界的重演面(T17/issue #18)——
//
// 口径:golden_gen replay 模式的 oracle 世界 = ore-lord 官方图 + 全量 ra 规则(21 渲染
// trait 剥离)+ 注入 5 actor(fact/proc/barr/e1/e1)+ mcv(SpawnStartingUnits),seed 20261103,
// NetFrameInterval=1,580 帧(调研 t16-t17 §3.2 配方)。本模块按同一装配序重建世界的
// ISync 面(SyncHash 五段式的段2 贡献面),oracle 侧字段级规格见
// .ora-explore/t17-analyze/out/{isync-init,isync-born,pertrail-*}.txt(探针反射 dump)。
//
// 段2 逐 trait 哈希规则(Sync.cs GenerateHashFunc,行为级复刻):
//   trait hash = XOR(成员哈希);成员哈希:int 直值、bool→0/1(bool 死代码怪癖)、
//   CPos→Bits=(x<<20)|(y<<8)|layer、WPos→X^Y^Z、WAngle→Angle、Actor→(ActorID<<16)
//   (null→0)、Player→((PlayerActorID<<16)*0x567 环绕)。
// 段5:单人局 DeveloperMode.Enabled=True → 全玩家 UnlockedRenderPlayer
//   (DeveloperMode.cs:159-161 NonBotPlayers.Count()==1)→ Σ HashPlayer 恒定。

// ISync trait 面(上游 trait 类型名的重演种类;枚举序即各类型内的构造序标记)
enum class FaceTraitKind : std::uint8_t {
  kDebugPauseState = 0,
  kShroud,
  kPlayerResources,
  kMissionObjectives,
  kDeveloperMode,
  kGpsWatcher,
  kPlayerExperience,
  kClassicProductionQueue,
  kPowerManager,
  kFrozenActorLayer,
  kImmobile,
  kBodyOrientation,
  kClassicFacingBodyOrientation,
  kBuilding,
  kHealth,
  kRevealsShroud,
  kFrozenUnderFog,
  kRepairableBuilding,
  kConyardChronoReturn,
  kDockHost,
  kGainsExperience,
  kMobile,
  kChangesHealth,
  kTakeCover,
  kAttackFrontal,
  kAutoTarget,
  kChronoshiftable,
  kStoresResources,
  kHarvester,
};

struct FaceTypeSpec {
  std::string_view str_type;
  std::int32_t int4_hp{};          // Health.HP 初值(0=无 Health trait)
  std::int32_t int4_facings{};     // BodyOrientation.QuantizedFacings
  std::uint8_t u1_flags{};         // bit0: 有 RevealsShroud;bit1: 有 FrozenUnderFog
  std::vector<FaceTraitKind> vec_traits;  // ISync trait 构造序
};

// 类型面表(oracle isync-init/born dump 同源;26+3 种)
[[nodiscard]] const FaceTypeSpec* face_type_find(std::string_view str_type);

// —— 重演世界的 actor / player 运行期状态 ——

constexpr std::int32_t kNoAggressor{-1};

struct FaceActor {
  std::uint32_t u4_id{};
  std::string str_type;
  const FaceTypeSpec* ptr_face{};
  std::uint32_t u4_owner{};  // player 索引(world/player actor 的 owner)
  std::int32_t int4_x{}, int4_y{};
  bool b_in_world{true};
  bool b_disposed{};

  // —— 运行期 ISync 成员(变化字段;39 个变化字段的载体)——
  std::int32_t int4_hp{};                  // Health.HP
  std::int32_t int4_facing{};              // Mobile.Facing(WAngle)
  std::int32_t int4_from_x{}, int4_from_y{}, int4_to_x{}, int4_to_y{};  // Mobile.From/ToCell
  std::int32_t int4_px{}, int4_py{}, int4_pz{};  // Mobile.CenterPosition(WPos)
  std::int32_t int4_next_scan{};           // AutoTarget.nextScanTime
  std::int32_t int4_aggressor{kNoAggressor};  // AutoTarget.Aggressor(Actor id;-1 null)
  std::int32_t int4_take_cover{};          // TakeCover.remainingDuration
  bool b_aiming{};                         // AttackFrontal.IsAiming
  std::int32_t int4_experience{}, int4_level{};  // GainsExperience
  std::int32_t int4_frozen_vis{};          // FrozenUnderFog.VisibilityHash(只增位包)
  std::int32_t int4_content_hash{};        // harv StoresResources.ContentHash
  std::int32_t int4_unload_ticks{};        // harv Harvester.currentUnloadTicks
  std::int32_t int4_ch_ticks[2]{};         // ChangesHealth.ticks ×2 实例
  std::int32_t int4_ch_damage[2]{};        // ChangesHealth.damageTicks ×2

  // —— 非同步但驱动 RNG/行为的状态(帧内序见 .ora-explore/t17-oracle-face.md)——
  bool b_activity{};                       // 有排队活动(busy:A 段不跑 TickIdle)
  int wib_state{0};                        // WithInfantryBody 主实例 state(0 Waiting/1 Idle/2 Moving)
  std::int32_t int4_wib_delay{};           // WithInfantryBody.idleDelay(Next(30,110) 产物)
  bool b_wib_quirk{};                      // 首 tick CurrentMovementTypes 误含 Horizontal 的一次性怪癖
  std::int32_t int4_reload{};              // Armament ReloadDelay 余量(开火后 20,每帧 B 段递减)
  std::int32_t int4_activity_target{-1};   // 活动目标(非同步;Aggressor 仅受击还击才置,见 AutoTarget.cs)
};

struct FacePlayer {
  std::string str_name;
  std::uint32_t u4_actor_id{};
  std::int32_t int4_cash{5000};         // PlayerResources.Cash(lobby StartingCash)
  std::int32_t int4_resources{};        // PlayerResources.Resources
  std::int32_t int4_capacity{};         // PlayerResources.ResourceCapacity
  std::int32_t int4_pexp{};             // PlayerExperience.Experience
  std::int32_t int4_power_provided{};   // PowerManager.PowerProvided
  std::int32_t int4_power_drained{};    // PowerManager.PowerDrained
  bool b_queue_enabled[6]{true, true, true, true, true, true};   // ClassicProductionQueue.Enabled
  bool b_queue_faction[6]{true, true, true, true, true, true};   // IsValidFaction
  std::int32_t int4_frozen_vis{};       // FrozenActorLayer.VisibilityHash(Σ雾中 frozen id)
  std::int32_t int4_frozen_sum{};       // FrozenActorLayer.FrozenHash(Σ全部 frozen id)
};

struct ReplayOracleWorld {
  std::vector<FaceActor> vec_actors;  // 下标 = ActorID(装配序 0..148 + 运行期出生)
  std::vector<FacePlayer> vec_players;
  support::MersenneTwister rng_shared;
  // 每玩家已探索格集(Shroud;f1 置位波建立,移动/出生扩展——扫描可见性与 frozen 波共用)
  std::vector<std::set<std::pair<std::int32_t, std::int32_t>>> vec_sight;
  // 延迟开火特效日程(命中帧 +2/+4 各耗 4;值不入对拍面,时点入 seg4 账本)
  std::vector<std::pair<std::int32_t, std::int32_t>> vec_delayed_effect;  // (frame, count)
  // 段3:ISync 效果(Bullet 弹道)——战斗期非空
  struct SyncEffect {
    std::int32_t int4_hash{};  // Bullet 的 ISync 哈希(弹道状态)
  };
  std::vector<SyncEffect> vec_sync_effects;
};

// 期望装配序断言用:ore-lord + 注入表 + mcv 的 149 个初始 actor(id:type:owner)。
[[nodiscard]] const std::vector<std::string>& replay_oracle_init_manifest();

// 装配(oracle BuildWorld + LoadComplete 子集的行为等价):
//   ActorID 0=world,1..4=player(Neutral/Creeps/Multi0/Everyone),5..142=地图 actor
//   (map.yaml Actors 节点序),143..147=注入表,148=mcv(首个 mpspawn 处)。
//   RNG:装配期 4 个 SharedRandom 消耗(SpawnMapActors 内,来源未逐 trait 定位——
//   按确定性常量预推进,登记 docs/COVERAGE.md)。
[[nodiscard]] std::expected<ReplayOracleWorld, std::string> replay_oracle_create(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap);

// f1 行为(LoadComplete 后首帧的确定性事件;oracle isync-changes/栈级插桩实测):
//   A 段(逐 actor 活动与 idle TickIdle):e1 146 WithInfantryBody pair(2 消耗)+ 首扫
//   (1 消耗,shroud 未探索 → 无目标)、e1 147 同构;B 段(ITick):矿 SeedsResource
//   RandomWalk ×28(全量世界;no-inject 26,建筑足迹 +2——RandomWalk 复刻为子面 4)
//   → 帧尾:proc FreeActor harv 出生(id 149,facing 256@(34,7))。shroud:置位波
//   (frozen 集可见判定)+ FrozenActorLayer 4 玩家聚合 + 队列 Enabled 波。
//   重置值口径:扫描 raw = 3 + out%5,**同帧 B 段 ITick 再递减一次**(终值 = raw-1)。
void replay_oracle_frame1(ReplayOracleWorld& w);

// 通用单帧(n ≥ 2):A 段(活动:e1 攻击链;idle TickIdle:WIB 状态机/AutoTarget 扫描)
// + B 段(ITick 递减:AutoTarget/TakeCover/WIB 状态迁移/harv 转身)+ 帧尾(伤害结算)。
// 逐帧与录包 sync(N+1) 对拍(录包口径:sync(N) = N-1 帧末状态)。
void replay_oracle_frame(ReplayOracleWorld& w, std::int32_t int4_frame);

// World.SyncHash() 五段式(int 环绕;n 跨段连续;段2 按 (ActorID,构造序))。
[[nodiscard]] std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w);

// 段2 调试 dump:每 (actor, trait) 的 hash 与贡献(探针 pertrait-*.txt 同格式)。
[[nodiscard]] std::string replay_oracle_pertrait_dump(const ReplayOracleWorld& w);

}  // namespace ora::game::replay
