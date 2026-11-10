module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

namespace ora::game::network {

namespace {

constexpr std::uint8_t kTypeFields{0xff};
constexpr std::uint8_t kTypeHandshake{0xfe};

constexpr std::int16_t kFieldTarget{0x0001};
constexpr std::int16_t kFieldExtraActors{0x0002};
constexpr std::int16_t kFieldTargetString{0x0004};
constexpr std::int16_t kFieldQueued{0x0008};
constexpr std::int16_t kFieldExtraLocation{0x0010};
constexpr std::int16_t kFieldExtraData{0x0020};
constexpr std::int16_t kFieldTargetIsCell{0x0040};
constexpr std::int16_t kFieldSubject{0x0080};
constexpr std::int16_t kFieldGrouped{0x0100};

void write_u1(std::vector<std::uint8_t>& vec_out, std::uint8_t u1_value) {
  vec_out.push_back(u1_value);
}

void write_u4le(std::vector<std::uint8_t>& vec_out, std::uint32_t u4_value) {
  vec_out.push_back(static_cast<std::uint8_t>(u4_value & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 8) & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 16) & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u4_value >> 24) & 0xff));
}

void write_i4le(std::vector<std::uint8_t>& vec_out, std::int32_t int4_value) {
  write_u4le(vec_out, static_cast<std::uint32_t>(int4_value));
}

void write_i2le(std::vector<std::uint8_t>& vec_out, std::int16_t int2_value) {
  const std::uint16_t u2_value{static_cast<std::uint16_t>(int2_value)};
  vec_out.push_back(static_cast<std::uint8_t>(u2_value & 0xff));
  vec_out.push_back(static_cast<std::uint8_t>((u2_value >> 8) & 0xff));
}

void write_7bit_string(std::vector<std::uint8_t>& vec_out, std::string_view str_value) {
  std::uint32_t u4_length{static_cast<std::uint32_t>(str_value.size())};
  while (u4_length >= 0x80) {
    write_u1(vec_out, static_cast<std::uint8_t>((u4_length & 0x7f) | 0x80));
    u4_length >>= 7;
  }
  write_u1(vec_out, static_cast<std::uint8_t>(u4_length));
  for (const char ch : str_value) vec_out.push_back(static_cast<std::uint8_t>(ch));
}

struct ReadCursor {
  OrderReader& ref_reader;
  bool b_ok{true};
};

std::optional<std::uint8_t> read_u1(OrderReader& reader) {
  if (reader.idx_pos >= reader.span_bytes.size()) return std::nullopt;
  return reader.span_bytes[reader.idx_pos++];
}

std::optional<std::uint32_t> read_u4le(OrderReader& reader) {
  if (reader.span_bytes.size() - reader.idx_pos < 4) return std::nullopt;
  const std::uint32_t u4_value{static_cast<std::uint32_t>(reader.span_bytes[reader.idx_pos]) |
                               (static_cast<std::uint32_t>(reader.span_bytes[reader.idx_pos + 1])
                                << 8) |
                               (static_cast<std::uint32_t>(reader.span_bytes[reader.idx_pos + 2])
                                << 16) |
                               (static_cast<std::uint32_t>(reader.span_bytes[reader.idx_pos + 3])
                                << 24)};
  reader.idx_pos += 4;
  return u4_value;
}

std::optional<std::int32_t> read_i4le(OrderReader& reader) {
  const auto opt{read_u4le(reader)};
  if (!opt.has_value()) return std::nullopt;
  return static_cast<std::int32_t>(*opt);
}

std::optional<std::int16_t> read_i2le(OrderReader& reader) {
  if (reader.span_bytes.size() - reader.idx_pos < 2) return std::nullopt;
  const std::uint16_t u2_value{
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(reader.span_bytes[reader.idx_pos]) |
                                 static_cast<std::uint16_t>(
                                     static_cast<std::uint16_t>(
                                         reader.span_bytes[reader.idx_pos + 1])
                                     << 8))};
  reader.idx_pos += 2;
  return static_cast<std::int16_t>(u2_value);
}

