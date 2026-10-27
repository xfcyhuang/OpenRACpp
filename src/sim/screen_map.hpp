#pragma once
import std;

#include "core/polygon.hpp"
#include "core/size.hpp"
#include "gfx/sprite.hpp"
#include "sim/actor.hpp"
#include "sim/effects.hpp"
#include "sim/spatially_partitioned.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {

class Player;

class FrozenActor;

struct ActorBoundsPair {
  Actor* ptr_actor = nullptr;
  Polygon bounds{Polygon::Empty()};

  std::string ToString() const;
};

class ScreenMap final : public TraitBase, public IWorldLoaded {
 public:
  static constexpr int kDefaultBinSize = 250;

  ScreenMap(World& world, int bin_size);

  ORA_TRAIT_INTERFACES(ScreenMap, OpenRA_Traits_ScreenMap, IWorldLoaded)

  void WorldLoaded(World& world, gfx::WorldRenderer* wr) override;

  void AddOrUpdate(Actor* a);
  void Remove(Actor* a);

  void Add(IEffect* effect, const WPos& position, const Size& size);
  void Add(IEffect* effect, const WPos& position, const gfx::Sprite& sprite);
  void Update(IEffect* effect, const WPos& position, const Size& size);
  void Update(IEffect* effect, const WPos& position, const gfx::Sprite& sprite);
  void Remove(IEffect* effect);

  void AddOrUpdateFrozen(Player* viewer, FrozenActor* fa);
  void RemoveFrozen(Player* viewer, FrozenActor* fa);
  std::vector<FrozenActor*> FrozenActorsAtMouse(Player* viewer,
                                                int2 world_px);
  std::vector<FrozenActor*> RenderableFrozenActorsInBox(Player* p, int2 a,
                                                        int2 b);

  std::vector<ActorBoundsPair> ActorsAtMouse(int2 world_px);
  std::vector<ActorBoundsPair> ActorsInMouseBox(int2 a, int2 b);
  std::vector<ActorBoundsPair> ActorsInMouseBox(Rectangle r);

  std::vector<Actor*> RenderableActorsInBox(int2 a, int2 b);
  std::vector<IEffect*> RenderableEffectsInBox(int2 a, int2 b);

  void TickRender();

  std::vector<Rectangle> RenderBounds() const;
  std::vector<Polygon> MouseBounds() const;
  std::vector<Rectangle> RenderBounds(Player* viewer) const;
  std::vector<Polygon> MouseBounds(Player* viewer) const;

  void SetActorBoundsSources(
      std::function<Polygon(Actor&)> fn_mouse_bounds,
      std::function<std::vector<Rectangle>(Actor&)> fn_screen_bounds) {
    fn_actor_mouse_bounds_ = std::move(fn_mouse_bounds);
    fn_actor_screen_bounds_ = std::move(fn_screen_bounds);
  }

 private:
  static bool ValidBounds(Rectangle bounds) {
    return bounds.Width > 0 && bounds.Height > 0;
  }
  static Rectangle RectWithCorners(int2 a, int2 b) {
    return Rectangle::FromLTRB(std::min(a.X, b.X), std::min(a.Y, b.Y),
                               std::max(a.X, b.X), std::max(a.Y, b.Y));
  }

  int bin_size_;
  int width_;

  int height_;
  SpatiallyPartitioned<Actor*> partitioned_mouse_actors_;
  SpatiallyPartitioned<Actor*> partitioned_renderable_actors_;
  SpatiallyPartitioned<IEffect*> partitioned_renderable_effects_;

  struct FrozenPlayerCaches {
    SpatiallyPartitioned<FrozenActor*> partitioned_mouse;
    SpatiallyPartitioned<FrozenActor*> partitioned_renderable;
    std::vector<FrozenActor*> vec_add_or_update;
    std::vector<FrozenActor*> vec_remove;
  };
  FrozenPlayerCaches& FrozenCachesOf(Player* viewer);

  std::unordered_map<Actor*, ActorBoundsPair> map_partitioned_mouse_actor_bounds_;
  std::unordered_map<Player*, FrozenPlayerCaches> map_frozen_caches_;

  std::vector<Actor*> vec_add_or_update_actors_;
  std::vector<Actor*> vec_remove_actors_;

  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;

  std::function<Polygon(Actor&)> fn_actor_mouse_bounds_;
  std::function<std::vector<Rectangle>(Actor&)> fn_actor_screen_bounds_;
};

}
