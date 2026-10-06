// UPSTREAM: OpenRA.Game/Map/Map.cs @b6fc03f(map.hpp 的实现)
//          The implementation of map.hpp.
import std;

#include "map/map.hpp"

#include "core/sha1.hpp"
#include "core/wvec.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "gfx/sequence_set.hpp"
#include "meta/field_loader.hpp"
#include "terrain/terrain_info.hpp"

namespace ora::map {

namespace {

/// .NET char.IsWhiteSpace 的 ASCII + Unicode 子集(MiniYaml 侧同款语义)
/// The ASCII + Unicode subset of .NET char.IsWhiteSpace (the MiniYaml-side
/// semantics).
bool IsWs(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}

std::string_view TrimWs(std::string_view s) {
  while (!s.empty() && IsWs(s.front()))
    s.remove_prefix(1);
  while (!s.empty() && IsWs(s.back()))
    s.remove_suffix(1);
  return s;
}

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

/// GetMapFormat 的行匹配(Map.cs L331 的 Regex "^MapFormat:\s*(\d*)\s*$")
/// The line match of GetMapFormat (Map.cs L331's regex).
std::optional<int> MatchMapFormatLine(std::string_view line) {
  // ^MapFormat:\s*(\d*)\s*$:前缀字面量 + 两侧空白 + 数字组(可空)
  if (!line.starts_with("MapFormat:"))
    return std::nullopt;
  const std::string_view tail = line.substr(std::strlen("MapFormat:"));
  const std::size_t pos_digits_start = tail.find_first_not_of(" \t\n\r\v\f");
  if (pos_digits_start == std::string_view::npos)
    return std::nullopt;
  std::size_t pos_digits_end = pos_digits_start;
  while (pos_digits_end < tail.size() && tail[pos_digits_end] >= '0' &&
         tail[pos_digits_end] <= '9')
    pos_digits_end++;
  if (TrimWs(tail.substr(pos_digits_end)) != tail.substr(pos_digits_end, 0) &&
      !TrimWs(tail.substr(pos_digits_end)).empty())
    return std::nullopt;  // 数字组后仍需 \s*$ | digits must be followed by \s*$
  if (pos_digits_end == pos_digits_start)
    return std::nullopt;  // \d* 至少需要匹配 $ 前内容;空组 = 无效
                          // (上游 (\d*) 可空但随后 \s*$ 必须达行尾;
                          //  "MapFormat:" 空值 → 组空 → GetValue<int>("") 失败
                          //  —— 等价忽略该行)
                          // an empty group fails GetValue<int> upstream —
                          // equivalently ignored here.
  return std::stoi(std::string{tail.substr(pos_digits_start,
                                           pos_digits_end - pos_digits_start)});
}

/// CryptoUtil.SHA1Hash(CryptoUtil.cs L251-258):20 字节摘要 → 小写 hex
/// CryptoUtil.SHA1Hash (CryptoUtil.cs L251-258): the 20-byte digest →
/// lowercase hex.
std::string CryptoSha1Hex(std::span<const unsigned char> digest) {
  static constexpr char kHexLower[] = "0123456789abcdef";
  std::string out;
  out.reserve(digest.size() * 2);
  for (const unsigned char b : digest) {
    out.push_back(kHexLower[b >> 4]);
    out.push_back(kHexLower[b & 0xF]);
  }
  return out;
}

}  // namespace

// ———— BinaryDataHeader(Map.cs L36-58)————
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

// ———— ComputeUID(Map.cs L280-319)————
std::string Map::ComputeUID(const fs::IReadOnlyPackage& package) {
  return ComputeUID(package, GetMapFormat(package));
}

std::string Map::ComputeUID(const fs::IReadOnlyPackage& package, int format) {
  // UID is calculated by taking an SHA1 of the yaml and binary data(上游注释)
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
      // 上游 GetStream 对存在的条目非 null;Contents 列表成员保证命中
      // Upstream GetStream is non-null for existing entries — the Contents
      // membership guarantees the hit.
    }
  }

  // Take the SHA1(上游注释)
  if (streams.empty()) {
    // CryptoUtil.SHA1Hash([])(L306)
    const auto digest = sha1::HashData({});
    return CryptoSha1Hex(digest);
  }
  sha1::Sha1 s;
  for (const auto& data : streams)
    s.Update(std::span<const unsigned char>(
        reinterpret_cast<const unsigned char*>(data.data()), data.size()));
  return CryptoSha1Hex(s.Finish());
}

