module ora.launcher;

import std;
import ora.game.input;
import ora.game.ui;

namespace ora::launcher {

namespace ui = game::ui;

namespace {

void flow_switch(FlowState& st, ui::MenuType type) {
  st.menu_type = type;
  st.vec_sequence.push_back(std::format("switch {}", ui::menu_type_name(type)));
}

}  // namespace

void flow_setup(FlowState& st) {
  st = FlowState{};
  st.mode = FlowMode::kMainMenu;
  flow_switch(st, ui::MenuType::kMain);
}

bool flow_dispatch(FlowState& st, FlowEvent ev) {
  switch (ev) {
    case FlowEvent::kMenuSingleplayer: {
      // SINGLEPLAYER_BUTTON（MAIN_MENU 容器）→ SwitchMenu(Singleplayer)
      if (st.mode != FlowMode::kMainMenu || st.menu_type != ui::MenuType::kMain) return false;
      flow_switch(st, ui::MenuType::kSingleplayer);
      return true;
    }
    case FlowEvent::kMenuExtras: {
      if (st.mode != FlowMode::kMainMenu || st.menu_type != ui::MenuType::kMain) return false;
      flow_switch(st, ui::MenuType::kExtras);
      return true;
    }
    case FlowEvent::kMenuMapEditor: {
      // MAP_EDITOR_BUTTON（EXTRAS_MENU 容器）→ SwitchMenu(MapEditor)
      if (st.mode != FlowMode::kMainMenu || st.menu_type != ui::MenuType::kExtras) return false;
      flow_switch(st, ui::MenuType::kMapEditor);
      return true;
    }
    case FlowEvent::kMenuBack: {
      // BACK_BUTTON 语境回退（mainmenu_logic back_click 同构）：
      // map-editor→Extras；singleplayer/extras→Main；Main/None 无此按钮
      if (st.mode != FlowMode::kMainMenu) return false;
      switch (st.menu_type) {
        case ui::MenuType::kSingleplayer:
        case ui::MenuType::kExtras:
          flow_switch(st, ui::MenuType::kMain);
          return true;
        case ui::MenuType::kMapEditor:
          flow_switch(st, ui::MenuType::kExtras);
          return true;
        default:
          return false;
      }
    }
    case FlowEvent::kMenuQuit: {
      // QUIT_BUTTON（MAIN_MENU 容器）→ Game.Exit（state=Success）
      if (st.mode != FlowMode::kMainMenu || st.menu_type != ui::MenuType::kMain) return false;
      st.mode = FlowMode::kExit;
      st.vec_sequence.push_back("quit");
      return true;
    }
    case FlowEvent::kSkirmish: {
      // SKIRMISH_BUTTON（SINGLEPLAYER_MENU 容器）→ StartSkirmishGame：
      // SwitchMenu(None) + 连接本地服务器 + SERVER_LOBBY
      if (st.mode != FlowMode::kMainMenu || st.menu_type != ui::MenuType::kSingleplayer)
        return false;
      st.menu_type = ui::MenuType::kNone;
      st.vec_sequence.push_back("switch None");
      st.mode = FlowMode::kLobby;
      st.vec_sequence.push_back("start-skirmish");
      return true;
    }
    case FlowEvent::kLobbyDisconnect: {
      // DISCONNECT → OpenSkirmishLobbyPanel onExit: Disconnect+SwitchMenu(Singleplayer)
      if (st.mode != FlowMode::kLobby) return false;
      st.mode = FlowMode::kMainMenu;
      flow_switch(st, ui::MenuType::kSingleplayer);
      st.vec_sequence.push_back("disconnect");
      return true;
    }
    case FlowEvent::kLobbyStart: {
      if (st.mode != FlowMode::kLobby) return false;
      st.mode = FlowMode::kGame;
      st.vec_sequence.push_back("start-game");
      return true;
    }
    case FlowEvent::kGameAbort: {
      // ABORT_MISSION → onExit → OpenMenuBasedOnLastGame(Skirmish) → StartSkirmishGame
      //（重连大堂：SwitchMenu(None) + 新本地服务器 + SERVER_LOBBY——与 kSkirmish 同链）
      if (st.mode != FlowMode::kGame) return false;
      st.mode = FlowMode::kLobby;
      st.menu_type = ui::MenuType::kNone;
      st.vec_sequence.push_back("abort-mission");
      st.vec_sequence.push_back("switch None");
      st.vec_sequence.push_back("start-skirmish");
      return true;
    }
  }
  return false;
}

}  // namespace ora::launcher
