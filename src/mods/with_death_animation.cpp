// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDeathAnimation.cs @b6fc03f
//          L18-127(statement-by-statement;机制对照见 with_death_animation.hpp)
//          Statement-by-statement; the mechanism mapping lives in
//          with_death_animation.hpp.
#include "mods/sprite_effect.hpp"
#include "mods/with_death_animation.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

/// DeathTypes 值:单标量或字符串数组皆收为后缀数组
/// A DeathTypes value: a lone scalar or a string array both become the
/// suffix array.
std::vector<std::string> ParseSequenceArray(const meta::GenericValue& gv_value) {
  std::vector<std::string> vec_out;
  if (const auto* str = std::get_if<std::string>(&gv_value.val)) {
    vec_out.emplace_back(*str);
    return vec_out;
  }
  if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(
          &gv_value.val))
    for (const meta::GenericValue& element : *arr)
      if (const auto* str = std::get_if<std::string>(&element.val))
        vec_out.emplace_back(*str);
  return vec_out;
}

}  // namespace

WithDeathAnimationInfoData WithDeathAnimationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithDeathAnimationInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "DeathSequence"))
    data.str_death_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "DeathSequencePalette"))
    data.str_death_sequence_palette = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "DeathPaletteIsPlayerPalette"))
    data.b_death_palette_is_player_palette = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "UseDeathTypeSuffix"))
    data.b_use_death_type_suffix = *v != 0;
  if (const auto v = sim::RecordFieldString(rec_info, "CrushedSequence"))
    data.str_crushed_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "CrushedSequencePalette"))
    data.str_crushed_sequence_palette = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info,
                                         "CrushedPaletteIsPlayerPalette"))
    data.b_crushed_palette_is_player_palette = *v != 0;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "DeathTypes"))
    if (const auto* dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *dict)
        if (const auto* str_key = std::get_if<std::string>(&key.val))
          data.vec_death_types.emplace_back(
              *str_key, ParseSequenceArray(value));
  if (const auto v = sim::RecordFieldString(rec_info, "FallbackSequence"))
    data.str_fallback_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "Delay"))
    data.int4_delay = static_cast<int>(*v);
  return data;
}

WithDeathAnimation::WithDeathAnimation(const ActorInitializer& init,
                                       WithDeathAnimationInfoData info)
    : sim::ConditionalTraitCore<WithDeathAnimation>(info.conditional),
      info_{std::move(info)} {
  Actor& self = init.Self();
  ptr_render_sprites_ = self.Trait<RenderSprites>();
}

void WithDeathAnimation::Killed(Actor& self, const sim::AttackInfo& e) {
  if (b_crushed_ || IsTraitDisabled())
    return;

  std::string str_palette = info_.str_death_sequence_palette;
  if (info_.b_death_palette_is_player_palette)
    str_palette += self.Owner()->InternalName();

  if (e.Damage->DamageTypes.IsEmpty()) {
    if (!info_.str_fallback_sequence.empty())
      SpawnDeathAnimation(self, self.CenterPosition(),
                          ptr_render_sprites_->GetImage(self),
                          info_.str_fallback_sequence, str_palette,
                          info_.int4_delay);
    return;
  }

  std::string str_sequence = info_.str_death_sequence;
  if (info_.b_use_death_type_suffix) {
    // 首个命中伤害类型的键胜(键序 = 插入序)
    // The first key hitting the damage type wins (the key order = the
    // insertion order).
    const std::vector<std::string>* vec_suffixes = nullptr;
    for (const auto& [str_name, vec_seqs] : info_.vec_death_types)
      if (e.Damage->DamageTypes.Contains(str_name)) {
        vec_suffixes = &vec_seqs;
        break;
      }
    if (vec_suffixes == nullptr)
      return;

    // Random(SharedRandom) = 同步域消耗
    // Random (SharedRandom) = a synced-domain consumption.
    str_sequence += (*vec_suffixes)[static_cast<std::size_t>(
        self.world().SharedRandom().Next(
            static_cast<std::int32_t>(vec_suffixes->size())))];
  }

  SpawnDeathAnimation(self, self.CenterPosition(),
                      ptr_render_sprites_->GetImage(self), str_sequence,
                      str_palette, info_.int4_delay);
}

void WithDeathAnimation::SpawnDeathAnimation(Actor& self, WPos pos,
                                             std::string_view str_image,
                                             std::string_view str_sequence,
                                             std::string_view str_palette,
                                             int int4_delay) {
  sim::World* ptr_world = &self.world();
  ptr_world->AddFrameEndTask(
      [ptr_world, pos, str_image = std::string{str_image},
       str_sequence = std::string{str_sequence},
       str_palette = std::string{str_palette}, int4_delay](sim::World& w) {
        (void)w;
        ptr_world->Add(std::make_unique<SpriteEffect>(
            pos, *ptr_world, std::move(str_image), std::move(str_sequence),
            std::move(str_palette), false, int4_delay));
      });
}

void WithDeathAnimation::OnCrush(Actor& self, Actor& crusher,
                                 const core::BitSet<sim::CrushClass>&
                                     crush_classes) {
  (void)crusher;
  (void)crush_classes;
  b_crushed_ = true;

  if (info_.str_crushed_sequence.empty())
    return;

  std::string str_crush_palette = info_.str_crushed_sequence_palette;
  if (info_.b_crushed_palette_is_player_palette)
    str_crush_palette += self.Owner()->InternalName();

  SpawnDeathAnimation(self, self.CenterPosition(),
                      ptr_render_sprites_->GetImage(self),
                      info_.str_crushed_sequence, str_crush_palette,
                      info_.int4_delay);
}

}  // namespace ora::mods
