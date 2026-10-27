import std;
#include "field_loader.hpp"
#include "generic_record.hpp"
#include "parse.hpp"
#include "variable_expression.hpp"
#include "core/text.hpp"

namespace ora::meta {

namespace {

void* FieldPtr(RecordObject* obj_record, const FieldDesc& desc) {
  return static_cast<void*>(reinterpret_cast<char*>(obj_record) + desc.off_offset);
}

}

namespace {
struct BitSetOps {
  std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits;
  std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings;

  std::function<std::optional<std::uint64_t>(std::string_view)> fn_bit_noalloc;
};

std::unordered_map<std::string, BitSetOps>& BitSetTable() {
  static std::unordered_map<std::string, BitSetOps> map_table;
  return map_table;
}
}

void RegisterBitSet(
    std::string_view str_tag,
    std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits,
    std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings,
    std::function<std::optional<std::uint64_t>(std::string_view)>
        fn_bit_noalloc) {
  BitSetTable().insert_or_assign(
      std::string{str_tag},
      BitSetOps{.fn_get_bits = std::move(fn_get_bits),
                .fn_get_strings = std::move(fn_get_strings),
                .fn_bit_noalloc = std::move(fn_bit_noalloc)});
}

std::uint64_t BitsOf(std::string_view str_tag, std::span<const std::string> vec_values) {
  const auto it_find = BitSetTable().find(std::string{str_tag});
  if (it_find == BitSetTable().end())
    std::abort();
  return it_find->second.fn_get_bits(vec_values);
}

std::vector<std::string> StringsOfBits(std::string_view str_tag, std::uint64_t uint8_bits) {
  const auto it_find = BitSetTable().find(std::string{str_tag});
  if (it_find == BitSetTable().end())
    std::abort();
  return it_find->second.fn_get_strings(uint8_bits);
}

void RegisterRuntimeBitSet(std::string_view str_tag) {

  auto map_bits = std::make_shared<std::unordered_map<std::string, std::size_t>>();
  auto vec_names = std::make_shared<std::vector<std::string>>();
  RegisterBitSet(
      str_tag,
      [map_bits, vec_names](std::span<const std::string> vec_values) {
        std::uint64_t uint8_bits{0};
        for (const std::string& str_value : vec_values) {
          std::size_t int4_index{};
          const auto it_find = map_bits->find(str_value);
          if (it_find == map_bits->end()) {
            int4_index = vec_names->size();
            map_bits->emplace(str_value, int4_index);
            vec_names->push_back(str_value);
          } else {
            int4_index = it_find->second;
          }
          uint8_bits |= std::uint64_t{1} << int4_index;
        }
        return uint8_bits;
      },
      [vec_names](std::uint64_t uint8_bits) {
        std::vector<std::string> vec_values;
        for (std::size_t int4_i{}; int4_i < vec_names->size(); int4_i++)
          if (uint8_bits & (std::uint64_t{1} << int4_i))
            vec_values.push_back((*vec_names)[int4_i]);
        return vec_values;
      },
      [map_bits](std::string_view sv_value)
          -> std::optional<std::uint64_t> {
        if (const auto it_find = map_bits->find(std::string{sv_value});
            it_find != map_bits->end())
          return std::uint64_t{1} << it_find->second;
        return std::nullopt;
      });
}

std::optional<std::uint64_t> BitSetBitsOfNoAlloc(
    std::string_view str_tag, std::span<const std::string> vec_values) {
  const auto it_find = BitSetTable().find(std::string{str_tag});
  if (it_find == BitSetTable().end() || !it_find->second.fn_bit_noalloc)
    return std::nullopt;
  std::uint64_t uint8_bits{0};
  for (const std::string& str_value : vec_values)
    if (const auto bit = it_find->second.fn_bit_noalloc(str_value))
      uint8_bits |= *bit;
  return uint8_bits;
}

bool BitSetContainsString(std::string_view str_tag, std::uint64_t uint8_bits,
                          std::string_view str_value) {
  const auto it_find = BitSetTable().find(std::string{str_tag});
  if (it_find == BitSetTable().end() || !it_find->second.fn_bit_noalloc)
    return false;
  const auto bit = it_find->second.fn_bit_noalloc(str_value);
  return bit.has_value() && (uint8_bits & *bit) != 0;
}

namespace {
std::unordered_map<std::string_view,
                   std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)>>&
LoaderTable() {
  static std::unordered_map<
      std::string_view,
      std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)>>
      map_table;
  return map_table;
}
}

