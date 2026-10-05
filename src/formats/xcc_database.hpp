// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XccLocalDatabase.cs @7d57605 L19-69
//          OpenRA.Mods.Cnc/FileFormats/XccGlobalDatabase.cs @7d57605 L18-60
// Xcc 数据库两件:local(mix 内嵌的文件名表,48 字节头 + count + NUL 串)
// 与 global(重复块 [int32 count + (name\0 comment\0)*] 至流尾)。解析逐
// 语义;Local 另带 Data() 写出器(上游同),供 mix 夹具构造与 oracle 对拍。
// The two Xcc databases: local (the filename table embedded in a mix — a
// 48-byte header + count + NUL strings) and global (repeated blocks of
// [int32 count + (name\0 comment\0)*] until end of stream). Parsing is
// statement-by-statement; Local also carries the Data() writer (as
// upstream), serving mix-fixture construction and the oracle differential.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// XccLocalDatabase(XccLocalDatabase.cs L19-69)。
class XccLocalDatabase {
 public:
  /// 流构造(L22-39):Seek(48) → int32 count → count 个 NUL 结尾字符串。
  /// The stream ctor (L22-39): Seek(48) → int32 count → count NUL-terminated
  /// strings.
  explicit XccLocalDatabase(std::span<const std::byte> vec_source);

  /// Data()(L46-67):48 字节头 + count + 条目(ASCII + NUL)。Size 字段 =
  /// 字符数总和 + 条目数 + 52(上游公式)。
  /// Data() (L46-67): the 48-byte header + count + entries (ASCII + NUL).
  /// The Size field = the total char count + the entry count + 52 (the
  /// upstream formula).
  static std::vector<std::byte> MakeData(std::span<const std::string> vec_entries);

  const std::vector<std::string>& Entries() const { return vec_entries_; }

 private:
  std::vector<std::string> vec_entries_;
};

/// XccGlobalDatabase(XccGlobalDatabase.cs L18-60):global mix database.dat
/// 的解析器("global mix database.dat" 内容)。
/// XccGlobalDatabase (XccGlobalDatabase.cs L18-60): the parser for the
/// "global mix database.dat" contents.
class XccGlobalDatabase {
 public:
  explicit XccGlobalDatabase(std::span<const std::byte> vec_source);

  const std::vector<std::string>& Entries() const { return vec_entries_; }

 private:
  std::vector<std::string> vec_entries_;
};

}  // namespace ora::fmt
