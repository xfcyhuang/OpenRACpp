module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.sound;
import ora.game.support;
import ora.platforms;

namespace ora::game::production {

namespace {

constexpr std::int32_t kBiBuildDurationModifier{60};
constexpr std::int32_t kQueueBuildDurationModifier{100};
constexpr std::array<std::int32_t, 7> kSpeedReduction{100, 86, 75, 67, 60, 55, 50};

std::vector<std::string_view> split_list(std::string_view str_value) {
  std::vector<std::string_view> vec_out;
  std::size_t idx_start{0};
  while (idx_start <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', idx_start)};
    const std::string_view str_part{str_value.substr(
        idx_start, idx_comma == std::string_view::npos ? std::string_view::npos : idx_comma - idx_start)};
    std::size_t idx_first{0};
    std::size_t idx_last{str_part.size()};
    while (idx_first < idx_last && (str_part[idx_first] == ' ' || str_part[idx_first] == '\t')) ++idx_first;
    while (idx_last > idx_first && (str_part[idx_last - 1] == ' ' || str_part[idx_last - 1] == '\t')) --idx_last;
    if (idx_last > idx_first) vec_out.push_back(str_part.substr(idx_first, idx_last - idx_first));
    if (idx_comma == std::string_view::npos) break;
    idx_start = idx_comma + 1;
  }
  return vec_out;
}

std::string_view player_faction(const world::WorldModel& w, std::uint32_t u4_player) {
  return u4_player < w.vec_players.size() ? std::string_view{w.vec_players[u4_player].str_faction}
                                          : std::string_view{};
}

const world::TraitTemplate* find_template(const world::WorldModel& w, std::string_view str_type,
                                          std::string_view str_kind) {
  for (const world::ActorTypeDef& def : w.vec_actor_types) {
    if (def.str_name_lower != str_type) continue;
    for (const world::TraitTemplate& tpl : def.vec_templates)
      if (tpl.str_kind == str_kind) return &tpl;
    return nullptr;
  }
  return nullptr;
}

const world::TraitTemplate* find_template(const world::WorldModel& w, std::uint32_t u4_actor_id,
                                          std::string_view str_kind) {
  if (u4_actor_id >= w.vec_actors_all.size()) return nullptr;
  return find_template(w, w.vec_actors_all[u4_actor_id].str_type, str_kind);
}

std::string_view kv_find(const world::TraitTemplate& tpl, std::string_view str_key) {
  for (const auto& [str_k, str_v] : tpl.vec_kv)
    if (str_k == str_key) return str_v;
  return {};
}

std::int32_t kv_find_int(const world::TraitTemplate& tpl, std::string_view str_key,
                         std::int32_t int4_fallback) {
  const std::string_view str_value{kv_find(tpl, str_key)};
  if (str_value.empty()) return int4_fallback;
  std::int32_t int4_out{};
  const auto res{std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_out)};
  if (res.ec != std::errc{} || res.ptr != str_value.data() + str_value.size()) return int4_fallback;
  return int4_out;
}

bool kv_find_bool(const world::TraitTemplate& tpl, std::string_view str_key, bool b_fallback) {
  const std::string_view str_value{kv_find(tpl, str_key)};
  if (str_value == "True") return true;
  if (str_value == "False") return false;
  return b_fallback;
}

struct FootprintShape {
  std::int32_t int4_dim_x{1};
  std::int32_t int4_dim_y{1};
  std::vector<std::string> vec_rows;
};

FootprintShape footprint_of(const world::WorldModel& w, std::string_view str_type) {
  FootprintShape shape;
  const world::TraitTemplate* ptr_tpl{find_template(w, str_type, "Building")};
  if (ptr_tpl == nullptr) return shape;
  const std::string_view str_dims{kv_find(*ptr_tpl, "Dimensions")};
  if (!str_dims.empty()) {
    const std::vector<std::string_view> vec_parts{split_list(str_dims)};
    if (vec_parts.size() == 2) {
      std::from_chars(vec_parts[0].data(), vec_parts[0].data() + vec_parts[0].size(), shape.int4_dim_x);
      std::from_chars(vec_parts[1].data(), vec_parts[1].data() + vec_parts[1].size(), shape.int4_dim_y);
    }
  }
  const std::string_view str_footprint{kv_find(*ptr_tpl, "Footprint")};
  std::size_t idx_start{0};
  while (idx_start <= str_footprint.size()) {
    const std::size_t idx_space{str_footprint.find(' ', idx_start)};
    const std::string_view str_row{str_footprint.substr(
        idx_start, idx_space == std::string_view::npos ? std::string_view::npos
                                                       : idx_space - idx_start)};
    if (!str_row.empty()) shape.vec_rows.push_back(std::string(str_row));
    if (idx_space == std::string_view::npos) break;
    idx_start = idx_space + 1;
  }
  return shape;
}

char footprint_cell(const FootprintShape& shape, std::int32_t int4_dx, std::int32_t int4_dy) {
  if (int4_dy < 0 || int4_dy >= static_cast<std::int32_t>(shape.vec_rows.size())) return '_';
  const std::string& str_row{shape.vec_rows[static_cast<std::size_t>(int4_dy)]};
  if (int4_dx < 0 || int4_dx >= static_cast<std::int32_t>(str_row.size())) return '_';
  return str_row[static_cast<std::size_t>(int4_dx)];
}

std::vector<std::pair<std::int32_t, std::int32_t>> footprint_tiles(const world::WorldModel& w,
                                                                   std::string_view str_type,
                                                                   std::int32_t int4_x,
                                                                   std::int32_t int4_y) {
  std::vector<std::pair<std::int32_t, std::int32_t>> vec_cells;
  const FootprintShape shape{footprint_of(w, str_type)};
  for (std::int32_t int4_dy{0}; int4_dy < shape.int4_dim_y; ++int4_dy)
    for (std::int32_t int4_dx{0}; int4_dx < shape.int4_dim_x; ++int4_dx)
      if (footprint_cell(shape, int4_dx, int4_dy) != '_') vec_cells.emplace_back(int4_x + int4_dx, int4_y + int4_dy);
  return vec_cells;
}

std::vector<std::pair<std::int32_t, std::int32_t>> actor_blocking_cells(const world::WorldModel& w,
                                                                        std::uint32_t u4_actor_id) {
  const world::ActorModel& actor{w.vec_actors_all[u4_actor_id]};
  if (find_template(w, actor.str_type, "Building") != nullptr)
    return footprint_tiles(w, actor.str_type, actor.int4_pos_x, actor.int4_pos_y);
  return {{actor.int4_pos_x, actor.int4_pos_y}};
}

std::int64_t parse_wdist(std::string_view str_value) {
  const std::size_t idx_c{str_value.find('c')};
  if (idx_c == std::string_view::npos) return 0;
  std::int64_t int8_cells{};
  std::int64_t int8_sub{};
  std::from_chars(str_value.data(), str_value.data() + idx_c, int8_cells);
  std::from_chars(str_value.data() + idx_c + 1, str_value.data() + str_value.size(), int8_sub);
  return int8_cells * 1024 + int8_sub;
}

std::pair<std::int64_t, std::int64_t> building_center(const world::WorldModel& w,
                                                      std::string_view str_type, std::int32_t int4_x,
                                                      std::int32_t int4_y) {
  const FootprintShape shape{footprint_of(w, str_type)};
  std::int64_t int8_x{static_cast<std::int64_t>(int4_x) * 1024 + 512};
  std::int64_t int8_y{static_cast<std::int64_t>(int4_y) * 1024 + 512};
  int8_x += (static_cast<std::int64_t>(shape.int4_dim_x) * 1024 + 512 - 1536) / 2;
  int8_y += (static_cast<std::int64_t>(shape.int4_dim_y) * 1024 + 512 - 1536) / 2;
  if (const world::TraitTemplate* ptr_tpl{find_template(w, str_type, "Building")}) {
    const std::vector<std::string_view> vec_offset{split_list(kv_find(*ptr_tpl, "LocalCenterOffset"))};
    if (vec_offset.size() == 3) {
      std::int64_t int8_off_x{};
      std::int64_t int8_off_y{};
      std::from_chars(vec_offset[0].data(), vec_offset[0].data() + vec_offset[0].size(), int8_off_x);
      std::from_chars(vec_offset[1].data(), vec_offset[1].data() + vec_offset[1].size(), int8_off_y);
      int8_x += int8_off_x;
      int8_y += int8_off_y;
    }
  }
  return {int8_x, int8_y};
}

std::int64_t horizontal_length(std::int64_t int8_dx, std::int64_t int8_dy) {
  return static_cast<std::int64_t>(
      std::sqrt(static_cast<double>(int8_dx * int8_dx + int8_dy * int8_dy)));
}

PlayerProductionState* player_state(world::WorldModel& w, std::uint32_t u4_player) {
  if (u4_player >= w.prod.vec_players.size()) return nullptr;
  return &w.prod.vec_players[u4_player];
}

ProductionQueueState* queue_state(PlayerProductionState& state, std::string_view str_type) {
  for (ProductionQueueState& queue : state.vec_queues)
    if (queue.str_type == str_type) return &queue;
  return nullptr;
}

const ProductionQueueState* queue_state(const PlayerProductionState& state,
                                        std::string_view str_type) {
  for (const ProductionQueueState& queue : state.vec_queues)
    if (queue.str_type == str_type) return &queue;
  return nullptr;
}

std::set<std::string> owned_prerequisites(const world::WorldModel& w, std::uint32_t u4_player) {
  std::set<std::string> set_base;
  std::vector<std::pair<const world::TraitModel*, const world::ActorModel*>> vec_gated;
  const std::string& str_faction{w.vec_players[u4_player].str_faction};

  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
      const world::TraitModel& trait{w.vec_traits[u4_handle]};
      if (trait.str_kind == "ProvidesTechPrerequisite") {
        if (trait.str_param2 == w.lobby.str_techlevel)
          for (const std::string_view str_p : split_list(trait.str_param_type))
            set_base.insert(std::string(str_p));
      } else if (trait.str_kind == "ProvidesPrerequisite") {
        const bool b_faction_ok{[&] {
          const std::vector<std::string_view> vec_factions{split_list(trait.str_param2)};
          if (vec_factions.empty()) return true;
          for (const std::string_view str_f : vec_factions)
            if (str_f == str_faction) return true;
          return false;
        }()};
        if (!b_faction_ok) continue;
        if (trait.str_param3.empty())
          set_base.insert(trait.str_param_type.empty() ? actor.str_type : trait.str_param_type);
        else
          vec_gated.emplace_back(&trait, &actor);
      }
    }
  }

  std::set<std::string> set_all{set_base};
  for (const auto& [ptr_trait, ptr_actor] : vec_gated) {
    bool b_ok{true};
    for (const std::string_view str_req : split_list(ptr_trait->str_param3))
      if (!set_base.contains(std::string(str_req))) b_ok = false;
    if (b_ok)
      set_all.insert(ptr_trait->str_param_type.empty() ? ptr_actor->str_type : ptr_trait->str_param_type);
  }
  return set_all;
}

