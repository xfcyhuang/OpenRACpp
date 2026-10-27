#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

enum class TextFieldType : std::uint8_t { General, Filename, Integer };

class TextFieldWidget : public InputWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "TextFieldWidget";

  TextFieldWidget();
  explicit TextFieldWidget(WidgetArgs& args_args) : TextFieldWidget() { (void)args_args; }
  explicit TextFieldWidget(const TextFieldWidget& widget_other);
  ~TextFieldWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new TextFieldWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  const std::string& Text() const { return str_text_; }
  void SetText(std::string str_value);

  int int4_max_length = 0;
  int int4_visual_height = 1;
  int int4_left_margin = 5;
  int int4_right_margin = 5;
  std::string str_background = "textfield";

  TextFieldType Type() const { return kind_type_; }
  void SetType(TextFieldType kind_value);

  std::function<bool(const KeyInput&)> fn_on_enter_key = [](const KeyInput&) { return false; };
  std::function<bool(const KeyInput&)> fn_on_tab_key = [](const KeyInput&) { return false; };
  std::function<bool(const KeyInput&)> fn_on_esc_key = [](const KeyInput&) { return false; };
  std::function<bool(const KeyInput&)> fn_on_arrow_up = [](const KeyInput&) { return false; };
  std::function<bool(const KeyInput&)> fn_on_arrow_down = [](const KeyInput&) { return false; };
  std::function<bool()> fn_on_alt_key = [] { return false; };
  std::function<void()> fn_on_lose_focus = [] {};
  std::function<void()> fn_on_text_edited = [] {};
  int int4_cursor_position = 0;

  std::function<bool()> fn_is_valid = [] { return true; };
  std::string str_font;
  core::Color color_text{};
  core::Color color_text_disabled{};
  core::Color color_text_invalid{};
  core::Color color_text_highlight{};

  bool YieldKeyboardFocus() override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool HandleKeyPress(const KeyInput& e_input) override;
  bool HandleTextInput(std::string_view str_text) override;
  void Tick() override;
  void Draw() override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  int SelectionStartIndex() const { return int4_selection_start_index_; }
  int SelectionEndIndex() const { return int4_selection_end_index_; }
  bool ShowCursor() const { return b_show_cursor_; }

 protected:

  virtual const std::string& GetApparentText() const { return str_text_; }

  int ClosestCursorPosition(int int4_x);

  int GetPrevWhitespaceIndex();
  int GetNextWhitespaceIndex();

  std::string RemoveInvalidCharacters(std::string_view str_input) const;

  void HandleSelectionUpdate(int int4_prev_cursor_pos, int int4_new_cursor_pos);
  void ClearSelection();
  void RemoveSelectedText();
  void ResetBlinkCycle();

  int int4_selection_start_index_ = -1;
  int int4_selection_end_index_ = -1;
  bool b_mouse_selection_active_ = false;

  int int4_blink_cycle_ = 10;
  bool b_show_cursor_ = true;
  bool b_was_disabled_ = false;

 private:
  std::string str_text_;
  TextFieldType kind_type_ = TextFieldType::General;
};

}
