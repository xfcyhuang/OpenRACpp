// golden-gen —— 无头 oracle 宿主（工单 #3 / T02；PRNG 模式源自 T03）
//
// 职责：ProjectReference 上游基线源码（OpenRA.Game）构建，进程内驱动上游真源产出
// 黄金基准产物，供 C++ 侧移植黄金对拍（入库字节 = 判定真值）。动作：
//   gen     重产基准产物并写入库位置（tests/golden/**）+ .sha256 旁车（sha256sum -c 兼容）
//   verify  重产并与入库黄金逐字节比对，退出码供脚本/ctest 判定
//   check   验收复验：每产物独立重产两次断言字节恒等（SHA256 一致），再与入库黄金比对
//
// 产物模式（artifacts 清单登记，随工单扩展）：
//   prng            上游 OpenRA.Support.MersenneTwister 序列 dump（7 种子 × 10 kind，T03 格式）
//   resolved-rules  上游 IUtilityCommand --resolved-rules ra:BADR.Bomber——覆盖 MiniYaml
//                   解析、Inherits/Inherits@ 继承展开与多文件合并语义
//   rules-stats     上游 Manifest.Rules 提取 + MiniYaml.Load 全量合并树（T05/issue #6）——
//                   ra-manifest.txt（16 文件有序清单）、ra-merged.yaml（全量合并树，
//                   Merge 输出序、WriteToString 序列化）、ra-stats.txt（计数头 + 逐 actor
//                   trait 计数）。轻路径：物理 IReadOnlyFileSystem + MiniYaml.Load 直驱，
//                   不装配 ModData/IUtilityCommand（无需 Game.ModData hack）。
//   map-info        无头 new Map(modData, oramap 包) 全量加载后的字段+图块直方图转储
//                   （T06 格式，tests/golden/maps/*.mapinfo.txt）；同批把 .oramap 与
//                   tileset yaml 原样从上游检出复制入库（基线漂移时 check 报警）
//
// 每次动作前断言上游检出 HEAD == UPSTREAM.baseline 首行，防基准漂移。
// resolved-rules 产物字节为上游命令 stdout 的原样捕获（UTF-8 无 BOM；末尾行尾来自
// Console.WriteLine，Windows 上为 CR LF）——黄金锚定 Windows 上游 stdout 语义，不做任何规范化。
//
// 退出码：0 成功 / 1 校验失败（含 check 双跑不恒等）/ 2 基线漂移 / 3 宿主错误；
//         77 前置缺失（dotnet/上游检出不在）由 run.cmd 判定，供 ctest 记为 SKIP。

using System.Diagnostics;
using System.Globalization;
using System.Reflection;
using System.Runtime.Serialization;
using System.Security.Cryptography;
using System.Text;
using OpenRA;
using OpenRA.FileSystem;
using OpenRA.Support;
using OpenRA.Traits;

// FormatterServices 仅为跳过 Actor ctor 伪造订单黄金（U029 配方），无序列化用途
#pragma warning disable SYSLIB0050

const int ExitOk = 0;
const int ExitVerifyFail = 1;   // verify/check：黄金缺失、字节不一致或双跑不恒等
const int ExitBaseline = 2;     // 上游检出与 UPSTREAM.baseline 不符（基准漂移）
const int ExitError = 3;        // 参数/构建/运行等其他宿主错误

try
{
    return Run(args);
}
catch (BaselineMismatchException e)
{
    return Fail(ExitBaseline, e.Message);
}
catch (Exception e)
{
    return Fail(ExitError, $"{e.GetType().Name}: {e.Message}");
}
finally
{
    // 必修：上游 Log 静态构造（OpenRA.Game/Support/Log.cs:49-54）启动的是前台线程，
    // 不 Dispose 进程永不退出——参照上游 OpenRA.Utility/Program.cs Main 的 finally 收尾。
    Log.Dispose();
}

