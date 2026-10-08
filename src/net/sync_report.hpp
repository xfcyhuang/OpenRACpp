// UPSTREAM: OpenRA.Game/Network/SyncReport.cs @b6fc03f L23-344(起步承载
//           面:环形 7 报告 + UpdateSyncReport 的哈希/帧/随机态记账 +
//           DumpSyncReport 的文本面)
//           The starter face of SyncReport.cs L23-344 (the 7-report ring +
//           UpdateSyncReport's hash/frame/random bookkeeping + the
//           DumpSyncReport text face).
//
// 机制对照 / Mechanism mapping:
//  - TypeInfo 的 Reflection.Emit 成员值快照(DumpSyncTrait 的逐成员
//    values)→ 可选 dump 注册表(手写 trait 按批注册;未注册类型 = 名字
//    表空值 —— 起步偏离,D30 扩展)
//    TypeInfo's Reflection.Emit member-value snapshots (DumpSyncTrait's
//    per-member values) → an optional dump registry (hand-written traits
//    register batch by batch; unregistered types carry the name table with
//    empty values — the starter deviation, extending D30).
//  - Log.AddChannel("sync")/Log.Write → 调用方注入的 sink(std::print 的
//    后端随日志设施批)
//    Log.AddChannel("sync")/Log.Write → the caller-injected sink (the
//    std::print backend rides the logging-facility batch).
//  - Values 的内联四槽优化 → vector(报告生成是罕见路径,分配无关紧要)
//    Values' inline-four-slot optimization → a vector (report generation
//    is a rare path; the allocation is immaterial).
#pragma once
import std;

#include "sim/sync_hash.hpp"

namespace ora::sim {
class World;
}

namespace ora::net {

class OrderManager;

/// SyncReport(SyncReport.cs L25-342)
/// SyncReport (SyncReport.cs L25-342).
class SyncReport {
 public:
  static constexpr int kNumSyncReports = 7;  // L25

  /// 一个成员的值快照(名 + 文本值;未注册 dumper = 空值)
  /// One member's value snapshot (the name + the text value; an
  /// unregistered dumper yields the empty value).
  struct MemberValue {
    std::string str_name;
    std::string str_value;
    bool b_has_value = false;
  };

  /// TypeInfo 的 dump 面:类型全名 → 成员名值序列(手写 trait 注册)
  /// TypeInfo's dump face: the type full name → the member name/value
  /// sequence (registered by hand-written traits).
  using SyncTraitDumper =
      std::function<std::vector<MemberValue>(const sim::ISync*)>;
  static void RegisterSyncTraitDumper(std::string str_type_full_name,
                                      SyncTraitDumper fn_dumper);

  explicit SyncReport(OrderManager& order_manager);

  /// UpdateSyncReport(L56-60;OrderManager L279 的挂点)
  /// UpdateSyncReport (L56-60; the OrderManager L279 mount).
  void UpdateSyncReport(
      const std::vector<sim::Actor*>& vec_actors_with_sync,
      const std::vector<sim::ISync*>& vec_synced_effects);

  /// DumpSyncReport(L107-164):帧命中报告 → sink 文本
  /// DumpSyncReport (L107-164): the frame-hit report → sink text.
  void DumpSyncReport(int frame, const std::function<void(std::string_view)>& fn_sink);

  /// 测试面:记录过的帧集
  /// The test face: the recorded frames.
  std::vector<int> RecordedFrames() const;

 private:
  /// (string[] Names, Values Values) 的报告形态
  /// The report shape of (string[] Names, Values Values).
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

}  // namespace ora::net
