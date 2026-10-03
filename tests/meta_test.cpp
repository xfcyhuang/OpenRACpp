// UPSTREAM: OpenRA.Game/FieldLoader.cs + Support/VariableExpression.cs + Exts.cs
//          的语义单测(PORTING_PLAN §Phase 2;断言文本与 C# 逐字)
//          Semantics unit tests for the Phase 2 metadata framework (assertion
//          texts verbatim against C#).
import std;
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"
#include "meta/type_registry.hpp"
#include "meta/variable_expression.hpp"
#include "core/bitset.hpp"
#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wvec.hpp"
#include "yaml/mini_yaml.hpp"

using namespace ora;
using namespace ora::meta;

// ————————————————————————————————————————————————
// 测试记录类型(描述表驱动加载的端到端夹具)
// ————————————————————————————————————————————————

/// 容器元素/键值描述(类外静态,与 schema_dumper 生成布局一致)
/// Container element/key/value descriptors (file-scope statics, matching the
/// schema_dumper output layout).
constexpr FieldDesc kElemString = ElemOf(FieldType::String);
constexpr FieldDesc kElemInt32 = ElemOf(FieldType::Int32);
constexpr FieldDesc kElemWDist = ElemOf(FieldType::WDist);
constexpr FieldDesc kElemWAngle = ElemOf(FieldType::WAngle);

/// 模拟上游 TraitInfo 派生类:字段名 = yaml 键,默认值 = C# 字段初始化器
/// Mimics an upstream TraitInfo derivative: field names are yaml keys,
/// defaults are the C# field initializers.
class TestInfo final : public meta::RecordObject {
 public:
  std::int32_t HP = 0;
  bool Flag = true;
  std::string Name = "default";
  WDist Dist{5};
  WVec Offset{};
  std::vector<std::string> Names{};
  std::vector<std::int32_t> Nums{};
  std::vector<WVec> Offsets{};
  std::int32_t Relationship = 1;  // PlayerRelationship(枚举,底层 int32)
  std::optional<WAngle> Maybe{};
  std::vector<std::pair<std::string, std::int32_t>> Dict{};
  std::uint64_t Targets{};  // BitSet<struct TargetableType>(uint64 布局)
  std::int32_t RequiredField = 0;
  std::string RequiredStr;

  static const meta::RecordDesc& Desc() {
    // 字段顺序 = C# 声明序(基类字段在前的约定在加载链用基类体现)
    static constexpr std::array<FieldDesc, 14> kFields{{
        ORA_FIELD(TestInfo, HP, Int32, false),
        ORA_FIELD(TestInfo, Flag, Bool, false),
        ORA_FIELD(TestInfo, Name, String, false),
        ORA_FIELD_NAMED(TestInfo, Dist, WDist, false, "OpenRA.WDist"),
        ORA_FIELD_NAMED(TestInfo, Offset, WVec, false, "OpenRA.WVec"),
        FieldDesc{.str_name = "Names", .type = FieldType::ImmutableArray, .b_required = false,
                  .str_loader = {}, .off_offset = __builtin_offsetof(TestInfo, Names),
                  .elem = &kElemString, .key = nullptr, .value = nullptr,
                  .str_type_name =
                      "System.Collections.Immutable.ImmutableArray`1[System.String]"},
        FieldDesc{.str_name = "Nums", .type = FieldType::ImmutableArray, .b_required = false,
                  .str_loader = {}, .off_offset = __builtin_offsetof(TestInfo, Nums),
                  .elem = &kElemInt32, .key = nullptr, .value = nullptr,
                  .str_type_name =
                      "System.Collections.Immutable.ImmutableArray`1[System.Int32]"},
        FieldDesc{.str_name = "Offsets", .type = FieldType::ImmutableArray, .b_required = false,
                  .str_loader = {}, .off_offset = __builtin_offsetof(TestInfo, Offsets),
                  .elem = &kElemWDist, .key = nullptr, .value = nullptr,
                  .str_type_name =
                      "System.Collections.Immutable.ImmutableArray`1[OpenRA.WVec]"},
        ORA_FIELD_NAMED(TestInfo, Relationship, Enum, false,
                        "OpenRA.Traits.PlayerRelationship"),
        FieldDesc{.str_name = "Maybe", .type = FieldType::Nullable, .b_required = false,
                  .str_loader = {}, .off_offset = __builtin_offsetof(TestInfo, Maybe),
                  .elem = &kElemWAngle, .key = nullptr, .value = nullptr,
                  .str_type_name = "System.Nullable`1[OpenRA.WAngle]"},
        FieldDesc{.str_name = "Dict", .type = FieldType::FrozenDictionary, .b_required = false,
                  .str_loader = {}, .off_offset = __builtin_offsetof(TestInfo, Dict),
                  .elem = nullptr, .key = &kElemString, .value = &kElemInt32,
                  .str_type_name =
                      "System.Collections.Frozen.FrozenDictionary`2[System.String,System.Int32]"},
        ORA_FIELD_NAMED(TestInfo, Targets, BitSet, false,
                        "OpenRA.Primitives.BitSet`1[OpenRA.Traits.TargetableType]"),
        ORA_FIELD_NAMED(TestInfo, RequiredField, Int32, true, "System.Int32"),
        ORA_FIELD_NAMED(TestInfo, RequiredStr, String, true, "System.String"),
    }};

    static const meta::RecordDesc kDesc{
        .str_name = "TestInfo",
        .str_base = "",
        .fields = std::span<const FieldDesc>{kFields},
        .requires_types = {},
        .not_before_types = {},
    };
    return kDesc;
  }

