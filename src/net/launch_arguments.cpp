import std;

#include "net/launch_arguments.hpp"

#include <boost/url.hpp>

namespace ora::net {

LaunchArguments::LaunchArguments(const core::Arguments& args_args) {
  if (args_args.Contains("Launch.Connect"))
    str_connect = args_args.GetValue("Launch.Connect", "");
  if (args_args.Contains("Launch.URI"))
    str_uri = args_args.GetValue("Launch.URI", "");
  if (args_args.Contains("Launch.Replay"))
    str_replay = args_args.GetValue("Launch.Replay", "");
  if (args_args.Contains("Launch.Benchmark"))
    str_benchmark = args_args.GetValue("Launch.Benchmark", "");
  if (args_args.Contains("Launch.Map"))
    str_map = args_args.GetValue("Launch.Map", "");
}

std::optional<ConnectionTarget> LaunchArguments::GetConnectEndPoint() const {
  std::string str_uri_text;
  if (!str_uri.empty())
    str_uri_text = str_uri;
  else if (!str_connect.empty())
    str_uri_text = "tcp://" + str_connect;
  else
    return std::nullopt;

  const auto result_url = boost::urls::parse_uri(str_uri_text);
  if (!result_url)
    return std::nullopt;
  const boost::urls::url_view url_viewed = *result_url;
  if (!url_viewed.has_scheme())
    return std::nullopt;

  std::string str_host{url_viewed.host()};
  if (str_host.empty())
    return std::nullopt;

  int int4_port = -1;
  const std::string str_port{url_viewed.port()};
  if (!str_port.empty()) {
    int int4_value = 0;
    const auto [ptr_next, ec] = std::from_chars(
        str_port.data(), str_port.data() + str_port.size(), int4_value);
    if (ec != std::errc{} || ptr_next != str_port.data() + str_port.size() ||
        int4_value < 0 || int4_value > 65535)
      return std::nullopt;
    int4_port = int4_value;
  }

  return ConnectionTarget(std::move(str_host), int4_port);
}

}
