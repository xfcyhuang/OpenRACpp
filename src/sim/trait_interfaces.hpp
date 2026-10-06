// UPSTREAM: OpenRA.Game/Traits/TraitsInterfaces.cs @b6fc03f L29-666
//          (仿真核心接口子集逐语义重写;渲染/调色板/UI 接口随 Phase 4/6 落地)
//          Verbatim-semantics rewrite of the sim-core interface subset;
//          render/palette/UI interfaces land with Phase 4/6.
//
// 机制对照 / Mechanism mapping:
//  - C# interface 查询键(Type 对象)→ gen::TypeId(gen/interfaces_gen.h 集中
//    分配,schema_dumper 导出,手写禁改;PORTING_PLAN §4.1)。每个接口绑定
//    kTypeId;TraitDictionary/AddTrait 的 GetInterfaces()+BaseTypes() 注册面
//    → 手写 trait 类的 ORA_TRAIT_INTERFACES 上行转换表(编译期生成,
//    多继承子对象地址由 static_cast lambda 承载)
//    The C# interface query key (a Type object) → gen::TypeId (centrally
//    allocated in gen/interfaces_gen.h, exported by schema_dumper, hand
//    edits forbidden; PORTING_PLAN §4.1). Every interface binds kTypeId;
//    the GetInterfaces()+BaseTypes() registration surface of
//    TraitDictionary/AddTrait → the ORA_TRAIT_INTERFACES upcast table of
//    hand-written trait classes (generated at compile time; multiple-
//    inheritance sub-object addresses carried by static_cast lambdas).
//  - delegate VariableObserverNotifier → std::function(规范:严禁函数指针);
//    IReadOnlyDictionary<string,int> → const std::map<std::string,int>&
//    (SortedDictionary→std::map 的既有映射;上游此处是只读视图)
//    delegate VariableObserverNotifier → std::function (spec: function
//    pointers forbidden); IReadOnlyDictionary<string,int> →
//    const std::map<std::string,int>& (the established
//    SortedDictionary→std::map mapping; upstream passes a read-only view).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/cell_pos.hpp"
#include "core/long_bitset.hpp"
#include "core/mersenne_twister.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "gen/interfaces_gen.h"
#include "yaml/mini_yaml.hpp"

namespace ora::mods::pathfinding {
class LocomotorInfo;  // Mods.Common;寻路批的类型面(a pathfinding-batch type)
}

namespace ora::gfx {
class WorldRenderer;  // 渲染域注入面(Phase 4)| the render-domain injection face.
}  // namespace ora::gfx

namespace ora::net {
struct Order;
class OrderManager;
struct SessionClient;
}  // namespace ora::net

