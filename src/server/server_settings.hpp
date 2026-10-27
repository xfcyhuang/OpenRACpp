#pragma once
import std;

#include "net/sanitize.hpp"

namespace ora::server {

struct ServerSettings {
  std::string Name;
  int ListenPort = 1234;
  bool AdvertiseOnline = true;
  bool AdvertiseOnLocalNetwork = true;
  std::string Password;
  bool DiscoverNatDevices = false;
  int NatPortMappingLifetime = 36000;
  std::string Map;
  std::vector<std::string> vec_ban;
  bool RequireAuthentication = false;
  std::vector<int> vec_profile_id_whitelist;
  std::vector<int> vec_profile_id_blacklist;
  bool EnableSingleplayer = false;
  bool EnableMapGeneration = true;
  bool QueryMapRepository = true;
  bool EnableSyncReports = false;
  std::string TimestampFormat = "yyyy-MM-ddTHH:mm:ss";
  bool ShareAnonymizedIPs = true;
  bool EnableGeoIP = true;
  bool RecordReplays = false;
  bool EnableLintChecks = true;
  std::vector<std::string> vec_map_pool;
  int FloodLimitJoinCooldown = 5000;
  int FloodLimitInterval = 5000;
  int FloodLimitMessageCount = 5;
  int FloodLimitCooldown = 15000;
  bool EnableVoteKick = true;
  int VoteKickTimer = 30000;
  int VoteKickerCooldown = 120000;

  std::string SanitizedServerName(std::string_view str_dirty,
                                  std::string_view str_player_name) const;

  ServerSettings Clone() const { return *this; }
};

using ora::net::SanitizedName;
using ora::net::SanitizedPlayerName;

}