void RegisterLoader(
    std::string_view str_loader,
    std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)> fn_load) {

  static std::deque<std::string> deque_pool;
  LoaderTable().insert_or_assign(*deque_pool.emplace(deque_pool.end(), str_loader),
                                 std::move(fn_load));
}

[[noreturn]] void UnknownLoader(std::string_view str_loader, std::string_view str_field) {
  throw yaml::YamlException(std::format(
      "FieldLoader: LoadUsing loader `{}` for field `{}` is not registered", str_loader,
      str_field));
}

[[noreturn]] void DefaultInvalidValue(std::string_view sv_value, std::string_view sv_field_type,
                                      std::string_view sv_field_name) {
  throw yaml::YamlException(std::format("FieldLoader: Cannot parse `{}` into field `{}` of type `{}`",
                                        sv_value, sv_field_name, sv_field_type));
}

[[noreturn]] void DefaultUnknownField(std::string_view sv_field_name) {
  throw yaml::YamlException(std::format("FieldLoader: Missing field `{}`", sv_field_name));
}

std::vector<const FieldDesc*> CollectFields(const RecordDesc& desc) {

  std::vector<const RecordDesc*> vec_chain;
  const RecordDesc* desc_cur = &desc;
  while (desc_cur != nullptr) {
    vec_chain.push_back(desc_cur);
    desc_cur =
        desc_cur->str_base.empty() ? nullptr : TypeRegistry::FindType(desc_cur->str_base);
  }
  std::vector<const FieldDesc*> vec_fields;
  for (auto it_desc = vec_chain.rbegin(); it_desc != vec_chain.rend(); ++it_desc)
    for (const FieldDesc& desc_f : (*it_desc)->fields)
      vec_fields.push_back(&desc_f);
  return vec_fields;
}

void Load(RecordObject* obj_record, const yaml::MiniYaml& yaml_my) {
  const RecordDesc& desc = obj_record->record_desc();
  const std::vector<const FieldDesc*> vec_fields = CollectFields(desc);
  std::vector<std::string> vec_missing;

  auto* rec_generated = dynamic_cast<GeneratedRecord*>(obj_record);

  if (rec_generated != nullptr && rec_generated->Values().size() != vec_fields.size())
    throw std::runtime_error(std::format(
        "GeneratedRecord slot mismatch: {} fields vs {} slots ({})",
        vec_fields.size(), rec_generated->Values().size(), desc.str_name));

  const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_md =
      yaml_my.ToDictionary();
  const auto find_md = [&vec_md](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const auto& [sv_k, yaml_v] : vec_md)
      if (sv_k == sv_key)
        return yaml_v;
    return nullptr;
  };

  for (std::size_t int4_slot{}; int4_slot < vec_fields.size(); int4_slot++) {
    const FieldDesc& desc_field = *vec_fields[int4_slot];

    if (!desc_field.str_loader.empty()) {

      const yaml::MiniYaml* yaml_found = find_md(desc_field.str_name);
      if (desc_field.b_required && yaml_found == nullptr) {
        vec_missing.push_back(std::string{desc_field.str_name});
        continue;
      }
      if (rec_generated != nullptr) {
        const auto* fn_loader = FindGeneratedLoader(desc_field.str_loader);
        if (fn_loader == nullptr)
          UnknownLoader(desc_field.str_loader, desc_field.str_name);
        (*fn_loader)(rec_generated->Slot(int4_slot), yaml_my);
      } else {
        const auto it_loader = LoaderTable().find(desc_field.str_loader);
        if (it_loader == LoaderTable().end())
          UnknownLoader(desc_field.str_loader, desc_field.str_name);
        it_loader->second(FieldPtr(obj_record, desc_field), yaml_my);
      }
      continue;
    }

    const yaml::MiniYaml* yaml_found = find_md(desc_field.str_name);
    if (yaml_found == nullptr) {

      if (desc_field.b_required)
        vec_missing.push_back(std::string{desc_field.str_name});
      continue;
    }

    if (rec_generated != nullptr) {
      LoadValueIntoValue(desc_field, rec_generated->Slot(int4_slot), *yaml_found);
    } else {
      GenericValue val_tmp = GenericValue::Null();
      LoadValueIntoValue(desc_field, val_tmp, *yaml_found);
      AssignToMemory(desc_field, val_tmp, FieldPtr(obj_record, desc_field));
    }
  }

  if (!vec_missing.empty())
    throw MissingFieldsException(std::move(vec_missing));
}