namespace ora::sim {

class Actor;
class Player;
class World;
class Activity;
struct Target;

/// Player.cs L37 的位标签 + LongBitSet 全量位集(core/long_bitset.hpp)
/// The Player.cs L37 bit tag + the full LongBitSet (core/long_bitset.hpp).
/// (置于本处以解 player.hpp 的循环包含 | placed here to break the
/// player.hpp include cycle)
class PlayerBitMask {};

using PlayerMaskSet = LongBitSet<PlayerBitMask>;

/// core::BitSet 的标签类型(TargetableType/DamageType/CrushClass ——
/// TraitsInterfaces.cs L46/L530/L647 的类型标签;BitSet 模板参数)
/// Type tags for core::BitSet (TargetableType/DamageType/CrushClass —
/// TraitsInterfaces.cs L46/L530/L647; BitSet template arguments).
class TargetableType {};
class DamageType {};
class CrushClass {};

// ———— 枚举(TraitsInterfaces.cs / Player.cs / Map.cs 的枚举面)————
// ———— Enums (the enum surface of TraitsInterfaces.cs / Player.cs / Map.cs) ————

/// TraitsInterfaces.cs L32-41
enum class DamageState : std::int32_t {
  Undamaged = 1,
  Light = 2,
  Medium = 4,
  Heavy = 8,
  Critical = 16,
  Dead = 32,
};

/// TraitsInterfaces.cs L65-72
enum class PlayerRelationship : std::int32_t {
  None = 0,
  Enemy = 1,
  Neutral = 2,
  Ally = 4,
};

inline bool HasRelationship(PlayerRelationship r, PlayerRelationship rel) {
  // PERF: Enum.HasFlag is slower and requires allocations. (上游注释语义)
  return (static_cast<std::int32_t>(r) & static_cast<std::int32_t>(rel)) ==
         static_cast<std::int32_t>(rel);
}

/// TraitsInterfaces.cs L322:SubCell : byte(cell 域常量,权威定义在
/// core/cell_pos.hpp —— Phase 5 地图层共用;此处保留 sim 命名空间别名)
/// TraitsInterfaces.cs L322: SubCell : byte (a cell-domain constant whose
/// authoritative definition lives in core/cell_pos.hpp — shared with the
/// map layer from Phase 5; the sim-namespace alias stays).
using SubCell = ora::SubCell;

/// Player.cs L35:WinState
enum class WinState : std::int32_t { Undefined, Won, Lost };

// ———— 条件系统观察者(TraitsInterfaces.cs L630-638)————
// ———— Condition-system observers (TraitsInterfaces.cs L630-638) ————

/// 条件计数快照的只读视图形态(Actor.conditionCache)
/// The read-only view shape of the condition-count snapshot
/// (Actor.conditionCache).
using ConditionCacheView = const std::map<std::string, int>&;

using VariableObserverNotifier =
    std::function<void(Actor& self, ConditionCacheView variables)>;

/// TraitsInterfaces.cs L633:readonly record struct VariableObserver
struct VariableObserver {
  VariableObserverNotifier fn_notifier;
  std::vector<std::string> vec_variables;
};

/// TraitsInterfaces.cs L635-638
class IObservesVariables {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IObservesVariables;
  virtual ~IObservesVariables() = default;
  virtual std::vector<VariableObserver> GetVariableObservers() = 0;
};

// ———— 仿真生命周期接口(TraitsInterfaces.cs L109-166)————
// ———— Sim lifecycle interfaces (TraitsInterfaces.cs L109-166) ————

/// TraitsInterfaces.cs L110
class ITick {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITick;
  virtual ~ITick() = default;
  virtual void Tick(Actor& self) = 0;
};

/// TraitsInterfaces.cs L112(渲染域;World.TickRender 分发面,Phase 4 起消费)
/// TraitsInterfaces.cs L112 (render domain; dispatched by World.TickRender,
/// consumed from Phase 4 on).
class ITickRender {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITickRender;
  virtual ~ITickRender() = default;
};

/// TraitsInterfaces.cs L155
class INotifyCreated {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyCreated;
  virtual ~INotifyCreated() = default;
  virtual void Created(Actor& self) = 0;
};

/// TraitsInterfaces.cs L158
class INotifyAddedToWorld {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyAddedToWorld;
  virtual ~INotifyAddedToWorld() = default;
  virtual void AddedToWorld(Actor& self) = 0;
};

/// TraitsInterfaces.cs L160
class INotifyRemovedFromWorld {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyRemovedFromWorld;
  virtual ~INotifyRemovedFromWorld() = default;
  virtual void RemovedFromWorld(Actor& self) = 0;
};

/// TraitsInterfaces.cs L163
class INotifyActorDisposing {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyActorDisposing;
  virtual ~INotifyActorDisposing() = default;
  virtual void Disposing(Actor& self) = 0;
};

/// TraitsInterfaces.cs L164
class INotifyOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyOwnerChanged;
  virtual ~INotifyOwnerChanged() = default;
  virtual void OnOwnerChanged(Actor& self, Player& old_owner,
                              Player& new_owner) = 0;
};

/// TraitsInterfaces.cs L165
class INotifyEffectiveOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyEffectiveOwnerChanged;
  virtual ~INotifyEffectiveOwnerChanged() = default;
  virtual void OnEffectiveOwnerChanged(Actor& self, Player& old_effective_owner,
                                       Player& new_effective_owner) = 0;
};

/// TraitsInterfaces.cs L166
class INotifyOwnerLost {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyOwnerLost;
  virtual ~INotifyOwnerLost() = default;
  virtual void OnOwnerLost(Actor& self) = 0;
};

/// TraitsInterfaces.cs L426
class INotifyBecomingIdle {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyBecomingIdle;
  virtual ~INotifyBecomingIdle() = default;
  virtual void OnBecomingIdle(Actor& self) = 0;
};

