#include "sim/effects.hpp"

#include "sim/world.hpp"

namespace ora::sim {

void DelayedAction::Tick(World& world) {

  if (--int4_delay_ <= 0) {
    auto fn = fn_;
    world.AddFrameEndTask([this, fn](World& w) {
      w.Remove(this);
      fn();
    });
  }
}

}
