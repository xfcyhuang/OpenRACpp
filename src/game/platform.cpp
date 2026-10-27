import std;
#include "game/platform.hpp"

namespace ora::game {

bool CurrentPlatformIsOSX() {
#if defined(__APPLE__)
  return true;
#else
  return false;
#endif
}

namespace {

std::string_view TrimEndSpaces(std::string_view sv) {
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t'))
    sv.remove_suffix(1);
  return sv;
}

std::string WithTrailingSlash(std::string str_dir) {
  if (!str_dir.empty() && str_dir.back() != '/' && str_dir.back() != '\\')
    str_dir.push_back('/');
  return str_dir;
}
}

PathResolver::PathResolver(std::string str_engine_dir, std::string str_support_dir)
    : str_engine_dir_{WithTrailingSlash(std::move(str_engine_dir))},
      str_support_dir_{WithTrailingSlash(std::move(str_support_dir))} {}

std::string PathResolver::operator()(const std::string& str_path) const {
  std::string str_ret{TrimEndSpaces(str_path)};

  if (str_ret == "^SupportDir")
    return str_support_dir_;
  if (str_ret == "^EngineDir")
    return str_engine_dir_;
  if (str_ret == "^BinDir")
    return str_engine_dir_;

  if (str_ret.starts_with("^SupportDir|"))
    return str_support_dir_ + str_ret.substr(12);
  if (str_ret.starts_with("^EngineDir|"))
    return str_engine_dir_ + str_ret.substr(11);
  if (str_ret.starts_with("^BinDir|"))
    return str_engine_dir_ + str_ret.substr(8);

  return str_ret;
}

}
