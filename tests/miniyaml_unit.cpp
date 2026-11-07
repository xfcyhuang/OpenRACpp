import std;

#include "game/miniyaml.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_MINIYAML_GOLDEN_DIR
#define ORA_MINIYAML_GOLDEN_DIR "tests/golden/miniyaml"
#endif

namespace {


std::string roundtrip(std::string_view str_text) {
  auto res_nodes = ora::game::miniyaml_from_string(str_text, "unit", false);
  if (!res_nodes.has_value()) {
    return std::string{"ERROR: "} + res_nodes.error();
  }
  return ora::game::miniyaml_write_to_string(*res_nodes);
}

std::string roundtrip_discard(std::string_view str_text) {
  auto res_nodes = ora::game::miniyaml_from_string(str_text, "unit", true);
  if (!res_nodes.has_value()) {
    return std::string{"ERROR: "} + res_nodes.error();
  }
  return ora::game::miniyaml_write_to_string(*res_nodes);
}

}

int main() {
  {
    auto res_empty = ora::game::miniyaml_from_string("", "unit", false);
    check(res_empty.has_value() && res_empty->empty(), "empty input yields no nodes");
    check(roundtrip("") == "\n", "empty input roundtrips to single newline");
  }
  {
    auto res_nodes = ora::game::miniyaml_from_string("a: v\n", "unit", false);
    check(res_nodes.has_value() && res_nodes->size() == 1, "single line yields one node");
    if (res_nodes.has_value() && res_nodes->size() == 1) {
      const auto& node_first = (*res_nodes)[0];
      check(node_first.str_key == "a", "key parsed");
      check(node_first.str_value == "v", "value parsed");
      check(!node_first.opt_comment.has_value(), "no comment");
      check(node_first.vec_children.empty(), "no children");
      check(node_first.loc_location.str_name == "unit", "location name");
      check(node_first.loc_location.int4_line == 1, "location line is 1-based");
    }
    check(roundtrip("a: v\n") == "a: v\n", "key: value roundtrip identity");
  }
  {
    auto res_nodes = ora::game::miniyaml_from_string("a:\n\tb: v\n\tc: w\n\td: x\n", "unit", false);
    check(res_nodes.has_value() && res_nodes->size() == 1, "parent yields single top node");
    if (res_nodes.has_value() && res_nodes->size() == 1) {
      const auto& node_root = (*res_nodes)[0];
      check(node_root.str_key == "a" && node_root.str_value.empty(), "parent key without value");
      check(node_root.vec_children.size() == 3, "parent has three children");
      if (node_root.vec_children.size() == 3) {
        check(node_root.vec_children[0].str_key == "b" && node_root.vec_children[0].str_value == "v",
              "first child");
        check(node_root.vec_children[0].loc_location.int4_line == 2, "child location line");
        check(node_root.vec_children[2].str_key == "d", "last child");
      }
    }
    check(roundtrip("a:\n\tb: v\n\tc: w\n\td: x\n") == "a:\n\tb: v\n\tc: w\n\td: x\n",
          "tab-indented tree roundtrip identity");
  }
  {
    auto res_nodes = ora::game::miniyaml_from_string("a:\n\tb: v\n\nc: w\n", "unit", false);
    check(res_nodes.has_value() && res_nodes->size() == 3, "dedent closes subtree, blank line kept");
    if (res_nodes.has_value() && res_nodes->size() == 3) {
      check((*res_nodes)[0].str_key == "a" && (*res_nodes)[0].vec_children.size() == 1,
            "first top node owns child");
      check((*res_nodes)[1].str_key.empty(), "blank line becomes null-key node");
      check((*res_nodes)[2].str_key == "c", "node after blank line is top level");
    }
    check(roundtrip("a:\n\tb: v\n\nc: w\n") == "a:\n\tb: v\n\nc: w\n", "blank inside tree identity");
  }
  check(roundtrip("a:\n    b: v\n    c:\n        d: w\n") == "a:\n\tb: v\n\tc:\n\t\td: w\n",
        "four spaces equal one level written as tab");
  check(roundtrip("a:\n  b: v\n") == "a:\nb: v\n", "two spaces do not make a level");
  check(roundtrip("a:\n     b: v\n\tc: w\n") == "a:\n\tb: v\n\tc: w\n", "five mixed spaces normalize");
  check(roundtrip("1:\n\t2: Test\n\t3: # Test\n\t4:\n\t\t4.1:\n\t5: Test\n\t\t5.1:\n\t6: # Test\n\t\t6.1:\n"
                  "\t7:\n\t\t7.1.1:\n\t\t7.1.2: Test\n\t\t7.1.3: # Test\n\t8: Test\n\t\t8.1.1:\n"
                  "\t\t8.1.2: Test\n\t\t8.1.3: # Test\n\t9: # Test\n\t\t9.1.1:\n\t\t9.1.2: Test\n"
                  "\t\t9.1.3: # Test\n") ==
            "1:\n\t2: Test\n\t3: # Test\n\t4:\n\t\t4.1:\n\t5: Test\n\t\t5.1:\n\t6: # Test\n\t\t6.1:\n"
            "\t7:\n\t\t7.1.1:\n\t\t7.1.2: Test\n\t\t7.1.3: # Test\n\t8: Test\n\t\t8.1.1:\n"
            "\t\t8.1.2: Test\n\t\t8.1.3: # Test\n\t9: # Test\n\t\t9.1.1:\n\t\t9.1.2: Test\n"
            "\t\t9.1.3: # Test\n",
        "upstream roundtrip test vector");
  check(roundtrip("bare\n") == "bare:\n", "bare entry gains colon");
  check(roundtrip("key:value\n") == "key: value\n", "missing space after colon normalized");
  check(roundtrip("key : value\n") == "key: value\n", "space before colon trimmed");
  check(roundtrip("key:  value  \n") == "key: value\n", "value trimmed");
  check(roundtrip("a: b: c\n") == "a: b: c\n", "value containing colon");
  check(roundtrip("a: v") == "a: v\n", "missing EOF newline restored");
  {
    auto res_nodes = ora::game::miniyaml_from_string("key: value # comment\n", "unit", false);
    check(res_nodes.has_value() && res_nodes->size() == 1, "inline comment node");
    if (res_nodes.has_value() && res_nodes->size() == 1)
      check((*res_nodes)[0].opt_comment.has_value() && (*res_nodes)[0].opt_comment == " comment",
            "comment keeps leading space, untrimmed");
  }
  check(roundtrip("key:#\n") == "key: #\n", "lone hash yields empty comment, not none");
  check(roundtrip("#\n") == "#\n", "lone hash line survives");
  check(roundtrip("key: v #\n") == "key: v #\n", "trailing lone hash survives");
  check(roundtrip("#comment\nkey: value #c\n") == "#comment\nkey: value #c\n", "comment lines identity");
  check(roundtrip("key: v # comment \n") == "key: v # comment \n", "comment trailing space kept");
  check(roundtrip("a:\n    #comment\n    b: v\n") == "a:\n\t#comment\n\tb: v\n",
        "indented comment gets tab level");
  check(roundtrip("Fifth# embedded comment:\n") == "Fifth: # embedded comment:\n",
        "hash takes precedence over colon");
  check(roundtrip("Sixth# embedded comment: still a comment\n") ==
            "Sixth: # embedded comment: still a comment\n",
        "rest of line after hash is comment");
  check(roundtrip("Seventh# embedded comment: still a comment # more comment\n") ==
            "Seventh: # embedded comment: still a comment # more comment\n",
        "second hash stays inside comment");
  check(roundtrip("# c: v\n") == "# c: v\n", "colon inside whole-line comment");
  check(roundtrip("a: v # comment: x\n") == "a: v # comment: x\n", "colon inside inline comment");
  check(roundtrip("Fifth\\# x: v\n") == "Fifth\\# x: v\n", "escaped hash in key not unescaped");
  {
    auto res_nodes = ora::game::miniyaml_from_string("Fifth\\# x: v\n", "unit", false);
    check(res_nodes.has_value() && res_nodes->size() == 1 && (*res_nodes)[0].str_key == "Fifth\\# x",
          "escaped hash stays part of key");
  }
  check(roundtrip("a: x\\#y\n") == "a: x\\#y\n", "escaped hash in value re-escaped on write");
  check(roundtrip("b: x#y\n") == "b: x #y\n", "unescaped hash becomes comment");
  check(roundtrip("key: \\ v \\\n") == "key:  v \n", "whitespace guards stripped");
  check(roundtrip("key2:   \\      test value    \\   \n") == "key2:       test value    \n",
        "guarded inner value from oracle");
  check(roundtrip("key3: \\x\n") == "key3: \\x\n", "backslash before non-space not stripped");
  check(roundtrip("key4: x\\\n") == "key4: x\\\n", "trailing backslash after non-space kept");
  {
    auto res_bad = ora::game::miniyaml_from_string("a:\n\t\tb: v\n", "test-filename", false);
    check(res_bad.has_value() == false, "indent jump deeper than one throws");
    if (!res_bad.has_value())
      check(res_bad.error() == "Bad indent in miniyaml at test-filename:2",
            "bad indent message matches upstream format");
  }
  {
    auto res_bad = ora::game::miniyaml_from_string("a:\n\tb:\n\n\t\tc: 1\n \n\td: 2\n", "unit", false);
    check(!res_bad.has_value() && res_bad.error() == "Bad indent in miniyaml at unit:4",
          "blank line resets depth context");
  }
  {
    auto res_bad = ora::game::miniyaml_from_string("\na:\n\t\tb: v\n", "unit", false);
    check(!res_bad.has_value() && res_bad.error() == "Bad indent in miniyaml at unit:3",
          "blank and comment lines count toward line numbers");
  }
  check(roundtrip("\t\ta: v\n") == "\n", "orphan deep first line silently dropped");
  check(roundtrip("\ta: v\n\tb: w\n") == "\n", "orphan level-1 first lines dropped");
  check(roundtrip("a:\n\n\tb: v\n") == "a:\n\n\tb: v\n", "blank line steals subtree but text unchanged");
  check(roundtrip("a: v\r\n\tb: w\r\n") == "a: v\n\tb: w\n", "crlf normalized to lf");
  check(roundtrip("a: x\rz\n") == "a: x\rz\n", "isolated carriage return kept in value");
  check(roundtrip("a: v\n\n\n\n") == "a: v\n", "trailing blank lines collapse to single newline");
  check(roundtrip("\n") == "\n", "only newline input");
  check(roundtrip("a:\n   \nb: v\n") == "a:\n\nb: v\n", "spaces-only line becomes empty line");
  check(roundtrip(" : value\n") == " value\n", "null key with value writes leading space");
  check(roundtrip(" # c\n : value\n") == "# c\n value\n", "comment then null-key value line");
  check(roundtrip("-Key: v\n") == "-Key: v\n", "removal prefix key is plain key");
  check(roundtrip("a: 1\na: 2\n") == "a: 1\na: 2\n", "duplicate keys kept verbatim");
  {
    check(roundtrip_discard("\n1:\n\n2: Test\n\n3: # Test\n\n4:\n\n\t4.1:\n\n\n5: Test\n\n"
                            "\t5.1:\n\n6: # Test\n\n\t6.1:\n\n7:\n\n\t7.1.1:\n\n"
                            "\t7.1.2: Test\n\n\t7.1.3: # Test\n\n8: Test\n\n\t8.1.1:\n\n"
                            "\t8.1.2: Test\n\n\t8.1.3: # Test\n\n9: # Test\n\n\t9.1.1:\n\n"
                            "\t9.1.2: Test\n\n\t9.1.3: # Test\n\n") ==
              "1:\n2: Test\n3:\n4:\n\t4.1:\n5: Test\n\t5.1:\n6:\n\t6.1:\n7:\n"
              "\t7.1.1:\n\t7.1.2: Test\n\t7.1.3:\n8: Test\n\t8.1.1:\n\t8.1.2: Test\n"
              "\t8.1.3:\n9:\n\t9.1.1:\n\t9.1.2: Test\n\t9.1.3:\n",
          "upstream TestParseEmptyLines vector");
  }
  {
    check(roundtrip_discard("\nRoot1:\n\tChild1:\n\t\tAttribute1: Test\n\t\tAttribute2: Test\n"
                            "\tChild2:\n\t\tAttribute1: Test\n\t\tAttribute2: Test\n"
                            "Root2:\n\tChild1:\n\t\tAttribute1: Test\n") ==
              roundtrip_discard("\nRoot1:\n    Child1:\n        Attribute1: Test\n"
                                "        Attribute2: Test\n\tChild2:\n\t\tAttribute1: Test\n"
                                "\t    Attribute2: Test\nRoot2:\n    Child1:\n"
                                "\t\tAttribute1: Test\n"),
          "upstream TestIndents mixed tabs and spaces equal tabs");
  }
  {
    auto res_lines =
        ora::game::miniyaml_from_string("\nTestA:\n\tNothing:\n\nTestB:\n\tNothing:\n", "unit", false);
    check(res_lines.has_value() && res_lines->size() == 4, "upstream line-number fixture parses");
    if (res_lines.has_value() && res_lines->size() == 4) {
      const auto& node_test_b{(*res_lines)[3]};
      check(node_test_b.str_key == "TestB" && node_test_b.loc_location.int4_line == 5,
            "upstream EmptyLinesShouldCountTowardLineNumbers: blank lines count");
      check(node_test_b.vec_children.size() == 1 &&
                node_test_b.vec_children[0].str_key == "Nothing" &&
                node_test_b.vec_children[0].loc_location.int4_line == 6,
            "child line number after blank line");
    }
  }
  {
    auto res_discard =
        ora::game::miniyaml_from_string("key: value # c\n# only comment\n", "unit", true);
    check(res_discard.has_value(), "discard mode parses");
    if (res_discard.has_value()) {
      check(ora::game::miniyaml_write_to_string(*res_discard) == "key: value\n",
            "discard mode drops comments and blank lines");
    }
  }
  {
    const std::string str_bom_path{std::string{ORA_MINIYAML_GOLDEN_DIR} + "/edge/bom.yaml"};
    auto res_nodes = ora::game::miniyaml_from_file(str_bom_path, false);
    check(res_nodes.has_value() && res_nodes->size() == 1, "from_file strips utf-8 bom");
    if (res_nodes.has_value() && res_nodes->size() == 1) {
      check((*res_nodes)[0].str_key == "a" && (*res_nodes)[0].str_value == "v", "bom file content");
      check(ora::game::miniyaml_write_to_string(*res_nodes) == "a: v\n", "bom file roundtrip");
    }
  }
  {
    const std::string str_crlf_path{std::string{ORA_MINIYAML_GOLDEN_DIR} + "/edge/crlf.yaml"};
    auto res_nodes = ora::game::miniyaml_from_file(str_crlf_path, false);
    check(res_nodes.has_value(), "from_file parses crlf file");
    if (res_nodes.has_value())
      check(ora::game::miniyaml_write_to_string(*res_nodes) == "a: v\n\tb: w\n",
            "crlf file roundtrip");
  }
  {
    auto res_missing = ora::game::miniyaml_from_file("no/such/file.yaml", false);
    check(!res_missing.has_value(), "missing file reports error");
  }

  return ora::test::test_exit_code("miniyaml_unit");
}
