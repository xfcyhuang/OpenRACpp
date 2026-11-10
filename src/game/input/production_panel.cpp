module ora.game.input;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;

namespace ora::game::input {

namespace {

const ProductionQueueView* queue_find(std::span<const ProductionQueueView> vec_queues,
                                      std::string_view str_type) {
  for (const ProductionQueueView& queue : vec_queues)
    if (queue.str_type == str_type) return &queue;
  return nullptr;
}

const ProductionItemView* queue_item_find(const ProductionQueueView& queue,
                                          std::string_view str_item) {
  for (const ProductionItemView& item : queue.vec_queue)
    if (item.str_item == str_item) return &item;
  return nullptr;
}

std::int32_t queue_count_item(const ProductionQueueView& queue) {
  return static_cast<std::int32_t>(queue.vec_queue.size());
}

std::uint32_t panel_start_count(ModifierKeys mods) {
  return has_modifier(mods, ModifierKeys::kShift) ? kPanelStartProductionShiftCount : 1;
}

struct PanelHit {
  std::optional<std::size_t> opt_tab;
  std::optional<std::size_t> opt_icon;
};

PanelHit panel_hit_test(PanelGeometry geo, std::int32_t int4_x, std::int32_t int4_y) {
  PanelHit hit;
  for (std::size_t idx{0}; idx < kPanelTabTypes.size(); ++idx)
    if (panel_tab_rect(geo, idx).contains(int4_x, int4_y)) hit.opt_tab = idx;
  for (std::size_t idx{0}; idx < 24; ++idx)
    if (panel_icon_rect(geo, idx).contains(int4_x, int4_y)) hit.opt_icon = idx;
  return hit;
}

}

PxRect panel_rect(PanelGeometry geo) {
  return PxRect{geo.int4_view_w - kPanelWidth, 0, kPanelWidth, geo.int4_view_h};
}

PxRect panel_tab_rect(PanelGeometry geo, std::size_t idx_tab) {
  const PxRect rect_panel{panel_rect(geo)};
  return PxRect{rect_panel.int4_x + 4, 8 + static_cast<std::int32_t>(idx_tab) * kPanelTabStride,
                kPanelTabSize, kPanelTabSize};
}

PxRect panel_icon_rect(PanelGeometry geo, std::size_t idx_icon) {
  const PxRect rect_panel{panel_rect(geo)};
  const std::int32_t int4_col{static_cast<std::int32_t>(idx_icon % kPanelIconColumns)};
  const std::int32_t int4_row{static_cast<std::int32_t>(idx_icon / kPanelIconColumns)};
  return PxRect{rect_panel.int4_x + 42 + int4_col * kPanelIconStrideX,
                8 + int4_row * kPanelIconStrideY, kPanelIconWidth, kPanelIconHeight};
}

bool panel_contains(PanelGeometry geo, std::int32_t int4_x, std::int32_t int4_y) {
  return panel_rect(geo).contains(int4_x, int4_y);
}

std::int32_t panel_item_progress_percent(const ProductionItemView& item) {
  if (item.int4_total_time <= 0) return 0;
  return (item.int4_total_time - item.int4_remaining_time) * 100 / item.int4_total_time;
}

PanelDispatchResult panel_handle_mouse(ProductionPanelModel& panel, PanelGeometry geo,
                                       std::span<const ProductionQueueView> vec_queues,
                                       std::uint32_t u4_queue_actor, const MouseInput& mi) {
  PanelDispatchResult result;

  const std::int32_t int4_x{view_to_world_px(mi.int4_x)};
  const std::int32_t int4_y{view_to_world_px(mi.int4_y)};
  if (!panel_contains(geo, int4_x, int4_y)) return result;
  result.b_handled = true;

  if (mi.event != MouseInputEvent::kDown) return result;

  const PanelHit hit{panel_hit_test(geo, int4_x, int4_y)};

  if (hit.opt_tab.has_value()) {
    const std::string_view str_type{kPanelTabTypes[*hit.opt_tab]};
    const ProductionQueueView* ptr_queue{queue_find(vec_queues, str_type)};
    if (ptr_queue != nullptr && ptr_queue->b_enabled) panel.str_current_type = std::string(str_type);
    return result;
  }

  if (!hit.opt_icon.has_value()) return result;

  const ProductionQueueView* ptr_queue{queue_find(vec_queues, panel.str_current_type)};
  if (ptr_queue == nullptr || !ptr_queue->b_enabled) return result;
  if (*hit.opt_icon >= ptr_queue->vec_items.size()) return result;

  const std::string str_item{ptr_queue->vec_items[*hit.opt_icon]};
  const ProductionItemView* ptr_item{queue_item_find(*ptr_queue, str_item)};

  if (mi.button == MouseButton::kMiddle) {
    const std::uint32_t u4_count{
        has_modifier(mi.mods, ModifierKeys::kCtrl)
            ? static_cast<std::uint32_t>(queue_count_item(*ptr_queue))
            : panel_start_count(mi.mods)};
    result.vec_orders.push_back(make_cancel_production_order(u4_queue_actor, str_item, u4_count));
    return result;
  }

  if (mi.button == MouseButton::kLeft) {
    if (ptr_item != nullptr && ptr_item->b_done && ptr_item->b_building) return result;
    if (ptr_item != nullptr && ptr_item->b_paused) {
      result.vec_orders.push_back(make_pause_production_order(u4_queue_actor, str_item, false));
      return result;
    }
    result.vec_orders.push_back(make_start_production_order(
        u4_queue_actor, str_item, panel_start_count(mi.mods),
        !has_modifier(mi.mods, ModifierKeys::kCtrl)));
    return result;
  }

  if (mi.button == MouseButton::kRight) {
    if (ptr_item == nullptr) return result;
    const std::uint32_t u4_count{
        has_modifier(mi.mods, ModifierKeys::kCtrl)
            ? static_cast<std::uint32_t>(queue_count_item(*ptr_queue))
            : panel_start_count(mi.mods)};
    if (ptr_queue->b_disallow_paused || ptr_item->b_paused || ptr_item->b_done ||
        ptr_item->int4_total_cost == ptr_item->int4_remaining_cost || !ptr_item->b_started) {
      result.vec_orders.push_back(make_cancel_production_order(u4_queue_actor, str_item, u4_count));
      return result;
    }
    result.vec_orders.push_back(make_pause_production_order(u4_queue_actor, str_item, true));
    return result;
  }

  return result;
}

}
