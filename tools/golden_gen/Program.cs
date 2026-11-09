// golden-gen —— 无头 oracle 宿主（工单 #3 / T02；PRNG 模式源自 T03）
//
// 职责：ProjectReference 上游基线源码（OpenRA.Game）构建，进程内驱动上游真源产出
// 黄金基准产物，供 C++ 侧移植黄金对拍（入库字节 = 判定真值）。动作：
//   gen     重产基准产物并写入库位置（tests/golden/**）+ .sha256 旁车（sha256sum -c 兼容）
//   verify  重产并与入库黄金逐字节比对，退出码供脚本/ctest 判定
//   check   验收复验：每产物独立重产两次断言字节恒等（SHA256 一致），再与入库黄金比对
//
// 产物模式（artifacts 清单登记，随工单扩展）：
//   prng            上游 OpenRA.Support.MersenneTwister 序列 dump（7 种子 × 10 kind，T03 格式）
//   resolved-rules  上游 IUtilityCommand --resolved-rules ra:BADR.Bomber——覆盖 MiniYaml
//                   解析、Inherits/Inherits@ 继承展开与多文件合并语义
//   rules-stats     上游 Manifest.Rules 提取 + MiniYaml.Load 全量合并树（T05/issue #6）——
//                   ra-manifest.txt（16 文件有序清单）、ra-merged.yaml（全量合并树，
//                   Merge 输出序、WriteToString 序列化）、ra-stats.txt（计数头 + 逐 actor
//                   trait 计数）。轻路径：物理 IReadOnlyFileSystem + MiniYaml.Load 直驱，
//                   不装配 ModData/IUtilityCommand（无需 Game.ModData hack）。
//   map-info        无头 new Map(modData, oramap 包) 全量加载后的字段+图块直方图转储
//                   （T06 格式，tests/golden/maps/*.mapinfo.txt）；同批把 .oramap 与
//                   tileset yaml 原样从上游检出复制入库（基线漂移时 check 报警）
//   pathfinding     反射无头 World（ore-lord 真实地形 + 最小合成规则集，t11 §4.1 八障碍
//                   配方）驱动的局部 A*（PathSearch.ToTargetCell，11 对 × w100/w125 +
//                   nolane 对照）、HPF 层（PathFinder.FindPathToTargetCell，10 对 ×
//                   none/all）、costmap 符号图与真实 Move Activity 444 tick 序列
//                   （tests/golden/pathfinding/ore-lord.{paths,move}.txt）
//   combat          真实 ra 武器表（MiniYaml.Load manifest 全文件继承合并）+ 最小合成
//                   规则集（t11 配方 + 战斗 trait）驱动的四剧本交战至死亡逐 tick dump
//                   （duelA InstantHit 对射 / duelB Bullet 炮塔 / duelC 弧线弹+散布 /
//                   duelD 超程接近）——HP/朝向/位置/effects/RNG/SyncHash/弹道行
//                   （tests/golden/combat/duels.txt）；武器 yaml 6 文件 + manifest
//                   原样入库作基线漂移哨兵。无头障碍 9-15 配方见 t12-combat.md §4.1
//   hotkeys         ra 热键绑定表轻路径 dump（T22/issue #29）：mod.yaml Hotkeys: 列表序
//                   Merge → 真·上游 HotkeyDefinition/Hotkey.TryParse → 合成 settings
//                   Keys: 覆盖段（改键/冲突/Readonly 跳过）——files.txt + 9 yaml
//                   原样入库（漂移哨兵）+ ra-bindings.txt（174 定义）+ ra-override.txt
//   settings        settings.yaml 持久化轻路径 dump（T21/issue #28）：真·上游 Settings
//                   类直驱（不装配 ModData）——fresh-default/all-types/roundtrip 互证、
//                   未知段键保留、空行分裂、坏值零值替换+告警逐字、CLI 覆盖、Keys 段
//                   无字段段形态、float 最短往返锚点；UUID 行级归一化后入库
//   chrome          widget 框架与 chrome 装载 dump（T24/issue #33）：四语料原样入库
//                   （ra chrome.yaml/ingame-player.yaml + common/ra metrics.yaml，GPL）
//                   + 真源 FieldLoader.Load<ChromeProvider.Collection> 集合表（排序序）
//                   + ChromeMetrics 双文件 Merge 覆盖表 + 装载树 bounds（真源
//                   IntegerExpression.Evaluate 按 Widget.Initialize substitutions 公式，
//                   顶层挂哑 root PARENT=0、窗口 1024x768）+ 表达式集求值表；
//                   事件路由不进 oracle（上游虚方法面无头不可驱动），由 C++ ctest 覆盖
//   widgets         六控件最小面 dump（T25/issue #34）：真源控件构造（ModData ra）+
//                   合成 MouseInput 喂 Ui.HandleInput 的状态机逐步行（states.txt，含
//                   OnClick/OnDoubleClick/OnChange 回调记录）、绘制调用序列（draw.txt，
//                   GetStatefulImageName 真函数 + DrawPanel/FillRectWithSprite 公式照抄）、
//                   chrome sheet 像素（pixels.txt，Png+FastCopyIntoSprite+PremultiplyAlpha
//                   真源，D075 兑现）+ 三 PNG 语料原样入库
//
// 每次动作前断言上游检出 HEAD == UPSTREAM.baseline 首行，防基准漂移。
// resolved-rules 产物字节为上游命令 stdout 的原样捕获（UTF-8 无 BOM；末尾行尾来自
// Console.WriteLine，Windows 上为 CR LF）——黄金锚定 Windows 上游 stdout 语义，不做任何规范化。
//
// 退出码：0 成功 / 1 校验失败（含 check 双跑不恒等）/ 2 基线漂移 / 3 宿主错误；
//         77 前置缺失（dotnet/上游检出不在）由 run.cmd 判定，供 ctest 记为 SKIP。

