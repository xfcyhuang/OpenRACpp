import std;

#include "map/map.hpp"

#include "core/wvec.hpp"
#include "formats/png.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "gfx/sequence_set.hpp"
#include "meta/field_loader.hpp"
#include "terrain/terrain_info.hpp"
#include <boost/hash2/sha1.hpp>
#include <boost/regex.hpp>

namespace ora::map {

namespace {

struct StreamReader {
  std::span<const char> bytes;
  std::size_t pos = 0;

  std::uint8_t ReadU8() {
    if (pos + 1 > bytes.size())
      throw std::runtime_error("Invalid tile data");
    return static_cast<std::uint8_t>(bytes[pos++]);
  }
  std::uint16_t ReadU16() {
    if (pos + 2 > bytes.size())
      throw std::runtime_error("Invalid tile data");
    const std::uint16_t v = static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(bytes[pos]) |
        (static_cast<std::uint8_t>(bytes[pos + 1]) << 8));
    pos += 2;
    return v;
  }
  std::uint32_t ReadU32() {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; i++)
      v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[pos + i]))
           << (i * 8);
    pos += 4;
    return v;
  }
};

std::optional<int> MatchMapFormatLine(std::string_view line) {
  static const boost::regex re_format("^MapFormat:\\s*(\\d*)\\s*$");
  boost::cmatch m;
  if (!boost::regex_search(line.data(), line.data() + line.size(), m, re_format))
    return std::nullopt;
  if (m[1].length() == 0)
    return std::nullopt;

  int int4_value = 0;
  if (std::from_chars(m[1].first, m[1].second, int4_value).ec != std::errc{})
    return std::nullopt;
  return int4_value;
}

std::string CryptoSha1Hex(const boost::hash2::digest<20>& digest) {
  static constexpr char kHexLower[] = "0123456789abcdef";
  std::string out;
  out.reserve(digest.size() * 2);
  for (std::size_t st_i = 0; st_i < digest.size(); ++st_i) {
    const unsigned char b = digest.data()[st_i];
    out.push_back(kHexLower[b >> 4]);
    out.push_back(kHexLower[b & 0xF]);
  }
  return out;
}

}

BinaryDataHeader::BinaryDataHeader(std::span<const char> s,
                                   Size expected_size) {
  StreamReader r{s};
  Format = r.ReadU8();
  const std::uint16_t width = r.ReadU16();
  const std::uint16_t height = r.ReadU16();
  if (width != static_cast<std::uint16_t>(expected_size.Width) ||
      height != static_cast<std::uint16_t>(expected_size.Height))
    throw std::runtime_error("Invalid tile data");

  if (Format == 1) {
    TilesOffset = 5;
    HeightsOffset = 0;
    ResourcesOffset = static_cast<std::uint32_t>(3 * width * height + 5);
  } else if (Format == 2) {
    TilesOffset = r.ReadU32();
    HeightsOffset = r.ReadU32();
    ResourcesOffset = r.ReadU32();
  } else {
    throw std::runtime_error(
        std::format("Unknown binary map format '{}'", Format));
  }
}

std::string Map::ComputeUID(const fs::IReadOnlyPackage& package) {
  return ComputeUID(package, GetMapFormat(package));
}

std::string Map::ComputeUID(const fs::IReadOnlyPackage& package, int format) {

  const std::array<std::string_view, 2> required_files{"map.yaml", "map.bin"};
  const std::vector<std::string> contents = package.Contents();
  for (const auto required : required_files) {
    const bool present =
        std::any_of(contents.begin(), contents.end(),
                    [&](const std::string& c) { return c == required; });
    if (!present)
      throw std::runtime_error(
          std::format("Required file {} not present in this map", required));
  }

  std::vector<std::vector<char>> streams;
  for (const auto& filename : contents) {
    const bool is_yaml = filename.ends_with(".yaml");
    const bool is_bin = filename.ends_with(".bin");
    const bool is_lua = filename.ends_with(".lua");
    if (is_yaml || is_bin || is_lua ||
        (format >= 12 && filename == "map.png")) {
      auto data = package.GetStream(filename);
      if (data.has_value())
        streams.push_back(std::move(*data));

    }
  }

  boost::hash2::sha1_160 hasher;
  for (const auto& data : streams)
    hasher.update(reinterpret_cast<const unsigned char*>(data.data()),
                  data.size());
  return CryptoSha1Hex(hasher.result());
}

int Map::GetMapFormat(const fs::IReadOnlyPackage& p) {
  auto yaml_stream = p.GetStream("map.yaml");
  if (!yaml_stream.has_value())
    throw std::runtime_error("Required file map.yaml not present in this map");

  std::string_view bytes{
      yaml_stream->data(), yaml_stream->size()};
  if (bytes.starts_with("\xEF\xBB\xBF"))
    bytes.remove_prefix(3);

  std::size_t pos = 0;
  while (pos <= bytes.size()) {
    const std::size_t pos_eol = bytes.find('\n', pos);
    std::string_view line = bytes.substr(
        pos, pos_eol == std::string_view::npos ? bytes.size() - pos
                                               : pos_eol - pos);
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    if (auto fmt = MatchMapFormatLine(line))
      return *fmt;
    if (pos_eol == std::string_view::npos)
      break;
    pos = pos_eol + 1;
  }

  throw std::runtime_error("MapFormat is not defined");
}

game::MapFileSystemFace Map::FileSystemFace() const {
  game::MapFileSystemFace face;
  face.fn_open = [this](const std::string& filename) { return Open(filename); };
  face.fn_try_open = [this](const std::string& filename,
                            std::vector<char>& bytes) {
    return TryOpen(filename, bytes);
  };
  face.fn_exists = [this](const std::string& filename) {
    return Exists(filename);
  };
  return face;
}

