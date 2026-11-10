#include "utility.hpp"

namespace ora::utility {

int utility_tag() {
  int int4_tag{8};
  return int4_tag;
}

std::string_view utility_name() {
  std::string_view str_name{"OpenRA.Utility"};
  return str_name;
}

}
