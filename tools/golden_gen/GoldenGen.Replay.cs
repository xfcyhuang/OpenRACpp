internal static partial class Program
{
const int ReplaySeed = 20261103;
const int ReplayFrames = 580;   // POWR 无电建造 180×3（低电桶）=540 tick + 放置余量

// replay 模式（T16/issue #17）：一次录制运行投影四产物（orarep/记录流/逐帧 dump/
// isync-changes）。组运行 = 无头引导 → 自录（RecordingEchoConnection 挂接语义）→
// 四投影；orarep 投影内含上游有效性校验 + 自录自放重演对拍 + 规范化（钉死墙钟）。
// isync-changes（T32/issue #51 重建生成器）：每帧 dump 全部 actor 的 ISync trait 的
// [VerifySync] 字段（int/bool/CPos/WPos 值域），帧间 diff——机制面接管字段经
// IsyncMechanized 剔除（C++ replay_world 已语义化，黄金不再垫底）。
internal static (string RelPath, byte[] Bytes)[] ReplayGroup(string openraRoot)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());
    Game.Sound.DisableAllSounds = true;   // t12 无头障碍 11
    ChromeMetricsOnce();

    var run = ReplayRecordRun();
    var stream = ReplayStreamPart(run);
    var frames = ReplayFramesPart(run);
    var orarep = ReplayOrarepPart(run);
    var isync = ReplayIsyncPart(run);
    return
    [
        ("tests/golden/replay/ore-lord.orarep", orarep),
        ("tests/golden/replay/ore-lord.record-stream.bin", stream),
        ("tests/golden/replay/ore-lord.frames.txt", frames),
        ("tests/golden/replay/ore-lord.isync-changes.txt", isync),
    ];
}

// 机制面接管排除集（T32）：这些 (actor,trait,field) 行不再入黄金——C++ replay_world
// 语义化机制驱动（AutoTarget 扫描序/TakeCover prone/GainsExperience/PlayerExperience）。
// actor<150 限定注入 e1（146/147）；生产 e1（150）的 AutoTarget/TakeCover 仍数据面。
static bool IsyncMechanized(uint actorId, string trait, string field)
{
    return (trait == "GainsExperience" && field == "Experience")
        || (trait == "PlayerExperience" && field == "Experience")
        || (actorId < 150 && trait == "AutoTarget" && field == "nextScanTime")
        || (actorId < 150 && trait == "TakeCover" && field == "remainingDuration");
}

static byte[] ReplayIsyncPart((string FilePath, string Dump, string IsyncDump) run)
{
    return new UTF8Encoding(false).GetBytes(run.IsyncDump);
}

static byte[] ReplayStreamPart((string FilePath, string Dump, string IsyncDump) run)
{
    var (metaOffset, _) = ReplayReadRecordStream(run.FilePath);
    using var fs = File.OpenRead(run.FilePath);
    var buf = new byte[metaOffset];
    fs.ReadExactly(buf);
    return buf;
}

static byte[] ReplayFramesPart((string FilePath, string Dump, string IsyncDump) run)
{
    return new UTF8Encoding(false).GetBytes(run.Dump);
}

static byte[] ReplayOrarepPart((string FilePath, string Dump, string IsyncDump) run)
{
    using (var rc = new ReplayConnection(run.FilePath))
    {
        if (!rc.IsValid || rc.LobbyInfo == null)
            throw new InvalidOperationException("上游 ReplayConnection 判定回放无效");
        if (rc.TickCount != 581 || rc.FinalGameTick != ReplayFrames)
            throw new InvalidOperationException(
                $"TickCount/FinalGameTick 异常：{rc.TickCount}/{rc.FinalGameTick}");
    }

    var replay = ReplayVerifyRun(run.FilePath, run.Dump);
    if (!replay)
        throw new InvalidOperationException("自录自放重演逐帧 SyncHash 对拍失败");

    var normalized = ReplayNormalize(run.FilePath);
    var replay2 = ReplayVerifyRun2(normalized, run.Dump);
    if (!replay2)
        throw new InvalidOperationException("规范化文件重演对拍失败");
    return normalized;
}

