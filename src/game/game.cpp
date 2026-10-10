// UPSTREAM: OpenRA.Game/Game.cs @b6fc03f(game.hpp 的实现)
// The implementation of game.hpp.
import std;

#include "game/game.hpp"

#include "gfx/chrome_provider.hpp"
#include "gfx/cursor_manager.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "game/mod_data.hpp"
#include "map/map_cache.hpp"
#include "net/replay_recorder.hpp"
#include "net/session.hpp"
#include "net/unit_orders.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"

namespace ora::game {

Game::Game(Deps deps_deps)
    : deps_(std::move(deps_deps)),
      mt_cosmetic_{static_cast<std::int32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count())},
      tp_started_(std::chrono::steady_clock::now()) {
  // 缺省时钟 = 构造起 steady 毫秒(Stopwatch.StartNew 等价)
  // the default clock = steady milliseconds since construction (the
  // Stopwatch.StartNew equivalent).
  if (!deps_.fn_clock)
    deps_.fn_clock = [this] {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - tp_started_)
          .count();
    };
}

Game::~Game() = default;

std::int64_t Game::RunTime() const { return deps_.fn_clock(); }

int Game::NetFrameNumber() const {
  // 上游 OrderManager 空引用等价抛 | the upstream null-OM NRE equivalent
  if (up_order_manager_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  return up_order_manager_->NetFrameNumber();
}

int Game::LocalTick() const {
  if (up_order_manager_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  return up_order_manager_->LocalFrameNumber();
}

void Game::JoinInner(std::unique_ptr<net::OrderManager> up_om) {
  // Refresh static classes before the game starts.(上游注释;
  // TextNotificationsManager 未移植,UnitOrders 无静态态 —— 均 no-op)
  if (up_order_manager_ != nullptr) {
    const bool b_keep = up_order_manager_->World() != nullptr &&
                        up_order_manager_->World()->Type() == sim::WorldType::Shellmap;
    // shellmap OM 存活(上游 HACK 注释) | the shellmap OM survives (the
    // upstream HACK comment)
    if (!b_keep)
      up_order_manager_.reset();
  }

  up_order_manager_ = std::move(up_om);

  // UnitOrders.ProcessOrder 分发面(上游静态直调;C++ 分层下逐 OM 装配)
  net::OrderManager* om = up_order_manager_.get();
  om->SetOrderProcessor(
      [](net::OrderManager& om_target, sim::World* world_target,
         int client_id, const net::Order& order) {
        net::ProcessOrder(om_target, world_target, client_id, order);
      });

  // StartGame 分支的 Game.StartGame 装配面(上游静态直调 + MapCache 状态
  // 门槛;不可用地图走上游 Disconnect+LoadShellMap 的 no-op 等价)
  net::SetStartGameHandler(
      [this](net::OrderManager&, const std::string& str_uid) {
        if (MapCacheFace().At(str_uid).Status() != map::MapStatus::Available)
          return;
        StartGame(str_uid, sim::WorldType::Regular);
      });
}

void Game::JoinReplay(std::vector<std::uint8_t> vec_replay_bytes,
                      int int4_order_latency) {
  JoinInner(std::make_unique<net::OrderManager>(
      std::make_unique<net::ReplayConnection>(std::move(vec_replay_bytes),
                                              int4_order_latency)));
}

map::MapCache& Game::MapCacheFace() {
  if (up_map_cache_ == nullptr) {
    if (deps_.ptr_mod_data == nullptr)
      throw std::runtime_error("NullReferenceException");
    up_map_cache_ = std::make_unique<map::MapCache>(
        deps_.ptr_mod_data->ManifestRef(), deps_.ptr_mod_data->ModFiles());
  }
  return *up_map_cache_;
}

void Game::StartGame(const std::string& str_uid, sim::WorldType type) {
  // Game.cs L187-194
  map::MapPreview& preview = MapCacheFace().At(str_uid);
  if (preview.Status() != map::MapStatus::Available)
    throw std::runtime_error("Invalid map uid: " + str_uid);

  std::unique_ptr<map::Map> map_world = preview.ToMap();
  StartGame(*map_world, type);
  // Map 所有权归 Game(上游 map 生命周期由 MapCache/World 共管的 GC 形态;
  // C++ 以 Game 承载 —— COVERAGE 登记)
  up_map_ = std::move(map_world);
}

void Game::StartGame(map::Map& map_world, sim::WorldType type) {
  // Game.cs L196-261
  if (deps_.ptr_mod_data == nullptr || up_order_manager_ == nullptr)
    throw std::runtime_error("NullReferenceException");

  // Dispose of the old world before creating a new one.(上游注释)
  up_world_renderer_.reset();
  up_world_.reset();

  // Cursor.SetCursor(null)(L201;光标面为注入 no-op)
  // BeforeGameStart()(L202;事件面空)

  // ModData.PrepareMap(map)(L206):InitializeLoaders + Sequences.LoadSprites
  // + 音乐装载 —— 序列装载面经 Deps.fn_prepare_map 注入(Phase 6 资产链)
  if (deps_.fn_prepare_map != nullptr)
    deps_.fn_prepare_map(map_world);
  sim::RegisterWorldTraits();

  // Renderer.SetDepthMargin(L219;深度边距面随渲染装配批)
  auto world = std::make_unique<sim::World>(map_world, *deps_.ptr_mod_data,
                                            *up_order_manager_, type);
  net::OrderManager* om = up_order_manager_.get();
  world->SetOrderIssueSink([om](net::Order* o) { om->IssueOrder(*o); });
  sim::World* ptr_world = world.get();
  world->SetIsReplayResolver(
      [om] { return om->Connection().IsReplay(); });
  om->SetOutOfSyncHandler(
      [ptr_world](int) { ptr_world->OutOfSync(); });
  up_world_ = std::move(world);
  om->SetWorld(up_world_.get());

  // worldRenderer = new WorldRenderer(...)(L225;渲染面注入钩子)
  if (deps_.fn_create_world_renderer != nullptr)
    up_world_renderer_ = deps_.fn_create_world_renderer(*up_world_);

  // OrderManager.World.GameOver += FinishBenchmark(L223;嵌入侧)

  up_world_->LoadComplete(up_world_renderer_.get());

  if (om->GameStarted())
    return;

  // Ui.MouseFocusWidget/KeyboardFocusWidget = null(L239-240)
  om->StartGame();
  // worldRenderer.RefreshPalette()(L243)
  // Cursor.SetCursor(ChromeMetrics.Get<string>("DefaultCursor"))(L244)
  up_world_->PostLoadComplete(up_world_renderer_.get());
  // AfterGameStart()(L260;事件面空)
}

void Game::JoinLocal() {
  JoinInner(std::make_unique<net::OrderManager>(
      std::make_unique<net::EchoConnection>()));

  // Add a spectator client for the local player(上游注释;色面随 Phase 5)
  net::SessionClient client_spectator;
  client_spectator.Index = up_order_manager_->Connection().LocalClientId();
  client_spectator.Name = deps_.str_player_name;
  client_spectator.Faction = "Random";
  client_spectator.SpawnPoint = 0;
  client_spectator.Team = 0;
  client_spectator.State = net::ClientState::Ready;
  up_order_manager_->LobbyInfo().vec_clients.push_back(client_spectator);
}

bool Game::WorldIsLoadingGameSave(const sim::World& world_world) const {
  // World.cs L116(经世界自属 OM 求值) | World.cs L116 (over the world's
  // own OM)
  const net::OrderManager* ptr_om = world_world.OM();
  return ptr_om != nullptr &&
         ptr_om->NetFrameNumber() <= ptr_om->GameSaveLastFrame();
}

void Game::InnerLogicTick(net::OrderManager& om_order) {
  const std::int64_t int8_tick = RunTime();

  sim::World* ptr_world = om_order.World();
  ui::Ui& ui_face = deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();

  if (ui_face.LastTickTime().ShouldAdvance(int8_tick)) {
    ui_face.LastTickTime().AdvanceTickTime(int8_tick);
    sim::RunUnsynced(true, ptr_world, [&ui_face] { ui_face.Tick(); });
    if (deps_.ptr_cursor != nullptr)
      deps_.ptr_cursor->Tick();
  }

  if (om_order.LastTickTime().ShouldAdvance(int8_tick)) {
    // PerfHistory.Reset 桩(GameStarted && LocalFrameNumber == 0;Phase 5)
    // the PerfHistory.Reset stub (Phase 5).

    // PerfSample("tick_time") 域 | the PerfSample("tick_time") scope
    om_order.LastTickTime().AdvanceTickTime(int8_tick);

    if (deps_.ptr_sound != nullptr)
      deps_.ptr_sound->Tick();

    sim::RunUnsynced(true, ptr_world, [&om_order] { om_order.TickImmediate(); });

    if (ptr_world == nullptr)
      return;  // PerfHistory.Reset 桩 | the PerfHistory.Reset stub

    if (om_order.TryTick()) {
      sim::RunUnsynced(true, ptr_world, [&] {
        if (deps_.fn_order_generator_tick != nullptr)
          deps_.fn_order_generator_tick(*ptr_world);
      });

      ptr_world->Tick();

      // PerfHistory.Tick(!world.Paused) 桩 | the PerfHistory.Tick stub
    }

    // Wait until we have done our first world Tick before TickRendering(上游注释)
    if (om_order.LocalFrameNumber() > 0)
      sim::RunUnsynced(true, ptr_world, [ptr_world] { ptr_world->TickRender(); });
  }
}

void Game::LogicTick() {
  PerformDelayedActions();

  // 上游 `Connection is NetworkConnection` 模式匹配 —— EchoConnection 恒
  // 跳过;连接态观测经 fn_connection_state(Phase 7)
  if (deps_.fn_connection_state != nullptr && up_order_manager_ != nullptr) {
    const std::optional<net::ConnectionState> kind_state = deps_.fn_connection_state();
    if (kind_state.has_value() && *kind_state != last_connection_state_) {
      last_connection_state_ = *kind_state;
      if (deps_.fn_connection_state_changed != nullptr)
        deps_.fn_connection_state_changed(*up_order_manager_);
    }
  }

  if (up_order_manager_ == nullptr)
    return;  // 上游 NRE 等价点前的护栏(测试注入序) | a guard before the
             // upstream NRE point (test assembly order)

  InnerLogicTick(*up_order_manager_);

  if (up_world_renderer_ != nullptr &&
      up_order_manager_->World() != &up_world_renderer_->World()) {
    sim::World& world_render = up_world_renderer_->World();
    // 上游直取 worldRenderer.World.OrderManager(无空判);C++ OM 可缺,
    // 缺则跳过(shellmap 前装配窗口)
    // upstream reads worldRenderer.World.OrderManager unchecked; the C++ OM
    // may be absent during the pre-shellmap assembly window.
    if (world_render.OM() != nullptr)
      InnerLogicTick(*world_render.OM());
  }
}

void Game::RenderTick() {
  // 无头面(测试注入):上游恒有 Renderer | the headless face (test
  // injection): upstream always has a Renderer
  if (deps_.ptr_renderer == nullptr)
    return;

  ++int4_render_frame_;

  gfx::WorldRenderer* ptr_wr = up_world_renderer_.get();
  const bool b_world_visible =
      ptr_wr != nullptr && !WorldIsLoadingGameSave(ptr_wr->World());
  ui::Ui& ui_face = deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();

  // Prepare renderables (i.e. render voxels) before calling BeginFrame(上游注释)
  if (ptr_wr != nullptr)
    ptr_wr->BeginFrame();

  if (b_world_visible) {
    // 上游 worldRenderer.Viewport.Tick();Viewport 随 Deps 配套注入
    if (ptr_viewport_ != nullptr)
      ptr_viewport_->Tick();
    ptr_wr->PrepareRenderables(std::span<gfx::IPaletteModifier* const>{});
  }

  ui_face.PrepareRenderables();

  if (ptr_wr != nullptr)
    ptr_wr->EndFrame();

  if (b_world_visible && ptr_viewport_ != nullptr) {
    deps_.ptr_renderer->BeginWorld(ptr_viewport_->CenterLocation(),
                                   ptr_viewport_->ViewportSize());
    if (deps_.ptr_sound != nullptr)
      deps_.ptr_sound->SetListenerPosition(ptr_viewport_->CenterPosition());
    ptr_wr->Draw();
  }

  deps_.ptr_renderer->BeginUI();

  if (b_world_visible)
    ptr_wr->DrawAnnotations();

  ui_face.Draw();

  if (b_hide_cursor_) {
    if (deps_.ptr_cursor != nullptr)
      deps_.ptr_cursor->SetCursor(nullptr);
  } else if (deps_.ptr_cursor != nullptr) {
    // 上游 `?? "default"` | the upstream ?? "default"
    static const std::string str_default_cursor = "default";
    const std::string* ptr_cursor_name =
        ui_face.Root()->GetCursorOuter(gfx::Viewport::int2_last_mouse_pos);
    deps_.ptr_cursor->SetCursor(ptr_cursor_name != nullptr ? ptr_cursor_name
                                                           : &str_default_cursor);
    deps_.ptr_cursor->Render(*deps_.ptr_renderer);
  }

  // 上游 EndFrame(new DefaultInputHandler(OM.World));输入面随 Phase 7 接线
  deps_.ptr_renderer->EndFrame();

  // takeScreenshot/PerfHistory 计面桩(Phase 5) | the takeScreenshot/
  // PerfHistory stubs (Phase 5)
}

void Game::Loop() {
  // When the logic has fallen behind by this much, skip the pending
  // updates and start fresh.(上游注释)
  constexpr std::int32_t kMaxLogicTicksBehind = 250;

  // Try to maintain at least this many FPS during replays(上游注释)
  constexpr std::int32_t kMinReplayFps = 10;

  std::int64_t int8_next_logic = RunTime();
  std::int64_t int8_next_render = RunTime();
  std::int64_t int8_forced_next_render = RunTime();
  bool b_render_before_next_tick = false;

  while (state_ == RunStatus::Running) {
    std::int32_t int4_logic_interval = ui::Ui::kTimestep;
    sim::World* ptr_logic_world =
        up_world_renderer_ != nullptr ? &up_world_renderer_->World() : nullptr;

    // ReplayTimestep = 0 means the replay is paused(上游注释)
    const bool b_is_replay =
        ptr_logic_world != nullptr && deps_.fn_is_replay != nullptr &&
        deps_.fn_is_replay(*ptr_logic_world);
    if (ptr_logic_world != nullptr && (!b_is_replay || ptr_logic_world->ReplayTimestep() != 0)) {
      // 上游三元左支要求 OrderManager 非空(NRE);C++ 缺 OM 走世界步长
      // the upstream left arm requires a non-null OM (NRE); a missing C++ OM
      // falls to the world timestep
      int4_logic_interval =
          up_order_manager_ != nullptr && ptr_logic_world == up_order_manager_->World()
              ? up_order_manager_->SuggestedTimestep()
              : ptr_logic_world->Timestep();
    }

    // Ideal time between screen updates(上游注释)
    std::int32_t int4_render_interval = int4_logic_interval;
    if (!deps_.b_cap_framerate_to_game_fps) {
      const std::int32_t int4_max_framerate =
          deps_.b_cap_framerate ? std::clamp(deps_.int4_max_framerate, 1, 1000) : 1000;
      int4_render_interval = 1000 / int4_max_framerate;
    }

    // Tick as fast as possible while restoring game saves(上游注释)
    if (up_order_manager_ != nullptr && up_order_manager_->World() != nullptr &&
        WorldIsLoadingGameSave(*up_order_manager_->World())) {
      int4_logic_interval = 1;
      int4_render_interval = 200;
    }

    const std::int64_t int8_now = RunTime();

    // If the logic has fallen behind too much, skip it and catch up(上游注释)
    if (int8_now - int8_next_logic > kMaxLogicTicksBehind)
      int8_next_logic = int8_now;

    const std::int64_t int8_next_update = std::min(int8_next_logic, int8_next_render);
    if (int8_now >= int8_next_update) {
      const bool b_force_render =
          b_render_before_next_tick || int8_now >= int8_forced_next_render;

      if (int8_now >= int8_next_logic && !b_render_before_next_tick) {
        int8_next_logic += int4_logic_interval;

        LogicTick();

        // Force at least one render per tick during regular gameplay(上游注释)
        if (up_order_manager_ != nullptr && up_order_manager_->World() != nullptr &&
            !WorldIsLoadingGameSave(*up_order_manager_->World()) &&
            !(deps_.fn_is_replay != nullptr && deps_.fn_is_replay(*up_order_manager_->World())))
          b_render_before_next_tick = true;
      }

      const bool b_have_time_until_next_logic = int8_now < int8_next_logic;
      const bool b_is_time_to_render = int8_now >= int8_next_render;
      const bool b_suspended = deps_.ptr_renderer != nullptr &&
                               deps_.ptr_renderer->window().IsSuspended();
      if (!b_suspended && ((b_is_time_to_render && b_have_time_until_next_logic) || b_force_render)) {
        int8_next_render = int8_now + int4_render_interval;

        // Pick the minimum allowed FPS(上游注释,含渲染耗时并入区间)
        const std::int64_t int8_max_render_interval =
            std::max<std::int64_t>(1000 / kMinReplayFps, int4_render_interval);
        int8_forced_next_render = int8_now + int8_max_render_interval;

        RenderTick();
        b_render_before_next_tick = false;
      }

      // Simulate a render tick if it was time to render but we skip actually
      // rendering(上游注释)
      if (b_suspended && b_is_time_to_render) {
        int8_next_render = int8_now + int4_render_interval;

        // Still process SDL events to allow a restore to come through(上游注释)
        if (deps_.fn_pump_suspended_input != nullptr)
          deps_.fn_pump_suspended_input();

        // Ensure that we still logic tick despite not rendering(上游注释)
        b_render_before_next_tick = false;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(int8_next_update - int8_now));
    }
  }
}

RunStatus Game::Run() {
  // 上游 MaxFramerate<1 的 Settings 修正随 Phase 5 实体;Loop 内 clamp 已保
  // 证 1..1000 域
  // the upstream MaxFramerate<1 Settings fix lands with the Phase 5 entity;
  // Loop's clamp already guarantees the 1..1000 domain.
  try {
    Loop();
  } catch (...) {
    // finally 语义:异常路径同样清理 | the finally semantics: the same
    // cleanup on the exception path
    up_world_.reset();
    up_order_manager_.reset();
    throw;
  }

  // Ensure that the active replay is properly saved(上游注释)
  up_world_.reset();
  up_order_manager_.reset();

  up_map_.reset();
  up_world_renderer_.reset();
  // ModData.Dispose / Sound.Dispose / Renderer.Dispose:嵌入侧所有权(C++
  // RAII;Game 不持有)
  if (deps_.ptr_chrome_provider != nullptr)
    deps_.ptr_chrome_provider->Deinitialize();

  if (deps_.fn_on_quit != nullptr)
    deps_.fn_on_quit();

  return state_;
}

bool Game::IsCurrentWorld(const sim::World* ptr_world) const {
  return up_order_manager_ != nullptr && up_order_manager_->World() == ptr_world &&
         ptr_world != nullptr && !ptr_world->Disposing();
}

void Game::SetWorldRenderer(std::unique_ptr<gfx::WorldRenderer> up_wr,
                            gfx::Viewport* ptr_viewport) {
  up_world_renderer_ = std::move(up_wr);
  ptr_viewport_ = ptr_viewport;
}

}  // namespace ora::game
