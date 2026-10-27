#include "mods/support_powers.hpp"

#include "fluent/fluent_provider.hpp"
#include "net/text_notifications.hpp"
#include "game/actor_info.hpp"
#include "sim/target.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
}

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<bool> FieldBool(const meta::RecordObject& rec_info,
                              std::string_view str_name) {
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return std::nullopt;
  if (const auto* b = std::get_if<bool>(&gv->val))
    return *b;
  return std::nullopt;
}

}

SupportPowerInfoData SupportPowerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  SupportPowerInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "ChargeInterval"))
    data.int4_charge_interval = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "IconImage"))
    data.str_icon_image = *v;
  if (const auto v = FieldString(rec_info, "Icon"))
    data.str_icon = *v;
  if (const auto v = FieldString(rec_info, "IconPalette"))
    data.str_icon_palette = *v;
  if (const auto v = FieldString(rec_info, "Name"))
    data.str_name = *v;
  if (const auto v = FieldString(rec_info, "Description"))
    data.str_description = *v;
  if (const auto v = FieldBool(rec_info, "AllowMultiple"))
    data.b_allow_multiple = *v;
  if (const auto v = FieldBool(rec_info, "OneShot"))
    data.b_one_shot = *v;
  if (const auto v = FieldString(rec_info, "Cursor"))
    data.str_cursor = *v;
  if (const auto v = FieldString(rec_info, "BlockedCursor"))
    data.str_blocked_cursor = *v;
  if (const auto v = FieldBool(rec_info, "StartFullyCharged"))
    data.b_start_fully_charged = *v;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "Prerequisites"))
    if (const auto* vec = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& e : *vec)
        if (const auto* s = std::get_if<std::string>(&e.val))
          data.vec_prerequisites.push_back(*s);
  if (const auto v = FieldString(rec_info, "DetectedSound"))
    data.str_detected_sound = *v;
  if (const auto v = FieldString(rec_info, "DetectedSpeechNotification"))
    data.str_detected_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "DetectedTextNotification"))
    data.str_detected_text_notification = *v;
  if (const auto v = FieldString(rec_info, "BeginChargeSound"))
    data.str_begin_charge_sound = *v;
  if (const auto v = FieldString(rec_info, "BeginChargeSpeechNotification"))
    data.str_begin_charge_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "BeginChargeTextNotification"))
    data.str_begin_charge_text_notification = *v;
  if (const auto v = FieldString(rec_info, "EndChargeSound"))
    data.str_end_charge_sound = *v;
  if (const auto v = FieldString(rec_info, "EndChargeSpeechNotification"))
    data.str_end_charge_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "EndChargeTextNotification"))
    data.str_end_charge_text_notification = *v;
  if (const auto v = FieldString(rec_info, "SelectTargetSound"))
    data.str_select_target_sound = *v;
  if (const auto v = FieldString(rec_info, "SelectTargetSpeechNotification"))
    data.str_select_target_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "SelectTargetTextNotification"))
    data.str_select_target_text_notification = *v;
  if (const auto v = FieldString(rec_info, "InsufficientPowerSound"))
    data.str_insufficient_power_sound = *v;
  if (const auto v = FieldString(rec_info, "InsufficientPowerSpeechNotification"))
    data.str_insufficient_power_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "InsufficientPowerTextNotification"))
    data.str_insufficient_power_text_notification = *v;
  if (const auto v = FieldString(rec_info, "LaunchSound"))
    data.str_launch_sound = *v;
  if (const auto v = FieldString(rec_info, "LaunchSpeechNotification"))
    data.str_launch_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "LaunchTextNotification"))
    data.str_launch_text_notification = *v;
  if (const auto v = FieldString(rec_info, "IncomingSound"))
    data.str_incoming_sound = *v;
  if (const auto v = FieldString(rec_info, "IncomingSpeechNotification"))
    data.str_incoming_speech_notification = *v;
  if (const auto v = FieldString(rec_info, "IncomingTextNotification"))
    data.str_incoming_text_notification = *v;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "DisplayTimerRelationships"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.kind_display_timer_relationships =
          static_cast<sim::PlayerRelationship>(*n);
  if (const auto v = FieldBool(rec_info, "DisplayBeacon"))
    data.b_display_beacon = *v;
  if (const auto v = FieldBool(rec_info, "BeaconPaletteIsPlayerPalette"))
    data.b_beacon_palette_is_player_palette = *v;
  if (const auto v = FieldString(rec_info, "BeaconPalette"))
    data.str_beacon_palette = *v;
  if (const auto v = FieldString(rec_info, "BeaconImage"))
    data.str_beacon_image = *v;
  if (const auto v = FieldString(rec_info, "BeaconPoster"))
    data.str_beacon_poster = *v;
  if (const auto v = FieldString(rec_info, "BeaconPosterPalette"))
    data.str_beacon_poster_palette = *v;
  if (const auto v = FieldString(rec_info, "ClockSequence"))
    data.str_clock_sequence = *v;
  if (const auto v = FieldString(rec_info, "BeaconSequence"))
    data.str_beacon_sequence = *v;
  if (const auto v = FieldString(rec_info, "ArrowSequence"))
    data.str_arrow_sequence = *v;
  if (const auto v = FieldString(rec_info, "CircleSequence"))
    data.str_circle_sequence = *v;
  if (const auto v = FieldInt(rec_info, "BeaconDelay"))
    data.int4_beacon_delay = static_cast<int>(*v);
  if (const auto v = FieldBool(rec_info, "DisplayRadarPing"))
    data.b_display_radar_ping = *v;
  if (const auto v = FieldInt(rec_info, "RadarPingDuration"))
    data.int4_radar_ping_duration = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "OrderName"))
    data.str_order_name = *v;
  if (const auto v = FieldInt(rec_info, "SupportPowerPaletteOrder"))
    data.int4_support_power_palette_order = static_cast<int>(*v);
  return data;
}