Map::Map(Params params, const ITerrainInfo& terrain_info, Size size)
    : grid_default_{}, region_all_cells_{MapGridType::Rectangular, CPos::Zero(),
                                         CPos::Zero()},
      params_{std::move(params)} {
  if (params_.mod_data == nullptr)
    throw std::invalid_argument("modData");
  size_map_ = size;
  ptr_grid_ = &params_.mod_data->GetOrCreateMapGrid();

  str_title_ = "Name your map here";
  str_author_ = "Your name here";

  str_tileset_ = terrain_info.Id();

  ptr_rule_definitions_ = std::make_unique<yaml::MiniYaml>();

  ptr_tiles_ = std::make_unique<CellLayer<TerrainTile>>(ptr_grid_->Type, size_map_);
  ptr_resources_ = std::make_unique<CellLayer<ResourceTile>>(ptr_grid_->Type, size_map_);
  ptr_height_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_ramp_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_tiles_->Clear(terrain_info.DefaultTerrainTile());

  if (ptr_grid_->MaximumTerrainHeight() > 0) {

    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateRamp(cell); });
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
    ptr_height_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
  }

  PostInit();
}

void Map::SetSequences(std::unique_ptr<gfx::SequenceSet> up_sequences) {
  ptr_sequences_ = std::move(up_sequences);
}

Map::Map(Params params, const fs::IReadOnlyPackage& package)
    : region_all_cells_{MapGridType::Rectangular, CPos::Zero(), CPos::Zero()},
      params_{std::move(params)} {
  if (params_.mod_data == nullptr)
    throw std::invalid_argument("modData");
  ptr_package_ = &package;

  if (!package.Contains("map.yaml") || !package.Contains("map.bin"))
    throw std::runtime_error(std::format("Not a valid map\n File: {}",
                                         package.Name()));

  auto yaml_bytes = package.GetStream("map.yaml");
  const std::vector<yaml::MiniYamlNode> yaml_nodes = yaml::MiniYaml::FromStream(
      std::string_view{yaml_bytes->data(), yaml_bytes->size()},
      std::format("{}:map.yaml", package.Name()), true, pool_yaml_);

  yaml::MiniYaml yaml_root;
  yaml_root.Nodes = yaml_nodes;

  const auto field = [&](std::string_view key) -> const yaml::MiniYamlNode* {
    return yaml_root.NodeWithKeyOrDefault(key);
  };
  const auto require = [&](std::string_view key) {
    throw yaml::YamlException(std::format(
        "Required field `{}` not found in map.yaml", key));
  };
  const auto value_of = [](const yaml::MiniYamlNode& n) -> std::string_view {
    return n.Value.Value != nullptr ? std::string_view{*n.Value.Value}
                                    : std::string_view{};
  };

  if (const auto* n = field("MapFormat"))
    int4_map_format_ = meta::GetInt32Value("MapFormat", value_of(*n));
  else
    require("MapFormat");
  if (const auto* n = field("RequiresMod")) {
    if (n->Value.Value != nullptr)
      str_requires_mod_ = *n->Value.Value;
  } else
    require("RequiresMod");
  if (const auto* n = field("Title")) {
    if (n->Value.Value != nullptr)
      str_title_ = *n->Value.Value;
  } else
    require("Title");
  if (const auto* n = field("Author")) {
    if (n->Value.Value != nullptr)
      str_author_ = *n->Value.Value;
  } else
    require("Author");
  if (const auto* n = field("Tileset")) {
    if (n->Value.Value != nullptr)
      str_tileset_ = *n->Value.Value;
  } else
    require("Tileset");
  if (const auto* n = field("MapSize")) {
    const int2 sz = meta::GetSizeValue("MapSize", value_of(*n));
    size_map_ = Size{sz.X, sz.Y};
  } else {
    require("MapSize");
  }
  if (const auto* n = field("Bounds"))
    rect_bounds_ = meta::GetRectangleValue("Bounds", value_of(*n));
  else
    require("Bounds");
  if (const auto* n = field("Visibility"))
    kind_visibility_ = static_cast<MapVisibility>(meta::GetEnumValue(
        "Visibility", value_of(*n), "OpenRA.MapVisibility"));
  else
    require("Visibility");
  if (const auto* n = field("Categories")) {
    if (n->Value.Value != nullptr)
      vec_categories_ = meta::GetStringArrayValue("Categories", value_of(*n));
  } else
    require("Categories");
  if (const auto* n = field("HideSpawnPreviews"))
    b_hide_spawn_previews_ =
        meta::GetBoolValue("HideSpawnPreviews", value_of(*n));
  if (const auto* n = field("LockPreview"))
    b_lock_preview_ = meta::GetBoolValue("LockPreview", value_of(*n));

  if (const auto* n = field("Players"))
    vec_player_definitions_ = n->Value.Nodes;
  if (const auto* n = field("Actors"))
    vec_actor_definitions_ = n->Value.Nodes;

  const auto copy_yaml = [&](std::string_view key)
      -> std::unique_ptr<yaml::MiniYaml> {
    if (const auto* n = field(key))
      return std::make_unique<yaml::MiniYaml>(n->Value);
    return nullptr;
  };
  ptr_rule_definitions_ = copy_yaml("Rules");
  ptr_fluent_message_definitions_ = copy_yaml("FluentMessages");
  ptr_sequence_definitions_ = copy_yaml("Sequences");
  ptr_model_sequence_definitions_ = copy_yaml("ModelSequences");
  ptr_weapon_definitions_ = copy_yaml("Weapons");
  ptr_voice_definitions_ = copy_yaml("Voices");
  ptr_music_definitions_ = copy_yaml("Music");
  ptr_notification_definitions_ = copy_yaml("Notifications");

  if (int4_map_format_ < kSupportedMapFormat)
    throw std::runtime_error(std::format(
        "Map format {} is not supported.\n File: {}", int4_map_format_,
        package.Name()));

  ptr_grid_ = &params_.mod_data->GetOrCreateMapGrid();

  ptr_tiles_ = std::make_unique<CellLayer<TerrainTile>>(ptr_grid_->Type, size_map_);
  ptr_resources_ = std::make_unique<CellLayer<ResourceTile>>(ptr_grid_->Type, size_map_);
  ptr_height_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_ramp_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);

  auto bin_bytes = package.GetStream("map.bin");
  const BinaryDataHeader header{
      std::span<const char>{bin_bytes->data(), bin_bytes->size()}, size_map_};
  StreamReader r{std::span<const char>{bin_bytes->data(), bin_bytes->size()}};

  if (header.TilesOffset > 0) {
    r.pos = header.TilesOffset;
    for (int i = 0; i < size_map_.Width; i++) {
      for (int j = 0; j < size_map_.Height; j++) {
        std::uint16_t tile = r.ReadU16();
        std::uint8_t index = r.ReadU8();

        if (index == 255)
          index = static_cast<std::uint8_t>(i % 4 + j % 4 * 4);

        ptr_tiles_->Set(MPos{i, j}, TerrainTile{tile, index});
      }
    }
  }

  if (header.ResourcesOffset > 0) {
    r.pos = header.ResourcesOffset;
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++) {
        const std::uint8_t type = r.ReadU8();
        const std::uint8_t density = r.ReadU8();
        ptr_resources_->Set(MPos{i, j}, ResourceTile{type, density});
      }
  }

  if (header.HeightsOffset > 0) {
    r.pos = header.HeightsOffset;
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++) {

        const std::uint8_t v = std::min(
            r.ReadU8(), ptr_grid_->MaximumTerrainHeight());
        ptr_height_->Set(MPos{i, j}, v);
      }
  }

  if (ptr_grid_->MaximumTerrainHeight() > 0) {

    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateRamp(cell); });
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
    ptr_height_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
  }

  PostInit();

  str_uid_ = ComputeUID(package, int4_map_format_);
}

