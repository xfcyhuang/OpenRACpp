import std;

#include <algorithm>
#include <sstream>

#include "game/input/hotkey.hpp"
#include "game/input/hotkey_actions.hpp"
#include "game/input/hotkey_registry.hpp"
#include "game/input/selection.hpp"
#include "game/world/world.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_HOTKEYS_GOLDEN_DIR
#define ORA_HOTKEYS_GOLDEN_DIR "tests/golden/hotkeys"
#endif

namespace {

namespace wig = ora::game::input;
namespace ww = ora::game::world;

std::string read_text_file(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  if (!fs) return {};
  std::ostringstream ss;
  ss << fs.rdbuf();
  return ss.str();
}

std::vector<std::string> split_lines(const std::string& str_text) {
  std::vector<std::string> vec_lines;
  std::size_t idx_begin{0};
  while (idx_begin <= str_text.size()) {
    const std::size_t idx_eol{str_text.find('\n', idx_begin)};
    std::string_view str_line{std::string_view{str_text}.substr(
        idx_begin, idx_eol == std::string::npos ? std::string_view::npos
                                                : idx_eol - idx_begin)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.remove_suffix(1);
    vec_lines.emplace_back(str_line);
    if (idx_eol == std::string::npos) break;
    idx_begin = idx_eol + 1;
  }
  return vec_lines;
}

std::vector<std::string> golden_data_lines(const std::string& str_text) {
  std::vector<std::string> vec_out;
  for (const std::string& str_line : split_lines(str_text))
    if (!str_line.empty() && str_line[0] != '#') vec_out.push_back(str_line);
  return vec_out;
}

std::string cpp_dump_table(const wig::HotkeyRegistryModel& reg) {
  std::string str_out;
  for (std::size_t i{0}; i < reg.vec_definitions.size(); ++i) {
    const wig::HotkeyDefinitionModel& def{reg.vec_definitions[i]};
    str_out += std::format("{}\t{}\t{}\t{}\n", def.str_name,
                           wig::keycode_name(reg.vec_keys[i].key),
                           wig::modifiers_to_string(reg.vec_keys[i].mods),
                           def.b_has_duplicates ? "True" : "False");
  }
  str_out += std::format("# total={} unbound={} readonly={} duplicates={}\n",
                         reg.vec_definitions.size(),
                         std::count_if(reg.vec_keys.begin(), reg.vec_keys.end(),
                                       [](const wig::HotkeyModel& hk) {
                                         return !wig::hotkey_is_valid(hk);
                                       }),
                         std::count_if(reg.vec_definitions.begin(), reg.vec_definitions.end(),
                                       [](const wig::HotkeyDefinitionModel& def) {
                                         return def.b_readonly;
                                       }),
                         std::count_if(reg.vec_definitions.begin(), reg.vec_definitions.end(),
                                       [](const wig::HotkeyDefinitionModel& def) {
                                         return def.b_has_duplicates;
                                       }));
  return str_out;
}

// —— 解析/序列化/UNKNOWN 哨兵语义（笔记 §1.1/§7-1/§7-2） ——
void test_hotkey_parse_and_equal() {
  auto res_plain{wig::hotkey_try_parse("H")};
  check(res_plain.has_value() && res_plain->key == wig::Keycode::kH &&
            res_plain->mods == wig::ModifierKeys::kNone,
        "parse: bare key binds with None mods");

  auto res_mods{wig::hotkey_try_parse("NUMBER_1 Ctrl, Shift")};
  check(res_mods.has_value() &&
            res_mods->key == wig::Keycode::kNumber1 &&
            res_mods->mods == (wig::ModifierKeys::kCtrl | wig::ModifierKeys::kShift),
        "parse: comma-space flags combine");

  auto res_upper{wig::hotkey_try_parse("F1 CTRL")};
  check(res_upper.has_value() && res_upper->key == wig::Keycode::kF1 &&
            res_upper->mods == wig::ModifierKeys::kCtrl,
        "parse: uppercase modifiers accepted (ignoreCase)");

  auto res_lower{wig::hotkey_try_parse("q ctrl")};
  check(res_lower.has_value() && res_lower->key == wig::Keycode::kQ,
        "parse: lowercase key accepted (ignoreCase)");

  auto res_lower_mod{wig::hotkey_try_parse("Tab shift")};
  check(res_lower_mod.has_value() && res_lower_mod->key == wig::Keycode::kTab &&
            res_lower_mod->mods == wig::ModifierKeys::kShift,
        "parse: Tab Shift chat binding");

  check(!wig::hotkey_try_parse("A B C").has_value(), "parse: three tokens rejected");
  check(!wig::hotkey_try_parse("").has_value(), "parse: empty rejected");
  check(!wig::hotkey_try_parse("NOT_A_KEY").has_value(), "parse: unknown keycode rejected");
  check(!wig::hotkey_try_parse("H CTRL, BOGUS").has_value(),
        "parse: unknown modifier token rejected");
  auto res_pad{wig::hotkey_try_parse("H   Ctrl")};
  check(res_pad.has_value() && res_pad->key == wig::Keycode::kH &&
            res_pad->mods == wig::ModifierKeys::kCtrl,
        "parse: leading spaces on mods segment trim like C# Enum.Parse tokens");

  check(wig::hotkey_to_string(*res_mods) == "NUMBER_1 Shift, Ctrl",
        "serialize: canonical KEY Mod1, Mod2 (declaration order, not yaml order)");
  check(wig::hotkey_to_string(wig::kHotkeyInvalid) == "UNKNOWN None",
        "serialize: Invalid dumps UNKNOWN None");
  check(wig::hotkey_to_string(wig::HotkeyModel{wig::Keycode::kP, wig::ModifierKeys::kCtrl}) ==
            "P Ctrl",
        "serialize: P Ctrl");
  check(wig::modifiers_to_string(wig::ModifierKeys::kMeta | wig::ModifierKeys::kShift) ==
            "Shift, Meta",
        "serialize: canonical modifier order Shift, Alt, Ctrl, Meta");

  const wig::HotkeyModel hk_valid{wig::Keycode::kQ, wig::ModifierKeys::kCtrl};
  check(wig::hotkey_equal(hk_valid, hk_valid), "equal: valid vs self");
  check(!wig::hotkey_equal(wig::kHotkeyInvalid, wig::kHotkeyInvalid),
        "equal: Invalid == Invalid is false (upstream operator== quirk)");
  check(!wig::hotkey_equal(wig::kHotkeyInvalid, hk_valid),
        "equal: Invalid vs valid is false");
  check(!wig::hotkey_equal(hk_valid, wig::kHotkeyInvalid),
        "equal: valid vs Invalid is false");
  check(wig::hotkey_equal(wig::HotkeyModel{wig::Keycode::kQ, wig::ModifierKeys::kNone},
                          wig::HotkeyModel{wig::Keycode::kQ, wig::ModifierKeys::kNone}),
        "equal: plain Q == plain Q");
  check(!wig::hotkey_equal(wig::HotkeyModel{wig::Keycode::kQ, wig::ModifierKeys::kNone},
                           wig::HotkeyModel{wig::Keycode::kQ, wig::ModifierKeys::kCtrl}),
        "equal: modifiers must match exactly");

  check(wig::hotkey_is_valid(wig::kHotkeyInvalid) == false, "valid: Invalid is not valid");
  check(wig::hotkey_is_valid(hk_valid), "valid: Q Ctrl is valid");

  wig::KeyInput ki{.event = wig::KeyInputEvent::kDown,
                   .key = wig::Keycode::kQ,
                   .mods = wig::ModifierKeys::kCtrl,
                   .int4_tap_count = 1};
  check(wig::hotkey_is_activated_by(hk_valid, ki), "activate: full match activates");
  check(!wig::hotkey_is_activated_by(hk_valid, wig::KeyInput{.event = wig::KeyInputEvent::kDown,
                                                             .key = wig::Keycode::kQ,
                                                             .mods = wig::ModifierKeys::kCtrl |
                                                                    wig::ModifierKeys::kShift}),
        "activate: extra shift does NOT activate (full-match, not bitmask-includes)");
  check(!wig::hotkey_is_activated_by(wig::kHotkeyInvalid, ki),
        "activate: Invalid reference never activates");
  check(wig::hotkey_from_key_input(ki).key == wig::Keycode::kQ &&
            wig::hotkey_from_key_input(ki).mods == wig::ModifierKeys::kCtrl,
        "from_key_input: (key, mods) pair");

  check(wig::keycode_name(wig::Keycode::kMouse4) == "MOUSE4" &&
            wig::keycode_name(wig::Keycode::kKpEnter) == "KP_ENTER",
        "keycode: extended keys and names (N|(1<<30), MOUSE4)");
  check(wig::keycode_from_name("number_1") == wig::Keycode::kNumber1 &&
            wig::keycode_from_name("UNKNOWN") == wig::Keycode::kUnknown,
        "keycode: case-insensitive name lookup");
}

// —— 黄金对拍：174 定义装载 + 合成 Keys 覆盖段（oracle ra-bindings/ra-override） ——
std::expected<wig::HotkeyRegistryModel, std::string> load_registry_from_golden(
    const std::vector<std::string>& vec_file_keys) {
  std::vector<std::vector<ora::game::MiniYamlNode>> vec_files;
  for (const std::string& str_key : vec_file_keys) {
    std::string str_local{str_key};
    std::replace(str_local.begin(), str_local.end(), '|', '-');
    std::replace(str_local.begin(), str_local.end(), '/', '-');
    auto res_nodes{ora::game::miniyaml_from_file(
        std::format("{}/yaml/{}", ORA_HOTKEYS_GOLDEN_DIR, str_local))};
    if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());
    vec_files.push_back(std::move(*res_nodes));
  }
  return wig::hotkey_registry_load(vec_files);
}

std::vector<std::pair<std::string, std::string>> override_segment_from_header(
    const std::string& str_golden) {
  std::vector<std::pair<std::string, std::string>> vec_out;
  bool b_in_segment{false};
  for (const std::string& str_line : split_lines(str_golden)) {
    if (str_line.starts_with("# synthetic settings Keys: segment")) {
      b_in_segment = true;
      continue;
    }
    if (b_in_segment) {
      if (!str_line.starts_with("#   ")) break;
      std::string_view str_entry{str_line};
      str_entry.remove_prefix(4);
      const std::size_t idx_colon{str_entry.find(':')};
      if (idx_colon != std::string_view::npos)
        vec_out.emplace_back(std::string{str_entry.substr(0, idx_colon)},
                             std::string{str_entry.substr(idx_colon + 2)});
    }
  }
  return vec_out;
}

void test_golden_bindings() {
  const std::string str_files{read_text_file(std::format("{}/files.txt", ORA_HOTKEYS_GOLDEN_DIR))};
  check(!str_files.empty(), "golden: files.txt readable");
  if (str_files.empty()) return;

  std::vector<std::string> vec_file_keys;
  for (const std::string& str_line : split_lines(str_files))
    if (!str_line.empty()) vec_file_keys.push_back(str_line);
  check(vec_file_keys.size() == 9, "golden: 9 hotkey yaml files in load order");

  auto res_reg{load_registry_from_golden(vec_file_keys)};
  check(res_reg.has_value(), "golden: registry loads");
  if (!res_reg.has_value()) {
    std::println("registry load error: {}", res_reg.error());
    return;
  }
  wig::HotkeyRegistryModel reg{std::move(*res_reg)};

  check(reg.vec_definitions.size() == 174, "golden: 174 definitions loaded");

  const std::string str_dump{cpp_dump_table(reg)};
  const std::string str_golden{
      read_text_file(std::format("{}/ra-bindings.txt", ORA_HOTKEYS_GOLDEN_DIR))};
  check(golden_data_lines(str_dump) == golden_data_lines(str_golden),
        "golden: C++ table dump matches oracle ra-bindings.txt line by line");

  check(str_dump.find("# total=174 unbound=10 readonly=2 duplicates=0") != std::string::npos,
        "golden: default table stats 174/10/2/0");

  // 关键边角行（Platform 覆盖的 OSX 行被忽略、Ctrl, Shift 序、空 value 未绑定、
  // 大写 CTRL）由黄金行内容钉死：
  const std::vector<std::string> vec_lines{golden_data_lines(str_golden)};
  const auto find_line{[&vec_lines](std::string_view str_prefix) -> const std::string* {
    for (const std::string& str_line : vec_lines)
      if (str_line.starts_with(str_prefix)) return &str_line;
    return nullptr;
  }};
  check(find_line("ControlGroupAddTo01\t") != nullptr &&
            *find_line("ControlGroupAddTo01\t") == "ControlGroupAddTo01\tNUMBER_1\tShift, Ctrl\tFalse",
        "golden: ControlGroupAddTo01 is NUMBER_1 Shift, Ctrl (OSX Meta row ignored)");
  check(find_line("Production13\t") != nullptr &&
            *find_line("Production13\t") == "Production13\tF1\tCtrl\tFalse",
        "golden: Production13 is F1 CTRL (uppercase CTRL canonicalized)");
  check(find_line("TogglePlayerStanceColor\t") != nullptr &&
            *find_line("TogglePlayerStanceColor\t") ==
                "TogglePlayerStanceColor\tUNKNOWN\tNone\tFalse",
        "golden: empty value binds Invalid (UNKNOWN None)");
  check(find_line("Autocomplete\t") != nullptr &&
            *find_line("Autocomplete\t") == "Autocomplete\tTAB\tNone\tFalse",
        "golden: readonly Autocomplete stays TAB");

  // —— 覆盖段对拍（改键/冲突/Readonly 跳过/UNKNOWN None 覆盖） ——
  const std::string str_override_golden{
      read_text_file(std::format("{}/ra-override.txt", ORA_HOTKEYS_GOLDEN_DIR))};
  const std::vector<std::pair<std::string, std::string>> vec_segment{
      override_segment_from_header(str_override_golden)};
  check(vec_segment.size() == 5, "golden: override segment parsed from header (5 entries)");
  const std::vector<std::string> vec_override_lines{golden_data_lines(str_override_golden)};
  const auto find_override{[&vec_override_lines](std::string_view str_prefix) -> const std::string* {
    for (const std::string& str_line : vec_override_lines)
      if (str_line.starts_with(str_prefix)) return &str_line;
    return nullptr;
  }};
  if (vec_segment.size() == 5) {
    std::vector<ora::game::MiniYamlNode> vec_nodes;
    for (const auto& [str_name, str_value] : vec_segment) {
      ora::game::MiniYamlNode node;
      node.str_key = str_name;
      node.str_value = str_value;
      vec_nodes.push_back(node);
    }
    wig::hotkey_registry_apply_settings(reg, vec_nodes);

    const std::string str_override_dump{cpp_dump_table(reg)};
    check(golden_data_lines(str_override_dump) == golden_data_lines(str_override_golden),
          "golden: override dump matches oracle ra-override.txt line by line");

    const auto* ptr_select_all{find_override("SelectAllUnits\t")};
    check(ptr_select_all != nullptr && *ptr_select_all == "SelectAllUnits\tQ\tCtrl\tTrue",
          "golden: SelectAllUnits rebound to Q Ctrl and flagged duplicate");
    check(find_override("Sell\t") != nullptr &&
              *find_override("Sell\t") == "Sell\tQ\tCtrl\tTrue",
          "golden: Sell Q Ctrl flagged duplicate");
    check(find_override("MapBookmarkSave01\t") != nullptr &&
              *find_override("MapBookmarkSave01\t") == "MapBookmarkSave01\tQ\tCtrl\tTrue",
          "golden: MapBookmarkSave01 (default Q Ctrl) also flagged");
    check(find_override("Autocomplete\t") != nullptr &&
              *find_override("Autocomplete\t") == "Autocomplete\tTAB\tNone\tFalse",
          "golden: readonly Autocomplete override skipped, stays TAB");
  }
}

// —— 合成世界（行为用例 10-12 共用） ——
constexpr std::string_view kRules{
    "player:\n"
    "\tTickCounter:\n"
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "probe.tank:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\tTargetable:\n"
    "\tMobileProbe:\n"
    "probe.wall:\n"
    "\tSelectable:\n"
    "\t\tBounds: 1024,1024\n"
    "probe.am:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\tAttackMoveProbe:\n"};

constexpr std::string_view kMap{
    "Title: hotkeys-map\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Creeps:\n"
    "\t\tName: Creeps\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: soviet\n"
    "\tPlayerReference@P1:\n"
    "\t\tName: P1\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"
    "\ttanka: probe.tank\n"
    "\t\tLocation: 10,10\n"
    "\t\tOwner: Neutral\n"
    "\ttankb: probe.tank\n"
    "\t\tLocation: 20,10\n"
    "\t\tOwner: Neutral\n"
    "\twall0: probe.wall\n"
    "\t\tLocation: 15,13\n"
    "\t\tOwner: Neutral\n"
    "\tam0: probe.am\n"
    "\t\tLocation: 25,10\n"
    "\t\tOwner: Neutral\n"};

constexpr std::uint32_t kViewerPlayer{0};
constexpr std::uint32_t kTankA{5};
constexpr std::uint32_t kTankB{6};
constexpr std::uint32_t kWall{7};
constexpr std::uint32_t kAm{8};

std::expected<ww::WorldModel, std::string> make_world() {
  auto res_map{ww::synthetic_map_from_yaml(kRules, kMap)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  return ww::world_create(*res_map, 0);
}

wig::KeyInput ki(wig::Keycode key, wig::ModifierKeys mods,
                 std::int32_t int4_tap_count = 1,
                 wig::KeyInputEvent event = wig::KeyInputEvent::kDown) {
  return wig::KeyInput{.event = event, .key = key, .mods = mods, .int4_tap_count = int4_tap_count};
}

// —— 用例 10：控制组五族走注册表活查询 ——
void test_control_groups_via_registry(const wig::HotkeyRegistryModel& reg) {
  auto res_world{make_world()};
  check(res_world.has_value(), "groups: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};

  wig::SelectionModel sel;
  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, false, false);

  // Ctrl+1 = ControlGroupCreate01（NUMBER_1 Ctrl）
  auto res_create{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kCtrl))};
  check(res_create.b_handled && sel.arr_groups[0] == std::vector<std::uint32_t>{kTankA, kTankB},
        "groups: Ctrl+NUMBER_1 creates group 1 from selection");

