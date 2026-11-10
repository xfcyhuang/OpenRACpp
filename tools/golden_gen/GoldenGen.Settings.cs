using System.Collections.Frozen;
using System.Collections.Immutable;

internal static partial class Program
{
// —— settings 模式（T21/issue #28）：轻路径，直接 new Settings(path, args)，
// 不装配 ModData/Game.Settings（SettingsModule.Save 会碰 Game.Settings，模式内
// 一律走实例 Save()）。产物 = 写盘原始字节（CRLF 原样），UUID 行级归一化
// （\tUUID: <guid> → \tUUID: <UUID>，覆盖"UUID 恒必写"事实本身）。
// 警告产物（badval-warnings.txt）以 \n 连接（Console.SetOut 捕获后 \r\n→\n）。

    sealed class SettingsFloatHolder
    {
        public float F;
    }

    static string FormatFloatViaFieldSaver(float f)
    {
        var holder = new SettingsFloatHolder { F = f };
        var field = typeof(SettingsFloatHolder).GetField(nameof(SettingsFloatHolder.F))!;
        return FieldSaver.FormatValue(holder, field);
    }

    static string NormalizeSettingsUuidText(string text) =>
        System.Text.RegularExpressions.Regex.Replace(
            text,
            @"(\tUUID: )[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}",
            "$1<UUID>");

    static string NewSettingsTempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), "ora-golden-settings",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return dir;
    }

    static byte[] ReadSettingsFile(string path) =>
        new UTF8Encoding(false).GetBytes(NormalizeSettingsUuidText(File.ReadAllText(path)));

    static byte[] DumpSettingsFreshDefault()
    {
        var dir = NewSettingsTempDir();
        new Settings(Path.Combine(dir, "settings.yaml"), Arguments.Empty).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static void RunSettingsAllTypes(string dir)
    {
        var s = new Settings(Path.Combine(dir, "settings.yaml"), Arguments.Empty);
        s.Player.Name = "Tester";
        s.Player.Color = Color.FromArgb(200, 32, 200);
        s.Player.CustomColors = ImmutableArray.Create(
            Color.FromArgb(200, 32, 200), Color.FromArgb(0, 255, 128));
        s.Game.MouseScroll = MouseScrollType.Standard;
        s.Game.MPGameFilters = MPGameFilters.Waiting | MPGameFilters.Protected;
        s.Game.ZoomModifier = Modifiers.Shift;
        s.Sound.SoundVolume = 0.75f;
        s.Sound.Device = "OpenAL Soft on Speakers";
        s.Sound.Mute = true;
        s.Graphics.Mode = WindowMode.Windowed;
        s.Graphics.WindowedSize = new int2(1600, 900);
        s.Graphics.UIScale = 1.25f;
        s.Server.Name = "My # Server";
        s.Server.Ban = new HashSet<string> { "1.2.3.4", "5.6.7.8" }.ToFrozenSet();
        s.Server.ProfileIDWhitelist = new HashSet<int> { 1, 3, 4, 5 }.ToFrozenSet();
        s.Debug.PerfText = true;
        s.Debug.UUID = "021de054-4aa4-470e-8e3c-dc006dc1312b";
        s.Save();
    }

    static byte[] DumpSettingsAllTypes()
    {
        var dir = NewSettingsTempDir();
        RunSettingsAllTypes(dir);
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsRoundtrip()
    {
        var dir = NewSettingsTempDir();
        RunSettingsAllTypes(dir);
        var src = Path.Combine(dir, "settings.yaml");
        var dst = Path.Combine(dir, "roundtrip.yaml");
        File.Copy(src, dst);
        new Settings(dst, Arguments.Empty).Save();
        return ReadSettingsFile(dst);
    }

    const string SettingsUnknownInput =
        "# top level user comment\n" +
        "Player:\n\tName: Tester2\n\n" +
        "UnknownSection:\n\tSomeKey: SomeValue\n\tNested:\n\t\tDeep: 5\n\n" +
        "Sound:\n\tMute: True\n\tUnknownInKnown: keepme\n\tSoundVolume: 0.4\n" +
        "\tMusicVolume: 0.3 # trailing comment\n\n" +
        "Server: # trailing on section\n\tName: x\n";

    const string SettingsBlankSplitInput =
        "Sound:\n\tMute: True\n\n\tMusicVolume: 0.3\n";

    const string SettingsBadvalInput =
        "Sound:\n\tSoundVolume: garbage\n\tMute: notabool\n" +
        "Game:\n\tMouseScroll: garbage\n\tSelectionDeadzone: notanint\n" +
        "Graphics:\n\tMode: garbage\n\tWindowedSize: notanint2\n" +
        "Player:\n\tColor: nothex\n\tCustomColors: C820C8, zz\n";

    const string SettingsKeysInput =
        "Keys:\n\tSell: Q Ctrl\n\tAutocomplete: F2\n\n" +
        "Sound:\n\tMute: True\n";

    static Settings RunSettingsFromText(string dir, string fileName, string text, out string warnings)
    {
        var path = Path.Combine(dir, fileName);
        File.WriteAllText(path, text);
        var sw = new StringWriter();
        var old = Console.Out;
        Settings s;
        try
        {
            Console.SetOut(sw);
            s = new Settings(path, Arguments.Empty);
        }
        finally
        {
            Console.SetOut(old);
        }
        warnings = sw.ToString().Replace("\r\n", "\n");
        return s;
    }

    static byte[] DumpSettingsUnknownPreserved()
    {
        var dir = NewSettingsTempDir();
        RunSettingsFromText(dir, "settings.yaml", SettingsUnknownInput, out _).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsBlankSplit()
    {
        var dir = NewSettingsTempDir();
        RunSettingsFromText(dir, "settings.yaml", SettingsBlankSplitInput, out _).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsBadvalFile()
    {
        var dir = NewSettingsTempDir();
        RunSettingsFromText(dir, "settings.yaml", SettingsBadvalInput, out _).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsBadvalWarnings()
    {
        var dir = NewSettingsTempDir();
        RunSettingsFromText(dir, "settings.yaml", SettingsBadvalInput, out var warnings);
        return new UTF8Encoding(false).GetBytes(warnings);
    }

    static byte[] DumpSettingsCliArgs()
    {
        var dir = NewSettingsTempDir();
        var args = new Arguments("Sound.Mute=True", "Game.ZoomModifier=Shift");
        new Settings(Path.Combine(dir, "settings.yaml"), args).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsKeysRebind()
    {
        var dir = NewSettingsTempDir();
        RunSettingsFromText(dir, "settings.yaml", SettingsKeysInput, out _).Save();
        return ReadSettingsFile(Path.Combine(dir, "settings.yaml"));
    }

    static byte[] DumpSettingsFloatFormats()
    {
        var values = new[]
        {
            0.5f, 0.75f, 30f, 50f, 1f, 0.04f, 0.1f, 1.25f, 2.5f, 16777216f,
            1e-8f, 0.0001f, 0.00001f, 0.000001f, 100000f, 1000000f, 10000000f, 100000000f,
            1000000000f, 123456.789f, 0.1f + 0.2f, -0.5f, 3.4028235e38f, 1e-5f,
            1234.5678f, 99999999f, 999999999f,
        };
        var sb = new StringBuilder();
        foreach (var f in values)
            sb.Append(FormatFloatViaFieldSaver(f)).Append('\n');
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }
}
