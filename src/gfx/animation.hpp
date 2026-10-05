// UPSTREAM: OpenRA.Game/Graphics/Animation.cs @b6fc03f L21-261(全文逐语义)+
//           OpenRA.Game/Graphics/AnimationWithOffset.cs @b6fc03f L17-60(全文)
// 动画状态机:Play*(五族 — Repeating/Then/BackwardsThen/FetchIndex/FetchDirection)
// 装配 tickFunc 委托,Tick(t) 的 timeUntilNextFrame 负债循环(while <= 0 追帧)
// 与 tickAlways 旁路逐句照抄;CurrentFrame 的 backwards 反转索引;Render 家
// 族的 shadow 两件套(高度 = map.DistanceAboveTerrain(pos).Length 的地面下
// 投 + ShadowZOffset)与 RenderUI 的 int2.FromVector/(int) 截断算式逐句。
// 形态适配:
//   - ctor(World,…) → Deps 注入(sequences + 地形高度查询;上游 world.Map
//     的两依赖拆出 —— Map 随 Phase 5,默认高度面 = WDist 0);
//   - tickFunc 闭包族 → TickMode 枚举 + after/fetch/direction 三回调(语义
//     一一对应,Play* 时装配);
//   - IRenderable[] 返回 → out 数组(≤ 2 项:shadow + image);
//   - RandomOrDefault 的 r.Next 可负(上游 ElementAt 负索引抛
//     ArgumentOutOfRangeException)—— 等价抛;.NET 消息依版本,取 .NET 10
//     形态文本。
// The animation state machine: the Play* family (five flavors — Repeating/
// Then/BackwardsThen/FetchIndex/FetchDirection) assembles the tickFunc
// delegate, with Tick(t)'s timeUntilNextFrame debt loop (the while <= 0
// catch-up) and the tickAlways bypass copied statement by statement;
// CurrentFrame's backwards index reversal; the Render family's shadow pair
// (the height = map.DistanceAboveTerrain(pos).Length ground projection +
// ShadowZOffset) and RenderUI's int2.FromVector/(int)-truncation arithmetic
// verbatim. Shape adaptations:
//   - ctor(World,…) → the Deps injection (sequences + the terrain-height
//     query; the two dependencies upstream pulls from world.Map split out —
//     Map arrives in Phase 5, the default height face = WDist 0);
//   - the tickFunc closure family → the TickMode enum plus the three
//     after/fetch/direction callbacks (one-to-one semantics, assembled at
//     Play*);
//   - the IRenderable[] returns → out arrays (at most 2 items: shadow +
//     image);
//   - RandomOrDefault's r.Next can go negative (upstream's ElementAt throws
//     ArgumentOutOfRangeException on the negative index) — thrown
//     equivalently; the .NET message is version-dependent, taking the .NET
//     10 shape.
#pragma once
import std;

#include "core/mersenne_twister.hpp"
#include "core/rectangle.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wvec.hpp"
#include "gfx/palette.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sequence_set.hpp"

namespace ora::gfx {

class WorldRenderer;

/// Animation(Animation.cs L21-261)。
class Animation {
 public:
  /// 上游 ctor 的注入等价(world.Map.Sequences + world.Map 的高度查询)。
  /// The injected equivalents of the upstream ctor (world.Map.Sequences +
  /// world.Map's height query).
  struct Deps {
    const SequenceSet* ptr_sequences = nullptr;
    std::function<WDist(WPos)> fn_distance_above_terrain;  // 缺省 = 平地 0 | default = flat ground 0
  };

  /// (world, name) / (world, name, facingFunc) / (world, name, paused) /
  /// (world, name, facingFunc, paused) 四构造的合并形态。
  /// The merged form of the four upstream constructors (world, name) /
  /// (world, name, facingFunc) / (world, name, paused) / (world, name,
  /// facingFunc, paused).
  Animation(Deps deps, std::string str_name,
            std::function<WAngle()> fn_facing = {} /* = WAngle.Zero */,
            std::function<bool()> fn_paused = {});

  /// 默认 facing(上游 `() =&gt; WAngle.Zero`)。
  /// The default facing (upstream's `() => WAngle.Zero`).
  static std::function<WAngle()> DefaultFacing() { return [] { return WAngle{}; }; }

  ISpriteSequence* CurrentSequence() const { return ptr_current_sequence_; }
  void SetCurrentSequence(ISpriteSequence* ptr_sequence) { ptr_current_sequence_ = ptr_sequence; }
  const std::string& Name() const { return str_name_; }
  bool IsDecoration = false;

  /// CurrentFrame(L56):backwards 时反转索引。
  /// CurrentFrame (L56): the reversed index when backwards.
  std::int32_t CurrentFrame() const;

  /// Image(L58)。
  Sprite Image();

  /// Render(L60-82):shadow + image 双件出(out 数组;count = 实际项数)。
  /// Render (L60-82): the shadow + image pair out (the out array; count =
  /// the actual item count).
  void Render(WPos wpos_pos, WVec wvec_offset, std::int32_t int4_z_offset,
              const PaletteReference* ptr_palette, std::array<RenderItem, 2>& arr_out,
              std::int32_t& int4_count);
  void Render(WPos wpos_pos, const PaletteReference* ptr_palette, std::array<RenderItem, 2>& arr_out,
              std::int32_t& int4_count);

  /// RenderUI(L84-103)。
  void RenderUI(WorldRenderer& wr_world_renderer, int2 int2_pos, WVec wvec_offset,
                std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                std::array<RenderItem, 2>& arr_out, std::int32_t& int4_count,
                float fp4_scale = 1.0f, float fp4_rotation = 0.0f);

