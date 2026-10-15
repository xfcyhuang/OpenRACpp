// UPSTREAM: OpenRA.Mods.Common/Widgets/WidgetUtils.cs @e180e3225(widget_utils.hpp
// 的实现)
// The implementation of widget_utils.hpp.
import std;

#include "ui/widget_utils.hpp"

#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/sprite_renderer.hpp"
#include "ui/ui.hpp"
#include "net/text_notifications.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

gfx::Renderer& RequireRenderer() {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  if (ptr_renderer == nullptr)
    throw std::runtime_error("Ui renderer deps not initialized.");
  return *ptr_renderer;
}

gfx::ChromeProvider* ChromeProviderOrNull() {
  return Ui::Instance().ChromeProviderDeps();
}

game::ModData* ModDataOrNull() {
  return Ui::Instance().ModDataDeps();
}

ChromeMetrics& RequireChromeMetrics() {
  ChromeMetrics* ptr_metrics = Ui::Instance().ChromeMetricsDeps();
  if (ptr_metrics == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return *ptr_metrics;
}

gfx::SpriteFont& RequireWidgetFont(const std::string& str_font) {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  gfx::SpriteFont* ptr_font =
      ptr_renderer != nullptr ? ptr_renderer->TryGetFont(str_font) : nullptr;
  if (ptr_font == nullptr)
    ptr_font = Ui::Instance().TryGetFontDeps(str_font);
  if (ptr_font == nullptr)
    throw std::runtime_error("Requested font '" + str_font + "' was not found.");
  return *ptr_font;
}

std::string GetStatefulImageName(std::string_view str_base_name, bool b_disabled,
                                 bool b_pressed, bool b_hover, bool b_focused) {
  std::string_view sv_suffix = b_disabled   ? "-disabled"
                               : b_focused  ? "-focused"
                               : b_pressed  ? "-pressed"
                               : b_hover    ? "-hover"
                                            : "";
  return std::string{str_base_name} + std::string{sv_suffix};
}

StatefulImageCache GetCachedStatefulImage(std::string str_collection,
                                          std::string str_image_name) {
  const std::string str_collection_owned = std::move(str_collection);
  const std::string str_image_owned = std::move(str_image_name);
  return StatefulImageCache(
      [str_collection_owned, str_image_owned](const StatefulImageKey& key_args) -> gfx::Sprite {
        const auto& [b_disabled, b_pressed, b_hover, b_focused, b_highlighted] = key_args;
        const std::string str_collection_name =
            str_collection_owned + (b_highlighted ? "-highlighted" : "");
        const std::string str_variant_image_name = GetStatefulImageName(
            str_image_owned, b_disabled, b_pressed, b_hover, b_focused);

        gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
        if (ptr_chrome == nullptr)
          return gfx::Sprite{};

        gfx::Sprite sprite_variant =
            ptr_chrome->TryGetImage(str_collection_name, str_variant_image_name);
        if (sprite_variant.ptr_sheet != nullptr)
          return sprite_variant;
        return ptr_chrome->GetImage(str_collection_name, str_image_owned);
      });
}

StatefulPanelCache GetCachedStatefulPanelImages(std::string str_collection) {
  const std::string str_collection_owned = std::move(str_collection);
  return StatefulPanelCache(
      [str_collection_owned](const StatefulImageKey& key_args) {
        const auto& [b_disabled, b_pressed, b_hover, b_focused, b_highlighted] = key_args;
        const std::string str_collection_name =
            str_collection_owned + (b_highlighted ? "-highlighted" : "");
        const std::string str_variant_collection_name = GetStatefulImageName(
            str_collection_name, b_disabled, b_pressed, b_hover, b_focused);

        gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
        if (ptr_chrome == nullptr)
          return std::vector<std::optional<gfx::Sprite>>{};

        std::vector<std::optional<gfx::Sprite>> vec_sprites =
            ptr_chrome->TryGetPanelImages(str_variant_collection_name);
        if (!vec_sprites.empty())
          return vec_sprites;
        return ptr_chrome->GetPanelImages(str_collection_name);
      });
}

void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos) {
  RequireRenderer().UIRgbaSpriteRenderer().DrawSprite(
      sprite_s, core::Vector3{vec_pos.X, vec_pos.Y, 0.0f});
}

void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, int2 int2_size) {
  DrawSprite(sprite_s, vec_pos,
             core::Vector2{static_cast<float>(int2_size.X), static_cast<float>(int2_size.Y)});
}

