// 基线漂移单独给退出码 2，脚本可区分“基准漂移”与“一般宿主错误”
sealed class BaselineMismatchException(string message) : Exception(message);

// combat 模式假序列（无头障碍 9，探针同款）：Length=1/Tick=40 → SpriteEffect 一 tick
// 后自删；GetSprite 返回 null（无头不渲染）。未知序列名仍抛错——忠实。
sealed class FakeSequence(string name) : OpenRA.Graphics.ISpriteSequence
{
    public string Name => name;
    public int Length => 1;
    public int Facings => 1;
    public int Tick => 40;
    public int ZOffset => 0;
    public int ShadowZOffset => 0;
    Rectangle OpenRA.Graphics.ISpriteSequence.Bounds => default;
    public bool IgnoreWorldTint => false;
    public float Scale => 1f;
    public void ResolveSprites(OpenRA.Graphics.SpriteCache cache) { }
    public OpenRA.Graphics.Sprite GetSprite(int frame) => null;
    public OpenRA.Graphics.Sprite GetSprite(int frame, WAngle facing) => null;
    public (OpenRA.Graphics.Sprite Sprite, WAngle Rotation) GetSpriteWithRotation(int frame, WAngle facing) => (null, WAngle.Zero);
    public OpenRA.Graphics.Sprite GetShadow(int frame, WAngle facing) => null;
    public float GetAlpha(int frame) => 1f;
}

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

    // economy 模式装配缓存（t13-scratch 探针同款；同进程双 pass 恒等已实证）
    public static (ModData ModData, IReadOnlyPackage Package)? EconomyAsm;
    // combat 模式装配缓存（ModData + ore-lord 包 + 武器表 + 最小规则 actor 集 +
    // 反射句柄包一次装配、每 pass 新 World 新 Map；探针实证同进程多 pass 恒等）
    public static (ModData ModData, IReadOnlyPackage Package)? CombatAsm;
    public static Dictionary<string, OpenRA.GameRules.WeaponInfo>? CombatWeapons;
    public static Dictionary<string, ActorInfo>? CombatActors;
    public static (MethodInfo AttackTarget, Type AttackFrontal, Type AttackTurreted, MethodInfo TraitGeneric,
        PropertyInfo Hp, PropertyInfo MaxHp, PropertyInfo FromCell, PropertyInfo Facing,
        PropertyInfo Center, object DefaultSource, Type FacingInit)? CombatReflection;

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
        // T34：v2rl/v2 爆炸 Shake 的 ScreenShaker.Tick 依赖 gameRenderer（无头 null）。
        "ScreenShakerInfo",
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

/// <summary>
/// EchoConnection 语义（本地回环、帧 +1 投影，Connection.cs:42-97）+ NetworkConnection 的
/// ReplayRecorder 挂接语义（Connection.cs:285-364：immediate→[0]、Ack 帧→[应用帧]、SendSync→17 字节）。
/// </summary>
sealed class RecordingEchoConnection : IConnection
{
    public const int LocalClient = 1;
    readonly Queue<(int Frame, int SyncHash, ulong DefeatState)> sync = [];
    readonly Queue<(int Frame, OrderPacket Orders)> orders = [];
    readonly Queue<OrderPacket> immediateOrders = [];
    bool disposed;

    public ReplayRecorder Recorder { get; }
    public List<(int Frame, int SyncHash, ulong DefeatState)> SentSyncs { get; } = [];

    public RecordingEchoConnection(Func<string> chooseFilename)
    {
        Recorder = new ReplayRecorder(chooseFilename);
    }

    int IConnection.LocalClientId => LocalClient;

    void IConnection.StartGame()
    {
        // EchoConnection：注入空帧填 +1 投影缝（Connection.cs:52-56）
        var empty = new OrderPacket([]);
        orders.Enqueue((0, empty));
        Recorder.Receive(LocalClient, empty.Serialize(1));   // 帧 pacing 空包（服务器对每客户端每帧必派发）
    }

    void IConnection.Send(int frame, IEnumerable<Order> o)
    {
        var packet = new OrderPacket(o);
        orders.Enqueue((frame, packet));
        // 记录「应用帧」= 发出帧 + 投影（NetworkConnection 走服务器 Ack 帧，Connection.cs:344-345）
        Recorder.Receive(LocalClient, packet.Serialize(frame + 1));
    }

    void IConnection.SendImmediate(IEnumerable<Order> o)
    {
        var packet = new OrderPacket(o);
        immediateOrders.Enqueue(packet);
        Recorder.Receive(LocalClient, packet.Serialize(0));
    }

    void IConnection.SendSync(int frame, int syncHash, ulong defeatState)
    {
        var s = (frame, syncHash, defeatState);
        sync.Enqueue(s);
        SentSyncs.Add(s);
        Recorder.Receive(LocalClient, OrderIO.SerializeSync(s));   // Connection.cs:302
    }

    void IConnection.Receive(OrderManager orderManager)
    {
        while (immediateOrders.TryDequeue(out var i))
        {
            orderManager.ReceiveImmediateOrders(LocalClient, i);
            if (disposed)
                break;
        }

        while (orders.TryDequeue(out var o))
            orderManager.ReceiveOrders(LocalClient, (o.Frame + 1, o.Orders));

        while (sync.TryDequeue(out var s))
            orderManager.ReceiveSync(s);
    }

    void IDisposable.Dispose()
    {
        disposed = true;
        Recorder.Dispose();
    }
}

