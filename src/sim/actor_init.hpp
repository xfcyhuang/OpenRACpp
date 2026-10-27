#pragma once
import std;

#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::sim {

class Actor;

template <class T>
T* FindInitByInstanceName(TypeDictionary& dict,
                          std::string_view info_instance_name) {
  auto inits = dict.WithInterface<T>();

  if (!info_instance_name.empty()) {
    T* named = nullptr;
    for (T* i : inits)
      if (i->InstanceName() == info_instance_name)
        named = i;
    if (named != nullptr)
      return named;
    T* unnamed = nullptr;
    for (T* i : inits)
      if (i->InstanceName().empty())
        unnamed = i;
    return unnamed;
  }

  T* unnamed = nullptr;
  for (T* i : inits)
    if (i->InstanceName().empty())
      unnamed = i;
  return unnamed;
}

class ActorInitializer {
 public:
  ActorInitializer(Actor& self, TypeDictionary& dict)
      : self_(self), dict_(dict) {}

  Actor& Self() { return self_; }
  Actor& Self() const { return self_; }
  TypeDictionary& Dict() { return dict_; }

  template <class T>
  T* GetOrDefault(std::string_view info_instance_name) const {
    return FindInitByInstanceName<T>(dict_, info_instance_name);
  }

  template <class T>
  T* Get(std::string_view info_instance_name) const {
    auto* init = GetOrDefault<T>(info_instance_name);
    if (init == nullptr)
      throw std::runtime_error(
          "TypeDictionary does not contain instance of type `" +
          std::string(ora::gen::kTypeFullNames[static_cast<std::size_t>(
              T::kTypeId)]) +
          "`");
    return init;
  }

  template <class T, class U>
  U GetValue(std::string_view info_instance_name) const {
    return Get<T>(info_instance_name)->Value();
  }

  template <class T, class U>
  U GetValue(std::string_view info_instance_name, U fallback) const {
    auto* init = GetOrDefault<T>(info_instance_name);
    return init != nullptr ? init->Value() : fallback;
  }

  template <class T>
  bool Contains(std::string_view info_instance_name) const {
    return GetOrDefault<T>(info_instance_name) != nullptr;
  }

  template <class T>
  T* GetOrDefault() const {
    return dict_.GetOrDefault<T>();
  }

  template <class T>
  T* Get() const {
    return dict_.Get<T>();
  }

  template <class T>
  auto GetValue() const {
    return Get<T>()->Value();
  }

  template <class T, class U>
  U GetValue(U fallback) const {
    auto* init = GetOrDefault<T>();
    return init != nullptr ? init->Value() : fallback;
  }

  template <class T>
  bool Contains() const {
    return GetOrDefault<T>() != nullptr;
  }

 private:
  Actor& self_;
  TypeDictionary& dict_;
};

template <class T>
class ValueActorInit : public ActorInit {
 public:
  explicit ValueActorInit(T value, std::string str_instance_name = "")
      : ActorInit(std::move(str_instance_name)), value_(std::move(value)) {}

  const T& Value() const { return value_; }

 private:
  T value_;
};

class LocationInit : public ValueActorInit<CPos>,
                     public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(LocationInit, OpenRA_LocationInit,
                gen::TypeId::OpenRA_ISingleInstanceInit);
  explicit LocationInit(CPos value)
      : ValueActorInit<CPos>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class OwnerInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(OwnerInit, OpenRA_OwnerInit,
                gen::TypeId::OpenRA_ISingleInstanceInit);

  explicit OwnerInit(Player* value);
  explicit OwnerInit(std::string str_internal_name);

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }

  Player* Value(class World& world) const;

  const std::string& InternalName() const { return str_internal_name_; }

 private:
  Player* p_value_ = nullptr;
  std::string str_internal_name_;
};

