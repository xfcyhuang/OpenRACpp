#include "sim/actor.hpp"

#include "gfx/renderable.hpp"
#include "sim/actor_init.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

struct ObserverEntry {
  VariableObserverNotifier notifier;
  std::vector<std::string> variables;
};

}

Actor::Actor(World& world, std::string str_name, TypeDictionary& init_dict)
    : world_(world), str_info_name_(std::move(str_name)) {

  {
    std::map<gen::TypeId, int> counts;
    gen::TypeId duplicate = gen::TypeId::OpenRA_Traits_ITraitInfoInterface;
    bool has_duplicate = false;
    for (auto* i : init_dict.WithInterface<ISingleInstanceInit>()) {
      auto* init = dynamic_cast<ActorInit*>(i);
      if (init == nullptr)
        continue;
      auto id = init->GetInitTypeId();
      if (++counts[id] > 1) {
        duplicate = id;
        has_duplicate = true;
        break;
      }
    }

    if (has_duplicate) {
      const auto v = static_cast<std::size_t>(duplicate);
      throw std::runtime_error(
          "Duplicate initializer '" +
          std::string(v < gen::kTypeIdCount ? gen::kTypeFullNames[v]
                                            : std::string_view("(unknown)")) +
          "'");
    }
  }

  ActorInitializer init(*this, init_dict);

  uint4_actor_id_ = world_.NextAID();
  auto* owner_init = init.GetOrDefault<OwnerInit>();
  if (owner_init != nullptr)
    p_owner_ = owner_init->Value(world_);

  if (!str_info_name_.empty())
    world_.CreateTraitsForActor(*this, init, str_info_name_);
}

Actor::~Actor() = default;

std::string Actor::DebugName() const {

  std::string name = str_info_name_ + " " + std::to_string(uint4_actor_id_);
  if (!b_is_in_world_)
    name += " (not in world)";
  return name;
}

CPos Actor::Location() const {

  return vec_occupy_space_.empty() ? CPos{}
                                   : vec_occupy_space_.back()->TopLeft();
}

WPos Actor::CenterPosition() const {
  return vec_occupy_space_.empty() ? WPos{}
                                   : vec_occupy_space_.back()->CenterPosition();
}

bool Actor::IsDead() const {
  return b_disposed_;
}

void Actor::Tick() {

  const bool was_idle = IsIdle();
  SetCurrentActivity(RunActivity(*this, CurrentActivity()));

  if (!was_idle && IsIdle()) {

    for (auto* n : world_.TraitDict().WithInterface<INotifyBecomingIdle>(this))
      n->OnBecomingIdle(*this);

    SetCurrentActivity(RunActivity(*this, CurrentActivity()));
  } else if (was_idle) {
    for (auto* t : world_.TraitDict().WithInterface<INotifyIdle>(this))
      t->TickIdle(*this);
  }
}

void Actor::QueueActivity(bool queued, Activity* next_activity) {

  if (!queued)
    CancelActivity();

  QueueActivity(next_activity);
}

void Actor::QueueActivity(Activity* next_activity) {

  if (!b_created_)
    throw std::runtime_error(
        "An activity was queued before the actor was created. Queue it inside "
        "the INotifyCreated.Created callback instead.");

  if (CurrentActivity() == nullptr)
    SetCurrentActivity(next_activity);
  else
    CurrentActivity()->Queue(next_activity);
}

void Actor::CancelActivity() {

  if (auto* a = CurrentActivity())
    a->Cancel(*this);
}

void Actor::AddTrait(TraitBase* trait) {

  world_.TraitDict().AddTrait(this, trait);

  for (const auto& entry : trait->TraitUpcasts()) {
    if (entry.type_id == IOccupySpace::kTypeId)
      vec_occupy_space_.push_back(
          static_cast<IOccupySpace*>(entry.upcast(trait)));
    else if (entry.type_id == IFacing::kTypeId)
      p_facing_ = static_cast<IFacing*>(entry.upcast(trait));
    else if (entry.type_id == ITargetable::kTypeId)
      vec_targetables_.push_back(
          static_cast<ITargetable*>(entry.upcast(trait)));
    else if (entry.type_id == ICrushable::kTypeId)
      vec_crushables_.push_back(
          static_cast<ICrushable*>(entry.upcast(trait)));
    else if (entry.type_id == IVisibilityModifier::kTypeId)
      vec_visibility_modifiers_.push_back(
          static_cast<IVisibilityModifier*>(entry.upcast(trait)));
    else if (entry.type_id == IDefaultVisibility::kTypeId)
      p_default_visibility_ =
          static_cast<IDefaultVisibility*>(entry.upcast(trait));
    else if (entry.type_id == ITargetablePositions::kTypeId) {

      vec_all_targetable_positions_.push_back(
          static_cast<ITargetablePositions*>(entry.upcast(trait)));
    } else if (entry.type_id == IEffectiveOwner::kTypeId)
      p_effective_owner_ =
          static_cast<IEffectiveOwner*>(entry.upcast(trait));
    else if (entry.type_id == IRender::kTypeId)
      vec_renders_.push_back(static_cast<IRender*>(entry.upcast(trait)));
    else if (entry.type_id == IMouseBounds::kTypeId)
      vec_mouse_bounds_.push_back(
          static_cast<IMouseBounds*>(entry.upcast(trait)));
  }
}

