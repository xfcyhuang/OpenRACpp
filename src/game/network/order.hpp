#pragma once

import std;

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {

enum class TargetKind : std::uint8_t {
  kInvalid = 0,
  kActor = 1,
  kTerrain = 2,
  kFrozenActor = 3,
};

struct TargetModel {
  TargetKind kind{};
  std::uint32_t u4_actor{};
  std::int32_t int4_generation{};
  std::uint32_t u4_viewer_actor{};
  std::uint32_t u4_frozen_id{};
  bool b_cell{};
  std::int32_t int4_cell_bits{};
  std::uint8_t u1_subcell{};
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
  std::int32_t int4_pos_z{};
  std::vector<std::array<std::int32_t, 3>> vec_terrain_positions{};
};

struct OrderModel {
  std::string str_order{};
  bool b_handshake{};
  bool b_has_subject{};
  std::uint32_t u4_subject{};
  TargetModel target{};
  std::optional<std::string> opt_str_target{};
  bool b_queued{};
  bool b_has_extra_actors{};
  std::vector<std::uint32_t> vec_extra_actors{};
  std::int32_t int4_extra_location_bits{};
  std::uint32_t u4_extra_data{};
  bool b_has_grouped{};
  std::vector<std::uint32_t> vec_grouped{};
  bool b_is_immediate{};
};

struct OrderReader {
  std::span<const std::uint8_t> span_bytes;
  std::size_t idx_pos{};
};

[[nodiscard]] std::vector<std::uint8_t> serialize_order(const OrderModel& order);

[[nodiscard]] std::optional<OrderModel> deserialize_order(OrderReader& reader,
                                                           const world::WorldModel* ptr_world);

}  // namespace ora::game::network
