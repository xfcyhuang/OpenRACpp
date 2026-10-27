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
class ActorInfo;
}

namespace ora::mods {
class Armament;

struct Barrel;
class DockClientManager;

}

namespace ora::mods::activities {
class MoveCooldownHelper;

}

namespace ora::mods::pathfinding {
class LocomotorInfo;
}

namespace ora::gfx {
class WorldRenderer;
struct RenderItem;

struct Sprite;
}

namespace ora::terrain {
class TerrainTemplateInfo;
struct TerrainTile;
}

namespace ora::net {
struct Order;
class OrderManager;
struct SessionClient;
}

namespace ora::sim {

class Actor;
class Player;
class World;
class Activity;
class TypeDictionary;
class TraitBase;

struct Target;

class PlayerBitMask {};

using PlayerMaskSet = LongBitSet<PlayerBitMask>;

class TargetableType {};
class DamageType {};
class CrushClass {};

enum class DamageState : std::int32_t {
  Undamaged = 1,
  Light = 2,
  Medium = 4,
  Heavy = 8,
  Critical = 16,
  Dead = 32,
};

enum class PlayerRelationship : std::int32_t {
  None = 0,
  Enemy = 1,
  Neutral = 2,
  Ally = 4,
};

inline bool HasRelationship(PlayerRelationship r, PlayerRelationship rel) {

  return (static_cast<std::int32_t>(r) & static_cast<std::int32_t>(rel)) ==
         static_cast<std::int32_t>(rel);
}

using SubCell = ora::SubCell;

enum class WinState : std::int32_t { Undefined, Won, Lost };

using ConditionCacheView = const std::map<std::string, int>&;

using VariableObserverNotifier =
    std::function<void(Actor& self, ConditionCacheView variables)>;

struct VariableObserver {
  VariableObserverNotifier fn_notifier;
  std::vector<std::string> vec_variables;
};

class IObservesVariables {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IObservesVariables;
  virtual ~IObservesVariables() = default;
  virtual std::vector<VariableObserver> GetVariableObservers() = 0;
};

class ITick {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITick;
  virtual ~ITick() = default;
  virtual void Tick(Actor& self) = 0;
};

class ITickRender {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITickRender;
  virtual ~ITickRender() = default;

  virtual void TickRender(Actor& self) = 0;
};

class INotifyCreated {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyCreated;
  virtual ~INotifyCreated() = default;
  virtual void Created(Actor& self) = 0;
};

class INotifyAddedToWorld {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyAddedToWorld;
  virtual ~INotifyAddedToWorld() = default;
  virtual void AddedToWorld(Actor& self) = 0;
};

class INotifyRemovedFromWorld {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyRemovedFromWorld;
  virtual ~INotifyRemovedFromWorld() = default;
  virtual void RemovedFromWorld(Actor& self) = 0;
};

class INotifyActorDisposing {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyActorDisposing;
  virtual ~INotifyActorDisposing() = default;
  virtual void Disposing(Actor& self) = 0;
};

class INotifyOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyOwnerChanged;
  virtual ~INotifyOwnerChanged() = default;
  virtual void OnOwnerChanged(Actor& self, Player& old_owner,
                              Player& new_owner) = 0;
};

class INotifyEffectiveOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyEffectiveOwnerChanged;
  virtual ~INotifyEffectiveOwnerChanged() = default;
  virtual void OnEffectiveOwnerChanged(Actor& self, Player& old_effective_owner,
                                       Player& new_effective_owner) = 0;
};

class INotifyOwnerLost {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyOwnerLost;
  virtual ~INotifyOwnerLost() = default;
  virtual void OnOwnerLost(Actor& self) = 0;
};

class INotifyBecomingIdle {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyBecomingIdle;
  virtual ~INotifyBecomingIdle() = default;
  virtual void OnBecomingIdle(Actor& self) = 0;
};

class INotifyIdle {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyIdle;
  virtual ~INotifyIdle() = default;
  virtual void TickIdle(Actor& self) = 0;
};

class IResolveOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IResolveOrder;
  virtual ~IResolveOrder() = default;
  virtual void ResolveOrder(Actor& self, const ora::net::Order& order) = 0;
};

class IValidateOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IValidateOrder;
  virtual ~IValidateOrder() = default;
  virtual bool OrderValidation(ora::net::OrderManager& order_manager,
                               World& world, int client_id,
                               const ora::net::Order& order) = 0;
};