class FacingInit : public ValueActorInit<WAngle>,
                   public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(FacingInit, OpenRA_Mods_Common_FacingInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit FacingInit(WAngle value)
      : ValueActorInit<WAngle>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class CreationActivityDelayInit : public ValueActorInit<int>,
                                 public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(CreationActivityDelayInit,
                OpenRA_Mods_Common_CreationActivityDelayInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit CreationActivityDelayInit(int value)
      : ValueActorInit<int>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class DynamicFacingInit
    : public ValueActorInit<std::function<WAngle()>>,
      public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(DynamicFacingInit, OpenRA_Mods_Common_DynamicFacingInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit DynamicFacingInit(std::function<WAngle()> value)
      : ValueActorInit<std::function<WAngle()>>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class TerrainOrientationInit : public ValueActorInit<WRot>,
                               public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(TerrainOrientationInit,
                OpenRA_Mods_Common_TerrainOrientationInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit TerrainOrientationInit(WRot value)
      : ValueActorInit<WRot>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class SubCellInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(SubCellInit, OpenRA_Mods_Common_SubCellInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)

  explicit SubCellInit(SubCell value)
      : uint1_value_(static_cast<std::uint8_t>(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }

  SubCell Value() const { return static_cast<SubCell>(uint1_value_); }

 private:
  std::uint8_t uint1_value_;
};

class CenterPositionInit : public ValueActorInit<WPos>,
                           public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(CenterPositionInit, OpenRA_Mods_Common_CenterPositionInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit CenterPositionInit(WPos value)
      : ValueActorInit<WPos>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class FactionInit : public ValueActorInit<std::string>,
                    public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(FactionInit, OpenRA_Mods_Common_FactionInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit FactionInit(std::string value)
      : ValueActorInit<std::string>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class RallyPointInit : public ValueActorInit<std::vector<CPos>>,
                       public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(RallyPointInit, OpenRA_Mods_Common_Traits_RallyPointInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit RallyPointInit(std::vector<CPos> value)
      : ValueActorInit<std::vector<CPos>>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class SpawnedByMapInit : public ActorInit,
                         public ISuppressInitExport,
                         public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(SpawnedByMapInit, OpenRA_Mods_Common_Traits_SpawnedByMapInit,
                gen::TypeId::OpenRA_ISuppressInitExport,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit SpawnedByMapInit(std::string str_instance_name = "")
      : ActorInit(std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class Player;

class EffectiveOwnerInit : public ValueActorInit<Player*>,
                           public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(EffectiveOwnerInit, OpenRA_Mods_Common_EffectiveOwnerInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit EffectiveOwnerInit(Player* value)
      : ValueActorInit<Player*>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class HiddenUnderFogInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HiddenUnderFogInit, OpenRA_Mods_Common_Traits_HiddenUnderFogInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  HiddenUnderFogInit() = default;

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class HuskSpeedInit : public ValueActorInit<int>, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HuskSpeedInit, OpenRA_Mods_Common_Traits_HuskSpeedInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit HuskSpeedInit(int value)
      : ValueActorInit<int>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class TurretFacingInit : public ValueActorInit<WAngle> {
 public:
  ORA_INIT_TYPE_SELF_ONLY(TurretFacingInit, OpenRA_Mods_Common_Traits_TurretFacingInit)
  explicit TurretFacingInit(WAngle value,
                            std::string str_instance_name = "")
      : ValueActorInit<WAngle>(value, std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class DynamicTurretFacingInit
    : public ValueActorInit<std::function<WAngle()>> {
 public:
  ORA_INIT_TYPE_SELF_ONLY(DynamicTurretFacingInit,
                          OpenRA_Mods_Common_Traits_DynamicTurretFacingInit)
  explicit DynamicTurretFacingInit(std::function<WAngle()> value,
                                   std::string str_instance_name = "")
      : ValueActorInit<std::function<WAngle()>>(std::move(value),
                                                std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class HealthInit : public ValueActorInit<int>, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HealthInit, OpenRA_Mods_Common_Traits_HealthInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit HealthInit(int value)
      : ValueActorInit<int>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class ExperienceInit : public ValueActorInit<int> {
 public:
  ORA_INIT_TYPE_SELF_ONLY(ExperienceInit, OpenRA_Mods_Common_Traits_ExperienceInit)
  explicit ExperienceInit(int value, std::string str_instance_name = "")
      : ValueActorInit<int>(value, std::move(str_instance_name)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class DeployStateInit : public ValueActorInit<int>, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(DeployStateInit, OpenRA_Mods_Common_Traits_DeployStateInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit DeployStateInit(int value)
      : ValueActorInit<int>(value) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class SkipMakeAnimsInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(SkipMakeAnimsInit, OpenRA_Mods_Common_Traits_SkipMakeAnimsInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  SkipMakeAnimsInit() = default;

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class RuntimeCargoInit
    : public ValueActorInit<std::vector<Actor*>>,
      public ISuppressInitExport {
 public:
  ORA_INIT_TYPE(RuntimeCargoInit, OpenRA_Mods_Common_Traits_RuntimeCargoInit,
                gen::TypeId::OpenRA_ISuppressInitExport)
  explicit RuntimeCargoInit(std::vector<Actor*> value)
      : ValueActorInit<std::vector<Actor*>>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class CargoInit : public ValueActorInit<std::vector<std::string>> {
 public:
  ORA_INIT_TYPE(CargoInit, OpenRA_Mods_Common_Traits_CargoInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  explicit CargoInit(std::vector<std::string> value)
      : ValueActorInit<std::vector<std::string>>(std::move(value)) {}

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

class InitRegistry {
 public:
  using InitFactory = std::function<ActorInit*(
      const yaml::MiniYaml& yaml_value, std::string str_instance_name)>;

  static InitRegistry& Instance();

  void Register(std::string str_init_name, InitFactory fn_factory);

  ActorInit* CreateOrThrow(const std::string& str_init_name,
                           const yaml::MiniYaml& yaml_value,
                           const std::string& str_instance_name) const;

  bool Contains(std::string_view str_init_name) const;

 private:
  std::vector<std::pair<std::string, InitFactory>> vec_entries_;
};

}