void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, core::Vector2 vec_size) {
  const core::Vector3 vec_scale{vec_size.X / sprite_s.vec_size.X, vec_size.Y / sprite_s.vec_size.Y,
                                1.0f};
  RequireRenderer().UIRgbaSpriteRenderer().DrawSprite(
      sprite_s, core::Vector3{vec_pos.X, vec_pos.Y, 0.0f}, vec_scale);
}

void DrawSpriteCentered(const gfx::Sprite& sprite_s, const gfx::PaletteReference* ptr_palette,
                        core::Vector2 vec_pos, float fp4_scale) {
  const core::Vector3 vec_location{
      vec_pos.X - 0.5f * fp4_scale * sprite_s.vec_size.X,
      vec_pos.Y - 0.5f * fp4_scale * sprite_s.vec_size.Y,
      0.0f};
  RequireRenderer().UISpriteRenderer().DrawSprite(sprite_s, ptr_palette, vec_location, fp4_scale);
}

void DrawPanel(std::string_view str_collection, Rectangle rect_bounds) {
  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  if (ptr_chrome == nullptr)
    return;
  std::vector<std::optional<gfx::Sprite>> vec_sprites =
      ptr_chrome->TryGetPanelImages(std::string{str_collection});
  if (!vec_sprites.empty())
    DrawPanel(rect_bounds, vec_sprites);
}

void FillRectWithSprite(Rectangle rect_r, const gfx::Sprite& sprite_s) {
  const float fp4_scale = sprite_s.vec_size.X / sprite_s.Bounds.Width;
  for (float fp4_x = static_cast<float>(rect_r.Left()); fp4_x < rect_r.Right();
       fp4_x += sprite_s.vec_size.X) {
    for (float fp4_y = static_cast<float>(rect_r.Top()); fp4_y < rect_r.Bottom();
         fp4_y += sprite_s.vec_size.Y) {
      gfx::Sprite sprite_ss = sprite_s;
      const float fp4_dx = static_cast<float>(rect_r.Right()) - fp4_x;
      const float fp4_dy = static_cast<float>(rect_r.Bottom()) - fp4_y;
      if (fp4_dx < sprite_s.vec_size.X || fp4_dy < sprite_s.vec_size.Y) {
        const Rectangle rect_rr{
            sprite_s.Bounds.Left(), sprite_s.Bounds.Top(),
            std::min(sprite_s.Bounds.Width,
                     static_cast<int>(fp4_dx / fp4_scale)),
            std::min(sprite_s.Bounds.Height,
                     static_cast<int>(fp4_dy / fp4_scale))};
        sprite_ss = gfx::Sprite(*sprite_s.ptr_sheet, rect_rr, sprite_s.kind_channel,
                                sprite_s.vec_size.X / sprite_s.Bounds.Width);
      }

      DrawSprite(sprite_ss, core::Vector2{fp4_x, fp4_y});
    }
  }
}

void FillRectWithColor(Rectangle rect_r, core::Color color_c) {
  const core::Vector3 vec_tl{static_cast<float>(rect_r.Left()) - 0.5f,
                             static_cast<float>(rect_r.Top()) - 0.5f, 0.0f};
  const core::Vector3 vec_br{static_cast<float>(rect_r.Right()) - 0.5f,
                             static_cast<float>(rect_r.Bottom()) - 0.5f, 0.0f};
  RequireRenderer().UIRgbaColorRenderer().FillRect(vec_tl, vec_br, color_c);
}

void FillRectWithColor(Rectangle rect_r, core::Color color_top_left, core::Color color_top_right,
                       core::Color color_bottom_right, core::Color color_bottom_left) {
  const core::Vector3 vec_tl{static_cast<float>(rect_r.Left()) - 0.5f,
                             static_cast<float>(rect_r.Top()) - 0.5f, 0.0f};
  const core::Vector3 vec_br{static_cast<float>(rect_r.Right()) - 0.5f,
                             static_cast<float>(rect_r.Bottom()) - 0.5f, 0.0f};

  const core::Vector3 vec_tr{vec_br.X, vec_tl.Y, 0.0f};
  const core::Vector3 vec_bl{vec_tl.X, vec_br.Y, 0.0f};

  RequireRenderer().UIRgbaColorRenderer().FillRect(vec_tl, vec_tr, vec_br, vec_bl, color_top_left,
                                                   color_top_right, color_bottom_right,
                                                   color_bottom_left);
}

