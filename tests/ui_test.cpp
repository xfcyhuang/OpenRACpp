// 第十五批验收:ActionQueue/WidgetArgs/Mediator/Widget 树/Ui 窗口栈与输入/
// ChromeMetrics/WidgetLoader 异常矩阵 + Game 主循环骨架(时钟注入下的节拍/
// JoinLocal/延迟动作/Run 清理)。期望值人工对照上游源码(ActionQueue.cs/
// Widget.cs/ChromeMetrics.cs/WidgetLoader.cs/Game.cs)。
// Phase 6 第二批追加:Hotkey/HotkeyManager(ra 真热键表)、SpriteFont(fake
// 字体的度量/绘制/对比度权图)、Fonts mod 装载、基础 widget 族的 ra 真本体
// chrome.yaml 装载冒烟(THREEBUTTON_PROMPT/MULTIPLAYER_FILTER_PANEL/
// LABEL_DROPDOWN_TEMPLATE)+ 按钮三态输入 + ScrollPanel 布局/滚动面。
// The fifteenth-batch acceptance: ActionQueue/WidgetArgs/Mediator/the Widget
// tree/the Ui window stack and input/ChromeMetrics/the WidgetLoader
// exception matrix + the Game main-loop skeleton (pacing under an injected
// clock/JoinLocal/delayed actions/Run cleanup). Expected values hand-checked
// against the upstream sources. The Phase 6 batch-two append:
// Hotkey/HotkeyManager (the real ra hotkey tables), SpriteFont (the fake
// font's metrics/draw/contrast weight maps), the Fonts mod load, and the
// base widget family's chrome.yaml load smoke against the real mod
// (THREEBUTTON_PROMPT/MULTIPLAYER_FILTER_PANEL/LABEL_DROPDOWN_TEMPLATE) +
// the button input trio + the ScrollPanel layout/scroll faces.
import std;

#include "core/action_queue.hpp"
#include "fs/file_system.hpp"
#include "formats/png.hpp"
#include "game/fonts.hpp"
#include "game/game.hpp"
#include "game/mod_data.hpp"
#include "game/game_records.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/create_map_players.hpp"
#include "mods/viewport_controller.hpp"
#include "mods/world_interaction_controller.hpp"
#include "net/session.hpp"
#include "server/game_information.hpp"
#include "server/game_server.hpp"
#include "sim/trait_registry.hpp"

namespace ora::gen {
void RegisterGeneratedAll();
}  // namespace ora::gen
#include "gfx/sheet.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/viewport.hpp"
#include "gfx/chrome_provider.hpp"
#include "platform/ifont.hpp"
#include "sim/world.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/production_tabs_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "mods/radar_widget.hpp"
#include "mods/support_powers_widget.hpp"
#include "mods/support_power_timer_widget.hpp"
#include "mods/observer_widgets.hpp"
#include "mods/video_player_widget.hpp"
#include "formats/video.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/default_input_handler.hpp"
#include "ui/hotkey.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/client_tooltip_region_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/exponential_slider_widget.hpp"
#include "ui/widgets/generated_map_preview_widget.hpp"
#include "ui/widgets/hotkey_entry_widget.hpp"
#include "ui/widgets/label_for_input_widget.hpp"
#include "ui/widgets/label_with_highlight_widget.hpp"
#include "ui/widgets/logic_key_listener_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/menu_button_widget.hpp"
#include "ui/widgets/password_field_widget.hpp"
#include "ui/widgets/rgba_sprite_widget.hpp"
#include "ui/widgets/world_button_widget.hpp"
#include "ui/widgets/badge_widget.hpp"
#include "ui/widgets/control_groups_widget.hpp"
#include "ui/widgets/line_graph_widget.hpp"
#include "ui/widgets/mouse_attachment_widget.hpp"
#include "ui/widgets/perf_graph_widget.hpp"
#include "ui/widgets/resource_bar_widget.hpp"
#include "ui/widgets/scrollable_line_graph_widget.hpp"
#include "ui/widgets/text_notifications_display_widget.hpp"
#include "ui/player_database.hpp"
#include "game/perf_history.hpp"
#include "game/graphics_boot.hpp"
#include "game/settings.hpp"
#include "ui/widgets/hue_slider_widget.hpp"
#include "ui/widgets/color_mixer_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "gfx/world_renderer.hpp"
#include "ui/widgets/actor_preview_widget.hpp"
#include "ui/widgets/resource_preview_widget.hpp"
#include "ui/widgets/terrain_template_preview_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"
#include <cstdio>  // setvbuf(白名单头)| setvbuf (a whitelisted header)

namespace {

using namespace ora;

int int4_failures = 0;

void Check(bool b_cond, std::string_view sv_what) {
  if (!b_cond) {
    std::println("FAIL: {}", sv_what);
    int4_failures++;
  }
}

template <class Fn>
void CheckThrow(std::string_view sv_message, Fn&& fn_check) {
  try {
    fn_check();
    Check(false, std::format("expected throw: {}", sv_message));
  } catch (const std::exception& e) {
    Check(std::string_view(e.what()).find(sv_message) != std::string_view::npos,
          std::format("throw text [{}]", sv_message));
  }
}

struct TestCountNotification {};

// ———— ActionQueue ————

void TestActionQueue() {
  core::ActionQueue queue_actions;
  std::vector<int> vec_ran;

  queue_actions.Add([&vec_ran] { vec_ran.push_back(2); }, 150);
  queue_actions.Add([&vec_ran] { vec_ran.push_back(1); }, 100);
  queue_actions.Add([&vec_ran] { vec_ran.push_back(3); }, 100);

  queue_actions.PerformActions(99);
  Check(vec_ran.empty(), "PerformActions 早于全部时间不执行");

  queue_actions.PerformActions(100);
  Check(vec_ran.size() == 2 && vec_ran[0] == 1 && vec_ran[1] == 3, "同值时间按插入序执行");

  queue_actions.PerformActions(150);
  Check(vec_ran.size() == 3 && vec_ran[2] == 2, "时间升序执行");

  bool b_null_threw = false;
  try {
    queue_actions.Add(nullptr, 0);
  } catch (const std::exception&) {
    b_null_threw = true;
  }
  Check(b_null_threw, "Add(null) 抛出");

  // 执行期重入 Add 不死锁且不入本轮快照(上游锁内切片语义)
  core::ActionQueue queue_reentrant;
  bool b_reentrant_ran = false;
  queue_reentrant.Add(
      [&] { queue_reentrant.Add([&b_reentrant_ran] { b_reentrant_ran = true; }, 10); }, 10);
  queue_reentrant.PerformActions(10);
  Check(!b_reentrant_ran, "重入动作不入本轮");
  queue_reentrant.PerformActions(10);
  Check(b_reentrant_ran, "重入动作下轮执行");
}

// ———— WidgetArgs ————

void TestWidgetArgs() {
  ui::WidgetArgs args_args;
  args_args.Add("a", std::int32_t{1});
  args_args.Set("b", std::string{"x"});
  Check(args_args.ContainsKey("a") && !args_args.ContainsKey("c"), "ContainsKey");
  Check(std::get<std::int32_t>(args_args.At("a")) == 1, "At 读回");
  args_args.Set("a", std::int32_t{2});
  Check(std::get<std::int32_t>(args_args.At("a")) == 2, "Set 覆盖");

  const ui::WidgetArgs args_filled = args_args.With("c", std::int32_t{3});
  Check(args_filled.ContainsKey("c") && std::get<std::int32_t>(args_args.At("a")) == 2 &&
            !args_args.ContainsKey("c"),
        "With 补缺不改源");

  args_args.Remove("a");
  Check(!args_args.ContainsKey("a"), "Remove");

  bool b_missing_threw = false;
  try {
    (void)args_args.At("missing");
  } catch (const std::exception&) {
    b_missing_threw = true;
  }
  Check(b_missing_threw, "At 缺键抛出");

  bool b_dup_threw = false;
  try {
    args_args.Add("b", std::int32_t{1});
  } catch (const std::exception&) {
    b_dup_threw = true;
  }
  Check(b_dup_threw, "Add 重复键抛出");
}

// ———— Mediator ————

class HandlerA final : public ui::NotificationHandlerSet<TestCountNotification> {
 public:
  void Handle(const TestCountNotification&) override { ++int4_count; }
  int int4_count = 0;
};

void TestMediator() {
  ui::Mediator mediator_m;
  HandlerA handler_a1;
  HandlerA handler_a2;

  mediator_m.Subscribe(handler_a1);
  mediator_m.Subscribe(handler_a2);
  mediator_m.Send(TestCountNotification{});
  Check(handler_a1.int4_count == 1 && handler_a2.int4_count == 1, "Send 插入序分发");

  mediator_m.Unsubscribe(handler_a1);
  mediator_m.Send(TestCountNotification{});
  Check(handler_a1.int4_count == 1 && handler_a2.int4_count == 2, "Unsubscribe 摘除");
}

// ———— Widget 树(纯逻辑)————

class CapturingWidget final : public ui::Widget {
 public:
  std::string_view TypeName() const override { return kWidgetTypeName; }
  std::unique_ptr<Widget> Clone() override { return std::make_unique<CapturingWidget>(); }

  bool HandleMouseInput(const MouseInput&) override {
    ++int4_mouse_inputs;
    return true;
  }
  bool HandleKeyPress(const KeyInput&) override {
    ++int4_key_presses;
    return true;
  }
  bool YieldMouseFocus(const MouseInput&) override {
    b_yield_called = true;
    return !b_refuse_yield;
  }

