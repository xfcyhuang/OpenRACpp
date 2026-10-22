// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
//          @e180e3225(实现;头注见 lobby_utils.hpp)
//          Implementation; the header notes live in lobby_utils.hpp.
#include "mods/lobby_utils.hpp"

#include "net/session.hpp"
#include "ui/widgets/client_tooltip_region_widget.hpp"
#include "ui/widgets/image_widget.hpp"

namespace ora::mods {

void SetupProfileWidget(ora::ui::Widget& widget_parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager* ptr_order_manager,
                        ora::gfx::WorldRenderer* ptr_world_renderer) {
  if (auto* ptr_profile =
          widget_parent.GetOrNull<ora::ui::ImageWidget>("PROFILE")) {
    std::string str_image_name = ptr_client->IsBot()
                                     ? std::string{"bot"}
                                     : ptr_client->IsAdmin
                                           ? std::string{"admin-"}
                                           : std::string{"player-"};

    if (!ptr_client->IsBot())
      str_image_name += !ptr_client->Fingerprint.empty() ? "registered"
                                                         : "anonymous";

    ptr_profile->fn_get_image_name = [str_image_name] {
      return str_image_name;
    };
    ptr_profile->fn_is_visible = [] { return true; };
  }

  if (auto* ptr_profile_tooltip =
          widget_parent.GetOrNull<ora::ui::ClientTooltipRegionWidget>(
              "PROFILE_TOOLTIP")) {
    if (!ptr_client->Fingerprint.empty())
      ptr_profile_tooltip->str_template = "REGISTERED_PLAYER_TOOLTIP";

    if (ptr_client->IsBot())
      ptr_profile_tooltip->str_template = "BOT_TOOLTIP";

    ptr_profile_tooltip->Bind(ptr_order_manager, ptr_world_renderer,
                              ptr_client);

    ptr_profile_tooltip->fn_is_visible = [] { return true; };
  }
}

}  // namespace ora::mods