bool prereqs_satisfied(const std::set<std::string>& set_owned, std::string_view str_prereqs) {
  for (const std::string_view str_p : split_list(str_prereqs)) {
    std::string_view str_key{str_p};
    const bool b_tilde{!str_key.empty() && str_key.front() == '~'};
    if (b_tilde) str_key.remove_prefix(1);
    const bool b_negate{!str_key.empty() && str_key.front() == '!'};
    if (b_negate) str_key.remove_prefix(1);
    const bool b_owned{set_owned.contains(std::string(str_key))};
    if (b_negate == b_owned) return false;
  }
  return true;
}

bool actor_buildable(const world::WorldModel& w, std::uint32_t u4_player, std::string_view str_type) {
  const world::TraitTemplate* ptr_tpl{find_template(w, str_type, "Buildable")};
  if (ptr_tpl == nullptr) return false;
  return prereqs_satisfied(owned_prerequisites(w, u4_player), ptr_tpl->str_param3);
}

std::int32_t production_cost(const world::WorldModel& w, std::string_view str_type) {
  const world::TraitTemplate* ptr_tpl{find_template(w, str_type, "Valued")};
  if (ptr_tpl == nullptr) return 0;
  return ptr_tpl->int4_param;
}

std::int32_t build_time(const world::WorldModel& w, std::uint32_t u4_player,
                        const ProductionQueueState& queue, std::string_view str_type) {
  const world::TraitTemplate* ptr_tpl{find_template(w, str_type, "Buildable")};
  std::int32_t int4_time{production_cost(w, str_type)};
  std::int32_t int4_bi_modifier{kBiBuildDurationModifier};
  std::int32_t int4_queue_modifier{kQueueBuildDurationModifier};
  if (ptr_tpl != nullptr) {
    int4_bi_modifier = kv_find_int(*ptr_tpl, "BuildDurationModifier", kBiBuildDurationModifier);
    int4_queue_modifier = kv_find_int(*ptr_tpl, "QueueBuildDurationModifier", kQueueBuildDurationModifier);
  }
  const std::array<std::int32_t, 2> vec4_mods{int4_bi_modifier, int4_queue_modifier};
  int4_time = apply_percentage_modifiers(int4_time, vec4_mods);

  std::string_view str_type_want{queue.str_type};
  if (ptr_tpl != nullptr && !ptr_tpl->str_param2.empty()) str_type_want = ptr_tpl->str_param2;
  std::int32_t int4_producers{0};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    const world::TraitTemplate* ptr_production{find_template(w, u4_id, "Production")};
    if (ptr_production == nullptr) continue;
    for (const std::string_view str_p : split_list(ptr_production->str_param_type))
      if (str_p == str_type_want) {
        ++int4_producers;
        break;
      }
  }
  std::size_t idx{static_cast<std::size_t>(std::clamp(int4_producers, 1, 7)) - 1};
  return int4_time * kSpeedReduction[idx] / 100;
}

