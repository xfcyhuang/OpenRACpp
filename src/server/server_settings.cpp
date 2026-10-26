// UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L515-522(SanitizedServerName
//          全文;SanitizedName/SanitizedPlayerName 已随第十六批迁入
//          ora::net —— net/sanitize.cpp)
//          The whole of SanitizedServerName (L515-522); SanitizedName/
//          SanitizedPlayerName moved into ora::net with batch 16 —
//          net/sanitize.cpp).
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

}  // namespace ora::server
