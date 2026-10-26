// UPSTREAM: OpenRA.Game/Network/ConnectionTarget.cs @1f594e320 L19-62(逐语义
//          重写;Dns.GetHostAddresses → getaddrinfo 全族解析,D204)
//          Verbatim-semantics rewrite; Dns.GetHostAddresses → getaddrinfo
//          all-family resolution, D204.
#pragma once
import std;

#include "platform/tcp_socket.hpp"

namespace ora::net {

/// DnsEndPoint 的最小承载(host + port)
/// The minimal DnsEndPoint carrier (host + port).
struct DnsEndpoint {
  std::string str_host;
  int int4_port = 0;
};

/// ConnectionTarget(ConnectionTarget.cs L19-62)
class ConnectionTarget {
 public:
  /// ():上游 [("invalid", 0)] 单端点形态
  /// (): upstream's [("invalid", 0)] single-endpoint form.
  ConnectionTarget() : vec_endpoints_{{std::string("invalid"), 0}} {}

  ConnectionTarget(std::string str_host, int int4_port)
      : vec_endpoints_{{std::move(str_host), int4_port}} {}

  /// 空集 → ArgumentException(文本逐字)
  /// An empty set → ArgumentException (verbatim text).
  explicit ConnectionTarget(std::vector<DnsEndpoint> vec_endpoints)
      : vec_endpoints_(std::move(vec_endpoints)) {
    if (vec_endpoints_.empty())
      throw std::invalid_argument(
          "ConnectionTarget must have at least one address.");
  }

  /// GetConnectEndPoints(L40-53):逐端点 DNS 解析展平;解析失败(上游
  /// catch → 空序列)以空 vector 逐端点跳过 —— 整体为解析成功的全集
  /// GetConnectEndPoints (L40-53): the per-endpoint DNS resolution
  /// flattened; a failed resolve (upstream's catch → empty sequence)
  /// skips that endpoint as an empty vector — the whole is the full set
  /// of resolved addresses.
  std::vector<platform::SocketAddress> GetConnectEndPoints() const;

  /// ToString(L56-61):"host:port" 以 "/" 连接
  /// ToString (L56-61): "host:port" joined with "/".
  std::string ToString() const;

  /// 逻辑首端点视图(connection.EndPoint 的 IpEndPoint 面承载)
  const DnsEndpoint* FirstEndpoint() const {
    return vec_endpoints_.empty() ? nullptr : &vec_endpoints_.front();
  }

 private:
  std::vector<DnsEndpoint> vec_endpoints_;
};

}  // namespace ora::net
