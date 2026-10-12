// UPSTREAM: NONE —— 与 tcp_socket.hpp 同一条目(WinSock2 直连基础设施)
// UPSTREAM: NONE — the same entry as tcp_socket.hpp (the WinSock2 direct
// infrastructure).
#include "platform/tcp_socket.hpp"

namespace ora::platform {

namespace {

std::once_flag flag_winsock;
bool b_winsock_ready = false;

void InitWinSockBody() {
  WSADATA wsa_data{};
  b_winsock_ready = WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
}

std::string FormatAddress(const sockaddr_storage& addr, socklen_t len) {
  char buf[INET6_ADDRSTRLEN + 16];
  if (addr.ss_family == AF_INET6) {
    char host[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6,
                  &reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_addr,
                  host, sizeof(host)) != nullptr) {
      std::snprintf(buf, sizeof(buf), "[%s]:%u", host,
                    ntohs(reinterpret_cast<const sockaddr_in6*>(&addr)
                              ->sin6_port));
      return buf;
    }
  } else {
    char host[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET,
                  &reinterpret_cast<const sockaddr_in*>(&addr)->sin_addr,
                  host, sizeof(host)) != nullptr) {
      std::snprintf(buf, sizeof(buf), "%s:%u", host,
                    ntohs(reinterpret_cast<const sockaddr_in*>(&addr)
                              ->sin_port));
      return buf;
    }
  }
  (void)len;
  return {};
}

}  // namespace

bool InitWinSockOnce() {
  std::call_once(flag_winsock, InitWinSockBody);
  return b_winsock_ready;
}

std::vector<SocketAddress> ResolveDns(const std::string& str_host,
                                      int int4_port) {
  std::vector<SocketAddress> vec_out;
  if (!InitWinSockOnce())
    return vec_out;

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* p_result = nullptr;
  if (getaddrinfo(str_host.c_str(), nullptr, &hints, &p_result) != 0)
    return vec_out;

  for (auto* p_ai = p_result; p_ai != nullptr; p_ai = p_ai->ai_next) {
    if (p_ai->ai_family != AF_INET && p_ai->ai_family != AF_INET6)
      continue;
    SocketAddress ep;
    std::memcpy(&ep.addr_storage, p_ai->ai_addr, p_ai->ai_addrlen);
    ep.len_addr = p_ai->ai_addrlen;
    if (p_ai->ai_family == AF_INET6)
      reinterpret_cast<sockaddr_in6*>(&ep.addr_storage)->sin6_port =
          htons(static_cast<std::uint16_t>(int4_port));
    else
      reinterpret_cast<sockaddr_in*>(&ep.addr_storage)->sin_port =
          htons(static_cast<std::uint16_t>(int4_port));
    ep.str_display = FormatAddress(ep.addr_storage, ep.len_addr);
    vec_out.push_back(std::move(ep));
  }
  freeaddrinfo(p_result);
  return vec_out;
}

