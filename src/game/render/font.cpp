#include "game/render/font.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "game/render/stb_truetype.h"

namespace ora::game::render {

namespace {

[[nodiscard]] std::uint16_t read_be16(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return static_cast<std::uint16_t>((span1u[sz_off] << 8) | span1u[sz_off + 1]);
}

[[nodiscard]] std::uint32_t read_be32(std::span<const std::uint8_t> span1u, std::size_t sz_off) {
  return (static_cast<std::uint32_t>(span1u[sz_off]) << 24)
      | (static_cast<std::uint32_t>(span1u[sz_off + 1]) << 16)
      | (static_cast<std::uint32_t>(span1u[sz_off + 2]) << 8)
      | static_cast<std::uint32_t>(span1u[sz_off + 3]);
}

[[nodiscard]] int parse_int_field(std::string_view str_value, int& int4_out) {
  int int4_parsed{};
  const auto* ptr_end{str_value.data() + str_value.size()};
  const auto res{std::from_chars(str_value.data(), ptr_end, int4_parsed)};
  if (res.ec != std::errc{} || res.ptr != ptr_end) return -1;
  int4_out = int4_parsed;
  return 0;
}

}

std::expected<FontFace, std::string> font_face_load(std::span<const std::uint8_t> span1u_bytes) {
  FontFace face;
  face.vec1u_ttf.assign(span1u_bytes.begin(), span1u_bytes.end());
  if (stbtt_InitFont(&face.st_info, face.vec1u_ttf.data(), 0) == 0)
    return std::unexpected("Failed to initialize font");

  face.int4_upem = 0;
  if (face.vec1u_ttf.size() >= 12) {
    const std::uint16_t u2_num_tables{read_be16(face.vec1u_ttf, 4)};
    for (std::uint16_t u2_t{0}; u2_t < u2_num_tables; ++u2_t) {
      const std::size_t sz_rec{12 + 16 * static_cast<std::size_t>(u2_t)};
      if (sz_rec + 16 > face.vec1u_ttf.size()) break;
      const std::uint32_t u4_tag{read_be32(face.vec1u_ttf, sz_rec)};
      if (u4_tag == 0x68656164) {
        const std::uint32_t u4_offset{read_be32(face.vec1u_ttf, sz_rec + 8)};
        if (u4_offset + 20 <= face.vec1u_ttf.size())
          face.int4_upem = static_cast<int>(read_be16(face.vec1u_ttf, u4_offset + 18));
        break;
      }
    }
  }
  if (face.int4_upem <= 0) return std::unexpected("Failed to initialize font");
  return face;
}

FontGlyph font_create_glyph(
    const FontFace& face, char16_t ch, int int4_size, float fp4_device_scale) {
  FontGlyph glyph;
  const std::uint32_t u4_scaled{
      static_cast<std::uint32_t>(static_cast<float>(int4_size) * fp4_device_scale)};
  if (u4_scaled == 0) return glyph;

  const float fp4_scale{static_cast<float>(u4_scaled) / static_cast<float>(face.int4_upem)};

  int int4_adv_w{};
  int int4_lsb{};
  stbtt_GetCodepointHMetrics(&face.st_info, static_cast<int>(ch), &int4_adv_w, &int4_lsb);
  glyph.fp4_advance =
      static_cast<float>(static_cast<int>(static_cast<float>(int4_adv_w) * fp4_scale + 0.5f));

  int int4_w{};
  int int4_h{};
  int int4_ox{};
  int int4_oy{};
  std::uint8_t* ptr_bmp{stbtt_GetCodepointBitmap(
      &face.st_info, fp4_scale, fp4_scale, static_cast<int>(ch), &int4_w, &int4_h, &int4_ox, &int4_oy)};
  glyph.b_loaded = true;
  glyph.int4_width = int4_w;
  glyph.int4_height = int4_h;
  glyph.int4_off_x = int4_ox;
  glyph.int4_off_y = int4_oy;
  if (ptr_bmp != nullptr) {
    glyph.vec1u_data.assign(ptr_bmp, ptr_bmp + static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_h));
    stbtt_FreeBitmap(ptr_bmp, nullptr);
  }
  return glyph;
}

