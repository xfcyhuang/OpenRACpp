// UPSTREAM: OpenRA.Mods.Common/Graphics/DefaultSpriteSequence.cs @b6fc03f L24-605
// 通用精灵序列:两段流水(ReserveSprites 预留记账 → ResolveSprites 物化重
// 索引),字段全走 FieldLoader.GetValue&lt;T&gt; 的 LoadField 查找(data 缺键
// 回退 defaults)。mods 侧 Classic/Tileset/D2k 变体继承此类。
// The generic sprite sequence: the two-stage pipeline (ReserveSprites
// booking → ResolveSprites materialization + reindexing), every field read
// through the LoadField lookup of FieldLoader.GetValue<T> (a data miss falls
// back to defaults). The mods-side Classic/Tileset/D2k variants derive from
// this class.
#pragma once
import std;

#include "gfx/gfx_util.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_cache.hpp"
#include "meta/field_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

/// DefaultSpriteSequenceLoader(L24-61):CreateSequence 虚工厂 +
/// ParseSequences(Defaults 剥离 + 逐序列构造/预留 + InvalidDataException
/// 包装文本)。
/// DefaultSpriteSequenceLoader (L24-61): the CreateSequence virtual factory
/// + ParseSequences (Defaults stripping + per-sequence
/// construct/reserve + the InvalidDataException wrapper text).
class DefaultSpriteSequenceLoader : public gfx::ISpriteSequenceLoader {
 public:
  std::vector<std::pair<std::string, std::unique_ptr<gfx::ISpriteSequence>>> ParseSequences(
      gfx::SpriteCache& cache_sprites, const std::string& str_tile_set,
      const yaml::MiniYamlNode& node_image) override;

  virtual std::unique_ptr<gfx::ISpriteSequence> CreateSequence(gfx::SpriteCache& cache_sprites,
                                                              const std::string& str_image,
                                                              const std::string& str_sequence,
                                                              const yaml::MiniYaml& yaml_data,
                                                              const yaml::MiniYaml& yaml_defaults);
};

/// DefaultSpriteSequence(L66-605)。
class DefaultSpriteSequence : public gfx::ISpriteSequence {
 public:
  DefaultSpriteSequence(gfx::SpriteCache& cache_sprites, gfx::ISpriteSequenceLoader& loader,
                        std::string str_image, std::string str_sequence, const yaml::MiniYaml& yaml_data,
                        const yaml::MiniYaml& yaml_defaults);

  // ———— ISpriteSequence ————
  std::string_view Name() const override { return str_name_; }
  std::int32_t Length() const override;
  std::int32_t Facings() const override { return opt_interpolated_facings_.value_or(int4_facings_); }
  std::int32_t Tick() const override { return int4_tick_; }
  std::int32_t ZOffset() const override { return int4_z_offset_; }
  std::int32_t ShadowZOffset() const override { return int4_shadow_z_offset_; }
  Rectangle Bounds() const override;
  bool IgnoreWorldTint() const override { return b_ignore_world_tint_; }
  float Scale() const override { return GetScale(); }  void ResolveSprites(gfx::SpriteCache& cache_sprites) override;
  gfx::Sprite GetSprite(std::int32_t int4_frame) override;
  gfx::Sprite GetSprite(std::int32_t int4_frame, WAngle wangle_facing) override;
  std::pair<gfx::Sprite, WAngle> GetSpriteWithRotation(std::int32_t int4_frame,
                                                       WAngle wangle_facing) override;
  gfx::Sprite GetShadow(std::int32_t int4_frame, WAngle wangle_facing) override;
  float GetAlpha(std::int32_t int4_frame) override;

  virtual void ReserveSprites(const std::string& str_tile_set, const yaml::MiniYaml& yaml_data,
                              const yaml::MiniYaml& yaml_defaults, gfx::SpriteCache& cache_sprites);

 protected:
  /// 预留记账(L68-77)。
  /// The reservation booking (L68-77).
  struct SpriteReservation {
    std::int32_t int4_token = 0;
    core::Vector3 vec_offset{};
    bool b_flip_x = false;
    bool b_flip_y = false;
    float fp4_z_ramp = 0.0f;
    gfx::BlendMode kind_blend_mode = gfx::BlendMode::Alpha;
    std::optional<std::vector<std::int32_t>> opt_vec_frames;  // nullopt = 上游 null | upstream's null
  };

  /// 文件名解析结果(L79-93)。
  /// The filename-parse result (L79-93).
  struct ReservationInfo {
    std::string str_filename;
    std::optional<std::vector<std::int32_t>> opt_vec_load_frames;
    std::optional<std::vector<std::int32_t>> opt_vec_frames;
    yaml::SourceLocation location_source{};
  };

  /// LoadField&lt;T&gt;(L245-270):data 键缺失回退 defaults;location 版带出
  /// SourceLocation(异常文本用)。
  /// LoadField<T> (L245-270): a data miss falls back to defaults; the
  /// location variant also yields the SourceLocation (for the exception
  /// texts).
  static const yaml::MiniYamlNode* FindFieldNode(std::string_view sv_key, const yaml::MiniYaml& yaml_data,
                                                 const yaml::MiniYaml* yaml_defaults) {
    if (const yaml::MiniYamlNode* node = yaml_data.NodeWithKeyOrDefault(sv_key))
      return node;
    return yaml_defaults != nullptr ? yaml_defaults->NodeWithKeyOrDefault(sv_key) : nullptr;
  }

