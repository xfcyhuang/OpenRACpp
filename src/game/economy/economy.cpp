module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

namespace ora::game::economy {

namespace {

using pathfinding::CPos;
using pathfinding::LevelOrderQueue;
using pathfinding::LocomotorMap;
using pathfinding::WPos;

constexpr std::int32_t kPathCostInvalid{std::numeric_limits<std::int32_t>::max()};
constexpr std::uint8_t kStatusOpen{1};
constexpr std::uint8_t kStatusClosed{2};

struct CVec2 {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
};

constexpr CVec2 kDirections8[]{
    CVec2{-1, -1}, CVec2{-1, 0}, CVec2{-1, 1}, CVec2{0, -1},
    CVec2{0, 1},   CVec2{1, -1}, CVec2{1, 0},  CVec2{1, 1},
};

constexpr CVec2 kDirectedNeighbors[][5]{
    {CVec2{-1, -1}, CVec2{0, -1}, CVec2{1, -1}, CVec2{-1, 0}, CVec2{-1, 1}},
    {CVec2{-1, -1}, CVec2{0, -1}, CVec2{1, -1}, CVec2{0, 0}, CVec2{0, 0}},
    {CVec2{-1, -1}, CVec2{0, -1}, CVec2{1, -1}, CVec2{1, 0}, CVec2{1, 1}},
    {CVec2{-1, -1}, CVec2{-1, 0}, CVec2{-1, 1}, CVec2{0, 0}, CVec2{0, 0}},
    {CVec2{0, 0}, CVec2{0, 0}, CVec2{0, 0}, CVec2{0, 0}, CVec2{0, 0}},
    {CVec2{1, -1}, CVec2{1, 0}, CVec2{1, 1}, CVec2{0, 0}, CVec2{0, 0}},
    {CVec2{-1, -1}, CVec2{-1, 0}, CVec2{-1, 1}, CVec2{0, 1}, CVec2{1, 1}},
    {CVec2{-1, 1}, CVec2{0, 1}, CVec2{1, 1}, CVec2{0, 0}, CVec2{0, 0}},
    {CVec2{1, -1}, CVec2{1, 0}, CVec2{-1, 1}, CVec2{0, 1}, CVec2{1, 1}},
};

constexpr std::size_t kDirectedNeighborsCount[]{5, 3, 5, 3, 0, 3, 5, 3, 5};
constexpr std::int32_t kSqrtTwoNum{46341};
constexpr std::int32_t kSqrtTwoDen{32768};

const MiniYamlNode* find_child(const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const char* ptr_first{str_value.data()};
  const char* ptr_last{str_value.data() + str_value.size()};
  const auto res{std::from_chars(ptr_first, ptr_last, int4_out)};
  if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
  return int4_out;
}

std::optional<std::int32_t> parse_int_kv(const world::TraitTemplate& tpl, std::string_view str_key) {
  for (const auto& [str_k, str_v] : tpl.vec_kv)
    if (str_k == str_key) return parse_int(str_v);
  return std::nullopt;
}

std::string_view kv_find(const world::TraitTemplate& tpl, std::string_view str_key) {
  for (const auto& [str_k, str_v] : tpl.vec_kv)
    if (str_k == str_key) return str_v;
  return {};
}

std::optional<std::int32_t> parse_int_node(const MiniYamlNode& node_parent, std::string_view str_key) {
  const MiniYamlNode* node{find_child(node_parent.vec_children, str_key)};
  if (node == nullptr) return std::nullopt;
  return parse_int(node->str_value);
}

std::vector<std::string_view> split_list(std::string_view str_value) {
  std::vector<std::string_view> vec_out;
  std::size_t idx_start{0};
  while (idx_start <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', idx_start)};
    const std::string_view str_part{str_value.substr(
        idx_start, idx_comma == std::string_view::npos ? std::string_view::npos : idx_comma - idx_start)};
    std::size_t idx_first{0};
    std::size_t idx_last{str_part.size()};
    while (idx_first < idx_last && str_part[idx_first] == ' ') ++idx_first;
    while (idx_last > idx_first && str_part[str_part.size() - 1] == ' ') --idx_last;
    if (idx_last > idx_first) vec_out.push_back(str_part.substr(idx_first, idx_last - idx_first));
    if (idx_comma == std::string_view::npos) break;
    idx_start = idx_comma + 1;
  }
  return vec_out;
}

std::size_t cell_index_map(const LocomotorMap& map, CPos cell) {
  return static_cast<std::size_t>(cell.int4_x) * static_cast<std::size_t>(map.int4_height) +
         static_cast<std::size_t>(cell.int4_y);
}

bool cell_in_bounds(const LocomotorMap& map, CPos cell) {
  return cell.int4_x >= 0 && cell.int4_x < map.int4_width && cell.int4_y >= 0 &&
         cell.int4_y < map.int4_height;
}

const world::TraitTemplate* find_world_template(const world::WorldModel& w, std::string_view str_type,
                                                std::string_view str_kind) {
  for (const world::ActorTypeDef& def : w.vec_actor_types) {
    if (def.str_name_lower != str_type) continue;
    for (const world::TraitTemplate& tpl : def.vec_templates)
      if (tpl.str_kind == str_kind) return &tpl;
    return nullptr;
  }
  return nullptr;
}

[[nodiscard]] std::int32_t econ_view_id(const world::WorldModel& w, std::uint32_t u4_actor) {
  if (u4_actor >= w.vec_actors_all.size()) return -1;
  const world::ActorModel& actor{w.vec_actors_all[u4_actor]};

  std::int32_t int4_next{1};
  for (const world::PlayerModel& player : w.vec_players) {
    if (player.u4_actor_id == u4_actor) {
      if (player.b_spectating) return -1;
      return int4_next;
    }
    if (!player.b_spectating) ++int4_next;
  }

  if (actor.str_type == "world") return 0;
  if (actor.str_type == "player") return -1;

  for (std::size_t idx{0}; idx < w.vec_actors_all.size(); ++idx) {
    const std::string& str_type{w.vec_actors_all[idx].str_type};
    if (str_type == "world" || str_type == "player") continue;
    if (static_cast<std::uint32_t>(idx) == u4_actor) return int4_next;
    ++int4_next;
  }
  return -1;
}

bool terrain_type_allowed(const ResourceTypeSpec& spec, std::string_view str_terrain) {
  for (const std::string& str_allowed : spec.vec_allowed_terrain)
    if (str_allowed == str_terrain) return true;
  return false;
}

bool allow_resource_at(const LocomotorMap& map, const Tileset& tileset,
                       const MapBin& bin_map, const ResourceTypeSpec& spec, CPos cell) {
  if (!pathfinding::map_contains(map, cell)) return false;
  if (!cell_in_bounds(map, cell)) return false;
  const std::uint8_t u1_terrain{tileset.terrain_index_of_tile(bin_map.tile_at(cell.int4_x, cell.int4_y))};
  return terrain_type_allowed(spec, tileset.vec_str_types[u1_terrain]);
}

void resource_write_cell_cost(EconomyWorldState& econ, const ResourceTypeSpec& spec, CPos cell) {
  LocomotorMap& map{econ.map_move};
  if (!cell_in_bounds(map, cell)) return;
  const std::size_t idx{cell_index_map(map, cell)};
  econ.map_cost_base.try_emplace({cell.int4_x, cell.int4_y}, map.vec2_cost[idx], map.vec4_speed[idx]);
  std::int16_t int2_cost{pathfinding::kCostUnreachable};
  std::int32_t int4_speed{0};
  for (const pathfinding::LocomotorTerrainSpeed& speed : econ.cfg.vec_locomotor_speeds)
    if (speed.str_type == spec.str_terrain) {
      int2_cost = static_cast<std::int16_t>(speed.int4_cost);
      int4_speed = speed.int4_speed;
    }
  map.vec2_cost[idx] = int2_cost;
  map.vec4_speed[idx] = int4_speed;
}

void resource_restore_cell_cost(EconomyWorldState& econ, CPos cell) {
  LocomotorMap& map{econ.map_move};
  if (!cell_in_bounds(map, cell)) return;
  const std::size_t idx{cell_index_map(map, cell)};
  const auto it{econ.map_cost_base.find({cell.int4_x, cell.int4_y})};
  if (it == econ.map_cost_base.end()) return;
  map.vec2_cost[idx] = it->second.first;
  map.vec4_speed[idx] = it->second.second;
}

void create_resource_cell(ResourceLayerState& layer, EconomyWorldState& econ, const ResourceTypeSpec& spec,
                          CPos cell, std::int32_t int4_density) {
  const std::size_t idx{cell_index_map(econ.map_move, cell)};
  layer.vec_cells[idx].str_type = spec.str_name;
  layer.vec_cells[idx].u1_density =
      static_cast<std::uint8_t>(std::clamp(int4_density, 1, spec.int4_max_density));
  resource_write_cell_cost(econ, spec, cell);
  ++layer.int4_res_cells;
}

struct SearchState {
  const LocomotorMap* ptr_map{};
  CPos cell_target{};
  std::int32_t int4_weight{};
  bool b_lane_bias{true};
  bool b_predicate{};
  std::int32_t int4_cell_cost{};
  std::int32_t int4_diag_cost{};
  const std::set<std::pair<std::int32_t, std::int32_t>>* ptr_set_blocked{};
  const std::function<std::int32_t(CPos)>* ptr_fn_custom_cost{};
  const std::function<bool(CPos)>* ptr_fn_target{};
  std::vector<pathfinding::CellInfoNode> vec_cells;
  LevelOrderQueue queue;
};

std::int32_t heuristic_cost(const SearchState& state, CPos cell) {
  if (state.b_predicate) return 0;
  const std::int32_t int4_dx{cell.int4_x > state.cell_target.int4_x
                                 ? cell.int4_x - state.cell_target.int4_x
                                 : state.cell_target.int4_x - cell.int4_x};
  const std::int32_t int4_dy{cell.int4_y > state.cell_target.int4_y
                                 ? cell.int4_y - state.cell_target.int4_y
                                 : state.cell_target.int4_y - cell.int4_y};
  const std::int32_t int4_diag{int4_dx < int4_dy ? int4_dx : int4_dy};
  const std::int32_t int4_straight{int4_dx + int4_dy};
  return state.int4_cell_cost * int4_straight +
         (state.int4_diag_cost - 2 * state.int4_cell_cost) * int4_diag;
}

std::int32_t path_cost_to_node(SearchState& state, CPos cell_dest, CVec2 vec_dir) {
  if (state.ptr_set_blocked != nullptr &&
      state.ptr_set_blocked->contains({cell_dest.int4_x, cell_dest.int4_y}))
    return kPathCostInvalid;
  const std::int16_t int2_cost{pathfinding::movement_cost_for_cell(*state.ptr_map, cell_dest)};
  if (int2_cost == pathfinding::kCostUnreachable) return kPathCostInvalid;

  std::int32_t int4_cost{vec_dir.int4_x * vec_dir.int4_y != 0 ? int2_cost * kSqrtTwoNum / kSqrtTwoDen
                                                              : int2_cost};

  if (state.ptr_fn_custom_cost != nullptr) {
    const std::int32_t int4_custom{(*state.ptr_fn_custom_cost)(cell_dest)};
    if (int4_custom == kPathCostInvalid) return kPathCostInvalid;
    int4_cost += int4_custom;
  }

  if (state.b_lane_bias) {
    const std::int32_t int4_ux{cell_dest.int4_x & 1};
    const std::int32_t int4_uy{cell_dest.int4_y & 1};
    if ((int4_ux == 0 && vec_dir.int4_y < 0) || (int4_ux == 1 && vec_dir.int4_y > 0)) ++int4_cost;
    if ((int4_uy == 0 && vec_dir.int4_x < 0) || (int4_uy == 1 && vec_dir.int4_x > 0)) ++int4_cost;
  }

  return int4_cost;
}

std::vector<pathfinding::GraphConnection> get_connections(SearchState& state, CPos cell_position) {
  const pathfinding::CellInfoNode& info{state.vec_cells[cell_index_map(*state.ptr_map, cell_position)]};
  const std::int32_t int4_dx{cell_position.int4_x - info.cell_prev.int4_x};
  const std::int32_t int4_dy{cell_position.int4_y - info.cell_prev.int4_y};
  const std::int32_t int4_index{int4_dy * 3 + int4_dx + 4};
  const CVec2* vec_dirs{kDirections8};
  std::size_t sz_count{8};
  if (int4_index != 4) {
    vec_dirs = kDirectedNeighbors[int4_index];
    sz_count = kDirectedNeighborsCount[int4_index];
  }

  std::vector<pathfinding::GraphConnection> vec_connections;
  vec_connections.reserve(sz_count);
  for (std::size_t idx{0}; idx < sz_count; ++idx) {
    const CVec2 vec_dir{vec_dirs[idx]};
    const CPos cell_neighbor{cell_position.int4_x + vec_dir.int4_x, cell_position.int4_y + vec_dir.int4_y};
    if (!cell_in_bounds(*state.ptr_map, cell_neighbor)) continue;
    const std::int32_t int4_cost{path_cost_to_node(state, cell_neighbor, vec_dir)};
    if (int4_cost != kPathCostInvalid &&
        state.vec_cells[cell_index_map(*state.ptr_map, cell_neighbor)].u1_status != kStatusClosed)
      vec_connections.push_back(pathfinding::GraphConnection{cell_neighbor, int4_cost});
  }
  return vec_connections;
}

void add_initial_cell(SearchState& state, CPos cell) {
  std::int32_t int4_initial_cost{0};
  if (state.ptr_fn_custom_cost != nullptr) {
    int4_initial_cost = (*state.ptr_fn_custom_cost)(cell);
    if (int4_initial_cost == kPathCostInvalid) return;
  }
  const std::int32_t int4_estimated{heuristic_cost(state, cell) * state.int4_weight / 100};
  pathfinding::CellInfoNode& info{state.vec_cells[cell_index_map(*state.ptr_map, cell)]};
  info.u1_status = kStatusOpen;
  info.int4_cost_so_far = int4_initial_cost;
  info.int4_est_total = int4_initial_cost + int4_estimated;
  info.cell_prev = cell;
  pathfinding::queue_add(state.queue, pathfinding::GraphConnection{cell, int4_estimated});
}

bool can_expand(SearchState& state) {
  while (true) {
    if (pathfinding::queue_empty(state.queue)) return false;
    if (state.vec_cells[cell_index_map(*state.ptr_map, state.queue.vec_items[0].cell)].u1_status ==
        kStatusClosed) {
      pathfinding::queue_pop(state.queue);
      continue;
    }
    return true;
  }
}

CPos expand(SearchState& state) {
  const CPos cell_current{pathfinding::queue_pop(state.queue).cell};
  pathfinding::CellInfoNode& info_current{state.vec_cells[cell_index_map(*state.ptr_map, cell_current)]};
  const std::int32_t int4_cost_so_far{info_current.int4_cost_so_far};
  info_current.u1_status = kStatusClosed;

  for (const pathfinding::GraphConnection& conn : get_connections(state, cell_current)) {
    const std::int32_t int4_cost_to{int4_cost_so_far + conn.int4_cost};
    pathfinding::CellInfoNode& info{state.vec_cells[cell_index_map(*state.ptr_map, conn.cell)]};

    if (info.u1_status == kStatusClosed ||
        (info.u1_status == kStatusOpen && int4_cost_to >= info.int4_cost_so_far))
      continue;

    std::int32_t int4_remaining{};
    if (info.u1_status == kStatusOpen)
      int4_remaining = info.int4_est_total - info.int4_cost_so_far;
    else
      int4_remaining = heuristic_cost(state, conn.cell) * state.int4_weight / 100;

    info.u1_status = kStatusOpen;
    info.int4_cost_so_far = int4_cost_to;
    info.int4_est_total = int4_cost_to + int4_remaining;
    info.cell_prev = cell_current;
    pathfinding::queue_add(state.queue,
                           pathfinding::GraphConnection{conn.cell, int4_cost_to + int4_remaining});
  }

  return cell_current;
}

std::vector<CPos> make_path(const SearchState& state, CPos cell_end) {
  std::vector<CPos> vec_path;
  CPos cell_node{cell_end};
  while (true) {
    vec_path.push_back(cell_node);
    const pathfinding::CellInfoNode& info{state.vec_cells[cell_index_map(*state.ptr_map, cell_node)]};
    if (info.cell_prev.int4_x == cell_node.int4_x && info.cell_prev.int4_y == cell_node.int4_y) break;
    cell_node = info.cell_prev;
  }
  return vec_path;
}

SearchState search_prepare(const EconomyWorldState& econ) {
  SearchState state;
  state.ptr_map = &econ.map_move;
  state.int4_cell_cost = econ.map_move.int4_min_cost;
  state.int4_diag_cost = econ.map_move.int4_min_cost * kSqrtTwoNum / kSqrtTwoDen;
  state.vec_cells.assign(
      static_cast<std::size_t>(econ.map_move.int4_width) * static_cast<std::size_t>(econ.map_move.int4_height),
      pathfinding::CellInfoNode{});
  return state;
}

std::optional<CPos> search_closest_harvestable(
    const EconomyWorldState& econ, const std::vector<CPos>& vec_sources,
    const std::set<std::pair<std::int32_t, std::int32_t>>& set_blocked,
    const std::function<std::int32_t(CPos)>& fn_custom_cost,
    const std::function<bool(CPos)>& fn_target) {
  SearchState state{search_prepare(econ)};
  state.b_predicate = true;
  state.int4_weight = 0;
  state.ptr_set_blocked = &set_blocked;
  state.ptr_fn_custom_cost = &fn_custom_cost;
  state.ptr_fn_target = &fn_target;

  for (const CPos cell : vec_sources)
    if (pathfinding::map_contains(econ.map_move, cell) &&
        set_blocked.find({cell.int4_x, cell.int4_y}) == set_blocked.end())
      add_initial_cell(state, cell);

  while (can_expand(state)) {
    const CPos cell{expand(state)};
    if (fn_target(cell)) return make_path(state, cell).front();
  }
  return std::nullopt;
}

HarvState* harv_state_of(EconomyWorldState& econ, std::uint32_t u4_actor) {
  for (HarvState& harv : econ.vec_harvs)
    if (harv.u4_actor == u4_actor) return &harv;
  return nullptr;
}

const HarvState* harv_state_of(const EconomyWorldState& econ, std::uint32_t u4_actor) {
  for (const HarvState& harv : econ.vec_harvs)
    if (harv.u4_actor == u4_actor) return &harv;
  return nullptr;
}

DockHostState* dock_host_of(EconomyWorldState& econ, std::uint32_t u4_actor) {
  for (DockHostState& host : econ.vec_hosts)
    if (host.u4_actor == u4_actor) return &host;
  return nullptr;
}

bool can_harvest_cell(const ResourceLayerState& layer, const HarvCargoState& cargo, CPos cell) {
  if (cell.int4_x < 0 || cell.int4_x >= layer.int4_width || cell.int4_y < 0 ||
      cell.int4_y >= layer.int4_height)
    return false;
  const ResourceCellState& content{
      layer.vec_cells[static_cast<std::size_t>(cell.int4_x) * static_cast<std::size_t>(layer.int4_height) +
                      static_cast<std::size_t>(cell.int4_y)]};
  if (content.str_type.empty()) return false;
  for (const auto& [str_key, int4_count] : cargo.vec_contents)
    if (str_key == content.str_type) return true;
  return false;
}

std::set<std::pair<std::int32_t, std::int32_t>> blocked_cells_build(
    const world::WorldModel& w, const EconomyWorldState& econ, std::uint32_t u4_ignore_actor) {
  std::set<std::pair<std::int32_t, std::int32_t>> set_blocked;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    if (u4_id == u4_ignore_actor) continue;
    const world::ActorModel& actor{w.vec_actors_all[u4_id]};

    if (find_world_template(w, actor.str_type, "Building") != nullptr) {
      const std::string_view str_dims{kv_find(*find_world_template(w, actor.str_type, "Building"),
                                              "Dimensions")};
      const std::string_view str_fp{
          kv_find(*find_world_template(w, actor.str_type, "Building"), "Footprint")};
      const std::vector<std::string_view> vec_dims{split_list(str_dims)};
      if (vec_dims.size() != 2) continue;
      const auto opt_dx{parse_int(vec_dims[0])};
      const auto opt_dy{parse_int(vec_dims[1])};
      if (!opt_dx.has_value() || !opt_dy.has_value()) continue;

      std::size_t idx_row{0};
      std::size_t idx_start{0};
      while (idx_start <= str_fp.size() && idx_row < static_cast<std::size_t>(*opt_dy)) {
        const std::size_t idx_space{str_fp.find(' ', idx_start)};
        const std::string_view str_row{str_fp.substr(
            idx_start, idx_space == std::string_view::npos ? std::string_view::npos : idx_space - idx_start)};
        for (std::int32_t int4_dx{0}; int4_dx < *opt_dx && static_cast<std::size_t>(int4_dx) < str_row.size();
             ++int4_dx) {
          const char ch_cell{str_row[static_cast<std::size_t>(int4_dx)]};

          if (ch_cell == '_' || ch_cell == '=') continue;
          set_blocked.insert({actor.int4_pos_x + int4_dx, actor.int4_pos_y + static_cast<std::int32_t>(idx_row)});
        }
        if (idx_space == std::string_view::npos) break;
        idx_start = idx_space + 1;
        ++idx_row;
      }
      continue;
    }

    if (const HarvState* ptr_harv{harv_state_of(econ, u4_id)}) {
      set_blocked.insert({ptr_harv->move.cell_to.int4_x, ptr_harv->move.cell_to.int4_y});
      continue;
    }
    set_blocked.insert({actor.int4_pos_x, actor.int4_pos_y});
  }
  return set_blocked;
}

std::int32_t wrap32(std::int64_t int8_value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(int8_value & 0xffffffff));
}

