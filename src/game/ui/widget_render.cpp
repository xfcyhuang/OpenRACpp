#include "game/ui/widget_render.hpp"

import std;

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

std::string metrics_get(const UiRenderCtx& ctx, std::string_view str_key, std::string_view str_fallback) {
  if (ctx.map_metrics != nullptr)
    if (const auto it{ctx.map_metrics->find(std::string{str_key})}; it != ctx.map_metrics->end())
      return it->second;
  return std::string{str_fallback};
}

int metrics_int(const UiRenderCtx& ctx, std::string_view str_key, int int4_fallback) {
  int int4_value{};
  const std::string str_text{metrics_get(ctx, str_key, "")};
  if (str_text.empty()) return int4_fallback;
  const auto res{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_value)};
  if (res.ec != std::errc{} || res.ptr != str_text.data() + str_text.size()) return int4_fallback;
  return int4_value;
}

std::uint32_t metrics_color(const UiRenderCtx& ctx, std::string_view str_key, std::uint32_t u4_fallback) {
  const std::string str_value{metrics_get(ctx, str_key, "")};
  if (str_value.empty()) return u4_fallback;
  if (const auto opt_color{parse_hex_color(str_value)}) return *opt_color;
  return u4_fallback;
}

struct UiDrawSink {
  std::vector<UiQuad>* ptr_quads{};
  std::vector<std::string>* ptr_lines{};
};

struct ScreenRect {
  int int4_x{};
  int int4_y{};
  int int4_w{};
  int int4_h{};
};

void emit_quad(UiDrawSink& sink, std::uint32_t u4_sheet, int int4_src_x, int int4_src_y, int int4_x,
               int int4_y, int int4_w, int int4_h, std::uint32_t u4_rgba = 0xFFFFFFFFu) {
  if (sink.ptr_quads != nullptr)
    sink.ptr_quads->push_back(
        UiQuad{int4_x, int4_y, int4_w, int4_h, int4_src_x, int4_src_y, u4_sheet, u4_rgba});
}

void emit_fill_rect(UiDrawSink& sink, std::uint32_t u4_sheet, const ChromeRect& src, const ScreenRect& r) {
  for (int int4_x{r.int4_x}; int4_x < r.int4_x + r.int4_w; int4_x += src.int4_width) {
    for (int int4_y{r.int4_y}; int4_y < r.int4_y + r.int4_h; int4_y += src.int4_height) {
      const int int4_dx{r.int4_x + r.int4_w - int4_x};
      const int int4_dy{r.int4_y + r.int4_h - int4_y};
      int int4_w{src.int4_width};
      int int4_h{src.int4_height};
      if (int4_dx < src.int4_width || int4_dy < src.int4_height) {
        int4_w = std::min(src.int4_width, int4_dx);
        int4_h = std::min(src.int4_height, int4_dy);
      }
      if (sink.ptr_lines != nullptr)
        sink.ptr_lines->push_back(std::format("q {} {},{},{},{} {},{},{},{}", u4_sheet, src.int4_x,
                                              src.int4_y, int4_w, int4_h, int4_x, int4_y, int4_w, int4_h));
      emit_quad(sink, u4_sheet, src.int4_x, src.int4_y, int4_x, int4_y, int4_w, int4_h);
    }
  }
}

void emit_sprite(UiDrawSink& sink, std::uint32_t u4_sheet, const ChromeRect& src, float fp4_x, float fp4_y) {
  if (sink.ptr_lines != nullptr)
    sink.ptr_lines->push_back(std::format("img {} {},{},{},{} {},{},{},{}", u4_sheet, src.int4_x,
                                          src.int4_y, src.int4_width, src.int4_height, fp4_x, fp4_y,
                                          src.int4_width, src.int4_height));
  emit_quad(sink, u4_sheet, src.int4_x, src.int4_y, static_cast<int>(fp4_x), static_cast<int>(fp4_y),
            src.int4_width, src.int4_height);
}

