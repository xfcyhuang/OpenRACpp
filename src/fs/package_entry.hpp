// UPSTREAM: OpenRA.Mods.Cnc/FileSystem/PackageEntry.cs @b6fc03f L21-117
// mix 目录项 + 双哈希:Classic(RA1/TD 的 rotate-add 32 位)与 CRC32(TS)。
// HashFilename 逐语义:大写化 + 4 字节对齐填充;Classic 填 NUL,CRC32 填
// (length % 4) 字节值并复制尾对齐字节。上游 ToString 的 Names 反查表仅
// 调试用,未移植(不可观测)。已登记偏离:哈希域 = ASCII 文件名(上游
// .NET ToUpperInvariant + ASCII 编码把非 ASCII 折为 '?',且按 UTF-16 字符
// 计长;真实 mix 文件名全为 ASCII,两者等价)。
// The mix directory entry + its two hashes: Classic (RA1/TD's rotate-add
// 32-bit) and CRC32 (TS). HashFilename statement-by-statement: uppercased +
// padded to a 4-byte boundary; Classic pads NUL, CRC32 pads the (length % 4)
// byte value and replicates the last aligned byte. The upstream ToString's
// Names reverse table is debug-only and not ported (unobservable).
// Registered deviation: the hash domain is ASCII filenames (upstream's .NET
// ToUpperInvariant + ASCII encoding folds non-ASCII to '?', and lengths
// count UTF-16 chars; real mix filenames are all ASCII, so the two agree).
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fs {

/// PackageHashType(PackageEntry.cs L21)。
enum class PackageHashType { Classic, CRC32 };

/// PackageEntry(PackageEntry.cs L23-117)。
struct PackageEntry {
  static constexpr int int4_size = 12;  // Size / the serialized size

  std::uint32_t uint4_hash{};
  std::uint32_t uint4_offset{};
  std::uint32_t uint4_length{};

  PackageEntry() = default;
  PackageEntry(std::uint32_t uint4_hash_in, std::uint32_t uint4_offset_in, std::uint32_t uint4_length_in)
      : uint4_hash{uint4_hash_in}, uint4_offset{uint4_offset_in}, uint4_length{uint4_length_in} {}

  /// 流构造(L37-42):三个小端 u32。
  /// The stream ctor (L37-42): three little-endian u32s.
  explicit PackageEntry(fmt::SpanReader& reader)
      : uint4_hash{reader.ReadUInt32()}, uint4_offset{reader.ReadUInt32()},
        uint4_length{reader.ReadUInt32()} {}

  /// HashFilename(L59-106)。
  static std::uint32_t HashFilename(std::string_view str_name, PackageHashType enum_type);
};

}  // namespace ora::fs