std::int32_t GetInt32Value(std::string_view str_field, std::string_view sv_value) {
  std::int32_t int4_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseInt32Invariant(sv_trimmed, int4_v))
    DefaultInvalidValue(sv_trimmed, "System.Int32", str_field);
  return int4_v;
}

float GetFloatValue(std::string_view str_field, std::string_view sv_value) {
  float fp4_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseFloatOrPercentInvariant(sv_trimmed, fp4_v))
    DefaultInvalidValue(sv_trimmed, "System.Single", str_field);
  return fp4_v;
}

bool GetBoolValue(std::string_view str_field, std::string_view sv_value) {
  bool b_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseBoolNet(sv_trimmed, b_v))
    DefaultInvalidValue(sv_trimmed, "System.Boolean", str_field);
  return b_v;
}

std::vector<std::string> GetStringArrayValue(std::string_view str_field, std::string_view sv_value) {

  static constexpr FieldDesc kElemStr = ElemOf(FieldType::String);
  const FieldDesc desc_arr{
      .str_name = str_field, .type = FieldType::ImmutableArray, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = &kElemStr, .key = nullptr,
      .value = nullptr,
      .str_type_name =
          "System.Collections.Immutable.ImmutableArray`1[System.String]"};
  GenericValue val_out = GenericValue::Null();
  const std::string str_trimmed{TrimNetWhiteSpace(sv_value)};
  yaml::MiniYaml node_scratch{&str_trimmed, {}};
  LoadValueIntoValue(desc_arr, val_out, node_scratch);
  std::vector<std::string> vec_ret;
  for (const GenericValue& val_e : std::get<std::vector<GenericValue>>(val_out.val))
    vec_ret.push_back(std::get<std::string>(val_e.val));
  return vec_ret;
}

namespace {

GenericTuple GetTupleValue(std::string_view str_field, std::string_view sv_value, FieldType type_field,
                           std::string_view str_type_name) {
  const FieldDesc desc_field{
      .str_name = str_field, .type = type_field, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = nullptr, .key = nullptr,
      .value = nullptr, .str_type_name = str_type_name};
  GenericValue val_out = GenericValue::Null();
  const std::string str_trimmed{TrimNetWhiteSpace(sv_value)};
  yaml::MiniYaml node_scratch{&str_trimmed, {}};
  LoadValueIntoValue(desc_field, val_out, node_scratch);
  return std::get<GenericTuple>(val_out.val);
}

}

Rectangle GetRectangleValue(std::string_view str_field, std::string_view sv_value) {
  const GenericTuple t_v = GetTupleValue(str_field, sv_value, FieldType::Rectangle,
                                         "OpenRA.Primitives.Rectangle");
  return Rectangle{static_cast<std::int32_t>(t_v.arr_ints[0]),
                   static_cast<std::int32_t>(t_v.arr_ints[1]),
                   static_cast<std::int32_t>(t_v.arr_ints[2]),
                   static_cast<std::int32_t>(t_v.arr_ints[3])};
}

int2 GetSizeValue(std::string_view str_field, std::string_view sv_value) {
  const GenericTuple t_v =
      GetTupleValue(str_field, sv_value, FieldType::Size, "System.Drawing.Size");
  return int2{static_cast<std::int32_t>(t_v.arr_ints[0]), static_cast<std::int32_t>(t_v.arr_ints[1])};
}

core::Vector2 GetVector2Value(std::string_view str_field, std::string_view sv_value) {
  const GenericTuple t_v = GetTupleValue(str_field, sv_value, FieldType::Vector2,
                                         "System.Numerics.Vector2");
  return core::Vector2{t_v.arr_floats[0], t_v.arr_floats[1]};
}

