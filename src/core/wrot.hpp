// UPSTREAM: OpenRA.Game/WRot.cs @7d57605 L19-221(全类型)
// 三维世界旋转:公开欧拉角(Roll/Pitch/Yaw)+ 内部整数四元数(1024 == 1.0)。
// 上游 x/y/z/w 为 private;C++ 侧公开——黄金对拍与后续序列化需要读取四元数分量,
// 只读约束以命名与注释约定(修改四元数而不重导欧拉角会破坏不变量)。
// WVec::Rotate(WRot) 的定义在本文件尾部(依赖 WRot 完整类型)。
// 3D world rotation: public Euler angles (Roll/Pitch/Yaw) + internal integer quaternion (1024 == 1.0).
// Upstream x/y/z/w are private; exposed on the C++ side -- golden-differential testing and later
// serialization need to read the quaternion components, so the read-only constraint is upheld by
// naming and comment convention (mutating the quaternion without re-deriving the Euler angles breaks the invariant).
// WVec::Rotate(WRot) is defined at the end of this file (requires the complete WRot type).
#pragma once
import std;

#include "exts_math.hpp"
#include "int2.hpp"
#include "wvec.hpp"

namespace ora {

/// 三维世界旋转(WRot.cs L19)
/// 3D world rotation (WRot.cs L19)
struct WRot {
  WAngle Roll{0};   // 欧拉滚转(直观公开表示,保留上游字段名) | Euler roll (intuitive public representation; upstream field name kept)
  WAngle Pitch{0};  // 欧拉俯仰 | Euler pitch
  WAngle Yaw{0};    // 欧拉偏航 | Euler yaw
  std::int32_t x{0};  // 四元数分量(上游 private;1024 == 1.0,勿直接改写) | Quaternion component (upstream private; 1024 == 1.0, do not write directly)
  std::int32_t y{0};
  std::int32_t z{0};
  std::int32_t w{0};

  /// 欧拉角构造(WRot.cs L30-52):角度顺时针增加,四元数归一化到 1024 == 1.0
  /// Euler-angle constructor (WRot.cs L30-52): angles increase clockwise, quaternion normalized to 1024 == 1.0
  constexpr WRot(WAngle roll_ang, WAngle pitch_ang, WAngle yaw_ang)
      : Roll{roll_ang}, Pitch{pitch_ang}, Yaw{yaw_ang} {
    const WAngle q_roll{-Roll.Angle / 2};
    const WAngle q_pitch{-Pitch.Angle / 2};
    const WAngle q_yaw{-Yaw.Angle / 2};
    const std::int64_t int8_cr{q_roll.Cos()};
    const std::int64_t int8_sr{q_roll.Sin()};
    const std::int64_t int8_cp{q_pitch.Cos()};
    const std::int64_t int8_sp{q_pitch.Sin()};
    const std::int64_t int8_cy{q_yaw.Cos()};
    const std::int64_t int8_sy{q_yaw.Sin()};

    x = static_cast<std::int32_t>((int8_sr * int8_cp * int8_cy - int8_cr * int8_sp * int8_sy) / 1048576);
    y = static_cast<std::int32_t>((int8_cr * int8_sp * int8_cy + int8_sr * int8_cp * int8_sy) / 1048576);
    z = static_cast<std::int32_t>((int8_cr * int8_cp * int8_sy - int8_sr * int8_sp * int8_cy) / 1048576);
    w = static_cast<std::int32_t>((int8_cr * int8_cp * int8_cy + int8_sr * int8_sp * int8_sy) / 1048576);
  }

  /// 轴角构造(WRot.cs L58-67):axis 须归一化到长度 1024
  /// Axis-angle constructor (WRot.cs L58-67): axis must be normalized to length 1024
  constexpr WRot(WVec axis_v, WAngle angle_ang) {
    const WAngle half_ang{-angle_ang.Angle / 2};
    x = axis_v.X * half_ang.Sin() / 1024;
    y = axis_v.Y * half_ang.Sin() / 1024;
    z = axis_v.Z * half_ang.Sin() / 1024;
    w = half_ang.Cos();

    const EulerAngles euler_a{QuaternionToEuler(x, y, z, w)};
    Roll = euler_a.Roll;
    Pitch = euler_a.Pitch;
    Yaw = euler_a.Yaw;
  }

  /// 四元数直入构造(WRot.cs L69-77,上游 private):重导欧拉角
  /// Direct-quaternion constructor (WRot.cs L69-77, upstream private): re-derives the Euler angles
  constexpr WRot(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z, std::int32_t int4_w)
      : x{int4_x}, y{int4_y}, z{int4_z}, w{int4_w} {
    const EulerAngles euler_a{QuaternionToEuler(x, y, z, w)};
    Roll = euler_a.Roll;
    Pitch = euler_a.Pitch;
    Yaw = euler_a.Yaw;
  }

