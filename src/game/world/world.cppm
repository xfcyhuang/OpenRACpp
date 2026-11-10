export module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.sound;
import ora.game.support;

export {
namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

enum class TargetKind : std::uint8_t {
  kInvalid = 0,
  kActor = 1,
  kTerrain = 2,
  kFrozenActor = 3,
};

struct TargetModel {
  TargetKind kind{};
  std::uint32_t u4_actor{};
  std::int32_t int4_generation{};
  std::uint32_t u4_viewer_actor{};
  std::uint32_t u4_frozen_id{};
  bool b_cell{};
  std::int32_t int4_cell_bits{};
  std::uint8_t u1_subcell{};
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
  std::int32_t int4_pos_z{};
  std::vector<std::array<std::int32_t, 3>> vec_terrain_positions{};
};

struct OrderModel {
  std::string str_order{};
  bool b_handshake{};
  bool b_has_subject{};
  std::uint32_t u4_subject{};
  TargetModel target{};
  std::optional<std::string> opt_str_target{};
  bool b_queued{};
  bool b_has_extra_actors{};
  std::vector<std::uint32_t> vec_extra_actors{};
  std::int32_t int4_extra_location_bits{};
  std::uint32_t u4_extra_data{};
  bool b_has_grouped{};
  std::vector<std::uint32_t> vec_grouped{};
  bool b_is_immediate{};
};

struct OrderReader {
  std::span<const std::uint8_t> span_bytes;
  std::size_t idx_pos{};
};

[[nodiscard]] std::vector<std::uint8_t> serialize_order(const OrderModel& order);

[[nodiscard]] std::optional<OrderModel> deserialize_order(OrderReader& reader,
                                                           const world::WorldModel* ptr_world);

}

namespace ora::game::replay {
struct ReplayRecorderModel;
struct ReplayFeedModel;
}

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

constexpr std::int32_t kLocalClientId{1};
constexpr std::int32_t kNetFrameInterval{1};
constexpr std::size_t kChatMessageMaxLength{2500};

struct ClientPendingQueue {
  std::int32_t int4_client{};
  std::deque<std::pair<std::int32_t, std::vector<std::uint8_t>>> deque_pending;
};

struct OrderManagerModel {
  std::vector<OrderModel> vec_local;
  std::vector<OrderModel> vec_local_immediate;
  std::deque<std::pair<std::int32_t, std::vector<std::uint8_t>>> deque_echo_orders;
  std::deque<std::vector<std::uint8_t>> deque_echo_immediate;
  std::vector<std::vector<std::uint8_t>> vec_echo_sync;
  std::vector<ClientPendingQueue> vec_pending;
  std::vector<std::pair<std::int32_t, std::int32_t>> vec_sync_received;
  std::vector<std::string> vec_system_log;
  std::int32_t int4_local_frame{};
  std::int32_t int4_sent_orders_frame{};

  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_sync_reference;
  bool b_game_started{};
  bool b_out_of_sync{};
  std::string str_fatal;

  replay::ReplayRecorderModel* ptr_recorder{};

  replay::ReplayFeedModel* ptr_replay_feed{};
};

void order_manager_start_game(OrderManagerModel& om, world::WorldModel& w,
                              std::span<const std::int32_t> vec_client_ids);

void issue_order(OrderManagerModel& om, const OrderModel& order);

void tick_immediate(OrderManagerModel& om, world::WorldModel& w);

bool try_tick(OrderManagerModel& om, world::WorldModel& w);

bool run_frame(OrderManagerModel& om, world::WorldModel& w);

[[nodiscard]] bool receive_sync_for_pump(OrderManagerModel& om, std::int32_t int4_frame,
                                         std::int32_t int4_sync_hash,
                                         std::uint64_t u8_defeat_state);

void receive_orders_for_pump(OrderManagerModel& om, std::int32_t int4_client,
                             std::int32_t int4_frame, std::vector<std::uint8_t>&& vec_bytes);

}

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

[[nodiscard]] std::vector<std::uint8_t> order_packet_from_orders(
    const std::vector<OrderModel>& vec_orders);

[[nodiscard]] std::vector<OrderModel> order_packet_get_orders(
    std::span<const std::uint8_t> span_data, const world::WorldModel* ptr_world);

[[nodiscard]] std::vector<std::uint8_t> order_packet_serialize_frame(
    std::span<const std::uint8_t> span_data, std::int32_t int4_frame);

[[nodiscard]] std::vector<std::uint8_t> serialize_sync_packet(std::int32_t int4_frame,
                                                              std::int32_t int4_sync_hash,
                                                              std::uint64_t u8_defeat_state);

struct ParsedOrderPacket {
  std::int32_t int4_frame{};
  std::vector<std::uint8_t> vec_order_bytes;
};

[[nodiscard]] std::optional<ParsedOrderPacket> try_parse_order_packet(
    std::span<const std::uint8_t> span_packet);

