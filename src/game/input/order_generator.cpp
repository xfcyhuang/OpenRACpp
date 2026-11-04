#include "game/input/order_generator.hpp"

#include "game/input/screen_map.hpp"
#include "game/world/trait.hpp"

namespace ora::game::input {

namespace {

const world::TraitModel* actor_trait_find(const world::WorldModel& w,
                                          const world::ActorModel& actor,
                                          std::string_view str_kind) {
  for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
    const world::TraitModel& trait{w.vec_traits[u4_handle]};
    if (trait.str_kind == str_kind) return &trait;
  }
  return nullptr;
}

bool actor_is_targetable(const world::WorldModel& w, const world::ActorModel& actor) {
  return actor_trait_find(w, actor, "Targetable") != nullptr;
}

std::optional<std::uint32_t> target_actor_for_input(const world::WorldModel& w,
                                                    std::uint32_t u4_viewer_player,
                                                    std::int32_t int4_x, std::int32_t int4_y,
                                                    ModifierKeys mods) {
  const std::vector<ActorBoundsPair> vec_hits{actors_at_mouse(w, int4_x, int4_y)};

  std::optional<std::uint32_t> opt_best;
  std::int64_t int8_best{std::numeric_limits<std::int64_t>::min()};
  for (const ActorBoundsPair& hit : vec_hits) {
    const world::ActorModel& actor{w.vec_actors_all[hit.u4_actor_id]};
    if (!actor_is_targetable(w, actor)) continue;

    const std::int32_t int4_priority{selection_priority(w, actor, u4_viewer_player, mods)};
    const std::int32_t int4_cx{hit.rect_bounds.int4_x + hit.rect_bounds.int4_w / 2};
    const std::int32_t int4_cy{hit.rect_bounds.int4_y + hit.rect_bounds.int4_h / 2};
    const std::int32_t int4_dist{pixel_distance(int4_cx - int4_x, int4_cy - int4_y)};
    const std::int64_t int8_score{
        static_cast<std::int64_t>(int4_priority) -
        (static_cast<std::int64_t>(int4_dist) << 16)};
    if (int8_score > int8_best) {
      int8_best = int8_score;
      opt_best = hit.u4_actor_id;
    }
  }
  return opt_best;
}

struct TargeterCandidate {
  std::uint32_t u4_trait_handle{};
  const world::TraitSpec* ptr_spec{};
  std::int32_t int4_priority{};
};

bool targeter_accepts(const world::TraitSpec& spec, const network::TargetModel& target) {
  if (target.kind == network::TargetKind::kActor) return spec.b_order_actor_target;
  if (target.kind == network::TargetKind::kTerrain) return spec.b_order_terrain_target;
  return false;
}

struct UnitOrderChoice {
  std::uint32_t u4_actor{};
  const world::TraitSpec* ptr_spec{};
  network::TargetModel target;
};

}  // namespace

