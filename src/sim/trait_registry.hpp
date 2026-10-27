#pragma once
import std;

#include "core/arena.hpp"
#include "meta/generic_record.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::sim {

class ActorInitializer;
class TraitBase;

using TraitCreateFn = std::function<TraitBase*(
    const meta::RecordObject& rec_info, ActorInitializer& init,
    ora::WorldArena& arena)>;

class TraitRegistry {
 public:
  static TraitRegistry& Instance();

  void Register(std::string str_name, TraitCreateFn fn_factory);

  TraitBase* Create(const std::string& str_name,
                    const meta::RecordObject& rec_info,
                    ActorInitializer& init, ora::WorldArena& arena) const;
  [[noreturn]] static void ThrowCannotLocate(const std::string& str_name);

  bool Contains(std::string_view str_name) const;

 private:
  std::vector<std::pair<std::string, TraitCreateFn>> vec_entries_;
};

void RegisterWorldTraits();

const meta::RecordObject* FindTraitInfoOfInterface(
    const game::ActorInfo& info, std::string_view str_interface);

std::optional<std::string_view> RecordFieldString(const meta::RecordObject& rec,
                                                  std::string_view str_name);

std::optional<std::int64_t> RecordFieldInt(const meta::RecordObject& rec,
                                           std::string_view str_name);

const meta::GenericValue* RecordFieldValue(const meta::RecordObject& rec,
                                           std::string_view str_name);

}
