/* ORA_PRIMITIVES — OpenRA 定点原语(C23 移植)
 *
 * 对应 C# 源(逐语义对齐,含整数回绕/截断行为):
 *   int2          OpenRA.Game/Primitives/int2.cs
 *   Rectangle     OpenRA.Game/Primitives/Rectangle.cs(Right = X + Width)
 *   CPos/CVec     OpenRA.Game/CPos.cs, CVec.cs
 *   MPos/PPos     OpenRA.Game/MPos.cs
 *   WDist         OpenRA.Game/WDist.cs
 *   WAngle        OpenRA.Game/WAngle.cs(整数三角,1024 = 整圆)
 *   WPos/WVec     OpenRA.Game/WPos.cs, WVec.cs(1024 = 1 cell)
 *   WRot          OpenRA.Game/WRot.cs(整数四元数)
 *   Int32Matrix4x4 OpenRA.Game/Primitives/Int32Matrix4x4.cs
 *
 * 命名约定:类型 ora_xxx;构造 ora_xxx(...);运算 ora_xxx_add/sub/...
 * 所有中间整数运算与 C# unchecked int 语义一致(依赖 -fwrapv,见 CMakeLists)。
 * 除法一律向零截断(C 与 C# 一致);C# decimal 的两处 LerpQuadratic 用 __int128
 * 精确整数除法复刻(理论差异仅在商小数部分连续 ≥12 个 9 时出现,见函数注释)。
 */
#ifndef ORA_PRIMITIVES_H
#define ORA_PRIMITIVES_H

#include <stdint.h>
#include <assert.h>
#include "ora_math.h"
#include "wangle_tables.h"

/* ————————————————————————— 标量辅助 ————————————————————————— */

