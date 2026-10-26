// UPSTREAM: OpenRA.Mods.Common/Traits/CustomSellValue.cs @1f594e320
//          L18-38 全文 + OpenRA.Mods.Common/Traits/Sellable.cs @1f594e320
//          L21-122 全文 + OpenRA.Mods.Common/Activities/Sell.cs @1f594e320
//          L19-58 全文(实现部分)。
#include "mods/sellable.hpp"

#include "game/actor_info.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "mods/order_generators.hpp"
#include "mods/with_make_animation.hpp"
#include "net/text_notifications.hpp"
#include "game/actor_info.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::string_view> FieldString(const meta::RecordObject& rec,
                                            std::string_view str_name) {
  return sim::RecordFieldString(rec, str_name);
}

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec, str_name);
}

std::optional<bool> FieldBool(const meta::RecordObject& rec,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

const meta::RecordObject* FindInfoByName(const sim::Actor& self,
                                         std::string_view str_name) {
  for (const meta::RecordObject* rec : self.Info()->TraitsInConstructOrder())
    if (rec->record_desc().str_name == str_name)
      return rec;
  return nullptr;
}

}  // namespace

CustomSellValueInfoData CustomSellValueInfoData::Parse(
    const meta::RecordObject& rec_info) {
  CustomSellValueInfoData data;
  if (const auto v = FieldInt(rec_info, "Value"))
    data.int4_value = static_cast<int>(*v);
  return data;
}

CustomSellValue::CustomSellValue(CustomSellValueInfoData info)
    : info_{std::move(info)} {}

int GetSellValue(sim::Actor& self) {
  if (const meta::RecordObject* rec_csv = FindInfoByName(self, "CustomSellValueInfo"))
    if (const auto v = FieldInt(*rec_csv, "Value"))
      return static_cast<int>(*v);

  if (const meta::RecordObject* rec_valued = FindInfoByName(self, "ValuedInfo"))
    if (const auto v = FieldInt(*rec_valued, "Cost"))
      return static_cast<int>(*v);

  return 0;
}

SellableInfoData SellableInfoData::Parse(const meta::RecordObject& rec_info) {
  SellableInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "RefundPercent"))
    data.int4_refund_percent = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec_info, "SellSounds"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_sell_sounds.push_back(*s);
  if (const auto v = FieldString(rec_info, "Notification"))
    data.str_notification = std::string{*v};
  if (const auto v = FieldString(rec_info, "TextNotification"))
    data.str_text_notification = std::string{*v};
  if (const auto v = FieldBool(rec_info, "ShowTicks"))
    data.b_show_ticks = *v;
  if (const auto v = FieldBool(rec_info, "ShowTooltipText"))
    data.b_show_tooltip_text = *v;
  if (const auto v = FieldBool(rec_info, "SkipMakeAnimation"))
    data.b_skip_make_animation = *v;
  if (const auto v = FieldString(rec_info, "Cursor"))
    data.str_cursor = std::string{*v};
  return data;
}

Sellable::Sellable(Actor& self, SellableInfoData info)
    : ConditionalTraitCore{info.conditional},
      ptr_self_{&self},
      info_{std::move(info)} {}

void Sellable::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string == "Sell")
    Sell(self);
}

void Sellable::Sell(Actor& self) {
  if (IsTraitDisabled())
    return;

  self.CancelActivity();

  for (auto* ns : self.TraitsImplementing<sim::INotifySold>())
    ns->Selling(self);

  if (!info_.b_skip_make_animation) {
    if (auto* make_animation = self.TraitOrDefault<WithMakeAnimation>()) {
      make_animation->Reverse(
          self, self.world().Arena().Create<SellActivity>(self, info_.b_show_ticks), false);
      return;
    }
  }

  self.QueueActivity(false, self.world().Arena().Create<SellActivity>(self, info_.b_show_ticks));
}

bool Sellable::IsTooltipVisible(sim::Player* ptr_for_player) {
  if (info_.b_show_tooltip_text && !IsTraitDisabled() &&
      dynamic_cast<class SellOrderGenerator*>(ptr_self_->world().OrderGenerator()) != nullptr)
    return ptr_for_player == ptr_self_->Owner();
  return false;
}

std::string_view Sellable::TooltipText() const {
  const int int4_sell_value = GetSellValue(*ptr_self_);

  sim::IHealth* ptr_health = ptr_self_->TraitOrDefault<sim::IHealth>();
  const std::int64_t int8_hp = ptr_health != nullptr ? ptr_health->HP() : 1;
  const std::int64_t int8_max_hp = ptr_health != nullptr ? ptr_health->MaxHP() : 1;
  const int int4_refund = static_cast<int>(static_cast<std::int64_t>(int4_sell_value) *
                                           info_.int4_refund_percent * int8_hp /
                                           (100 * int8_max_hp));

  str_tooltip_cache_ = "Refund: $" + std::to_string(int4_refund);
  return str_tooltip_cache_;
}

SellActivity::SellActivity(Actor& self, bool b_show_ticks)
    : b_show_ticks_{b_show_ticks} {
  ptr_health_ = self.TraitOrDefault<sim::IHealth>();
  if (const meta::RecordObject* rec = FindInfoByName(self, "SellableInfo"))
    sellable_info_ = SellableInfoData::Parse(*rec);
  ptr_player_resources_ =
      self.Owner()->PlayerActor()->Trait<PlayerResources>();
  b_is_interruptible_ = false;
}

bool SellActivity::Tick(Actor& self) {
  const int int4_sell_value = GetSellValue(self);

  const std::int64_t int8_hp = ptr_health_ != nullptr ? ptr_health_->HP() : 1;
  const std::int64_t int8_max_hp = ptr_health_ != nullptr ? ptr_health_->MaxHP() : 1;
  int int4_refund = static_cast<int>(static_cast<std::int64_t>(int4_sell_value) *
                                     sellable_info_.int4_refund_percent * int8_hp /
                                     (100 * int8_max_hp));
  int4_refund = ptr_player_resources_->ChangeCash(int4_refund);

  for (auto* ns : self.TraitsImplementing<sim::INotifySold>())
    ns->Sold(self);

  net::TextNotificationsManager::Instance().AddTransientLine(
      self.Owner(), sellable_info_.str_text_notification);

  self.Dispose();
  return false;
}

}  // namespace ora::mods
