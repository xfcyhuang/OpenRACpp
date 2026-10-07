// UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerResources.cs @b6fc03f
//          L17-264 全文(InsufficientFunds 的声音/文本通知随注入面;
//          ILobbyOptions 的 startingcash 选项声明随 Phase 7 lobby)
//          The whole of PlayerResources.cs L17-264 (the
//          InsufficientFunds sound/text notifications ride the injection
//          face; the ILobbyOptions startingcash declaration rides
//          Phase 7's lobby).
//
// 机制对照 / Mechanism mapping:
//  - GiveCash 的 C# checked 溢出捕获 → __builtin_add_overflow 的饱和
//    语义(int.MaxValue 钳位;行为等价)
//    GiveCash's C# checked-overflow catch → __builtin_add_overflow's
//    saturating semantics (the int.MaxValue clamp; behaviorally
//    equivalent).
//  - startingcash 的 LobbyInfo 读 = Shroud::Created 同形(Session 最小
//    承载面的 "True"/"False" 字典;int.TryParse 语义 = from_chars 全串
//    解析,失败回落 DefaultCash)
//    The startingcash LobbyInfo read takes Shroud::Created's same shape
//    (the Session minimal carrier's dictionary; int.TryParse semantics =
//    a full-string from_chars, falling back to DefaultCash).
#pragma once
import std;

#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::mods {

/// PlayerResourcesInfo(L19-105)的解析面(ILobbyOptions 声明面省)
/// The parse face of PlayerResourcesInfo (the ILobbyOptions declaration
/// face omitted).
struct PlayerResourcesInfoData {
  std::vector<std::int32_t> vec_selectable_cash{2500, 5000, 10000,
                                                20000};  // L29
  int int4_default_cash = 5000;                           // L32
  int int4_insufficient_funds_notification_interval = 30000;  // L51
  std::vector<std::pair<std::string, int>> vec_resource_values;  // L63

  static PlayerResourcesInfoData Parse(const meta::RecordObject& rec_info);
};

/// PlayerResources(L107-264;ISync {Cash, Resources, ResourceCapacity})
class PlayerResources final : public sim::TraitBase,
                              public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(
      PlayerResources, OpenRA_Mods_Common_Traits_PlayerResources,
      sim::ISync)

  PlayerResources(sim::ActorInitializer& init,
                  PlayerResourcesInfoData info);

  // [VerifySync] L84-90
  int Cash = 0;
  int Resources = 0;
  int ResourceCapacity = 0;

  int Earned = 0;  // L92
  int Spent = 0;   // L93

  /// L95-107:ChangeCash
  /// L95-107: ChangeCash.
  int ChangeCash(int amount);

  /// L109-111:CanGiveResources
  /// L109-111: CanGiveResources.
  bool CanGiveResources(int amount) const;

  /// L113-131:GiveResources / RefundResources
  void GiveResources(int num, bool is_refund = false);
  void RefundResources(int num) { GiveResources(num, true); }  // L133-135

  /// L137-141:TakeResources
  /// L137-141: TakeResources.
  bool TakeResources(int num);

  /// L143-174:GiveCash / RefundCash(checked 溢出 → 饱和)
  /// L143-174: GiveCash / RefundCash (checked overflow → saturation).
  void GiveCash(int num, bool is_refund = false);
  void RefundCash(int num) { GiveCash(num, true); }  // L176-178

  /// L180-201:TakeCash(低资通知随注入面)
  /// L180-201: TakeCash (the low-funds notification rides the injection
  /// face).
  bool TakeCash(int num, bool notify_low_funds = false);

  /// L203-205:AddStorageCapacity | L207-214:RemoveStorageCapacity
  void AddStorageCapacity(int capacity);
  void RemoveStorageCapacity(int capacity);

  /// L216-218:GetCashAndResources
  /// L216-218: GetCashAndResources.
  int GetCashAndResources() const { return Cash + Resources; }

  const PlayerResourcesInfoData& Info() const { return info_; }

 private:
  PlayerResourcesInfoData info_;
  sim::Player* p_owner_ = nullptr;
  long long int8_last_notification_time = 0;  // Game.RunTime 注入面缺省
};

}  // namespace ora::mods