class INotifyGameSaved {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_INotifyGameSaved;
  virtual ~INotifyGameSaved() = default;
  virtual void GameSaved(World& w, bool b_is_auto_save) = 0;
};

class ICreationActivity {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreationActivity;
  virtual ~ICreationActivity() = default;
  virtual Activity* GetCreationActivity() = 0;
};

class IActivityInterface {
 public:
  virtual ~IActivityInterface() = default;
};

class ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITraitInfoInterface;
  virtual ~ITraitInfoInterface() = default;
};

class IHealthInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IHealthInfo;
  virtual int MaxHP() const = 0;
};

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

class AttackInfo {
 public:
  const struct Damage* Damage = nullptr;
  const Actor* Attacker = nullptr;

  enum DamageState damage_state = DamageState::Undamaged;
  enum DamageState previous_damage_state = DamageState::Undamaged;
};

struct Damage {
  int Value = 0;
  core::BitSet<DamageType> DamageTypes;

  Damage() = default;
  Damage(int value, core::BitSet<DamageType> damage_types)
      : Value(value), DamageTypes(std::move(damage_types)) {}
  explicit Damage(int value) : Value(value) {}
};

class IEffectiveOwner {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IEffectiveOwner;
  virtual ~IEffectiveOwner() = default;
  virtual bool Disguised() const = 0;
  virtual Player* Owner() const = 0;
};

class IOccupySpaceInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOccupySpaceInfo;
};

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

class IFacing {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IFacing;
  virtual ~IFacing() = default;
  virtual WAngle TurnSpeed() const = 0;
  virtual WAngle Facing() const = 0;
  virtual void SetFacing(const WAngle& facing) = 0;
  virtual WRot Orientation() const = 0;
};

class ITargetable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITargetable;
  virtual ~ITargetable() = default;
  virtual core::BitSet<TargetableType> TargetTypes() const = 0;
  virtual bool TargetableBy(Actor& self, Actor& by_actor) = 0;
  virtual bool RequiresForceFire() const = 0;
};

class ITargetablePositions {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITargetablePositions;
  virtual ~ITargetablePositions() = default;
  virtual std::vector<WPos> TargetablePositions(Actor& self) = 0;
};

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

class ICreatePlayers {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreatePlayers;
  virtual ~ICreatePlayers() = default;
  virtual void CreatePlayers(World& world, MersenneTwister& player_random) = 0;
};

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

class IGameSaveTraitData {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IGameSaveTraitData;
  virtual ~IGameSaveTraitData() = default;
  virtual std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) = 0;
  virtual void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) = 0;
};

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

enum class MovementType : std::int32_t {
  None = 0,
  Horizontal = 1,
  Vertical = 2,
  Turn = 4,
};

inline bool HasMovementType(MovementType m, MovementType movement_type) {
  return (static_cast<std::int32_t>(m) &
          static_cast<std::int32_t>(movement_type)) ==
         static_cast<std::int32_t>(movement_type);
}

constexpr MovementType operator|(MovementType a, MovementType b) {
  return static_cast<MovementType>(static_cast<std::int32_t>(a) |
                                   static_cast<std::int32_t>(b));
}

enum class BlockedByActor : std::int32_t { None, Immovable, Stationary, All };

enum class TargetModifiers : std::int32_t {
  None = 0,
  ForceAttack = 1,
  ForceQueue = 2,
  ForceMove = 4,
};

constexpr PlayerRelationship operator|(PlayerRelationship a,
                                       PlayerRelationship b) {
  return static_cast<PlayerRelationship>(static_cast<std::int32_t>(a) |
                                         static_cast<std::int32_t>(b));
}

constexpr TargetModifiers operator|(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::int32_t>(a) |
                                      static_cast<std::int32_t>(b));
}

constexpr bool HasModifier(TargetModifiers m, TargetModifiers flag) {
  return (static_cast<std::int32_t>(m) &
          static_cast<std::int32_t>(flag)) != 0;
}

class ICustomMovementLayer;

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

  virtual void AddCellUpdatedListener(std::function<void(CPos)> fn) = 0;

  virtual std::span<ICustomMovementLayer* const> CustomMovementLayers()
      const = 0;
};

class IControlGroupsInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IControlGroupsInfo;
  virtual const std::vector<std::string>& Groups() const = 0;
};

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

