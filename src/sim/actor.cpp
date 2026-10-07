// UPSTREAM: OpenRA.Game/Actor.cs @b6fc03f L129-615(实现部分;渲染面除外)
//          The implementation half of Actor.cs (render surface excluded).
#include "sim/actor.hpp"

#include "sim/actor_init.hpp"
#include "sim/world.hpp"

namespace ora::sim {

namespace {

/// IObservesVariables trait 的观察者条目收集(Initialize 用)
/// Collector for the observer entries of IObservesVariables traits (for
/// Initialize).
struct ObserverEntry {
  VariableObserverNotifier notifier;
  std::vector<std::string> variables;
};

}  // namespace

Actor::Actor(World& world, std::string str_name, TypeDictionary& init_dict)
    : world_(world), str_info_name_(std::move(str_name)) {
  // L131-136:重复单实例 init 检查(按具体类型计数)
  {
    std::map<gen::TypeId, int> counts;
    gen::TypeId duplicate = gen::TypeId::OpenRA_Traits_ITraitInfoInterface;  // 哨兵
    bool has_duplicate = false;
    for (auto* i : init_dict.WithInterface<ISingleInstanceInit>()) {
      auto* init = dynamic_cast<ActorInit*>(i);
      if (init == nullptr)
        continue;
      auto id = init->GetInitTypeId();
      if (++counts[id] > 1) {
        duplicate = id;
        has_duplicate = true;
        break;  // C# FirstOrDefault 语义(取首个计数超 1 者)
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

  // L148-210:name 路径(trait 创建经 World 工厂;循环体内接口缓存保持上游
  // 形态)。name 小写化与规则表查询在工厂内闭合(同一依赖面)
  if (!str_info_name_.empty())
    world_.CreateTraitsForActor(*this, init, str_info_name_);
}

Actor::~Actor() = default;

std::string Actor::DebugName() const {
  // ToString(L390-397;PERF: Avoid format strings 的等价拼接)
  std::string name = str_info_name_ + " " + std::to_string(uint4_actor_id_);
  if (!b_is_in_world_)
    name += " (not in world)";
  return name;
}

CPos Actor::Location() const {
  // L84:OccupiesSpace.TopLeft(缓存 trait;上游最后写入者)
  return vec_occupy_space_.empty() ? CPos{}
                                   : vec_occupy_space_.back()->TopLeft();
}

WPos Actor::CenterPosition() const {
  return vec_occupy_space_.empty() ? WPos{}
                                   : vec_occupy_space_.back()->CenterPosition();
}

bool Actor::IsDead() const {
  return b_disposed_;  // L82:Disposed || health.IsDead(IHealth 接线 Phase 5)
}

void Actor::Tick() {
  // L272-290
  const bool was_idle = IsIdle();
  SetCurrentActivity(RunActivity(*this, CurrentActivity()));

  if (!was_idle && IsIdle()) {
    // INotifyBecomingIdle 缓存面:L179-194 的 becomingIdles(TraitsImplementing
    // 实时查询,上游构造期缓存的等价行为)
    for (auto* n : world_.TraitDict().WithInterface<INotifyBecomingIdle>(this))
      n->OnBecomingIdle(*this);

    // If IsIdle is true, it means the last CurrentActivity.Tick returned null.
    // If a next activity has been queued via OnBecomingIdle, we need to start
    // running it now, to avoid an 'empty' null tick where the actor will
    // (visibly, if moving) do nothing.
    SetCurrentActivity(RunActivity(*this, CurrentActivity()));
  } else if (was_idle) {
    for (auto* t : world_.TraitDict().WithInterface<INotifyIdle>(this))
      t->TickIdle(*this);
  }
}

void Actor::QueueActivity(bool queued, Activity* next_activity) {
  // L351-357
  if (!queued)
    CancelActivity();

  QueueActivity(next_activity);
}

void Actor::QueueActivity(Activity* next_activity) {
  // L359-368
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
  // L370-373
  if (auto* a = CurrentActivity())
    a->Cancel(*this);
}

void Actor::AddTrait(TraitBase* trait) {
  // L414-417(接口缓存面 L169-195:upcast 表逐键填充 —— 单值缓存后者覆写,
  // 列表缓存构造序;SyncHashes 由 CreateTraitsForActor 的同形循环收集)
  // L414-417 (the interface caches of L169-195: filled per key off the
  // upcast table — the single-value caches take the last write, the list
  // caches keep construct order; SyncHashes ride CreateTraitsForActor's
  // same-shape loop).
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
  }
}

core::BitSet<TargetableType> Actor::GetAllTargetTypes() const {
  // L517-525(PERF:避 LINQ;构造序并集)
  // L517-525 (PERF: avoiding LINQ; the construct-order union).
  std::uint64_t uint8_bits = 0;
  for (const auto* targetable : vec_targetables_)
    uint8_bits |= targetable->TargetTypes().RawBits();
  return core::BitSet<TargetableType>::FromRawBits(uint8_bits);
}

core::BitSet<TargetableType> Actor::GetEnabledTargetTypes() const {
  // L530-538(PERF:避 LINQ;启用者并集 —— ITargetable 实现即 TraitBase
  // 子对象的使能面)
  // L530-538 (PERF: avoiding LINQ; the enabled union — an ITargetable
  // implementor's enablement rides its TraitBase subobject).
  std::uint64_t uint8_bits = 0;
  for (auto* targetable : vec_targetables_) {
    auto* base = dynamic_cast<TraitBase*>(targetable);
    if (base == nullptr || base->IsTraitEnabled())
      uint8_bits |= targetable->TargetTypes().RawBits();
  }
  return core::BitSet<TargetableType>::FromRawBits(uint8_bits);
}

bool Actor::IsTargetableBy(Actor& by_actor) const {
  // L540-548(PERF:避 LINQ)
  // L540-548 (PERF: avoiding LINQ).
  for (auto* targetable : vec_targetables_)
    if (targetable->TargetableBy(const_cast<Actor&>(*this), by_actor))
      return true;

  return false;
}

void Actor::Initialize(bool add_to_world) {
  // L213-270
  b_created_ = true;

  // Make sure traits are usable for condition notifiers
  for (auto* t : TraitsImplementing<INotifyCreated>())
    t->Created(*this);

  // L221-238:观察者收集(上游 HashSet<Notifier> 的初始通知去重见头注偏离;
  // 此处按条目序通知)
  std::vector<ObserverEntry> all_entries;
  for (auto* provider : TraitsImplementing<IObservesVariables>()) {
    for (auto& variable_user : provider->GetVariableObservers()) {
      ObserverEntry entry{variable_user.fn_notifier,
                          variable_user.vec_variables};
      all_entries.push_back(std::move(entry));

      for (const auto& variable : variable_user.vec_variables) {
        auto& cs = map_condition_states_[variable];
        cs.vec_notifiers.push_back(variable_user.fn_notifier);

        // Initialize conditions that have not yet been granted to 0
        // NOTE: Some conditions may have already been granted by
        // INotifyCreated calling GrantCondition, and we choose to assign the
        // token count to safely cover both cases instead of adding an if
        // branch.
        map_condition_cache_[variable] =
            static_cast<int>(cs.set_tokens.size());
      }
    }
  }

  // Update all traits with their initial condition state
  // (上游 HashSet 枚举 → 条目序;见头注)
  for (auto& entry : all_entries)
    entry.notifier(*this, ConditionCache());

  // L249-266:ICreationActivity(仅一个启用;入列首)
  ICreationActivity* creation_activity_provider = nullptr;
  for (auto* ica : TraitsImplementing<ICreationActivity>()) {
    // 上游 ica.IsTraitEnabled() = Exts.IsTraitEnabled(IDisabledTrait 判定)
    // —— 经 TraitBase 使能面承载
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

void Actor::Dispose() {
  // L419-444
  // If CurrentActivity isn't null, run OnActorDisposeOuter in case some
  // cleanups are needed. This should be done before the FrameEndTask to
  // avoid dependency issues.
  if (auto* a = CurrentActivity())
    a->OnActorDisposeOuter(*this);

  // Allow traits/activities to prevent a race condition when they depend on
  // disposing the actor (e.g. Transforms)
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
  // L446-450
  for (auto* r : TraitsImplementing<IResolveOrder>())
    r->ResolveOrder(*this, order);
}

void Actor::ChangeOwner(Player* new_owner) {
  // L453-456
  world_.AddFrameEndTask([this, new_owner](World&) {
    if (!b_disposed_)
      ChangeOwnerSync(new_owner);
  });
}

void Actor::ChangeOwnerSync(Player* new_owner) {
  // L462-485
  if (b_disposed_)
    return;

  Player* old_owner = p_owner_;
  const bool was_in_world = b_is_in_world_;

  // momentarily remove from world so the ownership queries don't get confused
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

// ———— 条件系统(L558-615)————
// ———— The condition system (L558-615) ————

void Actor::UpdateConditionState(std::string_view condition, int token,
                                 bool is_revoke) {
  auto& condition_state = map_condition_states_[std::string(condition)];

  if (is_revoke)
    condition_state.set_tokens.erase(token);
  else
    condition_state.set_tokens.insert(token);

  map_condition_cache_[std::string(condition)] =
      static_cast<int>(condition_state.set_tokens.size());

  // Conditions may be granted or revoked before the state is initialized.
  // These notifications will be processed after INotifyCreated.Created.
  if (b_created_)
    for (auto& notify : condition_state.vec_notifiers)
      notify(*this, ConditionCache());
}

int Actor::GrantCondition(std::string_view condition) {
  // L583-592
  if (condition.empty())
    return InvalidConditionToken;

  auto token = next_condition_token_++;
  map_condition_tokens_[token] = std::string(condition);
  UpdateConditionState(condition, token, false);
  return token;
}

int Actor::RevokeCondition(int token) {
  // L599-607
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
  // L610-613
  return map_condition_tokens_.count(token) != 0;
}

}  // namespace ora::sim

namespace ora::sim::sync {

/// HashActor(Sync.cs L124-129)
int HashActor(const Actor* a) {
  if (a != nullptr)
    return static_cast<int>(a->ActorID() << 16);
  return 0;
}

}  // namespace ora::sim::sync
