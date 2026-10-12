// UPSTREAM: OpenRA.Game/Server/VoteKickTracker.cs @e180e3225 L18-223 全文
//          (ClientHasPower 的管理员/存活双门/Tick 的离场结束与超时
//          阻断/VoteKick 的七门 + 合法票数账本 + 起始广播与逐票进度 +
//          EndKickVoteAndBlockKicker 的失败冷却/EndKickVote 的全清;
//          (Client, Conn) 元组 = Client.Index + shared_ptr<ServerConnection>
//          承载 —— 上游按对象引用恒等,C++ 以 Index 恒等(索引唯一,等价;
//          登记 D223)
//          The whole of VoteKickTracker.cs L18-223 (ClientHasPower's
//          admin/alive double gate / Tick's departure-end and timeout-block /
//          VoteKick's seven gates + the eligible-votes ledger + the start
//          broadcast and the per-vote progress + EndKickVoteAndBlockKicker's
//          failure cooldown / EndKickVote's full clear; the (Client, Conn)
//          tuple = Client.Index + a shared_ptr<ServerConnection> — upstream
//          keys by object identity, C++ keys by Index (indexes unique,
//          equivalent; registered as D223).
#pragma once
import std;

#include "net/session.hpp"
#include "server/server_connection.hpp"


namespace ora::server {

class Server;

/// VoteKickTracker(VoteKickTracker.cs L18)
/// VoteKickTracker (VoteKickTracker.cs L18).
class VoteKickTracker final {
 public:
  explicit VoteKickTracker(Server& server);

  void Tick();

  /// VoteKick(L69-192);返回 true = 票数达标应踢
  /// VoteKick (L69-192); true = the vote threshold met, kick.
  bool VoteKick(ServerConnection& conn, net::SessionClient& kicker,
                ServerConnection& kickee_conn, net::SessionClient& kickee,
                int int4_kickee_id, bool b_vote);

 private:
  /// ClientHasPower(L51-52):管理员或未分胜负的非观察者
  /// ClientHasPower (L51-52): an admin, or a non-observer without a verdict.
  bool ClientHasPower(const net::SessionClient& client) const;

  void EndKickVoteAndBlockKicker();
  void EndKickVote(bool b_send_message = true);

  Server& server_;
  std::map<int, bool> map_vote_tracker_;                 // L38(conn 索引 → 票)
  std::map<int, std::int64_t> map_failed_vote_kickers_;  // L39(客户端索引 → 时间)
  std::optional<std::chrono::steady_clock::time_point>
      opt_time_vote_start_;  // L42(Stopwatch voteKickTimer;null = 无票)
  int int4_kickee_index_ = -1;  // L43 kickee.Client
  std::shared_ptr<ServerConnection> ptr_kickee_conn_;  // L43 kickee.Conn
  int int4_vote_kicker_starter_index_ = -1;  // L44 voteKickerStarter.Client
  std::shared_ptr<ServerConnection>
      ptr_vote_kicker_starter_conn_;  // L44 voteKickerStarter.Conn
};

}  // namespace ora::server
