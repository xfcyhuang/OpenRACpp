// 第十五批验收:ActionQueue/WidgetArgs/Mediator/Widget 树/Ui 窗口栈与输入/
// ChromeMetrics/WidgetLoader 异常矩阵 + Game 主循环骨架(时钟注入下的节拍/
// JoinLocal/延迟动作/Run 清理)。期望值人工对照上游源码(ActionQueue.cs/
// Widget.cs/ChromeMetrics.cs/WidgetLoader.cs/Game.cs)。
// The fifteenth-batch acceptance: ActionQueue/WidgetArgs/Mediator/the Widget
// tree/the Ui window stack and input/ChromeMetrics/the WidgetLoader
// exception matrix + the Game main-loop skeleton (pacing under an injected
// clock/JoinLocal/delayed actions/Run cleanup). Expected values hand-checked
// against the upstream sources.
import std;

#include "core/action_queue.hpp"
#include "fs/file_system.hpp"
#include "game/game.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"

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

}  // namespace

int main() {
  TestActionQueue();
  TestWidgetArgs();
  TestMediator();
  TestWidgetTree();
  TestGameLoop();
  TestWidgetFocusInput();
  TestUiWindowStack();
  TestChromeMetrics();
  TestWidgetLoader();

  if (int4_failures == 0) {
    std::println("ui_test: all passed");
    return 0;
  }
  std::println("ui_test: {} FAILURES", int4_failures);
  return 1;
}
