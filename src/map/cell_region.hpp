// UPSTREAM: OpenRA.Game/Map/CellRegion.cs @b6fc03f L18-247(逐语义重写)+
//          OpenRA.Game/Map/CellCoordsRegion.cs L15-368 +
//          OpenRA.Game/Map/MapCoordsRegion.cs L17-89
//          C# IEnumerable/yield → 零分配枚举器结构体(begin/end 迭代对;
//          CellRegion 的列主序枚举序 = 上游 CellRegionEnumerator 逐行推进)
//          The C# IEnumerable/yield surfaces become zero-allocation
//          enumerator structs (begin/end iteration pairs; CellRegion's
//          column-major enumeration order = the upstream
//          CellRegionEnumerator row-by-row advance).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"

namespace ora::map {

/// CellRegion(CellRegion.cs L98):左右下上皆含端点的格子矩形
/// CellRegion (CellRegion.cs L98): an inclusive-endpoints cell rectangle.
class CellRegion {
 public:
  /// 两构造器(L110-128):CPos 侧与 MPos 侧互为推导 | the two ctors
  /// (L110-128): the CPos and MPos forms derive each other.
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

  /// ToString(L130-133;CPos.ToString = "X,Y" 插值)| ToString (L130-133;
  /// CPos.ToString = "X,Y" interpolation).
  std::string ToString() const {
    return std::format("{},{}->{},{}", top_left_.X(), top_left_.Y(),
                       bottom_right_.X(), bottom_right_.Y());
  }

  /// Expand(L136-141):外扩 cordon(可越出地图)| Expand (L136-141):
  /// grows by cordon (may leave the map borders).
  static CellRegion Expand(CellRegion region, int cordon) {
    const CPos tl = MPos{region.map_top_left_.U - cordon,
                         region.map_top_left_.V - cordon}
                        .ToCPos(region.grid_type_);
    const CPos br = MPos{region.map_bottom_right_.U + cordon,
                         region.map_bottom_right_.V + cordon}
                        .ToCPos(region.grid_type_);
    return CellRegion{region.grid_type_, tl, br};
  }

  /// BoundingRegion(L144-167):至少覆盖 cells 的最小区域;空输入抛
  /// ArgumentException 同文本
  /// BoundingRegion (L144-167): the minimal region covering cells; an
  /// empty input throws the ArgumentException text verbatim.
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

  /// Contains(CellRegion)(L169-174):CPos 侧四端点包含 | Contains
  /// (CellRegion) (L169-174): the CPos-side four-endpoint containment.
  bool Contains(const CellRegion& region) const {
    return top_left_.X() <= region.top_left_.X() &&
           top_left_.Y() <= region.top_left_.Y() &&
           bottom_right_.X() >= region.bottom_right_.X() &&
           bottom_right_.Y() >= region.bottom_right_.Y();
  }

  /// Contains(CPos)(L176-180):MPos 域判定 | Contains (CPos) (L176-180):
  /// the MPos-domain check.
  bool Contains(CPos cell) const {
    const MPos uv = cell.ToMPos(grid_type_);
    return uv.U >= map_top_left_.U && uv.U <= map_bottom_right_.U &&
           uv.V >= map_top_left_.V && uv.V <= map_bottom_right_.V;
  }

  /// MapCoords/CellCoords 投影(L182-183) | the MapCoords/CellCoords
  /// projections (L182-183).
  class MapCoordsRegion MapCoords() const;
  class CellCoordsRegion CellCoords() const;

  /// ———— 枚举器(L200-245):先 u 后 v 推进;构造即 Reset + 预取 Current ——
  ///      C++ 侧以迭代器对的"解引用即 Current"语义承载;消费完毕以标志
  ///      判定(MoveNext 为假后进入终态)
  ///      ———— The enumerator (L200-245): advances u then v; construction
  ///      Reset+prefetches Current — the C++ iterator pair carries the
  ///      "dereference = Current" semantics; exhaustion is flagged
  ///      (b_done set once MoveNext returns false).
  struct CellRegionIterator {
    const CellRegion* ptr_region;
    int u, v;
    CPos cur;
    bool b_done = false;

    explicit CellRegionIterator(const CellRegion& region, bool b_begin)
        : ptr_region{&region} {
      // Reset(L235-240):起始位 = 首元素前 | Reset (L235-240): starts
      // *before* the first element.
      u = ptr_region->map_top_left_.U - 1;
      v = ptr_region->map_top_left_.V;
      if (b_begin)
        b_done = !MoveNext();
    }

