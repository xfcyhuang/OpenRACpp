#include "sim/sync_hash.hpp"

namespace ora::sim {

namespace {

std::map<std::string_view, std::span<const SyncMemberDesc>>& SyncMembers() {
  static std::map<std::string_view, std::span<const SyncMemberDesc>> registry;
  return registry;
}

std::map<std::string_view, SyncHashFn>& SyncHashFunctions() {
  static std::map<std::string_view, SyncHashFn> registry;
  return registry;
}
}

void RegisterSyncMembers(std::string_view str_type_full_name,
                         std::span<const SyncMemberDesc> members) {
  SyncMembers()[str_type_full_name] = members;
}

std::span<const SyncMemberDesc> FindSyncMembers(
    std::string_view str_type_full_name) {
  auto it = SyncMembers().find(str_type_full_name);
  return it == SyncMembers().end() ? std::span<const SyncMemberDesc>{}
                                   : it->second;
}

void RegisterSyncHashFunction(std::string_view str_type_full_name,
                              SyncHashFn fn) {
  SyncHashFunctions()[str_type_full_name] = fn;
}

SyncHashFn FindSyncHashFunctionByName(std::string_view str_type_full_name) {
  auto it = SyncHashFunctions().find(str_type_full_name);
  return it == SyncHashFunctions().end() ? nullptr : it->second;
}

SyncHashFn FindSyncHashFunction(gen::TypeId trait_type_id) {
  const auto v = static_cast<std::size_t>(trait_type_id);
  if (v >= gen::kTypeIdCount)
    return nullptr;
  return FindSyncHashFunctionByName(gen::kTypeFullNames[v]);
}

namespace {
int g_unsync_count = 0;
}

int UnsyncCountForTest() { return g_unsync_count; }

int& UnsyncCountRef() { return g_unsync_count; }

void AssertUnsynced(std::string_view message) {

  if (g_unsync_count == 0)
    throw std::runtime_error(std::string(message));
}

}
