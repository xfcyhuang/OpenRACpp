// UPSTREAM: OpenRA.Game/World.cs @b6fc03f L28-650(仿真核心逐语义重写;
//          Map/ModData/GameSpeed/ScreenMap/Selection/OrderGenerator 完整
//          构造链 Phase 5 随 Game 落地,本期以注入面承载 —— 机制面 [tick 序/
//          帧末任务/SyncHash/actors 有序遍历/effects] 完整移植)
//          Verbatim-semantics rewrite of the sim core; the full
//          Map/ModData/GameSpeed/ScreenMap/Selection/OrderGenerator
//          construction chain lands with Game in Phase 5, carried here by
//          injection surfaces — the mechanics (tick order / frame-end
//          tasks / SyncHash / ordered actor iteration / effects) are fully
//          ported.
//
// 机制对照 / Mechanism mapping:
//  - actors: SortedDictionary<uint, Actor> → std::map<uint32_t, Actor*>
//    (遍历序 = ActorID 升序,确定性关键);actor 对象所有权在
//    vec_owned_actors_(创建序;Dispose 逆序销毁语义见 Dispose)
//    actors: SortedDictionary<uint, Actor> → std::map<uint32_t, Actor*>
//    (iteration order = ascending ActorID, determinism-critical); actor
//    object ownership sits in vec_owned_actors_ (creation order; the
//    dispose-newest-first semantics live in Dispose).
//  - effects 三列表(effects/unpartitionedEffects/syncedEffects)→ 三平行
//    视图 + 单一所有权表(Remove/RemoveAll 同步维护;ISync 收集保 SyncHash)
//    The three effect lists → three parallel views over one ownership
//    table (Remove/RemoveAll maintain all; the ISync collection feeds
//    SyncHash).
//  - frameEndActions: Queue<Action<World>> → std::queue<std::function<
//    void(World&)>>(while drain,任务可再入队 —— C# 语义)
//  - trait 对象所有权:计划 §4.5 为 World arena(Phase 0 遗留项);本期以
//    World 级 unique_ptr 表承载,actor Dispose 只摘字典引用 —— COVERAGE 登记
//    Trait object ownership: per plan §4.5 a World arena (a Phase 0
//    leftover); carried meanwhile by a World-level unique_ptr table —
//    actor Dispose only drops dictionary references — in COVERAGE.
//  - SyncHash(L482-512):公式逐项(n 计数器跨段连续;乘法回绕 -fwrapv)
//    SyncHash (L482-512): the formula term by term (the n counter is
//    continuous across segments; multiplication wraps under -fwrapv).
#pragma once
import std;

#include "core/mersenne_twister.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/effects.hpp"
#include "sim/player.hpp"
#include "sim/target.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::net {
class OrderManager;
struct Order;
}  // namespace ora::net

namespace ora::sim {

/// WorldType(World.cs L28)
enum class WorldType { Regular, Shellmap, Editor };

/// World 仿真核心构造参数(上游构造依赖 ModData/LobbyInfo/Map;Phase 5 全量
/// 接线前的注入面)
/// The sim-core construction parameters (upstream construction needs
/// ModData/LobbyInfo/Map; the injection surface before the full Phase 5
/// wiring).
struct WorldSimParams {
  int int4_random_seed = 0;        // LobbyInfo.GlobalSettings.RandomSeed
  int int4_timestep = 40;          // GameSpeed.Timestep(默认 40ms)
  WorldType type = WorldType::Regular;
};

/// 子格中心换算器签名(Target.FromCell 的注入面)
/// The sub-cell center converter signature (the Target.FromCell
/// injection surface).
using CPosToWPos = std::function<WPos(const CPos&, sim::SubCell)>;

/// IEffect 视图(World.cs L34-36 的三列表)
/// The IEffect views (the three lists of World.cs L34-36).
class World final {
 public:
  /// 上游 internal 构造(L178-235)的仿真核心子集(见头注)
  explicit World(WorldSimParams params = {});
  ~World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

  // ———— 玩家面(L53-135;完整 ICreatePlayers 链 Phase 5)————
  const std::vector<Player*>& Players() const { return vec_players_; }
  void SetPlayers(std::vector<Player*> players, Player* local_player);
  Player* LocalPlayer() const { return p_local_player_; }
  Player* RenderPlayer() const { return p_render_player_; }

  // ———— 世界 actor(L137/202-206:SystemActors.World 命名 actor)————
  Actor* WorldActor() const { return p_world_actor_; }
  void SetWorldActor(Actor* a) { p_world_actor_ = a; }

