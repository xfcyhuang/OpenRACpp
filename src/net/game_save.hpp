#pragma once
import std;

#include "core/color.hpp"
#include "map/map_cache.hpp"
#include "net/order.hpp"
#include "net/session.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::net {

bool LoadGameSaveMapGenerationArgs(const yaml::MiniYaml& yaml_v,
                                   map::MapGenerationArgs& out);

yaml::MiniYamlNode SerializeGameSaveMapGenerationArgs(
    yaml::StringPool& pool, const map::MapGenerationArgs& args);

struct GameSaveSlotClient {
  core::Color color{};
  std::string str_faction;
  int int4_spawn_point = 0;
  int int4_team = 0;
  int int4_handicap = 0;
  std::string str_slot;
  std::string str_bot;
  bool b_is_admin = false;
  std::string str_bot_name;

  GameSaveSlotClient() = default;
  explicit GameSaveSlotClient(const SessionClient& client);

  void ApplyTo(SessionClient& client) const;

  static bool Deserialize(const yaml::MiniYaml& yaml_v, GameSaveSlotClient& out);

  yaml::MiniYamlNode Serialize(yaml::StringPool& pool,
                               const std::string& str_key) const;
};

class GameSave {
 public:
  static constexpr int kEofMarker = -2;
  static constexpr int kMetadataMarker = -1;
  static constexpr int kTraitDataMarker = -3;

  int LastOrdersFrame = -1;
  int LastSyncFrame = 0;
  std::vector<std::uint8_t> vec_last_sync_packet;

  std::optional<SessionGlobalSettings> opt_global_settings;
  std::vector<std::pair<std::string, SessionSlot>> vec_slots;
  std::vector<std::pair<std::string, GameSaveSlotClient>> vec_slot_clients;
  std::vector<std::pair<int, yaml::MiniYaml>> vec_trait_data;
  std::optional<map::MapGenerationArgs> opt_map_generation_args;

  std::vector<int> vec_clients_by_slot_index;
  int int4_first_bot_slot_index = -1;

  std::vector<std::uint8_t> vec_orders_stream;

  static std::optional<GameSave> Load(const std::string& str_filepath);

  void StartGame(Session& lobby_info, const map::MapPreview& map_preview);

  void DispatchOrders(int int4_player_index, int int4_frame,
                      const std::vector<std::uint8_t>& vec_data);

  void ParseOrders(
      Session& lobby_info,
      const std::function<void(int, int, const std::vector<std::uint8_t>&)>&
          fn_packet);

  void AddTraitData(int int4_trait_index, yaml::MiniYaml yaml_data);

  bool Save(const std::string& str_path) const;
};

}
