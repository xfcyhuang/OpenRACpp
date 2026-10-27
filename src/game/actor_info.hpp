#pragma once
import std;

#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

class ActorInfo final {
 public:
  static constexpr char kAbstractActorPrefix = '^';
  static constexpr char kTraitInstanceSeparator = '@';

  ActorInfo(std::string str_name, const yaml::MiniYaml& node);

  explicit ActorInfo(std::string str_name);

  const std::string& Name() const { return str_name_; }

  const std::vector<std::unique_ptr<meta::RecordObject>>& Traits() const {
    return vec_traits_;
  }
  const std::vector<std::string>& TraitInstanceNames() const {
    return vec_instanceNames_;
  }

  std::vector<const meta::RecordObject*> TraitInfosByInterface(
      std::string_view str_interface) const;

  std::string_view InstanceNameOf(const meta::RecordObject* rec) const;

  bool HasTraitInfoOfInterface(std::string_view str_interface) const;

  std::vector<const meta::RecordObject*> TraitsInConstructOrder() const;

 private:

  static std::unique_ptr<meta::RecordObject> LoadTraitInfo(
      std::string_view sv_trait_name, const yaml::MiniYaml& my,
      std::string& str_instance_name_out);

  std::string str_name_;
  std::vector<std::unique_ptr<meta::RecordObject>> vec_traits_;
  std::vector<std::string> vec_instanceNames_;
};

}
