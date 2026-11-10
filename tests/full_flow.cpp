import std;

import ora.game.input;
import ora.game.ui;
import ora.launcher;
import ora.test.util;
using ora::test::check;

namespace {

namespace lch = ora::launcher;
namespace ui = ora::game::ui;

std::string join(const std::vector<std::string>& vec, char ch_sep = ';') {
  std::string str_out;
  for (const std::string& str : vec) {
    if (!str_out.empty()) str_out += ch_sep;
    str_out += str;
  }
  return str_out;
}

// 上游真值（U100/U102 精读）：全流程模式切换序列——
// Main→(SINGLEPLAYER)→Singleplayer→(SKIRMISH)→Lobby→(START)→Game→(ABORT)→Lobby
// →(DISCONNECT)→Singleplayer→(BACK)→Main→(QUIT)→Exit
void test_full_sequence() {
  lch::FlowState st;
  lch::flow_setup(st);
  check(st.mode == lch::FlowMode::kMainMenu, "setup: menu mode");
  check(st.menu_type == ui::MenuType::kMain, "setup: menu type Main");

  check(lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer), "SINGLEPLAYER accepted");
  check(st.mode == lch::FlowMode::kMainMenu && st.menu_type == ui::MenuType::kSingleplayer,
        "SINGLEPLAYER: stays menu, type Singleplayer");

  check(lch::flow_dispatch(st, lch::FlowEvent::kSkirmish), "SKIRMISH accepted");
  check(st.mode == lch::FlowMode::kLobby, "SKIRMISH: to lobby");
  check(st.menu_type == ui::MenuType::kNone, "SKIRMISH: SwitchMenu(None)");

  check(lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart), "START accepted");
  check(st.mode == lch::FlowMode::kGame, "START: to game");

  // 上游对局退出 = ABORT_MISSION→onExit→OpenMenuBasedOnLastGame(Skirmish)
  // →StartSkirmishGame（重连大堂，不回菜单）
  check(lch::flow_dispatch(st, lch::FlowEvent::kGameAbort), "ABORT accepted");
  check(st.mode == lch::FlowMode::kLobby, "ABORT: back to lobby (StartSkirmishGame)");
  check(st.menu_type == ui::MenuType::kNone, "ABORT: SwitchMenu(None)");

  // 大堂 DISCONNECT → OpenSkirmishLobbyPanel onExit: Disconnect+SwitchMenu(Singleplayer)
  check(lch::flow_dispatch(st, lch::FlowEvent::kLobbyDisconnect), "DISCONNECT accepted");
  check(st.mode == lch::FlowMode::kMainMenu, "DISCONNECT: to menu");
  check(st.menu_type == ui::MenuType::kSingleplayer, "DISCONNECT: type Singleplayer");

  check(lch::flow_dispatch(st, lch::FlowEvent::kMenuBack), "BACK accepted");
  check(st.menu_type == ui::MenuType::kMain, "BACK: singleplayer context to Main");

  check(lch::flow_dispatch(st, lch::FlowEvent::kMenuQuit), "QUIT accepted");
  check(st.mode == lch::FlowMode::kExit, "QUIT: exit");

  check(join(st.vec_sequence) ==
            "switch Main;switch Singleplayer;switch None;start-skirmish;start-game;"
            "abort-mission;switch None;start-skirmish;switch Singleplayer;disconnect;"
            "switch Main;quit",
        "sequence golden");
}

// 各模式互斥/事件受理路由：模式外事件一律拒绝且不改状态
void test_exclusive_routing() {
  // SKIRMISH 仅 (MainMenu, Singleplayer) 受理
  {
    lch::FlowState st;
    lch::flow_setup(st);
    const std::vector<std::string> vec_seq_before{st.vec_sequence};
    check(!lch::flow_dispatch(st, lch::FlowEvent::kSkirmish),
          "skirmish rejected on Main menu");
    check(st.mode == lch::FlowMode::kMainMenu && st.menu_type == ui::MenuType::kMain,
          "rejected skirmish: state unchanged");
    check(st.vec_sequence == vec_seq_before, "rejected skirmish: no sequence entry");
  }
  // QUIT_BUTTON 在 MAIN_MENU 容器：(MainMenu, Singleplayer) 拒
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer);
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuQuit), "quit rejected on singleplayer");
  }
  // 大堂/对局内菜单按钮事件全拒（菜单不可达）
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer);
    lch::flow_dispatch(st, lch::FlowEvent::kSkirmish);
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuQuit), "menu quit rejected in lobby");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuBack), "menu back rejected in lobby");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer),
          "menu button rejected in lobby");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kSkirmish), "skirmish rejected in lobby");
    check(st.mode == lch::FlowMode::kLobby, "lobby unchanged");
  }
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer);
    lch::flow_dispatch(st, lch::FlowEvent::kSkirmish);
    lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart);
    check(!lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart), "start rejected in game");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kLobbyDisconnect),
          "disconnect rejected in game");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kSkirmish), "skirmish rejected in game");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuQuit), "menu quit rejected in game");
    check(st.mode == lch::FlowMode::kGame, "game unchanged");
  }
  // 大堂前 start/disconnect/abort 不可达
  {
    lch::FlowState st;
    lch::flow_setup(st);
    check(!lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart), "start rejected on menu");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kLobbyDisconnect),
          "disconnect rejected on menu");
    check(!lch::flow_dispatch(st, lch::FlowEvent::kGameAbort), "abort rejected on menu");
  }
  // Exit 终态全拒
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuQuit);
    for (const lch::FlowEvent ev :
         {lch::FlowEvent::kMenuSingleplayer, lch::FlowEvent::kMenuBack,
          lch::FlowEvent::kSkirmish, lch::FlowEvent::kLobbyStart, lch::FlowEvent::kGameAbort,
          lch::FlowEvent::kLobbyDisconnect})
      check(!lch::flow_dispatch(st, ev), "exit is terminal");
    check(st.mode == lch::FlowMode::kExit, "exit unchanged");
  }
}

