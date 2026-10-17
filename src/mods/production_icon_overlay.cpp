// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithProductionIconOverlay.cs +
//          Traits/Render/ProductionIconOverlayManager.cs @e180e3225
//          (实现部分)
//          The implementation halves.
#include "mods/production_icon_overlay.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/production_support.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

WithProductionIconOverlayInfoData WithProductionIconOverlayInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithProductionIconOverlayInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Types"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_types.push_back(*s);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Prerequisites"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_prerequisites.push_back(*s);
  return data;
}

ProductionIconOverlayManagerInfoData ProductionIconOverlayManagerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProductionIconOverlayManagerInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "Type"))
    data.str_type = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Image"))
    data.str_image = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Sequence"))
    data.str_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Palette"))
    if (!v->empty())
      data.str_palette = std::string{*v};
  return data;
}

ProductionIconOverlayManager::ProductionIconOverlayManager(
    sim::ActorInitializer& init, ProductionIconOverlayManagerInfoData info)
    : info_{std::move(info)} {
  p_self_ = &init.Self();

  if (init.Self().world().Map().Sequences() != nullptr) {
    gfx::Animation anim(RenderSprites::MakeAnimationDeps(init.Self().world()),
                        info_.str_image);
    anim.Play(info_.str_sequence);
    sprite_ = anim.Image();
  }

  str_prefix_ = info_.str_type + ".";
  TechTree* ttc = p_self_->Trait<TechTree>();

  for (const auto& [name, actor] :
       init.Self().world().Map().Rules().Actors()) {
    for (const meta::RecordObject* rec :
         actor->TraitInfosByInterface("OpenRA.Traits.ITraitInfoInterface")) {
      if (rec->record_desc().str_name != "WithProductionIconOverlayInfo")
        continue;
      const WithProductionIconOverlayInfoData wpio =
          WithProductionIconOverlayInfoData::Parse(*rec);
      for (const std::string& str_type : wpio.vec_types)
        if (str_type == info_.str_type)
          ttc->Add(MakeKey(name), wpio.vec_prerequisites, 0, this);
    }
  }
}

void ProductionIconOverlayManager::PrerequisitesAvailable(
    const std::string& key) {
  const game::ActorInfo* ai =
      p_self_->world().Map().Rules().FindActor(key.substr(str_prefix_.size()));
  if (ai == nullptr)
    return;
  map_overlay_active_[ai] = true;
}

void ProductionIconOverlayManager::PrerequisitesUnavailable(
    const std::string& key) {
  const game::ActorInfo* ai =
      p_self_->world().Map().Rules().FindActor(key.substr(str_prefix_.size()));
  if (ai == nullptr)
    return;
  map_overlay_active_[ai] = false;
}

core::Vector2 ProductionIconOverlayManager::OverlayOffset(
    core::Vector2 vec_icon_size) const {
  const float x = (sprite_.vec_size.X - vec_icon_size.X) / 2;
  const float y = (sprite_.vec_size.Y - vec_icon_size.Y) / 2;
  return core::Vector2{x, y};
}

bool ProductionIconOverlayManager::IsOverlayActive(
    const game::ActorInfo& ai) const {
  const auto it = map_overlay_active_.find(&ai);
  if (it == map_overlay_active_.end())
    return false;

  return it->second;
}

std::string ProductionIconOverlayManager::MakeKey(
    std::string_view str_name) const {
  return str_prefix_ + std::string{str_name};
}

}  // namespace ora::mods