void SupportPower::Created(Actor& self) {
  sim::ConditionalTraitCore<SupportPower>::CoreCreated(self);
  sim::Player* ptr_local = self.world().LocalPlayer();
  if (ptr_local != nullptr && ptr_local != self.Owner())
    net::TextNotificationsManager::Instance().AddTransientLine(
        ptr_local, info_.str_detected_text_notification);
}

void SupportPower::Charging(Actor& self, const std::string&) {
  net::TextNotificationsManager::Instance().AddTransientLine(
      self.Owner(), info_.str_begin_charge_text_notification);
}

void SupportPower::Charged(Actor& self, const std::string&) {
  net::TextNotificationsManager::Instance().AddTransientLine(
      self.Owner(), info_.str_end_charge_text_notification);

  for (auto* notify : self.TraitsImplementing<sim::INotifySupportPower>())
    notify->Charged(self);
}

SupportPowerInstance::SupportPowerInstance(std::string str_key,
                                           const SupportPowerInfoData& info,
                                           SupportPowerManager& manager)
    : manager_(manager),
      str_key_(std::move(str_key)),
      int4_total_ticks_(info.int4_charge_interval),
      int4_remaining_sub_ticks_(
          info.b_start_fully_charged ? 0 : info.int4_charge_interval * 100),
      str_name_(info.str_name.empty()
                    ? std::string{}
                    : fluent::FluentProvider::Instance().GetMessage(
                          info.str_name)),
      str_description_(
          info.str_description.empty()
              ? std::string{}
              : fluent::FluentProvider::Instance().GetMessage(
                    info.str_description)) {}

const SupportPowerInfoData* SupportPowerInstance::Info() const {
  return vec_instances_.empty() ? nullptr
                                : &vec_instances_.front()->Info();
}

bool SupportPowerInstance::Disabled() const {
  return manager_.Self()->Owner()->PlayerWinState() == sim::WinState::Lost ||
         (!b_prereqs_available_ && !manager_.DevMode()->AllTech) ||
         !b_instances_enabled_ || b_one_shot_fired_;
}

