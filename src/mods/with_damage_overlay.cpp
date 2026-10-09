// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDamageOverlay.cs @b6fc03f
//          L18-165
#include "mods/render_sprites.hpp"
#include "mods/render_utils.hpp"
#include "mods/util.hpp"
#include "mods/with_damage_overlay.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::vector<int> ParseIntArray(const meta::GenericValue& gv_value) {
  std::vector<int> vec_out;
  if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv_value.val)) {
    for (std::size_t i = 0; i < tuple->uint1_count; ++i)
      vec_out.push_back(static_cast<int>(tuple->arr_ints[i]));
    return vec_out;
  }
  if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(
          &gv_value.val))
    for (const meta::GenericValue& element : *arr)
      if (const auto* tuple = std::get_if<meta::GenericTuple>(&element.val))
        if (tuple->uint1_count >= 1)
          vec_out.push_back(static_cast<int>(tuple->arr_ints[0]));
  return vec_out;
}

}  // namespace

WithDamageOverlayInfoData WithDamageOverlayInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithDamageOverlayInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "Image"))
    data.str_image = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "StartSequence"))
    data.str_start_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "LoopSequence"))
    data.str_loop_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "EndSequence"))
    data.str_end_sequence = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Offset"))
    if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
      if (tuple->uint1_count >= 3)
        data.wvec_offset = WVec{
            static_cast<std::int32_t>(tuple->arr_ints[0]),
            static_cast<std::int32_t>(tuple->arr_ints[1]),
            static_cast<std::int32_t>(tuple->arr_ints[2])};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "LoopCount"))
    data.vec_loop_count = ParseIntArray(*gv);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "InitialDelay"))
    data.vec_initial_delay = ParseIntArray(*gv);
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "DamageTypes"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.bitset_damage_types = core::BitSet<sim::DamageType>::FromRawBits(
          static_cast<std::uint64_t>(*n));
  if (const auto v = sim::RecordFieldInt(rec_info, "MinimumDamageState"))
    data.minimum_damage_state = static_cast<sim::DamageState>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "MaximumDamageState"))
    data.maximum_damage_state = static_cast<sim::DamageState>(*v);
  return data;
}

WithDamageOverlay::WithDamageOverlay(const ActorInitializer& init,
                                     WithDamageOverlayInfoData info)
    : sim::ConditionalTraitCore<WithDamageOverlay>(info.conditional),
      info_{std::move(info)},
      anim_{RenderSprites::MakeAnimationDeps(init.Self()), info_.str_image} {}

void WithDamageOverlay::Created(Actor& self) {
  auto* rs = self.Trait<RenderSprites>();
  auto* body = self.TraitOrDefault<BodyOrientation>();
  BodyOrientation* ptr_body = body;
  const WithDamageOverlayInfoData* ptr_info = &info_;

  std::function<WVec()> fn_offset;
  if (info_.wvec_offset != WVec{} && body != nullptr)
    fn_offset = [ptr_body, ptr_info, &self]() {
      return ptr_body->LocalToWorld(
          ptr_info->wvec_offset.Rotate(
              ptr_body->QuantizeOrientation(self.Orientation())));
    };

  awo_overlay_.emplace(anim_, std::move(fn_offset),
                       [this]() { return !b_is_playing_animation_; });
  rs->Add(*awo_overlay_, info_.str_palette, info_.b_is_player_palette);
}

void WithDamageOverlay::Damaged(Actor& self, const sim::AttackInfo& e) {
  if (IsTraitDisabled() ||
      e.damage_state < info_.minimum_damage_state ||
      e.damage_state > info_.maximum_damage_state) {
    b_is_playing_animation_ = false;
    return;
  }

  // 负伤害 = 治疗,忽略 | Negative damage = healing, ignored.
  if (e.Damage->Value < 0)
    return;

  if (!b_is_playing_animation_ && int4_delay <= -1) {
    int4_delay = RandomInRange(self.world().SharedRandom(),
                               info_.vec_initial_delay);
    if (int4_delay <= 0)
      StartAnimation(self);
  }
}

void WithDamageOverlay::Tick(Actor& self) {
  if (int4_delay < 0)
    return;

  // 延迟期内伤害状态可能漂移,出区间即复位
  // The damage state may drift during the delay window; leaving the range
  // resets.
  const sim::DamageState state = render::GetDamageState(self);
  if (state < info_.minimum_damage_state ||
      state > info_.maximum_damage_state)
    int4_delay = -1;
  else if (--int4_delay <= 0)
    StartAnimation(self);
}

void WithDamageOverlay::StartAnimation(Actor& self) {
  (void)self;
  int4_delay = -1;
  int4_loop_count = RandomInRange(self.world().SharedRandom(),
                                  info_.vec_loop_count);
  b_is_playing_animation_ = true;

  if (!info_.str_start_sequence.empty())
    anim_.PlayThen(info_.str_start_sequence, [this]() { PlayAnimation(-1); });
  else
    PlayAnimation(-1);
}

void WithDamageOverlay::PlayAnimation(int int4_animation_state) {
  if (!b_is_playing_animation_)
    return;

  ++int4_animation_state;
  if (int4_animation_state < int4_loop_count &&
      !info_.str_loop_sequence.empty())
    anim_.PlayThen(info_.str_loop_sequence, [this, int4_animation_state]() {
      PlayAnimation(int4_animation_state);
    });
  else if (!info_.str_end_sequence.empty())
    anim_.PlayThen(info_.str_end_sequence,
                   [this]() { b_is_playing_animation_ = false; });
  else
    b_is_playing_animation_ = false;
}

}  // namespace ora::mods
