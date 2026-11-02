#include "cnc.hpp"

namespace ora::mods::cnc {

int cnc_tag() {
  int int4_tag{3};
  return int4_tag;
}

std::string_view cnc_name() {
  std::string_view str_name{"OpenRA.Mods.Cnc"};
  return str_name;
}

}
