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
//   pathfinding     反射无头 World（ore-lord 真实地形 + 最小合成规则集，t11 §4.1 八障碍
//                   配方）驱动的局部 A*（PathSearch.ToTargetCell，11 对 × w100/w125 +
//                   nolane 对照）、HPF 层（PathFinder.FindPathToTargetCell，10 对 ×
//                   none/all）、costmap 符号图与真实 Move Activity 444 tick 序列
//                   （tests/golden/pathfinding/ore-lord.{paths,move}.txt）
//
// 每次动作前断言上游检出 HEAD == UPSTREAM.baseline 首行，防基准漂移。
// resolved-rules 产物字节为上游命令 stdout 的原样捕获（UTF-8 无 BOM；末尾行尾来自
// Console.WriteLine，Windows 上为 CR LF）——黄金锚定 Windows 上游 stdout 语义，不做任何规范化。
//
// 退出码：0 成功 / 1 校验失败（含 check 双跑不恒等）/ 2 基线漂移 / 3 宿主错误；
//         77 前置缺失（dotnet/上游检出不在）由 run.cmd 判定，供 ctest 记为 SKIP。

using System.Diagnostics;
using System.Globalization;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.Serialization;
using System.Security.Cryptography;
using System.Text;
using OpenRA;
using OpenRA.FileSystem;
using OpenRA.Network;
using OpenRA.Primitives;
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
    var root = e;
    while (root is TargetInvocationException tie && tie.InnerException != null)
        root = tie.InnerException;
    return Fail(ExitError, root == e
        ? $"{e.GetType().Name}: {e.Message}\n{e.StackTrace}"
        : $"{e.GetType().Name}: {e.Message} -> {root.GetType().Name}: {root.Message}\n{root.StackTrace}");
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
        ("tests/golden/pathfinding/ore-lord.paths.txt", () => RunPathfindingPass(openraRoot).Paths),
        ("tests/golden/pathfinding/ore-lord.move.txt", () => RunPathfindingPass(openraRoot).Move),
        ("tests/golden/production/positive.txt", () => DumpProductionScenario(openraRoot, positive: true)),
        ("tests/golden/production/negative.txt", () => DumpProductionScenario(openraRoot, positive: false)),
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

