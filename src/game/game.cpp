module ora.game;

import std;

namespace ora::game {

int game_tag() {
  int int4_tag{1};
  return int4_tag;
}

std::string_view game_name() {
  std::string_view str_name{"OpenRA.Game"};
  return str_name;
}

}
