// UPSTREAM: OpenRA.Game/Map/CellLayerBase.cs @b6fc03f L19-78(逐语义重写)+
//          OpenRA.Game/Map/CellLayer.cs L18-174 +
//          OpenRA.Game/Map/ProjectedCellLayer.cs L106-151
//          C# CellLayer<T> 事件(CellEntryChanged)→ 回调表(construct 序
//          通知;CopyValuesFrom/Clear 的监听者检查 = 逐事件判空)
//          The C# CellLayer<T> events (CellEntryChanged) become callback
//          lists (notification in registration order; the listener checks
//          of CopyValuesFrom/Clear test the list for emptiness).
//
// 机制对照 / Mechanism mapping:
//  - Index(CPos)(L56-69):方格直乘;等距 (u,v) 换算 + Bounds 含端点检查 →
//    IndexOutOfRangeException 等价抛(上游消息固定)
//    Index (CPos) (L56-69): rectangular multiplies directly; isometric
//    (u,v) conversion + inclusive Bounds check → the equivalent
//    IndexOutOfRangeException throw (fixed upstream message).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/rectangle.hpp"
#include "core/size.hpp"
#include "map/cell_region.hpp"

namespace ora::map {

/// CellLayerBase<T>(CellLayerBase.cs L19):存储 + 尺寸/网格形状
/// CellLayerBase<T> (CellLayerBase.cs L19): storage + size/grid shape.
template <class T>
class CellLayerBase {
 public:
  CellLayerBase(MapGridType grid_type, Size size)
      : size_{size},
        bounds_{Rectangle::FromLTRB(0, 0, size.Width, size.Height)},
        grid_type_{grid_type},
        vec_entries_(static_cast<std::size_t>(size.Width) *
                     static_cast<std::size_t>(size.Height)) {}
  // 上游 Bounds = new Rectangle(0, 0, Size.Width, Size.Height)(含端点域
  // 判定用 Contains —— Rectangle.Contains 为开区间右下;上游照用)
  // Upstream Bounds = new Rectangle(0, 0, Size.Width, Size.Height) (used
  // via Contains — exclusive right/bottom; upstream does the same).

  explicit CellLayerBase(const class Map& map_world);  // 定义于 map.hpp 之后
                                                       // defined after map.hpp

  Size SizeOf() const { return size_; }
  MapGridType GridType() const { return grid_type_; }
  const Rectangle& Bounds() const { return bounds_; }

  /// CopyValuesFrom(L38-44):尺寸/形状失配 ArgumentException 同文本
  /// CopyValuesFrom (L38-44): size/shape mismatch throws the
  /// ArgumentException text verbatim.
  virtual void CopyValuesFrom(const CellLayerBase& another_layer) {
    if (size_ != another_layer.size_ || grid_type_ != another_layer.grid_type_)
      throw std::invalid_argument(
          "Layers must have a matching size and shape (grid type).");
    std::ranges::copy(another_layer.vec_entries_, vec_entries_.begin());
  }

  /// Clear(L47-56) | Clear (L47-56).
  virtual void Clear() { std::fill(vec_entries_.begin(), vec_entries_.end(), T{}); }
  virtual void Clear(T clear_value) {
    std::fill(vec_entries_.begin(), vec_entries_.end(), clear_value);
  }

  /// 存储直取(Entries 数组;AsReadOnlyMemory/AsMemory 面)| the raw
  /// storage (the Entries array; the AsReadOnlyMemory/AsMemory faces).
  std::span<T> Entries() { return vec_entries_; }
  std::span<const T> Entries() const { return vec_entries_; }

  virtual ~CellLayerBase() = default;

 protected:
  Size size_;
  Rectangle bounds_;
  MapGridType grid_type_;
  std::vector<T> vec_entries_;
};

/// CellLayer<T>(CellLayer.cs L18):格子/地图双坐标索引 + 变更事件
/// CellLayer<T> (CellLayer.cs L18): dual cell/map-coordinate indexing plus
/// the change event.
template <class T>
class CellLayer final : public CellLayerBase<T> {
 public:
  using Base = CellLayerBase<T>;

  CellLayer(MapGridType grid_type, Size size) : Base{grid_type, size} {}
  explicit CellLayer(const class Map& map_world) : Base{map_world} {}