std::int32_t cpos_bits(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_layer) {
  return ((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8) | (int4_layer & 0xff);
}

network::TargetModel make_cell_target(std::int32_t int4_x, std::int32_t int4_y) {
  network::TargetModel target;
  target.kind = network::TargetKind::kTerrain;
  target.b_cell = true;
  target.int4_cell_bits = cpos_bits(int4_x, int4_y, 0);
  target.u1_subcell = 0;
  target.int4_pos_x = actor_center_px(int4_x);
  target.int4_pos_y = actor_center_px(int4_y);
  return target;
}

network::TargetModel make_actor_target(std::uint32_t u4_actor, std::int32_t int4_generation) {
  network::TargetModel target;
  target.kind = network::TargetKind::kActor;
  target.u4_actor = u4_actor;
  target.int4_generation = int4_generation;
  return target;
}

network::OrderModel make_move_order(std::uint32_t u4_subject, std::int32_t int4_cell_x,
                                    std::int32_t int4_cell_y, bool b_queued) {
  network::OrderModel order;
  order.str_order = "Move";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target = make_cell_target(int4_cell_x, int4_cell_y);
  order.b_queued = b_queued;
  return order;
}

network::OrderModel make_attack_order(std::uint32_t u4_subject, std::uint32_t u4_target_actor,
                                      std::int32_t int4_generation, bool b_queued) {
  network::OrderModel order;
  order.str_order = "Attack";
  order.b_has_subject = true;
  order.u4_subject = u4_subject;
  order.target = make_actor_target(u4_target_actor, int4_generation);
  order.b_queued = b_queued;
  return order;
}

network::OrderModel make_create_group_order(std::uint32_t u4_player_actor,
                                            std::span<const std::uint32_t> vec_grouped) {
  network::OrderModel order;
  order.str_order = "CreateGroup";
  order.b_has_subject = true;
  order.u4_subject = u4_player_actor;
  order.b_has_grouped = true;
  order.vec_grouped.assign(vec_grouped.begin(), vec_grouped.end());
  order.b_queued = false;
  return order;
}

network::OrderModel make_start_production_order(std::uint32_t u4_queue_actor,
                                                std::string_view str_item, std::uint32_t u4_count,
                                                bool b_queued) {
  network::OrderModel order;
  order.str_order = "StartProduction";
  order.b_has_subject = true;
  order.u4_subject = u4_queue_actor;
  order.opt_str_target = std::string(str_item);
  order.u4_extra_data = u4_count;
  order.b_queued = b_queued;
  return order;
}

network::OrderModel make_pause_production_order(std::uint32_t u4_queue_actor,
                                                std::string_view str_item, bool b_paused) {
  network::OrderModel order;
  order.str_order = "PauseProduction";
  order.b_has_subject = true;
  order.u4_subject = u4_queue_actor;
  order.opt_str_target = std::string(str_item);
  order.u4_extra_data = b_paused ? 1u : 0u;
  return order;
}

network::OrderModel make_cancel_production_order(std::uint32_t u4_queue_actor,
                                                 std::string_view str_item, std::uint32_t u4_count) {
  network::OrderModel order;
  order.str_order = "CancelProduction";
  order.b_has_subject = true;
  order.u4_subject = u4_queue_actor;
  order.opt_str_target = std::string(str_item);
  order.u4_extra_data = u4_count;
  return order;
}

std::vector<network::OrderModel> unit_order_generator_order(const world::WorldModel& w,
                                                            std::uint32_t u4_viewer_player,
                                                            const SelectionModel& selection,
                                                            std::int32_t int4_cell_x,
                                                            std::int32_t int4_cell_y,
                                                            std::int32_t int4_world_px_x,
                                                            std::int32_t int4_world_px_y,
                                                            const MouseInput& mi) {
  if (mi.button != MouseButton::kRight) return {};

  network::TargetModel target{make_cell_target(int4_cell_x, int4_cell_y)};
  const auto opt_actor{target_actor_for_input(w, u4_viewer_player, int4_world_px_x,
                                              int4_world_px_y, mi.mods)};
  if (opt_actor.has_value())
    target = make_actor_target(*opt_actor, w.vec_actors_all[*opt_actor].int4_generation);

  const bool b_queued{has_modifier(mi.mods, ModifierKeys::kShift)};
  std::vector<UnitOrderChoice> vec_choices;
  for (const std::uint32_t u4_id : selection.vec_actor_ids) {
    if (u4_id >= w.vec_actors_all.size()) continue;
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_viewer_player) continue;
    if (actor.b_disposed || !actor.b_in_world) continue;

    std::vector<TargeterCandidate> vec_targeters;
    for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
      const world::TraitModel& trait{w.vec_traits[u4_handle]};
      const world::TraitSpec* ptr_spec{world::trait_spec_find(trait.str_kind)};
      if (ptr_spec == nullptr || ptr_spec->str_order_id.empty()) continue;
      vec_targeters.push_back(TargeterCandidate{u4_handle, ptr_spec,
                                                ptr_spec->int4_order_priority});
    }
    std::stable_sort(vec_targeters.begin(), vec_targeters.end(),
                     [](const TargeterCandidate& a, const TargeterCandidate& b) {
                       return a.int4_priority > b.int4_priority;
                     });

    bool b_found{false};
    for (int int4_pass{0}; int4_pass < 2 && !b_found; ++int4_pass) {
      const network::TargetModel target_pass{int4_pass == 0 ? target
                                                            : make_cell_target(int4_cell_x,
                                                                               int4_cell_y)};
      for (const TargeterCandidate& cand : vec_targeters) {
        if (!targeter_accepts(*cand.ptr_spec, target_pass)) continue;
        vec_choices.push_back(UnitOrderChoice{u4_id, cand.ptr_spec, target_pass});
        b_found = true;
        break;
      }
    }
  }

  if (vec_choices.empty()) return {};

  std::vector<network::OrderModel> vec_orders;
  const world::ActorModel& actor_first{w.vec_actors_all[vec_choices[0].u4_actor]};
  const std::uint32_t u4_player_actor{w.vec_players[actor_first.u4_owner].u4_actor_id};

  std::vector<std::uint32_t> vec_grouped;
  vec_grouped.reserve(vec_choices.size());
  for (const UnitOrderChoice& choice : vec_choices) vec_grouped.push_back(choice.u4_actor);
  vec_orders.push_back(make_create_group_order(u4_player_actor, vec_grouped));

  for (const UnitOrderChoice& choice : vec_choices) {
    if (choice.ptr_spec->str_order_id == "Attack")
      vec_orders.push_back(make_attack_order(choice.u4_actor, choice.target.u4_actor,
                                             choice.target.int4_generation, b_queued));
    else if (choice.ptr_spec->str_order_id == "Move")
      vec_orders.push_back(make_move_order(choice.u4_actor, int4_cell_x, int4_cell_y, b_queued));
    else
      continue;
  }
  return vec_orders;
}

std::vector<network::OrderModel> order_generator_handle_mouse(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    const DragState& drag, const MouseInput& mi) {
  if (mi.event != MouseInputEvent::kUp || mi.button != MouseButton::kRight) return {};
  if (drag_is_valid_box(drag)) return {};

  const std::int32_t int4_px_x{view_to_world_px(mi.int4_x)};
  const std::int32_t int4_px_y{view_to_world_px(mi.int4_y)};
  return unit_order_generator_order(w, u4_viewer_player, selection, world_px_to_cell(int4_px_x),
                                    world_px_to_cell(int4_px_y), int4_px_x, int4_px_y, mi);
}

}  // namespace ora::game::input
