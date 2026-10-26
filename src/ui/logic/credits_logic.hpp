// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/CreditsLogic.cs @1f594e320 L18-97(全文)。
// 形态适配:上游 Game.Renderer.Fonts 的局部 font 变量在 ctor 中未被消费,
// 不复刻;DefaultFileSystem(引擎级挂载面)以 ModFiles 代偿(登记 COVERAGE)。
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

}  // namespace ora::ui
