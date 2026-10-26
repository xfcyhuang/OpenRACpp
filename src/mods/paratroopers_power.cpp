// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/ParatroopersPower.cs
//          @1f594e320
#include "mods/paratroopers_power.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/cargo.hpp"
#include "mods/concrete_support_powers.hpp"
#include "mods/passenger.hpp"
#include "mods/support_power_effects.hpp"
#include "net/text_notifications.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

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

std::optional<WVec> FieldWVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return WVec{static_cast<std::int32_t>(tuple->arr_ints[0]),
                static_cast<std::int32_t>(tuple->arr_ints[1]),
                static_cast<std::int32_t>(tuple->arr_ints[2])};
  return std::nullopt;
}

std::optional<WDist> FieldWDist(const meta::RecordObject& rec_info,
                                std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WDist{static_cast<std::int32_t>(*v)};
}

std::string ToLowerInvariant(std::string str_value) {
  for (char& c : str_value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return str_value;
}

const game::ActorInfo* FindActorInfo(const sim::World& world,
                                     std::string_view str_name) {
  for (const auto& [name, info] : world.Map().Rules().Actors())
    if (name == str_name)
      return info.get();
  return nullptr;
}

}  // namespace

ParatroopersPowerInfoData ParatroopersPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ParatroopersPowerInfoData data;
  static_cast<DirectionalSupportPowerInfoData&>(data) =
      DirectionalSupportPowerInfoData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "UnitType"))
    data.str_unit_type = *v;
  if (const auto v = FieldInt(rec_info, "SquadSize"))
    data.int4_squad_size = static_cast<int>(*v);
  if (const auto v = FieldWVec(rec_info, "SquadOffset"))
    data.wvec_squad_offset = *v;
  if (const auto v = FieldString(
          rec_info, "ReinforcementsArrivedSpeechNotification"))
    data.str_reinforcements_arrived_speech_notification = *v;
  if (const auto v = FieldString(
          rec_info, "ReinforcementsArrivedTextNotification"))
    data.str_reinforcements_arrived_text_notification = *v;
  if (const auto v = FieldInt(rec_info, "QuantizedFacings"))
    data.int4_quantized_facings = static_cast<int>(*v);
  if (const auto v = FieldWDist(rec_info, "Cordon"))
    data.dist_cordon = *v;
  data.vec_drop_items = FieldStringArray(rec_info, "DropItems");
  if (const auto v = FieldBool(rec_info, "AllowImpassableCells"))
    data.b_allow_impassable_cells = *v;
  if (const auto v = FieldString(rec_info, "CameraActor"))
    data.str_camera_actor = *v;
  if (const auto v = FieldInt(rec_info, "CameraRemoveDelay"))
    data.int4_camera_remove_delay = static_cast<int>(*v);
  if (const auto v = FieldWDist(rec_info, "BeaconDistanceOffset"))
    data.dist_beacon_distance_offset = *v;
  return data;
}

ParatroopersPower::ParatroopersPower(const ActorInitializer& init,
                                     const ParatroopersPowerInfoData& info)
    : DirectionalSupportPower(init, info),
      paratroopers_info_{info} {}

void ParatroopersPower::Activate(Actor& self, const net::Order& order,
                                 SupportPowerManager& manager) {
  DirectionalSupportPower::Activate(self, order, manager);

  const auto facing =
      paratroopers_info_.b_use_directional_target &&
              order.uint4_extra_data != 0xFFFFFFFFu
          ? std::optional<WAngle>{WAngle::FromFacing(
                static_cast<std::int32_t>(order.uint4_extra_data))}
          : std::nullopt;
  SendParatroopers(self, order.target.CenterPosition(), facing);
}

