// UPSTREAM: OpenRA.Game/Graphics/SpriteFont.cs @1f594e320(sprite_font.hpp 的
// 实现)
// The implementation of sprite_font.hpp.
import std;

#include "gfx/sprite_font.hpp"

#include "core/text.hpp"

namespace ora::gfx {
namespace {

core::Vector3 TintOf(core::Color color_c) {
  return core::Vector3{static_cast<float>(color_c.R()) / 255.0f,
                       static_cast<float>(color_c.G()) / 255.0f,
                       static_cast<float>(color_c.B()) / 255.0f};
}

constexpr int TruncToInt(float fp4_v) { return static_cast<int>(fp4_v); }

}  // namespace

SpriteFont::SpriteFont(std::string str_name, std::span<const std::uint8_t> vec_data,
                       std::unique_ptr<IFaceFont> ptr_font, int int4_size, int int4_ascender,
                       float fp4_scale, SheetBuilder& builder_sheet, DrawSpriteFlatFn fn_draw_flat,
                       DrawSpriteQuadFn fn_draw_quad)
    : str_name_(std::move(str_name)),
      int4_size_(int4_size),
      int4_top_offset_(int4_size - int4_ascender),
      builder_sheet_(builder_sheet),
      fn_draw_flat_(std::move(fn_draw_flat)),
      fn_draw_quad_(std::move(fn_draw_quad)),
      ptr_font_(std::move(ptr_font)),
      fp4_device_scale_(fp4_scale) {
  (void)vec_data;
  if (builder_sheet_.CurrentChannel() != TextureChannel::RGBA)
    throw std::runtime_error("The sheet builder must create BGRA sheets. (Parameter 'builder')");

  // 上游 size <= 24 的预缓存 + `glyphs[n] == null` 抛(Cache 恒物化,不可达;
  // 此处仅预物化)
  // upstream's size <= 24 precache + its `glyphs[n] == null` throw (the Cache
  // always materializes; unreachable — here it only pre-materializes)
  if (int4_size <= 24)
    for (char32_t cp_n = 0x20; cp_n < 0x7f; cp_n++)
      (void)GlyphFor(cp_n);
}

SpriteFont::~SpriteFont() = default;

void SpriteFont::SetScale(float fp4_scale) {
  fp4_device_scale_ = fp4_scale;
  vec_glyphs_.clear();
  vec_contrast_glyphs_.clear();
}

void SpriteFont::DrawTextContrast(std::string_view str_text, core::Vector2 vec_location,
                                  core::Color color_contrast, int int4_contrast_offset) {
  vec_location.Y += static_cast<float>(int4_size_);

  const int int4_screen_contrast = TruncToInt(static_cast<float>(int4_contrast_offset) *
                                              fp4_device_scale_);
  int2 int2_screen{TruncToInt(vec_location.X * fp4_device_scale_ + 0.5f),
                   TruncToInt(vec_location.Y * fp4_device_scale_ + 0.5f)};
  const core::Vector2 vec_contrast{static_cast<float>(int4_screen_contrast),
                                   static_cast<float>(int4_screen_contrast)};
  const core::Vector3 vec_tint = TintOf(color_contrast);

  std::size_t int8_i = 0;
  while (int8_i < str_text.size()) {
    const char32_t cp_s = DecodeUtf8(str_text, int8_i);
    if (cp_s == U'\n') {
      vec_location.Y += static_cast<float>(int4_size_);
      int2_screen = int2{TruncToInt(vec_location.X * fp4_device_scale_ + 0.5f),
                         TruncToInt(vec_location.Y * fp4_device_scale_ + 0.5f)};
      continue;
    }

    const GlyphInfo& info_g = GlyphFor(cp_s);
    if (info_g.opt_sprite.has_value()) {
      const Sprite& sprite_contrast = ContrastGlyphFor(cp_s, int4_screen_contrast);
      const core::Vector2 vec_pos{
          (static_cast<float>(int2_screen.X + info_g.int2_offset.X) - vec_contrast.X) /
              fp4_device_scale_,
          (static_cast<float>(int2_screen.Y + info_g.int2_offset.Y) - vec_contrast.Y) /
              fp4_device_scale_};
      fn_draw_flat_(sprite_contrast, core::Vector3{vec_pos.X, vec_pos.Y, 0.0f},
                    1.0f / fp4_device_scale_, vec_tint, 1.0f);
    }

    int2_screen.X += TruncToInt(info_g.fp4_advance + 0.5f);
  }
}

void SpriteFont::DrawText(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_c) {
  vec_location.Y += static_cast<float>(int4_size_);

  int2 int2_screen{TruncToInt(vec_location.X * fp4_device_scale_ + 0.5f),
                   TruncToInt(vec_location.Y * fp4_device_scale_ + 0.5f)};
  const core::Vector3 vec_tint = TintOf(color_c);

  std::size_t int8_i = 0;
  while (int8_i < str_text.size()) {
    const char32_t cp_s = DecodeUtf8(str_text, int8_i);
    if (cp_s == U'\n') {
      vec_location.Y += static_cast<float>(int4_size_);
      int2_screen = int2{TruncToInt(vec_location.X * fp4_device_scale_ + 0.5f),
                         TruncToInt(vec_location.Y * fp4_device_scale_ + 0.5f)};
      continue;
    }

    const GlyphInfo& info_g = GlyphFor(cp_s);
    if (info_g.opt_sprite.has_value()) {
      const core::Vector2 vec_pos{
          static_cast<float>(int2_screen.X + info_g.int2_offset.X) / fp4_device_scale_,
          static_cast<float>(int2_screen.Y + info_g.int2_offset.Y) / fp4_device_scale_};
      fn_draw_flat_(*info_g.opt_sprite, core::Vector3{vec_pos.X, vec_pos.Y, 0.0f},
                    1.0f / fp4_device_scale_, vec_tint, 1.0f);
    }

    int2_screen.X += TruncToInt(info_g.fp4_advance + 0.5f);
  }
}

void SpriteFont::DrawText(std::string_view str_text, core::Vector2 vec_location,
                          core::Color color_c, float fp4_angle) {
  core::Vector2 vec_offset{0.0f, static_cast<float>(int4_size_)};
  const core::Vector3 vec_tint = TintOf(color_c);

  // transform = Matrix3x2.CreateRotation(-angle) * CreateTranslation(location)
  // (行向量形态:x' = x·cosA + y·sinA + t.X;y' = -x·sinA + y·cosA + t.Y)
  // transform = Matrix3x2.CreateRotation(-angle) *
  // CreateTranslation(location) (row-vector form: x' = x·cosA + y·sinA + t.X;
  // y' = -x·sinA + y·cosA + t.Y)
  const float fp4_c = std::cos(fp4_angle);
  const float fp4_s = std::sin(fp4_angle);
  const auto transform = [&](core::Vector2 vec_p) {
    return core::Vector2{vec_p.X * fp4_c + vec_p.Y * fp4_s + vec_location.X,
                         -vec_p.X * fp4_s + vec_p.Y * fp4_c + vec_location.Y};
  };

  core::Vector2 vec_p = vec_offset;
  std::size_t int8_i = 0;
  while (int8_i < str_text.size()) {
    const char32_t cp_s = DecodeUtf8(str_text, int8_i);
    if (cp_s == U'\n') {
      vec_offset.Y += static_cast<float>(int4_size_);
      vec_p = vec_offset;
      continue;
    }

    const GlyphInfo& info_g = GlyphFor(cp_s);
    if (info_g.opt_sprite.has_value()) {
      const core::Vector2 vec_tl{
          vec_p.X + static_cast<float>(info_g.int2_offset.X) / fp4_device_scale_,
          vec_p.Y + static_cast<float>(info_g.int2_offset.Y) / fp4_device_scale_};
      const core::Vector2 vec_br{
          vec_tl.X + info_g.opt_sprite->vec_size.X / fp4_device_scale_,
          vec_tl.Y + info_g.opt_sprite->vec_size.Y / fp4_device_scale_};
      const core::Vector2 vec_tr{vec_br.X, vec_tl.Y};
      const core::Vector2 vec_bl{vec_tl.X, vec_br.Y};

      const core::Vector2 vec_ra = transform(vec_tl);
      const core::Vector2 vec_rb = transform(vec_tr);
      const core::Vector2 vec_rc = transform(vec_br);
      const core::Vector2 vec_rd = transform(vec_bl);

      const core::Vector2 vec_screen_offset{
          (TruncToInt(vec_ra.X * fp4_device_scale_ + 0.5f)) / fp4_device_scale_ - vec_ra.X,
          (TruncToInt(vec_ra.Y * fp4_device_scale_ + 0.5f)) / fp4_device_scale_ - vec_ra.Y};

      const auto as_v3 = [&](core::Vector2 vec_v) {
        return core::Vector3{vec_v.X + vec_screen_offset.X, vec_v.Y + vec_screen_offset.Y, 0.0f};
      };
      fn_draw_quad_(*info_g.opt_sprite, as_v3(vec_ra), as_v3(vec_rb), as_v3(vec_rc),
                    as_v3(vec_rd), vec_tint, 1.0f);
    }

    vec_p.X += info_g.fp4_advance / fp4_device_scale_;
  }
}

void SpriteFont::DrawTextWithContrast(std::string_view str_text, core::Vector2 vec_location,
                                      core::Color color_fg, core::Color color_bg,
                                      int int4_offset) {
  if (int4_offset > 0)
    DrawTextContrast(str_text, vec_location, color_bg, int4_offset);

  DrawText(str_text, vec_location, color_fg);
}

void SpriteFont::DrawTextWithContrast(std::string_view str_text, core::Vector2 vec_location,
                                      core::Color color_fg, core::Color color_bg_dark,
                                      core::Color color_bg_light, int int4_offset) {
  DrawTextWithContrast(str_text, vec_location, color_fg,
                       GetContrastColor(color_fg, color_bg_dark, color_bg_light), int4_offset);
}

void SpriteFont::DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                                    core::Color color_fg, core::Color color_bg, int int4_offset) {
  if (int4_offset != 0) {
    const float fp4_screen_offset =
        static_cast<float>(TruncToInt(static_cast<float>(int4_offset) * fp4_device_scale_)) /
        fp4_device_scale_;
    DrawText(str_text,
             core::Vector2{vec_location.X + fp4_screen_offset, vec_location.Y + fp4_screen_offset},
             color_bg);
  }

  DrawText(str_text, vec_location, color_fg);
}

