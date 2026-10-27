import std;

#include "server/server_settings.hpp"

#include "core/text.hpp"

namespace ora::server {

std::string ServerSettings::SanitizedServerName(
    std::string_view str_dirty, std::string_view str_player_name) const {
  const auto str_clean = SanitizedName(str_dirty);
  if (ora::TrimNetWhiteSpace(str_clean).empty())
    return std::format("{}'s Game",
                       SanitizedPlayerName(str_player_name, "Commander"));
  return str_clean;
}

}
