// UPSTREAM: OpenRA.Game/ObjectCreator.cs @b6fc03f(trait_registry.hpp 的实现)
//          The implementation of trait_registry.hpp.
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态

#include "sim/trait_registry.hpp"

#include "game/actor_info.hpp"
#include "meta/field_loader.hpp"
#include "sim/actor_init.hpp"
#include "sim/screen_map.hpp"
#include "sim/selection.hpp"

namespace ora::sim {

TraitRegistry& TraitRegistry::Instance() {
  static TraitRegistry registry;
  return registry;
}

void TraitRegistry::Register(std::string str_name, TraitCreateFn fn_factory) {
  // 重复注册 = 上游 ObjectCreator 的静态表同键覆盖语义不存在 —— 先到先得
  // (上游 FindType 取程序集内唯一;双注册属装配错误,断言承载)
  assert(!Contains(str_name));
  vec_entries_.emplace_back(std::move(str_name), std::move(fn_factory));
}

TraitBase* TraitRegistry::Create(const std::string& str_name,
                                 const meta::RecordObject& rec_info,
                                 ActorInitializer& init,
                                 ora::WorldArena& arena) const {
  for (const auto& [name, factory] : vec_entries_)
    if (name == str_name)
      return factory(rec_info, init, arena);
  return nullptr;  // 未注册 Info:随 trait 批补齐(见 hpp 注记)
}

void TraitRegistry::ThrowCannotLocate(const std::string& str_name) {
  throw std::runtime_error("Cannot locate type: " + str_name);
}

bool TraitRegistry::Contains(std::string_view str_name) const {
  return std::any_of(vec_entries_.begin(), vec_entries_.end(),
                     [&](const auto& e) { return e.first == str_name; });
}

// ———— 按名槽位读值(GeneratedRecord 的 CollectFields 序 = 槽位序)————

const meta::RecordObject* FindTraitInfoOfInterface(
    const game::ActorInfo& info, std::string_view str_interface) {
  // ActorInfo.TraitInfos<T> 的接口名过滤面(注册序首条)
  const auto vec = info.TraitInfosByInterface(str_interface);
  return vec.empty() ? nullptr : vec.front();
}

std::optional<std::string_view> RecordFieldString(
    const meta::RecordObject& rec, std::string_view str_name) {
  const auto* generated = dynamic_cast<const meta::GeneratedRecord*>(&rec);
  if (generated == nullptr)
    return std::nullopt;
  const std::vector<const meta::FieldDesc*> fields =
      meta::CollectFields(generated->record_desc());
  for (std::size_t i = 0; i < fields.size(); i++) {
    if (fields[i]->str_name != str_name)
      continue;
    const meta::GenericValue& v = generated->Slot(i);
    if (auto* s = std::get_if<std::string>(&v.val))
      return std::string_view{*s};
    return std::string_view{};
  }
  return std::nullopt;
}

std::optional<std::int64_t> RecordFieldInt(const meta::RecordObject& rec,
                                           std::string_view str_name) {
  const auto* generated = dynamic_cast<const meta::GeneratedRecord*>(&rec);
  if (generated == nullptr)
    return std::nullopt;
  const std::vector<const meta::FieldDesc*> fields =
      meta::CollectFields(generated->record_desc());
  for (std::size_t i = 0; i < fields.size(); i++) {
    if (fields[i]->str_name != str_name)
      continue;
    const meta::GenericValue& v = generated->Slot(i);
    if (auto* n = std::get_if<std::int64_t>(&v.val))
      return *n;
    return std::int64_t{0};
  }
  return std::nullopt;
}

void RegisterWorldTraits() {
  static const bool b_registered = [] {
    // ScreenMapInfo.Create(init) → new ScreenMap(init.World, this)
    TraitRegistry::Instance().Register(
        "ScreenMapInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          const std::int64_t bin_size =
              RecordFieldInt(rec_info, "BinSize").value_or(250);
          return arena.Create<ScreenMap>(init.Self().world(),
                                         static_cast<int>(bin_size));
        });

    // SelectionInfo.Create(init) → new Selection()
    TraitRegistry::Instance().Register(
        "SelectionInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Selection>();
        });

    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::sim
