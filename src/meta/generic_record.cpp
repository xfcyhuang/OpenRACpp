import std;
#include "generic_record.hpp"
#include "field_loader.hpp"
#include "parse.hpp"
#include "variable_expression.hpp"
#include "core/bitset.hpp"
#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/cvec.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/text.hpp"
#include "core/vector_n.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"

namespace ora::meta {

namespace {

std::string_view DefaultTypeName(FieldType type) {
  switch (type) {
    case FieldType::Bool: return "System.Boolean";
    case FieldType::Byte: return "System.Byte";
    case FieldType::UInt16: return "System.UInt16";
    case FieldType::Int16: return "System.Int16";
    case FieldType::Int32: return "System.Int32";
    case FieldType::Float: return "System.Single";
    case FieldType::String: return "System.String";
    case FieldType::Color: return "OpenRA.Primitives.Color";
    case FieldType::WDist: return "OpenRA.WDist";
    case FieldType::WVec: return "OpenRA.WVec";
    case FieldType::WPos: return "OpenRA.WPos";
    case FieldType::WAngle: return "OpenRA.WAngle";
    case FieldType::WRot: return "OpenRA.WRot";
    case FieldType::CPos: return "OpenRA.CPos";
    case FieldType::CVec: return "OpenRA.CVec";
    case FieldType::Int2: return "OpenRA.int2";
    case FieldType::Size: return "System.Drawing.Size";
    case FieldType::Vector2: return "System.Numerics.Vector2";
    case FieldType::Vector3: return "System.Numerics.Vector3";
    case FieldType::Rectangle: return "OpenRA.Primitives.Rectangle";
    case FieldType::BooleanExpression: return "OpenRA.Support.BooleanExpression";
    case FieldType::IntegerExpression: return "OpenRA.Support.IntegerExpression";
    case FieldType::DateTime: return "System.DateTime";
    case FieldType::Hotkey: return "OpenRA.Hotkey";
    case FieldType::HotkeyReference: return "OpenRA.HotkeyReference";
    default: return {};
  }
}

[[noreturn]] void InvalidValueFor(const FieldDesc& desc, std::string_view sv_value) {
  const std::string_view sv_type =
      desc.str_type_name.empty() ? DefaultTypeName(desc.type) : desc.str_type_name;
  DefaultInvalidValue(sv_value, sv_type, desc.str_name);
}

std::string MakeExpressionText(const FieldDesc& desc, std::string_view sv_value) {
  try {
    return expr::VariableExpression{std::string{sv_value}}.Expression();
  } catch (const expr::ExpressionException& e_except) {
    throw yaml::YamlException(std::format(
        "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`: {}", sv_value,
        desc.str_name, desc.str_type_name, e_except.what()));
  }
}

GenericTuple ParseWTuple3(const FieldDesc& desc, std::string_view sv_value) {
  const auto fn_wdist = [](std::string_view sv_p, std::int32_t& int4_len) {
    const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_p);
    if (!exp_d.has_value())
      return false;
    int4_len = exp_d->Length;
    return true;
  };
  const auto fn_tuple = [](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z) {
    return GenericTuple{.arr_ints = {int4_x, int4_y, int4_z, 0}, .arr_floats = {}, .uint1_count = 3};
  };
  const auto opt_v = TryParseTuple3(sv_value, fn_wdist, fn_tuple);
  if (!opt_v.has_value())
    InvalidValueFor(desc, sv_value);
  return *opt_v;
}

struct KeyEqualsVisitor {
  bool operator()(std::monostate, std::monostate) const { return true; }
  bool operator()(std::int64_t int8_a, std::int64_t int8_b) const { return int8_a == int8_b; }
  bool operator()(const std::string& str_a, const std::string& str_b) const {
    return str_a == str_b;
  }
  bool operator()(float fp4_a, float fp4_b) const { return fp4_a == fp4_b; }
  bool operator()(const GenericTuple& t_a, const GenericTuple& t_b) const {
    return t_a.arr_ints == t_b.arr_ints && t_a.arr_floats == t_b.arr_floats;
  }
  bool operator()(const auto&, const auto&) const { return false; }
};

bool KeyEquals(const GenericValue& val_a, const GenericValue& val_b) {
  if (val_a.val.index() != val_b.val.index())
    return false;
  return std::visit(KeyEqualsVisitor{}, val_a.val, val_b.val);
}

}

GeneratedRecord::GeneratedRecord(const RecordDesc& desc,
                                 std::span<const GenericValue> defaults)
    : desc_{&desc} {
  vec_values_.reserve(defaults.size());
  for (const GenericValue& val_d : defaults)
    vec_values_.push_back(val_d);

}

namespace {

std::string_view InternLoaderKey(std::string_view sv) {
  static std::deque<std::string> deque_pool;
  return *deque_pool.emplace(deque_pool.end(), sv);
}

std::unordered_map<
    std::string_view,
    std::function<void(GenericValue& val_slot, const yaml::MiniYaml& trait_yaml)>>&
GeneratedLoaderTable() {
  static std::unordered_map<
      std::string_view,
      std::function<void(GenericValue& val_slot, const yaml::MiniYaml& trait_yaml)>>
      map_table;
  return map_table;
}
}

void RegisterGeneratedLoader(
    std::string_view str_loader,
    std::function<void(GenericValue& val_slot, const yaml::MiniYaml& trait_yaml)> fn_load) {
  GeneratedLoaderTable().insert_or_assign(InternLoaderKey(str_loader), std::move(fn_load));
}

void ClearGeneratedLoadersForTest() { GeneratedLoaderTable().clear(); }

const std::function<void(GenericValue&, const yaml::MiniYaml&)>* FindGeneratedLoader(
    std::string_view str_loader) {
  const auto it_find = GeneratedLoaderTable().find(str_loader);
  return it_find == GeneratedLoaderTable().end() ? nullptr : &it_find->second;
}

