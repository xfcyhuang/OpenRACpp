using System.Reflection;
using System.Runtime.InteropServices;
using OpenRA.FileFormats;
using OpenRA.Graphics;
using OpenRA.Widgets;

internal static partial class Program
{
// —— widgets 模式（T25/issue #34）：六控件最小面 oracle ——
// 三份 dump（模式组单次装配共享）：
//   states.txt   真源状态机对拍：ChromeMetrics/ChromeProvider 真装载 + 上游真控件
//                （ButtonWidget/CheckboxWidget/SliderWidget/DropDownButtonWidget/
//                BackgroundWidget/LabelWidget，Mods.Common），合成 MouseInput 喂
//                Ui.HandleInput 全路由，每步 dump handled/Depressed/HasMouseFocus/
//                MouseOver/Value(IsChecked) 与回调记录（OnClick/OnMouseDown/
//                OnDoubleClick/OnChange 闭包收集；委托面即 C++ D078 记录向量折叠的
//                对拍锚）。音效走 Stub + DisableAllSounds。
//   draw.txt     绘制调用序列：Draw() 依赖 Game.Renderer.Fonts/GL 不可无头驱动，按上游
//                Draw/DrawBackground/DrawPanel/FillRectWithSprite 逐行复刻纯计算面——
//                状态化图像名经上游真函数 WidgetUtils.GetStatefulImageName，九宫格/平铺
//                公式照抄（含上游 Right/Bottom 边块复用 margin 对侧变量的原样形态）；
//                文本行不带坐标（字体度量 oracle 不可得，字形几何由 C++ ctest 按
//                fonts 黄金独立真值断言，D077 口径延续）。
//   pixels.txt   D075 兑现：chrome sheet 像素面真源——上游 Png 解码（unfilter 5 滤波/
//                PLTE/tRNS）+ Util.FastCopyIntoSprite（Indexed8 查表/Rgb24 补 255/Rgba32
//                直通）+ PremultiplyAlpha，Sheet(BGRA) 数据逐字节 SHA256 + 确定性采样网格。
// fixture 三 PNG（dialog/glyphs/sidebar，U097 盘点形态）原样入库兼漂移哨兵。
//
// 装载口径：Mods.Common 不进宿主编译引用——上游 ObjectCreator 经 Support.AssemblyLoader
// 在隔离 ALC 加载 mod 程序集，宿主静态引用会产出第二份 Common 实例使 trait 类型身份
// 分裂（v2rl 的 Mobile 校验假失败实证）。宿主反射定位 ObjectCreator 已加载实例后以
// dynamic 交互；OpenRA.Game 与 runtime 类型跨 ALC 单一，直接强类型使用。

    const string WidgetsGoldenRelDir = "tests/golden/widgets";
    const int WidgetsWindowWidth = 1024;
    const int WidgetsWindowHeight = 768;

    // sheet id 分配序（两侧同构：C++ ChromeSheetSet 按同列表装载）
    static readonly string[] WidgetChromeSheets = ["dialog.png", "glyphs.png", "sidebar.png"];

    static readonly List<string> WidgetEvents = [];

    // Mods.Common 反射句柄（WidgetsGroup 装配时定位）
    static System.Reflection.Assembly WidgetsAsm;
    static Type WidgetsButton;
    static Type WidgetsCheckbox;
    static Type WidgetsSlider;
    static Type WidgetsDropDown;
    static Type WidgetsBackground;
    static Type WidgetsLabel;
    static MethodInfo WidgetsGetStatefulImageName;

    static void WidgetsBindCommon()
    {
        WidgetsAsm = AppDomain.CurrentDomain.GetAssemblies()
            .First(a => a.GetName().Name == "OpenRA.Mods.Common");
        WidgetsButton = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.ButtonWidget")!;
        WidgetsCheckbox = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.CheckboxWidget")!;
        WidgetsSlider = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.SliderWidget")!;
        WidgetsDropDown = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.DropDownButtonWidget")!;
        WidgetsBackground = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.BackgroundWidget")!;
        WidgetsLabel = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.LabelWidget")!;
        var utils = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.WidgetUtils")!;
        WidgetsGetStatefulImageName = utils.GetMethod("GetStatefulImageName",
            [typeof(string), typeof(bool), typeof(bool), typeof(bool), typeof(bool)])!;
    }

    static dynamic WidgetsNew(Type type, params object[] args)
    {
        return Activator.CreateInstance(type, args)!;
    }

    // 上游 WidgetUtils.GetStatefulImageName(baseName, disabled, pressed, hover)（真函数）
    static string WidgetsStatefulImage(string baseName, bool disabled, bool pressed, bool hover)
    {
        return (string)WidgetsGetStatefulImageName.Invoke(null, [baseName, disabled, pressed, hover, false])!;
    }

    // 上游 GetCachedStatefulImage 的 variant 回落语义（WidgetUtils.cs:45-55）
    static Rectangle? WidgetsCachedRegion(string collection, string baseName, bool disabled, bool pressed, bool hover)
    {
        var variant = WidgetsStatefulImage(baseName, disabled, pressed, hover);
        return WidgetRegion(collection, variant) ?? WidgetRegion(collection, baseName);
    }

