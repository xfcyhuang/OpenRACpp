// golden_gen — 黄金对拍数据生成器(独立 oracle)
//
// 内含从 OpenRA 源码【逐字复制】的原语实现(仅剥离 Lua/Scripting 接口与无关 using),
// 输出与 C 侧 tests/golden_core.c 完全相同的行序列,逐行比对即为移植验收
// (PORTING_PLAN.md §Phase 0:定点原语与 C# 对拍)。
//
// 用法:
//   dotnet run [-- <输出路径>]                        → 定点原语黄金(golden_core.txt)
//   dotnet run -- yaml <输出路径> <OpenRA 仓库根>     → MiniYaml 黄金(golden_yaml.txt,
//     PORTING_PLAN.md §Phase 1:mods/**/*.yaml 全量解析 + WriteToString 规范化输出)
//
// MiniYaml 黄金帧格式(\x01 为帧控制符,yaml 内容不会出现):
//   \x01F <相对路径>     文件条目开始
//   \x01M keep|discard   模式块(discardCommentsAndWhitespace)
//   <规范化输出行...>    WriteToString 输出,或
//   \x01E <异常消息>     解析抛错时
//   \x01X                文件条目结束

using System.Buffers;
using System.Globalization;
using System.Text;

if (args.Length > 0 && args[0] == "yaml")
{
    var outPathY = args.Length > 1 ? args[1] : "golden_yaml.txt";
    var repoRoot = args.Length > 2 ? args[2] : ".";
    var modsRoot = Path.Combine(repoRoot, "mods");

    // 与 C++ 侧一致的枚举序:相对路径(以 / 分隔)按 ordinal 排序
    var files = Directory.EnumerateFiles(modsRoot, "*.yaml", SearchOption.AllDirectories)
        .Where(p => p.EndsWith(".yaml", StringComparison.Ordinal))
        .Select(p => Path.GetRelativePath(repoRoot, p).Replace('\\', '/'))
    .OrderBy(p => p, StringComparer.Ordinal)
        .ToList();

    var sbY = new StringBuilder();
    foreach (var rel in files)
    {
        sbY.Append("\u0001F ").Append(rel).Append('\n');
        foreach (var keepComments in new[] { false, true })
        {
            sbY.Append(keepComments ? "\u0001M keep\n" : "\u0001M discard\n");
            try
            {
                // name(位置标注)统一用相对路径,与 C++ 侧一致
                using var s = File.OpenRead(Path.Combine(repoRoot, rel));
                var nodes = OpenRA.MiniYaml.FromStream(s, rel, discardCommentsAndWhitespace: keepComments);
                var output = OpenRA.MiniYamlExts.WriteToString(nodes);
                if (output.Contains('\u0001'))
                    throw new InvalidOperationException($"yaml 输出含帧控制符: {rel}");
                sbY.Append(output);
            }
            catch (Exception ex)
            {
                sbY.Append("\u0001E ").Append(ex.GetType().Name).Append(": ").Append(ex.Message).Append('\n');
            }
        }

        sbY.Append("\u0001X\n");
    }

    File.WriteAllText(outPathY, sbY.ToString());
    Console.WriteLine($"golden yaml written: {outPathY} ({files.Count} files)");
    return;
}


// ————————————————————————————————————————————————————————————————
// 以下为 OpenRA 原语的逐字副本(运算逻辑不做任何改动)
// ————————————————————————————————————————————————————————————————


// ————————————————————————————————————————————————————————————————
// golden 输出:与 tests/golden_core.c 的生成序列【严格一致】
// ————————————————————————————————————————————————————————————————

var sb = new StringBuilder();
void L(string s) => sb.AppendLine(s);

// 独立 LCG 输入流(两侧一致;只用于生成测试输入,与 MT 无关)
ulong lcg = 0x9E3779B97F4A7C15UL;
int Rand()
{
    lcg = lcg * 6364136223846793005UL + 1442695040888963407UL;
    return (int)(lcg >> 33); // [0, 2^31)
}

// 1) ISqrt:0..4096 全量 + 2^k 边界
for (uint n = 0; n <= 4096; n++)
    L($"ISQ n={n} v={OpenRA.Exts.ISqrt(n)}");
for (int k = 0; k <= 32; k++)
{
    ulong p = k == 32 ? ulong.MaxValue : (1UL << k);
    L($"ISQ64 n={p - 1} v={OpenRA.Exts.ISqrt(p - 1)}");
    L($"ISQ64 n={p} v={OpenRA.Exts.ISqrt(p)}");
    L($"ISQ64 n={p + 1} v={OpenRA.Exts.ISqrt(p + 1)}");
}

// 2) WAngle 全量 sin/cos/tan
for (int a = 0; a < 1024; a++)
{
    var wa = new OpenRA.WAngle(a);
    L($"WA a={a} sin={wa.Sin()} cos={wa.Cos()} tan={wa.Tan()} facing={wa.Facing}");
}

// 3) FromFacing / FromDegrees
for (int f = -8; f <= 8; f++)
    L($"WAF f={f} a={OpenRA.WAngle.FromFacing(f).Angle}");
for (int d = -720; d <= 720; d += 7)
    L($"WAD d={d} a={OpenRA.WAngle.FromDegrees(d).Angle}");

// 4) ArcSin / ArcCos 全量
for (int d = -1024; d <= 1024; d++)
{
    L($"ASIN d={d} a={OpenRA.WAngle.ArcSin(d).Angle}");
    L($"ACOS d={d} a={OpenRA.WAngle.ArcCos(d).Angle}");
}

// 5) ArcTan 网格(含轴与特殊分支)
for (int y = -613; y <= 613; y += 61)
    for (int x = -613; x <= 613; x += 61)
        L($"ATAN y={y} x={x} a={OpenRA.WAngle.ArcTan(y, x).Angle}");
for (int y = -2000000; y <= 2000000; y += 1000000)
    for (int x = -2000000; x <= 2000000; x += 1000000)
        L($"ATAN y={y} x={x} a={OpenRA.WAngle.ArcTan(y, x).Angle}");

// 6) WAngle.Lerp
int[] muls = [0, 1, 5, 13, 999, 1024];
int[] divs = [1, 7, 1000, 1024];
for (int a = 0; a < 1024; a += 97)
    for (int b = 0; b < 1024; b += 89)
        foreach (var m in muls)
            foreach (var d in divs)
                L($"WAL a={a} b={b} m={m} d={d} r={OpenRA.WAngle.Lerp(new(a), new(b), m, d).Angle}");

// 7) MersenneTwister
int[] seeds = [0, 1, 20250929];
foreach (var s in seeds)
{
    var mt = new OpenRA.Support.MersenneTwister(s);
    for (int i = 0; i < 3000; i++)
        L($"MTU s={s} i={i} v={mt.NextUint()}");
    for (int i = 0; i < 1000; i++)
        L($"MTN s={s} i={i} v={mt.Next()}");
    for (int i = 0; i < 200; i++)
        L($"MTUL s={s} i={i} v={mt.NextUlong()}");
    for (int i = 0; i < 500; i++)
    {
        int lo = (Rand() % 2001) - 1000;
        int hi = lo + (int)(Rand() % 3000);
        L($"MTR s={s} i={i} lo={lo} hi={hi} v={mt.Next(lo, hi)}");
    }

    L($"MTSTATE s={s} last={mt.Last} total={mt.TotalCount}");
}

foreach (var s in seeds)
{
    int[][] weightSets = [[1, 2, 3, 0, 7], [0, 0, 0], [5], [10, 10, 10, 10, 10, 10, 10, 10, 10, 10]];
    var mt = new OpenRA.Support.MersenneTwister(s + 1000);
    for (int i = 0; i < 500; i++)
    {
        var w = weightSets[i % weightSets.Length];
        L($"MTPW s={s} i={i} v={mt.PickWeighted(w)}");
    }
}

// 8) WDist/WVec.FromPDF
{
    var mt = new OpenRA.Support.MersenneTwister(7);
    int[] samplesList = [1, 2, 3, 5, 10];
    for (int i = 0; i < 20; i++)
        foreach (var n in samplesList)
            L($"PDF i={i} n={n} v={OpenRA.WDist.FromPDF(mt, n).Length}");

    mt = new OpenRA.Support.MersenneTwister(77);
    for (int i = 0; i < 20; i++)
    {
        var v = OpenRA.WVec.FromPDF(mt, 3);
        L($"PDFV i={i} x={v.X} y={v.Y} z={v.Z}");
    }
}

