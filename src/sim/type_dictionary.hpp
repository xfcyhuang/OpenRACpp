// UPSTREAM: OpenRA.Game/Primitives/TypeDictionary.cs @b6fc03f L19-183(逐语义
//          重写;Type 键 → gen::TypeId,泛型容器 → TypeId 桶 + void* 载荷)
//          Verbatim-semantics rewrite; the Type key becomes gen::TypeId and
//          the generic container a TypeId-bucketed void* store.
//
// 机制对照 / Mechanism mapping:
//  - Dictionary<Type, TypeContainer<T>> → std::map<gen::TypeId,
//    std::vector<void*>>(插入序保持 C# List 语义)
//  - Add(val):GetInterfaces()+BaseTypes() 全注册 → Add(val, type_ids)
//    (调用方 —— ActorInit 子类 —— 经 ORA_INIT_TYPE 静态声明注册面)
//  - Get<T>/GetOrDefault<T>/WithInterface<T>/Remove<T>:异常消息逐字
//    (typeof(T) 形态 → T 的全名,经 gen::kTypeFullNames 反查)
#pragma once
import std;

#include "gen/interfaces_gen.h"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

/// ISingleInstanceInit 标记(ActorInitializer.cs L139)
/// The ISingleInstanceInit marker (ActorInitializer.cs L139).
class ISingleInstanceInit {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_ISingleInstanceInit;
  virtual ~ISingleInstanceInit() = default;
};

/// ActorInit 基类(ActorInitializer.cs L124-137):InstanceName + 注册面
/// The ActorInit base (ActorInitializer.cs L124-137): InstanceName plus the
/// registration surface.
class ActorInit : public TraitBase {
 public:
  explicit ActorInit(std::string str_instance_name = "")
      : str_instance_name_(std::move(str_instance_name)) {}
  ~ActorInit() override = default;

  const std::string& InstanceName() const { return str_instance_name_; }

  /// 本 init 的注册面(全部接口+基类;等价 C# val.GetType() 的 GetInterfaces
  /// + BaseTypes)
  /// This init's registration surface (all interfaces + base classes; the
  /// equivalent of the C# GetInterfaces + BaseTypes of val.GetType()).
  virtual std::span<const gen::TypeId> InitTypeIds() const = 0;

  /// 具体类型键(重复 init 检查按此计数 —— C# i.GetType())
  /// The concrete-type key (the duplicate-init check counts on it — C#
  /// i.GetType()).
  virtual gen::TypeId GetInitTypeId() const = 0;

 private:
  std::string str_instance_name_;
};

/// ActorInit 子类的注册面声明(宏:静态 TypeId 表 + 运行时键;置于类内
/// public 区)。接口面 = {全部接口与基类 TypeId}(含 ISingleInstanceInit
/// 时才能参与单实例查询)
/// Declares the registration surface of an ActorInit subclass (the static
/// TypeId table + runtime keys; place inside the class's public section).
/// The interface face = {every interface and base TypeId} (only with
/// ISingleInstanceInit listed does the single-instance query see it).
///
/// 用法 / usage:
///   class LocationInit : public ValueActorInit<core::CPos>,
///                        public ISingleInstanceInit {
///    public:
///     ORA_INIT_TYPE(LocationInit, OpenRA_LocationInit,
///                   {gen::TypeId::OpenRA_ISingleInstanceInit});
///     explicit LocationInit(core::CPos value)
///         : ValueActorInit<core::CPos>(std::move(value)) {}
///   };
#define ORA_INIT_TYPE(InitT, TypeIdEnum, ...)                             \
  static constexpr gen::TypeId kInitTypeId = gen::TypeId::TypeIdEnum;     \
  gen::TypeId GetTraitTypeId() const override { return kInitTypeId; }    \
  gen::TypeId GetInitTypeId() const override { return kInitTypeId; }     \
  std::span<const gen::TypeId> InitTypeIds() const override {            \
    static constexpr gen::TypeId kIds[]{__VA_ARGS__};                    \
    return std::span<const gen::TypeId>(kIds);                           \
  }

/// TypeDictionary.cs L19-183
class TypeDictionary {
 public:
  /// 查询键解析:Add 注册 {接口集}∪{self 具体键}(上游 GetInterfaces() +
  /// BaseTypes() 含 self;基类链键无查询点);具体 init 类查询按
  /// kInitTypeId(ORA_INIT_TYPE),接口查询按 kTypeId
  /// Query-key resolution: Add registers {interfaces} ∪ {the self concrete
  /// key} (upstream's GetInterfaces() + BaseTypes() including self; the
  /// base-chain keys have no query sites); concrete-init queries go by
  /// kInitTypeId (ORA_INIT_TYPE), interface queries by kTypeId.
  template <class T>
  static constexpr gen::TypeId KeyOf() {
    if constexpr (requires { T::kInitTypeId; })
      return T::kInitTypeId;
    else
      return T::kTypeId;
  }

  /// Add(L41-53):按注册面全量登记
  void Add(ActorInit* val);

  /// Contains<T>(L56)/Contains(Type)(L60)
  template <class T>
  bool Contains() const {
    return Contains(KeyOf<T>());
  }
  bool Contains(gen::TypeId t) const { return data_.count(t) != 0; }

  /// Get<T>(L66):缺失/多实例异常消息逐字(TypeDictionary.cs L81/L87;
  /// typeof(T) → 全名)
  /// Get<T> (L66): missing/multiple exceptions verbatim (TypeDictionary.cs
  /// L81/L87; typeof(T) → the full name).
  template <class T>
  T* Get() {
    return GetImpl<T>(true);
  }

  /// GetOrDefault<T>(L71)
  template <class T>
  T* GetOrDefault() {
    return GetImpl<T>(false);
  }

  /// WithInterface<T>(L91-96):插入序拷贝(桶载荷即 T*;空桶返回空)
  /// WithInterface<T> (L91-96): insertion-order copy (bucket payloads are
  /// T*; empty buckets yield empty).
  template <class T>
  std::vector<T*> WithInterface() const {
    auto it = data_.find(KeyOf<T>());
    if (it == data_.end())
      return {};
    std::vector<T*> out;
    out.reserve(it->second.size());
    for (void* p : it->second)
      out.push_back(static_cast<T*>(p));
    return out;
  }

  /// Remove(L98-116):按注册面移除,空容器删键
  void Remove(ActorInit* val);

 private:
  template <class T>
  T* GetImpl(bool throws_if_missing) {
    constexpr gen::TypeId key = KeyOf<T>();
    auto it = data_.find(key);
    if (it == data_.end()) {
      if (throws_if_missing)
        throw std::runtime_error(
            "TypeDictionary does not contain instance of type `" +
            std::string(FullNameOfTypeIdSafe(key)) + "`");
      return nullptr;
    }

    std::vector<T*> list;
    list.reserve(it->second.size());
    for (void* p : it->second)
      list.push_back(static_cast<T*>(p));
    if (list.size() > 1)
      throw std::runtime_error(
          "TypeDictionary contains multiple instances of type `" +
          std::string(FullNameOfTypeIdSafe(key)) + "`");
    return list.empty() ? nullptr : list.front();
  }

  static std::string_view FullNameOfTypeIdSafe(gen::TypeId id) {
    const auto v = static_cast<std::size_t>(id);
    if (v < gen::kTypeIdCount)
      return gen::kTypeFullNames[v];
    return "(unknown)";
  }

  std::map<gen::TypeId, std::vector<void*>> data_;
};

}  // namespace ora::sim