  const meta::RecordDesc& record_desc() const override { return Desc(); }
};

// ————————————————————————————————————————————————
// 断言工具
// ————————————————————————————————————————————————

int g_failures = 0;

void Check(bool cond, std::string_view msg) {
  if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
    ++g_failures;
  }
}

template <class Fn>
std::string CatchWhat(const Fn& fn) {
  try {
    fn();
  } catch (const std::exception& e) {
    return e.what();
  }
  return {};
}

// ————————————————————————————————————————————————
// 1. 数字/bool 解析(Exts.TryParse* 语义)
// ————————————————————————————————————————————————

void TestNumericParse() {
  std::int32_t int4_v{};
  Check(TryParseInt32Invariant("42", int4_v) && int4_v == 42, "int 42");
  Check(TryParseInt32Invariant(" 42 ", int4_v) && int4_v == 42, "int whitespace-trimmed");
  Check(TryParseInt32Invariant("+42", int4_v) && int4_v == 42, "int leading +");
  Check(TryParseInt32Invariant("-42", int4_v) && int4_v == -42, "int negative");
  Check(!TryParseInt32Invariant("1,000", int4_v), "int no thousands");
  Check(!TryParseInt32Invariant("0x10", int4_v), "int no hex");
  Check(!TryParseInt32Invariant("2147483648", int4_v), "int overflow");
  Check(TryParseInt32Invariant("2147483647", int4_v) && int4_v == 2147483647, "int max");
  Check(TryParseInt32Invariant("-2147483648", int4_v) && int4_v == -2147483648, "int min");
  Check(!TryParseInt32Invariant("", int4_v), "int empty");
  Check(!TryParseInt32Invariant("12a", int4_v), "int junk suffix");

  std::uint8_t uint1_v{};
  Check(TryParseByteInvariant("255", uint1_v) && uint1_v == 255, "byte 255");
  Check(!TryParseByteInvariant("256", uint1_v), "byte overflow");

  float fp4_v{};
  Check(TryParseFloatOrPercentInvariant("0.25", fp4_v) && fp4_v == 0.25f, "float");
  Check(TryParseFloatOrPercentInvariant("50%", fp4_v) && fp4_v == 0.5f, "float percent");
  Check(TryParseFloatOrPercentInvariant("-1.5e2", fp4_v) && fp4_v == -150.0f, "float exp");
  Check(TryParseFloatOrPercentInvariant("1.", fp4_v) && fp4_v == 1.0f, "float trailing dot");
  Check(!TryParseFloatOrPercentInvariant("abc", fp4_v), "float junk");
  Check(TryParseFloatOrPercentInvariant("50 %", fp4_v) && fp4_v == 0.5f,
        "float spaced percent (all '%' stripped)");

  bool b_v{};
  Check(TryParseBoolNet("TRUE", b_v) && b_v, "bool TRUE");
  Check(TryParseBoolNet(" false ", b_v) && !b_v, "bool false trimmed");
  Check(!TryParseBoolNet("yes", b_v), "bool junk");
}

