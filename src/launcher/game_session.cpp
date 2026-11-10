module ora.launcher;

import std;
import ora.game;
import ora.game.combat;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.sound;
import ora.game.support;
import ora.game.world;
import ora.platforms;

namespace ora::launcher {

namespace {

namespace cmb = ora::game::combat;
namespace eco = ora::game::economy;
namespace inp = ora::game::input;
namespace net = ora::game::network;
namespace opro = ora::game::production;
namespace pfg = ora::game::pathfinding;
namespace ww = ora::game::world;

const ww::TraitTemplate* actor_template(const ww::WorldModel& w, const ww::ActorModel& actor,
                                        std::string_view str_kind) {
  for (const ww::ActorTypeDef& def : w.vec_actor_types) {
    if (def.str_name_lower != actor.str_type) continue;
    for (const ww::TraitTemplate& tpl : def.vec_templates)
      if (tpl.str_kind == str_kind) return &tpl;
  }
  return nullptr;
}

FighterState* fighter_of(GameSession& s, std::uint32_t u4_actor) {
  for (FighterState& fighter : s.vec_fighters)
    if (fighter.unit.u4_id == u4_actor) return &fighter;
  return nullptr;
}

bool actor_alive(const GameSession& s, std::uint32_t u4_actor) {
  if (u4_actor >= s.w.vec_actors_all.size()) return false;
  const ww::ActorModel& actor{s.w.vec_actors_all[u4_actor]};
  return actor.b_in_world && !actor.b_disposed;
}

void bridge_registry_sync(GameSession& s) {
  std::size_t idx_fighter{0};
  while (idx_fighter < s.vec_fighters.size()) {
    if (actor_alive(s, s.vec_fighters[idx_fighter].unit.u4_id)) {
      ++idx_fighter;
    } else {
      s.vec_fighters.erase(s.vec_fighters.begin() + static_cast<std::ptrdiff_t>(idx_fighter));
    }
  }

  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
    if (actor.b_disposed) continue;

    const bool b_armed{actor_template(s.w, actor, "AttackProbe") != nullptr};
    if (b_armed && fighter_of(s, u4_id) == nullptr) {
      FighterState fighter;
      fighter.unit.u4_id = u4_id;
      fighter.unit.str_type = actor.str_type;
      fighter.unit.u4_owner = actor.u4_owner;
      fighter.unit.pos_center = pfg::center_of_cell(pfg::CPos{actor.int4_pos_x, actor.int4_pos_y});
      fighter.unit.cell_from = pfg::CPos{actor.int4_pos_x, actor.int4_pos_y};
      fighter.unit.cell_to = fighter.unit.cell_from;
      fighter.unit.int4_quantized_facings = 8;
      fighter.unit.b_attack_follow = true;
      fighter.str_weapon = "carbine";
      const cmb::WeaponDef* ptr_weapon{s.weapons.find("carbine")};
      if (ptr_weapon != nullptr) {
        fighter.arm.ptr_weapon = ptr_weapon;
        fighter.arm.vec_barrel_offsets.emplace_back();
      }
      fighter.hp.int4_hp = 100;
      fighter.hp.int4_max_hp = 100;
      s.vec_fighters.push_back(std::move(fighter));
    }
  }
}

// 攻击桥喂单（D044 残留面）：Move 已世界内 resolve（订单管道），桥只消费 Attack。
void bridge_consume_orders(GameSession& s) {
  std::size_t idx{0};
  while (idx < s.vec_bridge_pending.size()) {
    const auto [u4_frame_apply, order]{s.vec_bridge_pending[idx]};
    if (static_cast<std::int32_t>(u4_frame_apply) > static_cast<std::int32_t>(s.w.u4_net_frame)) {
      ++idx;
      continue;
    }

    if (order.str_order == "Attack" && order.target.kind == net::TargetKind::kActor) {
      if (FighterState* ptr_fighter{fighter_of(s, order.u4_subject)}; ptr_fighter != nullptr) {
        ptr_fighter->u4_target = order.target.u4_actor;
        s.vec_events.push_back(
            std::format("order Attack id={} -> actor({})", order.u4_subject,
                        order.target.u4_actor));
      }
    }

    s.vec_bridge_pending.erase(s.vec_bridge_pending.begin() +
                               static_cast<std::ptrdiff_t>(idx));
  }
}

std::int64_t horizontal_distance_sq(pfg::WPos pos_a, pfg::WPos pos_b) {
  const std::int64_t int8_dx{pos_a.int4_x - pos_b.int4_x};
  const std::int64_t int8_dy{pos_a.int4_y - pos_b.int4_y};
  return int8_dx * int8_dx + int8_dy * int8_dy;
}

void bridge_fire(GameSession& s, FighterState& fighter, FighterState& target) {
  fighter.arm.vec_barrel_offsets.clear();
  fighter.arm.vec_barrel_offsets.emplace_back();
  const bool b_fired{cmb::armament_check_fire(
      fighter.unit, fighter.arm, target.unit.pos_center, true, true,
      [&](cmb::CombatUnit&, cmb::ArmamentState&, pfg::WPos, pfg::WPos pos_passive) {
        const cmb::WeaponDef& weapon{*fighter.arm.ptr_weapon};
        if (s.w.ptr_sfx != nullptr) {
          if (!weapon.vec_report.empty())
            sfx_on_weapon_fired(
                *s.w.ptr_sfx, weapon.vec_report,
                ora::platforms::wpos{.int4_x = fighter.unit.pos_center.int4_x,
                                     .int4_y = fighter.unit.pos_center.int4_y,
                                     .int4_z = fighter.unit.pos_center.int4_z});
          for (const cmb::WarheadDef& wh : weapon.vec_warheads)
            if (wh.kind == cmb::WarheadKind::kCreateEffect)
              sfx_on_warhead_impact(
                  *s.w.ptr_sfx,
                  ora::game::sfx_impact_sounds{.vec_explosions = wh.vec_explosions,
                                               .vec_impact_sounds = wh.vec_impact_sounds,
                                               .int4_chance = wh.int4_impact_sound_chance},
                  ora::platforms::wpos{.int4_x = pos_passive.int4_x,
                                       .int4_y = pos_passive.int4_y,
                                       .int4_z = pos_passive.int4_z});
        }
        for (const cmb::WarheadDef& wh : weapon.vec_warheads) {
          if (wh.kind != cmb::WarheadKind::kSpreadDamage) continue;
          const std::int32_t int4_damage{
              cmb::warhead_inflict_damage(wh, {100}, "none")};
          static_cast<void>(pos_passive);
          cmb::DamagePacket pkt;
          pkt.int4_value = int4_damage;
          const std::int32_t int4_dealt{cmb::health_inflict(target.hp, pkt, {})};
          s.vec_events.push_back(std::format("hit {} -> {} dmg {} hp {}", fighter.unit.u4_id,
                                             target.unit.u4_id, int4_dealt, target.hp.int4_hp));
          if (target.hp.b_died_this_call) {
            ww::dispose_actor(s.w, target.unit.u4_id);
            s.vec_events.push_back(
                std::format("killed {} by {}", target.unit.u4_id, fighter.unit.u4_id));
          }
        }
      })};
  static_cast<void>(b_fired);
}

void bridge_tick(GameSession& s) {
  for (FighterState& fighter : s.vec_fighters) {
    if (fighter.arm.ptr_weapon == nullptr) continue;
    // 移动承载已世界内（T31）：战斗侧读上一世界 tick 末的移动状态。
    ww::MobileState* ptr_mobile{ww::movement_state_of(s.w, fighter.unit.u4_id)};
    if (ptr_mobile != nullptr) {
      fighter.unit.pos_center = ptr_mobile->move.pos_center;
      fighter.unit.cell_from = ptr_mobile->move.cell_from;
      fighter.unit.cell_to = ptr_mobile->move.cell_to;
    }

    if (fighter.u4_target != 0) {
      FighterState* ptr_target{fighter_of(s, fighter.u4_target)};
      if (ptr_target == nullptr || !actor_alive(s, fighter.u4_target)) {
        fighter.u4_target = 0;
      } else {
        const std::int64_t int8_range_sq{
            static_cast<std::int64_t>(fighter.arm.ptr_weapon->int4_range) *
            static_cast<std::int64_t>(fighter.arm.ptr_weapon->int4_range)};
        const bool b_in_range{
            horizontal_distance_sq(fighter.unit.pos_center, ptr_target->unit.pos_center) <=
            int8_range_sq};
        if (ptr_mobile != nullptr && b_in_range) {
          if (ptr_mobile->move.b_has_activity)
            ww::movement_queue_move(s.w, fighter.unit.u4_id, ptr_mobile->move.cell_from);
        } else if (ptr_mobile != nullptr) {
          const ww::ActorModel& actor_target{s.w.vec_actors_all[fighter.u4_target]};
          const pfg::CPos cell_target{actor_target.int4_pos_x, actor_target.int4_pos_y};
          if (!ptr_mobile->move.b_has_activity || ptr_mobile->move.cell_destination != cell_target)
            ww::movement_queue_move(s.w, fighter.unit.u4_id, cell_target);
        }
        if (b_in_range) {
          bridge_fire(s, fighter, *ptr_target);
          if (ptr_target->hp.b_dead) fighter.u4_target = 0;
        }
      }
    }

    cmb::armament_tick(fighter.arm);
  }
}

void log_frame_events(GameSession& s, std::int32_t int4_cash_prev, std::int32_t int4_ore_prev,
                      std::int32_t int4_power_p_prev, std::int32_t int4_power_d_prev,
                      const std::vector<std::uint32_t>& vec_prev_in_world) {
  if (s.u4_viewer >= s.w.prod.vec_players.size()) return;
  const opro::PlayerProductionState& state{s.w.prod.vec_players[s.u4_viewer]};

  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    if (std::find(vec_prev_in_world.begin(), vec_prev_in_world.end(), u4_id) !=
        vec_prev_in_world.end())
      continue;
    const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
    if (actor.str_type == "world" || actor.str_type == "player") continue;
    if (actor.str_type == "powr" || actor.str_type == "barr" || actor.str_type == "silo" ||
        actor.str_type == "ftur")
      s.vec_events.push_back(std::format("placed {} id={} @ {},{}", actor.str_type, u4_id,
                                         actor.int4_pos_x, actor.int4_pos_y));
    else if (actor.str_type == "e1" || actor.str_type == "harv")
      s.vec_events.push_back(std::format(
          "produced {} id={} @ {},{}", actor.str_type, u4_id, actor.int4_pos_x,
          actor.int4_pos_y));
  }

