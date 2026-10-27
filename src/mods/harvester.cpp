#include "mods/harvester.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "game/actor_info.hpp"
#include "mods/harvest_activities.hpp"
#include "net/order.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

HarvesterInfoData HarvesterInfoData::Parse(
    const meta::RecordObject& rec_info) {
  HarvesterInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "Type") {
        if (auto* n = std::get_if<std::int64_t>(&v.val)) {
          data.bitset_type = core::BitSet<sim::DockType>::FromRawBits(
              static_cast<std::uint64_t>(*n));
        } else if (auto* s = std::get_if<std::string>(&v.val)) {
          const std::vector<std::string> vec_one{*s};
          data.bitset_type =
              core::BitSet<sim::DockType>::FromStringsNoAlloc(vec_one);
        }
      } else if (name == "UnblockCell") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          std::vector<int> vec_xy;
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              vec_xy.push_back(static_cast<int>(*n));
          if (vec_xy.size() == 2)
            data.vec_unblock_cell = CVec{vec_xy[0], vec_xy[1]};
        }
      } else if (name == "BaleLoadDelay") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_bale_load_delay = static_cast<int>(*n);
      } else if (name == "BaleUnloadDelay") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_bale_unload_delay = static_cast<int>(*n);
      } else if (name == "BaleUnloadAmount") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_bale_unload_amount = static_cast<int>(*n);
      } else if (name == "HarvestFacings") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_harvest_facings = static_cast<int>(*n);
      } else if (name == "Resources") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          data.vec_resources.clear();
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              data.vec_resources.push_back(*s);
        }
      } else if (name == "FullyLoadedSpeed") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_fully_loaded_speed = static_cast<int>(*n);
      } else if (name == "SearchOnCreation") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_search_on_creation = *b;
      } else if (name == "SearchFromProcRadius") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_search_from_proc_radius = static_cast<int>(*n);
      } else if (name == "SearchFromHarvesterRadius") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_search_from_harvester_radius = static_cast<int>(*n);
      } else if (name == "WaitDuration") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_wait_duration = static_cast<int>(*n);
      } else if (name == "ResourceRefineryDirectionPenalty") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_resource_refinery_direction_penalty =
              static_cast<int>(*n);
      } else if (name == "QueueFullLoad") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_queue_full_load = *b;
      } else if (name == "EmptyCondition") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_empty_condition = *s;
      } else if (name == "HarvestVoice") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_harvest_voice = *s;
      } else if (name == "HarvestLineColor") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.color_harvest_line = core::Color::FromArgbRaw(
              static_cast<std::uint32_t>(*n));
      } else if (name == "HarvestCursor") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_harvest_cursor = *s;
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

void HarvesterInfoData::ValidateResources(
    const game::ActorInfo& info) const {

  if (vec_resources.empty())
    throw yaml::YamlException("Harvester.Resources is empty.");

  std::vector<std::string> vec_stored;
  const auto append_split = [&vec_stored](const std::string& str_list) {
    std::string str_current;
    for (const char ch : str_list + ",") {
      if (ch == ',') {
        const std::size_t begin = str_current.find_first_not_of(' ');
        const std::size_t end = str_current.find_last_not_of(' ');
        if (begin != std::string::npos)
          vec_stored.push_back(
              str_current.substr(begin, end - begin + 1));
        str_current.clear();
      } else {
        str_current.push_back(ch);
      }
    }
  };
  for (const meta::RecordObject* rec :
       info.TraitInfosByInterface("OpenRA.Traits.IStoresResourcesInfo")) {
    const meta::GenericValue* v_value = sim::RecordFieldValue(*rec, "Resources");
    if (v_value == nullptr)
      continue;
    if (const auto* str_types = std::get_if<std::string>(&v_value->val))
      append_split(*str_types);
    else if (const auto* list =
                 std::get_if<std::vector<meta::GenericValue>>(
                     &v_value->val))
      for (const auto& element : *list)
        if (const auto* s2 = std::get_if<std::string>(&element.val))
          append_split(*s2);
  }

  std::vector<std::string> vec_invalid;
  for (const std::string& resource : vec_resources)
    if (std::find(vec_stored.begin(), vec_stored.end(), resource) ==
        vec_stored.end())
      vec_invalid.push_back(resource);

  if (!vec_invalid.empty()) {
    std::string str_joined;
    for (const std::string& str_invalid : vec_invalid) {
      if (!str_joined.empty())
        str_joined += ',';
      str_joined += str_invalid;
    }
    throw yaml::YamlException(
        "Invalid Harvester.Resources types: " + str_joined + ".");
  }
}

