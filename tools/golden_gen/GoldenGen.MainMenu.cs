using OpenRA.FileFormats;
using OpenRA.Graphics;
using OpenRA.Widgets;

internal static partial class Program
{
// —— mainmenu 模式（T26/issue #35）：主菜单与设置面板 oracle ——
// 三份 dump（模式组单次装配共享）+ 五份 chrome 语料原样入库（GPL COPYING 随带）：
//   tree.txt      装载树 bounds 真源（chrome 模式同款公式：MiniYaml 结构序 +
//                 IntegerExpression.Evaluate 按 Widget.Initialize substitutions，
//                 顶层挂哑 root PARENT=0、窗口 1024x768）——mainmenu.yaml + settings.yaml
//                 + display/audio/hotkeys 三页，段头 file <name>。
//   nav.txt       真 MainMenuLogic 驱动（MainMenuLogic.cs）：WidgetLoader 真装载
//                 mainmenu.yaml（Renderer 桩供 WINDOW substitutions / Resolution）+
//                 ModData ra + MapCache.LoadMaps + 调优 settings（FetchNews/prompt/
//                 CheckVersion 关）→ ctor 落 Main；逐步驱动纯态按钮 OnClick
//                 （SINGLEPLAYER/BACK/EXTRAS/MAP_EDITOR/QUIT），dump 菜单可见性 +
//                 真派生按钮禁用态（SKIRMISH/MISSIONS/LOAD/CONTENT/LOAD_MAP）。
//                 面板打开类按钮（SETTINGS/MULTIPLAYER/SKIRMISH start/MISSIONS/
//                 REPLAYS/MUSIC/CREDITS/NEW_MAP/LOAD_MAP/ENCYCLOPEDIA）不可无头驱动
//                 ——Game.OpenWindow→worldRenderer（需真 WorldRenderer/GL 窗口）、
//                 StartSkirmishGame→本地服务器；导航序列由源码精读 + C++ ctest 断言
//                 （偏离登记）。QUIT 经 Game.Exit → state 反射 dump。
//   bindings.txt  设置面板三页接线真源：SettingsUtils.BindSliderPref/BindCheckboxPref
//                 真调用（真 SliderWidget/CheckboxWidget + 真 Game.Settings 字段，
//                 OnChange 委托链反射触发）；Sound.SoundVolume 真链（记录引擎
//                 SetSoundVolume 调用）；MuteAudio/UnmuteAudio 真调用（master 0/1）；
//                 Display leave 公式（int.TryParse + 五字段 needsRestart，公式照抄
//                 DisplaySettingsLogic.cs:274-285）；ButtonStride 真 FieldLoader int2 +
//                 tab 克隆 bounds 公式 + 真 ListLayout.AdjustChildren（Mods.Common
//                 ListLayout.cs:32-45，settings-audio.yaml ROW 集真构造）；真
//                 HotkeyManager rekey 流（Set/GetFirstDuplicate/hotkeySettings.Yaml
//                 节点——ra 全 174 定义装载态）。MODE 下拉选项表与 fluent 文本不进
//                 oracle（FluentProvider 无头不可用），由 C++ 按源码常量对拍。

    const string MainMenuGoldenRelDir = "tests/golden/mainmenu";
    const int MainMenuWindowWidth = 1024;
    const int MainMenuWindowHeight = 768;

    static readonly string[] MainMenuChromeFiles =
    [
        "mainmenu.yaml",
        "settings.yaml",
        "settings-display.yaml",
        "settings-audio.yaml",
        "settings-hotkeys.yaml",
    ];

    // —— Renderer 桩：sealed Renderer 的 ctor 只走 IPlatform.CreateWindow 与
    // IGraphicsContext 的 Create* 族，全部返回桩；分辨率面固定 1024x768。
    sealed class MenuStubVertexBuffer<T> : IVertexBuffer<T> where T : struct
    {
        public void Bind() { }
        public void SetData(T[] vertices, int length) { }
        public void SetData(ref T[] vertices, int length) { }
        public void SetData(T[] vertices, int offset, int start, int length) { }
        public void Dispose() { }
    }

    sealed class MenuStubIndexBuffer : IIndexBuffer
    {
        public void Bind() { }
        public void Dispose() { }
    }

    sealed class MenuStubTexture : ITexture
    {
        public Size Size => new(0, 0);
        public TextureScaleFilter ScaleFilter { get; set; }
        public void SetData(byte[] colors, int width, int height) { }
        public void SetDataFromReadBuffer(Rectangle rect) { }
        public void SetFloatData(float[] data, int width, int height) { }
        public void SetSubData(byte[] colors, int xoffset, int yoffset, int width, int height) { }
        public byte[] GetData() => [];
        public void Dispose() { }
    }

