internal static partial class Program
{
static byte[] DumpPrngSequence(int seed)
{
    var sb = new StringBuilder();
    sb.Append("# seed ").Append(seed).Append('\n');
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 OpenRA.Game/Support/MersenneTwister.cs").Append('\n');
    sb.Append("# u32 x1000 NextUint hex | next x200 Next dec | f32 x200 NextFloat ieee754 bits hex | u64 x100 NextUlong hex").Append('\n');
    sb.Append("# range x200 Next(2,10) dec | rangen x100 Next(-5,5) dec | pw x100 PickWeighted[1,3,6] dec | pw0 x100 PickWeighted[0,0,0] dec").Append('\n');
    sb.Append("# shuffle x100 ShuffleInPlace[10..14] | u32_624 NextUint #624..#626 hex (after 623 NextUint)").Append('\n');

    void Line(string kind, string value)
    {
        sb.Append(kind).Append(' ').Append(value).Append('\n');
    }

    var m1 = new MersenneTwister(seed);
    for (var i = 0; i < 1000; i++)
        Line("u32", m1.NextUint().ToString("x8"));

    var m2 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("next", m2.Next().ToString(CultureInfo.InvariantCulture));

    var m3 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("f32", BitConverter.SingleToUInt32Bits(m3.NextFloat()).ToString("x8"));

    var m4 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("u64", m4.NextUlong().ToString("x16"));

    var m5 = new MersenneTwister(seed);
    for (var i = 0; i < 200; i++)
        Line("range", m5.Next(2, 10).ToString(CultureInfo.InvariantCulture));

    var m5n = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("rangen", m5n.Next(-5, 5).ToString(CultureInfo.InvariantCulture));

    var m6 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("pw", m6.PickWeighted([1, 3, 6]).ToString(CultureInfo.InvariantCulture));

    var m6z = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
        Line("pw0", m6z.PickWeighted([0, 0, 0]).ToString(CultureInfo.InvariantCulture));

    var m7 = new MersenneTwister(seed);
    for (var i = 0; i < 100; i++)
    {
        var span = new int[] { 10, 11, 12, 13, 14 };
        m7.ShuffleInPlace(span.AsSpan(), 0, span.Length);
        Line("shuffle", string.Join(' ', span));
    }

    var m8 = new MersenneTwister(seed);
    for (var i = 0; i < 623; i++)
        m8.NextUint();
    Line("u32_624", $"{m8.NextUint():x8} {m8.NextUint():x8} {m8.NextUint():x8}");

    // 与 T03 落盘编码一致：UTF-8 无 BOM，LF 行尾
    return new UTF8Encoding(false).GetBytes(sb.ToString());
}
}
