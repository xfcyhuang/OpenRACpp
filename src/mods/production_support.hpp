// UPSTREAM: OpenRA.Mods.Common/Traits/Valued.cs @b6fc03f L17-25 +
//          Traits/Buildable.cs L15-69 + Traits/Buildings/Exit.cs L19-95 +
//          Traits/Buildings/Reservable.cs L19-106 + Traits/Buildings/
//          RallyPoint.cs L19-199 + Traits/Player/ProvidesPrerequisite.cs
//          L17-107 + Traits/Player/TechTree.cs L17-200 + Traits/Player/
//          DeveloperMode.cs 的同步字段面(L17-60)
//          Valued.cs L17-25 + Buildable.cs L15-69 + Buildings/Exit.cs
//          L19-95 + Buildings/Reservable.cs L19-106 + Buildings/
//          RallyPoint.cs L19-199 + Player/ProvidesPrerequisite.cs L17-107 +
//          Player/TechTree.cs L17-200 + DeveloperMode.cs's synchronous
//          field face (L17-60).
//
// 机制对照 / Mechanism mapping:
//  - DeveloperMode:本批最小承载(FastBuild/AllTech 两 cheat 字段的同步
//    面;IResolveOrder 的 cheat 命令族/ILobbyOptions 随 Phase 6/7;
//    默认全 false = 上游 CheckboxEnabled=false 的等价域 —— 登记 COVERAGE)
//    DeveloperMode: this batch's minimal carrier (the synchronous face
//    of the FastBuild/AllTech cheat fields; the IResolveOrder cheat
//    command family/ILobbyOptions ride Phase 6/7; the all-false default
//    = upstream's CheckboxEnabled=false equivalent domain — registered
//    in COVERAGE).
//  - Reservable 的 Aircraft 面未移植:Reserve/UnReserve 的航空器调度
//    (AttackMoveActivity/TakeOff 排队)为空集,IsReserved 走
//    MayYieldReservation 恒假分支(IsReserved = reservedForAircraft !=
//    null 的等价面;登记 COVERAGE)
//    Reservable's Aircraft face is unported: Reserve/UnReserve's
//    aircraft scheduling (the AttackMoveActivity/TakeOff queueing) is an
//    empty set; IsReserved takes the MayYieldReservation-constantly-false
//    branch (the equivalent face of IsReserved = reservedForAircraft !=
//    null; registered in COVERAGE).
//  - RallyPoint 的 RallyPointIndicator 效果/IIssueOrder 的声音与文本
//    通知随渲染/声音注入面
//    RallyPoint's RallyPointIndicator effect/IIssueOrder's sound and
//    text notifications ride the render/sound injection faces.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
class World;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::net {
struct Order;
}

namespace ora::mods {

// ———— Valued(L17-25)————
// ———— Valued (L17-25) ————

struct ValuedInfoData {
  int int4_cost = 0;  // L22 Cost([FieldLoader.Require])
  static ValuedInfoData Parse(const meta::RecordObject& rec_info);
};

/// Valued(空运行时类;数据面在 Info)
/// Valued (the empty runtime class; the data face lives in the Info).
class Valued final : public sim::TraitBase {
 public:
  // 上游 TraitInfo<Valued> 直查具体类;无接口
  // Upstream's TraitInfo<Valued> queries the concrete class directly; no
  // interfaces.
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Valued;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Valued>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

// ———— Buildable(L15-69)————
// ———— Buildable (L15-69) ————

struct BuildableInfoData {
  std::vector<std::string> vec_prerequisites;  // L20
  std::vector<std::string> vec_queue;          // L23 Queue
  std::string str_build_at_production_type;    // L26
  int int4_build_limit = 0;                    // L29
  std::string str_force_faction;               // L32
  std::string str_icon{"icon"};                // L35
  std::string str_icon_palette{"chrome"};      // L38
  bool b_icon_palette_is_player_palette = false;  // L41
  int int4_build_duration = -1;                // L44
  int int4_build_duration_modifier = 60;       // L47
  int int4_build_palette_order = 9999;         // L50
  std::string str_description;                 // L53

  static BuildableInfoData Parse(const meta::RecordObject& rec_info);

  /// L57-60:GetInitialFaction
  /// L57-60: GetInitialFaction.
  static std::string GetInitialFaction(const game::ActorInfo& ai,
                                       const std::string& default_faction);
};

/// Buildable(空运行时类)
/// Buildable (the empty runtime class).
class Buildable final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Buildable;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Buildable>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

// ———— Exit(L19-95)————
// ———— Exit (L19-95) ————

struct ExitInfoData {
  WVec spawn_offset{0, 0, 0};       // L24
  CVec exit_cell{0, 0};             // L27
  std::optional<WAngle> opt_facing;  // L29
  std::vector<std::string> vec_production_types;  // L32
  int int4_exit_delay = 0;                        // L35
  int int4_priority = 1;                          // L38
  sim::ConditionalTraitData conditional;          // 基类字段