  /// 六分量直组装构造(WRot.cs L97-106,上游 private,一元负使用):不重导欧拉
  /// Six-component direct-assembly constructor (WRot.cs L97-106, upstream private, used by unary minus): no Euler re-derivation
  constexpr WRot(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z, std::int32_t int4_w,
                 WAngle roll_ang, WAngle pitch_ang, WAngle yaw_ang)
      : Roll{roll_ang}, Pitch{pitch_ang}, Yaw{yaw_ang},
        x{int4_x}, y{int4_y}, z{int4_z}, w{int4_w} {}

  static constexpr WRot None() { return WRot{WAngle{0}, WAngle{0}, WAngle{0}}; }
  static constexpr WRot FromFacing(std::int32_t int4_facing) {
    return WRot{WAngle{0}, WAngle{0}, WAngle::FromFacing(int4_facing)};
  }
  static constexpr WRot FromYaw(WAngle yaw_ang) { return WRot{WAngle{0}, WAngle{0}, yaw_ang}; }

  constexpr WRot WithRoll(WAngle roll_ang) const { return WRot{roll_ang, Pitch, Yaw}; }
  constexpr WRot WithPitch(WAngle pitch_ang) const { return WRot{Roll, pitch_ang, Yaw}; }
  constexpr WRot WithYaw(WAngle yaw_ang) const { return WRot{Roll, Pitch, yaw_ang}; }

  /// 四元数乘法复合旋转(WRot.cs L116-130):long 中间量,整除 1024
  /// Quaternion-multiplication rotation composition (WRot.cs L116-130): long intermediates, integer division by 1024
  constexpr WRot Rotate(WRot const& rot_r) const {
    if (*this == None())
      return rot_r;

    if (rot_r == None())
      return *this;

    const std::int64_t int8_rx{(static_cast<std::int64_t>(rot_r.w) * x + static_cast<std::int64_t>(rot_r.x) * w +
                                static_cast<std::int64_t>(rot_r.y) * z - static_cast<std::int64_t>(rot_r.z) * y) / 1024};
    const std::int64_t int8_ry{(static_cast<std::int64_t>(rot_r.w) * y - static_cast<std::int64_t>(rot_r.x) * z +
                                static_cast<std::int64_t>(rot_r.y) * w + static_cast<std::int64_t>(rot_r.z) * x) / 1024};
    const std::int64_t int8_rz{(static_cast<std::int64_t>(rot_r.w) * z + static_cast<std::int64_t>(rot_r.x) * y -
                                static_cast<std::int64_t>(rot_r.y) * x + static_cast<std::int64_t>(rot_r.z) * w) / 1024};
    const std::int64_t int8_rw{(static_cast<std::int64_t>(rot_r.w) * w - static_cast<std::int64_t>(rot_r.x) * x -
                                static_cast<std::int64_t>(rot_r.y) * y - static_cast<std::int64_t>(rot_r.z) * z) / 1024};

    return WRot{static_cast<std::int32_t>(int8_rx), static_cast<std::int32_t>(int8_ry),
                static_cast<std::int32_t>(int8_rz), static_cast<std::int32_t>(int8_rw)};
  }

  /// 定点旋转矩阵(WRot.cs L154-180):四元数 10 位,无溢出风险
  /// Fixed-point rotation matrix (WRot.cs L154-180): 10-bit quaternion, no overflow risk
  constexpr Int32Matrix4x4 AsMatrix() const {
    const std::int32_t int4_lsq{x * x + y * y + z * z + w * w};  // 理论 1024²,舍入可略偏 | nominally 1024², may deviate slightly due to rounding

    return Int32Matrix4x4(
        int4_lsq - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
        2 * (x * y - z * w), int4_lsq - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
        2 * (x * z + y * w), 2 * (y * z - x * w), int4_lsq - 2 * (x * x + y * y), 0,
        0, 0, 0, int4_lsq);
  }

  /// 球面线性插值(WRot.cs L195-220):整数四元数 slerp,末尾重归一化到 1024 == 1.0
  /// Spherical linear interpolation (WRot.cs L195-220): integer-quaternion SLerp, renormalized to 1024 == 1.0 at the end
  static constexpr WRot SLerp(WRot r_a, WRot r_b, std::int32_t int4_mul, std::int32_t int4_div) {
    const std::int32_t int4_dot{r_a.x * r_b.x + r_a.y * r_b.y + r_a.z * r_b.z + r_a.w * r_b.w};
    const std::int32_t int4_flip{int4_dot >= 0 ? 1 : -1};

    if (int4_flip * int4_dot >= 1024 * 1024)  // 同一旋转 | same rotation
      return r_a;

    const WAngle theta_ang{WAngle::ArcCos(int4_dot / 1024)};
    const std::int32_t int4_s1{WAngle{(int4_div - int4_mul) * theta_ang.Angle / int4_div}.Sin()};
    const std::int32_t int4_s2{WAngle{int4_mul * theta_ang.Angle / int4_div}.Sin()};
    const std::int32_t int4_s3{theta_ang.Sin()};

    const std::int64_t int8_x{(static_cast<std::int64_t>(r_a.x) * int4_s1 + int4_flip * r_b.x * int4_s2) / int4_s3};
    const std::int64_t int8_y{(static_cast<std::int64_t>(r_a.y) * int4_s1 + int4_flip * r_b.y * int4_s2) / int4_s3};
    const std::int64_t int8_z{(static_cast<std::int64_t>(r_a.z) * int4_s1 + int4_flip * r_b.z * int4_s2) / int4_s3};
    const std::int64_t int8_w{(static_cast<std::int64_t>(r_a.w) * int4_s1 + int4_flip * r_b.w * int4_s2) / int4_s3};

    const std::int64_t int8_l{ISqrt(int8_x * int8_x + int8_y * int8_y + int8_z * int8_z + int8_w * int8_w)};
    return WRot{static_cast<std::int32_t>(1024 * int8_x / int8_l),
                static_cast<std::int32_t>(1024 * int8_y / int8_l),
                static_cast<std::int32_t>(1024 * int8_z / int8_l),
                static_cast<std::int32_t>(1024 * int8_w / int8_l)};
  }