class IAssignSpawnPoints {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAssignSpawnPoints;
  virtual ~IAssignSpawnPoints() = default;
  virtual CPos AssignHomeLocation(World& world, net::SessionClient& client,
                                  MersenneTwister& player_random) = 0;
  virtual int SpawnPointForPlayer(Player* player) = 0;
};

class IAssignSpawnPointsInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAssignSpawnPointsInfo;
};

class IBotInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IBotInfo;
  virtual std::string Type() const = 0;
  virtual std::string Name() const = 0;
};

class IBot {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IBot;
  virtual ~IBot() = default;
  virtual void Activate(Player* p) = 0;
  virtual void QueueOrder(net::Order* order) = 0;
  virtual const IBotInfo* Info() const = 0;
  virtual Player* GetPlayer() const = 0;
};

class IUnlocksRenderPlayer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IUnlocksRenderPlayer;
  virtual ~IUnlocksRenderPlayer() = default;
  virtual bool RenderPlayerUnlocked() const = 0;
};

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
  virtual std::unique_ptr<net::Order> IssueOrder(
      Actor& self, IOrderTargeter* order,
      const Target& target, bool queued) = 0;
};

class IOrderVoice {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IOrderVoice;
  virtual ~IOrderVoice() = default;
  virtual std::string VoicePhraseForOrder(Actor& self,
                                          const net::Order& order) = 0;
};

enum class MoveResult : std::int32_t {
  InProgress = 0,
  CompleteCanceled = 1,
  CompleteDestinationReached = 2,
  CompleteDestinationBlocked = 3,
};

class INotifyCustomLayerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCustomLayerChanged;
  virtual ~INotifyCustomLayerChanged() = default;
  virtual void CustomLayerChanged(Actor& self, std::uint8_t old_layer,
                                  std::uint8_t new_layer) = 0;
};

class INotifyCenterPositionChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyCenterPositionChanged;
  virtual ~INotifyCenterPositionChanged() = default;
  virtual void CenterPositionChanged(Actor& self, std::uint8_t old_layer,
                                     std::uint8_t new_layer) = 0;
};

class INotifyFinishedMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyFinishedMoving;
  virtual ~INotifyFinishedMoving() = default;
  virtual void FinishedMoving(Actor& self, std::uint8_t old_layer,
                              std::uint8_t new_layer) = 0;
};

class INotifyMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyMoving;
  virtual ~INotifyMoving() = default;
  virtual void MovementTypeChanged(Actor& self, MovementType type) = 0;
};

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

class INotifyBurstComplete {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyBurstComplete;
  virtual ~INotifyBurstComplete() = default;
  virtual void FiredBurst(Actor& self, const Target& target,
                          mods::Armament& armament) = 0;
};

class INotifyBlockingMove {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyBlockingMove;
  virtual ~INotifyBlockingMove() = default;
  virtual void OnNotifyBlockingMove(Actor& self, Actor& blocking) = 0;
};

class IDeathActorInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDeathActorInitModifier;
  virtual ~IDeathActorInitModifier() = default;
  virtual void ModifyDeathActorInit(Actor& self, TypeDictionary& init) = 0;
};

class IActorPreviewInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IActorPreviewInitModifier;
  virtual ~IActorPreviewInitModifier() = default;
  virtual void ModifyActorPreviewInit(Actor& self, TypeDictionary& inits) = 0;
};

class ISpeedModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ISpeedModifier;
  virtual ~ISpeedModifier() = default;
  virtual int GetSpeedModifier() = 0;
};

class IFirepowerModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IFirepowerModifier;
  virtual ~IFirepowerModifier() = default;
  virtual int GetFirepowerModifier() = 0;
};

class IReloadModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IReloadModifier;
  virtual ~IReloadModifier() = default;
  virtual int GetReloadModifier() = 0;
};

class IInaccuracyModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IInaccuracyModifier;
  virtual ~IInaccuracyModifier() = default;
  virtual int GetInaccuracyModifier() = 0;
};

class IRangeModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRangeModifier;
  virtual ~IRangeModifier() = default;
  virtual int GetRangeModifier() = 0;
};

class IRangeModifierInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRangeModifierInfo;
  virtual int GetRangeModifierDefault() = 0;
};

