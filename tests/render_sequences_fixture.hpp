#pragma once
import std;

#include <process.h>

#include "formats/lcw.hpp"
#include "formats/shp_td.hpp"
#include "fs/file_system.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_loader.hpp"
#include "map/map.hpp"
#include "mods/render_sprites.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "sim/trait_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::testfx {

namespace {

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

  std::size_t st_off = st_data_base;
  std::size_t st_encoded = 0;
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    put_u32(14 + 8 * static_cast<std::size_t>(int4_f),
            static_cast<std::uint32_t>(st_off) | (0x80u << 24));
    st_off += st_encoded;

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

std::string ModFilesReadAll(ora::fs::FileSystem& fs_files,
                            const std::string& str_name) {
  const std::vector<char> vec_bytes = fs_files.Open(str_name);
  return std::string{vec_bytes.begin(), vec_bytes.end()};
}

}

inline gfx::SequenceSet* g_ptr_golden_sequences = nullptr;

inline int ResolveGoldenFacings(const ora::game::ActorInfo& actor,
                                const std::string& str_faction) {
  if (g_ptr_golden_sequences == nullptr)
    return 8;

  std::string str_sequence = "idle";
  const meta::RecordObject* rec_render_sprites = nullptr;
  for (const auto* rec : actor.TraitsInConstructOrder()) {
    const std::string_view str_full =
        rec->record_desc().str_full_name;
    if (str_full ==
        std::string_view{
            "OpenRA.Mods.Common.Traits.QuantizeFacingsFromSequenceInfo"}) {
      if (const auto v = ora::sim::RecordFieldString(*rec, "Sequence"))
        if (!v->empty())
          str_sequence = std::string{*v};
    } else if (str_full ==
               std::string_view{
                   "OpenRA.Mods.Common.Traits.Render.RenderSpritesInfo"}) {
      rec_render_sprites = rec;
    }
  }

  if (rec_render_sprites == nullptr)
    return 8;

  const std::string str_image =
      ora::mods::RenderSpritesInfoData::Parse(*rec_render_sprites)
          .GetImage(actor, str_faction);
  return g_ptr_golden_sequences->GetSequence(str_image, str_sequence)
      .Facings();
}

inline void InstallSyntheticSequences(map::Map& map_world,
                                      const game::Manifest& manifest,
                                      game::ModData& mod_data,
                                      const char* str_upstream_root) {

  static fs::FileSystem file_system_sequences;
  std::vector<std::string> vec_sequence_files = [&] {
    std::vector<std::string> vec_files;
    for (std::string_view sv_name : manifest.Sequences())
      vec_files.push_back(std::string{sv_name}.substr(
          std::string{sv_name}.find('|') + 1));
    return vec_files;
  }();
  {

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

      write_shared(sz_pipe == std::string::npos
                       ? str_asset
                       : str_asset.substr(sz_pipe + 1));
    }

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
  g_ptr_golden_sequences = map_world.Sequences();
  std::println("sequences loaded (tileset {})", map_world.Tileset());
}

}
