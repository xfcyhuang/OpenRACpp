#pragma once
import std;

#include "platform/tcp_socket.hpp"

namespace ora::net {

struct DnsEndpoint {
  std::string str_host;
  int int4_port = 0;
};

class ConnectionTarget {
 public:

  ConnectionTarget() : vec_endpoints_{{std::string("invalid"), 0}} {}

  ConnectionTarget(std::string str_host, int int4_port)
      : vec_endpoints_{{std::move(str_host), int4_port}} {}

  explicit ConnectionTarget(std::vector<DnsEndpoint> vec_endpoints)
      : vec_endpoints_(std::move(vec_endpoints)) {
    if (vec_endpoints_.empty())
      throw std::invalid_argument(
          "ConnectionTarget must have at least one address.");
  }

  std::vector<platform::SocketAddress> GetConnectEndPoints() const;

  std::string ToString() const;

  const DnsEndpoint* FirstEndpoint() const {
    return vec_endpoints_.empty() ? nullptr : &vec_endpoints_.front();
  }

 private:
  std::vector<DnsEndpoint> vec_endpoints_;
};

}
