import std;

#include "game/game.hpp"
#include "game/graphics_boot.hpp"
#include "game/perf_history.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/cursor_manager.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "game/fonts.hpp"
#include "game/local_player_profile.hpp"
#include "game/mod_data.hpp"
#include "map/map_cache.hpp"
#include "net/replay_recorder.hpp"
#include "net/session.hpp"
#include "net/text_notifications.hpp"
#include "net/unit_orders.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/default_input_handler.hpp"
#include "ui/widget_loader.hpp"

namespace ora::game {

Game::Game(Deps deps_deps)
    : deps_(std::move(deps_deps)),
      mt_cosmetic_{static_cast<std::int32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count())},
      tp_started_(std::chrono::steady_clock::now()) {

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

  net::TextNotificationsManager::Instance().Clear();
  net::ClearUnitOrdersStatics();

  if (up_order_manager_ != nullptr) {
    const bool b_keep = up_order_manager_->World() != nullptr &&
                        up_order_manager_->World()->Type() == sim::WorldType::Shellmap;

    if (!b_keep)
      up_order_manager_.reset();
  }

  up_order_manager_ = std::move(up_om);

  net::OrderManager* om = up_order_manager_.get();
  om->SetOrderProcessor(
      [](net::OrderManager& om_target, sim::World* world_target,
         int client_id, const net::Order& order) {
        net::ProcessOrder(om_target, world_target, client_id, order);
      });

  net::SetLobbyInfoChangedHandler([this]() {
    for (auto& fn : vec_lobby_info_changed_)
      if (fn)
        fn();
  });
  net::SetRuntimeMsFn([this] { return RunTime(); });

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
    if (ModDataFace() == nullptr)
      throw std::runtime_error("NullReferenceException");
    up_map_cache_ = std::make_unique<map::MapCache>(
        ModDataFace()->ManifestRef(), ModDataFace()->ModFiles());
  }
  return *up_map_cache_;
}

void Game::StartGame(const std::string& str_uid, sim::WorldType type) {

  map::MapPreview& preview = MapCacheFace().At(str_uid);
  if (preview.Status() != map::MapStatus::Available)
    throw std::runtime_error("Invalid map uid: " + str_uid);

  std::unique_ptr<map::Map> map_world = preview.ToMap();
  StartGame(*map_world, type);

  up_map_ = std::move(map_world);
}

