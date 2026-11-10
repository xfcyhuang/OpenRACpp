using System.Runtime.InteropServices;

internal static partial class Program
{
// —— fonts 模式（T23/issue #30）：轻路径，不装配 ModData/Game.Settings ——
// 逐字复刻上游 FreeTypeFont.cs 的 FreeType interop（同 x64 偏移、同 (uint) 截断、
// 同 >>6 26.6 截断、同 8bpp 逐行拷贝），dump 出 FTD1u8 二进制（metrics+位图）。
// Measure/WrapText oracle 按 SpriteFont.cs:225-250 / WidgetUtils.cs:243-283
// 逐行复刻（advance 求和 → /deviceScale → Math.Ceiling；deviceScale 钉 1.0f）。
// freetype6.dll 取 openra/bin（上游构建产物；缺失则整组产物跳过，与 render_assets
// 内容包 SKIP 同精神——C++ ctest 读的是入库黄金，不依赖该 dll）。
//
// 产物：
//   fixture/FreeSans.ttf,FreeSansBold.ttf   上游 mods/common 原样字节拷贝（漂移哨兵兼 C++ 测试输入）
//   ra-mod.yaml                             上游 mods/ra/mod.yaml 原样拷贝（漂移哨兵）
//   ra-fonts.txt                            mod.yaml Fonts: 段三元组解析转储（yaml 文档序）
//   ft-<font>-<size>.bin                    FTD1u8：ASCII 0x20..0x7E per cp {cp,w,h,adv,offX,offY,w*h 灰度}
//   ft-misc-FreeSans-14.bin                 显式特殊 cp 集（空格/nbsp/DEL/CJK/☺/孤立高位代理/'0'）
//   strings.txt                             字串集（转储与 C++ 测试共享的单一事实源）
//   measure-<font>-<size>.txt               Measure oracle 输出
//   wrap-<font>-<size>-w<width>.txt         WrapText oracle 输出（含超长词放弃/插入行再处理路径）

    const string FontsGoldenRelDir = "tests/golden/fonts";

    static readonly (string Font, int Size)[] FontsFreeSansDumpSizes =
    [
        ("FreeSans", 10), ("FreeSans", 12), ("FreeSans", 14), ("FreeSans", 18), ("FreeSans", 24),
    ];

    static readonly (string Font, int Size)[] FontsMetricsDumpSpecs =
    [
        .. FontsFreeSansDumpSizes,
        ("FreeSansBold", 14),
    ];

    // 特殊 cp 集：0x20 空格、0xA0 nbsp（cmap 有）、0x7F DEL（cmap 无→.notdef）、
    // 0x4E2D 中（cmap 无→.notdef 方框）、0x263A ☺（cmap 无）、0xD83D 孤立高位代理、
    // 0x30 '0'（正常字形对照）
    static readonly int[] FontsMiscCodepoints = [0x20, 0xA0, 0x7F, 0x4E2D, 0x263A, 0xD83D, 0x30];

    // (index, text)：转义后与 strings.txt 行序一致；全部可打印 ASCII + 显式 \n
    static readonly (int Index, string Text)[] FontsStrings =
    [
        (0, ""),
        (1, " "),
        (2, "A"),
        (3, "Hello World"),
        (4, "The quick brown fox jumps over the lazy dog"),
        (5, "line one\nline two\nline three"),
        (6, "\n"),
        (7, "a\n\nb"),
        (8, "trailing spaces   "),
        (9, "   leading spaces"),
        (10, "Supercalifragilisticexpialidocious"),
        (11, "a verylongunbreakableword here"),
        (12, "word word word word"),
        (13, "  double  spaces  inside"),
        (14, "OpenRA C++ port measures and wraps text like upstream FreeType dumps"),
        (15, "!!!???...,,," ),
    ];

    static readonly (string Font, int Size)[] FontsMeasureSpecs =
    [
        ("FreeSans", 14), ("FreeSans", 10), ("FreeSansBold", 14),
    ];

    static readonly (string Font, int Size, int Width)[] FontsWrapSpecs =
    [
        ("FreeSans", 14, 40), ("FreeSans", 14, 80), ("FreeSans", 10, 32), ("FreeSansBold", 14, 60),
    ];

    static bool FontsAvailable(string openraRoot) =>
        File.Exists(Path.Combine(openraRoot, "bin", "freetype6.dll"))
        && File.Exists(Path.Combine(openraRoot, "mods", "common", "FreeSans.ttf"))
        && File.Exists(Path.Combine(openraRoot, "mods", "common", "FreeSansBold.ttf"))
        && File.Exists(Path.Combine(openraRoot, "mods", "ra", "mod.yaml"));

