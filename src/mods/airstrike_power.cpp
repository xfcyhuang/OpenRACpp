// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/DirectionalSupportPower.cs
//          + AirstrikePower.cs + SelectDirectionalTarget.cs @e180e3225
//          的实现面
#include "mods/airstrike_power.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/sequence_set.hpp"
#include "map/map.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/attack_aircraft.hpp"
#include "mods/concrete_support_powers.hpp"
#include "mods/support_power_effects.hpp"
#include "sim/actor_init.hpp"
#include "sim/order_generator.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/mouse_attachment_widget.hpp"

namespace ora::mods {

namespace {

constexpr double kPi = 3.14159265358979323846;

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

const game::ActorInfo* FindActorInfo(const sim::World& world,
                                     std::string_view str_name) {
  for (const auto& [name, info] : world.Map().Rules().Actors())
    if (name == str_name)
      return info.get();
  return nullptr;
}

}  // namespace

DirectionalSupportPowerInfoData DirectionalSupportPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  DirectionalSupportPowerInfoData data;
  static_cast<SupportPowerInfoData&>(data) =
      SupportPowerInfoData::Parse(rec_info);
  if (const auto v = FieldBool(rec_info, "UseDirectionalTarget"))
    data.b_use_directional_target = *v;
  data.vec_arrows = FieldStringArray(rec_info, "Arrows");
  if (data.vec_arrows.empty())
    data.vec_arrows = {"arrow-t",  "arrow-tl", "arrow-l", "arrow-bl",
                       "arrow-b",  "arrow-br", "arrow-r", "arrow-tr"};
  if (const auto v = FieldString(rec_info, "DirectionArrowAnimation"))
    data.str_direction_arrow_animation = *v;
  if (const auto v = FieldString(rec_info, "DirectionArrowPalette"))
    data.str_direction_arrow_palette = *v;
  return data;
}

DirectionalSupportPower::DirectionalSupportPower(
    const ActorInitializer& init, const DirectionalSupportPowerInfoData& info)
    : SupportPower(init, info),
      directional_info_{info} {}

void DirectionalSupportPower::SelectTarget(Actor& self,
                                           const std::string& str_order,
                                           SupportPowerManager& manager) {
  if (directional_info_.b_use_directional_target)
    self.world().SetOrderGenerator(
        self.world().Arena().Create<SelectDirectionalTarget>(
            self.world(), str_order, manager, directional_info_));
  else
    SupportPower::SelectTarget(self, str_order, manager);
}

AirstrikePowerInfoData AirstrikePowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AirstrikePowerInfoData data;
  static_cast<DirectionalSupportPowerInfoData&>(data) =
      DirectionalSupportPowerInfoData::Parse(rec_info);
  if (const auto v = FieldString(rec_info, "UnitType"))
    data.str_unit_type = *v;
  if (const auto v = FieldInt(rec_info, "SquadSize"))
    data.int4_squad_size = static_cast<int>(*v);
  if (const auto v = FieldWVec(rec_info, "SquadOffset"))
    data.wvec_squad_offset = *v;
  if (const auto v = FieldInt(rec_info, "QuantizedFacings"))
    data.int4_quantized_facings = static_cast<int>(*v);
  if (const auto v = FieldWDist(rec_info, "Cordon"))
    data.dist_cordon = *v;
  if (const auto v = FieldString(rec_info, "CameraActor"))
    data.str_camera_actor = *v;
  if (const auto v = FieldInt(rec_info, "CameraRemoveDelay"))
    data.int4_camera_remove_delay = static_cast<int>(*v);
  if (const auto v = FieldWDist(rec_info, "BeaconDistanceOffset"))
    data.dist_beacon_distance_offset = *v;
  return data;
}

AirstrikePower::AirstrikePower(const ActorInitializer& init,
                               const AirstrikePowerInfoData& info)
    : DirectionalSupportPower(init, info),
      airstrike_info_{info} {}

void AirstrikePower::Activate(Actor& self, const net::Order& order,
                              SupportPowerManager& manager) {
  DirectionalSupportPower::Activate(self, order, manager);

  const auto facing =
      airstrike_info_.b_use_directional_target &&
              order.uint4_extra_data != 0xFFFFFFFFu
          ? std::optional<WAngle>{WAngle::FromFacing(
                static_cast<std::int32_t>(order.uint4_extra_data))}
          : std::nullopt;
  SendAirstrike(self, order.target.CenterPosition(), facing);
}