void FillEllipseWithColor(Rectangle rect_r, core::Color color_c) {
  const core::Vector3 vec_tl{static_cast<float>(rect_r.Left()), static_cast<float>(rect_r.Top()),
                             0.0f};
  const core::Vector3 vec_br{static_cast<float>(rect_r.Right()),
                             static_cast<float>(rect_r.Bottom()), 0.0f};
  RequireRenderer().UIRgbaColorRenderer().FillEllipse(vec_tl, vec_br, color_c);
}

Rectangle InflateBy(const Rectangle& rect_input, int int4_l, int int4_t, int int4_r, int int4_b) {
  return Rectangle::FromLTRB(rect_input.Left() - int4_l, rect_input.Top() - int4_t,
                             rect_input.Right() + int4_r, rect_input.Bottom() + int4_b);
}

void DrawPanel(Rectangle rect_bounds, const std::vector<std::optional<gfx::Sprite>>& vec_sprites) {
  if (vec_sprites.size() != 9)
    return;

  const auto size_or = [](const std::optional<gfx::Sprite>& sprite_opt, bool b_x) {
    if (!sprite_opt.has_value())
      return 0;
    return b_x ? static_cast<int>(sprite_opt->vec_size.X)
               : static_cast<int>(sprite_opt->vec_size.Y);
  };

  const int int4_margin_top = size_or(vec_sprites[1], false);
  const int int4_margin_left = size_or(vec_sprites[3], true);
  const int int4_margin_right = size_or(vec_sprites[5], true);
  const int int4_margin_bottom = size_or(vec_sprites[7], false);
  const int int4_margin_width = int4_margin_right + int4_margin_left;
  const int int4_margin_height = int4_margin_bottom + int4_margin_top;

  if (vec_sprites[4].has_value())
    FillRectWithSprite(
        Rectangle{rect_bounds.Left() + int4_margin_left, rect_bounds.Top() + int4_margin_top,
                  rect_bounds.Width - int4_margin_width, rect_bounds.Height - int4_margin_height},
        *vec_sprites[4]);

  if (vec_sprites[3].has_value())
    FillRectWithSprite(
        Rectangle{rect_bounds.Left(), rect_bounds.Top() + int4_margin_top, int4_margin_left,
                  rect_bounds.Height - int4_margin_height},
        *vec_sprites[3]);

  if (vec_sprites[5].has_value())
    FillRectWithSprite(
        Rectangle{rect_bounds.Right() - int4_margin_right, rect_bounds.Top() + int4_margin_top,
                  int4_margin_left, rect_bounds.Height - int4_margin_height},
        *vec_sprites[5]);

  if (vec_sprites[1].has_value())
    FillRectWithSprite(Rectangle{rect_bounds.Left() + int4_margin_left, rect_bounds.Top(),
                                 rect_bounds.Width - int4_margin_width, int4_margin_top},
                       *vec_sprites[1]);

  if (vec_sprites[7].has_value())
    FillRectWithSprite(
        Rectangle{rect_bounds.Left() + int4_margin_left,
                  rect_bounds.Bottom() - int4_margin_bottom, rect_bounds.Width - int4_margin_width,
                  int4_margin_top},
        *vec_sprites[7]);

  if (vec_sprites[0].has_value())
    DrawSprite(*vec_sprites[0],
               core::Vector2{static_cast<float>(rect_bounds.Left()),
                             static_cast<float>(rect_bounds.Top())});

  if (vec_sprites[2].has_value())
    DrawSprite(*vec_sprites[2],
               core::Vector2{static_cast<float>(rect_bounds.Right()) - vec_sprites[2]->vec_size.X,
                             static_cast<float>(rect_bounds.Top())});

  if (vec_sprites[6].has_value())
    DrawSprite(*vec_sprites[6],
               core::Vector2{static_cast<float>(rect_bounds.Left()),
                             static_cast<float>(rect_bounds.Bottom()) -
                                 vec_sprites[6]->vec_size.Y});

  if (vec_sprites[8].has_value())
    DrawSprite(*vec_sprites[8],
               core::Vector2{static_cast<float>(rect_bounds.Right()) - vec_sprites[8]->vec_size.X,
                             static_cast<float>(rect_bounds.Bottom()) -
                                 vec_sprites[8]->vec_size.Y});
}

