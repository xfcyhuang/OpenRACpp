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
#include "game/fonts.hpp"
#include "game/game.hpp"
#include "game/mod_data.hpp"
#include "game/game_records.hpp"
#include "mods/create_map_players.hpp"
#include "sim/trait_registry.hpp"

namespace ora::gen {
void RegisterGeneratedAll();
}  // namespace ora::gen
#include "gfx/sheet.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/viewport.hpp"
#include "platform/ifont.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/hotkey.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
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

void TestBaseWidgetsChrome(const std::string& str_root) {
  // ButtonWidget ctor 的 ModData.DefaultRules 需要全装载链注册(map_test 同序)
  // ButtonWidget's ctor's ModData.DefaultRules needs the full loading-chain
  // registration (the same order as map_test).
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

  // Fluent 面(标签/按钮文本解析所需)
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

  // —— THREEBUTTON_PROMPT:Background + Label×2 + Button×3 ——
  ui::WidgetArgs args_prompt;
  args_prompt.Add("modData", static_cast<game::ModData*>(&mod_data));
  args_prompt.Add("chromeMetrics", static_cast<ui::ChromeMetrics*>(&metrics_chrome));
  args_prompt.Add("windowSize", int2{1280, 960});
  ui::Widget* ptr_prompt =
      loader_chrome.LoadWidget(args_prompt, nullptr, "THREEBUTTON_PROMPT");
  Check(ptr_prompt != nullptr && ptr_prompt->TypeName() == "BackgroundWidget",
        "THREEBUTTON_PROMPT 根类型");
  // X = (1280-600)/2 = 340
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

  // Key 激活路径(yaml Visible:false → Logic 重绑可见后可交互)
  KeyInput input_return{};
  input_return.Event = KeyInputEvent::Down;
  input_return.Key = Keycode::RETURN;
  bool b_clicked = false;
  ptr_confirm->fn_is_visible = [] { return true; };
  ptr_confirm->fn_on_click = [&b_clicked] { b_clicked = true; };
  Check(ptr_confirm->HandleKeyPressOuter(input_return), "Return 键激活消费");
  Check(b_clicked, "Return 键触发 OnClick");

  // 鼠标三态:Down 持焦 + Depressed → Up 让焦触发(yaml Visible:false →
  // Logic 重绑可见)
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

  // —— MULTIPLAYER_FILTER_PANEL:ScrollPanel + Checkbox 族 ——
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
  // 装载序 quirk:AddChild 先于字段装载 → 加载毕 content 只剩首项基座;
  // Logic 侧以 AdjustChildren 重排(上游同序)
  // The load-order quirk: AddChild precedes field loading → after loading
  // the content holds only the first-item base; Logic re-runs AdjustChildren
  // (upstream's same order).
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

  // 滚轮:target offset 按 UIScrollSpeed(50)移动(取上界夹取)
  MouseInput input_scroll{.Location = int2{0, 0},
                          .Button = MouseButton::None,
                          .Event = MouseInputEvent::Scroll,
                          .Delta = int2{0, -1}};
  ptr_panel->HandleMouseInput(input_scroll);
  input_scroll.Delta = int2{0, 1};
  ptr_panel->HandleMouseInput(input_scroll);
  ptr_panel->ScrollToBottom();
  Check(ptr_panel->ScrolledToBottom(), "ScrollToBottom");
  // content(104)< 视口(130)→ min(0, 26) = 0
  Check(ptr_panel->CurrentListOffset() == 0.0f, "content<视口时底 offset = 0");
  ptr_panel->ScrollToTop();
  Check(ptr_panel->CurrentListOffset() == 0.0f, "ScrollToTop");

  // —— LABEL_DROPDOWN_TEMPLATE:ScrollItem 模板面 ——
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
  // 字段装载后于 AddChild → yaml 的 Y:0 终值(上游同序)
  // Field loading follows AddChild → yaml's Y:0 is final (upstream's same
  // order).
  Check(widget_header.bounds_widget.int4_y == 0, "装载序 Y 终值 = yaml 值");

  // 定界滚动面:面板高度注入 + AdjustChildren 后 content = 2+13+25+2 = 42
  ptr_list->bounds_widget.int4_height = 40;
  ptr_list->ptr_layout->AdjustChildren();
  Check(ptr_list->int4_content_height == 42, "dropdown content = 2+13+25+2");

  // Setup 装配方:克隆模板 → 可见 + 选中态
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

  // ———— Phase 6 第三批:Slider/TextField/LabelWithTooltip/DropDown/
  // SimpleTooltipLogic(真 ra chrome + fake 字体注入)————
  TestSliderWidget();
  TestChromeWidgetsBatch3(mod_data, metrics_chrome, loader_chrome);
}

