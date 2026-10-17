// UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionTabsWidget.cs @e180e3225
//          (实现部分)
//          The implementation half.
import std;

#include "mods/production_tabs_widget.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "meta/field_loader.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::mods {

namespace {

bool RecordDerivesProductionQueueInfo(const meta::RecordObject& rec) {
  const std::string_view str_name = rec.record_desc().str_name;
  if (str_name == "ProductionQueueInfo")
    return true;
  const std::string_view str_base = rec.record_desc().str_base;
  return str_base == "ProductionQueueInfo" ||
         str_base == "ClassicProductionQueueInfo" ||
         str_base == "BulkProductionQueueInfo" ||
         str_base == "ClassicParallelProductionQueueInfo" ||
         str_base == "ParallelProductionQueueInfo";
}

}  // namespace

bool ProductionTabGroup::Alert() const {
  for (const ProductionTab& tab : vec_tabs)
    for (const auto& item : tab.queue->AllQueued())
      if (item->Done())
        return true;
  return false;
}

void ProductionTabGroup::Update(
    std::span<ProductionQueue* const> vec_all_queues) {
  std::vector<ProductionQueue*> vec_queues;
  for (ProductionQueue* q : vec_all_queues)
    if (q->Enabled() && q->Info().str_group == group)
      vec_queues.push_back(q);

  std::vector<ProductionTab> vec_tabs;
  int largest_used_name = 0;

  for (const ProductionTab& tab : this->vec_tabs) {
    bool b_stale = true;
    for (ProductionQueue* q : vec_queues)
      if (q == tab.queue)
        b_stale = false;
    if (b_stale)
      continue;

    vec_tabs.push_back(tab);
    std::erase(vec_queues, tab.queue);
    largest_used_name =
        std::max(std::stoi(tab.name), largest_used_name);
  }

  int4_next_queue_name = largest_used_name + 1;

  for (ProductionQueue* queue : vec_queues) {
    ProductionTab tab;
    tab.name = std::to_string(int4_next_queue_name++);
    tab.queue = queue;
    vec_tabs.push_back(tab);
  }
  this->vec_tabs = std::move(vec_tabs);
}

ProductionTabsWidget::ProductionTabsWidget(ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;

  if (ptr_world != nullptr)
    for (const auto& [name, actor] :
         ptr_world->Map().Rules().Actors())
      for (const meta::RecordObject* rec :
           actor->TraitInfosByInterface(
               "OpenRA.Traits.ITraitInfoInterface")) {
        if (RecordDerivesProductionQueueInfo(*rec)) {
          const ProductionQueueInfoData data =
              ProductionQueueInfoData::Parse(*rec);
          if (map_groups_.find(data.str_group) == map_groups_.end())
            map_groups_[data.str_group] =
                ProductionTabGroup{.group = data.str_group};
        }
      }

  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr) {
    str_click_sound = ptr_metrics->Get("ClickSound");
    str_click_disabled_sound = ptr_metrics->Get("ClickDisabledSound");
  }

  fn_is_visible = [this] {
    return !str_queue_group.empty() &&
           map_groups_.find(str_queue_group) != map_groups_.end() &&
           !map_groups_[str_queue_group].vec_tabs.empty();
  };
}

ProductionTabsWidget::ProductionTabsWidget(
    const ProductionTabsWidget& widget_other)
    : ora::ui::Widget(widget_other),
      str_palette_widget(widget_other.str_palette_widget),
      str_types_container(widget_other.str_types_container),
      str_background_container(widget_other.str_background_container),
      int4_tab_width(widget_other.int4_tab_width),
      int4_arrow_width(widget_other.int4_arrow_width),
      str_click_sound(widget_other.str_click_sound),
      str_click_disabled_sound(widget_other.str_click_disabled_sound),
      hk_previous_production_tab_key(
          widget_other.hk_previous_production_tab_key),
      hk_next_production_tab_key(widget_other.hk_next_production_tab_key),
      str_arrow_button(widget_other.str_arrow_button),
      str_tab_button(widget_other.str_tab_button),
      str_background(widget_other.str_background),
      str_decorations(widget_other.str_decorations),
      str_decoration_scroll_left(widget_other.str_decoration_scroll_left),
      str_decoration_scroll_right(widget_other.str_decoration_scroll_right),
      color_tab(widget_other.color_tab),
      color_tab_done(widget_other.color_tab_done) {
  ptr_world = widget_other.ptr_world;
  map_groups_ = widget_other.map_groups_;

  fn_is_visible = [this] {
    return !str_queue_group.empty() &&
           map_groups_.find(str_queue_group) != map_groups_.end() &&
           !map_groups_[str_queue_group].vec_tabs.empty();
  };
}

