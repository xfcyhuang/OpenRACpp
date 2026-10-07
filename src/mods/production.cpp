// UPSTREAM: OpenRA.Mods.Common/Traits/Production.cs + OpenRA.Mods.Common/Traits/Player/ProductionQueue.cs + OpenRA.Mods.Common/Traits/Buildings/Exit.cs 实现部分
//          The implementation half.
#include "mods/production.hpp"

#include "core/percent_modifiers.hpp"
#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/actor_exts.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "net/order.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> RecInt(const meta::RecordObject& rec,
                                   std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

std::optional<std::string> RecString(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* s = std::get_if<std::string>(&v.val))
          return *s;
      }
  }
  return std::nullopt;
}

std::vector<std::string> RecStringArray(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  std::vector<std::string> vec_out;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val))
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              vec_out.push_back(*s);
      }
  }
  return vec_out;
}

/// 上游 producee.TraitInfoOrDefault<IFacingInfo>() 的 fi?.GetInitialFacing
/// 查询(FacingInfo 按记录全名;GetInitialFacing = WAngle(InitialFacing))
/// Upstream's producee.TraitInfoOrDefault<IFacingInfo>() fi?.
/// GetInitialFacing query (FacingInfo by record full name;
/// GetInitialFacing = WAngle(InitialFacing)).
std::optional<WAngle> InitialFacingOf(const game::ActorInfo& ai) {
  for (const meta::RecordObject* rec_trait :
       ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{"OpenRA.Traits.FacingInfo"})
      if (const auto v = RecInt(*rec_trait, "InitialFacing"))
        return WAngle{static_cast<std::int32_t>(*v)};
  return std::nullopt;
}

/// 上游 producee.HasTraitInfo<IOccupySpaceInfo>() 的记录接口集判定
/// (gen 的接口名表:BuildingInfo 实现集含 IOccupySpaceInfo;MobileInfo
/// 经 IPositionableInfo 链 —— 接口闭包,MobileInfo 直接实现
/// IPositionableInfo : IOccupySpaceInfo)
/// Upstream's producee.HasTraitInfo<IOccupySpaceInfo>() record-interface
/// test (gen's interface tables: BuildingInfo's implements-set carries
/// IOccupySpaceInfo; MobileInfo via the IPositionableInfo chain — the
/// interface closure).
bool HasOccupySpaceInfo(const game::ActorInfo& ai) {
  for (const meta::RecordObject* rec_trait :
       ai.TraitsInConstructOrder()) {
    const std::string_view str_full =
        rec_trait->record_desc().str_full_name;
    if (str_full ==
            std::string_view{
                "OpenRA.Mods.Common.Traits.Buildings.BuildingInfo"} ||
        str_full ==
            std::string_view{"OpenRA.Mods.Common.Traits.MobileInfo"})
      return true;
  }
  return false;
}