static (string FilePath, string Dump, string IsyncDump) ReplayRecordRun()
{
    var modData = ReplayNewModData(out var map);

    // Session 双程（serialize→deserialize 后才用——C++ 侧只有文件里的 yaml 这一信息源）
    var session = ReplayBuildSession(map.Uid);
    var sessionYaml = session.Serialize();
    var lobby = Session.Deserialize(sessionYaml, "t16");

    var fileName = $"t16-oracle-{Guid.NewGuid().ToString("N")[..8]}";
    var conn = new RecordingEchoConnection(() => fileName);
    var om = new OrderManager(conn);
    om.LobbyInfo = lobby;
    ReplaySetGameOrderManager(om);   // World ctor 内 CreatePlayers 读 Game.LocalClientId（t16 障碍 16）

    var world = ReplayBuildWorld(modData, map, om, lobby);

    // 元数据（客户端正路在 LoadComplete 尾段挂，World.cs:292-297；无头手工等价；Start 钉死）
    var gameInfo = new GameInformation
    {
        Mod = modData.Manifest.Id,
        Version = modData.Manifest.Metadata.Version,
        MapUid = map.Uid,
        MapTitle = map.Title,
        StartTimeUtc = new DateTime(2026, 1, 1, 0, 0, 0, DateTimeKind.Utc),
    };
    foreach (var p in world.Players)
        gameInfo.AddPlayer(p, lobby);
    conn.Recorder.Metadata = new ReplayMetadata(gameInfo);

    // 引导封包：服务器正路（Server.cs:1288 SyncInfo / :1428 StartGame，from=0 frame=0）
    conn.Recorder.Receive(0, new OrderPacket([Order.FromTargetString("SyncInfo", sessionYaml, true)]).Serialize(0));
    conn.Recorder.Receive(0, new OrderPacket([Order.FromTargetString("StartGame", "", true)]).Serialize(0));

    om.StartGame();

    var p0 = world.Players.First(p => p.InternalName == "Multi0");
    var atk = world.Actors.First(a => a.Info.Name == "e1" && a.Owner == p0);
    var sb = new StringBuilder();
    sb.Append(CultureInfo.InvariantCulture, $"# run frames {ReplayFrames} players {world.Players.Length} actors {world.Actors.Count()}\n");
    sb.Append("init actors " + string.Join(" ", world.Actors.Select(a => $"{a.ActorID}:{a.Info.Name}:{a.Owner?.InternalName}")) + "\n");

    var prev = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
    var moved = new HashSet<uint>();

    // isync-changes（T32）：首帧前的 [VerifySync] 基线快照 + 逐帧 diff
    var isyncSb = new StringBuilder();
    var isyncPrev = IsyncSnapshot(world);
    for (var f = 1; f <= ReplayFrames; f++)
    {
        // 剧本（发单走 OrderManager 正规管道：om.IssueOrder → SendOrders → echo 投影下一帧应用）
        switch (f)
        {
            case 5:
                om.IssueOrder(Order.StartProduction(p0.PlayerActor, "powr", 1, false));
                break;
            case 8:
                om.IssueOrder(Order.StartProduction(p0.PlayerActor, "e1", 1, false));
                break;
            case 15:
                om.IssueOrder(new Order("Move", atk, Target.FromCell(world, new CPos(27, 9)), false));
                break;
            case 25:
                om.IssueOrder(new Order("Attack", atk, Target.FromActor(world.Actors.First(a => a.Info.Name == "e1" && a.Owner.InternalName == "Creeps")), false));
                break;
            case 550:
                om.IssueOrder(new Order("PlaceBuilding", p0.PlayerActor, Target.FromCell(world, new CPos(24, 6)), false)
                {
                    TargetString = "powr",
                    ExtraData = (uint)p0.PlayerActor.ActorID,
                });
                break;
        }

        om.TickImmediate();
        om.TryTick();
        world.Tick();
        ReplaySweepEffects(world);

        var s = conn.SentSyncs.LastOrDefault(x => x.Frame == f);
        sb.Append(CultureInfo.InvariantCulture, $"f {f} sync {s.SyncHash} defeat {s.DefeatState} rng {world.SharedRandom.Last} {world.SharedRandom.TotalCount} actors {world.Actors.Count()} fnv {ReplayActorFnv(world)}\n");

        var isyncNow = IsyncSnapshot(world);
        foreach (var (key, val) in isyncNow)
            if (isyncPrev.TryGetValue(key, out var old) && old != val)
                isyncSb.Append(CultureInfo.InvariantCulture,
                    $"f {f} actor {key.ActorId} {key.TypeName} trait {key.TraitName} field {key.FieldName} {old} -> {val}\n");
        isyncPrev = isyncNow;

        var now = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
        foreach (var id in now.Keys.Where(k => !prev.ContainsKey(k)))
            sb.Append(CultureInfo.InvariantCulture, $"actor+ f{f} {id}:{now[id]}\n");
        foreach (var id in prev.Keys.Where(k => !now.ContainsKey(k)).OrderBy(k => k))
            if (moved.Add(id))
                sb.Append(CultureInfo.InvariantCulture, $"actor- f{f} {id}:{prev[id]}\n");
        prev = now;
    }

    gameInfo.FinalGameTick = world.WorldTick;
    conn.Recorder.Dispose();   // 写元数据（EndTimeUtc=UtcNow 墙钟，ReplayRecorder.cs:111）

    var filePath = Path.Combine(Platform.SupportDir, "Replays", modData.Manifest.Id,
        modData.Manifest.Metadata.Version, fileName + ".orarep");
    if (!File.Exists(filePath))
        throw new InvalidOperationException($"recorder 未产出 {filePath}");
    return (filePath, sb.ToString(), isyncSb.ToString());
}