void SpriteFont::DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                                    core::Color color_fg, core::Color color_bg_dark,
                                    core::Color color_bg_light, int int4_offset) {
  DrawTextWithShadow(str_text, vec_location, color_fg,
                     GetContrastColor(color_fg, color_bg_dark, color_bg_light), int4_offset);
}

void SpriteFont::DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                                    core::Color color_fg, core::Color color_bg, int int4_offset,
                                    float fp4_angle) {
  if (int4_offset != 0) {
    const float fp4_screen_offset =
        static_cast<float>(TruncToInt(static_cast<float>(int4_offset) * fp4_device_scale_)) /
        fp4_device_scale_;
    DrawText(str_text,
             core::Vector2{vec_location.X + fp4_screen_offset, vec_location.Y + fp4_screen_offset},
             color_bg, fp4_angle);
  }

  DrawText(str_text, vec_location, color_fg, fp4_angle);
}

void SpriteFont::DrawTextWithShadow(std::string_view str_text, core::Vector2 vec_location,
                                    core::Color color_fg, core::Color color_bg_dark,
                                    core::Color color_bg_light, int int4_offset,
                                    float fp4_angle) {
  DrawTextWithShadow(str_text, vec_location, color_fg,
                     GetContrastColor(color_fg, color_bg_dark, color_bg_light), int4_offset,
                     fp4_angle);
}

