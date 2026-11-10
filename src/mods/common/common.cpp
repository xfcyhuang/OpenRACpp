module ora.mods.common;

import std;

namespace ora::mods::common {

int common_tag() {
  int int4_tag{2};
  return int4_tag;
}

std::string_view common_name() {
  std::string_view str_name{"OpenRA.Mods.Common"};
  return str_name;
}

}
