// UPSTREAM: OpenRA.Game/Game.cs @1f594e320 + OpenRA.Game/Support/Program.cs
// L14-19(主循环骨架逐语义:RunTime 时钟/延迟动作队列/JoinInner·JoinLocal/
// InnerLogicTick 的双 TickTime 节拍/LogicTick 的连接态事件/RenderTick 的
// prepare→world→UI→flip 全序/Loop 的逻辑-渲染双时间表与追帧截断/Run 的
// finally 清理)。Phase 5-8 依赖面 → Deps/钩子注入:Renderer/Sound/
// CursorManager/ChromeProvider/WorldRenderer+Viewport/Ui、Settings.Graphics
// 值面、NetworkConnection 的连接态观测(Phase 7)、world.OrderGenerator
// (Phase 5)、PerfHistory/PerfSample/Benchmark/截图(Phase 5+)、
// TextNotificationsManager/UnitOrders.Clear(第十六批客户端接线落地:
// JoinInner 双 Clear + LobbyInfoChanged 事件转发)
// —— 不在场的编排点全部 no-op 并注释锚定。
// Verbatim-semantics rewrite of the main-loop skeleton (the RunTime clock /
// the delayed-action queue / JoinInner·JoinLocal / InnerLogicTick's dual
// TickTime pacing / LogicTick's connection-state event / RenderTick's
// prepare→world→UI→flip order / Loop's logic-render dual schedule with the
// catch-up cutoff / Run's finally cleanup). Phase 5-8 dependency faces
// arrive as Deps/hooks — every absent orchestration point is a commented
// no-op (see the CN notes).
#pragma once
import std;

#include "core/action_queue.hpp"
#include "core/arguments.hpp"
#include "core/mersenne_twister.hpp"
#include "game/load_screen.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "net/connection.hpp"
#include "net/order_manager.hpp"
#include "platform/ifont.hpp"
#include "platform/sdl2_input.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"

namespace ora::gfx {
class Renderer;
class CursorManager;
class ChromeProvider;
class WorldRenderer;
class Viewport;
struct IPaletteModifier;
}  // namespace ora::gfx
namespace ora::map {
class MapCache;
class MapPreview;
}  // namespace ora::map
namespace ora::sound {
class Sound;
}
namespace ora::net {
class ConnectionTarget;
}
namespace ora::sim {
class World;
}
namespace ora::game {
class ModData;
class LocalPlayerProfile;
}

namespace ora::game {

/// RunStatus(Program.cs L14-19)
/// RunStatus (Program.cs L14-19).
enum class RunStatus : std::int32_t {
  Error = -1,
  Success = 0,
  Running = std::numeric_limits<std::int32_t>::max(),
};

class Game {
 public:
  /// 注入面(缺失编排点的 no-op 语义见文件头) | the injection faces (the
  /// no-op semantics of absent orchestration points per the header).
  struct Deps {
    std::function<std::int64_t()> fn_clock;  // Game.RunTime;缺省 = 构造起 steady ms

    gfx::Renderer* ptr_renderer = nullptr;      // 空 = 无头(Loop 全逻辑化)
    sound::Sound* ptr_sound = nullptr;
    gfx::CursorManager* ptr_cursor = nullptr;
    gfx::ChromeProvider* ptr_chrome_provider = nullptr;
    ui::Ui* ptr_ui = nullptr;                       // 缺省取 Ui::Instance()

    // NetworkConnection 的连接态观测(Phase 7);空 = EchoConnection 恒跳过
    // the NetworkConnection state observation (Phase 7); empty = the
    // EchoConnection always skips.
    std::function<std::optional<net::ConnectionState>()> fn_connection_state;
    std::function<void(net::OrderManager&)> fn_connection_state_changed;

    /// world.OrderGenerator.Tick(world)(Phase 5;缺省 no-op)
    /// world.OrderGenerator.Tick(world) (Phase 5; a no-op by default).
    std::function<void(sim::World&)> fn_order_generator_tick;

    /// 挂起窗口的事件泵(Renderer.Window.PumpInput(NullInputHandler);输入
    /// 装配随 Phase 7) | the suspended-window event pump.
    std::function<void()> fn_pump_suspended_input;

