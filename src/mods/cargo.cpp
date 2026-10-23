// UPSTREAM: OpenRA.Mods.Common/Traits/Cargo.cs + ParaDrop.cs @e180e3225
#include "mods/cargo.hpp"

#include "game/actor_info.hpp"
#include "map/map.hpp"
#include "mods/actor_exts.hpp"
#include "mods/air_activities.hpp"
#include "mods/aircraft.hpp"
#include "mods/cargo_activities.hpp"
#include "mods/deploy_order_targeter.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "mods/passenger.hpp"
#include "mods/util.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> FieldInt(const meta::RecordObject& rec_info,
                                     std::string_view str_name) {
  return sim::RecordFieldInt(rec_info, str_name);
}

std::optional<std::string> FieldString(const meta::RecordObject& rec_info,
                                       std::string_view str_name) {
  if (const auto v = sim::RecordFieldString(rec_info, str_name))
    return std::string{*v};
  return std::nullopt;
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

std::vector<std::string> FieldStringArray(const meta::RecordObject& rec_info,
                                          std::string_view str_name) {
  std::vector<std::string> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* list =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *list)
      if (const auto* s = std::get_if<std::string>(&element.val))
        vec_out.push_back(*s);
  return vec_out;
}

std::vector<std::pair<std::string, std::string>> FieldStringDict(
    const meta::RecordObject& rec_info, std::string_view str_name) {
  std::vector<std::pair<std::string, std::string>> vec_out;
  const auto* gv = sim::RecordFieldValue(rec_info, str_name);
  if (gv == nullptr)
    return vec_out;
  if (const auto* dict =
          std::get_if<std::vector<meta::GenericValue>>(&gv->val))
    for (const auto& element : *dict)
      if (const auto* pair =
              std::get_if<std::vector<meta::GenericValue>>(&element.val)) {
        if (pair->size() != 2)
          continue;
        const auto* key = std::get_if<std::string>(&(*pair)[0].val);
        const auto* value = std::get_if<std::string>(&(*pair)[1].val);
        if (key != nullptr && value != nullptr)
          vec_out.emplace_back(*key, *value);
      }
  return vec_out;
}

std::optional<WDist> FieldWDist(const meta::RecordObject& rec_info,
                                std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WDist{static_cast<std::int32_t>(*v)};
}

std::optional<WAngle> FieldWAngle(const meta::RecordObject& rec_info,
                                  std::string_view str_name) {
  const auto v = FieldInt(rec_info, str_name);
  if (!v.has_value())
    return std::nullopt;
  return WAngle{static_cast<std::int32_t>(*v)};
}

std::string ToLowerInvariant(std::string str_value) {
  for (char& c : str_value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return str_value;
}

bool HasAnyVoice(Actor& self, std::string_view str_voice) {
  for (auto* voice : self.TraitsImplementing<sim::IVoiced>())
    if (voice->HasVoice(self, str_voice))
      return true;
  return false;
}

}  // namespace

CargoInfoData CargoInfoData::Parse(const meta::RecordObject& rec_info) {
  CargoInfoData data;
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  if (const auto v = FieldInt(rec_info, "MaxWeight"))
    data.int4_max_weight = static_cast<int>(*v);
  data.vec_types = FieldStringArray(rec_info, "Types");
  data.vec_initial_units = FieldStringArray(rec_info, "InitialUnits");
  if (const auto v = FieldBool(rec_info, "EjectOnSell"))
    data.b_eject_on_sell = *v;
  if (const auto v = FieldBool(rec_info, "EjectOnDeath"))
    data.b_eject_on_death = *v;
  data.vec_unload_terrain_types =
      FieldStringArray(rec_info, "UnloadTerrainTypes");
  if (const auto v = FieldString(rec_info, "UnloadVoice"))
    data.str_unload_voice = *v;
  if (const auto v = FieldWDist(rec_info, "LoadRange"))
    data.dist_load_range = *v;
  if (const auto v = FieldWAngle(rec_info, "PassengerFacing"))
    data.angle_passenger_facing = *v;
  if (const auto v = FieldInt(rec_info, "AfterLoadDelay"))
    data.int4_after_load_delay = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "BeforeUnloadDelay"))
    data.int4_before_unload_delay = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "AfterUnloadDelay"))
    data.int4_after_unload_delay = static_cast<int>(*v);
  if (const auto v = FieldInt(rec_info, "BetweenUnloadDelay"))
    data.int4_between_unload_delay = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "UnloadCursor"))
    data.str_unload_cursor = *v;
  if (const auto v = FieldString(rec_info, "UnloadBlockedCursor"))
    data.str_unload_blocked_cursor = *v;
  if (const auto v = FieldString(rec_info, "LoadingCondition"))
    data.str_loading_condition = *v;
  if (const auto v = FieldString(rec_info, "LoadedCondition"))
    data.str_loaded_condition = *v;
  data.vec_passenger_conditions =
      FieldStringDict(rec_info, "PassengerConditions");
  return data;
}

