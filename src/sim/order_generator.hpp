#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "gfx/renderable.hpp"
#include "platform/sdl2_input.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::net {
struct Order;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {

class World;

class IOrderGenerator {
 public:
  virtual ~IOrderGenerator() = default;

  virtual MouseButton ActionButton() const = 0;
  virtual std::vector<net::Order*> Order(World& world, CPos cell, int2 world_pixel,
                                         const MouseInput& mi) = 0;
  virtual void Tick(World& world) = 0;
  virtual std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer& wr,
                                                World& world) = 0;
  virtual std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer& wr,
                                                           World& world) = 0;
  virtual std::vector<gfx::IRenderable*> RenderAnnotations(
      gfx::WorldRenderer& wr, World& world) = 0;
  virtual std::string GetCursor(World& world, CPos cell, int2 world_pixel,
                                const MouseInput& mi) = 0;
  virtual void Deactivate() = 0;
  virtual bool HandleKeyPress(const KeyInput& e_input) { return false; }
  virtual void SelectionChanged(World& world, std::span<Actor* const> selected) = 0;
};

void RegisterOrderGenerator(
    std::string str_name,
    std::function<std::unique_ptr<IOrderGenerator>(World&)> fn_factory);
bool OrderGeneratorRegistered(const std::string& str_name);
std::unique_ptr<IOrderGenerator> CreateOrderGenerator(
    const std::string& str_name, World& world);

}  // namespace ora::sim
