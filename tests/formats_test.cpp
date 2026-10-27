import std;
#include <cstdio>

#include "formats/crc32.hpp"
#include "formats/fast_byte_reader.hpp"
#include "formats/lcw.hpp"
#include "formats/lzo.hpp"
#include "formats/rle_zeros.hpp"
#include "formats/shp_d2.hpp"
#include "formats/shp_td.hpp"
#include "formats/mp3_loader.hpp"
#include "formats/ogg_loader.hpp"
#include "formats/png_sheet_loader.hpp"
#include "formats/r8_loader.hpp"
#include "formats/voc_loader.hpp"
#include "formats/shp_ts.hpp"
#include "formats/span_reader.hpp"
#include "formats/tmp_ra.hpp"
#include "formats/tmp_td.hpp"
#include "formats/tmp_ts.hpp"
#include "formats/vqa_video.hpp"
#include "formats/wsa_video.hpp"
#include "formats/xor_delta.hpp"
#include "gfx/palette.hpp"
#include "gfx/sprite_cache.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "gfx/sheet.hpp"
#include "meta/dump_format.hpp"

#include <miniz.h>

#include "formats/dds.hpp"
#include "formats/embedded_sprite_palette.hpp"
#include "formats/png.hpp"
#include "formats/shp_remastered.hpp"
#include "formats/targa.hpp"

#include "formats/blowfish.hpp"
#include "formats/blowfish_key_provider.hpp"
#include "formats/crc32.hpp"
#include "formats/xcc_database.hpp"
#include "fs/d2k_sound_resources.hpp"
#include "fs/file_system.hpp"
#include "fs/mix_file.hpp"
#include "fs/package_entry.hpp"

#include "formats/aud_reader.hpp"
#include "formats/ima_adpcm.hpp"
#include "formats/wav_reader.hpp"
#include "formats/westwood_compressed.hpp"

#include "formats/hva_reader.hpp"
#include "formats/idx_reader.hpp"
#include "formats/vxl_reader.hpp"

namespace {

std::int32_t int4_failures = 0;

#define ORA_CHECK(...)                                                         \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      std::println(stderr, "FAIL {}:{} {}", __FILE__, __LINE__, #__VA_ARGS__); \
      ++int4_failures;                                                         \
    }                                                                          \
  } while (0)

std::vector<std::byte> ReadFileBytes(const std::string& str_path) {
  auto stream_file = std::ifstream{str_path, std::ios::binary};
  if (!stream_file)
    throw std::runtime_error("cannot open " + str_path);
  auto str_bytes = std::string{std::istreambuf_iterator<char>{stream_file}, std::istreambuf_iterator<char>{}};
  auto vec_out = std::vector<std::byte>(str_bytes.size());
  if (!str_bytes.empty())
    std::memcpy(vec_out.data(), str_bytes.data(), str_bytes.size());
  return vec_out;
}

std::vector<std::byte> HexToBytes(std::string_view str_hex) {
  auto vec_out = std::vector<std::byte>{};
  if (str_hex == "-")
    return vec_out;
  ORA_CHECK(str_hex.size() % 2 == 0);
  for (std::size_t st_i = 0; st_i + 1 < str_hex.size(); st_i += 2) {
    const auto char_digit = [](char char_c) {
      if (char_c >= '0' && char_c <= '9') return char_c - '0';
      if (char_c >= 'A' && char_c <= 'F') return char_c - 'A' + 10;
      throw std::runtime_error("bad hex digit");
    };
    vec_out.push_back(static_cast<std::byte>(char_digit(str_hex[st_i]) * 16 + char_digit(str_hex[st_i + 1])));
  }
  return vec_out;
}

std::string BytesToHexLower(const std::byte* ptr_data, std::size_t st_len) {
  static constexpr const char* kDigits = "0123456789ABCDEF";
  auto str_out = std::string{};
  str_out.reserve(st_len * 2);
  for (std::size_t st_i = 0; st_i < st_len; st_i++) {
    const auto uint1_v = static_cast<std::uint8_t>(ptr_data[st_i]);
    str_out.push_back(kDigits[uint1_v >> 4]);
    str_out.push_back(kDigits[uint1_v & 0xF]);
  }
  return str_out;
}

std::string BytesToHexLower(const std::span<const std::byte> vec_data) {
  return vec_data.empty() ? std::string{"-"} : BytesToHexLower(vec_data.data(), vec_data.size());
}

std::vector<std::byte> MakeBytes(std::initializer_list<std::uint8_t> vec_values) {
  auto vec_out = std::vector<std::byte>{};
  vec_out.reserve(vec_values.size());
  for (const auto uint1_v : vec_values)
    vec_out.push_back(static_cast<std::byte>(uint1_v));
  return vec_out;
}

void TestFastByteReader() {
  const auto vec_src = MakeBytes({0x01, 0x02, 0x03, 0x04, 0x05});
  auto reader = ora::fmt::FastByteReader{vec_src};
  ORA_CHECK(!reader.Done());
  ORA_CHECK(reader.Remaining() == 5);
  ORA_CHECK(static_cast<std::uint8_t>(reader.ReadByte()) == 0x01);
  ORA_CHECK(reader.ReadWord() == 0x0302);
  ORA_CHECK(reader.Remaining() == 2);

  auto vec_dest = std::vector<std::byte>(3);
  reader.CopyTo(vec_dest, 1, 2);
  ORA_CHECK(vec_dest == MakeBytes({0x00, 0x04, 0x05}));
  ORA_CHECK(reader.Done());
}

