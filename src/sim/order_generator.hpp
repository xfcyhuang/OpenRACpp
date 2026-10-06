// UPSTREAM: OpenRA.Game/Orders/IOrderGenerator.cs @b6fc03f L17-34 +
//          OpenRA.Mods.Common/Orders/OrderGenerator.cs L18-60(逐语义重写;
//          GameSettings/MouseActionType 面随 Settings 批)
//          Verbatim-semantics rewrite; the GameSettings/MouseActionType faces
//          land with the Settings batch.
#pragma once
import std;

#include "gfx/renderable.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

/// IOrderGenerator(IOrderGenerator.cs L17;Mods.Common 侧 OrderGenerator 基类
/// 的 C++ 承载 = 本抽象基;Render 族返回值 = IEnumerable<IRenderable> 的
/// 物化快照面)
/// IOrderGenerator (IOrderGenerator.cs L17; the C++ carrier of the
/// Mods.Common OrderGenerator base = this abstract base; the Render-family
/// returns are the materialized-snapshot form of IEnumerable<IRenderable>).
class IOrderGenerator {
 public:
  virtual ~IOrderGenerator() = default;

  /// Tick(IOrderGenerator.cs L20) | Tick (L20).
  virtual void Tick(World& world) = 0;
  virtual std::vector<gfx::IRenderable*> Render(World& world) = 0;              // L21
  virtual std::vector<gfx::IRenderable*> RenderAboveShroud(World& world) = 0;   // L22
  virtual std::vector<gfx::IRenderable*> RenderAnnotations(World& world) = 0;   // L23
  virtual std::string GetCursor(World& world, CPos cell, int2 world_pixel) = 0; // L24
  virtual void Deactivate() = 0;                                                // L25
  virtual bool HandleKeyPress() { return false; }  // L26(KeyInput 随输入批)
  virtual void SelectionChanged(World& world,
                                std::span<Actor* const> selected) = 0;          // L27
};

// ———— DefaultOrderGenerator 名字分派注册表(World.cs L185-192 的
//      ObjectCreator.FindType + ctor(World) 反射构造 → 注册表面)————
// ———— The DefaultOrderGenerator name-dispatch registry (the registry face
//      of World.cs L185-192's ObjectCreator.FindType + ctor(World)
//      reflection construct) ————

class World;

/// 未知名异常文本("… is not a valid DefaultOrderGenerator"逐字)
/// The unknown-name exception text ("… is not a valid
/// DefaultOrderGenerator" verbatim).
void RegisterOrderGenerator(
    std::string str_name,
    std::function<std::unique_ptr<IOrderGenerator>(World&)> fn_factory);
bool OrderGeneratorRegistered(const std::string& str_name);
std::unique_ptr<IOrderGenerator> CreateOrderGenerator(
    const std::string& str_name, World& world);

}  // namespace ora::sim
