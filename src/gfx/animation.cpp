// UPSTREAM: OpenRA.Game/Graphics/Animation.cs @b6fc03f L21-261 +
//           OpenRA.Game/Graphics/AnimationWithOffset.cs @b6fc03f L17-60
//          (animation.hpp 的实现;头注的形态适配说明适用)
//          Implementation of animation.hpp; the shape-adaptation notes of
//          the hpp header apply.
import std;
#include "gfx/animation.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::gfx {

namespace {

/// string.ToLowerInvariant(ASCII 域;上游调用点均为序列/图像名)。
/// string.ToLowerInvariant (the ASCII domain; upstream's call sites are all
/// sequence/image names).
std::string ToLowerInvariantAscii(std::string str_source) {
  for (char& chr_c : str_source)
    if (chr_c >= 'A' && chr_c <= 'Z')
      chr_c = static_cast<char>(chr_c - 'A' + 'a');
  return str_source;
}

}  // namespace

Animation::Animation(Deps deps, std::string str_name, std::function<WAngle()> fn_facing,
                     std::function<bool()> fn_paused)
    : deps_{std::move(deps)},
      fn_facing_{std::move(fn_facing)},
      fn_paused_{std::move(fn_paused)},
      str_name_{ToLowerInvariantAscii(std::move(str_name))} {
  if (!fn_facing_)
    fn_facing_ = DefaultFacing();
}

std::int32_t Animation::CurrentFrame() const {
  return b_backwards_ ? ptr_current_sequence_->Length() - int4_frame_ - 1 : int4_frame_;
}

Sprite Animation::Image() {
  return ptr_current_sequence_->GetSprite(CurrentFrame(), fn_facing_());
}

void Animation::Render(WPos wpos_pos, WVec wvec_offset, std::int32_t int4_z_offset,
                       const PaletteReference* ptr_palette, std::array<RenderItem, 2>& arr_out,
                       std::int32_t& int4_count) {
  const TintModifiers kind_tint_modifiers =
      ptr_current_sequence_->IgnoreWorldTint() ? TintModifiers::IgnoreWorldTint : TintModifiers::None;
  const float fp4_alpha = ptr_current_sequence_->GetAlpha(CurrentFrame());
  const auto [sprite_image, wangle_rotation] =
      ptr_current_sequence_->GetSpriteWithRotation(CurrentFrame(), fn_facing_());
  const RenderItem item_image =
      MakeSpriteRenderable(sprite_image, wpos_pos, wvec_offset,
                           ptr_current_sequence_->ZOffset() + int4_z_offset, ptr_palette,
                           ptr_current_sequence_->Scale(), fp4_alpha, core::Vector3{1.0f, 1.0f, 1.0f},
                           kind_tint_modifiers, IsDecoration, wangle_rotation);

  const Sprite sprite_shadow = ptr_current_sequence_->GetShadow(CurrentFrame(), fn_facing_());
  if (sprite_shadow.ptr_sheet != nullptr) {
    const WDist wdist_height = deps_.fn_distance_above_terrain
                                   ? deps_.fn_distance_above_terrain(wpos_pos)
                                   : WDist{0};
    const std::int32_t int4_height = wdist_height.Length;

    const RenderItem item_shadow = MakeSpriteRenderable(
        sprite_shadow, wpos_pos, wvec_offset - WVec{0, 0, int4_height},
        ptr_current_sequence_->ShadowZOffset() + int4_z_offset + int4_height, ptr_palette,
        ptr_current_sequence_->Scale(), 1.0f, core::Vector3{1.0f, 1.0f, 1.0f}, kind_tint_modifiers,
        true, wangle_rotation);
    arr_out[0] = item_shadow;
    arr_out[1] = item_image;
    int4_count = 2;
    return;
  }

  arr_out[0] = item_image;
  int4_count = 1;
}

void Animation::Render(WPos wpos_pos, const PaletteReference* ptr_palette,
                       std::array<RenderItem, 2>& arr_out, std::int32_t& int4_count) {
  Render(wpos_pos, WVec{}, 0, ptr_palette, arr_out, int4_count);
}

