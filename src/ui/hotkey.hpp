#pragma once
import std;

#include "platform/keycode.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::ui {

struct Hotkey {
  Keycode key_kind = Keycode::UNKNOWN;
  Modifiers kind_modifiers = Modifiers::None;

  static Hotkey Invalid() { return Hotkey{}; }

  bool IsValid() const { return key_kind != Keycode::UNKNOWN; }

  static bool TryParse(std::string_view sv_s, Hotkey& result_out);

  static Hotkey FromKeyInput(const KeyInput& ki_input) {
    return Hotkey{ki_input.Key, ki_input.ModifierFlags};
  }

  friend bool operator==(const Hotkey& hk_a, const Hotkey& hk_b) {

    if (hk_a.key_kind == Keycode::UNKNOWN)
      return false;
    return hk_a.key_kind == hk_b.key_kind && hk_a.kind_modifiers == hk_b.kind_modifiers;
  }
  friend bool operator!=(const Hotkey& hk_a, const Hotkey& hk_b) { return !(hk_a == hk_b); }

  std::string ToString() const;

  std::string DisplayString() const;
};

namespace keycode_exts {

bool TryParseKeycode(std::string_view sv_name, Keycode& key_out);

std::string KeycodeToString(Keycode key_kind);

std::string DisplayString(Keycode key_kind);
}

namespace modifiers_exts {
bool TryParseModifiers(std::string_view sv_name, Modifiers& mods_out);

std::string DisplayString(Modifiers mods_kind);
}

class HotkeyReference {
 public:
  HotkeyReference() = default;

  explicit HotkeyReference(std::function<Hotkey()> fn_get_value)
      : fn_get_value_(std::move(fn_get_value)) {}

  Hotkey GetValue() const {
    return fn_get_value_ ? fn_get_value_() : Hotkey::Invalid();
  }

  bool IsActivatedBy(const KeyInput& e_input) const {
    const Hotkey hk_current = GetValue();
    return hk_current.key_kind == e_input.Key &&
           hk_current.kind_modifiers == e_input.ModifierFlags;
  }

 private:
  std::function<Hotkey()> fn_get_value_;
};

class HotkeyDefinition {
 public:
  static constexpr std::string_view kContextFluentPrefix = "hotkey-context";

  std::string str_name;
  Hotkey hk_default = Hotkey::Invalid();
  std::string str_description;
  std::vector<std::string> vec_types;
  std::vector<std::string> vec_contexts;
  bool b_readonly = false;
  bool b_has_duplicates = false;

  HotkeyDefinition(std::string str_name_in, const yaml::MiniYaml& yaml_node);
};

class HotkeyManager {
 public:
  using OpenFn = std::function<std::vector<char>(const std::string&)>;

  struct SettingsOverride {
    std::string str_name;
    std::string str_value;
  };

  HotkeyManager(const OpenFn& fn_open, const std::vector<std::string>& vec_hotkey_files,
                std::span<const SettingsOverride> vec_settings_nodes);

  const std::vector<std::pair<std::string, HotkeyDefinition>>& Definitions() const {
    return vec_definitions_;
  }

  HotkeyReference Get(std::string_view str_name) const;

  void Set(std::string_view str_name, Hotkey hk_value);

  void SetPersistSink(
      std::function<void(const std::string& str_name,
                         std::optional<std::string> str_value)> fn_sink);
  void SetSaveSink(std::function<void()> fn_sink);
  void Save();

  const HotkeyDefinition* GetFirstDuplicate(const HotkeyDefinition& definition,
                                            Hotkey hk_value) const;

 private:
  std::vector<std::pair<std::string, HotkeyDefinition>> vec_definitions_;
  std::vector<std::pair<std::string, Hotkey>> vec_keys_;
  std::function<void(const std::string&, std::optional<std::string>)>
      fn_persist_sink_;
  std::function<void()> fn_save_sink_ = [] {};
};

HotkeyReference HotkeyReferenceFromNameOrCode(std::string_view str_name);

}