    /// World.IsReplay(上游 = Connection is ReplayConnection;Phase 7 前
    /// 缺省恒 false) | the World.IsReplay face (upstream = Connection is
    /// ReplayConnection; false by default until Phase 7).
    std::function<bool(const sim::World&)> fn_is_replay;

    std::function<void()> fn_on_quit;

    /// JoinLocal 观战客户端名(Settings.Player.Name;Phase 5)
    /// the JoinLocal spectator name (Settings.Player.Name; Phase 5).
    std::string str_player_name = "New Player";

    /// ModData(嵌入侧所有权 —— D105 系;MapCache/StartGame 装配所需)
    /// ModData (embedder-owned — the D105 family; needed by the MapCache/
    /// StartGame assembly).
    game::ModData* ptr_mod_data = nullptr;

    /// WorldRenderer 装配钩子(上游 Game.StartGame 直接 new;渲染面为
    /// 测试注入面 —— 无钩子 = 无头)
    /// The WorldRenderer assembly hook (upstream news it in StartGame; the
    /// render face is the test injection — no hook = headless).
    std::function<std::unique_ptr<gfx::WorldRenderer>(sim::World&)>
        fn_create_world_renderer;

    /// ModData.PrepareMap(L206)的序列装载面(上游 Sequences.LoadSprites;
    /// Phase 6 资产链前的注入面 —— 无钩子 = 跳过)
    /// ModData.PrepareMap's (L206) sequence-loading face (upstream's
    /// Sequences.LoadSprites; an injection face until the Phase 6 asset
    /// chain — no hook = skipped).
    std::function<void(map::Map&)> fn_prepare_map;
  };

  struct InitializeModDeps {
    std::function<void()> fn_shutdown_server;
    std::function<std::unique_ptr<ModData>(const Manifest&)>
        fn_create_mod_data;
    std::function<std::unique_ptr<ILoadScreen>(const Manifest&)>
        fn_create_load_screen;
    std::function<std::unique_ptr<gfx::CursorManager>(ModData&)>
        fn_create_cursor;
    std::function<void()> fn_reset_local_player_profile;
    std::function<void(const std::string&, const core::Arguments&)>
        fn_switch_mod;
    std::function<std::unique_ptr<net::OrderManager>(const std::string&)>
        fn_create_and_join_local_server;
    std::function<std::unique_ptr<platform::IFont>(
        std::span<const std::uint8_t>)>
        fn_create_font;
    std::string str_map_filter;
  };

  void SetInitializeModDeps(InitializeModDeps deps_init) {
    deps_init_ = std::move(deps_init);
  }

  void SetPrepareMapHook(std::function<void(map::Map&)> fn_prepare) {
    deps_.fn_prepare_map = std::move(fn_prepare);
  }

  explicit Game(Deps deps_args);
  ~Game();

  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  /// RunTime(Game.cs L130-132;Stopwatch 等价)
  /// RunTime (Game.cs L130-132; the Stopwatch equivalent).
  std::int64_t RunTime() const;

  std::int32_t RenderFrame() const { return int4_render_frame_; }  // L134

  /// NetFrameNumber/LocalTick(L135-136;OM 缺失时上游为 NRE —— 调用方保证)
  /// NetFrameNumber/LocalTick (L135-136; a missing OM is an upstream NRE —
  /// the caller guarantees).
  int NetFrameNumber() const;
  int LocalTick() const;

  net::OrderManager* OrderManagerFace() const { return up_order_manager_.get(); }

  /// CosmeticRandom(L53;not synced 上游注释) | CosmeticRandom (L53).
  MersenneTwister& CosmeticRandom() { return mt_cosmetic_; }

  bool HideCursor() const { return b_hide_cursor_; }
  void SetHideCursor(bool b_value) { b_hide_cursor_ = b_value; }

  Modifiers GetModifierKeys() const { return kind_modifiers_; }            // L327
  void HandleModifierKeys(Modifiers kind_mods) { kind_modifiers_ = kind_mods; }