/// BuildingInfo 记录判定(上游 HasTraitInfo<BuildingInfo>)
/// The BuildingInfo record test (upstream's HasTraitInfo<BuildingInfo>).
bool HasBuildingInfo(const game::ActorInfo& ai) {
  for (const meta::RecordObject* rec_trait :
       ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.Buildings.BuildingInfo"})
      return true;
  return false;
}

/// BuildableInfo 的解析(BuildableInfoData 已有;此处按记录查找)
/// The BuildableInfo parse (BuildableInfoData exists; looked up by
/// record here).
std::optional<BuildableInfoData> FindBuildable(
    const game::ActorInfo& ai) {
  for (const meta::RecordObject* rec_trait :
       ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.BuildableInfo"})
      return BuildableInfoData::Parse(*rec_trait);
  return std::nullopt;
}

}  // namespace

// ———— Exit 扩展(ExitExts.cs)————
// ———— The Exit extensions (ExitExts.cs) ————

std::vector<Exit*> ActorExits(sim::Actor& actor,
                              const std::string& production_type) {
  // L61-73
  std::vector<Exit*> vec_out;
  if (!actor.IsInWorld() || actor.Disposed())
    return vec_out;

  for (Exit* e : actor.TraitsImplementing<Exit>()) {
    if (e->IsTraitDisabled())
      continue;

    if (production_type.empty()) {
      vec_out.push_back(e);
      continue;
    }

    if (e->Info().vec_production_types.empty() ||
        std::find(e->Info().vec_production_types.begin(),
                  e->Info().vec_production_types.end(),
                  production_type) !=
            e->Info().vec_production_types.end())
      vec_out.push_back(e);
  }
  return vec_out;
}

Exit* NearestExitOrDefault(sim::Actor& actor, const WPos& pos,
                           const std::string& production_type,
                           const std::function<bool(Exit*)>& p) {
  // L44-59(OrderByDescending(Priority).ThenBy(dist²) 的稳定序 =
  // std::stable_sort 的 (priority 降, dist² 升) 复合键)
  // (OrderByDescending(Priority).ThenBy(dist²)'s stable order = the
  // composite key of std::stable_sort (priority desc, dist² asc)).
  std::vector<Exit*> vec_all = ActorExits(actor, production_type);
  std::stable_sort(vec_all.begin(), vec_all.end(),
                   [&](Exit* a, Exit* b) {
                     if (a->Info().int4_priority != b->Info().int4_priority)
                       return a->Info().int4_priority >
                              b->Info().int4_priority;
                     const WVec da =
                         actor.world().Map().CenterOfCell(
                             actor.Location() + a->Info().exit_cell) -
                         pos;
                     const WVec db =
                         actor.world().Map().CenterOfCell(
                             actor.Location() + b->Info().exit_cell) -
                         pos;
                     return da.LengthSquared() < db.LengthSquared();
                   });

  for (Exit* e : vec_all)
    if (p == nullptr || p(e))
      return e;
  return nullptr;
}

Exit* RandomExitOrDefault(sim::Actor& actor, sim::World& world,
                          const std::string& production_type,
                          const std::function<bool(Exit*)>& p) {
  // L75-93
  if (!actor.IsInWorld() || actor.Disposed())
    return nullptr;

  std::vector<Exit*> vec_all = ActorExits(actor, production_type);

  // GroupBy(Priority) 的首遇组序(保持原序分段)
  // GroupBy(Priority)'s first-encounter order (order-preserving
  // segmentation).
  std::vector<std::vector<Exit*>> vec_groups;
  std::vector<int> vec_keys;
  for (Exit* e : vec_all) {
    const int key = e->Info().int4_priority;
    bool found = false;
    for (std::size_t i = 0; i < vec_keys.size(); i++)
      if (vec_keys[i] == key) {
        vec_groups[i].push_back(e);
        found = true;
        break;
      }
    if (!found) {
      vec_keys.push_back(key);
      vec_groups.push_back({e});
    }
  }

  for (std::vector<Exit*>& vec_group : vec_groups) {
    // Shuffle(SharedRandom) 的 RNG 序保真(Fisher-Yates)
    // The Shuffle(SharedRandom) RNG order (Fisher-Yates).
    ShuffleSpan(vec_group, world.SharedRandom());
    if (p == nullptr) {
      if (!vec_group.empty())
        return vec_group.front();
      continue;
    }

    for (Exit* e : vec_group)
      if (p(e))
        return e;
  }

  return nullptr;
}

// ———— Production ————

ProductionInfoData ProductionInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProductionInfoData data;
  data.vec_produces = RecStringArray(rec_info, "Produces");
  if (const auto v = RecInt(rec_info, "UpdateFactionOnOwnerChange"))
    data.b_update_faction_on_owner_change = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

Production::Production(sim::ActorInitializer& init, ProductionInfoData info)
    : sim::ConditionalTraitCore<Production>{info.conditional},
      info_{std::move(info)} {
  // L45-52
  p_self_ = &init.Self();
  str_faction_ = init.GetValue<sim::FactionInit>(
      init.Self().Owner()->Faction().InternalName);
}

void Production::Created(sim::Actor& self) {
  // L54-59
  p_rally_point_ = self.TraitOrDefault<RallyPoint>();
  sim::ConditionalTraitCore<Production>::CoreCreated(self);
}

void Production::OnOwnerChanged(sim::Actor& self, sim::Player&,
                                sim::Player&) {
  // L151-155
  if (info_.b_update_faction_on_owner_change)
    str_faction_ = self.Owner()->Faction().Name;
}

void Production::DoProduction(sim::Actor& self,
                              const game::ActorInfo& producee,
                              const ExitInfoData* exit_info,
                              const std::string& production_type,
                              sim::TypeDictionary& inits) {
  // L61-108
  CPos exit{0, 0};
  std::vector<CPos> vec_exit_locations;

  // Clone the initializer dictionary for the new actor(上游注释)
  sim::TypeDictionary td = inits;

  if (exit_info != nullptr && self.OccupiesSpace() != nullptr &&
      HasOccupySpaceInfo(producee)) {
    exit = self.Location() + exit_info->exit_cell;
    const WPos spawn = self.CenterPosition() + exit_info->spawn_offset;
    const WPos to = self.world().Map().CenterOfCell(exit);

    WAngle initial_facing;
    if (!exit_info->opt_facing.has_value()) {
      const WVec delta = to - spawn;
      if (delta.HorizontalLengthSquared() == 0) {
        const std::optional<WAngle> fi = InitialFacingOf(producee);
        initial_facing = fi.has_value() ? *fi : WAngle{0};
      } else {
        initial_facing = delta.Yaw();
      }
    } else {
      initial_facing = *exit_info->opt_facing;
    }

    vec_exit_locations =
        (p_rally_point_ != nullptr && !p_rally_point_->Path.empty())
            ? p_rally_point_->Path
            : std::vector<CPos>{exit};

    td.Add(self.world().Arena().Create<sim::LocationInit>(exit));
    td.Add(self.world().Arena().Create<sim::CenterPositionInit>(spawn));
    td.Add(self.world().Arena().Create<sim::FacingInit>(
        initial_facing));
    td.Add(self.world().Arena().Create<sim::CreationActivityDelayInit>(
        exit_info->int4_exit_delay));
    td.Add(self.world().Arena().Create<sim::RallyPointInit>(
        vec_exit_locations));
  }

  const std::string producee_name = producee.Name();
  Production* production_this = this;
  const std::string production_type_copy = production_type;
  self.world().AddFrameEndTask([&self, production_this, producee_name,
                                exit, production_type_copy, td](
                                   sim::World& w) mutable {
    sim::Actor* new_unit = w.CreateActor(producee_name, td);
    if (!self.IsDead())
      for (auto* t : self.TraitsImplementing<sim::INotifyProduction>())
        t->UnitProduced(self, *new_unit, exit);

    for (auto& [notify_actor, notify] :
         w.ActorsWithTrait<sim::INotifyOtherProduction>())
      notify->UnitProducedByOther(*notify_actor, self, *new_unit,
                                  production_type_copy, td);
    (void)production_this;
  });
}

Exit* Production::SelectExitImpl(
    sim::Actor& self, const game::ActorInfo& producee,
    const std::string& production_type,
    const std::function<bool(Exit*)>& p) {
  // L110-122
  if (p_rally_point_ == nullptr || p_rally_point_->Path.empty())
    return RandomExitOrDefault(self, self.world(), production_type, p);

  return NearestExitOrDefault(
      self, self.world().Map().CenterOfCell(p_rally_point_->Path[0]),
      production_type, p);
}

Exit* Production::SelectExit(sim::Actor& self,
                             const game::ActorInfo& producee,
                             const std::string& production_type) {
  // L124-126
  return SelectExitImpl(self, producee, production_type,
                        [this, &self, &producee](Exit* e) {
                          return CanUseExit(self, producee, e->Info());
                        });
}

bool Production::Produce(sim::Actor& self,
                         const game::ActorInfo& producee,
                         const std::string& production_type,
                         sim::TypeDictionary& inits, int refundable_value) {
  // L128-140(Reservable.IsReserved 前置)
  // (the Reservable.IsReserved precondition).
  if (IsTraitDisabled() || IsTraitPaused() ||
      Reservable::IsReserved(self))
    return false;

  // Pick a spawn/exit point pair(上游注释)
  Exit* exit = SelectExit(self, producee, production_type);
  if (exit != nullptr || self.OccupiesSpace() == nullptr ||
      !HasOccupySpaceInfo(producee)) {
    DoProduction(self, producee, exit != nullptr ? &exit->Info()
                                                 : nullptr,
                 production_type, inits);
    return true;
  }

  return false;
}

bool Production::CanUseExit(sim::Actor& self,
                            const game::ActorInfo& producee,
                            const ExitInfoData& s) {
  // L142-146(MobileInfo 解析:Exit/ExitCell 的可入格判定)
  // (the MobileInfo parse: the exit-cell enterability test).
  std::optional<MobileInfoData> mobile_info;
  for (const meta::RecordObject* rec_trait :
       producee.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{"OpenRA.Mods.Common.Traits.MobileInfo"}) {
      mobile_info = MobileInfoData::Parse(*rec_trait);
      break;
    }

  NotifyBlocker(self, self.Location() + s.exit_cell);

  if (!mobile_info.has_value())
    return true;
  return mobile_info->CanEnterCell(self.world(), &self,
                                   self.Location() + s.exit_cell,
                                   sim::SubCell::Any, &self,
                                   sim::BlockedByActor::Immovable);
}

// ———— ProductionQueue ————

ProductionQueueInfoData ProductionQueueInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProductionQueueInfoData data;
  if (const auto v = RecString(rec_info, "Type"))
    data.str_type = *v;
  if (const auto v = RecInt(rec_info, "DisplayOrder"))
    data.int4_display_order = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "Group"))
    data.str_group = *v;
  data.vec_factions = RecStringArray(rec_info, "Factions");
  if (const auto v = RecInt(rec_info, "Sticky"))
    data.b_sticky = *v != 0;
  if (const auto v = RecInt(rec_info, "PayUpFront"))
    data.b_pay_up_front = *v != 0;
  if (const auto v = RecInt(rec_info, "DisallowPaused"))
    data.b_disallow_paused = *v != 0;
  if (const auto v = RecInt(rec_info, "BuildDurationModifier"))
    data.int4_build_duration_modifier = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "ItemLimit"))
    data.int4_item_limit = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "QueueLimit"))
    data.int4_queue_limit = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "LowPowerModifier"))
    data.int4_low_power_modifier = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "InfiniteBuildLimit"))
    data.int4_infinite_build_limit = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "ReadyAudio"))
    data.str_ready_audio = *v;
  if (const auto v = RecString(rec_info, "BlockedAudio"))
    data.str_blocked_audio = *v;
  if (const auto v = RecString(rec_info, "LimitedAudio"))
    data.str_limited_audio = *v;
  if (const auto v = RecString(rec_info, "CannotPlaceAudio"))
    data.str_cannot_place_audio = *v;
  if (const auto v = RecString(rec_info, "QueuedAudio"))
    data.str_queued_audio = *v;
  if (const auto v = RecString(rec_info, "OnHoldAudio"))
    data.str_on_hold_audio = *v;
  if (const auto v = RecString(rec_info, "CancelledAudio"))
    data.str_cancelled_audio = *v;
  return data;
}

