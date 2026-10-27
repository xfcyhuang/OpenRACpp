#include "mods/external_condition.hpp"

#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

bool ExternalCondition::CanGrantCondition(const void* source) const {

  if (source == nullptr)
    return false;

  if (int4_source_cap_ > 0) {
    if (auto it = map_permanent_tokens_.find(source);
        it != map_permanent_tokens_.end() &&
        static_cast<int>(it->second.size()) >= int4_source_cap_)
      return false;
  }

  if (int4_total_cap_ > 0) {
    int total = 0;
    for (const auto& [key, tokens] : map_permanent_tokens_)
      total += static_cast<int>(tokens.size());
    if (total >= int4_total_cap_)
      return false;
  }

  return true;
}

int ExternalCondition::GrantCondition(sim::Actor& self, const void* source,
                                      int duration, int remaining) {

  if (!CanGrantCondition(source))
    return sim::Actor::InvalidConditionToken;

  int token = self.GrantCondition(str_condition_);
  std::vector<int>* permanent = nullptr;
  if (auto it = map_permanent_tokens_.find(source);
      it != map_permanent_tokens_.end())
    permanent = &it->second;

  if (remaining <= 0 || remaining > duration)
    remaining = duration;

  if (duration > 0) {

    if (int4_source_cap_ > 0) {
      int timed_count = 0;
      for (const TimedToken& t : vec_timed_tokens_)
        if (t.ptr_source == source)
          timed_count++;
      if ((permanent != nullptr ? static_cast<int>(permanent->size()) : 0) +
              timed_count >=
          int4_source_cap_) {
        int expire_index = -1;
        for (std::size_t i = 0; i < vec_timed_tokens_.size(); i++)
          if (vec_timed_tokens_[i].ptr_source == source) {
            expire_index = static_cast<int>(i);
            break;
          }
        if (expire_index >= 0) {
          const int expire_token = vec_timed_tokens_[expire_index].int4_token;
          vec_timed_tokens_.erase(
              vec_timed_tokens_.begin() + expire_index);
          if (self.TokenValid(expire_token))
            self.RevokeCondition(expire_token);
        }
      }
    }

    if (int4_total_cap_ > 0) {
      int total_count = 0;
      for (const auto& [key, tokens] : map_permanent_tokens_)
        total_count += static_cast<int>(tokens.size());
      total_count += static_cast<int>(vec_timed_tokens_.size());
      if (total_count >= int4_total_cap_ && !vec_timed_tokens_.empty()) {
        const int expire = vec_timed_tokens_.front().int4_token;
        if (self.TokenValid(expire))
          self.RevokeCondition(expire);
        vec_timed_tokens_.erase(vec_timed_tokens_.begin());
      }
    }

    TimedToken timed_token;
    timed_token.int4_expires = self.world().WorldTick() + remaining;
    timed_token.int4_token = token;
    timed_token.ptr_source = source;

    std::size_t index = vec_timed_tokens_.size();
    for (std::size_t i = 0; i < vec_timed_tokens_.size(); i++)
      if (vec_timed_tokens_[i].int4_expires >= timed_token.int4_expires) {
        index = i;
        break;
      }
    vec_timed_tokens_.insert(
        vec_timed_tokens_.begin() + static_cast<std::ptrdiff_t>(index),
        timed_token);
    if (index == vec_timed_tokens_.size() - 1) {

      int4_expires_ = timed_token.int4_expires;
      int4_duration_ = duration;
    }
  } else if (permanent == nullptr) {
    map_permanent_tokens_[source] = std::vector<int>{token};
  } else {
    permanent->push_back(token);
  }

  return token;
}

