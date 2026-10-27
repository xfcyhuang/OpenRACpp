#include "mods/player_resources.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor_init.hpp"
#include "net/session.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> RecInt(const meta::RecordObject& rec,
                                   std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

}

PlayerResourcesInfoData PlayerResourcesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PlayerResourcesInfoData data;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "SelectableCash") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          data.vec_selectable_cash.clear();
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              data.vec_selectable_cash.push_back(
                  static_cast<std::int32_t>(*n));
        }
      }
  }
  if (const auto v = RecInt(rec_info, "DefaultCash"))
    data.int4_default_cash = static_cast<int>(*v);
  if (const auto v =
          RecInt(rec_info, "InsufficientFundsNotificationInterval"))
    data.int4_insufficient_funds_notification_interval =
        static_cast<int>(*v);
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const meta::GenericValue& v = generated->Slot(i);
      if (fields[i]->str_name == "CashTickUpNotification") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_cash_tick_up_notification = *s;
      } else if (fields[i]->str_name == "CashTickDownNotification") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_cash_tick_down_notification = *s;
      }
    }
  }

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "ResourceValues") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* dict = std::get_if<meta::GenericDict>(&v.val))
          for (const auto& [key, value] : *dict)
            if (auto* ks = std::get_if<std::string>(&key.val))
              if (auto* vn = std::get_if<std::int64_t>(&value.val))
                data.vec_resource_values.emplace_back(
                    *ks, static_cast<int>(*vn));
      }
  }
  return data;
}

PlayerResources::PlayerResources(sim::ActorInitializer& init,
                                 PlayerResourcesInfoData info)
    : info_{std::move(info)} {

  p_owner_ = init.Self().Owner();

  const std::string starting_cash =
      init.Self().world().LobbyInfo().global_settings.OptionOrDefault(
          "startingcash", std::to_string(info_.int4_default_cash));

  int parsed = 0;
  const char* first = starting_cash.c_str();
  const char* last = first + starting_cash.size();
  const auto [ptr, ec] = std::from_chars(first, last, parsed);
  Cash = (ec == std::errc{} && ptr == last) ? parsed
                                            : info_.int4_default_cash;

  int8_last_notification_time =
      -info_.int4_insufficient_funds_notification_interval;
}

int PlayerResources::ChangeCash(int amount) {

  if (amount >= 0) {
    GiveCash(amount);
  } else {

    amount = std::max(-GetCashAndResources(), amount);

    TakeCash(-amount);
  }

  return amount;
}

bool PlayerResources::CanGiveResources(int amount) const {

  return Resources + amount <= ResourceCapacity;
}

void PlayerResources::GiveResources(int num, bool is_refund) {

  Resources += num;

  if (!is_refund)
    Earned += num;
  else
    Spent -= num;

  if (Resources > ResourceCapacity) {
    if (!is_refund)
      Earned -= Resources - ResourceCapacity;
    else
      Spent += Resources - ResourceCapacity;

    Resources = ResourceCapacity;
  }
}

bool PlayerResources::TakeResources(int num) {

  if (Resources < num)
    return false;
  Resources -= num;
  Spent += num;

  return true;
}

void PlayerResources::GiveCash(int num, bool is_refund) {

  if (Cash < std::numeric_limits<int>::max()) {
    int result = 0;
    if (__builtin_add_overflow(Cash, num, &result))
      Cash = std::numeric_limits<int>::max();
    else
      Cash = result;
  }

  if (!is_refund && Earned < std::numeric_limits<int>::max()) {
    int result = 0;
    if (__builtin_add_overflow(Earned, num, &result))
      Earned = std::numeric_limits<int>::max();
    else
      Earned = result;
  } else if (is_refund && Spent > std::numeric_limits<int>::min()) {
    int result = 0;
    if (__builtin_add_overflow(Spent, -num, &result))
      Spent = std::numeric_limits<int>::min();
    else
      Spent = result;
  }
}

bool PlayerResources::TakeCash(int num, bool notify_low_funds) {

  if (GetCashAndResources() < num) {
    (void)notify_low_funds;
    return false;
  }

  Resources -= num;
  Spent += num;
  if (Resources < 0) {
    Cash += Resources;
    Resources = 0;
  }

  return true;
}

void PlayerResources::AddStorageCapacity(int capacity) {

  ResourceCapacity += capacity;
}

void PlayerResources::RemoveStorageCapacity(int capacity) {

  ResourceCapacity -= capacity;

  if (Resources > ResourceCapacity)
    Resources = ResourceCapacity;
}

}
