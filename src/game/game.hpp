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
}
namespace ora::map {
class MapCache;
class MapPreview;
}
namespace ora::sound {
class Sound;
}
namespace ora::net {
class ConnectionTarget;
}
namespace ora::sim {
class World;
}
namespace ora::ui {
class WidgetLoader;
}
namespace ora::game {
class ModData;
class LocalPlayerProfile;
}

namespace ora::game {

enum class RunStatus : std::int32_t {
  Error = -1,
  Success = 0,
  Running = std::numeric_limits<std::int32_t>::max(),
};

class Game {
 public:

  struct Deps {
    std::function<std::int64_t()> fn_clock;

    gfx::Renderer* ptr_renderer = nullptr;
    sound::Sound* ptr_sound = nullptr;
    gfx::CursorManager* ptr_cursor = nullptr;
    gfx::ChromeProvider* ptr_chrome_provider = nullptr;
    ui::Ui* ptr_ui = nullptr;

    std::function<std::optional<net::ConnectionState>()> fn_connection_state;
    std::function<void(net::OrderManager&)> fn_connection_state_changed;

    std::function<void(sim::World&)> fn_order_generator_tick;

    std::function<void()> fn_pump_suspended_input;

    std::function<bool(const sim::World&)> fn_is_replay;

    std::function<void()> fn_on_quit;

    std::string str_player_name = "New Player";

    game::ModData* ptr_mod_data = nullptr;

    std::function<std::unique_ptr<gfx::WorldRenderer>(sim::World&)>
        fn_create_world_renderer;

    std::function<void(map::Map&)> fn_prepare_map;

    std::function<void()> fn_take_screenshot_inner = [] {};
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

  std::int64_t RunTime() const;

  std::int32_t RenderFrame() const { return int4_render_frame_; }

  int NetFrameNumber() const;
  int LocalTick() const;

  net::OrderManager* OrderManagerFace() const { return up_order_manager_.get(); }

  MersenneTwister& CosmeticRandom() { return mt_cosmetic_; }

  bool HideCursor() const { return b_hide_cursor_; }
  void SetHideCursor(bool b_value) { b_hide_cursor_ = b_value; }

  Modifiers GetModifierKeys() const { return kind_modifiers_; }
  void HandleModifierKeys(Modifiers kind_mods) { kind_modifiers_ = kind_mods; }

  void RunAfterTick(std::function<void()> fn_a) { delayed_actions_.Add(std::move(fn_a), RunTime()); }
  void RunAfterDelay(std::int32_t int4_delay_ms, std::function<void()> fn_a) {
    delayed_actions_.Add(std::move(fn_a), RunTime() + int4_delay_ms);
  }
  void PerformDelayedActions() { delayed_actions_.PerformActions(RunTime()); }

  void JoinInner(std::unique_ptr<net::OrderManager> up_om);

  void JoinLocal();

  void JoinReplay(std::vector<std::uint8_t> vec_replay_bytes,
                  int int4_order_latency = 3);

  RunStatus State() const { return state_; }
  void Exit() { state_ = RunStatus::Success; }

  void InnerLogicTick(net::OrderManager& om_order);
  void LogicTick();
  void RenderTick();
  void Loop();
  RunStatus Run();

  bool IsCurrentWorld(const sim::World* ptr_world) const;

  std::function<void()> AddLobbyInfoChangedHandler(
      std::function<void()> fn_changed) {
    vec_lobby_info_changed_.push_back(std::move(fn_changed));
    const std::size_t sz_slot = vec_lobby_info_changed_.size() - 1;
    return [this, sz_slot] {
      if (sz_slot < vec_lobby_info_changed_.size())
        vec_lobby_info_changed_[sz_slot] = nullptr;
    };
  }

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

  map::MapCache& MapCacheFace();

  void StartGame(const std::string& str_uid, sim::WorldType type);

  void StartGame(map::Map& map_world, sim::WorldType type);

  ModData* ModDataFace() const {
    return up_mod_data_ != nullptr ? up_mod_data_.get() : deps_.ptr_mod_data;
  }

  gfx::CursorManager* CursorFace() const {
    return up_cursor_ != nullptr ? up_cursor_.get() : deps_.ptr_cursor;
  }

  ILoadScreen* LoadScreenFace() const { return up_load_screen_.get(); }

  ui::Widget* OpenWindow(sim::World& world_game, const std::string& str_widget);

  ui::Widget* OpenWindow(const std::string& str_widget, ui::WidgetArgs args_extra);

  ui::Widget* LoadWidget(sim::World& world_game, const std::string& str_id,
                         ui::Widget* ptr_parent, ui::WidgetArgs args_extra);

  void TakeScreenshot() { b_take_screenshot_ = true; }

  ui::WidgetLoader* WidgetLoaderFace() const {
    return up_widget_loader_.get();
  }

  bool InitializeMod(const Manifest& manifest, const core::Arguments& args);

  void LoadShellMap();

  void LoadMap(const std::string& str_launch_map);

  void CreateAndStartLocalServer(const std::string& str_map_uid,
                                 std::vector<net::Order> vec_setup_orders);

 private:

  bool WorldIsLoadingGameSave(const sim::World& world_world) const;

  std::string ChooseShellmap();

  Deps deps_;
  core::ActionQueue delayed_actions_;
  MersenneTwister mt_cosmetic_{0};

  std::vector<std::function<void()>> vec_lobby_info_changed_;
  std::vector<std::function<void()>> vec_after_game_start_;
  std::vector<std::function<void()>> vec_before_game_start_;
  std::vector<std::function<void()>> vec_on_shellmap_loaded_;
  std::vector<std::function<void(const net::ConnectionTarget&)>>
      vec_on_remote_direct_connect_;

  InitializeModDeps deps_init_;

  std::unique_ptr<net::OrderManager> up_order_manager_;
  std::unique_ptr<gfx::WorldRenderer> up_world_renderer_;
  gfx::Viewport* ptr_viewport_ = nullptr;
  std::unique_ptr<map::MapCache> up_map_cache_;
  std::unique_ptr<map::Map> up_map_;
  std::unique_ptr<sim::World> up_world_;
  std::unique_ptr<ModData> up_mod_data_;
  std::unique_ptr<ui::WidgetLoader> up_widget_loader_;
  std::unique_ptr<gfx::CursorManager> up_cursor_;
  std::unique_ptr<ILoadScreen> up_load_screen_;

  std::int32_t int4_render_frame_ = 0;
  bool b_take_screenshot_ = false;
  Modifiers kind_modifiers_ = Modifiers::None;
  net::ConnectionState last_connection_state_ = net::ConnectionState::PreConnecting;
  bool b_hide_cursor_ = false;
  RunStatus state_ = RunStatus::Running;

  std::chrono::steady_clock::time_point tp_started_;
};

}