void refund_item(PlayerResourcesState& res, ProductionItemModel& item) {
  if (item.int4_resources_paid > 0) {
    refund_resources(res, item.int4_resources_paid);
    item.int4_remaining_cost += item.int4_resources_paid;
  }
  refund_cash(res, item.int4_total_cost - item.int4_remaining_cost);
}

void end_production(ProductionQueueState& queue, std::size_t idx_item) {
  queue.vec_queue.erase(queue.vec_queue.begin() + static_cast<std::ptrdiff_t>(idx_item));
}

void item_tick(world::WorldModel& w, std::uint32_t u4_player, ProductionQueueState& queue,
               std::size_t idx_queue, ProductionItemModel& item) {
  PlayerProductionState& state{*player_state(w, u4_player)};
  if (!item.b_started) {
    const std::int32_t int4_time{build_time(w, u4_player, queue, item.str_item)};
    if (int4_time > 0) {
      item.int4_total_time = int4_time;
      item.int4_remaining_time = int4_time;
    }
    item.b_started = true;
  }

  if (item.b_done) {
    w.deque_frame_end.push_back(
        world::FrameEndTask{.u4_kind = 1, .u4_player = u4_player,
                            .u4_value = static_cast<std::uint32_t>(idx_queue),
                            .str_item = item.str_item});
    return;
  }

  if (item.b_paused) return;

  if (power_state(state.power.int4_power_provided, state.power.int4_power_drained) !=
      PowerState::kNormal) {
    item.int4_slowdown -= 100;
    if (item.int4_slowdown < 0)
      item.int4_slowdown += queue.int4_low_power_modifier;
    else
      return;
  }

  const std::int32_t int4_expected{item.int4_remaining_time == 1
                                       ? 0
                                       : item.int4_total_cost * item.int4_remaining_time /
                                             std::max(1, item.int4_total_time)};
  const std::int32_t int4_cost_frame{item.int4_remaining_cost - int4_expected};
  if (state.res.int4_resources > 0 && state.res.int4_resources <= int4_cost_frame)
    item.int4_resources_paid += state.res.int4_resources;
  else if (state.res.int4_resources > int4_cost_frame)
    item.int4_resources_paid += int4_cost_frame;
  if (int4_cost_frame != 0 && !take_cash(state.res, int4_cost_frame)) {
    item.int4_resources_paid -= state.res.int4_resources;
    return;
  }
  item.int4_remaining_cost -= int4_cost_frame;

  --item.int4_remaining_time;
  if (item.int4_remaining_time <= 0) item.b_done = true;
}

void cancel_unbuildable(world::WorldModel& w, std::uint32_t u4_player, ProductionQueueState& queue) {
  if (queue.vec_queue.empty()) return;
  std::set<std::string> set_buildable;
  const std::set<std::string> set_owned{owned_prerequisites(w, u4_player)};
  for (const world::ActorTypeDef& def : w.vec_actor_types) {
    for (const world::TraitTemplate& tpl : def.vec_templates) {
      if (tpl.str_kind != "Buildable") continue;
      bool b_queue_match{false};
      for (const std::string_view str_q : split_list(tpl.str_param_type))
        if (str_q == queue.str_type) b_queue_match = true;
      if (b_queue_match && prereqs_satisfied(set_owned, tpl.str_param3))
        set_buildable.insert(def.str_name_lower);
    }
  }

  PlayerProductionState& state{*player_state(w, u4_player)};
  for (std::size_t idx{queue.vec_queue.size()}; idx-- > 0;) {
    if (set_buildable.contains(queue.vec_queue[idx].str_item)) continue;
    refund_item(state.res, queue.vec_queue[idx]);
    end_production(queue, idx);
  }
}