Map::~Map() = default;

void Map::Dispose() {

  ptr_sequences_.reset();
}

void Map::PostInit() {
  try {
    ptr_rules_ = game::Ruleset::Load(
        *params_.mod_data, FileSystemFace(), str_tileset_,
        ptr_rule_definitions_.get(), ptr_weapon_definitions_.get(),
        ptr_voice_definitions_.get(), ptr_notification_definitions_.get(),
        ptr_music_definitions_.get(), ptr_model_sequence_definitions_.get());
  } catch (...) {

    ptr_invalid_custom_rules_exception_ = std::current_exception();
    b_invalid_custom_rules_ = true;
    ptr_rules_ = game::Ruleset::LoadDefaultsForTileSet(*params_.mod_data,
                                                       str_tileset_);
  }

  if (params_.fn_sequences_factory)
    ptr_sequences_ = params_.fn_sequences_factory(*this);

  const CPos tl = MPos{0, 0}.ToCPos(ptr_grid_->Type);
  const CPos br = MPos{size_map_.Width - 1, size_map_.Height - 1}
                      .ToCPos(ptr_grid_->Type);
  region_all_cells_ = CellRegion{ptr_grid_->Type, tl, br};

  SetBounds(PPos{rect_bounds_.Left(), rect_bounds_.Top()},
            PPos{rect_bounds_.Right() - 1, rect_bounds_.Bottom() - 1});

  ptr_custom_terrain_ =
      std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  for (const MPos uv : region_all_cells_.MapCoords())
    ptr_custom_terrain_->Set(uv, static_cast<std::uint8_t>(255));

  const ITerrainInfo& terrain_info = ptr_rules_->TerrainInfo();
  for (const MPos uv : region_all_cells_.MapCoords()) {
    const TerrainTileInfo* info = nullptr;
    if (!terrain_info.TryGetTerrainInfo(ptr_tiles_->Get(uv), info)) {
      vec_replaced_invalid_terrain_tiles_.emplace_back(uv.ToCPos(ptr_grid_->Type),
                                                       ptr_tiles_->Get(uv));
      ptr_tiles_->Set(uv, terrain_info.DefaultTerrainTile());
      info = &terrain_info.GetTerrainInfo(terrain_info.DefaultTerrainTile());
    }

    ptr_ramp_->Set(uv, info->RampType);
  }

  vec_all_edge_cells_ = UpdateEdgeCells();

  ptr_custom_terrain_->AddCellEntryChangedListener(
      [this](CPos c) {
        if (ptr_cached_terrain_indexes_ != nullptr)
          ptr_cached_terrain_indexes_->Set(c, kInvalidCachedTerrainIndex);
      });
  ptr_tiles_->AddCellEntryChangedListener([this](CPos c) {
    if (ptr_cached_terrain_indexes_ != nullptr)
      ptr_cached_terrain_indexes_->Set(c, kInvalidCachedTerrainIndex);
  });
}

void Map::UpdateRamp(CPos cell) {

  ptr_ramp_->Set(cell, ptr_rules_->TerrainInfo()
                           .GetTerrainInfo(ptr_tiles_->Get(cell))
                           .RampType);
}

void Map::InitializeCellProjection() {
  if (b_initialized_cell_projection_)
    return;

  b_initialized_cell_projection_ = true;

  ptr_cell_projection_ = std::make_unique<CellLayer<std::vector<PPos>>>(
      ptr_grid_->Type, size_map_);
  ptr_inverse_cell_projection_ =
      std::make_unique<CellLayer<std::vector<MPos>>>(ptr_grid_->Type, size_map_);
  ptr_projected_height_ =
      std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);

  for (const CPos cell : region_all_cells_) {
    const MPos uv = cell.ToMPos(ptr_grid_->Type);
    ptr_cell_projection_->Set(uv, {});
    ptr_inverse_cell_projection_->Set(uv, std::vector<MPos>{});
  }

  for (const CPos cell : region_all_cells_)
    UpdateProjection(cell);
}

void Map::UpdateProjection(CPos cell) {
  if (ptr_grid_->MaximumTerrainHeight() == 0) {

    const MPos uv = cell.ToMPos(ptr_grid_->Type);
    ptr_cell_projection_->Set(uv, std::vector<PPos>{PPos{uv.U, uv.V}});
    auto& inverse = ptr_inverse_cell_projection_->GetRef(uv);
    inverse.clear();
    inverse.push_back(uv);
    FireCellProjectionChanged(cell);
    return;
  }

  if (!b_initialized_cell_projection_)
    InitializeCellProjection();

  const MPos uv = cell.ToMPos(ptr_grid_->Type);

  for (const PPos puv : ptr_cell_projection_->Get(uv)) {
    const MPos temp = MPos{puv.U, puv.V};
    auto& inverse = ptr_inverse_cell_projection_->GetRef(temp);
    inverse.erase(std::remove(inverse.begin(), inverse.end(), uv),
                  inverse.end());
    ptr_projected_height_->Set(temp, ProjectedCellHeightInner(puv));
  }

  const std::vector<PPos> projected = ProjectCellInner(uv);
  ptr_cell_projection_->Set(uv, projected);

  for (const PPos puv : projected) {
    MPos temp = MPos{puv.U, puv.V};
    ptr_inverse_cell_projection_->Get(temp).push_back(uv);

    const std::uint8_t height = ProjectedCellHeightInner(puv);
    ptr_projected_height_->Set(temp, height);

    while (true) {
      temp = MPos{temp.U, temp.V - 1};
      if (!ptr_inverse_cell_projection_->Contains(temp) ||
          !ptr_inverse_cell_projection_->Get(temp).empty())
        break;

      ptr_projected_height_->Set(temp, height);
    }
  }

  FireCellProjectionChanged(cell);
}

