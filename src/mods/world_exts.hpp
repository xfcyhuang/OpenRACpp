// UPSTREAM: OpenRA.Mods.Common/WorldExtensions.cs @b6fc03f L~185-320
//          (FindActorsOnLine/FindBlockingActorsOnLine/FindActorsOnCircle/
//          MinimumPointLineProjection;逐语义重写)
//          FindActorsOnLine/FindBlockingActorsOnLine/FindActorsOnCircle/
//          MinimumPointLineProjection (verbatim-semantics rewrite).
#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "mods/hit_shape.hpp"
#include "sim/actor.hpp"
#include "sim/world.hpp"

namespace ora::mods {

/// WorldExtensions.cs L306-320:MinimumPointLineProjection(线段 AB 上距 C
/// 最近点;整数域长除防溢出照抄)
/// WorldExtensions.cs L306-320: MinimumPointLineProjection (the point on
/// segment AB closest to C; the integer-domain long-division overflow
/// guards kept).
inline WPos MinimumPointLineProjection(const WPos& line_start,
                                       const WPos& line_end,
                                       const WPos& point) {
  const std::int64_t squared_length =
      (line_end - line_start).HorizontalLengthSquared();

  // Line has zero length, so just use the lineEnd position as the closest
  // position.(上游注释)
  if (squared_length == 0)
    return line_end;

  // 上游点积的 long 中间量照抄(注释全文见上游)
  // Upstream's long intermediates kept (see the upstream comment).
  const std::int64_t x_diff =
      static_cast<std::int64_t>(point.X - line_end.X) *
      (line_start.X - line_end.X);
  const std::int64_t y_diff =
      static_cast<std::int64_t>(point.Y - line_end.Y) *
      (line_start.Y - line_end.Y);
  const std::int64_t t = x_diff + y_diff;

  // Beyond the 'target' end of the segment
  if (t < 0)
    return line_end;

  // Beyond the 'source' end of the segment
  if (t > squared_length)
    return line_start;

  // Projection falls on the segment
  return WPos::Lerp(line_end, line_start, static_cast<std::int32_t>(t),
                    static_cast<std::int32_t>(squared_length));
}

/// WorldExtensions.cs L~215-258:FindActorsOnLine(线上限宽内命中血量的
/// actor;onlyBlockers = 仅按 IBlocksProjectiles 者的最大半径扩张)
/// WorldExtensions.cs L~215-258: FindActorsOnLine (the actors whose health
/// radius meets the line within its width; onlyBlockers = the overscan
/// uses the largest IBlocksProjectiles actor radius alone).
inline std::vector<sim::Actor*> FindActorsOnLine(sim::World& world,
                                                 const WPos& line_start,
                                                 const WPos& line_end,
                                                 const WDist& line_width,
                                                 bool only_blockers = false) {
  // 上游注释全文照抄语义:方形粗选 + 逐 actor 的 health 半径/中心点检查
  // The upstream comment's semantics: the square pre-pass + the per-actor
  // health-radius/center check.
  const std::int32_t x_diff = line_end.X - line_start.X;
  const std::int32_t y_diff = line_end.Y - line_start.Y;
  const std::int32_t x_dir = x_diff < 0 ? -1 : 1;
  const std::int32_t y_dir = y_diff < 0 ? -1 : 1;

  const WVec dir{x_dir, y_dir, 0};
  const std::int32_t largest_valid_actor_radius =
      only_blockers
          ? world.ActorMapFace()->LargestBlockingActorRadius().Length
          : world.ActorMapFace()->LargestActorRadius().Length;
  const WVec overselect =
      dir * (1024 + line_width.Length + largest_valid_actor_radius);
  const WPos final_target = line_end + overselect;
  const WPos final_source = line_start - overselect;

  std::vector<sim::Actor*> vec_intersected_actors;
  for (sim::Actor* curr_actor :
       world.ActorMapFace()->ActorsInBox(final_target, final_source)) {
    std::int32_t actor_width = 0;

    // PERF: Avoid using TraitsImplementing<HitShape>...(上游注释;
    // EnabledTargetablePositions 的 HitShape 判 = dynamic_cast)
    // (the upstream PERF note; the EnabledTargetablePositions HitShape
    // test = a dynamic_cast).
    for (sim::ITargetablePositions* target_pos :
         curr_actor->EnabledTargetablePositions())
      if (auto* hitshape = dynamic_cast<HitShape*>(target_pos))
        actor_width = std::max(
            actor_width,
            hitshape->Info().ptr_type->OuterRadius().Length);

    const WPos projection = MinimumPointLineProjection(
        line_start, line_end, curr_actor->CenterPosition());
    const std::int32_t distance =
        (curr_actor->CenterPosition() - projection).HorizontalLength();
    const std::int32_t max_reach = actor_width + line_width.Length;

    if (distance <= max_reach)
      vec_intersected_actors.push_back(curr_actor);
  }

  return vec_intersected_actors;
}

/// WorldExtensions.cs L260-263:FindBlockingActorsOnLine
/// WorldExtensions.cs L260-263: FindBlockingActorsOnLine.
inline std::vector<sim::Actor*> FindBlockingActorsOnLine(
    sim::World& world, const WPos& line_start, const WPos& line_end,
    const WDist& line_width) {
  return FindActorsOnLine(world, line_start, line_end, line_width, true);
}

/// WorldExtensions.cs L262-266:FindActorsOnCircle
/// WorldExtensions.cs L262-266: FindActorsOnCircle.
inline std::vector<sim::Actor*> FindActorsOnCircle(sim::World& world,
                                                   const WPos& origin,
                                                   const WDist& r) {
  return world.FindActorsInCircle(
      origin, WDist{r.Length +
                    world.ActorMapFace()->LargestActorRadius().Length});
}

}  // namespace ora::mods
