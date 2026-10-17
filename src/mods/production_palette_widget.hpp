// UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionPaletteWidget.cs @e180e3225
//          L28-650 全文
//          The whole of ProductionPaletteWidget.cs L28-650.
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/animation.hpp"
#include "gfx/sprite_font.hpp"
#include "mods/production.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::ui {
class TooltipContainerWidget;
}

namespace ora::net {
class OrderManager;
}

namespace ora::mods {

/// ProductionIcon(L28-40)
struct ProductionIcon {
  const game::ActorInfo* actor = nullptr;
  std::string name;
  ui::HotkeyReference hotkey;
  gfx::Sprite sprite;
  gfx::PaletteReference* palette = nullptr;
  gfx::PaletteReference* icon_clock_palette = nullptr;
  gfx::PaletteReference* icon_darken_palette = nullptr;
  core::Vector2 pos;
  std::vector<ProductionItemLike*> vec_queued;
  ProductionQueue* production_queue = nullptr;
};

/// ReadyTextStyleOptions(L44)
enum class ReadyTextStyleOptions : std::uint8_t {
  Solid,
  AlternatingColor,
  Blinking,
};

/// TextAlign(上游 Widget 域同名枚举)
/// TextAlign (upstream's same-named widget-domain enum).
enum class TextAlign : std::uint8_t { Left, Center, Right };

class ProductionPaletteWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ProductionPaletteWidget";

  ProductionPaletteWidget(ora::ui::WidgetArgs& args_args);
  ProductionPaletteWidget(const ProductionPaletteWidget& widget_other);
  ~ProductionPaletteWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ProductionPaletteWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
  void Initialize(ora::ui::WidgetArgs& args_args) override;

  void Tick() override;
  void MouseEntered() override;
  void MouseExited() override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;
  void Draw() override;
  Rectangle EventBounds() const override { return rect_event_bounds_; }
  const std::string* GetCursor(int2 pt_pos) const override;

  // ———— 公有字段面(readonly 字段 + 属性)————
  ReadyTextStyleOptions kind_ready_text_style =
      ReadyTextStyleOptions::AlternatingColor;
  core::Color color_text{core::Color::FromArgb(255, 255, 255, 255)};
  core::Color color_ready_text_alt{core::Color::FromArgb(255, 255, 215, 0)};
  int int4_columns = 3;
  int2 int2_icon_size{64, 48};
  int2 int2_icon_margin{0, 0};
  int2 int2_icon_sprite_offset{0, 0};

  core::Vector2 vec_queued_offset{4.0f, 2.0f};
  core::Vector2 vec_bulk_offset{4.0f, 2.0f};
  TextAlign kind_queued_text_align = TextAlign::Left;

  std::string str_click_sound;
  std::string str_click_disabled_sound;
  std::string str_tooltip_container;
  std::string str_tooltip_template{"PRODUCTION_TOOLTIP"};

  std::string str_hotkey_prefix;
  int int4_hotkey_count = 0;
  ui::HotkeyReference hk_select_production_building;

  std::string str_clock_animation{"clock"};
  std::string str_clock_sequence{"idle"};
  std::string str_clock_palette{"chrome"};

  std::string str_not_buildable_animation{"clock"};
  std::string str_not_buildable_sequence{"idle"};
  std::string str_not_buildable_palette{"chrome"};

  std::string str_overlay_font{"TinyBold"};
  std::string str_symbols_font{"Symbols"};

  bool b_draw_time = true;

  std::string str_ready_text;
  std::string str_hold_text;
  std::string str_infinite_symbol{"\u221E"};

  int int4_displayed_icon_count = 0;
  int int4_total_icon_count = 0;
  std::function<void(int, int)> fn_on_icon_count_changed = [](int, int) {};

  sim::World* ptr_world = nullptr;
  gfx::WorldRenderer* ptr_world_renderer = nullptr;
  net::OrderManager* ptr_order_manager = nullptr;

  int int4_minimum_rows = 4;
  int int4_maximum_rows = std::numeric_limits<int>::max();

  int int4_icon_row_offset = 0;
  int int4_max_icon_row_offset = std::numeric_limits<int>::max();

  ProductionIcon* ptr_tooltip_icon = nullptr;
  std::function<ProductionIcon*()> fn_get_tooltip_icon;

  ProductionQueue* ptr_current_queue = nullptr;
  void SetCurrentQueue(ProductionQueue* ptr_queue);

  void ScrollDown();
  bool CanScrollDown() const;
  void ScrollUp();
  bool CanScrollUp() const { return int4_icon_row_offset > 0; }
  void ScrollToTop() { int4_icon_row_offset = 0; }

  std::vector<const game::ActorInfo*> AllBuildables() const;

  void PickUpCompletedBuilding();

  const std::vector<std::pair<Rectangle, ProductionIcon>>& IconsForTest()
      const {
    return vec_icons_;
  }
  const gfx::Animation* ClockAnimationForTest() const {
    return ptr_clock_ != nullptr ? ptr_clock_.get() : nullptr;
  }

 private:
  /// L298-312 | L298-312.
  bool PickUpCompletedBuildingIcon(ProductionItemLike* item);

  /// L319-365 | L319-365.
  bool HandleLeftClick(ProductionItemLike* item, ProductionIcon& icon,
                       int int4_handle_count, Modifiers modifiers);

  /// L367-410 | L367-410.
  bool HandleRightClick(ProductionItemLike* item, ProductionIcon& icon,
                        int int4_handle_count);

  /// L412-426 | L412-426.
  bool HandleMiddleClick(ProductionItemLike* item, ProductionIcon& icon,
                         int int4_handle_count);

  /// L428-443 | L428-443.
  bool HandleEvent(ProductionIcon& icon, MouseButton btn,
                   Modifiers modifiers);

  /// L461-481 | L461-481.
  bool SelectProductionBuilding();

  /// L483-487 | L483-487.
  void UpdateCachedProductionIconOverlays();

  /// L489-545 | L489-545.
  void RefreshIcons();

  void MaterializeTooltipContainer();

  std::vector<ui::HotkeyReference> vec_hotkeys_;
  ora::ui::TooltipContainerWidget* ptr_tooltip_container_widget_ = nullptr;
  bool b_tooltip_container_materialized_ = false;
  std::vector<std::pair<Rectangle, ProductionIcon>> vec_icons_;
  std::unique_ptr<gfx::Animation> ptr_clock_;
  std::unique_ptr<gfx::Animation> ptr_cant_build_;
  Rectangle rect_event_bounds_ = Rectangle::Empty();

  gfx::SpriteFont* ptr_overlay_font_ = nullptr;
  gfx::SpriteFont* ptr_symbol_font_ = nullptr;
  core::Vector2 vec_icon_offset_;
  core::Vector2 vec_hold_offset_;
  core::Vector2 vec_ready_offset_;
  core::Vector2 vec_time_offset_;
  core::Vector2 vec_infinite_offset_;
  core::Vector2 vec_queued_offset_config_;

  sim::Player* ptr_cached_queue_owner_ = nullptr;
  std::vector<sim::IProductionIconOverlay*> vec_pios_;
};

void RegisterProductionPaletteWidget();

}  // namespace ora::mods
