// UPSTREAM: OpenRA.Game/World.cs @b6fc03f L28-650(逐语义重写;gameInfo/
// ReplayMetadata/存档包面随 Phase 7 批)
//          Verbatim-semantics rewrite (gameInfo/ReplayMetadata/the save
//          package faces land with the Phase 7 batches).
//
// 机制对照 / Mechanism mapping:
//  - actors: SortedDictionary<uint, Actor> → std::map<uint32_t, Actor*>
//    (遍历序 = ActorID 升序,确定性关键)
//    actors: SortedDictionary → std::map (iteration order = ascending
//    ActorID, determinism-critical).
//  - trait 对象所有权:D26/D27 —— 上游 GC;C++ = WorldArena(§4.5 每局世界
//    区;bump + 逆序析构登记)。actor 的 Dispose 只摘字典引用,对象内存随
//    World 析构的 arena.Reset 统一蒸发;FrameArena 之外的字段(条件系统/
//    std::function)由登记表逆序析构
//    Trait object ownership (D26/D27): upstream GC; C++ = the WorldArena
//    (§4.5; bump + reverse-order destructor registration). An actor's
//    Dispose only drops dictionary references — the memory evaporates with
//    arena.Reset when the World dies.
//  - ScreenMap/Selection/ControlGroups/OrderValidators/notifyDisconnected:
//    WorldActor.Trait<>() 的构造期解析(全量 ctor;测试 ctor 保持注入面)
//    Resolved at construction from the WorldActor (the full ctor; the test
//    ctor keeps the injection faces).
//  - IActorMap:ActorMap trait(Mods.Common)随下一批 —— 本批解析面留空位
//    (AddToMaps/UpdateMaps/RemoveFromMaps 的 ActorMap 半段以空判跳过;
//    COVERAGE 登记)
//    IActorMap: the ActorMap trait lands next batch — the resolution face
//    is left empty this batch (the ActorMap half of AddToMaps/... skips on
//    null; in COVERAGE).
//  - SyncHash(L482-512):公式逐项(n 计数器跨段连续;乘法回绕 -fwrapv)
//    SyncHash (L482-512): the formula term by term.
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
#include "sim/screen_map.hpp"
#include "sim/target.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::net {
class OrderManager;
struct Order;
struct Session;
}  // namespace ora::net

namespace ora::game {
class GameSpeed;
class ModData;
}  // namespace ora::game

namespace ora::sim {

/// WorldType(World.cs L28)
enum class WorldType { Regular, Shellmap, Editor };

/// 测试/无地图构造参数(Phase 3 注入面;全量构造走 Map/ModData/OM ctor)
/// The test/no-map construction parameters (the Phase 3 injection face; the
/// full construction goes through the Map/ModData/OM ctor).
struct WorldSimParams {
  int int4_random_seed = 0;        // LobbyInfo.GlobalSettings.RandomSeed
  int int4_timestep = 40;          // GameSpeed.Timestep(默认 40ms)
  WorldType type = WorldType::Regular;
  map::Map* ptr_map = nullptr;     // 地图注入面(ActorMap/寻路的测试装配;
                                   // 全量 ctor 走 Map/ModData/OM 链)
                                   // The map injection face (the test
                                   // assembly of ActorMap/pathfinding; the
                                   // full ctor goes through the
                                   // Map/ModData/OM chain).
};

/// 子格中心换算器签名(Target.FromCell 的注入面)
/// The sub-cell center converter signature (the Target.FromCell
/// injection surface).
using CPosToWPos = std::function<WPos(const CPos&, sim::SubCell)>;

/// IEffect 视图(World.cs L34-36 的三列表)
class World final {
 public:
  /// 全量构造(World.cs L178-235):Map/ModData/OM 装配 + WorldActor 系统面
  /// 解析 + ICreatePlayers 链(Phase 5 第一批)
  /// The full ctor (World.cs L178-235): Map/ModData/OM assembly + the
  /// WorldActor system-face resolution + the ICreatePlayers chain.
  World(map::Map& map_world, game::ModData& mod_data, net::OrderManager& om,
        WorldType type);

