// UPSTREAM: NONE —— PORTING_PLAN §7 自有网络基础设施:.NET TcpClient/Dns/
// IPEndPoint 的 WinSock2 直连对应物(NetworkConnection 的连接/读线程与
// 测试侧伪服务器共用;上游无逐行对应源文件,语义对照 Connection.cs 的
// TcpClient 用法面)。NONE = C++ 侧自有设计,见 tools/upstream_check.py
// 登记形式。
// PORTING_PLAN §7 in-house network infrastructure: the WinSock2 direct
// counterparts of .NET's TcpClient/Dns/IPEndPoint (shared by
// NetworkConnection's connect/read threads and the test-side fake
// server; no line-by-line upstream source exists — the semantic anchor
// is Connection.cs's TcpClient usage). NONE = C++-native design, see
// tools/upstream_check.py for the registration form.
#pragma once
import std;

#include <winsock2.h>
#include <ws2tcpip.h>

namespace ora::platform {

/// 解析端点(DnsEndPoint 的对应物;display = "ip:port" 或 "[v6]:port")
/// A resolved endpoint (the DnsEndPoint counterpart; display = "ip:port"
/// or "[v6]:port").
struct SocketAddress {
  sockaddr_storage addr_storage{};
  socklen_t len_addr = 0;
  std::string str_display;
};

/// WSAStartup 一次性初始化(进程级;失败 = false,调用方走错误路径)
/// One-shot WSAStartup (process-wide; false = failure, callers take the
/// error path).
bool InitWinSockOnce();

/// Dns.GetHostAddresses(host) + IPEndPoint 装配:AF_UNSPEC/SOCK_STREAM
/// 全族解析,失败(host 不存在等)= 空 vector(上游 catch → 空数组同义)
/// Dns.GetHostAddresses(host) + IPEndPoint assembly: AF_UNSPEC/
/// SOCK_STREAM all-family resolution; failure (unknown host etc.) = an
/// empty vector (the counterpart of upstream's catch → empty array).
std::vector<SocketAddress> ResolveDns(const std::string& str_host,
                                      int int4_port);

/// TcpClient{NoDelay} + Connect 的直连形态(阻塞;失败 = 无效 socket)
/// The direct form of TcpClient{NoDelay} + Connect (blocking; failure =
/// an invalid socket).
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

  /// 主动连接(TCP_NODELAY 一并置位;RemoteDisplay 随成功物化)
  /// Active connect (TCP_NODELAY set in passing; RemoteDisplay
  /// materialized on success).
  static TcpSocket Connect(const SocketAddress& endpoint);

  /// GetStream().Write 的阻塞全量写(短写循环;失败 = false)
  /// The blocking full write of GetStream().Write (short-write loop;
  /// false = failure).
  bool SendAll(std::span<const std::uint8_t> span_bytes);

  /// 阻塞全量读(EOF/错误 = false)
  /// Blocking full read (EOF/error = false).
  bool ReceiveExact(std::uint8_t* p_buf, std::size_t sz_count);

  /// stream.ReadInt32():小端
  /// stream.ReadInt32(): little-endian.
  bool ReadInt32(std::int32_t& int4_out);

  /// (IPEndPoint)tcp.Client.RemoteEndPoint 的显示串(连接成功后有效)
  /// The display string of (IPEndPoint)tcp.Client.RemoteEndPoint (valid
  /// after a successful connect).
  const std::string& RemoteDisplay() const { return str_remote_; }

  /// Close():幂等;阻塞中的 recv 以失败唤醒,读线程自行退出
  /// Close(): idempotent; a blocking recv wakes as a failure and the
  /// reader thread exits on its own.
  void Close();

  // ———— 测试侧伪服务器面(上游 Server 随其批;此处仅最小承载)————
  // ———— The test-side fake-server face (upstream's Server rides its
  //    own batch; only the minimal surface lives here) ————

  /// 127.0.0.1:port 监听(port 0 = 系统分配;失败 = 无效 socket)
  /// Listen on 127.0.0.1:port (port 0 = system-assigned; failure = an
  /// invalid socket).
  static TcpSocket Listen(int int4_port);

  /// getsockname 的端口面(ephemeral 绑定后取实际端口)
  /// The getsockname port face (the actual port after an ephemeral
  /// bind).
  int BoundPort() const;

  /// 阻塞 accept(失败 = 无效 socket)
  /// Blocking accept (failure = an invalid socket).
  TcpSocket Accept();

 private:
  explicit TcpSocket(SOCKET h_socket) : h_socket_(h_socket) {}

  /// 句柄原子持有:Dispose 侧 Close 与读线程阻塞 recv 并发(值竞争 +
  /// WinSock 阻塞读需 shutdown 唤醒 —— .NET "关流使读抛错" 语义的直连形)
  /// The handle is held atomically: the Dispose-side Close races the
  /// reader thread's blocking recv (a value race + WinSock blocking reads
  /// needing a shutdown wake — the direct form of .NET's "closing the
  /// stream makes reads throw").
  std::atomic<SOCKET> h_socket_{INVALID_SOCKET};
  std::string str_remote_;
};

}  // namespace ora::platform