class IPositionableInfo : public IOccupySpaceInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPositionableInfo;
  virtual bool CanEnterCell(World& world, Actor* self, CPos cell,
                            SubCell sub_cell, Actor* ignore_actor,
                            BlockedByActor check) = 0;
};

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

class IMoveInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IMoveInfo;
};

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
  virtual Activity* MoveToTarget(Actor* self, const Target& target,
                                 std::optional<WPos> initial_target_position = {},
                                 std::optional<core::Color> target_line_color = {}) = 0;
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

class IWrapMove {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IWrapMove;
  virtual ~IWrapMove() = default;
  virtual Activity* WrapMove(Activity* move_inner) = 0;
};

class SequenceSet;

class IQuantizeBodyOrientationInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IQuantizeBodyOrientationInfo;
  virtual int QuantizedBodyFacings(const game::ActorInfo& ai,
                                   const SequenceSet& sequences,
                                   const std::string& faction) = 0;
};

class ITemporaryBlocker {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITemporaryBlocker;
  virtual ~ITemporaryBlocker() = default;
  virtual bool CanRemoveBlockage(Actor& self, Actor& blocking) = 0;
  virtual bool IsBlocking(Actor& self, CPos cell) = 0;
};

class INotifyAiming {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyAiming;
  virtual ~INotifyAiming() = default;
  virtual void StartedAiming(Actor& self, TraitBase* attack) = 0;
  virtual void StoppedAiming(Actor& self, TraitBase* attack) = 0;
};

class IOverrideAutoTarget {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IOverrideAutoTarget;
  virtual ~IOverrideAutoTarget() = default;
  virtual std::optional<struct Target> TryGetAutoTargetOverride(
      Actor& self) = 0;
};

class IDisableEnemyAutoTarget {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDisableEnemyAutoTarget;
  virtual ~IDisableEnemyAutoTarget() = default;
  virtual bool DisableEnemyAutoTarget(Actor& self, Actor& attacker) = 0;
};

class IRevealsShroudModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRevealsShroudModifier;
  virtual ~IRevealsShroudModifier() = default;
  virtual int GetRevealsShroudModifier() const = 0;
};

class ITargetableCells {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITargetableCells;
  virtual ~ITargetableCells() = default;
  virtual std::vector<std::pair<CPos, SubCell>> TargetableCells() = 0;
};

class IBlocksProjectilesInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IBlocksProjectilesInfo;
};

class IBlocksProjectiles {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IBlocksProjectiles;
  virtual ~IBlocksProjectiles() = default;
  virtual WDist BlockingHeight() const = 0;
  virtual PlayerRelationship ValidRelationships() const = 0;
};

class INotifySold {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifySold;
  virtual ~INotifySold() = default;
  virtual void Selling(Actor& self) = 0;
  virtual void Sold(Actor& self) = 0;
};

class INotifyTransform {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyTransform;
  virtual ~INotifyTransform() = default;
  virtual void BeforeTransform(Actor& self) = 0;
  virtual void OnTransform(Actor& self) = 0;
  virtual void AfterTransform(Actor& to_actor) = 0;
};

class INotifyProduction {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyProduction;
  virtual ~INotifyProduction() = default;
  virtual void UnitProduced(Actor& self, Actor& other, CPos exit) = 0;
};

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

class ITechTreePrerequisiteInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITechTreePrerequisiteInfo;
  virtual ~ITechTreePrerequisiteInfo() = default;
  virtual std::vector<std::string> Prerequisites(
      const game::ActorInfo& info) = 0;
};

class ITechTreePrerequisite {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITechTreePrerequisite;
  virtual ~ITechTreePrerequisite() = default;
  virtual std::vector<std::string> ProvidesPrerequisites() = 0;
};

class INotifyPowerLevelChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyPowerLevelChanged;
  virtual ~INotifyPowerLevelChanged() = default;
  virtual void PowerLevelChanged(Actor& self) = 0;
};

class IPowerModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPowerModifier;
  virtual ~IPowerModifier() = default;
  virtual int GetPowerModifier() = 0;
};

class IResourceValueModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IResourceValueModifier;
  virtual ~IResourceValueModifier() = default;
  virtual int GetResourceValueModifier() = 0;
};

class INotifyResourceAccepted {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyResourceAccepted;
  virtual ~INotifyResourceAccepted() = default;
  virtual void OnResourceAccepted(Actor& self, Actor& refinery,
                                  const std::string& resource_type, int count,
                                  int value) = 0;
};

