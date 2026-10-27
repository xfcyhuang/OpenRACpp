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

struct PlayerResourcesInfoData {
  std::vector<std::int32_t> vec_selectable_cash{2500, 5000, 10000,
                                                20000};
  int int4_default_cash = 5000;
  int int4_insufficient_funds_notification_interval = 30000;
  std::string str_cash_tick_up_notification;
  std::string str_cash_tick_down_notification;
  std::vector<std::pair<std::string, int>> vec_resource_values;

  static PlayerResourcesInfoData Parse(const meta::RecordObject& rec_info);
};

class PlayerResources final : public sim::TraitBase,
                              public sim::ISync {
 public:
  ORA_TRAIT_INTERFACES(
      PlayerResources, OpenRA_Mods_Common_Traits_PlayerResources,
      sim::ISync)

  PlayerResources(sim::ActorInitializer& init,
                  PlayerResourcesInfoData info);

  int Cash = 0;
  int Resources = 0;
  int ResourceCapacity = 0;

  int Earned = 0;
  int Spent = 0;

  int ChangeCash(int amount);

  bool CanGiveResources(int amount) const;

  void GiveResources(int num, bool is_refund = false);
  void RefundResources(int num) { GiveResources(num, true); }

  bool TakeResources(int num);

  void GiveCash(int num, bool is_refund = false);
  void RefundCash(int num) { GiveCash(num, true); }

  bool TakeCash(int num, bool notify_low_funds = false);

  void AddStorageCapacity(int capacity);
  void RemoveStorageCapacity(int capacity);

  int GetCashAndResources() const { return Cash + Resources; }

  const PlayerResourcesInfoData& Info() const { return info_; }

 private:
  PlayerResourcesInfoData info_;
  sim::Player* p_owner_ = nullptr;
  long long int8_last_notification_time = 0;
};

}