  int int4_mouse_inputs = 0;
  int int4_key_presses = 0;
  bool b_yield_called = false;
  bool b_refuse_yield = false;
};

int int4_counting_notifications_total = 0;

class CountingLogic final :
                            public ui::NotificationHandlerSet<TestCountNotification> {
 public:
  explicit CountingLogic(ui::WidgetArgs& args_args)
      : b_logic_args_present(args_args.TryGet("logicArgs") != nullptr) {}
  void Tick() override { ++int4_ticks; }
  void Handle(const TestCountNotification&) override {
    ++int4_notifications;
    ++int4_counting_notifications_total;
  }

  int int4_ticks = 0;
  int int4_notifications = 0;
  bool b_logic_args_present = false;
};

ORA_REGISTER_CHROME_LOGIC(CountingLogic);

void TestWidgetTree() {
  ui::WidgetArgs args_args;
  args_args.Set("windowSize", int2{640, 480});

  ui::ContainerWidget root_widget;
  root_widget.str_id = "ROOT";
  root_widget.expr_width.emplace("WINDOW_WIDTH");
  root_widget.expr_height.emplace("WINDOW_HEIGHT - 100");
  root_widget.Initialize(args_args);
  Check(root_widget.bounds_widget.int4_width == 640 &&
            root_widget.bounds_widget.int4_height == 380,
        "根界 = 窗口表达式求值");

  auto up_child = std::make_unique<CapturingWidget>();
  CapturingWidget* ptr_child = up_child.get();
  up_child->expr_x.emplace("10");
  up_child->expr_y.emplace("20");
  up_child->expr_width.emplace("PARENT_WIDTH - 40");
  up_child->expr_height.emplace("PARENT_HEIGHT - 60");
  root_widget.AddChild(std::move(up_child));
  ptr_child->Initialize(args_args);
  Check(ptr_child->bounds_widget.int4_width == 600 &&
            ptr_child->bounds_widget.int4_height == 320,
        "子界 PARENT_* 求值");
  const int2 int2_origin = ptr_child->RenderOrigin();
  Check(int2_origin.X == 10 && int2_origin.Y == 20, "RenderOrigin = 父原点 + 界");

  Check(root_widget.GetOrNull("ROOT") == &root_widget, "GetOrNull 自身命中");
  Check(root_widget.GetOrNull("nonexistent") == nullptr, "GetOrNull 未命中");
  CheckThrow("has no child nonexistent", [&] { (void)root_widget.Get("nonexistent"); });

  root_widget.LoadFieldOrProperty("Logic", "A, B ,,C");
  Check(root_widget.vec_logic.size() == 3 && root_widget.vec_logic[1] == "B",
        "Logic 拆分语义");

  bool b_bool_threw = false;
  try {
    root_widget.LoadFieldOrProperty("Visible", "yes");
  } catch (const std::exception&) {
    b_bool_threw = true;
  }
  Check(b_bool_threw, "bool 解析拒绝非字面量");
}

// ———— Game 主循环骨架(Ui 首次消费,LastTickTime 从 0 起)————

void TestGameLoop() {
  std::int64_t int8_clock = 0;
  game::Game::Deps deps_pacing;
  deps_pacing.fn_clock = [&int8_clock] { return int8_clock; };
  game::Game game_pacing(deps_pacing);

  game_pacing.JoinLocal();
  net::OrderManager* ptr_om = game_pacing.OrderManagerFace();
  Check(ptr_om != nullptr, "JoinLocal 建 OM");
  Check(ptr_om->LobbyInfo().vec_clients.size() == 1 &&
            ptr_om->LobbyInfo().vec_clients[0].Index ==
                ptr_om->Connection().LocalClientId() &&
            ptr_om->LobbyInfo().vec_clients[0].State == net::ClientState::Ready,
        "观战客户端面");

  int8_clock = 100;
  game_pacing.InnerLogicTick(*ptr_om);
  Check(ui::Ui::Instance().LastTickTime().Value() == 40, "Ui LastTickTime 前进一档");
  Check(ptr_om->LastTickTime().Value() == 40, "OM LastTickTime 前进一档");
  game_pacing.InnerLogicTick(*ptr_om);
  Check(ptr_om->LocalFrameNumber() == 0, "无世界不下帧");

  sim::World world_game(sim::WorldSimParams{});
  ptr_om->SetWorld(&world_game);
  ptr_om->StartGame();
  int8_clock = 200;
  game_pacing.InnerLogicTick(*ptr_om);
  Check(ptr_om->LocalFrameNumber() == 1, "首帧推进");
  Check(world_game.WorldTick() == 1, "world.Tick 执行");
  int8_clock = 300;
  game_pacing.InnerLogicTick(*ptr_om);
  Check(ptr_om->LocalFrameNumber() == 2 && world_game.WorldTick() == 2, "次帧推进");

  std::int64_t int8_clock2 = 0;
  game::Game::Deps deps_run;
  deps_run.fn_clock = [&int8_clock2] {
    int8_clock2 += 30;
    return int8_clock2;
  };
  bool b_quit = false;
  deps_run.fn_on_quit = [&b_quit] { b_quit = true; };
  game::Game game_run(deps_run);
  game_run.JoinLocal();
  game_run.RunAfterDelay(60, [&game_run] { game_run.Exit(); });
  const game::RunStatus kind_status = game_run.Run();
  Check(kind_status == game::RunStatus::Success, "Run 返回 Success");
  Check(game_run.OrderManagerFace() == nullptr, "Run 清理 OM");
  Check(b_quit, "OnQuit 钩子");

  bool b_current = game_pacing.IsCurrentWorld(&world_game);
  Check(b_current, "IsCurrentWorld 命中");
}

// ———— 焦点/悬停/生命周期(Ui 单例)————

void TestWidgetFocusInput() {
  ui::Ui& ui_face = ui::Ui::Instance();

  ui::WidgetArgs args_bounds;
  args_bounds.Set("windowSize", int2{100, 100});
  ui::ContainerWidget root_ui;
  root_ui.expr_width.emplace("WINDOW_WIDTH");
  root_ui.expr_height.emplace("WINDOW_HEIGHT");
  root_ui.Initialize(args_bounds);

  auto up_child = std::make_unique<CapturingWidget>();
  CapturingWidget* ptr_child = up_child.get();
  up_child->expr_width.emplace("50");
  up_child->expr_height.emplace("50");
  root_ui.AddChild(std::move(up_child));
  ptr_child->Initialize(args_bounds);

  const MouseInput mi_move_in{MouseInputEvent::Move,
                                        MouseButton::None,
                                        {10, 10},
                                        {0, 0},
                                        Modifiers::None,
                                        0};
  Check(root_ui.HandleMouseInputOuter(mi_move_in), "子级处理冒泡截停");
  Check(ui_face.MouseOverWidget() == ptr_child, "Move 悬停落子级");
  Check(ptr_child->int4_mouse_inputs == 1, "子级收到输入");

  // Ui.HandleInput 的 Move 清悬停 + MouseExited 通知(根不含点)
  ui_face.HandleInput(mi_move_in);
  Check(ui_face.MouseOverWidget() == nullptr, "Ui.HandleInput 清悬停");

  const MouseInput mi_down{MouseInputEvent::Down,
                                     MouseButton::Left,
                                     {10, 10},
                                     {0, 0},
                                     Modifiers::None,
                                     1};
  Check(ptr_child->TakeMouseFocus(mi_down), "TakeMouseFocus 成功");
  Check(ui_face.MouseFocusWidget() == ptr_child, "焦点落子级");

  auto up_other = std::make_unique<CapturingWidget>();
  CapturingWidget* ptr_other = up_other.get();
  Check(ptr_other->TakeMouseFocus(mi_down), "换焦点成功");
  Check(ptr_child->b_yield_called && ui_face.MouseFocusWidget() == ptr_other, "让出链");

  ptr_other->b_refuse_yield = true;
  ptr_other->Hidden();
  Check(ui_face.MouseFocusWidget() == nullptr, "Hidden 强制让出");

  const KeyInput ki_down{KeyInputEvent::Down, Keycode::A,
                                   Modifiers::None, 1, u'A', false};
  Check(ptr_other->TakeKeyboardFocus(), "TakeKeyboardFocus");
  Check(ui_face.HandleKeyPress(ki_down) && ptr_other->int4_key_presses == 1,
        "键盘路由焦点 widget");

  // Logic:PostInit 订阅 + TickOuter + Removed 退订
  ui::WidgetArgs args_logic;
  ui::ContainerWidget root_logic;
  root_logic.vec_logic = {"CountingLogic"};
  root_logic.PostInit(args_logic);
  Check(root_logic.LogicObjects().size() == 1, "Logic 物化");
  root_logic.TickOuter();
  const auto* ptr_logic =
      static_cast<const CountingLogic*>(root_logic.LogicObjects()[0].get());
  Check(ptr_logic->int4_ticks == 1, "TickOuter 推进 Logic");
  ui_face.Send(TestCountNotification{});
  Check(int4_counting_notifications_total == 1, "PostInit 已订阅");
  root_logic.Removed();
  ui_face.Send(TestCountNotification{});
  Check(int4_counting_notifications_total == 1, "Removed 退订(计数器断言)");

  // Clone 深拷贝 + InputWidget 的 IsDisabled 读源怪癖
  ui::ContainerWidget widget_source;
  auto up_input = std::make_unique<ui::InputWidget>();
  up_input->str_id = "IN";
  ui::InputWidget* ptr_input = up_input.get();
  widget_source.AddChild(std::move(up_input));
  auto up_clone = widget_source.Clone();
  auto* ptr_clone_input = up_clone->GetOrNull<ui::InputWidget>("IN");
  Check(ptr_clone_input != nullptr, "Clone 深拷贝子级");
  ptr_input->b_disabled = true;
  Check(ptr_clone_input->fn_is_disabled(), "InputWidget 克隆 IsDisabled 读源(上游怪癖)");
}

// ———— Ui 窗口栈 ————

int int4_stack_became_hidden = 0;
int int4_stack_became_visible = 0;

class StackLogic final : public ui::ChromeLogic {
 public:
  explicit StackLogic(ui::WidgetArgs&) {}
  void BecameHidden() override {
    ++int4_hidden;
    ++int4_stack_became_hidden;
  }
  void BecameVisible() override {
    ++int4_visible;
    ++int4_stack_became_visible;
  }
  int int4_hidden = 0;
  int int4_visible = 0;
};

ORA_REGISTER_CHROME_LOGIC(StackLogic);

void TestUiWindowStack() {
  ui::Ui& ui_face = ui::Ui::Instance();
  ui_face.ResetAll();

  std::vector<std::unique_ptr<ui::Widget>> vec_fake_owned;
  ui::Ui::Deps deps_ui;
  deps_ui.fn_load_widget = [&vec_fake_owned](ui::WidgetArgs& args_args,
                                             ui::Widget* ptr_parent,
                                             const std::string& str_id) {
    auto up_widget = std::make_unique<ui::ContainerWidget>();
    up_widget->str_id = str_id;
    up_widget->vec_logic = {"StackLogic"};
    up_widget->PostInit(args_args);
    ui::Widget* ptr_widget = up_widget.get();
    if (ptr_parent != nullptr)
      ptr_parent->AddChild(std::move(up_widget));
    else
      vec_fake_owned.push_back(std::move(up_widget));
    return ptr_widget;
  };
  ui_face.Initialize(std::move(deps_ui));

  ui::Widget* ptr_window_a = ui_face.OpenWindow("A");
  Check(ptr_window_a != nullptr && ui_face.CurrentWindow() == ptr_window_a,
        "OpenWindow 入栈");
  Check(ui_face.Root()->GetOrNull("A") == ptr_window_a, "窗口挂 Root");

  ui::Widget* ptr_window_b = ui_face.OpenWindow("B");
  Check(ui_face.Root()->GetOrNull("A") == nullptr, "开新窗隐藏旧窗");
  Check(ui_face.CurrentWindow() == ptr_window_b, "栈顶即新窗");

  ui_face.CloseWindow();
  Check(ui_face.CurrentWindow() == ptr_window_a, "CloseWindow 弹顶还原");
  Check(ui_face.Root()->GetOrNull("A") == ptr_window_a, "旧窗回 Root");
  Check(int4_stack_became_hidden == 1 && int4_stack_became_visible == 1,
        "B 的 BecameHidden 于 Dispose 后仍通知 + A 还原 BecameVisible");

  ui_face.OpenWindow("C");
  ui_face.CloseWindow();
  Check(int4_stack_became_hidden == 2 && int4_stack_became_visible == 2,
        "BecameHidden/BecameVisible 计数推进");

  ui_face.ResetAll();
  Check(ui_face.CurrentWindow() == nullptr && ui_face.Root()->Children().empty(),
        "ResetAll 清空");
}

// ———— ChromeMetrics ————

void TestChromeMetrics() {
  const auto dir_metrics = std::filesystem::temp_directory_path() / "ora_ui_test_metrics";
  std::filesystem::create_directories(dir_metrics);
  {
    std::ofstream out_metrics(dir_metrics / "metrics.yaml");
    out_metrics << "A:\n\tX: from-a\n\tText: hello\nB:\n\tX: from-b\n";
  }

  fs::FileSystem fs_metrics;
  fs_metrics.Mount(dir_metrics.generic_string());
  const std::vector<std::string> vec_files{"metrics.yaml"};
  ui::ChromeMetrics metrics_c;
  metrics_c.Initialize({&fs_metrics, &vec_files});

  Check(metrics_c.Get("X") == "from-b", "Get 后源覆盖");
  Check(metrics_c.Get("Text") == "hello", "Get 命中");
  std::string str_out;
  Check(!metrics_c.TryGet("nope", str_out) && str_out.empty(), "TryGet 未命中");
  CheckThrow("not present in the dictionary", [&] { (void)metrics_c.Get("nope"); });
}

// ———— WidgetLoader ————

void WriteLayout(const std::filesystem::path& dir_path, std::string_view sv_name,
                 std::string_view sv_body) {
  std::ofstream out_layout(dir_path / sv_name);
  out_layout << sv_body;
}

void TestWidgetLoader() {
  const auto dir_layout = std::filesystem::temp_directory_path() / "ora_ui_test_layout";
  std::filesystem::create_directories(dir_layout);
  WriteLayout(dir_layout, "menu.yaml",
              "Container@MENU:\n\tX: 10\n\tY: 20\n\tWidth: WINDOW_WIDTH - 20\n"
              "\tHeight: WINDOW_HEIGHT - 30\n\tLogic: CountingLogic\n\tChildren:\n"
              "\t\tContainer@CHILD:\n\t\t\tWidth: PARENT_WIDTH - 5\n\t\t\tHeight: 25\n");
  WriteLayout(dir_layout, "dup.yaml", "Container@DUP:\nContainer@DUP:\n");
  WriteLayout(dir_layout, "bogus.yaml", "Container@BOGUS:\n\tBogus: 1\n");
  WriteLayout(dir_layout, "unknown.yaml", "Foo@BAR:\n");
  WriteLayout(dir_layout, "multi.yaml", "Container@MULTI@EXTRA:\n");

  fs::FileSystem fs_layout;
  fs_layout.Mount(dir_layout.generic_string());

  const std::vector<std::string> vec_menu{"menu.yaml"};
  ui::WidgetLoader loader_menu(vec_menu, fs_layout);

  ui::WidgetArgs args_load;
  args_load.Set("windowSize", int2{640, 480});
  ui::ContainerWidget root_load;
  ui::Widget* ptr_menu = loader_menu.LoadWidget(args_load, &root_load, "MENU");
  Check(ptr_menu->str_id == "MENU", "Id 字段装配");
  Check(ptr_menu->bounds_widget.int4_x == 10 && ptr_menu->bounds_widget.int4_y == 20 &&
            ptr_menu->bounds_widget.int4_width == 620 &&
            ptr_menu->bounds_widget.int4_height == 450,
        "根界表达式");
  auto* ptr_child = ptr_menu->GetOrNull("CHILD");
  Check(ptr_child != nullptr && ptr_child->bounds_widget.int4_width == 615,
        "子界 PARENT_WIDTH 求值");
  Check(ptr_child->RenderOrigin().X == 10 && ptr_child->RenderOrigin().Y == 20,
        "子 RenderOrigin");

  const auto* ptr_logic =
      static_cast<const CountingLogic*>(ptr_menu->LogicObjects()[0].get());
  Check(ptr_logic->b_logic_args_present, "logicArgs 注入 Logic 构造");
  Check(!args_load.ContainsKey("logicArgs"), "logicArgs 用后移除");
  ui::Ui::Instance().Send(TestCountNotification{});
  Check(int4_counting_notifications_total == 2, "加载即订阅(全局计数 2)");

  root_load.RemoveChildren();
  ui::Ui::Instance().Send(TestCountNotification{});
  Check(int4_counting_notifications_total == 2, "RemoveChildren 退订(计数器断言)");

  CheckThrow("Widget has duplicate Key `Container@DUP`", [&] {
    const std::vector<std::string> vec_dup{"dup.yaml"};
    ui::WidgetLoader loader_dup(vec_dup, fs_layout);
  });

  CheckThrow("Cannot find widget with Id `nope`",
             [&] { (void)loader_menu.LoadWidget(args_load, &root_load, "nope"); });

  const std::vector<std::string> vec_bogus{"bogus.yaml"};
  ui::WidgetLoader loader_bogus(vec_bogus, fs_layout);
  CheckThrow("FieldLoader: Missing field `Bogus`",
             [&] { (void)loader_bogus.LoadWidget(args_load, &root_load, "BOGUS"); });

  const std::vector<std::string> vec_unknown{"unknown.yaml"};
  ui::WidgetLoader loader_unknown(vec_unknown, fs_layout);
  CheckThrow("Cannot locate type: FooWidget",
             [&] { (void)loader_unknown.LoadWidget(args_load, &root_load, "BAR"); });

  const std::vector<std::string> vec_multi{"multi.yaml"};
  ui::WidgetLoader loader_multi(vec_multi, fs_layout);
  ui::Widget* ptr_multi = loader_multi.LoadWidget(args_load, &root_load, "MULTI@EXTRA");
  Check(ptr_multi->str_id == "MULTI", "Id = Split('@')[1]");
}

// ———— Phase 6 第二批:Hotkey/HotkeyManager ————

void TestHotkey() {
  ui::Hotkey hk_key{};
  Check(ui::Hotkey::TryParse("F5", hk_key) && hk_key.key_kind == Keycode::F5 &&
            hk_key.kind_modifiers == Modifiers::None,
        "Hotkey.TryParse 单段");
  Check(ui::Hotkey::TryParse("f5 ctrl", hk_key) && hk_key.key_kind == Keycode::F5 &&
            hk_key.kind_modifiers == Modifiers::Ctrl,
        "Hotkey.TryParse 忽略大小写双段");
  Check(!ui::Hotkey::TryParse("NOT A KEY", hk_key), "Hotkey.TryParse 未知名失败");
  Check(!ui::Hotkey::TryParse("F5 BOGUSMOD", hk_key), "Hotkey.TryParse 未知修饰失败");
  Check((ui::Hotkey{Keycode::F5, Modifiers::None}).ToString() == "F5 None",
        "Hotkey.ToString");

  // FluentProvider 未初始化 = 键回落(D247 族)
  Check(ui::Hotkey{Keycode::F5, Modifiers::None}.DisplayString() == "keycode.f5",
        "KeycodeExts.DisplayString 键回落");

  const ui::Hotkey hk_invalid = ui::Hotkey::Invalid();
  Check(hk_invalid != ui::Hotkey{Keycode::F5, Modifiers::None} &&
            !(hk_invalid == ui::Hotkey::Invalid()),
        "UNKNOWN 永不相等");
  Check(ui::Hotkey{Keycode::F5, Modifiers::None} ==
            ui::Hotkey{Keycode::F5, Modifiers::None},
        "有效键相等");

  KeyInput input_key{};
  input_key.Event = KeyInputEvent::Down;
  input_key.Key = Keycode::F5;
  input_key.ModifierFlags = Modifiers::Ctrl;
  const ui::HotkeyReference ref_key = ui::HotkeyReferenceFromNameOrCode("f5 ctrl");
  Check(ref_key.IsActivatedBy(input_key), "HotkeyReference.IsActivatedBy 命中");
  input_key.Key = Keycode::F6;
  Check(!ref_key.IsActivatedBy(input_key), "HotkeyReference.IsActivatedBy 未命中");
  const ui::HotkeyReference ref_invalid = ui::HotkeyReferenceFromNameOrCode("junk");
  Check(!ref_invalid.GetValue().IsValid(), "硬编码回落失败 = Invalid");
}

void TestHotkeyManager(game::ModData& mod_data) {
  const ui::HotkeyManager::OpenFn fn_open = [&mod_data](const std::string& str_name) {
    return mod_data.ModFiles().Open(str_name);
  };

  // settings 覆盖:Sell = Z → 改设为 X
  std::vector<ui::HotkeyManager::SettingsOverride> vec_overrides{
      {"Sell", "X"}};
  ui::HotkeyManager manager_keys(fn_open, mod_data.ManifestRef().Hotkeys(),
                                 vec_overrides);

  Check(manager_keys.Definitions().size() > 50, "ra 热键定义装载量");

  KeyInput input_sell{};
  input_sell.Event = KeyInputEvent::Down;
  input_sell.Key = Keycode::X;
  Check(manager_keys.Get("Sell").IsActivatedBy(input_sell), "settings 覆盖生效");

  // Readonly 定义(chat.yaml 的 ToggleChatMode)Set 无效
  const ui::HotkeyDefinition* ptr_readonly = nullptr;
  for (const auto& [str_name, definition] : manager_keys.Definitions())
    if (definition.b_readonly) {
      ptr_readonly = &definition;
      break;
    }
  Check(ptr_readonly != nullptr, "Readonly 定义存在");
  if (ptr_readonly != nullptr) {
    manager_keys.Set(ptr_readonly->str_name, ui::Hotkey{Keycode::F5, Modifiers::None});
    Check(!manager_keys.Get(ptr_readonly->str_name)
               .GetValue()
               .IsValid() ||
          manager_keys.Get(ptr_readonly->str_name).GetValue().key_kind != Keycode::F5,
          "Readonly Set 跳过");
  }

  // 未知名回落硬编码解析
  KeyInput input_hard{};
  input_hard.Event = KeyInputEvent::Down;
  input_hard.Key = Keycode::Q;
  Check(manager_keys.Get("Q").IsActivatedBy(input_hard), "未知名硬编码回落");
}

// ———— Phase 6 第二批:SpriteFont(fake 字体)————

class FakeFont final : public platform::IFont {
 public:
  platform::FontGlyph CreateGlyph(std::uint32_t uint4_char_code, int int4_size,
                                  float fp4_device_scale) override {
    // 确定性度量:宽 2/高 3/advance 4/offset (1,2);数据 = 竖条纹
    // Deterministic metrics: width 2 / height 3 / advance 4 / offset (1,2);
    // data = vertical stripes.
    platform::FontGlyph glyph_ret;
    glyph_ret.offset = int2{1, 2};
    glyph_ret.int4_width = 2;
    glyph_ret.int4_height = 3;
    glyph_ret.fp4_advance = 4.0f;
    glyph_ret.vec_data.assign(2 * 3, 0);
    glyph_ret.vec_data[0] = 200;
    glyph_ret.vec_data[4] = 100;
    (void)uint4_char_code;
    (void)int4_size;
    (void)fp4_device_scale;
    return glyph_ret;
  }
};

void TestSpriteFont() {
  gfx::SheetBuilder builder_sheets(gfx::SheetType::BGRA, 128);

  struct Drawn {
    core::Vector3 vec_location;
    float fp4_scale;
    std::uint8_t u8_first_pixel;
  };
  std::vector<Drawn> vec_flat_drawn;
  std::vector<Drawn> vec_quad_drawn;

  auto up_font = std::make_unique<gfx::SpriteFont>(
      "fake", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8, 1.0f,
      builder_sheets,
      [&vec_flat_drawn](const gfx::Sprite& sprite_s, core::Vector3 v_location, float fp4_scale,
                        core::Vector3, float) {
        const std::span<const std::byte> span_data = sprite_s.ptr_sheet->GetData();
        vec_flat_drawn.push_back(
            Drawn{v_location, fp4_scale,
                  static_cast<std::uint8_t>(span_data[static_cast<std::size_t>(
                                               sprite_s.ptr_sheet->Size().X * 4 *
                                               sprite_s.Bounds.Top()) +
                                           4 * sprite_s.Bounds.Left()])});
      },
      [&vec_quad_drawn](const gfx::Sprite&, core::Vector3 v_a, core::Vector3, core::Vector3,
                        core::Vector3, core::Vector3, float) {
        vec_quad_drawn.push_back(Drawn{v_a, 0.0f, 0});
      });

  Check(up_font->TopOffset() == 2, "TopOffset = size - ascender");

  const int2 int2_size = up_font->Measure("AB");
  Check(int2_size.X == 8 && int2_size.Y == 10, "Measure(advance 和,行高 = size)");
  Check(up_font->Measure("") == int2{0, 10}, "Measure 空串");
  Check(up_font->Measure("A\nBB") == int2{8, 20}, "Measure 多行");

  up_font->DrawText("A", core::Vector2{10.0f, 20.0f}, core::Color::FromArgb(255, 255, 255, 255));
  Check(vec_flat_drawn.size() == 1, "DrawText 单字形单投递");
  // screen = ((int)(10*1+0.5), (int)((20+10)*1+0.5)) = (10, 30);pos = (10+1, 30+2)/1
  Check(vec_flat_drawn[0].vec_location.X == 11.0f && vec_flat_drawn[0].vec_location.Y == 32.0f,
        "DrawText 屏幕像素取整 + offset");
  Check(vec_flat_drawn[0].fp4_scale == 1.0f, "DrawText scale = 1/deviceScale");
  Check(vec_flat_drawn[0].u8_first_pixel == 200, "字形位图写入 sheet");

  // 阴影:offset=1 → bg 先绘(位移 1),fg 后绘
  vec_flat_drawn.clear();
  up_font->DrawTextWithShadow("A", core::Vector2{0.0f, 0.0f},
                              core::Color::FromArgb(255, 255, 255, 255),
                              core::Color::FromArgb(255, 0, 0, 0), 1);
  Check(vec_flat_drawn.size() == 2, "DrawTextWithShadow 双投递");
  Check(vec_flat_drawn[0].vec_location.X == vec_flat_drawn[1].vec_location.X + 1.0f,
        "阴影位移 1 像素");

  // 对比度:radius 1 → 膨胀字形先绘(半径 1 位移)
  vec_flat_drawn.clear();
  up_font->DrawTextWithContrast("A", core::Vector2{0.0f, 0.0f},
                                core::Color::FromArgb(255, 255, 255, 255),
                                core::Color::FromArgb(255, 0, 0, 0),
                                core::Color::FromArgb(255, 0, 0, 0), 1);
  Check(vec_flat_drawn.size() == 2, "DrawTextWithContrast 双投递");
  Check(vec_flat_drawn[0].vec_location.X == vec_flat_drawn[1].vec_location.X - 1.0f &&
            vec_flat_drawn[0].vec_location.Y == vec_flat_drawn[1].vec_location.Y - 1.0f,
        "对比度位移 (radius, radius)");

  // 角度绘制走四角形态
  up_font->DrawText("A", core::Vector2{5.0f, 5.0f},
                    core::Color::FromArgb(255, 255, 255, 255), 0.0f);
  Check(vec_quad_drawn.size() == 1, "DrawText(angle) 四角投递");

  // SetScale(2):advance 和除以 deviceScale
  up_font->SetScale(2.0f);
  Check(up_font->Measure("AB") == int2{4, 10}, "SetScale 后 Measure 减半");

  // 圆形权图:上游注释样例(r=1 角 0.60 / r=3 首行 0,0.44,0.80,1.00,…)
  const std::vector<float> vec_r1 = gfx::SpriteFont::CreateCircularWeightMap(1);
  Check(std::abs(vec_r1[0] - 0.60f) < 0.02f && std::abs(vec_r1[1] - 1.0f) < 0.001f &&
            std::abs(vec_r1[2] - 0.60f) < 0.02f && std::abs(vec_r1[4] - 1.0f) < 0.001f,
        "权图 r=1 角/边");
  const std::vector<float> vec_r3 = gfx::SpriteFont::CreateCircularWeightMap(3);
  Check(std::abs(vec_r3[0]) < 0.001f && std::abs(vec_r3[1] - 0.44f) < 0.02f &&
            std::abs(vec_r3[2] - 0.80f) < 0.02f && std::abs(vec_r3[3] - 1.0f) < 0.001f,
        "权图 r=3 首行");

  // GetContrastColor:白/亮色 → dark,暗色 → light
  Check(gfx::SpriteFont::GetContrastColor(core::Color::FromArgb(255, 255, 255, 255),
                                          core::Color::FromArgb(255, 1, 2, 3),
                                          core::Color::FromArgb(255, 4, 5, 6)) ==
            core::Color::FromArgb(255, 1, 2, 3),
        "GetContrastColor 白 → dark");
  Check(gfx::SpriteFont::GetContrastColor(core::Color::FromArgb(255, 10, 10, 10),
                                          core::Color::FromArgb(255, 1, 2, 3),
                                          core::Color::FromArgb(255, 4, 5, 6)) ==
            core::Color::FromArgb(255, 4, 5, 6),
        "GetContrastColor 暗 → light");
}

// ———— Phase 6 第二批:Fonts mod 装载 + 基础 widget 族 chrome.yaml 冒烟 ————

void TestFontsModData(game::ModData& mod_data) {
  const game::Fonts& fonts_mod = mod_data.GetOrCreateFonts();
  bool b_found_tiny = false;
  bool b_found_bold = false;
  for (const auto& [str_key, data_font] : fonts_mod.vec_font_list) {
    if (str_key == "Tiny") {
      b_found_tiny = data_font.int4_size == 10 && data_font.int4_ascender == 8 &&
                     data_font.str_font.find("FreeSans.ttf") != std::string::npos;
    }
    if (str_key == "Bold")
      b_found_bold = data_font.int4_size > 0;
  }
  Check(b_found_tiny, "Fonts.Tiny = FreeSans 10/8");
  Check(b_found_bold, "Fonts.Bold 存在");
}

void TestBaseWidgetsChrome(const std::string& str_root);

void TestSliderWidget();

void TestChromeWidgetsBatch3(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome);

void TestChromeWidgetsBatch4(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome);

void TestChromeWidgetsBatch5(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome);
void TestChromeWidgetsBatch6(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome);

void TestChromeWidgetsBatch7(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome);

void TestBaseWidgetsChrome(const std::string& str_root) {
  gen::RegisterGeneratedAll();
  game::RegisterGameLoaders();
  sim::RegisterWorldTraits();
  mods::RegisterCommonTraits();

  game::InstalledMods mods_installed{str_root + "/mods"};
  const game::Manifest* manifest_ra = mods_installed.Find("ra");
  Check(manifest_ra != nullptr, "ra mod found");
  if (manifest_ra == nullptr)
    return;
  game::ModData mod_data{*manifest_ra, mods_installed, str_root};

  mod_data.InitializeLoaders([&mod_data](const std::string& str_name) {
    std::optional<std::vector<char>> opt_bytes;
    try {
      opt_bytes = mod_data.ModFiles().Open(str_name);
    } catch (const std::exception&) {
    }
    return opt_bytes;
  });

  ui::ChromeMetrics metrics_chrome;
  metrics_chrome.Initialize(
      {.ptr_file_system = &mod_data.ModFiles(),
       .vec_metric_files = &manifest_ra->ChromeMetrics()});
  Check(metrics_chrome.GetColor("TextColor") == core::Color::FromArgb(255, 255, 255, 255),
        "metrics TextColor");
  Check(metrics_chrome.GetInt32("ButtonDepth") == 1, "metrics ButtonDepth = 1");
  Check(metrics_chrome.Get("ButtonFont") == "Regular", "metrics ButtonFont");

  ui::WidgetLoader loader_chrome(manifest_ra->ChromeLayout(), mod_data.ModFiles());

  ui::Ui::Deps deps_ui;
  deps_ui.ptr_mod_data = &mod_data;
  deps_ui.fn_window_size = [] { return int2{1280, 960}; };
  deps_ui.fn_clock = [] { return std::int64_t{0}; };
  deps_ui.ptr_chrome_metrics = &metrics_chrome;
  deps_ui.fn_load_widget = [&loader_chrome](ui::WidgetArgs& args_load, ui::Widget* ptr_parent,
                                            const std::string& str_id) {
    return loader_chrome.LoadWidget(args_load, ptr_parent, str_id);
  };
  ui::Ui::Instance().Initialize(deps_ui);

  ui::WidgetArgs args_prompt;
  args_prompt.Add("modData", static_cast<game::ModData*>(&mod_data));
  args_prompt.Add("chromeMetrics", static_cast<ui::ChromeMetrics*>(&metrics_chrome));
  args_prompt.Add("windowSize", int2{1280, 960});
  ui::Widget* ptr_prompt =
      loader_chrome.LoadWidget(args_prompt, nullptr, "THREEBUTTON_PROMPT");
  Check(ptr_prompt != nullptr && ptr_prompt->TypeName() == "BackgroundWidget",
        "THREEBUTTON_PROMPT 根类型");
  Check(ptr_prompt->RenderBounds().X == 340, "PARENT_WIDTH 方程求值(窗口 1280)");

  ui::ButtonWidget* ptr_confirm = ptr_prompt->GetOrNull<ui::ButtonWidget>("CONFIRM_BUTTON");
  ui::ButtonWidget* ptr_cancel = ptr_prompt->GetOrNull<ui::ButtonWidget>("CANCEL_BUTTON");
  Check(ptr_confirm != nullptr && ptr_cancel != nullptr, "按钮子装载");
  Check(ptr_confirm->str_text == "button-prompt-confirm", "按钮 Text 字段");
  Check(ptr_confirm->fn_get_text() == "Confirm", "按钮 Fluent 文本解析(en)");
  Check(ptr_confirm->int4_visual_height == 1, "按钮 VisualHeight = ButtonDepth");
  Check(ptr_confirm->fn_is_disabled != nullptr && !ptr_confirm->fn_is_disabled(),
        "按钮可用态");
  Check(ptr_confirm->key_hotkey.GetValue().key_kind == Keycode::RETURN,
        "按钮 Key: return 命名键");

  KeyInput input_return{};
  input_return.Event = KeyInputEvent::Down;
  input_return.Key = Keycode::RETURN;
  bool b_clicked = false;
  ptr_confirm->fn_is_visible = [] { return true; };
  ptr_confirm->fn_on_click = [&b_clicked] { b_clicked = true; };
  Check(ptr_confirm->HandleKeyPressOuter(input_return), "Return 键激活消费");
  Check(b_clicked, "Return 键触发 OnClick");

  b_clicked = false;
  ptr_cancel->fn_is_visible = [] { return true; };
  ptr_cancel->fn_on_click = [&b_clicked] { b_clicked = true; };
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  const int2 int2_center = ptr_cancel->RenderOrigin() +
                           int2{ptr_cancel->bounds_widget.int4_width / 2,
                                ptr_cancel->bounds_widget.int4_height / 2};
  MouseInput input_mouse{.Location = int2_center,
                         .Button = MouseButton::Left,
                         .Event = MouseInputEvent::Down};
  Check(ptr_cancel->HandleMouseInputOuter(input_mouse), "鼠标 Down 消费 + Depressed");
  Check(ptr_cancel->b_depressed, "Down 后 Depressed");
  input_mouse.Event = MouseInputEvent::Up;
  Check(ptr_cancel->HandleMouseInputOuter(input_mouse), "鼠标 Up 消费");
  Check(b_clicked, "Up 触发 OnClick");
  Check(!ptr_cancel->b_depressed, "Up 后 Depressed 复位");

  ui::WidgetArgs args_filter;
  ui::Widget* ptr_filter =
      loader_chrome.LoadWidget(args_filter, nullptr, "MULTIPLAYER_FILTER_PANEL");
  Check(ptr_filter != nullptr && ptr_filter->TypeName() == "ScrollPanelWidget",
        "MULTIPLAYER_FILTER_PANEL 根类型");
  auto* ptr_panel = static_cast<ui::ScrollPanelWidget*>(ptr_filter);
  Check(ptr_panel->OwnedChildren().size() >= 3, "过滤器面板 checkbox 数");
  const ui::CheckboxWidget* ptr_first_check =
      dynamic_cast<const ui::CheckboxWidget*>(ptr_panel->OwnedChildren().front().get());
  Check(ptr_first_check != nullptr, "子为 Checkbox");
  ptr_panel->ptr_layout->AdjustChildren();
  const int int4_expected_content = [&] {
    int int4_sum = ptr_panel->int4_top_bottom_spacing;
    for (const auto& up_child : ptr_panel->OwnedChildren())
      int4_sum += up_child->bounds_widget.int4_height + ptr_panel->int4_item_spacing;
    return int4_sum + ptr_panel->int4_top_bottom_spacing - ptr_panel->int4_item_spacing;
  }();
  Check(ptr_panel->int4_content_height == int4_expected_content,
        "ListLayout AdjustChildren 重排");
  Check(ptr_panel->int4_content_height == 104 && ptr_panel->bounds_widget.int4_height == 130,
        "过滤器面板 content(2+5×20+2=104)< 视口 130");

  MouseInput input_scroll{.Location = int2{0, 0},
                          .Button = MouseButton::None,
                          .Event = MouseInputEvent::Scroll,
                          .Delta = int2{0, -1}};
  ptr_panel->HandleMouseInput(input_scroll);
  input_scroll.Delta = int2{0, 1};
  ptr_panel->HandleMouseInput(input_scroll);
  ptr_panel->ScrollToBottom();
  Check(ptr_panel->ScrolledToBottom(), "ScrollToBottom");
  Check(ptr_panel->CurrentListOffset() == 0.0f, "content<视口时底 offset = 0");
  ptr_panel->ScrollToTop();
  Check(ptr_panel->CurrentListOffset() == 0.0f, "ScrollToTop");

  ui::WidgetArgs args_dropdown;
  ui::Widget* ptr_dropdown =
      loader_chrome.LoadWidget(args_dropdown, nullptr, "LABEL_DROPDOWN_TEMPLATE");
  Check(ptr_dropdown != nullptr && ptr_dropdown->TypeName() == "ScrollPanelWidget",
        "LABEL_DROPDOWN_TEMPLATE 根类型");
  auto* ptr_list = static_cast<ui::ScrollPanelWidget*>(ptr_dropdown);
  Check(ptr_list->OwnedChildren().size() == 2, "HEADER + TEMPLATE 两项");
  const ui::Widget& widget_header = *ptr_list->OwnedChildren().front();
  Check(widget_header.TypeName() == "ScrollItemWidget", "子为 ScrollItem");
  Check(!widget_header.fn_is_visible(), "模板 IsVisible 恒 false");
  Check(widget_header.bounds_widget.int4_y == 0, "装载序 Y 终值 = yaml 值");

  ptr_list->bounds_widget.int4_height = 40;
  ptr_list->ptr_layout->AdjustChildren();
  Check(ptr_list->int4_content_height == 42, "dropdown content = 2+13+25+2");

  auto* ptr_template_item = static_cast<ui::ScrollItemWidget*>(
      ptr_list->OwnedChildren()[1].get());
  bool b_selected = false;
  auto up_setup = ui::ScrollItemWidget::Setup(
      std::string{"k1"}, *ptr_template_item, [&b_selected] { return b_selected; },
      [] {}, [] {});
  Check(up_setup->fn_is_visible() && up_setup->str_item_key == "k1", "Setup 可见 + 键");

  ui::LabelWidget* ptr_label_inner =
      ptr_list->OwnedChildren().front()->GetOrNull<ui::LabelWidget>("LABEL");
  Check(ptr_label_inner != nullptr && ptr_label_inner->str_font == "TinyBold",
        "内嵌 Label Font");

  Check(!ptr_list->ScrolledToBottom(), "dropdown(40/42)未到底");
  ptr_list->ScrollToBottom();
  Check(ptr_list->CurrentListOffset() == -2.0f, "dropdown 底 offset = 40-42");
  Check(ptr_label_inner->fn_get_text().empty(), "Label 无 Text = 空串直通");

  TestSliderWidget();
  TestChromeWidgetsBatch3(mod_data, metrics_chrome, loader_chrome);
  TestChromeWidgetsBatch4(mod_data, metrics_chrome, loader_chrome);
  TestChromeWidgetsBatch5(mod_data, metrics_chrome);
  TestChromeWidgetsBatch6(mod_data, metrics_chrome, loader_chrome);
  TestChromeWidgetsBatch7(mod_data, metrics_chrome, loader_chrome);
}

void TestSliderWidget() {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  auto up_slider = std::make_unique<ui::SliderWidget>();
  up_slider->bounds_widget = {0, 0, 100, 20};
  ui::SliderWidget& slider_test = *up_slider;

  int int4_changes = 0;
  slider_test.fn_on_change = [&int4_changes](float) { ++int4_changes; };

  slider_test.fp4_minimum_value = 0.0f;
  slider_test.fp4_maximum_value = 1.0f;
  slider_test.UpdateValue(0.5f);
  Check(std::abs(slider_test.fp4_value - 0.5f) < 1e-6f && int4_changes == 1,
        "UpdateValue 0.5 + OnChange");
  slider_test.UpdateValue(2.0f);
  Check(slider_test.fp4_value == 1.0f && int4_changes == 2, "上界夹取");
  slider_test.UpdateValue(1.0f);
  Check(int4_changes == 2, "同值不再通知");

  slider_test.fp4_value = 0.0f;
  MouseInput input_slide{.Event = MouseInputEvent::Down,
                         .Button = MouseButton::Left,
                         .Location = int2{50, 10}};
  Check(slider_test.HandleMouseInput(input_slide), "Down 于 thumb 外仍取焦消费");
  Check(std::abs(slider_test.fp4_value - 0.5f) < 1e-6f, "Down 取值 0.5");
  input_slide.Event = MouseInputEvent::Move;
  input_slide.Location = int2{90, 10};
  Check(slider_test.HandleMouseInput(input_slide), "Move 消费");
  Check(std::abs(slider_test.fp4_value - 1.0f) < 1e-6f, "Move 拖至 1.0");
  input_slide.Event = MouseInputEvent::Up;
  Check(slider_test.HandleMouseInput(input_slide), "Up 消费");
  Check(ui::Ui::Instance().MouseFocusWidget() != &slider_test, "Up 让出鼠标焦点");

  input_slide.Event = MouseInputEvent::Down;
  input_slide.Button = MouseButton::Right;
  Check(!slider_test.HandleMouseInput(input_slide), "右键拒绝");
  slider_test.b_disabled = true;
  input_slide.Button = MouseButton::Left;
  Check(!slider_test.HandleMouseInput(input_slide), "禁用拒绝");
  slider_test.b_disabled = false;

  auto up_clone = std::unique_ptr<ui::SliderWidget>(
      static_cast<ui::SliderWidget*>(slider_test.Clone().release()));
  Check(up_clone->fp4_value == slider_test.fp4_value &&
            up_clone->fp4_maximum_value == 1.0f && up_clone->int4_ticks == 0,
        "Slider 克隆字段");
}

void TestChromeWidgetsBatch3(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome) {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  gfx::SheetBuilder builder_fonts(gfx::SheetType::BGRA, 128);
  constexpr auto fn_flat_noop = [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
                                  float) {};
  constexpr auto fn_quad_noop = [](const gfx::Sprite&, core::Vector3, core::Vector3,
                                  core::Vector3, core::Vector3, core::Vector3, float) {};
  auto up_font_regular = std::make_unique<gfx::SpriteFont>(
      "regular", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8, 1.0f,
      builder_fonts, fn_flat_noop, fn_quad_noop);
  auto up_font_bold = std::make_unique<gfx::SpriteFont>(
      "bold", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8, 1.0f,
      builder_fonts, fn_flat_noop, fn_quad_noop);
  gfx::SpriteFont* ptr_font_regular = up_font_regular.get();
  gfx::SpriteFont* ptr_font_bold = up_font_bold.get();

  auto deps_fonts = std::make_shared<ui::Ui::Deps>();
  deps_fonts->ptr_mod_data = &mod_data;
  deps_fonts->fn_window_size = [] { return int2{1280, 960}; };
  deps_fonts->fn_clock = [] { return std::int64_t{100000}; };
  deps_fonts->ptr_chrome_metrics = &metrics_chrome;
  deps_fonts->fn_load_widget = [&loader_chrome](ui::WidgetArgs& args_load, ui::Widget* ptr_parent,
                                                const std::string& str_id) {
    return loader_chrome.LoadWidget(args_load, ptr_parent, str_id);
  };
  deps_fonts->fn_release_root = [&loader_chrome](ui::Widget* ptr_root) {
    return ptr_root != nullptr ? loader_chrome.ReleaseRoot(*ptr_root) : nullptr;
  };
  deps_fonts->fn_try_get_font = [ptr_font_regular, ptr_font_bold](const std::string& str_key) {
    if (str_key == "Regular")
      return ptr_font_regular;
    if (str_key == "Bold")
      return ptr_font_bold;
    return static_cast<gfx::SpriteFont*>(nullptr);
  };
  ui::Ui::Instance().Initialize(*deps_fonts);

  {
    ui::WidgetArgs args_field;
    auto up_field = std::make_unique<ui::TextFieldWidget>(args_field);
    ui::TextFieldWidget& field_test = *up_field;
    field_test.bounds_widget = {0, 0, 100, 20};
    Check(field_test.str_font == "Regular", "TextField Font = TextfieldFont 指标");

    field_test.SetType(ui::TextFieldType::Filename);
    field_test.SetText("a<b>:c*?");
    Check(field_test.Text() == "abc", "Filename 过滤 9 标点");
    field_test.SetType(ui::TextFieldType::Integer);
    field_test.SetText("1a2b3");
    Check(field_test.Text() == "123", "Integer 只留数字");
    field_test.SetType(ui::TextFieldType::General);

    KeyInput input_key{};
    input_key.Event = KeyInputEvent::Down;
    input_key.Key = Keycode::LEFT;
    Check(!field_test.HandleKeyPress(input_key), "未聚焦按键不消费");
    Check(field_test.TakeKeyboardFocus(), "TakeKeyboardFocus");

    int int4_edited = 0;
    field_test.fn_on_text_edited = [&int4_edited] { ++int4_edited; };
    field_test.SetText("ab");
    field_test.int4_cursor_position = 1;
    Check(field_test.HandleTextInput("X") && field_test.Text() == "aXb" &&
              field_test.int4_cursor_position == 2 && int4_edited == 1,
          "光标位插入");

    field_test.int4_max_length = 3;
    Check(field_test.HandleTextInput("Y") && field_test.Text() == "aXb" && int4_edited == 1,
          "MaxLength 满员拒插");
    field_test.SetText("ab");
    field_test.int4_cursor_position = 2;
    field_test.HandleTextInput("cd");
    Check(field_test.Text() == "abc" && field_test.int4_cursor_position == 3,
          "粘贴截断到 MaxLength");
    field_test.int4_max_length = 0;

    field_test.SetText("hello world");
    input_key.Key = Keycode::END;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 11, "END 到尾");
    input_key.Key = Keycode::LEFT;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 10, "LEFT 退一");
    input_key.ModifierFlags = Modifiers::Ctrl;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 6, "Ctrl+LEFT 词跳到 6");
    input_key.ModifierFlags = Modifiers::None;
    input_key.Key = Keycode::HOME;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 0, "HOME 到头");
    input_key.Key = Keycode::RIGHT;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 1, "RIGHT 进一");
    input_key.ModifierFlags = Modifiers::Ctrl;
    input_key.Key = Keycode::RIGHT;
    field_test.HandleKeyPress(input_key);
    Check(field_test.int4_cursor_position == 5, "Ctrl+RIGHT 词跳到 5");
    input_key.ModifierFlags = Modifiers::None;

    input_key.Key = Keycode::A;
    input_key.ModifierFlags = Modifiers::Ctrl;
    field_test.HandleKeyPress(input_key);
    Check(field_test.SelectionStartIndex() == 0 && field_test.SelectionEndIndex() == 11,
          "Ctrl+A 全选");
    input_key.Key = Keycode::BACKSPACE;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text().empty() && field_test.int4_cursor_position == 0 &&
              field_test.SelectionStartIndex() == -1,
          "BACKSPACE 删选区");

    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 5;
    input_key.Key = Keycode::U;
    input_key.ModifierFlags = Modifiers::Ctrl;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text().empty() && field_test.int4_cursor_position == 0, "Ctrl+U 全删");

    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 2;
    input_key.Key = Keycode::K;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == "ab", "Ctrl+K 删尾");

    field_test.SetText("abc");
    field_test.int4_cursor_position = 0;
    input_key.Key = Keycode::D;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == "bc", "Ctrl+D");

    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 0;
    input_key.Key = Keycode::DELETE;
    input_key.ModifierFlags = Modifiers::None;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == "b cd", "DELETE 单字");
    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 0;
    input_key.ModifierFlags = Modifiers::Ctrl;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == " cd", "Ctrl+DELETE 词删(保留分隔空格)");
    input_key.ModifierFlags = Modifiers::None;

    input_key.Key = Keycode::LEFT;
    input_key.ModifierFlags = Modifiers::None;
    field_test.SetText("abc");
    field_test.int4_cursor_position = 3;
    field_test.HandleKeyPress(input_key);
    Check(field_test.ShowCursor(), "ResetBlinkCycle 后光标可见");
    for (int int4_i = 0; int4_i < 10; ++int4_i)
      field_test.Tick();
    Check(!field_test.ShowCursor(), "10 tick 后光标隐藏(相位翻转)");

    bool b_lost_focus = false;
    field_test.fn_on_lose_focus = [&b_lost_focus] { b_lost_focus = true; };
    field_test.YieldKeyboardFocus();
    Check(b_lost_focus, "YieldKeyboardFocus 触发 OnLoseFocus");

    field_test.SetText("ab");
    field_test.int4_cursor_position = 0;
    field_test.TakeKeyboardFocus();
    MouseInput input_field{.Event = MouseInputEvent::Down,
                           .Button = MouseButton::Left,
                           .Location = int2{7, 10}};
    Check(field_test.HandleMouseInput(input_field) && field_test.int4_cursor_position == 1,
          "点击最近字位");
    input_field.Event = MouseInputEvent::Up;
    Check(field_test.HandleMouseInput(input_field), "拖选 Up 结束");
    field_test.YieldKeyboardFocus();
  }

  {
    ui::TooltipContainerWidget container_logic;
    std::string str_tooltip_text = "AB\nCD";
    ui::WidgetArgs args_tooltip_panel;
    args_tooltip_panel.Add("modData", static_cast<game::ModData*>(&mod_data));
    args_tooltip_panel.Add("chromeMetrics", static_cast<ui::ChromeMetrics*>(&metrics_chrome));
    args_tooltip_panel.Add("windowSize", int2{1280, 960});
    args_tooltip_panel.Add("tooltipContainer", static_cast<ui::Widget*>(&container_logic));
    args_tooltip_panel.Add(
        "getText",
        std::function<std::string()>{[&str_tooltip_text] { return str_tooltip_text; }});
    ui::Widget* ptr_panel_raw =
        loader_chrome.LoadWidget(args_tooltip_panel, nullptr, "SIMPLE_TOOLTIP");
    std::unique_ptr<ui::Widget> up_panel_root = loader_chrome.ReleaseRoot(*ptr_panel_raw);
    Check(up_panel_root != nullptr && up_panel_root->TypeName() == "BackgroundWidget",
          "SIMPLE_TOOLTIP 装载");
    Check(up_panel_root->LogicObjects().size() == 1, "SimpleTooltipLogic 物化");

    container_logic.fn_before_render();
    Check(up_panel_root->OwnedChildren().size() == 2, "两行 Label 克隆");
    Check(up_panel_root->bounds_widget.int4_width == 22 &&
              up_panel_root->bounds_widget.int4_height == 47,
          "SimpleTooltip 自适应尺寸(14+8 / 44+3)");
    const ui::Widget& widget_line1 = *up_panel_root->OwnedChildren()[1];
    Check(widget_line1.bounds_widget.int4_y == 21 && widget_line1.bounds_widget.int4_width == 8,
          "第二行 Y=21(−1+3+19)/宽 8");

    std::string str_child_ptr = [&] {
      std::ostringstream oss_stream;
      oss_stream << up_panel_root->OwnedChildren()[0].get();
      return oss_stream.str();
    }();
    container_logic.fn_before_render();
    std::ostringstream oss_after;
    oss_after << up_panel_root->OwnedChildren()[0].get();
    Check(oss_after.str() == str_child_ptr, "同文本不重建");
    str_tooltip_text = "E";
    container_logic.fn_before_render();
    Check(up_panel_root->OwnedChildren().size() == 1 &&
              up_panel_root->bounds_widget.int4_height == 28,
          "新文本重建(单行 25+3)");
  }

  {
    auto up_container = std::make_unique<ui::TooltipContainerWidget>();
    up_container->str_id = "TOOLTIP_CONTAINER";
    ui::Widget* ptr_container = up_container.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_container));

    ui::WidgetArgs args_label;
    auto up_label = std::make_unique<ui::LabelWithTooltipWidget>(args_label);
    up_label->str_id = "LWT";
    up_label->str_tooltip_container = "TOOLTIP_CONTAINER";
    up_label->str_tooltip_template = "SIMPLE_TOOLTIP";
    up_label->fn_get_tooltip_text = [] { return std::string{"AB"}; };
    up_label->bounds_widget = {10, 10, 50, 10};
    ui::LabelWithTooltipWidget* ptr_label_lwt = up_label.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_label));

    ptr_label_lwt->MouseEntered();
    Check(ptr_container->OwnedChildren().empty(), "未过期前懒装载不触发");

    gfx::Viewport::int8_last_move_run_time = 0;
    ptr_container->DrawOuter();
    Check(ptr_container->OwnedChildren().size() == 1, "懒装载 SIMPLE_TOOLTIP");
    const ui::Widget& widget_tip = *ptr_container->OwnedChildren().front();
    Check(widget_tip.bounds_widget.int4_width == 22 &&
              widget_tip.bounds_widget.int4_height == 28,
          "端到端 tooltip 尺寸(AB 单行)");

    ptr_label_lwt->MouseExited();
    Check(ptr_container->OwnedChildren().empty(), "MouseExited 移除 tooltip");
    ui::Ui::Instance().Root()->RemoveChild(*ptr_label_lwt);
    ui::Ui::Instance().Root()->RemoveChild(*ptr_container);
  }

  {
    ui::WidgetArgs args_dropdown_button;
    args_dropdown_button.Add("modData", static_cast<game::ModData*>(&mod_data));
    auto up_button = std::make_unique<ui::DropDownButtonWidget>(args_dropdown_button);
    up_button->str_id = "DDB";
    up_button->bounds_widget = {10, 10, 120, 25};
    ui::DropDownButtonWidget* ptr_button_drop = up_button.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_button));

    bool b_option_clicked = false;
    const std::vector<std::string> vec_options{"one", "two"};
    ptr_button_drop->ShowDropDown<std::string>(
        "LABEL_DROPDOWN_TEMPLATE", 60, vec_options,
        [&b_option_clicked](const std::string& str_option, ui::ScrollItemWidget& item_template) {
          return ui::ScrollItemWidget::Setup(
              str_option, item_template, [] { return false; },
              [&b_option_clicked, str_option] { b_option_clicked = str_option == "two"; }, [] {});
        });

    ui::Widget* ptr_mask = nullptr;
    ui::ScrollPanelWidget* ptr_drop_panel = nullptr;
    for (const auto& up_child : ui::Ui::Instance().Root()->OwnedChildren()) {
      if (up_child->TypeName() == "MaskWidget")
        ptr_mask = up_child.get();
      if (up_child->TypeName() == "ScrollPanelWidget")
        ptr_drop_panel = static_cast<ui::ScrollPanelWidget*>(up_child.get());
    }
    Check(ptr_mask != nullptr && ptr_drop_panel != nullptr, "Mask + 面板挂 Root");
    Check(ptr_mask->bounds_widget.int4_width == 1280 && ptr_mask->bounds_widget.int4_height == 960,
          "Mask 全屏(分辨率注入)");
    Check(ptr_drop_panel->bounds_widget.int4_x == 10 &&
              ptr_drop_panel->bounds_widget.int4_y == 35,
          "面板锚定按钮下方(10, 10+25)");
    Check(ptr_drop_panel->bounds_widget.int4_height == 54, "面板高 = min(60, content 54)");
    Check(ptr_drop_panel->OwnedChildren().size() == 2, "两选项项");

    bool b_double_threw = false;
    try {
      auto up_dummy = std::make_unique<ui::ContainerWidget>();
      ptr_button_drop->AttachPanel(std::move(up_dummy));
    } catch (const std::exception& e_ptr) {
      b_double_threw = std::string_view(e_ptr.what()).find(
                           "Attempted to attach a panel to an open dropdown") !=
                       std::string_view::npos;
    }
    Check(b_double_threw, "开面板再挂抛出");

    ui::ScrollItemWidget* ptr_item_two =
        static_cast<ui::ScrollItemWidget*>(ptr_drop_panel->OwnedChildren()[1].get());
    ptr_item_two->fn_on_click();
    Check(b_option_clicked, "选项回调触发");
    bool b_panel_gone = true;
    for (const auto& up_child : ui::Ui::Instance().Root()->OwnedChildren())
      if (up_child.get() == ptr_drop_panel || up_child.get() == ptr_mask)
        b_panel_gone = false;
    Check(b_panel_gone, "点击后面板与 Mask 移除");

    ptr_button_drop->ShowDropDown<std::string>(
        "LABEL_DROPDOWN_TEMPLATE", 60, vec_options,
        [](const std::string& str_option, ui::ScrollItemWidget& item_template) {
          return ui::ScrollItemWidget::Setup(str_option, item_template, [] { return false; },
                                             [] {}, [] {});
        });
    Check(ptr_button_drop->HasKeyboardFocus(), "开面板取键盘焦点");
    KeyInput input_esc{};
    input_esc.Event = KeyInputEvent::Down;
    input_esc.Key = Keycode::ESCAPE;
    Check(ptr_button_drop->HandleKeyPress(input_esc), "ESC 撤销面板");

    ptr_button_drop->ShowDropDown<std::string>(
        "LABEL_DROPDOWN_TEMPLATE", 60, vec_options,
        [](const std::string& str_option, ui::ScrollItemWidget& item_template) {
          return ui::ScrollItemWidget::Setup(str_option, item_template, [] { return false; },
                                             [] {}, [] {});
        });
    ui::Widget* ptr_mask2 = nullptr;
    for (const auto& up_child : ui::Ui::Instance().Root()->OwnedChildren())
      if (up_child->TypeName() == "MaskWidget")
        ptr_mask2 = up_child.get();
    MouseInput input_mask{.Event = MouseInputEvent::Down,
                          .Button = MouseButton::Left,
                          .Location = int2{640, 480}};
    Check(ptr_mask2 != nullptr && ptr_mask2->HandleMouseInput(input_mask), "Mask 吞 Down");
    bool b_mask_gone = true;
    for (const auto& up_child : ui::Ui::Instance().Root()->OwnedChildren())
      if (up_child->TypeName() == "MaskWidget")
        b_mask_gone = false;
    Check(b_mask_gone, "Mask 点击后面板族移除");

    ui::Ui::Instance().Root()->RemoveChild(*ptr_button_drop);
  }
}

