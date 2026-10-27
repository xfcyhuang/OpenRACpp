#include "mods/warheads.hpp"

#include "core/percent_modifiers.hpp"
#include "core/int2.hpp"
#include "game/actor_info.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/armor.hpp"
#include "mods/hit_shape.hpp"
#include "mods/util.hpp"
#include "mods/smudge_layer.hpp"
#include "mods/world_exts.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

namespace {

std::optional<const meta::GenericValue*> RecordSlotOf(
    const meta::RecordObject& rec, std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name)
        return &generated->Slot(i);
  }
  return std::nullopt;
}

std::optional<std::int64_t> WarheadFieldInt(const meta::RecordObject& rec,
                                            std::string_view str_name) {
  const auto slot = RecordSlotOf(rec, str_name);
  if (slot.has_value())
    if (auto* n = std::get_if<std::int64_t>(&(*slot)->val))
      return *n;
  return std::nullopt;
}

}

void DamageWarhead::ParseDamageFields(const meta::RecordObject& rec_info) {
  if (const auto v = WarheadFieldInt(rec_info, "Damage"))
    Damage = static_cast<int>(*v);

  if (const auto slot = RecordSlotOf(rec_info, "DamageTypes"))
    if (auto* n = std::get_if<std::int64_t>(&(*slot)->val))
      bitset_damage_types = core::BitSet<sim::DamageType>::FromRawBits(
          static_cast<std::uint64_t>(*n));

  if (const auto slot = RecordSlotOf(rec_info, "Versus"))
    if (auto* dict = std::get_if<meta::GenericDict>(&(*slot)->val))
      for (const auto& [key, value] : *dict)
        if (auto* ks = std::get_if<std::string>(&key.val))
          if (auto* vn = std::get_if<std::int64_t>(&value.val))
            vec_versus.emplace_back(*ks, static_cast<int>(*vn));
}

bool DamageWarhead::IsValidAgainst(sim::Actor& victim,
                                   sim::Actor* fired_by) {

  if (!victim.Info()->HasTraitInfoOfInterface("OpenRA.Traits.IHealthInfo"))
    return false;

  return Warhead::IsValidAgainst(victim, fired_by);
}

void DamageWarhead::DoImpact(const sim::Target& target,
                             sim::WarheadArgs& args) {

  sim::Actor* fired_by = args.source_actor;

  if (target.Type() == sim::TargetType::Actor) {
    Actor& victim = *const_cast<Actor*>(target.ActorPtr);

    if (!IsValidAgainst(victim, fired_by))
      return;

    HitShape* closest_active_shape = nullptr;
    int closest_distance = std::numeric_limits<int>::max();
    for (sim::ITargetablePositions* t :
         victim.EnabledTargetablePositions())
      if (auto* h = dynamic_cast<HitShape*>(t)) {
        const int distance =
            h->DistanceFromEdge(victim, victim.CenterPosition()).Length;
        if (distance < closest_distance) {
          closest_distance = distance;
          closest_active_shape = h;
        }
      }

    if (closest_active_shape == nullptr)
      return;

    InflictDamage(victim, fired_by, *closest_active_shape, args);
  } else if (target.Type() != sim::TargetType::Invalid) {
    DoImpact(target.CenterPosition(), fired_by, args);
  }
}

int DamageWarhead::DamageVersus(sim::Actor& victim, HitShape& shape,
                                const sim::WarheadArgs& ) {

  if (vec_versus.empty())
    return 100;

  std::vector<int> vec_armor;
  for (Armor* a : victim.TraitsImplementing<Armor>()) {
    if (a->IsTraitDisabled())
      continue;
    const std::string& type = a->Info().str_type;
    if (type.empty())
      continue;
    const auto it_versus =
        std::find_if(vec_versus.begin(), vec_versus.end(),
                     [&](const auto& kv) { return kv.first == type; });
    if (it_versus == vec_versus.end())
      continue;

    if (!shape.Info().bitset_armor_types.IsEmpty() &&
        !shape.Info().bitset_armor_types.Contains(type))
      continue;
    vec_armor.push_back(it_versus->second);
  }

  return ApplyPercentageModifiers(100, vec_armor);
}

void DamageWarhead::InflictDamage(sim::Actor& victim, sim::Actor* fired_by,
                                  HitShape& shape,
                                  sim::WarheadArgs& args) {

  std::vector<std::int32_t> vec_modifiers = args.vec_damage_modifiers;
  vec_modifiers.push_back(DamageVersus(victim, shape, args));
  const int damage =
      ApplyPercentageModifiers(Damage, vec_modifiers);
  victim.Trait<sim::IHealth>()->InflictDamage(
      victim, fired_by, sim::Damage{damage, bitset_damage_types}, false);
}

