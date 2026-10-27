#include "mods/parachutable.hpp"

#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "mods/sprite_effect.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
}

std::optional<bool> FieldBool(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

std::vector<std::string> FieldStringArray(const meta::RecordObject& rec_info,
                                          std::string_view str_name) {
  std::vector<std::string> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* list =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *list)
      if (const auto* s = std::get_if<std::string>(&element.val))
        vec_out.push_back(*s);
  return vec_out;
}

}

ParachutableInfoData ParachutableInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ParachutableInfoData data;
  if (const auto v = FieldBool(rec_info, "KilledOnImpassableTerrain"))
    data.b_killed_on_impassable_terrain = *v;
  if (const auto vec_names = FieldStringArray(rec_info, "DamageTypes");
      !vec_names.empty())
    data.bitset_damage_types =
        core::BitSet<sim::DamageType>(vec_names);
  if (const auto v = FieldString(rec_info, "Image"))
    data.str_image = *v;
  if (const auto v = FieldString(rec_info, "GroundCorpseSequence"))
    data.str_ground_corpse_sequence = *v;
  if (const auto v = FieldString(rec_info, "GroundCorpsePalette"))
    data.str_ground_corpse_palette = *v;
  if (const auto v = FieldString(rec_info, "GroundImpactSound"))
    data.str_ground_impact_sound = *v;
  if (const auto v = FieldString(rec_info, "WaterCorpseSequence"))
    data.str_water_corpse_sequence = *v;
  if (const auto v = FieldString(rec_info, "WaterCorpsePalette"))
    data.str_water_corpse_palette = *v;
  if (const auto v = FieldStringArray(rec_info, "WaterTerrainTypes");
      !v.empty())
    data.vec_water_terrain_types = v;
  if (const auto v = FieldString(rec_info, "WaterImpactSound"))
    data.str_water_impact_sound = *v;
  if (const auto v = FieldInt(rec_info, "FallRate"))
    data.int4_fall_rate = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "ParachutingCondition"))
    data.str_parachuting_condition = *v;
  return data;
}

Parachutable::Parachutable(sim::Actor& self, const ParachutableInfoData& info)
    : info_{info},
      ptr_positionable_{self.Trait<sim::IPositionable>()},
      self_{self} {}

void Parachutable::OnParachute(sim::Actor& self) {
  b_is_in_air_ = true;

  if (int4_parachuting_token == sim::Actor::InvalidConditionToken)
    int4_parachuting_token =
        self.GrantCondition(info_.str_parachuting_condition);

  NotifyBlocker(self, self.Location());
}

void Parachutable::OnLanded(sim::Actor& self) {
  b_is_in_air_ = false;

  if (int4_parachuting_token != sim::Actor::InvalidConditionToken)
    int4_parachuting_token =
        self.RevokeCondition(int4_parachuting_token);

  if (!info_.b_killed_on_impassable_terrain)
    return;

  const CPos cell = self.Location();
  if (ptr_positionable_->CanEnterCell(cell, &self_, sim::BlockedByActor::All))
    return;

  if (IgnoreActor != nullptr) {
    bool only_ignored = true;
    for (auto* a : self.world().ActorMapFace()->GetActorsAt(cell))
      if (a != IgnoreActor && a != &self &&
          self.world().Map().DistanceAboveTerrain(a->CenterPosition()) ==
              WDist{0}) {
        only_ignored = false;
        break;
      }
    if (only_ignored)
      return;
  }

  const bool on_water =
      std::find(info_.vec_water_terrain_types.begin(),
                info_.vec_water_terrain_types.end(),
                self.world().Map().GetTerrainInfo(cell).Type) !=
      info_.vec_water_terrain_types.end();

  const std::string& str_sequence =
      on_water ? info_.str_water_corpse_sequence
               : info_.str_ground_corpse_sequence;
  const std::string& str_palette = on_water ? info_.str_water_corpse_palette
                                            : info_.str_ground_corpse_palette;
  if (!info_.str_image.empty() && !str_sequence.empty() &&
      !str_palette.empty()) {
    const std::string str_image = info_.str_image;
    self.world().AddFrameEndTask(
        [pos = self.OccupiesSpace()->CenterPosition(), str_image,
         str_sequence, str_palette](sim::World& w) {
          w.Add(std::make_unique<SpriteEffect>(pos, w, str_image,
                                               str_sequence, str_palette));
        });
  }

  self.Kill(&self, info_.bitset_damage_types);
}

}