void SupportPowerInstance::PrerequisitesAvailable(bool b_available) {
  b_prereqs_available_ = b_available;

  if (!b_available)
    int4_remaining_sub_ticks_ = int4_total_ticks_ * 100;
}

void SupportPowerInstance::Tick() {
  b_instances_enabled_ = false;
  for (const SupportPower* i : vec_instances_)
    if (!i->IsTraitDisabled()) {
      b_instances_enabled_ = true;
      break;
    }
  if (!b_instances_enabled_)
    int4_remaining_sub_ticks_ = int4_total_ticks_ * 100;

  b_active_ = !Disabled();
  bool any_unpaused = false;
  for (const SupportPower* i : vec_instances_)
    if (!i->IsTraitPaused()) {
      any_unpaused = true;
      break;
    }
  b_active_ = b_active_ && any_unpaused;
  if (!b_active_)
    return;

  SupportPower* power = vec_instances_[0];
  if (manager_.DevMode()->FastCharge && int4_remaining_sub_ticks_ > 2500)
    int4_remaining_sub_ticks_ = 2500;

  if (int4_remaining_sub_ticks_ > 0)
    int4_remaining_sub_ticks_ =
        std::clamp(int4_remaining_sub_ticks_ - 100, 0,
                   int4_total_ticks_ * 100);

  if (!b_notified_charging_) {
    power->Charging(*power->Self(), str_key_);
    b_notified_charging_ = true;
  }

  if (RemainingTicks() == 0 && !b_notified_ready_) {
    power->Charged(*power->Self(), str_key_);
    b_notified_ready_ = true;
  }
}

void SupportPowerInstance::Target() {
  if (!Ready())
    return;

  SupportPower* power = nullptr;
  for (SupportPower* i : vec_instances_)
    if (!i->IsTraitPaused()) {
      power = i;
      break;
    }

  if (power == nullptr)
    return;

  net::TextNotificationsManager::Instance().AddTransientLine(
      power->Self()->Owner(), power->Info().str_select_target_text_notification);

  power->SelectTarget(*power->Self(), str_key_, manager_);
}

void SupportPowerInstance::Activate(const net::Order& order) {
  if (!Ready())
    return;

  SupportPower* power = nullptr;
  std::int64_t best_distance = 0;
  for (SupportPower* a : vec_instances_) {
    if (a->IsTraitPaused() || a->IsTraitDisabled())
      continue;

    std::int64_t distance = 0;
    if (a->Self()->OccupiesSpace() != nullptr &&
        order.target.Type() != sim::TargetType::Invalid) {
      const WVec delta =
          a->Self()->CenterPosition() - order.target.CenterPosition();
      distance = delta.HorizontalLengthSquared();
    }

    if (power == nullptr || distance < best_distance) {
      power = a;
      best_distance = distance;
    }
  }

  if (power == nullptr)
    return;

  const SupportPowerInfoData& info = power->Info();
  power->Activate(*power->Self(), order, manager_);
  int4_remaining_sub_ticks_ = int4_total_ticks_ * 100;
  b_notified_charging_ = false;
  b_notified_ready_ = false;

  if (info.b_one_shot) {
    PrerequisitesAvailable(false);
    b_one_shot_fired_ = true;
  }
}

void SupportPower::SelectTarget(Actor& self, const std::string& str_order,
                                SupportPowerManager& manager) {
  self.world().AdoptOrderGenerator(std::make_unique<SelectGenericPowerTarget>(
      str_order, manager, info_));
}

void SupportPower::Activate(Actor& self, const net::Order& order,
                            SupportPowerManager& manager) {
  if (info_.b_display_radar_ping) {
    if (RadarPings* pings = manager.RadarPingsTrait()) {
      const bool b_is_viewer_allied =
          order.Player() != nullptr &&
          order.Player()->IsAlliedWith(self.world().RenderPlayer());
      ptr_ping_ = &pings->Add(
          [b_is_viewer_allied] { return b_is_viewer_allied; },
          order.target.CenterPosition(), order.Player()->Color(),
          info_.int4_radar_ping_duration);
    }
  }

  for (auto* notify : self.TraitsImplementing<sim::INotifySupportPower>())
    notify->Activated(self, info_.str_order_name);
}

