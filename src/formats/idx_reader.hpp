// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/IdxReader.cs @b6fc03f(全文)+
// IdxEntry.cs L16-45(全文)。RA2 声音索引:16 字节名 + "GABA" 魔数头 +
// u32==2 校验 + SoundCount 条目(名 16B/偏移/长度/采样率/标志/块大小)。
// IdxEntry 怪癖照抄:NUL 位 pos==0 不截断(16 个 NUL 原样入名);pos==-1
// (无 NUL 的 16 字符名)上游 name[..-1] 抛 ArgumentOutOfRangeException
// (双行默认消息,.NET 10 实测),此处等价抛。
// [UPSTREAM continued] IdxReader.cs in full plus IdxEntry.cs L16-45. The
// RA2 sound index: 16-byte name + the "GABA" magic header + a u32==2 check +
// SoundCount entries (name 16B/offset/length/sample rate/flags/chunk size).
// IdxEntry quirks kept: a NUL at pos 0 does not truncate (16 NULs stay in
// the name); pos == -1 (a 16-char name with no NUL) makes upstream's
// name[..-1] throw ArgumentOutOfRangeException (the two-line default
// message, verified against .NET 10), thrown here as the equivalent.
#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

/// IdxEntry.cs L16-45;Filename 含 .wav 后缀(pos==0 怪癖下为 16 个 NUL +
/// ".wav")。| IdxEntry.cs L16-45; Filename carries the .wav suffix (under
/// the pos==0 quirk: 16 NULs + ".wav").
struct IdxEntry {
  std::string str_filename;
  std::uint32_t uint4_offset = 0;
  std::uint32_t uint4_length = 0;
  std::uint32_t uint4_sample_rate = 0;
  std::uint32_t uint4_flags = 0;
  std::uint32_t uint4_chunk_size = 0;

  /// IdxEntry.ToString(L40-43)逐字。
  /// IdxEntry.ToString (L40-43) verbatim.
  [[nodiscard]] std::string ToString() const;
};

/// IdxReader.cs L17-44(全文逐语义)。抛点:魔数/版本失配的
/// InvalidDataException 等价(消息逐字,含实得值)。
/// IdxReader.cs L17-44 (verbatim semantics). Throw points: the
/// InvalidDataException equivalents for the magic/version mismatches
/// (messages verbatim, with the found values).
class IdxReader {
 public:
  explicit IdxReader(std::span<const std::byte> vec_file);

  std::int32_t int4_sound_count = 0;
  std::vector<IdxEntry> vec_entries;
};

}  // namespace ora::fmt