  /// 测试构造(无地图;规则/系统面经 SetTraitFactory 注入)
  /// The test ctor (no map; rules/system faces injected via
  /// SetTraitFactory).
  explicit World(WorldSimParams params = {});
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  // ———— 玩家面(L53-135)————
  const std::vector<Player*>& Players() const { return vec_players_; }
  void SetPlayers(std::vector<Player*> players, Player* local_player);
  /// Player 对象所有权(上游 GC 的显式等价;CreateMapPlayers 消费)
  /// The Player-object ownership (the explicit GC equivalent; consumed by
  /// CreateMapPlayers).
  Player* AdoptPlayer(std::unique_ptr<Player> player);
  Player* LocalPlayer() const { return p_local_player_; }
  Player* RenderPlayer() const { return p_render_player_; }
  void SetRenderPlayer(Player* p);  // L91-104

  // ———— 世界 actor(L137/202-206)————
  Actor* WorldActor() const { return p_world_actor_; }
  void SetWorldActor(Actor* a) { p_world_actor_ = a; }

  // ———— 系统面(L141-146/202-209)————
  /// ActorMap(L140:世界 actor 的 IActorMap trait —— Phase 5 第二批接线)
  /// ActorMap (L140: the world actor's IActorMap trait — wired in Phase 5
  /// batch 2).
  IActorMap* ActorMapFace() const { return ptr_actor_map_; }
  /// ControlGroups(L169) | ControlGroups (L169).
  IControlGroups* ControlGroups() const { return ptr_control_groups_; }
  /// ActorMap 解析面的测试注入(全量 ctor 由 WorldActor trait 解析)
  /// The test injection of the ActorMap resolution face (the full ctor
  /// resolves it from the WorldActor trait).
  void SetActorMapFace(IActorMap* ptr_map_face) { ptr_actor_map_ = ptr_map_face; }
  ScreenMap* ScreenMapFace() const { return ptr_screen_map_; }
  ISelection* Selection() const { return ptr_selection_; }
  IOrderGenerator* OrderGenerator() const { return ptr_order_generator_; }
  void SetOrderGenerator(IOrderGenerator* ptr_generator);  // L156-167
  /// 所有权接管的换面(CancelInputMode/ctor 的注册表构造产物)
  /// The ownership-taking face (the registry products of
  /// CancelInputMode/ctor).
  void AdoptOrderGenerator(std::unique_ptr<IOrderGenerator> ptr_generator);

  /// LobbyInfo(L138:OM 的大厅面) | LobbyInfo (L138: the OM's lobby face).
  net::Session& LobbyInfo();

  /// RulesContainTemporaryBlocker(L233) | RulesContainTemporaryBlocker
  /// (L233).
  bool RulesContainTemporaryBlocker() const {
    return b_rules_contain_temporary_blocker_;
  }

  /// GetCustomMovementLayers(ActorMapWorldExts L684-688 的接口化承载)
  /// GetCustomMovementLayers (the interface carrier of ActorMapWorldExts
  /// L684-688).
  std::span<ICustomMovementLayer* const> CustomMovementLayers() {
    return ptr_actor_map_ != nullptr ? ptr_actor_map_->CustomMovementLayers()
                                     : std::span<ICustomMovementLayer* const>{};
  }

  /// AllPlayersMask/NoPlayersMask(L53-54):CreateMapPlayers 装配前者
  /// AllPlayersMask/NoPlayersMask (L53-54): CreateMapPlayers fills the
  /// former.
  PlayerMaskSet AllPlayersMask;
  PlayerMaskSet NoPlayersMask;

  /// Game.LocalClientId / Game.IsHost 的注入面(引擎装配侧;JoinLocal 面
  /// 与 D101 系)| the Game.LocalClientId / Game.IsHost injection faces.
  void SetLocalClientId(int id) { int4_local_client_id_ = id; }
  int LocalClientId() const { return int4_local_client_id_; }
  void SetIsHostResolver(std::function<bool()> fn) {
    fn_is_host_ = std::move(fn);
  }
  bool IsHost() const { return fn_is_host_ ? fn_is_host_() : false; }