// ————————————————————————————————————————————————
// 2. 枚举解析/序列化(Enum.TryParse/ToString 语义)
// —————————————————————————————————————————————————

void TestEnums() {
  constexpr EnumMemberDesc kRel[] = {
      {"None", 0}, {"Enemy", 1}, {"Neutral", 2}, {"Ally", 4},
  };
  RegisterEnum("OpenRA.Traits.PlayerRelationship", kRel);

  std::int32_t int4_v{};
  Check(TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "Enemy", int4_v) && int4_v == 1,
        "enum name");
  Check(TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "enemy", int4_v) && int4_v == 1,
        "enum ignore-case");
  Check(TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "3", int4_v) && int4_v == 3,
        "enum numeric");
  Check(TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "Enemy, Neutral", int4_v) &&
            int4_v == 3,
        "enum comma list");
  Check(TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "Enemy, Neutral, Ally", int4_v) &&
            int4_v == 7,
        "enum comma list full");
  Check(!TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "enemy,", int4_v),
        "enum trailing comma (empty segment) fails");
  Check(!TryParseEnumNet("OpenRA.Traits.PlayerRelationship", "Missing", int4_v),
        "enum unknown name");
  Check(!TryParseEnumNet("OpenRA.Missing", "Enemy", int4_v), "enum unknown registry");

  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 3) == "Enemy, Neutral",
        "flags to-string");
  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 7) == "Enemy, Neutral, Ally",
        "flags to-string full");
  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 5) == "Enemy, Ally",
        "flags to-string gap");
  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 9) == "Enemy, 8",
        "flags to-string leftover");
  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 0) == "None", "enum zero name");
  Check(EnumToStringNet("OpenRA.Traits.PlayerRelationship", 4) == "Ally", "enum single");
}

// ————————————————————————————————————————————————
// 3. 变量表达式(VariableExpression 语义)
// ————————————————————————————————————————————————

void TestExpressions() {
  const std::vector<std::pair<std::string, std::int32_t>> vec_syms{{"a", 1}, {"b", 0}, {"n", 7}};

  {
    const expr::BooleanExpression e_boolex{"a && !b"};
    Check(e_boolex.Evaluate(vec_syms), "expr and-not");
    Check(e_boolex.Variables().size() == 2, "expr variables collected");
  }
  {
    const expr::IntegerExpression e_intex{"n * 2 + 1"};
    Check(e_intex.Evaluate(vec_syms) == 15, "expr arithmetic precedence");
  }
  {
    const expr::IntegerExpression e_intex{"n / 0"};
    Check(e_intex.Evaluate(vec_syms) == 0, "expr divide-by-zero yields 0");
  }
  {
    const expr::IntegerExpression e_intex{"n % 0"};
    Check(e_intex.Evaluate(vec_syms) == 0, "expr modulo-by-zero yields 0");
  }
  {
    const expr::IntegerExpression e_intex{"-n"};
    Check(e_intex.Evaluate(vec_syms) == -7, "expr negate");
  }
  {
    const expr::IntegerExpression e_intex{"~n"};
    Check(e_intex.Evaluate(vec_syms) == ~7, "expr ones-complement");
  }
  {
    // bool↔int 隐式转换(AstStack.Peek 语义)
    // Implicit bool↔int conversion (AstStack.Peek semantics).
    const expr::IntegerExpression e_intex{"(a && true) + (b || false)"};
    Check(e_intex.Evaluate(vec_syms) == 1, "expr bool-to-int");
  }

  // 构造期错误消息(逐字)
  // Construction-time error messages (verbatim).
  Check(CatchWhat([] { expr::BooleanExpression{"1 +2"}; }) ==
            "Missing whitespace at index 3, after `+` operator.",
        "expr missing whitespace after");
  Check(CatchWhat([] { expr::BooleanExpression{"1+ 2"}; }) ==
            "Missing whitespace at index 2, before `+` operator.",
        "expr missing whitespace before");
  Check(CatchWhat([] { expr::BooleanExpression{""}; }) == "Empty expression", "expr empty");
  Check(CatchWhat([] { expr::BooleanExpression{"(a"}; }) ==
            "Unclosed opening parenthesis at index 0", "expr unclosed paren");
  Check(CatchWhat([] { expr::BooleanExpression{"a)"}; }) ==
            "Unmatched closing parenthesis at index 1", "expr unmatched close");
  Check(CatchWhat([] { expr::BooleanExpression{"! + 2"}; }) ==
            "Missing value or sub-expression or there is an extra operator `!` at index 0 or "
            "`+` at index 2",
        "expr consecutive operators");
  Check(CatchWhat([] { expr::BooleanExpression{"a b"}; }) ==
            "Missing binary operation before `b` at index 2",
        "expr missing binary op");
  Check(CatchWhat([] { expr::BooleanExpression{"a + "}; }) ==
            "Missing value or sub-expression at end for `+` operator",
        "expr trailing operator");
  Check(CatchWhat([] { expr::BooleanExpression{"2x"}; }) ==
            "Number 2 and variable merged at index 0",
        "expr number variable merged");
  Check(CatchWhat([] { expr::BooleanExpression{"= 1"}; }) ==
            "Unexpected character '=' at index 0 - should it be `==`?",
        "expr single equals");
  Check(CatchWhat([] { expr::BooleanExpression{"a."}; }) ==
            "Invalid identifier end character at index 1 for `a.`",
        "expr mixed identifier end");
}