struct ParsedSyncPacket {
  std::int32_t int4_frame{};
  std::int32_t int4_sync_hash{};
  std::uint64_t u8_defeat_state{};
};

[[nodiscard]] std::optional<ParsedSyncPacket> try_parse_sync_packet(
    std::span<const std::uint8_t> span_packet);

}

namespace ora::game::network {

enum class LobbyClientState : std::int32_t {
  kNotReady = 0,
  kInvalid = 1,
  kReady = 2,
  kDisconnected = 1000,
};

[[nodiscard]] std::string_view lobby_client_state_name(LobbyClientState state);

struct LobbyClient {
  std::int32_t int4_index{};
  std::string str_preferred_color;
  std::string str_color;
  std::string str_faction;
  std::int32_t int4_spawn_point{};
  std::string str_name;
  bool b_has_anonymized_ip{};
  std::string str_anonymized_ip_address;
  bool b_has_location{};
  std::string str_location;
  std::string str_connection_quality{"Good"};
  LobbyClientState state{LobbyClientState::kInvalid};
  std::int32_t int4_team{};
  std::int32_t int4_handicap{};
  bool b_has_slot{};
  std::string str_slot;
  bool b_is_bot{};
  std::string str_bot;
  std::int32_t int4_bot_controller_client_index{};
  bool b_is_admin{};
  bool b_has_fingerprint{};
  std::string str_fingerprint;

  [[nodiscard]] bool is_ready() const { return state == LobbyClientState::kReady; }
  [[nodiscard]] bool is_observer() const { return !b_has_slot; }
};

struct LobbySlot {
  std::string str_player_reference;
  bool b_closed{};
  bool b_allow_bots{};
  bool b_lock_faction{};
  bool b_lock_color{};
  bool b_lock_team{};
  bool b_lock_handicap{};
  bool b_lock_spawn{};
  bool b_required{};
};

struct LobbyOptionState {
  std::string str_value;
  std::string str_preferred_value;
  bool b_is_locked{};
};

struct LobbyGlobal {
  std::string str_server_name;
  std::string str_map;
  std::string str_map_status;
  std::int32_t int4_random_seed{};
  bool b_allow_spectators{true};
  std::string str_game_uid;
  bool b_enable_singleplayer{};
  bool b_enable_map_generation{};
  bool b_enable_game_saves{};
  bool b_enable_sync_reports{};
  bool b_dedicated{};
  std::int32_t int4_net_frame_interval{3};
  std::int32_t int4_game_timestep{};
  std::vector<std::pair<std::string, LobbyOptionState>> vec_options;
};

struct LobbySession {
  std::vector<LobbyClient> vec_clients;
  std::vector<std::pair<std::string, LobbySlot>> vec_slots;
  std::vector<std::int32_t> vec_disabled_spawn_points;
  LobbyGlobal global;

  [[nodiscard]] const LobbyClient* client_with_index(std::int32_t int4_index) const;
  [[nodiscard]] LobbyClient* client_with_index(std::int32_t int4_index);
  [[nodiscard]] const LobbyClient* client_in_slot(std::string_view str_slot) const;
  [[nodiscard]] LobbyClient* client_in_slot(std::string_view str_slot);
  [[nodiscard]] const LobbySlot* slot(std::string_view str_player_reference) const;
  [[nodiscard]] const std::string* first_empty_slot() const;
  [[nodiscard]] std::string serialize() const;
};

struct LobbyPlayerRef {
  std::string str_name;
  bool b_playable{};
  bool b_allow_bots{true};
  bool b_lock_faction{};
  bool b_lock_color{};
  bool b_lock_team{};
  bool b_lock_handicap{};
  bool b_lock_spawn{};
  bool b_required{};
  std::string str_faction;
  std::int32_t int4_team{};
  std::string str_bot;
};

[[nodiscard]] std::expected<std::vector<LobbyPlayerRef>, std::string>
lobby_player_refs_from_map_yaml(std::string_view str_yaml, std::string_view str_source_name);

struct LobbyMapInfo {
  std::string str_name;
  std::string str_uid;
  std::string str_title;
  std::vector<LobbyPlayerRef> vec_players;
  std::vector<std::string> vec_selectable_factions;
};

[[nodiscard]] std::vector<std::string> ra_selectable_factions();

struct LobbyServerModel {
  LobbySession lobby_info;
  LobbyMapInfo map;
  std::vector<LobbyMapInfo> vec_maps;
  bool b_start_requested{};

  void server_start(std::vector<LobbyMapInfo> vec_maps_in, std::size_t sz_initial_index);
  void join_local_client(std::string_view str_name, std::string_view str_color_hex);
  [[nodiscard]] bool select_map(std::string_view str_map_name);
  [[nodiscard]] bool interpret_command(std::int32_t int4_client_index, std::string_view str_cmd);
};

}

