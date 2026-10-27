#pragma once
import std;

#include "core/arena.hpp"
#include "core/long_bitset.hpp"
#include "core/mersenne_twister.hpp"
#include "map/map.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/effects.hpp"
#include "sim/order_generator.hpp"
#include "sim/player.hpp"
#include "sim/selection.hpp"
#include "sim/shroud.hpp"
#include "sim/screen_map.hpp"
#include "sim/target.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::net {
class OrderManager;
struct Order;
struct Session;
}

namespace ora::game {
class GameSpeed;
class ModData;
}

namespace ora::sim {

enum class WorldType { Regular, Shellmap, Editor };

struct WorldSimParams {
  int int4_random_seed = 0;
  int int4_timestep = 40;
  WorldType type = WorldType::Regular;
  map::Map* ptr_map = nullptr;

};

using CPosToWPos = std::function<WPos(const CPos&, sim::SubCell)>;

class World final {
 public:

  World(map::Map& map_world, game::ModData& mod_data, net::OrderManager& om,
        WorldType type);

  explicit World(WorldSimParams params = {});
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  const std::vector<Player*>& Players() const { return vec_players_; }
  void SetPlayers(std::vector<Player*> players, Player* local_player);

  Player* AdoptPlayer(std::unique_ptr<Player> player);
  Player* LocalPlayer() const { return p_local_player_; }
  Player* RenderPlayer() const { return p_render_player_; }
  void SetRenderPlayer(Player* p);
  void SetLocalPlayerForTest(Player* p) { p_local_player_ = p; }

  Actor* WorldActor() const { return p_world_actor_; }
  void SetWorldActor(Actor* a) { p_world_actor_ = a; }

  IActorMap* ActorMapFace() const { return ptr_actor_map_; }

  IControlGroups* ControlGroups() const { return ptr_control_groups_; }

  void SetActorMapFace(IActorMap* ptr_map_face) { ptr_actor_map_ = ptr_map_face; }
  ScreenMap* ScreenMapFace() const { return ptr_screen_map_; }
  ISelection* Selection() const { return ptr_selection_; }
  IOrderGenerator* OrderGenerator() const { return ptr_order_generator_; }
  void SetOrderGenerator(IOrderGenerator* ptr_generator);

  void AdoptOrderGenerator(std::unique_ptr<IOrderGenerator> ptr_generator);

  net::Session& LobbyInfo();

  bool RulesContainTemporaryBlocker() const {
    return b_rules_contain_temporary_blocker_;
  }

  std::span<ICustomMovementLayer* const> CustomMovementLayers() {
    return ptr_actor_map_ != nullptr ? ptr_actor_map_->CustomMovementLayers()
                                     : std::span<ICustomMovementLayer* const>{};
  }

  PlayerMaskSet AllPlayersMask;
  PlayerMaskSet NoPlayersMask;

  void SetLocalClientId(int id) { int4_local_client_id_ = id; }
  int LocalClientId() const { return int4_local_client_id_; }
  void SetIsHostResolver(std::function<bool()> fn) {
    fn_is_host_ = std::move(fn);
  }
  bool IsHost() const { return fn_is_host_ ? fn_is_host_() : false; }

  void SetFogObscuresResolver(std::function<bool(Actor&)> fn_actor,
                              std::function<bool(const CPos&)> fn_cell,
                              std::function<bool(const WPos&)> fn_pos = {}) {
    fn_fog_obscures_actor_ = std::move(fn_actor);
    fn_fog_obscures_cell_ = std::move(fn_cell);
    fn_fog_obscures_pos_ = std::move(fn_pos);
  }
  bool FogObscures(Actor& a) {
    return RenderPlayer() != nullptr &&
           (fn_fog_obscures_actor_ ? fn_fog_obscures_actor_(a) : false);
  }
  bool FogObscures(const CPos& p) {
    return RenderPlayer() != nullptr &&
           (fn_fog_obscures_cell_ ? fn_fog_obscures_cell_(p) : false);
  }
  bool FogObscures(const WPos& pos) {
    return RenderPlayer() != nullptr &&
           (fn_fog_obscures_pos_ ? fn_fog_obscures_pos_(pos) : false);
  }
  bool ShroudObscures(const CPos& p) {
    return RenderPlayer() != nullptr && RenderPlayer()->GetShroud() != nullptr &&
           !RenderPlayer()->GetShroud()->IsExplored(p);
  }

