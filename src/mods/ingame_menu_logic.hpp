// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/IngameMenuLogic.cs
//          @e180e3225 L23-647 全文。
#pragma once
import std;

#include "core/int2.hpp"
#include "mods/menu_logics.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class World;
}

namespace ora::mods {

using ora::ui::YamlDictionary;

class IngameMenuLogic final : public ora::ui::ChromeLogic {
 public:
  IngameMenuLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                  sim::World& world, std::function<void()> fn_on_exit,
                  gfx::WorldRenderer* ptr_world_renderer,
                  IngameInfoPanel initial_panel,
                  const YamlDictionary* ptr_logic_args);

  static void OnQuit(sim::World& world);

 private:
  void ShowMenu();
  void CloseMenu();
  ora::ui::ButtonWidget* AddButton(const std::string& str_id,
                                   const std::string& str_label);
  void CreateAbortMissionButton();
  void CreateBackToEditorButton();
  void CreateRestartButton();
  void CreateSurrenderButton();
  void CreateLoadGameButton();
  void CreateSaveGameButton();
  void CreateMusicButton();
  void CreateSettingsButton();
  void CreateResumeButton();
  void CreateSaveMapButton();
  void CreatePlayMapButton();
  void CreateExitEditorButton();
  void ExitEditor(bool b_has_unsaved_items, std::function<void()> fn_on_success);

  ora::ui::Widget* ptr_menu_ = nullptr;
  ora::ui::Widget* ptr_button_container_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_button_template_;
  int2 int2_button_stride_{};
  std::vector<ora::ui::ButtonWidget*> vec_buttons_;

  game::ModData* ptr_mod_data_ = nullptr;
  std::function<void()> fn_on_exit_;
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  bool b_is_single_player_ = false;
  bool b_has_error_ = false;
  bool b_leaving_ = false;
  bool b_hide_menu_ = false;

  static bool b_last_game_editor_;
};

void RegisterIngameMenuLogic();

}  // namespace ora::mods
