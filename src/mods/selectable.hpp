// UPSTREAM: OpenRA.Mods.Common/Traits/Selectable.cs @b6fc03f L15-55 全文 +
//          Interactable.cs L15-105 全文
//          The whole of Selectable.cs L15-55 + Interactable.cs L15-105.
//
// 机制对照 / Mechanism mapping:
//  - Interactable 的 MouseoverBounds/PolygonBounds/DecorationBounds 依赖
//    WorldRenderer/IAutoMouseBounds(渲染批):Bounds/DecorationBounds/
//    Polygon 的值面与 polygonCenterOffset 预计算全量,屏幕换算面随渲染批
//    Interactable's MouseoverBounds/PolygonBounds/DecorationBounds depend
//    on WorldRenderer/IAutoMouseBounds (the render batch): the
//    Bounds/DecorationBounds/Polygon value faces and the
//    polygonCenterOffset precalculation are kept in full; the screen
//    conversion rides the render batch.
//  - 上游 SelectableInfo : InteractableInfo(单表继承);C++ 侧两 Info
//    解析面分立 + Selectable 持 Interactable 基段(基类仅 mouse-bounds
//    渲染面,数据面无字段 —— 直挂 TraitBase + 行为位)
//    Upstream's SelectableInfo : InteractableInfo (single-table
//    inheritance); the C++ side splits the two Info parse faces and
//    Selectable carries no Interactable base segment (the base holds
//    only the mouse-bounds render face — no data fields — so TraitBase
//    + the behaviour bit mount directly).
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

/// InteractableInfo(L17-38)的解析面
/// The parsed face of InteractableInfo (L17-38).
struct InteractableInfoData {
  std::vector<WDist> vec_bounds;          // L23
  std::vector<WDist> vec_decoration_bounds;  // L27
  std::vector<int2> vec_polygon;          // L31

  static InteractableInfoData Parse(const meta::RecordObject& rec);
};

/// SelectableInfo(L17-39)的解析面
/// The parsed face of SelectableInfo (L17-39).
struct SelectableInfoData {
  int int4_priority = 10;  // L19
  sim::SelectionPriorityModifiers priority_modifiers =
      sim::SelectionPriorityModifiers::None;  // L25
  std::string str_class;                       // L29
  std::string str_voice = "Select";            // L33
  InteractableInfoData interactable;           // 基类段 | the base segment

  static SelectableInfoData Parse(const meta::RecordObject& rec);
};

/// Interactable(L41-105;数据面 + polygon 居中预计算)
/// Interactable (L41-105; the data face + the polygon centering
/// precalculation).
class Interactable : public sim::TraitBase,
                     public sim::INotifyCreated,
                     public sim::IMouseBounds {
 public:
  ORA_TRAIT_INTERFACES(Interactable, OpenRA_Mods_Common_Traits_Interactable,
                       sim::INotifyCreated, sim::IMouseBounds)

  explicit Interactable(InteractableInfoData info);

  void Created(sim::Actor& self) override;  // L51-54

  Polygon MouseoverBounds(sim::Actor& self,
                          gfx::WorldRenderer* wr) override;

  /// DecorationBounds(L109-112):装饰矩形(DecorationBounds 空则回落
  /// Bounds;再空则 AutoBounds 首非空)
  /// DecorationBounds (L109-112): the decoration rect (an empty
  /// DecorationBounds falls back to Bounds; then to AutoBounds's first
  /// non-empty).
  Rectangle DecorationBounds(sim::Actor& self, gfx::WorldRenderer& wr);

 private:
  /// AutoBounds(L56-60):IAutoMouseBounds trait 集的首非空矩形
  /// AutoBounds (L56-60): the first non-empty rect of the IAutoMouseBounds
  /// set.
  Rectangle AutoBounds(sim::Actor& self, gfx::WorldRenderer& wr);

  /// PolygonBounds(L62-76):世界偏移顶点换算屏幕像素
  /// PolygonBounds (L62-76): the world-offset vertices converted to screen
  /// pixels.
  std::vector<int2> PolygonBounds(sim::Actor& self, gfx::WorldRenderer& wr);

  /// Bounds(L78-96):WDist 矩形换算屏幕像素(空 bounds = AutoBounds)
  /// Bounds (L78-96): the WDist rect converted to screen pixels (empty
  /// bounds = AutoBounds).
  Polygon Bounds(sim::Actor& self, gfx::WorldRenderer& wr,
                 const std::vector<WDist>& vec_bounds);

  InteractableInfoData info_;
  /// L46-52:polygon 包围盒的居中偏移预计算
  /// L46-52: the precalculated centering offset of the polygon's bounding
  /// rect.
  int2 int2_polygon_center_offset_{};
  std::vector<sim::IAutoMouseBounds*> vec_auto_bounds_;
};

/// Selectable(L41-54):承 Interactable —— 上游单表继承(SelectionDecorations
/// 的 Trait<Interactable> 查询需命中同一对象)
/// Selectable (L41-54): inherits Interactable — upstream's single-table
/// inheritance (the Trait<Interactable> query must hit the same object).
class Selectable final : public Interactable,
                         public sim::ISelectable {
 public:
  ORA_TRAIT_INTERFACES(Selectable, OpenRA_Mods_Common_Traits_Selectable,
                       Interactable, sim::INotifyCreated, sim::IMouseBounds,
                       sim::ISelectable)

  Selectable(sim::Actor& self, SelectableInfoData info);

  /// L53:ISelectable.Class
  std::string_view Class() const override { return str_selection_class_; }

  const SelectableInfoData& InfoData() const { return info_; }

 private:
  std::string str_selection_class_;  // L43(空/缺省 = actor 名)
                                     // L43 (empty/default = the actor name).
  SelectableInfoData info_;
};

}  // namespace ora::mods