static ModData ReplayNewModData(out Map map, string actorsYamlOverride = null)
{
    // 每 run 全新 ModData（t07 障碍 6a：规则剥离污染共享 ActorInfo）
    var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
    var modData = new ModData(mods["ra"], mods);
    Game.ModData = modData;
    modData.MapCache.LoadMaps(modData);

    var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", "ra", "maps"));
    // 不 using：Map 持包引用；desync → EndGame → MusicPlaylist 会回读地图包（t16 障碍 19）
    var package = mapsDir.OpenPackage("ore-lord.oramap", modData.ModFiles)
        ?? throw new InvalidOperationException("cannot open ore-lord.oramap");
    map = new Map(modData, package);
    modData.PrepareMap(map);

    // t07 障碍 6：渲染/音频静态依赖 trait 剥离（ActorInfo.traits 反射 Remove）
    var fld = typeof(ActorInfo).GetField("traits", BindingFlags.NonPublic | BindingFlags.Instance)!;
    foreach (var actorName in map.Rules.Actors.Keys.ToList())
    {
        var dict = (OpenRA.Primitives.TypeDictionary)fld.GetValue(map.Rules.Actors[actorName])!;
        var doomed = dict.OfType<TraitInfo>().Where(t => State.StripRenderTraits.Contains(t.GetType().Name)).ToList();
        foreach (var t in doomed)
            dict.Remove(t);
    }

    // 剧本 actor 注入（t13 的 ActorDefinitions 追加法：官方图不动 uid，注入物走 SpawnMapActors 正路）
    const string ActorsYaml = """
        t16fact0: fact
            Location: 21,6
            Owner: Multi0
        t16proc0: proc
            Location: 33,5
            Owner: Multi0
        t16barr0: barr
            Location: 21,9
            Owner: Multi0
        t16atk0: e1
            Location: 25,10
            Owner: Multi0
        t16vic0: e1
            Location: 28,12
            Owner: Creeps
        """;
    var adField = typeof(Map).GetField("ActorDefinitions", BindingFlags.Public | BindingFlags.Instance)!;
    var actorNodes = ((IReadOnlyCollection<MiniYamlNode>)adField.GetValue(map)!).ToList();
    var actorsYaml = actorsYamlOverride ?? ActorsYaml;
    actorNodes.AddRange(MiniYaml.FromString(actorsYaml, "injected-actors"));
    adField.SetValue(map, (IReadOnlyCollection<MiniYamlNode>)actorNodes);

    return modData;
}