std::uint8_t Map::ProjectedCellHeightInner(PPos puv) const {

  while (ptr_inverse_cell_projection_->Contains(MPos{puv.U, puv.V})) {
    const auto& inverse = ptr_inverse_cell_projection_->Get(MPos{puv.U, puv.V});
    if (!inverse.empty()) {

      MPos best = inverse.front();
      for (const MPos uv : inverse)
        if (uv.V >= best.V)
          best = uv;
      return static_cast<std::uint8_t>(
          ptr_height_->Get(best) -
          ptr_rules_->TerrainInfo().GetTerrainInfo(ptr_tiles_->Get(best)).Height);
    }

    puv = PPos{puv.U, puv.V + 1};
  }

  return 0;
}

std::vector<PPos> Map::ProjectCellInner(MPos uv) const {

  const auto& map_height = *ptr_height_;
  if (!map_height.Contains(uv))
    return {};

  const std::uint8_t height = map_height.Get(uv);
  if (height == 0)
    return {PPos{uv.U, uv.V}};

  std::int32_t int4_height = height;
  if ((int4_height & 1) == 1 && ptr_ramp_->Get(uv) != 0)
    int4_height++;

  std::vector<PPos> candidates;

  if ((int4_height & 1) == 1) {
    if ((uv.V & 1) == 1)
      candidates.emplace_back(uv.U + 1, uv.V - int4_height);
    else
      candidates.emplace_back(uv.U - 1, uv.V - int4_height);

    candidates.emplace_back(uv.U, uv.V - int4_height);
    candidates.emplace_back(uv.U, uv.V - int4_height + 1);
    candidates.emplace_back(uv.U, uv.V - int4_height - 1);
  } else {
    candidates.emplace_back(uv.U, uv.V - int4_height);
  }

  std::erase_if(candidates, [&](const PPos& c) {
    return !map_height.Contains(MPos{c.U, c.V});
  });
  return candidates;
}

std::vector<unsigned char> Map::SaveBinaryData() const {
  std::vector<unsigned char> data;
  const auto write_u8 = [&](std::uint8_t v) { data.push_back(v); };
  const auto write_u16 = [&](std::uint16_t v) {
    data.push_back(static_cast<std::uint8_t>(v));
    data.push_back(static_cast<std::uint8_t>(v >> 8));
  };
  const auto write_u32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; i++)
      data.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
  };

  write_u8(uint1_tile_format_);

  write_u16(static_cast<std::uint16_t>(size_map_.Width));
  write_u16(static_cast<std::uint16_t>(size_map_.Height));

  constexpr std::uint32_t kTilesOffset = 17;
  const std::uint32_t heights_offset =
      ptr_grid_->MaximumTerrainHeight() > 0
          ? 3 * static_cast<std::uint32_t>(size_map_.Width) * size_map_.Height +
                17
          : 0;
  const std::uint32_t resources_offset =
      (ptr_grid_->MaximumTerrainHeight() > 0 ? 4 : 3) *
          static_cast<std::uint32_t>(size_map_.Width) * size_map_.Height +
      17;

  write_u32(kTilesOffset);
  write_u32(heights_offset);
  write_u32(resources_offset);

  if (kTilesOffset != 0) {
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++) {
        const TerrainTile tile = ptr_tiles_->Get(MPos{i, j});
        write_u16(tile.Type);
        write_u8(tile.Index);
      }
  }

  if (heights_offset != 0)
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++)
        write_u8(ptr_height_->Get(MPos{i, j}));

  if (resources_offset != 0) {
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++) {
        const ResourceTile tile = ptr_resources_->Get(MPos{i, j});
        write_u8(tile.Type);
        write_u8(tile.Index);
      }
  }

  return data;
}

std::pair<int, int> Map::GetCellSpaceBounds() {
  int int4_top = std::numeric_limits<int>::max();
  int int4_bottom = std::numeric_limits<int>::min();

  for (int int4_x = rect_bounds_.Left(); int4_x < rect_bounds_.Right(); int4_x++) {
    const std::vector<MPos> vec_all_top = Unproject(PPos{int4_x, rect_bounds_.Top()});
    const std::vector<MPos> vec_all_bottom = Unproject(PPos{int4_x, rect_bounds_.Bottom()});
    if (!vec_all_top.empty()) {
      const MPos& uv_min = *std::min_element(vec_all_top.begin(), vec_all_top.end(),
                                              [](const MPos& uv_a, const MPos& uv_b) {
                                                return uv_a.V < uv_b.V;
                                              });
      int4_top = std::min(int4_top, uv_min.V);
    }

    if (!vec_all_bottom.empty()) {
      const MPos& uv_max = *std::max_element(vec_all_bottom.begin(), vec_all_bottom.end(),
                                              [](const MPos& uv_a, const MPos& uv_b) {
                                                return uv_a.V < uv_b.V;
                                              });
      int4_bottom = std::max(int4_bottom, uv_max.V);
    }
  }

  return {int4_top, int4_bottom};
}

