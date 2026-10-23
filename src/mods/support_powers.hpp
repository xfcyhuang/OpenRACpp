// UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/SupportPowerManager.cs
//          @e180e3225 L21-320(SupportPowerManager/SupportPowerInstance/
//          SelectGenericPowerTarget)+ SupportPower.cs L18-260(基类与
//          Info 字段面;具体 power 子类随其批)
//          L21-320 (SupportPowerManager/SupportPowerInstance/
//          SelectGenericPowerTarget) plus SupportPower.cs L18-260 (the
//          base class and Info field face; the concrete power subclasses
//          ride their own batches).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wpos.hpp"
#include "game/input.hpp"
#include "meta/generic_record.hpp"
#include "mods/radar.hpp"
#include "mods/production_support.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/order_generator.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

class SupportPowerManager;
class SupportPower;

/// SupportPowerInfo(SupportPower.cs L18-160)的解析面(OrderName 为各子类
/// ctor 派生的默认值,经值袋槽位直读)
/// The parsed face of SupportPowerInfo (SupportPower.cs L18-160; OrderName
/// is each subclass's ctor-derived default, read straight from the value
/// bag slot).
struct SupportPowerInfoData {
  sim::ConditionalTraitData conditional;
  int int4_charge_interval = 0;              // L21
  std::string str_icon_image{"icon"};        // L23
  std::string str_icon;                      // L27(null = 空)
  std::string str_icon_palette{"chrome"};    // L31
  std::string str_name;                      // L34
  std::string str_description;               // L37
  bool b_allow_multiple = false;             // L40
  bool b_one_shot = false;                   // L43
  std::string str_cursor{"ability"};         // L48
  std::string str_blocked_cursor{"generic-blocked"};  // L51
  bool b_start_fully_charged = false;        // L55
  std::vector<std::string> vec_prerequisites;  // L57
  std::string str_detected_sound;            // L59
  std::string str_detected_speech_notification;   // L62
  std::string str_detected_text_notification;     // L65
  std::string str_begin_charge_sound;             // L67
  std::string str_begin_charge_speech_notification;  // L70
  std::string str_begin_charge_text_notification;    // L73
  std::string str_end_charge_sound;               // L75
  std::string str_end_charge_speech_notification;    // L78
  std::string str_end_charge_text_notification;      // L81
  std::string str_select_target_sound;            // L83
  std::string str_select_target_speech_notification;  // L86
  std::string str_select_target_text_notification;    // L89
  std::string str_insufficient_power_sound;       // L91
  std::string str_insufficient_power_speech_notification;  // L93
  std::string str_insufficient_power_text_notification;    // L96
  std::string str_launch_sound;                   // L99
  std::string str_launch_speech_notification;     // L102
  std::string str_launch_text_notification;       // L105
  std::string str_incoming_sound;                 // L107
  std::string str_incoming_speech_notification;   // L110
  std::string str_incoming_text_notification;     // L113
  sim::PlayerRelationship kind_display_timer_relationships =
      sim::PlayerRelationship::None;  // L116
  bool b_display_beacon = false;               // L119
  bool b_beacon_palette_is_player_palette = true;  // L121
  std::string str_beacon_palette{"player"};    // L124
  std::string str_beacon_image{"beacon"};      // L126
  std::string str_beacon_poster;               // L129
  std::string str_beacon_poster_palette{"chrome"};  // L132
  std::string str_clock_sequence;              // L135
  std::string str_beacon_sequence;             // L138
  std::string str_arrow_sequence;              // L141
  std::string str_circle_sequence;             // L144
  int int4_beacon_delay = 0;                   // L147
  bool b_display_radar_ping = false;           // L149
  int int4_radar_ping_duration = 125;          // L152
  std::string str_order_name;                  // L154(子类 ctor 派生默认)
  int int4_support_power_palette_order = 9999;  // L157

  static SupportPowerInfoData Parse(const meta::RecordObject& rec_info);
};

/// SupportPowerInstance(SupportPowerManager.cs L141-281):manager 账本内
/// 的单条 power 实例(充能状态机/目标选择/激活)
/// SupportPowerInstance (SupportPowerManager.cs L141-281): the single
/// power entry inside the manager's ledger (the charge state machine,
/// target selection, activation).
class SupportPowerInstance {
 public:
  SupportPowerInstance(std::string str_key,
                       const SupportPowerInfoData& info,
                       SupportPowerManager& manager);

