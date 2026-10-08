// UPSTREAM: OpenRA.Game/Network/SyncReport.cs @b6fc03f L23-344(实现部分;
//          头注见 sync_report.hpp)
//          The implementation half of SyncReport.cs L23-344 (the header
//          note lives in sync_report.hpp).
#include "net/sync_report.hpp"

#include "game/actor_info.hpp"
#include "net/order_manager.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::net {

namespace {

/// TypeInfo 缓存的等价:类型全名 → dump 函数(手写 trait 按批注册)
/// The TypeInfo-cache equivalent: the type full name → the dump function
/// (registered by hand-written traits batch by batch).
std::map<std::string, SyncReport::SyncTraitDumper>& SyncTraitDumpers() {
  static std::map<std::string, SyncReport::SyncTraitDumper> map_dumpers;
  return map_dumpers;
}

/// trait 的 [VerifySync] 成员名表(gen/sync_gen 注册表 → 名字序列)
/// The trait's [VerifySync] member-name table (the gen/sync_gen registry
/// → the name sequence).
std::vector<std::string_view> MemberNamesOf(std::string_view str_full_name) {
  const std::span<const sim::SyncMemberDesc> vec_members =
      sim::FindSyncMembers(str_full_name);
  std::vector<std::string_view> vec_names;
  vec_names.reserve(vec_members.size());
  for (const auto& member : vec_members)
    vec_names.push_back(member.str_name);
  return vec_names;
}

/// TypeId → 上游 GetType().Name 的短名
/// TypeId → upstream's GetType().Name short name.
std::string ShortNameOf(gen::TypeId id_type) {
  const std::string_view str_full{
      gen::kTypeFullNames[static_cast<std::size_t>(id_type)]};
  const std::size_t sz_dot = str_full.rfind('.');
  return sz_dot != std::string_view::npos
             ? std::string{str_full.substr(sz_dot + 1)}
             : std::string{str_full};
}

/// TypeId → 全名 | TypeId → the full name.
std::string FullNameOf(gen::TypeId id_type) {
  return std::string{
      gen::kTypeFullNames[static_cast<std::size_t>(id_type)]};
}

}  // namespace

void SyncReport::RegisterSyncTraitDumper(std::string str_type_full_name,
                                         SyncTraitDumper fn_dumper) {
  SyncTraitDumpers()[std::move(str_type_full_name)] = std::move(fn_dumper);
}

SyncReport::SyncReport(OrderManager& order_manager)
    : order_manager_{order_manager} {
  // L49-54
  vec_sync_reports_.resize(kNumSyncReports);
}

void SyncReport::UpdateSyncReport(
    const std::vector<sim::Actor*>& vec_actors_with_sync,
    const std::vector<sim::ISync*>& vec_synced_effects) {
  // L56-60
  GenerateSyncReport(vec_sync_reports_[static_cast<std::size_t>(int4_cur_index_)],
                     vec_actors_with_sync, vec_synced_effects);
  int4_cur_index_ = (int4_cur_index_ + 1) % kNumSyncReports;
}