  static std::string LoadString(std::string_view sv_key, const std::string* str_fallback,
                                 const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static std::int32_t LoadInt32(std::string_view sv_key, std::int32_t int4_fallback,
                                const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static bool LoadBool(std::string_view sv_key, bool b_fallback, const yaml::MiniYaml& yaml_data,
                       const yaml::MiniYaml* yaml_defaults);
  static float LoadFloat(std::string_view sv_key, float fp4_fallback, const yaml::MiniYaml& yaml_data,
                         const yaml::MiniYaml* yaml_defaults);

  /// 带 SourceLocation 出参的三态 int(int? 字段:InterpolatedFacings 等)。
  /// The three-state int with a SourceLocation out (the int? fields:
  /// InterpolatedFacings among others).
  static std::optional<std::int32_t> LoadNullableInt32(std::string_view sv_key,
                                                       const yaml::MiniYaml& yaml_data,
                                                       const yaml::MiniYaml* yaml_defaults,
                                                       yaml::SourceLocation& location_out);
  static std::int32_t LoadInt32Located(std::string_view sv_key, std::int32_t int4_fallback,
                                       const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults,
                                       yaml::SourceLocation& location_out);
  static std::optional<std::vector<std::int32_t>> LoadFrames(std::string_view sv_key,
                                                             const yaml::MiniYaml& yaml_data,
                                                             const yaml::MiniYaml* yaml_defaults);
  static std::vector<float> LoadFloatArray(std::string_view sv_key, const yaml::MiniYaml& yaml_data,
                                           const yaml::MiniYaml* yaml_defaults);
  static core::Vector3 LoadVector3(std::string_view sv_key, core::Vector3 vec_fallback,
                                   const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static core::Vector2 LoadVector2(std::string_view sv_key, core::Vector2 vec_fallback,
                                   const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static core::Color LoadColor(std::string_view sv_key, core::Color color_fallback,
                               const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static gfx::BlendMode LoadBlendMode(std::string_view sv_key, gfx::BlendMode kind_fallback,
                                      const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static std::vector<std::pair<std::string, std::string>> LoadStringDictionary(
      std::string_view sv_key, const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);
  static std::int32_t LoadWDist(std::string_view sv_key, std::int32_t int4_fallback,
                                const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults);

  /// FlipRectangle(L272-280):翻转轴交换左右/上下边界。
  /// FlipRectangle (L272-280): flipping an axis swaps that axis's edges.
  static Rectangle FlipRectangle(Rectangle rect_in, bool b_flip_x, bool b_flip_y);

  /// CalculateFrameIndices(L282-312):length null = 全帧请求(null);影子段
  /// 追加 shadowOffset 平移副本。
  /// CalculateFrameIndices (L282-312): a null length requests all frames
  /// (null); the shadow section appends the shadowOffset-shifted copies.
  static std::optional<std::vector<std::int32_t>> CalculateFrameIndices(
      std::int32_t int4_start, const std::optional<std::int32_t>& opt_int4_length, std::int32_t int4_stride,
      std::int32_t int4_facings, const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
      bool b_transpose, bool b_reverse_facings, std::int32_t int4_shadow_start);

  virtual std::vector<ReservationInfo> ParseFilenames(const std::string& str_tile_set,
                                                      const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
                                                      const yaml::MiniYaml& yaml_data,
                                                      const yaml::MiniYaml& yaml_defaults);
  virtual std::vector<ReservationInfo> ParseCombineFilenames(
      const std::string& str_tile_set, const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
      const yaml::MiniYaml& yaml_data);

  void ThrowIfUnresolved() const;

  /// GetFacingFrameOffset(L590-593):IndexFacing 量化。
  /// GetFacingFrameOffset (L590-593): the IndexFacing quantization.
  virtual std::int32_t GetFacingFrameOffset(WAngle wangle_facing);
  virtual float GetScale() const { return fp4_scale_; }

  static const yaml::MiniYaml& NoData();

  // ———— 上游 protected 字段面(名随 C#)————
  // ———— The upstream protected field faces (C# names kept) ————
  std::string str_image_;
  std::string str_name_;
  std::vector<SpriteReservation> vec_sprites_to_load_;
  std::vector<gfx::Sprite> vec_sprites_;
  std::vector<gfx::Sprite> vec_shadow_sprites_;  // 空 = 上游 null | empty = upstream's null
  bool b_has_shadow_sprites_ = false;
  bool b_reverse_facings_ = false;
  bool b_reverses_ = false;

  std::int32_t int4_start_ = 0;
  std::int32_t int4_shadow_start_ = -1;
  std::optional<std::int32_t> opt_int4_length_;
  std::optional<std::int32_t> opt_int4_stride_;
  bool b_transpose_ = false;

  std::int32_t int4_facings_ = 1;
  std::optional<std::int32_t> opt_interpolated_facings_;
  std::int32_t int4_tick_ = 40;
  std::int32_t int4_z_offset_ = 0;
  std::int32_t int4_shadow_z_offset_ = -5;  // new WDist(-5).Length | the WDist(-5).Length
  bool b_ignore_world_tint_ = false;
  float fp4_scale_ = 1.0f;
  std::optional<std::vector<float>> opt_vec_alpha_;
  bool b_alpha_fade_ = false;
  std::optional<Rectangle> opt_rect_bounds_;

  std::optional<std::int32_t> opt_int4_depth_sprite_reservation_;
  core::Vector2 vec_depth_sprite_offset_{};

  // Cache/Loader 引用仅构造期消费(上游同:ctor 不留 cache 字段)
  // The cache/loader references are consumed at construction only (as
  // upstream: the ctor keeps no cache field).
  gfx::ISpriteSequenceLoader* ptr_loader_ = nullptr;

  // 当前预留轮的 tileset(上游方法参数的成员化;每轮 ReserveSprites 更新)
  // The tileset of the current reservation round (the member form of
  // upstream's method parameter; updated per ReserveSprites round).
  std::string str_tile_set_;
};

}  // namespace ora::mods