    bool MoveNext() {
      ++u;

      // Check for column overflow | 列溢出检查(L219-222)
      if (u > ptr_region->map_bottom_right_.U) {
        ++v;
        u = ptr_region->map_top_left_.U;

        // Check for row overflow | 行溢出检查(L225-227)
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

/// CellCoordsRegion(CellCoordsRegion.cs L265):CPos 域裸矩形(无网格换算)
/// CellCoordsRegion (CellCoordsRegion.cs L265): the raw CPos-domain
/// rectangle (no grid conversion).
class CellCoordsRegion {
 public:
  CellCoordsRegion(CPos top_left, CPos bottom_right)
      : top_left_{top_left}, bottom_right_{bottom_right} {}

  /// Contains(L315-318) | Contains (L315-318).
  bool Contains(CPos cell) const {
    return cell.X() >= top_left_.X() && cell.X() <= bottom_right_.X() &&
           cell.Y() >= top_left_.Y() && cell.Y() <= bottom_right_.Y();
  }

  std::string ToString() const {  // L320-323("X,Y->X,Y")
    return std::format("{},{}->{},{}", top_left_.X(), top_left_.Y(),
                       bottom_right_.X(), bottom_right_.Y());
  }

  CPos TopLeft() const { return top_left_; }
  CPos BottomRight() const { return bottom_right_; }

  /// BoundingRegion(L341-363):空输入 ArgumentException 同文本
  /// BoundingRegion (L341-363): the empty input throws the
  /// ArgumentException text verbatim.
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

  /// 枚举器(L267-307):MoveNext 先自增 X(列),溢出换行 —— 复刻逐字
  /// The enumerator (L267-307): MoveNext increments X (column) first,
  /// wrapping on overflow — verbatim.
  struct CellCoordsIterator {
    const CellCoordsRegion* ptr_region;
    CPos cur;
    bool b_done = false;

    explicit CellCoordsIterator(const CellCoordsRegion& region, bool b_begin)
        : ptr_region{&region} {
      // Reset(L298-301):首元素前 | Reset (L298-301): before the first.
      if (b_begin) {
        cur = CPos{ptr_region->top_left_.X() - 1, ptr_region->top_left_.Y()};
        MoveNext();
      }
    }

    bool MoveNext() {
      const int x = cur.X() + 1;
      const int y = cur.Y();

      // Check for column overflow | 列溢出检查(L283-291)
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

/// MapCoordsRegion(MapCoordsRegion.cs L17):MPos 域裸矩形
/// MapCoordsRegion (MapCoordsRegion.cs L17): the raw MPos-domain rectangle.
class MapCoordsRegion {
 public:
  MapCoordsRegion(MPos map_top_left, MPos map_bottom_right)
      : top_left_{map_top_left}, bottom_right_{map_bottom_right} {}

  MPos TopLeft() const { return top_left_; }
  MPos BottomRight() const { return bottom_right_; }

  std::string ToString() const {  // L66-69("U,V->U,V")
    return std::format("{},{}->{},{}", top_left_.U, top_left_.V,
                       bottom_right_.U, bottom_right_.V);
  }

  /// 枚举器(L19-58):u 先行,溢出换行 | the enumerator (L19-58): u first,
  /// wrapping on overflow.
  struct MapCoordsIterator {
    const MapCoordsRegion* ptr_region;
    MPos cur;
    bool b_done = false;

    explicit MapCoordsIterator(const MapCoordsRegion& region, bool b_begin)
        : ptr_region{&region} {
      if (b_begin) {
        // Reset(L50-53) | Reset (L50-53).
        cur = MPos{ptr_region->top_left_.U - 1, ptr_region->top_left_.V};
        MoveNext();
      }
    }

    bool MoveNext() {
      const int u = cur.U + 1;
      const int v = cur.V;

      // Check for column overflow | 列溢出检查(L35-42)
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

// ———— CellRegion 的两个投影(依赖完整类型)————
// ———— CellRegion's two projections (need the complete types) ————

inline MapCoordsRegion CellRegion::MapCoords() const {
  return MapCoordsRegion{map_top_left_, map_bottom_right_};
}

inline CellCoordsRegion CellRegion::CellCoords() const {
  return CellCoordsRegion{top_left_, bottom_right_};
}

}  // namespace ora::map
