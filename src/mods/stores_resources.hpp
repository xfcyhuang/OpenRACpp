// UPSTREAM: OpenRA.Mods.Common/Traits/StoresResources.cs @b6fc03f L18-107
//          全文 + StoresPlayerResources.cs L14-67 全文(逐语义重写)
//          The whole of StoresResources.cs L18-107 + the whole of
//          StoresPlayerResources.cs L14-67 (verbatim-semantics rewrites).
//
// 机制对照 / Mechanism mapping:
//  - contents 的 Dictionary<string, int> → 插入序 vector<pair>(构造序 =
//    Info.Resources 声明序;ContentHash 的键长位移照抄)
//    contents' Dictionary<string, int> → an insertion-ordered vector<pair>
//    (the construct order = Info.Resources' declaration order; the
//    ContentHash's key-length shift copied).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

class PlayerResources;

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

/// StoresResourcesInfo(L20-33)的解析面
/// The parsed face of StoresResourcesInfo (L20-33).
struct StoresResourcesInfoData : sim::IStoresResourcesInfo {
  int int4_capacity = 28;               // L24([FieldLoader.Require])
  std::vector<std::string> vec_resources;  // L27

  static StoresResourcesInfoData Parse(const meta::RecordObject& rec_info);

  // ———— IStoresResourcesInfo(L29)————
  // ———— IStoresResourcesInfo (L29) ————
  std::vector<std::string> ResourceTypes() override {
    return vec_resources;
  }
};

/// StoresResources(L34-106;ISync {ContentHash})
/// StoresResources (L34-106; ISync {ContentHash}).
class StoresResources final : public TraitBase,
                              public sim::IStoresResources,
                              public sim::ISync {
 public:
  StoresResources(ActorInitializer& init,
                  const StoresResourcesInfoData& info);

  ORA_TRAIT_INTERFACES(StoresResources,
                       OpenRA_Mods_Common_Traits_StoresResources,
                       sim::IStoresResources, sim::ISync)

  /// L39-50:[VerifySync] ContentHash(Σ value << key.Length)
  /// L39-50: the [VerifySync] ContentHash (Σ value << key.Length).
  int ContentHash() const;

  // ———— IStoresResources ————
  bool HasType(const std::string& resource_type) override;
  int Capacity() const override { return info_.int4_capacity; }
  const std::vector<std::pair<std::string, int>>& Contents()
      const override {
    return vec_contents_;
  }
  int ContentsSum() const override { return int4_contents_sum_; }
  int AddResource(const std::string& resource_type, int value) override;
  int RemoveResource(const std::string& resource_type,
                     int value) override;

 private:
  /// 按名取桶(未含 = nullptr)
  /// The bucket by name (nullptr when absent).
  int* Bucket(const std::string& resource_type);
  const int* Bucket(const std::string& resource_type) const;

  StoresResourcesInfoData info_;
  std::vector<std::pair<std::string, int>> vec_contents_;
  int int4_contents_sum_ = 0;  // L52
};

/// StoresPlayerResourcesInfo(L17-24)的解析面
/// The parsed face of StoresPlayerResourcesInfo (L17-24).
struct StoresPlayerResourcesInfoData {
  int int4_capacity = 0;  // L21([FieldLoader.Require])

  static StoresPlayerResourcesInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// StoresPlayerResources(L25-66)
class StoresPlayerResources final
    : public TraitBase,
      public sim::INotifyOwnerChanged,
      public sim::INotifyCapture,
      public sim::INotifyKilled,
      public sim::INotifyAddedToWorld,
      public sim::INotifyRemovedFromWorld {
 public:
  StoresPlayerResources(ActorInitializer& init,
                        const StoresPlayerResourcesInfoData& info);

  ORA_TRAIT_INTERFACES(StoresPlayerResources,
                       OpenRA_Mods_Common_Traits_StoresPlayerResources,
                       sim::INotifyOwnerChanged, sim::INotifyCapture,
                       sim::INotifyKilled, sim::INotifyAddedToWorld,
                       sim::INotifyRemovedFromWorld)

  /// L30:Stored(容量占比换算)
  /// L30: Stored (the capacity-proportional share).
  int Stored() const;

  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;
  void OnCapture(Actor& self, Actor& captor, Player& old_owner,
                 Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types)
      override;
  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;

  const StoresPlayerResourcesInfoData& Info() const { return info_; }

 private:
  StoresPlayerResourcesInfoData info_;
  PlayerResources* p_player_ = nullptr;
};

}  // namespace ora::mods
