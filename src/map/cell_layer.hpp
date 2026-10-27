#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/rectangle.hpp"
#include "core/size.hpp"
#include "map/cell_region.hpp"

namespace ora::map {

template <class T>
class CellLayerBase {
 public:
  CellLayerBase(MapGridType grid_type, Size size)
      : size_{size},
        bounds_{Rectangle::FromLTRB(0, 0, size.Width, size.Height)},
        grid_type_{grid_type},
        vec_entries_(static_cast<std::size_t>(size.Width) *
                     static_cast<std::size_t>(size.Height)) {}

  explicit CellLayerBase(const class Map& map_world);

  Size SizeOf() const { return size_; }
  MapGridType GridType() const { return grid_type_; }
  const Rectangle& Bounds() const { return bounds_; }

  virtual void CopyValuesFrom(const CellLayerBase& another_layer) {
    if (size_ != another_layer.size_ || grid_type_ != another_layer.grid_type_)
      throw std::invalid_argument(
          "Layers must have a matching size and shape (grid type).");
    std::ranges::copy(another_layer.vec_entries_, vec_entries_.begin());
  }

  virtual void Clear() { std::fill(vec_entries_.begin(), vec_entries_.end(), T{}); }
  virtual void Clear(T clear_value) {
    std::fill(vec_entries_.begin(), vec_entries_.end(), clear_value);
  }

  std::span<T> Entries() { return vec_entries_; }
  std::span<const T> Entries() const { return vec_entries_; }

  virtual ~CellLayerBase() = default;

 protected:
  Size size_;
  Rectangle bounds_;
  MapGridType grid_type_;
  std::vector<T> vec_entries_;
};

template <class T>
class CellLayer final : public CellLayerBase<T> {
 public:
  using Base = CellLayerBase<T>;

  CellLayer(MapGridType grid_type, Size size) : Base{grid_type, size} {}
  explicit CellLayer(const class Map& map_world) : Base{map_world} {}

  void AddCellEntryChangedListener(std::function<void(CPos)> fn_listener) {
    vec_cell_entry_changed_.push_back(std::move(fn_listener));
  }

  std::size_t SubscribeCellEntryChanged(std::function<void(CPos)> fn_listener) {
    vec_cell_entry_changed_.push_back(std::move(fn_listener));
    return vec_cell_entry_changed_.size() - 1;
  }
  void RemoveCellEntryChangedListener(std::size_t n_token) {
    if (n_token < vec_cell_entry_changed_.size())
      vec_cell_entry_changed_[n_token] = nullptr;
  }
  bool HasCellEntryChangedListeners() const {
    for (const auto& fn : vec_cell_entry_changed_)
      if (fn)
        return true;
    return false;
  }

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

  int Index(CPos cell) const {

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

  int Index(MPos uv) const {
    if (!Base::bounds_.Contains(uv.U, uv.V))
      throw std::out_of_range("Index was outside the bounds of the array.");
    return uv.V * Base::size_.Width + uv.U;
  }

  T Get(CPos cell) const { return Base::vec_entries_[Index(cell)]; }
  T& GetRef(CPos cell) { return Base::vec_entries_[Index(cell)]; }
  void Set(CPos cell, T value) {
    Base::vec_entries_[Index(cell)] = std::move(value);
    for (const auto& fn : vec_cell_entry_changed_)
      if (fn)
        fn(cell);
  }

  T Get(MPos uv) const { return Base::vec_entries_[Index(uv)]; }
  T& GetRef(MPos uv) { return Base::vec_entries_[Index(uv)]; }
  void Set(MPos uv, T value) {
    Base::vec_entries_[Index(uv)] = std::move(value);
    for (const auto& fn : vec_cell_entry_changed_)
      if (fn)
        fn(uv.ToCPos(Base::grid_type_));
  }

  bool TryGetValue(CPos cell, T& value_out) const {

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

  bool Contains(CPos cell) const {

    if (Base::grid_type_ == MapGridType::RectangularIsometric &&
        cell.X() < cell.Y())
      return false;

    return Contains(cell.ToMPos(Base::grid_type_));
  }

  bool Contains(MPos uv) const { return Base::bounds_.Contains(uv.U, uv.V); }

  CPos Clamp(CPos uv) const {
    return Clamp(uv.ToMPos(Base::grid_type_)).ToCPos(Base::grid_type_);
  }
  MPos Clamp(MPos uv) const {
    return uv.Clamp(
        Rectangle::FromLTRB(0, 0, Base::size_.Width - 1, Base::size_.Height - 1));
  }

  CellRegion Cells() const {
    return CellRegion{Base::grid_type_, MPos{0, 0},
                      MPos{Base::size_.Width - 1, Base::size_.Height - 1}};
  }

 private:
  std::vector<std::function<void(CPos)>> vec_cell_entry_changed_;
};

template <class T>
class ProjectedCellLayer final : public CellLayerBase<T> {
 public:
  using Base = CellLayerBase<T>;

  ProjectedCellLayer(MapGridType grid_type, Size size)
      : Base{grid_type, size} {}
  explicit ProjectedCellLayer(const class Map& map_world) : Base{map_world} {}

  int MaxIndex() const {
    return Base::size_.Width * Base::size_.Height;
  }

  int Index(PPos uv) const { return uv.V * Base::size_.Width + uv.U; }

  PPos PPosFromIndex(int index) const {
    return PPos{index % Base::size_.Width, index / Base::size_.Width};
  }

  T Get(int index) const { return Base::vec_entries_[index]; }
  void Set(int index, T value) { Base::vec_entries_[index] = std::move(value); }

  T Get(PPos uv) const { return Base::vec_entries_[Index(uv)]; }
  void Set(PPos uv, T value) {
    Base::vec_entries_[Index(uv)] = std::move(value);
  }

  bool Contains(PPos uv) const { return Base::bounds_.Contains(uv.U, uv.V); }

  void SetAll(T value) { Base::Clear(std::move(value)); }
};

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

}