std::optional<CPos> closest_harvestable_pos(world::WorldModel& w, HarvState& harv) {
  EconomyWorldState& econ{w.econ};
  CPos cell_search_from{};
  std::int32_t int4_radius{};
  const bool b_has_dock{harv.u4_last_reserved_host != kNoActor};
  if (harv.opt_order_loc.has_value()) {
    // Harvest 订单注入的 loc（FindAndDeliverResources(self, loc) 首轮搜索中心，
    // Harvester.cs:276）：首轮优先并消费（上游 FDA ctor 一次性参数）。
    int4_radius = econ.cfg.int4_search_from_harvester_radius;
    cell_search_from = *harv.opt_order_loc;
    harv.opt_order_loc.reset();
  } else if (harv.opt_last_harvested.has_value()) {
    int4_radius = econ.cfg.int4_search_from_harvester_radius;
    cell_search_from = *harv.opt_last_harvested;
  } else {
    int4_radius = econ.cfg.int4_search_from_proc_radius;
    cell_search_from = b_has_dock ? econ.cfg.cell_dock : harv.move.cell_from;
  }
  const std::int32_t int4_radius_squared{int4_radius * int4_radius};
  const WPos pos_dock{econ.cfg.pos_dock};
  const WPos pos_harv{harv.move.pos_center};
  const std::int32_t int4_penalty{econ.cfg.int4_refinery_penalty};
  const ResourceLayerState& layer{econ.layer};
  const HarvCargoState& cargo{harv.cargo};

  const auto fn_custom_cost{[&](CPos cell) -> std::int32_t {
    const std::int32_t int4_dx{cell.int4_x - cell_search_from.int4_x};
    const std::int32_t int4_dy{cell.int4_y - cell_search_from.int4_y};
    if (int4_dx * int4_dx + int4_dy * int4_dy > int4_radius_squared) return kPathCostInvalid;
    if (b_has_dock && int4_penalty > 0 && can_harvest_cell(layer, cargo, cell)) {
      const WPos pos_cell{center_of_cell(cell)};
      const std::int64_t int8_bx{pos_cell.int4_x - pos_dock.int4_x};
      const std::int64_t int8_by{pos_cell.int4_y - pos_dock.int4_y};
      const std::int64_t int8_cx{pos_cell.int4_x - pos_harv.int4_x};
      const std::int64_t int8_cy{pos_cell.int4_y - pos_harv.int4_y};
      if (int8_bx != 0 || int8_by != 0) {
        if (int8_cx != 0 || int8_cy != 0) {
          const std::int64_t int8_a2{static_cast<std::int64_t>(pos_harv.int4_x - pos_dock.int4_x) *
                                     (pos_harv.int4_x - pos_dock.int4_x) +
                                     static_cast<std::int64_t>(pos_harv.int4_y - pos_dock.int4_y) *
                                         (pos_harv.int4_y - pos_dock.int4_y)};
          const std::int32_t int4_b2{wrap32(int8_bx * int8_bx + int8_by * int8_by)};
          const std::int32_t int4_c2{wrap32(int8_cx * int8_cx + int8_cy * int8_cy)};
          const std::int32_t int4_a2{wrap32(int8_a2)};
          const std::int32_t int4_bl{static_cast<std::int32_t>(pathfinding::isqrt64(int8_bx * int8_bx + int8_by * int8_by))};
          const std::int32_t int4_cl{static_cast<std::int32_t>(pathfinding::isqrt64(int8_cx * int8_cx + int8_cy * int8_cy))};
          const std::int32_t int4_cos{wrap32(static_cast<std::int64_t>(512) * wrap32(
                                           static_cast<std::int64_t>(int4_b2) + int4_c2 - int4_a2)) /
                                       int4_bl / int4_cl};
          return std::abs(int4_penalty / 2) + int4_penalty * int4_cos / 2048;
        }
      }
    }
    return 0;
  }};

  const auto fn_target{[&](CPos cell) { return can_harvest_cell(layer, cargo, cell); }};

  const std::set<std::pair<std::int32_t, std::int32_t>> set_blocked{
      blocked_cells_build(w, econ, harv.u4_actor)};
  const std::vector<CPos> vec_sources{cell_search_from, harv.move.cell_from};
  return search_closest_harvestable(econ, vec_sources, set_blocked, fn_custom_cost, fn_target);
}

