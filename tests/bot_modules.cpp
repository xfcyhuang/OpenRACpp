import std;

import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.test.util;
using ora::test::check;

// T34 生产类 BotModule（issue #53）：模块装载面/决策面（rng_local 消耗序）/
// 出单管道（D091⑤）的单元验收。ai.yaml 语义锚：mods/ra/rules/ai.yaml（normal
// 配置）+ ModularBot.cs + 四模块源文件。

namespace {

constexpr std::int32_t kSeed{1234};

// ai.yaml normal 配置的最小语义面（参数值取 ai.yaml 实值；RequiresCondition 覆盖
// 析取与单条件两形）。资源世界单位：e1/harv（Buildable）、barr/weap（Production）、
// proc/tsla（Power）。
constexpr std::string_view kRules{
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "\tRngProbe:\n"
    "player:\n"
    "\tClassicProductionQueue@Vehicle:\n"
    "\t\tType: Vehicle\n"
    "\tClassicProductionQueue@Infantry:\n"
    "\t\tType: Infantry\n"
    "\tModularBot@NormalAI:\n"
    "\t\tType: normal\n"
    "\tModularBot@RushAI:\n"
    "\t\tType: rush\n"
    "\tGrantConditionOnBotOwner@normal:\n"
    "\t\tCondition: enable-normal-ai\n"
    "\t\tBots: normal\n"
    "\tGrantConditionOnBotOwner@rush:\n"
    "\t\tCondition: enable-rush-ai\n"
    "\t\tBots: rush\n"
    "\tHarvesterBotModule@normal-turtle:\n"
    "\t\tRequiresCondition: enable-normal-ai\n"
    "\t\tHarvesterTypes: harv\n"
    "\t\tRefineryTypes: proc\n"
    "\t\tInitialHarvesters: 4\n"
    "\tPowerDownBotModule:\n"
    "\t\tRequiresCondition: enable-normal-ai || enable-rush-ai\n"
    "\t\tPowerDownTypes: tsla\n"
    "\t\tInterval: 150\n"
    "\tBuildingRepairBotModule:\n"
    "\t\tRequiresCondition: enable-normal-ai || enable-rush-ai\n"
    "\tUnitBuilderBotModule@normal:\n"
    "\t\tRequiresCondition: enable-normal-ai\n"
    "\t\tUnitsToBuild:\n"
    "\t\t\te1: 65\n"
    "\t\t\tharv: 10\n"
    "\t\tUnitLimits:\n"
    "\t\t\tharv: 8\n"
    "\tUnitBuilderBotModule@rush:\n"
    "\t\tRequiresCondition: enable-rush-ai\n"
    "\t\tUnitsToBuild:\n"
    "\t\t\te1: 65\n"
    "e1:\n"
    "\tValued:\n"
    "\t\tCost: 100\n"
    "\tBuildable:\n"
    "\t\tQueue: Infantry\n"
    "harv:\n"
    "\tHarvester:\n"
    "\tValued:\n"
    "\t\tCost: 100\n"
    "\tBuildable:\n"
    "\t\tQueue: Vehicle\n"
    "barr:\n"
    "\tProduction:\n"
    "\t\tProduces: Infantry\n"
    "weap:\n"
    "\tProduction:\n"
    "\t\tProduces: Vehicle\n"
    "proc:\n"
    "\tPower:\n"
    "\t\tAmount: -30\n"
    "tsla:\n"
    "\tPower:\n"
    "\t\tAmount: -150\n"};

constexpr std::string_view kMapYaml{
    "Title: bot-modules\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"};

std::expected<ora::game::world::WorldModel, std::string> make_world(
    const std::string_view str_bot_type, const bool b_is_replay = false) {
  auto res_map{ora::game::world::synthetic_map_from_yaml(kRules, kMapYaml)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "soviet");
  if (!str_bot_type.empty()) lobby.vec_slot_bots.emplace_back("Multi0", std::string(str_bot_type));
  auto res_world{ora::game::world::world_create(*res_map, kSeed, lobby, b_is_replay)};
  if (!res_world.has_value()) return res_world;
  // 资源层非空面（HarvesterBotModule 的 resourceLayer.IsEmpty 早退门）：决策面
  // 只查计数，塞最小非空面。
  ora::game::world::WorldModel& w{*res_world};
  w.econ.layer.int4_res_cells = 1;
  w.econ.layer.vec_cells.resize(1);
  return res_world;
}

std::uint32_t spawn(ora::game::world::WorldModel& w, std::string_view str_type) {
  auto res{ora::game::world::create_actor(w, str_type, 1, 5, 5, true)};
  check(res.has_value(), std::format("spawn {} loads", str_type));
  return res.has_value() ? *res : 0xffffffffu;
}

ora::game::world::BotModel* bot_of(ora::game::world::WorldModel& w) {
  for (auto& bot : w.vec_bots)
    if (bot.b_enabled) return &bot;
  return nullptr;
}

const ora::game::world::BotModuleState* state_of(
    const ora::game::world::BotModel& bot, std::string_view str_kind) {
  for (const auto& state : bot.vec_module_states)
    if (state.str_kind == str_kind) return &state;
  return nullptr;
}

}  // namespace

