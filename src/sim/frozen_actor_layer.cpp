// UPSTREAM: OpenRA.Game/Traits/Player/FrozenActorLayer.cs @b6fc03f L19-386
//          实现部分(the implementation half)。
#include "sim/frozen_actor_layer.hpp"

#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/sync_hash.hpp"
#include "sim/world.hpp"

namespace ora::sim {

// ———— FrozenActor(L36-244)————
// ———— FrozenActor (L36-244) ————

FrozenActor::FrozenActor(Actor& actor, ICreatesFrozenActors& frozen_trait,
                         std::vector<PPos> vec_footprint, Player& viewer,
                         bool starts_revealed)
    : CenterPosition{actor.CenterPosition()},
      actor_{actor},
      frozen_trait_{frozen_trait},
      shroud_{*viewer.GetShroud()} {
  // L87-114
  p_viewer_ = &viewer;
  b_need_renderables_ = starts_revealed;

  // Consider all cells inside the map area (ignoring the current map
  // bounds)(上游注释)
  for (const PPos& m : vec_footprint)
    if (shroud_.Contains(m))
      Footprint.push_back(m);

  if (Footprint.empty()) {
    // 上游异常文本逐字(PPos/CPos ToString + JoinWith("|") 形态)
    // Upstream's exception text verbatim (the PPos/CPos ToString +
    // JoinWith("|") shape).
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
  // L116
  return actor_.ActorID();
}

const game::ActorInfo& FrozenActor::Info() const {
  // L118
  return *actor_.Info();
}

Actor* FrozenActor::ActorPtr() const {
  // L119
  return !actor_.IsDead() ? &actor_ : nullptr;
}

void FrozenActor::RefreshState() {
  // L121-140
  p_owner_ = actor_.Owner();
  bitset_target_types_ = actor_.GetEnabledTargetTypes();
  vec_targetable_positions_.clear();
  for (const WPos& p : actor_.GetTargetablePositions())
    vec_targetable_positions_.push_back(p);

  if (p_health_ != nullptr) {
    int4_hp_ = p_health_->HP();
    damage_state_ = p_health_->DamageState();
  }

  // FirstEnabledTraitOrDefault(首个非禁用;ITooltip 无使能面 →
  // TraitBase 子对象判)
  // FirstEnabledTraitOrDefault (the first non-disabled one; ITooltip
  // carries no enablement face → the TraitBase subobject check).
  for (ITooltip* tooltip : vec_tooltips_) {
    auto* tooltip_base = dynamic_cast<TraitBase*>(tooltip);
    if (tooltip_base != nullptr && !tooltip_base->IsTraitEnabled())
      continue;

    // TooltipInfo/TooltipOwner 的非空面随具体 Tooltip trait 批;空
    // ITooltipInfo 载荷保持 null(ITooltip 实现者自报)
    // The non-null TooltipInfo/TooltipOwner faces ride the concrete
    // Tooltip trait batch; the empty ITooltipInfo payload stays null
    // (the ITooltip implementor reports it).
    p_tooltip_info_ = tooltip->TooltipInfo();
    p_tooltip_owner_ = tooltip->Owner();
    break;
  }
}

void FrozenActor::RefreshHidden() {
  // L142-153
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
  // L155-162
  if (int4_flash_ticks_ > 0)
    int4_flash_ticks_--;

  if (b_update_visibility_next_tick_)
    UpdateVisibility();
}

void FrozenActor::UpdateVisibility() {
  // L164-193
  b_update_visibility_next_tick_ = false;

  const bool was_visible = b_visible_;
  b_shrouded_ = true;
  b_visible_ = true;

  // PERF: Avoid LINQ.(上游注释)
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

  // Force the backing trait to update so other actors can't
  // query inconsistent state (both hidden or both visible)(上游注释)
  if (b_visible_ != was_visible)
    frozen_trait_.OnVisibilityChanged(*this);

  b_need_renderables_ = b_need_renderables_ || (b_visible_ && !was_visible);
}

void FrozenActor::Invalidate() {
  // L195-198
  p_owner_ = nullptr;
}

void FrozenActor::Flash(core::Color color, float alpha) {
  // L200-206(flash 的渲染面随渲染批;tick 计数保留)
  // L200-206 (Flash's render face rides the render batch; the tick counter
  // stays).
  int4_flash_ticks_ = 5;
  [[maybe_unused]] const auto& tint = color;
  [[maybe_unused]] const float f = alpha;
}

void FrozenActor::Flash(core::Color tint) {
  // L208-214
  int4_flash_ticks_ = 5;
  [[maybe_unused]] const auto& t = tint;
}

// ———— FrozenActorLayer(L246-385)————
// ———— FrozenActorLayer (L246-385) ————

FrozenActorLayer::FrozenActorLayer(Actor& self,
                                   const FrozenActorLayerInfoData& info)
    : int4_bin_size_{info.int4_bin_size},
      world_{self.world()},
      p_owner_{self.Owner()},
      partitioned_frozen_actors_{world_.Map().MapSize().Width,
                                 world_.Map().MapSize().Height,
                                 int4_bin_size_} {
  // L260-275:Shroud.OnShroudChanged 订阅(+= → 回调表;self.Trait<Shroud>
  // = 上游取法 —— Player ctor 期 PlayerActor 尚未回挂,不可走 Player 面)
  // L260-275: the Shroud.OnShroudChanged subscription (+= → the callback
  // list; self.Trait<Shroud> is upstream's own lookup — the PlayerActor
  // back-link is not yet assigned during the Player ctor, so the Player
  // face is unusable here).
  self.Trait<Shroud>()->AddShroudChangedCallback(
      [this](PPos uv) {
        for (FrozenActor* fa : partitioned_frozen_actors_.At(
                 int2{uv.U, uv.V}))
          fa->SetUpdateVisibilityNextTick(true);
      });
}

void FrozenActorLayer::Add(FrozenActor* fa) {
  // L277-282
  map_frozen_actors_by_id_.emplace(fa->ID(), fa);
  world_.ScreenMapFace()->AddOrUpdateFrozen(p_owner_, fa);
  partitioned_frozen_actors_.Add(fa, FootprintBounds(*fa));
}

void FrozenActorLayer::Remove(FrozenActor* fa) {
  // L284-289
  partitioned_frozen_actors_.Remove(fa);
  world_.ScreenMapFace()->RemoveFrozen(p_owner_, fa);
  map_frozen_actors_by_id_.erase(fa->ID());
}

FrozenActor* FrozenActorLayer::FromID(std::uint32_t id) {
  // L356-362
  const auto it = map_frozen_actors_by_id_.find(id);
  return it != map_frozen_actors_by_id_.end() ? it->second : nullptr;
}

std::vector<FrozenActor*> FrozenActorLayer::FrozenActorsInRegion(
    const map::CellRegion& region, bool only_visible) {
  // L364-370(tl/br = CPos 域;FromLTRB 取 X/Y)
  // L364-370 (tl/br in the CPos domain; FromLTRB takes X/Y).
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
  // L372-384
  const CPos center_cell = world.Map().CellContaining(origin);
  const int cell_range = (r.Length + 1023) / 1024;
  const CPos tl = center_cell - CVec{cell_range, cell_range};
  const CPos br = center_cell + CVec{cell_range, cell_range};

  // Target ranges are calculated in 2D, so ignore height differences
  // (上游注释)
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
  // L291-312
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

void FrozenActorLayer::Tick(Actor& /*self*/) {
  // L314-341
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

}  // namespace ora::sim

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:FrozenActorLayer
//      {VisibilityHash, FrozenHash})————
// ———— The [VerifySync] hash registration (the gen/sync_gen.cpp member
//      table: FrozenActorLayer {VisibilityHash, FrozenHash}) ————
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

}  // namespace ora::sim
