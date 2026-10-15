#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "gfx/sheet.hpp"
#include "ui/widget.hpp"

namespace ora::sim {
class World;
}

namespace ora::ui {

class TooltipContainerWidget;

class EWMA {
 public:
  explicit EWMA(float fp4_anim_rate) : fp4_anim_rate_(fp4_anim_rate) {}

  float Update(float fp4_new_value);

 private:
  float fp4_anim_rate_;
  std::optional<float> opt_value_;
};

enum class ResourceBarOrientation { Vertical, Horizontal };

class ResourceBarWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ResourceBarWidget";

  ResourceBarWidget();
  explicit ResourceBarWidget(WidgetArgs& args_args);
  ~ResourceBarWidget() override = default;

  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Initialize(WidgetArgs& args_args) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  void MouseEntered() override;
  void MouseExited() override;
  void Draw() override;

  std::string str_tooltip_template;
  std::string str_tooltip_container;
  core::CachedTransform<std::tuple<float, float>, std::string> cache_tooltip_text;
  ResourceBarOrientation kind_orientation = ResourceBarOrientation::Vertical;
  std::string str_indicator_collection = "sidebar-bits";
  std::string str_indicator_image = "indicator";

  std::function<float()> fn_get_provided = [] { return 0.0f; };
  std::function<float()> fn_get_used = [] { return 0.0f; };
  std::function<core::Color()> fn_get_bar_color = [] {
    return core::Color::FromArgb(255, 255, 255, 255);
  };

  const gfx::Sprite& IndicatorSprite() const { return sprite_indicator_; }

 protected:
  TooltipContainerWidget* TooltipContainerLazy();

  sim::World* ptr_world_ = nullptr;
  TooltipContainerWidget* ptr_tooltip_container_ = nullptr;
  bool b_tooltip_container_created_ = false;
  EWMA lerp_provided_{0.3f};
  EWMA lerp_used_{0.3f};
  gfx::Sprite sprite_indicator_;
};

}  // namespace ora::ui
