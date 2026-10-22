// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
//          @e180e3225 L360-386(SetupProfileWidget —— PROFILE 图标的双态名
//          + PROFILE_TOOLTIP 的模板绑定;其余 Setup* 族随 Lobby 批)
//          L360-386 (SetupProfileWidget — the PROFILE icon's dual-state name
//          + PROFILE_TOOLTIP's template bind; the rest of the Setup* family
//          rides the Lobby batch).
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::net {
class OrderManager;
struct SessionClient;
}
namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

/// LobbyUtils.SetupProfileWidget(LobbyUtils.cs L360-386)
void SetupProfileWidget(ora::ui::Widget& widget_parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager* ptr_order_manager,
                        ora::gfx::WorldRenderer* ptr_world_renderer);

}  // namespace ora::mods
