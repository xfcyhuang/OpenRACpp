import std;

import ora.game;
import ora.game.miniyaml;
import ora.test.util;
using ora::test::check;

#ifndef ORA_RULES_GOLDEN_DIR
#define ORA_RULES_GOLDEN_DIR "tests/golden/rules"
#endif
#ifndef ORA_MINIYAML_GOLDEN_DIR
#define ORA_MINIYAML_GOLDEN_DIR "tests/golden/miniyaml"
#endif

namespace {


std::expected<std::vector<ora::game::MiniYamlNode>, std::string> parse(
    std::string_view str_text) {
  return ora::game::miniyaml_from_string(str_text, "m", true);
}

std::optional<std::string> read_file_bytes(const std::string& str_path) {
  std::ifstream fs_file{str_path, std::ios::binary};
  if (!fs_file) return std::nullopt;
  std::string str_text;
  fs_file.seekg(0, std::ios::end);
  const std::streampos pos_end{fs_file.tellg()};
  str_text.resize(static_cast<std::size_t>(pos_end));
  fs_file.seekg(0, std::ios::beg);
  fs_file.read(str_text.data(), static_cast<std::streamsize>(str_text.size()));
  return str_text;
}

std::vector<std::string> split_lines(std::string_view str_text) {
  std::vector<std::string> vec_lines;
  std::size_t pos{0};
  while (pos < str_text.size()) {
    const std::size_t idx_nl{str_text.find('\n', pos)};
    if (idx_nl == std::string_view::npos) {
      if (pos < str_text.size()) vec_lines.emplace_back(str_text.substr(pos));
      break;
    }
    vec_lines.emplace_back(str_text.substr(pos, idx_nl - pos));
    pos = idx_nl + 1;
  }
  return vec_lines;
}

}

