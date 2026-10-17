// UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowerTimerWidget.cs @e180e3225
//          L23-97 的实现面
//          The implementation face of L23-97.
#include "mods/support_power_timer_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/gfx_util.hpp"
#include "mods/actor_exts.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

constexpr std::string_view kFormat = "support-power-timer";

core::Vector2 Int2ToVector2(int2 v) {
  return core::Vector2{static_cast<float>(v.X), static_cast<float>(v.Y)};
}

}  // namespace

SupportPowerTimerWidget::SupportPowerTimerWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;

  if (ptr_world_ != nullptr)
    for (const auto& [actor, trait] :
         ptr_world_->ActorsWithTrait<SupportPowerManager>())
      if (!actor->IsDead() && !actor->Owner()->NonCombatant())
        vec_managers_.push_back(trait);

  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr) {
    color_bg_dark_ = ptr_metrics->GetColor("TextContrastColorDark");
    color_bg_light_ = ptr_metrics->GetColor("TextContrastColorLight");
  }
  ptr_font_ = ui::Ui::HasInstance()
                  ? ui::Ui::Instance().TryGetFontDeps(str_font)
                  : nullptr;
}

SupportPowerTimerWidget::SupportPowerTimerWidget(
    const SupportPowerTimerWidget& widget_other)
    : ora::ui::Widget(widget_other),
      str_font(widget_other.str_font),
      kind_align(widget_other.kind_align),
      kind_order(widget_other.kind_order),
      ptr_world_(widget_other.ptr_world_),
      vec_managers_(widget_other.vec_managers_),
      color_bg_dark_(widget_other.color_bg_dark_),
      color_bg_light_(widget_other.color_bg_light_) {
  ptr_font_ = ui::Ui::HasInstance()
                  ? ui::Ui::Instance().TryGetFontDeps(str_font)
                  : nullptr;
}

bool SupportPowerTimerWidget::LoadFieldOrProperty(
    std::string_view str_key, std::string_view str_value) {
  if (str_key == "Font") {
    str_font = std::string{str_value};
    return true;
  }
  if (str_key == "Align") {
    if (str_value == "Center")
      kind_align = TextAlign::Center;
    else if (str_value == "Right")
      kind_align = TextAlign::Right;
    else
      kind_align = TextAlign::Left;
    return true;
  }
  if (str_key == "Order") {
    kind_order = str_value == "Ascending" ? TimerOrder::Ascending
                                          : TimerOrder::Descending;
    return true;
  }
  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void SupportPowerTimerWidget::Tick() {
  vec_texts_.clear();

  for (SupportPowerManager* manager : vec_managers_) {
    for (const auto& [str_key, power] : manager->Powers()) {
      if (power->Instances().empty() ||
          power->Info() == nullptr ||
          power->Info()->kind_display_timer_relationships ==
              sim::PlayerRelationship::None ||
          power->Disabled())
        continue;

      sim::Actor* self = power->Instances().front()->Self();
      const sim::Player* ptr_viewer =
          self->Owner()->GetWorld().RenderPlayer() != nullptr
              ? self->Owner()->GetWorld().RenderPlayer()
              : self->Owner()->GetWorld().LocalPlayer();
      if (ptr_viewer != nullptr &&
          !sim::HasRelationship(
              power->Info()->kind_display_timer_relationships,
              self->Owner()->RelationshipWith(ptr_viewer)))
        continue;

      const std::string str_time = ui::FormatTime(
          power->RemainingTicks(), false, self->world().Timestep());
      std::vector<std::pair<std::string, fluent::FluentValue>>
          vec_fluent_args;
      vec_fluent_args.emplace_back(
          std::string{"player"},
          fluent::FluentValue{self->Owner()->ResolvedPlayerName()});
      vec_fluent_args.emplace_back(std::string{"support-power"},
                                   fluent::FluentValue{power->Name()});
      vec_fluent_args.emplace_back(std::string{"time"},
                                   fluent::FluentValue{str_time});
      const std::string str_text =
          fluent::FluentProvider::Instance().GetMessage(kFormat,
                                                        vec_fluent_args);

      const core::Color color_text =
          !power->Ready() || ui::Ui::Instance().LocalTick() % 50 < 25
              ? mods::OwnerColor(*self)
              : core::Color::FromArgb(255, 255, 255, 255);

      vec_texts_.emplace_back(str_text, color_text);
    }
  }
}

void SupportPowerTimerWidget::Draw() {
  if (!fn_is_visible() || ptr_font_ == nullptr)
    return;

  int y = 0;
  for (const auto& [str_text, color_text] : vec_texts_) {
    const int2 int2_text_size = ptr_font_->Measure(str_text);
    core::Vector2 vec_location{
        static_cast<float>(bounds_widget.int4_x),
        static_cast<float>(bounds_widget.int4_y + y)};

    if (kind_align == TextAlign::Center)
      vec_location =
          vec_location +
          core::Vector2{
              (bounds_widget.int4_width - int2_text_size.X) / 2.0f, 0};

    if (kind_align == TextAlign::Right)
      vec_location =
          vec_location +
          core::Vector2{
              static_cast<float>(bounds_widget.int4_width -
                                 int2_text_size.X),
              0};

    ptr_font_->DrawTextWithShadow(str_text, vec_location, color_text,
                                  color_bg_dark_, color_bg_light_, 1);
    y += (ptr_font_->Measure(str_text).Y + 5) *
         static_cast<int>(kind_order);
  }
}

void RegisterSupportPowerTimerWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "SupportPowerTimerWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<SupportPowerTimerWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
