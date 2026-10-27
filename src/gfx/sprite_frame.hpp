#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite.hpp"

namespace ora::gfx {

struct ISpriteFrame {
  virtual SpriteFrameType Type() const = 0;
  virtual int2 Size() const = 0;
  virtual int2 FrameSize() const = 0;
  virtual core::Vector2 Offset() const = 0;
  virtual std::span<const std::byte> Data() const = 0;
  virtual bool DisableExportPadding() const = 0;
  virtual ~ISpriteFrame() = default;
};

}
