/* golden_core — Phase 0 定点原语对拍测试
 *
 * 用与 tools/golden_gen/Program.cs 严格一致的输入序列(LCG)驱动 C 实现,
 * 逐行重放生成文本并与 C# 黄金文件比对;任何一行不一致即失败。
 * 这是 PORTING_PLAN.md §Phase 0 的验收关卡:定点原语与 C# 对拍一致。
 *
 * 用法: golden_core <golden_core.txt>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ora_primitives.h"
#include "ora_random.h"

static char buf[1024];

/* 与 C# 侧一致的独立 LCG 输入流 */
static uint64_t lcg = 0x9E3779B97F4A7C15ULL;
static int32_t Rand(void)
{
    lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int32_t)(uint32_t)(lcg >> 33);
}

static const char *golden_data; /* 整个文件 */
static size_t golden_pos, golden_line;

static bool next_golden_line(const char **line, size_t *len)
{
    if (golden_data[golden_pos] == '\0')
        return false;

    const char *start = golden_data + golden_pos;
    const char *nl = strchr(start, '\n');
    size_t l = nl ? (size_t)(nl - start) : strlen(start);
    if (l > 0 && start[l - 1] == '\r')
        l--;
    golden_pos += (nl ? (size_t)(nl - start) : l) + (nl ? 1 : 0);
    golden_line++;
    *line = start;
    *len = l;
    return true;
}

static int failures = 0;
static size_t total = 0;

