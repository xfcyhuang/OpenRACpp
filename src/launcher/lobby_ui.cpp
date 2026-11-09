#include "launcher/lobby_ui.hpp"

#include "game/filesystem/zip_read.hpp"
#include "game/map_read.hpp"
#include "game/miniyaml.hpp"
#include "game/render/png_decode.hpp"

namespace ora::launcher {

namespace {

namespace net = game::network;
namespace ogf = game::filesystem;

std::string read_file_string(const std::filesystem::path& path) {
  std::ifstream stream_in{path, std::ios::binary};
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::vector<std::uint8_t> to_bytes(const std::string& str_text) {
  return {str_text.begin(), str_text.end()};
}

std::vector<net::LobbyMapInfo> make_map_infos(const std::vector<LobbyMapEntry>& vec_entries) {
  std::vector<net::LobbyMapInfo> vec_out;
  for (const LobbyMapEntry& entry : vec_entries) {
    net::LobbyMapInfo info;
    info.str_name = entry.str_name;
    info.str_uid = entry.str_uid;
    info.str_title = entry.str_title;
    info.vec_players = entry.vec_players;
    info.vec_selectable_factions = net::ra_selectable_factions();
    vec_out.push_back(std::move(info));
  }
  return vec_out;
}

}

std::expected<std::vector<LobbyMapEntry>, std::string> lobby_scan_maps(
    std::string_view str_dir) {
  std::vector<LobbyMapEntry> vec_out;
  std::error_code ec;
  std::filesystem::path path_dir{std::string{str_dir}};
  if (!std::filesystem::exists(path_dir, ec))
    return std::unexpected(std::format("maps dir not found: {}", str_dir));

  std::vector<std::filesystem::path> vec_paths;
  for (const std::filesystem::path& path : std::filesystem::directory_iterator(path_dir, ec)) {
    if (path.extension() == ".oramap") vec_paths.push_back(path);
  }
  std::sort(vec_paths.begin(), vec_paths.end());
  if (vec_paths.empty())
    return std::unexpected(std::format("maps dir has no .oramap: {}", str_dir));

  for (const std::filesystem::path& path : vec_paths) {
    const std::string str_bytes{read_file_string(path)};
    if (str_bytes.empty()) continue;
    const auto res_zip{ogf::zip_read(to_bytes(str_bytes))};
    if (!res_zip.has_value()) continue;
    const auto vec_yaml{ogf::zip_entry_data(*res_zip, "map.yaml")};
    const auto vec_png{ogf::zip_entry_data(*res_zip, "map.png")};
    if (!vec_yaml.has_value() || !vec_png.has_value()) continue;
    const std::string str_yaml{vec_yaml->begin(), vec_yaml->end()};

    LobbyMapEntry entry;
    entry.str_path = path.string();
    entry.str_name = path.stem().string();
    entry.vec1u_map_png = *vec_png;
    if (const auto res_uid{game::map_uid(*res_zip)}; res_uid.has_value())
      entry.str_uid = *res_uid;
    if (const auto res_header{game::map_header_parse(*vec_yaml, "map.yaml")};
        res_header.has_value())
      entry.str_title = res_header->str_title;
    if (auto res_refs{net::lobby_player_refs_from_map_yaml(str_yaml, entry.str_name)};
        res_refs.has_value())
      entry.vec_players = std::move(*res_refs);
    vec_out.push_back(std::move(entry));
  }

  if (vec_out.empty())
    return std::unexpected(std::format("maps dir has no loadable .oramap: {}", str_dir));
  return vec_out;
}

std::string lobby_default_client_name() { return "Commander"; }

std::expected<void, std::string> lobby_runtime_create(LobbyRuntime& lobby,
                                                      std::string_view str_maps_dir,
                                                      std::string_view str_chrome_dir) {
  auto res_entries{lobby_scan_maps(str_maps_dir)};
  if (!res_entries.has_value()) return std::unexpected(res_entries.error());

  lobby.vec_entries = std::move(*res_entries);

  const auto res_tree{game::miniyaml_from_file_named(
      std::string{str_chrome_dir} + "/lobby.yaml", "lobby.yaml")};
  if (res_tree.has_value()) {
    if (const auto res_loader{ui::widget_loader_from_nodes(*res_tree)}; res_loader.has_value()) {
      lobby.loader_lobby = std::move(*res_loader);
      lobby.tree_lobby = ui::WidgetModel{};
      lobby.tree_lobby.str_type = "Container";
      lobby.tree_lobby.bounds.int4_width = 1024;
      lobby.tree_lobby.bounds.int4_height = 768;
      ui::WidgetInitCtx ctx{};
      ctx.int4_window_width = 1024;
      ctx.int4_window_height = 768;
      if (const auto res_widget{
              ui::widget_loader_load_widget(lobby.loader_lobby, lobby.tree_lobby, ctx,
                                            "SERVER_LOBBY")};
          res_widget.has_value())
        lobby.b_tree_loaded = true;
    }
  }

  std::size_t sz_initial{0};
  for (std::size_t sz_i{0}; sz_i < lobby.vec_entries.size(); ++sz_i)
    if (lobby.vec_entries[sz_i].str_name == "ore-lord") sz_initial = sz_i;
  lobby.server.server_start(make_map_infos(lobby.vec_entries), sz_initial);
  lobby.server.join_local_client(lobby_default_client_name(), "C81E28");
  return {};
}

const LobbyMapEntry* lobby_current_map(const LobbyRuntime& lobby) {
  const net::LobbyMapInfo* ptr_info{nullptr};
  for (const net::LobbyMapInfo& info : lobby.server.vec_maps)
    if (info.str_uid == lobby.server.lobby_info.global.str_map) ptr_info = &info;
  if (ptr_info == nullptr) return nullptr;
  for (const LobbyMapEntry& entry : lobby.vec_entries)
    if (entry.str_uid == ptr_info->str_uid) return &entry;
  return nullptr;
}

bool lobby_cycle_map(LobbyRuntime& lobby, int int4_dir) {
  const LobbyMapEntry* ptr_current{lobby_current_map(lobby)};
  if (ptr_current == nullptr) return false;
  std::size_t sz_index{0};
  for (std::size_t sz_i{0}; sz_i < lobby.vec_entries.size(); ++sz_i)
    if (lobby.vec_entries[sz_i].str_uid == ptr_current->str_uid) sz_index = sz_i;
  const std::size_t sz_count{lobby.vec_entries.size()};
  const std::size_t sz_next{
      (sz_index + sz_count + static_cast<std::size_t>(int4_dir % static_cast<int>(sz_count)))
      % sz_count};
  return lobby.server.select_map(lobby.vec_entries[sz_next].str_name);
}

bool lobby_cycle_faction(LobbyRuntime& lobby, int int4_dir) {
  const std::vector<std::string> vec_factions{net::ra_selectable_factions()};
  std::string str_current;
  for (const net::LobbyClient& client : lobby.server.lobby_info.vec_clients)
    if (client.b_is_admin) str_current = client.str_faction;
  std::size_t sz_index{0};
  for (std::size_t sz_i{0}; sz_i < vec_factions.size(); ++sz_i)
    if (vec_factions[sz_i] == str_current) sz_index = sz_i;
  const std::size_t sz_count{vec_factions.size()};
  const std::size_t sz_next{
      (sz_index + sz_count + static_cast<std::size_t>(int4_dir % static_cast<int>(sz_count)))
      % sz_count};
  return lobby.server.interpret_command(
      0, std::format("faction 0 {}", vec_factions[sz_next]));
}

bool lobby_cycle_team(LobbyRuntime& lobby, int int4_dir) {
  std::int32_t int4_current{0};
  for (const net::LobbyClient& client : lobby.server.lobby_info.vec_clients)
    if (client.b_is_admin) int4_current = client.int4_team;
  const std::int32_t int4_next{((int4_current + int4_dir) % 5 + 5) % 5};
  return lobby.server.interpret_command(0, std::format("team 0 {}", int4_next));
}

std::string lobby_faction_display(const LobbyRuntime& lobby) {
  for (const net::LobbyClient& client : lobby.server.lobby_info.vec_clients)
    if (client.b_is_admin) return client.str_faction;
  return "Random";
}

std::string lobby_team_display(const LobbyRuntime& lobby) {
  for (const net::LobbyClient& client : lobby.server.lobby_info.vec_clients)
    if (client.b_is_admin)
      return client.int4_team == 0 ? "-" : std::to_string(client.int4_team);
  return "-";
}

bool lobby_request_start(LobbyRuntime& lobby) {
  return lobby.server.interpret_command(0, "startgame");
}

LobbyStartConfig lobby_start_config(const LobbyRuntime& lobby) {
  LobbyStartConfig config;
  if (const LobbyMapEntry* ptr_map{lobby_current_map(lobby)}; ptr_map != nullptr)
    config.str_map_path = ptr_map->str_path;
  config.int4_seed = lobby.server.lobby_info.global.int4_random_seed;
  for (const net::LobbyClient& client : lobby.server.lobby_info.vec_clients) {
    if (!client.b_has_slot) continue;
    std::string str_faction{client.str_faction};
    if (str_faction != "allies" && str_faction != "soviet") str_faction = "soviet";
    config.vec_factions.emplace_back(client.str_slot, std::move(str_faction));
  }
  return config;
}

}
