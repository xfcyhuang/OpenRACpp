// music 模式（T20/issue #32）：合成 12 轨曲库（入库 music.yaml 单一真源，t03/t07 隐藏、
// t06 伪文件缺失）驱动的音乐播放器剧本 dump。真源面：上游 MersenneTwister +
// OpenRA.Mods.Common.Util.Shuffle（反射加载，同 audio 模式禁忌——ProjectReference 会因
// ObjectCreator 自定义 ALC 分裂类型身份）产出洗牌序；MusicPlaylist/Sound 音乐面为
// C# 逐行复刻（MusicPlaylist 依赖 World/Map.Rules 不可无头装配，GetSong/Play/Stop/
// PlayNextSong 与 Sound.PlayMusicThen/PlayMusic/StopMusic/Tick 逐行对照
// MusicPlaylist.cs:65-254 / Sound.cs:217-304）。伪音频文件 = 空文件（上游 MusicInfo.Load
// 语义：TryOpen 成功 → Exists=true；SoundLoaders 全不中 → Length=0）。
// 转储格式（与 tests/music.cpp 逐字节同形）：
//   # music player scenario seed=<> shuffle=<> repeat=<> music-volume=<R> background=<key|->
//     starting=<key|-> allow-mute-background=<> mute-background=<>
//   init installed=<n> available=<True|False> playlist=<csv> random=<csv>
//   op play -> play <key> vol=<R> loop=<True|False> | idle        （事件快照）
//   op complete -> 同上（强制 finish 后 tick 续播）
//   op next|prev -> <key|->
//   op play-key <key> -> 同 play
//   op stop -> stopped[; play <key> vol=<R> loop=<>]              （背景曲恢复）
//   op volume <R> -> live=<R|->                                   （music.Volume 直设）
//   op mute-background <Bool> -> muted=<Bool>
//   events <csv of played keys>
// float "R" ≍ C++ std::to_chars 最短往返。

internal static partial class Program
{
    const string MusicBaselineCommit = "1f594e32065615ed1847962c80ecfe6e868a11c3";

    static readonly string[] MusicPlaylistKeys = { "t01", "t02", "t04", "t05", "t08", "t09", "t10", "t11", "t12" };