namespace ora::game::world {

struct WorldModel;

constexpr std::uint32_t kOwnerNone{0xffffffffu};

struct ActorModel {
  std::uint32_t u4_id{};
  std::string str_type;
  std::uint32_t u4_owner{kOwnerNone};
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
  std::int32_t int4_generation{};
  bool b_in_world{};
  bool b_will_dispose{};
  bool b_disposed{};
  std::vector<std::uint32_t> vec_trait_handles;
};

[[nodiscard]] const ActorModel* world_actor_by_id(const WorldModel& w, std::uint32_t u4_id);

[[nodiscard]] ActorModel* world_actor_by_id(WorldModel& w, std::uint32_t u4_id);

}

namespace ora::game::network {
struct OrderModel;
}

namespace ora::game::world {

struct WorldModel;

struct TraitModel {
  std::string str_kind;
  std::string str_instance;
  std::int32_t int4_counter{};
  std::int32_t int4_param{};
  std::string str_param_type;
  std::string str_param2;
  std::string str_param3;
  bool b_sync_flag{};
  std::vector<std::pair<std::string, std::string>> vec_kv;
};

struct TraitTemplate {
  std::string str_kind;
  std::string str_instance;
  std::int32_t int4_param{};
  std::string str_param_type;
  std::string str_param2;
  std::string str_param3;
  std::vector<std::pair<std::string, std::string>> vec_kv;
};

struct TraitCreateCtx {
  std::uint32_t u4_owner;
};

struct TraitTickCtx {
  WorldModel& ref_world;
  ActorModel& ref_actor;
};

struct TraitResolveCtx {
  WorldModel& ref_world;
  ActorModel& ref_actor;
  const network::OrderModel& ref_order;
};

struct TraitSpec {
  std::string str_kind{};
  bool b_implements_tick{};
  bool b_implements_sync{};
  bool b_implements_resolve{};
  std::string str_int_param_key{};
  std::string str_str_param_key{};
  std::string str_str_param2_key{};
  std::string str_str_param3_key{};
  std::string str_order_id{};
  std::int32_t int4_order_priority{};
  bool b_order_actor_target{};
  bool b_order_terrain_target{};
  std::function<void(const TraitCreateCtx&, TraitModel&)> fn_create{};
  std::function<void(const TraitTickCtx&, TraitModel&)> fn_tick{};
  std::function<std::int32_t(const WorldModel&, std::uint32_t, const TraitModel&)> fn_sync_hash{};
  std::function<void(const TraitResolveCtx&, TraitModel&)> fn_resolve{};
};

[[nodiscard]] const std::vector<TraitSpec>& trait_registry();

[[nodiscard]] const TraitSpec* trait_spec_find(std::string_view str_kind);

[[nodiscard]] std::pair<std::string_view, std::string_view> trait_key_split(
    std::string_view str_key);

}

namespace ora::game::world {

struct TraitContainerModel {
  std::vector<std::uint32_t> vec_actor_ids;
  std::vector<std::uint32_t> vec_trait_handles;
};

void container_add(TraitContainerModel& container, std::uint32_t u4_actor_id,
                   std::uint32_t u4_trait_handle);

void container_remove_actor(TraitContainerModel& container, std::uint32_t u4_actor_id);

}

namespace ora::game::world {

struct WorldModel;

// ModularBot（ModularBot.cs:43-121）的数据面等价：IBot 激活态、待发订单队列与
// MinOrderQuotientPerTick 节流参数。BotModule 收集面（IBotTick/IBotRespondToAttack）
// 本票为空集，T34 起接入。
struct BotModel {
  std::string str_type;
  std::uint32_t u4_player{};
  bool b_enabled{};
  std::deque<network::OrderModel> deque_orders;
  std::int32_t int4_min_order_quotient{5};
  std::uint32_t u4_issued_total{};
};

// 每 tick 至少发出待发订单的 ceil(pending / quotient) 分之一（ModularBot.cs:101-103）。
[[nodiscard]] std::size_t bot_orders_to_issue(std::size_t u_pending,
                                              std::int32_t int4_min_order_quotient);

// IBot.QueueOrder（ModularBot.cs:87-90）等价：按持有玩家找已激活 bot 入队。
void bot_queue_order(WorldModel& w, std::uint32_t u4_player, network::OrderModel order);

// ModularBot.Activate（ModularBot.cs:60-74）等价：回放重演世界不激活
// （bot 订单已在录包内，Player.cs:217 host-only 激活在单机形态下恒真）。
// Invalid bot type 诊断面（Player.cs:220-221）待 ai.yaml 规则表接入（T34），本票透传。
void bot_activate(WorldModel& w, std::uint32_t u4_player, std::string_view str_bot_type);

// ModularBot.ITick.Tick（ModularBot.cs:93-106）等价：模块 tick（本票空）+
// 节流出单。出单暂存 vec_bot_issued_orders（上游 world.IssueOrder 进订单管道，
// T31 订单世界内 resolve 时改投）。RunUnsynced 包装不移植——bot 代码不得影响
// 同步面由测试侧 rng_shared 账本断言承担（D071 音效隔离断言同款口径）。
void bot_tick_all(WorldModel& w);

}

