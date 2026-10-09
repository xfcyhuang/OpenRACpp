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
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/long_bitset.hpp"
#include "core/mersenne_twister.hpp"
#include "core/polygon.hpp"
#include "core/rectangle.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "gen/interfaces_gen.h"
#include "yaml/mini_yaml.hpp"

namespace ora::game {
class ActorInfo;  // Phase 2 加载链产物 | the Phase 2 loading-chain product.
}

namespace ora::mods {
class Armament;  // Mods.Common(armament.hpp;接口签名前向承载)
                 // Mods.Common (armament.hpp; the forward carrier of the
                 // interface signatures).
struct Barrel;   // Armament.cs L22(同上 | same as above)
class DockClientManager;  // 第六批 dock 链(接口签名前向承载;定义于
                          // mods/dock_client.hpp)
                          // The batch-6 dock chain (the forward carrier of
                          // the interface signatures; defined in
                          // mods/dock_client.hpp).
}  // namespace ora::mods

namespace ora::mods::activities {
class MoveCooldownHelper;  // move_activities.hpp(IDockHost 签名前向)
                           // move_activities.hpp (IDockHost's signature
                           // forward).
}

namespace ora::mods::pathfinding {
class LocomotorInfo;  // Mods.Common;寻路批的类型面(a pathfinding-batch type)
}

