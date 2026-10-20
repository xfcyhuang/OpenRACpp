// UPSTREAM: OpenRA.Mods.Common/Widgets/ConfirmationDialogs.cs @e180e3225
//          L17-193 全文(实现部分)。
#include "mods/confirmation_dialogs.hpp"

#include "fluent/fluent_provider.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

namespace ora::mods {

void ButtonPrompt(const std::string& str_title, const std::string& str_text,
                  std::function<void()> fn_on_confirm,
                  const std::string& str_confirm_text,
                  std::function<void()> fn_on_cancel,
                  const std::string& str_cancel_text,
                  std::function<void()> fn_on_other,
                  const std::string& str_other_text,
                  const FluentArgs& vec_text_arguments) {
  const std::string str_prompt_name =
      fn_on_other != nullptr ? "THREEBUTTON_PROMPT" : "TWOBUTTON_PROMPT";
  ora::ui::Widget* ptr_prompt =
      ora::ui::Ui::Instance().OpenWindow(str_prompt_name);
  auto* ptr_confirm_button =
      ptr_prompt->GetOrNull<ora::ui::ButtonWidget>("CONFIRM_BUTTON");
  auto* ptr_cancel_button =
      ptr_prompt->GetOrNull<ora::ui::ButtonWidget>("CANCEL_BUTTON");
  auto* ptr_other_button =
      ptr_prompt->GetOrNull<ora::ui::ButtonWidget>("OTHER_BUTTON");

  const std::string str_title_message =
      fluent::FluentProvider::Instance().GetMessage(str_title);
  ptr_prompt->Get<ora::ui::LabelWidget>("PROMPT_TITLE")->fn_get_text =
      [str_title_message] { return str_title_message; };

  auto* ptr_header_template =
      ptr_prompt->Get<ora::ui::LabelWidget>("PROMPT_TEXT");
  const std::string str_text_message =
      fluent::FluentProvider::Instance().GetMessage(str_text,
                                                    vec_text_arguments);

  std::vector<std::string> vec_header_lines;
  {
    std::size_t pos_start = 0;
    const std::string& str_source = str_text_message;
    while (true) {
      const std::size_t pos_newline = str_source.find('\n', pos_start);
      if (pos_newline == std::string::npos) {
        vec_header_lines.push_back(str_source.substr(pos_start));
        break;
      }
      vec_header_lines.push_back(
          str_source.substr(pos_start, pos_newline - pos_start));
      pos_start = pos_newline + 1;
    }
  }

  int int4_header_height = 0;
  for (const std::string& str_line : vec_header_lines) {
    std::unique_ptr<ora::ui::Widget> up_line = ptr_header_template->Clone();
    if (auto* ptr_line = dynamic_cast<ora::ui::LabelWidget*>(up_line.get()))
      ptr_line->fn_get_text = [str_line] { return str_line; };
    up_line->bounds_widget.int4_y += int4_header_height;
    ptr_prompt->AddChild(std::move(up_line));

    int4_header_height += ptr_header_template->bounds_widget.int4_height;
  }

  ptr_prompt->bounds_widget.int4_height += int4_header_height;
  ptr_prompt->bounds_widget.int4_y -= int4_header_height / 2;

  if (fn_on_confirm != nullptr && ptr_confirm_button != nullptr) {
    ptr_confirm_button->b_visible = true;
    ptr_confirm_button->bounds_widget.int4_y += int4_header_height;
    ptr_confirm_button->fn_on_click = [fn_on_confirm]() mutable {
      ora::ui::Ui::Instance().CloseWindow();
      fn_on_confirm();
    };

    if (!str_confirm_text.empty()) {
      const std::string str_confirm_text_message =
          fluent::FluentProvider::Instance().GetMessage(str_confirm_text);
      ptr_confirm_button->fn_get_text =
          [str_confirm_text_message] { return str_confirm_text_message; };
    }
  }

  if (fn_on_cancel != nullptr && ptr_cancel_button != nullptr) {
    ptr_cancel_button->b_visible = true;
    ptr_cancel_button->bounds_widget.int4_y += int4_header_height;
    ptr_cancel_button->fn_on_click = [fn_on_cancel]() mutable {
      ora::ui::Ui::Instance().CloseWindow();
      fn_on_cancel();
    };

    if (!str_cancel_text.empty()) {
      const std::string str_cancel_text_message =
          fluent::FluentProvider::Instance().GetMessage(str_cancel_text);
      ptr_cancel_button->fn_get_text =
          [str_cancel_text_message] { return str_cancel_text_message; };
    }
  }

  if (fn_on_other != nullptr && ptr_other_button != nullptr) {
    ptr_other_button->b_visible = true;
    ptr_other_button->bounds_widget.int4_y += int4_header_height;
    ptr_other_button->fn_on_click = std::move(fn_on_other);

    if (!str_other_text.empty()) {
      const std::string str_other_text_message =
          fluent::FluentProvider::Instance().GetMessage(str_other_text);
      ptr_other_button->fn_get_text =
          [str_other_text_message] { return str_other_text_message; };
    }
  }
}

void TextInputPrompt(const std::string& str_title,
                     const std::string& str_prompt,
                     const std::string& str_initial_text,
                     std::function<void(std::string)> fn_on_accept,
                     std::function<void()> fn_on_cancel,
                     const std::string& str_accept_text,
                     const std::string& str_cancel_text,
                     std::function<bool(const std::string&)> fn_input_validator) {
  ora::ui::Widget* ptr_panel =
      ora::ui::Ui::Instance().OpenWindow("TEXT_INPUT_PROMPT");
  auto fn_do_validate =
      std::make_shared<std::function<bool()>>();
  auto* ptr_accept_button =
      static_cast<ora::ui::ButtonWidget*>(nullptr);
  auto* ptr_cancel_button =
      static_cast<ora::ui::ButtonWidget*>(nullptr);

  const std::string str_title_message =
      fluent::FluentProvider::Instance().GetMessage(str_title);
  ptr_panel->Get<ora::ui::LabelWidget>("PROMPT_TITLE")->fn_get_text =
      [str_title_message] { return str_title_message; };

  const std::string str_prompt_message =
      fluent::FluentProvider::Instance().GetMessage(str_prompt);
  ptr_panel->Get<ora::ui::LabelWidget>("PROMPT_TEXT")->fn_get_text =
      [str_prompt_message] { return str_prompt_message; };

  auto* ptr_input = ptr_panel->Get<ora::ui::TextFieldWidget>("INPUT_TEXT");
  auto b_is_valid = std::make_shared<bool>(false);
  ptr_input->SetText(str_initial_text);
  ptr_input->fn_is_valid = [b_is_valid] { return *b_is_valid; };
  ptr_input->fn_on_enter_key =
      [ptr_accept_button](const ora::KeyInput&) {
        if (ptr_accept_button->fn_is_disabled())
          return false;

        ptr_accept_button->fn_on_click();
        return true;
      };
  ptr_input->fn_on_esc_key =
      [ptr_cancel_button](const ora::KeyInput&) {
        if (ptr_cancel_button->fn_is_disabled())
          return false;

        ptr_cancel_button->fn_on_click();
        return true;
      };
  ptr_input->TakeKeyboardFocus();
  ptr_input->int4_cursor_position =
      static_cast<int>(ptr_input->Text().size());
  ptr_input->fn_on_text_edited = [fn_do_validate] { (*fn_do_validate)(); };

  ptr_accept_button = ptr_panel->Get<ora::ui::ButtonWidget>("ACCEPT_BUTTON");
  if (!str_accept_text.empty()) {
    const std::string str_accept_text_message =
        fluent::FluentProvider::Instance().GetMessage(str_accept_text);
    ptr_accept_button->fn_get_text =
        [str_accept_text_message] { return str_accept_text_message; };
  }

  ptr_accept_button->fn_on_click =
      [fn_do_validate, fn_on_accept, ptr_input]() mutable {
        if (!(*fn_do_validate)())
          return;

        ora::ui::Ui::Instance().CloseWindow();
        fn_on_accept(ptr_input->Text());
      };

  ptr_cancel_button = ptr_panel->Get<ora::ui::ButtonWidget>("CANCEL_BUTTON");
  if (!str_cancel_text.empty()) {
    const std::string str_cancel_text_message =
        fluent::FluentProvider::Instance().GetMessage(str_cancel_text);
    ptr_cancel_button->fn_get_text =
        [str_cancel_text_message] { return str_cancel_text_message; };
  }

  ptr_cancel_button->fn_on_click = [fn_on_cancel]() mutable {
    ora::ui::Ui::Instance().CloseWindow();
    if (fn_on_cancel)
      fn_on_cancel();
  };

  *fn_do_validate = [fn_input_validator, b_is_valid,
                     ptr_accept_button, ptr_input]() {
    if (fn_input_validator == nullptr)
      return true;

    *b_is_valid = fn_input_validator(ptr_input->Text());
    if (*b_is_valid) {
      ptr_accept_button->b_disabled = false;
      return true;
    }

    ptr_accept_button->b_disabled = true;
    return false;
  };

  (*fn_do_validate)();
}

}  // namespace ora::mods