/// TraitsInterfaces.cs L429
class INotifyIdle {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyIdle;
  virtual ~INotifyIdle() = default;
  virtual void TickIdle(Actor& self) = 0;
};

/// TraitsInterfaces.cs L150
class IResolveOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IResolveOrder;
  virtual ~IResolveOrder() = default;
  virtual void ResolveOrder(Actor& self, const ora::net::Order& order) = 0;
};

/// TraitsInterfaces.cs L151
class IValidateOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IValidateOrder;
  virtual ~IValidateOrder() = default;
  virtual bool OrderValidation(ora::net::OrderManager& order_manager,
                               World& world, int client_id,
                               const ora::net::Order& order) = 0;
};

/// TraitsInterfaces.cs L627
class ICreationActivity {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreationActivity;
  virtual ~ICreationActivity() = default;
  virtual Activity* GetCreationActivity() = 0;
};

/// TraitsInterfaces.cs L366:IActivityInterface(Activity 树查询标记)
/// TraitsInterfaces.cs L366: IActivityInterface (the Activity-tree query
/// marker).
class IActivityInterface {
 public:
  virtual ~IActivityInterface() = default;
};

/// TraitsInterfaces.cs L342:ITraitInfoInterface(Info 侧标记;C++ 侧 Info 走
/// meta::RecordObject 值袋/Phase 5 手写类,此接口仅作键位锚定)
/// TraitsInterfaces.cs L342: ITraitInfoInterface (the Info-side marker; on
/// the C++ side Info rides the meta::RecordObject value bag / Phase 5
/// hand-written classes — this interface only anchors the key slot).
class ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITraitInfoInterface;
  virtual ~ITraitInfoInterface() = default;
};

// ———— Actor 构造期缓存的查询接口(TraitsInterfaces.cs L48-63/203-208/309-341/
//      532-549/649-654;IOccupySpace/IFacing/IHealth/IEffectiveOwner/ITargetable/
//      ITargetablePositions/ICrushable/IDefaultVisibility/IVisibilityModifier)————
// ———— The query interfaces cached at Actor construction (TraitsInterfaces
//      .cs L48-63/203-208/309-341/532-549/649-654) ————

/// TraitsInterfaces.cs L48-51
class IHealthInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IHealthInfo;
  virtual int MaxHP() const = 0;
};

/// TraitsInterfaces.cs L53-63
class IHealth {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IHealth;
  virtual ~IHealth() = default;
  virtual DamageState DamageState() const = 0;
  virtual int HP() const = 0;
  virtual int MaxHP() const = 0;
  virtual int DisplayHP() const = 0;
  virtual bool IsDead() const = 0;
  virtual void InflictDamage(Actor& self, Actor* attacker,
                             const struct Damage& damage,
                             bool ignore_modifiers) = 0;
  virtual void Kill(Actor& self, Actor* attacker,
                    const core::BitSet<DamageType>& damage_types) = 0;
};

/// TraitsInterfaces.cs L83-107
class AttackInfo {
 public:
  const struct Damage* Damage = nullptr;  // 引用语义(上游可变引用)
  const Actor* Attacker = nullptr;
  // 上游成员名 DamageState 与类型同名(C# 允许);C++ 数据成员不可遮蔽
  // 类型,按规范前缀命名 —— COVERAGE 登记
  // The upstream member is named DamageState, shadowing its type (legal
  // C#); a C++ data member must not shadow the type, so it takes the
  // prefix name — registered in COVERAGE.
  enum DamageState damage_state = DamageState::Undamaged;
  enum DamageState previous_damage_state = DamageState::Undamaged;
};

/// TraitsInterfaces.cs L91-107
struct Damage {
  int Value = 0;
  core::BitSet<DamageType> DamageTypes;

  Damage() = default;
  Damage(int value, core::BitSet<DamageType> damage_types)
      : Value(value), DamageTypes(std::move(damage_types)) {}
  explicit Damage(int value) : Value(value) {}
};

/// TraitsInterfaces.cs L204-208
class IEffectiveOwner {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IEffectiveOwner;
  virtual ~IEffectiveOwner() = default;
  virtual bool Disguised() const = 0;
  virtual Player* Owner() const = 0;
};