// ———— Phase 6 第三批:SliderWidget(纯逻辑面)————

void TestSliderWidget() {
  // 前序测试的局部 widget 可能未让焦即亡(GC 域等价面):焦点三指针清悬垂
  // Prior tests' local widgets may die without yielding (the GC-domain
  // equivalent): clear dangling focus pointers first.
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  auto up_slider = std::make_unique<ui::SliderWidget>();
  up_slider->bounds_widget = {0, 0, 100, 20};
  ui::SliderWidget& slider_test = *up_slider;

  int int4_changes = 0;
  slider_test.fn_on_change = [&int4_changes](float) { ++int4_changes; };

  // UpdateValue:夹取 + 变更通知
  slider_test.fp4_minimum_value = 0.0f;
  slider_test.fp4_maximum_value = 1.0f;
  slider_test.UpdateValue(0.5f);
  Check(std::abs(slider_test.fp4_value - 0.5f) < 1e-6f && int4_changes == 1,
        "UpdateValue 0.5 + OnChange");
  slider_test.UpdateValue(2.0f);
  Check(slider_test.fp4_value == 1.0f && int4_changes == 2, "上界夹取");
  slider_test.UpdateValue(1.0f);
  Check(int4_changes == 2, "同值不再通知");

  // 拖拽:Down@50 → 0.5;Move@90 → 1.0;Up 让焦
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

  // 禁用面:非左键/IsDisabled 恒拒
  input_slide.Event = MouseInputEvent::Down;
  input_slide.Button = MouseButton::Right;
  Check(!slider_test.HandleMouseInput(input_slide), "右键拒绝");
  slider_test.b_disabled = true;
  input_slide.Button = MouseButton::Left;
  Check(!slider_test.HandleMouseInput(input_slide), "禁用拒绝");
  slider_test.b_disabled = false;

  // Clone 保值
  auto up_clone = std::unique_ptr<ui::SliderWidget>(
      static_cast<ui::SliderWidget*>(slider_test.Clone().release()));
  Check(up_clone->fp4_value == slider_test.fp4_value &&
            up_clone->fp4_maximum_value == 1.0f && up_clone->int4_ticks == 0,
        "Slider 克隆字段");
}

// ———— Phase 6 第三批:TextField/LabelWithTooltip/DropDown/SimpleTooltip ————

