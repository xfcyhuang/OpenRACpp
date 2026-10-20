// UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @e180e3225
//          L18-61 全文 + GlobalButtonOrderGenerator.cs @e180e3225 L19-95
//          全文 + RepairOrderGenerator.cs @e180e3225 L20-87 全文 +
//          BeaconOrderGenerator.cs @e180e3225 L18-40 全文。
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "game/input.hpp"
#include "sim/order_generator.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

class ModsOrderGeneratorBase : public sim::IOrderGenerator {
 public:
  explicit ModsOrderGeneratorBase(sim::World& world);

  MouseButton ActionButton() const override;
  MouseButton CancelButton() const;

  std::vector<net::Order*> Order(sim::World& world, CPos cell,
                                 int2 world_pixel,
                                 const MouseInput& mi) override;
  void Deactivate() override {}
  bool HandleKeyPress(const KeyInput&) override { return false; }
  void SelectionChanged(sim::World&,
                        std::span<sim::Actor* const>) override {}

 protected:
  virtual MouseActionType ActionType() const = 0;
  virtual std::vector<net::Order*> OrderInner(sim::World& world, CPos cell,
                                              int2 world_pixel,
                                              const MouseInput& mi) = 0;

  GameSettingsFace game_settings_;
};

template <class TraitT>
class GlobalButtonOrderGenerator : public ModsOrderGeneratorBase {
 public:
  GlobalButtonOrderGenerator(sim::World& world, std::string str_order)
      : ModsOrderGeneratorBase{world},
        str_order_{std::move(str_order)} {}

  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer&,
                                        sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }

 protected:
  MouseActionType ActionType() const override {
    return MouseActionType::GlobalCommand;
  }

  virtual bool IsValidTrait(const TraitT& trait_t) const;

  std::vector<net::Order*> OrderInner(sim::World& world, CPos cell,
                                      int2 world_pixel,
                                      const MouseInput& mi) override;

  std::string str_order_;
};

class SellOrderGenerator final
    : public GlobalButtonOrderGenerator<class Sellable> {
 public:
  explicit SellOrderGenerator(sim::World& world);

  std::string GetCursor(sim::World& world, CPos cell, int2 world_pixel,
                        const MouseInput& mi) override;
};

class PowerDownOrderGenerator final
    : public GlobalButtonOrderGenerator<class ToggleConditionOnOrder> {
 public:
  explicit PowerDownOrderGenerator(sim::World& world);

 protected:
  bool IsValidTrait(const ToggleConditionOnOrder& trait_t) const override;

 public:
  std::string GetCursor(sim::World& world, CPos cell, int2 world_pixel,
                        const MouseInput& mi) override;
};

class RepairOrderGenerator final : public ModsOrderGeneratorBase {
 public:
  explicit RepairOrderGenerator(sim::World& world);

  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer&,
                                        sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::string GetCursor(sim::World& world, CPos cell, int2 world_pixel,
                        const MouseInput& mi) override;

 protected:
  MouseActionType ActionType() const override {
    return MouseActionType::GlobalCommand;
  }
  std::vector<net::Order*> OrderInner(sim::World& world, CPos cell,
                                      int2 world_pixel,
                                      const MouseInput& mi) override;
};

class BeaconOrderGenerator final : public ModsOrderGeneratorBase {
 public:
  explicit BeaconOrderGenerator(sim::World& world);

  void Tick(sim::World&) override {}
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer&,
                                        sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer&,
                                                   sim::World&) override {
    return {};
  }
  std::string GetCursor(sim::World&, CPos, int2,
                        const MouseInput&) override;

 protected:
  MouseActionType ActionType() const override {
    return MouseActionType::PlaceBuilding;
  }
  std::vector<net::Order*> OrderInner(sim::World& world, CPos cell,
                                      int2 world_pixel,
                                      const MouseInput& mi) override;
};

}