void TestChromeWidgetsBatch4(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome) {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  gfx::SheetBuilder builder_fonts4(gfx::SheetType::BGRA, 128);
  constexpr auto fn_flat_noop4 = [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
                                    float) {};
  constexpr auto fn_quad_noop4 = [](const gfx::Sprite&, core::Vector3, core::Vector3,
                                    core::Vector3, core::Vector3, core::Vector3, float) {};

  std::vector<float> vec_flat_x;
  auto up_font_capture = std::make_unique<gfx::SpriteFont>(
      "regular-capture", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8,
      1.0f, builder_fonts4,
      [&vec_flat_x](const gfx::Sprite&, core::Vector3 v_location, float, core::Vector3,
                    float) { vec_flat_x.push_back(v_location.X); },
      fn_quad_noop4);
  auto up_font_bold4 = std::make_unique<gfx::SpriteFont>(
      "bold4", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8, 1.0f,
      builder_fonts4, fn_flat_noop4, fn_quad_noop4);
  auto up_font_tinybold4 = std::make_unique<gfx::SpriteFont>(
      "tinybold4", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8, 1.0f,
      builder_fonts4, fn_flat_noop4, fn_quad_noop4);
  gfx::SpriteFont* ptr_font_capture = up_font_capture.get();
  gfx::SpriteFont* ptr_font_bold4 = up_font_bold4.get();
  gfx::SpriteFont* ptr_font_tinybold4 = up_font_tinybold4.get();

  auto deps_fonts4 = std::make_shared<ui::Ui::Deps>();
  deps_fonts4->ptr_mod_data = &mod_data;
  deps_fonts4->fn_window_size = [] { return int2{1280, 960}; };
  deps_fonts4->fn_clock = [] { return std::int64_t{100000}; };
  deps_fonts4->ptr_chrome_metrics = &metrics_chrome;
  deps_fonts4->fn_load_widget = [&loader_chrome](ui::WidgetArgs& args_load, ui::Widget* ptr_parent,
                                                 const std::string& str_id) {
    return loader_chrome.LoadWidget(args_load, ptr_parent, str_id);
  };
  deps_fonts4->fn_release_root = [&loader_chrome](ui::Widget* ptr_root) {
    return ptr_root != nullptr ? loader_chrome.ReleaseRoot(*ptr_root) : nullptr;
  };
  deps_fonts4->fn_try_get_font = [ptr_font_capture, ptr_font_bold4,
                                  ptr_font_tinybold4](const std::string& str_key) {
    if (str_key == "Regular")
      return ptr_font_capture;
    if (str_key == "Bold")
      return ptr_font_bold4;
    if (str_key == "TinyBold")
      return ptr_font_tinybold4;
    return static_cast<gfx::SpriteFont*>(nullptr);
  };
  ui::Ui::Instance().Initialize(*deps_fonts4);

  {
    ui::WidgetArgs args_highlight;
    auto up_label = std::make_unique<ui::LabelWithHighlightWidget>(args_highlight);
    up_label->str_id = "LWH";
    up_label->bounds_widget = {0, 0, 200, 20};
    up_label->opt_text = "A<BB>\\nC";
    Check(up_label->color_highlight == core::Color::FromArgb(255, 255, 255, 0),
          "HighlightColor = TextHighlightColor 指标");

    vec_flat_x.clear();
    up_label->Draw();
    Check(vec_flat_x.size() == 4, "A<BB>\\nC 组件 4 字符");
    if (vec_flat_x.size() == 4) {
      Check(std::abs(vec_flat_x[1] - vec_flat_x[0] - 4.0f) < 0.01f, "BB 组基 = A 宽(4)");
      Check(std::abs(vec_flat_x[3] - vec_flat_x[1] - 8.0f) < 0.01f, "C 组基 = BB 宽(8)");
    }
  }

  {
    auto up_root = std::make_unique<ui::ContainerWidget>();
    up_root->str_id = "LFI_ROOT";
    ui::Widget* ptr_root_input = up_root.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_root));

    ui::WidgetArgs args_field4;
    auto up_field = std::make_unique<ui::TextFieldWidget>(args_field4);
    up_field->str_id = "FIELD";
    up_field->bounds_widget = {0, 0, 100, 20};
    ptr_root_input->AddChild(std::move(up_field));

    ui::WidgetArgs args_label_input;
    auto up_label_input = std::make_unique<ui::LabelForInputWidget>(args_label_input);
    up_label_input->str_id = "LFI";
    up_label_input->str_for = "FIELD";
    up_label_input->opt_text = "name";
    up_label_input->bounds_widget = {0, 0, 100, 20};
    ui::LabelForInputWidget* ptr_label_input = up_label_input.get();
    ptr_root_input->AddChild(std::move(up_label_input));

    vec_flat_x.clear();
    ptr_label_input->Draw();
    Check(vec_flat_x.size() == 4, "LabelForInput 未禁用绘制");
    static_cast<ui::InputWidget*>(ptr_root_input->Get("FIELD"))->b_disabled = true;
    vec_flat_x.clear();
    ptr_label_input->Draw();
    Check(vec_flat_x.size() == 4, "LabelForInput 禁用绘制(缓存换色)");

    auto up_label_miss = std::make_unique<ui::LabelForInputWidget>(args_label_input);
    up_label_miss->str_for = "MISSING";
    ui::LabelForInputWidget* ptr_label_miss = up_label_miss.get();
    ptr_root_input->AddChild(std::move(up_label_miss));
    CheckThrow("has no child", [ptr_label_miss] { ptr_label_miss->Draw(); });
    ui::Ui::Instance().Root()->RemoveChild(*ptr_root_input);
  }

  {
    struct ProbePasswordField final : ui::PasswordFieldWidget {
      const std::string& Apparent() const { return GetApparentText(); }
    };
    ProbePasswordField field_password{};
    field_password.bounds_widget = {0, 0, 100, 20};
    field_password.SetText("abc");
    Check(field_password.Apparent() == "***", "PasswordField 三星");
    field_password.SetText("abcdefgh");
    Check(field_password.Apparent().size() == 8 &&
              field_password.Apparent().find_first_not_of('*') == std::string::npos,
          "PasswordField 八星");
  }

  {
    struct ProbeExponentialSlider final : ui::ExponentialSliderWidget {
      float ValueAt(int int4_x) const { return ValueFromPx(int4_x); }
      int PxAt(float fp4_v) const { return PxFromValue(fp4_v); }
    };
    ProbeExponentialSlider slider_exponential{};
    slider_exponential.bounds_widget = {0, 0, 200, 20};
    slider_exponential.fp4_minimum_value = 0.0f;
    slider_exponential.fp4_maximum_value = 100.0f;

    Check(slider_exponential.ValueAt(10) == 0.0f, "指数滑条左端 0");
    Check(slider_exponential.ValueAt(190) == 100.0f, "指数滑条右端 100");
    Check(std::abs(slider_exponential.ValueAt(100) - 100.0f * 0.0316f) < 0.05f,
          "指数滑条中点 ~3.16");
    Check(slider_exponential.PxAt(50.0f) == 171, "对数回投:50 → 171");
    Check(slider_exponential.PxAt(100.0f) == 189, "对数回投:100 → 189");
    Check(slider_exponential.PxAt(slider_exponential.ValueAt(100)) == 100,
          "对数中点往返 = 100");
  }

  {
    ui::WidgetArgs args_listener;
    auto up_listener = std::make_unique<ui::LogicKeyListenerWidget>(args_listener);
    int int4_first = 0;
    int int4_second = 0;
    up_listener->AddHandler([&int4_first](const KeyInput&) {
      ++int4_first;
      return false;
    });
    up_listener->AddHandler([&int4_second](const KeyInput&) {
      ++int4_second;
      return true;
    });
    KeyInput input_probe{};
    input_probe.Event = KeyInputEvent::Down;
    Check(up_listener->HandleKeyPress(input_probe), "KeyListener 第二 handler 消费");
    Check(int4_first == 1 && int4_second == 1, "KeyListener 顺序全询");

    ui::WidgetArgs args_ticker;
    auto up_ticker = std::make_unique<ui::LogicTickerWidget>(args_ticker);
    int int4_ticks = 0;
    up_ticker->fn_on_tick = [&int4_ticks] { ++int4_ticks; };
    up_ticker->Tick();
    up_ticker->Tick();
    Check(int4_ticks == 2, "LogicTicker OnTick 双计");
  }

  {
    ui::WidgetArgs args_menu;
    auto up_menu = std::make_unique<ui::MenuButtonWidget>(args_menu);
    Check(up_menu->str_menu_container == "INGAME_MENU" && up_menu->b_pause &&
              up_menu->b_hide_ingame_ui && !up_menu->b_disable_world_sounds,
          "MenuButton 默认四字段");
    up_menu->str_menu_container = "EXTRA_MENU";
    up_menu->b_pause = false;
    up_menu->b_hide_ingame_ui = false;
    up_menu->b_disable_world_sounds = true;
    auto up_clone = up_menu->Clone();
    auto* ptr_clone = static_cast<ui::MenuButtonWidget*>(up_clone.get());
    Check(ptr_clone->str_menu_container == "EXTRA_MENU" && !ptr_clone->b_pause &&
              !ptr_clone->b_hide_ingame_ui,
          "MenuButton clone 三字段拷贝");
    Check(!ptr_clone->b_disable_world_sounds, "clone 不拷 DisableWorldSounds(上游 quirk)");
  }

  {
    ui::WidgetArgs args_rgba;
    auto up_rgba = std::make_unique<ui::RGBASpriteWidget>(args_rgba);
    bool b_called = false;
    up_rgba->fn_get_sprite = [&b_called] {
      b_called = true;
      return gfx::Sprite{};
    };
    up_rgba->Draw();
    Check(b_called, "RGBASprite GetSprite 调用(空精灵不投递)");
  }

  {
    auto up_container = std::make_unique<ui::TooltipContainerWidget>();
    up_container->str_id = "TOOLTIP_CONTAINER";
    ui::Widget* ptr_container = up_container.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_container));

    ui::WidgetArgs args_wbutton;
    sim::World* ptr_world_dummy = reinterpret_cast<sim::World*>(0x1234);
    args_wbutton.Add("world", ptr_world_dummy);
    args_wbutton.Add("modData", static_cast<game::ModData*>(&mod_data));
    auto up_button = std::make_unique<ui::WorldButtonWidget>(args_wbutton);
    up_button->str_id = "WB";
    up_button->bounds_widget = {10, 10, 120, 25};
    up_button->str_tooltip_container = "TOOLTIP_CONTAINER";
    up_button->opt_tooltip_text = "Menu";
    ui::Hotkey key_q = ui::Hotkey::Invalid();
    key_q.key_kind = Keycode::Q;
    up_button->key_hotkey = ui::HotkeyReference{[key_q] { return key_q; }};
    ui::WorldButtonWidget* ptr_button_world = up_button.get();
    Check(ptr_button_world->ptr_world == ptr_world_dummy, "WorldButton args.world 注入");
    ui::Ui::Instance().Root()->AddChild(std::move(up_button));

    ptr_button_world->MouseEntered();
    Check(ptr_container->OwnedChildren().empty(), "BUTTON_TOOLTIP 未过期不装载");
    gfx::Viewport::int8_last_move_run_time = 0;
    ptr_container->DrawOuter();
    Check(ptr_container->OwnedChildren().size() == 1, "BUTTON_TOOLTIP 懒装载");
    const ui::Widget& widget_tip4 = *ptr_container->OwnedChildren().front();
    Check(widget_tip4.bounds_widget.int4_width == 49, "BUTTON_TOOLTIP 宽 = 49");
    Check(widget_tip4.bounds_widget.int4_height == 29, "BUTTON_TOOLTIP 高 = 29");
    const ui::LabelWidget* ptr_label_tip = widget_tip4.GetOrNull<ui::LabelWidget>("LABEL");
    Check(ptr_label_tip != nullptr && ptr_label_tip->bounds_widget.int4_width == 16,
          "tooltip LABEL 宽 = 16");
    const ui::LabelWidget* ptr_hotkey_tip =
        widget_tip4.GetOrNull<ui::LabelWidget>("HOTKEY");
    Check(ptr_hotkey_tip != nullptr && ptr_hotkey_tip->fn_is_visible() &&
              ptr_hotkey_tip->bounds_widget.int4_x == 30,
          "tooltip HOTKEY 可见 + X = 30");
    Check(ptr_hotkey_tip->fn_get_text() == "(Q)", "tooltip HOTKEY 文本");
    ptr_button_world->MouseExited();
    Check(ptr_container->OwnedChildren().empty(), "WorldButton 退悬移除");

    ui::WidgetArgs args_wbutton2;
    args_wbutton2.Add("modData", static_cast<game::ModData*>(&mod_data));
    auto up_button2 = std::make_unique<ui::WorldButtonWidget>(args_wbutton2);
    up_button2->str_id = "WB2";
    up_button2->bounds_widget = {10, 40, 120, 25};
    up_button2->str_tooltip_container = "TOOLTIP_CONTAINER";
    up_button2->opt_tooltip_text = "Menu";
    up_button2->fn_get_tooltip_desc = [] { return std::string{"line1\nline22"}; };
    ui::WorldButtonWidget* ptr_button_world2 = up_button2.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_button2));
    ptr_button_world2->MouseEntered();
    gfx::Viewport::int8_last_move_run_time = 0;
    ptr_container->DrawOuter();
    Check(ptr_container->OwnedChildren().size() == 1, "DESC 形懒装载");
    const ui::Widget& widget_desc = *ptr_container->OwnedChildren().front();
    Check(widget_desc.bounds_widget.int4_width == 38, "DESC 形宽 = 38");
    Check(widget_desc.bounds_widget.int4_height == 60, "DESC 形高 = 60");
    const ui::LabelWidget* ptr_desc_line =
        widget_desc.GetOrNull<ui::LabelWidget>("DESC");
    Check(ptr_desc_line != nullptr && ptr_desc_line->fn_get_text() == "line1",
          "首 DESC 命中 = 行克隆(克隆拷 Id)");
    Check(widget_desc.OwnedChildren().size() == 4, "LABEL+HOTKEY+两行 DESC 克隆");
    ptr_button_world2->MouseExited();
    ui::Ui::Instance().Root()->RemoveChildren();
  }

  {
    auto up_container = std::make_unique<ui::TooltipContainerWidget>();
    up_container->str_id = "TOOLTIP_CONTAINER";
    ui::Widget* ptr_container = up_container.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_container));

    auto up_region = std::make_unique<ui::ClientTooltipRegionWidget>();
    up_region->str_id = "CTR";
    up_region->str_tooltip_container = "TOOLTIP_CONTAINER";
    up_region->str_template = "THREEBUTTON_PROMPT";
    net::SessionClient client_dummy{};
    net::OrderManager* ptr_om_dummy = reinterpret_cast<net::OrderManager*>(0x5678);
    up_region->Bind(ptr_om_dummy, nullptr, &client_dummy);
    ui::ClientTooltipRegionWidget* ptr_region = up_region.get();
    ui::Ui::Instance().Root()->AddChild(std::move(up_region));

    ptr_region->MouseEntered();
    gfx::Viewport::int8_last_move_run_time = 0;
    ptr_container->DrawOuter();
    Check(ptr_container->OwnedChildren().size() == 1, "ClientTooltipRegion 懒装载");
    ptr_region->MouseExited();
    Check(ptr_container->OwnedChildren().empty(), "ClientTooltipRegion 退悬移除");
    ui::Ui::Instance().Root()->RemoveChildren();
  }

  {
    const auto dir_version =
        std::filesystem::temp_directory_path() / "ora_ui_test_batch4";
    std::filesystem::create_directories(dir_version);
    WriteLayout(dir_version, "version.yaml",
                "Label@VERSION_LABEL:\n\tLogic: VersionLabelLogic\n\tX: 0\n\tY: 0\n"
                "\tWidth: 100\n\tHeight: 20\n\tFont: Regular\n");
    fs::FileSystem fs_version;
    fs_version.Mount(dir_version.generic_string());
    const std::vector<std::string> vec_version{"version.yaml"};
    ui::WidgetLoader loader_version(vec_version, fs_version);

    ui::WidgetArgs args_version;
    args_version.Add("modData", static_cast<game::ModData*>(&mod_data));
    args_version.Add("windowSize", int2{1280, 960});
    ui::Widget* ptr_version = loader_version.LoadWidget(args_version, nullptr, "VERSION_LABEL");
    Check(ptr_version != nullptr && ptr_version->TypeName() == "LabelWidget",
          "VERSION_LABEL 根类型");
    ui::LabelWidget* ptr_label_version = static_cast<ui::LabelWidget*>(ptr_version);
    Check(ptr_label_version->fn_get_text() == mod_data.ManifestRef().Metadata().str_version,
          "VersionLabel 文本 = Manifest.Version");
  }
}

