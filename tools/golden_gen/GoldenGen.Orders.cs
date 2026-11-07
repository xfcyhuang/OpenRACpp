// FormatterServices 仅为跳过 Actor ctor 伪造订单黄金（U029 配方），无序列化用途
#pragma warning disable SYSLIB0050

internal static partial class Program
{
// —— orders 模式（T08/issue #9）：宿主内直接构造 Order → Serialize() dump 字节。
//    写侧与 World 解耦（OrderTest.cs 蓝本 Tier A + 反射伪 actor/cell Target 的 Tier B）。
//    伪 actor 配方：FormatterServices.GetUninitializedObject(Actor) + 反射设 ActorID
//    （public readonly 字段，Actor.cs:53）与 Generation（public 字段，Actor.cs:68）——
//    Target.FromActor/Order ctor/Serialize 都只读这两个成员。
//    伪 cell/multi-pos Target：boxed 反射直填私有字段（type/cell/subCell/
//    terrainCenterPosition/terrainPositions），TargetType 是 internal enum 故经
//    Enum.ToObject(FieldType) 赋值。序列化只读 SerializableState 暴露的这些字段。
static byte[] DumpOrders()
{
    var sb = new StringBuilder();
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 OpenRA.Game/Network/Order.cs").Append('\n');
    sb.Append("# case <name> / hex <lowercase hex of Order.Serialize()> / fields <key=value ...>").Append('\n');

    void Case(string name, Order o)
    {
        try
        {
            var bytes = o.Serialize();
            sb.Append("case ").Append(name).Append('\n');
            sb.Append("hex ").Append(Convert.ToHexString(bytes).ToLowerInvariant()).Append('\n');
            sb.Append("fields ").Append(DescribeOrder(o, bytes)).Append('\n');
        }
        catch (Exception e)
        {
            throw new InvalidOperationException($"DumpOrders case '{name}' failed: {e}", e);
        }
    }

    // —— Tier A：worldless 往返面（上游 OrderTest.cs 六用例 + 扩展） ——
    Case("empty", new Order());
    Case("unqueued", new Order("Test", null, false));
    Case("queued", new Order("Test", null, true));
    Case("pos-target", new Order("Test", null, Target.FromPos(new WPos(int.MinValue, 0, int.MaxValue)), false));
    Case("invalid-target", new Order("Test", null, Target.Invalid, false));
    Case("extra-fields", new Order("Test", null, Target.Invalid, true)
    {
        TargetString = "TargetString",
        ExtraLocation = new CPos(2047, 2047, 128),
        ExtraData = uint.MaxValue,
        IsImmediate = true,
    });
    Case("target-string-empty", new Order("Test", null, false) { TargetString = "" });
    Case("utf8-strings", new Order("Тест-日本語", null, false) { TargetString = "τ=2π" });
    var handshakeOrder = new Order("Test", null, false);
    handshakeOrder.Type = OrderType.Handshake;
    handshakeOrder.TargetString = "payload";
    Case("handshake", handshakeOrder);

    var handshakeEmptyOrder = new Order("Test", null, false);
    handshakeEmptyOrder.Type = OrderType.Handshake;
    handshakeEmptyOrder.TargetString = "";
    Case("handshake-empty", handshakeEmptyOrder);
    Case("terrain-multi-pos", new Order("Attack", null, MakeTerrainPositionsTarget(
        new WPos(100, 200, 0), [new WPos(100, 200, 0), new WPos(300, 400, 0)]), false));
    Case("extra-actors-empty", new Order("Test", null, false) { ExtraActors = [] });
    Case("grouped-empty", new Order("CreateGroup", null, false, groupedActors: []));

    // —— Tier B：伪 actor / 伪 cell 目标（沙盒最小订单集，见调研 §3） ——
    var actorSubject = MakeFakeActor(7, 3);
    var actorAttacker = MakeFakeActor(8, 1);
    var actorFactory = MakeFakeActor(2, 0);
    var actorPlayer = MakeFakeActor(1, 0);
    var actorStance = MakeFakeActor(6, 0);
    var actorMoveTarget = MakeFakeActor(9, -2);

    Case("move-cell", new Order("Move", actorSubject, MakeCellTarget(new CPos(10, 5), (SubCell)2), true));
    Case("actor-target", new Order("Attack", actorAttacker, Target.FromActor(actorMoveTarget), true));
    Case("start-production", Order.StartProduction(actorFactory, "e1", 5));
    Case("pause-production", Order.PauseProduction(actorFactory, "e1", true));
    Case("cancel-production", Order.CancelProduction(actorFactory, "e1", 3));
    Case("place-building", new Order("PlaceBuilding", actorPlayer, MakeCellTarget(new CPos(20, 10), SubCell.FullCell), false)
    {
        TargetString = "fact",
        ExtraData = 42,
        ExtraLocation = new CPos(0, 1),
    });
    Case("line-build", new Order("LineBuild", actorPlayer, MakeCellTarget(new CPos(30, 15), SubCell.FullCell), false)
    {
        TargetString = "pbox",
        ExtraData = 43,
        ExtraLocation = new CPos(1, 0),
    });
    Case("create-group", new Order("CreateGroup", actorPlayer, false, groupedActors:
        [MakeFakeActor(3, 0), MakeFakeActor(4, 0), MakeFakeActor(5, 0)]));
    Case("set-unit-stance", new Order("SetUnitStance", actorStance, false) { ExtraData = 2 });
    Case("extra-actors", new Order("Test", null, false)
    {
        ExtraActors = [MakeFakeActor(3, 0), MakeFakeActor(4, 0)],
    });
    Case("chat", Order.Chat("Hello world"));
    Case("chat-team", Order.Chat("team only", 2));
    Case("chat-spectators", Order.Chat("spec", uint.MaxValue));
    Case("command", Order.Command("/help"));
    Case("pause-game", Order.FromTargetString("PauseGame", "Pause", false));

    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

static Actor MakeFakeActor(uint id, int generation)
{
    var actor = (Actor)FormatterServices.GetUninitializedObject(typeof(Actor));
    typeof(Actor).GetField(nameof(Actor.ActorID))!.SetValue(actor, id);
    typeof(Actor).GetField(nameof(Actor.Generation))!.SetValue(actor, generation);
    return actor;
}

static Target MakeCellTarget(CPos cell, SubCell subCell)
{
    var boxed = (object)Target.Invalid;
    const BindingFlags Flags = BindingFlags.Instance | BindingFlags.NonPublic;
    var typeField = typeof(Target).GetField("type", Flags)!;
    typeField.SetValue(boxed, Enum.ToObject(typeField.FieldType, 2));
    typeof(Target).GetField("cell", Flags)!.SetValue(boxed, (CPos?)cell);
    typeof(Target).GetField("subCell", Flags)!.SetValue(boxed, (SubCell?)subCell);
    var pos = WPos.Zero;
    typeof(Target).GetField("terrainCenterPosition", Flags)!.SetValue(boxed, pos);
    typeof(Target).GetField("terrainPositions", Flags)!.SetValue(boxed, new[] { pos });
    return (Target)boxed;
}

static Target MakeTerrainPositionsTarget(WPos center, WPos[] positions)
{
    var boxed = (object)Target.Invalid;
    const BindingFlags Flags = BindingFlags.Instance | BindingFlags.NonPublic;
    var typeField = typeof(Target).GetField("type", Flags)!;
    typeField.SetValue(boxed, Enum.ToObject(typeField.FieldType, 2));
    typeof(Target).GetField("terrainCenterPosition", Flags)!.SetValue(boxed, center);
    typeof(Target).GetField("terrainPositions", Flags)!.SetValue(boxed, positions);
    return (Target)boxed;
}

static string DescribeOrder(Order o, byte[] bytes)
{
    string flagsPart;
    if (bytes[0] == 0xFF)
    {
        var p = 1;
        var len = 0;
        var shift = 0;
        while (true)
        {
            var b = bytes[p++];
            len |= (b & 0x7f) << shift;
            shift += 7;
            if ((b & 0x80) == 0) break;
        }

        p += len;
        var flags = BitConverter.ToInt16(bytes, p);
        flagsPart = $"flags=0x{flags:x4}";
    }
    else
        flagsPart = "flags=handshake";

    var subject = o.Subject != null
        ? o.Subject.ActorID.ToString(CultureInfo.InvariantCulture) : "-";
    var text = o.TargetString == null ? "-" : $"{o.TargetString.Length}:{o.TargetString}";
    var loc = o.ExtraLocation.Bits.ToString(CultureInfo.InvariantCulture);
    var ea = o.ExtraActors == null ? "-" : o.ExtraActors.Length.ToString(CultureInfo.InvariantCulture);
    var grouped = o.GroupedActors == null ? "-" : o.GroupedActors.Length.ToString(CultureInfo.InvariantCulture);
    var targetKind = ((byte)o.Target.Type).ToString(CultureInfo.InvariantCulture);

    return $"type={(o.Type == OrderType.Handshake ? "handshake" : "fields")} orderlen={o.OrderString.Length} " +
        $"{flagsPart} subject={subject} queued={(o.Queued ? 1 : 0)} targetkind={targetKind} " +
        $"text={text} data={o.ExtraData.ToString(CultureInfo.InvariantCulture)} loc={loc} " +
        $"ea={ea} grouped={grouped}";
}
}