TcpSocket::~TcpSocket() { Close(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : h_socket_(other.h_socket_.load(std::memory_order_acquire)),
      str_remote_(std::move(other.str_remote_)) {
  other.h_socket_.store(INVALID_SOCKET, std::memory_order_release);
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
  if (this != &other) {
    Close();
    h_socket_.store(other.h_socket_.load(std::memory_order_acquire),
                    std::memory_order_release);
    str_remote_ = std::move(other.str_remote_);
    other.h_socket_.store(INVALID_SOCKET, std::memory_order_release);
  }
  return *this;
}

TcpSocket TcpSocket::Connect(const SocketAddress& endpoint) {
  if (!InitWinSockOnce() || endpoint.len_addr == 0)
    return TcpSocket{};

  const SOCKET sock = socket(endpoint.addr_storage.ss_family, SOCK_STREAM,
                             IPPROTO_TCP);
  if (sock == INVALID_SOCKET)
    return TcpSocket{};

  if (connect(sock, reinterpret_cast<const sockaddr*>(&endpoint.addr_storage),
              endpoint.len_addr) != 0) {
    closesocket(sock);
    return TcpSocket{};
  }

  const int int4_no_delay = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char*>(&int4_no_delay),
             sizeof(int4_no_delay));

  TcpSocket tcp{sock};
  sockaddr_storage peer{};
  socklen_t len_peer = sizeof(peer);
  if (getpeername(sock, reinterpret_cast<sockaddr*>(&peer), &len_peer) == 0)
    tcp.str_remote_ = FormatAddress(peer, len_peer);
  return tcp;
}

bool TcpSocket::SendAll(std::span<const std::uint8_t> span_bytes) {
  std::size_t sz_sent = 0;
  while (sz_sent < span_bytes.size()) {
    const int int4_n = send(
        h_socket_.load(std::memory_order_acquire),
        reinterpret_cast<const char*>(span_bytes.data() + sz_sent),
        static_cast<int>(span_bytes.size() - sz_sent), 0);
    if (int4_n <= 0)
      return false;
    sz_sent += static_cast<std::size_t>(int4_n);
  }
  return true;
}

bool TcpSocket::ReceiveExact(std::uint8_t* p_buf, std::size_t sz_count) {
  std::size_t sz_got = 0;
  while (sz_got < sz_count) {
    const int int4_n =
        recv(h_socket_.load(std::memory_order_acquire),
             reinterpret_cast<char*>(p_buf + sz_got),
             static_cast<int>(sz_count - sz_got), 0);
    if (int4_n <= 0)
      return false;
    sz_got += static_cast<std::size_t>(int4_n);
  }
  return true;
}

bool TcpSocket::ReadInt32(std::int32_t& int4_out) {
  std::uint8_t buf[4];
  if (!ReceiveExact(buf, sizeof(buf)))
    return false;
  int4_out = static_cast<std::int32_t>(static_cast<std::uint32_t>(buf[0]) |
                                       (static_cast<std::uint32_t>(buf[1]) << 8) |
                                       (static_cast<std::uint32_t>(buf[2]) << 16) |
                                       (static_cast<std::uint32_t>(buf[3]) << 24));
  return true;
}

void TcpSocket::Close() {
  const SOCKET sock = h_socket_.exchange(INVALID_SOCKET,
                                         std::memory_order_acq_rel);
  if (sock != INVALID_SOCKET) {
    shutdown(sock, SD_BOTH);
    closesocket(sock);
  }
}

TcpSocket TcpSocket::Listen(int int4_port) {
  if (!InitWinSockOnce())
    return TcpSocket{};

  const SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET)
    return TcpSocket{};

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<std::uint16_t>(int4_port));
  if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0 ||
      listen(sock, 1) != 0) {
    closesocket(sock);
    return TcpSocket{};
  }
  return TcpSocket{sock};
}

int TcpSocket::BoundPort() const {
  sockaddr_in addr{};
  socklen_t len_addr = sizeof(addr);
  if (getsockname(h_socket_.load(std::memory_order_acquire),
                  reinterpret_cast<sockaddr*>(&addr),
                  &len_addr) != 0)
    return 0;
  return ntohs(addr.sin_port);
}

TcpSocket TcpSocket::Accept() {
  sockaddr_storage peer{};
  socklen_t len_peer = sizeof(peer);
  const SOCKET sock = accept(h_socket_.load(std::memory_order_acquire),
                             reinterpret_cast<sockaddr*>(&peer), &len_peer);
  if (sock == INVALID_SOCKET)
    return TcpSocket{};

  TcpSocket tcp{sock};
  tcp.str_remote_ = FormatAddress(peer, len_peer);
  return tcp;
}