std::vector<std::byte> Map::SavePreview() {
  const bool b_isometric = ptr_grid_->Type == MapGridType::RectangularIsometric;

  int int4_top = 0;
  int int4_bottom = 0;
  if (ptr_grid_->MaximumTerrainHeight() > 0) {
    const auto [int4_top_bounds, int4_bottom_bounds] = GetCellSpaceBounds();
    int4_top = int4_top_bounds;
    int4_bottom = int4_bottom_bounds;

    if (int4_top == std::numeric_limits<int>::max() ||
        int4_bottom == std::numeric_limits<int>::min())
      throw std::runtime_error("The map has invalid boundaries");
  } else {
    int4_top = rect_bounds_.Top();
    int4_bottom = rect_bounds_.Bottom();
  }

  const int int4_width = rect_bounds_.Width;
  const int int4_height = int4_bottom - int4_top;

  const int int4_bitmap_width = b_isometric ? 2 * int4_width - 1 : int4_width;
  const std::size_t int8_stride = static_cast<std::size_t>(int4_bitmap_width) * 4;
  constexpr std::size_t kPxStride = 4;
  std::vector<std::byte> vec_minimap(int8_stride * int4_height);
  std::pair<core::Color, core::Color> pair_terrain{};

  for (int int4_y = 0; int4_y < int4_height; int4_y++) {
    for (int int4_x = 0; int4_x < int4_width; int4_x++) {
      const MPos uv{int4_x + rect_bounds_.Left(), int4_y + int4_top};
      const core::Color color_actor{};
      if (color_actor.A() == 0)
        pair_terrain = GetTerrainColorPair(uv);

      if (b_isometric) {
        const int int4_dx = uv.V & 1;
        const std::size_t int8_x_offset = kPxStride * (2 * int4_x + int4_dx);
        if (int4_x + int4_dx > 0) {
          std::size_t int8_z = static_cast<std::size_t>(int4_y) * int8_stride +
                               int8_x_offset - kPxStride;
          const core::Color color_c = color_actor.A() == 0 ? pair_terrain.first : color_actor;
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.R());
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.G());
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.B());
          vec_minimap[int8_z] = static_cast<std::byte>(color_c.A());
        }

        if (int8_x_offset < int8_stride) {
          std::size_t int8_z = static_cast<std::size_t>(int4_y) * int8_stride + int8_x_offset;
          const core::Color color_c = color_actor.A() == 0 ? pair_terrain.second : color_actor;
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.R());
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.G());
          vec_minimap[int8_z++] = static_cast<std::byte>(color_c.B());
          vec_minimap[int8_z] = static_cast<std::byte>(color_c.A());
        }
      } else {
        std::size_t int8_z =
            static_cast<std::size_t>(int4_y) * int8_stride + kPxStride * int4_x;
        const core::Color color_c = color_actor.A() == 0 ? pair_terrain.first : color_actor;
        vec_minimap[int8_z++] = static_cast<std::byte>(color_c.R());
        vec_minimap[int8_z++] = static_cast<std::byte>(color_c.G());
        vec_minimap[int8_z++] = static_cast<std::byte>(color_c.B());
        vec_minimap[int8_z] = static_cast<std::byte>(color_c.A());
      }
    }
  }

  return fmt::Png{std::move(vec_minimap), gfx::SpriteFrameType::Rgba32, int4_bitmap_width,
                  int4_height}
      .Save();
}

std::pair<core::Color, core::Color> Map::GetTerrainColorPair(MPos uv) const {
  const ITerrainInfo& terrain_info = ptr_rules_->TerrainInfo();
  const TerrainTileInfo& type = terrain_info.GetTerrainInfo(ptr_tiles_->Get(uv));

  MersenneTwister random{0};
  core::Color left = type.GetColor(random);
  core::Color right = type.GetColor(random);

  if (terrain_info.MinHeightColorBrightness() != 1.0f ||
      terrain_info.MaxHeightColorBrightness() != 1.0f) {

    const float t = static_cast<float>(ptr_height_->Get(uv)) /
                    static_cast<float>(ptr_grid_->MaximumTerrainHeight());
    const float scale = terrain_info.MinHeightColorBrightness() +
                        (terrain_info.MaxHeightColorBrightness() -
                         terrain_info.MinHeightColorBrightness()) * t;
    const auto clamp255 = [](int v) { return std::clamp(v, 0, 255); };
    left = core::Color::FromArgb(
        clamp255(static_cast<int>(scale * left.R())),
        clamp255(static_cast<int>(scale * left.G())),
        clamp255(static_cast<int>(scale * left.B())));
    right = core::Color::FromArgb(
        clamp255(static_cast<int>(scale * right.R())),
        clamp255(static_cast<int>(scale * right.G())),
        clamp255(static_cast<int>(scale * right.B())));
  }

  return {left, right};
}

bool Map::Contains(CPos cell) const {
  if (ptr_grid_->Type == MapGridType::RectangularIsometric) {

    if (cell.X() < cell.Y())
      return false;
  } else {

    if (ptr_grid_->MaximumTerrainHeight() == 0)
      return rect_bounds_.Contains(cell.X(), cell.Y());
  }

  return Contains(cell.ToMPos(ptr_grid_->Type));
}

bool Map::Contains(MPos uv) const {

  return ptr_custom_terrain_->Contains(uv) && ContainsAllProjectedCellsCovering(uv);
}

bool Map::ContainsAllProjectedCellsCovering(MPos uv) const {

  if (ptr_grid_->MaximumTerrainHeight() == 0)
    return rect_bounds_.Contains(uv.U, uv.V);

  if (rect_projection_safe_bounds_.Contains(uv.U, uv.V))
    return true;

  auto projected_cells = const_cast<Map*>(this)->ProjectedCellsCovering(uv);
  if (projected_cells.empty())
    return false;

  for (const PPos puv : projected_cells)
    if (!Contains(puv))
      return false;

  return true;
}

bool Map::Contains(PPos puv) const {
  return rect_bounds_.Contains(puv.U, puv.V);
}

WPos Map::CenterOfCell(CPos cell) const {
  if (ptr_grid_->Type == MapGridType::Rectangular)
    return WPos{1024 * cell.X() + 512, 1024 * cell.Y() + 512, 0};

  std::uint8_t height = 0;
  if (ptr_height_->TryGetValue(cell, height)) {
    const std::int32_t z = 724 * height +
                           ptr_grid_->Ramps()[ptr_ramp_->Get(cell)]
                               .int4_center_height_offset;
    return WPos{724 * (cell.X() - cell.Y() + 1), 724 * (cell.X() + cell.Y() + 1),
                z};
  }
  return WPos{724 * (cell.X() - cell.Y() + 1), 724 * (cell.X() + cell.Y() + 1),
              0};
}

WPos Map::CenterOfSubCell(CPos cell, SubCell sub_cell) const {

  const int index = static_cast<int>(sub_cell);
  if (index >= 0 && index < static_cast<int>(MapGrid::kSubCellOffsets.size())) {
    const WPos center = CenterOfCell(cell);
    WVec offset = MapGrid::kSubCellOffsets[index];
    std::uint8_t ramp = 0;
    if (ptr_ramp_->TryGetValue(cell, ramp) && ramp != 0) {
      const CellRamp& r = ptr_grid_->Ramps()[ramp];
      offset = WVec{offset.X, offset.Y,
                    offset.Z + (r.HeightOffset(offset.X, offset.Y) -
                                r.int4_center_height_offset)};
    }

    return WPos{center.X + offset.X, center.Y + offset.Y,
                center.Z + offset.Z};
  }

  return CenterOfCell(cell);
}

