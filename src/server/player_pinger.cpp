// UPSTREAM: OpenRA.Mods.Common/ServerTraits/PlayerPinger.cs @1f594e320
//          L18-104(实现体)
//          The implementation body.
#include "server/player_pinger.hpp"

#include "net/fluent_message.hpp"
#include "server/server.hpp"

namespace ora::server {

using ora::net::FluentArgument;
using ora::net::FluentArgumentType;
using ora::net::FluentMessage;

namespace {

constexpr std::string_view kPlayerDropped = "notification-player-dropped";
constexpr std::string_view kConnectionProblems =
    "notification-connection-problems";
constexpr std::string_view kTimeout = "notification-timeout-dropped";
constexpr std::string_view kTimeoutIn = "notification-timeout-dropped-in";

}  // namespace

void PlayerPinger::Tick(Server& server) {
  const auto i8_now = server.RunTimeMs();
  if ((i8_now - i8_last_ping_ > kPingInterval) || b_is_initial_ping_) {
    b_is_initial_ping_ = false;
    i8_last_ping_ = i8_now;

    std::size_t sz_non_bot_client_count = 0;
    {
      std::lock_guard lock{server.mutex_lobby};
      sz_non_bot_client_count = server.LobbyInfo.NonBotClients().size();
    }

    if (sz_non_bot_client_count >= 2 || server.Type == ServerType::Dedicated) {
      auto vec_conns = server.vec_conns;
      for (const auto& c : vec_conns) {
        if (!c->b_validated.load())
          continue;

        auto* client = server.GetClient(*c);
        if (client == nullptr) {
          server.DropClient(*c);
          server.SendFluentMessage(kPlayerDropped);
          continue;
        }

        if (c->TimeSinceLastResponse() < kConnTimeout) {
          if (!c->b_timeout_message_shown.load() &&
              c->TimeSinceLastResponse() > kPingInterval * 2) {
            server.SendFluentMessage(
                kConnectionProblems,
                std::vector<FluentArgument>{{"player", client->Name}});
            c->b_timeout_message_shown.store(true);
          }
        } else {
          server.SendFluentMessage(
              kTimeout, std::vector<FluentArgument>{{"player", client->Name}});
          server.DropClient(*c);
        }
      }

      if (i8_now - i8_last_conn_report_ > kConnReportInterval) {
        i8_last_conn_report_ = i8_now;

        std::vector<std::pair<std::int64_t, std::shared_ptr<ServerConnection>>>
            vec_timeouts;
        for (const auto& c : server.vec_conns)
          if (c->b_validated.load() &&
              c->TimeSinceLastResponse() > kConnReportInterval &&
              c->TimeSinceLastResponse() < kConnTimeout)
            vec_timeouts.emplace_back(c->TimeSinceLastResponse(), c);

        std::sort(vec_timeouts.begin(), vec_timeouts.end(),
                  [](const auto& a, const auto& b) {
                    return a.first < b.first;
                  });

        for (const auto& [i8_elapsed, c] : vec_timeouts) {
          auto* client = server.GetClient(*c);
          if (client != nullptr) {
            const auto int4_timeout =
                static_cast<int>(kConnTimeout - i8_elapsed) / 1000;
            server.SendFluentMessage(
                kTimeoutIn,
                std::vector<FluentArgument>{
                    {"player", client->Name},
                    {"timeout", std::to_string(int4_timeout),
                     FluentArgumentType::Number}});
          }
        }
      }
    }
  }
}

}  // namespace ora::server
