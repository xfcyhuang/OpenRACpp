#pragma once

import std;

#include "game/network/lobby_session.hpp"
#include "game/ui/widget.hpp"
#include "game/ui/widget_loader.hpp"

namespace ora::launcher {

namespace ui = game::ui;

struct LobbyMapEntry {
  std::string str_path;
  std::string str_name;
  std::string str_uid;
  std::string str_title;
  std::vector<game::network::LobbyPlayerRef> vec_players;
  std::vector<std::uint8_t> vec1u_map_png;
};

[[nodiscard]] std::expected<std::vector<LobbyMapEntry>, std::string> lobby_scan_maps(
    std::string_view str_dir);

[[nodiscard]] std::string lobby_default_client_name();

struct LobbyRuntime {
  game::network::LobbyServerModel server;
  std::vector<LobbyMapEntry> vec_entries;
  ui::WidgetModel tree_lobby;
  ui::WidgetLoaderModel loader_lobby;
  bool b_tree_loaded{};
};

[[nodiscard]] std::expected<void, std::string> lobby_runtime_create(
    LobbyRuntime& lobby_out, std::string_view str_maps_dir, std::string_view str_chrome_dir);

[[nodiscard]] const LobbyMapEntry* lobby_current_map(const LobbyRuntime& lobby);

bool lobby_cycle_map(LobbyRuntime& lobby, int int4_dir);

bool lobby_cycle_faction(LobbyRuntime& lobby, int int4_dir);

bool lobby_cycle_team(LobbyRuntime& lobby, int int4_dir);

[[nodiscard]] std::string lobby_faction_display(const LobbyRuntime& lobby);

[[nodiscard]] std::string lobby_team_display(const LobbyRuntime& lobby);

bool lobby_request_start(LobbyRuntime& lobby);

struct LobbyStartConfig {
  std::string str_map_path;
  std::int32_t int4_seed{};
  std::vector<std::pair<std::string, std::string>> vec_factions;
};

[[nodiscard]] LobbyStartConfig lobby_start_config(const LobbyRuntime& lobby);

}
