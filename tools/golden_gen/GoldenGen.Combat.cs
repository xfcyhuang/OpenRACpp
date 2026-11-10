internal static partial class Program
{
static string[] CombatWeaponManifest(string openraRoot)
{
    var fs = new GoldenModRootFs(openraRoot);
    var nodes = MiniYaml.FromStream(fs.Open("ra|mod.yaml"), "ra|mod.yaml").ToList();
    var yaml = new MiniYaml(null, MiniYaml.Merge([nodes])).ToDictionary();
    return yaml["Weapons"].Nodes.Select(n => n.Key).ToArray();
}

static byte[] DumpCombatWeaponsManifest(string openraRoot) =>
    new UTF8Encoding(false).GetBytes(string.Join("\n", CombatWeaponManifest(openraRoot)) + "\n");

static IEnumerable<(string RelName, string FullPath)> CombatWeaponFiles(string openraRoot)
{
    var root = Path.GetFullPath(Path.Combine(openraRoot, "mods"));
    foreach (var entry in CombatWeaponManifest(openraRoot))
    {
        // "ra|weapons/explosions.yaml" → 物理路径（与 tests/golden/rules 同款 '|' 命名入库）
        var rel = entry.Replace('|', Path.DirectorySeparatorChar);
        var full = Path.GetFullPath(Path.Combine(root, rel));
        yield return (entry.Replace('|', '_'), full);
    }
}

static byte[] RunCombatPass(string openraRoot)
{
    HeadlessHost.Boot(openraRoot, soundChannel: true);

    var (modData, package) = State.CombatAsm is { } cached ? cached : AssembleCombat(openraRoot);

    var sb = new StringBuilder();
    sb.Append("# baseline 1f594e32065615ed1847962c80ecfe6e868a11c3 combat duels, ore-lord/temperat, minimal ruleset + real ra weapons").Append('\n');

    // ---- 真实 ra 武器表（一次装配缓存，逐 pass 复用） ----
    var weapons = State.CombatWeapons!;

    sb.Append("weapons loaded ").Append(weapons.Count).Append('\n');
    sb.Append("weapon m1carbine range ").Append(weapons["m1carbine"].Range.Length)
      .Append(" reload ").Append(weapons["m1carbine"].ReloadDelay)
      .Append(" burst ").Append(weapons["m1carbine"].Burst).Append('\n');
    sb.Append("weapon 25mm range ").Append(weapons["25mm"].Range.Length)
      .Append(" reload ").Append(weapons["25mm"].ReloadDelay).Append('\n');
    sb.Append("weapon 155mm range ").Append(weapons["155mm"].Range.Length)
      .Append(" reload ").Append(weapons["155mm"].ReloadDelay)
      .Append(" minrange ").Append(weapons["155mm"].MinRange.Length).Append('\n');

    foreach (var wk in new[] { "m1carbine", "25mm", "155mm" })
    {
        var w = weapons[wk];
        sb.Append("warheads ").Append(wk).Append(" projectile ").Append(w.Projectile?.GetType().Name ?? "none").Append('\n');
        foreach (var wh in w.Warheads)
        {
            var wt2 = wh.GetType();
            var delayF = wt2.GetField("Delay");
            var inaccF = wt2.GetField("Inaccuracy");
            var dmgF = wt2.GetField("Damage");
            var vsF = wt2.GetField("Versus");
            sb.Append("  wh ").Append(wt2.Name)
              .Append(" delay ").Append(delayF?.GetValue(wh) ?? "-")
              .Append(" inacc ").Append(inaccF != null ? ((WDist)inaccF.GetValue(wh)!).Length : -1)
              .Append(" dmg ").Append(dmgF?.GetValue(wh) ?? "-");
            if (vsF?.GetValue(wh) is System.Collections.IDictionary vs && vs.Count > 0)
            {
                var parts = new List<string>();
                foreach (System.Collections.DictionaryEntry kv in vs)
                    parts.Add($"{kv.Key}:{kv.Value}");
                sb.Append(" versus ").Append(string.Join(",", parts));
            }

            sb.Append('\n');
        }
    }

    var asm = State.CombatReflection is { } asmRef ? asmRef : throw new InvalidOperationException("combat reflection not assembled");
    RunCombatScenario(sb, modData, package, weapons, State.CombatActors!, "duelA-instant-mutual",
        "rifleman", new CPos(30, 30), "rifleman", new CPos(30, 33), mutual: true, ticks: 90, asm);
    RunCombatScenario(sb, modData, package, weapons, State.CombatActors!, "duelB-bullet-turret",
        "tank", new CPos(30, 30), "rifleman", new CPos(30, 33), mutual: false, ticks: 220, asm);
    RunCombatScenario(sb, modData, package, weapons, State.CombatActors!, "duelC-arc-spread",
        "arty", new CPos(30, 30), "rifleman", new CPos(30, 38), mutual: false, ticks: 280, asm);
    RunCombatScenario(sb, modData, package, weapons, State.CombatActors!, "duelD-approach",
        "rifleman", new CPos(20, 20), "rifleman", new CPos(20, 26), mutual: false, ticks: 600, asm);

    return new UTF8Encoding(false).GetBytes(sb.ToString());
}

// combat 模式一次性装配（ModData/ore-lord 包/武器表/最小规则 actor 集 + 反射句柄包）
static (ModData, IReadOnlyPackage) AssembleCombat(string openraRoot)
{
    var mods = new InstalledMods(new[] { Path.Combine(Platform.EngineDir, "mods") }, Array.Empty<string>());
    var modData = new ModData(mods["ra"], mods);
    Game.ModData = modData;
    ObjectCreator.MissingTypeAction = s => Console.WriteLine($"!! MissingType: {s}");
    FieldLoader.UnknownFieldAction = f => Console.WriteLine($"!! UnknownField: {f}");
    Game.Sound = new Sound(new StubPlatform(), new SoundSettings());
    Game.Sound.DisableAllSounds = true;
    var mapsDir = new Folder(Path.Combine(Platform.EngineDir, "mods", "ra", "maps"));
    var package = mapsDir.OpenPackage("ore-lord.oramap", modData.ModFiles)
        ?? throw new InvalidOperationException("cannot open ore-lord.oramap");

    // 真实武器表：manifest 全文件 + 继承解析（与 Ruleset.Load 同路径）
    var weaponNodes = MiniYaml.Load(modData.ModFiles, modData.Manifest.Weapons, null);
    var weapons = new Dictionary<string, OpenRA.GameRules.WeaponInfo>();
    foreach (var n in weaponNodes)
        weapons[n.Key.ToLowerInvariant()] = new OpenRA.GameRules.WeaponInfo(n.Value);
    State.CombatWeapons = weapons;

    // 最小合成规则集（t11 基础 + 战斗 trait；t12-combat.md §4.1 配方照抄）
    const string RulesYaml = """
        world:
            ActorMap:
            ScreenMap:
            Selection:
            ControlGroups:
            Faction@allies:
                Name: Allies
                InternalName: allies
                Side: Allies
            Locomotor@TRACKED:
                Name: tracked
                TerrainSpeeds:
                    Clear: 100
                    Rough: 88
                    Road: 125
                    Bridge: 125
                    Ore: 88
                    Gems: 88
                    Beach: 88
            Locomotor@FOOT:
                Name: foot
                SharesCell: True
                TerrainSpeeds:
                    Clear: 100
                    Rough: 89
                    Road: 111
                    Bridge: 111
                    Ore: 89
                    Gems: 89
                    Beach: 89
            PathFinder:
            SmudgeLayer@CRATER:
                Type: Crater
                Sequence: craters
        player:
            Shroud:
        rifleman:
            Health:
                HP: 5000
            Armor:
                Type: None
            HitShape:
                Type: Circle
                    Radius: 128
            BodyOrientation:
                QuantizedFacings: 8
            Mobile:
                Locomotor: foot
                Speed: 54
                TurnSpeed: 512
                AlwaysTurnInPlace: True
            Targetable:
                TargetTypes: GroundActor, Infantry
            AttackFrontal:
                FacingTolerance: 0
            Armament:
                Weapon: M1Carbine
        tank:
            Health:
                HP: 23000
            Armor:
                Type: Heavy
            HitShape:
                Type: Circle
                    Radius: 426
            BodyOrientation:
                QuantizedFacings: 32
            Mobile:
                Locomotor: tracked
                Speed: 113
                TurnSpeed: 512
            Targetable:
                TargetTypes: GroundActor, Vehicle
            Turreted:
                TurnSpeed: 28
            AttackTurreted:
            Armament:
                Weapon: 25mm
                Recoil: 85
                RecoilRecovery: 25
                LocalOffset: 768,0,90
        arty:
            Health:
                HP: 10000
            Armor:
                Type: Light
            HitShape:
                Type: Circle
                    Radius: 426
            BodyOrientation:
                QuantizedFacings: 32
            Mobile:
                Locomotor: tracked
                Speed: 72
                TurnSpeed: 8
            Targetable:
                TargetTypes: GroundActor, Vehicle
            AttackFrontal:
                FacingTolerance: 0
            Armament:
                Weapon: 155mm
                LocalOffset: 624,0,208
        """;

    var nodes = MiniYaml.FromString(RulesYaml, "t12-minimal-rules");
    var actors = new Dictionary<string, ActorInfo>(StringComparer.OrdinalIgnoreCase);
    foreach (var n in nodes)
        actors[n.Key.ToLowerInvariant()] = new ActorInfo(modData.ObjectCreator, n.Key.ToLowerInvariant(), n.Value);
    State.CombatActors = actors;

    OpenRA.Widgets.ChromeMetrics.Initialize(modData);
    modData.MapCache.LoadMaps(modData);

    // 反射句柄包（Mods.Common 类型经 ObjectCreator 取，t06 ALC 禁忌）
    var attackBaseType = modData.ObjectCreator.FindType("AttackBase")!;
    var attackFrontalType = modData.ObjectCreator.FindType("AttackFrontal")!;
    var attackTurretedType = modData.ObjectCreator.FindType("AttackTurreted")!;
    var attackSourceType = modData.ObjectCreator.FindType("AttackSource")!;
    var healthType = modData.ObjectCreator.FindType("Health")!;
    var mobileType = modData.ObjectCreator.FindType("Mobile")!;
    var facingInitType = modData.ObjectCreator.FindType("FacingInit")!;
    State.CombatReflection = (
        attackBaseType.GetMethod("AttackTarget")!,
        attackFrontalType, attackTurretedType,
        typeof(Actor).GetMethods().First(m => m.Name == "Trait" && m.IsGenericMethodDefinition && m.GetParameters().Length == 0),
        healthType.GetProperty("HP")!, healthType.GetProperty("MaxHP")!,
        mobileType.GetProperty("FromCell")!, mobileType.GetProperty("Facing")!,
        mobileType.GetProperty("CenterPosition")!,
        Enum.Parse(attackSourceType, "Default"), facingInitType);

    State.CombatAsm = (modData, package);
    return (modData, package);
}

static void RunCombatScenario(
    StringBuilder sb, ModData modData, IReadOnlyPackage package,
    Dictionary<string, OpenRA.GameRules.WeaponInfo> weapons,
    Dictionary<string, ActorInfo> actors, string name,
    string typeA, CPos cellA, string typeB, CPos cellB, bool mutual, int ticks,
    (MethodInfo AttackTarget, Type AttackFrontal, Type AttackTurreted, MethodInfo TraitGeneric,
     PropertyInfo Hp, PropertyInfo MaxHp, PropertyInfo FromCell, PropertyInfo Facing,
     PropertyInfo Center, object DefaultSource, Type FacingInit) asm)
{
    sb.Append("scenario ").Append(name).Append('\n');
    try
    {
        var map = new Map(modData, package);

        // 无头障碍 9：Sequences.images 换假序列（保留键集；Length=1/Tick=40 自删）
        var imagesField = typeof(OpenRA.Graphics.SequenceSet).GetField("images", BindingFlags.NonPublic | BindingFlags.Instance)!;
        var origImages = (System.Collections.IEnumerable)imagesField.GetValue(map.Sequences)!;
        var fakeImages = new Dictionary<string, IReadOnlyDictionary<string, OpenRA.Graphics.ISpriteSequence>>();
        foreach (var imgEntry in origImages.Cast<dynamic>())
        {
            var inner = new Dictionary<string, OpenRA.Graphics.ISpriteSequence>();
            foreach (var seqEntry in ((System.Collections.IEnumerable)imgEntry.Value).Cast<dynamic>())
                inner[(string)seqEntry.Key] = new FakeSequence((string)seqEntry.Key);
            fakeImages[(string)imgEntry.Key] = inner;
        }

        imagesField.SetValue(map.Sequences, fakeImages);

        var mobileType = modData.ObjectCreator.FindType("Mobile")!;
        object TraitOf(Actor actor, Type type) => asm.TraitGeneric.MakeGenericMethod(type).Invoke(actor, null)!;
        var emptySounds = new Dictionary<string, OpenRA.GameRules.SoundInfo>();
        var emptyMusic = new Dictionary<string, OpenRA.GameRules.MusicInfo>();
        var emptyModelSeq = new Dictionary<string, MiniYamlNode>();
        var ruleset = new Ruleset(actors, weapons, emptySounds, emptySounds, emptyMusic, map.Rules.TerrainInfo, emptyModelSeq);
        typeof(Map).GetProperty("Rules", BindingFlags.Public | BindingFlags.Instance)!.SetValue(map, ruleset);

        var om = new OrderManager(new EchoConnection());
        var worldCtor = typeof(World).GetConstructor(BindingFlags.NonPublic | BindingFlags.Instance, null,
            [typeof(Map), typeof(ModData), typeof(OrderManager), typeof(WorldType)], null)!;
        var world = (World)worldCtor.Invoke([map, modData, om, WorldType.Regular]);
        om.World = world;

        // 双玩家 + 敌对掩码（障碍 14：默认 Neutral 不开火）
        var pr0 = new PlayerReference { Name = "Multi0", Faction = "allies", OwnsWorld = true };
        var pr1 = new PlayerReference { Name = "Multi1", Faction = "allies" };
        var p0 = new Player(world, null, pr0, new MersenneTwister(0));
        var p1 = new Player(world, null, pr1, new MersenneTwister(0));
        world.SetPlayers([p0, p1], p0);
        p0.EnemyPlayersMask = p0.EnemyPlayersMask.Union(p1.PlayerMask);
        p1.EnemyPlayersMask = p1.EnemyPlayersMask.Union(p0.PlayerMask);

        // LoadComplete 等价子集：跳过 SmudgeLayer（障碍 13，WorldLoaded 读 wr 必炸）
        foreach (var iwl in world.WorldActor.TraitsImplementing<IWorldLoaded>())
        {
            if (iwl.GetType().Name == "SmudgeLayer")
                continue;
            iwl.WorldLoaded(world, null);
        }

        Actor Spawn(string type, CPos cell, Player owner)
        {
            var r = new ActorReference(type);
            r.Add(new LocationInit(cell));
            r.Add(new OwnerInit(owner));
            r.Add((ActorInit)Activator.CreateInstance(asm.FacingInit, new object[] { new WAngle(0) })!);
            return world.CreateActor(true, r);
        }

        var a = Spawn(typeA, cellA, p0);
        var b = Spawn(typeB, cellB, p1);
        sb.Append("spawn a id ").Append(a.ActorID).Append(" type ").Append(typeA)
          .Append(" maxhp ").Append(asm.MaxHp.GetValue(TraitOf(a, modData.ObjectCreator.FindType("Health")!))).Append('\n');
        sb.Append("spawn b id ").Append(b.ActorID).Append(" type ").Append(typeB)
          .Append(" maxhp ").Append(asm.MaxHp.GetValue(TraitOf(b, modData.ObjectCreator.FindType("Health")!))).Append('\n');

        // 开火指令：Attack 订单等价（AttackBase.AttackTarget，queued=false allowMove=true forceAttack=false）
        var attackTraitA = asm.TraitGeneric.MakeGenericMethod(typeA == "tank" ? asm.AttackTurreted : asm.AttackFrontal).Invoke(a, null)!;
        asm.AttackTarget.Invoke(attackTraitA, [Target.FromActor(b), asm.DefaultSource, false, true, false, null]);
        if (mutual)
        {
            var attackTraitB = asm.TraitGeneric.MakeGenericMethod(asm.AttackFrontal).Invoke(b, null)!;
            asm.AttackTarget.Invoke(attackTraitB, [Target.FromActor(a), asm.DefaultSource, false, true, false, null]);
        }

        // 逐 tick dump（prevHp 预置 maxhp，消 t1 假 hit 行）
        var maxA = (int)asm.MaxHp.GetValue(TraitOf(a, modData.ObjectCreator.FindType("Health")!))!;
        var maxB = (int)asm.MaxHp.GetValue(TraitOf(b, modData.ObjectCreator.FindType("Health")!))!;
        var prevHpA = maxA;
        var prevHpB = maxB;
        var prevInWorldA = true;
        var prevInWorldB = true;
        (CPos Cell, int Face, WPos Pos) lastA = (cellA, 0, map.CenterOfCell(cellA));
        (CPos Cell, int Face, WPos Pos) lastB = (cellB, 0, map.CenterOfCell(cellB));
        for (var t = 0; t < ticks; t++)
        {
            world.Tick();

            // 无头障碍 10：帧尾 SpriteEffect 在下一 tick 前移除（RNG 消耗在创建之前，不受影响）
            foreach (var e in world.Effects.ToList())
                if (e.GetType().Name == "SpriteEffect")
                    world.Remove(e);

            var wt = world.WorldTick;

            var hpA = a.IsInWorld ? (int)asm.Hp.GetValue(TraitOf(a, modData.ObjectCreator.FindType("Health")!))! : 0;
            var hpB = b.IsInWorld ? (int)asm.Hp.GetValue(TraitOf(b, modData.ObjectCreator.FindType("Health")!))! : 0;

            if (hpA != prevHpA)
            {
                sb.Append(CultureInfo.InvariantCulture, $"hit tick {wt} victim a hp {prevHpA}->{hpA}\n");
                prevHpA = hpA;
            }

            if (hpB != prevHpB)
            {
                sb.Append(CultureInfo.InvariantCulture, $"hit tick {wt} victim b hp {prevHpB}->{hpB}\n");
                prevHpB = hpB;
            }

            if (hpA == 0 && prevInWorldA)
            {
                sb.Append(CultureInfo.InvariantCulture, $"death tick {wt} actor a\n");
                prevInWorldA = false;
            }

            if (hpB == 0 && prevInWorldB)
            {
                sb.Append(CultureInfo.InvariantCulture, $"death tick {wt} actor b\n");
                prevInWorldB = false;
            }

            if (a.IsInWorld != prevInWorldA)
            {
                sb.Append(CultureInfo.InvariantCulture, $"removed tick {wt} actor a\n");
                prevInWorldA = a.IsInWorld;
            }

            if (b.IsInWorld != prevInWorldB)
            {
                sb.Append(CultureInfo.InvariantCulture, $"removed tick {wt} actor b\n");
                prevInWorldB = b.IsInWorld;
            }

            // 状态行（actor 已 Dispose 后 trait 查询会炸 → 缓存最后已知值）
            if (a.IsInWorld)
                lastA = ((CPos)asm.FromCell.GetValue(TraitOf(a, mobileType))!,
                    ((WAngle)asm.Facing.GetValue(TraitOf(a, mobileType))!).Angle,
                    (WPos)asm.Center.GetValue(TraitOf(a, mobileType))!);

            if (b.IsInWorld)
                lastB = ((CPos)asm.FromCell.GetValue(TraitOf(b, mobileType))!,
                    ((WAngle)asm.Facing.GetValue(TraitOf(b, mobileType))!).Angle,
                    (WPos)asm.Center.GetValue(TraitOf(b, mobileType))!);

            var (fromA, faceA, posA) = lastA;
            var (fromB, faceB, posB) = lastB;

            var effects = world.Effects.ToList();
            var effNames = string.Join(",", effects.Select(e => e.GetType().Name));

            sb.Append(CultureInfo.InvariantCulture,
                $"tick {wt} a hp {hpA} face {faceA} cell {fromA.X},{fromA.Y} pos {posA.X},{posA.Y},{posA.Z} inworld {a.IsInWorld} | " +
                $"b hp {hpB} face {faceB} cell {fromB.X},{fromB.Y} pos {posB.X},{posB.Y},{posB.Z} inworld {b.IsInWorld} | " +
                $"eff {effects.Count}[{effNames}] rng {world.SharedRandom.Last} cnt {world.SharedRandom.TotalCount} sync {world.SyncHash()}\n");

            foreach (var e in effects)
            {
                var et = e.GetType();
                if (et.Name != "Bullet")
                    continue;
                var fPos = et.GetField("pos", BindingFlags.NonPublic | BindingFlags.Instance)!;
                var fTarget = et.GetField("target", BindingFlags.NonPublic | BindingFlags.Instance)!;
                var fSource = et.GetField("source", BindingFlags.NonPublic | BindingFlags.Instance)!;
                var bp = (WPos)fPos.GetValue(e)!;
                var bt = (WPos)fTarget.GetValue(e)!;
                var bs = (WPos)fSource.GetValue(e)!;
                sb.Append(CultureInfo.InvariantCulture, $"proj tick {wt} pos {bp.X},{bp.Y},{bp.Z} src {bs.X},{bs.Y},{bs.Z} tgt {bt.X},{bt.Y},{bt.Z}\n");
            }

            var effectsNow = world.Effects.ToList();
            if (!a.IsInWorld && !b.IsInWorld && effectsNow.Count == 0)
                break;
        }
    }
    catch (Exception ex)
    {
        sb.Append("SCENARIO FAILED: ").Append(ex.ToString()).Append('\n');
    }
}
}

internal static partial class Program
{
    internal static (string RelPath, byte[] Bytes)[] CombatGroup(string openraRoot)
    {
        return [("tests/golden/combat/duels.txt", RunCombatPass(openraRoot))];
    }
}