std::int32_t act_alloc(HarvState& harv, ActKind kind) {
  HarvActivity act;
  act.kind = kind;
  harv.vec_activities.push_back(std::move(act));
  return static_cast<std::int32_t>(harv.vec_activities.size()) - 1;
}

void act_queue_child(HarvState& harv, std::int32_t int4_parent, std::int32_t int4_child) {
  HarvActivity& parent{harv.vec_activities[static_cast<std::size_t>(int4_parent)]};
  if (parent.int4_child == -1) {
    parent.int4_child = int4_child;
    return;
  }
  std::int32_t int4_tail{parent.int4_child};
  while (harv.vec_activities[static_cast<std::size_t>(int4_tail)].int4_next != -1)
    int4_tail = harv.vec_activities[static_cast<std::size_t>(int4_tail)].int4_next;
  harv.vec_activities[static_cast<std::size_t>(int4_tail)].int4_next = int4_child;
}

void queue_child_fresh(HarvState& harv, std::int32_t int4_parent, ActKind kind, CPos cell_target,
                       std::int32_t int4_wait) {
  const std::int32_t int4_child{act_alloc(harv, kind)};
  HarvActivity& act_child{harv.vec_activities[static_cast<std::size_t>(int4_child)]};
  act_child.cell_target = cell_target;
  act_child.int4_wait_remaining = int4_wait;
  act_queue_child(harv, int4_parent, int4_child);
}

