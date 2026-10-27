#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

struct GenericTuple {
  std::array<std::int64_t, 4> arr_ints{};
  std::array<float, 4> arr_floats{};
  std::uint8_t uint1_count{};
};

using GenericDict = std::vector<std::pair<struct GenericValue, struct GenericValue>>;

struct GenericValue {

  std::variant<std::monostate,
               bool,
               std::int64_t,

               float,
               std::string,
               GenericTuple,
               std::vector<GenericValue>,
               GenericDict,
               std::shared_ptr<RecordObject>>

               val{};

  bool IsNull() const { return std::holds_alternative<std::monostate>(val); }

  static GenericValue Null() { return {}; }
  static GenericValue Of(bool b_v) {
    GenericValue v;
    v.val = b_v;
    return v;
  }
  static GenericValue Of(std::int64_t int8_v) {
    GenericValue v;
    v.val = int8_v;
    return v;
  }
  static GenericValue Of(float fp4_v) {
    GenericValue v;
    v.val = fp4_v;
    return v;
  }
  static GenericValue Of(std::string str_v) {
    GenericValue v;
    v.val = std::move(str_v);
    return v;
  }

  static GenericValue Of(const char* sz_v) { return Of(std::string{sz_v}); }
  static GenericValue Of(GenericTuple t_v) {
    GenericValue v;
    v.val = std::move(t_v);
    return v;
  }
  static GenericValue Of(std::vector<GenericValue> vec_v) {
    GenericValue v;
    v.val = std::move(vec_v);
    return v;
  }
  static GenericValue Of(GenericDict map_v) {
    GenericValue v;
    v.val = std::move(map_v);
    return v;
  }
  static GenericValue Of(std::shared_ptr<RecordObject> rec_v) {
    GenericValue v;
    v.val = std::move(rec_v);
    return v;
  }
};

class GeneratedRecord : public RecordObject {
 public:

  GeneratedRecord(const RecordDesc& desc, std::span<const GenericValue> defaults);

  const RecordDesc& record_desc() const override { return *desc_; }

  GenericValue& Slot(std::size_t int4_index) { return vec_values_[int4_index]; }
  const GenericValue& Slot(std::size_t int4_index) const { return vec_values_[int4_index]; }

  const std::vector<GenericValue>& Values() const { return vec_values_; }

 private:
  const RecordDesc* desc_;
  std::vector<GenericValue> vec_values_;
};

void LoadValueIntoValue(const FieldDesc& desc, GenericValue& val_out,
                        const yaml::MiniYaml& node);

void AssignToMemory(const FieldDesc& desc, const GenericValue& val, void* p_field);

GenericValue ReadFromMemory(const FieldDesc& desc, const void* p_field);

void RegisterGeneratedLoader(
    std::string_view str_loader,
    std::function<void(GenericValue& val_slot, const yaml::MiniYaml& trait_yaml)> fn_load);

const std::function<void(GenericValue&, const yaml::MiniYaml&)>* FindGeneratedLoader(
    std::string_view str_loader);

void ClearGeneratedLoadersForTest();

}
