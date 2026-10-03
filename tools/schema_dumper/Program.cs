// schema_dumper — 上游 C# 反射 → OpenRACpp 生成代码/黄金数据(非交付运行时依赖)
//
// 三个子命令(PORTING_PLAN.md §Phase 2):
//   --scan                     普查加载链类型(TraitInfo/IWarhead/IProjectileInfo)的
//                              字段类型全集、LoadUsing/Require 使用点、RulesetLoaded 实现集
//   --gen <输出目录> <仓库根>   导出 gen/ 描述表(C++26 constexpr 源码)+ enum 定义表
//   --dump <mod> [<mod>...]    深度解析黄金数据:加载 mod 默认规则集,逐 actor/weapon
//                              用 FieldSaver 语义 dump(与 C++ 侧 tests/golden_rules 重放对拍)
//
// 本工具运行在 .NET 10 上,引用上游构建产物 bin/*.dll(基线 commit 7d57605bca)。

using System.Reflection;

// ————————————————————————————————————————————————
// 装载上游程序集
// ————————————————————————————————————————————————

var repoRoot = args.Length > 0 && args[0] == "--gen" && args.Length > 2
    ? Path.GetFullPath(args[2])
    : FindRepoRoot();

var assemblies = new List<Assembly>
{
    typeof(OpenRA.Game).Assembly,
    typeof(OpenRA.Mods.Common.Traits.HealthInfo).Assembly,
    typeof(OpenRA.Mods.Cnc.Traits.HarvesterHuskModifierInfo).Assembly,
    typeof(OpenRA.Mods.D2k.Traits.AttractsWormsInfo).Assembly,
};

string FindRepoRoot()
{
    return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "OpenRA"));
}

if (args.Length == 0 || args[0] == "--scan")
{
    var fieldTypes = new SortedDictionary<string, int>();
    var loadUsing = new List<string>();
    var requiredFields = new List<string>();
    var ignoredFields = new List<string>();
    var rulesetLoaded = new List<string>();
    var typeCount = 0;

    foreach (var (t, cat) in LoadableTypes(assemblies))
    {
        typeCount++;
        if (t.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
            .Any(m => m.Name == "RulesetLoaded" && m.DeclaringType == t))
            rulesetLoaded.Add(TypeSignature(t));

        foreach (var f in LoadableFields(t))
        {
            var (serialize, required, loader) = SerializeInfo(f);
            if (!serialize)
            {
                ignoredFields.Add($"{TypeSignature(t)}.{f.Name}");
                continue;
            }

            fieldTypes.TryGetValue(TypeSignature(f.FieldType), out var n);
            fieldTypes[TypeSignature(f.FieldType)] = n + 1;

            if (required)
                requiredFields.Add($"{TypeSignature(t)}.{f.Name}");
            if (loader != null)
                loadUsing.Add($"{TypeSignature(t)}.{f.Name} -> {loader}");
        }
    }

    Console.WriteLine($"=== 可加载类型总数: {typeCount} (Trait/Warhead/Projectile) ===");
    Console.WriteLine($"\n=== 字段类型全集({fieldTypes.Count} 种) ===");
    foreach (var (sig, count) in fieldTypes)
        Console.WriteLine($"  {count,5}  {sig}");
    Console.WriteLine($"\n=== LoadUsing 使用点({loadUsing.Count}) ===");
    foreach (var s in loadUsing)
        Console.WriteLine($"  {s}");
    Console.WriteLine($"\n=== Require 使用点({requiredFields.Count}) ===");
    foreach (var s in requiredFields)
        Console.WriteLine($"  {s}");
    Console.WriteLine($"\n=== Ignore 字段({ignoredFields.Count}) ===");
    foreach (var s in ignoredFields)
        Console.WriteLine($"  {s}");
    Console.WriteLine($"\n=== RulesetLoaded 自定义实现({rulesetLoaded.Count}) ===");
    foreach (var s in rulesetLoaded)
        Console.WriteLine($"  {s}");
    return 0;
}

if (args[0] == "--gen")
{
    Console.WriteLine("--gen: not yet implemented (Phase 2 后续步骤)");
    return 1;
}

if (args[0] == "--dump")
{
    Console.WriteLine("--dump: not yet implemented (Phase 2 后续步骤)");
    return 1;
}

Console.WriteLine("用法: schema_dumper [--scan | --gen <输出目录> <仓库根> | --dump <mod>...]");
return 1;

// ————————————————————————————————————————————————
// 辅助(局部函数)
// ————————————————————————————————————————————————

// 与 FieldLoader.BuildTypeLoadInfo 相同的字段收集 BindingFlags
const BindingFlags FieldFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;

IEnumerable<FieldInfo> LoadableFields(Type t) => t.GetFields(FieldFlags);

IEnumerable<(Type Type, Category Cat)> LoadableTypes(List<Assembly> asms)
{
    var seen = new HashSet<Type>();
    foreach (var asm in asms)
    {
        foreach (var t in asm.GetTypes())
        {
            if (t.IsAbstract || !t.IsClass)
                continue;

            Category? cat = null;
            if (typeof(OpenRA.Traits.TraitInfo).IsAssignableFrom(t))
                cat = Category.Trait;
            else if (typeof(OpenRA.Traits.IWarhead).IsAssignableFrom(t))
                cat = Category.Warhead;
            else if (typeof(OpenRA.GameRules.IProjectileInfo).IsAssignableFrom(t))
                cat = Category.Projectile;

            if (cat != null && seen.Add(t))
                yield return (t, cat.Value);
        }
    }
}

// SerializeAttribute 携带的信息(内部类无法直接引用,反射读属性)
(bool Serialize, bool Required, string Loader) SerializeInfo(FieldInfo f)
{
    foreach (var a in f.GetCustomAttributes(false))
    {
        var at = a.GetType();
        if (at.Name == "SerializeAttribute" || at.Name == "RequireAttribute"
            || at.Name == "LoadUsingAttribute" || at.Name == "IgnoreAttribute")
        {
            var serialize = (bool)at.GetField("Serialize", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(a);
            var required = (bool)at.GetField("Required", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(a);
            var loader = (string)at.GetField("Loader", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(a);
            return (serialize, required, loader);
        }
    }

    return (true, false, null);
}

string TypeSignature(Type t)
{
    if (t.IsArray)
        return TypeSignature(t.GetElementType()!) + "[]";

    if (t.IsGenericType)
    {
        var def = t.GetGenericTypeDefinition();
        var args = string.Join(",", t.GetGenericArguments().Select(TypeSignature));
        var name = def.FullName!.Split('`')[0];
        return $"{name}<{args}>";
    }

    return t.FullName ?? t.Name;
}

// 参与描述表的类型类别
enum Category { Trait, Warhead, Projectile }
