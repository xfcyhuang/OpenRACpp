#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/mersenne_twister.hpp"
#include "core/size.hpp"
#include "fs/file_system.hpp"
#include "terrain/tile_reference.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::map {

constexpr std::string_view kTerrainPaletteInternalName = "terrain";

struct Riser {

  enum class Connection : std::int32_t {
    UL = 0,
    UR = 1,
    RU = 2,
    RD = 3,
    DR = 4,
    DL = 5,
    LD = 6,
    LU = 7,
  };

  static constexpr std::uint8_t kDefault = 255;

  std::uint64_t uint8_bits = std::numeric_limits<std::uint64_t>::max();

  Riser() = default;

  explicit Riser(const yaml::MiniYaml* my);

  std::optional<std::uint8_t> At(int i) const {
    if (i < 0 || i >= 8)
      throw std::out_of_range("Index was outside the bounds of the array.");
    const auto b = static_cast<std::uint8_t>((uint8_bits >> (i * 8)) & 0xFF);
    if (b != kDefault)
      return b;
    return std::nullopt;
  }

  std::optional<std::uint8_t> At(Connection c) const {
    return At(static_cast<int>(c));
  }
};

class TerrainTileInfo {
 public:

  std::uint8_t TerrainType{255};
  std::uint8_t Height{0};
  std::uint8_t RampType{0};
  core::Color MinColor{core::Color::FromArgbRaw(0)};
  core::Color MaxColor{core::Color::FromArgbRaw(0)};
  Riser riser{};

  virtual ~TerrainTileInfo() = default;

  core::Color GetColor(MersenneTwister& random) const {
    if (MinColor != MaxColor)
      return ColorLerp(random.NextFloat(), MinColor, MaxColor);
    return MinColor;
  }

  static core::Color ColorLerp(float t, core::Color c1, core::Color c2) {
    return core::Color::FromArgb(
        static_cast<int>(t * c2.A() + (1 - t) * c1.A()),
        static_cast<int>(t * c2.R() + (1 - t) * c1.R()),
        static_cast<int>(t * c2.G() + (1 - t) * c1.G()),
        static_cast<int>(t * c2.B() + (1 - t) * c1.B()));
  }
};

class TerrainTypeInfo {
 public:

  explicit TerrainTypeInfo(const yaml::MiniYaml& my);

  std::string Type{};
  std::uint64_t uint8_target_types{0};

  std::vector<std::string> vec_accepts_smudge_type{};
  core::Color Color{core::Color::FromArgbRaw(0)};
  bool b_restrict_player_color{false};
};

class ITerrainInfo {
 public:
  virtual ~ITerrainInfo() = default;

  virtual const std::string& Id() const = 0;
  virtual const std::string& Name() const = 0;
  virtual Size TileSize() const = 0;
  virtual std::span<const TerrainTypeInfo> TerrainTypes() const = 0;
  virtual const TerrainTileInfo& GetTerrainInfo(TerrainTile r) const = 0;
  virtual bool TryGetTerrainInfo(TerrainTile r,
                                 const TerrainTileInfo*& info_out) const = 0;
  virtual std::uint8_t GetTerrainIndex(std::string_view str_type) const = 0;
  virtual std::uint8_t GetTerrainIndex(TerrainTile r) const = 0;
  virtual TerrainTile DefaultTerrainTile() const = 0;

  virtual std::span<const core::Color> HeightDebugColors() const = 0;
  virtual std::vector<core::Color> RestrictedPlayerColors() const = 0;
  virtual float MinHeightColorBrightness() const = 0;
  virtual float MaxHeightColorBrightness() const = 0;
};

class TerrainTemplateInfo {
 public:
  TerrainTemplateInfo(const ITerrainInfo& terrain_info, const yaml::MiniYaml& my);

  std::uint16_t Id{0};
  int2 size_{0, 0};
  bool b_pick_any{false};
  std::vector<std::string> vec_categories{};

  const TerrainTileInfo* TileAt(int index) const {
    return vec_tile_info_[static_cast<std::size_t>(index)].get();
  }

  bool Contains(int index) const {
    return index >= 0 && index < static_cast<int>(vec_tile_info_.size());
  }

  int TilesCount() const { return static_cast<int>(vec_tile_info_.size()); }

 protected:

