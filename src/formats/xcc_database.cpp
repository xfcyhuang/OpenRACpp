// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XccLocalDatabase.cs @7d57605 L19-69
//          OpenRA.Mods.Cnc/FileFormats/XccGlobalDatabase.cs @7d57605 L18-60
#include "formats/xcc_database.hpp"

namespace ora::fmt {

XccLocalDatabase::XccLocalDatabase(std::span<const std::byte> vec_source) {
  // 跳过头数据(L24-25);reader 已在头部,Seek(48) 显式化。
  // Skip the header data (L24-25); Seek(48) made explicit on the reader.
  SpanReader reader{vec_source};
  reader.Seek(48);

  const auto int4_count = reader.ReadInt32();
  vec_entries_.reserve(static_cast<std::size_t>(int4_count));
  for (auto int4_i = 0; int4_i < int4_count; int4_i++) {
    // ReadChar 逐字符直至 NUL(L30-37)
    // ReadChar until the NUL (L30-37).
    auto str_entry = std::string{};
    while (const auto uint1_c = reader.ReadUInt8())
      str_entry.push_back(static_cast<char>(uint1_c));
    vec_entries_.push_back(std::move(str_entry));
  }
}

std::vector<std::byte> XccLocalDatabase::MakeData(std::span<const std::string> vec_entries) {
  auto vec_data = std::vector<std::byte>{};
  const auto push_u32 = [&vec_data](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_data.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };

  const auto push_ascii = [&vec_data](std::string_view sv) {
    for (const char chr_c : sv)
      vec_data.push_back(static_cast<std::byte>(chr_c));
  };

  push_ascii("XCC by Olaf van der Spek");
  for (const std::uint8_t uint1_v : {0x1A, 0x04, 0x17, 0x27, 0x10, 0x19, 0x80, 0x00})
    vec_data.push_back(static_cast<std::byte>(uint1_v));

  std::size_t st_size = 52 + vec_entries.size();
  for (const std::string& str_e : vec_entries)
    st_size += str_e.size();
  push_u32(static_cast<std::uint32_t>(st_size));  // Size
  push_u32(0);                                    // Type
  push_u32(0);                                    // Version
  push_u32(0);                                    // Game/Format(0 == TD)
  push_u32(static_cast<std::uint32_t>(vec_entries.size()));
  for (const std::string& str_e : vec_entries) {
    push_ascii(str_e);
    vec_data.push_back(std::byte{0});
  }

  return vec_data;
}

XccGlobalDatabase::XccGlobalDatabase(std::span<const std::byte> vec_source) {
  SpanReader reader{vec_source};

  // while (s.Peek() > -1)(L29):流尾即停。
  // while (s.Peek() > -1) (L29): stop at end of stream.
  while (reader.Position() < reader.Length()) {
    const auto int4_count = reader.ReadInt32();
    vec_entries_.reserve(vec_entries_.size() + static_cast<std::size_t>(int4_count));
    for (auto int4_i = 0; int4_i < int4_count; int4_i++) {
      // 文件名;随后跳过注释(第二个 NUL 串)(L35-49)。
      // The filename; then skip the comment (the second NUL string)
      // (L35-49).
      auto str_name = std::string{};
      while (const auto uint1_c = reader.ReadUInt8())
        str_name.push_back(static_cast<char>(uint1_c));
      vec_entries_.push_back(std::move(str_name));

      while (reader.ReadUInt8() != 0) {
      }
    }
  }
}

}  // namespace ora::fmt
