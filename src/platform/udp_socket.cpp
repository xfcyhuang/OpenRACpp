// UPSTREAM: NONE —— 自有基础设施(见 udp_socket.hpp 头注)
// NONE = in-house infrastructure (see udp_socket.hpp's header note).
#include "platform/udp_socket.hpp"

namespace ora::platform {

UdpSocket::~UdpSocket() {
  Close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : h_socket_{other.h_socket_.exchange(INVALID_SOCKET, std::memory_order_acq_rel)} {}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
  if (this != &other) {
    Close();
    h_socket_.store(other.h_socket_.exchange(INVALID_SOCKET, std::memory_order_acq_rel),
                    std::memory_order_release);
  }
  return *this;
}

UdpSocket UdpSocket::BindAny(int int4_port) {
  if (!InitWinSockOnce())
    return UdpSocket{INVALID_SOCKET};

  const SOCKET h_socket =
      socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (h_socket == INVALID_SOCKET)
    return UdpSocket{INVALID_SOCKET};

  BOOL b_reuse = TRUE;
  setsockopt(h_socket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&b_reuse), sizeof(b_reuse));

  BOOL b_broadcast = TRUE;
  setsockopt(h_socket, SOL_SOCKET, SO_BROADCAST,
             reinterpret_cast<const char*>(&b_broadcast),
             sizeof(b_broadcast));

  sockaddr_in addr_bind{};
  addr_bind.sin_family = AF_INET;
  addr_bind.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_bind.sin_port = htons(static_cast<u_short>(int4_port));
  if (bind(h_socket, reinterpret_cast<const sockaddr*>(&addr_bind),
           sizeof(addr_bind)) == SOCKET_ERROR) {
    closesocket(h_socket);
    return UdpSocket{INVALID_SOCKET};
  }

  return UdpSocket{h_socket};
}

int UdpSocket::ReceiveFrom(std::uint8_t* p_buf, std::size_t sz_cap,
                           SocketAddress& addr_out) {
  const SOCKET h_socket = h_socket_.load(std::memory_order_acquire);
  if (h_socket == INVALID_SOCKET)
    return -1;

  sockaddr_storage addr_peer{};
  socklen_t len_addr = sizeof(addr_peer);
  const int int4_n = recvfrom(h_socket, reinterpret_cast<char*>(p_buf),
                              static_cast<int>(sz_cap), 0,
                              reinterpret_cast<sockaddr*>(&addr_peer), &len_addr);
  if (int4_n <= 0)
    return int4_n == 0 ? 0 : -1;

  addr_out.addr_storage = addr_peer;
  addr_out.len_addr = len_addr;

  char arr_host[NI_MAXHOST]{};
  char arr_serv[NI_MAXSERV]{};
  if (getnameinfo(reinterpret_cast<const sockaddr*>(&addr_peer), len_addr,
                  arr_host, sizeof(arr_host), arr_serv, sizeof(arr_serv),
                  NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    const bool b_v6 = addr_peer.ss_family == AF_INET6;
    addr_out.str_display = b_v6 ? std::format("[{}]:{}", arr_host, arr_serv)
                                : std::format("{}:{}", arr_host, arr_serv);
  }

  return int4_n;
}

bool UdpSocket::PollReadable(int int4_timeout_us) {
  const SOCKET h_socket = h_socket_.load(std::memory_order_acquire);
  if (h_socket == INVALID_SOCKET)
    return false;

  fd_set set_read;
  FD_ZERO(&set_read);
  FD_SET(h_socket, &set_read);

  timeval tv_timeout{};
  tv_timeout.tv_sec = int4_timeout_us / 1000000;
  tv_timeout.tv_usec = int4_timeout_us % 1000000;

  const int int4_ready = select(static_cast<int>(h_socket) + 1, &set_read,
                                nullptr, nullptr, &tv_timeout);
  return int4_ready > 0;
}

bool UdpSocket::SendTo(const SocketAddress& addr,
                       std::span<const std::uint8_t> span_bytes) {
  const SOCKET h_socket = h_socket_.load(std::memory_order_acquire);
  if (h_socket == INVALID_SOCKET || span_bytes.empty())
    return false;

  const int int4_sent =
      sendto(h_socket, reinterpret_cast<const char*>(span_bytes.data()),
             static_cast<int>(span_bytes.size()), 0,
             reinterpret_cast<const sockaddr*>(&addr.addr_storage),
             addr.len_addr);
  return int4_sent == static_cast<int>(span_bytes.size());
}

void UdpSocket::Close() {
  const SOCKET h_socket = h_socket_.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
  if (h_socket != INVALID_SOCKET)
    closesocket(h_socket);
}

}  // namespace ora::platform
