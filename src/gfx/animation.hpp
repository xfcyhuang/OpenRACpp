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

class Animation {
 public:

  struct Deps {
    const SequenceSet* ptr_sequences = nullptr;
    std::function<WDist(WPos)> fn_distance_above_terrain;
  };

  Animation(Deps deps, std::string str_name,
            std::function<WAngle()> fn_facing = {} ,
            std::function<bool()> fn_paused = {});

  static std::function<WAngle()> DefaultFacing() { return [] { return WAngle{}; }; }

  ISpriteSequence* CurrentSequence() const { return ptr_current_sequence_; }
  void SetCurrentSequence(ISpriteSequence* ptr_sequence) { ptr_current_sequence_ = ptr_sequence; }
  const std::string& Name() const { return str_name_; }
  bool IsDecoration = false;

  std::int32_t CurrentFrame() const;

  Sprite Image();

  void Render(WPos wpos_pos, WVec wvec_offset, std::int32_t int4_z_offset,
              const PaletteReference* ptr_palette, std::array<RenderItem, 2>& arr_out,
              std::int32_t& int4_count);
  void Render(WPos wpos_pos, const PaletteReference* ptr_palette, std::array<RenderItem, 2>& arr_out,
              std::int32_t& int4_count);

  void RenderUI(WorldRenderer& wr_world_renderer, int2 int2_pos, WVec wvec_offset,
                std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                std::array<RenderItem, 2>& arr_out, std::int32_t& int4_count,
                float fp4_scale = 1.0f, float fp4_rotation = 0.0f);

  Rectangle ScreenBounds(WorldRenderer& wr_world_renderer, WPos wpos_pos, WVec wvec_offset);

  void Play(const std::string& str_sequence_name);
  void PlayRepeating(const std::string& str_sequence_name);

  bool ReplaceAnim(const std::string& str_sequence_name);
  void PlayThen(const std::string& str_sequence_name, std::function<void()> fn_after);
  void PlayBackwardsThen(const std::string& str_sequence_name, std::function<void()> fn_after);
  void PlayFetchIndex(const std::string& str_sequence_name, std::function<std::int32_t()> fn_fetch);
  void PlayFetchDirection(const std::string& str_sequence_name,
                          std::function<std::int32_t()> fn_direction);

  void Tick();

  void Tick(std::int32_t int4_t);

  void ChangeImage(std::string str_new_image, const std::string& str_new_anim_if_missing);

  bool HasSequence(const std::string& str_seq) const;
  ISpriteSequence& GetSequence(const std::string& str_sequence_name) const;

  std::string GetRandomExistingSequence(std::span<const std::string> vec_sequences,
                                        MersenneTwister& random) const;

 private:

  std::int32_t CurrentSequenceTickOrDefault() const;

  void PlaySequence(const std::string& str_sequence_name);

  enum class TickMode {
    None,
    Repeat,
    Then,
    FetchIndex,
    FetchDirection,
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

  static AnimationWithOffset Wrap(Animation& anim) { return AnimationWithOffset{anim, {}, {}}; }

  Animation* AnimationRef = nullptr;
  std::function<WVec()> OffsetFunc;
  std::function<bool()> DisableFunc;

  std::function<std::int32_t(WPos)> fn_z_offset_;

  std::int32_t Render(WPos wpos_center, const PaletteReference* ptr_palette,
                      std::array<RenderItem, 2>& arr_out);
};

}