// 9) WVec/WPos 随机运算
for (int i = 0; i < 4000; i++)
{
    // 值域贴近真实游戏(1 cell = 1024,大地图对角 ~5×10^5),并避开 90° 渐近线邻域
    // (Tan 在 angle≡256 处为 int.MaxValue,真实弹道仰角不可达,下游 (int) 转换会溢出)
    int ax = (Rand() % 200001) - 100000;
    int ay = (Rand() % 200001) - 100000;
    int az = (Rand() % 200001) - 100000;
    int bx = (Rand() % 200001) - 100000;
    int by = (Rand() % 200001) - 100000;
    int bz = (Rand() % 200001) - 100000;
    int s = (Rand() % 201) - 100;
    int d = (int)(Rand() % 100) + 1;
    int m = (int)(Rand() % (uint)d); // 真实调用约束:mul < div(进度分数),否则 LerpQuadratic 会溢出 int
    int p = (int)(Rand() % 1024);
    if ((p & 511) >= 254 && (p & 511) <= 258) p = (p + 8) & 1023; // 避开 90°/270° 两条渐近线邻域

    var a = new OpenRA.WVec(ax, ay, az);
    var b = new OpenRA.WVec(bx, by, bz);
    var add = a + b;
    var sub = a - b;
    var neg = -a;
    var div = a / d;
    var mul = s * a;
    L($"WV i={i} addx={add.X} addy={add.Y} addz={add.Z} subx={sub.X} suby={sub.Y} subz={sub.Z} negx={neg.X} negy={neg.Y} negz={neg.Z} divx={div.X} divy={div.Y} divz={div.Z} mulx={mul.X} muly={mul.Y} mulz={mul.Z}");
    L($"WVL i={i} dot={OpenRA.WVec.Dot(a, b)} lsq={a.LengthSquared} len={a.Length} hlsq={a.HorizontalLengthSquared} hlen={a.HorizontalLength} vlsq={a.VerticalLengthSquared} vlen={a.VerticalLength} yaw={a.Yaw.Angle}");
    var ler = OpenRA.WVec.Lerp(a, b, m, d);
    var lq = OpenRA.WVec.LerpQuadratic(a, b, new(p), m, d);
    L($"WVLQ i={i} lx={ler.X} ly={ler.Y} lz={ler.Z} qx={lq.X} qy={lq.Y} qz={lq.Z}");
    var pa = new OpenRA.WPos(ax, ay, az);
    var pb = new OpenRA.WPos(bx, by, bz);
    var pler = OpenRA.WPos.Lerp(pa, pb, m, d);
    var plq = OpenRA.WPos.LerpQuadratic(pa, pb, new(p), m, d);
    if (i == 0)
    {
        var xv = b - a;
        var dd = (decimal)xv.Length * new OpenRA.WAngle(p).Tan() * m * (d - m) / (1024 * d * d);
        Console.Error.WriteLine($"DBG i=0 ax={ax} ay={ay} az={az} bx={bx} by={by} bz={bz} s={s} d={d} m={m} p={p}");
        Console.Error.WriteLine($"DBG len={xv.Length} tan={new OpenRA.WAngle(p).Tan()} den={1024 * d * d} dec={dd.ToString(System.Globalization.CultureInfo.InvariantCulture)} scale={(System.Decimal.GetBits(dd)[3] >> 16) & 0xFF}");
        var wvq = OpenRA.WVec.LerpQuadratic(a, b, new(p), m, d);
        var wpq = OpenRA.WPos.LerpQuadratic(pa, pb, new(p), m, d);
        Console.Error.WriteLine($"DBG wvecqz={wvq.Z} wposqz={wpq.Z} retz={OpenRA.WVec.Lerp(a, b, m, d).Z} lz={pler.Z}");
    }
    L($"WPLQ i={i} lx={pler.X} ly={pler.Y} lz={pler.Z} qx={plq.X} qy={plq.Y} qz={plq.Z}");
    L($"WVR i={i} rx={a.Rotate(new OpenRA.WRot(new(p / 3 * 3), new((p + 128) / 3 * 3), new((p + 256) / 3 * 3))).X} ry={a.Rotate(new OpenRA.WRot(new(p), new(p), new(p))).Y} rz={a.Rotate(new OpenRA.WRot(new(p), new(p), new(p))).Z}");
}

// 10) WRot 全网格构造 + 矩阵 + Rotate/SLerp/Neg
for (int r = 0; r < 1024; r += 64)
    for (int p = 0; p < 1024; p += 64)
        for (int y = 0; y < 1024; y += 64)
        {
            var rot = new OpenRA.WRot(new(r), new(p), new(y));
            rot.AsMatrix(out var m);
            L($"WR r={r} p={p} y={y} q={rot.x},{rot.y},{rot.z},{rot.w} m={m.M11},{m.M12},{m.M13},{m.M21},{m.M22},{m.M23},{m.M31},{m.M32},{m.M33},{m.M44}");
        }

for (int r = 0; r < 1024; r += 97)
    for (int p = 0; p < 1024; p += 131)
    {
        var a = new OpenRA.WRot(new(r), new(p), new(300));
        var b = new OpenRA.WRot(new(700), new((p + 100) % 1024), new((r + 50) % 1024));
        var rr = a.Rotate(b);
        var neg = -a;
        L($"WRR r={r} p={p} q={rr.x},{rr.y},{rr.z},{rr.w} e={rr.Roll.Angle},{rr.Pitch.Angle},{rr.Yaw.Angle} nq={neg.x},{neg.y},{neg.z},{neg.w} ne={neg.Roll.Angle},{neg.Pitch.Angle},{neg.Yaw.Angle}");
    }

for (int i = 0; i < 2000; i++)
{
    int r = Rand() % 1024, p = Rand() % 1024, y = Rand() % 1024;
    int r2 = Rand() % 1024, p2 = Rand() % 1024, y2 = Rand() % 1024;
    int m = Rand() % 100, d = Rand() % 100 + 1;
    var ra = new OpenRA.WRot(new(r), new(p), new(y));
    var rb = new OpenRA.WRot(new(r2), new(p2), new(y2));
    var s = OpenRA.WRot.SLerp(ra, rb, m, d);
    L($"WRS i={i} q={s.x},{s.y},{s.z},{s.w} e={s.Roll.Angle},{s.Pitch.Angle},{s.Yaw.Angle}");
}

// 11) CPos/CVec/MPos 网格
for (int x = -2048; x <= 2047; x += 131)
    for (int yy = -2048; yy <= 2047; yy += 131)
        foreach (var layer in new byte[] { 0, 1, 255 })
        {
            var c = new OpenRA.CPos(x, yy, layer);
            L($"CP x={x} y={yy} l={layer} bits={c.Bits} rx={c.X} ry={c.Y} rl={c.Layer}");
        }

for (int x = -40; x <= 40; x += 3)
    for (int yy = -40; yy <= 40; yy += 3)
    {
        var c = new OpenRA.CPos(x, yy);
        var mr = c.ToMPos(OpenRA.MapGridType.Rectangular);
        var mi = c.ToMPos(OpenRA.MapGridType.RectangularIsometric);
        var br = mr.ToCPos(OpenRA.MapGridType.Rectangular);
        var bi = mi.ToCPos(OpenRA.MapGridType.RectangularIsometric);
        L($"CPM x={x} y={yy} ru={mr.U} rv={mr.V} iu={mi.U} iv={mi.V} bx={bi.X} by={bi.Y} rx={br.X} ry={br.Y}");
    }

// 12) CVec 运算采样
for (int i = 0; i < 2000; i++)
{
    int ax = (Rand() % 4001) - 2000;
    int ay = (Rand() % 4001) - 2000;
    int bx = (Rand() % 4001) - 2000;
    int by = (Rand() % 4001) - 2000;
    int s = (int)(Rand() % 50) + 1;
    var a = new OpenRA.CVec(ax, ay);
    var b = new OpenRA.CVec(bx, by);
    var add = a + b;
    var sub = a - b;
    var mul = s * a;
    var div = a / s;
    var mx = OpenRA.CVec.Max(a, b);
    var mn = OpenRA.CVec.Min(a, b);
    L($"CV i={i} ax={ax} ay={ay} bx={bx} by={by} s={s} addx={add.X} addy={add.Y} subx={sub.X} suby={sub.Y} mulx={mul.X} muly={mul.Y} divx={div.X} divy={div.Y} maxx={mx.X} maxy={mx.Y} minx={mn.X} miny={mn.Y} dot={OpenRA.CVec.Dot(a, b)} lsq={a.LengthSquared} len={a.Length} sgx={a.Sign().X} sgy={a.Sign().Y} absx={a.Abs().X} absy={a.Abs().Y}");
}

// ————————————————————————————————————————————————————————————————

var outPath = args.Length > 0 ? args[0] : "golden_core.txt";
File.WriteAllText(outPath, sb.ToString());
Console.WriteLine($"golden written: {outPath} ({sb.Length} chars, {sb.ToString().Count(c => c == '\n')} lines)");

#region OpenRA verbatim primitives

namespace OpenRA.Support
{
    // OpenRA.Game/Support/MersenneTwister.cs(逐字,去掉无参构造)
    public class MersenneTwister
    {
        readonly uint[] mt = new uint[624];
        int index = 0;

        public int Last;
        public int TotalCount = 0;

        public MersenneTwister(int seed)
        {
            mt[0] = (uint)seed;
            for (var i = 1u; i < mt.Length; i++)
                mt[i] = 1812433253u * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
        }

        public uint NextUint()
        {
            if (index == 0) Generate();

            var y = mt[index];
            y ^= y >> 11;
            y ^= (y << 7) & 2636928640;
            y ^= (y << 15) & 4022730752;
            y ^= y >> 18;

            index = (index + 1) % 624;
            TotalCount++;
            Last = (int)(y % int.MaxValue);
            return y;
        }

        public ulong NextUlong()
        {
            return (ulong)NextUint() << 32 | NextUint();
        }

        public int Next()
        {
            NextUint();
            return Last;
        }

        public int Next(int low, int high)
        {
            var diff = high - low;
            if (diff <= 1)
                return low;

            return low + Next() % diff;
        }

        public int PickWeighted(ReadOnlySpan<int> weights)
        {
            ulong total = 0;
            for (var i = 0; i < weights.Length; i++)
                total += (ulong)weights[i];

            if (total == 0)
                return Next(0, weights.Length);

            var spin = NextUlong() % total;
            ulong acc = 0;
            for (var i = 0; i < weights.Length; i++)
            {
                acc += (ulong)weights[i];
                if (spin < acc)
                    return i;
            }

            throw new InvalidOperationException("unreachable");
        }