std::optional<std::string> read_7bit_string(OrderReader& reader) {
  std::uint32_t u4_length{};
  std::uint32_t u4_shift{};
  std::uint8_t u1_byte{};
  do {
    const auto opt_byte{read_u1(reader)};
    if (!opt_byte.has_value()) return std::nullopt;
    u1_byte = *opt_byte;

    u4_length |= static_cast<std::uint32_t>(u1_byte & 0x7f) << u4_shift;
    u4_shift += 7;
    if (u4_shift >= 32) return std::nullopt;
  } while ((u1_byte & 0x80) != 0);

  if (reader.span_bytes.size() - reader.idx_pos < u4_length) return std::nullopt;
  std::string str_out;
  str_out.resize(u4_length);
  for (std::uint32_t idx{}; idx < u4_length; ++idx)
    str_out[idx] = static_cast<char>(reader.span_bytes[reader.idx_pos + idx]);
  reader.idx_pos += u4_length;
  return str_out;
}

}

std::vector<std::uint8_t> serialize_order(const OrderModel& order) {
  std::vector<std::uint8_t> vec_out;
  write_u1(vec_out, order.b_handshake ? kTypeHandshake : kTypeFields);
  write_7bit_string(vec_out, order.str_order);

  if (order.b_handshake) {
    write_7bit_string(vec_out, order.opt_str_target.value_or(""));
    return vec_out;
  }

  std::int16_t int2_flags{};
  if (order.target.kind != TargetKind::kInvalid) int2_flags |= kFieldTarget;
  if (order.b_has_extra_actors) int2_flags |= kFieldExtraActors;
  if (order.opt_str_target.has_value()) int2_flags |= kFieldTargetString;
  if (order.b_queued) int2_flags |= kFieldQueued;
  if (order.int4_extra_location_bits != 0) int2_flags |= kFieldExtraLocation;
  if (order.u4_extra_data != 0) int2_flags |= kFieldExtraData;
  if (order.target.kind == TargetKind::kTerrain && order.target.b_cell)
    int2_flags |= kFieldTargetIsCell;
  if (order.b_has_subject) int2_flags |= kFieldSubject;
  if (order.b_has_grouped) int2_flags |= kFieldGrouped;
  write_i2le(vec_out, int2_flags);

  if ((int2_flags & kFieldSubject) != 0) write_u4le(vec_out, order.u4_subject);

  if ((int2_flags & kFieldTarget) != 0) {
    write_u1(vec_out, static_cast<std::uint8_t>(order.target.kind));
    switch (order.target.kind) {
      case TargetKind::kActor:
        write_u4le(vec_out, order.target.u4_actor);
        write_i4le(vec_out, order.target.int4_generation);
        break;
      case TargetKind::kFrozenActor:
        write_u4le(vec_out, order.target.u4_viewer_actor);
        write_u4le(vec_out, order.target.u4_frozen_id);
        break;
      case TargetKind::kTerrain:
        if (order.target.b_cell) {
          write_i4le(vec_out, order.target.int4_cell_bits);
          write_u1(vec_out, order.target.u1_subcell);
        } else {
          write_i4le(vec_out, order.target.int4_pos_x);
          write_i4le(vec_out, order.target.int4_pos_y);
          write_i4le(vec_out, order.target.int4_pos_z);
          const bool b_single{order.target.vec_terrain_positions.size() == 1 &&
                              order.target.vec_terrain_positions[0][0] ==
                                  order.target.int4_pos_x &&
                              order.target.vec_terrain_positions[0][1] ==
                                  order.target.int4_pos_y &&
                              order.target.vec_terrain_positions[0][2] == order.target.int4_pos_z};
          if (b_single) {
            write_i2le(vec_out, -1);
          } else {
            write_i2le(vec_out,
                       static_cast<std::int16_t>(order.target.vec_terrain_positions.size()));
            for (const std::array<std::int32_t, 3>& arr_pos : order.target.vec_terrain_positions) {
              write_i4le(vec_out, arr_pos[0]);
              write_i4le(vec_out, arr_pos[1]);
              write_i4le(vec_out, arr_pos[2]);
            }
          }
        }
        break;
      case TargetKind::kInvalid:
        break;
    }
  }

  if ((int2_flags & kFieldTargetString) != 0) write_7bit_string(vec_out, *order.opt_str_target);

  if ((int2_flags & kFieldExtraActors) != 0) {
    write_i4le(vec_out, static_cast<std::int32_t>(order.vec_extra_actors.size()));
    for (const std::uint32_t u4_id : order.vec_extra_actors) write_u4le(vec_out, u4_id);
  }

  if ((int2_flags & kFieldExtraLocation) != 0) write_i4le(vec_out, order.int4_extra_location_bits);

  if ((int2_flags & kFieldExtraData) != 0) write_u4le(vec_out, order.u4_extra_data);

  if ((int2_flags & kFieldGrouped) != 0) {
    write_i4le(vec_out, static_cast<std::int32_t>(order.vec_grouped.size()));
    for (const std::uint32_t u4_id : order.vec_grouped) write_u4le(vec_out, u4_id);
  }

  return vec_out;
}

