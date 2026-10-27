#include "mods/production_support.hpp"

#include "game/actor_info.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "net/order.hpp"
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

std::optional<std::vector<CVec>> RecCVecArray(const meta::RecordObject& rec,
                                              std::string_view str_name) {
  std::vector<CVec> vec_out;
  bool found = false;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        found = true;
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val))
          for (const auto& element : *list)
            if (auto* tuple = std::get_if<meta::GenericTuple>(
                    &element.val))
              vec_out.emplace_back(
                  static_cast<std::int32_t>((*tuple).arr_ints[0]),
                  static_cast<std::int32_t>((*tuple).arr_ints[1]));
      }
  }
  return found ? std::optional<std::vector<CVec>>{std::move(vec_out)}
               : std::nullopt;
}

std::optional<WVec> RecWVec(const meta::RecordObject& rec,
                            std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* tuple = std::get_if<meta::GenericTuple>(&v.val))
          return WVec{static_cast<std::int32_t>((*tuple).arr_ints[0]),
                      static_cast<std::int32_t>((*tuple).arr_ints[1]),
                      static_cast<std::int32_t>((*tuple).arr_ints[2])};
        return std::nullopt;
      }
  }
  return std::nullopt;
}

std::string StripChar(std::string_view sv, char ch) {
  std::string out;
  out.reserve(sv.size());
  for (const char c : sv)
    if (c != ch)
      out.push_back(c);
  return out;
}

}

ValuedInfoData ValuedInfoData::Parse(const meta::RecordObject& rec_info) {
  ValuedInfoData data;
  if (const auto v = RecInt(rec_info, "Cost"))
    data.int4_cost = static_cast<int>(*v);
  return data;
}

BuildableInfoData BuildableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  BuildableInfoData data;
  data.vec_prerequisites = RecStringArray(rec_info, "Prerequisites");
  data.vec_queue = RecStringArray(rec_info, "Queue");
  if (const auto v = RecString(rec_info, "BuildAtProductionType"))
    data.str_build_at_production_type = *v;
  if (const auto v = RecInt(rec_info, "BuildLimit"))
    data.int4_build_limit = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "ForceFaction"))
    data.str_force_faction = *v;
  if (const auto v = RecString(rec_info, "Icon"))
    data.str_icon = *v;
  if (const auto v = RecString(rec_info, "IconPalette"))
    data.str_icon_palette = *v;
  if (const auto v = RecInt(rec_info, "IconPaletteIsPlayerPalette"))
    data.b_icon_palette_is_player_palette = *v != 0;
  if (const auto v = RecInt(rec_info, "BuildDuration"))
    data.int4_build_duration = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "BuildDurationModifier"))
    data.int4_build_duration_modifier = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "BuildPaletteOrder"))
    data.int4_build_palette_order = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "Description"))
    data.str_description = *v;
  return data;
}

std::string BuildableInfoData::GetInitialFaction(
    const game::ActorInfo& ai, const std::string& default_faction) {

  for (const meta::RecordObject* rec_trait :
       ai.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.BuildableInfo"}) {
      if (const auto v = RecString(*rec_trait, "ForceFaction"))
        return *v;
      return default_faction;
    }
  return default_faction;
}

ExitInfoData ExitInfoData::Parse(const meta::RecordObject& rec_info) {
  ExitInfoData data;
  if (const auto v = RecWVec(rec_info, "SpawnOffset"))
    data.spawn_offset = *v;
  if (const auto v = RecInt(rec_info, "ExitCell")) {

    if (const auto* generated =
            dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
      const std::vector<const meta::FieldDesc*> fields =
          meta::CollectFields(generated->record_desc());
      for (std::size_t i = 0; i < fields.size(); i++)
        if (fields[i]->str_name == "ExitCell") {
          const meta::GenericValue& v_slot = generated->Slot(i);
          if (auto* tuple =
                  std::get_if<meta::GenericTuple>(&v_slot.val))
            data.exit_cell =
                CVec{static_cast<std::int32_t>((*tuple).arr_ints[0]),
                     static_cast<std::int32_t>((*tuple).arr_ints[1])};
        }
    }
    (void)v;
  }
  if (const auto v = RecInt(rec_info, "Facing"))
    data.opt_facing = WAngle{static_cast<std::int32_t>(*v)};
  data.vec_production_types = RecStringArray(rec_info, "ProductionTypes");
  if (const auto v = RecInt(rec_info, "ExitDelay"))
    data.int4_exit_delay = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "Priority"))
    data.int4_priority = static_cast<int>(*v);
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