  /// CellEntryChanged 事件(C# event Action<CPos>):construct 序回调表
  /// The CellEntryChanged event (C# event Action<CPos>): callbacks in
  /// registration order.
  void AddCellEntryChangedListener(std::function<void(CPos)> fn_listener) {
    vec_cell_entry_changed_.push_back(std::move(fn_listener));
  }
  bool HasCellEntryChangedListeners() const {
    return !vec_cell_entry_changed_.empty();
  }

  /// CopyValuesFrom(CellLayer.cs L28-35):有监听者时 InvalidOperationException
  /// 同文本 | CopyValuesFrom (CellLayer.cs L28-35): the listener guard.
  void CopyValuesFrom(const CellLayerBase<T>& another_layer) override {
    if (HasCellEntryChangedListeners())
      throw std::runtime_error(std::format(
          "Cannot copy values when there are listeners attached to the {} event.",
          "CellEntryChanged"));
    Base::CopyValuesFrom(another_layer);
  }

  void Clear() override {
    if (HasCellEntryChangedListeners())
      throw std::runtime_error(std::format(
          "Cannot clear values when there are listeners attached to the {} event.",
          "CellEntryChanged"));
    Base::Clear();
  }

  void Clear(T clear_value) override {
    if (HasCellEntryChangedListeners())
      throw std::runtime_error(std::format(
          "Cannot clear values when there are listeners attached to the {} event.",
          "CellEntryChanged"));
    Base::Clear(std::move(clear_value));
  }

  /// Index(CPos)(L56-69) | Index (CPos) (L56-69).
  int Index(CPos cell) const {
    // PERF: Inline CPos.ToMPos to avoid MPos allocation(上游注释)
    const int x = cell.X();
    const int y = cell.Y();
    if (Base::grid_type_ == MapGridType::Rectangular)
      return y * Base::size_.Width + x;

    const int u = (x - y) / 2;
    const int v = x + y;
    if (!Base::bounds_.Contains(u, v))
      throw std::out_of_range("Index was outside the bounds of the array.");
    return v * Base::size_.Width + u;
  }

  /// Index(MPos)(L72-77) | Index (MPos) (L72-77).
  int Index(MPos uv) const {
    if (!Base::bounds_.Contains(uv.U, uv.V))
      throw std::out_of_range("Index was outside the bounds of the array.");
    return uv.V * Base::size_.Width + uv.U;
  }

  /// this[CPos](L80-90):写路径触发事件 | this[CPos] (L80-90): writes fire
  /// the event.
  T Get(CPos cell) const { return Base::vec_entries_[Index(cell)]; }
  T& GetRef(CPos cell) { return Base::vec_entries_[Index(cell)]; }
  void Set(CPos cell, T value) {
    Base::vec_entries_[Index(cell)] = std::move(value);
    for (const auto& fn : vec_cell_entry_changed_)
      fn(cell);
  }

  /// this[MPos](L93-103) | this[MPos] (L93-103).
  T Get(MPos uv) const { return Base::vec_entries_[Index(uv)]; }
  T& GetRef(MPos uv) { return Base::vec_entries_[Index(uv)]; }
  void Set(MPos uv, T value) {
    Base::vec_entries_[Index(uv)] = std::move(value);
    for (const auto& fn : vec_cell_entry_changed_)
      fn(uv.ToCPos(Base::grid_type_));
  }

  /// TryGetValue(L105-125):等距 X<Y 预滤怪癖照抄 | TryGetValue (L105-125):
  /// the isometric X<Y pre-filter quirk kept.
  bool TryGetValue(CPos cell, T& value_out) const {
    // .ToMPos() returns the same result if the X and Y coordinates
    // are switched. X < Y is invalid in the RectangularIsometric coordinate
    // system, so we pre-filter these to avoid returning the wrong result
    // (上游注释)
    if (Base::grid_type_ == MapGridType::RectangularIsometric &&
        cell.X() < cell.Y()) {
      value_out = T{};
      return false;
    }

    const MPos uv = cell.ToMPos(Base::grid_type_);
    if (Base::bounds_.Contains(uv.U, uv.V)) {
      value_out = Base::vec_entries_[Index(uv)];
      return true;
    }

    value_out = T{};
    return false;
  }