bool can_place_building(const world::WorldModel& w, std::string_view str_type, std::int32_t int4_x,
                        std::int32_t int4_y) {
  const std::vector<std::pair<std::int32_t, std::int32_t>> vec_tiles{
      footprint_tiles(w, str_type, int4_x, int4_y)};
  for (const auto& [int4_cx, int4_cy] : vec_tiles)
    for (const std::uint32_t u4_id : w.vec_in_world) {
      for (const auto& [int4_bx, int4_by] : actor_blocking_cells(w, u4_id))
        if (int4_bx == int4_cx && int4_by == int4_cy) return false;
    }
  return true;
}

bool is_close_enough_to_base(const world::WorldModel& w, std::uint32_t u4_player,
                             std::string_view str_type, std::int32_t int4_x, std::int32_t int4_y) {
  const world::TraitTemplate* ptr_building{find_template(w, str_type, "Building")};
  const std::int32_t int4_adjacent{ptr_building != nullptr
                                       ? kv_find_int(*ptr_building, "Adjacent", 2)
                                       : 2};
  const bool b_requires_base{
      ptr_building != nullptr && kv_find_bool(*ptr_building, "RequiresBaseProvider", false)};

  if (w.lobby.b_build_radius && b_requires_base) {
    bool b_provider{false};
    const auto [int8_cx, int8_cy]{building_center(w, str_type, int4_x, int4_y)};
    for (const std::uint32_t u4_id : w.vec_in_world) {
      const world::ActorModel& actor{w.vec_actors_all[u4_id]};
      if (actor.u4_owner != u4_player) continue;
      const world::TraitTemplate* ptr_tpl{find_template(w, u4_id, "BaseProvider")};
      if (ptr_tpl == nullptr) continue;
      const std::int64_t int8_range{parse_wdist(kv_find(*ptr_tpl, "Range"))};
      const auto [int8_px, int8_py]{
          building_center(w, actor.str_type, actor.int4_pos_x, actor.int4_pos_y)};
      if (horizontal_length(int8_cx - int8_px, int8_cy - int8_py) <= int8_range) b_provider = true;
    }
    if (!b_provider) return false;
  }

  const FootprintShape shape{footprint_of(w, str_type)};
  const std::int32_t int4_scan_x0{int4_x - int4_adjacent};
  const std::int32_t int4_scan_y0{int4_y - int4_adjacent};
  const std::int32_t int4_scan_x1{int4_x + shape.int4_dim_x + int4_adjacent};
  const std::int32_t int4_scan_y1{int4_y + shape.int4_dim_y + int4_adjacent};

  std::vector<std::pair<std::int32_t, std::int32_t>> vec_candidates;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    if (find_template(w, u4_id, "GivesBuildableArea") == nullptr) continue;
    for (const auto& cell : footprint_tiles(w, actor.str_type, actor.int4_pos_x, actor.int4_pos_y)) {
      if (cell.first < int4_scan_x0 || cell.first >= int4_scan_x1 || cell.second < int4_scan_y0 ||
          cell.second >= int4_scan_y1)
        continue;
      vec_candidates.push_back(cell);
    }
  }

  const std::vector<std::pair<std::int32_t, std::int32_t>> vec_tiles{
      footprint_tiles(w, str_type, int4_x, int4_y)};
  for (const auto& [int4_ax, int4_ay] : vec_candidates)
    for (const auto& [int4_bx, int4_by] : vec_tiles)
      if (std::abs(int4_ax - int4_bx) <= int4_adjacent && std::abs(int4_ay - int4_by) <= int4_adjacent)
        return true;
  return false;
}

bool exit_cell_usable(const world::WorldModel& w, std::uint32_t u4_producer,
                      std::int32_t int4_cx, std::int32_t int4_cy) {
  std::int32_t int4_sharing{0};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    if (u4_id == u4_producer) continue;
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.int4_pos_x != int4_cx || actor.int4_pos_y != int4_cy) continue;
    if (find_template(w, u4_id, "Building") != nullptr) return false;
    ++int4_sharing;
  }
  return int4_sharing < 4;
}

const world::TraitTemplate* select_exit(world::WorldModel& w, std::uint32_t u4_producer,
                                        std::string_view str_production_type,
                                        std::int32_t& int4_exit_x, std::int32_t& int4_exit_y) {
  const world::ActorModel& actor{w.vec_actors_all[u4_producer]};
  const world::ActorTypeDef* ptr_def{nullptr};
  for (const world::ActorTypeDef& def : w.vec_actor_types)
    if (def.str_name_lower == actor.str_type) ptr_def = &def;
  if (ptr_def == nullptr) return nullptr;

  std::vector<const world::TraitTemplate*> vec_group;
  for (const world::TraitTemplate& tpl : ptr_def->vec_templates) {
    if (tpl.str_kind != "Exit") continue;
    const std::vector<std::string_view> vec_types{split_list(tpl.str_param2)};
    if (!vec_types.empty()) {
      bool b_match{false};
      for (const std::string_view str_t : vec_types)
        if (str_t == str_production_type) b_match = true;
      if (!b_match) continue;
    }
    vec_group.push_back(&tpl);
  }

  auto cell_of{[](const world::TraitTemplate& tpl, std::int32_t& int4_dx, std::int32_t& int4_dy) {
    int4_dx = 0;
    int4_dy = 0;
    const std::vector<std::string_view> vec_cell{split_list(tpl.str_param_type)};
    if (vec_cell.size() == 2) {
      std::from_chars(vec_cell[0].data(), vec_cell[0].data() + vec_cell[0].size(), int4_dx);
      std::from_chars(vec_cell[1].data(), vec_cell[1].data() + vec_cell[1].size(), int4_dy);
    }
  }};

  for (std::size_t idx{0}; idx + 1 < vec_group.size(); ++idx) {
    const std::int32_t int4_j{
        support::next(w.rng_shared, static_cast<std::int32_t>(vec_group.size() - idx))};
    std::swap(vec_group[idx], vec_group[static_cast<std::size_t>(int4_j) + idx]);
    std::int32_t int4_dx{};
    std::int32_t int4_dy{};
    cell_of(*vec_group[idx], int4_dx, int4_dy);
    if (exit_cell_usable(w, u4_producer, actor.int4_pos_x + int4_dx, actor.int4_pos_y + int4_dy)) {
      int4_exit_x = actor.int4_pos_x + int4_dx;
      int4_exit_y = actor.int4_pos_y + int4_dy;
      return vec_group[idx];
    }
  }
  if (!vec_group.empty()) {
    std::int32_t int4_dx{};
    std::int32_t int4_dy{};
    cell_of(*vec_group.back(), int4_dx, int4_dy);
    if (exit_cell_usable(w, u4_producer, actor.int4_pos_x + int4_dx, actor.int4_pos_y + int4_dy)) {
      int4_exit_x = actor.int4_pos_x + int4_dx;
      int4_exit_y = actor.int4_pos_y + int4_dy;
      return vec_group.back();
    }
  }
  return nullptr;
}

