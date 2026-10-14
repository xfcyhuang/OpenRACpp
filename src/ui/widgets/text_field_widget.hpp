// UPSTREAM: OpenRA.Mods.Common/Widgets/TextFieldWidget.cs @e180e3225 L20-618
// (全文)。全编辑面:选择区双索引 + 鼠标拖选/键盘左移右移的词跳(Ctrl)/
// Home/End/Ctrl+D·K·U 的直写 backing field 面/Ctrl+X·C·V·A 的剪贴板对/
// 删除族(DELETE/BACKSPACE 的词删)/MaxLength 截断/blink 光标(20 tick 相位);
// 三类型字符过滤(Filename 的 Path.GetInvalidFileNameChars / Integer 的
// IsDigit)。Text/CursorPosition 的索引域 = UTF-8 字节(上游 UTF-16 单元;
// ASCII 域恒等,登记 COVERAGE);isOSX = game::CurrentPlatformIsOSX。
// The whole of TextFieldWidget.cs L20-618. The full editing face: the
// selection index pair + mouse drag-select / the keyboard arrows'
// Ctrl word-jumps / Home/End / Ctrl+D·K·U's direct-backing-field writes /
// the Ctrl+X·C·V·A clipboard quartet / the delete family (DELETE/BACKSPACE
// word-deletes) / MaxLength truncation / the blinking cursor (20-tick
// phase); the three-type character filter (Filename's Path.
// GetInvalidFileNameChars / Integer's IsDigit). Text/CursorPosition's
// index domain is UTF-8 bytes (upstream: UTF-16 units; identical over
// ASCII, registered); isOSX = game::CurrentPlatformIsOSX.
#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

/// TextFieldType(TextFieldWidget.cs L21)。
/// TextFieldType (TextFieldWidget.cs L21).
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

  /// Text 属性(setter 经 RemoveInvalidCharacters + 光标钳位 + 清选区)
  /// The Text property (the setter runs RemoveInvalidCharacters + cursor
  /// clamp + selection clear).
  const std::string& Text() const { return str_text_; }
  void SetText(std::string str_value);

  int int4_max_length = 0;
  int int4_visual_height = 1;
  int int4_left_margin = 5;
  int int4_right_margin = 5;
  std::string str_background = "textfield";

  /// Type 属性(setter 重验证文本)| The Type property (the setter
  /// revalidates the text).
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
  int int4_cursor_position = 0;  // CursorPosition{get; set;} 公开 | public

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

  /// 测试可见面 | the test-visible faces.
  int SelectionStartIndex() const { return int4_selection_start_index_; }
  int SelectionEndIndex() const { return int4_selection_end_index_; }
  bool ShowCursor() const { return b_show_cursor_; }

 protected:
  /// GetApparentText(L173;PasswordField 覆写面)| GetApparentText (L173;
  /// PasswordField's override face).
  virtual const std::string& GetApparentText() const { return str_text_; }

  /// ClosestCursorPosition(L175-192):前扫最近字节位(度量单调即断)。
  /// ClosestCursorPosition (L175-192): the forward scan for the nearest
  /// byte position (monotonic measure breaks early).
  int ClosestCursorPosition(int int4_x);

  int GetPrevWhitespaceIndex();
  int GetNextWhitespaceIndex();

  /// RemoveInvalidCharacters(L209-229)。
  /// RemoveInvalidCharacters (L209-229).
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

}  // namespace ora::ui