WDist Map::DistanceAboveTerrain(WPos pos) const {
  if (ptr_grid_->Type == MapGridType::Rectangular)
    return WDist{pos.Z};

  const CPos cell = CellContaining(pos);
  const WPos center = CenterOfCell(cell);
  const std::int32_t offset_z = pos.Z - center.Z;

  std::uint8_t ramp = 0;
  if (ptr_ramp_->TryGetValue(cell, ramp) && ramp != 0) {
    const CellRamp& r = ptr_grid_->Ramps()[ramp];
    return WDist{offset_z + r.int4_center_height_offset -
                 r.HeightOffset(pos.X - center.X, pos.Y - center.Y)};
  }

  return WDist{offset_z};
}

WRot Map::TerrainOrientation(CPos cell) const {

  std::uint8_t ramp = 0;
  if (ptr_ramp_->TryGetValue(cell, ramp))
    return ptr_grid_->Ramps()[ramp].orientation;
  return WRot::None();
}

WVec Map::Offset(CVec delta, int dz) const {

  if (ptr_grid_->Type == MapGridType::Rectangular)
    return WVec{1024 * delta.X, 1024 * delta.Y, 0};
  return WVec{724 * (delta.X - delta.Y), 724 * (delta.X + delta.Y), 724 * dz};
}

CPos Map::CellContaining(WPos pos) const {
  if (ptr_grid_->Type == MapGridType::Rectangular)
    return CPos{pos.X / 1024, pos.Y / 1024};

  const std::int32_t u =
      (pos.Y + pos.X - 724) / 1448;
  const std::int32_t v =
      (pos.Y - pos.X + (pos.Y > pos.X ? 724 : -724)) / 1448;
  return CPos{u, v};
}

PPos Map::ProjectedCellCovering(WPos pos) const {

  const WPos projected_pos = WPos{pos.X, pos.Y - pos.Z, pos.Z};
  const MPos mp = CellContaining(projected_pos).ToMPos(ptr_grid_->Type);
  return PPos{mp.U, mp.V};
}

std::span<const PPos> Map::ProjectedCellsCovering(MPos uv) {

  if (!b_initialized_cell_projection_)
    InitializeCellProjection();

  if (!ptr_cell_projection_->Contains(uv))
    return {};

  const std::vector<PPos>& vec_cells = ptr_cell_projection_->GetRef(uv);
  return std::span<const PPos>{vec_cells};
}

std::vector<MPos> Map::Unproject(PPos puv) {

  const MPos uv = MPos{puv.U, puv.V};

  if (!b_initialized_cell_projection_)
    InitializeCellProjection();

  if (!ptr_inverse_cell_projection_->Contains(uv))
    return {};

  return ptr_inverse_cell_projection_->Get(uv);
}

std::uint8_t Map::ProjectedHeight(PPos puv) const {

  return ptr_projected_height_->Get(MPos{puv.U, puv.V});
}

WAngle Map::FacingBetween(CPos cell, CPos towards, WAngle fallbackfacing) const {

  const WPos from = CenterOfCell(cell);
  const WPos to = CenterOfCell(towards);
  const WVec delta{to.X - from.X, to.Y - from.Y, to.Z - from.Z};
  if (delta.HorizontalLengthSquared() == 0)
    return fallbackfacing;

  return delta.Yaw();
}

void Map::Resize(int width, int height) {

  const CellLayer<TerrainTile>& old_map_tiles = *ptr_tiles_;
  const CellLayer<ResourceTile>& old_map_resources = *ptr_resources_;
  const CellLayer<std::uint8_t>& old_map_height = *ptr_height_;
  const CellLayer<std::uint8_t>& old_map_ramp = *ptr_ramp_;

  size_map_ = Size{width, height};
  ptr_tiles_ = std::make_unique<CellLayer<TerrainTile>>(
      map::Resize(old_map_tiles, size_map_, old_map_tiles.Get(MPos::Zero())));
  ptr_resources_ = std::make_unique<CellLayer<ResourceTile>>(
      map::Resize(old_map_resources, size_map_,
                  old_map_resources.Get(MPos::Zero())));
  ptr_height_ = std::make_unique<CellLayer<std::uint8_t>>(
      map::Resize(old_map_height, size_map_, old_map_height.Get(MPos::Zero())));
  ptr_ramp_ = std::make_unique<CellLayer<std::uint8_t>>(
      map::Resize(old_map_ramp, size_map_, old_map_ramp.Get(MPos::Zero())));

  const MPos tl = MPos{0, 0};
  const MPos br = MPos{size_map_.Width - 1, size_map_.Height - 1};
  region_all_cells_ = CellRegion{ptr_grid_->Type, tl.ToCPos(ptr_grid_->Type),
                                 br.ToCPos(ptr_grid_->Type)};
  SetBounds(PPos{tl.U + 1, tl.V + 1}, PPos{br.U - 1, br.V - 1});
}

void Map::SetBounds(PPos tl, PPos br) {

  rect_bounds_ =
      Rectangle::FromLTRB(tl.U, tl.V, br.U + 1, br.V + 1);

  std::int32_t max_height = ptr_grid_->MaximumTerrainHeight();
  if ((max_height & 1) == 1)
    max_height += 2;
  rect_projection_safe_bounds_ = Rectangle::FromLTRB(
      rect_bounds_.Left() + 1, rect_bounds_.Top() + max_height,
      rect_bounds_.Right() - 1, rect_bounds_.Bottom());

  if (ptr_grid_->Type == MapGridType::RectangularIsometric) {
    wpos_projected_top_left_ = WPos{tl.U * 1448, tl.V * 724, 0};
    wpos_projected_bottom_right_ =
        WPos{br.U * 1448 - 1, (br.V + 1) * 724 - 1, 0};
  } else {
    wpos_projected_top_left_ = WPos{tl.U * 1024, tl.V * 1024, 0};
    wpos_projected_bottom_right_ =
        WPos{br.U * 1024 - 1, (br.V + 1) * 1024 - 1, 0};
  }

  vec_projected_cells_.clear();
  for (const MPos uv : MapCoordsRegion{MPos{tl.U, tl.V}, MPos{br.U, br.V}})
    vec_projected_cells_.push_back(PPos{uv.U, uv.V});

}

