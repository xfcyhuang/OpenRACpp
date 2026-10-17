// UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowersWidget.cs @e180e3225
//          L24-297 的实现面
//          The implementation face of L24-297.
#include "mods/support_powers_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/field_loader.hpp"
#include "mods/render_sprites.hpp"
#include "net/text_notifications.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

core::Vector2 Int2ToVector2(int2 v) {
  return core::Vector2{static_cast<float>(v.X), static_cast<float>(v.Y)};
}

}  // namespace

SupportPowersWidget::SupportPowersWidget(ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr_arg;

  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };

  if (ptr_world_ != nullptr && ptr_world_->LocalPlayer() != nullptr)
    ptr_spm_ = ptr_world_->LocalPlayer()
                   ->PlayerActor()
                   ->TraitOrDefault<SupportPowerManager>();
}

SupportPowersWidget::SupportPowersWidget(
    const SupportPowersWidget& widget_other)
    : ora::ui::Widget(widget_other),
      str_ready_text(widget_other.str_ready_text),
      str_hold_text(widget_other.str_hold_text),
      str_overlay_font(widget_other.str_overlay_font),
      int2_icon_size(widget_other.int2_icon_size),
      int4_icon_margin(widget_other.int4_icon_margin),
      int2_icon_sprite_offset(widget_other.int2_icon_sprite_offset),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_hotkey_prefix(widget_other.str_hotkey_prefix),
      int4_hotkey_count(widget_other.int4_hotkey_count),
      str_clock_animation(widget_other.str_clock_animation),
      str_clock_sequence(widget_other.str_clock_sequence),
      str_clock_palette(widget_other.str_clock_palette),
      b_horizontal(widget_other.b_horizontal),
      fn_on_icon_count_changed(widget_other.fn_on_icon_count_changed),
      ptr_world_(widget_other.ptr_world_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      ptr_spm_(widget_other.ptr_spm_) {
  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };
}

bool SupportPowersWidget::LoadFieldOrProperty(std::string_view str_key,
                                              std::string_view str_value) {
  if (str_key == "ReadyText") {
    str_ready_text = std::string{str_value};
    return true;
  }
  if (str_key == "HoldText") {
    str_hold_text = std::string{str_value};
    return true;
  }
  if (str_key == "OverlayFont") {
    str_overlay_font = std::string{str_value};
    return true;
  }
  if (str_key == "IconSize") {
    int2_icon_size = meta::GetSizeValue("IconSize", str_value);
    return true;
  }
  if (str_key == "IconMargin") {
    int4_icon_margin = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "IconSpriteOffset") {
    int2_icon_sprite_offset =
        meta::GetSizeValue("IconSpriteOffset", str_value);
    return true;
  }
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }
  if (str_key == "HotkeyPrefix") {
    str_hotkey_prefix = std::string{str_value};
    return true;
  }
  if (str_key == "HotkeyCount") {
    int4_hotkey_count = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "ClockAnimation") {
    str_clock_animation = std::string{str_value};
    return true;
  }
  if (str_key == "ClockSequence") {
    str_clock_sequence = std::string{str_value};
    return true;
  }
  if (str_key == "ClockPalette") {
    str_clock_palette = std::string{str_value};
    return true;
  }
  if (str_key == "Horizontal") {
    b_horizontal = str_value == "True";
    return true;
  }
  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void SupportPowersWidget::Initialize(ora::ui::WidgetArgs& args_args) {
  ora::ui::Widget::Initialize(args_args);

  vec_hotkeys_.clear();
  for (int i = 0; i < int4_hotkey_count; i++) {
    const std::string str_name =
        str_hotkey_prefix + std::format("{:02}", i + 1);
    vec_hotkeys_.push_back(ui::Ui::Instance().HotkeyLookup(str_name));
  }

  ptr_overlay_font_ = ui::Ui::Instance().TryGetFontDeps(str_overlay_font);

  vec_icon_offset_ =
      0.5f * Int2ToVector2(int2_icon_size) +
      Int2ToVector2(int2_icon_sprite_offset);

  str_hold_text = fluent::FluentProvider::Instance().GetMessage(str_hold_text);
  if (ptr_overlay_font_ != nullptr)
    vec_hold_offset_ =
        vec_icon_offset_ -
        0.5f * Int2ToVector2(ptr_overlay_font_->Measure(str_hold_text));
  str_ready_text =
      fluent::FluentProvider::Instance().GetMessage(str_ready_text);
  if (ptr_overlay_font_ != nullptr)
    vec_ready_offset_ =
        vec_icon_offset_ -
        0.5f * Int2ToVector2(ptr_overlay_font_->Measure(str_ready_text));

  if (ptr_world_ != nullptr)
    ptr_clock_ = std::make_unique<gfx::Animation>(
        RenderSprites::MakeAnimationDeps(*ptr_world_), str_clock_animation);
}

