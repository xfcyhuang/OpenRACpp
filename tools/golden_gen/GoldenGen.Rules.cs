internal static partial class Program
{
// —— resolved-rules 模式：镜像上游 OpenRA.Utility/Program.cs:55-99 的无头装配，
//    命令体（--resolved-rules）经 ObjectCreator 自 openra/bin 的上游程序集加载，
//    oracle 纯度 = 产物全部来自上游代码 ——
static byte[] DumpResolvedRules(string openraRoot, string modId, string actorKey)
{
    // 前置自检：ra 的 manifest 声明程序集（OpenRA.Mods.Common/Cnc.dll）须已在
    // openra/bin——由 run.cmd 在宿主启动前整建 slnx。缺失时报可操作错误而非
    // 让 ObjectCreator 在装配深处抛难解的 FileNotFoundException。
    HeadlessHost.Boot(openraRoot, soundChannel: false);

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
    HeadlessHost.Boot(openraRoot, soundChannel: false);

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
}
