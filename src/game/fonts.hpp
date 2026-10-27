#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::game {

class FontData final : public meta::RecordObject {
 public:
  std::string str_font;
  std::int32_t int4_size = 0;
  std::int32_t int4_ascender = 0;

  const meta::RecordDesc& record_desc() const override;

  static constexpr std::string_view kTypeName = "OpenRA.FontData";
};

class Fonts {
 public:

  static std::vector<std::pair<std::string, FontData>> Load(const yaml::MiniYaml& yaml_node);

  std::vector<std::pair<std::string, FontData>> vec_font_list;
};

}
