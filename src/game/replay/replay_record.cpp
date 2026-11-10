module ora.game.world;

import std;
import ora.game.miniyaml;

namespace ora::game::replay {

namespace {

MiniYamlNode make_node(std::string_view str_key, std::string str_value = "") {
  MiniYamlNode node;
  node.str_key = std::string{str_key};
  node.str_value = std::move(str_value);
  return node;
}

void set_child_value(MiniYamlNode& node_parent, std::string_view str_key, std::string str_value) {
  for (MiniYamlNode& child : node_parent.vec_children)
    if (child.str_key == str_key) {
      child.str_value = std::move(str_value);
      return;
    }
  node_parent.vec_children.push_back(make_node(str_key, std::move(str_value)));
}

[[nodiscard]] std::string format_utc(const std::chrono::system_clock::time_point& tp_now) {
  const std::chrono::sys_days tp_day{std::chrono::floor<std::chrono::days>(tp_now)};
  const std::chrono::year_month_day ymd{tp_day};
  const auto dur_hms{std::chrono::floor<std::chrono::seconds>(tp_now) - tp_day};
  const std::chrono::hh_mm_ss<std::chrono::seconds> hms{dur_hms};
  return std::format("{:04d}-{:02d}-{:02d} {:02d}-{:02d}-{:02d}",
                     static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                     static_cast<unsigned>(ymd.day()), hms.hours().count(),
                     hms.minutes().count(), hms.seconds().count());
}

[[nodiscard]] bool packet_contains_start_game(std::span<const std::uint8_t> span_data) {
  if (span_data.size() < 4) return false;
  if (span_data[0] != 0 || span_data[1] != 0 || span_data[2] != 0 || span_data[3] != 0)
    return false;

  network::OrderReader reader{{span_data.begin() + 4, span_data.end()}};
  while (reader.idx_pos < reader.span_bytes.size()) {
    const std::optional<network::OrderModel> opt_order{
        network::deserialize_order(reader, nullptr)};
    if (!opt_order.has_value()) break;
    if (opt_order->str_order == "StartGame") return true;
  }
  return false;
}

[[nodiscard]] std::int32_t yaml_int(const MiniYamlNode& node, std::string_view str_key,
                                    std::int32_t int4_fallback) {
  for (const MiniYamlNode& child : node.vec_children)
    if (child.str_key == str_key && !child.str_value.empty()) {
      std::int32_t int4_out{};
      const auto res_chars{
          std::from_chars(child.str_value.data(), child.str_value.data() + child.str_value.size(),
                          int4_out)};
      if (res_chars.ec == std::errc{} &&
          res_chars.ptr == child.str_value.data() + child.str_value.size())
        return int4_out;
    }
  return int4_fallback;
}

}

std::string replay_session_yaml(const ReplaySessionInfo& info) {
  std::string str_out;
  str_out += "DisabledSpawnPoints:\n";
  for (const std::int32_t int4_index : info.vec_client_indices) {
    str_out += std::format("Client@{}:\n", int4_index);
    str_out += std::format("\tIndex: {}\n", int4_index);
    str_out += "\tState: Ready\n";
    str_out += "\tTeam: 0\n";
  }
  str_out += "Slot@Multi0:\n\tPlayerReference: Multi0\n\tAllowBots: True\n";
  str_out += "GlobalSettings:\n";
  str_out += "\tServerName:\n";
  str_out += std::format("\tMap: {}\n", info.str_map_uid);
  str_out += std::format("\tRandomSeed: {}\n", info.int4_seed);
  str_out += std::format("\tNetFrameInterval: {}\n", info.int4_net_frame_interval);
  str_out += std::format("\tGameTimestep: {}\n", info.int4_game_timestep);
  return str_out;
}

std::expected<ReplaySessionInfo, std::string> replay_session_parse(std::string_view str_yaml) {
  auto res_nodes{miniyaml_from_string(str_yaml, "orarep-syncinfo")};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());