std::vector<Actor*> AirstrikePower::SendAirstrike(Actor& self, WPos target,
                                                  std::optional<WAngle> facing) {
  std::vector<Actor*> vec_aircraft;
  if (!facing.has_value())
    facing = WAngle{1024 *
                    self.world().SharedRandom().Next(
                        airstrike_info_.int4_quantized_facings) /
                    airstrike_info_.int4_quantized_facings};

  const auto* unit_info =
      FindActorInfo(self.world(), airstrike_info_.str_unit_type);
  const auto* aircraft_rec = unit_info != nullptr
                                 ? sim::FindTraitInfoOfInterface(
                                       *unit_info,
                                       "OpenRA.Mods.Common.Traits.AircraftInfo")
                                 : nullptr;
  const auto altitude =
      aircraft_rec != nullptr
          ? FieldWDist(*aircraft_rec, "CruiseAltitude").value_or(WDist{1280})
                 .Length
          : WDist{1280}.Length;
  const auto attack_rotation = WRot::FromYaw(*facing);
  const auto delta = WVec{0, -1024, 0}.Rotate(attack_rotation);
  target = target + WVec{0, 0, altitude};
  const auto start_edge =
      target -
      (self.world().Map().DistanceToEdge(target, -delta) +
       airstrike_info_.dist_cordon).Length * delta / 1024;
  const auto finish_edge =
      target +
      (self.world().Map().DistanceToEdge(target, delta) +
       airstrike_info_.dist_cordon).Length * delta / 1024;

  struct StrikeState {
    Actor* camera = nullptr;
    Beacon* beacon = nullptr;
    std::map<Actor*, bool> map_aircraft_in_range;
  };
  auto strike_state = std::make_shared<StrikeState>();

  auto on_enter_range =
      [this, strike_state, target, owner = self.Owner(),
       world_ptr = &self.world()](Actor& a) {
        if (!airstrike_info_.str_camera_actor.empty() &&
            strike_state->camera == nullptr &&
            std::none_of(strike_state->map_aircraft_in_range.begin(),
                         strike_state->map_aircraft_in_range.end(),
                         [](const auto& kv) { return kv.second; })) {
          const auto target_pos = target;
          world_ptr->AddFrameEndTask(
              [world_ptr, strike_state,
               str_camera = airstrike_info_.str_camera_actor, owner,
               target_pos](sim::World& w) {
                auto dict = sim::TypeDictionary{};
                dict.Add(std::make_unique<sim::LocationInit>(
                    w.Map().CellContaining(target_pos)));
                dict.Add(std::make_unique<sim::OwnerInit>(owner));
                strike_state->camera = w.CreateActor(true, str_camera, dict);
              });
        }

        RemoveBeacon(strike_state->beacon);

        strike_state->map_aircraft_in_range[&a] = true;
      };

  auto on_exit_range = [this, strike_state](Actor& a) {
    strike_state->map_aircraft_in_range[&a] = false;

    if (std::none_of(strike_state->map_aircraft_in_range.begin(),
                     strike_state->map_aircraft_in_range.end(),
                     [](const auto& kv) { return kv.second; }))
      RemoveCamera(strike_state->camera);
  };

  auto on_removed_from_world = [this, strike_state](Actor& a) {
    strike_state->map_aircraft_in_range[&a] = false;

    if (std::all_of(strike_state->map_aircraft_in_range.begin(),
                    strike_state->map_aircraft_in_range.end(),
                    [](const auto& kv) { return !kv.first->IsInWorld(); })) {
      RemoveCamera(strike_state->camera);
      RemoveBeacon(strike_state->beacon);
    }
  };

  for (auto i = -airstrike_info_.int4_squad_size / 2;
       i <= airstrike_info_.int4_squad_size / 2; i++) {
    if (i == 0 && (airstrike_info_.int4_squad_size & 1) == 0)
      continue;

    const auto& so = airstrike_info_.wvec_squad_offset;
    const auto spawn_offset =
        WVec{i * so.Y, -std::abs(i) * so.X, 0}.Rotate(attack_rotation);
    const auto target_offset = WVec{i * so.Y, 0, 0}.Rotate(attack_rotation);

    auto dict = sim::TypeDictionary{};
    dict.Add(std::make_unique<sim::CenterPositionInit>(start_edge + spawn_offset));
    dict.Add(std::make_unique<sim::OwnerInit>(self.Owner()));
    dict.Add(std::make_unique<sim::FacingInit>(*facing));
    auto* a = self.world().CreateActor(false, airstrike_info_.str_unit_type,
                                       dict);

    vec_aircraft.push_back(a);
    strike_state->map_aircraft_in_range[a] = false;

    auto* attack = a->Trait<AttackBomber>();
    attack->SetTarget(target + target_offset);
    attack->AddOnEnteredAttackRange(on_enter_range);
    attack->AddOnExitedAttackRange(on_exit_range);
    attack->AddOnRemovedFromWorld(on_removed_from_world);
  }

  const auto start_edge_cap = start_edge;
  const auto finish_edge_cap = finish_edge;
  self.world().AddFrameEndTask(
      [this, &world = self.world(), vec_aircraft, start_edge_cap,
       finish_edge_cap, facing_cap = *facing,
       strike_state,
       attack_rotation,
       altitude,
       on_enter_range, on_exit_range,
       on_removed_from_world,
       target](sim::World& w) {
        PlayLaunchSounds();

        auto j = 0;
        Actor* distance_test_actor = nullptr;
        for (auto i = -airstrike_info_.int4_squad_size / 2;
             i <= airstrike_info_.int4_squad_size / 2; i++) {
          if (i == 0 && (airstrike_info_.int4_squad_size & 1) == 0)
            continue;

          const auto& so = airstrike_info_.wvec_squad_offset;
          const auto spawn_offset =
              WVec{i * so.Y, -std::abs(i) * so.X, 0}.Rotate(attack_rotation);

          auto* a = vec_aircraft[static_cast<std::size_t>(j++)];
          w.Add(a);

          a->QueueActivity(w.Arena().Create<activities::Fly>(
              *a, sim::Target::FromPos(target + spawn_offset)));
          a->QueueActivity(w.Arena().Create<activities::Fly>(
              *a, sim::Target::FromPos(finish_edge_cap + spawn_offset)));
          a->QueueActivity(w.Arena().Create<RemoveSelf>());
          distance_test_actor = a;
        }

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
               offset_len = airstrike_info_.dist_beacon_distance_offset
                                .Length]() {
                return 1.0f -
                       static_cast<float>(
                           (distance_test_actor->CenterPosition() - target)
                               .HorizontalLength() -
                           offset_len) /
                           static_cast<float>(distance);
              },
              Info().int4_beacon_delay);
          strike_state->beacon = new_beacon.get();

          w.Add(std::move(new_beacon));
        }
      });

  return vec_aircraft;
}