void Actor::Render(gfx::WorldRenderer& wr,
                   std::vector<gfx::RenderItem>& vec_out) {

  if (ptr_last_world_renderer_ != &wr) {
    ptr_last_world_renderer_ = &wr;
    vec_cached_renderables_.clear();
    for (auto* render : vec_renders_)
      render->Render(*this, wr, vec_cached_renderables_);
  }

  vec_out.insert(vec_out.end(), vec_cached_renderables_.begin(),
                 vec_cached_renderables_.end());
}

std::vector<Rectangle> Actor::ScreenBounds(gfx::WorldRenderer& wr) {

  std::vector<Rectangle> vec_bounds;
  for (auto* render : vec_renders_)
    for (const auto& rect : render->ScreenBounds(*this, wr))
      if (!rect.IsEmpty())
        vec_bounds.push_back(rect);
  return vec_bounds;
}

Polygon Actor::MouseBounds(gfx::WorldRenderer& wr) {

  for (auto* mb : vec_mouse_bounds_) {
    Polygon bounds = mb->MouseoverBounds(*this, &wr);
    if (!bounds.IsEmpty())
      return bounds;
  }

  return Polygon::Empty();
}

std::vector<WPos> Actor::GetTargetablePositions() const {

  if (!vec_enabled_targetable_positions_.empty())
    return vec_enabled_targetable_world_positions_;

  return std::vector<WPos>{CenterPosition()};
}

core::BitSet<TargetableType> Actor::GetAllTargetTypes() const {

  std::uint64_t uint8_bits = 0;
  for (const auto* targetable : vec_targetables_)
    uint8_bits |= targetable->TargetTypes().RawBits();
  return core::BitSet<TargetableType>::FromRawBits(uint8_bits);
}

core::BitSet<TargetableType> Actor::GetEnabledTargetTypes() const {

  std::uint64_t uint8_bits = 0;
  for (auto* targetable : vec_targetables_) {
    auto* base = dynamic_cast<TraitBase*>(targetable);
    if (base == nullptr || base->IsTraitEnabled())
      uint8_bits |= targetable->TargetTypes().RawBits();
  }
  return core::BitSet<TargetableType>::FromRawBits(uint8_bits);
}

bool Actor::IsTargetableBy(Actor& by_actor) const {

  for (auto* targetable : vec_targetables_)
    if (targetable->TargetableBy(const_cast<Actor&>(*this), by_actor))
      return true;

  return false;
}

void Actor::Initialize(bool add_to_world) {

  b_created_ = true;

  vec_enabled_targetable_positions_.clear();
  vec_enabled_targetable_world_positions_.clear();
  for (ITargetablePositions* positions : vec_all_targetable_positions_) {
    auto* base = dynamic_cast<TraitBase*>(positions);
    if (base == nullptr || base->IsTraitEnabled()) {
      vec_enabled_targetable_positions_.push_back(positions);
      for (const WPos& p : positions->TargetablePositions(*this))
        vec_enabled_targetable_world_positions_.push_back(p);
    }
  }

  for (auto* t : TraitsImplementing<INotifyCreated>())
    t->Created(*this);

  std::vector<ObserverEntry> all_entries;
  for (auto* provider : TraitsImplementing<IObservesVariables>()) {
    for (auto& variable_user : provider->GetVariableObservers()) {
      ObserverEntry entry{variable_user.fn_notifier,
                          variable_user.vec_variables};
      all_entries.push_back(std::move(entry));

      for (const auto& variable : variable_user.vec_variables) {
        auto& cs = map_condition_states_[variable];
        cs.vec_notifiers.push_back(variable_user.fn_notifier);

        map_condition_cache_[variable] =
            static_cast<int>(cs.set_tokens.size());
      }
    }
  }

  for (auto& entry : all_entries)
    entry.notifier(*this, ConditionCache());

  ICreationActivity* creation_activity_provider = nullptr;
  for (auto* ica : TraitsImplementing<ICreationActivity>()) {

    auto* ica_base = dynamic_cast<TraitBase*>(ica);
    if (ica_base != nullptr && !ica_base->IsTraitEnabled())
      continue;

    if (creation_activity_provider != nullptr)
      throw std::runtime_error(
          "More than one enabled ICreationActivity trait");

    auto* activity = ica->GetCreationActivity();
    if (activity == nullptr)
      continue;

    creation_activity_provider = ica;

    activity->Queue(CurrentActivity());
    SetCurrentActivity(activity);
  }

  if (add_to_world)
    world_.Add(this);
}