    sealed class MenuStubShader : IShader
    {
        public void SetBool(string name, bool value) { }
        public void SetVec(string name, float x) { }
        public void SetVec(string name, float x, float y) { }
        public void SetVec(string name, float x, float y, float z) { }
        public void SetVec(string name, ReadOnlyMemory<float> vec, int length) { }
        public void SetTexture(string param, ITexture texture) { }
        public void SetMatrix(string param, float[] mtx) { }
        public void PrepareRender() { }
        public void Bind() { }
    }

    sealed class MenuStubFrameBuffer : IFrameBuffer
    {
        public ITexture Texture => new MenuStubTexture();
        public void Bind() { }
        public void Unbind() { }
        public void EnableScissor(Rectangle rect) { }
        public void DisableScissor() { }
        public void Dispose() { }
    }

    sealed class MenuStubGraphicsContext : IGraphicsContext
    {
        public string GLVersion => "stub";
        public IVertexBuffer<T> CreateEmptyVertexBuffer<T>(int size) where T : struct => new MenuStubVertexBuffer<T>();
        public IVertexBuffer<T> CreateVertexBuffer<T>(T[] data, bool dynamic = true) where T : struct => new MenuStubVertexBuffer<T>();
        public T[] CreateVertices<T>(int size) where T : struct => new T[size];
        public IIndexBuffer CreateIndexBuffer(uint[] indices) => new MenuStubIndexBuffer();
        public ITexture CreateTexture() => new MenuStubTexture();
        public IFrameBuffer CreateFrameBuffer(Size s) => new MenuStubFrameBuffer();
        public IFrameBuffer CreateFrameBuffer(Size s, Color clearColor) => new MenuStubFrameBuffer();
        public IShader CreateShader(IShaderBindings shaderBindings) => new MenuStubShader();
        public void EnableScissor(int x, int y, int width, int height) { }
        public void DisableScissor() { }
        public void Present() { }
        public void DrawPrimitives(PrimitiveType pt, int firstVertex, int numVertices) { }
        public void DrawElements(int numIndices, int offset) { }
        public void Clear() { }
        public void EnableDepthBuffer() { }
        public void DisableDepthBuffer() { }
        public void ClearDepthBuffer() { }
        public void SetBlendMode(BlendMode mode) { }
        public void SetVSyncEnabled(bool enabled) { }
        public void Dispose() { }
    }

    // 记录引擎：SetSoundVolume/Volume 调用留痕（bindings.txt 的 T20 链对拍锚）
    sealed class MenuRecordingSoundEngine : ISoundEngine
    {
        public readonly List<string> Log = [];
        float volume;
        public bool Dummy => true;
        public float Volume
        {
            get => volume;
            set
            {
                volume = value;
                Log.Add($"master={value.ToString("F6", CultureInfo.InvariantCulture)}");
            }
        }

        public SoundDevice[] AvailableDevices() => [];
        public ISoundSource AddSoundSourceFromMemory(byte[] data, int channels, int soundBits, int soundRate) => new SandboxStubSoundSource();
        public ISound Play2D(ISoundSource sound, bool loop, bool relative, WPos pos, float vol, bool attenuateVolume) => null;
        public ISound Play2DStream(Stream stream, int channels, int soundBits, int soundRate, bool loop, bool relative, WPos pos, float volume) => null;
        public void SetSoundVolume(float volume, ISound music, ISound video) =>
            Log.Add($"setSoundVolume={volume.ToString("F6", CultureInfo.InvariantCulture)}");
        public void PauseSound(ISound sound, bool paused) { }
        public void StopSound(ISound sound) { }
        public void SetAllSoundsPaused(bool paused) { }
        public void StopAllSounds() { }
        public void SetListenerPosition(WPos position) { }
        public void SetSoundLooping(bool looping, ISound sound) { }
        public void SetSoundPosition(ISound sound, WPos position) { }
        public void Dispose() { }
    }

    sealed class MenuRecordingSoundPlatform : IPlatform
    {
        public readonly MenuRecordingSoundEngine Engine = new();
        public IPlatformWindow CreateWindow(Size size, WindowMode windowMode, float scaleModifier,
            int vertexBatchSize, int indexBatchBatchSize, int videoDisplay, GLProfile profile)
            => throw new NotImplementedException();
        public ISoundEngine CreateSound(string device) => Engine;
        public IFont CreateFont(byte[] data) => throw new NotImplementedException();
    }

