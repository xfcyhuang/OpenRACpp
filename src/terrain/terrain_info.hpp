// UPSTREAM: OpenRA.Game/Map/TerrainInfo.cs @b6fc03f L22-198(逐语义重写)+
//          OpenRA.Mods.Common/Terrain/TerrainInfo.cs L31-120(TerrainTemplateInfo)
//          + OpenRA.Mods.Common/Terrain/DefaultTerrain.cs L26-262(DefaultTerrain)
//          Verbatim-semantics rewrite.
//
// 已登记偏离(docs/COVERAGE.md,第十六批)/ Registered deviations (batch 16):
//  - DefaultTerrain 的 MultiBrushCollections(地图生成域)与 DumpSheets
//    (Utility 命令域)不解析 —— 本批无消费点
//    DefaultTerrain's MultiBrushCollections (map-generation domain) and
//    DumpSheets (the Utility-command domain) stay unparsed — no call sites
//    in this batch.
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

/// TileSet(TerrainInfo.cs L194-197):调色板内部名常量
/// TileSet (TerrainInfo.cs L194-197): the internal palette-name constant.
constexpr std::string_view kTerrainPaletteInternalName = "terrain";

/// Riser(TerrainInfo.cs L52-153):相邻格角部高程连接(8 连接位包)
/// Riser (TerrainInfo.cs L52-153): neighbor tile corner-height connections
/// (8 packed connection slots).
struct Riser {
  /// Connection(TerrainInfo.cs L59-69) | Connection (TerrainInfo.cs L59-69).
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

  std::uint64_t uint8_bits = std::numeric_limits<std::uint64_t>::max();  // L73

  Riser() = default;

  /// Riser(MiniYaml)(L82-131):长式 "6,6,0,0,0,0,6,6" 与短式 "LU=6" 双格式;
  /// 失败 YamlException "`{definition}` is not a valid Riser definition" 逐字
  /// Riser (MiniYaml) (L82-131): the long "6,6,0,0,0,0,6,6" and short
  /// "LU=6" forms; failures throw the YamlException text verbatim.
  explicit Riser(const yaml::MiniYaml* my);

  /// this[int](L133-143):byte? → optional(0xFF = null)
  /// this[int] (L133-143): byte? → optional (0xFF = null).
  std::optional<std::uint8_t> At(int i) const {
    if (i < 0 || i >= 8)
      throw std::out_of_range("Index was outside the bounds of the array.");
    const auto b = static_cast<std::uint8_t>((uint8_bits >> (i * 8)) & 0xFF);
    if (b != kDefault)
      return b;
    return std::nullopt;
  }

  /// this[Connection](L149-152) | this[Connection] (L149-152).
  std::optional<std::uint8_t> At(Connection c) const {
    return At(static_cast<int>(c));
  }
};

/// TerrainTileInfo(TerrainInfo.cs L155-179)
/// TerrainTileInfo (TerrainInfo.cs L155-179).
class TerrainTileInfo {
 public:
  /// TerrainType(L157):[FieldLoader.Ignore] + 默认 255 —— Load 跳过该键,
  /// 子类由 tileset 名手工回填(等价反射 SetValue 面)
  /// TerrainType (L157): [FieldLoader.Ignore] + default 255 — Load skips
  /// the key; subclasses backfill from the tileset name (the equivalent
  /// reflection SetValue face).
  std::uint8_t TerrainType{255};
  std::uint8_t Height{0};       // L159
  std::uint8_t RampType{0};     // L160
  core::Color MinColor{core::Color::FromArgbRaw(0)};  // L161(default Color)
  core::Color MaxColor{core::Color::FromArgbRaw(0)};  // L162
  Riser riser{};                                 // L164(LoadUsing LoadRiser)

  virtual ~TerrainTileInfo() = default;

  /// GetColor(L172-178):Min!=Max 时按随机数插值 | GetColor (L172-178):
  /// interpolates by the random number when Min!=Max.
  core::Color GetColor(MersenneTwister& random) const {
    if (MinColor != MaxColor)
      return ColorLerp(random.NextFloat(), MinColor, MaxColor);
    return MinColor;
  }

  /// Exts.ColorLerp(Exts.cs L467-474):float 通道 lerp,(int) 截断
  /// Exts.ColorLerp (Exts.cs L467-474): float channel lerp, (int) truncation.
  static core::Color ColorLerp(float t, core::Color c1, core::Color c2) {
    return core::Color::FromArgb(
        static_cast<int>(t * c2.A() + (1 - t) * c1.A()),
        static_cast<int>(t * c2.R() + (1 - t) * c1.R()),
        static_cast<int>(t * c2.G() + (1 - t) * c1.G()),
        static_cast<int>(t * c2.B() + (1 - t) * c1.B()));
  }
};

/// TerrainTypeInfo(TerrainInfo.cs L181-189)
/// TerrainTypeInfo (TerrainInfo.cs L181-189).
class TerrainTypeInfo {
 public:
  /// TerrainTypeInfo(MiniYaml)(L189):FieldLoader.Load 语义(字段全默认)
  /// TerrainTypeInfo (MiniYaml) (L189): the FieldLoader.Load semantics
  /// (all fields defaulted).
  explicit TerrainTypeInfo(const yaml::MiniYaml& my);

  std::string Type{};  // L183
  std::uint64_t uint8_target_types{0};  // BitSet<TargetableType>("Clean"?)
                                        // 默认空 | default empty (L184)
  std::vector<std::string> vec_accepts_smudge_type{};  // L185
  core::Color Color{core::Color::FromArgbRaw(0)};      // L186
  bool b_restrict_player_color{false};                 // L187
};

