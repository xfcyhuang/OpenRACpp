#pragma once
import std;

namespace ora::net {

std::string SanitizedName(std::string_view str_dirty);

std::string SanitizedPlayerName(std::string_view str_dirty,
                                std::string_view str_default_name);

}
