// UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowerTimerWidget.cs @1f594e320
//          L23-97 全文(观战的支持 power 充能倒计时列)
//          The whole of SupportPowerTimerWidget.cs L23-97 (the observer's
//          support-power charge countdown list).
#pragma once
import std;

#include "core/color.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite_font.hpp"
#include "mods/support_powers.hpp"
#include "mods/production_palette_widget.hpp"
#include "ui/widget.hpp"

namespace ora::mods {

/// TimerOrder(L96)
enum class TimerOrder : std::int8_t { Ascending = -1, Descending = 1 };

class SupportPowerTimerWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "SupportPowerTimerWidget";

  explicit SupportPowerTimerWidget(ora::ui::WidgetArgs& args_args);
  SupportPowerTimerWidget(const SupportPowerTimerWidget& widget_other);
  ~SupportPowerTimerWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new SupportPowerTimerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  void Tick() override;
  void Draw() override;

  std::string str_font{"Bold"};
  TextAlign kind_align = TextAlign::Left;
  TimerOrder kind_order = TimerOrder::Descending;

  const std::vector<std::pair<std::string, core::Color>>& TextsForTest()
      const {
    return vec_texts_;
  }

 private:
  sim::World* ptr_world_ = nullptr;
  std::vector<SupportPowerManager*> vec_managers_;
  gfx::SpriteFont* ptr_font_ = nullptr;
  core::Color color_bg_dark_;
  core::Color color_bg_light_;
  std::vector<std::pair<std::string, core::Color>> vec_texts_;
};

void RegisterSupportPowerTimerWidget();

}  // namespace ora::mods
