// UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionPaletteWidget.cs @1f594e320
//          (实现部分)
//          The implementation half.
import std;

#include "mods/production_palette_widget.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "fluent/fluent_provider.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "mods/player_resources.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/building_utils.hpp"
#include "mods/bulk_production_queue.hpp"
#include "mods/place_building.hpp"
#include "mods/place_building_order_generator.hpp"
#include "mods/render_sprites.hpp"
#include "net/order.hpp"
#include "net/order_manager.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/selection.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

void CenterOnActors(gfx::WorldRenderer& world_renderer,
                    const std::vector<sim::Actor*>& vec_actors) {
  if (vec_actors.empty())
    return;

  WVec vec_sum{};
  for (const sim::Actor* ptr_actor : vec_actors) {
    const WPos pos_center = ptr_actor->CenterPosition();
    vec_sum = vec_sum + WVec{pos_center.X, pos_center.Y, pos_center.Z};
  }
  const std::int32_t int4_count = static_cast<std::int32_t>(vec_actors.size());
  world_renderer.Viewport()->Center(
      WPos{vec_sum.X / int4_count, vec_sum.Y / int4_count,
           vec_sum.Z / int4_count});
}

std::string FormatHotkeyIndex(std::int32_t int4_index) {
  std::string str_suffix = std::to_string(int4_index + 1);
  while (str_suffix.size() < 2)
    str_suffix.insert(str_suffix.begin(), '0');
  return str_suffix;
}

core::Vector2 Int2ToVector2(int2 v) {
  return core::Vector2{static_cast<float>(v.X), static_cast<float>(v.Y)};
}

}  // namespace

ProductionPaletteWidget::ProductionPaletteWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer = *ptr_wr_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("orderManager"))
    if (const auto* ptr_om_arg = std::get_if<net::OrderManager*>(ptr_value))
      ptr_order_manager = *ptr_om_arg;

  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon; };

  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr) {
    str_click_sound = ptr_metrics->Get("ClickSound");
    str_click_disabled_sound = ptr_metrics->Get("ClickDisabledSound");
  }
}

ProductionPaletteWidget::ProductionPaletteWidget(
    const ProductionPaletteWidget& widget_other)
    : ora::ui::Widget(widget_other),
      kind_ready_text_style(widget_other.kind_ready_text_style),
      color_text(widget_other.color_text),
      color_ready_text_alt(widget_other.color_ready_text_alt),
      int4_columns(widget_other.int4_columns),
      int2_icon_size(widget_other.int2_icon_size),
      int2_icon_margin(widget_other.int2_icon_margin),
      int2_icon_sprite_offset(widget_other.int2_icon_sprite_offset),
      vec_queued_offset(widget_other.vec_queued_offset),
      vec_bulk_offset(widget_other.vec_bulk_offset),
      kind_queued_text_align(widget_other.kind_queued_text_align),
      str_click_sound(widget_other.str_click_sound),
      str_click_disabled_sound(widget_other.str_click_disabled_sound),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_hotkey_prefix(widget_other.str_hotkey_prefix),
      int4_hotkey_count(widget_other.int4_hotkey_count),
      hk_select_production_building(widget_other.hk_select_production_building),
      str_clock_animation(widget_other.str_clock_animation),
      str_clock_sequence(widget_other.str_clock_sequence),
      str_clock_palette(widget_other.str_clock_palette),
      str_not_buildable_animation(widget_other.str_not_buildable_animation),
      str_not_buildable_sequence(widget_other.str_not_buildable_sequence),
      str_not_buildable_palette(widget_other.str_not_buildable_palette),
      str_overlay_font(widget_other.str_overlay_font),
      str_symbols_font(widget_other.str_symbols_font),
      b_draw_time(widget_other.b_draw_time),
      str_ready_text(widget_other.str_ready_text),
      str_hold_text(widget_other.str_hold_text),
      str_infinite_symbol(widget_other.str_infinite_symbol),
      ptr_world(widget_other.ptr_world),
      ptr_world_renderer(widget_other.ptr_world_renderer),
      ptr_order_manager(widget_other.ptr_order_manager),
      int4_minimum_rows(widget_other.int4_minimum_rows),
      int4_maximum_rows(widget_other.int4_maximum_rows),
      int4_icon_row_offset(widget_other.int4_icon_row_offset),
      int4_max_icon_row_offset(widget_other.int4_max_icon_row_offset),
      vec_hotkeys_(widget_other.vec_hotkeys_),
      vec_queued_offset_config_(widget_other.vec_queued_offset_config_) {
  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon; };
}