    sealed class MenuStubWindow : IPlatformWindow
    {
        readonly MenuStubGraphicsContext context = new();
        public IGraphicsContext Context => context;
        public Size NativeWindowSize => new(MainMenuWindowWidth, MainMenuWindowHeight);
        public Size EffectiveWindowSize => new(MainMenuWindowWidth, MainMenuWindowHeight);
        public Size SurfaceSize => new(MainMenuWindowWidth, MainMenuWindowHeight);
        public float NativeWindowScale => 1f;
        public float EffectiveWindowScale => 1f;
        public int DisplayCount => 1;
        public int CurrentDisplay => 0;
        public bool HasInputFocus => true;
        public bool IsSuspended => false;
        public event Action<float, float, float, float> OnWindowScaleChanged { add { } remove { } }
        public void PumpInput(IInputHandler inputHandler) { }
        public string GetClipboardText() => "";
        public bool SetClipboardText(string text) => false;
        public bool TryOpenUrl(string url) => false;
        public void GrabWindowMouseFocus() { }
        public void ReleaseWindowMouseFocus() { }
        public IHardwareCursor CreateHardwareCursor(string name, Size size, byte[] data, int2 hotspot, bool pixelDouble) => null;
        public void SetHardwareCursor(IHardwareCursor cursor) { }
        public void SetWindowTitle(string title) { }
        public void SetRelativeMouseMode(bool mode) { }
        public void SetScaleModifier(float scale) { }
        public GLProfile GLProfile => GLProfile.Automatic;
        public GLProfile[] SupportedGLProfiles => [];
        public void Dispose() { }
    }

    // 零度量字体桩（PerfGraph/News 状态标签的 ctor 读 Fonts[...]：返回零尺寸 SpriteFont，
    // 字形全部零宽——Measure/WrapText 不 NPE，输出不依赖字形度量）
    sealed class MenuStubFont : IFont
    {
        public FontGlyph CreateGlyph(char c, int size, float deviceScale) => default;
        public void Dispose() { }
    }

    sealed class MenuStubPlatform : IPlatform
    {
        public IPlatformWindow CreateWindow(Size size, WindowMode windowMode, float scaleModifier,
            int vertexBatchSize, int indexBatchBatchSize, int videoDisplay, GLProfile profile)
            => new MenuStubWindow();
        public ISoundEngine CreateSound(string device) => throw new NotImplementedException();
        public IFont CreateFont(byte[] data) => new MenuStubFont();
    }

