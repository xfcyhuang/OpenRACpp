module ora.mods.ra;

import std;

namespace ora::mods::ra {

int ra_tag() {
  int int4_tag{5};
  return int4_tag;
}

std::string_view ra_name() {
  std::string_view str_name{"mods/ra"};
  return str_name;
}

}