void TestRLEZeros() {

  const auto vec_src = MakeBytes({0x01, 0x02, 0x00, 0x03, 0x05});
  auto vec_dest = std::vector<std::byte>(8, std::byte{0xAA});
  ora::fmt::rle_zeros::DecodeInto(vec_src, vec_dest, 0);
  ORA_CHECK(vec_dest == MakeBytes({0x01, 0x02, 0x00, 0x00, 0x00, 0x05, 0xAA, 0xAA}));

  auto vec_dest_small = std::vector<std::byte>(2);
  auto b_threw = false;
  try {
    ora::fmt::rle_zeros::DecodeInto(vec_src, vec_dest_small, 1);
  } catch (const std::runtime_error&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

void TestXORDelta() {
  using ora::fmt::xor_delta::DecodeInto;

  auto vec_src = MakeBytes({0x05, 0x10, 0x20, 0x30, 0x40, 0x50, 0x80, 0x00, 0x00, 0x00});
  auto vec_dest = std::vector<std::byte>(8, std::byte{0x0F});
  ORA_CHECK(DecodeInto(vec_src, vec_dest, 0) == 5);
  ORA_CHECK(vec_dest == MakeBytes({0x1F, 0x2F, 0x3F, 0x40 ^ 0x0F, 0x5F, 0x0F, 0x0F, 0x0F}));

  vec_src = MakeBytes({0x00, 0x04, 0xFF, 0x80, 0x00, 0x00, 0x00});
  std::ranges::fill(vec_dest, std::byte{0x11});
  ORA_CHECK(DecodeInto(vec_src, vec_dest, 0) == 4);
  ORA_CHECK(vec_dest == MakeBytes({0xEE, 0xEE, 0xEE, 0xEE, 0x11, 0x11, 0x11, 0x11}));

  vec_src = MakeBytes({0x83, 0x80, 0x02, 0x00, 0x80, 0x00, 0x00, 0x00});
  std::ranges::fill(vec_dest, std::byte{0});
  vec_dest[2] = std::byte{0x7F};
  vec_dest[7] = std::byte{0x55};
  ORA_CHECK(DecodeInto(vec_src, vec_dest, 0) == 5);
  ORA_CHECK(vec_dest == MakeBytes({0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x55}));

  vec_src = MakeBytes({0x80, 0x02, 0x80, 0x0A, 0x14, 0x80, 0x00, 0x00, 0x00});
  std::ranges::fill(vec_dest, std::byte{0x01});
  ORA_CHECK(DecodeInto(vec_src, vec_dest, 0) == 2);
  const auto vec_expect_c3 = MakeBytes({0x0B, 0x15});
  ORA_CHECK(std::ranges::equal(std::span<const std::byte>{vec_dest}.first(2), std::span<const std::byte>{vec_expect_c3}));

  vec_src = MakeBytes({0x80, 0x03, 0xC0, 0xAA, 0x80, 0x00, 0x00, 0x00});
  std::ranges::fill(vec_dest, std::byte{0x10});
  ORA_CHECK(DecodeInto(vec_src, vec_dest, 0) == 3);
  const auto vec_expect_c4 = MakeBytes({0xBA, 0xBA, 0xBA});
  ORA_CHECK(std::ranges::equal(std::span<const std::byte>{vec_dest}.first(3), std::span<const std::byte>{vec_expect_c4}));
}

void TestLCW() {
  using ora::fmt::lcw::DecodeInto;

  auto vec_src = MakeBytes({0x83, 0xAA, 0xBB, 0xCC, 0x80});
  auto vec_dest = std::vector<std::byte>(3);
  ORA_CHECK(DecodeInto(vec_src, vec_dest) == 3);
  ORA_CHECK(vec_dest == MakeBytes({0xAA, 0xBB, 0xCC}));

  vec_src = MakeBytes({0xFE, 0x05, 0x00, 0x77, 0x80});
  vec_dest.assign(5, std::byte{0});
  ORA_CHECK(DecodeInto(vec_src, vec_dest) == 5);
  ORA_CHECK(vec_dest == MakeBytes({0x77, 0x77, 0x77, 0x77, 0x77}));

  vec_src = MakeBytes({0x84, 0x11, 0x22, 0x33, 0x44, 0xC1, 0x00, 0x00, 0x80});
  vec_dest.assign(8, std::byte{0});
  ORA_CHECK(DecodeInto(vec_src, vec_dest) == 8);
  ORA_CHECK(vec_dest == MakeBytes({0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44}));

  vec_src = MakeBytes({0x82, 0x5A, 0x21, 0x10, 0x01, 0x80});
  vec_dest.assign(6, std::byte{0});
  ORA_CHECK(DecodeInto(vec_src, vec_dest) == 6);
  ORA_CHECK(vec_dest == MakeBytes({0x5A, 0x21, 0x21, 0x21, 0x21, 0x21}));

  vec_src = MakeBytes({0x10, 0x00, 0x80});
  vec_dest.assign(2, std::byte{0});
  ORA_CHECK(DecodeInto(vec_src, vec_dest) == 0);

  vec_src = MakeBytes({0x83, 0x5A, 0x21, 0x33, 0x80});
  vec_dest.assign(2, std::byte{0});
  auto b_threw = false;
  try {
    DecodeInto(vec_src, vec_dest);
  } catch (const std::runtime_error&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);

  auto vec_pattern = std::vector<std::byte>{};
  for (auto int4_i = 0; int4_i < 40; int4_i++)
    vec_pattern.push_back(static_cast<std::byte>(int4_i * 7 % 251));
  for (auto int4_i = 0; int4_i < 300; int4_i++)
    vec_pattern.push_back(std::byte{0xAB});
  for (auto int4_i = 0; int4_i < 61; int4_i++)
    vec_pattern.push_back(static_cast<std::byte>((int4_i * int4_i) % 256));

  const auto vec_encoded = ora::fmt::lcw::Encode(vec_pattern);
  auto vec_round = std::vector<std::byte>(vec_pattern.size());
  ORA_CHECK(DecodeInto(vec_encoded, vec_round) == static_cast<std::int32_t>(vec_pattern.size()));
  ORA_CHECK(vec_round == vec_pattern);

  ORA_CHECK(vec_encoded.front() == static_cast<std::byte>(0x80 | 40));
  bool b_has_rle = false;
  for (std::size_t st_i = 0; st_i + 3 < vec_encoded.size(); st_i++)
    if (vec_encoded[st_i] == std::byte{0xFE}) {
      b_has_rle = true;
      break;
    }
  ORA_CHECK(b_has_rle);
}

void TestCRC32() {

  auto arr_check = std::vector<std::byte>{};
  for (const char char_c : std::string_view{"123456789"})
    arr_check.push_back(static_cast<std::byte>(char_c));
  const auto uint4_whole = ora::fmt::CRC32::Calculate(arr_check);
  const auto uint4_chained = ora::fmt::CRC32::Finish(
      ora::fmt::CRC32::Update(ora::fmt::CRC32::Update(0xFFFFFFFF, std::span<const std::byte>{arr_check}.first(4)),
                              std::span<const std::byte>{arr_check}.last(5)));
  ORA_CHECK(uint4_whole == 0xCBF43926);
  ORA_CHECK(uint4_whole == uint4_chained);
}

std::vector<std::byte> MakeSyntheticShpTD(std::span<const std::byte> vec_frame, std::uint16_t uint2_w,
                                          std::uint16_t uint2_h) {
  auto vec_encoded = ora::fmt::lcw::Encode(vec_frame);
  const auto uint4_data_offset = 14u + 8u * 3u;
  auto vec_file = std::vector<std::byte>(uint4_data_offset + vec_encoded.size());

  const auto put_u16 = [&vec_file](std::size_t st_pos, std::uint16_t uint2_v) {
    vec_file[st_pos] = static_cast<std::byte>(uint2_v & 0xFF);
    vec_file[st_pos + 1] = static_cast<std::byte>(uint2_v >> 8);
  };
  const auto put_u32 = [&vec_file](std::size_t st_pos, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_file[st_pos + static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(uint4_v >> (8 * int4_i));
  };

  put_u16(0, 1);
  put_u16(6, uint2_w);
  put_u16(8, uint2_h);

  put_u32(14, uint4_data_offset | (0x80u << 24));

  put_u32(22, static_cast<std::uint32_t>(vec_file.size()));

  std::ranges::copy(vec_encoded, vec_file.begin() + static_cast<std::ptrdiff_t>(uint4_data_offset));
  return vec_file;
}

void TestSyntheticShpTD() {

  constexpr std::int32_t int4_w = 24, int4_h = 24;
  auto vec_frame = std::vector<std::byte>(int4_w * int4_h);
  for (auto int4_y = 8; int4_y < 16; int4_y++)
    for (auto int4_x = 8; int4_x < 16; int4_x++)
      vec_frame[static_cast<std::size_t>(int4_y * int4_w + int4_x)] =
          static_cast<std::byte>(int4_y * 8 + int4_x);

  auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
  ORA_CHECK(ora::fmt::TryParseShpTD(MakeSyntheticShpTD(vec_frame, int4_w, int4_h), vec_frames));
  ORA_CHECK(vec_frames.size() == 1);
  const auto& frame = *vec_frames.front();
  ORA_CHECK(frame.Type() == ora::gfx::SpriteFrameType::Indexed8);
  ORA_CHECK(frame.Size() == ora::int2{8, 8});
  ORA_CHECK(frame.FrameSize() == ora::int2{int4_w, int4_h});
  ORA_CHECK(frame.Offset().X == 0.0f && frame.Offset().Y == 0.0f);
  ORA_CHECK(frame.Data().size() == 64);
  ORA_CHECK(frame.Data()[0] == std::byte{8 * 8 + 8});
  ORA_CHECK(frame.Data()[63] == std::byte{15 * 8 + 15});
  ORA_CHECK(frame.DisableExportPadding() == false);

  auto vec_blank = std::vector<std::byte>(int4_w * int4_h);
  ORA_CHECK(ora::fmt::TryParseShpTD(MakeSyntheticShpTD(vec_blank, int4_w, int4_h), vec_frames));
  ORA_CHECK(vec_frames.size() == 1);
  ORA_CHECK(vec_frames.front()->Size() == ora::int2{0, 0});
  ORA_CHECK(vec_frames.front()->FrameSize() == ora::int2{0, 0});
  ORA_CHECK(vec_frames.front()->Data().empty());

  ORA_CHECK(!ora::fmt::IsShpTD(MakeBytes({0x01, 0x00, 0x00, 0x00})));
  ORA_CHECK(!ora::fmt::IsShpD2(MakeBytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})));
  ORA_CHECK(!ora::fmt::IsShpTS(MakeBytes({0x01, 0x00})));
  ORA_CHECK(!ora::fmt::IsTmpTD(std::vector<std::byte>(40, std::byte{0})));
}

void TestSheetBuilderFrameAdd() {
  constexpr std::int32_t int4_w = 24, int4_h = 24;
  auto vec_frame = std::vector<std::byte>(int4_w * int4_h);
  for (auto int4_y = 8; int4_y < 16; int4_y++)
    for (auto int4_x = 8; int4_x < 16; int4_x++)
      vec_frame[static_cast<std::size_t>(int4_y * int4_w + int4_x)] =
          static_cast<std::byte>(int4_y * 8 + int4_x);

  auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
  ORA_CHECK(ora::fmt::TryParseShpTD(MakeSyntheticShpTD(vec_frame, int4_w, int4_h), vec_frames));
  const auto& frame = *vec_frames.front();

  auto builder = ora::gfx::SheetBuilder{
      ora::gfx::SheetType::Indexed,
      [] {
        return std::make_unique<ora::gfx::Sheet>(ora::gfx::SheetType::Indexed, ora::int2{256, 256}, nullptr);
      },
      1};

  const auto sprite = builder.Add(frame);

  ORA_CHECK(sprite.ptr_sheet == builder.Current());
  ORA_CHECK(sprite.Bounds.Width == 8 && sprite.Bounds.Height == 8);
  ORA_CHECK(sprite.kind_channel == ora::gfx::TextureChannel::Red);
  const auto vec_sheet = builder.Current()->GetData();
  for (auto int4_y = 0; int4_y < 8; int4_y++)
    for (auto int4_x = 0; int4_x < 8; int4_x++) {

      const auto st_pixel = static_cast<std::size_t>((sprite.Bounds.Y + int4_y) * 256 + sprite.Bounds.X + int4_x) * 4 + 2;
      ORA_CHECK(vec_sheet[st_pixel] == frame.Data()[static_cast<std::size_t>(int4_y * 8 + int4_x)]);
    }

  auto vec_blank = std::vector<std::byte>(int4_w * int4_h);
  ORA_CHECK(ora::fmt::TryParseShpTD(MakeSyntheticShpTD(vec_blank, int4_w, int4_h), vec_frames));
  const auto sprite_blank = builder.Add(*vec_frames.front());
  ORA_CHECK(sprite_blank.Bounds.Width == 0 && sprite_blank.Bounds.Height == 0);
}

void TestBlowfish() {

  const auto vec_zero_key = std::vector<std::byte>(8, std::byte{0});
  const ora::fmt::Blowfish fish_zero{vec_zero_key};
  const auto vec_zero_words = std::vector<std::uint32_t>{0u, 0u};
  const auto vec_cipher = fish_zero.Encrypt(vec_zero_words);

  ORA_CHECK(vec_cipher.size() == 2 && vec_cipher[0] == 0x4597F94Eu && vec_cipher[1] == 0x78DD9861u);

  auto vec_key = std::vector<std::byte>(21);
  for (std::size_t st_i = 0; st_i < vec_key.size(); st_i++)
    vec_key[st_i] = static_cast<std::byte>((st_i * 37 + 11) & 0xFF);
  const ora::fmt::Blowfish fish{vec_key};
  auto vec_words = std::vector<std::uint32_t>(8);
  for (std::size_t st_i = 0; st_i < vec_words.size(); st_i++)
    vec_words[st_i] = static_cast<std::uint32_t>(0x9E3779B9u * (st_i + 1));
  const auto vec_round = fish.Decrypt(fish.Encrypt(vec_words));
  ORA_CHECK(vec_round == vec_words);

  const auto vec_odd = std::vector<std::uint32_t>{1u, 2u, 3u};
  const auto vec_odd_out = fish.Encrypt(vec_odd);
  ORA_CHECK(vec_odd_out.size() == 3 && vec_odd_out[2] == 0);

  const ora::fmt::Blowfish fish_short{std::span<const std::byte>{vec_zero_key}.first(3)};
  const auto vec_two_words = std::vector<std::uint32_t>{1u, 2u};
  ORA_CHECK(fish_short.Encrypt(vec_two_words).size() == 2);
}

void TestXccDatabases() {

  const std::vector<std::string> vec_names = {"ALPHA.DAT", "beta.bin", "gamma"};
  const auto vec_data = ora::fmt::XccLocalDatabase::MakeData(vec_names);
  const ora::fmt::XccLocalDatabase database{vec_data};
  ORA_CHECK(database.Entries() == vec_names);

  ORA_CHECK(vec_data.size() == 52 + (9 + 8 + 5) + 3);
  ORA_CHECK(std::string_view{reinterpret_cast<const char*>(vec_data.data()), 24} ==
            "XCC by Olaf van der Spek");

  auto b_threw = false;
  try {
    const ora::fmt::XccLocalDatabase database_bad{std::span<const std::byte>{vec_data}.first(52)};
  } catch (const std::exception&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);

  auto vec_global = std::vector<std::byte>{};
  const auto push_u32 = [&vec_global](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_global.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  const auto push_sz = [&vec_global](std::string_view sv) {
    for (const char chr_c : sv)
      vec_global.push_back(static_cast<std::byte>(chr_c));
    vec_global.push_back(std::byte{0});
  };
  push_u32(2);
  push_sz("one.dat");
  push_sz("first");
  push_sz("two.dat");
  push_sz("");
  push_u32(1);
  push_sz("three.dat");
  push_sz("note");
  const ora::fmt::XccGlobalDatabase global{vec_global};
  ORA_CHECK(global.Entries() ==
            std::vector<std::string>({"one.dat", "two.dat", "three.dat"}));
}

void TestPackageEntryHash() {
  using enum ora::fs::PackageHashType;

  const std::uint32_t uint4_classic_abc = ora::fs::PackageEntry::HashFilename("abc", Classic);
  ORA_CHECK(uint4_classic_abc != ora::fs::PackageEntry::HashFilename("abc", CRC32));
  ORA_CHECK(ora::fs::PackageEntry::HashFilename("abcd", Classic) != uint4_classic_abc);

  ORA_CHECK(ora::fs::PackageEntry::HashFilename("AbC", Classic) ==
            ora::fs::PackageEntry::HashFilename("aBc", Classic));
}

void TestMixFile() {
  using enum ora::fs::PackageHashType;

  const std::vector<std::string> vec_names = {"data.bin", "local mix database.dat"};
  auto vec_content0 = std::vector<std::byte>(6);
  for (std::size_t st_i = 0; st_i < 6; st_i++)
    vec_content0[st_i] = static_cast<std::byte>(st_i * 41);
  const std::vector<std::string> vec_db_names = {"data.bin", "local mix database.dat"};
  const auto vec_db = ora::fmt::XccLocalDatabase::MakeData(vec_db_names);

  auto vec_header = std::vector<std::byte>{};
  const auto put_u16 = [&vec_header](std::uint16_t uint2_v) {
    vec_header.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec_header.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  const auto put_u32 = [&vec_header](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_header.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  put_u16(2);
  put_u32(6 + vec_db.size());
  put_u32(ora::fs::PackageEntry::HashFilename(vec_names[0], Classic));
  put_u32(0);
  put_u32(6);

  put_u32(ora::fs::PackageEntry::HashFilename(vec_names[1], Classic));
  put_u32(6);
  put_u32(static_cast<std::uint32_t>(vec_db.size()));

  auto vec_file = vec_header;
  vec_file.insert(vec_file.end(), vec_content0.begin(), vec_content0.end());
  vec_file.insert(vec_file.end(), vec_db.begin(), vec_db.end());

  const std::vector<std::string> vec_globals = {"data.bin", "OTHER.FILE"};
  auto vec_bytes = std::vector<char>{};
  vec_bytes.resize(vec_file.size());
  std::memcpy(vec_bytes.data(), vec_file.data(), vec_file.size());
  const ora::fs::MixFile mix{std::move(vec_bytes), "test.mix", vec_globals};

  ORA_CHECK(mix.IsCncMix() && !mix.IsEncrypted() && mix.DataStart() == 30);
  auto vec_contents = mix.Contents();
  std::ranges::sort(vec_contents);
  ORA_CHECK(vec_contents == vec_names);
  ORA_CHECK(mix.Contains("data.bin") && !mix.Contains("OTHER.FILE"));
  const auto vec_stream = mix.GetStream("data.bin");
  ORA_CHECK(vec_stream.has_value() && vec_stream->size() == 6 &&
            static_cast<std::uint8_t>((*vec_stream)[3]) == 123);

  auto vec_enc = std::vector<std::byte>{};
  const auto push_u16e = [&vec_enc](std::uint16_t uint2_v) {
    vec_enc.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec_enc.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  push_u16e(0);
  push_u16e(0x2);
  for (auto int4_i = 0; int4_i < 80; int4_i++)
    vec_enc.push_back(std::byte{0x5A});

  auto b_threw = false;
  try {
    auto vec_copy = std::vector<char>{};
    vec_copy.resize(vec_enc.size());
    std::memcpy(vec_copy.data(), vec_enc.data(), vec_enc.size());
    const ora::fs::MixFile mix_bad{std::move(vec_copy), "bad.mix", {}};
  } catch (const std::exception&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);

  ora::fs::FileSystem fileSystem{};
  ora::fs::MixLoader loader{};
  std::unique_ptr<ora::fs::IReadOnlyPackage> package;
  auto vec_span = std::vector<char>{};
  vec_span.resize(vec_file.size());
  std::memcpy(vec_span.data(), vec_file.data(), vec_file.size());
  ORA_CHECK(loader.TryParsePackage(vec_span, "SOME.MIX", fileSystem, package) && package != nullptr);
  ORA_CHECK(loader.TryParsePackage(vec_span, "some.mix", fileSystem, package));
  ORA_CHECK(!loader.TryParsePackage(vec_span, "some.dat", fileSystem, package) && package == nullptr);
}

void TestD2kSoundResources() {
  auto vec_file = std::vector<std::byte>{};
  const auto push_u32 = [&vec_file](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_file.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  const auto push_sz = [&vec_file](std::string_view sv) {
    for (const char chr_c : sv)
      vec_file.push_back(static_cast<std::byte>(chr_c));
    vec_file.push_back(std::byte{0});
  };

  const std::size_t st_dir = 6 + 4 + 4 + 8 + 4 + 4;
  const std::uint32_t uint4_data_start = static_cast<std::uint32_t>(4 + st_dir);
  push_u32(static_cast<std::uint32_t>(st_dir));
  push_sz("a.wav");
  push_u32(uint4_data_start);
  push_u32(4);
  push_sz("btr.wav");
  push_u32(uint4_data_start + 4);
  push_u32(3);
  vec_file.push_back(std::byte{0x11});
  vec_file.push_back(std::byte{0x22});
  vec_file.push_back(std::byte{0x33});
  vec_file.push_back(std::byte{0x44});
  vec_file.push_back(std::byte{0x55});
  vec_file.push_back(std::byte{0x66});
  vec_file.push_back(std::byte{0x77});

  auto vec_chars = std::vector<char>{};
  vec_chars.resize(vec_file.size());
  std::memcpy(vec_chars.data(), vec_file.data(), vec_file.size());
  const ora::fs::D2kSoundResources rs{std::move(vec_chars), "sounds.rs"};
  ORA_CHECK(rs.Contains("a.wav") && rs.Contains("btr.wav") && !rs.Contains("c.wav"));
  const auto vec_a = rs.GetStream("a.wav");
  ORA_CHECK(vec_a.has_value() && vec_a->size() == 4 && (*vec_a)[0] == 0x11 && (*vec_a)[3] == 0x44);
  const auto vec_b = rs.GetStream("btr.wav");
  ORA_CHECK(vec_b.has_value() && vec_b->size() == 3 && (*vec_b)[2] == 0x77);
  ora::fs::FileSystem fileSystemForOpen{};
  ORA_CHECK(rs.OpenPackage("a.wav", fileSystemForOpen) == nullptr);

  ora::fs::D2kSoundResourcesLoader loader{};
  ora::fs::FileSystem fileSystem{};
  std::unique_ptr<ora::fs::IReadOnlyPackage> package;
  auto vec_span = std::vector<char>{};
  vec_span.resize(vec_file.size());
  std::memcpy(vec_span.data(), vec_file.data(), vec_file.size());
  ORA_CHECK(loader.TryParsePackage(vec_span, "X.RS", fileSystem, package) && package != nullptr);
  ORA_CHECK(!loader.TryParsePackage(vec_span, "X.WAV", fileSystem, package));

  auto vec_dup = std::vector<std::byte>{};
  const auto push_u32d = [&vec_dup](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_dup.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  const auto push_szd = [&vec_dup](std::string_view sv) {
    for (const char chr_c : sv)
      vec_dup.push_back(static_cast<std::byte>(chr_c));
    vec_dup.push_back(std::byte{0});
  };

  push_u32d(28);
  push_szd("a.wav");
  push_u32d(0);
  push_u32d(0);
  push_szd("a.wav");
  push_u32d(0);
  push_u32d(0);
  auto b_threw = false;
  try {
    auto vec_copy = std::vector<char>{};
    vec_copy.resize(vec_dup.size());
    std::memcpy(vec_copy.data(), vec_dup.data(), vec_dup.size());
    const ora::fs::D2kSoundResources rs_bad{std::move(vec_copy), "dup.rs"};
  } catch (const std::exception&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

void TestImaAdpcm() {
  using ora::fmt::ima_adpcm::DecodeImaAdpcmSample;

  auto int4_index = 0;
  auto int4_current = 0;
  auto int2_s = DecodeImaAdpcmSample(0x07, int4_index, int4_current);

  ORA_CHECK(int2_s == 12 && int4_index == 8 && int4_current == 12);
  for (auto int4_i = 0; int4_i < 200; int4_i++)
    int2_s = DecodeImaAdpcmSample(0x07, int4_index, int4_current);
  ORA_CHECK(int4_index == 88 && int4_current == 32767 && int2_s == 32767);

  for (auto int4_i = 0; int4_i < 200; int4_i++)
    int2_s = DecodeImaAdpcmSample(0x0F, int4_index, int4_current);
  ORA_CHECK(int4_current == -32768 && int2_s == -32768);

  int4_index = 0;
  DecodeImaAdpcmSample(0x00, int4_index, int4_current);
  ORA_CHECK(int4_index == 0);

  const auto vec_raw = MakeBytes({0x21, 0x73, 0x0C, 0xF8});
  auto int4_idx3 = 0;
  auto int4_cur3 = 0;
  const auto int2_first = DecodeImaAdpcmSample(0x21, int4_idx3, int4_cur3);
  auto vec_out2 = std::vector<std::byte>(16);
  int4_idx3 = 0;
  int4_cur3 = 0;
  ora::fmt::ima_adpcm::LoadImaAdpcmSound(vec_raw, int4_idx3, int4_cur3, vec_out2);
  ORA_CHECK(vec_out2[0] == static_cast<std::byte>(int2_first) && vec_out2[1] == static_cast<std::byte>(int2_first >> 8));
  ORA_CHECK(int4_idx3 <= 88);

  auto b_threw = false;
  try {
    auto vec_bad = std::vector<std::byte>(15);
    ora::fmt::ima_adpcm::LoadImaAdpcmSound(vec_raw, int4_idx3, vec_bad);
  } catch (const std::runtime_error&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

void TestWestwoodCompressed() {
  using ora::fmt::westwood_compressed::DecodeWestwoodCompressedSample;

  const auto vec_in = MakeBytes({0xC1, 0x82, 0x11, 0x22, 0x33, 0xA5, 0x00, 0xE4, 0x40, 0x1B, 0x40, 0x27, 0xBF});
  auto vec_out = std::vector<std::byte>(15);
  DecodeWestwoodCompressedSample(vec_in, vec_out);
  ORA_CHECK(vec_out == MakeBytes({0x80, 0x80, 0x11, 0x22, 0x33, 0x38, 0x36, 0x35, 0x35, 0x36, 0x39, 0x31, 0x30,
                                  0x2A, 0x29}));

  const auto vec_same = MakeBytes({0x44, 0x55, 0x66, 0x77});
  auto vec_pass = std::vector<std::byte>(4, std::byte{0});
  DecodeWestwoodCompressedSample(vec_same, vec_pass);
  ORA_CHECK(vec_pass == vec_same);

  const auto vec_jump = MakeBytes({0x80, 0x02, 0xB0});
  auto vec_jump_out = std::vector<std::byte>(2);
  DecodeWestwoodCompressedSample(vec_jump, vec_jump_out);
  ORA_CHECK(vec_jump_out == MakeBytes({0x02, 0xF2}));

  auto b_threw = false;
  try {
    auto vec_small = std::vector<std::byte>(3);
    DecodeWestwoodCompressedSample(vec_in, vec_small);
  } catch (const std::runtime_error&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

void TestAudReader() {

  auto vec_aud = std::vector<std::byte>(20, std::byte{0});
  ORA_CHECK(!ora::fmt::IsAud(vec_aud));
  vec_aud[11] = std::byte{99};
  ORA_CHECK(ora::fmt::IsAud(vec_aud));
  vec_aud[11] = std::byte{1};
  ORA_CHECK(ora::fmt::IsAud(vec_aud));
  vec_aud[11] = std::byte{2};
  ORA_CHECK(!ora::fmt::IsAud(vec_aud));
  ORA_CHECK(!ora::fmt::IsAud(std::span<const std::byte>{vec_aud}.first(11)));

  {
    auto vec_bad = std::vector<std::byte>{};
    const auto push_u16 = [&vec_bad](std::uint16_t uint2_v) {
      vec_bad.push_back(static_cast<std::byte>(uint2_v & 0xFF));
      vec_bad.push_back(static_cast<std::byte>(uint2_v >> 8));
    };
    const auto push_u32 = [&vec_bad](std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec_bad.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
    };
    push_u16(22050);
    push_u32(8);
    push_u32(4);
    vec_bad.push_back(std::byte{0x02});
    vec_bad.push_back(std::byte{42});
    auto info = ora::fmt::AudInfo{};
    ORA_CHECK(!ora::fmt::LoadAudSound(vec_bad, info));

    vec_bad[11] = std::byte{99};
    push_u16(0);
    push_u16(0);
    push_u32(0);
    info = ora::fmt::AudInfo{};
    auto b_threw = false;
    try {
      ora::fmt::LoadAudSound(vec_bad, info);
      ora::fmt::DecodeAudPcm(vec_bad, info);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "Chunk header is bogus";
    }
    ORA_CHECK(b_threw);

    auto vec_pcm = std::vector<std::byte>{};
    ORA_CHECK(!ora::fmt::TryParseAud(vec_bad, info, vec_pcm));
    ORA_CHECK(!ora::fmt::TryParseAud(MakeBytes({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}), info, vec_pcm));
  }
}

void TestWavReader() {

  ORA_CHECK(!ora::fmt::IsWave(MakeBytes({0x52, 0x49, 0x46, 0x46})));
  ORA_CHECK(!ora::fmt::IsWave(MakeBytes({'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'X'})));
  ORA_CHECK(ora::fmt::IsWave(MakeBytes({'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'})));

  const auto make_wav_head = [](std::uint16_t uint2_format, std::uint16_t uint2_channels) {
    auto vec = std::vector<std::byte>{};
    const auto push_u16 = [&vec](std::uint16_t uint2_v) {
      vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
      vec.push_back(static_cast<std::byte>(uint2_v >> 8));
    };
    const auto push_u32 = [&vec](std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
    };
    for (const char chr_c : "RIFF")
      if (chr_c)
        vec.push_back(static_cast<std::byte>(chr_c));
    push_u32(0);
    for (const char chr_c : "WAVEfmt ")
      if (chr_c)
        vec.push_back(static_cast<std::byte>(chr_c));
    push_u32(16);
    push_u16(uint2_format);
    push_u16(uint2_channels);
    push_u32(22050);
    push_u32(44100);
    push_u16(2);
    push_u16(16);
    for (const char chr_c : "data")
      if (chr_c)
        vec.push_back(static_cast<std::byte>(chr_c));
    push_u32(2);
    vec.push_back(std::byte{0});
    vec.push_back(std::byte{0});
    return vec;
  };

  {
    auto info = ora::fmt::WavInfo{};
    auto b_threw = false;
    try {
      ora::fmt::LoadWavSound(make_wav_head(3, 1), info);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.NotSupportedException: Compression type 3 is not supported.";
    }
    ORA_CHECK(b_threw);
  }

  {
    auto info = ora::fmt::WavInfo{};
    auto b_threw = false;
    try {
      ora::fmt::LoadWavSound(make_wav_head(1, 3), info);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.NotSupportedException: Expected 1 or 2 channels only for WAV file, received: 3";
    }
    ORA_CHECK(b_threw);
  }

  auto info = ora::fmt::WavInfo{};
  auto vec_pcm = std::vector<std::byte>{};
  ORA_CHECK(!ora::fmt::TryParseWav(make_wav_head(3, 1), info, vec_pcm));
  ORA_CHECK(!ora::fmt::TryParseWav(MakeBytes({0x01}), info, vec_pcm));

  {
    auto vec_ms = std::vector<std::byte>{};
    const auto push_u16 = [&vec_ms](std::uint16_t uint2_v) {
      vec_ms.push_back(static_cast<std::byte>(uint2_v & 0xFF));
      vec_ms.push_back(static_cast<std::byte>(uint2_v >> 8));
    };
    const auto push_u32 = [&vec_ms](std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec_ms.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
    };
    for (const char chr_c : "RIFF")
      if (chr_c)
        vec_ms.push_back(static_cast<std::byte>(chr_c));
    push_u32(0);
    for (const char chr_c : "WAVEfmt ")
      if (chr_c)
        vec_ms.push_back(static_cast<std::byte>(chr_c));
    push_u32(20);
    push_u16(2);
    push_u16(1);
    push_u32(11025);
    push_u32(5512);
    push_u16(9);
    push_u16(4);
    push_u16(0);
    push_u16(0);
    for (const char chr_c : "data")
      if (chr_c)
        vec_ms.push_back(static_cast<std::byte>(chr_c));
    push_u32(9);
    vec_ms.push_back(std::byte{7});
    for (auto int4_i = 0; int4_i < 8; int4_i++)
      vec_ms.push_back(std::byte{0});

    auto info_ms = ora::fmt::WavInfo{};
    auto b_threw = false;
    try {
      ora::fmt::LoadWavSound(vec_ms, info_ms);
      ora::fmt::DecodeWavPcm(vec_ms, info_ms);
    } catch (const std::runtime_error&) {
      b_threw = true;
    }
    ORA_CHECK(b_threw);
  }
}

std::vector<std::byte> MakeVocFile(std::initializer_list<std::tuple<int, std::vector<std::byte>>> vec_blocks) {
  auto vec = std::vector<std::byte>{};
  const std::string str_desc = "Creative Voice File";
  for (const char chr_c : str_desc)
    vec.push_back(static_cast<std::byte>(chr_c));
  for (std::size_t st_i = str_desc.size(); st_i < 20; st_i++)
    vec.push_back(std::byte{0});
  const auto put_u16 = [&vec](std::uint16_t uint2_v) {
    vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  put_u16(26);
  put_u16(0x0114);
  put_u16(~0x0114 + 0x1234);
  for (const auto& [int4_code, vec_body] : vec_blocks) {
    vec.push_back(static_cast<std::byte>(int4_code));
    const std::uint32_t uint4_len = static_cast<std::uint32_t>(vec_body.size());
    vec.push_back(static_cast<std::byte>(uint4_len & 0xFF));
    vec.push_back(static_cast<std::byte>((uint4_len >> 8) & 0xFF));
    vec.push_back(static_cast<std::byte>((uint4_len >> 16) & 0xFF));
    vec.insert(vec.end(), vec_body.begin(), vec_body.end());
  }
  vec.push_back(std::byte{0});
  return vec;
}

std::vector<std::byte> BytesOf(std::initializer_list<std::uint8_t> vec_list) {
  std::vector<std::byte> vec_out;
  for (const std::uint8_t uint1_v : vec_list)
    vec_out.push_back(static_cast<std::byte>(uint1_v));
  return vec_out;
}

void TestVocLoader() {
  using ora::fmt::VocInfo;
  using ora::fmt::TryParseVoc;

  auto vec_file = MakeVocFile({

      {8, BytesOf({0x00, 0xCE, 0x00, 0x00})},

      {1, BytesOf({0xD2, 0x00, 0x10, 0x20, 0x30, 0x40})},

      {3, BytesOf({0x03, 0x00, 0xD2})},

      {6, BytesOf({0x01, 0x00})},
      {7, {}},

      {8, BytesOf({0x00, 0xCE, 0x00, 0x00})},
      {1, BytesOf({0xD2, 0x00, 0xAA, 0xBB})},
  });

  VocInfo info_voc;
  std::vector<std::byte> vec_pcm;
  ORA_CHECK(TryParseVoc(vec_file, info_voc, vec_pcm));

  ORA_CHECK(info_voc.int4_sample_rate == 20000);
  ORA_CHECK(info_voc.int4_total_samples == 6);
  ORA_CHECK(vec_pcm.size() == 6);
  ORA_CHECK(static_cast<std::uint8_t>(vec_pcm[0]) == 0x10 &&
            static_cast<std::uint8_t>(vec_pcm[1]) == 0x20 &&
            static_cast<std::uint8_t>(vec_pcm[4]) == 0xAA &&
            static_cast<std::uint8_t>(vec_pcm[5]) == 0xBB);

  {
    const auto vec_simple = MakeVocFile({{1, BytesOf({0xA5, 0x00, 0x01})}});
    VocInfo info_a;
    std::vector<std::byte> vec_pcm_a1;
    ORA_CHECK(TryParseVoc(vec_simple, info_a, vec_pcm_a1));
    ORA_CHECK(info_a.int4_sample_rate == 11025);
  }

  {
    auto vec_bad = MakeVocFile({});

    vec_bad[18] = static_cast<std::byte>('X');
    VocInfo info_bad;
    std::vector<std::byte> vec_pcm_bad;
    ORA_CHECK(!TryParseVoc(vec_bad, info_bad, vec_pcm_bad));
  }
  {
    auto vec_bad = MakeVocFile({});
    vec_bad[20] = static_cast<std::byte>(25);
    vec_bad[21] = std::byte{0};
    VocInfo info_bad;
    std::vector<std::byte> vec_pcm_bad;
    ORA_CHECK(!TryParseVoc(vec_bad, info_bad, vec_pcm_bad));
  }
  {
    auto vec_bad = MakeVocFile({});
    vec_bad[22] = static_cast<std::byte>(0x00);
    vec_bad[23] = std::byte{0x00};
    VocInfo info_bad;
    try {
      ora::fmt::LoadVoc(vec_bad, info_bad);
      ORA_CHECK(false);
    } catch (const std::runtime_error& ex) {
      ORA_CHECK(std::string_view{ex.what()} == "Voc header version 0 not supported");
    }
  }
  {

    auto vec_bad = MakeVocFile({});
    vec_bad[24] = std::byte{0};
    VocInfo info_bad;
    try {
      ora::fmt::LoadVoc(vec_bad, info_bad);
      ORA_CHECK(false);
    } catch (const std::runtime_error& ex) {

      ORA_CHECK(std::string_view{ex.what()} ==
                "Voc header id is bogus - expected: 111F but value is : 1100");
    }
  }

  {
    const auto vec_mixed = MakeVocFile({
        {1, BytesOf({0xD2, 0x00, 0x01})},
        {1, BytesOf({0xA5, 0x00, 0x02})},
    });
    VocInfo info_mixed;
    try {
      ora::fmt::LoadVoc(vec_mixed, info_mixed);
      ORA_CHECK(false);
    } catch (const std::runtime_error& ex) {
      ORA_CHECK(std::string_view{ex.what()} ==
                "Voc file contains chunks with different sample rate");
    }
  }

  {
    const auto vec_unused = MakeVocFile({{8, BytesOf({0x00, 0xCE, 0x00, 0x00})}});
    VocInfo info_unused;
    try {
      ora::fmt::LoadVoc(vec_unused, info_unused);
      ORA_CHECK(false);
    } catch (const std::runtime_error& ex) {
      ORA_CHECK(std::string_view{ex.what()} == "Unused block 8 in voc file");
    }
  }
}

void TestOggMp3Sniff() {
  using ora::fmt::IsMp3;

  ORA_CHECK(IsMp3(BytesOf({'I', 'D', '3', 0x00})));
  ORA_CHECK(IsMp3(BytesOf({0xFF, 0xFB, 0x90, 0x00})));
  ORA_CHECK(!IsMp3(BytesOf({0x00, 0x00, 0x00, 0x00})));
  ORA_CHECK(!IsMp3(BytesOf({0xFF})));

  ora::fmt::OggInfo info_ogg;
  std::vector<std::byte> vec_pcm;
  ORA_CHECK(!ora::fmt::TryParseOgg(BytesOf({'O', 'g', 'g', 'S', 0x00}), info_ogg, vec_pcm));
  ORA_CHECK(vec_pcm.empty());
  ora::fmt::Mp3Info info_mp3;
  ORA_CHECK(!ora::fmt::TryParseMp3(BytesOf({0x12, 0x34, 0x56, 0x78}), info_mp3, vec_pcm));
}

std::vector<std::byte> MakeR8Frame(std::uint8_t uint1_type, std::int32_t int4_w, std::int32_t int4_h,
                                   std::int32_t int4_x, std::int32_t int4_y,
                                   std::int32_t int4_palette_handle, std::uint8_t uint1_bpp,
                                   std::span<const std::byte> vec_pixels,
                                   std::span<const std::uint16_t> vec_palette_packed) {
  auto vec = std::vector<std::byte>{};
  vec.push_back(std::byte{uint1_type});
  const auto put_i32 = [&vec](std::int32_t int4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(int4_v >> (8 * int4_i)));
  };
  const auto put_u32 = [&vec](std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  put_i32(int4_w);
  put_i32(int4_h);
  put_i32(int4_x);
  put_i32(int4_y);
  put_u32(0x1234);
  put_i32(int4_palette_handle);
  vec.push_back(std::byte{uint1_bpp});
  vec.push_back(std::byte{8});
  vec.push_back(std::byte{8});
  vec.push_back(std::byte{0});
  vec.insert(vec.end(), vec_pixels.begin(), vec_pixels.end());
  if (uint1_type == 1 && int4_palette_handle != 0) {
    put_u32(0);
    put_u32(0);
    for (const std::uint16_t uint2_packed : vec_palette_packed) {
      vec.push_back(static_cast<std::byte>(uint2_packed & 0xFF));
      vec.push_back(static_cast<std::byte>(uint2_packed >> 8));
    }
  }
  return vec;
}

void TestR8AndPngSheet() {

  {
    auto vec_first_zero = std::vector<std::byte>(26, std::byte{0});
    ORA_CHECK(!ora::fmt::IsR8(vec_first_zero));
    auto vec_ok8 = std::vector<std::byte>(26, std::byte{0});
    vec_ok8[0] = std::byte{1};
    vec_ok8[25] = std::byte{8};
    ORA_CHECK(ora::fmt::IsR8(vec_ok8));
    auto vec_bad_bpp = vec_ok8;
    vec_bad_bpp[25] = std::byte{7};
    ORA_CHECK(!ora::fmt::IsR8(vec_bad_bpp));
  }

  const std::vector<std::byte> vec_pixels = BytesOf({0x00, 0x01, 0x02, 0xFF, 0x01, 0x00, 0xFF, 0x02});
  std::vector<std::uint16_t> vec_palette(256, 0);
  vec_palette[1] = 0x7C00 | 0x0063;
  vec_palette[2] = 0x03E0;
  auto vec_r8 = MakeR8Frame(1, 4, 2, 2, 1, 0x77, 8, vec_pixels, vec_palette);
  const std::vector<std::byte> vec_pixels2 = BytesOf({0x01, 0x02});
  const std::vector<std::uint16_t> vec_no_palette{};
  const auto vec_frame2 = MakeR8Frame(2, 2, 1, 1, 0, 0, 8, vec_pixels2, vec_no_palette);
  vec_r8.insert(vec_r8.end(), vec_frame2.begin(), vec_frame2.end());

  std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>> vec_frames;
  ORA_CHECK(ora::fmt::TryParseR8(vec_r8, vec_frames));
  ORA_CHECK(vec_frames.size() == 2);
  ORA_CHECK(vec_frames[0]->Type() == ora::gfx::SpriteFrameType::Bgra32);
  ORA_CHECK(vec_frames[0]->Size() == ora::int2(4, 2));
  ORA_CHECK(vec_frames[0]->FrameSize() == ora::int2(8, 8));

  ORA_CHECK(vec_frames[0]->Offset() == ora::core::Vector2(0.0f, 0.0f));

  const auto u32_at = [&vec_frames](std::size_t st_i) {
    const auto vec_d = vec_frames[0]->Data();
    std::uint32_t uint4_v = 0;
    for (auto int4_i = 3; int4_i >= 0; int4_i--)
      uint4_v = (uint4_v << 8) |
                static_cast<std::uint8_t>(vec_d[st_i * 4 + static_cast<std::size_t>(int4_i)]);
    return uint4_v;
  };
  ORA_CHECK(u32_at(0) == 0x00000000u);
  ORA_CHECK(u32_at(1) == (140u << 24));
  const std::uint32_t uint4_expanded2 = (0xFFu << 24) | ((0x03E0u & 0x7C00u) << 9) |
                                         ((0x03E0u & 0x3E0u) << 6) | ((0x03E0u & 0x1Fu) << 3);
  ORA_CHECK(u32_at(2) == uint4_expanded2);

  ORA_CHECK(vec_frames[1]->Type() == ora::gfx::SpriteFrameType::Bgra32);
  ORA_CHECK(vec_frames[1]->Size() == ora::int2(2, 1));
  ORA_CHECK(vec_frames[1]->Data().size() == 8);

  {
    const std::vector<std::byte> vec_rgba = BytesOf({0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF});
    const ora::fmt::Png png_src{vec_rgba, ora::gfx::SpriteFrameType::Rgba32, 2, 1};
    const std::vector<std::byte> vec_png_file = png_src.Save();
    std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>> vec_sheet_frames;
    ORA_CHECK(ora::fmt::TryParsePngSheet(vec_png_file, vec_sheet_frames));
    ORA_CHECK(vec_sheet_frames.size() == 1);
    ORA_CHECK(vec_sheet_frames[0]->Size() == ora::int2(2, 1));
    ORA_CHECK(vec_sheet_frames[0]->Data().size() == 8);
    ORA_CHECK(static_cast<std::uint8_t>(vec_sheet_frames[0]->Data()[0]) == 0x10);
    ORA_CHECK(!ora::fmt::TryParsePngSheet(BytesOf({0x89, 'P', 'N', 'G', 0x00}), vec_sheet_frames));
  }
  {

    std::vector<std::byte> vec_rgba;
    for (std::uint8_t uint1_v = 0; uint1_v < 8; uint1_v++)
      for (auto int4_c = 0; int4_c < 4; int4_c++)
        vec_rgba.push_back(static_cast<std::byte>(uint1_v * 16 + int4_c * 3));
    const ora::fmt::Png png_src{vec_rgba, ora::gfx::SpriteFrameType::Rgba32, 4, 2, std::nullopt,
                                {{"FrameSize", "2,2"}}};
    const std::vector<std::byte> vec_png_file = png_src.Save();
    std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>> vec_sheet_frames;
    ORA_CHECK(ora::fmt::TryParsePngSheet(vec_png_file, vec_sheet_frames));

    ORA_CHECK(vec_sheet_frames.size() == 2);
    ORA_CHECK(vec_sheet_frames[0]->Size() == ora::int2(2, 2));
    ORA_CHECK(vec_sheet_frames[1]->Size() == ora::int2(2, 2));

    ORA_CHECK(static_cast<std::uint8_t>(vec_sheet_frames[0]->Data()[0]) == 0x00);
    ORA_CHECK(static_cast<std::uint8_t>(vec_sheet_frames[1]->Data()[0]) == 0x20);
    ORA_CHECK(static_cast<std::uint8_t>(vec_sheet_frames[0]->Data()[8]) == 0x40);
  }
}

using SpriteTryFn = bool (*)(std::span<const std::byte>, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>&);

struct LoaderDef {
  std::string_view str_name;
  SpriteTryFn fn_try;
};

std::vector<LoaderDef> LoadersFor(std::string_view str_mod) {
  static constexpr LoaderDef kShpD2{"ShpD2Loader", ora::fmt::TryParseShpD2};
  static constexpr LoaderDef kShpTD{"ShpTDLoader", ora::fmt::TryParseShpTD};
  static constexpr LoaderDef kTmpRA{"TmpRALoader", ora::fmt::TryParseTmpRA};
  static constexpr LoaderDef kTmpTD{"TmpTDLoader", ora::fmt::TryParseTmpTD};
  static constexpr LoaderDef kShpTS{"ShpTSLoader", ora::fmt::TryParseShpTS};
  static constexpr LoaderDef kTmpTS{"TmpTSLoader", ora::fmt::TryParseTmpTS};

  if (str_mod == "ra") return {kShpD2, kShpTD, kTmpRA, kTmpTD, kShpTS};
  if (str_mod == "cnc") return {kShpTD, kTmpTD, kShpTS, kTmpRA};
  if (str_mod == "d2k") return {kShpTD};
  if (str_mod == "ts") return {kShpTS, kTmpTS, kShpTD};
  return {kShpD2, kShpTD, kTmpRA, kTmpTD, kShpTS, kTmpTS};
}

void TestVqaWsaVideo() {

  const auto make_vqa_head = [](const char* chr_magic4, std::uint32_t uint4_len, const char* chr_form4) {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{chr_magic4})
      vec.push_back(static_cast<std::byte>(chr_c));
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(uint4_len >> (8 * int4_i)));
    for (const char chr_c : std::string_view{chr_form4})
      vec.push_back(static_cast<std::byte>(chr_c));
    return vec;
  };
  ORA_CHECK(ora::fmt::IsWestwoodVqa(make_vqa_head("FORM", 100, "WVQA")));
  ORA_CHECK(!ora::fmt::IsWestwoodVqa(make_vqa_head("XORM", 100, "WVQA")));
  ORA_CHECK(!ora::fmt::IsWestwoodVqa(make_vqa_head("FORM", 0, "WVQA")));
  ORA_CHECK(!ora::fmt::IsWestwoodVqa(make_vqa_head("FORM", 100, "XVQA")));

  {
    auto b_threw = false;
    try {
      const auto video = ora::fmt::VqaVideo{make_vqa_head("XORM", 100, "WVQA"), false};
      (void)video;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "System.IO.InvalidDataException: Invalid vqa (invalid FORM section)";
    }
    ORA_CHECK(b_threw);
  }

  ORA_CHECK(!ora::fmt::IsWsa(MakeBytes({0x01, 0x00})));
  {

    auto vec = std::vector<std::byte>{};
    const auto push_u16 = [&vec](std::uint16_t uint2_v) {
      vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
      vec.push_back(static_cast<std::byte>(uint2_v >> 8));
    };
    const auto push_u32 = [&vec](std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
    };
    push_u16(2);
    push_u16(0);
    push_u16(0);
    push_u16(4);
    push_u16(4);
    push_u16(37);
    push_u16(0);
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      push_u32(30 + static_cast<std::uint32_t>(int4_i));
    push_u32(46);
    for (auto int4_i = 0; int4_i < 8; int4_i++)
      vec.push_back(std::byte{0});
    ORA_CHECK(!ora::fmt::IsWsa(vec));
  }

  {
    auto vec = std::vector<std::byte>{};
    const auto push_u16 = [&vec](std::uint16_t uint2_v) {
      vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
      vec.push_back(static_cast<std::byte>(uint2_v >> 8));
    };
    const auto push_u32 = [&vec](std::uint32_t uint4_v) {
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
    };
    push_u16(2);
    push_u16(0);
    push_u16(0);
    push_u16(2);
    push_u16(2);
    push_u16(37);
    push_u16(0);

    const auto vec_term_frame = ora::fmt::lcw::Encode(MakeBytes({0x80, 0x00, 0x00, 0x00}));
    push_u32(30);
    push_u32(30 + static_cast<std::uint32_t>(vec_term_frame.size()));
    push_u32(30 + 2 * static_cast<std::uint32_t>(vec_term_frame.size()));
    push_u32(30 + 2 * static_cast<std::uint32_t>(vec_term_frame.size()));
    for (auto int4_f = 0; int4_f < 2; int4_f++)
      vec.insert(vec.end(), vec_term_frame.begin(), vec_term_frame.end());
    ORA_CHECK(ora::fmt::IsWsa(vec));

    auto b_threw = false;
    try {
      const auto video = ora::fmt::WsaVideo{vec, false};
      (void)video;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.NullReferenceException: Object reference not set to an instance of an object.";
    }
    ORA_CHECK(b_threw);
  }
}

void TestVxlHvaIdx() {

  {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"Voxel AnimXXXXZ"})
      vec.push_back(static_cast<std::byte>(chr_c));
    vec.push_back(std::byte{0});
    auto b_threw = false;
    try {
      const auto vxl = ora::fmt::VxlReader{vec};
      (void)vxl;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "System.IO.InvalidDataException: Invalid vxl header";
    }
    ORA_CHECK(b_threw);
  }

  {
    const auto arr_identity = std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const auto arr_inv = ora::fmt::MatrixInverse(arr_identity);
    ORA_CHECK(arr_inv.has_value());
    ORA_CHECK(*arr_inv == arr_identity);
    ORA_CHECK(!ora::fmt::MatrixInverse(std::array<float, 16>{}).has_value());
  }

  {
    auto vec = std::vector<std::byte>{};
    vec.insert(vec.end(), 16, std::byte{0});
    vec.push_back(std::byte{1});
    vec.insert(vec.end(), 3, std::byte{0});
    vec.push_back(std::byte{1});
    vec.insert(vec.end(), 3, std::byte{0});
    vec.insert(vec.end(), 16, std::byte{0});
    vec.insert(vec.end(), 12 * 4, std::byte{0});
    auto b_threw = false;
    try {
      const auto hva = ora::fmt::HvaReader{vec, "zero.hva"};
      (void)hva;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.IO.InvalidDataException: The transformation matrix for HVA file `zero.hva` section 0 frame 0 "
                "is invalid because it is not invertible!";
    }
    ORA_CHECK(b_threw);
  }

  {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"GABX"})
      vec.push_back(static_cast<std::byte>(chr_c));
    auto b_threw = false;
    try {
      const auto idx = ora::fmt::IdxReader{vec};
      (void)idx;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.IO.InvalidDataException: Unable to load Idx file, did not find magic id, found GABX instead";
    }
    ORA_CHECK(b_threw);
  }
  {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"GABA"})
      vec.push_back(static_cast<std::byte>(chr_c));
    for (const std::uint8_t uint1_b : {3, 0, 0, 0})
      vec.push_back(static_cast<std::byte>(uint1_b));
    auto b_threw = false;
    try {
      const auto idx = ora::fmt::IdxReader{vec};
      (void)idx;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.IO.InvalidDataException: Unable to load Idx file, did not find magic number 2, found 3 "
                "instead";
    }
    ORA_CHECK(b_threw);
  }

  {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"GABA"})
      vec.push_back(static_cast<std::byte>(chr_c));
    for (const std::uint8_t uint1_b : {2, 0, 0, 0, 1, 0, 0, 0})
      vec.push_back(static_cast<std::byte>(uint1_b));
    for (const char chr_c : std::string_view{"abcdefghijklmnop"})
      vec.push_back(static_cast<std::byte>(chr_c));
    for (auto int4_i = 0; int4_i < 20; int4_i++)
      vec.push_back(std::byte{0});
    auto b_threw = false;
    try {
      const auto idx = ora::fmt::IdxReader{vec};
      (void)idx;
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "System.ArgumentOutOfRangeException: length ('-1') must be a non-negative value. (Parameter "
                "'length')\nActual value was -1.";
    }
    ORA_CHECK(b_threw);
  }

  {
    auto vec = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"GABA"})
      vec.push_back(static_cast<std::byte>(chr_c));
    for (const std::uint8_t uint1_b : {2, 0, 0, 0, 1, 0, 0, 0})
      vec.push_back(static_cast<std::byte>(uint1_b));
    vec.insert(vec.end(), 16, std::byte{0});
    const auto arr_tail = std::array<std::uint32_t, 5>{0xDEADBEEF, 0x1, 22050, 0x2A, 0x200};
    for (const auto uint4_v : arr_tail)
      for (auto int4_i = 0; int4_i < 4; int4_i++)
        vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));

    const auto idx = ora::fmt::IdxReader{vec};
    ORA_CHECK(idx.int4_sound_count == 1);
    ORA_CHECK(idx.vec_entries.size() == 1);
    const auto& entry = idx.vec_entries[0];
    auto str_expect = std::string{};
    for (auto int4_i = 0; int4_i < 16; int4_i++)
      str_expect += '\0';
    str_expect += ".wav";
    ORA_CHECK(entry.str_filename == str_expect);
    ORA_CHECK(entry.uint4_offset == 0xDEADBEEF);
    ORA_CHECK(entry.uint4_length == 1);
    ORA_CHECK(entry.uint4_sample_rate == 22050);
    ORA_CHECK(entry.uint4_flags == 0x2A);
    ORA_CHECK(entry.uint4_chunk_size == 0x200);
    auto str_tostring_expect = std::string{};
    for (auto int4_i = 0; int4_i < 16; int4_i++)
      str_tostring_expect += '\0';
    str_tostring_expect += ".wav - offset 0xdeadbeef - length 0x00000001";
    ORA_CHECK(entry.ToString() == str_tostring_expect);
  }
}

void AppendFrameLines(const std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>& vec_frames, std::string& str_out) {
  str_out += std::format("N {}\n", vec_frames.size());
  for (const auto& ptr_frame : vec_frames) {
    const auto& frame = *ptr_frame;
    const auto vec_data = frame.Data();
    str_out += std::format("T {} W {} H {} FW {} FH {} O {} {} C {} D {} X {}\n",
                           static_cast<int>(frame.Type()), frame.Size().X, frame.Size().Y, frame.FrameSize().X,
                           frame.FrameSize().Y, ora::meta::FormatFloatNet(frame.Offset().X),
                           ora::meta::FormatFloatNet(frame.Offset().Y), ora::fmt::CRC32::Calculate(vec_data),
                           vec_data.size(), BytesToHexLower(vec_data.first(std::min<std::size_t>(32, vec_data.size()))));
  }
}

void AppendTryLines(std::span<const std::byte> vec_file, std::string_view str_name, const LoaderDef& loader,
                    std::string& str_out) {
  str_out += std::format("S {}\n", str_name);
  auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
  if (loader.fn_try(vec_file, vec_frames)) {
    str_out += std::format("L {}\n", loader.str_name);
    AppendFrameLines(vec_frames, str_out);
  } else
    str_out += "L NONE\n";
}

void PutU32(std::span<std::byte> vec_file, std::size_t st_pos, std::uint32_t uint4_v) {
  for (auto int4_i = 0; int4_i < 4; int4_i++)
    vec_file[st_pos + static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(uint4_v >> (8 * int4_i));
}

std::vector<std::byte> MakeTileBytesTDRA(std::uint32_t uint4_idx) {
  auto vec_t = std::vector<std::byte>(16);
  for (auto int4_j = 0; int4_j < 16; int4_j++)
    vec_t[static_cast<std::size_t>(int4_j)] = static_cast<std::byte>(uint4_idx * 16 + int4_j);
  return vec_t;
}

std::vector<std::byte> MakeTmpTD() {
  auto vec_f = std::vector<std::byte>(64 + 3);
  vec_f[0] = std::byte{4};
  vec_f[2] = std::byte{4};
  PutU32(vec_f, 12, 32);
  PutU32(vec_f, 16, 0);
  PutU32(vec_f, 20, 0x0D1AFFFF);
  PutU32(vec_f, 24, 67);
  PutU32(vec_f, 28, 64);
  std::ranges::copy(MakeTileBytesTDRA(0), vec_f.begin() + 32);
  std::ranges::copy(MakeTileBytesTDRA(1), vec_f.begin() + 48);
  vec_f[64] = std::byte{0};
  vec_f[65] = std::byte{255};
  vec_f[66] = std::byte{1};
  return vec_f;
}

std::vector<std::byte> MakeTmpRA() {
  auto vec_f = std::vector<std::byte>(72 + 3);
  vec_f[0] = std::byte{4};
  vec_f[2] = std::byte{4};
  PutU32(vec_f, 16, 40);
  PutU32(vec_f, 20, 0);
  vec_f[26] = std::byte{0x73};
  vec_f[27] = std::byte{0x2C};
  PutU32(vec_f, 28, 75);
  PutU32(vec_f, 36, 72);
  std::ranges::copy(MakeTileBytesTDRA(0), vec_f.begin() + 40);
  std::ranges::copy(MakeTileBytesTDRA(1), vec_f.begin() + 56);
  vec_f[72] = std::byte{0};
  vec_f[73] = std::byte{255};
  vec_f[74] = std::byte{1};
  return vec_f;
}

std::vector<std::byte> MakeTmpTS() {
  constexpr std::size_t st_tile_len = 20 + 16 + 4 + 12 + 2 * 64;
  auto vec_f = std::vector<std::byte>(12 + 2 * st_tile_len);
  PutU32(vec_f, 0, 3);
  PutU32(vec_f, 4, 1);
  PutU32(vec_f, 8, 8);
  PutU32(vec_f, 12, 8);
  PutU32(vec_f, 16, 12);
  PutU32(vec_f, 20, 12 + st_tile_len);
  PutU32(vec_f, 24, 0);

  for (auto uint4_tile = 0u; uint4_tile < 2u; uint4_tile++) {
    const auto st_o = 12 + static_cast<std::size_t>(uint4_tile) * st_tile_len;
    PutU32(vec_f, st_o + 12, 8 * 8 / 2 + 52);
    PutU32(vec_f, st_o + 20, 0);
    PutU32(vec_f, st_o + 24, 0);
    PutU32(vec_f, st_o + 28, 0);
    PutU32(vec_f, st_o + 32, 0);
    PutU32(vec_f, st_o + 36, 0);
    auto st_p = st_o + 52;
    for (auto uint4_pass = 0u; uint4_pass < 2u; uint4_pass++) {
      auto int4_width = 4;
      for (auto int4_j = 0; int4_j < 8; int4_j++) {
        for (auto int4_i = 0; int4_i < int4_width; int4_i++)
          vec_f[st_p++] = static_cast<std::byte>(uint4_tile * 31 + uint4_pass * 200 + 16 * int4_j + int4_i);
        int4_width += (int4_j < 8 / 2 - 1 ? 1 : -1) * 4;
      }
    }
  }

  return vec_f;
}

std::vector<std::byte> ConcatBytes(std::initializer_list<std::span<const std::byte>> vec_parts) {
  auto vec_out = std::vector<std::byte>{};
  for (const auto vec_part : vec_parts)
    vec_out.insert(vec_out.end(), vec_part.begin(), vec_part.end());
  return vec_out;
}

std::array<std::byte, 4> AdlerOf(std::span<const std::byte> vec_data) {
  std::int64_t int8_a = 1, int8_b = 0;
  for (const auto byte_x : vec_data) {
    int8_a = (int8_a + std::to_integer<int>(byte_x)) % 65521;
    int8_b = (int8_b + int8_a) % 65521;
  }
  return {static_cast<std::byte>(int8_b >> 8), static_cast<std::byte>(int8_b),
          static_cast<std::byte>(int8_a >> 8), static_cast<std::byte>(int8_a)};
}

std::vector<std::byte> MakeZlibStored(std::span<const std::byte> vec_raw) {
  auto vec_out = std::vector<std::byte>{};
  const auto push_u16 = [&vec_out](std::uint16_t uint2_v) {
    vec_out.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec_out.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  vec_out.push_back(std::byte{0x78});
  vec_out.push_back(std::byte{0x01});
  std::size_t st_pos = 0;
  while (st_pos < vec_raw.size()) {
    const auto uint2_n = static_cast<std::uint16_t>(std::min<std::size_t>(65535, vec_raw.size() - st_pos));
    const auto b_fin = st_pos + uint2_n >= vec_raw.size();
    vec_out.push_back(static_cast<std::byte>(b_fin ? 1 : 0));
    push_u16(uint2_n);
    push_u16(static_cast<std::uint16_t>(~uint2_n));
    vec_out.insert(vec_out.end(), vec_raw.begin() + static_cast<std::ptrdiff_t>(st_pos),
                   vec_raw.begin() + static_cast<std::ptrdiff_t>(st_pos + uint2_n));
    st_pos += uint2_n;
  }
  const auto arr_adler = AdlerOf(vec_raw);
  vec_out.insert(vec_out.end(), arr_adler.begin(), arr_adler.end());
  return vec_out;
}

void AppendBe32(std::vector<std::byte>& vec, std::uint32_t uint4_v) {
  for (auto int4_i = 0; int4_i < 4; int4_i++)
    vec.push_back(static_cast<std::byte>(uint4_v >> (8 * (3 - int4_i))));
}

std::vector<std::byte> MakePngChunk(std::uint32_t uint4_type, std::span<const std::byte> vec_data) {
  auto vec_crc_input = std::vector<std::byte>{};
  AppendBe32(vec_crc_input, uint4_type);
  vec_crc_input.insert(vec_crc_input.end(), vec_data.begin(), vec_data.end());
  auto vec_out = std::vector<std::byte>{};
  AppendBe32(vec_out, static_cast<std::uint32_t>(vec_data.size()));
  AppendBe32(vec_out, uint4_type);
  vec_out.insert(vec_out.end(), vec_data.begin(), vec_data.end());
  AppendBe32(vec_out, ora::fmt::CRC32::Calculate(vec_crc_input));
  return vec_out;
}

std::byte PaethPred(std::byte a, std::byte b, std::byte c) {
  const auto int_a = std::to_integer<int>(a);
  const auto int_b = std::to_integer<int>(b);
  const auto int_c = std::to_integer<int>(c);
  const auto int4_p = int_a + int_b - int_c;
  const auto int4_pa = std::abs(int4_p - int_a);
  const auto int4_pb = std::abs(int4_p - int_b);
  const auto int4_pc = std::abs(int4_p - int_c);
  return (int4_pa <= int4_pb && int4_pa <= int4_pc) ? a : (int4_pb <= int4_pc) ? b : c;
}

std::vector<std::byte> EncodePngRow(std::span<const std::byte> vec_u, const std::byte* ptr_up, int int4_filter,
                                    int int4_px_stride) {
  auto vec_f = std::vector<std::byte>(vec_u.size());
  for (std::size_t st_i = 0; st_i < vec_u.size(); st_i++) {
    const auto int_l = st_i >= static_cast<std::size_t>(int4_px_stride)
                           ? std::to_integer<int>(vec_u[st_i - static_cast<std::size_t>(int4_px_stride)])
                           : 0;
    const auto int_up = ptr_up != nullptr ? std::to_integer<int>(ptr_up[st_i]) : 0;
    const auto int_ul = (ptr_up != nullptr && st_i >= static_cast<std::size_t>(int4_px_stride))
                            ? std::to_integer<int>(ptr_up[st_i - static_cast<std::size_t>(int4_px_stride)])
                            : 0;
    int int4_pred = 0;
    switch (int4_filter) {
      case 0: int4_pred = 0; break;
      case 1: int4_pred = int_l; break;
      case 2: int4_pred = int_up; break;
      case 3: int4_pred = (int_l + int_up) / 2; break;
      default: int4_pred = std::to_integer<int>(PaethPred(static_cast<std::byte>(int_l), static_cast<std::byte>(int_up), static_cast<std::byte>(int_ul))); break;
    }
    vec_f[st_i] = static_cast<std::byte>(std::to_integer<int>(vec_u[st_i]) - int4_pred);
  }
  return vec_f;
}

std::vector<std::byte> PngRow8(int int4_w, int int4_px_stride, int int4_y) {
  auto vec_r = std::vector<std::byte>(static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_px_stride));
  for (auto int4_x = 0; int4_x < int4_w; int4_x++)
    for (auto int4_c = 0; int4_c < int4_px_stride; int4_c++)
      vec_r[static_cast<std::size_t>(int4_x * int4_px_stride + int4_c)] =
          static_cast<std::byte>((int4_x * 31 + int4_y * 17 + int4_c * 7) % 251);
  return vec_r;
}

std::vector<std::vector<std::byte>> SplitIdat(std::span<const std::byte> vec_zlib,
                                              std::initializer_list<int> vec_after) {
  auto vec_parts = std::vector<std::vector<std::byte>>{};
  auto st_prev = std::size_t{0};
  for (const auto int4_cut : vec_after) {
    const auto st_cut = static_cast<std::size_t>(int4_cut);
    vec_parts.emplace_back(vec_zlib.begin() + static_cast<std::ptrdiff_t>(st_prev),
                           vec_zlib.begin() + static_cast<std::ptrdiff_t>(st_cut));
    st_prev = st_cut;
  }
  vec_parts.emplace_back(vec_zlib.begin() + static_cast<std::ptrdiff_t>(st_prev), vec_zlib.end());
  return vec_parts;
}

std::vector<std::byte> PngIhdrBytes(int int4_w, int int4_h, int int4_bit_depth, int int4_color_type) {
  auto vec = std::vector<std::byte>{};
  AppendBe32(vec, static_cast<std::uint32_t>(int4_w));
  AppendBe32(vec, static_cast<std::uint32_t>(int4_h));
  vec.push_back(static_cast<std::byte>(int4_bit_depth));
  vec.push_back(static_cast<std::byte>(int4_color_type));
  vec.push_back(std::byte{0});
  vec.push_back(std::byte{0});
  vec.push_back(std::byte{0});
  return vec;
}

std::vector<std::byte> AssemblePng(const std::vector<std::byte>& vec_ihdr, const std::vector<std::byte>* ptr_plte,
                                   const std::vector<std::byte>* ptr_trns, const std::vector<std::vector<std::byte>>& vec_idats,
                                   const std::vector<std::pair<std::string, std::string>>& vec_texts,
                                   bool b_unknown_before_idat) {
  const std::array<std::byte, 8> kPngSig = {std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
                                            std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A}};
  auto vec_png = ConcatBytes({std::span<const std::byte, 8>{kPngSig}, MakePngChunk(0x49484452, vec_ihdr)});
  if (ptr_plte != nullptr) vec_png = ConcatBytes({vec_png, MakePngChunk(0x504C5445, *ptr_plte)});
  if (ptr_trns != nullptr) vec_png = ConcatBytes({vec_png, MakePngChunk(0x74524E53, *ptr_trns)});
  if (b_unknown_before_idat)
    vec_png = ConcatBytes({vec_png, MakePngChunk(0x63524170, MakeBytes({1, 2, 3}))});
  for (const auto& vec_d : vec_idats) vec_png = ConcatBytes({vec_png, MakePngChunk(0x49444154, vec_d)});
  for (const auto& [str_k, str_v] : vec_texts) {
    auto vec_text = std::vector<std::byte>{};
    for (const char char_c : str_k) vec_text.push_back(static_cast<std::byte>(char_c));
    vec_text.push_back(std::byte{0});
    for (const char char_c : str_v) vec_text.push_back(static_cast<std::byte>(char_c));
    vec_png = ConcatBytes({vec_png, MakePngChunk(0x74455874, vec_text)});
  }
  return ConcatBytes({vec_png, MakePngChunk(0x49454E44, {})});
}

std::vector<std::byte> PngPackedRow(int int4_w, int int4_depth, int int4_y) {
  const auto int4_ppb = 8 / int4_depth;
  auto arr_src = std::vector<int>(static_cast<std::size_t>(int4_w));
  for (auto int4_x = 0; int4_x < int4_w; int4_x++)
    arr_src[static_cast<std::size_t>(int4_x)] = (int4_x * 3 + int4_y * 5) % (1 << int4_depth);
  const auto st_bytes = (static_cast<std::size_t>(int4_w) + static_cast<std::size_t>(int4_ppb) - 1) /
                        static_cast<std::size_t>(int4_ppb);
  auto vec_r = std::vector<std::byte>(st_bytes, std::byte{0});
  for (auto int4_i = 0; int4_i < int4_w; int4_i++) {
    const auto st_j = static_cast<std::size_t>(int4_i) / static_cast<std::size_t>(int4_ppb);
    const auto int4_k = static_cast<int>(static_cast<std::size_t>(int4_i) % static_cast<std::size_t>(int4_ppb));
    vec_r[st_j] |= static_cast<std::byte>(arr_src[static_cast<std::size_t>(int4_i)] << (8 - (int4_k + 1) * int4_depth));
  }
  return vec_r;
}

std::vector<std::byte> InflateZlib(std::span<const std::byte> vec_src) {
  auto uint8_dest_len = std::max<std::size_t>(1024, vec_src.size() * 8);
  while (true) {
    auto vec_dest = std::vector<std::byte>(uint8_dest_len);
    auto mz8_out_len = static_cast<mz_ulong>(uint8_dest_len);
    const auto int4_status = mz_uncompress(reinterpret_cast<unsigned char*>(vec_dest.data()), &mz8_out_len,
                                           reinterpret_cast<const unsigned char*>(vec_src.data()),
                                           static_cast<mz_ulong>(vec_src.size()));
    if (int4_status == MZ_OK) {
      vec_dest.resize(static_cast<std::size_t>(mz8_out_len));
      return vec_dest;
    }
    if (int4_status == MZ_BUF_ERROR) {
      uint8_dest_len *= 4;
      continue;
    }
    throw std::runtime_error("zlib inflate failed");
  }
}

std::vector<std::byte> TgaHeaderBytes(int int4_w, int int4_h, int int4_depth, int int4_image_type,
                                      int int4_descriptor) {
  return MakeBytes({0, 0, static_cast<std::uint8_t>(int4_image_type), 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    static_cast<std::uint8_t>(int4_w), static_cast<std::uint8_t>(int4_w >> 8),
                    static_cast<std::uint8_t>(int4_h), static_cast<std::uint8_t>(int4_h >> 8),
                    static_cast<std::uint8_t>(int4_depth), static_cast<std::uint8_t>(int4_descriptor)});
}

std::vector<std::byte> TgaRow(int int4_w, int int4_bpp, int int4_y) {
  auto vec_r = std::vector<std::byte>(static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_bpp));
  for (auto int4_x = 0; int4_x < int4_w; int4_x++)
    for (auto int4_c = 0; int4_c < int4_bpp; int4_c++)
      vec_r[static_cast<std::size_t>(int4_x * int4_bpp + int4_c)] =
          static_cast<std::byte>((int4_x * 13 + int4_y * 29 + int4_c * 3) % 253);
  return vec_r;
}

std::vector<std::byte> TgaUncompressed(int int4_w, int int4_h, int int4_depth, int int4_descriptor) {
  const auto int4_bpp = int4_depth / 8;
  auto vec_out = TgaHeaderBytes(int4_w, int4_h, int4_depth, 2, int4_descriptor);
  for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
    const auto vec_row = TgaRow(int4_w, int4_bpp, int4_y);
    vec_out.insert(vec_out.end(), vec_row.begin(), vec_row.end());
  }
  return vec_out;
}

std::vector<std::byte> RlePacket(int int4_n, std::initializer_list<std::uint8_t> vec_pixel) {
  auto vec_out = MakeBytes({static_cast<std::uint8_t>(128 | (int4_n - 1))});
  const auto vec_px = MakeBytes(vec_pixel);
  vec_out.insert(vec_out.end(), vec_px.begin(), vec_px.end());
  return vec_out;
}

std::vector<std::byte> RawPacket(int int4_count, std::span<const std::byte> vec_bytes) {
  auto vec_out = MakeBytes({static_cast<std::uint8_t>(int4_count - 1)});
  vec_out.insert(vec_out.end(), vec_bytes.begin(), vec_bytes.end());
  return vec_out;
}

std::vector<std::byte> DdsHeaderBytes(int int4_w, int int4_h, int int4_mips, std::uint32_t uint4_four_cc,
                                      std::uint32_t uint4_pf_flags, std::uint32_t uint4_rgb_count, std::uint32_t uint4_rm,
                                      std::uint32_t uint4_gm, std::uint32_t uint4_bm, std::uint32_t uint4_am) {
  const auto put_u32 = [](std::vector<std::byte>& vec, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };
  auto vec = std::vector<std::byte>{};
  put_u32(vec, 0x20534444);
  put_u32(vec, 124);
  put_u32(vec, 0x100F);
  put_u32(vec, static_cast<std::uint32_t>(int4_h));
  put_u32(vec, static_cast<std::uint32_t>(int4_w));
  put_u32(vec, 0);
  put_u32(vec, 0);
  put_u32(vec, static_cast<std::uint32_t>(int4_mips));
  for (auto int4_i = 0; int4_i < 11; int4_i++) put_u32(vec, 0);
  put_u32(vec, 32);
  put_u32(vec, uint4_pf_flags);
  put_u32(vec, uint4_four_cc);
  put_u32(vec, uint4_rgb_count);
  put_u32(vec, uint4_rm);
  put_u32(vec, uint4_gm);
  put_u32(vec, uint4_bm);
  put_u32(vec, uint4_am);
  put_u32(vec, 0x1000);
  put_u32(vec, 0);
  put_u32(vec, 0);
  put_u32(vec, 0);
  put_u32(vec, 0);
  return vec;
}

std::vector<std::byte> DdsPlainData(int int4_w, int int4_h, int int4_bpp) {
  auto vec_r = std::vector<std::byte>(static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_h) *
                                      static_cast<std::size_t>(int4_bpp));
  for (std::size_t st_i = 0; st_i < vec_r.size(); st_i++)
    vec_r[st_i] = static_cast<std::byte>((st_i * 17 + 3) % 251);
  return vec_r;
}

void U16Into(std::vector<std::byte>& vec, std::uint16_t uint2_v) {
  vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
  vec.push_back(static_cast<std::byte>(uint2_v >> 8));
}

std::vector<std::byte> Dxt1Block(std::uint16_t uint2_c0, std::uint16_t uint2_c1, std::uint8_t uint1_r0,
                                 std::uint8_t uint1_r1, std::uint8_t uint1_r2, std::uint8_t uint1_r3) {
  auto vec = std::vector<std::byte>{};
  U16Into(vec, uint2_c0);
  U16Into(vec, uint2_c1);
  for (const auto uint1_r : {uint1_r0, uint1_r1, uint1_r2, uint1_r3}) vec.push_back(static_cast<std::byte>(uint1_r));
  return vec;
}

std::vector<std::byte> Dxt3Block(std::span<const std::byte> vec_alpha8, std::uint16_t uint2_c0, std::uint16_t uint2_c1,
                                 std::uint8_t uint1_r0, std::uint8_t uint1_r1, std::uint8_t uint1_r2,
                                 std::uint8_t uint1_r3) {
  auto vec = std::vector<std::byte>{vec_alpha8.begin(), vec_alpha8.end()};
  U16Into(vec, uint2_c0);
  U16Into(vec, uint2_c1);
  for (const auto uint1_r : {uint1_r0, uint1_r1, uint1_r2, uint1_r3}) vec.push_back(static_cast<std::byte>(uint1_r));
  return vec;
}

std::vector<std::byte> Dxt5Block(std::uint8_t uint1_a0, std::uint8_t uint1_a1, std::span<const std::byte> vec_codes6,
                                 std::uint16_t uint2_c0, std::uint16_t uint2_c1, std::uint8_t uint1_r0,
                                 std::uint8_t uint1_r1, std::uint8_t uint1_r2, std::uint8_t uint1_r3) {
  auto vec = std::vector<std::byte>{};
  vec.push_back(static_cast<std::byte>(uint1_a0));
  vec.push_back(static_cast<std::byte>(uint1_a1));
  vec.insert(vec.end(), vec_codes6.begin(), vec_codes6.end());
  U16Into(vec, uint2_c0);
  U16Into(vec, uint2_c1);
  for (const auto uint1_r : {uint1_r0, uint1_r1, uint1_r2, uint1_r3}) vec.push_back(static_cast<std::byte>(uint1_r));
  return vec;
}

std::vector<std::byte> ZipBytes(const std::vector<std::pair<std::string, std::vector<std::byte>>>& vec_entries) {
  auto vec_ms = std::vector<std::byte>{};
  auto vec_offsets = std::vector<std::size_t>{};
  const auto put_u16 = [](std::vector<std::byte>& vec, std::uint16_t uint2_v) {
    vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  const auto put_u32l = [](std::vector<std::byte>& vec, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
  };

  for (const auto& [str_name, vec_data] : vec_entries) {
    vec_offsets.push_back(vec_ms.size());
    const auto uint4_crc = ora::fmt::CRC32::Calculate(vec_data);
    put_u32l(vec_ms, 0x04034B50);
    put_u16(vec_ms, 20);
    put_u16(vec_ms, 0);
    put_u16(vec_ms, 0);
    put_u16(vec_ms, 0);
    put_u16(vec_ms, 0x21);
    put_u32l(vec_ms, uint4_crc);
    put_u32l(vec_ms, static_cast<std::uint32_t>(vec_data.size()));
    put_u32l(vec_ms, static_cast<std::uint32_t>(vec_data.size()));
    put_u16(vec_ms, static_cast<std::uint16_t>(str_name.size()));
    put_u16(vec_ms, 0);
    for (const char char_c : str_name) vec_ms.push_back(static_cast<std::byte>(char_c));
    vec_ms.insert(vec_ms.end(), vec_data.begin(), vec_data.end());
  }

  const auto st_cd_start = vec_ms.size();
  auto vec_central = std::vector<std::byte>{};
  for (std::size_t st_i = 0; st_i < vec_entries.size(); st_i++) {
    const auto& [str_name, vec_data] = vec_entries[st_i];
    const auto uint4_crc = ora::fmt::CRC32::Calculate(vec_data);
    put_u32l(vec_central, 0x02014B50);
    put_u16(vec_central, 20);
    put_u16(vec_central, 20);
    put_u16(vec_central, 0);
    put_u16(vec_central, 0);
    put_u16(vec_central, 0);
    put_u16(vec_central, 0x21);
    put_u32l(vec_central, uint4_crc);
    put_u32l(vec_central, static_cast<std::uint32_t>(vec_data.size()));
    put_u32l(vec_central, static_cast<std::uint32_t>(vec_data.size()));
    put_u16(vec_central, static_cast<std::uint16_t>(str_name.size()));
    put_u16(vec_central, 0);
    put_u16(vec_central, 0);
    put_u16(vec_central, 0);
    put_u16(vec_central, 0);
    put_u32l(vec_central, 0);
    put_u32l(vec_central, static_cast<std::uint32_t>(vec_offsets[st_i]));
    for (const char char_c : str_name) vec_central.push_back(static_cast<std::byte>(char_c));
  }
  vec_ms.insert(vec_ms.end(), vec_central.begin(), vec_central.end());

  put_u32l(vec_ms, 0x06054B50);
  put_u16(vec_ms, 0);
  put_u16(vec_ms, 0);
  put_u16(vec_ms, static_cast<std::uint16_t>(vec_entries.size()));
  put_u16(vec_ms, static_cast<std::uint16_t>(vec_entries.size()));
  put_u32l(vec_ms, static_cast<std::uint32_t>(vec_central.size()));
  put_u32l(vec_ms, static_cast<std::uint32_t>(st_cd_start));
  put_u16(vec_ms, 0);
  return vec_ms;
}

void DumpPngSection(std::span<const std::byte> vec_file, std::string_view str_name, std::string& str_out) {
  str_out += std::format("G {}\n", str_name);
  const auto png = ora::fmt::Png{vec_file};
  str_out += std::format("GT {} {} {}\n", png.Width(), png.Height(), static_cast<int>(png.Type()));
  if (!png.Palette().has_value()) {
    str_out += "GP -\n";
  } else {
    str_out += std::format("GP {}", (*png.Palette()).size());
    for (const auto& color_c : *png.Palette())
      str_out += std::format(" {} {} {} {}", color_c.uint1_r, color_c.uint1_g, color_c.uint1_b, color_c.uint1_a);
    str_out += '\n';
  }
  str_out += std::format("GD {} {}\n", ora::fmt::CRC32::Calculate(png.Data()), png.Data().size());
  for (const auto& [str_k, str_v] : png.EmbeddedData())
    str_out += std::format("GE {} {}\n", str_k, str_v);

  const auto vec_saved = png.Save();
  auto vec_meta = std::vector<std::byte>{};
  auto vec_idat = std::vector<std::byte>{};
  std::size_t st_p = 8;
  while (st_p < vec_saved.size()) {
    const auto uint4_be = [&vec_saved](std::size_t st) {
      return (std::to_integer<std::uint32_t>(vec_saved[st]) << 24) |
             (std::to_integer<std::uint32_t>(vec_saved[st + 1]) << 16) |
             (std::to_integer<std::uint32_t>(vec_saved[st + 2]) << 8) |
             std::to_integer<std::uint32_t>(vec_saved[st + 3]);
    };
    const auto uint4_len = uint4_be(st_p);
    const auto uint4_ty = uint4_be(st_p + 4);
    vec_meta.insert(vec_meta.end(), vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 4),
                    vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8));
    if (uint4_ty == 0x49444154)
      vec_idat.insert(vec_idat.end(), vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8),
                      vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8 + uint4_len));
    else
      vec_meta.insert(vec_meta.end(), vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8),
                      vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8 + uint4_len));
    st_p += 12 + uint4_len;
  }
  const auto vec_inflated = InflateZlib(vec_idat);
  str_out += std::format("GS {} {}\n", ora::fmt::CRC32::Calculate(vec_meta), ora::fmt::CRC32::Calculate(vec_inflated));
}