int2 SpriteFont::Measure(std::string_view str_text) {
  if (str_text.empty())
    return int2{0, int4_size_};

  float fp4_max_width = 0.0f;
  int int4_rows = 0;

  std::size_t int8_line_start = 0;
  while (int8_line_start <= str_text.size()) {
    const std::size_t int8_nl = str_text.find('\n', int8_line_start);
    const std::string_view sv_line =
        int8_nl == std::string_view::npos ? str_text.substr(int8_line_start)
                                          : str_text.substr(int8_line_start,
                                                            int8_nl - int8_line_start);
    int4_rows++;
    fp4_max_width = std::max(fp4_max_width, LineWidth(sv_line));

    if (int8_nl == std::string_view::npos)
      break;
    int8_line_start = int8_nl + 1;
  }

  return int2{static_cast<int>(std::ceil(fp4_max_width)), int4_rows * int4_size_};
}

float SpriteFont::LineWidth(std::string_view sv_line) {
  float fp4_result = 0.0f;
  std::size_t int8_i = 0;
  while (int8_i < sv_line.size())
    fp4_result += GlyphFor(DecodeUtf8(sv_line, int8_i)).fp4_advance;

  return fp4_result / fp4_device_scale_;
}

const GlyphInfo& SpriteFont::GlyphFor(char32_t cp_char) {
  for (const auto& [key_entry, info_entry] : vec_glyphs_)
    if (key_entry.cp_char == cp_char)
      return info_entry;

  return vec_glyphs_.emplace_back(GlyphKey{cp_char}, CreateGlyph(cp_char)).second;
}

