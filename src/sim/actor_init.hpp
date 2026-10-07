// UPSTREAM: OpenRA.Game/Map/ActorInitializer.cs @b6fc03f L21-262(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - IActorInitializer/ActorInitializer:模板查询转发 TypeDictionary;
//    info.InstanceName 匹配语义(带名 init 优先同名,回落无名;重复取最后
//    —— LastOrDefault)逐字保留
//    IActorInitializer/ActorInitializer: template queries forwarding into
//    the TypeDictionary; the info.InstanceName matching semantics (named
//    inits prefer the same name, falling back to unnamed ones; duplicates
//    take the last — LastOrDefault) kept verbatim.
//  - ValueActorInit<T>.Initialize(yaml) 的反射写字段 → 构造注入
//    (C++ 侧值不可变;Save/Initialize 面随 Phase 5 地图序列化落地)
//    The reflection-backed field writes of ValueActorInit<T>.Initialize
//    (yaml) become constructor injection (values are immutable on the C++
//    side; the Save/Initialize surface lands with Phase 5 map
//    serialization).
//  - 注册面:上游 Add 注册 GetInterfaces()+BaseTypes()(含泛型基类 Type 键);
//    C++ 侧经 ORA_INIT_TYPE 声明 {接口, 自身}。泛型基类键
//    (ValueActorInit<T>)无查询使用点,省略 —— COVERAGE 登记
//    Registration surface: upstream Add registers GetInterfaces() +
//    BaseTypes() (generic base Type keys included); the C++ side declares
//    {interfaces, self} via ORA_INIT_TYPE. Generic base keys
//    (ValueActorInit<T>) have no query sites and are omitted — registered
//    in COVERAGE.
#pragma once
import std;

#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::sim {

class Actor;

/// ActorInitializer.cs L37-109
class ActorInitializer {
 public:
  ActorInitializer(Actor& self, TypeDictionary& dict)
      : self_(self), dict_(dict) {}

  Actor& Self() { return self_; }
  Actor& Self() const { return self_; }
  TypeDictionary& Dict() { return dict_; }

  /// GetOrDefault<T>(L50-63):trait info 的 InstanceName 过滤
  /// GetOrDefault<T> (L50-63): the InstanceName filter of the trait info.
  template <class T>
  T* GetOrDefault(std::string_view info_instance_name) const {
    auto inits = dict_.WithInterface<T>();

    // Traits tagged with an instance name prefer inits with the same name.
    // If a more specific init is not available, fall back to an unnamed init.
    // If duplicate inits are defined, take the last to match standard yaml
    // override expectations
    if (!info_instance_name.empty()) {
      // 两次独立扫描,语义同 LastOrDefault(同名) ?? LastOrDefault(无名)
      // Two independent scans, equivalent to LastOrDefault(same name) ??
      // LastOrDefault(unnamed).
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

    // Untagged traits will only use untagged inits
    T* unnamed = nullptr;
    for (T* i : inits)
      if (i->InstanceName().empty())
        unnamed = i;
    return unnamed;
  }

  /// Get<T>(L65-72)
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

  /// GetValue(L74-82)
  template <class T, class U>
  U GetValue(std::string_view info_instance_name) const {
    return Get<T>(info_instance_name)->Value();
  }

  template <class T, class U>
  U GetValue(std::string_view info_instance_name, U fallback) const {
    auto* init = GetOrDefault<T>(info_instance_name);
    return init != nullptr ? init->Value() : fallback;
  }

  /// Contains(L85)
  template <class T>
  bool Contains(std::string_view info_instance_name) const {
    return GetOrDefault<T>(info_instance_name) != nullptr;
  }

  // ———— ISingleInstanceInit 直查族(L87-108)————
  // ———— The ISingleInstanceInit direct-query family (L87-108) ————
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

/// ValueActorInit<T>(ActorInitializer.cs L141-171):不可变值载荷
/// ValueActorInit<T> (ActorInitializer.cs L141-171): an immutable value
/// payload.
template <class T>
class ValueActorInit : public ActorInit {
 public:
  explicit ValueActorInit(T value, std::string str_instance_name = "")
      : ActorInit(std::move(str_instance_name)), value_(std::move(value)) {}

  const T& Value() const { return value_; }

 private:
  T value_;
};

/// LocationInit(ActorInitializer.cs L219-221)
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

/// OwnerInit(ActorInitializer.cs L223-262):Player 或 InternalName 延迟解析
/// OwnerInit (ActorInitializer.cs L223-262): a Player or a lazily resolved
/// InternalName.
class OwnerInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(OwnerInit, OpenRA_OwnerInit,
                gen::TypeId::OpenRA_ISingleInstanceInit);

  explicit OwnerInit(Player* value);
  explicit OwnerInit(std::string str_internal_name);

  // ActorInit → TraitBase 的纯虚收尾(init 非 trait,注册面为空;
  // GetTraitTypeId 由 ORA_INIT_TYPE 提供)
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }

  /// Value(world)(L239-241):显式 Player 优先,否则按 InternalName 找首个
  /// Value(world) (L239-241): an explicit Player wins; otherwise the first
  /// player with the InternalName.
  Player* Value(class World& world) const;

  const std::string& InternalName() const { return str_internal_name_; }

 private:
  Player* p_value_ = nullptr;
  std::string str_internal_name_;
};

// ———— 第三批 init 增补(Mods.Common/ActorInitializer.cs + 引用处定义)————
// ———— The batch-3 init additions (Mods.Common/ActorInitializer.cs + the
//      definitions at the use sites) ————

/// FacingInit(ActorInitializer.cs L20)
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

/// CreationActivityDelayInit(ActorInitializer.cs L28)
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

/// DynamicFacingInit(ActorInitializer.cs L32;Value = 面向闭包)
/// DynamicFacingInit (ActorInitializer.cs L32; the Value is a facing
/// closure).
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

/// SubCellInit(ActorInitializer.cs L37-58;非 ValueInit —— map.yaml 以数值
/// 而非枚举名携带)
/// SubCellInit (ActorInitializer.cs L37-58; not a ValueInit — map.yaml
/// carries the numeric value, not the enum name).
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

/// CenterPositionInit(ActorInitializer.cs L61)
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

/// FactionInit(ActorInitializer.cs L66;地图/变身指定 actor 的阵营变体)
/// FactionInit (ActorInitializer.cs L66; maps/transformations name the
/// actor's faction variant).
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

/// RallyPointInit(Production.cs L151;CPos[] 集结路径)
/// RallyPointInit (Production.cs L151; the CPos[] rally path).
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

/// SpawnedByMapInit(SpawnMapActors.cs L66-71;地图摆位标记 ——
/// FrozenUnderFog 的 startsRevealed 判据;SpawnMapActors trait 随后批,
/// 本批为标记承载面)
/// SpawnedByMapInit (SpawnMapActors.cs L66-71; the map-placement marker —
/// FrozenUnderFog's startsRevealed predicate; the SpawnMapActors trait
/// arrives with a later batch, this one carries the marker face).
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

/// HiddenUnderFogInit(FrozenUnderFog.cs L189;RuntimeFlagInit 标记)
/// HiddenUnderFogInit (FrozenUnderFog.cs L189; the RuntimeFlagInit
/// marker).
class HiddenUnderFogInit : public ActorInit, public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HiddenUnderFogInit, OpenRA_Mods_Common_Traits_HiddenUnderFogInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  HiddenUnderFogInit() = default;

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

/// HuskSpeedInit(Husk.cs L193;残骸拖尾速度)
/// HuskSpeedInit (Husk.cs L193; the husk's drag speed).
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

// ———— 第六批 init 增补(Turreted.cs L316-332)————
// ———— The batch-6 init additions (Turreted.cs L316-332) ————

/// TurretFacingInit(Turreted.cs L316-326;info 实例名携带 —— Turreted 的
/// 按名匹配面)
/// TurretFacingInit (Turreted.cs L316-326; carries the info instance name —
/// Turreted's by-name matching face).
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

/// DynamicTurretFacingInit(Turreted.cs L328-332;Value = 朝向闭包)
/// DynamicTurretFacingInit (Turreted.cs L328-332; the Value is a facing
/// closure).
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

}  // namespace ora::sim