  const std::string& Key() const { return str_key_; }
  int TotalTicks() const { return int4_total_ticks_; }
  int RemainingTicks() const { return int4_remaining_sub_ticks_ / 100; }
  bool Active() const { return b_active_; }
  bool Disabled() const;
  bool Ready() const { return Active() && RemainingTicks() == 0; }

  const SupportPowerInfoData* Info() const;
  const std::string& Name() const { return str_name_; }
  const std::string& Description() const { return str_description_; }

  std::vector<SupportPower*>& Instances() { return vec_instances_; }
  const std::vector<SupportPower*>& Instances() const {
    return vec_instances_;
  }

  void ResetTimer() { int4_remaining_sub_ticks_ = int4_total_ticks_ * 100; }

  virtual void PrerequisitesAvailable(bool b_available);
  virtual void Tick();
  virtual void Target();
  virtual void Activate(const net::Order& order);
  virtual const std::string* IconOverlayTextOverride() { return nullptr; }
  virtual const std::string* TooltipTimeTextOverride() { return nullptr; }

  virtual ~SupportPowerInstance() = default;

 protected:
  SupportPowerManager& Manager() const { return manager_; }
  SupportPowerManager& manager_;
  std::string str_key_;
  std::vector<SupportPower*> vec_instances_;
  int int4_total_ticks_ = 0;
  int int4_remaining_sub_ticks_ = 0;
  bool b_active_ = false;
  bool b_instances_enabled_ = false;
  bool b_prereqs_available_ = true;
  bool b_one_shot_fired_ = false;
  bool b_notified_charging_ = false;
  bool b_notified_ready_ = false;

 private:
  std::string str_name_;
  std::string str_description_;
};

