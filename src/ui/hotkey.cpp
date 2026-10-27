import std;

#include "ui/hotkey.hpp"

#include "fluent/fluent_provider.hpp"
#include "meta/field_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {
namespace {

struct KeycodeNameEntry {
  const char* str_name;
  Keycode key_kind;
  const char* str_fluent;
};

constexpr std::array<KeycodeNameEntry, 238> kKeycodeNames{{
    {"UNKNOWN", Keycode::UNKNOWN, "keycode.unknown"},
    {"RETURN", Keycode::RETURN, "keycode.return"},
    {"ESCAPE", Keycode::ESCAPE, "keycode.escape"},
    {"BACKSPACE", Keycode::BACKSPACE, "keycode.backspace"},
    {"TAB", Keycode::TAB, "keycode.tab"},
    {"SPACE", Keycode::SPACE, "keycode.space"},
    {"EXCLAIM", Keycode::EXCLAIM, "keycode.exclaim"},
    {"QUOTEDBL", Keycode::QUOTEDBL, "keycode.quotedbl"},
    {"HASH", Keycode::HASH, "keycode.hash"},
    {"PERCENT", Keycode::PERCENT, "keycode.percent"},
    {"DOLLAR", Keycode::DOLLAR, "keycode.dollar"},
    {"AMPERSAND", Keycode::AMPERSAND, "keycode.ampersand"},
    {"QUOTE", Keycode::QUOTE, "keycode.quote"},
    {"LEFTPAREN", Keycode::LEFTPAREN, "keycode.leftparen"},
    {"RIGHTPAREN", Keycode::RIGHTPAREN, "keycode.rightparen"},
    {"ASTERISK", Keycode::ASTERISK, "keycode.asterisk"},
    {"PLUS", Keycode::PLUS, "keycode.plus"},
    {"COMMA", Keycode::COMMA, "keycode.comma"},
    {"MINUS", Keycode::MINUS, "keycode.minus"},
    {"PERIOD", Keycode::PERIOD, "keycode.period"},
    {"SLASH", Keycode::SLASH, "keycode.slash"},
    {"NUMBER_0", Keycode::NUMBER_0, "keycode.number_0"},
    {"NUMBER_1", Keycode::NUMBER_1, "keycode.number_1"},
    {"NUMBER_2", Keycode::NUMBER_2, "keycode.number_2"},
    {"NUMBER_3", Keycode::NUMBER_3, "keycode.number_3"},
    {"NUMBER_4", Keycode::NUMBER_4, "keycode.number_4"},
    {"NUMBER_5", Keycode::NUMBER_5, "keycode.number_5"},
    {"NUMBER_6", Keycode::NUMBER_6, "keycode.number_6"},
    {"NUMBER_7", Keycode::NUMBER_7, "keycode.number_7"},
    {"NUMBER_8", Keycode::NUMBER_8, "keycode.number_8"},
    {"NUMBER_9", Keycode::NUMBER_9, "keycode.number_9"},
    {"COLON", Keycode::COLON, "keycode.colon"},
    {"SEMICOLON", Keycode::SEMICOLON, "keycode.semicolon"},
    {"LESS", Keycode::LESS, "keycode.less"},
    {"EQUALS", Keycode::EQUALS, "keycode.equals"},
    {"GREATER", Keycode::GREATER, "keycode.greater"},
    {"QUESTION", Keycode::QUESTION, "keycode.question"},
    {"AT", Keycode::AT, "keycode.at"},
    {"LEFTBRACKET", Keycode::LEFTBRACKET, "keycode.leftbracket"},
    {"BACKSLASH", Keycode::BACKSLASH, "keycode.backslash"},
    {"RIGHTBRACKET", Keycode::RIGHTBRACKET, "keycode.rightbracket"},
    {"CARET", Keycode::CARET, "keycode.caret"},
    {"UNDERSCORE", Keycode::UNDERSCORE, "keycode.underscore"},
    {"BACKQUOTE", Keycode::BACKQUOTE, "keycode.backquote"},
    {"A", Keycode::A, "keycode.a"},
    {"B", Keycode::B, "keycode.b"},
    {"C", Keycode::C, "keycode.c"},
    {"D", Keycode::D, "keycode.d"},
    {"E", Keycode::E, "keycode.e"},
    {"F", Keycode::F, "keycode.f"},
    {"G", Keycode::G, "keycode.g"},
    {"H", Keycode::H, "keycode.h"},
    {"I", Keycode::I, "keycode.i"},
    {"J", Keycode::J, "keycode.j"},
    {"K", Keycode::K, "keycode.k"},
    {"L", Keycode::L, "keycode.l"},
    {"M", Keycode::M, "keycode.m"},
    {"N", Keycode::N, "keycode.n"},
    {"O", Keycode::O, "keycode.o"},
    {"P", Keycode::P, "keycode.p"},
    {"Q", Keycode::Q, "keycode.q"},
    {"R", Keycode::R, "keycode.r"},
    {"S", Keycode::S, "keycode.s"},
    {"T", Keycode::T, "keycode.t"},
    {"U", Keycode::U, "keycode.u"},
    {"V", Keycode::V, "keycode.v"},
    {"W", Keycode::W, "keycode.w"},
    {"X", Keycode::X, "keycode.x"},
    {"Y", Keycode::Y, "keycode.y"},
    {"Z", Keycode::Z, "keycode.z"},
    {"CAPSLOCK", Keycode::CAPSLOCK, "keycode.capslock"},
    {"F1", Keycode::F1, "keycode.f1"},
    {"F2", Keycode::F2, "keycode.f2"},
    {"F3", Keycode::F3, "keycode.f3"},
    {"F4", Keycode::F4, "keycode.f4"},
    {"F5", Keycode::F5, "keycode.f5"},
    {"F6", Keycode::F6, "keycode.f6"},
    {"F7", Keycode::F7, "keycode.f7"},
    {"F8", Keycode::F8, "keycode.f8"},
    {"F9", Keycode::F9, "keycode.f9"},
    {"F10", Keycode::F10, "keycode.f10"},
    {"F11", Keycode::F11, "keycode.f11"},
    {"F12", Keycode::F12, "keycode.f12"},
    {"PRINTSCREEN", Keycode::PRINTSCREEN, "keycode.printscreen"},
    {"SCROLLLOCK", Keycode::SCROLLLOCK, "keycode.scrolllock"},
    {"PAUSE", Keycode::PAUSE, "keycode.pause"},
    {"INSERT", Keycode::INSERT, "keycode.insert"},
    {"HOME", Keycode::HOME, "keycode.home"},
    {"PAGEUP", Keycode::PAGEUP, "keycode.pageup"},
    {"DELETE", Keycode::DELETE, "keycode.delete"},
    {"END", Keycode::END, "keycode.end"},
    {"PAGEDOWN", Keycode::PAGEDOWN, "keycode.pagedown"},
    {"RIGHT", Keycode::RIGHT, "keycode.right"},
    {"LEFT", Keycode::LEFT, "keycode.left"},
    {"DOWN", Keycode::DOWN, "keycode.down"},
    {"UP", Keycode::UP, "keycode.up"},
    {"NUMLOCKCLEAR", Keycode::NUMLOCKCLEAR, "keycode.numlockclear"},
    {"KP_DIVIDE", Keycode::KP_DIVIDE, "keycode.kp_divide"},
    {"KP_MULTIPLY", Keycode::KP_MULTIPLY, "keycode.kp_multiply"},
    {"KP_MINUS", Keycode::KP_MINUS, "keycode.kp_minus"},
    {"KP_PLUS", Keycode::KP_PLUS, "keycode.kp_plus"},
    {"KP_ENTER", Keycode::KP_ENTER, "keycode.kp_enter"},
    {"KP_1", Keycode::KP_1, "keycode.kp_1"},
    {"KP_2", Keycode::KP_2, "keycode.kp_2"},
    {"KP_3", Keycode::KP_3, "keycode.kp_3"},
    {"KP_4", Keycode::KP_4, "keycode.kp_4"},
    {"KP_5", Keycode::KP_5, "keycode.kp_5"},
    {"KP_6", Keycode::KP_6, "keycode.kp_6"},
    {"KP_7", Keycode::KP_7, "keycode.kp_7"},
    {"KP_8", Keycode::KP_8, "keycode.kp_8"},
    {"KP_9", Keycode::KP_9, "keycode.kp_9"},
    {"KP_0", Keycode::KP_0, "keycode.kp_0"},
    {"KP_PERIOD", Keycode::KP_PERIOD, "keycode.kp_period"},
    {"APPLICATION", Keycode::APPLICATION, "keycode.application"},
    {"POWER", Keycode::POWER, "keycode.power"},
    {"KP_EQUALS", Keycode::KP_EQUALS, "keycode.kp_equals"},
    {"F13", Keycode::F13, "keycode.f13"},
    {"F14", Keycode::F14, "keycode.f14"},
    {"F15", Keycode::F15, "keycode.f15"},
    {"F16", Keycode::F16, "keycode.f16"},
    {"F17", Keycode::F17, "keycode.f17"},
    {"F18", Keycode::F18, "keycode.f18"},
    {"F19", Keycode::F19, "keycode.f19"},
    {"F20", Keycode::F20, "keycode.f20"},
    {"F21", Keycode::F21, "keycode.f21"},
    {"F22", Keycode::F22, "keycode.f22"},
    {"F23", Keycode::F23, "keycode.f23"},
    {"F24", Keycode::F24, "keycode.f24"},
    {"EXECUTE", Keycode::EXECUTE, "keycode.execute"},
    {"HELP", Keycode::HELP, "keycode.help"},
    {"MENU", Keycode::MENU, "keycode.menu"},
    {"SELECT", Keycode::SELECT, "keycode.select"},
    {"STOP", Keycode::STOP, "keycode.stop"},
    {"AGAIN", Keycode::AGAIN, "keycode.again"},
    {"UNDO", Keycode::UNDO, "keycode.undo"},
    {"CUT", Keycode::CUT, "keycode.cut"},
    {"COPY", Keycode::COPY, "keycode.copy"},
    {"PASTE", Keycode::PASTE, "keycode.paste"},
    {"FIND", Keycode::FIND, "keycode.find"},
    {"MUTE", Keycode::MUTE, "keycode.mute"},
    {"VOLUMEUP", Keycode::VOLUMEUP, "keycode.volumeup"},
    {"VOLUMEDOWN", Keycode::VOLUMEDOWN, "keycode.volumedown"},
    {"KP_COMMA", Keycode::KP_COMMA, "keycode.kp_comma"},
    {"KP_EQUALSAS400", Keycode::KP_EQUALSAS400, "keycode.kp_equalsas400"},
    {"ALTERASE", Keycode::ALTERASE, "keycode.alterase"},
    {"SYSREQ", Keycode::SYSREQ, "keycode.sysreq"},
    {"CANCEL", Keycode::CANCEL, "keycode.cancel"},
    {"CLEAR", Keycode::CLEAR, "keycode.clear"},
    {"PRIOR", Keycode::PRIOR, "keycode.prior"},
    {"RETURN2", Keycode::RETURN2, "keycode.return2"},
    {"SEPARATOR", Keycode::SEPARATOR, "keycode.separator"},
    {"OUT", Keycode::OUT, "keycode.out"},
    {"OPER", Keycode::OPER, "keycode.oper"},
    {"CLEARAGAIN", Keycode::CLEARAGAIN, "keycode.clearagain"},
    {"CRSEL", Keycode::CRSEL, "keycode.crsel"},
    {"EXSEL", Keycode::EXSEL, "keycode.exsel"},
    {"KP_00", Keycode::KP_00, "keycode.kp_00"},
    {"KP_000", Keycode::KP_000, "keycode.kp_000"},
    {"THOUSANDSSEPARATOR", Keycode::THOUSANDSSEPARATOR, "keycode.thousandsseparator"},
    {"DECIMALSEPARATOR", Keycode::DECIMALSEPARATOR, "keycode.decimalseparator"},
    {"CURRENCYUNIT", Keycode::CURRENCYUNIT, "keycode.currencyunit"},
    {"CURRENCYSUBUNIT", Keycode::CURRENCYSUBUNIT, "keycode.currencysubunit"},
    {"KP_LEFTPAREN", Keycode::KP_LEFTPAREN, "keycode.kp_leftparen"},
    {"KP_RIGHTPAREN", Keycode::KP_RIGHTPAREN, "keycode.kp_rightparen"},
    {"KP_LEFTBRACE", Keycode::KP_LEFTBRACE, "keycode.kp_leftbrace"},
    {"KP_RIGHTBRACE", Keycode::KP_RIGHTBRACE, "keycode.kp_rightbrace"},
    {"KP_TAB", Keycode::KP_TAB, "keycode.kp_tab"},
    {"KP_BACKSPACE", Keycode::KP_BACKSPACE, "keycode.kp_backspace"},
    {"KP_A", Keycode::KP_A, "keycode.kp_a"},
    {"KP_B", Keycode::KP_B, "keycode.kp_b"},
    {"KP_C", Keycode::KP_C, "keycode.kp_c"},
    {"KP_D", Keycode::KP_D, "keycode.kp_d"},
    {"KP_E", Keycode::KP_E, "keycode.kp_e"},
    {"KP_F", Keycode::KP_F, "keycode.kp_f"},
    {"KP_XOR", Keycode::KP_XOR, "keycode.kp_xor"},
    {"KP_POWER", Keycode::KP_POWER, "keycode.kp_power"},
    {"KP_PERCENT", Keycode::KP_PERCENT, "keycode.kp_percent"},
    {"KP_LESS", Keycode::KP_LESS, "keycode.kp_less"},
    {"KP_GREATER", Keycode::KP_GREATER, "keycode.kp_greater"},
    {"KP_AMPERSAND", Keycode::KP_AMPERSAND, "keycode.kp_ampersand"},
    {"KP_DBLAMPERSAND", Keycode::KP_DBLAMPERSAND, "keycode.kp_dblampersand"},
    {"KP_VERTICALBAR", Keycode::KP_VERTICALBAR, "keycode.kp_verticalbar"},
    {"KP_DBLVERTICALBAR", Keycode::KP_DBLVERTICALBAR, "keycode.kp_dblverticalbar"},
    {"KP_COLON", Keycode::KP_COLON, "keycode.kp_colon"},
    {"KP_HASH", Keycode::KP_HASH, "keycode.kp_hash"},
    {"KP_SPACE", Keycode::KP_SPACE, "keycode.kp_space"},
    {"KP_AT", Keycode::KP_AT, "keycode.kp_at"},
    {"KP_EXCLAM", Keycode::KP_EXCLAM, "keycode.kp_exclam"},
    {"KP_MEMSTORE", Keycode::KP_MEMSTORE, "keycode.kp_memstore"},
    {"KP_MEMRECALL", Keycode::KP_MEMRECALL, "keycode.kp_memrecall"},
    {"KP_MEMCLEAR", Keycode::KP_MEMCLEAR, "keycode.kp_memclear"},
    {"KP_MEMADD", Keycode::KP_MEMADD, "keycode.kp_memadd"},
    {"KP_MEMSUBTRACT", Keycode::KP_MEMSUBTRACT, "keycode.kp_memsubtract"},
    {"KP_MEMMULTIPLY", Keycode::KP_MEMMULTIPLY, "keycode.kp_memmultiply"},
    {"KP_MEMDIVIDE", Keycode::KP_MEMDIVIDE, "keycode.kp_memdivide"},
    {"KP_PLUSMINUS", Keycode::KP_PLUSMINUS, "keycode.kp_plusminus"},
    {"KP_CLEAR", Keycode::KP_CLEAR, "keycode.kp_clear"},
    {"KP_CLEARENTRY", Keycode::KP_CLEARENTRY, "keycode.kp_clearentry"},
    {"KP_BINARY", Keycode::KP_BINARY, "keycode.kp_binary"},
    {"KP_OCTAL", Keycode::KP_OCTAL, "keycode.kp_octal"},
    {"KP_DECIMAL", Keycode::KP_DECIMAL, "keycode.kp_decimal"},
    {"KP_HEXADECIMAL", Keycode::KP_HEXADECIMAL, "keycode.kp_hexadecimal"},
    {"LCTRL", Keycode::LCTRL, "keycode.lctrl"},
    {"LSHIFT", Keycode::LSHIFT, "keycode.lshift"},
    {"LALT", Keycode::LALT, "keycode.lalt"},
    {"LGUI", Keycode::LGUI, "keycode.lgui"},
    {"RCTRL", Keycode::RCTRL, "keycode.rctrl"},
    {"RSHIFT", Keycode::RSHIFT, "keycode.rshift"},
    {"RALT", Keycode::RALT, "keycode.ralt"},
    {"RGUI", Keycode::RGUI, "keycode.rgui"},
    {"MODE", Keycode::MODE, "keycode.mode"},
    {"AUDIONEXT", Keycode::AUDIONEXT, "keycode.audionext"},
    {"AUDIOPREV", Keycode::AUDIOPREV, "keycode.audioprev"},
    {"AUDIOSTOP", Keycode::AUDIOSTOP, "keycode.audiostop"},
    {"AUDIOPLAY", Keycode::AUDIOPLAY, "keycode.audioplay"},
    {"AUDIOMUTE", Keycode::AUDIOMUTE, "keycode.audiomute"},
    {"MEDIASELECT", Keycode::MEDIASELECT, "keycode.mediaselect"},
    {"WWW", Keycode::WWW, "keycode.www"},
    {"MAIL", Keycode::MAIL, "keycode.mail"},
    {"CALCULATOR", Keycode::CALCULATOR, "keycode.calculator"},
    {"COMPUTER", Keycode::COMPUTER, "keycode.computer"},
    {"AC_SEARCH", Keycode::AC_SEARCH, "keycode.ac_search"},
    {"AC_HOME", Keycode::AC_HOME, "keycode.ac_home"},
    {"AC_BACK", Keycode::AC_BACK, "keycode.ac_back"},
    {"AC_FORWARD", Keycode::AC_FORWARD, "keycode.ac_forward"},
    {"AC_STOP", Keycode::AC_STOP, "keycode.ac_stop"},
    {"AC_REFRESH", Keycode::AC_REFRESH, "keycode.ac_refresh"},
    {"AC_BOOKMARKS", Keycode::AC_BOOKMARKS, "keycode.ac_bookmarks"},
    {"BRIGHTNESSDOWN", Keycode::BRIGHTNESSDOWN, "keycode.brightnessdown"},
    {"BRIGHTNESSUP", Keycode::BRIGHTNESSUP, "keycode.brightnessup"},
    {"DISPLAYSWITCH", Keycode::DISPLAYSWITCH, "keycode.displayswitch"},
    {"KBDILLUMTOGGLE", Keycode::KBDILLUMTOGGLE, "keycode.kbdillumtoggle"},
    {"KBDILLUMDOWN", Keycode::KBDILLUMDOWN, "keycode.kbdillumdown"},
    {"KBDILLUMUP", Keycode::KBDILLUMUP, "keycode.kbdillumup"},
    {"EJECT", Keycode::EJECT, "keycode.eject"},
    {"SLEEP", Keycode::SLEEP, "keycode.sleep"},
    {"MOUSE4", Keycode::MOUSE4, "keycode.mouse4"},
    {"MOUSE5", Keycode::MOUSE5, "keycode.mouse5"},
}};

constexpr std::array<std::pair<std::string_view, Modifiers>, 5> kModifierNames{{
    {"None", Modifiers::None},
    {"Shift", Modifiers::Shift},
    {"Alt", Modifiers::Alt},
    {"Ctrl", Modifiers::Ctrl},
    {"Meta", Modifiers::Meta},
}};

constexpr std::array<std::pair<Modifiers, const char*>, 5> kModifierFluentKeys{{
    {Modifiers::None, "keycode-modifier.none"},
    {Modifiers::Shift, "keycode-modifier.shift"},
    {Modifiers::Alt, "keycode-modifier.alt"},
    {Modifiers::Ctrl, "keycode-modifier.ctrl"},
    {Modifiers::Meta, "keycode-modifier.meta"},
}};

bool IEquals(std::string_view sv_a, std::string_view sv_b) {
  if (sv_a.size() != sv_b.size())
    return false;
  const auto lower = [](char ch_c) {
    return ch_c >= 'A' && ch_c <= 'Z' ? static_cast<char>(ch_c - 'A' + 'a') : ch_c;
  };
  for (std::size_t int8_i = 0; int8_i < sv_a.size(); int8_i++)
    if (lower(sv_a[int8_i]) != lower(sv_b[int8_i]))
      return false;
  return true;
}

const KeycodeNameEntry* FindKeycode(std::string_view sv_name) {
  for (const KeycodeNameEntry& entry : kKeycodeNames)
    if (IEquals(sv_name, entry.str_name))
      return &entry;
  return nullptr;
}

std::string GetMessageOrDefault(std::string_view str_key) {
  return fluent::FluentProvider::Instance().GetMessage(str_key);
}

}

