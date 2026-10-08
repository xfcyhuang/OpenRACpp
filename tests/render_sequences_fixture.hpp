// UPSTREAM: NONE(测试设施:真 ra 序列表 + 合成资产副本的序列装配;
//          Phase 5 第八批起,渲染 trait 构造依赖 Map::Sequences —— 各
//          sim 测试共用此夹具装配)
//          Test facility (the real ra sequence tables + synthetic asset
//          copies; from Phase 5 batch 8 the render traits' construction
//          depends on Map::Sequences — the sim tests share this fixture).
#pragma once
import std;

#include <process.h>  // _getpid(ctest 并行的临时目录隔离;白名单头)| _getpid (the parallel ctest temp-dir isolation; a whitelisted header)

#include "formats/lcw.hpp"
#include "formats/shp_td.hpp"
#include "fs/file_system.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_loader.hpp"
#include "map/map.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::testfx {

namespace {

/// 合成 shpTD(seq_test 的 MakeFramesShpTD 同形;64B 帧)
/// The synthetic shpTD (seq_test's MakeFramesShpTD shape; 64-byte frames).
std::vector<std::byte> MakeFramesShpTD(std::uint8_t uint1_seed,
                                       std::int32_t int4_frame_count) {
  auto vec_encoded_all = std::vector<std::byte>{};
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    std::vector<std::byte> vec_frame(64);
    for (auto int4_i = 0; int4_i < 64; int4_i++)
      vec_frame[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 3 + uint1_seed + int4_f * 17) & 0xFF);
    const auto vec_encoded = ora::fmt::lcw::Encode(vec_frame);
    vec_encoded_all.insert(vec_encoded_all.end(), vec_encoded.begin(),
                           vec_encoded.end());
  }

  const std::size_t st_data_base =
      14 + 8 * (static_cast<std::size_t>(int4_frame_count) + 2);
  auto vec_file =
      std::vector<std::byte>(st_data_base + vec_encoded_all.size());
  const auto put_u16 = [&vec_file](std::size_t st_pos, std::uint16_t uint2_v) {
    vec_file[st_pos] = static_cast<std::byte>(uint2_v & 0xFF);
    vec_file[st_pos + 1] = static_cast<std::byte>(uint2_v >> 8);
  };
  const auto put_u32 = [&vec_file](std::size_t st_pos, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_file[st_pos + static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>(uint4_v >> (8 * int4_i));
  };

  put_u16(0, static_cast<std::uint16_t>(int4_frame_count));
  put_u16(6, 8);
  put_u16(8, 8);
  // 逐帧偏移:LCW 编码长累进
  // Per-frame offsets: the LCW-encoded lengths accumulate.
  std::size_t st_off = st_data_base;
  std::size_t st_encoded = 0;
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    put_u32(14 + 8 * static_cast<std::size_t>(int4_f),
            static_cast<std::uint32_t>(st_off) | (0x80u << 24));
    st_off += st_encoded;  // 占位;下方按实际编码长复写 | placeholder;
                           // rewritten below with real lengths
  }
  st_off = st_data_base;
  st_encoded = 0;
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    std::vector<std::byte> vec_frame(64);
    for (auto int4_i = 0; int4_i < 64; int4_i++)
      vec_frame[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 3 + uint1_seed + int4_f * 17) & 0xFF);
    const auto vec_encoded = ora::fmt::lcw::Encode(vec_frame);
    put_u32(14 + 8 * static_cast<std::size_t>(int4_f),
            static_cast<std::uint32_t>(st_data_base + st_encoded) |
                (0x80u << 24));
    st_encoded += vec_encoded.size();
  }
  put_u32(14 + 8 * static_cast<std::size_t>(int4_frame_count),
          static_cast<std::uint32_t>(vec_file.size()));
  put_u32(14 + 8 * (static_cast<std::size_t>(int4_frame_count) + 1), 0);

  std::ranges::copy(vec_encoded_all,
                    vec_file.begin() + static_cast<std::ptrdiff_t>(st_data_base));
  return vec_file;
}

/// ModFiles 读小文本(Open → 全字节;序列 yaml 仅几十 KB)
/// Read a small text off ModFiles (Open → all bytes; the sequence yamls
/// are only tens of KB).
std::string ModFilesReadAll(ora::fs::FileSystem& fs_files,
                            const std::string& str_name) {
  const std::vector<char> vec_bytes = fs_files.Open(str_name);
  return std::string{vec_bytes.begin(), vec_bytes.end()};
}

}  // namespace

