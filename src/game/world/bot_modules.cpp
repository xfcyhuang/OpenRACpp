module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

// T34 生产类 BotModule（issue #53）：ModularBot 收集面的四模块决策实现
// （UnitBuilder/Harvester/BuildingRepair/PowerDown，C# 逐语义复刻——决策随机走
// rng_local，订单经 bot_queue_order 入 ModularBot 待发队列）。
// 未实现模块（ResourceMap/SupportPower/BaseBuilder/SquadManager/Mcv×2/Minelayer/
// Capture）不入收集面：tick 序位与 TraitEnabled rng 消耗序相对 oracle 有缺
// （BaseBuilder 3 抽 + SquadManager N 抽 + McvExpansion 3 抽），登记见 COVERAGE。

namespace ora::game::world {

namespace {

constexpr std::array<std::string_view, 4> kBotModuleKinds{
    "HarvesterBotModule", "PowerDownBotModule", "UnitBuilderBotModule",
    "BuildingRepairBotModule"};

std::string_view kv_of(const std::vector<std::pair<std::string, std::string>>& vec_kv,
                       std::string_view str_key) {
  for (const auto& [k, v] : vec_kv)
    if (k == str_key) return v;
  return {};
}

std::int32_t kv_int_of(const std::vector<std::pair<std::string, std::string>>& vec_kv,
                       std::string_view str_key, std::int32_t int4_fallback) {
  const std::string_view str_value{kv_of(vec_kv, str_key)};
  if (str_value.empty()) return int4_fallback;
  std::int32_t int4_out{};
  const auto res{std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_out)};
  if (res.ec != std::errc{}) return int4_fallback;
  return int4_out;
}

std::vector<std::string> split_ws(std::string_view str_value) {
  std::vector<std::string> vec_out;
  std::size_t idx{};
  while (idx < str_value.size()) {
    while (idx < str_value.size() && (str_value[idx] == ' ' || str_value[idx] == ','))
      ++idx;
    std::size_t idx_end{idx};
    while (idx_end < str_value.size() && str_value[idx_end] != ' ' && str_value[idx_end] != ',')
      ++idx_end;
    if (idx_end > idx) vec_out.emplace_back(str_value.substr(idx, idx_end - idx));
    idx = idx_end;
  }
  return vec_out;
}

// RequiresCondition 析取求值（GrantConditionOnBotOwner 等价直配，D091⑨）：
// 条件名（enable-xxx-ai）经条件表映射回 bot 类型集，命中型即启用。
std::vector<std::string> bot_types_of_condition(
    const std::vector<std::pair<std::string, std::string>>& vec_kv,
    const std::map<std::string, std::vector<std::string>>& map_condition_bots) {
  std::vector<std::string> vec_types;
  const std::string_view str_requires{kv_of(vec_kv, "RequiresCondition")};
  std::size_t idx{};
  while (idx < str_requires.size()) {
    std::size_t idx_end{str_requires.find('|', idx)};
    if (idx_end == std::string_view::npos) idx_end = str_requires.size();
    std::string_view str_term{str_requires.substr(idx, idx_end - idx)};
    while (!str_term.empty() && (str_term.front() == ' ' || str_term.front() == '&'))
      str_term.remove_prefix(1);
    while (!str_term.empty() && (str_term.back() == ' ' || str_term.back() == '&'))
      str_term.remove_suffix(1);
    const auto it{map_condition_bots.find(std::string{str_term})};
    if (it != map_condition_bots.end())
      for (const std::string& str_type : it->second) vec_types.push_back(str_type);
    idx = idx_end == str_requires.size() ? idx_end : idx_end + 1;
  }
  return vec_types;
}

const ActorTypeDef* player_type_def(const WorldModel& w) {
  for (const ActorTypeDef& def : w.vec_actor_types)
    if (def.str_name_lower == "player") return &def;
  return nullptr;
}

const std::vector<TraitTemplate>& type_templates_of(const WorldModel& w,
                                                    std::string_view str_type) {
  static const std::vector<TraitTemplate> vec_empty{};
  for (const ActorTypeDef& def : w.vec_actor_types)
    if (def.str_name_lower == str_type) return def.vec_templates;
  return vec_empty;
}

const TraitTemplate* template_of(const WorldModel& w, std::string_view str_type,
                                 std::string_view str_kind) {
  for (const TraitTemplate& tpl : type_templates_of(w, str_type))
    if (tpl.str_kind == str_kind) return &tpl;
  return nullptr;
}

BotModuleState* module_state_of(BotModel& bot, std::string_view str_kind) {
  for (BotModuleState& state : bot.vec_module_states)
    if (state.str_kind == str_kind) return &state;
  return nullptr;
}

bool module_enabled(const BotModel& bot, std::string_view str_kind) {
  for (const BotModuleState& state : bot.vec_module_states)
    if (state.str_kind == str_kind) return true;
  return false;
}

std::int32_t count_owned_of_type(const WorldModel& w, std::uint32_t u4_player,
                                 std::string_view str_type) {
  std::int32_t int4_count{};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner == u4_player && actor.str_type == str_type) ++int4_count;
  }
  return int4_count;
}

