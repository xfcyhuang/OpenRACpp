// UPSTREAM: OpenRA.Game/Game.cs @b6fc03f + OpenRA.Game/Support/Program.cs
// L14-19(主循环骨架逐语义:RunTime 时钟/延迟动作队列/JoinInner·JoinLocal/
// InnerLogicTick 的双 TickTime 节拍/LogicTick 的连接态事件/RenderTick 的
// prepare→world→UI→flip 全序/Loop 的逻辑-渲染双时间表与追帧截断/Run 的
// finally 清理)。Phase 5-8 依赖面 → Deps/钩子注入:Renderer/Sound/
// CursorManager/ChromeProvider/WorldRenderer+Viewport/Ui、Settings.Graphics
// 值面、NetworkConnection 的连接态观测(Phase 7)、world.OrderGenerator
// (Phase 5)、PerfHistory/PerfSample/Benchmark/截图(Phase 5+)、
// TextNotificationsManager/UnitOrders.Clear(前者未移植,后者无静态态)
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
#include "core/mersenne_twister.hpp"
#include "net/connection.hpp"
#include "net/order_manager.hpp"
#include "platform/sdl2_input.hpp"
#include "ui/ui.hpp"

namespace ora::gfx {
class Renderer;
class CursorManager;
class ChromeProvider;
class WorldRenderer;
class Viewport;
struct IPaletteModifier;
}  // namespace ora::gfx
namespace ora::sound {
class Sound;
}
namespace ora::sim {
class World;
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

    // Settings.Graphics 值面(Settings.cs L256-262 默认;Phase 5 换实体)
    // the Settings.Graphics value face (Settings.cs L256-262 defaults).
    bool b_cap_framerate = false;
    std::int32_t int4_max_framerate = 60;
    bool b_cap_framerate_to_game_fps = false;

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
  };

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

  void SetWorldRenderer(std::unique_ptr<gfx::WorldRenderer> up_wr, gfx::Viewport* ptr_viewport);
  gfx::WorldRenderer* WorldRendererFace() const { return up_world_renderer_.get(); }

 private:
  /// World.IsLoadingGameSave(World.cs L116:NetFrameNumber <= GameSaveLastFrame;
  /// 以世界自属 OM 求值)
  /// World.IsLoadingGameSave (World.cs L116) evaluated over the world's own
  /// OM.
  bool WorldIsLoadingGameSave(const sim::World& world_world) const;

  Deps deps_;
  core::ActionQueue delayed_actions_;
  MersenneTwister mt_cosmetic_{0};  // 种子于 ctor 按时钟重设 | re-seeded in the ctor

  std::unique_ptr<net::OrderManager> up_order_manager_;
  std::unique_ptr<gfx::WorldRenderer> up_world_renderer_;
  gfx::Viewport* ptr_viewport_ = nullptr;  // 非拥有(WorldRenderer 配套) | non-owning

  std::int32_t int4_render_frame_ = 0;
  Modifiers kind_modifiers_ = Modifiers::None;
  net::ConnectionState last_connection_state_ = net::ConnectionState::PreConnecting;  // L140
  bool b_hide_cursor_ = false;
  RunStatus state_ = RunStatus::Running;  // L598

  std::chrono::steady_clock::time_point tp_started_;  // Stopwatch.StartNew 等价
};

}  // namespace ora::game
