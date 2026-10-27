#include "mods/support_power_effects.hpp"

#include "game/game_records.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "mods/projectiles.hpp"
#include "mods/sprite_effect.hpp"
#include "sim/effects.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/weapons.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

gfx::Animation::Deps MakeAnimDeps(const sim::World& world_ref) {
  gfx::Animation::Deps deps{};
  deps.ptr_sequences = world_ref.Map().Sequences();
  const map::Map* ptr_map = &world_ref.Map();
  deps.fn_distance_above_terrain =
      [ptr_map](WPos pos) { return ptr_map->DistanceAboveTerrain(pos); };
  return deps;
}

}

RangeCircleAnnotationRenderable::RangeCircleAnnotationRenderable(
    WPos wpos_pos, WDist dist_radius, std::int32_t int4_z_offset,
    core::Color color_c, float fp4_width, core::Color color_border,
    float fp4_border_width)
    : wpos_pos_{wpos_pos},
      dist_radius_{dist_radius},
      int4_z_offset_{int4_z_offset},
      color_c_{color_c},
      fp4_width_{fp4_width},
      color_border_{color_border},
      fp4_border_width_{fp4_border_width} {}

gfx::IRenderable* RangeCircleAnnotationRenderable::WithZOffset(
    std::int32_t int4_new_offset) {
  int4_z_offset_ = int4_new_offset;
  return this;
}

gfx::IRenderable* RangeCircleAnnotationRenderable::OffsetBy(
    const WVec& vec_offset) {
  wpos_pos_ = wpos_pos_ + vec_offset;
  return this;
}

void RangeCircleAnnotationRenderable::DrawRangeCircle(
    gfx::WorldRenderer& wr, const WPos& wpos_center, WDist dist_radius,
    float fp4_width, core::Color color_c, float fp4_border_width,
    core::Color color_border) {
  constexpr int kSegments = 32;
  gfx::Renderer* ptr_renderer = wr.RendererPtr();
  if (ptr_renderer == nullptr)
    return;
  gfx::RgbaColorRenderer& cr = ptr_renderer->WorldRgbaColorRenderer();

  const WVec vec_offset{dist_radius.Length, 0, 0};
  for (int i = 0; i < kSegments; i++) {
    const WRot rot_start = WRot::FromFacing(8 * i);
    const WRot rot_end = WRot::FromFacing(8 * i + 6);
    const core::Vector2 vec_sa = wr.ScreenPosition(
        wpos_center + vec_offset.Rotate(rot_start));
    const core::Vector2 vec_sb = wr.ScreenPosition(
        wpos_center + vec_offset.Rotate(rot_end));
    const int2 px_a = wr.Viewport()->WorldToViewPx(
        core::Vector3{vec_sa.X, vec_sa.Y, 0.0f});
    const int2 px_b = wr.Viewport()->WorldToViewPx(
        core::Vector3{vec_sb.X, vec_sb.Y, 0.0f});
    const core::Vector3 vec_a{static_cast<float>(px_a.X),
                              static_cast<float>(px_a.Y), 0.0f};
    const core::Vector3 vec_b{static_cast<float>(px_b.X),
                              static_cast<float>(px_b.Y), 0.0f};

    if (fp4_border_width > 0)
      cr.DrawLine(vec_a, vec_b, fp4_border_width, color_border);

    if (fp4_width > 0)
      cr.DrawLine(vec_a, vec_b, fp4_width, color_c);
  }
}

void RangeCircleAnnotationRenderable::Render(gfx::WorldRenderer& wr) {
  DrawRangeCircle(wr, wpos_pos_, dist_radius_, fp4_width_, color_c_,
                  fp4_border_width_, color_border_);
}

RevealShroudEffect::RevealShroudEffect(WPos wpos_pos, WDist dist_radius,
                                       sim::ShroudSourceType kind_type,
                                       sim::Player& player_for,
                                       sim::PlayerRelationship kind_stances,
                                       int delay, int duration)
    : wpos_pos_{wpos_pos},
      ptr_player_{&player_for},
      dist_reveal_radius_{dist_radius},
      kind_valid_stances_{kind_stances},
      kind_source_type_{kind_type},
      int4_duration_{duration},
      int4_ticks_{-delay} {}

void RevealShroudEffect::AddCellsToPlayerShroud(
    sim::Player& p, const std::vector<PPos>& vec_uv) {
  if (!sim::HasRelationship(kind_valid_stances_,
                            ptr_player_->RelationshipWith(&p)))
    return;

  p.GetShroud()->AddSource(this, kind_source_type_, vec_uv);
}