namespace ora::game::world {
struct WorldModel;
struct FrameEndTask;
}

namespace ora::game::economy {

constexpr std::uint32_t kNoActor{0xffffffffu};

struct ResourceTypeSpec {
  std::string str_name{};
  std::uint8_t u1_index{};
  std::string str_terrain{};
  std::vector<std::string> vec_allowed_terrain{};
  std::int32_t int4_max_density{};
  std::int32_t int4_value{};
};

struct ResourceCellState {
  std::string str_type;
  std::uint8_t u1_density{};
};

struct ResourceLayerState {
  std::vector<std::string> vec_type_order;
  std::map<std::string, ResourceTypeSpec> map_types;
  bool b_recalculate_density{};
  std::int32_t int4_res_cells{};
  std::int32_t int4_width{};
  std::int32_t int4_height{};
  std::vector<ResourceCellState> vec_cells;
};

struct HarvCargoState {
  std::vector<std::pair<std::string, std::int32_t>> vec_contents{};
  std::int32_t int4_capacity{20};
};

enum class ActKind : std::uint8_t {
  kFindAndDeliver = 0,
  kHarvestResource = 1,
  kMoveToDock = 2,
  kMoveOntoAndTurn = 3,
  kGenericDock = 4,
  kWait = 5,
  kMove = 6,
  kTurn = 7,

  kMoveReverse = 8,
};

struct HarvActivity {
  ActKind kind{};
  bool b_first_run{};
  bool b_finishing{};
  bool b_dock_initiated{};
  pathfinding::CPos cell_target{};
  std::int32_t int4_wait_remaining{};
  std::int32_t int4_dock_state{};
  std::int32_t int4_dock_frames{};
  std::int32_t int4_child{-1};
  std::int32_t int4_next{-1};
};

enum class MoveOutcome : std::uint8_t {
  kInProgress = 0,
  kCompleteDestinationReached = 1,
  kCompleteDestinationBlocked = 2,
};

struct HarvState {
  std::uint32_t u4_actor{kNoActor};
  pathfinding::MoveSimState move;
  std::vector<HarvActivity> vec_activities;
  std::int32_t int4_root{-1};
  HarvCargoState cargo;
  std::optional<pathfinding::CPos> opt_last_harvested;
  bool b_last_search_failed{};
  bool b_has_waited{};
  bool b_has_delivered_load{};
  bool b_has_harvested_cell{};
  std::uint32_t u4_reserved_host{kNoActor};
  std::uint32_t u4_last_reserved_host{kNoActor};
  MoveOutcome move_outcome{MoveOutcome::kInProgress};
  std::int32_t int4_unload_cooldown{};
  std::set<std::pair<std::int32_t, std::int32_t>> set_blocked_move;
};

struct DockHostState {
  std::uint32_t u4_actor{kNoActor};
  std::vector<std::uint32_t> vec_reserved;
  std::uint32_t u4_docked_client{kNoActor};
};

struct EconomyConfig {
  std::int32_t int4_bale_load_delay{4};
  std::int32_t int4_bale_unload_delay{1};
  std::int32_t int4_bale_unload_amount{1};
  std::int32_t int4_search_from_proc_radius{15};
  std::int32_t int4_search_from_harvester_radius{8};
  std::int32_t int4_harvest_facings{8};
  std::int32_t int4_harv_speed{72};
  std::int32_t int4_harv_turn_speed{20};
  std::int32_t int4_fully_loaded_speed{85};
  std::int32_t int4_dock_wait{10};
  std::int32_t int4_dock_frames{8};
  std::int32_t int4_dock_angle{256};
  std::int32_t int4_refinery_penalty{200};
  std::int32_t int4_wait_duration{25};
  std::int32_t int4_search_for_dock_delay{125};
  std::int32_t int4_max_queue_length{3};
  std::int32_t int4_harv_capacity{20};
  std::int32_t int4_player_capacity{2000};
  std::string str_free_actor{"harv"};
  std::int32_t int4_free_offset_x{1};
  std::int32_t int4_free_offset_y{2};
  std::int32_t int4_free_facing{256};
  pathfinding::CPos cell_dock{};
  pathfinding::WPos pos_dock{};
  std::string str_host_type{"proc"};
  std::string str_harv_type{"harv"};
  bool b_use_storage{true};
  bool b_discard_excess{false};
  std::vector<pathfinding::LocomotorTerrainSpeed> vec_locomotor_speeds;
};

struct EconomyWorldState {
  ResourceLayerState layer;
  std::vector<HarvState> vec_harvs;
  std::vector<DockHostState> vec_hosts;
  pathfinding::LocomotorMap map_move;
  std::map<std::pair<std::int32_t, std::int32_t>, std::pair<std::int16_t, std::int32_t>> map_cost_base;
  EconomyConfig cfg;
  bool b_ready{};
};

[[nodiscard]] std::expected<void, std::string> economy_setup(
    world::WorldModel& w, const MapBin& bin_map, const Tileset& tileset,
    std::string_view str_rules_text, std::int32_t int4_left, std::int32_t int4_top,
    std::int32_t int4_right, std::int32_t int4_bottom);

void economy_on_tick(world::WorldModel& w);

void economy_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id);

