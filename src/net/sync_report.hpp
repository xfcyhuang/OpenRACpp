#pragma once
import std;

#include "sim/sync_hash.hpp"

namespace ora::sim {
class World;
}

namespace ora::net {

class OrderManager;

class SyncReport {
 public:
  static constexpr int kNumSyncReports = 7;

  struct MemberValue {
    std::string str_name;
    std::string str_value;
    bool b_has_value = false;
  };

  using SyncTraitDumper =
      std::function<std::vector<MemberValue>(const sim::ISync*)>;
  static void RegisterSyncTraitDumper(std::string str_type_full_name,
                                      SyncTraitDumper fn_dumper);

  explicit SyncReport(OrderManager& order_manager);

  void UpdateSyncReport(
      const std::vector<sim::Actor*>& vec_actors_with_sync,
      const std::vector<sim::ISync*>& vec_synced_effects);

  void DumpSyncReport(int frame, const std::function<void(std::string_view)>& fn_sink);

  std::vector<int> RecordedFrames() const;

 private:

  struct TraitReport {
    std::uint32_t uint4_actor_id = 0;
    std::string str_type;
    std::string str_owner;
    std::string str_trait;
    int int4_hash = 0;
    std::vector<MemberValue> vec_names_values;
  };

  struct EffectReport {
    std::string str_name;
    int int4_hash = 0;
    std::vector<MemberValue> vec_names_values;
  };

  struct Report {
    int int4_frame = 0;
    int int4_synced_random = 0;
    int int4_total_count = 0;
    std::vector<TraitReport> vec_traits;
    std::vector<EffectReport> vec_effects;
  };

  void GenerateSyncReport(
      Report& report,
      const std::vector<sim::Actor*>& vec_actors_with_sync,
      const std::vector<sim::ISync*>& vec_synced_effects);

  OrderManager& order_manager_;
  std::vector<Report> vec_sync_reports_;
  int int4_cur_index_ = 0;
};

}