bool on_dock_tick(world::WorldModel& w, HarvState& harv) {
  EconomyWorldState& econ{w.econ};
  if (--harv.int4_unload_cooldown > 0) return false;
  const std::uint32_t u4_player{w.vec_actors_all[harv.u4_actor].u4_owner};
  for (auto& [str_type, int4_count] : harv.cargo.vec_contents) {
    const std::int32_t int4_take{std::min(int4_count, econ.cfg.int4_bale_unload_amount)};
    const std::int32_t int4_accepted{refinery_accept(w, u4_player, str_type, int4_take)};
    if (int4_accepted == 0) continue;
    int4_count -= int4_accepted;
    harv.int4_unload_cooldown = econ.cfg.int4_bale_unload_delay;
    return false;
  }
  return harv_is_empty(harv);
}

void unreserve_host(world::WorldModel& w, HarvState& harv) {
  EconomyWorldState& econ{w.econ};
  if (harv.u4_reserved_host == kNoActor) return;
  if (DockHostState* ptr_host{dock_host_of(econ, harv.u4_reserved_host)}) {
    const auto it{std::find(ptr_host->vec_reserved.begin(), ptr_host->vec_reserved.end(), harv.u4_actor)};
    if (it != ptr_host->vec_reserved.end()) ptr_host->vec_reserved.erase(it);
  }
  harv.u4_last_reserved_host = harv.u4_reserved_host;
  harv.u4_reserved_host = kNoActor;
}

bool act_tick(world::WorldModel& w, HarvState& harv, std::int32_t int4_idx) {
  EconomyWorldState& econ{w.econ};
  HarvActivity& act{harv.vec_activities[static_cast<std::size_t>(int4_idx)]};

  switch (act.kind) {
    case ActKind::kFindAndDeliver: {
      if (harv_is_full(econ, harv)) {
        if (harv.u4_reserved_host != kNoActor) return false;
        queue_child_fresh(harv, int4_idx, ActKind::kMoveToDock, CPos{}, 0);
        harv.b_has_delivered_load = true;
      }
      if (harv.b_last_search_failed && !harv.b_has_waited) {
        queue_child_fresh(harv, int4_idx, ActKind::kWait, CPos{}, econ.cfg.int4_wait_duration);
        harv.b_has_waited = true;
        return false;
      }
      harv.b_has_waited = false;

      std::optional<CPos> opt_cell{closest_harvestable_pos(w, harv)};
      if (!opt_cell.has_value()) {
        if (harv.opt_last_harvested.has_value()) {
          harv.opt_last_harvested = std::nullopt;
          opt_cell = closest_harvestable_pos(w, harv);
          harv.b_last_search_failed = !opt_cell.has_value();
        } else {
          harv.b_last_search_failed = true;
        }
      } else {
        harv.b_last_search_failed = false;
      }

      if (harv.b_last_search_failed) return false;

      queue_child_fresh(harv, int4_idx, ActKind::kHarvestResource, *opt_cell, 0);
      harv.opt_last_harvested = *opt_cell;
      harv.b_has_harvested_cell = true;
      return false;
    }

    case ActKind::kHarvestResource: {
      if (harv_is_full(econ, harv)) return true;
      if (harv.move.cell_from != act.cell_target) {
        queue_child_fresh(harv, int4_idx, ActKind::kMove, act.cell_target, 0);
        return false;
      }
      if (!can_harvest_cell(econ.layer, harv.cargo, harv.move.cell_from)) return true;

      const std::int32_t int4_step{1024 / std::max(1, econ.cfg.int4_harvest_facings)};
      const std::int32_t int4_desired{harv.move.int4_facing / int4_step * int4_step};
      if (int4_desired != harv.move.int4_facing) {
        queue_child_fresh(harv, int4_idx, ActKind::kTurn, CPos{int4_desired, 0}, 0);
        return false;
      }

      const std::string str_type{resource_get(econ.layer, harv.move.cell_from).str_type};
      if (str_type.empty() ||
          resource_remove(econ.layer, econ, str_type, harv.move.cell_from) != 1)
        return true;
      cargo_add(harv.cargo, str_type, 1);
      queue_child_fresh(harv, int4_idx, ActKind::kWait, CPos{}, econ.cfg.int4_bale_load_delay);
      return false;
    }

    case ActKind::kMoveToDock: {
      if (act.int4_dock_state == 0) {
        bool b_found{false};
        for (const std::uint32_t u4_id : w.vec_in_world) {
          if (dock_host_of(econ, u4_id) == nullptr) continue;
          if (w.vec_actors_all[u4_id].u4_owner != w.vec_actors_all[harv.u4_actor].u4_owner) continue;
          act.int4_dock_state = static_cast<std::int32_t>(u4_id);
          b_found = true;
          break;
        }
        if (!b_found) {
          queue_child_fresh(harv, int4_idx, ActKind::kWait, CPos{}, econ.cfg.int4_search_for_dock_delay);
          return false;
        }
      }

      const std::uint32_t u4_host{static_cast<std::uint32_t>(act.int4_dock_state)};
      DockHostState* ptr_host{dock_host_of(econ, u4_host)};
      bool b_reserved{ptr_host != nullptr};
      if (b_reserved && harv.u4_reserved_host != u4_host) {
        if (static_cast<std::int32_t>(ptr_host->vec_reserved.size()) >= econ.cfg.int4_max_queue_length)
          b_reserved = false;
        else {
          unreserve_host(w, harv);
          ptr_host->vec_reserved.push_back(harv.u4_actor);
          harv.u4_reserved_host = u4_host;
          harv.u4_last_reserved_host = kNoActor;
        }
      }

      if (b_reserved) {
        const bool b_at_dock{harv.move.cell_from == econ.cfg.cell_dock &&
                             harv.move.int4_facing == econ.cfg.int4_dock_angle};
        if (!b_at_dock) {
          queue_child_fresh(harv, int4_idx, ActKind::kMoveOntoAndTurn, econ.cfg.cell_dock, 0);
          return false;
        }
        queue_child_fresh(harv, int4_idx, ActKind::kGenericDock, CPos{}, 0);
        return true;
      }

      queue_child_fresh(harv, int4_idx, ActKind::kWait, CPos{}, econ.cfg.int4_search_for_dock_delay);
      return false;
    }

    case ActKind::kMoveOntoAndTurn: {
      if (harv.move.int4_facing != econ.cfg.int4_dock_angle) {
        queue_child_fresh(harv, int4_idx, ActKind::kTurn, CPos{econ.cfg.int4_dock_angle, 0}, 0);
        return false;
      }
      return true;
    }

    case ActKind::kGenericDock: {
      switch (act.int4_dock_state) {
        case 0: {
          act.int4_dock_state = 1;
          return false;
        }
        case 1: {
          if (!act.b_dock_initiated) {
            act.b_dock_initiated = true;
            if (DockHostState* ptr_host{dock_host_of(econ, harv.u4_reserved_host)})
              ptr_host->u4_docked_client = harv.u4_actor;
            act.int4_dock_frames = econ.cfg.int4_dock_frames;
            return false;
          }
          --act.int4_dock_frames;
          if (act.int4_dock_frames > 0) return false;
          act.int4_dock_state = 2;
          [[fallthrough]];
        }
        case 2: {
          if (on_dock_tick(w, harv)) act.int4_dock_state = 3;
          return false;
        }
        case 3: {
          if (act.int4_dock_frames == 0 && !act.b_finishing) {
            act.int4_dock_frames = econ.cfg.int4_dock_frames;
            return false;
          }
          --act.int4_dock_frames;
          if (act.int4_dock_frames > 0) return false;
          act.int4_dock_state = 4;
          [[fallthrough]];
        }
        case 4: {
          if (DockHostState* ptr_host{dock_host_of(econ, harv.u4_reserved_host)}) {
            if (ptr_host->u4_docked_client == harv.u4_actor) ptr_host->u4_docked_client = kNoActor;
          }
          unreserve_host(w, harv);
          return true;
        }
      }
      return false;
    }

    case ActKind::kWait:
      return act.int4_wait_remaining-- == 0;

    case ActKind::kMove:
    case ActKind::kMoveReverse: {

      const std::int32_t int4_modifier{
          100 - (100 - econ.cfg.int4_fully_loaded_speed) * harv_fullness(harv) / 100};
      pathfinding::MoveSimConfig cfg_move;
      cfg_move.ptr_map = &econ.map_move;
      cfg_move.int4_speed = econ.cfg.int4_harv_speed * int4_modifier;
      cfg_move.int4_speed_den = 10000;
      cfg_move.int4_turn_speed = econ.cfg.int4_harv_turn_speed;
      cfg_move.int4_heuristic_weight = 125;
      cfg_move.ptr_set_blocked = &harv.set_blocked_move;
      cfg_move.b_reverse_path = act.kind == ActKind::kMoveReverse;
      pathfinding::move_sim_tick(harv.move, cfg_move, w.int4_world_tick);
      if (harv.move.b_has_activity) return false;
      harv.move_outcome = MoveOutcome::kCompleteDestinationReached;
      return true;
    }

    case ActKind::kTurn: {
      if (harv.move.int4_facing == act.cell_target.int4_x) return true;
      harv.move.int4_facing = pathfinding::tick_facing(harv.move.int4_facing, act.cell_target.int4_x,
                                                       econ.cfg.int4_harv_turn_speed);
      return false;
    }
  }
  return false;
}