void TestChromeWidgetsBatch5(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome) {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  {
    ui::WidgetArgs args_mixer;
    args_mixer.Add("modData", static_cast<game::ModData*>(&mod_data));
    args_mixer.Add("windowSize", int2{1280, 960});
    auto up_mixer = std::make_unique<ui::ColorMixerWidget>(args_mixer);
    ui::ColorMixerWidget& mixer_test = *up_mixer;
    mixer_test.Initialize(args_mixer);
    mixer_test.bounds_widget = {10, 10, 100, 100};
    Check(mixer_test.mod_rules == &mod_data.DefaultRules(),
          "Mixer modRules = ModData.DefaultRules");
    Check(mixer_test.str_click_sound == "ClickSound", "Mixer ClickSound 指标");

    int int4_mixer_change = 0;
    mixer_test.OnChangeAdd([&int4_mixer_change] { ++int4_mixer_change; });
    mixer_test.SetColorLimits(0.3f, 0.95f, 0.3f, 0.95f);
    Check(int4_mixer_change == 0, "SetColorLimits 不触发 OnChange");

    MouseInput input_mixer{.Event = MouseInputEvent::Down,
                           .Button = MouseButton::Left,
                           .Location = int2{60, 60}};
    Check(mixer_test.HandleMouseInput(input_mixer), "Mixer Down 消费");
    const float fp4_expected_s =
        0.3f + (0.95f - 0.3f) * (1.0f - 50.0f / 100.0f);
    const float fp4_expected_v = 0.3f + (0.95f - 0.3f) * (50.0f / 100.0f);
    Check(mixer_test.fp4_s == fp4_expected_s && mixer_test.fp4_v == fp4_expected_v,
          "Mixer 中心点 S/V = 0.625");
    Check(mixer_test.ColorValue() ==
              core::Color::FromAhsv(0.0f, fp4_expected_s, fp4_expected_v),
          "Mixer Color = FromAhsv(H,S,V)");
    Check(int4_mixer_change == 1, "Down 触发 OnChange");

    const int2 int2_px = mixer_test.MixerPxFromValue();
    const int int4_expected_x = static_cast<int>(
        100.0f * (fp4_expected_v - 0.3f) / (0.95f - 0.3f));
    const int int4_expected_y = static_cast<int>(
        100.0f * (1.0f - (fp4_expected_s - 0.3f) / (0.95f - 0.3f)));
    Check(int2_px == int2{int4_expected_x, int4_expected_y}, "MixerPxFromValue 往返");

    input_mixer.Event = MouseInputEvent::Move;
    input_mixer.Location = int2{110, 10};
    Check(mixer_test.HandleMouseInput(input_mixer), "Mixer Move 消费");
    Check(mixer_test.fp4_v == 0.95f && int4_mixer_change == 2,
          "Move 到右缘 V 钳上界");
    input_mixer.Event = MouseInputEvent::Up;
    mixer_test.HandleMouseInput(input_mixer);

    mixer_test.Set(core::Color::FromAhsv(0.0f, 1.0f, 1.0f));
    Check(mixer_test.fp4_s == 0.95f && mixer_test.fp4_v == 0.95f &&
              int4_mixer_change == 3,
          "Set 的 S/V 钳域");
    mixer_test.Set(core::Color::FromAhsv(0.0f, 1.0f, 1.0f));
    Check(int4_mixer_change == 4 && mixer_test.fp4_s == 0.95f,
          "越域 Set 恒变更再通知(上游比输入非钳值)");
  }

  {
    ui::WidgetArgs args_hue;
    args_hue.Add("windowSize", int2{1280, 960});
    auto up_hue = std::make_unique<ui::HueSliderWidget>(args_hue);
    ui::HueSliderWidget& hue_test = *up_hue;
    hue_test.Initialize(args_hue);
    hue_test.bounds_widget = {0, 0, 100, 20};
    Check(hue_test.fp4_value == 0.0f, "HueSlider 初值 0");

    int int4_hue_change = 0;
    hue_test.fn_on_change = [&int4_hue_change](float) { ++int4_hue_change; };
    MouseInput input_hue{.Event = MouseInputEvent::Down,
                         .Button = MouseButton::Left,
                         .Location = int2{60, 10}};
    Check(hue_test.HandleMouseInput(input_hue), "HueSlider thumb 命中消费");
    const float fp4_expected =
        1.0f * (60.0f - 0.5f * 20.0f) / (100.0f - 20.0f);
    Check(hue_test.fp4_value == fp4_expected && int4_hue_change == 1,
          "HueSlider 值 = 0.625");
    input_hue.Event = MouseInputEvent::Up;
    hue_test.HandleMouseInput(input_hue);

    hue_test.Removed();
    hue_test.Removed();
    Check(true, "HueSlider Removed 幂等(RAII sheet)");
  }

  {
    std::vector<core::Color> vec_custom_stored;
    auto deps_picker = std::make_shared<ui::Ui::Deps>();
    deps_picker->ptr_mod_data = &mod_data;
    deps_picker->ptr_chrome_metrics = &metrics_chrome;
    deps_picker->fn_window_size = [] { return int2{1280, 960}; };
    deps_picker->fn_clock = [] { return std::int64_t{0}; };
    deps_picker->fn_get_custom_colors = [&vec_custom_stored] {
      return vec_custom_stored;
    };
    deps_picker->fn_store_custom_colors =
        [&vec_custom_stored](const std::vector<core::Color>& vec_in) {
          vec_custom_stored = vec_in;
        };
    ui::Ui::Instance().Initialize(*deps_picker);

    const auto dir_picker =
        std::filesystem::temp_directory_path() / "ora_ui_test_batch5";
    std::filesystem::create_directories(dir_picker);
    const std::string str_tree =
        "Background@COLOR_CHOOSER:\n"
        "    Logic: ColorPickerLogic\n"
        "        PaletteColumns: 8\n"
        "        PalettePresetRows: 2\n"
        "        PaletteCustomRows: 1\n"
        "    Width: 326\n"
        "    Height: 154\n"
        "    Children:\n"
        "        Button@RANDOM_BUTTON:\n"
        "            Width: 76\n"
        "            Height: 25\n"
        "        Button@STORE_BUTTON:\n"
        "            Width: 76\n"
        "            Height: 25\n"
        "        Button@MIXER_TAB_BUTTON:\n"
        "            Width: 80\n"
        "            Height: 25\n"
        "        Button@PALETTE_TAB_BUTTON:\n"
        "            Width: 80\n"
        "            Height: 25\n"
        "        Container@MIXER_TAB:\n"
        "            Width: 236\n"
        "            Height: 120\n"
        "            Children:\n"
        "                HueSlider@HUE_SLIDER:\n"
        "                    Width: 232\n"
        "                    Height: 13\n"
        "                    Ticks: 5\n"
        "                ColorMixer@MIXER:\n"
        "                    Width: 232\n"
        "                    Height: 90\n"
        "        Background@PALETTE_TAB:\n"
        "            Visible: false\n"
        "            Width: 236\n"
        "            Height: 120\n"
        "            Children:\n"
        "                Container@PALETTE_TAB_PANEL:\n"
        "                    Width: 236\n"
        "                    Height: 120\n"
        "                    Children:\n"
        "                        Container@PRESET_AREA:\n"
        "                            Width: 232\n"
        "                            Children:\n"
        "                                ColorBlock@COLORPRESET:\n"
        "                                    Width: 29\n"
        "                                    Height: 29\n"
        "                                    Visible: false\n"
        "                        Container@CUSTOM_AREA:\n"
        "                            Width: 232\n"
        "                            Children:\n"
        "                                ColorBlock@COLORCUSTOM:\n"
        "                                    Width: 29\n"
        "                                    Height: 29\n"
        "                                    Visible: false\n";
    WriteLayout(dir_picker, "colorpicker.yaml", str_tree);
    fs::FileSystem fs_picker;
    fs_picker.Mount(dir_picker.generic_string());
    const std::vector<std::string> vec_picker{"colorpicker.yaml"};
    ui::WidgetLoader loader_picker(vec_picker, fs_picker);

    sim::World world_picker(sim::WorldSimParams{});
    const core::Color color_initial =
        core::Color::FromArgb(255, 0x39, 0x1D, 0x1D);
    std::vector<core::Color> vec_changed;
    int int4_extra_logic = 0;
    ui::Widget* ptr_extra_widget = nullptr;

    ui::WidgetArgs args_picker;
    args_picker.Add("modData", static_cast<game::ModData*>(&mod_data));
    args_picker.Add("world", static_cast<sim::World*>(&world_picker));
    args_picker.Add("initialColor", color_initial);
    args_picker.Add(
        "onChange",
        std::function<void(core::Color)>{[&vec_changed](core::Color color_c) {
          vec_changed.push_back(color_c);
        }});
    args_picker.Add(
        "extraLogic",
        std::function<void(ui::Widget*)>{
            [&int4_extra_logic, &ptr_extra_widget](ui::Widget* ptr_w) {
              ++int4_extra_logic;
              ptr_extra_widget = ptr_w;
            }});
    ui::Widget* ptr_root =
        loader_picker.LoadWidget(args_picker, nullptr, "COLOR_CHOOSER");

    Check(int4_extra_logic == 1 && ptr_extra_widget == ptr_root,
          "extraLogic(widget) 恰一次");
    Check(!vec_changed.empty() && vec_changed.back() == color_initial,
          "末次 onChange = initialColor");

    ui::ColorMixerWidget* ptr_mixer = ptr_root->Get<ui::ColorMixerWidget>("MIXER");
    ui::HueSliderWidget* ptr_hue = ptr_root->Get<ui::HueSliderWidget>("HUE_SLIDER");
    ui::ContainerWidget* ptr_preset_area =
        ptr_root->Get<ui::ContainerWidget>("PRESET_AREA");
    ui::ContainerWidget* ptr_custom_area =
        ptr_root->Get<ui::ContainerWidget>("CUSTOM_AREA");
    ui::ButtonWidget* ptr_palette_tab_button =
        ptr_root->Get<ui::ButtonWidget>("PALETTE_TAB_BUTTON");

    Check(ptr_preset_area->OwnedChildren().size() == 17,
          "预设区 = 模板 + 16 克隆");
    Check(ptr_custom_area->OwnedChildren().size() == 9, "自定义区 = 模板 + 8 克隆");
    auto* ptr_swatch_i1 =
        static_cast<ui::ColorBlockWidget*>(ptr_preset_area->OwnedChildren()[2].get());
    Check(ptr_swatch_i1->bounds_widget.int4_x == 29 &&
              ptr_swatch_i1->bounds_widget.int4_y == 0,
          "克隆网格 (i=1,j=0) X=29");

    ui::Widget* ptr_mixer_tab = ptr_root->Get("MIXER_TAB");
    ui::Widget* ptr_palette_tab = ptr_root->Get("PALETTE_TAB");    Check(ptr_mixer_tab->fn_is_visible() && !ptr_palette_tab->fn_is_visible(),
          "初始 mixer 页可见");

    vec_custom_stored = {core::Color::FromArgb(255, 10, 20, 30),
                         core::Color::FromArgb(255, 40, 50, 60)};
    const std::size_t sz_changed_before_store = vec_changed.size();
    ptr_root->Get<ui::ButtonWidget>("STORE_BUTTON")->fn_on_click();
    Check(vec_custom_stored.size() == 3 && vec_custom_stored.back() == ptr_mixer->ColorValue(),
          "STORE 追加 mixer 色");
    Check(ptr_palette_tab_button->fn_is_highlighted(),
          "STORE 后 palette 页闪烁高亮");
    ptr_root->LogicObjects().front()->Tick();
    ptr_root->LogicObjects().front()->Tick();
    ptr_root->LogicObjects().front()->Tick();
    ptr_root->LogicObjects().front()->Tick();
    Check(!ptr_palette_tab_button->fn_is_highlighted(), "四次 Tick 闪烁归零");
    ptr_root->Get<ui::ButtonWidget>("STORE_BUTTON")->fn_on_click();
    Check(vec_custom_stored.size() == 3 && vec_custom_stored.back() == ptr_mixer->ColorValue(),
          "重复 STORE 去重再追加(未满员不截)");
    (void)sz_changed_before_store;

    vec_custom_stored.clear();
    for (int int4_i = 0; int4_i < 8; ++int4_i)
      vec_custom_stored.push_back(
          core::Color::FromArgb(255, 10 + 8 * int4_i, 20, 30));
    ptr_root->Get<ui::ButtonWidget>("STORE_BUTTON")->fn_on_click();
    Check(vec_custom_stored.size() == 8 &&
              vec_custom_stored.front() == core::Color::FromArgb(255, 18, 20, 30) &&
              vec_custom_stored.back() == ptr_mixer->ColorValue(),
          "满员 STORE 保留尾 8");

    ptr_palette_tab_button->fn_on_click();
    Check(ptr_palette_tab->fn_is_visible() && !ptr_mixer_tab->fn_is_visible(),
          "palette 页切换");
    ptr_root->Get<ui::ButtonWidget>("MIXER_TAB_BUTTON")->fn_on_click();
    Check(ptr_mixer_tab->fn_is_visible(), "mixer 页切回");

    const core::Color color_preset0 =
        core::Color::FromArgb(255, 0x39, 0x1D, 0x1D);
    const auto [int4_a_p0, fp4_h_p0, fp4_s_p0, fp4_v_p0] = color_preset0.ToAhsv();
    (void)int4_a_p0;
    auto* ptr_swatch_p0 =
        static_cast<ui::ColorBlockWidget*>(ptr_preset_area->OwnedChildren()[1].get());
    ptr_swatch_p0->fn_on_mouse_up(MouseInput{});
    Check(ptr_hue->fp4_value == fp4_h_p0, "预设点击 hue = ToAhsv(preset).H");
    Check(ptr_mixer->fp4_v >= 0.29f && ptr_mixer->fp4_v < 0.31f,
          "预设 V 低于下界钳 0.3 邻域");

    auto* ptr_swatch_c0 =
        static_cast<ui::ColorBlockWidget*>(ptr_custom_area->OwnedChildren()[1].get());
    Check(ptr_swatch_c0->fn_is_visible(), "自定义槽 0 可见(两色已存)");
    const core::Color color_custom0 = ptr_swatch_c0->fn_get_color();
    const auto [int4_a_c0, fp4_h_c0, fp4_s_c0, fp4_v_c0] = color_custom0.ToAhsv();
    (void)int4_a_c0;
    (void)fp4_h_c0;
    Check(fp4_s_c0 >= 0.29f && fp4_s_c0 <= 0.96f && fp4_v_c0 >= 0.29f && fp4_v_c0 <= 0.96f,
          "自定义槽 MakeValid 入域");
    ptr_swatch_c0->fn_on_mouse_up(MouseInput{});
    Check(ptr_mixer->fp4_s >= 0.29f && ptr_mixer->fp4_s <= 0.96f &&
              ptr_mixer->fp4_v >= 0.29f && ptr_mixer->fp4_v <= 0.96f,
          "自定义槽点击 mixer 入域");

    ptr_root->Get<ui::ButtonWidget>("RANDOM_BUTTON")->fn_on_click();
    Check(ptr_mixer->fp4_s >= 0.29f && ptr_mixer->fp4_s <= 0.96f &&
              ptr_mixer->fp4_v >= 0.29f && ptr_mixer->fp4_v <= 0.96f,
          "RANDOM 的 S/V 入域");

    WriteLayout(dir_picker, "colorpicker_bad.yaml",
                std::string{str_tree}.replace(str_tree.find("PaletteColumns: 8"),
                                              std::string_view{"PaletteColumns: 8"}.size(),
                                              "PaletteColumns: abc"));
    fs::FileSystem fs_bad;
    fs_bad.Mount(dir_picker.generic_string());
    const std::vector<std::string> vec_bad{"colorpicker_bad.yaml"};
    ui::WidgetLoader loader_bad(vec_bad, fs_bad);
    ui::WidgetArgs args_bad;
    args_bad.Add("modData", static_cast<game::ModData*>(&mod_data));
    args_bad.Add("world", static_cast<sim::World*>(&world_picker));
    args_bad.Add("initialColor", color_initial);
    args_bad.Add("onChange",
                 std::function<void(core::Color)>{[](core::Color) {}});
    args_bad.Add("extraLogic",
                 std::function<void(ui::Widget*)>{[](ui::Widget*) {}});
    CheckThrow("Invalid value for PaletteColumns: abc", [&] {
      loader_bad.LoadWidget(args_bad, nullptr, "COLOR_CHOOSER");
    });
  }
}