void AppendTaggedTry(char chr_tag, std::string_view str_name, std::span<const std::byte> vec_file,
                     bool (*fn_try)(std::span<const std::byte>, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>&),
                     std::string_view str_loader_name, std::string& str_out) {
  str_out += std::format("{} {}\n", chr_tag, str_name);
  auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
  if (fn_try(vec_file, vec_frames)) {
    str_out += std::format("L {}\n", str_loader_name);
    AppendFrameLines(vec_frames, str_out);
  } else {
    str_out += "L NONE\n";
  }
}

bool ThrowsWith(std::span<const std::byte> vec_file, std::string_view str_msg,
                void (*fn_parse)(std::span<const std::byte>, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>&)) {
  try {
    auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
    fn_parse(vec_file, vec_frames);
  } catch (const std::runtime_error& ex) {
    if (ex.what() != str_msg)
      std::println(stderr, "throw mismatch: expected [{}] actual [{}]", str_msg, ex.what());
    return ex.what() == str_msg;
  }
  return false;
}

void ParsePngNoThrow(std::span<const std::byte> vec, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>&) {
  const auto png = ora::fmt::Png{vec};
  (void)png;
}

void ParseTgaNoThrow(std::span<const std::byte> vec, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>& vec_frames) {
  vec_frames.push_back(std::make_unique<ora::fmt::TgaFrame>(vec));
}

