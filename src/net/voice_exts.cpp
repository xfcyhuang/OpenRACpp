import std;

#include "net/voice_exts.hpp"

#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora {
namespace {

bool PlayVoiceForOrder(const net::Order& o) {
  sim::Actor* order_subject = o.p_subject;
  if (order_subject == nullptr || order_subject->Disposed())
    return false;

  for (auto* voice : order_subject->TraitsImplementing<sim::IVoiced>()) {
    for (auto* v : order_subject->TraitsImplementing<sim::IOrderVoice>()) {
      const std::string phrase = v->VoicePhraseForOrder(*order_subject, o);
      if (voice->PlayVoice(*order_subject, phrase,
                           order_subject->Owner()->Faction().InternalName))
        return true;
    }
  }

  return false;
}

}  // namespace

void PlayVoiceForOrders(std::span<net::Order* const> vec_orders) {
  for (net::Order* o : vec_orders) {
    if (o == nullptr)
      continue;

    if (o->vec_grouped_actors.has_value()) {
      for (sim::Actor* subject : *o->vec_grouped_actors)
        if (PlayVoiceForOrder(net::Order::FromGroupedOrder(*o, subject)))
          return;
    } else if (PlayVoiceForOrder(*o)) {
      return;
    }
  }
}

}  // namespace ora
