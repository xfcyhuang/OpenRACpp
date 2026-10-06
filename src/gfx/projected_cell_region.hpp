// UPSTREAM: OpenRA.Game/Map/ProjectedCellRegion.cs @b6fc03f L18-125(全文逐语义)
// 投影单元格区域:屏幕上的 (U,V) 矩形集合(TopLeft/BottomRight 含端点)。
// 构造期由 Map 的 MaximumTerrainHeight 与 Height 层换算出"可能投影进本
// 区域"的地图坐标包围区(mapTopLeft/mapBottomRight);枚举器逐列推进
// (MoveNext 的溢列进行/溢行终止逐行)。
// 形态适配:
//   - Map 依赖(Height.Clamp / Grid.MaximumTerrainHeight / Grid.Type)
//     → 构造参数(heightOffset 与 heightClamp 由调用方给出 —— 完整 Map
//     随 Phase 5,WorldRenderer.TerrainMapSurface 同族注入);
//   - IEnumerable&lt;PPos&gt; → 前向迭代器对(begin/end),零分配枚举
//     (上游枚举器逐 MoveNext 分配 PPos 装箱的等价值语义)。
// The projected-cell region: a rectangular (U,V) collection on screen
// (TopLeft/BottomRight inclusive). Construction derives the bounding map
// region (mapTopLeft/mapBottomRight) that may project into this region from
// the Map's MaximumTerrainHeight and Height layer; the enumerator advances
// column by column (MoveNext's column-overflow row step and row-overflow
// stop kept line by line). Shape adaptations:
//   - the Map dependencies (Height.Clamp / Grid.MaximumTerrainHeight /
//     Grid.Type) arrive as constructor parameters (heightOffset and
//     heightClamp come from the caller — the full Map is Phase 5, the same
//     injection family as WorldRenderer.TerrainMapSurface);
//   - IEnumerable<PPos> → a forward iterator pair (begin/end), enumeration
//     with zero allocation (the value semantics of upstream's per-MoveNext
//     PPos production).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/rectangle.hpp"

namespace ora::gfx {

/// ProjectedCellRegion(ProjectedCellRegion.cs L26)。
class ProjectedCellRegion {
 public:
  /// 空区(0,0)-(0,0);上游无对应构造,C++ 侧 Viewport 成员初始化用
  /// (脏标记保证首查询即重算)。
  /// The empty (0,0)-(0,0) region; upstream has no counterpart — C++ uses it
  /// for Viewport member initialization (the dirty flags guarantee
  /// recalculation on the first query).
  ProjectedCellRegion() = default;

  /// 上游 ctor(ProjectedCellRegion(map, topLeft, bottomRight))的注入形态:
  /// heightOffset = 等距 ? maxHeight : maxHeight/2(L46-48);heightClamp =
  /// map.Height.Clamp(候选 bottomRight 的 V+heightOffset 行;上游 CellLayer
  /// 的界内夹取)。
  /// The injected shape of the upstream ctor (ProjectedCellRegion(map,
  /// topLeft, bottomRight)): heightOffset = isometric ? maxHeight :
  /// maxHeight/2 (L46-48); heightClamp = map.Height.Clamp (the in-bounds
  /// clamp of the candidate bottomRight's V+heightOffset row; upstream's
  /// CellLayer clamp).
  static ProjectedCellRegion Make(PPos puv_top_left, PPos puv_bottom_right,
                                  std::function<MPos(MPos)> fn_height_clamp, bool b_isometric,
                                  std::int32_t int4_max_height) {
    // MPos→PPos 投影不会产生更大的 V,故 MPos 区上沿 = PPos 区上沿(高度 0
    // 时两区相同)—— 上游注释逐句。
    // The MPos -> PPos projection cannot produce a larger V coordinate, so
    // the MPos region's top edge equals the PPos region's (in fact identical
    // when height == 0) — the upstream comment verbatim.
    const MPos uv_map_top_left = ToMPos(puv_top_left);

    // 下沿复杂些:高度 > 0 的 MPos.V > bottomRight.V 单元格也可能投影进本
    // 区域;每级高度 = 512 WDist,等距格 = 1 个 MPos 步,经典格仅半步。
    // The bottom edge is trickier: cells at MPos.V > bottomRight.V may
    // project into this region when their height > 0; each height step is
    // 512 WDist — one MPos step for isometric cells, half a step for classic.
    const std::int32_t int4_height_offset =
        b_isometric ? int4_max_height : int4_max_height / 2;

    // 以 Height 数据夹取下沿防溢出地图。
    // The Height data clamps the bottom coordinate against map overflow.
    const MPos uv_map_bottom_right = fn_height_clamp(MPos{puv_bottom_right.U,
                                                           puv_bottom_right.V + int4_height_offset});

    return ProjectedCellRegion{puv_top_left, puv_bottom_right, uv_map_top_left,
                               uv_map_bottom_right};
  }