bool ProductionPaletteWidget::LoadFieldOrProperty(
    std::string_view str_key, std::string_view str_value) {
  const auto load_int = [&](int& int4_target) {
    int4_target = std::stoi(std::string{str_value});
    return true;
  };

  if (str_key == "ReadyTextStyle") {
    if (str_value == "Solid")
      kind_ready_text_style = ReadyTextStyleOptions::Solid;
    else if (str_value == "AlternatingColor")
      kind_ready_text_style = ReadyTextStyleOptions::AlternatingColor;
    else if (str_value == "Blinking")
      kind_ready_text_style = ReadyTextStyleOptions::Blinking;
    return true;
  }
  if (str_key == "TextColor") {
    color_text = meta::GetColorValue("TextColor", str_value);
    return true;
  }
  if (str_key == "ReadyTextAltColor") {
    color_ready_text_alt = meta::GetColorValue("ReadyTextAltColor", str_value);
    return true;
  }
  if (str_key == "Columns")
    return load_int(int4_columns);
  if (str_key == "IconSize") {
    int2_icon_size = meta::GetSizeValue("IconSize", str_value);
    return true;
  }
  if (str_key == "IconMargin") {
    int2_icon_margin = meta::GetSizeValue("IconMargin", str_value);
    return true;
  }
  if (str_key == "IconSpriteOffset") {
    int2_icon_sprite_offset =
        meta::GetSizeValue("IconSpriteOffset", str_value);
    return true;
  }
  if (str_key == "QueuedOffset") {
    vec_queued_offset =
        Int2ToVector2(meta::GetSizeValue("QueuedOffset", str_value));
    return true;
  }
  if (str_key == "BulkOffset") {
    vec_bulk_offset =
        Int2ToVector2(meta::GetSizeValue("BulkOffset", str_value));
    return true;
  }
  if (str_key == "QueuedTextAlign") {
    if (str_value == "Center")
      kind_queued_text_align = TextAlign::Center;
    else if (str_value == "Right")
      kind_queued_text_align = TextAlign::Right;
    else
      kind_queued_text_align = TextAlign::Left;
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
  if (str_key == "HotkeyCount")
    return load_int(int4_hotkey_count);
  if (str_key == "SelectProductionBuildingHotkey") {
    hk_select_production_building =
        ui::Ui::Instance().HotkeyLookup(str_value);
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
  if (str_key == "NotBuildableAnimation") {
    str_not_buildable_animation = std::string{str_value};
    return true;
  }
  if (str_key == "NotBuildableSequence") {
    str_not_buildable_sequence = std::string{str_value};
    return true;
  }
  if (str_key == "NotBuildablePalette") {
    str_not_buildable_palette = std::string{str_value};
    return true;
  }
  if (str_key == "OverlayFont") {
    str_overlay_font = std::string{str_value};
    return true;
  }
  if (str_key == "SymbolsFont") {
    str_symbols_font = std::string{str_value};
    return true;
  }
  if (str_key == "DrawTime") {
    b_draw_time = meta::GetBoolValue("DrawTime", str_value);
    return true;
  }
  if (str_key == "ReadyText") {
    str_ready_text = std::string{str_value};
    return true;
  }
  if (str_key == "HoldText") {
    str_hold_text = std::string{str_value};
    return true;
  }
  if (str_key == "InfiniteSymbol") {
    str_infinite_symbol = std::string{str_value};
    return true;
  }
  if (str_key == "MinimumRows")
    return load_int(int4_minimum_rows);
  if (str_key == "MaximumRows")
    return load_int(int4_maximum_rows);
  if (str_key == "MaxIconRowOffset")
    return load_int(int4_max_icon_row_offset);

  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void ProductionPaletteWidget::Initialize(ora::ui::WidgetArgs& args_args) {
  ora::ui::Widget::Initialize(args_args);

  if (ptr_world != nullptr) {
    ptr_clock_ = std::make_unique<gfx::Animation>(
        RenderSprites::MakeAnimationDeps(*ptr_world), str_clock_animation);
    ptr_cant_build_ = std::make_unique<gfx::Animation>(
        RenderSprites::MakeAnimationDeps(*ptr_world),
        str_not_buildable_animation);
  }
  if (ptr_cant_build_ != nullptr)
    ptr_cant_build_->PlayFetchIndex(str_not_buildable_sequence,
                                    [] { return 0; });
  vec_hotkeys_.clear();
  for (int i = 0; i < int4_hotkey_count; i++)
    vec_hotkeys_.push_back(ui::Ui::Instance().HotkeyLookup(
        str_hotkey_prefix + FormatHotkeyIndex(i)));

  ptr_overlay_font_ = ui::Ui::Instance().TryGetFontDeps(str_overlay_font);
  ptr_symbol_font_ = ui::Ui::Instance().TryGetFontDeps(str_symbols_font);

  vec_icon_offset_ = 0.5f * Int2ToVector2(int2_icon_size) +
                      Int2ToVector2(int2_icon_sprite_offset);
  str_hold_text = str_hold_text.empty()
                            ? str_hold_text
                            : fluent::FluentProvider::Instance().GetMessage(
                                  str_hold_text);
  vec_hold_offset_ =
      ptr_overlay_font_ != nullptr
          ? vec_icon_offset_ -
                core::Vector2{
                    static_cast<float>(ptr_overlay_font_->Measure(str_hold_text).X) /
                        2,
                    static_cast<float>(ptr_overlay_font_->Measure(str_hold_text).Y) /
                        2}
          : vec_icon_offset_;
  str_ready_text = str_ready_text.empty()
                             ? str_ready_text
                             : fluent::FluentProvider::Instance().GetMessage(
                                   str_ready_text);
  vec_ready_offset_ =
      ptr_overlay_font_ != nullptr
          ? vec_icon_offset_ -
                core::Vector2{
                    static_cast<float>(ptr_overlay_font_->Measure(str_ready_text).X) /
                        2,
                    static_cast<float>(ptr_overlay_font_->Measure(str_ready_text).Y) /
                        2}
          : vec_icon_offset_;

  core::Vector2 vec_infinite_config{};
  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr)
    if (ptr_metrics->TryGetVector2("InfiniteOffset", vec_infinite_config))
      vec_infinite_offset_ = vec_infinite_config + vec_queued_offset_config_;
    else
      vec_infinite_offset_ = vec_queued_offset_config_;
  else
    vec_infinite_offset_ = vec_queued_offset_config_;
  vec_queued_offset_config_ = vec_queued_offset;
}

void ProductionPaletteWidget::ScrollDown() {
  if (CanScrollDown())
    int4_icon_row_offset++;
}

bool ProductionPaletteWidget::CanScrollDown() const {
  const int total_rows =
      (int4_total_icon_count + int4_columns - 1) / int4_columns;
  return int4_icon_row_offset < total_rows - int4_max_icon_row_offset;
}

void ProductionPaletteWidget::ScrollUp() {
  if (CanScrollUp())
    int4_icon_row_offset--;
}

std::vector<const game::ActorInfo*> ProductionPaletteWidget::AllBuildables()
    const {
  if (ptr_current_queue == nullptr)
    return {};

  std::vector<const game::ActorInfo*> vec_items = ptr_current_queue->AllItems();

  std::vector<std::pair<int, const game::ActorInfo*>> vec_ordered;
  for (const game::ActorInfo* item : vec_items) {
    int order = 9999;
    if (const meta::RecordObject* rec =
            FindTraitInfoRecord(*item, "BuildableInfo"))
      order = BuildableInfoData::Parse(*rec).int4_build_palette_order;
    vec_ordered.emplace_back(order, item);
  }
  std::stable_sort(vec_ordered.begin(), vec_ordered.end(),
                   [](const auto& a, const auto& b) {
                     return a.first < b.first;
                   });

  std::vector<const game::ActorInfo*> vec_out;
  for (const auto& [order, item] : vec_ordered)
    vec_out.push_back(item);
  return vec_out;
}

void ProductionPaletteWidget::SetCurrentQueue(ProductionQueue* ptr_queue) {
  ptr_current_queue = ptr_queue;
  if (ptr_current_queue != nullptr)
    UpdateCachedProductionIconOverlays();

  RefreshIcons();
}

void ProductionPaletteWidget::Tick() {
  int4_total_icon_count = static_cast<int>(AllBuildables().size());

  if (ptr_current_queue != nullptr &&
      !ptr_current_queue->Actor()->IsInWorld())
    ptr_current_queue = nullptr;

  if (ptr_current_queue != nullptr) {
    if (ptr_current_queue->Actor()->Owner() != ptr_cached_queue_owner_)
      UpdateCachedProductionIconOverlays();

    RefreshIcons();
  }
}

void ProductionPaletteWidget::MaterializeTooltipContainer() {
  if (b_tooltip_container_materialized_)
    return;
  b_tooltip_container_materialized_ = true;
  if (ui::Ui::Instance().Root() != nullptr)
    ptr_tooltip_container_widget_ =
        ui::Ui::Instance().Root()->Get<ui::TooltipContainerWidget>(
            str_tooltip_container);
}

void ProductionPaletteWidget::MouseEntered() {
  if (!str_tooltip_container.empty()) {
    MaterializeTooltipContainer();
    if (ptr_tooltip_container_widget_ != nullptr)
      ptr_tooltip_container_widget_->SetTooltip(
          str_tooltip_template,
          ora::ui::WidgetArgs{
              {"player", ptr_world->LocalPlayer()},
              {"world", ptr_world}});
  }
}

void ProductionPaletteWidget::MouseExited() {
  if (!str_tooltip_container.empty()) {
    MaterializeTooltipContainer();
    if (ptr_tooltip_container_widget_ != nullptr)
      ptr_tooltip_container_widget_->RemoveTooltip();
  }
}

bool ProductionPaletteWidget::HandleMouseInput(
    const ora::MouseInput& mi_input) {
  ProductionIcon* icon = nullptr;
  for (const auto& [rect, candidate] : vec_icons_)
    if (rect.Contains(mi_input.Location)) {
      icon = const_cast<ProductionIcon*>(&candidate);
      break;
    }

  if (mi_input.Event == MouseInputEvent::Move)
    ptr_tooltip_icon = icon;

  if (mi_input.Event == MouseInputEvent::Scroll) {
    if (mi_input.Delta.Y < 0 && CanScrollDown()) {
      ScrollDown();
      ui::Ui::Instance().ResetTooltips();
      ui::Ui::Instance().PlayNotification(
          ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
          str_click_sound);
    } else if (mi_input.Delta.Y > 0 && CanScrollUp()) {
      ScrollUp();
      ui::Ui::Instance().ResetTooltips();
      ui::Ui::Instance().PlayNotification(
          ptr_world != nullptr ? &ptr_world->Map().Rules() : nullptr,
          str_click_sound);
    }
  }

  if (icon == nullptr)
    return false;

  if (mi_input.Event != MouseInputEvent::Down)
    return true;

  return HandleEvent(*icon, mi_input.Button, mi_input.ModifierFlags);
}

bool ProductionPaletteWidget::PickUpCompletedBuildingIcon(
    ProductionItemLike* item) {
  if (item == nullptr)
    return false;

  const game::ActorInfo* actor = ptr_world->Map().Rules().FindActor(item->Item);
  if (actor == nullptr)
    return false;

  const bool b_is_building =
      FindTraitInfoRecord(*actor, "BuildingInfo") != nullptr;
  if (item->Done() && b_is_building) {
    ptr_world->AdoptOrderGenerator(
        std::make_unique<PlaceBuildingOrderGenerator>(
            *ptr_current_queue, item->Item, *ptr_world_renderer));
    return true;
  }

  return false;
}

void ProductionPaletteWidget::PickUpCompletedBuilding() {
  PickUpCompletedBuildingIcon(ptr_current_queue->CurrentItem());
}

bool ProductionPaletteWidget::HandleLeftClick(ProductionItemLike* item,
                                              ProductionIcon& icon,
                                              int int4_handle_count,
                                              Modifiers modifiers) {
  if (PickUpCompletedBuildingIcon(item)) {
    ui::Ui::Instance().PlayNotification(
        &ptr_world->Map().Rules(), str_click_sound);
    return true;
  }

  if (item != nullptr && item->Paused()) {
    ui::Ui::Instance().PlayNotification(
        &ptr_world->Map().Rules(), str_click_sound);

    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_world->LocalPlayer(),
        ptr_current_queue->Info().str_queued_text_notification);

    net::Order o = net::Order::PauseProduction(ptr_current_queue->Actor(),
                                               icon.name, false);
    ptr_world->IssueOrder(&o);
    return true;
  }

  const game::ActorInfo* buildable = nullptr;
  for (const game::ActorInfo* candidate :
       ptr_current_queue->BuildableItems())
    if (candidate->Name() == icon.name)
      buildable = candidate;

  if (buildable != nullptr) {
    if (ptr_current_queue->Info().b_pay_up_front &&
        ptr_current_queue->GetProductionCost(*buildable) >
            ptr_current_queue->Actor()
                ->Owner()
                ->PlayerActor()
                ->Trait<PlayerResources>()
                ->GetCashAndResources())
      return false;
    ui::Ui::Instance().PlayNotification(
        &ptr_world->Map().Rules(), str_click_sound);

    std::string str_notification_audio;
    std::string str_notification_text;
    const bool b_can_queue = ptr_current_queue->CanQueue(
        *buildable, str_notification_audio, str_notification_text);

    if (ptr_current_queue->AllQueued().empty())
      net::TextNotificationsManager::Instance().AddTransientLine(
          ptr_world->LocalPlayer(), str_notification_text);

    if (b_can_queue) {
      const bool queued = !HasModifier(modifiers, Modifiers::Ctrl);
      net::Order o = net::Order::StartProduction(
          ptr_current_queue->Actor(), icon.name, int4_handle_count, queued);
      ptr_world->IssueOrder(&o);
      return true;
    }
  }

  return false;
}

bool ProductionPaletteWidget::HandleRightClick(ProductionItemLike* item,
                                               ProductionIcon& icon,
                                               int int4_handle_count) {
  if (auto* bulk_production_queue =
          dynamic_cast<BulkProductionQueue*>(ptr_current_queue);
      bulk_production_queue != nullptr &&
      !bulk_production_queue->HasDeliveryStarted()) {
    bool b_ready = false;
    for (const BulkReadyActor& ready :
         bulk_production_queue->GetActorsReadyForDelivery())
      if (ready.actor_info != nullptr && ready.actor_info->Name() == icon.name)
        b_ready = true;
    if (b_ready) {
      net::Order o{"ReturnOrder", ptr_current_queue->Actor(), false};
      o.uint4_extra_data = static_cast<std::uint32_t>(int4_handle_count);
      o.str_target_string = icon.name;
      ptr_world->IssueOrder(&o);
      ui::Ui::Instance().PlayNotification(
          &ptr_world->Map().Rules(), str_click_sound);
      return true;
    }
    return false;
  }

  if (item == nullptr)
    return false;

  ui::Ui::Instance().PlayNotification(
      &ptr_world->Map().Rules(), str_click_sound);

  if (ptr_current_queue->Info().b_disallow_paused || item->Paused() ||
      item->Done() || item->TotalCost == item->RemainingCost ||
      !item->Started()) {
    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_world->LocalPlayer(),
        ptr_current_queue->Info().str_cancelled_text_notification);

    net::Order o = net::Order::CancelProduction(ptr_current_queue->Actor(),
                                                icon.name, int4_handle_count);
    ptr_world->IssueOrder(&o);
  } else {
    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_world->LocalPlayer(),
        ptr_current_queue->Info().str_on_hold_text_notification);

    net::Order o =
        net::Order::PauseProduction(ptr_current_queue->Actor(), icon.name,
                                    true);
    ptr_world->IssueOrder(&o);
  }

  return true;
}

