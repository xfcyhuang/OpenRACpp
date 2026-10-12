// UPSTREAM: OpenRA.Mods.Common/ServerTraits/PlayerPinger.cs @e180e3225
//          L18-104 全文(5s ping 周期的无 client 踢/超时预警/60s 超时
//          踢线 + 20s 周期的超时倒计汇报;单机 < 2 人且非专用服豁免;
//          Game.RunTime → Server::RunTimeMs 注入面)
//          The whole of PlayerPinger.cs L18-104 (the 5s-ping cycle's
//          client-less kick / the stall warning / the 60s-timeout drop +
//          the 20s cycle's timeout countdown report; the sub-2-player
//          non-dedicated exemption; Game.RunTime → the Server::RunTimeMs
//          injection face).
#pragma once
import std;

#include "server/server_traits.hpp"


namespace ora::server {

class PlayerPinger final : public ServerTrait, public ITick {
 public:
  void Tick(Server& server) override;

 private:
  static constexpr int kPingInterval = 5000;      // L32
  static constexpr int kConnReportInterval = 20000;  // L33
  static constexpr int kConnTimeout = 60000;      // L34

  std::int64_t i8_last_ping_ = 0;
  std::int64_t i8_last_conn_report_ = 0;
  bool b_is_initial_ping_ = true;
};

}  // namespace ora::server