static Session ReplayBuildSession(string mapUid)
{
    var lobby = new Session();
    lobby.GlobalSettings.RandomSeed = ReplaySeed;
    lobby.GlobalSettings.Map = mapUid;
    lobby.GlobalSettings.GameTimestep = 40;
    lobby.GlobalSettings.NetFrameInterval = 1;   // 1 帧 = ProcessOrders → world.Tick（t07 补记）
    lobby.GlobalSettings.LobbyOptions["crates"] = new Session.LobbyOptionState { Value = "False" };
    lobby.Slots["Multi0"] = new Session.Slot { PlayerReference = "Multi0", AllowBots = true };
    lobby.Clients.Add(new Session.Client
    {
        Index = 1,
        Slot = "Multi0",
        Name = "T16Oracle",
        Faction = "soviet",
        SpawnPoint = 1,
        Team = 0,
        State = Session.ClientState.Ready,
        PreferredColor = Color.FromArgb(255, 0, 0),
        Color = Color.FromArgb(255, 0, 0),
    });
    return lobby;
}

static World ReplayBuildWorld(ModData modData, Map map, OrderManager om, Session lobby)
{
    var ctor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
        [typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType)], null)
        ?? throw new InvalidOperationException("World ctor not found");
    var world = (World)ctor.Invoke([map, modData, om, WorldType.Regular]);
    om.World = world;

    // LocalPlayer 置空（t16 障碍 17）：MapStartingLocations.WorldLoaded 对 LocalPlayer 走
    // wr.Viewport（NRE）；SetLocalPlayer(null) 早退，写 backing field。回放侧本就 null。
    typeof(World).GetField("<LocalPlayer>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance)!.SetValue(world, null);

    // LoadComplete 等价子集（t07 障碍 4：跳过 wr 依赖者；ScreenMap 先行）
    var wrDependent = new HashSet<string> { "CameraOvalMover", "EditorActorLayer", "ResourceRenderer", "SmudgeLayer", "ShroudRenderer" };
    world.ScreenMap.WorldLoaded(world, null);
    foreach (var iwl in world.WorldActor.TraitsImplementing<IWorldLoaded>())
        if (!wrDependent.Contains(iwl.GetType().Name) && !ReferenceEquals(iwl, world.ScreenMap))
            iwl.WorldLoaded(world, null);
    foreach (var p in world.Players)
        foreach (var iwl in p.PlayerActor.TraitsImplementing<IWorldLoaded>())
            if (!wrDependent.Contains(iwl.GetType().Name))
                iwl.WorldLoaded(world, null);

    return world;
}

static void ReplaySetGameOrderManager(OrderManager om)
{
    // Game.OrderManager 是 internal static 字段（Game.cs:50）；World ctor 内 CreatePlayers
    // 经 Game.LocalClientId 读它（t16 障碍 16）
    typeof(Game).GetField("OrderManager", BindingFlags.NonPublic | BindingFlags.Static)!.SetValue(null, om);
}

static void ReplaySweepEffects(World world)
{
    // t12 无头障碍 10：SpriteEffect 家族 Tick 期注册 ScreenMap 需要 worldRenderer → 帧尾清扫。
    // RNG 消耗在 warhead 内、效果创建之前；ISync 效果（SyncHash 段3）两侧同为空——确定性成立。
    foreach (var e in world.Effects.ToList())
        if (e.GetType().Name is "SpriteEffect" or "FloatingSprite" or "SpriteAnnotation")
            world.Remove(e);
}

// isync-changes 快照（T32）：全部在世 actor 的 ISync trait 的 [VerifySync] 字段，
// 值域限 int/bool/CPos/WPos（Actor 引用字段如 AutoTarget.Aggressor 不 dump——与 T17
// 入库黄金的字段集一致）；机制面接管字段（IsyncMechanized）不入快照。
sealed record IsyncKey(uint ActorId, string TypeName, string TraitName, int TraitInstance, string FieldName);

