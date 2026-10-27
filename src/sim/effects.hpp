#pragma once
import std;

#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

class IEffect {
 public:
  virtual ~IEffect() = default;
  virtual void Tick(World& world) = 0;
};

class ISpatiallyPartitionable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Effects_ISpatiallyPartitionable;
  virtual ~ISpatiallyPartitionable() = default;
};

class DelayedAction final : public IEffect {
 public:
  DelayedAction(int delay, std::function<void()> fn)
      : fn_(std::move(fn)), int4_delay_(delay) {}

  void Tick(World& world) override;

 private:
  std::function<void()> fn_;
  int int4_delay_;
};

}
