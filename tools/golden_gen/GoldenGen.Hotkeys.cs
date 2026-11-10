internal static partial class Program
{
// —— hotkeys 模式（T22/issue #29）：轻路径，不装配 ModData/Game.Settings ——
// 复刻 HotkeyManager（HotkeyManager.cs:27-45）装载语义：mod.yaml Hotkeys: 列表序
// MiniYaml.Load Merge → 真·上游 HotkeyDefinition（解析/Types/Contexts/Platform/Readonly）
// → 合成 settings Keys: 覆盖段（跳 Readonly）→ GetFirstDuplicate 冲突复算
// （HotkeyManager.cs:91-106，Hotkey.operator== 的 UNKNOWN 永不等语义真源参与）。
// 覆盖解析失败走默认 InvalidValueAction（抛出）——合成段只放合法值。
// HasDuplicates 属性是 internal set，此处以 HashSet<string> 旁路记录同名布尔列。

    static readonly (string Name, string Value)[] HotkeysSyntheticKeysSegment =
    [
        // SelectAllUnits（player,spectator）与 Sell（player）语境相交 → 两者冲突
        ("SelectAllUnits", "Q Ctrl"),
        ("Sell", "Q Ctrl"),
        // Readonly 定义（chat.yaml Autocomplete=Tab）→ 覆盖被跳过，保持 Tab
        ("Autocomplete", "F2"),
        // ControlGroupSelect01 与 ControlGroupCombineWith01（同为 NUMBER_1 Shift，player）
        ("ControlGroupSelect01", "NUMBER_1 Shift"),
        // 未绑定定义写 UNKNOWN None：构造路径合法解析为 Hotkey.Invalid（无冲突）
        ("TogglePlayerStanceColor", "UNKNOWN None"),
    ];

    static (List<string> Files, List<HotkeyDefinition> Defs, Dictionary<string, Hotkey> Keys)
        LoadRaHotkeyDefinitions(string openraRoot, (string Name, string Value)[] settingsKeys)
    {
        var fs = new GoldenModRootFs(openraRoot);
        var modNodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
        var modYaml = new MiniYaml(null, MiniYaml.Merge([modNodes])).ToDictionary();
        var hotkeyFiles = modYaml["Hotkeys"].Nodes.Select(n => n.Key).ToList();

        var merged = MiniYaml.Load(fs, hotkeyFiles, null);
        var defs = new List<HotkeyDefinition>();
        var keys = new Dictionary<string, Hotkey>();
        foreach (var kd in merged)
        {
            var def = new HotkeyDefinition(kd.Key, kd.Value);
            defs.Add(def);
            keys[def.Name] = def.Default;
        }

        foreach (var (name, value) in settingsKeys)
        {
            var def = defs.FirstOrDefault(d => d.Name == name);
            if (def == null || def.Readonly)
                continue;
            keys[name] = FieldLoader.GetValue<Hotkey>(name, value);
        }

        return (hotkeyFiles, defs, keys);
    }

    // HotkeyManager.GetFirstDuplicate（HotkeyManager.cs:91-106）复刻：
    // keys 按插入序遍历（.NET Dictionary 无删除保插入序），值相等（UNKNOWN 永不等）
    // 且 Contexts 相交才算冲突。
    static HotkeyDefinition GetFirstHotkeyDuplicate(
        List<HotkeyDefinition> defs, Dictionary<string, Hotkey> keys,
        HotkeyDefinition definition, Hotkey value)
    {
        foreach (var kv in keys)
        {
            if (kv.Key == definition.Name)
                continue;

            var other = defs.First(d => d.Name == kv.Key);
            if (kv.Value == value && other.Contexts.Overlaps(definition.Contexts))
                return other;
        }

        return null;
    }

    static HashSet<string> ComputeHotkeyDuplicates(
        List<HotkeyDefinition> defs, Dictionary<string, Hotkey> keys)
    {
        var duplicateNames = new HashSet<string>();
        foreach (var def in defs)
            if (GetFirstHotkeyDuplicate(defs, keys, def, keys[def.Name]) != null)
                duplicateNames.Add(def.Name);
        return duplicateNames;
    }

    static byte[] DumpHotkeysTable(List<string> files, List<HotkeyDefinition> defs,
        Dictionary<string, Hotkey> keys, HashSet<string> duplicateNames, string title,
        bool withOverrideSegment)
    {
        var sb = new StringBuilder();
        sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 OpenRA HotkeyManager/HotkeyDefinition").Append('\n');
        sb.Append("# ").Append(title).Append('\n');
        sb.Append("# load order:").Append('\n');
        foreach (var f in files)
            sb.Append("#   ").Append(f).Append('\n');
        if (withOverrideSegment)
        {
            sb.Append("# synthetic settings Keys: segment (constructor override path):").Append('\n');
            foreach (var (name, value) in HotkeysSyntheticKeysSegment)
                sb.Append("#   ").Append(name).Append(": ").Append(value).Append('\n');
        }
        sb.Append("# columns: name<TAB>KEYCODE<TAB>MODIFIERS<TAB>HasDuplicates (canonical Modifiers:F)").Append('\n');

        foreach (var def in defs)
        {
            var key = keys[def.Name];
            sb.Append(def.Name).Append('\t').Append(key.Key).Append('\t')
                .Append(key.Modifiers.ToString("F")).Append('\t')
                .Append(duplicateNames.Contains(def.Name) ? "True" : "False").Append('\n');
        }

        var unbound = defs.Count(d => !keys[d.Name].IsValid());
        var readonlyCount = defs.Count(d => d.Readonly);
        sb.Append("# total=").Append(defs.Count).Append(" unbound=").Append(unbound)
            .Append(" readonly=").Append(readonlyCount)
            .Append(" duplicates=").Append(duplicateNames.Count).Append('\n');
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static byte[] DumpHotkeys(string openraRoot)
    {
        var (files, defs, keys) = LoadRaHotkeyDefinitions(openraRoot, []);
        var duplicates = ComputeHotkeyDuplicates(defs, keys);
        return DumpHotkeysTable(files, defs, keys, duplicates,
            "ra hotkey default bindings (174 definitions)", false);
    }

    static byte[] DumpHotkeysOverride(string openraRoot)
    {
        var (files, defs, keys) = LoadRaHotkeyDefinitions(openraRoot, HotkeysSyntheticKeysSegment);
        var duplicates = ComputeHotkeyDuplicates(defs, keys);
        return DumpHotkeysTable(files, defs, keys, duplicates,
            "ra hotkey bindings after synthetic settings Keys override", true);
    }

    static byte[] DumpHotkeysFilesManifest(string openraRoot)
    {
        var fs = new GoldenModRootFs(openraRoot);
        var modNodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
        var modYaml = new MiniYaml(null, MiniYaml.Merge([modNodes])).ToDictionary();
        var hotkeyFiles = modYaml["Hotkeys"].Nodes.Select(n => n.Key).ToList();

        var sb = new StringBuilder();
        foreach (var f in hotkeyFiles)
            sb.Append(f).Append('\n');
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static IEnumerable<(string RelName, string FullPath)> HotkeySourceFiles(string openraRoot)
    {
        var fs = new GoldenModRootFs(openraRoot);
        var modNodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
        var modYaml = new MiniYaml(null, MiniYaml.Merge([modNodes])).ToDictionary();
        var hotkeyFiles = modYaml["Hotkeys"].Nodes.Select(n => n.Key).ToList();
        return hotkeyFiles.Select(f =>
        {
            var sep = f.IndexOf('|');
            var rel = $"mods/{f[..sep]}/{f[(sep + 1)..]}";
            return (RelName: f.Replace('|', '-').Replace('/', '-'),
                FullPath: Path.Combine(openraRoot, rel.Replace('/', Path.DirectorySeparatorChar)));
        });
    }
}
