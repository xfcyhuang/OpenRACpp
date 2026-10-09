// UPSTREAM: OpenRA.Mods.Common/Traits/Render/ProductionBar.cs @b6fc03f
//          L18-105
#include "mods/production_bar.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

ProductionBarInfoData ProductionBarInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProductionBarInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = sim::RecordFieldString(rec_info, "ProductionType"))
    data.str_production_type = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Color"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.color_color = core::Color::FromArgbRaw(
          static_cast<std::uint32_t>(*n));
  return data;
}

ProductionBar::ProductionBar(const ActorInitializer& init,
                             ProductionBarInfoData info)
    : sim::ConditionalTraitCore<ProductionBar>(info.conditional),
      info_{std::move(info)} {
  ptr_self_ = &init.Self();
}

void ProductionBar::Created(Actor& self) {
  FindQueue(self);
}

void ProductionBar::FindQueue(Actor& self) {
  // 每个条必须绑到恰一个队列 —— 含禁用队列
  // Each bar must bind to exactly one queue — disabled ones included.
  ptr_queue_ = nullptr;
  for (ProductionQueue* queue :
       self.TraitsImplementing<ProductionQueue>())
    if (info_.str_production_type == queue->Info().str_type) {
      ptr_queue_ = queue;
      break;
    }

  if (ptr_queue_ == nullptr)
    for (ProductionQueue* queue :
         self.Owner()->PlayerActor()->TraitsImplementing<ProductionQueue>())
      if (info_.str_production_type == queue->Info().str_type) {
        ptr_queue_ = queue;
        break;
      }
}

void ProductionBar::Tick(Actor& self) {
  (void)self;
  if (IsTraitDisabled())
    return;

  const ProductionItemLike* item_current = nullptr;
  if (ptr_queue_ != nullptr)
    for (const auto& up_item : ptr_queue_->AllQueued())
      if (up_item->Started() &&
          (item_current == nullptr ||
           up_item->RemainingTime() < item_current->RemainingTime()))
        item_current = up_item.get();

  if (item_current == nullptr)
    float_value_ = 0;
  else if (item_current->TotalTime() <= 0)
    float_value_ = 1;
  else
    float_value_ = 1 - static_cast<float>(item_current->RemainingTime()) /
                           static_cast<float>(
                               item_current->TotalTime());
}

float ProductionBar::GetValue() {
  // 仅同盟可见;RenderPlayer 为 null 时按观战 = Ally → 可见(上游同)
  // Allies only; a null RenderPlayer counts as spectating = Ally →
  // visible (as upstream).
  if (IsTraitDisabled() ||
      !ptr_self_->Owner()->IsAlliedWith(ptr_self_->world().RenderPlayer()))
    return 0;

  return float_value_;
}

void ProductionBar::OnOwnerChanged(Actor& self, sim::Player& old_owner,
                                   sim::Player& new_owner) {
  (void)old_owner;
  (void)new_owner;
  FindQueue(self);
}

}  // namespace ora::mods
