// UPSTREAM: OpenRA.Game/Traits/World/ScreenMap.cs @b6fc03f(screen_map.hpp
//          的实现) | The implementation of screen_map.hpp.
import std;

#include "sim/screen_map.hpp"

#include "game/ruleset.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/world.hpp"

namespace ora::sim {

/// 分区宽高(L64-66:MapSize × TileSize)| the partition dimensions
/// (L64-66: MapSize × TileSize).
int ScreenMapBinWidth(World& world);
int ScreenMapBinHeight(World& world);

ScreenMap::ScreenMap(World& world, int bin_size)
    : bin_size_{bin_size},
      width_{ScreenMapBinWidth(world)},
      height_{ScreenMapBinHeight(world)},
      partitioned_mouse_actors_{ScreenMapBinWidth(world), ScreenMapBinHeight(world), bin_size},
      partitioned_renderable_actors_{ScreenMapBinWidth(world), ScreenMapBinHeight(world), bin_size},
      partitioned_renderable_effects_{ScreenMapBinWidth(world), ScreenMapBinHeight(world), bin_size} {}

void ScreenMap::WorldLoaded(World& world, gfx::WorldRenderer* wr) {
  ptr_world_renderer_ = wr;
}

void ScreenMap::AddOrUpdate(Actor* a) {
  // L96-101:removeActors 先摘,addOrUpdateActors 去重并入
  std::erase(vec_remove_actors_, a);
  if (std::find(vec_add_or_update_actors_.begin(),
                vec_add_or_update_actors_.end(),
                a) == vec_add_or_update_actors_.end())
    vec_add_or_update_actors_.push_back(a);
}

void ScreenMap::Remove(Actor* a) {
  // L103-106
  if (std::find(vec_remove_actors_.begin(), vec_remove_actors_.end(), a) ==
      vec_remove_actors_.end())
    vec_remove_actors_.push_back(a);
}

void ScreenMap::Add(IEffect* effect, const WPos& position, const Size& size) {
  // L108-116
  if (ptr_world_renderer_ == nullptr)
    return;  // WorldLoaded 前无屏幕面(上游 worldRenderer null 时 NRE;
             // 测试注入序护栏)
  const int2 screen_pos = ptr_world_renderer_->ScreenPxPosition(position);
  const int screen_width = std::abs(size.Width);
  const int screen_height = std::abs(size.Height);
  const Rectangle screen_bounds = Rectangle::FromLTRB(
      screen_pos.X - screen_width / 2, screen_pos.Y - screen_height / 2,
      screen_pos.X - screen_width / 2 + screen_width,
      screen_pos.Y - screen_height / 2 + screen_height);
  if (ValidBounds(screen_bounds))
    partitioned_renderable_effects_.Add(effect, screen_bounds);
}

void ScreenMap::Add(IEffect* effect, const WPos& position,
                    const gfx::Sprite& sprite) {
  // L118-122
  const Size size{static_cast<int>(sprite.vec_size.X),
                  static_cast<int>(sprite.vec_size.Y)};
  Add(effect, position, size);
}

void ScreenMap::Update(IEffect* effect, const WPos& position,
                       const Size& size) {
  // L124-128
  Remove(effect);
  Add(effect, position, size);
}

void ScreenMap::Update(IEffect* effect, const WPos& position,
                       const gfx::Sprite& sprite) {
  // L130-134
  const Size size{static_cast<int>(sprite.vec_size.X),
                  static_cast<int>(sprite.vec_size.Y)};
  Update(effect, position, size);
}

void ScreenMap::Remove(IEffect* effect) {
  // L136-139
  partitioned_renderable_effects_.Remove(effect);
}

std::vector<ActorBoundsPair> ScreenMap::ActorsAtMouse(int2 world_px) {
  // L162-168:At → 在场过滤 → bounds 取对 → 多边形包含过滤
  std::vector<ActorBoundsPair> out;
  for (Actor* a : partitioned_mouse_actors_.At(world_px)) {
    if (!a->IsInWorld())
      continue;
    const ActorBoundsPair& pair = map_partitioned_mouse_actor_bounds_.at(a);
    if (pair.bounds.Contains(world_px))
      out.push_back(pair);
  }
  return out;
}

std::vector<ActorBoundsPair> ScreenMap::ActorsInMouseBox(int2 a, int2 b) {
  // L180-183
  return ActorsInMouseBox(RectWithCorners(a, b));
}

std::vector<ActorBoundsPair> ScreenMap::ActorsInMouseBox(Rectangle r) {
  // L185-191
  std::vector<ActorBoundsPair> out;
  for (Actor* a : partitioned_mouse_actors_.InBox(r)) {
    if (!a->IsInWorld())
      continue;
    const ActorBoundsPair& pair = map_partitioned_mouse_actor_bounds_.at(a);
    if (pair.bounds.IntersectsWith(r))
      out.push_back(pair);
  }
  return out;
}

std::vector<Actor*> ScreenMap::RenderableActorsInBox(int2 a, int2 b) {
  // L193-196
  std::vector<Actor*> out;
  for (Actor* a2 : partitioned_renderable_actors_.InBox(RectWithCorners(a, b)))
    if (a2->IsInWorld())
      out.push_back(a2);
  return out;
}

std::vector<IEffect*> ScreenMap::RenderableEffectsInBox(int2 a, int2 b) {
  // L198-201
  return partitioned_renderable_effects_.InBox(RectWithCorners(a, b));
}

// ———— FrozenActor 面(L84-94/146-160/203-209)————
// ———— The FrozenActor faces (L84-94/146-160/203-209) ————

ScreenMap::FrozenPlayerCaches& ScreenMap::FrozenCachesOf(Player* viewer) {
  // Cache<Player,…> 惰性构造(_ => new SpatiallyPartitioned(…))
  // The Cache<Player,…> lazy construction (_ => new
  // SpatiallyPartitioned(…)).
  const auto it = map_frozen_caches_.find(viewer);
  if (it != map_frozen_caches_.end())
    return it->second;

  auto [inserted, _] = map_frozen_caches_.emplace(
      viewer,
      FrozenPlayerCaches{
          SpatiallyPartitioned<FrozenActor*>{width_, height_, bin_size_},
          SpatiallyPartitioned<FrozenActor*>{width_, height_, bin_size_},
          {}, {}});
  return inserted->second;
}

void ScreenMap::AddOrUpdateFrozen(Player* viewer, FrozenActor* fa) {
  // L84-89
  auto& caches = FrozenCachesOf(viewer);
  std::erase(caches.vec_remove, fa);
  if (std::find(caches.vec_add_or_update.begin(),
                caches.vec_add_or_update.end(),
                fa) == caches.vec_add_or_update.end())
    caches.vec_add_or_update.push_back(fa);
}

void ScreenMap::RemoveFrozen(Player* viewer, FrozenActor* fa) {
  // L91-94
  auto& caches = FrozenCachesOf(viewer);
  if (std::find(caches.vec_remove.begin(), caches.vec_remove.end(), fa) ==
      caches.vec_remove.end())
    caches.vec_remove.push_back(fa);
}

std::vector<FrozenActor*> ScreenMap::FrozenActorsAtMouse(Player* viewer,
                                                         int2 world_px) {
  // L146-155(MouseInput 重载随 UI 输入装配批)
  // L146-155 (the MouseInput overload lands with the UI-input assembly
  // batch).
  if (viewer == nullptr)
    return {};

  std::vector<FrozenActor*> out;
  for (FrozenActor* fa : FrozenCachesOf(viewer).partitioned_mouse.At(world_px))
    if (fa->IsValid() && fa->MouseBounds.Contains(world_px))
      out.push_back(fa);
  return out;
}

std::vector<FrozenActor*> ScreenMap::RenderableFrozenActorsInBox(Player* p,
                                                                 int2 a,
                                                                 int2 b) {
  // L203-209
  if (p == nullptr)
    return {};

  std::vector<FrozenActor*> out;
  for (FrozenActor* fa :
       FrozenCachesOf(p).partitioned_renderable.InBox(RectWithCorners(a, b)))
    if (fa->IsValid())
      out.push_back(fa);
  return out;
}

void ScreenMap::TickRender() {
  // L211-271(Actor 面 + FrozenActor 面)
  for (Actor* a : vec_add_or_update_actors_) {
    const Polygon mouse_bounds =
        fn_actor_mouse_bounds_ ? fn_actor_mouse_bounds_(*a) : Polygon::Empty();
    if (!mouse_bounds.IsEmpty()) {
      partitioned_mouse_actors_.Set(a, mouse_bounds.BoundingRect);
      map_partitioned_mouse_actor_bounds_[a] =
          ActorBoundsPair{a, mouse_bounds};
    } else {
      partitioned_mouse_actors_.Remove(a);
    }

    const std::vector<Rectangle> screen_rects =
        fn_actor_screen_bounds_ ? fn_actor_screen_bounds_(*a)
                                : std::vector<Rectangle>{};
    Rectangle screen_union = Rectangle::FromLTRB(0, 0, 0, 0);
    if (!screen_rects.empty()) {
      screen_union = screen_rects.front();
      for (const Rectangle& r : screen_rects)
        screen_union = Rectangle::Union(screen_union, r);
    }
    // 上游 screenBounds.Size.IsEmpty 判定(Rectangle.Union 后的 Size)
    if (screen_union.Width != 0 || screen_union.Height != 0)
      partitioned_renderable_actors_.Set(a, screen_union);
    else
      partitioned_renderable_actors_.Remove(a);
  }

  for (Actor* a : vec_remove_actors_) {
    partitioned_mouse_actors_.Remove(a);
    map_partitioned_mouse_actor_bounds_.erase(a);
    partitioned_renderable_actors_.Remove(a);
  }

  vec_add_or_update_actors_.clear();
  vec_remove_actors_.clear();

  // ———— FrozenActor 半(L231-258)————
  // ———— The FrozenActor half (L231-258) ————
  for (auto& [viewer, caches] : map_frozen_caches_) {
    for (FrozenActor* fa : caches.vec_add_or_update) {
      if (!fa->MouseBounds.IsEmpty())
        caches.partitioned_mouse.Set(fa, fa->MouseBounds.BoundingRect);
      else
        caches.partitioned_mouse.Remove(fa);

      const std::vector<Rectangle>* screen_rects = fa->ScreenBoundsList;
      Rectangle screen_union = Rectangle::FromLTRB(0, 0, 0, 0);
      if (screen_rects != nullptr && !screen_rects->empty()) {
        screen_union = screen_rects->front();
        for (const Rectangle& r : *screen_rects)
          screen_union = Rectangle::Union(screen_union, r);
      }
      if (screen_union.Width != 0 || screen_union.Height != 0)
        caches.partitioned_renderable.Set(fa, screen_union);
      else
        caches.partitioned_renderable.Remove(fa);
    }

    caches.vec_add_or_update.clear();
  }

  for (auto& [viewer, caches] : map_frozen_caches_) {
    for (FrozenActor* fa : caches.vec_remove) {
      caches.partitioned_mouse.Remove(fa);
      caches.partitioned_renderable.Remove(fa);
    }

    caches.vec_remove.clear();
  }
}

std::vector<Rectangle> ScreenMap::RenderBounds() const {
  // L273-279(viewer == nullptr 面)
  // L273-279 (the viewer == nullptr face).
  std::vector<Rectangle> out = partitioned_renderable_actors_.Values();
  const std::vector<Rectangle> effects = partitioned_renderable_effects_.Values();
  out.insert(out.end(), effects.begin(), effects.end());
  return out;
}

std::vector<Polygon> ScreenMap::MouseBounds() const {
  // L281-285(viewer == nullptr 面)
  // L281-285 (the viewer == nullptr face).
  std::vector<Polygon> out;
  for (const auto& [_, pair] : map_partitioned_mouse_actor_bounds_)
    out.push_back(pair.bounds);
  return out;
}

std::vector<Rectangle> ScreenMap::RenderBounds(Player* viewer) const {
  // L273-279(viewer 面:FrozenActor 项并入)
  // L273-279 (the viewer face: the FrozenActor items merge in).
  std::vector<Rectangle> out = RenderBounds();
  if (viewer != nullptr) {
    const auto it = map_frozen_caches_.find(viewer);
    if (it != map_frozen_caches_.end()) {
      const std::vector<Rectangle> frozen =
          it->second.partitioned_renderable.Values();
      out.insert(out.end(), frozen.begin(), frozen.end());
    }
  }
  return out;
}

std::vector<Polygon> ScreenMap::MouseBounds(Player* viewer) const {
  // L281-285(viewer 面)
  // L281-285 (the viewer face).
  std::vector<Polygon> out = MouseBounds();
  if (viewer != nullptr) {
    const auto it = map_frozen_caches_.find(viewer);
    if (it != map_frozen_caches_.end())
      for (const FrozenActor* fa : it->second.partitioned_mouse.Keys())
        out.push_back(fa->MouseBounds);
  }
  return out;
}

int ScreenMapBinWidth(World& world) {
  // L64-65:width = MapSize.Width × TileSize.Width
  return world.Map().MapSize().Width *
         world.Map().Rules().TerrainInfo().TileSize().Width;
}

int ScreenMapBinHeight(World& world) {
  return world.Map().MapSize().Height *
         world.Map().Rules().TerrainInfo().TileSize().Height;
}

}  // namespace ora::sim
