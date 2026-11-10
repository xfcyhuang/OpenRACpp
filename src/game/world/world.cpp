module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

namespace ora::game::world {

namespace {

std::string to_lower_ascii(std::string_view str_in) {
  std::string str_out;
  str_out.reserve(str_in.size());
  for (const char ch : str_in)
    str_out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  return str_out;
}

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const char* ptr_first{str_value.data()};
  const char* ptr_last{str_value.data() + str_value.size()};
  const auto res{std::from_chars(ptr_first, ptr_last, int4_out)};
  if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
  return int4_out;
}

std::optional<bool> parse_bool(std::string_view str_value) {
  if (str_value == "True") return true;
  if (str_value == "False") return false;
  return std::nullopt;
}

const MiniYamlNode* find_child(const std::vector<MiniYamlNode>& vec_nodes,
                               std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::optional<std::pair<std::int32_t, std::int32_t>> parse_location(std::string_view str_value) {
  const std::size_t idx_comma{str_value.find(',')};
  if (idx_comma == std::string_view::npos) return std::nullopt;
  const auto opt_x{parse_int(str_value.substr(0, idx_comma))};
  const auto opt_y{parse_int(str_value.substr(idx_comma + 1))};
  if (!opt_x.has_value() || !opt_y.has_value()) return std::nullopt;
  return std::pair{*opt_x, *opt_y};
}

void world_tick_activities(WorldModel& w) { economy::economy_on_tick(w); }

void world_tick_effects(WorldModel&) {}

void frame_end_run_task(WorldModel& w, const FrameEndTask& task) {
  if (task.u4_kind == 1 || task.u4_kind == 2) {
    production::production_frame_end_task(w, task.u4_kind, task.u4_player, task.u4_value,
                                          task.str_item);
    return;
  }

  if (task.u4_kind == 3) {
    economy::economy_frame_end_task(w, task);
    return;
  }

  if (task.u4_actor >= w.vec_actors_all.size()) return;
  ActorModel& actor{w.vec_actors_all[task.u4_actor]};
  if (actor.b_disposed) return;

  if (actor.b_in_world) {
    actor.b_in_world = false;
    const auto it{std::lower_bound(w.vec_in_world.begin(), w.vec_in_world.end(), task.u4_actor)};
    if (it != w.vec_in_world.end() && *it == task.u4_actor) w.vec_in_world.erase(it);
    production::production_on_actor_removed(w, task.u4_actor);
    economy::economy_on_actor_removed(w, task.u4_actor);
  }

  container_remove_actor(w.container_tick, task.u4_actor);
  container_remove_actor(w.container_sync, task.u4_actor);
  actor.b_disposed = true;
}

const ActorTypeDef* find_actor_type(const WorldModel& w, std::string_view str_type_lower) {
  for (const ActorTypeDef& def : w.vec_actor_types)
    if (def.str_name_lower == str_type_lower) return &def;
  return nullptr;
}

std::uint32_t player_index_by_name(const WorldModel& w, std::string_view str_name) {
  for (std::size_t idx{0}; idx < w.vec_players.size(); ++idx)
    if (w.vec_players[idx].str_internal_name == str_name)
      return static_cast<std::uint32_t>(idx);
  return kOwnerNone;
}

}

std::expected<SyntheticMap, std::string> synthetic_map_from_yaml(std::string_view str_rules_text,
                                                                 std::string_view str_map_text) {
  SyntheticMap map;
  map.vec_rule_texts.push_back(std::string(str_rules_text));
  map.str_map_text = std::string(str_map_text);

  auto res_map{miniyaml_from_string(str_map_text, "map", true)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());

  if (const MiniYamlNode* node_title{find_child(*res_map, "Title")})
    map.str_title = node_title->str_value;

  if (const MiniYamlNode* node_players{find_child(*res_map, "Players")}) {
    for (const MiniYamlNode& node_ref : node_players->vec_children) {
      MapPlayerRef ref;
      if (const MiniYamlNode* node_name{find_child(node_ref.vec_children, "Name")})
        ref.str_name = node_name->str_value;
      if (const MiniYamlNode* node_owns{find_child(node_ref.vec_children, "OwnsWorld")}) {
        const auto opt{parse_bool(node_owns->str_value)};
        if (!opt.has_value()) return std::unexpected("map: bad OwnsWorld value for " + ref.str_name);
        ref.b_owns_world = *opt;
      }
      if (const MiniYamlNode* node_noncomb{
              find_child(node_ref.vec_children, "NonCombatant")}) {
        const auto opt{parse_bool(node_noncomb->str_value)};
        if (!opt.has_value())
          return std::unexpected("map: bad NonCombatant value for " + ref.str_name);
        ref.b_non_combatant = *opt;
      }
      if (const MiniYamlNode* node_playable{find_child(node_ref.vec_children, "Playable")}) {
        const auto opt{parse_bool(node_playable->str_value)};
        if (!opt.has_value())
          return std::unexpected("map: bad Playable value for " + ref.str_name);
        ref.b_playable = *opt;
      }
      if (const MiniYamlNode* node_faction{find_child(node_ref.vec_children, "Faction")})
        ref.str_faction = node_faction->str_value;
      map.vec_player_refs.push_back(std::move(ref));
    }
  }

  if (const MiniYamlNode* node_actors{find_child(*res_map, "Actors")}) {
    for (const MiniYamlNode& node_actor : node_actors->vec_children) {
      MapActorRef ref;
      ref.str_type = node_actor.str_value;
      if (const MiniYamlNode* node_loc{find_child(node_actor.vec_children, "Location")}) {
        const auto opt{parse_location(node_loc->str_value)};
        if (!opt.has_value())
          return std::unexpected("map: bad Location value for actor " + node_actor.str_key);
        ref.int4_pos_x = opt->first;
        ref.int4_pos_y = opt->second;
      }
      if (const MiniYamlNode* node_owner{find_child(node_actor.vec_children, "Owner")})
        ref.str_owner = node_owner->str_value;
      map.vec_actor_defs.push_back(std::move(ref));
    }
  }

  return map;
}

std::expected<WorldModel, std::string> world_create(const SyntheticMap& map_syn,
                                                    std::int32_t int4_seed) {
  return world_create(map_syn, int4_seed, production::LobbyModel{});
}

std::expected<WorldModel, std::string> world_create(const SyntheticMap& map_syn,
                                                    std::int32_t int4_seed,
                                                    const production::LobbyModel& lobby,
                                                    const bool b_is_replay) {
  WorldModel w;
  w.lobby = lobby;
  // IsReplay 是世界构造期属性（上游 World ctor），玩家创建/bot 激活前生效。
  w.b_is_replay = b_is_replay;
  w.rng_shared = support::make_mersenne_twister(int4_seed);
  w.rng_player = support::make_mersenne_twister(int4_seed);
  // rng_local 与同步流分开派生（上游无种子墙钟，消耗序本就不对拍；见偏离登记）。
  w.rng_local = support::make_mersenne_twister(int4_seed ^ 0x35ee9b1);

  std::vector<std::vector<MiniYamlNode>> vec_sources;
  vec_sources.reserve(map_syn.vec_rule_texts.size());
  for (const std::string& str_text : map_syn.vec_rule_texts) {
    auto res_rules{miniyaml_from_string(str_text, "rules", true)};
    if (!res_rules.has_value()) return std::unexpected(res_rules.error());
    vec_sources.push_back(std::move(*res_rules));
  }

  auto res_merged{miniyaml_merge(vec_sources)};
  if (!res_merged.has_value()) return std::unexpected(res_merged.error());

  std::set<std::string> set_lower_seen;
  for (const MiniYamlNode& node : *res_merged) {
    if (node.str_key.starts_with('^')) continue;

    const std::string str_lower{to_lower_ascii(node.str_key)};
    if (!set_lower_seen.insert(str_lower).second)
      return std::unexpected(
          "LoadFromManifest<Manifest,Rules>, duplicate values found for the following keys: " +
          node.str_key);

    ActorTypeDef def;
    def.str_name_lower = str_lower;
    for (const MiniYamlNode& node_trait : node.vec_children) {
      if (!node_trait.str_value.empty())
        return std::unexpected(std::format("Junk value `{}` on trait node {}",
                                           node_trait.str_value, node_trait.str_key));

      const auto [str_kind, str_instance]{trait_key_split(node_trait.str_key)};
      const TraitSpec* ptr_spec{trait_spec_find(str_kind)};
      if (ptr_spec == nullptr)
        return std::unexpected("Cannot locate type: " + std::string(str_kind) + "Info");

      TraitTemplate tpl;
      tpl.str_kind = std::string(str_kind);
      tpl.str_instance = std::string(str_instance);
      if (!ptr_spec->str_int_param_key.empty()) {
        if (const MiniYamlNode* node_param{
                find_child(node_trait.vec_children, ptr_spec->str_int_param_key)}) {
          const auto opt{parse_int(node_param->str_value)};
          if (!opt.has_value())
            return std::unexpected(std::format("Cannot parse int value `{}` for {}",
                                               node_param->str_value, node_param->str_key));
          tpl.int4_param = *opt;
        }
      }
      if (!ptr_spec->str_str_param_key.empty()) {
        if (const MiniYamlNode* node_param{
                find_child(node_trait.vec_children, ptr_spec->str_str_param_key)})
          tpl.str_param_type = to_lower_ascii(node_param->str_value);
      }
      if (!ptr_spec->str_str_param2_key.empty()) {
        if (const MiniYamlNode* node_param{
                find_child(node_trait.vec_children, ptr_spec->str_str_param2_key)})
          tpl.str_param2 = to_lower_ascii(node_param->str_value);
      }
      if (!ptr_spec->str_str_param3_key.empty()) {
        if (const MiniYamlNode* node_param{
                find_child(node_trait.vec_children, ptr_spec->str_str_param3_key)})
          tpl.str_param3 = to_lower_ascii(node_param->str_value);
      }
      for (const MiniYamlNode& node_child : node_trait.vec_children)
        if (!node_child.str_value.empty() && node_child.vec_children.empty())
          tpl.vec_kv.emplace_back(node_child.str_key, node_child.str_value);
      def.vec_templates.push_back(std::move(tpl));
    }
    w.vec_actor_types.push_back(std::move(def));
  }

  for (const ActorTypeDef& def : w.vec_actor_types)
    for (const TraitTemplate& tpl : def.vec_templates)
      if (tpl.str_kind == "SpawnerProbe" && !tpl.str_param_type.empty() &&
          find_actor_type(w, tpl.str_param_type) == nullptr)
        return std::unexpected("No rules definition for unit " + tpl.str_param_type);

  auto res_world_actor{create_actor(w, "world", kOwnerNone, 0, 0, true)};
  if (!res_world_actor.has_value()) return std::unexpected(res_world_actor.error());

  std::vector<std::string> vec_factions;
  if (const ActorTypeDef* ptr_world_def{find_actor_type(w, "world")})
    for (const TraitTemplate& tpl : ptr_world_def->vec_templates)
      if (tpl.str_kind == "FactionInfo") vec_factions.push_back(tpl.str_instance);

  bool b_owner_found{false};
  for (const MapPlayerRef& ref : map_syn.vec_player_refs) {
    if (ref.b_playable) continue;

    const std::uint32_t u4_index{static_cast<std::uint32_t>(w.vec_players.size())};
    auto res{create_actor(w, "player", u4_index, 0, 0, true)};
    if (!res.has_value()) return std::unexpected(res.error());

    bool b_faction_hit{false};
    for (const std::string& str_faction : vec_factions)
      if (str_faction == ref.str_faction) b_faction_hit = true;
    if (!b_faction_hit)
      support::next(w.rng_player, 0, static_cast<std::int32_t>(vec_factions.size()));

    PlayerModel player;
    player.str_internal_name = ref.str_name;
    player.str_faction = ref.str_faction;
    player.u4_actor_id = *res;
    player.b_owns_world = ref.b_owns_world;
    player.b_non_combatant = ref.b_non_combatant;
    w.vec_players.push_back(std::move(player));

    if (ref.b_owns_world) {
      b_owner_found = true;
      w.u4_world_owner_player = u4_index;
      w.vec_actors_all[0].u4_owner = u4_index;
    }
  }

  if (!b_owner_found)
    return std::unexpected("Map " + map_syn.str_title +
                           " does not define a player actor owning the world.");

  for (const auto& [str_slot, str_faction] : lobby.vec_playable_factions) {
    const std::uint32_t u4_index{static_cast<std::uint32_t>(w.vec_players.size())};
    auto res{create_actor(w, "player", u4_index, 0, 0, true)};
    if (!res.has_value()) return std::unexpected(res.error());

    bool b_faction_hit{false};
    for (const std::string& str_faction_table : vec_factions)
      if (str_faction_table == str_faction) b_faction_hit = true;
    if (!b_faction_hit)
      support::next(w.rng_player, 0, static_cast<std::int32_t>(vec_factions.size()));

    PlayerModel player;
    player.str_internal_name = str_slot;
    player.str_faction = str_faction;
    player.u4_actor_id = *res;
    player.b_playable = true;
    w.vec_players.push_back(std::move(player));

    // Player ctor 尾的 host-only bot 激活（Player.cs:215-223）：单机形态 host 恒真。
    for (const auto& [str_bot_slot, str_bot_type] : lobby.vec_slot_bots)
      if (str_bot_slot == str_slot) bot_activate(w, u4_index, str_bot_type);
  }

  {
    const std::uint32_t u4_index{static_cast<std::uint32_t>(w.vec_players.size())};
    auto res{create_actor(w, "player", u4_index, 0, 0, true)};
    if (!res.has_value()) return std::unexpected(res.error());

    support::next(w.rng_player, 0, static_cast<std::int32_t>(vec_factions.size()));

    PlayerModel player;
    player.str_internal_name = "Everyone";
    player.str_faction = "Random";
    player.u4_actor_id = *res;
    player.b_non_combatant = true;
    player.b_spectating = true;
    w.vec_players.push_back(std::move(player));
  }

  production::production_queue_state_build(w);

  for (const MapActorRef& ref : map_syn.vec_actor_defs) {
    std::uint32_t u4_owner{player_index_by_name(w, ref.str_owner)};
    if (u4_owner == kOwnerNone) u4_owner = w.u4_world_owner_player;
    auto res{create_actor(w, ref.str_type, u4_owner, ref.int4_pos_x, ref.int4_pos_y, true)};
    if (!res.has_value()) return std::unexpected(res.error());
  }

  if (find_actor_type(w, "mcv") != nullptr) {
    for (std::size_t idx{0}; idx < w.vec_players.size(); ++idx) {
      if (!w.vec_players[idx].b_playable) continue;
      std::int32_t int4_home_x{0};
      std::int32_t int4_home_y{0};
      for (const MapActorRef& ref : map_syn.vec_actor_defs) {
        if (ref.str_type != "mpspawn") continue;
        int4_home_x = ref.int4_pos_x;
        int4_home_y = ref.int4_pos_y;
        break;
      }
      auto res{create_actor(w, "mcv", static_cast<std::uint32_t>(idx), int4_home_x, int4_home_y,
                            true)};
      if (!res.has_value()) return std::unexpected(res.error());
    }
  }

  return w;
}

std::expected<std::uint32_t, std::string> create_actor(WorldModel& w, std::string_view str_type,
                                                       std::uint32_t u4_owner,
                                                       std::int32_t int4_pos_x,
                                                       std::int32_t int4_pos_y,
                                                       bool b_add_to_world) {
  const std::string str_lower{to_lower_ascii(str_type)};
  const ActorTypeDef* ptr_def{find_actor_type(w, str_lower)};
  if (ptr_def == nullptr) return std::unexpected("No rules definition for unit " + str_lower);

  ActorModel actor;
  actor.u4_id = w.u4_next_aid++;
  actor.str_type = str_lower;
  actor.u4_owner = u4_owner;
  actor.int4_pos_x = int4_pos_x;
  actor.int4_pos_y = int4_pos_y;

  const TraitCreateCtx ctx_create{u4_owner};
  for (const TraitTemplate& tpl : ptr_def->vec_templates) {
    const TraitSpec* ptr_spec{trait_spec_find(tpl.str_kind)};

    TraitModel trait;
    trait.str_kind = tpl.str_kind;
    trait.str_instance = tpl.str_instance;
    trait.int4_param = tpl.int4_param;
    trait.str_param_type = tpl.str_param_type;
    trait.str_param2 = tpl.str_param2;
    trait.str_param3 = tpl.str_param3;
    trait.vec_kv = tpl.vec_kv;
    if (ptr_spec->fn_create) ptr_spec->fn_create(ctx_create, trait);

    const std::uint32_t u4_handle{static_cast<std::uint32_t>(w.vec_traits.size())};
    w.vec_traits.push_back(std::move(trait));
    actor.vec_trait_handles.push_back(u4_handle);

    if (ptr_spec->b_implements_tick) container_add(w.container_tick, actor.u4_id, u4_handle);
    if (ptr_spec->b_implements_sync) container_add(w.container_sync, actor.u4_id, u4_handle);
  }

  if (b_add_to_world) {
    actor.b_in_world = true;
    w.vec_in_world.push_back(actor.u4_id);
  }

  w.vec_actors_all.push_back(std::move(actor));
  if (b_add_to_world) {
    production::production_on_actor_added(w, w.vec_actors_all.back().u4_id);
    economy::economy_on_actor_added(w, w.vec_actors_all.back().u4_id);
  }
  return w.vec_actors_all.back().u4_id;
}

void dispose_actor(WorldModel& w, std::uint32_t u4_actor_id) {
  if (u4_actor_id >= w.vec_actors_all.size()) return;
  w.vec_actors_all[u4_actor_id].b_will_dispose = true;
  w.deque_frame_end.push_back(FrameEndTask{.u4_kind = 0, .u4_actor = u4_actor_id});
}

std::int32_t process_orders(WorldModel& w) {
  const std::int32_t int4_sync{sync_hash_upstream(w)};
  ++w.u4_net_frame;
  return int4_sync;
}

void world_tick(WorldModel& w) {
  ++w.int4_world_tick;

  world_tick_activities(w);

  const TraitContainerModel snapshot{w.container_tick};
  for (std::size_t idx{0}; idx < snapshot.vec_actor_ids.size(); ++idx) {
    ActorModel& actor{w.vec_actors_all[snapshot.vec_actor_ids[idx]]};
    TraitModel& trait{w.vec_traits[snapshot.vec_trait_handles[idx]]};
    const TraitSpec* ptr_spec{trait_spec_find(trait.str_kind)};
    if (ptr_spec != nullptr && ptr_spec->fn_tick)
      ptr_spec->fn_tick(TraitTickCtx{w, actor}, trait);
  }

  // 上游 ModularBot 是 player actor 的 ITick trait（随 actor 遍历序跑）；C++ 合成面
  // player actor 无 ModularBot trait，bot tick 独立段插在 effects 前（序位见偏离登记）。
  bot_tick_all(w);

  world_tick_effects(w);

  while (!w.deque_frame_end.empty()) {
    const FrameEndTask task{w.deque_frame_end.front()};
    w.deque_frame_end.pop_front();
    frame_end_run_task(w, task);
  }
}

}