  // Ctrl,Shift+1 = AddTo01：先清组内已有再加
  wig::selection_combine(sel, std::vector<std::uint32_t>{kAm}, false, false);
  auto res_add{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kCtrl | wig::ModifierKeys::kShift))};
  check(res_add.b_handled &&
            sel.arr_groups[0] == std::vector<std::uint32_t>{kTankA, kTankB, kAm},
        "groups: Ctrl+Shift+NUMBER_1 adds selection into group");

  // Shift+1 = CombineWith01：组并入当前选区
  wig::selection_combine(sel, std::vector<std::uint32_t>{kWall}, false, false);
  auto res_combine{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kShift))};
  check(res_combine.b_handled &&
            sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kWall, kAm}),
        "groups: Shift+NUMBER_1 combines group into selection");

  // 裸 1 = Select01：替换选区为组（tap=1 不跳转）
  wig::selection_clear(sel);
  auto res_select{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kNone))};
  check(res_select.b_handled && !res_select.opt_center_group.has_value() &&
            sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kAm}),
        "groups: NUMBER_1 selects group, no viewport hook on single tap");

  // 双击裸 1（tap=2）→ 跳转 hook 记录
  auto res_double{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kNone, 2))};
  check(res_double.b_handled && res_double.opt_center_group.has_value() &&
            *res_double.opt_center_group == 0,
        "groups: double-tap NUMBER_1 records viewport center hook");

  // Alt+1 = JumpTo01：只跳转不改选区
  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA}, false, false);
  auto res_jump{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber1, wig::ModifierKeys::kAlt))};
  check(res_jump.b_handled && res_jump.opt_center_group.has_value() &&
            sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA},
        "groups: Alt+NUMBER_1 jumps without changing selection");

  // Ctrl+2 空选区建组 = no-op（不清组）
  wig::selection_clear(sel);
  auto res_empty{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer, ki(
      wig::Keycode::kNumber2, wig::ModifierKeys::kCtrl))};
  check(res_empty.b_handled && sel.arr_groups[1].empty(),
        "groups: Ctrl+NUMBER_2 with empty selection is a no-op");

  // RemoveFromControlGroup 默认未绑 → 不命中
  check(!wig::hotkey_action_remove_from_control_group(reg, sel, ki(wig::Keycode::kDelete,
                                                                   wig::ModifierKeys::kNone)),
        "groups: RemoveFromControlGroup default unbound, key input unmatched");
  // 改键后走同一函数命中
  wig::HotkeyRegistryModel reg2{reg};
  check(wig::hotkey_set(reg2, "RemoveFromControlGroup",
                        wig::HotkeyModel{wig::Keycode::kBackspace, wig::ModifierKeys::kNone}),
        "groups: rebind RemoveFromControlGroup to BACKSPACE");
  wig::selection_combine(sel, std::vector<std::uint32_t>{kWall}, false, false);
  sel.arr_groups[0].push_back(kWall);
  check(wig::hotkey_action_remove_from_control_group(reg2, sel, ki(wig::Keycode::kBackspace,
                                                                   wig::ModifierKeys::kNone)) &&
            std::find(sel.arr_groups[0].begin(), sel.arr_groups[0].end(), kWall) ==
                sel.arr_groups[0].end(),
        "groups: BACKSPACE removes selected actor from its group");

  // 全匹配：NUMBER_1 Ctrl 不激活 Select01（NUMBER_1 裸键）
  wig::SelectionModel sel_probe;
  sel_probe.vec_actor_ids = {kTankA};
  const auto res_mismatch{wig::control_groups_handle_key(
      reg, sel_probe, w, kViewerPlayer,
      ki(wig::Keycode::kNumber1, wig::ModifierKeys::kCtrl | wig::ModifierKeys::kShift))};
  check(res_mismatch.b_handled && sel_probe.arr_groups[0] == std::vector<std::uint32_t>{kTankA},
        "groups: Ctrl+Shift+NUMBER_1 hits AddTo01, not Select01/Create01 (full-match dispatch)");
}

