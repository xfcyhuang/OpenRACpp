#include "game/input/hotkey.hpp"

#include <cctype>

namespace ora::game::input {

namespace {

// canonical 键名表（上游 C# Keycode 枚举名），与 Keycode 枚举声明一一对应。
struct KeycodeNameEntry {
  Keycode key;
  std::string_view str_name;
};

constexpr std::array<KeycodeNameEntry, 238> kKeycodeNames{{
    {Keycode::kUnknown, "UNKNOWN"},
    {Keycode::kReturn, "RETURN"},
    {Keycode::kEscape, "ESCAPE"},
    {Keycode::kBackspace, "BACKSPACE"},
    {Keycode::kTab, "TAB"},
    {Keycode::kSpace, "SPACE"},
    {Keycode::kExclaim, "EXCLAIM"},
    {Keycode::kQuotedbl, "QUOTEDBL"},
    {Keycode::kHash, "HASH"},
    {Keycode::kPercent, "PERCENT"},
    {Keycode::kDollar, "DOLLAR"},
    {Keycode::kAmpersand, "AMPERSAND"},
    {Keycode::kQuote, "QUOTE"},
    {Keycode::kLeftparen, "LEFTPAREN"},
    {Keycode::kRightparen, "RIGHTPAREN"},
    {Keycode::kAsterisk, "ASTERISK"},
    {Keycode::kPlus, "PLUS"},
    {Keycode::kComma, "COMMA"},
    {Keycode::kMinus, "MINUS"},
    {Keycode::kPeriod, "PERIOD"},
    {Keycode::kSlash, "SLASH"},
    {Keycode::kNumber0, "NUMBER_0"},
    {Keycode::kNumber1, "NUMBER_1"},
    {Keycode::kNumber2, "NUMBER_2"},
    {Keycode::kNumber3, "NUMBER_3"},
    {Keycode::kNumber4, "NUMBER_4"},
    {Keycode::kNumber5, "NUMBER_5"},
    {Keycode::kNumber6, "NUMBER_6"},
    {Keycode::kNumber7, "NUMBER_7"},
    {Keycode::kNumber8, "NUMBER_8"},
    {Keycode::kNumber9, "NUMBER_9"},
    {Keycode::kColon, "COLON"},
    {Keycode::kSemicolon, "SEMICOLON"},
    {Keycode::kLess, "LESS"},
    {Keycode::kEquals, "EQUALS"},
    {Keycode::kGreater, "GREATER"},
    {Keycode::kQuestion, "QUESTION"},
    {Keycode::kAt, "AT"},
    {Keycode::kLeftbracket, "LEFTBRACKET"},
    {Keycode::kBackslash, "BACKSLASH"},
    {Keycode::kRightbracket, "RIGHTBRACKET"},
    {Keycode::kCaret, "CARET"},
    {Keycode::kUnderscore, "UNDERSCORE"},
    {Keycode::kBackquote, "BACKQUOTE"},
    {Keycode::kA, "A"},
    {Keycode::kB, "B"},
    {Keycode::kC, "C"},
    {Keycode::kD, "D"},
    {Keycode::kE, "E"},
    {Keycode::kF, "F"},
    {Keycode::kG, "G"},
    {Keycode::kH, "H"},
    {Keycode::kI, "I"},
    {Keycode::kJ, "J"},
    {Keycode::kK, "K"},
    {Keycode::kL, "L"},
    {Keycode::kM, "M"},
    {Keycode::kN, "N"},
    {Keycode::kO, "O"},
    {Keycode::kP, "P"},
    {Keycode::kQ, "Q"},
    {Keycode::kR, "R"},
    {Keycode::kS, "S"},
    {Keycode::kT, "T"},
    {Keycode::kU, "U"},
    {Keycode::kV, "V"},
    {Keycode::kW, "W"},
    {Keycode::kX, "X"},
    {Keycode::kY, "Y"},
    {Keycode::kZ, "Z"},
    {Keycode::kCapslock, "CAPSLOCK"},
    {Keycode::kF1, "F1"},
    {Keycode::kF2, "F2"},
    {Keycode::kF3, "F3"},
    {Keycode::kF4, "F4"},
    {Keycode::kF5, "F5"},
    {Keycode::kF6, "F6"},
    {Keycode::kF7, "F7"},
    {Keycode::kF8, "F8"},
    {Keycode::kF9, "F9"},
    {Keycode::kF10, "F10"},
    {Keycode::kF11, "F11"},
    {Keycode::kF12, "F12"},
    {Keycode::kPrintscreen, "PRINTSCREEN"},
    {Keycode::kScrolllock, "SCROLLLOCK"},
    {Keycode::kPause, "PAUSE"},
    {Keycode::kInsert, "INSERT"},
    {Keycode::kHome, "HOME"},
    {Keycode::kPageup, "PAGEUP"},
    {Keycode::kDelete, "DELETE"},
    {Keycode::kEnd, "END"},
    {Keycode::kPagedown, "PAGEDOWN"},
    {Keycode::kRight, "RIGHT"},
    {Keycode::kLeft, "LEFT"},
    {Keycode::kDown, "DOWN"},
    {Keycode::kUp, "UP"},
    {Keycode::kNumlockclear, "NUMLOCKCLEAR"},
    {Keycode::kKpDivide, "KP_DIVIDE"},
    {Keycode::kKpMultiply, "KP_MULTIPLY"},
    {Keycode::kKpMinus, "KP_MINUS"},
    {Keycode::kKpPlus, "KP_PLUS"},
    {Keycode::kKpEnter, "KP_ENTER"},
    {Keycode::kKp1, "KP_1"},
    {Keycode::kKp2, "KP_2"},
    {Keycode::kKp3, "KP_3"},
    {Keycode::kKp4, "KP_4"},
    {Keycode::kKp5, "KP_5"},
    {Keycode::kKp6, "KP_6"},
    {Keycode::kKp7, "KP_7"},
    {Keycode::kKp8, "KP_8"},
    {Keycode::kKp9, "KP_9"},
    {Keycode::kKp0, "KP_0"},
    {Keycode::kKpPeriod, "KP_PERIOD"},
    {Keycode::kApplication, "APPLICATION"},
    {Keycode::kPower, "POWER"},
    {Keycode::kKpEquals, "KP_EQUALS"},
    {Keycode::kF13, "F13"},
    {Keycode::kF14, "F14"},
    {Keycode::kF15, "F15"},
    {Keycode::kF16, "F16"},
    {Keycode::kF17, "F17"},
    {Keycode::kF18, "F18"},
    {Keycode::kF19, "F19"},
    {Keycode::kF20, "F20"},
    {Keycode::kF21, "F21"},
    {Keycode::kF22, "F22"},
    {Keycode::kF23, "F23"},
    {Keycode::kF24, "F24"},
    {Keycode::kExecute, "EXECUTE"},
    {Keycode::kHelp, "HELP"},
    {Keycode::kMenu, "MENU"},
    {Keycode::kSelect, "SELECT"},
    {Keycode::kStop, "STOP"},
    {Keycode::kAgain, "AGAIN"},
    {Keycode::kUndo, "UNDO"},
    {Keycode::kCut, "CUT"},
    {Keycode::kCopy, "COPY"},
    {Keycode::kPaste, "PASTE"},
    {Keycode::kFind, "FIND"},
    {Keycode::kMute, "MUTE"},
    {Keycode::kVolumeup, "VOLUMEUP"},
    {Keycode::kVolumedown, "VOLUMEDOWN"},
    {Keycode::kKpComma, "KP_COMMA"},
    {Keycode::kKpEqualsas400, "KP_EQUALSAS400"},
    {Keycode::kAlterase, "ALTERASE"},
    {Keycode::kSysreq, "SYSREQ"},
    {Keycode::kCancel, "CANCEL"},
    {Keycode::kClear, "CLEAR"},
    {Keycode::kPrior, "PRIOR"},
    {Keycode::kReturn2, "RETURN2"},
    {Keycode::kSeparator, "SEPARATOR"},
    {Keycode::kOut, "OUT"},
    {Keycode::kOper, "OPER"},
    {Keycode::kClearagain, "CLEARAGAIN"},
    {Keycode::kCrsel, "CRSEL"},
    {Keycode::kExsel, "EXSEL"},
    {Keycode::kKp00, "KP_00"},
    {Keycode::kKp000, "KP_000"},
    {Keycode::kThousandsseparator, "THOUSANDSSEPARATOR"},
    {Keycode::kDecimalseparator, "DECIMALSEPARATOR"},
    {Keycode::kCurrencyunit, "CURRENCYUNIT"},
    {Keycode::kCurrencysubunit, "CURRENCYSUBUNIT"},
    {Keycode::kKpLeftparen, "KP_LEFTPAREN"},
    {Keycode::kKpRightparen, "KP_RIGHTPAREN"},
    {Keycode::kKpLeftbrace, "KP_LEFTBRACE"},
    {Keycode::kKpRightbrace, "KP_RIGHTBRACE"},
    {Keycode::kKpTab, "KP_TAB"},
    {Keycode::kKpBackspace, "KP_BACKSPACE"},
    {Keycode::kKpA, "KP_A"},
    {Keycode::kKpB, "KP_B"},
    {Keycode::kKpC, "KP_C"},
    {Keycode::kKpD, "KP_D"},
    {Keycode::kKpE, "KP_E"},
    {Keycode::kKpF, "KP_F"},
    {Keycode::kKpXor, "KP_XOR"},
    {Keycode::kKpPower, "KP_POWER"},
    {Keycode::kKpPercent, "KP_PERCENT"},
    {Keycode::kKpLess, "KP_LESS"},
    {Keycode::kKpGreater, "KP_GREATER"},
    {Keycode::kKpAmpersand, "KP_AMPERSAND"},
    {Keycode::kKpDblampersand, "KP_DBLAMPERSAND"},
    {Keycode::kKpVerticalbar, "KP_VERTICALBAR"},
    {Keycode::kKpDblverticalbar, "KP_DBLVERTICALBAR"},
    {Keycode::kKpColon, "KP_COLON"},
    {Keycode::kKpHash, "KP_HASH"},
    {Keycode::kKpSpace, "KP_SPACE"},
    {Keycode::kKpAt, "KP_AT"},
    {Keycode::kKpExclam, "KP_EXCLAM"},
    {Keycode::kKpMemstore, "KP_MEMSTORE"},
    {Keycode::kKpMemrecall, "KP_MEMRECALL"},
    {Keycode::kKpMemclear, "KP_MEMCLEAR"},
    {Keycode::kKpMemadd, "KP_MEMADD"},
    {Keycode::kKpMemsubtract, "KP_MEMSUBTRACT"},
    {Keycode::kKpMemmultiply, "KP_MEMMULTIPLY"},
    {Keycode::kKpMemdivide, "KP_MEMDIVIDE"},
    {Keycode::kKpPlusminus, "KP_PLUSMINUS"},
    {Keycode::kKpClear, "KP_CLEAR"},
    {Keycode::kKpClearentry, "KP_CLEARENTRY"},
    {Keycode::kKpBinary, "KP_BINARY"},
    {Keycode::kKpOctal, "KP_OCTAL"},
    {Keycode::kKpDecimal, "KP_DECIMAL"},
    {Keycode::kKpHexadecimal, "KP_HEXADECIMAL"},
    {Keycode::kLctrl, "LCTRL"},
    {Keycode::kLshift, "LSHIFT"},
    {Keycode::kLalt, "LALT"},
    {Keycode::kLgui, "LGUI"},
    {Keycode::kRctrl, "RCTRL"},
    {Keycode::kRshift, "RSHIFT"},
    {Keycode::kRalt, "RALT"},
    {Keycode::kRgui, "RGUI"},
    {Keycode::kMode, "MODE"},
    {Keycode::kAudionext, "AUDIONEXT"},
    {Keycode::kAudioprev, "AUDIOPREV"},
    {Keycode::kAudiostop, "AUDIOSTOP"},
    {Keycode::kAudioplay, "AUDIOPLAY"},
    {Keycode::kAudiomute, "AUDIOMUTE"},
    {Keycode::kMediaselect, "MEDIASELECT"},
    {Keycode::kWww, "WWW"},
    {Keycode::kMail, "MAIL"},
    {Keycode::kCalculator, "CALCULATOR"},
    {Keycode::kComputer, "COMPUTER"},
    {Keycode::kAcSearch, "AC_SEARCH"},
    {Keycode::kAcHome, "AC_HOME"},
    {Keycode::kAcBack, "AC_BACK"},
    {Keycode::kAcForward, "AC_FORWARD"},
    {Keycode::kAcStop, "AC_STOP"},
    {Keycode::kAcRefresh, "AC_REFRESH"},
    {Keycode::kAcBookmarks, "AC_BOOKMARKS"},
    {Keycode::kBrightnessdown, "BRIGHTNESSDOWN"},
    {Keycode::kBrightnessup, "BRIGHTNESSUP"},
    {Keycode::kDisplayswitch, "DISPLAYSWITCH"},
    {Keycode::kKbdillumtoggle, "KBDILLUMTOGGLE"},
    {Keycode::kKbdillumdown, "KBDILLUMDOWN"},
    {Keycode::kKbdillumup, "KBDILLUMUP"},
    {Keycode::kEject, "EJECT"},
    {Keycode::kSleep, "SLEEP"},
    {Keycode::kMouse4, "MOUSE4"},
    {Keycode::kMouse5, "MOUSE5"},
}};

bool equals_ignore_case(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i{0}; i < a.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

std::optional<ModifierKeys> modifier_token(std::string_view str_token) {
  if (equals_ignore_case(str_token, "None")) return ModifierKeys::kNone;
  if (equals_ignore_case(str_token, "Shift")) return ModifierKeys::kShift;
  if (equals_ignore_case(str_token, "Alt")) return ModifierKeys::kAlt;
  if (equals_ignore_case(str_token, "Ctrl")) return ModifierKeys::kCtrl;
  if (equals_ignore_case(str_token, "Meta")) return ModifierKeys::kMeta;
  return std::nullopt;
}

std::string_view trim_spaces(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

}

std::string_view keycode_name(Keycode key) {
  for (const KeycodeNameEntry& entry : kKeycodeNames)
    if (entry.key == key) return entry.str_name;
  return "UNKNOWN";
}

std::optional<Keycode> keycode_from_name(std::string_view str_name) {
  for (const KeycodeNameEntry& entry : kKeycodeNames)
    if (equals_ignore_case(entry.str_name, str_name)) return entry.key;
  return std::nullopt;
}

std::string modifiers_to_string(ModifierKeys mods) {
  const std::pair<ModifierKeys, std::string_view> arr_order[]{
      {ModifierKeys::kShift, "Shift"},
      {ModifierKeys::kAlt, "Alt"},
      {ModifierKeys::kCtrl, "Ctrl"},
      {ModifierKeys::kMeta, "Meta"},
  };
  std::string str_out;
  for (const auto& [mod, str_label] : arr_order) {
    if (!has_modifier(mods, mod)) continue;
    if (!str_out.empty()) str_out += ", ";
    str_out += str_label;
  }
  if (str_out.empty()) return "None";
  return str_out;
}

// 上游 C# Enum.TryParse<Modifiers>：逗号分隔 flags，token 忽略大小写；
// 空串/无 token 返回 nullopt（与 C# 空串解析失败一致）。
std::optional<ModifierKeys> modifiers_from_string(std::string_view str_text) {
  ModifierKeys mods{ModifierKeys::kNone};
  std::size_t idx_begin{0};
  bool b_any{false};
  while (idx_begin <= str_text.size()) {
    const std::size_t idx_comma{str_text.find(',', idx_begin)};
    const std::string_view str_token{trim_spaces(str_text.substr(
        idx_begin, idx_comma == std::string_view::npos ? std::string_view::npos
                                                       : idx_comma - idx_begin))};
    if (!str_token.empty()) {
      const auto opt{modifier_token(str_token)};
      if (!opt.has_value()) return std::nullopt;
      mods |= *opt;
      b_any = true;
    }
    if (idx_comma == std::string_view::npos) break;
    idx_begin = idx_comma + 1;
  }
  if (!b_any) return std::nullopt;
  return mods;
}

// 上游 Hotkey.TryParse（Hotkey.cs:27-45）：C# Split 目标槽为 2 时第二槽吃下
// 余下全文（"A B C" 的 mods 段为 "B C"，解析失败）——首个空格切两段等价。
std::expected<HotkeyModel, std::string> hotkey_try_parse(std::string_view str_text) {
  const std::size_t idx_space{str_text.find(' ')};
  const std::string_view str_key_part{
      idx_space == std::string_view::npos ? str_text : str_text.substr(0, idx_space)};
  const std::string_view str_mods_part{
      idx_space == std::string_view::npos ? std::string_view{} : str_text.substr(idx_space + 1)};

  const auto opt_key{keycode_from_name(str_key_part)};
  if (!opt_key.has_value())
    return std::unexpected(std::format("unknown keycode `{}'", str_key_part));

  ModifierKeys mods{ModifierKeys::kNone};
  if (!str_mods_part.empty()) {
    const auto opt_mods{modifiers_from_string(str_mods_part)};
    if (!opt_mods.has_value())
      return std::unexpected(std::format("unknown modifiers `{}'", str_mods_part));
    mods = *opt_mods;
  }
  return HotkeyModel{*opt_key, mods};
}

std::string hotkey_to_string(const HotkeyModel& hk) {
  return std::format("{} {}", keycode_name(hk.key), modifiers_to_string(hk.mods));
}

}
