#pragma once
import std;

#include "server/server_traits.hpp"

namespace ora::server {

class PlayerPinger final : public ServerTrait, public ITick {
 public:
  void Tick(Server& server) override;

 private:
  static constexpr int kPingInterval = 5000;
  static constexpr int kConnReportInterval = 20000;
  static constexpr int kConnTimeout = 60000;

  std::int64_t i8_last_ping_ = 0;
  std::int64_t i8_last_conn_report_ = 0;
  bool b_is_initial_ping_ = true;
};

}
