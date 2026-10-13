// UPSTREAM: OpenRA.Game/Settings.cs @e180e3225 L92-154(ServerSettings 全
//          字段 + 默认值逐字;L500-541 的 SanitizedName/SanitizedServerName/
//          SanitizedPlayerName 全文 —— MiniYAML/JSON 保留字符剥离与 16 字
//          截断;Ban/MapPool 的 FrozenSet → 插入序 vector,Whitelist/
//          Blacklist 同形)
//          Settings.cs L92-154 (ServerSettings' every field + default,
//          verbatim) + L500-541 (the whole of SanitizedName/
//          SanitizedServerName/SanitizedPlayerName — the MiniYAML/JSON
//          reserved-character strip and the 16-char truncation; Ban's/
//          MapPool's FrozenSet → insertion-ordered vectors, Whitelist/
//          Blacklist the same shape).
#pragma once
import std;

#include "net/sanitize.hpp"

namespace ora::server {

/// ServerSettings(Settings.cs L92-154)
/// ServerSettings (Settings.cs L92-154).
struct ServerSettings {
  std::string Name;                                            // L95
  int ListenPort = 1234;                                       // L98
  bool AdvertiseOnline = true;                                 // L101
  bool AdvertiseOnLocalNetwork = true;                         // L104
  std::string Password;                                        // L107
  bool DiscoverNatDevices = false;                             // L110
  int NatPortMappingLifetime = 36000;                          // L113
  std::string Map;                                             // L116(null→"")
  std::vector<std::string> vec_ban;                            // L119
  bool RequireAuthentication = false;                          // L122
  std::vector<int> vec_profile_id_whitelist;                   // L125
  std::vector<int> vec_profile_id_blacklist;                   // L128
  bool EnableSingleplayer = false;                             // L131
  bool EnableMapGeneration = true;                             // L134
  bool QueryMapRepository = true;                              // L137
  bool EnableSyncReports = false;                              // L140
  std::string TimestampFormat = "yyyy-MM-ddTHH:mm:ss";         // L143
  bool ShareAnonymizedIPs = true;                              // L146
  bool EnableGeoIP = true;                                     // L149
  bool RecordReplays = false;                                  // L152
  bool EnableLintChecks = true;                                // L155
  std::vector<std::string> vec_map_pool;                       // L158
  int FloodLimitJoinCooldown = 5000;                           // L161
  int FloodLimitInterval = 5000;                               // L164
  int FloodLimitMessageCount = 5;                              // L167
  int FloodLimitCooldown = 15000;                              // L170
  bool EnableVoteKick = true;                                  // L173
  int VoteKickTimer = 30000;                                   // L176
  int VoteKickerCooldown = 120000;                             // L179

  /// SanitizedServerName(L515-522;空/空白回落 "{player}'s Game")
  /// SanitizedServerName (L515-522; empty/whitespace falls back to
  /// "{player}'s Game").
  std::string SanitizedServerName(std::string_view str_dirty,
                                  std::string_view str_player_name) const;

  /// Settings 模块的 Commit 面(上游 SettingsModule;C++ 侧 = 值直传)
  /// The settings module's Commit face (upstream's SettingsModule; the
  /// C++ side passes values straight through).
  ServerSettings Clone() const { return *this; }
};

/// SanitizedName/SanitizedPlayerName(L500-538)已随第十六批迁入
/// ora::net(net/sanitize.hpp —— client/server 双消费的 lib 层属);此处
/// 以 using 转发保持 ora::server 侧旧调用点
/// SanitizedName/SanitizedPlayerName (L500-538) moved into ora::net
/// (net/sanitize.hpp — the lib layering for the client/server double
/// consumption) with batch 16; forwarded here by using-declaration for
/// the existing ora::server call sites.
using ora::net::SanitizedName;
using ora::net::SanitizedPlayerName;

}  // namespace ora::server
