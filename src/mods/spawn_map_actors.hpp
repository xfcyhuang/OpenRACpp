// UPSTREAM: OpenRA.Mods.Common/Traits/World/SpawnMapActors.cs @b6fc03f
//          L17-75 全文 + Game/Map/ActorReference.cs L17-113 的装载面
//          (InitRegistry 的 from-yaml 工厂 = LoadInit 的 C++ 承载)
//          The whole of SpawnMapActors.cs L17-75 + the loading face of
//          Game/Map/ActorReference.cs L17-113 (InitRegistry's from-yaml
//          factories = LoadInit's C++ carrier).
//
// 机制对照 / Mechanism mapping:
//  - 上游 ActorReference 的 Lazy initDict → 装载帧存活的 unique_ptr
//    vector(CreateActor 同步消费后释放 —— GC 的显式等价)
//    Upstream's Lazy initDict → a load-frame-lived unique_ptr vector
//    (released after CreateActor's synchronous consumption — the
//    explicit GC equivalent).
//  - IPreventMapSpawn 上游 mods 无实现者 → 空集直通
//    IPreventMapSpawn has no upstream mod implementors → an empty-set
//    pass-through.
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// SpawnMapActors(L27-73):开局摆位(IWorldLoaded)
/// SpawnMapActors (L27-73: the opening placement, IWorldLoaded).
class SpawnMapActors final : public sim::TraitBase,
                             public sim::IWorldLoaded {
 public:
  ORA_TRAIT_INTERFACES(SpawnMapActors,
                       OpenRA_Mods_Common_Traits_SpawnMapActors,
                       sim::IWorldLoaded)

  void WorldLoaded(sim::World& world,
                   gfx::WorldRenderer* wr) override;  // L33-62

  const std::map<std::string, sim::Actor*>& Actors() const {
    return map_actors_;
  }
  std::uint32_t LastMapActorID() const { return uint4_last_map_actor_id_; }

 private:
  std::map<std::string, sim::Actor*> map_actors_;  // L29(插入序遍历面)
  std::uint32_t uint4_last_map_actor_id_ = 0;      // L30
};

/// InitRegistry 的装配(引擎/测试装配入口各调一次;Location/Owner/Facing/
/// SubCell/TurretFacing/Health 六 init —— ra 地图全集)
/// InitRegistry's assembly (called once per engine/test assembly; the
/// six inits Location/Owner/Facing/SubCell/TurretFacing/Health — the
/// full ra-map set).
void RegisterCommonActorInits();

}  // namespace ora::mods