bool produce_unit(world::WorldModel& w, std::uint32_t u4_player, ProductionQueueState& queue,
                  const std::string& str_item) {
  const world::TraitTemplate* ptr_buildable{find_template(w, str_item, "Buildable")};
  std::string str_type{queue.str_type};
  if (ptr_buildable != nullptr && !ptr_buildable->str_param2.empty()) str_type = ptr_buildable->str_param2;

  std::vector<std::uint32_t> vec_producers;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    const world::TraitTemplate* ptr_tpl{find_template(w, u4_id, "Production")};
    if (ptr_tpl == nullptr) continue;
    for (const std::string_view str_p : split_list(ptr_tpl->str_param_type))
      if (str_p == str_type) {
        vec_producers.push_back(u4_id);
        break;
      }
  }
  std::sort(vec_producers.begin(), vec_producers.end(), std::greater<std::uint32_t>());

  for (const std::uint32_t u4_producer : vec_producers) {
    std::int32_t int4_exit_x{};
    std::int32_t int4_exit_y{};
    const world::TraitTemplate* ptr_exit{select_exit(w, u4_producer, str_type, int4_exit_x, int4_exit_y)};
    if (ptr_exit == nullptr) continue;
    auto res_spawn{world::create_actor(w, str_item, u4_player, int4_exit_x, int4_exit_y, true)};
    if (!res_spawn.has_value()) return false;

    if (const world::TraitTemplate* ptr_body{find_template(w, str_item, "WithInfantryBody")}) {
      const std::vector<std::string_view> vec_idle{split_list(ptr_body->str_param_type)};
      if (!vec_idle.empty()) {
        support::next(w.rng_shared, static_cast<std::int32_t>(vec_idle.size()));
        support::next(w.rng_shared, kv_find_int(*ptr_body, "MinIdleDelay", 30),
                      kv_find_int(*ptr_body, "MaxIdleDelay", 110));
      }
    }
    return true;
  }

  if (vec_producers.empty()) {
    PlayerProductionState& state{*player_state(w, u4_player)};
    for (std::size_t idx{queue.vec_queue.size()}; idx-- > 0;)
      if (queue.vec_queue[idx].str_item == str_item) {
        refund_item(state.res, queue.vec_queue[idx]);
        end_production(queue, idx);
      }
  }
  return false;
}

void queue_resolve(const world::TraitResolveCtx& ctx, world::TraitModel& trait) {
  const network::OrderModel& order{ctx.ref_order};
  if (order.str_order != "StartProduction" && order.str_order != "PauseProduction" &&
      order.str_order != "CancelProduction")
    return;

  world::WorldModel& w{ctx.ref_world};
  const std::uint32_t u4_player{ctx.ref_actor.u4_owner};
  PlayerProductionState* ptr_state{player_state(w, u4_player)};
  if (ptr_state == nullptr) return;
  ProductionQueueState* ptr_queue{queue_state(*ptr_state, trait.str_param_type)};
  if (ptr_queue == nullptr) return;
  if (!ptr_queue->b_enabled) return;

  if (order.str_order == "StartProduction") {
    if (!order.opt_str_target.has_value()) return;
    const std::string str_unit{*order.opt_str_target};
    const world::TraitTemplate* ptr_buildable{find_template(w, str_unit, "Buildable")};
    if (ptr_buildable == nullptr) return;
    bool b_queue_match{false};
    for (const std::string_view str_q : split_list(ptr_buildable->str_param_type))
      if (str_q == ptr_queue->str_type) b_queue_match = true;
    if (!b_queue_match) return;
    if (!actor_buildable(w, u4_player, str_unit)) return;

    const std::int32_t int4_cost{production_cost(w, str_unit)};
    const std::uint32_t u4_amount{order.u4_extra_data};
    for (std::uint32_t u4_n{0}; u4_n < u4_amount; ++u4_n) {
      ProductionItemModel item;
      item.str_item = str_unit;
      item.int4_total_cost = int4_cost;
      item.int4_remaining_cost = int4_cost;
      item.int4_total_time = 1;
      item.int4_remaining_time = 1;
      if (!order.b_queued && ptr_queue->vec_queue.size() > 1)
        ptr_queue->vec_queue.insert(ptr_queue->vec_queue.begin() + 1, std::move(item));
      else
        ptr_queue->vec_queue.push_back(std::move(item));
    }
    return;
  }

  if (order.str_order == "PauseProduction") {
    const bool b_pause{order.u4_extra_data != 0};
    for (ProductionItemModel& item : ptr_queue->vec_queue)
      if (order.opt_str_target == item.str_item) {
        item.b_paused = b_pause;
        break;
      }
    return;
  }

  const std::uint32_t u4_count{std::max<std::uint32_t>(1, order.u4_extra_data == 0 ? 1 : order.u4_extra_data)};
  for (std::uint32_t u4_n{0}; u4_n < u4_count; ++u4_n) {
    bool b_found{false};
    for (std::size_t idx{ptr_queue->vec_queue.size()}; idx-- > 0;) {
      if (ptr_queue->vec_queue[idx].str_item != order.opt_str_target) continue;
      refund_item(ptr_state->res, ptr_queue->vec_queue[idx]);
      end_production(*ptr_queue, idx);
      b_found = true;
      break;
    }
    if (!b_found) break;
  }
}