  static ExitInfoData Parse(const meta::RecordObject& rec_info);
};

/// Exit(L41-45;ConditionalTrait<ExitInfo> 的 CRTP 形)
/// Exit (L41-45; ConditionalTrait<ExitInfo>'s CRTP shape).
class Exit final : public sim::TraitBase,
                   public sim::ConditionalTraitCore<Exit>,
                   public sim::IObservesVariables,
                   public sim::INotifyCreated {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Exit;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Exit, sim::IObservesVariables,
                                 sim::INotifyCreated>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<Exit>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return ConditionalTraitCore<Exit>::CollectObservers();
  }

  void Created(sim::Actor& self) {
    ConditionalTraitCore<Exit>::CoreCreated(self);
  }

  explicit Exit(ExitInfoData info)
      : ConditionalTraitCore<Exit>{info.conditional},
        info_{std::move(info)} {}

  const ExitInfoData& Info() const { return info_; }

 private:
  ExitInfoData info_;
};

// ———— Reservable(L19-106)————
// ———— Reservable (L19-106) ————

/// Reservable(Aircraft 面空集的承载;IsReserved/IsAvailableFor 的判定面)
/// Reservable (the carrier with the empty Aircraft face; the
/// IsReserved/IsAvailableFor predicate faces).
class Reservable final : public sim::TraitBase,
                         public sim::ITick,
                         public sim::INotifyOwnerChanged,
                         public sim::INotifySold,
                         public sim::INotifyActorDisposing,
                         public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      Reservable, OpenRA_Mods_Common_Traits_Reservable, sim::ITick,
      sim::INotifyOwnerChanged, sim::INotifySold,
      sim::INotifyActorDisposing, sim::INotifyCreated)

  void Tick(sim::Actor& self) override;  // L40-53
  void Created(sim::Actor& self) override {}  // L33-38(RallyPoint 面空)
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L104
  void Selling(sim::Actor& self) override;               // L105
  void Sold(sim::Actor& self) override;                  // L106
  void Disposing(sim::Actor& self) override;             // L103

  /// L66-69:static IsReserved(Aircraft 空集 → reserved_for != null 等价)
  /// L66-69: static IsReserved (the empty Aircraft set → the
  /// reserved_for != null equivalent).
  static bool IsReserved(const sim::Actor& a);

  /// L71-74:static IsAvailableFor
  /// L71-74: static IsAvailableFor.
  static bool IsAvailableFor(sim::Actor& reservable,
                             sim::Actor* for_actor);

  /// L55-64:Reserve(Aircraft 面空集:登记 forActor;DisposableAction
  /// 的 GC 面 = 布尔登记)
  /// L55-64: Reserve (the empty Aircraft face: record forActor; the
  /// DisposableAction's GC face = the boolean record).
  void Reserve(sim::Actor* for_actor);

 private:
  /// L95-102:UnReserve(Aircraft 调度空集;仅清登记)
  /// L95-102: UnReserve (the aircraft scheduling is empty; only the
  /// record clears).
  void UnReserve();

  sim::Actor* p_reserved_for_ = nullptr;
};

// ———— RallyPoint(L19-199)————
// ———— RallyPoint (L19-199) ————

struct RallyPointInfoData {
  std::vector<CVec> vec_path;  // L48

  static RallyPointInfoData Parse(const meta::RecordObject& rec_info);
};

/// RallyPoint(L55-90 的同步面;IIssueOrder/RallyPointIndicator 随 UI 批)
/// RallyPoint (the synchronous face of L55-90; the IIssueOrder/
/// RallyPointIndicator ride the UI batch).
class RallyPoint final : public sim::TraitBase,
                         public sim::IResolveOrder,
                         public sim::INotifyOwnerChanged,
                         public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      RallyPoint, OpenRA_Mods_Common_Traits_RallyPoint, sim::IResolveOrder,
      sim::INotifyOwnerChanged, sim::INotifyCreated)

  RallyPoint(sim::Actor& self, RallyPointInfoData info);

  /// L63-66:ResetPath
  /// L63-66: ResetPath.
  void ResetPath(sim::Actor& self);

  void Created(sim::Actor& self) override {}  // L72-75(Indicator 视觉)
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L77-82
  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;  // L94-113

  std::vector<CPos> Path;

  /// L115-118:static IsForceSet
  /// L115-118: static IsForceSet.
  static bool IsForceSet(const ora::net::Order& order);

 private:
  RallyPointInfoData info_;
};

// ———— ProvidesPrerequisite(L17-107)————
// ———— ProvidesPrerequisite (L17-107) ————

class TechTree;  // 本文件后段定义 | defined later in this file.