void RevealShroudEffect::RemoveCellsFromPlayerShroud(sim::Player& p) {
  p.GetShroud()->RemoveSource(this);
}

std::vector<PPos> RevealShroudEffect::ProjectedCells(sim::World& world) {
  if (dist_reveal_radius_ == WDist{0})
    return {};

  return sim::Shroud::ProjectedCellsInRange(world.Map(), wpos_pos_,
                                            WDist{0}, dist_reveal_radius_);
}

void RevealShroudEffect::Tick(sim::World& world) {
  if (int4_ticks_ == 0) {
    const std::vector<PPos> vec_cells = ProjectedCells(world);
    for (auto* p : world.Players())
      AddCellsToPlayerShroud(*p, vec_cells);
  }

  if (int4_ticks_ == int4_duration_) {
    for (auto* p : world.Players())
      RemoveCellsFromPlayerShroud(*p);

    RevealShroudEffect* ptr_self = this;
    world.AddFrameEndTask(
        [ptr_self](sim::World& w) { w.Remove(ptr_self); });
  }

  int4_ticks_++;
}

Beacon::Beacon(sim::Player& owner, WPos wpos_pos, int duration,
               std::string str_beacon_palette, bool b_is_player_palette,
               std::string str_beacon_collection,
               std::string str_beacon_sequence, std::string str_arrow_sprite,
               std::string str_circle_sprite, int delay)
    : ptr_owner_{&owner},
      wpos_pos_{wpos_pos},
      b_is_player_palette_{b_is_player_palette},
      str_beacon_palette_{std::move(str_beacon_palette)},
      str_beacon_collection_{std::move(str_beacon_collection)},
      int4_duration_{duration},
      int4_delay_{delay} {
  const gfx::Animation::Deps deps = MakeAnimDeps(owner.GetWorld());

  if (!str_beacon_sequence.empty()) {
    opt_beacon_.emplace(deps, str_beacon_collection_);
    opt_beacon_->PlayRepeating(str_beacon_sequence);
  }

  if (!str_arrow_sprite.empty()) {
    opt_arrow_.emplace(deps, str_beacon_collection_);
    opt_arrow_->Play(str_arrow_sprite);
  }

  if (!str_circle_sprite.empty()) {
    opt_circles_.emplace(deps, str_beacon_collection_);
    opt_circles_->Play(str_circle_sprite);
  }
}

Beacon::Beacon(sim::Player& owner, WPos wpos_pos, bool b_is_player_palette,
               std::string str_palette, std::string str_poster_collection,
               std::string str_poster_type, std::string str_poster_palette,
               std::string str_beacon_sequence, std::string str_arrow_sequence,
               std::string str_circle_sequence, std::string str_clock_sequence,
               std::function<float()> fn_clock_fraction, int delay,
               int duration)
    : Beacon(owner, wpos_pos, duration, std::move(str_palette),
             b_is_player_palette, std::move(str_poster_collection),
             std::move(str_beacon_sequence), std::move(str_arrow_sequence),
             std::move(str_circle_sequence), delay) {
  str_poster_palette_ = std::move(str_poster_palette);

  if (!str_poster_type.empty()) {
    const gfx::Animation::Deps deps = MakeAnimDeps(ptr_owner_->GetWorld());
    opt_poster_.emplace(deps, str_beacon_collection_);
    opt_poster_->Play(str_poster_type);

    if (fn_clock_fraction != nullptr) {
      opt_clock_.emplace(deps, str_beacon_collection_);
      gfx::Animation* ptr_clock = &*opt_clock_;
      opt_clock_->PlayFetchIndex(
          str_clock_sequence, [ptr_clock, fn_clock_fraction]() {
            const std::int32_t len = ptr_clock->CurrentSequence()->Length();
            const int raw = static_cast<int>(fn_clock_fraction() *
                                             static_cast<float>(len - 1));
            return std::clamp(raw, 0, len - 1);
          });
    }
  }
}

void Beacon::Tick(sim::World& world) {
  if (int4_delay_-- > 0)
    return;

  int4_arrow_height_ += int4_arrow_speed_;
  const int clamped = std::clamp(int4_arrow_height_, 0, kMaxArrowHeight);
  if (int4_arrow_height_ != clamped) {
    int4_arrow_height_ = clamped;
    int4_arrow_speed_ *= -1;
  }

  if (opt_arrow_.has_value())
    opt_arrow_->Tick();
  if (opt_beacon_.has_value())
    opt_beacon_->Tick();
  if (opt_circles_.has_value())
    opt_circles_->Tick();
  if (opt_clock_.has_value())
    opt_clock_->Tick();

  if (int4_duration_ > 0 && int4_duration_ <= int4_tick_++) {
    Beacon* ptr_self = this;
    world.AddFrameEndTask(
        [ptr_self](sim::World& w) { w.Remove(ptr_self); });
  }
}

