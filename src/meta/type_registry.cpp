// UPSTREAM: OpenRA.Game/ObjectCreator.cs @b6fc03f L21-168(type_registry.hpp 的实现)
//          Implementation of type_registry.hpp.
//
// 双键注册 / Dual-key registration:
//  - 简单名键(ObjectCreator.FindType 的 Namespace+"."+className 匹配面;
//    同名类后注册覆盖 —— 与上游"首个命名空间命中"近似,冲突清单在
//    schema_dumper --gen 的警告输出)
//    The simple-name key (the Namespace+"."+className match surface of
//    ObjectCreator.FindType; same-named classes overwrite on later
//    registration — approximating the upstream first-namespace hit, with the
//    conflict list in the schema_dumper --gen warnings).
//  - 全名键(str_full_name;嵌套同名类如两族 ResourceTypeInfo 的精确寻址,
//    loader 侧使用)
//    The full-name key (str_full_name; exact addressing of nested same-named
//    classes like the two ResourceTypeInfo families, used by loaders).
import std;
#include "type_registry.hpp"

namespace ora::meta {

namespace {

struct RegistryEntry {
  RecordDesc desc;
  std::function<std::unique_ptr<RecordObject>()> fn_factory;
};

/// 注册键驻留池(调用方常传临时串视图;池化后 string_view 永不悬垂)
/// The intern pool of registry keys (callers often pass views over
/// temporaries; interning keeps the string_views alive forever).
std::string_view Intern(std::string_view sv) {
  static std::deque<std::string> deque_pool;
  return *deque_pool.emplace(deque_pool.end(), sv);
}

std::unordered_map<std::string_view, RegistryEntry>& Table() {
  static std::unordered_map<std::string_view, RegistryEntry> map_table;
  return map_table;
}

}  // namespace

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

}  // namespace ora::meta
