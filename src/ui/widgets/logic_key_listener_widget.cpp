// UPSTREAM: OpenRA.Mods.Common/Widgets/LogicKeyListenerWidget.cs @e180e3225 L19-36(全文)。
import std;

#include "ui/widgets/logic_key_listener_widget.hpp"

namespace ora::ui {

void LogicKeyListenerWidget::AddHandler(
    std::function<bool(const KeyInput&)> fn_handler) {
  vec_handlers_.push_back(std::move(fn_handler));
}

bool LogicKeyListenerWidget::HandleKeyPress(const KeyInput& e_input) {
  for (const auto& fn_handler : vec_handlers_)
    if (fn_handler(e_input))
      return true;

  return false;
}

}  // namespace ora::ui