void SupportPowersWidget::Tick() {
  vec_icons_.clear();
  if (ptr_spm_ == nullptr)
    return;

  std::vector<SupportPowerInstance*> vec_powers;
  for (const auto& [str_key, power] : ptr_spm_->Powers())
    if (!power->Disabled())
      vec_powers.push_back(power.get());
  std::stable_sort(vec_powers.begin(), vec_powers.end(),
                   [](const SupportPowerInstance* a,
                      const SupportPowerInstance* b) {
                     return a->Info()->int4_support_power_palette_order <
                            b->Info()->int4_support_power_palette_order;
                   });

  const int int4_old_icon_count = int4_icon_count_;
  int4_icon_count_ = 0;

  const Rectangle rect_rb = RenderBounds();
  for (SupportPowerInstance* p : vec_powers) {
    Rectangle rect;
    if (b_horizontal)
      rect = Rectangle{
          rect_rb.X + int4_icon_count_ * (int2_icon_size.X + int4_icon_margin),
          rect_rb.Y, int2_icon_size.X, int2_icon_size.Y};
    else
      rect = Rectangle{
          rect_rb.X,
          rect_rb.Y + int4_icon_count_ * (int2_icon_size.Y + int4_icon_margin),
          int2_icon_size.X, int2_icon_size.Y};

    gfx::Animation icon(RenderSprites::MakeAnimationDeps(*ptr_world_),
                        p->Info()->str_icon_image);
    icon.Play(p->Info()->str_icon);

    SupportPowerIcon power_icon;
    power_icon.ptr_power = p;
    power_icon.pos = core::Vector2{static_cast<float>(rect.Left()), static_cast<float>(rect.Top())};
    power_icon.sprite = icon.Image();
    power_icon.ptr_palette =
        ptr_world_renderer_->Palette(p->Info()->str_icon_palette);
    power_icon.ptr_icon_clock_palette =
        ptr_world_renderer_->Palette(str_clock_palette);
    if (int4_icon_count_ < int4_hotkey_count) {
      power_icon.hotkey = vec_hotkeys_[int4_icon_count_];
      power_icon.b_has_hotkey = true;
    }

    vec_icons_.emplace_back(rect, std::move(power_icon));
    int4_icon_count_++;
  }

  rect_event_bounds_ = Rectangle::Empty();
  for (const auto& [rect, icon] : vec_icons_) {
    if (rect_event_bounds_ == Rectangle::Empty())
      rect_event_bounds_ = rect;
    else
      rect_event_bounds_ =
          Rectangle::Union(rect_event_bounds_, rect);
  }

  if (int4_old_icon_count != int4_icon_count_)
    fn_on_icon_count_changed(int4_old_icon_count, int4_icon_count_);
}

void SupportPowersWidget::ClickIcon(SupportPowerIcon& clicked) {
  if (!clicked.ptr_power->Active()) {
    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_spm_->Self()->Owner(),
        clicked.ptr_power->Info()
            ->str_insufficient_power_text_notification);
  } else {
    clicked.ptr_power->Target();
  }
}

bool SupportPowersWidget::HandleKeyPress(const ora::KeyInput& e_input) {
  if (e_input.Event == KeyInputEvent::Down) {
    for (auto& [rect, icon] : vec_icons_)
      if (icon.b_has_hotkey && icon.hotkey.IsActivatedBy(e_input)) {
        ClickIcon(icon);
        return true;
      }
  }

  return false;
}