  /// ScreenBounds(L105-115)。
  Rectangle ScreenBounds(WorldRenderer& wr_world_renderer, WPos wpos_pos, WVec wvec_offset);

  void Play(const std::string& str_sequence_name);
  void PlayRepeating(const std::string& str_sequence_name);
  /// ReplaceAnim(L154-163):无该序列返回 false。
  /// ReplaceAnim (L154-163): false when the sequence is absent.
  bool ReplaceAnim(const std::string& str_sequence_name);
  void PlayThen(const std::string& str_sequence_name, std::function<void()> fn_after);
  void PlayBackwardsThen(const std::string& str_sequence_name, std::function<void()> fn_after);
  void PlayFetchIndex(const std::string& str_sequence_name, std::function<std::int32_t()> fn_fetch);
  void PlayFetchDirection(const std::string& str_sequence_name,
                          std::function<std::int32_t()> fn_direction);

  /// Tick(L217-221):未暂停时前进一帧(40ms)。
  /// Tick (L217-221): one frame (40ms) forward when unpaused.
  void Tick();

  /// Tick(t)(L223-236)。
  void Tick(std::int32_t int4_t);

  /// ChangeImage(L238-248)。
  void ChangeImage(std::string str_new_image, const std::string& str_new_anim_if_missing);

  bool HasSequence(const std::string& str_seq) const;
  ISpriteSequence& GetSequence(const std::string& str_sequence_name) const;

  /// GetRandomExistingSequence(L257-260):过滤存在序 + RandomOrDefault。
  /// GetRandomExistingSequence (L257-260): the exists-filtered order +
  /// RandomOrDefault.
  std::string GetRandomExistingSequence(std::span<const std::string> vec_sequences,
                                        MersenneTwister& random) const;

 private:
  /// CurrentSequenceTickOrDefault(L127-131):null 序列 = 40(25fps)。
  /// CurrentSequenceTickOrDefault (L127-131): a null sequence = 40 (25fps).
  std::int32_t CurrentSequenceTickOrDefault() const;

  void PlaySequence(const std::string& str_sequence_name);

  /// 上游 tickFunc 闭包族的枚举化(装配于 Play*)。
  /// The enumerated form of upstream's tickFunc closure family (assembled by
  /// Play*).
  enum class TickMode {
    None,           // tickFunc == null
    Repeat,         // ++frame;>= Length → 0
    Then,           // ++frame;>= Length → Length-1 + 结束回调
    FetchIndex,     // frame = fetch()(tickAlways)
    FetchDirection, // d&gt;0 ++ / d&lt;0 -- 双向回绕
  };

  void RunTickFunc();

  Deps deps_;
  std::function<WAngle()> fn_facing_;
  std::function<bool()> fn_paused_;

  ISpriteSequence* ptr_current_sequence_ = nullptr;
  std::string str_name_;

  std::int32_t int4_frame_ = 0;
  bool b_backwards_ = false;
  bool b_tick_always_ = false;
  std::int32_t int4_time_until_next_frame_ = 0;
  TickMode mode_tick_ = TickMode::None;
  std::function<void()> fn_after_;
  std::function<std::int32_t()> fn_fetch_;
  std::function<std::int32_t()> fn_direction_;
};

/// AnimationWithOffset(AnimationWithOffset.cs L17-60):偏移/禁用/ZOffset 三
/// 回调 + 隐式包 Animation 的伴随件。
/// AnimationWithOffset (AnimationWithOffset.cs L17-60): the offset/disable/
/// ZOffset callbacks plus the implicit-Animation companion.
class AnimationWithOffset {
 public:
  AnimationWithOffset(Animation& anim, std::function<WVec()> fn_offset,
                      std::function<bool()> fn_disable)
      : AnimationRef{&anim}, OffsetFunc{std::move(fn_offset)}, DisableFunc{std::move(fn_disable)} {}

  AnimationWithOffset(Animation& anim, std::function<WVec()> fn_offset,
                      std::function<bool()> fn_disable, std::int32_t int4_z_offset)
      : AnimationWithOffset(anim, std::move(fn_offset), std::move(fn_disable)) {
    fn_z_offset_ = [int4_z_offset](WPos) { return int4_z_offset; };
  }

  AnimationWithOffset(Animation& anim, std::function<WVec()> fn_offset,
                      std::function<bool()> fn_disable, std::function<std::int32_t(WPos)> fn_z_offset)
      : AnimationWithOffset(anim, std::move(fn_offset), std::move(fn_disable)) {
    fn_z_offset_ = std::move(fn_z_offset);
  }

  /// 上游隐式运算符 Animation → AnimationWithOffset 的等价工厂。
  /// The equivalent factory of upstream's implicit Animation →
  /// AnimationWithOffset operator.
  static AnimationWithOffset Wrap(Animation& anim) { return AnimationWithOffset{anim, {}, {}}; }

  Animation* AnimationRef = nullptr;
  std::function<WVec()> OffsetFunc;
  std::function<bool()> DisableFunc;

  /// ZOffset(L22;上游公有字段)。null = 0。
  /// ZOffset (L22; an upstream public field). null = 0.
  std::function<std::int32_t(WPos)> fn_z_offset_;

  /// Render(L38-45):center = 调用方注入的 Actor 中心位;返回计数。
  /// Render (L38-45): center = the actor-center position injected by the
  /// caller; returns the item count.
  std::int32_t Render(WPos wpos_center, const PaletteReference* ptr_palette,
                      std::array<RenderItem, 2>& arr_out);
};

}  // namespace ora::gfx