void TestChromeWidgetsBatch6(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome) {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  gfx::ChromeProvider chrome_provider;
  chrome_provider.Initialize(
      {.ptr_file_system = &mod_data.ModFiles(),
       .vec_chrome_files = &mod_data.ManifestRef().Chrome(),
       .fp4_dpi_scale = 1.0f});

  gfx::SheetBuilder builder_fonts6(gfx::SheetType::BGRA, 128);
  constexpr auto fn_flat6 = [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
                               float) {};
  constexpr auto fn_quad6 = [](const gfx::Sprite&, core::Vector3, core::Vector3,
                               core::Vector3, core::Vector3, core::Vector3, float) {};
  auto up_font_tiny_bold = std::make_unique<gfx::SpriteFont>(
      "tinybold", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8,
      1.0f, builder_fonts6, fn_flat6, fn_quad6);
  auto up_font_regular6 = std::make_unique<gfx::SpriteFont>(
      "regular6", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8,
      1.0f, builder_fonts6, fn_flat6, fn_quad6);
  gfx::SpriteFont* ptr_font_tiny_bold = up_font_tiny_bold.get();
  gfx::SpriteFont* ptr_font_regular6 = up_font_regular6.get();

  auto deps_batch6 = std::make_shared<ui::Ui::Deps>();
  deps_batch6->ptr_mod_data = &mod_data;
  deps_batch6->fn_window_size = [] { return int2{1280, 960}; };
  deps_batch6->fn_clock = [] { return std::int64_t{0}; };
  deps_batch6->ptr_chrome_metrics = &metrics_chrome;
  deps_batch6->ptr_chrome_provider = &chrome_provider;
  deps_batch6->fn_load_widget = [&loader_chrome](ui::WidgetArgs& args_load,
                                                 ui::Widget* ptr_parent,
                                                 const std::string& str_id) {
    return loader_chrome.LoadWidget(args_load, ptr_parent, str_id);
  };
  deps_batch6->fn_try_get_font = [ptr_font_tiny_bold,
                                  ptr_font_regular6](const std::string& str_key) {
    if (str_key == "TinyBold")
      return ptr_font_tiny_bold;
    if (str_key == "Regular")
      return ptr_font_regular6;
    return static_cast<gfx::SpriteFont*>(nullptr);
  };
  ui::Ui::Instance().Initialize(*deps_batch6);

  Check(metrics_chrome.Get("SpawnFont") == "TinyBold", "metrics SpawnFont");
  Check(metrics_chrome.GetInt2("SpawnLabelOffset") == int2{0, 1},
        "metrics SpawnLabelOffset = 0,1");

  game::Game game6{game::Game::Deps{.ptr_mod_data = &mod_data}};
  game6.JoinLocal();
  map::MapCache cache6{mod_data.ManifestRef(), mod_data.ModFiles()};
  cache6.LoadMaps(mod_data);

  std::vector<std::byte> vec_rgba6;
  for (int int4_p = 0; int4_p < 16; int4_p++) {
    vec_rgba6.push_back(std::byte{0x80});
    vec_rgba6.push_back(std::byte{0xC0});
    vec_rgba6.push_back(std::byte{0xFF});
    vec_rgba6.push_back(std::byte{0xFF});
  }
  const fmt::Png png_mini6{vec_rgba6, gfx::SpriteFrameType::Rgba32, 4, 4};
  const std::vector<std::byte> vec_png6 = png_mini6.Save();
  gfx::Sheet sheet_mini6{gfx::SheetType::BGRA,
                         std::span<const std::byte>{vec_png6.data(), vec_png6.size()}};
  const gfx::Sprite sprite_mini6{sheet_mini6, Rectangle{0, 0, 4, 4},
                                 gfx::TextureChannel::RGBA};

  map::MapPreview* ptr_map_preview6 = nullptr;
  ui::Widget* ptr_hotkey6_root = nullptr;
  for (map::MapPreview* p6 : cache6.Previews())
    if (p6->Status() == map::MapStatus::Available && !p6->SpawnPoints().empty() &&
        p6->Preview() != nullptr && !p6->HideSpawnPreviews() &&
        p6->GridType().has_value() && p6->GridType() == MapGridType::Rectangular) {
      ptr_map_preview6 = p6;
      break;
    }
  Check(ptr_map_preview6 != nullptr, "a flat rectangular ra map with spawns");

  if (ptr_map_preview6 != nullptr) {
    cache6.SetMinimapMaterializer([&sprite_mini6](map::MapPreview& p_mini) {
      p_mini.SetMinimap(reinterpret_cast<std::uintptr_t>(&sprite_mini6));
    });
    ptr_map_preview6->GetMinimap();
    for (int int4_i = 0; int4_i < 200 && ptr_map_preview6->GetMinimap() == 0; ++int4_i)
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    Check(ptr_map_preview6->GetMinimap() ==
              reinterpret_cast<std::uintptr_t>(&sprite_mini6),
          "minimap handle materialized");

    net::SessionClient client6{};
    client6.Name = "Alice";
    client6.Team = 2;
    client6.Faction = "soviet";
    client6.SpawnPoint = 3;
    client6.Color = core::Color::FromArgb(255, 1, 2, 3);
    const ui::SpawnOccupant occupant6{client6};
    Check(occupant6.str_player_name == "Alice" && occupant6.int4_team == 2 &&
              occupant6.str_faction == "soviet" && occupant6.int4_spawn_point == 3,
          "SpawnOccupant(SessionClient) non-bot");

    net::SessionClient client_bot6{};
    client_bot6.Name = "no.such.bot.key";
    client_bot6.Bot = "ai";
    const ui::SpawnOccupant occupant_bot6{client_bot6};
    Check(occupant_bot6.str_player_name == "no.such.bot.key",
          "SpawnOccupant bot name = fluent key fallback");

    server::GameInformationPlayer player6{};
    player6.Name = "Bob";
    player6.IsBot = true;
    player6.FactionId = "allies";
    const ui::SpawnOccupant occupant_info6{player6};
    Check(occupant_info6.str_player_name == "Bob" && occupant_info6.str_faction == "allies",
          "SpawnOccupant(GameInformation.Player)");

    server::GameClientData game_client6{};
    game_client6.Name = "Carol";
    game_client6.Faction = "england";
    const ui::SpawnOccupant occupant_gc6{game_client6, true};
    Check(occupant_gc6.str_player_name == "Carol" && occupant_gc6.str_faction.empty(),
          "SpawnOccupant(GameClient, suppressFaction)");

    ui::WidgetArgs args_preview6;
    auto up_preview6 = std::make_unique<ui::MapPreviewWidget>();
    ui::MapPreviewWidget& widget_preview6 = *up_preview6;
    widget_preview6.Initialize(args_preview6);
    Check(widget_preview6.str_tooltip_template == "SPAWN_TOOLTIP",
          "MapPreview TooltipTemplate default");
    widget_preview6.bounds_widget = {10, 20, 50, 100};
    widget_preview6.fn_preview = [&ptr_map_preview6] { return ptr_map_preview6; };

    widget_preview6.UpdateMinimap(sprite_mini6);
    Check(widget_preview6.PreviewScale() == 12.5f, "previewScale = min(50/4, 100/4)");
    Check(widget_preview6.MapRect() == Rectangle{10, 45, 50, 50},
          "mapRect 居中(y = 20 + (100-50)/2)");
    Check(widget_preview6.Loaded(), "MapPreview Loaded");

    const Rectangle rect_map_bounds6 = ptr_map_preview6->Bounds();
    const int int4_origin_shift6 = rect_map_bounds6.Top() & 1;
    const CPos cell_origin6{rect_map_bounds6.Left(), rect_map_bounds6.Top()};
    Check(widget_preview6.ConvertToPreview(cell_origin6, MapGridType::Rectangular) ==
              int2{10 + int4_origin_shift6, 45},
          "ConvertToPreview 原点(奇行 +1)");
    const CPos cell_far6{rect_map_bounds6.Left() + 2, rect_map_bounds6.Top() + 2};
    Check(widget_preview6.ConvertToPreview(cell_far6, MapGridType::Rectangular) ==
              int2{10 + 25 + int4_origin_shift6, 45 + 25},
          "ConvertToPreview (+2,+2) = (+25,+25)");

    bool b_mouse_down6 = false;
    widget_preview6.fn_on_mouse_down = [&b_mouse_down6](const MouseInput&) {
      b_mouse_down6 = true;
    };
    MouseInput input_map6{.Location = int2{30, 70},
                          .Button = MouseButton::Left,
                          .Event = MouseInputEvent::Down};
    Check(widget_preview6.HandleMouseInput(input_map6), "MapPreview Down 消费");
    Check(b_mouse_down6, "Down 触发 OnMouseDown");
    input_map6.Event = MouseInputEvent::Move;
    Check(!widget_preview6.HandleMouseInput(input_map6), "Move 不消费");
    widget_preview6.b_ignore_mouse_input = true;
    input_map6.Event = MouseInputEvent::Down;
    Check(!widget_preview6.HandleMouseInput(input_map6), "IgnoreMouseInput 走基类");

    auto up_map6 = ptr_map_preview6->ToMap();
    Check(up_map6 != nullptr, "preview.ToMap constructs");
    if (up_map6 != nullptr) {
      const std::vector<std::byte> vec_saved6 = up_map6->SavePreview();
      Check(fmt::Png::Verify(vec_saved6), "SavePreview 输出 PNG 签名");
      const fmt::Png png_saved6{vec_saved6};
      Check(png_saved6.Width() == up_map6->Bounds().Width &&
                png_saved6.Height() == up_map6->Bounds().Height,
            "SavePreview 尺寸 = Bounds(ra 平面图)");
    }

    auto up_generated6 = std::make_unique<ui::GeneratedMapPreviewWidget>();
    ui::GeneratedMapPreviewWidget& widget_generated6 = *up_generated6;
    ui::WidgetArgs args_generated6;
    widget_generated6.Initialize(args_generated6);
    widget_generated6.b_show_spawn_points = false;
    widget_generated6.bounds_widget = {0, 0, 100, 100};
    widget_generated6.Update(*ptr_map_preview6);
    Check(widget_generated6.Loaded(), "GeneratedMapPreview Update(MapPreview) loaded");
    const int int4_pw6 = ptr_map_preview6->Preview()->Width();
    const int int4_ph6 = ptr_map_preview6->Preview()->Height();
    const float fp4_scale6 = std::min(100.0f / int4_pw6, 100.0f / int4_ph6);
    Check(widget_generated6.MapRect() ==
              Rectangle{(100 - static_cast<int>(fp4_scale6 * int4_pw6)) / 2,
                        (100 - static_cast<int>(fp4_scale6 * int4_ph6)) / 2,
                        static_cast<int>(fp4_scale6 * int4_pw6),
                        static_cast<int>(fp4_scale6 * int4_ph6)},
          "GeneratedMapPreview mapRect 方程");

    if (up_map6 != nullptr) {
      widget_generated6.Update(*up_map6, true);
      Check(widget_generated6.Loaded(), "GeneratedMapPreview Update(Map, force) loaded");
      Check(widget_generated6.MapRect().Width ==
                static_cast<int>(std::min(100.0f / up_map6->Bounds().Width,
                                          100.0f / up_map6->Bounds().Height) *
                                 up_map6->Bounds().Width),
            "Update(Map, force) 重算 mapRect");
    }

    widget_generated6.Clear();
    Check(!widget_generated6.Loaded(), "Clear 后未装载");
  }

  const auto dir_batch6 = std::filesystem::temp_directory_path() / "ora_ui_test_batch6";
  std::filesystem::create_directories(dir_batch6);
  WriteLayout(dir_batch6, "previews.yaml",
              "GeneratedMapPreview@GPREVIEW:\n\tX: 1\n\tY: 1\n"
              "\tWidth: WINDOW_WIDTH - 2\n\tHeight: 98\n\tShowSpawnPoints: false\n"
              "Container@ENTRY_ROOT:\n\tX: 0\n\tY: 0\n\tWidth: 640\n\tHeight: 480\n"
              "\tChildren:\n\t\tHotkeyEntry@HOTKEY_ENTRY:\n"
              "\t\t\tX: 15 + 200 + 5\n\t\t\tY: 20\n\t\t\tWidth: 300\n\t\t\tHeight: 25\n");
  fs::FileSystem fs_batch6;
  fs_batch6.Mount(dir_batch6.generic_string());
  const std::vector<std::string> vec_batch6{"previews.yaml"};
  ui::WidgetLoader loader_batch6(vec_batch6, fs_batch6);

  ui::WidgetArgs args_yaml6;
  args_yaml6.Add("windowSize", int2{1280, 960});
  ui::Widget* ptr_yaml_preview6 = loader_batch6.LoadWidget(args_yaml6, nullptr, "GPREVIEW");
  Check(ptr_yaml_preview6 != nullptr &&
            ptr_yaml_preview6->TypeName() == "GeneratedMapPreviewWidget",
        "GeneratedMapPreview 装载类型");
  auto* ptr_generated_yaml6 =
      dynamic_cast<ui::GeneratedMapPreviewWidget*>(ptr_yaml_preview6);
  Check(ptr_generated_yaml6 != nullptr &&
            ptr_generated_yaml6->bounds_widget.int4_x == 1 &&
            ptr_generated_yaml6->bounds_widget.int4_width == 1278,
        "GPREVIEW X=1/W=WINDOW_WIDTH-2");
  Check(ptr_generated_yaml6 != nullptr && !ptr_generated_yaml6->b_show_spawn_points,
        "GPREVIEW ShowSpawnPoints: false");

  ptr_hotkey6_root = loader_batch6.LoadWidget(args_yaml6, nullptr, "ENTRY_ROOT");

  ui::Widget* ptr_hotkey6 =
      ptr_hotkey6_root != nullptr ? ptr_hotkey6_root->GetOrNull<ui::Widget>("HOTKEY_ENTRY")
                                  : nullptr;
  Check(ptr_hotkey6 != nullptr && ptr_hotkey6->TypeName() == "HotkeyEntryWidget",
        "HOTKEY_ENTRY 装载类型");
  auto* ptr_entry6 = dynamic_cast<ui::HotkeyEntryWidget*>(ptr_hotkey6);
  Check(ptr_entry6 != nullptr && ptr_entry6->bounds_widget.int4_x == 220 &&
            ptr_entry6->bounds_widget.int4_width == 300,
        "HOTKEY_ENTRY X=220/W=300");
  Check(ptr_entry6->str_font == "Regular", "HotkeyEntry Font = HotkeyFont 指标");
  Check(ptr_entry6->color_text_invalid == core::Color::FromArgb(255, 255, 0, 0),
        "HotkeyColorInvalid = FF0000");

  if (ptr_entry6 != nullptr) {
    ptr_entry6->fn_is_valid = [] { return true; };

    MouseInput input_entry6{.Location = int2{220 + 150, 20 + 12},
                            .Button = MouseButton::Left,
                            .Event = MouseInputEvent::Down};
    Check(ptr_entry6->HandleMouseInput(input_entry6), "HotkeyEntry Down 取焦");
    Check(ptr_entry6->HasKeyboardFocus(), "HotkeyEntry 持焦");

    KeyInput input_esc6{};
    input_esc6.Event = KeyInputEvent::Down;
    input_esc6.Key = Keycode::ESCAPE;
    bool b_esc6 = false;
    ptr_entry6->fn_on_esc_key = [&b_esc6](const KeyInput&) { b_esc6 = true; };
    Check(ptr_entry6->HandleKeyPress(input_esc6), "ESC 键消费");
    Check(b_esc6, "ESC 触发 OnEscKey");
    Check(!ptr_entry6->HasKeyboardFocus(), "ESC 后让焦(有效)");

    Check(ptr_entry6->HandleMouseInput(input_entry6), "再次 Down 取焦");
    KeyInput input_a6{};
    input_a6.Event = KeyInputEvent::Down;
    input_a6.Key = Keycode::A;
    Check(ptr_entry6->HandleKeyPress(input_a6), "A 键消费");
    Check(ptr_entry6->key_hotkey.key_kind == Keycode::A &&
              ptr_entry6->key_hotkey.kind_modifiers == Modifiers::None,
          "Key = FromKeyInput(A)");
    Check(ptr_entry6->key_hotkey.DisplayString() == "A", "DisplayString A");
    Check(!ptr_entry6->HasKeyboardFocus(), "捕获后让焦");

    ptr_entry6->fn_is_valid = [] { return false; };
    Check(ptr_entry6->HandleMouseInput(input_entry6), "无效态仍可取焦");
    KeyInput input_q6{};
    input_q6.Event = KeyInputEvent::Down;
    input_q6.Key = Keycode::Q;
    Check(ptr_entry6->HandleKeyPress(input_q6), "Q 键消费");
    Check(ptr_entry6->key_hotkey.key_kind == Keycode::Q, "Key = Q");
    Check(ptr_entry6->HasKeyboardFocus(), "无效键 YieldKeyboardFocus 拒绝让焦");
    Check(ptr_entry6->ForceYieldKeyboardFocus(), "ForceYield 强制让焦");
    Check(!ptr_entry6->HasKeyboardFocus(), "ForceYield 后无焦");

    Check(ptr_entry6->HandleMouseInput(input_entry6), "blink 取焦");
    for (int int4_t = 0; int4_t < 15; int4_t++)
      ptr_entry6->Tick();
    Check(!ptr_entry6->ShowEntry(), "15 tick 翻相位");
    for (int int4_t = 0; int4_t < 15; int4_t++)
      ptr_entry6->Tick();
    Check(ptr_entry6->ShowEntry(), "再 15 tick 翻回");
    ptr_entry6->ForceYieldKeyboardFocus();
  }

  chrome_provider.Deinitialize();
}

