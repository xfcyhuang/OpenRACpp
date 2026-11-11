internal static partial class Program
{
// T34 bot 剧本（issue #53）：normal bot 无头自走录包——宿主 admin 化（client1 观察位
// IsAdmin，Player.cs:217 host-only 激活恒真）+ bot client 注入（Session.Client.Bot 直造，
// 等价 LobbyCommands slot_bot 的服务器侧终态，ClientIndex=ChooseFreePlayerIndex=2）+
// LocalRandom 定种子（World.cs:200 无种子墙钟 MT 的宿主改造——录包可复现前提）。
// 产物 bot.orarep/bot.frames.txt/bot.isync-changes.txt：C++ 重演侧吃录包逐帧对拍
// （口径①A：bot 逻辑在 oracle 宿主侧跑，订单同步面全在录包内）。
const int BotSeed = 20261105;
const int BotFrames = 660;   // 出兵（FeedbackTime 30 节奏）+采矿 FDA+维修冷却 107+停电 Interval 150 触达余量

internal static (string RelPath, byte[] Bytes)[] BotGroup(string openraRoot)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());
    Game.Sound.DisableAllSounds = true;
    ChromeMetricsOnce();

    var run = BotRecordRun();
    return
    [
        ("tests/golden/replay/bot.orarep", run.Orarep),
        ("tests/golden/replay/bot.frames.txt", new UTF8Encoding(false).GetBytes(run.Dump)),
        ("tests/golden/replay/bot.isync-changes.txt", new UTF8Encoding(false).GetBytes(run.IsyncDump)),
    ];
}

// 剧本注入面：bot 生产基地（fact/proc/barr/weap）+ 低电欠额（powr 100 vs 耗 235：
// fact15+proc30+barr10+weap15+tsla150+dome15——ExcessPower<0 触发 PowerDownBotModule
// 停 tsla/dome）+ 采矿链（harv*2 + proc）+ 敌对攻击源（Creeps e1 两只——AutoTarget 互射（InstantHit 无弹道；对建筑伤害不足以跨 Light 阈值——BuildingRepair 的出单面由 C++ 决策单测承载，无头弹道障碍登记）——对
// 建筑 Heavy armor 有效伤，建筑 DamageState 跨 Light 阈值触发 BuildingRepair 的
// RespondToAttack；harv 受击触发 Dock 回港）。无 mcv、无支持力建筑。
const string BotActors = """
    t34fact0: fact
        Location: 21,6
        Owner: Multi0
    t34proc0: proc
        Location: 33,5
        Owner: Multi0
    t34barr0: barr
        Location: 21,9
        Owner: Multi0
    t34weap0: weap
        Location: 24,9
        Owner: Multi0
    t34powr0: powr
        Location: 19,6
        Owner: Multi0
    t34dome0: dome
        Location: 19,8
        Owner: Multi0
    t34tsla0: tsla
        Location: 27,8
        Owner: Multi0
    t34harv0: harv
        Location: 30,7
        Owner: Multi0
    t34harv1: harv
        Location: 31,9
        Owner: Multi0
    t34foe0: e1
        Location: 28,11
        Owner: Creeps
    t34foe1: e1
        Location: 29,12
        Owner: Creeps
    """;

static Session BotBuildSession(string mapUid)
{
    var lobby = new Session();
    lobby.GlobalSettings.RandomSeed = ReplaySeed;
    lobby.GlobalSettings.Map = mapUid;
    lobby.GlobalSettings.GameTimestep = 40;
    lobby.GlobalSettings.NetFrameInterval = 1;
    lobby.GlobalSettings.LobbyOptions["crates"] = new Session.LobbyOptionState { Value = "False" };
    lobby.Slots["Multi0"] = new Session.Slot { PlayerReference = "Multi0", AllowBots = true };

    // client1：宿主 admin（Game.IsHost 的判定源，Game.cs:316-324）。占 Multi1 观察
    // 对照槽（localPlayer 非空——T16 同构的订单链前提）。
    lobby.Slots["Multi1"] = new Session.Slot { PlayerReference = "Multi1" };
    lobby.Clients.Add(new Session.Client
    {
        Index = 1,
        Slot = "Multi1",
        Name = "T34Host",
        IsAdmin = true,
        Faction = "allies",
        SpawnPoint = 2,
        State = Session.ClientState.Ready,
        PreferredColor = Color.FromArgb(255, 255, 255),
        Color = Color.FromArgb(255, 255, 255),
    });

    // client2：bot 槽位（LobbyCommands.cs:521-544 slot_bot 的服务器侧终态：Bot=botType、
    // Faction 固定 soviet——绕开 ResolveFaction 的 playerRandom 抽、SpawnPoint=1、Ready）。
    lobby.Clients.Add(new Session.Client
    {
        Index = 2,
        Slot = "Multi0",
        Name = "T34Bot",
        Bot = "normal",
        Faction = "soviet",
        SpawnPoint = 1,
        Team = 0,
        State = Session.ClientState.Ready,
        PreferredColor = Color.FromArgb(255, 0, 0),
        Color = Color.FromArgb(255, 0, 0),
    });
    return lobby;
}

