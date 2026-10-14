// UPSTREAM: OpenRA.Game/Fonts.cs @e180e3225(fonts.hpp 的实现)
// The implementation of fonts.hpp.
import std;

#include "game/fonts.hpp"

#include "meta/field_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {
namespace {

constexpr std::array<meta::FieldDesc, 3> kFields_FontData{{
    {.str_name = "Font", .type = meta::FieldType::String, .b_required = true, .str_loader = {}, .off_offset = __builtin_offsetof(FontData, str_font), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Size", .type = meta::FieldType::Int32, .b_required = true, .str_loader = {}, .off_offset = __builtin_offsetof(FontData, int4_size), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "Ascender", .type = meta::FieldType::Int32, .b_required = true, .str_loader = {}, .off_offset = __builtin_offsetof(FontData, int4_ascender), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
}};

constexpr meta::RecordDesc kDesc_FontData{
    .str_name = "OpenRA.FontData",
    .str_base = {},
    .fields = kFields_FontData,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

}  // namespace

const meta::RecordDesc& FontData::record_desc() const { return kDesc_FontData; }

std::vector<std::pair<std::string, FontData>> Fonts::Load(const yaml::MiniYaml& yaml_node) {
  std::vector<std::pair<std::string, FontData>> vec_ret;
  vec_ret.reserve(yaml_node.Nodes.size());
  for (const yaml::MiniYamlNode& node : yaml_node.Nodes) {
    FontData data_font;
    meta::Load(&data_font, node.Value);
    vec_ret.emplace_back(node.Key != nullptr ? *node.Key : std::string{},
                         std::move(data_font));
  }
  return vec_ret;
}

}  // namespace ora::game