static inline int32_t ora_min_i32(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t ora_max_i32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t ora_wclamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int64_t ora_wclamp_i64(int64_t v, int64_t lo, int64_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ————————————————————————— int2 ————————————————————————— */

typedef struct ora_int2 { int32_t x, y; } ora_int2;

static inline ora_int2 ora_int2_mk(int32_t x, int32_t y) { return (ora_int2){ x, y }; }
static inline ora_int2 ora_int2_add(ora_int2 a, ora_int2 b) { return (ora_int2){ a.x + b.x, a.y + b.y }; }
static inline ora_int2 ora_int2_sub(ora_int2 a, ora_int2 b) { return (ora_int2){ a.x - b.x, a.y - b.y }; }
static inline ora_int2 ora_int2_neg(ora_int2 a) { return (ora_int2){ -a.x, -a.y }; }
static inline ora_int2 ora_int2_mul(ora_int2 b, int32_t a) { return (ora_int2){ a * b.x, a * b.y }; }
static inline ora_int2 ora_int2_div(ora_int2 a, int32_t b) { return (ora_int2){ a.x / b, a.y / b }; }
static inline bool ora_int2_eq(ora_int2 a, ora_int2 b) { return a.x == b.x && a.y == b.y; }
static inline int32_t ora_int2_sign1(int32_t v) { return (v > 0) - (v < 0); } /* Math.Sign */
static inline ora_int2 ora_int2_sign(ora_int2 a) { return (ora_int2){ ora_int2_sign1(a.x), ora_int2_sign1(a.y) }; }
static inline ora_int2 ora_int2_abs(ora_int2 a) { return (ora_int2){ a.x < 0 ? -a.x : a.x, a.y < 0 ? -a.y : a.y }; }
static inline int32_t ora_int2_length_sq(ora_int2 a) { return a.x * a.x + a.y * a.y; }
static inline int32_t ora_int2_length(ora_int2 a) { return ora_isqrt_i32(ora_int2_length_sq(a), ORA_ISQRT_FLOOR); }
static inline ora_int2 ora_int2_max(ora_int2 a, ora_int2 b) { return (ora_int2){ a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y }; }
static inline ora_int2 ora_int2_min(ora_int2 a, ora_int2 b) { return (ora_int2){ a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y }; }
static inline int32_t ora_int2_dot(ora_int2 a, ora_int2 b) { return a.x * b.x + a.y * b.y; }
/* int2.Lerp(int a, int b, int mul, int div) */
static inline int32_t ora_int2_lerp_i(int32_t a, int32_t b, int32_t mul, int32_t div) { return a + (b - a) * mul / div; }
static inline ora_int2 ora_int2_lerp(ora_int2 a, ora_int2 b, int32_t mul, int32_t div) { return ora_int2_add(a, ora_int2_div(ora_int2_mul(ora_int2_sub(b, a), mul), div)); }

/* int2.Swap:翻转 uint32 字节序 */
static inline uint32_t ora_int2_swap_u32(uint32_t orig)
{
    return ((orig & 0xff000000u) >> 24) | ((orig & 0x00ff0000u) >> 8) |
           ((orig & 0x0000ff00u) << 8) | ((orig & 0x000000ffu) << 24);
}

/* ————————————————————————— Rectangle ————————————————————————— */

typedef struct ora_rect { int32_t x, y, w, h; } ora_rect;

static inline int32_t ora_rect_right(ora_rect r) { return r.x + r.w; }   /* Rectangle.Right = X + Width */
static inline int32_t ora_rect_bottom(ora_rect r) { return r.y + r.h; }

/* ————————————————————————— CVec ————————————————————————— */

typedef struct ora_cvec { int32_t x, y; } ora_cvec;

static inline ora_cvec ora_cvec_mk(int32_t x, int32_t y) { return (ora_cvec){ x, y }; }
static inline ora_cvec ora_cvec_add(ora_cvec a, ora_cvec b) { return (ora_cvec){ a.x + b.x, a.y + b.y }; }
static inline ora_cvec ora_cvec_sub(ora_cvec a, ora_cvec b) { return (ora_cvec){ a.x - b.x, a.y - b.y }; }
static inline ora_cvec ora_cvec_mul(ora_cvec b, int32_t a) { return (ora_cvec){ a * b.x, a * b.y }; }
static inline ora_cvec ora_cvec_div(ora_cvec a, int32_t b) { return (ora_cvec){ a.x / b, a.y / b }; }
static inline ora_cvec ora_cvec_neg(ora_cvec a) { return (ora_cvec){ -a.x, -a.y }; }
static inline bool ora_cvec_eq(ora_cvec a, ora_cvec b) { return a.x == b.x && a.y == b.y; }
static inline ora_cvec ora_cvec_max(ora_cvec a, ora_cvec b) { return (ora_cvec){ a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y }; }
static inline ora_cvec ora_cvec_min(ora_cvec a, ora_cvec b) { return (ora_cvec){ a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y }; }
static inline int32_t ora_cvec_dot(ora_cvec a, ora_cvec b) { return a.x * b.x + a.y * b.y; }
static inline ora_cvec ora_cvec_sign(ora_cvec a) { return (ora_cvec){ ora_int2_sign1(a.x), ora_int2_sign1(a.y) }; }
static inline ora_cvec ora_cvec_abs(ora_cvec a) { return (ora_cvec){ a.x < 0 ? -a.x : a.x, a.y < 0 ? -a.y : a.y }; }
static inline int32_t ora_cvec_length_sq(ora_cvec a) { return a.x * a.x + a.y * a.y; }
static inline int32_t ora_cvec_length(ora_cvec a) { return ora_isqrt_i32(ora_cvec_length_sq(a), ORA_ISQRT_FLOOR); }
static inline ora_cvec ora_cvec_clamp(ora_cvec a, ora_rect r)
{
    /* CVec.Clamp(Rectangle)(CVec.cs:50-55):Min(Right, Max(v, Left)) */
    int32_t rx = ora_min_i32(r.x + r.w, a.x < r.x ? r.x : a.x);
    int32_t ry = ora_min_i32(r.y + r.h, a.y < r.y ? r.y : a.y);
    return (ora_cvec){ rx, ry };
}

/* ————————————————————————— CPos(12/12/8 位打包)————————————————————————— */

typedef struct ora_cpos { int32_t bits; } ora_cpos;

/* CPos(x, y, layer):XXXX XXXX XXXX YYYY YYYY YYYY LLLL LLLL(CPos.cs:40-43) */
static inline ora_cpos ora_cpos_mk(int32_t x, int32_t y, uint8_t layer)
{
    return (ora_cpos){ (int32_t)(((uint32_t)(x & 0xFFF) << 20) | ((uint32_t)(y & 0xFFF) << 8) | (uint32_t)layer) };
}
static inline ora_cpos ora_cpos_bits(int32_t bits) { return (ora_cpos){ bits }; }
static inline int32_t ora_cpos_x(ora_cpos p) { return p.bits >> 20; }
static inline int32_t ora_cpos_y(ora_cpos p) { return (int32_t)((int16_t)(p.bits >> 4)) >> 4; }
static inline uint8_t ora_cpos_layer(ora_cpos p) { return (uint8_t)p.bits; }
static inline bool ora_cpos_eq(ora_cpos a, ora_cpos b) { return a.bits == b.bits; }

static inline ora_cpos ora_cpos_add(ora_cvec a, ora_cpos b)
{
    return ora_cpos_mk(a.x + ora_cpos_x(b), a.y + ora_cpos_y(b), ora_cpos_layer(b));
}
static inline ora_cpos ora_cpos_sub(ora_cpos a, ora_cvec b)
{
    return ora_cpos_mk(ora_cpos_x(a) - b.x, ora_cpos_y(a) - b.y, ora_cpos_layer(a));
}
static inline ora_cvec ora_cpos_diff(ora_cpos a, ora_cpos b)
{
    return (ora_cvec){ ora_cpos_x(a) - ora_cpos_x(b), ora_cpos_y(a) - ora_cpos_y(b) };
}

/* ————————————————————————— MapGridType / MPos / PPos ————————————————————————— */

typedef enum ora_map_grid_type {
    ORA_GRID_RECTANGULAR = 0,
    ORA_GRID_RECTANGULAR_ISOMETRIC
} ora_map_grid_type;

typedef struct ora_mpos { int32_t u, v; } ora_mpos;
typedef struct ora_ppos { int32_t u, v; } ora_ppos;

static inline ora_mpos ora_mpos_mk(int32_t u, int32_t v) { return (ora_mpos){ u, v }; }
static inline bool ora_mpos_eq(ora_mpos a, ora_mpos b) { return a.u == b.u && a.v == b.v; }

static inline ora_mpos ora_mpos_clamp(ora_mpos a, ora_rect r)
{
    int32_t u = ora_min_i32(ora_rect_right(r), a.u < r.x ? r.x : a.u);
    int32_t v = ora_min_i32(ora_rect_bottom(r), a.v < r.y ? r.y : a.v);
    return (ora_mpos){ u, v };
}

static inline ora_ppos ora_ppos_mk(int32_t u, int32_t v) { return (ora_ppos){ u, v }; }

/* CPos.ToMPos(MapGridType)(CPos.cs:75-85):等距网格的交错行换算 */
static inline ora_mpos ora_cpos_to_mpos(ora_cpos c, ora_map_grid_type grid)
{
    int32_t x = ora_cpos_x(c), y = ora_cpos_y(c);
    if (grid == ORA_GRID_RECTANGULAR)
        return (ora_mpos){ x, y };

    int32_t v = x + y;
    int32_t u = (v - (v & 1)) / 2 - y;
    return (ora_mpos){ u, v };
}

/* MPos.ToCPos(MapGridType)(MPos.cs:45-62) */
static inline ora_cpos ora_mpos_to_cpos(ora_mpos m, ora_map_grid_type grid)
{
    if (grid == ORA_GRID_RECTANGULAR)
        return ora_cpos_mk(m.u, m.v, 0);

    int32_t y = (m.v - (m.v & 1)) / 2 - m.u;
    int32_t x = m.v - y;
    return ora_cpos_mk(x, y, 0);
}

/* ————————————————————————— WDist ————————————————————————— */

typedef struct ora_wdist { int32_t length; } ora_wdist;

static inline ora_wdist ora_wdist_mk(int32_t r) { return (ora_wdist){ r }; }
static inline ora_wdist ora_wdist_from_cells(int32_t cells) { return (ora_wdist){ 1024 * cells }; }
static inline ora_wdist ora_wdist_add(ora_wdist a, ora_wdist b) { return (ora_wdist){ a.length + b.length }; }
static inline ora_wdist ora_wdist_sub(ora_wdist a, ora_wdist b) { return (ora_wdist){ a.length - b.length }; }
static inline ora_wdist ora_wdist_neg(ora_wdist a) { return (ora_wdist){ -a.length }; }
static inline ora_wdist ora_wdist_div(ora_wdist a, int32_t b) { return (ora_wdist){ a.length / b }; }
static inline ora_wdist ora_wdist_mul(ora_wdist a, int32_t b) { return (ora_wdist){ a.length * b }; }
static inline bool ora_wdist_eq(ora_wdist a, ora_wdist b) { return a.length == b.length; }
static inline int64_t ora_wdist_length_sq(ora_wdist a) { return (int64_t)a.length * a.length; }

/* ————————————————————————— WAngle ————————————————————————— */

typedef struct ora_wangle { int32_t angle; } ora_wangle;

static inline ora_wangle ora_wangle_mk(int32_t a) { return (ora_wangle){ a & 1023 }; } /* 位掩码处理回绕与负数 */
static inline ora_wangle ora_wangle_from_facing(int32_t facing) { return ora_wangle_mk(facing * 4); }
static inline ora_wangle ora_wangle_from_degrees(int32_t degrees) { return ora_wangle_mk(degrees * 1024 / 360); }
static inline ora_wangle ora_wangle_add(ora_wangle a, ora_wangle b) { return ora_wangle_mk(a.angle + b.angle); }
static inline ora_wangle ora_wangle_sub(ora_wangle a, ora_wangle b) { return ora_wangle_mk(a.angle - b.angle); }
static inline ora_wangle ora_wangle_neg(ora_wangle a) { return ora_wangle_mk(-a.angle); }
static inline ora_wangle ora_wangle_mul(ora_wangle a, int32_t b) { return ora_wangle_mk(a.angle * b); }
static inline ora_wangle ora_wangle_div_i(ora_wangle a, int32_t b) { return ora_wangle_mk(a.angle / b); }
static inline int32_t ora_wangle_div_w(ora_wangle a, ora_wangle b) { return a.angle / b.angle; }
static inline bool ora_wangle_eq(ora_wangle a, ora_wangle b) { return a.angle == b.angle; }
static inline int32_t ora_wangle_facing(ora_wangle a) { return a.angle / 4; }
static inline int32_t ora_wangle_angle_sq(ora_wangle a) { return a.angle * a.angle; }

static inline int32_t ora_wangle_cos(ora_wangle a)
{
    /* WAngle.cs:64-86:对称映射 1024 圆到 0-256 象限索引,查表 + 无分支符号 */
    int32_t angle = a.angle;

    int32_t q_index = angle & 511;
    int32_t mirrored = 256 - q_index;
    int32_t mask = mirrored >> 31;
    int32_t final_index = 256 - ((mirrored ^ mask) - mask); /* 无分支 abs */

    int32_t sign_bit = (int32_t)((uint32_t)(angle + 256) >> 9) & 1; /* 相移 90° 把负半球对齐到第 9 位 */
    int32_t sign = 1 - (sign_bit << 1);

    return sign * WANGLE_COSINE_TABLE[(uint32_t)final_index];
}

static inline int32_t ora_wangle_sin(ora_wangle a) { return ora_wangle_cos(ora_wangle_mk(a.angle - 256)); }

static inline int32_t ora_wangle_tan(ora_wangle a)
{
    /* WAngle.cs:88-106 */
    int32_t angle = a.angle & 511;

    int32_t shifted = angle - 257; /* +257 保持 90° 渐近线(256)为正 */
    int32_t mask = shifted >> 31;

    int32_t t = angle - 256; /* 无分支 abs(angle - 256):0-511 → 256-0-255 三角波 */
    int32_t triangle = ((t ^ (t >> 31)) - (t >> 31));
    int32_t final_index = 256 - triangle;

    int32_t sign = -1 - (mask << 1);

    return sign * WANGLE_TAN_TABLE[(uint32_t)final_index];
}

/* WAngle.Lerp(WAngle.cs:108-121):跨 1024 回绕取最短路径 */
static inline ora_wangle ora_wangle_lerp(ora_wangle a, ora_wangle b, int32_t mul, int32_t div)
{
    int32_t start = a.angle;
    int32_t diff = b.angle - start;

    int32_t mask1 = (511 - diff) >> 31;
    int32_t mask2 = (diff + 512) >> 31;
    diff += (mask1 & -1024) | (mask2 & 1024);

    return ora_wangle_mk(start + diff * mul / div);
}

/* GetClosestCosineIndex(WAngle.cs:151-172):瀑布二分取下界后选最近邻 */
static inline int32_t ora_wangle_closest_cos_index(int32_t value)
{
    int32_t index = 0;

    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 128)] > value) ? 128 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 64)] > value) ? 64 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 32)] > value) ? 32 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 16)] > value) ? 16 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 8)] > value) ? 8 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 4)] > value) ? 4 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 2)] > value) ? 2 : 0;
    index |= (WANGLE_COSINE_TABLE[(uint32_t)(index | 1)] > value) ? 1 : 0;

    int32_t val0 = WANGLE_COSINE_TABLE[(uint32_t)index];
    int32_t idx1 = index + 1 < 256 ? index + 1 : 256;
    int32_t val1 = WANGLE_COSINE_TABLE[(uint32_t)idx1];

    return (val0 - value > value - val1) ? index + 1 : index;
}