Cargo::Cargo(sim::ActorInitializer& init, const CargoInfoData& info,
             std::string str_info_instance_name)
    : sim::ConditionalTraitCore<Cargo>(info.conditional),
      info_{info},
      str_info_instance_name_{std::move(str_info_instance_name)},
      self_{init.Self()} {
  b_check_terrain_type = !info_.vec_unload_terrain_types.empty();

  std::vector<Actor*> vec_initial;
  if (const auto* runtime_cargo_init =
          init.GetOrDefault<sim::RuntimeCargoInit>(str_info_instance_name_)) {
    vec_initial = runtime_cargo_init->Value();
  } else if (const auto* cargo_init =
                 init.GetOrDefault<sim::CargoInit>(str_info_instance_name_)) {
    for (const auto& u : cargo_init->Value()) {
      sim::TypeDictionary dict;
      sim::OwnerInit owner_init{self_.Owner()};
      dict.Add(std::make_unique<sim::OwnerInit>(self_.Owner()));
      vec_initial.push_back(self_.world().CreateActor(
          false, ToLowerInvariant(u), dict));
    }
  } else {
    for (const auto& u : info_.vec_initial_units) {
      sim::TypeDictionary dict;
      dict.Add(std::make_unique<sim::OwnerInit>(self_.Owner()));
      vec_initial.push_back(self_.world().CreateActor(
          false, ToLowerInvariant(u), dict));
    }
  }

  vec_cargo_ = std::move(vec_initial);
  for (Actor* a : vec_cargo_)
    int4_total_weight_ += GetWeight(*a);

  ptr_facing_ = self_.TraitOrDefault<sim::IFacing>();
}

void Cargo::Created(Actor& self) {
  sim::ConditionalTraitCore<Cargo>::CoreCreated(self);
  ptr_aircraft_ = self.TraitOrDefault<Aircraft>();

  if (!vec_cargo_.empty()) {
    for (auto* c : vec_cargo_)
      for (const auto& [name, condition] : info_.vec_passenger_conditions)
        if (name == c->InfoName())
          map_passenger_tokens_[c->InfoName()].push_back(
              self.GrantCondition(condition));

    if (!info_.str_loaded_condition.empty())
      vec_loaded_tokens_.push_back(
          self.GrantCondition(info_.str_loaded_condition));
  }

  self.world().AddFrameEndTask(
      [this](sim::World& w) {
        for (auto* c : vec_cargo_) {
          c->Trait<Passenger>()->SetTransport(&self_);

          for (auto* nec : c->TraitsImplementing<sim::INotifyEnteredCargo>())
            nec->OnEnteredCargo(*c, self_);

          for (auto* npe :
               self_.TraitsImplementing<sim::INotifyPassengerEntered>())
            npe->OnPassengerEntered(self_, *c);
        }

        b_initialised = true;
      });
}

int Cargo::GetWeight(Actor& a) {
  return a.Trait<Passenger>()->Info().int4_weight;
}

std::vector<sim::IOrderTargeter*> Cargo::Orders() {
  std::vector<sim::IOrderTargeter*> vec_orders;
  if (IsTraitDisabled())
    return vec_orders;

  if (!ptr_order_targeter_)
    ptr_order_targeter_ = std::make_unique<DeployOrderTargeter>(
        "Unload", 10, [this]() {
          return CanUnload() ? info_.str_unload_cursor
                             : info_.str_unload_blocked_cursor;
        });
  vec_orders.push_back(ptr_order_targeter_.get());
  return vec_orders;
}

std::unique_ptr<net::Order> Cargo::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target&, bool queued) {
  if (order->OrderID() == "Unload")
    return std::make_unique<net::Order>(order->OrderID(), &self, queued);

  return nullptr;
}

net::Order Cargo::IssueDeployOrder(Actor& self, bool queued) {
  return net::Order("Unload", &self, queued);
}

bool Cargo::CanIssueDeployOrder(Actor&, bool) { return true; }

