#include "server/lan_probe.hpp"

namespace ora::server {

namespace {

constexpr int kDiscoveryPort = 35891;
constexpr auto kProbePeriod = std::chrono::milliseconds{1000};
constexpr auto kBeaconTimeout = std::chrono::milliseconds{15000};

std::vector<std::uint8_t> BeaconEncode(std::string_view sv_text) {
  auto vec_out = std::vector<std::uint8_t>{};
  const auto u2_len = static_cast<std::uint16_t>(sv_text.size());
  vec_out.push_back(static_cast<std::uint8_t>(u2_len & 0xFF));
  vec_out.push_back(static_cast<std::uint8_t>((u2_len >> 8) & 0xFF));
  vec_out.insert(vec_out.end(), sv_text.begin(), sv_text.end());
  return vec_out;
}

std::int64_t SteadyNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}

LanProbe::LanProbe(std::string str_beacon_type)
    : str_beacon_type_{std::move(str_beacon_type)} {
  udp_ = platform::UdpSocket::BindAny(0);
  b_valid_ = udp_.IsValid();
}

LanProbe::~LanProbe() {
  b_stopped_.store(true);
  udp_.Close();
  if (thread_.joinable())
    thread_.join();
}

void LanProbe::Start() {
  if (thread_.joinable())
    return;
  b_stopped_.store(false);
  thread_ = std::thread{[this]() { Loop(); }};
}

std::vector<LanProbe::Location> LanProbe::Locations() {
  std::lock_guard lock{mutex_locations_};
  auto vec_out = std::vector<Location>{};
  const std::int64_t i8_now = SteadyNowMs();
  for (auto it = map_locations_.begin(); it != map_locations_.end();) {
    if (i8_now - it->second.i8_last_advertised > kBeaconTimeout.count())
      it = map_locations_.erase(it);
    else {
      vec_out.push_back(it->second);
      ++it;
    }
  }
  return vec_out;
}

void LanProbe::Loop() {
  const auto vec_probe = BeaconEncode(str_beacon_type_);

  auto addr_broadcast = platform::SocketAddress{};
  addr_broadcast.addr_storage.ss_family = AF_INET;
  auto& addr_in = reinterpret_cast<sockaddr_in&>(addr_broadcast.addr_storage);
  addr_in.sin_family = AF_INET;
  addr_in.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  addr_in.sin_port = htons(static_cast<u_short>(kDiscoveryPort));
  addr_broadcast.len_addr = sizeof(sockaddr_in);

  auto arr_buf = std::array<std::uint8_t, 65536>{};
  std::int64_t i8_last_probe = 0;

  while (!b_stopped_.load()) {
    const std::int64_t i8_now = SteadyNowMs();
    if (i8_now - i8_last_probe >= kProbePeriod.count()) {
      i8_last_probe = i8_now;
      udp_.SendTo(addr_broadcast, vec_probe);
    }

    while (udp_.PollReadable(100000)) {
      auto addr_peer = platform::SocketAddress{};
      const int int4_n =
          udp_.ReceiveFrom(arr_buf.data(), arr_buf.size(), addr_peer);
      if (int4_n <= 0 || b_stopped_.load())
        break;

      const auto span_packet =
          std::span{arr_buf.data(), arr_buf.data() + int4_n};
      if (span_packet.size() < vec_probe.size() + 4 ||
          !std::equal(vec_probe.begin(), vec_probe.end(),
                      span_packet.begin()))
        continue;

      std::size_t sz_i = vec_probe.size();
      const auto u2_port = static_cast<std::uint16_t>(
          (static_cast<std::uint16_t>(span_packet[sz_i]) << 8) |
          static_cast<std::uint16_t>(span_packet[sz_i + 1]));
      sz_i += 2;

      if (sz_i + 2 > span_packet.size())
        continue;
      const std::size_t sz_len =
          static_cast<std::size_t>(span_packet[sz_i]) |
          (static_cast<std::size_t>(span_packet[sz_i + 1]) << 8);
      sz_i += 2;
      if (sz_i + sz_len != span_packet.size())
        continue;

      const std::string str_data{
          reinterpret_cast<const char*>(span_packet.data() + sz_i), sz_len};

      const std::size_t sz_colon = addr_peer.str_display.find(':');
      const std::string str_ip =
          sz_colon == std::string::npos
              ? addr_peer.str_display
              : addr_peer.str_display.substr(0, sz_colon);

      Location location{};
      location.str_address =
          std::format("{}:{}", str_ip, static_cast<int>(u2_port));
      location.i8_last_advertised = i8_now;
      location.str_data = str_data;

      std::lock_guard lock{mutex_locations_};
      map_locations_[location.str_address] = std::move(location);
    }
  }
}

}