void LoadValueIntoValue(const FieldDesc& desc, GenericValue& val_out,
                        const yaml::MiniYaml& node) {
  const std::string_view sv_value =
      TrimNetWhiteSpace(node.Value != nullptr ? std::string_view{*node.Value}
                                              : std::string_view{});

  switch (desc.type) {

    case FieldType::Bool: {
      bool b_v{};
      if (!TryParseBoolNet(sv_value, b_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(b_v);
      return;
    }
    case FieldType::Byte: {
      std::uint8_t uint1_v{};
      if (!TryParseByteInvariant(sv_value, uint1_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{uint1_v});
      return;
    }
    case FieldType::UInt16: {
      std::uint16_t uint2_v{};
      if (!TryParseUInt16Invariant(sv_value, uint2_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{uint2_v});
      return;
    }
    case FieldType::Int16: {
      std::int16_t int2_v{};
      if (!TryParseInt16Invariant(sv_value, int2_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{int2_v});
      return;
    }
    case FieldType::Int32: {
      std::int32_t int4_v{};
      if (!TryParseInt32Invariant(sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{int4_v});
      return;
    }
    case FieldType::Enum: {
      std::int32_t int4_v{};
      if (!TryParseEnumNet(desc.str_type_name, sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{int4_v});
      return;
    }
    case FieldType::Float: {
      float fp4_v{};
      if (!TryParseFloatOrPercentInvariant(sv_value, fp4_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(fp4_v);
      return;
    }
    case FieldType::String:

      val_out = GenericValue::Of(std::string{sv_value});
      return;
    case FieldType::Color: {
      core::Color color_v{};
      if (!core::Color::TryParse(sv_value, color_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{color_v.argb});
      return;
    }
    case FieldType::WAngle: {
      std::int32_t int4_v{};
      if (!TryParseInt32Invariant(sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{int4_v});
      return;
    }
    case FieldType::WDist: {
      const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_value);
      if (!exp_d.has_value())
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(std::int64_t{exp_d->Length});
      return;
    }
    case FieldType::WVec:
      val_out = GenericValue::Of(ParseWTuple3(desc, sv_value));
      return;
    case FieldType::WPos:
      val_out = GenericValue::Of(ParseWTuple3(desc, sv_value));
      return;
    case FieldType::WRot: {
      const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
        return TryParseInt32Invariant(sv_p, int4_v);
      };
      const auto fn_tuple = [](std::int32_t int4_r, std::int32_t int4_p, std::int32_t int4_y) {
        return GenericTuple{.arr_ints = {int4_r, int4_p, int4_y, 0}, .arr_floats = {},
                            .uint1_count = 3};
      };
      const auto opt_v = TryParseTuple3(sv_value, fn_int, fn_tuple);
      if (!opt_v.has_value())
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(*opt_v);
      return;
    }
    case FieldType::CPos: {
      CPos cpos_v{};
      if (!TryParseCPosNet(sv_value, cpos_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(GenericTuple{
          .arr_ints = {cpos_v.X(), cpos_v.Y(), std::int64_t{cpos_v.Layer()}, 0},
          .arr_floats = {}, .uint1_count = 3});
      return;
    }
    case FieldType::CVec: {
      CVec cvec_v{};
      if (!TryParseCVecNet(sv_value, cvec_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(
          GenericTuple{.arr_ints = {cvec_v.X, cvec_v.Y, 0, 0}, .arr_floats = {}, .uint1_count = 2});
      return;
    }
    case FieldType::Int2: {
      int2 int2_v{};
      const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
        return TryParseInt32Invariant(sv_p, int4_v);
      };
      if (!TryParseTuple2(sv_value, fn_int, int2_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(
          GenericTuple{.arr_ints = {int2_v.X, int2_v.Y, 0, 0}, .arr_floats = {}, .uint1_count = 2});
      return;
    }
    case FieldType::Size: {

      int2 size_v{};
      const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
        return TryParseInt32Invariant(sv_p, int4_v);
      };
      if (!TryParseTuple2(sv_value, fn_int, size_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(
          GenericTuple{.arr_ints = {size_v.X, size_v.Y, 0, 0}, .arr_floats = {}, .uint1_count = 2});
      return;
    }
    case FieldType::Rectangle: {

      if (!sv_value.empty()) {
        const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv_value);
        std::int32_t int4_x{}, int4_y{}, int4_w{}, int4_h{};
        if (vec_parts.size() == 4 &&
            TryParseInt32Invariant(vec_parts[0], int4_x) &&
            TryParseInt32Invariant(vec_parts[1], int4_y) &&
            TryParseInt32Invariant(vec_parts[2], int4_w) &&
            TryParseInt32Invariant(vec_parts[3], int4_h)) {
          val_out = GenericValue::Of(
              GenericTuple{.arr_ints = {int4_x, int4_y, int4_w, int4_h}, .arr_floats = {},
                           .uint1_count = 4});
          return;
        }
      }
      InvalidValueFor(desc, sv_value);
    }
    case FieldType::Vector2: {
      core::Vector2 vec2_v{};
      const auto fn_float = [](std::string_view sv_p, float& fp4_v) {
        return TryParseFloatOrPercentInvariant(sv_p, fp4_v);
      };
      if (!TryParseTuple2(sv_value, fn_float, vec2_v))
        InvalidValueFor(desc, sv_value);
      val_out = GenericValue::Of(GenericTuple{
          .arr_ints = {}, .arr_floats = {vec2_v.X, vec2_v.Y, 0, 0}, .uint1_count = 2});
      return;
    }
    case FieldType::Vector3: {

      if (!sv_value.empty()) {
        const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv_value);
        float fp4_x{}, fp4_y{}, fp4_z{};
        if (vec_parts.size() == 3 &&
            TryParseFloatOrPercentInvariant(vec_parts[0], fp4_x) &&
            TryParseFloatOrPercentInvariant(vec_parts[1], fp4_y) &&
            TryParseFloatOrPercentInvariant(vec_parts[2], fp4_z)) {
          val_out = GenericValue::Of(
              GenericTuple{.arr_ints = {}, .arr_floats = {fp4_x, fp4_y, fp4_z, 0},
                           .uint1_count = 3});
          return;
        }
        if (vec_parts.size() == 2 &&
            TryParseFloatOrPercentInvariant(vec_parts[0], fp4_x) &&
            TryParseFloatOrPercentInvariant(vec_parts[1], fp4_y)) {
          val_out = GenericValue::Of(
              GenericTuple{.arr_ints = {}, .arr_floats = {fp4_x, fp4_y, 0, 0},
                           .uint1_count = 3});
          return;
        }
      }
      InvalidValueFor(desc, sv_value);
    }
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression:
      val_out = GenericValue::Of(MakeExpressionText(desc, sv_value));
      return;
    case FieldType::BitSet: {

      std::vector<std::string> vec_strs;
      if (!sv_value.empty()) {
        for (const std::string_view sv_part : SplitComma(sv_value)) {
          const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
          if (!sv_trimmed.empty())
            vec_strs.emplace_back(sv_trimmed);
        }
      }
      const std::uint64_t uint8_bits = BitsOf(desc.str_type_name, vec_strs);
      val_out = GenericValue::Of(std::int64_t{static_cast<std::int64_t>(uint8_bits)});
      return;
    }

    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {
      const FieldDesc& desc_elem = *desc.elem;
      const bool b_grouped = desc.type == FieldType::ImmutableArray &&
                             (desc_elem.type == FieldType::WVec ||
                              desc_elem.type == FieldType::CPos ||
                              desc_elem.type == FieldType::CVec ||
                              desc_elem.type == FieldType::Int2);
      if (b_grouped) {
        const std::size_t kWidth =
            desc_elem.type == FieldType::WVec ? 3 : 2;
        if (sv_value.empty())
          InvalidValueFor(desc, sv_value);

        const FieldType type_component =
            desc_elem.type == FieldType::WVec ? FieldType::WDist : FieldType::Int32;
        std::vector<GenericValue> vec_elems;
        std::array<std::int64_t, 3> arr_ints{};
        std::size_t int4_index{};
        for (const std::string_view sv_part : SplitComma(sv_value)) {
          const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
          if (sv_trimmed.empty())
            continue;

          const std::string str_trimmed{sv_trimmed};
          yaml::MiniYaml node_scratch{&str_trimmed, {}};
          GenericValue val_elem = GenericValue::Null();
          const FieldDesc desc_component = ElemOf(type_component);
          LoadValueIntoValue(desc_component, val_elem, node_scratch);
          arr_ints[int4_index++] = std::get<std::int64_t>(val_elem.val);
          if (int4_index == kWidth) {
            vec_elems.push_back(GenericValue::Of(
                GenericTuple{.arr_ints = {arr_ints[0], arr_ints[1], arr_ints[2], 0},
                             .arr_floats = {},
                             .uint1_count = static_cast<std::uint8_t>(kWidth)}));
            int4_index = 0;
          }
        }
        if (int4_index != 0)
          InvalidValueFor(desc, sv_value);
        val_out = GenericValue::Of(std::move(vec_elems));
        return;
      }

      std::vector<GenericValue> vec_elems;
      if (!sv_value.empty()) {
        for (const std::string_view sv_part : SplitComma(sv_value)) {
          const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
          if (sv_trimmed.empty())
            continue;

          const std::string str_trimmed{sv_trimmed};
          yaml::MiniYaml node_scratch{&str_trimmed, {}};
          GenericValue val_elem = GenericValue::Null();
          LoadValueIntoValue(desc_elem, val_elem, node_scratch);
          vec_elems.push_back(std::move(val_elem));
        }
      }
      val_out = GenericValue::Of(std::move(vec_elems));
      return;
    }

    case FieldType::Dictionary:
    case FieldType::FrozenDictionary: {
      const FieldDesc& desc_key = *desc.key;
      const FieldDesc& desc_val = *desc.value;
      GenericDict map_ret;
      for (const yaml::MiniYamlNode& node_entry : node.Nodes) {
        GenericValue val_key = GenericValue::Null();
        const std::string_view sv_key =
            TrimNetWhiteSpace(node_entry.Key != nullptr ? std::string_view{*node_entry.Key}
                                                        : std::string_view{});
        {
          const std::string str_key{sv_key};
          yaml::MiniYaml node_scratch{&str_key, {}};
          LoadValueIntoValue(desc_key, val_key, node_scratch);
        }

        GenericValue val_entry = GenericValue::Null();
        {

          LoadValueIntoValue(desc_val, val_entry, node_entry.Value);
        }

        for (const auto& [val_existing, val_ignored] : map_ret) {
          (void)val_ignored;
          if (KeyEquals(val_existing, val_key))
            throw yaml::YamlException("An item with the same key has already been added.");
        }
        map_ret.emplace_back(std::move(val_key), std::move(val_entry));
      }
      val_out = GenericValue::Of(std::move(map_ret));
      return;
    }

    case FieldType::Nullable: {
      if (sv_value.empty()) {
        val_out = GenericValue::Null();
        return;
      }
      LoadValueIntoValue(*desc.elem, val_out, node);
      return;
    }

    case FieldType::DateTime:
    case FieldType::Hotkey:
    case FieldType::HotkeyReference:
    case FieldType::Record:
    case FieldType::Opaque:

      DefaultUnknownField(desc.str_name);
  }
  std::unreachable();
}

namespace {

template <class T>
void AssignInt(const GenericValue& val, void* p_field) {
  *reinterpret_cast<T*>(p_field) = static_cast<T>(std::get<std::int64_t>(val.val));
}

}

void AssignToMemory(const FieldDesc& desc, const GenericValue& val, void* p_field) {
  switch (desc.type) {
    case FieldType::Bool:
      *reinterpret_cast<bool*>(p_field) = std::get<bool>(val.val);
      return;
    case FieldType::Byte:
      AssignInt<std::uint8_t>(val, p_field);
      return;
    case FieldType::UInt16:
      AssignInt<std::uint16_t>(val, p_field);
      return;
    case FieldType::Int16:
      AssignInt<std::int16_t>(val, p_field);
      return;
    case FieldType::Int32:
    case FieldType::Enum:
      AssignInt<std::int32_t>(val, p_field);
      return;
    case FieldType::Float:
      *reinterpret_cast<float*>(p_field) = std::get<float>(val.val);
      return;
    case FieldType::String:
      *reinterpret_cast<std::string*>(p_field) = std::get<std::string>(val.val);
      return;
    case FieldType::Color:
      *reinterpret_cast<core::Color*>(p_field) =
          core::Color::FromArgbRaw(static_cast<std::uint32_t>(std::get<std::int64_t>(val.val)));
      return;
    case FieldType::WAngle:
      *reinterpret_cast<WAngle*>(p_field) =
          WAngle{static_cast<std::int32_t>(std::get<std::int64_t>(val.val))};
      return;
    case FieldType::WDist:
      *reinterpret_cast<WDist*>(p_field) =
          WDist{static_cast<std::int32_t>(std::get<std::int64_t>(val.val))};
      return;
    case FieldType::WVec:
    case FieldType::WPos: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<WVec*>(p_field) =
          WVec{static_cast<std::int32_t>(t_v.arr_ints[0]),
               static_cast<std::int32_t>(t_v.arr_ints[1]),
               static_cast<std::int32_t>(t_v.arr_ints[2])};
      return;
    }
    case FieldType::WRot: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<WRot*>(p_field) =
          WRot{WAngle{static_cast<std::int32_t>(t_v.arr_ints[0])},
               WAngle{static_cast<std::int32_t>(t_v.arr_ints[1])},
               WAngle{static_cast<std::int32_t>(t_v.arr_ints[2])}};
      return;
    }
    case FieldType::CPos: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<CPos*>(p_field) =
          CPos{static_cast<std::int32_t>(t_v.arr_ints[0]),
               static_cast<std::int32_t>(t_v.arr_ints[1]),
               static_cast<std::uint8_t>(t_v.arr_ints[2])};
      return;
    }
    case FieldType::CVec: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<CVec*>(p_field) = CVec{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                               static_cast<std::int32_t>(t_v.arr_ints[1])};
      return;
    }
    case FieldType::Int2: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<int2*>(p_field) = int2{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                               static_cast<std::int32_t>(t_v.arr_ints[1])};
      return;
    }
    case FieldType::Size: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<int2*>(p_field) = int2{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                               static_cast<std::int32_t>(t_v.arr_ints[1])};
      return;
    }
    case FieldType::Rectangle: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<Rectangle*>(p_field) =
          Rectangle{static_cast<std::int32_t>(t_v.arr_ints[0]),
                    static_cast<std::int32_t>(t_v.arr_ints[1]),
                    static_cast<std::int32_t>(t_v.arr_ints[2]),
                    static_cast<std::int32_t>(t_v.arr_ints[3])};
      return;
    }
    case FieldType::Vector2: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<core::Vector2*>(p_field) = core::Vector2{t_v.arr_floats[0], t_v.arr_floats[1]};
      return;
    }
    case FieldType::Vector3: {
      const GenericTuple& t_v = std::get<GenericTuple>(val.val);
      *reinterpret_cast<core::Vector3*>(p_field) =
          core::Vector3{t_v.arr_floats[0], t_v.arr_floats[1], t_v.arr_floats[2]};
      return;
    }
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression:
      *reinterpret_cast<std::optional<expr::VariableExpression>*>(p_field) =
          std::holds_alternative<std::string>(val.val)
              ? std::optional<expr::VariableExpression>{expr::VariableExpression{
                    std::get<std::string>(val.val)}}
              : std::optional<expr::VariableExpression>{};
      return;
    case FieldType::BitSet:
      *reinterpret_cast<std::uint64_t*>(p_field) =
          static_cast<std::uint64_t>(std::get<std::int64_t>(val.val));
      return;
    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {
      const std::vector<GenericValue>& vec_vals = std::get<std::vector<GenericValue>>(val.val);
      const FieldDesc& desc_elem = *desc.elem;
      switch (desc_elem.type) {
        case FieldType::Int32:
        case FieldType::Enum: {
          std::vector<std::int32_t> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(static_cast<std::int32_t>(std::get<std::int64_t>(val_e.val)));
          *reinterpret_cast<std::vector<std::int32_t>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::Byte: {
          std::vector<std::uint8_t> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(static_cast<std::uint8_t>(std::get<std::int64_t>(val_e.val)));
          *reinterpret_cast<std::vector<std::uint8_t>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::UInt16: {
          std::vector<std::uint16_t> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(static_cast<std::uint16_t>(std::get<std::int64_t>(val_e.val)));
          *reinterpret_cast<std::vector<std::uint16_t>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::Int16: {
          std::vector<std::int16_t> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(static_cast<std::int16_t>(std::get<std::int64_t>(val_e.val)));
          *reinterpret_cast<std::vector<std::int16_t>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::Float: {
          std::vector<float> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(std::get<float>(val_e.val));
          *reinterpret_cast<std::vector<float>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::String: {
          std::vector<std::string> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(std::get<std::string>(val_e.val));
          *reinterpret_cast<std::vector<std::string>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::Color: {
          std::vector<core::Color> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(
                core::Color::FromArgbRaw(static_cast<std::uint32_t>(std::get<std::int64_t>(val_e.val))));
          *reinterpret_cast<std::vector<core::Color>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::WDist: {
          std::vector<WDist> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(WDist{static_cast<std::int32_t>(std::get<std::int64_t>(val_e.val))});
          *reinterpret_cast<std::vector<WDist>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::WAngle: {
          std::vector<WAngle> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(WAngle{static_cast<std::int32_t>(std::get<std::int64_t>(val_e.val))});
          *reinterpret_cast<std::vector<WAngle>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::BitSet: {
          std::vector<std::uint64_t> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals)
            vec_t.push_back(static_cast<std::uint64_t>(std::get<std::int64_t>(val_e.val)));
          *reinterpret_cast<std::vector<std::uint64_t>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::WVec: {
          std::vector<WVec> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_e.val);
            vec_t.push_back(WVec{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                 static_cast<std::int32_t>(t_v.arr_ints[1]),
                                 static_cast<std::int32_t>(t_v.arr_ints[2])});
          }
          *reinterpret_cast<std::vector<WVec>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::CPos: {
          std::vector<CPos> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_e.val);
            vec_t.push_back(CPos{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                 static_cast<std::int32_t>(t_v.arr_ints[1]),
                                 static_cast<std::uint8_t>(t_v.arr_ints[2])});
          }
          *reinterpret_cast<std::vector<CPos>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::CVec: {
          std::vector<CVec> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_e.val);
            vec_t.push_back(CVec{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                 static_cast<std::int32_t>(t_v.arr_ints[1])});
          }
          *reinterpret_cast<std::vector<CVec>*>(p_field) = std::move(vec_t);
          return;
        }
        case FieldType::Int2: {
          std::vector<int2> vec_t;
          vec_t.reserve(vec_vals.size());
          for (const GenericValue& val_e : vec_vals) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_e.val);
            vec_t.push_back(int2{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                 static_cast<std::int32_t>(t_v.arr_ints[1])});
          }
          *reinterpret_cast<std::vector<int2>*>(p_field) = std::move(vec_t);
          return;
        }
        default:
          DefaultUnknownField(desc.str_name);
      }
    }

    case FieldType::Dictionary:
    case FieldType::FrozenDictionary: {
      const GenericDict& map_vals = std::get<GenericDict>(val.val);
      const FieldDesc& desc_key = *desc.key;
      const FieldDesc& desc_val = *desc.value;

      const auto assign_pair = [&]<class K, class V>(K&&, V&&) {
        std::vector<std::pair<K, V>> vec_t;
        vec_t.reserve(map_vals.size());
        for (const auto& [val_k, val_v] : map_vals) {
          std::pair<K, V> pair_t{};
          if constexpr (std::same_as<K, std::int32_t>) {
            pair_t.first = static_cast<std::int32_t>(std::get<std::int64_t>(val_k.val));
          } else if constexpr (std::same_as<K, std::string>) {
            pair_t.first = std::get<std::string>(val_k.val);
          } else if constexpr (std::same_as<K, std::optional<expr::VariableExpression>>) {
            if (std::holds_alternative<std::string>(val_k.val))
              pair_t.first = expr::VariableExpression{std::get<std::string>(val_k.val)};
          } else if constexpr (std::same_as<K, CPos>) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_k.val);
            pair_t.first = CPos{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                static_cast<std::int32_t>(t_v.arr_ints[1]),
                                static_cast<std::uint8_t>(t_v.arr_ints[2])};
          } else if constexpr (std::same_as<K, CVec>) {
            const GenericTuple& t_v = std::get<GenericTuple>(val_k.val);
            pair_t.first = CVec{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                static_cast<std::int32_t>(t_v.arr_ints[1])};
          }

          if constexpr (std::same_as<V, std::int32_t>) {
            pair_t.second = static_cast<std::int32_t>(std::get<std::int64_t>(val_v.val));
          } else if constexpr (std::same_as<V, std::string>) {
            pair_t.second = std::get<std::string>(val_v.val);
          } else if constexpr (std::same_as<V, float>) {
            pair_t.second = std::get<float>(val_v.val);
          } else if constexpr (std::same_as<V, Rectangle>) {

            const GenericTuple& t_v = std::get<GenericTuple>(val_v.val);
            pair_t.second = Rectangle{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                      static_cast<std::int32_t>(t_v.arr_ints[1]),
                                      static_cast<std::int32_t>(t_v.arr_ints[2]),
                                      static_cast<std::int32_t>(t_v.arr_ints[3])};
          } else if constexpr (std::same_as<V, int2>) {

            const GenericTuple& t_v = std::get<GenericTuple>(val_v.val);
            pair_t.second = int2{static_cast<std::int32_t>(t_v.arr_ints[0]),
                                 static_cast<std::int32_t>(t_v.arr_ints[1])};
          } else if constexpr (std::same_as<V, std::optional<expr::VariableExpression>>) {
            if (std::holds_alternative<std::string>(val_v.val))
              pair_t.second = expr::VariableExpression{std::get<std::string>(val_v.val)};
          } else {

            using E = typename V::value_type;
            V vec_v;
            const std::vector<GenericValue>& vec_elems =
                std::get<std::vector<GenericValue>>(val_v.val);
            vec_v.reserve(vec_elems.size());
            for (const GenericValue& val_e : vec_elems) {
              if constexpr (std::same_as<E, std::int32_t>)
                vec_v.push_back(static_cast<std::int32_t>(std::get<std::int64_t>(val_e.val)));
              else if constexpr (std::same_as<E, std::string>)
                vec_v.push_back(std::get<std::string>(val_e.val));
              else if constexpr (std::same_as<E, float>)
                vec_v.push_back(std::get<float>(val_e.val));
              else
                DefaultUnknownField(desc.str_name);
            }
            pair_t.second = std::move(vec_v);
          }
          vec_t.push_back(std::move(pair_t));
        }
        *reinterpret_cast<std::vector<std::pair<K, V>>*>(p_field) = std::move(vec_t);
      };

      const auto dispatch_value = [&]<class K>(K&&) {
        switch (desc_val.type) {
          case FieldType::Int32:
          case FieldType::Enum:
            assign_pair(K{}, std::int32_t{});
            break;
          case FieldType::String:
            assign_pair(K{}, std::string{});
            break;
          case FieldType::Float:
            assign_pair(K{}, float{});
            break;
          case FieldType::Rectangle:
            assign_pair(K{}, Rectangle{});
            break;
          case FieldType::Int2:
          case FieldType::Size:
            assign_pair(K{}, int2{});
            break;
          case FieldType::BooleanExpression:
          case FieldType::IntegerExpression:
            assign_pair(K{}, std::optional<expr::VariableExpression>{});
            break;
          case FieldType::Array:
          case FieldType::ImmutableArray:
          case FieldType::List:
          case FieldType::HashSet:
          case FieldType::FrozenSet: {
            const FieldType type_elem = desc_val.elem->type;
            if (type_elem == FieldType::String)
              assign_pair(K{}, std::vector<std::string>{});
            else if (type_elem == FieldType::Int32 || type_elem == FieldType::Enum)
              assign_pair(K{}, std::vector<std::int32_t>{});
            else if (type_elem == FieldType::Float)
              assign_pair(K{}, std::vector<float>{});
            else
              DefaultUnknownField(desc.str_name);
            break;
          }
          default:
            DefaultUnknownField(desc.str_name);
        }
      };

      switch (desc_key.type) {
        case FieldType::Int32:
        case FieldType::Enum:
          dispatch_value(std::int32_t{});
          break;
        case FieldType::String:
          dispatch_value(std::string{});
          break;
        case FieldType::BooleanExpression:
        case FieldType::IntegerExpression:
          dispatch_value(std::optional<expr::VariableExpression>{});
          break;
        case FieldType::CPos:
          dispatch_value(CPos{});
          break;
        case FieldType::CVec:
          dispatch_value(CVec{});
          break;
        default:
          DefaultUnknownField(desc.str_name);
      }
      return;
    }

    case FieldType::Nullable: {
      auto* p_opt = static_cast<void*>(p_field);
      if (val.IsNull()) {
        switch (desc.elem->type) {
          case FieldType::Color:
            reinterpret_cast<std::optional<core::Color>*>(p_opt)->reset();
            return;
          case FieldType::WAngle:
            reinterpret_cast<std::optional<WAngle>*>(p_opt)->reset();
            return;
          case FieldType::WDist:
            reinterpret_cast<std::optional<WDist>*>(p_opt)->reset();
            return;
          case FieldType::Bool:
            reinterpret_cast<std::optional<bool>*>(p_opt)->reset();
            return;
          default:
            DefaultUnknownField(desc.str_name);
        }
      }
      switch (desc.elem->type) {
        case FieldType::Color:
          *reinterpret_cast<std::optional<core::Color>*>(p_opt) = core::Color::FromArgbRaw(
              static_cast<std::uint32_t>(std::get<std::int64_t>(val.val)));
          return;
        case FieldType::WAngle:
          *reinterpret_cast<std::optional<WAngle>*>(p_opt) =
              WAngle{static_cast<std::int32_t>(std::get<std::int64_t>(val.val))};
          return;
        case FieldType::WDist:
          *reinterpret_cast<std::optional<WDist>*>(p_opt) =
              WDist{static_cast<std::int32_t>(std::get<std::int64_t>(val.val))};
          return;
        case FieldType::Bool:
          *reinterpret_cast<std::optional<bool>*>(p_opt) = std::get<bool>(val.val);
          return;
        default:
          DefaultUnknownField(desc.str_name);
      }
    }

    case FieldType::Record:

      DefaultUnknownField(desc.str_name);

    case FieldType::DateTime:
    case FieldType::Hotkey:
    case FieldType::HotkeyReference:
    case FieldType::Opaque:
      DefaultUnknownField(desc.str_name);
  }
  std::unreachable();
}

GenericValue ReadFromMemory(const FieldDesc& desc, const void* p_field) {
  switch (desc.type) {
    case FieldType::Bool:
      return GenericValue::Of(*reinterpret_cast<const bool*>(p_field));
    case FieldType::Byte:
      return GenericValue::Of(std::int64_t{*reinterpret_cast<const std::uint8_t*>(p_field)});
    case FieldType::UInt16:
      return GenericValue::Of(std::int64_t{*reinterpret_cast<const std::uint16_t*>(p_field)});
    case FieldType::Int16:
      return GenericValue::Of(std::int64_t{*reinterpret_cast<const std::int16_t*>(p_field)});
    case FieldType::Int32:
    case FieldType::Enum:
    case FieldType::WDist:
    case FieldType::WAngle:
      return GenericValue::Of(std::int64_t{*reinterpret_cast<const std::int32_t*>(p_field)});
    case FieldType::Float:
      return GenericValue::Of(*reinterpret_cast<const float*>(p_field));
    case FieldType::String:
      return GenericValue::Of(*reinterpret_cast<const std::string*>(p_field));
    case FieldType::Color:
      return GenericValue::Of(
          std::int64_t{reinterpret_cast<const core::Color*>(p_field)->argb});
    case FieldType::WVec:
    case FieldType::WPos: {
      const WVec& vec_w = *reinterpret_cast<const WVec*>(p_field);
      return GenericValue::Of(GenericTuple{
          .arr_ints = {vec_w.X, vec_w.Y, vec_w.Z, 0}, .arr_floats = {}, .uint1_count = 3});
    }
    case FieldType::WRot: {
      const WRot& rot_w = *reinterpret_cast<const WRot*>(p_field);
      return GenericValue::Of(GenericTuple{
          .arr_ints = {rot_w.Roll.Angle, rot_w.Pitch.Angle, rot_w.Yaw.Angle, 0},
          .arr_floats = {}, .uint1_count = 3});
    }
    case FieldType::CPos: {
      const CPos& pos_c = *reinterpret_cast<const CPos*>(p_field);
      return GenericValue::Of(GenericTuple{
          .arr_ints = {pos_c.X(), pos_c.Y(), std::int64_t{pos_c.Layer()}, 0},
          .arr_floats = {}, .uint1_count = 3});
    }
    case FieldType::CVec: {
      const CVec& vec_c = *reinterpret_cast<const CVec*>(p_field);
      return GenericValue::Of(
          GenericTuple{.arr_ints = {vec_c.X, vec_c.Y, 0, 0}, .arr_floats = {}, .uint1_count = 2});
    }
    case FieldType::Int2: {
      const int2& int2_v = *reinterpret_cast<const int2*>(p_field);
      return GenericValue::Of(
          GenericTuple{.arr_ints = {int2_v.X, int2_v.Y, 0, 0}, .arr_floats = {}, .uint1_count = 2});
    }
    case FieldType::Vector2: {
      const core::Vector2& vec_v = *reinterpret_cast<const core::Vector2*>(p_field);
      return GenericValue::Of(
          GenericTuple{.arr_ints = {}, .arr_floats = {vec_v.X, vec_v.Y, 0, 0}, .uint1_count = 2});
    }
    case FieldType::Vector3: {
      const core::Vector3& vec_v = *reinterpret_cast<const core::Vector3*>(p_field);
      return GenericValue::Of(GenericTuple{
          .arr_ints = {}, .arr_floats = {vec_v.X, vec_v.Y, vec_v.Z, 0}, .uint1_count = 3});
    }
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression: {
      const auto& opt_expr =
          *reinterpret_cast<const std::optional<expr::VariableExpression>*>(p_field);
      return opt_expr.has_value() ? GenericValue::Of(opt_expr->Expression())
                                  : GenericValue::Null();
    }
    case FieldType::BitSet:
      return GenericValue::Of(
          static_cast<std::int64_t>(*reinterpret_cast<const std::uint64_t*>(p_field)));

    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {

      const FieldType type_elem = desc.elem->type;
      auto read_int_vec = [&]<class E>() {
        std::vector<GenericValue> vec_vals;
        for (const E e : *reinterpret_cast<const std::vector<E>*>(p_field))
          vec_vals.push_back(GenericValue::Of(std::int64_t{e}));
        return GenericValue::Of(std::move(vec_vals));
      };
      switch (type_elem) {
        case FieldType::Int32:
        case FieldType::Enum:
        case FieldType::WAngle:
          return read_int_vec.template operator()<std::int32_t>();
        case FieldType::Byte:
          return read_int_vec.template operator()<std::uint8_t>();
        case FieldType::UInt16:
          return read_int_vec.template operator()<std::uint16_t>();
        case FieldType::Int16:
          return read_int_vec.template operator()<std::int16_t>();
        case FieldType::WDist: {
          std::vector<GenericValue> vec_vals;
          for (const WDist dist_e : *reinterpret_cast<const std::vector<WDist>*>(p_field))
            vec_vals.push_back(GenericValue::Of(std::int64_t{dist_e.Length}));
          return GenericValue::Of(std::move(vec_vals));
        }
        case FieldType::Float: {
          std::vector<GenericValue> vec_vals;
          for (const float fp4_e : *reinterpret_cast<const std::vector<float>*>(p_field))
            vec_vals.push_back(GenericValue::Of(fp4_e));
          return GenericValue::Of(std::move(vec_vals));
        }
        case FieldType::String: {
          std::vector<GenericValue> vec_vals;
          for (const std::string& str_e :
               *reinterpret_cast<const std::vector<std::string>*>(p_field))
            vec_vals.push_back(GenericValue::Of(str_e));
          return GenericValue::Of(std::move(vec_vals));
        }
        case FieldType::Color: {
          std::vector<GenericValue> vec_vals;
          for (const core::Color color_e :
               *reinterpret_cast<const std::vector<core::Color>*>(p_field))
            vec_vals.push_back(GenericValue::Of(std::int64_t{color_e.argb}));
          return GenericValue::Of(std::move(vec_vals));
        }
        case FieldType::BitSet: {
          std::vector<GenericValue> vec_vals;
          for (const std::uint64_t uint8_e :
               *reinterpret_cast<const std::vector<std::uint64_t>*>(p_field))
            vec_vals.push_back(GenericValue::Of(static_cast<std::int64_t>(uint8_e)));
          return GenericValue::Of(std::move(vec_vals));
        }
        case FieldType::WVec: {
          std::vector<GenericValue> vec_vals;
          for (const WVec& vec_e : *reinterpret_cast<const std::vector<WVec>*>(p_field)) {
            const FieldDesc desc_wvec = ElemOf(FieldType::WVec);
            GenericValue val_tmp = GenericValue::Null();
            void* p_elem = const_cast<void*>(static_cast<const void*>(&vec_e));
            val_tmp = ReadFromMemory(desc_wvec, p_elem);
            vec_vals.push_back(std::move(val_tmp));
          }
          return GenericValue::Of(std::move(vec_vals));
        }
        default:
          return GenericValue::Null();
      }
    }

    case FieldType::Dictionary:
    case FieldType::FrozenDictionary: {

      const FieldDesc& desc_key = *desc.key;
      const FieldDesc& desc_val = *desc.value;
      GenericDict map_ret;

      const auto read_pairs = [&]<class K, class V>() {
        for (const std::pair<K, V>& pair_kv :
             *reinterpret_cast<const std::vector<std::pair<K, V>>*>(p_field)) {
          GenericValue val_k = GenericValue::Null();
          if constexpr (std::same_as<K, std::int32_t>) {
            (void)desc_key;
            val_k = GenericValue::Of(std::int64_t{pair_kv.first});
          } else if constexpr (std::same_as<K, std::string>) {
            (void)desc_key;
            val_k = GenericValue::Of(pair_kv.first);
          } else if constexpr (std::same_as<K, std::optional<expr::VariableExpression>>) {
            (void)desc_key;
            if (pair_kv.first.has_value())
              val_k = GenericValue::Of(pair_kv.first->Expression());
          } else if constexpr (std::same_as<K, CPos>) {
            (void)desc_key;
            val_k = GenericValue::Of(GenericTuple{
                .arr_ints = {pair_kv.first.X(), pair_kv.first.Y(),
                             std::int64_t{pair_kv.first.Layer()}, 0},
                .arr_floats = {}, .uint1_count = 3});
          } else if constexpr (std::same_as<K, CVec>) {
            (void)desc_key;
            val_k = GenericValue::Of(
                GenericTuple{.arr_ints = {pair_kv.first.X, pair_kv.first.Y, 0, 0},
                             .arr_floats = {}, .uint1_count = 2});
          }

          GenericValue val_v = GenericValue::Null();
          if constexpr (std::same_as<V, std::int32_t>) {
            (void)desc_val;
            val_v = GenericValue::Of(std::int64_t{pair_kv.second});
          } else if constexpr (std::same_as<V, std::string>) {
            (void)desc_val;
            val_v = GenericValue::Of(pair_kv.second);
          } else if constexpr (std::same_as<V, float>) {
            (void)desc_val;
            val_v = GenericValue::Of(pair_kv.second);
          } else if constexpr (std::same_as<V, std::optional<expr::VariableExpression>>) {
            (void)desc_val;
            if (pair_kv.second.has_value())
              val_v = GenericValue::Of(pair_kv.second->Expression());
          } else {

            using E = typename V::value_type;
            const FieldDesc desc_elem = ElemOf(desc_val.elem->type);
            std::vector<GenericValue> vec_vals;
            for (const E e : pair_kv.second) {
              void* p_elem = const_cast<void*>(static_cast<const void*>(&e));
              vec_vals.push_back(ReadFromMemory(desc_elem, p_elem));
            }
            val_v = GenericValue::Of(std::move(vec_vals));
          }
          map_ret.emplace_back(std::move(val_k), std::move(val_v));
        }
      };

      const auto dispatch_value = [&]<class K>(K&&) {
        switch (desc_val.type) {
          case FieldType::Int32:
          case FieldType::Enum:
            read_pairs.template operator()<K, std::int32_t>();
            break;
          case FieldType::String:
            read_pairs.template operator()<K, std::string>();
            break;
          case FieldType::Float:
            read_pairs.template operator()<K, float>();
            break;
          case FieldType::BooleanExpression:
          case FieldType::IntegerExpression:
            read_pairs.template operator()<K, std::optional<expr::VariableExpression>>();
            break;
          case FieldType::Array:
          case FieldType::ImmutableArray:
          case FieldType::List:
          case FieldType::HashSet:
          case FieldType::FrozenSet: {
            const FieldType type_elem = desc_val.elem->type;
            if (type_elem == FieldType::String)
              read_pairs.template operator()<K, std::vector<std::string>>();
            else if (type_elem == FieldType::Int32 || type_elem == FieldType::Enum)
              read_pairs.template operator()<K, std::vector<std::int32_t>>();
            else if (type_elem == FieldType::Float)
              read_pairs.template operator()<K, std::vector<float>>();
            break;
          }
          default:
            break;
        }
      };

      switch (desc_key.type) {
        case FieldType::Int32:
        case FieldType::Enum:
          dispatch_value(std::int32_t{});
          break;
        case FieldType::String:
          dispatch_value(std::string{});
          break;
        case FieldType::BooleanExpression:
        case FieldType::IntegerExpression:
          dispatch_value(std::optional<expr::VariableExpression>{});
          break;
        case FieldType::CPos:
          dispatch_value(CPos{});
          break;
        case FieldType::CVec:
          dispatch_value(CVec{});
          break;
        default:
          break;
      }
      return GenericValue::Of(std::move(map_ret));
    }

    case FieldType::Nullable: {
      const FieldDesc& desc_inner = *desc.elem;
      switch (desc_inner.type) {
        case FieldType::Color: {
          const auto& opt_v = *reinterpret_cast<const std::optional<core::Color>*>(p_field);
          return opt_v.has_value()
                     ? GenericValue::Of(std::int64_t{opt_v->argb})
                     : GenericValue::Null();
        }
        case FieldType::WAngle: {
          const auto& opt_v = *reinterpret_cast<const std::optional<WAngle>*>(p_field);
          return opt_v.has_value()
                     ? GenericValue::Of(std::int64_t{opt_v->Angle})
                     : GenericValue::Null();
        }
        case FieldType::WDist: {
          const auto& opt_v = *reinterpret_cast<const std::optional<WDist>*>(p_field);
          return opt_v.has_value()
                     ? GenericValue::Of(std::int64_t{opt_v->Length})
                     : GenericValue::Null();
        }
        case FieldType::Bool: {
          const auto& opt_v = *reinterpret_cast<const std::optional<bool>*>(p_field);
          return opt_v.has_value() ? GenericValue::Of(*opt_v) : GenericValue::Null();
        }
        default:
          return GenericValue::Null();
      }
    }

    case FieldType::Record:
    case FieldType::Opaque:
    case FieldType::DateTime:
    case FieldType::Hotkey:
    case FieldType::HotkeyReference:
    case FieldType::Size:
    case FieldType::Rectangle:

      return GenericValue::Null();
  }
  std::unreachable();
}

}