void Animation::RenderUI(WorldRenderer& wr_world_renderer, int2 int2_pos, WVec wvec_offset,
                         std::int32_t int4_z_offset, const PaletteReference* ptr_palette,
                         std::array<RenderItem, 2>& arr_out, std::int32_t& int4_count,
                         float fp4_scale, float fp4_rotation) {
  fp4_scale *= ptr_current_sequence_->Scale();
  const core::Vector3 vec_screen_offset_components =
      wr_world_renderer.ScreenVectorComponents(wvec_offset);
  const int2 int2_screen_offset =
      int2::FromVector(core::Vector2{fp4_scale * vec_screen_offset_components.X,
                                     fp4_scale * vec_screen_offset_components.Y});
  const Sprite sprite_image = Image();
  const int2 int2_image_pos =
      int2_pos + int2_screen_offset -
      int2{static_cast<std::int32_t>(fp4_scale * sprite_image.vec_size.X / 2),
           static_cast<std::int32_t>(fp4_scale * sprite_image.vec_size.Y / 2)};
  const float fp4_alpha = ptr_current_sequence_->GetAlpha(CurrentFrame());
  const RenderItem item_image = MakeUISpriteRenderable(
      sprite_image, WPos{} + wvec_offset, int2_image_pos.ToVector2(),
      ptr_current_sequence_->ZOffset() + int4_z_offset, ptr_palette, fp4_scale, fp4_alpha,
      fp4_rotation);

  const Sprite sprite_shadow = ptr_current_sequence_->GetShadow(CurrentFrame(), fn_facing_());
  if (sprite_shadow.ptr_sheet != nullptr) {
    const int2 int2_shadow_pos =
        int2_pos - int2{static_cast<std::int32_t>(fp4_scale * sprite_shadow.vec_size.X / 2),
                        static_cast<std::int32_t>(fp4_scale * sprite_shadow.vec_size.Y / 2)};
    const RenderItem item_shadow = MakeUISpriteRenderable(
        sprite_shadow, WPos{} + wvec_offset, int2_shadow_pos.ToVector2(),
        ptr_current_sequence_->ShadowZOffset() + int4_z_offset, ptr_palette, fp4_scale, 1.0f,
        fp4_rotation);
    arr_out[0] = item_shadow;
    arr_out[1] = item_image;
    int4_count = 2;
    return;
  }

  arr_out[0] = item_image;
  int4_count = 1;
}

Rectangle Animation::ScreenBounds(WorldRenderer& wr_world_renderer, WPos wpos_pos,
                                  WVec wvec_offset) {
  const float fp4_scale = ptr_current_sequence_->Scale();
  const int2 int2_xy = wr_world_renderer.ScreenPxPosition(wpos_pos) +
                       wr_world_renderer.ScreenPxOffset(wvec_offset);
  const Rectangle rect_bounds = ptr_current_sequence_->Bounds();
  return Rectangle::FromLTRB(
      int2_xy.X + static_cast<std::int32_t>(rect_bounds.Left() * fp4_scale),
      int2_xy.Y + static_cast<std::int32_t>(rect_bounds.Top() * fp4_scale),
      int2_xy.X + static_cast<std::int32_t>(rect_bounds.Right() * fp4_scale),
      int2_xy.Y + static_cast<std::int32_t>(rect_bounds.Bottom() * fp4_scale));
}

void Animation::Play(const std::string& str_sequence_name) {
  PlayThen(str_sequence_name, nullptr);
}

std::int32_t Animation::CurrentSequenceTickOrDefault() const {
  // 25 fps == 40 ms(上游常量注释照抄)| 25 fps == 40 ms (the upstream
  // constant comment kept).
  constexpr std::int32_t kDefaultTick = 40;
  return ptr_current_sequence_ != nullptr ? ptr_current_sequence_->Tick() : kDefaultTick;
}

void Animation::PlaySequence(const std::string& str_sequence_name) {
  ptr_current_sequence_ = &GetSequence(str_sequence_name);
  int4_time_until_next_frame_ = CurrentSequenceTickOrDefault();
}

void Animation::PlayRepeating(const std::string& str_sequence_name) {
  b_backwards_ = false;
  b_tick_always_ = false;
  PlaySequence(str_sequence_name);

  int4_frame_ = 0;
  mode_tick_ = TickMode::Repeat;
  fn_after_ = nullptr;
}

bool Animation::ReplaceAnim(const std::string& str_sequence_name) {
  if (!HasSequence(str_sequence_name))
    return false;

  ptr_current_sequence_ = &GetSequence(str_sequence_name);
  int4_time_until_next_frame_ = std::min(CurrentSequenceTickOrDefault(), int4_time_until_next_frame_);
  int4_frame_ %= ptr_current_sequence_->Length();
  return true;
}

void Animation::PlayThen(const std::string& str_sequence_name, std::function<void()> fn_after) {
  b_backwards_ = false;
  b_tick_always_ = false;
  PlaySequence(str_sequence_name);

  int4_frame_ = 0;
  mode_tick_ = TickMode::Then;
  fn_after_ = std::move(fn_after);
}

void Animation::PlayBackwardsThen(const std::string& str_sequence_name,
                                  std::function<void()> fn_after) {
  PlayThen(str_sequence_name, std::move(fn_after));
  b_backwards_ = true;
}

void Animation::PlayFetchIndex(const std::string& str_sequence_name,
                               std::function<std::int32_t()> fn_fetch) {
  b_backwards_ = false;
  b_tick_always_ = true;
  PlaySequence(str_sequence_name);

  // 上游 frame = func() 先于 tickFunc 装配(func 参数已在手)
  // Upstream's frame = func() runs before the tickFunc assembly (the func
  // parameter is already in hand).
  fn_fetch_ = std::move(fn_fetch);
  int4_frame_ = fn_fetch_();
  mode_tick_ = TickMode::FetchIndex;
}