/// ITerrainInfo(TerrainInfo.cs L27-43)
/// ITerrainInfo (TerrainInfo.cs L27-43).
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

/// TerrainTemplateInfo(Mods.Common/Terrain/TerrainInfo.cs L31-120)
/// TerrainTemplateInfo (Mods.Common/Terrain/TerrainInfo.cs L31-120).
class TerrainTemplateInfo {
 public:
  TerrainTemplateInfo(const ITerrainInfo& terrain_info, const yaml::MiniYaml& my);

  std::uint16_t Id{0};  // L33
  int2 size_{0, 0};     // L34
  bool b_pick_any{false};  // L35
  std::vector<std::string> vec_categories{};  // L36

  /// this[int](L115) | this[int] (L115).
  const TerrainTileInfo* TileAt(int index) const {
    return vec_tile_info_[static_cast<std::size_t>(index)].get();
  }
  /// Contains(L118-121) | Contains (L118-121).
  bool Contains(int index) const {
    return index >= 0 && index < static_cast<int>(vec_tile_info_.size());
  }
  /// TilesCount(L123) | TilesCount (L123).
  int TilesCount() const { return static_cast<int>(vec_tile_info_.size()); }

 protected:
  /// LoadTileInfo(L83-96;DefaultTerrainTemplateInfo 覆写) | LoadTileInfo
  /// (L83-96; overridden by DefaultTerrainTemplateInfo).
  virtual std::unique_ptr<TerrainTileInfo> LoadTileInfo(
      const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const;

 private:
  std::vector<std::unique_ptr<TerrainTileInfo>> vec_tile_info_;
};

/// DefaultTerrainTileInfo(DefaultTerrain.cs L36-40)
/// DefaultTerrainTileInfo (DefaultTerrain.cs L36-40).
class DefaultTerrainTileInfo final : public TerrainTileInfo {
 public:
  float ZOffset{0.0f};  // L38
  float ZRamp{1.0f};    // L39
};

/// DefaultTerrainTemplateInfo(DefaultTerrain.cs L42-70)
/// DefaultTerrainTemplateInfo (DefaultTerrain.cs L42-70).
class DefaultTerrainTemplateInfo final : public TerrainTemplateInfo {
 public:
  using TerrainTemplateInfo::TerrainTemplateInfo;

  std::vector<std::string> vec_images{};    // L44
  std::vector<std::string> vec_depth_images{};  // L45
  std::vector<std::int32_t> vec_frames{};   // L46
  std::string str_palette{};                // L47

 protected:
  std::unique_ptr<TerrainTileInfo> LoadTileInfo(
      const ITerrainInfo& terrain_info, const yaml::MiniYaml& my) const override;
};

/// DefaultTerrain(DefaultTerrain.cs L72-262):General/Terrain/Templates 三节
/// DefaultTerrain (DefaultTerrain.cs L72-262): the General/Terrain/Templates
/// sections.
class DefaultTerrain final : public ITerrainInfo {
 public:
  explicit DefaultTerrain(const fs::FileSystem& file_system,
                          const std::string& str_filepath);
  DefaultTerrain(std::span<const char> yaml_bytes,
                 const std::string& str_filepath);

  // ———— ITerrainInfo(TerrainInfo 接口显式实现区,L203-214)————
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
    // L216:TemplatesInDefinitionOrder[0].Id, 0 | L216.
    return TerrainTile{vec_templates_in_definition_order_.front()->Id, 0};
  }

  /// DefaultTerrain 直取面(L153-197) | the DefaultTerrain direct faces
  /// (L153-197).
  const TerrainTypeInfo& operator[](std::uint8_t index) const {
    return vec_terrain_info_[index];
  }

  std::uint8_t GetTerrainIndex(std::string_view str_type) const override;
  std::uint8_t GetTerrainIndex(TerrainTile r) const override;

  /// GetTileInfo/GetTileInfo(TerrainTile)(L180-194) | the GetTileInfo pair
  /// (L180-194).
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

  /// 模板面(ITemplatedTerrainInfo 的消费面;非拥有视图) | the template face
  /// (the ITemplatedTerrainInfo consumption surface; a non-owning view).
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

  // General 字段(DefaultTerrain.cs L74-85)
  Size size_{24, 24};
  std::int32_t int4_sheet_size{512};
  std::vector<core::Color> vec_height_debug_colors_{
      core::Color::FromArgb(255, 255, 0, 0)};  // Color.Red
  std::string str_editor_palette_{};           // Palette(Default = "terrain")
  bool b_ignore_tile_sprite_offsets{false};
  bool b_enable_depth{false};
  float fp4_min_height_color_brightness_{1.0f};
  float fp4_max_height_color_brightness_{1.0f};

 private:
  void Parse(std::span<const char> yaml_bytes, const std::string& str_filepath);

  std::string str_name_;   // L75
  std::string str_id_;     // L76
  std::vector<std::string> vec_editor_template_order_{};  // L80

  std::vector<TerrainTypeInfo> vec_terrain_info_{};  // L95
  std::unordered_map<std::uint16_t, TerrainTemplateInfo*>
      map_templates_;                                   // L88 等价(定义序表持有所有权)
  std::vector<std::unique_ptr<TerrainTemplateInfo>>
      vec_templates_in_definition_order_{};             // L89
  std::unordered_map<std::string, std::uint8_t> map_terrain_index_by_type_;
  std::uint8_t uint1_default_walkable_terrain_index_{0};
};

}  // namespace ora::map
