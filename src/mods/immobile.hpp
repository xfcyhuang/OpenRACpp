// UPSTREAM: OpenRA.Mods.Common/Traits/Immobile.cs @4f7930b5 L21-76 全文
//          (第十二批 G2 黄金对拍补面:mpspawn 等编辑器摆位 actor 的
//          ISync 条目 {TopLeft, CenterPosition} 缺失平移 SyncHash 的 n
//          计数)
//          The whole of Immobile.cs L21-76 (the batch-12 G2 golden
//          differential's missing face: editor-placement actors like
//          mpspawn lacked their ISync entries {TopLeft, CenterPosition},
//          shifting the SyncHash n counter).
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

/// ImmobileInfo 的解析面(L21-35)
/// The parsed face of ImmobileInfo (L21-35).
struct ImmobileInfoData {
  bool b_occupies_space = true;  // L26

  static ImmobileInfoData Parse(const meta::RecordObject& rec_info);
};

/// Immobile(L38-76):不可移动体的占位/同步面
/// Immobile (L38-76): the immovable body's occupancy/sync face.
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

  /// L50-51:[VerifySync] {TopLeft, CenterPosition}
  CPos TopLeft() const override { return pos_top_left_; }
  WPos CenterPosition() const override { return pos_center_; }

  /// L53:OccupiedCells
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

}  // namespace ora::mods