// BACK_BUTTON 语境语义（MainMenuLogic 三菜单同名 BACK，U100）：
// singleplayer/extras→Main、map-editor→Extras；Main 菜单无 BACK
void test_back_contexts() {
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuExtras);
    check(st.menu_type == ui::MenuType::kExtras, "extras entry");
    check(lch::flow_dispatch(st, lch::FlowEvent::kMenuBack), "extras back");
    check(st.menu_type == ui::MenuType::kMain, "extras back to Main");
  }
  {
    lch::FlowState st;
    lch::flow_setup(st);
    lch::flow_dispatch(st, lch::FlowEvent::kMenuExtras);
    // EXTRAS_MENU 的 MAP_EDITOR_BUTTON → MapEditor 菜单
    lch::flow_dispatch(st, lch::FlowEvent::kMenuMapEditor);
    check(st.menu_type == ui::MenuType::kMapEditor, "map editor entry");
    check(lch::flow_dispatch(st, lch::FlowEvent::kMenuBack), "map editor back");
    check(st.menu_type == ui::MenuType::kExtras, "map editor back to Extras");
  }
  {
    lch::FlowState st;
    lch::flow_setup(st);
    check(!lch::flow_dispatch(st, lch::FlowEvent::kMenuBack), "main menu has no BACK");
  }
}

// 衔接断言：菜单侧出口位（mainmenu_logic）与 flow 的 skirmish 事件同源
void test_menu_logic_bridge() {
  ui::MainMenuEnv env{};
  env.b_has_maps = true;
  ui::WidgetModel root;
  root.str_type = "Container";
  root.str_id = "ROOT";
  ui::MainMenuState menu;
  ui::main_menu_setup(menu);
  ui::UiEventRecord rec{};
  rec.str_kind = "click";
  rec.str_id = "SKIRMISH_BUTTON";
  ui::main_menu_dispatch(menu, env, root, {&rec, 1});
  check(menu.type == ui::MenuType::kNone, "menu logic: SwitchMenu(None) on skirmish");
  check(menu.b_start_skirmish, "menu logic: start skirmish exit flag");
  check(menu.vec_nav.back() == "start-skirmish", "menu logic: nav tail start-skirmish");

  // 大堂断开回菜单：launcher 以 SwitchMenu(Singleplayer) 恢复（与 flow 断开态一致）
  ui::main_menu_switch(menu, ui::MenuType::kSingleplayer);
  check(menu.type == ui::MenuType::kSingleplayer, "resume after disconnect: Singleplayer");

  lch::FlowState st;
  lch::flow_setup(st);
  lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer);
  lch::flow_dispatch(st, lch::FlowEvent::kSkirmish);
  lch::flow_dispatch(st, lch::FlowEvent::kLobbyDisconnect);
  check(st.menu_type == menu.type, "flow/menu agree on post-disconnect type");
}

// 两轮对局往返：abort→lobby→start→abort→lobby→disconnect（上游可重复 skirmish）
void test_repeat_game_loop() {
  lch::FlowState st;
  lch::flow_setup(st);
  lch::flow_dispatch(st, lch::FlowEvent::kMenuSingleplayer);
  lch::flow_dispatch(st, lch::FlowEvent::kSkirmish);
  lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart);
  lch::flow_dispatch(st, lch::FlowEvent::kGameAbort);
  lch::flow_dispatch(st, lch::FlowEvent::kLobbyStart);
  check(st.mode == lch::FlowMode::kGame, "second game after abort");
  lch::flow_dispatch(st, lch::FlowEvent::kGameAbort);
  lch::flow_dispatch(st, lch::FlowEvent::kLobbyDisconnect);
  check(st.mode == lch::FlowMode::kMainMenu
            && st.menu_type == ui::MenuType::kSingleplayer,
        "second abort then disconnect lands on singleplayer menu");
  int int4_skirmish_count{0};
  for (const std::string& str : st.vec_sequence)
    if (str == "start-skirmish") ++int4_skirmish_count;
  check(int4_skirmish_count == 3,
        "each round (initial + 2 aborts) restarts StartSkirmishGame");
}

}  // namespace

int main() {
  test_full_sequence();
  test_exclusive_routing();
  test_back_contexts();
  test_menu_logic_bridge();
  test_repeat_game_loop();
  return ora::test::test_exit_code("full_flow");
}
