// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Refinery.cs @b6fc03fc
//          L19-110 全文(逐语义重写;Requires<WithSpriteBodyInfo> 的
//          约束面 = WithSpriteBody 未移植的注册侧跳过 —— COVERAGE 登记;
//          FloatingText 的 ShowTicks 显示面随 Phase 6)
//          The whole of Buildings/Refinery.cs L19-110 (a verbatim-semantics
//          rewrite; the Requires<WithSpriteBodyInfo> constraint face = the
//          registration-side skip with WithSpriteBody unported — registered
//          in COVERAGE; the ShowTicks FloatingText display face rides
//          Phase 6).
#pragma once
import std;

#include "core/percent_modifiers.hpp"
#include "mods/player_resources.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Player;
using sim::TraitBase;

/// RefineryInfo(L21-33)的解析面
/// The parsed face of RefineryInfo (L21-33).
struct RefineryInfoData {
  bool b_use_storage = true;            // L24
  bool b_discard_excess_resources = false;  // L27
  bool b_show_ticks = true;             // L29
  int int4_tick_rate = 10;              // L30

  static RefineryInfoData Parse(const meta::RecordObject& rec_info);
};

/// Refinery(L35-109;IAcceptResources)
/// Refinery (L35-109; IAcceptResources).
class Refinery final : public TraitBase,
                       public sim::IAcceptResources,
                       public sim::INotifyCreated,
                       public sim::ITick,
                       public sim::INotifyOwnerChanged {
 public:
  Refinery(ActorInitializer& init, const RefineryInfoData& info);

  ORA_TRAIT_INTERFACES(Refinery, OpenRA_Mods_Common_Traits_Refinery,
                       sim::IAcceptResources, sim::INotifyCreated,
                       sim::ITick, sim::INotifyOwnerChanged)

  // ———— IAcceptResources(L55-91)————
  // ———— IAcceptResources (L55-91) ————
  int AcceptResources(Actor& self, const std::string& resource_type,
                      int count = 1) override;

  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

 private:
  RefineryInfoData info_;
  PlayerResources* p_player_resources_;
  std::vector<int> vec_resource_value_modifiers_;  // Created 一次物化
                                                   // (the one-shot
                                                   // Created materialization)

  int int4_current_display_tick_ = 0;
  int int4_current_display_value_ = 0;
};

}  // namespace ora::mods
