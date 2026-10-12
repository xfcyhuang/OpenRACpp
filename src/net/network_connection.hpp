// UPSTREAM: OpenRA.Game/Network/Connection.cs @e180e3225 L99-386(Network
//          Connection 全文:连接竞赛/读线程/发送链/Ack·Ping·TickScale 分发;
//          线程与并发原语的形态偏离 D203~D206,见 docs/COVERAGE.md)+
//          OpenRA.Game/Server/ProtocolVersion.cs L75(Handshake = 7)
//          The whole of NetworkConnection (L99-386): the connect race/
//          reader thread/send chain/Ack·Ping·TickScale dispatch; the
//          thread-and-concurrency shape deviations D203-D206, see
//          docs/COVERAGE.md + ProtocolVersion.cs L75 (Handshake = 7).
#pragma once
import std;

#include "net/connection.hpp"
#include "net/connection_target.hpp"
#include "net/replay_recorder.hpp"

namespace ora::net {

/// ProtocolVersion.Handshake(ProtocolVersion.cs L75;握手协议版本,上
/// 游注释:不兼容变更会破坏运行时 mod 切换,最后手段才改)
/// ProtocolVersion.Handshake (ProtocolVersion.cs L75; the handshake
/// protocol version — upstream note: incompatible changes break runtime
/// mod switching, change only as a last resort).
inline constexpr int kHandshakeProtocolVersion = 7;

/// NetworkConnection(Connection.cs L99-386):联机 TcpClient 面。
/// 线程模型(D203):上游 background Thread + BlockingCollection +
/// ConcurrentQueue → std::thread(Dispose 确定性 join)+ mutex/cv 阻塞
/// 集合 + mutex 双端队列;volatile → atomic。
/// NetworkConnection (Connection.cs L99-386): the online TcpClient face.
/// Thread model (D203): upstream's background Thread + BlockingCollection
/// + ConcurrentQueue → std::thread (deterministically joined at Dispose)
/// + a mutex/cv blocking collection + a mutex deque; volatile → atomic.
class NetworkConnection final : public IConnection {
 public:
  explicit NetworkConnection(ConnectionTarget target);
  ~NetworkConnection() override { Dispose(); }

  NetworkConnection(const NetworkConnection&) = delete;
  NetworkConnection& operator=(const NetworkConnection&) = delete;

  const ConnectionTarget& Target() const { return target_; }

  /// ConnectionState 属性(L124;volatile 读 → relaxed atomic)
  /// The ConnectionState property (L124; volatile read → relaxed atomic).
  ConnectionState State() const {
    return connection_state_.load(std::memory_order_relaxed);
  }

  /// EndPoint 属性(L126):连接成功即物化的远端显示串(断线后仍可用)
  /// The EndPoint property (L126): the remote display string materialized
  /// at connect (still valid after a disconnect).
  std::string EndPoint() const;

  /// ErrorMessage 属性(L128):连接失败的可展示原因
  /// The ErrorMessage property (L128): the displayable failure cause.
  std::string ErrorMessage() const;

  ReplayRecorder* Recorder() { return ptr_recorder_.get(); }

  int LocalClientId() override {
    return client_id_.load(std::memory_order_relaxed);
  }

  void StartGame() override {}

  void Send(int frame, const std::vector<const Order*>& orders) override;
  void SendImmediate(const std::vector<const Order*>& orders) override;
  void SendSync(int frame, int sync_hash,
                std::uint64_t uint8_defeat_state) override;
  void Receive(OrderManager& order_manager) override;

  /// StartRecording(L366-371):旧录制先存盘释放,再开新录制
  /// StartRecording (L366-371): the previous recording is saved/disposed,
  /// then a new one starts.
  void StartRecording(ReplayRecorder::ChooseFilename fn_choose_filename);

  /// Dispose(L373-385):幂等;关 socket 唤醒读线程,join 两线程,释放录制
  /// Dispose (L373-385): idempotent; closes the socket to wake the reader
  /// thread, joins both threads, disposes the recorder.
  void Dispose();

 private:
  void ConnectBody();
  void ReceiveBody();
  void FailReceive();

  /// Send(byte[])(L258-283):packet + 排队 sync 包冲刷 + 单次流写
  /// Send(byte[]) (L258-283): the packet + queued-sync flush + the single
  /// stream write.
  void SendRaw(const std::vector<std::uint8_t>& vec_packet);

  /// BlockingCollection<TcpClient> 的 mutex/cv 等价(D203):
  /// 首个成功连接胜出;CompleteAdding 后 Add 失败 = 落选者自关
  /// The mutex/cv counterpart of BlockingCollection<TcpClient> (D203):
  /// the first successful connect wins; Add after CompleteAdding fails =
  /// the loser closes itself.
  struct ConnectCollection {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<platform::TcpSocket> vec_ready;
    bool b_completed = false;

    bool Add(platform::TcpSocket tcp);
    bool TryTake(platform::TcpSocket& tcp_out, int int4_timeout_ms);
    void CompleteAdding();
    std::vector<platform::TcpSocket> TakeRemainder();
  };

  ConnectionTarget target_;
  std::unique_ptr<ReplayRecorder> ptr_recorder_;

  std::deque<SyncPacketData> deque_sent_sync_;
  std::deque<SyncPacketData> deque_queued_sync_;

  std::deque<std::pair<int, OrderPacket>> deque_sent_orders_;
  std::deque<OrderPacket> deque_sent_immediate_;

  /// ConcurrentQueue → mutex 双端队列(生产 = 读线程,消费 = 主线程)
  /// ConcurrentQueue → a mutex deque (producer = the reader thread,
  /// consumer = the main thread).
  std::mutex mutex_received_;
  std::deque<Packet> deque_received_;

  std::mutex mutex_socket_;
  platform::TcpSocket tcp_;

  std::atomic<ConnectionState> connection_state_{ConnectionState::Connecting};
  std::atomic<int> client_id_{0};
  std::atomic<bool> b_disposed_{false};

  mutable std::mutex mutex_status_;
  std::string str_end_point_;
  std::string str_error_;

  std::thread thread_connect_;
  std::thread thread_receive_;
  std::shared_ptr<ConnectCollection> ptr_connect_collection_;
};

}  // namespace ora::net