void ParseDdsNoThrow(std::span<const std::byte> vec, std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>& vec_frames) {
  vec_frames.push_back(std::make_unique<ora::fmt::DdsFrame>(vec));
}

void ParseRemasteredNoThrow(std::span<const std::byte> vec,
                            std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>&) {
  const auto sprite = ora::fmt::ShpRemasteredSprite{vec};
  (void)sprite;
}

const std::array<std::byte, 8> kPngSigBytes = {std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
                                               std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A}};

std::vector<std::byte> PngFileWithIhdr(int int4_w, int int4_h, int int4_bit_depth, int int4_color_type,
                                       int int4_compression, int int4_interlace) {
  auto vec_ihdr = std::vector<std::byte>{};
  AppendBe32(vec_ihdr, static_cast<std::uint32_t>(int4_w));
  AppendBe32(vec_ihdr, static_cast<std::uint32_t>(int4_h));
  vec_ihdr.push_back(static_cast<std::byte>(int4_bit_depth));
  vec_ihdr.push_back(static_cast<std::byte>(int4_color_type));
  vec_ihdr.push_back(static_cast<std::byte>(int4_compression));
  vec_ihdr.push_back(std::byte{0});
  vec_ihdr.push_back(static_cast<std::byte>(int4_interlace));
  return ConcatBytes({std::span<const std::byte, 8>{kPngSigBytes}, MakePngChunk(0x49484452, vec_ihdr)});
}

void TestPngPureLogic() {

  ORA_CHECK(ora::fmt::Png::Verify(PngFileWithIhdr(1, 1, 8, 2, 0, 0)));
  ORA_CHECK(!ora::fmt::Png::Verify(MakeBytes({0x89, 0x50})));

  auto vec_bad_sig = PngFileWithIhdr(1, 1, 8, 2, 0, 0);
  vec_bad_sig[0] = std::byte{0x00};
  ORA_CHECK(ThrowsWith(vec_bad_sig, "PNG Signature is bogus", ParsePngNoThrow));

  auto vec_text_first = ConcatBytes({std::span<const std::byte, 8>{kPngSigBytes},
                                     MakePngChunk(0x74455874, MakeBytes({'A', 0, 'B'}) )});
  ORA_CHECK(ThrowsWith(vec_text_first, "Invalid PNG file - header does not appear first.", ParsePngNoThrow));

  const auto vec_ihdr_once = PngFileWithIhdr(2, 2, 8, 2, 0, 0);
  auto vec_ihdr_twice = ConcatBytes({vec_ihdr_once, MakePngChunk(0x49484452, PngIhdrBytes(2, 2, 8, 2))});
  ORA_CHECK(ThrowsWith(vec_ihdr_twice, "Invalid PNG file - duplicate header.", ParsePngNoThrow));

  ORA_CHECK(ThrowsWith(PngFileWithIhdr(2, 2, 8, 2, 0, 1), "Interlacing not supported", ParsePngNoThrow));
  ORA_CHECK(ThrowsWith(PngFileWithIhdr(2, 2, 8, 2, 1, 0), "Compression method not supported", ParsePngNoThrow));
  ORA_CHECK(ThrowsWith(PngFileWithIhdr(2, 2, 16, 2, 0, 0), "Unknown pixel format", ParsePngNoThrow));

  auto vec_trunc = PngFileWithIhdr(2, 2, 8, 2, 0, 0);
  vec_trunc.resize(vec_trunc.size() - 4);
  ORA_CHECK(ThrowsWith(vec_trunc, "Invalid PNG file - no end chunk found.", ParsePngNoThrow));

  {
    const auto vec_row = PngRow8(2, 3, 0);
    auto vec_raw = std::vector<std::byte>{};
    vec_raw.push_back(std::byte{0});
    vec_raw.insert(vec_raw.end(), vec_row.begin(), vec_row.end());
    auto vec_file = ConcatBytes({PngFileWithIhdr(2, 1, 8, 2, 0, 0),
                                 MakePngChunk(0x49444154, MakeZlibStored(vec_raw)),
                                 MakePngChunk(0x63524170, MakeBytes({9}))});
    vec_file = ConcatBytes({vec_file, MakePngChunk(0x49444154, MakeZlibStored(vec_raw))});
    ORA_CHECK(ThrowsWith(vec_file, "Invalid PNG file - discontinuous IDAT chunks.", ParsePngNoThrow));
  }

  {
    auto vec_file = ConcatBytes({PngFileWithIhdr(2, 1, 8, 3, 0, 0), MakePngChunk(0x49454E44, {})});
    ORA_CHECK(ThrowsWith(vec_file, "Non-Palette indexed PNG are not supported.", ParsePngNoThrow));
  }

  {
    const auto vec_bgra = MakeBytes({0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88});
    const auto png = ora::fmt::Png{vec_bgra, ora::gfx::SpriteFrameType::Bgra32, 2, 1};
    ORA_CHECK(png.Type() == ora::gfx::SpriteFrameType::Bgra32);
    ORA_CHECK(png.PixelStride() == 4);
    ORA_CHECK(png.Data() == MakeBytes({0x33, 0x22, 0x11, 0x44, 0x77, 0x66, 0x55, 0x88}));

    auto b_threw = false;
    try {
      const auto png_bad = ora::fmt::Png{MakeBytes({1, 2, 3}), ora::gfx::SpriteFrameType::Rgba32, 2, 1};
      (void)png_bad;
    } catch (const std::runtime_error& ex) {
      b_threw = ex.what() == std::string_view{"Input data does not match expected length"};
    }
    ORA_CHECK(b_threw);
  }

  {
    constexpr int int4_w = 3, int4_h = 2;
    auto vec_data = std::vector<std::byte>(static_cast<std::size_t>(int4_w) * int4_h);
    for (std::size_t st_i = 0; st_i < vec_data.size(); st_i++)
      vec_data[st_i] = static_cast<std::byte>((st_i * 37 + 5) % 251);
    const std::vector<ora::fmt::PngColor> vec_palette = {ora::fmt::PngColor::FromRgb(1, 2, 3),
                                                         {4, 5, 6, 128}, {7, 8, 9, 255}};
    const std::vector<std::pair<std::string, std::string>> arr_embedded = {{"K1", "V1"}, {"K2", "V2"}};

    const auto png = ora::fmt::Png{vec_data, ora::gfx::SpriteFrameType::Indexed8, int4_w, int4_h, vec_palette,
                                   arr_embedded};
    const auto vec_saved = png.Save();
    const auto png2 = ora::fmt::Png{vec_saved};

    ORA_CHECK(png2.Width() == int4_w && png2.Height() == int4_h);
    ORA_CHECK(png2.Type() == ora::gfx::SpriteFrameType::Indexed8);
    ORA_CHECK(png2.Data() == vec_data);
    ORA_CHECK(png2.Palette().has_value() && *png2.Palette() == vec_palette);
    ORA_CHECK(png2.EmbeddedData() == arr_embedded);

    const std::uint32_t arr4_expect_types[] = {0x49484452, 0x504C5445, 0x74524E53, 0x49444154, 0x74455874,
                                               0x74455874, 0x49454E44};
    std::size_t st_p = 8, st_chunk = 0;
    auto vec_idat = std::vector<std::byte>{};
    while (st_p < vec_saved.size()) {
      const auto uint4_be = [&vec_saved](std::size_t st) {
        return (std::to_integer<std::uint32_t>(vec_saved[st]) << 24) |
               (std::to_integer<std::uint32_t>(vec_saved[st + 1]) << 16) |
               (std::to_integer<std::uint32_t>(vec_saved[st + 2]) << 8) |
               std::to_integer<std::uint32_t>(vec_saved[st + 3]);
      };
      const auto uint4_len = uint4_be(st_p);
      const auto uint4_ty = uint4_be(st_p + 4);
      ORA_CHECK(st_chunk < 7 && uint4_ty == arr4_expect_types[st_chunk]);

      auto vec_crc_in = std::vector<std::byte>{vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 4),
                                               vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8 + uint4_len)};
      ORA_CHECK(uint4_be(st_p + 8 + uint4_len) == ora::fmt::CRC32::Calculate(vec_crc_in));

      if (uint4_ty == 0x49444154)
        vec_idat.insert(vec_idat.end(), vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8),
                        vec_saved.begin() + static_cast<std::ptrdiff_t>(st_p + 8 + uint4_len));
      st_p += 12 + uint4_len;
      ++st_chunk;
    }
    ORA_CHECK(st_chunk == 7);

    auto vec_expect_stream = std::vector<std::byte>{};
    for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
      vec_expect_stream.push_back(std::byte{0});
      vec_expect_stream.insert(vec_expect_stream.end(),
                               vec_data.begin() + static_cast<std::ptrdiff_t>(int4_y * int4_w),
                               vec_data.begin() + static_cast<std::ptrdiff_t>((int4_y + 1) * int4_w));
    }
    ORA_CHECK(InflateZlib(vec_idat) == vec_expect_stream);
  }
}

