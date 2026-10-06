// UPSTREAM: OpenRA.Mods.Common/Graphics/TilesetSpecificSpriteSequence.cs @b6fc03f L20-93
//          (tileset_specific_sprite_sequence.hpp 的实现)
//          Implementation of tileset_specific_sprite_sequence.hpp.
import std;
#include "mods/tileset_specific_sprite_sequence.hpp"

namespace ora::mods {

std::unique_ptr<gfx::ISpriteSequence> TilesetSpecificSpriteSequenceLoader::CreateSequence(
    gfx::SpriteCache& cache_sprites, const std::string& str_image, const std::string& str_sequence,
    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) {
  return std::make_unique<TilesetSpecificSpriteSequence>(cache_sprites, *this, str_image,
                                                         str_sequence, yaml_data, yaml_defaults);
}

namespace {

/// tileset 命中节点的文件名/位置提取(protected 构造面由成员侧完成)。
/// The filename/location extraction of a tileset-hit node (the protected
/// construction face completes on the member side).
std::string FilenameOf(const yaml::MiniYamlNode& node_tileset) {
  return node_tileset.Value.Value != nullptr ? *node_tileset.Value.Value : std::string{};
}

}  // namespace

std::vector<DefaultSpriteSequence::ReservationInfo> TilesetSpecificSpriteSequence::ParseFilenames(
    const std::string& str_tile_set, const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) {
  // TilesetFilenamesPattern 优先(L43-55)
  // TilesetFilenamesPattern takes precedence (L43-55).
  const yaml::MiniYamlNode* node_pattern =
      FindFieldNode("TilesetFilenamesPattern", yaml_data, &yaml_defaults);
  if (node_pattern != nullptr) {
    const yaml::MiniYamlNode* node_tileset = node_pattern->Value.NodeWithKeyOrDefault(str_tile_set);
    if (node_tileset != nullptr) {
      const std::int32_t int4_pattern_start =
          LoadInt32("Start", 0, node_tileset->Value, nullptr);
      const std::int32_t int4_pattern_count = LoadInt32("Count", 1, node_tileset->Value, nullptr);

      std::vector<ReservationInfo> vec_infos;
      for (std::int32_t int4_i = int4_pattern_start;
           int4_i < int4_pattern_start + int4_pattern_count; int4_i++) {
        ReservationInfo info;
        info.str_filename = std::vformat(
            node_tileset->Value.Value != nullptr ? *node_tileset->Value.Value : std::string{},
            std::make_format_args(int4_i));
        info.opt_vec_load_frames = std::vector<std::int32_t>{0};
        info.opt_vec_frames = std::vector<std::int32_t>{0};
        info.location_source = node_tileset->Location;
        vec_infos.push_back(std::move(info));
      }
      return vec_infos;
    }
  }

  const yaml::MiniYamlNode* node_tileset_filenames =
      FindFieldNode("TilesetFilenames", yaml_data, &yaml_defaults);
  if (node_tileset_filenames != nullptr) {
    const yaml::MiniYamlNode* node_tileset =
        node_tileset_filenames->Value.NodeWithKeyOrDefault(str_tile_set);
    if (node_tileset != nullptr) {
      const std::int32_t int4_stride =
          opt_int4_stride_.has_value() ? *opt_int4_stride_ : opt_int4_length_.value_or(0);
      ReservationInfo info;
      info.str_filename = FilenameOf(*node_tileset);
      info.location_source = node_tileset->Location;
      info.opt_vec_load_frames =
          CalculateFrameIndices(int4_start_, opt_int4_length_, int4_stride, int4_facings_,
                                opt_vec_frames, b_transpose_, b_reverse_facings_, int4_shadow_start_);
      info.opt_vec_frames = opt_vec_frames;
      return {std::move(info)};
    }
  }

  return DefaultSpriteSequence::ParseFilenames(str_tile_set, opt_vec_frames, yaml_data,
                                               yaml_defaults);
}

std::vector<DefaultSpriteSequence::ReservationInfo>
TilesetSpecificSpriteSequence::ParseCombineFilenames(
    const std::string& str_tile_set, const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
    const yaml::MiniYaml& yaml_data) {
  // Combine 子节点的 TilesetFilenames 只看 data(L73-88)
  // Combine's sub-node TilesetFilenames reads data only (L73-88).
  const yaml::MiniYamlNode* node_tileset_filenames =
      FindFieldNode("TilesetFilenames", yaml_data, nullptr);
  if (node_tileset_filenames != nullptr) {
    const yaml::MiniYamlNode* node_tileset =
        node_tileset_filenames->Value.NodeWithKeyOrDefault(str_tile_set);
    if (node_tileset != nullptr) {
      ReservationInfo info;
      info.str_filename = FilenameOf(*node_tileset);
      info.location_source = node_tileset->Location;

      std::optional<std::vector<std::int32_t>> opt_vec_frames_local = opt_vec_frames;
      const yaml::MiniYamlNode* node_length = FindFieldNode("Length", yaml_data, nullptr);
      const bool b_length_star =
          node_length != nullptr && node_length->Value.Value != nullptr &&
          std::string_view{*node_length->Value.Value} == "*";
      if (!opt_vec_frames_local.has_value() && !b_length_star) {
        const std::int32_t int4_sub_start = LoadInt32("Start", 0, yaml_data, nullptr);
        const std::int32_t int4_sub_length = LoadInt32("Length", 1, yaml_data, nullptr);
        std::vector<std::int32_t> vec_frames;
        for (std::int32_t int4_i{}; int4_i < int4_sub_length; int4_i++)
          vec_frames.push_back(int4_sub_start + int4_i);
        opt_vec_frames_local = std::move(vec_frames);
      }

      info.opt_vec_load_frames = opt_vec_frames_local;
      info.opt_vec_frames = opt_vec_frames_local;
      return {std::move(info)};
    }
  }

  return DefaultSpriteSequence::ParseCombineFilenames(str_tile_set, opt_vec_frames, yaml_data);
}

}  // namespace ora::mods