    static (string RelPath, string Mode, Func<byte[]> Produce)[] WidgetsArtifacts(string openraRoot)
    {
        return
        [
            .. WidgetChromeSheets.Select(name => (
                $"{WidgetsGoldenRelDir}/fixture/{name}",
                (string)null,
                (Func<byte[]>)(() => File.ReadAllBytes(
                    Path.Combine(openraRoot, "mods", "ra", "uibits", name))))),
            ($"{WidgetsGoldenRelDir}/states.txt", "widgets", null),
            ($"{WidgetsGoldenRelDir}/draw.txt", "widgets", null),
            ($"{WidgetsGoldenRelDir}/pixels.txt", "widgets", null),
        ];
    }

    static byte[] WidgetsTextEnd(StringBuilder sb)
    {
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    // —— states：真源状态机 ——

    static void WidgetResetUi()
    {
        Ui.ResetAll();
        Ui.MouseFocusWidget = null;
        Ui.KeyboardFocusWidget = null;
        Ui.MouseOverWidget = null;
        Viewport.LastMousePos = int2.Zero;
        WidgetEvents.Clear();
    }

    // 事件步：全路由经 Ui.HandleInput（真源），dump 步后状态 + 本步回调记录
    static void WidgetStep(StringBuilder sb, dynamic w, string kind, MouseInputEvent ev, MouseButton btn, int x, int y, int tap)
    {
        var mi = new MouseInput(ev, btn, new int2(x, y), int2.Zero, Modifiers.None, tap);
        var handled = Ui.HandleInput(mi);

        var line = new StringBuilder();
        line.Append("ev ").Append(ev).Append(' ').Append(btn).Append(' ')
            .Append(x).Append(',').Append(y).Append(" tap=").Append(tap)
            .Append(" handled=").Append(handled ? 1 : 0);
        if (kind is "button" or "checkbox")
        {
            line.Append(" dep=").Append((bool)w.Depressed ? 1 : 0);
            if (kind == "checkbox")
            {
                Func<bool> isChecked = w.IsChecked;
                line.Append(" checked=").Append(isChecked() ? 1 : 0);
            }
        }
        else if (kind == "slider")
        {
            line.Append(" value=").Append(((float)w.Value).ToString("F6", CultureInfo.InvariantCulture));
        }

        line.Append(" mousefocus=").Append(Ui.MouseFocusWidget == w ? 1 : 0)
            .Append(" mouseover=").Append(Ui.MouseOverWidget == w ? 1 : 0)
            .Append(" events=").Append(string.Join(";", WidgetEvents));
        WidgetEvents.Clear();
        sb.Append(line).Append('\n');
    }

    static void DumpWidgetStates(StringBuilder sb, ModData modData)
    {
        sb.Append("# widgets state machine oracle (T25/issue #34)").Append('\n');
        sb.Append("# upstream widgets driven via Ui.HandleInput with synthetic MouseInput").Append('\n');
        sb.Append("# per-step: ev <event> <button> <x>,<y> tap=<n> handled=<0|1>"
            + " [dep] [checked] [value] mousefocus=<0|1> mouseover=<0|1> events=<a;b>").Append('\n');

        // Button 基本序列：进出/按下/拖出/拖回/释放点击
        {
            WidgetResetUi();
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            b.Text = "Hello";
            b.OnClick = (Action)(() => WidgetEvents.Add("click"));
            b.OnMouseDown = (Action<MouseInput>)(mi => WidgetEvents.Add($"down({mi.Location.X},{mi.Location.Y})"));
            Ui.Root.AddChild(b);
            sb.Append("scenario button-basic Button bounds=100,100,80,24").Append('\n');
            WidgetStep(sb, b, "button", MouseInputEvent.Move, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Move, MouseButton.Left, 20, 20, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Move, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Move, MouseButton.Left, 20, 20, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 20, 20, 1);
            sb.Append("end").Append('\n');
        }

        // 双击：tap=2 的 Up 走 OnDoubleClick 支路（不触发 OnMouseUp/OnClick）
        {
            WidgetResetUi();
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            b.OnClick = (Action)(() => WidgetEvents.Add("click"));
            b.OnDoubleClick = (Action)(() => WidgetEvents.Add("dblclick"));
            Ui.Root.AddChild(b);
            sb.Append("scenario button-double Button double=on").Append('\n');
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 120, 112, 2);
            sb.Append("end").Append('\n');
        }

        // Disabled：Down 不置 Depressed、立即 YieldMouseFocus、返回 false（事件冒泡）
        {
            WidgetResetUi();
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            b.Disabled = true;
            b.OnClick = (Action)(() => WidgetEvents.Add("click"));
            Ui.Root.AddChild(b);
            sb.Append("scenario button-disabled Button disabled=1").Append('\n');
            WidgetStep(sb, b, "button", MouseInputEvent.Move, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 2);
            sb.Append("end").Append('\n');
        }

        // 非左键：HandleMouseInput 首行短路 false
        {
            WidgetResetUi();
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            Ui.Root.AddChild(b);
            sb.Append("scenario button-rightclick Button").Append('\n');
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Right, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Right, 120, 112, 1);
            sb.Append("end").Append('\n');
        }

