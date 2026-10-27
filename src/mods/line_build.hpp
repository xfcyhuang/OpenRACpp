#pragma once
import std;

#include "core/cvec.hpp"
#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::sim {
class ActorInitializer;
}

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

enum class LineBuildDirection : std::int32_t { Unset = 0, X = 1, Y = 2 };

class LineBuildDirectionInit : public sim::ValueActorInit<LineBuildDirection>,
                               public sim::ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(LineBuildDirectionInit,
                OpenRA_Mods_Common_Traits_LineBuildDirectionInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit LineBuildDirectionInit(LineBuildDirection value)
      : ValueActorInit<LineBuildDirection>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class LineBuildParentInit : public sim::ValueActorInit<std::vector<sim::Actor*>>,
                            public sim::ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(LineBuildParentInit,
                OpenRA_Mods_Common_Traits_LineBuildParentInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit LineBuildParentInit(std::vector<sim::Actor*> value)
      : ValueActorInit<std::vector<sim::Actor*>>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

struct LineBuildInfoData {
  int int4_range = 5;
  std::vector<std::string> vec_node_types{"wall"};
  std::string str_segment_type;
  bool b_segments_require_node = false;

  static LineBuildInfoData Parse(const meta::RecordObject& rec_info);
};

class LineBuild final : public sim::TraitBase,
                        public sim::INotifyKilled,
                        public sim::INotifyAddedToWorld,
                        public sim::INotifyRemovedFromWorld,
                        public sim::INotifyLineBuildSegmentsChanged {
 public:
  ORA_TRAIT_INTERFACES(
      LineBuild, OpenRA_Mods_Common_Traits_LineBuild, sim::INotifyKilled,
      sim::INotifyAddedToWorld, sim::INotifyRemovedFromWorld,
      sim::INotifyLineBuildSegmentsChanged)

  LineBuild(sim::ActorInitializer& init, LineBuildInfoData info);

  void AddedToWorld(sim::Actor& self) override;
  void RemovedFromWorld(sim::Actor& self) override;
  void Killed(sim::Actor& self,
              const sim::AttackInfo& e) override;

  void SegmentAdded(sim::Actor& self, sim::Actor* segment) override;
  void SegmentRemoved(sim::Actor& self, sim::Actor* segment) override;

  const LineBuildInfoData& Info() const { return info_; }

 private:
  LineBuildInfoData info_;
  std::vector<sim::Actor*> vec_parent_nodes_;
  std::vector<sim::Actor*> vec_segments_;
};

struct LineBuildNodeInfoData {
  std::vector<std::string> vec_types{"wall"};
  std::vector<CVec> vec_connections{{CVec{1, 0}, CVec{0, 1}, CVec{-1, 0}, CVec{0, -1}}};

  static LineBuildNodeInfoData Parse(const meta::RecordObject& rec_info);
};

class LineBuildNode final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_LineBuildNode;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<LineBuildNode>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

}
