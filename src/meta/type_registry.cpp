// UPSTREAM: OpenRA.Game/ObjectCreator.cs @7d57605 L21-168(type_registry.hpp 的实现)
//          Implementation of type_registry.hpp.
import std;
#include "type_registry.hpp"

namespace ora::meta {

namespace {

struct RegistryEntry {
  RecordDesc desc;
  std::function<std::unique_ptr<RecordObject>()> fn_factory;
};

std::unordered_map<std::string_view, RegistryEntry>& Table() {
  static std::unordered_map<std::string_view, RegistryEntry> map_table;
  return map_table;
}

}  // namespace

void TypeRegistry::Register(RecordDesc desc,
                            std::function<std::unique_ptr<RecordObject>()> fn_factory) {
  RegistryEntry entry{.desc = std::move(desc), .fn_factory = std::move(fn_factory)};
  Table().insert_or_assign(entry.desc.str_name, std::move(entry));
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
  for (const auto& [str_name, entry] : Table())
    vec_names.push_back(str_name);
  std::ranges::sort(vec_names);
  return vec_names;
}

}  // namespace ora::meta
