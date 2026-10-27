#include "sim/frozen_actor_layer.hpp"

#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/sync_hash.hpp"
#include "sim/world.hpp"

namespace ora::sim {

FrozenActor::FrozenActor(Actor& actor, ICreatesFrozenActors& frozen_trait,
                         std::vector<PPos> vec_footprint, Player& viewer,
                         bool starts_revealed)
    : CenterPosition{actor.CenterPosition()},
      actor_{actor},
      frozen_trait_{frozen_trait},
      shroud_{*viewer.GetShroud()} {

  p_viewer_ = &viewer;
  b_need_renderables_ = starts_revealed;

  for (const PPos& m : vec_footprint)
    if (shroud_.Contains(m))
      Footprint.push_back(m);

  if (Footprint.empty()) {

    auto join_footprint = [this](const std::vector<PPos>& vec,
                                  bool as_contains) {
      std::string str_joined;
      for (std::size_t i = 0; i < vec.size(); i++) {
        if (i > 0)
          str_joined += "|";
        if (as_contains)
          str_joined += shroud_.Contains(vec[i]) ? "True" : "False";
        else
          str_joined +=
              std::format("{},{}", vec[i].U, vec[i].V);
      }
      return str_joined;
    };

    throw std::invalid_argument(std::format(
        "This frozen actor has no footprint.\n"
        "Actor Name: {}\n"
        "Actor Location: {},{}\n"
        "Input footprint: [{}]\n"
        "Input footprint (after shroud.Contains): [{}]",
        actor_.InfoName(), actor_.Location().X(), actor_.Location().Y(),
        join_footprint(vec_footprint, false),
        join_footprint(vec_footprint, true)));
  }

  vec_tooltips_ = actor.TraitsImplementing<ITooltip>();
  p_health_ = actor.TraitOrDefault<IHealth>();
  vec_visibility_modifiers_ = actor.TraitsImplementing<IVisibilityModifier>();

  UpdateVisibility();
}

std::uint32_t FrozenActor::ID() const {

  return actor_.ActorID();
}

const game::ActorInfo& FrozenActor::Info() const {

  return *actor_.Info();
}

Actor* FrozenActor::ActorPtr() const {

  return !actor_.IsDead() ? &actor_ : nullptr;
}

void FrozenActor::RefreshState() {

  p_owner_ = actor_.Owner();
  bitset_target_types_ = actor_.GetEnabledTargetTypes();
  vec_targetable_positions_.clear();
  for (const WPos& p : actor_.GetTargetablePositions())
    vec_targetable_positions_.push_back(p);

  if (p_health_ != nullptr) {
    int4_hp_ = p_health_->HP();
    damage_state_ = p_health_->DamageState();
  }

  for (ITooltip* tooltip : vec_tooltips_) {
    auto* tooltip_base = dynamic_cast<TraitBase*>(tooltip);
    if (tooltip_base != nullptr && !tooltip_base->IsTraitEnabled())
      continue;

    p_tooltip_info_ = tooltip->TooltipInfo();
    p_tooltip_owner_ = tooltip->Owner();
    break;
  }
}

void FrozenActor::RefreshHidden() {

  b_hidden_ = false;
  for (IVisibilityModifier* visibility_modifier :
       vec_visibility_modifiers_) {
    if (!visibility_modifier->IsVisible(actor_, p_viewer_)) {
      b_hidden_ = true;
      break;
    }
  }
}

void FrozenActor::Tick() {

  if (int4_flash_ticks_ > 0)
    int4_flash_ticks_--;

  if (b_update_visibility_next_tick_)
    UpdateVisibility();
}

void FrozenActor::UpdateVisibility() {

  b_update_visibility_next_tick_ = false;

  const bool was_visible = b_visible_;
  b_shrouded_ = true;
  b_visible_ = true;

  for (const PPos& puv : Footprint) {
    const CellVisibility cv = shroud_.GetVisibility(puv);
    if (HasCellVisibility(cv, CellVisibility::Visible)) {
      b_visible_ = false;
      b_shrouded_ = false;
      break;
    }

    if (b_shrouded_ && HasCellVisibility(cv, CellVisibility::Explored))
      b_shrouded_ = false;
  }

  if (b_visible_ != was_visible)
    frozen_trait_.OnVisibilityChanged(*this);

  b_need_renderables_ = b_need_renderables_ || (b_visible_ && !was_visible);
}

void FrozenActor::Invalidate() {

  p_owner_ = nullptr;
}

void FrozenActor::Flash(core::Color color, float alpha) {

  int4_flash_ticks_ = 5;
  [[maybe_unused]] const auto& tint = color;
  [[maybe_unused]] const float f = alpha;
}

void FrozenActor::Flash(core::Color tint) {

  int4_flash_ticks_ = 5;
  [[maybe_unused]] const auto& t = tint;
}

FrozenActorLayer::FrozenActorLayer(Actor& self,
                                   const FrozenActorLayerInfoData& info)
    : int4_bin_size_{info.int4_bin_size},
      world_{self.world()},
      p_owner_{self.Owner()},
      partitioned_frozen_actors_{world_.Map().MapSize().Width,
                                 world_.Map().MapSize().Height,
                                 int4_bin_size_} {

  self.Trait<Shroud>()->AddShroudChangedCallback(
      [this](PPos uv) {
        for (FrozenActor* fa : partitioned_frozen_actors_.At(
                 int2{uv.U, uv.V}))
          fa->SetUpdateVisibilityNextTick(true);
      });
}

void FrozenActorLayer::Add(FrozenActor* fa) {

  map_frozen_actors_by_id_.emplace(fa->ID(), fa);
  world_.ScreenMapFace()->AddOrUpdateFrozen(p_owner_, fa);
  partitioned_frozen_actors_.Add(fa, FootprintBounds(*fa));
}

void FrozenActorLayer::Remove(FrozenActor* fa) {

  partitioned_frozen_actors_.Remove(fa);
  world_.ScreenMapFace()->RemoveFrozen(p_owner_, fa);
  map_frozen_actors_by_id_.erase(fa->ID());
}

FrozenActor* FrozenActorLayer::FromID(std::uint32_t id) {

  const auto it = map_frozen_actors_by_id_.find(id);
  return it != map_frozen_actors_by_id_.end() ? it->second : nullptr;
}

std::vector<FrozenActor*> FrozenActorLayer::FrozenActorsInRegion(
    const map::CellRegion& region, bool only_visible) {

  const CPos tl = region.TopLeft();
  const CPos br = region.BottomRight();
  std::vector<FrozenActor*> vec_result;
  for (FrozenActor* fa : partitioned_frozen_actors_.InBox(
           Rectangle::FromLTRB(tl.X(), tl.Y(), br.X(), br.Y())))
    if (fa->IsValid() && (!only_visible || fa->Visible()))
      vec_result.push_back(fa);
  return vec_result;
}

std::vector<FrozenActor*> FrozenActorLayer::FrozenActorsInCircle(
    World& world, const WPos& origin, const WDist& r, bool only_visible) {

  const CPos center_cell = world.Map().CellContaining(origin);
  const int cell_range = (r.Length + 1023) / 1024;
  const CPos tl = center_cell - CVec{cell_range, cell_range};
  const CPos br = center_cell + CVec{cell_range, cell_range};

  std::vector<FrozenActor*> vec_result;
  for (FrozenActor* fa : partitioned_frozen_actors_.InBox(Rectangle::FromLTRB(
           tl.X(), tl.Y(), br.X(), br.Y())))
    if (fa->IsValid() && (!only_visible || fa->Visible()) &&
        (fa->CenterPosition - origin).HorizontalLengthSquared() <=
            r.LengthSquared())
      vec_result.push_back(fa);
  return vec_result;
}

Rectangle FrozenActorLayer::FootprintBounds(const FrozenActor& fa) {

  const PPos& p1 = fa.Footprint[0];
  int min_u = p1.U;
  int max_u = p1.U;
  int min_v = p1.V;
  int max_v = p1.V;
  for (const PPos& p : fa.Footprint) {
    if (min_u > p.U)
      min_u = p.U;
    else if (max_u < p.U)
      max_u = p.U;

    if (min_v > p.V)
      min_v = p.V;
    else if (max_v < p.V)
      max_v = p.V;
  }

  return Rectangle::FromLTRB(min_u, min_v, max_u + 1, max_v + 1);
}

void FrozenActorLayer::Tick(Actor& ) {

  std::vector<FrozenActor*> vec_frozen_actors_to_remove;
  VisibilityHash = 0;
  FrozenHash = 0;

  for (const auto& [id, frozen_actor] : map_frozen_actors_by_id_) {
    const int hash = static_cast<int>(id);
    FrozenHash += hash;

    frozen_actor->Tick();

    if (frozen_actor->Visible())
      VisibilityHash += hash;
    else if (frozen_actor->ActorPtr() == nullptr)
      vec_frozen_actors_to_remove.push_back(frozen_actor);
  }

  for (FrozenActor* fa : vec_frozen_actors_to_remove)
    Remove(fa);
}

}

namespace ora::sim {

int FrozenActorLayerSyncHash(const ISync* s) {
  const auto* layer = static_cast<const FrozenActorLayer*>(s);
  return sync::CombineSyncHash(
      sync::CombineSyncHash(0, layer->VisibilityHash), layer->FrozenHash);
}

const bool b_frozen_actor_layer_sync_registered = [] {
  RegisterSyncHashFunction("OpenRA.Traits.FrozenActorLayer",
                           &FrozenActorLayerSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_frozen_actor_layer_sync_registered_anchor =
    &b_frozen_actor_layer_sync_registered;

}