void Cargo::ResolveOrder(Actor& self, const net::Order& order) {
  if (order.str_order_string == "Unload") {
    if (!order.b_queued && !CanUnload())
      return;

    self.QueueActivity(
        order.b_queued,
        self.world().Arena().Create<activities::UnloadCargo>(
            self, info_.dist_load_range));
  }
}

std::vector<CPos> Cargo::CurrentAdjacentCells() {
  std::vector<CPos> vec_cells;
  for (const CPos c :
       AdjacentCells(self_.world(), sim::Target::FromActor(&self_)))
    if (!(self_.Location() == c))
      vec_cells.push_back(c);
  return vec_cells;
}

bool Cargo::CanUnload(sim::BlockedByActor check) {
  if (IsTraitDisabled())
    return false;

  if (b_check_terrain_type) {
    if (!self_.world().Map().Contains(self_.Location()))
      return false;

    if (std::find(info_.vec_unload_terrain_types.begin(),
                  info_.vec_unload_terrain_types.end(),
                  self_.world().Map().GetTerrainInfo(self_.Location()).Type) ==
        info_.vec_unload_terrain_types.end())
      return false;
  }

  if (!IsEmpty() &&
      (ptr_aircraft_ == nullptr ||
       ptr_aircraft_->CanLand(self_.Location(), nullptr, false))) {
    for (const CPos c : CurrentAdjacentCells())
      for (Actor* p : vec_cargo_)
        if (!p->IsDead() && p->Trait<sim::IPositionable>()
                                ->CanEnterCell(c, nullptr, check))
          return true;
  }

  return false;
}

bool Cargo::CanLoad(Actor& a) {
  return !IsTraitDisabled() &&
         (std::find(vec_reserves_.begin(), vec_reserves_.end(), &a) !=
              vec_reserves_.end() ||
          HasSpace(GetWeight(a)));
}

bool Cargo::ReserveSpace(Actor& a) {
  if (std::find(vec_reserves_.begin(), vec_reserves_.end(), &a) !=
      vec_reserves_.end())
    return true;

  const int w = GetWeight(a);
  if (!HasSpace(w))
    return false;

  if (int4_loading_token == sim::Actor::InvalidConditionToken)
    int4_loading_token = self_.GrantCondition(info_.str_loading_condition);

  vec_reserves_.push_back(&a);
  int4_reserved_weight_ += w;
  LockForPickup(self_);

  return true;
}

void Cargo::UnreserveSpace(Actor& a) {
  if (std::find(vec_reserves_.begin(), vec_reserves_.end(), &a) ==
          vec_reserves_.end() ||
      self_.IsDead())
    return;

  int4_reserved_weight_ -= GetWeight(a);
  vec_reserves_.erase(
      std::find(vec_reserves_.begin(), vec_reserves_.end(), &a));
  ReleaseLock(self_);

  if (int4_loading_token != sim::Actor::InvalidConditionToken)
    int4_loading_token = self_.RevokeCondition(int4_loading_token);
}

void Cargo::LockForPickup(Actor& self) {
  if (state_ == State::Locked)
    return;

  state_ = State::Locked;

  self.CancelActivity();

  auto* air = self.TraitOrDefault<Aircraft>();
  if (air != nullptr && !air->AtLandAltitude()) {
    b_take_off_after_load = true;
    self.QueueActivity(self.world().Arena().Create<activities::Land>(
        self, std::optional<WAngle>{}, std::optional<core::Color>{}));
  }

  self.QueueActivity(self.world().Arena().Create<activities::WaitFor>(
      [this]() { return state_ != State::Locked; }, false));
}

void Cargo::ReleaseLock(Actor& self) {
  if (int4_reserved_weight_ != 0)
    return;

  state_ = State::Free;

  self.QueueActivity(self.world().Arena().Create<activities::Wait>(
      info_.int4_after_load_delay, false));
  if (b_take_off_after_load)
    self.QueueActivity(
        self.world().Arena().Create<activities::TakeOff>(self));

  b_take_off_after_load = false;
}

std::string Cargo::VoicePhraseForOrder(Actor& self,
                                       const net::Order& order) {
  if (order.str_order_string != "Unload" || IsEmpty() ||
      !HasAnyVoice(self, info_.str_unload_voice))
    return {};

  return info_.str_unload_voice;
}