  if (state.res.int4_cash != int4_cash_prev || state.res.int4_resources != int4_ore_prev)
    s.vec_events.push_back(std::format("cash={} ore={}", state.res.int4_cash,
                                       state.res.int4_resources));
  if (state.power.int4_power_provided != int4_power_p_prev ||
      state.power.int4_power_drained != int4_power_d_prev)
    s.vec_events.push_back(std::format("power {}/{}", state.power.int4_power_provided,
                                       state.power.int4_power_drained));
}

}

std::expected<GameSession, std::string> session_setup(const SessionEnv& env) {
  auto res_loaded{env.str_map_path.empty()
                      ? load_builtin_sandbox()
                      : load_oramap(env.str_map_path, env.str_tileset_path)};
  if (!res_loaded.has_value()) return std::unexpected(res_loaded.error());
  const LoadedMap& loaded{*res_loaded};

  opro::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "soviet");
  lobby.int4_starting_cash = 5000;
  lobby.str_techlevel = "unrestricted";
  lobby.b_build_radius = true;
  if (env.opt_lobby.has_value()) lobby = *env.opt_lobby;

  auto res_world{ww::world_create(loaded.map, env.int4_seed, lobby)};
  if (!res_world.has_value()) return std::unexpected(res_world.error());

  GameSession s;
  s.w = std::move(*res_world);

  auto res_weapons{cmb::weapon_table_load({std::string{sandbox_weapons_text()}},
                                          {"sandbox-weapons.yaml"})};
  if (!res_weapons.has_value()) return std::unexpected(res_weapons.error());
  s.weapons = std::move(*res_weapons);

  auto res_econ{eco::economy_setup(s.w, loaded.bin_map, loaded.tileset, sandbox_rules_text(),
                                   loaded.int4_left, loaded.int4_top, loaded.int4_right,
                                   loaded.int4_bottom)};
  if (!res_econ.has_value()) {
    if (env.str_map_path.empty()) return std::unexpected(res_econ.error());
    s.vec_events.push_back(std::format("economy degraded on real map: {}", res_econ.error()));
  }

  const std::vector<std::int32_t> vec_clients{net::kLocalClientId};
  net::order_manager_start_game(s.om, s.w, vec_clients);

  s.u4_viewer = 0;
  for (std::size_t idx{0}; idx < s.w.vec_players.size(); ++idx)
    if (s.w.vec_players[idx].b_playable) s.u4_viewer = static_cast<std::uint32_t>(idx);

  bridge_registry_sync(s);

  std::int32_t int4_units{0};
  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const std::string& str_type{s.w.vec_actors_all[u4_id].str_type};
    if (str_type != "world" && str_type != "player") ++int4_units;
  }
  s.vec_events.push_back(std::format("world ready title={} seed={} actors={} players={}",
                                     loaded.str_title, env.int4_seed, int4_units,
                                     s.w.vec_players.size()));
  const opro::PlayerProductionState& state{s.w.prod.vec_players[s.u4_viewer]};
  s.vec_events.push_back(std::format("cash={} ore={}", state.res.int4_cash,
                                     state.res.int4_resources));
  s.vec_events.push_back(std::format("power {}/{}", state.power.int4_power_provided,
                                     state.power.int4_power_drained));
  return s;
}

