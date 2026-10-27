#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "mods/sprite_effect.hpp"
#include "sim/effects.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

gfx::Animation::Deps MakeDeps(const sim::World& world_ref) {
  gfx::Animation::Deps deps{};
  deps.ptr_sequences = world_ref.Map().Sequences();
  const map::Map* ptr_map = &world_ref.Map();
  deps.fn_distance_above_terrain =
      [ptr_map](WPos pos) { return ptr_map->DistanceAboveTerrain(pos); };
  return deps;
}

}

SpriteEffect::SpriteEffect(WPos wpos_pos, sim::World& world_ref,
                           std::string str_image, std::string str_sequence,
                           std::string str_palette,
                           bool b_visible_through_fog, int int4_delay)
    : world_ref_{world_ref},
      str_palette_{std::move(str_palette)},
      str_sequence_{std::move(str_sequence)},
      anim_{MakeDeps(world_ref), std::move(str_image)},
      wpos_pos_{wpos_pos},
      b_visible_through_fog_{b_visible_through_fog},
      int4_delay_{int4_delay} {}

void SpriteEffect::Tick(sim::World& world_ref) {

  if (int4_delay_-- > 0)
    return;

  if (!b_initialized_) {

    SpriteEffect* ptr_self = this;
    sim::World* ptr_world = &world_ref_;
    anim_.PlayThen(str_sequence_, [ptr_self, ptr_world]() {
      ptr_world->AddFrameEndTask(
          [ptr_self](sim::World& w) {
            w.Remove(ptr_self);
            w.ScreenMapFace()->Remove(ptr_self);
          });
    });
    world_ref.ScreenMapFace()->Add(this, wpos_pos_, anim_.Image());
    b_initialized_ = true;
  } else {
    anim_.Tick();

    world_ref.ScreenMapFace()->Update(this, wpos_pos_, anim_.Image());
  }
}

void SpriteEffect::Render(gfx::WorldRenderer& wr,
                          std::vector<gfx::RenderItem>& vec_out) {
  if (!b_initialized_ ||
      (!b_visible_through_fog_ && world_ref_.FogObscures(wpos_pos_)))
    return;

  std::array<gfx::RenderItem, 2> arr_items{};
  std::int32_t int4_count = 0;
  anim_.Render(wpos_pos_, wr.Palette(str_palette_), arr_items, int4_count);
  for (std::int32_t i = 0; i < int4_count; ++i)
    vec_out.push_back(arr_items[static_cast<std::size_t>(i)]);
}

}