/// 真序列表 + 合成资产副本装载到 map。ra 的序列引用 content|*.mix 的
/// 原版资产 —— 无 content 环境按序列表逐 Filename 生成合成 shpTD 副本,
/// Classic 加载器链全真;真资产物化随 Phase 6 的 content 装配。
/// Loads the real sequence tables + synthetic asset copies onto the map.
/// ra's sequences reference the vanilla assets inside content|*.mix — this
/// content-less environment generates a synthetic shpTD copy per
/// referenced Filename while the Classic loader chain stays real; the
/// real-asset materialization rides Phase 6's content assembly.
inline void InstallSyntheticSequences(map::Map& map_world,
                                      const game::Manifest& manifest,
                                      game::ModData& mod_data,
                                      const char* str_upstream_root) {
  // 静态生命周期:SequenceSet/SpriteCache 挂 Map 后仍可能惰性触 FS
  // Static lifetime: the SequenceSet/SpriteCache may still touch the FS
  // lazily after being mounted on the Map.
  static fs::FileSystem file_system_sequences;
  std::vector<std::string> vec_sequence_files = [&] {
    std::vector<std::string> vec_files;
    for (std::string_view sv_name : manifest.Sequences())
      vec_files.push_back(std::string{sv_name}.substr(
          std::string{sv_name}.find('|') + 1));
    return vec_files;
  }();
  {
    // 目录名带进程 id:ctest 并行时各测试互不踩踏(曾致 0xc0000409 竞态)
    // The directory name carries the process id: parallel ctest runs stay
    // out of each other's way (a 0xc0000409 race once lived here).
    const std::filesystem::path dir_sequences =
        std::filesystem::temp_directory_path() /
        (std::string{"ora_render_test_"} +
         std::to_string(static_cast<long long>(
#ifdef _WIN32
             _getpid()
#else
             getpid()
#endif
             )));
    std::filesystem::remove_all(dir_sequences);
    std::filesystem::create_directories(dir_sequences);

    // 收集序列表引用的全部文件名(Filename + TilesetFilenames 值)
    // Collect every filename the sequence tables reference (the Filename
    // + TilesetFilenames values).
    yaml::StringPool pool_yaml;
    std::vector<std::string> vec_referenced;
    for (const std::string& str_file : vec_sequence_files) {
      const std::string str_content =
          ModFilesReadAll(mod_data.ModFiles(), str_file);
      const std::vector<yaml::MiniYamlNode> vec_nodes =
          yaml::MiniYaml::FromStream(str_content, str_file, true, pool_yaml);
      std::function<void(const yaml::MiniYaml&)> visit =
          [&](const yaml::MiniYaml& node) {
            for (const auto& child : node.Nodes) {
              if (child.Key != nullptr &&
                  (*child.Key == "Filename" ||
                   *child.Key == "TilesetFilenames")) {
                if (child.Value.Value != nullptr)
                  vec_referenced.emplace_back(*child.Value.Value);
                for (const auto& sub : child.Value.Nodes)
                  if (sub.Value.Value != nullptr)
                    vec_referenced.emplace_back(*sub.Value.Value);
              }
              visit(child.Value);
            }
          };
      yaml::MiniYaml yaml_root;
      yaml_root.Nodes = vec_nodes;
      visit(yaml_root);
    }
    std::sort(vec_referenced.begin(), vec_referenced.end());
    vec_referenced.erase(
        std::unique(vec_referenced.begin(), vec_referenced.end()),
        vec_referenced.end());

    // 单份合成字节共享(所有引用文件同内容 —— 引用存在性即所测面)
    // One synthetic byte set shared (every referenced file carries the
    // same content — the reference's existence is the tested face).
    const std::vector<std::byte> vec_shp_shared = MakeFramesShpTD(3, 1024);
    const auto write_shared = [&](const std::string& str_asset) {
      const auto path_out = dir_sequences / str_asset;
      std::error_code ec;
      std::filesystem::create_directories(path_out.parent_path(), ec);
      std::ofstream out{path_out, std::ios::binary};
      out.write(reinterpret_cast<const char*>(vec_shp_shared.data()),
                static_cast<std::streamsize>(vec_shp_shared.size()));
    };
    for (const std::string& str_asset : vec_referenced) {
      const std::size_t sz_pipe = str_asset.find('|');
      // 带前缀引用(lores|/content|…)以前缀名挂载的同一合成目录承载
      // A prefixed reference (lores|/content|...) is carried by the same
      // synthetic directory mounted under the prefix name.
      write_shared(sz_pipe == std::string::npos
                       ? str_asset
                       : str_asset.substr(sz_pipe + 1));
    }

    // 合成目录优先(裸文件名);ra|/common| 前缀挂载承载序列 yaml;引用
    // 前缀(lores 等)逐名挂载同目录
    // The synthetic directory first (the bare filenames); the ra|/common|
    // prefixed mounts carry the sequence yamls; each referenced prefix
    // (lores etc.) mounts the same directory under its name.
    std::vector<std::string> vec_prefixes;
    for (const std::string& str_asset : vec_referenced) {
      const std::size_t sz_pipe = str_asset.find('|');
      if (sz_pipe != std::string::npos)
        vec_prefixes.push_back(str_asset.substr(0, sz_pipe));
    }
    std::sort(vec_prefixes.begin(), vec_prefixes.end());
    vec_prefixes.erase(
        std::unique(vec_prefixes.begin(), vec_prefixes.end()),
        vec_prefixes.end());
    file_system_sequences.Mount(dir_sequences.generic_string());
    for (const std::string& str_prefix : vec_prefixes)
      file_system_sequences.Mount(dir_sequences.generic_string(), str_prefix);
    file_system_sequences.Mount(
        std::string{str_upstream_root} + "/mods/ra", "ra");
    file_system_sequences.Mount(
        std::string{str_upstream_root} + "/mods/common", "common");
  }
  auto up_sequence_loader =
      mods::MakeSequenceLoader(std::string{manifest.SpriteSequenceFormat()});
  gfx::SequenceSet::Deps seq_deps;
  seq_deps.ptr_file_system = &file_system_sequences;
  std::vector<gfx::SpriteLoaderFn> vec_sprite_loaders{
      &ora::fmt::TryParseShpTD};
  seq_deps.vec_sprite_loaders = vec_sprite_loaders;
  seq_deps.vec_sequence_files = &vec_sequence_files;
  seq_deps.int4_bgra_sheet_size = 2048;
  seq_deps.int4_indexed_sheet_size = 2048;
  auto up_sequences = std::make_unique<gfx::SequenceSet>(
      seq_deps, *up_sequence_loader, map_world.Tileset());
  up_sequences->LoadSprites();
  map_world.SetSequences(std::move(up_sequences));
  std::println("sequences loaded (tileset {})", map_world.Tileset());
}

}  // namespace ora::testfx