std::uint32_t sheet_id_of(const UiRenderCtx& ctx, std::string_view str_collection) {
  if (ctx.set_sheets == nullptr || ctx.map_chrome == nullptr) return 0;
  const ChromeSheet* ptr_sheet{chrome_sheet_for_collection(*ctx.set_sheets, *ctx.map_chrome, str_collection)};
  return ptr_sheet != nullptr ? ptr_sheet->u4_id : 0;
}

void emit_panel(UiDrawSink& sink, const UiRenderCtx& ctx, std::string_view str_collection,
                const ScreenRect& r) {
  if (ctx.map_chrome == nullptr) return;
  const std::vector<std::optional<ChromeRect>> vec_slots{
      chrome_get_panel_images(*ctx.map_chrome, str_collection)};
  if (vec_slots.size() != 9) return;

  const std::uint32_t u4_sheet{sheet_id_of(ctx, str_collection)};
  const int int4_margin_top{vec_slots[1].has_value() ? vec_slots[1]->int4_height : 0};
  const int int4_margin_left{vec_slots[3].has_value() ? vec_slots[3]->int4_width : 0};
  const int int4_margin_right{vec_slots[5].has_value() ? vec_slots[5]->int4_width : 0};
  const int int4_margin_bottom{vec_slots[7].has_value() ? vec_slots[7]->int4_height : 0};
  const int int4_margin_w{int4_margin_right + int4_margin_left};
  const int int4_margin_h{int4_margin_bottom + int4_margin_top};

  if (vec_slots[4].has_value())
    emit_fill_rect(sink, u4_sheet, *vec_slots[4],
                   {r.int4_x + int4_margin_left, r.int4_y + int4_margin_top, r.int4_w - int4_margin_w,
                    r.int4_h - int4_margin_h});
  if (vec_slots[3].has_value())
    emit_fill_rect(sink, u4_sheet, *vec_slots[3],
                   {r.int4_x, r.int4_y + int4_margin_top, int4_margin_left, r.int4_h - int4_margin_h});
  if (vec_slots[5].has_value())
    emit_fill_rect(sink, u4_sheet, *vec_slots[5],
                   {r.int4_x + r.int4_w - int4_margin_right, r.int4_y + int4_margin_top,
                    int4_margin_left, r.int4_h - int4_margin_h});
  if (vec_slots[1].has_value())
    emit_fill_rect(sink, u4_sheet, *vec_slots[1],
                   {r.int4_x + int4_margin_left, r.int4_y, r.int4_w - int4_margin_w, int4_margin_top});
  if (vec_slots[7].has_value())
    emit_fill_rect(sink, u4_sheet, *vec_slots[7],
                   {r.int4_x + int4_margin_left, r.int4_y + r.int4_h - int4_margin_bottom,
                    r.int4_w - int4_margin_w, int4_margin_top});

  if (vec_slots[0].has_value()) emit_sprite(sink, u4_sheet, *vec_slots[0],
                                            static_cast<float>(r.int4_x), static_cast<float>(r.int4_y));
  if (vec_slots[2].has_value())
    emit_sprite(sink, u4_sheet, *vec_slots[2], static_cast<float>(r.int4_x + r.int4_w - vec_slots[2]->int4_width),
                static_cast<float>(r.int4_y));
  if (vec_slots[6].has_value())
    emit_sprite(sink, u4_sheet, *vec_slots[6], static_cast<float>(r.int4_x),
                static_cast<float>(r.int4_y + r.int4_h - vec_slots[6]->int4_height));
  if (vec_slots[8].has_value())
    emit_sprite(sink, u4_sheet, *vec_slots[8],
                static_cast<float>(r.int4_x + r.int4_w - vec_slots[8]->int4_width),
                static_cast<float>(r.int4_y + r.int4_h - vec_slots[8]->int4_height));
}

void emit_text_line(UiDrawSink& sink, std::string_view str_font, std::string_view str_align,
                    std::string_view str_valign, std::uint32_t u4_color, std::string_view str_text) {
  if (sink.ptr_lines != nullptr)
    sink.ptr_lines->push_back(std::format("t {} {} {} {:08X} {}", str_font, str_align, str_valign,
                                          ((u4_color & 0xFFu) << 24) | (u4_color >> 8), str_text));
}