// —— pathfinding 模式（T11/issue #12）：真实 ore-lord 地形 + 最小合成规则集 +
//    反射无头 World，转储局部 A*（PathSearch.ToTargetCell 反射，11 对 × w100/w125 +
//    straight-nolane 对照）与完整 HPF 层（PathFinder.FindPathToTargetCell，9 对 ×
//    none/all）与真实 Move Activity 逐 tick 序列。蓝本 = 调研探针
//    （.ora-explore/t11-scratch，双跑+跨进程恒等已实测）。无头 8 障碍配方见
//    t07-world-tick.md §8/§11 与 t11-pathfinding.md §4.1（最小合成规则集绕开
//    PathFinderOverlay 的渲染依赖、ChromeMetrics 先行、MapCache.LoadMaps、
//    HitShape+BodyOrientation、StubPlatform 声音垫层）。 ——
static (byte[] Paths, byte[] Move) RunPathfindingPass(string openraRoot)
{
    foreach (var assembly in new[] { "OpenRA.Mods.Common.dll", "OpenRA.Mods.Cnc.dll" })
        if (!File.Exists(Path.Combine(Platform.BinDir, assembly)))
            throw new InvalidOperationException(
                $"openra/bin 缺少 {assembly}（BinDir={Platform.BinDir}）。先运行 tools/golden_gen/run.cmd。");

    if (!State.EngineDirOverridden)
    {
        Platform.OverrideEngineDir(openraRoot);
        State.EngineDirOverridden = true;
    }

    Log.AddChannel("perf", null);
    Log.AddChannel("debug", null);
    Log.AddChannel("sound", null);
    Game.InitializeSettings(Arguments.Empty);

    if (State.PathfindingAsm is not { } asm)
    {
        var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
        var modData = new ModData(mods["ra"], mods);
        Game.ModData = modData;
        ObjectCreator.MissingTypeAction = s => Console.WriteLine($"!! MissingType: {s}");
        FieldLoader.UnknownFieldAction = f => Console.WriteLine($"!! UnknownField: {f}");
        Game.Sound = new Sound(new StubPlatform(), new SoundSettings());
        var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", "ra", "maps"));
        var package = mapsDir.OpenPackage("ore-lord.oramap", modData.ModFiles)
            ?? throw new InvalidOperationException("cannot open ore-lord.oramap");
        asm = (modData, package);
        State.PathfindingAsm = asm;
    }

    var (modData2, package2) = asm;

    var map = new Map(modData2, package2);

    const string RulesYaml = """
        world:
            ActorMap:
            ScreenMap:
            Selection:
            ControlGroups:
            Faction@allies:
                Name: Allies
                InternalName: allies
                Side: Allies
            Locomotor@TRACKED:
                Name: tracked
                TerrainSpeeds:
                    Clear: 100
                    Rough: 88
                    Road: 125
                    Bridge: 125
                    Ore: 88
                    Gems: 88
                    Beach: 88
            Locomotor@FOOT:
                Name: foot
                SharesCell: True
                TerrainSpeeds:
                    Clear: 100
                    Rough: 89
                    Road: 111
                    Bridge: 111
                    Ore: 89
                    Gems: 89
                    Beach: 89
            PathFinder:
        player:
            Shroud:
        mover:
            HitShape:
            BodyOrientation:
            Mobile:
                Locomotor: tracked
                Speed: 113
                TurnSpeed: 512
        """;

    var nodes = MiniYaml.FromString(RulesYaml, "t11-minimal-rules");
    var actors = new Dictionary<string, ActorInfo>(StringComparer.OrdinalIgnoreCase);
    foreach (var n in nodes)
        actors[n.Key.ToLowerInvariant()] = new ActorInfo(modData2.ObjectCreator, n.Key.ToLowerInvariant(), n.Value);

    var emptyWeapons = new Dictionary<string, OpenRA.GameRules.WeaponInfo>();
    var emptySounds = new Dictionary<string, OpenRA.GameRules.SoundInfo>();
    var emptyMusic = new Dictionary<string, OpenRA.GameRules.MusicInfo>();
    var emptyModelSeq = new Dictionary<string, MiniYamlNode>();
    var ruleset = new Ruleset(actors, emptyWeapons, emptySounds, emptySounds, emptyMusic, map.Rules.TerrainInfo, emptyModelSeq);
    typeof(Map).GetProperty("Rules", BindingFlags.Public | BindingFlags.Instance)!.SetValue(map, ruleset);

    OpenRA.Widgets.ChromeMetrics.Initialize(modData2);
    modData2.MapCache.LoadMaps(modData2);
    var om = new OrderManager(new EchoConnection());
    var worldCtor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
        [typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType)], null)
        ?? throw new InvalidOperationException("World ctor not found");
    var world = (World)worldCtor.Invoke([map, modData2, om, WorldType.Regular]);
    om.World = world;

    var pr = new PlayerReference { Name = "Neutral", Faction = "allies", OwnsWorld = true, NonCombatant = true };
    var player = new Player(world, null, pr, new MersenneTwister(0));
    world.SetPlayers([player], player);

    foreach (var iwl in world.WorldActor.TraitsImplementing<IWorldLoaded>())
        iwl.WorldLoaded(world, null);

    var sbPaths = new StringBuilder();
    var sbMove = new StringBuilder();

    sbPaths.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 PathSearch/PathFinder + Move, ore-lord/temperat, minimal ruleset").Append('\n');
    sbPaths.Append("map uid ").Append(map.Uid).Append('\n');
    sbPaths.Append("map size ").Append(map.MapSize.Width).Append(',').Append(map.MapSize.Height)
      .Append(" bounds ").Append(map.Bounds.Left).Append(',').Append(map.Bounds.Top)
      .Append(',').Append(map.Bounds.Right).Append(',').Append(map.Bounds.Bottom).Append('\n');
    sbPaths.Append("world created actors=").Append(world.Actors.Count()).Append('\n');
    sbPaths.Append("players ").Append(world.Players.Length).Append('\n');

    var locomotorType = modData2.ObjectCreator.FindType("Locomotor")
        ?? throw new InvalidOperationException("Locomotor type not found");
    var traitsImplementingGeneric = typeof(Actor).GetMethod("TraitsImplementing", Type.EmptyTypes)!;
    var locomotors = ((System.Collections.IEnumerable)traitsImplementingGeneric.MakeGenericMethod(locomotorType)
        .Invoke(world.WorldActor, null)!).Cast<object>().ToList();
    sbPaths.Append("locomotors ").Append(locomotors.Count).Append('\n');
    object tracked = null;
    foreach (var l in locomotors)
    {
        var info = locomotorType.GetField("Info")!.GetValue(l);
        var name = info!.GetType().GetField("Name")!.GetValue(info);
        sbPaths.Append("locomotor ").Append(name).Append('\n');
        if ((string)name == "tracked")
            tracked = l;
    }

    var movementCostForCell = locomotorType.GetMethod("MovementCostForCell", [typeof(CPos)])!;
    sbPaths.Append("costmap tracked\n");
    for (var y = map.Bounds.Top; y < map.Bounds.Bottom; y++)
    {
        var row = new StringBuilder();
        for (var x = map.Bounds.Left; x < map.Bounds.Right; x++)
        {
            var cost = (short)movementCostForCell.Invoke(tracked, [new CPos(x, y)])!;
            row.Append(cost == short.MaxValue ? '#' : cost == 100 ? '.' : cost == 88 ? 'r' : cost == 80 ? 'o' : cost == 113 ? 'R' : '?');
        }

        sbPaths.Append(row).Append('\n');
    }

    var pathSearchType = modData2.ObjectCreator.FindType("PathSearch")
        ?? throw new InvalidOperationException("PathSearch type not found");
    var toTargetCell = pathSearchType.GetMethod("ToTargetCell", BindingFlags.Public | BindingFlags.Static)!;
    var findPath = pathSearchType.GetMethod("FindPath", Type.EmptyTypes)!;
    var dispose = pathSearchType.GetMethod("Dispose", Type.EmptyTypes)!;
    var blockedNone = Enum.ToObject(modData2.ObjectCreator.FindType("BlockedByActor")!, 0);

    (CPos From, CPos To, string Tag)[] pairs =
    [
        (new(10, 17), new(35, 17), "straight"),
        (new(10, 17), new(35, 25), "diagonal"),
        (new(55, 8), new(8, 55), "cross"),
        (new(10, 17), new(50, 45), "longdiag"),
        (new(30, 3), new(30, 58), "vertical"),
        (new(10, 17), new(11, 18), "adjacent"),
        (new(10, 17), new(10, 17), "samecell"),
        (new(10, 17), new(0, 0), "outofbounds"),
        (new(10, 17), new(5, 3), "targetimpassable"),
        (new(5, 3), new(10, 10), "sourceimpassable"),
        (new(55, 8), new(50, 13), "nearpair"),
        // movepair：真实 Move 第二段原定对（远距 >20 格，HPF 走抽象层）——本图实证
        // HPF != 局部 A*（w100/w125 均不同），行保留作 T12+ HPF 复刻的对拍资产；
        // C++ T11 只对拍其 astar 行，hpf 交叉检查不覆盖该对（docs/COVERAGE.md D025）。
        (new(35, 17), new(12, 12), "movepair"),
        // moveturn：move 第二段现用对（近距 7²+7²<400，HPF 退化为 w100 有界局部搜索，
        // 实测 A100==A125==HPF 逐格一致）——覆盖 135° Turn 与 FirstHalf 弧线链。
        (new(35, 17), new(28, 10), "moveturn"),
    ];

    foreach (var (from, to, tag) in pairs)
    foreach (var weight in new[] { 100, 125 })
    {
        var search = toTargetCell.Invoke(null,
            [world, tracked, null, new List<CPos> { from }, to, blockedNone, weight, null, null, true, false, null, null, null])!;
        var path = (List<CPos>)findPath.Invoke(search, null)!;
        dispose.Invoke(search, null);
        sbPaths.Append($"astar {tag} w{weight} n{path.Count}");
        foreach (var c in path)
            sbPaths.Append(' ').Append(c.X).Append(',').Append(c.Y);
        sbPaths.Append('\n');
    }

    {
        var search = toTargetCell.Invoke(null,
            [world, tracked, null, new List<CPos> { new CPos(10, 17) }, new CPos(35, 17), blockedNone, 100, null, null, false, false, null, null, null])!;
        var path = (List<CPos>)findPath.Invoke(search, null)!;
        dispose.Invoke(search, null);
        sbPaths.Append($"astar straight-nolane w100 n{path.Count}");
        foreach (var c in path)
            sbPaths.Append(' ').Append(c.X).Append(',').Append(c.Y);
        sbPaths.Append('\n');
    }

    var moverRef = new ActorReference("mover");
    moverRef.Add(new LocationInit(new CPos(10, 17)));
    moverRef.Add(new OwnerInit(player));
    var mover = world.CreateActor(true, moverRef);
    sbMove.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 Move Activity tick sequence, mover tracked speed 113").Append('\n');
    sbMove.Append("mover id ").Append(mover.ActorID).Append('\n');

    var traitGeneric = typeof(Actor).GetMethods().First(m => m.Name == "Trait" && m.IsGenericMethodDefinition && m.GetParameters().Length == 0);
    var iPathFinderType = modData2.ObjectCreator.FindType("IPathFinder")!;
    var pathFinder = traitGeneric.MakeGenericMethod(iPathFinderType).Invoke(world.WorldActor, null);
    var findPathToTargetCell = iPathFinderType.GetMethod("FindPathToTargetCell")!;
    var blockedAll = Enum.ToObject(modData2.ObjectCreator.FindType("BlockedByActor")!, 3);
    foreach (var (from, to, tag) in new[]
             {
                 (new CPos(10, 17), new CPos(35, 17), "straight"),
                 (new CPos(10, 17), new CPos(35, 25), "diagonal"),
                 (new CPos(55, 8), new CPos(8, 55), "cross"),
                 (new CPos(10, 17), new CPos(50, 45), "longdiag"),
                 (new CPos(30, 3), new CPos(30, 58), "vertical"),
                 (new CPos(10, 17), new CPos(11, 18), "adjacent"),
                 (new CPos(10, 17), new CPos(0, 0), "outofbounds"),
                 (new CPos(10, 17), new CPos(5, 3), "targetimpassable"),
                 (new CPos(5, 3), new CPos(10, 10), "unreachable"),
                 (new CPos(55, 8), new CPos(50, 13), "nearpair"),
                 (new CPos(35, 17), new CPos(12, 12), "movepair"),
                 (new CPos(35, 17), new CPos(28, 10), "moveturn"),
             })
    {
        var sources = (IEnumerable<CPos>)new List<CPos> { from };
        var path = (List<CPos>)findPathToTargetCell.Invoke(pathFinder, [mover, sources, to, blockedNone, null, null, true])!;
        sbPaths.Append($"hpf {tag} none n{path.Count}");
        foreach (var c in path)
            sbPaths.Append(' ').Append(c.X).Append(',').Append(c.Y);
        sbPaths.Append('\n');

        path = (List<CPos>)findPathToTargetCell.Invoke(pathFinder, [mover, sources, to, blockedAll, null, null, true])!;
        sbPaths.Append($"hpf {tag} all n{path.Count}");
        foreach (var c in path)
            sbPaths.Append(' ').Append(c.X).Append(',').Append(c.Y);
        sbPaths.Append('\n');
    }

    var moveType = modData2.ObjectCreator.FindType("Move")!;
    var moveCtor = moveType.GetConstructor([typeof(Actor), typeof(CPos), typeof(WDist), typeof(Actor), typeof(bool), typeof(Color?)])!;
    var move = moveCtor.Invoke([mover, (object)new CPos(35, 17), (object)WDist.Zero, null, false, null]);
    var queueActivity = typeof(Actor).GetMethod("QueueActivity", [typeof(bool), typeof(OpenRA.Activities.Activity)])!;
    queueActivity.Invoke(mover, [false, move]);

    var mobileType = modData2.ObjectCreator.FindType("Mobile")!;
    var mobile = traitGeneric.MakeGenericMethod(mobileType).Invoke(mover, null);
    var pFromCell = mobileType.GetProperty("FromCell")!;
    var pToCell = mobileType.GetProperty("ToCell")!;
    var pFacing = mobileType.GetProperty("Facing")!;
    var pCenter = mobileType.GetProperty("CenterPosition")!;

    for (var t = 0; t < 400; t++)
    {
        world.Tick();
        var from = (CPos)pFromCell.GetValue(mobile);
        var to = (CPos)pToCell.GetValue(mobile);
        var facing = (WAngle)pFacing.GetValue(mobile);
        var pos = (WPos)pCenter.GetValue(mobile);
        sbMove.Append(CultureInfo.InvariantCulture, $"tick {world.WorldTick} from {from.X},{from.Y} to {to.X},{to.Y} facing {facing.Angle} pos {pos.X},{pos.Y},{pos.Z} rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount} idle {mover.IsIdle}\n");
        if (mover.IsIdle)
            break;
    }

    // 第二段：近距西北对角 (35,17)→(28,10)（= moveturn 对）→ 135° Turn（facing 768→128）
    // + 连续 MoveFirstHalf 弧线链。原定远距对 (12,12) 的 HPF 路径与局部 A* 不同
    // （movepair 行实证），T11 局部 A* 移动面无法逐 tick 对拍，故换此对。
    var move2 = moveCtor.Invoke([mover, (object)new CPos(28, 10), (object)WDist.Zero, null, false, null]);
    queueActivity.Invoke(mover, [false, move2]);
    for (var t = 0; t < 400; t++)
    {
        world.Tick();
        var from = (CPos)pFromCell.GetValue(mobile);
        var to = (CPos)pToCell.GetValue(mobile);
        var facing = (WAngle)pFacing.GetValue(mobile);
        var pos = (WPos)pCenter.GetValue(mobile);
        sbMove.Append(CultureInfo.InvariantCulture, $"tick2 {world.WorldTick} from {from.X},{from.Y} to {to.X},{to.Y} facing {facing.Angle} pos {pos.X},{pos.Y},{pos.Z} rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount} idle {mover.IsIdle}\n");
        if (mover.IsIdle)
            break;
    }

    return (new UTF8Encoding(false).GetBytes(sbPaths.ToString()),
        new UTF8Encoding(false).GetBytes(sbMove.ToString()));
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

