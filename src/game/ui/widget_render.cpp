#include "game/ui/widget_render.hpp"

namespace ora::game::ui {

namespace {

const std::string* find_field(const WidgetModel& w, std::string_view str_key) {
  for (const auto& [str_k, str_v] : w.vec_fields)
    if (str_k == str_key) return &str_v;
  return nullptr;
}

char to_lower_ascii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool name_is(std::string_view str_value, std::string_view str_name) {
  if (str_value.size() != str_name.size()) return false;
  for (std::size_t sz_i{0}; sz_i < str_value.size(); ++sz_i)
    if (to_lower_ascii(str_value[sz_i]) != to_lower_ascii(str_name[sz_i])) return false;
  return true;
}

std::optional<std::uint32_t> parse_hex_color(std::string_view str_value) {
  if (!str_value.empty() && str_value.front() == '#') str_value.remove_prefix(1);
  if (str_value.size() != 6 && str_value.size() != 8) return std::nullopt;
  std::uint32_t u4_color{0};
  for (const char c : str_value) {
    int int4_digit{-1};
    if (c >= '0' && c <= '9') int4_digit = c - '0';
    else if (c >= 'a' && c <= 'f') int4_digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') int4_digit = c - 'A' + 10;
    if (int4_digit < 0) return std::nullopt;
    u4_color = (u4_color << 4) | static_cast<std::uint32_t>(int4_digit);
  }
  if (str_value.size() == 6) u4_color = (u4_color << 8) | 0xFFu;
  return u4_color;
}

void draw_label(const WidgetModel& w, UiRenderCtx& ctx, std::vector<UiQuad>& vec_out) {
  const std::string* ptr_text{find_field(w, "Text")};
  if (ptr_text == nullptr) return;
  const std::string* ptr_font_name{find_field(w, "Font")};
  const std::string& str_font{ptr_font_name != nullptr ? *ptr_font_name : ctx.str_default_font};
  const auto it_font{ctx.map_fonts->find(str_font)};
  if (it_font == ctx.map_fonts->end()) return;
  render::SpriteFont& font{it_font->second};

  std::u16string str16_text;
  for (const char c : *ptr_text) str16_text.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
  if (const std::string* ptr_wrap{find_field(w, "WordWrap")}; ptr_wrap != nullptr && name_is(*ptr_wrap, "true"))
    str16_text = render::spritefont_wrap_text(font, str16_text, w.bounds.int4_width);

  const render::FontMeasure size_text{render::spritefont_measure(font, str16_text)};
  const auto [int4_ox, int4_oy]{widget_render_origin(w)};
  int int4_x{int4_ox};
  int int4_y{int4_oy};
  const int int4_offset{font.int4_top_offset};

  const std::string* ptr_valign{find_field(w, "VAlign")};
  if (ptr_valign == nullptr || name_is(*ptr_valign, "Middle"))
    int4_y += (w.bounds.int4_height - size_text.int4_height - int4_offset) / 2;
  else if (name_is(*ptr_valign, "Top"))
    int4_y -= int4_offset;
  else if (name_is(*ptr_valign, "Bottom"))
    int4_y += w.bounds.int4_height - size_text.int4_height;

  const std::string* ptr_align{find_field(w, "Align")};
  if (ptr_align != nullptr && name_is(*ptr_align, "Center"))
    int4_x += (w.bounds.int4_width - size_text.int4_width) / 2;
  else if (ptr_align != nullptr && name_is(*ptr_align, "Right"))
    int4_x += w.bounds.int4_width - size_text.int4_width;

  std::uint32_t u4_color{ctx.u4_default_color};
  if (const std::string* ptr_color{find_field(w, "TextColor")})
    if (const auto opt_color{parse_hex_color(*ptr_color)}) u4_color = *opt_color;

  int4_y += font.int4_size;
  for (const char16_t ch : str16_text) {
    if (ch == u'\n') {
      int4_y += font.int4_size;
      continue;
    }
    const render::GlyphInfo& g{render::spritefont_glyph(font, ch)};
    if (g.sprite.int4_width > 0 && g.sprite.int4_height > 0)
      vec_out.push_back(UiQuad{int4_x + g.int4_off_x, int4_y + g.int4_off_y, g.sprite.int4_width,
                               g.sprite.int4_height, g.sprite.int4_left, g.sprite.int4_top,
                               g.sprite.u4_sheet, u4_color});
    int4_x += static_cast<int>(g.fp4_advance + 0.5f);
  }
}

void draw_widget(const WidgetModel& w, UiRenderCtx& ctx, std::vector<UiQuad>& vec_out) {
  if (!w.b_visible) return;
  if (w.str_type == "Label" || w.str_type == "LabelWithTooltip") draw_label(w, ctx, vec_out);
  for (const auto& ptr_child : w.vec_children) draw_widget(*ptr_child, ctx, vec_out);
}

}  // namespace

std::vector<UiQuad> ui_draw_tree(const WidgetModel& root, const UiRenderCtx& ctx) {
  UiRenderCtx ctx_mut{ctx};
  std::vector<UiQuad> vec_out;
  draw_widget(root, ctx_mut, vec_out);
  return vec_out;
}

}  // namespace ora::game::ui
