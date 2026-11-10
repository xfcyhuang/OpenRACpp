module ora.game.input;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;

namespace ora::game::input {

namespace {

const world::TraitModel* actor_trait_find(const world::WorldModel& w, const world::ActorModel& actor,
                                       std::string_view str_kind) {
  for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
    const world::TraitModel& trait{w.vec_traits[u4_handle]};
    if (trait.str_kind == str_kind) return &trait;
  }
  return nullptr;
}

struct ParsedBounds {
  std::array<std::int32_t, 4> arr_values{};
  std::size_t idx_count{};
};

std::optional<ParsedBounds> parse_bounds(std::string_view str_bounds) {
  ParsedBounds out;
  std::size_t idx_start{0};
  while (out.idx_count < 4) {
    const std::size_t idx_comma{str_bounds.find(',', idx_start)};
    const std::string_view str_part{idx_comma == std::string_view::npos
                                        ? str_bounds.substr(idx_start)
                                        : str_bounds.substr(idx_start, idx_comma - idx_start)};
    std::int32_t int4_value{};
    const char* ptr_first{str_part.data()};
    const char* ptr_last{str_part.data() + str_part.size()};
    const auto res{std::from_chars(ptr_first, ptr_last, int4_value)};
    if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
    out.arr_values[out.idx_count++] = int4_value;
    if (idx_comma == std::string_view::npos) break;
    idx_start = idx_comma + 1;
  }
  if (out.idx_count != 2 && out.idx_count != 4) return std::nullopt;
  return out;
}

std::int32_t wdist_px(std::int32_t int4_wdist) { return int4_wdist * 24 / 1024; }

}

std::int32_t view_to_world_px(std::int32_t int4_view_px) { return int4_view_px; }

std::int32_t world_px_to_cell(std::int32_t int4_world_px) { return int4_world_px / 24; }

std::int32_t actor_center_px(std::int32_t int4_cell) { return int4_cell * 24 + 12; }

std::optional<PxRect> actor_mouse_bounds(const world::WorldModel& w, const world::ActorModel& actor) {
  const world::TraitModel* ptr_selectable{actor_trait_find(w, actor, "Selectable")};
  if (ptr_selectable == nullptr) return std::nullopt;

  const auto opt_bounds{parse_bounds(ptr_selectable->str_param_type)};
  if (!opt_bounds.has_value()) return std::nullopt;

  const std::int32_t int4_w{wdist_px(opt_bounds->arr_values[0])};
  const std::int32_t int4_h{wdist_px(opt_bounds->arr_values[1])};
  if (int4_w <= 0 || int4_h <= 0) return std::nullopt;

  std::int32_t int4_dx{0};
  std::int32_t int4_dy{0};
  if (opt_bounds->idx_count == 4) {
    int4_dx = wdist_px(opt_bounds->arr_values[2]);
    int4_dy = wdist_px(opt_bounds->arr_values[3]);
  }

  const std::int32_t int4_cx{actor_center_px(actor.int4_pos_x) - int4_w / 2 + int4_dx};
  const std::int32_t int4_cy{actor_center_px(actor.int4_pos_y) - int4_h / 2 + int4_dy};
  return PxRect{int4_cx, int4_cy, int4_w, int4_h};
}

std::vector<ActorBoundsPair> actors_at_mouse(const world::WorldModel& w, std::int32_t int4_x,
                                             std::int32_t int4_y) {
  std::vector<ActorBoundsPair> vec_out;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    const auto opt_bounds{actor_mouse_bounds(w, actor)};
    if (opt_bounds.has_value() && opt_bounds->contains(int4_x, int4_y))
      vec_out.push_back(ActorBoundsPair{u4_id, *opt_bounds});
  }
  return vec_out;
}

std::vector<ActorBoundsPair> actors_in_box(const world::WorldModel& w, const PxRect& rect) {
  std::vector<ActorBoundsPair> vec_out;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    const auto opt_bounds{actor_mouse_bounds(w, actor)};
    if (opt_bounds.has_value() && opt_bounds->intersects(rect))
      vec_out.push_back(ActorBoundsPair{u4_id, *opt_bounds});
  }
  return vec_out;
}

}
