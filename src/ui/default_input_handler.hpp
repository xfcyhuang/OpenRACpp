#pragma once
import std;

#include "platform/sdl2_input.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"

namespace ora::game {
class Game;
}

namespace ora::ui {

class DefaultInputHandler : public IInputHandler {
 public:
  DefaultInputHandler(game::Game& game_host, sim::World* ptr_world)
      : game_host_(game_host), ptr_world_(ptr_world) {}

  void ModifierKeys(Modifiers mods) override;
  void OnKeyInput(const KeyInput& input) override;
  void OnMouseInput(const MouseInput& input) override;
  void OnTextInput(std::string_view str_text) override;

 private:
  game::Game& game_host_;
  sim::World* ptr_world_;
};

}