/// SupportPower(SupportPower.cs L162-260):抽象基类(具体 power 随其批;
/// 声音面随声音注入面,文本通知直投)
/// SupportPower (SupportPower.cs L162-260): the abstract base (concrete
/// powers ride their batches; sounds ride the sound-injection face, text
/// notifications go direct).
class SupportPower : public TraitBase,
                     public sim::ConditionalTraitCore<SupportPower>,
                     public sim::IObservesVariables,
                     public sim::INotifyCreated {
 public:
  SupportPower(const ActorInitializer& init, SupportPowerInfoData info)
      : sim::ConditionalTraitCore<SupportPower>(info.conditional),
        ptr_self_{&init.Self()},
        info_{std::move(info)} {}

  ORA_TRAIT_INTERFACES(SupportPower,
                       OpenRA_Mods_Common_Traits_SupportPower,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<SupportPower>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<SupportPower>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(Actor& self) override;

  Actor* Self() const { return ptr_self_; }
  const SupportPowerInfoData& Info() const { return info_; }

  virtual std::unique_ptr<SupportPowerInstance> CreateInstance(
      const std::string& str_key, SupportPowerManager& manager) {
    return std::make_unique<SupportPowerInstance>(str_key, info_, manager);
  }

  virtual void Charging(Actor& self, const std::string& str_key);
  virtual void Charged(Actor& self, const std::string& str_key);

  virtual void SelectTarget(Actor& self, const std::string& str_order,
                            SupportPowerManager& manager);

  virtual void Activate(Actor& self, const net::Order& order,
                        SupportPowerManager& manager);

  virtual void PlayLaunchSounds();

  /// CellsMatching(L250-259):footprint 'x' 格展开
  /// CellsMatching (L250-259): the footprint 'x' cell expansion.
  static std::vector<CPos> CellsMatching(CPos location,
                                         std::span<const char> vec_footprint,
                                         CVec dimensions);

 protected:
  Actor* ptr_self_ = nullptr;
  SupportPowerInfoData info_;
  RadarPing* ptr_ping_ = nullptr;
};

/// SupportPowerManagerInfo(L23-26)的解析面
/// The parsed face of SupportPowerManagerInfo (L23-26).
struct SupportPowerManagerInfoData {
  static SupportPowerManagerInfoData Parse(const meta::RecordObject& rec_info);
};

/// SupportPowerManager(L28-138):player actor 的 power 账本(出生/移除的
/// 实例聚合 + TechTree 前置门 + order 分发)
/// SupportPowerManager (L28-138): the player actor's power ledger (the
/// spawn/removal instance aggregation, the TechTree prerequisite gate, the
/// order dispatch).
class SupportPowerManager final : public TraitBase,
                                  public sim::ITick,
                                  public sim::IResolveOrder,
                                  public sim::ITechTreeElement {
 public:
  explicit SupportPowerManager(const ActorInitializer& init);

  ORA_TRAIT_INTERFACES(SupportPowerManager,
                       OpenRA_Mods_Common_Traits_SupportPowerManager,
                       sim::ITick, sim::IResolveOrder, sim::ITechTreeElement)

  Actor* Self() const { return ptr_self_; }
  DeveloperMode* DevMode() const { return ptr_dev_mode_; }
  TechTree* GetTechTree() const { return ptr_tech_tree_; }

  /// Powers(Dictionary<string, SupportPowerInstance>;键序 map)
  /// Powers (Dictionary<string, SupportPowerInstance>; the map's key
  /// order).
  std::map<std::string, std::unique_ptr<SupportPowerInstance>>& Powers() {
    return map_powers_;
  }
  const std::map<std::string, std::unique_ptr<SupportPowerInstance>>& Powers()
      const {
    return map_powers_;
  }

  SupportPowerInstance* FindPower(const std::string& str_key) const {
    const auto it = map_powers_.find(str_key);
    return it != map_powers_.end() ? it->second.get() : nullptr;
  }

  RadarPings* RadarPingsTrait();

  static std::string MakeKey(const SupportPower& sp);

  void Tick(Actor& self) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  std::vector<SupportPowerInstance*> GetPowersForActor(Actor& a) const;

  void PrerequisitesAvailable(const std::string& key) override;
  void PrerequisitesUnavailable(const std::string& key) override;
  void PrerequisitesItemHidden(const std::string&) override {}
  void PrerequisitesItemVisible(const std::string&) override {}

  const std::vector<SupportPower*>& InstancesOfForTest(
      const std::string& str_key) const;

 private:
  void ActorAdded(Actor& a);
  void ActorRemoved(Actor& a);

  Actor* ptr_self_ = nullptr;
  DeveloperMode* ptr_dev_mode_ = nullptr;
  TechTree* ptr_tech_tree_ = nullptr;
  std::map<std::string, std::unique_ptr<SupportPowerInstance>> map_powers_;
  RadarPings* ptr_radar_pings_ = nullptr;
  bool b_radar_pings_resolved_ = false;
};

/// SelectGenericPowerTarget(L283-320):泛型落点 order generator
/// SelectGenericPowerTarget (L283-320): the generic target-pick order
/// generator.
class SelectGenericPowerTarget : public sim::IOrderGenerator {
 public:
  SelectGenericPowerTarget(std::string str_order,
                           SupportPowerManager& manager,
                           const SupportPowerInfoData& info,
                           GameSettingsFace game_settings = {});

  const std::string& OrderKey() const { return str_order_key_; }

  MouseButton ActionButton() const override {
    return game_settings_.ResolveActionButton(MouseActionType::SupportPower);
  }
  MouseButton CancelButton() const {
    return game_settings_.ResolveCancelButton(MouseActionType::SupportPower);
  }

  std::vector<std::unique_ptr<net::Order>> Order(
      sim::World& world, CPos cell,
      int2 world_pixel,
      const MouseInput& mi_input) override;
  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer&,
                                        sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::string GetCursor(sim::World& world, CPos cell, int2,
                        const MouseInput&) override;
  void Deactivate() override {}
  void SelectionChanged(sim::World&, std::span<Actor* const>) override {}

 private:
  std::vector<std::unique_ptr<net::Order>> OrderInner(
      sim::World& world, CPos cell,
      int2, const MouseInput&);

 protected:
  SupportPowerManager& manager_;
  std::string str_order_key_;
  const SupportPowerInfoData& info_;
  GameSettingsFace game_settings_;
};

}  // namespace ora::mods