/// TraitsInterfaces.cs L309-320
class IOccupySpaceInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOccupySpaceInfo;
};

/// TraitsInterfaces.cs L315-320
class IOccupySpace {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOccupySpace;
  virtual ~IOccupySpace() = default;
  virtual WPos CenterPosition() const = 0;
  virtual CPos TopLeft() const = 0;
  virtual std::vector<std::pair<CPos, SubCell>> OccupiedCells()
      const = 0;
};

/// TraitsInterfaces.cs L333-338
class IFacing {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IFacing;
  virtual ~IFacing() = default;
  virtual WAngle TurnSpeed() const = 0;
  virtual WAngle Facing() const = 0;
  virtual void SetFacing(const WAngle& facing) = 0;
  virtual WRot Orientation() const = 0;
};

/// TraitsInterfaces.cs L532-543
class ITargetable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITargetable;
  virtual ~ITargetable() = default;
  virtual core::BitSet<TargetableType> TargetTypes() const = 0;
  virtual bool TargetableBy(Actor& self, Actor& by_actor) = 0;
  virtual bool RequiresForceFire() const = 0;
};

/// TraitsInterfaces.cs L545-549
class ITargetablePositions {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITargetablePositions;
  virtual ~ITargetablePositions() = default;
  virtual std::vector<WPos> TargetablePositions(Actor& self) = 0;
};

/// TraitsInterfaces.cs L649-654(双 CrushableBy 重载:bool 询问 +
/// 掩码并集面 —— UpdateCellBlocking 消费后者)
/// TraitsInterfaces.cs L649-654 (both CrushableBy overloads: the bool
/// query + the mask-union face — UpdateCellBlocking consumes the latter).
class ICrushable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICrushable;
  virtual ~ICrushable() = default;
  virtual bool CrushableBy(Actor& self, Actor& crusher,
                           const core::BitSet<CrushClass>& crush_classes) = 0;
  virtual PlayerMaskSet CrushableByMask(
      Actor& self, const core::BitSet<CrushClass>& crush_classes) = 0;
};

// ———— Phase 5 第一批接口增量(TraitsInterfaces.cs / Mods.Common)————
// ———— The batch-16 interface additions (TraitsInterfaces.cs / Mods.Common)
//      ————

/// TraitsInterfaces.cs L~:IWorldLoaded / IPostWorldLoaded
class IWorldLoaded {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IWorldLoaded;
  virtual ~IWorldLoaded() = default;
  virtual void WorldLoaded(World& world, gfx::WorldRenderer* wr) = 0;
};

class IPostWorldLoaded {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IPostWorldLoaded;
  virtual ~IPostWorldLoaded() = default;
  virtual void PostWorldLoaded(World& world, gfx::WorldRenderer* wr) = 0;
};

/// TraitsInterfaces.cs L~:INotifySelection / INotifySelected
class INotifySelection {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifySelection;
  virtual ~INotifySelection() = default;
  virtual void SelectionChanged() = 0;
};

class INotifySelected {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifySelected;
  virtual ~INotifySelected() = default;
  virtual void Selected(Actor& self) = 0;
};

/// TraitsInterfaces.cs L~:IGameOver / INotifyPlayerDisconnected
class IGameOver {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IGameOver;
  virtual ~IGameOver() = default;
  virtual void GameOver(World& world) = 0;
};

class INotifyPlayerDisconnected {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyPlayerDisconnected;
  virtual ~INotifyPlayerDisconnected() = default;
  virtual void PlayerDisconnected(Actor& self, Player& player) = 0;
};

/// TraitsInterfaces.cs L~:ICreatePlayers
class ICreatePlayers {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreatePlayers;
  virtual ~ICreatePlayers() = default;
  virtual void CreatePlayers(World& world, MersenneTwister& player_random) = 0;
};

/// TraitsInterfaces.cs L~:INotifyGameLoading / INotifyGameLoaded
class INotifyGameLoading {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyGameLoading;
  virtual ~INotifyGameLoading() = default;
  virtual void GameLoading(World& world) = 0;
};

class INotifyGameLoaded {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyGameLoaded;
  virtual ~INotifyGameLoaded() = default;
  virtual void GameLoaded(World& world) = 0;
};

