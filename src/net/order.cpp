// UPSTREAM: OpenRA.Game/Network/Order.cs @7d57605(实现部分:逐字节序列化)
//          The implementation half of Order.cs (byte-exact serialization).
#include "net/order.hpp"

#include <cstdio>

#include "sim/world.hpp"

namespace ora::net {

namespace {

/// Order.cs L261-265
std::uint32_t UIntFromActor(const sim::Actor* a) {
  if (a == nullptr)
    return std::numeric_limits<std::uint32_t>::max();
  return a->ActorID();
}

/// Order.cs L267-277
bool TryGetActorFromUInt(sim::World* world, std::uint32_t a_id,
                         sim::Actor*& ret) {
  if (a_id == std::numeric_limits<std::uint32_t>::max()) {
    ret = nullptr;
    return true;
  }

  ret = world->GetActorById(a_id);
  return ret != nullptr;
}

/// Order.cs L254-259
bool HasRemainingBytes(ByteReader& r, int count, int item_size) {
  const auto remaining_bytes = r.Remaining();
  return count >= 0 &&
         count <= std::numeric_limits<int>::max() / item_size &&
         count <= remaining_bytes / item_size;
}

/// Order.cs L248-252
bool TryReadActorCount(ByteReader& r, int& count) {
  count = r.ReadInt32();
  return HasRemainingBytes(r, count, sizeof(std::uint32_t));
}

/// 上游 unknown order 的 Log.Write("debug", ...) 面(日志通道 Phase 5;
/// stderr 兜底)
void LogDebugUnknownOrder(std::uint8_t type) {
  std::fprintf(stderr, "Received unknown order with type %d\n",
               static_cast<int>(type));
}

}  // namespace

// ———— 构造族(Order.cs L81-93 主构造 / L333-343 公开重载)————
// ———— The constructor family (the L81-93 master / the L333-343 public
//      overloads) ————

sim::Player* Order::Player() const {
  // L76:Subject?.Owner
  return p_subject != nullptr ? p_subject->Owner() : nullptr;
}

Order::Order(std::string str_order, sim::Actor* subject, bool queued,
             std::optional<std::vector<sim::Actor*>> vec_extra_actors,
             std::optional<std::vector<sim::Actor*>> vec_grouped_actors)
    : Order(std::move(str_order), subject, sim::Target::Invalid(), queued,
            std::move(vec_extra_actors)) {
  vec_grouped_actors = std::move(vec_grouped_actors);
}

Order::Order(std::string str_order, sim::Actor* subject,
             const sim::Target& target, bool queued,
             std::optional<std::vector<sim::Actor*>> vec_extra_actors)
    : str_order_string(std::move(str_order)),
      p_subject(subject),
      b_queued(queued),
      target(target),
      vec_extra_actors(std::move(vec_extra_actors)) {}

// ———— 命名构造(Order.cs L281-327)————
// ———— The named constructors (Order.cs L281-327) ————

Order Order::Chat(std::string_view text, std::uint32_t team_number) {
  Order o{std::string(text), nullptr, false};
  o.b_is_immediate = true;
  o.str_target_string = std::string(text);
  o.uint4_extra_data = team_number;
  return o;
}

Order Order::FromTargetString(std::string_view order,
                              std::string_view target_string,
                              bool is_immediate) {
  Order o{std::string(order), nullptr, false};
  o.b_is_immediate = is_immediate;
  o.str_target_string = std::string(target_string);
  return o;
}

Order Order::FromTargetString(std::string_view order,
                              std::string_view target_string,
                              bool is_immediate, std::uint32_t extra_data) {
  auto o = FromTargetString(order, target_string, is_immediate);
  o.uint4_extra_data = extra_data;
  return o;
}

Order Order::FromGroupedOrder(const Order& grouped, sim::Actor* subject) {
  // L296-307(VisualFeedbackTarget 不带 —— 上游同名行为)
  Order o{grouped.str_order_string, subject, grouped.target,
          grouped.b_queued, grouped.vec_extra_actors};
  o.str_target_string = grouped.str_target_string;
  o.extra_location = grouped.extra_location;
  o.uint4_extra_data = grouped.uint4_extra_data;
  return o;
}

Order Order::Command(std::string_view text) {
  Order o{"Command", nullptr, false};
  o.b_is_immediate = true;
  o.str_target_string = std::string(text);
  return o;
}

Order Order::StartProduction(sim::Actor* subject, std::string_view item,
                             int count, bool queued) {
  Order o{"StartProduction", subject, queued};
  o.uint4_extra_data = static_cast<std::uint32_t>(count);
  o.str_target_string = std::string(item);
  return o;
}

Order Order::PauseProduction(sim::Actor* subject, std::string_view item,
                             bool pause) {
  Order o{"PauseProduction", subject, false};
  o.uint4_extra_data = pause ? 1u : 0u;
  o.str_target_string = std::string(item);
  return o;
}

Order Order::CancelProduction(sim::Actor* subject, std::string_view item,
                              int count) {
  Order o{"CancelProduction", subject, false};
  o.uint4_extra_data = static_cast<std::uint32_t>(count);
  o.str_target_string = std::string(item);
  return o;
}

// ———— Deserialize(Order.cs L95-246)————
// ———— Deserialize (Order.cs L95-246) ————

std::unique_ptr<Order> Order::Deserialize(sim::World* world, ByteReader& r) {
  try {
    const auto type = static_cast<OrderType>(r.ReadByte());
    switch (type) {
      case OrderType::Fields: {
        auto order = r.ReadString();
        const auto flags = static_cast<OrderFields>(r.ReadInt16());

        sim::Actor* subject = nullptr;
        if (HasField(flags, OrderFields::Subject)) {
          const auto subject_id = r.ReadUInt32();
          if (world != nullptr)
            TryGetActorFromUInt(world, subject_id, subject);
        }

        auto target = sim::Target::Invalid();
        if (HasField(flags, OrderFields::Target)) {
          switch (static_cast<sim::TargetType>(r.ReadByte())) {
            case sim::TargetType::Actor: {
              const auto actor_id = r.ReadUInt32();
              const auto actor_generation = r.ReadInt32();
              if (world != nullptr) {
                sim::Actor* target_actor = nullptr;
                if (TryGetActorFromUInt(world, actor_id, target_actor))
                  target = sim::Target::FromSerializedActor(
                      target_actor, actor_generation);
              }
              break;
            }

            case sim::TargetType::FrozenActor: {
              // FrozenActorLayer(Shroud 链,Phase 5):上游在层缺失时保持
              // Invalid —— 读序保留,物化面未接线
              const auto player_actor_id = r.ReadUInt32();
              const auto frozen_actor_id = r.ReadUInt32();
              (void)player_actor_id;
              (void)frozen_actor_id;
              break;
            }

            case sim::TargetType::Terrain: {
              if (HasField(flags, OrderFields::TargetIsCell)) {
                const auto cell = CPos{r.ReadInt32()};
                const auto sub_cell =
                    static_cast<sim::SubCell>(r.ReadByte());
                if (world != nullptr)
                  target = world->TargetFromCell(cell, sub_cell);
              } else {
                const WPos pos{r.ReadInt32(), r.ReadInt32(), r.ReadInt32()};

                const auto number_of_terrain_positions = r.ReadInt16();
                if (number_of_terrain_positions == -1)
                  target = sim::Target::FromPos(pos);
                else if (!HasRemainingBytes(r, number_of_terrain_positions,
                                            3 * sizeof(std::int32_t)))
                  return nullptr;
                else {
                  std::vector<WPos> terrain_positions(
                      static_cast<std::size_t>(number_of_terrain_positions));
                  for (auto& tp : terrain_positions)
                    tp = WPos{r.ReadInt32(), r.ReadInt32(), r.ReadInt32()};

                  target = sim::Target::FromSerializedTerrainPosition(
                      pos, std::move(terrain_positions));
                }
              }

              break;
            }

            default:
              break;
          }
        }

        auto target_string = HasField(flags, OrderFields::TargetString)
                                 ? std::optional<std::string>(r.ReadString())
                                 : std::nullopt;
        const auto queued = HasField(flags, OrderFields::Queued);

        std::optional<std::vector<sim::Actor*>> extra_actors;
        if (HasField(flags, OrderFields::ExtraActors)) {
          int count = 0;
          if (!TryReadActorCount(r, count))
            return nullptr;

          if (world != nullptr) {
            std::vector<sim::Actor*> actors(
                static_cast<std::size_t>(count));
            for (auto& a : actors)
              a = world->GetActorById(r.ReadUInt32());
            extra_actors = std::move(actors);
          } else {
            std::vector<std::uint8_t> discard;
            r.ReadBytes(static_cast<std::size_t>(4 * count), discard);
          }
        }

        const auto extra_location = HasField(flags, OrderFields::ExtraLocation)
                                        ? CPos{r.ReadInt32()}
                                        : CPos{};
        const auto extra_data =
            HasField(flags, OrderFields::ExtraData) ? r.ReadUInt32() : 0;

        std::optional<std::vector<sim::Actor*>> grouped_actors;
        if (HasField(flags, OrderFields::Grouped)) {
          int count = 0;
          if (!TryReadActorCount(r, count))
            return nullptr;

          if (world != nullptr) {
            std::vector<sim::Actor*> actors(
                static_cast<std::size_t>(count));
            for (auto& a : actors)
              a = world->GetActorById(r.ReadUInt32());
            grouped_actors = std::move(actors);
          } else {
            std::vector<std::uint8_t> discard;
            r.ReadBytes(static_cast<std::size_t>(4 * count), discard);
          }
        }

        if (world == nullptr) {
          auto o = std::make_unique<Order>(std::move(order), nullptr, target,
                                           queued);
          o->str_target_string = std::move(target_string);
          o->extra_location = extra_location;
          o->vec_extra_actors = std::move(extra_actors);
          o->vec_grouped_actors = std::move(grouped_actors);
          o->uint4_extra_data = extra_data;
          return o;
        }

        if (subject == nullptr && HasField(flags, OrderFields::Subject))
          return nullptr;

        auto o = std::make_unique<Order>(std::move(order), subject, target,
                                         queued, std::move(extra_actors));
        o->str_target_string = std::move(target_string);
        o->extra_location = extra_location;
        o->vec_grouped_actors = std::move(grouped_actors);
        o->uint4_extra_data = extra_data;
        return o;
      }

      case OrderType::Handshake: {
        auto name = r.ReadString();
        auto target_string = r.ReadString();

        auto o = std::make_unique<Order>(std::move(name), nullptr, false);
        o->type = OrderType::Handshake;
        o->str_target_string = std::move(target_string);
        return o;
      }

      default: {
        // Changing the Handshake order format will break cross-version
        // switching — unknown types are dropped (上游 Log + null)
        LogDebugUnknownOrder(static_cast<std::uint8_t>(type));
        return nullptr;
      }
    }
  } catch (const std::exception&) {
    // HACK: this can hopefully go away in the future(上游注释)——
    // 异常吞并返回 null;TextNotificationsManager 提示 Phase 6 接线
    return nullptr;
  }
}

// ———— Serialize(Order.cs L345-487)————
// ———— Serialize (Order.cs L345-487) ————

std::vector<std::uint8_t> Order::Serialize() const {
  ByteWriter w;

  w.Write(static_cast<std::uint8_t>(type));
  w.Write(str_order_string);

  switch (type) {
    case OrderType::Handshake: {
      // Changing the Handshake order format will break cross-version
      // switching. Don't do this unless you really have to!
      w.Write(str_target_string.value_or(""));

      break;
    }

    case OrderType::Fields: {
      const auto target_state = target.Serializable();

      auto flags = OrderFields::None;
      if (p_subject != nullptr)
        flags |= OrderFields::Subject;

      if (str_target_string.has_value())
        flags |= OrderFields::TargetString;

      if (uint4_extra_data != 0)
        flags |= OrderFields::ExtraData;

      if (target_state.type != sim::TargetType::Invalid)
        flags |= OrderFields::Target;

      if (b_queued)
        flags |= OrderFields::Queued;

      if (vec_grouped_actors.has_value())
        flags |= OrderFields::Grouped;

      if (vec_extra_actors.has_value())
        flags |= OrderFields::ExtraActors;

      if (target_state.has_cell &&
          (target_state.cell.Bits != CPos{}.Bits))
        flags |= OrderFields::TargetIsCell;

      w.Write(static_cast<std::int16_t>(flags));

      if (HasField(flags, OrderFields::Subject))
        w.Write(UIntFromActor(p_subject));

      if (HasField(flags, OrderFields::Target)) {
        w.Write(static_cast<std::uint8_t>(target_state.type));
        switch (target_state.type) {
          case sim::TargetType::Actor:
            w.Write(UIntFromActor(target_state.actor));
            w.Write(target_state.generation);
            break;
          case sim::TargetType::FrozenActor:
            // FrozenActor 面(Shroud 链,Phase 5);序列化侧对象未接线时
            // 不可达 —— 上游同点位直读 Viewer/FrozenActor
            throw std::runtime_error("Cannot serialize FrozenActor target");
          case sim::TargetType::Terrain:
            if (HasField(flags, OrderFields::TargetIsCell)) {
              w.Write(target_state.cell.Bits);
              w.Write(static_cast<std::uint8_t>(target_state.sub_cell));
            } else {
              w.Write(target_state.pos.X);
              w.Write(target_state.pos.Y);
              w.Write(target_state.pos.Z);

              // Don't send extra data over the network that will be
              // restored by the Target ctor
              const auto* positions = target_state.terrain_positions;
              const auto terrain_positions =
                  positions != nullptr ? positions->size() : std::size_t{0};
              if (terrain_positions == 1 && (*positions)[0] == target_state.pos)
                w.Write(static_cast<std::int16_t>(-1));
              else {
                w.Write(static_cast<std::int16_t>(terrain_positions));
                if (positions != nullptr) {
                  for (const auto& position : *positions) {
                    w.Write(position.X);
                    w.Write(position.Y);
                    w.Write(position.Z);
                  }
                }
              }
            }

            break;
          case sim::TargetType::Invalid:
          default:
            break;
        }
      }

      if (HasField(flags, OrderFields::TargetString))
        w.Write(*str_target_string);

      if (HasField(flags, OrderFields::ExtraActors)) {
        w.Write(static_cast<std::int32_t>(vec_extra_actors->size()));
        for (auto* a : *vec_extra_actors)
          w.Write(UIntFromActor(a));
      }

      if (HasField(flags, OrderFields::ExtraLocation))
        w.Write(extra_location.Bits);

      if (HasField(flags, OrderFields::ExtraData))
        w.Write(uint4_extra_data);

      if (HasField(flags, OrderFields::Grouped)) {
        w.Write(static_cast<std::int32_t>(vec_grouped_actors->size()));
        for (auto* a : *vec_grouped_actors)
          w.Write(UIntFromActor(a));
      }

      break;
    }

    default:
      throw std::runtime_error("Cannot serialize order type " +
                               std::to_string(static_cast<int>(type)));
  }

  return w.TakeBytes();
}

}  // namespace ora::net
