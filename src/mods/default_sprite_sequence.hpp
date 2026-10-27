#pragma once
import std;

#include "gfx/gfx_util.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_cache.hpp"
#include "meta/field_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

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

class DefaultSpriteSequence : public gfx::ISpriteSequence {
 public:
  DefaultSpriteSequence(gfx::SpriteCache& cache_sprites, gfx::ISpriteSequenceLoader& loader,
                        std::string str_image, std::string str_sequence, const yaml::MiniYaml& yaml_data,
                        const yaml::MiniYaml& yaml_defaults);

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

  struct SpriteReservation {
    std::int32_t int4_token = 0;
    core::Vector3 vec_offset{};
    bool b_flip_x = false;
    bool b_flip_y = false;
    float fp4_z_ramp = 0.0f;
    gfx::BlendMode kind_blend_mode = gfx::BlendMode::Alpha;
    std::optional<std::vector<std::int32_t>> opt_vec_frames;
  };

  struct ReservationInfo {
    std::string str_filename;
    std::optional<std::vector<std::int32_t>> opt_vec_load_frames;
    std::optional<std::vector<std::int32_t>> opt_vec_frames;
    yaml::SourceLocation location_source{};
  };

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

  static Rectangle FlipRectangle(Rectangle rect_in, bool b_flip_x, bool b_flip_y);

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

  virtual std::int32_t GetFacingFrameOffset(WAngle wangle_facing);
  virtual float GetScale() const { return fp4_scale_; }

  static const yaml::MiniYaml& NoData();

  std::string str_image_;
  std::string str_name_;
  std::vector<SpriteReservation> vec_sprites_to_load_;
  std::vector<gfx::Sprite> vec_sprites_;
  std::vector<gfx::Sprite> vec_shadow_sprites_;
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
  std::int32_t int4_shadow_z_offset_ = -5;
  bool b_ignore_world_tint_ = false;
  float fp4_scale_ = 1.0f;
  std::optional<std::vector<float>> opt_vec_alpha_;
  bool b_alpha_fade_ = false;
  std::optional<Rectangle> opt_rect_bounds_;

  std::optional<std::int32_t> opt_int4_depth_sprite_reservation_;
  core::Vector2 vec_depth_sprite_offset_{};

  gfx::ISpriteSequenceLoader* ptr_loader_ = nullptr;

  std::string str_tile_set_;
};

}
