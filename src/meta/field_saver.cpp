// UPSTREAM: OpenRA.Game/FieldSaver.cs @b6fc03f L26-166(实现部分;头注见
//          field_saver.hpp)
//          The implementation half of FieldSaver.cs L26-166 (the header
//          note lives in field_saver.hpp).
#include "meta/field_saver.hpp"

#include "meta/dump_format.hpp"
#include "meta/field_loader.hpp"

namespace ora::meta {

std::string SaveFormatValue(const FieldDesc& desc, const GenericValue& val) {
  // L72-160 的值袋分派(null → "";集合/元组的逗号拼接 = 上游
  // JoinWith(", "))
  // The value-bag dispatch (null → ""; the collections/tuples join like
  // upstream's JoinWith(", ")).
  if (val.IsNull())
    return "";

  switch (desc.type) {
    case FieldType::Bool:
      return std::get<bool>(val.val) ? "True" : "False";

    case FieldType::String:
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression:
      return std::get<std::string>(val.val);

    case FieldType::BitSet: {
      // L78-79:标签名 ", " 连接(空集 = 空串)
      // L78-79: the tag names joined ", " (an empty set = "").
      const std::vector<std::string> vec_strs = StringsOfBits(
          desc.str_type_name,
          static_cast<std::uint64_t>(std::get<std::int64_t>(val.val)));
      std::string str_ret;
      bool b_first = true;
      for (const std::string& str_s : vec_strs) {
        if (!b_first)
          str_ret += ", ";
        b_first = false;
        str_ret += str_s;
      }
      return str_ret;
    }

    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {
      // L81-99:逐元素格式化后 ", " 连接
      // L81-99: the elements formatted then joined ", ".
      std::string str_ret;
      bool b_first = true;
      if (const auto* arr =
              std::get_if<std::vector<GenericValue>>(&val.val)) {
        for (const auto& element : *arr) {
          if (!b_first)
            str_ret += ", ";
          b_first = false;
          str_ret.append(SaveFormatValue(*desc.elem, element));
        }
      }
      return str_ret;
    }

    case FieldType::Enum:
      // .NET EnumConverter.ConvertToInvariantString = 底层值
      // .NET EnumConverter.ConvertToInvariantString = the underlying value.
      return std::to_string(
          static_cast<std::int32_t>(std::get<std::int64_t>(val.val)));

    case FieldType::Float:
      return FormatFloatNet(std::get<float>(val.val));

    case FieldType::Nullable:
      return desc.elem != nullptr
                 ? SaveFormatValue(*desc.elem, val)
                 : std::string{};

    case FieldType::Record:
      // 嵌套记录字段(Save 的字典子节点形态专管;标量面不达)
      // A nested-record field (Save's dictionary sub-node form owns it;
      // the scalar face is unreachable).
      return std::string{};

    default:
      break;
  }

  // 元组族(WDist/WVec/WPos/WAngle/WRot/CPos/CVec/Int2/Size/Rectangle)与
  // 整型族(Byte/UInt16/Int16/Int32/Color/Hotkey/DateTime/Vector2/3):上游
  // TypeConverter 的不变文化输出 —— 委托 dump 协议(逐分支对齐 C#
  /// Program.cs;差异 = dump 的 null/empty 标记,但此处已排空)
  // The tuple family and the integer family: upstream's invariant-culture
  // TypeConverter output — delegated to the dump protocol (branch-aligned
  // with the C# Program.cs; its null/empty markers differ, but null is
  // already excluded here).
  return FormatValue(desc, val);
}

yaml::MiniYaml SaveRecord(const RecordObject& rec, yaml::StringPool& pool) {
  // L28-52(值袋路径:GeneratedRecord 的展开槽位序 = CollectFields 序)
  // L28-52 (the value-bag path: GeneratedRecord's expanded slot order = the
  // CollectFields order).
  std::vector<yaml::MiniYamlNode> vec_nodes;
  const auto* ptr_generated = dynamic_cast<const GeneratedRecord*>(&rec);
  const std::vector<const FieldDesc*> vec_fields =
      CollectFields(rec.record_desc());
  if (ptr_generated != nullptr) {
    for (std::size_t i = 0; i < vec_fields.size(); ++i) {
      const FieldDesc& desc = *vec_fields[i];
      const GenericValue& val = ptr_generated->Slot(i);
      if (desc.type == FieldType::Dictionary ||
          desc.type == FieldType::FrozenDictionary) {
        // L34-46:字典字段展开为子节点
        // L34-46: a dictionary field expands into sub-nodes.
        std::vector<yaml::MiniYamlNode> vec_dict_nodes;
        if (const auto* map_dict = std::get_if<GenericDict>(&val.val))
          for (const auto& [key, value] : *map_dict)
            vec_dict_nodes.emplace_back(
                pool.Intern(SaveFormatValue(*desc.key, key)),
                yaml::MiniYaml{
                    pool.Intern(SaveFormatValue(*desc.value, value))});

        vec_nodes.emplace_back(
            pool.Intern(desc.str_name),
            yaml::MiniYaml{nullptr, std::move(vec_dict_nodes)});
      } else {
        vec_nodes.emplace_back(
            pool.Intern(desc.str_name),
            yaml::MiniYaml{pool.Intern(SaveFormatValue(desc, val))});
      }
    }
  }

  return yaml::MiniYaml{nullptr, std::move(vec_nodes)};
}

yaml::MiniYaml SaveRecordDifferences(const RecordObject& rec_o,
                                     const RecordObject& rec_from,
                                     yaml::StringPool& pool) {
  // L54-65
  if (&rec_o.record_desc() != &rec_from.record_desc())
    throw std::runtime_error(
        "FieldSaver: can't diff objects of different types");

  const auto* ptr_o = dynamic_cast<const GeneratedRecord*>(&rec_o);
  const auto* ptr_from = dynamic_cast<const GeneratedRecord*>(&rec_from);
  const std::vector<const FieldDesc*> vec_fields =
      CollectFields(rec_o.record_desc());

  std::vector<yaml::MiniYamlNode> vec_nodes;
  if (ptr_o != nullptr && ptr_from != nullptr) {
    for (std::size_t i = 0; i < vec_fields.size(); ++i) {
      const FieldDesc& desc = *vec_fields[i];
      if (SaveFormatValue(desc, ptr_o->Slot(i)) !=
          SaveFormatValue(desc, ptr_from->Slot(i)))
        vec_nodes.emplace_back(
            pool.Intern(desc.str_name),
            yaml::MiniYaml{
                pool.Intern(SaveFormatValue(desc, ptr_o->Slot(i)))});
    }
  }

  return yaml::MiniYaml{nullptr, std::move(vec_nodes)};
}

yaml::MiniYamlNode SaveRecordField(const RecordObject& rec,
                                   std::string_view str_field,
                                   yaml::StringPool& pool) {
  // L67-70
  const auto* ptr_generated = dynamic_cast<const GeneratedRecord*>(&rec);
  const std::vector<const FieldDesc*> vec_fields =
      CollectFields(rec.record_desc());
  if (ptr_generated != nullptr)
    for (std::size_t i = 0; i < vec_fields.size(); ++i)
      if (vec_fields[i]->str_name == str_field)
        return yaml::MiniYamlNode{
            pool.Intern(str_field),
            yaml::MiniYaml{pool.Intern(
                SaveFormatValue(*vec_fields[i], ptr_generated->Slot(i)))}};

  throw std::runtime_error("FieldSaver: unknown field `" +
                           std::string{str_field} + "`");
}

}  // namespace ora::meta
