#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::ui {

class ScrollPanelWidget;
class LabelWidget;

class CreditsLogic : public ChromeLogic {
 public:
  explicit CreditsLogic(WidgetArgs& args_args);

  void ShowCredits(bool b_mod_credits);

  bool IsShowingModTabForTest() const { return b_is_showing_mod_tab_; }
  bool ShowModTabForTest() const { return b_show_mod_tab_; }
  bool ShowEngineTabForTest() const { return b_show_engine_tab_; }
  const std::string& ModLinesForTest() const { return str_mod_lines_; }
  const std::string& EngineLinesForTest() const { return str_engine_lines_; }

 private:
  ScrollPanelWidget* ptr_scroll_panel_ = nullptr;
  std::unique_ptr<LabelWidget> up_template_;

  bool b_show_mod_tab_ = false;
  bool b_show_engine_tab_ = false;
  bool b_is_showing_mod_tab_ = false;
  std::string str_mod_lines_;
  std::string str_engine_lines_;
};

}
