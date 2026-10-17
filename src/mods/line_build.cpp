// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/LineBuild.cs @e180e3225
//          (实现部分)+ Traits/Buildings/LineBuildNode.cs
//          The implementation half + LineBuildNode.cs.
#include "mods/line_build.hpp"

#include "meta/generic_record.hpp"
#include "sim/actor_init.hpp"
#include "sim/world.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

LineBuildInfoData LineBuildInfoData::Parse(const meta::RecordObject& rec_info) {
  LineBuildInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "Range"))
    data.int4_range = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "NodeTypes"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val)) {
      data.vec_node_types.clear();
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_node_types.push_back(*s);
    }
  if (const auto v = sim::RecordFieldString(rec_info, "SegmentType"))
    if (!v->empty())
      data.str_segment_type = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "SegmentsRequireNode"))
    data.b_segments_require_node = *v != 0;
  return data;
}

LineBuildNodeInfoData LineBuildNodeInfoData::Parse(
    const meta::RecordObject& rec_info) {
  LineBuildNodeInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Types"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val)) {
      data.vec_types.clear();
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_types.push_back(*s);
    }
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Connections"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val)) {
      data.vec_connections.clear();
      for (const auto& item : *items)
        if (const auto* tuple = std::get_if<meta::GenericTuple>(&item.val))
          if (tuple->uint1_count >= 2)
            data.vec_connections.push_back(
                CVec{static_cast<int>(tuple->arr_ints[0]),
                     static_cast<int>(tuple->arr_ints[1])});
    }
  return data;
}

LineBuild::LineBuild(sim::ActorInitializer& init, LineBuildInfoData info)
    : info_{std::move(info)} {
  if (const auto* init_parents =
          init.GetOrDefault<LineBuildParentInit>())
    vec_parent_nodes_ = init_parents->Value();
}

void LineBuild::SegmentAdded(sim::Actor&, sim::Actor* segment) {
  for (sim::Actor* s : vec_segments_)
    if (s == segment)
      return;
  vec_segments_.push_back(segment);
}

void LineBuild::SegmentRemoved(sim::Actor&, sim::Actor* segment) {
  std::erase(vec_segments_, segment);
}

void LineBuild::AddedToWorld(sim::Actor& self) {
  for (sim::Actor* parent : vec_parent_nodes_) {
    if (parent == nullptr || parent->Disposed())
      continue;
    for (auto* n :
         parent->TraitsImplementing<sim::INotifyLineBuildSegmentsChanged>())
      n->SegmentAdded(*parent, &self);
  }
}

void LineBuild::RemovedFromWorld(sim::Actor& self) {
  for (sim::Actor* parent : vec_parent_nodes_) {
    if (parent == nullptr || parent->Disposed())
      continue;
    for (auto* n :
         parent->TraitsImplementing<sim::INotifyLineBuildSegmentsChanged>())
      n->SegmentRemoved(*parent, &self);
  }

  if (info_.b_segments_require_node)
    for (sim::Actor* s : vec_segments_)
      s->Kill(nullptr);
}

void LineBuild::Killed(sim::Actor& self, const sim::AttackInfo& e) {
  if (info_.b_segments_require_node)
    for (sim::Actor* s : vec_segments_)
      s->Kill(const_cast<sim::Actor*>(e.Attacker));
}

}  // namespace ora::mods