TcpSocket TcpSocket::ListenEndpoint(std::string_view str_host,
                                    int int4_port) {
  if (!InitWinSockOnce())
    return TcpSocket{};

  const auto str_trimmed = std::string{str_host};
  const bool b_v6 = str_trimmed == "::" || str_trimmed == "::0";

  const SOCKET sock =
      socket(b_v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET)
    return TcpSocket{};

  sockaddr_storage addr{};
  socklen_t len_addr = 0;
  if (b_v6) {
    auto* p_addr6 = reinterpret_cast<sockaddr_in6*>(&addr);
    p_addr6->sin6_family = AF_INET6;
    p_addr6->sin6_addr = in6addr_any;
    p_addr6->sin6_port = htons(static_cast<std::uint16_t>(int4_port));
    len_addr = sizeof(sockaddr_in6);
  } else {
    auto* p_addr4 = reinterpret_cast<sockaddr_in*>(&addr);
    p_addr4->sin_family = AF_INET;
    p_addr4->sin_addr.s_addr =
        str_trimmed.empty() || str_trimmed == "0.0.0.0"
            ? htonl(INADDR_ANY)
            : inet_addr(str_trimmed.c_str());
    p_addr4->sin_port = htons(static_cast<std::uint16_t>(int4_port));
    len_addr = sizeof(sockaddr_in);
  }

  if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), len_addr) != 0 ||
      listen(sock, SOMAXCONN) != 0) {
    closesocket(sock);
    return TcpSocket{};
  }
  return TcpSocket{sock};
}

bool TcpSocket::PollReadable(int int4_timeout_us) {
  const SOCKET sock = h_socket_.load(std::memory_order_acquire);
  if (sock == INVALID_SOCKET)
    return false;

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(sock, &read_fds);

  timeval timeout{};
  timeout.tv_sec = int4_timeout_us / 1000000;
  timeout.tv_usec = int4_timeout_us % 1000000;

  return select(static_cast<int>(sock) + 1, &read_fds, nullptr, nullptr,
                &timeout) > 0 &&
         FD_ISSET(sock, &read_fds);
}

int TcpSocket::ReceiveUpTo(std::uint8_t* p_buf, std::size_t sz_cap) {
  const int int4_n =
      recv(h_socket_.load(std::memory_order_acquire),
           reinterpret_cast<char*>(p_buf), static_cast<int>(sz_cap), 0);
  return int4_n;
}

bool TcpSocket::SetNonBlocking(bool b_non_blocking) {
  const SOCKET sock = h_socket_.load(std::memory_order_acquire);
  if (sock == INVALID_SOCKET)
    return false;

  u_long mode = b_non_blocking ? 1 : 0;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

bool TcpSocket::SetNoDelay() {
  const SOCKET sock = h_socket_.load(std::memory_order_acquire);
  if (sock == INVALID_SOCKET)
    return false;

  const int int4_no_delay = 1;
  return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<const char*>(&int4_no_delay),
                    sizeof(int4_no_delay)) == 0;
}

int TcpSocket::SendNonBlocking(const std::uint8_t* p_buf, std::size_t sz_count,
                               bool& b_would_block_out) {
  b_would_block_out = false;
  const int int4_n =
      send(h_socket_.load(std::memory_order_acquire),
           reinterpret_cast<const char*>(p_buf),
           static_cast<int>(sz_count), 0);
  if (int4_n != SOCKET_ERROR)
    return int4_n;

  if (WSAGetLastError() == WSAEWOULDBLOCK) {
    b_would_block_out = true;
    return 0;
  }
  return -1;
}

SocketAddress TcpSocket::LocalEndpoint() {
  SocketAddress ep;
  socklen_t len_addr = sizeof(ep.addr_storage);
  if (getsockname(h_socket_.load(std::memory_order_acquire),
                  reinterpret_cast<sockaddr*>(&ep.addr_storage),
                  &len_addr) != 0)
    return ep;
  ep.len_addr = len_addr;
  ep.str_display = FormatAddress(ep.addr_storage, len_addr);
  return ep;
}

}  // namespace ora::platform
