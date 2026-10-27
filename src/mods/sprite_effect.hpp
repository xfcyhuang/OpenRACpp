#pragma once
import std;

#include "core/wpos.hpp"
#include "gfx/animation.hpp"
#include "gfx/renderable.hpp"
#include "sim/effects.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

class SpriteEffect final : public sim::IEffect,
                           public sim::ISpatiallyPartitionable {
 public:
  SpriteEffect(WPos wpos_pos, sim::World& world_ref,
               std::string str_image, std::string str_sequence,
               std::string str_palette, bool b_visible_through_fog = false,
               int int4_delay = 0);

  void Tick(sim::World& world_ref) override;

  void Render(gfx::WorldRenderer& wr, std::vector<gfx::RenderItem>& vec_out);

  const std::string& ImageNameForTest() const { return anim_.Name(); }
  const std::string& SequenceNameForTest() const { return str_sequence_; }

 private:
  sim::World& world_ref_;
  std::string str_palette_;
  std::string str_sequence_;
  gfx::Animation anim_;
  WPos wpos_pos_;
  bool b_visible_through_fog_ = false;
  bool b_initialized_ = false;
  int int4_delay_ = 0;
};

}
