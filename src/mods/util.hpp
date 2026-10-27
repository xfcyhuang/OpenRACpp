#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/mersenne_twister.hpp"
#include "core/percent_modifiers.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "game/game_records.hpp"
#include "sim/target.hpp"
#include "sim/weapons.hpp"
#include "sim/world.hpp"

namespace ora::mods {

enum class InaccuracyType : std::int32_t {
  Maximum = 0,
  PerCellIncrement = 1,
  Absolute = 2,
};

inline int RandomInRange(MersenneTwister& random,
                         const std::vector<int>& vec_range) {
  if (vec_range.empty())
    return 0;
  if (vec_range.size() == 1)
    return vec_range.front();
  return random.Next(vec_range[0], vec_range[1]);
}

inline int TickFacingInt(int facing, int desired_facing, int rot) {
  const int left_turn = (facing - desired_facing) & 0xFF;
  if (left_turn < rot)
    return desired_facing & 0xFF;

  const int right_turn = (desired_facing - facing) & 0xFF;
  if (right_turn < rot)
    return desired_facing & 0xFF;

  if (right_turn < left_turn)
    return (facing + rot) & 0xFF;

  return (facing - rot) & 0xFF;
}

inline int NormalizeFacing(int f) {
  if (f >= 0)
    return f & 0xFF;

  const int negative = -f & 0xFF;
  return negative == 0 ? 0 : 256 - negative;
}

inline WAngle TickFacing(WAngle facing, WAngle desired_facing, WAngle step) {
  const int left_turn = (facing - desired_facing).Angle;
  if (left_turn < step.Angle)
    return desired_facing;

  const int right_turn = (desired_facing - facing).Angle;
  if (right_turn < step.Angle)
    return desired_facing;

  return right_turn < left_turn ? facing + step : facing - step;
}

inline int GetTurnDirection(WAngle facing, WAngle desired_facing) {
  return (facing - desired_facing).Angle < 512 ? -1 : 1;
}

inline int IndexFacing(WAngle facing, int num_frames) {

  const int step = 1024 / num_frames;
  const int a = (facing.Angle + step / 2) & 1023;
  return a / step;
}

inline WAngle QuantizeFacing(WAngle facing, int facings) {
  return WAngle{IndexFacing(facing, facings) * (1024 / facings)};
}

inline WPos BetweenCells(sim::World& w, CPos from, CPos to) {
  const WPos from_pos =
      from.Layer() == 0
          ? w.Map().CenterOfCell(from)
          : w.ActorMapFace()->CustomMovementLayers()[from.Layer()]->CenterOfCell(from);

  const WPos to_pos =
      to.Layer() == 0
          ? w.Map().CenterOfCell(to)
          : w.ActorMapFace()->CustomMovementLayers()[to.Layer()]->CenterOfCell(to);

  return WPos::Lerp(from_pos, to_pos, 1, 2);
}

inline bool AreAdjacentCells(CPos a, CPos b) {
  const CVec offset = b - a;
  return std::abs(offset.X) < 2 && std::abs(offset.Y) < 2;
}

inline void AppendNeighbours(std::vector<CPos>& vec_out, CPos cell,
                             bool allow_diagonal) {
  vec_out.push_back(cell);
  vec_out.push_back(CPos{cell.X() - 1, cell.Y()});
  vec_out.push_back(CPos{cell.X() + 1, cell.Y()});
  vec_out.push_back(CPos{cell.X(), cell.Y() - 1});
  vec_out.push_back(CPos{cell.X(), cell.Y() + 1});
  if (allow_diagonal) {
    vec_out.push_back(CPos{cell.X() - 1, cell.Y() - 1});
    vec_out.push_back(CPos{cell.X() + 1, cell.Y() - 1});
    vec_out.push_back(CPos{cell.X() - 1, cell.Y() + 1});
    vec_out.push_back(CPos{cell.X() + 1, cell.Y() + 1});
  }
}

inline std::vector<CPos> AdjacentCells(sim::World& w, const sim::Target& target) {

  std::vector<CPos> vec_cells;
  for (const auto& p : target.Positions()) {
    const CPos cell = w.Map().CellContaining(p);
    if (std::find(vec_cells.begin(), vec_cells.end(), cell) == vec_cells.end())
      vec_cells.push_back(cell);
  }

  std::vector<CPos> vec_expanded;
  for (const CPos cell : vec_cells)
    AppendNeighbours(vec_expanded, cell, true);

  std::vector<CPos> vec_distinct;
  for (const CPos cell : vec_expanded)
    if (std::find(vec_distinct.begin(), vec_distinct.end(), cell) ==
        vec_distinct.end())
      vec_distinct.push_back(cell);
  return vec_distinct;
}

inline bool FacingWithinTolerance(WAngle facing, WAngle desired_facing,
                                  WAngle facing_tolerance) {
  if (facing_tolerance.Angle == 0 && facing == desired_facing)
    return true;

  const int delta = (desired_facing - facing).Angle;
  return delta <= facing_tolerance.Angle ||
         delta >= 1024 - facing_tolerance.Angle;
}

inline WAngle GetVerticalAngle(const WPos& source, const WPos& target) {
  const WVec delta = target - source;
  const std::int32_t horizontal_delta = delta.HorizontalLength();
  const WVec vertical_vector{-delta.Z, -horizontal_delta, 0};

  return vertical_vector.Yaw();
}

inline std::int32_t GetProjectileInaccuracy(
    std::int32_t int4_base_inaccuracy, InaccuracyType inaccuracy_type,
    const sim::ProjectileArgs& args) {
  const std::int32_t inaccuracy = ApplyPercentageModifiers(
      int4_base_inaccuracy, args.vec_inaccuracy_modifiers);
  switch (inaccuracy_type) {
    case InaccuracyType::Maximum: {
      const std::int32_t weapon_max_range = ApplyPercentageModifiers(
          args.weapon->int4_range, args.vec_range_modifiers);
      return inaccuracy *
             (args.passive_target - args.source).Length() /
             weapon_max_range;
    }
    case InaccuracyType::PerCellIncrement:
      return inaccuracy * (args.passive_target - args.source).Length() /
             1024;
    case InaccuracyType::Absolute:
      return inaccuracy;
    default:
      throw std::invalid_argument("inaccuracy_type");
  }
}

}
