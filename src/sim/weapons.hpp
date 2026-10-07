// UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @b6fc03f L21-72(ProjectileArgs/
//          WarheadArgs/IProjectile/IProjectileInfo 的承载面)+
//          L178-268 的仿真面方法(IsValidTarget/IsValidAgainst/Impact)+
//          OpenRA.Mods.Common/Warheads/Warhead.cs L22-106(Warhead 基类)+
//          OpenRA.Game/Effects/DelayedImpact.cs L15-43
//          The carrier faces of WeaponInfo.cs L21-72 (ProjectileArgs/
//          WarheadArgs/IProjectile/IProjectileInfo) + the sim-domain
//          methods of L178-268 (IsValidTarget/IsValidAgainst/Impact) +
//          Warheads/Warhead.cs L22-106 (the Warhead base) +
//          Effects/DelayedImpact.cs L15-43.
//
// 机制对照 / Mechanism mapping:
//  - 上游 WeaponInfo 是引擎类,加载链字段已在 game/game_records.hpp 的值袋
//    (ora::game::WeaponInfo);仿真面方法在此以自由函数承载(首参 = 值袋)
//    —— 值袋不可加成员,分层即"加载面在 game、仿真面在 sim"
//    Upstream's WeaponInfo is an engine class whose loading-chain fields
//    already live in the value bag at game/game_records.hpp
//    (ora::game::WeaponInfo); the sim-domain methods ride free functions
//    here (first parameter = the bag) — the bag cannot grow members, so
//    the layering is "loading face in game, sim face in sim".
//  - IProjectileInfo/IWarhead:上游 Game.CreateObject("{值}Info"/"{值}
//    Warhead") 工厂;C++ = 值袋记录 + 名字注册表(记录名 → 工厂 → 构造)。
//    本批零注册(Bullet/Missile/DamageWarhead 等随武器批)—— 未注册 →
//    nullptr(上游 ObjectCreator 未注册类型同为 null;部分覆盖装配面,
//    COVERAGE 登记)。解析产物为无状态数据对象,进程级缓存按记录指针
//    一记录一构造(确定性;非 world 生命周期 —— 与上游 WeaponInfo 的
//    ruleset 生命周期同阶)
//    IProjectileInfo/IWarhead: upstream's Game.CreateObject("{value}Info"/
//    "{value}Warhead") factories; C++ = the value-bag record + the
//    name-keyed registry (record name → factory → construct). Zero
//    registrations this batch (Bullet/Missile/DamageWarhead & co. arrive
//    with the weapons batch) — unregistered → nullptr (upstream's
//    ObjectCreator is likewise null for unregistered types; the
//    partial-coverage assembly is registered in COVERAGE). The resolved
//    products are stateless data objects with a process-level
//    one-record-one-construct cache keyed by the record pointer
//    (deterministic; not world lifetime — the same order as upstream's
//    ruleset-lifetime WeaponInfo).
//  - WarheadArgs 的 int[] 快照 → std::vector<int> 拷贝(上游 FireBarrel 每
//    发 ToArray() 快照,引用别名不可观测;OPT-A9 的零分配落在 CheckFire
//    热循环的修正链收集(armament.hpp),延迟弹着面保持快照形态)
//    WarheadArgs's int[] snapshot → a std::vector<int> copy (upstream's
//    FireBarrel snapshots with ToArray() per shot, so the array aliasing
//    is unobservable; OPT-A9's zero allocation lands in the CheckFire
//    hot-loop modifier collection (armament.hpp), while the delayed-impact
//    face keeps the snapshot shape).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "meta/generic_record.hpp"
#include "sim/effects.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class WeaponInfo;
}

namespace ora::sim {

class Actor;
class World;

class WarheadArgs;

/// WeaponInfo.cs L178-181:IsValidTarget(BitSet)
/// WeaponInfo.cs L178-181: IsValidTarget(BitSet).
bool WeaponIsValidTarget(const game::WeaponInfo& weapon,
                         const core::BitSet<TargetableType>& target_types);

/// WeaponInfo.cs L184-207:IsValidAgainst(in Target, World, Actor)
/// WeaponInfo.cs L184-207: IsValidAgainst(in Target, World, Actor).
bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, const Target& target,
                          World& world, Actor* fired_by);

/// WeaponInfo.cs L210-218:IsValidAgainst(Actor, Actor)
/// WeaponInfo.cs L210-218: IsValidAgainst(Actor, Actor).
bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, Actor& victim,
                          Actor* fired_by);

