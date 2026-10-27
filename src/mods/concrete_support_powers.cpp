#include "mods/concrete_support_powers.hpp"

#include "core/cvec.hpp"
#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "mods/body_orientation.hpp"
#include "mods/external_condition.hpp"
#include "mods/move_activities.hpp"
#include "mods/primary_building.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/selection_decorations.hpp"
#include "mods/sprite_effect.hpp"
#include "mods/with_sprite_body.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/target.hpp"
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

std::vector<std::int64_t> FieldIntArray(const meta::RecordObject& rec_info,
                                        std::string_view str_name) {
  std::vector<std::int64_t> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* list =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *list)
      if (const auto* n = std::get_if<std::int64_t>(&element.val))
        vec_out.push_back(*n);
  return vec_out;
}

std::optional<WVec> FieldWVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return WVec{
        static_cast<std::int32_t>((*tuple).arr_ints[0]),
        static_cast<std::int32_t>((*tuple).arr_ints[1]),
        static_cast<std::int32_t>((*tuple).arr_ints[2])};
  return std::nullopt;
}

std::optional<CVec> FieldCVec(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
    return CVec{static_cast<int>((*tuple).arr_ints[0]),
                static_cast<int>((*tuple).arr_ints[1])};
  return std::nullopt;
}

std::optional<core::Color> FieldColor(const meta::RecordObject& rec_info,
                                      std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* n = std::get_if<std::int64_t>(&gv->val))
    return core::Color::FromArgbRaw(static_cast<std::uint32_t>(*n));
  return std::nullopt;
}

std::optional<sim::PlayerRelationship> FieldRelationship(
    const meta::RecordObject& rec_info, std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* n = std::get_if<std::int64_t>(&gv->val))
    return static_cast<sim::PlayerRelationship>(*n);
  return std::nullopt;
}

std::string ToLower(std::string_view str_text) {
  std::string str_out{str_text};
  std::transform(str_out.begin(), str_out.end(), str_out.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return str_out;
}

const game::ActorInfo* FindActorInfo(const sim::World& world,
                                     std::string_view str_name) {
  for (const auto& [name, info] : world.Map().Rules().Actors())
    if (name == str_name)
      return info.get();
  return nullptr;
}

SupportPowerSoundPlayer& FnSupportPowerSoundPlayer() {
  static SupportPowerSoundPlayer fn_play;
  return fn_play;
}

}

void SetSupportPowerSoundPlayer(SupportPowerSoundPlayer fn_play) {
  FnSupportPowerSoundPlayer() = std::move(fn_play);
}

void PlaySupportPowerSoundIfWired(const std::string& str_name,
                                  sim::World& world, const WPos& wpos_pos) {
  if (FnSupportPowerSoundPlayer() == nullptr)
    return;
  FnSupportPowerSoundPlayer()(str_name, world, wpos_pos);
}

bool RemoveSelf::Tick(sim::Actor& self) {
  if (IsCanceling())
    return true;
  self.Dispose();
  Cancel(self);
  return true;
}

