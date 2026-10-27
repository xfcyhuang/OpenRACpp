#pragma once
import std;

#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::sim {
class World;
}
namespace ora::mods {
class MusicPlaylist;
}

namespace ora::mods {

class SingleHotkeyBaseLogic : public ora::ui::ChromeLogic {
 public:
  SingleHotkeyBaseLogic(ora::ui::Widget& widget_root,
                        game::ModData& mod_data,
                        const std::string& str_arg_name,
                        const std::string& str_parent_name,
                        const ora::ui::YamlDictionary* ptr_logic_args);

 private:
  virtual bool OnHotkeyActivated(const KeyInput& e_input) = 0;
};

class MusicHotkeyLogic final : public ora::ui::ChromeLogic {
 public:
  MusicHotkeyLogic(ora::ui::Widget& widget_root,
                   game::ModData& mod_data, sim::World& world,
                   const ora::ui::YamlDictionary* ptr_logic_args);

 private:
  void StopMusic();
  void PauseOrResumeMusic();

  MusicPlaylist* ptr_playlist_ = nullptr;
};

class ScreenshotHotkeyLogic final : public SingleHotkeyBaseLogic {
 public:
  ScreenshotHotkeyLogic(ora::ui::Widget& widget_root,
                        game::ModData& mod_data,
                        const ora::ui::YamlDictionary* ptr_logic_args);

 private:
  bool OnHotkeyActivated(const KeyInput& e_input) override;
};

class MuteHotkeyLogic final : public SingleHotkeyBaseLogic {
 public:
  MuteHotkeyLogic(ora::ui::Widget& widget_root,
                  game::ModData& mod_data,
                  const ora::ui::YamlDictionary* ptr_logic_args);

 private:
  bool OnHotkeyActivated(const KeyInput& e_input) override;
};

void RegisterHotkeyLogics();

}  // namespace ora::mods