SheetSprite sheetbuilder_allocate(SheetBuilder& builder, int int4_width, int int4_height) {
  if (builder.vec_vec1u_sheets.empty())
    builder.vec_vec1u_sheets.emplace_back(
        static_cast<std::size_t>(builder.int4_sheet_size) * static_cast<std::size_t>(builder.int4_sheet_size) * 4, 0);

  if (int4_width + builder.int4_cursor_x + builder.int4_margin > builder.int4_sheet_size) {
    builder.int4_cursor_x = 0;
    builder.int4_cursor_y += builder.int4_row_height + builder.int4_margin;
    builder.int4_row_height = int4_height;
  }
  if (int4_height > builder.int4_row_height)
    builder.int4_row_height = int4_height;

  if (builder.int4_cursor_y + int4_height + builder.int4_margin > builder.int4_sheet_size) {
    builder.vec_vec1u_sheets.emplace_back(
        static_cast<std::size_t>(builder.int4_sheet_size) * static_cast<std::size_t>(builder.int4_sheet_size) * 4, 0);
    builder.int4_row_height = int4_height;
    builder.int4_cursor_x = 0;
    builder.int4_cursor_y = 0;
  }

  SheetSprite sprite;
  sprite.int4_left = builder.int4_cursor_x + builder.int4_margin;
  sprite.int4_top = builder.int4_cursor_y + builder.int4_margin;
  sprite.int4_width = int4_width;
  sprite.int4_height = int4_height;
  sprite.u4_sheet = static_cast<std::uint32_t>(builder.vec_vec1u_sheets.size() - 1);
  builder.int4_cursor_x += int4_width + builder.int4_margin;
  return sprite;
}

void sheetbuilder_write_glyph(SheetBuilder& builder, const SheetSprite& sprite, const FontGlyph& glyph) {
  if (sprite.int4_width <= 0 || sprite.int4_height <= 0) return;
  if (static_cast<std::size_t>(sprite.u4_sheet) >= builder.vec_vec1u_sheets.size()) return;
  auto& vec1u_sheet{builder.vec_vec1u_sheets[sprite.u4_sheet]};
  const std::size_t sz_stride{static_cast<std::size_t>(builder.int4_sheet_size) * 4};
  std::size_t sz_src{0};
  for (int int4_j{0}; int4_j < sprite.int4_height; ++int4_j) {
    for (int int4_i{0}; int4_i < sprite.int4_width; ++int4_i) {
      const std::uint8_t u1_p{glyph.vec1u_data[sz_src++]};
      if (u1_p != 0) {
        const std::size_t sz_q{sz_stride * static_cast<std::size_t>(int4_j + sprite.int4_top)
            + 4 * static_cast<std::size_t>(int4_i + sprite.int4_left)};
        vec1u_sheet[sz_q] = u1_p;
        vec1u_sheet[sz_q + 1] = u1_p;
        vec1u_sheet[sz_q + 2] = u1_p;
        vec1u_sheet[sz_q + 3] = u1_p;
      }
    }
  }
}

std::expected<SpriteFont, std::string> spritefont_create(
    std::shared_ptr<const FontFace> face, SheetBuilder& builder,
    int int4_size, int int4_ascender, float fp4_scale) {
  if (!face) return std::unexpected("Failed to initialize font");
  SpriteFont font;
  font.face = std::move(face);
  font.ptr_builder = &builder;
  font.int4_size = int4_size;
  font.int4_ascender = int4_ascender;
  font.int4_top_offset = int4_size - int4_ascender;
  font.fp4_device_scale = fp4_scale;
  if (int4_size <= 24)
    for (char16_t ch{0x20}; ch < 0x7f; ++ch) static_cast<void>(spritefont_glyph(font, ch));
  return font;
}

const GlyphInfo& spritefont_glyph(SpriteFont& font, char16_t ch) {
  const auto it{font.map_glyphs.find(ch)};
  if (it != font.map_glyphs.end()) return it->second;

  const FontGlyph glyph{
      font_create_glyph(*font.face, ch, font.int4_size, font.fp4_device_scale)};
  GlyphInfo info;
  if (glyph.b_loaded) {
    info.fp4_advance = glyph.fp4_advance;
    info.int4_off_x = glyph.int4_off_x;
    info.int4_off_y = glyph.int4_off_y;
    info.sprite = sheetbuilder_allocate(*font.ptr_builder, glyph.int4_width, glyph.int4_height);
    info.b_visible = true;
    sheetbuilder_write_glyph(*font.ptr_builder, info.sprite, glyph);
  }
  return font.map_glyphs.emplace(ch, info).first->second;
}

