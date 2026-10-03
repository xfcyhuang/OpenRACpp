// UPSTREAM: OpenRA.Game/Map/MapGrid.cs @7d57605 L20(地图网格类型枚举)
//          OpenRA.Game/MPos.cs @7d57605 L17-93(MPos/PPos)
//          OpenRA.Game/CPos.cs @7d57605 L19-146(除 Lua 脚本绑定接口)
// 单元格坐标族:CPos(单元格位,12/12/8 位打包)、MPos(地图位)、PPos(投影位)。
// CPos.ToMPos 与 MPos.ToCPos 互相依赖,故三类型合置于本文件并按依赖序定义。
// 字段/方法名保留 C# 原名以保持审计对照;C# Lua 绑定不移植(Phase 8)。
#pragma once
import std;

#include "cvec.hpp"

namespace ora {

/// 地图网格类型(MapGrid.cs L20):RectangularIsometric 用于 RA2 类等距地图
enum class MapGridType : std::uint8_t { Rectangular, RectangularIsometric };

/// 地图(存储)坐标(MPos.cs L17)
struct CPos;  // 前置声明:ToCPos 返回类型,定义在后

struct MPos {
  std::int32_t U{0};  // 地图列(保留上游字段名)
  std::int32_t V{0};  // 地图行

  constexpr MPos() = default;
  constexpr MPos(std::int32_t int4_u, std::int32_t int4_v) : U{int4_u}, V{int4_v} {}

  static constexpr MPos Zero() { return MPos{0, 0}; }

  /// 夹取到矩形内(MPos.cs L32-35):Min(Right, Max(u, Left))
  constexpr MPos Clamp(Rectangle const& rect_r) const {
    return MPos{std::min(rect_r.Right(), std::max(U, rect_r.Left())),
                std::min(rect_r.Bottom(), std::max(V, rect_r.Top()))};
  }

  /// 上游 GetHashCode(MPos.cs L27):U ^ V
  constexpr std::int32_t Hash() const { return U ^ V; }

  constexpr CPos ToCPos(MapGridType grid_type) const;  // 定义在 CPos 之后

  friend constexpr bool operator==(MPos uv_a, MPos uv_b) { return uv_a.U == uv_b.U && uv_a.V == uv_b.V; }
  friend constexpr bool operator!=(MPos uv_a, MPos uv_b) { return !(uv_a == uv_b); }
};

/// 投影地图坐标(MPos.cs L68)。地图渲染/可见性域使用的 (U,V) 对
struct PPos {
  std::int32_t U{0};  // 投影列
  std::int32_t V{0};  // 投影行

  constexpr PPos() = default;
  constexpr PPos(std::int32_t int4_u, std::int32_t int4_v) : U{int4_u}, V{int4_v} {}

  static constexpr PPos Zero() { return PPos{0, 0}; }

  constexpr PPos Clamp(Rectangle const& rect_r) const {
    return PPos{std::min(rect_r.Right(), std::max(U, rect_r.Left())),
                std::min(rect_r.Bottom(), std::max(V, rect_r.Top()))};
  }

  constexpr std::int32_t Hash() const { return U ^ V; }

  friend constexpr bool operator==(PPos uv_a, PPos uv_b) { return uv_a.U == uv_b.U && uv_a.V == uv_b.V; }
  friend constexpr bool operator!=(PPos uv_a, PPos uv_b) { return !(uv_a == uv_b); }
};

/// MPos ↔ PPos 显式互转(MPos.cs L78-79)
constexpr MPos ToMPos(PPos puv_v) { return MPos{puv_v.U, puv_v.V}; }
constexpr PPos ToPPos(MPos uv_v) { return PPos{uv_v.U, uv_v.V}; }

/// 单元格坐标(CPos.cs L19)。X/Y 各 12 位有符号(-2048..2047),Layer 8 位无符号,
/// 打包为 XXXX XXXX XXXX YYYY YYYY YYYY LLLL LLLL(保留上游字段名 Bits)
struct CPos {
  std::int32_t Bits{0};  // 32 位打包表示;Bits==0 即 CPos.Zero