void TestTgaDdsPureLogic() {

  auto vec_t2 = TgaHeaderBytes(4, 4, 24, 2, 0x00);
  auto vec_probe = vec_t2;
  ORA_CHECK(ora::fmt::IsTga(vec_probe));
  vec_probe[1] = std::byte{1};
  ORA_CHECK(!ora::fmt::IsTga(vec_probe));
  vec_probe = vec_t2;
  vec_probe[2] = std::byte{10};
  ORA_CHECK(!ora::fmt::IsTga(vec_probe));
  vec_probe = vec_t2;
  vec_probe[5] = std::byte{1};
  ORA_CHECK(!ora::fmt::IsTga(vec_probe));
  vec_probe = vec_t2;
  vec_probe[7] = std::byte{8};
  ORA_CHECK(!ora::fmt::IsTga(vec_probe));

  auto vec_16 = TgaHeaderBytes(2, 1, 16, 2, 0x00);
  vec_16.insert(vec_16.end(), 4, std::byte{0x5A});
  ORA_CHECK(ThrowsWith(vec_16, "Unhandled ImageFormat R5g5b5", ParseTgaNoThrow));

  auto vec_8 = TgaHeaderBytes(2, 1, 8, 2, 0x00);
  vec_8.insert(vec_8.end(), 2, std::byte{0x5A});
  ORA_CHECK(ThrowsWith(vec_8, "Unhandled ImageFormat Rgb8", ParseTgaNoThrow));

  auto vec_neg = TgaHeaderBytes(-1, 1, 24, 2, 0x00);
  ORA_CHECK(ThrowsWith(vec_neg, "Width must be greater than zero", ParseTgaNoThrow));

  const auto vec_dds = DdsHeaderBytes(4, 4, 1, 0, 0x40, 24, 1, 2, 3, 0);
  ORA_CHECK(ora::fmt::IsDds(vec_dds));

  auto vec_bad_magic = DdsHeaderBytes(4, 4, 1, 0, 0x40, 24, 1, 2, 3, 0);
  vec_bad_magic[0] = std::byte{0};
  ORA_CHECK(ThrowsWith(vec_bad_magic, "Not a valid DDS", ParseDdsNoThrow));

  auto vec_bad_hdr = DdsHeaderBytes(4, 4, 1, 0, 0x40, 24, 1, 2, 3, 0);
  vec_bad_hdr[4] = std::byte{123};
  ORA_CHECK(ThrowsWith(vec_bad_hdr, "Not a valid header size", ParseDdsNoThrow));

  auto vec_bad_pf = DdsHeaderBytes(4, 4, 1, 0, 0x40, 24, 1, 2, 3, 0);
  vec_bad_pf[76] = std::byte{31};
  ORA_CHECK(ThrowsWith(vec_bad_pf, "Expected pixel size to be 32, not: $31", ParseDdsNoThrow));

  const auto vec_dxt2 = ConcatBytes(
      {DdsHeaderBytes(4, 4, 1, 844388420, 0x4, 0, 0, 0, 0, 0), DdsPlainData(4, 4, 1)});
  ORA_CHECK(ThrowsWith(vec_dxt2, "Cannot support DXT2 or DXT4", ParseDdsNoThrow));

  const auto vec_bc5 = ConcatBytes(
      {DdsHeaderBytes(4, 4, 1, 1429553986, 0x4, 0, 0, 0, 0, 0), DdsPlainData(4, 4, 1)});
  ORA_CHECK(ThrowsWith(vec_bc5, "FourCC: 1429553986 not supported.", ParseDdsNoThrow));

  const auto vec_rgb8 = ConcatBytes(
      {DdsHeaderBytes(4, 4, 1, 0, 0x40, 8, 4, 2, 1, 0), DdsPlainData(4, 4, 1)});
  ORA_CHECK(ThrowsWith(vec_rgb8, "Unhandled ImageFormat Rgb8", ParseDdsNoThrow));

  const auto vec_rgb8_swapped = ConcatBytes(
      {DdsHeaderBytes(4, 4, 1, 0, 0x40, 8, 1, 2, 3, 0), DdsPlainData(4, 4, 1)});
  ORA_CHECK(ThrowsWith(vec_rgb8_swapped, "Do not know how to swap Rgb8", ParseDdsNoThrow));
}

void TestShpRemasteredPureLogic() {
  const auto vec_tga = TgaUncompressed(2, 2, 32, 0x20);

  const auto vec_mismatch = ZipBytes({{"a-0000.tga", vec_tga}, {"b-0001.tga", vec_tga}});
  ORA_CHECK(ThrowsWith(vec_mismatch, "Frame prefix mismatch: `b-` != `a-`", ParseRemasteredNoThrow));

  auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
  const auto vec_only_notes = ZipBytes({{"notes.txt", MakeBytes({'x'})}});
  ORA_CHECK(ora::fmt::IsShpRemastered(vec_only_notes));
  ORA_CHECK(ora::fmt::TryParseShpRemastered(vec_only_notes, vec_frames));
  ORA_CHECK(vec_frames.empty());

  auto vec_meta_bad = std::vector<std::byte>{};
  for (const char char_c : std::string_view{"{\"size\":[6,5]}"})
    vec_meta_bad.push_back(static_cast<std::byte>(char_c));
  const auto vec_bad_meta = ZipBytes({{"e1-0000.tga", vec_tga}, {"e1-0000.meta", vec_meta_bad}});
  ORA_CHECK(ThrowsWith(vec_bad_meta, "FormatException: failed to parse remastered meta", ParseRemasteredNoThrow));

  ORA_CHECK(!ora::fmt::IsShpRemastered(MakeBytes({0x50, 0x4B, 0x05, 0x06, 0x00})));
}

void TestEmbeddedSpritePalette() {
  using ora::fmt::EmbeddedSpritePalette;
  auto span_palette = std::span<const std::uint32_t>{};

  const auto vec_file = std::vector<std::uint32_t>{0xFF000001, 0xFF000002};
  const std::map<int, std::vector<std::uint32_t>> map_frames = {{1, {0xAA000001}}};

  const auto palette_both = EmbeddedSpritePalette{vec_file, map_frames};
  ORA_CHECK(palette_both.TryGetPaletteForFrame(1, span_palette) && span_palette.size() == 1 &&
            span_palette[0] == 0xAA000001u);
  ORA_CHECK(palette_both.TryGetPaletteForFrame(0, span_palette) && span_palette.size() == 2 &&
            span_palette[0] == 0xFF000001u);
  ORA_CHECK(palette_both.TryGetPaletteForFrame(9, span_palette) && span_palette.size() == 2);

  const auto palette_none = EmbeddedSpritePalette{};
  ORA_CHECK(!palette_none.TryGetPaletteForFrame(0, span_palette));
}

std::vector<std::byte> Pat(int int4_n, int int4_step, int int4_seed) {
  auto vec = std::vector<std::byte>{};
  vec.reserve(static_cast<std::size_t>(int4_n));
  for (auto int4_i = 0; int4_i < int4_n; int4_i++)
    vec.push_back(static_cast<std::byte>((int4_i * int4_step + int4_seed) & 0xFF));
  return vec;
}

std::vector<std::byte> MakeAudFile(std::uint16_t uint2_rate, std::uint8_t uint1_flags, std::uint8_t uint1_format,
                                   std::int32_t int4_output_size,
                                   std::vector<std::pair<std::vector<std::byte>, std::int32_t>> vec_chunks) {
  auto vec = std::vector<std::byte>{};
  const auto push_u16 = [&vec](std::uint16_t uint2_v) {
    vec.push_back(static_cast<std::byte>(uint2_v & 0xFF));
    vec.push_back(static_cast<std::byte>(uint2_v >> 8));
  };
  const auto push_i32 = [&vec](std::int32_t int4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(static_cast<std::uint32_t>(int4_v) >> (8 * int4_i)));
  };
  push_u16(uint2_rate);
  std::int32_t int4_data_size = 0;
  for (const auto& [vec_input, int4_chunk_out] : vec_chunks)
    int4_data_size += 8 + static_cast<std::int32_t>(vec_input.size());
  push_i32(int4_data_size);
  push_i32(int4_output_size);
  vec.push_back(static_cast<std::byte>(uint1_flags));
  vec.push_back(static_cast<std::byte>(uint1_format));
  for (const auto& [vec_input, int4_chunk_out] : vec_chunks) {
    push_u16(static_cast<std::uint16_t>(vec_input.size()));
    push_u16(static_cast<std::uint16_t>(int4_chunk_out));
    push_i32(0xDEAF);
    vec.insert(vec.end(), vec_input.begin(), vec_input.end());
  }
  return vec;
}

std::vector<std::byte> MakeWavFile(std::int16_t int2_format, std::int16_t int2_channels, std::int32_t int4_rate,
                                   std::int16_t int2_block_align, std::int16_t int2_bits, bool bool_fact,
                                   std::int32_t int4_fact_samples, std::int32_t int4_data_size_field,
                                   std::span<const std::byte> vec_data, std::span<const std::byte> vec_trailing) {
  auto vec = std::vector<std::byte>{};
  const auto push_tag = [&vec](std::string_view sv) {
    for (const char chr_c : sv)
      vec.push_back(static_cast<std::byte>(chr_c));
  };
  const auto push_i32 = [&vec](std::int32_t int4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec.push_back(static_cast<std::byte>(static_cast<std::uint32_t>(int4_v) >> (8 * int4_i)));
  };

  auto vec_fmt = std::vector<std::byte>{};
  const auto push_f16 = [&vec_fmt](std::int16_t int2_v) {
    vec_fmt.push_back(static_cast<std::byte>(int2_v & 0xFF));
    vec_fmt.push_back(static_cast<std::byte>(int2_v >> 8));
  };
  const auto push_f32 = [&vec_fmt](std::int32_t int4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_fmt.push_back(static_cast<std::byte>(static_cast<std::uint32_t>(int4_v) >> (8 * int4_i)));
  };
  push_f16(int2_format);
  push_f16(int2_channels);
  push_f32(int4_rate);
  push_f32(int4_rate * int2_channels * int2_bits / 8);
  push_f16(int2_block_align);
  push_f16(int2_bits);

  push_tag("RIFF");
  push_i32(static_cast<std::int32_t>(4 + 8 + vec_fmt.size() + (bool_fact ? 12 : 0) + 8 + vec_data.size() +
                                     vec_trailing.size()));
  push_tag("WAVE");
  push_tag("fmt ");
  push_i32(static_cast<std::int32_t>(vec_fmt.size()));
  vec.insert(vec.end(), vec_fmt.begin(), vec_fmt.end());
  if (bool_fact) {
    push_tag("fact");
    push_i32(4);
    push_i32(int4_fact_samples);
  }
  push_tag("data");
  push_i32(int4_data_size_field >= 0 ? int4_data_size_field : static_cast<std::int32_t>(vec_data.size()));
  vec.insert(vec.end(), vec_data.begin(), vec_data.end());
  vec.insert(vec.end(), vec_trailing.begin(), vec_trailing.end());
  return vec;
}

void AppendSoundLines(std::string_view sv_tag, std::string_view sv_name, std::span<const std::byte> vec_file,
                      std::string& str_out) {
  str_out += std::format("{} {}\n", sv_tag, sv_name);
  auto info_aud = ora::fmt::AudInfo{};
  auto info_wav = ora::fmt::WavInfo{};
  auto vec_pcm = std::vector<std::byte>{};
  const char* chr_hit = "NONE";
  if (ora::fmt::TryParseAud(vec_file, info_aud, vec_pcm))
    chr_hit = "AudLoader";
  else if (ora::fmt::TryParseWav(vec_file, info_wav, vec_pcm))
    chr_hit = "WavLoader";
  str_out += std::format("L {}\n", chr_hit);
  if (std::string_view{chr_hit} == "NONE")
    return;

  if (std::string_view{chr_hit} == "AudLoader")
    str_out += std::format("SD {} {} {} {}\n", info_aud.int4_channels, info_aud.int4_sample_bits,
                           info_aud.int4_sample_rate, ora::meta::FormatFloatNet(info_aud.fp4_length_in_seconds));
  else
    str_out += std::format("SD {} {} {} {}\n", info_wav.int2_channels, info_wav.int4_sample_bits,
                           info_wav.int4_sample_rate, ora::meta::FormatFloatNet(info_wav.fp4_length_in_seconds));
  str_out += std::format("SC {} {}\n", ora::fmt::CRC32::Calculate(vec_pcm), vec_pcm.size());
  str_out += std::format(
      "SX {}\n",
      BytesToHexLower(std::span<const std::byte>{vec_pcm}.first(std::min<std::size_t>(32, vec_pcm.size()))));
}

void VqPutTag(std::vector<std::byte>& vec, std::string_view sv) {
  for (const char chr_c : sv)
    vec.push_back(static_cast<std::byte>(chr_c));
}

void VqPutU16(std::vector<std::byte>& vec, int int4_v) {
  vec.push_back(static_cast<std::byte>(int4_v));
  vec.push_back(static_cast<std::byte>(int4_v >> 8));
}

void VqPutU32L(std::vector<std::byte>& vec, std::uint32_t uint4_v) {
  for (auto int4_i = 0; int4_i < 4; int4_i++)
    vec.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
}

void VqPutU32B(std::vector<std::byte>& vec, std::uint32_t uint4_v) {
  vec.push_back(static_cast<std::byte>(uint4_v >> 24));
  vec.push_back(static_cast<std::byte>(uint4_v >> 16));
  vec.push_back(static_cast<std::byte>(uint4_v >> 8));
  vec.push_back(static_cast<std::byte>(uint4_v));
}

void VqPutChunk(std::vector<std::byte>& vec, std::string_view sv_type, std::span<const std::byte> vec_payload) {
  VqPutTag(vec, sv_type);
  VqPutU32B(vec, static_cast<std::uint32_t>(vec_payload.size()));
  vec.insert(vec.end(), vec_payload.begin(), vec_payload.end());
  if (vec_payload.size() % 2 == 1)
    vec.push_back(std::byte{0});
}

std::vector<std::byte> MakeVqaFile(std::uint16_t uint2_flags, std::uint16_t uint2_frame_count,
                                   std::uint16_t uint2_width, std::uint16_t uint2_height, std::uint8_t uint1_bw,
                                   std::uint8_t uint1_bh, std::uint8_t uint1_parts, std::uint16_t uint2_sample_rate,
                                   std::uint8_t uint1_channels, std::uint8_t uint1_sample_bits,
                                   const std::vector<std::vector<std::byte>>& vec_frames) {
  auto vec = std::vector<std::byte>{};
  VqPutTag(vec, "FORM");
  VqPutU32L(vec, 0);
  VqPutTag(vec, "WVQA");
  VqPutTag(vec, "VQHD");
  VqPutU32L(vec, 42);
  VqPutU16(vec, 0);
  VqPutU16(vec, uint2_flags);
  VqPutU16(vec, uint2_frame_count);
  VqPutU16(vec, uint2_width);
  VqPutU16(vec, uint2_height);
  vec.push_back(static_cast<std::byte>(uint1_bw));
  vec.push_back(static_cast<std::byte>(uint1_bh));
  vec.push_back(std::byte{12});
  vec.push_back(static_cast<std::byte>(uint1_parts));
  VqPutU16(vec, 256);
  VqPutU16(vec, 0);
  VqPutU16(vec, 0);
  VqPutU32L(vec, 0);
  VqPutU16(vec, uint2_sample_rate);
  vec.push_back(static_cast<std::byte>(uint1_channels));
  vec.push_back(static_cast<std::byte>(uint1_sample_bits));
  VqPutU32L(vec, 0);
  VqPutU16(vec, 0);
  VqPutU32L(vec, 256000);
  VqPutU32L(vec, 0);

  VqPutTag(vec, "XXXF");
  VqPutU32B(vec, 4);
  vec.insert(vec.end(), 4, std::byte{0});

  VqPutTag(vec, "FINF");
  VqPutU16(vec, 0);
  VqPutU16(vec, 0);

  auto uint4_pos = static_cast<std::uint32_t>(vec.size() + 4 * vec_frames.size());
  for (const auto& vec_f : vec_frames) {
    VqPutU32L(vec, 0x40000000u | (uint4_pos >> 1));
    uint4_pos += static_cast<std::uint32_t>(vec_f.size());
  }

  for (const auto& vec_f : vec_frames)
    vec.insert(vec.end(), vec_f.begin(), vec_f.end());

  const auto uint4_len = static_cast<std::uint32_t>(vec.size() - 8);
  for (auto int4_i = 0; int4_i < 4; int4_i++)
    vec[static_cast<std::size_t>(4 + int4_i)] = static_cast<std::byte>(uint4_len >> (8 * int4_i));
  return vec;
}

std::vector<std::byte> VqSub(std::string_view sv_type, std::span<const std::byte> vec_data) {
  auto vec = std::vector<std::byte>{};
  VqPutChunk(vec, sv_type, vec_data);
  return vec;
}

std::vector<std::byte> VqCpl0(int int4_seed) {
  auto vec_p = std::vector<std::byte>(768, std::byte{0});
  for (auto int4_i = 0; int4_i < 768; int4_i++)
    vec_p[static_cast<std::size_t>(int4_i)] = static_cast<std::byte>((int4_i + int4_seed) & 0xFF);
  return VqSub("CPL0", vec_p);
}

std::vector<std::byte> VqVptz(std::span<const std::byte> vec_orig) {
  return VqSub("VPTZ", ora::fmt::lcw::Encode(vec_orig));
}

std::vector<std::byte> VqVptr(std::span<const std::byte> vec_orig) { return VqSub("VPTR", vec_orig); }

std::vector<std::byte> VqVprz(std::span<const std::byte> vec_orig) {
  return VqSub("VPRZ", ora::fmt::lcw::Encode(vec_orig));
}

std::vector<std::byte> LcwLiteralReversed(std::span<const std::byte> vec_data) {
  auto vec = std::vector<std::byte>{std::byte{0}};
  auto int4_off = 0;
  while (int4_off < static_cast<int>(vec_data.size())) {
    const auto int4_n = std::min(63, static_cast<int>(vec_data.size()) - int4_off);
    vec.push_back(static_cast<std::byte>(0x80 | int4_n));
    vec.insert(vec.end(), vec_data.begin() + int4_off, vec_data.begin() + int4_off + int4_n);
    int4_off += int4_n;
  }

  vec.push_back(std::byte{0x80});
  return vec;
}

std::vector<std::byte> NonHqOrig(int int4_bx, int int4_by, int int4_seed) {
  auto vec = std::vector<std::byte>(2 * static_cast<std::size_t>(int4_bx) * int4_by, std::byte{0});
  for (auto int4_y = 0; int4_y < int4_by; int4_y++)
    for (auto int4_x = 0; int4_x < int4_bx; int4_x++) {
      const auto st_i = static_cast<std::size_t>(int4_x + int4_y * int4_bx);
      vec[st_i] = static_cast<std::byte>((int4_x * 7 + int4_y * 3 + int4_seed) & 0xFF);
      vec[st_i + static_cast<std::size_t>(int4_bx * int4_by)] =
          (int4_x + int4_y * int4_seed) % 5 == 0 ? std::byte{0x0f} : static_cast<std::byte>(int4_y % 4);
    }

  return vec;
}

std::vector<std::byte> NonHqOrigSmall(int int4_bx, int int4_by, int int4_seed) {
  auto vec = std::vector<std::byte>(2 * static_cast<std::size_t>(int4_bx) * int4_by, std::byte{0});
  for (auto int4_y = 0; int4_y < int4_by; int4_y++)
    for (auto int4_x = 0; int4_x < int4_bx; int4_x++) {
      const auto st_i = static_cast<std::size_t>(int4_x + int4_y * int4_bx);
      vec[st_i] = static_cast<std::byte>((int4_x + int4_y + int4_seed) & 0xFF);
      vec[st_i + static_cast<std::size_t>(int4_bx * int4_by)] =
          (int4_x + int4_y + int4_seed) % 3 == 0 ? std::byte{0x0f} : std::byte{0};
    }

  return vec;
}

std::vector<std::byte> HqInstr(int int4_bx, int int4_by, int int4_seed) {
  auto vec = std::vector<std::byte>{};
  const auto ins = [&vec](int int4_val) {
    vec.push_back(static_cast<std::byte>(int4_val & 0xFF));
    vec.push_back(static_cast<std::byte>((int4_val >> 8) & 0xFF));
  };

  ins(0 << 13 | 10);
  ins(3 << 13 | (300 + int4_seed));
  ins(2 << 13 | ((77 + int4_seed) & 0xFF));
  vec.push_back(static_cast<std::byte>(41 + int4_seed));
  vec.push_back(static_cast<std::byte>(7 + int4_seed));
  ins(5 << 13 | (500 + int4_seed));
  vec.push_back(std::byte{5});
  ins(1 << 13 | (1 << 8) | ((90 + int4_seed) & 0xFF));
  ins(0 << 13 | 41);
  for (auto int4_y = 1; int4_y < int4_by; int4_y++) {
    ins(5 << 13 | (int4_y * int4_bx + int4_seed));
    vec.push_back(std::byte{32});
    ins(5 << 13 | (int4_y * int4_bx + 32 + int4_seed));
    vec.push_back(std::byte{32});
  }

  return vec;
}

std::vector<std::byte> VqFrame(std::span<const std::byte> vec_subchunks) {
  auto vec = std::vector<std::byte>{};
  VqPutChunk(vec, "VQFR", vec_subchunks);
  return vec;
}

void AppendVideoLines(std::string_view sv_name, std::span<const std::byte> vec_file, bool b_pad,
                      std::string& str_out) {
  str_out += std::format("VQ {} {}\n", sv_name, b_pad ? 1 : 0);
  const auto loader_vqa = ora::fmt::VqaLoader{};
  const auto loader_wsa = ora::fmt::WsaLoader{};
  const ora::fmt::IVideoLoader* const arr_loaders[] = {&loader_vqa, &loader_wsa};
  auto ptr_video = std::unique_ptr<ora::fmt::IVideo>{};
  const char* chr_hit = "NONE";
  for (const auto* ptr_loader : arr_loaders) {
    if (ptr_loader->TryParseVideo(vec_file, b_pad, ptr_video)) {
      chr_hit = ptr_loader == &loader_vqa ? "VqaLoader" : "WsaLoader";
      break;
    }
  }

  str_out += std::format("VL {}\n", chr_hit);
  if (!ptr_video)
    return;

  str_out += std::format("VV {} {} {} {} {} {} {} {}\n", ptr_video->FrameCount(), ptr_video->Framerate(),
                         ptr_video->Width(), ptr_video->Height(), ptr_video->HasAudio() ? 1 : 0,
                         ptr_video->AudioChannels(), ptr_video->SampleBits(), ptr_video->SampleRate());
  const auto vec_audio = ptr_video->AudioData();
  str_out += std::format("VA {} {}\n", vec_audio.empty() ? 0 : ora::fmt::CRC32::Calculate(vec_audio),
                         vec_audio.size());

  ptr_video->Reset();
  for (auto int4_i = 0; int4_i < ptr_video->FrameCount(); int4_i++) {
    if (int4_i > 0)
      ptr_video->AdvanceFrame();
    str_out += std::format("VF {} {}\n", int4_i, ora::fmt::CRC32::Calculate(ptr_video->CurrentFrameData()));
  }

  const auto vec_frame = ptr_video->CurrentFrameData();
  str_out += std::format(
      "VX {}\n", BytesToHexLower(vec_frame.first(std::min<std::size_t>(32, vec_frame.size()))));
}