  /// FogObscures(L106-108):注入承载(缺省 false = 无迷雾)
  /// FogObscures (L106-108): injection-carried (false = no fog).
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

  /// 上游 trait 查询的"缺实例"异常文本(ActorMap/ControlGroups 等待补批)
  /// The missing-instance exception text of the upstream trait queries.
  [[noreturn]] static void ThrowMissingTrait(std::string_view str_type_full) {
    throw std::runtime_error(
        std::format("TypeDictionary does not contain instance of type `{}`",
                    str_type_full));
  }

  // ———— trait 工厂(D26/D27;arena 内构造)————
  /// 测试注入面:返回 arena 内构造的 trait 列表(对象所有权归 World arena)
  /// The test injection face: returns arena-constructed traits (ownership
  /// sits in the World arena).
  using TraitFactory = std::function<std::vector<TraitBase*>(
      const std::string& str_name, ActorInitializer& init)>;
  void SetTraitFactory(TraitFactory fn) { fn_trait_factory_ = std::move(fn); }

  /// Actor 构造的规则面入口(全量 ctor = 规则表 + TraitRegistry;测试 ctor =
  /// 注入工厂)
  /// The rules-face entry of Actor construction (the full ctor = the rules
  /// table + TraitRegistry; the test ctor = the injected factory).
  void CreateTraitsForActor(Actor& actor, ActorInitializer& init,
                            const std::string& str_name);

  // ———— 内存面(§4.5;D26/D27)————
  ora::WorldArena& Arena() { return arena_; }

  // ———— actor 生命周期(L317-352)————
  Actor* CreateActor(const std::string& str_name, TypeDictionary& init_dict);
  Actor* CreateActor(bool add_to_world, const std::string& str_name,
                     TypeDictionary& init_dict);
  void Add(Actor* a);
  void Remove(Actor* a);

  // ———— ActorAdded/ActorRemoved 事件面(World.cs L338/L348;订阅方:TechTree
  //      的前置监听 / 渲染与脚本批的消费面)————
  // ———— The ActorAdded/ActorRemoved event faces (World.cs L338/L348; the
  //      subscribers: TechTree's prerequisite listening / the render and
  //      scripting batches' consumers). ————
  void AddActorAddedHandler(std::function<void(Actor&)> fn) {
    vec_actor_added_handlers_.push_back(std::move(fn));
  }
  void AddActorRemovedHandler(std::function<void(Actor&)> fn) {
    vec_actor_removed_handlers_.push_back(std::move(fn));
  }

  /// actor 对象所有权(arena 直构;测试构造路径同)—— AdoptActor 保留给
  /// 需要外部生命周期的注入面
  Actor* AdoptActor(std::unique_ptr<Actor> a);

  // ———— 地图登记面(L237-258)————
  void AddToMaps(Actor* self, IOccupySpace* ios);
  void UpdateMaps(Actor* self, IOccupySpace* ios);
  void RemoveFromMaps(Actor* self, IOccupySpace* ios);

  // ———— effects(L354-381)————
  void Add(std::unique_ptr<IEffect> e);
  void Remove(IEffect* e);
  void RemoveAll(const std::function<bool(IEffect*)>& predicate);

  // ———— 帧末任务队列(L383)————
  void AddFrameEndTask(std::function<void(World&)> a) {
    queue_frame_end_actions_.push(std::move(a));
  }

  // ———— 暂停/节拍(L388-462)————
  bool Paused() const { return b_paused_; }
  void SetPaused(bool b) { b_paused_ = b; }
  bool PredictedPaused() const { return b_predicted_paused_; }
  void SetPredictedPaused(bool b) { b_predicted_paused_ = b; }
  void SetPauseState(bool paused);      // L399-406
  void SetLocalPauseState(bool paused) {  // L408-411
    b_paused_ = b_predicted_paused_ = paused;
  }