bool Hotkey::TryParse(std::string_view sv_s, Hotkey& result_out) {
  result_out = Invalid();

  const std::size_t int8_space = sv_s.find(' ');
  const std::string_view sv_first =
      int8_space == std::string_view::npos ? sv_s : sv_s.substr(0, int8_space);
  const std::string_view sv_second =
      int8_space == std::string_view::npos ? std::string_view{} : sv_s.substr(int8_space + 1);

  const KeycodeNameEntry* ptr_entry = FindKeycode(sv_first);
  if (ptr_entry == nullptr)
    return false;

  Modifiers mods_kind = Modifiers::None;
  if (int8_space != std::string_view::npos && !modifiers_exts::TryParseModifiers(sv_second, mods_kind))
    return false;

  result_out = Hotkey{ptr_entry->key_kind, mods_kind};
  return true;
}

std::string Hotkey::ToString() const {

  std::string str_modifiers;
  for (const auto& [str_entry_name, mods_entry] : kModifierNames)
    if (mods_entry == kind_modifiers) {
      str_modifiers = str_entry_name;
      break;
    }
  if (str_modifiers.empty())
    str_modifiers = std::to_string(static_cast<std::int32_t>(kind_modifiers));
  return keycode_exts::KeycodeToString(key_kind) + " " + str_modifiers;
}

