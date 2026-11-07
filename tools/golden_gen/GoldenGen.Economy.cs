internal static partial class Program
{
// —— economy 模式（T13/issue #14）：t13-scratch 探针 RunAll 逻辑照搬（探针已实测双跑+
//    跨进程恒等，完整经济闭环 t167 首 bale→t659 E1 出厂）。
//    ore-lord 原图资源区 + 最小经济规则集 + 注入 PROC/BARR（剔除全部 12 个 MINE →
//    零 SeedsResource RNG）+ PROC FreeActor 附送 HARV + startingcash=0 + t460 下单 E1。
//    rng 账本：采集/寻矿/回程/docking/卸货全链 0 耗，E1 出厂恰 +1（Exit shuffle）。
static byte[] DumpEconomyScenario(string openraRoot)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);

    if (State.EconomyAsm is not { } asm)
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
        State.EconomyAsm = asm;
    }

    var (modData2, package2) = asm;
    const int seed = 0;
    const int MaxTicks = 1200;
    const int OrderTick = 460;

    var sb = new StringBuilder();
    var map = new Map(modData2, package2);
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 ra economy scenario ore-lord").Append('\n');
    sb.Append("# seed ").Append(seed).Append(" map ore-lord ticks 679 order ").Append(OrderTick)
      .Append(" startingcash 0 actors proc0@11,13 barr0@15,14 MINE stripped").Append('\n');
    sb.Append("map uid ").Append(map.Uid).Append('\n');

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
            Faction@soviet:
                Name: Soviet
                InternalName: soviet
                Side: Soviet
            Locomotor@HEAVYWHEELED:
                Name: heavywheeled
                TerrainSpeeds:
                    Clear: 100
                    Rough: 50
                    Road: 125
                    Bridge: 125
                    Ore: 88
                    Gems: 88
                    Beach: 50
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
            SpawnMapActors:
            ResourceClaimLayer:
            ResourceLayer:
                RecalculateResourceDensity: true
                ResourceTypes:
                    Ore:
                        ResourceIndex: 1
                        TerrainType: Ore
                        AllowedTerrainTypes: Clear, Road
                        MaxDensity: 12
                    Gems:
                        ResourceIndex: 2
                        TerrainType: Gems
                        AllowedTerrainTypes: Clear, Road
                        MaxDensity: 3
            BuildingInfluence:
        player:
            Shroud:
            DeveloperMode:
            PlayerResources:
                ResourceValues:
                    Ore: 25
                    Gems: 50
            TechTree:
            ClassicProductionQueue@Infantry:
                Type: Infantry
                LowPowerModifier: 300
                SpeedUp: true
        proc:
            RenderSprites:
            WithSpriteBody:
            Targetable:
                TargetTypes: GroundActor, Structure
            Building:
                Footprint: _X_ xxx X== ===
                Dimensions: 3,4
                LocalCenterOffset: 0,-512,0
            DockHost:
                Type: Unload
                DockAngle: 256
                DockOffset: 0,1c0,0
            Refinery:
                ShowTicks: false
            StoresPlayerResources:
                Capacity: 2000
            FreeActor:
                Actor: HARV
                SpawnOffset: 1,2
                Facing: 256
        barr:
            RenderSprites:
            WithSpriteBody:
            Targetable:
                TargetTypes: GroundActor, Structure
            Building:
                Footprint: xx xx ==
                Dimensions: 2,3
                LocalCenterOffset: 0,-512,0
            Production:
                Produces: Infantry, Soldier
            ProvidesPrerequisite:
                Prerequisite: barracks
            Exit@1:
                ExitCell: 1,2
                ProductionTypes: Soldier, Infantry
            Exit@2:
                ExitCell: 0,2
                ProductionTypes: Soldier, Infantry
        harv:
            RenderSprites:
            WithSpriteBody:
            Targetable:
                TargetTypes: GroundActor, Vehicle
            HitShape:
            BodyOrientation:
                QuantizedFacings: 32
            Mobile:
                Locomotor: heavywheeled
                Speed: 72
                TurnSpeed: 20
            StoresResources:
                Capacity: 20
                Resources: Ore,Gems
            Harvester:
                Resources: Ore,Gems
                BaleUnloadDelay: 1
                SearchFromProcRadius: 15
                SearchFromHarvesterRadius: 8
                HarvestFacings: 8
            DockClientManager:
            WithDockingAnimation:
        e1:
            Targetable:
                TargetTypes: GroundActor, Infantry
            HitShape:
            BodyOrientation:
                QuantizedFacings: 8
            Mobile:
                Locomotor: foot
                Speed: 113
                TurnSpeed: 512
            Valued:
                Cost: 100
            Buildable:
                Queue: Infantry
                Prerequisites: ~barracks
                BuildDurationModifier: 60
        """;

    var nodes = MiniYaml.FromString(RulesYaml, "t13-minimal-rules");
    var actors = new Dictionary<string, ActorInfo>(StringComparer.OrdinalIgnoreCase);
    foreach (var n in nodes)
        actors[n.Key.ToLowerInvariant()] = new ActorInfo(modData2.ObjectCreator, n.Key.ToLowerInvariant(), n.Value);

    var emptyWeapons = new Dictionary<string, OpenRA.GameRules.WeaponInfo>();
    var emptySounds = new Dictionary<string, OpenRA.GameRules.SoundInfo>();
    var emptyMusic = new Dictionary<string, OpenRA.GameRules.MusicInfo>();
    var emptyModelSeq = new Dictionary<string, MiniYamlNode>();
    var ruleset = new Ruleset(actors, emptyWeapons, emptySounds, emptySounds, emptyMusic, map.Rules.TerrainInfo, emptyModelSeq);
    typeof(Map).GetProperty("Rules", BindingFlags.Public | BindingFlags.Instance)!.SetValue(map, ruleset);

    const string ActorsYaml = """
        proc0: proc
            Location: 11,13
            Owner: Multi0
        barr0: barr
            Location: 15,14
            Owner: Multi0
        """;
    var actorNodes = MiniYaml.FromString(ActorsYaml, "t13-injected-actors").ToList();
    var adField = typeof(Map).GetField("ActorDefinitions", BindingFlags.Public | BindingFlags.Instance)!;
    adField.SetValue(map, (IReadOnlyCollection<MiniYamlNode>)actorNodes);

    OpenRA.Widgets.ChromeMetrics.Initialize(modData2);
    modData2.MapCache.LoadMaps(modData2);
    var om = new OrderManager(new EchoConnection());
    om.LobbyInfo = new Session();
    om.LobbyInfo.GlobalSettings.RandomSeed = seed;
    om.LobbyInfo.GlobalSettings.LobbyOptions["startingcash"] = new Session.LobbyOptionState { Value = "0" };
    om.LobbyInfo.GlobalSettings.LobbyOptions["crates"] = new Session.LobbyOptionState { Value = "False" };
    om.LobbyInfo.GlobalSettings.NetFrameInterval = 1;
    om.LobbyInfo.Clients.Add(new Session.Client
    {
        Index = 1,
        Name = "Probe",
        State = Session.ClientState.Ready,
        Slot = "slot0",
        Faction = "soviet",
        Color = Color.Firebrick,
    });
    om.LobbyInfo.Slots["slot0"] = new Session.Slot { PlayerReference = "Multi0" };

    var worldCtor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
        [typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType)], null)
        ?? throw new InvalidOperationException("World ctor not found");
    var world = (World)worldCtor.Invoke([map, modData2, om, WorldType.Regular]);
    om.World = world;
    sb.Append("world created actors=").Append(world.Actors.Count()).Append('\n');

    map.Sequences.LoadSprites();

    var prNeutral = new PlayerReference { Name = "Neutral", Faction = "allies", OwnsWorld = true, NonCombatant = true };
    var prMulti0 = new PlayerReference { Name = "Multi0", Faction = "soviet", Playable = true };
    var neutral = new Player(world, null, prNeutral, new MersenneTwister(seed));
    var multi0 = new Player(world, null, prMulti0, new MersenneTwister(seed));
    world.SetPlayers([neutral, multi0], multi0);
    world.SetWorldOwner(neutral);
    sb.Append("players ").Append(world.Players.Length).Append('\n');

    foreach (var iwl in world.WorldActor.TraitsImplementing<IWorldLoaded>())
        iwl.WorldLoaded(world, null);

    om.StartGame();

    var oc = modData2.ObjectCreator;
    var traitGeneric = typeof(Actor).GetMethods().First(m => m.Name == "Trait" && m.IsGenericMethodDefinition && m.GetParameters().Length == 0);
    var traitsImplGeneric = typeof(Actor).GetMethod("TraitsImplementing", Type.EmptyTypes)!;
    object TraitOf(Actor a, string typeName) => traitGeneric.MakeGenericMethod(oc.FindType(typeName)!).Invoke(a, null);

    var playerResType = oc.FindType("PlayerResources")!;
    var res = TraitOf(multi0.PlayerActor, "PlayerResources");
    var pCash = playerResType.GetField("Cash")!;
    var pOre = playerResType.GetField("Resources")!;
    var pCap = playerResType.GetField("ResourceCapacity")!;

    var iResourceLayerType = oc.FindType("IResourceLayer")!;
    var resourceLayer = traitGeneric.MakeGenericMethod(iResourceLayerType).Invoke(world.WorldActor, null);
    var getResource = iResourceLayerType.GetMethod("GetResource")!;
    var layerInstance = TraitOf(world.WorldActor, "ResourceLayer");
    var resCellsField = oc.FindType("ResourceLayer")!.GetField("resCells", BindingFlags.NonPublic | BindingFlags.Instance)!;
    var contentsType = oc.FindType("ResourceLayerContents")!;
    var cTypeField = contentsType.GetField("Type")!;
    var cDensityField = contentsType.GetField("Density")!;

    var productionQueueType = oc.FindType("ProductionQueue")!;
    var queueFieldType = productionQueueType.GetField("Queue", BindingFlags.NonPublic | BindingFlags.Instance)!;
    var dockHostType = oc.FindType("DockHost")!;

    sb.Append(CultureInfo.InvariantCulture, $"init startcash 0 ore-cap {pCap.GetValue(res)} rescells {resCellsField.GetValue(layerInstance)} rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount}\n");
    sb.Append("init actors " + string.Join(" ", world.Actors.OrderBy(a => a.ActorID).Select(a => $"{a.ActorID}:{a.Info.Name}:{a.Owner?.InternalName ?? "-"}")) + "\n");

    var fieldCells = new List<(CPos Cell, string Type, byte Density)>();
    foreach (var cell in map.AllCells)
    {
        var content = getResource.Invoke(resourceLayer, [cell]);
        var t = (string)cTypeField.GetValue(content);
        var d = (byte)cDensityField.GetValue(content);
        if (t != null)
            fieldCells.Add((cell, t, d));
    }

    sb.Append($"init field cells {fieldCells.Count} sumdensity {fieldCells.Sum(c => c.Density)}\n");
    foreach (var g in fieldCells.GroupBy(c => c.Type))
        sb.Append(CultureInfo.InvariantCulture, $"init field {g.Key} n {g.Count()} sum {g.Sum(c => c.Density)} min {g.Min(c => c.Density)} max {g.Max(c => c.Density)}\n");

    var ordered = false;
    var e1SeenTick = -1;
    var lastActorLine = "";
    for (var t = 0; t < MaxTicks; t++)
    {
        if (world.WorldTick == OrderTick && !ordered)
        {
            world.IssueOrder(new Order("StartProduction", multi0.PlayerActor, true) { TargetString = "e1", ExtraData = 1 });
            ordered = true;
            sb.Append(CultureInfo.InvariantCulture, $"E {world.WorldTick} order StartProduction e1 issued\n");
        }

        om.TickImmediate();
        om.TryTick();

        world.Tick();

        var cash = (int)pCash.GetValue(res);
        var ore = (int)pOre.GetValue(res);
        var cap = (int)pCap.GetValue(res);
        var rc = (int)resCellsField.GetValue(layerInstance);
        sb.Append(CultureInfo.InvariantCulture, $"E {world.WorldTick} cash {cash} ore {ore} cap {cap} rescells {rc} rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount}\n");

        foreach (var harv in world.Actors.Where(a => a.Info.Name == "harv" && a.IsInWorld).OrderBy(a => a.ActorID))
        {
            var mobile = TraitOf(harv, "Mobile");
            var mType = oc.FindType("Mobile")!;
            var loc = (CPos)mType.GetProperty("FromCell")!.GetValue(mobile);
            var toCell = (CPos)mType.GetProperty("ToCell")!.GetValue(mobile);
            var facing = (WAngle)mType.GetProperty("Facing")!.GetValue(mobile);
            var pos = (WPos)mType.GetProperty("CenterPosition")!.GetValue(mobile);
            var stores = ((System.Collections.IEnumerable)traitsImplGeneric.MakeGenericMethod(oc.FindType("IStoresResources")!).Invoke(harv, null)!).Cast<object>().ToList();
            var storesType = oc.FindType("StoresResources")!;
            var contentsSum = storesType.GetProperty("ContentsSum")!.GetValue(stores[0]);
            var contents = storesType.GetProperty("Contents")!.GetValue(stores[0]);
            var contentsStr = new List<string>();
            foreach (var kv in (System.Collections.IEnumerable)contents)
            {
                var k = kv.GetType().GetProperty("Key")!.GetValue(kv);
                var v = kv.GetType().GetProperty("Value")!.GetValue(kv);
                contentsStr.Add($"{k}={v}");
            }

            var act = harv.CurrentActivity?.GetType().Name ?? "-";
            var childActProp = typeof(OpenRA.Activities.Activity).GetProperty("ChildActivity", BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Public);
            var chain = new List<string>();
            var node = harv.CurrentActivity;
            while (node != null)
            {
                chain.Add(node.GetType().Name);
                node = childActProp?.GetValue(node) as OpenRA.Activities.Activity;
            }

            var moveResultProp = mType.GetProperty("MoveResult");
            var moveResult = moveResultProp != null ? moveResultProp.GetValue(mobile)?.ToString() : "n/a";
            sb.Append(CultureInfo.InvariantCulture, $"H {world.WorldTick} id {harv.ActorID} at {loc.X},{loc.Y}->{toCell.X},{toCell.Y} pos {pos.X},{pos.Y} face {facing.Angle} load {contentsSum} [{string.Join(",", contentsStr)}] act {string.Join("/", chain)} mr {moveResult} idle {harv.IsIdle}\n");
        }

        foreach (var proc in world.Actors.Where(a => a.Info.Name == "proc" && a.IsInWorld).OrderBy(a => a.ActorID))
        {
            var dh = traitGeneric.MakeGenericMethod(dockHostType).Invoke(proc, null);
            var reserved = (System.Collections.IEnumerable)dockHostType.GetField("ReservedDockClients", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(dh)!;
            var reservedCount = 0;
            foreach (var _ in reserved) reservedCount++;
            var docked = dockHostType.GetField("dockedClientActor", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(dh) as Actor;
            sb.Append(CultureInfo.InvariantCulture, $"D {world.WorldTick} proc {proc.ActorID} reserved {reservedCount} docked {(docked != null ? docked.ActorID : -1)}\n");
        }

        var queues = (System.Collections.IEnumerable)traitsImplGeneric.MakeGenericMethod(productionQueueType).Invoke(multi0.PlayerActor, null)!;
        foreach (var q in queues)
        {
            var qType = q.GetType();
            var qEnabled = qType.GetProperty("Enabled")!.GetValue(q);
            var items = (System.Collections.IEnumerable)queueFieldType.GetValue(q)!;
            var itemType = oc.FindType("ProductionItem")!;
            var parts = new List<string>();
            foreach (var item in items)
            {
                string ItemVal(string f)
                {
                    var fi = itemType.GetField(f);
                    if (fi != null)
                        return fi.GetValue(item)?.ToString() ?? "-";
                    var pi = itemType.GetProperty(f);
                    return pi?.GetValue(item)?.ToString() ?? "-";
                }

                parts.Add($"{ItemVal("Item")}:t{ItemVal("RemainingTime")}/{ItemVal("TotalTime")}:c{ItemVal("RemainingCost")}/{ItemVal("TotalCost")}:s{ItemVal("Slowdown")}:d{ItemVal("Done")}:p{ItemVal("ResourcesPaid")}");
            }

            sb.Append(CultureInfo.InvariantCulture, $"Q {world.WorldTick} {qType.Name} enabled {qEnabled} [{string.Join(" | ", parts)}]\n");
        }

        var actorLine = string.Join(" ", world.Actors.OrderBy(a => a.ActorID).Select(a => $"{a.ActorID}:{a.Info.Name}"));
        if (actorLine != lastActorLine)
        {
            sb.Append(CultureInfo.InvariantCulture, $"A {world.WorldTick} {actorLine}\n");
            lastActorLine = actorLine;
        }

        if (e1SeenTick < 0 && world.Actors.Any(a => a.Info.Name == "e1" && a.IsInWorld))
        {
            e1SeenTick = world.WorldTick;
            var e1 = world.Actors.First(a => a.Info.Name == "e1");
            var em = TraitOf(e1, "Mobile");
            var eloc = (CPos)oc.FindType("Mobile")!.GetProperty("FromCell")!.GetValue(em);
            sb.Append(CultureInfo.InvariantCulture, $"E {world.WorldTick} e1 spawned id {e1.ActorID} at {eloc.X},{eloc.Y}\n");
        }

        if (e1SeenTick >= 0 && world.WorldTick >= e1SeenTick + 20)
            break;
    }

    sb.Append(CultureInfo.InvariantCulture, $"final tick {world.WorldTick} cash {pCash.GetValue(res)} ore {pOre.GetValue(res)} rescells {resCellsField.GetValue(layerInstance)} rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount}\n");
    return new UTF8Encoding(false).GetBytes(sb.ToString());
}
}

internal static partial class Program
{
    internal static (string RelPath, byte[] Bytes)[] EconomyGroup(string openraRoot)
    {
        return [("tests/golden/economy/ore-lord.econ.txt", DumpEconomyScenario(openraRoot))];
    }
}