void Reservable::Tick(sim::Actor& self) {

  if (p_reserved_for_ == nullptr)
    return;

  if (!sim::Target::FromActor(p_reserved_for_).IsValidFor(&self)) {

    UnReserve();
  }
}

void Reservable::OnOwnerChanged(sim::Actor&, sim::Player&,
                                sim::Player&) {
  UnReserve();
}

void Reservable::Selling(sim::Actor&) { UnReserve(); }
void Reservable::Sold(sim::Actor&) { UnReserve(); }
void Reservable::Disposing(sim::Actor&) { UnReserve(); }

bool Reservable::IsReserved(const sim::Actor& a) {

  const auto* res = const_cast<sim::Actor&>(a).TraitOrDefault<Reservable>();
  return res != nullptr && res->p_reserved_for_ != nullptr;
}

bool Reservable::IsAvailableFor(sim::Actor& reservable,
                                sim::Actor* for_actor) {

  auto* res = reservable.TraitOrDefault<Reservable>();
  return res == nullptr || res->p_reserved_for_ == nullptr ||
         res->p_reserved_for_ == for_actor;
}

void Reservable::Reserve(sim::Actor* for_actor) {

  p_reserved_for_ = for_actor;
}

void Reservable::UnReserve() {
  p_reserved_for_ = nullptr;
}

RallyPointInfoData RallyPointInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RallyPointInfoData data;
  if (const auto v = RecCVecArray(rec_info, "Path"))
    data.vec_path = std::move(*v);
  return data;
}

RallyPoint::RallyPoint(sim::Actor& self, RallyPointInfoData info)
    : info_{std::move(info)} {

  ResetPath(self);
}

void RallyPoint::ResetPath(sim::Actor& self) {

  Path.clear();
  for (const CVec p : info_.vec_path)
    Path.push_back(self.Location() + p);
}

void RallyPoint::OnOwnerChanged(sim::Actor& self, sim::Player&,
                                sim::Player&) {

  ResetPath(self);
}

void RallyPoint::ResolveOrder(sim::Actor& self,
                              const ora::net::Order& order) {

  if (order.str_order_string == "Stop") {
    Path.clear();
    return;
  }

  static constexpr std::string_view kOrderId = "SetRallyPoint";
  if (order.str_order_string != kOrderId)
    return;

  if (!order.target.IsValidFor(&self))
    return;

  if (!order.b_queued)
    Path.clear();

  Path.push_back(
      self.world().Map().CellContaining(order.target.CenterPosition()));
}

bool RallyPoint::IsForceSet(const ora::net::Order& order) {

  return order.str_order_string == "SetRallyPoint" &&
         order.uint4_extra_data == 1;
}

ProvidesPrerequisiteInfoData ProvidesPrerequisiteInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProvidesPrerequisiteInfoData data;
  if (const auto v = RecString(rec_info, "Prerequisite"))
    data.str_prerequisite = *v;
  data.vec_requires_prerequisites =
      RecStringArray(rec_info, "RequiresPrerequisites");
  data.vec_factions = RecStringArray(rec_info, "Factions");
  if (const auto v = RecInt(rec_info, "ResetOnOwnerChange"))
    data.b_reset_on_owner_change = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