std::string Hotkey::DisplayString() const {
  std::string str_ret = keycode_exts::DisplayString(key_kind);

  if (HasModifier(kind_modifiers, Modifiers::Shift))
    str_ret = modifiers_exts::DisplayString(Modifiers::Shift) + " + " + str_ret;

  if (HasModifier(kind_modifiers, Modifiers::Alt))
    str_ret = modifiers_exts::DisplayString(Modifiers::Alt) + " + " + str_ret;

  if (HasModifier(kind_modifiers, Modifiers::Ctrl))
    str_ret = modifiers_exts::DisplayString(Modifiers::Ctrl) + " + " + str_ret;

  if (HasModifier(kind_modifiers, Modifiers::Meta))
    str_ret = modifiers_exts::DisplayString(Modifiers::Meta) + " + " + str_ret;

  return str_ret;
}

bool keycode_exts::TryParseKeycode(std::string_view sv_name, Keycode& key_out) {
  key_out = Keycode::UNKNOWN;
  const KeycodeNameEntry* ptr_entry = FindKeycode(sv_name);
  if (ptr_entry == nullptr)
    return false;
  key_out = ptr_entry->key_kind;
  return true;
}

std::string keycode_exts::KeycodeToString(Keycode key_kind) {
  for (const KeycodeNameEntry& entry : kKeycodeNames)
    if (entry.key_kind == key_kind)
      return entry.str_name;
  return std::to_string(static_cast<std::int32_t>(key_kind));
}

