#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::sim {
class World;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::game {
struct PlayerProfile;
}

namespace ora::mods {

class LoadLocalPlayerProfileLogic final : public ora::ui::ChromeLogic {
 public:
  LoadLocalPlayerProfileLogic(ora::ui::Widget& widget_root,
                              sim::World& world);
};

class LocalProfileLogic final : public ora::ui::ChromeLogic {
 public:
  LocalProfileLogic(ora::ui::Widget& widget_root,
                    gfx::WorldRenderer* ptr_world_renderer,
                    std::function<bool()> fn_minimal_profile);
  ~LocalProfileLogic() override;

 private:
  void RefreshBadges();

  ora::ui::Widget& widget_root_;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  ora::ui::Widget* ptr_badge_container_ = nullptr;
  bool b_badges_visible_ = false;
  std::shared_ptr<bool> up_alive_;
  std::function<void()> fn_unsubscribe_;
};

class PlayerProfileBadgesLogic final : public ora::ui::ChromeLogic {
 public:
  PlayerProfileBadgesLogic(ora::ui::Widget& widget_root,
                           const game::PlayerProfile* ptr_profile,
                           std::function<int(int)> fn_negotiate_width);

 private:
  std::unique_ptr<ora::ui::Widget> up_badge_template_;
};

void RegisterLocalProfileLogics();

}