bool ExternalCondition::TryRevokeCondition(sim::Actor& self,
                                           const void* source, int token) {

  if (source == nullptr)
    return false;

  if (auto it = map_permanent_tokens_.find(source);
      it != map_permanent_tokens_.end()) {
    auto& tokens = it->second;
    auto tok = std::find(tokens.begin(), tokens.end(), token);
    if (tok == tokens.end())
      return false;
    tokens.erase(tok);
  } else {
    std::ptrdiff_t index = -1;
    for (std::size_t i = 0; i < vec_timed_tokens_.size(); i++)
      if (vec_timed_tokens_[i].int4_token == token) {
        index = static_cast<std::ptrdiff_t>(i);
        break;
      }
    if (index >= 0 && vec_timed_tokens_[index].ptr_source == source)
      vec_timed_tokens_.erase(vec_timed_tokens_.begin() + index);
    else
      return false;
  }

  if (self.TokenValid(token))
    self.RevokeCondition(token);

  return true;
}

void ExternalCondition::Tick(sim::Actor& self) {

  if (vec_timed_tokens_.empty())
    return;

  const int world_tick = self.world().WorldTick();
  std::size_t count = 0;
  while (count < vec_timed_tokens_.size() &&
         vec_timed_tokens_[count].int4_expires < world_tick) {
    const int token = vec_timed_tokens_[count].int4_token;
    if (self.TokenValid(token))
      self.RevokeCondition(token);
    count++;
  }

  if (count > 0) {
    vec_timed_tokens_.erase(
        vec_timed_tokens_.begin(),
        vec_timed_tokens_.begin() + static_cast<std::ptrdiff_t>(count));
    if (vec_timed_tokens_.empty()) {

      for (auto* w : vec_watchers_)
        w->Update(0, 0);
      return;
    }
  }

  if (!vec_timed_tokens_.empty()) {
    const int remaining = int4_expires_ - world_tick;
    for (auto* w : vec_watchers_)
      w->Update(int4_duration_, remaining);
  }
}

void ExternalCondition::Created(sim::Actor& self) {

  for (auto* w : self.TraitsImplementing<sim::IConditionTimerWatcher>())
    if (w->Condition() == str_condition_)
      vec_watchers_.push_back(w);
}

void ExternalCondition::OnOwnerChanged(sim::Actor& self,
                                       sim::Player& old_owner,
                                       sim::Player& new_owner) {

  for (auto& pair : self.world().ActorsWithTrait<
           sim::INotifyProximityOwnerChanged>())
    pair.trait->OnProximityOwnerChanged(self, &old_owner, &new_owner);
}

void GrantExternalConditionByName(sim::Actor& target, sim::Actor& source,
                                  const std::string& str_condition,
                                  int duration) {

  for (ExternalCondition* external :
       target.TraitsImplementing<ExternalCondition>())
    if (external->Condition() == str_condition &&
        external->CanGrantCondition(&source)) {
      external->GrantCondition(target, &source, duration);
      return;
    }
}

ProximityExternalCondition::ProximityExternalCondition(
    sim::Actor& self, std::string str_condition, WDist range,
    WDist maximum_vertical_offset,
    sim::PlayerRelationship valid_relationships, bool b_affects_parent,
    sim::ConditionalTraitData conditional)
    : sim::ConditionalTraitCore<ProximityExternalCondition>{conditional},
      str_condition_{std::move(str_condition)},
      range_{range},
      v_range_max_{maximum_vertical_offset},
      valid_relationships_{valid_relationships},
      b_affects_parent_{b_affects_parent} {
  ptr_self_ = &self;
}

ProximityExternalCondition::~ProximityExternalCondition() = default;

void ProximityExternalCondition::AddedToWorld(sim::Actor& self) {

  pos_cached_ = self.CenterPosition();
  int4_proximity_trigger_ =
      self.world().ActorMapFace()->AddProximityTrigger(
          pos_cached_, range_cached_, v_range_cached_,
          [this](sim::Actor& a) { ActorEntered(a); },
          [this](sim::Actor& a) { ActorExited(a); });
}

void ProximityExternalCondition::RemovedFromWorld(sim::Actor& self) {

  self.world().ActorMapFace()->RemoveProximityTrigger(int4_proximity_trigger_);
}

