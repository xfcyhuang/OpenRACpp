// UPSTREAM: OpenRA.Game/Input/Hotkey.cs @1f594e320 L20-101(全文)+
//          HotkeyReference.cs L15-46(全文)+ HotkeyDefinition.cs L20-66(全文)
//          + HotkeyManager.cs L20-116(核心面)+ Keycode.cs L259-511
//          (KeycodeExts.KeycodeFluentKeys/DisplayString)+ IInputHandler.cs
//          L53-77(ModifiersExts)。
// Hotkey 值语义(UNKNOWN 永不相等/TryParse 的 ' ' 双段解析/DisplayString
// 的 Shift+Alt+Ctrl+Meta 前缀序)与 HotkeyReference 的惰性取值/IsActivatedBy
// 逐行照抄;HotkeyManager 的 definitions/keys 双表 + GetFirstDuplicate 的
// Contexts 交集判 + Set 的重算面保真,settings 持久化节点(HotkeySettings)
// 为注入面(缺省空 = 全部默认键)。Enum.TryParse 的名字 ↔ 值经
// Build/keycode_names.txt 生成的 238 项名表(与 keycode.hpp 枚举体同源)。
// The Hotkey value semantics (UNKNOWN never equal / TryParse's two-segment
// ' ' parse / DisplayString's Shift+Alt+Ctrl+Meta prefix order) and
// HotkeyReference's lazy get/IsActivatedBy copied line for line;
// HotkeyManager's definitions/keys dual table + GetFirstDuplicate's
// Contexts-intersection test + Set's recompute kept, the settings
// persistence nodes (HotkeySettings) an injected face (defaulting empty =
// all default keys). Enum.TryParse's name ↔ value goes through the
// 238-entry name table generated from Build/keycode_names.txt (same source
// as the keycode.hpp enum body).
#pragma once
import std;

#include "platform/keycode.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::ui {

/// Hotkey(Hotkey.cs L22-101;readonly struct)。
/// Hotkey (Hotkey.cs L22-101; the readonly struct).
struct Hotkey {
  Keycode key_kind = Keycode::UNKNOWN;
  Modifiers kind_modifiers = Modifiers::None;

  static Hotkey Invalid() { return Hotkey{}; }

  bool IsValid() const { return key_kind != Keycode::UNKNOWN; }

  /// TryParse(L32-48):' ' 分段;段 0 = Keycode(忽略大小写),段 1 = Modifiers。
  /// TryParse (L32-48): ' ' segments; segment 0 = Keycode (case-insensitive),
  /// segment 1 = Modifiers.
  static bool TryParse(std::string_view sv_s, Hotkey& result_out);

  static Hotkey FromKeyInput(const KeyInput& ki_input) {
    return Hotkey{ki_input.Key, ki_input.ModifierFlags};
  }

  friend bool operator==(const Hotkey& hk_a, const Hotkey& hk_b) {
    // Unknown keys are never equal(Hotkey.cs L73-75)
    if (hk_a.key_kind == Keycode::UNKNOWN)
      return false;
    return hk_a.key_kind == hk_b.key_kind && hk_a.kind_modifiers == hk_b.kind_modifiers;
  }
  friend bool operator!=(const Hotkey& hk_a, const Hotkey& hk_b) { return !(hk_a == hk_b); }

  /// ToString(L91):"{Key} {Modifiers:F}"(枚举名;[Flags] 无值名 = 数值)。
  /// ToString (L91): "{Key} {Modifiers:F}" (enum names; an unnamed [Flags]
  /// combination falls to the numeric text).
  std::string ToString() const;

  /// DisplayString(L93-107):KeycodeExts.DisplayString + 修饰键前缀链。
  /// DisplayString (L93-107): KeycodeExts.DisplayString + the modifier
  /// prefix chain.
  std::string DisplayString() const;
};

/// KeycodeExts.KeycodeFluentKeys / DisplayString(Keycode.cs L261-511)。
/// KeycodeExts.KeycodeFluentKeys / DisplayString (Keycode.cs L261-511).
namespace keycode_exts {
/// 名 → 键码(Enum.TryParse 域;忽略大小写)。
/// Name → keycode (the Enum.TryParse domain; case-insensitive).
bool TryParseKeycode(std::string_view sv_name, Keycode& key_out);
/// 键码 → 枚举名(ToString 域;未知值 → 数值串)。
/// Keycode → enum name (the ToString domain; unknown values → numeric).
std::string KeycodeToString(Keycode key_kind);
/// DisplayString(L505-511):fluent 表命中 → FluentProvider.GetMessage,否则
/// 枚举名。FluentProvider 未初始化 = 键回落(D247 族)。
/// DisplayString (L505-511): a fluent-table hit →
/// FluentProvider.GetMessage, else the enum name. An uninitialized
/// FluentProvider falls back to the key (the D247 family).
std::string DisplayString(Keycode key_kind);
}  // namespace keycode_exts