Harvester::Harvester(ActorInitializer& init, const HarvesterInfoData& info)
    : DockClientBaseCore<Harvester>(init.Self(), info.conditional),
      info_{info} {

  for (sim::IStoresResources* sr :
       init.Self().TraitsImplementing<sim::IStoresResources>()) {
    bool b_has_type = false;
    for (const std::string& r : info_.vec_resources)
      if (sr->HasType(r)) {
        b_has_type = true;
        break;
      }
    if (b_has_type)
      vec_stores_resources_.push_back(sr);
  }
  p_resource_layer_ =
      init.Self().world().WorldActor()->Trait<sim::IResourceLayer>();
  p_claim_layer_ =
      init.Self().world().WorldActor()->Trait<ResourceClaimLayer>();
  p_self_ = &init.Self();
}

void Harvester::Created(Actor& self) {

  p_mobile_ = self.TraitOrDefault<Mobile>();
  UpdateCondition(self);

  if (info_.b_search_on_creation && p_mobile_ != nullptr)
    self.QueueActivity(activities::NewActivity<
                       activities::FindAndDeliverResources>(self));

  CoreCreated(self);
}

bool Harvester::IsFull() const {

  for (const sim::IStoresResources* sr : vec_stores_resources_)
    if (sr->ContentsSum() < sr->Capacity())
      return false;
  return true;
}

bool Harvester::IsEmpty() const {

  for (const sim::IStoresResources* sr : vec_stores_resources_)
    if (sr->ContentsSum() != 0)
      return false;
  return true;
}

int Harvester::Fullness() const {

  int sum = 0;
  for (const sim::IStoresResources* sr : vec_stores_resources_)
    sum += sr->ContentsSum() * 100 / sr->Capacity();
  return sum / static_cast<int>(vec_stores_resources_.size());
}

bool Harvester::CanDock(const core::BitSet<sim::DockType>& type,
                        bool force_enter) {

  return DockClientBaseCore<Harvester>::CanDock(type, force_enter) &&
         (force_enter || !IsEmpty());
}

bool Harvester::CanDockAt(Actor& host_actor, sim::IDockHost* host,
                          bool force_enter, bool ignore_occupancy) {

  return DockClientBaseCore<Harvester>::CanDockAt(host_actor, host,
                                                  force_enter,
                                                  ignore_occupancy) &&
         (p_self_->Owner() == host_actor.Owner() ||
          (ignore_occupancy &&
           p_self_->Owner()->IsAlliedWith(host_actor.Owner())));
}

bool Harvester::CanQueueDockAt(Actor& host_actor, sim::IDockHost* host,
                               bool force_enter, bool is_queued) {

  return DockClientBaseCore<Harvester>::CanQueueDockAt(
             host_actor, host, force_enter, is_queued) &&
         p_self_->Owner()->IsAlliedWith(host_actor.Owner());
}

void Harvester::UpdateCondition(Actor& self) {

  if (info_.str_empty_condition.empty())
    return;

  const bool b_enabled = IsEmpty();

  if (b_enabled && int4_condition_token_ == Actor::InvalidConditionToken)
    int4_condition_token_ =
        self.GrantCondition(info_.str_empty_condition);
  else if (!b_enabled &&
           int4_condition_token_ != Actor::InvalidConditionToken)
    int4_condition_token_ =
        self.RevokeCondition(int4_condition_token_);
}

void Harvester::AddResource(Actor& self,
                            const std::string& resource_type) {

  for (sim::IStoresResources* sr : vec_stores_resources_)
    if (sr->AddResource(resource_type, 1) == 0)
      break;

  UpdateCondition(self);
}

void Harvester::OnDockStarted(Actor& , Actor& host_actor,
                              sim::IDockHost* host) {

  if (DockClientBaseCore<Harvester>::CanDock(host->GetDockType()))
    p_accept_resources_ = host_actor.TraitOrDefault<sim::IAcceptResources>();
}

bool Harvester::OnDockTick(Actor& self, Actor& ,
                           sim::IDockHost* ) {

  if (p_accept_resources_ == nullptr || IsTraitDisabled())
    return true;

  if (--current_unload_ticks > 0)
    return false;

  for (sim::IStoresResources* sr : vec_stores_resources_) {
    for (const auto& [resource_type, count] : sr->Contents()) {
      const int unload_count =
          std::min(count, info_.int4_bale_unload_amount);
      const int accepted = p_accept_resources_->AcceptResources(
          *Self(), resource_type, unload_count);
      if (accepted == 0)
        continue;

      sr->RemoveResource(resource_type, accepted);
      current_unload_ticks = info_.int4_bale_unload_delay;
      UpdateCondition(self);
      return false;
    }
  }

  return IsEmpty();
}

