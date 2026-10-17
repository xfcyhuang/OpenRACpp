// UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlaceBuilding.cs @e180e3225
//          (实现部分)+ Traits/Buildings/PlaceBuildingVariants.cs +
//          Traits/Replacement.cs + Traits/Replaceable.cs
//          The implementation halves.
#include "mods/place_building.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "meta/generic_record.hpp"
#include "meta/variable_expression.hpp"
#include "mods/building.hpp"
#include "mods/building_utils.hpp"
#include "mods/line_build.hpp"
#include "mods/plug.hpp"
#include "mods/production.hpp"
#include "net/order.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "ui/hotkey.hpp"

namespace ora::mods {

PlaceBuildingInfoData PlaceBuildingInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PlaceBuildingInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "NewOptionsNotificationDelay"))
    data.int4_new_options_notification_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldString(rec_info, "NewOptionsNotification"))
    data.str_new_options_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "NewOptionsTextNotification"))
    data.str_new_options_text_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "CannotPlaceNotification"))
    data.str_cannot_place_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "CannotPlaceTextNotification"))
    data.str_cannot_place_text_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "ToggleVariantKey"))
    if (!v->empty()) {
      ui::Hotkey hk_parsed{};
      if (ui::Hotkey::TryParse(*v, hk_parsed))
        data.hk_toggle_variant_key =
            ui::HotkeyReference([hk_parsed] { return hk_parsed; });
    }
  return data;
}

PlaceBuildingVariantsInfoData PlaceBuildingVariantsInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PlaceBuildingVariantsInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Actors"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_actors.push_back(*s);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Facings"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* tuple = std::get_if<meta::GenericTuple>(&item.val))
          data.vec_facings.push_back(
              WAngle{static_cast<std::int32_t>(tuple->arr_ints[0])});
  return data;
}

ReplacementInfoData ReplacementInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ReplacementInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ReplaceableTypes"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_replaceable_types.push_back(*s);
  return data;
}

ReplaceableInfoData ReplaceableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ReplaceableInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Types"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_types.push_back(*s);
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

PlaceBuilding::PlaceBuilding(sim::ActorInitializer& init,
                             PlaceBuildingInfoData info)
    : info_{std::move(info)},
      hk_toggle_variant_key{info_.hk_toggle_variant_key} {
  p_self_ = &init.Self();
}