/// ModifiersExts(IInputHandler.cs L60-77)。
/// ModifiersExts (IInputHandler.cs L60-77).
namespace modifiers_exts {
bool TryParseModifiers(std::string_view sv_name, Modifiers& mods_out);
/// DisplayString(IInputHandler.cs L70-77;OSX 的 Meta → Cmd 分支随平台注入
/// 面,Windows 恒走 fluent 表)。
/// DisplayString (IInputHandler.cs L70-77; OSX's Meta → Cmd branch waits
/// for the platform-injection face — Windows always takes the fluent
/// table).
std::string DisplayString(Modifiers mods_kind);
}  // namespace modifiers_exts

/// HotkeyReference(HotkeyReference.cs L19-46;命名/静态键的惰性引用)。
/// HotkeyReference (HotkeyReference.cs L19-46; the lazy reference to a
/// named or static hotkey).
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

/// HotkeyDefinition(HotkeyDefinition.cs L24-64)。
/// HotkeyDefinition (HotkeyDefinition.cs L24-64).
class HotkeyDefinition {
 public:
  static constexpr std::string_view kContextFluentPrefix = "hotkey-context";

  std::string str_name;
  Hotkey hk_default = Hotkey::Invalid();
  std::string str_description;
  std::vector<std::string> vec_types;       // Types(FrozenSet → 插入序)
  std::vector<std::string> vec_contexts;    // Contexts(逐项前缀拼接)
  bool b_readonly = false;
  bool b_has_duplicates = false;

  HotkeyDefinition(std::string str_name_in, const yaml::MiniYaml& yaml_node);
};

/// HotkeyManager(HotkeyManager.cs L18-116;settings 持久化节点为注入面,
/// 缺省空表 = 全默认键)。字典 → 插入序 vector。
/// HotkeyManager (HotkeyManager.cs L18-116; the settings persistence nodes
/// are an injected face, the default empty table = all default keys).
/// Dictionaries become insertion-ordered vectors.
class HotkeyManager {
 public:
  using OpenFn = std::function<std::vector<char>(const std::string&)>;

  /// settings 键覆盖项(HotkeySettings.Yaml 的节点面;name → Hotkey 文本)。
  /// A settings key override (the HotkeySettings.Yaml node face; name →
  /// Hotkey text).
  struct SettingsOverride {
    std::string str_name;
    std::string str_value;
  };

  HotkeyManager(const OpenFn& fn_open, const std::vector<std::string>& vec_hotkey_files,
                std::span<const SettingsOverride> vec_settings_nodes);

  const std::vector<std::pair<std::string, HotkeyDefinition>>& Definitions() const {
    return vec_definitions_;
  }

  /// this[name](L108)→ GetHotkeyReference(L50-66)。
  /// this[name] (L108) → GetHotkeyReference (L50-66).
  HotkeyReference Get(std::string_view str_name) const;

  void Set(std::string_view str_name, Hotkey hk_value);

  void SetPersistSink(
      std::function<void(const std::string& str_name,
                         std::optional<std::string> str_value)> fn_sink);
  void SetSaveSink(std::function<void()> fn_sink);
  void Save();

  /// GetFirstDuplicate(L98-107):值相等且 Contexts 相交的首个其它定义。
  /// GetFirstDuplicate (L98-107): the first other definition with an equal
  /// value and intersecting Contexts.
  const HotkeyDefinition* GetFirstDuplicate(const HotkeyDefinition& definition,
                                            Hotkey hk_value) const;

 private:
  std::vector<std::pair<std::string, HotkeyDefinition>> vec_definitions_;
  std::vector<std::pair<std::string, Hotkey>> vec_keys_;
  std::function<void(const std::string&, std::optional<std::string>)>
      fn_persist_sink_;
  std::function<void()> fn_save_sink_ = [] {};
};

/// ParseHotkeyReference(FieldLoader.cs L222-225)的缺省回落:GetHotkeyReference
/// 尾段(命名键不投递时的硬编码解析)。
/// The default fallback of ParseHotkeyReference (FieldLoader.cs L222-225):
/// GetHotkeyReference's tail (the hardcoded parse when no named key
/// delivers).
HotkeyReference HotkeyReferenceFromNameOrCode(std::string_view str_name);

}  // namespace ora::ui
