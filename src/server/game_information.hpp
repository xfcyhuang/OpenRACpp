// UPSTREAM: OpenRA.Game/GameInformation.cs @e180e3225 L21-237(Root/Player
//          双节 yaml 形态:字段序 = 声明序;DateTime → "yyyy-MM-dd HH-mm-ss"
//          不变量格式;WinState → 枚举名;玩家空位 = 上游 null 的
//          std::optional 承载 —— 服务器 defeat 跟踪保持 world.Players
//          的下标对齐)
//          GameInformation.cs L21-237 (the Root/Player two-node yaml shape:
//          field order = declaration order; DateTime → the invariant
//          "yyyy-MM-dd HH-mm-ss" format; WinState → the enum name; the
//          null player slots ride std::optional — the server's defeat
//          tracking keeps the world.Players index alignment).
#pragma once
import std;

#include "core/color.hpp"
#include "net/session.hpp"


namespace ora::server {

/// Player.cs L35 | Player.cs L35.
enum class WinState { Undefined = 0, Won = 1, Lost = 2 };

/// DateTime 的线承载(FieldSaver 的 "yyyy-MM-dd HH-mm-ss" 不变量格式;
/// 默认值 = .NET DateTime.MinValue 的 "0001-01-01 00-00-00")
/// DateTime's wire carrier (FieldSaver's invariant "yyyy-MM-dd HH-mm-ss"
/// format; the default = .NET DateTime.MinValue's "0001-01-01 00-00-00").
struct GameDateTime {
  std::string str_text = "0001-01-01 00-00-00";

  /// DateTime.UtcNow | DateTime.UtcNow.
  static GameDateTime UtcNow();
};

/// GameInformation.Player(GameInformation.cs L184-235)
/// GameInformation.Player (GameInformation.cs L184-235).
struct GameInformationPlayer {
  int ClientIndex = 0;
  std::string Name;
  bool IsHuman = false;
  bool IsBot = false;
  std::string BotType;
  std::string FactionName;
  std::string FactionId;
  core::Color Color{};
  std::string DisplayFactionName;
  std::string DisplayFactionId;
  int Team = 0;
  int SpawnPoint = 0;
  int Handicap = 0;
  bool IsRandomFaction = false;
  bool IsRandomSpawnPoint = false;
  std::string Fingerprint;

  WinState Outcome = WinState::Undefined;
  GameDateTime OutcomeTimestampUtc{};
  int DisconnectFrame = 0;
};

/// GameInformation(GameInformation.cs L21-236);服务器消费面 = 玩家胜负
/// 跟踪 + ReplayMetadata 载荷
/// GameInformation (GameInformation.cs L21-236); the server consumes the
/// win/loss tracking + the ReplayMetadata payload.
struct GameInformation {
  std::string Mod;
  std::string Version;
  std::string MapUid;
  std::string MapTitle;
  int FinalGameTick = 0;
  GameDateTime StartTimeUtc{};
  GameDateTime EndTimeUtc{};

  /// 上游 Players 列表(仅非 null 入列;world 玩家空位表 = 服务器侧的
  /// worldPlayers 向量,不入此结构)
  /// Upstream's Players list (only the non-null entries; the world-player
  /// null-slot table is the server-side worldPlayers vector, outside this
  /// struct).
  std::vector<GameInformationPlayer> vec_players;
  std::vector<int> vec_disabled_spawn_points;

  /// Serialize(L107-121):Root 节 + Player@i 节(空位跳过)
  /// Serialize (L107-121): the Root node + Player@i nodes (empty slots
  /// skipped).
  std::string Serialize() const;

  /// Deserialize(L71-105);失败 = nullopt
  /// Deserialize (L71-105); a parse failure yields nullopt.
  static std::optional<GameInformation> Deserialize(
      std::string_view str_data, std::string_view str_path);
};

}  // namespace ora::server
