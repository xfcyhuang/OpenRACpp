#pragma once
import std;
#include "field_desc.hpp"
#include "type_registry.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

class MissingFieldsException : public yaml::YamlException {
 public:

  MissingFieldsException(std::vector<std::string> vec_missing, std::string str_header = {},
                         std::string str_header_single = {})
      : yaml::YamlException(BuildMessage(vec_missing,
                                         vec_missing.size() > 1 ? str_header
                                                                : FirstNonEmpty(str_header_single,
                                                                                str_header))),
        vec_missing_{std::move(vec_missing)} {}

  const std::vector<std::string>& Missing() const { return vec_missing_; }

 private:
  static std::string FirstNonEmpty(std::string_view sv_a, std::string_view sv_b) {
    return std::string{sv_a.empty() ? sv_b : sv_a};
  }
  static std::string BuildMessage(const std::vector<std::string>& vec_missing,
                                  std::string_view sv_header) {

    std::string str_ret{sv_header.empty() ? std::string{} : std::string{sv_header} + ": "};
    if (!vec_missing.empty()) {
      str_ret += vec_missing.front();
      for (std::size_t int4_i{1}; int4_i < vec_missing.size(); int4_i++)
        str_ret += ", " + vec_missing[int4_i];
    }
    return str_ret;
  }

  std::vector<std::string> vec_missing_;
};

[[noreturn]] void DefaultInvalidValue(std::string_view sv_value, std::string_view sv_field_type,
                                      std::string_view sv_field_name);

[[noreturn]] void DefaultUnknownField(std::string_view sv_field_name);

void Load(RecordObject* obj_record, const yaml::MiniYaml& yaml_my);

std::int32_t GetInt32Value(std::string_view str_field, std::string_view sv_value);
float GetFloatValue(std::string_view str_field, std::string_view sv_value);
bool GetBoolValue(std::string_view str_field, std::string_view sv_value);
std::vector<std::string> GetStringArrayValue(std::string_view str_field, std::string_view sv_value);

Rectangle GetRectangleValue(std::string_view str_field, std::string_view sv_value);
int2 GetSizeValue(std::string_view str_field, std::string_view sv_value);
core::Vector2 GetVector2Value(std::string_view str_field, std::string_view sv_value);

std::int32_t GetWDistValue(std::string_view str_field, std::string_view sv_value);
core::Vector3 GetVector3Value(std::string_view str_field, std::string_view sv_value);
core::Color GetColorValue(std::string_view str_field, std::string_view sv_value);
std::vector<std::int32_t> GetInt32ArrayValue(std::string_view str_field, std::string_view sv_value);
std::vector<float> GetFloatArrayValue(std::string_view str_field, std::string_view sv_value);

std::vector<std::pair<std::string, std::string>> GetStringDictionaryValue(
    const yaml::MiniYaml& yaml_node, std::string_view str_field);

std::int32_t GetEnumValue(std::string_view str_field, std::string_view sv_value,
                          std::string_view str_enum_full_name);

[[noreturn]] void UnknownLoader(std::string_view str_loader, std::string_view str_field);

std::vector<const FieldDesc*> CollectFields(const RecordDesc& desc);

void RegisterBitSet(
    std::string_view str_tag,
    std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits,
    std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings,
    std::function<std::optional<std::uint64_t>(std::string_view)>
        fn_bit_noalloc = {});

void RegisterRuntimeBitSet(std::string_view str_tag);

std::uint64_t BitsOf(std::string_view str_tag, std::span<const std::string> vec_values);

std::vector<std::string> StringsOfBits(std::string_view str_tag, std::uint64_t uint8_bits);

std::optional<std::uint64_t> BitSetBitsOfNoAlloc(
    std::string_view str_tag, std::span<const std::string> vec_values);

bool BitSetContainsString(std::string_view str_tag, std::uint64_t uint8_bits,
                          std::string_view str_value);

void RegisterLoader(
    std::string_view str_loader,
    std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)> fn_load);

}