        void Generate()
        {
            unchecked
            {
                for (var i = 0u; i < mt.Length; i++)
                {
                    var y = (mt[i] & 0x80000000) | (mt[(i + 1) % 624] & 0x7fffffff);
                    mt[i] = mt[(i + 397u) % 624u] ^ (y >> 1);
                    if ((y & 1) == 1)
                        mt[i] ^= 2567483615;
                }
            }
        }
    }
}

namespace OpenRA
{
    // OpenRA.Game/WAngle.cs(运算部分逐字;查表访问改普通数组索引)
    public readonly struct WAngle
    {
        public readonly int Angle;

        public WAngle(int a)
        {
            // Bitwise mask handles wrapping and negatives.
            Angle = a & 1023;
        }

        public static WAngle FromFacing(int facing) { return new WAngle(facing * 4); }
        public static WAngle FromDegrees(int degrees) { return new WAngle(degrees * 1024 / 360); }
        public static WAngle operator +(WAngle a, WAngle b) { return new WAngle(a.Angle + b.Angle); }
        public static WAngle operator -(WAngle a, WAngle b) { return new WAngle(a.Angle - b.Angle); }
        public static WAngle operator -(WAngle a) { return new WAngle(-a.Angle); }

        public static bool operator ==(WAngle me, WAngle other) { return me.Angle == other.Angle; }
        public static bool operator !=(WAngle me, WAngle other) { return me.Angle != other.Angle; }

        public int Facing => Angle / 4;

        public int Sin()
        {
            return new WAngle(Angle - 256).Cos();
        }

        public int Cos()
        {
            var angle = Angle;
            var qIndex = angle & 511;
            var mirrored = 256 - qIndex;
            var mask = mirrored >> 31;
            var finalIndex = 256 - ((mirrored ^ mask) - mask);
            var signBit = (int)((uint)(angle + 256) >> 9) & 1;
            var sign = 1 - (signBit << 1);
            return sign * CosineTable[finalIndex];
        }

        public int Tan()
        {
            var angle = Angle & 511;
            var shifted = angle - 257;
            var mask = shifted >> 31;
            var triangle = ((angle - 256) ^ (angle - 256 >> 31)) - (angle - 256 >> 31);
            var finalIndex = 256 - triangle;
            var sign = -1 - (mask << 1);
            return sign * TanTable[finalIndex];
        }

        public static WAngle Lerp(WAngle a, WAngle b, int mul, int div)
        {
            var start = a.Angle;
            var diff = b.Angle - start;
            var mask1 = (511 - diff) >> 31;
            var mask2 = (diff + 512) >> 31;
            diff += (mask1 & -1024) | (mask2 & 1024);
            return new WAngle(start + diff * mul / div);
        }

        public static WAngle ArcSin(int d)
        {
            var index = GetClosestCosineIndex(Math.Abs(d));
            var sign = d >> 31;
            return new WAngle((sign & (768 + index)) | (~sign & (256 - index)));
        }

        public static WAngle ArcCos(int d)
        {
            var index = GetClosestCosineIndex(Math.Abs(d));
            var sign = d >> 31;
            return new WAngle((sign & (512 - index)) | (~sign & index));
        }

        static int GetClosestCosineIndex(int value)
        {
            var index = 0;
            index |= (CosineTable[index | 128] > value) ? 128 : 0;
            index |= (CosineTable[index | 64] > value) ? 64 : 0;
            index |= (CosineTable[index | 32] > value) ? 32 : 0;
            index |= (CosineTable[index | 16] > value) ? 16 : 0;
            index |= (CosineTable[index | 8] > value) ? 8 : 0;
            index |= (CosineTable[index | 4] > value) ? 4 : 0;
            index |= (CosineTable[index | 2] > value) ? 2 : 0;
            index |= (CosineTable[index | 1] > value) ? 1 : 0;

            int val0 = CosineTable[index];
            int val1 = CosineTable[Math.Min(index + 1, 256)];

            return (val0 - value > value - val1) ? index + 1 : index;
        }

        public static WAngle ArcTan(int y, int x)
        {
            if (y == 0)
                return new WAngle(x >= 0 ? 0 : 512);

            if (x == 0)
                return new WAngle(y > 0 ? 256 : 768);

            var ay = Math.Abs((long)y);
            var ax = Math.Abs((long)x);

            if (ay >= ax * 167)
                return new WAngle(y > 0 ? 256 : 768);

            var target = (int)((ay << 10) / ax);

            var index = 0;
            index |= (TanTable[index | 128] <= target) ? 128 : 0;
            index |= (TanTable[index | 64] <= target) ? 64 : 0;
            index |= (TanTable[index | 32] <= target) ? 32 : 0;
            index |= (TanTable[index | 16] <= target) ? 16 : 0;
            index |= (TanTable[index | 8] <= target) ? 8 : 0;
            index |= (TanTable[index | 4] <= target) ? 4 : 0;
            index |= (TanTable[index | 2] <= target) ? 2 : 0;
            index |= (TanTable[index | 1] <= target) ? 1 : 0;

            var val = TanTable[index];
            var nextVal = TanTable[index + 1];
            index += (target - val > nextVal - target) ? 1 : 0;

            var xNegResult = 512 + (y < 0 ? index : -index);
            var xPosResult = y < 0 ? 1024 - index : index;

            return new WAngle(x < 0 ? xNegResult : xPosResult);
        }

        static readonly short[] CosineTable =
        [
            1024, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1022, 1022, 1022, 1021,
            1021, 1020, 1020, 1019, 1019, 1018, 1017, 1017, 1016, 1015, 1014, 1013,
            1012, 1011, 1010, 1009, 1008, 1007, 1006, 1005, 1004, 1003, 1001, 1000,
            999, 997, 996, 994, 993, 991, 990, 988, 986, 985, 983, 981, 979, 978,
            976, 974, 972, 970, 968, 966, 964, 962, 959, 957, 955, 953, 950, 948,
            946, 943, 941, 938, 936, 933, 930, 928, 925, 922, 920, 917, 914, 911,
            908, 906, 903, 900, 897, 894, 890, 887, 884, 881, 878, 875, 871, 868,
            865, 861, 858, 854, 851, 847, 844, 840, 837, 833, 829, 826, 822, 818,
            814, 811, 807, 803, 799, 795, 791, 787, 783, 779, 775, 771, 767, 762,
            758, 754, 750, 745, 741, 737, 732, 728, 724, 719, 715, 710, 706, 701,
            696, 692, 687, 683, 678, 673, 668, 664, 659, 654, 649, 644, 639, 634,
            629, 625, 620, 615, 609, 604, 599, 594, 589, 584, 579, 574, 568, 563,
            558, 553, 547, 542, 537, 531, 526, 521, 515, 510, 504, 499, 493, 488,
            482, 477, 471, 466, 460, 454, 449, 443, 437, 432, 426, 420, 414, 409,
            403, 397, 391, 386, 380, 374, 368, 362, 356, 350, 344, 339, 333, 327,
            321, 315, 309, 303, 297, 291, 285, 279, 273, 267, 260, 254, 248, 242,
            236, 230, 224, 218, 212, 205, 199, 193, 187, 181, 175, 168, 162, 156,
            150, 144, 137, 131, 125, 119, 112, 106, 100, 94, 87, 81, 75, 69, 62,
            56, 50, 43, 37, 31, 25, 18, 12, 6, 0
        ];

        static readonly int[] TanTable =
        [
            0, 6, 12, 18, 25, 31, 37, 44, 50, 56, 62, 69, 75, 81, 88, 94, 100, 107,
            113, 119, 126, 132, 139, 145, 151, 158, 164, 171, 177, 184, 190, 197,
            203, 210, 216, 223, 229, 236, 243, 249, 256, 263, 269, 276, 283, 290,
            296, 303, 310, 317, 324, 331, 338, 345, 352, 359, 366, 373, 380, 387,
            395, 402, 409, 416, 424, 431, 438, 446, 453, 461, 469, 476, 484, 492,
            499, 507, 515, 523, 531, 539, 547, 555, 563, 571, 580, 588, 596, 605,
            613, 622, 630, 639, 648, 657, 666, 675, 684, 693, 702, 711, 721, 730,
            740, 749, 759, 769, 779, 789, 799, 809, 819, 829, 840, 850, 861, 872,
            883, 894, 905, 916, 928, 939, 951, 963, 974, 986, 999, 1011, 1023, 1036,
            1049, 1062, 1075, 1088, 1102, 1115, 1129, 1143, 1158, 1172, 1187, 1201,
            1216, 1232, 1247, 1263, 1279, 1295, 1312, 1328, 1345, 1363, 1380, 1398,
            1416, 1435, 1453, 1473, 1492, 1512, 1532, 1553, 1574, 1595, 1617, 1639,
            1661, 1684, 1708, 1732, 1756, 1782, 1807, 1833, 1860, 1887, 1915, 1944,
            1973, 2003, 2034, 2065, 2098, 2131, 2165, 2199, 2235, 2272, 2310, 2348,
            2388, 2429, 2472, 2515, 2560, 2606, 2654, 2703, 2754, 2807, 2861, 2918,
            2976, 3036, 3099, 3164, 3232, 3302, 3375, 3451, 3531, 3613, 3700, 3790,
            3885, 3984, 4088, 4197, 4311, 4432, 4560, 4694, 4836, 4987, 5147, 5318,
            5499, 5693, 5901, 6124, 6364, 6622, 6903, 7207, 7539, 7902, 8302, 8743,
            9233, 9781, 10396, 11094, 11891, 12810, 13882, 15148, 16667, 18524, 20843,
            23826, 27801, 33366, 41713, 55622, 83438, 166883, int.MaxValue
        ];
    }