bool widget_hovered(const UiRenderCtx& ctx, const WidgetModel& w) {
  return ctx.ptr_state != nullptr && ctx.ptr_state->ptr_mouse_over == &w;
}

std::string widget_field_or(const WidgetModel& w, std::string_view str_key, std::string_view str_fallback) {
  if (const std::string* ptr_value{find_field(w, str_key)}) return *ptr_value;
  return std::string{str_fallback};
}

void emit_glyphs(UiDrawSink& sink, render::SpriteFont& font, std::u16string str16_text, float fp4_x,
                 float fp4_y, std::uint32_t u4_color) {
  int int4_x{static_cast<int>(fp4_x)};
  int int4_y{static_cast<int>(fp4_y) + font.int4_size};
  for (const char16_t ch : str16_text) {
    if (ch == u'\n') {
      int4_y += font.int4_size;
      continue;
    }
    const render::GlyphInfo& g{render::spritefont_glyph(font, ch)};
    if (g.sprite.int4_width > 0 && g.sprite.int4_height > 0)
      emit_quad(sink, g.sprite.u4_sheet, g.sprite.int4_left, g.sprite.int4_top, int4_x + g.int4_off_x,
                int4_y + g.int4_off_y, g.sprite.int4_width, g.sprite.int4_height, u4_color);
    int4_x += static_cast<int>(g.fp4_advance + 0.5f);
  }
}

std::u16string widen_text(const std::string& str_text) {
  std::u16string str16_out;
  for (const char c : str_text) str16_out.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
  return str16_out;
}

void draw_label(const WidgetModel& w, UiRenderCtx& ctx, UiDrawSink& sink) {
  const std::string* ptr_text{find_field(w, "Text")};
  if (ptr_text == nullptr) return;
  const std::string* ptr_font_name{find_field(w, "Font")};
  const std::string str_font{ptr_font_name != nullptr ? *ptr_font_name
                                                      : metrics_get(ctx, "TextFont", ctx.str_default_font)};
  const std::string* ptr_align{find_field(w, "Align")};
  const std::string* ptr_valign{find_field(w, "VAlign")};
  std::uint32_t u4_color{ctx.u4_default_color};
  if (const std::string* ptr_color{find_field(w, "TextColor")})
    if (const auto opt_color{parse_hex_color(*ptr_color)}) u4_color = *opt_color;
  if (find_field(w, "TextColor") == nullptr) u4_color = metrics_color(ctx, "TextColor", u4_color);
  emit_text_line(sink, str_font, ptr_align != nullptr ? *ptr_align : "Left",
                 ptr_valign != nullptr ? *ptr_valign : "Middle", u4_color, *ptr_text);
  if (ctx.map_fonts == nullptr) return;
  const auto it_font{ctx.map_fonts->find(str_font)};
  if (it_font == ctx.map_fonts->end()) return;
  render::SpriteFont& font{it_font->second};

  std::u16string str16_text{widen_text(*ptr_text)};
  if (const std::string* ptr_wrap{find_field(w, "WordWrap")}; ptr_wrap != nullptr && name_is(*ptr_wrap, "true"))
    str16_text = render::spritefont_wrap_text(font, str16_text, w.bounds.int4_width);

  const render::FontMeasure size_text{render::spritefont_measure(font, str16_text)};
  const auto [int4_ox, int4_oy]{widget_render_origin(w)};
  int int4_x{int4_ox};
  int int4_y{int4_oy};
  const int int4_offset{font.int4_top_offset};

  if (ptr_valign == nullptr || name_is(*ptr_valign, "Middle"))
    int4_y += (w.bounds.int4_height - size_text.int4_height - int4_offset) / 2;
  else if (name_is(*ptr_valign, "Top"))
    int4_y -= int4_offset;
  else if (name_is(*ptr_valign, "Bottom"))
    int4_y += w.bounds.int4_height - size_text.int4_height;

  if (ptr_align != nullptr && name_is(*ptr_align, "Center"))
    int4_x += (w.bounds.int4_width - size_text.int4_width) / 2;
  else if (ptr_align != nullptr && name_is(*ptr_align, "Right"))
    int4_x += w.bounds.int4_width - size_text.int4_width;

  emit_glyphs(sink, font, std::move(str16_text), static_cast<float>(int4_x), static_cast<float>(int4_y),
              u4_color);
}

