#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/rectangle.hpp"

namespace ora::gfx {

class ProjectedCellRegion {
 public:

  ProjectedCellRegion() = default;

  static ProjectedCellRegion Make(PPos puv_top_left, PPos puv_bottom_right,
                                  std::function<MPos(MPos)> fn_height_clamp, bool b_isometric,
                                  std::int32_t int4_max_height) {

    const MPos uv_map_top_left = ToMPos(puv_top_left);

    const std::int32_t int4_height_offset =
        b_isometric ? int4_max_height : int4_max_height / 2;

    const MPos uv_map_bottom_right = fn_height_clamp(MPos{puv_bottom_right.U,
                                                           puv_bottom_right.V + int4_height_offset});

    return ProjectedCellRegion{puv_top_left, puv_bottom_right, uv_map_top_left,
                               uv_map_bottom_right};
  }

  bool Contains(PPos puv_p) const {
    return puv_p.U >= puv_top_left_.U && puv_p.U <= puv_bottom_right_.U &&
           puv_p.V >= puv_top_left_.V && puv_p.V <= puv_bottom_right_.V;
  }

  std::pair<MPos, MPos> CandidateMapCoords() const { return {uv_map_top_left_, uv_map_bottom_right_}; }

  PPos TopLeft() const { return puv_top_left_; }
  PPos BottomRight() const { return puv_bottom_right_; }

  class Iterator {
   public:

    Iterator& operator++() {
      int4_u_++;

      if (int4_u_ > region_->puv_bottom_right_.U) {
        int4_v_++;
        int4_u_ = region_->puv_top_left_.U;

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

      int4_u_ = region_->puv_top_left_.U - 1;
      int4_v_ = region_->puv_top_left_.V;
    }

    const ProjectedCellRegion* region_;
    std::int32_t int4_u_{0};
    std::int32_t int4_v_{0};
    bool b_done_{false};
    bool b_end_{false};
    PPos puv_current_{};
  };

  Iterator begin() const { return Iterator{this, false}; }
  Iterator end() const { return Iterator{this, true}; }

 private:
  ProjectedCellRegion(PPos puv_top_left, PPos puv_bottom_right, MPos uv_map_top_left,
                      MPos uv_map_bottom_right)
      : puv_top_left_{puv_top_left}, puv_bottom_right_{puv_bottom_right},
        uv_map_top_left_{uv_map_top_left}, uv_map_bottom_right_{uv_map_bottom_right} {}

  PPos puv_top_left_;
  PPos puv_bottom_right_;

  MPos uv_map_top_left_;
  MPos uv_map_bottom_right_;
};

}
