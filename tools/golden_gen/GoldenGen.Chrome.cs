using OpenRA.Graphics;

internal static partial class Program
{
// —— chrome 模式（T24/issue #33）：轻路径，不装配 ModData ——
// 四份语料原样入库（mods/ra/chrome.yaml、mods/ra/chrome/ingame-player.yaml、
// mods/common/metrics.yaml、mods/ra/metrics.yaml；GPL 随带 COPYING）+ 四份规范化转储：
//   ra-chrome-collections.txt  上游 MiniYaml.Merge + FieldLoader.Load<ChromeProvider.Collection>
//                              真源装载（^ 前缀抽象集合跳过，序 = Merge 输出序）——
//                              Image/Image2x/Image3x/PanelRegion(int 逗号)/PanelSides(数值)/
//                              Regions(名 x,y,w,h 保序)
//   metrics.txt                上游 ChromeMetrics.Initialize 语义（common+ra 双文件 Merge →
//                              顶层节点子键值覆盖表），dump 按 key ordinal 排序（C++ std::map 序）
//   ingame-player-tree.txt     装载树规范化转储：MiniYaml.FromStream 结构序 + 上游真源
//                              IntegerExpression.Evaluate 按 Widget.Initialize 公式
//                              （Widget.cs:268-283 复刻：WINDOW 1024x768、顶层挂哑 root
//                              PARENT=0、先 W/H 后 X/Y、WIDTH/HEIGHT 注入）逐节点求 bounds
//   expressions.txt            语料全部 X/Y/Width/Height 表达式（保序去重）× 固定
//                              substitution 集真源 Evaluate——求值器等价对拍
//
// 事件路由不进 oracle（上游 Widget.HandleMouseInputOuter 分散在各 Widget 子类虚方法，
// 无头不可驱动），由 C++ ctest 按上游精读逐行同构断言。

    const string ChromeGoldenRelDir = "tests/golden/chrome";
    const int ChromeWindowWidth = 1024;
    const int ChromeWindowHeight = 768;

    static readonly string[] ChromeMetricsFiles =
    [
        "mods/common/metrics.yaml",
        "mods/ra/metrics.yaml",
    ];

