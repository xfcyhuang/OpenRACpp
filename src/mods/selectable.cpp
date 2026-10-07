// UPSTREAM: OpenRA.Mods.Common/Traits/Selectable.cs @b6fc03f L15-55 +
//          Interactable.cs L15-105(逐语句重写;机制对照见 selectable.hpp
//          头注)
//          Statement-by-statement; the mechanism mapping lives in
//          selectable.hpp's header note.
#include "game/actor_info.hpp"
#include "mods/selectable.hpp"
#include "sim/trait_registry.hpp"


namespace ora::mods {

namespace {

/// ImmutableArray<WDist>/ImmutableArray<int2> 的数组槽读取
/// The array-slot reads of ImmutableArray<WDist>/ImmutableArray<int2>.
std::vector<WDist> ParseWDistArray(const meta::RecordObject& rec,
                                   std::string_view str_name) {
  std::vector<WDist> out;
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(
            &gv->val))
      for (const meta::GenericValue& element : *arr)
        if (const auto* tuple =
                std::get_if<meta::GenericTuple>(&element.val))
          if (tuple->uint1_count >= 1)
            out.push_back(
                WDist{static_cast<int>(tuple->arr_ints[0])});
  return out;
}

std::vector<int2> ParseInt2Array(const meta::RecordObject& rec,
                                 std::string_view str_name) {
  std::vector<int2> out;
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(
            &gv->val))
      for (const meta::GenericValue& element : *arr)
        if (const auto* tuple =
                std::get_if<meta::GenericTuple>(&element.val))
          if (tuple->uint1_count >= 2)
            out.push_back(int2{
                static_cast<std::int32_t>(tuple->arr_ints[0]),
                static_cast<std::int32_t>(tuple->arr_ints[1])});
  return out;
}

}  // namespace

InteractableInfoData InteractableInfoData::Parse(
    const meta::RecordObject& rec) {
  InteractableInfoData data;
  data.vec_bounds = ParseWDistArray(rec, "Bounds");
  data.vec_decoration_bounds = ParseWDistArray(rec, "DecorationBounds");
  data.vec_polygon = ParseInt2Array(rec, "Polygon");
  return data;
}

SelectableInfoData SelectableInfoData::Parse(
    const meta::RecordObject& rec) {
  SelectableInfoData data;
  if (const auto v = sim::RecordFieldInt(rec, "Priority"))
    data.int4_priority = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "PriorityModifiers"))
    data.priority_modifiers =
        static_cast<sim::SelectionPriorityModifiers>(*v);
  if (const auto s = sim::RecordFieldString(rec, "Class"))
    data.str_class = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec, "Voice"))
    data.str_voice = std::string{*s};
  data.interactable = InteractableInfoData::Parse(rec);
  return data;
}

Selectable::Selectable(sim::Actor& self, SelectableInfoData info)
    : info_{std::move(info)} {
  // L43-51:selectionClass = 空 Class ? actor 名 : Class
  // L43-51: selectionClass = empty Class ? the actor name : Class.
  str_selection_class_ = info_.str_class.empty()
                             ? self.Info()->Name()
                             : info_.str_class;
}

Interactable::Interactable(InteractableInfoData info)
    : info_{std::move(info)} {
  // L46-52:polygon 包围盒居中偏移(-width/2, -height/2)
  // L46-52: the polygon bounding rect's centering offset (-width/2,
  // -height/2).
  if (!info_.vec_polygon.empty()) {
    int2 lo = info_.vec_polygon.front();
    int2 hi = info_.vec_polygon.front();
    for (const int2& vertex : info_.vec_polygon) {
      lo.X = std::min(lo.X, vertex.X);
      lo.Y = std::min(lo.Y, vertex.Y);
      hi.X = std::max(hi.X, vertex.X);
      hi.Y = std::max(hi.Y, vertex.Y);
    }
    int2_polygon_center_offset_ =
        int2{-(hi.X - lo.X) / 2, -(hi.Y - lo.Y) / 2};
  }
}

void Interactable::Created(sim::Actor&) {
  // L51-54:IAutoMouseBounds 缓存 = 渲染 trait 集(空 —— WithSpriteBody 批)
  // L51-54: the IAutoMouseBounds cache = the render-trait set (empty —
  // the WithSpriteBody batch).
}

}  // namespace ora::mods
