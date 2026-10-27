#pragma once
import std;

#include "core/bitset.hpp"
#include "core/polygon.hpp"
#include "core/rectangle.hpp"
#include "core/wpos.hpp"
#include "map/cell_region.hpp"
#include "sim/shroud.hpp"
#include "sim/spatially_partitioned.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::gfx {
struct IRenderable;

}

namespace ora::sim {

class FrozenActor;

class Actor;
class Player;
class World;

class ICreatesFrozenActors {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Traits_ICreatesFrozenActors;
  virtual ~ICreatesFrozenActors() = default;
  virtual void OnVisibilityChanged(FrozenActor& frozen) = 0;
};

struct FrozenActorLayerInfoData {
  int int4_bin_size = 10;
};

class FrozenActor final {
 public:
  FrozenActor(Actor& actor, ICreatesFrozenActors& frozen_trait,
              std::vector<PPos> vec_footprint, Player& viewer,
              bool starts_revealed);

  std::vector<PPos> Footprint;
  const WPos CenterPosition;

  std::uint32_t ID() const;
  bool IsValid() const { return p_owner_ != nullptr; }
  const game::ActorInfo& Info() const;
  Actor* ActorPtr() const;

  Player* Viewer() const { return p_viewer_; }
  Player* Owner() const { return p_owner_; }
  core::BitSet<TargetableType> TargetTypes() const {
    return bitset_target_types_;
  }
  const std::vector<WPos>& TargetablePositions() const {
    return vec_targetable_positions_;
  }

  const ITooltipInfo* TooltipInfo() const {
    return p_tooltip_info_;
  }
  Player* TooltipOwner() const { return p_tooltip_owner_; }

  int HP() const { return int4_hp_; }
  enum DamageState DamageState() const {

    return damage_state_;
  }

  bool Visible() const { return b_visible_; }
  bool Hidden() const { return b_hidden_; }

  bool Shrouded() const { return b_shrouded_; }
  bool NeedRenderables() const { return b_need_renderables_; }
  void SetNeedRenderables(bool b) { b_need_renderables_ = b; }
  bool HasRenderables() const { return Renderables != nullptr; }
  bool UpdateVisibilityNextTick() const {
    return b_update_visibility_next_tick_;
  }
  void SetUpdateVisibilityNextTick(bool b) {
    b_update_visibility_next_tick_ = b;
  }

  const std::vector<gfx::IRenderable*>* Renderables = nullptr;
  const std::vector<Rectangle>* ScreenBoundsList = nullptr;
  Polygon MouseBounds{Polygon::Empty()};

  void RefreshState();
  void RefreshHidden();
  void Tick();
  void Invalidate();
  void Flash(core::Color color, float alpha);
  void Flash(core::Color tint);

 private:

  void UpdateVisibility();

  Actor& actor_;
  ICreatesFrozenActors& frozen_trait_;
  Shroud& shroud_;
  std::vector<WPos> vec_targetable_positions_;

  Player* p_viewer_ = nullptr;
  Player* p_owner_ = nullptr;
  core::BitSet<TargetableType> bitset_target_types_;
  const ITooltipInfo* p_tooltip_info_ = nullptr;
  Player* p_tooltip_owner_ = nullptr;
  std::vector<ITooltip*> vec_tooltips_;
  IHealth* p_health_ = nullptr;
  int int4_hp_ = 0;
  enum DamageState damage_state_ = DamageState::Undamaged;
  std::vector<IVisibilityModifier*> vec_visibility_modifiers_;

  bool b_visible_ = true;
  bool b_hidden_ = false;
  bool b_shrouded_ = false;
  bool b_need_renderables_ = false;
  bool b_update_visibility_next_tick_ = false;

  int int4_flash_ticks_ = 0;
};

class FrozenActorLayer final : public TraitBase, public ITick, public ISync {
 public:
  FrozenActorLayer(Actor& self, const FrozenActorLayerInfoData& info);

  ORA_TRAIT_INTERFACES(FrozenActorLayer, OpenRA_Traits_FrozenActorLayer,
                       ITick, ISync)

  int VisibilityHash = 0;
  int FrozenHash = 0;

  void Tick(Actor& self) override;

  void Add(FrozenActor* fa);
  void Remove(FrozenActor* fa);

  FrozenActor* FromID(std::uint32_t id);

  std::vector<FrozenActor*> FrozenActorsInRegion(
      const map::CellRegion& region, bool only_visible = true);

  std::vector<FrozenActor*> FrozenActorsInCircle(World& world,
                                                 const WPos& origin,
                                                 const WDist& r,
                                                 bool only_visible = true);

 private:

  static Rectangle FootprintBounds(const FrozenActor& fa);

  int int4_bin_size_;
  World& world_;
  Player* p_owner_ = nullptr;
  std::map<std::uint32_t, FrozenActor*> map_frozen_actors_by_id_;
  SpatiallyPartitioned<FrozenActor*> partitioned_frozen_actors_;
};

}
