#pragma once
import std;

#include "core/arguments.hpp"
#include "net/connection_target.hpp"

namespace ora::net {

struct LaunchArguments {
  std::string str_connect{};
  std::string str_uri{};
  std::string str_replay{};
  std::string str_benchmark{};
  std::string str_map{};

  LaunchArguments() = default;

  explicit LaunchArguments(const core::Arguments& args_args);

  std::optional<ConnectionTarget> GetConnectEndPoint() const;
};

}
