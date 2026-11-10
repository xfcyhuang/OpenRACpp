#pragma once

import std;

#include "game/ui/mainmenu_logic.hpp"

namespace ora::launcher {

// 全流程模式（T28 串联票）：同一时刻仅一个模式活跃（互斥由 dispatch 受理矩阵保证，
// 模式外事件拒绝且状态不变）。上游等价物 = MainMenuLogic 的 MenuType + 窗口回调网
//（OpenSkirmishLobbyPanel onStart/onExit、IngameMenuLogic onExit、OpenMenuBasedOnLastGame），
// 上游无集中状态机对象——C++ 折叠为单一可无头断言的序列真源（U100/U102 精读锚定）。
enum class FlowMode { kMainMenu, kLobby, kGame, kExit };

// 串联面事件（上游语义括注）：
// kMenuSingleplayer/kMenuExtras/kMenuMapEditor —— 主菜单容器内按钮 SwitchMenu
// kMenuBack   —— BACK_BUTTON 语境回退（singleplayer/extras→Main、map-editor→Extras）
// kMenuQuit   —— QUIT_BUTTON（MAIN_MENU 容器）→ Game.Exit
// kSkirmish   —— SKIRMISH_BUTTON（SINGLEPLAYER_MENU 容器）→ StartSkirmishGame：
//                SwitchMenu(None) + 连接本地服务器 → SERVER_LOBBY 大堂
// kLobbyDisconnect —— DISCONNECT/ESC 大堂断开 → onExit: Disconnect+SwitchMenu(Singleplayer)
// kLobbyStart —— START GAME/Enter → RemoveShellmapUI + lastGameState=Skirmish → 对局
// kGameAbort  —— 对局 ESC/ABORT_MISSION → onExit → shellmap → OpenMenuBasedOnLastGame
//                (Skirmish) → StartSkirmishGame（重连大堂，不落主菜单）
enum class FlowEvent {
  kMenuSingleplayer,
  kMenuExtras,
  kMenuMapEditor,
  kMenuBack,
  kMenuQuit,
  kSkirmish,
  kLobbyDisconnect,
  kLobbyStart,
  kGameAbort
};

struct FlowState {
  FlowMode mode{FlowMode::kMainMenu};
  game::ui::MenuType menu_type{game::ui::MenuType::kStartupPrompts};
  // 模式切换序列记录（"switch <MenuType>" 与 mainmenu_logic vec_nav 同格式 +
  // start-skirmish/start-game/abort-mission/disconnect/quit 编排行）
  std::vector<std::string> vec_sequence;
};

// StartupPrompts→Main 折叠（同 main_menu_setup，U100 ctor 尾链）。
void flow_setup(FlowState& st);

// 事件受理路由：受理返回 true 并推进模式/菜单态；模式外/语境外事件拒绝（false）且
// 状态与序列不变。终态 kExit 拒绝一切事件。
bool flow_dispatch(FlowState& st, FlowEvent ev);

}  // namespace ora::launcher
