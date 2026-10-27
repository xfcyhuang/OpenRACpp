import std;
#include "type_registry.hpp"

namespace ora::meta {

namespace {

struct RegistryEntry {
  RecordDesc desc;
  std::function<std::unique_ptr<RecordObject>()> fn_factory;
};

std::string_view Intern(std::string_view sv) {
  static std::deque<std::string> deque_pool;
  return *deque_pool.emplace(deque_pool.end(), sv);
}

std::unordered_map<std::string_view, RegistryEntry>& Table() {
  static std::unordered_map<std::string_view, RegistryEntry> map_table;
  return map_table;
}

}

void TypeRegistry::Register(RecordDesc desc,
                            std::function<std::unique_ptr<RecordObject>()> fn_factory) {
  RegistryEntry entry{.desc = std::move(desc), .fn_factory = std::move(fn_factory)};
  Table().insert_or_assign(Intern(entry.desc.str_name), entry);
  if (!entry.desc.str_full_name.empty())
    Table().insert_or_assign(Intern(entry.desc.str_full_name), std::move(entry));
}

const RecordDesc* TypeRegistry::FindType(std::string_view str_class_name) {
  const auto it_find = Table().find(str_class_name);
  return it_find != Table().end() ? &it_find->second.desc : nullptr;
}

std::unique_ptr<RecordObject> TypeRegistry::CreateObject(std::string_view str_class_name) {
  const auto it_find = Table().find(str_class_name);
  if (it_find == Table().end())
    return nullptr;
  return it_find->second.fn_factory();
}

void TypeRegistry::ClearForTest() {
  Table().clear();
}

std::vector<std::string_view> TypeRegistry::AllTypeNames() {
  std::vector<std::string_view> vec_names;
  for (const auto& [str_name, entry] : Table()) {
    (void)entry;
    vec_names.push_back(str_name);
  }
  std::ranges::sort(vec_names);
  return vec_names;
}

}
