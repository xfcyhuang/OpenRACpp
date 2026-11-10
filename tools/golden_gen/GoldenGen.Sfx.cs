// sfx 模式（T19/issue #31）：世界事件音效接入 oracle——驱动真上游 Sound 门面
// （Sound.cs Play/PlayToPlayer/PlayPredefined/PlayNotification 全语义）+ 录制引擎 +
// 「名字回注」假文件系统/加载器（最终名经 PCM 字节进引擎，录制层拿得到）+ 真 ra
// notifications/voices SoundInfo + 已播种 Game.CosmeticRandom（池洗牌袋确定化）。
// World/Player/Actor 为最小反射注入体：PlayPredefined 只触 LocalPlayer（auto-property
// backing field）、Selection（ISelection 代理，Contains 查脚本选区）、ActorID/World 字段；
// Game.Settings 注入仅 Sound 段（InternalSoundVolume = 0.5 × modifier）。
// 转储与 tests/sfx.cpp 的 sfx_dump_sequence 逐字节同形（float "R" ≍ std::to_chars）。

using System.Runtime.CompilerServices;

internal static partial class Program
{
    const string SfxBaselineCommit = "1f594e32065615ed1847962c80ecfe6e868a11c3";
    const int SfxSeed = 20261107;

    sealed class SfxStubPlatform : IPlatform
    {
        readonly SfxRecordingEngine engine;
        public SfxStubPlatform(SfxRecordingEngine engine) => this.engine = engine;
        public IPlatformWindow CreateWindow(Size size, WindowMode windowMode, float scaleModifier, int vertexBatchSize, int indexBatchBatchSize, int videoDisplay, GLProfile profile)
            => throw new NotImplementedException();
        public ISoundEngine CreateSound(string device) => engine;
        public IFont CreateFont(byte[] data) => throw new NotImplementedException();
    }

    sealed class SfxRecordingSoundSource : ISoundSource
    {
        public string Name = "";
        public void Dispose() { }
    }

    sealed class SfxRecordingSound : ISound
    {
        public bool Stopped;
        public float Held;
        public float Volume { get => Held; set => Held = value; }
        public float SeekPosition => 0f;
        public bool Complete => Stopped;
        public void SetPosition(WPos pos) { }
    }

    sealed class SfxRecordingEngine : ISoundEngine
    {
        public readonly List<string> Log = [];

        public ISoundSource AddSoundSourceFromMemory(byte[] data, int channels, int sampleBits, int sampleRate)
            => new SfxRecordingSoundSource { Name = Encoding.UTF8.GetString(data) };

        public ISound Play2D(ISoundSource sound, bool loop, bool relative, WPos pos, float volume, bool attenuateVolume)
        {
            var name = (sound as SfxRecordingSoundSource)?.Name ?? "";
            Log.Add($"  play2d name={name} pos={pos.X},{pos.Y},{pos.Z} loop={(loop ? 1 : 0)}" +
                    $" rel={(relative ? 1 : 0)} vol={FmtFloatR(volume)} atten={(attenuateVolume ? 1 : 0)}");
            return new SfxRecordingSound();
        }

        public void StopSound(ISound sound)
        {
            Log.Add("  stopsound");
            if (sound is SfxRecordingSound s)
                s.Stopped = true;
        }

        public ISound Play2DStream(Stream stream, int channels, int sampleBits, int sampleRate, bool loop, bool relative, WPos pos, float volume) => null;
        public SoundDevice[] AvailableDevices() => [];
        public bool Dummy => true;
        public float Volume { get; set; }
        public void PauseSound(ISound sound, bool paused) { }
        public void SetAllSoundsPaused(bool paused) { }
        public void StopAllSounds() { }
        public void SetListenerPosition(WPos position) { }
        public void SetSoundVolume(float volume, ISound music, ISound video) { }
        public void SetSoundLooping(bool looping, ISound sound) { }
        public void SetSoundPosition(ISound sound, WPos position) { }
        public void Dispose() { }
    }

