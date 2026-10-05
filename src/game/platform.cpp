// UPSTREAM: OpenRA.Game/Platform.cs @b6fc03f(platform.hpp 的实现;
//          ResolvePath 逐语义,见 hpp 头注)
//          Implementation of platform.hpp — the verbatim ResolvePath; see
//          the hpp header notes.
import std;
#include "game/platform.hpp"

namespace ora::game {

namespace {
/// 逐码点级尾部空白裁剪(C# path.TrimEnd(' ', '\t') 只剥这两个字符)
/// Code-point-level tail trimming (C# path.TrimEnd(' ', '\t') strips these
/// two characters only).
std::string_view TrimEndSpaces(std::string_view sv) {
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t'))
    sv.remove_suffix(1);
  return sv;
}

/// 目录规范化:保证尾部 '/'(上游 BinDir 的 "Add trailing
/// DirectorySeparator" 语义 —— '^EngineDir|xxx' 的字符串直拼依赖它)
/// Directory normalization: guarantee the trailing '/' (the "Add trailing
/// DirectorySeparator" semantics of the upstream BinDir — the string
/// concatenation of '^EngineDir|xxx' depends on it).
std::string WithTrailingSlash(std::string str_dir) {
  if (!str_dir.empty() && str_dir.back() != '/' && str_dir.back() != '\\')
    str_dir.push_back('/');
  return str_dir;
}
}  // namespace

PathResolver::PathResolver(std::string str_engine_dir, std::string str_support_dir)
    : str_engine_dir_{WithTrailingSlash(std::move(str_engine_dir))},
      str_support_dir_{WithTrailingSlash(std::move(str_support_dir))} {}

std::string PathResolver::operator()(const std::string& str_path) const {
  std::string str_ret{TrimEndSpaces(str_path)};

  // 整值形态 / whole-value forms (L292-300).
  if (str_ret == "^SupportDir")
    return str_support_dir_;
  if (str_ret == "^EngineDir")
    return str_engine_dir_;
  if (str_ret == "^BinDir")
    return str_engine_dir_;

  // '前缀|' 形态 / the 'prefix|' forms (L302-310).
  if (str_ret.starts_with("^SupportDir|"))
    return str_support_dir_ + str_ret.substr(12);
  if (str_ret.starts_with("^EngineDir|"))
    return str_engine_dir_ + str_ret.substr(11);
  if (str_ret.starts_with("^BinDir|"))
    return str_engine_dir_ + str_ret.substr(8);

  return str_ret;
}

}  // namespace ora::game
