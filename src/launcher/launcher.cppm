export module ora.launcher;

import std;
import ora.game;
import ora.game.combat;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.world;
import ora.game.pathfinding;
import ora.game.ui;

export {
namespace ora::launcher {

struct CliArgs {
  std::string str_mod{"ra"};
  std::string str_map;
  std::int32_t int4_seed{0};
  std::int32_t int4_tick_ms{40};
  bool b_fullscreen{};
  std::string str_content_dir;
  std::string str_log_file;
  bool b_headless{};
  std::int32_t int4_headless_ticks{600};
  bool b_demo{};
  bool b_lobby{};
  std::string str_maps_dir;
  std::string str_chrome_dir;
  std::string str_record;
  std::string str_replay;
};

struct CliParseResult {
  bool b_ok{true};
  bool b_help{};
  bool b_version{};
  CliArgs args;
  std::vector<std::string> vec_errors;
};

[[nodiscard]] CliParseResult cli_args_parse(std::span<const std::string_view> vec_args);

[[nodiscard]] std::string cli_help_text();

}

namespace ora::launcher {

struct SandboxMapData {
  game::MapBin bin_map;
  game::Tileset tileset;
};

[[nodiscard]] std::string_view sandbox_rules_text();

[[nodiscard]] std::string_view sandbox_map_text();

[[nodiscard]] std::string_view sandbox_weapons_text();

[[nodiscard]] SandboxMapData sandbox_map_data();

struct LoadedMap {
  game::world::SyntheticMap map;
  game::MapBin bin_map;
  game::Tileset tileset;
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::string str_title;
};

[[nodiscard]] std::expected<LoadedMap, std::string> load_builtin_sandbox();

[[nodiscard]] std::expected<LoadedMap, std::string> load_oramap(
    std::string_view str_path, std::string_view str_tileset_path = "data/temperat.yaml");

}

namespace ora::launcher {

// 全流程模式（T28 串联票）：同一时刻仅一个模式活跃（互斥由 dispatch 受理矩阵保证，
// 模式外事件拒绝且状态不变）。上游等价物 = MainMenuLogic 的 MenuType + 窗口回调网
//（OpenSkirmishLobbyPanel onStart/onExit、IngameMenuLogic onExit、OpenMenuBasedOnLastGame），
// 上游无集中状态机对象——C++ 折叠为单一可无头断言的序列真源（U100/U102 精读锚定）。
enum class FlowMode { kMainMenu, kLobby, kGame, kExit };

// 串联面事件（上游语义括注）：
// kMenuSingleplayer/kMenuExtras/kMenuMapEditor —— 主菜单容器内按钮 SwitchMenu
// kMenuBack   —— BACK_BUTTON 语境回退（singleplayer/extras→Main、map-editor→Extras）
// kMenuQuit   —— QUIT_BUTTON（MAIN_MENU 容器）→ Game.Exit
// kSkirmish   —— SKIRMISH_BUTTON（SINGLEPLAYER_MENU 容器）→ StartSkirmishGame：
//                SwitchMenu(None) + 连接本地服务器 → SERVER_LOBBY 大堂
// kLobbyDisconnect —— DISCONNECT/ESC 大堂断开 → onExit: Disconnect+SwitchMenu(Singleplayer)
// kLobbyStart —— START GAME/Enter → RemoveShellmapUI + lastGameState=Skirmish → 对局
// kGameAbort  —— 对局 ESC/ABORT_MISSION → onExit → shellmap → OpenMenuBasedOnLastGame
//                (Skirmish) → StartSkirmishGame（重连大堂，不落主菜单）
enum class FlowEvent {
  kMenuSingleplayer,
  kMenuExtras,
  kMenuMapEditor,
  kMenuBack,
  kMenuQuit,
  kSkirmish,
  kLobbyDisconnect,
  kLobbyStart,
  kGameAbort
};

struct FlowState {
  FlowMode mode{FlowMode::kMainMenu};
  game::ui::MenuType menu_type{game::ui::MenuType::kStartupPrompts};
  // 模式切换序列记录（"switch <MenuType>" 与 mainmenu_logic vec_nav 同格式 +
  // start-skirmish/start-game/abort-mission/disconnect/quit 编排行）
  std::vector<std::string> vec_sequence;
};

// StartupPrompts→Main 折叠（同 main_menu_setup，U100 ctor 尾链）。
void flow_setup(FlowState& st);

// 事件受理路由：受理返回 true 并推进模式/菜单态；模式外/语境外事件拒绝（false）且
// 状态与序列不变。终态 kExit 拒绝一切事件。
bool flow_dispatch(FlowState& st, FlowEvent ev);

}  // namespace ora::launcher

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

namespace ora::launcher {

struct SessionEnv {
  std::int32_t int4_seed{0};
  std::string str_map_path;

  std::string str_tileset_path{"data/temperat.yaml"};
  std::optional<game::production::LobbyModel> opt_lobby;
};

// D044 移动桥已退役（T31 移动 Activity 化）：移动承载在世界侧
// WorldModel.vec_mobiles（Move/AttackMove 订单经订单管道 world 内 resolve）。
// FighterState 为攻击桥残留面（攻击/HP/开火归 T32/T34 票），其移动接口改接
// world::movement_queue_move/movement_state_of。
struct FighterState {
  game::combat::CombatUnit unit;
  game::combat::ArmamentState arm;
  game::combat::HealthState hp;
  std::string str_weapon;
  std::uint32_t u4_target{};
};

struct GameSession {
  game::world::WorldModel w;
  game::network::OrderManagerModel om;
  game::combat::WeaponTable weapons;
  std::vector<FighterState> vec_fighters;
  // 攻击桥喂单面（D044 残留：Attack 订单按 N 发 N+1 应用帧对齐；Move 已世界内）。
  std::vector<std::pair<std::uint32_t, game::network::OrderModel>> vec_bridge_pending;
  std::uint32_t u4_viewer{2};
  std::vector<std::string> vec_events;
};

[[nodiscard]] std::expected<GameSession, std::string> session_setup(const SessionEnv& env);

void session_issue_order(GameSession& s, const game::network::OrderModel& order);

void session_tick(GameSession& s);

[[nodiscard]] std::string session_digest(const GameSession& s);

[[nodiscard]] std::string session_hud_line(const GameSession& s);

[[nodiscard]] std::vector<game::input::ProductionQueueView> session_queue_views(
    const GameSession& s);

}
}