    sealed class SfxFakeFileSystem : IReadOnlyFileSystem
    {
        static Stream NameStream(string name)
        {
            var bytes = Encoding.UTF8.GetBytes(name);
            return new MemoryStream(bytes);
        }

        public Stream Open(string filename) => NameStream(filename);
        public bool TryOpen(string filename, out Stream s) { s = NameStream(filename); return true; }
        public bool Exists(string filename) => true;
        public bool TryGetPackageContaining(string path, out IReadOnlyPackage package, out string filename)
        {
            package = null;
            filename = null;
            return false;
        }

        public bool IsExternalFile(string filename) => false;
    }

    sealed class SfxFakeLoader : ISoundLoader
    {
        public bool TryParseSound(Stream stream, out ISoundFormat sound)
        {
            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            sound = new SfxFakeFormat(Encoding.UTF8.GetString(ms.ToArray()));
            return true;
        }
    }

    sealed class SfxFakeFormat : ISoundFormat
    {
        readonly byte[] bytes;
        public SfxFakeFormat(string name) => bytes = Encoding.UTF8.GetBytes(name);
        public int Channels => 1;
        public int SampleBits => 16;
        public int SampleRate => 22050;
        public float LengthInSeconds => 0f;
        public Stream GetPCMInputStream() => new MemoryStream(bytes);
        public void Dispose() { }
    }

    // ISelection 代理：Contains 只查脚本选区（Add/Remove 等不会走到）。
    sealed class SfxProxySelection : ISelection
    {
        public readonly HashSet<Actor> Selected = [];
        public int Hash => 0;
        public IReadOnlyCollection<Actor> Actors => Selected;
        public void Add(Actor a) => Selected.Add(a);
        public void Remove(Actor a) => Selected.Remove(a);
        public bool Contains(Actor a) => Selected.Contains(a);
        public void Combine(World world, IEnumerable<Actor> newSelection, bool isCombine, bool isClick) { }
        public void Clear() => Selected.Clear();
        public bool RolloverContains(Actor a) => false;
        public void SetRollover(IEnumerable<Actor> actors) { }
    }