NukePowerInfoData NukePowerInfoData::Parse(const meta::RecordObject& rec_info,
                                           const sim::World& world) {
  NukePowerInfoData data;
  static_cast<SupportPowerInfoData&>(data) =
      SupportPowerInfoData::Parse(rec_info);

  if (const auto v = FieldString(rec_info, "MissileWeapon"))
    data.str_missile_weapon = *v;
  if (const auto v = FieldInt(rec_info, "MissileDelay"))
    data.int4_missile_delay = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "MissileImage"))
    data.str_missile_image = *v;
  if (const auto v = FieldString(rec_info, "MissileUp"))
    data.str_missile_up = *v;
  if (const auto v = FieldString(rec_info, "MissileDown"))
    data.str_missile_down = *v;
  if (const auto v = FieldWVec(rec_info, "SpawnOffset"))
    data.wvec_spawn_offset = *v;
  if (const auto v = FieldInt(rec_info, "DetonationAltitude"))
    data.dist_detonation_altitude = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = FieldBool(rec_info, "RemoveMissileOnDetonation"))
    data.b_remove_missile_on_detonation = *v;
  if (const auto v = FieldString(rec_info, "MissilePalette"))
    data.str_missile_palette = *v;
  if (const auto v = FieldBool(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v;
  if (const auto v = FieldString(rec_info, "TrailImage"))
    data.str_trail_image = *v;
  data.vec_trail_sequences = FieldStringArray(rec_info, "TrailSequences");
  if (const auto v = FieldInt(rec_info, "TrailInterval"))
    data.int4_trail_interval = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "TrailDelay"))
    data.int4_trail_delay = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "TrailPalette"))
    data.str_trail_palette = *v;
  if (const auto v = FieldBool(rec_info, "TrailUsePlayerPalette"))
    data.b_trail_use_player_palette = *v;
  if (const auto v = FieldInt(rec_info, "FlightDelay"))
    data.int4_flight_delay = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "FlightVelocity"))
    data.dist_flight_velocity = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = FieldBool(rec_info, "SkipAscent"))
    data.b_skip_ascent = *v;
  if (const auto v = FieldInt(rec_info, "BeaconRemoveAdvance"))
    data.int4_beacon_remove_advance = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "CameraRange"))
    data.dist_camera_range = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = FieldBool(rec_info, "RevealGeneratedShroud"))
    data.b_reveal_generated_shroud = *v;
  if (const auto v = FieldRelationship(rec_info, "CameraRelationships"))
    data.kind_camera_relationships = *v;
  if (const auto v = FieldInt(rec_info, "CameraSpawnAdvance"))
    data.int4_camera_spawn_advance = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "CameraRemoveDelay"))
    data.int4_camera_remove_delay = static_cast<int>(*v);
  if (const auto v = FieldColor(rec_info, "CircleColor"))
    data.color_circle = *v;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "CircleWidth"))
    if (const auto* f = std::get_if<float>(&gv->val))
      data.fp4_circle_width = *f;
  if (const auto v = FieldColor(rec_info, "CircleBorderColor"))
    data.color_circle_border = *v;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "CircleBorderWidth"))
    if (const auto* f = std::get_if<float>(&gv->val))
      data.fp4_circle_border_width = *f;
  {
    const std::vector<std::int64_t> vec_ranges =
        FieldIntArray(rec_info, "CircleRanges");
    if (!vec_ranges.empty()) {
      data.b_has_circle_ranges = true;
      for (const std::int64_t n : vec_ranges)
        data.vec_circle_ranges.push_back(
            WDist{static_cast<std::int32_t>(n)});
    }
  }

  const std::string weapon_to_lower = ToLower(data.str_missile_weapon);
  const game::WeaponInfo* weapon_info = nullptr;
  for (const auto& [name, weapon] : world.Map().Rules().Weapons())
    if (name == weapon_to_lower) {
      weapon_info = weapon.get();
      break;
    }

  if (weapon_info == nullptr)
    throw yaml::YamlException(
        "Weapons Ruleset does not contain an entry '" + weapon_to_lower + "'");

  data.ptr_weapon_info = weapon_info;
  return data;
}

NukePower::NukePower(const ActorInitializer& init, NukePowerInfoData info)
    : SupportPower(init, info), nuke_info_{std::move(info)} {}

void NukePower::Created(Actor& self) {
  SupportPower::Created(self);
  ptr_body_ = self.TraitOrDefault<BodyOrientation>();
}

void NukePower::SelectTarget(Actor& self, const std::string& str_order,
                             SupportPowerManager& manager) {
  self.world().AdoptOrderGenerator(std::make_unique<SelectNukePowerTarget>(
      str_order, manager, nuke_info_));
}

void NukePower::Activate(Actor& self, const net::Order& order,
                         SupportPowerManager& manager) {
  SupportPower::Activate(self, order, manager);
  PlayLaunchSounds();
  ActivateAt(self, order.target.CenterPosition());
}

