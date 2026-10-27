#pragma once
import std;

#include "core/exts_math.hpp"
#include "core/int2.hpp"
#include "core/polygon.hpp"

namespace ora::mods::pathfinding {

inline const std::vector<CPos>& NoPathVector() {
  static const std::vector<CPos> vec_no_path;
  return vec_no_path;
}

inline constexpr int kPathCostForInvalidPath = std::numeric_limits<int>::max();
inline constexpr short kMovementCostForUnreachableCell =
    std::numeric_limits<short>::max();

enum class CellStatus : std::uint8_t { Unvisited, Open, Closed };

struct CellInfo {
  CellStatus status = CellStatus::Unvisited;
  int cost_so_far = 0;
  int estimated_total_cost = 0;
  CPos previous_node{CPos::Zero()};

  CellInfo() = default;
  CellInfo(CellStatus status, int cost_so_far, int estimated_total_cost,
           CPos previous_node)
      : status{status},
        cost_so_far{cost_so_far},
        estimated_total_cost{estimated_total_cost},
        previous_node{previous_node} {

    if (status == CellStatus::Unvisited)
      throw std::invalid_argument(
          "The default CellInfo is the only such CellInfo allowed for "
          "representing an Unvisited location. (Parameter 'status')");
  }
};

struct GraphConnection {
  CPos destination{CPos::Zero()};
  int cost = 0;

  GraphConnection() = default;
  GraphConnection(CPos destination, int cost)
      : destination{destination}, cost{cost} {
    if (cost < 0)
      throw std::out_of_range("cost cannot be negative");
    if (cost == kPathCostForInvalidPath)
      throw std::out_of_range(
          "cost cannot be used for an unreachable path");
  }

  struct CostComparer {
    bool operator()(const GraphConnection& x, const GraphConnection& y) const {
      return x.cost < y.cost;
    }
    static int Compare(const GraphConnection& x, const GraphConnection& y) {
      if (x.cost < y.cost)
        return -1;
      return x.cost > y.cost ? 1 : 0;
    }
  };

  struct GraphEdge ToEdge(CPos source) const;
};

struct GraphEdge {
  CPos source{CPos::Zero()};
  CPos destination{CPos::Zero()};
  int cost = 0;

  GraphEdge() = default;
  GraphEdge(CPos source, CPos destination, int cost)
      : source{source}, destination{destination}, cost{cost} {
    if (source == destination)
      throw std::invalid_argument(
          "source and destination must refer to different cells");
    if (cost < 0)
      throw std::out_of_range("cost cannot be negative");
    if (cost == kPathCostForInvalidPath)
      throw std::out_of_range(
          "cost cannot be used for an unreachable path");
  }

  GraphConnection ToConnection() const { return GraphConnection{destination, cost}; }
};

inline struct GraphEdge GraphConnection::ToEdge(CPos source) const {
  return GraphEdge{source, destination, cost};
}

struct Grid {

  CPos top_left{CPos::Zero()};

  CPos bottom_right{CPos::Zero()};

  bool single_layer = false;

  Grid() = default;
  Grid(CPos top_left, CPos bottom_right, bool single_layer)
      : top_left{top_left},
        bottom_right{bottom_right},
        single_layer{single_layer} {
    if (top_left.Layer() != bottom_right.Layer())
      throw std::invalid_argument(
          "topLeft and bottomRight must have the same Layer");
  }

  int Width() const { return bottom_right.X() - top_left.X(); }
  int Height() const { return bottom_right.Y() - top_left.Y(); }

  bool Contains(CPos cell) const {
    return cell.X() >= top_left.X() && cell.X() < bottom_right.X() &&
           cell.Y() >= top_left.Y() && cell.Y() < bottom_right.Y() &&
           (!single_layer || cell.Layer() == top_left.Layer());
  }

  bool IntersectsLine(CPos start, CPos end) const {
    const int2 s{start.X(), start.Y()};
    const int2 e{end.X(), end.Y()};
    const int2 tl{top_left.X(), top_left.Y()};
    const int2 tr{bottom_right.X(), top_left.Y()};
    const int2 bl{top_left.X(), bottom_right.Y()};
    const int2 br{bottom_right.X(), bottom_right.Y()};
    return LinesIntersect(s, e, tl, tr) || LinesIntersect(s, e, tl, bl) ||
           LinesIntersect(s, e, bl, br) || LinesIntersect(s, e, tr, br);
  }
};

class IPathGraph {
 public:
  virtual ~IPathGraph() = default;
  virtual std::vector<GraphConnection>& GetConnections(
      CPos source, const std::function<bool(CPos)>& target_predicate) = 0;
  virtual CellInfo At(CPos node) const = 0;
  virtual void Set(CPos node, CellInfo info) = 0;

  virtual void Dispose() = 0;
};

}