void SyncReport::GenerateSyncReport(
    Report& report,
    const std::vector<sim::Actor*>& vec_actors_with_sync,
    const std::vector<sim::ISync*>& vec_synced_effects) {
  // L62-105
  sim::World* ptr_world = order_manager_.World();
  report.int4_frame = order_manager_.NetFrameNumber();
  report.int4_synced_random =
      ptr_world != nullptr ? ptr_world->SharedRandom().Last : 0;
  report.int4_total_count =
      ptr_world != nullptr ? ptr_world->SharedRandom().TotalCount : 0;
  report.vec_traits.clear();
  report.vec_effects.clear();

  for (sim::Actor* actor : vec_actors_with_sync) {
    for (const auto& sync_hash : actor->SyncHashes()) {
      const int int4_hash = sync_hash.Hash();
      if (int4_hash == 0)
        continue;

      TraitReport trait_report;
      trait_report.uint4_actor_id = actor->ActorID();
      trait_report.str_type = actor->Info() != nullptr
                                  ? actor->Info()->Name()
                                  : std::string{};
      trait_report.str_owner = actor->Owner() != nullptr
                                   ? actor->Owner()->PlayerName()
                                   : std::string{"null"};
      trait_report.str_trait = ShortNameOf(sync_hash.type_id);
      trait_report.int4_hash = int4_hash;

      // DumpSyncTrait:注册 dumper 的逐成员值;未注册 = 名字表空值
      // DumpSyncTrait: the registered dumper's member values; an
      // unregistered type carries the name table with empty values.
      const std::string str_full = FullNameOf(sync_hash.type_id);
      auto& map_dumpers = SyncTraitDumpers();
      if (const auto it = map_dumpers.find(str_full);
          it != map_dumpers.end()) {
        trait_report.vec_names_values = it->second(sync_hash.trait);
      } else {
        for (const auto& str_name : MemberNamesOf(str_full))
          trait_report.vec_names_values.push_back(
              MemberValue{std::string{str_name}, std::string{}, false});
      }

      report.vec_traits.push_back(std::move(trait_report));
    }
  }

  for (sim::ISync* sync : vec_synced_effects) {
    const int int4_hash =
        ptr_world != nullptr ? ptr_world->SyncEffectHash(sync) : 0;
    if (int4_hash == 0)
      continue;

    // ISync effect 的具体类型名 = 反射域(C++ 侧名字空串 —— 起步偏离,
    // D30 扩展:效果 dumper 键按需接入)
    // The ISync effect's concrete type name = a reflection domain (an
    // empty name on the C++ side — the starter deviation, extending D30:
    // the effect-dumper keys wire in on demand).
    EffectReport effect_report;
    effect_report.int4_hash = int4_hash;

    report.vec_effects.push_back(std::move(effect_report));
  }
}

void SyncReport::DumpSyncReport(
    int frame, const std::function<void(std::string_view)>& fn_sink) {
  // L107-164(头部 Player/Game ID 行依赖 Game/Platform 的全局面 —— 引擎
  // 装配批接入前的省略登记)
  // L107-164 (the header Player/Game-ID lines depend on the Game/Platform
  // globals — the omission is registered until the engine-assembly batch).
  std::vector<int> vec_recorded_frames;
  bool b_desync_frame_found = false;
  for (const auto& r : vec_sync_reports_) {
    vec_recorded_frames.push_back(r.int4_frame);
    if (r.int4_frame == frame) {
      b_desync_frame_found = true;
      fn_sink(std::format("Sync for net frame {} -------------", r.int4_frame));
      fn_sink(std::format("SharedRandom: {} (#{})", r.int4_synced_random,
                          r.int4_total_count));
      fn_sink("Synced Traits:");
      for (const auto& a : r.vec_traits) {
        fn_sink(std::format("\t {} {} {} {} ({})", a.uint4_actor_id,
                            a.str_type, a.str_owner, a.str_trait,
                            a.int4_hash));

        for (const auto& member : a.vec_names_values)
          if (member.b_has_value)
            fn_sink(std::format("\t\t {}: {}", member.str_name,
                                member.str_value));
      }

      fn_sink("Synced Effects:");
      for (const auto& e : r.vec_effects) {
        fn_sink(std::format("\t {} ({})", e.str_name, e.int4_hash));

        for (const auto& member : e.vec_names_values)
          if (member.b_has_value)
            fn_sink(std::format("\t\t {}: {}", member.str_name,
                                member.str_value));
      }
    }
  }

  fn_sink("Sync Report System Info:");
  fn_sink(std::format("Out of sync frame: {}", frame));
  std::string str_recorded;
  for (std::size_t i = 0; i < vec_recorded_frames.size(); ++i) {
    if (i != 0)
      str_recorded += ",";
    str_recorded += std::to_string(vec_recorded_frames[i]);
  }
  fn_sink(std::format("Recorded frames: {}", str_recorded));

  if (!b_desync_frame_found)
    fn_sink(std::format(
        "Recorded frames do not contain the frame {}. No sync report "
        "available!",
        frame));
}

std::vector<int> SyncReport::RecordedFrames() const {
  std::vector<int> vec_frames;
  vec_frames.reserve(vec_sync_reports_.size());
  for (const auto& r : vec_sync_reports_)
    vec_frames.push_back(r.int4_frame);
  return vec_frames;
}

}  // namespace ora::net
