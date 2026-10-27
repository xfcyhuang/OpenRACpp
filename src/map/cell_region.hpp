#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"

namespace ora::map {

class CellRegion {
 public:

  CellRegion(MapGridType grid_type, CPos top_left, CPos bottom_right)
      : grid_type_{grid_type},
        top_left_{top_left},
        bottom_right_{bottom_right} {
    map_top_left_ = top_left.ToMPos(grid_type);
    map_bottom_right_ = bottom_right.ToMPos(grid_type);
  }

  CellRegion(MapGridType grid_type, MPos top_left, MPos bottom_right)
      : grid_type_{grid_type},
        top_left_{top_left.ToCPos(grid_type)},
        bottom_right_{bottom_right.ToCPos(grid_type)},
        map_top_left_{top_left},
        map_bottom_right_{bottom_right} {}

  CPos TopLeft() const { return top_left_; }
  CPos BottomRight() const { return bottom_right_; }

  std::string ToString() const {
    return std::format("{},{}->{},{}", top_left_.X(), top_left_.Y(),
                       bottom_right_.X(), bottom_right_.Y());
  }

  static CellRegion Expand(CellRegion region, int cordon) {
    const CPos tl = MPos{region.map_top_left_.U - cordon,
                         region.map_top_left_.V - cordon}
                        .ToCPos(region.grid_type_);
    const CPos br = MPos{region.map_bottom_right_.U + cordon,
                         region.map_bottom_right_.V + cordon}
                        .ToCPos(region.grid_type_);
    return CellRegion{region.grid_type_, tl, br};
  }

  static CellRegion BoundingRegion(MapGridType shape,
                                   std::span<const CPos> cells) {
    if (cells.empty())
      throw std::invalid_argument("cells must not be null or empty.");

    int min_u = std::numeric_limits<int>::max();
    int min_v = std::numeric_limits<int>::max();
    int max_u = std::numeric_limits<int>::min();
    int max_v = std::numeric_limits<int>::min();
    for (const CPos cell : cells) {
      const MPos uv = cell.ToMPos(shape);
      if (min_u > uv.U)
        min_u = uv.U;
      if (max_u < uv.U)
        max_u = uv.U;
      if (min_v > uv.V)
        min_v = uv.V;
      if (max_v < uv.V)
        max_v = uv.V;
    }

    return CellRegion{shape, MPos{min_u, min_v}.ToCPos(shape),
                      MPos{max_u, max_v}.ToCPos(shape)};
  }

  bool Contains(const CellRegion& region) const {
    return top_left_.X() <= region.top_left_.X() &&
           top_left_.Y() <= region.top_left_.Y() &&
           bottom_right_.X() >= region.bottom_right_.X() &&
           bottom_right_.Y() >= region.bottom_right_.Y();
  }

  bool Contains(CPos cell) const {
    const MPos uv = cell.ToMPos(grid_type_);
    return uv.U >= map_top_left_.U && uv.U <= map_bottom_right_.U &&
           uv.V >= map_top_left_.V && uv.V <= map_bottom_right_.V;
  }

  class MapCoordsRegion MapCoords() const;
  class CellCoordsRegion CellCoords() const;

  struct CellRegionIterator {
    const CellRegion* ptr_region;
    int u, v;
    CPos cur;
    bool b_done = false;

    explicit CellRegionIterator(const CellRegion& region, bool b_begin)
        : ptr_region{&region} {

      u = ptr_region->map_top_left_.U - 1;
      v = ptr_region->map_top_left_.V;
      if (b_begin)
        b_done = !MoveNext();
    }

    bool MoveNext() {
      ++u;

      if (u > ptr_region->map_bottom_right_.U) {
        ++v;
        u = ptr_region->map_top_left_.U;

        if (v > ptr_region->map_bottom_right_.V)
          return false;
      }

      cur = MPos{u, v}.ToCPos(ptr_region->grid_type_);
      return true;
    }

    CPos operator*() const { return cur; }
    CellRegionIterator& operator++() {
      if (!b_done)
        b_done = !MoveNext();
      return *this;
    }
    bool operator!=(const CellRegionIterator&) const {
      return !b_done;
    }
  };

  CellRegionIterator begin() const { return CellRegionIterator{*this, true}; }
  CellRegionIterator end() const {
    CellRegionIterator it{*this, false};
    it.b_done = true;
    return it;
  }

 private:
  friend class MapCoordsRegion;
  friend class CellCoordsRegion;
  MapGridType grid_type_;
  CPos top_left_;
  CPos bottom_right_;
  MPos map_top_left_{};
  MPos map_bottom_right_{};
};

class CellCoordsRegion {
 public:
  CellCoordsRegion(CPos top_left, CPos bottom_right)
      : top_left_{top_left}, bottom_right_{bottom_right} {}

