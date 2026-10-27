#pragma once
import std;

namespace ora::game {

bool CurrentPlatformIsOSX();

class PathResolver final {
 public:

  PathResolver(std::string str_engine_dir, std::string str_support_dir);

  std::string operator()(const std::string& str_path) const;

 private:
  std::string str_engine_dir_;
  std::string str_support_dir_;
};

}