void place_building_resolve(const world::TraitResolveCtx& ctx, world::TraitModel&) {
  const network::OrderModel& order{ctx.ref_order};
  if (order.str_order != "PlaceBuilding" && order.str_order != "LineBuild" &&
      order.str_order != "PlacePlug")
    return;
  if (order.str_order != "PlaceBuilding") return;
  if (!order.opt_str_target.has_value()) return;

  ctx.ref_world.deque_frame_end.push_back(
      world::FrameEndTask{.u4_kind = 2, .u4_player = ctx.ref_actor.u4_owner,
                          .u4_value = static_cast<std::uint32_t>(order.target.int4_cell_bits),
                          .str_item = *order.opt_str_target});
}

void queue_tick(const world::TraitTickCtx& ctx, world::TraitModel& trait) {
  world::WorldModel& w{ctx.ref_world};
  const std::uint32_t u4_player{ctx.ref_actor.u4_owner};
  PlayerProductionState* ptr_state{player_state(w, u4_player)};
  if (ptr_state == nullptr) return;
  ProductionQueueState* ptr_queue{queue_state(*ptr_state, trait.str_param_type)};
  if (ptr_queue == nullptr) return;
  ProductionQueueState& queue{*ptr_queue};

  bool b_enabled{false};
  bool b_active{false};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
      const world::TraitModel& t{w.vec_traits[u4_handle]};
      if (t.str_kind != "Production") continue;
      bool b_match{false};
      for (const std::string_view str_p : split_list(t.str_param_type))
        if (str_p == queue.str_type) b_match = true;
      if (actor.u4_owner != u4_player || !b_match) continue;
      b_enabled |= queue.b_valid_faction;
      b_active |= true;
    }
  }

  if (!b_enabled && queue.b_enabled) {
    for (ProductionItemModel& item : queue.vec_queue) refund_item(ptr_state->res, item);
    queue.vec_queue.clear();
  }
  queue.b_enabled = b_enabled;

  cancel_unbuildable(w, u4_player, queue);
  if (!queue.vec_queue.empty() && b_active) {
    const std::size_t idx_queue{static_cast<std::size_t>(&queue - ptr_state->vec_queues.data())};
    item_tick(w, u4_player, queue, idx_queue, queue.vec_queue[0]);
  }
}

std::int32_t queue_sync_hash(const world::WorldModel& w, std::uint32_t u4_actor_id,
                             const world::TraitModel& trait) {
  if (u4_actor_id >= w.vec_actors_all.size()) return 0;
  const std::uint32_t u4_player{w.vec_actors_all[u4_actor_id].u4_owner};
  if (u4_player >= w.prod.vec_players.size()) return 0;
  const ProductionQueueState* ptr_queue{queue_state(w.prod.vec_players[u4_player], trait.str_param_type)};
  if (ptr_queue == nullptr) return 0;
  return (ptr_queue->b_enabled ? 1 : 0) ^ (ptr_queue->b_valid_faction ? 1 : 0);
}

std::int32_t power_manager_sync_hash(const world::WorldModel& w, std::uint32_t u4_actor_id,
                                     const world::TraitModel&) {
  if (u4_actor_id >= w.vec_actors_all.size()) return 0;
  const std::uint32_t u4_player{w.vec_actors_all[u4_actor_id].u4_owner};
  if (u4_player >= w.prod.vec_players.size()) return 0;
  const PowerManagerState& power{w.prod.vec_players[u4_player].power};
  return power.int4_power_provided ^ power.int4_power_drained;
}

std::int32_t player_resources_sync_hash(const world::WorldModel& w, std::uint32_t u4_actor_id,
                                        const world::TraitModel&) {
  if (u4_actor_id >= w.vec_actors_all.size()) return 0;
  const std::uint32_t u4_player{w.vec_actors_all[u4_actor_id].u4_owner};
  if (u4_player >= w.prod.vec_players.size()) return 0;
  const PlayerResourcesState& res{w.prod.vec_players[u4_player].res};
  return res.int4_cash ^ res.int4_resources ^ res.int4_capacity;
}

}

PowerState power_state(std::int32_t int4_provided, std::int32_t int4_drained) {
  if (int4_provided >= int4_drained) return PowerState::kNormal;
  if (int4_provided > int4_drained / 2) return PowerState::kLow;
  return PowerState::kCritical;
}

std::string_view power_state_name(PowerState state) {
  switch (state) {
    case PowerState::kNormal:
      return "normal";
    case PowerState::kLow:
      return "low";
    case PowerState::kCritical:
      return "critical";
  }
  return "normal";
}

std::int32_t apply_percentage_modifiers(std::int32_t int4_value,
                                        std::span<const std::int32_t> vec4_modifiers) {
  std::int64_t int8_numerator{int4_value};
  std::int64_t int8_denominator{1};
  for (const std::int32_t int4_modifier : vec4_modifiers) {
    int8_numerator *= int4_modifier;
    int8_denominator *= 100;
  }
  return static_cast<std::int32_t>(int8_numerator / int8_denominator);
}

bool take_cash(PlayerResourcesState& res, std::int32_t int4_num) {
  if (res.int4_resources + res.int4_cash < int4_num) return false;
  res.int4_resources -= int4_num;
  if (res.int4_resources < 0) {
    res.int4_cash += res.int4_resources;
    res.int4_resources = 0;
  }
  return true;
}

void refund_cash(PlayerResourcesState& res, std::int32_t int4_num) { res.int4_cash += int4_num; }

void refund_resources(PlayerResourcesState& res, std::int32_t int4_num) {
  res.int4_resources += int4_num;
  if (res.int4_resources > res.int4_capacity) res.int4_resources = res.int4_capacity;
}