std::string keycode_exts::DisplayString(Keycode key_kind) {
  for (const KeycodeNameEntry& entry : kKeycodeNames)
    if (entry.key_kind == key_kind) {
      if (entry.str_fluent == nullptr)
        return std::string{entry.str_name};
      return GetMessageOrDefault(entry.str_fluent);
    }
  return std::to_string(static_cast<std::int32_t>(key_kind));
}

bool modifiers_exts::TryParseModifiers(std::string_view sv_name, Modifiers& mods_out) {

  const auto parse_single = [](std::string_view sv_part, Modifiers& mods_part) {
    for (const auto& [str_entry_name, mods_entry] : kModifierNames)
      if (IEquals(sv_part, str_entry_name)) {
        mods_part = mods_entry;
        return true;
      }
    return false;
  };

  if (sv_name.find(',') == std::string_view::npos)
    return parse_single(sv_name, mods_out);

  Modifiers mods_accum = Modifiers::None;
  std::size_t int8_start = 0;
  while (true) {
    const std::size_t int8_comma = sv_name.find(',', int8_start);
    const std::string_view sv_part =
        int8_comma == std::string_view::npos
            ? sv_name.substr(int8_start)
            : sv_name.substr(int8_start, int8_comma - int8_start);
    const std::size_t int8_begin = sv_part.find_first_not_of(' ');
    const std::size_t int8_end = sv_part.find_last_not_of(' ');
    if (int8_begin == std::string_view::npos)
      return false;
    Modifiers mods_part;
    if (!parse_single(sv_part.substr(int8_begin, int8_end - int8_begin + 1), mods_part))
      return false;
    mods_accum = mods_accum | mods_part;
    if (int8_comma == std::string_view::npos)
      break;
    int8_start = int8_comma + 1;
  }

  mods_out = mods_accum;
  return true;
}