struct ButtonTextLayout {
  float fp4_x{};
  float fp4_y{};
  int int4_text_h{};
  int int4_top_offset{};
  std::string str_font;
  std::string str_align;
  std::string str_text;
  std::uint32_t u4_color{};
};

ButtonTextLayout button_text_layout(const WidgetModel& w, const UiRenderCtx& ctx, bool b_disabled,
                                    int int4_usable_width, int int4_visual_height) {
  const std::string str_font{widget_field_or(w, "Font", metrics_get(ctx, "ButtonFont", ctx.str_default_font))};
  const std::string str_text{widget_field_or(w, "Text", "")};

  int int4_text_w{0};
  int int4_text_h{0};
  int int4_top_offset{0};
  if (ctx.map_fonts != nullptr) {
    if (const auto it_font{ctx.map_fonts->find(str_font)}; it_font != ctx.map_fonts->end()) {
      const render::FontMeasure size_text{render::spritefont_measure(it_font->second, widen_text(str_text))};
      int4_text_w = size_text.int4_width;
      int4_text_h = size_text.int4_height;
      int4_top_offset = it_font->second.int4_top_offset;
    }
  }

  const WidgetBounds rb{widget_event_bounds(w)};
  const float fp4_y{static_cast<float>(rb.int4_y + (w.bounds.int4_height - int4_text_h - int4_top_offset) / 2)};
  const std::string str_align{widget_field_or(w, "Align", "Center")};
  const int int4_left_margin{5};
  const int int4_right_margin{5};

  float fp4_x{static_cast<float>(rb.int4_x)};
  if (name_is(str_align, "Left"))
    fp4_x += static_cast<float>(int4_left_margin);
  else if (name_is(str_align, "Right"))
    fp4_x += static_cast<float>(int4_usable_width - int4_text_w - int4_right_margin);
  else
    fp4_x += static_cast<float>(int4_usable_width - int4_text_w) / 2.0f;

  const std::uint32_t u4_color{
      b_disabled ? metrics_color(ctx, "ButtonTextColorDisabled", 0xFF808080u)
                 : metrics_color(ctx, "ButtonTextColor", 0xFFFFFFFFu)};

  const int int4_state_offset{w.b_depressed ? int4_visual_height : 0};
  return ButtonTextLayout{fp4_x + static_cast<float>(int4_state_offset),
                          fp4_y + static_cast<float>(int4_state_offset), int4_text_h, int4_top_offset,
                          str_font, str_align, str_text, u4_color};
}

void draw_button_face(const WidgetModel& w, UiRenderCtx& ctx, UiDrawSink& sink,
                      const ScreenRect& r_background, int int4_usable_width, const std::string& str_base,
                      bool b_fixed_text = false) {
  const bool b_disabled{w.b_disabled};
  const bool b_hover{widget_hovered(ctx, w)};
  const std::string str_variant{find_field(w, "Highlighted") != nullptr
          && name_is(*find_field(w, "Highlighted"), "true")
          ? str_base + "-highlighted"
          : str_base};
  const std::string str_image{widget_stateful_image_name(str_variant, b_disabled, w.b_depressed, b_hover)};
  emit_panel(sink, ctx, str_image, r_background);

  const int int4_visual_height{metrics_int(ctx, "ButtonDepth", 1)};
  ButtonTextLayout layout{button_text_layout(w, ctx, b_disabled, int4_usable_width, int4_visual_height)};
  if (b_fixed_text) {
    layout.fp4_x = static_cast<float>(widget_render_origin(w).first)
        + static_cast<float>(w.bounds.int4_height) * 1.5f;
    layout.fp4_y = static_cast<float>(
        widget_render_origin(w).second
        + (w.bounds.int4_height - layout.int4_text_h - layout.int4_top_offset) / 2);
  }
  emit_text_line(sink, layout.str_font, layout.str_align, "-", layout.u4_color, layout.str_text);
  if (ctx.map_fonts != nullptr)
    if (const auto it_font{ctx.map_fonts->find(layout.str_font)}; it_font != ctx.map_fonts->end())
      emit_glyphs(sink, it_font->second, widen_text(layout.str_text), layout.fp4_x, layout.fp4_y,
                  layout.u4_color);
}

