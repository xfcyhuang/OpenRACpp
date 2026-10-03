// UPSTREAM: OpenRA.Test/OpenRA.Game/MiniYamlTest.cs @7d57605 L26-997(全用例移植;
// FromStreamAsEnumerable 并发流式用例不移植——IEnumerable 惰性属实现细节)
// All upstream cases ported; the FromStreamAsEnumerable concurrent-streaming case is
// not ported (IEnumerable laziness is an implementation detail).
// 断言文本与上游逐字一致(含异常消息),作为 Merge/继承/弱删除语义的回归契约。
// Assertion texts match upstream verbatim (including exception messages), serving as
// the regression contract for merge/inheritance/weak-removal semantics.
// 多行 yaml 字面量由 tools/fill_yaml_test_literals.py 从上游源码按字节注入
// ("函数.常量" 双花括号占位符),杜绝缩进层级的手抄偏差。
// Multi-line yaml literals are injected byte-exactly from the upstream source by
// tools/fill_yaml_test_literals.py (Func.Const double-brace placeholders),
// eliminating hand-transcription indent errors.
#include "yaml/mini_yaml.hpp"

namespace {

using ora::yaml::MiniYaml;
using ora::yaml::MiniYamlNode;
using ora::yaml::StringPool;
using ora::yaml::WriteToString;
using ora::yaml::YamlException;

int checks = 0;
int failures = 0;

void Fail(const std::string& name, const std::string& detail) {
  failures++;
  std::println(std::cerr, "FAIL [{}] {}", name, detail);
}

void CheckTrue(const std::string& name, bool cond, const std::string& what) {
  checks++;
  if (!cond)
    Fail(name, what);
}

void CheckEq(const std::string& name, const std::string& actual, const std::string& expected) {
  checks++;
  if (actual != expected)
    Fail(name, std::format("输出不一致\n  期望: [{}]\n  实际: [{}]", expected, actual));
}

// 单一字符串池贯穿全部用例(合并路径的池化不变量)
// One string pool threads through every case (pooling invariant for merge paths).
StringPool& Pool() {
  static StringPool pool;
  return pool;
}

std::vector<MiniYamlNode> FromStr(std::string_view text, std::string_view name = "",
                                  bool keep = true) {
  return MiniYaml::FromString(text, name, keep, Pool());
}

const MiniYamlNode* FindNode(std::span<const MiniYamlNode> nodes, std::string_view key) {
  for (const MiniYamlNode& n : nodes)
    if (n.Key != nullptr && std::string_view{*n.Key} == key)
      return &n;
  return nullptr;
}

bool HasKey(std::span<const MiniYamlNode> nodes, std::string_view key) {
  return FindNode(nodes, key) != nullptr;
}

std::string ValueOf(const MiniYamlNode* n) {  // C# .Value.Value(null → "<null>")
  return n != nullptr && n->Value.Value != nullptr ? *n->Value.Value : std::string{"<null>"};
}

template <typename F>
void CheckThrowsYaml(const std::string& name, F&& fn, const std::string& expectedMsg) {
  checks++;
  try {
    fn();
    Fail(name, "期望抛 YamlException,实际未抛");
  } catch (const YamlException& e) {
    if (std::string_view{e.what()} != expectedMsg)
      Fail(name, std::format("异常消息不一致\n  期望: [{}]\n  实际: [{}]", expectedMsg, e.what()));
  }
}

// ———— 用例(MiniYamlTest.cs 同名直译)————
// ———— Cases (direct ports of the MiniYamlTest.cs counterparts) ————

void TestParseRoundtrip() {
  CheckEq("Parse tree roundtrips", WriteToString(FromStr("1:\n2: Test\n3: # Test\n4:\n\t4.1:\n5: Test\n\t5.1:\n6: # Test\n\t6.1:\n7:\n\t7.1.1:\n\t7.1.2: Test\n\t7.1.3: # Test\n8: Test\n\t8.1.1:\n\t8.1.2: Test\n\t8.1.3: # Test\n9: # Test\n\t9.1.1:\n\t9.1.2: Test\n\t9.1.3: # Test\n", "", false)),
          "1:\n2: Test\n3: # Test\n4:\n\t4.1:\n5: Test\n\t5.1:\n6: # Test\n\t6.1:\n7:\n\t7.1.1:\n\t7.1.2: Test\n\t7.1.3: # Test\n8: Test\n\t8.1.1:\n\t8.1.2: Test\n\t8.1.3: # Test\n9: # Test\n\t9.1.1:\n\t9.1.2: Test\n\t9.1.3: # Test\n");
}

void TestParseEmptyLines() {
  CheckEq("Parse tree can handle empty lines",
          WriteToString(FromStr("1:\n\n2: Test\n\n3: # Test\n\n4:\n\n\t4.1:\n\n5: Test\n\n\t5.1:\n\n6: # Test\n\n\t6.1:\n\n7:\n\n\t7.1.1:\n\n\t7.1.2: Test\n\n\t7.1.3: # Test\n\n8: Test\n\n\t8.1.1:\n\n\t8.1.2: Test\n\n\t8.1.3: # Test\n\n9: # Test\n\n\t9.1.1:\n\n\t9.1.2: Test\n\n\t9.1.3: # Test\n\n")), "1:\n2: Test\n3:\n4:\n\t4.1:\n5: Test\n\t5.1:\n6:\n\t6.1:\n7:\n\t7.1.1:\n\t7.1.2: Test\n\t7.1.3:\n8: Test\n\t8.1.1:\n\t8.1.2: Test\n\t8.1.3:\n9:\n\t9.1.1:\n\t9.1.2: Test\n\t9.1.3:\n");
}

void TestIndents() {
  CheckEq("Mixed tabs & spaces indents", WriteToString(FromStr("\nRoot1:\n\tChild1:\n\t\tAttribute1: Test\n\t\tAttribute2: Test\n\tChild2:\n\t\tAttribute1: Test\n\t\tAttribute2: Test\nRoot2:\n\tChild1:\n\t\tAttribute1: Test\n")),
          WriteToString(FromStr("\nRoot1:\n    Child1:\n        Attribute1: Test\n        Attribute2: Test\n\tChild2:\n\t\tAttribute1: Test\n\t    Attribute2: Test\nRoot2:\n    Child1:\n\t\tAttribute1: Test\n")));
}

void NodeRemoval() {
  CheckEq("Yaml files should be able to remove nodes",
          WriteToString(MiniYaml::Merge({FromStr("\nParent:\n\tChild:\n\t\tKey: value\n\t\t-Key:\n")})),
          "Parent:\n\tChild:\n");
}

void NodeRemovalAndOverride() {
  CheckEq("Yaml files should be able to remove nodes and immediately override",
          WriteToString(MiniYaml::Merge({FromStr("\nParent:\n\tChild:\n\t\tKey: value\n\t\t-Key:\n\t\tKey: value2\n")})),
          "Parent:\n\tChild:\n\t\tKey: value2\n");
}

void MergedNodeRemoval() {
  CheckEq("Merged yaml files should be able to remove nodes",
          WriteToString(MiniYaml::Merge(
              {FromStr("\nParent:\n\tChild:\n\t\tKey: value\n"), FromStr("\nParent:\n\tChild:\n\t\t-Key:\n")})),
          "Parent:\n\tChild:\n");
}

void MergedNodeRemovalAndOverride() {
  CheckEq("Merged yaml files should be able to remove nodes and immediately override",
          WriteToString(MiniYaml::Merge(
              {FromStr("\nParent:\n\tChild:\n\t\tKey: value\n"),
               FromStr("\nParent:\n\tChild:\n\t\t-Key:\n\t\tKey: value2\n")})),
          "Parent:\n\tChild:\n\t\tKey: value2\n");
}

void MergedInheritedNodeRemoval() {
  CheckEq("Merged yaml files should be able to remove nodes from inherited parents",
          WriteToString(MiniYaml::Merge(
              {FromStr("\n^Base:\n\tChild:\n\t\tKey: value\nParent:\n\tInherits: ^Base\n"),
               FromStr("\nParent:\n\tChild:\n\t\t-Key:\n")})),
          "^Base:\n\tChild:\n\t\tKey: value\nParent:\n\tChild:\n");
}

void MergedInheritedNodeRemovalAndOverride() {
  CheckEq(
      "Merged yaml files should be able to remove nodes from inherited parents and immediately "
      "override",
      WriteToString(MiniYaml::Merge(
          {FromStr("\n^Base:\n\tChild:\n\t\tKey: value\nParent:\n\tInherits: ^Base\n"),
           FromStr("\nParent:\n\tChild:\n\t\t-Key:\n\t\tKey: value2\n")})),
      "^Base:\n\tChild:\n\t\tKey: value\nParent:\n\tChild:\n\t\tKey: value2\n");
}

void InheritanceAndRemovalCanBeComposed() {
  auto result = MiniYaml::Merge({FromStr("\n^BaseA:\n\tMockA2:\n^BaseB:\n\tInherits@a: ^BaseA\n\tMockB2:\n"),
                                  FromStr("\nTest:\n\tInherits@b: ^BaseB\n\t-MockA2:\n"),
                                  FromStr("\n^BaseC:\n\tMockC2:\nTest:\n\tInherits@c: ^BaseC\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Inheritance and removal can be composed", test != nullptr, "缺 Test 节点");
  if (test != nullptr) {
    CheckTrue("Inheritance and removal can be composed", !HasKey(test->Value.Nodes, "MockA2"),
              "Node should not have the MockA2 child, but does.");
    CheckTrue("Inheritance and removal can be composed", HasKey(test->Value.Nodes, "MockB2"),
              "Node should have the MockB2 child, but does not.");
    CheckTrue("Inheritance and removal can be composed", HasKey(test->Value.Nodes, "MockC2"),
              "Node should have the MockC2 child, but does not.");
  }
}

void ChildCanBeRemovedAfterMultipleInheritance() {
  auto result = MiniYaml::Merge(
      {FromStr("\n^BaseA:\n\tMockA2:\nTest:\n\tInherits: ^BaseA\n\tMockA2:\n"),
       FromStr("\nTest:\n\t-MockA2\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Child can be removed after multiple inheritance", test != nullptr, "缺 Test 节点");
  if (test != nullptr)
    CheckTrue("Child can be removed after multiple inheritance",
              !HasKey(test->Value.Nodes, "MockA2"),
              "Node should not have the MockA2 child, but does.");
}

void InheritedChildCanBeImmediatelyRemoved() {
  auto result = MiniYaml::Merge({FromStr("\n^BaseA:\n\tMockString:\n\t\tAString: Base\nTest:\n\tInherits: ^BaseA\n\tMockString:\n\t\tAString: Override\n\t-MockString:\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Inherited child can be immediately removed", test != nullptr, "缺 Test 节点");
  if (test != nullptr)
    CheckTrue("Inherited child can be immediately removed",
              !HasKey(test->Value.Nodes, "MockString"),
              "Node should not have the MockString child, but does.");
}

void InheritedChildCanBeRemovedAndImmediatelyOverridden() {
  auto result =
      MiniYaml::Merge({FromStr("\n^BaseA:\n\tMockString:\n\t\tAString: Base\nTest:\n\tInherits: ^BaseA\n\t-MockString:\n\tMockString:\n\t\tAString: Override\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Inherited child can be removed and immediately overridden", test != nullptr,
            "缺 Test 节点");
  if (test != nullptr) {
    const MiniYamlNode* mock = FindNode(test->Value.Nodes, "MockString");
    CheckTrue("Inherited child can be removed and immediately overridden", mock != nullptr,
              "Node should have the MockString child, but does not.");
    if (mock != nullptr)
      CheckTrue("Inherited child can be removed and immediately overridden",
                ValueOf(FindNode(mock->Value.Nodes, "AString")) == "Override",
                "MockString value has not been set with the correct override value for AString.");
  }
}

void InheritedChildCanBeRemovedAndLaterOverridden() {
  auto result = MiniYaml::Merge(
      {FromStr("\n^BaseA:\n\tMockString:\n\t\tAString: Base\nTest:\n\tInherits: ^BaseA\n\t-MockString:\n"),
       FromStr("\nTest:\n\tMockString:\n\t\tAString: Override\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Inherited child can be removed and later overridden", test != nullptr, "缺 Test 节点");
  if (test != nullptr) {
    const MiniYamlNode* mock = FindNode(test->Value.Nodes, "MockString");
    CheckTrue("Inherited child can be removed and later overridden", mock != nullptr,
              "Node should have the MockString child, but does not.");
    if (mock != nullptr)
      CheckTrue("Inherited child can be removed and later overridden",
                ValueOf(FindNode(mock->Value.Nodes, "AString")) == "Override",
                "MockString value has not been set with the correct override value for AString.");
  }
}

void InheritedChildCanBeOverriddenThenRemoved() {
  auto result = MiniYaml::Merge(
      {FromStr("\n^BaseA:\n\tMockString:\n\t\tAString: Base\n^BaseB:\n\tInherits: ^BaseA\n\tMockString:\n\t\tAString: Override\n"),
       FromStr("\nTest:\n\tInherits: ^BaseB\n\tMockString:\n\t\t-AString:\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  CheckTrue("Inherited child can be removed from intermediate parent", test != nullptr,
            "缺 Test 节点");
  if (test != nullptr) {
    const MiniYamlNode* mock = FindNode(test->Value.Nodes, "MockString");
    CheckTrue("Inherited child can be removed from intermediate parent", mock != nullptr,
              "Node should have the MockString child, but does not.");
    if (mock != nullptr)
      CheckTrue("Inherited child can be removed from intermediate parent",
                !HasKey(mock->Value.Nodes, "AString"),
                "MockString value should have been removed, but was not.");
  }
}

// 返回的 fieldSub 指向 outMerged 内部,outMerged 须在调用方存活
// (避免悬垂:合并树与返回指针同生命周期)
// The returned fieldSub points inside outMerged, which must outlive the pointer
// (no dangling: the merged tree and the returned pointer share a lifetime).
const MiniYamlNode* SingleChildFieldSubNodes(const std::string& name,
                                             std::vector<std::vector<MiniYamlNode>> sources,
                                             std::vector<MiniYamlNode>& outMerged) {
  outMerged = MiniYaml::Merge(std::move(sources));
  const MiniYamlNode* test = FindNode(outMerged, "Test");
  if (test == nullptr) {
    Fail(name, "缺 Test 节点");
    return nullptr;
  }
  if (test->Value.Nodes.size() != 1) {
    Fail(name, "Test 应只有一个 trait 子节点");
    return nullptr;
  }
  const MiniYamlNode* traitNode = &test->Value.Nodes[0];
  if (traitNode->Value.Nodes.size() != 1) {
    Fail(name, "trait 应只有一个字段子节点");
    return nullptr;
  }
  return &traitNode->Value.Nodes[0];
}

void MergedChildSubNodeCanBeRemovedAndImmediatelyOverridden() {
  const std::string name =
      "Merged child subnode can be removed and immediately overridden";
  std::vector<MiniYamlNode> merged;
  const MiniYamlNode* fieldSub = SingleChildFieldSubNodes(
      name,
      {FromStr("\nTest:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringA: A\n\t\t\tStringB: B\nTest:\n\tMockString:\n\t\t-CollectionOfStrings:\n\t\tCollectionOfStrings:\n\t\t\tStringC: C\n")},
      merged);
  if (fieldSub == nullptr)
    return;
  CheckTrue(name, fieldSub->Value.Nodes.size() == 1,
            "Collection of strings should only contain the overriding subnode.");
  const MiniYamlNode* sc = FindNode(fieldSub->Value.Nodes, "StringC");
  CheckTrue(name, sc != nullptr && ValueOf(sc) == "C",
            "CollectionOfStrings value has not been set with the correct override value for "
            "StringC.");
}

void MergedChildSubNodeCanBeRemovedAndLaterOverridden() {
  const std::string name = "Merged child subnode can be removed and later overridden";
  std::vector<MiniYamlNode> merged;
  const MiniYamlNode* fieldSub = SingleChildFieldSubNodes(
      name,
      {FromStr("\nTest:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringA: A\n\t\t\tStringB: B\nTest:\n\tMockString:\n\t\t-CollectionOfStrings:\n"),
       FromStr("\nTest:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringC: C\n")},
      merged);
  if (fieldSub == nullptr)
    return;
  CheckTrue(name, fieldSub->Value.Nodes.size() == 1,
            "Collection of strings should only contain the overriding subnode.");
  const MiniYamlNode* sc = FindNode(fieldSub->Value.Nodes, "StringC");
  CheckTrue(name, sc != nullptr && ValueOf(sc) == "C",
            "CollectionOfStrings value has not been set with the correct override value for "
            "StringC.");
}

void InheritedChildSubNodeCanBeRemovedAndImmediatelyOverridden() {
  const std::string name =
      "Inherited child subnode can be removed and immediately overridden";
  std::vector<MiniYamlNode> merged;
  const MiniYamlNode* fieldSub = SingleChildFieldSubNodes(
      name,
      {FromStr("\n^BaseA:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringA: A\n\t\t\tStringB: B\nTest:\n\tInherits: ^BaseA\n\tMockString:\n\t\t-CollectionOfStrings:\n\t\tCollectionOfStrings:\n\t\t\tStringC: C\n")},
      merged);
  if (fieldSub == nullptr)
    return;
  CheckTrue(name, fieldSub->Value.Nodes.size() == 1,
            "Collection of strings should only contain the overriding subnode.");
  const MiniYamlNode* sc = FindNode(fieldSub->Value.Nodes, "StringC");
  CheckTrue(name, sc != nullptr && ValueOf(sc) == "C",
            "CollectionOfStrings value has not been set with the correct override value for "
            "StringC.");
}

void InheritedChildSubNodeCanBeRemovedAndLaterOverridden() {
  const std::string name = "Inherited child subnode can be removed and later overridden";
  std::vector<MiniYamlNode> merged;
  const MiniYamlNode* fieldSub = SingleChildFieldSubNodes(
      name,
      {FromStr("\n^BaseA:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringA: A\n\t\t\tStringB: B\nTest:\n\tInherits: ^BaseA\n\tMockString:\n\t\t-CollectionOfStrings:\n"),
       FromStr("\nTest:\n\tMockString:\n\t\tCollectionOfStrings:\n\t\t\tStringC: C\n")},
      merged);
  if (fieldSub == nullptr)
    return;
  CheckTrue(name, fieldSub->Value.Nodes.size() == 1,
            "Collection of strings should only contain the overriding subnode.");
  const MiniYamlNode* sc = FindNode(fieldSub->Value.Nodes, "StringC");
  CheckTrue(name, sc != nullptr && ValueOf(sc) == "C",
            "CollectionOfStrings value has not been set with the correct override value for "
            "StringC.");
}

void InheritanceWorksForNestedNodes() {
  CheckEq("Inheritance works for nested nodes",
          WriteToString(MiniYaml::Merge(
              {FromStr("\n^DefaultKey:\n\tKey: value\n"),
               FromStr("\nParent:\n\tChild:\n\t\tInherits: ^DefaultKey\n")})),
          "^DefaultKey:\n\tKey: value\nParent:\n\tChild:\n\t\tKey: value\n");
}

void EmptyLinesShouldCountTowardLineNumbers() {
  // 树须存活到断言之后(FindNode(FromStr(...)) 的临时穿透会悬垂)
  // The tree must outlive the assertions (piping a temporary into FindNode dangles).
  auto nodes = FromStr("\nTestA:\n\tNothing:\n\nTestB:\n\tNothing:\n");
  const MiniYamlNode* b = FindNode(nodes, "TestB");
  CheckTrue("Empty lines should count toward line numbers", b != nullptr, "缺 TestB");
  if (b != nullptr)
    CheckTrue("Empty lines should count toward line numbers", b->Location.Line == 5,
              std::format("期望行 5,实际 {}", b->Location.Line));
}

void TestSelfMerging() {
  auto result = MiniYaml::Merge({FromStr("\nTest:\n\tMerge: original\n\t\tChild: original\n\tOriginal:\nTest:\n\tMerge: override\n\t\tChild: override\n\tOverride:\n")});
  std::size_t count = 0;
  for (const auto& n : result)
    if (n.Key != nullptr && *n.Key == "Test")
      count++;
  CheckTrue("Duplicated nodes are correctly merged", count == 1,
            "Result should have exactly one Test node.");
  const MiniYamlNode* test = FindNode(result, "Test");
  if (test == nullptr)
    return;
  const auto& nodes = test->Value.Nodes;
  const bool orderOk = nodes.size() == 3 && nodes[0].Key && *nodes[0].Key == "Merge" &&
                       nodes[1].Key && *nodes[1].Key == "Original" && nodes[2].Key &&
                       *nodes[2].Key == "Override";
  CheckTrue("Duplicated nodes are correctly merged", orderOk,
            "Merged Test node has incorrect child nodes.");
  const MiniYamlNode* mergeNode = FindNode(nodes, "Merge");
  CheckTrue("Duplicated nodes are correctly merged",
            mergeNode != nullptr && ValueOf(mergeNode) == "override",
            "Merge node has incorrect value.");
  CheckTrue("Duplicated nodes are correctly merged",
            mergeNode != nullptr && !mergeNode->Value.Nodes.empty() &&
                ValueOf(&mergeNode->Value.Nodes[0]) == "override",
            "Merge node Child value should be 'override', but is not");
}

void TestSelfMergingMultiSource() {
  auto result = MiniYaml::Merge(
      {FromStr("\nTest:\n\tMerge: original\n\t\tChild: original\n\tOriginal:\n"),
       FromStr("\nTest:\n\tMerge: original\n\t\tChild: original\n\tOriginal:\nTest:\n\tMerge: override\n\t\tChild: override\n\tOverride:\n")});
  std::size_t count = 0;
  for (const auto& n : result)
    if (n.Key != nullptr && *n.Key == "Test")
      count++;
  CheckTrue("Duplicated nodes across multiple sources are correctly merged", count == 1,
            "Result should have exactly one Test node.");
  const MiniYamlNode* test = FindNode(result, "Test");
  if (test == nullptr)
    return;
  const auto& nodes = test->Value.Nodes;
  const bool orderOk = nodes.size() == 3 && nodes[0].Key && *nodes[0].Key == "Merge" &&
                       nodes[1].Key && *nodes[1].Key == "Original" && nodes[2].Key &&
                       *nodes[2].Key == "Override";
  CheckTrue("Duplicated nodes across multiple sources are correctly merged", orderOk,
            "Merged Test node has incorrect child nodes.");
  const MiniYamlNode* mergeNode = FindNode(nodes, "Merge");
  CheckTrue("Duplicated nodes across multiple sources are correctly merged",
            mergeNode != nullptr && ValueOf(mergeNode) == "override",
            "Merge node has incorrect value.");
  CheckTrue("Duplicated nodes across multiple sources are correctly merged",
            mergeNode != nullptr && !mergeNode->Value.Nodes.empty() &&
                ValueOf(&mergeNode->Value.Nodes[0]) == "override",
            "Merge node Child value should be 'override', but is not");
}

void TestMergeConflictsNoMerge() {
  CheckThrowsYaml("Duplicated child nodes throw merge error if parent does not require merging",
                  [&] { MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\tChild:\n\t\tChild:\n", "test-filename")}); },
                  "MiniYaml.Merge, duplicate values found for the following keys: Child: [Child "
                  "(at test-filename:4),Child (at test-filename:5)]");
}

void TestDuplicatedRemovals() {
  CheckThrowsYaml("Duplicated removal nodes throw removal error",
                  [&] { MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\tChild:\n\t\t-Child:\n\t\t-Child:\n", "test-filename")}); },
                  "test-filename:6: There are no elements with key `Child` to remove");
}

void TestMergeConflictsNoMergeWithRemovals() {
  CheckEq("Duplicated child nodes with intervening removals do not throw if parent does not "
          "require merging",
          WriteToString(MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\tChildA:\n\t\tChildB:\n\t\t-ChildA:\n\t\tChildA:\n\t\t-ChildB:\n\t\tChildB:\n")})),
          "Test:\n\tMerge:\n\t\tChildA:\n\t\tChildB:\n");
}

void TestMergeConflictsNoMergeWithInsufficientRemovals() {
  CheckThrowsYaml(
      "Duplicated child nodes with insufficient intervening removals throw merge error",
      [&] {
        MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\t-ChildA:\n\t\t-ChildB:\n\t\tChildA:\n\t\tChildB:\n\t\tChildA:\n\t\t-ChildB:\n\t\tChildB:\n",
                                "test-filename")});
      },
      "MiniYaml.Merge, duplicate values found for the following keys: ChildA: [ChildA (at "
      "test-filename:6),ChildA (at test-filename:8)]");
}

void TestMergeMultiSourceWithRemovals() {
  CheckEq("Duplicated child nodes with intervening removals across multiple source do not throw",
          WriteToString(MiniYaml::Merge(
              {FromStr("\nTest:\n\tMerge:\n\t\tChildA:\n\t\tChildB:\n"),
               FromStr("\nTest:\n\tMerge:\n\t\t-ChildB:\n\t\tChildA:\n\t\tChildB:\n\t\t-ChildA:\n\t\t-ChildB:\n")})),
          "Test:\n\tMerge:\n");
}

void TestMergeConflictsFirstParent() {
  CheckThrowsYaml("Duplicated child nodes throw merge error if first parent requires merging",
                  [&] {
                    MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\tChild1:\n\t\tChild1:\n\tMerge:\n",
                                            "test-filename")});
                  },
                  "MiniYaml.Merge, duplicate values found for the following keys: Child1: [Child1 "
                  "(at test-filename:4),Child1 (at test-filename:5)]");
}

void TestMergeConflictsSecondParent() {
  CheckThrowsYaml("Duplicated child nodes throw merge error if second parent requires merging",
                  [&] {
                    MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\tMerge:\n\t\tChild2:\n\t\tChild2:\n",
                                            "test-filename")});
                  },
                  "MiniYaml.Merge, duplicate values found for the following keys: Child2: [Child2 "
                  "(at test-filename:5),Child2 (at test-filename:6)]");
}

void TestMergeConflictsMultiSourceMerge() {
  auto result = MiniYaml::Merge(
      {FromStr("\nTest:\n\tMerge:\n\t\tChild:\n"),
       FromStr("\nTest:\n\tMerge:\n\t\tChild:\n")});
  const MiniYamlNode* test = FindNode(result, "Test");
  const MiniYamlNode* mergeNode = test != nullptr ? FindNode(test->Value.Nodes, "Merge") : nullptr;
  CheckTrue("Duplicated child nodes across multiple sources do not throw",
            mergeNode != nullptr && mergeNode->Value.Nodes.size() == 1, "Merge 子节点数 != 1");
}

void TestMergeConflictsMultiSourceFirstParent() {
  CheckThrowsYaml(
      "Duplicated child nodes across multiple sources throw merge error if first parent requires "
      "merging",
      [&] {
        MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n\t\tChild1:\n\t\tChild1:\n",
                                "test-filename"),
                         FromStr("\nTest:\n\tMerge:\n")});
      },
      "MiniYaml.Merge, duplicate values found for the following keys: Child1: [Child1 (at "
      "test-filename:4),Child1 (at test-filename:5)]");
}

void TestMergeConflictsMultiSourceSecondParent() {
  CheckThrowsYaml(
      "Duplicated child nodes across multiple sources throw merge error if second parent "
      "requires merging",
      [&] {
        MiniYaml::Merge({FromStr("\nTest:\n\tMerge:\n"),
                         FromStr("\nTest:\n\tMerge:\n\t\tChild2:\n\t\tChild2:\n",
                                "test-filename")});
      },
      "MiniYaml.Merge, duplicate values found for the following keys: Child2: [Child2 (at "
      "test-filename:4),Child2 (at test-filename:5)]");
}

void TestMergeComments() {
  checks++;  // Assert.That(Merge, Throws.Nothing)
  try {
    (void)MiniYaml::Merge({FromStr("\n# Random comment\nT:\n\tTest2:\n\t\tMockString:\n\t\t\tMockString2:\n\t\t\tMockString3:\n\t\t\t\tChild1:\n\t\t\t\t# Random comment\n\t\t# Random comment\n\t\tMockString4:\n\t# Random comment\n# Random comment\nT:\n\tTest2:\n\t\tMockString:\n\t\t\t-MockString2:\n\t\t\tMockString3:\n\t\t\t\t# Random comment\n\t\t\t\t-Child1:\n\t\t# Random comment\n\t\tMockString4:\n\t# Random comment\n# Random comment\n", "test-filename", false)});
  } catch (const YamlException& e) {
    Fail("Merging may be done on yaml that was not sanitised from comments.",
         std::format("不应抛异常: {}", e.what()));
  }
}

void TestEscapedHashInValues() {
  const std::string name = "Comments are correctly separated from values";
  {
    auto nodes = FromStr("key: value # comment", "", false);
    CheckTrue(name, nodes.size() == 1 && ValueOf(&nodes[0]) == "value", "value 解析错误");
    CheckTrue(name, nodes[0].Comment != nullptr && *nodes[0].Comment == " comment",
              "comment 解析错误");
  }
  {
    auto nodes = FromStr("key:value# comment", "", false);
    CheckTrue(name, nodes.size() == 1 && ValueOf(&nodes[0]) == "value", "value 解析错误");
    CheckTrue(name, nodes[0].Comment != nullptr && *nodes[0].Comment == " comment",
              "comment 解析错误");
  }
  {
    auto nodes = FromStr("key: before \\# after # comment", "", false);
    CheckTrue(name, nodes.size() == 1 && ValueOf(&nodes[0]) == "before # after",
              "转义 # 解析错误");
    CheckTrue(name, nodes[0].Comment != nullptr && *nodes[0].Comment == " comment",
              "comment 解析错误");
  }
  {
    auto nodes = FromStr("key:#", "", false);
    CheckTrue(name, nodes.size() == 1 && nodes[0].Value.Value == nullptr, "value 应为 null");
    CheckTrue(name, nodes[0].Comment != nullptr && nodes[0].Comment->empty(),
              "comment 应为空串(非 null)");
  }
  {
    auto nodes = FromStr("key:", "", false);
    CheckTrue(name, nodes.size() == 1 && nodes[0].Value.Value == nullptr, "value 应为 null");
    CheckTrue(name, nodes[0].Comment == nullptr, "comment 应为 null");
  }
  {
    auto nodes = FromStr(" : value", "", false);
    CheckTrue(name, nodes.size() == 1 && nodes[0].Key == nullptr, "key 应为 null");
    CheckTrue(name, ValueOf(&nodes[0]) == "value", "value 解析错误");
    CheckTrue(name, nodes[0].Comment == nullptr, "comment 应为 null");
  }
}

void TestGuardedWhitespace() {
  auto nodes = FromStr("key:   \\      test value    \\   ");
  CheckTrue("Leading and trailing whitespace can be guarded using a backslash",
            nodes.size() == 1 && ValueOf(&nodes[0]) == "      test value    ",
            "守护空白解析错误");
}

void CommentsShouldCountTowardLineNumbers() {
  const std::string yamlS = "\nTestA:\n\tNothing:\n\n# Comment\nTestB:\n\tNothing:\n";
  {
    auto result = FromStr(yamlS);
    const MiniYamlNode* b = FindNode(result, "TestB");
    CheckTrue("Comments should count toward line numbers", b != nullptr && b->Location.Line == 6,
              b != nullptr
                  ? std::format("期望行 6,实际 {}(discarding)", b->Location.Line)
                  : "缺 TestB");
    CheckTrue("Comments should count toward line numbers",
              result.size() == 2 && result[1].Key != nullptr && *result[1].Key == "TestB",
              "Node TestB should be the second child of the root node, but is not (discarding "
              "comments)");
  }
  {
    auto result = FromStr(yamlS, "", false);
    const MiniYamlNode* b = FindNode(result, "TestB");
    CheckTrue("Comments should count toward line numbers", b != nullptr && b->Location.Line == 6,
              b != nullptr ? std::format("期望行 6,实际 {}(parsing)", b->Location.Line)
                           : "缺 TestB");
    CheckTrue("Comments should count toward line numbers",
              result.size() >= 5 && result[4].Key != nullptr && *result[4].Key == "TestB",
              "Node TestB should be the fifth child of the root node, but is not (parsing "
              "comments)");
  }
}

void CommentsSurviveRoundTrip() {
  // 上游以 .Replace("\r\n","\n") 归一;提取器已按 LF 提取,语义一致
  // Upstream normalises via .Replace("\r\n","\n"); the extractor already yields LF.
  CheckEq("Comments should survive a round trip intact",
          WriteToString(FromStr("\n# Top level comment node\n#\nParent: # comment without value\n\t# Indented comment node\n\t#\n\t\t# Double Indented comment node\n\t\t#\n\t\t\t# Triple Indented comment node\n\t\t\t#\n\tFirst: value containing a \\# character\n\tSecond: value # node with inline comment\n\tThird: value #\n\tFourth: #\n\tFifth# embedded comment:\n\tSixth# embedded comment: still a comment\n\tSeventh# embedded comment: still a comment # more comment\n", "", false)),
          "\n# Top level comment node\n#\nParent: # comment without value\n\t# Indented comment node\n\t#\n\t\t# Double Indented comment node\n\t\t#\n\t\t\t# Triple Indented comment node\n\t\t\t#\n\tFirst: value containing a \\# character\n\tSecond: value # node with inline comment\n\tThird: value #\n\tFourth: #\n\tFifth: # embedded comment:\n\tSixth: # embedded comment: still a comment\n\tSeventh: # embedded comment: still a comment # more comment\n");
}

void CommentsShouldntSurviveRoundTrip() {
  CheckEq("Comments should be removed when discardCommentsAndWhitespace is false",
          WriteToString(FromStr("\n# Top level comment node\n#\nParent: # comment without value\n\t# Indented comment node\n\t#\n\t\t# Double Indented comment node\n\t\t#\n\t\t\t# Triple Indented comment node\n\t\t\t#\n\tFirst: value containing a \\# character\n\tSecond: value # node with inline comment\n\tThird: value #\n\tFourth: #\n\tFifth# embedded comment:\n\tSixth# embedded comment: still a comment\n\tSeventh# embedded comment: still a comment # more comment\n")),
          "Parent:\n\tFirst: value containing a \\# character\n\tSecond: value\n\tThird: value\n\tFourth:\n\tFifth:\n\tSixth:\n\tSeventh:\n");
}

}  // namespace

int main() {
  TestParseRoundtrip();
  TestParseEmptyLines();
  TestIndents();
  NodeRemoval();
  NodeRemovalAndOverride();
  MergedNodeRemoval();
  MergedNodeRemovalAndOverride();
  MergedInheritedNodeRemoval();
  MergedInheritedNodeRemovalAndOverride();
  InheritanceAndRemovalCanBeComposed();
  ChildCanBeRemovedAfterMultipleInheritance();
  InheritedChildCanBeImmediatelyRemoved();
  InheritedChildCanBeRemovedAndImmediatelyOverridden();
  InheritedChildCanBeRemovedAndLaterOverridden();
  InheritedChildCanBeOverriddenThenRemoved();
  MergedChildSubNodeCanBeRemovedAndImmediatelyOverridden();
  MergedChildSubNodeCanBeRemovedAndLaterOverridden();
  InheritedChildSubNodeCanBeRemovedAndImmediatelyOverridden();
  InheritedChildSubNodeCanBeRemovedAndLaterOverridden();
  InheritanceWorksForNestedNodes();
  EmptyLinesShouldCountTowardLineNumbers();
  TestSelfMerging();
  TestSelfMergingMultiSource();
  TestMergeConflictsNoMerge();
  TestDuplicatedRemovals();
  TestMergeConflictsNoMergeWithRemovals();
  TestMergeConflictsNoMergeWithInsufficientRemovals();
  TestMergeMultiSourceWithRemovals();
  TestMergeConflictsFirstParent();
  TestMergeConflictsSecondParent();
  TestMergeConflictsMultiSourceMerge();
  TestMergeConflictsMultiSourceFirstParent();
  TestMergeConflictsMultiSourceSecondParent();
  TestMergeComments();
  TestEscapedHashInValues();
  TestGuardedWhitespace();
  CommentsShouldCountTowardLineNumbers();
  CommentsSurviveRoundTrip();
  CommentsShouldntSurviveRoundTrip();

  std::println("mini_yaml_test: {} checks, {} failures", checks, failures);
  return failures == 0 ? 0 : 1;
}