static Dictionary<IsyncKey, string> IsyncSnapshot(World world, bool applyMechanizedExclusion = true)
{
    var snap = new Dictionary<IsyncKey, string>();
    foreach (var a in world.Actors)
    {
        if (!a.IsInWorld)
            continue;

        var traitInstanceCounts = new Dictionary<string, int>();
        foreach (var t in a.TraitsImplementing<ISync>())
        {
            var traitType = t.GetType();
            var instance = traitInstanceCounts.TryGetValue(traitType.Name, out var n) ? n : 0;
            traitInstanceCounts[traitType.Name] = instance + 1;
            foreach (var fi in traitType.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
            {
                if (!fi.HasAttribute<VerifySyncAttribute>())
                    continue;

                string val;
                var v = fi.GetValue(t);
                switch (v)
                {
                    case bool b: val = b ? "True" : "False"; break;
                    case int i: val = i.ToString(CultureInfo.InvariantCulture); break;
                    case CPos p: val = $"{p.X.ToString(CultureInfo.InvariantCulture)},{p.Y.ToString(CultureInfo.InvariantCulture)}"; break;
                    case WPos w: val = $"{w.X.ToString(CultureInfo.InvariantCulture)},{w.Y.ToString(CultureInfo.InvariantCulture)},{w.Z.ToString(CultureInfo.InvariantCulture)}"; break;
                    default: continue;   // Actor/其它类型不入 dump 集
                }

                if (applyMechanizedExclusion && IsyncMechanized(a.ActorID, traitType.Name, fi.Name))
                    continue;

                snap[new IsyncKey(a.ActorID, a.Info.Name, traitType.Name, instance, fi.Name)] = val;
            }

            foreach (var pi in traitType.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance))
            {
                if (!pi.HasAttribute<VerifySyncAttribute>())
                    continue;

                string val;
                var v = pi.GetValue(t);
                switch (v)
                {
                    case bool b: val = b ? "True" : "False"; break;
                    case int i: val = i.ToString(CultureInfo.InvariantCulture); break;
                    case WAngle wa: val = wa.Angle.ToString(CultureInfo.InvariantCulture); break;
                    case CPos p: val = $"{p.X.ToString(CultureInfo.InvariantCulture)},{p.Y.ToString(CultureInfo.InvariantCulture)}"; break;
                    case WPos w: val = $"{w.X.ToString(CultureInfo.InvariantCulture)},{w.Y.ToString(CultureInfo.InvariantCulture)},{w.Z.ToString(CultureInfo.InvariantCulture)}"; break;
                    default: continue;
                }

                if (applyMechanizedExclusion && IsyncMechanized(a.ActorID, traitType.Name, pi.Name))
                    continue;

                snap[new IsyncKey(a.ActorID, a.Info.Name, traitType.Name, instance, pi.Name)] = val;
            }
        }
    }

    return snap;
}

static string ReplayActorFnv(World w)
{
    ulong h = 1469598103934665603;
    foreach (var a in w.Actors)
        foreach (var c in $"{a.ActorID}:{a.Info.Name}:{(a.IsInWorld ? 1 : 0)}\n")
        {
            h ^= (byte)c;
            h *= 1099511628211;
        }
    return h.ToString("x16", CultureInfo.InvariantCulture);
}

static (long MetaOffset, byte[] Data) ReplayReadRecordStream(string path)
{
    using var fs = File.OpenRead(path);
    while (fs.Position < fs.Length)
    {
        var offset = fs.Position;
        if (fs.ReadInt32() == ReplayMetadata.MetaStartMarker)
        {
            var buf = new byte[offset];
            fs.Position = 0;
            fs.ReadExactly(buf);
            return (offset, buf);
        }
        var len = fs.ReadInt32();
        fs.ReadBytes(len);
    }
    throw new InvalidDataException("no MetaStartMarker");
}

static byte[] ReplayNormalize(string path)
{
    // T16 读→写：记录流原样 + 元数据模型化后重写（钉死墙钟 → 可复现黄金）
    var (metaOffset, head) = ReplayReadRecordStream(path);
    var meta = ReplayMetadata.Read(path) ?? throw new InvalidOperationException("no metadata");
    meta.GameInfo.StartTimeUtc = new DateTime(2026, 1, 1, 0, 0, 0, DateTimeKind.Utc);
    meta.GameInfo.EndTimeUtc = new DateTime(2026, 1, 1, 0, 5, 0, DateTimeKind.Utc);

    using var output = new MemoryStream();
    output.Write(head);
    using (var writer = new BinaryWriter(output, Encoding.UTF8, leaveOpen: true))
        meta.Write(writer);
    return output.ToArray();
}