void economy_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id);

void economy_frame_end_task(world::WorldModel& w, const world::FrameEndTask& task);

[[nodiscard]] std::int32_t harv_cargo_sum(const HarvCargoState& cargo);

[[nodiscard]] bool harv_is_full(const EconomyWorldState& econ, const HarvState& harv);

[[nodiscard]] bool harv_is_empty(const HarvState& harv);

[[nodiscard]] std::int32_t harv_fullness(const HarvState& harv);

void cargo_add(HarvCargoState& cargo, std::string_view str_type, std::int32_t int4_amount);

std::int32_t refinery_accept(world::WorldModel& w, std::uint32_t u4_player,
                             std::string_view str_type, std::int32_t int4_count);

std::int32_t resource_remove(ResourceLayerState& layer, EconomyWorldState& econ,
                             std::string_view str_type, pathfinding::CPos cell);

[[nodiscard]] const ResourceCellState& resource_get(const ResourceLayerState& layer,
                                                    pathfinding::CPos cell);

[[nodiscard]] std::string economy_observation(const world::WorldModel& w,
                                              std::uint32_t u4_player);

[[nodiscard]] std::string economy_actor_line(const world::WorldModel& w);

[[nodiscard]] std::string harv_activity_chain(const HarvState& harv);

[[nodiscard]] const std::vector<world::TraitSpec>& economy_trait_specs();

}

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::production {

enum class PowerState : std::uint8_t { kNormal = 0, kLow = 1, kCritical = 2 };

struct LobbyModel {
  std::vector<std::pair<std::string, std::string>> vec_playable_factions;
  // 槽位 bot 注入面（上游 Session.Client.Bot → Player.BotType 的桥）：
  // 槽名 → bot 类型（ModularBotInfo.Type，如 normal/rush）。
  std::vector<std::pair<std::string, std::string>> vec_slot_bots;
  std::int32_t int4_starting_cash{5000};
  std::string str_techlevel{"unrestricted"};
  bool b_build_radius{true};
};

struct ProductionItemModel {
  std::string str_item;
  std::int32_t int4_total_cost{};
  std::int32_t int4_remaining_cost{};
  std::int32_t int4_resources_paid{};
  std::int32_t int4_total_time{1};
  std::int32_t int4_remaining_time{1};
  std::int32_t int4_slowdown{};
  bool b_started{};
  bool b_paused{};
  bool b_done{};
  bool b_ready_notified{};
};

struct ProductionQueueState {
  std::string str_type;
  std::int32_t int4_low_power_modifier{100};
  bool b_enabled{};
  bool b_valid_faction{true};
  std::vector<ProductionItemModel> vec_queue;
  std::string str_ready_audio;
  std::string str_blocked_audio;
};

struct PowerManagerState {
  std::map<std::uint32_t, std::int32_t> map_power_drain;
  std::int32_t int4_power_provided{};
  std::int32_t int4_power_drained{};
};

struct PlayerResourcesState {
  std::int32_t int4_cash{};
  std::int32_t int4_resources{};
  std::int32_t int4_capacity{};
};

struct PlayerProductionState {
  std::vector<ProductionQueueState> vec_queues;
  PowerManagerState power;
  PlayerResourcesState res;
};

struct ProductionWorldState {
  std::vector<PlayerProductionState> vec_players;
};

[[nodiscard]] PowerState power_state(std::int32_t int4_provided, std::int32_t int4_drained);

[[nodiscard]] std::string_view power_state_name(PowerState state);

[[nodiscard]] std::int32_t apply_percentage_modifiers(
    std::int32_t int4_value, std::span<const std::int32_t> vec4_modifiers);

bool take_cash(PlayerResourcesState& res, std::int32_t int4_num);

void refund_cash(PlayerResourcesState& res, std::int32_t int4_num);

void refund_resources(PlayerResourcesState& res, std::int32_t int4_num);

template <typename T>
void shuffle_forward(support::MersenneTwister& rng, std::span<T> span_items) {
  for (std::size_t idx{0}; idx + 1 < span_items.size(); ++idx) {
    const std::int32_t int4_j{
        support::next(rng, static_cast<std::int32_t>(span_items.size() - idx))};
    std::swap(span_items[idx], span_items[static_cast<std::size_t>(int4_j) + idx]);
  }
}

[[nodiscard]] std::string production_observation(const world::WorldModel& w,
                                                 std::uint32_t u4_player);

[[nodiscard]] std::vector<std::string> queue_buildable_items(const world::WorldModel& w,
                                                             std::uint32_t u4_player,
                                                             std::string_view str_queue_type);

void production_queue_state_build(world::WorldModel& w);

void production_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id);

