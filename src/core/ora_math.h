/* ORA_MATH — 整数平方根与整数近似(ISqrt 等)
 * 对应 C#: OpenRA.Game/Exts.cs:282-373
 * 所有除法向零截断,与 C# 一致;无任何浮点。
 */
#ifndef ORA_MATH_H
#define ORA_MATH_H

#include <stdint.h>
#include <assert.h>

typedef enum ora_isqrt_round {
    ORA_ISQRT_FLOOR = 0,
    ORA_ISQRT_NEAREST,
    ORA_ISQRT_CEILING
} ora_isqrt_round;

static inline uint32_t ora_isqrt_u32(uint32_t number, ora_isqrt_round round)
{
    uint32_t divisor = 1u << 30;
    uint32_t root = 0, remainder = number;

    while (divisor > number)
        divisor >>= 2;

    while (divisor != 0)
    {
        if (root + divisor <= remainder)
        {
            remainder -= root + divisor;
            root += 2 * divisor;
        }

        root >>= 1;
        divisor >>= 2;
    }

    if (round == ORA_ISQRT_NEAREST && remainder > root)
        root++;
    else if (round == ORA_ISQRT_CEILING && root * root < number)
        root++;

    return root;
}

static inline uint64_t ora_isqrt_u64(uint64_t number, ora_isqrt_round round)
{
    uint64_t divisor = 1ull << 62;
    uint64_t root = 0, remainder = number;

    while (divisor > number)
        divisor >>= 2;

    while (divisor != 0)
    {
        if (root + divisor <= remainder)
        {
            remainder -= root + divisor;
            root += 2 * divisor;
        }

        root >>= 1;
        divisor >>= 2;
    }

    if (round == ORA_ISQRT_NEAREST && remainder > root)
        root++;
    else if (round == ORA_ISQRT_CEILING && root * root < number)
        root++;

    return root;
}

/* Exts.ISqrt(int):负数输入在 C# 抛异常,这里断言 */
static inline int32_t ora_isqrt_i32(int32_t number, ora_isqrt_round round)
{
    assert(number >= 0);
    return (int32_t)ora_isqrt_u32((uint32_t)number, round);
}

static inline int64_t ora_isqrt_i64(int64_t number, ora_isqrt_round round)
{
    assert(number >= 0);
    return (int64_t)ora_isqrt_u64((uint64_t)number, round);
}

/* Exts.MultiplyBySqrtTwo / MultiplyBySqrtTwoOverTwo(Exts.cs:365-373) */
static inline int32_t ora_mul_sqrt2(int16_t number) { return (int32_t)number * 46341 / 32768; }
static inline int32_t ora_mul_sqrt2_over_2(int32_t number) { return (int32_t)((int64_t)number * 23170 / 32768); }

#endif /* ORA_MATH_H */