void act_on_first_run(world::WorldModel& w, HarvState& harv, std::int32_t int4_idx) {
  EconomyWorldState& econ{w.econ};
  HarvActivity& act{harv.vec_activities[static_cast<std::size_t>(int4_idx)]};
  if (act.kind == ActKind::kMove || act.kind == ActKind::kMoveReverse) {
    harv.set_blocked_move = blocked_cells_build(w, econ, harv.u4_actor);
    harv.move_outcome = MoveOutcome::kInProgress;
    pathfinding::move_queue(harv.move, act.cell_target);
  }
  if (act.kind == ActKind::kGenericDock) {
    queue_child_fresh(harv, int4_idx, ActKind::kWait, CPos{}, econ.cfg.int4_dock_wait);
  }
  if (act.kind == ActKind::kMoveOntoAndTurn) {
    queue_child_fresh(harv, int4_idx, ActKind::kMoveReverse, econ.cfg.cell_dock, 0);
  }
}

std::int32_t tick_outer(world::WorldModel& w, HarvState& harv, std::int32_t int4_idx) {
  if (!harv.vec_activities[static_cast<std::size_t>(int4_idx)].b_first_run) {
    harv.vec_activities[static_cast<std::size_t>(int4_idx)].b_first_run = true;
    act_on_first_run(w, harv, int4_idx);
  }

  bool b_child_done{true};
  {
    const std::int32_t int4_child{harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_child};
    if (int4_child != -1) {
      std::int32_t int4_next{int4_child};
      while (int4_next != -1) {
        const std::int32_t int4_prev{int4_next};
        int4_next = tick_outer(w, harv, int4_prev);
        if (int4_next == int4_prev) break;
      }
      harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_child = int4_next;
      b_child_done = int4_next == -1;
    }
  }

  bool b_last{b_child_done && (harv.vec_activities[static_cast<std::size_t>(int4_idx)].b_finishing ||
                               act_tick(w, harv, int4_idx))};
  if (b_last) harv.vec_activities[static_cast<std::size_t>(int4_idx)].b_finishing = true;

  {
    const std::int32_t int4_child{harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_child};
    if (int4_child != -1 && !harv.vec_activities[static_cast<std::size_t>(int4_child)].b_first_run) {
      std::int32_t int4_next{int4_child};
      while (int4_next != -1) {
        const std::int32_t int4_prev{int4_next};
        int4_next = tick_outer(w, harv, int4_prev);
        if (int4_next == int4_prev) break;
      }
      harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_child = int4_next;
      b_last = harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_child == -1 &&
               harv.vec_activities[static_cast<std::size_t>(int4_idx)].b_finishing;
    }
  }

  if (b_last) return harv.vec_activities[static_cast<std::size_t>(int4_idx)].int4_next;
  return int4_idx;
}

void harv_run_tick(world::WorldModel& w, HarvState& harv) {
  if (harv.int4_root == -1) return;
  std::int32_t int4_next{harv.int4_root};
  while (int4_next != -1) {
    const std::int32_t int4_prev{int4_next};
    int4_next = tick_outer(w, harv, int4_prev);
    if (int4_next == int4_prev) break;
  }
  harv.int4_root = int4_next;
}

const char* act_name(ActKind kind) {
  switch (kind) {
    case ActKind::kFindAndDeliver: return "FindAndDeliverResources";
    case ActKind::kHarvestResource: return "HarvestResource";
    case ActKind::kMoveToDock: return "MoveToDock";
    case ActKind::kMoveOntoAndTurn: return "MoveOntoAndTurn";
    case ActKind::kGenericDock: return "GenericDockSequence";
    case ActKind::kWait: return "Wait";
    case ActKind::kMove: return "Move";
    case ActKind::kMoveReverse: return "Move";
    case ActKind::kTurn: return "Turn";
  }
  return "-";
}

const char* move_part_name(pathfinding::PartKind kind) {
  switch (kind) {
    case pathfinding::PartKind::kTurn: return "Turn";
    case pathfinding::PartKind::kFirstHalf: return "MoveFirstHalf";
    case pathfinding::PartKind::kSecondHalf: return "MoveSecondHalf";
    case pathfinding::PartKind::kNone: break;
  }
  return nullptr;
}

}

[[nodiscard]] std::int32_t harv_cargo_sum(const HarvCargoState& cargo) {
  std::int32_t int4_sum{0};
  for (const auto& [str_type, int4_count] : cargo.vec_contents) int4_sum += int4_count;
  return int4_sum;
}

[[nodiscard]] bool harv_is_full(const EconomyWorldState& econ, const HarvState& harv) {
  return harv_cargo_sum(harv.cargo) >= econ.cfg.int4_harv_capacity;
}

[[nodiscard]] bool harv_is_empty(const HarvState& harv) { return harv_cargo_sum(harv.cargo) == 0; }

[[nodiscard]] std::int32_t harv_fullness(const HarvState& harv) {
  return harv_cargo_sum(harv.cargo) * 100 / harv.cargo.int4_capacity;
}

void cargo_add(HarvCargoState& cargo, std::string_view str_type, std::int32_t int4_amount) {
  for (auto& [str_key, int4_count] : cargo.vec_contents)
    if (str_key == str_type) {
      int4_count += int4_amount;
      return;
    }
  cargo.vec_contents.emplace_back(std::string(str_type), int4_amount);
}

std::int32_t refinery_accept(world::WorldModel& w, std::uint32_t u4_player, std::string_view str_type,
                             std::int32_t int4_count) {
  EconomyWorldState& econ{w.econ};
  const auto it_type{econ.layer.map_types.find(std::string(str_type))};
  if (it_type == econ.layer.map_types.end() || u4_player >= w.prod.vec_players.size()) return 0;
  const std::int32_t int4_resource_value{it_type->second.int4_value};

  std::int32_t int4_value{int4_count * int4_resource_value};
  production::PlayerResourcesState& res{w.prod.vec_players[u4_player].res};

  if (econ.cfg.b_use_storage) {
    const std::int32_t int4_storage_limit{std::max(res.int4_capacity - res.int4_resources, 0)};
    if (!econ.cfg.b_discard_excess) {
      while (int4_value > int4_storage_limit) {
        --int4_count;
        int4_value = int4_count * int4_resource_value;
      }
    } else {
      int4_value = std::min(int4_value, int4_storage_limit);
    }
    res.int4_resources += int4_value;
    if (res.int4_resources > res.int4_capacity) res.int4_resources = res.int4_capacity;
  } else {
    res.int4_cash += int4_value;
  }
  return int4_count;
}

std::int32_t resource_remove(ResourceLayerState& layer, EconomyWorldState& econ, std::string_view str_type,
                             CPos cell) {
  if (!cell_in_bounds(econ.map_move, cell)) return 0;
  const std::size_t idx{cell_index_map(econ.map_move, cell)};
  ResourceCellState& content{layer.vec_cells[idx]};
  if (content.str_type.empty() || content.str_type != str_type) return 0;

  const std::int32_t int4_old{content.u1_density};
  const std::int32_t int4_new{std::max(0, int4_old - 1)};
  if (int4_new == 0) {
    content.str_type.clear();
    content.u1_density = 0;
    resource_restore_cell_cost(econ, cell);
    --layer.int4_res_cells;
  } else {
    content.u1_density = static_cast<std::uint8_t>(int4_new);
  }
  return int4_old - int4_new;
}

[[nodiscard]] const ResourceCellState& resource_get(const ResourceLayerState& layer, CPos cell) {
  return layer.vec_cells[static_cast<std::size_t>(cell.int4_x) *
                             static_cast<std::size_t>(layer.int4_height) +
                         static_cast<std::size_t>(cell.int4_y)];
}

[[nodiscard]] std::string harv_activity_chain(const HarvState& harv) {
  std::string str_out;
  std::int32_t int4_idx{harv.int4_root};
  while (int4_idx != -1) {
    const HarvActivity& act{harv.vec_activities[static_cast<std::size_t>(int4_idx)]};
    if (!str_out.empty()) str_out += '/';
    str_out += act_name(act.kind);
    if (act.kind == ActKind::kMove || act.kind == ActKind::kMoveReverse) {
      if (const char* ptr_part{move_part_name(harv.move.part.kind)}) {
        str_out += '/';
        str_out += ptr_part;
      }
    }
    int4_idx = act.int4_child;
  }
  return str_out;
}

void economy_on_tick(world::WorldModel& w) {
  if (!w.econ.b_ready) return;
  for (HarvState& harv : w.econ.vec_harvs) {
    if (harv.u4_actor >= w.vec_actors_all.size()) continue;
    if (!w.vec_actors_all[harv.u4_actor].b_in_world) continue;
    harv_run_tick(w, harv);
  }
}