void production_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id);

void production_frame_end_task(world::WorldModel& w, std::uint32_t u4_kind,
                               std::uint32_t u4_player, std::uint32_t u4_value,
                               const std::string& str_item);

[[nodiscard]] const std::vector<world::TraitSpec>& production_trait_specs();

}

namespace ora::game::world {

// T31 移动活动面（Activity.cs/Move.cs/AttackMoveActivity.cs 的最小等价，无阻塞面）：
// 每 Mobile 单位一份移动状态 = 上游 Actor.CurrentActivity 的 Move 子集，由
// move_sim（T11 引擎）承载；Move/AttackMove 订单经订单管道 world 内 resolve 驱动。
struct MobileState {
  std::uint32_t u4_actor{};
  pathfinding::MoveSimState move;
  // Actor.QueueActivity(queued=true)（Activity.cs:227 Queue）的 next 链最小等价：
  // 当前移动完成后依序起（ActivityUtils.RunActivity 同帧接续语义）。
  std::vector<pathfinding::CPos> vec_queued_destinations;
  std::int32_t int4_speed{113};
  std::int32_t int4_turn_speed{512};
};

[[nodiscard]] MobileState* movement_state_of(WorldModel& w, std::uint32_t u4_actor);

[[nodiscard]] const MobileState* movement_state_of(const WorldModel& w,
                                                   std::uint32_t u4_actor);

// Mobile 单位登记/摘除（create_actor/帧尾移除时挂接；harv 归 economy FDA 自治不入面）。
void movement_on_actor_added(WorldModel& w, std::uint32_t u4_actor_id);

void movement_on_actor_removed(WorldModel& w, std::uint32_t u4_actor_id);

// world_tick 的 Activity 段移动面（上游 Actor.CurrentActivity tick 等价位）。
void movement_on_tick(WorldModel& w);

// 世界内活动驱动的移动入队（上游 Activity 内部 move：AttackBase 追击停走等，
// 不经订单管道——launcher 攻击桥的移动承载改接此处）。
void movement_queue_move(WorldModel& w, std::uint32_t u4_actor, pathfinding::CPos cell);

// 移动域 trait 注册（Mobile=IResolveOrder "Move"、AttackMove=AttackMove/AssaultMove）。
[[nodiscard]] const std::vector<TraitSpec>& movement_trait_specs();

}

namespace ora::game::world {

[[nodiscard]] std::int32_t sync_hash_upstream(const WorldModel& w);

[[nodiscard]] std::string state_digest(const WorldModel& w);

}

namespace ora::game::world {

struct MapPlayerRef {
  std::string str_name;
  bool b_owns_world{};
  bool b_non_combatant{};
  bool b_playable{};
  std::string str_faction;
};

struct MapActorRef {
  std::string str_type;
  std::string str_owner;
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
};

struct SyntheticMap {
  std::string str_title;
  std::vector<std::string> vec_rule_texts;
  std::string str_map_text;
  std::vector<MapPlayerRef> vec_player_refs;
  std::vector<MapActorRef> vec_actor_defs;
};

struct FrameEndTask {
  std::uint32_t u4_kind{};
  std::uint32_t u4_actor{};
  std::uint32_t u4_player{};
  std::uint32_t u4_value{};
  std::string str_item{};
};

struct PlayerModel {
  std::string str_internal_name;
  std::string str_faction;
  std::uint32_t u4_actor_id{};
  bool b_owns_world{};
  bool b_non_combatant{};
  bool b_spectating{};
  bool b_playable{};
  // Player.IsBot = BotType != null（Player.cs:202）的载体：空串 = 非 bot。
  std::string str_bot_type;
};

struct ActorTypeDef {
  std::string str_name_lower;
  std::vector<TraitTemplate> vec_templates;
};

struct WorldModel {
  std::vector<ActorTypeDef> vec_actor_types;
  std::vector<ActorModel> vec_actors_all;
  std::vector<std::uint32_t> vec_in_world;
  std::vector<TraitModel> vec_traits;
  TraitContainerModel container_tick;
  TraitContainerModel container_sync;
  std::vector<PlayerModel> vec_players;
  production::ProductionWorldState prod;
  economy::EconomyWorldState econ;
  std::vector<MobileState> vec_mobiles;
  production::LobbyModel lobby;
  std::vector<BotModel> vec_bots;
  // bot_tick 出单暂存面（上游 world.IssueOrder 的等价落点，T31 改投订单管道）。
  std::vector<network::OrderModel> vec_bot_issued_orders;
  support::MersenneTwister rng_shared;
  support::MersenneTwister rng_player;
  // World.LocalRandom（World.cs:200，无种子墙钟 MT）的等价载体：bot 决策随机
  // 走此流，不进同步面（种子派生口径见 COVERAGE 登记）。
  support::MersenneTwister rng_local;
  std::int32_t int4_world_tick{};
  bool b_paused{};
  // 回放重演世界位（ModularBot.Activate 的 IsReplay 早退）。
  bool b_is_replay{};
  std::uint32_t u4_net_frame{1};
  std::deque<FrameEndTask> deque_frame_end;
  std::uint32_t u4_next_aid{};
  std::uint32_t u4_world_owner_player{};
  ora::game::sfx_context* ptr_sfx{nullptr};
};

[[nodiscard]] std::expected<SyntheticMap, std::string> synthetic_map_from_yaml(
    std::string_view str_rules_text, std::string_view str_map_text);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed,
    const production::LobbyModel& lobby, bool b_is_replay = false);