/// TraitsInterfaces.cs L~:IGameSaveTraitData
class IGameSaveTraitData {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IGameSaveTraitData;
  virtual ~IGameSaveTraitData() = default;
  virtual std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) = 0;
  virtual void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) = 0;
};

/// TraitsInterfaces.cs L231-232(可见性查询;CanBeViewedByPlayer 消费)
/// TraitsInterfaces.cs L231-232 (visibility queries; consumed by
/// CanBeViewedByPlayer).
class IDefaultVisibility {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IDefaultVisibility;
  virtual ~IDefaultVisibility() = default;
  virtual bool IsVisible(Actor& self, Player* by_player) = 0;
};

class IVisibilityModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IVisibilityModifier;
  virtual ~IVisibilityModifier() = default;
  virtual bool IsVisible(Actor& self, Player* by_player) = 0;
};

// ———— Phase 5 第二批接口增量(TraitsInterfaces.cs 引擎侧 + Mods.Common)————
// ———— The batch-17 interface additions (TraitsInterfaces.cs engine side +
//      Mods.Common) ————

/// OpenRA.Mods.Common/TraitsInterfaces.cs L757:[Flags] MovementType
/// OpenRA.Mods.Common/TraitsInterfaces.cs L757: [Flags] MovementType.
enum class MovementType : std::int32_t {
  None = 0,
  Horizontal = 1,
  Vertical = 2,
  Turn = 4,
};

/// LocomoterExts.HasMovementType(Locomotor.cs L44-48;HasFlag 的位测等价)
/// LocomoterExts.HasMovementType (Locomotor.cs L44-48): the bit-test
/// equivalent of HasFlag.
inline bool HasMovementType(MovementType m, MovementType movement_type) {
  return (static_cast<std::int32_t>(m) &
          static_cast<std::int32_t>(movement_type)) ==
         static_cast<std::int32_t>(movement_type);
}

/// OpenRA.Mods.Common/TraitsInterfaces.cs L858:BlockedByActor
/// OpenRA.Mods.Common/TraitsInterfaces.cs L858: BlockedByActor.
enum class BlockedByActor : std::int32_t { None, Immovable, Stationary, All };

/// OpenRA.Game/Traits/TraitsInterfaces.cs L130:[Flags] TargetModifiers
/// OpenRA.Game/Traits/TraitsInterfaces.cs L130: [Flags] TargetModifiers.
enum class TargetModifiers : std::int32_t {
  None = 0,
  ForceAttack = 1,
  ForceQueue = 2,
  ForceMove = 4,
};

constexpr TargetModifiers operator|(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::int32_t>(a) |
                                      static_cast<std::int32_t>(b));
}

/// TraitsInterfaces.cs L234-268:IActorMap(实现 = Mods.Common ActorMap)
/// TraitsInterfaces.cs L234-268: IActorMap (implemented by Mods.Common's
/// ActorMap).
class ICustomMovementLayer;  // Mods.Common(L492);定义于本文件下方
// Mods.Common 的 LocomotorInfo 前置声明已在文件头(全局域)完成
// (the Mods.Common LocomotorInfo fwd declaration is at the file head,
// global scope).
// (前置于 ora::sim 打开前 —— 全局限定名查找不落入 ora::sim 内的嵌套域)
// (declared before ora::sim opens — the global qualification avoids the
// nested-domain lookup)

