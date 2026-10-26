// UPSTREAM: OpenRA.Game/Network/ConnectionTarget.cs @1f594e320 L19-62(实现;
//          头文件同条目)
//          The implementation; the header carries the same tag.
#include "net/connection_target.hpp"

namespace ora::net {

std::vector<platform::SocketAddress>
ConnectionTarget::GetConnectEndPoints() const {
  std::vector<platform::SocketAddress> vec_out;
  for (const auto& endpoint : vec_endpoints_) {
    auto vec_resolved = platform::ResolveDns(endpoint.str_host,
                                             endpoint.int4_port);
    vec_out.insert(vec_out.end(),
                   std::make_move_iterator(vec_resolved.begin()),
                   std::make_move_iterator(vec_resolved.end()));
  }
  return vec_out;
}

std::string ConnectionTarget::ToString() const {
  std::string str_out;
  for (std::size_t i = 0; i < vec_endpoints_.size(); ++i) {
    if (i != 0)
      str_out += '/';
    const auto& endpoint = vec_endpoints_[i];
    str_out += endpoint.str_host;
    str_out += ':';
    str_out += std::to_string(endpoint.int4_port);
  }
  return str_out;
}

}  // namespace ora::net
