// UPSTREAM: OpenRA.Game/Server/Connection.cs @1f594e320 L24-220(服务器侧
//          每连接线程全文:100ms Poll 读帧机(Header/Data 状态机 + 128kB
//          超长闸)、Ping 的线程内自处理(历史 15 样本上限)、发送队列的
//          WouldBlock 回落阻塞发送、空包断连语义;BlockingCollection →
//          mutex/cv 集合(D203 同形),background Thread → 自持 shared
//          引用的 std::thread(末路自分离 = GC 常青语义的确定性等价))
//          The whole of the server-side per-connection thread, Connection.
//          cs L24-220 (the 100ms-poll read frame machine with the Header/
//          Data states + the 128kB oversize gate, the inline Ping handling
//          with the 15-sample history cap, the send queue's WouldBlock
//          fallback to a blocking send, the empty-packet disconnect
//          semantics; BlockingCollection → the mutex/cv collection (the
//          same shape as D203), the background Thread → a shared-self-
//          holding std::thread (the final self-detach = the deterministic
//          equivalent of GC evergreen liveness)).
#pragma once
import std;

#include "net/order_io.hpp"
#include "platform/tcp_socket.hpp"


namespace ora::server {

class Server;

/// Connection(Connection.cs L24-217)
/// Connection (Connection.cs L24-217).
class ServerConnection : public std::enable_shared_from_this<ServerConnection> {
 public:
  static constexpr int kMaxOrderLength = 131072;  // L26
  static constexpr int kMaxPingSamples = 15;      // L29

  using ClockFn = std::function<std::int64_t()>;

  const int int4_player_index = 0;
  const std::string str_auth_token;
  const std::string str_end_point;

  std::atomic<bool> b_timeout_message_shown{false};  // L38
  std::atomic<bool> b_validated{false};              // L39
  std::atomic<int> int4_last_orders_frame{0};        // L40

  ServerConnection(int int4_player_index, std::string str_auth_token,
                   std::string str_end_point,
                   platform::TcpSocket tcp_accepted, ClockFn fn_clock);

  /// 读线程的启动面(shared_from_this 就绪后由服务器调用)
  /// The reader-thread start face (called by the server once
  /// shared_from_this is ready).
  void StartReader(Server& server, bool b_is_multiplayer);

  /// TimeSinceLastResponse(L36)
  std::int64_t TimeSinceLastResponse() const;

  /// ConnectionTimer 的毫秒面(PlayerMessageTracker 消费)
  /// The ConnectionTimer's milliseconds face (consumed by
  /// PlayerMessageTracker).
  std::int64_t ConnectionTimerMs() const;

  /// TrySendData(L195-210):入发送队列;已完结 = false
  /// TrySendData (L195-210): enqueue; completed = false.
  bool TrySendData(std::vector<std::uint8_t> vec_data);

  /// Dispose(L212-216):CompleteAdding 的信号语义(线程退出由读循环
  /// 自决;join/分离见析构)
  /// Dispose (L212-216): the CompleteAdding signal (the thread's exit is
  /// the read loop's own decision; the join/detach lives in the dtor).
  void Dispose();

  ~ServerConnection();

 private:
  /// CreatePingFrame(L60-69):len=13 帧(Ping + RunTime)
  /// CreatePingFrame (L60-69): the len-13 frame (Ping + RunTime).
  std::vector<std::uint8_t> CreatePingFrame() const;

  /// SendReceiveLoop(L71-193)全文
  /// The whole of SendReceiveLoop (L71-193).
  void SendReceiveLoop(Server* p_server, bool b_is_multiplayer);

  ClockFn fn_clock_;
  platform::TcpSocket tcp_;
  std::chrono::steady_clock::time_point time_connected_{};
  std::atomic<std::int64_t> i8_last_received{0};

  /// BlockingCollection<byte[]> → mutex/cv 集合(D203 形)
  /// BlockingCollection<byte[]> → the mutex/cv collection (the D203
  /// shape).
  std::mutex mutex_send_;
  std::condition_variable cv_send_;
  std::deque<std::vector<std::uint8_t>> deque_send_;
  bool b_send_completed_ = false;

  std::thread thread_;
  bool b_reader_started_ = false;
};

}  // namespace ora::server