  bool Contains(CPos cell) const {
    return cell.X() >= top_left_.X() && cell.X() <= bottom_right_.X() &&
           cell.Y() >= top_left_.Y() && cell.Y() <= bottom_right_.Y();
  }

  std::string ToString() const {
    return std::format("{},{}->{},{}", top_left_.X(), top_left_.Y(),
                       bottom_right_.X(), bottom_right_.Y());
  }

  CPos TopLeft() const { return top_left_; }
  CPos BottomRight() const { return bottom_right_; }

  static CellCoordsRegion BoundingRegion(std::span<const CPos> cells) {
    if (cells.empty())
      throw std::invalid_argument("cells must not be null or empty.");

    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();
    for (const CPos cell : cells) {
      if (min_x > cell.X())
        min_x = cell.X();
      if (max_x < cell.X())
        max_x = cell.X();
      if (min_y > cell.Y())
        min_y = cell.Y();
      if (max_y < cell.Y())
        max_y = cell.Y();
    }

    return CellCoordsRegion{CPos{min_x, min_y}, CPos{max_x, max_y}};
  }

  struct CellCoordsIterator {
    const CellCoordsRegion* ptr_region;
    CPos cur;
    bool b_done = false;

    explicit CellCoordsIterator(const CellCoordsRegion& region, bool b_begin)
        : ptr_region{&region} {

      if (b_begin) {
        cur = CPos{ptr_region->top_left_.X() - 1, ptr_region->top_left_.Y()};
        MoveNext();
      }
    }

    bool MoveNext() {
      const int x = cur.X() + 1;
      const int y = cur.Y();

      if (x > ptr_region->bottom_right_.X()) {
        int y2 = y + 1;
        int x2 = ptr_region->top_left_.X();
        if (y2 > ptr_region->bottom_right_.Y()) {
          b_done = true;
          return false;
        }
        cur = CPos{x2, y2};
        return true;
      }

      cur = CPos{x, y};
      return true;
    }

    CPos operator*() const { return cur; }
    CellCoordsIterator& operator++() {
      if (!b_done)
        b_done = !MoveNext();
      return *this;
    }
    bool operator!=(const CellCoordsIterator&) const {
      return !b_done;
    }
  };

  CellCoordsIterator begin() const {
    return CellCoordsIterator{*this, true};
  }
  CellCoordsIterator end() const {
    CellCoordsIterator it{*this, false};
    it.b_done = true;
    return it;
  }

 private:
  CPos top_left_;
  CPos bottom_right_;
};

class MapCoordsRegion {
 public:
  MapCoordsRegion(MPos map_top_left, MPos map_bottom_right)
      : top_left_{map_top_left}, bottom_right_{map_bottom_right} {}

  MPos TopLeft() const { return top_left_; }
  MPos BottomRight() const { return bottom_right_; }

  std::string ToString() const {
    return std::format("{},{}->{},{}", top_left_.U, top_left_.V,
                       bottom_right_.U, bottom_right_.V);
  }

  struct MapCoordsIterator {
    const MapCoordsRegion* ptr_region;
    MPos cur;
    bool b_done = false;

    explicit MapCoordsIterator(const MapCoordsRegion& region, bool b_begin)
        : ptr_region{&region} {
      if (b_begin) {

        cur = MPos{ptr_region->top_left_.U - 1, ptr_region->top_left_.V};
        MoveNext();
      }
    }

    bool MoveNext() {
      const int u = cur.U + 1;
      const int v = cur.V;

      if (u > ptr_region->bottom_right_.U) {
        int v2 = v + 1;
        int u2 = ptr_region->top_left_.U;
        if (v2 > ptr_region->bottom_right_.V) {
          b_done = true;
          return false;
        }
        cur = MPos{u2, v2};
        return true;
      }

      cur = MPos{u, v};
      return true;
    }

    MPos operator*() const { return cur; }
    MapCoordsIterator& operator++() {
      if (!b_done)
        b_done = !MoveNext();
      return *this;
    }
    bool operator!=(const MapCoordsIterator&) const {
      return !b_done;
    }
  };

  MapCoordsIterator begin() const { return MapCoordsIterator{*this, true}; }
  MapCoordsIterator end() const {
    MapCoordsIterator it{*this, false};
    it.b_done = true;
    return it;
  }

 private:
  MPos top_left_;
  MPos bottom_right_;
};

inline MapCoordsRegion CellRegion::MapCoords() const {
  return MapCoordsRegion{map_top_left_, map_bottom_right_};
}

inline CellCoordsRegion CellRegion::CellCoords() const {
  return CellCoordsRegion{top_left_, bottom_right_};
}

}
