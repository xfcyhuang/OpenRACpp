#include "mods/wands.hpp"

#include "core/wrot.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/mobile.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

WandersInfoData WandersInfoData::Parse(const meta::RecordObject& rec_info) {
  WandersInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "WanderMoveRadius"))
    data.int4_wander_move_radius = static_cast<int>(*v);
  if (const auto v =
          sim::RecordFieldInt(rec_info, "ReduceMoveRadiusDelay"))
    data.int4_reduce_move_radius_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "MinMoveDelay"))
    data.int4_min_move_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxMoveDelay"))
    data.int4_max_move_delay = static_cast<int>(*v);

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "AvoidTerrainTypes") {

        if (auto* s = std::get_if<std::string>(&v.val)) {
          std::string str_current;
          for (const char ch : *s + ",") {
            if (ch == ',') {
              const std::size_t begin =
                  str_current.find_first_not_of(' ');
              const std::size_t end =
                  str_current.find_last_not_of(' ');
              if (begin != std::string::npos)
                data.vec_avoid_terrain_types.emplace_back(
                    str_current.substr(
                        begin, end - begin + 1));
              str_current.clear();
            } else {
              str_current.push_back(ch);
            }
          }
        } else if (auto* list =
                       std::get_if<std::vector<meta::GenericValue>>(
                           &v.val)) {
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              data.vec_avoid_terrain_types.push_back(*s);
        }
      }
    }
  }

  return data;
}

Wanders::Wanders(ActorInitializer& init, WandersInfoData&& info)
    : sim::ConditionalTraitCore<Wanders>(info.conditional),
      info_{std::move(info)} {
  int4_effective_move_radius = info_.int4_wander_move_radius;
  int4_countdown = init.Self().world().SharedRandom().Next(
      info_.int4_min_move_delay, info_.int4_max_move_delay);
  ptr_move_ = init.Self().Trait<sim::IMove>();
  if (auto* mobile = dynamic_cast<Mobile*>(ptr_move_))
    color_target_line_ = mobile->Info().color_target_line;
}

void Wanders::OnBecomingIdleInner(Actor& self) {
  int4_countdown = self.world().SharedRandom().Next(
      info_.int4_min_move_delay, info_.int4_max_move_delay);
}

void Wanders::OnBecomingIdle(Actor& self) {
  OnBecomingIdleInner(self);
}

void Wanders::TickIdle(Actor& self) {
  if (IsTraitDisabled())
    return;

  if (--int4_countdown > 0)
    return;

  const std::optional<CPos> target_cell = PickTargetLocation(self);
  if (target_cell.has_value())
    DoAction(self, *target_cell);
}

std::optional<CPos> Wanders::PickTargetLocation(Actor& self) {
  const WVec offset{0, -1024 * int4_effective_move_radius, 0};
  const WPos target =
      self.CenterPosition() +
      offset.Rotate(WRot::FromFacing(
          self.world().SharedRandom().Next(255)));
  const CPos target_cell = self.world().Map().CellContaining(target);

  if (!self.world().Map().Contains(target_cell)) {

    if (++int4_ticks_idle % info_.int4_reduce_move_radius_delay == 0)
      int4_effective_move_radius--;

    return std::nullopt;
  }

  if (!info_.vec_avoid_terrain_types.empty()) {
    const std::string_view terrain_type =
        self.world().Map().GetTerrainInfo(target_cell).Type;
    bool avoided = false;
    for (const auto& avoid : info_.vec_avoid_terrain_types)
      if (avoid == terrain_type) {
        avoided = true;
        break;
      }
    if (avoided)
      return std::nullopt;
  }

  int4_ticks_idle = 0;
  int4_effective_move_radius = info_.int4_wander_move_radius;

  return target_cell;
}

void Wanders::DoAction(Actor& self, const CPos& target_cell) {
  self.QueueActivity(ptr_move_->MoveTo(
      target_cell, -2, nullptr, true, color_target_line_));
}

void RegisterCommonTraitsBatch12() {
  auto& registry = sim::TraitRegistry::Instance();
  using sim::TraitBase;
  using ActorInitializer = sim::ActorInitializer;
  using ora::WorldArena;

  registry.Register(
      "WandersInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         WorldArena& arena) -> TraitBase* {
        WandersInfoData data = WandersInfoData::Parse(rec_info);
        return arena.Create<Wanders>(init, std::move(data));
      });
}

}