ProvidesPrerequisite::ProvidesPrerequisite(sim::ActorInitializer& init,
                                           ProvidesPrerequisiteInfoData info)
    : sim::ConditionalTraitCore<ProvidesPrerequisite>{info.conditional},
      info_{std::move(info)} {

  if (info_.str_prerequisite.empty())
    vec_prerequisites_.push_back(init.Self().Info()->Name());
  else
    vec_prerequisites_.push_back(info_.str_prerequisite);

  str_faction_ = init.GetValue<sim::FactionInit>(
      init.Self().Owner()->Faction().InternalName);
}

void ProvidesPrerequisite::Created(sim::Actor& self) {

  p_tech_tree_ = self.Owner()->PlayerActor()->Trait<TechTree>();
  sim::ConditionalTraitCore<ProvidesPrerequisite>::CoreCreated(self);
  Update();
}

void ProvidesPrerequisite::OnOwnerChanged(sim::Actor& self,
                                          sim::Player&,
                                          sim::Player& new_owner) {

  p_tech_tree_ = new_owner.PlayerActor()->Trait<TechTree>();

  if (info_.b_reset_on_owner_change)
    str_faction_ = new_owner.Faction().Name;

  Update();
}

std::vector<std::string> ProvidesPrerequisite::ProvidesPrerequisites() {

  return b_enabled_ ? vec_prerequisites_ : std::vector<std::string>{};
}

void ProvidesPrerequisite::Update() {

  b_enabled_ = !IsTraitDisabled();
  if (IsTraitDisabled())
    return;

  if (!info_.vec_factions.empty())
    b_enabled_ = std::find(info_.vec_factions.begin(),
                           info_.vec_factions.end(),
                           str_faction_) != info_.vec_factions.end();

  if (!info_.vec_requires_prerequisites.empty() && b_enabled_)
    b_enabled_ =
        p_tech_tree_->HasPrerequisites(info_.vec_requires_prerequisites);
}

TechTree::TechTree(sim::ActorInitializer& init) {

  p_owner_ = init.Self().Owner();
  p_world_ = &init.Self().world();
  init.Self().world().AddActorAddedHandler(
      [this](sim::Actor& a) { ActorChanged(a); });
  init.Self().world().AddActorRemovedHandler(
      [this](sim::Actor& a) { ActorChanged(a); });
}

void TechTree::ActorChanged(sim::Actor& a) {

  bool has_buildable = false;
  int int4_build_limit = 0;
  for (const meta::RecordObject* rec_trait :
       a.Info()->TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.BuildableInfo"}) {
      has_buildable = true;
      if (const auto v = RecInt(*rec_trait, "BuildLimit"))
        int4_build_limit = static_cast<int>(*v);
      break;
    }

  if (a.Owner() == p_owner_ && int4_build_limit > 0)
    Update();
  (void)has_buildable;

}

void TechTree::Update() {

  const std::map<std::string, int> map_owned =
      GatherOwnedPrerequisites(p_owner_);
  for (Watcher& w : vec_watchers_)
    w.Update(map_owned);
}

void TechTree::Add(const std::string& key,
                   const std::vector<std::string>& vec_prerequisites,
                   int limit, sim::ITechTreeElement* tte) {

  vec_watchers_.push_back(Watcher{key, tte, vec_prerequisites, false,
                                  limit, false, false});
}

void TechTree::Remove(const std::string& key) {

  std::erase_if(vec_watchers_,
                [&](const Watcher& w) { return w.key == key; });
}

void TechTree::RemoveElement(sim::ITechTreeElement* tte) {

  std::erase_if(vec_watchers_,
                [&](const Watcher& w) { return w.registered_by == tte; });
}

bool TechTree::HasPrerequisites(
    const std::vector<std::string>& vec_prerequisites) {

  const std::map<std::string, int> map_owned =
      GatherOwnedPrerequisites(p_owner_);
  for (const std::string& p : vec_prerequisites) {
    const std::string without_tilde = StripChar(p, '~');
    const bool starts_bang = !without_tilde.empty() &&
                             without_tilde.front() == '!';
    const bool contains = map_owned.count(
                              StripChar(without_tilde, '!')) > 0;

    if (!(starts_bang != contains))
      return false;
  }
  return true;
}