class INotifyHarvestAction {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyHarvestAction;
  virtual ~INotifyHarvestAction() = default;
  virtual void Harvested(Actor& self, const std::string& resource_type) = 0;
  virtual void MovingToResources(Actor& self, CPos target_cell) = 0;
  virtual void MovementCancelled(Actor& self) = 0;
};

class CaptureType {};

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

struct ResourceLayerContents {

  static const ResourceLayerContents& Empty();

  std::string str_type;
  std::uint8_t uint1_density = 0;
};

inline const ResourceLayerContents& ResourceLayerContents::Empty() {
  static const ResourceLayerContents k_empty{};
  return k_empty;
}

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

class ISeedableResource {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ISeedableResource;
  virtual ~ISeedableResource() = default;
  virtual void Seed(Actor& self) = 0;
};

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

class IStoresResourcesInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IStoresResourcesInfo;
  virtual std::vector<std::string> ResourceTypes() = 0;
};

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

class IAcceptResources {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IAcceptResources;
  virtual ~IAcceptResources() = default;
  virtual int AcceptResources(Actor& self, const std::string& resource_type,
                              int count = 1) = 0;
};

class DockType {};

class IDockClientInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClientInfo;
};

class IDockHost;

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

class IDockClientManagerInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDockClientManagerInfo;
};

class INotifyDockHost {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockHost;
  virtual ~INotifyDockHost() = default;
  virtual void Docked(Actor& self, Actor& client) = 0;
  virtual void Undocked(Actor& self, Actor& client) = 0;
};

class INotifyDockClient {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockClient;
  virtual ~INotifyDockClient() = default;
  virtual void Docked(Actor& self, Actor& host) = 0;
  virtual void Undocked(Actor& self, Actor& host) = 0;
};

class INotifyDockClientMoving {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDockClientMoving;
  virtual ~INotifyDockClientMoving() = default;
  virtual void MovingToDock(Actor& self, Actor& host_actor,
                            IDockHost* host) = 0;
  virtual void MovementCancelled(Actor& self) = 0;
};

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

struct TraitUpcastEntry;

class ISelectionBar {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ISelectionBar;
  virtual ~ISelectionBar() = default;
  virtual float GetValue() = 0;
  virtual core::Color GetColor() = 0;
  virtual bool DisplayWhenEmpty() const = 0;
};

enum class SelectionPriorityModifiers : std::int32_t {
  None = 0,
  Ctrl = 1,
  Alt = 2,
};

class ISelectableInfo : public ITraitInfoInterface {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ISelectableInfo;
  virtual int Priority() const = 0;
  virtual SelectionPriorityModifiers PriorityModifiers() const = 0;
  virtual std::string_view Voice() const = 0;
};

class ISelectable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ISelectable;
  virtual ~ISelectable() = default;
  virtual std::string_view Class() const = 0;
};

class IVoiced {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IVoiced;
  virtual ~IVoiced() = default;
  virtual std::string_view VoiceSet() const = 0;
  virtual bool PlayVoice(Actor& self, std::string_view str_phrase,
                         std::string_view str_variant) = 0;
  virtual bool PlayVoiceLocal(Actor& self, std::string_view str_phrase,
                              std::string_view str_variant, float fp4_volume) = 0;
  virtual bool HasVoice(Actor& self, std::string_view str_voice) = 0;
};

class INotifyOrderIssued {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyOrderIssued;
  virtual ~INotifyOrderIssued() = default;
  virtual bool OrderIssued(World& world, std::string_view str_order_string,
                           const Target& target) = 0;
};

class IProvideTooltipInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IProvideTooltipInfo;
  virtual ~IProvideTooltipInfo() = default;
  virtual bool IsTooltipVisible(Player* for_player) = 0;
  virtual std::string_view TooltipText() const = 0;
};

class IConditionTimerWatcher {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IConditionTimerWatcher;
  virtual ~IConditionTimerWatcher() = default;
  virtual std::string_view Condition() const = 0;
  virtual void Update(int duration, int remaining) = 0;
};

class INotifySupportPower {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifySupportPower;
  virtual ~INotifySupportPower() = default;
  virtual void Charged(Actor& self) = 0;
  virtual void Activated(Actor& self, const std::string& order_name) = 0;
};

class INotifyLoadCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyLoadCargo;
  virtual ~INotifyLoadCargo() = default;
  virtual void Loading(Actor& self) = 0;
};

class INotifyUnloadCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyUnloadCargo;
  virtual ~INotifyUnloadCargo() = default;
  virtual void Unloading(Actor& self) = 0;
};

class INotifyDemolition {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDemolition;
  virtual ~INotifyDemolition() = default;
  virtual void Demolishing(Actor& self) = 0;
};

class INotifyPassengerEntered {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyPassengerEntered;
  virtual ~INotifyPassengerEntered() = default;
  virtual void OnPassengerEntered(Actor& self, Actor& passenger) = 0;
};

class INotifyPassengerExited {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyPassengerExited;
  virtual ~INotifyPassengerExited() = default;
  virtual void OnPassengerExited(Actor& self, Actor& passenger) = 0;
};

class INotifyEnteredCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyEnteredCargo;
  virtual ~INotifyEnteredCargo() = default;
  virtual void OnEnteredCargo(Actor& self, Actor& cargo) = 0;
};

class INotifyExitedCargo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyExitedCargo;
  virtual ~INotifyExitedCargo() = default;
  virtual void OnExitedCargo(Actor& self, Actor& cargo) = 0;
};

class INotifyParachute {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyParachute;
  virtual ~INotifyParachute() = default;
  virtual void OnParachute(Actor& self) = 0;
  virtual void OnLanded(Actor& self) = 0;
};

class INotifyInfiltration {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyInfiltration;
  virtual ~INotifyInfiltration() = default;
  virtual void Infiltrating(Actor& self) = 0;
};

class INotifyProximityOwnerChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyProximityOwnerChanged;
  virtual ~INotifyProximityOwnerChanged() = default;
  virtual void OnProximityOwnerChanged(Actor& actor, Player* old_owner,
                                       Player* new_owner) = 0;
};

class INotifyDeployTriggered {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDeployTriggered;
  virtual ~INotifyDeployTriggered() = default;
  virtual void Deploy(Actor& self, bool skip_make_anim) = 0;
  virtual void Undeploy(Actor& self, bool skip_make_anim) = 0;
};

class INotifyDeployComplete {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyDeployComplete;
  virtual ~INotifyDeployComplete() = default;
  virtual void FinishedDeploy(Actor& self) = 0;
  virtual void FinishedUndeploy(Actor& self) = 0;
};

class IIssueDeployOrder {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IIssueDeployOrder;
  virtual ~IIssueDeployOrder() = default;
  virtual net::Order IssueDeployOrder(Actor& self, bool queued) = 0;
  virtual bool CanIssueDeployOrder(Actor& self, bool queued) = 0;
};

class IDelayCarryallPickup {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDelayCarryallPickup;
  virtual ~IDelayCarryallPickup() = default;
  virtual bool TryLockForPickup(Actor& self, Actor& carrier) = 0;
};

class ITransformActorInitModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITransformActorInitModifier;
  virtual ~ITransformActorInitModifier() = default;
  virtual void ModifyTransformActorInit(Actor& self,
                                        TypeDictionary& init) = 0;
};

class IGivesExperienceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IGivesExperienceModifier;
  virtual ~IGivesExperienceModifier() = default;
  virtual int GetGivesExperienceModifier() const = 0;
};

class IGainsExperienceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IGainsExperienceModifier;
  virtual ~IGainsExperienceModifier() = default;
  virtual int GetGainsExperienceModifier() const = 0;
};

class IDetectCloakedModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IDetectCloakedModifier;
  virtual ~IDetectCloakedModifier() = default;
  virtual int GetDetectCloakedModifier() const = 0;
};

class IPreventMapSpawn {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IPreventMapSpawn;
  virtual ~IPreventMapSpawn() = default;
};

class ICaptureProgressWatcher {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ICaptureProgressWatcher;
  virtual ~ICaptureProgressWatcher() = default;
  virtual void Update(Actor& self, Actor& captor, Actor& target,
                      int progress, int total) = 0;
};

