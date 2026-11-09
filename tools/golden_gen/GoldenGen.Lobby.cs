using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using OpenRA.FileFormats;
using OpenRA.Graphics;
using OpenRA.Network;
using OpenRA.Primitives;
using OpenRA.Server;
using OpenRA.Traits;

internal static partial class Program
{
// —— lobby 模式（T27/issue #36）：遭遇战大堂最小面 oracle ——
// 驱动面裁定（chrome 模式先例延续）：上游 LobbyLogic（Widgets/Logic，依赖
// WorldRenderer/OrderManager/真连接）与 LobbyCommands.InterpretCommand 全量（依赖真
// TCP Server/FluentProvider 广播，Server ctor 即起监听线程且 RandomSeed 取实时钟，
// 无头不可确定性驱动）不进 oracle；由可无头驱动的真源单元产出黄金：
//   fixture 三 yaml      mods/common/chrome/{lobby,lobby-players,lobby-mappreview}.yaml
//                        原样入库（GPL，漂移哨兵）
//   chrome-trees.txt     lobby.yaml 族装载树 bounds 转储（chrome 模式同款：真源
//                        MiniYaml.FromStream 结构序 + IntegerExpression.Evaluate 按
//                        Widget.Initialize 公式，窗口 1024x768 哑 root PARENT=0）
//   preview.txt          预览渲染真值：三语料地图 map.png 真源 Png 解码（尺寸 +
//                        Sheet(BGRA) 字节 SHA256 + 采样网格，widgets 模式 pixels 同链）
//                        + MapPreviewWidget.Draw 缩放居中公式输出（previewScale=min、
//                        (int)截断、(rb.W-w)/2 int 除）——上游大堂预览 = map.png 解码
//                        直绘（MapCache.LoadAsyncInternal sheetBuilder.Add(p.Preview)），
//                        无地形快照回落，C++ 同构（T25 png_decode 直用）
//   slots.txt（重组件）  真源 new Map(modData, oramap 包) 的 Players.Players
//                        PlayerReference 槽位面（MakeSlotFromPlayerReference 源数据：
//                        Name/Playable/AllowBots/Lock*/Required/Faction/Team/Bot 保序）
//                        + 地图可选势力表（真 WorldActorInfo.TraitInfos<FactionInfo>()
//                        .Where(Selectable) 的 InternalName 序）
//   session.txt（重组件）大堂操作序列终态：脚本化命令（server-started/join/faction/
//                        team/map/state/startgame）照 LobbyCommands.cs 各 handler 读源
//                        装配上游真源 Session 对象（Client/Slot/Global 直构，
//                        SyncClientToPlayerReference/SanitizePlayerFaction 真函数调用），
//                        每步 dump 真 Session.Serialize() 字节（FieldSaver+WriteToString，
//                        序列化零复刻）。C++ 侧独立实现同一语义，两侧仅共享上游源码
//                        这一真源（黄金对拍口径）。颜色经真 IColorPickerManagerInfo
//                        .MakeValid 探针定值（固定种子，双跑恒等）；GameUid 归一化为
//                        固定常量（settings 模式 UUID 归一化先例）。

    const string LobbyGoldenRelDir = "tests/golden/lobby";
    const int LobbyWindowWidth = 1024;
    const int LobbyWindowHeight = 768;

    static readonly string[] LobbyChromeFiles =
    [
        "lobby.yaml",
        "lobby-players.yaml",
        "lobby-mappreview.yaml",
    ];

    // session 剧本固定参数（两侧字面量一致）
    const string LobbyServerName = "";
    const string LobbyGameUid = "00000000-0000-0000-0000-000000000000";
    const int LobbyRandomSeed = 0;
    const string LobbyClientName = "Commander";
    const uint LobbyClientArgb = 0xFFC81E28u; // Color.FromArgb(0xC8,0x1E,0x28) 固定色

