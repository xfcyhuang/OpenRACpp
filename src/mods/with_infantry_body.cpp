#include "mods/with_infantry_body.hpp"

#include "mods/armament.hpp"
#include "sim/actor_init.hpp"
#include "sim/target.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::vector<std::string> RecStringArray(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  std::vector<std::string> vec_out;
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& element : *arr)
        if (const auto* str = std::get_if<std::string>(&element.val))
          vec_out.push_back(*str);
  return vec_out;
}

}

WithInfantryBodyInfoData WithInfantryBodyInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithInfantryBodyInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldInt(rec_info, "MinIdleDelay"))
    data.int4_min_idle_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxIdleDelay"))
    data.int4_max_idle_delay = static_cast<int>(*v);
  if (const auto s = sim::RecordFieldString(rec_info, "MoveSequence"))
    data.str_move_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec_info, "DefaultAttackSequence"))
    data.str_default_attack_sequence = std::string{*s};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "AttackSequences"))
    if (const auto* map_dict = std::get_if<meta::GenericDict>(&gv->val))
      for (const auto& [key, value] : *map_dict) {
        const auto* str_key = std::get_if<std::string>(&key.val);
        if (str_key == nullptr)
          continue;
        std::vector<std::string> vec_sequences;
        if (const auto* arr =
                std::get_if<std::vector<meta::GenericValue>>(&value.val))
          for (const auto& element : *arr)
            if (const auto* str = std::get_if<std::string>(&element.val))
              vec_sequences.push_back(*str);
        data.vec_attack_sequences.emplace_back(*str_key,
                                               std::move(vec_sequences));
      }
  data.vec_idle_sequences = RecStringArray(rec_info, "IdleSequences");
  if (const auto vec_stand = RecStringArray(rec_info, "StandSequences");
      !vec_stand.empty())
    data.vec_stand_sequences = vec_stand;
  if (const auto s = sim::RecordFieldString(rec_info, "Palette"))
    data.str_palette = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  return data;
}

WithInfantryBody::WithInfantryBody(const ActorInitializer& init,
                                   const WithInfantryBodyInfoData& info)
    : sim::ConditionalTraitCore<WithInfantryBody>(info.conditional),
      info_{info} {

  Actor& self = init.Self();
  auto* rs = self.Trait<RenderSprites>();

  up_default_animation_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), rs->GetImage(self),
      RenderSprites::MakeFacingFunc(self));
  awo_default_ = gfx::AnimationWithOffset{
      *up_default_animation_, {}, [this]() { return IsTraitDisabled(); }};
  rs->Add(*awo_default_, info.str_palette.empty()
                            ? std::string_view{}
                            : std::string_view{info.str_palette},
          info.b_is_player_palette);
  PlayStandAnimation(self);

  ptr_move_ = self.Trait<sim::IMove>();
}

void WithInfantryBody::Created(Actor& self) {

  ptr_rsm_ = self.TraitOrDefault<sim::IRenderInfantrySequenceModifier>();
  const WithInfantryBodyInfoData& info = GetDisplayInfo();
  int4_idle_delay_ =
      self.world().SharedRandom().Next(info.int4_min_idle_delay,
                                       info.int4_max_idle_delay);

  sim::ConditionalTraitCore<WithInfantryBody>::CoreCreated(self);
}

std::string WithInfantryBody::NormalizeInfantrySequence(
    Actor& self, std::string_view str_base_sequence) {

  const std::string str_prefix =
      IsModifyingSequence() ? std::string{ptr_rsm_->SequencePrefix()}
                            : std::string{};

  const std::string str_prefixed = str_prefix + std::string{str_base_sequence};
  if (up_default_animation_->HasSequence(str_prefixed))
    return str_prefixed;

  return std::string{str_base_sequence};
}

bool WithInfantryBody::AllowIdleAnimation(Actor& ) {

  return !GetDisplayInfo().vec_idle_sequences.empty() && !IsModifyingSequence();
}

void WithInfantryBody::PlayStandAnimation(Actor& self) {

  state_animation_ = AnimationState::Waiting;

  const std::string str_sequence = up_default_animation_->GetRandomExistingSequence(
      info_.vec_stand_sequences, self.world().LocalRandom());
  if (!str_sequence.empty()) {
    const std::string str_normalized =
        NormalizeInfantrySequence(self, str_sequence);
    up_default_animation_->PlayRepeating(str_normalized);
  }
}