void AirstrikePower::RemoveCamera(Actor* camera) {
  if (camera == nullptr)
    return;

  camera->QueueActivity(
      Self()->world().Arena().Create<activities::Wait>(
          airstrike_info_.int4_camera_remove_delay));
  camera->QueueActivity(
      Self()->world().Arena().Create<RemoveSelf>());
}

void AirstrikePower::RemoveBeacon(Beacon* beacon) {
  if (beacon == nullptr)
    return;

  Beacon* beacon_cap = beacon;
  Self()->world().AddFrameEndTask(
      [beacon_cap](sim::World& w) { w.Remove(beacon_cap); });
}

SelectDirectionalTarget::SelectDirectionalTarget(
    sim::World& world, std::string str_order, SupportPowerManager& manager,
    const DirectionalSupportPowerInfoData& info)
    : str_order_{std::move(str_order)},
      manager_{manager},
      info_{info} {
  vec_direction_arrows_ =
      LoadArrows(info.str_direction_arrow_animation, world,
                 static_cast<int>(info.vec_arrows.size()));
}

std::vector<std::unique_ptr<net::Order>> SelectDirectionalTarget::Order(
    sim::World& world, CPos cell, int2, const MouseInput& mi_input) {
  std::vector<std::unique_ptr<net::Order>> vec_orders;
  if (mi_input.Button == CancelButton()) {
    world.CancelInputMode();
    return vec_orders;
  }

  if (mi_input.Button == ActionButton() &&
      mi_input.Event == MouseInputEvent::Down) {
    if (!b_activated_ && world.Map().Contains(cell)) {
      cell_target_ = cell;
      int2_target_location_ = mi_input.Location;
      b_activated_ = true;
      ui::Ui::Instance().LockCursor();
    }

    return vec_orders;
  }

  if (!b_activated_)
    return vec_orders;

  if (mi_input.Event == MouseInputEvent::Move) {
    vec2_drag_direction_.X += static_cast<float>(mi_input.Delta.X);
    vec2_drag_direction_.Y += static_cast<float>(mi_input.Delta.Y);

    const auto angle = AngleOf(vec2_drag_direction_);
    if (std::sqrt(vec2_drag_direction_.X * vec2_drag_direction_.X +
                  vec2_drag_direction_.Y * vec2_drag_direction_.Y) >
        75.0f) {
      const auto unit =
          gfx::FromAngle(static_cast<float>(angle * (kPi / 180.0)));
      vec2_drag_direction_ = core::Vector2{-75.0f * unit.X,
                                           -75.0f * unit.Y};
    }

    arrow_current_ = GetArrow(angle);

    if (auto* mouse_attachment =
            ui::Ui::Instance().Root()
                ? ui::Ui::Instance()
                      .Root()
                      ->GetOrNull<ui::MouseAttachmentWidget>(
                          "MOUSE_ATTATCHMENT")
                : nullptr)
      mouse_attachment->SetAttachment(
          int2_target_location_, arrow_current_.sprite,
          info_.str_direction_arrow_palette);
    b_drag_started_ = true;
  }

  if (mi_input.Button == ActionButton() &&
      mi_input.Event == MouseInputEvent::Up) {
    auto up_order = std::make_unique<net::Order>(
        str_order_, manager_.Self(),
        sim::Target::FromCell(manager_.Self()->world(), cell_target_), false);
    up_order->b_suppress_visual_feedback = true;
    up_order->uint4_extra_data =
        IsOutsideDragZone()
            ? static_cast<std::uint32_t>(
                  arrow_current_.angle_direction.Facing())
            : 0xFFFFFFFFu;
    vec_orders.push_back(std::move(up_order));

    world.CancelInputMode();
  }

  return vec_orders;
}