std::string FormatTime(int int4_ticks, int int4_timestep) {
  return FormatTime(int4_ticks, true, int4_timestep);
}

std::string FormatTime(int int4_ticks, bool b_leading_minute_zero, int int4_timestep) {
  const int int4_seconds =
      static_cast<int>(std::ceil(int4_ticks * int4_timestep / 1000.0f));
  return FormatTimeSeconds(int4_seconds, b_leading_minute_zero);
}

std::string FormatTimeSeconds(int int4_seconds) {
  return FormatTimeSeconds(int4_seconds, true);
}

std::string FormatTimeSeconds(int int4_seconds, bool b_leading_minute_zero) {
  const int int4_minutes = int4_seconds / 60;

  if (int4_minutes >= 60)
    return std::format("{}:{:02}:{:02}", int4_minutes / 60, int4_minutes % 60,
                       int4_seconds % 60);
  if (b_leading_minute_zero)
    return std::format("{:02}:{:02}", int4_minutes, int4_seconds % 60);
  return std::format("{}:{:02}", int4_minutes, int4_seconds % 60);
}

std::string WrapText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text) {
  const int2 int2_text_size = font_text.Measure(str_text);
  if (int2_text_size.X > int4_width) {
    std::vector<std::string> vec_lines;
    std::size_t int8_start = 0;
    while (int8_start <= str_text.size()) {
      const std::size_t int8_nl = str_text.find('\n', int8_start);
      vec_lines.push_back(
          int8_nl == std::string::npos ? str_text.substr(int8_start)
                                       : str_text.substr(int8_start, int8_nl - int8_start));
      if (int8_nl == std::string::npos)
        break;
      int8_start = int8_nl + 1;
    }

    for (std::size_t int8_i = 0; int8_i < vec_lines.size(); int8_i++) {
      std::string& str_line = vec_lines[int8_i];
      if (font_text.Measure(str_line).X <= int4_width)
        continue;

      std::size_t int8_start = 0;
      while (true) {
        const std::size_t int8_space = str_line.find(' ', int8_start);
        if (int8_space == std::string::npos)
          break;

        const int int4_fragment_width =
            font_text.Measure(str_line.substr(0, int8_space)).X;
        if (int4_fragment_width > int4_width)
          break;

        int8_start = int8_space + 1;
      }

      if (int8_start > 0) {
        const std::string str_tail = str_line.substr(int8_start);
        str_line = str_line.substr(0, int8_start - 1);
        vec_lines.insert(vec_lines.begin() + static_cast<std::ptrdiff_t>(int8_i) + 1, str_tail);
      }
    }

    std::string str_ret;
    for (std::size_t int8_i = 0; int8_i < vec_lines.size(); int8_i++) {
      if (int8_i > 0)
        str_ret += '\n';
      str_ret += vec_lines[int8_i];
    }
    return str_ret;
  }

  return str_text;
}

std::string TruncateText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text) {
  int int4_trimmed_width = font_text.Measure(str_text).X;
  if (int4_trimmed_width <= int4_width)
    return str_text;

  std::string str_trimmed = str_text;
  while (int4_trimmed_width > int4_width &&
         str_trimmed.size() > 3) {
    str_trimmed = str_text.substr(0, str_trimmed.size() - 4) + "...";
    int4_trimmed_width = font_text.Measure(str_trimmed).X;
  }

  return str_trimmed;
}


void TruncateButtonToTooltip(ButtonWidget& button_target, const std::string& str_text) {
  const std::string str_truncated_text =
      TruncateText(str_text,
                   button_target.bounds_widget.int4_width - button_target.int4_left_margin -
                       button_target.int4_right_margin,
                   RequireRenderer().Font(button_target.str_font));

  button_target.fn_get_text = [str_truncated_text] { return str_truncated_text; };

  if (str_text != str_truncated_text)
    button_target.fn_get_tooltip_text = [str_text] { return str_text; };
  else
    button_target.fn_get_tooltip_text = nullptr;
}

