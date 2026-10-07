// UPSTREAM: OpenRA.Mods.Common/Warheads/DamageWarhead.cs @b6fc03f L20-94 +
//          SpreadDamageWarhead.cs L20-143(全文逐语义重写;DebugVisualizations
//          的 WarheadDebugOverlay 面随 Phase 6)
//          The whole of DamageWarhead.cs L20-94 + SpreadDamageWarhead.cs
//          L20-143 (verbatim-semantics rewrites; the DebugVisualizations/
//          WarheadDebugOverlay faces land with Phase 6).
//
// 机制对照 / Mechanism mapping:
//  - 上游 abstract DamageWarhead 继承链(Warhead 基类已在 sim/weapons.hpp)
//    → C++ DamageWarhead 中间基类 + SpreadDamageWarhead 具体类;基字段
//    (Warhead 七字段)由 sim::Warhead 承载,值袋解析经 CollectFields 的
//    base 链扁平序
//    Upstream's abstract DamageWarhead inheritance chain (the Warhead base
//    already lives in sim/weapons.hpp) → the C++ DamageWarhead intermediate
//    base + the SpreadDamageWarhead concrete class; the base fields (the
//    Warhead seven) ride sim::Warhead, with the value-bag parse going
//    through CollectFields' flattened base-chain order.
//  - Versus 的 FrozenDictionary<string, int> → 插入序 vector<pair>(键查
//    找线性;规模 = 装甲类型数)
//    Versus's FrozenDictionary<string, int> → an insertion-ordered
//    vector<pair> (linear key lookup; the scale = the armor-type count).
//  - IRulesetLoaded<WeaponInfo>.RulesetLoaded 的校验 → 工厂解析时点
//    (异常文本逐字;同 D 系时点偏离)
//    The IRulesetLoaded<WeaponInfo>.RulesetLoaded validation → the
//    factory-parse timing (exception texts verbatim; the same D-series
//    timing deviation).
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/hit_shape.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;

/// SpreadDamageWarhead.cs L20:DamageCalculationType(gen 枚举同步)
/// SpreadDamageWarhead.cs L20: DamageCalculationType (mirroring the gen
/// enum).
enum class DamageCalculationType : std::int32_t {
  HitShape = 0,
  ClosestTargetablePosition = 1,
  CenterPosition = 2,
};

/// DamageWarhead(DamageWarhead.cs L21-94;抽象中间基)
/// DamageWarhead (DamageWarhead.cs L21-94; the abstract intermediate
/// base).
class DamageWarhead : public Warhead {
 public:
  int Damage = 0;  // L24
  core::BitSet<sim::DamageType> bitset_damage_types;  // L27
  std::vector<std::pair<std::string, int>> vec_versus;  // L30(插入序)

  /// L32-39:IsValidAgainst 覆写(无 IHealthInfo 者拒)
  /// L32-39: the IsValidAgainst override (victims without IHealthInfo are
  /// rejected).
  bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by) override;

  /// L41-70:DoImpact(in Target, WarheadArgs)
  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;

 protected:
  /// L72-84:DamageVersus(virtual;armor 链 × Versus 表)
  /// L72-84: DamageVersus (virtual; the armor chain × the Versus table).
  virtual int DamageVersus(sim::Actor& victim, HitShape& shape,
                           const sim::WarheadArgs& args);

  /// L86-90:InflictDamage(virtual)
  /// L86-90: InflictDamage (virtual).
  virtual void InflictDamage(sim::Actor& victim, sim::Actor* fired_by,
                             HitShape& shape, sim::WarheadArgs& args);

  /// L92:DoImpact(pos, firedBy, args)(纯虚)
  /// L92: DoImpact (pos, firedBy, args) (pure virtual).
  virtual void DoImpact(const WPos& pos, sim::Actor* fired_by,
                        sim::WarheadArgs& args) = 0;

  /// DamageWarhead 基字段解析(Damage/DamageTypes/Versus;调用方先解析
  /// Warhead 七字段)
  /// The DamageWarhead base-field parse (Damage/DamageTypes/Versus; the
  /// caller parses the Warhead seven first).
  void ParseDamageFields(const meta::RecordObject& rec_info);
};

