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

}

void PlayVoiceForOrders(
    std::span<const std::unique_ptr<net::Order>> vec_orders) {
  for (const auto& up_o : vec_orders) {
    if (up_o == nullptr)
      continue;

    if (up_o->vec_grouped_actors.has_value()) {
      for (sim::Actor* subject : *up_o->vec_grouped_actors)
        if (PlayVoiceForOrder(net::Order::FromGroupedOrder(*up_o, subject)))
          return;
    } else if (PlayVoiceForOrder(*up_o)) {
      return;
    }
  }
}

}