    static readonly FieldInfo SfxFiActorId = typeof(Actor).GetField("ActorID")!;
    static readonly FieldInfo SfxFiActorWorld = typeof(Actor).GetField("World")!;
    static readonly FieldInfo SfxFiPlayerWorld = typeof(Player).GetField(
        "<World>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance)!;
    static readonly FieldInfo SfxFiWorldLocalPlayer = typeof(World).GetField(
        "<LocalPlayer>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance)!;
    static readonly FieldInfo SfxFiWorldSelection = typeof(World).GetField("Selection")!;
    static readonly FieldInfo SfxFiSettingsSound = typeof(Settings).GetField("Sound")!;

    static Actor SfxMakeActor(World world, uint actorId)
    {
        var actor = (Actor)RuntimeHelpers.GetUninitializedObject(typeof(Actor));
        SfxFiActorId.SetValue(actor, actorId);
        SfxFiActorWorld.SetValue(actor, world);
        return actor;
    }

    static Player SfxMakePlayer(World world)
    {
        var player = (Player)RuntimeHelpers.GetUninitializedObject(typeof(Player));
        SfxFiPlayerWorld.SetValue(player, world);
        return player;
    }

    static Dictionary<string, SoundInfo> SfxLoadSoundInfos(string openraRoot, string relPath)
    {
        var nodes = MiniYaml.FromFile(Path.Combine(openraRoot, relPath)).ToList();
        var dict = new Dictionary<string, SoundInfo>(StringComparer.Ordinal);
        foreach (var node in nodes)
            dict[node.Key.ToLowerInvariant()] = new SoundInfo(node.Value);
        return dict;
    }

    static byte[] SfxDumpSequence(string openraRoot)
    {
        // Game.Settings 注入（Sound 门面读 Settings.Sound.SoundVolume；默认 0.5）。
        var settings = (Settings)RuntimeHelpers.GetUninitializedObject(typeof(Settings));
        SfxFiSettingsSound.SetValue(settings, new SoundSettings());
        Game.Settings = settings;

        var engine = new SfxRecordingEngine();
        var sound = new Sound(new SfxStubPlatform(engine), new SoundSettings());
        sound.Initialize([new SfxFakeLoader()], new SfxFakeFileSystem());
        Game.CosmeticRandom = new MersenneTwister(SfxSeed);

        var voices = SfxLoadSoundInfos(openraRoot, Path.Combine("mods", "ra", "audio", "voices.yaml"));
        var notifications = SfxLoadSoundInfos(openraRoot, Path.Combine("mods", "ra", "audio", "notifications.yaml"));
        var ruleset = new Ruleset(
            new Dictionary<string, ActorInfo>(),
            new Dictionary<string, WeaponInfo>(),
            voices,
            notifications,
            new Dictionary<string, MusicInfo>(),
            null,
            null);

        var selection = new SfxProxySelection();
        var world = (World)RuntimeHelpers.GetUninitializedObject(typeof(World));
        SfxFiWorldLocalPlayer.SetValue(world, null);
        SfxFiWorldSelection.SetValue(world, selection);
        var local = SfxMakePlayer(world);
        var foe = SfxMakePlayer(world);
        var actor5 = SfxMakeActor(world, 5);
        var actor7 = SfxMakeActor(world, 7);
        var actor9 = SfxMakeActor(world, 9);

        var lines = new List<string>
        {
            $"# baseline {SfxBaselineCommit} sfx sequence dump v1",
            $"# seed {SfxSeed.ToString(CultureInfo.InvariantCulture)}",
        };

        var step = 0;
        void EmitStep(string head, Func<string> run)
        {
            lines.Add(head);
            var ret = run();
            lines.AddRange(engine.Log);
            engine.Log.Clear();
            lines.Add($"  ret {ret}");
            ++step;
        }

        static string RetSound(ISound s) => s != null ? "sound" : "null";
        static string RetPredefined(Func<bool> run)
        {
            try
            {
                return run() ? "true" : "false";
            }
            catch (Exception ex)
            {
                return $"error {ex.Message}";
            }
        }

        static string Head(int stepIdx, string kind) =>
            $"step {stepIdx.ToString(CultureInfo.InvariantCulture)} {kind}";

        string PredefHead(string st, Player player, string playerLabel, int voiced, bool selected, string type, string def,
            string variant, bool rel, WPos pos, float volmod, bool atten) =>
            $"step {step.ToString(CultureInfo.InvariantCulture)} predefined st={st}" +
            $" player={playerLabel}" +
            $" voiced={(voiced >= 0 ? voiced.ToString(CultureInfo.InvariantCulture) : "-1")}" +
            $" selected={(selected ? 1 : 0)} type={type} def={def} variant={variant}" +
            $" rel={(rel ? 1 : 0)} pos={pos.X},{pos.Y},{pos.Z} volmod={FmtFloatR(volmod)} atten={(atten ? 1 : 0)}";

        bool Predef(string st, Player player, Actor voiced, string type, string def, string variant,
            bool rel, WPos pos, float volmod, bool atten) =>
            sound.PlayPredefined(
                st == "World" ? SoundType.World : SoundType.UI,
                ruleset, player, voiced, type, def, variant == "-" ? null : variant, rel, pos,
                volmod, atten);

        void SetLocal(Player p)
        {
            SfxFiWorldLocalPlayer.SetValue(world, p);
            lines.Add(p != null ? "local 2" : "local -");
        }

        var wpos0 = new WPos(0, 0, 0);
        var wposFire = new WPos(1024, 2048, 0);
        var wposPlace = new WPos(4096, 2048, 0);
        var wposVoice = new WPos(2048, 4096, 0);

        SetLocal(local);

        EmitStep(Head(step, "playtoplayer name=placedown1.aud player=2 pos=4096,2048,0"),
            () => RetSound(sound.PlayToPlayer(SoundType.World, local, "placedown1.aud", wposPlace)));
        EmitStep(Head(step, "playtoplayer name=placedown1.aud player=3 pos=4096,2048,0"),
            () => RetSound(sound.PlayToPlayer(SoundType.World, foe, "placedown1.aud", wposPlace)));
        EmitStep(Head(step, "play name=explosion1.aud pos=1024,2048,0 volmod=1"),
            () => RetSound(sound.Play(SoundType.World, "explosion1.aud", wposFire)));
        EmitStep(Head(step, "play name= pos=0,0,0 volmod=1"),
            () => RetSound(sound.Play(SoundType.World, "", wpos0)));

        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "RadarUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "RadarUp", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "RadarUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "RadarUp", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "CashTickUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "CashTickUp", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "CashTickUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "CashTickUp", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "ChatLine", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "ChatLine", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "sounds", "ChatLine", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "sounds", "ChatLine", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "speech", "HarvesterAttack", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "speech", "HarvesterAttack", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "speech", "NoSuchDef", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "speech", "NoSuchDef", null, true, wpos0, 1f, false)));
        EmitStep(PredefHead("World", null, "-", -1, false, "notype", "RadarUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", null, null, "notype", "RadarUp", null, true, wpos0, 1f, false)));

        EmitStep(PredefHead("World", null, "-", 5, false, "genericvoice", "Select", "allies", false, wposVoice, 1f, true),
            () => RetPredefined(() => Predef("World", null, actor5, "genericvoice", "Select", "allies", false, wposVoice, 1f, true)));
        EmitStep(PredefHead("World", null, "-", 7, true, "genericvoice", "Select", "allies", false, wpos0, 1f, true),
            () => { selection.Selected.Add(actor7); return RetPredefined(() => Predef("World", null, actor7, "genericvoice", "Select", "allies", false, wpos0, 1f, true)); });
        EmitStep(PredefHead("World", null, "-", 7, true, "genericvoice", "Select", "allies", false, wpos0, 1f, true),
            () => RetPredefined(() => Predef("World", null, actor7, "genericvoice", "Select", "allies", false, wpos0, 1f, true)));
        EmitStep(PredefHead("World", null, "-", 9, false, "genericvoice", "Action", "allies", false, wpos0, 1f, true),
            () => RetPredefined(() => Predef("World", null, actor9, "genericvoice", "Action", "allies", false, wpos0, 1f, true)));
        EmitStep(PredefHead("World", null, "-", 5, true, "genericvoice", "Action", "allies", false, wpos0, 1f, true),
            () => { selection.Selected.Add(actor5); return RetPredefined(() => Predef("World", null, actor5, "genericvoice", "Action", "allies", false, wpos0, 1f, true)); });

        // 观战：LocalPlayer = null → 带 player 的调用静默但恒 true。
        SetLocal(null);
        EmitStep(PredefHead("World", local, "2", -1, false, "sounds", "RadarUp", "-", true, wpos0, 1f, false),
            () => RetPredefined(() => Predef("World", local, null, "sounds", "RadarUp", null, true, wpos0, 1f, false)));
        EmitStep(Head(step, "notification player=2 type=Sounds def=RadarUp variant=-"),
            () => RetPredefined(() => sound.PlayNotification(ruleset, local, "Sounds", "RadarUp", null)));

        SetLocal(local);
        EmitStep(Head(step, "notification player=2 type=Sounds def=RadarUp variant=-"),
            () => RetPredefined(() => sound.PlayNotification(ruleset, local, "Sounds", "RadarUp", null)));

        return new UTF8Encoding(false).GetBytes(string.Join("\n", lines) + "\n");
    }
}
