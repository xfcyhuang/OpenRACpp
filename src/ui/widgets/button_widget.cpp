// UPSTREAM: OpenRA.Mods.Common/Widgets/ButtonWidget.cs @e180e3225(button_widget.hpp
// 的实现)
// The implementation of button_widget.hpp.
import std;

#include "ui/widgets/button_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {
namespace {

gfx::SpriteFont& RequireFont(const std::string& str_font) {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  gfx::SpriteFont* ptr_font =
      ptr_renderer != nullptr ? ptr_renderer->TryGetFont(str_font) : nullptr;
  if (ptr_font == nullptr)
    throw std::runtime_error("Requested font '" + str_font + "' was not found.");
  return *ptr_font;
}

}  // namespace

ButtonWidget::ButtonWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod = std::get_if<game::ModData*>(ptr_value))
      mod_rules = &(*ptr_mod)->DefaultRules();

  ChromeMetrics& metrics = RequireChromeMetrics();
  int4_visual_height = metrics.GetInt32("ButtonDepth");
  str_font = metrics.Get("ButtonFont");
  color_text = metrics.GetColor("ButtonTextColor");
  color_text_disabled = metrics.GetColor("ButtonTextColorDisabled");
  b_contrast = metrics.GetBool("ButtonTextContrast");
  b_shadow = metrics.GetBool("ButtonTextShadow");
  color_contrast_dark = metrics.GetColor("ButtonTextContrastColorDark");
  color_contrast_light = metrics.GetColor("ButtonTextContrastColorLight");
  int4_contrast_radius = metrics.GetInt32("ButtonTextContrastRadius");
  str_click_sound = metrics.Get("ClickSound");
  str_click_disabled_sound = metrics.Get("ClickDisabledSound");
  str_cursor = metrics.Get("ButtonCursor");

  BindDefaults();
}

ButtonWidget::ButtonWidget(const ButtonWidget& widget_other)
    : InputWidget(widget_other),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_text(widget_other.str_text),
      kind_align(widget_other.kind_align),
      int4_left_margin(widget_other.int4_left_margin),
      int4_right_margin(widget_other.int4_right_margin),
      str_font(widget_other.str_font),
      color_text(widget_other.color_text),
      color_text_disabled(widget_other.color_text_disabled),
      b_contrast(widget_other.b_contrast),
      b_shadow(widget_other.b_shadow),
      str_background(widget_other.str_background),
      b_depressed(widget_other.b_depressed),
      int4_visual_height(widget_other.int4_visual_height),
      color_contrast_dark(widget_other.color_contrast_dark),
      color_contrast_light(widget_other.color_contrast_light),
      int4_contrast_radius(widget_other.int4_contrast_radius),
      fn_on_mouse_down(widget_other.fn_on_mouse_down),
      opt_tooltip_text(widget_other.opt_tooltip_text),
      opt_tooltip_desc(widget_other.opt_tooltip_desc),
      fn_on_click(widget_other.fn_on_click),
      fn_on_double_click(widget_other.fn_on_double_click),
      fn_on_key_press(widget_other.fn_on_key_press),
      mod_rules(widget_other.mod_rules) {
  // 上游 clone 拷贝 GetText 委托(闭包绑源体,源体存活期由树持有);C++ 侧
  // 以自有缓存重建标准委托族 —— 模板面语义等价(缓存透明),偏离登记
  // upstream's clone copies the GetText delegates (closures bound to the
  // source, kept alive by the tree); the C++ side rebuilds the standard
  // delegate family over its own caches — equivalent on the template face
  // (the cache is transparent); registered.
  BindDefaults();
}

bool ButtonWidget::YieldMouseFocus(const MouseInput& mi_input) {
  b_depressed = false;
  return InputWidget::YieldMouseFocus(mi_input);
}