// ————————————————————————————————————————————————
// 4. FieldLoader::Load 端到端
// ————————————————————————————————————————————————

void TestFieldLoader() {
  // BitSet 标签注册(真实加载链由 mods 层注册)
  // BitSet tag registration (the real loading chain registers from the mods
  // layer).
  struct TargetableType {};
  // 注册键 = 字段 str_type_name 的 C# Type.ToString() 形态(生成器同约定)
  // The registry key equals the C# Type.ToString() form of the field's
  // str_type_name (the generator follows the same convention).
  RegisterBitSet(
      "OpenRA.Primitives.BitSet`1[OpenRA.Traits.TargetableType]",
      [](std::span<const std::string> vec) {
        return core::BitSetAllocator<TargetableType>::GetBits(vec);
      },
      [](std::uint64_t uint8_bits) {
        return core::BitSetAllocator<TargetableType>::GetStrings(uint8_bits);
      });

  auto load = [](std::string_view sv_yaml) {
    auto info = std::make_unique<TestInfo>();
    auto nodes = yaml::MiniYaml::FromString(sv_yaml, "test");
    // 单节点(键 TestInfo)直接取其值加载;顶层字段用节点值
    // Load the value of the single top-level node (fields live under it).
    meta::Load(info.get(), nodes.at(0).Value);
    return info;
  };

  {
    auto info = load("t:\n\tHP: 100\n\tFlag: false\n\tName: hello\n\tDist: 2c512\n"
                     "\tOffset: 1, 2, 3\n\tNames: a, b, c\n\tNums: 1, 2, 3\n"
                     "\tOffsets: 1,2,3, 4,5,6\n\tRelationship: Enemy, Neutral\n"
                     "\tMaybe: 128\n\tTargets: Ground, Water\n"
                     "\tRequiredField: 7\n\tRequiredStr: x\n"
                     "\tDict:\n\t\ta: 1\n\t\tb: 2\n");
    Check(info->HP == 100, "load int");
    Check(!info->Flag, "load bool");
    Check(info->Name == "hello", "load string");
    Check(info->Dist.Length == 2 * 1024 + 512, "load wdist NcM");
    Check(info->Offset.X == 1 && info->Offset.Y == 2 && info->Offset.Z == 3, "load wvec");
    Check(info->Names == std::vector<std::string>{"a", "b", "c"}, "load string array");
    Check(info->Nums == std::vector<std::int32_t>{1, 2, 3}, "load int array");
    Check(info->Offsets.size() == 2 && info->Offsets[1].Y == 5, "load grouped wvec array");
    Check(info->Relationship == 3, "load flags enum");
    Check(info->Maybe.has_value() && info->Maybe->Angle == 128, "load nullable wangle");
    Check(info->Targets != 0, "load bitset");
    Check(info->Dict.size() == 2 && info->Dict[0].first == "a" && info->Dict[0].second == 1,
          "load dictionary");
    Check(info->RequiredField == 7, "load required present");
  }

  {
    // 未给出的字段保持默认值
    // Untouched fields keep their defaults.
    auto info = load("t:\n\tHP: 5\n\tRequiredField: 1\n\tRequiredStr: r\n");
    Check(info->Flag, "default bool kept");
    Check(info->Name == "default", "default string kept");
    Check(info->Dist.Length == 5, "default wdist kept");
    Check(info->Names.empty(), "default array kept");
    Check(!info->Maybe.has_value(), "default nullable kept");
  }

  {
    // MissingFieldsException(无 header 形态;消息逐字)
    // MissingFieldsException (headerless form; verbatim message).
    const std::string str_err = CatchWhat([] {
      auto info = std::make_unique<TestInfo>();
      auto nodes = yaml::MiniYaml::FromString("t:\n\tHP: 1\n", "test");
      meta::Load(info.get(), nodes.at(0).Value);
    });
    Check(str_err == "RequiredField, RequiredStr", "missing fields message");
  }

  {
    // InvalidValueAction 消息(逐字;fieldType = C# Type.ToString())
    // InvalidValueAction message (verbatim; fieldType = C# Type.ToString()).
    const std::string str_err = CatchWhat([] {
      auto info = std::make_unique<TestInfo>();
      auto nodes = yaml::MiniYaml::FromString("t:\n\tHP: abc\n\tRequiredField: 1\n"
                                              "\tRequiredStr: r\n",
                                              "test");
      meta::Load(info.get(), nodes.at(0).Value);
    });
    Check(str_err == "FieldLoader: Cannot parse `abc` into field `HP` of type `System.Int32`",
          "invalid value message, got: " + str_err);
  }

  {
    // 表达式字段的包装消息前缀(经 BooleanExpression 字段的字典场景外的
    // 直接字段形态由 WeaponInfo 链覆盖;此处以字典值构造错误验证包装格式)
    // The expression-field wrap prefix (verified via a dictionary-value
    // construction error; direct fields are exercised by the WeaponInfo chain).
    const std::string str_err = CatchWhat([] {
      auto nodes = yaml::MiniYaml::FromString(
          "t:\n\tHP: 1\n\tRequiredField: 1\n\tRequiredStr: r\n", "test");
      try {
        expr::VariableExpression{"1 +2"};
      } catch (const expr::ExpressionException& e_except) {
        throw yaml::YamlException(std::format(
            "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`: {}", "1 +2",
            "RequiresCondition", "OpenRA.Support.BooleanExpression", e_except.what()));
      }
    });
    Check(str_err ==
              "FieldLoader: Cannot parse `1 +2` into field `RequiresCondition` of type "
              "`OpenRA.Support.BooleanExpression`: Missing whitespace at index 3, after `+` "
              "operator.",
          "expression wrap message");
  }
}