// —— 用例 11：注册表激活矩阵（全匹配语义全员钉死） ——
void test_activation_matrix(const wig::HotkeyRegistryModel& reg) {
  std::size_t sz_valid_defs{0};
  for (std::size_t i{0}; i < reg.vec_definitions.size(); ++i) {
    const wig::HotkeyModel hk{reg.vec_keys[i]};
    if (!wig::hotkey_is_valid(hk)) continue;
    ++sz_valid_defs;
  }
  check(sz_valid_defs == 164, "matrix: 164 of 174 definitions have default bindings");

  // 激活集合 = 恰为 (key,mods) 双双相等的定义集合（全匹配：含修饰的输入不激活
  // 无修饰绑定，反之亦然；同键同修饰多绑互相命中，胜者归分发层）。
  for (std::size_t i{0}; i < reg.vec_definitions.size(); ++i) {
    const wig::HotkeyModel hk{reg.vec_keys[i]};
    if (!wig::hotkey_is_valid(hk)) continue;
    wig::KeyInput ki_probe{.event = wig::KeyInputEvent::kDown, .key = hk.key, .mods = hk.mods};
    for (std::size_t j{0}; j < reg.vec_definitions.size(); ++j) {
      const bool b_same{reg.vec_keys[j].key == hk.key && reg.vec_keys[j].mods == hk.mods};
      const bool b_got{wig::hotkey_is_activated_by(reg.vec_keys[j], ki_probe)};
      if (b_got != b_same)
        check(false,
              std::format("matrix: input of {} vs {} got {} want {}",
                          reg.vec_definitions[i].str_name, reg.vec_definitions[j].str_name,
                          b_got, b_same));
    }
  }

  // 具名反面例：NUMBER_1 Ctrl 不激活 ControlGroupSelect01（NUMBER_1），激活的是
  // ControlGroupCreate01。
  wig::KeyInput ki_n1c{.event = wig::KeyInputEvent::kDown,
                       .key = wig::Keycode::kNumber1,
                       .mods = wig::ModifierKeys::kCtrl};
  check(!wig::hotkey_is_activated_by(
              wig::hotkey_resolve(reg, "ControlGroupSelect01"), ki_n1c),
        "matrix: NUMBER_1 Ctrl does not activate plain NUMBER_1 binding");
  check(wig::hotkey_is_activated_by(wig::hotkey_resolve(reg, "ControlGroupCreate01"), ki_n1c),
        "matrix: NUMBER_1 Ctrl activates ControlGroupCreate01");

  // 字面 fallback：名字不是定义 → TryParse；再失败 → Invalid。
  check(wig::hotkey_resolve(reg, "escape").key == wig::Keycode::kEscape &&
            wig::hotkey_resolve(reg, "escape").mods == wig::ModifierKeys::kNone,
        "resolve: literal `escape' parses via fallback (not a definition name)");
  check(!wig::hotkey_is_valid(wig::hotkey_resolve(reg, "not_a_hotkey_name")),
        "resolve: unknown name falls back to Invalid");
}