std::map<std::string, int> TechTree::GatherOwnedPrerequisites(
    sim::Player* player) {

  std::map<std::string, int> map_ret;
  if (player == nullptr)
    return map_ret;

  for (auto& [actor, trait] :
       player->GetWorld().ActorsWithTrait<sim::ITechTreePrerequisite>()) {
    if (actor->Owner() != player || !actor->IsInWorld() ||
        actor->IsDead())
      continue;

    for (const std::string& p : trait->ProvidesPrerequisites()) {

      if (p.empty())
        continue;

      map_ret[p] = map_ret[p] + 1;
    }
  }

  for (auto& [actor, trait] :
       player->GetWorld().ActorsWithTrait<Buildable>()) {
    (void)trait;
    if (actor->Owner() != player || !actor->IsInWorld() ||
        actor->IsDead() || map_ret.count(actor->Info()->Name()) > 0)
      continue;

    int int4_build_limit = 0;
    for (const meta::RecordObject* rec_trait :
         actor->Info()->TraitsInConstructOrder())
      if (rec_trait->record_desc().str_full_name ==
          std::string_view{
              "OpenRA.Mods.Common.Traits.BuildableInfo"}) {
        if (const auto v = RecInt(*rec_trait, "BuildLimit"))
          int4_build_limit = static_cast<int>(*v);
        break;
      }
    if (int4_build_limit <= 0)
      continue;

    map_ret[actor->Info()->Name()] =
        map_ret[actor->Info()->Name()] + 1;
  }

  return map_ret;
}

void TechTree::Watcher::Update(
    const std::map<std::string, int>& map_owned) {

  bool has_reached_limit = false;
  if (int4_limit > 0) {
    const auto it = map_owned.find(key);
    has_reached_limit =
        it != map_owned.end() && it->second >= int4_limit;
  }

  bool now_has_prerequisites = !has_reached_limit;
  if (now_has_prerequisites) {
    for (const std::string& prereq : vec_prerequisites) {
      const std::string without_tilde = StripChar(prereq, '~');
      const bool starts_bang = !without_tilde.empty() &&
                               without_tilde.front() == '!';
      const bool contains =
          map_owned.count(StripChar(without_tilde, '!')) > 0;
      if (starts_bang != contains) {
        now_has_prerequisites = false;
        break;
      }
    }
  }

  bool now_hidden = false;
  for (const std::string& prereq : vec_prerequisites) {
    if (prereq.empty() || prereq.front() != '~')
      continue;
    const std::string without_tilde = StripChar(prereq, '~');
    const bool starts_bang = !without_tilde.empty() &&
                             without_tilde.front() == '!';
    const bool contains =
        map_owned.count(StripChar(without_tilde, '!')) > 0;
    if (starts_bang != contains) {
      now_hidden = true;
      break;
    }
  }

  if (!b_initialized) {
    b_initialized = true;
    b_has_prerequisites = !now_has_prerequisites;
    b_hidden = !now_hidden;
  }

  if (now_hidden && !b_hidden)
    registered_by->PrerequisitesItemHidden(key);

  if (!now_hidden && b_hidden)
    registered_by->PrerequisitesItemVisible(key);

  if (now_has_prerequisites && !b_has_prerequisites)
    registered_by->PrerequisitesAvailable(key);

  if (!now_has_prerequisites && b_has_prerequisites)
    registered_by->PrerequisitesUnavailable(key);

  b_hidden = now_hidden;
  b_has_prerequisites = now_has_prerequisites;
}

int DeveloperModeSyncHash(const sim::ISync* s) {
  const auto* dev = static_cast<const DeveloperMode*>(s);
  int acc = 0;
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->FastCharge));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->AllTech));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->FastBuild));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->DisableShroud));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->PathDebug));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->UnlimitedPower));
  acc = sim::sync::CombineSyncHash(acc, sim::sync::HashBool(dev->BuildAnywhere));
  return acc;
}

const bool b_developer_mode_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.DeveloperMode",
                                &DeveloperModeSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_developer_mode_sync_registered_anchor =
    &b_developer_mode_sync_registered;

}
