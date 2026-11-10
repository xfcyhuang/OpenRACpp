#include "server.hpp"

namespace ora::server {

int server_tag() {
  int int4_tag{7};
  return int4_tag;
}

std::string_view server_name() {
  std::string_view str_name{"OpenRA.Server"};
  return str_name;
}

}
