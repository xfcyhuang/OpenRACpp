// UPSTREAM: OpenRA.Game/MiniYaml.cs @b6fc03f L295,L299(.Trim() 调用点的 BCL 语义支撑)
// 复刻 .NET 的 char.IsWhiteSpace / MemoryExtensions.Trim:MiniYaml/FieldLoader 的
// Replicates .NET char.IsWhiteSpace / MemoryExtensions.Trim: the key/value trimming of MiniYaml/FieldLoader
// 键值裁剪依赖该语义(Unicode 空白集含 NBSP/U+1680/U+2000-200A/U+3000 等),
// depends on this semantics (the Unicode whitespace set includes NBSP/U+1680/U+2000-200A/U+3000 etc.),
// 字节级处理会在多字节空白上与 C# 产生偏差,故按 UTF-8 码点判定。
// byte-level handling would diverge from C# on multi-byte whitespace, so the check is done per UTF-8 code point.
// 无效 UTF-8 序列按"每字节一个 U+FFFD"处理(mods 数据为合法 UTF-8,仅为兜底;
// Invalid UTF-8 sequences are handled as "one U+FFFD per byte" (mods data is valid UTF-8, this is only a fallback;
// C# StreamReader 的替换策略在连续无效字节上可能吞多字节,该角落记为已知偏离)。
// C# StreamReader's replacement strategy may swallow multiple bytes on consecutive invalid bytes; that corner is recorded as a known deviation).
#pragma once
import std;

namespace ora {

/// .NET char.IsWhiteSpace 的码点全集(均为 BMP,单 UTF-16 单元,故码点级判定等价):
/// Full code point set of .NET char.IsWhiteSpace (all BMP, single UTF-16 unit, so code-point-level checking is equivalent):
/// 0009-000D、0020、0085、00A0、1680、2000-200A、2028、2029、202F、205F、3000
constexpr bool IsNetWhiteSpace(char32_t cp) {
  return (cp >= 0x0009 && cp <= 0x000D) || cp == 0x0020 || cp == 0x0085 || cp == 0x00A0 ||
         cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
         cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/// 解码 text[i] 起的一个 UTF-8 码点并推进 int8_i;无效序列按单字节 U+FFFD。
/// Decode one UTF-8 code point starting at text[i] and advance int8_i; invalid sequences yield one single-byte U+FFFD.
constexpr char32_t DecodeUtf8(std::string_view text, std::size_t& int8_i) {
  const unsigned char b0{static_cast<unsigned char>(text[int8_i])};
  const std::size_t int8_start{int8_i};
  if (b0 < 0x80) {
    int8_i += 1;
    return b0;
  }
  std::size_t int8_len{};
  char32_t cp{};
  if ((b0 & 0xE0) == 0xC0) {
    int8_len = 2;
    cp = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    int8_len = 3;
    cp = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    int8_len = 4;
    cp = b0 & 0x07;
  } else {
    int8_i += 1;
    return 0xFFFD;
  }
  if (int8_start + int8_len > text.size()) {
    int8_i += 1;
    return 0xFFFD;
  }
  for (std::size_t k{1}; k < int8_len; k++) {
    const unsigned char bk{static_cast<unsigned char>(text[int8_start + k])};
    if ((bk & 0xC0) != 0x80) {
      int8_i += 1;
      return 0xFFFD;
    }
    cp = (cp << 6) | (bk & 0x3F);
  }
  // 超范围/过长编码同样视为无效 | Out-of-range/overlong encodings are also treated as invalid
  if (cp > 0x10FFFF || (int8_len == 2 && cp < 0x80) || (int8_len == 3 && cp < 0x800) ||
      (int8_len == 4 && cp < 0x10000)) {
    int8_i += 1;
    return 0xFFFD;
  }
  int8_i += int8_len;
  return cp;
}

/// MemoryExtensions.Trim():两端剥离 Unicode 空白(码点级),返回原缓冲的子视图。
/// MemoryExtensions.Trim(): strips Unicode whitespace (code-point level) from both ends, returning a sub-view of the original buffer.
constexpr std::string_view TrimNetWhiteSpace(std::string_view text) {
  std::size_t int8_begin{0};
  while (int8_begin < text.size()) {
    std::size_t int8_next{int8_begin};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_next)))
      break;
    int8_begin = int8_next;
  }
  std::size_t int8_end{text.size()};
  while (int8_end > int8_begin) {
    // 从尾部逐字节回退找最后一个码点的起点 | Step back byte-by-byte from the tail to find the start of the last code point
    std::size_t int8_p{int8_end - 1};
    while (int8_p > int8_begin && (static_cast<unsigned char>(text[int8_p]) & 0xC0) == 0x80)
      int8_p--;
    const std::size_t int8_cpStart{int8_p};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_p)))
      break;
    int8_end = int8_cpStart;
  }
  return text.substr(int8_begin, int8_end - int8_begin);
}

/// 仅剥尾端(MiniYamlExts.WriteToFile 的 x.TrimEnd():C# 无参 TrimEnd 只剥尾部,
/// Strip the tail only (x.TrimEnd() of MiniYamlExts.WriteToFile: C# parameterless TrimEnd strips only the tail;
/// 行首 '\t' 缩进必须保留)
/// leading '\t' indentation must be preserved)
constexpr std::string_view TrimEndNetWhiteSpace(std::string_view text) {
  std::size_t int8_end{text.size()};
  while (int8_end > 0) {
    std::size_t int8_p{int8_end - 1};
    while (int8_p > 0 && (static_cast<unsigned char>(text[int8_p]) & 0xC0) == 0x80)
      int8_p--;
    const std::size_t int8_cpStart{int8_p};
    if (!IsNetWhiteSpace(DecodeUtf8(text, int8_p)))
      break;
    int8_end = int8_cpStart;
  }
  return text.substr(0, int8_end);
}

}  // namespace ora