void session_issue_order(GameSession& s, const net::OrderModel& order) {
  net::issue_order(s.om, order);
  s.vec_bridge_pending.emplace_back(s.w.u4_net_frame + 1, order);
}

void session_tick(GameSession& s) {
  const opro::PlayerProductionState& state_prev{s.w.prod.vec_players[s.u4_viewer]};
  const std::int32_t int4_cash_prev{state_prev.res.int4_cash};
  const std::int32_t int4_ore_prev{state_prev.res.int4_resources};
  const std::int32_t int4_pp_prev{state_prev.power.int4_power_provided};
  const std::int32_t int4_pd_prev{state_prev.power.int4_power_drained};
  const std::vector<std::uint32_t> vec_prev_in_world{s.w.vec_in_world};

  net::tick_immediate(s.om, s.w);
  net::try_tick(s.om, s.w);
  bridge_consume_orders(s);
  bridge_tick(s);
  ww::world_tick(s.w);
  bridge_registry_sync(s);
  log_frame_events(s, int4_cash_prev, int4_ore_prev, int4_pp_prev, int4_pd_prev,
                   vec_prev_in_world);
}

std::string session_digest(const GameSession& s) {
  std::string str_out{ww::state_digest(s.w)};
  // movers 行改读世界侧移动面（D044 移动桥退役，T31 Activity 化）。
  str_out += "movers";
  for (const ww::MobileState& mobile : s.w.vec_mobiles) {
    str_out += std::format(" {}:{},{},{}->{}", mobile.u4_actor, mobile.move.pos_center.int4_x,
                           mobile.move.pos_center.int4_y, mobile.move.cell_from.int4_x,
                           mobile.move.cell_destination.int4_x);
    str_out += std::format(",{}", mobile.move.cell_destination.int4_y);
  }
  str_out += '\n';
  str_out += "fighters";
  for (const FighterState& fighter : s.vec_fighters) {
    str_out += std::format(" {}:{}/{},t{},r{}", fighter.unit.u4_id, fighter.hp.int4_hp,
                           fighter.hp.int4_max_hp, fighter.u4_target,
                           fighter.arm.int4_ticks_since_last_shot);
  }
  str_out += '\n';
  return str_out;
}

