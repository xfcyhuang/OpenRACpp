import std;

import ora.game.input;
import ora.game.miniyaml;
import ora.game.render;
import ora.game.ui;
import ora.test.util;
using ora::test::check;

#ifndef ORA_WIDGETS_GOLDEN_DIR
#define ORA_WIDGETS_GOLDEN_DIR "tests/golden/widgets"
#endif

#ifndef ORA_UI_GOLDEN_DIR
#define ORA_UI_GOLDEN_DIR "tests/golden/chrome"
#endif

namespace {

namespace ui = ora::game::ui;
namespace input = ora::game::input;

std::optional<std::vector<std::uint8_t>> read_binary_file(const std::string& str_path) {
  std::ifstream fs_file{str_path, std::ios::binary};
  if (!fs_file.is_open()) return std::nullopt;
  std::vector<std::uint8_t> vec_out;
  char buf_chars[4096];
  while (fs_file.read(buf_chars, sizeof(buf_chars)) || fs_file.gcount() > 0)
    vec_out.insert(vec_out.end(), buf_chars, buf_chars + fs_file.gcount());
  return vec_out;
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

void check_golden_lines(std::string_view str_what, const std::vector<std::string>& vec_produced,
                        std::string_view str_rel) {
  const auto opt_text{ora::test::read_text_file(
      std::string{ORA_WIDGETS_GOLDEN_DIR} + "/" + std::string{str_rel})};
  if (!opt_text.has_value()) {
    check(false, std::format("{}: golden {} 缺失", str_what, str_rel));
    return;
  }
  const std::vector<std::string> vec_golden{split_lines(*opt_text)};
  const std::size_t sz_n{std::min(vec_produced.size(), vec_golden.size())};
  bool b_same{vec_produced.size() == vec_golden.size()};
  std::size_t sz_first_diff{vec_produced.size()};
  for (std::size_t sz_i{0}; sz_i < sz_n; ++sz_i)
    if (vec_produced[sz_i] != vec_golden[sz_i]) {
      b_same = false;
      sz_first_diff = sz_i;
      break;
    }
  if (b_same) {
    check(true, std::format("{}: golden {} 全等（{} 行）", str_what, str_rel, vec_produced.size()));
    return;
  }
  check(false, std::format("{}: golden {} 不一致（produced {} 行 / golden {} 行）", str_what, str_rel,
                           vec_produced.size(), vec_golden.size()));
  if (sz_first_diff < sz_n)
    std::println("  first diff at line {}: produced [{}] golden [{}]", sz_first_diff + 1,
                 vec_produced[sz_first_diff], vec_golden[sz_first_diff]);
}

std::array<std::uint32_t, 8> sha256_rounds(const std::vector<std::uint8_t>& vec1u_message) {
  constexpr std::uint32_t k1uK[64]{
      0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
      0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
      0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
      0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
      0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
      0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
      0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
      0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2};

  std::vector<std::uint8_t> vec1u_data{vec1u_message};
  const std::uint64_t u8_bit_length{static_cast<std::uint64_t>(vec1u_message.size()) * 8};
  vec1u_data.push_back(0x80);
  while (vec1u_data.size() % 64 != 56) vec1u_data.push_back(0);
  for (int int4_shift{56}; int4_shift >= 0; int4_shift -= 8)
    vec1u_data.push_back(static_cast<std::uint8_t>((u8_bit_length >> int4_shift) & 0xFF));

  std::uint32_t u4_h[8]{0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
                        0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19};

  const auto rotr{[](std::uint32_t u4_x, int int4_n) {
    return (u4_x >> int4_n) | (u4_x << (32 - int4_n));
  }};

  for (std::size_t sz_block{0}; sz_block < vec1u_data.size(); sz_block += 64) {
    std::uint32_t u4_w[64]{};
    for (int int4_i{0}; int4_i < 16; ++int4_i)
      for (int int4_b{0}; int4_b < 4; ++int4_b)
        u4_w[int4_i] = (u4_w[int4_i] << 8)
            | vec1u_data[sz_block + static_cast<std::size_t>(int4_i) * 4 + static_cast<std::size_t>(int4_b)];
    for (int int4_i{16}; int4_i < 64; ++int4_i) {
      const std::uint32_t u4_s0{
          rotr(u4_w[int4_i - 15], 7) ^ rotr(u4_w[int4_i - 15], 18) ^ (u4_w[int4_i - 15] >> 3)};
      const std::uint32_t u4_s1{
          rotr(u4_w[int4_i - 2], 17) ^ rotr(u4_w[int4_i - 2], 19) ^ (u4_w[int4_i - 2] >> 10)};
      u4_w[int4_i] = u4_w[int4_i - 16] + u4_s0 + u4_w[int4_i - 7] + u4_s1;
    }

    std::uint32_t u4_a{u4_h[0]}, u4_b{u4_h[1]}, u4_c{u4_h[2]}, u4_d{u4_h[3]};
    std::uint32_t u4_e{u4_h[4]}, u4_f{u4_h[5]}, u4_g{u4_h[6]}, u4_hh{u4_h[7]};
    for (int int4_i{0}; int4_i < 64; ++int4_i) {
      const std::uint32_t u4_s1{rotr(u4_e, 6) ^ rotr(u4_e, 11) ^ rotr(u4_e, 25)};
      const std::uint32_t u4_ch{(u4_e & u4_f) ^ (~u4_e & u4_g)};
      const std::uint32_t u4_temp1{u4_hh + u4_s1 + u4_ch + k1uK[int4_i] + u4_w[int4_i]};
      const std::uint32_t u4_s0{rotr(u4_a, 2) ^ rotr(u4_a, 13) ^ rotr(u4_a, 22)};
      const std::uint32_t u4_maj{(u4_a & u4_b) ^ (u4_a & u4_c) ^ (u4_b & u4_c)};
      const std::uint32_t u4_temp2{u4_s0 + u4_maj};
      u4_hh = u4_g;
      u4_g = u4_f;
      u4_f = u4_e;
      u4_e = u4_d + u4_temp1;
      u4_d = u4_c;
      u4_c = u4_b;
      u4_b = u4_a;
      u4_a = u4_temp1 + u4_temp2;
    }
    u4_h[0] += u4_a;
    u4_h[1] += u4_b;
    u4_h[2] += u4_c;
    u4_h[3] += u4_d;
    u4_h[4] += u4_e;
    u4_h[5] += u4_f;
    u4_h[6] += u4_g;
    u4_h[7] += u4_hh;
  }

  return {u4_h[0], u4_h[1], u4_h[2], u4_h[3], u4_h[4], u4_h[5], u4_h[6], u4_h[7]};
}

std::string sha256_hex(const std::vector<std::uint8_t>& vec1u_message) {
  std::string str_out;
  for (const std::uint32_t u4_word : sha256_rounds(vec1u_message))
    str_out += std::format("{:08x}", u4_word);
  return str_out;
}

input::MouseInput make_mouse(input::MouseInputEvent ev, input::MouseButton btn, int int4_x, int int4_y,
                             int int4_tap) {
  input::MouseInput mi{};
  mi.event = ev;
  mi.button = btn;
  mi.int4_x = int4_x;
  mi.int4_y = int4_y;
  mi.int4_tap_count = int4_tap;
  return mi;
}

constexpr input::MouseButton kLeft{input::MouseButton::kLeft};
constexpr input::MouseButton kRight{input::MouseButton::kRight};
constexpr input::MouseInputEvent kDown{input::MouseInputEvent::kDown};
constexpr input::MouseInputEvent kUp{input::MouseInputEvent::kUp};
constexpr input::MouseInputEvent kMove{input::MouseInputEvent::kMove};

ui::WidgetModel make_widget(std::string_view str_type, std::string_view str_id, int int4_x, int int4_y,
                            int int4_w, int int4_h) {
  ui::WidgetModel w{};
  w.str_type = std::string{str_type};
  w.str_id = std::string{str_id};
  w.bounds = ui::WidgetBounds{int4_x, int4_y, int4_w, int4_h};
  if (str_type == "Container") {
    w.b_click_through = true;
    w.b_ignore_mouse_over = true;
  } else if (str_type == "Image") {
    w.b_click_through = true;
  }
  return w;
}

ui::WidgetModel& add_child(ui::WidgetModel& w, ui::WidgetModel c) {
  w.vec_children.push_back(std::make_unique<ui::WidgetModel>(std::move(c)));
  ui::WidgetModel& ref{*w.vec_children.back()};
  ref.ptr_parent = &w;
  return ref;
}

const char* event_name(input::MouseInputEvent ev) {
  switch (ev) {
    case input::MouseInputEvent::kDown:
      return "Down";
    case input::MouseInputEvent::kUp:
      return "Up";
    case input::MouseInputEvent::kMove:
      return "Move";
    default:
      return "None";
  }
}

const char* button_name(input::MouseButton btn) {
  switch (btn) {
    case input::MouseButton::kLeft:
      return "Left";
    case input::MouseButton::kMiddle:
      return "Middle";
    case input::MouseButton::kRight:
      return "Right";
    default:
      return "None";
  }
}

void state_step(std::vector<std::string>& vec_lines, ui::WidgetModel& root, ui::WidgetModel& w,
                ui::UiState& st, std::string_view str_kind, input::MouseInputEvent ev,
                input::MouseButton btn, int int4_x, int int4_y, int int4_tap) {
  const std::size_t sz_event_base{st.vec_events.size()};
  const bool b_handled{ui::ui_handle_input(root, st, make_mouse(ev, btn, int4_x, int4_y, int4_tap))};

  std::string str_line{std::format("ev {} {} {},{} tap={} handled={}", event_name(ev), button_name(btn),
                                   int4_x, int4_y, int4_tap, b_handled ? 1 : 0)};
  if (str_kind == "button" || str_kind == "checkbox") {
    str_line += std::format(" dep={}", w.b_depressed ? 1 : 0);
    if (str_kind == "checkbox") str_line += std::format(" checked={}", w.b_checked ? 1 : 0);
  } else if (str_kind == "slider") {
    str_line += std::format(" value={:.6f}", w.fp4_value);
  }
  str_line += std::format(" mousefocus={} mouseover={} events=", st.ptr_mouse_focus == &w ? 1 : 0,
                          st.ptr_mouse_over == &w ? 1 : 0);
  for (std::size_t sz_i{sz_event_base}; sz_i < st.vec_events.size(); ++sz_i) {
    if (sz_i != sz_event_base) str_line += ';';
    str_line += st.vec_events[sz_i].str_kind;
  }
  vec_lines.push_back(std::move(str_line));
}

std::expected<std::map<std::string, ui::ChromeCollection>, std::string> load_chrome_collections() {
  const auto res_nodes{ora::game::miniyaml_from_file_named(
      std::string{ORA_UI_GOLDEN_DIR} + "/fixture/ra-chrome.yaml", "ra-chrome.yaml")};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());
  return ui::chrome_provider_initialize({*res_nodes});
}

}  // namespace