std::string BuildActualText(const std::string& str_upstream_root) {
  auto str_out = std::string{};

  auto vec_shp = std::vector<std::string>{};
  for (const auto& entry : std::filesystem::recursive_directory_iterator{str_upstream_root + "/mods"}) {
    if (!entry.is_regular_file())
      continue;
    auto str_rel = entry.path().generic_string();
    if (!str_rel.ends_with(".shp"))
      continue;
    str_rel = str_rel.substr(str_upstream_root.size() + 1);
    vec_shp.push_back(str_rel);
  }
  std::ranges::sort(vec_shp);

  for (const auto& str_rel : vec_shp) {
    str_out += std::format("F {}\n", str_rel);
    const auto str_mod = str_rel.substr(5, str_rel.find('/', 5) - 5);
    const auto vec_file = ReadFileBytes(str_upstream_root + "/" + str_rel);

    auto b_hit = false;
    for (const auto& loader : LoadersFor(str_mod)) {
      auto vec_frames = std::vector<std::unique_ptr<ora::gfx::ISpriteFrame>>{};
      if (loader.fn_try(vec_file, vec_frames)) {
        str_out += std::format("L {}\n", loader.str_name);
        AppendFrameLines(vec_frames, str_out);
        b_hit = true;
        break;
      }
    }
    if (!b_hit)
      str_out += "L NONE\n";
  }

  auto vec_pal = std::vector<std::string>{};
  for (const auto& entry : std::filesystem::recursive_directory_iterator{str_upstream_root + "/mods"}) {
    if (!entry.is_regular_file())
      continue;
    auto str_rel = entry.path().generic_string();
    if (!str_rel.ends_with(".pal"))
      continue;
    str_rel = str_rel.substr(str_upstream_root.size() + 1);
    vec_pal.push_back(str_rel);
  }
  std::ranges::sort(vec_pal);

  for (const auto& str_rel : vec_pal) {
    str_out += std::format("F {}\n", str_rel);
    auto vec_file = ReadFileBytes(str_upstream_root + "/" + str_rel);
    ORA_CHECK(vec_file.size() >= 768);
    const std::int32_t arr_remap_transparent[]{0};
    const auto palette = ora::gfx::ImmutablePalette{std::span<const std::byte>{vec_file}.first(768),
                                                    arr_remap_transparent, std::span<const std::int32_t>{}};
    str_out += "P";
    for (auto int4_i = 0; int4_i < 256; int4_i++)
      str_out += std::format(" {}", palette.At(int4_i));
    str_out += '\n';
  }

  static constexpr LoaderDef kTmpTDDef{"TmpTDLoader", ora::fmt::TryParseTmpTD};
  static constexpr LoaderDef kTmpRADef{"TmpRALoader", ora::fmt::TryParseTmpRA};
  static constexpr LoaderDef kTmpTSDef{"TmpTSLoader", ora::fmt::TryParseTmpTS};
  AppendTryLines(MakeTmpTD(), "tmpTD_synthetic", kTmpTDDef, str_out);
  AppendTryLines(MakeTmpRA(), "tmpRA_synthetic", kTmpRADef, str_out);
  AppendTryLines(MakeTmpTS(), "tmpTS_synthetic", kTmpTSDef, str_out);

  const std::initializer_list<std::initializer_list<std::uint8_t>> vec_lzo_vectors = {
      {18, 'X', 0x55, 0x11, 0x00, 0x00},
      {22, 'a', 'b', 'c', 'd', 'e', 0x11, 0x00, 0x00},
      {0x11, 0x00, 0x00},
      {22, 'a', 'b', 'c', 'd', 'e', 0x20, 0x01, 0x00, 0x00, 0x11, 0x00, 0x00},
  };
  for (const auto& vec_v : vec_lzo_vectors) {
    auto vec_src = std::vector<std::byte>{};
    for (const auto uint1_v : vec_v)
      vec_src.push_back(static_cast<std::byte>(uint1_v));
    str_out += std::format("Z {}\n", BytesToHexLower(vec_src));

    auto vec_dest = std::vector<std::byte>(65536);
    auto uint4_out_len = std::uint32_t{0};
    const auto int4_ret = ora::fmt::lzo::DecodeInto(vec_src, 0, vec_src.size(), vec_dest, 0, uint4_out_len);
    str_out += std::format("ZR {} {} {}\n", int4_ret, uint4_out_len,
                           BytesToHexLower(std::span<const std::byte>{vec_dest}.first(uint4_out_len)));
  }

  auto vec_lcw_src = std::vector<std::byte>(96);
  for (auto int4_i = 0; int4_i < 96; int4_i++)
    vec_lcw_src[static_cast<std::size_t>(int4_i)] =
        static_cast<std::byte>(int4_i < 24 ? int4_i * 3 : int4_i < 48 ? 0xAB : int4_i - 48);
  str_out += std::format("E {}\n", BytesToHexLower(vec_lcw_src));
  str_out += std::format("ER {}\n", BytesToHexLower(ora::fmt::lcw::Encode(vec_lcw_src)));

  {

    constexpr int int4_w = 7, int4_h = 5;
    auto vec_raw = std::vector<std::byte>{};
    const auto ptr_prev_of = [](std::vector<std::vector<std::byte>>& vec_rows, int int4_y) -> const std::byte* {
      return int4_y > 0 ? vec_rows[static_cast<std::size_t>(int4_y - 1)].data() : nullptr;
    };
    auto vec_rows = std::vector<std::vector<std::byte>>{};
    const int arr4_filters[] = {0, 1, 2, 3, 4};
    for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
      vec_rows.push_back(PngRow8(int4_w, 1, int4_y));
      vec_raw.push_back(static_cast<std::byte>(arr4_filters[int4_y]));
      const auto vec_f = EncodePngRow(vec_rows[static_cast<std::size_t>(int4_y)], ptr_prev_of(vec_rows, int4_y),
                                       arr4_filters[int4_y], 1);
      vec_raw.insert(vec_raw.end(), vec_f.begin(), vec_f.end());
    }
    const auto vec_zlib = MakeZlibStored(vec_raw);
    const auto vec_plte = MakeBytes({10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120});
    const auto vec_trns = MakeBytes({0, 255});
    const std::vector<std::pair<std::string, std::string>> arr_texts = {{"Title", "oracle-png"}, {"Software", "OpenRA"},
                                                                        {"Title", "updated"}};
    DumpPngSection(AssemblePng(PngIhdrBytes(int4_w, int4_h, 8, 3), &vec_plte, &vec_trns,
                               SplitIdat(vec_zlib, {5}), arr_texts, false),
                   "png_idx8_7x5", str_out);
  }
  {
    const auto dump_packed = [&str_out](int int4_w, int int4_h, int int4_depth, std::string_view str_name) {
      auto vec_raw = std::vector<std::byte>{};
      for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
        vec_raw.push_back(std::byte{0});
        const auto vec_row = PngPackedRow(int4_w, int4_depth, int4_y);
        vec_raw.insert(vec_raw.end(), vec_row.begin(), vec_row.end());
      }
      auto vec_plte = std::vector<std::byte>(3 * (1 << int4_depth));
      for (std::size_t st_i = 0; st_i < vec_plte.size(); st_i++)
        vec_plte[st_i] = static_cast<std::byte>(st_i * 7 % 251);
      DumpPngSection(AssemblePng(PngIhdrBytes(int4_w, int4_h, int4_depth, 3), &vec_plte, nullptr,
                                 {MakeZlibStored(vec_raw)}, {}, false),
                     str_name, str_out);
    };
    dump_packed(5, 3, 4, "png_idx4_5x3");
    dump_packed(3, 2, 2, "png_idx2_3x2");
    dump_packed(9, 2, 1, "png_idx1_9x2");
  }
  {
    constexpr int int4_w = 4, int4_h = 3;
    auto vec_raw = std::vector<std::byte>{};
    auto vec_rows = std::vector<std::vector<std::byte>>{};
    const int arr4_filters[] = {4, 3, 0};
    for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
      vec_rows.push_back(PngRow8(int4_w, 3, int4_y));
      vec_raw.push_back(static_cast<std::byte>(arr4_filters[int4_y]));
      const auto ptr_up = int4_y > 0 ? vec_rows[static_cast<std::size_t>(int4_y - 1)].data() : nullptr;
      const auto vec_f = EncodePngRow(vec_rows[static_cast<std::size_t>(int4_y)], ptr_up, arr4_filters[int4_y], 3);
      vec_raw.insert(vec_raw.end(), vec_f.begin(), vec_f.end());
    }
    DumpPngSection(AssemblePng(PngIhdrBytes(int4_w, int4_h, 8, 2), nullptr, nullptr, {MakeZlibStored(vec_raw)}, {},
                               true),
                   "png_rgb24_4x3", str_out);
  }
  {
    constexpr int int4_w = 3, int4_h = 2;
    auto vec_raw = std::vector<std::byte>{};
    auto vec_rows = std::vector<std::vector<std::byte>>{};
    const int arr4_filters[] = {1, 4};
    for (auto int4_y = 0; int4_y < int4_h; int4_y++) {
      vec_rows.push_back(PngRow8(int4_w, 4, int4_y));
      vec_raw.push_back(static_cast<std::byte>(arr4_filters[int4_y]));
      const auto ptr_up = int4_y > 0 ? vec_rows[static_cast<std::size_t>(int4_y - 1)].data() : nullptr;
      const auto vec_f = EncodePngRow(vec_rows[static_cast<std::size_t>(int4_y)], ptr_up, arr4_filters[int4_y], 4);
      vec_raw.insert(vec_raw.end(), vec_f.begin(), vec_f.end());
    }
    DumpPngSection(AssemblePng(PngIhdrBytes(int4_w, int4_h, 8, 6), nullptr, nullptr, {MakeZlibStored(vec_raw)},
                               {std::pair<std::string, std::string>{"Note", "rgba"}}, false),
                   "png_rgba32_3x2", str_out);
  }

  AppendTaggedTry('H', "tga_t2_24_bl", TgaUncompressed(5, 4, 24, 0x00), ora::fmt::TryParseTga, "TgaLoader", str_out);
  AppendTaggedTry('H', "tga_t2_24_br", TgaUncompressed(5, 2, 24, 0x10), ora::fmt::TryParseTga, "TgaLoader", str_out);
  AppendTaggedTry('H', "tga_t2_32_tl", TgaUncompressed(4, 3, 32, 0x20), ora::fmt::TryParseTga, "TgaLoader", str_out);
  {
    auto vec_tga = ConcatBytes({TgaHeaderBytes(6, 3, 32, 10, 0x20),
                                RlePacket(3, {10, 11, 12, 13}),
                                RawPacket(3, TgaRow(3, 4, 0)),
                                RawPacket(6, TgaRow(6, 4, 1)),
                                RlePacket(2, {20, 21, 22, 23}),
                                RlePacket(2, {30, 31, 32, 33}),
                                RawPacket(2, TgaRow(2, 4, 2))});
    str_out += "H tga_t10_32_tl\nL TgaSprite\n";
    AppendFrameLines(ora::fmt::TgaSprite{vec_tga}.Frames(), str_out);
  }
  {
    auto vec_tga = ConcatBytes({TgaHeaderBytes(5, 3, 24, 10, 0x00),
                                RlePacket(4, {1, 2, 3}),
                                RawPacket(1, TgaRow(1, 3, 0)),
                                RawPacket(2, TgaRow(2, 3, 1)),
                                RlePacket(3, {4, 5, 6}),
                                RlePacket(1, {7, 8, 9}),
                                RlePacket(2, {10, 11, 12}),
                                RawPacket(2, TgaRow(2, 3, 2))});
    str_out += "H tga_t10_24_bl\nL TgaSprite\n";
    AppendFrameLines(ora::fmt::TgaSprite{vec_tga}.Frames(), str_out);
  }

  AppendTaggedTry('J', "dds_unc32_5x4",
                  ConcatBytes({DdsHeaderBytes(5, 4, 1, 0, 0x41, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000),
                               DdsPlainData(5, 4, 4)}),
                  ora::fmt::TryParseDds, "DdsLoader", str_out);
  AppendTaggedTry('J', "dds_unc24_3x3_swapped",
                  ConcatBytes({DdsHeaderBytes(3, 3, 1, 0, 0x40, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0),
                               DdsPlainData(3, 3, 3)}),
                  ora::fmt::TryParseDds, "DdsLoader", str_out);
  AppendTaggedTry('J', "dds_unc32_mips_4x4",
                  ConcatBytes({DdsHeaderBytes(4, 4, 3, 0, 0x41, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000),
                               DdsPlainData(4, 4, 4), DdsPlainData(2, 2, 4), DdsPlainData(1, 1, 4)}),
                  ora::fmt::TryParseDds, "DdsLoader", str_out);
  {
    auto vec_dxt1 = ConcatBytes({DdsHeaderBytes(5, 5, 1, 827611204, 0x4, 0, 0, 0, 0, 0),
                                 Dxt1Block(0xF800, 0x07E0, 0x1B, 0xE4, 0x00, 0xFF),
                                 Dxt1Block(0x07E0, 0xF800, 0xE4, 0x1B, 0xFF, 0x00),
                                 Dxt1Block(0x001F, 0x0000, 0x55, 0xAA, 0x55, 0xAA),
                                 Dxt1Block(0xFFFF, 0x0000, 0xC3, 0x3C, 0x81, 0x7E)});
    AppendTaggedTry('J', "dds_dxt1_5x5", vec_dxt1, ora::fmt::TryParseDds, "DdsLoader", str_out);
  }
  {
    auto vec_dxt3 = ConcatBytes({DdsHeaderBytes(4, 4, 1, 861165636, 0x4, 0, 0, 0, 0, 0),
                                 Dxt3Block(MakeBytes({0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}), 0xF800,
                                           0x001F, 0x1B, 0xE4, 0x00, 0xFF)});
    AppendTaggedTry('J', "dds_dxt3_4x4", vec_dxt3, ora::fmt::TryParseDds, "DdsLoader", str_out);
  }
  {
    auto vec_dxt5 = ConcatBytes({DdsHeaderBytes(4, 4, 1, 894720068, 0x4, 0, 0, 0, 0, 0),
                                 Dxt5Block(255, 0, MakeBytes({0x00, 0x11, 0x22, 0x33, 0x44, 0x55}), 0xF800, 0x07E0,
                                           0xE4, 0x1B, 0x00, 0xFF)});
    AppendTaggedTry('J', "dds_dxt5_4x4", vec_dxt5, ora::fmt::TryParseDds, "DdsLoader", str_out);
  }
  {
    auto vec_dxt5b = ConcatBytes({DdsHeaderBytes(4, 4, 1, 894720068, 0x4, 0, 0, 0, 0, 0),
                                  Dxt5Block(10, 250, MakeBytes({0x00, 0x92, 0x24, 0x49, 0xDB, 0xFF}), 0x07E0, 0xF800,
                                            0x1B, 0xE4, 0xFF, 0x00)});
    AppendTaggedTry('J', "dds_dxt5b_4x4", vec_dxt5b, ora::fmt::TryParseDds, "DdsLoader", str_out);
  }

  {
    auto vec_tga0 = TgaUncompressed(4, 4, 32, 0x20);
    auto vec_meta0 = std::vector<std::byte>{};
    for (const char char_c : std::string_view{"{\"size\":[6,5],\"crop\":[1,2,4,5]}"})
      vec_meta0.push_back(static_cast<std::byte>(char_c));
    auto vec_tga2 = TgaUncompressed(3, 3, 24, 0x00);
    auto vec_notes = std::vector<std::byte>{};
    for (const char char_c : std::string_view{"ignored"})
      vec_notes.push_back(static_cast<std::byte>(char_c));
    const auto vec_zip = ZipBytes({{"e1-0000.tga", vec_tga0}, {"e1-0000.meta", vec_meta0},
                                   {"e1-0002.tga", vec_tga2}, {"notes.txt", vec_notes}});
    AppendTaggedTry('K', "remastered_synthetic", vec_zip, ora::fmt::TryParseShpRemastered, "ShpRemasteredLoader",
                    str_out);
  }

  {

    const std::vector<std::string> vec_hash_names = {
        "a", "ab", "abc", "abcd", "abcde", "test", "DATA.INI", "data.bin",
        "IMAGE.SHP", "local mix database.dat", "GDI.SHP", "CONQUER.MIX",
        "rules.yaml",
        "averylongfilename_that-exceeds.sixtyfour.characters_0123456789.dat"};
    for (const std::string& str_name : vec_hash_names)
      str_out += std::format("Q {} {} {}\n", str_name,
                             ora::fs::PackageEntry::HashFilename(str_name, ora::fs::PackageHashType::Classic),
                             ora::fs::PackageEntry::HashFilename(str_name, ora::fs::PackageHashType::CRC32));

    auto arr_keyblock = std::array<std::byte, 80>{};
    for (auto int4_i = 0; int4_i < 80; int4_i++)
      arr_keyblock[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 7 + 1) & 0xFF);
    ora::fmt::BlowfishKeyProvider provider{};
    const auto arr_bf_key = provider.DecryptKey(arr_keyblock);
    str_out += std::format("KB {}\n", BytesToHexLower(std::as_bytes(std::span{arr_bf_key})));

    const ora::fmt::Blowfish fish{std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(arr_bf_key.data()), arr_bf_key.size()}};
    auto vec_plain = std::vector<std::byte>(16);
    for (auto int4_i = 0; int4_i < 16; int4_i++)
      vec_plain[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 11 + 3) & 0xFF);
    auto vec_words = std::vector<std::uint32_t>(4);
    std::memcpy(vec_words.data(), vec_plain.data(), 16);
    const auto vec_cipher = fish.Encrypt(vec_words);
    auto vec_cipher_bytes = std::vector<std::byte>(16);
    std::memcpy(vec_cipher_bytes.data(), vec_cipher.data(), 16);
    str_out += std::format("BE {}\n", BytesToHexLower(vec_cipher_bytes));

    const std::vector<std::string> vec_local_db_names = {"IMAGE.SHP", "data.bin",
                                                          "local mix database.dat", "not.present"};
    const std::vector<std::string> vec_entry_names = {"data.bin", "IMAGE.SHP",
                                                       "local mix database.dat", "unknown.file"};
    std::vector<std::vector<std::byte>> vec_entry_contents;
    {
      auto vec_c0 = std::vector<std::byte>(17);
      for (auto int4_i = 0; int4_i < 17; int4_i++)
        vec_c0[static_cast<std::size_t>(int4_i)] =
            static_cast<std::byte>((int4_i * 13 + 5) % 251);
      vec_entry_contents.push_back(std::move(vec_c0));
      auto vec_c1 = std::vector<std::byte>(24);
      for (auto int4_i = 0; int4_i < 24; int4_i++)
        vec_c1[static_cast<std::size_t>(int4_i)] =
            static_cast<std::byte>((int4_i * 29 + 7) % 251);
      vec_entry_contents.push_back(std::move(vec_c1));
      vec_entry_contents.push_back(ora::fmt::XccLocalDatabase::MakeData(vec_local_db_names));
      auto vec_c3 = std::vector<std::byte>(8);
      for (auto int4_i = 0; int4_i < 8; int4_i++)
        vec_c3[static_cast<std::size_t>(int4_i)] = static_cast<std::byte>(200 + int4_i);
      vec_entry_contents.push_back(std::move(vec_c3));
    }
    const std::vector<std::string> vec_globals = {"IMAGE.SHP", "data.bin", "not.present",
                                                   "UNRELATED.TXT"};

    const auto make_mix_bytes = [&](ora::fs::PackageHashType enum_hash,
                                     std::string_view sv_variant) {
      auto vec_header = std::vector<std::byte>{};
      const auto push_u16 = [&vec_header](std::uint16_t uint2_v) {
        vec_header.push_back(static_cast<std::byte>(uint2_v & 0xFF));
        vec_header.push_back(static_cast<std::byte>(uint2_v >> 8));
      };
      const auto push_u32 = [&vec_header](std::uint32_t uint4_v) {
        for (auto int4_i = 0; int4_i < 4; int4_i++)
          vec_header.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
      };
      std::uint32_t uint4_size = 0;
      for (const auto& vec_c : vec_entry_contents)
        uint4_size += static_cast<std::uint32_t>(vec_c.size());
      push_u16(static_cast<std::uint16_t>(vec_entry_names.size()));
      push_u32(uint4_size);
      std::uint32_t uint4_off = 0;
      for (std::size_t st_i = 0; st_i < vec_entry_names.size(); st_i++) {
        push_u32(ora::fs::PackageEntry::HashFilename(vec_entry_names[st_i], enum_hash));
        push_u32(uint4_off);
        push_u32(static_cast<std::uint32_t>(vec_entry_contents[st_i].size()));
        uint4_off += static_cast<std::uint32_t>(vec_entry_contents[st_i].size());
      }

      auto vec_out = std::vector<std::byte>{};
      if (sv_variant == "cnc") {
        vec_out = vec_header;
      } else if (sv_variant == "ra") {
        vec_out.push_back(std::byte{0});
        vec_out.push_back(std::byte{0});
        vec_out.push_back(std::byte{0});
        vec_out.push_back(std::byte{0});
        vec_out.insert(vec_out.end(), vec_header.begin(), vec_header.end());
      } else {

        const auto int4_blocks =
            static_cast<int>((13 + static_cast<long long>(vec_entry_names.size()) * 12) / 8);
        auto vec_padded = std::vector<std::byte>(static_cast<std::size_t>(int4_blocks) * 8, std::byte{0});
        std::ranges::copy(vec_header, vec_padded.begin());
        auto vec_words_h = std::vector<std::uint32_t>(vec_padded.size() / 4);
        std::memcpy(vec_words_h.data(), vec_padded.data(), vec_padded.size());
        const auto vec_encrypted = fish.Encrypt(vec_words_h);
        auto vec_enc_bytes = std::vector<std::byte>(vec_encrypted.size() * 4);
        std::memcpy(vec_enc_bytes.data(), vec_encrypted.data(), vec_enc_bytes.size());

        vec_out.push_back(std::byte{0});
        vec_out.push_back(std::byte{0});
        vec_out.push_back(std::byte{0x02});
        vec_out.push_back(std::byte{0x00});
        vec_out.insert(vec_out.end(), arr_keyblock.begin(), arr_keyblock.end());
        vec_out.insert(vec_out.end(), vec_enc_bytes.begin(), vec_enc_bytes.end());
      }

      for (const auto& vec_c : vec_entry_contents)
        vec_out.insert(vec_out.end(), vec_c.begin(), vec_c.end());
      return vec_out;
    };

    const auto dump_mix = [&](std::string_view sv_name, std::vector<std::byte> vec_file,
                              std::string_view sv_variant) {
      str_out += std::format("M {}\n", sv_name);
      str_out += std::format("ML {} {}\n", sv_variant, sv_variant == "ts" ? 1 : 0);
      auto vec_chars = std::vector<char>{};
      vec_chars.resize(vec_file.size());
      std::memcpy(vec_chars.data(), vec_file.data(), vec_file.size());
      const ora::fs::MixFile mix{std::move(vec_chars), std::string{sv_name}, vec_globals};

      auto vec_absolute = mix.AbsoluteIndex();
      std::ranges::sort(vec_absolute,
                        [](const auto& a, const auto& b) { return a.first < b.first; });
      for (const auto& [str_entry_name, entry] : vec_absolute) {
        const auto vec_content = mix.GetStream(str_entry_name);
        ORA_CHECK(vec_content.has_value());
        str_out += std::format("ME {} {} {} {}\n", str_entry_name, entry.uint4_offset,
                               vec_content->size(),
                               ora::fmt::CRC32::Calculate(std::as_bytes(std::span{*vec_content})));
      }
      str_out += std::format("MU {}\n", vec_entry_names.size() - vec_absolute.size());
    };

    dump_mix("mix_cnc_classic", make_mix_bytes(ora::fs::PackageHashType::Classic, "cnc"), "cnc");
    dump_mix("mix_ra_classic", make_mix_bytes(ora::fs::PackageHashType::Classic, "ra"), "ra");
    dump_mix("mix_ts_crc32_encrypted", make_mix_bytes(ora::fs::PackageHashType::CRC32, "ts"), "ts");

    {
      const auto vec_db = ora::fmt::XccLocalDatabase::MakeData(vec_local_db_names);
      const ora::fmt::XccLocalDatabase database{vec_db};
      str_out += std::format("X {}\n", database.Entries().size());
      for (const std::string& str_e : database.Entries())
        str_out += std::format("XE {}\n", str_e);
    }

    {
      auto vec_global = std::vector<std::byte>{};
      const auto push_u32 = [&vec_global](std::uint32_t uint4_v) {
        for (auto int4_i = 0; int4_i < 4; int4_i++)
          vec_global.push_back(static_cast<std::byte>(uint4_v >> (8 * int4_i)));
      };
      const auto push_sz = [&vec_global](std::string_view sv) {
        for (const char chr_c : sv)
          vec_global.push_back(static_cast<std::byte>(chr_c));
        vec_global.push_back(std::byte{0});
      };
      push_u32(2);
      push_sz("a.dat");
      push_sz("comment one");
      push_sz("b.dat");
      push_sz("second comment");
      push_u32(1);
      push_sz("averylongfilename.somewhat.shx");
      push_sz("");
      push_u32(1);
      push_sz("dup.dat");
      push_sz("x");

      const ora::fmt::XccGlobalDatabase global{vec_global};
      str_out += std::format("G {}\n", global.Entries().size());
      for (const std::string& str_e : global.Entries())
        str_out += std::format("GE {}\n", str_e);
    }
  }

  {
    struct SoundFileRef {
      std::string str_rel;
      bool bool_wav;
    };
    auto vec_sounds = std::vector<SoundFileRef>{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator{str_upstream_root + "/mods"}) {
      if (!entry.is_regular_file())
        continue;
      auto str_path = entry.path().generic_string();
      const auto st_dot = str_path.find_last_of('.');
      if (st_dot == std::string::npos)
        continue;
      auto str_ext = str_path.substr(st_dot + 1);
      std::ranges::transform(str_ext, str_ext.begin(), [](char chr_c) { return chr_c >= 'A' && chr_c <= 'Z' ? static_cast<char>(chr_c - 'A' + 'a') : chr_c; });
      if (str_ext != "aud" && str_ext != "wav")
        continue;
      vec_sounds.push_back({str_path.substr(str_upstream_root.size() + 1), str_ext == "wav"});
    }
    std::ranges::sort(vec_sounds, [](const SoundFileRef& a, const SoundFileRef& b) { return a.str_rel < b.str_rel; });
    for (const auto& ref : vec_sounds)
      AppendSoundLines(ref.bool_wav ? "B" : "A", ref.str_rel, ReadFileBytes(str_upstream_root + "/" + ref.str_rel),
                       str_out);
  }

  AppendSoundLines("SA", "aud_ima_multi",
                   MakeAudFile(22050, 0x02, 99, 52, {{Pat(8, 7, 0), 32}, {Pat(5, 13, 3), 20}}), str_out);
  AppendSoundLines("SA", "aud_ima_odd", MakeAudFile(22050, 0x02, 99, 7, {{Pat(3, 11, 1), 12}}), str_out);
  AppendSoundLines("SA", "aud_ws_cases",
                   MakeAudFile(11025, 0x00, 1, 17,
                               {{MakeBytes({0xC1, 0x82, 0x11, 0x22, 0x33, 0xA5, 0x00, 0xE4, 0x40, 0x1B, 0x40, 0x27,
                                           0xBF}),
                                 15},
                                {MakeBytes({0x44, 0x55, 0x66, 0x77}), 4}}),
                   str_out);
  AppendSoundLines("SA", "aud_ws_stale",
                   MakeAudFile(11025, 0x00, 1, 9,
                               {{MakeBytes({0x82, 0xAA, 0xBB, 0xCC}), 3},
                                {MakeBytes({0xC1}), 4},
                                {MakeBytes({0xC0}), 2}}),
                   str_out);

  {
    auto vec_pcm_ramp = std::vector<std::byte>(20);
    for (auto int4_i = 0; int4_i < 10; int4_i++) {
      const auto int2_v = static_cast<std::int16_t>(int4_i < 5 ? int4_i * 1111 : -(int4_i - 4) * 2222);
      vec_pcm_ramp[static_cast<std::size_t>(2 * int4_i)] = static_cast<std::byte>(int2_v);
      vec_pcm_ramp[static_cast<std::size_t>(2 * int4_i + 1)] = static_cast<std::byte>(int2_v >> 8);
    }

    AppendSoundLines("SV", "wav_pcm_mono", MakeWavFile(1, 1, 22050, 2, 16, false, 0, -1, vec_pcm_ramp, {}), str_out);
    AppendSoundLines("SV", "wav_pcm_defect_data",
                     MakeWavFile(1, 2, 22050, 2, 8, false, 0, 0x7FFFFFFF,
                                 MakeBytes({0x11, 0x22, 0x33, 0x44, 0x55, 0x66}),
                                 MakeBytes({'J', 'U', 'N', 'K', 'J', 'U', 'N', 'K'})),
                     str_out);
    {
      auto vec_list_pad = std::vector<std::byte>{};
      const auto push_pad = [&vec_list_pad](std::string_view sv, std::initializer_list<std::uint8_t> vec_bytes) {
        for (const char chr_c : sv)
          if (chr_c)
            vec_list_pad.push_back(static_cast<std::byte>(chr_c));
        for (const auto uint1_v : vec_bytes)
          vec_list_pad.push_back(static_cast<std::byte>(uint1_v));
      };
      push_pad("LIST", {3, 0, 0, 0});
      push_pad("abc", {});
      vec_list_pad.push_back(std::byte{0});
      push_pad("cue ", {0, 0, 0, 0});
      AppendSoundLines("SV", "wav_pcm_list_pad",
                       MakeWavFile(1, 1, 22050, 2, 16, false, 0, -1,
                                   std::span<const std::byte>{vec_pcm_ramp}.first(6), vec_list_pad),
                       str_out);
    }

    auto vec_ima_block = std::vector<std::byte>(36, std::byte{0});
    vec_ima_block[0] = std::byte{0x34};
    vec_ima_block[1] = std::byte{0x12};
    vec_ima_block[2] = std::byte{3};
    const auto vec_ima_data = Pat(32, 5, 1);
    std::ranges::copy(vec_ima_data, vec_ima_block.begin() + 4);

    AppendSoundLines("SV", "wav_ima_mono", MakeWavFile(0x11, 1, 22050, 36, 4, true, 33, -1, vec_ima_block, {}),
                     str_out);
    AppendSoundLines("SV", "wav_ima_nofact", MakeWavFile(0x11, 1, 22050, 36, 4, false, 0, -1, vec_ima_block, {}),
                     str_out);
    {
      auto vec_ima_stereo = std::vector<std::byte>(64, std::byte{0});
      const auto fill_stereo_block = [&vec_ima_stereo](int int4_blk, std::size_t st_off) {
        const auto int2_p = static_cast<std::int16_t>(int4_blk == 0 ? 0x0BB8 : -0x0BB8);
        vec_ima_stereo[st_off] = static_cast<std::byte>(int2_p);
        vec_ima_stereo[st_off + 1] = static_cast<std::byte>(int2_p >> 8);
        vec_ima_stereo[st_off + 2] = std::byte{2};
        const auto int4_np = static_cast<int>(
            ~static_cast<int>(int2_p));
        vec_ima_stereo[st_off + 4] = static_cast<std::byte>(int4_np);
        vec_ima_stereo[st_off + 5] = static_cast<std::byte>(int4_np >> 8);
        vec_ima_stereo[st_off + 6] = std::byte{5};
        std::ranges::copy(Pat(24, 3, 1 + int4_blk * 7),
                          vec_ima_stereo.begin() + static_cast<std::ptrdiff_t>(st_off + 8));
      };
      fill_stereo_block(0, 0);
      fill_stereo_block(1, 32);
      AppendSoundLines("SV", "wav_ima_stereo",
                       MakeWavFile(0x11, 2, 44100, 32, 4, true, 25, -1, vec_ima_stereo, {}), str_out);
    }

    auto vec_ms_mono = std::vector<std::byte>(14, std::byte{0});
    vec_ms_mono[0] = std::byte{1};
    vec_ms_mono[1] = std::byte{100};
    vec_ms_mono[3] = std::byte{0xE8};
    vec_ms_mono[4] = std::byte{0x03};
    vec_ms_mono[5] = std::byte{0x30};
    vec_ms_mono[6] = std::byte{0xF8};
    std::ranges::copy(Pat(7, 9, 2), vec_ms_mono.begin() + 7);
    AppendSoundLines("SV", "wav_ms_mono", MakeWavFile(2, 1, 11025, 14, 4, false, 0, -1, vec_ms_mono, {}), str_out);

    auto vec_ms_stereo = std::vector<std::byte>(22, std::byte{0});
    vec_ms_stereo[0] = std::byte{1};
    vec_ms_stereo[1] = std::byte{3};
    vec_ms_stereo[2] = std::byte{100};
    vec_ms_stereo[4] = std::byte{0xCE};
    vec_ms_stereo[5] = std::byte{0xFF};
    vec_ms_stereo[6] = std::byte{0xE8};
    vec_ms_stereo[7] = std::byte{0x03};
    vec_ms_stereo[8] = std::byte{0x18};
    vec_ms_stereo[9] = std::byte{0xF4};
    vec_ms_stereo[10] = std::byte{0x30};
    vec_ms_stereo[11] = std::byte{0xF8};
    vec_ms_stereo[12] = std::byte{0xF4};
    vec_ms_stereo[13] = std::byte{0x01};
    std::ranges::copy(Pat(8, 7, 1), vec_ms_stereo.begin() + 14);
    AppendSoundLines("SV", "wav_ms_stereo", MakeWavFile(2, 2, 22050, 22, 4, false, 0, -1, vec_ms_stereo, {}),
                     str_out);
  }

  {
    auto vec_iv = std::vector<std::byte>{};
    for (auto int4_i = 0; int4_i < 32; int4_i++)
      vec_iv.push_back(static_cast<std::byte>(int4_i));
    str_out += std::format("IV {}\n", BytesToHexLower(vec_iv));

    auto int4_index = 0;
    auto int4_current = 0;
    auto vec_shorts = std::vector<std::byte>{};
    for (const auto byte_b : vec_iv) {
      const auto int2_s =
          ora::fmt::ima_adpcm::DecodeImaAdpcmSample(static_cast<std::uint8_t>(byte_b), int4_index, int4_current);
      vec_shorts.push_back(static_cast<std::byte>(int2_s));
      vec_shorts.push_back(static_cast<std::byte>(int2_s >> 8));
    }
    str_out += std::format("IVR {} {} {}\n", int4_index, int4_current, BytesToHexLower(vec_shorts));
  }
  {
    const auto dump_wv = [&str_out](std::string_view sv_tag, std::vector<std::byte> vec_input,
                                    std::size_t st_out_len) {
      auto vec_output = std::vector<std::byte>(st_out_len, std::byte{0});
      ora::fmt::westwood_compressed::DecodeWestwoodCompressedSample(vec_input, vec_output);
      str_out += std::format("{} {}\n", sv_tag, BytesToHexLower(vec_input));
      str_out += std::format("{}R {}\n", sv_tag, BytesToHexLower(vec_output));
    };
    dump_wv("WV", MakeBytes({0xC1, 0x82, 0x11, 0x22, 0x33, 0xA5, 0x00, 0xE4, 0x40, 0x1B, 0x40, 0x27, 0xBF}), 15);
    dump_wv("WV", MakeBytes({0x82, 0xAA, 0xBB, 0xCC}), 3);
    dump_wv("WV", MakeBytes({0xC1}), 4);
    dump_wv("WVP", MakeBytes({0x44, 0x55, 0x66, 0x77}), 4);
  }

  {

    const auto uint1_bw = std::uint8_t{2};
    const auto uint1_bh = std::uint8_t{2};
    const auto uint2_w = std::uint16_t{128};
    const auto uint2_h = std::uint16_t{64};
    const auto int4_bx = uint2_w / uint1_bw;
    const auto int4_by = uint2_h / uint1_bh;
    const auto vec_cbf0 = Pat(uint2_w * uint2_h, 7, 0);
    const auto vec_cbf1 = Pat(uint2_w * uint2_h, 11, 5);
    const auto vec_orig0 = NonHqOrig(int4_bx, int4_by, 0);
    const auto vec_orig1 = NonHqOrig(int4_bx, int4_by, 13);
    const auto vec_orig2 = NonHqOrig(int4_bx, int4_by, 29);

    auto vec_f0 = std::vector<std::byte>{};
    VqPutChunk(vec_f0, "SND0", Pat(8, 5, 1));
    auto vec_vqfr0 = std::vector<std::byte>{};
    {
      const auto cpl0 = VqCpl0(0);
      vec_vqfr0.insert(vec_vqfr0.end(), cpl0.begin(), cpl0.end());
      const auto cbfz = VqSub("CBFZ", ora::fmt::lcw::Encode(vec_cbf0));
      vec_vqfr0.insert(vec_vqfr0.end(), cbfz.begin(), cbfz.end());
      const auto vptz = VqVptz(vec_orig0);
      vec_vqfr0.insert(vec_vqfr0.end(), vptz.begin(), vptz.end());
    }
    VqPutChunk(vec_f0, "VQFR", vec_vqfr0);

    auto vec_f1 = std::vector<std::byte>{};
    {
      auto vec_sn2j = std::vector<std::byte>{};
      VqPutU32B(vec_sn2j, 4);
      vec_sn2j.insert(vec_sn2j.end(), 4, std::byte{0});
      VqPutChunk(vec_f1, "SN2J", vec_sn2j);
      const auto snd2a = Pat(6, 3, 7);
      VqPutChunk(vec_f1, "SND2", snd2a);
      auto vec_vqfr1 = std::vector<std::byte>{};
      const auto vptz = VqVptz(vec_orig1);
      vec_vqfr1.insert(vec_vqfr1.end(), vptz.begin(), vptz.end());
      VqPutChunk(vec_f1, "VQFR", vec_vqfr1);
      const auto snd2b = Pat(5, 9, 2);
      VqPutChunk(vec_f1, "SND2", snd2b);
    }

    auto vec_f2 = std::vector<std::byte>{};
    {
      auto vec_nul_vqf = std::vector<std::byte>{std::byte{0}};
      auto vec_vqfr2 = std::vector<std::byte>{};
      const auto cpl0 = VqCpl0(100);
      vec_vqfr2.insert(vec_vqfr2.end(), cpl0.begin(), cpl0.end());
      const auto vptz = VqVptz(vec_orig2);
      vec_vqfr2.insert(vec_vqfr2.end(), vptz.begin(), vptz.end());
      vec_nul_vqf.insert(vec_nul_vqf.end(), vec_vqfr2.begin(), vec_vqfr2.end());
      VqPutChunk(vec_f2, std::string_view{"\0VQF", 4}, vec_nul_vqf);
    }

    auto vec_f3 = std::vector<std::byte>{};
    VqPutChunk(vec_f3, "VQFL", VqSub("CBFZ", ora::fmt::lcw::Encode(vec_cbf1)));

    const auto vec_vqa_file =
        MakeVqaFile(0, 4, uint2_w, uint2_h, uint1_bw, uint1_bh, 0, 22050, 1, 8, {vec_f0, vec_f1, vec_f2, vec_f3});
    AppendVideoLines("vqa_nonhq", vec_vqa_file, false, str_out);
    AppendVideoLines("vqa_nonhq", vec_vqa_file, true, str_out);

    const auto uint2_sw = std::uint16_t{64};
    const auto uint2_sh = std::uint16_t{32};
    const auto vec_s_orig = NonHqOrigSmall(uint2_sw / uint1_bw, uint2_sh / uint1_bh, 3);
    auto vec_sf = std::vector<std::byte>{};
    {
      auto vec_svqfr = std::vector<std::byte>{};
      const auto cpl0 = VqCpl0(0);
      vec_svqfr.insert(vec_svqfr.end(), cpl0.begin(), cpl0.end());
      const auto cbfz = VqSub("CBFZ", ora::fmt::lcw::Encode(Pat(uint2_sw * uint2_sh, 7, 0)));
      vec_svqfr.insert(vec_svqfr.end(), cbfz.begin(), cbfz.end());
      const auto vptz = VqVptz(vec_s_orig);
      vec_svqfr.insert(vec_svqfr.end(), vptz.begin(), vptz.end());
      VqPutChunk(vec_sf, "VQFR", vec_svqfr);
      const auto snd2 = Pat(5, 3, 1);
      VqPutChunk(vec_sf, "SND2", snd2);
    }
    const auto vec_vqa_stereo =
        MakeVqaFile(0, 1, uint2_sw, uint2_sh, uint1_bw, uint1_bh, 0, 44100, 2, 8, {vec_sf});
    AppendVideoLines("vqa_stereo", vec_vqa_stereo, false, str_out);

    const auto vec_cbp_a = Pat(uint2_w * uint2_h, 11, 5);
    auto vec_cbp_target = std::vector<std::byte>(static_cast<std::size_t>(uint2_w) * uint2_h, std::byte{0});
    for (std::size_t st_i = 0; st_i < vec_cbp_target.size(); st_i++)
      vec_cbp_target[st_i] = static_cast<std::byte>((st_i >> 5) & 0xFF);
    const auto vec_cbp_stream = ora::fmt::lcw::Encode(vec_cbp_target);

    const auto make_cbp_frame = [&](std::vector<std::byte> vec_sub1, std::vector<std::byte> vec_sub2) {
      auto vec_subs = std::vector<std::byte>{};
      vec_subs.insert(vec_subs.end(), vec_sub1.begin(), vec_sub1.end());
      vec_subs.insert(vec_subs.end(), vec_sub2.begin(), vec_sub2.end());
      return VqFrame(vec_subs);
    };
    const auto half = [](const std::vector<std::byte>& vec, bool b_first) {
      const auto st_mid = vec.size() / 2;
      return b_first ? std::vector<std::byte>{vec.begin(), vec.begin() + st_mid}
                     : std::vector<std::byte>{vec.begin() + st_mid, vec.end()};
    };

    auto vec_cf0 = std::vector<std::byte>{};
    {
      const auto cpl0 = VqCpl0(0);
      vec_cf0.insert(vec_cf0.end(), cpl0.begin(), cpl0.end());
      const auto cbp0 = VqSub("CBP0", half(vec_cbp_a, true));
      vec_cf0.insert(vec_cf0.end(), cbp0.begin(), cbp0.end());
      const auto vptz = VqVptz(vec_orig0);
      vec_cf0.insert(vec_cf0.end(), vptz.begin(), vptz.end());
    }
    auto vec_cf1 = std::vector<std::byte>{};
    {
      const auto cbp0 = VqSub("CBP0", half(vec_cbp_a, false));
      vec_cf1.insert(vec_cf1.end(), cbp0.begin(), cbp0.end());
      const auto vptz = VqVptz(vec_orig1);
      vec_cf1.insert(vec_cf1.end(), vptz.begin(), vptz.end());
    }
    auto vec_cf2 = VqFrame(VqVptz(vec_orig2));
    auto vec_cf3 = std::vector<std::byte>{};
    {
      const auto cbp0 = VqSub("CBP0", half(vec_cbp_stream, true));
      vec_cf3.insert(vec_cf3.end(), cbp0.begin(), cbp0.end());
      const auto vptz = VqVptz(vec_orig0);
      vec_cf3.insert(vec_cf3.end(), vptz.begin(), vptz.end());
    }
    auto vec_cf4 = std::vector<std::byte>{};
    {
      const auto cbpz = VqSub("CBPZ", half(vec_cbp_stream, false));
      vec_cf4.insert(vec_cf4.end(), cbpz.begin(), cbpz.end());
      const auto vptz = VqVptz(vec_orig1);
      vec_cf4.insert(vec_cf4.end(), vptz.begin(), vptz.end());
    }
    auto vec_cf5 = VqFrame(VqVptz(vec_orig2));
    const auto vec_vqa_cbp = MakeVqaFile(0, 6, uint2_w, uint2_h, uint1_bw, uint1_bh, 2, 0, 0, 0,
                                          {VqFrame(vec_cf0), VqFrame(vec_cf1), vec_cf2, VqFrame(vec_cf3),
                                           VqFrame(vec_cf4), vec_cf5});
    AppendVideoLines("vqa_cbp", vec_vqa_cbp, false, str_out);

    auto vec_cbf16 = std::vector<std::byte>(4096, std::byte{0});
    for (std::size_t st_i = 0; st_i < vec_cbf16.size(); st_i++)
      vec_cbf16[st_i] = static_cast<std::byte>((st_i * 29 + 7) & 0xFF);
    auto vec_cbf16b = std::vector<std::byte>(4096, std::byte{0});
    for (std::size_t st_i = 0; st_i < vec_cbf16b.size(); st_i++)
      vec_cbf16b[st_i] = static_cast<std::byte>((st_i * 53 + 29) & 0xFF);
    const auto vec_instr0 = HqInstr(int4_bx, int4_by, 0);
    const auto vec_instr1 = HqInstr(int4_bx, int4_by, 1);
    const auto vec_instr2 = HqInstr(int4_bx, int4_by, 2);
    auto vec_hf0 = std::vector<std::byte>{};
    {
      const auto cpl0 = VqCpl0(0);
      vec_hf0.insert(vec_hf0.end(), cpl0.begin(), cpl0.end());
      const auto cbfz = VqSub("CBFZ", ora::fmt::lcw::Encode(vec_cbf16));
      vec_hf0.insert(vec_hf0.end(), cbfz.begin(), cbfz.end());
      const auto vprz = VqVprz(vec_instr0);
      vec_hf0.insert(vec_hf0.end(), vprz.begin(), vprz.end());
    }
    const auto vec_hf1 = VqFrame(VqVptr(vec_instr1));
    auto vec_hf2 = std::vector<std::byte>{};
    {
      const auto cbfz = VqSub("CBFZ", LcwLiteralReversed(vec_cbf16b));
      vec_hf2.insert(vec_hf2.end(), cbfz.begin(), cbfz.end());
      const auto vprz = VqSub("VPRZ", LcwLiteralReversed(vec_instr2));
      vec_hf2.insert(vec_hf2.end(), vprz.begin(), vprz.end());
    }
    const auto vec_vqa_hq =
        MakeVqaFile(0x10, 3, uint2_w, uint2_h, uint1_bw, uint1_bh, 0, 0, 0, 0,
                    {VqFrame(vec_hf0), vec_hf1, VqFrame(vec_hf2)});
    AppendVideoLines("vqa_hq", vec_vqa_hq, false, str_out);

    const auto encode_xor_delta = [](const std::vector<std::byte>& vec_prev, const std::vector<std::byte>& vec_cur) {
      auto vec = std::vector<std::byte>{};
      auto int4_i = 0;
      const auto int4_n = static_cast<int>(vec_cur.size());
      while (int4_i < int4_n) {
        if (vec_cur[static_cast<std::size_t>(int4_i)] == vec_prev[static_cast<std::size_t>(int4_i)]) {
          auto int4_run = 0;
          while (int4_i + int4_run < int4_n &&
                 vec_cur[static_cast<std::size_t>(int4_i + int4_run)] == vec_prev[static_cast<std::size_t>(int4_i + int4_run)] &&
                 int4_run < 127)
            int4_run++;
          vec.push_back(static_cast<std::byte>(0x80 | int4_run));
          int4_i += int4_run;
        } else {
          auto int4_run = 0;
          while (int4_i + int4_run < int4_n &&
                 vec_cur[static_cast<std::size_t>(int4_i + int4_run)] != vec_prev[static_cast<std::size_t>(int4_i + int4_run)] &&
                 int4_run < 127)
            int4_run++;
          vec.push_back(static_cast<std::byte>(int4_run));
          for (auto int4_k = 0; int4_k < int4_run; int4_k++)
            vec.push_back(static_cast<std::byte>(
                static_cast<std::uint8_t>(vec_cur[static_cast<std::size_t>(int4_i + int4_k)]) ^
                static_cast<std::uint8_t>(vec_prev[static_cast<std::size_t>(int4_i + int4_k)])));
          int4_i += int4_run;
        }
      }

      vec.push_back(std::byte{0x80});
      vec.push_back(std::byte{0});
      vec.push_back(std::byte{0});
      vec.push_back(std::byte{0});
      return vec;
    };

    const auto make_wsa_file = [&](std::uint16_t uint2_frames, std::uint16_t uint2_width, std::uint16_t uint2_height,
                                   bool b_palette, const std::vector<std::vector<std::byte>>& vec_frame_payloads,
                                   int int4_trailing) {
      auto vec = std::vector<std::byte>{};
      VqPutU16(vec, uint2_frames);
      VqPutU16(vec, 10);
      VqPutU16(vec, 20);
      VqPutU16(vec, uint2_width);
      VqPutU16(vec, uint2_height);
      VqPutU16(vec, 37);
      VqPutU16(vec, b_palette ? 1 : 0);

      auto uint4_pos = static_cast<std::uint32_t>(vec.size() + 4 * (static_cast<std::size_t>(uint2_frames) + 2));
      auto vec_offs = std::vector<std::uint32_t>{};
      for (const auto& vec_p : vec_frame_payloads) {
        vec_offs.push_back(uint4_pos);
        uint4_pos += static_cast<std::uint32_t>(vec_p.size());
      }

      vec_offs.push_back(uint4_pos);
      vec_offs.push_back(uint4_pos + int4_trailing);
      for (const auto uint4_o : vec_offs)
        VqPutU32L(vec, uint4_o);
      if (b_palette)
        for (auto int4_i = 0; int4_i < 768; int4_i++)
          vec.push_back(static_cast<std::byte>(int4_i & 0xFF));
      for (const auto& vec_p : vec_frame_payloads)
        vec.insert(vec.end(), vec_p.begin(), vec_p.end());
      vec.insert(vec.end(), static_cast<std::size_t>(int4_trailing), static_cast<std::byte>('T'));
      return vec;
    };

    const auto uint2_ww = std::uint16_t{30};
    const auto uint2_wh = std::uint16_t{20};
    const auto st_n = static_cast<std::size_t>(uint2_ww) * uint2_wh;

    auto vec_zeros = std::vector<std::byte>(st_n, std::byte{0});
    auto vec_idx0 = std::vector<std::byte>(st_n, std::byte{0});
    for (std::size_t st_i = 0; st_i < st_n; st_i++)
      vec_idx0[st_i] = st_i % 50 == 1 ? static_cast<std::byte>((st_i * 7 + 3) & 0xFF) : std::byte{0};
    auto vec_idx1 = std::vector<std::byte>(st_n, std::byte{0});
    for (std::size_t st_i = 0; st_i < st_n; st_i++)
      vec_idx1[st_i] = st_i % 50 == 1 || st_i % 50 == 7 ? static_cast<std::byte>((st_i * 11 + 9) & 0xFF)
                                                        : std::byte{0};
    const auto vec_p0 = ora::fmt::lcw::Encode(encode_xor_delta(vec_zeros, vec_idx0));
    const auto vec_p1 = ora::fmt::lcw::Encode(encode_xor_delta(vec_idx0, vec_idx1));
    const auto vec_wsa_file = make_wsa_file(2, uint2_ww, uint2_wh, true, {vec_p0, vec_p1}, 4);
    AppendVideoLines("wsa_basic", vec_wsa_file, false, str_out);
    AppendVideoLines("wsa_basic", vec_wsa_file, true, str_out);

    const auto vec_terminator = std::vector<std::byte>{std::byte{0x80}, std::byte{0}};
    AppendVideoLines("wsa_shp_like", make_wsa_file(1, 4, 4, false, {vec_terminator}, 0), false, str_out);
  }

  {
    const auto esc = [](std::string_view sv) {
      auto str_out2 = std::string{};
      for (const char chr_c : sv) {
        if (chr_c == '\0')
          str_out2 += "\\0";
        else
          str_out2.push_back(chr_c);
      }
      return str_out2;
    };
    const auto ex_line = [](const std::exception& ex) {
      auto str_msg = std::string_view{ex.what()};
      const auto st_colon = str_msg.find(": ");
      if (st_colon != std::string_view::npos && str_msg.substr(0, st_colon).starts_with("System."))
        str_msg = str_msg.substr(st_colon + 2);
      auto str_out2 = std::string{};
      for (const char chr_c : str_msg)
        if (chr_c == '\n')
          str_out2 += "\\n";
        else
          str_out2.push_back(chr_c);
      return str_out2;
    };
    const auto put_f32 = [](std::vector<std::byte>& vec, float fp4_v) {
      const auto uint4_bits = std::bit_cast<std::uint32_t>(fp4_v);
      VqPutU32L(vec, uint4_bits);
    };
    const auto put_name16 = [](std::vector<std::byte>& vec, std::string_view sv_name) {
      for (auto int4_i = 0; int4_i < 16; int4_i++)
        vec.push_back(static_cast<std::size_t>(int4_i) < sv_name.size()
                          ? static_cast<std::byte>(sv_name[static_cast<std::size_t>(int4_i)])
                          : std::byte{0});
    };

    struct VxlRun {
      int int4_skip;
      int int4_count;
    };
    using VxlCol = std::optional<std::vector<VxlRun>>;

    const auto make_vox = [](int int4_col, int int4_run, int int4_k) {
      return std::pair<std::uint8_t, std::uint8_t>{
          static_cast<std::uint8_t>((int4_col * 7 + int4_run * 13 + int4_k * 3) & 0xFF),
          static_cast<std::uint8_t>((int4_k * 31 + 5) & 0xFF)};
    };

    const auto make_vxl_limb_data = [&](int int4_sx, int int4_sy, const std::vector<VxlCol>& vec_cols) {
      const auto int4_base_size = int4_sx * int4_sy;
      auto vec = std::vector<std::byte>{};
      auto vec_streams = std::vector<std::byte>{};
      auto vec_offs = std::vector<std::int32_t>(static_cast<std::size_t>(int4_base_size));
      for (auto int4_i = 0; int4_i < int4_base_size; int4_i++) {
        if (!vec_cols[static_cast<std::size_t>(int4_i)].has_value()) {
          vec_offs[static_cast<std::size_t>(int4_i)] = -1;
          continue;
        }

        vec_offs[static_cast<std::size_t>(int4_i)] = static_cast<std::int32_t>(vec_streams.size());
        const auto& vec_runs = *vec_cols[static_cast<std::size_t>(int4_i)];
        for (auto int4_r = 0; int4_r < static_cast<int>(vec_runs.size()); int4_r++) {
          const auto run = vec_runs[static_cast<std::size_t>(int4_r)];
          vec_streams.push_back(static_cast<std::byte>(run.int4_skip));
          vec_streams.push_back(static_cast<std::byte>(run.int4_count));
          for (auto int4_k = 0; int4_k < run.int4_count; int4_k++) {
            const auto [uint1_c, uint1_n] = make_vox(int4_i, int4_r, int4_k);
            vec_streams.push_back(static_cast<std::byte>(uint1_c));
            vec_streams.push_back(static_cast<std::byte>(uint1_n));
          }

          vec_streams.push_back(static_cast<std::byte>(run.int4_count));
        }
      }

      for (const auto int4_o : vec_offs)
        VqPutU32L(vec, static_cast<std::uint32_t>(int4_o));
      vec.insert(vec.end(), static_cast<std::size_t>(4 * int4_base_size), std::byte{0});
      vec.insert(vec.end(), vec_streams.begin(), vec_streams.end());
      return vec;
    };

    struct VxlLimbSpec {
      std::string str_name;
      float fp4_scale;
      std::array<float, 6> arr_bounds;
      std::array<std::uint8_t, 3> arr_size;
      std::uint8_t uint1_type;
      std::vector<std::byte> vec_data;
    };

    const auto make_vxl_file = [&](const std::vector<VxlLimbSpec>& vec_limbs, int int4_gap) {
      auto vec = std::vector<std::byte>{};

      for (const char chr_c : std::string_view{"Voxel Animation"})
        vec.push_back(static_cast<std::byte>(chr_c));
      vec.push_back(std::byte{0});
      VqPutU32L(vec, 0);
      VqPutU32L(vec, static_cast<std::uint32_t>(vec_limbs.size()));
      VqPutU32L(vec, 0);
      const auto st_body_size_pos = vec.size();
      VqPutU32L(vec, 0);
      vec.insert(vec.end(), 770, std::byte{0});

      for (const auto& limb : vec_limbs) {
        put_name16(vec, limb.str_name);
        vec.insert(vec.end(), 12, std::byte{0});
      }

      const auto st_data_base = vec.size();
      auto vec_offs = std::vector<std::uint32_t>{};
      for (const auto& limb : vec_limbs) {
        vec_offs.push_back(static_cast<std::uint32_t>(vec.size() - st_data_base));
        vec.insert(vec.end(), limb.vec_data.begin(), limb.vec_data.end());
      }

      vec.insert(vec.end(), static_cast<std::size_t>(int4_gap), static_cast<std::byte>('G'));
      const auto uint4_body_size = static_cast<std::uint32_t>(vec.size() - st_data_base);
      vec[st_body_size_pos] = static_cast<std::byte>(uint4_body_size & 0xFF);
      vec[st_body_size_pos + 1] = static_cast<std::byte>((uint4_body_size >> 8) & 0xFF);
      vec[st_body_size_pos + 2] = static_cast<std::byte>((uint4_body_size >> 16) & 0xFF);
      vec[st_body_size_pos + 3] = static_cast<std::byte>((uint4_body_size >> 24) & 0xFF);

      for (std::size_t st_i = 0; st_i < vec_limbs.size(); st_i++) {
        const auto& limb = vec_limbs[st_i];
        VqPutU32L(vec, vec_offs[st_i]);
        vec.insert(vec.end(), 8, std::byte{0});
        put_f32(vec, limb.fp4_scale);
        vec.insert(vec.end(), 48, std::byte{0});
        for (const auto fp4_f : limb.arr_bounds)
          put_f32(vec, fp4_f);
        vec.push_back(static_cast<std::byte>(limb.arr_size[0]));
        vec.push_back(static_cast<std::byte>(limb.arr_size[1]));
        vec.push_back(static_cast<std::byte>(limb.arr_size[2]));
        vec.push_back(static_cast<std::byte>(limb.uint1_type));
      }

      return vec;
    };

    const auto append_vxl_lines = [&](std::string_view sv_name, std::span<const std::byte> vec_file,
                                      std::string& str_target) {
      str_target += std::format("XQ {}\n", sv_name);
      try {
        const auto vxl = ora::fmt::VxlReader{vec_file};
        str_target += std::format("XC {}\n", vxl.uint4_limb_count);
        for (std::size_t st_i = 0; st_i < vxl.vec_limbs.size(); st_i++) {
          const auto& limb = vxl.vec_limbs[st_i];
          str_target += std::format("XB {} {} {}", st_i, esc(limb.str_name),
                                    ora::meta::FormatFloatNet(limb.fp4_scale));
          for (const auto fp4_f : limb.arr_bounds)
            str_target += " " + ora::meta::FormatFloatNet(fp4_f);
          str_target += std::format(" {} {} {} {} {}\n", static_cast<int>(limb.arr_size[0]),
                                    static_cast<int>(limb.arr_size[1]), static_cast<int>(limb.arr_size[2]),
                                    static_cast<int>(limb.enum_type), limb.uint4_voxel_count);
          for (auto int4_y = 0; int4_y < limb.arr_size[1]; int4_y++)
            for (auto int4_x = 0; int4_x < limb.arr_size[0]; int4_x++) {
              const auto st_slot =
                  static_cast<std::size_t>(int4_x) + static_cast<std::size_t>(int4_y) * limb.arr_size[0];
              if (limb.vec_column_present[st_slot] == 0)
                continue;
              const auto& vec_column = limb.vec_voxel_map[st_slot];
              str_target += std::format("XCM {} {} {} {}\n", st_i, int4_x, int4_y, vec_column.size());
              for (const auto& [uint1_z, element] : vec_column)
                str_target += std::format("XM {} {} {} {} {} {}\n", st_i, int4_x, int4_y,
                                          static_cast<int>(uint1_z), static_cast<int>(element.uint1_color),
                                          static_cast<int>(element.uint1_normal));
            }
        }
      } catch (const std::exception& ex) {
        str_target += std::format("XX {} EX {}\n", sv_name, ex_line(ex));
      }
    };

    const auto hva_cell = [](int int4_j, int int4_i) {
      const auto int4_n = int4_i % 2;
      return std::array<float, 12>{

          static_cast<float>(int4_n + 1), 0.0f,                                   0.0f, int4_j + 0.5f,
          0.0f,                          static_cast<float>(int4_n + 2),         1.0f, static_cast<float>(int4_i) - 0.25f,
          0.0f,                          1.0f, static_cast<float>(int4_n + 3), static_cast<float>(int4_j * 2)};
    };

    const auto make_hva_file = [&](std::uint32_t uint4_frames, std::uint32_t uint4_limbs,
                                   const std::function<std::array<float, 12>(int, int)>& fn_cell) {
      auto vec = std::vector<std::byte>{};
      put_name16(vec, "hvafile");
      VqPutU32L(vec, uint4_frames);
      VqPutU32L(vec, uint4_limbs);
      for (std::uint32_t uint4_i = 0; uint4_i < uint4_limbs; uint4_i++)
        put_name16(vec, std::format("limb{}", uint4_i));
      for (std::uint32_t uint4_j = 0; uint4_j < uint4_frames; uint4_j++)
        for (std::uint32_t uint4_i = 0; uint4_i < uint4_limbs; uint4_i++)
          for (const auto fp4_f : fn_cell(static_cast<int>(uint4_j), static_cast<int>(uint4_i)))
            put_f32(vec, fp4_f);
      return vec;
    };

    const auto append_hva_lines = [&](std::string_view sv_name, std::span<const std::byte> vec_file,
                                      std::string& str_target) {
      str_target += std::format("HQ {} ", sv_name);
      try {
        const auto hva = ora::fmt::HvaReader{vec_file, sv_name};
        str_target += std::format("{} {}\n", hva.uint4_frame_count, hva.uint4_limb_count);
        for (auto uint4_j = 0u; uint4_j < hva.uint4_frame_count; uint4_j++)
          for (auto uint4_i = 0u; uint4_i < hva.uint4_limb_count; uint4_i++) {
            str_target += std::format("HT {} {}", uint4_j, uint4_i);
            for (auto int4_k = 0; int4_k < 16; int4_k++)
              str_target += " " + ora::meta::FormatFloatNet(
                                     hva.vec_transforms[16 * (hva.uint4_limb_count * uint4_j + uint4_i) +
                                                        static_cast<std::size_t>(int4_k)]);
            str_target += '\n';
          }
      } catch (const std::exception& ex) {
        str_target += std::format("EX {}\n", ex_line(ex));
      }
    };

    struct IdxSpec {
      std::string str_name;
      std::uint32_t uint4_off;
      std::uint32_t uint4_len;
      std::uint32_t uint4_rate;
      std::uint32_t uint4_flags;
      std::uint32_t uint4_chunk;
    };

    const auto make_idx_file = [&](std::string_view sv_magic, int int4_two, const std::vector<IdxSpec>& vec_entries) {
      auto vec = std::vector<std::byte>{};
      for (const char chr_c : sv_magic)
        vec.push_back(static_cast<std::byte>(chr_c));
      VqPutU32L(vec, static_cast<std::uint32_t>(int4_two));
      VqPutU32L(vec, static_cast<std::uint32_t>(vec_entries.size()));
      for (const auto& entry : vec_entries) {
        put_name16(vec, entry.str_name);
        VqPutU32L(vec, entry.uint4_off);
        VqPutU32L(vec, entry.uint4_len);
        VqPutU32L(vec, entry.uint4_rate);
        VqPutU32L(vec, entry.uint4_flags);
        VqPutU32L(vec, entry.uint4_chunk);
      }

      return vec;
    };

    const auto append_idx_lines = [&](std::string_view sv_name, std::span<const std::byte> vec_file,
                                      std::string& str_target) {
      str_target += std::format("IQ {} ", sv_name);
      try {
        const auto idx = ora::fmt::IdxReader{vec_file};
        str_target += std::format("{}\n", idx.int4_sound_count);
        for (const auto& entry : idx.vec_entries) {
          str_target += std::format("IE {} {} {} {} {} {}\n", esc(entry.str_filename), entry.uint4_offset,
                                    entry.uint4_length, entry.uint4_sample_rate, entry.uint4_flags,
                                    entry.uint4_chunk_size);
          str_target += std::format("IT {}\n", esc(entry.ToString()));
        }
      } catch (const std::exception& ex) {
        str_target += std::format("EX {}\n", ex_line(ex));
      }
    };

    auto vec_l0_cols = std::vector<VxlCol>(12);
    vec_l0_cols[0] = std::vector<VxlRun>{{1, 2}, {0, 3}};
    vec_l0_cols[2] = std::vector<VxlRun>{{2, 1}, {1, 2}};
    vec_l0_cols[3] = std::vector<VxlRun>{{5, 0}};
    vec_l0_cols[4] = std::vector<VxlRun>{{3, 2}};
    vec_l0_cols[6] = std::vector<VxlRun>{{0, 5}};
    vec_l0_cols[7] = std::vector<VxlRun>{{4, 1}};
    auto vec_l1_cols = std::vector<VxlCol>(4);
    vec_l1_cols[0] = std::vector<VxlRun>{{0, 3}};
    vec_l1_cols[2] = std::vector<VxlRun>{{1, 1}, {1, 1}};
    vec_l1_cols[3] = std::vector<VxlRun>{{2, 1}};

    const auto vec_vxl_file = make_vxl_file(
        {VxlLimbSpec{"turret", 0.5f, {-1.25f, 2.5f, -0.125f, 1.5f, 3.75f, -2.25f}, {4, 3, 5}, 2,
                     make_vxl_limb_data(4, 3, vec_l0_cols)},
         VxlLimbSpec{"bodyfull12345678", 0.03125f, {-100.5f, 100.5f, 0.0f, 1.0f, -1.0f, 16.0f}, {2, 2, 3}, 4,
                     make_vxl_limb_data(2, 2, vec_l1_cols)}},
        3);
    append_vxl_lines("vxl_basic", vec_vxl_file, str_out);

    auto vec_bad_head = std::vector<std::byte>{};
    for (const char chr_c : std::string_view{"Voxel AnimXXXXZ"})
      vec_bad_head.push_back(static_cast<std::byte>(chr_c));
    vec_bad_head.push_back(std::byte{0});
    append_vxl_lines("vxl_badhead", vec_bad_head, str_out);

    append_hva_lines("hva_basic", make_hva_file(2, 2, hva_cell), str_out);
    append_hva_lines("hva_singular", make_hva_file(1, 1, [](int, int) { return std::array<float, 12>{}; }),
                     str_out);

    append_idx_lines("idx_basic",
                     make_idx_file("GABA", 2,
                                   {IdxSpec{"a10rumor", 0x1234, 0x5678, 22050, 0, 0},
                                    IdxSpec{"", 0xFFFFFFFF, 0, 8000, 7, 4096},
                                    IdxSpec{"buzzer", 0, 0xFFFFFFFF, 44100, 1, 512}}),
                     str_out);
    append_idx_lines("idx_badmagic", make_idx_file("GABX", 2, {}), str_out);
    append_idx_lines("idx_badtwo", make_idx_file("GABA", 3, {}), str_out);
  }

  {
    const auto dump_voc = [&](std::string_view str_name, std::span<const std::byte> vec_file) {
      str_out += std::format("VO {}\n", str_name);
      ora::fmt::VocInfo info_voc;
      std::vector<std::byte> vec_pcm;
      if (ora::fmt::TryParseVoc(vec_file, info_voc, vec_pcm)) {
        str_out += "L VocLoader\n";
        str_out += std::format("SD {} {} {} {}\n", info_voc.int4_channels, info_voc.int4_sample_bits,
                               info_voc.int4_sample_rate,
                               ora::meta::FormatFloatNet(info_voc.fp4_length_in_seconds));
        str_out += std::format("SC {} {}\n", ora::fmt::CRC32::Calculate(vec_pcm), vec_pcm.size());
        str_out += std::format("SX {}\n",
                               BytesToHexLower(std::span<const std::byte>{vec_pcm}.first(std::min<std::size_t>(32, vec_pcm.size()))));
      } else
        str_out += "L NONE\n";
    };

    dump_voc("voc_full", MakeVocFile({
                             std::make_tuple(8, BytesOf({0x00, 0xCE, 0x00, 0x00})),
                             std::make_tuple(1, BytesOf({0xD2, 0x00, 0x10, 0x20, 0x30, 0x40})),
                             std::make_tuple(3, BytesOf({0x03, 0x00, 0xD2})),
                             std::make_tuple(6, BytesOf({0x01, 0x00})),
                             std::make_tuple(7, std::vector<std::byte>{}),
                             std::make_tuple(8, BytesOf({0x00, 0xCE, 0x00, 0x00})),
                             std::make_tuple(1, BytesOf({0xD2, 0x00, 0xAA, 0xBB})),
                         }));
    dump_voc("voc_11025", MakeVocFile({std::make_tuple(1, BytesOf({0xA5, 0x00, 0x01}))}));
    {
      auto vec_neg = MakeVocFile({});
      vec_neg[18] = static_cast<std::byte>('X');
      dump_voc("voc_negdesc", vec_neg);
    }
    dump_voc("voc_negmixed", MakeVocFile({
                                 std::make_tuple(1, BytesOf({0xD2, 0x00, 0x01})),
                                 std::make_tuple(1, BytesOf({0xA5, 0x00, 0x02})),
                             }));

    {
      const std::vector<std::byte> vec_pixels = BytesOf({0x00, 0x01, 0x02, 0xFF, 0x01, 0x00, 0xFF, 0x02});
      std::vector<std::uint16_t> vec_palette(256, 0);
      vec_palette[1] = 0x7C00 | 0x0063;
      vec_palette[2] = 0x03E0;
      auto vec_r8 = MakeR8Frame(1, 4, 2, 2, 1, 0x77, 8, vec_pixels, vec_palette);
      const auto vec_frame2 = MakeR8Frame(2, 2, 1, 1, 0, 0, 8, BytesOf({0x01, 0x02}), {});
      vec_r8.insert(vec_r8.end(), vec_frame2.begin(), vec_frame2.end());
      AppendTaggedTry('R', "r8_two_frames", vec_r8, ora::fmt::TryParseR8, "R8Loader", str_out);
    }
    AppendTaggedTry('R', "r8_neg_zero", std::vector<std::byte>(30, std::byte{0}), ora::fmt::TryParseR8,
                     "R8Loader", str_out);

    {
      const std::vector<std::byte> vec_rgba = BytesOf({0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF});
      const ora::fmt::Png png_single{vec_rgba, ora::gfx::SpriteFrameType::Rgba32, 2, 1};
      AppendTaggedTry('R', "pngsheet_single", png_single.Save(), ora::fmt::TryParsePngSheet, "PngSheetLoader",
                     str_out);

      std::vector<std::byte> vec_rgba2;
      for (std::uint8_t uint1_p = 0; uint1_p < 8; uint1_p++)
        for (auto int4_c = 0; int4_c < 4; int4_c++)
          vec_rgba2.push_back(static_cast<std::byte>(uint1_p * 16 + int4_c * 3));
      const ora::fmt::Png png_frames{vec_rgba2, ora::gfx::SpriteFrameType::Rgba32, 4, 2, std::nullopt,
                                     {{"FrameSize", "2,2"}}};
      AppendTaggedTry('R', "pngsheet_frames", png_frames.Save(), ora::fmt::TryParsePngSheet, "PngSheetLoader",
                     str_out);

      const ora::fmt::Png png_manual{vec_rgba2, ora::gfx::SpriteFrameType::Rgba32, 4, 2, std::nullopt,
                                     {{"Frame[0]", "0,0,2,2;0,0"}, {"Frame[1]", "2,0,2,2;1,-2.5"}}};
      AppendTaggedTry('R', "pngsheet_manual", png_manual.Save(), ora::fmt::TryParsePngSheet, "PngSheetLoader",
                      str_out);
    }
  }

  {
    struct SeqCase {
      const char* sz_name;
      std::string str_yaml;
      bool b_classic;
    };
    const std::vector<SeqCase> vec_seq_cases = {
        {"defaults_inherit",
         "img:\n    Defaults:\n        Tick: 25\n        ZOffset: 512\n    seq:\n        Filename: a.shp\n"
         "        Length: 2\n",
         false},
        {"overrides",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Tick: 33\n        ZOffset: 1024\n"
         "        ShadowZOffset: -64\n        IgnoreWorldTint: True\n        Scale: 2.5\n        Facings: 8\n"
         "        InterpolatedFacings: 32\n",
         false},
        {"star_length", "img:\n    seq:\n        Filename: a.shp\n        Length: *\n", false},
        {"e_facings3", "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Facings: 3\n", false},
        {"e_facings1025",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Facings: 1025\n",
         false},
        {"e_facings0", "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Facings: 0\n", false},
        {"e_facings_neg", "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Facings: -8\n", false},
        {"e_interp_small",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 1\n        Facings: 2\n"
         "        InterpolatedFacings: 2\n",
         false},
        {"e_interp_notpow2",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 1\n        Facings: 2\n"
         "        InterpolatedFacings: 3\n",
         false},
        {"e_length0", "img:\n    seq:\n        Filename: a.shp\n        Length: 0\n", false},
        {"e_star_facings", "img:\n    seq:\n        Filename: a.shp\n        Length: *\n        Facings: 2\n", false},
        {"e_alphafade",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        AlphaFade: True\n        Alpha: 0.5\n",
         false},
        {"classic_ok",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 1\n        Facings: 32\n"
         "        UseClassicFacings: True\n",
         true},
        {"e_classic",
         "img:\n    seq:\n        Filename: a.shp\n        Length: 2\n        Facings: 16\n"
         "        UseClassicFacings: True\n",
         true},
    };

    for (const SeqCase& case_seq : vec_seq_cases) {
      const std::vector<ora::yaml::MiniYamlNode> vec_nodes =
          ora::yaml::MiniYaml::FromString(case_seq.str_yaml, "seq21", true,
                                          ora::yaml::MiniYaml::GlobalPool());
      auto up_loader = ora::mods::MakeSequenceLoader(
          case_seq.b_classic ? "ClassicSpriteSequence" : "DefaultSpriteSequence");

      try {

        ora::fs::FileSystem file_system_empty{};
        const std::vector<ora::gfx::SpriteLoaderFn> vec_loaders_empty{};
        ora::gfx::SpriteCache cache_seq{file_system_empty, vec_loaders_empty, 128, 128};
        auto vec_parsed = up_loader->ParseSequences(cache_seq, "temperat", vec_nodes.front());
        for (auto& [str_seq_name, seq_seq] : vec_parsed) {
          str_out += std::format("SQ {} Name {}\n", case_seq.sz_name, str_seq_name);
          str_out += std::format("SQ {} Facings {}\n", case_seq.sz_name, seq_seq->Facings());
          str_out += std::format("SQ {} Tick {}\n", case_seq.sz_name, seq_seq->Tick());
          str_out += std::format("SQ {} ZOffset {}\n", case_seq.sz_name, seq_seq->ZOffset());
          str_out += std::format("SQ {} ShadowZOffset {}\n", case_seq.sz_name, seq_seq->ShadowZOffset());

          str_out += std::format("SQ {} IgnoreWorldTint {}\n", case_seq.sz_name,
                                 seq_seq->IgnoreWorldTint() ? "True" : "False");
          str_out += std::format("SQ {} Scale {}\n", case_seq.sz_name,
                                 ora::meta::FormatFloatNet(seq_seq->Scale()));
        }
      } catch (const std::exception& e) {

        const std::string str_what = e.what();
        const auto st_nl = str_what.find('\n');
        const std::string str_head = st_nl == std::string::npos ? str_what : str_what.substr(0, st_nl);
        const std::string str_inner = st_nl == std::string::npos ? "" : str_what.substr(st_nl + 1);
        str_out += std::format("SQE {} {}|{}\n", case_seq.sz_name, str_head, str_inner);
      }
    }
  }

  return str_out;
}