// UnitsToBuild/UnitLimits/UnitDelays 子表（装载器展平为 "表名.键" 前缀键）。
std::map<std::string, std::int32_t> kv_int_table(
    const std::vector<std::pair<std::string, std::string>>& vec_kv,
    std::string_view str_table) {
  std::map<std::string, std::int32_t> map_out;
  const std::string str_prefix{std::string{str_table} + "."};
  for (const auto& [k, v] : vec_kv) {
    if (!k.starts_with(str_prefix)) continue;
    std::int32_t int4_out{};
    const auto res{std::from_chars(v.data(), v.data() + v.size(), int4_out)};
    if (res.ec == std::errc{}) map_out[k.substr(str_prefix.size())] = int4_out;
  }
  return map_out;
}

// —— UnitBuilderBotModule（UnitBuilderBotModule.cs:66-160）——

std::string_view kv_of_template(const TraitTemplate& tpl, std::string_view str_key) {
  for (const auto& [k, v] : tpl.vec_kv)
    if (k == str_key) return v;
  return {};
}

std::vector<std::string_view> split_list_view(std::string_view str_value) {
  std::vector<std::string_view> vec_out;
  std::size_t idx{};
  while (idx < str_value.size()) {
    while (idx < str_value.size() && (str_value[idx] == ' ' || str_value[idx] == ','))
      ++idx;
    std::size_t idx_end{idx};
    while (idx_end < str_value.size() && str_value[idx_end] != ' ' && str_value[idx_end] != ',')
      ++idx_end;
    if (idx_end > idx) vec_out.push_back(str_value.substr(idx, idx_end - idx));
    idx = idx_end;
  }
  return vec_out;
}

constexpr std::int32_t kFeedbackTime{30};

// AIUtils.FindQueuesByCategory 的 C++ 面：玩家 ProductionQueue trait 挂在建筑上，
// Enabled 建筑存在即有队列。返回 category → 空闲（AllQueued 空）队列的建筑 id。
std::vector<std::uint32_t> free_queues_of_category(const WorldModel& w, std::uint32_t u4_player,
                                                   std::string_view str_category_raw) {
  std::vector<std::uint32_t> vec_out;
  const std::string str_category{string_to_lower_ascii(str_category_raw)};
  const production::PlayerProductionState* ptr_state{
      u4_player < w.prod.vec_players.size() ? &w.prod.vec_players[u4_player] : nullptr};
  if (ptr_state == nullptr) return vec_out;
  const production::ProductionQueueState* ptr_queue{nullptr};
  for (const production::ProductionQueueState& queue : ptr_state->vec_queues)
    if (queue.str_type == str_category) ptr_queue = &queue;
  if (ptr_queue == nullptr || !ptr_queue->b_enabled) return vec_out;
  if (!ptr_queue->vec_queue.empty()) return vec_out;

  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    const TraitTemplate* ptr_production{template_of(w, actor.str_type, "Production")};
    if (ptr_production == nullptr) continue;
    for (const std::string_view str_p : split_list_view(ptr_production->str_param_type))
      if (str_p == str_category) {
        vec_out.push_back(u4_id);
        break;
      }
  }
  return vec_out;
}