std::uint8_t Map::GetTerrainIndex(CPos cell) const {
  return GetTerrainIndex(cell.ToMPos(ptr_grid_->Type));
}

std::uint8_t Map::GetTerrainIndex(MPos uv) const {

  if (ptr_cached_terrain_indexes_ == nullptr) {
    ptr_cached_terrain_indexes_ = std::make_unique<CellLayer<short>>(
        ptr_grid_->Type, size_map_);
    ptr_cached_terrain_indexes_->Clear(kInvalidCachedTerrainIndex);
  }

  short terrain_index = ptr_cached_terrain_indexes_->Get(uv);

  if (terrain_index == kInvalidCachedTerrainIndex) {
    const std::uint8_t custom = ptr_custom_terrain_->Get(uv);
    terrain_index = custom != 255
                        ? static_cast<short>(custom)
                        : static_cast<short>(ptr_rules_->TerrainInfo()
                                                 .GetTerrainInfo(ptr_tiles_->Get(uv))
                                                 .TerrainType);
    ptr_cached_terrain_indexes_->Set(uv, terrain_index);
  }

  return static_cast<std::uint8_t>(terrain_index);
}

const TerrainTypeInfo& Map::GetTerrainInfo(CPos cell) const {
  return GetTerrainInfo(cell.ToMPos(ptr_grid_->Type));
}

const TerrainTypeInfo& Map::GetTerrainInfo(MPos uv) const {

  return ptr_rules_->TerrainInfo().TerrainTypes()[GetTerrainIndex(uv)];
}

CPos Map::Clamp(CPos cell) const {
  return Clamp(cell.ToMPos(ptr_grid_->Type)).ToCPos(ptr_grid_->Type);
}

MPos Map::Clamp(MPos uv) const {
  if (ptr_grid_->MaximumTerrainHeight() == 0)
    return MPos{Clamp(PPos{uv.U, uv.V}).U, Clamp(PPos{uv.U, uv.V}).V};

  if (ContainsAllProjectedCellsCovering(uv))
    return uv;

  uv = ptr_cell_projection_->Clamp(
      MPos{std::clamp(uv.U, rect_bounds_.Left(), rect_bounds_.Right()), uv.V});

  const std::span<const PPos> all_projected = const_cast<Map*>(this)->ProjectedCellsCovering(uv);
  PPos projected =
      !all_projected.empty()
          ? all_projected[0]
          : PPos{uv.U, std::clamp(uv.V, rect_bounds_.Top(), rect_bounds_.Bottom())};

  projected = Clamp(projected);

  std::vector<MPos> un_projected = const_cast<Map*>(this)->Unproject(projected);
  if (un_projected.empty()) {

    for (int x = 2; x <= 2 * ptr_grid_->MaximumTerrainHeight(); x++) {
      const int dv = ((x & 1) == 1 ? 1 : -1) * x / 2;
      const PPos test{projected.U, projected.V + dv};
      if (!Contains(test))
        continue;

      un_projected = const_cast<Map*>(this)->Unproject(test);
      if (!un_projected.empty())
        break;
    }

    if (un_projected.empty()) {
      return uv;
    }
  }

  return projected.V == rect_bounds_.Bottom()
             ? *std::max_element(un_projected.begin(), un_projected.end(),
                                 [](const MPos& a, const MPos& b) {
                                   return a.V < b.V;
                                 })
             : *std::min_element(un_projected.begin(), un_projected.end(),
                                 [](const MPos& a, const MPos& b) {
                                   return a.V < b.V;
                                 });
}

PPos Map::Clamp(PPos puv) const {

  const Rectangle bounds = Rectangle::FromLTRB(
      rect_bounds_.Left(), rect_bounds_.Top(), rect_bounds_.Right() - 1,
      rect_bounds_.Bottom() - 1);
  return puv.Clamp(bounds);
}

CPos Map::ChooseRandomCell(MersenneTwister& rand) {

  std::vector<MPos> cells;
  do {
    const int u = rand.Next(rect_bounds_.Left(), rect_bounds_.Right());
    const int v = rand.Next(rect_bounds_.Top(), rect_bounds_.Bottom());
    cells = Unproject(PPos{u, v});
  } while (cells.empty());

  const MPos picked = cells[static_cast<std::size_t>(
      rand.Next(static_cast<std::int32_t>(cells.size())))];
  return picked.ToCPos(ptr_grid_->Type);
}

CPos Map::ChooseClosestEdgeCell(CPos cell) const {

  return ChooseClosestEdgeCell(cell.ToMPos(ptr_grid_->Type))
      .ToCPos(ptr_grid_->Type);
}

MPos Map::ChooseClosestEdgeCell(MPos uv) const {

  const std::span<const PPos> all_projected = const_cast<Map*>(this)->ProjectedCellsCovering(uv);

  PPos edge;
  if (!all_projected.empty()) {
    const PPos puv = all_projected[0];
    const int horizontal_bound =
        (puv.U - rect_bounds_.Left() < rect_bounds_.Width / 2)
            ? rect_bounds_.Left()
            : rect_bounds_.Right();
    const int vertical_bound =
        (puv.V - rect_bounds_.Top() < rect_bounds_.Height / 2)
            ? rect_bounds_.Top()
            : rect_bounds_.Bottom();

    const int du = std::abs(horizontal_bound - puv.U);
    const int dv = std::abs(vertical_bound - puv.V);

    edge = du < dv ? PPos{horizontal_bound, puv.V} : PPos{puv.U, vertical_bound};
  } else {
    edge = PPos{rect_bounds_.Left(), rect_bounds_.Top()};
  }

  std::vector<MPos> un_projected = const_cast<Map*>(this)->Unproject(edge);
  if (un_projected.empty()) {

    for (int x = 2; x <= 2 * ptr_grid_->MaximumTerrainHeight(); x++) {
      const int dv = ((x & 1) == 1 ? 1 : -1) * x / 2;
      const PPos test{edge.U, edge.V + dv};
      if (!Contains(test))
        continue;

      un_projected = const_cast<Map*>(this)->Unproject(test);
      if (!un_projected.empty())
        break;
    }

    if (un_projected.empty())
      return uv;
  }

  return edge.V == rect_bounds_.Bottom()
             ? *std::max_element(un_projected.begin(), un_projected.end(),
                                 [](const MPos& a, const MPos& b) {
                                   return a.V < b.V;
                                 })
             : *std::min_element(un_projected.begin(), un_projected.end(),
                                 [](const MPos& a, const MPos& b) {
                                   return a.V < b.V;
                                 });
}

