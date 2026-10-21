// UPSTREAM: OpenRA.Game/Network/GameSave.cs @e180e3225 L21-333 全文
//          (SlotClient 值面 + GameSave 的 orasav 读/写双态)。
//          The whole of GameSave.cs L21-333 (the SlotClient value face +
//          GameSave's orasav read/write states).
//
// 机制对照 / Mechanism mapping:
//  - FieldLoader.Load<SlotClient>/FieldSaver.Save → 按名字段手写表(色 =
//    Session 同一 hex TryParse/ToString 域)
//    FieldLoader.Load<SlotClient>/FieldSaver.Save → the hand-written by-name
//    table (the color = the same hex TryParse/ToString domain as Session).
//  - ctor(path) 的 InvalidDataException → std::optional 返回(严禁异常协议;
//    上游调用方 browser 以 try/catch 吞,等价面 = nullopt)
//    The ctor(path)'s InvalidDataException → the std::optional return (the
//    no-throw protocol; upstream's caller swallows via try/catch — the
//    equivalent face is nullopt).
//  - DispatchOrders(Connection,...) 的 conn.PlayerIndex → int 形参
//    (net 层不依赖 server 的连接类;消费面 = 服务器存档运行时)
//    DispatchOrders' conn.PlayerIndex → the int parameter (the net layer
//    must not depend on the server's connection class; the consumer is the
//    server's save runtime).
//  - Dictionary<int, MiniYaml> TraitData → 插入序 vector(上游字典迭代 =
//    插入序;ParseOrders 的包序保持)
//    Dictionary<int, MiniYaml> TraitData → the insertion-order vector
//    (upstream's dictionary iteration = insertion order; ParseOrders keeps
//    the packet order).
#pragma once
import std;

#include "core/color.hpp"
#include "map/map_cache.hpp"
#include "net/order.hpp"
#include "net/session.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::net {

/// MapGenerationArgs 的按名装载(unit_orders 的 GenerateMap 分支同构;
/// FieldLoader.Load 的手写面)
/// The by-name MapGenerationArgs load (the same shape as unit_orders'
/// GenerateMap branch; the hand-written FieldLoader.Load face).
bool LoadGameSaveMapGenerationArgs(const yaml::MiniYaml& yaml_v,
                                   map::MapGenerationArgs& out);

/// MapGenerationArgs 的 FieldSaver.Save 面(WriteToString 域)
/// MapGenerationArgs' FieldSaver.Save face (the WriteToString domain).
yaml::MiniYamlNode SerializeGameSaveMapGenerationArgs(
    yaml::StringPool& pool, const map::MapGenerationArgs& args);

/// SlotClient(GameSave.cs L22-76)
/// SlotClient (GameSave.cs L22-76).
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

  /// Deserialize(L67-70):FieldLoader.Load 的按名装载;失败 = false
  /// Deserialize (L67-70): the by-name FieldLoader.Load; false on failure.
  static bool Deserialize(const yaml::MiniYaml& yaml_v, GameSaveSlotClient& out);

  /// Serialize(L72-75):SlotClient@{key} 包装节
  /// Serialize (L72-75): the SlotClient@{key} wrapper node.
  yaml::MiniYamlNode Serialize(yaml::StringPool& pool,
                               const std::string& str_key) const;
};

/// GameSave(GameSave.cs L78-332)
/// GameSave (GameSave.cs L78-332).
class GameSave {
 public:
  static constexpr int kEofMarker = -2;      // L80
  static constexpr int kMetadataMarker = -1;  // L81
  static constexpr int kTraitDataMarker = -3;  // L82

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

  /// GameSave(filepath)(L108-159);无效 orasav → nullopt
  /// GameSave(filepath) (L108-159); an invalid orasav yields nullopt.
  static std::optional<GameSave> Load(const std::string& str_filepath);

  /// StartGame(L161-197)
  void StartGame(Session& lobby_info, const map::MapPreview& map_preview);

  /// DispatchOrders(L199-242);conn.PlayerIndex → int 形参
  /// DispatchOrders (L199-242); conn.PlayerIndex → the int parameter.
  void DispatchOrders(int int4_player_index, int int4_frame,
                      const std::vector<std::uint8_t>& vec_data);

  /// ParseOrders(L244-272)
  void ParseOrders(
      Session& lobby_info,
      const std::function<void(int, int, const std::vector<std::uint8_t>&)>&
          fn_packet);

  /// AddTraitData(L274-277)
  void AddTraitData(int int4_trait_index, yaml::MiniYaml yaml_data);

  /// Save(L279-331);失败 = false
  /// Save (L279-331); false on failure.
  bool Save(const std::string& str_path) const;
};

}  // namespace ora::net