global using System.Diagnostics;
global using System.Globalization;
global using System.IO.Compression;
global using System.Reflection;
global using System.Runtime.Serialization;
global using System.Security.Cryptography;
global using System.Text;
global using OpenRA;
global using OpenRA.FileSystem;
global using OpenRA.FileFormats;
global using OpenRA.Network;
global using OpenRA.Primitives;
global using OpenRA.Support;
global using OpenRA.Traits;

internal static partial class Program
{
    const int ExitOk = 0;
    const int ExitVerifyFail = 1;   // verify/check：黄金缺失、字节不一致或双跑不恒等
    const int ExitBaseline = 2;     // 上游检出与 UPSTREAM.baseline 不符（基准漂移）
    const int ExitError = 3;        // 参数/构建/运行等其他宿主错误

    static int Main(string[] args)
    {
        try
        {
            return Run(args);
        }
        catch (BaselineMismatchException e)
        {
            return Fail(ExitBaseline, e.Message);
        }
        catch (Exception e)
        {
            var root = e;
            while (root is TargetInvocationException tie && tie.InnerException != null)
                root = tie.InnerException;
            return Fail(ExitError, root == e
                ? $"{e.GetType().Name}: {e.Message}\n{e.StackTrace}"
                : $"{e.GetType().Name}: {e.Message} -> {root.GetType().Name}: {root.Message}\n{root.StackTrace}");
        }
        finally
        {
            // 必修：上游 Log 静态构造（OpenRA.Game/Support/Log.cs:49-54）启动的是前台线程，
            // 不 Dispose 进程永不退出——参照上游 OpenRA.Utility/Program.cs Main 的 finally 收尾。
            Log.Dispose();
        }
    }

    static int Run(string[] args)
    {
        // —— 参数解析（run.cmd 会显式传入两个根路径） ——
        string verb = null;
        string repoRootArg = null;
        string openraArg = null;
        for (var i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--repo-root": repoRootArg = NextArg(args, ref i); break;
                case "--openra": openraArg = NextArg(args, ref i); break;
                default:
                    if (verb == null && args[i] is "gen" or "verify" or "check") verb = args[i];
                    else throw new ArgumentException(
                        $"未知参数或动作：{args[i]}（用法：golden-gen gen|verify|check [--repo-root DIR] [--openra DIR]）");
                    break;
            }
        }

        if (verb == null)
            throw new ArgumentException("缺少动作。用法：golden-gen gen|verify|check [--repo-root DIR] [--openra DIR]");

        // —— 路径解析 ——
        var repoRoot = ResolveRepoRoot(repoRootArg);
        var openraRoot = ResolveOpenRaRoot(openraArg, repoRoot);

        // —— 基线断言：上游检出必须钉在 UPSTREAM.baseline 记录的 commit ——
        AssertUpstreamBaseline(openraRoot, repoRoot);

        // 注：宿主不在此整建上游 slnx——宿主 exe 与其加载的 OpenRA.Game.dll 都在
        // openra/bin，进程运行中锁文件，进程内再构建同一输出目录会自锁
        // （MSB3026 复制失败重试耗尽）。上游构建职责在 run.cmd：先建 slnx 再建宿主，
        // 宿主启动后对 mod 程序集做存在性自检（见 DumpResolvedRules）。

