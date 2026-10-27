#pragma once
import std;

namespace ora::meta {

class TraitRecordBase;

enum class FieldType : std::uint8_t {

  Bool,
  Byte,
  UInt16,
  Int16,
  Int32,
  Float,
  String,
  Color,
  WDist,
  WVec,
  WPos,
  WAngle,
  WRot,
  CPos,
  CVec,
  Int2,
  Size,
  Vector2,
  Vector3,
  Rectangle,
  BooleanExpression,
  IntegerExpression,
  DateTime,
  Hotkey,
  HotkeyReference,
  Enum,

  Array,
  ImmutableArray,
  List,
  HashSet,
  FrozenSet,
  Dictionary,
  FrozenDictionary,
  BitSet,
  Nullable,

  Record,

  Opaque,

};

struct FieldDesc {
  std::string_view str_name;

  FieldType type;
  bool b_required;
  std::string_view str_loader;

  std::ptrdiff_t off_offset{};

  const FieldDesc* elem{};

  const FieldDesc* key{};

  const FieldDesc* value{};

  std::string_view str_type_name{};

};

constexpr FieldDesc ElemOf(FieldType type, std::string_view str_type_name = {}) {
  return FieldDesc{.str_name = {}, .type = type, .b_required = false, .str_loader = {},
                   .off_offset = 0, .elem = nullptr, .key = nullptr, .value = nullptr,
                   .str_type_name = str_type_name};
}

struct RecordDesc {
  std::string_view str_name;

  std::string_view str_full_name;

  std::string_view str_base;

  std::span<const FieldDesc> fields;

  std::span<const std::string_view> requires_types;
  std::span<const std::string_view> not_before_types;
  std::span<const std::string_view> interfaces;

};

#define ORA_FIELD(Class, Member, Type, Required)                        \
  ::ora::meta::FieldDesc{                                               \
      .str_name = #Member, .type = ::ora::meta::FieldType::Type,        \
      .b_required = (Required), .str_loader = {},                       \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr, \
      .key = nullptr, .value = nullptr, .str_type_name = {}}

#define ORA_FIELD_NAMED(Class, Member, Type, Required, TypeName)         \
  ::ora::meta::FieldDesc{                                                \
      .str_name = #Member, .type = ::ora::meta::FieldType::Type,         \
      .b_required = (Required), .str_loader = {},                        \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr,  \
      .key = nullptr, .value = nullptr, .str_type_name = TypeName}

#define ORA_FIELD_LOADER(Class, Member, Loader)                           \
  ::ora::meta::FieldDesc{                                                 \
      .str_name = #Member, .type = ::ora::meta::FieldType::Opaque,        \
      .b_required = false, .str_loader = #Loader,                         \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr,   \
      .key = nullptr, .value = nullptr, .str_type_name = #Loader}

}