bool ProductionPaletteWidget::HandleMiddleClick(ProductionItemLike* item,
                                                ProductionIcon& icon,
                                                int int4_handle_count) {
  if (item != nullptr) {
    ui::Ui::Instance().PlayNotification(
        &ptr_world->Map().Rules(), str_click_sound);
    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_world->LocalPlayer(),
        ptr_current_queue->Info().str_cancelled_text_notification);

    net::Order o = net::Order::CancelProduction(ptr_current_queue->Actor(),
                                                icon.name, int4_handle_count);
    ptr_world->IssueOrder(&o);
    return true;
  }

  return false;
}

bool ProductionPaletteWidget::HandleEvent(ProductionIcon& icon,
                                          MouseButton btn,
                                          Modifiers modifiers) {
  const int start_count =
      HasModifier(modifiers, Modifiers::Shift) ? 5 : 1;

  const int cancel_count =
      HasModifier(modifiers, Modifiers::Ctrl)
          ? static_cast<int>(ptr_current_queue->AllQueued().size())
          : start_count;
  ProductionItemLike* item =
      !icon.vec_queued.empty() ? icon.vec_queued.front() : nullptr;

  bool handled = false;
  if (btn == MouseButton::Left)
    handled = HandleLeftClick(item, icon, start_count, modifiers);
  else if (btn == MouseButton::Right)
    handled = HandleRightClick(item, icon, cancel_count);
  else if (btn == MouseButton::Middle)
    handled = HandleMiddleClick(item, icon, cancel_count);

  if (!handled)
    ui::Ui::Instance().PlayNotification(
        &ptr_world->Map().Rules(), str_click_disabled_sound);

  return true;
}