FontMeasure spritefont_measure(SpriteFont& font, std::u16string_view str_text) {
  if (str_text.empty()) return {0, font.int4_size};

  float fp4_max_width{0.0f};
  int int4_rows{0};
  std::u16string_view str_rest{str_text};
  while (true) {
    const std::size_t idx_eol{str_rest.find(u'\n')};
    const std::u16string_view str_line{idx_eol == std::u16string_view::npos
            ? str_rest
            : str_rest.substr(0, idx_eol)};
    ++int4_rows;
    float fp4_line{0.0f};
    for (const char16_t ch : str_line) fp4_line += spritefont_glyph(font, ch).fp4_advance;
    fp4_max_width = std::max(fp4_max_width, fp4_line / font.fp4_device_scale);
    if (idx_eol == std::u16string_view::npos) break;
    str_rest.remove_prefix(idx_eol + 1);
  }

  return {static_cast<int>(std::ceil(static_cast<double>(fp4_max_width))),
      int4_rows * font.int4_size};
}

std::u16string spritefont_wrap_text(
    SpriteFont& font, std::u16string_view str_text, int int4_width) {
  const int int4_text_width{spritefont_measure(font, str_text).int4_width};
  if (int4_text_width <= int4_width) return std::u16string{str_text};

  std::vector<std::u16string> vec_lines;
  {
    std::u16string_view str_rest{str_text};
    while (true) {
      const std::size_t idx_eol{str_rest.find(u'\n')};
      if (idx_eol == std::u16string_view::npos) {
        vec_lines.emplace_back(str_rest);
        break;
      }
      vec_lines.emplace_back(str_rest.substr(0, idx_eol));
      str_rest.remove_prefix(idx_eol + 1);
    }
  }

  for (std::size_t sz_i{0}; sz_i < vec_lines.size(); ++sz_i) {
    const std::u16string& str_line{vec_lines[sz_i]};
    if (spritefont_measure(font, str_line).int4_width <= int4_width) continue;

    std::size_t sz_start{0};
    while (true) {
      const std::size_t idx_space{str_line.find(u' ', sz_start)};
      if (idx_space == std::u16string::npos) break;
      if (spritefont_measure(font, std::u16string_view{str_line}.substr(0, idx_space)).int4_width
          > int4_width)
        break;
      sz_start = idx_space + 1;
    }

    if (sz_start > 0) {
      const std::u16string str_head{str_line.substr(0, sz_start - 1)};
      const std::u16string str_tail{str_line.substr(sz_start)};
      vec_lines[sz_i] = str_head;
      vec_lines.insert(vec_lines.begin() + static_cast<std::ptrdiff_t>(sz_i) + 1, str_tail);
    }
  }

  std::u16string str_out;
  for (std::size_t sz_i{0}; sz_i < vec_lines.size(); ++sz_i) {
    if (sz_i > 0) str_out.push_back(u'\n');
    str_out += vec_lines[sz_i];
  }
  return str_out;
}

void spritefont_set_scale(SpriteFont& font, float fp4_scale) {
  font.fp4_device_scale = fp4_scale;
  font.map_glyphs.clear();
}

std::expected<std::vector<std::pair<std::string, FontDataEntry>>, std::string> fonts_load(
    const std::vector<ora::game::MiniYamlNode>& vec_mod_yaml_nodes) {
  const ora::game::MiniYamlNode* ptr_fonts{nullptr};
  for (const ora::game::MiniYamlNode& node : vec_mod_yaml_nodes)
    if (node.str_key == "Fonts") ptr_fonts = &node;
  if (ptr_fonts == nullptr)
    return std::unexpected("fonts section missing in mod.yaml");

  std::vector<std::pair<std::string, FontDataEntry>> vec_out;
  for (const ora::game::MiniYamlNode& node : ptr_fonts->vec_children) {
    FontDataEntry entry;
    for (const ora::game::MiniYamlNode& child : node.vec_children) {
      if (child.str_key == "Font") {
        entry.str_font = child.str_value;
      } else if (child.str_key == "Size") {
        if (parse_int_field(child.str_value, entry.int4_size) != 0)
          return std::unexpected(std::format("Font {}: invalid Size", node.str_key));
      } else if (child.str_key == "Ascender") {
        if (parse_int_field(child.str_value, entry.int4_ascender) != 0)
          return std::unexpected(std::format("Font {}: invalid Ascender", node.str_key));
      }
    }
    for (const auto& [str_name, entry_existing] : vec_out)
      if (str_name == node.str_key)
        return std::unexpected(std::format("duplicate font {}", node.str_key));
    vec_out.emplace_back(node.str_key, entry);
  }
  return vec_out;
}

}
