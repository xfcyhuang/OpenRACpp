#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game::replay {

constexpr std::int32_t kMetaStartMarker{-1};
constexpr std::int32_t kMetaEndMarker{-2};
constexpr std::int32_t kMetaVersion{1};

struct ReplayRecord {
  std::int32_t int4_client{};
  std::vector<std::uint8_t> vec_data;
  std::size_t idx_offset{};
};

struct ReplayFile {
  std::vector<ReplayRecord> vec_records;
  bool b_has_metadata{};
  std::vector<MiniYamlNode> vec_meta_nodes;
  std::int32_t int4_final_game_tick{};
};

[[nodiscard]] std::expected<ReplayFile, std::string> replay_read(
    std::span<const std::uint8_t> span_bytes);

[[nodiscard]] std::vector<std::uint8_t> replay_write(const ReplayFile& file);

[[nodiscard]] std::optional<std::string_view> meta_find_value(
    const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_parent,
    std::string_view str_key);

}
