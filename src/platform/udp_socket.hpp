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

  static UdpSocket BindAny(int int4_port);

  int ReceiveFrom(std::uint8_t* p_buf, std::size_t sz_cap,
                  SocketAddress& addr_out);

  bool PollReadable(int int4_timeout_us);

  bool SendTo(const SocketAddress& addr, std::span<const std::uint8_t> span_bytes);

  void Close();

 private:
  explicit UdpSocket(SOCKET h_socket) : h_socket_(h_socket) {}

  std::atomic<SOCKET> h_socket_{INVALID_SOCKET};
};

}