// 自录自放重演（ReplayPumpConnection 泵 + 逐帧 SyncHash 对拍 + dump 恒等）
static bool ReplayVerifyRun(string path, string recordDump)
{
    Session lobby;
    using (var rc = new ReplayConnection(path))
        lobby = rc.LobbyInfo ?? throw new InvalidOperationException("no lobby in replay");

    var modData = ReplayNewModData(out var map);
    var conn = new ReplayPumpConnection(path);
    var om = new OrderManager(conn);
    om.LobbyInfo = lobby;
    ReplaySetGameOrderManager(om);
    var world = ReplayBuildWorld(modData, map, om, lobby);
    om.StartGame();

    var sb = new StringBuilder();
    sb.Append(CultureInfo.InvariantCulture, $"# run frames {ReplayFrames} players {world.Players.Length} actors {world.Actors.Count()}\n");
    sb.Append("init actors " + string.Join(" ", world.Actors.Select(a => $"{a.ActorID}:{a.Info.Name}:{a.Owner?.InternalName}")) + "\n");

    var prev = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
    var moved = new HashSet<uint>();
    var mismatches = new List<string>();
    for (var f = 1; f <= ReplayFrames; f++)
    {
        om.TickImmediate();
        om.TryTick();
        world.Tick();
        ReplaySweepEffects(world);

        var s = conn.SentSyncs.LastOrDefault(x => x.Frame == f);
        sb.Append(CultureInfo.InvariantCulture, $"f {f} sync {s.SyncHash} defeat {s.DefeatState} rng {world.SharedRandom.Last} {world.SharedRandom.TotalCount} actors {world.Actors.Count()} fnv {ReplayActorFnv(world)}\n");

        // 逐帧对拍：本地 ProcessOrders 点的 hash vs 录中 sync 封包（含 defeatState）
        if (conn.RecordedSyncs.TryGetValue(f, out var rec))
        {
            if (s.SyncHash != 0 || s.DefeatState != 0)
                if (rec.Hash != s.SyncHash || rec.Defeat != s.DefeatState)
                    mismatches.Add($"frame {f}: recorded ({rec.Hash},{rec.Defeat}) != local ({s.SyncHash},{s.DefeatState})");
        }
        else
            mismatches.Add($"frame {f}: no recorded sync in file");

        var now = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
        foreach (var id in now.Keys.Where(k => !prev.ContainsKey(k)))
            sb.Append(CultureInfo.InvariantCulture, $"actor+ f{f} {id}:{now[id]}\n");
        foreach (var id in prev.Keys.Where(k => !now.ContainsKey(k)).OrderBy(k => k))
            if (moved.Add(id))
                sb.Append(CultureInfo.InvariantCulture, $"actor- f{f} {id}:{prev[id]}\n");
        prev = now;
    }

    if (mismatches.Count > 0)
        throw new InvalidOperationException($"replay mismatches: {string.Join("; ", mismatches.Take(3))}");
    if (om.IsOutOfSync)
        throw new InvalidOperationException("replay: OrderManager.IsOutOfSync set");
    return sb.ToString() == recordDump;
}

// 规范化字节流的重演校验（临时文件落盘后走同一泵路径）
static bool ReplayVerifyRun2(byte[] normalized, string recordDump)
{
    var tmp = Path.Combine(Path.GetTempPath(), $"t16-norm-{Guid.NewGuid():N}.orarep");
    try
    {
        File.WriteAllBytes(tmp, normalized);
        return ReplayVerifyRun(tmp, recordDump);
    }
    finally
    {
        File.Delete(tmp);
    }
}

