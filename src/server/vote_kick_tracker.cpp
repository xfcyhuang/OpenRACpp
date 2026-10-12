// UPSTREAM: OpenRA.Game/Server/VoteKickTracker.cs @e180e3225 L18-223
//          (实现体)
//          The implementation body.
#include "server/vote_kick_tracker.hpp"

#include "server/fluent_message.hpp"
#include "server/server.hpp"

namespace ora::server {

namespace {

constexpr std::string_view kInsufficientVotes =
    "notification-insufficient-votes-to-kick";
constexpr std::string_view kAlreadyVoted = "notification-kick-already-voted";
constexpr std::string_view kVoteKickStarted = "notification-vote-kick-started";
constexpr std::string_view kUnableToStartAVote =
    "notification-unable-to-start-a-vote";
constexpr std::string_view kVoteKickProgress =
    "notification-vote-kick-in-progress";
constexpr std::string_view kVoteKickEnded = "notification-vote-kick-ended";

std::int64_t ElapsedMs(const std::chrono::steady_clock::time_point& start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start)
      .count();
}

}  // namespace

VoteKickTracker::VoteKickTracker(Server& server) : server_(server) {}

bool VoteKickTracker::ClientHasPower(
    const net::SessionClient& client) const {
  return client.IsAdmin ||
         (!client.IsObserver() && !server_.HasClientWonOrLost(client));
}

void VoteKickTracker::Tick() {
  if (!opt_time_vote_start_)
    return;

  const bool b_kickee_online = std::any_of(
      server_.vec_conns.begin(), server_.vec_conns.end(),
      [this](const std::shared_ptr<ServerConnection>& c) {
        return c.get() == ptr_kickee_conn_.get();
      });
  if (!b_kickee_online) {
    EndKickVote();
    return;
  }

  if (ElapsedMs(*opt_time_vote_start_) > server_.Settings.VoteKickTimer)
    EndKickVoteAndBlockKicker();
}