ProductionQueue::ProductionQueue(sim::ActorInitializer& init,
                                 ProductionQueueInfoData info)
    : info_{std::move(info)} {
  // L164-175
  p_actor_ = &init.Self();

  str_faction_ = init.GetValue<sim::FactionInit>(
      p_actor_->Owner()->Faction().InternalName);
  b_is_valid_faction_ = info_.vec_factions.empty() ||
                        std::find(info_.vec_factions.begin(),
                                  info_.vec_factions.end(),
                                  str_faction_) !=
                            info_.vec_factions.end();
  b_enabled_ = b_is_valid_faction_;
}

void ProductionQueue::Created(sim::Actor& self) {
  // L177-186(PowerManager 未移植 → null)
  p_player_resources_ =
      self.Owner()->PlayerActor()->Trait<PlayerResources>();
  p_developer_mode_ =
      self.Owner()->PlayerActor()->TraitOrDefault<DeveloperMode>();
  p_tech_tree_ = self.Owner()->PlayerActor()->Trait<TechTree>();

  for (Production* p : self.TraitsImplementing<Production>())
    if (std::find(p->Info().vec_produces.begin(),
                  p->Info().vec_produces.end(),
                  info_.str_type) != p->Info().vec_produces.end())
      vec_production_traits_.push_back(p);
  CacheProducibles();
}

void ProductionQueue::ClearQueue() {
  // L188-203
  for (auto& item : vec_queue_) {
    if (item->ResourcesPaid > 0) {
      p_player_resources_->RefundResources(item->ResourcesPaid);
      item->RemainingCost += item->ResourcesPaid;
    }

    p_player_resources_->RefundCash(item->TotalCost -
                                    item->RemainingCost);
  }

  vec_queue_.clear();
}

void ProductionQueue::OnOwnerChanged(sim::Actor& self,
                                     sim::Player& old_owner,
                                     sim::Player& new_owner) {
  // L205-224
  ClearQueue();

  p_player_resources_ =
      new_owner.PlayerActor()->Trait<PlayerResources>();
  p_developer_mode_ =
      new_owner.PlayerActor()->TraitOrDefault<DeveloperMode>();
  p_tech_tree_ = new_owner.PlayerActor()->Trait<TechTree>();

  if (!info_.b_sticky) {
    str_faction_ = self.Owner()->Faction().Name;
    b_is_valid_faction_ =
        info_.vec_factions.empty() ||
        std::find(info_.vec_factions.begin(), info_.vec_factions.end(),
                  str_faction_) != info_.vec_factions.end();
  }

  // Regenerate the producibles and tech tree state(上游注释)
  old_owner.PlayerActor()->Trait<TechTree>()->RemoveElement(this);
  CacheProducibles();
  p_tech_tree_->Update();
}

void ProductionQueue::Killed(sim::Actor& killed,
                             const sim::AttackInfo& e) {
  // L226
  if (&killed == p_actor_) {
    ClearQueue();
    b_enabled_ = false;
  }
}

void ProductionQueue::Selling(sim::Actor&) {
  // L227
  ClearQueue();
  b_enabled_ = false;
}

void ProductionQueue::BeforeTransform(sim::Actor&) {
  // L230
  ClearQueue();
  b_enabled_ = false;
}

void ProductionQueue::CacheProducibles() {
  // L234-247
  vec_producible_.clear();
  if (!b_enabled_)
    return;

  for (const game::ActorInfo* a : AllBuildables(info_.str_type)) {
    const std::optional<BuildableInfoData> bi = FindBuildable(*a);

    vec_producible_.push_back(ProducibleEntry{a, true, false});
    p_tech_tree_->Add(a->Name(), bi->vec_prerequisites,
                      bi->int4_build_limit, this);
  }
}

