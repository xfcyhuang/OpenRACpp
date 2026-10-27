#pragma once
import std;

#include "core/int2.hpp"
#include "core/polygon.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

struct InteractableInfoData {
  std::vector<WDist> vec_bounds;
  std::vector<WDist> vec_decoration_bounds;
  std::vector<int2> vec_polygon;

  static InteractableInfoData Parse(const meta::RecordObject& rec);
};

struct SelectableInfoData {
  int int4_priority = 10;
  sim::SelectionPriorityModifiers priority_modifiers =
      sim::SelectionPriorityModifiers::None;
  std::string str_class;
  std::string str_voice = "Select";
  InteractableInfoData interactable;

  static SelectableInfoData Parse(const meta::RecordObject& rec);
};

class Interactable : public sim::TraitBase,
                     public sim::INotifyCreated,
                     public sim::IMouseBounds {
 public:
  ORA_TRAIT_INTERFACES(Interactable, OpenRA_Mods_Common_Traits_Interactable,
                       sim::INotifyCreated, sim::IMouseBounds)

  explicit Interactable(InteractableInfoData info);

  void Created(sim::Actor& self) override;

  Polygon MouseoverBounds(sim::Actor& self,
                          gfx::WorldRenderer* wr) override;

  Rectangle DecorationBounds(sim::Actor& self, gfx::WorldRenderer& wr);

 private:

  Rectangle AutoBounds(sim::Actor& self, gfx::WorldRenderer& wr);

  std::vector<int2> PolygonBounds(sim::Actor& self, gfx::WorldRenderer& wr);

  Polygon Bounds(sim::Actor& self, gfx::WorldRenderer& wr,
                 const std::vector<WDist>& vec_bounds);

  InteractableInfoData info_;

  int2 int2_polygon_center_offset_{};
  std::vector<sim::IAutoMouseBounds*> vec_auto_bounds_;
};

class Selectable final : public Interactable,
                         public sim::ISelectable {
 public:
  ORA_TRAIT_INTERFACES(Selectable, OpenRA_Mods_Common_Traits_Selectable,
                       Interactable, sim::INotifyCreated, sim::IMouseBounds,
                       sim::ISelectable)

  Selectable(sim::Actor& self, SelectableInfoData info);

  std::string_view Class() const override { return str_selection_class_; }

  const SelectableInfoData& InfoData() const { return info_; }

 private:
  std::string str_selection_class_;

  SelectableInfoData info_;
};

}