// —— production 模式（T10/issue #11）：无头 World 剧本驱动（t07-probe 配方 9 障碍全解）
//    + T10 沙盒图（程序化构造的全 Clear 64x64 TEMPERAT .oramap，临时目录，不入上游检出）
//    + 订单剧本（StartProduction/PlaceBuilding，EchoConnection 投影下一帧）
//    + 逐 tick 观测行（与 C++ production_observation 逐字节同格式）。
//    口径修正：上游电力不足语义 = 生产 ×LowPowerModifier/100 减速（ra 300 → 每 3 tick 推进 1），
//    非停摆；真停摆仅缺钱/出口堵/生产建筑被禁用。
//    Mods.Common trait（ProductionQueue/PowerManager/PlayerResources）经反射访问（t06 ALC 禁忌：
//    宿主不得 ProjectReference Mods.Common，否则 ObjectCreator 双加载致类型身份分裂）。
static byte[] DumpProductionScenario(string openraRoot, bool positive)
{
    foreach (var assembly in new[] { "OpenRA.Mods.Common.dll", "OpenRA.Mods.Cnc.dll" })
        if (!File.Exists(Path.Combine(Platform.BinDir, assembly)))
            throw new InvalidOperationException(
                $"openra/bin 缺少 {assembly}（BinDir={Platform.BinDir}）。先运行 tools/golden_gen/run.cmd。");

    if (!State.EngineDirOverridden)
    {
        Platform.OverrideEngineDir(openraRoot);
        State.EngineDirOverridden = true;
    }

    Log.AddChannel("perf", null);
    Log.AddChannel("debug", null);
    Game.InitializeSettings(Arguments.Empty);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());

    if (!State.ChromeInitialized)
    {
        var bootMods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
        OpenRA.Widgets.ChromeMetrics.Initialize(new ModData(bootMods["ra"], bootMods));
        State.ChromeInitialized = true;
    }

    var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
    var modData = new ModData(mods["ra"], mods);
    Game.ModData = modData;
    modData.MapCache.LoadMaps(modData);

    var map = new Map(modData, MakeSandboxPackage(positive));
    modData.PrepareMap(map);

    foreach (var actorName in map.Rules.Actors.Keys.ToList())
    {
        var info = map.Rules.Actors[actorName];
        var fld = typeof(ActorInfo).GetField("traits", BindingFlags.NonPublic | BindingFlags.Instance);
        var dict = (OpenRA.Primitives.TypeDictionary)fld.GetValue(info);
        var doomed = dict.OfType<TraitInfo>().Where(t => State.StripRenderTraits.Contains(t.GetType().Name)).ToList();
        foreach (var t in doomed)
            dict.Remove(t);
    }

    const int seed = 42;
    var om = new OrderManager(new EchoConnection());
    om.LobbyInfo = new Session();
    om.LobbyInfo.GlobalSettings.RandomSeed = seed;
    om.LobbyInfo.GlobalSettings.NetFrameInterval = 1;
    om.LobbyInfo.GlobalSettings.LobbyOptions["crates"] = new Session.LobbyOptionState { Value = "False" };
    om.LobbyInfo.GlobalSettings.LobbyOptions["techlevel"] = new Session.LobbyOptionState { Value = "unrestricted" };
    om.LobbyInfo.GlobalSettings.LobbyOptions["startingcash"] = new Session.LobbyOptionState { Value = "5000" };
    om.LobbyInfo.Slots["Multi0"] = new Session.Slot { PlayerReference = "Multi0" };
    om.LobbyInfo.Clients.Add(new Session.Client
    {
        Index = 1,
        Name = "T10Oracle",
        Slot = "Multi0",
        // ra 的 "soviet" 派系 Selectable: False（玩家选国籍 russia/ukraine 等）——
        // ResolveFaction(requireSelectable) 会过滤掉 soviet 随机回落；russia 属苏系
        Faction = "russia",
        SpawnPoint = 0,
    });

    // Game.LocalClientId 在 CreateMapPlayers 读取（Game.OrderManager.Connection.LocalClientId），
    // 无头为 null —— 反射设一个一次性 OrderManager（其 EchoConnection LocalClientId=1）
    typeof(Game).GetField("OrderManager", BindingFlags.NonPublic | BindingFlags.Static)!
        .SetValue(null, new OrderManager(new EchoConnection()));

    var ctor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
        new[] { typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType) }, null)
        ?? throw new InvalidOperationException("World ctor 反射失败");
    var world = (World)ctor.Invoke(new object[] { map, modData, om, WorldType.Regular });
    om.World = world;

    // wr 依赖过滤集 + MapStartingLocations（其 WorldLoaded 对 LocalPlayer 触 wr.Viewport ——
    // 沙盒 client Index=1 == Game.LocalClientId 使 Multi0 成为 LocalPlayer；跳过仅损失
    // 出生点周边 shroud explore，纯观察者状态不进同步面）
    var wrDependent = new HashSet<string> { "CameraOvalMover", "EditorActorLayer", "ResourceRenderer", "SmudgeLayer", "ShroudRenderer", "MapStartingLocations" };
    world.ScreenMap.WorldLoaded(world, null);
    foreach (var iwl in world.WorldActor.TraitsImplementing<IWorldLoaded>())
        if (!wrDependent.Contains(iwl.GetType().Name) && !ReferenceEquals(iwl, world.ScreenMap))
            iwl.WorldLoaded(world, null);
    foreach (var p in world.Players)
        foreach (var iwl in p.PlayerActor.TraitsImplementing<IWorldLoaded>())
            if (!wrDependent.Contains(iwl.GetType().Name))
                iwl.WorldLoaded(world, null);

    om.StartGame();

    var player = world.Players.First(p => p.InternalName == "Multi0");
    var playerActor = player.PlayerActor;

    // ObjectCreator.FindType 以程序集根命名空间拼接（OpenRA.Mods.Common.ProductionQueue 映射 Traits/Player/ProductionQueue.cs）
    var queueType = modData.ObjectCreator.FindType("ProductionQueue")
        ?? throw new InvalidOperationException("ProductionQueue 类型未找到");
    var powerType = modData.ObjectCreator.FindType("PowerManager")
        ?? throw new InvalidOperationException("PowerManager 类型未找到");
    var resourcesType = modData.ObjectCreator.FindType("PlayerResources")
        ?? throw new InvalidOperationException("PlayerResources 类型未找到");

    var traitsImplementing = typeof(Actor).GetMethods()
        .First(m => m.Name == "TraitsImplementing" && m.IsGenericMethodDefinition && m.GetParameters().Length == 0);

    object? Prop(object target, string name)
    {
        var type = target.GetType();
        var prop = type.GetProperty(name, BindingFlags.Public | BindingFlags.Instance);
        if (prop != null)
            return prop.GetValue(target);
        return type.GetField(name, BindingFlags.Public | BindingFlags.Instance)?.GetValue(target);
    }

    var queues = ((System.Collections.IEnumerable)traitsImplementing.MakeGenericMethod(queueType).Invoke(playerActor, null)).Cast<object>().ToList();
    var power = ((System.Collections.IEnumerable)traitsImplementing.MakeGenericMethod(powerType).Invoke(playerActor, null)).Cast<object>().First();
    var resources = ((System.Collections.IEnumerable)traitsImplementing.MakeGenericMethod(resourcesType).Invoke(playerActor, null)).Cast<object>().First();

    var schedule = positive
        ? new (int Frame, Action<World, Actor>)[]
        {
            (10, (w, pa) => w.IssueOrder(Order.StartProduction(pa, "powr", 1))),
            (200, (w, pa) => w.IssueOrder(MakePlaceBuildingOrder(w, pa, "powr", 33, 30))),
            (210, (w, pa) => w.IssueOrder(Order.StartProduction(pa, "barr", 1))),
            (520, (w, pa) => w.IssueOrder(MakePlaceBuildingOrder(w, pa, "barr", 33, 34))),
            (540, (w, pa) => w.IssueOrder(Order.StartProduction(pa, "e1", 1))),
        }
        : new (int Frame, Action<World, Actor>)[]
        {
            (20, (w, pa) => w.IssueOrder(Order.StartProduction(pa, "powr", 1))),
            // 放置点须落在 GivesBuildableArea 邻接（Adjacent=2，IsCloseEnoughToBase 双条件：
            // BaseProvider 半径 AND 邻接扫描框内有 GivesBuildableArea 格）——(33,30) 邻接 fact，
            // 与 positive 同款；40,30 距 fact/barr/silo 均 >2 格会被沉默拒绝（实证 golden）。
            (570, (w, pa) => w.IssueOrder(MakePlaceBuildingOrder(w, pa, "powr", 33, 30))),
            (580, (w, pa) => w.IssueOrder(Order.StartProduction(pa, "e1", 1))),
        };
    // 剧本截断帧 = 首个单位出厂帧（positive e1@601 / negative e1@761）。
    // 实证（T10 关键发现）：AutoTarget ScanOnIdle 默认 True，idle 扫描每次
    // SharedRandom.Next(3,8) 必耗 1 u32（MersenneTwister.Next(low,high) 仅 diff<=1 短路）——
    // 带武装 actor（FTUR/E1）出场后每 3-8 tick 持续耗 rng（战斗面，T12 领域）。
    // 生产链剧本以"出厂帧收尾"保持 rng 账本只含出厂出口 shuffle；负向压电建筑
    // 改用无武装 SILO（^Building、无 AutoTarget 无 Production）。
    // 另：StartingUnitsInfo.BaseActorFacing 默认 WAngle(512) 非 null——MCV 出生不抽样
    // 不耗 rng（SpawnStartingUnits 的组选择 RandomOrDefault 单组 Next(1) 短路 0 耗）。
    var totalTicks = positive ? 601 : 761;

    var sb = new StringBuilder();
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 ra production scenario ")
      .Append(positive ? "positive" : "negative").Append('\n');
    sb.Append("# seed ").Append(seed).Append(" map t10-sandbox ticks ").Append(totalTicks).Append('\n');
    sb.Append("# lobby crates=False techlevel=unrestricted startingcash=5000 faction=russia startingunits=none(mcvonly)").Append('\n');

    var syncSamples = 0;
    for (var i = 0; i < totalTicks; i++)
    {
        foreach (var (frame, issue) in schedule)
            if (om.NetFrameNumber == frame)
                issue(world, playerActor);

        om.TickImmediate();
        if (om.TryTick())
            syncSamples++;

        world.Tick();

        var cash = (int)Prop(resources, "Cash")!;
        var ore = (int)Prop(resources, "Resources")!;
        var powerP = (int)Prop(power, "PowerProvided")!;
        var powerD = (int)Prop(power, "PowerDrained")!;
        var state = Prop(power, "PowerState")!.ToString()!.ToLowerInvariant();
        sb.Append("tick ").Append(world.WorldTick.ToString(CultureInfo.InvariantCulture))
          .Append(" rng ").Append(world.SharedRandom.Last.ToString(CultureInfo.InvariantCulture))
          .Append(' ').Append(world.SharedRandom.TotalCount.ToString(CultureInfo.InvariantCulture))
          .Append(" cash ").Append(cash.ToString(CultureInfo.InvariantCulture))
          .Append(" ore ").Append(ore.ToString(CultureInfo.InvariantCulture))
          .Append(" power ").Append(powerP.ToString(CultureInfo.InvariantCulture))
          .Append(' ').Append(powerD.ToString(CultureInfo.InvariantCulture))
          .Append(' ').Append(state).Append('\n');

        foreach (var queue in queues)
        {
            var typeName = ((string)Prop(Prop(queue, "Info")!, "Type")!).ToLowerInvariant();
            var enabled = (bool)Prop(queue, "Enabled")!;
            sb.Append("q ").Append(typeName).Append(" en=").Append(enabled ? 1 : 0).Append('\n');
            var items = (System.Collections.IEnumerable)queue.GetType().GetMethod("AllQueued")!.Invoke(queue, null)!;
            foreach (var item in items)
            {
                sb.Append("i ").Append((string)Prop(item, "Item")!)
                  .Append(" rem=").Append(((int)Prop(item, "RemainingTime")!).ToString(CultureInfo.InvariantCulture))
                  .Append(" rc=").Append(((int)Prop(item, "RemainingCost")!).ToString(CultureInfo.InvariantCulture))
                  .Append(" slow=").Append(((int)Prop(item, "Slowdown")!).ToString(CultureInfo.InvariantCulture))
                  .Append(" st=").Append((bool)Prop(item, "Started")! ? 1 : 0)
                  .Append(" pz=").Append((bool)Prop(item, "Paused")! ? 1 : 0)
                  .Append(" dn=").Append((bool)Prop(item, "Done")! ? 1 : 0)
                  .Append(" tt=").Append(((int)Prop(item, "TotalTime")!).ToString(CultureInfo.InvariantCulture))
                  .Append(" tc=").Append(((int)Prop(item, "TotalCost")!).ToString(CultureInfo.InvariantCulture))
                  .Append(" paid=").Append(((int)Prop(item, "ResourcesPaid")!).ToString(CultureInfo.InvariantCulture))
                  .Append('\n');
            }
        }

        sb.Append("actors");
        foreach (var a in world.Actors)
            sb.Append(' ').Append(a.ActorID.ToString(CultureInfo.InvariantCulture))
              .Append(':').Append(a.Info.Name)
              .Append(':').Append(a.Owner == null ? "?" : a.Owner.InternalName);
        sb.Append('\n');
    }

    if (syncSamples != totalTicks)
        throw new InvalidOperationException($"SyncHash 采样 {syncSamples} != tick 数（NetFrameInterval 相位未对齐）");

    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

