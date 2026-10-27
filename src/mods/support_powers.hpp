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

struct SupportPowerInfoData {
  sim::ConditionalTraitData conditional;
  int int4_charge_interval = 0;
  std::string str_icon_image{"icon"};
  std::string str_icon;
  std::string str_icon_palette{"chrome"};
  std::string str_name;
  std::string str_description;
  bool b_allow_multiple = false;
  bool b_one_shot = false;
  std::string str_cursor{"ability"};
  std::string str_blocked_cursor{"generic-blocked"};
  bool b_start_fully_charged = false;
  std::vector<std::string> vec_prerequisites;
  std::string str_detected_sound;
  std::string str_detected_speech_notification;
  std::string str_detected_text_notification;
  std::string str_begin_charge_sound;
  std::string str_begin_charge_speech_notification;
  std::string str_begin_charge_text_notification;
  std::string str_end_charge_sound;
  std::string str_end_charge_speech_notification;
  std::string str_end_charge_text_notification;
  std::string str_select_target_sound;
  std::string str_select_target_speech_notification;
  std::string str_select_target_text_notification;
  std::string str_insufficient_power_sound;
  std::string str_insufficient_power_speech_notification;
  std::string str_insufficient_power_text_notification;
  std::string str_launch_sound;
  std::string str_launch_speech_notification;
  std::string str_launch_text_notification;
  std::string str_incoming_sound;
  std::string str_incoming_speech_notification;
  std::string str_incoming_text_notification;
  sim::PlayerRelationship kind_display_timer_relationships =
      sim::PlayerRelationship::None;
  bool b_display_beacon = false;
  bool b_beacon_palette_is_player_palette = true;
  std::string str_beacon_palette{"player"};
  std::string str_beacon_image{"beacon"};
  std::string str_beacon_poster;
  std::string str_beacon_poster_palette{"chrome"};
  std::string str_clock_sequence;
  std::string str_beacon_sequence;
  std::string str_arrow_sequence;
  std::string str_circle_sequence;
  int int4_beacon_delay = 0;
  bool b_display_radar_ping = false;
  int int4_radar_ping_duration = 125;
  std::string str_order_name;
  int int4_support_power_palette_order = 9999;

  static SupportPowerInfoData Parse(const meta::RecordObject& rec_info);
};

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

  static std::vector<CPos> CellsMatching(CPos location,
                                         std::span<const char> vec_footprint,
                                         CVec dimensions);

 protected:
  Actor* ptr_self_ = nullptr;
  SupportPowerInfoData info_;
  RadarPing* ptr_ping_ = nullptr;
};

struct SupportPowerManagerInfoData {
  static SupportPowerManagerInfoData Parse(const meta::RecordObject& rec_info);
};

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

}