  // ———— 延迟动作(L601-606/691-694)————
  void RunAfterTick(std::function<void()> fn_a) { delayed_actions_.Add(std::move(fn_a), RunTime()); }
  void RunAfterDelay(std::int32_t int4_delay_ms, std::function<void()> fn_a) {
    delayed_actions_.Add(std::move(fn_a), RunTime() + int4_delay_ms);
  }
  void PerformDelayedActions() { delayed_actions_.PerformActions(RunTime()); }

  // ———— 连接面(L89-128/934-946)————
  /// JoinInner(L89-104):非 shellmap OM 先 Dispose(上游所有权序)
  /// JoinInner (L89-104): a non-shellmap OM is disposed first.
  void JoinInner(std::unique_ptr<net::OrderManager> up_om);

  /// JoinLocal(L111-128):EchoConnection + 观战客户端(色面随 Phase 5)
  /// JoinLocal (L111-128): the EchoConnection + the spectator client (the
  /// color faces land with Phase 5).
  void JoinLocal();

  /// JoinReplay(L106-109):OrderManager(ReplayConnection);orderLatency =
  /// GameSpeeds 注入面(上游 ctor 内查;第十一批)
  /// JoinReplay (L106-109): the OrderManager over the ReplayConnection;
  /// orderLatency = the GameSpeeds injection face (upstream reads it in
  /// the ctor; batch 11).
  void JoinReplay(std::vector<std::uint8_t> vec_replay_bytes,
                  int int4_order_latency = 3);

  // ———— 主循环(L598/625-932)————
  RunStatus State() const { return state_; }
  void Exit() { state_ = RunStatus::Success; }  // L929-932

  /// InnerLogicTick(L625-674) | InnerLogicTick (L625-674).
  void InnerLogicTick(net::OrderManager& om_order);
  void LogicTick();     // L676-689
  void RenderTick();    // L701-769
  void Loop();          // L771-897
  RunStatus Run();      // L899-927

  /// IsCurrentWorld(L987-990) | IsCurrentWorld (L987-990).
  bool IsCurrentWorld(const sim::World* ptr_world) const;