std::pair<std::vector<Actor*>, std::vector<Actor*>>
ParatroopersPower::SendParatroopers(Actor& self, WPos target,
                                    std::optional<WAngle> facing) {
  std::vector<Actor*> vec_aircraft;
  std::vector<Actor*> vec_units;

  if (!facing.has_value())
    facing = WAngle{
        1024 *
        self.world().SharedRandom().Next(
            paratroopers_info_.int4_quantized_facings) /
        paratroopers_info_.int4_quantized_facings};

  const std::string str_unit_type_lower =
      ToLowerInvariant(paratroopers_info_.str_unit_type);
  const game::ActorInfo* unit_type =
      FindActorInfo(self.world(), str_unit_type_lower);
  if (unit_type == nullptr)
    throw yaml::YamlException(
        "Actors ruleset does not include the entry '" +
        str_unit_type_lower + "'");

  const meta::RecordObject* rec_aircraft = sim::FindTraitInfoOfInterface(
      *unit_type, "OpenRA.Mods.Common.Traits.AircraftInfo");
  const auto altitude =
      rec_aircraft != nullptr
          ? FieldWDist(*rec_aircraft, "CruiseAltitude").value_or(WDist{1280})
                .Length
          : WDist{1280}.Length;
  const WRot drop_rotation = WRot::FromYaw(*facing);
  const WVec delta = WVec{0, -1024, 0}.Rotate(drop_rotation);
  target = target + WVec{0, 0, altitude};
  const WPos start_edge =
      target -
      (self.world().Map().DistanceToEdge(target, -delta) +
       paratroopers_info_.dist_cordon).Length * delta / 1024;
  const WPos finish_edge =
      target +
      (self.world().Map().DistanceToEdge(target, delta) +
       paratroopers_info_.dist_cordon).Length * delta / 1024;

  struct DropState {
    Actor* camera = nullptr;
    Beacon* beacon = nullptr;
    std::map<Actor*, bool> map_aircraft_in_range;
  };
  auto drop_state = std::make_shared<DropState>();

  auto on_enter_range =
      [this, drop_state, target, owner = self.Owner(),
       world_ptr = &self.world()](Actor& a) {
        if (!paratroopers_info_.str_camera_actor.empty() &&
            drop_state->camera == nullptr &&
            std::none_of(drop_state->map_aircraft_in_range.begin(),
                         drop_state->map_aircraft_in_range.end(),
                         [](const auto& kv) { return kv.second; })) {
          const auto target_pos = target;
          world_ptr->AddFrameEndTask(
              [world_ptr, drop_state,
               str_camera = paratroopers_info_.str_camera_actor, owner,
               target_pos](sim::World& w) {
                auto dict = sim::TypeDictionary{};
                dict.Add(std::make_unique<sim::LocationInit>(
                    w.Map().CellContaining(target_pos)));
                dict.Add(std::make_unique<sim::OwnerInit>(owner));
                drop_state->camera = w.CreateActor(true, str_camera, dict);
              });
        }

        RemoveBeacon(drop_state->beacon);

        if (std::none_of(drop_state->map_aircraft_in_range.begin(),
                         drop_state->map_aircraft_in_range.end(),
                         [](const auto& kv) { return kv.second; })) {
          net::TextNotificationsManager::Instance().AddTransientLine(
              owner,
              paratroopers_info_.str_reinforcements_arrived_text_notification);
        }

        drop_state->map_aircraft_in_range[&a] = true;
      };

  auto on_exit_range = [this, drop_state](Actor& a) {
    drop_state->map_aircraft_in_range[&a] = false;

    if (std::none_of(drop_state->map_aircraft_in_range.begin(),
                     drop_state->map_aircraft_in_range.end(),
                     [](const auto& kv) { return kv.second; }))
      RemoveCamera(drop_state->camera);
  };

  auto on_removed_from_world = [this, drop_state](Actor& a) {
    drop_state->map_aircraft_in_range[&a] = false;

    if (std::all_of(drop_state->map_aircraft_in_range.begin(),
                    drop_state->map_aircraft_in_range.end(),
                    [](const auto& kv) { return !kv.first->IsInWorld(); })) {
      RemoveCamera(drop_state->camera);
      RemoveBeacon(drop_state->beacon);
    }
  };

  for (auto i = -paratroopers_info_.int4_squad_size / 2;
       i <= paratroopers_info_.int4_squad_size / 2; i++) {
    if (i == 0 && (paratroopers_info_.int4_squad_size & 1) == 0)
      continue;

    const auto& so = paratroopers_info_.wvec_squad_offset;
    const WVec spawn_offset =
        WVec{i * so.Y, -std::abs(i) * so.X, 0}.Rotate(drop_rotation);

    auto dict = sim::TypeDictionary{};
    dict.Add(std::make_unique<sim::CenterPositionInit>(start_edge + spawn_offset));
    dict.Add(std::make_unique<sim::OwnerInit>(self.Owner()));
    dict.Add(std::make_unique<sim::FacingInit>(*facing));
    vec_aircraft.push_back(self.world().CreateActor(
        false, str_unit_type_lower, dict));
  }

  for (const auto& p : paratroopers_info_.vec_drop_items) {
    auto dict = sim::TypeDictionary{};
    dict.Add(std::make_unique<sim::OwnerInit>(self.Owner()));
    vec_units.push_back(
        self.world().CreateActor(false, ToLowerInvariant(p), dict));
  }

  const auto start_edge_cap = start_edge;
  const auto finish_edge_cap = finish_edge;
  self.world().AddFrameEndTask(
      [this, &world = self.world(), vec_aircraft, vec_units, start_edge_cap,
       finish_edge_cap, target, facing_cap = *facing, drop_state,
       drop_rotation, altitude, on_enter_range, on_exit_range,
       on_removed_from_world](sim::World& w) {
        PlayLaunchSounds();

        const int passengers_per_plane =
            (static_cast<int>(paratroopers_info_.vec_drop_items.size()) +
             paratroopers_info_.int4_squad_size - 1) /
            paratroopers_info_.int4_squad_size;
        std::size_t added = 0;
        std::size_t j = 0;
        Actor* distance_test_actor = nullptr;
        for (auto i = -paratroopers_info_.int4_squad_size / 2;
             i <= paratroopers_info_.int4_squad_size / 2; i++) {
          if (i == 0 && (paratroopers_info_.int4_squad_size & 1) == 0)
            continue;

          const auto& so = paratroopers_info_.wvec_squad_offset;
          const WVec spawn_offset =
              WVec{i * so.Y, -std::abs(i) * so.X, 0}.Rotate(drop_rotation);
          const WVec target_offset =
              WVec{i * so.Y, 0, 0}.Rotate(drop_rotation);
          auto* a = vec_aircraft[j++];
          w.Add(a);

          auto* drop = a->Trait<ParaDrop>();
          drop->SetLZ(w.Map().CellContaining(target + target_offset),
                      !paratroopers_info_.b_allow_impassable_cells);
          drop->AddOnEnteredDropRange(on_enter_range);
          drop->AddOnExitedDropRange(on_exit_range);
          drop->AddOnRemovedFromWorld(on_removed_from_world);

          auto* cargo = a->Trait<Cargo>();
          for (std::size_t k = added;
               k < added + static_cast<std::size_t>(passengers_per_plane) &&
               k < vec_units.size();
               k++) {
            cargo->Load(*a, *vec_units[k]);
            added++;
          }

          a->QueueActivity(w.Arena().Create<activities::Fly>(
              *a, sim::Target::FromPos(target + spawn_offset)));
          a->QueueActivity(w.Arena().Create<activities::Fly>(
              *a, sim::Target::FromPos(finish_edge_cap + spawn_offset)));
          a->QueueActivity(w.Arena().Create<RemoveSelf>());
          distance_test_actor = a;
        }

        for (auto k = added; k < vec_units.size(); k++)
          vec_units[k]->Dispose();

        if (Info().b_display_beacon) {
          const auto distance = (target - start_edge_cap).HorizontalLength();

          auto new_beacon = std::make_unique<Beacon>(
              *Self()->Owner(), target - WVec{0, 0, altitude},
              Info().b_beacon_palette_is_player_palette,
              Info().str_beacon_palette, Info().str_beacon_image,
              Info().str_beacon_poster, Info().str_beacon_poster_palette,
              Info().str_beacon_sequence, Info().str_arrow_sequence,
              Info().str_circle_sequence, Info().str_clock_sequence,
              [distance_test_actor, target, distance,
               offset_len =
                   paratroopers_info_.dist_beacon_distance_offset
                       .Length]() {
                return 1.0f -
                       static_cast<float>(
                           (distance_test_actor->CenterPosition() - target)
                               .HorizontalLength() -
                           offset_len) /
                           static_cast<float>(distance);
              },
              Info().int4_beacon_delay);
          drop_state->beacon = new_beacon.get();

          w.Add(std::move(new_beacon));
        }
      });

  return {vec_aircraft, vec_units};
}

void ParatroopersPower::RemoveCamera(Actor* camera) {
  if (camera == nullptr)
    return;

  camera->QueueActivity(
      Self()->world().Arena().Create<activities::Wait>(
          paratroopers_info_.int4_camera_remove_delay));
  camera->QueueActivity(
      Self()->world().Arena().Create<RemoveSelf>());
}

void ParatroopersPower::RemoveBeacon(Beacon* beacon) {
  if (beacon == nullptr)
    return;

  Beacon* beacon_cap = beacon;
  Self()->world().AddFrameEndTask(
      [beacon_cap](sim::World& w) { w.Remove(beacon_cap); });
}

}  // namespace ora::mods