// RA 面 HasAdequateAirUnitReloadBuildings（UnitBuilderBotModule.cs:204-221）：
// AircraftInfo 单位要求 RearmActors 数 >= 现有 Aircraft 数。
bool has_adequate_air_reload_buildings(const WorldModel& w, std::uint32_t u4_player,
                                       std::string_view str_unit) {
  if (template_of(w, str_unit, "Aircraft") == nullptr) return true;
  std::int32_t int4_rearms{};
  std::int32_t int4_aircraft{};
  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ActorModel& actor{w.vec_actors_all[u4_id]};
    if (actor.u4_owner != u4_player) continue;
    if (template_of(w, actor.str_type, "RearmActors") != nullptr) ++int4_rearms;
    if (template_of(w, actor.str_type, "Aircraft") != nullptr) ++int4_aircraft;
  }
  return int4_rearms >= int4_aircraft;
}

void bot_queue_order_for(BotModel& bot, network::OrderModel order) {
  bot.deque_orders.push_back(std::move(order));
}

// ChooseRandomUnitToBuild（UnitBuilderBotModule.cs:164-196）：Shuffle 消耗 rng_local
// （尾向 Fisher-Yates，n-1 抽），误差最小者出单；error<0（缺口）即取（air 校验）。
void unit_builder_build_random(WorldModel& w, BotModel& bot,
                               const BotModuleModel& module,
                               std::string_view str_queue_type) {
  const auto map_units{kv_int_table(module.vec_kv, "UnitsToBuild")};
  if (map_units.empty()) return;
  const auto map_limits{kv_int_table(module.vec_kv, "UnitLimits")};
  const auto map_delays{kv_int_table(module.vec_kv, "UnitDelays")};

  std::vector<std::string> vec_buildable{
      production::queue_buildable_items(w, bot.u4_player, str_queue_type)};
  if (vec_buildable.empty()) return;
  {
    std::vector<std::string> vec_pool;
    for (const std::string& str_unit : vec_buildable)
      if (map_units.contains(str_unit)) vec_pool.push_back(str_unit);
    vec_buildable = std::move(vec_pool);
  }
  support::shuffle_in_place(
      w.rng_local, std::span<std::string>{vec_buildable}, 0,
      static_cast<std::int32_t>(vec_buildable.size()));

  // unitsToBuild.Actors——UnitsToBuild 键名下玩家在世 actor（!IsDead）。
  std::int32_t int4_all_units{};
  for (const auto& [str_unit, int4_share] : map_units)
    int4_all_units += count_owned_of_type(w, bot.u4_player, str_unit);

  const std::string* ptr_desired{nullptr};
  std::int32_t int4_desired_error{std::numeric_limits<std::int32_t>::max()};
  for (const std::string& str_unit : vec_buildable) {
    const auto it_delay{map_delays.find(str_unit)};
    if (it_delay != map_delays.end() && it_delay->second > w.int4_world_tick) continue;
    const std::int32_t int4_unit_count{count_owned_of_type(w, bot.u4_player, str_unit)};
    const auto it_limit{map_limits.find(str_unit)};
    if (it_limit != map_limits.end() && int4_unit_count >= it_limit->second) continue;

    const std::int32_t int4_share{map_units.at(str_unit)};
    const std::int32_t int4_error{
        int4_all_units > 0 ? int4_unit_count * 100 / int4_all_units - int4_share : -1};
    if (int4_error < 0) {
      if (has_adequate_air_reload_buildings(w, bot.u4_player, str_unit)) {
        network::OrderModel order;
        order.str_order = "StartProduction";
        order.b_has_subject = true;
        order.u4_subject = w.vec_players[bot.u4_player].u4_actor_id;
        order.opt_str_target = str_unit;
        order.u4_extra_data = 1;
        bot_queue_order_for(bot, std::move(order));
      }
      return;
    }
    if (int4_error < int4_desired_error) {
      int4_desired_error = int4_error;
      ptr_desired = &str_unit;
    }
  }

  if (ptr_desired != nullptr && has_adequate_air_reload_buildings(w, bot.u4_player, *ptr_desired)) {
    network::OrderModel order;
    order.str_order = "StartProduction";
    order.b_has_subject = true;
    order.u4_subject = w.vec_players[bot.u4_player].u4_actor_id;
    order.opt_str_target = *ptr_desired;
    order.u4_extra_data = 1;
    bot_queue_order_for(bot, std::move(order));
  }
}

