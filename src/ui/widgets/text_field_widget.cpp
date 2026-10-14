// UPSTREAM: OpenRA.Mods.Common/Widgets/TextFieldWidget.cs @e180e3225
// (text_field_widget.hpp 的实现)
// The implementation of text_field_widget.hpp.
import std;

#include "ui/widgets/text_field_widget.hpp"

#include "core/text.hpp"
#include "core/vector_n.hpp"
#include "game/platform.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {
namespace {

/// Path.GetInvalidFileNameChars 的 Windows 域 41 字符(0x00-0x1F 控制位 +
/// 九个标点;多字节 UTF-8 序列的字节均 ≥0x80,永不命中 —— 字节域过滤 =
/// 码点域过滤)
/// Path.GetInvalidFileNameChars' 41-character Windows set (the 0x00-0x1F
/// control range + the nine punctuation; a multibyte UTF-8 sequence's bytes
/// are all >= 0x80 and never hit — a byte-domain filter equals the
/// code-point-domain filter).
bool IsInvalidFilenameChar(char chr_value) {
  const unsigned char uint1_c{static_cast<unsigned char>(chr_value)};
  return uint1_c < 0x20 || uint1_c == '"' || uint1_c == '<' || uint1_c == '>' ||
         uint1_c == '|' || uint1_c == ':' || uint1_c == '*' || uint1_c == '?' ||
         uint1_c == '\\' || uint1_c == '/';
}

int ClampInt(int int4_v, int int4_lo, int int4_hi) {
  return std::max(int4_lo, std::min(int4_v, int4_hi));
}

}  // namespace

TextFieldWidget::TextFieldWidget() {
  ChromeMetrics& metrics = RequireChromeMetrics();
  str_font = metrics.Get("TextfieldFont");
  color_text = metrics.GetColor("TextfieldColor");
  color_text_disabled = metrics.GetColor("TextfieldColorDisabled");
  color_text_invalid = metrics.GetColor("TextfieldColorInvalid");
  color_text_highlight = metrics.GetColor("TextfieldColorHighlight");
}

TextFieldWidget::TextFieldWidget(const TextFieldWidget& widget_other)
    : InputWidget(widget_other),
      int4_max_length(widget_other.int4_max_length),
      int4_left_margin(widget_other.int4_left_margin),
      int4_right_margin(widget_other.int4_right_margin),
      str_font(widget_other.str_font),
      color_text(widget_other.color_text),
      color_text_disabled(widget_other.color_text_disabled),
      color_text_invalid(widget_other.color_text_invalid),
      color_text_highlight(widget_other.color_text_highlight),
      int4_visual_height(widget_other.int4_visual_height),
      str_text_(widget_other.str_text_),
      kind_type_(widget_other.kind_type_) {
  int4_cursor_position = ClampInt(widget_other.int4_cursor_position, 0,
                                  static_cast<int>(str_text_.size()));
}

void TextFieldWidget::SetText(std::string str_value) {
  str_text_ = RemoveInvalidCharacters(str_value);
  int4_cursor_position =
      ClampInt(int4_cursor_position, 0, static_cast<int>(str_text_.size()));
  ClearSelection();
}

void TextFieldWidget::SetType(TextFieldType kind_value) {
  kind_type_ = kind_value;

  // Revalidate text(上游注释)| Revalidate the text (the upstream comment).
  str_text_ = RemoveInvalidCharacters(str_text_);
  int4_cursor_position =
      ClampInt(int4_cursor_position, 0, static_cast<int>(str_text_.size()));
}

bool TextFieldWidget::YieldKeyboardFocus() {
  fn_on_lose_focus();
  return InputWidget::YieldKeyboardFocus();
}

void TextFieldWidget::ResetBlinkCycle() {
  int4_blink_cycle_ = 10;
  b_show_cursor_ = true;
}