    static (string RelPath, string Mode, Func<byte[]> Produce)[] FontsArtifacts(string openraRoot)
    {
        var dir = FontsGoldenRelDir;
        return
        [
            ($"{dir}/fixture/FreeSans.ttf", null, () =>
                File.ReadAllBytes(Path.Combine(openraRoot, "mods", "common", "FreeSans.ttf"))),
            ($"{dir}/fixture/FreeSansBold.ttf", null, () =>
                File.ReadAllBytes(Path.Combine(openraRoot, "mods", "common", "FreeSansBold.ttf"))),
            ($"{dir}/ra-mod.yaml", null, () =>
                File.ReadAllBytes(Path.Combine(openraRoot, "mods", "ra", "mod.yaml"))),
            ($"{dir}/ra-fonts.txt", null, () => DumpFontsManifest(openraRoot)),
            .. FontsMetricsDumpSpecs.Select(spec => (
                $"{dir}/ft-{spec.Font}-{spec.Size}.bin", (string)null,
                (Func<byte[]>)(() => DumpFontsFtBitmap(openraRoot, spec.Font, spec.Size, null)))),
            ($"{dir}/ft-misc-FreeSans-14.bin", null,
                () => DumpFontsFtBitmap(openraRoot, "FreeSans", 14, FontsMiscCodepoints)),
            ($"{dir}/strings.txt", null, () => DumpFontsStrings()),
            .. FontsMeasureSpecs.Select(spec => (
                $"{dir}/measure-{spec.Font}-{spec.Size}.txt", (string)null,
                (Func<byte[]>)(() => DumpFontsMeasure(openraRoot, spec.Font, spec.Size)))),
            .. FontsWrapSpecs.Select(spec => (
                $"{dir}/wrap-{spec.Font}-{spec.Size}-w{spec.Width}.txt", (string)null,
                (Func<byte[]>)(() => DumpFontsWrap(openraRoot, spec.Font, spec.Size, spec.Width)))),
        ];
    }

    static string FontsEscape(string text) => text
        .Replace("\\", "\\\\").Replace("\n", "\\n").Replace("\t", "\\t").Replace("\r", "\\r");

