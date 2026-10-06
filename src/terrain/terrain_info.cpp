// UPSTREAM: OpenRA.Game/Map/TerrainInfo.cs @b6fc03f + OpenRA.Mods.Common/
//          Terrain/{TerrainInfo,DefaultTerrain}.cs @b6fc03f(terrain_info.hpp
//          的实现) | The implementation of terrain_info.hpp.
import std;

#include "terrain/terrain_info.hpp"

#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::map {

namespace {

/// node 值文本(Node.Value 可空)| a node's value text (Node.Value nullable).
std::string_view ValueOf(const yaml::MiniYamlNode& node) {
  return node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                     : std::string_view{};
}

}  // namespace

// ———— Riser(TerrainInfo.cs L82-131)————
Riser::Riser(const yaml::MiniYaml* my) {
  if (my == nullptr)
    return;
  const std::string* definition = my->Value;
  if (definition == nullptr)
    return;

  const std::string_view s{*definition};

  // 长式:逗号 8 段(L90-104)
  // The long form: 8 comma parts (L90-104).
  const auto parts = meta::SplitComma(s);
  if (parts.size() == 8) {
    uint8_bits = 0;
    for (int i = 0; i < 8; i++) {
      std::uint8_t b = 0;
      if (!meta::TryParseByteInvariant(parts[i], b))
        throw yaml::YamlException(
            std::format("`{}` is not a valid Riser definition", *definition));
      uint8_bits |= static_cast<std::uint64_t>(b) << (i * 8);
    }
    return;
  }

  // 短式:"LU=6"(L106-128)
  // The short form: "LU=6" (L106-128).
  const std::size_t pos_eq = s.find('=');
  if (pos_eq != std::string_view::npos) {
    std::uint8_t b = 0;
    if (!meta::TryParseByteInvariant(s.substr(pos_eq + 1), b))
      throw yaml::YamlException(
          std::format("`{}` is not a valid Riser definition", *definition));

    uint8_bits = static_cast<std::uint64_t>(b) * 0x0101010101010101ull;

    // TODO: make stricter(上游注释;大小写不敏感包含检查)
    std::string lower_key{s.substr(0, pos_eq)};
    std::ranges::transform(lower_key, lower_key.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    if (lower_key.find('u') == std::string_view::npos)
      uint8_bits |= 0x00'00'00'00'00'00'ff'ffull;
    if (lower_key.find('r') == std::string_view::npos)
      uint8_bits |= 0x00'00'00'00'ff'ff'00'00ull;
    if (lower_key.find('d') == std::string_view::npos)
      uint8_bits |= 0x00'00'ff'ff'00'00'00'00ull;
    if (lower_key.find('l') == std::string_view::npos)
      uint8_bits |= 0xff'ff'00'00'00'00'00'00ull;
    return;
  }

  throw yaml::YamlException(
      std::format("`{}` is not a valid Riser definition", *definition));
}

// ———— TerrainTypeInfo(TerrainInfo.cs L189)————
TerrainTypeInfo::TerrainTypeInfo(const yaml::MiniYaml& my) {
  // FieldLoader.Load(this, my):Type/TargetTypes/AcceptsSmudgeType/Color/
  // RestrictPlayerColor 全带默认(非 Required)
  // FieldLoader.Load(this, my): every field defaulted (nothing Required).
  if (const auto* node = my.NodeWithKeyOrDefault("Type")) {
    if (node->Value.Value != nullptr)
      Type = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("TargetTypes")) {
    if (node->Value.Value != nullptr)
      uint8_target_types = meta::BitsOf(
          "OpenRA.Traits.TargetableType", meta::GetStringArrayValue(
                                              "TargetTypes", ValueOf(*node)));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("AcceptsSmudgeType")) {
    if (node->Value.Value != nullptr)
      vec_accepts_smudge_type =
          meta::GetStringArrayValue("AcceptsSmudgeType", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Color")) {
    if (node->Value.Value != nullptr)
      Color = meta::GetColorValue("Color", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("RestrictPlayerColor")) {
    if (node->Value.Value != nullptr)
      b_restrict_player_color =
          meta::GetBoolValue("RestrictPlayerColor", ValueOf(*node));
  }
}

// ———— TerrainTemplateInfo(Mods.Common/Terrain/TerrainInfo.cs L38-96)————
TerrainTemplateInfo::TerrainTemplateInfo(const ITerrainInfo& terrain_info,
                                         const yaml::MiniYaml& my) {
  // FieldLoader.Load(this, my):Id/Size/PickAny/Categories
  if (const auto* node = my.NodeWithKeyOrDefault("Id"))
    Id = static_cast<std::uint16_t>(
        meta::GetInt32Value("Id", ValueOf(*node)));
  if (const auto* node = my.NodeWithKeyOrDefault("Size"))
    size_ = meta::GetSizeValue("Size", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("PickAny"))
    b_pick_any = meta::GetBoolValue("PickAny", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Categories")) {
    if (node->Value.Value != nullptr)
      vec_categories = meta::GetStringArrayValue("Categories", ValueOf(*node));
  }

  const yaml::MiniYamlNode& tiles_node = my.NodeWithKey("Tiles");
  const auto& nodes = tiles_node.Value.Nodes;

  if (!b_pick_any) {
    vec_tile_info_.resize(static_cast<std::size_t>(size_.X) *
                          static_cast<std::size_t>(size_.Y));
    for (const auto& node : nodes) {
      int key = 0;
      if (node.Key == nullptr ||
          !meta::TryParseInt32Invariant(*node.Key, key))
        throw yaml::YamlException(std::format(
            "Tileset `{}` template `{}` defines a frame `{}` that is not a "
            "valid integer.",
            terrain_info.Id(), Id,
            node.Key != nullptr ? std::string_view{*node.Key}
                                : std::string_view{}));

      if (key < 0 || key >= static_cast<int>(vec_tile_info_.size()))
        throw yaml::YamlException(std::format(
            "Tileset `{}` template `{}` references frame {}, but only "
            "[0..{}] are valid for a {}x{} Size template.",
            terrain_info.Id(), Id, key,
            static_cast<int>(vec_tile_info_.size()) - 1, size_.X, size_.Y));

      vec_tile_info_[static_cast<std::size_t>(key)] =
          LoadTileInfo(terrain_info, node.Value);
    }
  } else {
    vec_tile_info_.resize(nodes.size());

    int i = 0;
    for (const auto& node : nodes) {
      int key = 0;
      if (node.Key == nullptr ||
          !meta::TryParseInt32Invariant(*node.Key, key))
        throw yaml::YamlException(std::format(
            "Tileset `{}` template `{}` defines a frame `{}` that is not a "
            "valid integer.",
            terrain_info.Id(), Id,
            node.Key != nullptr ? std::string_view{*node.Key}
                                : std::string_view{}));

      if (key != i++)
        throw yaml::YamlException(std::format(
            "Tileset `{}` template `{}` is missing a definition for frame {}.",
            terrain_info.Id(), Id, i - 1));

      vec_tile_info_[static_cast<std::size_t>(key)] =
          LoadTileInfo(terrain_info, node.Value);
    }
  }
}

std::unique_ptr<TerrainTileInfo> TerrainTemplateInfo::LoadTileInfo(
    const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const {
  // L83-96:FieldLoader.Load + TerrainType 名换索引 + 颜色回落
  auto tile = std::make_unique<TerrainTileInfo>();
  if (const auto* node = my.NodeWithKeyOrDefault("Height"))
    tile->Height =
        static_cast<std::uint8_t>(meta::GetInt32Value("Height", ValueOf(*node)));
  if (const auto* node = my.NodeWithKeyOrDefault("RampType"))
    tile->RampType = static_cast<std::uint8_t>(
        meta::GetInt32Value("RampType", ValueOf(*node)));
  if (const auto* node = my.NodeWithKeyOrDefault("MinColor")) {
    if (node->Value.Value != nullptr)
      tile->MinColor = meta::GetColorValue("MinColor", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("MaxColor")) {
    if (node->Value.Value != nullptr)
      tile->MaxColor = meta::GetColorValue("MaxColor", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Riser"))
    tile->riser = Riser{&node->Value};

  // Terrain type must be converted from a string to a
  // n index(上游注释;LoadUsing LoadRiser 之外的反射面)
  const std::string_view tileset_name =
      my.Value != nullptr ? std::string_view{*my.Value} : std::string_view{};
  tile->TerrainType = terrain_info.GetTerrainIndex(tileset_name);

  // Fall back to the terrain-type color if necessary(上游注释)
  const core::Color override_color =
      terrain_info.TerrainTypes()[tile->TerrainType].Color;
  if (tile->MinColor == core::Color::FromArgbRaw(0))
    tile->MinColor = override_color;
  if (tile->MaxColor == core::Color::FromArgbRaw(0))
    tile->MaxColor = override_color;

  return tile;
}

std::unique_ptr<TerrainTileInfo> DefaultTerrainTemplateInfo::LoadTileInfo(
    const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const {
  // DefaultTerrain.cs L52-69:直接子类加载(FieldLoader.Load(tile, my) 的
  // 字段集 = 基类四项 + ZOffset/ZRamp)+ TerrainType 换索引 + 颜色回落
  // DefaultTerrain.cs L52-69: the direct subclass load (the FieldLoader
  // field set = the base four + ZOffset/ZRamp) + the TerrainType index
  // conversion + the color fallback.
  auto tile = std::make_unique<DefaultTerrainTileInfo>();
  if (const auto* node = my.NodeWithKeyOrDefault("Height"))
    tile->Height =
        static_cast<std::uint8_t>(meta::GetInt32Value("Height", ValueOf(*node)));
  if (const auto* node = my.NodeWithKeyOrDefault("RampType"))
    tile->RampType = static_cast<std::uint8_t>(
        meta::GetInt32Value("RampType", ValueOf(*node)));
  if (const auto* node = my.NodeWithKeyOrDefault("MinColor")) {
    if (node->Value.Value != nullptr)
      tile->MinColor = meta::GetColorValue("MinColor", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("MaxColor")) {
    if (node->Value.Value != nullptr)
      tile->MaxColor = meta::GetColorValue("MaxColor", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Riser"))
    tile->riser = Riser{&node->Value};
  if (const auto* node = my.NodeWithKeyOrDefault("ZOffset"))
    tile->ZOffset = meta::GetFloatValue("ZOffset", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("ZRamp"))
    tile->ZRamp = meta::GetFloatValue("ZRamp", ValueOf(*node));

  // Terrain type must be converted from a string to an index(上游注释)
  const std::string_view tileset_name =
      my.Value != nullptr ? std::string_view{*my.Value} : std::string_view{};
  tile->TerrainType = terrain_info.GetTerrainIndex(tileset_name);

  // Fall back to the terrain-type color if necessary(上游注释)
  const core::Color override_color =
      terrain_info.TerrainTypes()[tile->TerrainType].Color;
  if (tile->MinColor == core::Color::FromArgbRaw(0))
    tile->MinColor = override_color;
  if (tile->MaxColor == core::Color::FromArgbRaw(0))
    tile->MaxColor = override_color;

  return tile;
}

// ———— DefaultTerrain(DefaultTerrain.cs L99-151)————
DefaultTerrain::DefaultTerrain(const fs::FileSystem& file_system,
                               const std::string& str_filepath) {
  Parse(file_system.Open(str_filepath), str_filepath);
}

DefaultTerrain::DefaultTerrain(std::span<const char> yaml_bytes,
                               const std::string& str_filepath) {
  Parse(yaml_bytes, str_filepath);
}

void DefaultTerrain::Parse(std::span<const char> yaml_bytes,
                           const std::string& str_filepath) {
  // MiniYaml.FromStream(...).ToDictionary(x => x.Key, x => x.Value)(L101-102)
  const std::vector<yaml::MiniYamlNode> nodes = yaml::MiniYaml::FromStream(
      std::string_view{yaml_bytes.data(), yaml_bytes.size()}, str_filepath);

  yaml::MiniYaml root;
  root.Nodes = nodes;
  const auto yaml = root.ToDictionary();

  const auto find = [&](std::string_view key) -> const yaml::MiniYaml* {
    for (const auto& [k, v] : yaml)
      if (k == key)
        return v;
    return nullptr;
  };

  // General info(L105)
  const yaml::MiniYaml* general = find("General");
  if (general != nullptr) {
    if (const auto* node = general->NodeWithKeyOrDefault("Name")) {
      if (node->Value.Value != nullptr)
        str_name_ = *node->Value.Value;
    }
    if (const auto* node = general->NodeWithKeyOrDefault("Id")) {
      if (node->Value.Value != nullptr)
        str_id_ = *node->Value.Value;
    }
    if (const auto* node = general->NodeWithKeyOrDefault("TileSize")) {
      // Size("w,h") 经 int2 解析面(同形) | the Size ("w,h") parse rides
      // the int2 face (same shape).
      const int2 sz = meta::GetSizeValue("TileSize", ValueOf(*node));
      size_ = Size{sz.X, sz.Y};
    }
    if (const auto* node = general->NodeWithKeyOrDefault("SheetSize"))
      int4_sheet_size =
          meta::GetInt32Value("SheetSize", ValueOf(*node));
    if (const auto* node = general->NodeWithKeyOrDefault("HeightDebugColors")) {
      if (node->Value.Value != nullptr) {
        vec_height_debug_colors_.clear();
        for (const std::string_view sv : meta::SplitComma(ValueOf(*node)))
          vec_height_debug_colors_.push_back(
              meta::GetColorValue("HeightDebugColors", sv));
      }
    }
    if (const auto* node = general->NodeWithKeyOrDefault("EditorTemplateOrder")) {
      if (node->Value.Value != nullptr)
        vec_editor_template_order_ =
            meta::GetStringArrayValue("EditorTemplateOrder", ValueOf(*node));
    }
    if (const auto* node = general->NodeWithKeyOrDefault("IgnoreTileSpriteOffsets"))
      b_ignore_tile_sprite_offsets = meta::GetBoolValue(
          "IgnoreTileSpriteOffsets", ValueOf(*node));
    if (const auto* node = general->NodeWithKeyOrDefault("EnableDepth"))
      b_enable_depth = meta::GetBoolValue("EnableDepth", ValueOf(*node));
    if (const auto* node =
            general->NodeWithKeyOrDefault("MinHeightColorBrightness"))
      fp4_min_height_color_brightness_ = meta::GetFloatValue(
          "MinHeightColorBrightness", ValueOf(*node));
    if (const auto* node =
            general->NodeWithKeyOrDefault("MaxHeightColorBrightness"))
      fp4_max_height_color_brightness_ = meta::GetFloatValue(
          "MaxHeightColorBrightness", ValueOf(*node));
    if (const auto* node = general->NodeWithKeyOrDefault("Palette")) {
      if (node->Value.Value != nullptr)
        str_editor_palette_ = *node->Value.Value;
    } else {
      str_editor_palette_ = std::string{kTerrainPaletteInternalName};
    }
  } else {
    str_editor_palette_ = std::string{kTerrainPaletteInternalName};
  }

  // TerrainTypes(L108-125):按 Type 名排序
  // TerrainTypes (L108-125): ordered by the Type name.
  const yaml::MiniYaml* terrain = find("Terrain");
  if (terrain != nullptr) {
    for (const auto& [key, value] : terrain->ToDictionary())
      vec_terrain_info_.emplace_back(*value);
    std::ranges::sort(vec_terrain_info_, {},
                      [](const TerrainTypeInfo& tt) { return tt.Type; });
  }

  if (vec_terrain_info_.size() >= 255)
    throw yaml::YamlException("Too many terrain types.");

  std::unordered_map<std::string, std::uint8_t> tiby;
  tiby.reserve(vec_terrain_info_.size());
  for (std::uint8_t i = 0; i < vec_terrain_info_.size(); i++) {
    const std::string& tt = vec_terrain_info_[i].Type;
    if (!tiby.emplace(tt, i).second)
      throw yaml::YamlException(std::format(
          "Duplicate terrain type '{}' in '{}'.", tt, str_filepath));
  }
  map_terrain_index_by_type_ = std::move(tiby);

  uint1_default_walkable_terrain_index_ = GetTerrainIndex("Clear");  // L127

  // Templates(L130-134)
  const yaml::MiniYaml* templates = find("Templates");
  if (templates != nullptr) {
    for (const auto& node : templates->Nodes) {
      auto tpl =
          std::make_unique<DefaultTerrainTemplateInfo>(*this, node.Value);
      vec_templates_in_definition_order_.push_back(std::move(tpl));
    }
    // 模板所有权在定义序表(L89);字典视图持裸指针(上游两视图共享引用)
    // Ownership stays in the definition-order vector (L89); the dictionary
    // view holds raw pointers (upstream's two views share references).
    for (auto& tpl : vec_templates_in_definition_order_)
      map_templates_.emplace(tpl->Id, tpl.get());
  }
}

std::uint8_t DefaultTerrain::GetTerrainIndex(std::string_view str_type) const {
  // L157-164:缺失 InvalidDataException 同文本
  const auto it = map_terrain_index_by_type_.find(std::string{str_type});
  if (it == map_terrain_index_by_type_.end())
    throw std::runtime_error(std::format(
        "Tileset '{}' lacks terrain type '{}'", str_id_, str_type));
  return it->second;
}

std::uint8_t DefaultTerrain::GetTerrainIndex(TerrainTile r) const {
  // L166-172
  const TerrainTileInfo* tile = GetTileInfo(r);
  if (tile->TerrainType != 255)
    return tile->TerrainType;
  return uint1_default_walkable_terrain_index_;
}

std::vector<core::Color> DefaultTerrain::RestrictedPlayerColors() const {
  // L211(TerrainInfo.Where(ti => ti.RestrictPlayerColor).Select(ti => ti.Color))
  std::vector<core::Color> out;
  for (const TerrainTypeInfo& ti : vec_terrain_info_)
    if (ti.b_restrict_player_color)
      out.push_back(ti.Color);
  return out;
}

}  // namespace ora::map