class IActorMap {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IActorMap;
  virtual ~IActorMap() = default;
  virtual std::vector<Actor*> GetActorsAt(CPos a) = 0;
  virtual std::vector<Actor*> GetActorsAt(CPos a, SubCell sub) = 0;
  virtual bool HasFreeSubCell(CPos cell, bool check_transient = true) = 0;
  virtual SubCell FreeSubCell(CPos cell, SubCell preferred_sub_cell = SubCell::Any,
                              bool check_transient = true) = 0;
  virtual SubCell FreeSubCell(CPos cell, SubCell preferred_sub_cell,
                              const std::function<bool(Actor&)>& check_if_blocker) = 0;
  virtual bool AnyActorsAt(CPos a) = 0;
  virtual bool AnyActorsAt(CPos a, SubCell sub, bool check_transient = true) = 0;
  virtual bool AnyActorsAt(CPos a, SubCell sub,
                           const std::function<bool(Actor&)>& with_condition) = 0;
  virtual std::vector<Actor*> AllActors() = 0;
  virtual void AddInfluence(Actor* self, IOccupySpace* ios) = 0;
  virtual void RemoveInfluence(Actor* self, IOccupySpace* ios) = 0;
  virtual int AddCellTrigger(const std::vector<CPos>& cells,
                             std::function<void(Actor&)> on_entry,
                             std::function<void(Actor&)> on_exit) = 0;
  virtual std::vector<CPos> TriggerPositions() = 0;
  virtual void RemoveCellTrigger(int id) = 0;
  virtual int AddProximityTrigger(const WPos& pos, const WDist& range,
                                  const WDist& v_range,
                                  std::function<void(Actor&)> on_entry,
                                  std::function<void(Actor&)> on_exit) = 0;
  virtual void RemoveProximityTrigger(int id) = 0;
  virtual void UpdateProximityTrigger(int id, const WPos& new_pos,
                                      const WDist& new_range,
                                      const WDist& new_v_range) = 0;
  virtual void AddPosition(Actor* a, IOccupySpace* ios) = 0;
  virtual void RemovePosition(Actor* a, IOccupySpace* ios) = 0;
  virtual void UpdatePosition(Actor* a, IOccupySpace* ios) = 0;
  virtual std::vector<Actor*> ActorsInBox(const WPos& a, const WPos& b) = 0;
  virtual WDist LargestActorRadius() const = 0;
  virtual WDist LargestBlockingActorRadius() const = 0;
  virtual void UpdateOccupiedCells(IOccupySpace* ios) = 0;
  /// CellUpdated 事件 → 回调表(construct 序)| the CellUpdated event →
  /// callbacks in registration order.
  virtual void AddCellUpdatedListener(std::function<void(CPos)> fn) = 0;
  /// GetCustomMovementLayers 扩展方法(ActorMapWorldExts L684-688)的接口化
  /// 承载(C++ 分层下扩展方法 → 接口成员;形状适配登记 COVERAGE)
  /// The interface carrier of the GetCustomMovementLayers extension method
  /// (ActorMapWorldExts L684-688) (extension method → an interface member
  /// under the C++ layering — the shape adaptation is registered in
  /// COVERAGE).
  virtual std::span<ICustomMovementLayer* const> CustomMovementLayers()
      const = 0;
};

/// TraitsInterfaces.cs L508-511:IControlGroupsInfo | IControlGroupsInfo
/// (TraitsInterfaces.cs L508-511).
class IControlGroupsInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IControlGroupsInfo;
  virtual const std::vector<std::string>& Groups() const = 0;
};

/// TraitsInterfaces.cs L513-522:IControlGroups | IControlGroups
/// (TraitsInterfaces.cs L513-522).
class IControlGroups {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IControlGroups;
  virtual ~IControlGroups() = default;
  virtual const std::vector<std::string>& Groups() const = 0;
  virtual void SelectControlGroup(int group) = 0;
  virtual void CreateControlGroup(int group) = 0;
  virtual void AddSelectionToControlGroup(int group) = 0;
  virtual void CombineSelectionWithControlGroup(int group) = 0;
  virtual void AddToControlGroup(Actor* a, int group) = 0;
  virtual void RemoveFromControlGroup(Actor* a) = 0;
  virtual std::optional<int> GetControlGroupForActor(Actor* a) = 0;
  virtual std::vector<Actor*> GetActorsInControlGroup(int group) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L492-501:ICustomMovementLayer(具体实现
/// 为 mod 侧 trait;本批无实现 —— 数组恒 [null])
/// Mods.Common/TraitsInterfaces.cs L492-501: ICustomMovementLayer (the
/// concrete implementations are mod traits; none this batch — the array
/// stays [null]).
class ICustomMovementLayer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ICustomMovementLayer;
  virtual ~ICustomMovementLayer() = default;
  virtual std::uint8_t Index() const = 0;
  virtual bool InteractsWithDefaultLayer() const = 0;
  virtual bool ReturnToGroundLayerOnIdle() const = 0;
  virtual bool EnabledForLocomotor(
      const ora::mods::pathfinding::LocomotorInfo& li) const = 0;
  virtual short EntryMovementCost(
      const ora::mods::pathfinding::LocomotorInfo& li, CPos cell) const = 0;
  virtual short ExitMovementCost(
      const ora::mods::pathfinding::LocomotorInfo& li, CPos cell) const = 0;
  virtual std::uint8_t GetTerrainIndex(CPos cell) const = 0;
  virtual WPos CenterOfCell(CPos cell) const = 0;
};

/// TraitsInterfaces.cs L395-400:IAssignSpawnPoints(实现随 SpawnPoints
/// mod trait 批;接口先锚定) | IAssignSpawnPoints (TraitsInterfaces.cs
/// L395-400; the implementation lands with the SpawnPoints trait batch).
class IAssignSpawnPoints {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAssignSpawnPoints;
  virtual ~IAssignSpawnPoints() = default;
  virtual CPos AssignHomeLocation(World& world, net::SessionClient& client,
                                  MersenneTwister& player_random) = 0;
  virtual int SpawnPointForPlayer(Player* player) = 0;
};

/// TraitsInterfaces.cs L402-406:IAssignSpawnPointsInfo | the Info face.
class IAssignSpawnPointsInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAssignSpawnPointsInfo;
};

