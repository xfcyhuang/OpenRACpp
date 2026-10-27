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

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::platform {

struct SocketAddress {
  sockaddr_storage addr_storage{};
  socklen_t len_addr = 0;
  std::string str_display;
};

bool InitWinSockOnce();

std::vector<SocketAddress> ResolveDns(const std::string& str_host,
                                      int int4_port);

class TcpSocket {
 public:
  TcpSocket() = default;
  ~TcpSocket();

  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;
  TcpSocket(TcpSocket&& other) noexcept;
  TcpSocket& operator=(TcpSocket&& other) noexcept;

  bool IsValid() const {
    return h_socket_.load(std::memory_order_acquire) != INVALID_SOCKET;
  }

  static TcpSocket Connect(const SocketAddress& endpoint);

  bool SendAll(std::span<const std::uint8_t> span_bytes);

  bool ReceiveExact(std::uint8_t* p_buf, std::size_t sz_count);

  bool ReadInt32(std::int32_t& int4_out);

  static TcpSocket ListenEndpoint(std::string_view str_host, int int4_port);

  bool PollReadable(int int4_timeout_us);

  int ReceiveUpTo(std::uint8_t* p_buf, std::size_t sz_cap);

  bool SetNonBlocking(bool b_non_blocking);

  bool SetNoDelay();

  int SendNonBlocking(const std::uint8_t* p_buf, std::size_t sz_count,
                      bool& b_would_block_out);

  SocketAddress LocalEndpoint();

  const std::string& RemoteDisplay() const { return str_remote_; }

  void Close();

  static TcpSocket Listen(int int4_port);

  int BoundPort() const;

  TcpSocket Accept();

 private:
  explicit TcpSocket(SOCKET h_socket) : h_socket_(h_socket) {}

  std::atomic<SOCKET> h_socket_{INVALID_SOCKET};
  std::string str_remote_;
};

}