std::unique_ptr<SpreadDamageWarhead> SpreadDamageWarhead::Parse(
    const meta::RecordObject& rec_info) {
  auto wh = std::make_unique<SpreadDamageWarhead>();

  if (const auto v = WarheadFieldInt(rec_info, "ValidTargets"))
    wh->valid_types = core::BitSet<sim::TargetableType>::FromRawBits(
        static_cast<std::uint64_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "InvalidTargets"))
    wh->invalid_types = core::BitSet<sim::TargetableType>::FromRawBits(
        static_cast<std::uint64_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "ValidRelationships"))
    wh->valid_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "AffectsParent"))
    wh->affects_parent = *v != 0;
  if (const auto v = WarheadFieldInt(rec_info, "AirThreshold"))
    wh->air_threshold = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = WarheadFieldInt(rec_info, "Delay"))
    wh->delay = static_cast<int>(*v);

  wh->ParseDamageFields(rec_info);

  if (const auto v = WarheadFieldInt(rec_info, "Spread"))
    wh->Spread = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = WarheadFieldInt(rec_info, "DamageCalculationType"))
    wh->damage_calculation_type =
        static_cast<DamageCalculationType>(
            static_cast<std::int32_t>(*v));

  if (const auto slot = RecordSlotOf(rec_info, "Falloff"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val)) {
      wh->vec_falloff.clear();
      for (const auto& element : *list)
        if (auto* n = std::get_if<std::int64_t>(&element.val))
          wh->vec_falloff.push_back(static_cast<int>(*n));
    }
  if (const auto slot = RecordSlotOf(rec_info, "Range"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val)) {
      wh->vec_range.clear();
      for (const auto& element : *list)
        if (auto* n = std::get_if<std::int64_t>(&element.val))
          wh->vec_range.push_back(
              WDist{static_cast<std::int32_t>(*n)});
    }

  if (!wh->vec_range.empty()) {
    if (wh->vec_range.size() != 1 &&
        wh->vec_range.size() != wh->vec_falloff.size())
      throw yaml::YamlException(
          "Number of range values must be 1 or equal to the number of "
          "Falloff values.");

    for (std::size_t i = 0; i + 1 < wh->vec_range.size(); i++)
      if (wh->vec_range[i] > wh->vec_range[i + 1])
        throw yaml::YamlException(
            "Range values must be specified in an increasing order.");

    wh->vec_effective_range = wh->vec_range;
  } else {
    wh->vec_effective_range.clear();
    for (std::size_t i = 0; i < wh->vec_falloff.size(); i++)
      wh->vec_effective_range.push_back(
          WDist{static_cast<std::int32_t>(i) * wh->Spread.Length});
  }

  return wh;
}

void SpreadDamageWarhead::DoImpact(const WPos& pos, sim::Actor* fired_by,
                                   sim::WarheadArgs& args) {

  sim::World& world = fired_by->world();
  const WDist& outermost = vec_effective_range.back();

  for (Actor* victim : FindActorsOnCircle(world, pos, outermost)) {
    if (!IsValidAgainst(*victim, fired_by))
      continue;

    HitShape* closest_active_shape = nullptr;
    int closest_distance = std::numeric_limits<int>::max();

    for (sim::ITargetablePositions* target_pos :
         victim->EnabledTargetablePositions()) {
      if (auto* h = dynamic_cast<HitShape*>(target_pos)) {
        const int distance = h->DistanceFromEdge(*victim, pos).Length;
        if (distance < closest_distance) {
          closest_distance = distance;
          closest_active_shape = h;
        }
      }
    }

    if (closest_active_shape == nullptr)
      continue;

    int falloff_distance = 0;
    switch (damage_calculation_type) {
      case DamageCalculationType::HitShape:
        falloff_distance = closest_distance;
        break;
      case DamageCalculationType::ClosestTargetablePosition: {
        int min_length = std::numeric_limits<int>::max();
        for (const WPos& x : victim->GetTargetablePositions())
          min_length = std::min(min_length, (x - pos).Length());
        falloff_distance = min_length;
        break;
      }
      case DamageCalculationType::CenterPosition:
        falloff_distance = (victim->CenterPosition() - pos).Length();
        break;
    }

    if (falloff_distance > vec_effective_range.back().Length)
      continue;

    std::vector<std::int32_t> vec_local_modifiers = args.vec_damage_modifiers;
    vec_local_modifiers.push_back(GetDamageFalloff(falloff_distance));
    WRot impact_orientation = args.impact_orientation;

    if (falloff_distance > 0) {
      const WAngle towards_target_yaw =
          (victim->CenterPosition() - args.impact_position).Yaw();
      const WAngle impact_angle =
          GetVerticalAngle(args.impact_position, victim->CenterPosition());
      impact_orientation =
          WRot{WAngle{0}, impact_angle, towards_target_yaw};
    }

    sim::WarheadArgs updated_warhead_args{args};
    updated_warhead_args.vec_damage_modifiers = std::move(vec_local_modifiers);
    updated_warhead_args.impact_orientation = impact_orientation;

    InflictDamage(*victim, fired_by, *closest_active_shape,
                  updated_warhead_args);
  }
}