void NukePower::ActivateAt(Actor& self, const WPos& wpos_target) {
  sim::World& world = self.world();
  const std::string str_palette =
      nuke_info_.b_is_player_palette
          ? nuke_info_.str_missile_palette + self.Owner()->InternalName()
          : nuke_info_.str_missile_palette;
  const bool b_skip_ascent = nuke_info_.b_skip_ascent || ptr_body_ == nullptr;
  const WPos wpos_launch =
      b_skip_ascent
          ? WPos{}
          : self.CenterPosition() +
                ptr_body_->LocalToWorld(nuke_info_.wvec_spawn_offset);

  auto up_missile = std::make_unique<NukeLaunch>(
      *self.Owner(), nuke_info_.str_missile_image, *nuke_info_.ptr_weapon_info,
      str_palette, nuke_info_.str_missile_up, nuke_info_.str_missile_down,
      wpos_launch, wpos_target, nuke_info_.dist_detonation_altitude,
      nuke_info_.b_remove_missile_on_detonation,
      nuke_info_.dist_flight_velocity, nuke_info_.int4_missile_delay,
      nuke_info_.int4_flight_delay, b_skip_ascent,
      nuke_info_.str_trail_image, nuke_info_.vec_trail_sequences,
      nuke_info_.str_trail_palette, nuke_info_.b_trail_use_player_palette,
      nuke_info_.int4_trail_delay, nuke_info_.int4_trail_interval);
  const std::shared_ptr<float> ptr_fraction = up_missile->FractionHandle();

  auto sp_holder =
      std::make_shared<std::unique_ptr<NukeLaunch>>(std::move(up_missile));
  world.AddFrameEndTask([sp_holder](sim::World& w) {
    if (*sp_holder != nullptr)
      w.Add(std::move(*sp_holder));
  });

  if (nuke_info_.dist_camera_range != WDist{0}) {
    const sim::ShroudSourceType kind_type =
        nuke_info_.b_reveal_generated_shroud
            ? sim::ShroudSourceType::Visibility
            : sim::ShroudSourceType::PassiveVisibility;
    sim::Player* ptr_owner = self.Owner();
    const int int4_delay = nuke_info_.int4_flight_delay -
                          nuke_info_.int4_camera_spawn_advance;
    const int int4_duration = nuke_info_.int4_camera_spawn_advance +
                             nuke_info_.int4_camera_remove_delay;
    const WDist dist_range = nuke_info_.dist_camera_range;
    const sim::PlayerRelationship kind_relationships =
        nuke_info_.kind_camera_relationships;
    world.AddFrameEndTask([ptr_owner, wpos_target, dist_range, kind_type,
                           int4_delay, int4_duration,
                           kind_relationships](sim::World& w) {
      w.Add(std::make_unique<RevealShroudEffect>(
          wpos_target, dist_range, kind_type, *ptr_owner, kind_relationships,
          int4_delay, int4_duration));
    });
  }

  if (nuke_info_.b_display_beacon) {
    sim::Player* ptr_owner = self.Owner();
    const int int4_beacon_delay = nuke_info_.int4_beacon_delay;
    const int int4_duration =
        nuke_info_.int4_flight_delay - nuke_info_.int4_beacon_remove_advance;
    const bool b_player_palette = nuke_info_.b_beacon_palette_is_player_palette;
    const std::string str_beacon_palette = nuke_info_.str_beacon_palette;
    const std::string str_beacon_image = nuke_info_.str_beacon_image;
    const std::string str_beacon_poster = nuke_info_.str_beacon_poster;
    const std::string str_beacon_poster_palette =
        nuke_info_.str_beacon_poster_palette;
    const std::string str_beacon_sequence = nuke_info_.str_beacon_sequence;
    const std::string str_arrow_sequence = nuke_info_.str_arrow_sequence;
    const std::string str_circle_sequence = nuke_info_.str_circle_sequence;
    const std::string str_clock_sequence = nuke_info_.str_clock_sequence;
    world.AddFrameEndTask(
        [ptr_owner, wpos_target, ptr_fraction, int4_beacon_delay,
         int4_duration, b_player_palette, str_beacon_palette,
         str_beacon_image, str_beacon_poster, str_beacon_poster_palette,
         str_beacon_sequence, str_arrow_sequence, str_circle_sequence,
         str_clock_sequence](sim::World& w) {
          w.Add(std::make_unique<Beacon>(
              *ptr_owner, wpos_target, b_player_palette, str_beacon_palette,
              str_beacon_image, str_beacon_poster,
              str_beacon_poster_palette, str_beacon_sequence,
              str_arrow_sequence, str_circle_sequence, str_clock_sequence,
              [ptr_fraction]() { return *ptr_fraction; }, int4_beacon_delay,
              int4_duration));
        });
  }
}