bool TextFieldWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (fn_is_disabled())
    return false;

  if (b_mouse_selection_active_) {
    if (mi_input.Event == MouseInputEvent::Up) {
      b_mouse_selection_active_ = false;
      return true;
    } else if (mi_input.Event != MouseInputEvent::Move)
      return false;
  } else if (mi_input.Event != MouseInputEvent::Down)
    return false;

  // Attempt to take keyboard focus(上游注释)| the upstream comment.
  if (!RenderBounds().Contains(mi_input.Location) || !TakeKeyboardFocus())
    return false;

  b_mouse_selection_active_ = true;

  ResetBlinkCycle();

  const int int4_cached_cursor_pos = int4_cursor_position;
  int4_cursor_position = ClosestCursorPosition(mi_input.Location.X);

  if (HasModifier(mi_input.ModifierFlags, Modifiers::Shift) ||
      (mi_input.Event == MouseInputEvent::Move && b_mouse_selection_active_))
    HandleSelectionUpdate(int4_cached_cursor_pos, int4_cursor_position);
  else
    ClearSelection();

  return true;
}

int TextFieldWidget::ClosestCursorPosition(int int4_x) {
  const std::string& str_apparent = GetApparentText();
  gfx::SpriteFont& font_text = RequireWidgetFont(str_font);
  const int2 int2_text_size = font_text.Measure(str_apparent);

  int int4_start = RenderOrigin().X + int4_left_margin;
  if (int2_text_size.X > bounds_widget.int4_width - int4_left_margin - int4_right_margin &&
      HasKeyboardFocus())
    int4_start += bounds_widget.int4_width - int4_left_margin - int4_right_margin -
                  int2_text_size.X;

  int int4_min_index = -1;
  int int4_min_value = std::numeric_limits<int>::max();
  for (int int4_i = 0; int4_i <= static_cast<int>(str_apparent.size()); ++int4_i) {
    const int int4_dist =
        std::abs(int4_start +
                     font_text.Measure(str_apparent.substr(0, int4_i)).X - int4_x);
    if (int4_dist > int4_min_value)
      break;
    int4_min_value = int4_dist;
    int4_min_index = int4_i;
  }

  return int4_min_index;
}

int TextFieldWidget::GetPrevWhitespaceIndex() {
  const std::string_view sv_before = TrimEndNetWhiteSpace(
      std::string_view{str_text_}.substr(0, int4_cursor_position));
  const std::size_t int8_last_space = sv_before.find_last_of(' ');
  return int8_last_space == std::string_view::npos
             ? 0
             : static_cast<int>(int8_last_space) + 1;
}

int TextFieldWidget::GetNextWhitespaceIndex() {
  const std::string_view sv_substr =
      std::string_view{str_text_}.substr(int4_cursor_position);
  const std::string_view sv_trimmed = TrimStartNetWhiteSpace(sv_substr);
  const std::size_t int8_trimmed_spaces = sv_substr.size() - sv_trimmed.size();
  const std::size_t int8_next_whitespace = sv_trimmed.find(' ');
  if (int8_next_whitespace == std::string_view::npos)
    return static_cast<int>(str_text_.size());

  return static_cast<int>(int4_cursor_position + int8_trimmed_spaces +
                          int8_next_whitespace);
}

std::string TextFieldWidget::RemoveInvalidCharacters(std::string_view str_input) const {
  switch (kind_type_) {
    case TextFieldType::Filename: {
      std::string str_ret;
      str_ret.reserve(str_input.size());
      for (const char chr_c : str_input)
        if (!IsInvalidFilenameChar(chr_c))
          str_ret.push_back(chr_c);
      return str_ret;
    }

    case TextFieldType::Integer: {
      // char.IsDigit 域:多字节序列字节 ≥0x80 永非 ASCII 数字(等价过滤)
      // The char.IsDigit domain: multibyte bytes are >= 0x80, never ASCII
      // digits (the equivalent filter).
      std::string str_ret;
      str_ret.reserve(str_input.size());
      for (const char chr_c : str_input)
        if (chr_c >= '0' && chr_c <= '9')
          str_ret.push_back(chr_c);
      return str_ret;
    }

    default:
      return std::string{str_input};
  }
}