void TestProductionPaletteBatch10() {
  using namespace ora;

  static ui::ChromeMetrics metrics_batch10;
  {
    const auto dir_metrics = std::filesystem::temp_directory_path() /
                             "ora_ui_test_batch10_metrics";
    std::filesystem::create_directories(dir_metrics);
    {
      std::ofstream out_metrics(dir_metrics / "metrics.yaml");
      out_metrics << "Chrome:\n";
      out_metrics << "\tButtonDepth: 1\n";
      out_metrics << "\tButtonFont: Regular\n";
      out_metrics << "\tButtonTextColor: FFFFFFFF\n";
      out_metrics << "\tButtonTextColorDisabled: 808080FF\n";
      out_metrics << "\tButtonTextContrast: True\n";
      out_metrics << "\tButtonTextShadow: False\n";
      out_metrics << "\tButtonTextContrastColorDark: 000000FF\n";
      out_metrics << "\tButtonTextContrastColorLight: FFFFFFFF\n";
      out_metrics << "\tButtonTextContrastRadius: 1\n";
      out_metrics << "\tClickSound: click_sound\n";
      out_metrics << "\tClickDisabledSound: click_disabled_sound\n";
      out_metrics << "\tButtonCursor: default\n";
      out_metrics << "\tWorldDefaultCursor: default\n";
      out_metrics << "\tInfiniteOffset: 4, 2\n";
    }
    fs::FileSystem fs_metrics;
    fs_metrics.Mount(dir_metrics.generic_string());
    const std::vector<std::string> vec_files_metrics{"metrics.yaml"};
    metrics_batch10.Initialize({&fs_metrics, &vec_files_metrics});

    ui::Ui::Deps deps_batch10;
    deps_batch10.ptr_chrome_metrics = &metrics_batch10;
    deps_batch10.fn_window_size = [] { return int2{1280, 720}; };
    ui::Ui::Instance().Initialize(std::move(deps_batch10));
  }

  // 无世界形态:字段装载 + 滚动算术 + tabs 空组面
  // The worldless form: the field loading + the scroll arithmetic + the
  // tabs' empty-group face.
  ui::WidgetArgs args_palette;
  mods::ProductionPaletteWidget widget_palette(args_palette);
  Check(widget_palette.LoadFieldOrProperty("Columns", "5") &&
            widget_palette.int4_columns == 5,
        "p6b10: palette Columns loads");
  Check(widget_palette.LoadFieldOrProperty("ReadyTextStyle", "Blinking") &&
            widget_palette.kind_ready_text_style ==
                mods::ReadyTextStyleOptions::Blinking,
        "p6b10: ReadyTextStyle Blinking loads");
  Check(widget_palette.LoadFieldOrProperty("QueuedTextAlign", "Center") &&
            widget_palette.kind_queued_text_align == mods::TextAlign::Center,
        "p6b10: QueuedTextAlign Center loads");
  Check(widget_palette.LoadFieldOrProperty("DrawTime", "False") &&
            !widget_palette.b_draw_time,
        "p6b10: DrawTime False loads");
  Check(widget_palette.LoadFieldOrProperty("IconSize", "64, 48") &&
            widget_palette.int2_icon_size == int2{64, 48},
        "p6b10: IconSize tuple loads");
  Check(widget_palette.LoadFieldOrProperty("TooltipTemplate",
                                           "PRODUCTION_TOOLTIP_CUSTOM") &&
            widget_palette.str_tooltip_template ==
                "PRODUCTION_TOOLTIP_CUSTOM",
        "p6b10: TooltipTemplate loads");
  ui::WidgetArgs args_palette_init;
  widget_palette.Initialize(args_palette_init);
  Check(widget_palette.CanScrollUp() == false && widget_palette.int4_icon_row_offset == 0,
        "p6b10: palette starts unscrolled");
  widget_palette.ScrollDown();
  Check(widget_palette.int4_icon_row_offset == 0,
        "p6b10: scroll down clamped with no icons");
  widget_palette.int4_total_icon_count = 13;
  widget_palette.int4_columns = 3;
  widget_palette.int4_max_icon_row_offset = 5;
  Check(widget_palette.CanScrollDown() == false,
        "p6b10: 13 icons / 3 cols / cap 5 rows cannot scroll (5 rows total)");
  widget_palette.int4_max_icon_row_offset = 1;
  Check(widget_palette.CanScrollDown() == true,
        "p6b10: 13 icons / 3 cols / cap 1 row can scroll");
  widget_palette.ScrollToTop();
  Check(widget_palette.int4_icon_row_offset == 0,
        "p6b10: ScrollToTop resets");
  auto widget_palette_clone = widget_palette.Clone();
  Check(widget_palette_clone->TypeName() ==
            std::string_view{"ProductionPaletteWidget"},
        "p6b10: palette clone type");
  Check(static_cast<mods::ProductionPaletteWidget*>(widget_palette_clone.get())
            ->int4_columns == 3,
        "p6b10: palette clone carries the loaded Columns field");

  ui::WidgetArgs args_tabs;
  mods::ProductionTabsWidget widget_tabs(args_tabs);
  Check(widget_tabs.GroupsForTest().empty(),
        "p6b10: worldless tabs gather no groups");
  Check(widget_tabs.LoadFieldOrProperty("TabWidth", "30") &&
            widget_tabs.LoadFieldOrProperty("DecorationScrollLeft",
                                            "scroll-left") &&
            widget_tabs.LoadFieldOrProperty("TabColorDone", "FFD700FF"),
        "p6b10: tabs fields load");
  ui::WidgetArgs args_tabs_init;
  widget_tabs.Initialize(args_tabs_init);
  Check(widget_tabs.HandleKeyPress(KeyInput{
            .Event = KeyInputEvent::Down,
            .Key = Keycode::A,
            .ModifierFlags = Modifiers::None,
            .MultiTapCount = 0,
            .UnicodeChar = u'a',
            .IsRepeat = false}) == false,
        "p6b10: tabs keys unbound without hotkeys");
  auto widget_tabs_clone = widget_tabs.Clone();
  Check(widget_tabs_clone->TypeName() ==
            std::string_view{"ProductionTabsWidget"},
        "p6b10: tabs clone type");

  // ProductionTabGroup 纯逻辑(命名链续接 + 移除)
  // ProductionTabGroup's pure logic (the naming-chain continuation + the
  // removal).
  mods::ProductionTabGroup group;
  group.group = "Building";
  mods::ProductionTab tab_keep{.name = "2", .queue = nullptr};
  mods::ProductionTab tab_stale{.name = "5", .queue = nullptr};
  group.vec_tabs = {tab_keep, tab_stale};
  group.Update({});
  Check(group.vec_tabs.empty(),
        "p6b10: every tab goes stale against an empty queue set");
  Check(group.int4_next_queue_name == 1,
        "p6b10: stale names leave NextQueueName at 1 (kept-tab naming only)");

  ui::WidgetArgs args_type_button;
  mods::ProductionTypeButtonWidget widget_type_button(args_type_button);
  Check(widget_type_button.LoadFieldOrProperty("ProductionGroup",
                                               "Vehicle") &&
            widget_type_button.ProductionGroup() == "Vehicle",
        "p6b10: worldless type button ProductionGroup loads");
}

void TestInputAssemblyBatch9() {
  using namespace ora;

  GameSettingsFace face_settings;
  Check(face_settings.ResolveActionButton(MouseActionType::Contextual) ==
            MouseButton::Right,
        "p6b9: modern Contextual action = Right");
  Check(face_settings.ResolveActionButton(MouseActionType::ConfirmOrder) ==
            MouseButton::Right,
        "p6b9: modern ConfirmOrder action = Right");
  Check(face_settings.ResolveActionButton(MouseActionType::GlobalCommand) ==
            MouseButton::Left,
        "p6b9: modern GlobalCommand action = Left");
  face_settings.mouse_control_style = MouseControlStyle::Classic;
  Check(face_settings.ResolveActionButton(MouseActionType::Contextual) ==
            MouseButton::Left,
        "p6b9: classic Contextual action = Left");
  Check(face_settings.ResolveActionButton(MouseActionType::Contextual) ==
                MouseButton::Left
            ? face_settings.ResolveCancelButton(MouseActionType::Contextual) ==
                  MouseButton::Right
            : false,
        "p6b9: cancel mirrors action");

  const GameSettingsInputFace face_input_defaults{};
  Check(face_input_defaults.b_viewport_edge_scroll &&
            face_input_defaults.int4_viewport_edge_scroll_margin == 5 &&
            face_input_defaults.fp4_viewport_edge_scroll_step == 30.0f &&
            face_input_defaults.fp4_zoom_speed == 0.04f &&
            face_input_defaults.int4_selection_deadzone == 24 &&
            face_input_defaults.int4_mouse_scroll_deadzone == 8 &&
            face_input_defaults.kind_mouse_scroll == MouseScrollType::Joystick,
        "p6b9: GameSettingsInputFace upstream defaults");

  Check(HasModifier(Modifiers::Ctrl | Modifiers::Shift, Modifiers::Ctrl) &&
            !HasModifier(Modifiers::Ctrl, Modifiers::Ctrl | Modifiers::Shift),
        "p6b9: HasModifier exact-flag semantics");

  Check(ui::Ui::Instance().SelectionDeadzone() == 24,
        "p6b9: Ui SelectionDeadzone default 24");

  game::Game game_input{game::Game::Deps{}};
  game_input.JoinLocal();
  CapturingWidget widget_capture;
  ui::Ui::Instance().SetKeyboardFocusWidget(&widget_capture);
  ui::DefaultInputHandler handler_input(game_input, nullptr);
  handler_input.ModifierKeys(Modifiers::Ctrl);
  Check(game_input.GetModifierKeys() == Modifiers::Ctrl,
        "p6b9: DefaultInputHandler ModifierKeys forwards to Game");
  const int int4_keys_before = widget_capture.int4_key_presses;
  handler_input.OnKeyInput(KeyInput{.Event = KeyInputEvent::Down,
                                    .Key = Keycode::A,
                                    .ModifierFlags = Modifiers::None,
                                    .MultiTapCount = 0,
                                    .UnicodeChar = u'a',
                                    .IsRepeat = false});
  Check(widget_capture.int4_key_presses == int4_keys_before + 1,
        "p6b9: DefaultInputHandler OnKeyInput routes through Ui focus");
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);

  mods::RegisterCommonTraits();

  ui::WidgetArgs args_wic;
  args_wic.Add("world", static_cast<sim::World*>(nullptr));
  args_wic.Add("worldRenderer", static_cast<gfx::WorldRenderer*>(nullptr));
  auto up_wic = ui::WidgetTypeRegistry::Instance().Create(
      "WorldInteractionControllerWidget", args_wic);
  Check(up_wic != nullptr &&
            up_wic->TypeName() ==
                std::string_view{"WorldInteractionControllerWidget"},
        "p6b9: WIC registry creation");
  Check(up_wic->Clone()->TypeName() ==
            std::string_view{"WorldInteractionControllerWidget"},
        "p6b9: WIC clone type");

  ui::WidgetArgs args_vcw;
  args_vcw.Add("world", static_cast<sim::World*>(nullptr));
  args_vcw.Add("worldRenderer", static_cast<gfx::WorldRenderer*>(nullptr));
  auto up_vcw = ui::WidgetTypeRegistry::Instance().Create(
      "ViewportControllerWidget", args_vcw);
  Check(up_vcw != nullptr &&
            up_vcw->TypeName() ==
                std::string_view{"ViewportControllerWidget"},
        "p6b9: VCW registry creation");
  Check(up_vcw->Clone()->TypeName() ==
            std::string_view{"ViewportControllerWidget"},
        "p6b9: VCW clone type");

  auto* ptr_vcw = dynamic_cast<mods::ViewportControllerWidget*>(up_vcw.get());
  Check(ptr_vcw != nullptr, "p6b9: VCW dynamic type");
  if (ptr_vcw != nullptr) {
    Check(ptr_vcw->LoadFieldOrProperty("ZoomInKey", "Z"), "p6b9: ZoomInKey load");
    Check(ptr_vcw->LoadFieldOrProperty("BookmarkSaveKeyPrefix", "Save"),
          "p6b9: BookmarkSaveKeyPrefix load");
    Check(ptr_vcw->LoadFieldOrProperty("BookmarkRestoreKeyPrefix", "Restore"),
          "p6b9: BookmarkRestoreKeyPrefix load");
    Check(ptr_vcw->LoadFieldOrProperty("BookmarkKeyCount", "2") &&
              ptr_vcw->int4_bookmark_key_count == 2,
          "p6b9: BookmarkKeyCount load");
    Check(ptr_vcw->LoadFieldOrProperty("TooltipContainer",
                                       "TOOLTIP_CONTAINER"),
          "p6b9: TooltipContainer load");
  }
}

void TestPreviewWidgetsBatch8() {
  const auto dir_layout =
      std::filesystem::temp_directory_path() / "ora_ui_test_preview8";
  std::filesystem::create_directories(dir_layout);
  WriteLayout(dir_layout, "preview8.yaml",
              "Container@ROOT:\n\tChildren:\n"
              "\t\tActorPreview@APREV:\n\t\t\tX: 1\n\t\t\tWidth: 32\n"
              "\t\tResourcePreview@RPREV:\n\t\t\tX: 2\n");
  WriteLayout(dir_layout, "terrain8.yaml",
              "Container@ROOT:\n\tChildren:\n"
              "\t\tTerrainTemplatePreview@TPREV:\n\t\t\tX: 3\n");

  fs::FileSystem fs_preview8;
  fs_preview8.Mount(dir_layout.generic_string());

  sim::World world_preview8(sim::WorldSimParams{});
  gfx::WorldRenderer wr_preview8{world_preview8, gfx::WorldRenderer::Desc{}};

  const std::vector<std::string> vec_preview8{"preview8.yaml"};
  ui::WidgetLoader loader_preview8(vec_preview8, fs_preview8);
  ui::WidgetArgs args_preview8;
  args_preview8.Add("worldRenderer", static_cast<gfx::WorldRenderer*>(&wr_preview8));
  args_preview8.Add("world", static_cast<sim::World*>(&world_preview8));
  ui::ContainerWidget root_preview8;
  ui::Widget* ptr_root8 =
      loader_preview8.LoadWidget(args_preview8, &root_preview8, "ROOT");
  Check(ptr_root8 != nullptr, "p6b8 loader root");

  ui::ActorPreviewWidget* ptr_aprev =
      ptr_root8->Get<ui::ActorPreviewWidget>("APREV");
  Check(ptr_aprev != nullptr, "p6b8 ActorPreview loaded");
  if (ptr_aprev != nullptr) {
    Check(ptr_aprev->bounds_widget.int4_x == 1, "p6b8 ActorPreview X=1");
    Check(ptr_aprev->PreviewOffset() == int2{0, 0} &&
              ptr_aprev->IdealPreviewSize() == int2{0, 0},
          "p6b8 ActorPreview empty before SetPreview");
    ptr_aprev->Tick();
    auto up_clone = ptr_aprev->Clone();
    Check(up_clone->TypeName() == std::string_view{"ActorPreviewWidget"},
          "p6b8 ActorPreview clone");
  }

  ui::ResourcePreviewWidget* ptr_rprev =
      ptr_root8->Get<ui::ResourcePreviewWidget>("RPREV");
  Check(ptr_rprev != nullptr, "p6b8 ResourcePreview loaded");
  if (ptr_rprev != nullptr) {
    ptr_rprev->SetResourceType("ore");
    Check(ptr_rprev->IdealPreviewSize() == Size{0, 0},
          "p6b8 ResourcePreview mapless fallback 0");
    ptr_rprev->fp4_scale = 2.0f;
    ptr_rprev->Draw();
  }

  const std::vector<std::string> vec_terrain8{"terrain8.yaml"};
  ui::WidgetLoader loader_terrain8(vec_terrain8, fs_preview8);
  ui::WidgetArgs args_terrain8;
  args_terrain8.Add("worldRenderer", static_cast<gfx::WorldRenderer*>(&wr_preview8));
  args_terrain8.Add("world", static_cast<sim::World*>(&world_preview8));
  ui::ContainerWidget root_terrain8;
  CheckThrow(
      "TerrainTemplatePreviewWidget requires a tile-based terrain renderer.",
      [&] {
        (void)loader_terrain8.LoadWidget(args_terrain8, &root_terrain8, "ROOT");
      });
}