  [[noreturn]] static void ThrowMissingTrait(std::string_view str_type_full) {
    throw std::runtime_error(
        std::format("TypeDictionary does not contain instance of type `{}`",
                    str_type_full));
  }

  using TraitFactory = std::function<std::vector<TraitBase*>(
      const std::string& str_name, ActorInitializer& init)>;
  void SetTraitFactory(TraitFactory fn) { fn_trait_factory_ = std::move(fn); }

  void CreateTraitsForActor(Actor& actor, ActorInitializer& init,
                            const std::string& str_name);

  ora::WorldArena& Arena() { return arena_; }

  Actor* CreateActor(const std::string& str_name, TypeDictionary& init_dict);
  Actor* CreateActor(bool add_to_world, const std::string& str_name,
                     TypeDictionary& init_dict);
  void Add(Actor* a);
  void Remove(Actor* a);

  std::uint64_t AddActorAddedHandler(std::function<void(Actor&)> fn) {
    const std::uint64_t uint8_token = uint8_next_event_token_++;
    vec_actor_added_handlers_.push_back({uint8_token, std::move(fn)});
    return uint8_token;
  }
  void RemoveActorAddedHandler(std::uint64_t uint8_token) {
    for (auto& pair_handler : vec_actor_added_handlers_)
      if (pair_handler.first == uint8_token)
        pair_handler.second = nullptr;
  }
  std::uint64_t AddActorRemovedHandler(std::function<void(Actor&)> fn) {
    const std::uint64_t uint8_token = uint8_next_event_token_++;
    vec_actor_removed_handlers_.push_back({uint8_token, std::move(fn)});
    return uint8_token;
  }
  void RemoveActorRemovedHandler(std::uint64_t uint8_token) {
    for (auto& pair_handler : vec_actor_removed_handlers_)
      if (pair_handler.first == uint8_token)
        pair_handler.second = nullptr;
  }

  void AddRenderPlayerChangedHandler(std::function<void(Player*)> fn) {
    vec_render_player_changed_handlers_.push_back(std::move(fn));
  }

  Actor* AdoptActor(std::unique_ptr<Actor> a);

  void AddToMaps(Actor* self, IOccupySpace* ios);
  void UpdateMaps(Actor* self, IOccupySpace* ios);
  void RemoveFromMaps(Actor* self, IOccupySpace* ios);

  void Add(std::unique_ptr<IEffect> e);
  void Remove(IEffect* e);
  void RemoveAll(const std::function<bool(IEffect*)>& predicate);

  void AddFrameEndTask(std::function<void(World&)> a) {
    queue_frame_end_actions_.push(std::move(a));
  }

  bool Paused() const { return b_paused_; }
  void SetPaused(bool b) { b_paused_ = b; }
  bool PredictedPaused() const { return b_predicted_paused_; }
  void SetPredictedPaused(bool b) { b_predicted_paused_ = b; }
  void SetPauseState(bool paused);
  void SetLocalPauseState(bool paused) {
    b_paused_ = b_predicted_paused_ = paused;
  }

  int WorldTick() const { return int4_world_tick_; }
  int Timestep() const { return int4_timestep_; }
  int ReplayTimestep() const { return int4_replay_timestep_; }
  void SetReplayTimestep(int v) { int4_replay_timestep_ = v; }

  WorldType Type() const { return type_; }

  MersenneTwister& SharedRandom() { return mt_shared_; }
  MersenneTwister& LocalRandom() { return mt_local_; }

  void Tick();
  void TickRender();

  std::vector<Actor*> Actors() const;
  const std::vector<IEffect*>& Effects() const { return vec_effects_; }
  const std::vector<IEffect*>& UnpartitionedEffects() const {
    return vec_unpartitioned_effects_;
  }
  const std::vector<ISync*>& SyncedEffects() const {
    return vec_synced_effects_;
  }
  Actor* GetActorById(std::uint32_t actor_id);

  TraitDictionary& TraitDict() { return trait_dict_; }

  template <class T>
  std::vector<TraitPair<T>> ActorsWithTrait() {
    return trait_dict_.ActorsWithTrait<T>();
  }

  template <class T>
  void ApplyToActorsWithTrait(const std::function<void(Actor*, T*)>& action) {
    trait_dict_.ApplyToActorsWithTrait<T>(action);
  }

  template <class T>
  void ApplyToActorsWithTraitTimed(
      const std::function<void(Actor*, T*)>& action, std::string_view text) {
    trait_dict_.ApplyToActorsWithTraitTimed<T>(action, text);
  }

