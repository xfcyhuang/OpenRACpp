internal static partial class Program
{
static void WriteGolden(string artifactPath, string relPath, byte[] bytes)
{
    var dir = Path.GetDirectoryName(artifactPath);
    Directory.CreateDirectory(dir);

    // 临时文件 + 原子替换，避免中断留下半截产物
    var tmp = artifactPath + ".tmp";
    File.WriteAllBytes(tmp, bytes);
    File.Move(tmp, artifactPath, overwrite: true);

    var sidecar = artifactPath + ".sha256";
    tmp = sidecar + ".tmp";
    File.WriteAllText(tmp, SidecarLine(relPath, bytes), new UTF8Encoding(false));
    File.Move(tmp, sidecar, overwrite: true);

    Console.WriteLine($"gen: {relPath} sha256={Sha256Hex(bytes)} bytes={bytes.Length}");
}

static bool VerifyGolden(string artifactPath, string relPath, byte[] produced)
{
    if (!File.Exists(artifactPath))
    {
        Console.WriteLine($"verify: {relPath} FAIL —— 黄金文件缺失，先运行 gen 生成");
        return false;
    }

    var golden = File.ReadAllBytes(artifactPath);
    if (!produced.AsSpan().SequenceEqual(golden))
    {
        var offset = FirstDiffOffset(produced, golden);
        Console.WriteLine($"verify: {relPath} FAIL —— 与入库黄金字节不一致" +
            $"（produced={produced.Length}B golden={golden.Length}B，首个差异偏移 {offset}）");
        return false;
    }

    // 旁车 .sha256 与黄金文件自身一致性（格式兼容 `sha256sum -c`）
    var sidecar = artifactPath + ".sha256";
    if (!File.Exists(sidecar) || File.ReadAllText(sidecar) != SidecarLine(relPath, golden))
    {
        Console.WriteLine($"verify: {relPath} FAIL —— .sha256 旁车缺失或与黄金文件不匹配");
        return false;
    }

    return true;
}

static string SidecarLine(string relPath, byte[] bytes) =>
    $"{Sha256Hex(bytes)}  {Path.GetFileName(relPath)}\n";

static string Sha256Hex(byte[] bytes) =>
    Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();

static int FirstDiffOffset(byte[] a, byte[] b)
{
    var n = Math.Min(a.Length, b.Length);
    for (var i = 0; i < n; i++)
        if (a[i] != b[i]) return i;
    return n;
}

// —— 基线断言 ——
static void AssertUpstreamBaseline(string openraRoot, string repoRoot)
{
    var baselineFile = Path.Combine(repoRoot, "UPSTREAM.baseline");
    if (!File.Exists(baselineFile))
        throw new InvalidOperationException($"未找到 {baselineFile}");

    var expected = File.ReadLines(baselineFile).Select(l => l.Trim()).FirstOrDefault(l => l.Length > 0)
        ?? throw new InvalidOperationException("UPSTREAM.baseline 为空");

    var actual = RunCapture("git", $"-C \"{openraRoot}\" rev-parse HEAD", "git rev-parse 失败").Trim();

    if (!string.Equals(expected, actual, StringComparison.OrdinalIgnoreCase))
        throw new BaselineMismatchException(
            $"上游检出与基线不符：UPSTREAM.baseline={expected}，{openraRoot} HEAD={actual}。" +
            "请按 UPSTREAM.baseline 同步流程处理后再产/验黄金。");
}

// —— 路径解析 ——
static string ResolveRepoRoot(string arg)
{
    if (arg != null)
    {
        var full = Path.GetFullPath(arg);
        if (!File.Exists(Path.Combine(full, "UPSTREAM.baseline")))
            throw new InvalidOperationException($"{full} 下无 UPSTREAM.baseline，不是本仓库根目录");
        return full;
    }

    // 默认：自当前目录向上找 UPSTREAM.baseline
    for (var dir = new DirectoryInfo(Directory.GetCurrentDirectory()); dir != null; dir = dir.Parent)
        if (File.Exists(Path.Combine(dir.FullName, "UPSTREAM.baseline")))
            return dir.FullName;

    throw new InvalidOperationException("未能定位仓库根（未指定 --repo-root 且当前目录链上无 UPSTREAM.baseline）");
}

static string ResolveOpenRaRoot(string arg, string repoRoot)
{
    // 主检出布局 <repo>/../openra（PORTING_PLAN 约定）；worktree 布局 <repo>/../../openra
    string[] candidates = arg != null ? [arg] :
    [
        Path.GetFullPath(Path.Combine(repoRoot, "..", "openra")),
        Path.GetFullPath(Path.Combine(repoRoot, "..", "..", "openra")),
    ];

    foreach (var c in candidates)
        if (File.Exists(Path.Combine(c, "OpenRA.slnx")))
            return c;

    throw new InvalidOperationException($"未找到上游 openra 检出（尝试：{string.Join("; ", candidates)}）");
}

// —— 进程工具 ——
static string RunCapture(string exe, string arguments, string what)
{
    var psi = new ProcessStartInfo(exe, arguments)
    {
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
    };
    using var p = Process.Start(psi) ?? throw new InvalidOperationException($"无法启动 {exe}");
    var stdout = p.StandardOutput.ReadToEnd();
    var stderr = p.StandardError.ReadToEnd();
    p.WaitForExit();
    if (p.ExitCode != 0)
        throw new InvalidOperationException($"{what}（exit {p.ExitCode}）：{stderr.Trim()}");
    return stdout;
}

static string NextArg(string[] args, ref int i)
{
    if (++i >= args.Length) throw new ArgumentException("选项缺少取值");
    return args[i];
}

static int Fail(int code, string message)
{
    Console.Error.WriteLine($"golden-gen: {message}");
    return code;
}
}