        // —— 基准产物清单 ——
        // Mode == null 的直接产物（轻量）：check 语义 = 每产物独立双跑。
        // Mode 非空的场景产物（重宿主装配）：按 (openraRoot, mode) 缓存模式运行——
        // check 双跑 = 每模式两份独立运行（真双跑语义保留），模式内多产物
        // 共享同一次运行（此前 pathfinding 2 产物 ×2 遍 / replay 3 产物 ×2 遍整跑的浪费消除）。
        (string RelPath, string Mode, Func<byte[]> Produce)[] artifacts =
        [
            .. State.PrngSeeds.Select(seed => (
                $"tests/golden/prng/seed_{seed.ToString(CultureInfo.InvariantCulture)}.txt",
                (string)null,
                (Func<byte[]>)(() => DumpPrngSequence(seed)))),
            ("tests/golden/resolved_rules/BADR.Bomber.yaml", null,
                () => DumpResolvedRules(openraRoot, "ra", "BADR.Bomber")),
            ("tests/golden/rules/ra-manifest.txt", null, () => DumpRaManifest(openraRoot)),
            ("tests/golden/rules/ra-merged.yaml", null, () => DumpRaMerged(openraRoot)),
            ("tests/golden/rules/ra-stats.txt", null, () => DumpRaStats(openraRoot)),
            .. State.MapNames.Select(mapName => (
                $"tests/golden/maps/{mapName}.oramap",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(
                    Path.Combine(openraRoot, "mods", "ra", "maps", mapName + ".oramap"))))),
            .. State.MapNames.Select(mapName => (
                $"tests/golden/maps/{mapName}.mapinfo.txt",
                (string)null,
                (Func<byte[]>)(() => DumpMapInfo(openraRoot, "ra", mapName)))),
            ("tests/golden/maps/tilesets/temperat.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "tilesets", "temperat.yaml"))),
            ("tests/golden/maps/tilesets/snow.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "tilesets", "snow.yaml"))),
            ("tests/golden/orders/representative.txt", null, () => DumpOrders()),
            ("tests/golden/pathfinding/ore-lord.paths.txt", "pathfinding", null),
            ("tests/golden/pathfinding/ore-lord.move.txt", "pathfinding", null),
            // combat 模式（T12/issue #13）：ra 武器表 6 文件原样入库（与 tilesets 同款
            // 基线漂移哨兵）+ manifest 清单 + 四剧本交战 dump（duels.txt，探针
            // .ora-explore/t12-scratch 装配照搬：无头障碍 9-15，双跑恒等已实证）
            ("tests/golden/combat/weapons-manifest.txt", null, () => DumpCombatWeaponsManifest(openraRoot)),
            .. CombatWeaponFiles(openraRoot).Select(f => (
                $"tests/golden/combat/weapons/{f.RelName}",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(f.FullPath)))),
            ("tests/golden/combat/duels.txt", "combat", null),
            ("tests/golden/production/positive.txt", "production-positive", null),
            ("tests/golden/production/negative.txt", "production-negative", null),
            ("tests/golden/economy/ore-lord.econ.txt", "economy", null),
            // hotkeys 模式（T22/issue #29）：ra 绑定表 174 定义轻路径 dump + 合成 Keys
            // 覆盖段（改键/冲突/Readonly 跳过）+ 9 个热键 yaml 原样入库（基线漂移哨兵）
            ("tests/golden/hotkeys/files.txt", null, () => DumpHotkeysFilesManifest(openraRoot)),
            .. HotkeySourceFiles(openraRoot).Select(f => (
                $"tests/golden/hotkeys/yaml/{f.RelName}",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(f.FullPath)))),
            ("tests/golden/hotkeys/ra-bindings.txt", null, () => DumpHotkeys(openraRoot)),
            ("tests/golden/hotkeys/ra-override.txt", null, () => DumpHotkeysOverride(openraRoot)),
            // replay 模式（T16/issue #17）：t16-scratch 探针配方——ore-lord + 全量 ra 规则（t07
            // 剥离 21 项）+ 注入剧本 actor（t13 ActorDefinitions 追加法），RecordingEchoConnection
            // 复刻 NetworkConnection 的 ReplayRecorder 挂接语义，580 帧五类订单（建造/生产/
            // 移动/攻击/采矿）自录 .orarep；规范化（钉死 Start/EndTimeUtc）后入库 + 记录流
            // （MetaStartMarker 前透传段）+ 逐帧 dump。orarep 的组运行内含上游
            // ReplayConnection 有效性校验 + 自录自放逐帧 SyncHash 对拍（探针实测 0 mismatch）。
            ("tests/golden/replay/ore-lord.orarep", "replay", null),
            ("tests/golden/replay/ore-lord.record-stream.bin", "replay", null),
            ("tests/golden/replay/ore-lord.frames.txt", "replay", null),
            // audio 模式（T18/issue #27）：三 yaml 原样入库（漂移哨兵）+ SoundInfo/MusicInfo
            // 规范化转储 + d2k WAV 语料原样入库（GPL-3，COPYING 同 render）+ 解码统计
            ("tests/golden/audio/notifications.txt", null,
                () => DumpAudioSoundInfo(openraRoot, Path.Combine("mods", "ra", "audio", "notifications.yaml"))),
            ("tests/golden/audio/voices.txt", null,
                () => DumpAudioSoundInfo(openraRoot, Path.Combine("mods", "ra", "audio", "voices.yaml"))),
            ("tests/golden/audio/music.txt", null,
                () => DumpAudioMusicInfo(openraRoot, Path.Combine("mods", "ra", "audio", "music.yaml"))),
            .. (new[] { "notifications", "voices", "music" }).Select(name => (
                $"tests/golden/audio/yaml/{name}.yaml",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(
                    Path.Combine(openraRoot, "mods", "ra", "audio", $"{name}.yaml"))))),
            ("tests/golden/audio/wav/stats.txt", null, () => DumpWavStats(openraRoot)),
            .. (new[] { "CHAT1REVERSE.WAV", "DEVASTATORMELTDOWN.WAV", "ORNIBOMB.WAV", "RELEASESARDAUKARS.WAV" })
                .Select(name => (
                    $"tests/golden/audio/wav/{name}",
                    (string)null,
                    (Func<byte[]>)(() => File.ReadAllBytes(
                        Path.Combine(openraRoot, "mods", "d2k", "bits", name))))),
            // sfx 模式（T19/issue #31）：世界事件音效序列——真上游 Sound 门面
            // （Play/PlayToPlayer/PlayPredefined/PlayNotification）+ 录制引擎 + 名字回注
            // 假文件系统 + 真 ra audio SoundInfo + 播种 CosmeticRandom（池洗牌确定化）。
            // World/Player/Actor 最小反射体（LocalPlayer/Selection/ActorID）。
            ("tests/golden/sfx/sequence.txt", null, () => SfxDumpSequence(openraRoot)),
            // fonts 模式（T23/issue #30）：FreeType interop 轻路径 dump——fixture TTF 原样
            // 入库（漂移哨兵兼 C++ 测试输入）+ ra mod.yaml Fonts: 三元组解析转储 + FTD1u8
            // metrics/位图（ASCII 95 cp × FreeSans 10/12/14/18/24、FreeSansBold 14 + 特殊
            // cp 集缺字/.notdef 面）+ Measure/WrapText oracle（C# 侧逐行复刻真源逻辑）。
            // 前置 freetype6.dll（上游 bin 构建产物）缺失时整组跳过——C++ ctest 读入库
            // 黄金，不依赖该 dll。
            .. (FontsAvailable(openraRoot)
                ? FontsArtifacts(openraRoot)
                : Array.Empty<(string RelPath, string Mode, Func<byte[]> Produce)>()),
            // settings 模式（T21/issue #28）：settings.yaml 段/默认值/未知段键保留、
            // 读写往返字节恒等（UUID 行级归一化；9 用例 + 坏值告警逐字清单）
            ("tests/golden/settings/fresh-default.yaml", null, () => DumpSettingsFreshDefault()),
            ("tests/golden/settings/all-types.yaml", null, () => DumpSettingsAllTypes()),
            ("tests/golden/settings/roundtrip.yaml", null, () => DumpSettingsRoundtrip()),
            ("tests/golden/settings/unknown-preserved.yaml", null, () => DumpSettingsUnknownPreserved()),
            ("tests/golden/settings/blank-split.yaml", null, () => DumpSettingsBlankSplit()),
            ("tests/golden/settings/badval-substituted.yaml", null, () => DumpSettingsBadvalFile()),
            ("tests/golden/settings/badval-warnings.txt", null, () => DumpSettingsBadvalWarnings()),
            ("tests/golden/settings/cliargs.yaml", null, () => DumpSettingsCliArgs()),
            ("tests/golden/settings/keys-rebind.yaml", null, () => DumpSettingsKeysRebind()),
            ("tests/golden/settings/float-formats.txt", null, () => DumpSettingsFloatFormats()),

            // music 模式（T20/issue #32）：合成 12 轨曲库（t03/t07 隐藏、t06 缺失）+ 真源
            // MersenneTwister/Util.Shuffle 洗牌序 + MusicPlaylist/Sound 音乐面逐行复刻剧本
            // dump（顺序/洗牌/循环/背景曲四剧本 + 音量实时链），tests/golden/music/
            ("tests/golden/music/music.yaml", null, () => MusicLibraryYaml()),
            ("tests/golden/music/playlist-order.txt", null, () => DumpMusicPlaylistOrder()),
            ("tests/golden/music/scenario-order.txt", null,
                () => DumpMusicScenarioFile(MusicScenarioOrder())),
            ("tests/golden/music/scenario-shuffle.txt", null,
                () => DumpMusicScenarioFile(MusicScenarioShuffle())),
            ("tests/golden/music/scenario-repeat.txt", null,
                () => DumpMusicScenarioFile(MusicScenarioRepeat())),
            ("tests/golden/music/scenario-background.txt", null,
                () => DumpMusicScenarioFile(MusicScenarioBackground())),

            // chrome 模式（T24/issue #33）：widget 框架与 chrome 装载——四语料原样入库
            // （ra chrome/ingame-player + common/ra metrics，GPL）+ ChromeProvider 集合表/
            // ChromeMetrics 覆盖表/装载树 bounds/表达式求值四份真源转储
            .. ChromeArtifacts(openraRoot),

            // widgets 模式（T25/issue #34）：六控件最小面——状态机真源驱动 dump（states）
            // + 绘制调用序列（draw，公式照抄 + GetStatefulImageName 真函数）+ chrome sheet
            // 像素面（pixels，Png/FastCopyIntoSprite/PremultiplyAlpha 真源，D075 兑现）
            // + 三 PNG 语料原样入库（漂移哨兵）
            .. WidgetsArtifacts(openraRoot),

        ];

        var firstRuns = new Dictionary<string, (string RelPath, byte[] Bytes)[]>();
        var secondRuns = new Dictionary<string, (string RelPath, byte[] Bytes)[]>();

        (string RelPath, byte[] Bytes)[] ScenarioRun(string mode, bool second)
        {
            var cache = second ? secondRuns : firstRuns;
            if (cache.TryGetValue(mode, out var run)) return run;
            run = RunScenarioGroup(openraRoot, mode);
            cache[mode] = run;
            return run;
        }

        var failures = 0;
        foreach (var (relPath, mode, produce) in artifacts)
        {
            var artifactPath = Path.Combine(repoRoot, relPath.Replace('/', Path.DirectorySeparatorChar));

            byte[] first;
            byte[] second;
            if (mode == null)
            {
                first = produce();
                second = produce();
            }
            else
            {
                first = ScenarioRun(mode, second: false).First(r => r.RelPath == relPath).Bytes;
                second = ScenarioRun(mode, second: true).First(r => r.RelPath == relPath).Bytes;
            }

            if (verb == "check")
            {
                // 验收语义：独立重产两次，字节必须恒等（真双跑，非同义反复）
                if (!first.AsSpan().SequenceEqual(second))
                {
                    failures++;
                    Console.WriteLine($"check: {relPath} FAIL —— 连续两次产出字节不一致" +
                        $"（{Sha256Hex(first)} vs {Sha256Hex(second)}）");
                    continue;
                }

                Console.WriteLine($"check: {relPath} 双跑恒等 sha256={Sha256Hex(first)} bytes={first.Length}");
                if (!VerifyGolden(artifactPath, relPath, first))
                    failures++;
            }
            else
            {
                var bytes = first;
                if (verb == "gen")
                    WriteGolden(artifactPath, relPath, bytes);
                else if (!VerifyGolden(artifactPath, relPath, bytes))
                    failures++;
            }
        }

        if (failures > 0)
            return Fail(ExitVerifyFail, $"{failures} 个黄金产物校验失败");

        var summary = verb switch
        {
            "gen" => $"gen: {artifacts.Length} 个黄金产物已写入",
            "check" => $"check: {artifacts.Length} 个产物双跑恒等且与入库黄金一致",
            _ => "verify: 全部通过",
        };
        Console.WriteLine(summary);
        return ExitOk;
    }

    static (string RelPath, byte[] Bytes)[] RunScenarioGroup(string openraRoot, string mode) => mode switch
    {
        "pathfinding" => PathfindingGroup(openraRoot),
        "combat" => CombatGroup(openraRoot),
        "production-positive" => ProductionGroup(openraRoot, positive: true),
        "production-negative" => ProductionGroup(openraRoot, positive: false),
        "economy" => EconomyGroup(openraRoot),
        "replay" => ReplayGroup(openraRoot),
        "widgets" => WidgetsGroup(openraRoot),
        _ => throw new ArgumentException($"未知场景模式：{mode}"),
    };
}