/// TraitsInterfaces.cs L408-412:IBotInfo(AI 面随 Phase 8;接口锚定)
/// TraitsInterfaces.cs L408-412: IBotInfo (the AI face lands in Phase 8;
/// the interface anchors now).
class IBotInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IBotInfo;
  virtual std::string Type() const = 0;
  virtual std::string Name() const = 0;
};

/// TraitsInterfaces.cs L414-420:IBot | IBot.
class IBot {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IBot;
  virtual ~IBot() = default;
  virtual void Activate(Player* p) = 0;
  virtual void QueueOrder(net::Order* order) = 0;
  virtual const IBotInfo* Info() const = 0;
  virtual Player* GetPlayer() const = 0;
};

/// TraitsInterfaces.cs L624:IUnlocksRenderPlayer | IUnlocksRenderPlayer
/// (TraitsInterfaces.cs L624).
class IUnlocksRenderPlayer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IUnlocksRenderPlayer;
  virtual ~IUnlocksRenderPlayer() = default;
  virtual bool RenderPlayerUnlocked() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L~:INotifyDamage 家族 + IDamageModifier
/// (Health 的通知面;Damage/AttackInfo 见上方定义)
/// The Mods.Common INotifyDamage family + IDamageModifier (Health's
/// notification faces; Damage/AttackInfo are defined above).
class INotifyDamage {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDamage;
  virtual ~INotifyDamage() = default;
  virtual void Damaged(Actor& self, const AttackInfo& ai) = 0;
};

class INotifyDamageStateChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDamageStateChanged;
  virtual ~INotifyDamageStateChanged() = default;
  virtual void DamageStateChanged(Actor& self, const AttackInfo& ai) = 0;
};

class INotifyKilled {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyKilled;
  virtual ~INotifyKilled() = default;
  virtual void Killed(Actor& self, const AttackInfo& ai) = 0;
};

class INotifyAppliedDamage {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyAppliedDamage;
  virtual ~INotifyAppliedDamage() = default;
  virtual void AppliedDamage(Actor& self, Actor& hit, const AttackInfo& ai) = 0;
};

class IDamageModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDamageModifier;
  virtual ~IDamageModifier() = default;
  virtual int GetDamageModifier(Actor* attacker,
                                const Damage& damage) const = 0;
};

/// TraitsInterfaces.cs L123-127:IIssueOrder + L141-148:IOrderTargeter
/// (订单目标器面;具体 trait 随 Mobile/Building 批接入 —— 本批无实现,
/// UnitOrderGenerator 的空集行为即部分覆盖装配面的上游等价)
/// TraitsInterfaces.cs L123-127 IIssueOrder + L141-148 IOrderTargeter (the
/// order-targeter faces; concrete traits arrive with the Mobile/Building
/// batches — no implementations this batch, so UnitOrderGenerator's
/// empty-set behavior is the upstream equivalent under the partial-
/// coverage assembly face).
class IOrderTargeter {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOrderTargeter;
  virtual ~IOrderTargeter() = default;
  virtual std::string OrderID() const = 0;
  virtual int OrderPriority() const = 0;
  virtual bool CanTarget(Actor& self, const Target& target,
                         TargetModifiers& modifiers,
                         std::string& cursor) = 0;
  virtual bool IsQueued() const = 0;
  virtual bool TargetOverridesSelection(Actor& self, const Target& target,
                                        std::span<Actor* const> actors_at,
                                        CPos xy, TargetModifiers modifiers) = 0;
};