std::optional<OrderModel> deserialize_order(OrderReader& reader,
                                             const world::WorldModel* ptr_world) {
  const auto opt_type{read_u1(reader)};
  if (!opt_type.has_value()) return std::nullopt;

  OrderModel order;
  if (*opt_type == kTypeHandshake) {
    auto opt_name{read_7bit_string(reader)};
    if (!opt_name.has_value()) return std::nullopt;
    auto opt_target{read_7bit_string(reader)};
    if (!opt_target.has_value()) return std::nullopt;
    order.b_handshake = true;
    order.str_order = std::move(*opt_name);
    order.opt_str_target = std::move(*opt_target);
    return order;
  }

  if (*opt_type != kTypeFields) return std::nullopt;

  auto opt_str{read_7bit_string(reader)};
  if (!opt_str.has_value()) return std::nullopt;
  order.str_order = std::move(*opt_str);

  const auto opt_flags{read_i2le(reader)};
  if (!opt_flags.has_value()) return std::nullopt;
  const std::int16_t int2_flags{*opt_flags};

  bool b_subject_dropped{false};
  if ((int2_flags & kFieldSubject) != 0) {
    const auto opt_subject{read_u4le(reader)};
    if (!opt_subject.has_value()) return std::nullopt;
    if (ptr_world == nullptr) {
      order.b_has_subject = true;
      order.u4_subject = *opt_subject;
    } else if (*opt_subject == 0xffffffffu ||
               world::world_actor_by_id(*ptr_world, *opt_subject) == nullptr) {
      b_subject_dropped = true;
    } else {
      order.b_has_subject = true;
      order.u4_subject = *opt_subject;
    }
  }

  if ((int2_flags & kFieldTarget) != 0) {
    const auto opt_kind{read_u1(reader)};
    if (!opt_kind.has_value()) return std::nullopt;
    order.target.kind = static_cast<TargetKind>(*opt_kind);
    switch (order.target.kind) {
      case TargetKind::kActor: {
        const auto opt_id{read_u4le(reader)};
        const auto opt_generation{read_i4le(reader)};
        if (!opt_id.has_value() || !opt_generation.has_value()) return std::nullopt;
        if (ptr_world == nullptr) {
          order.target.u4_actor = *opt_id;
          order.target.int4_generation = *opt_generation;
        } else if (*opt_id != 0xffffffffu &&
                   world::world_actor_by_id(*ptr_world, *opt_id) != nullptr) {
          order.target.u4_actor = *opt_id;
          order.target.int4_generation = *opt_generation;
        } else {
          order.target.kind = TargetKind::kInvalid;
        }
        break;
      }
      case TargetKind::kFrozenActor: {
        const auto opt_viewer{read_u4le(reader)};
        const auto opt_frozen{read_u4le(reader)};
        if (!opt_viewer.has_value() || !opt_frozen.has_value()) return std::nullopt;
        if (ptr_world == nullptr) {
          order.target.u4_viewer_actor = *opt_viewer;
          order.target.u4_frozen_id = *opt_frozen;
        } else {
          order.target.kind = TargetKind::kInvalid;
        }
        break;
      }
      case TargetKind::kTerrain: {
        if ((int2_flags & kFieldTargetIsCell) != 0) {
          const auto opt_bits{read_i4le(reader)};
          const auto opt_subcell{read_u1(reader)};
          if (!opt_bits.has_value() || !opt_subcell.has_value()) return std::nullopt;
          order.target.b_cell = true;
          order.target.int4_cell_bits = *opt_bits;
          order.target.u1_subcell = *opt_subcell;
        } else {
          const auto opt_x{read_i4le(reader)};
          const auto opt_y{read_i4le(reader)};
          const auto opt_z{read_i4le(reader)};
          const auto opt_count{read_i2le(reader)};
          if (!opt_x.has_value() || !opt_y.has_value() || !opt_z.has_value() ||
              !opt_count.has_value())
            return std::nullopt;
          order.target.int4_pos_x = *opt_x;
          order.target.int4_pos_y = *opt_y;
          order.target.int4_pos_z = *opt_z;

          if (*opt_count == -1) {
            order.target.vec_terrain_positions.push_back({*opt_x, *opt_y, *opt_z});
          } else {
            const std::size_t idx_remaining{reader.span_bytes.size() - reader.idx_pos};
            if (*opt_count < 0 ||
                static_cast<std::uint64_t>(*opt_count) > idx_remaining / 12)
              return std::nullopt;
            for (std::int16_t idx{}; idx < *opt_count; ++idx) {
              const auto opt_px{read_i4le(reader)};
              const auto opt_py{read_i4le(reader)};
              const auto opt_pz{read_i4le(reader)};
              if (!opt_px.has_value() || !opt_py.has_value() || !opt_pz.has_value())
                return std::nullopt;
              order.target.vec_terrain_positions.push_back({*opt_px, *opt_py, *opt_pz});
            }
          }
        }
        break;
      }
      default:
        order.target.kind = TargetKind::kInvalid;
        break;
    }
  }

  if ((int2_flags & kFieldTargetString) != 0) {
    auto opt_str_target{read_7bit_string(reader)};
    if (!opt_str_target.has_value()) return std::nullopt;
    order.opt_str_target = std::move(*opt_str_target);
  }

  if ((int2_flags & kFieldQueued) != 0) order.b_queued = true;

  if ((int2_flags & kFieldExtraActors) != 0) {
    const auto opt_count{read_i4le(reader)};
    if (!opt_count.has_value()) return std::nullopt;
    const std::size_t idx_remaining{reader.span_bytes.size() - reader.idx_pos};
    if (*opt_count < 0 || static_cast<std::uint64_t>(*opt_count) > idx_remaining / 4)
      return std::nullopt;
    order.b_has_extra_actors = true;
    for (std::int32_t idx{}; idx < *opt_count; ++idx) {
      const auto opt_id{read_u4le(reader)};
      if (!opt_id.has_value()) return std::nullopt;
      order.vec_extra_actors.push_back(*opt_id);
    }
  }

  if ((int2_flags & kFieldExtraLocation) != 0) {
    const auto opt_bits{read_i4le(reader)};
    if (!opt_bits.has_value()) return std::nullopt;
    order.int4_extra_location_bits = *opt_bits;
  }

  if ((int2_flags & kFieldExtraData) != 0) {
    const auto opt_data{read_u4le(reader)};
    if (!opt_data.has_value()) return std::nullopt;
    order.u4_extra_data = *opt_data;
  }

  if ((int2_flags & kFieldGrouped) != 0) {
    const auto opt_count{read_i4le(reader)};
    if (!opt_count.has_value()) return std::nullopt;
    const std::size_t idx_remaining{reader.span_bytes.size() - reader.idx_pos};
    if (*opt_count < 0 || static_cast<std::uint64_t>(*opt_count) > idx_remaining / 4)
      return std::nullopt;
    order.b_has_grouped = true;
    for (std::int32_t idx{}; idx < *opt_count; ++idx) {
      const auto opt_id{read_u4le(reader)};
      if (!opt_id.has_value()) return std::nullopt;
      order.vec_grouped.push_back(*opt_id);
    }
  }

  if (b_subject_dropped) return std::nullopt;
  return order;
}

}