bool ProductionPaletteWidget::HandleKeyPress(const ora::KeyInput& e_input) {
  if (e_input.Event == KeyInputEvent::Up || ptr_current_queue == nullptr)
    return false;

  if (hk_select_production_building.IsActivatedBy(e_input))
    return SelectProductionBuilding();

  const Modifiers batch_modifiers =
      HasModifier(e_input.ModifierFlags, Modifiers::Shift)
          ? Modifiers::Shift
          : Modifiers::None;

  ora::KeyInput e_batch = e_input;
  e_batch.ModifierFlags = static_cast<Modifiers>(
      static_cast<std::int32_t>(e_input.ModifierFlags) &
      ~static_cast<std::int32_t>(Modifiers::Shift));

  for (const auto& [rect, icon_entry] : vec_icons_) {
    static_cast<void>(rect);
    if (icon_entry.hotkey.IsActivatedBy(e_batch))
      return HandleEvent(const_cast<ProductionIcon&>(icon_entry),
                         MouseButton::Left, batch_modifiers);
  }
  return false;
}

bool ProductionPaletteWidget::SelectProductionBuilding() {
  gfx::IViewportSurface* viewport =
      ptr_world_renderer != nullptr ? ptr_world_renderer->Viewport() : nullptr;
  sim::ISelection* selection =
      ptr_world != nullptr ? ptr_world->Selection() : nullptr;

  if (ptr_current_queue == nullptr)
    return true;

  Production* facility = ptr_current_queue->MostLikelyProducer();
  sim::Actor* facility_actor =
      facility != nullptr ? &facility->HostActor() : nullptr;

  if (facility_actor == nullptr || facility_actor->OccupiesSpace() == nullptr)
    return true;

  if (selection != nullptr && selection->Actors().size() == 1 &&
      selection->Contains(facility_actor))
    CenterOnActors(*ptr_world_renderer,
                   std::vector<sim::Actor*>{facility_actor});
  else if (selection != nullptr)
    selection->Combine(*ptr_world,
                       std::vector<sim::Actor*>{facility_actor}, false,
                       true);

  ui::Ui::Instance().PlayNotification(
      &ptr_world->Map().Rules(), str_click_sound);
  return true;
}