class IIssueOrder {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IIssueOrder;
  virtual ~IIssueOrder() = default;
  virtual std::vector<IOrderTargeter*> Orders() = 0;
  virtual net::Order* IssueOrder(Actor& self, IOrderTargeter* order,
                                 const Target& target, bool queued) = 0;
};

/// trait 运行时对象的公共基(C# object 等价;TypeDictionary/TraitDictionary
/// 容器存储要求)。具体 trait 类继承本类 + 各接口,并以 ORA_TRAIT_INTERFACES
/// 声明类型注册面
/// The common base of runtime trait objects (the C# object equivalent;
/// required for TypeDictionary/TraitContainer storage). Concrete traits
/// inherit this base plus their interfaces and declare the registration
/// surface with ORA_TRAIT_INTERFACES.
struct TraitUpcastEntry;

class TraitBase {
 public:
  virtual ~TraitBase() = default;
  virtual gen::TypeId GetTraitTypeId() const = 0;  // 运行时键(哈希注册表查名)
  /// 注册面视图(AddTrait 的 GetInterfaces()+BaseTypes() 等价)
  /// The registration-surface view (the GetInterfaces()+BaseTypes()
  /// equivalent of AddTrait).
  virtual std::span<const TraitUpcastEntry> TraitUpcasts() const = 0;
  /// trait 使能判定(上游 Exts.IsTraitEnabled —— IDisabledTrait/条件系统联动;
  /// Phase 5 条件 trait 接线,基线恒启用)
  /// The trait-enabled check (upstream Exts.IsTraitEnabled — the
  /// IDisabledTrait/condition interplay; wired with Phase 5 conditional
  /// traits, always enabled at this stage).
  virtual bool IsTraitEnabled() const { return true; }
  virtual bool IsTraitDisabled() const { return false; }
};

// ———— trait 上行转换表(AddTrait 的 GetInterfaces()+BaseTypes() 注册面)————
// ———— The trait upcast table (the GetInterfaces()+BaseTypes()
//      registration surface of AddTrait) ————

/// 一条注册项:类型键 + void*→该类型子对象指针的上行转换
/// One registration entry: the type key + the void*→sub-object upcast.
struct TraitUpcastEntry {
  gen::TypeId type_id;
  void* (*upcast)(void*);
};

/// 为具体 trait 类型生成注册表(自身 + 列出的全部接口/基类;重复键在
/// TraitDictionary 内按 C# 语义重复注册)
/// Builds the registration table for a concrete trait type (itself + every
/// listed interface/base; duplicate keys re-register per C# semantics).
template <class TraitT, class... Ifaces>
consteval std::array<TraitUpcastEntry, sizeof...(Ifaces) + 1>
MakeTraitUpcasts() {
  return {
      TraitUpcastEntry{
          TraitT::kTypeId,
          [](void* p) -> void* { return static_cast<TraitT*>(p); }},
      TraitUpcastEntry{
          Ifaces::kTypeId,
          [](void* p) -> void* {
            return static_cast<Ifaces*>(static_cast<TraitT*>(p));
          }}...,
  };
}

/// 手写 trait 类声明静态注册表(成员名固定 kTraitUpcasts;
/// kTraitTypeId = gen/interfaces_gen.h 的权威 ID)
/// Declares the static registration table on a hand-written trait class
/// (member name fixed as kTraitUpcasts; kTraitTypeId = the authoritative
/// id from gen/interfaces_gen.h).
#define ORA_TRAIT_INTERFACES(TraitT, TypeIdEnum, ...)                     \
  static constexpr gen::TypeId kTypeId = gen::TypeId::TypeIdEnum;        \
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }        \
  static constexpr auto kTraitUpcasts =                                 \
      ora::sim::MakeTraitUpcasts<TraitT, __VA_ARGS__>();\
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {\
    return kTraitUpcasts;\
  }

}  // namespace ora::sim