int Map::GetMapFormat(const fs::IReadOnlyPackage& p) {
  auto yaml_stream = p.GetStream("map.yaml");
  if (!yaml_stream.has_value())
    throw std::runtime_error("Required file map.yaml not present in this map");

  // ReadAllLines 的逐行语义(MiniYaml.FromStream 同源:UTF-8 BOM 剥离 + \r 剥离)
  // The ReadAllLines line semantics (the same source as MiniYaml.FromStream:
  // the UTF-8 BOM strip + \r strip).
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

// ———— Ruleset::Load 的文件系统面(闭包快照)————
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

// ———— 构造族(Map.cs L344-455)————
Map::Map(Params params, const ITerrainInfo& terrain_info, Size size)
    : grid_default_{}, region_all_cells_{MapGridType::Rectangular, CPos::Zero(),
                                         CPos::Zero()},
      params_{std::move(params)} {
  if (params_.mod_data == nullptr)
    throw std::invalid_argument("modData");
  size_map_ = size;
  ptr_grid_ = &params_.mod_data->GetOrCreateMapGrid();

  str_title_ = "Name your map here";  // L350-351
  str_author_ = "Your name here";

  str_tileset_ = terrain_info.Id();

  // Empty rules that can be added to by the importers.(上游注释)
  ptr_rule_definitions_ = std::make_unique<yaml::MiniYaml>();

  ptr_tiles_ = std::make_unique<CellLayer<TerrainTile>>(ptr_grid_->Type, size_map_);
  ptr_resources_ = std::make_unique<CellLayer<ResourceTile>>(ptr_grid_->Type, size_map_);
  ptr_height_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_ramp_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_tiles_->Clear(terrain_info.DefaultTerrainTile());

  if (ptr_grid_->MaximumTerrainHeight() > 0) {
    // L366-369 的挂载序:UpdateRamp → UpdateProjection(×2)
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateRamp(cell); });
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
    ptr_height_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
  }

  PostInit();
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

  // L383-385:map.yaml 解析 + MapField 逐字段 Deserialize(见头注)
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

  // YamlFields 序(Map.cs L161-183;MapField.Deserialize 逐字段)
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

  // Players/Actors(L390-391):Nodes 直取(缺 = 空)
  if (const auto* n = field("Players"))
    vec_player_definitions_ = n->Value.Nodes;
  if (const auto* n = field("Actors"))
    vec_actor_definitions_ = n->Value.Nodes;

  // MiniYaml 型字段(L175-182;required: false)
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

  ptr_grid_ = &params_.mod_data->GetOrCreateMapGrid();  // L393

  ptr_tiles_ = std::make_unique<CellLayer<TerrainTile>>(ptr_grid_->Type, size_map_);
  ptr_resources_ = std::make_unique<CellLayer<ResourceTile>>(ptr_grid_->Type, size_map_);
  ptr_height_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);
  ptr_ramp_ = std::make_unique<CellLayer<std::uint8_t>>(ptr_grid_->Type, size_map_);

  // map.bin(L400-443)
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

        // TODO: Remember to remove this when rewriting tile variants /
        // PickAny(上游注释)
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
        // byte.Clamp(0, MaximumTerrainHeight)(L441)
        const std::uint8_t v = std::min(
            r.ReadU8(), ptr_grid_->MaximumTerrainHeight());
        ptr_height_->Set(MPos{i, j}, v);
      }
  }

  if (ptr_grid_->MaximumTerrainHeight() > 0) {
    // L445-450 的挂载序 | the L445-450 wiring order.
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateRamp(cell); });
    ptr_tiles_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
    ptr_height_->AddCellEntryChangedListener(
        [this](CPos cell) { UpdateProjection(cell); });
  }

  PostInit();

  str_uid_ = ComputeUID(package, int4_map_format_);  // L454
}