void SelectDirectionalTarget::Tick(sim::World& world) {
  const auto* p = manager_.FindPower(str_order_);
  if (p == nullptr || !p->Active() || !p->Ready())
    world.CancelInputMode();
}

std::string SelectDirectionalTarget::GetCursor(sim::World& world, CPos cell,
                                               int2, const MouseInput&) {
  return world.Map().Contains(cell) ? info_.str_cursor
                                    : info_.str_blocked_cursor;
}

void SelectDirectionalTarget::Deactivate() {
  if (b_activated_) {
    if (auto* mouse_attachment =
            ui::Ui::Instance().Root()
                ? ui::Ui::Instance()
                      .Root()
                      ->GetOrNull<ui::MouseAttachmentWidget>(
                          "MOUSE_ATTATCHMENT")
                : nullptr)
      mouse_attachment->Reset();
    ui::Ui::Instance().UnlockCursor();
  }
}

double SelectDirectionalTarget::AngleOf(core::Vector2 delta) {
  const auto radian = std::atan2(static_cast<double>(delta.Y),
                                 static_cast<double>(delta.X));
  auto d = radian * (180.0 / kPi);
  if (d < 0.0)
    d += 360.0;
  auto angle = 270.0 - d;
  if (angle < 0.0)
    angle += 360.0;

  return angle;
}

SelectDirectionalTarget::Arrow SelectDirectionalTarget::GetArrow(
    double degree) {
  for (const auto& arrow : vec_direction_arrows_)
    if (arrow.fp8_end_angle >= degree)
      return arrow;
  return vec_direction_arrows_.front();
}

std::vector<SelectDirectionalTarget::Arrow>
SelectDirectionalTarget::LoadArrows(const std::string& str_cursor_animation,
                                    sim::World& world,
                                    int n_no_of_dividing_points) {
  std::vector<Arrow> vec_points(static_cast<std::size_t>(n_no_of_dividing_points));
  const auto part_angle = 360 / n_no_of_dividing_points;
  const auto i1 = part_angle / 2.0;

  for (auto i = 0; i < n_no_of_dividing_points; i++) {
    const auto sprite =
        world.Map().Sequences()->GetSequence(
            str_cursor_animation,
            info_.vec_arrows[static_cast<std::size_t>(i)])
            .GetSprite(0);

    const auto angle = i * part_angle;
    const auto direction = WAngle::FromDegrees(angle);
    const auto end_angle = angle + i1;

    vec_points[static_cast<std::size_t>(i)] =
        Arrow{sprite, end_angle, direction};
  }

  return vec_points;
}

bool SelectDirectionalTarget::IsOutsideDragZone() const {
  return b_drag_started_ &&
         std::sqrt(vec2_drag_direction_.X * vec2_drag_direction_.X +
                   vec2_drag_direction_.Y * vec2_drag_direction_.Y) > 20.0;
}

}  // namespace ora::mods
