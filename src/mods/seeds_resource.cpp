// UPSTREAM: OpenRA.Mods.Common/Traits/SeedsResource.cs 实现部分
//          The implementation half.
#include "mods/seeds_resource.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

SeedsResourceInfoData SeedsResourceInfoData::Parse(
    const meta::RecordObject& rec_info) {
  SeedsResourceInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "Interval"))
    data.int4_interval = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldString(rec_info, "ResourceType"))
    data.str_resource_type = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxRange"))
    data.int4_max_range = static_cast<int>(*v);
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  return data;
}

SeedsResource::SeedsResource(ActorInitializer& init,
                             SeedsResourceInfoData&& info)
    : sim::ConditionalTraitCore<SeedsResource>(info.conditional),
      info_{std::move(info)} {
  ptr_resource_layer_ =
      init.Self().world().WorldActor()->Trait<sim::IResourceLayer>();
}

void SeedsResource::Tick(Actor& self) {
  // L57-66:首 Tick 即播种(ticks 自 0 起)
  // L57-66: seeds on the first tick (ticks starts at 0).
  if (IsTraitDisabled())
    return;

  if (--int4_ticks <= 0) {
    Seed(self);
    int4_ticks = info_.int4_interval;
  }
}

void SeedsResource::Seed(Actor& self) {
  // L68-80:RandomWalk 的惰性枚举 —— Take(MaxRange) 截断,SkipWhile
  // 跳过「已是本资源且不可再加」的格,取首格;每步两抽,(0,0) 步抽而不
  // 产出(RNG 消耗序保真)
  // L68-80: the RandomWalk lazy enumeration — Take(MaxRange) truncates,
  // SkipWhile skips the cells that are already this resource and not
  // addable, the first cell wins; two draws per step, a (0,0) step draws
  // without yielding (the RNG consumption order preserved verbatim).
  if (ptr_resource_layer_ == nullptr)
    return;

  CPos p = self.Location();
  for (int i = 0; i < info_.int4_max_range; ++i) {
    int dx;
    int dy;
    do {
      dx = self.world().SharedRandom().Next(-1, 2);
      dy = self.world().SharedRandom().Next(-1, 2);
    } while (dx == 0 && dy == 0);

    p = p + CVec{dx, dy};

    const sim::ResourceLayerContents contents =
        ptr_resource_layer_->GetResource(p);
    if (contents.str_type == info_.str_resource_type &&
        !ptr_resource_layer_->CanAddResource(info_.str_resource_type, p))
      continue;

    if (ptr_resource_layer_->CanAddResource(info_.str_resource_type, p))
      ptr_resource_layer_->AddResource(info_.str_resource_type, p);
    return;
  }
}

void RegisterCommonTraitsBatch12Seeds() {
  auto& registry = sim::TraitRegistry::Instance();

  registry.Register(
      "SeedsResourceInfo",
      [](const meta::RecordObject& rec_info, ActorInitializer& init,
         ora::WorldArena& arena) -> TraitBase* {
        return arena.Create<SeedsResource>(
            init, SeedsResourceInfoData::Parse(rec_info));
      });
}

}  // namespace ora::mods