int SpreadDamageWarhead::GetDamageFalloff(int distance) const {

  int inner = vec_effective_range[0].Length;
  for (std::size_t i = 1; i < vec_effective_range.size(); i++) {
    const int outer = vec_effective_range[i].Length;
    if (outer > distance)
      return int2::Lerp(vec_falloff[i - 1], vec_falloff[i],
                        distance - inner, outer - inner);

    inner = outer;
  }

  return 0;
}

namespace {

void ParseWarheadBaseFields(Warhead& wh, const meta::RecordObject& rec_info) {
  if (const auto v = WarheadFieldInt(rec_info, "ValidTargets"))
    wh.valid_types = core::BitSet<sim::TargetableType>::FromRawBits(
        static_cast<std::uint64_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "InvalidTargets"))
    wh.invalid_types = core::BitSet<sim::TargetableType>::FromRawBits(
        static_cast<std::uint64_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "ValidRelationships"))
    wh.valid_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  if (const auto v = WarheadFieldInt(rec_info, "AffectsParent"))
    wh.affects_parent = *v != 0;
  if (const auto v = WarheadFieldInt(rec_info, "AirThreshold"))
    wh.air_threshold = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = WarheadFieldInt(rec_info, "Delay"))
    wh.delay = static_cast<int>(*v);
}

}

std::unique_ptr<TargetDamageWarhead> TargetDamageWarhead::Parse(
    const meta::RecordObject& rec_info) {
  auto wh = std::make_unique<TargetDamageWarhead>();
  ParseWarheadBaseFields(*wh, rec_info);
  wh->ParseDamageFields(rec_info);
  if (const auto v = WarheadFieldInt(rec_info, "Spread"))
    wh->Spread = WDist{static_cast<std::int32_t>(*v)};
  return wh;
}

void TargetDamageWarhead::DoImpact(const WPos& pos, sim::Actor* fired_by,
                                   sim::WarheadArgs& args) {

  if (Spread == WDist{0})
    return;

  sim::World& world = fired_by->world();
  for (Actor* victim : FindActorsOnCircle(world, pos, Spread)) {
    if (!IsValidAgainst(*victim, fired_by))
      continue;

    HitShape* closest_active_shape = nullptr;
    int closest_distance = std::numeric_limits<int>::max();

    for (sim::ITargetablePositions* target_pos :
         victim->EnabledTargetablePositions()) {
      if (auto* h = dynamic_cast<HitShape*>(target_pos)) {
        const int distance = h->DistanceFromEdge(*victim, pos).Length;
        if (distance < closest_distance) {
          closest_distance = distance;
          closest_active_shape = h;
        }
      }
    }

    if (closest_active_shape == nullptr)
      continue;

    if (closest_distance > Spread.Length)
      continue;

    InflictDamage(*victim, fired_by, *closest_active_shape, args);
  }
}

namespace {

std::function<void(const std::string&, sim::World&, const WPos&)>
    fn_warhead_sound_play_ = nullptr;

std::string RandomOrDefaultOf(MersenneTwister& rng,
                              const std::vector<std::string>& vec_xs) {
  if (vec_xs.empty())
    return {};
  const std::int32_t index =
      rng.Next(0, static_cast<std::int32_t>(vec_xs.size()));
  return vec_xs[static_cast<std::size_t>(index)];
}

}

void SetWarheadSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play) {
  fn_warhead_sound_play_ = std::move(fn_play);
}

