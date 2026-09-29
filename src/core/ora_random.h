/* ORA_RANDOM — MersenneTwister(MT19937,OpenRA 变体)
 * 对应 C#: OpenRA.Game/Support/MersenneTwister.cs
 * 该实现是仿真确定性的根基:任何数值/顺序变化都会导致 desync。
 */
#ifndef ORA_RANDOM_H
#define ORA_RANDOM_H

#include <stdint.h>
#include <assert.h>
#include <stddef.h>

typedef struct ora_mt {
    uint32_t mt[624];
    int32_t index;
    int32_t last;        /* 上一次 Next() 的值(参与 SyncHash) */
    int32_t total_count; /* 累计产生 uint 次数 */
} ora_mt;

/* MersenneTwister(int seed)(MersenneTwister.cs:28-33) */
static inline void ora_mt_init(ora_mt *r, int32_t seed)
{
    r->mt[0] = (uint32_t)seed;
    for (uint32_t i = 1; i < 624; i++)
        r->mt[i] = 1812433253u * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + i;
    r->index = 0;
    r->last = 0;
    r->total_count = 0;
}

/* Generate()(MersenneTwister.cs:140-152):twist */
static inline void ora_mt_generate(ora_mt *r)
{
    for (uint32_t i = 0; i < 624; i++)
    {
        uint32_t y = (r->mt[i] & 0x80000000u) | (r->mt[(i + 1) % 624] & 0x7fffffffu);
        r->mt[i] = r->mt[(i + 397u) % 624u] ^ (y >> 1);
        if ((y & 1) == 1)
            r->mt[i] ^= 2567483615u;
    }
}

/* NextUint()(MersenneTwister.cs:38-52) */
static inline uint32_t ora_mt_next_uint(ora_mt *r)
{
    if (r->index == 0)
        ora_mt_generate(r);

    uint32_t y = r->mt[r->index];
    y ^= y >> 11;
    y ^= (y << 7) & 2636928640u;
    y ^= (y << 15) & 4022730752u;
    y ^= y >> 18;

    r->index = (r->index + 1) % 624;
    r->total_count++;
    /* C# Last = (int)(y % int.MaxValue):uint/int 提升为 long 后取模,值非负 */
    r->last = (int32_t)((uint64_t)y % 2147483647u);
    return y;
}

/* NextUlong()(MersenneTwister.cs:57-60) */
static inline uint64_t ora_mt_next_ulong(ora_mt *r)
{
    return (uint64_t)ora_mt_next_uint(r) << 32 | ora_mt_next_uint(r);
}

/* Next()(MersenneTwister.cs:66-70):-0x7fffffff..0x7fffffff,0 概率加倍 */
static inline int32_t ora_mt_next(ora_mt *r)
{
    ora_mt_next_uint(r);
    return r->last;
}

/* Next(low, high)(MersenneTwister.cs:72-82):含 low 不含 high */
static inline int32_t ora_mt_next_range(ora_mt *r, int32_t low, int32_t high)
{
    assert(high >= low);
    int32_t diff = high - low;
    if (diff <= 1)
        return low;
    return low + ora_mt_next(r) % diff;
}

/* PickWeighted(MersenneTwister.cs:101-125):按权重选下标 */
static inline int32_t ora_mt_pick_weighted(ora_mt *r, const int32_t *weights, size_t n)
{
    uint64_t total = 0;
    for (size_t i = 0; i < n; i++)
    {
        assert(weights[i] >= 0);
        total += (uint64_t)weights[i];
    }

    if (total == 0)
        return ora_mt_next_range(r, 0, (int32_t)n);

    uint64_t spin = ora_mt_next_ulong(r) % total;
    uint64_t acc = 0;
    for (size_t i = 0; i < n; i++)
    {
        acc += (uint64_t)weights[i];
        if (spin < acc)
            return (int32_t)i;
    }

    assert(0 && "unreachable");
    return 0;
}

#endif /* ORA_RANDOM_H */