  template <class T>
  std::vector<Actor*> ActorsHavingTrait() {
    return trait_dict_.ActorsHavingTrait<T>();
  }

  template <class T>
  std::vector<Actor*> ActorsHavingTrait(
      const std::function<bool(T*)>& predicate) {
    return trait_dict_.ActorsHavingTrait<T>(predicate);
  }

  int SyncHash();

  sim::Target TargetFromCell(const CPos& cell, sim::SubCell sub_cell) const;
  void SetSubCellCenterResolver(CPosToWPos fn_resolve) {
    fn_subcell_center_ = std::move(fn_resolve);
  }

  void SetSyncEffectHashResolver(std::function<int(const ISync*)> fn_resolve) {
    fn_sync_effect_hash_ = std::move(fn_resolve);
  }

  int SyncEffectHash(const ISync* s) const {
    return fn_sync_effect_hash_ ? fn_sync_effect_hash_(s) : 0;
  }

  map::Map& Map() const {
    if (ptr_map_ == nullptr)
      throw std::runtime_error("NullReferenceException");
    return *ptr_map_;
  }
  map::Map* MapPtr() const { return ptr_map_; }
  game::ModData* ModDataFace() const { return ptr_mod_data_; }
  net::OrderManager* OM() const { return p_order_manager_; }
  void SetOrderManager(net::OrderManager* om) { p_order_manager_ = om; }

  void SetOrderIssueSink(std::function<void(net::Order*)> fn) {
    fn_issue_order_ = std::move(fn);
  }
  void IssueOrder(net::Order* o);

  void SetRequestGameSaveSink(
      std::function<void(const std::string&, bool)> fn) {
    fn_request_game_save_ = std::move(fn);
  }
  void RequestGameSave(const std::string& str_filename,
                       bool b_is_autosave) {
    if (fn_request_game_save_)
      fn_request_game_save_(str_filename, b_is_autosave);
  }

  bool IsGameOver() const { return b_is_game_over_; }
  void EndGame();
  void OutOfSync();
  void SetWorldOwner(Player* p);

  void SetIsReplayResolver(std::function<bool()> fn) {
    fn_is_replay_ = std::move(fn);
  }
  bool IsReplay() const { return fn_is_replay_ ? fn_is_replay_() : false; }

  bool IsLoadingGameSave() const;
  int GameSaveLoadingPercentage() const;

  void LoadComplete(gfx::WorldRenderer* wr);
  void PostLoadComplete(gfx::WorldRenderer* wr);

  void AddGameSaveTraitData(int trait_index, yaml::MiniYaml yaml_value) {
    map_game_save_trait_data_[trait_index] = std::move(yaml_value);
  }

  void OnClientDisconnected(int client_id);

  void OnClientDisconnectedProxy(int client_id) {
    OnClientDisconnected(client_id);
  }

  void SetPauseShellmap(bool b) { b_pause_shellmap_ = b; }

  void SetSoundHooks(std::function<void()> fn_stop_audio,
                     std::function<void()> fn_stop_video,
                     std::function<void(bool)> fn_disable_all_sounds) {
    fn_sound_stop_audio_ = std::move(fn_stop_audio);
    fn_sound_stop_video_ = std::move(fn_stop_video);
    fn_sound_disable_all_sounds_ = std::move(fn_disable_all_sounds);
  }

  void OnPlayerWinStateChanged(Player& player);

  bool Disposing() const { return b_disposing_; }

  bool IsGameOverProxy() const { return IsGameOver(); }

  void Dispose();

  template <class Fn>
  void RunUnsyncedGuard(Fn&& fn) {
    RunUnsynced(true, this, std::forward<Fn>(fn));
  }

  void CancelInputMode();

  bool RulesContainTemporaryBlocker();

  bool ContainsTemporaryBlocker(CPos cell, Actor* ignore_actor = nullptr);

  std::vector<Actor*> FindActorsInCircle(const WPos& origin, const WDist& r);

 private:

  friend class Actor;
  std::uint32_t NextAID() { return uint4_next_aid_++; }

  TraitDictionary trait_dict_;
  std::map<std::uint32_t, Actor*> map_actors_;

  std::vector<std::pair<std::uint64_t, std::function<void(Actor&)>>>
      vec_actor_added_handlers_;
  std::vector<std::pair<std::uint64_t, std::function<void(Actor&)>>>
      vec_actor_removed_handlers_;
  std::uint64_t uint8_next_event_token_ = 1;

