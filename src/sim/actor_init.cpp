// UPSTREAM: OpenRA.Game/Map/ActorInitializer.cs @b6fc03f L223-262(OwnerInit)
//          The OwnerInit half of ActorInitializer.cs.
#include "sim/actor_init.hpp"

#include "sim/world.hpp"

namespace ora::sim {

OwnerInit::OwnerInit(Player* value)
    : ActorInit(value != nullptr ? value->InternalName() : ""),
      p_value_(value),
      str_internal_name_(value != nullptr ? value->InternalName() : "") {}

OwnerInit::OwnerInit(std::string str_internal_name)
    : ActorInit(str_internal_name),
      str_internal_name_(std::move(str_internal_name)) {}

Player* OwnerInit::Value(World& world) const {
  // L239-241:显式 Player 优先;否则按 InternalName 取首个(未命中抛 ——
  // 上游 First() 的 InvalidOperationException 等价)
  if (p_value_ != nullptr)
    return p_value_;
  for (auto* p : world.Players())
    if (p->InternalName() == str_internal_name_)
      return p;
  throw std::runtime_error("No player found with internal name '" +
                           str_internal_name_ + "'.");
}

}  // namespace ora::sim
