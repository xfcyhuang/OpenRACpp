module ora.mods.d2k;

import std;

namespace ora::mods::d2k {

int d2k_tag() {
  int int4_tag{4};
  return int4_tag;
}

std::string_view d2k_name() {
  std::string_view str_name{"OpenRA.Mods.D2k"};
  return str_name;
}

}