int main() {
  // —— 装载面：模块表按 bot 类型过滤（GrantConditionOnBotOwner 直配等价）——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "normal bot world loads");
    if (res.has_value()) {
      auto& w{*res};
      check(w.vec_bots.size() == 1, "one bot activated");
      const auto& bot{w.vec_bots.front()};
      check(bot.vec_module_states.size() == 4, std::format("normal enables 4 modules (got {})", bot.vec_module_states.size()));
      // tick 序 = ai.yaml 声明序（Harvester→PowerDown→UnitBuilder；BuildingRepair
      // 为 RespondToAttack 模块占位其序位之后）。
      check(bot.vec_module_states[0].str_kind == "HarvesterBotModule" &&
                bot.vec_module_states[1].str_kind == "PowerDownBotModule" &&
                bot.vec_module_states[2].str_kind == "BuildingRepairBotModule" &&
                bot.vec_module_states[3].str_kind == "UnitBuilderBotModule",
            "module order: ai.yaml declaration order");
      // 激活抽（TraitEnabled 声明序）：Harvester 双抽 + PowerDown 单抽 = 恰 3 次。
      check(w.rng_local.int4_total_count == 3,
            std::format("activate consumes rng_local exactly 3 (got {})",
                        w.rng_local.int4_total_count));
      // RunUnsynced 等价：激活/决策不碰同步流（RngProbe 尚未 tick）。
      check(w.rng_shared.int4_total_count == 0, "rng_shared untouched at activate");
    }
  }
  {
    // rush：UnitBuilder@rush 命中（enable-rush-ai）而 normal-turtle Harvester 不命中。
    auto res{make_world("rush")};
    check(res.has_value(), "rush bot world loads");
    if (res.has_value()) {
      const auto& bot{res->vec_bots.front()};
      bool b_harv{false}, b_ub{false};
      for (const auto& state : bot.vec_module_states) {
        if (state.str_kind == "HarvesterBotModule") b_harv = true;
        if (state.str_kind == "UnitBuilderBotModule") b_ub = true;
      }
      check(!b_harv && b_ub, "rush: Harvester off, UnitBuilder on");
      check(res->rng_local.int4_total_count == 1,
            "rush activate: PowerDown only draw = 1");
    }
  }
  {
    // Invalid bot type 诊断面（Player.cs:220-221）：无 ModularBot 声明 → 不激活。
    auto res{make_world("crazy")};
    check(res.has_value(), "world with unknown bot type loads");
    if (res.has_value())
      check(res->vec_bots.empty(), "invalid bot type does not activate");
    check(!ora::game::world::bot_type_declared(*res, "crazy"), "crazy undeclared");
  }

  // —— UnitBuilder 决策面：FeedbackTime 节奏 + rng_local Shuffle 序 + 出单 ——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "unit builder world loads");
    if (res.has_value()) {
      auto& w{*res};
      (void)spawn(w, "barr");
      const std::int32_t int4_count_after_spawn{w.rng_local.int4_total_count};
      check(w.rng_local.int4_total_count == int4_count_after_spawn,
            "spawning barracks consumes no rng_local");

      // cash：lobby 默认 5000 >= 500 → tick 1 起计数；f30 首次出单尝试。
      // UnitQueues 默认 [Vehicle,Infantry,Plane,Ship,Aircraft]——首 Feedback 轮转
      // 到 Infantry（idx1，barr 在世）→ Shuffle(buildable=2: e1+harv? harv 的
      // Queue=Vehicle 不在 Infantry 队列 → buildable={e1} → Shuffle 1 元素 0 抽)。
      // e1 error = 0*100/0-65 = -1 <0 → 直接出单 e1（allUnits=0 分支 error=-1）。
      std::size_t u_orders{};
      for (std::int32_t f{0}; f < 31; ++f) {
        ora::game::world::world_tick(w);
        u_orders = w.vec_bot_issued_orders.size();
      }
      check(u_orders == 1, std::format("f30: exactly 1 StartProduction issued (got {})", u_orders));
      if (u_orders == 1) {
        const auto& order{w.vec_bot_issued_orders.front()};
        check(order.str_order == "StartProduction" && order.b_has_subject &&
                  order.u4_subject == w.vec_players[1].u4_actor_id &&
                  order.opt_str_target == "e1" && order.u4_extra_data == 1,
              "order is StartProduction(player-actor, e1, 1)");
      }
      // Shuffle 消耗：buildable 集过滤 UnitsToBuild 后={e1}（harv 不在 Infantry
      // 队列），1 元素 ShuffleInPlace 不抽 → rng_local 消耗仍为激活 3 次。
      check(w.rng_local.int4_total_count == 3,
            std::format("no shuffle draws for singleton buildable (got {})",
                        w.rng_local.int4_total_count));
      // rng_shared 零消耗（RunUnsynced 等价断言；RngProbe 每 tick 1 抽作基准）。
      check(w.rng_shared.int4_total_count == 31,
            std::format("rng_shared consumed only by RngProbe (31 ticks): {}",
                        w.rng_shared.int4_total_count));
    }
  }
  {
    // 出单管道（D091⑤）：ptr_order_manager 挂接 → bot 单投 issue_order 进
    // om.vec_local（经 run_frame 的 echo 回环 resolve）；vec_bot_issued_orders
    // 为账本。
    auto res{make_world("normal")};
    check(res.has_value(), "pipeline world loads");
    if (res.has_value()) {
      auto& w{*res};
      (void)spawn(w, "barr");
      ora::game::network::OrderManagerModel om;
      const std::array vec_clients{ora::game::network::kLocalClientId};
      ora::game::network::order_manager_start_game(om, w, vec_clients);
      w.ptr_order_manager = &om;

      for (std::int32_t f{0}; f < 34; ++f)
        ora::game::network::run_frame(om, w);

      // f30 出单入队 → 节流出单投 om.vec_local → send_orders（f31 打包 f32
      // 应用）。34 帧后 om 侧已发送。
      check(!w.vec_bot_issued_orders.empty(), "bot order ledger non-empty");
      check(om.vec_local.empty() && !w.vec_bot_issued_orders.empty(),
            "orders flushed from om.vec_local after send");
      // StartProduction 已被世界 resolve（barr 队列 vec_queue 有 e1 项）。
      const auto* ptr_state{[&]() -> const ora::game::production::ProductionQueueState* {
        for (const auto& queue : w.prod.vec_players[1].vec_queues)
          if (queue.str_type == "infantry") return &queue;
        return nullptr;
      }()};
      check(ptr_state != nullptr && !ptr_state->vec_queue.empty() &&
                ptr_state->vec_queue.front().str_item == "e1",
            "StartProduction resolved into barr queue (e1 queued)");
    }
  }
  {
    // 节流公式与管道叠加：13 单入队 → 首 tick ceil(13/5)=3。
    auto res{make_world("normal")};
    if (res.has_value()) {
      auto& w{*res};
      ora::game::network::OrderModel order;
      order.str_order = "Move";
      order.b_has_subject = true;
      order.u4_subject = 0;
      auto* ptr_bot{bot_of(w)};
      check(ptr_bot != nullptr, "bot present");
      if (ptr_bot != nullptr) {
        for (std::int32_t idx{0}; idx < 13; ++idx)
          ptr_bot->deque_orders.push_back(order);
        ora::game::world::world_tick(w);
        check(ptr_bot->deque_orders.size() == 10 && w.vec_bot_issued_orders.size() == 3,
              "throttle: 13 queued -> issue 3, keep 10");
        check(ptr_bot->u4_issued_total == 3, "issued total tracked");
      }
    }
  }

  // —— Harvester 决策面：harv 补产请求链（Random 消耗 + 跨模块 RequestUnitProduction）——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "harvester module world loads");
    if (res.has_value()) {
      auto& w{*res};
      spawn(w, "proc");
      auto* ptr_bot{bot_of(w)};
      check(ptr_bot != nullptr, "bot present");
      const std::int32_t int4_scan0{[&] {
        const auto* ptr_state{state_of(*ptr_bot, "HarvesterBotModule")};
        return ptr_state != nullptr ? ptr_state->int4_state : -1;
      }()};
      check(int4_scan0 >= 0 && int4_scan0 <= 50, std::format("scan idle seeded in [0,50): {}", int4_scan0));

      // 跑到 scan idle 归零（<=50 tick）：harv(0) < InitialHarvesters(4) →
      // Random(1) 1 抽（rng_local 第 4 抽）+ UnitBuilder.vec_str_pending=[harv]。
      std::int32_t f{0};
      for (; f < 60; ++f) {
        ora::game::world::world_tick(w);
        const auto* ptr_ub{state_of(*ptr_bot, "UnitBuilderBotModule")};
        if (ptr_ub != nullptr && !ptr_ub->vec_str_pending.empty()) break;
      }
      check(f < 60, std::format("harv replacement requested within 60 ticks (f={})", f));
      // Random(单元素集)=Next(0,1)：diff<=1 早退不抽（上游 MT 同构）——消耗序
      // 仍是激活 3 抽。
      check(w.rng_local.int4_total_count == 3,
            std::format("Random(singleton harvTypes) draws nothing (total 3, got {})",
                        w.rng_local.int4_total_count));
      const auto* ptr_ub{state_of(*ptr_bot, "UnitBuilderBotModule")};
      check(ptr_ub != nullptr && ptr_ub->vec_str_pending.size() == 1 &&
                ptr_ub->vec_str_pending.front() == "harv",
            "UnitBuilder pending carries harv request");

      // 请求出单：weap 在世（Vehicle 队列）→ 下个 Feedback 出 StartProduction
      // (weap, harv)。proc 的 Buildable 队列（Vehicle）空闲即可——用 weap 建筑。
      spawn(w, "weap");
      std::size_t u_harv_orders{};
      for (std::int32_t idx{0}; idx < 31; ++idx) {
        ora::game::world::world_tick(w);
        for (const auto& order : w.vec_bot_issued_orders)
          if (order.opt_str_target == "harv") ++u_harv_orders;
      }
      check(u_harv_orders >= 1, std::format("harv StartProduction issued (got {})", u_harv_orders));
      check(ptr_ub != nullptr && ptr_ub->vec_str_pending.empty(),
            "request consumed after BuildUnit");
    }
  }

  // —— PowerDown 决策面：低电停电单（grouped 订单）+ Interval 节流 ——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "powerdown world loads");
    if (res.has_value()) {
      auto& w{*res};
      spawn(w, "tsla");
      spawn(w, "proc");
      // excess = 0 - (150+30) = -180 < 0 → 首个到期 tick 出停电单（grouped tsla）。
      auto* ptr_bot{bot_of(w)};
      check(ptr_bot != nullptr, "bot present");
      std::int32_t f{0};
      const ora::game::network::OrderModel* ptr_power_order{nullptr};
      for (; f < 160; ++f) {
        ora::game::world::world_tick(w);
        for (const auto& order : w.vec_bot_issued_orders)
          if (order.str_order == "PowerDown") ptr_power_order = &order;
        if (ptr_power_order != nullptr) break;
      }
      check(ptr_power_order != nullptr, std::format("PowerDown order issued within 160 ticks (f={})", f));
      if (ptr_power_order != nullptr) {
        check(!ptr_power_order->b_has_subject && ptr_power_order->b_has_grouped &&
                  ptr_power_order->vec_grouped.size() == 1,
              "PowerDown order: no subject, grouped 1 actor");
      }
      const auto* ptr_state{state_of(*ptr_bot, "PowerDownBotModule")};
      check(ptr_state != nullptr && ptr_state->vec4u_toggled.size() == 1,
            "tsla tracked in toggled list");
      check(w.rng_local.int4_total_count == 3,
            "PowerDown decision draws no rng_local (total still 3)");
    }
  }

  // —— Harvest 决策/resolve 闭环（HarvesterBotModule.cs:229-293 + Harvester.cs:258-277）——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "harvest world loads");
    if (res.has_value()) {
      auto& w{*res};
      const std::uint32_t u4_harv{spawn(w, "harv")};
      // econ 未 setup（b_ready=false → economy_on_actor_added 早退）——手动建
      // HarvState（贴 economy_on_actor_added 的 harv 分支构建面）。
      ora::game::economy::HarvState harv_manual;
      harv_manual.u4_actor = u4_harv;
      harv_manual.cargo.int4_capacity = 20;
      harv_manual.move.cell_from = ora::game::pathfinding::CPos{5, 5};
      harv_manual.move.cell_to = harv_manual.move.cell_from;
      harv_manual.int4_root = 0;
      harv_manual.vec_activities.resize(1);
      harv_manual.vec_activities.front().kind = ora::game::economy::ActKind::kFindAndDeliver;
      w.econ.vec_harvs.push_back(std::move(harv_manual));
      ora::game::economy::HarvState& harv{w.econ.vec_harvs.back()};
      check(harv.int4_root != -1, "harv starts with FDA activity");

      // 寻矿失败路径：无资源场（econ 未 setup）→ idle harv 首个 bot tick 走
      // NoResourcesCooldown=ScanIntervalMultiplerWhenNoResources。
      harv.int4_root = -1;
      ora::game::world::world_tick(w);
      check(harv.int4_no_res_cooldown == 5, std::format("no-resource cooldown set to 5 (got {})",
            harv.int4_no_res_cooldown));

      // resolve 注入路径：手投 Harvest 单（上游 bot 单 target=FromCell）→ 活动链
      // 重置重挂 FDA + opt_order_loc 首轮中心。
      ora::game::network::OrderManagerModel om;
      const std::array<std::int32_t, 1> vec_clients{ora::game::network::kLocalClientId};
      ora::game::network::order_manager_start_game(om, w, vec_clients);
      w.ptr_order_manager = &om;
      ora::game::network::OrderModel order;
      order.str_order = "Harvest";
      order.b_has_subject = true;
      order.u4_subject = u4_harv;
      order.target.kind = ora::game::network::TargetKind::kTerrain;
      order.target.b_cell = true;
      order.target.int4_cell_bits = (27 << 20) | (9 << 8);
      ora::game::network::issue_order(om, order);
      for (std::int32_t f{0}; f < 4; ++f) ora::game::network::run_frame(om, w);
      check(harv.int4_root != -1 && !harv.vec_activities.empty() &&
                harv.vec_activities.front().kind == ora::game::economy::ActKind::kFindAndDeliver,
            "Harvest order resolves: FDA re-queued");
    }
  }

  // —— BuildingRepair 决策面：单点受击判定 + 周期冷却 ——
  {
    auto res{make_world("normal")};
    check(res.has_value(), "repair world loads");
    if (res.has_value()) {
      auto& w{*res};
      const std::uint32_t u4_powr{spawn(w, "proc")};
      auto* ptr_bot{bot_of(w)};
      check(ptr_bot != nullptr, "bot present");

      // 中立攻击者（Neutral 拥有的敌意面按 world owner 判定）→ 早退不出单。
      // 敌对判定面：attacker owner != self owner 且 != world owner。
      const std::uint32_t u4_creep_actor{[&w]() {
        auto res_spawn{ora::game::world::create_actor(w, "e1", 2, 6, 6, true)};
        check(res_spawn.has_value(), "creep e1 spawns");
        return res_spawn.value_or(0xffffffffu);
      }()};
      w.int4_world_tick = 10;
      ora::game::world::bot_respond_to_attack_all(w, 1, u4_powr, u4_creep_actor, 0, 2);
      check(ptr_bot->deque_orders.size() == 1 &&
                ptr_bot->deque_orders.front().str_order == "RepairBuilding" &&
                ptr_bot->deque_orders.front().target.u4_actor == u4_powr,
            "ds 0->2 crosses light threshold: RepairBuilding queued");

      // prevTicks 冷却：第二次受击在 107 内走单点判定（ds 无 Light 跨越不触发）。
      ora::game::world::bot_respond_to_attack_all(w, 1, u4_powr, u4_creep_actor, 2, 2);
      check(ptr_bot->deque_orders.size() == 1, "no re-queue while ds stays above light");
    }
  }

  return 0;
}