const Sprite& SpriteFont::ContrastGlyphFor(char32_t cp_char, int int4_radius) {
  for (const auto& [key_entry, sprite_entry] : vec_contrast_glyphs_)
    if (key_entry.cp_char == cp_char && key_entry.int4_radius == int4_radius)
      return sprite_entry;

  return vec_contrast_glyphs_
      .emplace_back(ContrastKey{cp_char, int4_radius}, CreateContrastGlyph(cp_char, int4_radius))
      .second;
}

const std::vector<float>& SpriteFont::DilationElementsFor(int int4_radius) {
  for (const auto& [int4_entry_radius, vec_entry] : vec_dilation_elements_)
    if (int4_entry_radius == int4_radius)
      return vec_entry;

  return vec_dilation_elements_.emplace_back(int4_radius, CreateCircularWeightMap(int4_radius))
      .second;
}

GlyphInfo SpriteFont::CreateGlyph(char32_t cp_char) {
  const platform::FontGlyph glyph =
      ptr_font_->CreateGlyph(static_cast<std::uint32_t>(cp_char), int4_size_, fp4_device_scale_);
  if (glyph.Empty())
    return GlyphInfo{};

  const Sprite sprite_s = builder_sheet_.Allocate(int2{glyph.int4_width, glyph.int4_height});
  GlyphInfo info_g;
  info_g.opt_sprite = sprite_s;
  info_g.fp4_advance = glyph.fp4_advance;
  info_g.int2_offset = glyph.offset;

  Sheet& sheet_target = *sprite_s.ptr_sheet;
  const std::span<std::byte> span_dest = sheet_target.GetData();
  const int int4_dest_stride = sheet_target.Size().X * 4;

  for (int int4_j = 0; int4_j < sprite_s.Bounds.Height; int4_j++) {
    for (int int4_i = 0; int4_i < sprite_s.Bounds.Width; int4_i++) {
      const std::uint8_t u8_p = glyph.vec_data[static_cast<std::size_t>(int4_j) *
                                                   glyph.int4_width +
                                               int4_i];
      if (u8_p != 0) {
        const std::size_t uint8_q =
            static_cast<std::size_t>(int4_dest_stride) * (int4_j + sprite_s.Bounds.Top()) +
            4 * (int4_i + sprite_s.Bounds.Left());
        const std::byte byte_p{u8_p};
        span_dest[uint8_q] = byte_p;
        span_dest[uint8_q + 1] = byte_p;
        span_dest[uint8_q + 2] = byte_p;
        span_dest[uint8_q + 3] = byte_p;
      }
    }
  }

  sheet_target.CommitBufferedData(sprite_s.Bounds);
  return info_g;
}