std::unique_ptr<CreateEffectWarhead> CreateEffectWarhead::Parse(
    const meta::RecordObject& rec_info) {
  auto wh = std::make_unique<CreateEffectWarhead>();
  ParseWarheadBaseFields(*wh, rec_info);

  if (const auto slot = RecordSlotOf(rec_info, "Explosions"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val))
      for (const auto& element : *list)
        if (auto* s = std::get_if<std::string>(&element.val))
          wh->vec_explosions.push_back(*s);
  if (const auto slot = RecordSlotOf(rec_info, "Image"))
    if (auto* s = std::get_if<std::string>(&(*slot)->val))
      wh->str_image = *s;
  if (const auto slot = RecordSlotOf(rec_info, "ExplosionPalette"))
    if (auto* s = std::get_if<std::string>(&(*slot)->val))
      wh->str_explosion_palette = *s;
  if (const auto v = WarheadFieldInt(rec_info, "UsePlayerPalette"))
    wh->b_use_player_palette = *v != 0;
  if (const auto v = WarheadFieldInt(rec_info, "ForceDisplayAtGroundLevel"))
    wh->b_force_display_at_ground_level = *v != 0;
  if (const auto slot = RecordSlotOf(rec_info, "ImpactSounds"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val))
      for (const auto& element : *list)
        if (auto* s = std::get_if<std::string>(&element.val))
          wh->vec_impact_sounds.push_back(*s);
  if (const auto v = WarheadFieldInt(rec_info, "ImpactSoundChance"))
    wh->int4_impact_sound_chance = static_cast<int>(*v);
  if (const auto v = WarheadFieldInt(rec_info, "ImpactActors"))
    wh->b_impact_actors = *v != 0;
  if (const auto v = WarheadFieldInt(rec_info, "Inaccuracy"))
    wh->dist_inaccuracy = WDist{static_cast<std::int32_t>(*v)};
  return wh;
}

CreateEffectWarhead::ImpactActorType CreateEffectWarhead::ActorTypeAtImpact(
    sim::World& world, const WPos& pos, sim::Actor* fired_by) {

  bool any_invalid_actor = false;

  for (Actor* victim : FindActorsOnCircle(world, pos, WDist{0})) {
    if (!affects_parent && victim == fired_by)
      continue;

    bool any_active_shape_within = false;
    for (sim::ITargetablePositions* t :
         victim->EnabledTargetablePositions())
      if (auto* h = dynamic_cast<HitShape*>(t);
          h != nullptr && !h->IsTraitDisabled() &&
          h->DistanceFromEdge(*victim, pos).Length <= 0) {
        any_active_shape_within = true;
        break;
      }
    if (!any_active_shape_within)
      continue;

    if (IsValidAgainst(*victim, fired_by))
      return ImpactActorType::Valid;

    any_invalid_actor = true;
  }

  return any_invalid_actor ? ImpactActorType::Invalid
                           : ImpactActorType::None;
}

bool CreateEffectWarhead::IsValidAgainst(sim::Actor& victim,
                                         sim::Actor* fired_by) {

  const sim::PlayerRelationship relationship =
      fired_by->Owner()->RelationshipWith(victim.Owner());
  if (!sim::HasRelationship(valid_relationships, relationship))
    return false;

  if (!IsValidTarget(victim.GetEnabledTargetTypes()))
    return false;

  return true;
}

void CreateEffectWarhead::DoImpact(const sim::Target& target,
                                   sim::WarheadArgs& args) {

  if (target.Type() == sim::TargetType::Invalid)
    return;

  sim::Actor* fired_by = args.source_actor;
  WPos pos = target.CenterPosition();
  sim::World& world = fired_by->world();
  const ImpactActorType actor_at_impact =
      b_impact_actors ? ActorTypeAtImpact(world, pos, fired_by)
                      : ImpactActorType::None;

  if (actor_at_impact == ImpactActorType::Invalid)
    return;

  if (actor_at_impact == ImpactActorType::None &&
      !IsValidAgainstTerrain(world, pos))
    return;

  const std::string explosion =
      RandomOrDefaultOf(world.LocalRandom(), vec_explosions);
  if (str_image.empty() == false && !explosion.empty()) {
    if (dist_inaccuracy.Length > 0)
      pos = pos + WVec::FromPDF(world.SharedRandom(), 2) *
                      dist_inaccuracy.Length / 1024;

    if (b_force_display_at_ground_level) {
      const WDist dat = world.Map().DistanceAboveTerrain(pos);
      pos = pos - WVec{0, 0, dat.Length};
    }

    std::string palette = str_explosion_palette;
    if (b_use_player_palette)
      palette += fired_by->Owner()->InternalName();

  }

  const std::string impact_sound =
      RandomOrDefaultOf(world.LocalRandom(), vec_impact_sounds);
  if (!impact_sound.empty() &&
      world.LocalRandom().Next(0, 100) < int4_impact_sound_chance) {
    if (fn_warhead_sound_play_ != nullptr)
      fn_warhead_sound_play_(impact_sound, world, pos);
  }
}