void Animation::PlayFetchDirection(const std::string& str_sequence_name,
                                   std::function<std::int32_t()> fn_direction) {
  b_tick_always_ = false;
  PlaySequence(str_sequence_name);

  int4_frame_ = 0;
  mode_tick_ = TickMode::FetchDirection;
  fn_direction_ = std::move(fn_direction);
}

void Animation::RunTickFunc() {
  switch (mode_tick_) {
    case TickMode::None:
      break;
    case TickMode::Repeat:
      ++int4_frame_;
      if (int4_frame_ >= ptr_current_sequence_->Length())
        int4_frame_ = 0;
      break;
    case TickMode::Then:
      ++int4_frame_;
      if (int4_frame_ >= ptr_current_sequence_->Length()) {
        int4_frame_ = ptr_current_sequence_->Length() - 1;
        mode_tick_ = TickMode::None;
        if (fn_after_)
          fn_after_();
      }
      break;
    case TickMode::FetchIndex:
      int4_frame_ = fn_fetch_();
      break;
    case TickMode::FetchDirection: {
      const std::int32_t int4_d = fn_direction_();
      if (int4_d > 0 && ++int4_frame_ >= ptr_current_sequence_->Length())
        int4_frame_ = 0;
      if (int4_d < 0 && --int4_frame_ < 0)
        int4_frame_ = ptr_current_sequence_->Length() - 1;
      break;
    }
  }
}

void Animation::Tick() {
  if (!fn_paused_ || !fn_paused_())
    Tick(40);  // tick one frame(上游注释)| tick one frame (upstream comment)
}

void Animation::Tick(std::int32_t int4_t) {
  if (b_tick_always_)
    RunTickFunc();
  else {
    int4_time_until_next_frame_ -= int4_t;
    while (int4_time_until_next_frame_ <= 0) {
      RunTickFunc();
      int4_time_until_next_frame_ += CurrentSequenceTickOrDefault();
    }
  }
}

void Animation::ChangeImage(std::string str_new_image, const std::string& str_new_anim_if_missing) {
  str_new_image = ToLowerInvariantAscii(std::move(str_new_image));

  if (str_name_ != str_new_image) {
    str_name_ = str_new_image;
    if (!ReplaceAnim(std::string{ptr_current_sequence_->Name()}))
      ReplaceAnim(str_new_anim_if_missing);
  }
}

bool Animation::HasSequence(const std::string& str_seq) const {
  return deps_.ptr_sequences->HasSequence(str_name_, str_seq);
}

ISpriteSequence& Animation::GetSequence(const std::string& str_sequence_name) const {
  return deps_.ptr_sequences->GetSequence(str_name_, str_sequence_name);
}

std::string Animation::GetRandomExistingSequence(std::span<const std::string> vec_sequences,
                                                 MersenneTwister& random) const {
  // sequences.Where(HasSequence).RandomOrDefault(random)(L257-260)
  std::vector<const std::string*> vec_existing;
  for (const std::string& str_candidate : vec_sequences)
    if (HasSequence(str_candidate))
      vec_existing.push_back(&str_candidate);

  // RandomOrDefault:空集 → default(null → 空串);xs.ElementAt(r.Next(count))
  // 的 r.Next 可负 → 上游 ElementAt 抛 ArgumentOutOfRangeException(等价抛)
  // RandomOrDefault: an empty set → default (null → the empty string);
  // xs.ElementAt(r.Next(count)) with a possibly-negative r.Next → upstream's
  // ElementAt throws ArgumentOutOfRangeException (thrown equivalently).
  if (vec_existing.empty())
    return std::string{};
  const std::int32_t int4_index = random.Next(0, static_cast<std::int32_t>(vec_existing.size()));
  if (int4_index < 0)
    throw std::runtime_error(
        "System.ArgumentOutOfRangeException: Index was out of range. Must be non-negative and "
        "less than the size of the collection. (Parameter 'index')");
  return *vec_existing[static_cast<std::size_t>(int4_index)];
}

// ———— AnimationWithOffset ————

std::int32_t AnimationWithOffset::Render(WPos wpos_center, const PaletteReference* ptr_palette,
                                         std::array<RenderItem, 2>& arr_out) {
  const WVec wvec_offset = OffsetFunc ? OffsetFunc() : WVec{};
  const std::int32_t int4_z =
      fn_z_offset_ ? fn_z_offset_(wpos_center + wvec_offset) : 0;
  std::int32_t int4_count = 0;
  AnimationRef->Render(wpos_center, wvec_offset, int4_z, ptr_palette, arr_out, int4_count);
  return int4_count;
}

}  // namespace ora::gfx