/// WeaponInfo.cs L221-230:Impact(in Target, WarheadArgs)—— 逐 warhead:
/// Delay>0 → 帧末 DelayedImpact 效果;否则立即 DoImpact
/// WeaponInfo.cs L221-230: Impact(in Target, WarheadArgs) — per warhead:
/// Delay>0 → the frame-end DelayedImpact effect; otherwise DoImpact now.
void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  WarheadArgs& args);

/// WeaponInfo.cs L233-250:Impact(in Target, Actor)(无弹丸特例面)
/// WeaponInfo.cs L233-250: Impact(in Target, Actor) (the projectile-less
/// special-case face).
void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  Actor* fired_by);

// ———— WeaponInfo.cs L22-34:ProjectileArgs ————
// ———— WeaponInfo.cs L22-34: ProjectileArgs ————

struct ProjectileArgs {
  const game::WeaponInfo* weapon = nullptr;
  std::vector<int> vec_damage_modifiers;
  std::vector<int> vec_inaccuracy_modifiers;
  std::vector<int> vec_range_modifiers;
  WAngle facing;
  std::function<WAngle()> fn_current_muzzle_facing;
  WPos source;
  std::function<WPos()> fn_current_source;
  Actor* source_actor = nullptr;
  WPos passive_target;
  Target guided_target;
};

// ———— WeaponInfo.cs L36-62:WarheadArgs ————
// ———— WeaponInfo.cs L36-62: WarheadArgs ————

class WarheadArgs {
 public:
  /// L44-52:WarheadArgs(ProjectileArgs)—— 弹着位 = PassiveTarget
  /// L44-52: WarheadArgs(ProjectileArgs) — the impact position is the
  /// passive target.
  explicit WarheadArgs(const ProjectileArgs& args);

  /// L55-61:拷贝构造(上游"只改部分字段"的构造即拷贝语义)
  /// L55-61: the copy constructor (upstream's "update only some fields"
  /// constructor is exactly copy semantics).
  WarheadArgs(const WarheadArgs& args) = default;
  WarheadArgs& operator=(const WarheadArgs& args) = default;

  /// L64:默认空构造 | L64: the default empty constructor.
  WarheadArgs() = default;

  const game::WeaponInfo* weapon = nullptr;
  std::vector<int> vec_damage_modifiers;
  bool b_has_source = false;  // WPos? Source 的有值标记 | the has-value flag of WPos? Source
  WPos source{};
  WRot impact_orientation = WRot::None();
  WPos impact_position;
  Actor* source_actor = nullptr;
  Target weapon_target;
};

// ———— WeaponInfo.cs L71-72:IProjectile/IProjectileInfo ————
// ———— WeaponInfo.cs L71-72: IProjectile/IProjectileInfo ————

/// IProjectile : IEffect(弹丸效果;World.Add 入效果表)
/// IProjectile : IEffect (the projectile effect; World.Add enters the
/// effect list).
class IProjectile : public IEffect {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_GameRules_IProjectile;
};

/// IProjectileInfo:值袋记录 → Create(args)(工厂经 ProjectileRegistry)
/// IProjectileInfo: the value-bag record → Create(args) (the factory via
/// the ProjectileRegistry).
class IProjectileInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_GameRules_IProjectileInfo;
  virtual ~IProjectileInfo() = default;
  virtual IProjectile* Create(ProjectileArgs& args) = 0;
};

/// 弹丸工厂注册表:记录类型名(如 "BulletInfo")→ IProjectileInfo 构造
/// (TraitRegistry 同形;未注册 → nullptr)
/// The projectile-factory registry: the record type name (e.g.
/// "BulletInfo") → the IProjectileInfo construct (the TraitRegistry
/// shape; unregistered → nullptr).
class ProjectileRegistry {
 public:
  static ProjectileRegistry& Instance();

  using CreateFn = std::function<IProjectileInfo*(
      const meta::RecordObject& rec_info)>;

  void Register(std::string str_name, CreateFn fn_create);

  /// 未注册名 → nullptr(部分覆盖装配面;上游 ObjectCreator 未注册类型
  /// 的 null 路径等价 —— COVERAGE 登记)
  /// An unregistered name → nullptr (the partial-coverage assembly face;
  /// the equivalent of upstream's null path for unregistered ObjectCreator
  /// types — registered in COVERAGE).
  IProjectileInfo* Create(const std::string& str_name,
                          const meta::RecordObject& rec_info);