static Order MakePlaceBuildingOrder(World world, Actor playerActor, string building, int x, int y)
{
    return new Order("PlaceBuilding", playerActor, Target.FromCell(world, new CPos(x, y)), false)
    {
        TargetString = building,
        ExtraData = playerActor.ActorID,
    };
}

static IReadOnlyPackage MakeSandboxPackage(bool positive)
{
    var dir = Path.Combine(Path.GetTempPath(), "openracpp-golden-t10", positive ? "pos" : "neg");
    if (Directory.Exists(dir))
        Directory.Delete(dir, recursive: true);
    Directory.CreateDirectory(dir);

    var actors = new StringBuilder();
    actors.Append("Actors:\n");
    actors.Append("\tActor0: mpspawn\n\t\tLocation: 8,8\n\t\tOwner: Neutral\n");
    actors.Append("\tActor1: fact\n\t\tLocation: 30,30\n\t\tOwner: Multi0\n");
    if (!positive)
    {
        // 压电建筑用无武装 SILO（^Building、Power -10、无 AutoTarget/Production）：
        // FTUR 继承 ^AutoTargetGround，idle 扫描每 3-8 tick 耗 SharedRandom（见剧本注释）。
        // BARR(-20) + 10×SILO(-100) = -120：开局 0 vs 120 Critical；POWR(+100) 放置后 100 vs 120 Low。
        actors.Append("\tActor2: barr\n\t\tLocation: 33,34\n\t\tOwner: Multi0\n");
        var siloSpots = new (int X, int Y)[] { (36, 38), (37, 38), (38, 38), (39, 38), (36, 40), (37, 40), (38, 40), (39, 40), (36, 42), (37, 42) };
        for (var i = 0; i < siloSpots.Length; i++)
            actors.Append($"\tActor{i + 3}: silo\n\t\tLocation: {siloSpots[i].X},{siloSpots[i].Y}\n\t\tOwner: Multi0\n");
    }

    var mapYaml = "MapFormat: 12\n"
        + "RequiresMod: ra\n"
        + $"Title: t10-sandbox-{(positive ? "positive" : "negative")}\n"
        + "Author: golden-gen\n"
        + "Tileset: TEMPERAT\n"
        + "MapSize: 64,64\n"
        + "Bounds: 1,1,62,62\n"
        + "Visibility: Lobby\n"
        + "Categories: Conquest\n"
        + "Players:\n"
        + "\tPlayerReference@Neutral:\n"
        + "\t\tName: Neutral\n"
        + "\t\tOwnsWorld: True\n"
        + "\t\tNonCombatant: True\n"
        + "\t\tFaction: allies\n"
        + "\tPlayerReference@Multi0:\n"
        + "\t\tName: Multi0\n"
        + "\t\tPlayable: True\n"
        + "\t\tFaction: Random\n"
        + actors;

    const int width = 64;
    const int height = 64;
    var mapBin = new MemoryStream();
    mapBin.WriteByte(2);
    mapBin.Write(BitConverter.GetBytes((ushort)width));
    mapBin.Write(BitConverter.GetBytes((ushort)height));
    mapBin.Write(BitConverter.GetBytes((uint)17));
    mapBin.Write(BitConverter.GetBytes((uint)0));
    mapBin.Write(BitConverter.GetBytes((uint)(17 + 3 * width * height)));
    for (var i = 0; i < width * height; i++)
    {
        mapBin.Write(BitConverter.GetBytes((ushort)65535));
        mapBin.WriteByte(0);
    }
    for (var i = 0; i < width * height; i++)
    {
        mapBin.WriteByte(0);
        mapBin.WriteByte(0);
    }

    var oramapPath = Path.Combine(dir, "sandbox.oramap");
    using (var archive = ZipFile.Open(oramapPath, ZipArchiveMode.Create))
    {
        var yamlEntry = archive.CreateEntry("map.yaml");
        using (var writer = new StreamWriter(yamlEntry.Open(), new UTF8Encoding(false)))
            writer.Write(mapYaml);
        var binEntry = archive.CreateEntry("map.bin");
        mapBin.Position = 0;
        using (var stream = binEntry.Open())
            mapBin.CopyTo(stream);
    }

    return new Folder(dir).OpenPackage("sandbox.oramap", null)
        ?? throw new InvalidOperationException("沙盒 .oramap 打开失败");
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

// pathfinding 模式的无头声音垫层（t07 §8 障碍 2 同款：CreateSound 全 no-op，其余成员 throw）
sealed class StubPlatform : IPlatform
{
    public IPlatformWindow CreateWindow(Size size, WindowMode windowMode, float scaleModifier, int vertexBatchSize, int indexBatchBatchSize, int videoDisplay, GLProfile profile)
        => throw new NotImplementedException();
    public ISoundEngine CreateSound(string device) => new StubSoundEngine();
    public IFont CreateFont(byte[] data) => throw new NotImplementedException();
}

sealed class StubSoundEngine : ISoundEngine
{
    public SoundDevice[] AvailableDevices() => [];
    public ISoundSource AddSoundSourceFromMemory(byte[] data, int channels, int soundBits, int soundRate) => null;
    public ISound Play2D(ISoundSource sound, bool loop, bool relative, WPos pos, float volume, bool attenuateVolume) => null;
    public ISound Play2DStream(Stream stream, int channels, int soundBits, int soundRate, bool loop, bool relative, WPos pos, float volume) => null;
    public bool Dummy => true;
    public float Volume { get; set; }
    public void PauseSound(ISound sound, bool paused) { }
    public void StopSound(ISound sound) { }
    public void SetAllSoundsPaused(bool paused) { }
    public void StopAllSounds() { }
    public void SetListenerPosition(WPos position) { }
    public void SetSoundVolume(float volume, ISound music, ISound video) { }
    public void SetSoundLooping(bool looping, ISound sound) { }
    public void SetSoundPosition(ISound sound, WPos position) { }
    public void Dispose() { }
}

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

    // pathfinding 模式装配缓存（ModData + ore-lord 包一次装配、每 pass 新 World 新 Map；
    // 探针实证同进程多 pass 恒等，见 t11-pathfinding.md §4.1）
    public static (ModData ModData, IReadOnlyPackage Package)? PathfindingAsm;

    // ChromeMetrics 进程级一次初始化守卫（World ctor 的 UnitOrderGenerator 构造期读取）
    public static bool ChromeInitialized;

    // 渲染/音频静态依赖的 trait（Info 类名），无头剥离（t07-probe 实证集）
    public static readonly HashSet<string> StripRenderTraits =
    [
        "MenuPostProcessEffectInfo",
        "ChronoshiftPostProcessEffectInfo",
        "FlashPostProcessEffectInfo",
        "TintPostProcessEffectInfo",
        "ChronoVortexRendererInfo",
        "RenderDebugStateInfo",
        "ActorMapOverlayInfo",
        "BuildableTerrainOverlayInfo",
        "CellTriggerOverlayInfo",
        "CustomTerrainDebugOverlayInfo",
        "DebugVisualizationsInfo",
        "HierarchicalPathFinderOverlayInfo",
        "MarkerLayerOverlayInfo",
        "PathFinderOverlayInfo",
        "TerrainGeometryOverlayInfo",
        "TerrainRendererInfo",
        "WarheadDebugOverlayInfo",
        "WithResourceAnimationInfo",
        "LoadWidgetAtGameStartInfo",
        "ObjectivesPanelInfo",
        "StartGameNotificationInfo",
    ];
}