std::string modifiers_exts::DisplayString(Modifiers mods_kind) {
  for (const auto& [mods_entry, str_entry_fluent] : kModifierFluentKeys)
    if (mods_entry == mods_kind)
      return GetMessageOrDefault(str_entry_fluent);
  return std::to_string(static_cast<std::int32_t>(mods_kind));
}

HotkeyReference HotkeyReferenceFromNameOrCode(std::string_view str_name) {

  const std::string str_name_owned{str_name};
  return HotkeyReference([str_name_owned]() {
    Hotkey hk_key;
    if (!Hotkey::TryParse(str_name_owned, hk_key))
      hk_key = Hotkey::Invalid();
    return hk_key;
  });
}

HotkeyDefinition::HotkeyDefinition(std::string str_name_in, const yaml::MiniYaml& yaml_node)
    : str_name(std::move(str_name_in)) {
  const auto value_text = [](const yaml::MiniYaml& yaml_value) -> std::string_view {
    return yaml_value.Value != nullptr ? std::string_view{*yaml_value.Value} : std::string_view{};
  };
  const auto value_owned = [](const yaml::MiniYaml& yaml_value) -> std::string {
    return yaml_value.Value != nullptr ? *yaml_value.Value : std::string{};
  };
  const auto throw_invalid = [](std::string_view sv_value) -> yaml::YamlException {
    return yaml::YamlException(std::format(
        "FieldLoader: Cannot parse `{}` into field `value` of type `OpenRA.Hotkey`", sv_value));
  };

  if (yaml_node.Value != nullptr && !yaml_node.Value->empty()) {
    if (!Hotkey::TryParse(*yaml_node.Value, hk_default))
      throw throw_invalid(*yaml_node.Value);
  }

  if (const yaml::MiniYamlNode* node_description = yaml_node.NodeWithKeyOrDefault("Description"))
    str_description = value_owned(node_description->Value);

  if (const yaml::MiniYamlNode* node_types = yaml_node.NodeWithKeyOrDefault("Types")) {
    const std::string_view sv_types = value_text(node_types->Value);
    if (!sv_types.empty())
      vec_types = meta::GetStringArrayValue("Types", sv_types);
  }

  if (const yaml::MiniYamlNode* node_contexts = yaml_node.NodeWithKeyOrDefault("Contexts")) {
    const std::string_view sv_contexts = value_text(node_contexts->Value);
    if (!sv_contexts.empty())
      for (const std::string& str_context : meta::GetStringArrayValue("Contexts", sv_contexts))
        vec_contexts.push_back(std::string{kContextFluentPrefix} + "." + str_context);
  }

  if (const yaml::MiniYamlNode* node_platform = yaml_node.NodeWithKeyOrDefault("Platform")) {
    if (const yaml::MiniYamlNode* node_override =
            node_platform->Value.NodeWithKeyOrDefault("Windows")) {
      const std::string_view sv_override = value_text(node_override->Value);
      Hotkey hk_override;
      if (!Hotkey::TryParse(sv_override, hk_override))
        throw throw_invalid(sv_override);
      hk_default = hk_override;
    }
  }

  if (const yaml::MiniYamlNode* node_readonly = yaml_node.NodeWithKeyOrDefault("Readonly")) {
    const std::string_view sv_readonly = value_text(node_readonly->Value);
    if (!sv_readonly.empty())
      b_readonly = meta::GetBoolValue("Readonly", sv_readonly);
  }
}

