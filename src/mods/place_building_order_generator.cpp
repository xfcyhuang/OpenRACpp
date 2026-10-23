// UPSTREAM: OpenRA.Mods.Common/Orders/PlaceBuildingOrderGenerator.cs @e180e3225
//          (实现部分)
//          The implementation half.
#include "mods/place_building_order_generator.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/generic_record.hpp"
#include "mods/ai_utils.hpp"
#include "mods/building_utils.hpp"
#include "mods/place_building.hpp"
#include "mods/production.hpp"
#include "net/order.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/selection.hpp"
#include "sim/target.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"

namespace ora::mods {

namespace {

PlaceBuildingCellType MakeCellType(bool b_valid, bool b_line_build = false) {
  PlaceBuildingCellType cell = b_valid ? PlaceBuildingCellType::Valid
                                       : PlaceBuildingCellType::Invalid;
  if (b_line_build)
    cell = cell | PlaceBuildingCellType::LineBuild;
  return cell;
}

}  // namespace

PlaceBuildingOrderGenerator::VariantWrapper::VariantWrapper(
    gfx::WorldRenderer& wr, ProductionQueue& queue, const game::ActorInfo& ai)
    : actor_info{&ai} {
  if (const meta::RecordObject* rec_building =
          FindTraitInfoRecord(ai, "BuildingInfo"))
    building_info = BuildingInfoData::Parse(*rec_building);
  if (const meta::RecordObject* rec_plug = FindTraitInfoRecord(ai, "PlugInfo"))
    opt_plug_info = PlugInfoData::Parse(*rec_plug);
  if (const meta::RecordObject* rec_line =
          FindTraitInfoRecord(ai, "LineBuildInfo"))
    opt_line_build_info = LineBuildInfoData::Parse(*rec_line);

  const meta::RecordObject* rec_preview_generator = nullptr;
  for (const meta::RecordObject* rec :
       ai.TraitInfosByInterface(
           "OpenRA.Mods.Common.Orders.IPlaceBuildingPreviewGeneratorInfo")) {
    rec_preview_generator = rec;
    break;
  }

  if (rec_preview_generator != nullptr) {
    std::string faction;
    std::string str_force_faction;
    if (const meta::RecordObject* rec_buildable =
            FindTraitInfoRecord(ai, "BuildableInfo"))
      str_force_faction =
          BuildableInfoData::Parse(*rec_buildable).str_force_faction;

    if (!str_force_faction.empty())
      faction = str_force_faction;
    else {
      Production* most_likely_producer = queue.MostLikelyProducer();
      faction = most_likely_producer != nullptr &&
                        !most_likely_producer->Faction().empty()
                    ? most_likely_producer->Faction()
                    : queue.Actor()->Owner()->Faction().InternalName;
    }

    sim::TypeDictionary td;
    td.Add(wr.World().Arena().Create<sim::FactionInit>(faction));
    td.Add(wr.World().Arena().Create<sim::OwnerInit>(queue.Actor()->Owner()));

    for (const meta::RecordObject* rec :
         ai.TraitInfosByInterface("OpenRA.Traits.IActorPreviewInitInfo")) {
      static_cast<void>(rec);
    }

    if (rec_preview_generator->record_desc().str_name ==
        "FootprintPlaceBuildingPreviewInfo") {
      ptr_preview = CreateFootprintPlaceBuildingPreview(
          wr, ai,
          FootprintPlaceBuildingPreviewInfoData::Parse(*rec_preview_generator));
    }
  }
}

PlaceBuildingOrderGenerator::PlaceBuildingOrderGenerator(
    ProductionQueue& queue, std::string str_name,
    gfx::WorldRenderer& world_renderer)
    : ptr_queue_{&queue},
      ptr_world_{&queue.Actor()->world()},
      ptr_viewport_{world_renderer.Viewport()},
      ptr_world_renderer_{&world_renderer} {
  if (ui::Ui::HasInstance() && ui::Ui::Instance().ChromeMetricsDeps() != nullptr)
    str_world_default_cursor =
        ui::Ui::Instance().ChromeMetricsDeps()->Get("WorldDefaultCursor");

  game_settings_.mouse_control_style =
      ui::Ui::Instance().GameSettingsInput().kind_mouse_control_style;

  if (const sim::Actor* player_actor =
          queue.Actor()->Owner()->PlayerActor()) {
    if (const game::ActorInfo* info = player_actor->Info()) {
      if (const meta::RecordObject* rec =
              FindTraitInfoRecord(*info, "PlaceBuildingInfo"))
        place_building_info_ = PlaceBuildingInfoData::Parse(*rec);
    }
  }

  if (game_settings_.mouse_control_style == MouseControlStyle::Classic)
    ptr_world_->Selection()->Clear();

  const game::ActorInfo* first_info =
      ptr_world_->Map().Rules().FindActor(str_name);
  if (first_info == nullptr)
    return;
  vec_variants_.push_back(std::make_unique<VariantWrapper>(
      world_renderer, queue, *first_info));

  const game::ActorInfo& first = *vec_variants_.front()->actor_info;
  for (const meta::RecordObject* rec :
       TraitInfoRecords(first, "PlaceBuildingVariantsInfo")) {
    const PlaceBuildingVariantsInfoData data =
        PlaceBuildingVariantsInfoData::Parse(*rec);
    for (const std::string& str_actor : data.vec_actors) {
      const game::ActorInfo* variant_info =
          ptr_world_->Map().Rules().FindActor(str_actor);
      if (variant_info == nullptr)
        continue;
      vec_variants_.push_back(std::make_unique<VariantWrapper>(
          world_renderer, queue, *variant_info));
    }
  }
}

std::vector<std::unique_ptr<net::Order>> PlaceBuildingOrderGenerator::Order(
    sim::World& world, CPos cell, int2 world_pixel, const MouseInput& mi) {
  const MouseButton action_button =
      game_settings_.ResolveActionButton(MouseActionType::PlaceBuilding);
  const MouseButton cancel_button =
      game_settings_.ResolveCancelButton(MouseActionType::PlaceBuilding);
  if ((mi.Button == action_button && mi.Event == MouseInputEvent::Down) ||
      (mi.Button == cancel_button && mi.Event == MouseInputEvent::Up)) {
    if (mi.Button == cancel_button)
      world.CancelInputMode();

    std::vector<std::unique_ptr<net::Order>> vec_ret =
        InnerOrder(world, cell, mi);

    bool b_placed = false;
    for (const auto& up_o : vec_ret)
      if (up_o->str_order_string == "PlaceBuilding" ||
          up_o->str_order_string == "LineBuild" ||
          up_o->str_order_string == "PlacePlug")
        b_placed = true;
    if (b_placed)
      world.CancelInputMode();

    return vec_ret;
  }

  return {};
}

CPos PlaceBuildingOrderGenerator::TopLeft() const {
  int2 offset_pos = gfx::Viewport::int2_last_mouse_pos;
  if (!vec_variants_.empty() && vec_variants_[n_variant_]->ptr_preview !=
                                     nullptr)
    offset_pos = ptr_viewport_->WorldToViewPx(
        ptr_viewport_->ViewToWorldPx(offset_pos) +
        vec_variants_[n_variant_]->ptr_preview->TopLeftScreenOffset());

  return ptr_viewport_->ViewToWorld(offset_pos);
}

std::vector<std::unique_ptr<net::Order>>
PlaceBuildingOrderGenerator::InnerOrder(
    sim::World& world, CPos, const MouseInput& mi) {
  if (world.Paused())
    return {};

  sim::Player& owner = *ptr_queue_->Actor()->Owner();
  const VariantWrapper& variant = *vec_variants_[n_variant_];

  std::vector<std::unique_ptr<net::Order>> vec_out;
  if (mi.Button == ActionButton()) {
    std::string order_type = "PlaceBuilding";
    const CPos top_left = TopLeft();

    if (variant.opt_plug_info.has_value()) {
      order_type = "PlacePlug";
      if (!AcceptsPlug(top_left, *variant.opt_plug_info)) {
        net::TextNotificationsManager::Instance().AddTransientLine(
            &owner, place_building_info_.str_cannot_place_text_notification);
        return {};
      }
    } else {
      if (!CanPlaceBuilding(world, top_left, variant.actor_info,
                            variant.building_info, nullptr) ||
          !variant.building_info.IsCloseEnoughToBase(
              world, owner, *variant.actor_info, top_left)) {
        vec_out = ClearBlockersOrders(top_left);

        net::TextNotificationsManager::Instance().AddTransientLine(
            &owner, place_building_info_.str_cannot_place_text_notification);
        return vec_out;
      }

      if (variant.opt_line_build_info.has_value() &&
          !HasModifier(mi.ModifierFlags, Modifiers::Shift))
        order_type = "LineBuild";
    }

    auto up_o = std::make_unique<net::Order>(
        order_type, owner.PlayerActor(),
        sim::Target::FromCell(world, top_left), false);
    up_o->str_target_string = vec_variants_.front()->actor_info->Name();
    up_o->uint4_extra_data = ptr_queue_->Actor()->ActorID();
    up_o->extra_location = CPos{static_cast<std::int32_t>(n_variant_), 0};
    up_o->b_suppress_visual_feedback = true;
    vec_out.push_back(std::move(up_o));
  }

  return vec_out;
}

void PlaceBuildingOrderGenerator::Tick(sim::World& world) {
  bool b_done = false;
  for (const auto& queued : ptr_queue_->AllQueued())
    if (queued->Done() &&
        queued->Item == vec_variants_.front()->actor_info->Name())
      b_done = true;
  if (!b_done)
    world.CancelInputMode();

  for (const auto& v : vec_variants_)
    if (v->ptr_preview != nullptr)
      v->ptr_preview->Tick();
}

bool PlaceBuildingOrderGenerator::AcceptsPlug(
    CPos cell, const PlugInfoData& plug) const {
  for (sim::Actor* a : ptr_world_->ActorMapFace()->GetActorsAt(cell))
    for (Pluggable* p : a->TraitsImplementing<Pluggable>())
      if (p->AcceptsPlug(plug.str_type))
        return true;

  return false;
}

std::vector<gfx::IRenderable*> PlaceBuildingOrderGenerator::Render(
    gfx::WorldRenderer&, sim::World&) {
  return {};
}

std::vector<gfx::IRenderable*>
PlaceBuildingOrderGenerator::RenderAboveShroud(gfx::WorldRenderer& wr,
                                               sim::World& world) {
  const CPos top_left = TopLeft();
  std::vector<std::pair<CPos, PlaceBuildingCellType>> vec_footprint;
  const VariantWrapper& variant = *vec_variants_[n_variant_];
  const sim::Player& owner = *ptr_queue_->Actor()->Owner();

  if (variant.opt_plug_info.has_value()) {
    vec_footprint.emplace_back(
        top_left, MakeCellType(AcceptsPlug(top_left, *variant.opt_plug_info)));
  } else if (variant.opt_line_build_info.has_value() &&
             owner.GetShroud() != nullptr &&
             owner.GetShroud()->IsExplored(top_left)) {
    const game::ActorInfo* segment_info = variant.actor_info;
    BuildingInfoData segment_building_info = variant.building_info;
    if (!variant.opt_line_build_info->str_segment_type.empty()) {
      const game::ActorInfo* segment_found = world.Map().Rules().FindActor(
          variant.opt_line_build_info->str_segment_type);
      if (segment_found != nullptr) {
        segment_info = segment_found;
        if (const meta::RecordObject* rec_b =
                FindTraitInfoRecord(*segment_info, "BuildingInfo"))
          segment_building_info = BuildingInfoData::Parse(*rec_b);
      }
    }

    if (!HasModifier(ui::Ui::Instance().GetModifierKeys(),
                     Modifiers::Shift))
      for (const auto& [cell, segment_actor] :
           GetLineBuildCells(world, top_left, *variant.actor_info,
                             variant.building_info,
                             *ptr_queue_->Actor()->Owner())) {
        static_cast<void>(segment_actor);
      const bool b_line_buildable =
          IsCellBuildable(world, cell, segment_info, segment_building_info);
      const bool b_line_close_enough = segment_building_info.IsCloseEnoughToBase(
          world, *world.LocalPlayer(), *segment_info, cell);
        vec_footprint.emplace_back(
            cell, MakeCellType(b_line_buildable && b_line_close_enough, true));
      }

    const bool b_buildable =
        IsCellBuildable(world, top_left, variant.actor_info,
                        variant.building_info);
    const bool b_close_enough = variant.building_info.IsCloseEnoughToBase(
        world, *world.LocalPlayer(), *variant.actor_info, top_left);
    bool b_top_left_found = false;
    for (auto& [cell, kind_type] : vec_footprint)
      if (cell == top_left) {
        kind_type = MakeCellType(b_buildable && b_close_enough);
        b_top_left_found = true;
      }
    if (!b_top_left_found)
      vec_footprint.emplace_back(
          top_left, MakeCellType(b_buildable && b_close_enough));
  } else {
    const bool b_is_close_enough = variant.building_info.IsCloseEnoughToBase(
        world, *world.LocalPlayer(), *variant.actor_info, top_left);
    for (const CPos t : variant.building_info.Tiles(top_left))
      vec_footprint.emplace_back(
          t, MakeCellType(b_is_close_enough &&
                              IsCellBuildable(world, t, variant.actor_info,
                                              variant.building_info)));
  }

  return variant.ptr_preview != nullptr
             ? variant.ptr_preview->Render(wr, top_left, vec_footprint)
             : std::vector<gfx::IRenderable*>{};
}

std::vector<gfx::IRenderable*>
PlaceBuildingOrderGenerator::RenderAnnotations(gfx::WorldRenderer& wr,
                                               sim::World&) {
  const VariantWrapper& variant = *vec_variants_[n_variant_];
  return variant.ptr_preview != nullptr
             ? variant.ptr_preview->RenderAnnotations(wr, TopLeft())
             : std::vector<gfx::IRenderable*>{};
}

bool PlaceBuildingOrderGenerator::HandleKeyPress(const KeyInput& e_input) {
  if (!vec_variants_.empty() &&
      place_building_info_.hk_toggle_variant_key.IsActivatedBy(e_input)) {
    n_variant_++;
    if (n_variant_ >= vec_variants_.size())
      n_variant_ = 0;

    return true;
  }

  return false;
}

std::string PlaceBuildingOrderGenerator::GetCursor(sim::World& world,
                                                   CPos cell,
                                                   int2 world_pixel,
                                                   const MouseInput& mi) {
  return GetCursorOverride(world, cell, world_pixel, mi);
}

std::string PlaceBuildingOrderGenerator::GetCursorOverride(sim::World&,
                                                           CPos, int2,
                                                           const MouseInput&) {
  return str_world_default_cursor;
}

std::vector<std::unique_ptr<net::Order>>
PlaceBuildingOrderGenerator::ClearBlockersOrders(
    CPos top_left) {
  return mods::ClearBlockersOrders(
      vec_variants_[n_variant_]->building_info.Tiles(top_left),
      *ptr_queue_->Actor()->Owner());
}

}  // namespace ora::mods
