// UPSTREAM: OpenRA.Game/Graphics/SpriteFont.cs @1f594e320 L20-431(全文)+
// PlatformInterfaces.cs L192-195(IFont)。字形缓存/对比度字形(灰度膨胀的
// 圆形权图)/阴影与对比度文本绘制/Measure 的行宽累加;deviceScale 的屏幕
// 像素取整面(0.5f 偏移 + (int) 截断)逐行保真。
// 形态映射:
//   - Game.Renderer.RgbaSpriteRenderer 静态 → 两个绘制 sink 注入(平铺/四角
//     形态;生产 = UIRgbaSpriteRenderer 的对应重载,测试可捕捕获;D 注册);
//   - char(UTF-16 单元)迭代 → UTF-8 码点迭代(core/text.hpp DecodeUtf8;
//     字形键 = 码点 —— 上游 >0xFFFF 以代理对半值查字形本就无字形,语义
//     等价,登记 COVERAGE);
//   - Cache<char,…>/Cache<(char,int),…>/Cache<int,…> → 插入序 vector 键表
//     (C# Cache 的 GetValue 工厂等价;SetScale 的 Clear 语义照抄);
//   - Platform.CreateFont(data) → IFont 抽象直传(Renderer.InitializeFonts
//     经 FreeTypeFont 实现供应);
//   - ctor 的 `glyphs[n] == null` 抛检查上游不可达(Cache 恒物化),省略。
// Shape mappings: Game.Renderer.RgbaSpriteRenderer's static becomes two
// injected draw sinks (the flat and four-corner shapes; production wires
// UIRgbaSpriteRenderer's matching overloads, tests capture; registered);
// the char (UTF-16 unit) iteration becomes a UTF-8 code-point iteration
// (core/text.hpp DecodeUtf8; the glyph key = the code point — upstream's
// >0xFFFF surrogates looked up half-value glyphs that never existed,
// semantically equal, registered in COVERAGE); the
// Cache<char,…>/Cache<(char,int),…>/Cache<int,…> become insertion-ordered
// vector-keyed tables (the C# Cache's GetValue-factory equivalent;
// SetScale's Clear kept); Platform.CreateFont(data) arrives as the IFont
// abstraction passed in (Renderer.InitializeFonts supplies FreeTypeFont
// instances); the ctor's `glyphs[n] == null` throw is unreachable upstream
// (the Cache always materializes) and is dropped.
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

/// IFont(PlatformInterfaces.cs L192-195):FreeTypeFont 实现的字体抽象。
/// IFont (PlatformInterfaces.cs L192-195): the font abstraction FreeTypeFont
/// implements.
using IFaceFont = platform::IFont;

/// GlyphInfo(SpriteFont.cs L424-429;internal)。
/// GlyphInfo (SpriteFont.cs L424-429; internal).
struct GlyphInfo {
  float fp4_advance = 0.0f;
  int2 int2_offset{};
  std::optional<Sprite> opt_sprite;  // 空 = C# null | empty = C# null
};

/// SpriteFont(SpriteFont.cs;不可拷贝)。
/// SpriteFont (SpriteFont.cs; non-copyable).
class SpriteFont {
 public:
  /// RgbaSpriteRenderer.DrawSprite 的两个消费形态(sink 注入面)。
  /// The two consumed shapes of RgbaSpriteRenderer.DrawSprite (the injected
  /// sink face).
  using DrawSpriteFlatFn = std::function<void(const Sprite&, core::Vector3, float,
                                              core::Vector3, float)>;
  using DrawSpriteQuadFn = std::function<void(const Sprite&, core::Vector3, core::Vector3,
                                              core::Vector3, core::Vector3, core::Vector3,
                                              float)>;

  /// 上游 ctor 的 platform/name/data 形参收敛为 data 携带的字体工厂 +
  /// 名(异常/诊断文本无消费点)。
  /// Upstream's platform/name/data parameters collapse into the data-fed
  /// font factory + name (no consumer of the exception/diagnostic texts).
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

  /// GetContrastColor(SpriteFont.cs L404-406;static)。
  /// GetContrastColor (SpriteFont.cs L404-406; static).
  static core::Color GetContrastColor(core::Color color_fg, core::Color color_bg_dark,
                                      core::Color color_bg_light) {
    return color_fg == core::Color::FromArgb(0xFFFFFFFF) ||
                   color_fg.GetBrightness() > 0.33f
               ? color_bg_dark
               : color_bg_light;
  }

  /// CreateCircularWeightMap(L281-338;上游 private static —— 公开化仅为
  /// golden 断言面,数值与上游注释样例对齐)。
  /// CreateCircularWeightMap (L281-338; private static upstream — made
  /// public solely as the golden assertion face, its values pinned against
  /// upstream's commented examples).
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

}  // namespace ora::gfx
