internal static partial class Program
{
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
    HeadlessHost.Boot(openraRoot, soundChannel: false);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());

    ChromeMetricsOnce();

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
}

internal static partial class Program
{
    internal static (string RelPath, byte[] Bytes)[] ProductionGroup(string openraRoot, bool positive)
    {
        return [("tests/golden/production/" + (positive ? "positive" : "negative") + ".txt",
                 DumpProductionScenario(openraRoot, positive))];
    }
}