std::vector<const game::ActorInfo*> ProductionQueue::AllBuildables(
    const std::string& category) const {
  // L249-255(Actors.Values 的字典序 = ActorInfoDictionary 键序;
  // '^' 抽象过滤 + Queue 包含)
  // (Actors.Values' dictionary order = ActorInfoDictionary's key order;
  // the '^' abstract filter + the Queue containment).
  std::vector<const game::ActorInfo*> vec_out;
  for (const auto& [name, actor_ptr] :
       p_actor_->world().Map().Rules().Actors()) {
    if (name.empty() || name.front() == '^')
      continue;
    const game::ActorInfo* x = actor_ptr.get();
    if (x == nullptr)
      continue;
    const std::optional<BuildableInfoData> bi = FindBuildable(*x);
    if (!bi.has_value())
      continue;
    if (std::find(bi->vec_queue.begin(), bi->vec_queue.end(),
                  category) != bi->vec_queue.end())
      vec_out.push_back(x);
  }
  return vec_out;
}

void ProductionQueue::PrerequisitesAvailable(const std::string& key) {
  // L258-260
  if (ProducibleEntry* entry = FindProducibleByName(key))
    entry->b_buildable = true;
}

void ProductionQueue::PrerequisitesUnavailable(const std::string& key) {
  // L262-264
  if (ProducibleEntry* entry = FindProducibleByName(key))
    entry->b_buildable = false;
}

void ProductionQueue::PrerequisitesItemHidden(const std::string& key) {
  // L266-268
  if (ProducibleEntry* entry = FindProducibleByName(key))
    entry->b_visible = false;
}

void ProductionQueue::PrerequisitesItemVisible(const std::string& key) {
  // L270-272
  if (ProducibleEntry* entry = FindProducibleByName(key))
    entry->b_visible = true;
}

bool ProductionQueue::IsProducing(
    const ProductionItemLike& item) const {
  // L278-281
  return !vec_queue_.empty() && vec_queue_[0].get() == &item;
}

bool ProductionQueue::IsInQueue(
    const game::ActorInfo& actor) const {
  // L283-286
  for (auto& i : vec_queue_)
    if (i->Item == actor.Name())
      return true;
  return false;
}

ProductionItemLike* ProductionQueue::CurrentItem() {
  // L288-290
  return vec_queue_.empty() ? nullptr : vec_queue_[0].get();
}

std::vector<const game::ActorInfo*> ProductionQueue::AllItems() {
  // L298-306(allProducibles 的消费时物化)
  // (allProducibles' per-consumption materialization).
  if (!vec_production_traits_.empty()) {
    bool all_disabled = true;
    for (const Production* p : vec_production_traits_)
      if (!p->IsTraitDisabled())
        all_disabled = false;
    if (all_disabled)
      return {};
  }
  if (p_developer_mode_ != nullptr && p_developer_mode_->AllTech) {
    std::vector<const game::ActorInfo*> vec_out;
    for (const ProducibleEntry& e : vec_producible_)
      vec_out.push_back(e.actor_info);
    return vec_out;
  }

  std::vector<const game::ActorInfo*> vec_out;
  for (const ProducibleEntry& e : vec_producible_)
    if (e.b_buildable || e.b_visible)
      vec_out.push_back(e.actor_info);
  return vec_out;
}

std::vector<const game::ActorInfo*> ProductionQueue::BuildableItems() {
  // L308-321(buildableProducibles 的消费时物化)
  // (buildableProducibles' per-consumption materialization).
  if (!vec_production_traits_.empty()) {
    bool all_disabled = true;
    for (const Production* p : vec_production_traits_)
      if (!p->IsTraitDisabled())
        all_disabled = false;
    if (all_disabled)
      return {};
  }
  if (!b_enabled_)
    return {};

  const bool all_tech =
      p_developer_mode_ != nullptr && p_developer_mode_->AllTech;
  std::vector<const game::ActorInfo*> vec_out;
  for (const ProducibleEntry& e : vec_producible_) {
    if (all_tech) {
      if (!info_.b_pay_up_front ||
          GetProductionCost(*e.actor_info) <=
              p_player_resources_->GetCashAndResources() ||
          IsInQueue(*e.actor_info))
        vec_out.push_back(e.actor_info);
      continue;
    }

    if (!e.b_buildable)
      continue;
    if (info_.b_pay_up_front &&
        !(GetProductionCost(*e.actor_info) <=
              p_player_resources_->GetCashAndResources() ||
          IsInQueue(*e.actor_info)))
      continue;
    vec_out.push_back(e.actor_info);
  }
  return vec_out;
}

bool ProductionQueue::AnyItemsToBuild() {
  // L323-328
  bool any_enabled = vec_production_traits_.empty();
  for (const Production* p : vec_production_traits_)
    if (!p->IsTraitDisabled())
      any_enabled = true;

  if (!(b_enabled_ && any_enabled))
    return false;

  if (p_developer_mode_ != nullptr && p_developer_mode_->AllTech &&
      !vec_producible_.empty())
    return true;

  for (const ProducibleEntry& e : vec_producible_)
    if (e.b_buildable)
      return true;
  return false;
}

bool ProductionQueue::CanBuild(const game::ActorInfo& actor) {
  // L330-336
  const ProducibleEntry* ps = FindProducible(&actor);
  if (ps == nullptr)
    return false;

  return ps->b_buildable ||
         (p_developer_mode_ != nullptr && p_developer_mode_->AllTech);
}

void ProductionQueue::Tick(sim::Actor& self) {
  TickImpl(self);
}

void ProductionQueue::TickImpl(sim::Actor& self) {
  // L343-359
  bool any_enabled_production = false;
  bool any_unpaused_production = false;
  for (const Production* p : vec_production_traits_) {
    any_enabled_production |= !p->IsTraitDisabled();
    any_unpaused_production |= !p->IsTraitPaused();
  }

  if (!any_enabled_production)
    ClearQueue();

  b_enabled_ = b_is_valid_faction_ && any_enabled_production;
  TickInner(self, !any_unpaused_production);
}