/// <summary>
/// 照抄 ReplayConnection（ReplayConnection.cs:19-135）：ctor 聚合 chunk；Receive 先排干本地 sync
/// 队列（成为 syncForFrame 参照）再按 orderLatency 投喂 chunk。差异：frame-0 引导单（SyncInfo/
/// StartGame）不投喂——无头宿主不能让 UnitOrders 的 StartGame 走 Game.StartGame（渲染链），
/// 世界重建由宿主手工完成（与 C++ T16 重演器同构）。
/// </summary>
sealed class ReplayPumpConnection : IConnection
{
    sealed class Chunk
    {
        public int Frame;
        public List<(int Client, byte[] Packet)> Packets = [];
    }

    readonly Queue<Chunk> chunks = [];
    readonly Queue<(int Frame, int SyncHash, ulong DefeatState)> localSync = [];
    const int OrderLatency = 3;   // ra 默认 "default" 速度（mod.yaml:296），只影响投喂提前量

    public Dictionary<int, ReplaySyncEntry> RecordedSyncs { get; } = [];
    public List<(int Frame, int SyncHash, ulong DefeatState)> SentSyncs { get; } = [];

    public ReplayPumpConnection(string path)
    {
        var packets = new List<(int Client, byte[] Packet)>();
        var chunk = new Chunk();
        foreach (var r in ReplayReadRecordsOf(path))
        {
            packets.Add((r.Client, r.Data));
            if (r.Data.Length > 4 && (r.Data[4] == (byte)OrderType.Disconnect || r.Data[4] == (byte)OrderType.SyncHash))
            {
                if (r.Data.Length == 17 && r.Data[4] == (byte)OrderType.SyncHash)
                    RecordedSyncs[r.Frame] = new ReplaySyncEntry(BitConverter.ToInt32(r.Data, 5), BitConverter.ToUInt64(r.Data, 9));
                continue;   // sync/disconnect 不终结 chunk（ReplayConnection.cs:59-60）
            }

            if (r.Frame == 0)
                continue;   // 引导/immediate 单：累积进下一 chunk 但不终结（:62-75）
            chunk.Frame = r.Frame;
            chunk.Packets = packets.ToList();
            packets.Clear();
            chunks.Enqueue(chunk);
            chunk = new Chunk();
        }
    }

    static List<(long Offset, int Client, int Frame, byte[] Data)> ReplayReadRecordsOf(string path)
    {
        var records = new List<(long, int, int, byte[])>();
        using var fs = File.OpenRead(path);
        while (fs.Position < fs.Length)
        {
            var offset = fs.Position;
            var client = fs.ReadInt32();
            if (client == ReplayMetadata.MetaStartMarker)
                break;
            var len = fs.ReadInt32();
            var data = fs.ReadBytes(len);
            records.Add((offset, client, BitConverter.ToInt32(data, 0), data));
        }
        return records;
    }

    int IConnection.LocalClientId => -1;

    void IConnection.StartGame() { }
    void IConnection.Send(int frame, IEnumerable<Order> orders) { }
    void IConnection.SendImmediate(IEnumerable<Order> orders) { }

    void IConnection.SendSync(int frame, int syncHash, ulong defeatState)
    {
        localSync.Enqueue((frame, syncHash, defeatState));
        SentSyncs.Add((frame, syncHash, defeatState));
    }

    void IConnection.Receive(OrderManager orderManager)
    {
        while (localSync.Count != 0)
            orderManager.ReceiveSync(localSync.Dequeue());

        while (chunks.Count != 0 && chunks.Peek().Frame <= orderManager.NetFrameNumber + OrderLatency)
        {
            foreach (var o in chunks.Dequeue().Packets)
            {
                if (OrderIO.TryParseSync(o.Packet, out var sync))
                    orderManager.ReceiveSync(sync);   // 与本地 hash 比对 → 不等则 OutOfSync（OrderManager.cs:161-170）
                else if (OrderIO.TryParseOrderPacket(o.Packet, out var orders) && orders.Frame != 0)
                    orderManager.ReceiveOrders(o.Client, orders);
            }
        }
    }

    void IDisposable.Dispose() { }
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


sealed record ReplaySyncEntry(int Hash, ulong Defeat);

// 无头宿主引导样板收敛（P2-7）：程序集自检 → EngineDir 一次覆写（上游 Platform 对
// 二次 OverrideEngineDir 直接 throw，进程内只覆写一次）→ Log 通道注册（AddChannel
// 幂等，上游 Log.cs:130-131）→ 设置初始化。各场景函数的个性面（Game.Sound 桩装配、
// ChromeMetrics 一次初始化）仍留在场景侧。
#nullable disable

static class HeadlessHost
{
    public static void Boot(string openraRoot, bool soundChannel = false)
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
        if (soundChannel) Log.AddChannel("sound", null);
        Game.InitializeSettings(Arguments.Empty);
    }
}

internal static partial class Program
{
    // ChromeMetrics 进程级一次初始化（World ctor 的 UnitOrderGenerator 构造期读取——
    // t07 无头障碍 5；production/replay 场景共用）
    internal static void ChromeMetricsOnce()
    {
        if (State.ChromeInitialized) return;
        var bootMods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
        OpenRA.Widgets.ChromeMetrics.Initialize(new ModData(bootMods["ra"], bootMods));
        State.ChromeInitialized = true;
    }
}