void Actor::Kill(Actor* attacker,
                 const core::BitSet<DamageType>& damage_types) {
  if (Disposed())
    return;

  if (auto* health = TraitOrDefault<IHealth>())
    health->Kill(*this, attacker, damage_types);
}

void Actor::Dispose() {

  if (auto* a = CurrentActivity())
    a->OnActorDisposeOuter(*this);

  b_will_dispose_ = true;

  world_.AddFrameEndTask([this](World& w) {
    if (b_disposed_)
      return;

    if (b_is_in_world_)
      w.Remove(this);

    for (auto* t : TraitsImplementing<INotifyActorDisposing>())
      t->Disposing(*this);

    w.TraitDict().RemoveActor(this);
    b_disposed_ = true;
  });
}

void Actor::ResolveOrder(const net::Order& order) {

  for (auto* r : TraitsImplementing<IResolveOrder>())
    r->ResolveOrder(*this, order);
}

void Actor::ChangeOwner(Player* new_owner) {

  world_.AddFrameEndTask([this, new_owner](World&) {
    if (!b_disposed_)
      ChangeOwnerSync(new_owner);
  });
}

void Actor::ChangeOwnerSync(Player* new_owner) {

  if (b_disposed_)
    return;

  Player* old_owner = p_owner_;
  const bool was_in_world = b_is_in_world_;

  if (was_in_world)
    world_.Remove(this);

  p_owner_ = new_owner;
  ++int4_generation_;

  if (old_owner && new_owner)
    for (auto* t : TraitsImplementing<INotifyOwnerChanged>())
      t->OnOwnerChanged(*this, *old_owner, *new_owner);

  if (world_.WorldActor() && old_owner && new_owner)
    for (auto* t : world_.WorldActor()->TraitsImplementing<INotifyOwnerChanged>())
      t->OnOwnerChanged(*this, *old_owner, *new_owner);

  if (was_in_world)
    world_.Add(this);
}

void Actor::UpdateConditionState(std::string_view condition, int token,
                                 bool is_revoke) {
  auto& condition_state = map_condition_states_[std::string(condition)];

  if (is_revoke)
    condition_state.set_tokens.erase(token);
  else
    condition_state.set_tokens.insert(token);

  map_condition_cache_[std::string(condition)] =
      static_cast<int>(condition_state.set_tokens.size());

  if (b_created_)
    for (auto& notify : condition_state.vec_notifiers)
      notify(*this, ConditionCache());
}

int Actor::GrantCondition(std::string_view condition) {

  if (condition.empty())
    return InvalidConditionToken;

  auto token = next_condition_token_++;
  map_condition_tokens_[token] = std::string(condition);
  UpdateConditionState(condition, token, false);
  return token;
}

int Actor::RevokeCondition(int token) {

  auto it = map_condition_tokens_.find(token);
  if (it == map_condition_tokens_.end())
    throw std::runtime_error("Attempting to revoke condition with invalid token " +
                             std::to_string(token) + " for " + DebugName() +
                             ".");

  std::string condition = it->second;
  map_condition_tokens_.erase(it);
  UpdateConditionState(condition, token, true);
  return InvalidConditionToken;
}

bool Actor::TokenValid(int token) const {

  return map_condition_tokens_.count(token) != 0;
}

}

namespace ora::sim::sync {

int HashActor(const Actor* a) {
  if (a != nullptr)
    return static_cast<int>(a->ActorID() << 16);
  return 0;
}

}