static inline ora_wangle ora_wangle_arcsin(int32_t d)
{
    /* WAngle.cs:123-135;d ∈ [-1024,1024],越界在 C# 抛异常,这里断言 */
    assert((uint32_t)(d + 1024) <= 2048);

    int32_t index = ora_wangle_closest_cos_index(d < 0 ? -d : d);
    int32_t sign = d >> 31;

    return ora_wangle_mk((sign & (768 + index)) | (~sign & (256 - index)));
}

static inline ora_wangle ora_wangle_arccos(int32_t d)
{
    /* WAngle.cs:137-147 */
    assert((uint32_t)(d + 1024) <= 2048);

    int32_t index = ora_wangle_closest_cos_index(d < 0 ? -d : d);
    int32_t sign = d >> 31;

    return ora_wangle_mk((sign & (512 - index)) | (~sign & index));
}

static inline ora_wangle ora_wangle_arctan(int32_t y, int32_t x)
{
    /* WAngle.cs:174-216 */
    if (y == 0)
        return ora_wangle_mk(x >= 0 ? 0 : 512);

    if (x == 0)
        return ora_wangle_mk(y > 0 ? 256 : 768);

    int64_t ay = y < 0 ? -(int64_t)y : (int64_t)y;
    int64_t ax = x < 0 ? -(int64_t)x : (int64_t)x;

    /* 比值超出正切表精度极限(~89.6°)时返回 90° */
    if (ay >= ax * 167)
        return ora_wangle_mk(y > 0 ? 256 : 768);

    int32_t target = (int32_t)((ay << 10) / ax);

    int32_t index = 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 128)] <= target) ? 128 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 64)] <= target) ? 64 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 32)] <= target) ? 32 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 16)] <= target) ? 16 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 8)] <= target) ? 8 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 4)] <= target) ? 4 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 2)] <= target) ? 2 : 0;
    index |= (WANGLE_TAN_TABLE[(uint32_t)(index | 1)] <= target) ? 1 : 0;

    int32_t val = WANGLE_TAN_TABLE[(uint32_t)index];
    int32_t next_val = WANGLE_TAN_TABLE[(uint32_t)(index + 1)];
    index += (target - val > next_val - target) ? 1 : 0;

    int32_t x_neg_result = 512 + (y < 0 ? index : -index);
    int32_t x_pos_result = y < 0 ? 1024 - index : index;

    return ora_wangle_mk(x < 0 ? x_neg_result : x_pos_result);
}