void TestChromeWidgetsBatch7(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome) {
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  {
    game::PerfHistory& history_perf = game::PerfHistory::Instance();
    history_perf.Reset();
    game::PerfItem& item_a = history_perf.Item("b7-a");
    Check(item_a.C() == core::Color::FromArgb(255, 255, 0, 0), "PerfHistory 首项 Red");
    game::PerfItem& item_b = history_perf.Item("b7-b");
    Check(item_b.C() == core::Color::FromArgb(255, 0, 128, 0), "次项 Green");

    history_perf.Increment("b7-a", 2.0);
    history_perf.Increment("b7-a", 3.0);
    item_b.fp8_val = 7.0;
    history_perf.Tick(true);
    Check(item_a.LastValue() == 5.0 && item_a.fp8_val == 0.0,
          "Tick 入账 5 并清 Val");
    Check(item_b.LastValue() == 7.0, "b 项 7");
    Check(item_a.ActiveGameTotalAverage() == 5.0 &&
              item_a.Average(25) == 2.5,
          "平均域(Average 含 tail 槽的 0)");
    Check(history_perf.Item("b7-a").Samples().size() == 2 &&
              history_perf.Item("b7-a").Samples()[0] == 5.0 &&
              history_perf.Item("b7-a").Samples()[1] == 0.0,
          "Samples 倒序(含 tail 槽)");

    for (int int4_t = 0; int4_t < 150; ++int4_t)
      history_perf.Tick(false);
    Check(item_a.Samples().size() == 99, "环形稳态 99(tail 紧随)");
    Check(item_a.ActiveGameTotalSamplesCount() == 1, "gameIsActive=false 不积总");

    history_perf.Reset();
    Check(item_a.Samples().size() == 1 && item_a.Samples()[0] == 0.0 &&
              item_a.fp8_val == 0.0,
          "Reset 清样(回初始单槽)");
  }

  {
    const auto fn_fmt = [](double fp8_v) {
      std::string str_out;
      ui::PerfGraphWidget::SixCharacterFormatFloat(str_out, fp8_v);
      return str_out;
    };
    Check(fn_fmt(std::nan("")) == "NaN   ", "fmt NaN");
    Check(fn_fmt(std::numeric_limits<double>::infinity()) == "+Inf  " &&
              fn_fmt(-std::numeric_limits<double>::infinity()) == "-Inf  ", "fmt ±Inf");
    Check(fn_fmt(-0.000001) == "<0    ", "fmt 负值");
    Check(fn_fmt(0) == ".00000", "fmt 0");
    Check(fn_fmt(0.000004) == ".00000", "fmt 4e-6");
    Check(fn_fmt(0.000005) == ".00001", "fmt 5e-6 进位");
    Check(fn_fmt(0.0001) == ".00010", "fmt 1e-4");
    Check(fn_fmt(0.5) == ".50000", "fmt 0.5");
    Check(fn_fmt(1) == "1.0000", "fmt 1");
    Check(fn_fmt(9.00005) == "9.0001", "fmt 9.00005 进位");
    Check(fn_fmt(10) == "10.000", "fmt 10");
    Check(fn_fmt(99.0005) == "99.001", "fmt 99.0005");
    Check(fn_fmt(100) == "100.00", "fmt 100");
    Check(fn_fmt(123.4321) == "123.43", "fmt 123.4321");
    Check(fn_fmt(999.005) == "999.01", "fmt 999.005");
    Check(fn_fmt(1000) == "1000.0", "fmt 1000");
    Check(fn_fmt(9999.04999999) == "9999.0", "fmt 9999.049");
    Check(fn_fmt(9999.05000001) == "9999.1", "fmt 9999.05 进位");
    Check(fn_fmt(10000) == "10000.", "fmt 10000");
    Check(fn_fmt(99999.9) == "100000", "fmt 99999.9 进六位");
    Check(fn_fmt(100000) == "100000", "fmt 100000");
    Check(fn_fmt(999999) == "999999", "fmt 999999");
    Check(fn_fmt(1000000) == "TooBig", "fmt 1e6 TooBig");
    Check(fn_fmt(1234321) == "TooBig", "fmt 1234321 TooBig");
  }

  Check(ui::FormatCurrent("{0}", 1234.5) == "1234.5", "FormatCurrent 1234.5");
  Check(ui::FormatCurrent("v={0}x", 42.0) == "v=42x", "FormatCurrent 前后缀");
  Check(ui::FormatCurrent("{0}", 0.5) == "0.5", "FormatCurrent 0.5");
  {
    ui::EWMA lerp_ewma{0.3f};
    Check(lerp_ewma.Update(100.0f) == 100.0f, "EWMA 首值直通");
    Check(lerp_ewma.Update(0.0f) == 70.0f, "EWMA 0.3 步");
    Check(lerp_ewma.Update(0.0f) == 49.0f, "EWMA 链");
  }

  {
    game::Settings settings_debug;
    const game::SettingsModule* ptr_module = settings_debug.Module("Debug");
    Check(ptr_module != nullptr, "Settings Debug 段装配");
    const meta::GeneratedRecord& record_debug = ptr_module->Record();
    const auto& vec_debug = record_debug.Values();
    Check(std::get<std::int64_t>(vec_debug[2].val) == 25, "Debug Samples 默认 25");
    Check(std::get<bool>(vec_debug[1].val) == false, "PerfGraph 默认 false");
    Check(std::get<bool>(vec_debug[3].val) == true, "CheckVersion 默认 true");
    Check(std::get<float>(vec_debug[12].val) == 1.0f, "LongTickThresholdMs 默认 1");
    const std::string& str_uuid = std::get<std::string>(vec_debug[6].val);
    Check(str_uuid.size() == 36 && str_uuid[8] == '-' && str_uuid[13] == '-' &&
              str_uuid[18] == '-' && str_uuid[23] == '-',
          "UUID 36 位连字符形");
  }

  gfx::ChromeProvider chrome_provider7;
  chrome_provider7.Initialize(
      {.ptr_file_system = &mod_data.ModFiles(),
       .vec_chrome_files = &mod_data.ManifestRef().Chrome(),
       .fp4_dpi_scale = 1.0f});

  gfx::SheetBuilder builder_fonts7(gfx::SheetType::BGRA, 128);
  constexpr auto fn_flat7 = [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
                               float) {};
  constexpr auto fn_quad7 = [](const gfx::Sprite&, core::Vector3, core::Vector3,
                               core::Vector3, core::Vector3, core::Vector3, float) {};
  auto up_font_tiny7 = std::make_unique<gfx::SpriteFont>(
      "tiny7", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8,
      1.0f, builder_fonts7, fn_flat7, fn_quad7);
  auto up_font_regular7 = std::make_unique<gfx::SpriteFont>(
      "regular7", std::span<const std::uint8_t>{}, std::make_unique<FakeFont>(), 10, 8,
      1.0f, builder_fonts7, fn_flat7, fn_quad7);
  gfx::SpriteFont* ptr_font_tiny7 = up_font_tiny7.get();
  gfx::SpriteFont* ptr_font_regular7 = up_font_regular7.get();

  std::int64_t int8_clock7 = 0;
  auto deps_batch7 = std::make_shared<ui::Ui::Deps>();
  deps_batch7->ptr_mod_data = &mod_data;
  deps_batch7->fn_window_size = [] { return int2{1280, 960}; };
  deps_batch7->fn_clock = [&int8_clock7] { return int8_clock7; };
  deps_batch7->ptr_chrome_metrics = &metrics_chrome;
  deps_batch7->ptr_chrome_provider = &chrome_provider7;
  deps_batch7->fn_load_widget = [&loader_chrome](ui::WidgetArgs& args_load,
                                                 ui::Widget* ptr_parent,
                                                 const std::string& str_id) {
    return loader_chrome.LoadWidget(args_load, ptr_parent, str_id);
  };
  deps_batch7->fn_release_root = [&loader_chrome](ui::Widget* ptr_root) {
    return ptr_root != nullptr ? loader_chrome.ReleaseRoot(*ptr_root) : nullptr;
  };
  deps_batch7->fn_try_get_font = [ptr_font_tiny7,
                                  ptr_font_regular7](const std::string& str_key) {
    if (str_key == "Tiny")
      return ptr_font_tiny7;
    if (str_key == "Regular")
      return ptr_font_regular7;
    return static_cast<gfx::SpriteFont*>(nullptr);
  };
  ui::Ui::Instance().Initialize(*deps_batch7);

  {
    ui::Widget* ptr_chat_line =
        loader_chrome.LoadWidget(*(new ui::WidgetArgs()), nullptr, "CHAT_LINE_TEMPLATE");
    std::unique_ptr<ui::Widget> up_chat_line_owned =
        loader_chrome.ReleaseRoot(*ptr_chat_line);
    Check(ptr_chat_line != nullptr && ptr_chat_line->TypeName() == "ContainerWidget",
          "CHAT_LINE_TEMPLATE 真 ra 模板装载");
    const net::TextNotification notification_chat{
        net::TextNotificationPool::Chat, 1, std::string{"Alice"},
        std::string{"hello world"}, core::Color::FromArgb(255, 9, 8, 7), std::nullopt};
    ui::SetupTextNotification(*ptr_chat_line, notification_chat, 200, true);

    ui::LabelWidget* ptr_time = ptr_chat_line->Get<ui::LabelWidget>("TIME");
    ui::LabelWidget* ptr_prefix = ptr_chat_line->Get<ui::LabelWidget>("PREFIX");
    ui::LabelWidget* ptr_text = ptr_chat_line->Get<ui::LabelWidget>("TEXT");

    Check(ptr_time->fn_get_text().size() == 5 && ptr_time->fn_get_text()[2] == ':',
          "TIME = HH:MM");
    Check(ptr_time->bounds_widget.int4_x == 5 + 37 + 5,
          "timeOffset = TIME.Width + TIME.X");
    Check(ptr_prefix->fn_get_text() == "Alice:", "PREFIX = 'Alice:'");
    Check(ptr_prefix->fn_get_color() == core::Color::FromArgb(255, 9, 8, 7),
          "PREFIX 色 = PrefixColor");
    Check(ptr_prefix->bounds_widget.int4_x == 5 + 42,
          "PREFIX 随 timeOffset 右移");
    Check(ptr_text->bounds_widget.int4_x == 5 + 42 - 42 + 4 * 6 + 5,
          "TEXT X = X + prefixOffset - timeOffset");
    Check(ptr_text->fn_get_text() == "hello world", "TEXT 短文不换行");
    Check(ptr_text->fn_get_color() == ptr_text->color_text,
          "无 TextColor 回落默认");
    Check(ptr_chat_line->bounds_widget.int4_width == 200,
          "行宽 = boxWidth - X");

  }

  const auto dir_batch7 = std::filesystem::temp_directory_path() / "ora_ui_test_batch7";
  std::filesystem::create_directories(dir_batch7);
  WriteLayout(dir_batch7, "batch7.yaml",
              "Container@BATCH7_ROOT:\n"
              "\tWidth: 300\n"
              "\tHeight: 200\n"
              "\tChildren:\n"
              "\t\tTextNotificationsDisplay@CHAT_DISPLAY:\n"
              "\t\t\tWidth: 200\n"
              "\t\t\tHeight: 60\n"
              "\t\t\tLogLength: 3\n"
              "\t\t\tDisplayDurationMs: 100\n"
              "\t\t\tBottomSpacing: 3\n"
              "\t\tLineGraph@GRAPH:\n"
              "\t\t\tXAxisSize: 20\n"
              "\t\t\tXAxisTicksPerLabel: 5\n"
              "\t\t\tLabelFont: Tiny\n"
              "\t\t\tAxisFont: Tiny\n"
              "\t\t\tPadding: 7\n"
              "\t\t\tBackgroundColorDark: 101010\n"
              "\t\tScrollableLineGraph@SCROLL_GRAPH:\n"
              "\t\t\tScrollbarHeight: 12\n"
              "\t\t\tSmoothScrollSpeed: 0.25\n"
              "\t\t\tMinimumThumbWidth: 15\n"
              "\t\tPerfGraph@PERF\n"
              "\t\tResourceBar@RBAR:\n"
              "\t\t\tOrientation: Horizontal\n"
              "\t\t\tIndicatorCollection: lobby-bits\n"
              "\t\t\tIndicatorImage: huepicker\n"
              "\t\tMouseAttachment@MOUSE\n"
              "\t\tControlGroups@GROUPS:\n"
              "\t\t\tSelectGroupKeyPrefix: b7-select\n"
              "\t\tBadge@BADGE\n");
  fs::FileSystem fs_batch7;
  fs_batch7.Mount(dir_batch7.generic_string());
  const std::vector<std::string> vec_batch7{"batch7.yaml"};
  ui::WidgetLoader loader_batch7(vec_batch7, fs_batch7);

  ui::WidgetArgs args_batch7;
  args_batch7.Add("modData", static_cast<game::ModData*>(&mod_data));
  args_batch7.Add("chromeMetrics", static_cast<ui::ChromeMetrics*>(&metrics_chrome));
  args_batch7.Add("windowSize", int2{1280, 960});
  ui::Widget* ptr_root7 =
      loader_batch7.LoadWidget(args_batch7, nullptr, "BATCH7_ROOT");

  ui::TextNotificationsDisplayWidget* ptr_display =
      ptr_root7->Get<ui::TextNotificationsDisplayWidget>("CHAT_DISPLAY");
  Check(ptr_display != nullptr, "CHAT_DISPLAY 装载");
  Check(ptr_display->int4_display_duration_ms == 100 &&
            ptr_display->int4_log_length == 3 && ptr_display->int4_bottom_spacing == 3,
        "通知字段装载");

  const auto fn_add_chat = [&ptr_display](const char* str_prefix, const char* str_text) {
    ptr_display->AddNotification(net::TextNotification{
        net::TextNotificationPool::Chat, 1, std::string{str_prefix},
        std::string{str_text}, std::nullopt, std::nullopt});
  };

  fn_add_chat("A", "one");
  Check(ptr_display->OwnedChildren().size() == 1 &&
            ptr_display->OwnedChildren().back()->bounds_widget.int4_y == 60 - 16 - 3,
        "首行 Y = Height - 行高 - BottomSpacing");
  fn_add_chat("B", "two");
  Check(ptr_display->OwnedChildren().size() == 2 &&
            ptr_display->OwnedChildren()[0]->bounds_widget.int4_y ==
                60 - 16 - 3 - 20 &&
            ptr_display->OwnedChildren()[1]->bounds_widget.int4_y == 60 - 16 - 3,
        "第二行:旧行上移 20,新行落底");
  fn_add_chat("C", "three");
  fn_add_chat("D", "four");
  Check(ptr_display->OwnedChildren().size() == 3,
        "LogLength=3 首行淘汰");

  ptr_display->RemoveMostRecentNotification();
  Check(ptr_display->OwnedChildren().size() == 2 &&
            ptr_display->OwnedChildren()[0]->bounds_widget.int4_y == 21 &&
            ptr_display->OwnedChildren()[1]->bounds_widget.int4_y == 41,
        "RemoveMostRecent 回推 20");

  int8_clock7 = 99;
  ptr_display->Tick();
  Check(ptr_display->OwnedChildren().size() == 2, "99ms 未到期");
  int8_clock7 = 100;
  ptr_display->Tick();
  Check(ptr_display->OwnedChildren().empty(), "100ms 全部到期");

  ui::LineGraphWidget* ptr_graph =
      ptr_root7->Get<ui::LineGraphWidget>("GRAPH");
  Check(ptr_graph != nullptr && ptr_graph->int4_x_axis_size == 20 &&
            ptr_graph->int4_x_axis_ticks_per_label == 5 &&
            ptr_graph->str_label_font == "Tiny" && ptr_graph->int4_padding == 7 &&
            ptr_graph->color_background_dark == core::Color::FromArgb(255, 0x10, 0x10, 0x10),
        "LineGraph 字段装载");
  Check(ptr_graph->fn_get_series == nullptr, "GetSeries 未装 = null");
  ptr_graph->fn_get_series = [] {
    return std::vector<ui::LineGraphSeries>{
        ui::LineGraphSeries{std::string{"k1"}, core::Color::FromArgb(255, 1, 2, 3),
                            std::vector<float>{1.0f, 2.0f, 3.0f}}};
  };
  ptr_graph->Draw();
  std::unique_ptr<ui::Widget> up_graph_clone = ptr_graph->Clone();
  auto* ptr_graph_clone = static_cast<ui::LineGraphWidget*>(up_graph_clone.get());
  Check(ptr_graph_clone->int4_x_axis_size == 20 &&
            ptr_graph_clone->fn_get_series != nullptr,
        "LineGraph Clone 携字段与委托");

  ui::ScrollableLineGraphWidget* ptr_scroll_graph =
      ptr_root7->Get<ui::ScrollableLineGraphWidget>("SCROLL_GRAPH");
  Check(ptr_scroll_graph != nullptr && ptr_scroll_graph->int4_scrollbar_height == 12 &&
            ptr_scroll_graph->fp4_smooth_scroll_speed == 0.25f &&
            ptr_scroll_graph->int4_minimum_thumb_width == 15,
        "ScrollableLineGraph 字段装载");
  std::unique_ptr<ui::Widget> up_scroll_clone = ptr_scroll_graph->Clone();
  Check(static_cast<ui::ScrollableLineGraphWidget*>(up_scroll_clone.get())
                ->int4_scrollbar_height == 12,
        "ScrollableLineGraph Clone");

  ui::PerfGraphWidget widget_perf7;
  game::PerfHistory::Instance().Reset();
  game::PerfHistory::Instance().Increment("b7-perf", 1.5);
  widget_perf7.Draw();

  ui::ResourceBarWidget* ptr_rbar = ptr_root7->Get<ui::ResourceBarWidget>("RBAR");
  Check(ptr_rbar != nullptr &&
            ptr_rbar->kind_orientation == ui::ResourceBarOrientation::Horizontal,
        "ResourceBar Orientation 装载");
  Check(ptr_rbar->IndicatorSprite().ptr_sheet != nullptr,
        "Indicator = chrome lobby-bits/huepicker");
  ptr_rbar->fn_get_provided = [] { return 50.0f; };
  ptr_rbar->fn_get_used = [] { return 25.0f; };
  ptr_rbar->Draw();
  ptr_rbar->MouseEntered();
  ptr_rbar->MouseExited();

  ui::MouseAttachmentWidget* ptr_mouse =
      ptr_root7->Get<ui::MouseAttachmentWidget>("MOUSE");
  Check(ptr_mouse != nullptr && ptr_mouse->b_click_through, "MouseAttachment 装载");
  ptr_mouse->SetAttachment(int2{4, 5}, gfx::Sprite{}, std::string{"pal"});
  Check(ptr_mouse->ChildOrigin() == int2{4, 5}, "ChildOrigin = location");
  ptr_mouse->Reset();
  ptr_mouse->Draw();

  ui::ControlGroupsWidget* ptr_groups =
      ptr_root7->Get<ui::ControlGroupsWidget>("GROUPS");
  Check(ptr_groups != nullptr &&
            ptr_groups->str_select_group_key_prefix == "b7-select",
        "ControlGroups 前缀装载");
  KeyInput input_groups7{};
  input_groups7.Event = KeyInputEvent::Down;
  input_groups7.Key = Keycode::NUMBER_1;
  Check(!ptr_groups->HandleKeyPress(input_groups7), "空热键表不消费");

  ui::BadgeWidget* ptr_badge = ptr_root7->Get<ui::BadgeWidget>("BADGE");
  Check(ptr_badge != nullptr, "BadgeWidget 装载");
  ptr_badge->Draw();

  {
    yaml::StringPool pool_badge;
    const std::vector<yaml::MiniYamlNode> vec_badge_yaml = yaml::MiniYaml::FromString(
        "Badge:\n\tLabel: MyBadge\n\tIcon24: a24.png\n\tIcon48: b48.png\n",
        "badge", true, pool_badge);
    std::optional<ui::PlayerBadge> opt_badge_yaml =
        ui::PlayerDatabase::Instance().LoadBadge(vec_badge_yaml.front().Value);
    Check(opt_badge_yaml.has_value() && opt_badge_yaml->str_label == "MyBadge" &&
              opt_badge_yaml->opt_icon == std::optional<std::string>{"a24.png"} &&
              opt_badge_yaml->opt_icon3x == std::nullopt,
          "LoadBadge 四节点提取");

    const std::vector<yaml::MiniYamlNode> vec_badge_empty =
        yaml::MiniYaml::FromString("Badge:\n\tIcon24: x.png\n", "badge", true,
                                   pool_badge);
    Check(!ui::PlayerDatabase::Instance()
               .LoadBadge(vec_badge_empty.front().Value)
               .has_value(),
          "缺 Label = 空徽章");

    const gfx::Sprite sprite_icon1 =
        ui::PlayerDatabase::Instance().GetIcon(*opt_badge_yaml);
    const gfx::Sprite sprite_icon2 =
        ui::PlayerDatabase::Instance().GetIcon(*opt_badge_yaml);
    Check(sprite_icon1.ptr_sheet != nullptr &&
              sprite_icon1.Bounds == sprite_icon2.Bounds,
          "GetIcon 分配并缓存(空 RendererDeps = density 1)");
    ptr_badge->opt_badge = opt_badge_yaml;
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
  }

  chrome_provider7.Deinitialize();
}

}  // namespace