void SupportPower::PlayLaunchSounds() {
  sim::Player* ptr_local = ptr_self_->world().LocalPlayer();
  if (ptr_local == nullptr || ptr_local->Spectating())
    return;

  const bool b_is_allied = ptr_self_->Owner()->IsAlliedWith(ptr_local);
  net::TextNotificationsManager::Instance().AddTransientLine(
      ptr_local, b_is_allied ? info_.str_launch_text_notification
                             : info_.str_incoming_text_notification);
}

std::vector<CPos> SupportPower::CellsMatching(CPos location,
                                              std::span<const char> vec_footprint,
                                              CVec dimensions) {
  std::vector<CPos> vec_cells;
  int index = 0;
  const int x = location.X() - (dimensions.X - 1) / 2;
  const int y = location.Y() - (dimensions.Y - 1) / 2;
  for (int j = 0; j < dimensions.Y; j++)
    for (int i = 0; i < dimensions.X; i++)
      if (vec_footprint[index++] == 'x')
        vec_cells.emplace_back(x + i, y + j);
  return vec_cells;
}

SupportPowerManagerInfoData SupportPowerManagerInfoData::Parse(
    const meta::RecordObject&) {
  return {};
}

SupportPowerManager::SupportPowerManager(const ActorInitializer& init)
    : ptr_self_{&init.Self()} {
  ptr_dev_mode_ = ptr_self_->TraitOrDefault<DeveloperMode>();
  ptr_tech_tree_ = ptr_self_->TraitOrDefault<TechTree>();

  sim::World& world = ptr_self_->world();
  world.AddActorAddedHandler([this](Actor& a) { ActorAdded(a); });
  world.AddActorRemovedHandler([this](Actor& a) { ActorRemoved(a); });
}

std::string SupportPowerManager::MakeKey(const SupportPower& sp) {
  if (sp.Info().b_allow_multiple)
    return sp.Info().str_order_name + "_" +
           std::to_string(sp.Self()->ActorID());
  return sp.Info().str_order_name;
}

RadarPings* SupportPowerManager::RadarPingsTrait() {
  if (!b_radar_pings_resolved_) {
    b_radar_pings_resolved_ = true;
    ptr_radar_pings_ =
        ptr_self_->world().WorldActor()->TraitOrDefault<RadarPings>();
  }
  return ptr_radar_pings_;
}

void SupportPowerManager::ActorAdded(Actor& a) {
  if (a.Owner() != ptr_self_->Owner())
    return;

  for (auto* t : a.TraitsImplementing<SupportPower>()) {
    const std::string str_key = MakeKey(*t);

    SupportPowerInstance* spi = FindPower(str_key);
    if (spi == nullptr) {
      auto instance = t->CreateInstance(str_key, *this);
      spi = instance.get();
      map_powers_.emplace(str_key, std::move(instance));

      if (!t->Info().vec_prerequisites.empty()) {
        ptr_tech_tree_->Add(str_key, t->Info().vec_prerequisites, 0, this);
        ptr_tech_tree_->Update();
      }
    }

    spi->Instances().push_back(t);
  }
}

void SupportPowerManager::ActorRemoved(Actor& a) {
  if (a.Owner() != ptr_self_->Owner() ||
      !a.Info()->HasTraitInfoOfInterface("OpenRA.Mods.Common.Traits.SupportPower"))
    return;

  for (auto* t : a.TraitsImplementing<SupportPower>()) {
    const std::string str_key = MakeKey(*t);
    const auto it = map_powers_.find(str_key);
    if (it == map_powers_.end())
      continue;

    std::erase(it->second->Instances(), t);

    if (it->second->Instances().empty() && !it->second->Disabled()) {
      map_powers_.erase(it);
      ptr_tech_tree_->Remove(str_key);
      ptr_tech_tree_->Update();
    }
  }
}