static void Check(const char *s)
{
    const char *gl;
    size_t glen;
    total++;
    if (!next_golden_line(&gl, &glen))
    {
        printf("FAIL line %llu: golden 已耗尽,C 多输出: %s\n", golden_line, s);
        failures++;
        return;
    }
    if (strlen(s) != glen || strncmp(s, gl, glen) != 0)
    {
        printf("FAIL line %zu:\n  golden: %.*s\n  c     : %s\n", golden_line, (int)glen, gl, s);
        if (++failures > 10)
        {
            printf("…差异过多,中止\n");
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "用法: golden_core <golden_core.txt>\n");
        return 2;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f)
    {
        fprintf(stderr, "无法打开 golden 文件: %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    golden_data = malloc((size_t)size + 1);
    if (fread((void *)golden_data, 1, (size_t)size, f) != (size_t)size)
        return 2;
    ((char *)golden_data)[size] = '\0';
    fclose(f);

    /* 1) ISqrt:0..4096 全量 + 2^k 边界 */
    for (uint32_t n = 0; n <= 4096; n++)
    {
        snprintf(buf, sizeof buf, "ISQ n=%u v=%u", n, ora_isqrt_u32(n, ORA_ISQRT_FLOOR));
        Check(buf);
    }
    for (int k = 0; k <= 32; k++)
    {
        uint64_t p = k == 32 ? UINT64_MAX : (1ULL << k);
        snprintf(buf, sizeof buf, "ISQ64 n=%llu v=%llu", (unsigned long long)(p - 1), (unsigned long long)ora_isqrt_u64(p - 1, ORA_ISQRT_FLOOR));
        Check(buf);
        snprintf(buf, sizeof buf, "ISQ64 n=%llu v=%llu", (unsigned long long)p, (unsigned long long)ora_isqrt_u64(p, ORA_ISQRT_FLOOR));
        Check(buf);
        snprintf(buf, sizeof buf, "ISQ64 n=%llu v=%llu", (unsigned long long)(p + 1), (unsigned long long)ora_isqrt_u64(p + 1, ORA_ISQRT_FLOOR));
        Check(buf);
    }

    /* 2) WAngle 全量 */
    for (int a = 0; a < 1024; a++)
    {
        ora_wangle wa = ora_wangle_mk(a);
        snprintf(buf, sizeof buf, "WA a=%d sin=%d cos=%d tan=%d facing=%d",
            a, ora_wangle_sin(wa), ora_wangle_cos(wa), ora_wangle_tan(wa), ora_wangle_facing(wa));
        Check(buf);
    }

    /* 3) FromFacing / FromDegrees */
    for (int f2 = -8; f2 <= 8; f2++)
    {
        snprintf(buf, sizeof buf, "WAF f=%d a=%d", f2, ora_wangle_from_facing(f2).angle);
        Check(buf);
    }
    for (int d2 = -720; d2 <= 720; d2 += 7)
    {
        snprintf(buf, sizeof buf, "WAD d=%d a=%d", d2, ora_wangle_from_degrees(d2).angle);
        Check(buf);
    }

    /* 4) ArcSin / ArcCos 全量 */
    for (int d = -1024; d <= 1024; d++)
    {
        snprintf(buf, sizeof buf, "ASIN d=%d a=%d", d, ora_wangle_arcsin(d).angle);
        Check(buf);
        snprintf(buf, sizeof buf, "ACOS d=%d a=%d", d, ora_wangle_arccos(d).angle);
        Check(buf);
    }

    /* 5) ArcTan 网格 */
    for (int y = -613; y <= 613; y += 61)
        for (int x = -613; x <= 613; x += 61)
        {
            snprintf(buf, sizeof buf, "ATAN y=%d x=%d a=%d", y, x, ora_wangle_arctan(y, x).angle);
            Check(buf);
        }
    for (int y = -2000000; y <= 2000000; y += 1000000)
        for (int x = -2000000; x <= 2000000; x += 1000000)
        {
            snprintf(buf, sizeof buf, "ATAN y=%d x=%d a=%d", y, x, ora_wangle_arctan(y, x).angle);
            Check(buf);
        }

    /* 6) WAngle.Lerp */
    {
        static const int muls[] = { 0, 1, 5, 13, 999, 1024 };
        static const int divs[] = { 1, 7, 1000, 1024 };
        for (int a = 0; a < 1024; a += 97)
            for (int b = 0; b < 1024; b += 89)
                for (size_t mi = 0; mi < sizeof muls / sizeof muls[0]; mi++)
                    for (size_t di = 0; di < sizeof divs / sizeof divs[0]; di++)
                    {
                        snprintf(buf, sizeof buf, "WAL a=%d b=%d m=%d d=%d r=%d", a, b, muls[mi], divs[di],
                            ora_wangle_lerp(ora_wangle_mk(a), ora_wangle_mk(b), muls[mi], divs[di]).angle);
                        Check(buf);
                    }
    }

    /* 7) MersenneTwister */
    {
        static const int seeds[] = { 0, 1, 20250929 };
        for (size_t si = 0; si < 3; si++)
        {
            int s = seeds[si];
            ora_mt mt;
            ora_mt_init(&mt, s);
            for (int i = 0; i < 3000; i++)
            {
                snprintf(buf, sizeof buf, "MTU s=%d i=%d v=%u", s, i, ora_mt_next_uint(&mt));
                Check(buf);
            }
            for (int i = 0; i < 1000; i++)
            {
                snprintf(buf, sizeof buf, "MTN s=%d i=%d v=%d", s, i, ora_mt_next(&mt));
                Check(buf);
            }
            for (int i = 0; i < 200; i++)
            {
                snprintf(buf, sizeof buf, "MTUL s=%d i=%d v=%llu", s, i, (unsigned long long)ora_mt_next_ulong(&mt));
                Check(buf);
            }
            for (int i = 0; i < 500; i++)
            {
                int lo = (Rand() % 2001) - 1000;
                int hi = lo + (int)(Rand() % 3000);
                snprintf(buf, sizeof buf, "MTR s=%d i=%d lo=%d hi=%d v=%d", s, i, lo, hi, ora_mt_next_range(&mt, lo, hi));
                Check(buf);
            }
            snprintf(buf, sizeof buf, "MTSTATE s=%d last=%d total=%d", s, mt.last, mt.total_count);
            Check(buf);
        }

        for (size_t si = 0; si < 3; si++)
        {
            static const int w5[] = { 1, 2, 3, 0, 7 };
            static const int w3[] = { 0, 0, 0 };
            static const int w1[] = { 5 };
            static const int w10[] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
            static const int *sets[] = { w5, w3, w1, w10 };
            static const size_t n[] = { 5, 3, 1, 10 };

            int s = seeds[si];
            ora_mt mt;
            ora_mt_init(&mt, s + 1000);
            for (int i = 0; i < 500; i++)
            {
                const int *w = sets[i % 4];
                snprintf(buf, sizeof buf, "MTPW s=%d i=%d v=%d", s, i, ora_mt_pick_weighted(&mt, w, n[i % 4]));
                Check(buf);
            }
        }
    }

    /* 8) FromPDF */
    {
        ora_mt mt;
        ora_mt_init(&mt, 7);
        static const int samplesList[] = { 1, 2, 3, 5, 10 };
        for (int i = 0; i < 20; i++)
            for (size_t j = 0; j < 5; j++)
            {
                int sum = 0;
                for (int k = 0; k < samplesList[j]; k++)
                    sum += ora_mt_next_range(&mt, -1024, 1024);
                snprintf(buf, sizeof buf, "PDF i=%d n=%d v=%d", i, samplesList[j], sum / samplesList[j]);
                Check(buf);
            }

        ora_mt_init(&mt, 77);
        for (int i = 0; i < 20; i++)
        {
            int32_t px = 0, py = 0;
            for (int k = 0; k < 3; k++)
                px += ora_mt_next_range(&mt, -1024, 1024);
            px /= 3;
            for (int k = 0; k < 3; k++)
                py += ora_mt_next_range(&mt, -1024, 1024);
            py /= 3;
            snprintf(buf, sizeof buf, "PDFV i=%d x=%d y=%d z=0", i, px, py);
            Check(buf);
        }
    }

    /* 9) WVec/WPos 随机运算 */
    for (int i = 0; i < 4000; i++)
    {
        int ax = (Rand() % 200001) - 100000;
        int ay = (Rand() % 200001) - 100000;
        int az = (Rand() % 200001) - 100000;
        int bx = (Rand() % 200001) - 100000;
        int by = (Rand() % 200001) - 100000;
        int bz = (Rand() % 200001) - 100000;
        int s = (Rand() % 201) - 100;
        int d = (int)(Rand() % 100) + 1;
        int m = (int32_t)((uint32_t)Rand() % (uint32_t)d);
        int p = (int)(Rand() % 1024);
        if ((p & 511) >= 254 && (p & 511) <= 258)
            p = (p + 8) & 1023;

        ora_wvec a = ora_wvec_mk(ax, ay, az);
        ora_wvec b = ora_wvec_mk(bx, by, bz);
        ora_wvec add = ora_wvec_add(a, b);
        ora_wvec sub = ora_wvec_sub(a, b);
        ora_wvec neg = ora_wvec_neg(a);
        ora_wvec div = ora_wvec_div(a, d);
        ora_wvec mul = ora_wvec_mul(a, s);
        snprintf(buf, sizeof buf,
            "WV i=%d addx=%d addy=%d addz=%d subx=%d suby=%d subz=%d negx=%d negy=%d negz=%d divx=%d divy=%d divz=%d mulx=%d muly=%d mulz=%d",
            i, add.x, add.y, add.z, sub.x, sub.y, sub.z, neg.x, neg.y, neg.z, div.x, div.y, div.z, mul.x, mul.y, mul.z);
        Check(buf);
        snprintf(buf, sizeof buf,
            "WVL i=%d dot=%d lsq=%lld len=%d hlsq=%lld hlen=%d vlsq=%lld vlen=%d yaw=%d",
            i, ora_wvec_dot(a, b), (long long)ora_wvec_length_sq(a), ora_wvec_length(a),
            (long long)ora_wvec_hlength_sq(a), ora_wvec_hlength(a),
            (long long)ora_wvec_vlength_sq(a), ora_wvec_vlength(a), ora_wvec_yaw(a).angle);
        Check(buf);

        ora_wvec ler = ora_wvec_lerp(a, b, m, d);
        ora_wvec lq = ora_wvec_lerp_quadratic(a, b, ora_wangle_mk(p), m, d);
        snprintf(buf, sizeof buf, "WVLQ i=%d lx=%d ly=%d lz=%d qx=%d qy=%d qz=%d",
            i, ler.x, ler.y, ler.z, lq.x, lq.y, lq.z);
        Check(buf);

        ora_wpos pa = ora_wpos_mk(ax, ay, az);
        ora_wpos pb = ora_wpos_mk(bx, by, bz);
        ora_wpos pler = ora_wpos_lerp(pa, pb, m, d);
        ora_wpos plq = ora_wpos_lerp_quadratic(pa, pb, ora_wangle_mk(p), m, d);
        snprintf(buf, sizeof buf, "WPLQ i=%d lx=%d ly=%d lz=%d qx=%d qy=%d qz=%d",
            i, pler.x, pler.y, pler.z, plq.x, plq.y, plq.z);
        Check(buf);

        ora_wrot rot1 = ora_wrot_mk(ora_wangle_mk(p / 3 * 3), ora_wangle_mk((p + 128) / 3 * 3), ora_wangle_mk((p + 256) / 3 * 3));
        ora_wrot rot2 = ora_wrot_mk(ora_wangle_mk(p), ora_wangle_mk(p), ora_wangle_mk(p));
        ora_wvec r1 = ora_wvec_rotate(a, ora_wrot_as_matrix(rot1));
        ora_wvec r2 = ora_wvec_rotate(a, ora_wrot_as_matrix(rot2));
        snprintf(buf, sizeof buf, "WVR i=%d rx=%d ry=%d rz=%d", i, r1.x, r2.y, r2.z);
        Check(buf);
    }

    /* 10) WRot 全网格 */
    for (int r = 0; r < 1024; r += 64)
        for (int p = 0; p < 1024; p += 64)
            for (int y = 0; y < 1024; y += 64)
            {
                ora_wrot rot = ora_wrot_mk(ora_wangle_mk(r), ora_wangle_mk(p), ora_wangle_mk(y));
                ora_mat44 m = ora_wrot_as_matrix(rot);
                snprintf(buf, sizeof buf,
                    "WR r=%d p=%d y=%d q=%d,%d,%d,%d m=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    r, p, y, rot.x, rot.y, rot.z, rot.w,
                    m.m11, m.m12, m.m13, m.m21, m.m22, m.m23, m.m31, m.m32, m.m33, m.m44);
                Check(buf);
            }

    for (int r = 0; r < 1024; r += 97)
        for (int p = 0; p < 1024; p += 131)
        {
            ora_wrot a = ora_wrot_mk(ora_wangle_mk(r), ora_wangle_mk(p), ora_wangle_mk(300));
            ora_wrot b = ora_wrot_mk(ora_wangle_mk(700), ora_wangle_mk((p + 100) % 1024), ora_wangle_mk((r + 50) % 1024));
            ora_wrot rr = ora_wrot_rotate(a, b);
            ora_wrot neg = ora_wrot_neg(a);
            snprintf(buf, sizeof buf, "WRR r=%d p=%d q=%d,%d,%d,%d e=%d,%d,%d nq=%d,%d,%d,%d ne=%d,%d,%d",
                r, p, rr.x, rr.y, rr.z, rr.w, rr.roll.angle, rr.pitch.angle, rr.yaw.angle,
                neg.x, neg.y, neg.z, neg.w, neg.roll.angle, neg.pitch.angle, neg.yaw.angle);
            Check(buf);
        }

    for (int i = 0; i < 2000; i++)
    {
        int r = Rand() % 1024, p = Rand() % 1024, y = Rand() % 1024;
        int r2 = Rand() % 1024, p2 = Rand() % 1024, y2 = Rand() % 1024;
        int m = Rand() % 100, d = Rand() % 100 + 1;
        ora_wrot ra = ora_wrot_mk(ora_wangle_mk(r), ora_wangle_mk(p), ora_wangle_mk(y));
        ora_wrot rb = ora_wrot_mk(ora_wangle_mk(r2), ora_wangle_mk(p2), ora_wangle_mk(y2));
        ora_wrot srot = ora_wrot_slerp(ra, rb, m, d);
        snprintf(buf, sizeof buf, "WRS i=%d q=%d,%d,%d,%d e=%d,%d,%d",
            i, srot.x, srot.y, srot.z, srot.w, srot.roll.angle, srot.pitch.angle, srot.yaw.angle);
        Check(buf);
    }

    /* 11) CPos/MPos 网格 */
    {
        static const uint8_t layers[] = { 0, 1, 255 };
        for (int x = -2048; x <= 2047; x += 131)
            for (int yy = -2048; yy <= 2047; yy += 131)
                for (size_t li = 0; li < 3; li++)
                {
                    ora_cpos c = ora_cpos_mk(x, yy, layers[li]);
                    snprintf(buf, sizeof buf, "CP x=%d y=%d l=%u bits=%d rx=%d ry=%d rl=%u",
                        x, yy, layers[li], c.bits, ora_cpos_x(c), ora_cpos_y(c), ora_cpos_layer(c));
                    Check(buf);
                }

        for (int x = -40; x <= 40; x += 3)
            for (int yy = -40; yy <= 40; yy += 3)
            {
                ora_cpos c = ora_cpos_mk(x, yy, 0);
                ora_mpos mr = ora_cpos_to_mpos(c, ORA_GRID_RECTANGULAR);
                ora_mpos mi = ora_cpos_to_mpos(c, ORA_GRID_RECTANGULAR_ISOMETRIC);
                ora_cpos br = ora_mpos_to_cpos(mr, ORA_GRID_RECTANGULAR);
                ora_cpos bi = ora_mpos_to_cpos(mi, ORA_GRID_RECTANGULAR_ISOMETRIC);
                snprintf(buf, sizeof buf, "CPM x=%d y=%d ru=%d rv=%d iu=%d iv=%d bx=%d by=%d rx=%d ry=%d",
                    x, yy, mr.u, mr.v, mi.u, mi.v, ora_cpos_x(bi), ora_cpos_y(bi), ora_cpos_x(br), ora_cpos_y(br));
                Check(buf);
            }
    }

    /* 12) CVec 采样 */
    for (int i = 0; i < 2000; i++)
    {
        int ax = (Rand() % 4001) - 2000;
        int ay = (Rand() % 4001) - 2000;
        int bx = (Rand() % 4001) - 2000;
        int by = (Rand() % 4001) - 2000;
        int s = (int)(Rand() % 50) + 1;
        ora_cvec a = ora_cvec_mk(ax, ay);
        ora_cvec b = ora_cvec_mk(bx, by);
        ora_cvec add = ora_cvec_add(a, b);
        ora_cvec sub = ora_cvec_sub(a, b);
        ora_cvec mul = ora_cvec_mul(a, s);
        ora_cvec div = ora_cvec_div(a, s);
        ora_cvec mx = ora_cvec_max(a, b);
        ora_cvec mn = ora_cvec_min(a, b);
        snprintf(buf, sizeof buf,
            "CV i=%d ax=%d ay=%d bx=%d by=%d s=%d addx=%d addy=%d subx=%d suby=%d mulx=%d muly=%d divx=%d divy=%d maxx=%d maxy=%d minx=%d miny=%d dot=%d lsq=%d len=%d sgx=%d sgy=%d absx=%d absy=%d",
            i, ax, ay, bx, by, s, add.x, add.y, sub.x, sub.y, mul.x, mul.y, div.x, div.y,
            mx.x, mx.y, mn.x, mn.y, ora_cvec_dot(a, b), ora_cvec_length_sq(a), ora_cvec_length(a),
            ora_cvec_sign(a).x, ora_cvec_sign(a).y, ora_cvec_abs(a).x, ora_cvec_abs(a).y);
        Check(buf);
    }

    /* golden 是否有剩余行 */
    const char *gl;
    size_t glen;
    if (next_golden_line(&gl, &glen) && glen > 0)
    {
        printf("FAIL: C 输出已结束但 golden 还有剩余(行 %zu 起): %.*s\n", golden_line, (int)glen, gl);
        failures++;
    }

    if (failures == 0)
    {
        printf("golden_core: PASS  (%zu 行全部一致)\n", total);
        return 0;
    }
    printf("golden_core: FAIL  (%d 处差异 / %zu 行)\n", failures, total);
    return 1;
}