bool ProductionTabsWidget::LoadFieldOrProperty(
    std::string_view str_key, std::string_view str_value) {
  const auto load_hotkey = [&](ui::HotkeyReference& hk_target) {
    hk_target = ui::Ui::Instance().HotkeyLookup(str_value);
    return true;
  };

  if (str_key == "PaletteWidget") {
    str_palette_widget = std::string{str_value};
    return true;
  }
  if (str_key == "TypesContainer") {
    str_types_container = std::string{str_value};
    return true;
  }
  if (str_key == "BackgroundContainer") {
    str_background_container = std::string{str_value};
    return true;
  }
  if (str_key == "TabWidth") {
    int4_tab_width = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "ArrowWidth") {
    int4_arrow_width = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "PreviousProductionTabKey")
    return load_hotkey(hk_previous_production_tab_key);
  if (str_key == "NextProductionTabKey")
    return load_hotkey(hk_next_production_tab_key);
  if (str_key == "ArrowButton") {
    str_arrow_button = std::string{str_value};
    return true;
  }
  if (str_key == "TabButton") {
    str_tab_button = std::string{str_value};
    return true;
  }
  if (str_key == "Background") {
    str_background = std::string{str_value};
    return true;
  }
  if (str_key == "Decorations") {
    str_decorations = std::string{str_value};
    return true;
  }
  if (str_key == "DecorationScrollLeft") {
    str_decoration_scroll_left = std::string{str_value};
    return true;
  }
  if (str_key == "DecorationScrollRight") {
    str_decoration_scroll_right = std::string{str_value};
    return true;
  }
  if (str_key == "TabColor") {
    color_tab = meta::GetColorValue("TabColor", str_value);
    return true;
  }
  if (str_key == "TabColorDone") {
    color_tab_done = meta::GetColorValue("TabColorDone", str_value);
    return true;
  }

  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void ProductionTabsWidget::Initialize(ora::ui::WidgetArgs& args_args) {
  ora::ui::Widget::Initialize(args_args);

  const Rectangle rb = RenderBounds();
  rect_left_button_ =
      Rectangle{rb.X, rb.Y, int4_arrow_width, rb.Height};
  rect_right_button_ =
      Rectangle{rb.Right() - int4_arrow_width, rb.Y, int4_arrow_width,
                rb.Height};
  ptr_font_ = ui::Ui::Instance().TryGetFontDeps("TinyBold");

  cache_left_arrow_ =
      ui::GetCachedStatefulImage(str_decorations, str_decoration_scroll_left);
  cache_right_arrow_ =
      ui::GetCachedStatefulImage(str_decorations, str_decoration_scroll_right);
}

bool ProductionTabsWidget::SelectNextTab(bool reverse) {
  if (str_queue_group.empty())
    return true;

  std::vector<std::pair<bool, ProductionQueue*>> vec_flagged;
  for (const ProductionTab& tab : map_groups_[str_queue_group].vec_tabs) {
    bool b_alert = false;
    for (const auto& item : tab.queue->AllQueued())
      if (item->Done())
        b_alert = true;
    vec_flagged.emplace_back(b_alert, tab.queue);
  }

  std::stable_sort(vec_flagged.begin(), vec_flagged.end(),
                   [](const auto& a, const auto& b) {
                     return a.first && !b.first;
                   });

  std::vector<ProductionQueue*> vec_ordered;
  for (const auto& [alert, queue] : vec_flagged)
    vec_ordered.push_back(queue);
  if (reverse)
    std::reverse(vec_ordered.begin(), vec_ordered.end());

  ProductionQueue* current = CurrentQueue();
  ProductionQueue* next = nullptr;
  bool b_after_current = false;
  for (ProductionQueue* q : vec_ordered) {
    if (q == current) {
      b_after_current = true;
      continue;
    }
    if (b_after_current) {
      next = q;
      break;
    }
  }
  if (next == nullptr && !vec_ordered.empty())
    next = vec_ordered.front();

  SetCurrentQueue(next);
  return true;
}

void ProductionTabsWidget::PickUpCompletedBuilding() {
  MaterializePaletteWidget();
  if (ptr_palette_widget_ != nullptr)
    ptr_palette_widget_->PickUpCompletedBuilding();
}

void ProductionTabsWidget::SetQueueGroup(std::string str_group) {
  fp4_list_offset = 0.0f;
  str_queue_group = std::move(str_group);
  SelectNextTab(false);
}

ProductionQueue* ProductionTabsWidget::CurrentQueue() const {
  MaterializePaletteWidget();
  return ptr_palette_widget_ != nullptr
             ? ptr_palette_widget_->ptr_current_queue
             : nullptr;
}

void ProductionTabsWidget::SetCurrentQueue(ProductionQueue* ptr_queue) {
  MaterializePaletteWidget();
  if (ptr_palette_widget_ != nullptr)
    ptr_palette_widget_->SetCurrentQueue(ptr_queue);
  str_queue_group = ptr_queue != nullptr ? ptr_queue->Info().str_group : "";
}

void ProductionTabsWidget::MaterializePaletteWidget() const {
  if (b_palette_materialized_)
    return;
  b_palette_materialized_ = true;
  if (ui::Ui::Instance().Root() != nullptr)
    ptr_palette_widget_ =
        ui::Ui::Instance().Root()->Get<ProductionPaletteWidget>(
            str_palette_widget);
}

void ProductionTabsWidget::ActorChanged(sim::Actor& a) {
  bool b_has_queue_info = false;
  if (a.Info() != nullptr)
    for (const meta::RecordObject* rec :
         a.Info()->TraitInfosByInterface(
             "OpenRA.Traits.ITraitInfoInterface"))
      if (RecordDerivesProductionQueueInfo(*rec))
        b_has_queue_info = true;
  if (!b_has_queue_info || a.Owner() != a.world().LocalPlayer())
    return;

  std::vector<ProductionQueue*> vec_queues;
  for (const auto& pair : a.world().ActorsWithTrait<ProductionQueue>())
    if (pair.actor->Owner() == a.world().LocalPlayer() &&
        pair.actor->IsInWorld())
      vec_queues.push_back(pair.trait);

  vec_cached_production_queue_enabled_states_.clear();
  for (ProductionQueue* queue : vec_queues)
    vec_cached_production_queue_enabled_states_.emplace_back(
        queue, queue->Enabled());

  for (auto& [key, g] : map_groups_) {
    std::vector<ProductionQueue*> vec_update;
    for (const auto& [queue, enabled] :
         vec_cached_production_queue_enabled_states_)
      vec_update.push_back(queue);
    g.Update(vec_update);
  }

  if (str_queue_group.empty())
    return;

  if (map_groups_[str_queue_group].vec_tabs.empty()) {
    for (const auto& [key, g] : map_groups_)
      if (!g.vec_tabs.empty()) {
        SetQueueGroup(key);
        break;
      }
  } else {
    bool b_contains_current = false;
    for (const ProductionTab& tab :
         map_groups_[str_queue_group].vec_tabs)
      if (tab.queue == CurrentQueue())
        b_contains_current = true;
    if (!b_contains_current)
      SelectNextTab(false);
  }
}

void ProductionTabsWidget::Tick() {
  if (b_left_pressed)
    Scroll(1);
  if (b_right_pressed)
    Scroll(-1);

  bool b_should_update_queues = false;
  for (std::size_t i = 0;
       i < vec_cached_production_queue_enabled_states_.size(); i++) {
    auto& [queue, enabled] = vec_cached_production_queue_enabled_states_[i];
    if (queue->Enabled() != enabled) {
      b_should_update_queues = true;
      enabled = queue->Enabled();
    }
  }

  if (b_should_update_queues)
    for (auto& [key, g] : map_groups_) {
      std::vector<ProductionQueue*> vec_update;
      for (const auto& [queue, enabled] :
           vec_cached_production_queue_enabled_states_)
        vec_update.push_back(queue);
      g.Update(vec_update);
    }
}

void ProductionTabsWidget::Draw() {
  if (str_queue_group.empty() ||
      map_groups_.find(str_queue_group) == map_groups_.end())
    return;

  std::vector<const ProductionTab*> vec_tabs;
  for (const ProductionTab& tab : map_groups_[str_queue_group].vec_tabs)
    if (!tab.queue->BuildableItems().empty())
      vec_tabs.push_back(&tab);

  if (vec_tabs.empty())
    return;

  const Rectangle rb = RenderBounds();

  const bool b_left_disabled = fp4_list_offset >= 0;
  const bool b_left_hover =
      ui::Ui::Instance().MouseOverWidget() == this &&
      rect_left_button_.Contains(gfx::Viewport::int2_last_mouse_pos);
  const bool b_right_disabled =
      fp4_list_offset <= static_cast<float>(
                             bounds_widget.int4_width - rect_right_button_.Width -
                             rect_left_button_.Width - int4_content_width);
  const bool b_right_hover =
      ui::Ui::Instance().MouseOverWidget() == this &&
      rect_right_button_.Contains(gfx::Viewport::int2_last_mouse_pos);

  ui::DrawPanel(str_background, rb);
  ui::ButtonWidget::DrawBackground(str_arrow_button, rect_left_button_,
                                   b_left_disabled, b_left_pressed,
                                   b_left_hover, false);
  ui::ButtonWidget::DrawBackground(str_arrow_button, rect_right_button_,
                                   b_right_disabled, b_right_pressed,
                                   b_right_hover, false);

  const gfx::Sprite& left_arrow_image = cache_left_arrow_.Update(
      ui::StatefulImageKey{b_left_disabled, b_left_pressed, b_left_hover,
                           false, false});
  ui::DrawSprite(
      left_arrow_image,
      core::Vector2{
          static_cast<float>(rect_left_button_.Left() +
                             static_cast<int>(
                                 (rect_left_button_.Width -
                                  static_cast<int>(left_arrow_image.vec_size.X)) /
                                 2)),
          static_cast<float>(rect_left_button_.Top() +
                             static_cast<int>(
                                 (rect_left_button_.Height -
                                  static_cast<int>(left_arrow_image.vec_size.Y)) /
                                 2))});

  const gfx::Sprite& right_arrow_image = cache_right_arrow_.Update(
      ui::StatefulImageKey{b_right_disabled, b_right_pressed, b_right_hover,
                           false, false});
  ui::DrawSprite(
      right_arrow_image,
      core::Vector2{
          static_cast<float>(rect_right_button_.Left() +
                             static_cast<int>(
                                 (rect_right_button_.Width -
                                  static_cast<int>(right_arrow_image.vec_size.X)) /
                                 2)),
          static_cast<float>(rect_right_button_.Top() +
                             static_cast<int>(
                                 (rect_right_button_.Height -
                                  static_cast<int>(right_arrow_image.vec_size.Y)) /
                                 2))});

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps()) {
    ptr_renderer->EnableScissor(
        Rectangle{rect_left_button_.Right(), rb.Y + 1,
                  rect_right_button_.Left() - rect_left_button_.Right() - 1,
                  rb.Height});
    const int2 origin{rect_left_button_.Right() - 1 +
                          static_cast<int>(fp4_list_offset),
                      rect_left_button_.Y};
    int4_content_width = 0;

    for (const ProductionTab* tab : vec_tabs) {
      const Rectangle rect{origin.X + int4_content_width, origin.Y,
                           int4_tab_width, rb.Height};
      const bool b_hover = !b_left_hover && !b_right_hover &&
                           ui::Ui::Instance().MouseOverWidget() == this &&
                           rect.Contains(gfx::Viewport::int2_last_mouse_pos);
      const bool b_highlighted = tab->queue == CurrentQueue();
      ui::ButtonWidget::DrawBackground(str_tab_button, rect, false, false,
                                       b_hover, b_highlighted);
      int4_content_width += int4_tab_width - 1;

      if (ptr_font_ != nullptr) {
        const int2 size_text = ptr_font_->Measure(tab->name);
        const int2 position{
            rect.X + (rect.Width - size_text.X) / 2,
            rect.Y + (rect.Height - size_text.Y) / 2};
        bool b_alert = false;
        for (const auto& item : tab->queue->AllQueued())
          if (item->Done())
            b_alert = true;
        ptr_font_->DrawTextWithContrast(
            tab->name,
            core::Vector2{static_cast<float>(position.X),
                          static_cast<float>(position.Y)},
            b_alert ? color_tab_done : color_tab,
            core::Color::FromArgb(255, 0, 0, 0), 1);
      }
    }

    ptr_renderer->DisableScissor();
  }
}