struct ProvidesPrerequisiteInfoData {
  std::string str_prerequisite;                // L21
  std::vector<std::string> vec_requires_prerequisites;  // L24
  std::vector<std::string> vec_factions;       // L27
  bool b_reset_on_owner_change = false;        // L30
  sim::ConditionalTraitData conditional;       // 基类字段

  static ProvidesPrerequisiteInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// ProvidesPrerequisite(L35-107)
class ProvidesPrerequisite final : public sim::TraitBase,
                                   public sim::ConditionalTraitCore<
                                       ProvidesPrerequisite>,
                                   public sim::ITechTreePrerequisite,
                                   public sim::INotifyOwnerChanged,
                                   public sim::INotifyCreated,
                                   public sim::IObservesVariables {
 public:
  ORA_TRAIT_INTERFACES(
      ProvidesPrerequisite, OpenRA_Mods_Common_Traits_ProvidesPrerequisite,
      sim::ITechTreePrerequisite, sim::INotifyOwnerChanged,
      sim::INotifyCreated, sim::IObservesVariables)

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<
        ProvidesPrerequisite>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return ConditionalTraitCore<ProvidesPrerequisite>::
        CollectObservers();
  }

  ProvidesPrerequisite(sim::ActorInitializer& init,
                       ProvidesPrerequisiteInfoData info);

  void Created(sim::Actor& self) override;  // L52-60
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L77-86
  std::vector<std::string> ProvidesPrerequisites()
      override;  // L44(属性面)

  /// L88-105:Update(技术树联动)
  /// L88-105: Update (the tech-tree interplay).
  void Update();

 private:
  std::vector<std::string> vec_prerequisites_;  // L37
  bool b_enabled_ = false;
  TechTree* p_tech_tree_ = nullptr;
  std::string str_faction_;
  ProvidesPrerequisiteInfoData info_;
};

// ———— TechTree(L17-200)————
// ———— TechTree (L17-200) ————

class TechTree final : public sim::TraitBase {
 public:
  // 上游无接口(Trait<TechTree> 具体类直查);PlayerActor 挂载
  // Upstream has no interface (the concrete-class Trait<TechTree> query);
  // mounted on the PlayerActor.
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_TechTree;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<TechTree>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  explicit TechTree(sim::ActorInitializer& init);

  /// L33-40:ActorChanged(ActorAdded/Removed 订阅回调)
  /// L33-40: ActorChanged (the ActorAdded/Removed subscription
  /// callback).
  void ActorChanged(sim::Actor& a);

  /// L42-48:Update
  /// L42-48: Update.
  void Update();

  /// L50-53:Add
  /// L50-53: Add.
  void Add(const std::string& key,
           const std::vector<std::string>& vec_prerequisites, int limit,
           sim::ITechTreeElement* tte);

  /// L55-57:Remove(key)
  /// L55-57: Remove (key).
  void Remove(const std::string& key);

  /// L59-62:Remove(ITechTreeElement)
  /// L59-62: Remove (ITechTreeElement).
  void RemoveElement(sim::ITechTreeElement* tte);

  /// L64-73:HasPrerequisites
  /// L64-73: HasPrerequisites.
  bool HasPrerequisites(const std::vector<std::string>& vec_prerequisites);

  sim::Player* Owner() const { return p_owner_; }

 private:
  /// L154-200:Watcher(密封类的承载)
  /// L154-200: Watcher (the sealed-class carrier).
  struct Watcher {
    std::string key;
    sim::ITechTreeElement* registered_by = nullptr;
    std::vector<std::string> vec_prerequisites;
    bool b_has_prerequisites = false;
    int int4_limit = 0;
    bool b_hidden = false;
    bool b_initialized = false;

    /// L186-200:Update
    /// L186-200: Update.
    void Update(const std::map<std::string, int>& map_owned);
  };

  /// L75-98:GatherOwnedPrerequisites(静态)
  /// L75-98: GatherOwnedPrerequisites (static).
  static std::map<std::string, int> GatherOwnedPrerequisites(
      sim::Player* player);

  std::vector<Watcher> vec_watchers_;
  sim::Player* p_owner_ = nullptr;
  sim::World* p_world_ = nullptr;
};

// ———— DeveloperMode(最小承载)————
// ———— DeveloperMode (the minimal carrier) ————

/// DeveloperMode 的同步 cheat 字段面(FastBuild/AllTech;默认 false =
/// 上游 CheckboxEnabled=false)。cheat 命令族与 lobby 面随 Phase 6/7
/// The synchronous cheat-field face of DeveloperMode (FastBuild/AllTech;
/// the all-false default = upstream's CheckboxEnabled=false). The cheat
/// command family and the lobby face ride Phase 6/7.
class DeveloperMode final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_DeveloperMode;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<DeveloperMode>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  bool FastBuild = false;  // L53
  bool AllTech = false;    // L59(cheat 开关的同步面)
};

}  // namespace ora::mods
