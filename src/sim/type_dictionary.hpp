#pragma once
import std;

#include "gen/interfaces_gen.h"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class ISingleInstanceInit {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_ISingleInstanceInit;
  virtual ~ISingleInstanceInit() = default;
};

class ISuppressInitExport {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_ISuppressInitExport;
  virtual ~ISuppressInitExport() = default;
};

class ActorInit : public TraitBase {
 public:
  explicit ActorInit(std::string str_instance_name = "")
      : str_instance_name_(std::move(str_instance_name)) {}
  ~ActorInit() override = default;

  const std::string& InstanceName() const { return str_instance_name_; }

  virtual std::span<const gen::TypeId> InitTypeIds() const = 0;

  virtual gen::TypeId GetInitTypeId() const = 0;

 private:
  std::string str_instance_name_;
};

#define ORA_INIT_TYPE(InitT, TypeIdEnum, ...)                             \
  static constexpr gen::TypeId kInitTypeId = gen::TypeId::TypeIdEnum;     \
  gen::TypeId GetTraitTypeId() const override { return kInitTypeId; }    \
  gen::TypeId GetInitTypeId() const override { return kInitTypeId; }     \
  std::span<const gen::TypeId> InitTypeIds() const override {            \
    static constexpr gen::TypeId kIds[]{__VA_ARGS__};                    \
    return std::span<const gen::TypeId>(kIds);                           \
  }

#define ORA_INIT_TYPE_SELF_ONLY(InitT, TypeIdEnum)                        \
  static constexpr gen::TypeId kInitTypeId = gen::TypeId::TypeIdEnum;     \
  gen::TypeId GetTraitTypeId() const override { return kInitTypeId; }    \
  gen::TypeId GetInitTypeId() const override { return kInitTypeId; }     \
  std::span<const gen::TypeId> InitTypeIds() const override {            \
    return {};                                                           \
  }

class TypeDictionary {
 public:
  TypeDictionary() = default;

  TypeDictionary(const TypeDictionary& dict_other)
      : data_(dict_other.data_) {}
  TypeDictionary& operator=(const TypeDictionary&) = delete;

  template <class T>
  static constexpr gen::TypeId KeyOf() {
    if constexpr (requires { T::kInitTypeId; })
      return T::kInitTypeId;
    else
      return T::kTypeId;
  }

  void Add(ActorInit* val);

  void Add(std::unique_ptr<ActorInit> val) {
    ActorInit* ptr_val = val.get();
    map_owned_[ptr_val] = std::move(val);
    Add(ptr_val);
  }

  template <class T>
  bool Contains() const {
    return Contains(KeyOf<T>());
  }
  bool Contains(gen::TypeId t) const { return data_.count(t) != 0; }

  template <class T>
  T* Get() {
    return GetImpl<T>(true);
  }

  template <class T>
  T* GetOrDefault() {
    return GetImpl<T>(false);
  }

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
  std::map<ActorInit*, std::unique_ptr<ActorInit>> map_owned_;
};

}