void WithInfantryBody::AttackingInner(Actor& self, Armament& armament,
                                      const Barrel* ptr_barrel) {

  const WithInfantryBodyInfoData& info = GetDisplayInfo();
  std::string str_sequence = info.str_default_attack_sequence;

  const std::vector<std::string>* ptr_sequences = nullptr;
  for (const auto& [name, sequences] : info.vec_attack_sequences)
    if (name == armament.InfoData().str_name) {
      ptr_sequences = &sequences;
      break;
    }

  if (ptr_sequences != nullptr && !ptr_sequences->empty()) {
    str_sequence = (*ptr_sequences)[0];

    if (ptr_barrel != nullptr && ptr_sequences->size() > 1)
      for (std::size_t i = 0; i < ptr_sequences->size(); ++i)
        if (&armament.vec_barrels[i] == ptr_barrel)
          str_sequence = (*ptr_sequences)[i];
  }

  if (!str_sequence.empty() &&
      up_default_animation_->HasSequence(
          NormalizeInfantrySequence(self, str_sequence))) {
    state_animation_ = AnimationState::Attacking;
    up_default_animation_->PlayThen(
        NormalizeInfantrySequence(self, str_sequence),
        [this, &self]() { PlayStandAnimation(self); });
  }
}

void WithInfantryBody::PreparingAttack(Actor& self,
                                       const sim::Target& ,
                                       Armament& armament,
                                       const Barrel& barrel) {

  std::optional<std::size_t> opt_barrel_index;
  for (std::size_t i = 0; i < armament.vec_barrels.size(); ++i)
    if (&armament.vec_barrels[i] == &barrel) {
      opt_barrel_index = i;
      break;
    }

  self.world().AddFrameEndTask(
      [this, &self, &armament, opt_barrel_index](sim::World&) {
        AttackingInner(
            self, armament,
            opt_barrel_index.has_value()
                ? &armament.vec_barrels[*opt_barrel_index]
                : nullptr);
      });
}

void WithInfantryBody::Tick(Actor& self) {
  TickInner(self);
}

void WithInfantryBody::TickInner(Actor& self) {

  if (ptr_rsm_ != nullptr) {
    if (b_was_modifying_ != ptr_rsm_->IsModifyingSequence())
      b_dirty_ = true;

    b_was_modifying_ = ptr_rsm_->IsModifyingSequence();
  }

  const bool b_moving_horizontal =
      (static_cast<std::int32_t>(ptr_move_->CurrentMovementTypes()) &
       static_cast<std::int32_t>(sim::MovementType::Horizontal)) != 0;

  if ((state_animation_ != AnimationState::Moving || b_dirty_) &&
      b_moving_horizontal) {
    state_animation_ = AnimationState::Moving;
    up_default_animation_->PlayRepeating(NormalizeInfantrySequence(
        self, GetDisplayInfo().str_move_sequence));
  } else if (((state_animation_ == AnimationState::Moving || b_dirty_) &&
              !b_moving_horizontal) ||
             ((state_animation_ == AnimationState::Idle ||
               state_animation_ == AnimationState::IdleAnimating) &&
              !self.IsIdle()))
    PlayStandAnimation(self);

  b_dirty_ = false;
}

void WithInfantryBody::TickIdle(Actor& self) {

  if (!AllowIdleAnimation(self))
    return;

  if (state_animation_ == AnimationState::Waiting) {
    state_animation_ = AnimationState::Idle;
    const WithInfantryBodyInfoData& info = GetDisplayInfo();

    str_idle_sequence_ = info.vec_idle_sequences[
        static_cast<std::size_t>(self.world().SharedRandom().Next(
            static_cast<std::int32_t>(
                info.vec_idle_sequences.size())))];
    int4_idle_delay_ = self.world().SharedRandom().Next(
        info.int4_min_idle_delay, info.int4_max_idle_delay);
  } else if (state_animation_ == AnimationState::Idle &&
             int4_idle_delay_ > 0 && --int4_idle_delay_ == 0) {
    state_animation_ = AnimationState::IdleAnimating;
    up_default_animation_->PlayThen(str_idle_sequence_,
                                    [this, &self]() { PlayStandAnimation(self); });
  }
}

}
