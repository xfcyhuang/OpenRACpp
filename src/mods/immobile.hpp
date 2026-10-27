#pragma once
import std;

#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct ImmobileInfoData {
  bool b_occupies_space = true;

  static ImmobileInfoData Parse(const meta::RecordObject& rec_info);
};

class Immobile final : public TraitBase,
                       public sim::IOccupySpace,
                       public sim::ISync,
                       public sim::INotifyAddedToWorld,
                       public sim::INotifyRemovedFromWorld {
 public:
  Immobile(ActorInitializer& init, ImmobileInfoData&& info);

  ORA_TRAIT_INTERFACES(Immobile, OpenRA_Mods_Common_Traits_Immobile,
                       sim::IOccupySpace, sim::ISync,
                       sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld)

  CPos TopLeft() const override { return pos_top_left_; }
  WPos CenterPosition() const override { return pos_center_; }

  std::vector<std::pair<CPos, sim::SubCell>> OccupiedCells()
      const override {
    return vec_occupied_;
  }

  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;

 private:
  CPos pos_top_left_{};
  WPos pos_center_{};
  std::vector<std::pair<CPos, sim::SubCell>> vec_occupied_;
  ImmobileInfoData info_;
};

}
