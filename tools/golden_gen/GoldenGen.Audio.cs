// audio 模式（T18/issue #27）：ra audio 三 yaml（notifications/voices/music）经上游
// SoundInfo/MusicInfo 构造后规范化转储 + mods/d2k/bits 四个 WAV 语料的上游 WavLoader
// 解码统计（pcmBytes+sum+head8 钉死解码正确性，lenSec 复刻「整文件长度」怪癖）。
// Mods.Common 程序集走反射加载（Rules.cs 禁忌：ProjectReference 会因 ObjectCreator
// 自定义 ALC 双加载分裂 trait 类型身份），WavLoader/ISoundFormat 经 OpenRA.ISoundLoader
// 接口直调（接口在 OpenRA.Game，直接引用）。
// 转储格式（与 tests/audio.cpp 逐字节同形；# 头行 C++ 侧跳过）：
//   type <lowered-key>
//     default_variant <v> / default_prefix <v>
//     variants <k>=<csv> / prefixes <k>=<csv>            键 Ordinal 排序、数组原序
//     disable_variants <csv> / disable_prefixes <csv>    排序、非空才落行
//     voices <k>=<csv>|vol=1|int=DoNotPlay               键排序（VoicePools 恒定参数显式落行防漂移）
//     notifications <k>=<csv>|vol=<R>|int=<Enum>          键排序、float 最短往返
//   track <lowered-key>|<filename>|<title>|hidden=<True|False>|vol=<R>   键排序
// float "R" ≍ C++ std::to_chars 最短往返；lenSec 固定 F6 invariant。

global using OpenRA.GameRules;

internal static partial class Program
{
    const string AudioBaselineCommit = "1f594e32065615ed1847962c80ecfe6e868a11c3";

    static string FmtFloatR(float f) => f.ToString("R", CultureInfo.InvariantCulture);

    static Assembly LoadModsCommonAssembly()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "OpenRA.Mods.Common.dll");
        return Assembly.LoadFrom(path);
    }

    static byte[] DumpAudioSoundInfo(string openraRoot, string relPath)
    {
        var nodes = MiniYaml.FromFile(Path.Combine(openraRoot, relPath)).ToList();
        var sb = new StringBuilder();
        sb.Append("# baseline ").Append(AudioBaselineCommit).Append(" audio soundinfo dump ").Append(relPath).Append('\n');
        foreach (var node in nodes.OrderBy(n => n.Key.ToLowerInvariant(), StringComparer.Ordinal))
        {
            var info = new SoundInfo(node.Value);
            sb.Append("type ").Append(node.Key.ToLowerInvariant()).Append('\n');
            sb.Append("  default_variant ").Append(info.DefaultVariant).Append('\n');
            sb.Append("  default_prefix ").Append(info.DefaultPrefix).Append('\n');
            foreach (var kv in info.Variants.OrderBy(k => k.Key, StringComparer.Ordinal))
                sb.Append("  variants ").Append(kv.Key).Append('=').Append(string.Join(",", kv.Value)).Append('\n');
            foreach (var kv in info.Prefixes.OrderBy(k => k.Key, StringComparer.Ordinal))
                sb.Append("  prefixes ").Append(kv.Key).Append('=').Append(string.Join(",", kv.Value)).Append('\n');
            if (info.DisableVariants.Count > 0)
                sb.Append("  disable_variants ").Append(string.Join(",",
                    info.DisableVariants.OrderBy(x => x, StringComparer.Ordinal))).Append('\n');
            if (info.DisablePrefixes.Count > 0)
                sb.Append("  disable_prefixes ").Append(string.Join(",",
                    info.DisablePrefixes.OrderBy(x => x, StringComparer.Ordinal))).Append('\n');
            foreach (var kv in info.Voices.OrderBy(k => k.Key, StringComparer.Ordinal))
                sb.Append("  voices ").Append(kv.Key).Append('=').Append(string.Join(",", kv.Value))
                    .Append("|vol=").Append(FmtFloatR(1f)).Append("|int=DoNotPlay").Append('\n');
            if (node.Value.Nodes.Any(n => n.Key == "Notifications"))
            {
                var pools = info.NotificationsPools.Value;
                foreach (var kv in pools.OrderBy(k => k.Key, StringComparer.Ordinal))
                {
                    var clips = info.Notifications[kv.Key];
                    sb.Append("  notifications ").Append(kv.Key).Append('=')
                        .Append(string.Join(",", clips))
                        .Append("|vol=").Append(FmtFloatR(kv.Value.VolumeModifier))
                        .Append("|int=").Append(kv.Value.Type).Append('\n');
                }
            }
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static byte[] DumpAudioMusicInfo(string openraRoot, string relPath)
    {
        var nodes = MiniYaml.FromFile(Path.Combine(openraRoot, relPath)).ToList();
        var sb = new StringBuilder();
        sb.Append("# baseline ").Append(AudioBaselineCommit).Append(" audio music dump ").Append(relPath).Append('\n');
        foreach (var node in nodes.OrderBy(n => n.Key.ToLowerInvariant(), StringComparer.Ordinal))
        {
            var m = new MusicInfo(node.Key, node.Value);
            sb.Append("track ").Append(node.Key.ToLowerInvariant())
                .Append('|').Append(m.Filename)
                .Append('|').Append(m.Title)
                .Append("|hidden=").Append(m.Hidden ? "True" : "False")
                .Append("|vol=").Append(FmtFloatR(m.VolumeModifier)).Append('\n');
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static byte[] DumpWavStats(string openraRoot)
    {
        var modsCommon = LoadModsCommonAssembly();
        var loaderType = modsCommon.GetType("OpenRA.Mods.Common.AudioLoaders.WavLoader")
            ?? throw new InvalidOperationException("OpenRA.Mods.Common 未构建（WavLoader 类型缺失）");
        var loader = (ISoundLoader)Activator.CreateInstance(loaderType);

        var sb = new StringBuilder();
        sb.Append("# baseline ").Append(AudioBaselineCommit).Append(" wav decode stats").Append('\n');
        foreach (var name in new[] { "CHAT1REVERSE.WAV", "DEVASTATORMELTDOWN.WAV", "ORNIBOMB.WAV", "RELEASESARDAUKARS.WAV" })
        {
            using var fs = File.OpenRead(Path.Combine(openraRoot, "mods", "d2k", "bits", name));
            if (!loader.TryParseSound(fs, out var fmt))
            {
                sb.Append(name).Append(": NOT WAV").Append('\n');
                continue;
            }

            using var pcmStream = fmt.GetPCMInputStream();
            using var ms = new MemoryStream();
            pcmStream.CopyTo(ms);
            var pcm = ms.ToArray();
            long sum = 0;
            for (var i = 0; i + 1 < pcm.Length; i += 2)
                sum += BitConverter.ToInt16(pcm, i);
            var head = new List<string>();
            for (var i = 0; i < 8 && (i * 2 + 1) < pcm.Length; i++)
                head.Add(BitConverter.ToInt16(pcm, i * 2).ToString(CultureInfo.InvariantCulture));

            sb.Append(name).Append(": ch=").Append(fmt.Channels)
                .Append(" bits=").Append(fmt.SampleBits)
                .Append(" rate=").Append(fmt.SampleRate)
                .Append(" lenSec=").Append(fmt.LengthInSeconds.ToString("F6", CultureInfo.InvariantCulture))
                .Append(" pcmBytes=").Append(pcm.Length)
                .Append(" sum=").Append(sum)
                .Append(" head8=").Append(string.Join(",", head)).Append('\n');
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }
}
