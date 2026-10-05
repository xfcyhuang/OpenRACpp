// UPSTREAM: OpenRA.Game/Graphics/CursorSequence.cs @b6fc03f L14-52(全文逐语义)
// cursors.yaml 的单条光标序列:Name/Src(来自文件级上下文)+ X/Y 热点
// (TryParseInt32Invariant,失败静默保持 0 的上游怪癖)+ Start(必填,缺键 =
// KeyNotFound 等价抛)+ Length 的三态怪癖照抄 —— Length 键存在且值 != "*" =
// 数值;值为 "*" = null(至序列尾);End 键存在且值恰为 "*" 时上游的三元
// 表达式恒取 null(死分支保真);其余 = 1。
// The single cursor sequence of cursors.yaml: Name/Src (from the file-level
// context) + the X/Y hotspot (TryParseInt32Invariant with upstream's
// silently-keep-0-on-failure quirk) + Start (required; a missing key = the
// KeyNotFound-equivalent throw) + Length's three kept states — a Length key
// with a value != "*" parses; the value "*" means null (through the sequence
// end); an End key whose value is exactly "*" takes upstream's always-null
// ternary (the dead branch preserved); everything else = 1.
#pragma once
import std;

#include "core/int2.hpp"
#include "meta/parse.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

namespace {

/// Exts.ParseInt32Invariant(Exts.cs L500-503):int.Parse 失败抛 FormatException
/// (消息依 .NET 版本;取 10 形态)。
/// Exts.ParseInt32Invariant (Exts.cs L500-503): int.Parse failures throw
/// FormatException (the message is .NET-version-dependent; the 10 shape).
std::int32_t ParseInt32(std::string_view sv_value) {
  std::int32_t int4_out{};
  if (!meta::TryParseInt32Invariant(sv_value, int4_out))
    throw std::runtime_error(std::format(
        "System.FormatException: The input string '{}' was not in a correct format.", sv_value));
  return int4_out;
}

}  // namespace


/// CursorSequence(CursorSequence.cs L14-52)。
class CursorSequence {
 public:
  /// (name, cursorSrc, palette, info):info = 序列节点。palette = 文件级
  /// 值节点(fileNode.Value.Value)。
  /// (name, cursorSrc, palette, info): info = the sequence node; palette =
  /// the file-level value node (fileNode.Value.Value).
  CursorSequence(std::string str_name, std::string str_cursor_src, std::string str_palette,
                 const yaml::MiniYaml& yaml_info)
      : str_palette_{std::move(str_palette)}, str_name_{std::move(str_name)},
        str_src_{std::move(str_cursor_src)} {
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_d =
        yaml_info.ToDictionary();
    const auto find_value = [&vec_d](std::string_view sv_key) -> const std::string* {
      for (const auto& [sv_k, yaml_v] : vec_d)
        if (sv_k == sv_key)
          return yaml_v->Value;
      return nullptr;
    };

    if (const std::string* str_value = find_value("X")) {
      std::int32_t int4_x{};
      meta::TryParseInt32Invariant(*str_value, int4_x);  // 失败静默(上游怪癖)| silent on failure (upstream quirk)
      int2_hotspot_ = int2_hotspot_.WithX(int4_x);
    }

    if (const std::string* str_value = find_value("Y")) {
      std::int32_t int4_y{};
      meta::TryParseInt32Invariant(*str_value, int4_y);
      int2_hotspot_ = int2_hotspot_.WithY(int4_y);
    }

    const std::string* str_start = find_value("Start");
    if (str_start == nullptr)
      throw std::runtime_error(
          "System.Collections.Generic.KeyNotFoundException: The given key was not present in the "
          "dictionary.");
    int4_start_ = ParseInt32(*str_start);

    if (const std::string* str_value = find_value("Length")) {
      opt_int4_length_ = *str_value != "*" ? std::optional<std::int32_t>{
                                                 ParseInt32(*str_value)}
                                           : std::nullopt;
    } else if ((str_value = find_value("End")) != nullptr && *str_value == "*") {
      // 上游 L46-47 的三元在此分支恒假 —— 死分支照抄(净效果:End="*" →
      // Length=null)
      // Upstream's L46-47 ternary is always false in this branch — the dead
      // branch kept (net effect: End="*" → Length=null).
      opt_int4_length_ = *str_value != "*"
                             ? std::optional<std::int32_t>{ParseInt32(*str_value) -
                                                           int4_start_}
                             : std::nullopt;
    } else {
      opt_int4_length_ = std::int32_t{1};
    }
  }

  const std::string& Name() const { return str_name_; }
  const std::string& Src() const { return str_src_; }
  std::int32_t Start() const { return int4_start_; }
  const std::optional<std::int32_t>& Length() const { return opt_int4_length_; }
  const std::string& Palette() const { return str_palette_; }
  const int2& Hotspot() const { return int2_hotspot_; }

 private:
  std::string str_name_;
  std::string str_src_;
  std::int32_t int4_start_ = 0;
  std::optional<std::int32_t> opt_int4_length_;
  std::string str_palette_;
  int2 int2_hotspot_{};
};

}  // namespace ora::gfx
