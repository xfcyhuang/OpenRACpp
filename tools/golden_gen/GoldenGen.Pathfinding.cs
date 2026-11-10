internal static partial class Program
{
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
    HeadlessHost.Boot(openraRoot, soundChannel: true);

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
}

internal static partial class Program
{
    internal static (string RelPath, byte[] Bytes)[] PathfindingGroup(string openraRoot)
    {
        var (paths, move) = RunPathfindingPass(openraRoot);
        return
        [
            ("tests/golden/pathfinding/ore-lord.paths.txt", paths),
            ("tests/golden/pathfinding/ore-lord.move.txt", move),
        ];
    }
}
