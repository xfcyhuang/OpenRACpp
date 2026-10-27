#include "mods/smudge_layer.hpp"

#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

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

}

SmudgeLayerInfoData SmudgeLayerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  SmudgeLayerInfoData data;
  if (const auto v = RecString(rec_info, "Type"))
    data.str_type = *v;
  if (const auto v = RecString(rec_info, "Sequence"))
    data.str_sequence = *v;
  if (const auto v = RecInt(rec_info, "SmokeChance"))
    data.int4_smoke_chance = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "MaxSmokeOffsetDistance"))
    data.max_smoke_offset_distance =
        WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecString(rec_info, "SmokeImage"))
    data.str_smoke_image = *v;
  if (const auto v = RecString(rec_info, "SmokePalette"))
    data.str_smoke_palette = *v;
  if (const auto v = RecString(rec_info, "Palette"))
    data.str_palette = *v;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "InitialSmudges") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* dict = std::get_if<meta::GenericDict>(&v.val))
          for (const auto& [key, value] : *dict) {
            const auto* key_tuple =
                std::get_if<meta::GenericTuple>(&key.val);
            auto* value_dict = std::get_if<meta::GenericDict>(&value.val);
            if (key_tuple == nullptr || value_dict == nullptr)
              continue;
            MapSmudge smudge;
            for (const auto& [field_key, field_value] : *value_dict) {
              const auto* field_name =
                  std::get_if<std::string>(&field_key.val);
              if (field_name == nullptr)
                continue;
              if (*field_name == "Type")
                if (auto* s = std::get_if<std::string>(&field_value.val))
                  smudge.str_type = *s;
              if (*field_name == "Depth")
                if (auto* n =
                        std::get_if<std::int64_t>(&field_value.val))
                  smudge.int4_depth = static_cast<int>(*n);
            }
            data.vec_initial_smudges.emplace_back(
                CPos{static_cast<std::int32_t>((*key_tuple).arr_ints[0]),
                     static_cast<std::int32_t>((*key_tuple).arr_ints[1]),
                     static_cast<std::uint8_t>((*key_tuple).arr_ints[2])},
                std::move(smudge));
          }
      }
  }
  return data;
}

SmudgeLayer::SmudgeLayer(sim::Actor& self, SmudgeLayerInfoData info)
    : info_{std::move(info)}, world_{self.world()} {

  for (const auto& [cell, smudge] : info_.vec_initial_smudges) {

    if (smudge.str_type != info_.str_type)
      continue;
    map_tiles_[cell] = Smudge{smudge.str_type, smudge.int4_depth, false};
  }
}

void SmudgeLayer::AddSmudge(CPos loc) {

  if (!world_.Map().Contains(loc))
    return;

  const auto it_dirty = map_dirty_.find(loc);
  const bool b_dirty_deleted_or_absent =
      it_dirty == map_dirty_.end() || it_dirty->second.b_deleted;
  if (b_dirty_deleted_or_absent && map_tiles_.find(loc) == map_tiles_.end()) {

    map_dirty_[loc] = Smudge{info_.str_type, 0, false};
  } else {

    Smudge tile = (it_dirty != map_dirty_.end() && !it_dirty->second.b_deleted)
                      ? it_dirty->second
                      : map_tiles_[loc];
    map_dirty_[loc] = tile;
  }
}

void SmudgeLayer::RemoveSmudge(CPos loc) {

  if (!world_.Map().Contains(loc))
    return;

  Smudge tile{};
  if (const auto it_dirty = map_dirty_.find(loc);
      it_dirty != map_dirty_.end())
    tile = it_dirty->second;

  tile.b_deleted = true;
  map_dirty_[loc] = tile;
}

void SmudgeLayer::Disposing(sim::Actor&) {

  if (b_disposed_)
    return;

  b_disposed_ = true;
}

}