Map::~Map() = default;

void Map::Dispose() {
  // L1477-1480:Sequences.Dispose(RAII 形态 = 释放)
  ptr_sequences_.reset();
}

// ———— PostInit(L457-515)————
void Map::PostInit() {
  try {
    ptr_rules_ = game::Ruleset::Load(
        *params_.mod_data, FileSystemFace(), str_tileset_,
        ptr_rule_definitions_.get(), ptr_weapon_definitions_.get(),
        ptr_voice_definitions_.get(), ptr_notification_definitions_.get(),
        ptr_music_definitions_.get(), ptr_model_sequence_definitions_.get());
  } catch (...) {
    // Log.Write("debug") 不落地(D 系日志面);异常快照 + tileset 默认规则
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

  // Replace invalid tiles and cache ramp state(上游注释)
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

  // InvalidateTerrainIndex(L504-514):缓存惰性初始化,事件挂载即时 ——
  // 挂载序保证本 handler 先行(上游注释)
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
  // L517-520
  ptr_ramp_->Set(cell, ptr_rules_->TerrainInfo()
                           .GetTerrainInfo(ptr_tiles_->Get(cell))
                           .RampType);
}

// ———— 投影族(L522-652)————
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

  // Initialize collections(L534-540)
  for (const CPos cell : region_all_cells_) {
    const MPos uv = cell.ToMPos(ptr_grid_->Type);
    ptr_cell_projection_->Set(uv, {});
    ptr_inverse_cell_projection_->Set(uv, std::vector<MPos>{});
  }

  // Initialize projections(L542-543)
  for (const CPos cell : region_all_cells_)
    UpdateProjection(cell);
}