HotkeyManager::HotkeyManager(const OpenFn& fn_open,
                             const std::vector<std::string>& vec_hotkey_files,
                             std::span<const SettingsOverride> vec_settings_nodes) {
  yaml::StringPool pool_yaml;
  for (const std::string& str_file : vec_hotkey_files) {
    const std::vector<char> vec_bytes = fn_open(str_file);
    for (const yaml::MiniYamlNode& node_kd : yaml::MiniYaml::FromStream(
             std::string_view{vec_bytes.data(), vec_bytes.size()}, str_file, true, pool_yaml)) {
      const std::string str_key = node_kd.Key != nullptr ? *node_kd.Key : std::string{};
      HotkeyDefinition definition{str_key, node_kd.Value};
      vec_definitions_.emplace_back(str_key, std::move(definition));
      vec_keys_.emplace_back(str_key, vec_definitions_.back().second.hk_default);
    }
  }

  for (const SettingsOverride& override_setting : vec_settings_nodes) {
    for (auto& [str_entry_key, definition_entry] : vec_definitions_) {
      if (str_entry_key == override_setting.str_name && !definition_entry.b_readonly) {
        for (auto& [str_key_key, hk_key] : vec_keys_) {
          if (str_key_key == override_setting.str_name) {
            Hotkey hk_parsed;
            if (!Hotkey::TryParse(override_setting.str_value, hk_parsed))
              throw yaml::YamlException(std::format(
                  "FieldLoader: Cannot parse `{}` into field `{}` of type `OpenRA.Hotkey`",
                  override_setting.str_value, override_setting.str_name));
            hk_key = hk_parsed;
            break;
          }
        }
        break;
      }
    }
  }

  for (auto& [str_entry_key, definition_entry] : vec_definitions_)
    definition_entry.b_has_duplicates =
        GetFirstDuplicate(definition_entry, Get(str_entry_key).GetValue()) != nullptr;
}