// 全 no-op 声音引擎（两个接口都在 OpenRA.Game，public；t07-probe 同款）
sealed class SandboxStubSoundSource : ISoundSource
{
    public void Dispose() { }
}

sealed class SandboxStubSound : ISound
{
    public float Volume { get; set; }
    public float SeekPosition => 0;
    public bool Complete => true;
    public void SetPosition(WPos pos) { }
    public void Dispose() { }
}

sealed class SandboxStubSoundEngine : ISoundEngine
{
    public bool Dummy => true;
    public float Volume { get; set; }
    public SoundDevice[] AvailableDevices() => Array.Empty<SoundDevice>();
    public ISoundSource AddSoundSourceFromMemory(byte[] data, int channels, int soundBits, int sampleRate) => new SandboxStubSoundSource();
    public ISound Play2D(ISoundSource sound, bool loop, bool relative, WPos pos, float volume, bool attenuateVolume) => null;
    public ISound Play2DStream(Stream stream, int channels, int soundBits, int sampleRate, bool loop, bool relative, WPos pos, float volume) => null;
    public void PauseSound(ISound sound, bool paused) { }
    public void StopSound(ISound sound) { }
    public void SetAllSoundsPaused(bool paused) { }
    public void StopAllSounds() { }
    public void SetListenerPosition(WPos position) { }
    public void SetSoundVolume(float volume, ISound music, ISound video) { }
    public void SetSoundLooping(bool looping, ISound sound) { }
    public void SetSoundPosition(ISound sound, WPos position) { }
    public void Dispose() { }
}

sealed class SandboxStubPlatform : IPlatform
{
    public IPlatformWindow CreateWindow(Size size, WindowMode windowMode, float scaleModifier, int vertexBatchSize, int indexBatchSize, int videoDisplay, GLProfile profile) => throw new NotImplementedException();
    public ISoundEngine CreateSound(string device) => new SandboxStubSoundEngine();
    public IFont CreateFont(byte[] data) => throw new NotImplementedException();
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