void PlaceBuilding::ResolveOrder(sim::Actor& self,
                                 const ora::net::Order& order) {
  const std::string_view os = order.str_order_string;
  if (os != "PlaceBuilding" && os != "LineBuild" && os != "PlacePlug")
    return;

  sim::World& world = self.world();
  const net::Order order_copy = order;
  const std::string str_order_kind{os};
  world.AddFrameEndTask([this, &self, order_copy, str_order_kind](sim::World& w) {
    const net::Order& order = order_copy;

    const int prev_items = GetNumBuildables(*self.Owner());
    sim::Actor* target_actor = w.GetActorById(order.uint4_extra_data);
    const CPos target_location =
        w.Map().CellContaining(order.target.CenterPosition());

    if (target_actor == nullptr || target_actor->IsDead())
      return;

    const game::ActorInfo* actor_info =
        w.Map().Rules().FindActor(*order.str_target_string);
    if (actor_info == nullptr)
      return;

    ProductionQueue* queue = nullptr;
    for (ProductionQueue* q :
         target_actor->TraitsImplementing<ProductionQueue>())
      if (q->CanBuild(*actor_info)) {
        bool b_done_item = false;
        for (const auto& item : q->AllQueued())
          if (item->Done() && item->Item == *order.str_target_string)
            b_done_item = true;
        if (b_done_item) {
          queue = q;
          break;
        }
      }

    if (queue == nullptr)
      return;

    ProductionItemLike* item = nullptr;
    for (const auto& queued : queue->AllQueued())
      if (queued->Done() && queued->Item == *order.str_target_string) {
        item = queued.get();
        break;
      }

    if (item == nullptr)
      return;

    if (order.extra_location.X() > 0) {
      std::vector<std::string> vec_variants;
      for (const meta::RecordObject* rec :
           TraitInfoRecords(*actor_info, "PlaceBuildingVariantsInfo")) {
        const PlaceBuildingVariantsInfoData data =
            PlaceBuildingVariantsInfoData::Parse(*rec);
        vec_variants.insert(vec_variants.end(), data.vec_actors.begin(),
                            data.vec_actors.end());
      }

      if (static_cast<std::size_t>(order.extra_location.X() - 1) <
          vec_variants.size()) {
        const game::ActorInfo* variant_info = w.Map().Rules().FindActor(
            vec_variants[static_cast<std::size_t>(
                order.extra_location.X() - 1)]);
        if (variant_info != nullptr)
          actor_info = variant_info;
      }
    }

    Production* producer = queue->MostLikelyProducer();
    std::string faction =
        producer != nullptr && !producer->Faction().empty()
            ? producer->Faction()
            : self.Owner()->Faction().InternalName;

    const meta::RecordObject* rec_building =
        FindTraitInfoRecord(*actor_info, "BuildingInfo");
    if (rec_building == nullptr)
      return;
    const BuildingInfoData building_info = BuildingInfoData::Parse(*rec_building);

    if (const meta::RecordObject* rec_buildable =
            FindTraitInfoRecord(*actor_info, "BuildableInfo")) {
      const BuildableInfoData buildable =
          BuildableInfoData::Parse(*rec_buildable);
      if (!buildable.str_force_faction.empty())
        faction = buildable.str_force_faction;
    }

    std::set<std::string> set_replaceable_types;
    for (const meta::RecordObject* rec :
         TraitInfoRecords(*actor_info, "ReplacementInfo")) {
      const ReplacementInfoData data = ReplacementInfoData::Parse(*rec);
      for (const std::string& str_type : data.vec_replaceable_types)
        set_replaceable_types.insert(str_type);
    }

    if (!set_replaceable_types.empty())
      for (const CPos t : building_info.Tiles(target_location))
        for (sim::Actor* a : w.ActorMapFace()->GetActorsAt(t))
          for (Replaceable* r : a->TraitsImplementing<Replaceable>())
            if (!r->IsTraitDisabled())
              for (const std::string& str_type : r->Info().vec_types)
                if (set_replaceable_types.count(str_type) != 0) {
                  w.Remove(a);
                  break;
                }

    const auto notify_building_placed = [&](sim::Actor& placed) {
      if (producer != nullptr)
        for (auto* nbp : producer->HostActor()
                             .TraitsImplementing<sim::INotifyBuildingPlaced>())
          nbp->BuildingPlaced(producer->HostActor(), placed);

      for (auto* nbp : self.TraitsImplementing<sim::INotifyBuildingPlaced>())
        nbp->BuildingPlaced(self, placed);
    };

    if (str_order_kind == "LineBuild") {
      sim::TypeDictionary td;
      td.Add(w.Arena().Create<sim::LocationInit>(target_location));
      td.Add(w.Arena().Create<sim::OwnerInit>(order.Player()));
      td.Add(w.Arena().Create<sim::FactionInit>(faction));
      td.Add(w.Arena().Create<PlaceBuildingInit>());
      sim::Actor* placed = w.CreateActor(actor_info->Name(), td);

      notify_building_placed(*placed);

      std::string segment_type;
      if (const meta::RecordObject* rec_line =
              FindTraitInfoRecord(*actor_info, "LineBuildInfo")) {
        const LineBuildInfoData line = LineBuildInfoData::Parse(*rec_line);
        segment_type = line.str_segment_type;
      }
      if (segment_type.empty())
        segment_type = actor_info->Name();

      for (const auto& [cell, segment_actor] :
           GetLineBuildCells(w, target_location, *actor_info, building_info,
                             *order.Player())) {
        if (cell == target_location)
          continue;

        const game::ActorInfo* segment_info =
            w.Map().Rules().FindActor(segment_type);
        if (segment_info == nullptr)
          continue;

        std::set<std::string> set_replaceable_segments;
        for (const meta::RecordObject* rec :
             TraitInfoRecords(*segment_info, "ReplacementInfo")) {
          const ReplacementInfoData data = ReplacementInfoData::Parse(*rec);
          for (const std::string& str_type : data.vec_replaceable_types)
            set_replaceable_segments.insert(str_type);
        }

        if (!set_replaceable_segments.empty())
          for (sim::Actor* a : w.ActorMapFace()->GetActorsAt(cell))
            for (Replaceable* r : a->TraitsImplementing<Replaceable>())
              if (!r->IsTraitDisabled())
                for (const std::string& str_type : r->Info().vec_types)
                  if (set_replaceable_segments.count(str_type) != 0) {
                    w.Remove(a);
                    break;
                  }

        sim::TypeDictionary td_segment;
        td_segment.Add(w.Arena().Create<sim::LocationInit>(cell));
        td_segment.Add(w.Arena().Create<sim::OwnerInit>(order.Player()));
        td_segment.Add(w.Arena().Create<sim::FactionInit>(faction));
        td_segment.Add(w.Arena().Create<LineBuildDirectionInit>(
            cell.X() == target_location.X()
                ? LineBuildDirection::Y
                : LineBuildDirection::X));
        td_segment.Add(w.Arena().Create<LineBuildParentInit>(
            std::vector<sim::Actor*>{segment_actor, placed}));
        td_segment.Add(w.Arena().Create<PlaceBuildingInit>());
        sim::Actor* segment = w.CreateActor(segment_info->Name(), td_segment);

        notify_building_placed(*segment);
      }
    } else if (str_order_kind == "PlacePlug") {
      const meta::RecordObject* rec_plug =
          FindTraitInfoRecord(*actor_info, "PlugInfo");
      if (rec_plug == nullptr)
        return;
      const PlugInfoData plug_info = PlugInfoData::Parse(*rec_plug);

      for (sim::Actor* a : w.ActorMapFace()->GetActorsAt(target_location)) {
        Pluggable* pluggable = nullptr;
        std::vector<Pluggable*> vec_candidates;
        for (Pluggable* p : a->TraitsImplementing<Pluggable>())
          if (p->AcceptsPlug(plug_info.str_type))
            vec_candidates.push_back(p);

        for (Pluggable* p : vec_candidates)
          if (a->Location() + p->Info().cvec_offset == target_location)
            pluggable = p;
        if (pluggable == nullptr && !vec_candidates.empty())
          pluggable = vec_candidates.front();

        if (pluggable == nullptr)
          return;

        pluggable->EnablePlug(*a, plug_info.str_type);
      }
    } else {
      if (!CanPlaceBuilding(w, target_location, actor_info, building_info,
                            nullptr) ||
          !building_info.IsCloseEnoughToBase(w, *order.Player(), *actor_info,
                                             target_location))
        return;

      sim::TypeDictionary td;
      td.Add(w.Arena().Create<sim::LocationInit>(target_location));
      td.Add(w.Arena().Create<sim::OwnerInit>(order.Player()));
      td.Add(w.Arena().Create<sim::FactionInit>(faction));
      td.Add(w.Arena().Create<PlaceBuildingInit>());
      sim::Actor* building = w.CreateActor(actor_info->Name(), td);

      notify_building_placed(*building);
    }

    queue->EndProduction(*item);

    if (building_info.b_requires_base_provider)
      static_cast<void>(building_info.FindBaseProvider(w, *self.Owner(),
                                                       target_location));

    if (GetNumBuildables(*self.Owner()) > prev_items)
      b_trigger_notification = true;
  });
}

void PlaceBuilding::Tick(sim::Actor& self) {
  if (!b_trigger_notification)
    return;

  if (int4_tick_++ >= info_.int4_new_options_notification_delay)
    PlayNotification(self);
}

void PlaceBuilding::PlayNotification(sim::Actor& self) {
  net::TextNotificationsManager::Instance().AddTransientLine(
      self.Owner(), info_.str_new_options_text_notification);

  b_trigger_notification = false;
  int4_tick_ = 0;
}

int PlaceBuilding::GetNumBuildables(sim::Player& p) {
  if (p.world_ptr() == nullptr)
    return 0;
  sim::World& world = p.GetWorld();
  if (world.LocalPlayer() != &p)
    return 0;

  std::set<std::string> set_names;
  for (const auto& pair : world.ActorsWithTrait<ProductionQueue>())
    if (pair.actor->Owner() == &p)
      for (const game::ActorInfo* buildable : pair.trait->BuildableItems())
        set_names.insert(buildable->Name());
  return static_cast<int>(set_names.size());
}

Replaceable::Replaceable(sim::ActorInitializer& init, ReplaceableInfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  static_cast<void>(init);
}

}  // namespace ora::mods