Actor* Cargo::Unload(Actor& self, Actor* passenger) {
  if (passenger == nullptr)
    passenger = vec_cargo_.back();
  const auto it = std::find(vec_cargo_.begin(), vec_cargo_.end(), passenger);
  if (it == vec_cargo_.end())
    throw std::invalid_argument(
        "Attempted to unload an actor that is not a passenger.");
  vec_cargo_.erase(it);

  int4_total_weight_ -= GetWeight(*passenger);

  SetPassengerFacing(*passenger);

  for (auto* npe : self.TraitsImplementing<sim::INotifyPassengerExited>())
    npe->OnPassengerExited(self, *passenger);

  for (auto* nec :
       passenger->TraitsImplementing<sim::INotifyExitedCargo>())
    nec->OnExitedCargo(*passenger, self);

  passenger->Trait<Passenger>()->SetTransport(nullptr);

  const auto it_tokens = map_passenger_tokens_.find(passenger->InfoName());
  if (it_tokens != map_passenger_tokens_.end() &&
      !it_tokens->second.empty()) {
    self.RevokeCondition(it_tokens->second.back());
    it_tokens->second.pop_back();
  }

  if (!vec_loaded_tokens_.empty()) {
    self.RevokeCondition(vec_loaded_tokens_.back());
    vec_loaded_tokens_.pop_back();
  }

  return passenger;
}

void Cargo::SetPassengerFacing(Actor& passenger) {
  if (ptr_facing_ == nullptr)
    return;

  auto* passenger_facing = passenger.TraitOrDefault<sim::IFacing>();
  if (passenger_facing != nullptr)
    passenger_facing->SetFacing(
        WAngle{ptr_facing_->Facing().Angle +
               info_.angle_passenger_facing.Angle});
}

void Cargo::Load(Actor& self, Actor& a) {
  vec_cargo_.push_back(&a);
  const int w = GetWeight(a);
  int4_total_weight_ += w;
  const auto it_reserve =
      std::find(vec_reserves_.begin(), vec_reserves_.end(), &a);
  if (it_reserve != vec_reserves_.end()) {
    int4_reserved_weight_ -= w;
    vec_reserves_.erase(it_reserve);
    ReleaseLock(self);

    if (int4_loading_token != sim::Actor::InvalidConditionToken)
      int4_loading_token = self.RevokeCondition(int4_loading_token);
  }

  if (b_initialised) {
    a.Trait<Passenger>()->SetTransport(&self);

    for (auto* nec : a.TraitsImplementing<sim::INotifyEnteredCargo>())
      nec->OnEnteredCargo(a, self);

    for (auto* npe :
         self.TraitsImplementing<sim::INotifyPassengerEntered>())
      npe->OnPassengerEntered(self, a);
  }

  for (const auto& [name, condition] : info_.vec_passenger_conditions)
    if (name == a.InfoName())
      map_passenger_tokens_[a.InfoName()].push_back(
          self.GrantCondition(condition));

  if (!info_.str_loaded_condition.empty())
    vec_loaded_tokens_.push_back(
        self.GrantCondition(info_.str_loaded_condition));
}

void Cargo::Killed(Actor& self, const sim::AttackInfo& e) {
  if (info_.b_eject_on_death && IsAtGroundLevel(self) &&
      (!b_check_terrain_type ||
       std::find(info_.vec_unload_terrain_types.begin(),
                 info_.vec_unload_terrain_types.end(),
                 self.world().Map().GetTerrainInfo(self.Location()).Type) !=
           info_.vec_unload_terrain_types.end())) {
    while (!IsEmpty()) {
      Actor* passenger = Unload(self);
      self.world().AddFrameEndTask(
          [passenger, attacker = e.Attacker,
           location = self.Location()](sim::World& w) {
            auto* positionable = passenger->Trait<sim::IPositionable>();
            if (positionable->CanEnterCell(location, passenger,
                                           sim::BlockedByActor::All)) {
              positionable->SetPosition(passenger, location, SubCell::Any);
              w.Add(passenger);

              for (auto* nbm :
                   passenger
                       ->TraitsImplementing<sim::INotifyBlockingMove>())
                nbm->OnNotifyBlockingMove(*passenger, *passenger);

              passenger->Trait<Passenger>()->OnEjectedFromKilledCargo(
                  *passenger);
            } else {
              passenger->Kill(const_cast<Actor*>(attacker));
            }
          });
    }
  } else {
    for (auto* c : vec_cargo_)
      c->Kill(const_cast<Actor*>(e.Attacker));
  }

  vec_cargo_.clear();
}

void Cargo::Disposing(Actor&) {
  for (auto* c : vec_cargo_)
    c->Dispose();

  vec_cargo_.clear();
}