void economy_on_actor_added(world::WorldModel& w, std::uint32_t u4_actor_id) {
  if (!w.econ.b_ready) return;
  if (u4_actor_id >= w.vec_actors_all.size()) return;
  EconomyWorldState& econ{w.econ};
  const world::ActorModel& actor{w.vec_actors_all[u4_actor_id]};

  if (find_world_template(w, actor.str_type, "FreeActor") != nullptr)
    w.deque_frame_end.push_back(world::FrameEndTask{.u4_kind = 3, .u4_actor = u4_actor_id});

  if (find_world_template(w, actor.str_type, "DockHost") != nullptr) {
    if (dock_host_of(econ, u4_actor_id) == nullptr) {
      DockHostState host;
      host.u4_actor = u4_actor_id;
      econ.vec_hosts.push_back(host);
    }
  }

  if (find_world_template(w, actor.str_type, "StoresPlayerResources") != nullptr) {
    const world::TraitTemplate* ptr_tpl{find_world_template(w, actor.str_type, "StoresPlayerResources")};
    const std::int32_t int4_capacity{parse_int_kv(*ptr_tpl, "Capacity").value_or(0)};
    if (actor.u4_owner < w.prod.vec_players.size())
      w.prod.vec_players[actor.u4_owner].res.int4_capacity += int4_capacity;
  }

  if (actor.str_type == econ.cfg.str_harv_type && harv_state_of(econ, u4_actor_id) == nullptr) {
    HarvState harv;
    harv.u4_actor = u4_actor_id;
    harv.cargo.int4_capacity = econ.cfg.int4_harv_capacity;
    for (const std::string& str_type : econ.layer.vec_type_order)
      harv.cargo.vec_contents.emplace_back(str_type, 0);
    harv.move.cell_from = CPos{actor.int4_pos_x, actor.int4_pos_y};
    harv.move.cell_to = harv.move.cell_from;
    harv.move.pos_center = center_of_cell(harv.move.cell_from);
    harv.int4_root = act_alloc(harv, ActKind::kFindAndDeliver);
    econ.vec_harvs.push_back(std::move(harv));
  }
}

void economy_on_actor_removed(world::WorldModel& w, std::uint32_t u4_actor_id) {
  for (DockHostState& host : w.econ.vec_hosts) {
    const auto it{std::find(host.vec_reserved.begin(), host.vec_reserved.end(), u4_actor_id)};
    if (it != host.vec_reserved.end()) host.vec_reserved.erase(it);
    if (host.u4_docked_client == u4_actor_id) host.u4_docked_client = kNoActor;
  }
}

void economy_frame_end_task(world::WorldModel& w, const world::FrameEndTask& task) {
  if (task.u4_actor >= w.vec_actors_all.size()) return;
  const world::ActorModel& host{w.vec_actors_all[task.u4_actor]};
  const world::TraitTemplate* ptr_free{find_world_template(w, host.str_type, "FreeActor")};
  if (ptr_free == nullptr) return;

  const std::string_view str_actor{kv_find(*ptr_free, "Actor")};
  const std::vector<std::string_view> vec_offset{split_list(kv_find(*ptr_free, "SpawnOffset"))};
  std::int32_t int4_off_x{0};
  std::int32_t int4_off_y{0};
  if (vec_offset.size() == 2) {
    const auto opt_x{parse_int(vec_offset[0])};
    const auto opt_y{parse_int(vec_offset[1])};
    if (opt_x.has_value()) int4_off_x = *opt_x;
    if (opt_y.has_value()) int4_off_y = *opt_y;
  }
  const std::int32_t int4_facing{parse_int_kv(*ptr_free, "Facing").value_or(0)};

  std::string str_type_lower;
  for (const char ch : str_actor)
    str_type_lower.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);

  auto res_spawn{world::create_actor(w, str_type_lower, host.u4_owner, host.int4_pos_x + int4_off_x,
                                     host.int4_pos_y + int4_off_y, true)};
  if (!res_spawn.has_value()) return;
  if (HarvState* ptr_harv{harv_state_of(w.econ, *res_spawn)})
    ptr_harv->move.int4_facing = int4_facing;
}

[[nodiscard]] std::string economy_observation(const world::WorldModel& w, std::uint32_t u4_player) {
  const EconomyWorldState& econ{w.econ};
  std::string str_out;

  if (u4_player < w.prod.vec_players.size()) {
    const production::PlayerResourcesState& res{w.prod.vec_players[u4_player].res};
    str_out += std::format("E {} cash {} ore {} cap {} rescells {} rng {} cnt {}\n", w.int4_world_tick,
                           res.int4_cash, res.int4_resources, res.int4_capacity, econ.layer.int4_res_cells,
                           w.rng_shared.int4_last, w.rng_shared.int4_total_count);
  }

  for (const HarvState& harv : econ.vec_harvs) {
    if (harv.u4_actor >= w.vec_actors_all.size()) continue;
    if (!w.vec_actors_all[harv.u4_actor].b_in_world) continue;
    const std::int32_t int4_view_id{econ_view_id(w, harv.u4_actor)};
    if (int4_view_id < 0) continue;
    std::string str_contents;
    for (const auto& [str_type, int4_count] : harv.cargo.vec_contents) {
      if (!str_contents.empty()) str_contents += ',';
      str_contents += std::format("{}={}", str_type, int4_count);
    }
    str_out += std::format(
        "H {} id {} at {},{}->{},{} pos {},{} face {} load {} [{}] act {} mr {} idle {}\n",
        w.int4_world_tick, int4_view_id, harv.move.cell_from.int4_x, harv.move.cell_from.int4_y,
        harv.move.cell_to.int4_x, harv.move.cell_to.int4_y, harv.move.pos_center.int4_x,
        harv.move.pos_center.int4_y, harv.move.int4_facing, harv_cargo_sum(harv.cargo),
        str_contents, harv_activity_chain(harv),
        harv.move_outcome == MoveOutcome::kCompleteDestinationReached ? "CompleteDestinationReached"
                                                                     : "InProgress",
        "False");
  }

  for (const DockHostState& host : econ.vec_hosts) {
    if (host.u4_actor >= w.vec_actors_all.size()) continue;
    if (!w.vec_actors_all[host.u4_actor].b_in_world) continue;
    const std::int32_t int4_view_id{econ_view_id(w, host.u4_actor)};
    if (int4_view_id < 0) continue;
    const std::int32_t int4_docked_id{
        host.u4_docked_client == kNoActor ? -1 : econ_view_id(w, host.u4_docked_client)};
    str_out += std::format("D {} proc {} reserved {} docked {}\n", w.int4_world_tick, int4_view_id,
                           host.vec_reserved.size(), int4_docked_id);
  }

  if (u4_player < w.prod.vec_players.size()) {
    for (const production::ProductionQueueState& queue : w.prod.vec_players[u4_player].vec_queues) {
      str_out += std::format("Q {} ClassicProductionQueue enabled {} [", w.int4_world_tick,
                             queue.b_enabled ? "True" : "False");
      bool b_first{true};
      for (const production::ProductionItemModel& item : queue.vec_queue) {
        if (!b_first) str_out += " | ";
        b_first = false;
        str_out += std::format("{}:t{}/{}:c{}/{}:s{}:d{}:p{}", item.str_item, item.int4_remaining_time,
                               item.int4_total_time, item.int4_remaining_cost, item.int4_total_cost,
                               item.int4_slowdown, item.b_done ? "True" : "False",
                               item.int4_resources_paid);
      }
      str_out += "]\n";
    }
  }

  return str_out;
}

[[nodiscard]] std::string economy_actor_line(const world::WorldModel& w) {
  std::string str_out;
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const std::int32_t int4_view_id{econ_view_id(w, u4_id)};
    if (int4_view_id < 0) continue;
    if (!str_out.empty()) str_out += ' ';
    str_out += std::format("{}:{}", int4_view_id, w.vec_actors_all[u4_id].str_type);
  }
  return str_out;
}

// Harvester.IResolveOrder.ResolveOrder 的 "Harvest"（Harvester.cs:258-277）：
// target cell → QueueActivity(FindAndDeliverResources(self, loc))。NearestCell 的
// claim/邻格退化面不在最小面（D038 裁面——目标格直取）；bot 单 target=FromCell。
void harvester_resolve(const world::TraitResolveCtx& ctx, world::TraitModel&) {
  const network::OrderModel& order{ctx.ref_order};
  if (order.str_order != "Harvest") return;
  HarvState* ptr_harv{harv_state_of(ctx.ref_world.econ, ctx.ref_actor.u4_id)};
  if (ptr_harv == nullptr) return;

  CPos cell_loc{ptr_harv->move.cell_from};
  if (order.target.kind == network::TargetKind::kTerrain && order.target.b_cell)
    cell_loc = CPos{static_cast<std::int32_t>(static_cast<std::uint32_t>(
                        order.target.int4_cell_bits) >> 20),
                    (static_cast<std::int16_t>(order.target.int4_cell_bits >> 4)) >> 4};

  // QueueActivity(queued=false, ...)：bot 单恒非 queued——当前活动链整体替换
  // （上游 QueueActivity(queued=false) 走 CancelAndCancelActivityTriggers 链语义）。
  ptr_harv->vec_activities.clear();
  ptr_harv->int4_root = act_alloc(*ptr_harv, ActKind::kFindAndDeliver);
  ptr_harv->opt_order_loc = cell_loc;
}