 private:
  std::map<std::string, CreateFn> map_creators_;
};

/// WeaponInfo 加载链的 Projectile 值袋(game_records.hpp 的 rec_projectile)
/// → 运行时 IProjectileInfo(注册表解析;无 Projectile 键/无注册 → nullptr;
/// 按记录指针缓存)
/// The loading chain's Projectile value bag (game_records.hpp's
/// rec_projectile) → the runtime IProjectileInfo (registry-resolved; no
/// Projectile key / no registration → nullptr; cached by record pointer).
IProjectileInfo* ResolveProjectileInfo(const meta::RecordObject& rec_projectile);

}  // namespace ora::sim

namespace ora::mods {

class IWarhead;

/// IWarhead(OpenRA.Traits;WeaponInfo.Warheads 元素协议)。具体 warhead
/// 继承 mods::Warhead 基类
/// IWarhead (OpenRA.Traits; the protocol of WeaponInfo.Warheads elements).
/// Concrete warheads derive the mods::Warhead base.
class IWarhead {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IWarhead;
  virtual ~IWarhead() = default;
  virtual int Delay() const = 0;
  virtual void DoImpact(const sim::Target& target,
                        sim::WarheadArgs& args) = 0;
};

/// Warhead 基类(Warheads/Warhead.cs L22-106 全文:七字段 + IsValidTarget/
/// IsValidAgainst(Actor)/(FrozenActor 面)/DoImpact 纯虚)
/// The Warhead base (the whole of Warheads/Warhead.cs L22-106: the seven
/// fields + IsValidTarget/IsValidAgainst(Actor)/(the FrozenActor face)/
/// the DoImpact pure virtual).
class Warhead : public IWarhead {
 public:
  core::BitSet<sim::TargetableType> valid_types;     // L26 ValidTargets
  core::BitSet<sim::TargetableType> invalid_types;   // L29 InvalidTargets
  sim::PlayerRelationship valid_relationships =      // L32
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  bool affects_parent = false;                        // L35
  WDist air_threshold{128};                           // L41
  int delay = 0;                                      // L44

  int Delay() const override { return delay; }        // L46(IWarhead.Delay)

  /// L52-54:protected IsValidTarget → 公开(子类 DoImpact 消费)
  /// L52-54: the protected IsValidTarget → public (consumed by the
  /// subclasses' DoImpact).
  bool IsValidTarget(
      const core::BitSet<sim::TargetableType>& target_types) const {
    return valid_types.Overlaps(target_types) &&
           !invalid_types.Overlaps(target_types);
  }

  /// L59-79:IsValidAgainst(Actor, Actor)
  /// L59-79: IsValidAgainst(Actor, Actor).
  virtual bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by);

  /// L82-96:IsValidAgainst(FrozenActor, Actor)—— FrozenActor 面随 Shroud 批
  /// (本批恒假分支;登记 COVERAGE)
  /// L82-96: IsValidAgainst(FrozenActor, Actor) — the FrozenActor face rides
  /// the Shroud batch (this batch keeps the always-false branch;
  /// registered in COVERAGE).
  static bool IsValidAgainstFrozen(const sim::Actor& victim,
                                   const sim::Actor* fired_by);

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override = 0;  // L99
};

/// Warhead 工厂注册表("{值}Warhead" 名 → 值袋 → 构造;未注册 → nullptr)
/// The warhead-factory registry (the "{value}Warhead" name → the bag →
/// the construct; unregistered → nullptr).
class WarheadRegistry {
 public:
  static WarheadRegistry& Instance();

  using CreateFn =
      std::function<IWarhead*(const meta::RecordObject& rec_info)>;

  void Register(std::string str_name, CreateFn fn_create);

  IWarhead* Create(const std::string& str_name,
                   const meta::RecordObject& rec_info);

 private:
  std::map<std::string, CreateFn> map_creators_;
};

/// WeaponInfo 加载链的 Warheads 值袋列表 → 运行时 IWarhead 列表(按注册表
/// 解析;未注册条目跳过 —— 上游 CreateObject null 后 continue 的等价面;
/// 按记录指针缓存)
/// The loading chain's Warheads value-bag list → the runtime IWarhead list
/// (registry-resolved; unregistered entries skip — the equivalent of
/// upstream's CreateObject-null-then-continue; cached by record pointer).
std::vector<IWarhead*> ResolveWarheads(
    const std::vector<std::shared_ptr<meta::RecordObject>>& vec_rec_warheads);

}  // namespace ora::mods