void Beacon::RenderAboveShroud(gfx::WorldRenderer& wr,
                               std::vector<gfx::RenderItem>& vec_out) {
  if (int4_delay_ > 0)
    return;

  if (!ptr_owner_->IsAlliedWith(ptr_owner_->GetWorld().RenderPlayer()))
    return;

  const gfx::PaletteReference* ptr_palette = wr.Palette(
      b_is_player_palette_
          ? str_beacon_palette_ + ptr_owner_->InternalName()
          : str_beacon_palette_);

  const auto append = [&vec_out](std::optional<gfx::Animation>& anim,
                                 WPos pos,
                                 const gfx::PaletteReference* pal) {
    if (!anim.has_value())
      return;
    std::array<gfx::RenderItem, 2> arr_items{};
    std::int32_t int4_count = 0;
    anim->Render(pos, pal, arr_items, int4_count);
    for (std::int32_t i = 0; i < int4_count; i++)
      vec_out.push_back(arr_items[static_cast<std::size_t>(i)]);
  };

  append(opt_beacon_, wpos_pos_, ptr_palette);
  append(opt_circles_, wpos_pos_, ptr_palette);
  append(opt_arrow_, wpos_pos_ + WVec{0, 0, int4_arrow_height_}, ptr_palette);

  if (opt_poster_.has_value()) {
    const gfx::PaletteReference* ptr_poster_pal =
        wr.Palette(str_poster_palette_);
    append(opt_poster_, wpos_pos_, ptr_poster_pal);
    append(opt_clock_, wpos_pos_, ptr_poster_pal);
  }
}

NukeLaunch::NukeLaunch(sim::Player& fired_by, std::string str_image,
                       const game::WeaponInfo& weapon,
                       std::string str_weapon_palette,
                       std::string str_up_sequence,
                       std::string str_down_sequence, WPos wpos_launch_pos,
                       WPos wpos_target_pos, WDist dist_detonation_altitude,
                       bool b_remove_on_detonation, WDist dist_velocity,
                       int int4_launch_delay, int int4_impact_delay,
                       bool b_skip_ascent, std::string str_trail_image,
                       std::vector<std::string> vec_trail_sequences,
                       std::string str_trail_palette,
                       bool b_trail_use_player_palette, int int4_trail_delay,
                       int int4_trail_interval)
    : ptr_fired_by_{&fired_by},
      opt_anim_{},
      weapon_{&weapon},
      str_weapon_palette_{std::move(str_weapon_palette)},
      str_up_sequence_{std::move(str_up_sequence)},
      str_down_sequence_{std::move(str_down_sequence)},
      dist_detonation_altitude_{dist_detonation_altitude},
      b_remove_on_detonation_{b_remove_on_detonation},
      int4_impact_delay_{int4_impact_delay},
      int4_turn_{b_skip_ascent ? 0 : int4_impact_delay / 2},
      str_trail_image_{std::move(str_trail_image)},
      vec_trail_sequences_{std::move(vec_trail_sequences)},
      str_trail_palette_{std::move(str_trail_palette)},
      int4_trail_interval_{int4_trail_interval},
      int4_trail_delay_{int4_trail_delay},
      int4_trail_ticks_{int4_trail_delay},
      int4_launch_delay_{int4_launch_delay} {
  if (b_trail_use_player_palette)
    str_trail_palette_ += fired_by.InternalName();

  const WVec vec_offset{
      WDist{0}, WDist{0},
      WDist{dist_velocity.Length * (int4_impact_delay_ - int4_turn_)}};
  wpos_ascend_source_ = wpos_launch_pos;
  wpos_ascend_target_ = wpos_launch_pos + vec_offset;
  wpos_descend_source_ = wpos_target_pos + vec_offset;
  wpos_descend_target_ = wpos_target_pos;

  if (!str_image.empty())
    opt_anim_.emplace(MakeAnimDeps(fired_by.GetWorld()), std::move(str_image));

  wpos_pos_ = b_skip_ascent ? wpos_descend_source_ : wpos_ascend_source_;
}