    static (string RelPath, string Mode, Func<byte[]> Produce)[] ChromeArtifacts(string openraRoot)
    {
        return
        [
            ($"{ChromeGoldenRelDir}/fixture/ra-chrome.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "chrome.yaml"))),
            ($"{ChromeGoldenRelDir}/fixture/ingame-player.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "chrome", "ingame-player.yaml"))),
            ($"{ChromeGoldenRelDir}/fixture/common-metrics.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "common", "metrics.yaml"))),
            ($"{ChromeGoldenRelDir}/fixture/ra-metrics.yaml", null,
                () => File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "metrics.yaml"))),
            ($"{ChromeGoldenRelDir}/ra-chrome-collections.txt", null,
                () => DumpChromeCollections(openraRoot)),
            ($"{ChromeGoldenRelDir}/metrics.txt", null,
                () => DumpChromeMetrics(openraRoot)),
            ($"{ChromeGoldenRelDir}/ingame-player-tree.txt", null,
                () => DumpChromeWidgetTree(openraRoot)),
            ($"{ChromeGoldenRelDir}/expressions.txt", null,
                () => DumpChromeExpressions(openraRoot)),
        ];
    }

    static byte[] ChromeTextEnd(StringBuilder sb)
    {
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    // 上游 ChromeProvider.Initialize（ChromeProvider.cs:69-85）：Merge 全部 chrome 文件后
    // 非 ^ 前缀集合逐个 FieldLoader.Load<Collection>。真源驱动。
    static byte[] DumpChromeCollections(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# chrome collections oracle (T24/issue #33)").Append('\n');
        sb.Append("# source: mods/ra/chrome.yaml via upstream MiniYaml.Merge + FieldLoader.Load<ChromeProvider.Collection>").Append('\n');
        sb.Append("# abstract collections (^ prefix) skipped; collection and region names ordinal sorted").Append('\n');
        using var stream = File.OpenRead(Path.Combine(openraRoot, "mods", "ra", "chrome.yaml"));
        var merged = MiniYaml.Merge([MiniYaml.FromStream(stream, "ra|chrome.yaml")]);
        var collections = new List<(string Name, ChromeProvider.Collection Collection)>();
        foreach (var c in merged)
        {
            if (c.Key.StartsWith('^'))
                continue;
            collections.Add((c.Key, FieldLoader.Load<ChromeProvider.Collection>(c.Value)));
        }

        foreach (var (name, col) in collections.OrderBy(k => k.Name, StringComparer.Ordinal))
        {
            sb.Append("collection\t").Append(name).Append('\n');
            sb.Append("Image\t").Append(col.Image ?? "").Append('\n');
            sb.Append("Image2x\t").Append(col.Image2x ?? "").Append('\n');
            sb.Append("Image3x\t").Append(col.Image3x ?? "").Append('\n');
            sb.Append("PanelRegion\t").Append(col.PanelRegion.IsDefault
                ? "" : string.Join(",", col.PanelRegion.ToArray())).Append('\n');
            sb.Append("PanelSides\t").Append((int)col.PanelSides).Append('\n');
            foreach (var region in col.Regions.OrderBy(r => r.Key, StringComparer.Ordinal))
                sb.Append("Region\t").Append(region.Key).Append('\t')
                    .Append(region.Value.X).Append(',')
                    .Append(region.Value.Y).Append(',')
                    .Append(region.Value.Width).Append(',')
                    .Append(region.Value.Height).Append('\n');
        }

        return ChromeTextEnd(sb);
    }

    // 上游 ChromeMetrics.Initialize（ChromeMetrics.cs:20-29）：多文件 Merge →
    // 全部顶层节点子键值覆盖表。dump 按 key ordinal 排序（对 C++ std::map 序）。
    static byte[] DumpChromeMetrics(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# chrome metrics oracle (T24/issue #33)").Append('\n');
        sb.Append("# source: common+ra metrics.yaml via upstream MiniYaml.Merge (ChromeMetrics.Initialize semantics)").Append('\n');
        sb.Append("# format: key<TAB>value, keys ordinal sorted").Append('\n');
        var data = new Dictionary<string, string>();
        var sources = ChromeMetricsFiles
            .Select(f => MiniYaml.FromStream(File.OpenRead(Path.Combine(openraRoot, f)), f))
            .ToList();
        var merged = MiniYaml.Merge(sources);
        foreach (var m in merged)
            foreach (var n in m.Value.Nodes)
                data[n.Key] = n.Value.Value;
        foreach (var key in data.Keys.OrderBy(k => k, StringComparer.Ordinal))
            sb.Append(key).Append('\t').Append(data[key]).Append('\n');
        return ChromeTextEnd(sb);
    }

    // Widget.cs Initialize（Widget.cs:268-283）公式的真源求值：表达式 Evaluate 用上游
    // IntegerExpression；substitutions 装配照 Ui.OpenWindow 形态（顶层挂 Ui.Root 哑容器，
    // PARENT = 哑 root bounds 0x0；WINDOW 1024x768）。
    static (int X, int Y, int Width, int Height) ChromeWidgetBounds(
        List<MiniYamlNode> fields, int parentWidth, int parentHeight)
    {
        string Expr(string key)
        {
            var node = fields.FirstOrDefault(n => n.Key == key);
            return node?.Value.Value;
        }

        var substitutions = new Dictionary<string, int>
        {
            ["WINDOW_WIDTH"] = ChromeWindowWidth,
            ["WINDOW_HEIGHT"] = ChromeWindowHeight,
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

    static void DumpChromeWidgetNode(StringBuilder sb, MiniYamlNode node, int depth, int parentWidth, int parentHeight)
    {
        var indent = new string('\t', depth);
        var at = node.Key.IndexOf('@');
        var type = at >= 0 ? node.Key[..at] : node.Key;
        var id = at >= 0 ? node.Key[(at + 1)..] : "";
        var fields = node.Value.Nodes.Where(n => n.Key != "Children").ToList();
        var (x, y, width, height) = ChromeWidgetBounds(fields, parentWidth, parentHeight);
        sb.Append(indent).Append(type).Append('@').Append(id)
            .Append('\t').Append(x).Append(',').Append(y).Append(',').Append(width).Append(',').Append(height).Append('\n');
        foreach (var f in fields)
            sb.Append(indent).Append('\t').Append("field\t").Append(f.Key).Append('\t').Append(f.Value.Value ?? "").Append('\n');
        foreach (var child in node.Value.Nodes.Where(n => n.Key == "Children"))
            foreach (var c in child.Value.Nodes)
                DumpChromeWidgetNode(sb, c, depth + 1, width, height);
    }

    static byte[] DumpChromeWidgetTree(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# chrome widget tree oracle (T24/issue #33)").Append('\n');
        sb.Append("# source: mods/ra/chrome/ingame-player.yaml; window=1024x768; top-level mounted on dumb root (PARENT=0)").Append('\n');
        sb.Append("# bounds via upstream IntegerExpression.Evaluate with Widget.Initialize substitution order (W/H before X/Y)").Append('\n');
        sb.Append("# format: <depth tabs>Type@Id<TAB>x,y,w,h ; field lines indented one deeper").Append('\n');
        using var stream = File.OpenRead(Path.Combine(openraRoot, "mods", "ra", "chrome", "ingame-player.yaml"));
        var nodes = MiniYaml.FromStream(stream, "ra|chrome/ingame-player.yaml");
        foreach (var top in nodes)
            DumpChromeWidgetNode(sb, top, 0, 0, 0);
        return ChromeTextEnd(sb);
    }

    static byte[] DumpChromeExpressions(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# chrome layout expressions oracle (T24/issue #33)").Append('\n');
        sb.Append("# all X/Y/Width/Height expressions from mods/ra/chrome/ingame-player.yaml + chrome.yaml PanelRegion-free texts,").Append('\n');
        sb.Append("# deduplicated in document order, evaluated with upstream IntegerExpression.Evaluate").Append('\n');
        sb.Append("# subs: WINDOW_WIDTH=1024 WINDOW_HEIGHT=768 PARENT_WIDTH=238 PARENT_HEIGHT=250 WIDTH=62 HEIGHT=46").Append('\n');
        sb.Append("# format: <expr><TAB>value").Append('\n');
        var seen = new HashSet<string>();
        var exprs = new List<string>();
        void Collect(IEnumerable<MiniYamlNode> nodes)
        {
            foreach (var n in nodes)
            {
                if (n.Key is "X" or "Y" or "Width" or "Height")
                {
                    var expr = n.Value.Value;
                    if (!string.IsNullOrEmpty(expr) && seen.Add(expr))
                        exprs.Add(expr);
                }

                Collect(n.Value.Nodes);
            }
        }

        Collect(MiniYaml.FromStream(
            File.OpenRead(Path.Combine(openraRoot, "mods", "ra", "chrome", "ingame-player.yaml")),
            "ra|chrome/ingame-player.yaml"));
        var substitutions = new Dictionary<string, int>
        {
            ["WINDOW_WIDTH"] = ChromeWindowWidth,
            ["WINDOW_HEIGHT"] = ChromeWindowHeight,
            ["PARENT_WIDTH"] = 238,
            ["PARENT_HEIGHT"] = 250,
            ["WIDTH"] = 62,
            ["HEIGHT"] = 46,
        };
        foreach (var expr in exprs)
            sb.Append(expr).Append('\t')
                .Append(new IntegerExpression(expr).Evaluate(substitutions)
                    .ToString(CultureInfo.InvariantCulture))
                .Append('\n');
        return ChromeTextEnd(sb);
    }
}