int Run(string[] args)
{
    // —— 参数解析（run.cmd 会显式传入两个根路径） ——
    string verb = null;
    string repoRootArg = null;
    string openraArg = null;
    for (var i = 0; i < args.Length; i++)
    {
        switch (args[i])
        {
            case "--repo-root": repoRootArg = NextArg(args, ref i); break;
            case "--openra": openraArg = NextArg(args, ref i); break;
            default:
                if (verb == null && args[i] is "gen" or "verify" or "check") verb = args[i];
                else throw new ArgumentException(
                    $"未知参数或动作：{args[i]}（用法：golden-gen gen|verify|check [--repo-root DIR] [--openra DIR]）");
                break;
        }
    }

    if (verb == null)
        throw new ArgumentException("缺少动作。用法：golden-gen gen|verify|check [--repo-root DIR] [--openra DIR]");

    // —— 路径解析 ——
    var repoRoot = ResolveRepoRoot(repoRootArg);
    var openraRoot = ResolveOpenRaRoot(openraArg, repoRoot);

    // —— 基线断言：上游检出必须钉在 UPSTREAM.baseline 记录的 commit ——
    AssertUpstreamBaseline(openraRoot, repoRoot);

    // 注：宿主不在此整建上游 slnx——宿主 exe 与其加载的 OpenRA.Game.dll 都在
    // openra/bin，进程运行中锁文件，进程内再构建同一输出目录会自锁
    // （MSB3026 复制失败重试耗尽）。上游构建职责在 run.cmd：先建 slnx 再建宿主，
    // 宿主启动后对 mod 程序集做存在性自检（见 DumpResolvedRules）。

    // —— 基准产物清单（后续 yaml-roundtrip / 真回放等模式在此追加） ——
    (string RelPath, Func<byte[]> Produce)[] artifacts =
    [
        .. State.PrngSeeds.Select(seed => (
            $"tests/golden/prng/seed_{seed.ToString(CultureInfo.InvariantCulture)}.txt",
            (Func<byte[]>)(() => DumpPrngSequence(seed)))),
        ("tests/golden/resolved_rules/BADR.Bomber.yaml",
            () => DumpResolvedRules(openraRoot, "ra", "BADR.Bomber")),
        ("tests/golden/rules/ra-manifest.txt", () => DumpRaManifest(openraRoot)),
        ("tests/golden/rules/ra-merged.yaml", () => DumpRaMerged(openraRoot)),
        ("tests/golden/rules/ra-stats.txt", () => DumpRaStats(openraRoot)),
        .. State.MapNames.Select(mapName => (
            $"tests/golden/maps/{mapName}.oramap",
            (Func<byte[]>)(() => File.ReadAllBytes(
                Path.Combine(openraRoot, "mods", "ra", "maps", mapName + ".oramap"))))),
        .. State.MapNames.Select(mapName => (
            $"tests/golden/maps/{mapName}.mapinfo.txt",
            (Func<byte[]>)(() => DumpMapInfo(openraRoot, "ra", mapName)))),
        ("tests/golden/maps/tilesets/temperat.yaml",
            () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "tilesets", "temperat.yaml"))),
        ("tests/golden/maps/tilesets/snow.yaml",
            () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "tilesets", "snow.yaml"))),
        ("tests/golden/orders/representative.txt", () => DumpOrders()),
    ];

    var failures = 0;
    foreach (var (relPath, produce) in artifacts)
    {
        var artifactPath = Path.Combine(repoRoot, relPath.Replace('/', Path.DirectorySeparatorChar));

        if (verb == "check")
        {
            // 验收语义：独立重产两次，字节必须恒等（真双跑，非同义反复）
            var first = produce();
            var second = produce();
            if (!first.AsSpan().SequenceEqual(second))
            {
                failures++;
                Console.WriteLine($"check: {relPath} FAIL —— 连续两次产出字节不一致" +
                    $"（{Sha256Hex(first)} vs {Sha256Hex(second)}）");
                continue;
            }

            Console.WriteLine($"check: {relPath} 双跑恒等 sha256={Sha256Hex(first)} bytes={first.Length}");
            if (!VerifyGolden(artifactPath, relPath, first))
                failures++;
        }
        else
        {
            var bytes = produce();
            if (verb == "gen")
                WriteGolden(artifactPath, relPath, bytes);
            else if (!VerifyGolden(artifactPath, relPath, bytes))
                failures++;
        }
    }

    if (failures > 0)
        return Fail(ExitVerifyFail, $"{failures} 个黄金产物校验失败");

    var summary = verb switch
    {
        "gen" => $"gen: {artifacts.Length} 个黄金产物已写入",
        "check" => $"check: {artifacts.Length} 个产物双跑恒等且与入库黄金一致",
        _ => "verify: 全部通过",
    };
    Console.WriteLine(summary);
    return ExitOk;
}

// —— prng 模式：驱动上游 MersenneTwister 真源，格式与 T03 入库文件一致
//    （首部注释即格式说明，C++ 侧 tests/prng.cpp 按此解析） ——