    static byte[] DumpFontsStrings()
    {
        var sb = new StringBuilder();
        sb.Append("# T23 oracle strings set (T23/issue #30)").Append('\n');
        sb.Append("# escaping: backslash->\\\\, newline->\\n, tab->\\t, CR->\\r; "
            + "unescaped text is UTF-16 code units (C# string)").Append('\n');
        sb.Append("# format: <index><TAB><escaped>").Append('\n');
        foreach (var (index, text) in FontsStrings)
            sb.Append(index).Append('\t').Append(FontsEscape(text)).Append('\n');
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    // 上游 Fonts.cs LoadFonts（Fonts.cs:26-33）：mod.yaml Fonts: 段逐节点
    // FieldLoader.Load<FontData>——Font/Size/Ascender 子键，缺省留默认。
    static (string Name, string Font, int Size, int Ascender)[] LoadFontsManifest(string openraRoot)
    {
        var fs = new GoldenModRootFs(openraRoot);
        var modNodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
        var modYaml = new MiniYaml(null, MiniYaml.Merge([modNodes])).ToDictionary();
        return modYaml["Fonts"].Nodes
            .Select(node => (
                Name: node.Key,
                Font: node.Value.Nodes.FirstOrDefault(n => n.Key == "Font")?.Value.Value,
                Size: int.Parse(node.Value.Nodes.FirstOrDefault(n => n.Key == "Size")?.Value.Value ?? "0",
                    CultureInfo.InvariantCulture),
                Ascender: int.Parse(node.Value.Nodes.FirstOrDefault(n => n.Key == "Ascender")?.Value.Value ?? "0",
                    CultureInfo.InvariantCulture)))
            .ToArray();
    }

    static byte[] DumpFontsManifest(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# ra mod.yaml Fonts: section parsed with upstream Fonts.LoadFonts semantics").Append('\n');
        sb.Append("# format: <name><TAB><font><TAB><size><TAB><ascender> (yaml document order)").Append('\n');
        foreach (var (name, font, size, ascender) in LoadFontsManifest(openraRoot))
            sb.Append(name).Append('\t').Append(font).Append('\t')
                .Append(size.ToString(CultureInfo.InvariantCulture)).Append('\t')
                .Append(ascender.ToString(CultureInfo.InvariantCulture)).Append('\n');
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    // FTD1u8（与探索产物 t23-scratch/ft-dump 同布局）：
    //   "FTD1u8" | int32 sizeCount | per size { int32 size; int32 cpCount;
    //     per cp { int32 cp; int32 w; int32 h; float adv; int32 offX; int32 offY; w*h bytes } }
    //   | int32 upem | int32 hheaAsc | int32 hheaDesc | int32 hheaGap
    static byte[] DumpFontsFtBitmap(string openraRoot, string fontName, int size, int[] codepoints)
    {
        var fontPath = fontName switch
        {
            "FreeSans" => Path.Combine(openraRoot, "mods", "common", "FreeSans.ttf"),
            "FreeSansBold" => Path.Combine(openraRoot, "mods", "common", "FreeSansBold.ttf"),
            _ => throw new ArgumentException($"未知字体 fixture：{fontName}"),
        };

        var cps = codepoints ?? Enumerable.Range(0x20, 0x7f - 0x20).ToArray();
        var fontData = File.ReadAllBytes(fontPath);
        using var fonts = FontsFtHost.Load();
        using var face = FontsFtHost.LoadFace(fonts, fontData);

        using var ms = new MemoryStream();
        using var bw = new BinaryWriter(ms);
        bw.Write("FTD1u8"u8.ToArray());
        bw.Write(1);
        bw.Write(size);
        bw.Write(cps.Length);
        foreach (var cp in cps)
        {
            var g = FontsFtHost.CreateGlyph(face.Handle, (char)cp, size, 1.0f);
            bw.Write(cp);
            bw.Write(g.Width);
            bw.Write(g.Height);
            bw.Write(g.Advance);
            bw.Write(g.OffsetX);
            bw.Write(g.OffsetY);
            if (g.Data != null) bw.Write(g.Data);
        }

        var (upem, asc, desc, gap) = FontsReadTtfMetrics(fontData);
        bw.Write(upem);
        bw.Write(asc);
        bw.Write(desc);
        bw.Write(gap);
        bw.Flush();
        return ms.ToArray();
    }

    static (int upem, int asc, int desc, int gap) FontsReadTtfMetrics(byte[] d)
    {
        int Be16(int o) => (d[o] << 8) | d[o + 1];
        int Be32(int o) => (d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3];
        var numTables = Be16(4);
        int upem = 0, asc = 0, desc = 0, gap = 0;
        for (var t = 0; t < numTables; t++)
        {
            var rec = 12 + t * 16;
            var tag = Encoding.ASCII.GetString(d, rec, 4);
            var offset = Be32(rec + 8);
            if (tag == "head") upem = Be16(offset + 18);
            if (tag == "hhea") { asc = Be16(offset + 4); desc = Be16(offset + 6); gap = Be16(offset + 8); }
        }
        return (upem, asc, desc, gap);
    }

    // 上游 FreeTypeFont.CreateGlyph 逐行同构（FreeTypeFont.cs:79-130）：
    // (uint)(size*deviceScale) 截断 → FT_Set_Pixel_Sizes → FT_Load_Char(FT_LOAD_RENDER) →
    // metrics (w,h,adv)>>6 → 8bpp 逐行拷贝。load 失败 → EmptyGlyph（Advance=0、Data=null）。
    static class FontsFtHost
    {
        const uint OK = 0x00;
        const int FT_LOAD_RENDER = 0x04;

        // 上游 FreeTypeFont.cs:27-36 的 x64 原始偏移
        const int MetricsWidthOffset = 0;
        const int BitmapPitchOffset = 8;
        static readonly int FaceRecGlyphOffset = IntPtr.Size == 8 ? 152 : 84;
        static readonly int GlyphSlotMetricsOffset = IntPtr.Size == 8 ? 48 : 24;
        static readonly int GlyphSlotBitmapOffset = IntPtr.Size == 8 ? 152 : 76;
        static readonly int GlyphSlotBitmapLeftOffset = IntPtr.Size == 8 ? 192 : 100;
        static readonly int GlyphSlotBitmapTopOffset = IntPtr.Size == 8 ? 196 : 104;
        static readonly int MetricsHeightOffset = IntPtr.Size == 8 ? 8 : 4;
        static readonly int MetricsAdvanceOffset = IntPtr.Size == 8 ? 32 : 16;
        static readonly int BitmapBufferOffset = IntPtr.Size == 8 ? 16 : 12;

        [DllImport("kernel32", SetLastError = true)]
        internal static extern bool SetDllDirectory(string lpPathName);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_Init_FreeType(out IntPtr library);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_New_Memory_Face(IntPtr library, IntPtr file_base, int file_size, int face_index, out IntPtr aface);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_Done_Face(IntPtr face);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_Done_FreeType(IntPtr library);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_Set_Pixel_Sizes(IntPtr face, uint pixel_width, uint pixel_height);

        [DllImport("freetype6", CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint FT_Load_Char(IntPtr face, uint char_code, int load_flags);

        internal struct GlyphDump
        {
            public int Width, Height, OffsetX, OffsetY;
            public float Advance;
            public byte[] Data;
        }

        internal sealed class FontsHost : IDisposable
        {
            public IntPtr Library;

            public void Dispose()
            {
                if (Library != IntPtr.Zero && FT_Done_FreeType(Library) == OK)
                    Library = IntPtr.Zero;
            }
        }

        internal sealed class FontsFace : IDisposable
        {
            public IntPtr Handle;
            internal GCHandle Pinned;

            public void Dispose()
            {
                if (Handle != IntPtr.Zero) FT_Done_Face(Handle);
                Handle = IntPtr.Zero;
                if (Pinned.IsAllocated) Pinned.Free();
            }

            internal void Pin(byte[] data) => Pinned = GCHandle.Alloc(data, GCHandleType.Pinned);
        }

        internal static FontsHost Load()
        {
            var host = new FontsHost();
            if (FT_Init_FreeType(out host.Library) != OK)
                throw new InvalidOperationException("Failed to initialize FreeType");
            return host;
        }

        internal static FontsFace LoadFace(FontsHost host, byte[] data)
        {
            var face = new FontsFace();
            face.Pin(data);
            if (FT_New_Memory_Face(host.Library, face.Pinned.AddrOfPinnedObject(), data.Length, 0, out var ptr) != OK)
                throw new InvalidDataException("Failed to initialize font");
            face.Handle = ptr;
            return face;
        }

        internal static GlyphDump Empty() => new() { OffsetX = 0, OffsetY = 0, Width = 0, Height = 0, Advance = 0, Data = null };

        internal static GlyphDump CreateGlyph(IntPtr face, char c, int size, float deviceScale)
        {
            var scaledSize = (uint)(size * deviceScale);
            if (FT_Set_Pixel_Sizes(face, scaledSize, scaledSize) != OK)
                return Empty();

            if (FT_Load_Char(face, c, FT_LOAD_RENDER) != OK)
                return Empty();

            var glyph = Marshal.ReadIntPtr(IntPtr.Add(face, FaceRecGlyphOffset));
            var metrics = IntPtr.Add(glyph, GlyphSlotMetricsOffset);
            var metricsWidth = Marshal.ReadIntPtr(IntPtr.Add(metrics, MetricsWidthOffset));
            var metricsHeight = Marshal.ReadIntPtr(IntPtr.Add(metrics, MetricsHeightOffset));
            var metricsAdvance = Marshal.ReadIntPtr(IntPtr.Add(metrics, MetricsAdvanceOffset));
            var bitmap = IntPtr.Add(glyph, GlyphSlotBitmapOffset);
            var bitmapPitch = Marshal.ReadInt32(IntPtr.Add(bitmap, BitmapPitchOffset));
            var bitmapBuffer = Marshal.ReadIntPtr(IntPtr.Add(bitmap, BitmapBufferOffset));
            var bitmapLeft = Marshal.ReadInt32(IntPtr.Add(glyph, GlyphSlotBitmapLeftOffset));
            var bitmapTop = Marshal.ReadInt32(IntPtr.Add(glyph, GlyphSlotBitmapTopOffset));

            var glyphWidth = (int)metricsWidth >> 6;
            var glyphHeight = (int)metricsHeight >> 6;
            var glyphAdvance = (int)metricsAdvance >> 6;

            var data = new byte[glyphWidth * glyphHeight];
            var k = 0;
            for (var j = 0; j < glyphHeight; j++)
            {
                Marshal.Copy(IntPtr.Add(bitmapBuffer, j * bitmapPitch), data, k, glyphWidth);
                k += glyphWidth;
            }

            return new GlyphDump
            {
                Advance = glyphAdvance,
                OffsetX = bitmapLeft,
                OffsetY = -bitmapTop,
                Width = glyphWidth,
                Height = glyphHeight,
                Data = data,
            };
        }
    }

    // —— Measure/WrapText oracle（复刻上游真源，advance 来自 FT dump）——
    // SpriteFont.cs:225-250 Measure：空串 (0,size)；按 '\n' 拆行（含空段）；
    // 行宽 = Σ Advance（float 累加）/ deviceScale；ceil(max) × rows*size。
    static (int W, int H) FontsMeasure(string text, Dictionary<char, float> advances, int size, float deviceScale)
    {
        if (string.IsNullOrEmpty(text))
            return (0, size);

        var maxWidth = 0f;
        var rows = 0;
        foreach (var line in text.Split('\n'))
        {
            rows++;
            var lineWidth = 0f;
            foreach (var c in line)
                lineWidth += advances[c];
            maxWidth = Math.Max(maxWidth, lineWidth / deviceScale);
        }

        return ((int)Math.Ceiling(maxWidth), rows * size);
    }

    // WidgetUtils.cs:243-283 WrapText 逐行复刻（插入行由同一 for 循环继续处理；
    // 首个空格前缀即超宽 → 整行放弃断行）。
    static string FontsWrapText(string text, int width, Dictionary<char, float> advances, int size, float deviceScale)
    {
        int MeasureX(string s) => FontsMeasure(s, advances, size, deviceScale).W;

        var textSize = MeasureX(text);
        if (textSize > width)
        {
            var lines = text.Split('\n').ToList();

            for (var i = 0; i < lines.Count; i++)
            {
                var line = lines[i];
                if (MeasureX(line) <= width)
                    continue;

                var start = 0;
                while (true)
                {
                    var spaceIndex = line.IndexOf(' ', start);
                    if (spaceIndex == -1)
                        break;

                    var fragmentWidth = MeasureX(line[..spaceIndex]);
                    if (fragmentWidth > width)
                        break;

                    start = spaceIndex + 1;
                }

                if (start > 0)
                {
                    lines[i] = line[..(start - 1)];
                    lines.Insert(i + 1, line[start..]);
                }
            }

            return string.Join("\n", lines);
        }

        return text;
    }

    static Dictionary<char, float> FontsLoadAdvances(string openraRoot, string fontName, int size)
    {
        var advances = new Dictionary<char, float>();
        var bytes = DumpFontsFtBitmap(openraRoot, fontName, size, null);
        var magic = Encoding.ASCII.GetString(bytes, 0, 6);
        if (magic != "FTD1u8")
            throw new InvalidOperationException($"FTD1u8 magic mismatch: {magic}");
        var cpCount = BitConverter.ToInt32(bytes, 14);
        var offset = 18;
        for (var i = 0; i < cpCount; i++)
        {
            var cp = BitConverter.ToInt32(bytes, offset);
            advances[(char)cp] = BitConverter.ToSingle(bytes, offset + 12);
            offset += 24 + BitConverter.ToInt32(bytes, offset + 4) * BitConverter.ToInt32(bytes, offset + 8);
        }
        return advances;
    }

    static byte[] DumpFontsMeasure(string openraRoot, string fontName, int size)
    {
        var advances = FontsLoadAdvances(openraRoot, fontName, size);
        var sb = new StringBuilder();
        sb.Append("# Measure oracle: font=").Append(fontName).Append(" size=").Append(size)
            .Append(" deviceScale=1").Append('\n');
        sb.Append("# format: <index><TAB><width><TAB><height>").Append('\n');
        foreach (var (index, text) in FontsStrings)
        {
            var (w, h) = FontsMeasure(text, advances, size, 1.0f);
            sb.Append(index).Append('\t')
                .Append(w.ToString(CultureInfo.InvariantCulture)).Append('\t')
                .Append(h.ToString(CultureInfo.InvariantCulture)).Append('\n');
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static byte[] DumpFontsWrap(string openraRoot, string fontName, int size, int width)
    {
        var advances = FontsLoadAdvances(openraRoot, fontName, size);
        var sb = new StringBuilder();
        sb.Append("# WrapText oracle: font=").Append(fontName).Append(" size=").Append(size)
            .Append(" width=").Append(width).Append(" deviceScale=1").Append('\n');
        sb.Append("# format: <index><TAB><escaped result (\\n-joined lines)>").Append('\n');
        foreach (var (index, text) in FontsStrings)
        {
            var wrapped = FontsWrapText(text, width, advances, size, 1.0f);
            sb.Append(index).Append('\t').Append(FontsEscape(wrapped)).Append('\n');
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }
}
