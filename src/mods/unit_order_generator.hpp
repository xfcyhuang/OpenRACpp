#pragma once
import std;

#include "game/input.hpp"
#include "sim/order_generator.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::IOrderGenerator;
using sim::IOrderTargeter;
using sim::IIssueOrder;
using ora::MersenneTwister;
using sim::TraitBase;
using sim::World;

struct UnitOrderResult {
  sim::Actor* actor = nullptr;
  sim::IOrderTargeter* order = nullptr;
  sim::IIssueOrder* trait = nullptr;
  std::string cursor;
  sim::Target target{};
};

class UnitOrderGenerator : public sim::IOrderGenerator {
 public:
  explicit UnitOrderGenerator(
      sim::World& world,
      const GameSettingsFace& game_settings = GameSettingsFace{});

  static sim::Target TargetForInput(sim::World& world, CPos cell,
                                    int2 world_pixel,
                                    const MouseInput& mi);

  std::vector<std::unique_ptr<net::Order>> Order(
      sim::World& world, CPos cell,
      int2 world_pixel, const MouseInput& mi) override;

  std::vector<std::unique_ptr<net::Order>> OrderInner(
      sim::World& world, CPos cell,
      int2 world_pixel,
      const MouseInput& mi);

  MouseButton ActionButton() const;
  MouseButton CancelButton() const;
  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer& wr,
                                        sim::World& world) override;
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;
  std::string GetCursor(sim::World& world, CPos cell,
                        int2 world_pixel, const MouseInput& mi) override;
  void Deactivate() override;
  bool HandleKeyPress(const KeyInput& e_input) override { return false; }
  void SelectionChanged(sim::World& world,
                        std::span<sim::Actor* const> selected) override;

  bool InputOverridesSelection(sim::World& world, int2 xy, const MouseInput& mi);

  std::optional<UnitOrderResult> OrderForUnit(sim::Actor* self,
                                              const sim::Target& target_in,
                                              CPos xy, const MouseInput& mi);

  bool ClearSelectionOnLeftClick() const { return b_clear_selection_on_left_click_; }
  bool HasIssuedQueuedCommand() const { return b_has_issued_queued_command_; }
  void SetHasIssuedQueuedCommand(bool b) { b_has_issued_queued_command_ = b; }

 protected:
  virtual MouseActionType ActionType() const {
    return MouseActionType::Contextual;
  }

 private:

  static std::unique_ptr<net::Order> CheckSameOrder(
      sim::IOrderTargeter* iot, std::unique_ptr<net::Order> order);

  std::string world_select_cursor_{"select"};

  std::string world_default_cursor_{"default"};
  GameSettingsFace game_settings_;
  bool b_clear_selection_on_left_click_ = true;
  bool b_has_issued_queued_command_ = false;
};

}