  constexpr CPos() = default;
  constexpr explicit CPos(std::int32_t int4_bits) : Bits{int4_bits} {}
  constexpr CPos(std::int32_t int4_x, std::int32_t int4_y) : CPos{int4_x, int4_y, 0} {}
  constexpr CPos(std::int32_t int4_x, std::int32_t int4_y, std::uint8_t uint1_layer)
      : Bits{(int4_x & 0xFFF) << 20 | (int4_y & 0xFFF) << 8 | int4_layer_value(uint1_layer)} {}

  static constexpr CPos Zero() { return CPos{0, 0, 0}; }

  /// X 左对齐 MSB,算术右移自带符号扩展(CPos.cs L29)
  constexpr std::int32_t X() const { return Bits >> 20; }
  /// Y 先对齐 short 再算术右移,复刻 C# ((short)(Bits >> 4)) >> 4 的符号扩展(CPos.cs L33)
  constexpr std::int32_t Y() const {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(Bits >> 4)) >> 4;
  }
  /// Layer 取最低字节(CPos.cs L35)
  constexpr std::uint8_t Layer() const { return static_cast<std::uint8_t>(Bits); }

  /// 上游 GetHashCode(CPos.cs L57)
  constexpr std::int32_t Hash() const { return Bits; }

  /// 单元格坐标 → 地图坐标(CPos.cs L75-85)。等距网格交错行的换算照抄上游
  constexpr MPos ToMPos(MapGridType grid_type) const {
    if (grid_type == MapGridType::Rectangular)
      return MPos{X(), Y()};

    const std::int32_t int4_v{X() + Y()};                       // 交错行号
    const std::int32_t int4_u{(int4_v - (int4_v & 1)) / 2 - Y()};  // 去奇数位后折半再平移
    return MPos{int4_u, int4_v};
  }

  friend constexpr CPos operator+(CVec v_a, CPos cell_b) {  // CVec + CPos(CPos.cs L49)
    return CPos{v_a.X + cell_b.X(), v_a.Y + cell_b.Y(), cell_b.Layer()};
  }
  friend constexpr CPos operator+(CPos cell_a, CVec v_b) {  // CPos + CVec(CPos.cs L50)
    return CPos{cell_a.X() + v_b.X, cell_a.Y() + v_b.Y, cell_a.Layer()};
  }
  friend constexpr CPos operator-(CPos cell_a, CVec v_b) {  // CPos - CVec(CPos.cs L51)
    return CPos{cell_a.X() - v_b.X, cell_a.Y() - v_b.Y, cell_a.Layer()};
  }
  friend constexpr CVec operator-(CPos cell_a, CPos cell_b) {  // CPos - CPos → CVec(L52)
    return CVec{cell_a.X() - cell_b.X(), cell_a.Y() - cell_b.Y()};
  }
  friend constexpr bool operator==(CPos cell_a, CPos cell_b) { return cell_a.Bits == cell_b.Bits; }
  friend constexpr bool operator!=(CPos cell_a, CPos cell_b) { return !(cell_a == cell_b); }

 private:
  // 构造打包用的 Layer 辅助:保持公有构造体为委托调用形式,与上游三元重载链对应
  static constexpr std::int32_t int4_layer_value(std::uint8_t uint1_layer) {
    return static_cast<std::int32_t>(uint1_layer);
  }
};

/// 地图坐标 → 单元格坐标(MPos.cs L45-62)。等距交错行换算照抄上游注释中的推导
constexpr CPos MPos::ToCPos(MapGridType grid_type) const {
  if (grid_type == MapGridType::Rectangular)
    return CPos{U, V};

  const std::int32_t int4_y{(V - (V & 1)) / 2 - U};  // 去奇数位折半再平移
  const std::int32_t int4_x{V - int4_y};             // 行号减纵坐标得横坐标
  return CPos{int4_x, int4_y};
}

}  // namespace ora
