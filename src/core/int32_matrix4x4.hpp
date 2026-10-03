// UPSTREAM: OpenRA.Game/Primitives/Int32Matrix4x4.cs @7d57605 L16-66(全类型)
// 32 位整数 4×4 矩阵,WRot 的旋转矩阵表示(定点数,1024 == 1.0)。
// 字段名保留 C# 原名(M11..M44)以保持审计对照;ToString 上游为字符串拼接,移植为 format。
#pragma once
import std;

namespace ora {

/// 整数 4×4 矩阵(Int32Matrix4x4.cs L16)
struct Int32Matrix4x4 {
  std::int32_t M11{0}, M12{0}, M13{0}, M14{0};  // 第一行(行主序,保留上游字段名)
  std::int32_t M21{0}, M22{0}, M23{0}, M24{0};  // 第二行
  std::int32_t M31{0}, M32{0}, M33{0}, M34{0};  // 第三行
  std::int32_t M41{0}, M42{0}, M43{0}, M44{0};  // 第四行

  constexpr Int32Matrix4x4() = default;

  constexpr Int32Matrix4x4(
      std::int32_t int4_m11, std::int32_t int4_m12, std::int32_t int4_m13, std::int32_t int4_m14,
      std::int32_t int4_m21, std::int32_t int4_m22, std::int32_t int4_m23, std::int32_t int4_m24,
      std::int32_t int4_m31, std::int32_t int4_m32, std::int32_t int4_m33, std::int32_t int4_m34,
      std::int32_t int4_m41, std::int32_t int4_m42, std::int32_t int4_m43, std::int32_t int4_m44)
      : M11{int4_m11}, M12{int4_m12}, M13{int4_m13}, M14{int4_m14},
        M21{int4_m21}, M22{int4_m22}, M23{int4_m23}, M24{int4_m24},
        M31{int4_m31}, M32{int4_m32}, M33{int4_m33}, M34{int4_m34},
        M41{int4_m41}, M42{int4_m42}, M43{int4_m43}, M44{int4_m44} {}

  /// 上游 GetHashCode(L58):M11 ^ M22 ^ M33 ^ M44
  constexpr std::int32_t Hash() const { return M11 ^ M22 ^ M33 ^ M44; }

  friend constexpr bool operator==(Int32Matrix4x4 const& mtx_a, Int32Matrix4x4 const& mtx_b) {
    return mtx_a.M11 == mtx_b.M11 && mtx_a.M12 == mtx_b.M12 && mtx_a.M13 == mtx_b.M13 &&
           mtx_a.M14 == mtx_b.M14 && mtx_a.M21 == mtx_b.M21 && mtx_a.M22 == mtx_b.M22 &&
           mtx_a.M23 == mtx_b.M23 && mtx_a.M24 == mtx_b.M24 && mtx_a.M31 == mtx_b.M31 &&
           mtx_a.M32 == mtx_b.M32 && mtx_a.M33 == mtx_b.M33 && mtx_a.M34 == mtx_b.M34 &&
           mtx_a.M41 == mtx_b.M41 && mtx_a.M42 == mtx_b.M42 && mtx_a.M43 == mtx_b.M43 &&
           mtx_a.M44 == mtx_b.M44;
  }
  friend constexpr bool operator!=(Int32Matrix4x4 const& mtx_a, Int32Matrix4x4 const& mtx_b) {
    return !(mtx_a == mtx_b);
  }
};

}  // namespace ora
