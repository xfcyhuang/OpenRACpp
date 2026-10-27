#include "net/sync_report.hpp"

#include "game/actor_info.hpp"
#include "net/order_manager.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::net {

namespace {

std::map<std::string, SyncReport::SyncTraitDumper>& SyncTraitDumpers() {
  static std::map<std::string, SyncReport::SyncTraitDumper> map_dumpers;
  return map_dumpers;
}

std::vector<std::string_view> MemberNamesOf(std::string_view str_full_name) {
  const std::span<const sim::SyncMemberDesc> vec_members =
      sim::FindSyncMembers(str_full_name);
  std::vector<std::string_view> vec_names;
  vec_names.reserve(vec_members.size());
  for (const auto& member : vec_members)
    vec_names.push_back(member.str_name);
  return vec_names;
}

std::string ShortNameOf(gen::TypeId id_type) {
  const std::string_view str_full{
      gen::kTypeFullNames[static_cast<std::size_t>(id_type)]};
  const std::size_t sz_dot = str_full.rfind('.');
  return sz_dot != std::string_view::npos
             ? std::string{str_full.substr(sz_dot + 1)}
             : std::string{str_full};
}

std::string FullNameOf(gen::TypeId id_type) {
  return std::string{
      gen::kTypeFullNames[static_cast<std::size_t>(id_type)]};
}

}

void SyncReport::RegisterSyncTraitDumper(std::string str_type_full_name,
                                         SyncTraitDumper fn_dumper) {
  SyncTraitDumpers()[std::move(str_type_full_name)] = std::move(fn_dumper);
}

SyncReport::SyncReport(OrderManager& order_manager)
    : order_manager_{order_manager} {

  vec_sync_reports_.resize(kNumSyncReports);
}

void SyncReport::UpdateSyncReport(
    const std::vector<sim::Actor*>& vec_actors_with_sync,
    const std::vector<sim::ISync*>& vec_synced_effects) {

  GenerateSyncReport(vec_sync_reports_[static_cast<std::size_t>(int4_cur_index_)],
                     vec_actors_with_sync, vec_synced_effects);
  int4_cur_index_ = (int4_cur_index_ + 1) % kNumSyncReports;
}

void SyncReport::GenerateSyncReport(
    Report& report,
    const std::vector<sim::Actor*>& vec_actors_with_sync,
    const std::vector<sim::ISync*>& vec_synced_effects) {

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

    EffectReport effect_report;
    effect_report.int4_hash = int4_hash;

    report.vec_effects.push_back(std::move(effect_report));
  }
}

void SyncReport::DumpSyncReport(
    int frame, const std::function<void(std::string_view)>& fn_sink) {

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

}