void TestRadarObserversBatch11() {
  using namespace ora;

  static ui::ChromeMetrics metrics_batch11;
  {
    const auto dir_metrics = std::filesystem::temp_directory_path() /
                             "ora_ui_test_batch11_metrics";
    std::filesystem::create_directories(dir_metrics);
    {
      std::ofstream out_metrics(dir_metrics / "metrics.yaml");
      out_metrics << "Chrome:\n";
      out_metrics << "\tTextContrastColorDark: 00000080\n";
      out_metrics << "\tTextContrastColorLight: FFFFFF80\n";
      out_metrics << "\tClickSound: click_sound\n";
      out_metrics << "\tWorldSelectCursor: select\n";
      out_metrics << "\tWorldDefaultCursor: default\n";
    }
    fs::FileSystem fs_metrics;
    fs_metrics.Mount(dir_metrics.generic_string());
    const std::vector<std::string> vec_files_metrics{"metrics.yaml"};
    metrics_batch11.Initialize({&fs_metrics, &vec_files_metrics});

    ui::Ui::Deps deps_batch11;
    deps_batch11.ptr_chrome_metrics = &metrics_batch11;
    deps_batch11.fn_window_size = [] { return int2{1280, 720}; };
    ui::Ui::Instance().Initialize(std::move(deps_batch11));
  }

  // VideoPlayerWidget:字段装载 + 合成 IVideo 的缩放方程
  // VideoPlayerWidget: field loading + the scale equation on a synthetic
  // IVideo.
  ui::WidgetArgs args_video;
  mods::VideoPlayerWidget widget_video(args_video);
  Check(widget_video.LoadFieldOrProperty("AspectRatio", "1.5") &&
            widget_video.fp4_aspect_ratio == 1.5f,
        "p6b11: VideoPlayer AspectRatio loads");
  Check(widget_video.LoadFieldOrProperty("Skippable", "False") &&
            !widget_video.b_skippable,
        "p6b11: VideoPlayer Skippable False loads");
  Check(widget_video.LoadFieldOrProperty("DrawOverlay", "False") &&
            !widget_video.b_draw_overlay,
        "p6b11: VideoPlayer DrawOverlay False loads");
  Check(widget_video.LoadFieldOrProperty("CancelKey", "ESCAPE Ctrl") &&
            widget_video.hk_cancel_key.key_kind == Keycode::ESCAPE,
        "p6b11: VideoPlayer CancelKey loads");
  auto widget_video_clone = widget_video.Clone();
  Check(widget_video_clone->TypeName() ==
            std::string_view{"VideoPlayerWidget"},
        "p6b11: VideoPlayer clone type");
  Check(widget_video.Video() == nullptr && widget_video.Paused(),
        "p6b11: VideoPlayer starts empty and paused");

  class FakeVideo final : public fmt::IVideo {
   public:
    std::uint16_t FrameCount() const override { return 10; }
    std::uint8_t Framerate() const override { return 10; }
    std::uint16_t Width() const override { return 320; }
    std::uint16_t Height() const override { return 200; }
    std::span<const std::byte> CurrentFrameData() const override {
      return vec_frame;
    }
    std::int32_t CurrentFrameIndex() const override { return 0; }
    void AdvanceFrame() override {}
    bool HasAudio() const override { return false; }
    std::span<const std::byte> AudioData() const override { return {}; }
    std::int32_t AudioChannels() const override { return 0; }
    std::int32_t SampleBits() const override { return 0; }
    std::int32_t SampleRate() const override { return 0; }
    void Reset() override {}

    std::vector<std::byte> vec_frame =
        std::vector<std::byte>(320 * 200 * 4, std::byte{7});
  };
  FakeVideo fake_video;
  widget_video.bounds_widget = {0, 0, 640, 480};
  widget_video.Play(std::shared_ptr<fmt::IVideo>(
      std::shared_ptr<fmt::IVideo>{}, &fake_video));
  Check(widget_video.Video() == &fake_video,
        "p6b11: VideoPlayer adopts the video");
  Check(widget_video.VideoSizeForTest().X == 512.0f &&
            widget_video.VideoSizeForTest().Y == 480.0f,
        "p6b11: 320x200@1.5 fits 640x480 as 512x480 (scale 1.6)");
  Check(widget_video.VideoOriginForTest().X == 64.0f &&
            widget_video.VideoOriginForTest().Y == 0.0f,
        "p6b11: centered origin lands at 64,0");
  widget_video.PlayThen([] {});
  Check(!widget_video.Paused(), "p6b11: PlayThen starts playback");
  widget_video.Pause();
  Check(widget_video.Paused(), "p6b11: Pause stops playback");
  widget_video.Stop();
  Check(widget_video.Paused() && widget_video.Video() == &fake_video,
        "p6b11: Stop resets without dropping the video");
  widget_video.CloseVideo();
  Check(widget_video.Video() == nullptr,
        "p6b11: CloseVideo drops the video");

  // RadarWidget:字段装载面(无世界形态不 Initialize —— sheet 面随世界)
  // RadarWidget: the field-loading face (no Initialize in the worldless
  // form — the sheet face rides a world).
  ui::WidgetArgs args_radar;
  mods::RadarWidget widget_radar(args_radar);
  Check(widget_radar.LoadFieldOrProperty("AnimationLength", "7") &&
            widget_radar.int4_animation_length == 7,
        "p6b11: Radar AnimationLength loads");
  Check(widget_radar.LoadFieldOrProperty("WorldInteractionController",
                                         "SIDEBAR_CONTROLLER") &&
            widget_radar.str_world_interaction_controller ==
                "SIDEBAR_CONTROLLER",
        "p6b11: Radar WorldInteractionController loads");
  Check(widget_radar.LoadFieldOrProperty("SoundUp", "beep_up") &&
            widget_radar.str_sound_up == "beep_up",
        "p6b11: Radar SoundUp loads");
  Check(widget_radar.LoadFieldOrProperty("ColorFog", "00000080") &&
            widget_radar.uint4_color_fog == 0x80000000u,
        "p6b11: Radar ColorFog parses to argb");
  Check(widget_radar.LoadFieldOrProperty("ColorShroud", "000000FF") &&
            widget_radar.uint4_color_shroud == 0xFF000000u,
        "p6b11: Radar ColorShroud parses to argb");
  widget_radar.fn_is_enabled = [] { return true; };
  widget_radar.fn_animating = [&](float) {};
  auto widget_radar_clone = widget_radar.Clone();
  Check(widget_radar_clone->TypeName() == std::string_view{"RadarWidget"},
        "p6b11: Radar clone type");

  // SupportPowersWidget/Timer/Observer 族:字段矩阵 + 空态 no-op
  // The SupportPowersWidget/Timer/Observer family: the field matrix +
  // the empty-state no-ops.
  ui::WidgetArgs args_powers;
  mods::SupportPowersWidget widget_powers(args_powers);
  Check(widget_powers.LoadFieldOrProperty("IconSize", "48, 36") &&
            widget_powers.int2_icon_size == int2{48, 36},
        "p6b11: SupportPowers IconSize loads");
  Check(widget_powers.LoadFieldOrProperty("IconMargin", "4") &&
            widget_powers.int4_icon_margin == 4,
        "p6b11: SupportPowers IconMargin loads");
  Check(widget_powers.LoadFieldOrProperty("Horizontal", "True") &&
            widget_powers.b_horizontal,
        "p6b11: SupportPowers Horizontal loads");
  Check(widget_powers.LoadFieldOrProperty("HotkeyPrefix", "sp") &&
            widget_powers.LoadFieldOrProperty("HotkeyCount", "3") &&
            widget_powers.int4_hotkey_count == 3,
        "p6b11: SupportPowers hotkey pair loads");
  Check(widget_powers.LoadFieldOrProperty("ClockPalette", "chromefoo") &&
            widget_powers.str_clock_palette == "chromefoo",
        "p6b11: SupportPowers ClockPalette loads");
  ui::WidgetArgs args_powers_init;
  widget_powers.Initialize(args_powers_init);
  widget_powers.Tick();
  Check(widget_powers.IconCount() == 0,
        "p6b11: SupportPowers worldless Tick stays empty");
  KeyInput key_probe{};
  key_probe.Event = KeyInputEvent::Down;
  Check(!widget_powers.HandleKeyPress(key_probe),
        "p6b11: SupportPowers empty icons ignore keys");
  MouseInput mi_probe{};
  mi_probe.Event = MouseInputEvent::Down;
  Check(widget_powers.HandleMouseInput(mi_probe),
        "p6b11: SupportPowers Down swallows with no icons");
  auto widget_powers_clone = widget_powers.Clone();
  Check(widget_powers_clone->TypeName() ==
            std::string_view{"SupportPowersWidget"},
        "p6b11: SupportPowers clone type");

  ui::WidgetArgs args_timer;
  mods::SupportPowerTimerWidget widget_timer(args_timer);
  Check(widget_timer.LoadFieldOrProperty("Font", "Tiny") &&
            widget_timer.str_font == "Tiny",
        "p6b11: Timer Font loads");
  Check(widget_timer.LoadFieldOrProperty("Align", "Right") &&
            widget_timer.kind_align == mods::TextAlign::Right,
        "p6b11: Timer Align Right loads");
  Check(widget_timer.LoadFieldOrProperty("Order", "Ascending") &&
            widget_timer.kind_order == mods::TimerOrder::Ascending,
        "p6b11: Timer Order Ascending loads");
  widget_timer.Tick();
  widget_timer.Draw();
  Check(widget_timer.TextsForTest().empty(),
        "p6b11: Timer worldless Tick draws nothing");
  auto widget_timer_clone = widget_timer.Clone();
  Check(widget_timer_clone->TypeName() ==
            std::string_view{"SupportPowerTimerWidget"},
        "p6b11: Timer clone type");

  ui::WidgetArgs args_army;
  mods::ObserverArmyIconsWidget widget_army(args_army);
  widget_army.int4_icon_width = 40;
  auto widget_army_clone = widget_army.Clone();
  Check(widget_army_clone->TypeName() ==
            std::string_view{"ObserverArmyIconsWidget"},
        "p6b11: ObserverArmy clone type");
  widget_army.fn_get_player = [] { return nullptr; };
  widget_army.Draw();
  widget_army.Tick();
  Check(widget_army.ArmyIconsForTest().empty(),
        "p6b11: ObserverArmy null player draws nothing");

  ui::WidgetArgs args_prod_icons;
  mods::ObserverProductionIconsWidget widget_prod_icons(args_prod_icons);
  auto widget_prod_clone = widget_prod_icons.Clone();
  Check(widget_prod_clone->TypeName() ==
            std::string_view{"ObserverProductionIconsWidget"},
        "p6b11: ObserverProduction clone type");
  widget_prod_icons.fn_get_player = [] { return nullptr; };
  widget_prod_icons.Draw();
  Check(widget_prod_icons.ProductionIconsForTest().empty(),
        "p6b11: ObserverProduction null player draws nothing");

  ui::WidgetArgs args_sp_icons;
  mods::ObserverSupportPowerIconsWidget widget_sp_icons(args_sp_icons);
  auto widget_sp_clone = widget_sp_icons.Clone();
  Check(widget_sp_clone->TypeName() ==
            std::string_view{"ObserverSupportPowerIconsWidget"},
        "p6b11: ObserverSupportPower clone type");
  widget_sp_icons.fn_get_player = [] { return nullptr; };
  widget_sp_icons.Draw();
  Check(widget_sp_icons.SupportPowerIconsForTest().empty(),
        "p6b11: ObserverSupportPower null player draws nothing");

  mods::SupportPowerIcon icon_default;
  Check(icon_default.ptr_power == nullptr && !icon_default.b_has_hotkey,
        "p6b11: SupportPowerIcon defaults");
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  TestActionQueue();
  TestWidgetArgs();
  TestMediator();
  TestWidgetTree();
  TestGameLoop();
  TestWidgetFocusInput();
  TestUiWindowStack();
  TestChromeMetrics();
  TestWidgetLoader();
  TestPreviewWidgetsBatch8();
  TestInputAssemblyBatch9();
  TestProductionPaletteBatch10();
  TestRadarObserversBatch11();

  // Phase 6 第二批(mod 根经 argv[1] 注入;缺省跳过 —— 与 platform_test 的
  // SKIP 门同形)
  // The Phase 6 batch two (the mod root arrives via argv[1]; skip when
  // absent — the same shape as platform_test's SKIP gate).
  TestHotkey();
  TestSpriteFont();
  if (argc > 1) {
    const std::string str_root{argv[1]};
    game::InstalledMods mods_keys{str_root + "/mods"};
    const game::Manifest* manifest_keys = mods_keys.Find("ra");
    Check(manifest_keys != nullptr, "ra mod found (hotkeys)");
    if (manifest_keys != nullptr) {
      game::ModData mod_keys{*manifest_keys, mods_keys, str_root};
          TestHotkeyManager(mod_keys);
          TestFontsModData(mod_keys);
    }
      TestBaseWidgetsChrome(str_root);
  } else
    std::println("SKIP: mod 根未提供(HotkeyManager/Fonts/chrome 冒烟)| SKIP: no mod root (HotkeyManager/Fonts/chrome smoke)");

  {
    game::Settings settings_sound;
    const game::SettingsModule* ptr_module =
        settings_sound.Module("Sound");
    Check(ptr_module != nullptr, "Settings Sound 段装配");
    if (ptr_module != nullptr) {
      const meta::GeneratedRecord& record_sound = ptr_module->Record();
      const auto& vec_sound = record_sound.Values();
      Check(vec_sound.size() == 9, "Sound 段九字段");
      Check(std::get<float>(vec_sound[0].val) == 0.5f, "SoundVolume 默认 0.5");
      Check(std::get<float>(vec_sound[1].val) == 0.5f, "MusicVolume 默认 0.5");
      Check(std::get<float>(vec_sound[2].val) == 0.5f, "VideoVolume 默认 0.5");
      Check(std::get<bool>(vec_sound[3].val) == false, "Shuffle 默认 false");
      Check(std::get<bool>(vec_sound[4].val) == false, "Repeat 默认 false");
      Check(std::get<std::string>(vec_sound[5].val).empty(), "Device 默认空");
      Check(std::get<bool>(vec_sound[6].val) == true, "CashTicks 默认 true");
      Check(std::get<bool>(vec_sound[7].val) == false, "Mute 默认 false");
      Check(std::get<bool>(vec_sound[8].val) == false,
            "MuteBackgroundMusic 默认 false");

      yaml::StringPool pool_sound;
      const std::vector<yaml::MiniYamlNode> vec_saved_default =
          settings_sound.Save(pool_sound);
      bool b_sound_section_present = false;
      for (const auto& node : vec_saved_default)
        if (node.Key != nullptr && *node.Key == "Sound")
          b_sound_section_present = true;
      Check(!b_sound_section_present,
            "Sound 段默认值全免存(空节点段过滤 = 上游 Where(Count > 0))");

      std::vector<yaml::MiniYamlNode> vec_override;
      vec_override.emplace_back(
          pool_sound.Intern("Sound"),
          yaml::MiniYaml{nullptr,
                         std::vector<yaml::MiniYamlNode>{
                             {pool_sound.Intern("CashTicks"), yaml::MiniYaml{pool_sound.Intern("False")}},
                             {pool_sound.Intern("SoundVolume"), yaml::MiniYaml{pool_sound.Intern("0.25")}}}});
      settings_sound.Load(vec_override, pool_sound);
      Check(std::get<bool>(record_sound.Values()[6].val) == false,
            "CashTicks 覆写为 false");
      Check(std::get<float>(record_sound.Values()[0].val) == 0.25f,
            "SoundVolume 覆写为 0.25");
    }
    ui::Ui::Deps deps_cash_ticks;
    Check(deps_cash_ticks.fn_cash_ticks(), "fn_cash_ticks 缺省 = 上游默认 true");
  }

  {
    game::Settings settings_game20;
    const game::SettingsModule* ptr_module_game =
        settings_game20.Module("Game");
    Check(ptr_module_game != nullptr, "Settings Game 段装配");
    if (ptr_module_game != nullptr) {
      const meta::GeneratedRecord& record_game = ptr_module_game->Record();
      const auto& vec_game = record_game.Values();
      std::size_t sz_index_allow = vec_game.size();
      std::size_t sz_index_filters = vec_game.size();
      const auto span_fields_game = record_game.record_desc().fields;
      for (std::size_t sz_i = 0; sz_i < span_fields_game.size(); ++sz_i) {
        const std::string_view sv_name = span_fields_game[sz_i].str_name;
        if (sv_name == "AllowDownloading")
          sz_index_allow = sz_i;
        else if (sv_name == "MPGameFilters")
          sz_index_filters = sz_i;
      }
      Check(sz_index_allow < vec_game.size() &&
                std::get<bool>(vec_game[sz_index_allow].val),
            "AllowDownloading 默认 true");
      Check(sz_index_filters < vec_game.size() &&
                std::get<std::int64_t>(vec_game[sz_index_filters].val) == 15,
            "MPGameFilters 默认 Waiting|Empty|Protected|Started");

      yaml::StringPool pool_game20;
      std::vector<yaml::MiniYamlNode> vec_override20;
      vec_override20.emplace_back(
          pool_game20.Intern("Game"),
          yaml::MiniYaml{nullptr,
                         std::vector<yaml::MiniYamlNode>{
                             {pool_game20.Intern("MPGameFilters"),
                              yaml::MiniYaml{pool_game20.Intern(
                                  "Waiting, Protected")}}}});
      settings_game20.Load(vec_override20, pool_game20);
      Check(std::get<std::int64_t>(vec_game[sz_index_filters].val) == 5,
            "MPGameFilters 覆写为 Waiting|Protected");
    }
  }

  {
    game::Settings settings_server21;
    const game::SettingsModule* ptr_module =
        settings_server21.Module("Server");
    Check(ptr_module != nullptr, "Settings Server 段装配");
    if (ptr_module != nullptr) {
      const meta::GeneratedRecord& record_server = ptr_module->Record();
      const auto& vec_server = record_server.Values();
      Check(vec_server.size() == 29, "Server 段 29 字段");
      Check(std::get<std::string>(vec_server[0].val).empty(),
            "Name 默认空");
      Check(std::get<std::int64_t>(vec_server[1].val) == 1234,
            "ListenPort 默认 1234");
      Check(std::get<bool>(vec_server[2].val) == true,
            "AdvertiseOnline 默认 true");
      Check(std::get<bool>(vec_server[3].val) == true,
            "AdvertiseOnLocalNetwork 默认 true");
      Check(std::get<std::string>(vec_server[4].val).empty(),
            "Password 默认空");
      Check(std::get<bool>(vec_server[5].val) == false,
            "DiscoverNatDevices 默认 false");
      Check(std::get<std::int64_t>(vec_server[6].val) == 36000,
            "NatPortMappingLifetime 默认 36000");
      Check(std::get<std::string>(vec_server[7].val).empty(),
            "Map 默认空");
      Check(std::get<bool>(vec_server[12].val) == false,
            "EnableSingleplayer 默认 false");
      Check(std::get<std::string>(vec_server[16].val) ==
                "yyyy-MM-ddTHH:mm:ss",
            "TimestampFormat 默认 ISO 8601");
      Check(std::get<bool>(vec_server[26].val) == true,
            "EnableVoteKick 默认 true");
      Check(std::get<std::int64_t>(vec_server[28].val) == 120000,
            "VoteKickerCooldown 默认 120000");

      yaml::StringPool pool_server21;
      const std::vector<yaml::MiniYamlNode> vec_saved_default =
          settings_server21.Save(pool_server21);
      bool b_server_section_present = false;
      for (const auto& node : vec_saved_default)
        if (node.Key != nullptr && *node.Key == "Server")
          b_server_section_present = true;
      Check(!b_server_section_present,
            "Server 段默认值全免存(空节点段过滤 = 上游 Where(Count > 0))");

      std::vector<yaml::MiniYamlNode> vec_override21;
      vec_override21.emplace_back(
          pool_server21.Intern("Server"),
          yaml::MiniYaml{nullptr,
                         std::vector<yaml::MiniYamlNode>{
                             {pool_server21.Intern("ListenPort"),
                              yaml::MiniYaml{pool_server21.Intern("4321")}},
                             {pool_server21.Intern("Name"),
                              yaml::MiniYaml{pool_server21.Intern(
                                  "My Server")}}}});
      settings_server21.Load(vec_override21, pool_server21);
      Check(std::get<std::int64_t>(vec_server[1].val) == 4321,
            "ListenPort 覆写 4321");
      Check(std::get<std::string>(vec_server[0].val) == "My Server",
            "Name 覆写");

      yaml::StringPool pool_server_save21;
      const std::vector<yaml::MiniYamlNode> vec_saved_override =
          settings_server21.Save(pool_server_save21);
      for (const auto& node : vec_saved_override)
        if (node.Key != nullptr && *node.Key == "Server") {
          Check(node.Value.Nodes.size() == 2,
                "Server 段仅差异字段入存");
          bool b_name_saved = false;
          bool b_port_saved = false;
          for (const auto& node_field : node.Value.Nodes)
            if (node_field.Key != nullptr) {
              if (*node_field.Key == "Name")
                b_name_saved = true;
              if (*node_field.Key == "ListenPort")
                b_port_saved = true;
            }
          Check(b_name_saved && b_port_saved,
                "Server 段差异集 = Name + ListenPort");
        }
    }

    {
      const auto dir_fs21 = std::filesystem::temp_directory_path() /
                            "ora_ui_test_fs21";
      std::filesystem::create_directories(dir_fs21 / "pkg-a");
      std::filesystem::create_directories(dir_fs21 / "pkg-b");
      {
        std::ofstream out_a(dir_fs21 / "pkg-a" / "one.txt");
        std::ofstream out_b(dir_fs21 / "pkg-b" / "two.txt");
      }
      fs::FileSystem file_system21;
      file_system21.Mount((dir_fs21 / "pkg-a").generic_string(), "alpha");
      file_system21.Mount((dir_fs21 / "pkg-b").generic_string());

      const std::vector<fs::IReadOnlyPackage*> vec_mounted =
          file_system21.MountedPackages();
      Check(vec_mounted.size() == 2, "MountedPackages 两包");
      Check(file_system21.GetPrefix(vec_mounted[0]).has_value() &&
                *file_system21.GetPrefix(vec_mounted[0]) == "alpha",
            "GetPrefix 反查显式挂载名");
      Check(!file_system21.GetPrefix(vec_mounted[1]).has_value(),
            "GetPrefix 非显式挂载 = nullopt");
    }
  }

  {
    Check(game::GetGraphicsResolution(0, int2{1920, 1080},
                                      int2{1024, 768}) == int2{1024, 768},
          "graphics_boot: windowed mode picks WindowedSize");
    Check(game::GetGraphicsResolution(1, int2{1920, 1080},
                                      int2{1024, 768}) == int2{1920, 1080},
          "graphics_boot: fullscreen mode picks FullscreenSize");
    Check(game::GetGraphicsResolution(2, int2{1920, 1080},
                                      int2{1024, 768}) == int2{1920, 1080},
          "graphics_boot: pseudo-fullscreen picks FullscreenSize");

    game::Settings settings_graphics24;
    const game::GraphicsBootDesc desc_default =
        game::GraphicsBootFromSettings(settings_graphics24);
    Check(desc_default.mode_window ==
                  platform::WindowMode::PseudoFullscreen &&
              desc_default.int2_resolution == int2{0, 0} &&
              desc_default.fp4_ui_scale == 1.0f &&
              desc_default.b_cap_framerate == false &&
              desc_default.int4_max_framerate == 60 &&
              desc_default.int4_video_display == 0 &&
              desc_default.kind_profile == platform::GLProfileKind::Modern,
          "graphics_boot: the Graphics defaults boot pseudo-fullscreen "
          "at the desktop-size zero sentinel");

    game::SettingsModule* ptr_graphics =
        settings_graphics24.Module("Graphics");
    Check(ptr_graphics != nullptr, "graphics_boot: Graphics 段装配");
    if (ptr_graphics != nullptr) {
      const std::optional<std::size_t> opt_slot_mode =
          ptr_graphics->SlotIndexOf("Mode");
      const std::optional<std::size_t> opt_slot_full =
          ptr_graphics->SlotIndexOf("FullscreenSize");
      const std::optional<std::size_t> opt_slot_windowed =
          ptr_graphics->SlotIndexOf("WindowedSize");
      const std::optional<std::size_t> opt_slot_scale =
          ptr_graphics->SlotIndexOf("UIScale");
      const std::optional<std::size_t> opt_slot_display =
          ptr_graphics->SlotIndexOf("VideoDisplay");
      const std::optional<std::size_t> opt_slot_profile =
          ptr_graphics->SlotIndexOf("GLProfile");
      const std::optional<std::size_t> opt_slot_vsync =
          ptr_graphics->SlotIndexOf("VSync");
      const std::optional<std::size_t> opt_slot_cap =
          ptr_graphics->SlotIndexOf("CapFramerate");
      const std::optional<std::size_t> opt_slot_max =
          ptr_graphics->SlotIndexOf("MaxFramerate");
      Check(opt_slot_mode.has_value() && opt_slot_full.has_value() &&
                opt_slot_windowed.has_value() && opt_slot_scale.has_value() &&
                opt_slot_display.has_value() && opt_slot_profile.has_value() &&
                opt_slot_vsync.has_value() && opt_slot_cap.has_value() &&
                opt_slot_max.has_value(),
            "graphics_boot: Graphics 段全字段槽");

      ptr_graphics->SetInt(*opt_slot_mode, 0);
      ptr_graphics->SetInt2(*opt_slot_full, int2{1920, 1080});
      ptr_graphics->SetInt2(*opt_slot_windowed, int2{1280, 800});
      ptr_graphics->SetFloat(*opt_slot_scale, 1.5f);
      ptr_graphics->SetInt(*opt_slot_display, 1);
      ptr_graphics->SetInt(*opt_slot_profile, 3);
      ptr_graphics->SetBool(*opt_slot_vsync, false);
      ptr_graphics->SetBool(*opt_slot_cap, true);
      ptr_graphics->SetInt(*opt_slot_max, 100);

      const game::GraphicsFramerateFace face_capped =
          game::GraphicsFramerateFromSettings(settings_graphics24);
      Check(face_capped.b_cap_framerate == true &&
                face_capped.int4_max_framerate == 100 &&
                face_capped.b_cap_framerate_to_game_fps == false,
            "graphics_boot: the loop face reads the raw cap trio");

      const game::GraphicsBootDesc desc_windowed =
          game::GraphicsBootFromSettings(settings_graphics24);
      Check(desc_windowed.mode_window == platform::WindowMode::Windowed &&
                desc_windowed.int2_resolution == int2{1280, 800} &&
                desc_windowed.fp4_ui_scale == 1.5f &&
                desc_windowed.int4_video_display == 1 &&
                desc_windowed.kind_profile ==
                    platform::GLProfileKind::Embedded &&
                desc_windowed.b_vsync == false,
            "graphics_boot: an overridden section boots windowed on "
            "display 1");

      Check(!game::SanitizeMaxFramerate(settings_graphics24),
            "graphics_boot: a positive MaxFramerate stays untouched");

      ptr_graphics->SetInt(*opt_slot_max, -5);
      ptr_graphics->SetInt(*opt_slot_profile, 0);
      const game::GraphicsBootDesc desc_automatic =
          game::GraphicsBootFromSettings(settings_graphics24);
      Check(desc_automatic.kind_profile == platform::GLProfileKind::Modern,
            "graphics_boot: the Automatic profile maps to Modern");

      Check(game::SanitizeMaxFramerate(settings_graphics24),
            "graphics_boot: MaxFramerate<1 trips the Run fixup");
      Check(ptr_graphics->GetInt(*opt_slot_max) == 60 &&
                ptr_graphics->GetBool(*opt_slot_cap) == false,
            "graphics_boot: the fixup restores the default and drops the "
            "cap");
      Check(!game::SanitizeMaxFramerate(settings_graphics24),
            "graphics_boot: the fixup is idempotent");
    }
  }

  if (int4_failures == 0) {
    std::println("ui_test: all passed");
    return 0;
  }
  std::println("ui_test: {} FAILURES", int4_failures);
  return 1;
}
