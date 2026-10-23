// UPSTREAM: OpenRA.Mods.Common/Orders/PlaceBuildingOrderGenerator.cs @e180e3225
//          L24-337 全文
//          The whole of PlaceBuildingOrderGenerator.cs L24-337.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "game/input.hpp"
#include "mods/building.hpp"
#include "mods/footprint_place_building_preview.hpp"
#include "mods/place_building.hpp"
#include "mods/line_build.hpp"
#include "mods/plug.hpp"
#include "sim/order_generator.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
struct IViewportSurface;
class WorldRenderer;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::mods {

class ProductionQueue;

/// PlaceBuildingOrderGenerator(L41-336)
class PlaceBuildingOrderGenerator final : public sim::IOrderGenerator {
 public:
  /// VariantWrapper(L45-86;上游 LineBuild/Plug 的 TraitInfo 查询 → 构造期
  /// 解析值)
  /// VariantWrapper (L45-86; upstream's LineBuild/Plug TraitInfo lookups →
  /// construction-time parsed values).
  struct VariantWrapper {
    const game::ActorInfo* actor_info = nullptr;
    BuildingInfoData building_info;
    std::optional<PlugInfoData> opt_plug_info;
    std::optional<LineBuildInfoData> opt_line_build_info;
    std::unique_ptr<IPlaceBuildingPreview> ptr_preview;

    VariantWrapper(gfx::WorldRenderer& wr, ProductionQueue& queue,
                   const game::ActorInfo& ai);
  };

  PlaceBuildingOrderGenerator(ProductionQueue& queue, std::string str_name,
                              gfx::WorldRenderer& world_renderer);

  MouseButton ActionButton() const override {
    return game_settings_.ResolveActionButton(MouseActionType::PlaceBuilding);
  }

  std::vector<std::unique_ptr<net::Order>> Order(
      sim::World& world, CPos cell,
      int2 world_pixel,
      const MouseInput& mi) override;
  void Tick(sim::World& world) override;
  std::vector<gfx::IRenderable*> Render(gfx::WorldRenderer& wr,
                                        sim::World& world) override;
  std::vector<gfx::IRenderable*> RenderAboveShroud(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;
  std::vector<gfx::IRenderable*> RenderAnnotations(gfx::WorldRenderer& wr,
                                                   sim::World& world) override;
  std::string GetCursor(sim::World& world, CPos cell, int2 world_pixel,
                        const MouseInput& mi) override;
  void Deactivate() override {}
  bool HandleKeyPress(const KeyInput& e_input) override;
  void SelectionChanged(sim::World&,
                        std::span<sim::Actor* const>) override {}

  /// L154-164 | L154-164.
  CPos TopLeft() const;

  /// L312-315(GetCursor 覆写点;子类基)
  /// L312-315 (the GetCursor override point; the subclass base).
  virtual std::string GetCursorOverride(sim::World& world, CPos cell,
                                        int2 world_pixel,
                                        const MouseInput& mi);

 protected:
  /// L166-225 | L166-225.
  virtual std::vector<std::unique_ptr<net::Order>> InnerOrder(
      sim::World& world, CPos cell,
      const MouseInput& mi);

  /// L332-335 | L332-335.
  std::vector<std::unique_ptr<net::Order>> ClearBlockersOrders(CPos top_left);

  ProductionQueue* ptr_queue_ = nullptr;
  sim::World* ptr_world_ = nullptr;
  PlaceBuildingInfoData place_building_info_;
  gfx::IViewportSurface* ptr_viewport_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  std::vector<std::unique_ptr<VariantWrapper>> vec_variants_;
  GameSettingsFace game_settings_;
  std::size_t n_variant_ = 0;
  std::string str_world_default_cursor{"default"};

 private:
  /// L238-246 | L238-246.
  bool AcceptsPlug(CPos cell, const PlugInfoData& plug) const;
};

}  // namespace ora::mods