void ProductionQueue::TickInner(sim::Actor& self,
                                bool all_production_paused) {
  // L361-367
  CancelUnbuildableItems();

  if (!vec_queue_.empty() && !all_production_paused)
    vec_queue_[0]->Tick(*p_player_resources_);
}

void ProductionQueue::CancelUnbuildableItems() {
  // L369-402
  if (vec_queue_.empty())
    return;

  const std::vector<const game::ActorInfo*> vec_buildable =
      BuildableItems();
  std::vector<std::string> vec_buildable_names;
  for (const game::ActorInfo* b : vec_buildable)
    vec_buildable_names.push_back(b->Name());

  // EndProduction removes the item from the queue, so we enumerate by
  // index in reverse to avoid issues with index reassignment(上游注释)
  bool cancelled_an_item = false;
  for (std::size_t i = vec_queue_.size(); i-- > 0;) {
    if (std::find(vec_buildable_names.begin(), vec_buildable_names.end(),
                  vec_queue_[i]->Item) != vec_buildable_names.end())
      continue;

    // Refund spent resources(上游注释)
    if (vec_queue_[i]->ResourcesPaid > 0) {
      p_player_resources_->RefundResources(vec_queue_[i]->ResourcesPaid);
      vec_queue_[i]->RemainingCost += vec_queue_[i]->ResourcesPaid;
    }

    // Refund what's been paid so far(上游注释)
    p_player_resources_->RefundCash(vec_queue_[i]->TotalCost -
                                    vec_queue_[i]->RemainingCost);
    EndProduction(*vec_queue_[i]);
    cancelled_an_item = true;
  }

  // CancelledAudio 的声音/文本通知随注入面(通知不可观测)
  // (the CancelledAudio sound/text notification rides the injection
  // face; the notification is unobservable here).
  (void)cancelled_an_item;
}

bool ProductionQueue::CanQueue(const game::ActorInfo& actor,
                               std::string& notification_audio,
                               std::string& notification_text) {
  // L404-445
  notification_audio = info_.str_blocked_audio;
  notification_text.clear();  // BlockedTextNotification(文本面随 UI 批)

  const std::optional<BuildableInfoData> bi = FindBuildable(actor);
  if (!bi.has_value())
    return false;

  const bool all_tech =
      p_developer_mode_ != nullptr && p_developer_mode_->AllTech;
  if (!all_tech) {
    if (info_.b_pay_up_front) {
      std::optional<ValuedInfoData> valued;
      for (const meta::RecordObject* rec_trait :
           actor.TraitsInConstructOrder())
        if (rec_trait->record_desc().str_full_name ==
            std::string_view{"OpenRA.Mods.Common.Traits.ValuedInfo"}) {
          valued = ValuedInfoData::Parse(*rec_trait);
          break;
        }
      const int cost = valued.has_value() ? valued->int4_cost : 0;
      if (cost > p_player_resources_->GetCashAndResources())
        return false;
    }

    if (info_.int4_queue_limit > 0 &&
        static_cast<int>(vec_queue_.size()) >= info_.int4_queue_limit) {
      notification_audio = info_.str_limited_audio;
      return false;
    }

    int queue_count = 0;
    for (auto& i : vec_queue_)
      if (i->Item == actor.Name())
        queue_count++;
    if (info_.int4_item_limit > 0 &&
        queue_count >= info_.int4_item_limit) {
      notification_audio = info_.str_limited_audio;
      return false;
    }

    if (bi->int4_build_limit > 0) {
      int owned = 0;
      for (auto& [a, trait] :
           p_actor_->world().ActorsWithTrait<Buildable>())
        if (a->Info()->Name() == actor.Name() &&
            a->Owner() == p_actor_->Owner())
          owned++;
      if (queue_count + owned >= bi->int4_build_limit)
        return false;
    }
  }

  notification_audio = info_.str_queued_audio;
  return true;
}

void ProductionQueue::ResolveOrder(sim::Actor& self,
                                   const ora::net::Order& order) {
  // L447-539
  if (!b_enabled_)
    return;

  const game::Ruleset& rules = self.world().Map().Rules();
  if (order.str_order_string == "StartProduction") {
    const game::ActorInfo* unit =
        rules.FindActor(order.str_target_string.value_or(""));
    if (unit == nullptr)
      return;
    const std::optional<BuildableInfoData> bi = FindBuildable(*unit);

    // Not built by this queue(上游注释)
    if (!bi.has_value() ||
        std::find(bi->vec_queue.begin(), bi->vec_queue.end(),
                  info_.str_type) == bi->vec_queue.end())
      return;

    // You can't build that(上游注释)
    const std::vector<const game::ActorInfo*> vec_buildable =
        BuildableItems();
    bool buildable = false;
    for (const game::ActorInfo* b : vec_buildable)
      if (b->Name() == order.str_target_string) {
        buildable = true;
        break;
      }
    if (!buildable)
      return;

    // Check if the player is trying to build more units that they are
    // allowed(上游注释)
    const bool all_tech =
        p_developer_mode_ != nullptr && p_developer_mode_->AllTech;
    int from_limit = std::numeric_limits<int>::max();
    if (!all_tech) {
      if (info_.int4_queue_limit > 0)
        from_limit = info_.int4_queue_limit -
                     static_cast<int>(vec_queue_.size());

      if (info_.int4_item_limit > 0) {
        int queue_count = 0;
        for (auto& i : vec_queue_)
          if (i->Item == order.str_target_string)
            queue_count++;
        from_limit = std::min(from_limit,
                              info_.int4_item_limit - queue_count);
      }

      if (bi->int4_build_limit > 0) {
        int in_queue = 0;
        for (auto& pi : vec_queue_)
          if (pi->Item == order.str_target_string)
            in_queue++;
        int owned = 0;
        for (auto& [a, trait] :
             self.world().ActorsWithTrait<Buildable>())
          if (a->Info()->Name() == order.str_target_string &&
              a->Owner() == self.Owner())
            owned++;
        from_limit =
            std::min(from_limit, bi->int4_build_limit - (in_queue + owned));
      }

      if (from_limit <= 0)
        return;
    }

    const int cost = GetProductionCost(*unit);
    const int time = GetBuildTime(*unit);
    const int amount_to_build =
        std::min(from_limit, static_cast<int>(order.uint4_extra_data));
    for (int n = 0; n < amount_to_build; n++) {
      if (info_.b_pay_up_front &&
          cost > p_player_resources_->GetCashAndResources())
        return;

      // OnComplete 的帧末闭包(上游捕获 notified 局部 → item->b_notified
      // 成员承载;声音/文本通知随注入面)
      // The OnComplete frame-end closure (upstream captures the notified
      // local → the item->b_notified member; the sound/text
      // notifications ride the injection face).
      const std::string unit_name = unit->Name();
      ProductionQueue* queue_this = this;
      auto item = std::make_unique<ProductionItemLike>(
          *this, order.str_target_string.value_or(""), cost,
          std::function<void(ProductionItemLike*)>{});
      item->OnComplete =
          [queue_this, unit_name, time,
           is_building = HasBuildingInfo(*unit)](
              ProductionItemLike* self_item) {
            sim::World& w = queue_this->Actor()->world();
            w.AddFrameEndTask([queue_this, unit_name, time,
                               is_building,
                               self_item](sim::World&) {
              // Make sure the item hasn't been invalidated between the
              // ProductionItem ticking and this FrameEndTask running
              // (上游注释)
              bool any_done = false;
              for (auto& i : queue_this->AllQueued())
                if (i->Done() && i->Item == unit_name) {
                  any_done = true;
                  break;
                }
              if (!any_done) {
                if (self_item != nullptr)
                  self_item->b_notified = false;
                return;
              }

              if (is_building) {
                // ReadyAudio 通知随注入面
                if (self_item != nullptr && !self_item->b_notified)
                  self_item->b_notified = true;
              } else {
                const game::ActorInfo* unit_info =
                    queue_this->Actor()->world().Map().Rules().FindActor(
                        unit_name);
                if (unit_info != nullptr &&
                    queue_this->BuildUnit(*unit_info)) {
                  // ReadyAudio 随注入面
                } else if (self_item != nullptr && !self_item->b_notified &&
                           time > 0) {
                  self_item->b_notified = true;
                }
              }
            });
          };

      BeginProduction(std::move(item), !order.b_queued);
    }
  } else if (order.str_order_string == "PauseProduction") {
    PauseProduction(order.str_target_string.value_or(""),
                    order.uint4_extra_data != 0);
  } else if (order.str_order_string == "CancelProduction") {
    CancelProduction(order.str_target_string.value_or(""),
                     order.uint4_extra_data);
  }
}