    static (string RelPath, string Mode, Func<byte[]> Produce)[] LobbyArtifacts(string openraRoot)
    {
        return
        [
            .. LobbyChromeFiles.Select(name => (
                $"{LobbyGoldenRelDir}/fixture/{name}",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(
                    Path.Combine(openraRoot, "mods", "common", "chrome", name))))),
            ($"{LobbyGoldenRelDir}/chrome-trees.txt", null, () => DumpLobbyChromeTrees(openraRoot)),
            ($"{LobbyGoldenRelDir}/preview.txt", null, () => DumpLobbyPreview(openraRoot)),
            ($"{LobbyGoldenRelDir}/slots.txt", "lobby", null),
            ($"{LobbyGoldenRelDir}/session.txt", "lobby", null),
        ];
    }

    static byte[] LobbyTextEnd(StringBuilder sb)
    {
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    // —— chrome-trees：lobby.yaml 族装载树（chrome 模式公式照搬）——

    static (int X, int Y, int Width, int Height) LobbyWidgetBounds(
        List<MiniYamlNode> fields, int parentWidth, int parentHeight)
    {
        string Expr(string key)
        {
            var node = fields.FirstOrDefault(n => n.Key == key);
            return node?.Value.Value;
        }

        var substitutions = new Dictionary<string, int>
        {
            ["WINDOW_WIDTH"] = LobbyWindowWidth,
            ["WINDOW_HEIGHT"] = LobbyWindowHeight,
            ["PARENT_WIDTH"] = parentWidth,
            ["PARENT_HEIGHT"] = parentHeight,
        };
        var width = Expr("Width") != null ? new IntegerExpression(Expr("Width")).Evaluate(substitutions) : 0;
        var height = Expr("Height") != null ? new IntegerExpression(Expr("Height")).Evaluate(substitutions) : 0;
        substitutions["WIDTH"] = width;
        substitutions["HEIGHT"] = height;
        var x = Expr("X") != null ? new IntegerExpression(Expr("X")).Evaluate(substitutions) : 0;
        var y = Expr("Y") != null ? new IntegerExpression(Expr("Y")).Evaluate(substitutions) : 0;
        return (x, y, width, height);
    }

    static void DumpLobbyWidgetNode(StringBuilder sb, MiniYamlNode node, int depth, int parentWidth, int parentHeight)
    {
        var indent = new string('\t', depth);
        var at = node.Key.IndexOf('@');
        var type = at >= 0 ? node.Key[..at] : node.Key;
        var id = at >= 0 ? node.Key[(at + 1)..] : "";
        var fields = node.Value.Nodes.Where(n => n.Key != "Children").ToList();
        var (x, y, width, height) = LobbyWidgetBounds(fields, parentWidth, parentHeight);
        sb.Append(indent).Append(type).Append('@').Append(id)
            .Append('\t').Append(x).Append(',').Append(y).Append(',').Append(width).Append(',').Append(height).Append('\n');
        foreach (var f in fields)
            sb.Append(indent).Append('\t').Append("field\t").Append(f.Key).Append('\t').Append(f.Value.Value ?? "").Append('\n');
        foreach (var child in node.Value.Nodes.Where(n => n.Key == "Children"))
            foreach (var c in child.Value.Nodes)
                DumpLobbyWidgetNode(sb, c, depth + 1, width, height);
    }

    static byte[] DumpLobbyChromeTrees(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# lobby chrome trees oracle (T27/issue #36)").Append('\n');
        sb.Append("# source: mods/common/chrome/{lobby,lobby-players,lobby-mappreview}.yaml; window=1024x768; top-level on dumb root (PARENT=0)").Append('\n');
        sb.Append("# bounds via upstream IntegerExpression.Evaluate with Widget.Initialize substitution order (W/H before X/Y)").Append('\n');
        foreach (var name in LobbyChromeFiles)
        {
            sb.Append("# file ").Append(name).Append('\n');
            using var stream = File.OpenRead(Path.Combine(openraRoot, "mods", "common", "chrome", name));
            var nodes = MiniYaml.FromStream(stream, "common|chrome/" + name);
            foreach (var top in nodes)
                DumpLobbyWidgetNode(sb, top, 0, 0, 0);
        }

        return LobbyTextEnd(sb);
    }

    // —— preview：map.png 真源解码 + MapPreviewWidget.Draw 缩放居中公式 ——

    static byte[] DumpLobbyPreview(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# lobby preview oracle (T27/issue #36)").Append('\n');
        sb.Append("# upstream lobby minimap = map.png decoded by Png and drawn via sheetBuilder.Add(p.Preview) (MapCache.LoadAsyncInternal);").Append('\n');
        sb.Append("# no terrain-snapshot fallback exists in upstream lobby preview. C++ uses T25 png_decode directly.").Append('\n');
        sb.Append("# scale/center via MapPreviewWidget.Draw: scale=min(rb.W/w,rb.H/h) float; w/h=(int)(scale*size); x=rb.X+(rb.W-w)/2 int div").Append('\n');
        sb.Append("# widget bounds from true lobby-mappreview.yaml tree: MAP_LARGE/MAP_PREVIEW 1,1,172,172 (see chrome-trees)").Append('\n');
        foreach (var mapName in State.MapNames)
        {
            using var zip = ZipFile.OpenRead(Path.Combine(openraRoot, "mods", "ra", "maps", mapName + ".oramap"));
            var entry = zip.GetEntry("map.png") ?? throw new InvalidOperationException($"{mapName}.oramap lacks map.png");
            using var es = entry.Open();
            using var ms = new MemoryStream();
            es.CopyTo(ms);
            var png = new Png(new MemoryStream(ms.ToArray()));

            var sheet = new Sheet(SheetType.BGRA, new Size(png.Width, png.Height));
            var sprite = new Sprite(sheet, new Rectangle(0, 0, png.Width, png.Height), TextureChannel.Red);
            Util.FastCopyIntoSprite(sprite, png);
            var data = sheet.GetData();
            var pixels = MemoryMarshal.Cast<byte, uint>(data);

            sb.Append("map ").Append(mapName).Append(' ').Append(png.Width).Append('x').Append(png.Height)
                .Append(" sha256=").Append(Convert.ToHexStringLower(SHA256.HashData(data))).Append('\n');
            for (var y = 0; y < png.Height; y += 53)
                for (var x = 0; x < png.Width; x += 47)
                    sb.Append("p ").Append(x).Append(',').Append(y).Append(' ')
                        .Append(pixels[y * png.Width + x].ToString("X8", CultureInfo.InvariantCulture)).Append('\n');

            // MapPreviewWidget.Draw 的缩放居中（RenderBounds=MAP_PREVIEW 172x172 @1,1）
            const int RbX = 1, RbY = 1, RbW = 172, RbH = 172;
            var previewScale = Math.Min(RbW / (float)png.Width, RbH / (float)png.Height);
            var rw = (int)(previewScale * png.Width);
            var rh = (int)(previewScale * png.Height);
            var rx = RbX + (RbW - rw) / 2;
            var ry = RbY + (RbH - rh) / 2;
            sb.Append("rect ").Append(mapName).Append(' ').Append(rx).Append(',').Append(ry).Append(',')
                .Append(rw).Append(',').Append(rh).Append('\n');
        }

        return LobbyTextEnd(sb);
    }

    // —— 重组件装配（ModData + ra + 三地图包；组内共享一次装配）——

    static ModData LobbyAsm;

    static (ModData ModData, Dictionary<string, Map> Maps) LobbyAssemble(string openraRoot)
    {
        HeadlessHost.Boot(openraRoot);
        if (LobbyAsm is not { } modData)
        {
            var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
            modData = new ModData(mods["ra"], mods);
            Game.ModData = modData;
            Game.Sound = new Sound(new StubPlatform(), new SoundSettings());
            LobbyAsm = modData;
        }

        // Map 每跑新建（包一次装配缓存 ModData；探针实证同进程多 pass 恒等）
        var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", "ra", "maps"));
        var maps = new Dictionary<string, Map>();
        foreach (var name in State.MapNames)
        {
            var package = mapsDir.OpenPackage(name + ".oramap", modData.ModFiles)
                ?? throw new InvalidOperationException($"cannot open {name}.oramap");
            maps[name] = new Map(modData, package);
        }

        return (modData, maps);
    }

    static byte[] DumpLobbySlots(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# lobby slots oracle (T27/issue #36)").Append('\n');
        sb.Append("# true Map(modData, oramap) PlayerDefinitions via MapPlayers PlayerReference fields (MakeSlotFromPlayerReference source data),").Append('\n');
        sb.Append("# document order; slot lines only for Playable refs. factions = map rules world actor selectable FactionInfo InternalNames").Append('\n');
        var (_, maps) = LobbyAssemble(openraRoot);
        foreach (var (name, map) in maps)
        {
            sb.Append("map ").Append(name).Append(" uid ").Append(map.Uid).Append('\n');
            foreach (var pr in new MapPlayers(map.PlayerDefinitions).Players.Values)
                sb.Append("player ").Append(pr.Name)
                    .Append(" playable=").Append(pr.Playable)
                    .Append(" allowBots=").Append(pr.AllowBots)
                    .Append(" lockFaction=").Append(pr.LockFaction)
                    .Append(" lockColor=").Append(pr.LockColor)
                    .Append(" lockTeam=").Append(pr.LockTeam)
                    .Append(" lockHandicap=").Append(pr.LockHandicap)
                    .Append(" lockSpawn=").Append(pr.LockSpawn)
                    .Append(" required=").Append(pr.Required)
                    .Append(" faction=").Append(pr.Faction ?? "")
                    .Append(" team=").Append(pr.Team)
                    .Append(" bot=").Append(pr.Bot ?? "")
                    .Append('\n');
            var factions = string.Join(",", map.Rules.Actors[SystemActors.World].TraitInfos<FactionInfo>()
                .Where(f => f.Selectable).Select(f => f.InternalName));
            sb.Append("factions ").Append(factions).Append('\n');
        }

        return LobbyTextEnd(sb);
    }

    // —— session：脚本化大堂操作序列逐步真源 Session.Serialize() ——

    static byte[] DumpLobbySession(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# lobby session oracle (T27/issue #36)").Append('\n');
        sb.Append("# scripted lobby ops assembled into true-source Session per LobbyCommands.cs handler semantics;").Append('\n');
        sb.Append("# each step dumped via upstream Session.Serialize() (FieldSaver + WriteToString, zero reimplementation).").Append('\n');
        sb.Append("# normalizations: GameUid fixed constant (upstream Guid.NewGuid), RandomSeed fixed 0 (upstream DateTime seed),").Append('\n');
        sb.Append("# ServerName '' (ServerSettings.Name default). color fixed literal (MakeValid not ported, see COVERAGE).").Append('\n');
        var (_, maps) = LobbyAssemble(openraRoot);

        // 颜色裁定：SanitizePlayerColor/IColorPickerManagerInfo.MakeValid 属 Mods.Common ALC
        // 隔离面且最小面不移植（C++ 颜色为透传字面量，COVERAGE 登记）；剧本用固定色，
        // 两侧字面量一致即字节恒等
        var probeColor = Color.FromArgb((uint)LobbyClientArgb);
        sb.Append("color fixed ").Append(FieldSaver.FormatValue(probeColor)).Append('\n');

        var selectableFactions = maps["ore-lord"].Rules.Actors[SystemActors.World].TraitInfos<FactionInfo>()
            .Where(f => f.Selectable).Select(f => f.InternalName).ToList();

        Session MakeStarted(Map map)
        {
            // Server ctor + LobbyCommands.ServerStarted（LobbyCommands.cs:1348-1367 + Server.cs:325-341）
            var gs = new Session.Global
            {
                ServerName = LobbyServerName,
                Map = map.Uid,
                MapStatus = Session.MapStatus.Playable,
                RandomSeed = LobbyRandomSeed,
                AllowSpectators = true,
                GameUid = LobbyGameUid,
                EnableSingleplayer = true,
                EnableMapGeneration = true,
                EnableSyncReports = false,
                Dedicated = false,
                NetFrameInterval = 3,
                GameTimestep = 0,
            };
            return new Session
            {
                GlobalSettings = gs,
                Slots = new MapPlayers(map.PlayerDefinitions).Players.Values
                    .Select(MakeSlotFromPlayerReferenceUpstream)
                    .Where(s => s != null)
                    .ToDictionary(s => s.PlayerReference),
            };
        }

        Session.Slot MakeSlotFromPlayerReferenceUpstream(PlayerReference pr)
        {
            if (!pr.Playable)
                return null;
            return new Session.Slot
            {
                PlayerReference = pr.Name,
                Closed = false,
                AllowBots = pr.AllowBots,
                LockFaction = pr.LockFaction,
                LockColor = pr.LockColor,
                LockTeam = pr.LockTeam,
                LockHandicap = pr.LockHandicap,
                LockSpawn = pr.LockSpawn,
                Required = pr.Required,
            };
        }

        void DumpStep(string op, Session session)
        {
            sb.Append("step ").Append(op).Append('\n');
            sb.Append("--- session ---").Append('\n');
            sb.Append(session.Serialize());
            sb.Append("--- end ---").Append('\n');
        }

        // [server-started] ore-lord：初始槽位装载
        var session = MakeStarted(maps["ore-lord"]);
        DumpStep("server-started ore-lord", session);

        // [join] 本地 client 0（Server.cs:491-507 握手构造 + CompleteConnection 547-566）：
        // Faction=Random/SpawnPoint=0/Team=0/Handicap=0/State=Invalid；Slot=FirstEmptySlot()；
        // IsAdmin=!Clients.Any(IsAdmin)；SyncClientToPlayerReference（LockColor false → Color=PreferredColor）
        var client = new Session.Client
        {
            Name = LobbyClientName,
            Index = 0,
            PreferredColor = probeColor,
            Color = probeColor,
            Faction = "Random",
            SpawnPoint = 0,
            Team = 0,
            Handicap = 0,
            State = Session.ClientState.Invalid,
            ConnectionQuality = Session.ConnectionQuality.Good,
        };
        client.Slot = session.FirstEmptySlot();
        client.IsAdmin = !session.Clients.Any(c => c.IsAdmin);
        if (client.Slot != null)
            Server.SyncClientToPlayerReference(client, new MapPlayers(maps["ore-lord"].PlayerDefinitions).Players[client.Slot]);
        session.Clients.Add(client);
        DumpStep("join-client-0 " + LobbyClientName, session);

        // [faction 0 <f>]（LobbyCommands.cs:1019-1049）：目标 client 有槽、admin 可改、
        // LockFaction false、f ∈ 地图可选势力表
        void CmdFaction(string f, bool expectOk)
        {
            var target = session.ClientWithIndex(0);
            var ok = target.Slot != null && target.IsAdmin
                && !session.Slots[target.Slot].LockFaction
                && selectableFactions.Contains(f);
            if (ok)
                target.Faction = f;
            sb.Append("cmd faction 0 ").Append(f).Append(" -> ").Append(ok ? "ok" : "rejected").Append('\n');
            DumpStep("faction 0 " + f, session);
            _ = expectOk;
        }

        CmdFaction("russia", true);
        CmdFaction("creepus", false);

        // [team 0 <n>]（LobbyCommands.cs:1051-1077）：LockTeam false 即赋值
        session.ClientWithIndex(0).Team = 2;
        sb.Append("cmd team 0 2 -> ok").Append('\n');
        DumpStep("team 0 2", session);
        session.ClientWithIndex(0).Team = 0;
        sb.Append("cmd team 0 0 -> ok").Append('\n');
        DumpStep("team 0 0", session);

        // [map <uid>]（LobbyCommands.cs:561-660）：admin；Global.Map=uid、槽位按新图重建、
        // 全 client State=Invalid、faction 按新图可选表 sanitize、SpawnPoint=0、按旧槽序
        // 重指派到新槽（单人：旧 Multi0 → 新 slots[0]）、DisabledSpawnPoints 清空
        void CmdMap(string mapName)
        {
            var map = maps[mapName];
            var oldSlots = session.Slots.Keys.ToArray();
            session.GlobalSettings.Map = map.Uid;
            session.Slots = new MapPlayers(map.PlayerDefinitions).Players.Values
                .Select(MakeSlotFromPlayerReferenceUpstream)
                .Where(s => s != null)
                .ToDictionary(s => s.PlayerReference);
            var newSelectable = map.Rules.Actors[SystemActors.World].TraitInfos<FactionInfo>()
                .Where(f => f.Selectable).Select(f => f.InternalName).ToList();
            foreach (var c in session.Clients)
            {
                c.Faction = !newSelectable.Contains(c.Faction) ? "Random" : c.Faction;
                c.State = Session.ClientState.Invalid;
            }

            var slots = session.Slots.Keys.ToArray();
            var i = 0;
            foreach (var os in oldSlots)
            {
                var c = session.ClientInSlot(os);
                if (c == null)
                    continue;
                c.SpawnPoint = 0;
                c.Slot = i < slots.Length ? slots[i++] : null;
                if (c.Slot != null)
                    Server.SyncClientToPlayerReference(c, new MapPlayers(map.PlayerDefinitions).Players[c.Slot]);
                else
                    c.Color = Color.White;
            }

            session.DisabledSpawnPoints.Clear();
            sb.Append("cmd map ").Append(mapName).Append(" -> ok").Append('\n');
            DumpStep("map " + mapName, session);
        }

        CmdMap("doubles");
        CmdMap("ore-lord");

        // [faction 0 Random]：Random ∈ 可选表（ra Faction@random 无 Selectable: False）
        CmdFaction("Random", true);

        // [state Ready]（LobbyCommands.cs:316-335）：State=Ready；CheckAutoStart 条件满足
        // （单非 bot player、EnableSingleplayer、无 Required 空槽、 spawns 足够）→ 服务端
        // StartGame——导航面不进 oracle，dump 停在就绪终态
        session.ClientWithIndex(0).State = Session.ClientState.Ready;
        sb.Append("cmd state Ready -> ok").Append('\n');
        DumpStep("state Ready", session);

        // [startgame]（LobbyCommands.cs:316-352 + Server.StartGame 1349-1400）：
        // host/单人校验通过；进 session 的可观测变更 = EnableGameSaves=True
        // （integrated 且非 bot client 数 1）
        session.GlobalSettings.EnableGameSaves = true;
        sb.Append("cmd startgame -> ok").Append('\n');
        DumpStep("startgame", session);

        return LobbyTextEnd(sb);
    }

    static (string RelPath, byte[] Bytes)[] LobbyGroup(string openraRoot)
    {
        // Game.Settings 兜底（widgets 模式同款）：全默认 Settings 下 DiscordService 走
        // Game.Renderer==null 早退；ModData 装配需要
        Game.Settings = new Settings(null, new Arguments());
        var slots = DumpLobbySlots(openraRoot);
        var session = DumpLobbySession(openraRoot);
        return
        [
            ($"{LobbyGoldenRelDir}/slots.txt", slots),
            ($"{LobbyGoldenRelDir}/session.txt", session),
        ];
    }
}
