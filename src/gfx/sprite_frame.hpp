// UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @b6fc03f L53-71
// ISpriteFrame:格式加载器(SHP/TMP/PNG…)对渲染侧暴露的帧契约。
// 上游为 C# 属性(Type/Size/FrameSize/Offset/Data/DisableExportPadding);
// C++ 侧为纯虚 getter —— 字段语义逐一对应:
//   - Size = Data 的像素尺寸;FrameSize = 整帧画框尺寸(SHPTD 裁边帧
//     FrameSize > Size;TMP 空帧两者皆 0);
//   - Offset = 帧在画框内的偏移(半像素对齐语义,可能为 0.5 步进);
//   - Data = 索引色(BGRA)字节流;上游 null 与 byte[0] 在消费路径
//     (SheetBuilder.Add 的空尺寸早退)不可区分,C++ 统一为空 span。
// ISpriteFrame: the frame contract that format loaders (SHP/TMP/PNG...)
// expose to the render side. Upstream models these as C# properties; the
// C++ side uses pure-virtual getters — field semantics map one to one:
//   - Size is the pixel size of Data; FrameSize is the full frame canvas
//     (SHPTD trimmed frames carry FrameSize > Size; TMP empty tiles zero
//     both);
//   - Offset is the frame's offset within the canvas (half-pixel aligned
//     semantics, in 0.5 steps possible);
//   - Data is the indexed-color (BGRA) byte stream; upstream's null vs
//     byte[0] is indistinguishable on consumer paths (SheetBuilder.Add's
//     empty-size early-out), unified here as an empty span.
#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite.hpp"

namespace ora::gfx {

/// 帧契约接口(SpriteLoader.cs L58-71)。
/// The frame contract interface (SpriteLoader.cs L58-71).
struct ISpriteFrame {
  virtual SpriteFrameType Type() const = 0;
  virtual int2 Size() const = 0;
  virtual int2 FrameSize() const = 0;
  virtual core::Vector2 Offset() const = 0;
  virtual std::span<const std::byte> Data() const = 0;
  virtual bool DisableExportPadding() const = 0;
  virtual ~ISpriteFrame() = default;
};

}  // namespace ora::gfx