  int WorldTick() const { return int4_world_tick_; }
  int Timestep() const { return int4_timestep_; }
  int ReplayTimestep() const { return int4_replay_timestep_; }
  void SetReplayTimestep(int v) { int4_replay_timestep_ = v; }

  WorldType Type() const { return type_; }

  MersenneTwister& SharedRandom() { return mt_shared_; }
  MersenneTwister& LocalRandom() { return mt_local_; }

  void Tick();       // L413-455
  void TickRender(); // L458-462

  // ———— 查询面(L464-537)————
  std::vector<Actor*> Actors() const;
  const std::vector<IEffect*>& Effects() const { return vec_effects_; }
  const std::vector<IEffect*>& UnpartitionedEffects() const {
    return vec_unpartitioned_effects_;
  }
  const std::vector<ISync*>& SyncedEffects() const {
    return vec_synced_effects_;
  }
  Actor* GetActorById(std::uint32_t actor_id);  // L469-474

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

  // ———— SyncHash(L482-512)————
  int SyncHash();

  /// Target.FromCell 的子格中心换算(Map.CenterOfSubCell 已接线 —— 全量 ctor
  /// 注入精确实现;测试 ctor 保持 square 公式缺省)
  sim::Target TargetFromCell(const CPos& cell, sim::SubCell sub_cell) const;
  void SetSubCellCenterResolver(CPosToWPos fn_resolve) {
    fn_subcell_center_ = std::move(fn_resolve);
  }

  /// ISync effect 的哈希函数解析器注入(Sync.Hash(effect) 的反射工厂面)
  void SetSyncEffectHashResolver(std::function<int(const ISync*)> fn_resolve) {
    fn_sync_effect_hash_ = std::move(fn_resolve);
  }

  /// Sync.Hash(effect) 的公开面(SyncReport 的效果哈希记账;未装配 = 0)
  /// The public face of Sync.Hash(effect) (SyncReport's effect-hash
  /// bookkeeping; unassembled = 0).
  int SyncEffectHash(const ISync* s) const {
    return fn_sync_effect_hash_ ? fn_sync_effect_hash_(s) : 0;
  }

  // ———— Map/ModData/OM 面(L139-151)————
  map::Map& Map() const {
    if (ptr_map_ == nullptr)
      throw std::runtime_error("NullReferenceException");
    return *ptr_map_;
  }
  map::Map* MapPtr() const { return ptr_map_; }
  game::ModData* ModDataFace() const { return ptr_mod_data_; }
  net::OrderManager* OM() const { return p_order_manager_; }
  void SetOrderManager(net::OrderManager* om) { p_order_manager_ = om; }
  /// IssueOrder(L151):经 sink 分发(上游单程序集内直调 OM;C++ 分层下
  /// ora_sim 不链接 ora_net —— 由嵌入侧/Game 注入 sink,COVERAGE 登记)
  /// IssueOrder (L151): dispatched through a sink (upstream calls OM
  /// directly inside one assembly; the layered C++ build keeps ora_sim off
  /// ora_net — the embedder/Game injects the sink; in COVERAGE).
  void SetOrderIssueSink(std::function<void(net::Order*)> fn) {
    fn_issue_order_ = std::move(fn);
  }
  void IssueOrder(net::Order* o);  // L151

  // ———— 战局/存档面(L74-88/114-118/201-207/260-310/393-397/539-563)————
  bool IsGameOver() const { return b_is_game_over_; }
  void EndGame();  // L75-88
  void OutOfSync();  // L201-207(EndGame + ReplayTimestep 永久暂停)
  void SetWorldOwner(Player* p);  // L312-315

  /// IsReplay(World.cs L114)的注入面(上游 = Connection is ReplayConnection;
  /// 第十一批起由 Game 装配解析)
  void SetIsReplayResolver(std::function<bool()> fn) {
    fn_is_replay_ = std::move(fn);
  }
  bool IsReplay() const { return fn_is_replay_ ? fn_is_replay_() : false; }

  bool IsLoadingGameSave() const;  // L116(以 OM 面直算)
  int GameSaveLoadingPercentage() const;  // L118