bool TextFieldWidget::HandleKeyPress(const KeyInput& e_input) {
  if (fn_is_disabled() || e_input.Event == KeyInputEvent::Up)
    return false;

  // Only take input if we are focused(上游注释)| the upstream comment.
  if (!HasKeyboardFocus())
    return false;

  const bool b_is_osx = game::CurrentPlatformIsOSX();

  switch (e_input.Key) {
    case Keycode::RETURN:
    case Keycode::KP_ENTER:
      if (fn_on_enter_key(e_input))
        return true;
      break;

    case Keycode::TAB:
      if (fn_on_tab_key(e_input))
        return true;
      break;

    case Keycode::ESCAPE:
      ClearSelection();
      if (fn_on_esc_key(e_input))
        return true;
      break;

    case Keycode::LALT:
      if (fn_on_alt_key())
        return true;
      break;

    case Keycode::LEFT:
      ResetBlinkCycle();
      if (int4_cursor_position > 0) {
        const int int4_cached_current_cursor_pos = int4_cursor_position;

        if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
            (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Alt)))
          int4_cursor_position = GetPrevWhitespaceIndex();
        else if (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))
          int4_cursor_position = 0;
        else
          --int4_cursor_position;

        if (HasModifier(e_input.ModifierFlags, Modifiers::Shift))
          HandleSelectionUpdate(int4_cached_current_cursor_pos, int4_cursor_position);
        else
          ClearSelection();
      }

      break;

    case Keycode::RIGHT:
      ResetBlinkCycle();
      if (int4_cursor_position <= static_cast<int>(str_text_.size()) - 1) {
        const int int4_cached_current_cursor_pos = int4_cursor_position;

        if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
            (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Alt)))
          int4_cursor_position = GetNextWhitespaceIndex();
        else if (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))
          int4_cursor_position = static_cast<int>(str_text_.size());
        else
          ++int4_cursor_position;

        if (HasModifier(e_input.ModifierFlags, Modifiers::Shift))
          HandleSelectionUpdate(int4_cached_current_cursor_pos, int4_cursor_position);
        else
          ClearSelection();
      }

      break;

    case Keycode::UP:
      if (fn_on_arrow_up(e_input))
        return true;
      break;

    case Keycode::DOWN:
      if (fn_on_arrow_down(e_input))
        return true;
      break;

    case Keycode::HOME:
      ResetBlinkCycle();
      if (HasModifier(e_input.ModifierFlags, Modifiers::Shift))
        HandleSelectionUpdate(int4_cursor_position, 0);
      else
        ClearSelection();

      int4_cursor_position = 0;
      break;

    case Keycode::END:
      ResetBlinkCycle();

      if (HasModifier(e_input.ModifierFlags, Modifiers::Shift))
        HandleSelectionUpdate(int4_cursor_position, static_cast<int>(str_text_.size()));
      else
        ClearSelection();

      int4_cursor_position = static_cast<int>(str_text_.size());
      break;

    case Keycode::D:
      if (HasModifier(e_input.ModifierFlags, Modifiers::Ctrl) &&
          int4_cursor_position < static_cast<int>(str_text_.size())) {
        // Write directly to the Text backing field to avoid unnecessary
        // validation(上游注释)| the upstream comment.
        str_text_.erase(int4_cursor_position, 1);
        int4_cursor_position =
            ClampInt(int4_cursor_position, 0, static_cast<int>(str_text_.size()));

        fn_on_text_edited();
      }

      break;

    case Keycode::K:
      // ctrl+k is equivalent to cmd+delete on osx (but also works on osx)
      // (上游注释)| the upstream comment.
      ResetBlinkCycle();
      if (HasModifier(e_input.ModifierFlags, Modifiers::Ctrl) &&
          int4_cursor_position < static_cast<int>(str_text_.size())) {
        // Write directly to the Text backing field to avoid unnecessary
        // validation(上游注释)| the upstream comment.
        str_text_ = str_text_.substr(0, int4_cursor_position);
        int4_cursor_position =
            ClampInt(int4_cursor_position, 0, static_cast<int>(str_text_.size()));

        fn_on_text_edited();
      }

      break;

    case Keycode::U:
      // ctrl+u is equivalent to cmd+backspace on osx(上游注释)| the
      // upstream comment.
      ResetBlinkCycle();
      if (!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl) &&
          int4_cursor_position > 0) {
        // Write directly to the Text backing field to avoid unnecessary
        // validation(上游注释)| the upstream comment.
        str_text_ = str_text_.substr(int4_cursor_position);
        int4_cursor_position = 0;
        ClearSelection();
        fn_on_text_edited();
      }

      break;

    case Keycode::X:
      ResetBlinkCycle();
      if (((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
           (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))) &&
          !str_text_.empty() && int4_selection_start_index_ != -1) {
        const int int4_lowest_index =
            std::min(int4_selection_start_index_, int4_selection_end_index_);
        const int int4_highest_index =
            std::max(int4_selection_start_index_, int4_selection_end_index_);
        RequireRenderer().SetClipboardText(
            str_text_.substr(int4_lowest_index, int4_highest_index - int4_lowest_index));

        RemoveSelectedText();
      }

      break;

    case Keycode::C:
      ResetBlinkCycle();
      if (((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
           (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))) &&
          !str_text_.empty() && int4_selection_start_index_ != -1) {
        const int int4_lowest_index =
            std::min(int4_selection_start_index_, int4_selection_end_index_);
        const int int4_highest_index =
            std::max(int4_selection_start_index_, int4_selection_end_index_);
        RequireRenderer().SetClipboardText(
            str_text_.substr(int4_lowest_index, int4_highest_index - int4_lowest_index));
      }

      break;

    case Keycode::DELETE:
      // cmd+delete is equivalent to ctrl+k on non-osx(上游注释)| the
      // upstream comment.
      ResetBlinkCycle();
      if (int4_selection_start_index_ != -1)
        RemoveSelectedText();
      else if (int4_cursor_position < static_cast<int>(str_text_.size())) {
        // Write directly to the Text backing field to avoid unnecessary
        // validation(上游注释)| the upstream comment.
        if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
            (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Alt)))
          str_text_ = str_text_.substr(0, int4_cursor_position) +
                      str_text_.substr(GetNextWhitespaceIndex());
        else if (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))
          str_text_ = str_text_.substr(0, int4_cursor_position);
        else
          str_text_.erase(int4_cursor_position, 1);

        int4_cursor_position =
            ClampInt(int4_cursor_position, 0, static_cast<int>(str_text_.size()));
        fn_on_text_edited();
      }

      break;

    case Keycode::BACKSPACE:
      // cmd+backspace is equivalent to ctrl+u on non-osx(上游注释)| the
      // upstream comment.
      ResetBlinkCycle();
      if (int4_selection_start_index_ != -1)
        RemoveSelectedText();
      else if (int4_cursor_position > 0) {
        // Write directly to the Text backing field to avoid unnecessary
        // validation(上游注释)| the upstream comment.
        if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
            (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Alt))) {
          const int int4_prev_whitespace = GetPrevWhitespaceIndex();
          str_text_ = str_text_.substr(0, int4_prev_whitespace) +
                      str_text_.substr(int4_cursor_position);
          int4_cursor_position = int4_prev_whitespace;
        } else if (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta)) {
          str_text_ = str_text_.substr(int4_cursor_position);
          int4_cursor_position = 0;
        } else {
          --int4_cursor_position;
          str_text_.erase(int4_cursor_position, 1);
        }

        fn_on_text_edited();
      }

      break;

    case Keycode::V:
      ResetBlinkCycle();

      if (int4_selection_start_index_ != -1)
        RemoveSelectedText();

      if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
          (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))) {
        std::string str_clipboard_text = RequireRenderer().GetClipboardText();

        // Take only the first line of the clipboard contents(上游注释)|
        // the upstream comment.
        const std::size_t int8_nl = str_clipboard_text.find('\n');
        if (int8_nl > 0 && int8_nl != std::string::npos)
          str_clipboard_text = str_clipboard_text.substr(0, int8_nl);

        str_clipboard_text = TrimNetWhiteSpace(str_clipboard_text);
        if (!str_clipboard_text.empty())
          HandleTextInput(str_clipboard_text);
      }

      break;

    case Keycode::A:
      // Ctrl+A as Select-All, or Cmd+A on OSX(上游注释)| the upstream
      // comment.
      if ((!b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Ctrl)) ||
          (b_is_osx && HasModifier(e_input.ModifierFlags, Modifiers::Meta))) {
        ClearSelection();
        HandleSelectionUpdate(0, static_cast<int>(str_text_.size()));
      }

      break;

    default:
      break;
  }

  return true;
}