SelectNukePowerTarget::SelectNukePowerTarget(
    std::string str_order, SupportPowerManager& manager,
    const NukePowerInfoData& info, GameSettingsFace game_settings)
    : SelectGenericPowerTarget(std::move(str_order), manager, info,
                               game_settings),
      nuke_info_{info} {
  if (!nuke_info_.b_has_circle_ranges)
    return;

  vec_circles_.reserve(nuke_info_.vec_circle_ranges.size());
  for (const WDist& dist_range : nuke_info_.vec_circle_ranges)
    vec_circles_.emplace_back(WPos{}, dist_range, 0, nuke_info_.color_circle,
                              nuke_info_.fp4_circle_width,
                              nuke_info_.color_circle_border,
                              nuke_info_.fp4_circle_border_width);
}

std::vector<gfx::IRenderable*> SelectNukePowerTarget::RenderAnnotations(
    gfx::WorldRenderer& wr, sim::World& world) {
  if (!nuke_info_.b_has_circle_ranges)
    return {};

  const CPos cpos_mouse =
      wr.Viewport()->ViewToWorld(gfx::Viewport::int2_last_mouse_pos);
  const WPos wpos_center = world.Map().CenterOfCell(cpos_mouse);

  vec_circle_ptrs_.clear();
  for (std::size_t i = 0; i < vec_circles_.size(); i++) {
    vec_circles_[i] = RangeCircleAnnotationRenderable{
        wpos_center, nuke_info_.vec_circle_ranges[i], 0,
        nuke_info_.color_circle, nuke_info_.fp4_circle_width,
        nuke_info_.color_circle_border, nuke_info_.fp4_circle_border_width};
    vec_circle_ptrs_.push_back(&vec_circles_[i]);
  }
  return vec_circle_ptrs_;
}

SpawnActorPowerInfoData SpawnActorPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  SpawnActorPowerInfoData data;
  static_cast<SupportPowerInfoData&>(data) =
      SupportPowerInfoData::Parse(rec_info);

  if (const auto v = FieldString(rec_info, "Actor"))
    data.str_actor = *v;
  if (const auto v = FieldInt(rec_info, "LifeTime"))
    data.int4_life_time = static_cast<int>(*v);
  {
    const std::vector<std::string> vec_terrain =
        FieldStringArray(rec_info, "Terrain");
    if (!vec_terrain.empty()) {
      data.b_has_terrain = true;
      data.vec_terrain = vec_terrain;
    }
  }
  if (const auto v = FieldBool(rec_info, "AllowUnderShroud"))
    data.b_allow_under_shroud = *v;
  if (const auto v = FieldString(rec_info, "DeploySound"))
    data.str_deploy_sound = *v;
  if (const auto v = FieldString(rec_info, "EffectImage"))
    data.str_effect_image = *v;
  if (const auto v = FieldString(rec_info, "EffectSequence"))
    data.str_effect_sequence = *v;
  if (const auto v = FieldString(rec_info, "EffectPalette"))
    data.str_effect_palette = *v;
  if (const auto v = FieldBool(rec_info, "EffectPaletteIsPlayerPalette"))
    data.b_effect_palette_is_player_palette = *v;
  return data;
}

SpawnActorPower::SpawnActorPower(const ActorInitializer& init,
                                 SpawnActorPowerInfoData info)
    : SupportPower(init, info), spawn_info_{std::move(info)} {}

