// UPSTREAM: OpenRA.Game/World.cs @b6fc03f L28-627(world.hpp 的实现;全量
//          构造链 + trait arena + 系统 trait 面)
//          The implementation of world.hpp — the full construction chain,
//          the trait arena, and the system-trait faces.
import std;

#include "sim/world.hpp"

#include "game/game_speed.hpp"
#include "game/mod_data.hpp"
#include "gfx/world_renderer.hpp"
#include "net/order_manager.hpp"
#include "net/session.hpp"
#include "sim/activity.hpp"
#include "sim/trait_registry.hpp"

namespace ora::sim {

namespace {

/// ToLowerInvariant(ASCII 面)| ToLowerInvariant (the ASCII face).
std::string LowerName(const std::string& str_name) {
  std::string lowered;
  lowered.reserve(str_name.size());
  for (char c : str_name)
    lowered.push_back(
        static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
  return lowered;
}

}  // namespace

// ———— 全量构造(L178-235)————
World::World(map::Map& map_world, game::ModData& mod_data,
             net::OrderManager& om, WorldType type)
    : ptr_map_(&map_world),
      ptr_mod_data_(&mod_data),
      p_order_manager_(&om),
      mt_shared_(om.LobbyInfo().global_settings.RandomSeed),
      mt_local_(0),  // LocalRandom:UI 域(同步面禁隐式播种 —— 显式 0)
      type_(type) {
  // L185-192:DefaultOrderGenerator 校验 + 注册表构造(ObjectCreator 反射
  // 面 → 名字分派注册表;异常文本逐字)
  // L185-192: the DefaultOrderGenerator check + the registry construct
  // (the ObjectCreator reflection face → the name-dispatch registry; the
  // exception texts verbatim).
  const game::Manifest& manifest = mod_data.ManifestRef();
  str_default_order_generator_ = manifest.DefaultOrderGenerator();
  if (str_default_order_generator_.empty())
    throw std::runtime_error("mod.yaml must define a DefaultOrderGenerator");
  if (!OrderGeneratorRegistered(str_default_order_generator_))
    throw std::runtime_error(std::format("{} is not a valid DefaultOrderGenerator",
                                         str_default_order_generator_));
  owned_order_generator_ = CreateOrderGenerator(str_default_order_generator_, *this);
  ptr_order_generator_ = owned_order_generator_.get();

  // GameSpeed(L194-197)
  const game::GameSpeeds& game_speeds = mod_data.GetOrCreateGameSpeeds();
  const std::string str_game_speed_name =
      om.LobbyInfo().global_settings.OptionOrDefault("gamespeed",
                                                     game_speeds.DefaultSpeed);
  const game::GameSpeed* speed = game_speeds.Find(str_game_speed_name);
  if (speed == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  int4_timestep_ = int4_replay_timestep_ = speed->Timestep;

  // L202-209:WorldActor + 系统 trait 解析
  const char* world_actor_type =
      type == WorldType::Editor ? "editorworld" : "world";
  TypeDictionary empty_dict;
  p_world_actor_ = CreateActor(world_actor_type, empty_dict);

  ptr_actor_map_ = p_world_actor_->TraitOrDefault<IActorMap>();
  if (ptr_actor_map_ == nullptr)
    ThrowMissingTrait("OpenRA.Traits.IActorMap");
  ptr_screen_map_ = p_world_actor_->TraitOrDefault<ScreenMap>();
  if (ptr_screen_map_ == nullptr)
    ThrowMissingTrait("OpenRA.Traits.ScreenMap");
  ptr_selection_ = p_world_actor_->TraitOrDefault<ISelection>();
  if (ptr_selection_ == nullptr)
    ThrowMissingTrait("OpenRA.Traits.ISelection");
  ptr_control_groups_ = p_world_actor_->TraitOrDefault<IControlGroups>();
  if (ptr_control_groups_ == nullptr)
    ThrowMissingTrait("OpenRA.Traits.IControlGroups");
  vec_order_validators_ =
      p_world_actor_->TraitsImplementing<IValidateOrder>();
  vec_notify_disconnected_ =
      p_world_actor_->TraitsImplementing<INotifyPlayerDisconnected>();

  // L211:LongBitSet<PlayerBitMask>.Reset()(玩家创建前复位位分配器)
  // L211: LongBitSet<PlayerBitMask>.Reset() (rewinds the allocator before
  // player creation).
  PlayerMaskSet::Reset();

  // Create an isolated RNG ...(上游注释)
  MersenneTwister player_random{om.LobbyInfo().global_settings.RandomSeed};
  for (auto* cmp : p_world_actor_->TraitsImplementing<ICreatePlayers>())
    cmp->CreatePlayers(*this, player_random);

  // Game.Sound.SoundVolumeModifier = 1.0f(L218;声音面随嵌入侧)
  // gameInfo(L220-231;GameInformation 随 Phase 7)
  // RulesContainTemporaryBlocker(L233)
  b_rules_contain_temporary_blocker_ = std::any_of(
      ptr_map_->Rules().Actors().begin(), ptr_map_->Rules().Actors().end(),
      [](const auto& pair_actor) {
        return pair_actor.second->HasTraitInfoOfInterface(
            "OpenRA.Traits.ITemporaryBlockerInfo");
      });
  // gameSettings(L234;PauseShellmap 的 Settings 面随 Settings 批)

  // Map.CenterOfSubCell 接线(全量 ctor 注入精确实现 —— D28 的解除面)
  SetSubCellCenterResolver([this](const CPos& cell, SubCell sub_cell) {
    return ptr_map_->CenterOfSubCell(cell, sub_cell);
  });
}

// ———— 测试构造(Phase 3 注入面)————
World::World(WorldSimParams params)
    : ptr_map_{params.ptr_map},
      int4_timestep_(params.int4_timestep),
      int4_replay_timestep_(params.int4_timestep),
      mt_shared_(params.int4_random_seed),
      mt_local_(0),
      type_(params.type) {}

World::~World() {
  // Dispose(L589-619)的析构形态(显式 Dispose 的幂等复核)
  if (!b_disposing_)
    Dispose();

  // arena.Reset 统一蒸发 trait/actor 内存(析构序:登记表逆序 —— 新 trait
  // 先于旧 trait,与上游 GC 终态等价)
  for (auto it = vec_owned_actors_.rbegin(); it != vec_owned_actors_.rend();
       ++it) {
    if (!(*it)->Disposed())
      (*it)->Dispose();
  }
  while (!queue_frame_end_actions_.empty()) {
    auto task = std::move(queue_frame_end_actions_.front());
    queue_frame_end_actions_.pop();
    task(*this);
  }
  vec_owned_effects_.clear();
  vec_effects_.clear();
  vec_unpartitioned_effects_.clear();
  vec_synced_effects_.clear();
  arena_.Reset();
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

  // if (IsReplay) return;(L128-130)
  if (IsReplay())
    return;

  p_render_player_ = local_player;  // L134(renderPlayer 直写字段)
}

Player* World::AdoptPlayer(std::unique_ptr<Player> player) {
  // 上游 GC 的显式等价(SetPlayers 前建链的 CreatePlayers 面)
  // The explicit GC equivalent (the CreatePlayers face building before
  // SetPlayers).
  Player* raw = player.get();
  vec_owned_players_.push_back(std::move(player));
  return raw;
}

void World::SetRenderPlayer(Player* p) {
  // L91-104
  if (p_local_player_ == nullptr || p_local_player_->UnlockedRenderPlayer()) {
    p_render_player_ = p;
    // RenderPlayerChanged 事件(L101):消费面随渲染 trait 批
  }
}

void World::SetOrderGenerator(IOrderGenerator* ptr_generator) {
  // L156-167(注入面:清除注册表产物的所有权)
  // L156-167 (the injection face: releases the registry product's
  // ownership).
  RunUnsynced(true, this, [&] {});
  if (ptr_order_generator_ != nullptr)
    ptr_order_generator_->Deactivate();
  owned_order_generator_.reset();
  ptr_order_generator_ = ptr_generator;
}

void World::AdoptOrderGenerator(std::unique_ptr<IOrderGenerator> ptr_generator) {
  // L156-167 的所有权接管形态(注册表产物)
  // The ownership-taking form of L156-167 (the registry products).
  RunUnsynced(true, this, [&] {});
  if (ptr_order_generator_ != nullptr)
    ptr_order_generator_->Deactivate();
  owned_order_generator_ = std::move(ptr_generator);
  ptr_order_generator_ = owned_order_generator_.get();
}

void World::CancelInputMode() {
  // L172:defaultOrderGeneratorType.GetConstructor(World)?.Invoke → 注册表
  // 重造(未注册 = null 复位)
  // L172: the GetConstructor(World)?.Invoke → the registry rebuild (an
  // unregistered name resets to null).
  if (str_default_order_generator_.empty() ||
      !OrderGeneratorRegistered(str_default_order_generator_)) {
    AdoptOrderGenerator(nullptr);
    return;
  }
  AdoptOrderGenerator(CreateOrderGenerator(str_default_order_generator_, *this));
}

net::Session& World::LobbyInfo() {
  // L138:LobbyInfo => OrderManager.LobbyInfo
  if (p_order_manager_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  return p_order_manager_->LobbyInfo();
}

Actor* World::AdoptActor(std::unique_ptr<Actor> a) {
  Actor* raw = a.get();
  vec_owned_actors_.push_back(std::move(a));
  return raw;
}

void World::CreateTraitsForActor(Actor& actor, ActorInitializer& init,
                                 const std::string& str_name) {
  // Actor.cs L169-195 的 World 侧半边
  const std::string lowered = LowerName(str_name);

  if (fn_trait_factory_) {
    // 测试面:工厂返回 arena 内构造的 trait(所有权归 arena —— D26/D27)
    auto traits = fn_trait_factory_(lowered, init);
    if (traits.empty())
      throw std::runtime_error("No rules definition for unit " + str_name);
    for (TraitBase* trait : traits) {
      actor.AddTrait(trait);
      for (const auto& entry : trait->TraitUpcasts()) {
        if (entry.type_id == ISync::kTypeId) {
          auto* s = static_cast<ISync*>(entry.upcast(trait));
          actor.MutableSyncHashes().push_back(
              ActorSyncHashEntry{s, FindSyncHashFunction(trait->GetTraitTypeId())});
        }
      }
    }
    return;
  }

  // 全量面:规则表解析 + TraitRegistry 逐 trait 构造(上游
  // info.TraitsInConstructOrder() 循环)
  if (ptr_map_ == nullptr)
    throw std::runtime_error("No rules definition for unit " + str_name);

  const game::ActorInfo* info = ptr_map_->Rules().FindActor(lowered);
  if (info == nullptr)
    throw std::runtime_error("No rules definition for unit " + str_name);

  for (const meta::RecordObject* rec : info->TraitsInConstructOrder()) {
    TraitBase* trait = TraitRegistry::Instance().Create(
        std::string{rec->record_desc().str_name}, *rec, init, arena_);
    if (trait == nullptr)
      continue;  // 未注册 Info:部分覆盖装配面(COVERAGE 登记)
    actor.AddTrait(trait);
    for (const auto& entry : trait->TraitUpcasts()) {
      if (entry.type_id == ISync::kTypeId) {
        auto* s = static_cast<ISync*>(entry.upcast(trait));
        actor.MutableSyncHashes().push_back(
            ActorSyncHashEntry{s, FindSyncHashFunction(trait->GetTraitTypeId())});
      }
    }
  }
}

Actor* World::CreateActor(const std::string& str_name,
                          TypeDictionary& init_dict) {
  return CreateActor(true, str_name, init_dict);
}

Actor* World::CreateActor(bool add_to_world, const std::string& str_name,
                          TypeDictionary& init_dict) {
  // L327-332(actor 对象入 arena —— D26/D27)
  Actor* a = arena_.Create<Actor>(*this, str_name, init_dict);
  a->Initialize(add_to_world);
  return a;
}

void World::Add(Actor* a) {
  // L334-342
  a->SetIsInWorld(true);
  map_actors_.emplace(a->ActorID(), a);
  // ActorAdded 事件(L338;消费面随渲染/脚本批)

  for (auto* t : a->TraitsImplementing<INotifyAddedToWorld>())
    t->AddedToWorld(*a);
}

void World::Remove(Actor* a) {
  // L344-352
  a->SetIsInWorld(false);
  map_actors_.erase(a->ActorID());
  // ActorRemoved 事件(L348)

  for (auto* t : a->TraitsImplementing<INotifyRemovedFromWorld>())
    t->RemovedFromWorld(*a);
}

void World::AddToMaps(Actor* self, IOccupySpace* ios) {
  // L237-242
  ptr_actor_map_->AddInfluence(self, ios);
  ptr_actor_map_->AddPosition(self, ios);
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->AddOrUpdate(self);
}

void World::UpdateMaps(Actor* self, IOccupySpace* ios) {
  // L244-251
  if (!self->IsInWorld())
    return;
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->AddOrUpdate(self);
  ptr_actor_map_->UpdatePosition(self, ios);
}

void World::RemoveFromMaps(Actor* self, IOccupySpace* ios) {
  // L253-258
  ptr_actor_map_->RemoveInfluence(self, ios);
  ptr_actor_map_->RemovePosition(self, ios);
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->Remove(self);
}

void World::Add(std::unique_ptr<IEffect> e) {
  // L354-363
  IEffect* raw = e.get();
  vec_effects_.push_back(raw);

  if (dynamic_cast<ISpatiallyPartitionable*>(raw) == nullptr)
    vec_unpartitioned_effects_.push_back(raw);

  if (auto* se = dynamic_cast<ISync*>(raw))
    vec_synced_effects_.push_back(se);

  vec_owned_effects_.push_back(std::move(e));
}

void World::Remove(IEffect* e) {
  // L365-374(三列表同步摘除 + 所有权即刻释放)
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

void World::SetPauseState(bool paused) {
  // L399-406
  if (b_is_game_over_)
    return;
  // IssueOrder(Order.FromTargetString("PauseGame", paused ? "Pause" :
  // "UnPause", false))(Order 静态工厂随订单族批;此处保留锚点)
  b_predicted_paused_ = paused;
}

void World::Tick() {
  // L413-455
  if (b_was_loading_game_save_ && !IsLoadingGameSave()) {
    // L415-436:gameSaveTraitData 回灌
    for (auto& [key, yaml_value] : map_game_save_trait_data_) {
      auto pairs = ActorsWithTrait<IGameSaveTraitData>();
      // Skip(kv.Key).FirstOrDefault()
      if (static_cast<std::size_t>(key) >= pairs.size())
        break;
      pairs[static_cast<std::size_t>(key)].trait->ResolveTraitData(
          *pairs[static_cast<std::size_t>(key)].actor, yaml_value);
    }
    map_game_save_trait_data_.clear();

    if (fn_sound_disable_all_sounds_)
      fn_sound_disable_all_sounds_(false);
    for (auto* nsr : p_world_actor_->TraitsImplementing<INotifyGameLoaded>())
      nsr->GameLoaded(*this);

    b_was_loading_game_save_ = false;
  }

  // Allow users to pause the shellmap via the settings menu(上游注释)
  if (!b_paused_ &&
      (type_ != WorldType::Shellmap || !b_pause_shellmap_ ||
       int4_world_tick_ == 0)) {
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
  // L458-462
  ApplyToActorsWithTraitTimed<ITickRender>(
      [](Actor*, ITickRender*) {}, "Render");
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->TickRender();
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
  // Target.cs L46-57(ctor)
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

// ———— DefaultOrderGenerator 名字分派注册表(order_generator.hpp 的实现)
// ———— The DefaultOrderGenerator name-dispatch registry (the
// order_generator.hpp implementation).
namespace {

std::vector<std::pair<std::string,
                      std::function<std::unique_ptr<IOrderGenerator>(World&)>>>&
OrderGeneratorEntries() {
  static std::vector<
      std::pair<std::string,
                std::function<std::unique_ptr<IOrderGenerator>(World&)>>>
      entries;
  return entries;
}

}  // namespace

void RegisterOrderGenerator(
    std::string str_name,
    std::function<std::unique_ptr<IOrderGenerator>(World&)> fn_factory) {
  auto& entries = OrderGeneratorEntries();
  // 重复注册 = 装配错误(先到先得,与 TraitRegistry 同形)
  // A duplicate registration is an assembly error (first wins, the same
  // shape as TraitRegistry).
  if (!std::any_of(entries.begin(), entries.end(),
                   [&](const auto& e) { return e.first == str_name; }))
    entries.emplace_back(std::move(str_name), std::move(fn_factory));
}

bool OrderGeneratorRegistered(const std::string& str_name) {
  const auto& entries = OrderGeneratorEntries();
  return std::any_of(entries.begin(), entries.end(),
                     [&](const auto& e) { return e.first == str_name; });
}

std::unique_ptr<IOrderGenerator> CreateOrderGenerator(const std::string& str_name,
                                                      World& world) {
  for (const auto& [name, factory] : OrderGeneratorEntries())
    if (name == str_name)
      return factory(world);
  return nullptr;  // 上游 FindType null → ?.Invoke 的 null 面
                   // the null face of FindType null → ?.Invoke.
}

void World::IssueOrder(net::Order* o) {
  // L151:sink 分发(见 world.hpp 注记;缺 sink = OM 缺失的 NRE 等价抛)
  if (fn_issue_order_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  fn_issue_order_(o);
}

bool World::IsLoadingGameSave() const {
  // L116
  const net::OrderManager* om = p_order_manager_;
  return om != nullptr && om->NetFrameNumber() <= om->GameSaveLastFrame();
}

int World::GameSaveLoadingPercentage() const {
  // L118
  const net::OrderManager* om = p_order_manager_;
  if (om == nullptr)
    throw std::runtime_error("NullReferenceException");
  return om->NetFrameNumber() * 100 / om->GameSaveLastFrame();
}

void World::EndGame() {
  // L75-88
  if (b_is_game_over_) {
    return;
  }
  SetPauseState(true);
  b_is_game_over_ = true;

  for (auto* t : p_world_actor_->TraitsImplementing<IGameOver>())
    t->GameOver(*this);

  // gameInfo.FinalGameTick = WorldTick(L85;GameInformation 随 Phase 7)
  // GameOver() 事件(L86;FinishBenchmark 随嵌入侧)
}

void World::SetWorldOwner(Player* p) {
  // L312-315
  p_world_actor_->SetOwnerInternal(p);
}

void World::LoadComplete(gfx::WorldRenderer* wr) {
  // L260-298(wr 可空 = 测试无头面;gameInfo/ReplayMetadata 面随 Phase 7)
  if (IsLoadingGameSave()) {
    b_was_loading_game_save_ = true;
    if (fn_sound_disable_all_sounds_)
      fn_sound_disable_all_sounds_(true);
    for (auto* nsr : p_world_actor_->TraitsImplementing<INotifyGameLoading>())
      nsr->GameLoading(*this);
  }

  // ScreenMap must be initialized before anything else(上游注释)
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->WorldLoaded(*this, wr);

  for (auto* iwl : p_world_actor_->TraitsImplementing<IWorldLoaded>()) {
    // These have already been initialized(上游注释)
    if (iwl == dynamic_cast<IWorldLoaded*>(ptr_screen_map_))
      continue;
    iwl->WorldLoaded(*this, wr);
  }

  for (auto* p : vec_players_)
    for (auto* iwl : p->PlayerActor()->TraitsImplementing<IWorldLoaded>())
      iwl->WorldLoaded(*this, wr);

  // gameInfo.AddPlayer / DisabledSpawnPoints / StartTimeUtc(L289-294;
  // GameInformation 随 Phase 7)
  // ReplayMetadata(L296-297;随 Phase 7)
}

void World::PostLoadComplete(gfx::WorldRenderer* wr) {
  // L300-310
  for (auto* iwl : p_world_actor_->TraitsImplementing<IPostWorldLoaded>())
    iwl->PostWorldLoaded(*this, wr);

  for (auto* p : vec_players_)
    for (auto* iwl : p->PlayerActor()->TraitsImplementing<IPostWorldLoaded>())
      iwl->PostWorldLoaded(*this, wr);
}

void World::OnPlayerWinStateChanged(Player& player) {
  // L539-547(gameInfo 面随 Phase 7)
  (void)player;
}

void World::OnClientDisconnected(int client_id) {
  // L549-563
  for (auto* player : vec_players_) {
    if (player->ClientIndex() != client_id)
      continue;
    // p.PlayerReference.Playable 过滤(PlayerReference 面随玩家创建链批;
    // Playable 字段承载)
    if (!player->Playable())
      continue;

    for (auto* np : vec_notify_disconnected_)
      np->PlayerDisconnected(*p_world_actor_, *player);

    for (auto* p : vec_players_)
      p->PlayerDisconnected(*player);  // L558

    // gameInfo.DisconnectFrame(L559-561;随 Phase 7)
  }
}

void World::Dispose() {
  // L589-619
  b_disposing_ = true;

  if (ptr_order_generator_ != nullptr)
    ptr_order_generator_->Deactivate();

  while (!queue_frame_end_actions_.empty())
    queue_frame_end_actions_.pop();

  if (fn_sound_stop_audio_)
    fn_sound_stop_audio_();
  if (fn_sound_stop_video_)
    fn_sound_stop_video_();
  if (IsLoadingGameSave() && fn_sound_disable_all_sounds_)
    fn_sound_disable_all_sounds_(false);

  // Dispose newer actors first, and the world actor last(上游注释)
  for (auto it = map_actors_.rbegin(); it != map_actors_.rend(); ++it)
    it->second->Dispose();

  // Actor disposals are done in a FrameEndTask(上游注释)
  while (!queue_frame_end_actions_.empty()) {
    auto task = std::move(queue_frame_end_actions_.front());
    queue_frame_end_actions_.pop();
    task(*this);
  }

  // HACK: The shellmap OrderManager is owned by its world ...(上游注释;
  // C++ 侧 OM 归 Game 所有 —— shellmap 分支由 JoinInner 的保活语义承载,
  // D105 系)
  // Map.Dispose()(L616)
  if (ptr_map_ != nullptr)
    ptr_map_->Dispose();

  // Game.FinishBenchmark(L618;嵌入侧)
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