void Cargo::Sold(Actor& self) {
  if (!info_.b_eject_on_sell)
    return;

  while (!IsEmpty())
    SpawnPassenger(Unload(self));
}

void Cargo::SpawnPassenger(Actor* passenger) {
  self_.world().AddFrameEndTask(
      [passenger, location = self_.Location()](sim::World& w) {
        w.Add(passenger);
        passenger->Trait<sim::IPositionable>()->SetPosition(passenger,
                                                            location,
                                                            SubCell::Any);
      });
}

void Cargo::OnOwnerChanged(Actor&, Player& old_owner, Player& new_owner) {
  for (auto* p : vec_cargo_)
    p->ChangeOwner(&new_owner);
}

void Cargo::ModifyTransformActorInit(Actor&, sim::TypeDictionary& init) {
  init.Add(std::make_unique<sim::RuntimeCargoInit>(vec_cargo_));
}

ParaDropInfoData ParaDropInfoData::Parse(const meta::RecordObject& rec_info) {
  ParaDropInfoData data;
  if (const auto v = FieldWDist(rec_info, "DropRange"))
    data.dist_drop_range = *v;
  if (const auto v = FieldInt(rec_info, "DropInterval"))
    data.int4_drop_interval = static_cast<int>(*v);
  if (const auto v = FieldString(rec_info, "ChuteSound"))
    data.str_chute_sound = *v;
  return data;
}

ParaDrop::ParaDrop(Actor& self, const ParaDropInfoData& info)
    : info_{info}, self_{self}, cargo_{*self.Trait<Cargo>()} {}

void ParaDrop::SetLZ(CPos lz, bool check_landing_cell) {
  target_ = sim::Target::FromCell(self_.world(), lz);
  b_check_for_suitable_cell_ = check_landing_cell;
}

void ParaDrop::Tick(Actor& self) {
  if (int4_drop_delay_ > 0) {
    int4_drop_delay_--;
    return;
  }

  const bool was_in_drop_range = b_in_drop_range_;
  b_in_drop_range_ =
      target_.IsInRange(self.CenterPosition(), info_.dist_drop_range);

  if (b_in_drop_range_ && !was_in_drop_range)
    for (auto& handler : vec_on_entered_drop_range_)
      handler(self);

  if (!b_in_drop_range_ && was_in_drop_range)
    for (auto& handler : vec_on_exited_drop_range_)
      handler(self);

  if (!b_in_drop_range_ || cargo_.IsEmpty() ||
      !self.world().Map().Contains(self.Location()))
    return;

  Actor* drop_actor = cargo_.Peek();
  auto* drop_positionable = drop_actor->Trait<sim::IPositionable>();
  const CPos drop_cell = self.Location();
  SubCell drop_sub_cell = drop_positionable->GetAvailableSubCell(
      drop_cell, SubCell::Any, nullptr, sim::BlockedByActor::All);
  if (drop_sub_cell == SubCell::Invalid) {
    if (b_check_for_suitable_cell_)
      return;

    drop_sub_cell = SubCell::Any;
  }

  if (cargo_.Unload(self) != drop_actor)
    throw std::runtime_error("Peeked cargo was not unloaded!");

  self.world().AddFrameEndTask(
      [drop_actor, drop_positionable, drop_cell, drop_sub_cell,
       plane_z = self.CenterPosition().Z](sim::World& w) {
        drop_positionable->SetPosition(drop_actor, drop_cell, drop_sub_cell);

        const WPos drop_position =
            drop_actor->CenterPosition() +
            WVec{0, 0, plane_z - drop_actor->CenterPosition().Z};
        drop_positionable->SetCenterPosition(drop_actor, drop_position);
        w.Add(drop_actor);
      });

  int4_drop_delay_ = info_.int4_drop_interval;
}

void ParaDrop::RemovedFromWorld(Actor& self) {
  for (auto& handler : vec_on_removed_from_world_)
    handler(self);
}

int ParaDropSyncHash(const sim::ISync* s) {
  const auto* drop = static_cast<const ParaDrop*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(
          sim::sync::CombineSyncHash(0, sim::sync::HashBool(
                                           drop->b_in_drop_range_)),
          sim::HashTarget(drop->target_)),
      drop->int4_drop_delay_);
}

const bool b_para_drop_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.ParaDrop",
                                &ParaDropSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_para_drop_sync_registered_anchor =
    &b_para_drop_sync_registered;

}  // namespace ora::mods
