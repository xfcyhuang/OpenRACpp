#pragma once

import std;

#include "game/world/actor.hpp"

namespace ora::game::network {
struct OrderModel;
}

namespace ora::game::world {

struct WorldModel;

struct TraitModel {
  std::string str_kind;
  std::string str_instance;
  std::int32_t int4_counter{};
  std::int32_t int4_param{};
  std::string str_param_type;
  std::string str_param2;
  std::string str_param3;
  bool b_sync_flag{};
};

struct TraitTemplate {
  std::string str_kind;
  std::string str_instance;
  std::int32_t int4_param{};
  std::string str_param_type;
  std::string str_param2;
  std::string str_param3;
  std::vector<std::pair<std::string, std::string>> vec_kv;
};

struct TraitCreateCtx {
  std::uint32_t u4_owner;
};

struct TraitTickCtx {
  WorldModel& ref_world;
  ActorModel& ref_actor;
};

struct TraitResolveCtx {
  WorldModel& ref_world;
  ActorModel& ref_actor;
  const network::OrderModel& ref_order;
};

struct TraitSpec {
  std::string str_kind;
  bool b_implements_tick{};
  bool b_implements_sync{};
  bool b_implements_resolve{};
  std::string str_int_param_key;
  std::string str_str_param_key;
  std::string str_str_param2_key;
  std::string str_str_param3_key;
  std::function<void(const TraitCreateCtx&, TraitModel&)> fn_create;
  std::function<void(const TraitTickCtx&, TraitModel&)> fn_tick;
  std::function<std::int32_t(const WorldModel&, std::uint32_t, const TraitModel&)> fn_sync_hash;
  std::function<void(const TraitResolveCtx&, TraitModel&)> fn_resolve;
};

[[nodiscard]] const std::vector<TraitSpec>& trait_registry();

[[nodiscard]] const TraitSpec* trait_spec_find(std::string_view str_kind);

[[nodiscard]] std::pair<std::string_view, std::string_view> trait_key_split(
    std::string_view str_key);

}  // namespace ora::game::world