void unit_builder_tick(WorldModel& w, BotModel& bot, BotModuleState& state,
                       const BotModuleModel& module) {
  const production::PlayerProductionState* ptr_state{
      bot.u4_player < w.prod.vec_players.size() ? &w.prod.vec_players[bot.u4_player]
                                                : nullptr};
  if (ptr_state == nullptr) return;

  // PERF 面板：现金+矿低于门槛不出单（UnitBuilderBotModule.cs:88-90）。
  if (ptr_state->res.int4_cash + ptr_state->res.int4_resources <
      kv_int_of(module.vec_kv, "ProductionMinCashRequirement", 500))
    return;

  ++state.int4_state;
  if (state.int4_state % kFeedbackTime != 0) return;

  // 外部请求首项（HarvesterBotModule 的 harv 补产请求）：找该单位 Buildable 队列
  // 的空闲生产建筑出单（BuildUnit，UnitBuilderBotModule.cs:123-148）。
  if (!state.vec_str_pending.empty()) {
    const std::string str_unit{state.vec_str_pending.front()};
    const TraitTemplate* ptr_buildable{template_of(w, str_unit, "Buildable")};
    if (ptr_buildable != nullptr) {
      for (const std::string_view str_q :
           split_list_view(kv_of_template(*ptr_buildable, "Queue"))) {
        const std::vector<std::uint32_t> vec_queues{
            free_queues_of_category(w, bot.u4_player, str_q)};
        if (!vec_queues.empty()) {
          network::OrderModel order;
          order.str_order = "StartProduction";
          order.b_has_subject = true;
          order.u4_subject = w.vec_players[bot.u4_player].u4_actor_id;
          order.opt_str_target = str_unit;
          order.u4_extra_data = 1;
          bot_queue_order_for(bot, std::move(order));
          break;
        }
      }
    }
    state.vec_str_pending.erase(state.vec_str_pending.begin());
  }

  // IdleBaseUnitsMaximum：normal 未设（-1）→ 恒过（UnitBuilderBotModule.cs:107）。
  const std::int32_t int4_idle_max{kv_int_of(module.vec_kv, "IdleBaseUnitsMaximum", -1)};
  if (int4_idle_max > 0) {
    // idle 单位计数依赖 SquadManager 的 IBotNotifyIdleBaseUnits 喂给——T35 接线，
    // 未接线面视为 0（上游初始值同为 0）。
  }
  std::size_t u_current{static_cast<std::size_t>(state.int4_state2)};
  const std::vector<std::string> vec_queues{
      [&module]() {
        const std::string_view str_value{kv_of(module.vec_kv, "UnitQueues")};
        if (!str_value.empty()) return split_ws(str_value);
        return std::vector<std::string>{"Vehicle", "Infantry", "Plane", "Ship", "Aircraft"};
      }()};
  if (vec_queues.empty()) return;
  for (std::size_t idx{0}; idx < vec_queues.size(); ++idx) {
    ++u_current;
    if (u_current >= vec_queues.size()) u_current = 0;
    // currentQueueIndex 持久推进（无论是否出单——上游每次 Feedback 从上次位 ++）。
    state.int4_state2 = static_cast<std::int32_t>(u_current);
    const std::vector<std::uint32_t> vec_free{
        free_queues_of_category(w, bot.u4_player, vec_queues[u_current])};
    if (!vec_free.empty()) {
      unit_builder_build_random(w, bot, module, vec_queues[u_current]);
      break;
    }
  }
}

// —— HarvesterBotModule（HarvesterBotModule.cs:132-172）——