int main() {
  {
    auto res_empty = ora::game::miniyaml_merge({});
    check(res_empty.has_value() && res_empty->empty(), "empty sources merge to empty");
  }
  {

    auto src = parse("A: 1\nB:\n\tC: x\n");
    check(src.has_value(), "slice2 parse ok");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value(), "single file passthrough succeeds");
    if (res.has_value() && res->size() == 2) {
      check((*res)[0].str_key == "A" && (*res)[0].str_value == "1",
            "top value kept on passthrough");
      check((*res)[0].vec_children.empty(), "leaf has no children");
      check((*res)[1].str_key == "B" && (*res)[1].str_value.empty(),
            "second key kept without value");
      check((*res)[1].vec_children.size() == 1 &&
                (*res)[1].vec_children[0].str_key == "C" &&
                (*res)[1].vec_children[0].str_value == "x",
            "subtree kept on passthrough");
    } else {
      check(false, "single file passthrough yields two top nodes");
    }
  }

  {

    auto src = parse("A:\n\tT: 1\nA:\n\tU: 2\n");
    check(src.has_value(), "self-partial parse ok");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 1, "duplicate in-file top keys merge to one");
    if (res.has_value() && res->size() == 1) {
      const auto& node_a{(*res)[0]};
      check(node_a.str_key == "A" && node_a.vec_children.size() == 2 &&
                node_a.vec_children[0].str_key == "T" &&
                node_a.vec_children[1].str_key == "U",
            "self-partial merges subtrees of duplicate keys");
    }
  }
  {

    auto f1 = parse("A: 1\nB:\n\tC: x\n");
    auto f2 = parse("A: 2\nB:\n\tD: y\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(res.has_value() && res->size() == 2, "cross-file merge keeps two tops");
    if (res.has_value() && res->size() == 2) {
      check((*res)[0].str_key == "A" && (*res)[0].str_value == "2",
            "override non-empty value wins");
      check((*res)[1].str_key == "B" && (*res)[1].vec_children.size() == 2 &&
                (*res)[1].vec_children[0].str_key == "C" &&
                (*res)[1].vec_children[1].str_key == "D",
            "deep merge appends new child keys");
    }
  }
  {

    auto f1 = parse("X:\n\tT:\n\t\tV: a\n\t\tW: b\n");
    auto f2 = parse("X:\n\tT:\n\t\tW: c\n\t\tU: d\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(res.has_value() && res->size() == 1, "redefined trait merges");
    if (res.has_value() && res->size() == 1 && (*res)[0].vec_children.size() == 1) {
      const auto& node_t{(*res)[0].vec_children[0]};
      check(node_t.str_key == "T" && node_t.vec_children.size() == 3, "grandchild union");
      if (node_t.vec_children.size() == 3) {
        check(node_t.vec_children[0].str_key == "V" && node_t.vec_children[0].str_value == "a",
              "existing grandchild kept");
        check(node_t.vec_children[1].str_key == "W" && node_t.vec_children[1].str_value == "c",
              "override grandchild value wins");
        check(node_t.vec_children[2].str_key == "U", "new grandchild appended");
      }
    }
  }
  {

    auto src = parse("^P:\n\tT1: a\n\tT2: b\nA:\n\tInherits: ^P\n\tT3: c\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 2, "abstract parent kept in output");
    if (res.has_value() && res->size() == 2) {
      check((*res)[1].str_key == "A" && (*res)[1].vec_children.size() == 3 &&
                (*res)[1].vec_children[0].str_key == "T1" &&
                (*res)[1].vec_children[1].str_key == "T2" &&
                (*res)[1].vec_children[2].str_key == "T3",
            "parent traits insert at Inherits statement");
      check((*res)[0].str_key == "^P" && (*res)[0].vec_children.size() == 2,
            "abstract parent children untouched");
    }
  }
  {

    auto src = parse("^P1:\n\tS1: a\n^P2:\n\tS2: b\nA:\n\tInherits: ^P1\n\tInherits@2: ^P2\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 3, "multi-parent parse");
    if (res.has_value() && res->size() == 3) {
      const auto& node_a{(*res)[2]};
      check(node_a.str_key == "A" && node_a.vec_children.size() == 2 &&
                node_a.vec_children[0].str_key == "S1" &&
                node_a.vec_children[1].str_key == "S2",
            "multiple Inherits expand in statement order");
    }
  }
  {

    auto src = parse("^P:\n\tT:\n\t\tV: old\nA:\n\tInherits: ^P\n\tT:\n\t\tV: new\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 2, "inherit override parse");
    if (res.has_value() && res->size() == 2) {
      const auto& node_a{(*res)[1]};
      check(node_a.vec_children.size() == 1 &&
                node_a.vec_children[0].str_key == "T" &&
                node_a.vec_children[0].vec_children.size() == 1 &&
                node_a.vec_children[0].vec_children[0].str_value == "new",
            "inherited trait merges with child override");
    }
  }
  {

    auto src = parse("^P:\n\tT1: a\n\tT2@x: b\nA:\n\tInherits: ^P\n\t-T1\n\t-T2@x\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 2, "strong removal parse");
    if (res.has_value() && res->size() == 2)
      check((*res)[1].str_key == "A" && (*res)[1].vec_children.empty(),
            "strong removal matches full key including @ suffix");
  }
  {

    auto src = parse("A:\n\t-Ghost\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(!res.has_value(), "removal without match fails");
    if (!res.has_value())
      check(res.error() == "m:2: There are no elements with key `Ghost` to remove",
            "no-elements removal message matches upstream format");
  }
  {

    auto src = parse("A:\n\tInherits: ^Nope\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(!res.has_value(), "missing parent fails");
    if (!res.has_value())
      check(res.error() == "m:2: Parent type `^Nope` not found",
            "parent-not-found message matches upstream format");
  }
  {

    auto src = parse("A:\n\tInherits: A\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(!res.has_value(), "self-inheritance fails");
    if (!res.has_value())
      check(res.error() ==
                "m:2: Parent type `A` was already inherited by this yaml tree at :0 "
                "(note: may be from a derived tree)",
            "cycle message matches upstream format with default seed location");
  }
  {

    auto src = parse("A:\n\tInherits: B\nB:\n\tInherits: A\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(!res.has_value(), "indirect cycle fails");
    if (!res.has_value())
      check(res.error() ==
                "m:4: Parent type `A` was already inherited by this yaml tree at :0 "
                "(note: may be from a derived tree)",
            "indirect cycle reports at inheriting line");
  }
  {

    auto f1 = parse("A:\n\tT: 1\nB:\n\tT: 2\n");
    auto f2 = parse("-A\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(res.has_value() && res->size() == 1 && (*res)[0].str_key == "B",
          "top-level removal removes whole actor");
  }
  {

    auto f1 = parse("X:\n\tT: a\n\t-T\n\tT: b\n");
    auto f2 = parse("X:\n\tU: c\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(res.has_value() && res->size() == 1, "weak-removal tolerance parse");
    if (res.has_value() && res->size() == 1) {
      const auto& node_x{(*res)[0]};
      check(node_x.str_key == "X" && node_x.vec_children.size() == 2 &&
                node_x.vec_children[0].str_key == "T" &&
                node_x.vec_children[0].str_value == "b" &&
                node_x.vec_children[1].str_key == "U",
            "K,-K,K applies old then removes then applies new");
    }
  }
  {

    auto f1 = parse("X:\n\tT: 1\n\t-T\n\tT: 2\n\tT: 3\n");
    auto f2 = parse("X:\n\tU: 0\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(!res.has_value(), "duplicate keys after removal fail");
    if (!res.has_value())
      check(res.error() ==
                "MiniYaml.Merge, duplicate values found for the following keys: "
                "T: [T (at m:4),T (at m:5)]",
            "duplicate-keys message matches upstream format");
  }
  {

    auto src = parse("A:\n\tW: 1\n\tW@x: 2\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 1, "at-suffix parse");
    if (res.has_value() && res->size() == 1)
      check((*res)[0].vec_children.size() == 2 && (*res)[0].vec_children[1].str_key == "W@x",
            "at-suffixed trait keys kept verbatim");
  }
  {

    auto f1 = parse("B: 1\nA: 2\n");
    auto f2 = parse("C: 3\n");
    auto res = ora::game::miniyaml_merge({*f1, *f2});
    check(res.has_value() && res->size() == 3, "order parse");
    if (res.has_value() && res->size() == 3)
      check((*res)[0].str_key == "B" && (*res)[1].str_key == "A" && (*res)[2].str_key == "C",
            "first-seen order preserved");
  }
  {

    auto src = parse("^P:\n\tInherits: ^Q\n\tT3: c\n^Q:\n\tT1: a\nA:\n\tInherits: ^P\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 3, "inherit chain parse");
    if (res.has_value() && res->size() == 3) {
      const auto& node_a{(*res)[2]};
      check(node_a.str_key == "A" && node_a.vec_children.size() == 2 &&
                node_a.vec_children[0].str_key == "T1" &&
                node_a.vec_children[1].str_key == "T3",
            "inheritance resolves recursively through parents");
    }
  }
  {

    auto src = parse("^P:\n\tT: parent\nA:\n\tInherits: ^P\n\tT: child\n");
    auto res = ora::game::miniyaml_merge({*src});
    check(res.has_value() && res->size() == 2, "inherit value override parse");
    if (res.has_value() && res->size() == 2)
      check((*res)[1].vec_children.size() == 1 &&
                (*res)[1].vec_children[0].str_value == "child",
            "inherited scalar value overridden by child");
  }

  {

    const std::string str_rules_dir{ORA_RULES_GOLDEN_DIR};
    const auto opt_golden{read_file_bytes(str_rules_dir + "/ra-manifest.txt")};
    check(opt_golden.has_value(), "golden manifest readable");
    auto res = ora::game::manifest_rules_files(str_rules_dir + "/mod.yaml");
    check(res.has_value(), "manifest extraction succeeds");
    if (res.has_value() && opt_golden.has_value()) {
      const std::vector<std::string> vec_golden{split_lines(*opt_golden)};
      check(res->size() == 16, "manifest lists 16 rule files");
      check(*res == vec_golden, "manifest list matches oracle golden line by line");
      if (!vec_golden.empty())
        check((*res)[0] == "ra|rules/misc.yaml" &&
                  (*res)[15] == "ra|rules/map-generators.yaml",
              "manifest order matches mod.yaml");
    }
  }
  {

    auto res_empty =
        ora::game::manifest_rules_files(std::string{ORA_MINIYAML_GOLDEN_DIR} + "/edge/empty.yaml");
    check(res_empty.has_value() && res_empty->empty(),
          "missing Rules section yields empty list");
    auto res_missing = ora::game::manifest_rules_files("no/such/mod.yaml");
    check(!res_missing.has_value(), "missing mod.yaml reports error");
  }

  {

    const std::string str_rules_dir{ORA_RULES_GOLDEN_DIR};
    const std::string str_ra_dir{std::string{ORA_MINIYAML_GOLDEN_DIR} + "/ra"};

    auto res_manifest{ora::game::manifest_rules_files(str_rules_dir + "/mod.yaml")};
    check(res_manifest.has_value() && res_manifest->size() == 16,
          "ra manifest yields 16 entries");
    if (!res_manifest.has_value() || res_manifest->size() != 16) {
      std::println("rules_load: aborting golden section (manifest broken)");
    } else {
      std::vector<std::pair<std::string, std::string>> vec_entries;
      vec_entries.reserve(res_manifest->size());
      for (const std::string& str_entry : *res_manifest) {
        const std::size_t idx_slash{str_entry.rfind('/')};
        vec_entries.emplace_back(str_entry,
                                 str_ra_dir + "/" + str_entry.substr(idx_slash + 1));
      }

      auto res_model{ora::game::ruleset_load_minimal(vec_entries)};
      check(res_model.has_value(), "ra rules load succeeds with zero failures");
      if (res_model.has_value()) {
        const auto& vec_actors{res_model->vec_actors};
        check(vec_actors.size() == 307, "actor count is 307");
        std::size_t sz_traits{0};
        for (const auto& actor : vec_actors) sz_traits += actor.vec_trait_keys.size();
        check(sz_traits == 13850, "trait instance count is 13850");

        const auto opt_stats{read_file_bytes(str_rules_dir + "/ra-stats.txt")};
        check(opt_stats.has_value(), "golden stats readable");
        if (opt_stats.has_value()) {
          const std::vector<std::string> vec_lines{split_lines(*opt_stats)};

          std::size_t sz_golden_actors{0};
          bool b_all_match{true};
          bool b_counts_match{true};
          for (const std::string& str_line : vec_lines) {
            if (str_line.empty() || str_line[0] == '#') continue;
            const std::size_t idx_space{str_line.rfind(' ')};
            if (idx_space == std::string::npos || sz_golden_actors >= vec_actors.size()) {
              b_all_match = false;
              break;
            }
            const auto& actor{vec_actors[sz_golden_actors]};
            if (actor.str_name != str_line.substr(0, idx_space)) b_all_match = false;
            unsigned u4_count{0};
            const std::string_view sv_count{str_line};
            std::from_chars(sv_count.data() + idx_space + 1, sv_count.data() + sv_count.size(),
                            u4_count);
            if (actor.vec_trait_keys.size() != static_cast<std::size_t>(u4_count))
              b_counts_match = false;
            ++sz_golden_actors;
          }
          check(sz_golden_actors == vec_actors.size() && b_all_match,
                "per-actor names match oracle golden in merge order");
          check(b_counts_match, "per-actor trait counts match oracle golden");
        }
      }

      std::vector<std::vector<ora::game::MiniYamlNode>> vec_sources;
      for (const std::string& str_entry : *res_manifest) {
        const std::size_t idx_slash{str_entry.rfind('/')};
        auto res_file{ora::game::miniyaml_from_file_named(
            str_ra_dir + "/" + str_entry.substr(idx_slash + 1), str_entry, true)};
        check(res_file.has_value(), "rule file parses: " + str_entry);
        if (res_file.has_value()) vec_sources.push_back(std::move(*res_file));
      }
      auto res_merged{ora::game::miniyaml_merge(vec_sources)};
      check(res_merged.has_value(), "ra merge succeeds");
      if (res_merged.has_value()) {
        std::size_t sz_abstract{0};
        std::size_t sz_inherits{0};
        std::size_t sz_removals{0};
        for (const auto& node : *res_merged) {
          if (node.str_key.starts_with('^')) ++sz_abstract;
          for (const auto& child : node.vec_children) {
            if (child.str_key == "Inherits" || child.str_key.starts_with("Inherits@"))
              ++sz_inherits;
            if (child.str_key.starts_with('-')) ++sz_removals;
          }
        }
        check(res_merged->size() == 375, "merged tree has 375 top-level nodes");
        check(sz_abstract == 68, "merged tree has 68 abstract ^ nodes");
        check(sz_inherits == 0, "no Inherits nodes survive the merge");
        check(sz_removals == 0, "no removal markers survive the merge");

        const std::string str_written{ora::game::miniyaml_write_to_string(*res_merged)};
        const auto opt_golden{read_file_bytes(str_rules_dir + "/ra-merged.yaml")};
        check(opt_golden.has_value(), "golden merged tree readable");
        if (opt_golden.has_value()) {
          const std::string& str_golden{*opt_golden};
          if (str_written == str_golden) {
            check(str_written.size() == 1025317, "merged tree byte count is 1025317");
          } else {
            std::size_t sz_diff{0};
            const std::size_t sz_min{str_written.size() < str_golden.size()
                                         ? str_written.size()
                                         : str_golden.size()};
            while (sz_diff < sz_min && str_written[sz_diff] == str_golden[sz_diff]) ++sz_diff;
            check(false, std::format("merged tree differs from golden at offset {} ({} vs {} bytes)",
                                     sz_diff, str_written.size(), str_golden.size()));
          }
        }
      }
    }
  }

  return ora::test::test_exit_code("rules_load");
}