  void LoadComplete(gfx::WorldRenderer* wr);       // L260-298
  void PostLoadComplete(gfx::WorldRenderer* wr);   // L300-310

  /// gameSaveTraitData(L393-397) | gameSaveTraitData (L393-397).
  void AddGameSaveTraitData(int trait_index, yaml::MiniYaml yaml_value) {
    map_game_save_trait_data_[trait_index] = std::move(yaml_value);
  }

  /// OnClientDisconnected(L549-563)
  void OnClientDisconnected(int client_id);
  /// Phase 3 的代理名(兼容保留)| the Phase 3 proxy name (kept).
  void OnClientDisconnectedProxy(int client_id) {
    OnClientDisconnected(client_id);
  }

  /// PauseShellmap 的 Settings 值面(Phase 5 Settings 批换实体)
  /// The Settings value face of PauseShellmap (replaced by the real Settings
  /// in the Phase 5 Settings batch).
  void SetPauseShellmap(bool b) { b_pause_shellmap_ = b; }

  /// Game.Sound 面(StopAudio/StopVideo/DisableAllSounds/SoundVolumeModifier;
  /// 引擎嵌入侧注入,缺省 no-op)
  /// The Game.Sound faces (engine-embedder injection; no-ops by default).
  void SetSoundHooks(std::function<void()> fn_stop_audio,
                     std::function<void()> fn_stop_video,
                     std::function<void(bool)> fn_disable_all_sounds) {
    fn_sound_stop_audio_ = std::move(fn_stop_audio);
    fn_sound_stop_video_ = std::move(fn_stop_video);
    fn_sound_disable_all_sounds_ = std::move(fn_disable_all_sounds);
  }

  /// OnPlayerWinStateChanged(L539-547;gameInfo 面随 Phase 7)
  void OnPlayerWinStateChanged(Player& player);

  /// 生命周期标记(L587) | the lifetime flag (L587).
  bool Disposing() const { return b_disposing_; }

  /// Phase 3 的代理名(unit_orders 消费;EndGame 全量后语义一致)
  /// The Phase 3 proxy name (consumed by unit_orders; identical semantics
  /// after the full EndGame).
  bool IsGameOverProxy() const { return IsGameOver(); }

  /// Dispose(L589-619) | Dispose (L589-619).
  void Dispose();

  /// RunUnsynced 的 World 级便捷面(Selection 等消费) | the World-level
  /// RunUnsynced convenience (consumed by Selection etc.).
  template <class Fn>
  void RunUnsyncedGuard(Fn&& fn) {
    RunUnsynced(true, this, std::forward<Fn>(fn));
  }

  /// CancelInputMode(L172):经 DefaultOrderGenerator 注册表重造
  /// (defaultOrderGeneratorType.GetConstructor(World) 的注册表等价)
  /// CancelInputMode (L172): rebuilt through the DefaultOrderGenerator
  /// registry (the registry equivalent of the GetConstructor(World)
  /// reflection construct).
  void CancelInputMode();

  // ———— 第三批扩展(World.cs L234 的 RulesContainTemporaryBlocker +
  //      WorldUtils.cs L77-95 的 ContainsTemporaryBlocker)————
  // ———— The batch-3 extensions (World.cs L234's
  //      RulesContainTemporaryBlocker + WorldUtils.cs L77-95's
  //      ContainsTemporaryBlocker) ————

  /// World.cs L234:RulesContainTemporaryBlocker(ctor 内规则扫描;
  /// C++ 侧首查物化 —— 同一布尔,时点差异不可观测)
  /// World.cs L234: RulesContainTemporaryBlocker (the ctor's rules scan;
  /// materialized on first query here — the same boolean, the timing
  /// difference unobservable).
  bool RulesContainTemporaryBlocker();

  /// WorldUtils.cs L77-95:ContainsTemporaryBlocker(cell, ignoreActor)
  /// WorldUtils.cs L77-95: ContainsTemporaryBlocker(cell, ignoreActor).
  bool ContainsTemporaryBlocker(CPos cell, Actor* ignore_actor = nullptr);