bool SpawnActorPower::Validate(sim::World& world, CPos cell) const {
  if (!world.Map().Contains(cell))
    return false;

  if (!spawn_info_.b_allow_under_shroud &&
      !Self()->Owner()->GetShroud()->IsExplored(cell))
    return false;

  if (spawn_info_.b_has_terrain) {
    const std::string& str_terrain =
        world.Map().GetTerrainInfo(cell).Type;
    if (std::find(spawn_info_.vec_terrain.begin(),
                  spawn_info_.vec_terrain.end(),
                  str_terrain) == spawn_info_.vec_terrain.end())
      return false;
  }

  return true;
}

void SpawnActorPower::SelectTarget(Actor& self, const std::string& str_order,
                                   SupportPowerManager& manager) {
  net::TextNotificationsManager::Instance().AddTransientLine(
      manager.Self()->Owner(), info_.str_select_target_text_notification);
  self.world().AdoptOrderGenerator(
      std::make_unique<SelectSpawnActorPowerTarget>(str_order, manager, *this));
}

void SpawnActorPower::Activate(Actor& self, const net::Order& order,
                               SupportPowerManager& manager) {
  sim::World& world = self.world();
  const WPos wpos_target = order.target.CenterPosition();
  const CPos cpos_target = world.Map().CellContaining(wpos_target);

  if (!Validate(world, cpos_target))
    return;

  SupportPower::Activate(self, order, manager);

  sim::Player* ptr_owner = self.Owner();
  const std::string str_actor = spawn_info_.str_actor;
  const std::string str_deploy_sound = spawn_info_.str_deploy_sound;
  const std::string str_effect_image = spawn_info_.str_effect_image;
  const std::string str_effect_sequence = spawn_info_.str_effect_sequence;
  const std::string str_effect_palette =
      spawn_info_.b_effect_palette_is_player_palette
          ? spawn_info_.str_effect_palette + ptr_owner->InternalName()
          : spawn_info_.str_effect_palette;
  const int int4_life_time = spawn_info_.int4_life_time;

  world.AddFrameEndTask([this, ptr_owner, cpos_target, wpos_target, str_actor,
                         str_deploy_sound, str_effect_image,
                         str_effect_sequence, str_effect_palette,
                         int4_life_time](sim::World& w) {
    PlayLaunchSounds();
    if (!str_deploy_sound.empty())
      PlaySupportPowerSoundIfWired(str_deploy_sound, w, wpos_target);

    if (!str_effect_sequence.empty() && !str_effect_palette.empty())
      w.Add(std::make_unique<SpriteEffect>(wpos_target, w, str_effect_image,
                                           str_effect_sequence,
                                           str_effect_palette));

    sim::TypeDictionary inits;
    inits.Add(w.Arena().Create<sim::LocationInit>(cpos_target));
    inits.Add(w.Arena().Create<sim::OwnerInit>(ptr_owner));
    sim::Actor* ptr_actor = w.CreateActor(str_actor, inits);

    if (int4_life_time > -1) {
      ptr_actor->QueueActivity(
          w.Arena().Create<activities::Wait>(int4_life_time));
      ptr_actor->QueueActivity(w.Arena().Create<RemoveSelf>());
    }
  });
}

SelectSpawnActorPowerTarget::SelectSpawnActorPowerTarget(
    std::string str_order, SupportPowerManager& manager,
    SpawnActorPower& power, GameSettingsFace game_settings)
    : manager_{manager},
      power_{power},
      str_order_key_{std::move(str_order)},
      game_settings_{game_settings} {}

MouseButton SelectSpawnActorPowerTarget::ActionButton() const {
  return game_settings_.ResolveActionButton(MouseActionType::SupportPower);
}