  ReplaySessionInfo info;
  for (const MiniYamlNode& node : *res_nodes) {
    if (node.str_key.starts_with("Client@")) {
      const std::int32_t int4_index{yaml_int(node, "Index", -1)};
      if (int4_index >= 0) info.vec_client_indices.push_back(int4_index);
    } else if (node.str_key == "GlobalSettings") {
      info.int4_seed = yaml_int(node, "RandomSeed", 0);
      info.int4_net_frame_interval = yaml_int(node, "NetFrameInterval", 3);
      info.int4_game_timestep = yaml_int(node, "GameTimestep", 40);
      for (const MiniYamlNode& child : node.vec_children)
        if (child.str_key == "Map") info.str_map_uid = child.str_value;
    }
  }
  return info;
}

std::vector<MiniYamlNode> replay_metadata_nodes(const ReplayGameInfo& info) {
  MiniYamlNode root{make_node("Root")};
  root.vec_children = {
      make_node("Mod", info.str_mod), make_node("Version", info.str_version),
      make_node("MapUid", info.str_map_uid), make_node("MapTitle", info.str_map_title),
      make_node("FinalGameTick", std::format("{}", info.int4_final_game_tick)),
      make_node("StartTimeUtc", info.str_start_time_utc), make_node("EndTimeUtc", ""),
      make_node("DisabledSpawnPoints", "")};

  std::vector<MiniYamlNode> vec_nodes;
  vec_nodes.push_back(std::move(root));
  for (std::size_t idx{}; idx < info.vec_players.size(); ++idx) {
    const ReplayGameInfo::PlayerInfo& player{info.vec_players[idx]};
    MiniYamlNode node{make_node(std::format("Player@{}", idx))};
    node.vec_children = {
        make_node("ClientIndex", std::format("{}", player.int4_client_index)),
        make_node("Name", player.str_name),
        make_node("IsHuman", player.b_is_human ? "True" : "False"),
        make_node("IsBot", "False"), make_node("Team", "0"),
        make_node("Outcome", "Undefined"), make_node("DisconnectFrame", "0")};
    vec_nodes.push_back(std::move(node));
  }
  return vec_nodes;
}

void recorder_receive(ReplayRecorderModel& rec, std::int32_t int4_client,
                      std::vector<std::uint8_t> vec_data) {
  if (!rec.b_game_start_seen && packet_contains_start_game(vec_data))
    rec.b_game_start_seen = true;

  ReplayRecord record;
  record.int4_client = int4_client;
  record.vec_data = std::move(vec_data);
  rec.vec_records.push_back(std::move(record));
}

void recorder_attach_metadata(ReplayRecorderModel& rec, std::vector<MiniYamlNode> vec_nodes) {
  rec.vec_meta_nodes = std::move(vec_nodes);
  rec.b_metadata_attached = true;
}

std::expected<std::vector<std::uint8_t>, std::string> recorder_dispose(
    ReplayRecorderModel& rec) {
  if (!rec.b_game_start_seen)
    return std::unexpected(
        "recorder: game never started (no frame-0 StartGame packet; pre-start buffer discarded)");
  if (!rec.b_metadata_attached)
    return std::unexpected("recorder: no metadata attached (server-side desync discard form)");

  const std::string str_end{format_utc(std::chrono::system_clock::now())};
  for (MiniYamlNode& node : rec.vec_meta_nodes)
    if (node.str_key == "Root") set_child_value(node, "EndTimeUtc", str_end);

  ReplayFile file;
  file.vec_records = std::move(rec.vec_records);
  file.b_has_metadata = true;
  file.vec_meta_nodes = std::move(rec.vec_meta_nodes);
  return replay_write(file);
}

std::vector<std::uint8_t> make_boot_packet(std::string_view str_order,
                                           std::string_view str_target) {
  network::OrderModel order;
  order.str_order = std::string{str_order};
  order.opt_str_target = std::string{str_target};
  order.b_is_immediate = true;
  return network::order_packet_serialize_frame(network::order_packet_from_orders({order}), 0);
}

}