void NukeLaunch::Tick(sim::World& world) {
  if (int4_launch_delay_-- > 0)
    return;

  if (!b_is_launched_) {
    if (!weapon_->vec_report.empty()) {
      const std::int32_t index = world.LocalRandom().Next(
          0, static_cast<std::int32_t>(weapon_->vec_report.size()));
      PlayProjectileSoundIfWired(
          weapon_->vec_report[static_cast<std::size_t>(index)], world,
          wpos_pos_);
    }

    if (opt_anim_.has_value()) {
      opt_anim_->PlayRepeating(str_up_sequence_);
      world.ScreenMapFace()->Add(this, wpos_pos_, opt_anim_->Image());
    }

    b_is_launched_ = true;
  }

  if (opt_anim_.has_value()) {
    opt_anim_->Tick();

    if (int4_ticks_ == int4_turn_)
      opt_anim_->PlayRepeating(str_down_sequence_);
  }

  const bool b_is_descending = int4_ticks_ >= int4_turn_;
  if (!b_is_descending)
    wpos_pos_ = WPos::LerpQuadratic(wpos_ascend_source_, wpos_ascend_target_,
                                    WAngle{0}, int4_ticks_, int4_turn_);
  else
    wpos_pos_ = WPos::LerpQuadratic(
        wpos_descend_source_, wpos_descend_target_, WAngle{0},
        int4_ticks_ - int4_turn_, int4_impact_delay_ - int4_turn_);

  if (!str_trail_image_.empty() && --int4_trail_ticks_ < 0) {
    const WPos wpos_trail =
        !b_is_descending
            ? WPos::LerpQuadratic(wpos_ascend_source_, wpos_ascend_target_,
                                  WAngle{0}, int4_ticks_ - int4_trail_delay_,
                                  int4_turn_)
            : WPos::LerpQuadratic(
                  wpos_descend_source_, wpos_descend_target_, WAngle{0},
                  int4_ticks_ - int4_turn_ - int4_trail_delay_,
                  int4_impact_delay_ - int4_turn_);

    const std::string str_image = str_trail_image_;
    const std::vector<std::string> vec_sequences = vec_trail_sequences_;
    const std::string str_palette = str_trail_palette_;
    world.AddFrameEndTask(
        [str_image, vec_sequences, str_palette, wpos_trail](sim::World& w) {
          const std::int32_t index = w.SharedRandom().Next(
              0, static_cast<std::int32_t>(vec_sequences.size()));
          w.Add(std::make_unique<SpriteEffect>(
              wpos_trail, w, str_image,
              vec_sequences[static_cast<std::size_t>(index)], str_palette));
        });

    int4_trail_ticks_ = int4_trail_interval_;
  }

  const WDist dat = world.Map().DistanceAboveTerrain(wpos_pos_);
  if (int4_ticks_ == int4_impact_delay_ ||
      (b_is_descending && dat <= dist_detonation_altitude_))
    Explode(world, int4_ticks_ == int4_impact_delay_ ||
                       b_remove_on_detonation_);

  if (opt_anim_.has_value())
    world.ScreenMapFace()->Update(this, wpos_pos_, opt_anim_->Image());

  int4_ticks_++;
}

float NukeLaunch::FractionComplete() const {
  return static_cast<float>(int4_ticks_) / static_cast<float>(int4_impact_delay_);
}

void NukeLaunch::Explode(sim::World& world, bool b_remove_projectile) {
  if (b_remove_projectile) {
    b_in_world_ = false;
    NukeLaunch* ptr_self = this;
    world.AddFrameEndTask([ptr_self](sim::World& w) {
      w.Remove(ptr_self);
      w.ScreenMapFace()->Remove(ptr_self);
    });
  }

  if (b_detonated_)
    return;

  const sim::Target target = sim::Target::FromPos(wpos_pos_);
  sim::WarheadArgs warhead_args{};
  warhead_args.weapon = weapon_;
  warhead_args.source = target.CenterPosition();
  warhead_args.source_actor = ptr_fired_by_->PlayerActor();
  warhead_args.weapon_target = target;

  sim::WeaponImpact(*weapon_, target, warhead_args);

  b_detonated_ = true;
}

void NukeLaunch::Render(gfx::WorldRenderer& wr,
                        std::vector<gfx::RenderItem>& vec_out) {
  if (!b_is_launched_ || !opt_anim_.has_value())
    return;

  std::array<gfx::RenderItem, 2> arr_items{};
  std::int32_t int4_count = 0;
  opt_anim_->Render(wpos_pos_, wr.Palette(str_weapon_palette_), arr_items,
                    int4_count);
  for (std::int32_t i = 0; i < int4_count; i++)
    vec_out.push_back(arr_items[static_cast<std::size_t>(i)]);
}

}