void harvester_tick(WorldModel& w, BotModel& bot, BotModuleState& state,
                    const BotModuleModel& module) {
  --bot.int4_harv_attack_cooldown;

  // resourceLayer 空 → 早退（BotTick 头两行）。C++ 面无资源层装载（economy 未
  // setup）时资源面视为空。
  if (w.econ.layer.vec_cells.empty() || w.econ.layer.int4_res_cells <= 0) return;

  // firstTick 的 resourceMapModule 收集（未实现模块，无 rng）。

  // HarvestIfAble 面（HarvesterBotModule.cs:149-151/229-293）：上游经
  // harvestersNeedingOrders 栈逐 harv 出单，每 tick 至多一次 FindNextResource
  // （PERF 注释）；C++ per-harv wrapper 栈缺——最小等价=扫在世 idle harv（活动
  // 链空 = 上游 IsIdle；FDA 运行中/LastSearchFailed 面缺——C++ FDA 完成即 idle）。
  bool b_searched{false};
  for (economy::HarvState& harv : w.econ.vec_harvs) {
    if (b_searched) break;
    const ActorModel* ptr_actor{world_actor_by_id(w, harv.u4_actor)};
    if (ptr_actor == nullptr || ptr_actor->b_disposed || !ptr_actor->b_in_world)
      continue;
    if (ptr_actor->u4_owner != bot.u4_player) continue;
    if (harv.int4_root != -1) continue;

    if (harv.int4_no_res_cooldown > 1) {
      --harv.int4_no_res_cooldown;
      continue;
    }

    const std::optional<pathfinding::CPos> opt_patch{
        economy::harv_find_next_resource(w, harv)};
    if (opt_patch.has_value()) {
      network::OrderModel order;
      order.str_order = "Harvest";
      order.b_has_subject = true;
      order.u4_subject = harv.u4_actor;
      order.target.kind = network::TargetKind::kTerrain;
      order.target.b_cell = true;
      order.target.int4_cell_bits =
          (opt_patch->int4_x << 20) | (opt_patch->int4_y << 8);
      bot_queue_order_for(bot, std::move(order));
      b_searched = true;
    } else {
      harv.int4_no_res_cooldown =
          kv_int_of(module.vec_kv, "ScanIntervalMultiplerWhenNoResources", 5);
      b_searched = true;
    }
  }

  if (--state.int4_state <= 0) {
    state.int4_state = kv_int_of(module.vec_kv, "ScanForIdleHarvestersInterval", 50);

    const std::vector<std::string> vec_harv_types{
        split_ws(kv_of(module.vec_kv, "HarvesterTypes"))};
    if (!vec_harv_types.empty() && module_enabled(bot, "UnitBuilderBotModule")) {
      const std::int32_t int4_harvs{
          count_owned_of_type(w, bot.u4_player, vec_harv_types.front())};
      std::int32_t int4_refineries{0};
      for (const std::string_view str_ref : split_ws(kv_of(module.vec_kv, "RefineryTypes")))
        int4_refineries += count_owned_of_type(w, bot.u4_player, str_ref);
      const std::int32_t int4_initial{
          kv_int_of(module.vec_kv, "InitialHarvesters", 4)};
      if (int4_harvs < int4_initial || int4_harvs < int4_refineries) {
        // HarvesterTypes.Random(world.LocalRandom)——单元素集即 1 抽（Random 的
        // Next(count) 消耗在任意基数下发生）。
        const std::int32_t int4_pick{support::next(
            w.rng_local, static_cast<std::int32_t>(vec_harv_types.size()))};
        const std::string str_type{vec_harv_types[static_cast<std::size_t>(int4_pick)]};
        BotModuleState* ptr_builder{module_state_of(bot, "UnitBuilderBotModule")};
        if (ptr_builder != nullptr) {
          bool b_requested{false};
          for (const std::string& str_pending : ptr_builder->vec_str_pending)
            if (str_pending == str_type) b_requested = true;
          if (!b_requested) ptr_builder->vec_str_pending.push_back(str_type);
        }
      }
    }
  }

  // scanForLowEffectHarvesters：依赖 ResourceMapBotModule（未实现）→ 空转。
  if (--state.int4_state2 <= 0)
    state.int4_state2 = kv_int_of(module.vec_kv, "ScanForLowEffectHarvestersInterval", 433);
}

// —— PowerDownBotModule（PowerDownBotManager.cs:107-148）——

// GetTogglePowerChanging：RA 的 PowerDownTypes 全为耗电防御建筑（Power.Amount<0）
// 且 PowerMultiplier@POWERDOWN Modifier=0——powerChanging = amount*(0-100)/100 =
// -amount（停电后净增电力）；条件未授予时 PowerMultiplier disabled 不取反。
std::int32_t toggle_power_changing(const WorldModel& w, const ActorModel& actor) {
  std::int32_t int4_amount{0};
  bool b_has_power{false};
  for (const std::uint32_t u4_handle : actor.vec_trait_handles)
    if (w.vec_traits[u4_handle].str_kind == "Power") {
      int4_amount += w.vec_traits[u4_handle].int4_param;
      b_has_power = true;
    }
  if (!b_has_power || int4_amount == 0) return 0;
  return -int4_amount;
}

