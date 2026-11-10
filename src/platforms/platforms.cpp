module ora.platforms;

import std;

namespace ora::platforms {

int platforms_tag() {
  int int4_tag{6};
  return int4_tag;
}

std::string_view platforms_name() {
  std::string_view str_name{"OpenRA.Platforms.Default"};
  return str_name;
}

}
