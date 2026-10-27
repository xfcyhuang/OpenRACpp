#pragma once
import std;

#include "game/game_records.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}
namespace ora::ui {
class Widget;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct LoadWidgetAtGameStartInfoData {
  std::string str_shellmap_root = "MAINMENU";
  std::string str_ingame_root = "INGAME_ROOT";
  std::string str_editor_root = "EDITOR_ROOT";
  std::string str_game_save_loading_root = "GAMESAVE_LOADING_SCREEN";
  bool b_clear_root = true;

  static LoadWidgetAtGameStartInfoData Parse(const meta::RecordObject& rec_info);
};

class LoadWidgetAtGameStart final : public TraitBase,
                                    public sim::IWorldLoaded,
                                    public sim::INotifyGameLoading,
                                    public sim::INotifyGameLoaded {
 public:
  LoadWidgetAtGameStart(LoadWidgetAtGameStartInfoData&& info);

  ORA_TRAIT_INTERFACES(LoadWidgetAtGameStart,
                       OpenRA_Mods_Common_Traits_LoadWidgetAtGameStart,
                       sim::IWorldLoaded, sim::INotifyGameLoading,
                       sim::INotifyGameLoaded)

  void GameLoading(sim::World& world) override;
  void WorldLoaded(sim::World& world,
                   gfx::WorldRenderer* ptr_world_renderer) override;
  void GameLoaded(sim::World& world) override;

 private:
  ui::Widget* LoadRootWidget(sim::World& world);

  LoadWidgetAtGameStartInfoData info_;
  ui::Widget* ptr_root_ = nullptr;
};

void RegisterLoadWidgetAtGameStart();

}