  /// Contains(L65-68):闭矩形判定。
  /// Contains (L65-68): closed-rectangle test.
  bool Contains(PPos puv_p) const {
    return puv_p.U >= puv_top_left_.U && puv_p.U <= puv_bottom_right_.U &&
           puv_p.V >= puv_top_left_.V && puv_p.V <= puv_bottom_right_.V;
  }

  /// CandidateMapCoords(L73-77):可能投影进本区域的地图坐标区
  /// (性能取舍:不逐格验证是否真投影在内 —— 上游注释逐句)。
  /// CandidateMapCoords (L73-77): the map-coords region that may project
  /// into this region (a performance trade-off: individual cells are not
  /// validated — the upstream comment verbatim).
  std::pair<MPos, MPos> CandidateMapCoords() const { return {uv_map_top_left_, uv_map_bottom_right_}; }

  PPos TopLeft() const { return puv_top_left_; }
  PPos BottomRight() const { return puv_bottom_right_; }

  /// 枚举器(ProjectedCellRegionEnumerator,L84-124):u/v 从
  /// (TopLeft.U-1, TopLeft.V) 起步,MoveNext 先 ++u、溢列换行、溢行终止;
  /// Current 恒为最近一次 MoveNext 产物。
  /// The enumerator (ProjectedCellRegionEnumerator, L84-124): u/v start at
  /// (TopLeft.U-1, TopLeft.V); MoveNext increments u first, steps the row on
  /// column overflow, and stops on row overflow; Current is always the most
  /// recent MoveNext product.
  class Iterator {
   public:
    // 上游 MoveNext:溢列时 ++v、u 回 TopLeft.U;溢行返回 false。
    // Upstream MoveNext: on column overflow ++v and u resets to TopLeft.U;
    // on row overflow returns false.
    Iterator& operator++() {
      int4_u_++;

      // 溢列检查 / Check for column overflow.
      if (int4_u_ > region_->puv_bottom_right_.U) {
        int4_v_++;
        int4_u_ = region_->puv_top_left_.U;

        // 溢行检查 / Check for row overflow.
        if (int4_v_ > region_->puv_bottom_right_.V)
          b_done_ = true;
      }

      if (!b_done_)
        puv_current_ = PPos{int4_u_, int4_v_};
      return *this;
    }

    PPos operator*() const { return puv_current_; }
    PPos operator->() const { return puv_current_; }

    friend bool operator==(const Iterator& it_a, const Iterator& it_b) {
      if (it_a.b_end_ && it_b.b_end_)
        return true;
      // 终止哨兵等价:done 位对 end 标记。
      // The terminal-sentinel equivalence: the done bit vs the end mark.
      if (it_b.b_end_)
        return it_a.b_done_;
      if (it_a.b_end_)
        return it_b.b_done_;
      return it_a.region_ == it_b.region_ && it_a.int4_u_ == it_b.int4_u_ &&
             it_a.int4_v_ == it_b.int4_v_;
    }

   private:
    friend class ProjectedCellRegion;
    Iterator(const ProjectedCellRegion* region, bool b_end)
        : region_{region}, b_end_{b_end} {
      // Reset(L120-124):枚举器起步于序列首元素之前。
      // Reset (L120-124): the enumerator starts *before* the first element.
      int4_u_ = region_->puv_top_left_.U - 1;
      int4_v_ = region_->puv_top_left_.V;
    }

    const ProjectedCellRegion* region_;
    std::int32_t int4_u_{0};
    std::int32_t int4_v_{0};
    bool b_done_{false};  // MoveNext 溢行 | the MoveNext row overflow
    bool b_end_{false};   // end() 哨兵 | the end() sentinel
    PPos puv_current_{};
  };

  Iterator begin() const { return Iterator{this, false}; }
  Iterator end() const { return Iterator{this, true}; }

 private:
  ProjectedCellRegion(PPos puv_top_left, PPos puv_bottom_right, MPos uv_map_top_left,
                      MPos uv_map_bottom_right)
      : puv_top_left_{puv_top_left}, puv_bottom_right_{puv_bottom_right},
        uv_map_top_left_{uv_map_top_left}, uv_map_bottom_right_{uv_map_bottom_right} {}

  // 区域角点(UPSTREAM 字段名)| the region corners (UPSTREAM field names).
  PPos puv_top_left_;
  PPos puv_bottom_right_;

  // 包含全部可能投影单元格的地图区包围角(上游 readonly 私有字段)。
  // The corners of the bounding map region containing every cell that may
  // project inside (upstream's private readonly fields).
  MPos uv_map_top_left_;
  MPos uv_map_bottom_right_;
};

}  // namespace ora::gfx
