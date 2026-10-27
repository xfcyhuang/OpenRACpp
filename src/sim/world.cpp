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

std::string LowerName(const std::string& str_name) {
  std::string lowered;
  lowered.reserve(str_name.size());
  for (char c : str_name)
    lowered.push_back(
        static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
  return lowered;
}

}

World::World(map::Map& map_world, game::ModData& mod_data,
             net::OrderManager& om, WorldType type)
    : ptr_map_(&map_world),
      ptr_mod_data_(&mod_data),
      p_order_manager_(&om),
      mt_shared_(om.LobbyInfo().global_settings.RandomSeed),
      mt_local_(0),
      type_(type) {

  const game::Manifest& manifest = mod_data.ManifestRef();
  str_default_order_generator_ = manifest.DefaultOrderGenerator();
  if (str_default_order_generator_.empty())
    throw std::runtime_error("mod.yaml must define a DefaultOrderGenerator");
  if (!OrderGeneratorRegistered(str_default_order_generator_))
    throw std::runtime_error(std::format("{} is not a valid DefaultOrderGenerator",
                                         str_default_order_generator_));
  owned_order_generator_ = CreateOrderGenerator(str_default_order_generator_, *this);
  ptr_order_generator_ = owned_order_generator_.get();

  const game::GameSpeeds& game_speeds = mod_data.GetOrCreateGameSpeeds();
  const std::string str_game_speed_name =
      om.LobbyInfo().global_settings.OptionOrDefault("gamespeed",
                                                     game_speeds.DefaultSpeed);
  const game::GameSpeed* speed = game_speeds.Find(str_game_speed_name);
  if (speed == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  int4_timestep_ = int4_replay_timestep_ = speed->Timestep;

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

  PlayerMaskSet::Reset();

  MersenneTwister player_random{om.LobbyInfo().global_settings.RandomSeed};
  for (auto* cmp : p_world_actor_->TraitsImplementing<ICreatePlayers>())
    cmp->CreatePlayers(*this, player_random);

  b_rules_contain_temporary_blocker_ = std::any_of(
      ptr_map_->Rules().Actors().begin(), ptr_map_->Rules().Actors().end(),
      [](const auto& pair_actor) {
        return pair_actor.second->HasTraitInfoOfInterface(
            "OpenRA.Traits.ITemporaryBlockerInfo");
      });

  SetSubCellCenterResolver([this](const CPos& cell, SubCell sub_cell) {
    return ptr_map_->CenterOfSubCell(cell, sub_cell);
  });
}

World::World(WorldSimParams params)
    : ptr_map_{params.ptr_map},
      int4_timestep_(params.int4_timestep),
      int4_replay_timestep_(params.int4_timestep),
      mt_shared_(params.int4_random_seed),
      mt_local_(0),
      type_(params.type) {}

World::~World() {

  if (!b_disposing_)
    Dispose();

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

  if (!vec_players_.empty())
    throw std::runtime_error("Players are fixed once they have been set.");
  vec_players_ = std::move(players);

  if (local_player == nullptr)
    return;
  bool found = false;
  for (auto* p : vec_players_)
    found = found || p == local_player;
  if (!found)
    throw std::runtime_error(
        "The local player must be one of the players in the world.");

  p_local_player_ = local_player;

  if (IsReplay())
    return;

  p_render_player_ = local_player;
}

Player* World::AdoptPlayer(std::unique_ptr<Player> player) {

  Player* raw = player.get();
  vec_owned_players_.push_back(std::move(player));
  return raw;
}

void World::SetRenderPlayer(Player* p) {

  if (p_local_player_ == nullptr || p_local_player_->UnlockedRenderPlayer()) {
    p_render_player_ = p;

    for (const auto& fn : vec_render_player_changed_handlers_)
      fn(p);
  }
}

void World::SetOrderGenerator(IOrderGenerator* ptr_generator) {

  RunUnsynced(true, this, [&] {});
  if (ptr_order_generator_ != nullptr)
    ptr_order_generator_->Deactivate();
  owned_order_generator_.reset();
  ptr_order_generator_ = ptr_generator;
}

void World::AdoptOrderGenerator(std::unique_ptr<IOrderGenerator> ptr_generator) {

  RunUnsynced(true, this, [&] {});
  if (ptr_order_generator_ != nullptr)
    ptr_order_generator_->Deactivate();
  owned_order_generator_ = std::move(ptr_generator);
  ptr_order_generator_ = owned_order_generator_.get();
}

void World::CancelInputMode() {

  if (str_default_order_generator_.empty() ||
      !OrderGeneratorRegistered(str_default_order_generator_)) {
    AdoptOrderGenerator(nullptr);
    return;
  }
  AdoptOrderGenerator(CreateOrderGenerator(str_default_order_generator_, *this));
}

net::Session& World::LobbyInfo() {

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

  const std::string lowered = LowerName(str_name);

  if (fn_trait_factory_) {

    auto traits = fn_trait_factory_(lowered, init);
    if (traits.empty())
      throw std::runtime_error("No rules definition for unit " + str_name);
    for (TraitBase* trait : traits) {
      actor.AddTrait(trait);
      for (const auto& entry : trait->TraitUpcasts()) {
        if (entry.type_id == ISync::kTypeId) {
          auto* s = static_cast<ISync*>(entry.upcast(trait));
          actor.MutableSyncHashes().push_back(
              ActorSyncHashEntry{s, trait->GetTraitTypeId(),
                                 FindSyncHashFunction(trait->GetTraitTypeId())});
        }
      }
    }
    return;
  }

  if (ptr_map_ == nullptr)
    throw std::runtime_error("No rules definition for unit " + str_name);

  const game::ActorInfo* info = ptr_map_->Rules().FindActor(lowered);
  if (info == nullptr)
    throw std::runtime_error("No rules definition for unit " + str_name);

  actor.SetInfo(info);

  for (const meta::RecordObject* rec : info->TraitsInConstructOrder()) {
    TraitBase* trait = TraitRegistry::Instance().Create(
        std::string{rec->record_desc().str_name}, *rec, init, arena_);
    if (trait == nullptr)
      continue;
    actor.AddTrait(trait);
    for (const auto& entry : trait->TraitUpcasts()) {
      if (entry.type_id == ISync::kTypeId) {
        auto* s = static_cast<ISync*>(entry.upcast(trait));
        actor.MutableSyncHashes().push_back(
            ActorSyncHashEntry{s, trait->GetTraitTypeId(),
                               FindSyncHashFunction(trait->GetTraitTypeId())});
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

  Actor* a = arena_.Create<Actor>(*this, str_name, init_dict);
  a->Initialize(add_to_world);
  return a;
}

void World::Add(Actor* a) {

  a->SetIsInWorld(true);
  map_actors_.emplace(a->ActorID(), a);

  for (auto& [uint8_token, fn] : vec_actor_added_handlers_)
    if (fn)
      fn(*a);

  for (auto* t : a->TraitsImplementing<INotifyAddedToWorld>())
    t->AddedToWorld(*a);
}

void World::Remove(Actor* a) {

  a->SetIsInWorld(false);
  map_actors_.erase(a->ActorID());

  for (auto& [uint8_token, fn] : vec_actor_removed_handlers_)
    if (fn)
      fn(*a);

  for (auto* t : a->TraitsImplementing<INotifyRemovedFromWorld>())
    t->RemovedFromWorld(*a);
}

void World::AddToMaps(Actor* self, IOccupySpace* ios) {

  ptr_actor_map_->AddInfluence(self, ios);
  ptr_actor_map_->AddPosition(self, ios);
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->AddOrUpdate(self);
}

void World::UpdateMaps(Actor* self, IOccupySpace* ios) {

  if (!self->IsInWorld())
    return;
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->AddOrUpdate(self);
  ptr_actor_map_->UpdatePosition(self, ios);
}

void World::RemoveFromMaps(Actor* self, IOccupySpace* ios) {

  ptr_actor_map_->RemoveInfluence(self, ios);
  ptr_actor_map_->RemovePosition(self, ios);
  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->Remove(self);
}

void World::Add(std::unique_ptr<IEffect> e) {

  IEffect* raw = e.get();
  vec_effects_.push_back(raw);

  if (dynamic_cast<ISpatiallyPartitionable*>(raw) == nullptr)
    vec_unpartitioned_effects_.push_back(raw);

  if (auto* se = dynamic_cast<ISync*>(raw))
    vec_synced_effects_.push_back(se);

  vec_owned_effects_.push_back(std::move(e));
}

void World::Remove(IEffect* e) {

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

  std::vector<IEffect*> to_remove;
  for (auto& e : vec_owned_effects_)
    if (predicate(e.get()))
      to_remove.push_back(e.get());
  for (auto* e : to_remove)
    Remove(e);
}

void World::SetPauseState(bool paused) {

  if (b_is_game_over_)
    return;

  b_predicted_paused_ = paused;
}

void World::Tick() {

  if (b_was_loading_game_save_ && !IsLoadingGameSave()) {

    for (auto& [key, yaml_value] : map_game_save_trait_data_) {
      auto pairs = ActorsWithTrait<IGameSaveTraitData>();

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

  ApplyToActorsWithTraitTimed<ITickRender>(
      [](Actor* a, ITickRender* t) { t->TickRender(*a); }, "Render");
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

  auto it = map_actors_.find(actor_id);
  if (it != map_actors_.end())
    return it->second;
  return nullptr;
}

sim::Target World::TargetFromCell(const CPos& cell,
                                  sim::SubCell sub_cell) const {

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

}

void RegisterOrderGenerator(
    std::string str_name,
    std::function<std::unique_ptr<IOrderGenerator>(World&)> fn_factory) {
  auto& entries = OrderGeneratorEntries();

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
  return nullptr;

}

void World::IssueOrder(net::Order* o) {

  if (fn_issue_order_ == nullptr)
    throw std::runtime_error("NullReferenceException");
  fn_issue_order_(o);
}

bool World::IsLoadingGameSave() const {

  const net::OrderManager* om = p_order_manager_;
  return om != nullptr && om->NetFrameNumber() <= om->GameSaveLastFrame();
}

int World::GameSaveLoadingPercentage() const {

  const net::OrderManager* om = p_order_manager_;
  if (om == nullptr)
    throw std::runtime_error("NullReferenceException");
  return om->NetFrameNumber() * 100 / om->GameSaveLastFrame();
}

void World::EndGame() {

  if (b_is_game_over_) {
    return;
  }
  SetPauseState(true);
  b_is_game_over_ = true;

  for (auto* t : p_world_actor_->TraitsImplementing<IGameOver>())
    t->GameOver(*this);

}

void World::OutOfSync() {

  EndGame();

  int4_replay_timestep_ = 0;
}

void World::SetWorldOwner(Player* p) {

  p_world_actor_->SetOwnerInternal(p);
}

void World::LoadComplete(gfx::WorldRenderer* wr) {

  if (IsLoadingGameSave()) {
    b_was_loading_game_save_ = true;
    if (fn_sound_disable_all_sounds_)
      fn_sound_disable_all_sounds_(true);
    for (auto* nsr : p_world_actor_->TraitsImplementing<INotifyGameLoading>())
      nsr->GameLoading(*this);
  }

  if (ptr_screen_map_ != nullptr)
    ptr_screen_map_->WorldLoaded(*this, wr);

  for (auto* iwl : p_world_actor_->TraitsImplementing<IWorldLoaded>()) {

    if (iwl == dynamic_cast<IWorldLoaded*>(ptr_screen_map_))
      continue;
    iwl->WorldLoaded(*this, wr);
  }

  for (auto* p : vec_players_)
    for (auto* iwl : p->PlayerActor()->TraitsImplementing<IWorldLoaded>())
      iwl->WorldLoaded(*this, wr);

}

void World::PostLoadComplete(gfx::WorldRenderer* wr) {

  for (auto* iwl : p_world_actor_->TraitsImplementing<IPostWorldLoaded>())
    iwl->PostWorldLoaded(*this, wr);

  for (auto* p : vec_players_)
    for (auto* iwl : p->PlayerActor()->TraitsImplementing<IPostWorldLoaded>())
      iwl->PostWorldLoaded(*this, wr);
}

void World::OnPlayerWinStateChanged(Player& player) {

  (void)player;
}

void World::OnClientDisconnected(int client_id) {

  for (auto* player : vec_players_) {
    if (player->ClientIndex() != client_id)
      continue;

    if (!player->Playable())
      continue;

    for (auto* np : vec_notify_disconnected_)
      np->PlayerDisconnected(*p_world_actor_, *player);

    for (auto* p : vec_players_)
      p->PlayerDisconnected(*player);

  }
}

void World::Dispose() {

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

  for (auto it = map_actors_.rbegin(); it != map_actors_.rend(); ++it)
    it->second->Dispose();

  while (!queue_frame_end_actions_.empty()) {
    auto task = std::move(queue_frame_end_actions_.front());
    queue_frame_end_actions_.pop();
    task(*this);
  }

  if (ptr_map_ != nullptr)
    ptr_map_->Dispose();

}

int World::SyncHash() {

  auto n = 0;
  auto ret = 0;

  for (auto* a : Actors())
    ret += n++ * static_cast<int>(1 + a->ActorID()) * sync::HashActor(a);

  for (auto* actor : ActorsHavingTrait<ISync>())
    for (const auto& sync_hash : actor->SyncHashes())
      ret += n++ * static_cast<int>(1 + actor->ActorID()) *
             sync_hash.Hash();

  for (auto* sync : vec_synced_effects_)
    ret += n++ * (fn_sync_effect_hash_ ? fn_sync_effect_hash_(sync) : 0);

  ret += mt_shared_.Last;

  for (auto* p : vec_players_)
    if (p->UnlockedRenderPlayer())
      ret += sync::HashPlayer(p);

  return ret;
}

bool World::RulesContainTemporaryBlocker() {

  if (!b_rules_temporary_blocker_cached_) {
    b_rules_contain_temporary_blocker_ = false;
    if (ptr_map_ != nullptr) {
      for (const auto& [name, info] : Map().Rules().Actors()) {
        (void)name;
        if (info->HasTraitInfoOfInterface("OpenRA.Traits.ITemporaryBlockerInfo")) {
          b_rules_contain_temporary_blocker_ = true;
          break;
        }
      }
    }
    b_rules_temporary_blocker_cached_ = true;
  }

  return b_rules_contain_temporary_blocker_;
}

std::vector<Actor*> World::FindActorsInCircle(const WPos& origin,
                                                const WDist& r) {

  const WVec vec{r, r, WDist{0}};
  std::vector<Actor*> vec_out;
  for (Actor* a : ptr_actor_map_->ActorsInBox(origin - vec, origin + vec))
    if ((a->CenterPosition() - origin).HorizontalLengthSquared() <=
        r.LengthSquared())
      vec_out.push_back(a);
  return vec_out;
}

bool World::ContainsTemporaryBlocker(CPos cell, Actor* ignore_actor) {

  if (!RulesContainTemporaryBlocker())
    return false;

  for (Actor* temporary_blocker :
       ptr_actor_map_->GetActorsAt(cell)) {
    if (temporary_blocker == ignore_actor)
      continue;

    for (auto* temporary_blocker_trait :
         temporary_blocker->TraitsImplementing<ITemporaryBlocker>())
      if (temporary_blocker_trait->IsBlocking(*temporary_blocker, cell))
        return true;
  }

  return false;
}

}