int ProductionQueue::GetBuildTime(const game::ActorInfo& unit) {
  // L541-556(FastBuild = 0;IProductionTimeModifierInfo 空集)
  // (FastBuild = 0; the IProductionTimeModifierInfo set is empty).
  if (p_developer_mode_ != nullptr && p_developer_mode_->FastBuild)
    return 0;

  const std::optional<BuildableInfoData> bi = FindBuildable(unit);
  int time = bi.has_value() ? bi->int4_build_duration : -1;
  if (time == -1)
    time = GetProductionCost(unit);

  std::vector<int> vec_modifiers;
  if (bi.has_value())
    vec_modifiers.push_back(bi->int4_build_duration_modifier);
  vec_modifiers.push_back(info_.int4_build_duration_modifier);

  return ApplyPercentageModifiers(time, vec_modifiers);
}

int ProductionQueue::GetProductionCost(
    const game::ActorInfo& unit) {
  // L558-568(IProductionCostModifierInfo 空集 → Valued 原值)
  // (the IProductionCostModifierInfo set is empty → the raw Valued).
  for (const meta::RecordObject* rec_trait :
       unit.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{"OpenRA.Mods.Common.Traits.ValuedInfo"})
      return ValuedInfoData::Parse(*rec_trait).int4_cost;
  return 0;
}

void ProductionQueue::PauseProduction(const std::string& item_name,
                                       bool paused) {
  // L570-572
  for (auto& a : vec_queue_)
    if (a->Item == item_name) {
      a->Pause(paused);
      return;
    }
}

void ProductionQueue::CancelProduction(const std::string& item_name,
                                       std::uint32_t number_to_cancel) {
  // L574-580
  for (std::uint32_t i = 0; i < number_to_cancel; i++)
    if (!CancelProductionInner(item_name))
      break;
}

bool ProductionQueue::CancelProductionInner(const std::string& item_name) {
  // L582-611
  ProductionItemLike* item = nullptr;
  for (auto it = vec_queue_.rbegin(); it != vec_queue_.rend(); ++it)
    if ((*it)->Item == item_name) {
      item = it->get();
      break;
    }

  if (item != nullptr) {
    if (item->Infinite) {
      item->Infinite = false;
      for (int i = 1; i < info_.int4_infinite_build_limit; i++)
        vec_queue_.push_back(std::make_unique<ProductionItemLike>(
            *this, item->Item, item->TotalCost,
            std::function<void(ProductionItemLike*)>{}));
    } else {
      // Refund what has been paid(上游注释)
      if (item->ResourcesPaid > 0) {
        p_player_resources_->RefundResources(item->ResourcesPaid);
        item->RemainingCost += item->ResourcesPaid;
      }

      p_player_resources_->RefundCash(item->TotalCost -
                                      item->RemainingCost);
      EndProduction(*item);
    }

    return true;
  }

  return false;
}

void ProductionQueue::EndProduction(ProductionItemLike& item) {
  // L613-619(erase 触发 unique_ptr 析构 —— 先快照重建字段)
  // (the erase destroys the unique_ptr — snapshot the rebuild fields
  // first).
  const bool infinite = item.Infinite;
  const std::string rebuild_name = item.Item;
  const int rebuild_cost = item.TotalCost;
  auto rebuild_on_complete = item.OnComplete;
  std::erase_if(vec_queue_,
                [&item](const std::unique_ptr<ProductionItemLike>& i) {
                  return i.get() == &item;
                });

  if (infinite) {
    auto rebuilt = std::make_unique<ProductionItemLike>(
        *this, rebuild_name, rebuild_cost, std::move(rebuild_on_complete));
    rebuilt->Infinite = true;  // 上游对象初始化器 { Infinite = true }
    vec_queue_.push_back(std::move(rebuilt));
  }
}