void TestChromeWidgetsBatch3(game::ModData& mod_data, ui::ChromeMetrics& metrics_chrome,
                             ui::WidgetLoader& loader_chrome) {
  // 焦点三指针清悬垂(TestWidgetFocusInput 的局部 widget 未让焦即亡)
  // Clear the dangling focus trio (TestWidgetFocusInput's local widgets die
  // without yielding).
  ui::Ui::Instance().SetKeyboardFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseFocusWidget(nullptr);
  ui::Ui::Instance().SetMouseOverWidget(nullptr);

  // fake 字体注入(Regular = TextField/Bold = SIMPLE_TOOLTIP 的 LABEL)
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
  // 大时钟:TooltipContainer 的 delay 门常开(懒装载即时可见)
  // A large clock: TooltipContainer's delay gate stays open (the lazy load
  // is immediately visible).
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

  // —— TextFieldWidget:类型过滤/插入/光标/选择/删除族 ——
  {
    ui::WidgetArgs args_field;
    auto up_field = std::make_unique<ui::TextFieldWidget>(args_field);
    ui::TextFieldWidget& field_test = *up_field;
    field_test.bounds_widget = {0, 0, 100, 20};
    Check(field_test.str_font == "Regular", "TextField Font = TextfieldFont 指标");

    // 类型过滤
    field_test.SetType(ui::TextFieldType::Filename);
    field_test.SetText("a<b>:c*?");
    Check(field_test.Text() == "abc", "Filename 过滤 9 标点");
    field_test.SetType(ui::TextFieldType::Integer);
    field_test.SetText("1a2b3");
    Check(field_test.Text() == "123", "Integer 只留数字");
    field_test.SetType(ui::TextFieldType::General);

    // 焦点门:未聚焦不消费
    KeyInput input_key{};
    input_key.Event = KeyInputEvent::Down;
    input_key.Key = Keycode::LEFT;
    Check(!field_test.HandleKeyPress(input_key), "未聚焦按键不消费");
    Check(field_test.TakeKeyboardFocus(), "TakeKeyboardFocus");

    // 插入:光标位插字
    int int4_edited = 0;
    field_test.fn_on_text_edited = [&int4_edited] { ++int4_edited; };
    field_test.SetText("ab");
    field_test.int4_cursor_position = 1;
    Check(field_test.HandleTextInput("X") && field_test.Text() == "aXb" &&
              field_test.int4_cursor_position == 2 && int4_edited == 1,
          "光标位插入");

    // MaxLength 门
    field_test.int4_max_length = 3;
    Check(field_test.HandleTextInput("Y") && field_test.Text() == "aXb" && int4_edited == 1,
          "MaxLength 满员拒插");
    field_test.SetText("ab");
    field_test.int4_cursor_position = 2;
    field_test.HandleTextInput("cd");
    Check(field_test.Text() == "abc" && field_test.int4_cursor_position == 3,
          "粘贴截断到 MaxLength");
    field_test.int4_max_length = 0;

    // 光标移动:END/LEFT/Ctrl+LEFT/HOME/RIGHT/Ctrl+RIGHT
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

    // 选择:Ctrl+A 全选 → BACKSPACE 删选区
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

    // Ctrl+U:光标前全删
    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 5;
    input_key.Key = Keycode::U;
    input_key.ModifierFlags = Modifiers::Ctrl;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text().empty() && field_test.int4_cursor_position == 0, "Ctrl+U 全删");

    // Ctrl+K:光标后全删
    field_test.SetText("ab cd");
    field_test.int4_cursor_position = 2;
    input_key.Key = Keycode::K;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == "ab", "Ctrl+K 删尾");

    // Ctrl+D:光标处删一字
    field_test.SetText("abc");
    field_test.int4_cursor_position = 0;
    input_key.Key = Keycode::D;
    field_test.HandleKeyPress(input_key);
    Check(field_test.Text() == "bc", "Ctrl+D");

    // DELETE:单字与词删
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

    // blink:10 tick 翻相位;LEFT 复位
    input_key.Key = Keycode::LEFT;
    input_key.ModifierFlags = Modifiers::None;
    field_test.SetText("abc");
    field_test.int4_cursor_position = 3;
    field_test.HandleKeyPress(input_key);
    Check(field_test.ShowCursor(), "ResetBlinkCycle 后光标可见");
    for (int int4_i = 0; int4_i < 10; ++int4_i)
      field_test.Tick();
    Check(!field_test.ShowCursor(), "10 tick 后光标隐藏(相位翻转)");

    // 让焦回调
    bool b_lost_focus = false;
    field_test.fn_on_lose_focus = [&b_lost_focus] { b_lost_focus = true; };
    field_test.YieldKeyboardFocus();
    Check(b_lost_focus, "YieldKeyboardFocus 触发 OnLoseFocus");

    // 鼠标点击取位(fake 字体 advance 4:x=7 → 最近 i=1)
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

  // —— SimpleTooltipLogic(装载 args 仿容器的 tooltipContainer/getText 注入)——
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

    // cachedText 门:同文本不重建
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

  // —— LabelWithTooltip 挂真容器端到端(懒装载 + BeforeRender)——
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

    // 时钟越过 delay → 可见 → DrawOuter 经 IsVisible 触发懒装载 + BeforeRender
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

  // —— DropDownButtonWidget:ShowDropDown/Mask/ESC/双挂异常 ——
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

    // Root 新增 Mask + panel;面板锚在按钮下方,高 = min(60, 2+25+25+2=54)
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

    // 双挂异常
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

    // 项点击 → 原回调 + RemovePanel
    ui::ScrollItemWidget* ptr_item_two =
        static_cast<ui::ScrollItemWidget*>(ptr_drop_panel->OwnedChildren()[1].get());
    ptr_item_two->fn_on_click();
    Check(b_option_clicked, "选项回调触发");
    bool b_panel_gone = true;
    for (const auto& up_child : ui::Ui::Instance().Root()->OwnedChildren())
      if (up_child.get() == ptr_drop_panel || up_child.get() == ptr_mask)
        b_panel_gone = false;
    Check(b_panel_gone, "点击后面板与 Mask 移除");

    // 重开 → ESC 撤销
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

    // Mask 点击路径:开面板 → Mask 吞输入并撤
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

}  // namespace

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

  if (int4_failures == 0) {
    std::println("ui_test: all passed");
    return 0;
  }
  std::println("ui_test: {} FAILURES", int4_failures);
  return 1;
}
