// UPSTREAM: OpenRA.Game/Traits/World/ScreenMap.cs @b6fc03f L21-287(逐语义
//          重写;FrozenActor 面随 FrozenActorLayer 批落地)
//          Verbatim-semantics rewrite; the FrozenActor faces land with the
//          FrozenActorLayer batch.
//
// 机制对照 / Mechanism mapping:
//  - SpatiallyPartitioned<Actor> → OPT-A8 slab 数组化(spatially_partitioned.hpp)
//    SpatiallyPartitioned<Actor> → the OPT-A8 slab form.
//  - Cache<Player,...> 惰性缓存(FrozenActor 域)本批不落;addOrUpdate/remove
//    单遍合批(L54-58 的注释语义)对 Actor/IEffect 面保留
//    The Cache<Player,...> lazy caches (the FrozenActor domain) are not
//    carried this batch; the one-pass addOrUpdate/remove batching note
//    (L54-58) stays for the Actor/IEffect faces.
//  - a.MouseBounds(worldRenderer)/a.ScreenBounds(worldRenderer) 的 Actor 渲染
//    缓存面随渲染 trait 批接入 —— 本批以注入面承载(缺省 = 空界,即不注册)
//    Actor.MouseBounds/ScreenBounds arrive with the render-trait batch —
//    carried by injection faces here (the defaults are empty bounds, i.e.
//    nothing registers).
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

/// ActorBoundsPair(ScreenMap.cs L21-29)
/// ActorBoundsPair (ScreenMap.cs L21-29).
struct ActorBoundsPair {
  Actor* ptr_actor = nullptr;
  Polygon bounds{Polygon::Empty()};

  std::string ToString() const;  // 定义于 screen_map.cpp(Actor 完整类型)
};

/// ScreenMap(ScreenMap.cs L40;Actor/IEffect 面)
/// ScreenMap (ScreenMap.cs L40; the Actor/IEffect faces).
class ScreenMap final : public TraitBase, public IWorldLoaded {
 public:
  static constexpr int kDefaultBinSize = 250;  // ScreenMapInfo.BinSize 默认

  ScreenMap(World& world, int bin_size);

  ORA_TRAIT_INTERFACES(ScreenMap, OpenRA_Traits_ScreenMap, IWorldLoaded)

  /// WorldLoaded(L82) | WorldLoaded (L82).
  void WorldLoaded(World& world, gfx::WorldRenderer& wr) override;

  // ———— Actor 面(L96-106)————
  void AddOrUpdate(Actor* a);
  void Remove(Actor* a);

  // ———— IEffect 面(L108-139)————
  void Add(IEffect* effect, const WPos& position, const Size& size);
  void Add(IEffect* effect, const WPos& position, const gfx::Sprite& sprite);
  void Update(IEffect* effect, const WPos& position, const Size& size);
  void Update(IEffect* effect, const WPos& position, const gfx::Sprite& sprite);
  void Remove(IEffect* effect);

  /// FrozenActor 面(L84-94/146-160/203-209)随 FrozenActorLayer 批
  /// The FrozenActor faces (L84-94/146-160/203-209) land with the
  /// FrozenActorLayer batch.

  /// ActorsAtMouse(L162-173;MouseInput 重载随 UI 输入装配批) | the
  /// ActorsAtMouse face (the MouseInput overloads land with the UI-input
  /// assembly batch).
  std::vector<ActorBoundsPair> ActorsAtMouse(int2 world_px);
  std::vector<ActorBoundsPair> ActorsInMouseBox(int2 a, int2 b);
  std::vector<ActorBoundsPair> ActorsInMouseBox(Rectangle r);

  std::vector<Actor*> RenderableActorsInBox(int2 a, int2 b);
  std::vector<IEffect*> RenderableEffectsInBox(int2 a, int2 b);

  /// TickRender(L211-271;Actor 面半) | TickRender (L211-271; the Actor
  /// half).
  void TickRender();

  /// RenderBounds/MouseBounds(L273-285;FrozenActor 项随该批) | the bounds
  /// enumerations (the FrozenActor items land with that batch).
  std::vector<Rectangle> RenderBounds() const;
  std::vector<Polygon> MouseBounds() const;

  /// Actor 界源注入(上游 a.MouseBounds(worldRenderer)/a.ScreenBounds(
  /// worldRenderer) 的渲染缓存面;空缺省 = 不注册 —— 见头注)
  /// The actor-bounds injection faces (upstream's
  /// a.MouseBounds(worldRenderer)/a.ScreenBounds(worldRenderer) render
  /// caches; the empty defaults register nothing — see the header).
  void SetActorBoundsSources(
      std::function<Polygon(Actor&)> fn_mouse_bounds,
      std::function<std::vector<Rectangle>(Actor&)> fn_screen_bounds) {
    fn_actor_mouse_bounds_ = std::move(fn_mouse_bounds);
    fn_actor_screen_bounds_ = std::move(fn_screen_bounds);
  }

 private:
  static bool ValidBounds(Rectangle bounds) {  // L141-144
    return bounds.Width > 0 && bounds.Height > 0;
  }
  static Rectangle RectWithCorners(int2 a, int2 b) {  // L175-178
    return Rectangle::FromLTRB(std::min(a.X, b.X), std::min(a.Y, b.Y),
                               std::max(a.X, b.X), std::max(a.Y, b.Y));
  }

  int bin_size_;
  SpatiallyPartitioned<Actor*> partitioned_mouse_actors_;
  SpatiallyPartitioned<Actor*> partitioned_renderable_actors_;
  SpatiallyPartitioned<IEffect*> partitioned_renderable_effects_;

  std::unordered_map<Actor*, ActorBoundsPair> map_partitioned_mouse_actor_bounds_;

  // Updates are done in one pass to ensure all bound changes have been
  // applied(上游注释;HashSet → 插入序 vector)
  std::vector<Actor*> vec_add_or_update_actors_;
  std::vector<Actor*> vec_remove_actors_;

  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;

  std::function<Polygon(Actor&)> fn_actor_mouse_bounds_;
  std::function<std::vector<Rectangle>(Actor&)> fn_actor_screen_bounds_;
};

}  // namespace ora::sim