void ProductionQueue::BeginProduction(
    std::unique_ptr<ProductionItemLike> item, bool has_priority) {
  // L621-663
  if (info_.b_pay_up_front) {
    if (p_player_resources_->Resources > 0 &&
        p_player_resources_->Resources <= item->TotalCost)
      item->ResourcesPaid = p_player_resources_->Resources;
    else if (p_player_resources_->Resources > item->TotalCost)
      item->ResourcesPaid = item->TotalCost;

    p_player_resources_->TakeCash(item->TotalCost);
    item->RemainingCost = 0;
  }

  bool any_infinite_same = false;
  for (auto& i : vec_queue_)
    if (i->Item == item->Item && i->Infinite)
      any_infinite_same = true;
  if (any_infinite_same)
    return;
  if (has_priority && vec_queue_.size() > 1)
    vec_queue_.insert(vec_queue_.begin() + 1, std::move(item));
  else
    vec_queue_.push_back(std::move(item));

  if (info_.int4_infinite_build_limit < 0)
    return;

  std::vector<ProductionItemLike*> vec_queued;
  for (auto& i : vec_queue_)
    if (i->Item == item->Item)
      vec_queued.push_back(i.get());

  if (static_cast<int>(vec_queued.size()) <= info_.int4_infinite_build_limit)
    return;

  vec_queued[0]->Infinite = true;

  for (std::size_t i = 1; i < vec_queued.size(); i++) {
    // Refund what has been paid(上游注释)
    if (vec_queued[i]->ResourcesPaid > 0) {
      p_player_resources_->RefundResources(vec_queued[i]->ResourcesPaid);
      vec_queued[i]->RemainingCost += vec_queued[i]->ResourcesPaid;
    }

    p_player_resources_->RefundCash(vec_queued[i]->TotalCost -
                                    vec_queued[i]->RemainingCost);
    EndProduction(*vec_queued[i]);
  }
}

int ProductionQueue::RemainingTimeActual(
    const ProductionItemLike& item) const {
  // L665-668
  return item.RemainingTimeActual();
}

Production* ProductionQueue::MostLikelyProducer() {
  // L671-678(OrderBy(IsTraitPaused) 稳定序首项)
  // (OrderBy(IsTraitPaused)'s stable-order first).
  Production* best = nullptr;
  bool best_paused = true;
  for (Production* p : vec_production_traits_) {
    if (p->IsTraitDisabled())
      continue;
    if (std::find(p->Info().vec_produces.begin(),
                  p->Info().vec_produces.end(),
                  info_.str_type) == p->Info().vec_produces.end())
      continue;
    const bool paused = p->IsTraitPaused();
    if (best == nullptr || (paused && !best_paused) ||
        (paused == best_paused && false)) {
      best = p;
      best_paused = paused;
    }
  }
  return best;
}

bool ProductionQueue::BuildUnit(const game::ActorInfo& unit) {
  // L680-709
  Production* most_likely_producer_trait = MostLikelyProducer();

  // Cannot produce if I'm dead or trait is disabled(上游注释)
  if (!p_actor_->IsInWorld() || p_actor_->IsDead() ||
      most_likely_producer_trait == nullptr) {
    CancelProduction(unit.Name(), 1);
    return false;
  }

  sim::TypeDictionary inits;
  inits.Add(p_actor_->world().Arena().Create<sim::OwnerInit>(
      p_actor_->Owner()));
  inits.Add(p_actor_->world().Arena().Create<sim::FactionInit>(
      BuildableInfoData::GetInitialFaction(unit, str_faction_)));

  const std::optional<BuildableInfoData> bi = FindBuildable(unit);
  const bool all_tech =
      p_developer_mode_ != nullptr && p_developer_mode_->AllTech;
  const std::string type =
      all_tech ? info_.str_type
               : (!bi.has_value() || bi->str_build_at_production_type.empty()
                      ? info_.str_type
                      : bi->str_build_at_production_type);
  ProductionItemLike* item = nullptr;
  for (auto& i : vec_queue_)
    if (i->Done() && i->Item == unit.Name()) {
      item = i.get();
      break;
    }
  if (item == nullptr)
    return false;
  if (!most_likely_producer_trait->IsTraitPaused() &&
      most_likely_producer_trait->Produce(*p_actor_, unit, type, inits,
                                          item->TotalCost)) {
    EndProduction(*item);
    return true;
  }

  return false;
}

ProductionQueue::ProducibleEntry* ProductionQueue::FindProducible(
    const game::ActorInfo* actor) {
  for (ProducibleEntry& e : vec_producible_)
    if (e.actor_info == actor)
      return &e;
  return nullptr;
}

ProductionQueue::ProducibleEntry* ProductionQueue::FindProducibleByName(
    const std::string& name) {
  for (ProducibleEntry& e : vec_producible_)
    if (e.actor_info->Name() == name)
      return &e;
  return nullptr;
}

// ———— ClassicProductionQueue(L58-139)————
// ———— ClassicProductionQueue (L58-139) ————

void ClassicProductionQueue::TickImpl(sim::Actor& self) {
  // L58-83(上游 PERF 注释:避 LINQ)
  b_enabled_ = false;
  bool is_active = false;
  for (auto& [actor, trait] :
       self.world().ActorsWithTrait<Production>()) {
    if (trait->IsTraitDisabled())
      continue;

    if (actor->Owner() != self.Owner() ||
        std::find(trait->Info().vec_produces.begin(),
                  trait->Info().vec_produces.end(),
                  Info().str_type) == trait->Info().vec_produces.end())
      continue;

    b_enabled_ |= IsValidFaction();
    is_active |= !trait->IsTraitPaused();
  }

  if (!b_enabled_)
    ClearQueue();

  TickInner(self, !is_active);
}

std::vector<const game::ActorInfo*> ClassicProductionQueue::AllItems() {
  // L85-88
  return Enabled() ? ProductionQueue::AllItems()
                   : std::vector<const game::ActorInfo*>{};
}

std::vector<const game::ActorInfo*>
ClassicProductionQueue::BuildableItems() {
  // L80-83
  return Enabled() ? ProductionQueue::BuildableItems()
                   : std::vector<const game::ActorInfo*>{};
}