void Game::StartGame(map::Map& map_world, sim::WorldType type) {
  if (ModDataFace() == nullptr || up_order_manager_ == nullptr)
    throw std::runtime_error("NullReferenceException");

  up_world_renderer_.reset();
  up_world_.reset();

  if (CursorFace() != nullptr)
    CursorFace()->SetCursor(nullptr);

  for (auto& fn : vec_before_game_start_)
    if (fn)
      fn();

  if (deps_.fn_prepare_map != nullptr)
    deps_.fn_prepare_map(map_world);
  sim::RegisterWorldTraits();

  auto world = std::make_unique<sim::World>(map_world, *ModDataFace(),
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

  if (deps_.fn_create_world_renderer != nullptr)
    up_world_renderer_ = deps_.fn_create_world_renderer(*up_world_);

  up_world_->LoadComplete(up_world_renderer_.get());

  if (om->GameStarted())
    return;

  ui::Ui& ui_face = deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();
  ui_face.SetMouseFocusWidget(nullptr);
  ui_face.SetKeyboardFocusWidget(nullptr);

  om->StartGame();
  if (up_world_renderer_ != nullptr)
    up_world_renderer_->RefreshPalette(
        std::span<gfx::IPaletteModifier* const>{});
  if (CursorFace() != nullptr) {
    const ui::ChromeMetrics* ptr_metrics = ui_face.ChromeMetricsDeps();
    if (ptr_metrics != nullptr)
      CursorFace()->SetCursor(&ptr_metrics->Get("DefaultCursor"));
  }
  up_world_->PostLoadComplete(up_world_renderer_.get());

  for (auto& fn : vec_after_game_start_)
    if (fn)
      fn();
}

void Game::JoinLocal() {
  JoinInner(std::make_unique<net::OrderManager>(
      std::make_unique<net::EchoConnection>()));

  net::SessionClient client_spectator;
  client_spectator.Index = up_order_manager_->Connection().LocalClientId();
  client_spectator.Name = deps_.str_player_name;
  core::Color color_player = core::Color::FromArgbRaw(0xFFC82020);
  if (SettingsModule* ptr_player = Settings::Instance().Module("Player")) {
    if (const std::optional<std::size_t> opt_slot =
            ptr_player->SlotIndexOf("Color"))
      color_player = core::Color::FromArgbRaw(
          static_cast<std::uint32_t>(ptr_player->GetInt(*opt_slot)));
  }
  client_spectator.PreferredColor = color_player;
  client_spectator.Color = color_player;
  client_spectator.Faction = "Random";
  client_spectator.SpawnPoint = 0;
  client_spectator.Team = 0;
  client_spectator.State = net::ClientState::Ready;
  up_order_manager_->LobbyInfo().vec_clients.push_back(client_spectator);
}

void Game::RemoteDirectConnect(const net::ConnectionTarget& target_endpoint) {
  for (auto& fn : vec_on_remote_direct_connect_)
    if (fn)
      fn(target_endpoint);
}

bool Game::InitializeMod(const Manifest& manifest,
                         const core::Arguments& args) {
  vec_lobby_info_changed_.clear();
  deps_.fn_connection_state_changed = nullptr;
  vec_before_game_start_.clear();
  vec_on_remote_direct_connect_.clear();
  delayed_actions_.Clear();

  ui::Ui& ui_face = deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();
  ui_face.ResetAll();

  up_world_renderer_.reset();
  up_world_.reset();
  up_map_.reset();
  up_map_cache_.reset();
  if (deps_init_.fn_shutdown_server != nullptr)
    deps_init_.fn_shutdown_server();
  up_order_manager_.reset();

  up_mod_data_.reset();
  up_cursor_.reset();
  up_load_screen_.reset();

  std::println("Loading mod: {}", manifest.Id());

  if (deps_.ptr_sound != nullptr)
    deps_.ptr_sound->StopVideo();

  if (deps_init_.fn_create_mod_data != nullptr) {
    up_mod_data_ = deps_init_.fn_create_mod_data(manifest);
    deps_.ptr_mod_data = up_mod_data_.get();
  }
  if (deps_init_.fn_reset_local_player_profile != nullptr)
    deps_init_.fn_reset_local_player_profile();

  if (ModDataFace() == nullptr)
    throw std::runtime_error("NullReferenceException");

  up_widget_loader_ = std::make_unique<ui::WidgetLoader>(
      manifest.ChromeLayout(), ModDataFace()->ModFiles());
  ui_face.WireModData(
      ModDataFace(),
      [this](ui::WidgetArgs& args_load, ui::Widget* ptr_parent,
             const std::string& str_id) -> ui::Widget* {
        return up_widget_loader_->LoadWidget(args_load, ptr_parent, str_id);
      },
      [this](sim::World& world_load, const std::string& str_id_load,
             ui::Widget* ptr_parent_load,
             ui::WidgetArgs& args_load) -> ui::Widget* {
        return LoadWidget(world_load, str_id_load, ptr_parent_load,
                          std::move(args_load));
      });

  ModDataFace()->InitializeLoaders([this](const std::string& str_path) {
    return std::optional<std::vector<char>>(
        ModDataFace()->ModFiles().Open(str_path));
  });

  if (deps_init_.fn_create_load_screen != nullptr) {
    up_load_screen_ = deps_init_.fn_create_load_screen(manifest);
    if (up_load_screen_ != nullptr) {
      up_load_screen_->Init(manifest, ModDataFace()->ModFiles());
      up_load_screen_->Display();
    }
  }

  if (up_load_screen_ != nullptr &&
      !up_load_screen_->BeforeLoad(*ModDataFace()))
    return false;

  ModDataFace()->InitializeLoaders([this](const std::string& str_path) {
    return std::optional<std::vector<char>>(
        ModDataFace()->ModFiles().Open(str_path));
  });

  if (deps_.ptr_renderer != nullptr && deps_init_.fn_create_font != nullptr) {
    Fonts& fonts_mod = ModDataFace()->GetOrCreateFonts();
    std::vector<gfx::Renderer::FontDesc> vec_descs;
    vec_descs.reserve(fonts_mod.vec_font_list.size());
    for (const auto& [str_key, rec_font] : fonts_mod.vec_font_list)
      vec_descs.push_back(gfx::Renderer::FontDesc{
          str_key, rec_font.str_font, rec_font.int4_size,
          rec_font.int4_ascender});
    deps_.ptr_renderer->InitializeFonts(
        vec_descs,
        ModDataFace()->ManifestRef().RendererConstantsRef().int4_fontSheetSize,
        deps_.ptr_renderer->EffectiveWindowScale(),
        [this](const std::string& str_file) {
          const std::vector<char> vec_bytes =
              ModDataFace()->ModFiles().Open(str_file);
          return std::vector<std::uint8_t>(vec_bytes.begin(), vec_bytes.end());
        },
        deps_init_.fn_create_font);
  }

  ui_face.WireMapCache(&MapCacheFace());
  MapCacheFace().LoadMaps(*ModDataFace(), deps_init_.str_map_filter);

  if (deps_init_.fn_create_cursor != nullptr)
    up_cursor_ = deps_init_.fn_create_cursor(*ModDataFace());

  const std::string& str_window_title =
      ModDataFace()->ManifestRef().Metadata().str_windowTitle;
  if (!str_window_title.empty() && deps_.ptr_renderer != nullptr)
    deps_.ptr_renderer->window().SetWindowTitle(
        fluent::FluentProvider::Instance().GetMessage(str_window_title));
  for (const std::string_view sv_name : {"render", "batches", "render_world",
                                          "render_widgets", "render_flip",
                                          "terrain_lighting"})
    PerfHistory::Instance().Item(std::string{sv_name}).b_has_normal_tick =
        false;

  JoinLocal();

  if (up_load_screen_ != nullptr)
    up_load_screen_->StartGame(args);

  return true;
}

void Game::LoadShellMap() {
  const std::string str_shellmap = ChooseShellmap();
  StartGame(str_shellmap, sim::WorldType::Shellmap);
  for (auto& fn : vec_on_shellmap_loaded_)
    if (fn)
      fn();
}

ui::Widget* Game::OpenWindow(sim::World& world_game,
                             const std::string& str_widget) {
  ui::WidgetArgs args_window{};
  args_window.Add("world", &world_game);
  args_window.Add("orderManager", OrderManagerFace());
  args_window.Add("worldRenderer", WorldRendererFace());
  ui::Ui& ui_face =
      deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();
  return ui_face.OpenWindow(str_widget, std::move(args_window));
}

ui::Widget* Game::OpenWindow(const std::string& str_widget,
                             ui::WidgetArgs args_extra) {
  if (WorldRendererFace() == nullptr)
    throw std::runtime_error("NullReferenceException");
  args_extra.Add("world", &WorldRendererFace()->World());
  args_extra.Add("orderManager", OrderManagerFace());
  args_extra.Add("worldRenderer", WorldRendererFace());
  ui::Ui& ui_face =
      deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();
  return ui_face.OpenWindow(str_widget, std::move(args_extra));
}

ui::Widget* Game::LoadWidget(sim::World& world_game,
                             const std::string& str_id,
                             ui::Widget* ptr_parent, ui::WidgetArgs args_extra) {
  if (ModDataFace() == nullptr || up_widget_loader_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  args_extra.Add("modData", ModDataFace());
  args_extra.Add("world", &world_game);
  args_extra.Add("orderManager", OrderManagerFace());
  args_extra.Add("worldRenderer", WorldRendererFace());
  return up_widget_loader_->LoadWidget(args_extra, ptr_parent, str_id);
}

std::string Game::ChooseShellmap() {
  std::vector<const std::string*> vec_uids;
  for (const map::MapPreview* ptr_preview : MapCacheFace().Previews())
    if (ptr_preview->Status() == map::MapStatus::Available &&
        (static_cast<std::int32_t>(ptr_preview->Visibility()) &
         static_cast<std::int32_t>(map::MapVisibility::Shellmap)) != 0)
      vec_uids.push_back(&ptr_preview->Uid());
  if (vec_uids.empty())
    throw std::runtime_error("No valid shellmaps available");
  const std::int32_t int4_pick =
      mt_cosmetic_.Next(0, static_cast<std::int32_t>(vec_uids.size()));
  if (int4_pick < 0)
    throw std::runtime_error(
        "System.ArgumentOutOfRangeException: Index was out of range. Must be "
        "non-negative and less than the size of the collection. (Parameter "
        "'index')");
  return *vec_uids[static_cast<std::size_t>(int4_pick)];
}

void Game::LoadMap(const std::string& str_launch_map) {
  std::vector<net::Order> vec_orders;
  vec_orders.push_back(net::Order::Command("option gamespeed default"));
  vec_orders.push_back(
      net::Order::Command("state " +
                          std::to_string(static_cast<std::int32_t>(
                              net::ClientState::Ready))));

  const map::MapPreview* ptr_found = nullptr;
  for (const map::MapPreview* ptr_preview : MapCacheFace().Previews()) {
    const std::filesystem::path path_map{ptr_preview->Path()};
    if (ptr_preview->Uid() == str_launch_map ||
        path_map.filename().string() == str_launch_map) {
      ptr_found = ptr_preview;
      break;
    }
  }
  if (ptr_found == nullptr)
    throw std::invalid_argument("Could not find map '" + str_launch_map +
                                "'.");

  CreateAndStartLocalServer(ptr_found->Uid(), std::move(vec_orders));
}

void Game::CreateAndStartLocalServer(
    const std::string& str_map_uid, std::vector<net::Order> vec_setup_orders) {
  auto up_orders =
      std::make_shared<std::vector<net::Order>>(std::move(vec_setup_orders));
  auto up_unsub = std::make_shared<std::function<void()>>();
  *up_unsub = AddLobbyInfoChangedHandler([this, up_orders, up_unsub] {
    (*up_unsub)();
    if (up_order_manager_ != nullptr)
      for (const net::Order& order_setup : *up_orders)
        up_order_manager_->IssueOrder(order_setup);
  });

  if (deps_init_.fn_create_and_join_local_server != nullptr) {
    if (auto up_om = deps_init_.fn_create_and_join_local_server(str_map_uid);
        up_om != nullptr)
      JoinInner(std::move(up_om));
  }
}

bool Game::WorldIsLoadingGameSave(const sim::World& world_world) const {

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
    if (CursorFace() != nullptr)
      CursorFace()->Tick();
  }

  if (om_order.LastTickTime().ShouldAdvance(int8_tick)) {
    if (om_order.GameStarted() && om_order.LocalFrameNumber() == 0)
      PerfHistory::Instance().Reset();

    om_order.LastTickTime().AdvanceTickTime(int8_tick);

    if (deps_.ptr_sound != nullptr)
      deps_.ptr_sound->Tick();

    sim::RunUnsynced(true, ptr_world, [&om_order] { om_order.TickImmediate(); });

    if (ptr_world == nullptr) {
      if (om_order.GameStarted())
        PerfHistory::Instance().Reset();
      return;
    }

    if (om_order.TryTick()) {
      sim::RunUnsynced(true, ptr_world, [&] {
        if (deps_.fn_order_generator_tick != nullptr)
          deps_.fn_order_generator_tick(*ptr_world);
      });

      ptr_world->Tick();

      PerfHistory::Instance().Tick(!ptr_world->Paused());
    }

    if (om_order.LocalFrameNumber() > 0)
      sim::RunUnsynced(true, ptr_world, [ptr_world] { ptr_world->TickRender(); });
  }
}

void Game::LogicTick() {
  PerformDelayedActions();

  if (deps_.fn_connection_state != nullptr && up_order_manager_ != nullptr) {
    const std::optional<net::ConnectionState> kind_state = deps_.fn_connection_state();
    if (kind_state.has_value() && *kind_state != last_connection_state_) {
      last_connection_state_ = *kind_state;
      if (deps_.fn_connection_state_changed != nullptr)
        deps_.fn_connection_state_changed(*up_order_manager_);
    }
  }

  if (up_order_manager_ == nullptr)
    return;

  InnerLogicTick(*up_order_manager_);

  if (up_world_renderer_ != nullptr &&
      up_order_manager_->World() != &up_world_renderer_->World()) {
    sim::World& world_render = up_world_renderer_->World();

    if (world_render.OM() != nullptr)
      InnerLogicTick(*world_render.OM());
  }
}

void Game::RenderTick() {

  if (deps_.ptr_renderer == nullptr)
    return;

  ++int4_render_frame_;

  gfx::WorldRenderer* ptr_wr = up_world_renderer_.get();
  const bool b_world_visible =
      ptr_wr != nullptr && !WorldIsLoadingGameSave(ptr_wr->World());
  ui::Ui& ui_face = deps_.ptr_ui != nullptr ? *deps_.ptr_ui : ui::Ui::Instance();

  if (ptr_wr != nullptr)
    ptr_wr->BeginFrame();

  if (b_world_visible) {

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
    if (CursorFace() != nullptr)
      CursorFace()->SetCursor(nullptr);
  } else if (CursorFace() != nullptr) {

    static const std::string str_default_cursor = "default";
    const std::string* ptr_cursor_name =
        ui_face.Root()->GetCursorOuter(gfx::Viewport::int2_last_mouse_pos);
    CursorFace()->SetCursor(ptr_cursor_name != nullptr ? ptr_cursor_name
                                                       : &str_default_cursor);
    CursorFace()->Render(*deps_.ptr_renderer);
  }

  net::OrderManager* ptr_om_input = OrderManagerFace();
  if (ptr_om_input != nullptr && ptr_om_input->World() != nullptr) {
    ui::DefaultInputHandler handler_input(*this, ptr_om_input->World());
    deps_.ptr_renderer->EndFrame(&handler_input);
  } else {
    NullInputHandler handler_input_null{};
    deps_.ptr_renderer->EndFrame(&handler_input_null);
  }

  if (b_take_screenshot_) {
    b_take_screenshot_ = false;
    deps_.fn_take_screenshot_inner();
  }
}

void Game::Loop() {

  constexpr std::int32_t kMaxLogicTicksBehind = 250;

  constexpr std::int32_t kMinReplayFps = 10;

  std::int64_t int8_next_logic = RunTime();
  std::int64_t int8_next_render = RunTime();
  std::int64_t int8_forced_next_render = RunTime();
  bool b_render_before_next_tick = false;

  while (state_ == RunStatus::Running) {
    std::int32_t int4_logic_interval = ui::Ui::kTimestep;
    sim::World* ptr_logic_world =
        up_world_renderer_ != nullptr ? &up_world_renderer_->World() : nullptr;

    const bool b_is_replay =
        ptr_logic_world != nullptr && deps_.fn_is_replay != nullptr &&
        deps_.fn_is_replay(*ptr_logic_world);
    if (ptr_logic_world != nullptr && (!b_is_replay || ptr_logic_world->ReplayTimestep() != 0)) {

      int4_logic_interval =
          up_order_manager_ != nullptr && ptr_logic_world == up_order_manager_->World()
              ? up_order_manager_->SuggestedTimestep()
              : ptr_logic_world->Timestep();
    }

    std::int32_t int4_render_interval = int4_logic_interval;
    const GraphicsFramerateFace face_framerate =
        GraphicsFramerateFromSettings(Settings::Instance());
    if (!face_framerate.b_cap_framerate_to_game_fps) {
      const std::int32_t int4_max_framerate =
          face_framerate.b_cap_framerate
              ? std::clamp(face_framerate.int4_max_framerate, 1, 1000)
              : 1000;
      int4_render_interval = 1000 / int4_max_framerate;
    }

    if (up_order_manager_ != nullptr && up_order_manager_->World() != nullptr &&
        WorldIsLoadingGameSave(*up_order_manager_->World())) {
      int4_logic_interval = 1;
      int4_render_interval = 200;
    }

    const std::int64_t int8_now = RunTime();

    if (int8_now - int8_next_logic > kMaxLogicTicksBehind)
      int8_next_logic = int8_now;

    const std::int64_t int8_next_update = std::min(int8_next_logic, int8_next_render);
    if (int8_now >= int8_next_update) {
      const bool b_force_render =
          b_render_before_next_tick || int8_now >= int8_forced_next_render;

      if (int8_now >= int8_next_logic && !b_render_before_next_tick) {
        int8_next_logic += int4_logic_interval;

        LogicTick();

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

        const std::int64_t int8_max_render_interval =
            std::max<std::int64_t>(1000 / kMinReplayFps, int4_render_interval);
        int8_forced_next_render = int8_now + int8_max_render_interval;

        RenderTick();
        if (deps_.ptr_renderer != nullptr && deps_.ptr_renderer->InputQuitRequested())
          Exit();
        b_render_before_next_tick = false;
      }

      if (b_suspended && b_is_time_to_render) {
        int8_next_render = int8_now + int4_render_interval;

        if (deps_.fn_pump_suspended_input != nullptr)
          deps_.fn_pump_suspended_input();

        b_render_before_next_tick = false;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(int8_next_update - int8_now));
    }
  }
}

RunStatus Game::Run() {

  SanitizeMaxFramerate(Settings::Instance());

  try {
    Loop();
  } catch (...) {

    up_world_.reset();
    up_order_manager_.reset();
    throw;
  }

  up_world_.reset();
  up_order_manager_.reset();

  up_map_.reset();
  up_world_renderer_.reset();

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

}
