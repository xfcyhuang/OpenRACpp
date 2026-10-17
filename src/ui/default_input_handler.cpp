import std;

#include "ui/default_input_handler.hpp"

#include "game/game.hpp"

namespace ora::ui {

void DefaultInputHandler::ModifierKeys(Modifiers mods) {
  game_host_.HandleModifierKeys(mods);
}

void DefaultInputHandler::OnKeyInput(const KeyInput& input) {
  sim::RunUnsynced(true, ptr_world_, [&] { Ui::Instance().HandleKeyPress(input); });
}

void DefaultInputHandler::OnTextInput(std::string_view str_text) {
  sim::RunUnsynced(true, ptr_world_, [&] { Ui::Instance().HandleTextInput(str_text); });
}

void DefaultInputHandler::OnMouseInput(const MouseInput& input) {
  sim::RunUnsynced(true, ptr_world_, [&] { Ui::Instance().HandleInput(input); });
}

}  // namespace ora::ui