std::optional<ChromeRect> cached_region(const UiRenderCtx& ctx, std::string_view str_collection,
                                        std::string_view str_base, bool b_disabled, bool b_pressed,
                                        bool b_hover) {
  if (ctx.map_chrome == nullptr) return std::nullopt;
  const std::string str_variant{
      widget_stateful_image_name(str_base, b_disabled, b_pressed, b_hover)};
  if (const auto opt_rect{chrome_try_get_image(*ctx.map_chrome, str_collection, str_variant)})
    return opt_rect;
  return chrome_try_get_image(*ctx.map_chrome, str_collection, str_base);
}

void draw_widget(const WidgetModel& w, UiRenderCtx& ctx, UiDrawSink& sink) {
  if (!w.b_visible) return;

  if (w.str_type == "Label" || w.str_type == "LabelWithTooltip") {
    draw_label(w, ctx, sink);
  } else if (w.str_type == "Background") {
    emit_panel(sink, ctx, widget_field_or(w, "Background", "dialog"),
               {widget_event_bounds(w).int4_x, widget_event_bounds(w).int4_y, w.bounds.int4_width,
                w.bounds.int4_height});
  } else if (w.str_type == "Button" || w.str_type == "MenuButton"
             || w.str_type == "ProductionTypeButton") {
    const WidgetBounds rb{widget_event_bounds(w)};
    draw_button_face(w, ctx, sink, {rb.int4_x, rb.int4_y, w.bounds.int4_width, w.bounds.int4_height},
                     w.bounds.int4_width, widget_field_or(w, "Background", "button"));
  } else if (w.str_type == "Checkbox") {
    const WidgetBounds rb{widget_event_bounds(w)};
    const int int4_size{w.bounds.int4_height};
    draw_button_face(w, ctx, sink, {rb.int4_x, rb.int4_y, int4_size, int4_size}, w.bounds.int4_width,
                     widget_field_or(w, "Background", "checkbox"), true);

    const std::string str_check_collection{
        "checkmark-" + widget_field_or(w, "Checkmark", "tick")};
    if (const auto opt_rect{cached_region(ctx, str_check_collection,
                                          w.b_checked ? "checked" : "unchecked", w.b_disabled,
                                          w.b_depressed, widget_hovered(ctx, w))}) {
      emit_sprite(sink, sheet_id_of(ctx, str_check_collection), *opt_rect,
                  static_cast<float>(rb.int4_x + int4_size
                                     - static_cast<int>((static_cast<float>(int4_size)
                                                         + static_cast<float>(opt_rect->int4_width))
                                        / 2)),
                  static_cast<float>(rb.int4_y
                                     + static_cast<int>((static_cast<float>(int4_size)
                                                         - static_cast<float>(opt_rect->int4_height))
                                        / 2)));
    }
  } else if (w.str_type == "Slider") {
    const SliderGeometry geo{widget_slider_geometry(w)};
    if (ctx.map_chrome != nullptr)
      if (const auto opt_tick{chrome_try_get_image(*ctx.map_chrome, "slider", "tick")}) {
      const std::uint32_t u4_sheet{sheet_id_of(ctx, "slider")};
      for (int int4_i{0}; int4_i < w.int4_ticks; ++int4_i) {
        const float fp4_x{static_cast<float>(geo.int4_track_origin_x)
            + static_cast<float>(int4_i * (geo.int4_track_w - opt_tick->int4_width) / (w.int4_ticks - 1))
            - static_cast<float>(opt_tick->int4_width) / 2.0f};
        emit_sprite(sink, u4_sheet, *opt_tick, fp4_x,
                    static_cast<float>(geo.int4_track_y + geo.int4_track_h));
      }
    }
    emit_panel(sink, ctx, widget_field_or(w, "Track", "slider-track"),
               {geo.int4_track_x, geo.int4_track_y, geo.int4_track_w, geo.int4_track_h});
    const bool b_thumb_hover{widget_hovered(ctx, w)
        && widget_slider_thumb_contains(w, ctx.ptr_state != nullptr ? ctx.ptr_state->int4_last_mouse_x : 0,
                                        ctx.ptr_state != nullptr ? ctx.ptr_state->int4_last_mouse_y : 0)};
    const std::string str_thumb{
        widget_stateful_image_name(widget_field_or(w, "Thumb", "slider-thumb"), w.b_disabled, w.b_moving,
                                   b_thumb_hover)};
    emit_panel(sink, ctx, str_thumb, {geo.int4_thumb_x, geo.int4_thumb_y, geo.int4_thumb_w,
                                      geo.int4_thumb_h});
  } else if (w.str_type == "DropDownButton") {
    const WidgetBounds rb{widget_event_bounds(w)};
    draw_button_face(w, ctx, sink, {rb.int4_x, rb.int4_y, w.bounds.int4_width, w.bounds.int4_height},
                     w.bounds.int4_width - w.bounds.int4_height,
                     widget_field_or(w, "Background", "button"));

    const int int4_visual_height{metrics_int(ctx, "ButtonDepth", 1)};
    const int int4_state_offset{w.b_depressed ? int4_visual_height : 0};
    if (const auto opt_marker{cached_region(ctx, "dropdown-decorations", "marker", w.b_disabled,
                                            w.b_depressed, widget_hovered(ctx, w))})
      emit_sprite(sink, sheet_id_of(ctx, "dropdown-decorations"), *opt_marker,
                  static_cast<float>(int4_state_offset + rb.int4_x + w.bounds.int4_width
                                     - static_cast<int>((static_cast<float>(w.bounds.int4_height)
                                                         + static_cast<float>(opt_marker->int4_width))
                                        / 2)),
                  static_cast<float>(int4_state_offset + rb.int4_y
                                     + static_cast<int>((static_cast<float>(w.bounds.int4_height)
                                                         - static_cast<float>(opt_marker->int4_height))
                                        / 2)));
    if (const auto opt_sep{cached_region(ctx, "dropdown-separators", "separator", w.b_disabled,
                                         w.b_depressed, widget_hovered(ctx, w))})
      emit_sprite(sink, sheet_id_of(ctx, "dropdown-separators"), *opt_sep,
                  static_cast<float>(int4_state_offset + -3 + rb.int4_x + w.bounds.int4_width
                                     - w.bounds.int4_height + 4),
                  static_cast<float>(int4_state_offset + rb.int4_y
                                     + static_cast<int>((static_cast<float>(w.bounds.int4_height)
                                                         - static_cast<float>(opt_sep->int4_height))
                                        / 2)));
  }

  for (const auto& ptr_child : w.vec_children) draw_widget(*ptr_child, ctx, sink);
}

}  // namespace

std::string widget_stateful_image_name(std::string_view str_base, bool b_disabled, bool b_pressed,
                                       bool b_hover) {
  std::string str_out{str_base};
  if (b_disabled) str_out += "-disabled";
  else if (b_pressed) str_out += "-pressed";
  else if (b_hover) str_out += "-hover";
  return str_out;
}

std::vector<UiQuad> ui_draw_tree(const WidgetModel& root, const UiRenderCtx& ctx) {
  UiRenderCtx ctx_mut{ctx};
  std::vector<UiQuad> vec_out;
  UiDrawSink sink{.ptr_quads = &vec_out};
  draw_widget(root, ctx_mut, sink);
  return vec_out;
}

std::vector<std::string> widgets_draw_dump(const WidgetModel& root, const UiRenderCtx& ctx) {
  UiRenderCtx ctx_mut{ctx};
  std::vector<std::string> vec_out;
  UiDrawSink sink{.ptr_lines = &vec_out};
  draw_widget(root, ctx_mut, sink);
  return vec_out;
}

}
