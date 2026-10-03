// UPSTREAM: OpenRA.Game/World.cs @7d57605 L178-627(实现部分;仿真核心)
//          The implementation half of World.cs (sim core).
#include "sim/world.hpp"

#include "sim/activity.hpp"

namespace ora::sim {

World::World(WorldSimParams params)
    : int4_timestep_(params.int4_timestep),
      int4_replay_timestep_(params.int4_timestep),
      mt_shared_(params.int4_random_seed),
      mt_local_(0),  // LocalRandom:UI 域(C# 无参构造用 TickCount;同步面禁
                     // 隐式播种 —— 显式 0,COVERAGE 登记)
      type_(params.type) {}

World::~World() {
  // Dispose(L589-619)的仿真核心子集:所有权逆序销毁(新 actor 先,
  // world actor 最后 —— "Dispose newer actors first, and the world actor
  // last")
  b_disposing_ = true;

  for (auto it = vec_owned_actors_.rbegin(); it != vec_owned_actors_.rend();
       ++it) {
    if (!(*it)->Disposed())
      (*it)->Dispose();
  }

  // Actor disposals are done in a FrameEndTask
  while (!queue_frame_end_actions_.empty()) {
    auto task = std::move(queue_frame_end_actions_.front());
    queue_frame_end_actions_.pop();
    task(*this);
  }

  vec_owned_effects_.clear();
  vec_effects_.clear();
  vec_unpartitioned_effects_.clear();
  vec_synced_effects_.clear();
}

void World::SetPlayers(std::vector<Player*> players, Player* local_player) {
  // L60-66
  if (!vec_players_.empty())
    throw std::runtime_error("Players are fixed once they have been set.");
  vec_players_ = std::move(players);

  // SetLocalPlayer(L120-135)
  if (local_player == nullptr)
    return;
  bool found = false;
  for (auto* p : vec_players_)
    found = found || p == local_player;
  if (!found)
    throw std::runtime_error(
        "The local player must be one of the players in the world.");

  p_local_player_ = local_player;
  p_render_player_ = local_player;
}

Actor* World::AdoptActor(std::unique_ptr<Actor> a) {
  Actor* raw = a.get();
  vec_owned_actors_.push_back(std::move(a));
  return raw;
}

void World::CreateTraitsForActor(Actor& actor, ActorInitializer& init,
                                 const std::string& str_name) {
  (void)init;  // trait 工厂签名对齐上游 Create(init);桩工厂不消费 init
  if (!fn_trait_factory_)
    throw std::runtime_error("No rules definition for unit " + str_name);

  // name 小写化 + 规则查询 + 构造序创建在工厂内闭合(上游 L150-155/169-171)
  std::string lowered;
  lowered.reserve(str_name.size());
  for (char c : str_name)
    lowered.push_back(static_cast<char>(
        c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));  // ToLowerInvariant(ASCII)

  auto traits = fn_trait_factory_(lowered);
  if (traits.empty())
    throw std::runtime_error("No rules definition for unit " + str_name);

  // Actor.cs L169-195 循环的 World 侧半边:AddTrait + ISync 收集(L193)
  for (auto& trait : traits) {
    actor.AddTrait(trait.get());

    for (const auto& entry : trait->TraitUpcasts()) {
      if (entry.type_id == ISync::kTypeId) {
        auto* s = static_cast<ISync*>(entry.upcast(trait.get()));
        actor.MutableSyncHashes().push_back(ActorSyncHashEntry{
            s, FindSyncHashFunction(trait->GetTraitTypeId())});
      }
    }
  }

  // trait 对象所有权:World 级表(见头注;Phase 5 arena 替换)
  for (auto& trait : traits)
    vec_owned_traits_.push_back(std::move(trait));
}

Actor* World::CreateActor(const std::string& str_name,
                          TypeDictionary& init_dict) {
  return CreateActor(true, str_name, init_dict);
}

Actor* World::CreateActor(bool add_to_world, const std::string& str_name,
                          TypeDictionary& init_dict) {
  // L327-332
  auto a = std::make_unique<Actor>(*this, str_name, init_dict);
  Actor* raw = AdoptActor(std::move(a));
  raw->Initialize(add_to_world);
  return raw;
}

void World::Add(Actor* a) {
  // L334-342
  a->SetIsInWorld(true);
  map_actors_.emplace(a->ActorID(), a);

  for (auto* t : a->TraitsImplementing<INotifyAddedToWorld>())
    t->AddedToWorld(*a);
}

void World::Remove(Actor* a) {
  // L344-352
  a->SetIsInWorld(false);
  map_actors_.erase(a->ActorID());

  for (auto* t : a->TraitsImplementing<INotifyRemovedFromWorld>())
    t->RemovedFromWorld(*a);
}

void World::Add(std::unique_ptr<IEffect> e) {
  // L354-363
  IEffect* raw = e.get();
  vec_effects_.push_back(raw);

  // is not ISpatiallyPartitionable → unpartitioned(dynamic_cast 一次性
  // 构造成本;上游 is 模式匹配)
  if (dynamic_cast<ISpatiallyPartitionable*>(raw) == nullptr)
    vec_unpartitioned_effects_.push_back(raw);

  if (auto* se = dynamic_cast<ISync*>(raw))
    vec_synced_effects_.push_back(se);

  vec_owned_effects_.push_back(std::move(e));
}

void World::Remove(IEffect* e) {
  // L365-374(三列表同步摘除 + 所有权即刻释放;上游 GC 语义下 Remove 后
  // 不再触达 —— DelayedAction 的闭包在入队时已拷贝 fn)
  auto drop_effect = [e](std::vector<IEffect*>& v) {
    v.erase(std::remove(v.begin(), v.end(), e), v.end());
  };
  drop_effect(vec_effects_);
  drop_effect(vec_unpartitioned_effects_);

  if (auto* se = dynamic_cast<ISync*>(e)) {
    vec_synced_effects_.erase(
        std::remove(vec_synced_effects_.begin(), vec_synced_effects_.end(), se),
        vec_synced_effects_.end());
  }

  vec_owned_effects_.erase(
      std::remove_if(vec_owned_effects_.begin(), vec_owned_effects_.end(),
                     [e](const std::unique_ptr<IEffect>& p) {
                       return p.get() == e;
                     }),
      vec_owned_effects_.end());
}

void World::RemoveAll(const std::function<bool(IEffect*)>& predicate) {
  // L376-381:先收集后逐个 Remove(迭代中析构安全)
  std::vector<IEffect*> to_remove;
  for (auto& e : vec_owned_effects_)
    if (predicate(e.get()))
      to_remove.push_back(e.get());
  for (auto* e : to_remove)
    Remove(e);
}

void World::Tick() {
  // L413-455(gameSave 恢复段属 Phase 5 游戏存档面;wasLoadingGameSave 恒
  // false 跳过)
  if (!b_paused_) {
    ++int4_world_tick_;

    for (auto& [id, a] : map_actors_)
      a->Tick();

    ApplyToActorsWithTraitTimed<ITick>(
        [](Actor* actor, ITick* trait) { trait->Tick(*actor); }, "Trait");

    for (auto* e : vec_effects_)
      e->Tick(*this);
  }

  while (!queue_frame_end_actions_.empty()) {
    auto task = std::move(queue_frame_end_actions_.front());
    queue_frame_end_actions_.pop();
    task(*this);
  }
}

void World::TickRender() {
  // L458-462(ITickRender 分发;WorldRenderer/ScreenMap 参数 Phase 4)
  ApplyToActorsWithTraitTimed<ITickRender>(
      [](Actor*, ITickRender*) {}, "Render");
}

std::vector<Actor*> World::Actors() const {
  std::vector<Actor*> out;
  out.reserve(map_actors_.size());
  for (auto& [id, a] : map_actors_)
    out.push_back(a);
  return out;
}

Actor* World::GetActorById(std::uint32_t actor_id) {
  // L469-474
  auto it = map_actors_.find(actor_id);
  if (it != map_actors_.end())
    return it->second;
  return nullptr;
}

sim::Target World::TargetFromCell(const CPos& cell,
                                  sim::SubCell sub_cell) const {
  // Target.cs L46-57(ctor):type=Terrain,center=CenterOfSubCell(cell,
  // subCell),positions=[center],cell/subCell 槽位保留
  const WPos center = fn_subcell_center_
                          ? fn_subcell_center_(cell, sub_cell)
                          : WPos{cell.X() * 1024 + 512, cell.Y() * 1024 + 512, 0};
  sim::Target t = sim::Target::FromPos(center);
  t.b_has_cell = true;
  t.cell = cell;
  t.b_has_sub_cell = true;
  t.sub_cell = sub_cell;
  return t;
}

void World::OnClientDisconnectedProxy(int client_id) {
  // L549-563 仿真核心面:玩家过滤 + Player.PlayerDisconnected(Phase 5
  // 完整通知链 —— notifyDisconnected trait 族与 gameInfo 登记)
  (void)client_id;
}

int World::SyncHash() {
  // L482-512:公式逐项(n 连续;乘法回绕)
  auto n = 0;
  auto ret = 0;

  // Hash all the actors.
  for (auto* a : Actors())
    ret += n++ * static_cast<int>(1 + a->ActorID()) * sync::HashActor(a);

  // Hash fields marked with the ISync interface.
  for (auto* actor : ActorsHavingTrait<ISync>())
    for (const auto& sync_hash : actor->SyncHashes())
      ret += n++ * static_cast<int>(1 + actor->ActorID()) *
             sync_hash.Hash();

  // Hash game state relevant effects such as projectiles.
  for (auto* sync : vec_synced_effects_)
    ret += n++ * (fn_sync_effect_hash_ ? fn_sync_effect_hash_(sync) : 0);

  // Hash the shared random number generator.
  ret += mt_shared_.Last;

  // Hash player RenderPlayer status
  for (auto* p : vec_players_)
    if (p->UnlockedRenderPlayer())
      ret += sync::HashPlayer(p);

  return ret;
}

}  // namespace ora::sim