void SupportPowerManager::Tick(Actor&) {
  for (auto& [str_key, power] : map_powers_)
    power->Tick();
}

void SupportPowerManager::ResolveOrder(Actor&, const net::Order& order) {
  if (SupportPowerInstance* sp = FindPower(order.str_order_string))
    sp->Activate(order);
}

std::vector<SupportPowerInstance*> SupportPowerManager::GetPowersForActor(
    Actor& a) const {
  std::vector<SupportPowerInstance*> vec_out;
  if (map_powers_.empty() || a.Owner() != ptr_self_->Owner() ||
      !a.Info()->HasTraitInfoOfInterface(
          "OpenRA.Mods.Common.Traits.SupportPower"))
    return vec_out;

  for (auto* t : a.TraitsImplementing<SupportPower>()) {
    SupportPowerInstance* power = FindPower(MakeKey(*t));
    bool any_local_enabled = false;
    for (const SupportPower* i : power->Instances())
      if (!i->IsTraitDisabled() && i->Self() == &a) {
        any_local_enabled = true;
        break;
      }
    if (any_local_enabled)
      vec_out.push_back(power);
  }

  return vec_out;
}

void SupportPowerManager::PrerequisitesAvailable(const std::string& key) {
  if (SupportPowerInstance* sp = FindPower(key))
    sp->PrerequisitesAvailable(true);
}

void SupportPowerManager::PrerequisitesUnavailable(const std::string& key) {
  if (SupportPowerInstance* sp = FindPower(key))
    sp->PrerequisitesAvailable(false);
}

const std::vector<SupportPower*>& SupportPowerManager::InstancesOfForTest(
    const std::string& str_key) const {
  static const std::vector<SupportPower*> vec_empty;
  const SupportPowerInstance* spi = FindPower(str_key);
  return spi != nullptr ? spi->Instances() : vec_empty;
}

SelectGenericPowerTarget::SelectGenericPowerTarget(
    std::string str_order, SupportPowerManager& manager,
    const SupportPowerInfoData& info, GameSettingsFace game_settings)
    : manager_(manager),
      str_order_key_(std::move(str_order)),
      info_(info),
      game_settings_(game_settings) {
  if (game_settings_.mouse_control_style == MouseControlStyle::Classic)
    manager_.Self()->world().Selection()->Clear();
}

std::vector<std::unique_ptr<net::Order>> SelectGenericPowerTarget::Order(
    sim::World& world, CPos cell, int2 world_pixel,
    const MouseInput& mi) {
  if (mi.Button == ActionButton() && mi.Event == MouseInputEvent::Down)
    return OrderInner(world, cell, world_pixel, mi);

  if (mi.Button == CancelButton() && mi.Event == MouseInputEvent::Up)
    world.CancelInputMode();

  return {};
}

std::vector<std::unique_ptr<net::Order>> SelectGenericPowerTarget::OrderInner(
    sim::World& world, CPos cell, int2, const MouseInput&) {

  const std::string str_key = str_order_key_;
  sim::Actor* ptr_subject = manager_.Self();
  world.CancelInputMode();
  if (!world.Map().Contains(cell))
    return {};

  auto up_order = std::make_unique<net::Order>(
      str_key, ptr_subject, sim::Target::FromCell(world, cell), false);
  up_order->b_suppress_visual_feedback = true;
  std::vector<std::unique_ptr<net::Order>> vec_out;
  vec_out.push_back(std::move(up_order));
  return vec_out;
}

void SelectGenericPowerTarget::Tick(sim::World& world) {
  const SupportPowerInstance* p = manager_.FindPower(str_order_key_);
  if (p == nullptr || !p->Active() || !p->Ready())
    world.CancelInputMode();
}

std::string SelectGenericPowerTarget::GetCursor(sim::World& world, CPos cell,
                                                int2, const MouseInput&) {
  return world.Map().Contains(cell) ? info_.str_cursor
                                    : info_.str_blocked_cursor;
}

}