// ————————————————————————————————————————————————
// 5. Color / BitSet
// ————————————————————————————————————————————————

void TestColorAndBitSet() {
  core::Color color_v{};
  Check(core::Color::TryParse("FF0000", color_v) && color_v.A() == 255 && color_v.R() == 255,
        "color 6-digit");
  Check(core::Color::TryParse("ff000080", color_v) && color_v.A() == 0x80,
        "color 8-digit case-insensitive");
  Check(!core::Color::TryParse("FFF00", color_v), "color bad length");
  Check(!core::Color::TryParse("GGGGGG", color_v), "color bad hex");
  Check(core::Color::FromArgb(0xFF, 0x12, 0x34, 0x56).ToString() == "123456",
        "color to-string 6");
  Check(core::Color::FromArgb(0x80, 0x12, 0x34, 0x56).ToString() == "12345680",
        "color to-string 8");

  struct TagA {};
  const core::BitSet<TagA> set_a{std::array<std::string, 3>{"x", "y", "z"}};
  Check(set_a.ToString() == "x,y,z", "bitset to-string allocation order");
  const core::BitSet<TagA> set_b{std::array<std::string, 2>{"x", "z"}};
  Check(set_a.Overlaps(set_b) && set_a.Union(set_b).ToString() == "x,y,z", "bitset ops");
  Check(core::BitSet<TagA>::FromStringsNoAlloc(std::array<std::string, 1>{"missing"}).IsEmpty(),
        "bitset no-alloc ignores unknown");
}

int main() {
  TestNumericParse();
  TestEnums();
  TestExpressions();
  TestFieldLoader();
  TestColorAndBitSet();

  if (g_failures != 0) {
    std::println("{} failures", g_failures);
    return 1;
  }
  std::println("meta_test: all passed");
  return 0;
}
