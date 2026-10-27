#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "formats/span_reader.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

class R8Frame : public gfx::ISpriteFrame {
 public:

  R8Frame(SpanReader& reader_source, const std::vector<std::uint32_t>* ptr_last_palette);

  gfx::SpriteFrameType Type() const override { return type_frame_; }
  int2 Size() const override { return int2_size_; }
  int2 FrameSize() const override { return int2_frame_size_; }
  core::Vector2 Offset() const override { return vec2_offset_; }
  std::span<const std::byte> Data() const override { return vec_data_; }
  bool DisableExportPadding() const override { return true; }

  const std::optional<std::vector<std::uint32_t>>& Palette() const { return opt_palette_; }

 private:
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Indexed8;
  int2 int2_size_{};
  int2 int2_frame_size_{};
  core::Vector2 vec2_offset_{};
  std::vector<std::byte> vec_data_;
  std::optional<std::vector<std::uint32_t>> opt_palette_;
};

class R8RemappableFrame : public gfx::ISpriteFrame {
 public:

  explicit R8RemappableFrame(R8Frame frame_inner, bool b_use_shadow = true,
                             bool b_convert_shroud_to_fog = false,
                             core::Color color_remap = core::Color{})
      : frame_inner_{std::move(frame_inner)},
        b_use_shadow_{b_use_shadow},
        b_convert_shroud_to_fog_{b_convert_shroud_to_fog},
        color_remap_{color_remap} {}

  gfx::SpriteFrameType Type() const override { return gfx::SpriteFrameType::Bgra32; }
  int2 Size() const override { return frame_inner_.Size(); }
  int2 FrameSize() const override { return frame_inner_.FrameSize(); }
  core::Vector2 Offset() const override { return frame_inner_.Offset(); }
  bool DisableExportPadding() const override { return frame_inner_.DisableExportPadding(); }
  std::span<const std::byte> Data() const override;

  R8RemappableFrame WithSequenceFlags(bool b_use_shadow, bool b_convert_shroud_to_fog,
                                      core::Color color_remap) const {
    return R8RemappableFrame{frame_inner_, b_use_shadow, b_convert_shroud_to_fog, color_remap};
  }

 private:
  R8Frame frame_inner_;
  bool b_use_shadow_;
  bool b_convert_shroud_to_fog_;
  core::Color color_remap_;
  mutable std::vector<std::byte> vec_data_cached_;
  mutable bool b_data_materialized_ = false;
};

bool IsR8(std::span<const std::byte> vec_file);

bool TryParseR8(std::span<const std::byte> vec_file,
                std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

}
