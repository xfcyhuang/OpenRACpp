#pragma once
import std;

#include "platform/udp_socket.hpp"

namespace ora::server {

class LanProbe final {
 public:
  struct Location {
    std::string str_address;
    std::int64_t i8_last_advertised = 0;
    std::string str_data;
  };

  explicit LanProbe(std::string str_beacon_type);
  ~LanProbe();

  LanProbe(const LanProbe&) = delete;
  LanProbe& operator=(const LanProbe&) = delete;

  bool IsValid() const { return b_valid_; }
  void Start();

  std::vector<Location> Locations();

 private:
  void Loop();

  std::string str_beacon_type_;
  platform::UdpSocket udp_;
  bool b_valid_ = false;
  std::atomic<bool> b_stopped_{false};
  std::thread thread_;
  std::mutex mutex_locations_;
  std::map<std::string, Location> map_locations_;
};

}  // namespace ora::server