void ProximityExternalCondition::TraitEnabledHook(sim::Actor& self) {

  range_desired_ = range_;
  v_range_desired_ = v_range_max_;
}

void ProximityExternalCondition::TraitDisabledHook(sim::Actor&) {

  range_desired_ = WDist::Zero();
  v_range_desired_ = WDist::Zero();
}

void ProximityExternalCondition::Tick(sim::Actor& self) {

  if (self.CenterPosition() != pos_cached_ ||
      range_desired_ != range_cached_ || v_range_desired_ != v_range_cached_) {
    pos_cached_ = self.CenterPosition();
    range_cached_ = range_desired_;
    v_range_cached_ = v_range_desired_;
    self.world().ActorMapFace()->UpdateProximityTrigger(
        int4_proximity_trigger_, pos_cached_, range_cached_,
        v_range_cached_);
  }
}

void ProximityExternalCondition::ActorEntered(sim::Actor& a) {

  if (a.Disposed() || ptr_self_->Disposed())
    return;

  if (&a == ptr_self_ && !b_affects_parent_)
    return;

  if (map_tokens_.count(&a) != 0)
    return;

  const sim::PlayerRelationship relationship =
      ptr_self_->Owner()->RelationshipWith(a.Owner());
  if (!sim::HasRelationship(valid_relationships_, relationship))
    return;

  GrantInRange(a, *ptr_self_);
}

bool ProximityExternalCondition::GrantInRange(sim::Actor& target,
                                              sim::Actor& self) {
  for (ExternalCondition* external :
       target.TraitsImplementing<ExternalCondition>())
    if (external->Condition() == str_condition_ &&
        external->CanGrantCondition(&self)) {
      map_tokens_[&target] = external->GrantCondition(target, &self);
      return true;
    }
  return false;
}

void ProximityExternalCondition::UnitProducedByOther(
    sim::Actor& self, sim::Actor&, sim::Actor& produced,
    const std::string&, sim::TypeDictionary&) {

  if (produced.OccupiesSpace() == nullptr)
    return;

  if (sim::ConditionalTraitCore<
          ProximityExternalCondition>::IsTraitDisabled())
    return;

  const WVec delta = produced.CenterPosition() - self.CenterPosition();
  if (delta.HorizontalLengthSquared() <= range_.LengthSquared()) {
    if (!sim::HasRelationship(
            valid_relationships_,
            self.Owner()->RelationshipWith(produced.Owner())))
      return;
    GrantInRange(produced, self);
  }
}

void ProximityExternalCondition::ActorExited(sim::Actor& a) {

  if (a.Disposed())
    return;

  auto it = map_tokens_.find(&a);
  if (it == map_tokens_.end())
    return;

  const int token = it->second;
  map_tokens_.erase(it);
  for (ExternalCondition* external :
       a.TraitsImplementing<ExternalCondition>())
    if (external->TryRevokeCondition(a, ptr_self_, token))
      break;
}

void ProximityExternalCondition::OnProximityOwnerChanged(
    sim::Actor& actor, sim::Player*, sim::Player*) {

  if (actor.OccupiesSpace() == nullptr)
    return;

  if (sim::ConditionalTraitCore<
          ProximityExternalCondition>::IsTraitDisabled())
    return;

  const WVec delta = actor.CenterPosition() - ptr_self_->CenterPosition();
  if (delta.HorizontalLengthSquared() <= range_.LengthSquared()) {
    const bool has_relationship = sim::HasRelationship(
        valid_relationships_,
        ptr_self_->Owner()->RelationshipWith(actor.Owner()));
    const bool contains = map_tokens_.count(&actor) != 0;
    if (has_relationship && !contains) {
      GrantInRange(actor, *ptr_self_);
    } else if (!has_relationship && contains) {
      const int token = map_tokens_[&actor];
      map_tokens_.erase(&actor);
      for (ExternalCondition* external :
           actor.TraitsImplementing<ExternalCondition>())
        if (external->TryRevokeCondition(actor, ptr_self_, token))
          break;
    }
  }
}

}