static void BotSeedLocalRandom(World world)
{
    // World.cs:52 `public readonly MersenneTwister LocalRandom` 无参构造（:200 墙钟）。
    // 录包可复现前提：反射替换为定种子实例（bot 决策随机源的宿主侧钉死）。
    typeof(World).GetField("LocalRandom")!.SetValue(world, new MersenneTwister(BotSeed));
}

static (byte[] Orarep, string Dump, string IsyncDump) BotRecordRun()
{
    var modData = ReplayNewModData(out var map, BotActors);

    var session = BotBuildSession(map.Uid);
    var sessionYaml = session.Serialize();
    var lobby = Session.Deserialize(sessionYaml, "t34");

    var fileName = $"t34-bot-{Guid.NewGuid().ToString("N")[..8]}";
    var conn = new RecordingEchoConnection(() => fileName);
    var om = new OrderManager(conn);
    om.LobbyInfo = lobby;
    ReplaySetGameOrderManager(om);

    var world = ReplayBuildWorld(modData, map, om, lobby);
    BotSeedLocalRandom(world);

    // 激活链宿主改造（票面「Player.cs:217 host-only 激活的 oracle 侧」）：无头宿主
    // 的 Game.IsHost 在 World ctor 期不可达（Game.OrderManager.Connection 的
    // LocalClient 上下文在 ctor 期未装配）——ctor 后手动补激活（等价 host 客户端
    // 的 Player.cs:217-223 路径，Activate 内自带 IBotTick/IBotRespondToAttack 收集）。
    var botPlayer = world.Players.First(p => p.InternalName == "Multi0");

    // 宿主改造（订单链）：OrderValidators 对「host client(1) 发出的 bot 玩家单」按
    // 跨玩家权限拒单（上游真服务器上 bot 单由服务器内嵌 bot client 连接以自己的
    // clientIndex 发送）——无头单连接形态下清空 validator 面（重演侧 C++ 不查权限，
    // 录制/重演两侧等价；bot 单记包归属 host client 的差异登记偏离）。
    typeof(World).GetField("OrderValidators")!.SetValue(world,
        System.Array.Empty<OpenRA.Traits.IValidateOrder>());
    var modularBot = botPlayer.PlayerActor.TraitsImplementing<ITick>()
        .FirstOrDefault(t => t.GetType().Name == "ModularBot");
    if (modularBot == null)
        throw new InvalidOperationException("bot 未激活：ModularBot 缺失");
    var activate = modularBot.GetType().GetMethod("Activate", new[] { typeof(Player) })!;
    activate.Invoke(modularBot, [botPlayer]);
    var isEnabled = (bool)(modularBot.GetType().GetField("IsEnabled")!.GetValue(modularBot) ?? false);
    if (!isEnabled)
        throw new InvalidOperationException("bot 手动激活失败");


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

    conn.Recorder.Receive(0, new OrderPacket([Order.FromTargetString("SyncInfo", sessionYaml, true)]).Serialize(0));
    conn.Recorder.Receive(0, new OrderPacket([Order.FromTargetString("StartGame", "", true)]).Serialize(0));

    om.StartGame();

    var sb = new StringBuilder();
    sb.Append(CultureInfo.InvariantCulture, $"# run frames {BotFrames} players {world.Players.Length} actors {world.Actors.Count()}\n");
    sb.Append("init actors " + string.Join(" ", world.Actors.Select(a => $"{a.ActorID}:{a.Info.Name}:{a.Owner?.InternalName}")) + "\n");

    var prev = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
    var moved = new HashSet<uint>();

    var isyncSb = new StringBuilder();
    var isyncPrev = IsyncSnapshot(world);
    for (var f = 1; f <= BotFrames; f++)
    {
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
    conn.Recorder.Dispose();

    var filePath = Path.Combine(Platform.SupportDir, "Replays", modData.Manifest.Id,
        modData.Manifest.Metadata.Version, fileName + ".orarep");
    if (!File.Exists(filePath))
        throw new InvalidOperationException($"recorder 未产出 {filePath}");

    var normalized = ReplayNormalize(filePath);
    using (var rc = new ReplayConnection(filePath))
        if (!rc.IsValid || rc.LobbyInfo == null)
            throw new InvalidOperationException("上游 ReplayConnection 判定 bot 回放无效");
    return (normalized, sb.ToString(), isyncSb.ToString());
}
}