static byte[] DumpPrngSequence(int seed)
{
    var sb = new StringBuilder();
    sb.Append("# seed ").Append(seed).Append('\n');
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 OpenRA.Game/Support/MersenneTwister.cs").Append('\n');
    sb.Append("# u32 x1000 NextUint hex | next x200 Next dec | f32 x200 NextFloat ieee754 bits hex | u64 x100 NextUlong hex").Append('\n');
    sb.Append("# range x200 Next(2,10) dec | rangen x100 Next(-5,5) dec | pw x100 PickWeighted[1,3,6] dec | pw0 x100 PickWeighted[0,0,0] dec").Append('\n');
    sb.Append("# shuffle x100 ShuffleInPlace[10..14] | u32_624 NextUint #624..#626 hex (after 623 NextUint)").Append('\n');

    void Line(string kind, string value)
    {
        sb.Append(kind).Append(' ').Append(value).Append('\n');
    }

    var m1 = new MersenneTwister(seed);
    for (var i = 0; i < 1000; i++)
        Line("u32", m1.NextUint().ToString("x8"));

    var m2 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("next", m2.Next().ToString(CultureInfo.InvariantCulture));

    var m3 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("f32", BitConverter.SingleToUInt32Bits(m3.NextFloat()).ToString("x8"));

    var m4 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("u64", m4.NextUlong().ToString("x16"));

    var m5 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("range", m5.Next(2, 10).ToString(CultureInfo.InvariantCulture));

    var m5n = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("rangen", m5n.Next(-5, 5).ToString(CultureInfo.InvariantCulture));

    var m6 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("pw", m6.PickWeighted([1, 3, 6]).ToString(CultureInfo.InvariantCulture));

    var m6z = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("pw0", m6z.PickWeighted([0, 0, 0]).ToString(CultureInfo.InvariantCulture));

    var m7 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
    {
        var span = new int[] { 10, 11, 12, 13, 14 };
        m7.ShuffleInPlace(span.AsSpan(), 0, span.Length);
        Line("shuffle", string.Join(' ', span));
    }

    var m8 = new MersenneTwister(seed);
    for (var i = 0; i < 623; i++)
        m8.NextUint();
    Line("u32_624", $"{m8.NextUint():x8} {m8.NextUint():x8} {m8.NextUint():x8}");

    // 与 T03 落盘编码一致：UTF-8 无 BOM，LF 行尾
    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

// —— resolved-rules 模式：镜像上游 OpenRA.Utility/Program.cs:55-99 的无头装配，
//    命令体（--resolved-rules）经 ObjectCreator 自 openra/bin 的上游程序集加载，
//    oracle 纯度 = 产物全部来自上游代码 ——
static byte[] DumpResolvedRules(string openraRoot, string modId, string actorKey)
{
    // 前置自检：ra 的 manifest 声明程序集（OpenRA.Mods.Common/Cnc.dll）须已在
    // openra/bin——由 run.cmd 在宿主启动前整建 slnx。缺失时报可操作错误而非
    // 让 ObjectCreator 在装配深处抛难解的 FileNotFoundException。
    foreach (var assembly in new[] { "OpenRA.Mods.Common.dll", "OpenRA.Mods.Cnc.dll" })
        if (!File.Exists(Path.Combine(Platform.BinDir, assembly)))
            throw new InvalidOperationException(
                $"openra/bin 缺少 {assembly}（BinDir={Platform.BinDir}）。" +
                "先运行 tools/golden_gen/run.cmd，或手动执行 " +
                $"dotnet build {Path.Combine(openraRoot, "OpenRA.slnx")} -c Release 后重试。");

    // 与上游同序：先覆写引擎目录（须在任何 EngineDir 访问前）。上游 Platform
    // 对二次 OverrideEngineDir 直接 throw，check 双跑时只覆写一次。
    if (!State.EngineDirOverridden)
    {
        Platform.OverrideEngineDir(openraRoot);
        State.EngineDirOverridden = true;
    }

    // AddChannel 幂等（上游 Log.cs:130-131），重复装配时安全
    Log.AddChannel("perf", null);
    Log.AddChannel("debug", null);
    Game.InitializeSettings(Arguments.Empty);

    var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
    if (!mods.ContainsKey(modId))
        throw new InvalidOperationException($"mod '{modId}' 不在可用列表：{string.Join(", ", mods.Keys)}");

    var modData = new ModData(mods[modId], mods);
    var utility = new Utility(modData, mods);

    var command = modData.ObjectCreator.GetTypesImplementing<IUtilityCommand>()
        .Select(t => (IUtilityCommand)Activator.CreateInstance(t))
        .FirstOrDefault(c => c.Name == "--resolved-rules")
        ?? throw new InvalidOperationException("上游程序集中未找到 --resolved-rules 命令");

    var commandArgs = new[] { "--resolved-rules", actorKey };
    if (!command.ValidateArguments(commandArgs))
        throw new InvalidOperationException($"--resolved-rules 参数不合法：{string.Join(" ", commandArgs)}");

    // 捕获上游命令的 stdout 作为产物字节（UTF-8、无 BOM），宿主自身日志不混入
    var captured = new StringWriter();
    var oldOut = Console.Out;
    Console.SetOut(captured);
    try
    {
        command.Run(utility, commandArgs);
    }
    finally
    {
        Console.SetOut(oldOut);
    }

    return Encoding.UTF8.GetBytes(captured.ToString());
}

// —— rules-stats 模式（T05/issue #6）：轻路径直驱上游 Manifest/MiniYaml 真源 ——
//    复刻 Manifest.cs 的 Rules 提取（FromStream → Merge([nodes]) → ToDictionary → YamlList）
//    与 MiniYaml.Load 全量合并管线；物理 IReadOnlyFileSystem 映射 ra|x → <root>/mods/ra/x。
//    产物顺序一律用上游输出序（Merge 输出 = 文件清单序 + 首现序，实测双跑恒等），勿排序。

static (List<string> Rules, List<MiniYamlNode> Merged) LoadRaRules(string openraRoot)
{
    var fs = new GoldenModRootFs(openraRoot);

    // Manifest.cs:97-124 路径（ra 的 mod.yaml 无 Include，该展开分支不触发）
    var nodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
    var yaml = new MiniYaml(null, MiniYaml.Merge([nodes])).ToDictionary();
    var rules = yaml["Rules"].Nodes.Select(n => n.Key).ToList();

    var merged = MiniYaml.Load(fs, rules, null);
    return (rules, merged.ToList());
}

static byte[] DumpRaManifest(string openraRoot)
{
    var (rules, _) = LoadRaRules(openraRoot);
    var sb = new StringBuilder();
    foreach (var r in rules)
        sb.Append(r).Append('\n');
    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

static byte[] DumpRaMerged(string openraRoot)
{
    var (_, merged) = LoadRaRules(openraRoot);
    // WriteToString（MiniYamlExts）：每顶层 key: + 子行 TAB 缩进，LF、无 BOM；
    // 与 resolved-rules 单 actor 产物格式同构（本产物是全集超集）
    return new UTF8Encoding(false).GetBytes(merged.WriteToString());
}

static byte[] DumpRaStats(string openraRoot)
{
    var (_, merged) = LoadRaRules(openraRoot);
    var actors = merged.Where(n => !n.Key.StartsWith('^')).ToList();
    var traits = actors.Sum(a => a.Value.Nodes.Length);
    var abstractCount = merged.Count - actors.Count;

    var sb = new StringBuilder();
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 ra rules (manifest 16 files)").Append('\n');
    sb.Append("# actors ").Append(actors.Count).Append('\n');
    sb.Append("# traits ").Append(traits).Append('\n');
    sb.Append("# abstract ").Append(abstractCount).Append('\n');
    foreach (var a in actors)
        sb.Append(a.Key).Append(' ').Append(a.Value.Nodes.Length).Append('\n');
    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

// —— map-info 模式（T06）：无头 new Map(modData, oramap 包) 全量加载后转储关键字段
//    与图块/资源直方图。蓝本 = 调研探针（.ora-explore/t06-scratch，5 图双跑恒等已实测）。
//    禁忌：宿主只 ProjectReference OpenRA.Game——加 Mods.Common 会因 ObjectCreator 自定义
//    ALC 双加载造成 trait 类型身份分裂，new Map 抛与真因无关的假错（调研 §0-6）。
//    地形信息只经 map.Rules.TerrainInfo（ITerrainInfo 在 OpenRA.Game）。
//    bounds 打印宽高形 Left,Top,Width,Height（与 map.yaml 原文同形；探针初版的
//    Right/Bottom 形已废弃——转储格式一经入库不得再改）。 ——
static byte[] DumpMapInfo(string openraRoot, string modId, string mapName)
{
    foreach (var assembly in new[] { "OpenRA.Mods.Common.dll", "OpenRA.Mods.Cnc.dll" })
        if (!File.Exists(Path.Combine(Platform.BinDir, assembly)))
            throw new InvalidOperationException(
                $"openra/bin 缺少 {assembly}（BinDir={Platform.BinDir}）。" +
                "先运行 tools/golden_gen/run.cmd，或手动执行 " +
                $"dotnet build {Path.Combine(openraRoot, "OpenRA.slnx")} -c Release 后重试。");

    if (!State.EngineDirOverridden)
    {
        Platform.OverrideEngineDir(openraRoot);
        State.EngineDirOverridden = true;
    }

    Log.AddChannel("perf", null);
    Log.AddChannel("debug", null);
    Game.InitializeSettings(Arguments.Empty);

    var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
    if (!mods.ContainsKey(modId))
        throw new InvalidOperationException($"mod '{modId}' 不在可用列表：{string.Join(", ", mods.Keys)}");

    var modData = new ModData(mods[modId], mods);
    var utility = new Utility(modData, mods);
    Game.ModData = modData;

    var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", modId, "maps"));
    using var package = mapsDir.OpenPackage(mapName + ".oramap", modData.ModFiles)
        ?? throw new InvalidOperationException($"无法打开 {mapName}.oramap（zip/目录签名均不匹配）");

    var map = new Map(modData, package);
    var sb = new StringBuilder();
    sb.Append("uid ").Append(map.Uid).Append('\n');
    sb.Append("mapformat ").Append(map.MapFormat).Append('\n');
    sb.Append("requiresmod ").Append(map.RequiresMod).Append('\n');
    sb.Append("title ").Append(map.Title).Append('\n');
    sb.Append("author ").Append(map.Author).Append('\n');
    sb.Append("tileset ").Append(map.Tileset).Append('\n');
    sb.Append("mapsize ").Append(map.MapSize.Width).Append(',').Append(map.MapSize.Height).Append('\n');
    sb.Append("bounds ").AppendFormat(CultureInfo.InvariantCulture, "{0},{1},{2},{3}",
        map.Bounds.Left, map.Bounds.Top, map.Bounds.Width, map.Bounds.Height).Append('\n');
    sb.Append("visibility ").Append(map.Visibility).Append('\n');
    sb.Append("categories ").Append(string.Join(",", map.Categories)).Append('\n');
    sb.Append("players ").Append(map.PlayerDefinitions.Count).Append('\n');
    sb.Append("actors ").Append(map.ActorDefinitions.Count).Append('\n');
    sb.Append("invalidcustomrules ").Append(map.InvalidCustomRules).Append('\n');
    sb.Append("replacedinvalidtiles ").Append(map.ReplacedInvalidTerrainTiles.Count).Append('\n');

    // 图块直方图：与 map.bin 读取序一致（i=宽 外层，j=高 内层），输出按 (type,index) 升序
    var hist = new Dictionary<(ushort, byte), int>();
    var resHist = new Dictionary<(byte, byte), int>();
    for (var i = 0; i < map.MapSize.Width; i++)
    {
        for (var j = 0; j < map.MapSize.Height; j++)
        {
            var uv = new MPos(i, j);
            var t = map.Tiles[uv];
            hist[(t.Type, t.Index)] = hist.TryGetValue((t.Type, t.Index), out var c) ? c + 1 : 1;
            var r = map.Resources[uv];
            resHist[(r.Type, r.Index)] = resHist.TryGetValue((r.Type, r.Index), out var c2) ? c2 + 1 : 1;
        }
    }

    var ordered = hist.OrderBy(k => k.Key.Item1).ThenBy(k => k.Key.Item2).ToList();
    sb.Append("tilerecords ").Append(ordered.Count).Append('\n');
    foreach (var kv in ordered)
        sb.Append("tile ").Append(kv.Key.Item1).Append(',').Append(kv.Key.Item2).Append(' ').Append(kv.Value).Append('\n');

    sb.Append("resourcerecords ").Append(resHist.Count).Append('\n');
    foreach (var kv in resHist.OrderBy(k => k.Key.Item1).ThenBy(k => k.Key.Item2))
        sb.Append("resource ").Append(kv.Key.Item1).Append(',').Append(kv.Key.Item2).Append(' ').Append(kv.Value).Append('\n');

    var ti = map.Rules.TerrainInfo;
    sb.Append("terraininfo id=").Append(ti.Id)
      .Append(" name=").Append(ti.Name)
      .Append(" tilesize=").Append(ti.TileSize.Width).Append(',').Append(ti.TileSize.Height)
      .Append(" terraintypes=").Append(ti.TerrainTypes.Length).Append('\n');
    var dt = ti.DefaultTerrainTile;
    sb.Append("defaulttile ").Append(dt.Type).Append(',').Append(dt.Index).Append('\n');

    // 每条 tile 记录（升序前 5 条）的 GetTerrainIndex：覆盖类型表排序编号 + 模板帧映射
    var terrainIdx = new StringBuilder();
    var n = 0;
    foreach (var kv in ordered)
    {
        if (n++ == 5) break;
        terrainIdx.Append(kv.Key.Item1).Append(',').Append(kv.Key.Item2).Append("->")
            .Append(ti.GetTerrainIndex(new TerrainTile(kv.Key.Item1, kv.Key.Item2))).Append(' ');
    }

    sb.Append("terrainidx ").Append(terrainIdx.ToString().TrimEnd()).Append('\n');

    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

// —— orders 模式（T08/issue #9）：宿主内直接构造 Order → Serialize() dump 字节。
//    写侧与 World 解耦（OrderTest.cs 蓝本 Tier A + 反射伪 actor/cell Target 的 Tier B）。
//    伪 actor 配方：FormatterServices.GetUninitializedObject(Actor) + 反射设 ActorID
//    （public readonly 字段，Actor.cs:53）与 Generation（public 字段，Actor.cs:68）——
//    Target.FromActor/Order ctor/Serialize 都只读这两个成员。
//    伪 cell/multi-pos Target：boxed 反射直填私有字段（type/cell/subCell/
//    terrainCenterPosition/terrainPositions），TargetType 是 internal enum 故经
//    Enum.ToObject(FieldType) 赋值。序列化只读 SerializableState 暴露的这些字段。
static byte[] DumpOrders()
{
    var sb = new StringBuilder();
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 OpenRA.Game/Network/Order.cs").Append('\n');
    sb.Append("# case <name> / hex <lowercase hex of Order.Serialize()> / fields <key=value ...>").Append('\n');

    void Case(string name, Order o)
    {
        try
        {
            var bytes = o.Serialize();
            sb.Append("case ").Append(name).Append('\n');
            sb.Append("hex ").Append(Convert.ToHexString(bytes).ToLowerInvariant()).Append('\n');
            sb.Append("fields ").Append(DescribeOrder(o, bytes)).Append('\n');
        }
        catch (Exception e)
        {
            throw new InvalidOperationException($"DumpOrders case '{name}' failed: {e}", e);
        }
    }

    // —— Tier A：worldless 往返面（上游 OrderTest.cs 六用例 + 扩展） ——
    Case("empty", new Order());
    Case("unqueued", new Order("Test", null, false));
    Case("queued", new Order("Test", null, true));
    Case("pos-target", new Order("Test", null, Target.FromPos(new WPos(int.MinValue, 0, int.MaxValue)), false));
    Case("invalid-target", new Order("Test", null, Target.Invalid, false));
    Case("extra-fields", new Order("Test", null, Target.Invalid, true)
    {
        TargetString = "TargetString",
        ExtraLocation = new CPos(2047, 2047, 128),
        ExtraData = uint.MaxValue,
        IsImmediate = true,
    });
    Case("target-string-empty", new Order("Test", null, false) { TargetString = "" });
    Case("utf8-strings", new Order("Тест-日本語", null, false) { TargetString = "τ=2π" });
    var handshakeOrder = new Order("Test", null, false);
    handshakeOrder.Type = OrderType.Handshake;
    handshakeOrder.TargetString = "payload";
    Case("handshake", handshakeOrder);

    var handshakeEmptyOrder = new Order("Test", null, false);
    handshakeEmptyOrder.Type = OrderType.Handshake;
    handshakeEmptyOrder.TargetString = "";
    Case("handshake-empty", handshakeEmptyOrder);
    Case("terrain-multi-pos", new Order("Attack", null, MakeTerrainPositionsTarget(
        new WPos(100, 200, 0), [new WPos(100, 200, 0), new WPos(300, 400, 0)]), false));
    Case("extra-actors-empty", new Order("Test", null, false) { ExtraActors = [] });
    Case("grouped-empty", new Order("CreateGroup", null, false, groupedActors: []));

    // —— Tier B：伪 actor / 伪 cell 目标（沙盒最小订单集，见调研 §3） ——
    var actorSubject = MakeFakeActor(7, 3);
    var actorAttacker = MakeFakeActor(8, 1);
    var actorFactory = MakeFakeActor(2, 0);
    var actorPlayer = MakeFakeActor(1, 0);
    var actorStance = MakeFakeActor(6, 0);
    var actorMoveTarget = MakeFakeActor(9, -2);

    Case("move-cell", new Order("Move", actorSubject, MakeCellTarget(new CPos(10, 5), (SubCell)2), true));
    Case("actor-target", new Order("Attack", actorAttacker, Target.FromActor(actorMoveTarget), true));
    Case("start-production", Order.StartProduction(actorFactory, "e1", 5));
    Case("pause-production", Order.PauseProduction(actorFactory, "e1", true));
    Case("cancel-production", Order.CancelProduction(actorFactory, "e1", 3));
    Case("place-building", new Order("PlaceBuilding", actorPlayer, MakeCellTarget(new CPos(20, 10), SubCell.FullCell), false)
    {
        TargetString = "fact",
        ExtraData = 42,
        ExtraLocation = new CPos(0, 1),
    });
    Case("line-build", new Order("LineBuild", actorPlayer, MakeCellTarget(new CPos(30, 15), SubCell.FullCell), false)
    {
        TargetString = "pbox",
        ExtraData = 43,
        ExtraLocation = new CPos(1, 0),
    });
    Case("create-group", new Order("CreateGroup", actorPlayer, false, groupedActors:
        [MakeFakeActor(3, 0), MakeFakeActor(4, 0), MakeFakeActor(5, 0)]));
    Case("set-unit-stance", new Order("SetUnitStance", actorStance, false) { ExtraData = 2 });
    Case("extra-actors", new Order("Test", null, false)
    {
        ExtraActors = [MakeFakeActor(3, 0), MakeFakeActor(4, 0)],
    });
    Case("chat", Order.Chat("Hello world"));
    Case("chat-team", Order.Chat("team only", 2));
    Case("chat-spectators", Order.Chat("spec", uint.MaxValue));
    Case("command", Order.Command("/help"));
    Case("pause-game", Order.FromTargetString("PauseGame", "Pause", false));

    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

static Actor MakeFakeActor(uint id, int generation)
{
    var actor = (Actor)FormatterServices.GetUninitializedObject(typeof(Actor));
    typeof(Actor).GetField(nameof(Actor.ActorID))!.SetValue(actor, id);
    typeof(Actor).GetField(nameof(Actor.Generation))!.SetValue(actor, generation);
    return actor;
}

static Target MakeCellTarget(CPos cell, SubCell subCell)
{
    var boxed = (object)Target.Invalid;
    const BindingFlags Flags = BindingFlags.Instance | BindingFlags.NonPublic;
    var typeField = typeof(Target).GetField("type", Flags)!;
    typeField.SetValue(boxed, Enum.ToObject(typeField.FieldType, 2));
    typeof(Target).GetField("cell", Flags)!.SetValue(boxed, (CPos?)cell);
    typeof(Target).GetField("subCell", Flags)!.SetValue(boxed, (SubCell?)subCell);
    var pos = WPos.Zero;
    typeof(Target).GetField("terrainCenterPosition", Flags)!.SetValue(boxed, pos);
    typeof(Target).GetField("terrainPositions", Flags)!.SetValue(boxed, new[] { pos });
    return (Target)boxed;
}

static Target MakeTerrainPositionsTarget(WPos center, WPos[] positions)
{
    var boxed = (object)Target.Invalid;
    const BindingFlags Flags = BindingFlags.Instance | BindingFlags.NonPublic;
    var typeField = typeof(Target).GetField("type", Flags)!;
    typeField.SetValue(boxed, Enum.ToObject(typeField.FieldType, 2));
    typeof(Target).GetField("terrainCenterPosition", Flags)!.SetValue(boxed, center);
    typeof(Target).GetField("terrainPositions", Flags)!.SetValue(boxed, positions);
    return (Target)boxed;
}

static string DescribeOrder(Order o, byte[] bytes)
{
    string flagsPart;
    if (bytes[0] == 0xFF)
    {
        var p = 1;
        var len = 0;
        var shift = 0;
        while (true)
        {
            var b = bytes[p++];
            len |= (b & 0x7f) << shift;
            shift += 7;
            if ((b & 0x80) == 0) break;
        }

        p += len;
        var flags = BitConverter.ToInt16(bytes, p);
        flagsPart = $"flags=0x{flags:x4}";
    }
    else
        flagsPart = "flags=handshake";

    var subject = o.Subject != null
        ? o.Subject.ActorID.ToString(CultureInfo.InvariantCulture) : "-";
    var text = o.TargetString == null ? "-" : $"{o.TargetString.Length}:{o.TargetString}";
    var loc = o.ExtraLocation.Bits.ToString(CultureInfo.InvariantCulture);
    var ea = o.ExtraActors == null ? "-" : o.ExtraActors.Length.ToString(CultureInfo.InvariantCulture);
    var grouped = o.GroupedActors == null ? "-" : o.GroupedActors.Length.ToString(CultureInfo.InvariantCulture);
    var targetKind = ((byte)o.Target.Type).ToString(CultureInfo.InvariantCulture);

    return $"type={(o.Type == OrderType.Handshake ? "handshake" : "fields")} orderlen={o.OrderString.Length} " +
        $"{flagsPart} subject={subject} queued={(o.Queued ? 1 : 0)} targetkind={targetKind} " +
        $"text={text} data={o.ExtraData.ToString(CultureInfo.InvariantCulture)} loc={loc} " +
        $"ea={ea} grouped={grouped}";
}

static void WriteGolden(string artifactPath, string relPath, byte[] bytes)
{
    var dir = Path.GetDirectoryName(artifactPath);
    Directory.CreateDirectory(dir);

    // 临时文件 + 原子替换，避免中断留下半截产物
    var tmp = artifactPath + ".tmp";
    File.WriteAllBytes(tmp, bytes);
    File.Move(tmp, artifactPath, overwrite: true);

    var sidecar = artifactPath + ".sha256";
    tmp = sidecar + ".tmp";
    File.WriteAllText(tmp, SidecarLine(relPath, bytes), new UTF8Encoding(false));
    File.Move(tmp, sidecar, overwrite: true);

    Console.WriteLine($"gen: {relPath} sha256={Sha256Hex(bytes)} bytes={bytes.Length}");
}

static bool VerifyGolden(string artifactPath, string relPath, byte[] produced)
{
    if (!File.Exists(artifactPath))
    {
        Console.WriteLine($"verify: {relPath} FAIL —— 黄金文件缺失，先运行 gen 生成");
        return false;
    }

    var golden = File.ReadAllBytes(artifactPath);
    if (!produced.AsSpan().SequenceEqual(golden))
    {
        var offset = FirstDiffOffset(produced, golden);
        Console.WriteLine($"verify: {relPath} FAIL —— 与入库黄金字节不一致" +
            $"（produced={produced.Length}B golden={golden.Length}B，首个差异偏移 {offset}）");
        return false;
    }

    // 旁车 .sha256 与黄金文件自身一致性（格式兼容 `sha256sum -c`）
    var sidecar = artifactPath + ".sha256";
    if (!File.Exists(sidecar) || File.ReadAllText(sidecar) != SidecarLine(relPath, golden))
    {
        Console.WriteLine($"verify: {relPath} FAIL —— .sha256 旁车缺失或与黄金文件不匹配");
        return false;
    }

    return true;
}

static string SidecarLine(string relPath, byte[] bytes) =>
    $"{Sha256Hex(bytes)}  {Path.GetFileName(relPath)}\n";

static string Sha256Hex(byte[] bytes) =>
    Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();

static int FirstDiffOffset(byte[] a, byte[] b)
{
    var n = Math.Min(a.Length, b.Length);
    for (var i = 0; i < n; i++)
        if (a[i] != b[i]) return i;
    return n;
}

// —— 基线断言 ——
static void AssertUpstreamBaseline(string openraRoot, string repoRoot)
{
    var baselineFile = Path.Combine(repoRoot, "UPSTREAM.baseline");
    if (!File.Exists(baselineFile))
        throw new InvalidOperationException($"未找到 {baselineFile}");

    var expected = File.ReadLines(baselineFile).Select(l => l.Trim()).FirstOrDefault(l => l.Length > 0)
        ?? throw new InvalidOperationException("UPSTREAM.baseline 为空");

    var actual = RunCapture("git", $"-C \"{openraRoot}\" rev-parse HEAD", "git rev-parse 失败").Trim();

    if (!string.Equals(expected, actual, StringComparison.OrdinalIgnoreCase))
        throw new BaselineMismatchException(
            $"上游检出与基线不符：UPSTREAM.baseline={expected}，{openraRoot} HEAD={actual}。" +
            "请按 UPSTREAM.baseline 同步流程处理后再产/验黄金。");
}

// —— 路径解析 ——
static string ResolveRepoRoot(string arg)
{
    if (arg != null)
    {
        var full = Path.GetFullPath(arg);
        if (!File.Exists(Path.Combine(full, "UPSTREAM.baseline")))
            throw new InvalidOperationException($"{full} 下无 UPSTREAM.baseline，不是本仓库根目录");
        return full;
    }

    // 默认：自当前目录向上找 UPSTREAM.baseline
    for (var dir = new DirectoryInfo(Directory.GetCurrentDirectory()); dir != null; dir = dir.Parent)
        if (File.Exists(Path.Combine(dir.FullName, "UPSTREAM.baseline")))
            return dir.FullName;

    throw new InvalidOperationException("未能定位仓库根（未指定 --repo-root 且当前目录链上无 UPSTREAM.baseline）");
}

static string ResolveOpenRaRoot(string arg, string repoRoot)
{
    // 主检出布局 <repo>/../openra（PORTING_PLAN 约定）；worktree 布局 <repo>/../../openra
    string[] candidates = arg != null ? [arg] :
    [
        Path.GetFullPath(Path.Combine(repoRoot, "..", "openra")),
        Path.GetFullPath(Path.Combine(repoRoot, "..", "..", "openra")),
    ];

    foreach (var c in candidates)
        if (File.Exists(Path.Combine(c, "OpenRA.slnx")))
            return c;

    throw new InvalidOperationException($"未找到上游 openra 检出（尝试：{string.Join("; ", candidates)}）");
}

// —— 进程工具 ——
static string RunCapture(string exe, string arguments, string what)
{
    var psi = new ProcessStartInfo(exe, arguments)
    {
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
    };
    using var p = Process.Start(psi) ?? throw new InvalidOperationException($"无法启动 {exe}");
    var stdout = p.StandardOutput.ReadToEnd();
    var stderr = p.StandardError.ReadToEnd();
    p.WaitForExit();
    if (p.ExitCode != 0)
        throw new InvalidOperationException($"{what}（exit {p.ExitCode}）：{stderr.Trim()}");
    return stdout;
}

static string NextArg(string[] args, ref int i)
{
    if (++i >= args.Length) throw new ArgumentException("选项缺少取值");
    return args[i];
}

static int Fail(int code, string message)
{
    Console.Error.WriteLine($"golden-gen: {message}");
    return code;
}

// 基线漂移单独给退出码 2，脚本可区分“基准漂移”与“一般宿主错误”
sealed class BaselineMismatchException(string message) : Exception(message);

// 宿主可变状态（顶层语句文件的方法声明区不能含字段，收拢于此）
static class State
{
    // prng 模式种子集：0/1/42/-1/边界值/日期种子（与 T03 入库文件一一对应）
    public static readonly int[] PrngSeeds = [0, 1, 42, -1, int.MinValue, int.MaxValue, 20261103];

    // map-info 模式地图集：TEMPERAT 主选（最小 64×64）+ SNOW 跨 tileset + 128×128 大图
    // （模板 65535 合法 PickAny 使用），调研 §5 选型
    public static readonly string[] MapNames = ["ore-lord", "doubles", "a-path-beyond"];

    // resolved-rules 装配守卫：上游 Platform 二次 OverrideEngineDir 会 throw（见 DumpResolvedRules）
    public static bool EngineDirOverridden;
}

// 上游检出 mods 树之上的最小 IReadOnlyFileSystem（探针 ModRootFs 同构）：
// "ra|rules/misc.yaml" → <root>/mods/ra/rules/misc.yaml；"common|x" → <root>/mods/common/x
sealed class GoldenModRootFs(string root) : IReadOnlyFileSystem
{
    string Map(string filename)
    {
        var sep = filename.IndexOf('|');
        var rel = sep >= 0 ? $"mods/{filename[..sep]}/{filename[(sep + 1)..]}" : filename;
        return Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar));
    }

    public Stream Open(string filename) => File.OpenRead(Map(filename));
    public bool TryOpen(string filename, out Stream s)
    {
        if (!Exists(filename)) { s = null; return false; }
        s = Open(filename); return true;
    }

    public bool Exists(string filename) => File.Exists(Map(filename));
    public bool IsExternalFile(string filename) => false;
    public bool TryGetPackageContaining(string path, out IReadOnlyPackage package, out string filename)
    { package = null; filename = null; return false; }
}
