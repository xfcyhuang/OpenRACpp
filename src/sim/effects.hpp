// UPSTREAM: OpenRA.Game/Effects/IEffect.cs @b6fc03f L16-28
//          + OpenRA.Game/Effects/DelayedAction.cs L18-37
//          + OpenRA.Game/Effects/DelayedImpact.cs(结构对照;渲染面 Phase 4)
//          IEffect.cs + DelayedAction.cs + DelayedImpact.cs (structure
//          carried; render surface Phase 4).
//
// 机制对照 / Mechanism mapping:
//  - IEffect.Render(WorldRenderer) → 本期空实现占位(WorldRenderer 是 Phase 4
//    类型;World.Remove 的 effect 存储与 ISync 收集不依赖渲染)
//    IEffect.Render(WorldRenderer) → an empty placeholder this phase
//    (WorldRenderer is a Phase 4 type; World's effect storage and ISync
//    collection do not depend on rendering).
//  - DelayedAction 的 world.AddFrameEndTask(w => { w.Remove(this); a(); }):
//    捕获 this 的裸引用 —— effect 生命周期由 World 的 effects 容器托管,
//    Remove 后由 World 统一析构(见 world.hpp 所有权注释)
//    DelayedAction's world.AddFrameEndTask(w => { w.Remove(this); a(); }):
//    a bare self-reference — effect lifetimes are owned by the World's
//    effects containers and destroyed by the World after Remove (see the
//    ownership notes in world.hpp).
#pragma once
import std;

#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

/// IEffect(IEffect.cs L17-21)
class IEffect {
 public:
  virtual ~IEffect() = default;
  virtual void Tick(World& world) = 0;
};

/// ISpatiallyPartitionable(IEffect.cs L24;ScreenMap 分区标记,Phase 4/5)
/// ISpatiallyPartitionable (IEffect.cs L24; the ScreenMap partition marker,
/// Phase 4/5).
class ISpatiallyPartitionable {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Effects_ISpatiallyPartitionable;
  virtual ~ISpatiallyPartitionable() = default;
};

/// DelayedAction(DelayedAction.cs L18-36):延迟帧末任务 effect
/// DelayedAction (DelayedAction.cs L18-36): the delayed frame-end task
/// effect.
class DelayedAction final : public IEffect {
 public:
  DelayedAction(int delay, std::function<void()> fn)
      : fn_(std::move(fn)), int4_delay_(delay) {}

  void Tick(World& world) override;

 private:
  std::function<void()> fn_;
  int int4_delay_;
};

}  // namespace ora::sim