Production* ClassicProductionQueue::MostLikelyProducer() {
  // L85-99(OrderBy(paused).ThenByDescending(IsPrimaryBuilding).
  // ThenByDescending(ActorID) —— IsPrimaryBuilding 未移植,序退化为
  // (paused asc, ActorID desc);无 PrimaryBuilding trait 的世界等价;
  // 登记 COVERAGE)
  // (OrderBy(paused).ThenByDescending(IsPrimaryBuilding).
  // ThenByDescending(ActorID) — IsPrimaryBuilding is unported, the
  // order degrades to (paused asc, ActorID desc); the equivalent of a
  // world without the PrimaryBuilding trait; registered in COVERAGE).
  std::vector<Production*> vec_candidates;
  for (auto& [actor, trait] :
       Actor()->world().ActorsWithTrait<Production>()) {
    if (actor->Owner() != Actor()->Owner() ||
        trait->IsTraitDisabled())
      continue;
    if (std::find(trait->Info().vec_produces.begin(),
                  trait->Info().vec_produces.end(),
                  Info().str_type) == trait->Info().vec_produces.end())
      continue;
    vec_candidates.push_back(trait);
  }

  Production* best = nullptr;
  for (Production* p : vec_candidates) {
    if (best == nullptr) {
      best = p;
      continue;
    }
    // (paused asc, ActorID desc) 的逐项择优
    // The per-item pick of (paused asc, ActorID desc).
    const bool p_paused = p->IsTraitPaused();
    const bool b_paused = best->IsTraitPaused();
    if (p_paused != b_paused) {
      if (!p_paused)
        best = p;
      continue;
    }
    if (p->HostActor().ActorID() > best->HostActor().ActorID())
      best = p;
  }
  return best;
}

bool ClassicProductionQueue::BuildUnit(const game::ActorInfo& unit) {
  // L101-139
  // Find a production structure to build this actor(上游注释)
  const std::optional<BuildableInfoData> bi = FindBuildable(unit);

  // Some units may request a specific production type, which is ignored
  // if the AllTech cheat is enabled(上游注释)
  const bool all_tech =
      p_developer_mode_ != nullptr && p_developer_mode_->AllTech;
  const std::string type =
      all_tech ? Info().str_type
               : (!bi.has_value() || bi->str_build_at_production_type.empty()
                      ? Info().str_type
                      : bi->str_build_at_production_type);

  std::vector<Production*> vec_producers;
  for (auto& [actor, trait] :
       Actor()->world().ActorsWithTrait<Production>()) {
    if (actor->Owner() != Actor()->Owner() ||
        trait->IsTraitDisabled())
      continue;
    if (std::find(trait->Info().vec_produces.begin(),
                  trait->Info().vec_produces.end(),
                  type) == trait->Info().vec_produces.end())
      continue;
    vec_producers.push_back(trait);
  }
  // ThenByDescending(IsPrimaryBuilding) 的退化序 = ActorID 降(登记)
  // ThenByDescending(IsPrimaryBuilding)'s degraded order = ActorID
  // descending (registered).
  std::stable_sort(vec_producers.begin(), vec_producers.end(),
                   [](Production* a, Production* b) {
                     return a->HostActor().ActorID() >
                            b->HostActor().ActorID();
                   });

  bool any_producers = false;
  for (Production* p : vec_producers) {
    any_producers = true;
    if (p->IsTraitPaused())
      continue;

    sim::TypeDictionary inits;
    inits.Add(Actor()->world().Arena().Create<sim::OwnerInit>(
        Actor()->Owner()));
    inits.Add(Actor()->world().Arena().Create<sim::FactionInit>(
        BuildableInfoData::GetInitialFaction(unit, p->Faction())));

    ProductionItemLike* item = nullptr;
    for (auto& i : vec_queue_)
      if (i->Done() && i->Item == unit.Name()) {
        item = i.get();
        break;
      }
    if (item == nullptr)
      continue;
    if (p->Produce(p->HostActor(), unit, type, inits,
                   item->TotalCost)) {
      EndProduction(*item);
      return true;
    }
  }

  if (!any_producers)
    return ProductionQueue::BuildUnit(unit);

  return false;
}

// ———— ProductionItem ————

ProductionItemLike::ProductionItemLike(
    ProductionQueue& queue, std::string item, int cost,
    std::function<void(ProductionItemLike*)> on_complete)
    : Item{std::move(item)},
      TotalCost{cost},
      RemainingCost{cost},
      Queue{&queue},
      OnComplete{std::move(on_complete)} {
  // L743-756
  int4_remaining_time_ = int4_total_time_ = 1;
  p_ai_ = queue.Actor()->world().Map().Rules().FindActor(Item);
  const std::optional<BuildableInfoData> bi =
      p_ai_ != nullptr ? FindBuildable(*p_ai_) : std::nullopt;
  int4_build_palette_order_ =
      bi.has_value() ? bi->int4_build_palette_order : 0;
  Infinite = false;
}

int ProductionItemLike::RemainingTimeActual() const {
  // L728-731(pm == null 路径 —— PowerManager 未移植;登记)
  // (the pm == null path — PowerManager unported; registered.)
  return int4_remaining_time_;
}

void ProductionItemLike::Tick(PlayerResources& pr) {
  // L758-809
  if (!b_started_) {
    const int time = Queue->GetBuildTime(*p_ai_);
    if (time > 0)
      int4_remaining_time_ = int4_total_time_ = time;
    b_started_ = true;
  }

  if (b_done_) {
    if (OnComplete)
      OnComplete(this);
    return;
  }

  if (b_paused_)
    return;

  // PowerManager 的 Slowdown 面未移植(pm == null 跳过;登记)
  // PowerManager's Slowdown face is unported (the pm == null skip;
  // registered).

  if (!Queue->Info().b_pay_up_front) {
    const int expected_remaining_cost =
        int4_remaining_time_ == 1
            ? 0
            : TotalCost * int4_remaining_time_ /
                  std::max(1, int4_total_time_);
    const int cost_this_frame = RemainingCost - expected_remaining_cost;
    if (pr.Resources > 0 && pr.Resources <= cost_this_frame)
      ResourcesPaid += pr.Resources;
    else if (pr.Resources > cost_this_frame)
      ResourcesPaid += cost_this_frame;
    if (cost_this_frame != 0 && !pr.TakeCash(cost_this_frame, true)) {
      ResourcesPaid -= pr.Resources;
      return;
    }

    RemainingCost -= cost_this_frame;
  }

  int4_remaining_time_--;
  if (int4_remaining_time_ > 0)
    return;

  b_done_ = true;
}

}  // namespace ora::mods
