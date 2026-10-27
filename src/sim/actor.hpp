#pragma once
import std;

#include "sim/activity.hpp"
#include "sim/type_dictionary.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::net {
struct Order;
}

namespace ora::gfx {
class WorldRenderer;
struct RenderItem;
}

namespace ora::sim {

class World;
class Player;

enum class SystemActors : std::int32_t {
  Player = 0,
  EditorPlayer = 1,
  World = 2,
  EditorWorld = 4,
};

struct ActorSyncHashEntry {
  ISync* trait = nullptr;
  gen::TypeId type_id =
      gen::TypeId::OpenRA_ISync;
  int (*hash_function)(const ISync*) = nullptr;
  int Hash() const { return hash_function != nullptr ? hash_function(trait) : 0; }
};

class Actor final {
 public:

  static constexpr int InvalidConditionToken = -1;

  Actor(World& world, std::string str_name, TypeDictionary& init_dict);
  ~Actor();

  Actor(const Actor&) = delete;
  Actor& operator=(const Actor&) = delete;

  const game::ActorInfo* Info() const { return info_; }
  const std::string& InfoName() const { return str_info_name_; }
  std::string DebugName() const;
  World& world() const { return world_; }
  std::uint32_t ActorID() const { return uint4_actor_id_; }
  Player* Owner() const { return p_owner_; }
  void SetOwnerInternal(Player* p) { p_owner_ = p; }
  bool IsInWorld() const { return b_is_in_world_; }
  void SetIsInWorld(bool b) { b_is_in_world_ = b; }

  void SetInfo(const game::ActorInfo* info) { info_ = info; }
  bool WillDispose() const { return b_will_dispose_; }
  bool Disposed() const { return b_disposed_; }

  Activity* CurrentActivity() {
    return Activity::SkipDoneActivities(current_activity_);
  }
  void SetCurrentActivity(Activity* a) { current_activity_ = a; }

  int Generation() const { return int4_generation_; }
  void BumpGeneration() { ++int4_generation_; }
  Actor* ReplacedByActor() const { return p_replaced_by_; }
  void SetReplacedByActor(Actor* a) { p_replaced_by_ = a; }

  bool IsIdle() const {
    return const_cast<Actor*>(this)->CurrentActivity() == nullptr;
  }

  CPos Location() const;
  WPos CenterPosition() const;
  bool IsDead() const;

  IOccupySpace* OccupiesSpace() const {
    return vec_occupy_space_.empty() ? nullptr : vec_occupy_space_.back();
  }

  WRot Orientation() const {
    return p_facing_ != nullptr ? p_facing_->Orientation() : WRot::None();
  }

  core::BitSet<TargetableType> GetAllTargetTypes() const;

  std::span<ITargetablePositions* const> EnabledTargetablePositions() const {
    return vec_enabled_targetable_positions_;
  }

  const std::vector<WPos>& EnabledTargetableWorldPositions() const {
    return vec_enabled_targetable_world_positions_;
  }

  std::vector<WPos> GetTargetablePositions() const;

  core::BitSet<TargetableType> GetEnabledTargetTypes() const;

  bool IsTargetableBy(Actor& by_actor) const;

  bool CanBeViewedByPlayer(Player* player) const {
    for (auto* modifier : vec_visibility_modifiers_)
      if (!modifier->IsVisible(const_cast<Actor&>(*this), player))
        return false;

    return p_default_visibility_ != nullptr
               ? p_default_visibility_->IsVisible(const_cast<Actor&>(*this),
                                                  player)
               : true;
  }

  std::span<ITargetable* const> Targetables() const { return vec_targetables_; }
  std::span<ICrushable* const> Crushables() const { return vec_crushables_; }

  bool AcceptsOrder(std::string_view ) const { return true; }

  IEffectiveOwner* EffectiveOwner() const {
    return p_effective_owner_;
  }

  void ShowTargetLines() const {}

  void Tick();

  void QueueActivity(bool queued, Activity* next_activity);
  void QueueActivity(Activity* next_activity);
  void CancelActivity();

  template <class T>
  T* Trait();
  template <class T>
  T* TraitOrDefault();
  template <class T>
  std::vector<T*> TraitsImplementing();
  void AddTrait(TraitBase* trait);

  void Initialize(bool add_to_world = true);
  void Dispose();

  void Kill(Actor* attacker,
            const core::BitSet<DamageType>& damage_types = {});

  void ResolveOrder(const net::Order& order);

  void ChangeOwner(Player* new_owner);
  void ChangeOwnerSync(Player* new_owner);

  int GrantCondition(std::string_view condition);
  int RevokeCondition(int token);
  bool TokenValid(int token) const;

  const std::map<std::string, int>& ConditionCache() const {
    return map_condition_cache_;
  }

  const std::vector<ActorSyncHashEntry>& SyncHashes() const {
    return vec_sync_hashes_;
  }
  std::vector<ActorSyncHashEntry>& MutableSyncHashes() {
    return vec_sync_hashes_;
  }

  void Render(gfx::WorldRenderer& wr, std::vector<gfx::RenderItem>& vec_out);

  std::vector<Rectangle> ScreenBounds(gfx::WorldRenderer& wr);

  Polygon MouseBounds(gfx::WorldRenderer& wr);

  bool Equals(const Actor& other) const {
    return uint4_actor_id_ == other.uint4_actor_id_;
  }

 private:

  void UpdateConditionState(std::string_view condition, int token,
                            bool is_revoke);

  World& world_;
  std::uint32_t uint4_actor_id_;
  const game::ActorInfo* info_ = nullptr;
  std::string str_info_name_;
  Player* p_owner_ = nullptr;
  bool b_is_in_world_ = false;
  bool b_will_dispose_ = false;
  bool b_disposed_ = false;
  Activity* current_activity_ = nullptr;
  int int4_generation_ = 0;
  Actor* p_replaced_by_ = nullptr;

  struct ConditionState {

    std::vector<VariableObserverNotifier> vec_notifiers;

    std::set<int> set_tokens;
  };
  std::map<std::string, ConditionState> map_condition_states_;
  std::map<int, std::string> map_condition_tokens_;
  int next_condition_token_ = 1;
  std::map<std::string, int> map_condition_cache_;

  std::vector<ActorSyncHashEntry> vec_sync_hashes_;
  std::vector<IOccupySpace*> vec_occupy_space_;

  IFacing* p_facing_ = nullptr;

  std::vector<ITargetable*> vec_targetables_;
  std::vector<ICrushable*> vec_crushables_;
  std::vector<IVisibilityModifier*> vec_visibility_modifiers_;
  IDefaultVisibility* p_default_visibility_ = nullptr;
  std::vector<ITargetablePositions*> vec_all_targetable_positions_;

  std::vector<ITargetablePositions*>
      vec_enabled_targetable_positions_;
  std::vector<WPos> vec_enabled_targetable_world_positions_;
  IEffectiveOwner* p_effective_owner_ = nullptr;
  std::vector<IRender*> vec_renders_;
  std::vector<IMouseBounds*> vec_mouse_bounds_;
  gfx::WorldRenderer* ptr_last_world_renderer_ =
      nullptr;
  std::vector<gfx::RenderItem>
      vec_cached_renderables_;
  bool b_created_ = false;
};

}

#include "sim/trait_dictionary.hpp"
