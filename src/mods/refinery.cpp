// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Refinery.cs 实现部分
//          The implementation half.
#include "mods/refinery.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— RefineryInfoData(L21-33)————

RefineryInfoData RefineryInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RefineryInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "UseStorage") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_use_storage = *b;
      } else if (name == "DiscardExcessResources") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_discard_excess_resources = *b;
      } else if (name == "ShowTicks") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_show_ticks = *b;
      } else if (name == "TickRate") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_tick_rate = static_cast<int>(*n);
      }
    }
  }

  return data;
}

// ———— Refinery(L35-109)————

Refinery::Refinery(ActorInitializer& init, const RefineryInfoData& info)
    : info_{info} {
  // L43-48
  p_player_resources_ =
      init.Self().Owner()->PlayerActor()->Trait<PlayerResources>();
  int4_current_display_tick_ = info.int4_tick_rate;
}

void Refinery::Created(Actor& self) {
  // L50-53(修正 trait 的值链一次物化 —— 上游 Lazy+Select 的数组值)
  // L50-53 (the modifier traits' value chain materialized once —
  // upstream's Lazy + Select array values).
  for (sim::IResourceValueModifier* m :
       self.TraitsImplementing<sim::IResourceValueModifier>())
    vec_resource_value_modifiers_.push_back(m->GetResourceValueModifier());
}

int Refinery::AcceptResources(Actor& self,
                              const std::string& resource_type, int count) {
  // L55-91
  int resource_value = 0;
  bool b_found = false;
  for (const auto& [name, value] :
       p_player_resources_->Info().vec_resource_values)
    if (name == resource_type) {
      resource_value = value;
      b_found = true;
      break;
    }
  if (!b_found)
    return 0;

  int value =
      ApplyPercentageModifiers(count * resource_value,
                                      vec_resource_value_modifiers_);

  if (info_.b_use_storage) {
    const int storage_limit = std::max(
        p_player_resources_->ResourceCapacity -
            p_player_resources_->Resources,
        0);
    if (!info_.b_discard_excess_resources) {
      // Reduce amount if needed until it will fit the available storage
      // (上游注释)
      while (value > storage_limit) {
        --count;
        value = ApplyPercentageModifiers(
            count * resource_value, vec_resource_value_modifiers_);
      }
    } else {
      value = std::min(value, p_player_resources_->ResourceCapacity -
                                  p_player_resources_->Resources);
    }

    p_player_resources_->GiveResources(value);
  } else {
    value = p_player_resources_->ChangeCash(value);
  }

  for (auto& [actor, trait] :
       self.world().ActorsWithTrait<sim::INotifyResourceAccepted>()) {
    if (actor->Owner() != self.Owner())
      continue;

    trait->OnResourceAccepted(*actor, self, resource_type, count, value);
  }

  if (info_.b_show_ticks)
    int4_current_display_value_ += value;

  return count;
}

void Refinery::Tick(Actor& self) {
  // L93-103(FloatingText 显示面随 Phase 6;节拍状态照抄)
  // L93-103 (the FloatingText display face rides Phase 6; the cadence
  // state copied).
  if (info_.b_show_ticks && int4_current_display_value_ > 0 &&
      --int4_current_display_tick_ <= 0) {
    int4_current_display_tick_ = info_.int4_tick_rate;
    int4_current_display_value_ = 0;
  }
}

void Refinery::OnOwnerChanged(Actor& /*self*/, Player& /*old_owner*/,
                              Player& new_owner) {
  // L105-108
  p_player_resources_ =
      new_owner.PlayerActor()->Trait<PlayerResources>();
}

}  // namespace ora::mods