bool ButtonWidget::HandleKeyPress(const KeyInput& e_input) {
  if (!key_hotkey.IsActivatedBy(e_input) || e_input.Event != KeyInputEvent::Down ||
      (b_disable_key_repeat && e_input.IsRepeat))
    return false;

  if (!fn_is_disabled()) {
    fn_on_key_press(e_input);
    if (!b_disable_key_sound)
      Ui::Instance().PlayNotification(mod_rules, str_click_sound);
  } else if (!b_disable_key_sound)
    Ui::Instance().PlayNotification(mod_rules, str_click_disabled_sound);

  return true;
}

bool ButtonWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Button != MouseButton::Left)
    return false;

  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return false;

  const bool b_disabled = fn_is_disabled();
  if (HasMouseFocus() && mi_input.Event == MouseInputEvent::Up && mi_input.MultiTapCount == 2 &&
      fn_on_double_click) {
    if (!b_disabled) {
      fn_on_double_click();
      return YieldMouseFocus(mi_input);
    }
  } else if (HasMouseFocus() && mi_input.Event == MouseInputEvent::Up) {
    // Only fire the onMouseUp event if we successfully lost focus, and were
    // pressed
    if (b_depressed && !b_disabled)
      fn_on_mouse_up(mi_input);

    return YieldMouseFocus(mi_input);
  }

  if (mi_input.Event == MouseInputEvent::Down) {
    // OnMouseDown returns false if the button shouldn't be pressed
    if (!b_disabled) {
      fn_on_mouse_down(mi_input);
      b_depressed = true;
      Ui::Instance().PlayNotification(mod_rules, str_click_sound);
    } else {
      YieldMouseFocus(mi_input);
      Ui::Instance().PlayNotification(mod_rules, str_click_disabled_sound);
    }
  } else if (mi_input.Event == MouseInputEvent::Move && HasMouseFocus())
    b_depressed = RenderBounds().Contains(mi_input.Location);

  return b_depressed;
}

void ButtonWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  if (fn_get_tooltip_text) {
    TooltipContainerWidget* ptr_container = TooltipContainerOrNull();
    if (ptr_container == nullptr)
      return;

    WidgetArgs args_tooltip;
    args_tooltip.Add("button", static_cast<Widget*>(this));
    args_tooltip.Add("getText", fn_get_tooltip_text);
    args_tooltip.Add("getDesc", fn_get_tooltip_desc);
    ptr_container->SetTooltip(str_tooltip_template, std::move(args_tooltip));
  }
}

void ButtonWidget::MouseExited() {
  if (str_tooltip_container.empty() || !b_tooltip_resolved_)
    return;

  if (TooltipContainerWidget* ptr_container = TooltipContainerOrNull())
    ptr_container->RemoveTooltip();
}

int2 ButtonWidget::ChildOrigin() const {
  return RenderOrigin() +
         (b_depressed ? int2{int4_visual_height, int4_visual_height} : int2{0, 0});
}

void ButtonWidget::Draw() {
  const Rectangle rect_rb = RenderBounds();
  const bool b_disabled = fn_is_disabled();
  const bool b_highlighted = fn_is_highlighted();
  gfx::SpriteFont& font_text = RequireFont(str_font);
  const std::string str_text_drawn = fn_get_text();
  const core::Color color_drawn = fn_get_color();
  const core::Color color_drawn_disabled = fn_get_color_disabled();
  const core::Color color_bg_dark = fn_get_contrast_color_dark();
  const core::Color color_bg_light = fn_get_contrast_color_light();

  const int2 int2_state_offset =
      b_depressed ? int2{int4_visual_height, int4_visual_height} : int2{0, 0};

  const int2 int2_position = GetTextPosition(str_text_drawn, font_text, rect_rb);

  const bool b_hover = Ui::Instance().MouseOverWidget() == this ||
                       [this]() {
                         for (Widget* ptr_child : Children())
                           if (ptr_child == Ui::Instance().MouseOverWidget())
                             return true;
                         return false;
                       }();
  DrawBackground(rect_rb, b_disabled, b_depressed, b_hover, b_highlighted);
  if (b_contrast)
    font_text.DrawTextWithContrast(
        str_text_drawn,
        core::Vector2{static_cast<float>(int2_position.X + int2_state_offset.X),
                      static_cast<float>(int2_position.Y + int2_state_offset.Y)},
        b_disabled ? color_drawn_disabled : color_drawn, color_bg_dark, color_bg_light,
        int4_contrast_radius);
  else if (b_shadow)
    font_text.DrawTextWithShadow(
        str_text_drawn,
        core::Vector2{static_cast<float>(int2_position.X), static_cast<float>(int2_position.Y)},
        color_drawn, color_bg_dark, color_bg_light, 1);
  else
    font_text.DrawText(
        str_text_drawn,
        core::Vector2{static_cast<float>(int2_position.X + int2_state_offset.X),
                      static_cast<float>(int2_position.Y + int2_state_offset.Y)},
        b_disabled ? color_drawn_disabled : color_drawn);
}