[[nodiscard]] std::expected<std::uint32_t, std::string> create_actor(
    WorldModel& w, std::string_view str_type, std::uint32_t u4_owner,
    std::int32_t int4_pos_x, std::int32_t int4_pos_y, bool b_add_to_world);

void dispose_actor(WorldModel& w, std::uint32_t u4_actor_id);

[[nodiscard]] std::int32_t process_orders(WorldModel& w);

void world_tick(WorldModel& w);

}

namespace ora::game::replay {

constexpr std::int32_t kMetaStartMarker{-1};
constexpr std::int32_t kMetaEndMarker{-2};
constexpr std::int32_t kMetaVersion{1};

struct ReplayRecord {
  std::int32_t int4_client{};
  std::vector<std::uint8_t> vec_data;
  std::size_t idx_offset{};
};

struct ReplayFile {
  std::vector<ReplayRecord> vec_records;
  bool b_has_metadata{};
  std::vector<MiniYamlNode> vec_meta_nodes;
  std::int32_t int4_final_game_tick{};
};

[[nodiscard]] std::expected<ReplayFile, std::string> replay_read(
    std::span<const std::uint8_t> span_bytes);

[[nodiscard]] std::vector<std::uint8_t> replay_write(const ReplayFile& file);

[[nodiscard]] std::optional<std::string_view> meta_find_value(
    const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_parent,
    std::string_view str_key);

}

namespace ora::game::replay {

struct ReplaySessionInfo {
  std::int32_t int4_seed{};
  std::string str_map_uid;
  std::int32_t int4_net_frame_interval{1};
  std::int32_t int4_game_timestep{40};
  std::vector<std::int32_t> vec_client_indices;
};

[[nodiscard]] std::string replay_session_yaml(const ReplaySessionInfo& info);

[[nodiscard]] std::expected<ReplaySessionInfo, std::string> replay_session_parse(
    std::string_view str_yaml);

struct ReplayGameInfo {
  std::string str_mod;
  std::string str_version;
  std::string str_map_uid;
  std::string str_map_title;
  std::int32_t int4_final_game_tick{};
  std::string str_start_time_utc;

  struct PlayerInfo {
    std::int32_t int4_client_index{};
    std::string str_name;
    bool b_is_human{true};
  };
  std::vector<PlayerInfo> vec_players;
};

[[nodiscard]] std::vector<MiniYamlNode> replay_metadata_nodes(const ReplayGameInfo& info);

struct ReplayRecorderModel {
  std::vector<ReplayRecord> vec_records;
  bool b_game_start_seen{};
  std::vector<MiniYamlNode> vec_meta_nodes;
  bool b_metadata_attached{};
};

void recorder_receive(ReplayRecorderModel& rec, std::int32_t int4_client,
                      std::vector<std::uint8_t> vec_data);

void recorder_attach_metadata(ReplayRecorderModel& rec, std::vector<MiniYamlNode> vec_nodes);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> recorder_dispose(
    ReplayRecorderModel& rec);

[[nodiscard]] std::vector<std::uint8_t> make_boot_packet(std::string_view str_order,
                                                         std::string_view str_target);

}

namespace ora::game::replay {

enum class FaceTraitKind : std::uint8_t {
  kDebugPauseState = 0,
  kShroud,
  kPlayerResources,
  kMissionObjectives,
  kDeveloperMode,
  kGpsWatcher,
  kPlayerExperience,
  kClassicProductionQueue,
  kPowerManager,
  kFrozenActorLayer,
  kImmobile,
  kBodyOrientation,
  kClassicFacingBodyOrientation,
  kBuilding,
  kHealth,
  kRevealsShroud,
  kFrozenUnderFog,
  kRepairableBuilding,
  kConyardChronoReturn,
  kDockHost,
  kGainsExperience,
  kMobile,
  kChangesHealth,
  kTakeCover,
  kAttackFrontal,
  kAutoTarget,
  kChronoshiftable,
  kStoresResources,
  kHarvester,
};

struct FaceTypeSpec {
  std::string_view str_type;
  std::int32_t int4_hp{};
  std::int32_t int4_facings{};
  std::uint8_t u1_flags{};
  std::vector<FaceTraitKind> vec_traits;
};

[[nodiscard]] const FaceTypeSpec* face_type_find(std::string_view str_type);

constexpr std::int32_t kNoAggressor{-1};

struct FaceActor {
  std::uint32_t u4_id{};
  std::string str_type;
  const FaceTypeSpec* ptr_face{};
  std::uint32_t u4_owner{};
  std::int32_t int4_x{}, int4_y{};
  bool b_in_world{true};
  bool b_disposed{};

