// UPSTREAM: OpenRA.Game/Network/GameSave.cs @e180e3225 L21-333(见 game_save.hpp)。
#include "net/game_save.hpp"

#include <cstdio>

#include "net/byte_io.hpp"

namespace ora::net {
namespace {

std::optional<std::int32_t> ParseInt32(std::string_view sv) {
  if (sv.empty())
    return std::nullopt;
  std::int32_t v{};
  const auto* first = sv.data();
  const auto* last = sv.data() + sv.size();
  const auto [ptr, ec] = std::from_chars(first, last, v);
  if (ec != std::errc{} || ptr != last)
    return std::nullopt;
  return v;
}

std::string_view NodeKey(const yaml::MiniYamlNode& node) {
  return node.Key != nullptr ? std::string_view{*node.Key}
                             : std::string_view{};
}

std::string_view NodeText(const yaml::MiniYamlNode& node) {
  return node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                     : std::string_view{};
}

/// ReadLengthPrefixedString(UTF-8)的等价面(ByteReader.ReadString =
/// 7-bit 前缀;上游 Connection.MaxOrderLength 上限 = 读侧不设)
/// The ReadLengthPrefixedString(UTF-8) equivalent (ByteReader.ReadString's
/// 7-bit prefix; upstream's Connection.MaxOrderLength cap is writer-side).
std::optional<std::string> ReadLengthPrefixedString(net::ByteReader& r) {
  if (r.Remaining() == 0)
    return std::nullopt;
  return r.ReadString();
}

std::vector<std::uint8_t> ReadFileBytes(const std::string& str_path) {
  std::FILE* file = std::fopen(str_path.c_str(), "rb");
  if (file == nullptr)
    return {};
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  std::vector<std::uint8_t> bytes;
  if (size > 0) {
    bytes.resize(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    bytes.resize(read);
  }
  std::fclose(file);
  return bytes;
}

}  // namespace

bool LoadGameSaveMapGenerationArgs(const yaml::MiniYaml& yaml_v,
                                   map::MapGenerationArgs& out) {
  for (const auto& node : yaml_v.Nodes) {
    const auto key = NodeKey(node);
    const auto text = NodeText(node);
    if (key == "Uid") {
      out.str_uid = std::string{text};
    } else if (key == "Generator") {
      out.str_generator = std::string{text};
    } else if (key == "Tileset") {
      out.str_tileset = std::string{text};
    } else if (key == "Size") {
      const auto comma = text.find(',');
      if (comma == std::string_view::npos)
        return false;
      const auto width = ParseInt32(text.substr(0, comma));
      const auto height = ParseInt32(text.substr(comma + 1));
      if (!width.has_value() || !height.has_value())
        return false;
      out.size_ = Size{*width, *height};
    } else if (key == "Title") {
      out.str_title = std::string{text};
    } else if (key == "Author") {
      out.str_author = std::string{text};
    } else if (key == "PreviewVisibility") {
      if (text == "None")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::None;
      else if (text == "Lobby")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::Lobby;
      else if (text == "MapChooser")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::MapChooser;
      else if (text == "All")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::All;
      else
        return false;
    } else if (key == "Options") {
      for (const auto& option_node : node.Value.Nodes)
        out.vec_options.emplace_back(std::string{NodeKey(option_node)},
                                     std::string{NodeText(option_node)});
    }
  }
  return true;
}

yaml::MiniYamlNode SerializeGameSaveMapGenerationArgs(
    yaml::StringPool& pool, const map::MapGenerationArgs& args) {
  const auto intern = [&pool](std::string_view sv) {
    return pool.Intern(sv);
  };
  std::vector<yaml::MiniYamlNode> nodes;
  nodes.emplace_back(intern("Uid"), yaml::MiniYaml{intern(args.str_uid)});
  nodes.emplace_back(intern("Generator"),
                     yaml::MiniYaml{intern(args.str_generator)});
  nodes.emplace_back(intern("Tileset"),
                     yaml::MiniYaml{intern(args.str_tileset)});
  const std::string str_size =
      std::to_string(args.size_.Width) + "," + std::to_string(args.size_.Height);
  nodes.emplace_back(intern("Size"), yaml::MiniYaml{intern(str_size)});
  nodes.emplace_back(intern("Title"), yaml::MiniYaml{intern(args.str_title)});
  nodes.emplace_back(intern("Author"), yaml::MiniYaml{intern(args.str_author)});
  const char* str_visibility = "All";
  if (args.kind_preview_visibility ==
      map::MapGenerationArgs::PreviewVisibilityFlags::None)
    str_visibility = "None";
  else if (args.kind_preview_visibility ==
           map::MapGenerationArgs::PreviewVisibilityFlags::Lobby)
    str_visibility = "Lobby";
  else if (args.kind_preview_visibility ==
           map::MapGenerationArgs::PreviewVisibilityFlags::MapChooser)
    str_visibility = "MapChooser";
  nodes.emplace_back(intern("PreviewVisibility"),
                     yaml::MiniYaml{intern(str_visibility)});
  if (!args.vec_options.empty()) {
    std::vector<yaml::MiniYamlNode> option_nodes;
    for (const auto& [key, value] : args.vec_options)
      option_nodes.emplace_back(intern(key), yaml::MiniYaml{intern(value)});
    nodes.emplace_back(intern("Options"),
                       yaml::MiniYaml{nullptr, std::move(option_nodes)});
  }
  return yaml::MiniYamlNode{intern("MapGenerationArgs"),
                            yaml::MiniYaml{nullptr, std::move(nodes)}};
}

GameSaveSlotClient::GameSaveSlotClient(const SessionClient& client) {
  color = client.Color;
  str_faction = client.Faction;
  int4_spawn_point = client.SpawnPoint;
  int4_team = client.Team;
  int4_handicap = client.Handicap;
  str_slot = client.Slot;
  str_bot = client.Bot;
  b_is_admin = client.IsAdmin;
  if (!client.Bot.empty())
    str_bot_name = client.Name;
}

void GameSaveSlotClient::ApplyTo(SessionClient& client) const {
  client.Color = color;
  client.Faction = str_faction;
  client.SpawnPoint = int4_spawn_point;
  client.Team = int4_team;
  client.Handicap = int4_handicap;
  client.Slot = str_slot;
  client.Bot = str_bot;
  client.IsAdmin = b_is_admin;
  if (!str_bot.empty())
    client.Name = str_bot_name;
}

bool GameSaveSlotClient::Deserialize(const yaml::MiniYaml& yaml_v,
                                     GameSaveSlotClient& out) {
  for (const auto& node : yaml_v.Nodes) {
    const auto key = NodeKey(node);
    const auto text = NodeText(node);
    if (key == "Color") {
      if (!core::Color::TryParse(text, out.color))
        return false;
    } else if (key == "Faction") {
      out.str_faction = std::string{text};
    } else if (key == "SpawnPoint") {
      const auto v = ParseInt32(text);
      if (!v.has_value())
        return false;
      out.int4_spawn_point = *v;
    } else if (key == "Team") {
      const auto v = ParseInt32(text);
      if (!v.has_value())
        return false;
      out.int4_team = *v;
    } else if (key == "Handicap") {
      const auto v = ParseInt32(text);
      if (!v.has_value())
        return false;
      out.int4_handicap = *v;
    } else if (key == "Slot") {
      out.str_slot = std::string{text};
    } else if (key == "Bot") {
      out.str_bot = std::string{text};
    } else if (key == "IsAdmin") {
      if (text != "True" && text != "False")
        return false;
      out.b_is_admin = text == "True";
    } else if (key == "BotName") {
      out.str_bot_name = std::string{text};
    }
  }
  return true;
}

yaml::MiniYamlNode GameSaveSlotClient::Serialize(
    yaml::StringPool& pool, const std::string& str_key) const {
  const auto intern = [&pool](std::string_view sv) {
    return pool.Intern(sv);
  };
  std::vector<yaml::MiniYamlNode> nodes;
  nodes.emplace_back(intern("Color"), yaml::MiniYaml{intern(color.ToString())});
  nodes.emplace_back(intern("Faction"), yaml::MiniYaml{intern(str_faction)});
  nodes.emplace_back(intern("SpawnPoint"),
                     yaml::MiniYaml{intern(std::to_string(int4_spawn_point))});
  nodes.emplace_back(intern("Team"),
                     yaml::MiniYaml{intern(std::to_string(int4_team))});
  nodes.emplace_back(intern("Handicap"),
                     yaml::MiniYaml{intern(std::to_string(int4_handicap))});
  nodes.emplace_back(intern("Slot"), yaml::MiniYaml{intern(str_slot)});
  nodes.emplace_back(intern("Bot"), yaml::MiniYaml{intern(str_bot)});
  nodes.emplace_back(intern("IsAdmin"),
                     yaml::MiniYaml{intern(b_is_admin ? "True" : "False")});
  nodes.emplace_back(intern("BotName"), yaml::MiniYaml{intern(str_bot_name)});
  return yaml::MiniYamlNode{intern("SlotClient@" + str_key),
                            yaml::MiniYaml{nullptr, std::move(nodes)}};
}

std::optional<GameSave> GameSave::Load(const std::string& str_filepath) {
  const std::vector<std::uint8_t> vec_file = ReadFileBytes(str_filepath);
  if (vec_file.size() < 12)
    return std::nullopt;

  GameSave save{};
  std::size_t pos = vec_file.size() - 12;
  const auto read_i32_at = [&vec_file](std::size_t at) -> std::int32_t {
    std::int32_t v{};
    std::memcpy(&v, vec_file.data() + at, 4);
    return v;
  };
  const std::int32_t int4_metadata_offset = read_i32_at(pos);
  const std::int32_t int4_trait_data_offset = read_i32_at(pos + 4);
  if (read_i32_at(pos + 8) != kEofMarker)
    return std::nullopt;

  pos = static_cast<std::size_t>(int4_metadata_offset);
  const auto bounds_ok = [&vec_file](std::size_t at,
                                     std::size_t count) -> bool {
    return at <= vec_file.size() && count <= vec_file.size() - at;
  };
  if (!bounds_ok(pos, 4) || read_i32_at(pos) != kMetadataMarker)
    return std::nullopt;

  pos += 4;
  if (!bounds_ok(pos, 8))
    return std::nullopt;
  save.LastOrdersFrame = read_i32_at(pos);
  save.LastSyncFrame = read_i32_at(pos + 4);
  pos += 8;
  if (!bounds_ok(pos, Order::SyncHashOrderLength))
    return std::nullopt;
  save.vec_last_sync_packet.assign(
      vec_file.begin() + static_cast<std::ptrdiff_t>(pos),
      vec_file.begin() + static_cast<std::ptrdiff_t>(pos + Order::SyncHashOrderLength));
  pos += Order::SyncHashOrderLength;

  const std::size_t sz_reader_start = pos;
  net::ByteReader reader{vec_file.data() + pos, vec_file.size() - pos};
  const auto read_yaml_section =
      [&str_filepath](net::ByteReader& r,
                      const char* str_tag) -> std::optional<std::vector<yaml::MiniYamlNode>> {
    const auto str_data = ReadLengthPrefixedString(r);
    if (!str_data.has_value())
      return std::nullopt;
    return yaml::MiniYaml::FromString(
        *str_data, str_filepath + ":" + str_tag);
  };

  const auto nodes_global = read_yaml_section(reader, "globalSettings");
  if (!nodes_global.has_value() || nodes_global->empty())
    return std::nullopt;
  SessionGlobalSettings global{};
  if (!LoadSessionGlobalSettings((*nodes_global)[0].Value, global))
    return std::nullopt;
  save.opt_global_settings = global;

  const auto nodes_slots = read_yaml_section(reader, "slots");
  if (!nodes_slots.has_value())
    return std::nullopt;
  for (const auto& node : *nodes_slots) {
    SessionSlot slot{};
    if (!LoadSessionSlot(node.Value, slot))
      return std::nullopt;
    save.vec_slots.emplace_back(slot.PlayerReference, slot);
  }

  const auto nodes_slot_clients = read_yaml_section(reader, "slotClients");
  if (!nodes_slot_clients.has_value())
    return std::nullopt;
  for (const auto& node : *nodes_slot_clients) {
    GameSaveSlotClient slot_client{};
    if (!GameSaveSlotClient::Deserialize(node.Value, slot_client))
      return std::nullopt;
    save.vec_slot_clients.emplace_back(slot_client.str_slot, slot_client);
  }

  const auto str_map_generation_args = ReadLengthPrefixedString(reader);
  if (!str_map_generation_args.has_value())
    return std::nullopt;
  if (!str_map_generation_args->empty()) {
    const auto nodes_args = yaml::MiniYaml::FromString(
        *str_map_generation_args, str_filepath + ":mapGenerationArgs");
    map::MapGenerationArgs args{};
    if (nodes_args.empty() ||
        !LoadGameSaveMapGenerationArgs(nodes_args[0].Value, args))
      return std::nullopt;
    save.opt_map_generation_args = args;
  }

  pos = sz_reader_start + reader.Position();
  if (pos != static_cast<std::size_t>(int4_trait_data_offset) ||
      !bounds_ok(pos, 4) || read_i32_at(pos) != kTraitDataMarker)
    return std::nullopt;

  net::ByteReader reader_trait{vec_file.data() + pos + 4,
                               vec_file.size() - pos - 4};
  const auto nodes_trait_data = read_yaml_section(reader_trait, "traitData");
  if (!nodes_trait_data.has_value())
    return std::nullopt;
  for (const auto& node : *nodes_trait_data) {
    const auto index = ParseInt32(NodeKey(node));
    if (!index.has_value())
      return std::nullopt;
    save.vec_trait_data.emplace_back(*index, node.Value);
  }

  if (!bounds_ok(0, static_cast<std::size_t>(int4_metadata_offset)))
    return std::nullopt;
  save.vec_orders_stream.assign(
      vec_file.begin(), vec_file.begin() + int4_metadata_offset);
  return save;
}

void GameSave::StartGame(Session& lobby_info,
                         const map::MapPreview& map_preview) {
  if (map_preview.Class() == map::MapClassification::Generated)
    opt_map_generation_args = map_preview.GenerationArgs();

  vec_clients_by_slot_index.clear();
  for (const auto& [str_slot_key, slot] : lobby_info.vec_slots) {
    const SessionClient* ptr_client = lobby_info.ClientInSlot(str_slot_key);
    vec_clients_by_slot_index.push_back(ptr_client != nullptr
                                             ? ptr_client->Index
                                             : -1);
  }

  yaml::StringPool pool{};
  {
    const yaml::MiniYamlNode node_global =
        SerializeSessionGlobalSettings(pool, lobby_info.global_settings);
    SessionGlobalSettings global{};
    if (LoadSessionGlobalSettings(node_global.Value, global))
      opt_global_settings = global;
    else
      opt_global_settings = lobby_info.global_settings;
  }

  vec_slots.clear();
  vec_slot_clients.clear();
  int4_first_bot_slot_index = -1;
  for (const auto& [str_slot_key, slot] : lobby_info.vec_slots) {
    const yaml::MiniYamlNode node_slot =
        SerializeSessionSlot(pool, slot);
    SessionSlot slot_round_trip{};
    if (LoadSessionSlot(node_slot.Value, slot_round_trip))
      vec_slots.emplace_back(str_slot_key, slot_round_trip);
    else
      vec_slots.emplace_back(str_slot_key, slot);

    const map::PlayerReference* ptr_reference = nullptr;
    for (const auto& [str_name, reference] : map_preview.Players().Players)
        if (str_name == slot.PlayerReference) {
          ptr_reference = &reference;
          break;
        }

    const SessionClient* ptr_client = lobby_info.ClientInSlot(str_slot_key);
    if (ptr_reference != nullptr && ptr_reference->Playable &&
        ptr_client != nullptr) {
      vec_slot_clients.emplace_back(str_slot_key,
                                    GameSaveSlotClient{*ptr_client});
      if (!ptr_client->Bot.empty() && int4_first_bot_slot_index < 0) {
        for (std::size_t i = 0; i < vec_clients_by_slot_index.size(); ++i)
          if (vec_clients_by_slot_index[i] == ptr_client->Index) {
            int4_first_bot_slot_index = static_cast<int>(i);
            break;
          }
      }
    }
  }
}

void GameSave::DispatchOrders(int int4_player_index, int int4_frame,
                              const std::vector<std::uint8_t>& vec_data) {
  if (!vec_data.empty() &&
      vec_data[0] == static_cast<std::uint8_t>(OrderType::SyncHash) &&
      int4_frame > LastSyncFrame) {
    if (vec_data.size() != static_cast<std::size_t>(Order::SyncHashOrderLength))
      return;

    LastSyncFrame = int4_frame;
    vec_last_sync_packet = vec_data;
  }

  if (int4_frame <= LastOrdersFrame)
    return;

  if (!vec_data.empty() && vec_data[0] == 0xFE)
    return;

  int int4_client_slot = -1;
  for (std::size_t i = 0; i < vec_clients_by_slot_index.size(); ++i)
    if (vec_clients_by_slot_index[i] == int4_player_index) {
      int4_client_slot = static_cast<int>(i);
      break;
    }

  if (int4_client_slot == -1)
    int4_client_slot = int4_first_bot_slot_index;

  net::ByteWriter writer{};
  writer.Write(static_cast<std::int32_t>(vec_data.size() + 8));
  writer.Write(static_cast<std::int32_t>(int4_frame));
  writer.Write(static_cast<std::int32_t>(int4_client_slot));
  writer.WriteBytes(vec_data);
  vec_orders_stream.insert(vec_orders_stream.end(), writer.Bytes().begin(),
                           writer.Bytes().end());
  LastOrdersFrame = int4_frame;
}

void GameSave::ParseOrders(
    Session& lobby_info,
    const std::function<void(int, int, const std::vector<std::uint8_t>&)>&
        fn_packet) {
  for (const auto& [int4_index, yaml_data] : vec_trait_data) {
    yaml::StringPool pool{};
    std::vector<yaml::MiniYamlNode> nodes;
    nodes.emplace_back(pool.Intern(std::to_string(int4_index)), yaml_data);
    const Order order_trait = Order::FromTargetString(
        "SaveTraitData", yaml::WriteToString(nodes), true);
    fn_packet(0, 0, order_trait.Serialize());
  }

  std::size_t pos = 0;
  while (pos + 12 <= vec_orders_stream.size()) {
    std::int32_t int4_data_length{};
    std::int32_t int4_frame{};
    std::int32_t int4_slot{};
    std::memcpy(&int4_data_length, vec_orders_stream.data() + pos, 4);
    std::memcpy(&int4_frame, vec_orders_stream.data() + pos + 4, 4);
    std::memcpy(&int4_slot, vec_orders_stream.data() + pos + 8, 4);
    pos += 12;
    const std::size_t sz_data =
        static_cast<std::size_t>(int4_data_length - 8);
    if (pos + sz_data > vec_orders_stream.size())
      break;
    std::vector<std::uint8_t> vec_data(
        vec_orders_stream.begin() + static_cast<std::ptrdiff_t>(pos),
        vec_orders_stream.begin() + static_cast<std::ptrdiff_t>(pos + sz_data));
    pos += sz_data;

    int int4_client_index = int4_slot >= 0 &&
                                    static_cast<std::size_t>(int4_slot) <
                                        vec_clients_by_slot_index.size()
                                ? vec_clients_by_slot_index[static_cast<std::size_t>(int4_slot)]
                                : -1;
    const SessionClient* ptr_client =
        lobby_info.ClientWithIndex(int4_client_index);
    if (ptr_client != nullptr && !ptr_client->Bot.empty())
      int4_client_index = ptr_client->BotControllerClientIndex;

    fn_packet(int4_frame, int4_client_index, vec_data);
  }

  fn_packet(LastSyncFrame, 0, vec_last_sync_packet);
}

void GameSave::AddTraitData(int int4_trait_index, yaml::MiniYaml yaml_data) {
  for (auto& [index, data] : vec_trait_data)
    if (index == int4_trait_index) {
      data = std::move(yaml_data);
      return;
    }
  vec_trait_data.emplace_back(int4_trait_index, std::move(yaml_data));
}

bool GameSave::Save(const std::string& str_path) const {
  if (!opt_global_settings.has_value())
    return false;

  std::FILE* file = std::fopen(str_path.c_str(), "wb");
  if (file == nullptr)
    return false;

  yaml::StringPool pool{};
  net::ByteWriter writer{};
  writer.WriteBytes(vec_orders_stream);
  writer.Write(static_cast<std::int32_t>(kMetadataMarker));
  writer.Write(static_cast<std::int32_t>(LastOrdersFrame));
  writer.Write(static_cast<std::int32_t>(LastSyncFrame));
  std::vector<std::uint8_t> vec_sync_packet = vec_last_sync_packet;
  vec_sync_packet.resize(Order::SyncHashOrderLength, 0);
  writer.WriteBytes(vec_sync_packet);

  {
    std::vector<yaml::MiniYamlNode> nodes_global;
    nodes_global.push_back(
        SerializeSessionGlobalSettings(pool, *opt_global_settings));
    writer.Write(yaml::WriteToString(nodes_global));
  }

  {
    std::vector<yaml::MiniYamlNode> nodes_slots;
    for (const auto& [str_key, slot] : vec_slots)
      nodes_slots.push_back(SerializeSessionSlot(pool, slot));
    writer.Write(yaml::WriteToString(nodes_slots));
  }

  {
    std::vector<yaml::MiniYamlNode> nodes_slot_clients;
    for (const auto& [str_key, slot_client] : vec_slot_clients)
      nodes_slot_clients.push_back(slot_client.Serialize(pool, str_key));
    writer.Write(yaml::WriteToString(nodes_slot_clients));
  }

  if (opt_map_generation_args.has_value()) {
    std::vector<yaml::MiniYamlNode> nodes_args;
    nodes_args.push_back(
        SerializeGameSaveMapGenerationArgs(pool, *opt_map_generation_args));
    writer.Write(yaml::WriteToString(nodes_args));
  } else {
    writer.Write(std::string_view{});
  }

  const std::size_t sz_trait_data_offset = writer.Bytes().size();
  writer.Write(static_cast<std::int32_t>(kTraitDataMarker));
  {
    std::vector<yaml::MiniYamlNode> nodes_trait;
    for (const auto& [int4_index, yaml_data] : vec_trait_data)
      nodes_trait.emplace_back(pool.Intern(std::to_string(int4_index)),
                               yaml_data);
    writer.Write(yaml::WriteToString(nodes_trait));
  }

  writer.Write(static_cast<std::int32_t>(vec_orders_stream.size()));
  writer.Write(static_cast<std::int32_t>(sz_trait_data_offset));
  writer.Write(static_cast<std::int32_t>(kEofMarker));

  const bool b_ok =
      std::fwrite(writer.Bytes().data(), 1, writer.Bytes().size(), file) ==
      writer.Bytes().size();
  std::fclose(file);
  return b_ok;
}

}  // namespace ora::net
