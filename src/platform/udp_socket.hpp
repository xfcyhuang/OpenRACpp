// UPSTREAM: NONE —— PORTING_PLAN §7 自有网络基础设施:.NET UdpClient 的
// WinSock2 直连对应物(BeaconLib 的 LAN 发现应答与测试侧共用;上游无逐
// 行对应源文件,语义对照 BeaconLib 1.0.2 (rix0rrr/beacon) 的 UdpClient
// 用法面 —— SO_REUSEADDR + Any:port 绑定 + BeginReceive 循环 + Send 回
// 应答)。NONE = C++ 侧自有设计,见 tools/upstream_check.py 登记形式。
// PORTING_PLAN §7 in-house network infrastructure: the WinSock2 direct
// counterpart of .NET's UdpClient (shared by BeaconLib's LAN-discovery
// responder and the test side; no line-by-line upstream source exists — the
// semantic anchor is BeaconLib 1.0.2 (rix0rrr/beacon)'s UdpClient usage:
// SO_REUSEADDR + an Any:port bind + the BeginReceive loop + the Send
// reply). NONE = C++-native design, see tools/upstream_check.py for the
// registration form.
#pragma once
import std;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "platform/tcp_socket.hpp"

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::platform {

/// UDP 数据报 socket(SOCK_DGRAM;BeaconLib Beacon 的承载原语)
/// A UDP datagram socket (SOCK_DGRAM; the carrying primitive of BeaconLib's
/// Beacon).
class UdpSocket {
 public:
  UdpSocket() = default;
  ~UdpSocket();

  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;
  UdpSocket(UdpSocket&& other) noexcept;
  UdpSocket& operator=(UdpSocket&& other) noexcept;

  bool IsValid() const {
    return h_socket_.load(std::memory_order_acquire) != INVALID_SOCKET;
  }

  /// socket + SO_REUSEADDR + bind(IPAddress.Any:port)(失败 = 无效 socket)
  /// socket + SO_REUSEADDR + bind (IPAddress.Any:port) (failure = an
  /// invalid socket).
  static UdpSocket BindAny(int int4_port);

  /// 阻塞 recvfrom(返回 >0 = 字节数,0 = 关闭,-1 = 错误;对端地址出参)
  /// The blocking recvfrom (>0 = the byte count, 0 = closed, -1 = error;
  /// the peer's address as an out-param).
  int ReceiveFrom(std::uint8_t* p_buf, std::size_t sz_cap,
                  SocketAddress& addr_out);

  /// Socket.Poll(timeout, SelectRead)(timeout_us = 微秒)
  /// Socket.Poll(timeout, SelectRead) (timeout_us in microseconds).
  bool PollReadable(int int4_timeout_us);


  /// sendto(失败 = false)
  /// sendto (false = failure).
  bool SendTo(const SocketAddress& addr, std::span<const std::uint8_t> span_bytes);

  /// Close():幂等;阻塞 recvfrom 以错误唤醒
  /// Close(): idempotent; a blocking recvfrom wakes as an error.
  void Close();

 private:
  explicit UdpSocket(SOCKET h_socket) : h_socket_(h_socket) {}

  std::atomic<SOCKET> h_socket_{INVALID_SOCKET};
};

}  // namespace ora::platform