bool ButtonWidget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }
  if (str_key == "Key") {
    key_hotkey = Ui::Instance().Hotkey(str_value);
    return true;
  }
  if (str_key == "DisableKeyRepeat") {
    b_disable_key_repeat = meta::GetBoolValue("DisableKeyRepeat", str_value);
    return true;
  }
  if (str_key == "DisableKeySound") {
    b_disable_key_sound = meta::GetBoolValue("DisableKeySound", str_value);
    return true;
  }
  if (str_key == "Text") {
    str_text = std::string{str_value};
    return true;
  }
  if (str_key == "Align") {
    const std::string str_value_owned{str_value};
    if (str_value_owned == "Left")
      kind_align = TextAlign::Left;
    else if (str_value_owned == "Center")
      kind_align = TextAlign::Center;
    else if (str_value_owned == "Right")
      kind_align = TextAlign::Right;
    else
      throw std::runtime_error(
          "FieldLoader: Cannot parse `" + str_value_owned +
          "` into field `Align` of type `OpenRA.Mods.Common.Widgets.TextAlign`");
    return true;
  }
  if (str_key == "LeftMargin") {
    int4_left_margin = meta::GetInt32Value("LeftMargin", str_value);
    return true;
  }
  if (str_key == "RightMargin") {
    int4_right_margin = meta::GetInt32Value("RightMargin", str_value);
    return true;
  }
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "Depressed") {
    b_depressed = meta::GetBoolValue("Depressed", str_value);
    return true;
  }
  if (str_key == "VisualHeight") {
    int4_visual_height = meta::GetInt32Value("VisualHeight", str_value);
    return true;
  }
  if (str_key == "Font") {
    str_font = std::string{str_value};
    return true;
  }
  if (str_key == "TextColor") {
    color_text = meta::GetColorValue("TextColor", str_value);
    return true;
  }
  if (str_key == "TextColorDisabled") {
    color_text_disabled = meta::GetColorValue("TextColorDisabled", str_value);
    return true;
  }
  if (str_key == "Contrast") {
    b_contrast = meta::GetBoolValue("Contrast", str_value);
    return true;
  }
  if (str_key == "Shadow") {
    b_shadow = meta::GetBoolValue("Shadow", str_value);
    return true;
  }
  if (str_key == "ContrastColorDark") {
    color_contrast_dark = meta::GetColorValue("ContrastColorDark", str_value);
    return true;
  }
  if (str_key == "ContrastColorLight") {
    color_contrast_light = meta::GetColorValue("ContrastColorLight", str_value);
    return true;
  }
  if (str_key == "ContrastRadius") {
    int4_contrast_radius = meta::GetInt32Value("ContrastRadius", str_value);
    return true;
  }
  if (str_key == "ClickSound") {
    str_click_sound = std::string{str_value};
    return true;
  }
  if (str_key == "ClickDisabledSound") {
    str_click_disabled_sound = std::string{str_value};
    return true;
  }
  if (str_key == "Highlighted") {
    b_highlighted = meta::GetBoolValue("Highlighted", str_value);
    return true;
  }
  if (str_key == "TooltipText") {
    opt_tooltip_text = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipDesc") {
    opt_tooltip_desc = std::string{str_value};
    return true;
  }
  if (str_key == "Cursor") {
    str_cursor = std::string{str_value};
    return true;
  }

  return InputWidget::LoadFieldOrProperty(str_key, str_value);
}