// —— 用例 12：改键/冲突/Set 写回 ——
void test_rebind_and_conflicts(const wig::HotkeyRegistryModel& reg) {
  wig::HotkeyRegistryModel reg2{reg};

  const wig::HotkeyModel hk_q_ctrl{wig::Keycode::kQ, wig::ModifierKeys::kCtrl};
  wig::KeyInput ki_q{.event = wig::KeyInputEvent::kDown,
                     .key = wig::Keycode::kQ,
                     .mods = wig::ModifierKeys::kCtrl};

  check(wig::hotkey_set(reg2, "Sell", hk_q_ctrl), "rebind: Set Sell=Q Ctrl succeeds");
  check(wig::hotkey_resolve(reg2, "Sell").key == wig::Keycode::kQ &&
            wig::hotkey_resolve(reg2, "Sell").mods == wig::ModifierKeys::kCtrl,
        "rebind: resolve(Sell) now live-returns Q Ctrl");
  check(wig::hotkey_is_activated_by(wig::hotkey_resolve(reg2, "Sell"), ki_q),
        "rebind: Q Ctrl now activates Sell");

  // Sell(Q Ctrl) 与 MapBookmarkSave01（默认 Q Ctrl，player,spectator）语境相交 → 双双冲突；
  // SelectAllUnits（Q，无修饰）不受牵连。
  check(reg2.vec_definitions[wig::hotkey_registry_find(reg2, "Sell").value()].b_has_duplicates &&
            reg2.vec_definitions[wig::hotkey_registry_find(reg2, "MapBookmarkSave01").value()]
                .b_has_duplicates,
        "rebind: Sell and MapBookmarkSave01 flagged duplicates (Q Ctrl, contexts overlap)");
  check(!reg2.vec_definitions[wig::hotkey_registry_find(reg2, "SelectAllUnits").value()]
              .b_has_duplicates,
        "rebind: SelectAllUnits (plain Q) not flagged");

  check(wig::hotkey_set(reg2, "SelectAllUnits", hk_q_ctrl),
        "rebind: Set SelectAllUnits=Q Ctrl succeeds");
  check(reg2.vec_definitions[wig::hotkey_registry_find(reg2, "SelectAllUnits").value()]
              .b_has_duplicates,
        "rebind: SelectAllUnits now flagged too (third Q Ctrl binding)");

  // 冲突不阻止运行期绑定生效：三个都激活，胜者由分发序决定。
  check(wig::hotkey_is_activated_by(wig::hotkey_resolve(reg2, "Sell"), ki_q) &&
            wig::hotkey_is_activated_by(wig::hotkey_resolve(reg2, "SelectAllUnits"), ki_q) &&
            wig::hotkey_is_activated_by(wig::hotkey_resolve(reg2, "MapBookmarkSave01"), ki_q),
        "rebind: conflicting bindings all activate (duplicates do not block)");

  // 改回默认：冲突清零、settings 节点移除。
  check(wig::hotkey_set(reg2, "Sell", wig::HotkeyModel{wig::Keycode::kZ, wig::ModifierKeys::kNone}),
        "rebind: Set Sell back to default Z");
  check(!reg2.vec_definitions[wig::hotkey_registry_find(reg2, "Sell").value()].b_has_duplicates,
        "rebind: Sell duplicate flag cleared after restore");
  check(reg2.vec_definitions[wig::hotkey_registry_find(reg2, "MapBookmarkSave01").value()]
              .b_has_duplicates,
        "rebind: MapBookmarkSave01 still flagged (SelectAllUnits still Q Ctrl)");

  // 语境不相交不算冲突：F1（player 的 Production01）改绑为 spectator 语境键。
  wig::HotkeyRegistryModel reg3{reg};
  check(wig::hotkey_set(reg3, "Production01",
                        wig::HotkeyModel{wig::Keycode::kMinus, wig::ModifierKeys::kNone}),
        "rebind: Production01=MINUS");
  check(!reg3.vec_definitions[wig::hotkey_registry_find(reg3, "Production01").value()]
              .b_has_duplicates,
        "rebind: player-only context does not collide with spectator-only MINUS binding");

  // Readonly：Autocomplete（Tab）Set 无效。
  check(!wig::hotkey_set(reg3, "Autocomplete",
                         wig::HotkeyModel{wig::Keycode::kF2, wig::ModifierKeys::kNone}),
        "rebind: Set on readonly definition rejected");
  check(wig::hotkey_resolve(reg3, "Autocomplete").key == wig::Keycode::kTab,
        "rebind: readonly binding unchanged");

  // 未知名 Set 无效。
  check(!wig::hotkey_set(reg3, "NoSuchHotkey", wig::HotkeyModel{wig::Keycode::kQ,
                                                                wig::ModifierKeys::kNone}),
        "rebind: Set on unknown name rejected");

  // 写回 settings 段：非默认 → 节点；回默认 → 节点清除；Invalid quirk 照抄。
  wig::HotkeyRegistryModel reg4{reg};
  check(wig::hotkey_set(reg4, "Sell", wig::HotkeyModel{wig::Keycode::kQ, wig::ModifierKeys::kNone}),
        "settings: Set Sell=Q succeeds");
  check(reg4.vec_settings.size() == 1 && reg4.vec_settings[0].first == "Sell" &&
            reg4.vec_settings[0].second == "Q None",
        "settings: non-default Set appends serialized `Q None' node");
  check(wig::hotkey_set(reg4, "Sell", wig::HotkeyModel{wig::Keycode::kW, wig::ModifierKeys::kNone}),
        "settings: re-Set Sell=W succeeds");
  check(reg4.vec_settings.size() == 1 && reg4.vec_settings[0].second == "W None",
        "settings: re-Set replaces node (RemoveAll+append), never duplicates");
  check(wig::hotkey_set(reg4, "Sell", wig::HotkeyModel{wig::Keycode::kZ, wig::ModifierKeys::kNone}),
        "settings: Set Sell=Z again succeeds");
  check(reg4.vec_settings.empty(),
        "settings: Set back to default clears the node (Sell default is Z)");

  // 上游 quirk：Set(name, Invalid) 对 Default=Invalid 的定义恒"不等" → 写 UNKNOWN None。
  check(wig::hotkey_set(reg4, "TogglePlayerStanceColor", wig::kHotkeyInvalid),
        "settings: Set(Invalid) on unbound definition accepted");
  check(reg4.vec_settings.size() == 1 && reg4.vec_settings[0].second == "UNKNOWN None",
        "settings: Set(Invalid) writes UNKNOWN None node (upstream quirk preserved)");
  wig::HotkeyRegistryModel reg5{reg};
  std::vector<ora::game::MiniYamlNode> vec_nodes;
  ora::game::MiniYamlNode node_quirk;
  node_quirk.str_key = "TogglePlayerStanceColor";
  node_quirk.str_value = "UNKNOWN None";
  vec_nodes.push_back(node_quirk);
  wig::hotkey_registry_apply_settings(reg5, vec_nodes);
  check(!wig::hotkey_is_valid(wig::hotkey_resolve(reg5, "TogglePlayerStanceColor")),
        "settings: UNKNOWN None override parses back to Invalid (round-trip)");
}

}

int main() {
  test_hotkey_parse_and_equal();
  test_golden_bindings();

  const std::string str_files{read_text_file(std::format("{}/files.txt", ORA_HOTKEYS_GOLDEN_DIR))};
  std::vector<std::string> vec_file_keys;
  for (const std::string& str_line : split_lines(str_files))
    if (!str_line.empty()) vec_file_keys.push_back(str_line);
  auto res_reg{load_registry_from_golden(vec_file_keys)};
  if (res_reg.has_value()) {
    test_control_groups_via_registry(*res_reg);
    test_activation_matrix(*res_reg);
    test_rebind_and_conflicts(*res_reg);
  }

  return ora::test::test_exit_code("hotkeys");
}