namespace {

GenericValue GetScalarValue(std::string_view str_field, std::string_view sv_value, FieldType type_field,
                            std::string_view str_type_name) {
  const FieldDesc desc_field{
      .str_name = str_field, .type = type_field, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = nullptr, .key = nullptr,
      .value = nullptr, .str_type_name = str_type_name};
  GenericValue val_out = GenericValue::Null();
  const std::string str_trimmed{TrimNetWhiteSpace(sv_value)};
  yaml::MiniYaml node_scratch{&str_trimmed, {}};
  LoadValueIntoValue(desc_field, val_out, node_scratch);
  return val_out;
}

GenericValue GetArrayValue(std::string_view str_field, std::string_view sv_value,
                           const FieldDesc& desc_elem, std::string_view str_type_name) {
  const FieldDesc desc_arr{
      .str_name = str_field, .type = FieldType::ImmutableArray, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = &desc_elem, .key = nullptr,
      .value = nullptr, .str_type_name = str_type_name};
  GenericValue val_out = GenericValue::Null();
  const std::string str_trimmed{TrimNetWhiteSpace(sv_value)};
  yaml::MiniYaml node_scratch{&str_trimmed, {}};
  LoadValueIntoValue(desc_arr, val_out, node_scratch);
  return val_out;
}

}

std::int32_t GetWDistValue(std::string_view str_field, std::string_view sv_value) {

  return static_cast<std::int32_t>(
      std::get<std::int64_t>(GetScalarValue(str_field, sv_value, FieldType::WDist, "OpenRA.WDist").val));
}

core::Vector3 GetVector3Value(std::string_view str_field, std::string_view sv_value) {
  const GenericTuple t_v = GetTupleValue(str_field, sv_value, FieldType::Vector3,
                                         "System.Numerics.Vector3");
  return core::Vector3{t_v.arr_floats[0], t_v.arr_floats[1], t_v.arr_floats[2]};
}

core::Color GetColorValue(std::string_view str_field, std::string_view sv_value) {

  return core::Color::FromArgbRaw(
      static_cast<std::uint32_t>(std::get<std::int64_t>(
          GetScalarValue(str_field, sv_value, FieldType::Color, "OpenRA.Primitives.Color").val)));
}

std::vector<std::int32_t> GetInt32ArrayValue(std::string_view str_field, std::string_view sv_value) {
  static constexpr FieldDesc kElemInt32 = ElemOf(FieldType::Int32);
  std::vector<std::int32_t> vec_ret;
  for (const GenericValue& val_e :
       std::get<std::vector<GenericValue>>(
           GetArrayValue(str_field, sv_value, kElemInt32,
                         "System.Collections.Immutable.ImmutableArray`1[System.Int32]")
               .val))
    vec_ret.push_back(static_cast<std::int32_t>(std::get<std::int64_t>(val_e.val)));
  return vec_ret;
}

std::vector<float> GetFloatArrayValue(std::string_view str_field, std::string_view sv_value) {
  static constexpr FieldDesc kElemFloat = ElemOf(FieldType::Float);
  std::vector<float> vec_ret;
  for (const GenericValue& val_e :
       std::get<std::vector<GenericValue>>(
           GetArrayValue(str_field, sv_value, kElemFloat,
                         "System.Collections.Immutable.ImmutableArray`1[System.Single]")
               .val))
    vec_ret.push_back(std::get<float>(val_e.val));
  return vec_ret;
}

std::vector<std::pair<std::string, std::string>> GetStringDictionaryValue(
    const yaml::MiniYaml& yaml_node, std::string_view str_field) {

  static constexpr FieldDesc kElemString = ElemOf(FieldType::String);
  const FieldDesc desc_dict{
      .str_name = str_field, .type = FieldType::FrozenDictionary, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = nullptr, .key = &kElemString,
      .value = &kElemString,
      .str_type_name =
          "System.Collections.Frozen.FrozenDictionary`2[System.String,System.String]"};
  GenericValue val_out = GenericValue::Null();
  LoadValueIntoValue(desc_dict, val_out, yaml_node);

  std::vector<std::pair<std::string, std::string>> vec_ret;
  for (const auto& [val_k, val_v] : std::get<GenericDict>(val_out.val))
    vec_ret.emplace_back(std::get<std::string>(val_k.val), std::get<std::string>(val_v.val));
  return vec_ret;
}

std::int32_t GetEnumValue(std::string_view str_field, std::string_view sv_value,
                          std::string_view str_enum_full_name) {

  return static_cast<std::int32_t>(
      std::get<std::int64_t>(GetScalarValue(str_field, sv_value, FieldType::Enum,
                                            str_enum_full_name)
                                 .val));
}

}