std::vector<std::unique_ptr<net::Order>> SelectSpawnActorPowerTarget::Order(
    sim::World& world, CPos cell, int2 world_pixel, const MouseInput& mi) {
  if (mi.Button != ActionButton() || mi.Event != MouseInputEvent::Down)
    return {};

  const bool b_valid = power_.Validate(world, cell);
  const std::string str_key = str_order_key_;
  sim::Actor* ptr_subject = manager_.Self();
  world.CancelInputMode();
  if (!b_valid)
    return {};

  auto up_order = std::make_unique<net::Order>(
      str_key, ptr_subject, sim::Target::FromCell(world, cell), false);
  up_order->b_suppress_visual_feedback = true;
  std::vector<std::unique_ptr<net::Order>> vec_out;
  vec_out.push_back(std::move(up_order));
  return vec_out;
}

void SelectSpawnActorPowerTarget::Tick(sim::World& world) {
  if (manager_.FindPower(str_order_key_) == nullptr)
    world.CancelInputMode();
}

std::string SelectSpawnActorPowerTarget::GetCursor(sim::World& world, CPos cell,
                                                   int2, const MouseInput&) {
  return power_.Validate(world, cell) ? power_.Info().str_cursor
                                      : power_.Info().str_blocked_cursor;
}

ProduceActorPowerInfoData ProduceActorPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProduceActorPowerInfoData data;
  static_cast<SupportPowerInfoData&>(data) =
      SupportPowerInfoData::Parse(rec_info);

  data.vec_actors = FieldStringArray(rec_info, "Actors");
  if (const auto v = FieldString(rec_info, "Type"))
    data.str_type = *v;
  if (const auto v = FieldString(rec_info, "ReadyAudio"))
    data.str_ready_audio = *v;
  if (const auto v = FieldString(rec_info, "ReadyTextNotification"))
    data.str_ready_text_notification = *v;
  if (const auto v = FieldString(rec_info, "BlockedAudio"))
    data.str_blocked_audio = *v;
  if (const auto v = FieldString(rec_info, "BlockedTextNotification"))
    data.str_blocked_text_notification = *v;
  return data;
}

ProduceActorPower::ProduceActorPower(const ActorInitializer& init,
                                     ProduceActorPowerInfoData info)
    : SupportPower(init, info),
      produce_info_{std::move(info)},
      str_faction_{init.GetValue<sim::FactionInit, std::string>(
          init.Self().Owner()->Faction().InternalName)} {}

void ProduceActorPower::SelectTarget(Actor& self, const std::string& str_order,
                                     SupportPowerManager& manager) {
  auto up_order = std::make_unique<net::Order>(str_order, manager.Self(),
                                               false);
  self.world().IssueOrder(up_order.get());
}

void ProduceActorPower::Activate(Actor& self, const net::Order& order,
                                 SupportPowerManager& manager) {
  SupportPower::Activate(self, order, manager);
  PlayLaunchSounds();

  sim::World& world = self.world();
  struct Producer {
    sim::Actor* ptr_actor = nullptr;
    Production* ptr_production = nullptr;
    bool b_primary = false;
  };
  std::vector<Producer> vec_producers;
  for (auto& pair : world.ActorsWithTrait<Production>()) {
    if (pair.actor->Owner() != self.Owner() || pair.trait->IsTraitDisabled())
      continue;
    if (std::find(pair.trait->Info().vec_produces.begin(),
                  pair.trait->Info().vec_produces.end(),
                  produce_info_.str_type) ==
        pair.trait->Info().vec_produces.end())
      continue;

    PrimaryBuilding* ptr_primary =
        pair.actor->TraitOrDefault<PrimaryBuilding>();
    vec_producers.push_back(Producer{
        pair.actor, pair.trait,
        ptr_primary != nullptr && ptr_primary->IsPrimary()});
  }

  std::stable_sort(
      vec_producers.begin(), vec_producers.end(),
      [](const Producer& a, const Producer& b) {
        if (a.b_primary != b.b_primary)
          return a.b_primary;
        return a.ptr_actor->ActorID() > b.ptr_actor->ActorID();
      });

  bool b_activated = false;
  for (const Producer& producer : vec_producers) {
    for (const std::string& str_name : produce_info_.vec_actors) {
      const game::ActorInfo* ptr_ai =
          FindActorInfo(world, ToLower(str_name));
      if (ptr_ai == nullptr)
        throw yaml::YamlException("Cannot locate type: " + ToLower(str_name));

      sim::TypeDictionary inits;
      inits.Add(world.Arena().Create<sim::OwnerInit>(self.Owner()));
      inits.Add(world.Arena().Create<sim::FactionInit>(
          BuildableInfoData::GetInitialFaction(*ptr_ai, str_faction_)));

      b_activated |= producer.ptr_production->Produce(
          *producer.ptr_actor, *ptr_ai, produce_info_.str_type, inits, 0);
    }

    if (b_activated)
      break;
  }

  if (b_activated) {
    net::TextNotificationsManager::Instance().AddTransientLine(
        manager.Self()->Owner(), produce_info_.str_ready_text_notification);
  } else {
    net::TextNotificationsManager::Instance().AddTransientLine(
        manager.Self()->Owner(), produce_info_.str_blocked_text_notification);
  }
}