  std::int32_t int4_hp{};
  std::int32_t int4_facing{};
  std::int32_t int4_from_x{}, int4_from_y{}, int4_to_x{}, int4_to_y{};
  std::int32_t int4_px{}, int4_py{}, int4_pz{};
  std::int32_t int4_next_scan{};
  std::int32_t int4_aggressor{kNoAggressor};
  std::int32_t int4_take_cover{};
  bool b_aiming{};
  std::int32_t int4_experience{}, int4_level{};
  std::int32_t int4_frozen_vis{};
  std::int32_t int4_content_hash{};
  std::int32_t int4_unload_ticks{};
  std::int32_t int4_ch_ticks[2]{};
  std::int32_t int4_ch_damage[2]{};

  bool b_activity{};
  bool b_moving{};
  std::int32_t int4_wib_state{0};
  std::int32_t int4_wib_delay{};
  bool b_wib_quirk{};
  std::int32_t int4_reload{};
  std::int32_t int4_activity_target{-1};
};

struct FacePlayer {
  std::string str_name;
  std::uint32_t u4_actor_id{};
  std::int32_t int4_cash{5000};
  std::int32_t int4_resources{};
  std::int32_t int4_capacity{};
  std::int32_t int4_pexp{};
  std::int32_t int4_power_provided{};
  std::int32_t int4_power_drained{};
  bool b_queue_enabled[6]{true, true, true, true, true, true};
  bool b_queue_faction[6]{true, true, true, true, true, true};
  std::int32_t int4_frozen_vis{};
  std::int32_t int4_frozen_sum{};
};

struct OracleProdItem {
  std::string str_item;
  std::int32_t int4_cost{};
  std::int32_t int4_total_time{};
  std::int32_t int4_remaining_time{};
  std::int32_t int4_remaining_cost{};
  std::int32_t int4_slowdown{};
  bool b_started{};
  bool b_done{};
  bool b_oncomplete_fired{};
};

struct ReplayOracleWorld {
  std::vector<FaceActor> vec_actors;
  std::vector<FacePlayer> vec_players;
  support::MersenneTwister rng_shared;

  std::vector<std::set<std::pair<std::int32_t, std::int32_t>>> vec_sight;

  std::vector<std::pair<std::int32_t, std::int32_t>> vec_delayed_effect;

  std::vector<OracleProdItem> vec_prod_items;

  struct SyncEffect {
    std::int32_t int4_hash{};
  };
  std::vector<SyncEffect> vec_sync_effects;

  std::string str_fatal{};
};

struct OracleChange {
  std::uint32_t u4_actor{};
  std::string str_trait;
  std::string str_field;
  std::string str_old;
  std::string str_new;
};

struct OracleReplayContext {

  std::map<std::int32_t, std::vector<OracleChange>> map_changes;

  std::map<std::int32_t, std::int32_t> map_rng_count;

  std::int32_t int4_146_move_end{};
};

[[nodiscard]] std::expected<OracleReplayContext, std::string> replay_oracle_context_parse(
    std::string_view str_changes_text, std::string_view str_frames_text);

[[nodiscard]] const std::vector<std::string>& replay_oracle_init_manifest();

[[nodiscard]] std::expected<ReplayOracleWorld, std::string> replay_oracle_create(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap);

void replay_oracle_frame1(ReplayOracleWorld& w);

void replay_oracle_frame(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                         std::int32_t int4_frame);

void replay_oracle_apply_order(ReplayOracleWorld& w, const network::OrderModel& order);

void replay_oracle_spawn_e1(ReplayOracleWorld& w);
void replay_oracle_spawn_powr(ReplayOracleWorld& w, std::int32_t int4_cell_bits);

[[nodiscard]] std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w);

[[nodiscard]] std::string replay_oracle_pertrait_dump(const ReplayOracleWorld& w);

}

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {
struct OrderManagerModel;
}

namespace ora::game::replay {

constexpr std::int32_t kReplayOrderLatency{3};

struct ReplayChunk {
  std::int32_t int4_frame{};
  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_packets;
};

struct ReplayFeedModel {
  std::deque<ReplayChunk> deque_chunks;

  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_recorded_syncs;
  ReplaySessionInfo session;
  bool b_valid{};
  std::int32_t int4_tick_count{};

  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_local_syncs;

  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_replayed_orders;

  std::vector<std::string> vec_desync_reports;
};

[[nodiscard]] std::expected<ReplayFeedModel, std::string> replay_feed_build(
    const ReplayFile& file);

void replay_feed_pump(ReplayFeedModel& feed, network::OrderManagerModel& om,
                      world::WorldModel& w);

}
}