std::vector<float> SpriteFont::CreateCircularWeightMap(int int4_r) {
  const int int4_stride = 2 * int4_r + 1;
  std::vector<float> vec_elem(static_cast<std::size_t>(int4_stride) * int4_stride, 0.0f);

  for (int int4_j = 0; int4_j <= 2 * int4_r; int4_j++) {
    for (int int4_i = 0; int4_i <= 2 * int4_r; int4_i++) {
      const int int4_di = int4_i - int4_r;
      const int int4_dj = int4_j - int4_r;

      // 与圆无交 | no intersection with the circle
      if (int4_di * int4_di + int4_dj * int4_dj > (int4_r + 1) * (int4_r + 1))
        continue;

      // 全含于圆 | fully contained within the circle
      if (int4_di * int4_di + int4_dj * int4_dj < (int4_r - 1) * (int4_r - 1)) {
        vec_elem[static_cast<std::size_t>(int4_j) * int4_stride + int4_i] = 1.0f;
        continue;
      }

      // 5×5 子网格近似亚像素交(Math.Sign(0) = 0,非 copysign)
      // the 5x5-grid sub-pixel approximation (Math.Sign(0) = 0, not
      // copysign)
      const auto sign_net = [](int int4_v) { return (int4_v > 0) - (int4_v < 0); };
      for (int int4_jj = 0; int4_jj < 5; int4_jj++) {
        for (int int4_ii = 0; int4_ii < 5; int4_ii++) {
          const float fp4_si =
              int4_di - sign_net(int4_di) * int4_ii / 5.0f;
          const float fp4_sj =
              int4_dj - sign_net(int4_dj) * int4_jj / 5.0f;
          if (fp4_si * fp4_si + fp4_sj * fp4_sj <= static_cast<float>(int4_r) * int4_r)
            vec_elem[static_cast<std::size_t>(int4_j) * int4_stride + int4_i] += 0.04f;
        }
      }
    }
  }

  return vec_elem;
}

Sprite SpriteFont::CreateContrastGlyph(char32_t cp_char, int int4_radius) {
  const GlyphInfo& info_glyph = GlyphFor(cp_char);

  const Sprite sprite_s = builder_sheet_.Allocate(
      int2{info_glyph.opt_sprite->Bounds.Width + 2 * int4_radius,
           info_glyph.opt_sprite->Bounds.Height + 2 * int4_radius});
  Sheet& sheet_dest = *sprite_s.ptr_sheet;
  const std::span<std::byte> span_dest = sheet_dest.GetData();
  const int int4_dest_stride = sheet_dest.Size().X * 4;

  const Sprite& sprite_glyph = *info_glyph.opt_sprite;
  Sheet& sheet_glyph = *sprite_glyph.ptr_sheet;
  const std::span<std::byte> span_glyph = sheet_glyph.GetData();
  const int int4_glyph_stride = sheet_glyph.Size().X * 4;
  const Rectangle rect_glyph_bounds = sprite_glyph.Bounds;

  const std::vector<float>& vec_elem = DilationElementsFor(int4_radius);
  const int int4_elem_stride = 2 * int4_radius + 1;

  for (int int4_j = 0; int4_j < sprite_s.Bounds.Height; int4_j++) {
    for (int int4_i = 0; int4_i < sprite_s.Bounds.Width; int4_i++) {
      bool b_first = true;
      std::uint8_t u8_alpha = 0;
      for (int int4_wj = 0; int4_wj <= 2 * int4_radius; int4_wj++) {
        for (int int4_wi = 0; int4_wi <= 2 * int4_radius; int4_wi++) {
          const int int4_ii = int4_i + int4_wi - 2 * int4_radius;
          const int int4_jj = int4_j + int4_wj - 2 * int4_radius;
          if (int4_ii < 0 || int4_ii >= rect_glyph_bounds.Width || int4_jj < 0 ||
              int4_jj >= rect_glyph_bounds.Height)
            continue;

          const float fp4_weighted =
              vec_elem[static_cast<std::size_t>(int4_wj) * int4_elem_stride + int4_wi] *
              static_cast<std::uint8_t>(
                  span_glyph[static_cast<std::size_t>(int4_glyph_stride) *
                                 (int4_jj + rect_glyph_bounds.Top()) +
                             4 * (int4_ii + rect_glyph_bounds.Left()) + 3]);
          const auto u8_weighted = static_cast<std::uint8_t>(fp4_weighted);
          if (b_first || u8_weighted > u8_alpha) {
            u8_alpha = u8_weighted;
            b_first = false;
          }
        }
      }

      if (u8_alpha > 0) {
        const std::size_t uint8_q =
            static_cast<std::size_t>(int4_dest_stride) * (int4_j + sprite_s.Bounds.Top()) +
            4 * (int4_i + sprite_s.Bounds.Left());
        const std::byte byte_alpha{u8_alpha};
        span_dest[uint8_q] = byte_alpha;
        span_dest[uint8_q + 1] = byte_alpha;
        span_dest[uint8_q + 2] = byte_alpha;
        span_dest[uint8_q + 3] = byte_alpha;
      }
    }
  }

  sheet_dest.CommitBufferedData(sprite_s.Bounds);
  return sprite_s;
}

}  // namespace ora::gfx