  virtual std::unique_ptr<TerrainTileInfo> LoadTileInfo(
      const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const;

 private:
  std::vector<std::unique_ptr<TerrainTileInfo>> vec_tile_info_;
};

class DefaultTerrainTileInfo final : public TerrainTileInfo {
 public:
  float ZOffset{0.0f};
  float ZRamp{1.0f};
};

class DefaultTerrainTemplateInfo final : public TerrainTemplateInfo {
 public:
  using TerrainTemplateInfo::TerrainTemplateInfo;

  std::vector<std::string> vec_images{};
  std::vector<std::string> vec_depth_images{};
  std::vector<std::int32_t> vec_frames{};
  std::string str_palette{};

 protected:
  std::unique_ptr<TerrainTileInfo> LoadTileInfo(
      const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const override;
};

class DefaultTerrain final : public ITerrainInfo {
 public:
  explicit DefaultTerrain(const fs::FileSystem& file_system,
                          const std::string& str_filepath);
  DefaultTerrain(std::span<const char> yaml_bytes,
                 const std::string& str_filepath);

  const std::string& Id() const override { return str_id_; }
  const std::string& Name() const override { return str_name_; }
  Size TileSize() const override { return size_; }
  std::span<const TerrainTypeInfo> TerrainTypes() const override {
    return vec_terrain_info_;
  }
  const TerrainTileInfo& GetTerrainInfo(TerrainTile r) const override {
    return *GetTileInfo(r);
  }
  bool TryGetTerrainInfo(TerrainTile r,
                         const TerrainTileInfo*& info_out) const override {
    return TryGetTileInfo(r, info_out);
  }
  std::span<const core::Color> HeightDebugColors() const override {
    return vec_height_debug_colors_;
  }
  std::vector<core::Color> RestrictedPlayerColors() const override;
  float MinHeightColorBrightness() const override {
    return fp4_min_height_color_brightness_;
  }
  float MaxHeightColorBrightness() const override {
    return fp4_max_height_color_brightness_;
  }
  TerrainTile DefaultTerrainTile() const override {

    return TerrainTile{vec_templates_in_definition_order_.front()->Id, 0};
  }

  const TerrainTypeInfo& operator[](std::uint8_t index) const {
    return vec_terrain_info_[index];
  }

  std::uint8_t GetTerrainIndex(std::string_view str_type) const override;
  std::uint8_t GetTerrainIndex(TerrainTile r) const override;

  const TerrainTileInfo* GetTileInfo(TerrainTile r) const {
    return Templates().at(r.Type)->TileAt(r.Index);
  }
  bool TryGetTileInfo(TerrainTile r, const TerrainTileInfo*& info_out) const {
    const auto it = map_templates_.find(r.Type);
    if (it == map_templates_.end() || !it->second->Contains(r.Index)) {
      info_out = nullptr;
      return false;
    }
    info_out = it->second->TileAt(r.Index);
    return info_out != nullptr;
  }

  const std::unordered_map<std::uint16_t, TerrainTemplateInfo*>&
  Templates() const {
    return map_templates_;
  }
  const std::vector<std::unique_ptr<TerrainTemplateInfo>>&
  TemplatesInDefinitionOrder() const {
    return vec_templates_in_definition_order_;
  }
  const std::vector<std::string>& EditorTemplateOrder() const {
    return vec_editor_template_order_;
  }

  Size size_{24, 24};
  std::int32_t int4_sheet_size{512};
  std::vector<core::Color> vec_height_debug_colors_{
      core::Color::FromArgb(255, 255, 0, 0)};
  std::string str_editor_palette_{};
  bool b_ignore_tile_sprite_offsets{false};
  bool b_enable_depth{false};
  float fp4_min_height_color_brightness_{1.0f};
  float fp4_max_height_color_brightness_{1.0f};

 private:
  void Parse(std::span<const char> yaml_bytes, const std::string& str_filepath);

  std::string str_name_;
  std::string str_id_;
  std::vector<std::string> vec_editor_template_order_{};

  std::vector<TerrainTypeInfo> vec_terrain_info_{};
  std::unordered_map<std::uint16_t, TerrainTemplateInfo*>
      map_templates_;
  std::vector<std::unique_ptr<TerrainTemplateInfo>>
      vec_templates_in_definition_order_{};
  std::unordered_map<std::string, std::uint8_t> map_terrain_index_by_type_;
  std::uint8_t uint1_default_walkable_terrain_index_{0};
};

}
