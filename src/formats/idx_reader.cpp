#include "formats/idx_reader.hpp"

#include "formats/vxl_reader.hpp"

namespace ora::fmt {

namespace {

IdxEntry ReadIdxEntry(SpanReader& reader) {
  auto str_name = ReadAscii(reader.ReadBytes(16));
  const auto int4_pos = static_cast<std::int32_t>(str_name.find('\0'));
  if (int4_pos != 0) {
    if (int4_pos < 0) [[unlikely]]

      throw std::runtime_error(
          "System.ArgumentOutOfRangeException: length ('-1') must be a non-negative value. (Parameter 'length')\n"
          "Actual value was -1.");
    str_name = str_name.substr(0, static_cast<std::size_t>(int4_pos));
  }

  auto entry = IdxEntry{};
  entry.str_filename = std::format("{}.wav", str_name);
  entry.uint4_offset = reader.ReadUInt32();
  entry.uint4_length = reader.ReadUInt32();
  entry.uint4_sample_rate = reader.ReadUInt32();
  entry.uint4_flags = reader.ReadUInt32();
  entry.uint4_chunk_size = reader.ReadUInt32();
  return entry;
}

}

std::string IdxEntry::ToString() const {
  return std::format("{} - offset 0x{:08x} - length 0x{:08x}", str_filename, uint4_offset, uint4_length);
}

IdxReader::IdxReader(std::span<const std::byte> vec_file) {
  auto reader = SpanReader{vec_file};
  reader.Seek(0);

  const auto str_id = ReadAscii(reader.ReadBytes(4));

  if (str_id != "GABA")
    throw std::runtime_error(std::format(
        "System.IO.InvalidDataException: Unable to load Idx file, did not find magic id, found {} instead", str_id));

  const auto int4_two = reader.ReadInt32();

  if (int4_two != 2)
    throw std::runtime_error(std::format(
        "System.IO.InvalidDataException: Unable to load Idx file, did not find magic number 2, found {} instead",
        int4_two));

  int4_sound_count = reader.ReadInt32();

  vec_entries.reserve(static_cast<std::size_t>(int4_sound_count));
  for (auto int4_i = 0; int4_i < int4_sound_count; int4_i++)
    vec_entries.push_back(ReadIdxEntry(reader));
}

}
