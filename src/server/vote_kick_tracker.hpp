#pragma once
import std;

#include "net/session.hpp"
#include "server/server_connection.hpp"

namespace ora::server {

class Server;

class VoteKickTracker final {
 public:
  explicit VoteKickTracker(Server& server);

  void Tick();

  bool VoteKick(ServerConnection& conn, net::SessionClient& kicker,
                ServerConnection& kickee_conn, net::SessionClient& kickee,
                int int4_kickee_id, bool b_vote);

 private:

  bool ClientHasPower(const net::SessionClient& client) const;

  void EndKickVoteAndBlockKicker();
  void EndKickVote(bool b_send_message = true);

  Server& server_;
  std::map<int, bool> map_vote_tracker_;
  std::map<int, std::int64_t> map_failed_vote_kickers_;
  std::optional<std::chrono::steady_clock::time_point>
      opt_time_vote_start_;
  int int4_kickee_index_ = -1;
  std::shared_ptr<ServerConnection> ptr_kickee_conn_;
  int int4_vote_kicker_starter_index_ = -1;
  std::shared_ptr<ServerConnection>
      ptr_vote_kicker_starter_conn_;
};

}