  std::vector<std::function<void(Player*)>> vec_render_player_changed_handlers_;

  bool b_rules_temporary_blocker_cached_ = false;
  bool b_rules_contain_temporary_blocker_ = false;

  ora::WorldArena arena_{64 * 1024};
  std::vector<std::unique_ptr<Actor>> vec_owned_actors_;

  std::vector<std::unique_ptr<IEffect>> vec_owned_effects_;
  std::vector<IEffect*> vec_effects_;
  std::vector<IEffect*> vec_unpartitioned_effects_;
  std::vector<ISync*> vec_synced_effects_;

  std::queue<std::function<void(World&)>> queue_frame_end_actions_;

  map::Map* ptr_map_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;

  int int4_timestep_ = 40;
  int int4_replay_timestep_ = 40;
  MersenneTwister mt_shared_;
  MersenneTwister mt_local_;

  std::vector<Player*> vec_players_;
  std::vector<std::unique_ptr<Player>> vec_owned_players_;

  Player* p_local_player_ = nullptr;
  Player* p_render_player_ = nullptr;
  Actor* p_world_actor_ = nullptr;
  net::OrderManager* p_order_manager_ = nullptr;
  std::function<void(net::Order*)> fn_issue_order_;
  std::function<void(const std::string&, bool)> fn_request_game_save_;
  TraitFactory fn_trait_factory_;
  CPosToWPos fn_subcell_center_;
  std::function<int(const ISync*)> fn_sync_effect_hash_;

  ScreenMap* ptr_screen_map_ = nullptr;
  IActorMap* ptr_actor_map_ = nullptr;
  IControlGroups* ptr_control_groups_ = nullptr;
  ISelection* ptr_selection_ = nullptr;
  IOrderGenerator* ptr_order_generator_ = nullptr;
  std::unique_ptr<IOrderGenerator> owned_order_generator_;

  std::string str_default_order_generator_;
  std::vector<IValidateOrder*> vec_order_validators_;
  std::vector<INotifyPlayerDisconnected*> vec_notify_disconnected_;

  bool b_is_game_over_ = false;
  bool b_was_loading_game_save_ = false;
  std::map<int, yaml::MiniYaml> map_game_save_trait_data_;
  bool b_pause_shellmap_ = false;

  std::function<bool()> fn_is_replay_;
  std::function<bool()> fn_is_host_;
  int int4_local_client_id_ = 0;
  std::function<bool(Actor&)> fn_fog_obscures_actor_;
  std::function<bool(const CPos&)> fn_fog_obscures_cell_;
  std::function<bool(const WPos&)> fn_fog_obscures_pos_;
  std::function<void()> fn_sound_stop_audio_;
  std::function<void()> fn_sound_stop_video_;
  std::function<void(bool)> fn_sound_disable_all_sounds_;

  WorldType type_ = WorldType::Regular;
  bool b_paused_ = false;
  bool b_predicted_paused_ = false;
  int int4_world_tick_ = 0;
  std::uint32_t uint4_next_aid_ = 0;
  bool b_disposing_ = false;
};

template <class Fn>
auto RunUnsynced(bool check_sync_hash, World* world, Fn&& fn)
    -> decltype(fn()) {
  auto& count = sim::UnsyncCountRef();
  ++count;

  const int sync =
      count == 1 && check_sync_hash && world != nullptr ? world->SyncHash()
                                                         : 0;

  struct Dec {
    ~Dec() { --sim::UnsyncCountRef(); }
  } dec{};

  if constexpr (std::is_void_v<std::invoke_result_t<Fn&>>) {
    fn();
    if (count == 1 && check_sync_hash && world != nullptr &&
        !world->Disposing() && sync != world->SyncHash())
      throw std::runtime_error("RunUnsynced: sync-changing code may not run here");
  } else {
    auto result = fn();
    if (count == 1 && check_sync_hash && world != nullptr &&
        !world->Disposing() && sync != world->SyncHash())
      throw std::runtime_error("RunUnsynced: sync-changing code may not run here");
    return result;
  }
}

template <class T>
inline T* Actor::Trait() {
  return world_.TraitDict().Get<T>(this);
}

template <class T>
inline T* Actor::TraitOrDefault() {
  return world_.TraitDict().GetOrDefault<T>(this);
}

template <class T>
inline std::vector<T*> Actor::TraitsImplementing() {
  return world_.TraitDict().WithInterface<T>(this);
}

}