bool VoteKickTracker::VoteKick(ServerConnection& conn,
                               net::SessionClient& kicker,
                               ServerConnection& kickee_conn,
                               net::SessionClient& kickee, int int4_kickee_id,
                               bool b_vote) {
  const bool b_vote_in_progress = opt_time_vote_start_.has_value();

  const bool b_same_ongoing_kickee =
      !b_vote_in_progress || int4_kickee_index_ == kickee.Index;
  if (server_.State() != ServerState::GameStarted ||
      (kickee.IsAdmin && server_.Type != ServerType::Dedicated) ||
      (!b_vote_in_progress && !b_vote) ||
      !b_same_ongoing_kickee || !ClientHasPower(kicker)) {
    server_.SendFluentMessageTo(conn, kUnableToStartAVote);
    return false;
  }

  short int2_eligible_players = 0;
  auto b_is_kickee_online = false;
  auto b_admin_is_dead_but_online = false;
  for (const auto& c : server_.vec_conns) {
    auto* client = server_.GetClient(*c);
    if (client != nullptr && client != &kickee && ClientHasPower(*client))
      int2_eligible_players++;

    if (c.get() == &kickee_conn)
      b_is_kickee_online = true;

    if (client != nullptr && client->IsAdmin &&
        (client->IsObserver() || server_.HasClientWonOrLost(*client)))
      b_admin_is_dead_but_online = true;
  }

  if (!b_is_kickee_online) {
    EndKickVote();
    return false;
  }

  if (int2_eligible_players < 2 ||
      (b_admin_is_dead_but_online && !kickee.IsAdmin &&
       int2_eligible_players < 3)) {
    if (!kickee.IsObserver() && !server_.HasClientWonOrLost(kickee)) {
      server_.SendFluentMessageTo(
          conn, kInsufficientVotes,
          std::vector<FluentArgument>{{"kickee", kickee.Name}});
      EndKickVote();
      return false;
    } else if (b_vote) {
      EndKickVote(false);
      return true;
    }
  }

  if (!b_vote_in_progress) {
    if (const auto it = map_failed_vote_kickers_.find(kicker.Index);
        it != map_failed_vote_kickers_.end()) {
      if (it->second + server_.Settings.VoteKickerCooldown >
          kickee_conn.ConnectionTimerMs()) {
        server_.SendFluentMessageTo(conn, kUnableToStartAVote);
        return false;
      } else {
        map_failed_vote_kickers_.erase(it);
      }
    }

    server_.ServerLog(std::format("Vote kick started on {}.", int4_kickee_id));
    opt_time_vote_start_ = std::chrono::steady_clock::now();
    server_.SendFluentMessage(
        kVoteKickStarted,
        std::vector<FluentArgument>{{"kicker", kicker.Name},
                                    {"kickee", kickee.Name}});

    auto o = net::Order{"StartKickVote", nullptr, false};
    o.uint4_extra_data = static_cast<std::uint32_t>(int4_kickee_id);
    server_.DispatchServerOrdersToClients(o.Serialize());

    int4_kickee_index_ = kickee.Index;
    ptr_kickee_conn_ = kickee_conn.shared_from_this();
    int4_vote_kicker_starter_index_ = kicker.Index;
    ptr_vote_kicker_starter_conn_ = conn.shared_from_this();
  }

  if (!map_vote_tracker_.count(conn.int4_player_index))
    map_vote_tracker_[conn.int4_player_index] = b_vote;
  else {
    server_.SendFluentMessageTo(conn, kAlreadyVoted);
    return false;
  }

  short int2_votes_for = 0;
  short int2_votes_against = 0;
  for (const auto& [int4_index, b_val] : map_vote_tracker_) {
    if (b_val)
      int2_votes_for++;
    else
      int2_votes_against++;
  }

  if (ClientHasPower(kickee)) {
    int2_eligible_players++;
    int2_votes_against++;
  }

  const auto int2_votes_needed = int2_eligible_players / 2 + 1;
  server_.SendFluentMessage(
      kVoteKickProgress,
      std::vector<FluentArgument>{
          {"kickee", kickee.Name},
          {"percentage", std::to_string(int2_votes_for * 100 /
                                        int2_eligible_players),
           FluentArgumentType::Number}});

  if (b_vote && (int2_votes_for >= int2_votes_needed)) {
    EndKickVote(false);
    return true;
  }

  if (int2_eligible_players - int2_votes_against < int2_votes_needed) {
    EndKickVoteAndBlockKicker();
    return false;
  }

  *opt_time_vote_start_ = std::chrono::steady_clock::now();
  return false;
}

void VoteKickTracker::EndKickVoteAndBlockKicker() {
  if (!opt_time_vote_start_)
    return;

  const bool b_starter_online = std::any_of(
      server_.vec_conns.begin(), server_.vec_conns.end(),
      [this](const std::shared_ptr<ServerConnection>& c) {
        return c.get() == ptr_vote_kicker_starter_conn_.get();
      });
  if (b_starter_online && ptr_vote_kicker_starter_conn_)
    map_failed_vote_kickers_[int4_vote_kicker_starter_index_] =
        ptr_vote_kicker_starter_conn_->ConnectionTimerMs();

  EndKickVote();
}

void VoteKickTracker::EndKickVote(bool b_send_message) {
  if (!opt_time_vote_start_)
    return;

  const auto* p_kickee = server_.LobbyInfo.ClientWithIndex(int4_kickee_index_);
  if (b_send_message && p_kickee != nullptr)
    server_.SendFluentMessage(kVoteKickEnded,
                              std::vector<FluentArgument>{
                                  {"kickee", p_kickee->Name}});

  if (p_kickee != nullptr) {
    auto o = net::Order{"EndKickVote", nullptr, false};
    o.uint4_extra_data = static_cast<std::uint32_t>(p_kickee->Index);
    server_.DispatchServerOrdersToClients(o.Serialize());
  }

  opt_time_vote_start_.reset();
  int4_vote_kicker_starter_index_ = -1;
  ptr_vote_kicker_starter_conn_.reset();
  int4_kickee_index_ = -1;
  ptr_kickee_conn_.reset();
  map_vote_tracker_.clear();
}

}  // namespace ora::server
