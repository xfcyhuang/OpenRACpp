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
    // World.cs:52 `public readonly MersenneTwister LocalRandom` 无参构造（:200 墙钟：
    // MersenneTwister.cs:25 `: this(Environment.TickCount)`）。
    // 录包可复现前提：反射替换为定种子实例（bot 决策随机源的宿主侧钉死）。
    typeof(World).GetField("LocalRandom")!.SetValue(world, new MersenneTwister(BotSeed));
}

// 激活抽重放（T34 打回·f3 跨进程 sync 分歧根因修复）：PlayerActor.Initialize 在
// World ctor 期被 CreatePlayers→Player ctor 同步触发，GrantConditionOnBotOwner@normal
// 的 INotifyCreated 链在此时授 enable-normal-ai → 各 bot 模块 TraitEnabled 的节奏抽
//（HarvesterBotModule.cs:140-141/PowerDownBotManager.cs:79 等）落在**墙钟种子**的
// LocalRandom 上（ctor 先于 BotSeedLocalRandom 的反射替换）——Environment.TickCount
// 跨进程不同 → toggleTick 等激活节奏值跨进程分歧 → PowerDown 停电时机/state 分歧
// → SyncHash f3 起分歧（bot-diag 双进程实证：PowerManager h 52 vs 172、dome/tsla
// ToggleConditionOnOrder h 1 vs 0）。修法=LocalRandom 定种子后，对该条件 token 做
// revoke→grant 重放：TraitDisabled 无 override（全模块无副作用，grep 实证），重授走
// 同一 cs.Notifiers（Actor.cs:229-231 List 装配序）→ 全部模块 TraitEnabled 在定种子
// MT 上按 ctor 期同序重抽——等价「ctor 期即用 BotSeed」的终态。
static void BotReplayActivationRolls(Actor playerActor)
{
    var tokenField = default(System.Reflection.FieldInfo);
    object granted = null;
    foreach (var t in playerActor.TraitsImplementing<INotifyCreated>())
    {
        if (t.GetType().Name != "GrantConditionOnBotOwner")
            continue;
        var f = t.GetType().GetField("conditionToken", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!;
        var tok = (int)f.GetValue(t)!;
        if (tok == Actor.InvalidConditionToken)
            continue;   // 未授予（Bots 不含 normal 的 @rush/@turtle/@naval）
        tokenField = f;
        granted = t;
        break;
    }

    if (granted == null)
        throw new InvalidOperationException("GrantConditionOnBotOwner 授予面缺失：bot 未条件激活");

    // 幂等清理：SupportPowerBotModule.TraitEnabled 无条件 Add powerDecisions
    //（SupportPowerBotModule.cs:63-66，TraitDisabled 无清理）——重放前清空避免撞 key。
    //（ConditionalTrait 实现 INotifyCreated，ConditionalTrait.cs:41——INotifyCreated
    // 遍历可达全部条件模块；Mods.Common 类型经 ALC 隔离，宿主只按类型名匹配。）
    foreach (var t in playerActor.TraitsImplementing<INotifyCreated>())
        if (t.GetType().Name == "SupportPowerBotModule")
        {
            var pd = t.GetType().GetField("powerDecisions",
                System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.GetValue(t)!;
            pd.GetType().GetMethod("Clear")!.Invoke(pd, null);
        }

    var oldToken = (int)tokenField.GetValue(granted)!;
    var info = granted.GetType().GetField("info",
        System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!.GetValue(granted)!;
    var condition = (string)info.GetType().GetField("Condition")!.GetValue(info)!;
    playerActor.RevokeCondition(oldToken);
    var newToken = playerActor.GrantCondition(condition);
    tokenField.SetValue(granted, newToken);
}

static (byte[] Orarep, string Dump, string IsyncDump) BotRecordRun(System.IO.TextWriter diag = null)
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

    // effects 投影（T34 打回）：SendSync 时刻（SyncHash 段3 同拍——ProcessOrders 后、
    // world.Tick 前）的 world.SyncedEffects 全量 (类型,hash) 行，帧序拼入 isync dump
    // 尾部。C++ 重演侧段3 垫底（Missile 弹道等 ISync effect——actor 遍历不含 effects，
    // isync-changes 是段3 数据面唯一入产物渠道）。
    // mid 快照（T34 打回③）：同一时刻的 isync 全量快照——帧内 isync 变化按 SendSync
    // 拍拆 o（ProcessOrders 面：PowerDown toggle/PowerManager 重算等）/ t（world.Tick
    // 面：tsla 充能/harv 移动/出生帧尾任务等）两段行，C++ 侧 SyncHash 对比点（SendSync
    // = orders 后 tick 前）两侧各取所需，逐帧全等不再受 orders/tick 混合歧义拖累。
    var effectsSb = new StringBuilder();
    var midSnaps = new Dictionary<int, Dictionary<IsyncKey, string>>();
    conn.OnSendSync = (f, h, d) =>
    {
        if (f >= 1)
        {
            foreach (var se in world.SyncedEffects)
                effectsSb.Append(CultureInfo.InvariantCulture,
                    $"e {f} effect {se.GetType().Name} h {BotSyncHashOf(se)}\n");
            midSnaps[f] = IsyncSnapshot(world, applyMechanizedExclusion: false, includeActorRefs: true);
            if (diag != null)
                BotDiagSegments(world, f, h, diag);
        }
    };

    // 激活链宿主改造（票面「Player.cs:217 host-only 激活的 oracle 侧」）：无头宿主
    // 的 Game.IsHost 在 World ctor 期不可达（Game.OrderManager.Connection 的
    // LocalClient 上下文在 ctor 期未装配）——ctor 后手动补激活（等价 host 客户端
    // 的 Player.cs:217-223 路径，Activate 内自带 IBotTick/IBotRespondToAttack 收集）。
    var botPlayer = world.Players.First(p => p.InternalName == "Multi0");
    BotReplayActivationRolls(botPlayer.PlayerActor);

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
    var isyncPrev = IsyncSnapshot(world, applyMechanizedExclusion: false, includeActorRefs: true);   // Aggressor 等 Actor 引用面入 dump（T34 打回）
    for (var f = 1; f <= BotFrames; f++)
    {
om.TickImmediate();
        om.TryTick();
        world.Tick();
        ReplaySweepEffects(world);

        var s = conn.SentSyncs.LastOrDefault(x => x.Frame == f);
        sb.Append(CultureInfo.InvariantCulture, $"f {f} sync {s.SyncHash} defeat {s.DefeatState} rng {world.SharedRandom.Last} {world.SharedRandom.TotalCount} actors {world.Actors.Count()} fnv {ReplayActorFnv(world)}\n");

        var isyncMid = midSnaps[f];
        var isyncNow = IsyncSnapshot(world, applyMechanizedExclusion: false, includeActorRefs: true);
        // isync 变化拆段（T34 打回③）：o=SendSync 拍前（ProcessOrders 面）、t=SendSync
        // 拍后（world.Tick 面）——SyncHash 计算点在两段之间，C++ 侧对比前应用 o 段、
        // 对比后应用 t 段。
        foreach (var (key, val) in isyncMid)
            if (isyncPrev.TryGetValue(key, out var old) && old != val)
                isyncSb.Append(CultureInfo.InvariantCulture,
                    $"o {f} actor {key.ActorId} {key.TypeName} trait {key.TraitName}#{key.TraitInstance} field {key.FieldName} {old} -> {val}\n");
        foreach (var (key, val) in isyncNow)
            if (isyncMid.TryGetValue(key, out var old) && old != val)
                isyncSb.Append(CultureInfo.InvariantCulture,
                    $"t {f} actor {key.ActorId} {key.TypeName} trait {key.TraitName}#{key.TraitInstance} field {key.FieldName} {old} -> {val}\n");
        isyncPrev = isyncNow;

        var now = world.Actors.ToDictionary(a => a.ActorID, a => $"{a.Info.Name}");
        foreach (var id in now.Keys.Where(k => !prev.ContainsKey(k)))
        {
            // 出生格附注（T34 打回）：出生 actor 的 Mobile.FromCell（帧尾快照）——
            // C++ 重演侧 SyncHash 段2 的 Mobile hash 进面（from/to/wpos），isync dump
            // 无出生初值行（无变化），出生位由本行承载。建筑无 Mobile，取 Immobile
            // .TopLeft。均在 Mods.Common（ALC 隔离），走 ISync 遍历 + 反射读。
            var born = world.Actors.First(a => a.ActorID == id);
            var at = "";
            foreach (var t in born.TraitsImplementing<ISync>())
                if (t.GetType().Name == "Mobile")
                {
                    var fromCell = (CPos)t.GetType().GetProperty("FromCell")!.GetValue(t)!;
                    var facing = ((WAngle)t.GetType().GetProperty("Facing")!.GetValue(t)!).Angle;
                    // CenterPosition（T34 打回③）：生产出口 eject 的出生位带 subcell 偏移
                    // （e1 出 wep 后 21803,10880 vs 格心 22016,11776——Mobile hash 进面，
                    // isync 无出生初值行，由本行承载）。
                    var center = (WPos)t.GetType().GetProperty("CenterPosition")!.GetValue(t)!;
                    at = $" @{fromCell.X},{fromCell.Y}:{facing}|{center.X},{center.Y},{center.Z}";
                }
                else if (t.GetType().Name is "Immobile" or "Building")
                {
                    var topLeft = (CPos)t.GetType().GetProperty("TopLeft")!.GetValue(t)!;
                    at = $" @{topLeft.X},{topLeft.Y}";
                }
            sb.Append(CultureInfo.InvariantCulture, $"actor+ f{f} {id}:{now[id]}{at}\n");
        }
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
    isyncSb.Append(effectsSb);
    return (normalized, sb.ToString(), isyncSb.ToString());
}

// —— bot-diag（T34 打回）：跨进程 sync 分歧分段定位 ——
// SendSync 时刻（ProcessOrders 后、world.Tick 前）逐项复刻 World.SyncHash() 的四段
// 聚合（actors 段/ISync 段/effects 段/rng+players 段），明细行带 n 序与单 trait hash。
// 两独立进程各跑一次 bot-diag，diff 明细即得首分歧贡献项（actor/trait/hash 三元组）。
const int BotDiagDetailFrames = 660;   // 明细行帧窗（逐帧全等攻坚期全开；总值行全帧）

static readonly Func<ISync, int> BotSyncHashOf = MakeBotSyncHashOf();

static Func<ISync, int> MakeBotSyncHashOf()
{
    var m = typeof(Sync).GetMethod("Hash", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static,
        [typeof(ISync)]) ?? throw new InvalidOperationException("Sync.Hash 反射失败");
    return (Func<ISync, int>)Delegate.CreateDelegate(typeof(Func<ISync, int>), m);
}

static void BotDiagSegments(World world, int frame, int sentHash, System.IO.TextWriter diag)
{
    var n = 0;
    var ret = 0;
    var detail = frame <= BotDiagDetailFrames;

    // 段1：actors（World.cs:490-491）
    foreach (var a in world.Actors)
        ret += n++ * (int)(1 + a.ActorID) * Sync.HashActor(a);
    diag.WriteLine($"D f{frame} seg1 {ret} n {n}");

    // 段2：ISync trait（World.cs:494-496；TraitsImplementing<ISync> 序=SyncHashes 数组序——
    // 同一 ctor 循环构建，见 Actor.cs:193/207）
    foreach (var actor in world.ActorsHavingTrait<ISync>())
        foreach (var t in actor.TraitsImplementing<ISync>())
        {
            var h = BotSyncHashOf(t);
            if (detail)
                diag.WriteLine($"D f{frame} a {actor.ActorID} {actor.Info.Name} t {t.GetType().Name} h {h} n {n}");
            ret += n++ * (int)(1 + actor.ActorID) * h;
        }
    diag.WriteLine($"D f{frame} seg12 {ret} n {n}");

    // 段3：synced effects（World.cs:499-500）
    foreach (var se in world.SyncedEffects)
    {
        var h = BotSyncHashOf(se);
        if (detail)
            diag.WriteLine($"D f{frame} e {se.GetType().Name} h {h} n {n}");
        ret += n++ * h;
    }

    // 段4：SharedRandom.Last（World.cs:503）+ UnlockedRenderPlayer 段（:506-508）
    var players = 0;
    foreach (var p in world.Players)
        if (p.UnlockedRenderPlayer)
            players += Sync.HashPlayer(p);
    var total = ret + world.SharedRandom.Last + players;
    diag.WriteLine($"D f{frame} seg123 {ret} n {n} rng {world.SharedRandom.Last} players {players} total {total} sent {sentHash} {(total == sentHash ? "OK" : "MISMATCH")}");
}

internal static int BotDiag(string openraRoot, string diagOut)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);
    Game.Sound = new Sound(new SandboxStubPlatform(), new SoundSettings());
    Game.Sound.DisableAllSounds = true;
    ChromeMetricsOnce();
    using var w = new System.IO.StreamWriter(diagOut, false, new UTF8Encoding(false));
    // seed 探针：.NET string.GetHashCode per-process 随机化的直接存在性证据
    w.WriteLine($"# bot-diag seed-probe \"openra\".GetHashCode={"openra".GetHashCode()} \"units\".GetHashCode={"units".GetHashCode()}");
    BotRecordRun(diag: w);
    Console.WriteLine($"bot-diag: {diagOut}");
    return ExitOk;
}
}