  /// WorldUtils.cs L69-75:FindActorsInCircle(origin, r)(2D 距离)
  /// WorldUtils.cs L69-75: FindActorsInCircle(origin, r) (2D distance).
  std::vector<Actor*> FindActorsInCircle(const WPos& origin, const WDist& r);

 private:
  // C# internal(同程序集可见)的友元等价:Actor 构造调 NextAID
  friend class Actor;
  std::uint32_t NextAID() { return uint4_next_aid_++; }  // L476-480

  TraitDictionary trait_dict_;
  std::map<std::uint32_t, Actor*> map_actors_;  // SortedDictionary 等价

  // ActorAdded/ActorRemoved 的订阅表(上游 event 多播的回调承载)
  // The ActorAdded/ActorRemoved subscription tables (the callback carrier
  // of upstream's event multicast).
  std::vector<std::function<void(Actor&)>> vec_actor_added_handlers_;
  std::vector<std::function<void(Actor&)>> vec_actor_removed_handlers_;

  // RulesContainTemporaryBlocker 的首查物化(上游 ctor 一次性扫描)
  // The first-query materialization of RulesContainTemporaryBlocker
  // (upstream's one-shot ctor scan).
  bool b_rules_temporary_blocker_cached_ = false;
  bool b_rules_contain_temporary_blocker_ = false;

  ora::WorldArena arena_{64 * 1024};  // §4.5 每局世界区(D26/D27)
  std::vector<std::unique_ptr<Actor>> vec_owned_actors_;  // 测试路径所有权

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
  std::vector<std::unique_ptr<Player>> vec_owned_players_;  // 玩家所有权
                                                            // the player
                                                            // ownership.
  Player* p_local_player_ = nullptr;
  Player* p_render_player_ = nullptr;
  Actor* p_world_actor_ = nullptr;
  net::OrderManager* p_order_manager_ = nullptr;
  std::function<void(net::Order*)> fn_issue_order_;
  TraitFactory fn_trait_factory_;
  CPosToWPos fn_subcell_center_;
  std::function<int(const ISync*)> fn_sync_effect_hash_;

  ScreenMap* ptr_screen_map_ = nullptr;      // L142
  IActorMap* ptr_actor_map_ = nullptr;       // L140
  IControlGroups* ptr_control_groups_ = nullptr;  // L169
  ISelection* ptr_selection_ = nullptr;      // L166
  IOrderGenerator* ptr_order_generator_ = nullptr;  // L155
  std::unique_ptr<IOrderGenerator> owned_order_generator_;  // 注册表产物的
                                                            // 所有权面(the
                                                            // registry
                                                            // product's
                                                            // ownership)
  std::string str_default_order_generator_;           // L153(defaultOrderGeneratorType)
  std::vector<IValidateOrder*> vec_order_validators_;   // L145
  std::vector<INotifyPlayerDisconnected*> vec_notify_disconnected_;  // L146

  bool b_is_game_over_ = false;                          // L74
  bool b_was_loading_game_save_ = false;                 // L176
  std::map<int, yaml::MiniYaml> map_game_save_trait_data_;  // L393
  bool b_pause_shellmap_ = false;                        // PauseShellmap 值面

  std::function<bool()> fn_is_replay_;
  std::function<bool()> fn_is_host_;
  int int4_local_client_id_ = 0;  // Game.LocalClientId 注入面
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

// ———— RunUnsynced(Sync.cs L183-204;模板体需 World 完整类型)————
template <class Fn>
auto RunUnsynced(bool check_sync_hash, World* world, Fn&& fn)
    -> decltype(fn()) {
  auto& count = sim::UnsyncCountRef();
  ++count;

  // Detect sync changes in top level entry point only. Do not recalculate
  // sync hash during reentry.
  const int sync =
      count == 1 && check_sync_hash && world != nullptr ? world->SyncHash()
                                                         : 0;

  // Running this inside a scope-guard means the count decrements as soon
  // as fn completes (the finally semantics).
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

// ———— Actor::Trait 查询转发定义(Actor.cs L399-417)————
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

}  // namespace ora::sim