std::string production_observation(const world::WorldModel& w, std::uint32_t u4_player) {
  if (u4_player >= w.prod.vec_players.size()) return {};
  const PlayerProductionState& state{w.prod.vec_players[u4_player]};

  std::string str_out;
  str_out += std::format("tick {} rng {} {} cash {} ore {} power {} {} {}\n", w.int4_world_tick,
                         w.rng_shared.int4_last, w.rng_shared.int4_total_count, state.res.int4_cash,
                         state.res.int4_resources, state.power.int4_power_provided,
                         state.power.int4_power_drained,
                         power_state_name(power_state(state.power.int4_power_provided,
                                                      state.power.int4_power_drained)));

  for (const ProductionQueueState& queue : state.vec_queues) {
    str_out += std::format("q {} en={}\n", queue.str_type, queue.b_enabled ? 1 : 0);
    for (const ProductionItemModel& item : queue.vec_queue)
      str_out += std::format("i {} rem={} rc={} slow={} st={} pz={} dn={} tt={} tc={} paid={}\n",
                             item.str_item, item.int4_remaining_time, item.int4_remaining_cost,
                             item.int4_slowdown, item.b_started ? 1 : 0, item.b_paused ? 1 : 0,
                             item.b_done ? 1 : 0, item.int4_total_time, item.int4_total_cost,
                             item.int4_resources_paid);
  }

  str_out += "actors";
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};
    str_out += std::format(" {}:{}", u4_id, actor.str_type);
    if (actor.u4_owner < w.vec_players.size())
      str_out += std::format(":{}", w.vec_players[actor.u4_owner].str_internal_name);
    else
      str_out += ":?";
  }
  str_out += '\n';
  return str_out;
}

std::vector<std::string> queue_buildable_items(const world::WorldModel& w,
                                               std::uint32_t u4_player,
                                               std::string_view str_queue_type) {
  std::vector<std::string> vec_items;
  const std::set<std::string> set_owned{owned_prerequisites(w, u4_player)};
  for (const world::ActorTypeDef& def : w.vec_actor_types) {
    const world::TraitTemplate* ptr_buildable{find_template(w, def.str_name_lower, "Buildable")};
    if (ptr_buildable == nullptr) continue;
    if (kv_find(*ptr_buildable, "Queue") != str_queue_type) continue;
    if (!prereqs_satisfied(set_owned, kv_find(*ptr_buildable, "Prerequisites"))) continue;
    vec_items.push_back(def.str_name_lower);
  }
  return vec_items;
}

void production_queue_state_build(world::WorldModel& w) {
  w.prod.vec_players.clear();
  w.prod.vec_players.resize(w.vec_players.size());
  for (std::size_t idx{0}; idx < w.vec_players.size(); ++idx) {
    const world::ActorModel& actor{w.vec_actors_all[w.vec_players[idx].u4_actor_id]};
    for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
      const world::TraitModel& trait{w.vec_traits[u4_handle]};
      if (trait.str_kind != "ClassicProductionQueue") continue;
      ProductionQueueState queue;
      queue.str_type = trait.str_param_type;
      queue.int4_low_power_modifier =
          trait.int4_param > 0 ? trait.int4_param : 100;
      queue.b_enabled = true;
      queue.b_valid_faction = true;
      for (const auto& [str_k, str_v] : trait.vec_kv) {
        if (str_k == "ReadyAudio") queue.str_ready_audio = str_v;
        if (str_k == "BlockedAudio") queue.str_blocked_audio = str_v;
      }
      w.prod.vec_players[idx].vec_queues.push_back(std::move(queue));
    }
    w.prod.vec_players[idx].res.int4_cash = w.lobby.int4_starting_cash;
  }
}

void production_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id) {
  if (u4_actor_id >= w.vec_actors_all.size()) return;
  const world::ActorModel& actor{w.vec_actors_all[u4_actor_id]};
  if (actor.u4_owner >= w.prod.vec_players.size()) return;

  std::int32_t int4_amount{0};
  bool b_has_power{false};
  for (const std::uint32_t u4_handle : actor.vec_trait_handles)
    if (w.vec_traits[u4_handle].str_kind == "Power") {
      int4_amount += w.vec_traits[u4_handle].int4_param;
      b_has_power = true;
    }
  if (!b_has_power) return;

  PowerManagerState& power{w.prod.vec_players[actor.u4_owner].power};
  auto it_old{power.map_power_drain.find(u4_actor_id)};
  const std::int32_t int4_old{it_old != power.map_power_drain.end() ? it_old->second : 0};
  power.map_power_drain[u4_actor_id] = int4_amount;
  if (int4_amount == int4_old) return;
  if (int4_old > 0)
    power.int4_power_provided -= int4_old;
  else if (int4_old < 0)
    power.int4_power_drained += int4_old;
  if (int4_amount > 0)
    power.int4_power_provided += int4_amount;
  else if (int4_amount < 0)
    power.int4_power_drained -= int4_amount;
}

void production_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id) {
  if (u4_actor_id >= w.vec_actors_all.size()) return;
  const world::ActorModel& actor{w.vec_actors_all[u4_actor_id]};
  if (actor.u4_owner >= w.prod.vec_players.size()) return;
  PowerManagerState& power{w.prod.vec_players[actor.u4_owner].power};
  const auto it{power.map_power_drain.find(u4_actor_id)};
  if (it == power.map_power_drain.end()) return;
  const std::int32_t int4_amount{it->second};
  power.map_power_drain.erase(it);
  if (int4_amount > 0)
    power.int4_power_provided -= int4_amount;
  else if (int4_amount < 0)
    power.int4_power_drained += int4_amount;
}

