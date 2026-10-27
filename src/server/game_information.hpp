#pragma once
import std;

#include "core/color.hpp"
#include "net/session.hpp"

namespace ora::server {

enum class WinState { Undefined = 0, Won = 1, Lost = 2 };

struct GameDateTime {
  std::string str_text = "0001-01-01 00-00-00";

  static GameDateTime UtcNow();

  std::int64_t ToUnixSeconds() const;
};

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

struct GameInformation {
  std::string Mod;
  std::string Version;
  std::string MapUid;
  std::string MapTitle;
  int FinalGameTick = 0;
  GameDateTime StartTimeUtc{};
  GameDateTime EndTimeUtc{};

  std::vector<GameInformationPlayer> vec_players;
  std::vector<int> vec_disabled_spawn_points;

  std::string Serialize() const;

  double DurationSeconds() const {
    const std::int64_t t_start = StartTimeUtc.ToUnixSeconds();
    const std::int64_t t_end = EndTimeUtc.ToUnixSeconds();
    return t_end > t_start ? static_cast<double>(t_end - t_start) : 0.0;
  }

  bool IsSinglePlayer() const {
    std::size_t n_humans = 0;
    for (const auto& player : vec_players)
      if (player.IsHuman && ++n_humans > 1)
        return false;
    return n_humans == 1;
  }

  static std::optional<GameInformation> Deserialize(
      std::string_view str_data, std::string_view str_path);
};

}