void power_down_tick(WorldModel& w, BotModel& bot, BotModuleState& state,
                     const BotModuleModel& module) {
  if (state.int4_state > 0) {
    --state.int4_state;
    return;
  }

  const production::PlayerProductionState* ptr_state{
      bot.u4_player < w.prod.vec_players.size() ? &w.prod.vec_players[bot.u4_player]
                                                : nullptr};
  if (ptr_state == nullptr) return;

  const std::int32_t int4_excess{
      ptr_state->power.int4_power_provided - ptr_state->power.int4_power_drained};
  std::vector<std::uint32_t> vec_toggling;

  if (int4_excess > 0 && !state.vec4u_toggled.empty()) {
    // 复电：ExpectedPowerChanging 降序尝试（上游 List.Sort 比较器取反=降序）。
    std::vector<std::size_t> vec_order(state.vec4u_toggled.size());
    for (std::size_t idx{0}; idx < vec_order.size(); ++idx) vec_order[idx] = idx;
    std::stable_sort(vec_order.begin(), vec_order.end(),
                     [&](std::size_t a, std::size_t b) {
                       return state.vec4_toggled_power[a] > state.vec4_toggled_power[b];
                     });
    std::int32_t int4_power{int4_excess};
    for (const std::size_t idx : vec_order) {
      if (idx >= state.vec4u_toggled.size()) break;
      const std::uint32_t u4_actor{state.vec4u_toggled[idx]};
      const ActorModel* ptr_actor{world_actor_by_id(w, u4_actor)};
      if (ptr_actor == nullptr || ptr_actor->b_disposed ||
          ptr_actor->u4_owner != bot.u4_player) {
        // isTogglableValid 失效剔除（RemoveAll）。
        state.vec4u_toggled.erase(state.vec4u_toggled.begin() + idx);
        state.vec4_toggled_power.erase(state.vec4_toggled_power.begin() + idx);
        continue;
      }
      if (int4_power + state.vec4_toggled_power[idx] < 0) continue;
      vec_toggling.push_back(u4_actor);
      int4_power += state.vec4_toggled_power[idx];
      state.vec4u_toggled.erase(state.vec4u_toggled.begin() + idx);
      state.vec4_toggled_power.erase(state.vec4_toggled_power.begin() + idx);
    }
  } else if (int4_excess < 0) {
    // 停电：GetOnlineBuildings 按 powerChanging 升序停电直到电力回正。
    struct OnlineBuilding {
      std::uint32_t u4_actor;
      std::int32_t int4_changing;
    };
    std::vector<OnlineBuilding> vec_online;
    const std::vector<std::string> vec_types{split_ws(kv_of(module.vec_kv, "PowerDownTypes"))};
    for (const std::uint32_t u4_id : w.vec_in_world) {
      const ActorModel& actor{w.vec_actors_all[u4_id]};
      if (actor.u4_owner != bot.u4_player) continue;
      bool b_match{false};
      for (const std::string& str_type : vec_types)
        if (actor.str_type == str_type) b_match = true;
      if (!b_match) continue;
      const std::int32_t int4_changing{toggle_power_changing(w, actor)};
      if (int4_changing > 0) vec_online.push_back({u4_id, int4_changing});
    }
    std::stable_sort(vec_online.begin(), vec_online.end(),
                     [](const OnlineBuilding& a, const OnlineBuilding& b) {
                       return a.int4_changing < b.int4_changing;
                     });
    std::int32_t int4_power{int4_excess};
    for (const OnlineBuilding& online : vec_online) {
      if (int4_power > 0) break;
      vec_toggling.push_back(online.u4_actor);
      state.vec4u_toggled.push_back(online.u4_actor);
      state.vec4_toggled_power.push_back(-online.int4_changing);
      int4_power += online.int4_changing;
    }
  }

  if (!vec_toggling.empty()) {
    network::OrderModel order;
    order.str_order = kv_of(module.vec_kv, "PowerDownOrder");
    if (order.str_order.empty()) order.str_order = "PowerDown";
    order.b_has_grouped = true;
    order.vec_grouped = std::move(vec_toggling);
    bot_queue_order_for(bot, std::move(order));
  }
  state.int4_state = kv_int_of(module.vec_kv, "Interval", 150);
}

}  // namespace