void SupportPowersWidget::Draw() {
  if (ptr_overlay_font_ != nullptr)
    vec_time_offset_ =
        vec_icon_offset_ -
        0.5f * Int2ToVector2(ptr_overlay_font_->Measure(
                    ui::FormatTime(0, ptr_world_->Timestep())));

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps()) {
    ptr_renderer->EnableAntialiasingFilter();
    for (const auto& [rect, p] : vec_icons_) {
      ui::DrawSpriteCentered(p.sprite, p.ptr_palette,
                                          p.pos + vec_icon_offset_);

      SupportPowerInstance* sp = p.ptr_power;
      if (ptr_clock_ != nullptr) {
        ptr_clock_->PlayFetchIndex(
            str_clock_sequence, [sp, this]() -> std::int32_t {
              if (sp->TotalTicks() == 0)
                return ptr_clock_->CurrentSequence()->Length() - 1;
              return (sp->TotalTicks() - sp->RemainingTicks()) *
                     (ptr_clock_->CurrentSequence()->Length() - 1) /
                     sp->TotalTicks();
            });

        ptr_clock_->Tick();
        ui::DrawSpriteCentered(ptr_clock_->Image(),
                                            p.ptr_icon_clock_palette,
                                            p.pos + vec_icon_offset_);
      }
    }
    ptr_renderer->DisableAntialiasingFilter();
  }

  if (ptr_overlay_font_ == nullptr)
    return;

  for (const auto& [rect, p] : vec_icons_) {
    const std::string* str_custom_text =
        p.ptr_power->IconOverlayTextOverride();
    if (str_custom_text != nullptr) {
      const core::Vector2 vec_custom_offset =
          vec_icon_offset_ -
          0.5f * Int2ToVector2(ptr_overlay_font_->Measure(*str_custom_text));
      ptr_overlay_font_->DrawTextWithContrast(
          *str_custom_text, p.pos + vec_custom_offset,
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    } else if (p.ptr_power->Ready()) {
      ptr_overlay_font_->DrawTextWithContrast(
          str_ready_text, p.pos + vec_ready_offset_,
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    } else if (!p.ptr_power->Active()) {
      ptr_overlay_font_->DrawTextWithContrast(
          str_hold_text, p.pos + vec_hold_offset_,
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    } else {
      ptr_overlay_font_->DrawTextWithContrast(
          ui::FormatTime(p.ptr_power->RemainingTicks(),
                                      ptr_world_->Timestep()),
          p.pos + vec_time_offset_,
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    }
  }
}

void SupportPowersWidget::MouseEntered() {
  if (str_tooltip_container.empty() ||
      !ui::Ui::HasInstance() || ui::Ui::Instance().Root() == nullptr)
    return;

  if (!b_tooltip_container_materialized_) {
    b_tooltip_container_materialized_ = true;
    ptr_tooltip_container_widget_ =
        ui::Ui::Instance().Root()->Get<ora::ui::TooltipContainerWidget>(
            str_tooltip_container);
  }

  ora::ui::WidgetArgs args_tooltip;
  args_tooltip.Add("world", static_cast<sim::World*>(ptr_world_));
  args_tooltip.Add("player", ptr_spm_->Self()->Owner());
  ptr_tooltip_container_widget_->SetTooltip(str_tooltip_template,
                                            std::move(args_tooltip));
}

void SupportPowersWidget::MouseExited() {
  if (str_tooltip_container.empty() ||
      !b_tooltip_container_materialized_)
    return;

  ptr_tooltip_container_widget_->RemoveTooltip();
}

bool SupportPowersWidget::HandleMouseInput(const ora::MouseInput& mi_input) {
  if (mi_input.Event == MouseInputEvent::Move) {
    ptr_tooltip_icon_ = nullptr;
    for (auto& [rect, icon] : vec_icons_)
      if (rect.Contains(mi_input.Location)) {
        ptr_tooltip_icon_ = &icon;
        break;
      }

    return false;
  }

  if (mi_input.Event != MouseInputEvent::Down)
    return false;

  for (auto& [rect, icon] : vec_icons_)
    if (rect.Contains(mi_input.Location)) {
      ClickIcon(icon);
      break;
    }

  return true;
}

void RegisterSupportPowersWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "SupportPowersWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<SupportPowersWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