// FindNextResource（HarvesterBotModule.cs:295-345）的 C++ 等价入口：多源寻矿
// （closest_harvestable_pos——无敌人时威胁规避 bin 成本恒 0，HarvesterEnemyAvoidance
// 面在无敌对单位场景与上游等价）。返回 path[0]（Target.FromCell 的格）。
[[nodiscard]] std::optional<CPos> harv_find_next_resource(world::WorldModel& w,
                                                          HarvState& harv) {
  return closest_harvestable_pos(w, harv);
}

[[nodiscard]] const std::vector<world::TraitSpec>& economy_trait_specs() {
  static const std::vector<world::TraitSpec> vec_specs{
      world::TraitSpec{.str_kind = "Locomotor", .str_str_param_key = "Name"},
      world::TraitSpec{.str_kind = "ResourceLayer"},
      world::TraitSpec{.str_kind = "ResourceClaimLayer"},
      world::TraitSpec{.str_kind = "SpawnMapActors"},
      world::TraitSpec{.str_kind = "BuildingInfluence"},
      world::TraitSpec{.str_kind = "PathFinder"},
      world::TraitSpec{.str_kind = "ActorMap"},
      world::TraitSpec{.str_kind = "ScreenMap"},
      world::TraitSpec{.str_kind = "Selection"},
      world::TraitSpec{.str_kind = "ControlGroups"},
      world::TraitSpec{.str_kind = "Shroud"},
      world::TraitSpec{.str_kind = "DeveloperMode"},
      world::TraitSpec{.str_kind = "TechTree"},
      world::TraitSpec{.str_kind = "RenderSprites"},
      world::TraitSpec{.str_kind = "WithSpriteBody"},
      world::TraitSpec{.str_kind = "WithDockingAnimation"},
      world::TraitSpec{.str_kind = "Targetable"},
      world::TraitSpec{.str_kind = "HitShape"},
      world::TraitSpec{.str_kind = "BodyOrientation"},
      world::TraitSpec{.str_kind = "DockHost"},
      world::TraitSpec{.str_kind = "Refinery"},
      world::TraitSpec{.str_kind = "StoresPlayerResources"},
      world::TraitSpec{.str_kind = "FreeActor"},
      world::TraitSpec{.str_kind = "StoresResources"},
      world::TraitSpec{.str_kind = "Harvester",
                       .b_implements_resolve = true,
                       .str_order_id = "Harvest",
                       .b_order_terrain_target = true,
                       .fn_resolve = harvester_resolve},
      world::TraitSpec{.str_kind = "DockClientManager"},
  };
  return vec_specs;
}