bool bot_type_declared(const WorldModel& w, std::string_view str_bot_type) {
  const ActorTypeDef* ptr_def{player_type_def(w)};
  if (ptr_def == nullptr) return false;
  for (const TraitTemplate& tpl : ptr_def->vec_templates) {
    if (tpl.str_kind != "ModularBot") continue;
    // ModularBotInfo.Type（TraitSpec 无注册——str_param_type 装载 Type 字段）。
    if (kv_of(tpl.vec_kv, "Type") == str_bot_type) return true;
  }
  return false;
}

std::vector<BotModuleModel> bot_modules_from_actor_types(const WorldModel& w,
                                                         std::string_view str_bot_type) {
  std::vector<BotModuleModel> vec_modules;
  const ActorTypeDef* ptr_def{player_type_def(w)};
  if (ptr_def == nullptr) return vec_modules;

  std::map<std::string, std::vector<std::string>> map_condition_bots;
  for (const TraitTemplate& tpl : ptr_def->vec_templates) {
    if (tpl.str_kind != "GrantConditionOnBotOwner") continue;
    const std::string str_condition{std::string{kv_of(tpl.vec_kv, "Condition")}};
    if (str_condition.empty()) continue;
    map_condition_bots[str_condition] = split_ws(kv_of(tpl.vec_kv, "Bots"));
  }

  for (const TraitTemplate& tpl : ptr_def->vec_templates) {
    bool b_known{false};
    for (const std::string_view str_kind : kBotModuleKinds)
      if (tpl.str_kind == str_kind) b_known = true;
    if (!b_known) continue;

    BotModuleModel module;
    module.str_kind = tpl.str_kind;
    module.str_instance = tpl.str_instance;
    module.vec_kv = tpl.vec_kv;
    module.vec_b_bot_types = bot_types_of_condition(tpl.vec_kv, map_condition_bots);
    bool b_enabled{false};
    for (const std::string& str_type : module.vec_b_bot_types)
      if (str_type == str_bot_type) b_enabled = true;
    if (b_enabled) vec_modules.push_back(std::move(module));
  }
  return vec_modules;
}

void bot_respond_to_attack_all(WorldModel& w, std::uint32_t u4_player, std::uint32_t u4_self,
                               std::uint32_t u4_attacker, std::int32_t int4_prev_ds,
                               std::int32_t int4_ds) {
  // DamageState：Undamaged=0/Light=25%/Medium=50%/Heavy=75%/Critical=100%/Dead。
  constexpr std::int32_t kLight{1};
  for (BotModel& bot : w.vec_bots) {
    if (bot.u4_player != u4_player || !bot.b_enabled) continue;
    BotModuleState* ptr_repair{module_state_of(bot, "BuildingRepairBotModule")};
    if (ptr_repair == nullptr) continue;
    const std::vector<BotModuleModel> vec_modules{bot_modules_from_actor_types(w, bot.str_type)};
    const BotModuleModel* ptr_module{nullptr};
    for (const BotModuleModel& module : vec_modules)
      if (module.str_kind == "BuildingRepairBotModule") ptr_module = &module;
    if (ptr_module == nullptr) continue;

    // 周期全建筑维修（BuildingRepairBotModule.cs:34-57）：RepairAllBuildingsCoolDown
    // >= 0 且 prevTicks + coolDown < WorldTick 时全扫（单点入参面=直发当前建筑单）。
    const std::int32_t int4_cool{
        kv_int_of(ptr_module->vec_kv, "RepairAllBuildingsCoolDown", 107)};
    if (int4_cool >= 0 && ptr_repair->int4_state + int4_cool < w.int4_world_tick) {
      ptr_repair->int4_state = w.int4_world_tick;
      network::OrderModel order;
      order.str_order = "RepairBuilding";
      order.b_has_subject = true;
      order.u4_subject = w.vec_players[u4_player].u4_actor_id;
      order.target.kind = network::TargetKind::kActor;
      order.target.u4_actor = u4_self;
      bot_queue_order_for(bot, std::move(order));
      return;
    }

    // 单点判定（:60-72）：Dead/中立攻击者早退；DamageState 跨过 Light 阈值出单。
    const ActorModel* ptr_self{world_actor_by_id(w, u4_self)};
    if (ptr_self == nullptr || ptr_self->b_disposed) return;
    if (u4_attacker == kOwnerNone) return;
    const ActorModel* ptr_attacker{world_actor_by_id(w, u4_attacker)};
    if (ptr_attacker == nullptr) return;
    const bool b_neutral{ptr_attacker->u4_owner == w.u4_world_owner_player ||
                         ptr_attacker->u4_owner == u4_player};
    if (b_neutral) return;
    if (int4_ds > kLight && int4_prev_ds <= kLight) {
      network::OrderModel order;
      order.str_order = "RepairBuilding";
      order.b_has_subject = true;
      order.u4_subject = w.vec_players[u4_player].u4_actor_id;
      order.target.kind = network::TargetKind::kActor;
      order.target.u4_actor = u4_self;
      bot_queue_order_for(bot, std::move(order));
    }
  }
}