void ProductionPaletteWidget::UpdateCachedProductionIconOverlays() {
  ptr_cached_queue_owner_ = ptr_current_queue->Actor()->Owner();
  vec_pios_ = ptr_cached_queue_owner_->PlayerActor()
                   ->TraitsImplementing<sim::IProductionIconOverlay>();
}

void ProductionPaletteWidget::RefreshIcons() {
  vec_icons_.clear();
  Production* producer =
      ptr_current_queue != nullptr
          ? ptr_current_queue->MostLikelyProducer()
          : nullptr;
  if (ptr_current_queue == nullptr || producer == nullptr) {
    if (int4_displayed_icon_count != 0) {
      fn_on_icon_count_changed(int4_displayed_icon_count, 0);
      int4_displayed_icon_count = 0;
    }

    rect_event_bounds_ = Rectangle::Empty();
    return;
  }

  const int old_icon_count = int4_displayed_icon_count;
  int4_displayed_icon_count = 0;

  const Rectangle rb = RenderBounds();
  const std::string faction = producer->Faction();

  const std::vector<const game::ActorInfo*> vec_buildables = AllBuildables();
  const int skip = int4_icon_row_offset * int4_columns;
  const int take = int4_max_icon_row_offset * int4_columns;
  for (std::size_t idx = static_cast<std::size_t>(
           std::max(0, skip));
       idx < vec_buildables.size() &&
       static_cast<int>(idx - static_cast<std::size_t>(std::max(0, skip))) <
           take;
       idx++) {
    const game::ActorInfo* item = vec_buildables[idx];
    const int x = int4_displayed_icon_count % int4_columns;
    const int y = int4_displayed_icon_count / int4_columns;
    const Rectangle rect{
        rb.X + x * (int2_icon_size.X + int2_icon_margin.X),
        rb.Y + y * (int2_icon_size.Y + int2_icon_margin.Y),
        int2_icon_size.X, int2_icon_size.Y};

    std::string str_image;
    if (const meta::RecordObject* rec_rsi =
            FindTraitInfoRecord(*item, "RenderSpritesInfo"))
      str_image = RenderSpritesInfoData::Parse(*rec_rsi).GetImage(*item,
                                                                  faction);

    BuildableInfoData bi;
    if (const meta::RecordObject* rec_bi =
            FindTraitInfoRecord(*item, "BuildableInfo"))
      bi = BuildableInfoData::Parse(*rec_bi);

    gfx::Animation icon_anim(RenderSprites::MakeAnimationDeps(*ptr_world),
                             str_image);
    icon_anim.Play(bi.str_icon);

    std::string palette = bi.str_icon_palette;
    if (bi.b_icon_palette_is_player_palette)
      palette += producer->HostActor().Owner()->InternalName();

    ProductionIcon pi;
    pi.actor = item;
    pi.name = item->Name();
    pi.hotkey = int4_displayed_icon_count < int4_hotkey_count &&
                        static_cast<std::size_t>(int4_displayed_icon_count) <
                            vec_hotkeys_.size()
                    ? vec_hotkeys_[static_cast<std::size_t>(
                          int4_displayed_icon_count)]
                    : ui::HotkeyReference{};
    pi.sprite = icon_anim.Image();
    pi.palette = ptr_world_renderer->Palette(palette);
    pi.icon_clock_palette = ptr_world_renderer->Palette(str_clock_palette);
    pi.icon_darken_palette =
        ptr_world_renderer->Palette(str_not_buildable_palette);
    pi.pos = Int2ToVector2(int2{rect.X, rect.Y});
    for (const auto& queued : ptr_current_queue->AllQueued())
      if (queued->Item == item->Name())
        pi.vec_queued.push_back(queued.get());
    pi.production_queue = ptr_current_queue;

    vec_icons_.emplace_back(rect, std::move(pi));
    int4_displayed_icon_count++;
  }

  Rectangle bounds_union = Rectangle::Empty();
  for (const auto& [rect, icon_entry] : vec_icons_) {
    static_cast<void>(icon_entry);
    bounds_union = Rectangle::Union(bounds_union, rect);
  }
  rect_event_bounds_ = bounds_union;

  if (old_icon_count != int4_displayed_icon_count)
    fn_on_icon_count_changed(old_icon_count, int4_displayed_icon_count);
}