  /// 上游 GetHashCode(WRot.cs L188)
  /// Upstream GetHashCode (WRot.cs L188)
  constexpr std::int32_t Hash() const { return Roll.Hash() ^ Pitch.Hash() ^ Yaw.Hash(); }

  friend constexpr WRot operator+(WRot r_a, WRot r_b) {  // 欧拉域相加(WRot.cs L112) | addition in the Euler domain (WRot.cs L112)
    return WRot{r_a.Roll + r_b.Roll, r_a.Pitch + r_b.Pitch, r_a.Yaw + r_b.Yaw};
  }
  friend constexpr WRot operator-(WRot r_a, WRot r_b) {  // 欧拉域相减(WRot.cs L113) | subtraction in the Euler domain (WRot.cs L113)
    return WRot{r_a.Roll - r_b.Roll, r_a.Pitch - r_b.Pitch, r_a.Yaw - r_b.Yaw};
  }
  /// 一元负(WRot.cs L114):直接翻转四元数 xyz 与欧拉角,w 保留,不重导
  /// Unary minus (WRot.cs L114): directly negates quaternion xyz and the Euler angles, keeps w, no re-derivation
  friend constexpr WRot operator-(WRot r_a) {
    return WRot{-r_a.x, -r_a.y, -r_a.z, r_a.w, -r_a.Roll, -r_a.Pitch, -r_a.Yaw};
  }
  /// 相等比较仅看欧拉角(WRot.cs L132-135)
  /// Equality compares the Euler angles only (WRot.cs L132-135)
  friend constexpr bool operator==(WRot r_a, WRot r_b) {
    return r_a.Roll == r_b.Roll && r_a.Pitch == r_b.Pitch && r_a.Yaw == r_b.Yaw;
  }
  friend constexpr bool operator!=(WRot r_a, WRot r_b) { return !(r_a == r_b); }

 private:
  /// 四元数 → 欧拉角元组(WRot.cs L79-95)
  /// Quaternion -> Euler-angle tuple (WRot.cs L79-95)
  struct EulerAngles {
    WAngle Roll;
    WAngle Pitch;
    WAngle Yaw;
  };

  static constexpr EulerAngles QuaternionToEuler(std::int32_t int4_x, std::int32_t int4_y,
                                                 std::int32_t int4_z, std::int32_t int4_w) {
    const std::int32_t int4_lsq{int4_x * int4_x + int4_y * int4_y + int4_z * int4_z + int4_w * int4_w};

    const std::int32_t int4_srcp{2 * (int4_w * int4_x + int4_y * int4_z)};
    const std::int32_t int4_crcp{int4_lsq - 2 * (int4_x * int4_x + int4_y * int4_y)};
    const std::int32_t int4_sp{(int4_w * int4_y - int4_z * int4_x) / 512};
    const std::int32_t int4_sycp{2 * (int4_w * int4_z + int4_x * int4_y)};
    const std::int32_t int4_cycp{int4_lsq - 2 * (int4_y * int4_y + int4_z * int4_z)};

    const std::int32_t int4_sp_abs{int4_sp < 0 ? -int4_sp : int4_sp};
    const WAngle pitch_ang{-(int4_sp_abs >= 1024 ? WAngle{int2::SignOf(int4_sp) * 256}
                                                 : WAngle::ArcSin(int4_sp))};

    return EulerAngles{-WAngle::ArcTan(int4_srcp, int4_crcp), pitch_ang,
                       -WAngle::ArcTan(int4_sycp, int4_cycp)};
  }
};

/// WVec::Rotate(WRot)(WVec.cs L49-53):经旋转矩阵
/// WVec::Rotate(WRot) (WVec.cs L49-53): goes through the rotation matrix
inline WVec WVec::Rotate(WRot const& rot_r) const { return Rotate(rot_r.AsMatrix()); }

}  // namespace ora