void Map::UpdateProjection(CPos cell) {
  if (ptr_grid_->MaximumTerrainHeight() == 0) {
    // L550-559:平图直投影 | the flat-map direct projection.
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

  // Remove old reverse projection(L566-572)
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

    // Propagate height up cliff faces(上游注释)
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
  // L599-617(const 投影数据只读遍历)
  while (ptr_inverse_cell_projection_->Contains(MPos{puv.U, puv.V})) {
    const auto& inverse = ptr_inverse_cell_projection_->Get(MPos{puv.U, puv.V});
    if (!inverse.empty()) {
      // The original games treat the top of cliffs the same way as the
      // bottom(上游注释;MaxBy(V) = 首遇最大)
      MPos best = inverse.front();
      for (const MPos uv : inverse)
        if (uv.V >= best.V)
          best = uv;
      return static_cast<std::uint8_t>(
          ptr_height_->Get(best) -
          ptr_rules_->TerrainInfo().GetTerrainInfo(ptr_tiles_->Get(best)).Height);
    }

    // Try the next cell down if this is a cliff face(上游注释)
    puv = PPos{puv.U, puv.V + 1};
  }

  return 0;
}

std::vector<PPos> Map::ProjectCellInner(MPos uv) const {
  // L619-652
  const auto& map_height = *ptr_height_;
  if (!map_height.Contains(uv))
    return {};

  const std::uint8_t height = map_height.Get(uv);
  if (height == 0)
    return {PPos{uv.U, uv.V}};

  // Odd-height ramps get bumped up a level to the next even height layer
  // (上游注释)
  std::int32_t int4_height = height;
  if ((int4_height & 1) == 1 && ptr_ramp_->Get(uv) != 0)
    int4_height++;

  std::vector<PPos> candidates;

  // Odd-height level tiles are equally covered by four projected tiles
  // (上游注释)
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

// ———— SaveBinaryData(L708-765)————
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

  // Binary data version(L713)
  write_u8(uint1_tile_format_);

  // Size(L716-718)
  write_u16(static_cast<std::uint16_t>(size_map_.Width));
  write_u16(static_cast<std::uint16_t>(size_map_.Height));

  // Data offsets(L720-727)
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

  // Tile data(L730-741)
  if (kTilesOffset != 0) {
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++) {
        const TerrainTile tile = ptr_tiles_->Get(MPos{i, j});
        write_u16(tile.Type);
        write_u8(tile.Index);
      }
  }

  // Height data(L744-747)
  if (heights_offset != 0)
    for (int i = 0; i < size_map_.Width; i++)
      for (int j = 0; j < size_map_.Height; j++)
        write_u8(ptr_height_->Get(MPos{i, j}));

  // Resource data(L750-761)
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

// ———— GetTerrainColorPair(L767-782)————
std::pair<core::Color, core::Color> Map::GetTerrainColorPair(MPos uv) const {
  const ITerrainInfo& terrain_info = ptr_rules_->TerrainInfo();
  const TerrainTileInfo& type = terrain_info.GetTerrainInfo(ptr_tiles_->Get(uv));
  // Game.CosmeticRandom 的消费面随引擎嵌入(注入点 = GetTerrainColorPair 的
  // 调用方;本批以每调用局部种皮随机承载 —— SavePreview 域消费点未落地)
  // Game.CosmeticRandom rides the engine-embedder face; carried by a local
  // random per call (the SavePreview consumers have not landed).
  MersenneTwister random{0};
  core::Color left = type.GetColor(random);
  core::Color right = type.GetColor(random);

  if (terrain_info.MinHeightColorBrightness() != 1.0f ||
      terrain_info.MaxHeightColorBrightness() != 1.0f) {
    // Util.Lerp(float)(Util.cs: minValue + (maxValue - minValue) * t;
    // float 域内运算)
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

// ———— 包含/换算族(L913-1114)————
bool Map::Contains(CPos cell) const {
  if (ptr_grid_->Type == MapGridType::RectangularIsometric) {
    // .ToMPos() returns the same result if the X and Y coordinates
    // are switched. X < Y is invalid in the RectangularIsometric coordinate
    // system, so we pre-filter these to avoid returning the wrong result
    // (上游注释)
    if (cell.X() < cell.Y())
      return false;
  } else {
    // If the mod uses flat & rectangular maps, ToMPos and Contains(MPos)
    // create unnecessary cost. Just check if CPos is within map bounds.
    // (上游注释)
    if (ptr_grid_->MaximumTerrainHeight() == 0)
      return rect_bounds_.Contains(cell.X(), cell.Y());
  }

  return Contains(cell.ToMPos(ptr_grid_->Type));
}

bool Map::Contains(MPos uv) const {
  // The first check ensures that the cell is within the valid map region,
  // avoiding potential crashes in deeper code. All CellLayers have the same
  // geometry, and CustomTerrain is convenient. (上游注释)
  return ptr_custom_terrain_->Contains(uv) && ContainsAllProjectedCellsCovering(uv);
}

bool Map::ContainsAllProjectedCellsCovering(MPos uv) const {
  // PERF: Checking the bounds directly here is the same as calling
  // Contains((PPos)uv) but saves an allocation (上游注释)
  if (ptr_grid_->MaximumTerrainHeight() == 0)
    return rect_bounds_.Contains(uv.U, uv.V);

  // PERF: Most cells lie within a region where no matter their height,
  // all possible projected cells would remain in the map area. For these,
  // we can do a fast-path check. (上游注释)
  if (rect_projection_safe_bounds_.Contains(uv.U, uv.V))
    return true;

  // Now we need to do a slow-check.(上游注释)
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

  // Convert from isometric cell position (x, y) to world position (u, v)
  // (上游推导注释保留语义)
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
  // L993-1010
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

  // Apply ramp offset(L1017-1027)
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
  // L1030-1036
  std::uint8_t ramp = 0;
  if (ptr_ramp_->TryGetValue(cell, ramp))
    return ptr_grid_->Ramps()[ramp].orientation;
  return WRot::None();
}

WVec Map::Offset(CVec delta, int dz) const {
  // L1038-1044
  if (ptr_grid_->Type == MapGridType::Rectangular)
    return WVec{1024 * delta.X, 1024 * delta.Y, 0};
  return WVec{724 * (delta.X - delta.Y), 724 * (delta.X + delta.Y), 724 * dz};
}

CPos Map::CellContaining(WPos pos) const {
  if (ptr_grid_->Type == MapGridType::Rectangular)
    return CPos{pos.X / 1024, pos.Y / 1024};

  // Convert from world position to isometric cell position (上游推导注释)
  const std::int32_t u =
      (pos.Y + pos.X - 724) / 1448;
  const std::int32_t v =
      (pos.Y - pos.X + (pos.Y > pos.X ? 724 : -724)) / 1448;
  return CPos{u, v};
}

PPos Map::ProjectedCellCovering(WPos pos) const {
  // L1071-1075
  const WPos projected_pos = WPos{pos.X, pos.Y - pos.Z, pos.Z};
  const MPos mp = CellContaining(projected_pos).ToMPos(ptr_grid_->Type);
  return PPos{mp.U, mp.V};
}

std::span<const PPos> Map::ProjectedCellsCovering(MPos uv) {
  // L1078-1087
  if (!b_initialized_cell_projection_)
    InitializeCellProjection();

  if (!ptr_cell_projection_->Contains(uv))
    return {};
  return ptr_cell_projection_->Get(uv);
}

std::vector<MPos> Map::Unproject(PPos puv) {
  // L1089-1100
  const MPos uv = MPos{puv.U, puv.V};

  if (!b_initialized_cell_projection_)
    InitializeCellProjection();

  if (!ptr_inverse_cell_projection_->Contains(uv))
    return {};

  return ptr_inverse_cell_projection_->Get(uv);
}

std::uint8_t Map::ProjectedHeight(PPos puv) const {
  // L1102-1105
  return ptr_projected_height_->Get(MPos{puv.U, puv.V});
}

WAngle Map::FacingBetween(CPos cell, CPos towards, WAngle fallbackfacing) const {
  // L1107-1114:delta = CenterOfCell(towards) - CenterOfCell(cell)(WPos 差
  // → WVec 的语义由分量差承载)
  const WPos from = CenterOfCell(cell);
  const WPos to = CenterOfCell(towards);
  const WVec delta{to.X - from.X, to.Y - from.Y, to.Z - from.Z};
  if (delta.HorizontalLengthSquared() == 0)
    return fallbackfacing;

  return delta.Yaw();
}

// ———— 尺寸/边界(L1116-1174)————
void Map::Resize(int width, int height) {
  // L1116-1133
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
  // The tl and br coordinates are inclusive, but the Rectangle
  // is exclusive. Pad the right and bottom edges to match. (上游注释)
  rect_bounds_ =
      Rectangle::FromLTRB(tl.U, tl.V, br.U + 1, br.V + 1);

  // See ProjectCellInner to see how any given position may be projected.
  // (上游注释保留)
  std::int32_t max_height = ptr_grid_->MaximumTerrainHeight();
  if ((max_height & 1) == 1)
    max_height += 2;
  rect_projection_safe_bounds_ = Rectangle::FromLTRB(
      rect_bounds_.Left() + 1, rect_bounds_.Top() + max_height,
      rect_bounds_.Right() - 1, rect_bounds_.Bottom());

  // Directly calculate the projected map corners in world units(上游注释)
  if (ptr_grid_->Type == MapGridType::RectangularIsometric) {
    wpos_projected_top_left_ = WPos{tl.U * 1448, tl.V * 724, 0};
    wpos_projected_bottom_right_ =
        WPos{br.U * 1448 - 1, (br.V + 1) * 724 - 1, 0};
  } else {
    wpos_projected_top_left_ = WPos{tl.U * 1024, tl.V * 1024, 0};
    wpos_projected_bottom_right_ =
        WPos{br.U * 1024 - 1, (br.V + 1) * 1024 - 1, 0};
  }

  // PERF: This enumeration isn't going to change during the game(上游注释)
  vec_projected_cells_.clear();
  for (const MPos uv : MapCoordsRegion{MPos{tl.U, tl.V}, MPos{br.U, br.V}})
    vec_projected_cells_.push_back(PPos{uv.U, uv.V});
  // 上游 = new ProjectedCellRegion(this, tl, br).ToArray();ProjectedCellRegion
  // 的 MPos 构造路径枚举序 = MapCoordsRegion 同序(行主序),此处直取
  // (the upstream ProjectedCellRegion(MPos) enumeration is row-major —
  // identical order to MapCoordsRegion).
}

// ———— 地形索引(L1176-1210)————
std::uint8_t Map::GetTerrainIndex(CPos cell) const {
  return GetTerrainIndex(cell.ToMPos(ptr_grid_->Type));
}

std::uint8_t Map::GetTerrainIndex(MPos uv) const {
  // Lazily initialize a cache for terrain indexes.(上游注释)
  if (ptr_cached_terrain_indexes_ == nullptr) {
    ptr_cached_terrain_indexes_ = std::make_unique<CellLayer<short>>(
        ptr_grid_->Type, size_map_);
    ptr_cached_terrain_indexes_->Clear(kInvalidCachedTerrainIndex);
  }

  short terrain_index = ptr_cached_terrain_indexes_->Get(uv);

  // PERF: Cache terrain indexes per cell on demand.(上游注释)
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
  // L1207-1210:TerrainTypes[GetTerrainIndex(uv)]
  return ptr_rules_->TerrainInfo().TerrainTypes()[GetTerrainIndex(uv)];
}

// ———— 夹取(L1212-1281)————
CPos Map::Clamp(CPos cell) const {
  return Clamp(cell.ToMPos(ptr_grid_->Type)).ToCPos(ptr_grid_->Type);
}

MPos Map::Clamp(MPos uv) const {
  if (ptr_grid_->MaximumTerrainHeight() == 0)
    return MPos{Clamp(PPos{uv.U, uv.V}).U, Clamp(PPos{uv.U, uv.V}).V};
  // 上游 (MPos)Clamp((PPos)uv) —— PPos/MPos 直转(U/V 各自夹取一次;
  // 上游表达式对同一输入求两次 Clamp 值恒等,此处展开为两次调用同值)
  // Upstream casts the clamped PPos back; calling Clamp twice on the same
  // input yields the same values.

  // Already in bounds, so don't need to do anything.(上游注释)
  if (ContainsAllProjectedCellsCovering(uv))
    return uv;

  // Clamping map coordinates is trickier than it might first look!(上游
  // 注释族保留;三难例与投影变换利用逐行照抄)
  uv = ptr_cell_projection_->Clamp(
      MPos{std::clamp(uv.U, rect_bounds_.Left(), rect_bounds_.Right()), uv.V});

  // Project this guessed cell and take the first available cell(上游注释)
  const std::span<const PPos> all_projected = const_cast<Map*>(this)->ProjectedCellsCovering(uv);
  PPos projected =
      !all_projected.empty()
          ? all_projected[0]
          : PPos{uv.U, std::clamp(uv.V, rect_bounds_.Top(), rect_bounds_.Bottom())};

  // Clamp the projected cell to the map area(上游注释)
  projected = Clamp(projected);

  // Project the cell back into map coordinates.(上游注释)
  std::vector<MPos> un_projected = const_cast<Map*>(this)->Unproject(projected);
  if (un_projected.empty()) {
    // Adjust V until we find a cell that works(上游注释)
    for (int x = 2; x <= 2 * ptr_grid_->MaximumTerrainHeight(); x++) {
      const int dv = ((x & 1) == 1 ? 1 : -1) * x / 2;
      const PPos test{projected.U, projected.V + dv};
      if (!Contains(test))
        continue;

      un_projected = const_cast<Map*>(this)->Unproject(test);
      if (!un_projected.empty())
        break;
    }

    // This shouldn't happen. But if it does, return the original value and
    // hope the caller doesn't explode. (上游注释)
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
  // L1277-1281
  const Rectangle bounds = Rectangle::FromLTRB(
      rect_bounds_.Left(), rect_bounds_.Top(), rect_bounds_.Right() - 1,
      rect_bounds_.Bottom() - 1);
  return puv.Clamp(bounds);
}

// ———— 边格/随机格(L1283-1395)————
CPos Map::ChooseRandomCell(MersenneTwister& rand) {
  // L1283-1296
  std::vector<MPos> cells;
  do {
    const int u = rand.Next(rect_bounds_.Left(), rect_bounds_.Right());
    const int v = rand.Next(rect_bounds_.Top(), rect_bounds_.Bottom());
    cells = Unproject(PPos{u, v});
  } while (cells.empty());

  // Exts.Random(均匀索引)| Exts.Random (uniform index).
  const MPos picked = cells[static_cast<std::size_t>(
      rand.Next(static_cast<std::int32_t>(cells.size())))];
  return picked.ToCPos(ptr_grid_->Type);
}

CPos Map::ChooseClosestEdgeCell(CPos cell) const {
  // L1298-1301
  return ChooseClosestEdgeCell(cell.ToMPos(ptr_grid_->Type))
      .ToCPos(ptr_grid_->Type);
}

MPos Map::ChooseClosestEdgeCell(MPos uv) const {
  // L1303-1347
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
    // Adjust V until we find a cell that works(上游注释)
    for (int x = 2; x <= 2 * ptr_grid_->MaximumTerrainHeight(); x++) {
      const int dv = ((x & 1) == 1 ? 1 : -1) * x / 2;
      const PPos test{edge.U, edge.V + dv};
      if (!Contains(test))
        continue;

      un_projected = const_cast<Map*>(this)->Unproject(test);
      if (!un_projected.empty())
        break;
    }

    // This shouldn't happen.(上游注释)
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
  // L1349-1352:OrderBy(Length).FirstOrDefault(match) —— 稳定序下首个匹配
  std::vector<CPos> sorted = vec_all_edge_cells_;
  std::ranges::stable_sort(sorted, [&](const CPos& a, const CPos& b) {
    return (cell - a).Length() < (cell - b).Length();
  });
  for (const CPos c : sorted)
    if (match(c))
      return c;
  return CPos::Zero();
}
// FirstOrDefault 的"无匹配 = default"以 CPos::Zero 承载(上游 default(CPos))

std::vector<CPos> Map::UpdateEdgeCells() {
  // L1354-1382
  std::vector<CPos> edge_cells;
  std::vector<MPos> un_projected;
  const int bottom = rect_bounds_.Bottom() - 1;
  for (int u = rect_bounds_.Left(); u < rect_bounds_.Right(); u++) {
    un_projected = Unproject(PPos{u, rect_bounds_.Top()});
    if (!un_projected.empty()) {
      // MinBy(V):首遇最小(LINQ MinBy 语义)
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
  // L1389-1395
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
  // L1401-1424:校验两抛点逐字 + 距离桶枚举
  if (max_range < min_range)
    throw std::out_of_range(
        "Maximum range is less than the minimum range.");  // ArgumentOutOfRangeException

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

// ———— IReadOnlyFileSystem 面(L1431-1475)————
std::vector<char> Map::Open(const std::string& filename) const {
  // Explicit package paths never refer to a map(上游注释)
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
  // Packages aren't supported inside maps(上游注释)
  fs::IReadOnlyPackage* pkg = nullptr;
  const bool ok =
      params_.mod_data->ModFiles().TryGetPackageContaining(path, pkg, filename);
  package = pkg;
  return ok;
}

bool Map::TryOpen(const std::string& filename, std::vector<char>& bytes) const {
  // Explicit package paths never refer to a map(上游注释)
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
  // Explicit package paths never refer to a map(上游注释)
  if (filename.find('|') == std::string::npos && ptr_package_ != nullptr &&
      ptr_package_->Contains(filename))
    return true;

  return params_.mod_data->ModFiles().Exists(filename);
}

bool Map::IsExternalFile(const std::string& filename) const {
  // Explicit package paths never refer to a map(上游注释)
  if (filename.find('|') != std::string::npos)
    return false;
  // 上游:modData.DefaultFileSystem.IsExternalFile —— C++ FileSystem 面无
  // 外部文件域(挂载全为包内;Phase 6 content installer 批补),恒 false
  // the upstream IsExternalFile face; the C++ FileSystem has no external-file
  // domain yet (the Phase 6 content-installer batch) — always false.
  return false;
}

CPos Map::RandomOf(std::span<const CPos> cells, MersenneTwister& rand) {
  return cells[static_cast<std::size_t>(
      rand.Next(static_cast<std::int32_t>(cells.size())))];
}

}  // namespace ora::map