  /// LobbyInfoChanged(L179-183 事件面;UnitOrders 的 SyncLobby* 分支经
  /// Game.SyncLobbyInfo 触发 —— 第十六批客户端接线)。返回退订器(上游
  /// event -= 的等价;置空移除)
  /// LobbyInfoChanged (the L179-183 event face; UnitOrders' SyncLobby*
  /// branches fire it through Game.SyncLobbyInfo — the batch-16 client
  /// wiring). Returns the unsubscriber (the event -= equivalent; removal by
  /// nulling out).
  std::function<void()> AddLobbyInfoChangedHandler(
      std::function<void()> fn_changed) {
    vec_lobby_info_changed_.push_back(std::move(fn_changed));
    const std::size_t sz_slot = vec_lobby_info_changed_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_lobby_info_changed_.size())
        vec_lobby_info_changed_[sz_slot] = nullptr;
    };
  }

  /// AfterGameStart(L186 事件面;StartGame L260 触发 —— PerfDebugLogic 的
  /// FPS 计时重置消费)。返回退订器(上游 event -= 的等价;置空移除)
  /// AfterGameStart (the L186 event face; fired at StartGame L260 — consumed
  /// by PerfDebugLogic's FPS-timing reset). Returns the unsubscriber (the
  /// event -= equivalent; removal by nulling out).
  std::function<void()> AddAfterGameStartHandler(std::function<void()> fn_handler) {
    vec_after_game_start_.push_back(std::move(fn_handler));
    const std::size_t sz_slot = vec_after_game_start_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_after_game_start_.size())
        vec_after_game_start_[sz_slot] = nullptr;
    };
  }

  std::function<void()> AddBeforeGameStartHandler(std::function<void()> fn_handler) {
    vec_before_game_start_.push_back(std::move(fn_handler));
    const std::size_t sz_slot = vec_before_game_start_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_before_game_start_.size())
        vec_before_game_start_[sz_slot] = nullptr;
    };
  }

  std::function<void()> AddShellmapLoadedHandler(std::function<void()> fn_handler) {
    vec_on_shellmap_loaded_.push_back(std::move(fn_handler));
    const std::size_t sz_slot = vec_on_shellmap_loaded_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_on_shellmap_loaded_.size())
        vec_on_shellmap_loaded_[sz_slot] = nullptr;
    };
  }

  std::function<void()> AddOnRemoteDirectConnectHandler(
      std::function<void(const net::ConnectionTarget&)> fn_handler) {
    vec_on_remote_direct_connect_.push_back(std::move(fn_handler));
    const std::size_t sz_slot = vec_on_remote_direct_connect_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_on_remote_direct_connect_.size())
        vec_on_remote_direct_connect_[sz_slot] = nullptr;
    };
  }

  void RemoteDirectConnect(const net::ConnectionTarget& target_endpoint);

  void SetWorldRenderer(std::unique_ptr<gfx::WorldRenderer> up_wr, gfx::Viewport* ptr_viewport);
  gfx::WorldRenderer* WorldRendererFace() const { return up_world_renderer_.get(); }

  // ———— StartGame 装配(Game.cs L187-261;Phase 5 第一批)————
  /// ModData 的 MapCache 面(上游 ModData.MapCache;C++ 分层下由 Game 持有
  /// —— 所有权偏离随 D105 族登记)
  map::MapCache& MapCacheFace();

  /// StartGame(uid, WorldType)(L187-194)
  void StartGame(const std::string& str_uid, sim::WorldType type);
  /// StartGame(Map, WorldType)(L196-261)
  void StartGame(map::Map& map_world, sim::WorldType type);

  ModData* ModDataFace() const {
    return up_mod_data_ != nullptr ? up_mod_data_.get() : deps_.ptr_mod_data;
  }

  gfx::CursorManager* CursorFace() const {
    return up_cursor_ != nullptr ? up_cursor_.get() : deps_.ptr_cursor;
  }

  ILoadScreen* LoadScreenFace() const { return up_load_screen_.get(); }

  bool InitializeMod(const Manifest& manifest, const core::Arguments& args);

  void LoadShellMap();

  void LoadMap(const std::string& str_launch_map);

  void CreateAndStartLocalServer(const std::string& str_map_uid,
                                 std::vector<net::Order> vec_setup_orders);

 private:
  /// World.IsLoadingGameSave(World.cs L116:NetFrameNumber <= GameSaveLastFrame;
  /// 以世界自属 OM 求值)
  /// World.IsLoadingGameSave (World.cs L116) evaluated over the world's own
  /// OM.
  bool WorldIsLoadingGameSave(const sim::World& world_world) const;

  std::string ChooseShellmap();

  Deps deps_;
  core::ActionQueue delayed_actions_;
  MersenneTwister mt_cosmetic_{0};  // 种子于 ctor 按时钟重设 | re-seeded in the ctor

  std::vector<std::function<void()>> vec_lobby_info_changed_;
  std::vector<std::function<void()>> vec_after_game_start_;
  std::vector<std::function<void()>> vec_before_game_start_;
  std::vector<std::function<void()>> vec_on_shellmap_loaded_;
  std::vector<std::function<void(const net::ConnectionTarget&)>>
      vec_on_remote_direct_connect_;

  InitializeModDeps deps_init_;

  std::unique_ptr<net::OrderManager> up_order_manager_;
  std::unique_ptr<gfx::WorldRenderer> up_world_renderer_;
  gfx::Viewport* ptr_viewport_ = nullptr;  // 非拥有(WorldRenderer 配套) | non-owning
  std::unique_ptr<map::MapCache> up_map_cache_;
  std::unique_ptr<map::Map> up_map_;
  std::unique_ptr<sim::World> up_world_;
  std::unique_ptr<ModData> up_mod_data_;
  std::unique_ptr<gfx::CursorManager> up_cursor_;
  std::unique_ptr<ILoadScreen> up_load_screen_;

  std::int32_t int4_render_frame_ = 0;
  Modifiers kind_modifiers_ = Modifiers::None;
  net::ConnectionState last_connection_state_ = net::ConnectionState::PreConnecting;  // L140
  bool b_hide_cursor_ = false;
  RunStatus state_ = RunStatus::Running;  // L598

  std::chrono::steady_clock::time_point tp_started_;  // Stopwatch.StartNew 等价
};

}  // namespace ora::game