  // ———— trait 工厂注入面(上游 traitInfo.Create;Phase 5 接 game 加载链)————
  /// 工厂协议:name(原样)→ 返回构造序 trait 列表;未注册规则时抛
  /// "No rules definition for unit X"(逐字);name 小写化在此闭合
  /// Factory protocol: the (raw) name → the construct-order trait list;
  /// throw "No rules definition for unit X" (verbatim) for unknown rules;
  /// name lower-casing closes here.
  using TraitFactory = std::function<std::vector<std::unique_ptr<TraitBase>>(
      const std::string& str_name)>;
  void SetTraitFactory(TraitFactory fn) { fn_trait_factory_ = std::move(fn); }

  /// Actor 构造的规则面入口(name 非空时调用;内部走工厂 + AddTrait + 接口
  /// 缓存收集,Actor.cs L169-195 循环的 World 侧半边)
  /// The rules-face entry of Actor construction (called for non-empty
  /// names; runs the factory + AddTrait + interface-cache collection —
  /// the World-side half of the Actor.cs L169-195 loop).
  void CreateTraitsForActor(Actor& actor, ActorInitializer& init,
                            const std::string& str_name);

  // ———— actor 生命周期(L317-352)————
  Actor* CreateActor(const std::string& str_name, TypeDictionary& init_dict);
  Actor* CreateActor(bool add_to_world, const std::string& str_name,
                     TypeDictionary& init_dict);
  void Add(Actor* a);
  void Remove(Actor* a);

  /// 返回所有权(handle 由 World 持有;Phase 3 = new,Phase 5 = arena)
  /// Returns ownership (held by the World; Phase 3 = new, Phase 5 = arena).
  Actor* AdoptActor(std::unique_ptr<Actor> a);

  // ———— effects(L354-381)————
  void Add(std::unique_ptr<IEffect> e);
  void Remove(IEffect* e);
  void RemoveAll(const std::function<bool(IEffect*)>& predicate);

  // ———— 帧末任务队列(L383)————
  void AddFrameEndTask(std::function<void(World&)> a) {
    queue_frame_end_actions_.push(std::move(a));
  }

  // ———— tick 核心(L388-455)————
  bool Paused() const { return b_paused_; }
  void SetPaused(bool b) { b_paused_ = b; }
  bool PredictedPaused() const { return b_predicted_paused_; }
  void SetPredictedPaused(bool b) { b_predicted_paused_ = b; }
  void SetLocalPauseState(bool paused) {  // L409-411
    b_paused_ = b_predicted_paused_ = paused;
  }

  int WorldTick() const { return int4_world_tick_; }
  int Timestep() const { return int4_timestep_; }
  int ReplayTimestep() const { return int4_replay_timestep_; }

  WorldType Type() const { return type_; }

  MersenneTwister& SharedRandom() { return mt_shared_; }
  MersenneTwister& LocalRandom() { return mt_local_; }

  void Tick();       // L413-455
  void TickRender(); // L458-462(ITickRender 分发;渲染参数 Phase 4,本期空)

  // ———— 查询面(L464-537)————
  std::vector<Actor*> Actors() const;
  const std::vector<IEffect*>& Effects() const { return vec_effects_; }
  const std::vector<IEffect*>& UnpartitionedEffects() const {
    return vec_unpartitioned_effects_;
  }
  const std::vector<ISync*>& SyncedEffects() const {
    return vec_synced_effects_;
  }  Actor* GetActorById(std::uint32_t actor_id);  // L469-474

  /// TraitDictionary 直取(Actor 查询转发用)
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

  /// Target.FromCell 的子格中心换算(Order 反序列化 TargetIsCell 分支;
  /// Map.CenterOfSubCell 依赖 Phase 5 —— 默认 square 网格公式
  /// (cell*1024+512),可注入精确实现 —— COVERAGE 登记)
  /// The sub-cell center conversion of Target.FromCell (the
  /// TargetIsCell branch of order deserialization; Map.CenterOfSubCell is
  /// a Phase 5 dependency — defaults to the square-grid formula
  /// (cell*1024+512), injectable with the precise implementation — in
  /// COVERAGE).
  sim::Target TargetFromCell(const CPos& cell, sim::SubCell sub_cell) const;
  void SetSubCellCenterResolver(CPosToWPos fn_resolve) {
    fn_subcell_center_ = std::move(fn_resolve);
  }

  /// ISync effect 的哈希函数解析器注入(Sync.Hash(effect) 的反射工厂面;
  /// Phase 5 手写 effect 类按 gen/sync_gen 表注册;未注入时该段贡献 0 ——
  /// COVERAGE 登记)
  /// The hash-function resolver injection for ISync effects (the
  /// reflection-factory face of Sync.Hash(effect); Phase 5 hand-written
  /// effect classes register against the gen/sync_gen table; without an
  /// injection the segment contributes 0 — registered in COVERAGE).
  void SetSyncEffectHashResolver(
      std::function<int(const ISync*)> fn_resolve) {
    fn_sync_effect_hash_ = std::move(fn_resolve);
  }

