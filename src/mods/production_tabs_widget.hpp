#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_font.hpp"
#include "mods/production.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"

namespace ora::mods {

class ProductionPaletteWidget;

struct ProductionTab {
  std::string name;
  ProductionQueue* queue = nullptr;
};

class ProductionTabGroup {
 public:
  std::vector<ProductionTab> vec_tabs;
  std::string group;
  int int4_next_queue_name = 1;

  bool Alert() const;

  void Update(std::span<ProductionQueue* const> vec_all_queues);
};

class ProductionTabsWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "ProductionTabsWidget";

  explicit ProductionTabsWidget(ora::ui::WidgetArgs& args_args);
  ProductionTabsWidget(const ProductionTabsWidget& widget_other);
  ~ProductionTabsWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new ProductionTabsWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
  void Initialize(ora::ui::WidgetArgs& args_args) override;

  bool SelectNextTab(bool reverse);

  void PickUpCompletedBuilding();

  void Tick() override;
  void Draw() override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;
  bool YieldMouseFocus(const ora::MouseInput& mi_input) override;

  std::string str_queue_group;
  bool b_queue_group_set_ = false;

  std::string str_palette_widget;
  std::string str_types_container;
  std::string str_background_container;

  int int4_tab_width = 30;
  int int4_arrow_width = 20;

  std::string str_click_sound;
  std::string str_click_disabled_sound;

  ui::HotkeyReference hk_previous_production_tab_key;
  ui::HotkeyReference hk_next_production_tab_key;

  std::string str_arrow_button{"button"};
  std::string str_tab_button{"button"};

  std::string str_background{"panel-black"};
  std::string str_decorations{"scrollpanel-decorations"};
  std::string str_decoration_scroll_left{"left"};
  std::string str_decoration_scroll_right{"right"};

  core::Color color_tab{core::Color::FromArgb(255, 255, 255, 255)};
  core::Color color_tab_done{core::Color::FromArgb(255, 255, 215, 0)};

  const std::string& QueueGroup() const { return str_queue_group; }
  bool QueueGroupSet() const { return b_queue_group_set_; }
  void SetQueueGroup(std::string str_group);

  ProductionQueue* CurrentQueue() const;
  void SetCurrentQueue(ProductionQueue* ptr_queue);

  void ActorChanged(sim::Actor& a);

  const std::map<std::string, ProductionTabGroup>& Groups() const {
    return map_groups_;
  }
  std::map<std::string, ProductionTabGroup>& GroupsMutable() {
    return map_groups_;
  }

  const std::map<std::string, ProductionTabGroup>& GroupsForTest() const {
    return map_groups_;
  }
  const std::vector<ProductionTab>& TabsForTest() const;
  float ListOffsetForTest() const { return fp4_list_offset; }

 private:

  void Scroll(int int4_amount);

  void MaterializePaletteWidget() const;

  sim::World* ptr_world = nullptr;
  std::map<std::string, ProductionTabGroup> map_groups_;
  mutable ProductionPaletteWidget* ptr_palette_widget_ = nullptr;
  mutable bool b_palette_materialized_ = false;

  ui::StatefulImageCache cache_left_arrow_;
  ui::StatefulImageCache cache_right_arrow_;

  int int4_content_width = 0;
  float fp4_list_offset = 0.0f;
  bool b_left_pressed = false;
  bool b_right_pressed = false;
  gfx::SpriteFont* ptr_font_ = nullptr;
  Rectangle rect_left_button_;
  Rectangle rect_right_button_;
  std::vector<std::pair<ProductionQueue*, bool>>
      vec_cached_production_queue_enabled_states_;
  mutable std::string str_cursor_cache_;
};

void RegisterProductionTabsWidget();

}