void BindButtonIcon(ButtonWidget& button_target) {
  ImageWidget* ptr_icon = button_target.GetOrNull<ImageWidget>("ICON");

  StatefulImageCache cache_icon = GetCachedStatefulImage(ptr_icon->str_image_collection,
                                                         ptr_icon->str_image_name);
  ButtonWidget* ptr_button = &button_target;
  ptr_icon->fn_get_sprite = [cache_icon = std::move(cache_icon), ptr_button]() mutable {
    return cache_icon.Update(StatefulImageKey{ptr_button->fn_is_disabled(),
                                              ptr_button->b_depressed,
                                              Ui::Instance().MouseOverWidget() == ptr_button,
                                              false, ptr_button->fn_is_highlighted()});
  };
}
std::string FormatCurrent(std::string_view str_format, double fp8_value) {
  const std::size_t sz_slot = str_format.find("{0}");
  if (sz_slot == std::string_view::npos)
    return std::string{str_format};
  return std::string{str_format.substr(0, sz_slot)} +
         std::format("{}", fp8_value) +
         std::string{str_format.substr(sz_slot + 3)};
}

void SetupTextNotification(Widget& widget_notification,
                           const net::TextNotification& notification,
                           int int4_box_width, bool b_with_timestamp) {
  LabelWidget* ptr_time_label = widget_notification.GetOrNull<LabelWidget>("TIME");
  LabelWidget* ptr_prefix_label = widget_notification.GetOrNull<LabelWidget>("PREFIX");
  LabelWidget* ptr_text_label = widget_notification.Get<LabelWidget>("TEXT");

  gfx::SpriteFont& font_text = RequireWidgetFont(ptr_text_label->str_font);
  int int4_text_width =
      int4_box_width - widget_notification.bounds_widget.int4_x -
      ptr_text_label->bounds_widget.int4_x;

  const bool b_has_prefix = !notification.Prefix().empty() && ptr_prefix_label != nullptr;
  int int4_time_offset = 0;

  if (b_with_timestamp && ptr_time_label != nullptr) {
    const auto time_total = std::chrono::floor<std::chrono::minutes>(
        notification.time_time.time_since_epoch());
    const auto time_day = std::chrono::floor<std::chrono::hours>(notification.time_time.time_since_epoch());
    const int int4_hour = static_cast<int>((time_total - time_day).count() / 60);
    const int int4_minute = static_cast<int>((time_total - time_day).count() % 60);
    const std::string str_time = std::format("{:02}:{:02}", int4_hour, int4_minute);
    int4_time_offset =
        ptr_time_label->bounds_widget.int4_width + ptr_time_label->bounds_widget.int4_x;

    ptr_time_label->fn_get_text = [str_time] { return str_time; };

    int4_text_width -= int4_time_offset;
    ptr_time_label->bounds_widget.int4_x += int4_time_offset;

    if (b_has_prefix)
      ptr_prefix_label->bounds_widget.int4_x += int4_time_offset;
  }

  if (b_has_prefix) {
    const std::string str_prefix = notification.Prefix() + ":";
    const int2 int2_prefix_size =
        RequireWidgetFont(ptr_prefix_label->str_font).Measure(str_prefix);
    const int int4_prefix_offset =
        int2_prefix_size.X + ptr_prefix_label->bounds_widget.int4_x;

    ptr_prefix_label->fn_get_color = [ptr_prefix_label,
                                       notification_copy = notification]() {
      return notification_copy.PrefixColor().value_or(ptr_prefix_label->color_text);
    };
    ptr_prefix_label->fn_get_text = [str_prefix] { return str_prefix; };
    ptr_prefix_label->bounds_widget.int4_width = int2_prefix_size.X;

    int4_text_width -= int4_prefix_offset;
    ptr_text_label->bounds_widget.int4_x += int4_prefix_offset - int4_time_offset;
  }

  ptr_text_label->fn_get_color = [ptr_text_label,
                                    notification_copy = notification]() {
    return notification_copy.TextColor().value_or(ptr_text_label->color_text);
  };
  ptr_text_label->bounds_widget.int4_width = int4_text_width;

  const std::string str_text =
      WrapText(notification.Text(), ptr_text_label->bounds_widget.int4_width, font_text);
  ptr_text_label->fn_get_text = [str_text] { return str_text; };
  const int int4_dh =
      font_text.Measure(str_text).Y - ptr_text_label->bounds_widget.int4_height;
  if (int4_dh > 0) {
    ptr_text_label->bounds_widget.int4_height += int4_dh;
    widget_notification.bounds_widget.int4_height += int4_dh;
  }

  widget_notification.bounds_widget.int4_width =
      int4_box_width - widget_notification.bounds_widget.int4_x;
}

}  // namespace ora::ui