/* ————————————————————————— WPos / WVec ————————————————————————— */

typedef struct ora_wpos { int32_t x, y, z; } ora_wpos;
typedef struct ora_wvec { int32_t x, y, z; } ora_wvec;

static inline ora_wpos ora_wpos_mk(int32_t x, int32_t y, int32_t z) { return (ora_wpos){ x, y, z }; }
static inline ora_wvec ora_wvec_mk(int32_t x, int32_t y, int32_t z) { return (ora_wvec){ x, y, z }; }
static inline ora_wpos ora_wpos_add(ora_wpos a, ora_wvec b) { return (ora_wpos){ a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline ora_wpos ora_wpos_sub(ora_wpos a, ora_wvec b) { return (ora_wpos){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline ora_wvec ora_wpos_diff(ora_wpos a, ora_wpos b) { return (ora_wvec){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline bool ora_wpos_eq(ora_wpos a, ora_wpos b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static inline ora_wvec ora_wvec_add(ora_wvec a, ora_wvec b) { return (ora_wvec){ a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline ora_wvec ora_wvec_sub(ora_wvec a, ora_wvec b) { return (ora_wvec){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline ora_wvec ora_wvec_neg(ora_wvec a) { return (ora_wvec){ -a.x, -a.y, -a.z }; }
static inline ora_wvec ora_wvec_div(ora_wvec a, int32_t b) { return (ora_wvec){ a.x / b, a.y / b, a.z / b }; }
static inline ora_wvec ora_wvec_mul(ora_wvec b, int32_t a) { return (ora_wvec){ a * b.x, a * b.y, a * b.z }; }
static inline bool ora_wvec_eq(ora_wvec a, ora_wvec b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static inline int32_t ora_wvec_dot(ora_wvec a, ora_wvec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline int64_t ora_wvec_length_sq(ora_wvec a) { return (int64_t)a.x * a.x + (int64_t)a.y * a.y + (int64_t)a.z * a.z; }
static inline int32_t ora_wvec_length(ora_wvec a) { return (int32_t)ora_isqrt_u64((uint64_t)ora_wvec_length_sq(a), ORA_ISQRT_FLOOR); }
static inline int64_t ora_wvec_hlength_sq(ora_wvec a) { return (int64_t)a.x * a.x + (int64_t)a.y * a.y; }
static inline int32_t ora_wvec_hlength(ora_wvec a) { return (int32_t)ora_isqrt_u64((uint64_t)ora_wvec_hlength_sq(a), ORA_ISQRT_FLOOR); }
static inline int64_t ora_wvec_vlength_sq(ora_wvec a) { return (int64_t)a.z * a.z; }
static inline int32_t ora_wvec_vlength(ora_wvec a) { return (int32_t)ora_isqrt_u64((uint64_t)ora_wvec_vlength_sq(a), ORA_ISQRT_FLOOR); }

/* WVec.Yaw(WVec.cs:66-76):OpenRA 定义北为 -y */
static inline ora_wangle ora_wvec_yaw(ora_wvec a)
{
    if (ora_wvec_length_sq(a) == 0)
        return ora_wangle_mk(0);
    return ora_wangle_sub(ora_wangle_arctan(-a.y, a.x), ora_wangle_mk(256));
}

static inline ora_wpos ora_wpos_lerp(ora_wpos a, ora_wpos b, int32_t mul, int32_t div)
{
    return ora_wpos_add(a, ora_wvec_div(ora_wvec_mul(ora_wpos_diff(b, a), mul), div));
}

static inline ora_wpos ora_wpos_lerp64(ora_wpos a, ora_wpos b, int64_t mul, int64_t div)
{
    /* WPos.Lerp(long mul, long div)(WPos.cs:47-56):中间量超 int 精度 */
    return (ora_wpos){
        (int32_t)(a.x + ((int64_t)b.x - a.x) * mul / div),
        (int32_t)(a.y + ((int64_t)b.y - a.y) * mul / div),
        (int32_t)(a.z + ((int64_t)b.z - a.z) * mul / div) };
}

static inline ora_wvec ora_wvec_lerp(ora_wvec a, ora_wvec b, int32_t mul, int32_t div)
{
    return ora_wvec_add(a, ora_wvec_div(ora_wvec_mul(ora_wvec_sub(b, a), mul), div));
}

/*
 * WVec.LerpQuadratic(WVec.cs:80-92)。
 * C# 用 decimal(96 位十进制整数)防溢出;此处用 __int128 精确整数除法复刻:
 *   offset = (int)( (Length * Tan * mul * (div-mul)) / (1024*div*div) )   —— 除法向零截断
 * 与 decimal 的理论差异仅当商的小数展开连续 12+ 个 9 且触发 decimal 第 28 位
 * 进位时出现(概率意义上不可达,由 golden 对拍覆盖)。
 * 注意:分母 `1024*div*div` 在 C# 中是 int 域运算(可回绕),必须先回绕再入 128 位。
 */
static inline ora_wvec ora_wvec_lerp_quadratic(ora_wvec a, ora_wvec b, ora_wangle pitch, int32_t mul, int32_t div)
{
    ora_wvec ret = ora_wvec_lerp(a, b, mul, div);
    if (pitch.angle == 0)
        return ret;

    int32_t len = ora_wvec_length(ora_wvec_sub(b, a));
    int32_t tan = ora_wangle_tan(pitch);

    __int128 num = (__int128)len * tan * mul * (div - mul);
    __int128 den = (__int128)(1024 * div * div); /* int 域回绕后转 128 位,对齐 C# */
    int32_t offset = (int32_t)(num / den);

    return (ora_wvec){ ret.x, ret.y, ret.z + offset };
}

/*
 * WPos.LerpQuadratic(WPos.cs:58-72):与 WVec 版不同——offset 不先截断,
 * (offset + ret.Z) 在 decimal 域相加后统一向零截断,再 clamp 到 int 范围。
 * 注意向零截断在跨零时不满足平移不变(见函数内注释),不能写成 trunc(offset)+z。
 */
static inline ora_wpos ora_wpos_lerp_quadratic(ora_wpos a, ora_wpos b, ora_wangle pitch, int32_t mul, int32_t div)
{
    ora_wpos ret = ora_wpos_lerp(a, b, mul, div);
    if (pitch.angle == 0)
        return ret;

    int32_t len = ora_wvec_length(ora_wpos_diff(b, a));
    int32_t tan = ora_wangle_tan(pitch);

    __int128 num = (__int128)len * tan * mul * (div - mul);
    __int128 den = (__int128)(1024 * div * div);

    /* C# 是 (int)(offset + ret.Z):offset 与 z 在 decimal 域相加后【统一向零截断】。
       当 offset 与 (offset + z) 异号(跨零)时 trunc(x) + z != trunc(x + z),
       故必须通分后一次除法:trunc((num + z*den) / den)。
       实证:golden WPLQ i=0, offset=-20533.08, z=68260 → 47726(而非 47727)。 */
    __int128 q = (num + (__int128)ret.z * den) / den;
    int32_t clamped = q < INT32_MIN ? INT32_MIN : (q > INT32_MAX ? INT32_MAX : (int32_t)q);
    return (ora_wpos){ ret.x, ret.y, clamped };
}

/* ————————————————————————— Int32Matrix4x4 / WRot ————————————————————————— */

typedef struct ora_mat44 {
    int32_t m11, m12, m13, m14, m21, m22, m23, m24;
    int32_t m31, m32, m33, m34, m41, m42, m43, m44;
} ora_mat44;

typedef struct ora_wrot {
    ora_wangle roll, pitch, yaw; /* 欧拉角表示(公开) */
    int32_t x, y, z, w;          /* 四元数(内部计算用,1024 == 1.0) */
} ora_wrot;

static inline bool ora_wrot_eq(ora_wrot a, ora_wrot b)
{
    return ora_wangle_eq(a.roll, b.roll) && ora_wangle_eq(a.pitch, b.pitch) && ora_wangle_eq(a.yaw, b.yaw);
}

/* QuaternionToEuler(WRot.cs:79-95) */
static inline void ora_wrot_quat_to_euler(int32_t x, int32_t y, int32_t z, int32_t w,
    ora_wangle *roll, ora_wangle *pitch, ora_wangle *yaw)
{
    int32_t lsq = x * x + y * y + z * z + w * w; /* 理论 1024²,舍入可略有偏差 */

    int32_t srcp = 2 * (w * x + y * z);
    int32_t crcp = lsq - 2 * (x * x + y * y);
    int32_t sp = (w * y - z * x) / 512;
    int32_t sycp = 2 * (w * z + x * y);
    int32_t cycp = lsq - 2 * (y * y + z * z);

    ora_wangle r = ora_wangle_neg(ora_wangle_arctan(srcp, crcp));
    ora_wangle p = ora_wangle_neg((sp < 0 ? -sp : sp) >= 1024
        ? ora_wangle_mk(ora_int2_sign1(sp) * 256)
        : ora_wangle_arcsin(sp));
    ora_wangle yw = ora_wangle_neg(ora_wangle_arctan(sycp, cycp));

    *roll = r; *pitch = p; *yaw = yw;
}

/* 私有构造器 WRot(x, y, z, w)(WRot.cs:69-77) */
static inline ora_wrot ora_wrot_from_quat(int32_t x, int32_t y, int32_t z, int32_t w)
{
    ora_wrot r = { .x = x, .y = y, .z = z, .w = w };
    ora_wrot_quat_to_euler(x, y, z, w, &r.roll, &r.pitch, &r.yaw);
    return r;
}

/* WRot(roll, pitch, yaw)(WRot.cs:30-52):欧拉 → 归一化四元数 */
static inline ora_wrot ora_wrot_mk(ora_wangle roll, ora_wangle pitch, ora_wangle yaw)
{
    ora_wrot r = { .roll = roll, .pitch = pitch, .yaw = yaw };

    /* 角度顺时针增加 */
    ora_wangle qr = ora_wangle_mk(-roll.angle / 2);
    ora_wangle qp = ora_wangle_mk(-pitch.angle / 2);
    ora_wangle qy = ora_wangle_mk(-yaw.angle / 2);
    int64_t cr = ora_wangle_cos(qr), sr = ora_wangle_sin(qr);
    int64_t cp = ora_wangle_cos(qp), sp = ora_wangle_sin(qp);
    int64_t cy = ora_wangle_cos(qy), sy = ora_wangle_sin(qy);

    r.x = (int32_t)((sr * cp * cy - cr * sp * sy) / 1048576);
    r.y = (int32_t)((cr * sp * cy + sr * cp * sy) / 1048576);
    r.z = (int32_t)((cr * cp * sy - sr * sp * cy) / 1048576);
    r.w = (int32_t)((cr * cp * cy + sr * sp * sy) / 1048576);
    return r;
}

/* WRot(axis, angle)(WRot.cs:58-67):轴须归一化到长度 1024 */
static inline ora_wrot ora_wrot_axis_angle(ora_wvec axis, ora_wangle angle)
{
    ora_wangle half = ora_wangle_mk(-angle.angle / 2);
    int32_t x = axis.x * ora_wangle_sin(half) / 1024;
    int32_t y = axis.y * ora_wangle_sin(half) / 1024;
    int32_t z = axis.z * ora_wangle_sin(half) / 1024;
    int32_t w = ora_wangle_cos(half);
    return ora_wrot_from_quat(x, y, z, w);
}

static inline ora_wrot ora_wrot_none(void) { return ora_wrot_mk(ora_wangle_mk(0), ora_wangle_mk(0), ora_wangle_mk(0)); }
static inline ora_wrot ora_wrot_from_facing(int32_t facing) { return ora_wrot_mk(ora_wangle_mk(0), ora_wangle_mk(0), ora_wangle_from_facing(facing)); }
static inline ora_wrot ora_wrot_from_yaw(ora_wangle yaw) { return ora_wrot_mk(ora_wangle_mk(0), ora_wangle_mk(0), yaw); }

static inline ora_wrot ora_wrot_add(ora_wrot a, ora_wrot b)
{
    return ora_wrot_mk(ora_wangle_add(a.roll, b.roll), ora_wangle_add(a.pitch, b.pitch), ora_wangle_add(a.yaw, b.yaw));
}

static inline ora_wrot ora_wrot_sub(ora_wrot a, ora_wrot b)
{
    return ora_wrot_mk(ora_wangle_sub(a.roll, b.roll), ora_wangle_sub(a.pitch, b.pitch), ora_wangle_sub(a.yaw, b.yaw));
}

/* 一元负(WRot.cs:114):直接翻转四元数与欧拉,不重新推导 */
static inline ora_wrot ora_wrot_neg(ora_wrot a)
{
    return (ora_wrot){
        .roll = ora_wangle_neg(a.roll), .pitch = ora_wangle_neg(a.pitch), .yaw = ora_wangle_neg(a.yaw),
        .x = -a.x, .y = -a.y, .z = -a.z, .w = a.w };
}

/* WRot.Rotate(WRot.cs:116-130):四元数乘法 */
static inline ora_wrot ora_wrot_rotate(ora_wrot self, ora_wrot rot)
{
    if (ora_wrot_eq(self, ora_wrot_none()))
        return rot;
    if (ora_wrot_eq(rot, ora_wrot_none()))
        return self;

    int64_t rx = ((int64_t)rot.w * self.x + (int64_t)rot.x * self.w + (int64_t)rot.y * self.z - (int64_t)rot.z * self.y) / 1024;
    int64_t ry = ((int64_t)rot.w * self.y - (int64_t)rot.x * self.z + (int64_t)rot.y * self.w + (int64_t)rot.z * self.x) / 1024;
    int64_t rz = ((int64_t)rot.w * self.z + (int64_t)rot.x * self.y - (int64_t)rot.y * self.x + (int64_t)rot.z * self.w) / 1024;
    int64_t rw = ((int64_t)rot.w * self.w - (int64_t)rot.x * self.x - (int64_t)rot.y * self.y - (int64_t)rot.z * self.z) / 1024;

    return ora_wrot_from_quat((int32_t)rx, (int32_t)ry, (int32_t)rz, (int32_t)rw);
}

/* WRot.SLerp(WRot.cs:195-220):整数球面线性插值 */
static inline ora_wrot ora_wrot_slerp(ora_wrot a, ora_wrot b, int32_t mul, int32_t div)
{
    int32_t dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    int32_t flip = dot >= 0 ? 1 : -1;

    if (flip * dot >= 1024 * 1024) /* 同一旋转 */
        return a;

    ora_wangle theta = ora_wangle_arccos(dot / 1024);
    int32_t s1 = ora_wangle_sin(ora_wangle_mk((div - mul) * theta.angle / div));
    int32_t s2 = ora_wangle_sin(ora_wangle_mk(mul * theta.angle / div));
    int32_t s3 = ora_wangle_sin(theta);

    int64_t x = ((int64_t)a.x * s1 + (int64_t)(flip * b.x * s2)) / s3;
    int64_t y = ((int64_t)a.y * s1 + (int64_t)(flip * b.y * s2)) / s3;
    int64_t z = ((int64_t)a.z * s1 + (int64_t)(flip * b.z * s2)) / s3;
    int64_t w = ((int64_t)a.w * s1 + (int64_t)(flip * b.w * s2)) / s3;

    int64_t l = ora_isqrt_i64(x * x + y * y + z * z + w * w, ORA_ISQRT_FLOOR);
    return ora_wrot_from_quat(
        (int32_t)(1024 * x / l), (int32_t)(1024 * y / l),
        (int32_t)(1024 * z / l), (int32_t)(1024 * w / l));
}

/* WRot.AsMatrix(WRot.cs:154-180) */
static inline ora_mat44 ora_wrot_as_matrix(ora_wrot r)
{
    int32_t x = r.x, y = r.y, z = r.z, w = r.w;
    int32_t lsq = x * x + y * y + z * z + w * w;

    return (ora_mat44){
        lsq - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
        2 * (x * y - z * w), lsq - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
        2 * (x * z + y * w), 2 * (y * z - x * w), lsq - 2 * (x * x + y * y), 0,
        0, 0, 0, lsq };
}

/* WVec.Rotate(ref Int32Matrix4x4)(WVec.cs:55-64):long 中间量,除以 M44 */
static inline ora_wvec ora_wvec_rotate(ora_wvec v, ora_mat44 mtx)
{
    int64_t lx = v.x, ly = v.y, lz = v.z;
    return (ora_wvec){
        (int32_t)((lx * mtx.m11 + ly * mtx.m21 + lz * mtx.m31) / mtx.m44),
        (int32_t)((lx * mtx.m12 + ly * mtx.m22 + lz * mtx.m32) / mtx.m44),
        (int32_t)((lx * mtx.m13 + ly * mtx.m23 + lz * mtx.m33) / mtx.m44) };
}

#endif /* ORA_PRIMITIVES_H */