    static (string RelPath, string Mode, Func<byte[]> Produce)[] MainMenuArtifacts(string openraRoot)
    {
        return
        [
            .. MainMenuChromeFiles.Select(name => (
                $"{MainMenuGoldenRelDir}/fixture/{name}",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(
                    Path.Combine(openraRoot, "mods", "common", "chrome", name))))),
            ($"{MainMenuGoldenRelDir}/fixture/COPYING", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "COPYING"))),
            ($"{MainMenuGoldenRelDir}/tree.txt", "mainmenu", null),
            ($"{MainMenuGoldenRelDir}/nav.txt", "mainmenu", null),
            ($"{MainMenuGoldenRelDir}/bindings.txt", "mainmenu", null),
        ];
    }

    static byte[] MainMenuTextEnd(StringBuilder sb) =>
        new UTF8Encoding(false).GetBytes(sb.ToString());

    // —— tree：五语料装载树（chrome 模式公式复用）——
    static byte[] DumpMainMenuTree(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# mainmenu/settings chrome tree oracle (T26/issue #35)").Append('\n');
        sb.Append("# source: mods/common/chrome/{mainmenu,settings,settings-display,settings-audio,settings-hotkeys}.yaml").Append('\n');
        sb.Append("# window=1024x768; top-level mounted on dumb root (PARENT=0); format as chrome/ingame-player-tree.txt").Append('\n');
        foreach (var file in MainMenuChromeFiles)
        {
            sb.Append("file\t").Append(file).Append('\n');
            using var stream = File.OpenRead(Path.Combine(openraRoot, "mods", "common", "chrome", file));
            var nodes = MiniYaml.FromStream(stream, $"common|chrome/{file}");
            foreach (var top in nodes)
                DumpChromeWidgetNode(sb, top, 0, 0, 0);
        }

        return MainMenuTextEnd(sb);
    }

    // —— nav：真 MainMenuLogic 驱动 ——
    static string MainMenuTypeOf(Widget root)
    {
        // menuType 私有；经四菜单 IsVisible() lambda（捕获 menuType）可观测推断
        var main = root.Get("MAIN_MENU").IsVisible();
        var single = root.Get("SINGLEPLAYER_MENU").IsVisible();
        var extras = root.Get("EXTRAS_MENU").IsVisible();
        var editor = root.Get("MAP_EDITOR_MENU").IsVisible();
        if (main) return "Main";
        if (single) return "Singleplayer";
        if (extras) return "Extras";
        if (editor) return "MapEditor";
        return "None";
    }

    // Mods.Common ButtonWidget 经反射 Get<T>（宿主不引用 mod 程序集，Widgets 模式同款）
    static dynamic MenuButtonOf(Widget root, string id)
    {
        WidgetsBindCommon();
        var get = typeof(Widget).GetMethods()
            .First(m => m.Name == "Get" && m.IsGenericMethodDefinition
                && m.GetParameters().Length == 1 && m.GetParameters()[0].ParameterType == typeof(string))
            .MakeGenericMethod(WidgetsButton);
        return get.Invoke(root, [id])!;
    }

    // 容器限定变体：三个菜单各有同名 BACK_BUTTON，root.Get 只命中先序首个——
    // 语境化驱动必须经所属菜单容器再 Get
    static dynamic MenuButtonOf(Widget root, string container, string id)
    {
        WidgetsBindCommon();
        var getDef = typeof(Widget).GetMethods()
            .First(m => m.Name == "Get" && m.IsGenericMethodDefinition
                && m.GetParameters().Length == 1 && m.GetParameters()[0].ParameterType == typeof(string));
        var containerWidget = (Widget)getDef.MakeGenericMethod(typeof(Widget)).Invoke(root, [container])!;
        return getDef.MakeGenericMethod(WidgetsButton).Invoke(containerWidget, [id])!;
    }

    static void DumpMainMenuNavState(StringBuilder sb, Widget root, string step)
    {
        sb.Append("step\t").Append(step).Append('\n');
        sb.Append("menu\t").Append(MainMenuTypeOf(root)).Append('\n');
        sb.Append("visible\tMAIN_MENU=").Append(root.Get("MAIN_MENU").IsVisible() ? 1 : 0)
            .Append("\tSINGLEPLAYER_MENU=").Append(root.Get("SINGLEPLAYER_MENU").IsVisible() ? 1 : 0)
            .Append("\tEXTRAS_MENU=").Append(root.Get("EXTRAS_MENU").IsVisible() ? 1 : 0)
            .Append("\tMAP_EDITOR_MENU=").Append(root.Get("MAP_EDITOR_MENU").IsVisible() ? 1 : 0).Append('\n');
        sb.Append("disabled\tSKIRMISH=").Append(MenuButtonOf(root, "SKIRMISH_BUTTON").IsDisabled() ? 1 : 0)
            .Append("\tMISSIONS=").Append(MenuButtonOf(root, "MISSIONS_BUTTON").IsDisabled() ? 1 : 0)
            .Append("\tLOAD=").Append(MenuButtonOf(root, "LOAD_BUTTON").IsDisabled() ? 1 : 0)
            .Append("\tCONTENT=").Append(MenuButtonOf(root, "CONTENT_BUTTON").IsDisabled() ? 1 : 0)
            .Append("\tLOAD_MAP=").Append(MenuButtonOf(root, "LOAD_MAP_BUTTON").IsDisabled() ? 1 : 0).Append('\n');
    }

    static byte[] DumpMainMenuNav(ModData modData, World world)
    {
        var sb = new StringBuilder();
        sb.Append("# mainmenu navigation oracle (T26/issue #35)").Append('\n');
        sb.Append("# upstream MainMenuLogic driven on real mainmenu.yaml (Renderer stub 1024x768, ra ModData + MapCache)").Append('\n');
        sb.Append("# settings tuned: FetchNews=False prompts satisfied CheckVersion=False -> ctor lands on Main").Append('\n');
        sb.Append("# panel-opening buttons (SETTINGS/MULTIPLAYER/missions/skirmish/extras panels) not headless-drivable").Append('\n');
        sb.Append("# (Game.OpenWindow needs WorldRenderer; StartSkirmishGame starts a local server) - deviation registered").Append('\n');

        Ui.ResetAll();
        Ui.Root = new ContainerWidget { Bounds = new WidgetBounds(0, 0, MainMenuWindowWidth, MainMenuWindowHeight) };
        Ui.Initialize(modData);

        var root = Ui.LoadWidget("MAINMENU", Ui.Root, new WidgetArgs
        {
            ["world"] = world,
            ["worldRenderer"] = (WorldRenderer)null,
            ["orderManager"] = (OrderManager)null,
        });

        DumpMainMenuNavState(sb, root, "init");

        MenuButtonOf(root, "SINGLEPLAYER_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "SINGLEPLAYER_BUTTON");
        MenuButtonOf(root, "SINGLEPLAYER_MENU", "BACK_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "BACK_BUTTON(singleplayer)");

        MenuButtonOf(root, "EXTRAS_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "EXTRAS_BUTTON");
        MenuButtonOf(root, "MAP_EDITOR_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "MAP_EDITOR_BUTTON");
        MenuButtonOf(root, "MAP_EDITOR_MENU", "BACK_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "BACK_BUTTON(map-editor)");
        MenuButtonOf(root, "EXTRAS_MENU", "BACK_BUTTON").OnClick();
        DumpMainMenuNavState(sb, root, "BACK_BUTTON(extras)");

        MenuButtonOf(root, "QUIT_BUTTON").OnClick();
        var stateField = typeof(Game).GetField("state", BindingFlags.NonPublic | BindingFlags.Static);
        sb.Append("quit\t").Append(stateField?.GetValue(null) ?? "?").Append('\n');

        return MainMenuTextEnd(sb);
    }

    // —— bindings：三页接线真源 ——
    static byte[] DumpMainMenuBindings(string openraRoot, ModData modData, MenuRecordingSoundEngine engine)
    {
        var sb = new StringBuilder();
        sb.Append("# settings panel bindings oracle (T26/issue #35)").Append('\n');
        sb.Append("# real SettingsUtils.Bind*Pref on real widgets + real Game.Settings fields;").Append('\n');
        sb.Append("# display-leave formula transcribed from DisplaySettingsLogic.cs:274-285; ListLayout real AdjustChildren").Append('\n');

        // slider-bind：BindSliderPref 真调用 + OnChange 委托链反射触发（AudioSettingsLogic.cs:48-50,93）
        WidgetsBindCommon();
        var settingsUtils = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.Logic.SettingsUtils")!;
        var bindSlider = settingsUtils.GetMethod("BindSliderPref")!;
        var bindCheckbox = settingsUtils.GetMethod("BindCheckboxPref")!;
        {
            var parent = new ContainerWidget();
            var slider = WidgetsNew(WidgetsSlider);
            slider.Id = "SOUND_VOLUME";
            parent.AddChild(slider);

            var soundSettings = Game.Settings.Sound;
            soundSettings.SoundVolume = 0.5f;
            bindSlider.Invoke(null, [parent, "SOUND_VOLUME", soundSettings, "SoundVolume"]);
            sb.Append("slider-bind\tSOUND_VOLUME\tinitial=").Append(slider.Value.ToString("F6", CultureInfo.InvariantCulture)).Append('\n');

            // OnChange 是 field-like event：支持字段与事件同名但编译器生成 private
            var onChangeField = WidgetsSlider.GetField("OnChange", BindingFlags.NonPublic | BindingFlags.Instance);
            var onChange = (Action<float>)onChangeField!.GetValue(slider)!;
            onChange(0.75f);
            sb.Append("slider-change\tSOUND_VOLUME\t0.75\tfield=").Append(soundSettings.SoundVolume.ToString("F6", CultureInfo.InvariantCulture)).Append('\n');

            // AudioSettingsLogic.cs:93 追加订阅等效：Game.Sound.SoundVolume = x（真 Sound 链）
            Game.Sound.SoundVolume = 0.75f;
            sb.Append("sound-volume-chain\t").Append(engine.Log[^1]).Append('\n');
            Game.Sound.MusicVolume = 0.25f;
            Game.Sound.VideoVolume = 0.6f;
            sb.Append("volumes\tSound=").Append(Game.Settings.Sound.SoundVolume.ToString("F6", CultureInfo.InvariantCulture))
                .Append("\tMusic=").Append(Game.Settings.Sound.MusicVolume.ToString("F6", CultureInfo.InvariantCulture))
                .Append("\tVideo=").Append(Game.Settings.Sound.VideoVolume.ToString("F6", CultureInfo.InvariantCulture)).Append('\n');
        }

        // checkbox-bind：BindCheckboxPref 真调用 + MuteAudio/UnmuteAudio 真调用（AudioSettingsLogic.cs:45,57-65）
        {
            var parent = new ContainerWidget();
            var cb = WidgetsNew(WidgetsCheckbox, modData);
            cb.Id = "MUTE_SOUND";
            parent.AddChild(cb);

            var soundSettings = Game.Settings.Sound;
            soundSettings.Mute = false;
            bindCheckbox.Invoke(null, [parent, "MUTE_SOUND", soundSettings, "Mute"]);
            cb.OnClick();
            sb.Append("checkbox-bind\tMUTE_SOUND\tchecked=").Append(cb.IsChecked() ? 1 : 0)
                .Append("\tfield.Mute=").Append(soundSettings.Mute ? "True" : "False").Append('\n');

            Game.Sound.MuteAudio();
            Game.Sound.UnmuteAudio();
            sb.Append("mute-chain\t").Append(string.Join(",", engine.Log.Where(l => l.StartsWith("master=")))).Append('\n');
        }

        // display-leave：公式照抄（DisplaySettingsLogic.cs:274-285）
        {
            GraphicSettings MakeOriginal() => new() { Mode = WindowMode.PseudoFullscreen, VideoDisplay = 0, WindowedSize = new int2(1024, 768), FullscreenSize = new int2(0, 0), GLProfile = GLProfile.Automatic };
            bool LeaveNeedsRestart(GraphicSettings g, GraphicSettings orig, string widthText, string heightText, out int2 windowed)
            {
                int.TryParse(widthText, out var x);
                int.TryParse(heightText, out var y);
                windowed = new int2(x, y);
                g.WindowedSize = windowed;
                return g.Mode != orig.Mode || g.VideoDisplay != orig.VideoDisplay
                    || g.WindowedSize != orig.WindowedSize || g.FullscreenSize != orig.FullscreenSize
                    || g.GLProfile != orig.GLProfile;
            }

            var cases = new (string Name, Action<GraphicSettings> Mutate, string Width, string Height)[]
            {
                ("untouched", _ => { }, "1024", "768"),
                ("mode-windowed", g => g.Mode = WindowMode.Windowed, "1024", "768"),
                ("size-changed", _ => { }, "1280", "720"),
                ("bad-text", _ => { }, "not-a-number", "x"),
                ("video-display", g => g.VideoDisplay = 1, "1024", "768"),
                ("gl-profile", g => g.GLProfile = GLProfile.Modern, "1024", "768"),
            };
            foreach (var (name, mutate, width, height) in cases)
            {
                var g = MakeOriginal();
                mutate(g);
                var restart = LeaveNeedsRestart(g, MakeOriginal(), width, height, out var windowed);
                sb.Append("display-leave\t").Append(name)
                    .Append("\twindowed=").Append(windowed.X).Append(',').Append(windowed.Y)
                    .Append("\trestart=").Append(restart ? 1 : 0).Append('\n');
            }
        }

        // tab 布局：ButtonStride 真 FieldLoader int2 + 克隆 bounds 公式 + 真 ListLayout.AdjustChildren
        {
            var stride = FieldLoader.GetValue<int2>("ButtonStride", "0, 35");
            sb.Append("tab-stride\t").Append(stride.X).Append(',').Append(stride.Y).Append('\n');
            for (var i = 0; i < 3; i++)
                sb.Append("tab\t").Append(i)
                    .Append('\t').Append(i == 0 ? 0 : stride.X * i).Append(',').Append(i == 0 ? 0 : stride.Y * i).Append('\n');

            // settings-audio.yaml SETTINGS_SCROLLPANEL 真源驱动：ScrollPanel 字段
            // （TopBottomSpacing/ItemSpacing/CollapseHiddenChildren）与行高全部经
            // FieldLoader 自 yaml 读入，真 ListLayout.AdjustChildren 布局后 dump
            {
                using var stream = File.OpenRead(Path.Combine(openraRoot, "mods", "common", "chrome", "settings-audio.yaml"));
                var audioNodes = MiniYaml.FromStream(stream, "common|chrome/settings-audio.yaml").ToList();
                MiniYamlNode FindNode(MiniYamlNode n, string keySuffix)
                {
                    if (n.Key.EndsWith(keySuffix, StringComparison.Ordinal)) return n;
                    foreach (var c in n.Value.Nodes)
                    {
                        var f = FindNode(c, keySuffix);
                        if (f != null) return f;
                    }
                    return null;
                }

                var scrollNode = FindNode(audioNodes[0], "@SETTINGS_SCROLLPANEL")
                    ?? throw new InvalidOperationException("SETTINGS_SCROLLPANEL not found");
                var scroll = WidgetsNew(WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.ScrollPanelWidget")!, modData);
                foreach (var field in scrollNode.Value.Nodes.Where(n => n.Key != "Children"))
                    FieldLoader.LoadFieldOrProperty(scroll, field.Key, field.Value.Value);
                foreach (var rowNode in scrollNode.Value.Nodes.First(n => n.Key == "Children").Value.Nodes)
                {
                    // Widget.Height 是只读投影（Bounds.Height），经 Bounds 装载；
                    // settings-audio 行高均为字面量
                    var heightNode = rowNode.Value.Nodes.FirstOrDefault(n => n.Key == "Height");
                    var h = heightNode != null ? FieldLoader.GetValue<int>("Height", heightNode.Value.Value) : 0;
                    var row = new ContainerWidget { Bounds = new WidgetBounds(0, 0, 0, h) };
                    scroll.AddChild(row);
                }

                var layoutType = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.ListLayout")!;
                var layout = layoutType.GetConstructors()[0].Invoke([scroll]);
                layoutType.GetMethod("AdjustChildren")!.Invoke(layout, null);
                foreach (var child in (List<Widget>)scroll.Children)
                    sb.Append("scroll-row\ty=").Append(child.Bounds.Y).Append("\th=").Append(child.Bounds.Height).Append('\n');
                sb.Append("scroll-content-height\t").Append(scroll.ContentHeight).Append('\n');
            }
        }

        // hotkeys：真 HotkeyManager rekey 流（HotkeysSettingsLogic SaveHotkey/冲突面）
        {
            var hotkeys = modData.Hotkeys;
            var yamlField = typeof(HotkeyManager).GetField("hotkeySettings", BindingFlags.NonPublic | BindingFlags.Instance)!;
            List<(string Key, string Value)> SettingsNodes()
            {
                var module = yamlField.GetValue(hotkeys)!;
                var builder = (MiniYamlBuilder)module.GetType().GetField("Yaml")!.GetValue(module)!;
                return builder.Nodes.Select(n => (n.Key, n.Value.Value)).ToList();
            }

            sb.Append("hotkey-definitions\t").Append(hotkeys.Definitions.Count()).Append('\n');
            foreach (var name in new[] { "CycleBase", "ToLastEvent", "ToSelection", "ToggleChatMode" })
            {
                var d = hotkeys.Definitions.First(hd => hd.Name == name);
                sb.Append("hotkey-row\t").Append(name)
                    .Append('\t').Append(hotkeys[name].GetValue())
                    .Append('\t').Append(d.Default)
                    .Append("\tdefault-same=").Append(hotkeys[name].GetValue() == d.Default ? 1 : 0)
                    .Append("\treadonly=").Append(d.Readonly ? 1 : 0).Append('\n');
            }

            // rekey：Set + hotkeySettings.Yaml 节点（RemoveAll + value != Default 追加）
            hotkeys.Set("CycleBase", new Hotkey(Keycode.E, Modifiers.Ctrl));
            sb.Append("hotkey-rekey\tCycleBase\tE Ctrl\tcurrent=").Append(hotkeys["CycleBase"].GetValue()).Append('\n');
            foreach (var (key, value) in SettingsNodes())
                sb.Append("hotkey-settings-node\t").Append(key).Append('\t').Append(value).Append('\n');

            // rekey back to default：节点移除（value == Default 不追加）
            hotkeys.Set("CycleBase", hotkeys.Definitions.First(hd => hd.Name == "CycleBase").Default);
            sb.Append("hotkey-rekey-default\tCycleBase\tnodes=").Append(SettingsNodes().Count).Append('\n');

            // readonly 拒绝（HotkeyManager.Set :63-89 返回 void：经“值未变”观测）
            hotkeys.Set("ToggleChatMode", new Hotkey(Keycode.E, Modifiers.None));
            sb.Append("hotkey-readonly\tToggleChatMode\tcurrent=").Append(hotkeys["ToggleChatMode"].GetValue()).Append('\n');

            // 冲突检测（GetFirstDuplicate :91-106）正例：ToLastEvent 先改 HOME，
            // 再把 ToSelection 改 HOME → ToLastEvent 被标重复（Set 全表重算 HasDuplicates）
            hotkeys.Set("ToLastEvent", new Hotkey(Keycode.HOME, Modifiers.None));
            hotkeys.Set("ToSelection", new Hotkey(Keycode.HOME, Modifiers.None));
            var dup = hotkeys.GetFirstDuplicate(
                hotkeys.Definitions.First(hd => hd.Name == "ToSelection"), hotkeys["ToSelection"].GetValue());
            sb.Append("hotkey-duplicate\tToSelection\t").Append(dup?.Name ?? "none").Append('\n');
            // 解除冲突：ToSelection 回默认 → 双方 HasDuplicates 复位（C++ 全表重算对拍锚）
            hotkeys.Set("ToSelection", hotkeys.Definitions.First(hd => hd.Name == "ToSelection").Default);
            sb.Append("hotkey-duplicate-cleared\tToLastEvent\t")
                .Append(hotkeys.Definitions.First(hd => hd.Name == "ToLastEvent").HasDuplicates ? 1 : 0).Append('\n');
        }

        return MainMenuTextEnd(sb);
    }

    // 最小 MenuWorld（MainMenuLogic 的 music 面板/热键 logic 需要 world.WorldActor 上的
    // MusicPlaylist trait）：ore-lord 真图 + 全量 ra 规则（t07 渲染 trait 剥离）+ 反射
    // internal World ctor（replay 模式同款，跳过 WorldLoaded 循环——菜单世界不跑 tick）。
    static World MenuBuildWorld(ModData modData)
    {
        var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", "ra", "maps"));
        var package = mapsDir.OpenPackage("ore-lord.oramap", modData.ModFiles)
            ?? throw new InvalidOperationException("cannot open ore-lord.oramap");
        var map = new Map(modData, package);
        modData.PrepareMap(map);

        var fld = typeof(ActorInfo).GetField("traits", BindingFlags.NonPublic | BindingFlags.Instance)!;
        foreach (var actorName in map.Rules.Actors.Keys.ToList())
        {
            var dict = (OpenRA.Primitives.TypeDictionary)fld.GetValue(map.Rules.Actors[actorName])!;
            var doomed = dict.OfType<TraitInfo>().Where(t => State.StripRenderTraits.Contains(t.GetType().Name)).ToList();
            foreach (var t in doomed)
                dict.Remove(t);
        }

        var om = new OrderManager(new RecordingEchoConnection(() => "menu-world.orarep"));
        var ctor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
            [typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType)], null)
            ?? throw new InvalidOperationException("World ctor not found");
        var world = (World)ctor.Invoke([map, modData, om, WorldType.Regular]);
        om.World = world;
        typeof(World).GetField("<LocalPlayer>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance)!.SetValue(world, null);
        return world;
    }

    // mainmenu 模式组装配（一次）+ 三产物
    static (string RelPath, byte[] Bytes)[] MainMenuGroup(string openraRoot)
    {
        HeadlessHost.Boot(openraRoot, soundChannel: false);
        Game.InitializeSettings(new Arguments(
        [
            "Game.FetchNews=False",
            "Game.IntroductionPromptVersion=999",
            "Debug.SystemInformationVersionPrompt=999",
            "Debug.CheckVersion=False",
        ]));

        var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
        var modData = new ModData(mods["ra"], mods);
        Game.ModData = modData;
        ObjectCreator.MissingTypeAction = s => Console.WriteLine($"!! MissingType: {s}");
        FieldLoader.UnknownFieldAction = f => Console.WriteLine($"!! UnknownField: {f}");

        var enginePlatform = new MenuRecordingSoundPlatform();
        var sound = new Sound(enginePlatform, new SoundSettings());
        typeof(Game).GetField("Sound", BindingFlags.Public | BindingFlags.Static)!.SetValue(null, sound);
        Game.Sound.DisableAllSounds = true;

        modData.MapCache.LoadMaps(modData);
        Program.ChromeMetricsOnce();

        // Renderer 桩：Widget.Initialize 的 WINDOW substitutions 走 Game.Renderer.Resolution；
        // Fonts 塞零度量 SpriteFont（PerfGraphWidget/News 标签 ctor 读 Fonts[name].Measure）
        Game.Renderer = new Renderer(new MenuStubPlatform(), Game.Settings.Graphics, 32);
        var menuFontPlatform = new MenuStubPlatform();
        var menuFontBuilder = new SheetBuilder(SheetType.BGRA, 512);
        SpriteFont MenuStubSpriteFont() =>
            new(menuFontPlatform, "stub", [], 10, 8, 1f, menuFontBuilder);
        var menuFonts = new Dictionary<string, SpriteFont>();
        foreach (var name in new[] { "Regular", "Bold", "Tiny", "TinyBold", "Title" })
            menuFonts[name] = MenuStubSpriteFont();
        typeof(Renderer).GetField("Fonts")!.SetValue(Game.Renderer, menuFonts);

        // 菜单世界（MusicHotkeyLogic 的 MusicPlaylist 依赖）+ 本地玩家档案
        // （Game.cs:504 形态：LocalProfileLogic ctor 读 Game.LocalPlayerProfile）
        var world = MenuBuildWorld(modData);
        Game.LocalPlayerProfile = new LocalPlayerProfile(
            Path.Combine(Platform.SupportDir, Game.Settings.Game.AuthProfile),
            modData.GetOrCreate<PlayerDatabase>());

        return
        [
            ($"{MainMenuGoldenRelDir}/tree.txt", DumpMainMenuTree(openraRoot)),
            ($"{MainMenuGoldenRelDir}/nav.txt", DumpMainMenuNav(modData, world)),
            ($"{MainMenuGoldenRelDir}/bindings.txt", DumpMainMenuBindings(openraRoot, modData, enginePlatform.Engine)),
        ];
    }
}