bool CreateEffectWarhead::IsValidAgainstTerrain(sim::World& world,
                                                const WPos& pos) {

  const CPos cell = world.Map().CellContaining(pos);
  if (!world.Map().Contains(cell))
    return false;

  const WDist dat = world.Map().DistanceAboveTerrain(pos);

  if (dat > air_threshold) {
    const std::vector<std::string> vec_air{"Air"};
    return IsValidTarget(core::BitSet<sim::TargetableType>::FromRawBits(
        meta::BitsOf("OpenRA.Traits.TargetableType", vec_air)));
  }
  return IsValidTarget(
      core::BitSet<sim::TargetableType>::FromRawBits(
          world.Map().GetTerrainInfo(cell).uint8_target_types));
}

std::unique_ptr<LeaveSmudgeWarhead> LeaveSmudgeWarhead::Parse(
    const meta::RecordObject& rec_info) {
  auto wh = std::make_unique<LeaveSmudgeWarhead>();
  ParseWarheadBaseFields(*wh, rec_info);

  if (const auto slot = RecordSlotOf(rec_info, "Size"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val)) {
      wh->vec_size.clear();
      for (const auto& element : *list)
        if (auto* n = std::get_if<std::int64_t>(&element.val))
          wh->vec_size.push_back(static_cast<int>(*n));
    }
  if (const auto slot = RecordSlotOf(rec_info, "SmudgeType"))
    if (auto* list =
            std::get_if<std::vector<meta::GenericValue>>(&(*slot)->val))
      for (const auto& element : *list)
        if (auto* s = std::get_if<std::string>(&element.val))
          wh->vec_smudge_type.push_back(*s);
  if (const auto v = WarheadFieldInt(rec_info, "Chance"))
    wh->int4_chance = static_cast<int>(*v);
  return wh;
}

void LeaveSmudgeWarhead::DoImpact(const sim::Target& target,
                                  sim::WarheadArgs& args) {

  if (target.Type() == sim::TargetType::Invalid)
    return;

  sim::Actor* fired_by = args.source_actor;
  sim::World& world = fired_by->world();

  if (int4_chance < world.LocalRandom().Next(100))
    return;

  WPos pos = target.CenterPosition();
  const WDist dat = world.Map().DistanceAboveTerrain(pos);

  if (dat > air_threshold)
    return;

  const CPos target_tile = world.Map().CellContaining(pos);

  std::vector<SmudgeLayer*> vec_layers;
  for (auto* layer : world.WorldActor()->TraitsImplementing<SmudgeLayer>())
    vec_layers.push_back(layer);

  const int min_range =
      (vec_size.size() > 1 && vec_size[1] > 0) ? vec_size[1] : 0;
  const std::vector<CPos> vec_all_cells =
      world.Map().FindTilesInAnnulus(target_tile, min_range, vec_size[0]);

  for (const CPos& sc : vec_all_cells) {

    std::string smudge_type;
    bool found = false;
    for (const std::string& accepts :
         world.Map().GetTerrainInfo(sc).vec_accepts_smudge_type)
      if (std::find(vec_smudge_type.begin(), vec_smudge_type.end(),
                    accepts) != vec_smudge_type.end()) {
        smudge_type = accepts;
        found = true;
        break;
      }
    if (!found)
      continue;

    bool any_invalid = false;
    for (Actor* a : world.ActorMapFace()->GetActorsAt(sc))
      if (!IsValidAgainst(*a, fired_by)) {
        any_invalid = true;
        break;
      }
    if (any_invalid)
      continue;

    SmudgeLayer* smudge_layer = nullptr;
    for (SmudgeLayer* layer : vec_layers)
      if (layer->Info().str_type == smudge_type) {
        smudge_layer = layer;
        break;
      }
    if (smudge_layer == nullptr)
      throw std::runtime_error(
          "Unknown smudge type `" + smudge_type +
          "`");

    smudge_layer->AddSmudge(sc);
  }
}

}
