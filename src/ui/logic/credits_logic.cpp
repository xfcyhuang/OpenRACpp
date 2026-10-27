import std;

#include "ui/logic/credits_logic.hpp"

#include "game/mod_data.hpp"
#include "game/mod_credits.hpp"
#include "game/platform.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"

namespace ora::ui {
namespace {

std::string ParseLines(std::span<const char> span_file) {
  std::string str_text{span_file.begin(), span_file.end()};

  std::string str_result;
  str_result.reserve(str_text.size() + 16);
  for (std::size_t sz_i = 0; sz_i < str_text.size(); sz_i++) {
    const char chr_current = str_text[sz_i];
    if (chr_current == '\r' && sz_i + 1 < str_text.size() &&
        str_text[sz_i + 1] == '\n')
      continue;
    if (chr_current == '\t') {
      str_result.append("    ");
    } else if (chr_current == '*') {
      str_result.append("\u2022");
    } else
      str_result.push_back(chr_current);
  }
  return str_result;
}

std::vector<char> ReadFileBytes(const std::string& str_path) {
  std::ifstream stream_file{str_path, std::ios::binary};
  if (!stream_file)
    throw std::runtime_error("Unable to open file: " + str_path);
  return std::vector<char>{std::istreambuf_iterator<char>{stream_file},
                           std::istreambuf_iterator<char>{}};
}

}

CreditsLogic::CreditsLogic(WidgetArgs& args_args) {
  Widget* ptr_widget = std::get<Widget*>(args_args.At("widget"));
  game::ModData* ptr_mod_data =
      std::get<game::ModData*>(args_args.At("modData"));
  const auto fn_on_exit = std::get<std::function<void()>>(args_args.At("onExit"));

  Widget& widget_panel = *ptr_widget->Get("CREDITS_PANEL");

  widget_panel.Get<ButtonWidget>("BACK_BUTTON")->fn_on_click =
      [fn_on_exit] {
        Ui::Instance().CloseWindow();
        fn_on_exit();
      };

  const game::ModCredits& credits_mod = ptr_mod_data->GetOrCreateModCredits();
  Widget* ptr_tab_container = widget_panel.Get("TAB_CONTAINER");

  if (credits_mod.HasModCreditsFile()) {
    b_show_mod_tab_ = true;
    str_mod_lines_ =
        ParseLines(ptr_mod_data->ModFiles().Open(credits_mod.str_mod_credits_file));

    ButtonWidget* ptr_mod_tab = ptr_tab_container->Get<ButtonWidget>("MOD_TAB");
    ptr_mod_tab->fn_is_highlighted = [this] { return b_is_showing_mod_tab_; };
    ptr_mod_tab->fn_on_click = [this] { ShowCredits(true); };
    ptr_mod_tab->fn_get_text = [str_title = credits_mod.str_mod_tab_title] {
      return str_title;
    };
  }

  if (!credits_mod.str_engine_credits_file.empty()) {
    b_show_engine_tab_ = true;
    const game::PathResolver resolver_paths{ptr_mod_data->EngineDir(),
                                            std::string{}};
    str_engine_lines_ = ParseLines(
        ReadFileBytes(resolver_paths(credits_mod.str_engine_credits_file)));

    ButtonWidget* ptr_engine_tab =
        ptr_tab_container->Get<ButtonWidget>("ENGINE_TAB");
    ptr_engine_tab->fn_is_highlighted = [this] { return !b_is_showing_mod_tab_; };
    ptr_engine_tab->fn_on_click = [this] { ShowCredits(false); };
  }

  ptr_scroll_panel_ = widget_panel.Get<ScrollPanelWidget>("CREDITS_DISPLAY");
  LabelWidget* ptr_template =
      ptr_scroll_panel_->Get<LabelWidget>("CREDITS_TEMPLATE");
  up_template_ =
      std::unique_ptr<LabelWidget>{static_cast<LabelWidget*>(
          ptr_scroll_panel_->RemoveChild(*ptr_template).release())};

  const bool b_both_tabs = b_show_mod_tab_ && b_show_engine_tab_;
  ptr_tab_container->fn_is_visible = [b_both_tabs] { return b_both_tabs; };
  if (b_both_tabs) {
    ptr_scroll_panel_->bounds_widget.int4_y +=
        ptr_tab_container->bounds_widget.int4_height;
    ptr_scroll_panel_->bounds_widget.int4_height -=
        ptr_tab_container->bounds_widget.int4_height;
  }

  ShowCredits(b_show_mod_tab_);
}

void CreditsLogic::ShowCredits(bool b_mod_credits) {
  b_is_showing_mod_tab_ = b_mod_credits;

  ptr_scroll_panel_->RemoveChildren();
  const std::string& str_lines = b_mod_credits ? str_mod_lines_ : str_engine_lines_;

  std::unique_ptr<Widget> up_label = up_template_->Clone();
  LabelWidget* ptr_label = static_cast<LabelWidget*>(up_label.get());
  ptr_label->fn_get_text = [str_lines] { return str_lines; };
  ptr_label->IncreaseHeightToFitCurrentText();
  ptr_scroll_panel_->AddChild(std::move(up_label));
}

}