    static byte[] MusicLibraryYaml()
    {
        var sb = new StringBuilder();
        sb.Append("t01: Track One\n");
        sb.Append("t02: Track Two\n\tVolumeModifier: 0.7\n");
        sb.Append("t03: Track Three\n\tHidden: true\n");
        sb.Append("t04: Track Four\n");
        sb.Append("t05: Track Five\n");
        sb.Append("t06: Track Six\n");
        sb.Append("t07: Track Seven\n\tHidden: true\n");
        sb.Append("t08: Track Eight\n");
        sb.Append("t09: Track Nine\n");
        sb.Append("t10: Track Ten\n\tVolumeModifier: 0.6\n");
        sb.Append("t11: Track Eleven\n");
        sb.Append("t12: Track Twelve\n");
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    static List<int> MusicUtilShuffle(int count, MersenneTwister mt)
    {
        var modsCommon = LoadModsCommonAssembly();
        var util = modsCommon.GetType("OpenRA.Mods.Common.Util")
            ?? throw new InvalidOperationException("OpenRA.Mods.Common 未构建（Util 类型缺失）");
        var shuffle = util.GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Single(m => m.Name == "Shuffle" && m.GetParameters().Length == 2);
        var enumerable = shuffle.MakeGenericMethod(typeof(int))
            .Invoke(null, new object[] { Enumerable.Range(0, count), mt });
        return ((IEnumerable<int>)enumerable).ToList();
    }

    static byte[] DumpMusicPlaylistOrder()
    {
        var sb = new StringBuilder();
        sb.Append("# baseline ").Append(MusicBaselineCommit)
            .Append(" music playlist shuffle n=").Append(MusicPlaylistKeys.Length).Append('\n');
        sb.Append("playlist ").Append(string.Join(",", MusicPlaylistKeys)).Append('\n');
        foreach (var seed in new[] { 1, 2, 3 })
        {
            var mt = new MersenneTwister(seed);
            var order = MusicUtilShuffle(MusicPlaylistKeys.Length, mt);
            sb.Append("seed ").Append(seed).Append(" draws ").Append(mt.TotalCount)
                .Append(" random ")
                .Append(string.Join(",", order.Select(i => MusicPlaylistKeys[i]))).Append('\n');
        }
        return new UTF8Encoding(false).GetBytes(sb.ToString());
    }

    sealed class MusicOracleTrack
    {
        public string Key;
        public MusicInfo Info;
        public bool Exists;
    }

    sealed class MusicOracleStream
    {
        public string Name;
        public float Volume;
        public bool Loop;
        public bool Complete;
    }

    sealed class MusicOraclePlayer
    {
        public readonly List<MusicOracleTrack> Library;
        public readonly List<MusicOracleTrack> Playlist = new();
        public readonly List<MusicOracleTrack> Random = new();
        public string CurrentSong;
        public string CurrentBackgroundSong;
        public bool CurrentSongIsBackground;
        public bool IsMusicAvailable;
        public readonly bool AllowMuteBackgroundMusic;

        readonly Func<bool> shuffleGet;
        readonly Func<bool> repeatGet;
        readonly Func<bool> muteBackgroundGet;
        readonly Func<float> musicVolumeGet;
        readonly Action<float> musicVolumeSet;

        public MusicOracleStream Music;
        public MusicOracleTrack CurrentMusic;
        public bool MusicPlaying;
        public readonly List<string> Events = new();
        public float LiveGain = -1f;

        public MusicOraclePlayer(List<MusicOracleTrack> library, MersenneTwister cosmeticRandom,
            Func<bool> shuffle, Func<bool> repeat, Func<bool> muteBackground,
            Func<float> musicVolume, Action<float> musicVolumeSet,
            string backgroundMusic, string startingMusic, bool allowMuteBackgroundMusic)
        {
            Library = library;
            shuffleGet = shuffle;
            repeatGet = repeat;
            muteBackgroundGet = muteBackground;
            musicVolumeGet = musicVolume;
            this.musicVolumeSet = musicVolumeSet;
            AllowMuteBackgroundMusic = allowMuteBackgroundMusic;

            var installed = library.Where(t => t.Exists).ToList();
            if (!installed.Any())
                return;

            foreach (var t in library.Where(t => t.Exists && !t.Info.Hidden))
                Playlist.Add(t);

            // MusicPlaylist ctor: random = playlist.Shuffle(Game.CosmeticRandom).ToArray()
            var order = MusicUtilShuffle(Playlist.Count, cosmeticRandom);
            foreach (var i in order)
                Random.Add(Playlist[i]);
            IsMusicAvailable = Playlist.Count > 0;

            if (SongExists(backgroundMusic))
            {
                CurrentBackgroundSong = backgroundMusic;
                CurrentSong = backgroundMusic;
                CurrentSongIsBackground = true;
            }
            else if (Random.Count > 0)
            {
                CurrentSong = Random[0].Key;
            }

            if (SongExists(startingMusic))
            {
                CurrentSong = startingMusic;
                CurrentSongIsBackground = false;
            }
        }

        public MusicOracleTrack Find(string key)
        {
            if (key == null)
                return null;
            return Library.FirstOrDefault(t => t.Key == key);
        }

        bool SongExists(string key)
        {
            var t = Find(key);
            return t != null && t.Exists;
        }

        public string GetSong(bool reverse)
        {
            if (!IsMusicAvailable)
                return null;

            var songs = shuffleGet() ? Random : Playlist;
            IEnumerable<MusicOracleTrack> seq = reverse ? ((IEnumerable<MusicOracleTrack>)songs).Reverse() : songs;
            var next = seq.SkipWhile(t => t.Key != CurrentSong).Skip(1).FirstOrDefault()
                ?? seq.FirstOrDefault();

            if (next != null && next.Exists)
                return next.Key;
            return null;
        }

        bool BackgroundMusicMuted()
        {
            return AllowMuteBackgroundMusic && muteBackgroundGet();
        }

        // MusicPlaylist.Play
        public void Play()
        {
            if (!SongExists(CurrentSong) || (CurrentSongIsBackground && BackgroundMusicMuted()))
                return;
            PlayMusicThen(Find(CurrentSong));
        }

        // MusicPlaylist.PlayNextSong（Sound.Tick 完成回调折叠为调用方显式步）
        public void PlayNextSong()
        {
            if (!CurrentSongIsBackground)
                CurrentSong = GetSong(false);
            Play();
        }

        // MusicPlaylist.Play(MusicInfo)
        public void PlayTrack(string key)
        {
            var t = Find(key);
            if (t == null)
                return;
            CurrentSong = key;
            CurrentSongIsBackground = false;
            Play();
        }

        // MusicPlaylist.Stop
        public void Stop()
        {
            CurrentSong = null;
            StopMusic();
            if (CurrentBackgroundSong != null)
            {
                CurrentSong = CurrentBackgroundSong;
                CurrentSongIsBackground = true;
                if (!BackgroundMusicMuted())
                    Play();
            }
        }

        // Sound.PlayMusicThen（onMusicComplete = 播放器 PlayNextSong 折叠进剧本 complete 步）
        public void PlayMusicThen(MusicOracleTrack m)
        {
            if (m == null || !m.Exists)
                return;
            if (CurrentMusic == m && Music != null)
            {
                MusicPlaying = true;
                return;
            }
            PlayMusic(m, repeatGet());
        }

        // Sound.PlayMusic
        public void PlayMusic(MusicOracleTrack m, bool looped)
        {
            if (m == null || !m.Exists)
                return;
            StopMusic();
            var volume = musicVolumeGet() * m.Info.VolumeModifier;
            Music = new MusicOracleStream { Name = m.Info.Filename, Volume = volume, Loop = looped };
            Events.Add(m.Key);
            LiveGain = volume;
            CurrentMusic = m;
            MusicPlaying = true;
        }

        // Sound.StopMusic
        public void StopMusic()
        {
            Music = null;
            CurrentMusic = null;
            MusicPlaying = false;
        }

        // Sound.Tick（完成检测）——返回是否触发完成续播
        public bool Tick()
        {
            if (MusicPlaying && Music != null && Music.Complete)
            {
                StopMusic();
                return true;
            }
            return false;
        }

        // Sound.MusicVolume setter
        public void SetMusicVolume(float value)
        {
            musicVolumeSet(value);
            if (Music != null)
                LiveGain = value;
        }
    }

    sealed class MusicScenario
    {
        public int Seed = 1;
        public bool Shuffle;
        public bool Repeat;
        public string Background = "";
        public string Starting = "";
        public bool AllowMuteBackground;
        public bool MuteBackground;
        public string[] Ops = Array.Empty<string>();
    }

    static string RunMusicScenario(MusicScenario scenario)
    {
        // 合成曲库（与 tests/golden/music/music.yaml 逐字段同构；t06 伪文件缺失）
        var library = new List<MusicOracleTrack>();
        for (var i = 1; i <= 12; i++)
        {
            var key = $"t{i:00}";
            var children = i == 2
                ? new List<MiniYamlNode> { new("VolumeModifier", "0.7") }
                : i == 3 || i == 7
                    ? new List<MiniYamlNode> { new("Hidden", "true") }
                    : i == 10
                        ? new List<MiniYamlNode> { new("VolumeModifier", "0.6") }
                        : new List<MiniYamlNode>();
            var info = new MusicInfo(key, new MiniYaml($"Track {i:00}", children));
            library.Add(new MusicOracleTrack { Key = key, Info = info, Exists = i != 6 });
        }

        var shuffle = scenario.Shuffle;
        var repeat = scenario.Repeat;
        var muteBackground = scenario.MuteBackground;
        var musicVolume = 0.5f;

        using var tempDir = new TempMusicDir();
        foreach (var t in library.Where(t => t.Exists))
            File.WriteAllBytes(Path.Combine(tempDir.Path, t.Info.Filename), Array.Empty<byte>());
        foreach (var t in library)
            t.Exists = File.Exists(Path.Combine(tempDir.Path, t.Info.Filename));

        var player = new MusicOraclePlayer(library, new MersenneTwister(scenario.Seed),
            () => shuffle, () => repeat, () => muteBackground, () => musicVolume,
            v => musicVolume = v, scenario.Background, scenario.Starting,
            scenario.AllowMuteBackground);

        var installed = library.Count(t => t.Exists);
        var sb = new StringBuilder();
        sb.Append("# music player scenario seed=").Append(scenario.Seed)
            .Append(" shuffle=").Append(scenario.Shuffle ? "True" : "False")
            .Append(" repeat=").Append(scenario.Repeat ? "True" : "False")
            .Append(" music-volume=").Append(FmtFloatR(musicVolume))
            .Append(" background=").Append(scenario.Background.Length == 0 ? "-" : scenario.Background)
            .Append(" starting=").Append(scenario.Starting.Length == 0 ? "-" : scenario.Starting)
            .Append(" allow-mute-background=").Append(scenario.AllowMuteBackground ? "True" : "False")
            .Append(" mute-background=").Append(scenario.MuteBackground ? "True" : "False")
            .Append('\n');
        string JoinCsv(IEnumerable<MusicOracleTrack> ts) => string.Join(",", ts.Select(t => t.Key));
        sb.Append("init installed=").Append(installed)
            .Append(" available=").Append(player.IsMusicAvailable ? "True" : "False")
            .Append(" playlist=").Append(JoinCsv(player.Playlist))
            .Append(" random=").Append(JoinCsv(player.Random)).Append('\n');

        var played = new List<string>();
        string PlayLine(string prefix, int eventsBefore)
        {
            if (player.Events.Count == eventsBefore)
                return prefix + "same";
            var track = player.CurrentMusic;
            var key = track != null ? track.Key : "";
            return prefix + "play " + key + " vol=" + FmtFloatR(player.Music.Volume)
                + " loop=" + (player.Music.Loop ? "True" : "False");
        }

        foreach (var op in scenario.Ops)
        {
            var line = "op " + op + " -> ";
            if (op == "play")
            {
                var before = player.Events.Count;
                player.Play();
                if (player.Events.Count > before)
                    played.Add(player.CurrentSong);
                line += PlayLine("", before);
            }
            else if (op == "complete")
            {
                var before = player.Events.Count;
                if (player.Music != null)
                    player.Music.Complete = true;
                if (player.Tick())
                    player.PlayNextSong();
                if (player.Events.Count > before)
                    played.Add(player.CurrentSong);
                line += PlayLine("", before);
            }
            else if (op == "next")
            {
                var key = player.GetSong(false);
                line += key ?? "-";
            }
            else if (op == "prev")
            {
                var key = player.GetSong(true);
                line += key ?? "-";
            }
            else if (op.StartsWith("play-key ", StringComparison.Ordinal))
            {
                var key = op.Substring(9);
                var before = player.Events.Count;
                player.PlayTrack(key);
                if (player.Events.Count > before)
                    played.Add(key);
                line += PlayLine("", before);
            }
            else if (op == "stop")
            {
                var before = player.Events.Count;
                player.Stop();
                line += "stopped";
                if (player.Events.Count > before)
                {
                    played.Add(player.CurrentSong);
                    line += PlayLine("; ", before);
                }
            }
            else if (op.StartsWith("volume ", StringComparison.Ordinal))
            {
                var value = float.Parse(op.Substring(7), CultureInfo.InvariantCulture);
                player.SetMusicVolume(value);
                line += player.Music != null ? "live=" + FmtFloatR(player.LiveGain) : "live=-";
            }
            else if (op.StartsWith("mute-background ", StringComparison.Ordinal))
            {
                muteBackground = op.Substring(16) == "True";
                line += "muted=" + (muteBackground ? "True" : "False");
            }
            else
                throw new ArgumentException("未知剧本操作：" + op);
            sb.Append(line).Append('\n');
        }

        sb.Append("events");
        foreach (var key in played)
            sb.Append(' ').Append(key);
        sb.Append('\n');
        return sb.ToString();
    }

    sealed class TempMusicDir : IDisposable
    {
        public readonly string Path =
            System.IO.Path.Combine(System.IO.Path.GetTempPath(), "ora-music-golden-oracle");

        public TempMusicDir()
        {
            if (Directory.Exists(Path))
                Directory.Delete(Path, true);
            Directory.CreateDirectory(Path);
        }

        public void Dispose()
        {
            if (Directory.Exists(Path))
                Directory.Delete(Path, true);
        }
    }

    static byte[] DumpMusicScenarioFile(MusicScenario scenario) =>
        new UTF8Encoding(false).GetBytes(RunMusicScenario(scenario));

    static MusicScenario MusicScenarioOrder() => new()
    {
        Ops = new[]
        {
            "play", "complete", "next", "prev", "play-key t12", "complete", "play-key t01",
            "prev", "stop", "next", "play", "volume 0.25", "play-key t02", "volume 0.3",
            "complete", "stop"
        }
    };

    static MusicScenario MusicScenarioShuffle() => new()
    {
        Seed = 2,
        Shuffle = true,
        Ops = new[]
        {
            "play", "complete", "complete", "complete", "next", "prev", "stop", "next",
            "play-key t02", "complete"
        }
    };

    static MusicScenario MusicScenarioRepeat() => new()
    {
        Seed = 3,
        Repeat = true,
        Ops = new[] { "play", "complete", "play", "stop", "play", "complete" }
    };

    static MusicScenario MusicScenarioBackground() => new()
    {
        Background = "t04",
        AllowMuteBackground = true,
        Ops = new[]
        {
            "play", "complete", "stop", "mute-background True", "play", "stop",
            "mute-background False", "stop", "play-key t08", "stop", "play"
        }
    };
}