void Harvester::OnDockCompleted(Actor& self, Actor& ,
                                sim::IDockHost* dock) {

  p_accept_resources_ = nullptr;

  if (GetDockType().Overlaps(dock->GetDockType())) {
    sim::Activity* current_activity = self.CurrentActivity();
    const activities::FindAndDeliverResources* current_find =
        dynamic_cast<const activities::FindAndDeliverResources*>(
            current_activity);
    if (current_activity == nullptr ||
        (current_find == nullptr &&
         current_activity->NextActivity() == nullptr))
      self.QueueActivity(true, activities::NewActivity<
                                   activities::FindAndDeliverResources>(
                                      self));
  }
}

bool Harvester::CanHarvestCell(CPos cell) const {

  if (cell.Layer() != 0)
    return false;

  const std::string str_resource_type =
      p_resource_layer_->GetResource(cell).str_type;
  if (str_resource_type.empty())
    return false;

  return std::find(info_.vec_resources.begin(), info_.vec_resources.end(),
                   str_resource_type) != info_.vec_resources.end();
}

std::vector<sim::IOrderTargeter*> Harvester::Orders() {

  std::vector<sim::IOrderTargeter*> vec_out;
  if (IsTraitDisabled() || p_mobile_ == nullptr)
    return vec_out;

  class HarvestOrderTargeter final : public sim::IOrderTargeter {
   public:
    std::string OrderID() const override { return "Harvest"; }
    int OrderPriority() const override { return 10; }
    bool IsQueued() const override { return b_is_queued_; }
    bool TargetOverridesSelection(
        Actor& , const sim::Target& ,
        std::span<Actor* const> , CPos ,
        sim::TargetModifiers ) override {
      return true;
    }
    bool CanTarget(Actor& , const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override {

      if (target.Type() != sim::TargetType::Terrain)
        return false;

      if (sim::HasModifier(modifiers, sim::TargetModifiers::ForceMove))
        return false;

      cursor.clear();
      return false;
    }

   private:
    bool b_is_queued_ = false;
  };

  ptr_harvest_order_targeter_ = std::make_unique<HarvestOrderTargeter>();
  vec_out.push_back(ptr_harvest_order_targeter_.get());
  return vec_out;
}

std::unique_ptr<net::Order> Harvester::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {

  if (order->OrderID() == "Harvest")
    return std::make_unique<net::Order>(order->OrderID(), &self, target,
                                        queued);

  return nullptr;
}

std::string Harvester::VoicePhraseForOrder(Actor& ,
                                           const net::Order& order) {

  if (order.str_order_string == "Harvest" && p_mobile_ != nullptr)
    return info_.str_harvest_voice;

  return {};
}

void Harvester::ResolveOrder(Actor& self, const net::Order& order) {

  if (order.str_order_string == "Harvest" && p_mobile_ != nullptr) {
    CPos loc;
    if (order.target.Type() != sim::TargetType::Invalid) {

      const CPos cell =
          self.world().Map().CellContaining(order.target.CenterPosition());
      loc = p_mobile_->NearestCell(
          cell,
          [this, &self](CPos p) {
            return p_mobile_->CanEnterCell(p, nullptr,
                                           sim::BlockedByActor::All) &&
                   p_claim_layer_->TryClaimCell(self, p);
          },
          1, 6);
    } else {

      loc = self.Location();
    }

    self.QueueActivity(
        order.b_queued,
        activities::NewActivity<activities::FindAndDeliverResources>(
            self, loc));
    self.ShowTargetLines();
  }
}

int Harvester::GetSpeedModifier() {

  return 100 - (100 - info_.int4_fully_loaded_speed) * Fullness() / 100;
}

void Harvester::TraitDisabledHook(Actor& self) {

  if (int4_condition_token_ != Actor::InvalidConditionToken)
    int4_condition_token_ =
        self.RevokeCondition(int4_condition_token_);
}

namespace {

int HarvesterSyncHash(const sim::ISync* s) {
  const auto* harv = static_cast<const Harvester*>(s);
  return sim::sync::CombineSyncHash(0, harv->current_unload_ticks);
}

const bool b_harvester_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Harvester",
                                &HarvesterSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_harvester_sync_registered_anchor =
    &b_harvester_sync_registered;

}

}
