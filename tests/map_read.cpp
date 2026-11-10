import std;

#include "game/filesystem/inflate.hpp"
#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/tileset.hpp"
#include "game/support/sha1.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

namespace {

namespace ogs = ora::game::support;
namespace ogf = ora::game::filesystem;
namespace ogm = ora::game;



void run_sha1_unit() {
  check(ogs::sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1 空串向量");
  check(ogs::sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1 abc 向量");
  check(ogs::sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
      == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "sha1 56 字节向量");
  std::string str_million(1'000'000, 'a');
  check(ogs::sha1_hex(str_million) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "sha1 百万 a 向量");
  check(ogs::sha1_hex(std::string(63, 'x')) == "0ddc4e0cccd9a12850deb5abb0853a4425559fec", "sha1 63 字节边界");
  check(ogs::sha1_hex(std::string(64, 'x')) == "bb2fa3ee7afb9f54c6dfb5d021f14b1ffe40c163", "sha1 64 字节边界");
  check(ogs::sha1_hex(std::string(65, 'x')) == "78c741ddc482e4cdf8c474a0876347a0905b6233", "sha1 65 字节边界");
  check(ogs::sha1_hex(std::string(128, 'x')) == "150fa3fbdc899bd0b8f95a9fb6027f564d953762", "sha1 128 字节双块");
}

std::optional<std::string> read_file_bytes(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string{str_path}}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::vector<std::uint8_t> unhex(std::string_view str_hex) {
  auto nib{[](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  }};
  std::vector<std::uint8_t> vec1u_out;
  vec1u_out.reserve(str_hex.size() / 2);
  for (std::size_t sz_i{0}; sz_i + 1 < str_hex.size(); sz_i += 2)
    vec1u_out.push_back(static_cast<std::uint8_t>((nib(str_hex[sz_i]) << 4) | nib(str_hex[sz_i + 1])));
  return vec1u_out;
}

void check_inflate_roundtrip(std::string_view str_hex_stream, std::span<const std::uint8_t> span1u_expected,
    std::string_view str_what) {
  const auto res_inflated{ogf::inflate_raw(unhex(str_hex_stream))};
  if (!res_inflated) {
    check(false, std::format("{}: inflate_raw 报错 {}", str_what, res_inflated.error()));
    return;
  }
  check(*res_inflated == std::vector<std::uint8_t>(span1u_expected.begin(), span1u_expected.end()),
      std::format("{}: 解压结果 {} 字节不符", str_what, res_inflated->size()));
}

void run_inflate_unit() {
  const std::string str_stored{"hello world hello world"};
  check_inflate_roundtrip(
      "011700e8ff68656c6c6f20776f726c642068656c6c6f20776f726c64",
      {reinterpret_cast<const std::uint8_t*>(str_stored.data()), str_stored.size()}, "inflate stored 块");

  const std::string str_fixed{"fixed huffman block test data"};
  check_inflate_roundtrip(
      "4bcbac484d51c8284d4bcb4dcc5348cac94fce5628492d2e5148492c490400",
      {reinterpret_cast<const std::uint8_t*>(str_fixed.data()), str_fixed.size()}, "inflate fixed 块");

  check_inflate_roundtrip("0300", {}, "inflate 空流");

  std::string str_repeat;
  for (int int4_i{0}; int4_i < 5000; ++int4_i) str_repeat += "ab";
  check_inflate_roundtrip(
      "edc2010900000002a0adf57f443b02d134000000000000000000d07703",
      {reinterpret_cast<const std::uint8_t*>(str_repeat.data()), str_repeat.size()}, "inflate dynamic 块长回引");

  std::vector<std::uint8_t> vec1u_binary;
  for (int int4_r{0}; int4_r < 16; ++int4_r)
    for (int int4_i{0}; int4_i < 256; ++int4_i)
      vec1u_binary.push_back(static_cast<std::uint8_t>(int4_i));
  check_inflate_roundtrip(
      "6360646266616563e7e0e4e2e6e1e5e317101412161115139790949296919593"
      "5750545256515553d7d0d4d2d6d1d5d33730343236313533b7b0b4b2b6b1b5b3"
      "7770747276717573f7f0f4f2f6f1f5f30f080c0a0e090d0b8f888c8a8e898d8b"
      "4f484c4a4e494d4bcfc8cccacec9cdcb2f282c2a2e292d2bafa8acaaaea9adab"
      "6f686c6a6e696d6befe8eceaeee9edeb9f3071d2e42953a74d9f3173d6ec3973"
      "e7cd5fb070d1e2254b972d5fb172d5ea356bd7addfb071d3e62d5bb76ddfb173"
      "d7ee3d7bf7ed3f70f0d0e123478f1d3f71f2d4e93367cf9dbf70f1d2e52b57af"
      "5dbf71f3d6ed3b77efdd7ff0f0d1e3274f9f3d7ff1f2d5eb376fdfbdfff0f1d3"
      "e72f5fbf7dfff1f3d7ef3f7ffffd6718f5ffa8ff47fd3feaff51ff8ffa7fd4ff"
      "a3fe1ff5ffa8ff47fd3feaff51ff8ffa7f04f81f00",
      vec1u_binary, "inflate 二进制 4096 字节");

  std::vector<std::uint8_t> vec1u_multi;
  vec1u_multi.reserve(38400);
  for (int int4_r{0}; int4_r < 32; ++int4_r)
    for (int int4_g{0}; int4_g < 4; ++int4_g)
      for (int int4_i{0}; int4_i < 300; ++int4_i)
        vec1u_multi.push_back(static_cast<std::uint8_t>(32 + (int4_i * 7) % 64));
  check_inflate_roundtrip(
      "e5ce874d15000040c1553e82d2a5f7de7bff20a0c0fe5b30c6e5c5012eb9c1f8ef95eda3cb87"
      "d7af5fb3cb5b8717f77f3e7fce2c6d1e9cdfbd7c8c4d2f6eec9fdd3eff1b9d5a58df3bbd19fe"
      "fd3139bfb67b72fdf43e3231b7ba737cf5f836f88f7ce9aa7de9aa7de9aa7de9aa7de9aa7de9"
      "aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7d"
      "e9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa"
      "7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9"
      "aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7d"
      "e9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa"
      "7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9"
      "aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7d"
      "e9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa"
      "7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9"
      "aa7de9aa7de9aa7de9aa7de9aa7de9aa7de9aa7de93ac4fe1b",
      vec1u_multi, "inflate 多块跨窗口 38400 字节");

  check(!ogf::inflate_raw(unhex("07")).has_value(), "inflate BTYPE=3 拒绝");
  check(!ogf::inflate_raw(unhex("01050000006865")).has_value(), "inflate stored LEN/NLEN 不符拒绝");
  check(!ogf::inflate_raw(unhex("01050000")).has_value(), "inflate stored 数据截断拒绝");
  check(!ogf::inflate_raw({}).has_value(), "inflate 空输入拒绝");
}

void run_zip_unit() {
  const std::string str_zip_hex{
      "504b0304140000080000000000006a39e0d0050000000500000005000400612e7478745859"
      "0102616c706861504b03041400000800000000000000000000000000000000000004000000"
      "6469722f504b030414000008080000000000a0fcb031250000006000000005000000622e62"
      "696e6360646266616563e7e0e4e2e6e1e5e31710141216111513979094929691959367a050"
      "1e00504b01021400140000080000000000006a39e0d0050000000500000005000000000000"
      "0000000000000000000000612e747874504b01021400140000080000000000000000000000"
      "0000000000000004000000000000000000000000002c0000006469722f504b010214001400"
      "0008080000000000a0fcb031250000006000000005000000000000000000000000004e0000"
      "00622e62696e504b0506000000000300030098000000960000000000"};
  const auto vec1u_zip{unhex(str_zip_hex)};

  const auto res_zip{ogf::zip_read(vec1u_zip)};
  if (!res_zip) {
    check(false, std::format("zip_read 合成包报错 {}", res_zip.error()));
    return;
  }
  check(res_zip->vec_entries.size() == 2, "zip 条目数（目录条目滤除）");
  if (res_zip->vec_entries.size() == 2) {
    check(res_zip->vec_entries[0].str_name == "a.txt" && res_zip->vec_entries[1].str_name == "b.bin",
        "zip 条目保序（中央目录书写序）");
    check(res_zip->vec_entries[0].u2_method == 0 && res_zip->vec_entries[0].u4_usize == 5, "zip stored 条目元数据");
    check(res_zip->vec_entries[1].u2_method == 8 && res_zip->vec_entries[1].u4_usize == 96, "zip deflate 条目元数据");
  }

  const auto res_a{ogf::zip_entry_data(*res_zip, "a.txt")};
  check(res_a.has_value() && std::string(res_a->begin(), res_a->end()) == "alpha",
      "zip local header extra 差异下取数");

  const auto res_b{ogf::zip_entry_data(*res_zip, "b.bin")};
  if (res_b) {
    check(ogs::sha1_hex(*res_b) == "9b177a3a97d827faf494440705f3e1bce94d3019", "zip deflate 条目取数");
  } else {
    check(false, std::format("zip b.bin 取数报错 {}", res_b.error()));
  }

  check(!ogf::zip_entry_data(*res_zip, "missing.txt").has_value(), "zip 缺名条目报错");

  const auto vec1u_trunc{std::vector<std::uint8_t>(vec1u_zip.begin(), vec1u_zip.begin() + 40)};
  check(!ogf::zip_read(vec1u_trunc).has_value(), "zip 截断输入报错");
  check(!ogf::zip_read(unhex("00000000")).has_value(), "zip 非 zip 签名报错");
}

void run_map_bin_unit() {
  const auto res_bin{ogm::map_bin_parse(unhex("020200020011000000000000001d000000"
      "0500ff0600ff0700ff080002010c000002030105"), 2, 2)};
  if (!res_bin) {
    check(false, std::format("map_bin fmt2 报错 {}", res_bin.error()));
    return;
  }
  check(res_bin->vec_tiles.size() == 4 && res_bin->vec_resources.size() == 4, "map_bin fmt2 网格尺寸");
  check(res_bin->u4_tiles_offset == 17 && res_bin->u4_resources_offset == 29 && res_bin->u4_heights_offset == 0,
      "map_bin fmt2 偏移字段");
  check(res_bin->tile_at(0, 0).u2_type == 5 && res_bin->tile_at(0, 0).u1_index == 0, "map.bin index==255 特判 (0,0)");
  check(res_bin->tile_at(0, 1).u2_type == 6 && res_bin->tile_at(0, 1).u1_index == 4, "map.bin index==255 特判 (0,1)");
  check(res_bin->tile_at(1, 0).u2_type == 7 && res_bin->tile_at(1, 0).u1_index == 1, "map.bin index==255 特判 (1,0)");
  check(res_bin->tile_at(1, 1).u2_type == 8 && res_bin->tile_at(1, 1).u1_index == 2, "map.bin 正常帧保留");
  check(res_bin->resource_at(0, 0).u1_type == 1 && res_bin->resource_at(0, 0).u1_density == 12, "map.bin resource (0,0)");
  check(res_bin->resource_at(1, 1).u1_type == 1 && res_bin->resource_at(1, 1).u1_density == 5, "map.bin resource (1,1)");

  const auto res_bin1{ogm::map_bin_parse(unhex("0102000200"
      "0500ff0600ff0700ff080002010c000002030105"), 2, 2)};
  check(res_bin1.has_value() && res_bin1->u4_tiles_offset == 5 && res_bin1->u4_resources_offset == 17
      && res_bin1->vec_tiles.size() == 4 && res_bin1->vec_resources.size() == 4
      && res_bin1->resource_at(0, 0).u1_density == 12, "map.bin fmt1 隐式偏移");

  const auto res_bad{ogm::map_bin_parse(unhex("030200020011000000000000001d000000"), 2, 2)};
  check(!res_bad.has_value() && res_bad.error() == "Unknown binary map format '3'", "map.bin 未知格式错误串");

  const auto res_size{ogm::map_bin_parse(unhex("020200020011000000000000001d000000"), 3, 3)};
  check(!res_size.has_value() && res_size.error() == "Invalid tile data", "map.bin 尺寸不符错误串");

  check(!ogm::map_bin_parse(unhex("020200020011000000"), 2, 2).has_value(), "map.bin 截断报错");
}

constexpr std::string_view kInlineTileset = R"yaml(General:
	Name: tileset-test
	Id: TEST
Terrain:
	TerrainType@Water:
		Type: Water
	TerrainType@Clear:
		Type: Clear
Templates:
	Template@10:
		Id: 10
		Images: x.tem
		Size: 2,1
		Categories: Terrain
		Tiles:
			0: Clear
			1: Water
	Template@7:
		Id: 7
		Images: clear.tem
		Size: 1,1
		PickAny: True
		Categories: Terrain
		Tiles:
			0: Clear
			1: Clear
	Template@8:
		Id: 8
		Images: half.tem
		Size: 2,1
		Categories: Terrain
		Tiles:
			0: Water
)yaml";

void run_tileset_unit() {
  const auto res_ts{ogm::tileset_load(kInlineTileset, "inline")};
  if (!res_ts) {
    check(false, std::format("tileset 内联文本报错 {}", res_ts.error()));
    return;
  }
  check(res_ts->str_id == "TEST" && res_ts->str_name == "tileset-test", "tileset General Id/Name");
  check(res_ts->int4_tile_size_x == 24 && res_ts->int4_tile_size_y == 24, "tileset TileSize 默认 24,24");
  check(res_ts->vec_str_types.size() == 2 && res_ts->vec_str_types[0] == "Clear"
      && res_ts->vec_str_types[1] == "Water", "tileset 类型表按名排序");
  check(res_ts->terrain_index_of_type("Clear") == 0 && res_ts->terrain_index_of_type("Water") == 1,
      "tileset GetTerrainIndex 查表");
  const auto res_missing{res_ts->terrain_index_of_type("Ore")};
  check(!res_missing.has_value() && res_missing.error() == "Tileset 'TEST' lacks terrain type 'Ore'",
      "tileset 缺类型错误串");
  check(res_ts->default_terrain_tile().u2_type == 10 && res_ts->default_terrain_tile().u1_index == 0,
      "tileset DefaultTerrainTile 取 yaml 定义序首个模板");
  check(res_ts->vec_templates.size() == 3, "tileset 模板表大小");
  check(res_ts->tile_is_valid({10, 0}) && res_ts->tile_is_valid({10, 1}), "tileset 非 PickAny 列出帧有效");
  check(!res_ts->tile_is_valid({10, 2}), "tileset 非 PickAny 帧越界无效");
  check(!res_ts->tile_is_valid({99, 0}), "tileset 模板缺失无效");
  check(!res_ts->tile_is_valid({8, 1}), "tileset 非 PickAny 未列出帧为 null 无效");
  check(res_ts->tile_is_valid({8, 0}), "tileset 非 PickAny 列出帧有效");
  check(res_ts->tile_is_valid({7, 1}) && res_ts->tile_is_valid({7, 0}), "tileset PickAny 连续帧有效");
  check(!res_ts->tile_is_valid({7, 2}), "tileset PickAny 帧越界无效");
  check(res_ts->terrain_index_of_tile({10, 1}) == 1, "tileset 帧地形索引（Water）");
  check(res_ts->terrain_index_of_tile({7, 0}) == 0, "tileset 帧地形索引（Clear）");

  const auto res_no_clear{ogm::tileset_load(R"yaml(General:
	Name: x
	Id: NOCLEAR
Terrain:
	TerrainType@Water:
		Type: Water
)yaml", "inline2")};
  check(!res_no_clear.has_value() && res_no_clear.error() == "Tileset 'NOCLEAR' lacks terrain type 'Clear'",
      "tileset 缺 Clear 类型报错");

  const auto str_temperat{read_file_bytes(std::string{ORA_MAPS_GOLDEN_DIR} + "/tilesets/temperat.yaml")};
  if (!str_temperat) {
    check(false, "tileset temperat.yaml 黄金文件缺失");
    return;
  }
  const auto res_temp{ogm::tileset_load(*str_temperat, "temperat.yaml")};
  if (!res_temp) {
    check(false, std::format("tileset temperat 报错 {}", res_temp.error()));
    return;
  }
  check(res_temp->str_id == "TEMPERAT" && res_temp->str_name == "tileset-temperat"
      && res_temp->int4_tile_size_x == 24 && res_temp->int4_tile_size_y == 24
      && res_temp->vec_str_types.size() == 12, "tileset temperat 概要（对齐 oracle terraininfo 行）");
  const std::vector<std::string> vec_str_want{"Beach", "Bridge", "Clear", "Gems", "Ore", "River",
      "Road", "Rock", "Rough", "Tree", "Wall", "Water"};
  check(res_temp->vec_str_types == vec_str_want, "tileset temperat 类型排序表");
  check(res_temp->default_terrain_tile().u2_type == 255 && res_temp->default_terrain_tile().u1_index == 0,
      "tileset temperat DefaultTerrainTile 255,0");
  check(res_temp->terrain_index_of_tile({105, 0}) == 7 && res_temp->terrain_index_of_tile({109, 0}) == 7
      && res_temp->terrain_index_of_tile({109, 3}) == 7, "tileset temperat terrainidx 样本（对齐 oracle）");
  check(res_temp->tile_is_valid({65535, 3}), "tileset temperat 模板 65535 合法 PickAny");
  check(res_temp->find_template(65535) != nullptr, "tileset temperat 模板 65535 在表");

  const auto str_snow{read_file_bytes(std::string{ORA_MAPS_GOLDEN_DIR} + "/tilesets/snow.yaml")};
  if (!str_snow) {
    check(false, "tileset snow.yaml 黄金文件缺失");
    return;
  }
  const auto res_snow{ogm::tileset_load(*str_snow, "snow.yaml")};
  if (!res_snow) {
    check(false, std::format("tileset snow 报错 {}", res_snow.error()));
    return;
  }
  check(res_snow->str_id == "SNOW" && res_snow->str_name == "tileset-snow"
      && res_snow->vec_str_types.size() == 12, "tileset snow 概要（对齐 oracle terraininfo 行）");
  check(res_snow->default_terrain_tile().u2_type == 255, "tileset snow DefaultTerrainTile 255,0");
  check(res_snow->terrain_index_of_tile({1, 0}) == 11 && res_snow->terrain_index_of_tile({2, 0}) == 11,
      "tileset snow terrainidx 样本（对齐 oracle）");
}

constexpr std::string_view kInlineMapYaml = R"yaml(MapFormat: 12
RequiresMod: ra
Title: Inline Test
Author:Tester

Tileset: TEMPERAT
MapSize: 4,3
Bounds: 1,0,2,3
Visibility: Lobby
Categories: Conquest, Classic
Players:
	PlayerReference@Neutral:
		Name: Neutral
	PlayerReference@Multi0:
		Name: Multi0
Actors:
	Actor0: mpspawn
		Location: 1,1
		Owner: Neutral
)yaml";

std::vector<std::uint8_t> to_bytes(std::string_view str_text) {
  return {reinterpret_cast<const std::uint8_t*>(str_text.data()),
      reinterpret_cast<const std::uint8_t*>(str_text.data()) + str_text.size()};
}

void run_map_header_unit() {
  const auto res_hdr{ogm::map_header_parse(to_bytes(kInlineMapYaml), "inline:map.yaml")};
  if (!res_hdr) {
    check(false, std::format("map_header 内联报错 {}", res_hdr.error()));
    return;
  }
  check(res_hdr->int4_format == 12 && res_hdr->str_requires_mod == "ra" && res_hdr->str_title == "Inline Test"
      && res_hdr->str_author == "Tester" && res_hdr->str_tileset == "TEMPERAT", "map.yaml 标量字段");
  check(res_hdr->int4_size_x == 4 && res_hdr->int4_size_y == 3, "map.yaml MapSize");
  check(res_hdr->int4_bounds_x == 1 && res_hdr->int4_bounds_y == 0 && res_hdr->int4_bounds_w == 2
      && res_hdr->int4_bounds_h == 3, "map.yaml Bounds 宽高语义");
  check(res_hdr->u4_visibility == 1u, "map.yaml Visibility flags");
  check(res_hdr->vec_str_categories.size() == 2 && res_hdr->vec_str_categories[0] == "Conquest"
      && res_hdr->vec_str_categories[1] == "Classic", "map.yaml Categories 拆分");
  check(res_hdr->sz_players == 2 && res_hdr->sz_actors == 1, "map.yaml Players/Actors 计数");
  check(ogm::map_scan_format(to_bytes(kInlineMapYaml)) == 12, "MapFormat 逐行扫描");

  const auto res_missing{ogm::map_header_parse(to_bytes("MapFormat: 12\nRequiresMod: ra\n"),
      "inline:map.yaml")};
  check(!res_missing.has_value() && res_missing.error() == "Required field `Title` not found in map.yaml",
      "map.yaml 缺必填键错误串");
}

void run_map_info_golden() {
  const std::string str_maps_dir{ORA_MAPS_GOLDEN_DIR};
  for (const std::string_view str_map : {"ore-lord", "doubles", "a-path-beyond"}) {
    const auto str_oramap{read_file_bytes(std::format("{}/{}.oramap", str_maps_dir, str_map))};
    const auto str_want{read_file_bytes(std::format("{}/{}.mapinfo.txt", str_maps_dir, str_map))};
    if (!str_oramap || !str_want) {
      check(false, std::format("{}: 黄金文件缺失", str_map));
      continue;
    }
    const auto res_dump{ogm::map_info_dump(to_bytes(*str_oramap), std::format("{}/tilesets", str_maps_dir))};
    if (!res_dump) {
      check(false, std::format("{}: map_info_dump 报错 {}", str_map, res_dump.error()));
      continue;
    }
    if (*res_dump == *str_want) {
      check(true, std::format("{}: mapinfo 逐字节一致", str_map));
      continue;
    }
    std::size_t sz_diff{0};
    const std::size_t sz_min{std::min(res_dump->size(), str_want->size())};
    while (sz_diff < sz_min && (*res_dump)[sz_diff] == (*str_want)[sz_diff]) ++sz_diff;
    const auto line_of{[](const std::string& str_text, std::size_t sz_off) {
      const std::size_t idx_line_start{str_text.rfind('\n', sz_off)};
      const std::size_t idx_line_end{str_text.find('\n', sz_off)};
      return str_text.substr(idx_line_start == std::string::npos ? 0 : idx_line_start + 1,
          (idx_line_end == std::string::npos ? str_text.size() : idx_line_end)
              - (idx_line_start == std::string::npos ? 0 : idx_line_start + 1));
    }};
    check(false, std::format("{}: mapinfo 首差异偏移 {} got[{}] want[{}]", str_map, sz_diff,
        line_of(*res_dump, sz_diff), line_of(*str_want, sz_diff)));
  }

  const auto str_ore{read_file_bytes(std::format("{}/ore-lord.oramap", str_maps_dir))};
  if (str_ore) {
    const auto res_zip{ogf::zip_read(to_bytes(*str_ore))};
    if (res_zip) {
      const auto res_uid{ogm::map_uid(*res_zip)};
      check(res_uid.has_value() && *res_uid == "b0a8d5bb32ba1eb4fa36b16a198db8b831b37f93",
          "ore-lord uid 黄金锚（== 上游 --map-hash）");
    } else {
      check(false, "ore-lord zip 打开失败");
    }
  }
}

}

int main() {
  run_sha1_unit();
  run_inflate_unit();
  run_zip_unit();
  run_map_bin_unit();
  run_tileset_unit();
  run_map_header_unit();
  run_map_info_golden();

  return ora::test::test_exit_code("map_read");
}