        // 拖出释放：Depressed=false 时 Up 不触发 OnMouseUp（上游 ButtonWidget.cs:181-188）
        {
            WidgetResetUi();
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            b.OnClick = (Action)(() => WidgetEvents.Add("click"));
            Ui.Root.AddChild(b);
            sb.Append("scenario button-outside-release Button").Append('\n');
            WidgetStep(sb, b, "button", MouseInputEvent.Down, MouseButton.Left, 120, 112, 1);
            WidgetStep(sb, b, "button", MouseInputEvent.Up, MouseButton.Left, 20, 20, 1);
            sb.Append("end").Append('\n');
        }

        // Checkbox：状态机同 Button（继承），IsChecked 观测列（toggle 属 logic 层不在状态机）
        {
            WidgetResetUi();
            dynamic c = WidgetsNew(WidgetsCheckbox, modData);
            c.Id = "CHK";
            c.Bounds = new WidgetBounds(40, 40, 200, 20);
            c.Text = "Option";
            c.IsChecked = (Func<bool>)(() => true);
            c.OnClick = (Action)(() => WidgetEvents.Add("click"));
            Ui.Root.AddChild(c);
            sb.Append("scenario checkbox-checked Checkbox bounds=40,40,200,20 checked=1").Append('\n');
            WidgetStep(sb, c, "checkbox", MouseInputEvent.Move, MouseButton.Left, 50, 50, 1);
            WidgetStep(sb, c, "checkbox", MouseInputEvent.Down, MouseButton.Left, 50, 50, 1);
            WidgetStep(sb, c, "checkbox", MouseInputEvent.Up, MouseButton.Left, 50, 50, 1);
            sb.Append("end").Append('\n');
        }

        // DropDownButton：状态机继承 Button
        {
            WidgetResetUi();
            dynamic d = WidgetsNew(WidgetsDropDown, modData);
            d.Id = "DROP";
            d.Bounds = new WidgetBounds(10, 10, 120, 25);
            d.OnClick = (Action)(() => WidgetEvents.Add("click"));
            Ui.Root.AddChild(d);
            sb.Append("scenario dropdown DropDownButton bounds=10,10,120,25").Append('\n');
            WidgetStep(sb, d, "button", MouseInputEvent.Move, MouseButton.Left, 60, 22, 1);
            WidgetStep(sb, d, "button", MouseInputEvent.Down, MouseButton.Left, 60, 22, 1);
            WidgetStep(sb, d, "button", MouseInputEvent.Move, MouseButton.Left, 60, 22, 1);
            WidgetStep(sb, d, "button", MouseInputEvent.Up, MouseButton.Left, 60, 22, 1);
            sb.Append("end").Append('\n');
        }

        // Slider：Down 吸附值 → 拖动连续更新（clamp）→ Up 释放 → 拖后无变化
        {
            WidgetResetUi();
            dynamic s = WidgetsNew(WidgetsSlider);
            s.Id = "SLD";
            s.Bounds = new WidgetBounds(10, 50, 200, 16);
            s.MinimumValue = 0f;
            s.MaximumValue = 1f;
            s.Value = 0.25f;
            s.Ticks = 5;
            s.OnChange += (Action<float>)(v => WidgetEvents.Add("change " + v.ToString("F6", CultureInfo.InvariantCulture)));
            Ui.Root.AddChild(s);
            sb.Append("scenario slider Slider bounds=10,50,200,16 min=0 max=1 value=0.25").Append('\n');
            WidgetStep(sb, s, "slider", MouseInputEvent.Down, MouseButton.Left, 60, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Move, MouseButton.Left, 150, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Move, MouseButton.Left, 5, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Move, MouseButton.Left, 300, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Up, MouseButton.Left, 300, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Move, MouseButton.Left, 100, 58, 1);
            sb.Append("end").Append('\n');
        }

        // Slider 禁用：首行 IsDisabled 短路（不取焦点）
        {
            WidgetResetUi();
            dynamic s = WidgetsNew(WidgetsSlider);
            s.Id = "SLD";
            s.Bounds = new WidgetBounds(10, 50, 200, 16);
            s.MinimumValue = 0f;
            s.MaximumValue = 1f;
            s.Value = 0.25f;
            s.Disabled = true;
            s.OnChange += (Action<float>)(v => WidgetEvents.Add("change " + v.ToString("F6", CultureInfo.InvariantCulture)));
            Ui.Root.AddChild(s);
            sb.Append("scenario slider-disabled Slider disabled=1").Append('\n');
            WidgetStep(sb, s, "slider", MouseInputEvent.Down, MouseButton.Left, 60, 58, 1);
            WidgetStep(sb, s, "slider", MouseInputEvent.Move, MouseButton.Left, 150, 58, 1);
            sb.Append("end").Append('\n');
        }