HotkeyReference HotkeyManager::Get(std::string_view str_name) const {
  for (const auto& [str_entry_key, hk_entry] : vec_keys_)
    if (str_entry_key == str_name) {
      const std::string str_owned{str_entry_key};
      return HotkeyReference([&vec_keys = vec_keys_, str_owned]() {
        for (const auto& [str_key_inner, hk_inner] : vec_keys)
          if (str_key_inner == str_owned)
            return hk_inner;
        return Hotkey::Invalid();
      });
    }

  return HotkeyReferenceFromNameOrCode(str_name);
}

void HotkeyManager::Set(std::string_view str_name, Hotkey hk_value) {
  HotkeyDefinition* ptr_definition = nullptr;
  for (auto& [str_entry_key, definition_entry] : vec_definitions_)
    if (str_entry_key == str_name) {
      ptr_definition = &definition_entry;
      break;
    }
  if (ptr_definition == nullptr)
    return;
  if (ptr_definition->b_readonly)
    return;

  for (auto& [str_entry_key, hk_entry] : vec_keys_)
    if (str_entry_key == str_name)
      hk_entry = hk_value;

  if (fn_persist_sink_) {
    std::optional<std::string> str_persist;
    if (!(hk_value == ptr_definition->hk_default))
      str_persist = hk_value.ToString();
    fn_persist_sink_(std::string{str_name}, std::move(str_persist));
  }

  const bool b_had_duplicates = ptr_definition->b_has_duplicates;
  ptr_definition->b_has_duplicates =
      GetFirstDuplicate(*ptr_definition, Get(str_name).GetValue()) != nullptr;

  if (b_had_duplicates || ptr_definition->b_has_duplicates)
    for (auto& [str_entry_key, definition_entry] : vec_definitions_) {
      if (&definition_entry == ptr_definition)
        continue;
      definition_entry.b_has_duplicates =
          GetFirstDuplicate(definition_entry, Get(str_entry_key).GetValue()) != nullptr;
    }
}

void HotkeyManager::SetPersistSink(
    std::function<void(const std::string&, std::optional<std::string>)> fn_sink) {
  fn_persist_sink_ = std::move(fn_sink);
}

void HotkeyManager::SetSaveSink(std::function<void()> fn_sink) {
  fn_save_sink_ = std::move(fn_sink);
}

void HotkeyManager::Save() {
  fn_save_sink_();
}

const HotkeyDefinition* HotkeyManager::GetFirstDuplicate(const HotkeyDefinition& definition,
                                                         Hotkey hk_value) const {
  for (const auto& [str_entry_key, hk_entry] : vec_keys_) {
    if (str_entry_key == definition.str_name)
      continue;

    if (hk_entry == hk_value) {
      for (const auto& [str_def_key, definition_entry] : vec_definitions_) {
        if (str_def_key != str_entry_key)
          continue;
        bool b_overlaps = false;
        for (const std::string& str_context : definition_entry.vec_contexts) {
          for (const std::string& str_other : definition.vec_contexts)
            if (str_context == str_other) {
              b_overlaps = true;
              break;
            }
          if (b_overlaps)
            break;
        }
        if (b_overlaps)
          return &definition_entry;
      }
    }
  }

  return nullptr;
}

}