bool TextFieldWidget::HandleTextInput(std::string_view str_text) {
  if (!HasKeyboardFocus() || fn_is_disabled())
    return false;

  // Validate input(上游注释)| the upstream comment.
  const std::string str_input = RemoveInvalidCharacters(str_text);
  if (str_input.empty())
    return true;

  if (int4_selection_start_index_ != -1)
    RemoveSelectedText();

  if (int4_max_length > 0 &&
      static_cast<int>(str_text_.size()) >= int4_max_length)
    return true;

  int int4_paste_length = static_cast<int>(str_input.size());

  // Truncate the pasted string if the total length (current + paste) is
  // greater than the maximum.(上游注释)| the upstream comment.
  if (int4_max_length > 0 && int4_max_length > static_cast<int>(str_text_.size()))
    int4_paste_length = std::min(
        int4_paste_length, int4_max_length - static_cast<int>(str_text_.size()));

  // Write directly to the Text backing field to avoid repeating the invalid
  // character validation(上游注释)| the upstream comment.
  str_text_.insert(int4_cursor_position, str_input, 0, int4_paste_length);
  int4_cursor_position += int4_paste_length;
  ClearSelection();
  fn_on_text_edited();

  return true;
}

void TextFieldWidget::HandleSelectionUpdate(int int4_prev_cursor_pos,
                                            int int4_new_cursor_pos) {
  // If selection index is -1, there's no selection already open so create
  // one(上游注释)| the upstream comment.
  if (int4_selection_start_index_ == -1)
    int4_selection_start_index_ = int4_prev_cursor_pos;

  int4_selection_end_index_ = int4_new_cursor_pos;

  if (int4_selection_start_index_ == int4_selection_end_index_)
    ClearSelection();
}

