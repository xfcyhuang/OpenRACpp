// UPSTREAM: OpenRA.Mods.Common/Pathfinder/IPathGraph.cs @b6fc03f +
//          Pathfinder/CellInfo.cs + Pathfinder/Grid.cs(逐语义重写)
//          Verbatim-semantics rewrites.
//
// 机制对照 / Mechanism mapping:
//  - GraphEdge/GraphConnection 的三处构造校验文本逐字(负代价/不可达代价/
//    自环);CostComparer → 轻型比较结构(可内联,同零虚化)
//    The three construction-check texts of GraphEdge/GraphConnection kept
//    verbatim (negative cost / unreachable cost / self loop); CostComparer
//    → a light comparer struct (inlinable, equally devirtualized).
//  - CellInfo 的 Unvisited 单例校验文本逐字 | the CellInfo Unvisited-only
//    default check text verbatim.
//  - Grid 的 int2 线段相交走 core/polygon.hpp 的 Exts 复刻
//    Grid's int2 line intersection goes through the core Exts replica.
#pragma once
import std;

#include "core/exts_math.hpp"
#include "core/int2.hpp"
#include "core/polygon.hpp"

namespace ora::mods::pathfinding {

/// PathFinder.NoPath(PathFinder.cs L43:static readonly List<CPos>)的
/// 共享空载体 | the shared empty carrier of PathFinder.NoPath
/// (PathFinder.cs L43).
inline const std::vector<CPos>& NoPathVector() {
  static const std::vector<CPos> vec_no_path;
  return vec_no_path;
}

/// PathGraph 常量(IPathGraph.cs L38-42) | the PathGraph constants
/// (IPathGraph.cs L38-42).
inline constexpr int kPathCostForInvalidPath = std::numeric_limits<int>::max();
inline constexpr short kMovementCostForUnreachableCell =
    std::numeric_limits<short>::max();

/// CellStatus(CellInfo.cs L20-25) | CellStatus (CellInfo.cs L20-25).
enum class CellStatus : std::uint8_t { Unvisited, Open, Closed };

/// CellInfo(CellInfo.cs L31-75;readonly struct → 值类型)
/// CellInfo (CellInfo.cs L31-75; the readonly struct → a value type).
struct CellInfo {
  CellStatus status = CellStatus::Unvisited;  // L36
  int cost_so_far = 0;                        // L41
  int estimated_total_cost = 0;               // L46
  CPos previous_node{CPos::Zero()};           // L51

  CellInfo() = default;
  CellInfo(CellStatus status, int cost_so_far, int estimated_total_cost,
           CPos previous_node)
      : status{status},
        cost_so_far{cost_so_far},
        estimated_total_cost{estimated_total_cost},
        previous_node{previous_node} {
    // The default CellInfo is the only such CellInfo allowed for
    // representing an Unvisited location.(上游校验文本逐字)
    if (status == CellStatus::Unvisited)
      throw std::invalid_argument(
          "The default CellInfo is the only such CellInfo allowed for "
          "representing an Unvisited location. (Parameter 'status')");
  }
};

/// GraphConnection(IPathGraph.cs L78-108) | GraphConnection (IPathGraph.cs
/// L78-108).
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

  /// GraphConnection.CostComparer(L80-86) | the CostComparer (L80-86).
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

  /// ToEdge(L102-105) | ToEdge (L102-105).
  struct GraphEdge ToEdge(CPos source) const;
};

/// GraphEdge(IPathGraph.cs L47-73) | GraphEdge (IPathGraph.cs L47-73).
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

  /// ToConnection(L67-70) | ToConnection (L67-70).
  GraphConnection ToConnection() const { return GraphConnection{destination, cost}; }
};

inline struct GraphEdge GraphConnection::ToEdge(CPos source) const {
  return GraphEdge{source, destination, cost};
}

/// Grid(Grid.cs L28-93;readonly struct → 值类型)
/// Grid (Grid.cs L28-93; the readonly struct → a value type).
struct Grid {
  /// Inclusive.(上游注释)| Inclusive. (upstream comment)
  CPos top_left{CPos::Zero()};
  /// Exclusive.(上游注释)| Exclusive. (upstream comment)
  CPos bottom_right{CPos::Zero()};
  /// When true, the grid spans only the single layer given by the cells.
  /// When false, it spans all layers.(上游注释)
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

  int Width() const { return bottom_right.X() - top_left.X(); }   // L55
  int Height() const { return bottom_right.Y() - top_left.Y(); }  // L56

  /// Contains(L61-67) | Contains (L61-67).
  bool Contains(CPos cell) const {
    return cell.X() >= top_left.X() && cell.X() < bottom_right.X() &&
           cell.Y() >= top_left.Y() && cell.Y() < bottom_right.Y() &&
           (!single_layer || cell.Layer() == top_left.Layer());
  }

  /// IntersectsLine(L74-87):层次忽略 | IntersectsLine (L74-87): layers
  /// are ignored.
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

/// IPathGraph(IPathGraph.cs L22-36;GetConnections 的 List 契约 → vector;
/// OPT-A2:稠密图的实现以成员暂存缓冲承载,见 dense_path_graph.hpp)
/// IPathGraph (IPathGraph.cs L22-36; the List contract → vector; OPT-A2:
/// the dense implementations carry a member scratch buffer — see
/// dense_path_graph.hpp).
class IPathGraph {
 public:
  virtual ~IPathGraph() = default;
  virtual std::vector<GraphConnection>& GetConnections(
      CPos source, const std::function<bool(CPos)>& target_predicate) = 0;
  virtual CellInfo At(CPos node) const = 0;
  virtual void Set(CPos node, CellInfo info) = 0;
  /// IDisposable.Dispose 面(池归还;C++ 侧由层池句柄 RAII 承载 —— 保留
  /// 虚点位以便 SparsePathGraph 的无操作形态)
  /// The IDisposable.Dispose face (the pool return; carried by the layer
  /// pool handle's RAII on the C++ side — the virtual slot stays for
  /// SparsePathGraph's no-op form).
  virtual void Dispose() = 0;
};

}  // namespace ora::mods::pathfinding