void ButtonWidget::DrawBackground(Rectangle rect_bounds, bool b_disabled, bool b_pressed,
                                  bool b_hover, bool b_highlighted) {
  DrawBackground(str_background, rect_bounds, b_disabled, b_pressed, b_hover, b_highlighted);
}

void ButtonWidget::DrawBackground(std::string_view str_base_name, Rectangle rect_bounds,
                                  bool b_disabled, bool b_pressed, bool b_hover,
                                  bool b_highlighted) {
  if (str_base_name.empty())
    return;

  const std::string str_variant_name = b_highlighted
                                           ? std::string{str_base_name} + "-highlighted"
                                           : std::string{str_base_name};
  const std::string str_image_name =
      GetStatefulImageName(str_variant_name, b_disabled, b_pressed, b_hover);

  DrawPanel(str_image_name, rect_bounds);
}

int2 ButtonWidget::GetTextPosition(const std::string& str_text, gfx::SpriteFont& font_text,
                                   Rectangle rect_rb) {
  const int2 int2_text_size = font_text.Measure(str_text);
  const int int4_y = rect_rb.Y +
                     (bounds_widget.int4_height - int2_text_size.Y - font_text.TopOffset()) / 2;

  switch (kind_align) {
    case TextAlign::Left:
      return int2{rect_rb.X + int4_left_margin, int4_y};
    case TextAlign::Center:
    default:
      return int2{rect_rb.X + (UsableWidth() - int2_text_size.X) / 2, int4_y};
    case TextAlign::Right:
      return int2{rect_rb.X + UsableWidth() - int2_text_size.X - int4_right_margin, int4_y};
  }
}

TooltipContainerWidget* ButtonWidget::TooltipContainerOrNull() {
  if (b_tooltip_resolved_)
    return ptr_tooltip_container_cached_;

  b_tooltip_resolved_ = true;
  ptr_tooltip_container_cached_ = ResolveTooltipContainer();
  return ptr_tooltip_container_cached_;
}

void ButtonWidget::BindDefaults() {
  const auto fn_fluent = [](const std::string& str_s) {
    return !str_s.empty() ? fluent::FluentProvider::Instance().GetMessage(str_s)
                          : std::string{};
  };
  cache_text_ = std::make_unique<core::CachedTransform<std::string, std::string>>(fn_fluent);
  cache_tooltip_text_ =
      std::make_unique<core::CachedTransform<std::string, std::string>>(fn_fluent);
  cache_tooltip_desc_ =
      std::make_unique<core::CachedTransform<std::string, std::string>>(fn_fluent);

  fn_get_text = [this] { return cache_text_->Update(str_text); };
  fn_get_color = [this] { return color_text; };
  fn_get_color_disabled = [this] { return color_text_disabled; };
  fn_get_contrast_color_dark = [this] { return color_contrast_dark; };
  fn_get_contrast_color_light = [this] { return color_contrast_light; };
  fn_on_mouse_up = [this](const MouseInput&) { fn_on_click(); };
  fn_on_key_press = [this](const KeyInput&) { fn_on_click(); };
  fn_is_highlighted = [this] { return b_highlighted; };
  fn_get_tooltip_text = [this] {
    return cache_tooltip_text_->Update(opt_tooltip_text.value_or(""));
  };
  fn_get_tooltip_desc = [this] {
    return cache_tooltip_desc_->Update(opt_tooltip_desc.value_or(""));
  };
  b_tooltip_resolved_ = false;
  ptr_tooltip_container_cached_ = nullptr;
}

TooltipContainerWidget* ButtonWidget::ResolveTooltipContainer() {
  if (Ui::HasInstance() && Ui::Instance().Root() != nullptr)
    return Ui::Instance().Root()->GetOrNull<TooltipContainerWidget>(str_tooltip_container);
  return nullptr;
}

}  // namespace ora::ui