class IRender {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IRender;
  virtual ~IRender() = default;
  virtual void Render(Actor& self, gfx::WorldRenderer& wr,
                      std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual std::vector<Rectangle> ScreenBounds(Actor& self,
                                              gfx::WorldRenderer& wr) = 0;
};

class IMouseBounds {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IMouseBounds;
  virtual ~IMouseBounds() = default;
  virtual Polygon MouseoverBounds(Actor& self, gfx::WorldRenderer* wr) = 0;
};

class IAutoMouseBounds {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IAutoMouseBounds;
  virtual ~IAutoMouseBounds() = default;
  virtual Rectangle AutoMouseoverBounds(Actor& self,
                                        gfx::WorldRenderer* wr) = 0;
};

class IRenderAnnotations {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_IRenderAnnotations;
  virtual ~IRenderAnnotations() = default;
  virtual void RenderAnnotations(Actor& self, gfx::WorldRenderer& wr,
                                 std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual bool SpatiallyPartitionable() const = 0;
};

class IRenderInfantrySequenceModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRenderInfantrySequenceModifier;
  virtual ~IRenderInfantrySequenceModifier() = default;
  virtual bool IsModifyingSequence() const = 0;
  virtual std::string_view SequencePrefix() const = 0;
};

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
  virtual gen::TypeId GetTraitTypeId() const = 0;

  virtual std::span<const TraitUpcastEntry> TraitUpcasts() const = 0;

  virtual bool IsTraitEnabled() const { return true; }
  virtual bool IsTraitDisabled() const { return false; }
};

struct TraitUpcastEntry {
  gen::TypeId type_id;
  void* (*upcast)(void*);
};

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

#define ORA_TRAIT_INTERFACES(TraitT, TypeIdEnum, ...)                     \
  static constexpr gen::TypeId kTypeId = gen::TypeId::TypeIdEnum;        \
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }        \
  static constexpr auto kTraitUpcasts =                                 \
      ora::sim::MakeTraitUpcasts<TraitT, __VA_ARGS__>();\
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {\
    return kTraitUpcasts;\
  }

class ITiledTerrainRenderer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ITiledTerrainRenderer;
  virtual ~ITiledTerrainRenderer() = default;
  virtual const gfx::Sprite* MissingTile() = 0;
  virtual const gfx::Sprite* TileSprite(terrain::TerrainTile tile,
                                        const int* ptr_variant) = 0;
  virtual Rectangle TemplateBounds(
      const terrain::TerrainTemplateInfo& template_info) = 0;
  virtual void RenderUIPreview(gfx::WorldRenderer& wr,
                               const terrain::TerrainTemplateInfo& template_info,
                               int2 origin, float scale,
                               std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual void RenderPreview(gfx::WorldRenderer& wr,
                             const terrain::TerrainTemplateInfo& template_info,
                             WPos origin, std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual void RenderPreview(gfx::WorldRenderer& wr, terrain::TerrainTile tile,
                             WPos origin, std::vector<gfx::RenderItem>& vec_out) = 0;
};

class IResourceRenderer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IResourceRenderer;
  virtual ~IResourceRenderer() = default;
  virtual std::vector<std::string> ResourceTypes() = 0;
  virtual std::string GetRenderedResourceType(CPos cell) = 0;
  virtual std::string GetRenderedResourceTooltip(CPos cell) = 0;
  virtual void RenderUIPreview(gfx::WorldRenderer& wr,
                               const std::string& resource_type, int2 origin,
                               float scale,
                               std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual void RenderPreview(gfx::WorldRenderer& wr,
                             const std::string& resource_type, WPos origin,
                             std::vector<gfx::RenderItem>& vec_out) = 0;
};

class IProductionIconOverlay {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IProductionIconOverlay;
  virtual ~IProductionIconOverlay() = default;
  virtual const gfx::Sprite* OverlaySprite() const = 0;
  virtual std::string_view OverlayPalette() const = 0;
  virtual core::Vector2 OverlayOffset(core::Vector2 vec_icon_size) const = 0;
  virtual bool IsOverlayActive(const game::ActorInfo& ai) const = 0;
};

class INotifyLineBuildSegmentsChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyLineBuildSegmentsChanged;
  virtual ~INotifyLineBuildSegmentsChanged() = default;
  virtual void SegmentAdded(Actor& self, Actor* segment) = 0;
  virtual void SegmentRemoved(Actor& self, Actor* segment) = 0;
};

class INotifyBuildingPlaced {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyBuildingPlaced;
  virtual ~INotifyBuildingPlaced() = default;
  virtual void BuildingPlaced(Actor& self, Actor& placed) = 0;
};

