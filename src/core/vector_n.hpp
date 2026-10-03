// UPSTREAM: OpenRA.Game/FieldLoader.cs @7d57605 L531-566(ParseVector2/ParseVector3
//          的 X/Y/Z 数据语义与往返格式;System.Numerics.Vector2/3 的最小等价物)
//          The X/Y/Z data semantics and round-trip format of
//          ParseVector2/ParseVector3 (a minimal equivalent of
//          System.Numerics.Vector2/3).
#pragma once
import std;

namespace ora::core {

/// System.Numerics.Vector2(仅 X/Y 数据语义;渲染运算 Phase 4 随 gfx 扩展)
/// System.Numerics.Vector2 (data semantics only; rendering math extends with
/// gfx in Phase 4).
struct Vector2 {
  float X{};
  float Y{};
  bool operator==(const Vector2&) const = default;
};

/// System.Numerics.Vector3(z 分量可缺省,Vector2 兼容语义见 FieldLoader)
/// System.Numerics.Vector3 (the z component may default — see the FieldLoader
/// Vector2-compat rule).
struct Vector3 {
  float X{};
  float Y{};
  float Z{};
  bool operator==(const Vector3&) const = default;
};

}  // namespace ora::core