GrantExternalConditionPowerInfoData
GrantExternalConditionPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  GrantExternalConditionPowerInfoData data;
  static_cast<SupportPowerInfoData&>(data) =
      SupportPowerInfoData::Parse(rec_info);

  if (const auto v = FieldString(rec_info, "Condition"))
    data.str_condition = *v;
  if (const auto v = FieldInt(rec_info, "Duration"))
    data.int4_duration = static_cast<int>(*v);
  if (const auto v = FieldCVec(rec_info, "Dimensions"))
    data.cvec_dimensions = *v;
  if (const auto v = FieldString(rec_info, "Footprint"))
    data.str_footprint = *v;
  if (const auto v = FieldString(rec_info, "OnFireSound"))
    data.str_on_fire_sound = *v;
  if (const auto v = FieldRelationship(rec_info, "ValidRelationships"))
    data.kind_valid_relationships = *v;
  if (const auto v = FieldString(rec_info, "Sequence"))
    data.str_sequence = *v;
  if (const auto v = FieldString(rec_info, "FootprintImage"))
    data.str_footprint_image = *v;
  if (const auto v = FieldString(rec_info, "FootprintSequence"))
    data.str_footprint_sequence = *v;
  return data;
}

GrantExternalConditionPower::GrantExternalConditionPower(
    const ActorInitializer& init, GrantExternalConditionPowerInfoData info)
    : SupportPower(init, info), condition_info_{std::move(info)} {
  for (const char c : condition_info_.str_footprint)
    if (!std::isspace(static_cast<unsigned char>(c)))
      vec_footprint_.push_back(c);
}

void GrantExternalConditionPower::SelectTarget(
    Actor& self, const std::string& str_order, SupportPowerManager& manager) {
  self.world().AdoptOrderGenerator(std::make_unique<SelectConditionTarget>(
      self.world(), str_order, manager, *this));
}

void GrantExternalConditionPower::Activate(Actor& self,
                                           const net::Order& order,
                                           SupportPowerManager& manager) {
  SupportPower::Activate(self, order, manager);
  PlayLaunchSounds();

  WithSpriteBody* ptr_wsb = self.TraitOrDefault<WithSpriteBody>();
  if (ptr_wsb != nullptr &&
      ptr_wsb->DefaultAnimation().HasSequence(condition_info_.str_sequence))
    ptr_wsb->PlayCustomAnimation(self, condition_info_.str_sequence);

  PlaySupportPowerSoundIfWired(condition_info_.str_on_fire_sound, self.world(),
                               order.target.CenterPosition());
  for (sim::Actor* ptr_actor :
       UnitsInRange(self.world().Map().CellContaining(
           order.target.CenterPosition()))) {
    for (ExternalCondition* ptr_condition :
         ptr_actor->TraitsImplementing<ExternalCondition>())
      if (ptr_condition->Condition() == condition_info_.str_condition &&
          ptr_condition->CanGrantCondition(&self)) {
        ptr_condition->GrantCondition(*ptr_actor, &self,
                                      condition_info_.int4_duration);
        break;
      }
  }
}