std::string session_hud_line(const GameSession& s) {
  const opro::PlayerProductionState& state{s.w.prod.vec_players[s.u4_viewer]};
  return std::format("tick {} cash {} ore {} power {}/{} {}", s.w.int4_world_tick,
                     state.res.int4_cash, state.res.int4_resources,
                     state.power.int4_power_provided, state.power.int4_power_drained,
                     opro::power_state_name(opro::power_state(state.power.int4_power_provided,
                                                              state.power.int4_power_drained)));
}

std::vector<inp::ProductionQueueView> session_queue_views(const GameSession& s) {
  std::vector<inp::ProductionQueueView> vec_views;
  const opro::PlayerProductionState& state{s.w.prod.vec_players[s.u4_viewer]};
  for (const std::string_view str_type : inp::kPanelTabTypes) {
    inp::ProductionQueueView view;
    view.str_type = std::string{str_type};
    std::string str_type_lower{str_type};
    for (char& ch : str_type_lower)
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    const opro::ProductionQueueState* ptr_queue{};
    for (const opro::ProductionQueueState& queue : state.vec_queues)
      if (queue.str_type == str_type_lower) ptr_queue = &queue;
    if (ptr_queue != nullptr) {
      view.b_enabled = ptr_queue->b_enabled;
      view.vec_items = opro::queue_buildable_items(s.w, s.u4_viewer, str_type);
      for (const opro::ProductionItemModel& item : ptr_queue->vec_queue) {
        inp::ProductionItemView view_item;
        view_item.str_item = item.str_item;
        view_item.int4_total_time = item.int4_total_time;
        view_item.int4_remaining_time = item.int4_remaining_time;
        view_item.int4_total_cost = item.int4_total_cost;
        view_item.int4_remaining_cost = item.int4_remaining_cost;
        view_item.b_started = item.b_started;
        view_item.b_paused = item.b_paused;
        view_item.b_done = item.b_done;
        view_item.b_building = str_type == "Building" || str_type == "Defense";
        view.vec_queue.push_back(std::move(view_item));
      }
    }
    vec_views.push_back(std::move(view));
  }
  return vec_views;
}

}
