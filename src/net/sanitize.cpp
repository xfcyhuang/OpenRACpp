// UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L500-538(实现全文;
//          第十六批自 src/server/server_settings.cpp 迁入)
//          The whole implementation (migrated from src/server/
//          server_settings.cpp with batch 16).
import std;

#include "net/sanitize.hpp"

#include "core/text.hpp"

namespace ora::net {

namespace {

bool IsForbiddenName(std::string_view str_clean) {
  return str_clean == "Open" || str_clean == "Closed";
}

}  // namespace

std::string SanitizedName(std::string_view str_dirty) {
  if (str_dirty.empty())
    return {};

  auto str_clean = std::string{str_dirty};
  constexpr char kDisallowed[] = {'#',  '@',  ':', '\n', '\t',
                                  '[', ']', '{', '}',  '<',
                                  '>', '"', '`'};
  for (const char disallowed : kDisallowed)
    str_clean.erase(std::remove(str_clean.begin(), str_clean.end(), disallowed),
                    str_clean.end());

  return str_clean;
}

std::string SanitizedPlayerName(std::string_view str_dirty,
                                std::string_view str_default_name) {
  auto str_clean = SanitizedName(str_dirty);
  if (ora::TrimNetWhiteSpace(str_clean).empty() ||
      IsForbiddenName(str_clean))
    str_clean = std::string{str_default_name};

  auto vec_positions = std::vector<std::size_t>{};
  for (std::size_t i = 0; i < str_clean.size();) {
    vec_positions.push_back(i);
    auto cp = ora::DecodeUtf8(str_clean, i);
    static_cast<void>(cp);
  }

  if (vec_positions.size() > 16)
    str_clean = str_clean.substr(0, vec_positions[16]);

  return str_clean;
}

}  // namespace ora::net