std::vector<sim::Actor*> GrantExternalConditionPower::UnitsInRange(
    CPos cpos_xy) {
  const std::vector<CPos> vec_tiles = CellsMatching(
      cpos_xy, vec_footprint_, condition_info_.cvec_dimensions);
  std::vector<sim::Actor*> vec_units;
  sim::IActorMap* ptr_actor_map = Self()->world().ActorMapFace();
  if (ptr_actor_map == nullptr)
    return vec_units;
  for (const CPos& cpos : vec_tiles)
    for (sim::Actor* ptr_actor : ptr_actor_map->GetActorsAt(cpos))
      if (std::find(vec_units.begin(), vec_units.end(), ptr_actor) ==
          vec_units.end())
        vec_units.push_back(ptr_actor);

  std::erase_if(vec_units, [this](sim::Actor* ptr_actor) {
    if (!sim::HasRelationship(
            condition_info_.kind_valid_relationships,
            Self()->Owner()->RelationshipWith(ptr_actor->Owner())))
      return true;

    for (ExternalCondition* ptr_condition :
         ptr_actor->TraitsImplementing<ExternalCondition>())
      if (ptr_condition->Condition() == condition_info_.str_condition &&
          ptr_condition->CanGrantCondition(Self()))
        return false;
    return true;
  });

  return vec_units;
}

SelectConditionTarget::SelectConditionTarget(
    sim::World& world, std::string str_order, SupportPowerManager& manager,
    GrantExternalConditionPower& power, GameSettingsFace game_settings)
    : manager_{manager},
      power_{power},
      str_order_key_{std::move(str_order)},
      game_settings_{game_settings},
      vec_footprint_{power.Footprint()},
      cvec_dimensions_{power.ConditionInfo().cvec_dimensions} {
  gfx::ISpriteSequence& sequence =
      world.Map().Sequences()->GetSequence(
          power.ConditionInfo().str_footprint_image,
          power.ConditionInfo().str_footprint_sequence);
  sprite_tile_ = sequence.GetSprite(0);
  fp4_alpha_ = sequence.GetAlpha(0);
}

MouseButton SelectConditionTarget::ActionButton() const {
  return game_settings_.ResolveActionButton(MouseActionType::SupportPower);
}

std::vector<std::unique_ptr<net::Order>> SelectConditionTarget::Order(
    sim::World& world, CPos cell, int2 world_pixel, const MouseInput& mi) {
  if (mi.Button != ActionButton() || mi.Event != MouseInputEvent::Down)
    return {};

  const bool b_valid = power_.HasUnitsInRange(cell);
  const std::string str_key = str_order_key_;
  sim::Actor* ptr_subject = manager_.Self();
  world.CancelInputMode();
  if (!b_valid)
    return {};

  auto up_order = std::make_unique<net::Order>(
      str_key, ptr_subject, sim::Target::FromCell(world, cell), false);
  up_order->b_suppress_visual_feedback = true;
  std::vector<std::unique_ptr<net::Order>> vec_out;
  vec_out.push_back(std::move(up_order));
  return vec_out;
}

void SelectConditionTarget::Tick(sim::World& world) {
  const SupportPowerInstance* ptr_power = manager_.FindPower(str_order_key_);
  if (ptr_power == nullptr || !ptr_power->Active() || !ptr_power->Ready())
    world.CancelInputMode();
}

std::vector<gfx::IRenderable*> SelectConditionTarget::Render(
    gfx::WorldRenderer& wr, sim::World& world) {
  (void)wr;
  (void)world;
  return {};
}

std::vector<gfx::IRenderable*> SelectConditionTarget::RenderAnnotations(
    gfx::WorldRenderer& wr, sim::World& world) {
  (void)wr;
  (void)world;
  return {};
}

std::string SelectConditionTarget::GetCursor(sim::World& world, CPos cell,
                                             int2, const MouseInput&) {
  (void)world;
  return power_.HasUnitsInRange(cell) ? power_.Info().str_cursor
                                      : power_.Info().str_blocked_cursor;
}

}