/// SpreadDamageWarhead(SpreadDamageWarhead.cs L24-142)
class SpreadDamageWarhead final : public DamageWarhead {
 public:
  WDist Spread{43};  // L26
  std::vector<int> vec_falloff{100, 37, 14, 5, 0};  // L29
  std::vector<WDist> vec_range;  // L32(Range;空 = 未定义)
  DamageCalculationType damage_calculation_type =
      DamageCalculationType::HitShape;  // L35

  std::vector<WDist> vec_effective_range;  // L37(effectiveRange)

  /// 工厂解析(RulesetLoaded L39-54 的校验时点前移;异常文本逐字)
  /// The factory parse (RulesetLoaded L39-54's validation moved to the
  /// front; exception texts verbatim).
  static std::unique_ptr<SpreadDamageWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const WPos& pos, sim::Actor* fired_by,
                sim::WarheadArgs& args) override;  // L56-126

 private:
  /// L128-141:GetDamageFalloff | L128-141: GetDamageFalloff.
  int GetDamageFalloff(int distance) const;
};

/// TargetDamageWarhead(TargetDamageWarhead.cs L15-66;DamageWarhead 子类)
/// TargetDamageWarhead (TargetDamageWarhead.cs L15-66; the DamageWarhead
/// subclass).
class TargetDamageWarhead final : public DamageWarhead {
 public:
  WDist Spread{0};  // L19

  /// 工厂解析(基字段链先于自有序)
  /// The factory parse (the base field chain precedes the own order).
  static std::unique_ptr<TargetDamageWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const WPos& pos, sim::Actor* fired_by,
                sim::WarheadArgs& args) override;  // L21-64
};

/// CreateEffectWarhead(CreateEffectWarhead.cs L17-149)
class CreateEffectWarhead final : public Warhead {
 public:
  std::vector<std::string> vec_explosions;  // L21 Explosions
  std::string str_image{"explosion"};       // L24 Image
  std::string str_explosion_palette{"effect"};  // L27
  bool b_use_player_palette = false;            // L30
  bool b_force_display_at_ground_level = false;  // L33
  std::vector<std::string> vec_impact_sounds;   // L36
  int int4_impact_sound_chance = 100;           // L39
  bool b_impact_actors = true;                  // L42
  WDist dist_inaccuracy{0};                     // L45

  static std::unique_ptr<CreateEffectWarhead> Parse(
      const meta::RecordObject& rec_info);

  /// L96-101:关系 + 目标类型(覆写:不带 DamageWarhead 的 IHealth 前置)
  /// L96-101: relationships + target types (the override: without
  /// DamageWarhead's IHealth precondition).
  bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by) override;

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;  // L103-137

 private:
  /// 上游 L49 的 ImpactActorType(gen 枚举 OpenRA.Mods.Common.Traits.
  /// ImpactActorType:{None=0, Invalid=1, Valid=2})
  /// Upstream's L49 ImpactActorType (the gen enum
  /// OpenRA.Mods.Common.Traits.ImpactActorType: {None=0, Invalid=1,
  /// Valid=2}).
  enum class ImpactActorType : std::int32_t {
    None = 0,
    Invalid = 1,
    Valid = 2,
  };

  /// L51-70:ActorTypeAtImpact
  /// L51-70: ActorTypeAtImpact.
  ImpactActorType ActorTypeAtImpact(sim::World& world, const WPos& pos,
                                    sim::Actor* fired_by);

  /// L140-149:IsValidAgainstTerrain
  /// L140-149: IsValidAgainstTerrain.
  bool IsValidAgainstTerrain(sim::World& world, const WPos& pos);
};

/// LeaveSmudgeWarhead(LeaveSmudgeWarhead.cs L16-75)
class LeaveSmudgeWarhead final : public Warhead {
 public:
  std::vector<int> vec_size{0, 0};     // L20 Size(双值 = 环形)
  std::vector<std::string> vec_smudge_type;  // L23 SmudgeType
  int int4_chance = 100;               // L26

  static std::unique_ptr<LeaveSmudgeWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;  // L28-75
};

/// 弹着音注入面(CreateEffect 的 Game.Sound.Play;SoundType.World 形)
/// The impact-sound injection face (CreateEffect's Game.Sound.Play;
/// the SoundType.World form).
void SetWarheadSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play);

}  // namespace ora::mods
