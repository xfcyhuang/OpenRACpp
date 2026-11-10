#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/render/stb_truetype.h"

namespace ora::game::render {

struct FontGlyph {
  int int4_off_x{};
  int int4_off_y{};
  int int4_width{};
  int int4_height{};
  float fp4_advance{};
  bool b_loaded{};
  std::vector<std::uint8_t> vec1u_data;
};

struct FontFace {
  std::vector<std::uint8_t> vec1u_ttf;
  stbtt_fontinfo st_info{};
  int int4_upem{};

  FontFace() = default;
  FontFace(const FontFace&) = delete;
  FontFace& operator=(const FontFace&) = delete;
  FontFace(FontFace&&) = default;
  FontFace& operator=(FontFace&&) = default;
};

[[nodiscard]] std::expected<FontFace, std::string> font_face_load(
    std::span<const std::uint8_t> span1u_bytes);

[[nodiscard]] FontGlyph font_create_glyph(
    const FontFace& face, char16_t ch, int int4_size, float fp4_device_scale);

struct SheetSprite {
  int int4_left{};
  int int4_top{};
  int int4_width{};
  int int4_height{};
  std::uint32_t u4_sheet{};
};

struct GlyphInfo {
  float fp4_advance{};
  int int4_off_x{};
  int int4_off_y{};
  SheetSprite sprite{};
  bool b_visible{};
};

struct SheetBuilder {
  int int4_sheet_size{512};
  int int4_margin{1};
  std::vector<std::vector<std::uint8_t>> vec_vec1u_sheets;
  int int4_cursor_x{};
  int int4_cursor_y{};
  int int4_row_height{};
};

[[nodiscard]] SheetSprite sheetbuilder_allocate(SheetBuilder& builder, int int4_width, int int4_height);

void sheetbuilder_write_glyph(SheetBuilder& builder, const SheetSprite& sprite, const FontGlyph& glyph);

struct SpriteFont {
  std::shared_ptr<const FontFace> face;
  SheetBuilder* ptr_builder{};
  int int4_size{};
  int int4_ascender{};
  int int4_top_offset{};
  float fp4_device_scale{1.0f};
  std::map<char16_t, GlyphInfo> map_glyphs;
};

[[nodiscard]] std::expected<SpriteFont, std::string> spritefont_create(
    std::shared_ptr<const FontFace> face, SheetBuilder& builder,
    int int4_size, int int4_ascender, float fp4_scale);

[[nodiscard]] const GlyphInfo& spritefont_glyph(SpriteFont& font, char16_t ch);

struct FontMeasure {
  int int4_width{};
  int int4_height{};
};

[[nodiscard]] FontMeasure spritefont_measure(SpriteFont& font, std::u16string_view str_text);

[[nodiscard]] std::u16string spritefont_wrap_text(
    SpriteFont& font, std::u16string_view str_text, int int4_width);

void spritefont_set_scale(SpriteFont& font, float fp4_scale);

struct FontDataEntry {
  std::string str_font;
  int int4_size{};
  int int4_ascender{};
};

[[nodiscard]] std::expected<std::vector<std::pair<std::string, FontDataEntry>>, std::string> fonts_load(
    const std::vector<ora::game::MiniYamlNode>& vec_mod_yaml_nodes);

}