        // Background：!ClickThrough && contains（不分键）；Move 只做 mouseover 追踪
        {
            WidgetResetUi();
            dynamic bg = WidgetsNew(WidgetsBackground);
            bg.Id = "BG";
            bg.Bounds = new WidgetBounds(0, 0, 200, 100);
            Ui.Root.AddChild(bg);
            sb.Append("scenario background Background bounds=0,0,200,100").Append('\n');
            WidgetStep(sb, bg, "", MouseInputEvent.Move, MouseButton.Left, 50, 50, 1);
            WidgetStep(sb, bg, "", MouseInputEvent.Down, MouseButton.Left, 50, 50, 1);
            WidgetStep(sb, bg, "", MouseInputEvent.Up, MouseButton.Left, 50, 50, 1);
            WidgetStep(sb, bg, "", MouseInputEvent.Down, MouseButton.Right, 50, 50, 1);
            WidgetStep(sb, bg, "", MouseInputEvent.Move, MouseButton.Left, 300, 300, 1);
            sb.Append("end").Append('\n');
        }

        // Label：Widget 基类（非 Input），Down 恒 false；mouseover 可观测
        {
            WidgetResetUi();
            dynamic l = WidgetsNew(WidgetsLabel, modData);
            l.Id = "LBL";
            l.Bounds = new WidgetBounds(0, 0, 100, 20);
            l.Text = "Text";
            Ui.Root.AddChild(l);
            sb.Append("scenario label Label bounds=0,0,100,20").Append('\n');
            WidgetStep(sb, l, "", MouseInputEvent.Move, MouseButton.Left, 50, 10, 1);
            WidgetStep(sb, l, "", MouseInputEvent.Down, MouseButton.Left, 50, 10, 1);
            WidgetStep(sb, l, "", MouseInputEvent.Up, MouseButton.Left, 50, 10, 1);
            sb.Append("end").Append('\n');
        }
    }

    // —— draw：绘制调用序列（数据真源 + 公式照抄上游 Draw/DrawPanel/FillRectWithSprite）———

    static int WidgetSheetId(string image)
    {
        return Array.IndexOf(WidgetChromeSheets, image) + 1;
    }

    static int WidgetSheetOf(string collection)
    {
        return ChromeProvider.Collections[collection].Image is { } img ? WidgetSheetId(img) : 0;
    }

    // ChromeProvider.TryGetPanelImages 的矩形面（sprite 不可无头构造，矩形/尺寸同构：
    // density=1 时 Sprite 采样矩形 = 下列 Rectangle，Size = 矩形宽高）
    static Rectangle?[] WidgetPanelRects(string collection)
    {
        if (!ChromeProvider.Collections.TryGetValue(collection, out var c))
            return null;

        if (!c.PanelRegion.IsDefault && c.PanelRegion.Length == 8)
        {
            var pr = c.PanelRegion;
            var ps = c.PanelSides;
            Rectangle? Side(PanelSides side, Rectangle r) => ps.HasSide(side) ? r : null;
            return
            [
                Side(PanelSides.Top | PanelSides.Left, new Rectangle(pr[0], pr[1], pr[2], pr[3])),
                Side(PanelSides.Top, new Rectangle(pr[0] + pr[2], pr[1], pr[4], pr[3])),
                Side(PanelSides.Top | PanelSides.Right, new Rectangle(pr[0] + pr[2] + pr[4], pr[1], pr[6], pr[3])),
                Side(PanelSides.Left, new Rectangle(pr[0], pr[1] + pr[3], pr[2], pr[5])),
                Side(PanelSides.Center, new Rectangle(pr[0] + pr[2], pr[1] + pr[3], pr[4], pr[5])),
                Side(PanelSides.Right, new Rectangle(pr[0] + pr[2] + pr[4], pr[1] + pr[3], pr[6], pr[5])),
                Side(PanelSides.Bottom | PanelSides.Left, new Rectangle(pr[0], pr[1] + pr[3] + pr[5], pr[2], pr[7])),
                Side(PanelSides.Bottom, new Rectangle(pr[0] + pr[2], pr[1] + pr[3] + pr[5], pr[4], pr[7])),
                Side(PanelSides.Bottom | PanelSides.Right, new Rectangle(pr[0] + pr[2] + pr[4], pr[1] + pr[3] + pr[5], pr[6], pr[7])),
            ];
        }

        if (c.Regions.Count == 0)
            return [];

        return
        [
            c.Regions.TryGetValue("corner-tl", out var tl) ? tl : null,
            c.Regions.TryGetValue("border-t", out var t) ? t : null,
            c.Regions.TryGetValue("corner-tr", out var tr) ? tr : null,
            c.Regions.TryGetValue("border-l", out var l) ? l : null,
            c.Regions.TryGetValue("background", out var bg) ? bg : null,
            c.Regions.TryGetValue("border-r", out var r) ? r : null,
            c.Regions.TryGetValue("corner-bl", out var bl) ? bl : null,
            c.Regions.TryGetValue("border-b", out var bo) ? bo : null,
            c.Regions.TryGetValue("corner-br", out var br) ? br : null,
        ];
    }

    static Rectangle? WidgetRegion(string collection, string image)
    {
        if (!ChromeProvider.Collections.TryGetValue(collection, out var c))
            return null;
        return c.Regions.TryGetValue(image, out var r) ? r : null;
    }

    // WidgetUtils.FillRectWithSprite（scale=1）：x 外层 y 内层，尾块裁剪
    static void WidgetDumpFillRect(StringBuilder sb, int sheet, Rectangle src, Rectangle r)
    {
        for (var x = r.Left; x < r.Right; x += src.Width)
        {
            for (var y = r.Top; y < r.Bottom; y += src.Height)
            {
                var dx = r.Right - x;
                var dy = r.Bottom - y;
                int w = src.Width, h = src.Height;
                if (dx < src.Width || dy < src.Height)
                {
                    w = Math.Min(src.Width, dx);
                    h = Math.Min(src.Height, dy);
                }

                sb.Append("q ").Append(sheet).Append(' ')
                    .Append(src.Left).Append(',').Append(src.Top).Append(',').Append(w).Append(',').Append(h)
                    .Append(' ').Append(x).Append(',').Append(y).Append(',').Append(w).Append(',').Append(h)
                    .Append('\n');
            }
        }
    }

    // WidgetUtils.DrawPanel(bounds, sprites)：Center/Left/Right/Top/Bottom 平铺 + 四角
    // （Right 边宽用 marginLeft、Bottom 边高用 marginTop 的上游原样形态照抄）
    static void WidgetDumpPanel(StringBuilder sb, string collection, Rectangle bounds)
    {
        var sprites = WidgetPanelRects(collection);
        if (sprites == null || sprites.Length != 9)
            return;

        var sheet = WidgetSheetOf(collection);
        var marginTop = sprites[1]?.Height ?? 0;
        var marginLeft = sprites[3]?.Width ?? 0;
        var marginRight = sprites[5]?.Width ?? 0;
        var marginBottom = sprites[7]?.Height ?? 0;
        var marginWidth = marginRight + marginLeft;
        var marginHeight = marginBottom + marginTop;

        if (sprites[4] is { } c)
            WidgetDumpFillRect(sb, sheet, c, new Rectangle(
                bounds.Left + marginLeft, bounds.Top + marginTop,
                bounds.Width - marginWidth, bounds.Height - marginHeight));

        if (sprites[3] is { } l)
            WidgetDumpFillRect(sb, sheet, l, new Rectangle(
                bounds.Left, bounds.Top + marginTop, marginLeft, bounds.Height - marginHeight));

        if (sprites[5] is { } r)
            WidgetDumpFillRect(sb, sheet, r, new Rectangle(
                bounds.Right - marginRight, bounds.Top + marginTop,
                marginLeft, bounds.Height - marginHeight));

        if (sprites[1] is { } t)
            WidgetDumpFillRect(sb, sheet, t, new Rectangle(
                bounds.Left + marginLeft, bounds.Top, bounds.Width - marginWidth, marginTop));

        if (sprites[7] is { } b)
            WidgetDumpFillRect(sb, sheet, b, new Rectangle(
                bounds.Left + marginLeft, bounds.Bottom - marginBottom,
                bounds.Width - marginWidth, marginTop));

        if (sprites[0] is { } tl)
            sb.Append("img ").Append(sheet).Append(' ')
                .Append(tl.Left).Append(',').Append(tl.Top).Append(',').Append(tl.Width).Append(',').Append(tl.Height)
                .Append(' ').Append(bounds.Left).Append(',').Append(bounds.Top).Append(',').Append(tl.Width).Append(',')
                .Append(tl.Height).Append('\n');

        if (sprites[2] is { } tr)
            sb.Append("img ").Append(sheet).Append(' ')
                .Append(tr.Left).Append(',').Append(tr.Top).Append(',').Append(tr.Width).Append(',').Append(tr.Height)
                .Append(' ').Append(bounds.Right - tr.Width).Append(',').Append(bounds.Top).Append(',')
                .Append(tr.Width).Append(',').Append(tr.Height).Append('\n');

        if (sprites[6] is { } bl)
            sb.Append("img ").Append(sheet).Append(' ')
                .Append(bl.Left).Append(',').Append(bl.Top).Append(',').Append(bl.Width).Append(',').Append(bl.Height)
                .Append(' ').Append(bounds.Left).Append(',').Append(bounds.Bottom - bl.Height).Append(',')
                .Append(bl.Width).Append(',').Append(bl.Height).Append('\n');

        if (sprites[8] is { } br)
            sb.Append("img ").Append(sheet).Append(' ')
                .Append(br.Left).Append(',').Append(br.Top).Append(',').Append(br.Width).Append(',').Append(br.Height)
                .Append(' ').Append(bounds.Right - br.Width).Append(',').Append(bounds.Bottom - br.Height).Append(',')
                .Append(br.Width).Append(',').Append(br.Height).Append('\n');
    }

    // WidgetUtils.DrawSprite 的单 sprite 行（位置可含 float 公式输入）
    static void WidgetDumpSprite(StringBuilder sb, int sheet, Rectangle src, float x, float y)
    {
        sb.Append("img ").Append(sheet).Append(' ')
            .Append(src.Left).Append(',').Append(src.Top).Append(',').Append(src.Width).Append(',').Append(src.Height)
            .Append(' ').Append(x.ToString(CultureInfo.InvariantCulture)).Append(',')
            .Append(y.ToString(CultureInfo.InvariantCulture)).Append(',')
            .Append(src.Width).Append(',').Append(src.Height).Append('\n');
    }

    // ButtonWidget.DrawBackground → GetStatefulImageName（上游真函数）→ DrawPanel
    static void WidgetDumpButtonBackground(StringBuilder sb, dynamic b, Rectangle rb, bool hover)
    {
        Func<bool> isDisabled = b.IsDisabled;
        Func<bool> isHighlighted = b.IsHighlighted;
        var disabled = isDisabled();
        var variantName = isHighlighted() ? (string)b.Background + "-highlighted" : (string)b.Background;
        var imageName = WidgetsStatefulImage(variantName, disabled, (bool)b.Depressed, hover);
        WidgetDumpPanel(sb, imageName, rb);
    }

    static void WidgetDumpTextLine(StringBuilder sb, string font, string align, string valign, string color, string text)
    {
        sb.Append("t ").Append(font).Append(' ').Append(align).Append(' ').Append(valign).Append(' ')
            .Append(color).Append(' ').Append(text).Append('\n');
    }

    static void DumpWidgetDraws(StringBuilder sb, ModData modData)
    {
        sb.Append("# widgets draw call oracle (T25/issue #34)").Append('\n');
        sb.Append("# per-state draw call sequence; q = panel fill tile, img = single sprite,"
            + " t = text (metrics-free: coordinates asserted in ctest per D077)").Append('\n');
        sb.Append("# sheets: 1=dialog.png 2=glyphs.png 3=sidebar.png; stateful image names via upstream GetStatefulImageName").Append('\n');

        // Background：单快照（无交互状态面）
        {
            dynamic bg = WidgetsNew(WidgetsBackground);
            bg.Id = "BG";
            bg.Bounds = new WidgetBounds(20, 20, 340, 280);
            Ui.Root.AddChild(bg);
            sb.Append("state Background normal").Append('\n');
            WidgetDumpPanel(sb, (string)bg.Background, (Rectangle)bg.RenderBounds);
            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(bg);
        }

        // Button：四状态快照
        Action<dynamic>[] buttonApply =
        [
            _ => { },
            b => Ui.MouseOverWidget = (Widget)b,
            b => b.Depressed = true,
            b => b.Disabled = true,
        ];
        string[] buttonNames = ["normal", "hover", "pressed", "disabled"];
        for (var i = 0; i < buttonNames.Length; i++)
        {
            dynamic b = WidgetsNew(WidgetsButton, modData);
            b.Id = "BTN";
            b.Bounds = new WidgetBounds(100, 100, 80, 24);
            b.Text = "Hello";
            Ui.Root.AddChild(b);
            Ui.MouseOverWidget = null;
            buttonApply[i](b);
            sb.Append("state Button ").Append(buttonNames[i]).Append('\n');
            var hover = Ui.MouseOverWidget == (Widget)b;
            WidgetDumpButtonBackground(sb, b, (Rectangle)b.RenderBounds, hover);
            Func<string> getText = b.GetText;
            Func<Color> getColor = b.IsDisabled() ? b.GetColorDisabled : b.GetColor;
            WidgetDumpTextLine(sb, (string)b.Font, (string)b.Align.ToString(), "-",
                getColor().ToArgb().ToString("X8", CultureInfo.InvariantCulture), getText());
            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(b);
            Ui.MouseOverWidget = null;
        }

        // Checkbox：checked/unchecked/disabled（背景块 = Height×Height 九宫格 + checkmark）
        (string Name, Action<dynamic> Apply)[] checkboxSnapshots =
        [
            ("checked", c => c.IsChecked = (Func<bool>)(() => true)),
            ("unchecked", c => c.IsChecked = (Func<bool>)(() => false)),
            ("checked-disabled", c => { c.IsChecked = (Func<bool>)(() => true); c.Disabled = true; }),
        ];
        foreach (var (name, apply) in checkboxSnapshots)
        {
            dynamic c = WidgetsNew(WidgetsCheckbox, modData);
            c.Id = "CHK";
            c.Bounds = new WidgetBounds(40, 40, 200, 20);
            c.Text = "Option";
            Ui.Root.AddChild(c);
            Ui.MouseOverWidget = null;
            apply(c);
            sb.Append("state Checkbox ").Append(name).Append('\n');
            Func<bool> isDisabled = c.IsDisabled;
            var disabled = isDisabled();
            var hover = Ui.MouseOverWidget == (Widget)c;
            var rb = (Rectangle)c.RenderBounds;
            var rect = new Rectangle(rb.Left, rb.Top, c.Bounds.Height, c.Bounds.Height);

            WidgetDumpButtonBackground(sb, c, rect, hover);

            Func<string> getText = c.GetText;
            Func<Color> getColor = disabled ? c.GetColorDisabled : c.GetColor;
            WidgetDumpTextLine(sb, (string)c.Font, (string)c.Align.ToString(), "-",
                getColor().ToArgb().ToString("X8", CultureInfo.InvariantCulture), getText());

            // CheckboxWidget.Draw 尾部 checkmark（GetCachedStatefulImage 语义：variant 回落 base）
            Func<string> getCheckmark = c.GetCheckmark;
            Func<bool> isChecked = c.IsChecked;
            var checkCollection = "checkmark-" + getCheckmark();
            var checkBase = isChecked() ? "checked" : "unchecked";
            if (WidgetsCachedRegion(checkCollection, checkBase, disabled, (bool)c.Depressed, hover) is { } ci)
                WidgetDumpSprite(sb, WidgetSheetOf(checkCollection), ci,
                    rect.Right - (int)((rect.Height + ci.Width) / 2),
                    rect.Top + (int)((rect.Height - ci.Height) / 2));

            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(c);
            Ui.MouseOverWidget = null;
        }

        // Slider：值/悬停/禁用快照（isMoving 非拖拽期恒 false）
        (string Name, Action<dynamic> Apply)[] sliderSnapshots =
        [
            ("value025", s => s.Value = 0.25f),
            ("value075", s => s.Value = 0.75f),
            ("thumbhover", s => { s.Value = 0.5f; Ui.MouseOverWidget = (Widget)s; }),
            ("disabled", s => { s.Value = 0.5f; s.Disabled = true; }),
        ];
        foreach (var (name, apply) in sliderSnapshots)
        {
            dynamic s = WidgetsNew(WidgetsSlider);
            s.Id = "SLD";
            s.Bounds = new WidgetBounds(10, 50, 200, 16);
            s.MinimumValue = 0f;
            s.MaximumValue = 1f;
            s.Value = 0.25f;
            s.Ticks = 5;
            Ui.Root.AddChild(s);
            Ui.MouseOverWidget = null;
            Viewport.LastMousePos = int2.Zero;
            apply(s);
            sb.Append("state Slider ").Append(name).Append('\n');

            // SliderWidget.Draw: ThumbRect / trackRect / tick 公式照抄
            var rb = (Rectangle)s.RenderBounds;
            var value = (float)s.Value;
            var minValue = (float)s.MinimumValue;
            var maxValue = (float)s.MaximumValue;
            var ticks = (int)s.Ticks;
            var trackHeight = (int)s.TrackHeight;
            var thumbPos = (int)(0.5f * rb.Height + (rb.Width - rb.Height) * (value - minValue) / (maxValue - minValue));
            var tr = new Rectangle((int)(rb.X + thumbPos - rb.Height / 2f), rb.Y, rb.Height, rb.Height);
            var trackOrigin = rb.X + rb.Height / 2;
            var trackRect = new Rectangle(trackOrigin - 1, rb.Y + (rb.Height - trackHeight) / 2,
                rb.Width - rb.Height + 2, trackHeight);

            // Tickmarks 先画（SliderWidget.Draw 顺序）
            if (WidgetRegion("slider", "tick") is { } tk)
            {
                var sheet = WidgetSheetOf("slider");
                for (var i = 0; i < ticks; i++)
                {
                    var x = trackOrigin + i * (trackRect.Width - tk.Width) / (ticks - 1) - tk.Width / 2f;
                    WidgetDumpSprite(sb, sheet, tk, x, trackRect.Bottom);
                }
            }

            WidgetDumpPanel(sb, (string)s.Track, trackRect);

            Func<bool> isDisabled = s.IsDisabled;
            var thumbHover = Ui.MouseOverWidget == (Widget)s && tr.Contains(Viewport.LastMousePos);
            var imageName = WidgetsStatefulImage((string)s.Thumb, isDisabled(), false, thumbHover);
            WidgetDumpPanel(sb, imageName, tr);

            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(s);
            Ui.MouseOverWidget = null;
        }

        // DropDownButton：四状态（base Button 面 + marker/separator 装饰）
        for (var i = 0; i < buttonNames.Length; i++)
        {
            dynamic d = WidgetsNew(WidgetsDropDown, modData);
            d.Id = "DROP";
            d.Bounds = new WidgetBounds(10, 10, 120, 25);
            d.Text = "Menu";
            Ui.Root.AddChild(d);
            Ui.MouseOverWidget = null;
            buttonApply[i](d);
            sb.Append("state DropDownButton ").Append(buttonNames[i]).Append('\n');
            var rb = (Rectangle)d.RenderBounds;
            Func<bool> isDisabled = d.IsDisabled;
            var disabled = isDisabled();
            var hover = Ui.MouseOverWidget == (Widget)d || d.Children.Contains(Ui.MouseOverWidget);
            var stateOffset = (bool)d.Depressed ? (int)d.VisualHeight : 0;

            WidgetDumpButtonBackground(sb, d, rb, hover);
            Func<string> getText = d.GetText;
            Func<Color> getColor = disabled ? d.GetColorDisabled : d.GetColor;
            WidgetDumpTextLine(sb, (string)d.Font, (string)d.Align.ToString(), "-",
                getColor().ToArgb().ToString("X8", CultureInfo.InvariantCulture), getText());

            // marker / separator（GetCachedStatefulImage：variant 回落 base）
            var decorations = (string)d.Decorations;
            var decorationMarker = (string)d.DecorationMarker;
            if (WidgetsCachedRegion(decorations, decorationMarker, disabled, (bool)d.Depressed, hover) is { } mk)
                WidgetDumpSprite(sb, WidgetSheetOf(decorations), mk,
                    stateOffset + rb.Right - (int)((rb.Height + mk.Width) / 2),
                    stateOffset + rb.Top + (int)((rb.Height - mk.Height) / 2));

            var separators = (string)d.Separators;
            var separatorImage = (string)d.SeparatorImage;
            if (WidgetsCachedRegion(separators, separatorImage, disabled, (bool)d.Depressed, hover) is { } sp)
                WidgetDumpSprite(sb, WidgetSheetOf(separators), sp,
                    stateOffset + -3 + rb.Right - rb.Height + 4,
                    stateOffset + rb.Top + (int)((rb.Height - sp.Height) / 2));

            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(d);
            Ui.MouseOverWidget = null;
        }

        // Label：文本面快照（含 Align/VAlign 变体）
        var textAlign = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.TextAlign")!;
        var textVAlign = WidgetsAsm.GetType("OpenRA.Mods.Common.Widgets.TextVAlign")!;
        (string Name, Action<dynamic> Apply)[] labelSnapshots =
        [
            ("left-middle", _ => { }),
            ("center-top", l => { dynamic a = Enum.Parse(textAlign, "Center"); dynamic v = Enum.Parse(textVAlign, "Top"); l.Align = a; l.VAlign = v; }),
            ("right-bottom", l => { dynamic a = Enum.Parse(textAlign, "Right"); dynamic v = Enum.Parse(textVAlign, "Bottom"); l.Align = a; l.VAlign = v; }),
        ];
        foreach (var (name, apply) in labelSnapshots)
        {
            dynamic l = WidgetsNew(WidgetsLabel, modData);
            l.Id = "LBL";
            l.Bounds = new WidgetBounds(0, 0, 120, 24);
            l.Text = "Label";
            Ui.Root.AddChild(l);
            apply(l);
            sb.Append("state Label ").Append(name).Append('\n');
            Func<string> getText = l.GetText;
            Func<Color> getColor = l.GetColor;
            WidgetDumpTextLine(sb, (string)l.Font, (string)l.Align.ToString(), (string)l.VAlign.ToString(),
                getColor().ToArgb().ToString("X8", CultureInfo.InvariantCulture), getText());
            sb.Append("end").Append('\n');
            Ui.Root.RemoveChild(l);
        }
    }

    // —— pixels：chrome sheet 像素面（Png 真解码 + FastCopyIntoSprite + PremultiplyAlpha）———

    static byte[] DumpWidgetPixels(string openraRoot)
    {
        var sb = new StringBuilder();
        sb.Append("# chrome sheet pixels oracle (T25/issue #34, D075 recovery)").Append('\n');
        sb.Append("# upstream Png decode + Util.FastCopyIntoSprite + PremultiplyAlpha into Sheet(BGRA) data;").Append('\n');
        sb.Append("# sha256 over full sheet byte data; deterministic sample grid p <x>,<y> <argb8>").Append('\n');
        foreach (var name in WidgetChromeSheets)
        {
            using var fs = File.OpenRead(Path.Combine(openraRoot, "mods", "ra", "uibits", name));
            var png = new Png(fs);
            var sheet = new Sheet(SheetType.BGRA, new Size(png.Width, png.Height));
            var sprite = new Sprite(sheet, new Rectangle(0, 0, png.Width, png.Height), TextureChannel.Red);
            Util.FastCopyIntoSprite(sprite, png);
            var data = sheet.GetData();
            var pixels = MemoryMarshal.Cast<byte, uint>(data);

            sb.Append("png ").Append(name).Append(' ').Append(png.Width).Append('x').Append(png.Height)
                .Append(" sha256=").Append(Convert.ToHexStringLower(SHA256.HashData(data))).Append('\n');
            for (var y = 0; y < png.Height; y += 53)
                for (var x = 0; x < png.Width; x += 47)
                    sb.Append("p ").Append(x).Append(',').Append(y).Append(' ')
                        .Append(pixels[y * png.Width + x].ToString("X8", CultureInfo.InvariantCulture)).Append('\n');
        }

        return WidgetsTextEnd(sb);
    }

    static (string RelPath, byte[] Bytes)[] WidgetsGroup(string openraRoot)
    {
        // Game.Settings 兜底：sfx 模式注入的合成 Settings 仅含 Sound 段（Game 段 null），
        // ModData 装配 GlobalModData 的 DiscordService ctor 读 Game.Settings.Game 会 NRE；
        // 全默认 Settings 下 DiscordService 走 Game.Renderer==null 早退，无副作用。
        Game.Settings = new Settings(null, new Arguments());
        var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
        var modData = new ModData(mods["ra"], mods);
        Game.ModData = modData;
        Game.Sound = new Sound(new StubPlatform(), new SoundSettings());
        Game.Sound.DisableAllSounds = true;
        ChromeMetrics.Initialize(modData);
        ChromeProvider.Initialize(modData);
        WidgetsBindCommon();

        var states = new StringBuilder();
        DumpWidgetStates(states, modData);
        var draws = new StringBuilder();
        DumpWidgetDraws(draws, modData);

        return
        [
            ($"{WidgetsGoldenRelDir}/states.txt", WidgetsTextEnd(states)),
            ($"{WidgetsGoldenRelDir}/draw.txt", WidgetsTextEnd(draws)),
            ($"{WidgetsGoldenRelDir}/pixels.txt", DumpWidgetPixels(openraRoot)),
        ];
    }
}
