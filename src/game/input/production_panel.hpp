#pragma once

import std;

#include "game/input/input_events.hpp"
#include "game/input/screen_map.hpp"
#include "game/network/order.hpp"

namespace ora::game::input {

constexpr std::int32_t kPanelWidth{238};
constexpr std::int32_t kPanelTabSize{28};
constexpr std::int32_t kPanelTabStride{30};
constexpr std::int32_t kPanelIconWidth{62};
constexpr std::int32_t kPanelIconHeight{46};
constexpr std::int32_t kPanelIconStrideX{64};
constexpr std::int32_t kPanelIconStrideY{48};
constexpr std::int32_t kPanelIconColumns{3};
constexpr std::int32_t kPanelStartProductionShiftCount{5};

inline constexpr std::array<std::string_view, 6> kPanelTabTypes{
    "Building", "Defense", "Infantry", "Vehicle", "Aircraft", "Naval"};

struct ProductionItemView {
  std::string str_item;
  std::int32_t int4_total_time{1};
  std::int32_t int4_remaining_time{1};
  std::int32_t int4_total_cost{};
  std::int32_t int4_remaining_cost{};
  bool b_started{};
  bool b_paused{};
  bool b_done{};
  bool b_building{};
};

struct ProductionQueueView {
  std::string str_type;
  bool b_enabled{};
  bool b_disallow_paused{};
  std::vector<std::string> vec_items;
  std::vector<ProductionItemView> vec_queue;
};

struct ProductionPanelModel {
  std::string str_current_type;
};

struct PanelGeometry {
  std::int32_t int4_view_w{};
  std::int32_t int4_view_h{};
};

struct PanelDispatchResult {
  bool b_handled{};
  std::vector<network::OrderModel> vec_orders;
};

[[nodiscard]] PxRect panel_rect(PanelGeometry geo);

[[nodiscard]] PxRect panel_tab_rect(PanelGeometry geo, std::size_t idx_tab);

[[nodiscard]] PxRect panel_icon_rect(PanelGeometry geo, std::size_t idx_icon);

[[nodiscard]] bool panel_contains(PanelGeometry geo, std::int32_t int4_x, std::int32_t int4_y);

[[nodiscard]] std::int32_t panel_item_progress_percent(const ProductionItemView& item);

[[nodiscard]] PanelDispatchResult panel_handle_mouse(ProductionPanelModel& panel,
                                                     PanelGeometry geo,
                                                     std::span<const ProductionQueueView> vec_queues,
                                                     std::uint32_t u4_queue_actor,
                                                     const MouseInput& mi);

}
