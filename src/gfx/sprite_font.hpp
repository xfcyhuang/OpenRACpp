#pragma once
import std;

#ifdef DrawText
#undef DrawText
#endif

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sheet.hpp"
#include "platform/ifont.hpp"

namespace ora::gfx {

using IFaceFont = platform::IFont;

struct GlyphInfo {
  float fp4_advance = 0.0f;
  int2 int2_offset{};
  std::optional<Sprite> opt_sprite;
};

class SpriteFont {
 public:

  using DrawSpriteFlatFn = std::function<void(const Sprite&, core::Vector3, float,
                                              core::Vector3, float)>;
  using DrawSpriteQuadFn = std::function<void(const Sprite&, core::Vector3, core::Vector3,
                                              core::Vector3, core::Vector3, core::Vector3,
                                              float)>;

  SpriteFont(std::string str_name, std::span<const std::uint8_t> vec_data,
             std::unique_ptr<IFaceFont> ptr_font, int int4_size, int int4_ascender,
             float fp4_scale, SheetBuilder& builder_sheet, DrawSpriteFlatFn fn_draw_flat,
             DrawSpriteQuadFn fn_draw_quad);

  SpriteFont(const SpriteFont&) = delete;
  SpriteFont& operator=(const SpriteFont&) = delete;
  ~SpriteFont();

  int TopOffset() const { return int4_top_offset_; }

  void SetScale(float fp4_scale);

  void DrawTextContrast(std::string_view str_text, core::Vector2 vec_location,
                        core::Color color_contrast, int int4_contrast_offset);
  void DrawText(std::string_view str_text, core::Vector2 vec_location, core::Color color_c);
  void DrawText(std::string_view str_text, core::Vector2 vec_location, core::Color color_c,
                float fp4_angle);
  void DrawTextWithContrast(std::string_view str_text, core::Vector2 vec_location,
                            core::Color color_fg, core::Color color_bg, int int4_offset);
  void DrawTextWithContrast(std::string_view str_text, core::Vector2 vec_location,
                            core::Color color_fg, core::Color color_bg_dark,
                            core::Color color_bg_light, int int4_offset);
  void DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_fg, core::Color color_bg, int int4_offset);
  void DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_fg, core::Color color_bg_dark,
                          core::Color color_bg_light, int int4_offset);
  void DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_fg, core::Color color_bg, int int4_offset,
                          float fp4_angle);
  void DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_fg, core::Color color_bg_dark,
                          core::Color color_bg_light, int int4_offset, float fp4_angle);

  int2 Measure(std::string_view str_text);

  static core::Color GetContrastColor(core::Color color_fg, core::Color color_bg_dark,
                                      core::Color color_bg_light) {
    return color_fg == core::Color::FromArgb(0xFFFFFFFF) ||
                   color_fg.GetBrightness() > 0.33f
               ? color_bg_dark
               : color_bg_light;
  }

  static std::vector<float> CreateCircularWeightMap(int int4_radius);

 private:
  struct GlyphKey {
    char32_t cp_char{};
    friend bool operator==(const GlyphKey&, const GlyphKey&) = default;
  };

  struct ContrastKey {
    char32_t cp_char{};
    int int4_radius{};
    friend bool operator==(const ContrastKey&, const ContrastKey&) = default;
  };

  const GlyphInfo& GlyphFor(char32_t cp_char);
  const Sprite& ContrastGlyphFor(char32_t cp_char, int int4_radius);
  const std::vector<float>& DilationElementsFor(int int4_radius);
  GlyphInfo CreateGlyph(char32_t cp_char);
  Sprite CreateContrastGlyph(char32_t cp_char, int int4_radius);
  float LineWidth(std::string_view sv_line);

  std::string str_name_;
  int int4_size_;
  int int4_top_offset_;
  SheetBuilder& builder_sheet_;
  DrawSpriteFlatFn fn_draw_flat_;
  DrawSpriteQuadFn fn_draw_quad_;
  std::unique_ptr<IFaceFont> ptr_font_;

  float fp4_device_scale_ = 1.0f;

  std::vector<std::pair<GlyphKey, GlyphInfo>> vec_glyphs_;
  std::vector<std::pair<ContrastKey, Sprite>> vec_contrast_glyphs_;
  std::vector<std::pair<int, std::vector<float>>> vec_dilation_elements_;
};

}