    // OpenRA.Game/WDist.cs(运算部分逐字)
    public readonly struct WDist
    {
        public readonly int Length;

        public WDist(int r) { Length = r; }

        public static WDist FromPDF(OpenRA.Support.MersenneTwister r, int samples)
        {
            var sum = 0;
            for (var i = 0; i < samples; i++)
                sum += r.Next(-1024, 1024);
            return new WDist(sum / samples);
        }
    }

    // OpenRA.Game/WPos.cs / WVec.cs(运算部分逐字,decimal 保留)
    public readonly struct WPos
    {
        public readonly int X, Y, Z;

        public WPos(int x, int y, int z) { X = x; Y = y; Z = z; }

        public static WPos operator +(in WPos a, in WVec b) { return new WPos(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
        public static WPos operator -(in WPos a, in WVec b) { return new WPos(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
        public static WVec operator -(in WPos a, in WPos b) { return new WVec(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }

        public static WPos Lerp(in WPos a, in WPos b, int mul, int div) { return a + (b - a) * mul / div; }

        public static WPos LerpQuadratic(in WPos a, in WPos b, WAngle pitch, int mul, int div)
        {
            var ret = Lerp(a, b, mul, div);

            if (pitch.Angle == 0)
                return ret;

            var offset = (decimal)(b - a).Length * pitch.Tan() * mul * (div - mul) / (1024 * div * div);
            var clampedOffset = (int)(offset + ret.Z).Clamp(int.MinValue, int.MaxValue);

            return new WPos(ret.X, ret.Y, clampedOffset);
        }
    }

    public readonly struct WVec
    {
        public readonly int X, Y, Z;

        public WVec(int x, int y, int z) { X = x; Y = y; Z = z; }
        public WVec(WDist x, WDist y, WDist z) { X = x.Length; Y = y.Length; Z = z.Length; }

        public static WVec operator +(in WVec a, in WVec b) { return new WVec(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
        public static WVec operator -(in WVec a, in WVec b) { return new WVec(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
        public static WVec operator -(in WVec a) { return new WVec(-a.X, -a.Y, -a.Z); }
        public static WVec operator /(in WVec a, int b) { return new WVec(a.X / b, a.Y / b, a.Z / b); }
        public static WVec operator *(int a, in WVec b) { return new WVec(a * b.X, a * b.Y, a * b.Z); }
        public static WVec operator *(in WVec a, int b) { return b * a; }

        public static int Dot(in WVec a, in WVec b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
        public long LengthSquared => (long)X * X + (long)Y * Y + (long)Z * Z;
        public int Length => (int)Exts.ISqrt(LengthSquared);
        public long HorizontalLengthSquared => (long)X * X + (long)Y * Y;
        public int HorizontalLength => (int)Exts.ISqrt(HorizontalLengthSquared);
        public long VerticalLengthSquared => (long)Z * Z;
        public int VerticalLength => (int)Exts.ISqrt(VerticalLengthSquared);

        public WVec Rotate(in WRot rot)
        {
            rot.AsMatrix(out var mtx);
            return Rotate(ref mtx);
        }

        public WVec Rotate(ref Int32Matrix4x4 mtx)
        {
            var lx = (long)X;
            var ly = (long)Y;
            var lz = (long)Z;
            return new WVec(
                (int)((lx * mtx.M11 + ly * mtx.M21 + lz * mtx.M31) / mtx.M44),
                (int)((lx * mtx.M12 + ly * mtx.M22 + lz * mtx.M32) / mtx.M44),
                (int)((lx * mtx.M13 + ly * mtx.M23 + lz * mtx.M33) / mtx.M44));
        }

        public WAngle Yaw
        {
            get
            {
                if (LengthSquared == 0)
                    return new WAngle(0);

                return WAngle.ArcTan(-Y, X) - new WAngle(256);
            }
        }

        public static WVec Lerp(in WVec a, in WVec b, int mul, int div) { return a + (b - a) * mul / div; }

        public static WVec LerpQuadratic(in WVec a, in WVec b, WAngle pitch, int mul, int div)
        {
            var ret = Lerp(a, b, mul, div);

            if (pitch.Angle == 0)
                return ret;

            var offset = (int)((decimal)(b - a).Length * pitch.Tan() * mul * (div - mul) / (1024 * div * div));
            return new WVec(ret.X, ret.Y, ret.Z + offset);
        }

        public static WVec FromPDF(OpenRA.Support.MersenneTwister r, int samples)
        {
            return new WVec(WDist.FromPDF(r, samples), WDist.FromPDF(r, samples), new WDist(0));
        }
    }

    // OpenRA.Game/WRot.cs(运算部分逐字;唯一差异:x/y/z/w 从 private 改 public 以供 golden 输出)
    public readonly struct WRot
    {
        public readonly WAngle Roll, Pitch, Yaw;
        public readonly int x, y, z, w;

        public WRot(WAngle roll, WAngle pitch, WAngle yaw)
        {
            Roll = roll;
            Pitch = pitch;
            Yaw = yaw;

            var qr = new WAngle(-Roll.Angle / 2);
            var qp = new WAngle(-Pitch.Angle / 2);
            var qy = new WAngle(-Yaw.Angle / 2);
            var cr = (long)qr.Cos();
            var sr = (long)qr.Sin();
            var cp = (long)qp.Cos();
            var sp = (long)qp.Sin();
            var cy = (long)qy.Cos();
            var sy = (long)qy.Sin();

            x = (int)((sr * cp * cy - cr * sp * sy) / 1048576);
            y = (int)((cr * sp * cy + sr * cp * sy) / 1048576);
            z = (int)((cr * cp * sy - sr * sp * cy) / 1048576);
            w = (int)((cr * cp * cy + sr * sp * sy) / 1048576);
        }

        public WRot(WVec axis, WAngle angle)
        {
            x = axis.X * new WAngle(-angle.Angle / 2).Sin() / 1024;
            y = axis.Y * new WAngle(-angle.Angle / 2).Sin() / 1024;
            z = axis.Z * new WAngle(-angle.Angle / 2).Sin() / 1024;
            w = new WAngle(-angle.Angle / 2).Cos();

            (Roll, Pitch, Yaw) = QuaternionToEuler(x, y, z, w);
        }

        WRot(int x, int y, int z, int w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;

            (Roll, Pitch, Yaw) = QuaternionToEuler(x, y, z, w);
        }

        static (WAngle Roll, WAngle Pitch, WAngle Yaw) QuaternionToEuler(int x, int y, int z, int w)
        {
            var lsq = x * x + y * y + z * z + w * w;

            var srcp = 2 * (w * x + y * z);
            var crcp = lsq - 2 * (x * x + y * y);
            var sp = (w * y - z * x) / 512;
            var sycp = 2 * (w * z + x * y);
            var cycp = lsq - 2 * (y * y + z * z);

            var roll = -WAngle.ArcTan(srcp, crcp);
            var pitch = -(Math.Abs(sp) >= 1024 ? new WAngle(Math.Sign(sp) * 256) : WAngle.ArcSin(sp));
            var yaw = -WAngle.ArcTan(sycp, cycp);

            return (roll, pitch, yaw);
        }

        WRot(int x, int y, int z, int w, WAngle roll, WAngle pitch, WAngle yaw)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
            Roll = roll;
            Pitch = pitch;
            Yaw = yaw;
        }

        public static readonly WRot None = new(new WAngle(0), new WAngle(0), new WAngle(0));

        public static WRot FromFacing(int facing) { return new WRot(new WAngle(0), new WAngle(0), WAngle.FromFacing(facing)); }
        public static WRot operator +(in WRot a, in WRot b) { return new WRot(a.Roll + b.Roll, a.Pitch + b.Pitch, a.Yaw + b.Yaw); }
        public static WRot operator -(in WRot a, in WRot b) { return new WRot(a.Roll - b.Roll, a.Pitch - b.Pitch, a.Yaw - b.Yaw); }
        public static WRot operator -(in WRot a) { return new WRot(-a.x, -a.y, -a.z, a.w, -a.Roll, -a.Pitch, -a.Yaw); }

        public WRot Rotate(in WRot rot)
        {
            if (this == None)
                return rot;

            if (rot == None)
                return this;

            var rx = ((long)rot.w * x + (long)rot.x * w + (long)rot.y * z - (long)rot.z * y) / 1024;
            var ry = ((long)rot.w * y - (long)rot.x * z + (long)rot.y * w + (long)rot.z * x) / 1024;
            var rz = ((long)rot.w * z + (long)rot.x * y - (long)rot.y * x + (long)rot.z * w) / 1024;
            var rw = ((long)rot.w * w - (long)rot.x * x - (long)rot.y * y - (long)rot.z * z) / 1024;

            return new WRot((int)rx, (int)ry, (int)rz, (int)rw);
        }

        public static bool operator ==(in WRot me, in WRot other)
        {
            return me.Roll == other.Roll && me.Pitch == other.Pitch && me.Yaw == other.Yaw;
        }

        public static bool operator !=(in WRot me, in WRot other) { return !(me == other); }

        public void AsMatrix(out Int32Matrix4x4 mtx)
        {
            var lsq = x * x + y * y + z * z + w * w;

            mtx = new Int32Matrix4x4(
                lsq - 2 * (y * y + z * z),
                2 * (x * y + z * w),
                2 * (x * z - y * w),
                0,
                2 * (x * y - z * w),
                lsq - 2 * (x * x + z * z),
                2 * (y * z + x * w),
                0,
                2 * (x * z + y * w),
                2 * (y * z - x * w),
                lsq - 2 * (x * x + y * y),
                0,
                0,
                0,
                0,
                lsq);
        }

        public static WRot SLerp(in WRot a, in WRot b, int mul, int div)
        {
            var dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            var flip = dot >= 0 ? 1 : -1;

            if (flip * dot >= 1024 * 1024)
                return a;

            var theta = WAngle.ArcCos(dot / 1024);
            var s1 = new WAngle((div - mul) * theta.Angle / div).Sin();
            var s2 = new WAngle(mul * theta.Angle / div).Sin();
            var s3 = theta.Sin();

            var x = ((long)a.x * s1 + flip * b.x * s2) / s3;
            var y = ((long)a.y * s1 + flip * b.y * s2) / s3;
            var z = ((long)a.z * s1 + flip * b.z * s2) / s3;
            var w = ((long)a.w * s1 + flip * b.w * s2) / s3;

            var l = Exts.ISqrt(x * x + y * y + z * z + w * w);
            return new WRot((int)(1024 * x / l), (int)(1024 * y / l), (int)(1024 * z / l), (int)(1024 * w / l));
        }
    }

    // OpenRA.Game/Primitives/Int32Matrix4x4.cs(逐字)
    public readonly struct Int32Matrix4x4
    {
        public readonly int M11, M12, M13, M14, M21, M22, M23, M24, M31, M32, M33, M34, M41, M42, M43, M44;

        public Int32Matrix4x4(
            int m11, int m12, int m13, int m14,
            int m21, int m22, int m23, int m24,
            int m31, int m32, int m33, int m34,
            int m41, int m42, int m43, int m44)
        {
            M11 = m11; M12 = m12; M13 = m13; M14 = m14;
            M21 = m21; M22 = m22; M23 = m23; M24 = m24;
            M31 = m31; M32 = m32; M33 = m33; M34 = m34;
            M41 = m41; M42 = m42; M43 = m43; M44 = m44;
        }
    }

    // OpenRA.Game/Exts.cs:282-363(ISqrt,逐字)
    public static class Exts
    {
        public enum ISqrtRoundMode { Floor, Nearest, Ceiling }

        public static int ISqrt(int number, ISqrtRoundMode round = ISqrtRoundMode.Floor)
        {
            if (number < 0)
                throw new InvalidOperationException($"Attempted to calculate the square root of a negative integer: {number}");

            return (int)ISqrt((uint)number, round);
        }

        public static uint ISqrt(uint number, ISqrtRoundMode round = ISqrtRoundMode.Floor)
        {
            var divisor = 1U << 30;

            var root = 0U;
            var remainder = number;

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

            if (round == ISqrtRoundMode.Nearest && remainder > root)
                root++;
            else if (round == ISqrtRoundMode.Ceiling && root * root < number)
                root++;

            return root;
        }

        public static long ISqrt(long number, ISqrtRoundMode round = ISqrtRoundMode.Floor)
        {
            return (long)ISqrt((ulong)number, round);
        }

        public static ulong ISqrt(ulong number, ISqrtRoundMode round = ISqrtRoundMode.Floor)
        {
            var divisor = 1UL << 62;

            var root = 0UL;
            var remainder = number;

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

            if (round == ISqrtRoundMode.Nearest && remainder > root)
                root++;
            else if (round == ISqrtRoundMode.Ceiling && root * root < number)
                root++;

            return root;
        }

        public static T Clamp<T>(this T val, T min, T max) where T : IComparable<T>
        {
            if (val.CompareTo(min) < 0)
                return min;
            if (val.CompareTo(max) > 0)
                return max;
            return val;
        }

        // OpenRA.Game/Exts.cs L383-386(逐字)
        public static string JoinWith<T>(this IEnumerable<T> ts, string j)
        {
            return string.Join(j, ts);
        }

        // OpenRA.Game/Exts.cs L414-467(逐字;MiniYaml.Merge 的冲突日志依赖)
        public static void IntoDictionaryWithConflictLog<TSource, TKey, TElement>(
            this IEnumerable<TSource> source, Func<TSource, TKey> keySelector, Func<TSource, TElement> elementSelector,
            string debugName, Dictionary<TKey, TElement> output,
            Func<TKey, string> logKey = null, Func<TElement, string> logValue = null)
        {
            // Fall back on ToString() if null functions are provided:
            logKey ??= s => s.ToString();
            logValue ??= s => s.ToString();

            // Try to build a dictionary and log all duplicates found (if any):
            Dictionary<TKey, List<string>> dupKeys = null;
            var capacity = source is ICollection<TSource> collection ? collection.Count : 0;
            output.Clear();
            output.EnsureCapacity(capacity);
            foreach (var item in source)
            {
                var key = keySelector(item);
                var element = elementSelector(item);

                // Discard elements with null keys
                if (!typeof(TKey).IsValueType && key == null)
                    continue;

                // Check for a key conflict:
                if (!output.TryAdd(key, element))
                {
                    dupKeys ??= [];
                    if (!dupKeys.TryGetValue(key, out var dupKeyMessages))
                    {
                        // Log the initial conflicting value already inserted:
                        dupKeyMessages =
                        [
                            logValue(output[key])
                        ];
                        dupKeys.Add(key, dupKeyMessages);
                    }

                    // Log this conflicting value:
                    dupKeyMessages.Add(logValue(element));
                }
            }

            // If any duplicates were found, throw a descriptive error
            if (dupKeys != null)
            {
                var badKeysFormatted = new StringBuilder(
                    $"{debugName}, duplicate values found for the following keys: ");
                foreach (var p in dupKeys)
                    badKeysFormatted.Append(CultureInfo.InvariantCulture, $"{logKey(p.Key)}: [{string.Join(",", p.Value)}]");
                throw new ArgumentException(badKeysFormatted.ToString());
            }
        }
    }

    // OpenRA.Game/Primitives/MapGrid.cs 中的枚举(值仅作判别)
    public enum MapGridType { Rectangular, RectangularIsometric }

    // OpenRA.Game/CVec.cs(运算部分逐字)
    public readonly struct CVec
    {
        public readonly int X, Y;

        public CVec(int x, int y) { X = x; Y = y; }

        public static CVec operator +(CVec a, CVec b) { return new CVec(a.X + b.X, a.Y + b.Y); }
        public static CVec operator -(CVec a, CVec b) { return new CVec(a.X - b.X, a.Y - b.Y); }
        public static CVec operator *(int a, CVec b) { return new CVec(a * b.X, a * b.Y); }
        public static CVec operator *(CVec b, int a) { return new CVec(a * b.X, a * b.Y); }
        public static CVec operator /(CVec a, int b) { return new CVec(a.X / b, a.Y / b); }
        public static CVec operator -(CVec a) { return new CVec(-a.X, -a.Y); }

        public static CVec Max(CVec a, CVec b) { return new CVec(Math.Max(a.X, b.X), Math.Max(a.Y, b.Y)); }
        public static CVec Min(CVec a, CVec b) { return new CVec(Math.Min(a.X, b.X), Math.Min(a.Y, b.Y)); }

        public static int Dot(CVec a, CVec b) { return a.X * b.X + a.Y * b.Y; }

        public CVec Sign() { return new CVec(Math.Sign(X), Math.Sign(Y)); }
        public CVec Abs() { return new CVec(Math.Abs(X), Math.Abs(Y)); }
        public int LengthSquared => X * X + Y * Y;
        public int Length => Exts.ISqrt(LengthSquared);

        public override string ToString() { return X + "," + Y; }
    }

    // OpenRA.Game/CPos.cs(运算部分逐字)
    public readonly struct CPos
    {
        // Packing is XXXX XXXX XXXX YYYY YYYY YYYY LLLL LLLL
        public readonly int Bits;

        public int X => Bits >> 20;
        public int Y => ((short)(Bits >> 4)) >> 4;
        public byte Layer => (byte)Bits;

        public CPos(int bits) { Bits = bits; }

        public CPos(int x, int y)
            : this(x, y, 0) { }

        public CPos(int x, int y, byte layer)
        {
            Bits = (x & 0xFFF) << 20 | (y & 0xFFF) << 8 | layer;
        }

        public MPos ToMPos(MapGridType gridType)
        {
            if (gridType == MapGridType.Rectangular)
                return new MPos(X, Y);

            var v = X + Y;
            var u = (v - (v & 1)) / 2 - Y;
            return new MPos(u, v);
        }
    }

    // OpenRA.Game/MPos.cs(运算部分逐字)
    public readonly struct MPos
    {
        public readonly int U, V;

        public MPos(int u, int v) { U = u; V = v; }

        public CPos ToCPos(MapGridType gridType)
        {
            if (gridType == MapGridType.Rectangular)
                return new CPos(U, V);

            var y = (V - (V & 1)) / 2 - U;
            var x = V - y;
            return new CPos(x, y);
        }
    }
}

namespace OpenRA
{
    using System.Collections.Immutable;

    // OpenRA.Game/StreamExts.cs L200-248(逐字;MiniYaml.FromStream 的行切分语义)
    public static class StreamExts
    {
        /// <summary>
        /// Streams each line of characters from a stream, exposing the line as <see cref="ReadOnlyMemory{T}"/>.
        /// The memory lifetime is only valid during that iteration. Advancing the iteration invalidates the memory.
        /// Consumers should call <see cref="ReadOnlyMemory{T}.Span"/> on each line and otherwise avoid operating on
        /// the memory to ensure they meet the lifetime restrictions.
        /// </summary>
        public static IEnumerable<ReadOnlyMemory<char>> ReadAllLinesAsMemory(this Stream s)
        {
            var buffer = ArrayPool<char>.Shared.Rent(128);
            try
            {
                using (var sr = new StreamReader(s))
                {
                    var offset = 0;
                    int read;
                    while ((read = sr.Read(buffer, offset, buffer.Length - offset)) != 0)
                    {
                        offset += read;

                        var consumedIndex = 0;
                        int newlineIndex;
                        while ((newlineIndex = Array.IndexOf(buffer, '\n', offset - read, read)) != -1)
                        {
                            if (newlineIndex > 0 && buffer[newlineIndex - 1] == '\r')
                                yield return buffer.AsMemory(consumedIndex, newlineIndex - consumedIndex - 1);
                            else
                                yield return buffer.AsMemory(consumedIndex, newlineIndex - consumedIndex);

                            var afterNewlineIndex = newlineIndex + 1;
                            read = offset - afterNewlineIndex;
                            consumedIndex = afterNewlineIndex;
                        }

                        if (consumedIndex > 0)
                        {
                            Array.Copy(buffer, consumedIndex, buffer, 0, offset - consumedIndex);
                            offset = read;
                        }

                        if (offset == buffer.Length)
                        {
                            var newBuffer = ArrayPool<char>.Shared.Rent(buffer.Length * 2);
                            Array.Copy(buffer, newBuffer, buffer.Length);
                            ArrayPool<char>.Shared.Return(buffer);
                            buffer = newBuffer;
                        }
                    }

                    if (offset > 0)
                        yield return buffer.AsMemory(0, offset);
                }
            }
            finally
            {
                ArrayPool<char>.Shared.Return(buffer);
            }
        }
    }

    // OpenRA.Game/MiniYaml.cs L22-791(逐字;仅剥离 Load 方法及其
    // using OpenRA.FileSystem / FieldLoader 依赖 —— 该路径由 C++ 侧集成测试覆盖)
    public static class MiniYamlExts
    {
        public static void WriteToFile(this IEnumerable<MiniYamlNode> y, string filename)
        {
            File.WriteAllLines(filename, y.ToLines().Select(x => x.TrimEnd()).ToArray());
        }

        public static string WriteToString(this IEnumerable<MiniYamlNode> y)
        {
            // Remove all trailing newlines and restore the final EOF newline
            return y.ToLines().JoinWith("\n").TrimEnd('\n') + "\n";
        }

        public static IEnumerable<string> ToLines(this IEnumerable<MiniYamlNode> y)
        {
            foreach (var kv in y)
                foreach (var line in kv.Value.ToLines(kv.Key, kv.Comment))
                    yield return line;
        }

        public static void WriteToFile(this IEnumerable<MiniYamlNodeBuilder> y, string filename)
        {
            File.WriteAllLines(filename, y.ToLines().Select(x => x.TrimEnd()).ToArray());
        }

        public static string WriteToString(this IEnumerable<MiniYamlNodeBuilder> y)
        {
            // Remove all trailing newlines and restore the final EOF newline
            return y.ToLines().JoinWith("\n").TrimEnd('\n') + "\n";
        }

        public static IEnumerable<string> ToLines(this IEnumerable<MiniYamlNodeBuilder> y)
        {
            foreach (var kv in y)
                foreach (var line in kv.Value.ToLines(kv.Key, kv.Comment))
                    yield return line;
        }
    }

    public sealed class MiniYamlNode
    {
        public readonly struct SourceLocation(string name, int line)
        {
            public readonly string Name = name;
            public readonly int Line = line;

            public override string ToString() { return $"{Name}:{Line}"; }
        }

        public readonly SourceLocation Location;
        public readonly string Key;
        public readonly MiniYaml Value;
        public readonly string Comment;

        public MiniYamlNode WithValue(MiniYaml value)
        {
            if (Value == value)
                return this;
            return new MiniYamlNode(Key, value, Comment, Location);
        }

        public MiniYamlNode(string k, MiniYaml v, string c = null)
        {
            Key = k;
            Value = v;
            Comment = c;
        }

        public MiniYamlNode(string k, MiniYaml v, string c, SourceLocation loc)
            : this(k, v, c)
        {
            Location = loc;
        }

        public MiniYamlNode(string k, string v, string c = null)
            : this(k, new MiniYaml(v, []), c) { }

        public MiniYamlNode(string k, string v, IEnumerable<MiniYamlNode> n)
            : this(k, new MiniYaml(v, n), null) { }

        public override string ToString()
        {
            return $"{{YamlNode: {Key} @ {Location}}}";
        }
    }

    public sealed class MiniYaml
    {
        const int SpacesPerLevel = 4;
        static readonly Func<string, string> StringIdentity = s => s;
        static readonly Func<MiniYaml, MiniYaml> MiniYamlIdentity = my => my;
        static readonly Lock ConflictScratchLock = new();
        static readonly Dictionary<string, MiniYamlNode> ConflictScratch = [];

        public readonly string Value;
        public readonly ImmutableArray<MiniYamlNode> Nodes;

        public MiniYaml WithValue(string value)
        {
            if (Value == value)
                return this;
            return new MiniYaml(value, Nodes);
        }

        public MiniYaml WithNodes(IEnumerable<MiniYamlNode> nodes)
        {
            if (nodes is ImmutableArray<MiniYamlNode> n && Nodes == n)
                return this;
            return new MiniYaml(Value, nodes);
        }

        public MiniYaml WithNodesAppended(IEnumerable<MiniYamlNode> nodes)
        {
            var newNodes = Nodes.AddRange(nodes);
            if (Nodes == newNodes)
                return this;
            return new MiniYaml(Value, newNodes);
        }

        public MiniYamlNode NodeWithKey(string key)
        {
            var result = NodeWithKeyOrDefault(key);
            if (result == null)
                throw new InvalidDataException($"No node with key '{key}'");
            return result;
        }

        public MiniYamlNode NodeWithKeyOrDefault(string key)
        {
            // PERF: Avoid LINQ.
            var first = true;
            MiniYamlNode result = null;
            foreach (var node in Nodes)
            {
                if (node.Key != key)
                    continue;

                if (!first)
                    throw new InvalidDataException($"Duplicate key '{node.Key}' in {node.Location}");

                first = false;
                result = node;
            }

            return result;
        }

        public Dictionary<string, MiniYaml> ToDictionary()
        {
            return ToDictionary(MiniYamlIdentity);
        }

        public Dictionary<string, TElement> ToDictionary<TElement>(Func<MiniYaml, TElement> elementSelector)
        {
            return ToDictionary(StringIdentity, elementSelector);
        }

        public Dictionary<TKey, TElement> ToDictionary<TKey, TElement>(
            Func<string, TKey> keySelector, Func<MiniYaml, TElement> elementSelector)
        {
            var ret = new Dictionary<TKey, TElement>(Nodes.Length);
            foreach (var y in Nodes)
            {
                var key = keySelector(y.Key);
                var element = elementSelector(y.Value);
                if (!ret.TryAdd(key, element))
                    throw new InvalidDataException($"Duplicate key '{y.Key}' in {y.Location}");
            }

            return ret;
        }

        public MiniYaml(string value)
            : this(value, []) { }

        public MiniYaml(string value, IEnumerable<MiniYamlNode> nodes)
        {
            Value = value;
            Nodes = nodes.ToImmutableArray();
        }

        static IEnumerable<MiniYamlNode> FromLines(
            IEnumerable<ReadOnlyMemory<char>> lines, string name, bool discardCommentsAndWhitespace, HashSet<string> stringPool)
        {
            // YAML config often contains repeated strings for key, values, comments.
            // Pool these strings so we only need one copy of each unique string.
            // This saves on long-term memory usage as parsed values can often live a long time.
            // A caller can also provide a pool as input, allowing de-duplication across multiple parses.
            stringPool ??= [];
            var stringPoolLookup = stringPool.GetAlternateLookup<ReadOnlySpan<char>>();

            var result = new List<List<MiniYamlNode>>
            {
                new()
            };
            var parsedLines = new List<(int Level, string Key, string Value, string Comment, MiniYamlNode.SourceLocation Location)>();

            var lineNo = 0;
            foreach (var ll in lines)
            {
                var line = ll.Span;
                ++lineNo;

                var keyStart = 0;
                var level = 0;
                var spaces = 0;
                var textStart = false;

                ReadOnlySpan<char> key = default;
                ReadOnlySpan<char> value = default;
                ReadOnlySpan<char> comment = default;
                var location = new MiniYamlNode.SourceLocation(name, lineNo);

                if (line.Length > 0)
                {
                    var currChar = line[keyStart];

                    while (!(currChar == '\n' || currChar == '\r') && keyStart < line.Length && !textStart)
                    {
                        currChar = line[keyStart];
                        switch (currChar)
                        {
                            case ' ':
                                spaces++;
                                if (spaces >= SpacesPerLevel)
                                {
                                    spaces = 0;
                                    level++;
                                }

                                keyStart++;
                                break;
                            case '\t':
                                level++;
                                keyStart++;
                                break;
                            default:
                                textStart = true;
                                break;
                        }
                    }

                    // Extract key, value, comment from line as `<key>: <value>#<comment>`
                    // The # character is allowed in the value if escaped (\#).
                    // Leading and trailing whitespace is always trimmed from keys.
                    // Leading and trailing whitespace is trimmed from values unless they
                    // are marked with leading or trailing backslashes
                    var keyLength = line.Length - keyStart;
                    var valueStart = -1;
                    var valueLength = 0;
                    var commentStart = -1;
                    for (var i = 0; i < line.Length; i++)
                    {
                        if (valueStart < 0 && line[i] == ':')
                        {
                            valueStart = i + 1;
                            keyLength = i - keyStart;
                            valueLength = line.Length - i - 1;
                        }

                        if (commentStart < 0 && line[i] == '#' && (i == 0 || line[i - 1] != '\\'))
                        {
                            commentStart = i + 1;
                            if (i <= keyStart + keyLength)
                                keyLength = i - keyStart;
                            else
                                valueLength = i - valueStart;

                            break;
                        }
                    }

                    if (keyLength > 0)
                        key = line.Slice(keyStart, keyLength).Trim();

                    if (valueStart >= 0)
                    {
                        var trimmed = line.Slice(valueStart, valueLength).Trim();
                        if (trimmed.Length > 0)
                            value = trimmed;
                    }

                    if (commentStart >= 0 && !discardCommentsAndWhitespace)
                        comment = line[commentStart..];

                    if (value.Length > 1)
                    {
                        // Remove leading/trailing whitespace guards
                        var trimLeading = value[0] == '\\' && (value[1] == ' ' || value[1] == '\t') ? 1 : 0;
                        var trimTrailing = value[^1] == '\\' && (value[^2] == ' ' || value[^2] == '\t') ? 1 : 0;
                        if (trimLeading + trimTrailing > 0)
                            value = value.Slice(trimLeading, value.Length - trimLeading - trimTrailing);

                        // Remove escape characters from #
                        if (value.Contains("\\#", StringComparison.Ordinal))
                            value = value.ToString().Replace("\\#", "#");
                    }
                }

                if (!key.IsEmpty || !discardCommentsAndWhitespace)
                {
                    if (parsedLines.Count > 0 && parsedLines[^1].Level < level - 1)
                        throw new YamlException($"Bad indent in miniyaml at {location}");

                    while (parsedLines.Count > 0 && parsedLines[^1].Level > level)
                        BuildCompletedSubNode(level);

                    string GetOrAdd(ReadOnlySpan<char> value)
                    {
                        if (stringPoolLookup.TryGetValue(value, out var result))
                            return result;
                        stringPool.Add(result = value.ToString());
                        return result;
                    }

                    var keyString = key.IsEmpty ? null : GetOrAdd(key);
                    var valueString = value.IsEmpty ? null : GetOrAdd(value);

                    // Note: We need to support empty comments here to ensure that empty comments
                    // (i.e. a lone # at the end of a line) can be correctly re-serialized
                    var commentString = comment == ReadOnlySpan<char>.Empty ? null : GetOrAdd(comment);

                    parsedLines.Add((level, keyString, valueString, commentString, location));
                }

                foreach (var topLevelNode in result[0])
                    yield return topLevelNode;
                result[0].Clear();
            }

            if (parsedLines.Count > 0)
            {
                BuildCompletedSubNode(0);
                foreach (var topLevelNode in result[0])
                    yield return topLevelNode;
                result[0].Clear();
            }

            void BuildCompletedSubNode(int level)
            {
                var lastLevel = parsedLines[^1].Level;
                while (lastLevel >= result.Count)
                    result.Add([]);

                while (parsedLines.Count > 0 && parsedLines[^1].Level >= level)
                {
                    var parent = parsedLines[^1];
                    var startOfRange = parsedLines.Count - 1;
                    while (startOfRange > 0 && parsedLines[startOfRange - 1].Level == parent.Level)
                        startOfRange--;

                    for (var i = startOfRange; i < parsedLines.Count - 1; i++)
                    {
                        var sibling = parsedLines[i];
                        result[parent.Level].Add(
                            new MiniYamlNode(sibling.Key, new MiniYaml(sibling.Value), sibling.Comment, sibling.Location));
                    }

                    var childNodes = parent.Level + 1 < result.Count ? result[parent.Level + 1] : null;
                    result[parent.Level].Add(new MiniYamlNode(
                        parent.Key,
                        new MiniYaml(parent.Value, childNodes ?? Enumerable.Empty<MiniYamlNode>()),
                        parent.Comment,
                        parent.Location));
                    childNodes?.Clear();

                    parsedLines.RemoveRange(startOfRange, parsedLines.Count - startOfRange);
                }
            }
        }

        public static IEnumerable<MiniYamlNode> FromFile(string path, bool discardCommentsAndWhitespace = true, HashSet<string> stringPool = null)
        {
            return FromStream(File.OpenRead(path), path, discardCommentsAndWhitespace, stringPool);
        }

        public static IEnumerable<MiniYamlNode> FromStream(Stream s, string name, bool discardCommentsAndWhitespace = true, HashSet<string> stringPool = null)
        {
            return FromLines(s.ReadAllLinesAsMemory(), name, discardCommentsAndWhitespace, stringPool);
        }

        public static IEnumerable<MiniYamlNode> FromString(string text, string name, bool discardCommentsAndWhitespace = true, HashSet<string> stringPool = null)
        {
            return FromLines(text.Split(["\r\n", "\n"], StringSplitOptions.None).Select(s => s.AsMemory()), name, discardCommentsAndWhitespace, stringPool);
        }

        public static List<MiniYamlNode> Merge(IEnumerable<IEnumerable<MiniYamlNode>> sources)
        {
            var sourcesList = sources.ToList();
            if (sourcesList.Count == 0)
                return [];

            var tree = sourcesList
                .Where(s => s != null)
                .Select(s => s as IReadOnlyCollection<MiniYamlNode> ?? s.ToList())
                .Select(MergeSelfPartial)
                .Aggregate(MergePartial)
                .Where(n => n.Key != null)
                .ToDictionary(n => n.Key, n => n.Value);

            var resolved = new Dictionary<string, MiniYaml>(tree.Count);
            foreach (var kv in tree)
            {
                // Inheritance is tracked from parent->child, but not from child->parentsiblings.
                var inherited = ImmutableDictionary<string, MiniYamlNode.SourceLocation>.Empty.Add(kv.Key, default);
                var children = ResolveInherits(kv.Value, tree, inherited);
                resolved.Add(kv.Key, new MiniYaml(kv.Value.Value, children));
            }

            // Resolve any top-level removals (e.g. removing whole actor blocks)
            var nodes = new MiniYaml("", resolved.Select(kv => new MiniYamlNode(kv.Key, kv.Value)));
            var result = ResolveInherits(nodes, tree, []);
            return result as List<MiniYamlNode> ?? result.ToList();
        }

        static void MergeIntoResolved(MiniYamlNode overrideNode, List<MiniYamlNode> existingNodes, HashSet<string> existingNodeKeys,
            Dictionary<string, MiniYaml> tree, ImmutableDictionary<string, MiniYamlNode.SourceLocation> inherited)
        {
            var existingNodeIndex = -1;
            MiniYamlNode existingNode = null;
            if (!existingNodeKeys.Add(overrideNode.Key))
            {
                existingNodeIndex = IndexOfKey(existingNodes, overrideNode.Key);
                existingNode = existingNodes[existingNodeIndex];
            }

            var value = MergePartial(existingNode?.Value, overrideNode.Value);
            var nodes = ResolveInherits(value, tree, inherited);
            if (!value.Nodes.SequenceEqual(nodes))
                value = value.WithNodes(nodes);

            if (existingNode != null)
                existingNodes[existingNodeIndex] = existingNode.WithValue(value);
            else
                existingNodes.Add(overrideNode.WithValue(value));
        }

        static IReadOnlyCollection<MiniYamlNode> ResolveInherits(
            MiniYaml node, Dictionary<string, MiniYaml> tree, ImmutableDictionary<string, MiniYamlNode.SourceLocation> inherited)
        {
            if (node.Nodes.Length == 0)
                return node.Nodes;

            var resolved = new List<MiniYamlNode>(node.Nodes.Length);
            var resolvedKeys = new HashSet<string>(node.Nodes.Length);

            foreach (var n in node.Nodes)
            {
                if (n.Key == "Inherits" || n.Key.StartsWith("Inherits@", StringComparison.Ordinal))
                {
                    if (!tree.TryGetValue(n.Value.Value, out var parent))
                        throw new YamlException(
                            $"{n.Location}: Parent type `{n.Value.Value}` not found");

                    try
                    {
                        inherited = inherited.Add(n.Value.Value, n.Location);
                    }
                    catch (ArgumentException)
                    {
                        throw new YamlException(
                            $"{n.Location}: Parent type `{n.Value.Value}` was already inherited by this yaml tree at {inherited[n.Value.Value]} (note: may be from a derived tree)");
                    }

                    foreach (var r in ResolveInherits(parent, tree, inherited))
                        MergeIntoResolved(r, resolved, resolvedKeys, tree, inherited);
                }
                else if (n.Key.StartsWith('-'))
                {
                    var removed = n.Key[1..];
                    if (resolved.RemoveAll(r => r.Key == removed) == 0)
                        throw new YamlException($"{n.Location}: There are no elements with key `{removed}` to remove");
                    resolvedKeys.Remove(removed);
                }
                else
                    MergeIntoResolved(n, resolved, resolvedKeys, tree, inherited);
            }

            return resolved;
        }

        /// <summary>
        /// Merges any duplicate keys that are defined within the same set of nodes.
        /// Does not resolve inheritance or node removals.
        /// </summary>
        static IReadOnlyCollection<MiniYamlNode> MergeSelfPartial(IReadOnlyCollection<MiniYamlNode> existingNodes)
        {
            if (existingNodes.Count == 0)
                return existingNodes;

            var keys = new HashSet<string>(existingNodes.Count);
            var ret = new List<MiniYamlNode>(existingNodes.Count);
            foreach (var n in existingNodes)
            {
                if (n.Key == null)
                    continue;

                if (keys.Add(n.Key))
                    ret.Add(n);
                else
                {
                    // Node with the same key has already been added: merge new node over the existing one
                    var originalIndex = IndexOfKey(ret, n.Key);
                    var original = ret[originalIndex];
                    ret[originalIndex] = original.WithValue(MergePartial(original.Value, n.Value));
                }
            }

            return ret;
        }

        static IReadOnlyList<MiniYamlNode> WeakResolveRemovals(IReadOnlyList<MiniYamlNode> nodes)
        {
            if (nodes == null || nodes.Count == 0)
                return nodes;

            List<MiniYamlNode> ret = null;
            for (var i = 0; i < nodes.Count; i++)
            {
                var node = nodes[i];
                if (node.Key == null)
                    continue;

                if (node.Key.StartsWith('-'))
                {
                    if (ret == null)
                    {
                        ret ??= new List<MiniYamlNode>(nodes.Count);
                        ret.AddRange(nodes.Take(i));
                    }

                    // Apply the removal node - but "weakly" - don't throw if there is no prior node to remove.
                    var removed = node.Key[1..];
                    ret.RemoveAll(r => r.Key == removed);
                }
                else
                {
                    ret?.Add(node);
                }
            }

            return ret ?? nodes;
        }

        static MiniYaml MergePartial(MiniYaml existingNodes, MiniYaml overrideNodes)
        {
            var resolvedExistingNodes = WeakResolveRemovals(existingNodes?.Nodes);
            var resolvedOverrideNodes = WeakResolveRemovals(overrideNodes?.Nodes);

            lock (ConflictScratchLock)
            {
                try
                {
                    // PERF: Reuse ConflictScratch for all conflict checks to avoid allocations.
                    resolvedExistingNodes?.IntoDictionaryWithConflictLog(
                        n => n.Key, n => n, "MiniYaml.Merge", ConflictScratch, k => k, n => $"{n.Key} (at {n.Location})");
                    resolvedOverrideNodes?.IntoDictionaryWithConflictLog(
                        n => n.Key, n => n, "MiniYaml.Merge", ConflictScratch, k => k, n => $"{n.Key} (at {n.Location})");
                    ConflictScratch.Clear();
                }
                catch (ArgumentException ex)
                {
                    throw new YamlException(ex.Message);
                }
            }

            if (existingNodes == null)
                return overrideNodes;

            if (overrideNodes == null)
                return existingNodes;

            return new MiniYaml(overrideNodes.Value ?? existingNodes.Value, MergePartial(existingNodes.Nodes, overrideNodes.Nodes));
        }

        static IReadOnlyCollection<MiniYamlNode> MergePartial(IReadOnlyCollection<MiniYamlNode> existingNodes, IReadOnlyCollection<MiniYamlNode> overrideNodes)
        {
            if (existingNodes.Count == 0)
                return overrideNodes;

            if (overrideNodes.Count == 0)
                return existingNodes;

            var ret = new List<MiniYamlNode>(existingNodes.Count + overrideNodes.Count);
            var plainKeys = new HashSet<string>(existingNodes.Count + overrideNodes.Count);

            foreach (var node in existingNodes)
                MergeNode(node);
            foreach (var node in overrideNodes)
                MergeNode(node);

            void MergeNode(MiniYamlNode node)
            {
                if (node.Key == null)
                    return;

                // Append Removal nodes to the result.
                // Therefore: we know the remainder of the method deals with a plain node.
                if (node.Key.StartsWith('-'))
                {
                    ret.Add(node);
                    return;
                }

                // If no previous node with this key is present, it is new and can just be appended.
                if (plainKeys.Add(node.Key))
                {
                    ret.Add(node);
                    return;
                }

                // A Removal node is closer than the previous node.
                // We should not merge the new node, as the data being merged will jump before the Removal.
                // Instead, append it so the previous node is applied, then removed, then the new node is applied.
                var previousNodeIndex = LastIndexOfKey(ret, node.Key);
                var previousRemovalNodeIndex = LastIndexOfKey(ret, $"-{node.Key}");
                if (previousRemovalNodeIndex != -1 && previousRemovalNodeIndex > previousNodeIndex)
                {
                    ret.Add(node);
                    return;
                }

                // A previous node is present with no intervening Removal.
                // We should merge the new one into it, in place.
                ret[previousNodeIndex] = node.WithValue(MergePartial(ret[previousNodeIndex].Value, node.Value));
            }

            return ret;
        }

        static int IndexOfKey(List<MiniYamlNode> nodes, string key)
        {
            // PERF: Avoid LINQ.
            for (var i = 0; i < nodes.Count; i++)
                if (nodes[i].Key == key)
                    return i;
            return -1;
        }

        static int LastIndexOfKey(List<MiniYamlNode> nodes, string key)
        {
            // PERF: Avoid LINQ.
            for (var i = nodes.Count - 1; i >= 0; i--)
                if (nodes[i].Key == key)
                    return i;
            return -1;
        }

        public IEnumerable<string> ToLines(string key, string comment = null)
        {
            var hasKey = !string.IsNullOrEmpty(key);
            var hasValue = !string.IsNullOrEmpty(Value);
            var hasComment = comment != null;
            yield return (hasKey ? key + ":" : "")
                + (hasValue ? " " + Value.Replace("#", "\\#") : "")
                + (hasComment ? (hasKey || hasValue ? " " : "") + "#" + comment : "");

            if (Nodes != null)
                foreach (var line in Nodes.ToLines())
                    yield return "\t" + line;
        }

        // Load(MiniYaml.cs L684-698)依赖 IReadOnlyFileSystem/FieldLoader,不入本
        // oracle —— 由 C++ 侧集成测试(Phase 1 后半 FileSystem 就位后)覆盖
    }

    public sealed class MiniYamlNodeBuilder
    {
        public MiniYamlNode.SourceLocation Location;
        public string Key;
        public MiniYamlBuilder Value;
        public string Comment;

        public MiniYamlNodeBuilder(MiniYamlNode node)
        {
            Location = node.Location;
            Key = node.Key;
            Value = new MiniYamlBuilder(node.Value);
            Comment = node.Comment;
        }

        public MiniYamlNodeBuilder(string k, MiniYamlBuilder v, string c = null)
        {
            Key = k;
            Value = v;
            Comment = c;
        }

        public MiniYamlNodeBuilder(string k, MiniYamlBuilder v, string c, MiniYamlNode.SourceLocation loc)
            : this(k, v, c)
        {
            Location = loc;
        }

        public MiniYamlNodeBuilder(string k, string v, string c = null)
            : this(k, new MiniYamlBuilder(v, null), c) { }

        public MiniYamlNodeBuilder(string k, string v, List<MiniYamlNode> n)
            : this(k, new MiniYamlBuilder(v, n), null) { }

        public MiniYamlNode Build()
        {
            return new MiniYamlNode(Key, Value.Build(), Comment, Location);
        }
    }

    public sealed class MiniYamlBuilder
    {
        public string Value;
        public List<MiniYamlNodeBuilder> Nodes;

        public MiniYamlBuilder(MiniYaml yaml)
        {
            Value = yaml.Value;
            Nodes = yaml.Nodes.Select(n => new MiniYamlNodeBuilder(n)).ToList();
        }

        public MiniYamlBuilder(string value)
            : this(value, null) { }

        public MiniYamlBuilder(string value, List<MiniYamlNode> nodes)
        {
            Value = value;
            Nodes = nodes == null ? [] : nodes.ConvertAll(x => new MiniYamlNodeBuilder(x));
        }

        public MiniYaml Build()
        {
            return new MiniYaml(Value, Nodes.Select(n => n.Build()));
        }

        public IEnumerable<string> ToLines(string key, string comment = null)
        {
            var hasKey = !string.IsNullOrEmpty(key);
            var hasValue = !string.IsNullOrEmpty(Value);
            var hasComment = comment != null;
            yield return (hasKey ? key + ":" : "")
                + (hasValue ? " " + Value.Replace("#", "\\#") : "")
                + (hasComment ? (hasKey || hasValue ? " " : "") + "#" + comment : "");

            if (Nodes != null)
                foreach (var line in Nodes.ToLines())
                    yield return "\t" + line;
        }

        public MiniYamlNodeBuilder NodeWithKeyOrDefault(string key)
        {
            return Nodes.SingleOrDefault(n => n.Key == key);
        }
    }

    public class YamlException : Exception
    {
        public YamlException(string s)
            : base(s) { }
    }
}

#endregion