  /// Contains(CPos)(L127-136) | Contains (CPos) (L127-136).
  bool Contains(CPos cell) const {
    // .ToMPos() returns the same result if the X and Y coordinates
    // are switched. X < Y is invalid in the RectangularIsometric coordinate
    // system, so we pre-filter these to avoid returning the wrong result
    // (上游注释)
    if (Base::grid_type_ == MapGridType::RectangularIsometric &&
        cell.X() < cell.Y())
      return false;

    return Contains(cell.ToMPos(Base::grid_type_));
  }

  /// Contains(MPos)(L138-141) | Contains (MPos) (L138-141).
  bool Contains(MPos uv) const { return Base::bounds_.Contains(uv.U, uv.V); }

  /// Clamp(L143-151) | Clamp (L143-151).
  CPos Clamp(CPos uv) const {
    return Clamp(uv.ToMPos(Base::grid_type_)).ToCPos(Base::grid_type_);
  }
  MPos Clamp(MPos uv) const {
    return uv.Clamp(
        Rectangle::FromLTRB(0, 0, Base::size_.Width - 1, Base::size_.Height - 1));
  }

  /// CellRegion(L153-154) | CellRegion (L153-154).
  CellRegion Cells() const {
    return CellRegion{Base::grid_type_, MPos{0, 0},
                      MPos{Base::size_.Width - 1, Base::size_.Height - 1}};
  }

 private:
  std::vector<std::function<void(CPos)>> vec_cell_entry_changed_;
};

/// ProjectedCellLayer<T>(ProjectedCellLayer.cs L106):PPos 直引索
/// ProjectedCellLayer<T> (ProjectedCellLayer.cs L106): PPos-direct indexing.
template <class T>
class ProjectedCellLayer final : public CellLayerBase<T> {
 public:
  using Base = CellLayerBase<T>;

  ProjectedCellLayer(MapGridType grid_type, Size size)
      : Base{grid_type, size} {}
  explicit ProjectedCellLayer(const class Map& map_world) : Base{map_world} {}

  int MaxIndex() const {
    return Base::size_.Width * Base::size_.Height;
  }  // L108

  /// Index(PPos)(L117-120) | Index (PPos) (L117-120).
  int Index(PPos uv) const { return uv.V * Base::size_.Width + uv.U; }

  /// PPosFromIndex(L122-125) | PPosFromIndex (L122-125).
  PPos PPosFromIndex(int index) const {
    return PPos{index % Base::size_.Width, index / Base::size_.Width};
  }

  /// this[int](L127-132) | this[int] (L127-132).
  T Get(int index) const { return Base::vec_entries_[index]; }
  void Set(int index, T value) { Base::vec_entries_[index] = std::move(value); }

  /// this[PPos](L135-140) | this[PPos] (L135-140).
  T Get(PPos uv) const { return Base::vec_entries_[Index(uv)]; }
  void Set(PPos uv, T value) {
    Base::vec_entries_[Index(uv)] = std::move(value);
  }

  /// Contains(L142-145) | Contains (L142-145).
  bool Contains(PPos uv) const { return Base::bounds_.Contains(uv.U, uv.V); }

  /// SetAll(L147-150) | SetAll (L147-150).
  void SetAll(T value) { Base::Clear(std::move(value)); }
};

/// CellLayer.Resize(CellLayer.cs L161-173):新建层按 defaultValue 填充后
/// 逐格拷贝交集(非缩层直拷 —— 上游就是逐 MPos)
/// CellLayer.Resize (CellLayer.cs L161-173): builds a new layer filled
/// with defaultValue then copies the intersection cell by cell (upstream
/// iterates MPos verbatim).
template <class T>
CellLayer<T> Resize(const CellLayer<T>& layer, Size new_size, T default_value) {
  CellLayer<T> result{layer.GridType(), new_size};
  const int width = std::min(layer.SizeOf().Width, new_size.Width);
  const int height = std::min(layer.SizeOf().Height, new_size.Height);

  result.Clear(default_value);
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++)
      result.Set(MPos{i, j}, layer.Get(MPos{i, j}));

  return result;
}

}  // namespace ora::map