// —— skirmish 模式（T32/issue #51）：敌对单位互射剧本 ——
// 注入 1v2 敌对 e1（AutoTarget 扫描序 + TakeCover prone + 击杀经验/Veterancy 路径：
// A0 双杀 B0/B1 各得 10000 经验 → 20000 达 rank-veteran@200×cost 阈值 → Level 1，
// FirepowerMultiplier 105% 后续伤害面）+ 远角 idle 单位（WithInfantryBody idle 抽样
// 循环锚）。无订单：纯 AutoTarget 自主交战。产物 skirmish.frames.txt——C++ 机制面
// 逐帧 SyncHash/rng 账本对拍（无 isync 数据面垫底、无账本补齐，机制面全语义硬验收）。
const int SkirmishFrames = 160;
const string CHARNLCONST = "\n";

internal static (string RelPath, byte[] Bytes)[] SkirmishGroup(string openraRoot)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());
    Game.Sound.DisableAllSounds = true;
    ChromeMetricsOnce();
    var run = SkirmishRecordRun();
    return
    [
        ("tests/golden/replay/skirmish.frames.txt", new UTF8Encoding(false).GetBytes(run.Dump)),
        ("tests/golden/replay/skirmish.isync-changes.txt", new UTF8Encoding(false).GetBytes(run.IsyncDump)),
    ];
}

static (string Dump, string IsyncDump) SkirmishRecordRun()
{
    const string SkirmishActors = """
        t32a0: e1
            Location: 25,10
            Owner: Multi0
        t32a1: e1
            Location: 24,10
            Owner: Multi0
        t32b0: e1
            Location: 28,12
            Owner: Creeps
        t32b1: e1
            Location: 28,13
            Owner: Creeps
        t32c0: e1
            Location: 50,48
            Owner: Multi0
        """;
    var modData = ReplayNewModData(out var map, SkirmishActors);
    var session = ReplayBuildSession(map.Uid);
    var lobby = Session.Deserialize(session.Serialize(), "t32");
    var conn = new RecordingEchoConnection(() => $"t32-skirmish-{Guid.NewGuid().ToString("N")[..8]}");
    var om = new OrderManager(conn);
    om.LobbyInfo = lobby;
    ReplaySetGameOrderManager(om);
    var world = ReplayBuildWorld(modData, map, om, lobby);
    om.StartGame();

    var sb = new StringBuilder();
    sb.Append(CultureInfo.InvariantCulture, $"# run frames {SkirmishFrames} players {world.Players.Length} actors {world.Actors.Count()}\n");
    sb.Append("init actors " + string.Join(" ", world.Actors.Select(a => $"{a.ActorID}:{a.Info.Name}:{a.Owner?.InternalName}")) + "\n");
    var prev = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
    var moved = new HashSet<uint>();
    var isyncSb = new StringBuilder();
    var isyncPrev = IsyncSnapshot(world, applyMechanizedExclusion: false);
    for (var f = 1; f <= SkirmishFrames; f++)
    {
        om.TickImmediate();
        om.TryTick();
        world.Tick();
        ReplaySweepEffects(world);

        var isyncNow = IsyncSnapshot(world, applyMechanizedExclusion: false);
        foreach (var (key, val) in isyncNow)
            if (isyncPrev.TryGetValue(key, out var old) && old != val)
                isyncSb.Append($"f {f} actor {key.ActorId} {key.TypeName} trait {key.TraitName} field {key.FieldName} {old} -> {val}" + CHARNLCONST);
        isyncPrev = isyncNow;

        var sync = conn.SentSyncs.LastOrDefault(x => x.Frame == f);
        sb.Append(CultureInfo.InvariantCulture, $"f {f} sync {sync.SyncHash} defeat {sync.DefeatState} rng {world.SharedRandom.Last} {world.SharedRandom.TotalCount} actors {world.Actors.Count()} fnv {ReplayActorFnv(world)}\n");

        var now = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
        foreach (var id in now.Keys.Where(k => !prev.ContainsKey(k)))
            sb.Append(CultureInfo.InvariantCulture, $"actor+ f{f} {id}:{now[id]}\n");
        foreach (var id in prev.Keys.Where(k => !now.ContainsKey(k)).OrderBy(k => k))
            if (moved.Add(id))
                sb.Append(CultureInfo.InvariantCulture, $"actor- f{f} {id}:{prev[id]}\n");
        prev = now;
    }

    return (sb.ToString(), isyncSb.ToString());
}
}