CPos Map::ChooseClosestMatchingEdgeCell(
    CPos cell, const std::function<bool(CPos)>& match) const {

  std::vector<CPos> sorted = vec_all_edge_cells_;
  std::ranges::stable_sort(sorted, [&](const CPos& a, const CPos& b) {
    return (cell - a).Length() < (cell - b).Length();
  });
  for (const CPos c : sorted)
    if (match(c))
      return c;
  return CPos::Zero();
}

std::vector<CPos> Map::UpdateEdgeCells() {

  std::vector<CPos> edge_cells;
  std::vector<MPos> un_projected;
  const int bottom = rect_bounds_.Bottom() - 1;
  for (int u = rect_bounds_.Left(); u < rect_bounds_.Right(); u++) {
    un_projected = Unproject(PPos{u, rect_bounds_.Top()});
    if (!un_projected.empty()) {

      MPos best = un_projected.front();
      for (const MPos uv : un_projected)
        if (uv.V < best.V)
          best = uv;
      edge_cells.push_back(best.ToCPos(ptr_grid_->Type));
    }

    un_projected = Unproject(PPos{u, bottom});
    if (!un_projected.empty()) {
      MPos best = un_projected.front();
      for (const MPos uv : un_projected)
        if (uv.V >= best.V)
          best = uv;
      edge_cells.push_back(best.ToCPos(ptr_grid_->Type));
    }
  }

  for (int v = rect_bounds_.Top(); v < rect_bounds_.Bottom(); v++) {
    un_projected = Unproject(PPos{rect_bounds_.Left(), v});
    if (!un_projected.empty()) {
      const bool b_bottom = v == bottom;
      MPos best = un_projected.front();
      for (const MPos uv : un_projected)
        if (b_bottom ? uv.V >= best.V : uv.V < best.V)
          best = uv;
      edge_cells.push_back(best.ToCPos(ptr_grid_->Type));
    }

    un_projected = Unproject(PPos{rect_bounds_.Right() - 1, v});
    if (!un_projected.empty()) {
      const bool b_bottom = v == bottom;
      MPos best = un_projected.front();
      for (const MPos uv : un_projected)
        if (b_bottom ? uv.V >= best.V : uv.V < best.V)
          best = uv;
      edge_cells.push_back(best.ToCPos(ptr_grid_->Type));
    }
  }

  return edge_cells;
}

WDist Map::DistanceToEdge(WPos pos, const WVec& dir) const {

  const WPos projected_pos = WPos{pos.X, pos.Y - pos.Z, pos.Z};
  const std::int32_t x =
      dir.X == 0 ? std::numeric_limits<std::int32_t>::max()
                 : ((dir.X < 0 ? wpos_projected_top_left_.X
                               : wpos_projected_bottom_right_.X) -
                    projected_pos.X) /
                       dir.X;
  const std::int32_t y =
      dir.Y == 0 ? std::numeric_limits<std::int32_t>::max()
                 : ((dir.Y < 0 ? wpos_projected_top_left_.Y
                               : wpos_projected_bottom_right_.Y) -
                    projected_pos.Y) /
                       dir.Y;
  return WDist{std::min(x, y) * dir.Length()};
}

std::vector<CPos> Map::FindTilesInAnnulus(CPos center, int min_range,
                                          int max_range,
                                          bool allow_outside_bounds) const {

  if (max_range < min_range)
    throw std::out_of_range(
        "Maximum range is less than the minimum range.");

  if (max_range >= static_cast<int>(ptr_grid_->TilesByDistance().size()))
    throw std::out_of_range(std::format(
        "The requested range ({}) cannot exceed the value of "
        "MaximumTileSearchRange ({})",
        max_range, ptr_grid_->MaximumTileSearchRange()));

  std::vector<CPos> out;
  for (int i = min_range; i <= max_range; i++) {
    for (const CVec& offset : ptr_grid_->TilesByDistance()[i]) {
      const CPos t = offset + center;
      if (allow_outside_bounds ? ptr_tiles_->Contains(t) : Contains(t))
        out.push_back(t);
    }
  }

  return out;
}

std::vector<char> Map::Open(const std::string& filename) const {

  if (filename.find('|') == std::string::npos && ptr_package_ != nullptr &&
      ptr_package_->Contains(filename)) {
    auto data = ptr_package_->GetStream(filename);
    if (data.has_value())
      return std::move(*data);
  }

  return params_.mod_data->ModFiles().Open(filename);
}

bool Map::TryGetPackageContaining(const std::string& path,
                                  const fs::IReadOnlyPackage*& package,
                                  std::string& filename) const {

  fs::IReadOnlyPackage* pkg = nullptr;
  const bool ok =
      params_.mod_data->ModFiles().TryGetPackageContaining(path, pkg, filename);
  package = pkg;
  return ok;
}

bool Map::TryOpen(const std::string& filename, std::vector<char>& bytes) const {

  if (filename.find('|') == std::string::npos && ptr_package_ != nullptr) {
    auto data = ptr_package_->GetStream(filename);
    if (data.has_value()) {
      bytes = std::move(*data);
      return true;
    }
  }

  return params_.mod_data->ModFiles().TryOpen(filename, bytes);
}

bool Map::Exists(const std::string& filename) const {

  if (filename.find('|') == std::string::npos && ptr_package_ != nullptr &&
      ptr_package_->Contains(filename))
    return true;

  return params_.mod_data->ModFiles().Exists(filename);
}

bool Map::IsExternalFile(const std::string& filename) const {

  if (filename.find('|') != std::string::npos)
    return false;

  return false;
}

CPos Map::RandomOf(std::span<const CPos> cells, MersenneTwister& rand) {
  return cells[static_cast<std::size_t>(
      rand.Next(static_cast<std::int32_t>(cells.size())))];
}

}