namespace ora::gfx {
class WorldRenderer;  // 渲染域注入面(Phase 4)| the render-domain injection face.
struct RenderItem;  // 渲染出参域(renderable.hpp;第八批接口面)| the render
                    // out-parameter domain (renderable.hpp; the batch-8
                    // interface faces).
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
class TypeDictionary;
class TraitBase;  // INotifyAiming 的参数域(定义于本文件后段)
                   // INotifyAiming's parameter domain (defined later in
                   // this file).
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
  /// TraitsInterfaces.cs L112:TickRender(WorldRenderer, Actor)——
  /// WorldRenderer 参数 = 渲染批的注入面,C++ 承载为 Actor 单参
  /// TraitsInterfaces.cs L112: TickRender(WorldRenderer, Actor) — the
  /// WorldRenderer parameter is the render batch's injection face,
  /// carried in C++ as the single Actor parameter.
  virtual void TickRender(Actor& self) = 0;
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

/// MovementType 的 [Flags] 组合(C# |=)
/// The [Flags] composition of MovementType (C#'s |=).
constexpr MovementType operator|(MovementType a, MovementType b) {
  return static_cast<MovementType>(static_cast<std::int32_t>(a) |
                                   static_cast<std::int32_t>(b));
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

/// PlayerRelationship 的 [Flags] 组合(C# '|')
/// The [Flags] composition of PlayerRelationship (C# '|').
constexpr PlayerRelationship operator|(PlayerRelationship a,
                                       PlayerRelationship b) {
  return static_cast<PlayerRelationship>(static_cast<std::int32_t>(a) |
                                         static_cast<std::int32_t>(b));
}

constexpr TargetModifiers operator|(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::int32_t>(a) |
                                      static_cast<std::int32_t>(b));
}

/// TargetModifiers 的位测试(上游 modifiers.HasModifier(...) 的等价面)
/// The bit test of TargetModifiers (the equivalent of upstream's
/// modifiers.HasModifier(...)).
constexpr bool HasModifier(TargetModifiers m, TargetModifiers flag) {
  return (static_cast<std::int32_t>(m) &
          static_cast<std::int32_t>(flag)) != 0;
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

// ———— 第三批接口增量(TraitsInterfaces.cs 引擎侧 + Mods.Common;
//      Mobile/Move 族/Armament 的承载面)————
// ———— The batch-3 interface additions (TraitsInterfaces.cs engine side +
//      Mods.Common; the carrier faces of Mobile/the move family/Armament)
// ————

/// OpenRA.Traits/TraitsInterfaces.cs L152:IOrderVoice
/// OpenRA.Traits/TraitsInterfaces.cs L152: IOrderVoice.
class IOrderVoice {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOrderVoice;
  virtual ~IOrderVoice() = default;
  virtual std::string VoicePhraseForOrder(Actor& self,
                                          const net::Order& order) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L765-771:MoveResult
/// Mods.Common/TraitsInterfaces.cs L765-771: MoveResult.
enum class MoveResult : std::int32_t {
  InProgress = 0,
  CompleteCanceled = 1,
  CompleteDestinationReached = 2,
  CompleteDestinationBlocked = 3,
};

/// Mods.Common/TraitsInterfaces.cs L77:INotifyCustomLayerChanged
class INotifyCustomLayerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCustomLayerChanged;
  virtual ~INotifyCustomLayerChanged() = default;
  virtual void CustomLayerChanged(Actor& self, std::uint8_t old_layer,
                                  std::uint8_t new_layer) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L83:INotifyCenterPositionChanged
class INotifyCenterPositionChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCenterPositionChanged;
  virtual ~INotifyCenterPositionChanged() = default;
  virtual void CenterPositionChanged(Actor& self, std::uint8_t old_layer,
                                     std::uint8_t new_layer) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L89:INotifyFinishedMoving
class INotifyFinishedMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyFinishedMoving;
  virtual ~INotifyFinishedMoving() = default;
  virtual void FinishedMoving(Actor& self, std::uint8_t old_layer,
                              std::uint8_t new_layer) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L774:INotifyMoving
class INotifyMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyMoving;
  virtual ~INotifyMoving() = default;
  virtual void MovementTypeChanged(Actor& self, MovementType type) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L102:INotifyCrushed(双方法)
/// Mods.Common/TraitsInterfaces.cs L102: INotifyCrushed (both methods).
class INotifyCrushed {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCrushed;
  virtual ~INotifyCrushed() = default;
  virtual void OnCrush(Actor& self, Actor& crusher,
                       const core::BitSet<CrushClass>& crush_classes) = 0;
  virtual void WarnCrush(Actor& self, Actor& crusher,
                         const core::BitSet<CrushClass>& crush_classes) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L116:INotifyAttack(签名经 ora::mods::
/// Armament/Barrel 头部前向声明承载)
/// Mods.Common/TraitsInterfaces.cs L116: INotifyAttack (the signatures ride
/// the head's forward declarations of ora::mods::Armament/Barrel).
class INotifyAttack {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyAttack;
  virtual ~INotifyAttack() = default;
  virtual void Attacking(Actor& self, const Target& target,
                         mods::Armament& armament, const mods::Barrel& barrel) = 0;
  virtual void PreparingAttack(Actor& self, const Target& target,
                               mods::Armament& armament,
                               const mods::Barrel& barrel) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L149:INotifyBurstComplete
class INotifyBurstComplete {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyBurstComplete;
  virtual ~INotifyBurstComplete() = default;
  virtual void FiredBurst(Actor& self, const Target& target,
                          mods::Armament& armament) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L194:INotifyBlockingMove
class INotifyBlockingMove {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyBlockingMove;
  virtual ~INotifyBlockingMove() = default;
  virtual void OnNotifyBlockingMove(Actor& self, Actor& blocking) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L395:IDeathActorInitModifier
class IDeathActorInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDeathActorInitModifier;
  virtual ~IDeathActorInitModifier() = default;
  virtual void ModifyDeathActorInit(Actor& self, TypeDictionary& init) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L425:IActorPreviewInitModifier
class IActorPreviewInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IActorPreviewInitModifier;
  virtual ~IActorPreviewInitModifier() = default;
  virtual void ModifyActorPreviewInit(Actor& self, TypeDictionary& inits) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L450:ISpeedModifier
class ISpeedModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ISpeedModifier;
  virtual ~ISpeedModifier() = default;
  virtual int GetSpeedModifier() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L453:IFirepowerModifier
class IFirepowerModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IFirepowerModifier;
  virtual ~IFirepowerModifier() = default;
  virtual int GetFirepowerModifier() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L456:IReloadModifier
class IReloadModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IReloadModifier;
  virtual ~IReloadModifier() = default;
  virtual int GetReloadModifier() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L462:IInaccuracyModifier
class IInaccuracyModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IInaccuracyModifier;
  virtual ~IInaccuracyModifier() = default;
  virtual int GetInaccuracyModifier() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L465:IRangeModifier
class IRangeModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRangeModifier;
  virtual ~IRangeModifier() = default;
  virtual int GetRangeModifier() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L468:IRangeModifierInfo
class IRangeModifierInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRangeModifierInfo;
  virtual int GetRangeModifierDefault() = 0;
};

/// Mods.Common/TraitsInterfaces.cs L911-916:IPositionableInfo
class IPositionableInfo : public IOccupySpaceInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPositionableInfo;
  virtual bool CanEnterCell(World& world, Actor* self, CPos cell,
                            SubCell sub_cell, Actor* ignore_actor,
                            BlockedByActor check) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L917-928:IPositionable
class IPositionable : public IOccupySpace {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPositionable;
  virtual bool CanExistInCell(CPos location) = 0;
  virtual bool IsLeavingCell(CPos location, SubCell sub_cell) = 0;
  virtual bool CanEnterCell(CPos location, Actor* ignore_actor,
                            BlockedByActor check) = 0;
  virtual SubCell GetValidSubCell(SubCell preferred) = 0;
  virtual SubCell GetAvailableSubCell(CPos location, SubCell preferred_sub_cell,
                                      Actor* ignore_actor,
                                      BlockedByActor check) = 0;
  virtual void SetPosition(Actor* self, CPos cell, SubCell sub_cell) = 0;
  virtual void SetPosition(Actor* self, WPos pos) = 0;
  virtual void SetCenterPosition(Actor* self, WPos pos) = 0;
};

/// OpenRA.Traits/TraitsInterfaces.cs L551:IMoveInfo(查询面随 Mobile 接线)
/// OpenRA.Traits/TraitsInterfaces.cs L551: IMoveInfo (the query face wires
/// up with Mobile).
class IMoveInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IMoveInfo;
};

/// Mods.Common/TraitsInterfaces.cs L524-545:IMove(MoveTo/MoveWithinRange 等
/// 活动面;本批实现 = Mobile)
/// Mods.Common/TraitsInterfaces.cs L524-545: IMove (the MoveTo/MoveWithinRange
/// activity face; this batch's implementor is Mobile).
class IMove {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IMove;
  virtual ~IMove() = default;
  virtual Activity* MoveTo(CPos cell, int near_enough, Actor* ignore_actor,
                           bool evaluate_nearest_movable_cell,
                           std::optional<core::Color> target_line_color) = 0;
  virtual Activity* MoveWithinRange(
      const Target& target, WDist range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) = 0;
  virtual Activity* MoveWithinRange(
      const Target& target, WDist min_range, WDist max_range,
      std::optional<WPos> initial_target_position,
      std::optional<core::Color> target_line_color) = 0;
  virtual Activity* MoveFollow(Actor* self, const Target& target,
                               WDist min_range, WDist max_range,
                               std::optional<WPos> initial_target_position,
                               std::optional<core::Color> target_line_color) = 0;
  virtual Activity* ReturnToCell(Actor* self) = 0;
  virtual Activity* MoveIntoTarget(Actor* self, const Target& target) = 0;
  virtual Activity* MoveOntoTarget(Actor* self, const Target& target,
                                   const WVec& offset,
                                   std::optional<WAngle> facing,
                                   std::optional<core::Color> target_line_color) = 0;
  virtual Activity* LocalMove(Actor* self, WPos from_pos, WPos to_pos) = 0;
  virtual int EstimatedMoveDuration(Actor* self, WPos from_pos, WPos to_pos) = 0;
  virtual CPos NearestMoveableCell(CPos target) = 0;
  virtual MovementType CurrentMovementTypes() const = 0;
  virtual void SetCurrentMovementTypes(MovementType type) = 0;
  virtual bool CanEnterTargetNow(Actor* self, const Target& target) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L547:IWrapMove
class IWrapMove {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IWrapMove;
  virtual ~IWrapMove() = default;
  virtual Activity* WrapMove(Activity* move_inner) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L42:IQuantizeBodyOrientationInfo(实现 =
/// 渲染批的 QuantizeFacingsFromSequence 等;接口先行锚定)
/// Mods.Common/TraitsInterfaces.cs L42: IQuantizeBodyOrientationInfo (the
/// implementors are the render batch's QuantizeFacingsFromSequence & co.;
/// the interface anchors first).
class SequenceSet;  // gfx 前向(gfx/sprite_cache 侧;实现批接线)
                    // the gfx fwd (the gfx/sprite_cache side; wired by the
                    // implementing batch).
class IQuantizeBodyOrientationInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IQuantizeBodyOrientationInfo;
  virtual int QuantizedBodyFacings(const game::ActorInfo& ai,
                                   const SequenceSet& sequences,
                                   const std::string& faction) = 0;
};

/// OpenRA.Game/Traits/TraitsInterfaces.cs L324-330:ITemporaryBlocker
/// (实现 = mod 侧 TransientBlocker 等;接口随 ContainsTemporaryBlocker 锚定)
/// OpenRA.Game/Traits/TraitsInterfaces.cs L324-330: ITemporaryBlocker (the
/// implementors are mod traits like TransientBlocker; anchored with
/// ContainsTemporaryBlocker).
class ITemporaryBlocker {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITemporaryBlocker;
  virtual ~ITemporaryBlocker() = default;
  virtual bool CanRemoveBlockage(Actor& self, Actor& blocking) = 0;
  virtual bool IsBlocking(Actor& self, CPos cell) = 0;
};

// ———— 第四批接口增量(Mods.Common 的攻击/战争迷雾面)————
// ———— The batch-4 interface additions (the Mods.Common attack/fog faces)
//      ————

/// Mods.Common INotifyAiming(AttackBase 的 aiming 通知;attack 参数 =
/// AttackBase trait 的公共基引用)
/// Mods.Common's INotifyAiming (AttackBase's aiming notifications; the
/// attack parameter = a common-base reference of the AttackBase trait).
class INotifyAiming {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyAiming;
  virtual ~INotifyAiming() = default;
  virtual void StartedAiming(Actor& self, TraitBase* attack) = 0;
  virtual void StoppedAiming(Actor& self, TraitBase* attack) = 0;
};

/// Mods.Common IOverrideAutoTarget(tryGetAutoTargetOverride 的 Try 形态:
/// out 参数 → 返回 optional)
/// Mods.Common's IOverrideAutoTarget (TryGetAutoTargetOverride's Try
/// shape: the out parameter → a returned optional).
class IOverrideAutoTarget {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IOverrideAutoTarget;
  virtual ~IOverrideAutoTarget() = default;
  virtual std::optional<struct Target> TryGetAutoTargetOverride(
      Actor& self) = 0;
};

/// Mods.Common IDisableEnemyAutoTarget
/// Mods.Common's IDisableEnemyAutoTarget.
class IDisableEnemyAutoTarget {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDisableEnemyAutoTarget;
  virtual ~IDisableEnemyAutoTarget() = default;
  virtual bool DisableEnemyAutoTarget(Actor& self, Actor& attacker) = 0;
};

/// Mods.Common IRevealsShroudModifier(GetRevealsShroudModifier)
/// Mods.Common's IRevealsShroudModifier (GetRevealsShroudModifier).
class IRevealsShroudModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRevealsShroudModifier;
  virtual ~IRevealsShroudModifier() = default;
  virtual int GetRevealsShroudModifier() const = 0;
};

/// Mods.Common ITargetableCells(HitShape 的 UseTargetableCellsOffsets 面)
/// Mods.Common's ITargetableCells (HitShape's
/// UseTargetableCellsOffsets face).
class ITargetableCells {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITargetableCells;
  virtual ~ITargetableCells() = default;
  virtual std::vector<std::pair<CPos, SubCell>> TargetableCells() = 0;
};

/// Mods.Common IBlocksProjectilesInfo | Mods.Common's
/// IBlocksProjectilesInfo.
class IBlocksProjectilesInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IBlocksProjectilesInfo;
};

/// Mods.Common IBlocksProjectiles(BlockingHeight/ValidRelationships)
/// Mods.Common's IBlocksProjectiles (BlockingHeight/
/// ValidRelationships).
class IBlocksProjectiles {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IBlocksProjectiles;
  virtual ~IBlocksProjectiles() = default;
  virtual WDist BlockingHeight() const = 0;
  virtual PlayerRelationship ValidRelationships() const = 0;
};

// ———— 第五批接口增量(建筑/生产链 + 迷雾修饰的承载面)————
// ———— The batch-5 interface additions (the carrier faces of the
//      building/production chain + the fog modifiers) ————

/// Mods.Common/TraitsInterfaces.cs L70-74:INotifySold
/// Mods.Common/TraitsInterfaces.cs L70-74: INotifySold.
class INotifySold {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifySold;
  virtual ~INotifySold() = default;
  virtual void Selling(Actor& self) = 0;
  virtual void Sold(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L333-338:INotifyTransform
/// (AfterTransform 的参数 = 变换后的 actor)
/// Mods.Common/TraitsInterfaces.cs L333-338: INotifyTransform (the
/// AfterTransform parameter = the post-transform actor).
class INotifyTransform {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyTransform;
  virtual ~INotifyTransform() = default;
  virtual void BeforeTransform(Actor& self) = 0;
  virtual void OnTransform(Actor& self) = 0;
  virtual void AfterTransform(Actor& to_actor) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L151:INotifyProduction
/// Mods.Common/TraitsInterfaces.cs L151: INotifyProduction.
class INotifyProduction {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyProduction;
  virtual ~INotifyProduction() = default;
  virtual void UnitProduced(Actor& self, Actor& other, CPos exit) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L152:INotifyOtherProduction
/// (TypeDictionary 前置声明承载 init 参数面)
/// Mods.Common/TraitsInterfaces.cs L152: INotifyOtherProduction (a
/// forward-declared TypeDictionary carries the init parameter face).
class TypeDictionary;

class INotifyOtherProduction {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyOtherProduction;
  virtual ~INotifyOtherProduction() = default;
  virtual void UnitProducedByOther(Actor& self, Actor& producer,
                                   Actor& produced,
                                   const std::string& production_type,
                                   TypeDictionary& inits) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L317-323:ITechTreeElement
/// (ProductionQueue 等 prerequisite 消费者的回调面)
/// Mods.Common/TraitsInterfaces.cs L317-323: ITechTreeElement (the
/// callback face of prerequisite consumers such as ProductionQueue).
class ITechTreeElement {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITechTreeElement;
  virtual ~ITechTreeElement() = default;
  virtual void PrerequisitesAvailable(const std::string& key) = 0;
  virtual void PrerequisitesUnavailable(const std::string& key) = 0;
  virtual void PrerequisitesItemHidden(const std::string& key) = 0;
  virtual void PrerequisitesItemVisible(const std::string& key) = 0;
};

/// Mods.Common ITechTreePrerequisiteInfo(Prerequisites(info) 的 Info 面;
/// 返回 string 序列)
/// Mods.Common's ITechTreePrerequisiteInfo (the Info face of
/// Prerequisites(info); returns the string sequence).
class ITechTreePrerequisiteInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITechTreePrerequisiteInfo;
  virtual ~ITechTreePrerequisiteInfo() = default;
  virtual std::vector<std::string> Prerequisites(
      const game::ActorInfo& info) = 0;
};

/// Mods.Common ITechTreePrerequisite(ProvidesPrerequisites 的查询面)
/// Mods.Common's ITechTreePrerequisite (the ProvidesPrerequisites query).
class ITechTreePrerequisite {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITechTreePrerequisite;
  virtual ~ITechTreePrerequisite() = default;
  virtual std::vector<std::string> ProvidesPrerequisites() = 0;
};

// ———— 第六批接口增补(Modules:电源/资源/dock/turret 链)————
// ———— The batch-6 interface additions (power/resources/dock/turret) ————

/// Mods.Common TraitsInterfaces.cs L145:INotifyPowerLevelChanged
/// Mods.Common TraitsInterfaces.cs L145: INotifyPowerLevelChanged.
class INotifyPowerLevelChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyPowerLevelChanged;
  virtual ~INotifyPowerLevelChanged() = default;
  virtual void PowerLevelChanged(Actor& self) = 0;
};

/// TraitsInterfaces.cs L471:IPowerModifier
/// TraitsInterfaces.cs L471: IPowerModifier.
class IPowerModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPowerModifier;
  virtual ~IPowerModifier() = default;
  virtual int GetPowerModifier() = 0;
};

/// TraitsInterfaces.cs L489:IResourceValueModifier
/// TraitsInterfaces.cs L489: IResourceValueModifier.
class IResourceValueModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IResourceValueModifier;
  virtual ~IResourceValueModifier() = default;
  virtual int GetResourceValueModifier() = 0;
};

/// TraitsInterfaces.cs L176:INotifyResourceAccepted
/// TraitsInterfaces.cs L176: INotifyResourceAccepted.
class INotifyResourceAccepted {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyResourceAccepted;
  virtual ~INotifyResourceAccepted() = default;
  virtual void OnResourceAccepted(Actor& self, Actor& refinery,
                                  const std::string& resource_type, int count,
                                  int value) = 0;
};

/// TraitsInterfaces.cs L208-213:INotifyHarvestAction
/// TraitsInterfaces.cs L208-213: INotifyHarvestAction.
class INotifyHarvestAction {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyHarvestAction;
  virtual ~INotifyHarvestAction() = default;
  virtual void Harvested(Actor& self, const std::string& resource_type) = 0;
  virtual void MovingToResources(Actor& self, CPos target_cell) = 0;
  virtual void MovementCancelled(Actor& self) = 0;
};

/// CaptureType 位标签(TraitsInterfaces.cs L180 的 BitSet<CaptureType>)
/// The CaptureType bit tag (TraitsInterfaces.cs L180's
/// BitSet<CaptureType>).
class CaptureType {};

/// TraitsInterfaces.cs L178-181:INotifyCapture(BitSet<CaptureType> 面随
/// Capturable 族批;本批实现者仅消费 owner 迁移语义)
/// TraitsInterfaces.cs L178-181: INotifyCapture (the BitSet<CaptureType>
/// face rides the Capturable-family batch; this batch's implementers only
/// consume the owner-migration semantics).
class INotifyCapture {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCapture;
  virtual ~INotifyCapture() = default;
  virtual void OnCapture(Actor& self, Actor& captor, Player& old_owner,
                         Player& new_owner,
                         const core::BitSet<CaptureType>&
                             capture_types) = 0;
};

/// ResourceLayer.cs L21-26:ResourceLayerContents(readonly struct)
/// ResourceLayer.cs L21-26: ResourceLayerContents (a readonly struct).
struct ResourceLayerContents {
  /// 上游 static readonly Empty(类内不完整类型 → 访问器函数承载)
  /// Upstream's static readonly Empty (in-class incompleteness → an
  /// accessor function).
  static const ResourceLayerContents& Empty();

  std::string str_type;  // null 语义 = 空串(上游 null 与 "" 同为无资源域)
  std::uint8_t uint1_density = 0;
};

inline const ResourceLayerContents& ResourceLayerContents::Empty() {
  static const ResourceLayerContents k_empty{};
  return k_empty;
}

/// TraitsInterfaces.cs L812:IResourceLayerInfo
/// TraitsInterfaces.cs L812: IResourceLayerInfo.
class IResourceLayerInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IResourceLayerInfo;
  virtual ~IResourceLayerInfo() = default;
  virtual bool TryGetTerrainType(const std::string& resource_type,
                                 std::string& terrain_type_out) = 0;
  virtual bool TryGetResourceIndex(const std::string& resource_type,
                                   std::uint8_t& index_out) = 0;
};

/// TraitsInterfaces.cs L822-835:IResourceLayer(CellChanged 事件 → 回调表
/// 形态,与 World 事件面同法;本批无订阅者)
/// TraitsInterfaces.cs L822-835: IResourceLayer (the CellChanged event →
/// a callback list, the same device as the World event faces; no
/// subscribers this batch).
class IResourceLayer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IResourceLayer;
  virtual ~IResourceLayer() = default;
  virtual ResourceLayerContents GetResource(CPos cell) = 0;
  virtual std::uint8_t GetMaxDensity(const std::string& resource_type) = 0;
  virtual bool CanAddResource(const std::string& resource_type, CPos cell,
                              std::uint8_t amount = 1) = 0;
  virtual int AddResource(const std::string& resource_type, CPos cell,
                          std::uint8_t amount = 1) = 0;
  virtual int RemoveResource(const std::string& resource_type, CPos cell,
                             std::uint8_t amount = 1) = 0;
  virtual void ClearResources(CPos cell) = 0;
  virtual bool IsVisible(CPos cell) = 0;
  virtual bool IsEmpty() = 0;
  virtual const IResourceLayerInfo& ResourceLayerInfo() const = 0;
};

/// Game TraitsInterfaces.cs L178-181:IStoresResourcesInfo
/// Game TraitsInterfaces.cs L178-181: IStoresResourcesInfo.
class IStoresResourcesInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IStoresResourcesInfo;
  virtual std::vector<std::string> ResourceTypes() = 0;
};

/// Game TraitsInterfaces.cs L183-200:IStoresResources(Contents 的只读字典
/// → 插入序 vector 对)
/// Game TraitsInterfaces.cs L183-200: IStoresResources (the read-only
/// Contents dictionary → insertion-ordered pairs).
class IStoresResources {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IStoresResources;
  virtual ~IStoresResources() = default;
  virtual bool HasType(const std::string& resource_type) = 0;
  virtual int Capacity() const = 0;
  virtual const std::vector<std::pair<std::string, int>>& Contents()
      const = 0;
  virtual int ContentsSum() const = 0;
  virtual int AddResource(const std::string& resource_type, int value) = 0;
  virtual int RemoveResource(const std::string& resource_type, int value) = 0;
};

/// TraitsInterfaces.cs L297:IAcceptResources
/// TraitsInterfaces.cs L297: IAcceptResources.
class IAcceptResources {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IAcceptResources;
  virtual ~IAcceptResources() = default;
  virtual int AcceptResources(Actor& self, const std::string& resource_type,
                              int count = 1) = 0;
};

/// DockHost.cs L19:DockType 位标签(BitSet<DockType> 的 tag)
/// DockHost.cs L19: the DockType bit tag (BitSet<DockType>'s tag).
class DockType {};

/// TraitsInterfaces.cs L215:IDockClientInfo | TraitsInterfaces.cs L215:
/// IDockClientInfo.
class IDockClientInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClientInfo;
};

class IDockHost;  // 定义于本文件下方(IDockClient 签名先行)
                   // (defined later in this file — IDockClient's
                   // signatures come first).

/// TraitsInterfaces.cs L217-243:IDockClient(DockClientManager 前向引用:
/// mods 层具体类)
/// TraitsInterfaces.cs L217-243: IDockClient (DockClientManager forward
/// references the mods-layer concrete class).
class IDockClient {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClient;
  virtual ~IDockClient() = default;
  virtual core::BitSet<DockType> GetDockType() = 0;
  virtual ora::mods::DockClientManager* GetDockClientManager() = 0;
  virtual void OnDockStarted(Actor& self, Actor& host_actor,
                             IDockHost* host) = 0;
  virtual bool OnDockTick(Actor& self, Actor& host_actor,
                          IDockHost* dock) = 0;
  virtual void OnDockCompleted(Actor& self, Actor& host_actor,
                               IDockHost* host) = 0;
  virtual bool CanDock(const core::BitSet<DockType>& type,
                       bool force_enter = false) = 0;
  virtual bool CanDockAt(Actor& host_actor, IDockHost* host,
                         bool force_enter = false,
                         bool ignore_occupancy = false) = 0;
  virtual bool CanQueueDockAt(Actor& host_actor, IDockHost* host,
                              bool force_enter, bool is_queued) = 0;
};

/// TraitsInterfaces.cs L251-280:IDockHost(QueueMoveActivity 的
/// MoveCooldownHelper 为 mods::activities 前向)
/// TraitsInterfaces.cs L251-280: IDockHost (QueueMoveActivity's
/// MoveCooldownHelper forwards from mods::activities).
class IDockHost {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockHost;
  virtual ~IDockHost() = default;
  virtual core::BitSet<DockType> GetDockType() = 0;
  virtual bool IsEnabledAndInWorld() = 0;
  virtual int ReservationCount() = 0;
  virtual bool CanBeReserved() = 0;
  virtual WPos DockPosition() = 0;
  virtual bool IsDockingPossible(
      Actor& client_actor, IDockClient* client,
      bool ignore_reservations = false) = 0;
  virtual bool Reserve(Actor& self,
                       ora::mods::DockClientManager* client) = 0;
  virtual void UnreserveAll() = 0;
  virtual void Unreserve(ora::mods::DockClientManager* client) = 0;
  virtual void OnDockStarted(Actor& self, Actor& client_actor,
                             ora::mods::DockClientManager* client) = 0;
  virtual void OnDockCompleted(Actor& self, Actor& client_actor,
                               ora::mods::DockClientManager* client) = 0;
  virtual bool QueueMoveActivity(
      class Activity* move_to_dock_activity, Actor& self, Actor& client_actor,
      ora::mods::DockClientManager* client,
      ora::mods::activities::MoveCooldownHelper& move_cooldown_helper) = 0;
  virtual void QueueDockActivity(
      class Activity* move_to_dock_activity, Actor& self, Actor& client_actor,
      ora::mods::DockClientManager* client) = 0;
};

/// TraitsInterfaces.cs L283:IDockClientManagerInfo
/// TraitsInterfaces.cs L283: IDockClientManagerInfo.
class IDockClientManagerInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClientManagerInfo;
};

/// TraitsInterfaces.cs L164:INotifyDockHost | TraitsInterfaces.cs L164:
/// INotifyDockHost.
class INotifyDockHost {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockHost;
  virtual ~INotifyDockHost() = default;
  virtual void Docked(Actor& self, Actor& client) = 0;
  virtual void Undocked(Actor& self, Actor& client) = 0;
};

/// TraitsInterfaces.cs L166:INotifyDockClient | TraitsInterfaces.cs L166:
/// INotifyDockClient.
class INotifyDockClient {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockClient;
  virtual ~INotifyDockClient() = default;
  virtual void Docked(Actor& self, Actor& host) = 0;
  virtual void Undocked(Actor& self, Actor& host) = 0;
};

/// TraitsInterfaces.cs L169-173:INotifyDockClientMoving
/// TraitsInterfaces.cs L169-173: INotifyDockClientMoving.
class INotifyDockClientMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockClientMoving;
  virtual ~INotifyDockClientMoving() = default;
  virtual void MovingToDock(Actor& self, Actor& host_actor,
                            IDockHost* host) = 0;
  virtual void MovementCancelled(Actor& self) = 0;
};

/// TraitsInterfaces.cs L289-293:IDockClientBody(动画闭包 → std::function)
/// TraitsInterfaces.cs L289-293: IDockClientBody (the animation closure →
/// std::function).
class IDockClientBody {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClientBody;
  virtual ~IDockClientBody() = default;
  virtual void PlayDockAnimation(
      Actor& self, std::function<void()> after) = 0;
  virtual void PlayReverseDockAnimation(
      Actor& self, std::function<void()> after) = 0;
};

/// trait 运行时对象的公共基(C# object 等价;TypeDictionary/TraitDictionary
/// 容器存储要求)。具体 trait 类继承本类 + 各接口,并以 ORA_TRAIT_INTERFACES
/// 声明类型注册面
/// The common base of runtime trait objects (the C# object equivalent;
/// required for TypeDictionary/TraitContainer storage). Concrete traits
/// inherit this base plus their interfaces and declare the registration
/// surface with ORA_TRAIT_INTERFACES.
struct TraitUpcastEntry;

// ———— 第七批接口面(Conditions/Cloak/Experience/Capture/Selectable/
//      SpawnMapActors 批)————
// ———— The batch-7 interface faces (the Conditions/Cloak/Experience/
//      Capture/Selectable/SpawnMapActors batch) ————

/// TraitsInterfaces.cs L293:ISelectionBar(float 域;sim 侧仅承载
/// GetValue/GetColor/DisplayWhenEmpty 的行为面,UI 消费随 Phase 6)
/// TraitsInterfaces.cs L293: ISelectionBar (a float domain; the sim side
/// carries the GetValue/GetColor/DisplayWhenEmpty behaviour face, the UI
/// consumption rides Phase 6).
class ISelectionBar {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ISelectionBar;
  virtual ~ISelectionBar() = default;
  virtual float GetValue() = 0;
  virtual core::Color GetColor() = 0;
  virtual bool DisplayWhenEmpty() const = 0;
};

/// Game/Traits/TraitsInterfaces.cs L475-480:SelectionPriorityModifiers
/// (Ctrl/Alt 的选择优先级热键位)
/// Game/Traits/TraitsInterfaces.cs L475-480: SelectionPriorityModifiers
/// (the Ctrl/Alt selection-priority hotkey bits).
enum class SelectionPriorityModifiers : std::int32_t {
  None = 0,
  Ctrl = 1,
  Alt = 2,
};

/// Game/Traits/TraitsInterfaces.cs L487-493:ISelectableInfo
class ISelectableInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ISelectableInfo;
  virtual int Priority() const = 0;
  virtual SelectionPriorityModifiers PriorityModifiers() const = 0;
  virtual std::string_view Voice() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L786-790:ISelectable(Class = 按类型
/// 选择的分组键)
/// Mods.Common/TraitsInterfaces.cs L786-790: ISelectable (Class = the
/// select-by-type grouping key).
class ISelectable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ISelectable;
  virtual ~ISelectable() = default;
  virtual std::string_view Class() const = 0;
};

/// ExternalCondition.cs L17-20:IConditionTimerWatcher(计时型外部条件
/// 的观察者;Duration/Remaining 每 tick 通知)
/// ExternalCondition.cs L17-20: IConditionTimerWatcher (the observer of
/// timed external conditions; Duration/Remaining notified per tick).
class IConditionTimerWatcher {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IConditionTimerWatcher;
  virtual ~IConditionTimerWatcher() = default;
  virtual std::string_view Condition() const = 0;
  virtual void Update(int duration, int remaining) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L146:INotifySupportPower
class INotifySupportPower {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifySupportPower;
  virtual ~INotifySupportPower() = default;
  virtual void Charged(Actor& self) = 0;
  virtual void Activated(Actor& self, const std::string& order_name) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L284-288:INotifyLoadCargo 族四件
/// (Load/Unload/Demolition/Infiltration —— Cloak 的 UncloakOn 事件面)
/// Mods.Common/TraitsInterfaces.cs L284-288: the INotifyLoadCargo family
/// of four (Load/Unload/Demolition/Infiltration — Cloak's UncloakOn event
/// faces).
class INotifyLoadCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyLoadCargo;
  virtual ~INotifyLoadCargo() = default;
  virtual void Loading(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L290-293
class INotifyUnloadCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyUnloadCargo;
  virtual ~INotifyUnloadCargo() = default;
  virtual void Unloading(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L296-299
class INotifyDemolition {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDemolition;
  virtual ~INotifyDemolition() = default;
  virtual void Demolishing(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L302-305
class INotifyInfiltration {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyInfiltration;
  virtual ~INotifyInfiltration() = default;
  virtual void Infiltrating(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L181:INotifyProximityOwnerChanged
/// (ProximityExternalCondition 的域内换主通知)
/// Mods.Common/TraitsInterfaces.cs L181: INotifyProximityOwnerChanged
/// (the in-range owner-change notification of ProximityExternalCondition).
class INotifyProximityOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyProximityOwnerChanged;
  virtual ~INotifyProximityOwnerChanged() = default;
  virtual void OnProximityOwnerChanged(Actor& actor, Player* old_owner,
                                       Player* new_owner) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L346-351:INotifyDeployTriggered(部署
/// 动画的播放面;实现者 = WithSpriteBody 等渲染 trait —— 渲染批接线,
/// 本批为空集)
/// Mods.Common/TraitsInterfaces.cs L346-351: INotifyDeployTriggered (the
/// deploy-animation play face; the implementors are the render traits —
/// WithSpriteBody & co. — riding the render batch; an empty set in this
/// batch).
class INotifyDeployTriggered {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDeployTriggered;
  virtual ~INotifyDeployTriggered() = default;
  virtual void Deploy(Actor& self, bool skip_make_anim) = 0;
  virtual void Undeploy(Actor& self, bool skip_make_anim) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L340-344:INotifyDeployComplete
class INotifyDeployComplete {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDeployComplete;
  virtual ~INotifyDeployComplete() = default;
  virtual void FinishedDeploy(Actor& self) = 0;
  virtual void FinishedUndeploy(Actor& self) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L510-515:IIssueDeployOrder(Deploy UI
/// 钮/热键的 order 面;net::Order 走文件头前向)
/// Mods.Common/TraitsInterfaces.cs L510-515: IIssueDeployOrder (the order
/// face of the Deploy UI button/hotkey; net::Order via the header's
/// forward declaration).
class IIssueDeployOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IIssueDeployOrder;
  virtual ~IIssueDeployOrder() = default;
  virtual net::Order IssueDeployOrder(Actor& self, bool queued) = 0;
  virtual bool CanIssueDeployOrder(Actor& self, bool queued) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L517-519:IDelayCarryallPickup(Carryall
/// 批的延迟拾取面;先行锚定)
/// Mods.Common/TraitsInterfaces.cs L517-519: IDelayCarryallPickup (the
/// carryall batch's delayed-pickup face; anchored ahead of its consumer).
class IDelayCarryallPickup {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDelayCarryallPickup;
  virtual ~IDelayCarryallPickup() = default;
  virtual bool TryLockForPickup(Actor& self, Actor& carrier) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L400-403:ITransformActorInitModifier
/// (Transform 时向新 actor 注入 init;GainsExperience 的 ExperienceInit)
/// Mods.Common/TraitsInterfaces.cs L400-403: ITransformActorInitModifier
/// (injecting inits into the new actor on Transform; GainsExperience's
/// ExperienceInit).
class ITransformActorInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITransformActorInitModifier;
  virtual ~ITransformActorInitModifier() = default;
  virtual void ModifyTransformActorInit(Actor& self,
                                        TypeDictionary& init) = 0;
};

/// Mods.Common/TraitsInterfaces.cs L474:IGivesExperienceModifier(OPT-A1
/// 修正链的 experience 域)
/// Mods.Common/TraitsInterfaces.cs L474: IGivesExperienceModifier (the
/// experience domain of the OPT-A1 modifier chain).
class IGivesExperienceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IGivesExperienceModifier;
  virtual ~IGivesExperienceModifier() = default;
  virtual int GetGivesExperienceModifier() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L477:IGainsExperienceModifier
class IGainsExperienceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IGainsExperienceModifier;
  virtual ~IGainsExperienceModifier() = default;
  virtual int GetGainsExperienceModifier() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L480:IDetectCloakedModifier(DetectCloaked
/// 的范围修正链)
/// Mods.Common/TraitsInterfaces.cs L480: IDetectCloakedModifier
/// (DetectCloaked's range modifier chain).
class IDetectCloakedModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDetectCloakedModifier;
  virtual ~IDetectCloakedModifier() = default;
  virtual int GetDetectCloakedModifier() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L751-754:IPreventMapSpawn(地图摆位的
/// 抑制面;上游 mods 无实现者 —— 空集承载)
/// Mods.Common/TraitsInterfaces.cs L751-754: IPreventMapSpawn (the
/// map-spawn suppression face; no upstream mod implementors — carried as
/// an empty set).
class IPreventMapSpawn {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPreventMapSpawn;
  virtual ~IPreventMapSpawn() = default;
};

/// CaptureManager.cs L30-33:ICaptureProgressWatcher(捕获进度条的观察
/// 面;UI 消费随 Phase 6)
/// CaptureManager.cs L30-33: ICaptureProgressWatcher (the capture-progress
/// observation face; the UI consumption rides Phase 6).
class ICaptureProgressWatcher {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ICaptureProgressWatcher;
  virtual ~ICaptureProgressWatcher() = default;
  virtual void Update(Actor& self, Actor& captor, Actor& target,
                      int progress, int total) = 0;
};

// ———— 第八批接口面(Render·WithSpriteBody 渲染族 +
//      ProximityCapturable 批;上游 TraitsInterfaces.cs 的渲染域)————
// ———— The batch-8 interface faces (the Render·WithSpriteBody family +
//      the ProximityCapturable batch; the render domain of upstream's
//      TraitsInterfaces.cs) ————

/// TraitsInterfaces.cs L113-117:IRender。上游 yield 惰性序列 → 出参
/// vector(RenderItem 为 POD,帧复用缓冲;yield 序 = push 序)。
/// TraitsInterfaces.cs L113-117: IRender. Upstream's lazy yield becomes
/// the out vector (RenderItem is a POD behind a frame-reused buffer; the
/// yield order = the push order).
class IRender {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IRender;
  virtual ~IRender() = default;
  virtual void Render(Actor& self, gfx::WorldRenderer& wr,
                      std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual std::vector<Rectangle> ScreenBounds(Actor& self,
                                              gfx::WorldRenderer& wr) = 0;
};

/// TraitsInterfaces.cs L119:IMouseBounds(多边形鼠标界;首非空者胜)
/// TraitsInterfaces.cs L119: IMouseBounds (the polygon mouse bounds; the
/// first non-empty one wins).
class IMouseBounds {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IMouseBounds;
  virtual ~IMouseBounds() = default;
  virtual Polygon MouseoverBounds(Actor& self, gfx::WorldRenderer* wr) = 0;
};

/// TraitsInterfaces.cs L121:IAutoMouseBounds(矩形自动界)
/// TraitsInterfaces.cs L121: IAutoMouseBounds (the rectangular auto bounds).
class IAutoMouseBounds {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAutoMouseBounds;
  virtual ~IAutoMouseBounds() = default;
  virtual Rectangle AutoMouseoverBounds(Actor& self,
                                        gfx::WorldRenderer* wr) = 0;
};

/// TraitsInterfaces.cs L456-460:IRenderAnnotations(注释层;消费 =
/// WorldRenderer::DrawAnnotations 的装配面)
/// TraitsInterfaces.cs L456-460: IRenderAnnotations (the annotation layer;
/// consumed by WorldRenderer::DrawAnnotations's assembly face).
class IRenderAnnotations {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IRenderAnnotations;
  virtual ~IRenderAnnotations() = default;
  virtual void RenderAnnotations(Actor& self, gfx::WorldRenderer& wr,
                                 std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual bool SpatiallyPartitionable() const = 0;
};

/// Mods.Common/TraitsInterfaces.cs L431-435
class IRenderInfantrySequenceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRenderInfantrySequenceModifier;
  virtual ~IRenderInfantrySequenceModifier() = default;
  virtual bool IsModifyingSequence() const = 0;
  virtual std::string_view SequencePrefix() const = 0;
};

// ———— 第九批接口面:ISelectionDecorations/IDecoration(注释·装饰渲染族)————
// ———— The batch-9 faces: ISelectionDecorations/IDecoration ————

/// TraitsInterfaces.cs L295-299(yield 序 = push 序)
/// TraitsInterfaces.cs L295-299 (the yield order = the push order).
class ISelectionDecorations {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ISelectionDecorations;
  virtual ~ISelectionDecorations() = default;
  virtual void RenderSelectionAnnotations(
      Actor& self, gfx::WorldRenderer& world_renderer, core::Color color,
      std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual int2 GetDecorationOrigin(Actor& self, gfx::WorldRenderer& wr,
                                   std::string_view pos, int2 margin) = 0;
};

/// Mods.Common/TraitsInterfaces.cs:IDecoration(RenderDecoration 出参 vector)
/// Mods.Common/TraitsInterfaces.cs: IDecoration (the out-vector face).
class IDecoration {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDecoration;
  virtual ~IDecoration() = default;
  virtual bool RequiresSelection() const = 0;
  virtual void RenderDecoration(Actor& self, gfx::WorldRenderer& wr,
                                ISelectionDecorations& container,
                                std::vector<gfx::RenderItem>& vec_out) = 0;
};

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
