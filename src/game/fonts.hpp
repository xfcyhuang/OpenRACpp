// UPSTREAM: OpenRA.Game/Fonts.cs @e180e3225 L15-38(全文)。Fonts =
// IGlobalModData,FontList 由 LoadUsing(LoadFonts) 从 mod.yaml "Fonts" 节点
// 逐键装载(FontData = FieldLoader.Load 三字段)。C# FrozenDictionary →
// 插入序 vector<pair>;GetOrCreate 的 C++ 承载 = ModData::GetOrCreateFonts
// (mod_data.hpp,MergedNode("Fonts") 合并树优先,缺省空表)。
// The whole of Fonts.cs L15-38. Fonts is an IGlobalModData whose FontList
// loads through LoadUsing(LoadFonts) from the mod.yaml "Fonts" node, one
// FontData per key (FieldLoader.Load's three fields). The C#
// FrozenDictionary becomes an insertion-ordered vector<pair>; the
// GetOrCreate carrier is ModData::GetOrCreateFonts (mod_data.hpp, the
// MergedNode("Fonts") merged tree first, the empty table as the lazy
// default).
#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::game {

/// FontData(Fonts.cs L17-21;Font/Size/Ascender 三必填字段)。
/// FontData (Fonts.cs L17-21; the Font/Size/Ascender required trio).
class FontData final : public meta::RecordObject {
 public:
  std::string str_font;
  std::int32_t int4_size = 0;
  std::int32_t int4_ascender = 0;

  const meta::RecordDesc& record_desc() const override;

  static constexpr std::string_view kTypeName = "OpenRA.FontData";
};

/// Fonts(Fonts.cs L24-38;FontList 插入序表)。
/// Fonts (Fonts.cs L24-38; the insertion-ordered FontList).
class Fonts {
 public:
  /// LoadFonts(L27-34):逐节点 FieldLoader.Load<FontData>。
  /// LoadFonts (L27-34): one FieldLoader.Load<FontData> per node.
  static std::vector<std::pair<std::string, FontData>> Load(const yaml::MiniYaml& yaml_node);

  std::vector<std::pair<std::string, FontData>> vec_font_list;
};

}  // namespace ora::game