class IRadarSignature {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRadarSignature;
  virtual ~IRadarSignature() = default;
  virtual void PopulateRadarSignatureCells(
      Actor& self, std::vector<std::pair<CPos, core::Color>>& vec_dest) = 0;
};

class IRadarColorModifier {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRadarColorModifier;
  virtual ~IRadarColorModifier() = default;
  virtual core::Color RadarColorOverride(Actor& self, core::Color color) = 0;
};

class IRadarTerrainLayer {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IRadarTerrainLayer;
  virtual ~IRadarTerrainLayer() = default;
  virtual std::size_t SubscribeCellEntryChanged(std::function<void(CPos)> fn) = 0;
  virtual void RemoveCellEntryChangedListener(std::size_t n_token) = 0;
  virtual bool TryGetTerrainColorPair(
      MPos uv, std::optional<std::pair<core::Color, core::Color>>& out_pair) = 0;
};

class INotifyWinStateChanged {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyWinStateChanged;
  virtual ~INotifyWinStateChanged() = default;
  virtual void OnPlayerLost(Player& player) = 0;
  virtual void OnPlayerWon(Player& player) = 0;
};

class INotifyObjectivesUpdated {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyObjectivesUpdated;
  virtual ~INotifyObjectivesUpdated() = default;
  virtual void OnObjectiveAdded(Player& player, int objective_id) = 0;
  virtual void OnObjectiveCompleted(Player& player, int objective_id) = 0;
  virtual void OnObjectiveFailed(Player& player, int objective_id) = 0;
};

class ITooltip {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_ITooltip;
  virtual ~ITooltip() = default;
  virtual const class ITooltipInfo* TooltipInfo() const = 0;
  virtual Player* Owner() const = 0;
};

class ITooltipInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ITooltipInfo;
  virtual ~ITooltipInfo() = default;
  virtual std::string TooltipForPlayerStance(PlayerRelationship stance) = 0;
  virtual bool IsOwnerRowVisible() const = 0;
};

class INotifyTakeOff {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyTakeOff;
  virtual ~INotifyTakeOff() = default;
  virtual void TakeOff(Actor& self) = 0;
};

class INotifyLanding {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyLanding;
  virtual ~INotifyLanding() = default;
  virtual void Landing(Actor& self) = 0;
};

class IAircraftCenterPositionOffset {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IAircraftCenterPositionOffset;
  virtual ~IAircraftCenterPositionOffset() = default;
  virtual WVec PositionOffset() const = 0;
};

class IOverrideAircraftLanding {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IOverrideAircraftLanding;
  virtual ~IOverrideAircraftLanding() = default;
  virtual std::span<const std::string> LandableTerrainTypes() = 0;
};

enum class ResupplyType : std::int32_t {
  None = 0,
  Rearm = 1,
  Repair = 2,
};

inline constexpr ResupplyType operator|(ResupplyType a, ResupplyType b) {
  return static_cast<ResupplyType>(static_cast<std::int32_t>(a) |
                                   static_cast<std::int32_t>(b));
}

inline constexpr ResupplyType operator&(ResupplyType a, ResupplyType b) {
  return static_cast<ResupplyType>(static_cast<std::int32_t>(a) &
                                   static_cast<std::int32_t>(b));
}

inline constexpr ResupplyType operator~(ResupplyType a) {
  return static_cast<ResupplyType>(~static_cast<std::int32_t>(a));
}

inline constexpr ResupplyType& operator|=(ResupplyType& a, ResupplyType b) {
  return a = a | b;
}

inline constexpr ResupplyType& operator&=(ResupplyType& a, ResupplyType b) {
  return a = a & b;
}

class INotifyResupply {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_INotifyResupply;
  virtual ~INotifyResupply() = default;
  virtual void BeforeResupply(Actor& self, Actor& unit,
                              ResupplyType types) = 0;
  virtual void ResupplyTick(Actor& self, Actor& unit, ResupplyType types) = 0;
};

class ICallForTransport {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ICallForTransport;
  virtual ~ICallForTransport() = default;
  virtual WDist MinimumDistance() const = 0;
  virtual bool WantsTransport() const = 0;
  virtual void MovementCancelled(Actor& self) = 0;
  virtual void RequestTransport(Actor& self, CPos destination) = 0;
};

}
