module ora.game.world;

import std;
import ora.game.miniyaml;

namespace ora::game::replay {

namespace {

void append_i4le(std::vector<std::uint8_t>& vec_out, std::int32_t int4_value) {
  const std::uint32_t u4{static_cast<std::uint32_t>(int4_value)};
  for (std::uint32_t idx{}; idx < 4; ++idx)
    vec_out.push_back(static_cast<std::uint8_t>((u4 >> (8 * idx)) & 0xff));
}

[[nodiscard]] std::optional<std::int32_t> read_i4le(std::span<const std::uint8_t> span_bytes,
                                                    std::size_t idx_offset) {
  if (idx_offset + 4 > span_bytes.size()) return std::nullopt;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(span_bytes[idx_offset]) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 1]) << 8) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 2]) << 16) |
                                   (static_cast<std::uint32_t>(span_bytes[idx_offset + 3]) << 24));
}

}

std::optional<std::string_view> meta_find_value(const std::vector<MiniYamlNode>& vec_nodes,
                                                std::string_view str_parent,
                                                std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes) {
    if (node.str_key != str_parent) continue;
    for (const MiniYamlNode& child : node.vec_children)
      if (child.str_key == str_key) return child.str_value;
    return std::nullopt;
  }
  return std::nullopt;
}

std::expected<ReplayFile, std::string> replay_read(std::span<const std::uint8_t> span_bytes) {
  ReplayFile file;
  std::size_t idx_pos{};

  while (true) {
    const auto opt_client{read_i4le(span_bytes, idx_pos)};
    if (!opt_client.has_value())
      return std::unexpected(std::format("replay: no metadata block (truncated at byte {})",
                                         idx_pos));
    if (*opt_client == kMetaStartMarker) break;

    const auto opt_len{read_i4le(span_bytes, idx_pos + 4)};
    if (!opt_len.has_value() || *opt_len < 0)
      return std::unexpected(
          std::format("replay: truncated record header at byte {}", idx_pos));

    const std::size_t sz_len{static_cast<std::size_t>(*opt_len)};
    if (idx_pos + 8 + sz_len > span_bytes.size())
      return std::unexpected(std::format("replay: record data overruns file at byte {}", idx_pos));

    ReplayRecord record;
    record.int4_client = *opt_client;
    record.vec_data.assign(span_bytes.begin() + static_cast<std::ptrdiff_t>(idx_pos + 8),
                           span_bytes.begin() + static_cast<std::ptrdiff_t>(idx_pos + 8 + sz_len));
    record.idx_offset = idx_pos;
    file.vec_records.push_back(std::move(record));
    idx_pos += 8 + sz_len;
  }

  const auto opt_version{read_i4le(span_bytes, idx_pos + 4)};
  const auto opt_yaml_len{read_i4le(span_bytes, idx_pos + 8)};
  if (!opt_version.has_value() || !opt_yaml_len.has_value())
    return std::unexpected("replay: truncated metadata header");
  if (*opt_version != kMetaVersion)
    return std::unexpected(std::format("replay: metadata version {} unsupported", *opt_version));

  const std::size_t sz_yaml{static_cast<std::size_t>(*opt_yaml_len)};
  if (idx_pos + 12 + sz_yaml + 8 > span_bytes.size())
    return std::unexpected("replay: metadata yaml overruns file");

  const auto opt_data_len{read_i4le(span_bytes, idx_pos + 12 + sz_yaml)};
  const auto opt_end_marker{read_i4le(span_bytes, idx_pos + 12 + sz_yaml + 4)};
  if (!opt_data_len.has_value() || !opt_end_marker.has_value())
    return std::unexpected("replay: truncated metadata tail");
  if (*opt_data_len != static_cast<std::int32_t>(4 + sz_yaml))
    return std::unexpected(std::format("replay: metadata dataLength {} != 4 + {}",
                                       *opt_data_len, sz_yaml));
  if (*opt_end_marker != kMetaEndMarker)
    return std::unexpected("replay: metadata end marker mismatch");
  if (idx_pos + 12 + sz_yaml + 8 != span_bytes.size())
    return std::unexpected("replay: trailing bytes after metadata block");

  const std::string str_yaml{
      std::string_view{reinterpret_cast<const char*>(span_bytes.data() + idx_pos + 12), sz_yaml}};
  auto res_nodes{miniyaml_from_string(str_yaml, "orarep-metadata")};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());

  file.b_has_metadata = true;
  file.vec_meta_nodes = std::move(*res_nodes);
  const auto opt_tick{meta_find_value(file.vec_meta_nodes, "Root", "FinalGameTick")};
  if (opt_tick.has_value() && !opt_tick->empty()) {
    std::int32_t int4_tick{};
    const auto res_chars{
        std::from_chars(opt_tick->data(), opt_tick->data() + opt_tick->size(), int4_tick)};
    if (res_chars.ec == std::errc{} && res_chars.ptr == opt_tick->data() + opt_tick->size())
      file.int4_final_game_tick = int4_tick;
  }
  return file;
}

std::vector<std::uint8_t> replay_write(const ReplayFile& file) {
  std::vector<std::uint8_t> vec_out;
  for (const ReplayRecord& record : file.vec_records) {
    append_i4le(vec_out, record.int4_client);
    append_i4le(vec_out, static_cast<std::int32_t>(record.vec_data.size()));
    vec_out.insert(vec_out.end(), record.vec_data.begin(), record.vec_data.end());
  }

  if (file.b_has_metadata) {
    const std::string str_yaml{miniyaml_write_to_string(file.vec_meta_nodes)};
    append_i4le(vec_out, kMetaStartMarker);
    append_i4le(vec_out, kMetaVersion);
    append_i4le(vec_out, static_cast<std::int32_t>(str_yaml.size()));
    vec_out.insert(vec_out.end(), str_yaml.begin(), str_yaml.end());
    append_i4le(vec_out, static_cast<std::int32_t>(4 + str_yaml.size()));
    append_i4le(vec_out, kMetaEndMarker);
  }
  return vec_out;
}

}
