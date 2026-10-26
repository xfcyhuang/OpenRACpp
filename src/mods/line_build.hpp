// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/LineBuild.cs @1f594e320
//          L19-124 全文 + Traits/Buildings/LineBuildNode.cs L18-34
//          The whole of LineBuild.cs L19-124 + LineBuildNode.cs L18-34.
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

/// LineBuildParentInit(L26-44):parents 为 Actor 引用载荷(Value 的 string[]
/// 名字面仅 SpawnMapActors 回退;C++ 直持 Actor* —— 上游 ActorValue 的两
/// 分支合并为构造注入)
/// LineBuildParentInit (L26-44): parents travel as Actor references
/// (upstream's string[] Value only feeds the SpawnMapActors fallback; C++
/// holds Actor* directly — upstream ActorValue's two branches merge into
/// the constructor injection).
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
  int int4_range = 5;  // L57
  std::vector<std::string> vec_node_types{"wall"};  // L60
  std::string str_segment_type;                     // L63(null → 空)
  bool b_segments_require_node = false;             // L66

  static LineBuildInfoData Parse(const meta::RecordObject& rec_info);
};

/// LineBuild(L71-123)
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

  void AddedToWorld(sim::Actor& self) override;      // L97-103
  void RemovedFromWorld(sim::Actor& self) override;  // L105-115
  void Killed(sim::Actor& self,
              const sim::AttackInfo& e) override;    // L117-122

  void SegmentAdded(sim::Actor& self, sim::Actor* segment) override;    // L85-90
  void SegmentRemoved(sim::Actor& self, sim::Actor* segment) override;  // L91-95

  const LineBuildInfoData& Info() const { return info_; }

 private:
  LineBuildInfoData info_;
  std::vector<sim::Actor*> vec_parent_nodes_;  // L74
  std::vector<sim::Actor*> vec_segments_;      // L75(HashSet → vector;规模 = 段数)
};

struct LineBuildNodeInfoData {
  std::vector<std::string> vec_types{"wall"};  // L24
  std::vector<CVec> vec_connections{{CVec{1, 0}, CVec{0, 1}, CVec{-1, 0}, CVec{0, -1}}};  // L27

  static LineBuildNodeInfoData Parse(const meta::RecordObject& rec_info);
};

/// LineBuildNode(空运行时类)
/// LineBuildNode (the empty runtime class).
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

}  // namespace ora::mods