void TextFieldWidget::ClearSelection() {
  int4_selection_start_index_ = -1;
  int4_selection_end_index_ = -1;
}

void TextFieldWidget::RemoveSelectedText() {
  if (int4_selection_start_index_ != -1) {
    const int int4_lowest_index =
        std::min(int4_selection_start_index_, int4_selection_end_index_);
    const int int4_highest_index =
        std::max(int4_selection_start_index_, int4_selection_end_index_);

    // Write directly to the Text backing field to avoid unnecessary
    // validation(上游注释)| the upstream comment.
    str_text_.erase(int4_lowest_index, int4_highest_index - int4_lowest_index);

    ClearSelection();

    int4_cursor_position = int4_lowest_index;
    fn_on_text_edited();
  }
}

void TextFieldWidget::Tick() {
  // Remove the blinking cursor when disabled(上游注释)| the upstream
  // comment.
  const bool b_is_disabled = fn_is_disabled();
  if (b_is_disabled != b_was_disabled_) {
    b_was_disabled_ = b_is_disabled;
    if (b_is_disabled && Ui::Instance().KeyboardFocusWidget() == this)
      YieldKeyboardFocus();
  }

  if (--int4_blink_cycle_ <= 0) {
    int4_blink_cycle_ = 20;
    b_show_cursor_ = !b_show_cursor_;
  }
}