int main() {
  const auto res_cols{load_chrome_collections()};
  check(res_cols.has_value(), "widgets: chrome collections load");
  if (!res_cols.has_value()) {
    std::println("chrome error: {}", res_cols.error());
    return ora::test::test_exit_code("widgets");
  }

  ui::ChromeSheetSet set_sheets;
  std::vector<std::string> vec_pixel_lines;
  for (const std::string_view str_name : {"dialog.png", "glyphs.png", "sidebar.png"}) {
    const auto opt_png{read_binary_file(
        std::string{ORA_WIDGETS_GOLDEN_DIR} + "/fixture/" + std::string{str_name})};
    check(opt_png.has_value(), std::format("widgets: fixture {} present", str_name));
    if (!opt_png.has_value()) return ora::test::test_exit_code("widgets");
    const auto res_id{ui::chrome_sheet_load(set_sheets, str_name, *opt_png)};
    check(res_id.has_value(), std::format("widgets: sheet {} decode", str_name));
    if (!res_id.has_value()) {
      std::println("png error: {}", res_id.error());
      return ora::test::test_exit_code("widgets");
    }
    const ui::ChromeSheet& sheet{set_sheets.map_sheets.at(std::string{str_name})};
    std::vector<std::uint8_t> vec1u_bytes;
    vec1u_bytes.reserve(sheet.vec_u4_argb.size() * 4);
    for (const std::uint32_t u4_pixel : sheet.vec_u4_argb)
      for (int int4_shift{0}; int4_shift < 32; int4_shift += 8)
        vec1u_bytes.push_back(static_cast<std::uint8_t>((u4_pixel >> int4_shift) & 0xFF));
    vec_pixel_lines.push_back(std::format("png {} {}x{} sha256={}", str_name, sheet.int4_width,
                                          sheet.int4_height, sha256_hex(vec1u_bytes)));
    for (int int4_y{0}; int4_y < sheet.int4_height; int4_y += 53)
      for (int int4_x{0}; int4_x < sheet.int4_width; int4_x += 47)
        vec_pixel_lines.push_back(std::format(
            "p {},{} {:08X}", int4_x, int4_y,
            sheet.vec_u4_argb[static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(sheet.int4_width)
                              + static_cast<std::size_t>(int4_x)]));
  }
  check_golden_lines("chrome sheet pixels", vec_pixel_lines, "pixels.txt");

  check(set_sheets.map_sheets.at("dialog.png").vec_u4_argb.size()
            == static_cast<std::size_t>(1024) * 512,
        "widgets: dialog sheet premultiplied argb plane size");
  check(set_sheets.map_sheets.at("dialog.png").u4_id == 1
            && set_sheets.map_sheets.at("glyphs.png").u4_id == 2
            && set_sheets.map_sheets.at("sidebar.png").u4_id == 3,
        "widgets: sheet ids assigned in load order");

  {
    const auto res_bad{ora::game::render::png_decode(std::span<const std::uint8_t>{})};
    check(!res_bad.has_value(), "widgets: png decode rejects empty input");
    const std::vector<std::uint8_t> vec1u_bogus{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    check(!ora::game::render::png_decode(vec1u_bogus).has_value(), "widgets: png decode rejects bogus signature");
  }

  {
    std::vector<std::string> vec_lines;

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& b{add_child(root, make_widget("Button", "BTN", 100, 100, 80, 24))};
      b.vec_fields.emplace_back("Text", std::string{"Hello"});
      b.b_record_mouse_down = true;
      ui::UiState st{};
      vec_lines.push_back("scenario button-basic Button bounds=100,100,80,24");
      state_step(vec_lines, root, b, st, "button", kMove, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kMove, kLeft, 20, 20, 1);
      state_step(vec_lines, root, b, st, "button", kMove, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kMove, kLeft, 20, 20, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 20, 20, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& b{add_child(root, make_widget("Button", "BTN", 100, 100, 80, 24))};
      b.b_has_double_click = true;
      ui::UiState st{};
      vec_lines.push_back("scenario button-double Button double=on");
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 120, 112, 2);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& b{add_child(root, make_widget("Button", "BTN", 100, 100, 80, 24))};
      b.b_disabled = true;
      ui::UiState st{};
      vec_lines.push_back("scenario button-disabled Button disabled=1");
      state_step(vec_lines, root, b, st, "button", kMove, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 2);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& b{add_child(root, make_widget("Button", "BTN", 100, 100, 80, 24))};
      ui::UiState st{};
      vec_lines.push_back("scenario button-rightclick Button");
      state_step(vec_lines, root, b, st, "button", kDown, kRight, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kRight, 120, 112, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& b{add_child(root, make_widget("Button", "BTN", 100, 100, 80, 24))};
      ui::UiState st{};
      vec_lines.push_back("scenario button-outside-release Button");
      state_step(vec_lines, root, b, st, "button", kDown, kLeft, 120, 112, 1);
      state_step(vec_lines, root, b, st, "button", kUp, kLeft, 20, 20, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& c{add_child(root, make_widget("Checkbox", "CHK", 40, 40, 200, 20))};
      c.vec_fields.emplace_back("Text", std::string{"Option"});
      c.b_checked = true;
      ui::UiState st{};
      vec_lines.push_back("scenario checkbox-checked Checkbox bounds=40,40,200,20 checked=1");
      state_step(vec_lines, root, c, st, "checkbox", kMove, kLeft, 50, 50, 1);
      state_step(vec_lines, root, c, st, "checkbox", kDown, kLeft, 50, 50, 1);
      state_step(vec_lines, root, c, st, "checkbox", kUp, kLeft, 50, 50, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& d{add_child(root, make_widget("DropDownButton", "DROP", 10, 10, 120, 25))};
      ui::UiState st{};
      vec_lines.push_back("scenario dropdown DropDownButton bounds=10,10,120,25");
      state_step(vec_lines, root, d, st, "button", kMove, kLeft, 60, 22, 1);
      state_step(vec_lines, root, d, st, "button", kDown, kLeft, 60, 22, 1);
      state_step(vec_lines, root, d, st, "button", kMove, kLeft, 60, 22, 1);
      state_step(vec_lines, root, d, st, "button", kUp, kLeft, 60, 22, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& s{add_child(root, make_widget("Slider", "SLD", 10, 50, 200, 16))};
      s.fp4_min_value = 0.0f;
      s.fp4_max_value = 1.0f;
      s.fp4_value = 0.25f;
      s.int4_ticks = 5;
      ui::UiState st{};
      vec_lines.push_back("scenario slider Slider bounds=10,50,200,16 min=0 max=1 value=0.25");
      state_step(vec_lines, root, s, st, "slider", kDown, kLeft, 60, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kMove, kLeft, 150, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kMove, kLeft, 5, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kMove, kLeft, 300, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kUp, kLeft, 300, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kMove, kLeft, 100, 58, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& s{add_child(root, make_widget("Slider", "SLD", 10, 50, 200, 16))};
      s.fp4_min_value = 0.0f;
      s.fp4_max_value = 1.0f;
      s.fp4_value = 0.25f;
      s.b_disabled = true;
      ui::UiState st{};
      vec_lines.push_back("scenario slider-disabled Slider disabled=1");
      state_step(vec_lines, root, s, st, "slider", kDown, kLeft, 60, 58, 1);
      state_step(vec_lines, root, s, st, "slider", kMove, kLeft, 150, 58, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& bg{add_child(root, make_widget("Background", "BG", 0, 0, 200, 100))};
      ui::UiState st{};
      vec_lines.push_back("scenario background Background bounds=0,0,200,100");
      state_step(vec_lines, root, bg, st, "", kMove, kLeft, 50, 50, 1);
      state_step(vec_lines, root, bg, st, "", kDown, kLeft, 50, 50, 1);
      state_step(vec_lines, root, bg, st, "", kUp, kLeft, 50, 50, 1);
      state_step(vec_lines, root, bg, st, "", kDown, kRight, 50, 50, 1);
      state_step(vec_lines, root, bg, st, "", kMove, kLeft, 300, 300, 1);
      vec_lines.emplace_back("end");
    }

    {
      ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
      ui::WidgetModel& l{add_child(root, make_widget("Label", "LBL", 0, 0, 100, 20))};
      l.vec_fields.emplace_back("Text", std::string{"Text"});
      ui::UiState st{};
      vec_lines.push_back("scenario label Label bounds=0,0,100,20");
      state_step(vec_lines, root, l, st, "", kMove, kLeft, 50, 10, 1);
      state_step(vec_lines, root, l, st, "", kDown, kLeft, 50, 10, 1);
      state_step(vec_lines, root, l, st, "", kUp, kLeft, 50, 10, 1);
      vec_lines.emplace_back("end");
    }

    check_golden_lines("widgets state machine", vec_lines, "states.txt");
  }

  {
    const auto res_common{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/common-metrics.yaml", "common-metrics.yaml")};
    const auto res_ra{ora::game::miniyaml_from_file_named(
        std::string{ORA_UI_GOLDEN_DIR} + "/fixture/ra-metrics.yaml", "ra-metrics.yaml")};
    check(res_common.has_value() && res_ra.has_value(), "widgets: metrics fixtures parse");
    if (!res_common.has_value() || !res_ra.has_value()) return ora::test::test_exit_code("widgets");
    const auto res_metrics{ui::chrome_metrics_initialize({*res_common, *res_ra})};
    check(res_metrics.has_value(), "widgets: metrics load");
    if (!res_metrics.has_value()) return ora::test::test_exit_code("widgets");

    ui::UiRenderCtx ctx{};
    ctx.map_chrome = &*res_cols;
    ctx.set_sheets = &set_sheets;
    ctx.map_metrics = &*res_metrics;
    ui::UiState st{};
    ctx.ptr_state = &st;

    std::vector<std::string> vec_lines;

    {
      ui::WidgetModel bg{make_widget("Background", "BG", 20, 20, 340, 280)};
      vec_lines.emplace_back("state Background normal");
      for (const std::string& str_line : ui::widgets_draw_dump(bg, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }

    const struct {
      const char* sz_name;
      int int4_flags;
    } kSnapshots[]{{"normal", 0}, {"hover", 1}, {"pressed", 2}, {"disabled", 4}};

    for (const auto& [sz_name, int4_flags] : kSnapshots) {
      ui::WidgetModel b{make_widget("Button", "BTN", 100, 100, 80, 24)};
      b.vec_fields.emplace_back("Text", std::string{"Hello"});
      st.ptr_mouse_over = (int4_flags & 1) != 0 ? &b : nullptr;
      b.b_depressed = (int4_flags & 2) != 0;
      b.b_disabled = (int4_flags & 4) != 0;
      vec_lines.push_back(std::format("state Button {}", sz_name));
      for (const std::string& str_line : ui::widgets_draw_dump(b, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }

    for (const auto& [sz_name, int4_flags] : std::initializer_list<std::pair<const char*, int>>{
             {"checked", 0}, {"unchecked", 1}, {"checked-disabled", 6}}) {
      ui::WidgetModel c{make_widget("Checkbox", "CHK", 40, 40, 200, 20)};
      c.vec_fields.emplace_back("Text", std::string{"Option"});
      c.b_checked = (int4_flags & 1) == 0;
      c.b_disabled = (int4_flags & 4) != 0;
      st.ptr_mouse_over = nullptr;
      vec_lines.push_back(std::format("state Checkbox {}", sz_name));
      for (const std::string& str_line : ui::widgets_draw_dump(c, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }

    for (const auto& [sz_name, int4_variant] : std::initializer_list<std::pair<const char*, int>>{
             {"value025", 0}, {"value075", 1}, {"thumbhover", 2}, {"disabled", 3}}) {
      ui::WidgetModel s{make_widget("Slider", "SLD", 10, 50, 200, 16)};
      s.fp4_min_value = 0.0f;
      s.fp4_max_value = 1.0f;
      s.fp4_value = int4_variant == 0 ? 0.25f : int4_variant == 1 ? 0.75f : 0.5f;
      s.int4_ticks = 5;
      s.b_disabled = int4_variant == 3;
      st.ptr_mouse_over = int4_variant == 2 ? &s : nullptr;
      st.int4_last_mouse_x = 0;
      st.int4_last_mouse_y = 0;
      vec_lines.push_back(std::format("state Slider {}", sz_name));
      for (const std::string& str_line : ui::widgets_draw_dump(s, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }
    st.ptr_mouse_over = nullptr;

    for (const auto& [sz_name, int4_flags] : kSnapshots) {
      ui::WidgetModel d{make_widget("DropDownButton", "DROP", 10, 10, 120, 25)};
      d.vec_fields.emplace_back("Text", std::string{"Menu"});
      st.ptr_mouse_over = (int4_flags & 1) != 0 ? &d : nullptr;
      d.b_depressed = (int4_flags & 2) != 0;
      d.b_disabled = (int4_flags & 4) != 0;
      vec_lines.push_back(std::format("state DropDownButton {}", sz_name));
      for (const std::string& str_line : ui::widgets_draw_dump(d, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }
    st.ptr_mouse_over = nullptr;

    const struct {
      const char* sz_name;
      const char* sz_align;
      const char* sz_valign;
    } kLabelShots[]{{"left-middle", "", ""}, {"center-top", "Center", "Top"},
                    {"right-bottom", "Right", "Bottom"}};
    for (const auto& [sz_name, sz_align, sz_valign] : kLabelShots) {
      ui::WidgetModel l{make_widget("Label", "LBL", 0, 0, 120, 24)};
      l.vec_fields.emplace_back("Text", std::string{"Label"});
      if (std::string_view{sz_align}.size() > 0) {
        l.vec_fields.emplace_back("Align", std::string{sz_align});
        l.vec_fields.emplace_back("VAlign", std::string{sz_valign});
      }
      vec_lines.push_back(std::format("state Label {}", sz_name));
      for (const std::string& str_line : ui::widgets_draw_dump(l, ctx)) vec_lines.push_back(str_line);
      vec_lines.emplace_back("end");
    }

    check_golden_lines("widgets draw calls", vec_lines, "draw.txt");
  }

  {
    ui::WidgetModel s{make_widget("Slider", "SLD", 10, 50, 200, 16)};
    s.fp4_min_value = 0.0f;
    s.fp4_max_value = 1.0f;
    s.fp4_value = 0.25f;
    const ui::SliderGeometry geo{ui::widget_slider_geometry(s)};
    check(geo.int4_track_origin_x == 18 && geo.int4_track_x == 17 && geo.int4_track_y == 55
              && geo.int4_track_w == 186 && geo.int4_track_h == 5,
          "widgets: slider track geometry");
    const int int4_expect_thumb{static_cast<int>(10 + (8 + 184 * 0.25f) - 8.0f)};
    check(geo.int4_thumb_x == int4_expect_thumb && geo.int4_thumb_y == 50 && geo.int4_thumb_w == 16
              && geo.int4_thumb_h == 16,
          "widgets: slider thumb geometry");
  }

  {
    constexpr std::string_view kSliderYaml{
        "Slider@SLD:\n"
        "\tX: 10\n"
        "\tY: 50\n"
        "\tWidth: 200\n"
        "\tHeight: 16\n"
        "\tMinimumValue: 0\n"
        "\tMaximumValue: 1\n"
        "\tValue: 0.25\n"
        "\tTicks: 5\n"
        "\tTrackHeight: 5\n"};
    const auto res_nodes{ora::game::miniyaml_from_string(kSliderYaml, "slider-yaml")};
    check(res_nodes.has_value(), "widgets: slider yaml parses");
    if (res_nodes.has_value()) {
      const auto res_loader{ui::widget_loader_from_nodes(*res_nodes)};
      check(res_loader.has_value(), "widgets: slider loader build");
      if (res_loader.has_value()) {
        ui::WidgetModel root{make_widget("Container", "ROOT", 0, 0, 1024, 768)};
        ui::WidgetInitCtx ctx_init{};
        const auto res_widget{ui::widget_loader_load_widget(*res_loader, root, ctx_init, "SLD")};
        check(res_widget.has_value(), "widgets: slider widget load");
        if (!res_widget.has_value()) std::println("load error: {}", res_widget.error());
        if (res_widget.has_value()) {
          const ui::WidgetModel& s{**res_widget};
          check(s.fp4_min_value == 0.0f && s.fp4_max_value == 1.0f && s.fp4_value == 0.25f,
                "widgets: slider numeric fields loaded");
          check(s.int4_ticks == 5 && s.int4_track_height == 5, "widgets: slider int fields loaded");
        }
      }
    }
  }

  {
    check(ui::widget_stateful_image_name("button", false, false, false) == "button",
          "widgets: stateful name normal");
    check(ui::widget_stateful_image_name("button", true, true, true) == "button-disabled",
          "widgets: stateful name disabled wins");
    check(ui::widget_stateful_image_name("button", false, true, true) == "button-pressed",
          "widgets: stateful name pressed beats hover");
    check(ui::widget_stateful_image_name("button", false, false, true) == "button-hover",
          "widgets: stateful name hover");
  }

  return ora::test::test_exit_code("widgets");
}
