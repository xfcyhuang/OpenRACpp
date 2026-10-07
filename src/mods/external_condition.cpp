// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ExternalCondition.cs @b6fc03f
//          L14-190 + Traits/Conditions/ProximityExternalCondition.cs
//          L18-194(逐语句重写;机制对照见 external_condition.hpp 头注)
//          Statement-by-statement; the mechanism mapping lives in
//          external_condition.hpp's header note.
#include "mods/external_condition.hpp"


#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— ExternalCondition ————

bool ExternalCondition::CanGrantCondition(const void* source) const {
  // L57-76
  if (source == nullptr)
    return false;

  // Timed 不占 source 上限:剩余最短者可随时撤销腾位(上游注释)
  // Timed ones do not occupy the source cap: the shortest remaining can
  // always be revoked to make room (upstream comment).
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
  // L78-138
  if (!CanGrantCondition(source))
    return sim::Actor::InvalidConditionToken;

  int token = self.GrantCondition(str_condition_);
  std::vector<int>* permanent = nullptr;
  if (auto it = map_permanent_tokens_.find(source);
      it != map_permanent_tokens_.end())
    permanent = &it->second;

  // 调用方可在 1..duration 内覆盖剩余时长
  // Callers can override the remaining time between 1 and duration.
  if (remaining <= 0 || remaining > duration)
    remaining = duration;

  if (duration > 0) {
    // 上限检查(source 上限淘汰同源最近到期者)
    // The caps (the source cap evicts the same-source nearest expiry).
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

    // 升序 Expires 插入(FindIndex(t.Expires >= new.Expires))
    // The ascending-Expires insert (FindIndex(t.Expires >= new.Expires)).
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
      // 追加尾 = 最长剩余计时(上游跟踪)
      // Appended at the tail = the longest remaining timer (upstream
      // tracks it).
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
  // L140-160
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
  // L162-186
  if (vec_timed_tokens_.empty())
    return;

  // 到期撤销(升序头段)
  // The expiry revocations (the ascending head run).
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
      // 全部到期:通知观察者 0/0
      // All expired: notify the watchers of 0/0.
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
  // L188-190:watchers = 同 actor 的同条件 IConditionTimerWatcher
  // L188-190: watchers = the same actor's IConditionTimerWatcher set for
  // this condition.
  for (auto* w : self.TraitsImplementing<sim::IConditionTimerWatcher>())
    if (w->Condition() == str_condition_)
      vec_watchers_.push_back(w);
}

void ExternalCondition::OnOwnerChanged(sim::Actor& self,
                                       sim::Player& old_owner,
                                       sim::Player& new_owner) {
  // L105-108:域内换主的广播(ProximityExternalCondition 的工作面)
  // L105-108: the in-range owner-change broadcast (the working face of
  // ProximityExternalCondition).
  for (auto& pair : self.world().ActorsWithTrait<
           sim::INotifyProximityOwnerChanged>())
    pair.trait->OnProximityOwnerChanged(self, &old_owner, &new_owner);
}

// ———— GrantExternalConditionByName ————

void GrantExternalConditionByName(sim::Actor& target, sim::Actor& source,
                                  const std::string& str_condition,
                                  int duration) {
  // TraitsImplementing<ExternalCondition>().FirstOrDefault(Info.Condition
  // == Condition && CanGrant) → GrantCondition(上游 FirstOrDefault 语义 =
  // 首个匹配)
  // TraitsImplementing<ExternalCondition>().FirstOrDefault(
  // Info.Condition == Condition && CanGrant) → GrantCondition
  // (upstream's FirstOrDefault = the first match).
  for (ExternalCondition* external :
       target.TraitsImplementing<ExternalCondition>())
    if (external->Condition() == str_condition &&
        external->CanGrantCondition(&source)) {
      external->GrantCondition(target, &source, duration);
      return;
    }
}

// ———— ProximityExternalCondition ————

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
  // L57-61
  pos_cached_ = self.CenterPosition();
  int4_proximity_trigger_ =
      self.world().ActorMapFace()->AddProximityTrigger(
          pos_cached_, range_cached_, v_range_cached_,
          [this](sim::Actor& a) { ActorEntered(a); },
          [this](sim::Actor& a) { ActorExited(a); });
}

void ProximityExternalCondition::RemovedFromWorld(sim::Actor& self) {
  // L63-66
  self.world().ActorMapFace()->RemoveProximityTrigger(int4_proximity_trigger_);
}

void ProximityExternalCondition::TraitEnabledHook(sim::Actor& self) {
  // L68-73(EnableSound 省略)
  range_desired_ = range_;
  v_range_desired_ = v_range_max_;
}

void ProximityExternalCondition::TraitDisabledHook(sim::Actor&) {
  // L75-80(DisableSound 省略)
  range_desired_ = WDist::Zero();
  v_range_desired_ = WDist::Zero();
}

void ProximityExternalCondition::Tick(sim::Actor& self) {
  // L85-96
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
  // L98-114
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
  // L117-135
  // 不占空间的出厂 actor 不会进域
  // A produced actor that occupies no space cannot be in range.
  if (produced.OccupiesSpace() == nullptr)
    return;

  if (sim::ConditionalTraitCore<
          ProximityExternalCondition>::IsTraitDisabled())
    return;

  // 域内出厂第二 tick 才触发的 workaround(上游注释)
  // The workaround for actors produced within the region not triggering
  // until the second tick (upstream comment).
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
  // L137-154
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
  // L158-194
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

}  // namespace ora::mods