// 模块 tick 序（ModularBot.Activate 的 TraitsImplementing<IBotTick> 声明序遍历；
// BuildingRepair 为 IBotRespondToAttack 不在 tick 序）。决策全部不触碰 rng_shared
// （RunUnsynced 等价断言由测试侧承担）。
void bot_modules_tick(WorldModel& w, BotModel& bot,
                      const std::vector<BotModuleModel>& vec_modules) {
  for (const BotModuleModel& module : vec_modules) {
    BotModuleState* ptr_state{module_state_of(bot, module.str_kind)};
    if (ptr_state == nullptr) continue;
    if (module.str_kind == "HarvesterBotModule")
      harvester_tick(w, bot, *ptr_state, module);
    else if (module.str_kind == "PowerDownBotModule")
      power_down_tick(w, bot, *ptr_state, module);
    else if (module.str_kind == "UnitBuilderBotModule")
      unit_builder_tick(w, bot, *ptr_state, module);
  }
}

// TraitEnabled 激活抽（声明序）：Harvester 双抽（Next(scanIdle)+Next(scanLow)）→
// PowerDown 单抽（Next(Interval)）。未实现模块（BaseBuilder 3 抽/SquadManager/
// McvExpansion 3 抽）缺席——rng_local 序位偏离登记。
void bot_modules_trait_enabled(WorldModel& w, BotModel& bot,
                               const std::vector<BotModuleModel>& vec_modules) {
  for (const BotModuleModel& module : vec_modules) {
    BotModuleState* ptr_state{module_state_of(bot, module.str_kind)};
    if (ptr_state == nullptr) continue;
    if (module.str_kind == "HarvesterBotModule") {
      ptr_state->int4_state = support::next(
          w.rng_local,
          kv_int_of(module.vec_kv, "ScanForIdleHarvestersInterval", 50));
      ptr_state->int4_state2 = support::next(
          w.rng_local,
          kv_int_of(module.vec_kv, "ScanForLowEffectHarvestersInterval", 433));
    } else if (module.str_kind == "PowerDownBotModule") {
      ptr_state->int4_state =
          support::next(w.rng_local, kv_int_of(module.vec_kv, "Interval", 150));
    }
  }
}

// ai.yaml Player: 段的 bot 族 trait 装载占位：无运行钩子（bot 决策不作为 player
// actor 的 tick trait 跑——bot_tick_all 独立段驱动，D091③），仅为规则装载与
// vec_kv 数据面（模块表/参数）提供注册。
const std::vector<TraitSpec>& bot_trait_specs() {
  static const std::vector<TraitSpec> vec_specs{
      TraitSpec{.str_kind = "ModularBot", .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "GrantConditionOnBotOwner",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "ResourceMapBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "SupportPowerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "HarvesterBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "BaseBuilderBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "PowerDownBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "BuildingRepairBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "SquadManagerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "McvManagerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "McvExpansionManagerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "UnitBuilderBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "MinelayerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
      TraitSpec{.str_kind = "CaptureManagerBotModule",
                .fn_create = [](const TraitCreateCtx&, TraitModel&) {}},
  };
  return vec_specs;
}

}  // namespace ora::game::world