  // ———— OrderManager 面(L151;net 侧接线)————
  net::OrderManager* OM() const { return p_order_manager_; }
  void SetOrderManager(net::OrderManager* om) { p_order_manager_ = om; }
  void IssueOrder(net::Order* o);

  /// OnClientDisconnected(L549-563)的仿真核心代理:玩家断连通知链
  /// (notifyDisconnected trait 族,Phase 5 WorldActor 接线;默认 no-op)
  /// The sim-core proxy of OnClientDisconnected (L549-563): the
  /// player-disconnect notification chain (the notifyDisconnected trait
  /// family, wired with the Phase 5 WorldActor; no-op by default).
  void OnClientDisconnectedProxy(int client_id);

  /// IsGameOver(L74)的 Phase 3 桩(EndGame/IGameOver 链 Phase 5;恒 false)
  /// The Phase 3 stub of IsGameOver (L74; the EndGame/IGameOver chain is
  /// Phase 5; always false).
  bool IsGameOverProxy() const { return false; }

  /// 生命周期标记(L587)
  bool Disposing() const { return b_disposing_; }

 private:
  // C# internal(同程序集可见)的友元等价:Actor 构造调 NextAID
  // The friend equivalent of C# internal visibility: Actor's
  //  constructor calls NextAID.
  friend class Actor;
  std::uint32_t NextAID() { return uint4_next_aid_++; }  // L476-480

  TraitDictionary trait_dict_;
  std::map<std::uint32_t, Actor*> map_actors_;  // SortedDictionary 等价
  std::vector<std::unique_ptr<Actor>> vec_owned_actors_;  // 创建序所有权
  std::vector<std::unique_ptr<TraitBase>> vec_owned_traits_;  // 见头注

  std::vector<std::unique_ptr<IEffect>> vec_owned_effects_;
  std::vector<IEffect*> vec_effects_;
  std::vector<IEffect*> vec_unpartitioned_effects_;
  std::vector<ISync*> vec_synced_effects_;

  std::queue<std::function<void(World&)>> queue_frame_end_actions_;

  int int4_timestep_;
  int int4_replay_timestep_;
  MersenneTwister mt_shared_;
  MersenneTwister mt_local_;

  std::vector<Player*> vec_players_;
  Player* p_local_player_ = nullptr;
  Player* p_render_player_ = nullptr;
  Actor* p_world_actor_ = nullptr;
  net::OrderManager* p_order_manager_ = nullptr;
  TraitFactory fn_trait_factory_;
  CPosToWPos fn_subcell_center_;
  std::function<int(const ISync*)> fn_sync_effect_hash_;

  WorldType type_;
  bool b_paused_ = false;
  bool b_predicted_paused_ = false;
  int int4_world_tick_ = 0;
  std::uint32_t uint4_next_aid_ = 0;
  bool b_disposing_ = false;
};

// ———— RunUnsynced(Sync.cs L183-204;模板体需 World 完整类型)————
// ———— RunUnsynced (Sync.cs L183-204; the template body needs the
//      complete World type) ————

/// 非同步代码门禁:fn 内世界哈希不得变化(顶层入口校验;嵌套调用不重算;
/// disposing 世界跳过 —— 上游 null/Disposing 语义)
/// The unsynced-code gate: the world hash may not change inside fn
/// (checked at the top-level entry; nested calls do not recheck;
/// disposing worlds skip — the upstream null/Disposing semantics).
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

  // 上游 Action(void) 面:void 可调用体不携带返回值
  // upstream's Action (void) face: void callables carry no return value
  if constexpr (std::is_void_v<std::invoke_result_t<Fn&>>) {
    fn();
  } else {
    auto result = fn();

    // When the world is disposing all actors and effects have been removed
    // So do not check the hash for a disposing world since it definitively
    // has changed
    if (count == 1 && check_sync_hash && world != nullptr &&
        !world->Disposing() && sync != world->SyncHash())
      throw std::runtime_error("RunUnsynced: sync-changing code may not run here");

    return result;
  }

  // When the world is disposing all actors and effects have been removed
  // So do not check the hash for a disposing world since it definitively
  // has changed
  if (count == 1 && check_sync_hash && world != nullptr &&
      !world->Disposing() && sync != world->SyncHash())
    throw std::runtime_error("RunUnsynced: sync-changing code may not run here");
}


// ———— Actor::Trait 查询转发定义(Actor.cs L399-417;函数体在此处才有
//      World 完整类型)————
// ———— The Actor::Trait forwarding definitions (Actor.cs L399-417; the
//      bodies only see the complete World type here) ————
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