void TestGoldenDifferential(const std::string& str_upstream_root, const std::string& str_golden_path) {
  auto vec_golden = ReadFileBytes(str_golden_path);
  auto str_golden = std::string{reinterpret_cast<const char*>(vec_golden.data()), vec_golden.size()};

  auto str_actual = BuildActualText(str_upstream_root);

  auto vec_golden_lines = str_golden | std::views::split('\n') | std::ranges::to<std::vector<std::string>>();
  auto vec_actual_lines = str_actual | std::views::split('\n') | std::ranges::to<std::vector<std::string>>();
  ORA_CHECK(vec_golden_lines.back().empty());
  ORA_CHECK(vec_actual_lines.back().empty());
  vec_golden_lines.pop_back();
  vec_actual_lines.pop_back();

  ORA_CHECK(vec_golden_lines.size() == vec_actual_lines.size());
  auto int4_mismatches = 0;
  auto st_len = std::min(vec_golden_lines.size(), vec_actual_lines.size());
  for (std::size_t st_i = 0; st_i < st_len; st_i++) {
    if (vec_golden_lines[st_i] != vec_actual_lines[st_i]) {
      if (int4_mismatches < 20)
        std::println(stderr, "golden diff @line {}: expected [{}] actual [{}]", st_i + 1, vec_golden_lines[st_i],
                     vec_actual_lines[st_i]);
      ++int4_mismatches;
    }
  }
  ORA_CHECK(int4_mismatches == 0);

  std::println("formats golden: {} lines compared", st_len);
}

}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 3) {
    std::println(stderr, "usage: formats_test <upstream-root> <golden-formats.txt>");
    return 2;
  }

  try {
    TestFastByteReader();
    TestRLEZeros();
    TestXORDelta();
    TestLCW();
    TestCRC32();
    TestSyntheticShpTD();
    TestSheetBuilderFrameAdd();
    TestPngPureLogic();
    TestTgaDdsPureLogic();
    TestShpRemasteredPureLogic();
    TestEmbeddedSpritePalette();
    TestBlowfish();
    TestXccDatabases();
    TestPackageEntryHash();
    TestMixFile();
    TestD2kSoundResources();
    TestImaAdpcm();
    TestWestwoodCompressed();
    TestAudReader();
    TestWavReader();
    TestVqaWsaVideo();
    TestVxlHvaIdx();
    TestVocLoader();
    TestOggMp3Sniff();
    TestR8AndPngSheet();
    std::println("pure-logic ok");
    TestGoldenDifferential(argv[1], argv[2]);
  } catch (const std::exception& ex) {
    std::println(stderr, "formats_test exception: {}", ex.what());
    ++int4_failures;
  }

  if (int4_failures != 0) {
    std::println(stderr, "formats_test: {} FAILURES", int4_failures);
    return 1;
  }
  std::println("formats_test: all ok");
  return 0;
}
