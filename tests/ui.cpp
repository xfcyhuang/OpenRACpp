import std;

import ora.game.input;
import ora.game.miniyaml;
import ora.game.render;
import ora.game.ui;
import ora.test.util;
using ora::test::check;

#ifndef ORA_UI_GOLDEN_DIR
#define ORA_UI_GOLDEN_DIR "tests/golden/chrome"
#endif

#ifndef ORA_FONTS_GOLDEN_DIR
#define ORA_FONTS_GOLDEN_DIR "tests/golden/fonts"
#endif

namespace {

namespace ui = ora::game::ui;

using SymbolMap = std::map<std::string, int>;

int eval_or_die(std::string_view str_expr, const SymbolMap& map_syms) {
  const auto res{ui::widget_int_eval(str_expr, map_syms)};
  if (!res) {
    check(false, std::format("eval '{}' failed: {}", str_expr, res.error()));
    return 0;
  }
  return *res;
}

ui::WidgetModel make_widget(std::string_view str_type, std::string_view str_id) {
  ui::WidgetModel w{};
  w.str_type = std::string{str_type};
  w.str_id = std::string{str_id};
  if (str_type == "Container") {
    w.b_click_through = true;
    w.b_ignore_mouse_over = true;
  } else if (str_type == "Image") {
    w.b_click_through = true;
  }
  return w;
}

ora::game::input::MouseInput make_mouse(
    ora::game::input::MouseInputEvent ev, ora::game::input::MouseButton btn, int x, int y) {
  ora::game::input::MouseInput mi{};
  mi.event = ev;
  mi.button = btn;
  mi.int4_x = x;
  mi.int4_y = y;
  return mi;
}

namespace mouse = ora::game::input;
constexpr mouse::MouseButton kLeft{mouse::MouseButton::kLeft};
constexpr mouse::MouseInputEvent kDown{mouse::MouseInputEvent::kDown};
constexpr mouse::MouseInputEvent kUp{mouse::MouseInputEvent::kUp};
constexpr mouse::MouseInputEvent kMove{mouse::MouseInputEvent::kMove};

ui::WidgetModel& add_child(ui::WidgetModel& w, ui::WidgetModel c) {
  w.vec_children.push_back(std::make_unique<ui::WidgetModel>(std::move(c)));
  ui::WidgetModel& ref{*w.vec_children.back()};
  ref.ptr_parent = &w;
  return ref;
}

std::vector<std::string> split_lines(const std::string& str_text) {
  std::vector<std::string> vec_out;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t sz_nl{str_text.find('\n', sz_pos)};
    std::string str_line{str_text.substr(
        sz_pos, sz_nl == std::string::npos ? std::string::npos : sz_nl - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    if (!str_line.empty() && str_line.front() != '#') vec_out.push_back(std::move(str_line));
    if (sz_nl == std::string::npos) break;
    sz_pos = sz_nl + 1;
  }
  return vec_out;
}

std::optional<std::vector<std::string>> load_golden_lines(std::string_view str_rel) {
  const auto opt_text{ora::test::read_text_file(std::string{ORA_UI_GOLDEN_DIR} + "/" + std::string{str_rel})};
  if (!opt_text.has_value()) return std::nullopt;
  return split_lines(*opt_text);
}

void check_golden_lines(std::string_view str_what, const std::vector<std::string>& vec_produced,
                        std::string_view str_rel) {
  const auto opt_golden{load_golden_lines(str_rel)};
  if (!opt_golden.has_value()) {
    check(false, std::format("{}: golden {} 缺失", str_what, str_rel));
    return;
  }
  const std::size_t sz_n{std::min(vec_produced.size(), opt_golden->size())};
  bool b_same{vec_produced.size() == opt_golden->size()};
  std::size_t sz_first_diff{vec_produced.size()};
  for (std::size_t sz_i{0}; sz_i < sz_n; ++sz_i)
    if (vec_produced[sz_i] != (*opt_golden)[sz_i]) {
      b_same = false;
      sz_first_diff = sz_i;
      break;
    }
  if (b_same) {
    check(true, std::format("{}: golden {} 全等（{} 行）", str_what, str_rel, vec_produced.size()));
    return;
  }
  check(false, std::format("{}: golden {} 不一致（produced {} 行 / golden {} 行）", str_what, str_rel,
                           vec_produced.size(), opt_golden->size()));
  if (sz_first_diff < sz_n)
    std::println("  first diff at line {}: produced [{}] golden [{}]", sz_first_diff + 1,
                 vec_produced[sz_first_diff], (*opt_golden)[sz_first_diff]);
}

std::string dump_chrome_collections(const std::map<std::string, ui::ChromeCollection>& map_cols) {
  std::string str_out;
  for (const auto& [str_name, col] : map_cols) {
    str_out += "collection\t";
    str_out += str_name;
    str_out += '\n';
    str_out += "Image\t" + col.str_image + '\n';
    str_out += "Image2x\t" + col.str_image2x + '\n';
    str_out += "Image3x\t" + col.str_image3x + '\n';
    str_out += "PanelRegion\t";
    for (std::size_t sz_i{0}; sz_i < col.vec_panel_region.size(); ++sz_i) {
      if (sz_i != 0) str_out += ',';
      str_out += std::to_string(col.vec_panel_region[sz_i]);
    }
    str_out += '\n';
    str_out += "PanelSides\t" + std::to_string(col.u4_panel_sides) + '\n';
    for (const auto& [str_region, rect] : col.map_regions) {
      str_out += "Region\t" + str_region + '\t' + std::to_string(rect.int4_x) + ','
          + std::to_string(rect.int4_y) + ',' + std::to_string(rect.int4_width) + ','
          + std::to_string(rect.int4_height) + '\n';
    }
  }
  return str_out;
}

void dump_widget_tree_node(std::string& str_out, const ora::game::MiniYamlNode& node,
                           const ui::WidgetModel* ptr_widget, int int4_depth) {
  const std::string str_indent(static_cast<std::size_t>(int4_depth), '\t');
  const std::string_view str_type{ui::widget_key_type_part(node.str_key)};
  const std::string_view str_id{ui::widget_key_id_part(node.str_key)};
  str_out += str_indent;
  str_out.append(str_type);
  str_out += '@';
  str_out.append(str_id);
  if (ptr_widget != nullptr)
    str_out += '\t' + std::to_string(ptr_widget->bounds.int4_x) + ','
        + std::to_string(ptr_widget->bounds.int4_y) + ',' + std::to_string(ptr_widget->bounds.int4_width)
        + ',' + std::to_string(ptr_widget->bounds.int4_height);
  else
    str_out += "\t?";
  str_out += '\n';
  for (const ora::game::MiniYamlNode& child : node.vec_children) {
    if (child.str_key == "Children") continue;
    str_out += str_indent + "\tfield\t" + child.str_key + '\t' + child.str_value + '\n';
  }
  for (const ora::game::MiniYamlNode& child : node.vec_children) {
    if (child.str_key != "Children") continue;
    for (std::size_t sz_i{0}; sz_i < child.vec_children.size(); ++sz_i) {
      const ui::WidgetModel* ptr_child{nullptr};
      if (ptr_widget != nullptr && sz_i < ptr_widget->vec_children.size())
        ptr_child = ptr_widget->vec_children[sz_i].get();
      dump_widget_tree_node(str_out, child.vec_children[sz_i], ptr_child, int4_depth + 1);
    }
  }
}

}  // namespace

int main() {
  const SymbolMap kEmpty{};

  check(eval_or_die("10", kEmpty) == 10, "literal");
  check(eval_or_die("0", kEmpty) == 0, "literal zero");
  check(eval_or_die("-2", kEmpty) == -2, "negative literal");
  check(eval_or_die("2 + 3 * 4", kEmpty) == 14, "precedence mult");
  check(eval_or_die("2 * 3 + 4", kEmpty) == 10, "precedence mult left");
  check(eval_or_die("(2 + 3) * 4", kEmpty) == 20, "parens");
  check(eval_or_die("10 - 3 - 2", kEmpty) == 5, "left assoc sub");
  check(eval_or_die("100 / 7", kEmpty) == 14, "int div");
  check(eval_or_die("7 % 3", kEmpty) == 1, "modulo");
  check(eval_or_die("5 / 0", kEmpty) == 0, "div zero yields zero");
  check(eval_or_die("5 % 0", kEmpty) == 0, "mod zero yields zero");

  check(eval_or_die("WINDOW_WIDTH - WIDTH - 5",
                     SymbolMap{{"WINDOW_WIDTH", 1024}, {"WIDTH", 44}}) == 975,
        "variable substitution");
  check(eval_or_die("PARENT_WIDTH - 2 * X", SymbolMap{{"PARENT_WIDTH", 238}, {"X", 10}}) == 218,
        "parent width expr");
  check(eval_or_die("UNKNOWN_VAR", kEmpty) == 0, "unknown variable yields zero");
  check(eval_or_die("0 - 2", kEmpty) == -2, "explicit sub");

  check(eval_or_die("1 < 2", kEmpty) == 1, "bool true converts to 1");
  check(eval_or_die("2 < 1", kEmpty) == 0, "bool false converts to 0");
  check(eval_or_die("1 && 0", kEmpty) == 0, "and");
  check(eval_or_die("1 && 2", kEmpty) == 1, "and nonzero");
  check(eval_or_die("0 || 1", kEmpty) == 1, "or");
  check(eval_or_die("!0", kEmpty) == 1, "not");
  check(eval_or_die("1 == 1", kEmpty) == 1, "equals");
  check(eval_or_die("1 != 1", kEmpty) == 0, "not equals");
  check(eval_or_die("2 >= 2", kEmpty) == 1, "gte");
  check(eval_or_die("-3 < -2", kEmpty) == 1, "negative compare");

  check(eval_or_die("~0", kEmpty) == -1, "ones complement");
  check(eval_or_die("-WIDTH", SymbolMap{{"WIDTH", 7}}) == -7, "unary minus variable");

  const auto res_bad{ui::widget_int_eval("1 +", kEmpty)};
  check(!res_bad.has_value(), "trailing operator is error");
  const auto res_bad2{ui::widget_int_eval("(1", kEmpty)};
  check(!res_bad2.has_value(), "unclosed paren is error");
  const auto res_bad3{ui::widget_int_eval("", kEmpty)};
  check(!res_bad3.has_value(), "empty expression is error");
  const auto res_bad4{ui::widget_int_eval("1 2", kEmpty)};
  check(!res_bad4.has_value(), "missing binary op is error");

  const ui::WidgetInitCtx kCtx{1024, 768, nullptr, "default"};

  {
    ui::WidgetModel w{make_widget("Container", "ROOT")};
    w.str_width = "434";
    w.str_height = "44";
    w.str_x = "5";
    w.str_y = "WINDOW_HEIGHT - HEIGHT - 5";
    ui::widget_initialize(w, kCtx);
    check(w.bounds.int4_x == 5 && w.bounds.int4_y == 719 && w.bounds.int4_width == 434
              && w.bounds.int4_height == 44,
          "command bar bounds");
  }

  {
    ui::WidgetModel w{make_widget("Container", "ROOT")};
    w.str_width = "PARENT_WIDTH - 10";
    w.str_height = "10";
    w.str_x = "WIDTH - WIDTH";
    w.str_y = "0";
    ui::widget_initialize(w, kCtx);
    check(w.bounds.int4_width == 1014, "parent width defaults to window at root");
    check(w.bounds.int4_x == 0, "self width reference");
  }

  {
    ui::WidgetModel parent{make_widget("Container", "P")};
    parent.str_width = "238";
    parent.str_height = "262";
    parent.str_x = "10";
    parent.str_y = "10";
    ui::widget_initialize(parent, kCtx);
    ui::WidgetModel& child{add_child(parent, make_widget("Image", "C"))};
    child.str_x = "0 - 2";
    child.str_y = "PARENT_HEIGHT - 8";
    child.str_width = "62";
    child.str_height = "46";
    ui::widget_initialize(child, kCtx);
    check(child.bounds.int4_x == -2 && child.bounds.int4_y == 254, "child parent-relative expr");
    const auto [int4_ox, int4_oy]{ui::widget_render_origin(child)};
    check(int4_ox == 8 && int4_oy == 264, "render origin chains parent");
    const auto [int4_ex, int4_ey, int4_ew, int4_eh]{ui::widget_event_bounds(child)};
    check(int4_ex == 8 && int4_ey == 264 && int4_ew == 62 && int4_eh == 46, "event bounds absolute");
  }

  {
    ui::WidgetModel w{make_widget("Container", "N")};
    ui::widget_initialize(w, kCtx);
    check(w.bounds.int4_x == 0 && w.bounds.int4_y == 0 && w.bounds.int4_width == 0
              && w.bounds.int4_height == 0,
          "empty expressions default zero");
  }

  ui::WidgetInitCtx ctx_cursor{1024, 768, nullptr, "default"};

  {
    ui::WidgetModel root{make_widget("Container", "ROOT")};
    ui::WidgetModel& p{add_child(root, make_widget("Container", "P"))};
    p.b_click_through = false;
    p.str_x = "0";
    p.str_y = "0";
    p.str_width = "200";
    p.str_height = "200";
    ui::WidgetModel& b1{add_child(p, make_widget("Button", "B1"))};
    b1.str_x = "10";
    b1.str_y = "10";
    b1.str_width = "30";
    b1.str_height = "20";
    ui::WidgetModel& b2{add_child(p, make_widget("Button", "B2"))};
    b2.str_x = "50";
    b2.str_y = "10";
    b2.str_width = "30";
    b2.str_height = "20";
    ui::widget_initialize(root, ctx_cursor);
    ui::widget_initialize(p, ctx_cursor);
    ui::widget_initialize(b1, ctx_cursor);
    ui::widget_initialize(b2, ctx_cursor);

    ui::UiState st{};
    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 60, 15)), "button down handled");
    check(st.ptr_mouse_focus == &b2, "down takes focus on later sibling");
    check(b2.b_depressed, "down depresses button");
    check(!b1.b_depressed, "other button not depressed");

    check(ui::ui_handle_input(root, st, make_mouse(kUp, kLeft, 60, 15)), "button up handled");
    check(st.ptr_mouse_focus == nullptr, "up yields focus");
    check(!b2.b_depressed, "up releases button");

    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 150, 150)),
          "non click-through container consumes inside bounds");
    check(st.ptr_mouse_focus == nullptr, "container down does not take focus");

    check(!ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 900, 900)),
          "outside root is unhandled");

    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 60, 15)), "second down handled");
    check(!ui::ui_handle_input(root, st, make_mouse(kMove, mouse::MouseButton::kNone, 999, 999)),
          "focused move away unhandled");
    check(st.ptr_mouse_focus == &b2, "move keeps mouse focus");
    check(st.ptr_mouse_over == &b2, "focused move sets over without position check");
    check(st.vec_entered.size() == 1 && st.vec_entered[0] == &b2, "entered recorded for focused move");
    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 999, 999)),
          "focused widget consumes down outside bounds");
    check(st.ptr_mouse_focus == &b2, "focused down keeps focus");
    check(ui::ui_handle_input(root, st, make_mouse(kUp, kLeft, 999, 999)),
          "focused widget consumes up outside bounds");
    check(st.ptr_mouse_focus == nullptr, "up outside yields focus");

    check(ui::ui_handle_input(root, st, make_mouse(kMove, mouse::MouseButton::kNone, 60, 15)),
          "move over button is consumed by container ancestor");
    check(st.ptr_mouse_over == &b2, "move sets mouse over");
    check(st.vec_entered.size() == 1, "no duplicate entered when over unchanged");

    check(ui::ui_handle_input(root, st, make_mouse(kMove, mouse::MouseButton::kNone, 15, 15)),
          "move to other button");
    check(st.ptr_mouse_over == &b1, "over switches to first button");
    check(st.vec_exited.size() == 1 && st.vec_exited[0] == &b2, "exited recorded for old over");
    check(st.vec_entered.size() == 2 && st.vec_entered[1] == &b1, "entered recorded for new over");

    check(!ui::ui_handle_input(root, st, make_mouse(kMove, mouse::MouseButton::kNone, 700, 700)),
          "move to empty space");
    check(st.ptr_mouse_over == nullptr, "over cleared on empty space");
    check(st.vec_exited.size() == 2 && st.vec_exited[1] == &b1, "exited recorded for b1");
  }

  {
    ui::WidgetModel root{make_widget("Container", "ROOT")};
    ui::WidgetModel& p{add_child(root, make_widget("Container", "P"))};
    p.b_ignore_child_mouse_over = true;
    p.b_click_through = false;
    p.str_width = "100";
    p.str_height = "100";
    ui::WidgetModel& b{add_child(p, make_widget("Button", "B"))};
    b.str_width = "10";
    b.str_height = "10";
    ui::widget_initialize(root, ctx_cursor);
    ui::widget_initialize(p, ctx_cursor);
    ui::widget_initialize(b, ctx_cursor);

    ui::UiState st{};
    static_cast<void>(ui::ui_handle_input(root, st, make_mouse(kMove, mouse::MouseButton::kNone, 5, 5)));
    check(st.ptr_mouse_over == nullptr, "ignore child mouse over restores previous over");
  }

  {
    ui::WidgetModel root{make_widget("Container", "ROOT")};
    ui::WidgetModel& p{add_child(root, make_widget("Container", "P"))};
    p.b_click_through = false;
    p.str_width = "100";
    p.str_height = "100";
    ui::WidgetModel& hidden{add_child(p, make_widget("Button", "HIDDEN"))};
    hidden.str_width = "50";
    hidden.str_height = "50";
    hidden.b_visible = false;
    ui::WidgetModel& shown{add_child(p, make_widget("Button", "SHOWN"))};
    shown.str_x = "60";
    shown.str_width = "30";
    shown.str_height = "30";
    ui::widget_initialize(root, ctx_cursor);
    ui::widget_initialize(p, ctx_cursor);
    ui::widget_initialize(hidden, ctx_cursor);
    ui::widget_initialize(shown, ctx_cursor);

    ui::UiState st{};
    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 10, 10)),
          "invisible button skipped, container consumes");
    check(st.ptr_mouse_focus == nullptr && !hidden.b_depressed,
          "invisible button does not take focus or depress");
    check(ui::ui_handle_input(root, st, make_mouse(kDown, kLeft, 70, 10)),
          "visible sibling still receives");
    check(st.ptr_mouse_focus == &shown && shown.b_depressed, "visible button focused");
  }

  {
    ui::WidgetModel root{make_widget("Container", "ROOT")};
    ui::WidgetModel& p{add_child(root, make_widget("Container", "P"))};
    p.b_click_through = false;
    p.str_width = "100";
    p.str_height = "100";
    ui::WidgetModel& img1{add_child(p, make_widget("Image", "I1"))};
    img1.str_x = "10";
    img1.str_y = "10";
    img1.str_width = "20";
    img1.str_height = "20";
    ui::WidgetModel& img2{add_child(p, make_widget("Image", "I2"))};
    img2.str_x = "50";
    img2.str_y = "50";
    img2.str_width = "20";
    img2.str_height = "20";
    ui::widget_initialize(root, ctx_cursor);
    ui::widget_initialize(p, ctx_cursor);
    ui::widget_initialize(img1, ctx_cursor);
    ui::widget_initialize(img2, ctx_cursor);

    const std::string* cursor{ui::widget_hit_test(root, 60, 60)};
    check(cursor != nullptr && *cursor == "default", "hit test returns inner cursor");
    cursor = ui::widget_hit_test(root, 15, 15);
    check(cursor != nullptr && *cursor == "default", "hit test first image");
    cursor = ui::widget_hit_test(root, 95, 95);
    check(cursor == nullptr, "container cursor is null even inside bounds");
    cursor = ui::widget_hit_test(root, 500, 500);
    check(cursor == nullptr, "hit test outside is null");
    cursor = ui::widget_hit_test(img2, 55, 55);
    check(cursor != nullptr, "single widget hit inside");

    img2.b_visible = false;
    cursor = ui::widget_hit_test(root, 60, 60);
    check(cursor == nullptr, "invisible child contributes no cursor");
    img2.b_visible = true;

    ui::WidgetModel orphan{make_widget("Image", "ORPHAN")};
    orphan.str_x = "10";
    orphan.str_y = "10";
    orphan.str_width = "10";
    orphan.str_height = "10";
    ui::widget_initialize(orphan, ctx_cursor);
    cursor = ui::widget_hit_test(orphan, 15, 15);
    check(cursor != nullptr, "parentless widget hit test works");
  }

  {
    ui::WidgetModel root{make_widget("Container", "ROOT")};
    ui::WidgetModel& p{add_child(root, make_widget("Container", "P"))};
    p.str_width = "100";
    p.str_height = "100";
    ui::widget_initialize(root, ctx_cursor);
    ui::widget_initialize(p, ctx_cursor);
    ui::UiState st{};
    mouse::KeyInput ki{};
    ki.key = mouse::Keycode::kEscape;
    check(!ui::ui_handle_key_press(root, st, ki), "no key handler consumes");
    st.ptr_keyboard_focus = &p;
    check(!ui::ui_handle_key_press(root, st, ki), "focus without handler is unhandled");
  }

  {
    constexpr std::string_view kChrome{
        "Container@PARENT:\n"
        "\tX: 10\n"
        "\tY: 20\n"
        "\tWidth: 100\n"
        "\tHeight: 50\n"
        "\tVisible: False\n"
        "\tChildren:\n"
        "\t\tImage@CHILD:\n"
        "\t\t\tX: 0 - 2\n"
        "\t\t\tWidth: PARENT_WIDTH - 10\n"
        "\t\t\tImageCollection: sidebar\n"
        "\t\t\tImageName: background\n"
        "\t\tButton@BTN:\n"
        "\t\t\tClickThrough: false\n"
        "\t\t\tBackground: button\n"
        "\t\t\tDisabled: True\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kChrome, "test-chrome")};
    check(res_nodes.has_value(), "chrome yaml parses");
    if (!res_nodes.has_value()) return ora::test::test_exit_code("ui");

    const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
    check(res_loader.has_value(), "loader build");
    if (!res_loader.has_value()) return ora::test::test_exit_code("ui");

    ui::WidgetModel root{make_widget("Container", "ROOT")};
    const auto res_widget{ui::widget_loader_load_widget(*res_loader, root, ctx_cursor, "PARENT")};
    check(res_widget.has_value(), "load PARENT");
    if (!res_widget.has_value()) {
      std::println("load error: {}", res_widget.error());
      return ora::test::test_exit_code("ui");
    }
    ui::WidgetModel& w{**res_widget};
    check(w.str_type == "Container" && w.str_id == "PARENT", "root widget type and id");
    check(w.b_click_through, "container default click through");
    check(w.b_ignore_mouse_over, "container default ignore mouse over");
    check(!w.b_visible, "visible false loaded");
    check(w.vec_children.size() == 2, "children order preserved");
    check(w.vec_children[0]->str_type == "Image" && w.vec_children[0]->str_id == "CHILD",
          "first child image");
    check(w.vec_children[1]->str_type == "Button" && w.vec_children[1]->str_id == "BTN",
          "second child button");
    check(!w.vec_children[1]->b_click_through, "click through false overrides default");
    check(w.vec_children[1]->b_disabled, "disabled true loaded");
    check(!w.vec_children[1]->b_ignore_mouse_over, "button has no container defaults");
    check(w.vec_children[0]->b_click_through, "image default click through");

    check(w.vec_children[0]->str_x == "0 - 2" && w.vec_children[0]->str_width == "PARENT_WIDTH - 10",
          "expression fields kept verbatim");
    bool b_found_collection{false};
    bool b_found_name{false};
    for (const auto& [str_k, str_v] : w.vec_children[0]->vec_fields) {
      if (str_k == "ImageCollection") {
        check(str_v == "sidebar", "image collection field");
        b_found_collection = true;
      }
      if (str_k == "ImageName") {
        check(str_v == "background", "image name field");
        b_found_name = true;
      }
    }
    check(b_found_collection && b_found_name, "unknown fields kept in field list");

    ui::widget_initialize(w, ctx_cursor);
    check(w.bounds.int4_x == 10 && w.bounds.int4_width == 100, "loaded root bounds");
    ui::widget_initialize(*w.vec_children[0], ctx_cursor);
    check(w.vec_children[0]->bounds.int4_x == -2 && w.vec_children[0]->bounds.int4_width == 90,
          "child uses parent width");

    const auto res_missing{ui::widget_loader_load_widget(*res_loader, root, ctx_cursor, "NOPE")};
    check(!res_missing.has_value(), "unknown widget id errors");
    check(ui::widget_type_spec_find("Container") != nullptr, "container registered");
    check(ui::widget_type_spec_find("NoSuchType") == nullptr, "unknown type unregistered");
  }

  {
    constexpr std::string_view kDup{
        "Container@SAME:\n"
        "Container@SAME:\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kDup, "dup")};
    const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
    check(!res_loader.has_value(), "duplicate key errors");
  }

  {
    constexpr std::string_view kBadType{"FooBar@X:\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kBadType, "badtype")};
    check(res_nodes.has_value(), "bad type yaml parses");
    if (res_nodes.has_value()) {
      const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
      check(res_loader.has_value(), "unknown type key still indexes");
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      if (res_loader.has_value()) {
        const auto res_w{ui::widget_loader_load_widget(*res_loader, root, ctx_cursor, "X")};
        check(!res_w.has_value(), "unknown widget type errors on load");
      }
    }
  }

  {
    constexpr std::string_view kLogic{
        "Container@L:\n"
        "\tLogic: LoadIngameChatLogic, OtherLogic\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kLogic, "logic")};
    check(res_nodes.has_value(), "logic yaml parses");
    if (res_nodes.has_value()) {
      const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
      check(res_loader.has_value(), "logic loader build");
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      if (res_loader.has_value()) {
        const auto res_w{ui::widget_loader_load_widget(*res_loader, root, ctx_cursor, "L")};
        check(res_w.has_value(), "logic widget loads");
        if (res_w.has_value()) {
          check((*res_w)->vec_logic.size() == 2 && (*res_w)->vec_logic[0] == "LoadIngameChatLogic"
                    && (*res_w)->vec_logic[1] == "OtherLogic",
                "logic comma split");
        }
      }
    }
  }

  {
    constexpr std::string_view kMetrics{
        "Metrics:\n"
        "\tButtonDepth: 1\n"
        "\tButtonFont: Regular\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kMetrics, "metrics")};
    check(res_nodes.has_value(), "metrics yaml parses");
    if (res_nodes.has_value()) {
      const auto res_map{ui::chrome_metrics_initialize({*res_nodes})};
      check(res_map.has_value(), "metrics load");
      if (res_map.has_value()) {
        check(res_map->size() == 2, "metrics count");
        check(res_map->contains("ButtonDepth") && res_map->at("ButtonDepth") == "1", "metrics int value");
        check(res_map->contains("ButtonFont") && res_map->at("ButtonFont") == "Regular",
              "metrics string value");
      }
    }
  }

  {
    constexpr std::string_view kChromeYaml{
        "^Sidebar:\n"
        "\tImage: sidebar.png\n"
        "sidebar-allies:\n"
        "\tInherits: ^Sidebar\n"
        "\tRegions:\n"
        "\t\tradar: 290, 67, 222, 222\n"
        "sidebar-button-allies:\n"
        "\tInherits: ^Sidebar\n"
        "\tPanelRegion: 260, 281, 5, 5, 18, 18, 5, 5\n"
        "command-button-allies:\n"
        "\tInherits: ^Sidebar\n"
        "\tPanelRegion: 260, 281, 3, 3, 22, 22, 3, 3\n"
        "\tPanelSides: Center\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kChromeYaml, "chrome")};
    check(res_nodes.has_value(), "chrome collections yaml parses");
    if (!res_nodes.has_value()) return ora::test::test_exit_code("ui");

    const auto res_cols{ui::chrome_provider_initialize({*res_nodes})};
    check(res_cols.has_value(), "chrome provider load");
    if (!res_cols.has_value()) {
      std::println("chrome error: {}", res_cols.error());
      return ora::test::test_exit_code("ui");
    }
    const auto& cols{*res_cols};
    check(cols.size() == 3, "abstract collections skipped");
    check(cols.contains("sidebar-allies") && cols.contains("sidebar-button-allies")
              && cols.contains("command-button-allies"),
          "collection names");
    check(cols.at("sidebar-allies").str_image == "sidebar.png", "inherited image");

    const auto opt_radar{ui::chrome_try_get_image(cols, "sidebar-allies", "radar")};
    check(opt_radar.has_value() && opt_radar->int4_x == 290 && opt_radar->int4_y == 67
              && opt_radar->int4_width == 222 && opt_radar->int4_height == 222,
          "region rect parsed");
    check(!ui::chrome_try_get_image(cols, "sidebar-allies", "nope").has_value(), "missing image empty");
    check(!ui::chrome_try_get_image(cols, "no-collection", "radar").has_value(),
          "missing collection empty");

    const auto vec_panel{ui::chrome_get_panel_images(cols, "sidebar-button-allies")};
    check(vec_panel.size() == 9, "panel has 9 slots");
    check(vec_panel[0].has_value() && vec_panel[0]->int4_x == 260 && vec_panel[0]->int4_y == 281
              && vec_panel[0]->int4_width == 5 && vec_panel[0]->int4_height == 5,
          "panel top-left");
    check(vec_panel[1].has_value() && vec_panel[1]->int4_x == 265 && vec_panel[1]->int4_width == 18,
          "panel top");
    check(vec_panel[4].has_value() && vec_panel[4]->int4_x == 265 && vec_panel[4]->int4_y == 286
              && vec_panel[4]->int4_width == 18 && vec_panel[4]->int4_height == 18,
          "panel center");
    check(vec_panel[8].has_value() && vec_panel[8]->int4_x == 283 && vec_panel[8]->int4_y == 304,
          "panel bottom-right");

    const auto vec_center{ui::chrome_get_panel_images(cols, "command-button-allies")};
    check(vec_center[4].has_value() && vec_center[4]->int4_width == 22, "center side kept");
    for (std::size_t sz_i{0}; sz_i < 9; ++sz_i)
      if (sz_i != 4) check(!vec_center[sz_i].has_value(), "non-center sides nulled");

    const auto [int4_mw, int4_mh]{ui::chrome_get_minimum_panel_size(cols, "sidebar-button-allies")};
    check(int4_mw == 10 && int4_mh == 10, "minimum panel size");
    const auto [int4_mw0, int4_mh0]{ui::chrome_get_minimum_panel_size(cols, "sidebar-allies")};
    check(int4_mw0 == 0 && int4_mh0 == 0, "no panel region size zero");

    const auto vec_manual{ui::chrome_get_panel_images(cols, "sidebar-allies")};
    check(vec_manual.size() == 9, "regions-only collection falls back to manual panel slots");
    for (std::size_t sz_i{0}; sz_i < vec_manual.size(); ++sz_i)
      check(!vec_manual[sz_i].has_value(), "manual slot without region is null");
  }

  {
    namespace fr = ora::game::render;
    const auto opt_ttf{ora::test::read_text_file(
        std::string{ORA_FONTS_GOLDEN_DIR} + "/fixture/FreeSans.ttf")};
    check(opt_ttf.has_value(), "fonts golden fixture present");
    if (!opt_ttf.has_value()) return ora::test::test_exit_code("ui");

    auto res_face{fr::font_face_load({reinterpret_cast<const std::uint8_t*>(opt_ttf->data()),
                                      opt_ttf->size()})};
    check(res_face.has_value(), "label: font face load");
    if (!res_face.has_value()) return ora::test::test_exit_code("ui");

    const std::shared_ptr<const fr::FontFace> ptr_face{
        std::make_shared<const fr::FontFace>(std::move(*res_face))};
    fr::SheetBuilder builder{};
    builder.int4_sheet_size = 512;
    builder.int4_margin = 1;
    auto res_font{fr::spritefont_create(ptr_face, builder, 14, 11, 1.0f)};
    check(res_font.has_value(), "label: spritefont create");
    if (!res_font.has_value()) return ora::test::test_exit_code("ui");

    std::map<std::string, fr::SpriteFont> map_fonts;
    map_fonts.emplace("FreeSans", std::move(*res_font));

    int int4_golden_w{-1};
    int int4_golden_h{-1};
    if (const auto opt_measure{ora::test::read_text_file(
            std::string{ORA_FONTS_GOLDEN_DIR} + "/measure-FreeSans-14.txt")}) {
      int int4_index{-1};
      std::size_t sz_pos{0};
      while (sz_pos <= opt_measure->size()) {
        const std::size_t sz_nl{opt_measure->find('\n', sz_pos)};
        const std::string str_line{opt_measure->substr(
            sz_pos, sz_nl == std::string::npos ? std::string::npos : sz_nl - sz_pos)};
        sz_pos = sz_nl == std::string::npos ? opt_measure->size() + 1 : sz_nl + 1;
        if (str_line.empty() || str_line.front() == '#') continue;
        ++int4_index;
        if (int4_index != 3) continue;
        const std::size_t sz_tab1{str_line.find('\t')};
        const std::size_t sz_tab2{str_line.find('\t', sz_tab1 + 1)};
        const std::string str_w{str_line.substr(sz_tab1 + 1, sz_tab2 - sz_tab1 - 1)};
        const std::string str_h{str_line.substr(sz_tab2 + 1)};
        int int4_w{};
        int int4_h{};
        std::from_chars(str_w.data(), str_w.data() + str_w.size(), int4_w);
        std::from_chars(str_h.data(), str_h.data() + str_h.size(), int4_h);
        int4_golden_w = int4_w;
        int4_golden_h = int4_h;
        break;
      }
    }
    check(int4_golden_w > 0 && int4_golden_h > 0, "label: measure golden row present");
    if (int4_golden_w <= 0) return ora::test::test_exit_code("ui");

    const fr::GlyphInfo& glyph_h{fr::spritefont_glyph(map_fonts.at("FreeSans"), u'H')};
    const fr::GlyphInfo& glyph_a{fr::spritefont_glyph(map_fonts.at("FreeSans"), u'a')};
    const fr::GlyphInfo& glyph_b{fr::spritefont_glyph(map_fonts.at("FreeSans"), u'b')};
    const fr::GlyphInfo& glyph_d{fr::spritefont_glyph(map_fonts.at("FreeSans"), u'd')};

    ui::UiRenderCtx rctx{&map_fonts, "FreeSans", 0xFFFFFFFFu};

    auto make_label{[](int int4_x, int int4_y, int int4_w, int int4_h,
                       std::string_view str_align) {
      ui::WidgetModel w{make_widget("Label", "LBL")};
      w.str_x = std::to_string(int4_x);
      w.str_y = std::to_string(int4_y);
      w.str_width = std::to_string(int4_w);
      w.str_height = std::to_string(int4_h);
      w.vec_fields.emplace_back("Text", "Hello World");
      w.vec_fields.emplace_back("Font", "FreeSans");
      w.vec_fields.emplace_back("Align", std::string{str_align});
      return w;
    }};

    {
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      ui::WidgetModel& label{add_child(root, make_label(5, 5, 100, 20, "Center"))};
      ui::widget_initialize(root, kCtx);
      ui::widget_initialize(label, kCtx);
      const std::vector<ui::UiQuad> vec_quads{ui::ui_draw_tree(root, rctx)};
      check(vec_quads.size() == 10, "label: one quad per visible glyph");
      if (vec_quads.size() == 10) {
        const int int4_expect_x{5 + (100 - int4_golden_w) / 2 + glyph_h.int4_off_x};
        const int int4_expect_y{5 + (20 - int4_golden_h - 3) / 2 + 14 + glyph_h.int4_off_y};
        check(vec_quads[0].int4_x == int4_expect_x, "label: center align x from golden width");
        check(vec_quads[0].int4_y == int4_expect_y, "label: middle valign y");
        bool b_monotonic{true};
        for (std::size_t sz_i{1}; sz_i < vec_quads.size(); ++sz_i)
          if (vec_quads[sz_i].int4_x < vec_quads[sz_i - 1].int4_x) b_monotonic = false;
        check(b_monotonic, "label: glyph quads advance left to right");
        check(vec_quads[0].u4_rgba == 0xFFFFFFFFu, "label: default color white");
        bool b_same_sheet{true};
        for (const ui::UiQuad& q : vec_quads)
          if (q.u4_sheet != vec_quads[0].u4_sheet) b_same_sheet = false;
        check(b_same_sheet, "label: glyphs share one sheet");
        check(vec_quads[9].int4_x + vec_quads[9].int4_width <= 5 + 100,
              "label: quads stay inside centered bounds");
        static_cast<void>(glyph_d);
      }
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      ui::WidgetModel& label{add_child(root, make_label(0, 0, 100, 20, "Right"))};
      ui::widget_initialize(root, kCtx);
      ui::widget_initialize(label, kCtx);
      const std::vector<ui::UiQuad> vec_quads{ui::ui_draw_tree(root, rctx)};
      check(vec_quads.size() == 10, "label: right align quad count");
      if (vec_quads.size() == 10) {
        const int int4_expect_x{100 - int4_golden_w + glyph_h.int4_off_x};
        check(vec_quads[0].int4_x == int4_expect_x, "label: right align x");
        check(vec_quads[0].int4_y == (20 - int4_golden_h - 3) / 2 + 14 + glyph_h.int4_off_y,
              "label: default valign is middle");
      }
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      ui::WidgetModel& label{add_child(root, make_label(0, 0, 100, 20, "Left"))};
      label.vec_fields.clear();
      label.vec_fields.emplace_back("Text", std::string{"a\nb"});
      label.vec_fields.emplace_back("Font", std::string{"FreeSans"});
      label.vec_fields.emplace_back("VAlign", std::string{"Top"});
      ui::widget_initialize(root, kCtx);
      ui::widget_initialize(label, kCtx);
      const std::vector<ui::UiQuad> vec_quads{ui::ui_draw_tree(root, rctx)};
      check(vec_quads.size() == 2, "label: newline splits rows");
      if (vec_quads.size() == 2) {
        check(vec_quads[0].int4_y == -3 + 14 + glyph_a.int4_off_y, "label: top valign y");
        check(vec_quads[1].int4_y - vec_quads[0].int4_y == 14 + glyph_b.int4_off_y - glyph_a.int4_off_y,
              "label: row pitch is font size plus bearing delta");
        check(vec_quads[1].int4_x
                  == static_cast<int>(glyph_a.fp4_advance + 0.5f) + glyph_b.int4_off_x,
              "label: second row restarts at advance of first glyph");
      }
    }

    {
      constexpr std::string_view kChromeLabel{
          "Container@PANEL:\n"
          "\tX: 0\n"
          "\tY: 0\n"
          "\tWidth: 120\n"
          "\tHeight: 40\n"
          "\tChildren:\n"
          "\t\tLabel@TITLE:\n"
          "\t\t\tX: 10\n"
          "\t\t\tY: 0\n"
          "\t\t\tWidth: 100\n"
          "\t\t\tHeight: 20\n"
          "\t\t\tText: Hello World\n"
          "\t\t\tFont: FreeSans\n"
          "\t\t\tAlign: Center\n"};
      const auto res_nodes{ora::game::miniyaml_from_string(kChromeLabel, "label-chrome")};
      check(res_nodes.has_value(), "label chrome yaml parses");
      if (res_nodes.has_value()) {
        const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
        check(res_loader.has_value(), "label chrome loader build");
        ui::WidgetModel root{make_widget("Container", "ROOT")};
        if (res_loader.has_value()) {
          const auto res_widget{ui::widget_loader_load_widget(*res_loader, root, ctx_cursor, "PANEL")};
          check(res_widget.has_value(), "label chrome load widget");
          if (res_widget.has_value()) {
            const std::vector<ui::UiQuad> vec_quads{ui::ui_draw_tree(root, rctx)};
            check(vec_quads.size() == 10, "label: chrome loaded label renders");
            if (vec_quads.size() == 10) {
              const int int4_expect_x{10 + (100 - int4_golden_w) / 2 + glyph_h.int4_off_x};
              check(vec_quads[0].int4_x == int4_expect_x, "label: chrome label centered at origin 10");
            }
            check((*res_widget)->vec_children[0]->str_type == "Label", "label type registered");
          }
        }
      }
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT")};
      ui::WidgetModel& label{add_child(root, make_label(0, 0, 100, 20, "Center"))};
      ui::WidgetModel& hidden{add_child(root, make_widget("Image", "HID"))};
      hidden.b_visible = false;
      label.b_visible = false;
      ui::widget_initialize(root, kCtx);
      ui::widget_initialize(label, kCtx);
      check(ui::ui_draw_tree(root, rctx).empty(), "label: invisible subtree skipped");
    }
  }

  {
    const auto res_nodes{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/ra-chrome.yaml", "ra-chrome.yaml")};
    check(res_nodes.has_value(), "chrome golden: fixture yaml parses");
    if (!res_nodes.has_value()) return ora::test::test_exit_code("ui");
    const auto res_cols{ui::chrome_provider_initialize({*res_nodes})};
    check(res_cols.has_value(), "chrome golden: provider load");
    if (!res_cols.has_value()) {
      std::println("chrome error: {}", res_cols.error());
      return ora::test::test_exit_code("ui");
    }
    const std::string str_dump{dump_chrome_collections(*res_cols)};
    check_golden_lines("chrome collections", split_lines(str_dump), "ra-chrome-collections.txt");

    std::size_t sz_panel_collections{0};
    std::size_t sz_region_collections{0};
    for (const auto& [str_name, col] : *res_cols) {
      const auto vec_panel{ui::chrome_get_panel_images(*res_cols, str_name)};
      if (col.vec_panel_region.size() == 8) {
        ++sz_panel_collections;
        check(vec_panel.size() == 9, std::format("panel slots for {}", str_name));
      } else if (!col.map_regions.empty()) {
        ++sz_region_collections;
        check(vec_panel.size() == 9, std::format("manual panel slots for {}", str_name));
      } else {
        check(vec_panel.empty(), std::format("no panel sprites for {}", str_name));
      }
    }
    check(sz_panel_collections >= 20 && sz_region_collections >= 1,
          "chrome golden: panel collections counted");
  }

  {
    const auto res_common{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/common-metrics.yaml", "common-metrics.yaml")};
    const auto res_ra{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/ra-metrics.yaml", "ra-metrics.yaml")};
    check(res_common.has_value() && res_ra.has_value(), "chrome golden: metrics fixtures parse");
    if (!res_common.has_value() || !res_ra.has_value()) return ora::test::test_exit_code("ui");
    const auto res_metrics{ui::chrome_metrics_initialize({*res_common, *res_ra})};
    check(res_metrics.has_value(), "chrome golden: metrics load");
    if (!res_metrics.has_value()) return ora::test::test_exit_code("ui");
    std::string str_dump;
    for (const auto& [str_key, str_value] : *res_metrics) str_dump += str_key + '\t' + str_value + '\n';
    check_golden_lines("chrome metrics", split_lines(str_dump), "metrics.txt");
    check(res_metrics->contains("ButtonDepth") && res_metrics->at("ButtonDepth") == "1",
          "chrome golden: metrics merged value");
  }

  {
    const auto res_nodes{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/ingame-player.yaml", "ingame-player.yaml")};
    check(res_nodes.has_value(), "chrome golden: layout fixture parses");
    if (!res_nodes.has_value()) return ora::test::test_exit_code("ui");
    check(res_nodes->size() == 1 && (*res_nodes)[0].str_key == "Container@PLAYER_WIDGETS",
          "chrome golden: single top widget");

    const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
    check(res_loader.has_value(), "chrome golden: loader build");
    if (!res_loader.has_value()) return ora::test::test_exit_code("ui");

    ui::WidgetModel dumb_root{make_widget("Container", "ROOT")};
    const ui::WidgetInitCtx ctx_window{1024, 768, nullptr, "default"};
    const auto res_widget{ui::widget_loader_load_widget(*res_loader, dumb_root, ctx_window,
                                                        "PLAYER_WIDGETS")};
    check(res_widget.has_value(), "chrome golden: PLAYER_WIDGETS loads");
    if (!res_widget.has_value()) {
      std::println("load error: {}", res_widget.error());
      return ora::test::test_exit_code("ui");
    }
    check(dumb_root.vec_children.size() == 1, "chrome golden: mounted under dumb root");

    std::string str_dump;
    dump_widget_tree_node(str_dump, (*res_nodes)[0], dumb_root.vec_children[0].get(), 0);
    check_golden_lines("chrome tree", split_lines(str_dump), "ingame-player-tree.txt");

    ui::UiState st{};
    check(ui::ui_handle_input(dumb_root, st, make_mouse(kDown, kLeft, 60, 730)),
          "chrome tree: command bar click consumed");
    check(st.ptr_mouse_focus != nullptr, "chrome tree: command bar button takes focus");
    check(ui::ui_handle_input(dumb_root, st, make_mouse(kDown, kLeft, 60, 400)),
          "chrome tree: focused button consumes off-bounds down");
    check(ui::ui_handle_input(dumb_root, st, make_mouse(kUp, kLeft, 60, 400)),
          "chrome tree: focused button consumes up and yields");
    check(st.ptr_mouse_focus == nullptr, "chrome tree: focus yielded after up");
    check(!ui::ui_handle_input(dumb_root, st, make_mouse(kMove, mouse::MouseButton::kNone, 60, 400)),
          "chrome tree: transparent area move unhandled");
    check(!ui::ui_handle_input(dumb_root, st, make_mouse(kDown, kLeft, 60, 400)),
          "chrome tree: transparent area click unhandled");
  }

  {
    const auto opt_golden{load_golden_lines("expressions.txt")};
    if (!opt_golden.has_value()) {
      check(false, "chrome golden: expressions.txt missing");
    } else {
      const SymbolMap map_subs{{"WINDOW_WIDTH", 1024},
                               {"WINDOW_HEIGHT", 768},
                               {"PARENT_WIDTH", 238},
                               {"PARENT_HEIGHT", 250},
                               {"WIDTH", 62},
                               {"HEIGHT", 46}};
      std::size_t sz_checked{0};
      std::size_t sz_failed{0};
      for (const std::string& str_line : *opt_golden) {
        const std::size_t sz_tab{str_line.find('\t')};
        if (sz_tab == std::string::npos) continue;
        const std::string str_expr{str_line.substr(0, sz_tab)};
        const std::string str_value{str_line.substr(sz_tab + 1)};
        int int4_expect{};
        std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_expect);
        const auto res_eval{ui::widget_int_eval(str_expr, map_subs)};
        ++sz_checked;
        if (!res_eval.has_value() || *res_eval != int4_expect) {
          ++sz_failed;
          if (sz_failed <= 3)
            std::println("  expr mismatch: [{}] got {} want {}", str_expr,
                         res_eval.has_value() ? std::to_string(*res_eval) : std::string{"error"},
                         str_value);
        }
      }
      check(sz_checked >= 60 && sz_failed == 0,
            std::format("chrome expressions: {} checked {} failed", sz_checked, sz_failed));
    }
  }

  return ora::test::test_exit_code("ui");
}