void production_frame_end_task(world::WorldModel& w, std::uint32_t u4_kind, std::uint32_t u4_player,
                               std::uint32_t u4_value, const std::string& str_item) {
  if (u4_player >= w.prod.vec_players.size()) return;
  PlayerProductionState& state{w.prod.vec_players[u4_player]};

  if (u4_kind == 1) {
    if (u4_value >= state.vec_queues.size()) return;
    ProductionQueueState& queue{state.vec_queues[u4_value]};
    bool b_done{false};
    for (const ProductionItemModel& item : queue.vec_queue)
      if (item.b_done && item.str_item == str_item) b_done = true;
    if (!b_done) return;

    const world::TraitTemplate* ptr_building{find_template(w, str_item, "Building")};
    if (ptr_building != nullptr) {
      for (ProductionItemModel& item : queue.vec_queue)
        if (item.b_done && item.str_item == str_item && !item.b_ready_notified) {
          item.b_ready_notified = true;
          if (w.ptr_sfx != nullptr && !queue.str_ready_audio.empty())
            sfx_on_production_ready(*w.ptr_sfx, u4_player, player_faction(w, u4_player),
                                         queue.str_ready_audio);
        }
      return;
    }
    if (produce_unit(w, u4_player, queue, str_item)) {
      for (std::size_t idx{queue.vec_queue.size()}; idx-- > 0;)
        if (queue.vec_queue[idx].b_done && queue.vec_queue[idx].str_item == str_item) {
          end_production(queue, idx);
          break;
        }
      if (w.ptr_sfx != nullptr && !queue.str_ready_audio.empty())
        sfx_on_production_ready(*w.ptr_sfx, u4_player, player_faction(w, u4_player),
                                     queue.str_ready_audio);
    } else if (w.ptr_sfx != nullptr && !queue.str_blocked_audio.empty()) {
      sfx_on_production_blocked(*w.ptr_sfx, u4_player, player_faction(w, u4_player),
                                     queue.str_blocked_audio);
    }
    return;
  }

  if (u4_kind != 2) return;

  const std::int32_t int4_bits{static_cast<std::int32_t>(u4_value)};
  const std::int32_t int4_x{static_cast<std::int32_t>(static_cast<std::uint32_t>(int4_bits) >> 20)};
  const std::int32_t int4_y{(static_cast<std::int16_t>(int4_bits >> 4)) >> 4};

  ProductionQueueState* ptr_queue{nullptr};
  ProductionItemModel* ptr_item{nullptr};
  std::size_t idx_item{0};
  for (ProductionQueueState& queue : state.vec_queues) {
    if (!actor_buildable(w, u4_player, str_item)) continue;
    for (std::size_t idx{0}; idx < queue.vec_queue.size(); ++idx) {
      if (queue.vec_queue[idx].b_done && queue.vec_queue[idx].str_item == str_item) {
        ptr_queue = &queue;
        ptr_item = &queue.vec_queue[idx];
        idx_item = idx;
        break;
      }
    }
    if (ptr_item != nullptr) break;
  }
  if (ptr_item == nullptr) return;

  if (!can_place_building(w, str_item, int4_x, int4_y)) return;
  if (!is_close_enough_to_base(w, u4_player, str_item, int4_x, int4_y)) return;

  auto res_building{world::create_actor(w, str_item, u4_player, int4_x, int4_y, true)};
  if (!res_building.has_value()) return;
  end_production(*ptr_queue, idx_item);
  if (w.ptr_sfx != nullptr) {
    std::vector<std::string> vec_build_sounds;
    if (const world::TraitTemplate* ptr_tpl{find_template(w, str_item, "Building")})
      for (const auto& [str_k, str_v] : ptr_tpl->vec_kv)
        if (str_k == "BuildSounds")
          for (const std::string_view str_part : split_list(str_v))
            vec_build_sounds.emplace_back(str_part);
    if (!vec_build_sounds.empty())
      sfx_on_building_placed(*w.ptr_sfx, u4_player, vec_build_sounds,
                                  ora::platforms::wpos{.int4_x = int4_x * 1024 + 512,
                                                       .int4_y = int4_y * 1024 + 512,
                                                       .int4_z = 0});
  }
}

const std::vector<world::TraitSpec>& production_trait_specs() {
  static const std::vector<world::TraitSpec> vec_specs{
      world::TraitSpec{.str_kind = "ClassicProductionQueue",
                       .b_implements_tick = true,
                       .b_implements_sync = true,
                       .b_implements_resolve = true,
                       .str_int_param_key = "LowPowerModifier",
                       .str_str_param_key = "Type",
                       .fn_tick = queue_tick,
                       .fn_sync_hash = queue_sync_hash,
                       .fn_resolve = queue_resolve},
      world::TraitSpec{.str_kind = "PlaceBuilding",
                       .b_implements_resolve = true,
                       .fn_resolve = place_building_resolve},
      world::TraitSpec{.str_kind = "PlayerResources",
                       .b_implements_sync = true,
                       .fn_sync_hash = player_resources_sync_hash},
      world::TraitSpec{.str_kind = "PowerManager",
                       .b_implements_sync = true,
                       .fn_sync_hash = power_manager_sync_hash},
      world::TraitSpec{.str_kind = "Production", .str_str_param_key = "Produces"},
      world::TraitSpec{.str_kind = "BaseProvider", .str_str_param_key = "Range"},
      world::TraitSpec{.str_kind = "GivesBuildableArea"},
      world::TraitSpec{.str_kind = "Building"},
      world::TraitSpec{.str_kind = "Power", .str_int_param_key = "Amount"},
      world::TraitSpec{.str_kind = "Exit",
                       .str_str_param_key = "ExitCell",
                       .str_str_param2_key = "ProductionTypes"},
      world::TraitSpec{.str_kind = "Valued", .str_int_param_key = "Cost"},
      world::TraitSpec{.str_kind = "WithInfantryBody",
                       .str_str_param_key = "IdleSequences"},
      world::TraitSpec{.str_kind = "Buildable",
                       .str_str_param_key = "Queue",
                       .str_str_param2_key = "BuildAtProductionType",
                       .str_str_param3_key = "Prerequisites"},
      world::TraitSpec{.str_kind = "ProvidesPrerequisite",
                       .str_str_param_key = "Prerequisite",
                       .str_str_param2_key = "Factions",
                       .str_str_param3_key = "RequiresPrerequisites"},
      world::TraitSpec{.str_kind = "ProvidesTechPrerequisite",
                       .str_str_param_key = "Prerequisites",
                       .str_str_param2_key = "Id"},
  };
  return vec_specs;
}

}
