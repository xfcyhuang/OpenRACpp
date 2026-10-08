// UPSTREAM: OpenRA.Game/FieldSaver.cs @b6fc03f L26-166 全文(值袋承载形
//          态:反射面 → RecordDesc 描述表;Settings 只存差异的序列化面)
//          The whole of FieldSaver.cs L26-166 (the value-bag form: the
//          reflection faces → the RecordDesc tables; the save-differences
//          serialization face of Settings).
//
// 机制对照 / Mechanism mapping:
//  - GetTypeLoadInfo 的反射字段枚举 → CollectFields(desc)(base 链序 = C#
//    GetFields 序)
//    GetTypeLoadInfo's reflective field enumeration → CollectFields(desc)
//    (the base-chain order = C# GetFields order).
//  - FormatValue 的 TypeConverter/ToString 兜底链 → desc.type 判别的值袋
//    分支(null → "";BitSet → 注册标签名 ", " 连接;bool → True/False)
//    FormatValue's TypeConverter/ToString fallback chain → the value-bag
//    branches discriminating on desc.type (null → ""; BitSet → the
//    registered tag names joined ", "; bool → True/False).
//  - Save 的字典子节点形态保留(上游 settings 键段);DateTime/Vector2/3 与
//    文档用字典串接面 = 值袋无此载荷,静态断言于 FieldType 域外
//    Save's dictionary sub-node form is kept (the settings key sections);
//    DateTime/Vector2/3 and the doc-only dictionary-concat faces have no
//    value-bag payloads — outside the FieldType domain.
#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

/// FieldSaver.FormatValue(L72-160)的值袋面(输出可被 FieldLoader 读回)
/// The value-bag face of FieldSaver.FormatValue (the output reloads
/// through FieldLoader).
std::string SaveFormatValue(const FieldDesc& desc, const GenericValue& val);

/// Save(L28-52):整对象 → MiniYaml(字典字段展开为子节点)
/// Save (L28-52): the whole object → MiniYaml (dictionary fields expand
/// into sub-nodes).
yaml::MiniYaml SaveRecord(const RecordObject& rec, yaml::StringPool& pool);

/// SaveDifferences(L54-65):只存与 from 的差异(类型不同 → 异常文本逐字)
/// SaveDifferences (L54-65): only the fields differing from from (a type
/// mismatch → the verbatim exception).
yaml::MiniYaml SaveRecordDifferences(const RecordObject& rec_o,
                                     const RecordObject& rec_from,
                                     yaml::StringPool& pool);

/// SaveField(L67-70)
yaml::MiniYamlNode SaveRecordField(const RecordObject& rec,
                                   std::string_view str_field,
                                   yaml::StringPool& pool);

}  // namespace ora::meta