void ProductionPaletteWidget::Draw() {
  if (ptr_overlay_font_ != nullptr)
    vec_time_offset_ =
        vec_icon_offset_ -
        core::Vector2{
            static_cast<float>(ptr_overlay_font_->Measure(ui::FormatTime(
                                   0, ptr_world->Timestep()))
                                   .X) /
                2,
            static_cast<float>(ptr_overlay_font_->Measure(ui::FormatTime(
                                   0, ptr_world->Timestep()))
                                   .Y) /
                2};

  if (ptr_current_queue == nullptr)
    return;

  std::vector<const game::ActorInfo*> vec_buildable_items =
      ptr_current_queue->BuildableItems();

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps()) {
    ptr_renderer->EnableAntialiasingFilter();
    for (const auto& [rect, icon_entry] : vec_icons_) {
      static_cast<void>(rect);
      const ProductionIcon& icon = icon_entry;
      ui::DrawSpriteCentered(icon.sprite, icon.palette,
                             icon.pos + vec_icon_offset_);

      for (sim::IProductionIconOverlay* pio : vec_pios_)
        if (pio->IsOverlayActive(*icon.actor)) {
          const gfx::PaletteReference* ptr_palette =
              ptr_world_renderer->Palette(std::string{pio->OverlayPalette()});
          ui::DrawSpriteCentered(*pio->OverlaySprite(), ptr_palette,
                                 icon.pos + vec_icon_offset_ +
                                     pio->OverlayOffset(
                                         Int2ToVector2(int2_icon_size)));
        }

      if (!icon.vec_queued.empty()) {
        ProductionItemLike* first = icon.vec_queued.front();
        ptr_clock_->PlayFetchIndex(
            str_clock_sequence,
            [first, this] {
              return (first->TotalTime() - first->RemainingTime()) *
                     (ptr_clock_->CurrentSequence() != nullptr
                          ? ptr_clock_->CurrentSequence()->Length() - 1
                          : 0) /
                     first->TotalTime();
            });
        ptr_clock_->Tick();

        ui::DrawSpriteCentered(ptr_clock_->Image(), icon.icon_clock_palette,
                               icon.pos + vec_icon_offset_);
      } else {
        bool b_buildable = false;
        for (const game::ActorInfo* candidate : vec_buildable_items)
          if (candidate->Name() == icon.name)
            b_buildable = true;
        if (!b_buildable)
          ui::DrawSpriteCentered(ptr_cant_build_->Image(),
                                 icon.icon_darken_palette,
                                 icon.pos + vec_icon_offset_);
      }
    }
    ptr_renderer->DisableAntialiasingFilter();
  }

  if (ptr_overlay_font_ == nullptr)
    return;

  for (const auto& [rect, icon_entry] : vec_icons_) {
    static_cast<void>(rect);
    const ProductionIcon& icon = icon_entry;
    const std::size_t total = icon.vec_queued.size();
    if (total > 0) {
      ProductionItemLike* first = icon.vec_queued.front();
      const bool b_waiting =
          !ptr_current_queue->IsProducing(*first) && !first->Done();
      if (first->Done()) {
        if (dynamic_cast<BulkProductionQueue*>(ptr_current_queue) == nullptr &&
            ptr_order_manager != nullptr) {
          const std::int64_t frame = ptr_order_manager->LocalFrameNumber();
          if (kind_ready_text_style == ReadyTextStyleOptions::Solid ||
              frame * ptr_world_renderer->World().Timestep() / 360 % 2 == 0)
            ptr_overlay_font_->DrawTextWithContrast(
                str_ready_text, icon.pos + vec_ready_offset_, color_text,
                core::Color::FromArgb(255, 0, 0, 0), 1);
          else if (kind_ready_text_style ==
                   ReadyTextStyleOptions::AlternatingColor)
            ptr_overlay_font_->DrawTextWithContrast(
                str_ready_text, icon.pos + vec_ready_offset_,
                color_ready_text_alt, core::Color::FromArgb(255, 0, 0, 0), 1);
        }
      } else if (first->Paused()) {
        ptr_overlay_font_->DrawTextWithContrast(
            str_hold_text, icon.pos + vec_hold_offset_, color_text,
            core::Color::FromArgb(255, 0, 0, 0), 1);
      } else if (!b_waiting && b_draw_time) {
        ptr_overlay_font_->DrawTextWithContrast(
            ui::FormatTime(
                first->Queue->RemainingTimeActual(*first),
                ptr_world->Timestep()),
            icon.pos + vec_time_offset_, color_text,
            core::Color::FromArgb(255, 0, 0, 0), 1);
      }

      if (first->Infinite && ptr_symbol_font_ != nullptr) {
        ptr_symbol_font_->DrawTextWithContrast(
            str_infinite_symbol, icon.pos + vec_infinite_offset_, color_text,
            core::Color::FromArgb(255, 0, 0, 0), 1);
      } else if (total > 1 || b_waiting) {
        core::Vector2 pos = vec_queued_offset_config_;
        const std::string str_count = std::to_string(total);
        if (kind_queued_text_align != TextAlign::Left && ptr_overlay_font_ != nullptr) {
          const int2 size = ptr_overlay_font_->Measure(str_count);
          pos = kind_queued_text_align == TextAlign::Center
                    ? core::Vector2{vec_queued_offset_config_.X -
                                        static_cast<float>(size.X) / 2,
                                    vec_queued_offset_config_.Y}
                    : core::Vector2{vec_queued_offset_config_.X -
                                        static_cast<float>(size.X),
                                    vec_queued_offset_config_.Y};
        }

        ptr_overlay_font_->DrawTextWithContrast(
            str_count, icon.pos + pos, color_text,
            core::Color::FromArgb(255, 0, 0, 0), 1);
      }
    }
  }
}

const std::string* ProductionPaletteWidget::GetCursor(int2 pt_pos) const {
  for (const auto& [rect, icon_entry] : vec_icons_)
    if (rect.Contains(pt_pos))
      return ora::ui::Widget::GetCursor(pt_pos);

  return nullptr;
}

void RegisterProductionPaletteWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "ProductionPaletteWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ProductionPaletteWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