void ProductionTabsWidget::Scroll(int int4_amount) {
  fp4_list_offset += int4_amount * ui::Ui::Instance().UiScrollSpeed();
  fp4_list_offset = std::min(
      0.0f,
      std::max(static_cast<float>(bounds_widget.int4_width - rect_right_button_.Width -
                                  rect_left_button_.Width -
                                  int4_content_width),
               fp4_list_offset));
}

const std::vector<ProductionTab>& ProductionTabsWidget::TabsForTest() const {
  static const std::vector<ProductionTab> vec_empty;
  const auto it = map_groups_.find(str_queue_group);
  return it != map_groups_.end() ? it->second.vec_tabs : vec_empty;
}

bool ProductionTabsWidget::YieldMouseFocus(
    const ora::MouseInput& mi_input) {
  b_left_pressed = b_right_pressed = false;
  return ora::ui::Widget::YieldMouseFocus(mi_input);
}

bool ProductionTabsWidget::HandleMouseInput(
    const ora::MouseInput& mi_input) {
  if (mi_input.Event == MouseInputEvent::Scroll) {
    Scroll(mi_input.Delta.Y);
    return true;
  }

  if (mi_input.Button != MouseButton::Left)
    return true;

  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return true;

  if (!HasMouseFocus())
    return true;

  if (HasMouseFocus() && mi_input.Event == MouseInputEvent::Up)
    return YieldMouseFocus(mi_input);

  b_left_pressed = rect_left_button_.Contains(mi_input.Location);
  b_right_pressed = rect_right_button_.Contains(mi_input.Location);
  const bool b_left_disabled = fp4_list_offset >= 0;
  const bool b_right_disabled =
      fp4_list_offset <=
      static_cast<float>(bounds_widget.int4_width - rect_right_button_.Width -
                         rect_left_button_.Width - int4_content_width);

  if (b_left_pressed || b_right_pressed) {
    if ((b_left_pressed && !b_left_disabled) ||
        (b_right_pressed && !b_right_disabled))
      ui::Ui::Instance().PlayNotification(
          ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
          str_click_sound);
    else
      ui::Ui::Instance().PlayNotification(
          ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
          str_click_disabled_sound);
  }

  if (!str_queue_group.empty()) {
    const int2 offsetloc{
        mi_input.Location.X -
            (rect_left_button_.Right() - 1 + static_cast<int>(fp4_list_offset)),
        mi_input.Location.Y - rect_left_button_.Y};
    if (offsetloc.X > 0 && offsetloc.X < int4_content_width &&
        map_groups_.find(str_queue_group) != map_groups_.end()) {
      const std::vector<ProductionTab>& vec_tabs =
          map_groups_[str_queue_group].vec_tabs;
      const int index = offsetloc.X / (int4_tab_width - 1);
      if (index >= 0 &&
          static_cast<std::size_t>(index) < vec_tabs.size()) {
        SetCurrentQueue(vec_tabs[static_cast<std::size_t>(index)].queue);
        ui::Ui::Instance().PlayNotification(
            &ptr_world->Map().Rules(), str_click_sound);
      }
    }
  }

  return true;
}

bool ProductionTabsWidget::HandleKeyPress(const ora::KeyInput& e_input) {
  if (e_input.Event != KeyInputEvent::Down)
    return false;

  if (hk_previous_production_tab_key.IsActivatedBy(e_input)) {
    ui::Ui::Instance().PlayNotification(
        ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
        str_click_sound);
    return SelectNextTab(true);
  }

  if (hk_next_production_tab_key.IsActivatedBy(e_input)) {
    ui::Ui::Instance().PlayNotification(
        ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
        str_click_sound);
    return SelectNextTab(false);
  }

  return false;
}

void RegisterProductionTabsWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "ProductionTabsWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ProductionTabsWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

void RegisterProductionTypeButtonWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "ProductionTypeButtonWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ProductionTypeButtonWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