std::expected<void, std::string> economy_setup(world::WorldModel& w, const MapBin& bin_map,
                                               const Tileset& tileset, std::string_view str_rules_text,
                                               std::int32_t int4_left, std::int32_t int4_top,
                                               std::int32_t int4_right, std::int32_t int4_bottom) {
  EconomyWorldState& econ{w.econ};
  auto res_rules{miniyaml_from_string(str_rules_text, "economy-rules", true)};
  if (!res_rules.has_value()) return std::unexpected(res_rules.error());

  const MiniYamlNode* node_world{nullptr};
  const MiniYamlNode* node_player{nullptr};
  for (const MiniYamlNode& node : *res_rules) {
    if (node.str_key == "world") node_world = &node;
    if (node.str_key == "player") node_player = &node;
  }
  if (node_world == nullptr || node_player == nullptr)
    return std::unexpected("economy rules lack world/player");

  std::string str_host_type;
  for (const world::ActorTypeDef& def : w.vec_actor_types) {
    if (find_world_template(w, def.str_name_lower, "DockHost") != nullptr) {
      str_host_type = def.str_name_lower;
      break;
    }
  }
  if (str_host_type.empty()) return std::unexpected("economy rules lack DockHost actor");
  econ.cfg.str_host_type = str_host_type;

  const world::TraitTemplate* ptr_free{find_world_template(w, str_host_type, "FreeActor")};
  if (ptr_free == nullptr) return std::unexpected("dock host lacks FreeActor");
  std::string str_harv_type;
  for (const char ch : kv_find(*ptr_free, "Actor"))
    str_harv_type.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  econ.cfg.str_harv_type = str_harv_type;

  std::string str_locomotor_name{"heavywheeled"};
  if (const world::TraitTemplate* ptr_mobile_tpl{find_world_template(w, str_harv_type, "Mobile")};
      ptr_mobile_tpl != nullptr && !ptr_mobile_tpl->str_param_type.empty())
    str_locomotor_name = ptr_mobile_tpl->str_param_type;
  const MiniYamlNode* node_locomotor{nullptr};
  for (const MiniYamlNode& node : node_world->vec_children) {
    if (!node.str_key.starts_with("Locomotor@")) continue;
    const MiniYamlNode* node_name{find_child(node.vec_children, "Name")};
    if (node_name != nullptr && node_name->str_value == str_locomotor_name) {
      node_locomotor = &node;
      break;
    }
  }
  if (node_locomotor == nullptr) return std::unexpected("economy rules lack Locomotor " + str_locomotor_name);
  const auto res_def{pathfinding::locomotor_def_from_node(*node_locomotor)};
  if (!res_def.has_value()) return std::unexpected(res_def.error());
  econ.cfg.vec_locomotor_speeds = res_def->vec_speeds;
  econ.map_move =
      pathfinding::locomotor_map_build(bin_map, tileset, *res_def, int4_left, int4_top, int4_right, int4_bottom);

  const MiniYamlNode* node_layer{find_child(node_world->vec_children, "ResourceLayer")};
  if (node_layer == nullptr) return std::unexpected("economy rules lack ResourceLayer");
  if (const MiniYamlNode* node_recalc{find_child(node_layer->vec_children,
                                                 "RecalculateResourceDensity")})
    econ.layer.b_recalculate_density =
        node_recalc->str_value == "true" || node_recalc->str_value == "True";
  if (const MiniYamlNode* node_types{find_child(node_layer->vec_children, "ResourceTypes")}) {
    for (const MiniYamlNode& node_type : node_types->vec_children) {
      ResourceTypeSpec spec;
      spec.str_name = node_type.str_key;
      spec.u1_index = static_cast<std::uint8_t>(parse_int_node(node_type, "ResourceIndex").value_or(0));
      if (const MiniYamlNode* node_terrain{find_child(node_type.vec_children, "TerrainType")})
        spec.str_terrain = node_terrain->str_value;
      if (const MiniYamlNode* node_allowed{find_child(node_type.vec_children, "AllowedTerrainTypes")})
        for (const std::string_view str_part : split_list(node_allowed->str_value))
          spec.vec_allowed_terrain.emplace_back(str_part);
      spec.int4_max_density = parse_int_node(node_type, "MaxDensity").value_or(1);
      econ.layer.vec_type_order.push_back(spec.str_name);
      econ.layer.map_types.emplace(spec.str_name, std::move(spec));
    }
  }

  if (const MiniYamlNode* node_res{find_child(node_player->vec_children, "PlayerResources")}) {
    if (const MiniYamlNode* node_values{find_child(node_res->vec_children, "ResourceValues")}) {
      for (const MiniYamlNode& node_value : node_values->vec_children) {
        const auto it_type{econ.layer.map_types.find(node_value.str_key)};
        if (it_type != econ.layer.map_types.end())
          it_type->second.int4_value = parse_int(node_value.str_value).value_or(0);
      }
    }
  }

  const world::TraitTemplate* ptr_harv_mobile{find_world_template(w, str_harv_type, "Mobile")};
  if (ptr_harv_mobile != nullptr) {
    econ.cfg.int4_harv_speed = parse_int_kv(*ptr_harv_mobile, "Speed").value_or(econ.cfg.int4_harv_speed);
    econ.cfg.int4_harv_turn_speed =
        parse_int_kv(*ptr_harv_mobile, "TurnSpeed").value_or(econ.cfg.int4_harv_turn_speed);
  }

  const world::TraitTemplate* ptr_stores{find_world_template(w, str_harv_type, "StoresResources")};
  if (ptr_stores != nullptr) {
    econ.cfg.int4_harv_capacity = parse_int_kv(*ptr_stores, "Capacity").value_or(econ.cfg.int4_harv_capacity);
    const std::vector<std::string_view> vec_resources{split_list(kv_find(*ptr_stores, "Resources"))};
    econ.layer.vec_type_order.clear();
    for (const std::string_view str_type : vec_resources)
      if (econ.layer.map_types.contains(std::string(str_type)))
        econ.layer.vec_type_order.emplace_back(str_type);
  }

  const world::TraitTemplate* ptr_harvester{find_world_template(w, str_harv_type, "Harvester")};
  if (ptr_harvester != nullptr) {
    econ.cfg.int4_bale_load_delay =
        parse_int_kv(*ptr_harvester, "BaleLoadDelay").value_or(econ.cfg.int4_bale_load_delay);
    econ.cfg.int4_bale_unload_delay =
        parse_int_kv(*ptr_harvester, "BaleUnloadDelay").value_or(econ.cfg.int4_bale_unload_delay);
    econ.cfg.int4_bale_unload_amount =
        parse_int_kv(*ptr_harvester, "BaleUnloadAmount").value_or(econ.cfg.int4_bale_unload_amount);
    econ.cfg.int4_search_from_proc_radius =
        parse_int_kv(*ptr_harvester, "SearchFromProcRadius").value_or(econ.cfg.int4_search_from_proc_radius);
    econ.cfg.int4_search_from_harvester_radius =
        parse_int_kv(*ptr_harvester, "SearchFromHarvesterRadius")
            .value_or(econ.cfg.int4_search_from_harvester_radius);
    econ.cfg.int4_harvest_facings =
        parse_int_kv(*ptr_harvester, "HarvestFacings").value_or(econ.cfg.int4_harvest_facings);
    econ.cfg.int4_wait_duration =
        parse_int_kv(*ptr_harvester, "WaitDuration").value_or(econ.cfg.int4_wait_duration);
    econ.cfg.int4_fully_loaded_speed =
        parse_int_kv(*ptr_harvester, "FullyLoadedSpeed").value_or(econ.cfg.int4_fully_loaded_speed);
    econ.cfg.int4_refinery_penalty = parse_int_kv(*ptr_harvester, "ResourceRefineryDirectionPenalty")
                                         .value_or(econ.cfg.int4_refinery_penalty);
  }

  const world::TraitTemplate* ptr_dock_host{find_world_template(w, str_host_type, "DockHost")};
  if (ptr_dock_host != nullptr) {
    econ.cfg.int4_dock_angle = parse_int_kv(*ptr_dock_host, "DockAngle").value_or(econ.cfg.int4_dock_angle);
    econ.cfg.int4_dock_wait = parse_int_kv(*ptr_dock_host, "DockWait").value_or(econ.cfg.int4_dock_wait);
    econ.cfg.int4_max_queue_length =
        parse_int_kv(*ptr_dock_host, "MaxQueueLength").value_or(econ.cfg.int4_max_queue_length);
  }

  const world::TraitTemplate* ptr_refinery{find_world_template(w, str_host_type, "Refinery")};
  if (ptr_refinery != nullptr) {
    const std::string_view str_use{kv_find(*ptr_refinery, "UseStorage")};
    if (str_use == "False") econ.cfg.b_use_storage = false;
    const std::string_view str_discard{kv_find(*ptr_refinery, "DiscardExcessResources")};
    if (str_discard == "True") econ.cfg.b_discard_excess = true;
  }

  econ.layer.int4_width = bin_map.int4_width;
  econ.layer.int4_height = bin_map.int4_height;
  econ.layer.vec_cells.assign(
      static_cast<std::size_t>(bin_map.int4_width) * static_cast<std::size_t>(bin_map.int4_height),
      ResourceCellState{});

  for (std::int32_t int4_x{0}; int4_x < bin_map.int4_width; ++int4_x)
    for (std::int32_t int4_y{0}; int4_y < bin_map.int4_height; ++int4_y) {
      const ResourceTile& res_tile{bin_map.resource_at(int4_x, int4_y)};
      if (res_tile.u1_type == 0) continue;
      const ResourceTypeSpec* ptr_spec{nullptr};
      for (const auto& [str_name, spec] : econ.layer.map_types)
        if (spec.u1_index == res_tile.u1_type) ptr_spec = &spec;
      if (ptr_spec == nullptr) continue;
      const CPos cell{int4_x, int4_y};
      if (!allow_resource_at(econ.map_move, tileset, bin_map, *ptr_spec, cell)) continue;
      create_resource_cell(econ.layer, econ, *ptr_spec, cell, res_tile.u1_density);
    }

  if (econ.layer.b_recalculate_density) {
    for (std::int32_t int4_x{0}; int4_x < bin_map.int4_width; ++int4_x)
      for (std::int32_t int4_y{0}; int4_y < bin_map.int4_height; ++int4_y) {
        ResourceCellState& content{
            econ.layer.vec_cells[static_cast<std::size_t>(int4_x) *
                                     static_cast<std::size_t>(bin_map.int4_height) +
                                 static_cast<std::size_t>(int4_y)]};
        if (content.str_type.empty()) continue;
        const ResourceTypeSpec& spec{econ.layer.map_types.find(content.str_type)->second};
        std::int32_t int4_adjacent{0};
        for (const CVec2& vec_dir : kDirections8) {
          const CPos cell_neighbor{int4_x + vec_dir.int4_x, int4_y + vec_dir.int4_y};
          if (!cell_in_bounds(econ.map_move, cell_neighbor)) continue;
          const ResourceCellState& content_neighbor{
              econ.layer.vec_cells[static_cast<std::size_t>(cell_neighbor.int4_x) *
                                       static_cast<std::size_t>(bin_map.int4_height) +
                                   static_cast<std::size_t>(cell_neighbor.int4_y)]};
          if (content_neighbor.str_type == content.str_type) ++int4_adjacent;
        }
        content.u1_density =
            static_cast<std::uint8_t>(std::max(spec.int4_max_density * int4_adjacent / 9, 1));
      }
  }

  {
    const world::TraitTemplate* ptr_building{find_world_template(w, str_host_type, "Building")};
    const std::vector<std::string_view> vec_dims{split_list(kv_find(*ptr_building, "Dimensions"))};
    std::int32_t int4_dim_x{1};
    std::int32_t int4_dim_y{1};
    if (vec_dims.size() == 2) {
      const auto opt_x{parse_int(vec_dims[0])};
      const auto opt_y{parse_int(vec_dims[1])};
      if (opt_x.has_value()) int4_dim_x = *opt_x;
      if (opt_y.has_value()) int4_dim_y = *opt_y;
    }
    const world::ActorModel* ptr_host_actor{nullptr};
    for (const std::uint32_t u4_id : w.vec_in_world)
      if (w.vec_actors_all[u4_id].str_type == str_host_type) {
        ptr_host_actor = &w.vec_actors_all[u4_id];
        break;
      }
    if (ptr_host_actor == nullptr) return std::unexpected("dock host actor not in world");

    std::int64_t int8_x{static_cast<std::int64_t>(ptr_host_actor->int4_pos_x) * 1024 + 512 +
                        (static_cast<std::int64_t>(int4_dim_x) * 1024 + 512 - 1536) / 2};
    std::int64_t int8_y{static_cast<std::int64_t>(ptr_host_actor->int4_pos_y) * 1024 + 512 +
                        (static_cast<std::int64_t>(int4_dim_y) * 1024 + 512 - 1536) / 2};
    if (const std::string_view str_center_off{kv_find(*ptr_building, "LocalCenterOffset")};
        !str_center_off.empty()) {
      const std::vector<std::string_view> vec_off{split_list(str_center_off)};
      if (vec_off.size() == 3) {
        std::int64_t int8_off_x{};
        std::int64_t int8_off_y{};
        std::from_chars(vec_off[0].data(), vec_off[0].data() + vec_off[0].size(), int8_off_x);
        std::from_chars(vec_off[1].data(), vec_off[1].data() + vec_off[1].size(), int8_off_y);
        int8_x += int8_off_x;
        int8_y += int8_off_y;
      }
    }

    const std::vector<std::string_view> vec_dock_off{split_list(kv_find(*ptr_dock_host, "DockOffset"))};
    std::int64_t int8_dock_x{};
    std::int64_t int8_dock_y{};
    if (vec_dock_off.size() == 3) {
      const std::string_view str_x{vec_dock_off[0]};
      const std::string_view str_y{vec_dock_off[1]};
      const std::size_t idx_cx{str_x.find('c')};
      const std::size_t idx_cy{str_y.find('c')};
      if (idx_cx != std::string_view::npos) {
        std::int64_t int8_cells{};
        std::int64_t int8_sub{};
        std::from_chars(str_x.data(), str_x.data() + idx_cx, int8_cells);
        std::from_chars(str_x.data() + idx_cx + 1, str_x.data() + str_x.size(), int8_sub);
        int8_dock_x = int8_cells * 1024 + int8_sub;
      } else std::from_chars(str_x.data(), str_x.data() + str_x.size(), int8_dock_x);
      if (idx_cy != std::string_view::npos) {
        std::int64_t int8_cells{};
        std::int64_t int8_sub{};
        std::from_chars(str_y.data(), str_y.data() + idx_cy, int8_cells);
        std::from_chars(str_y.data() + idx_cy + 1, str_y.data() + str_y.size(), int8_sub);
        int8_dock_y = int8_cells * 1024 + int8_sub;
      } else std::from_chars(str_y.data(), str_y.data() + str_y.size(), int8_dock_y);
    }

    econ.cfg.pos_dock = WPos{static_cast<std::int32_t>(int8_x + int8_dock_x),
                             static_cast<std::int32_t>(int8_y + int8_dock_y), 0};
    econ.cfg.cell_dock = CPos{(static_cast<std::int32_t>(int8_x + int8_dock_x)) / 1024,
                              (static_cast<std::int32_t>(int8_y + int8_dock_y)) / 1024};
  }

  econ.b_ready = true;
  for (const std::uint32_t u4_id : w.vec_in_world) economy_on_actor_added(w, u4_id);
  return {};
}

}