void TextFieldWidget::Draw() {
  const std::string& str_apparent_text = GetApparentText();
  gfx::SpriteFont& font_text = RequireWidgetFont(str_font);
  const int2 int2_pos = RenderOrigin();

  const int2 int2_text_size = font_text.Measure(str_apparent_text);
  const int2 int2_cursor_offset =
      font_text.Measure(str_apparent_text.substr(0, int4_cursor_position));

  const bool b_disabled = fn_is_disabled();
  bool b_hover = Ui::Instance().MouseOverWidget() == this;
  if (!b_hover)
    for (Widget* ptr_child : Children())
      if (ptr_child == Ui::Instance().MouseOverWidget()) {
        b_hover = true;
        break;
      }
  const std::string str_state = GetStatefulImageName(str_background, b_disabled, false,
                                                     b_hover, HasKeyboardFocus());

  DrawPanel(str_state, Rectangle{int2_pos.X, int2_pos.Y, bounds_widget.int4_width,
                                 bounds_widget.int4_height});

  // Inset text by the margin and center vertically(上游注释)| the upstream
  // comment.
  const int int4_vertical_margin =
      (bounds_widget.int4_height - int2_text_size.Y) / 2 - int4_visual_height;
  int2 int2_text_pos = int2_pos + int2{int4_left_margin, int4_vertical_margin};

  // Right align when editing and scissor when the text overflows(上游注释)
  // | the upstream comment.
  const bool b_is_text_overflowing =
      int2_text_size.X > bounds_widget.int4_width - int4_left_margin - int4_right_margin;
  if (b_is_text_overflowing) {
    if (HasKeyboardFocus())
      int2_text_pos = int2_text_pos +
                      int2{bounds_widget.int4_width - int4_left_margin - int4_right_margin -
                               int2_text_size.X,
                           0};

    RequireRenderer().EnableScissor(
        Rectangle{int2_pos.X + int4_left_margin, int2_pos.Y,
                  bounds_widget.int4_width - int4_left_margin - int4_right_margin,
                  bounds_widget.Bottom()});
  }

  // Draw the highlight around the selected area(上游注释)| the upstream
  // comment.
  if (int4_selection_start_index_ != -1) {
    const int int4_visual_selection_start_index =
        std::min(int4_selection_start_index_, int4_selection_end_index_);
    const int int4_visual_selection_end_index =
        std::max(int4_selection_start_index_, int4_selection_end_index_);
    const int int4_highlight_start_x =
        font_text.Measure(str_apparent_text.substr(0, int4_visual_selection_start_index)).X;
    const int int4_highlight_end_x =
        font_text.Measure(str_apparent_text.substr(0, int4_visual_selection_end_index)).X;

    FillRectWithColor(Rectangle{int2_text_pos.X + int4_highlight_start_x, int2_text_pos.Y,
                                int4_highlight_end_x - int4_highlight_start_x,
                                bounds_widget.int4_height - int4_vertical_margin * 2},
                      color_text_highlight);
  }

  const core::Color color_draw =
      b_disabled ? color_text_disabled
                 : fn_is_valid() ? color_text
                                 : color_text_invalid;
  font_text.DrawText(str_apparent_text,
                     core::Vector2{static_cast<float>(int2_text_pos.X),
                                   static_cast<float>(int2_text_pos.Y)},
                     color_draw);

  if (b_show_cursor_ && HasKeyboardFocus())
    font_text.DrawText("|",
                       core::Vector2{
                           static_cast<float>(int2_text_pos.X + int2_cursor_offset.X - 2),
                           static_cast<float>(int2_text_pos.Y)},
                       color_text);

  if (b_is_text_overflowing)
    RequireRenderer().DisableScissor();
}

bool TextFieldWidget::LoadFieldOrProperty(std::string_view str_key,
                                          std::string_view str_value) {
  if (str_key == "Text") {
    SetText(std::string{str_value});
    return true;
  }
  if (str_key == "MaxLength") {
    int4_max_length = meta::GetInt32Value("MaxLength", str_value);
    return true;
  }
  if (str_key == "VisualHeight") {
    int4_visual_height = meta::GetInt32Value("VisualHeight", str_value);
    return true;
  }
  if (str_key == "LeftMargin") {
    int4_left_margin = meta::GetInt32Value("LeftMargin", str_value);
    return true;
  }
  if (str_key == "RightMargin") {
    int4_right_margin = meta::GetInt32Value("RightMargin", str_value);
    return true;
  }
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "Type") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "General")
      SetType(TextFieldType::General);
    else if (str_value_owned == "Filename")
      SetType(TextFieldType::Filename);
    else if (str_value_owned == "Integer")
      SetType(TextFieldType::Integer);
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `Type` of type `OpenRA.Mods.Common.Widgets.TextFieldType`");
    return true;
  }
  if (str_key == "TextColor") {
    color_text = meta::GetColorValue("TextColor", str_value);
    return true;
  }
  if (str_key == "TextColorDisabled") {
    color_text_disabled = meta::GetColorValue("TextColorDisabled", str_value);
    return true;
  }
  if (str_key == "TextColorInvalid") {
    color_text_invalid = meta::GetColorValue("TextColorInvalid", str_value);
    return true;
  }
  if (str_key == "TextColorHighlight") {
    color_text_highlight = meta::GetColorValue("TextColorHighlight", str_value);
    return true;
  }
  if (str_key == "Font") {
    str_font = std::string{str_value};
    return true;
  }

  return InputWidget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
