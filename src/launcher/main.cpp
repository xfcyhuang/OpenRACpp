import std;

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <windows.h>
import ora.game;
import ora.game.combat;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.render;
import ora.game.sound;
import ora.game.support;
import ora.game.ui;
import ora.game.world;
import ora.launcher;
import ora.platforms;


namespace replay_ns = ora::game::replay;

namespace {

namespace inp = ora::game::input;
namespace lch = ora::launcher;
namespace net = ora::game::network;
namespace opro = ora::game::production;
namespace ww = ora::game::world;
namespace hotk = ora::game::input;

constexpr int kViewW{1280};
constexpr int kViewH{1024};
constexpr int kCellPx{24};
constexpr int kRenderFrameMs{16};
constexpr int kLogicBehindDropMs{250};

volatile std::sig_atomic_t g_ctrl_c{0};

BOOL WINAPI console_ctrl_handler(DWORD) {
  g_ctrl_c = 1;
  return TRUE;
}

typedef unsigned int GLenum;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef int GLint;
typedef int GLsizei;

static void (*gl_clear_color)(GLfloat, GLfloat, GLfloat, GLfloat);
static void (*gl_clear)(GLenum);
static unsigned (*gl_get_error)();
static unsigned (*gl_create_shader)(GLenum);
static void (*gl_shader_source)(unsigned, GLsizei, const char* const*, const int*);
static void (*gl_compile_shader)(unsigned);
static void (*gl_get_shader_iv)(unsigned, GLenum, int*);
static int (*gl_create_program)();
static void (*gl_attach_shader)(unsigned, unsigned);
static void (*gl_link_program)(unsigned);
static void (*gl_use_program)(unsigned);
static void (*gl_gen_buffers)(GLsizei, unsigned*);
static void (*gl_bind_buffer)(GLenum, unsigned);
static void (*gl_buffer_data)(GLenum, long, const void*, GLenum);
static int (*gl_get_attrib_location)(unsigned, const char*);
static void (*gl_enable_vertex_attrib_array)(int);
static void (*gl_vertex_attrib_pointer)(int, GLint, GLenum, GLboolean, GLsizei, const void*);
static void (*gl_draw_arrays)(GLenum, GLint, GLsizei);
static void (*gl_gen_vertex_arrays)(GLsizei, unsigned*);
static void (*gl_bind_vertex_array)(unsigned);
static void (*gl_read_pixels)(int, int, GLsizei, GLsizei, GLenum, GLenum, void*);
static unsigned (*gl_gen_textures)(GLsizei, unsigned*);
static void (*gl_bind_texture)(GLenum, unsigned);
static void (*gl_tex_image_2d)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                               const void*);
static void (*gl_tex_parameteri)(GLenum, GLenum, GLint);
static int (*gl_get_uniform_location)(unsigned, const char*);
static void (*gl_uniform4f)(int, GLfloat, GLfloat, GLfloat, GLfloat);

constexpr GLenum GL_TEXTURE_2D{0x0DE1};
constexpr GLenum GL_TEXTURE_MIN_FILTER{0x2801};
constexpr GLenum GL_TEXTURE_MAG_FILTER{0x2800};
constexpr GLenum GL_TEXTURE_WRAP_S{0x2802};
constexpr GLenum GL_TEXTURE_WRAP_T{0x2803};
constexpr GLint GL_NEAREST{0x2600};
constexpr GLint GL_REPEAT{0x2901};
constexpr GLint GL_RGBA8{0x8058};

constexpr GLenum GL_COLOR_BUFFER_BIT{0x4000};
constexpr GLenum GL_TRIANGLES{0x0004};
constexpr GLenum GL_FLOAT{0x1406};
constexpr GLboolean GL_FALSE{0};
constexpr GLenum GL_VERTEX_SHADER{0x8B31};
constexpr GLenum GL_FRAGMENT_SHADER{0x8B30};
constexpr GLenum GL_COMPILE_STATUS{0x8B81};
constexpr GLenum GL_ARRAY_BUFFER{0x8892};
constexpr GLenum GL_STATIC_DRAW{0x88E4};
constexpr GLenum GL_RGB{0x1907};
constexpr GLenum GL_RGBA{0x1908};
constexpr GLenum GL_UNSIGNED_BYTE{0x1401};

template <typename T>
T gl_load(const char* sz_name) {
  return reinterpret_cast<T>(SDL_GL_GetProcAddress(sz_name));
}

struct Quad {
  GLfloat fp4_x{}, fp4_y{}, fp4_w{}, fp4_h{};
  GLfloat fp4_r{}, fp4_g{}, fp4_b{}, fp4_a{};
};

std::vector<Quad> vec_quads;

void push_quad(GLfloat fp4_x, GLfloat fp4_y, GLfloat fp4_w, GLfloat fp4_h, GLfloat fp4_r,
               GLfloat fp4_g, GLfloat fp4_b, GLfloat fp4_a) {
  vec_quads.push_back(Quad{fp4_x, fp4_y, fp4_w, fp4_h, fp4_r, fp4_g, fp4_b, fp4_a});
}

void push_rect_outline(const inp::PxRect& rect, GLfloat fp4_thick, GLfloat fp4_r, GLfloat fp4_g,
                       GLfloat fp4_b) {
  const GLfloat fp4_x{static_cast<GLfloat>(rect.int4_x)};
  const GLfloat fp4_y{static_cast<GLfloat>(rect.int4_y)};
  const GLfloat fp4_w{static_cast<GLfloat>(rect.int4_w)};
  const GLfloat fp4_h{static_cast<GLfloat>(rect.int4_h)};
  push_quad(fp4_x, fp4_y, fp4_w, fp4_thick, fp4_r, fp4_g, fp4_b, 1);
  push_quad(fp4_x, fp4_y + fp4_h - fp4_thick, fp4_w, fp4_thick, fp4_r, fp4_g, fp4_b, 1);
  push_quad(fp4_x, fp4_y, fp4_thick, fp4_h, fp4_r, fp4_g, fp4_b, 1);
  push_quad(fp4_x + fp4_w - fp4_thick, fp4_y, fp4_thick, fp4_h, fp4_r, fp4_g, fp4_b, 1);
}

constexpr int kFontW{5};
constexpr int kFontH{7};

constexpr const char* kFontGlyphs{"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -:.!/"};

const unsigned char* font_glyph(char ch) {
  static const unsigned char arr_font[][kFontH]{
      {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
      {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
      {0x1F, 0x10, 0x10, 0x0E, 0x10, 0x10, 0x1F}, {0x1F, 0x10, 0x10, 0x0E, 0x10, 0x10, 0x10},
      {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
      {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, {0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E},
      {0x11, 0x11, 0x12, 0x1C, 0x12, 0x11, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
      {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
      {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0E, 0x10, 0x10, 0x10},
      {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x11},
      {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
      {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
      {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
      {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
      {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0E}, {0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00},
      {0x0E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x0E}, {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04},
      {0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A}, {0x04, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x04},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
      {0x00, 0x11, 0x00, 0x00, 0x11, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11},
      {0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x11}, {0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10}};
  const char ch_upper{ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch};
  const char* ptr_found{std::strchr(kFontGlyphs, ch_upper)};
  if (ptr_found == nullptr || ch_upper == '\0') return arr_font[36];
  return arr_font[ptr_found - kFontGlyphs];
}

void push_text(std::string_view str_text, GLfloat fp4_x, GLfloat fp4_y, GLfloat fp4_scale,
               GLfloat fp4_r, GLfloat fp4_g, GLfloat fp4_b) {
  GLfloat fp4_cursor{fp4_x};
  for (const char ch : str_text) {
    if (ch == ' ') {
      fp4_cursor += (kFontW + 1) * fp4_scale;
      continue;
    }
    const unsigned char* ptr_rows{font_glyph(ch)};
    for (int int4_row{0}; int4_row < kFontH; ++int4_row)
      for (int int4_col{0}; int4_col < kFontW; ++int4_col)
        if (ptr_rows[int4_row] & (1u << (kFontW - 1 - int4_col)))
          push_quad(fp4_cursor + int4_col * fp4_scale, fp4_y + int4_row * fp4_scale, fp4_scale,
                    fp4_scale, fp4_r, fp4_g, fp4_b, 1);
    fp4_cursor += (kFontW + 1) * fp4_scale;
  }
}

void save_screenshot(SDL_Window* ptr_window, const char* sz_path) {
  int int4_w{};
  int int4_h{};
  SDL_GL_GetDrawableSize(ptr_window, &int4_w, &int4_h);
  std::vector<unsigned char> vec1u_px(static_cast<std::size_t>(int4_w) * int4_h * 3);
  gl_read_pixels(0, 0, int4_w, int4_h, GL_RGB, GL_UNSIGNED_BYTE, vec1u_px.data());

  SDL_Surface* surf_shot{
      SDL_CreateRGBSurfaceWithFormat(0, int4_w, int4_h, 24, SDL_PIXELFORMAT_RGB24)};
  auto* p1u_dst{static_cast<unsigned char*>(surf_shot->pixels)};
  const int int4_pitch{surf_shot->pitch};
  for (int int4_y{0}; int4_y < int4_h; ++int4_y)
    for (int int4_x{0}; int4_x < int4_w * 3; ++int4_x)
      p1u_dst[int4_y * int4_pitch + int4_x] =
          vec1u_px[static_cast<std::size_t>(int4_h - 1 - int4_y) * int4_w * 3 +
                   static_cast<std::size_t>(int4_x)];
  if (SDL_SaveBMP(surf_shot, sz_path) != 0)
    std::println("ora: screenshot failed: {}", SDL_GetError());
  else
    std::println("screenshot: {}", sz_path);
  SDL_FreeSurface(surf_shot);
}

struct MultiTapState {
  std::uint32_t u4_ticks{};
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_count{};
};

std::int32_t tap_count_update(MultiTapState& tap, std::uint32_t u4_ticks, std::int32_t int4_x,
                              std::int32_t int4_y) {
  const std::int64_t int8_dist{static_cast<std::int64_t>((tap.int4_x - int4_x) * (tap.int4_x - int4_x) +
                                                         (tap.int4_y - int4_y) * (tap.int4_y - int4_y))};
  if (u4_ticks - tap.u4_ticks <= 250 && int8_dist < 16)
    tap.int4_count = tap.int4_count >= 3 ? 3 : tap.int4_count + 1;
  else
    tap.int4_count = 1;
  tap.u4_ticks = u4_ticks;
  tap.int4_x = int4_x;
  tap.int4_y = int4_y;
  return tap.int4_count;
}

inp::ModifierKeys mods_from_sdl(std::uint16_t u2_mods) {
  inp::ModifierKeys mods{inp::ModifierKeys::kNone};
  if (u2_mods & KMOD_CTRL) mods |= inp::ModifierKeys::kCtrl;
  if (u2_mods & KMOD_ALT) mods |= inp::ModifierKeys::kAlt;
  if (u2_mods & KMOD_SHIFT) mods |= inp::ModifierKeys::kShift;
  return mods;
}

// autotest 步骤串拆分（逗号分隔）
std::vector<std::string> autotest_split_commas(std::string str_in) {
  std::vector<std::string> vec_out;
  std::size_t sz_comma{std::string::npos};
  while ((sz_comma = str_in.find(',')) != std::string::npos) {
    vec_out.push_back(str_in.substr(0, sz_comma));
    str_in = str_in.substr(sz_comma + 1);
  }
  vec_out.push_back(std::move(str_in));
  return vec_out;
}

// autotest 轮次表（T28 验收探针）：环境串按分号分组=每轮序列（无分号=单组，仅首轮）；
// int4_round 随相位进入递增，超出组数返回空（该轮转交互模式）
std::vector<std::string> autotest_round_steps(const char* sz_env, int& int4_round) {
  if (sz_env == nullptr || *sz_env == '\0') return {};
  std::vector<std::vector<std::string>> vec_rounds;
  std::string str_rest{sz_env};
  std::size_t sz_semi{std::string::npos};
  while ((sz_semi = str_rest.find(';')) != std::string::npos) {
    vec_rounds.push_back(autotest_split_commas(str_rest.substr(0, sz_semi)));
    str_rest = str_rest.substr(sz_semi + 1);
  }
  vec_rounds.push_back(autotest_split_commas(std::move(str_rest)));
  const int int4_current{int4_round++};
  if (static_cast<std::size_t>(int4_current) >= vec_rounds.size()) return {};
  std::println("autotest: round {} -> {} steps", int4_current + 1,
               vec_rounds[static_cast<std::size_t>(int4_current)].size());
  return vec_rounds[static_cast<std::size_t>(int4_current)];
}

struct UiState {
  inp::SelectionModel selection;
  inp::DragState drag;
  inp::ProductionPanelModel panel;
  MultiTapState tap;
  std::string str_place_item;
  bool b_quit{};
  bool b_screenshot{};
};

std::string screenshot_path(std::string_view str_name) {
  std::error_code ec;
  std::filesystem::create_directories("shots", ec);
  return std::format("shots/{}.bmp", str_name);
}

std::string resolve_tileset_path(char* sz_argv0) {
  std::error_code ec;
  const std::filesystem::path path_exe{sz_argv0 != nullptr ? sz_argv0 : ""};
  const std::filesystem::path path_next_exe{path_exe.parent_path() / "data" /
                                            "temperat.yaml"};
  if (path_exe.has_parent_path() && std::filesystem::exists(path_next_exe, ec))
    return path_next_exe.string();
  return "data/temperat.yaml";
}

lch::SessionEnv make_session_env(const lch::CliArgs& args, char* sz_argv0) {
  lch::SessionEnv env;
  env.int4_seed = args.int4_seed;
  env.str_map_path = args.str_map;
  env.str_tileset_path = resolve_tileset_path(sz_argv0);
  return env;
}

void flush_events(const lch::GameSession& s, std::size_t& sz_events_printed,
                  std::ofstream* ptr_log) {
  while (sz_events_printed < s.vec_events.size()) {
    const std::string& str_event{s.vec_events[sz_events_printed]};
    std::println("{}", str_event);
    if (ptr_log != nullptr) *ptr_log << str_event << '\n';
    ++sz_events_printed;
  }
}

void issue_orders(lch::GameSession& s, std::span<const net::OrderModel> vec_orders) {
  for (const net::OrderModel& order : vec_orders) lch::session_issue_order(s, order);
}

namespace recorder_ns_local {

void recorder_boot_packets(replay_ns::ReplayRecorderModel& recorder,
                           const std::string& str_session_yaml) {
  replay_ns::recorder_receive(recorder, 0, replay_ns::make_boot_packet("SyncInfo", str_session_yaml));
  replay_ns::recorder_receive(recorder, 0, replay_ns::make_boot_packet("StartGame", ""));
  replay_ns::recorder_receive(recorder, net::kLocalClientId,
                              net::order_packet_serialize_frame(std::vector<std::uint8_t>{}, 1));
}

void recorder_finish(replay_ns::ReplayRecorderModel& recorder,
                     const replay_ns::ReplayGameInfo& info, const std::string& str_path,
                     std::ofstream* ptr_log) {
  replay_ns::recorder_attach_metadata(recorder, replay_ns::replay_metadata_nodes(info));
  auto res_file{replay_ns::recorder_dispose(recorder)};
  if (!res_file.has_value()) {
    std::println("ora: record failed: {}", res_file.error());
    return;
  }
  std::ofstream fs_out{str_path, std::ios::binary | std::ios::trunc};
  if (!fs_out) {
    std::println("ora: cannot write replay: {}", str_path);
    return;
  }
  fs_out.write(reinterpret_cast<const char*>(res_file->data()),
               static_cast<std::streamsize>(res_file->size()));
  const std::string str_line{std::format("record: {} bytes -> {}", res_file->size(), str_path)};
  std::println("{}", str_line);
  if (ptr_log != nullptr) *ptr_log << str_line << '\n';
}

}

std::optional<int> parse_dim(std::string_view str_value, std::size_t idx_part) {
  const std::size_t idx_comma{str_value.find(',')};
  if (idx_comma == std::string_view::npos) return std::nullopt;
  const std::string_view str_part{idx_part == 0 ? str_value.substr(0, idx_comma)
                                                : str_value.substr(idx_comma + 1)};
  int int4_out{};
  const auto res{std::from_chars(str_part.data(), str_part.data() + str_part.size(), int4_out)};
  if (res.ec != std::errc{} || res.ptr != str_part.data() + str_part.size()) return std::nullopt;
  return int4_out;
}

std::vector<int> building_footprint_extent(const ww::WorldModel& w, const ww::ActorModel& actor) {
  const lch::GameSession* ptr_unused{nullptr};
  static_cast<void>(ptr_unused);
  std::vector<int> vec_extent{1, 1};
  for (const auto& def : w.vec_actor_types) {
    if (def.str_name_lower != actor.str_type) continue;
    for (const auto& tpl : def.vec_templates) {
      if (tpl.str_kind != "Building") continue;
      for (const auto& [str_key, str_value] : tpl.vec_kv) {
        if (str_key == "Dimensions") {
          const std::size_t idx_comma{str_value.find(',')};
          if (idx_comma != std::string::npos) {
            vec_extent[0] = parse_dim(str_value, 0).value_or(1);
            vec_extent[1] = parse_dim(str_value, 1).value_or(1);
          }
        }
      }
    }
  }
  return vec_extent;
}

bool placement_blocked(const lch::GameSession& s, std::string_view str_item, std::int32_t int4_x,
                       std::int32_t int4_y) {
  std::int32_t int4_w{1};
  std::int32_t int4_h{1};
  for (const auto& def : s.w.vec_actor_types) {
    if (def.str_name_lower != str_item) continue;
    for (const auto& tpl : def.vec_templates) {
      if (tpl.str_kind != "Building") continue;
      for (const auto& [str_key, str_value] : tpl.vec_kv)
        if (str_key == "Dimensions") {
          const std::size_t idx_comma{str_value.find(',')};
          if (idx_comma != std::string::npos) {
            int4_w = parse_dim(str_value, 0).value_or(1);
            int4_h = parse_dim(str_value, 1).value_or(1);
          }
        }
    }
  }
  const std::int32_t int4_map_w{s.w.econ.layer.int4_width};
  const std::int32_t int4_map_h{s.w.econ.layer.int4_height};
  if (int4_x < 0 || int4_y < 0 || int4_x + int4_w > int4_map_w || int4_y + int4_h > int4_map_h)
    return true;

  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const ww::ActorModel& other{s.w.vec_actors_all[u4_id]};
    if (other.str_type == "world" || other.str_type == "player") continue;
    const std::vector<int> vec_extent{building_footprint_extent(s.w, other)};
    const std::int32_t int4_ox{other.int4_pos_x};
    const std::int32_t int4_oy{other.int4_pos_y};
    if (int4_x < int4_ox + vec_extent[0] && int4_ox < int4_x + int4_w &&
        int4_y < int4_oy + vec_extent[1] && int4_oy < int4_y + int4_h)
      return true;
  }
  return false;
}

bool panel_done_building_hit(const std::string& str_current_type,
                             const std::vector<inp::ProductionQueueView>& vec_queues,
                             const inp::MouseInput& mi, std::string& str_item_out) {
  if (mi.event != inp::MouseInputEvent::kDown || mi.button != inp::MouseButton::kLeft)
    return false;
  const inp::PanelGeometry kGeo{kViewW, kViewH};
  if (!inp::panel_contains(kGeo, mi.int4_x, mi.int4_y)) return false;

  for (const inp::ProductionQueueView& queue : vec_queues) {
    if (queue.str_type != str_current_type) continue;
    for (std::size_t idx{0}; idx < queue.vec_items.size() && idx < 24; ++idx) {
      const inp::PxRect rect_icon{inp::panel_icon_rect(kGeo, idx)};
      if (!rect_icon.contains(mi.int4_x, mi.int4_y)) continue;
      for (const inp::ProductionItemView& item : queue.vec_queue)
        if (item.str_item == queue.vec_items[idx] && item.b_done && item.b_building) {
          str_item_out = item.str_item;
          return true;
        }
    }
  }
  return false;
}

void dispatch_mouse(lch::GameSession& s, UiState& ui,
                    const std::vector<inp::ProductionQueueView>& vec_queues,
                    const inp::MouseInput& mi) {
  const inp::PanelGeometry kGeo{kViewW, kViewH};

  if (mi.event == inp::MouseInputEvent::kDown && mi.button == inp::MouseButton::kRight &&
      !ui.str_place_item.empty()) {
    ui.str_place_item.clear();
    return;
  }

  if (!ui.str_place_item.empty() && mi.event == inp::MouseInputEvent::kDown &&
      mi.button == inp::MouseButton::kLeft && !inp::panel_contains(kGeo, mi.int4_x, mi.int4_y)) {
    const std::int32_t int4_cell_x{inp::world_px_to_cell(inp::view_to_world_px(mi.int4_x))};
    const std::int32_t int4_cell_y{inp::world_px_to_cell(inp::view_to_world_px(mi.int4_y))};
    if (!placement_blocked(s, ui.str_place_item, int4_cell_x, int4_cell_y)) {
      const std::uint32_t u4_player_actor{s.w.vec_players[s.u4_viewer].u4_actor_id};
      net::OrderModel order;
      order.str_order = "PlaceBuilding";
      order.b_has_subject = true;
      order.u4_subject = u4_player_actor;
      order.target.kind = net::TargetKind::kTerrain;
      order.target.b_cell = true;
      order.target.int4_cell_bits = static_cast<std::int32_t>(
          ((static_cast<std::uint32_t>(int4_cell_x) & 0xfff) << 20) |
          ((static_cast<std::uint32_t>(int4_cell_y) & 0xfff) << 8));
      order.target.u1_subcell = 0;
      order.opt_str_target = ui.str_place_item;
      order.u4_extra_data = u4_player_actor;
      issue_orders(s, {&order, 1});
      ui.str_place_item.clear();
    }
    return;
  }

  std::string str_done_item;
  if (panel_done_building_hit(ui.panel.str_current_type, vec_queues, mi, str_done_item)) {
    ui.str_place_item = str_done_item;
    return;
  }

  const inp::PanelDispatchResult res_panel{
      inp::panel_handle_mouse(ui.panel, kGeo, vec_queues,
                              s.w.vec_players[s.u4_viewer].u4_actor_id, mi)};
  if (res_panel.b_handled) {
    issue_orders(s, res_panel.vec_orders);
    return;
  }

  inp::selection_handle_mouse(s.w, s.u4_viewer, ui.selection, ui.drag, mi);
  const std::vector<net::OrderModel> vec_orders{
      inp::order_generator_handle_mouse(s.w, s.u4_viewer, ui.selection, ui.drag, mi)};
  issue_orders(s, vec_orders);
}

struct DemoState {
  int int4_stage{1};
  std::int32_t int4_wait{};
  int int4_seq{};
  bool b_shot_done{};
};

std::uint32_t demo_actor_of_owner(const lch::GameSession& s, std::string_view str_type,
                                  bool b_own, std::size_t idx_skip) {
  std::size_t idx_seen{0};
  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
    if (actor.str_type != str_type) continue;
    if ((actor.u4_owner == s.u4_viewer) != b_own) continue;
    if (idx_seen == idx_skip) return u4_id;
    ++idx_seen;
  }
  return 0xffffffffu;
}

inp::MouseInput demo_mouse(inp::MouseInputEvent event, inp::MouseButton button, std::int32_t int4_x,
                           std::int32_t int4_y) {
  inp::MouseInput mi;
  mi.event = event;
  mi.button = button;
  mi.int4_x = int4_x;
  mi.int4_y = int4_y;
  mi.int4_tap_count = 1;
  return mi;
}

bool demo_queue_done(const lch::GameSession& s, std::string_view str_type,
                     std::string_view str_item) {
  for (const auto& queue : s.w.prod.vec_players[s.u4_viewer].vec_queues) {
    if (queue.str_type != str_type) continue;
    for (const auto& item : queue.vec_queue)
      if (item.str_item == str_item && item.b_done) return true;
  }
  return false;
}

bool demo_events_have(const lch::GameSession& s, std::string_view str_prefix) {
  for (const std::string& str_event : s.vec_events)
    if (str_event.starts_with(str_prefix)) return true;
  return false;
}

void demo_click(lch::GameSession& s, UiState& ui,
                const std::vector<inp::ProductionQueueView>& vec_queues, std::int32_t int4_x,
                std::int32_t int4_y) {
  dispatch_mouse(s, ui, vec_queues,
                 demo_mouse(inp::MouseInputEvent::kDown, inp::MouseButton::kLeft, int4_x, int4_y));
  dispatch_mouse(s, ui, vec_queues,
                 demo_mouse(inp::MouseInputEvent::kUp, inp::MouseButton::kLeft, int4_x, int4_y));
}

void demo_click_tab(lch::GameSession& s, UiState& ui,
                    const std::vector<inp::ProductionQueueView>& vec_queues,
                    std::string_view str_type) {
  for (std::size_t idx{0}; idx < inp::kPanelTabTypes.size(); ++idx) {
    if (inp::kPanelTabTypes[idx] != str_type) continue;
    const inp::PxRect rect{inp::panel_tab_rect(inp::PanelGeometry{kViewW, kViewH}, idx)};
    demo_click(s, ui, vec_queues, rect.int4_x + rect.int4_w / 2, rect.int4_y + rect.int4_h / 2);
    return;
  }
}

void demo_click_icon(lch::GameSession& s, UiState& ui,
                     const std::vector<inp::ProductionQueueView>& vec_queues,
                     std::string_view str_tab, std::string_view str_item) {
  const inp::PanelGeometry geo{kViewW, kViewH};
  std::size_t idx_target{0};
  bool b_found{false};
  for (const inp::ProductionQueueView& queue : vec_queues) {
    if (queue.str_type != str_tab) continue;
    for (std::size_t idx_item{0}; idx_item < queue.vec_items.size(); ++idx_item)
      if (queue.vec_items[idx_item] == str_item) {
        idx_target = idx_item;
        b_found = true;
        break;
      }
  }
  if (!b_found) return;
  const inp::PxRect rect{inp::panel_icon_rect(geo, idx_target)};
  demo_click(s, ui, vec_queues, rect.int4_x + rect.int4_w / 2, rect.int4_y + rect.int4_h / 2);
}

void demo_right_click(lch::GameSession& s, UiState& ui,
                      const std::vector<inp::ProductionQueueView>& vec_queues, std::int32_t int4_x,
                      std::int32_t int4_y) {
  dispatch_mouse(s, ui, vec_queues,
                 demo_mouse(inp::MouseInputEvent::kDown, inp::MouseButton::kRight, int4_x, int4_y));
  dispatch_mouse(s, ui, vec_queues,
                 demo_mouse(inp::MouseInputEvent::kUp, inp::MouseButton::kRight, int4_x, int4_y));
}

void demo_queue_production(lch::GameSession& s, UiState& ui,
                           const std::vector<inp::ProductionQueueView>& vec_queues,
                           std::string_view str_tab, std::string_view str_item) {
  demo_click_tab(s, ui, vec_queues, str_tab);
  demo_click_icon(s, ui, vec_queues, str_tab, str_item);
}

void demo_step(lch::GameSession& s, UiState& ui, DemoState& demo,
               const std::vector<inp::ProductionQueueView>& vec_queues, SDL_Window* ptr_window) {
  if (demo.b_shot_done) {
    ++demo.int4_wait;
    if (demo.int4_wait > 3) {
      demo.b_shot_done = false;
      demo.int4_wait = 0;
      ++demo.int4_stage;
      demo.int4_seq = 0;
    }
    return;
  }

  switch (demo.int4_stage) {
    case 1: {

      if (demo.int4_wait < 12) {
        ++demo.int4_wait;
        return;
      }
      save_screenshot(ptr_window, screenshot_path("demo-01-start").c_str());
      demo.b_shot_done = true;
      return;
    }
    case 2: {
      if (demo.int4_seq == 0) {
        demo_queue_production(s, ui, vec_queues, "Building", "powr");
        demo.int4_seq = 1;
        return;
      }
      if (demo_queue_done(s, "building", "powr")) {
        save_screenshot(ptr_window, screenshot_path("demo-02-powr-done").c_str());
        demo.b_shot_done = true;
      }
      return;
    }
    case 3: {
      if (demo.int4_seq == 0) {
        demo_click_icon(s, ui, vec_queues, "Building", "powr");
        demo.int4_seq = 1;
        return;
      }
      if (demo.int4_seq == 1 && !ui.str_place_item.empty()) {
        demo_click(s, ui, vec_queues, 15 * kCellPx + 12, 23 * kCellPx + 12);
        demo.int4_seq = 2;
        return;
      }
      if (demo.int4_seq == 2 && demo_events_have(s, "placed powr id=")) {
        save_screenshot(ptr_window, screenshot_path("demo-03-powr-placed").c_str());
        demo.b_shot_done = true;
      }
      return;
    }
    case 4: {
      if (demo.int4_seq == 0) {
        demo_queue_production(s, ui, vec_queues, "Building", "barr");
        demo.int4_seq = 1;
        return;
      }
      if (demo.int4_seq == 1 && demo_queue_done(s, "building", "barr")) {
        demo_click_icon(s, ui, vec_queues, "Building", "barr");
        demo.int4_seq = 2;
        return;
      }
      if (demo.int4_seq == 2 && !ui.str_place_item.empty()) {
        demo_click(s, ui, vec_queues, 15 * kCellPx + 12, 27 * kCellPx + 12);
        demo.int4_seq = 3;
        return;
      }
      if (demo.int4_seq == 3 && demo_events_have(s, "placed barr id=")) {
        save_screenshot(ptr_window, screenshot_path("demo-04-barr-placed").c_str());
        demo.b_shot_done = true;
      }
      return;
    }
    case 5: {
      if (demo.int4_seq == 0) {
        demo_queue_production(s, ui, vec_queues, "Infantry", "e1");
        demo.int4_seq = 1;
        return;
      }
      if (demo_events_have(s, "produced e1 id=")) {
        save_screenshot(ptr_window, screenshot_path("demo-05-e1-produced").c_str());
        demo.b_shot_done = true;
      }
      return;
    }
    case 6: {
      const std::uint32_t u4_e1{demo_actor_of_owner(s, "e1", true, 1)};
      if (u4_e1 == 0xffffffffu) return;
      const ww::ActorModel& actor{s.w.vec_actors_all[u4_e1]};
      const std::int32_t int4_px_x{actor.int4_pos_x * kCellPx + 12};
      const std::int32_t int4_px_y{actor.int4_pos_y * kCellPx + 12};
      if (demo.int4_seq == 0) {
        demo_click(s, ui, vec_queues, int4_px_x, int4_px_y);
        demo.int4_seq = 1;
        return;
      }
      if (demo.int4_seq == 1 && !ui.selection.vec_actor_ids.empty()) {
        demo_right_click(s, ui, vec_queues, 10 * kCellPx + 12, 30 * kCellPx + 12);
        demo.int4_seq = 2;
        return;
      }
      if (demo.int4_seq == 2) {
        ++demo.int4_wait;
        if (demo.int4_wait > 50) {
          save_screenshot(ptr_window, screenshot_path("demo-06-move").c_str());
          demo.b_shot_done = true;
        }
      }
      return;
    }
    case 7: {
      const auto& res{s.w.prod.vec_players[s.u4_viewer].res};
      if (res.int4_resources >= 200) {
        save_screenshot(ptr_window, screenshot_path("demo-07-harvest").c_str());
        demo.b_shot_done = true;
      }
      return;
    }
    case 8: {
      const std::uint32_t u4_e1{demo_actor_of_owner(s, "e1", true, 1)};
      const std::uint32_t u4_foe{demo_actor_of_owner(s, "e1", false, 0)};

      if (demo.int4_seq == 2 &&
          (demo_events_have(s, "killed ") || u4_foe == 0xffffffffu)) {
        save_screenshot(ptr_window, screenshot_path("demo-08-attack").c_str());
        demo.b_shot_done = true;
        return;
      }
      if (u4_e1 == 0xffffffffu || u4_foe == 0xffffffffu) return;
      if (demo.int4_seq == 0) {
        const ww::ActorModel& actor{s.w.vec_actors_all[u4_e1]};
        demo_click(s, ui, vec_queues, actor.int4_pos_x * kCellPx + 12,
                   actor.int4_pos_y * kCellPx + 12);
        demo.int4_seq = 1;
        return;
      }
      if (demo.int4_seq == 1 && !ui.selection.vec_actor_ids.empty()) {
        const ww::ActorModel& foe{s.w.vec_actors_all[u4_foe]};
        demo_right_click(s, ui, vec_queues, foe.int4_pos_x * kCellPx + 12,
                         foe.int4_pos_y * kCellPx + 12);
        demo.int4_seq = 2;
        return;
      }
      return;
    }
    case 9: {

      static constexpr std::int32_t arr_cells[][2]{
          {14, 31}, {17, 31}, {20, 31}, {23, 31}, {26, 31}};
      std::int32_t int4_placed{0};
      for (const std::string& str_event : s.vec_events)
        if (str_event.starts_with("placed ftur id=")) ++int4_placed;

      if (int4_placed >= 5) {
        const auto& power{s.w.prod.vec_players[s.u4_viewer].power};
        if (power.int4_power_drained > power.int4_power_provided) {
          demo_queue_production(s, ui, vec_queues, "Defense", "ftur");
          save_screenshot(ptr_window, screenshot_path("demo-09-lowpower").c_str());
          demo.b_shot_done = true;
        }
        return;
      }

      bool b_in_queue{false};
      for (const auto& queue : s.w.prod.vec_players[s.u4_viewer].vec_queues) {
        if (queue.str_type != "defense") continue;
        for (const auto& item : queue.vec_queue)
          if (item.str_item == "ftur") b_in_queue = true;
      }
      if (!b_in_queue && ui.str_place_item.empty()) {
        demo_queue_production(s, ui, vec_queues, "Defense", "ftur");
        return;
      }
      if (ui.str_place_item.empty() && demo_queue_done(s, "defense", "ftur")) {
        demo_click_icon(s, ui, vec_queues, "Defense", "ftur");
        if (!ui.str_place_item.empty()) {
          const std::int32_t* const p4n_cell{arr_cells[int4_placed]};
          demo_click(s, ui, vec_queues, p4n_cell[0] * kCellPx + 12, p4n_cell[1] * kCellPx + 12);
        }
      }
      return;
    }
    case 10: {
      ++demo.int4_wait;
      if (demo.int4_wait > 25) {
        save_screenshot(ptr_window, screenshot_path("demo-10-final").c_str());
        std::println("demo: 10-step sequence complete, exiting");
        ui.b_quit = true;
      }
      return;
    }
    default: {
      ui.b_quit = true;
      return;
    }
  }
}

void push_world_draw(lch::GameSession& s, const UiState& ui) {
  const GLfloat fp4_map_px{static_cast<GLfloat>(s.w.econ.layer.int4_width * kCellPx)};
  const GLfloat fp4_map_py{static_cast<GLfloat>(s.w.econ.layer.int4_height * kCellPx)};
  push_quad(0, 0, fp4_map_px, fp4_map_py, 0.08f, 0.13f, 0.09f, 1);

  for (int int4_y{0}; int4_y < s.w.econ.layer.int4_height; ++int4_y)
    for (int int4_x{0}; int4_x < s.w.econ.layer.int4_width; ++int4_x) {
      const auto& cell{s.w.econ.layer.vec_cells[static_cast<std::size_t>(int4_x) *
                                                     static_cast<std::size_t>(
                                                         s.w.econ.layer.int4_height) +
                                                 static_cast<std::size_t>(int4_y)]};
      if (cell.str_type.empty()) continue;
      GLfloat fp4_g{0.55f};
      if (cell.str_type == "Gems") fp4_g = 0.85f;
      const GLfloat fp4_fill{0.4f + 0.5f * cell.u1_density / 12.0f};
      push_quad(static_cast<GLfloat>(int4_x * kCellPx) + 2,
                static_cast<GLfloat>(int4_y * kCellPx) + 2, kCellPx - 4, kCellPx - 4, fp4_fill,
                fp4_g, 0.1f, 0.9f);
    }

  for (int int4_y{0}; int4_y <= s.w.econ.layer.int4_height; int4_y += 4)
    push_quad(0, static_cast<GLfloat>(int4_y * kCellPx), fp4_map_px, 1, 0.12f, 0.2f, 0.13f, 0.5f);
  for (int int4_x{0}; int4_x <= s.w.econ.layer.int4_width; int4_x += 4)
    push_quad(static_cast<GLfloat>(int4_x * kCellPx), 0, 1, fp4_map_py, 0.12f, 0.2f, 0.13f, 0.5f);

  for (const std::uint32_t u4_id : s.w.vec_in_world) {
    const ww::ActorModel& actor{s.w.vec_actors_all[u4_id]};
    if (actor.str_type == "world" || actor.str_type == "player") continue;
    const auto opt_bounds{inp::actor_mouse_bounds(s.w, actor)};
    if (!opt_bounds.has_value()) continue;

    GLfloat fp4_r{0.9f}, fp4_g{0.2f}, fp4_b{0.2f};
    if (actor.u4_owner == s.u4_viewer) {
      fp4_r = 0.2f;
      fp4_g = 0.9f;
      fp4_b = 0.3f;
    } else if (actor.u4_owner < s.w.vec_players.size() &&
               s.w.vec_players[actor.u4_owner].b_non_combatant) {
      fp4_r = 0.85f;
      fp4_g = 0.75f;
      fp4_b = 0.2f;
    }
    push_quad(static_cast<GLfloat>(opt_bounds->int4_x),
              static_cast<GLfloat>(opt_bounds->int4_y),
              static_cast<GLfloat>(opt_bounds->int4_w),
              static_cast<GLfloat>(opt_bounds->int4_h), fp4_r, fp4_g, fp4_b, 0.95f);

    for (const lch::FighterState& fighter : s.vec_fighters) {
      if (fighter.unit.u4_id != u4_id) continue;
      const GLfloat fp4_hp{static_cast<GLfloat>(fighter.hp.int4_hp) /
                           static_cast<GLfloat>(fighter.hp.int4_max_hp)};
      push_quad(static_cast<GLfloat>(opt_bounds->int4_x),
                static_cast<GLfloat>(opt_bounds->int4_y - 4),
                static_cast<GLfloat>(opt_bounds->int4_w) * fp4_hp, 2, 1, 0.3f, 0.2f, 1);
    }

    bool b_selected{false};
    for (const std::uint32_t u4_sel : ui.selection.vec_actor_ids)
      if (u4_sel == u4_id) b_selected = true;
    if (b_selected) push_rect_outline(*opt_bounds, 2, 1, 1, 1);
  }

  if (!ui.str_place_item.empty()) {
    const int int4_mx{ui.drag.int4_mouse_x};
    const int int4_my{ui.drag.int4_mouse_y};
    const std::int32_t int4_cell_x{inp::world_px_to_cell(inp::view_to_world_px(int4_mx))};
    const std::int32_t int4_cell_y{inp::world_px_to_cell(inp::view_to_world_px(int4_my))};
    std::int32_t int4_w{1};
    std::int32_t int4_h{1};
    for (const auto& def : s.w.vec_actor_types) {
      if (def.str_name_lower != ui.str_place_item) continue;
      for (const auto& tpl : def.vec_templates)
        for (const auto& [str_key, str_value] : tpl.vec_kv)
          if (str_key == "Dimensions") {
            const std::size_t idx_comma{str_value.find(',')};
            if (idx_comma != std::string::npos) {
              int4_w = parse_dim(str_value, 0).value_or(1);
              int4_h = parse_dim(str_value, 1).value_or(1);
            }
          }
    }
    const bool b_blocked{placement_blocked(s, ui.str_place_item, int4_cell_x, int4_cell_y)};
    const GLfloat fp4_r{b_blocked ? 0.9f : 0.2f};
    const GLfloat fp4_g{b_blocked ? 0.15f : 0.9f};
    push_quad(static_cast<GLfloat>(int4_cell_x * kCellPx),
              static_cast<GLfloat>(int4_cell_y * kCellPx), static_cast<GLfloat>(int4_w * kCellPx),
              static_cast<GLfloat>(int4_h * kCellPx), fp4_r, fp4_g, 0.2f, 0.5f);
    push_text(std::format("PLACE {}", ui.str_place_item), 8, kViewH - 16, 2, 1, 0.8f, 0.2f);
  }

  if (ui.drag.b_dragging)
    push_rect_outline(
        inp::PxRect{std::min(ui.drag.int4_start_x, ui.drag.int4_mouse_x),
                    std::min(ui.drag.int4_start_y, ui.drag.int4_mouse_y),
                    std::abs(ui.drag.int4_start_x - ui.drag.int4_mouse_x),
                    std::abs(ui.drag.int4_start_y - ui.drag.int4_mouse_y)},
        1, 1, 1, 0.6f);
}

void push_panel_draw(const lch::GameSession& s, const UiState& ui,
                     const std::vector<inp::ProductionQueueView>& vec_queues) {
  constexpr inp::PanelGeometry kGeo{kViewW, kViewH};
  const inp::PxRect rect_panel{inp::panel_rect(kGeo)};
  push_quad(static_cast<GLfloat>(rect_panel.int4_x), static_cast<GLfloat>(rect_panel.int4_y),
            static_cast<GLfloat>(rect_panel.int4_w), static_cast<GLfloat>(rect_panel.int4_h),
            0.16f, 0.16f, 0.2f, 1);

  for (std::size_t idx{0}; idx < inp::kPanelTabTypes.size(); ++idx) {
    const inp::PxRect rect_tab{inp::panel_tab_rect(kGeo, idx)};
    const bool b_current{ui.panel.str_current_type == inp::kPanelTabTypes[idx]};
    bool b_enabled{false};
    for (const inp::ProductionQueueView& queue : vec_queues)
      if (queue.str_type == inp::kPanelTabTypes[idx] && queue.b_enabled) b_enabled = true;
    if (b_current)
      push_quad(static_cast<GLfloat>(rect_tab.int4_x), static_cast<GLfloat>(rect_tab.int4_y),
                static_cast<GLfloat>(rect_tab.int4_w), static_cast<GLfloat>(rect_tab.int4_h),
                0.5f, 0.7f, 0.5f, 1);
    else if (b_enabled)
      push_quad(static_cast<GLfloat>(rect_tab.int4_x), static_cast<GLfloat>(rect_tab.int4_y),
                static_cast<GLfloat>(rect_tab.int4_w), static_cast<GLfloat>(rect_tab.int4_h),
                0.3f, 0.35f, 0.3f, 1);
    else
      push_quad(static_cast<GLfloat>(rect_tab.int4_x), static_cast<GLfloat>(rect_tab.int4_y),
                static_cast<GLfloat>(rect_tab.int4_w), static_cast<GLfloat>(rect_tab.int4_h),
                0.12f, 0.12f, 0.12f, 1);
    push_text(inp::kPanelTabTypes[idx].substr(0, 1), static_cast<GLfloat>(rect_tab.int4_x + 11),
              static_cast<GLfloat>(rect_tab.int4_y + 10), 2, 1, 1, 1);
  }

  const inp::ProductionQueueView* ptr_queue{};
  for (const inp::ProductionQueueView& queue : vec_queues)
    if (queue.str_type == ui.panel.str_current_type) ptr_queue = &queue;
  if (ptr_queue == nullptr) return;

  for (std::size_t idx{0}; idx < ptr_queue->vec_items.size() && idx < 24; ++idx) {
    const inp::PxRect rect_icon{inp::panel_icon_rect(kGeo, idx)};
    push_quad(static_cast<GLfloat>(rect_icon.int4_x), static_cast<GLfloat>(rect_icon.int4_y),
              static_cast<GLfloat>(rect_icon.int4_w), static_cast<GLfloat>(rect_icon.int4_h),
              0.35f, 0.35f, 0.5f, 1);
    push_text(ptr_queue->vec_items[idx], static_cast<GLfloat>(rect_icon.int4_x + 4),
              static_cast<GLfloat>(rect_icon.int4_y + 4), 2, 1, 1, 1);

    for (const inp::ProductionItemView& item : ptr_queue->vec_queue) {
      if (item.str_item != ptr_queue->vec_items[idx]) continue;
      const std::int32_t int4_progress{inp::panel_item_progress_percent(item)};
      push_quad(static_cast<GLfloat>(rect_icon.int4_x),
                static_cast<GLfloat>(rect_icon.int4_y + rect_icon.int4_h - 8),
                static_cast<GLfloat>(rect_icon.int4_w) * int4_progress / 100, 6, 1, 0.8f, 0.1f,
                0.9f);
      if (item.b_paused)
        push_text("HOLD", static_cast<GLfloat>(rect_icon.int4_x + 4),
                  static_cast<GLfloat>(rect_icon.int4_y + 20), 2, 1, 0.6f, 0.1f);
      else if (item.b_done)
        push_text("RDY", static_cast<GLfloat>(rect_icon.int4_x + 4),
                  static_cast<GLfloat>(rect_icon.int4_y + 20), 2, 0.2f, 1, 0.3f);
      break;
    }
  }

  push_text(lch::session_hud_line(s), static_cast<GLfloat>(rect_panel.int4_x + 8),
            static_cast<GLfloat>(rect_panel.int4_y + rect_panel.int4_h - 30), 2, 1, 1, 0.5f);
  push_text(std::format("SEL {}", ui.selection.vec_actor_ids.size()),
            static_cast<GLfloat>(rect_panel.int4_x + 8),
            static_cast<GLfloat>(rect_panel.int4_y + rect_panel.int4_h - 14), 2, 1, 1, 0.5f);
}

int run_headless(const lch::CliArgs& args, std::ofstream* ptr_log, char* sz_argv0) {

  std::optional<replay_ns::ReplayFeedModel> opt_feed;
  std::int32_t int4_ticks{args.int4_headless_ticks};
  if (!args.str_replay.empty()) {
    std::ifstream fs_replay{args.str_replay, std::ios::binary};
    if (!fs_replay) {
      std::println("ora: replay file not found: {}", args.str_replay);
      return 1;
    }
    fs_replay.seekg(0, std::ios::end);
    std::vector<std::uint8_t> vec_file(static_cast<std::size_t>(fs_replay.tellg()));
    fs_replay.seekg(0, std::ios::beg);
    fs_replay.read(reinterpret_cast<char*>(vec_file.data()),
                   static_cast<std::streamsize>(vec_file.size()));

    auto res_read{replay_ns::replay_read(vec_file)};
    if (!res_read.has_value()) {
      std::println("ora: replay read failed: {}", res_read.error());
      return 1;
    }
    auto res_feed{replay_ns::replay_feed_build(*res_read)};
    if (!res_feed.has_value()) {
      std::println("ora: replay feed failed: {}", res_feed.error());
      return 1;
    }
    opt_feed.emplace(std::move(*res_feed));

    int4_ticks = opt_feed->int4_tick_count - 1;
    std::println("replay: {} seed={} net-frame-interval={} final-tick={} clients={}",
                 args.str_replay, opt_feed->session.int4_seed,
                 opt_feed->session.int4_net_frame_interval, res_read->int4_final_game_tick,
                 opt_feed->session.vec_client_indices.size());
  }

  lch::SessionEnv env{make_session_env(args, sz_argv0)};
  if (opt_feed.has_value()) env.int4_seed = opt_feed->session.int4_seed;
  auto res_session{lch::session_setup(env)};
  if (!res_session.has_value()) {
    std::println("ora: setup failed: {}", res_session.error());
    return 1;
  }
  lch::GameSession& s{*res_session};

  replay_ns::ReplayRecorderModel recorder;
  if (!args.str_record.empty()) {
    s.om.ptr_recorder = &recorder;
    const replay_ns::ReplaySessionInfo info{
        .int4_seed = env.int4_seed,
        .str_map_uid = args.str_map.empty() ? std::string{"ora-sandbox"} : args.str_map,
        .int4_net_frame_interval = net::kNetFrameInterval,
        .int4_game_timestep = args.int4_tick_ms,
        .vec_client_indices = {net::kLocalClientId}};
    recorder_ns_local::recorder_boot_packets(recorder, replay_ns::replay_session_yaml(info));
  }

  if (opt_feed.has_value()) s.om.ptr_replay_feed = &*opt_feed;

  std::size_t sz_printed{0};
  std::size_t sz_orders_fed{};
  flush_events(s, sz_printed, ptr_log);
  for (std::int32_t int4_tick{0}; int4_tick < int4_ticks; ++int4_tick) {

    if (opt_feed.has_value()) {
      while (sz_orders_fed < opt_feed->vec_replayed_orders.size()) {
        const auto& [int4_frame, vec_bytes]{opt_feed->vec_replayed_orders[sz_orders_fed++]};
        for (const net::OrderModel& order : net::order_packet_get_orders(vec_bytes, &s.w))
          s.vec_bridge_pending.emplace_back(static_cast<std::uint32_t>(int4_frame), order);
      }
    }
    lch::session_tick(s);
    if (int4_tick % 40 == 0) std::println("{}", lch::session_hud_line(s));
    flush_events(s, sz_printed, ptr_log);
    if (!s.om.str_fatal.empty()) {
      std::println("ora: order manager fatal: {}", s.om.str_fatal);
      return 1;
    }
  }

  std::println("{}", lch::session_hud_line(s));

  if (!args.str_record.empty()) {
    replay_ns::ReplayGameInfo info;
    info.str_mod = args.str_mod;
    info.str_version = "ora-m1";
    info.str_map_uid = args.str_map.empty() ? std::string{"ora-sandbox"} : args.str_map;
    info.str_map_title = args.str_map.empty() ? std::string{"ora sandbox"} : args.str_map;
    info.int4_final_game_tick = s.w.int4_world_tick;
    info.str_start_time_utc = "2026-01-01 00-00-00";
    info.vec_players.push_back(replay_ns::ReplayGameInfo::PlayerInfo{
        .int4_client_index = net::kLocalClientId, .str_name = "ora-headless", .b_is_human = true});
    recorder_ns_local::recorder_finish(recorder, info, args.str_record, ptr_log);
  }

  if (opt_feed.has_value()) {
    for (const std::string& str_report : opt_feed->vec_desync_reports)
      std::println("ora: desync: {}", str_report);
    if (s.om.b_out_of_sync || !opt_feed->vec_desync_reports.empty()) {
      std::println("ora: replay desync detected ({} reports)", opt_feed->vec_desync_reports.size());
      return 1;
    }
    std::println("replay: clean {} frames, sync hash consistent", int4_ticks);
  }

  std::println("exiting clean frames={}", int4_ticks);
  return 0;
}


namespace lob {

struct ButtonRect {
  std::string_view str_id;
  int int4_x{};
  int int4_y{};
  int int4_w{};
  int int4_h{};
};

const ora::game::ui::WidgetModel* widget_find(const ora::game::ui::WidgetModel& root,
                                              std::string_view str_id) {
  std::vector<const ora::game::ui::WidgetModel*> vec_stack{&root};
  while (!vec_stack.empty()) {
    const ora::game::ui::WidgetModel* ptr_w{vec_stack.back()};
    vec_stack.pop_back();
    if (ptr_w->str_id == str_id) return ptr_w;
    for (const auto& ptr_child : ptr_w->vec_children) vec_stack.push_back(ptr_child.get());
  }
  return nullptr;
}

ora::game::ui::WidgetBounds widget_origin_bounds(const ora::game::ui::WidgetModel& w) {
  int int4_ox{0};
  int int4_oy{0};
  const ora::game::ui::WidgetModel* ptr_iter{&w};
  while (ptr_iter->ptr_parent != nullptr) {
    int4_ox += ptr_iter->bounds.int4_x;
    int4_oy += ptr_iter->bounds.int4_y;
    ptr_iter = ptr_iter->ptr_parent;
  }
  return {int4_ox, int4_oy, w.bounds.int4_width, w.bounds.int4_height};
}

ButtonRect button_from_tree(const lch::LobbyRuntime& lobby, std::string_view str_id,
                            int int4_fx, int int4_fy, int int4_fw, int int4_fh) {
  if (lobby.b_tree_loaded) {
    if (const ora::game::ui::WidgetModel* ptr_w{widget_find(lobby.tree_lobby, str_id)}) {
      const ora::game::ui::WidgetBounds b{widget_origin_bounds(*ptr_w)};
      return {str_id, b.int4_x, b.int4_y, b.int4_width, b.int4_height};
    }
  }
  return {str_id, int4_fx, int4_fy, int4_fw, int4_fh};
}

struct PreviewQuad {
  int int4_x{};
  int int4_y{};
  int int4_w{};
  int int4_h{};
};

PreviewQuad preview_quad(int int4_png_w, int int4_png_h) {
  constexpr int kRbX{1 + 62 + 706};
  constexpr int kRbY{1 + 84 + 67};
  constexpr int kRbW{172};
  constexpr int kRbH{172};
  if (int4_png_w <= 0 || int4_png_h <= 0) return {kRbX, kRbY, kRbW, kRbH};
  const float fp4_scale{std::min(static_cast<float>(kRbW) / static_cast<float>(int4_png_w),
                                 static_cast<float>(kRbH) / static_cast<float>(int4_png_h))};
  const int int4_w{static_cast<int>(fp4_scale * static_cast<float>(int4_png_w))};
  const int int4_h{static_cast<int>(fp4_scale * static_cast<float>(int4_png_h))};
  return {kRbX + (kRbW - int4_w) / 2, kRbY + (kRbH - int4_h) / 2, int4_w, int4_h};
}

bool rect_contains(const ButtonRect& rect, int int4_x, int int4_y) {
  return int4_x >= rect.int4_x && int4_x < rect.int4_x + rect.int4_w && int4_y >= rect.int4_y
      && int4_y < rect.int4_y + rect.int4_h;
}

int run_lobby_phase(lch::LobbyRuntime& lobby, SDL_Window* ptr_window, unsigned u_prog_color) {
  const auto res_png{ora::game::render::png_decode(lobby_current_map(lobby)->vec1u_map_png)};
  std::vector<std::uint8_t> vec1u_rgba;
  int int4_png_w{0};
  int int4_png_h{0};
  if (res_png.has_value()) {
    int4_png_w = res_png->int4_width;
    int4_png_h = res_png->int4_height;
    vec1u_rgba.reserve(res_png->vec_u4_argb.size() * 4);
    for (const std::uint32_t u4_px : res_png->vec_u4_argb) {
      vec1u_rgba.push_back(static_cast<std::uint8_t>((u4_px >> 16) & 0xFF));
      vec1u_rgba.push_back(static_cast<std::uint8_t>((u4_px >> 8) & 0xFF));
      vec1u_rgba.push_back(static_cast<std::uint8_t>(u4_px & 0xFF));
      vec1u_rgba.push_back(static_cast<std::uint8_t>((u4_px >> 24) & 0xFF));
    }
  }

  const char* sz_vert_tex{"#version 150\n"
                          "in vec2 pos;\n"
                          "in vec2 uv;\n"
                          "out vec2 vuv;\n"
                          "void main() {\n"
                          "  vuv = uv;\n"
                          "  gl_Position = vec4(pos.x / 640.0 - 1.0, 1.0 - pos.y / 512.0, 0.0, 1.0);\n"
                          "}\n"};
  const char* sz_frag_tex{"#version 150\n"
                          "in vec2 vuv;\n"
                          "uniform sampler2D tex;\n"
                          "out vec4 frag;\n"
                          "void main() { frag = texture(tex, vuv); }\n"};

  const auto make_shader_lobby = [&](GLenum type, const char* sz_src) {
    const unsigned u_shader{gl_create_shader(type)};
    gl_shader_source(u_shader, 1, &sz_src, nullptr);
    gl_compile_shader(u_shader);
    return u_shader;
  };
  const unsigned u_prog_tex{static_cast<unsigned>(gl_create_program())};
  gl_attach_shader(u_prog_tex, make_shader_lobby(GL_VERTEX_SHADER, sz_vert_tex));
  gl_attach_shader(u_prog_tex, make_shader_lobby(GL_FRAGMENT_SHADER, sz_frag_tex));
  gl_link_program(u_prog_tex);
  gl_use_program(u_prog_tex);
  unsigned u_tex{0};
  gl_gen_textures(1, &u_tex);
  gl_bind_texture(GL_TEXTURE_2D, u_tex);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  if (!vec1u_rgba.empty())
    gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8, int4_png_w, int4_png_h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, vec1u_rgba.data());
  const int idx_pos_tex{gl_get_attrib_location(static_cast<int>(u_prog_tex), "pos")};
  const int idx_uv_tex{gl_get_attrib_location(static_cast<int>(u_prog_tex), "uv")};
  gl_enable_vertex_attrib_array(idx_pos_tex);
  gl_vertex_attrib_pointer(idx_pos_tex, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
  gl_enable_vertex_attrib_array(idx_uv_tex);
  gl_vertex_attrib_pointer(idx_uv_tex, 2, GL_FLOAT, GL_FALSE, 16,
                           reinterpret_cast<const void*>(8));
  const int idx_pos_color{gl_get_attrib_location(u_prog_color, "pos")};
  const int idx_col_color{gl_get_attrib_location(u_prog_color, "col")};

  const ButtonRect rect_change{
      button_from_tree(lobby, "CHANGEMAP_BUTTON", 768, 375, 174, 25)};
  const ButtonRect rect_start{
      button_from_tree(lobby, "START_GAME_BUTTON", 692, 639, 120, 25)};
  const ButtonRect rect_quit{
      button_from_tree(lobby, "DISCONNECT_BUTTON", 822, 639, 120, 25)};
  const ButtonRect rect_faction{"FACTION_ROW", 330, 210, 150, 25};
  const ButtonRect rect_team{"TEAM_ROW", 490, 210, 60, 25};

  bool b_quit{false};
  bool b_start{false};
  std::uint32_t u4_next_render{SDL_GetTicks()};

  // ORA_LOBBY_AUTOTEST 驱动面（T28 验收探针，分号轮次表）：
  // 每轮逗号分隔 wait<n>/shot<name>/cyclemap/start/disconnect；截图落 ORA_FLOW_SHOT_DIR
  static int int4_lobby_round{0};
  const std::vector<std::string> vec_autotest{
      autotest_round_steps(std::getenv("ORA_LOBBY_AUTOTEST"), int4_lobby_round)};
  std::string str_shot_dir{"."};
  if (const char* sz_shot{std::getenv("ORA_FLOW_SHOT_DIR")}; sz_shot != nullptr && *sz_shot != '\0')
    str_shot_dir = sz_shot;
  std::size_t sz_next_step{0};
  std::uint64_t u8_frames{0};
  std::uint64_t u8_step_ready_frame{5};

  while (!b_quit && !b_start) {
    if (g_ctrl_c) b_quit = true;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) b_quit = true;
      if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode key{event.key.keysym.sym};
        if (key == SDLK_ESCAPE) b_quit = true;
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
          if (lch::lobby_request_start(lobby)) b_start = true;
        }
        if (key == SDLK_p && (event.key.keysym.mod & KMOD_CTRL))
          save_screenshot(ptr_window, screenshot_path("lobby").c_str());
      }
      if (event.type == SDL_MOUSEBUTTONDOWN) {
        const int int4_x{event.button.x};
        const int int4_y{event.button.y};
        const bool b_left{event.button.button == SDL_BUTTON_LEFT};
        const bool b_right{event.button.button == SDL_BUTTON_RIGHT};
        if (!b_left && !b_right) continue;
        const int int4_dir{b_left ? 1 : -1};
        if (rect_contains(rect_change, int4_x, int4_y)) lch::lobby_cycle_map(lobby, int4_dir);
        else if (rect_contains(rect_faction, int4_x, int4_y))
          lch::lobby_cycle_faction(lobby, int4_dir);
        else if (rect_contains(rect_team, int4_x, int4_y))
          lch::lobby_cycle_team(lobby, int4_dir);
        else if (b_left && rect_contains(rect_start, int4_x, int4_y)) {
          if (lch::lobby_request_start(lobby)) b_start = true;
        } else if (b_left && rect_contains(rect_quit, int4_x, int4_y))
          b_quit = true;
      }
    }

    const std::uint32_t u4_now{SDL_GetTicks()};
    if (static_cast<std::int32_t>(u4_now - u4_next_render) < 0) {
      const std::int32_t int4_sleep{static_cast<std::int32_t>(u4_next_render - u4_now)};
      SDL_Delay(static_cast<std::uint32_t>(int4_sleep));
      continue;
    }
    u4_next_render += kRenderFrameMs;

    vec_quads.clear();
    push_quad(0, 0, kViewW, kViewH, 0.05f, 0.07f, 0.1f, 1);
    push_quad(62, 84, 900, 600, 0.08f, 0.1f, 0.14f, 0.96f);
    push_quad(64, 86, 896, 596, 0.11f, 0.13f, 0.18f, 0.96f);

    const lch::LobbyMapEntry* ptr_map{lch::lobby_current_map(lobby)};
    if (ptr_map != nullptr) {
      push_quad(768, 151, 174, 250, 0.05f, 0.06f, 0.08f, 1);
      std::string str_map_row{ptr_map->str_name};
      for (char& ch : str_map_row)
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
      const PreviewQuad pq{preview_quad(int4_png_w, int4_png_h)};
      push_text(str_map_row, static_cast<GLfloat>(pq.int4_x), 336.0f, 2, 1, 0.85f, 0.4f);
    }

    push_text("ORA SKIRMISH LOBBY", 480.0f, 100.0f, 3, 1, 0.85f, 0.4f);

    {
      int int4_row_y{180};
      for (const auto& [str_slot, slot] : lobby.server.lobby_info.vec_slots) {
        const net::LobbyClient* ptr_client{lobby.server.lobby_info.client_in_slot(str_slot)};
        std::string str_line{std::format("{}  {}", str_slot,
                                         ptr_client == nullptr ? std::string{"OPEN"}
                                                               : ptr_client->str_name)};
        for (char& ch : str_line)
          if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        push_text(str_line, 82.0f, static_cast<GLfloat>(int4_row_y), 2, 0.9f, 0.9f, 0.9f);
        push_text(ptr_client == nullptr ? "-" : lch::lobby_faction_display(lobby),
                  static_cast<GLfloat>(rect_faction.int4_x + 8),
                  static_cast<GLfloat>(int4_row_y), 2, 0.5f, 0.85f, 1);
        push_text(ptr_client == nullptr ? "-" : lch::lobby_team_display(lobby),
                  static_cast<GLfloat>(rect_team.int4_x + 8),
                  static_cast<GLfloat>(int4_row_y), 2, 1, 0.8f, 0.3f);
        int4_row_y += 34;
      }
      push_text("SLOT       NAME      FACTION  TEAM", 82.0f, 160.0f, 1, 0.6f, 0.6f, 0.6f);
    }

    const auto upper_label = [](std::string str_in) {
      for (char& ch : str_in)
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
      return str_in;
    };
    const auto draw_button = [&upper_label](const ButtonRect& rect, const std::string& str_label,
                                            bool b_accent) {
      const GLfloat fr{b_accent ? 0.25f : 0.16f};
      const GLfloat fg{b_accent ? 0.45f : 0.2f};
      const GLfloat fb{b_accent ? 0.2f : 0.26f};
      push_quad(static_cast<GLfloat>(rect.int4_x), static_cast<GLfloat>(rect.int4_y),
                static_cast<GLfloat>(rect.int4_w), static_cast<GLfloat>(rect.int4_h), fr, fg, fb,
                1);
      push_quad(static_cast<GLfloat>(rect.int4_x), static_cast<GLfloat>(rect.int4_y),
                static_cast<GLfloat>(rect.int4_w), 2.0f, 0.5f, 0.6f, 0.7f, 1);
      push_text(upper_label(str_label), static_cast<GLfloat>(rect.int4_x + 8),
                static_cast<GLfloat>(rect.int4_y + 6), 2, 1, 1, 1);
    };

    draw_button(rect_change, "< MAP >", false);
    draw_button(rect_faction,
                std::format("< {} >", lch::lobby_faction_display(lobby)), false);
    draw_button(rect_team, std::format("< {} >", lch::lobby_team_display(lobby)), false);
    draw_button(rect_start, "START GAME", true);
    draw_button(rect_quit, "DISCONNECT", false);

    push_text("LEFT CLICK: NEXT   RIGHT CLICK: PREV   ENTER: START   CTRL P: SCREENSHOT",
              90.0f, 700.0f, 1, 0.55f, 0.6f, 0.65f);

    std::vector<GLfloat> vec_verts;
    vec_verts.reserve(vec_quads.size() * 30);
    for (const Quad& quad : vec_quads) {
      const GLfloat fp4_verts[]{quad.fp4_x, quad.fp4_y, quad.fp4_r, quad.fp4_g, quad.fp4_b,
                                quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g,
                                quad.fp4_b, quad.fp4_a,
                                quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g,
                                quad.fp4_b, quad.fp4_a,
                                quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g,
                                quad.fp4_b, quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g,
                                quad.fp4_b, quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y + quad.fp4_h, quad.fp4_r,
                                quad.fp4_g, quad.fp4_b, quad.fp4_a};
      vec_verts.insert(vec_verts.end(), fp4_verts, fp4_verts + 36);
    }
    gl_use_program(u_prog_color);
    gl_enable_vertex_attrib_array(idx_pos_color);
    gl_vertex_attrib_pointer(idx_pos_color, 2, GL_FLOAT, GL_FALSE, 24, nullptr);
    gl_enable_vertex_attrib_array(idx_col_color);
    gl_vertex_attrib_pointer(idx_col_color, 4, GL_FLOAT, GL_FALSE, 24,
                             reinterpret_cast<const void*>(8));
    gl_clear_color(0, 0, 0, 1);
    gl_clear(GL_COLOR_BUFFER_BIT);
    gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(vec_verts.size() * sizeof(GLfloat)),
                   vec_verts.data(), GL_STATIC_DRAW);
    gl_draw_arrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_quads.size() * 6));

    if (!vec1u_rgba.empty()) {
      const PreviewQuad pq{preview_quad(int4_png_w, int4_png_h)};
      const GLfloat fp4_verts[]{
          static_cast<GLfloat>(pq.int4_x), static_cast<GLfloat>(pq.int4_y), 0, 0,
          static_cast<GLfloat>(pq.int4_x + pq.int4_w), static_cast<GLfloat>(pq.int4_y), 1, 0,
          static_cast<GLfloat>(pq.int4_x), static_cast<GLfloat>(pq.int4_y + pq.int4_h), 0, 1,
          static_cast<GLfloat>(pq.int4_x), static_cast<GLfloat>(pq.int4_y + pq.int4_h), 0, 1,
          static_cast<GLfloat>(pq.int4_x + pq.int4_w), static_cast<GLfloat>(pq.int4_y), 1, 0,
          static_cast<GLfloat>(pq.int4_x + pq.int4_w), static_cast<GLfloat>(pq.int4_y + pq.int4_h),
          1, 1};
      gl_use_program(u_prog_tex);
      gl_bind_texture(GL_TEXTURE_2D, u_tex);
      gl_enable_vertex_attrib_array(idx_pos_tex);
      gl_vertex_attrib_pointer(idx_pos_tex, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
      gl_enable_vertex_attrib_array(idx_uv_tex);
      gl_vertex_attrib_pointer(idx_uv_tex, 2, GL_FLOAT, GL_FALSE, 16,
                               reinterpret_cast<const void*>(8));
      gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(sizeof fp4_verts), fp4_verts,
                     GL_STATIC_DRAW);
      gl_draw_arrays(GL_TRIANGLES, 0, 6);
    }

    SDL_GL_SwapWindow(ptr_window);
    ++u8_frames;
    // autotest 步进：wait<n> 定帧，其余步骤每 3 帧一步（画面刷新间隙）
    if (sz_next_step < vec_autotest.size() && u8_frames >= u8_step_ready_frame) {
      const std::string& str_step{vec_autotest[sz_next_step]};
      if (str_step.starts_with("wait")) {
        u8_step_ready_frame = u8_frames + std::stoull(str_step.substr(4));
      } else {
        if (str_step.starts_with("shot")) {
          const std::string str_path{str_shot_dir + "/" + str_step.substr(4) + ".bmp"};
          save_screenshot(ptr_window, str_path.c_str());
          std::println("lobby-autotest: shot {}", str_path);
        } else if (str_step == "cyclemap") {
          lch::lobby_cycle_map(lobby, 1);
        } else if (str_step == "start") {
          if (lch::lobby_request_start(lobby)) b_start = true;
        } else if (str_step == "disconnect") {
          b_quit = true;
        }
        u8_step_ready_frame = u8_frames + 3;
      }
      ++sz_next_step;
    }
    if (sz_next_step >= vec_autotest.size() && !vec_autotest.empty()
        && u8_frames >= u8_step_ready_frame + 30)
      b_quit = true;  // 序列放完自动断开（防呆：disconnect 步缺失时也收尾）
  }

  // 退出前恢复 color program 顶点布局——对局循环假设 pos(stride24)+col(stride24)
  // 初始布局仍生效，不恢复会把纹理 pass 的 pos/uv(stride16) 布局带进对局首帧。
  gl_use_program(u_prog_color);
  gl_enable_vertex_attrib_array(idx_pos_color);
  gl_vertex_attrib_pointer(idx_pos_color, 2, GL_FLOAT, GL_FALSE, 24, nullptr);
  gl_enable_vertex_attrib_array(idx_col_color);
  gl_vertex_attrib_pointer(idx_col_color, 4, GL_FLOAT, GL_FALSE, 24,
                           reinterpret_cast<const void*>(8));

  return b_start ? 1 : 0;
}

}

// —— T28 全流程共用装载面前向声明（定义在 menu 相位区，菜单/会话两相位共用）——
std::optional<std::filesystem::path> find_golden_root(char* sz_argv0);
std::string menu_settings_path();
ora::game::SettingsModel flow_settings_load(const char* sz_where);
std::optional<hotk::HotkeyRegistryModel> flow_hotkey_registry_load(
    const std::filesystem::path& path_golden, ora::game::SettingsModel& sm);
std::vector<std::pair<std::string, ora::game::music_info>> flow_music_library(
    const std::filesystem::path& path_golden);
inp::Keycode sdl_keycode_to_ora(SDL_Keycode key);

// T28：大堂+对局会话窗口。ptr_flow 非空 = 菜单全流程模式（DISCONNECT 上报后由
// run_flow 回菜单相位）；nullptr = CLI 直进（--lobby/--map/--demo：DISCONNECT 即退出）。
int run_window(const lch::CliArgs& args, std::ofstream* ptr_log, char* sz_argv0,
               lch::FlowState* ptr_flow) {
  // 全流程共用装载面：设置（音量/mute/vsync 生效全链）+ 热键（改键重进生效）+
  // 音乐曲库（对局 BGM；上游 mods/ra/audio/music.yaml 真源语料）
  ora::game::SettingsModel sm{flow_settings_load("session")};
  hotk::HotkeyRegistryModel hotkeys{};
  std::vector<std::pair<std::string, ora::game::music_info>> vec_music_library;
  if (const auto opt_golden{find_golden_root(sz_argv0)}; opt_golden.has_value()) {
    if (const auto opt_hotkeys{flow_hotkey_registry_load(*opt_golden, sm)}; opt_hotkeys.has_value())
      hotkeys = std::move(*opt_hotkeys);
    vec_music_library = flow_music_library(*opt_golden);
  } else {
    std::println("flow: golden corpus not found, hotkeys/music defaults");
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::println("ora: SDL_Init failed: {}", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_Window* ptr_window{
      SDL_CreateWindow("ora sandbox", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kViewW,
                       kViewH, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI)};
  if (ptr_window == nullptr) {
    std::println("ora: window creation failed: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext ctx{SDL_GL_CreateContext(ptr_window)};
  if (ctx == nullptr) {
    std::println("ora: GL context failed: {}", SDL_GetError());
    SDL_DestroyWindow(ptr_window);
    SDL_Quit();
    return 1;
  }

  gl_clear_color = gl_load<decltype(gl_clear_color)>("glClearColor");
  gl_clear = gl_load<decltype(gl_clear)>("glClear");
  gl_get_error = gl_load<decltype(gl_get_error)>("glGetError");
  gl_create_shader = gl_load<decltype(gl_create_shader)>("glCreateShader");
  gl_shader_source = gl_load<decltype(gl_shader_source)>("glShaderSource");
  gl_compile_shader = gl_load<decltype(gl_compile_shader)>("glCompileShader");
  gl_get_shader_iv = gl_load<decltype(gl_get_shader_iv)>("glGetShaderiv");
  gl_create_program = gl_load<decltype(gl_create_program)>("glCreateProgram");
  gl_attach_shader = gl_load<decltype(gl_attach_shader)>("glAttachShader");
  gl_link_program = gl_load<decltype(gl_link_program)>("glLinkProgram");
  gl_use_program = gl_load<decltype(gl_use_program)>("glUseProgram");
  gl_gen_buffers = gl_load<decltype(gl_gen_buffers)>("glGenBuffers");
  gl_bind_buffer = gl_load<decltype(gl_bind_buffer)>("glBindBuffer");
  gl_buffer_data = gl_load<decltype(gl_buffer_data)>("glBufferData");
  gl_get_attrib_location = gl_load<decltype(gl_get_attrib_location)>("glGetAttribLocation");
  gl_enable_vertex_attrib_array =
      gl_load<decltype(gl_enable_vertex_attrib_array)>("glEnableVertexAttribArray");
  gl_vertex_attrib_pointer =
      gl_load<decltype(gl_vertex_attrib_pointer)>("glVertexAttribPointer");
  gl_draw_arrays = gl_load<decltype(gl_draw_arrays)>("glDrawArrays");
  gl_gen_vertex_arrays = gl_load<decltype(gl_gen_vertex_arrays)>("glGenVertexArrays");
  gl_bind_vertex_array = gl_load<decltype(gl_bind_vertex_array)>("glBindVertexArray");
  gl_read_pixels = gl_load<decltype(gl_read_pixels)>("glReadPixels");
  gl_gen_textures = gl_load<decltype(gl_gen_textures)>("glGenTextures");
  gl_bind_texture = gl_load<decltype(gl_bind_texture)>("glBindTexture");
  gl_tex_image_2d = gl_load<decltype(gl_tex_image_2d)>("glTexImage2D");
  gl_tex_parameteri = gl_load<decltype(gl_tex_parameteri)>("glTexParameteri");
  gl_get_uniform_location =
      gl_load<decltype(gl_get_uniform_location)>("glGetUniformLocation");

  const char* sz_vert{"#version 150\n"
                      "in vec2 pos;\n"
                      "in vec4 col;\n"
                      "out vec4 vcol;\n"
                      "void main() {\n"
                      "  vcol = col;\n"
                      "  gl_Position = vec4(pos.x / 640.0 - 1.0, 1.0 - pos.y / 512.0, 0.0, 1.0);\n"
                      "}\n"};
  const char* sz_frag{"#version 150\n"
                      "in vec4 vcol;\n"
                      "out vec4 frag;\n"
                      "void main() { frag = vcol; }\n"};

  const auto make_shader = [&](GLenum type, const char* sz_src) {
    const unsigned u_shader{gl_create_shader(type)};
    gl_shader_source(u_shader, 1, &sz_src, nullptr);
    gl_compile_shader(u_shader);
    int int4_ok{0};
    gl_get_shader_iv(u_shader, GL_COMPILE_STATUS, &int4_ok);
    if (!int4_ok) {
      std::println("ora: shader compile failed");
      std::exit(1);
    }
    return u_shader;
  };

  const unsigned u_prog{static_cast<unsigned>(gl_create_program())};
  gl_attach_shader(u_prog, make_shader(GL_VERTEX_SHADER, sz_vert));
  gl_attach_shader(u_prog, make_shader(GL_FRAGMENT_SHADER, sz_frag));
  gl_link_program(u_prog);
  gl_use_program(u_prog);

  unsigned u_vao{0};
  unsigned u_vbo{0};
  gl_gen_vertex_arrays(1, &u_vao);
  gl_bind_vertex_array(u_vao);
  gl_gen_buffers(1, &u_vbo);
  gl_bind_buffer(GL_ARRAY_BUFFER, u_vbo);
  const int idx_pos{gl_get_attrib_location(u_prog, "pos")};
  const int idx_col{gl_get_attrib_location(u_prog, "col")};
  gl_enable_vertex_attrib_array(idx_pos);
  gl_vertex_attrib_pointer(idx_pos, 2, GL_FLOAT, GL_FALSE, 24, nullptr);
  gl_enable_vertex_attrib_array(idx_col);
  gl_vertex_attrib_pointer(idx_col, 4, GL_FLOAT, GL_FALSE, 24,
                           reinterpret_cast<const void*>(8));

  // 音频门面（T28 全链贯通：设置驱动音量/mute；engine 生命周期跨对局轮——
  // 上游 Game.Sound 进程级单例的会话内等价）
  ora::game::sound_facade sound{
      ora::game::make_sound_facade_settings(sm.sound.str_device, sm.sound.b_mute, sm.sound)};
  std::println("sound: {} engine (sound={:.3f} music={:.3f} mute={})",
               ora::platforms::engine_dummy(sound.engine) ? "dummy" : "openal",
               sm.sound.fp_sound_volume, sm.sound.fp_music_volume, sm.sound.b_mute ? 1 : 0);

  // 对局轮出口：kAborted=ESC（上游 ABORT_MISSION→重进大堂）、kWindowClosed=窗口
  // 关闭/Ctrl+C（进程退出）、kSetupFailed=装载失败（进程退出）
  enum class GameRoundExit { kAborted, kWindowClosed, kSetupFailed };

  // 对局轮（T28 串联）：session 装配 + 事件音效 + 对局 BGM（上游 MusicPlaylist
  // .PostWorldLoaded 自动 Play 折叠）+ 输入/渲染循环 + 轮末清理
  const auto game_round{[&](std::optional<lch::LobbyStartConfig> opt_lobby_start) {
    lch::SessionEnv env{make_session_env(args, sz_argv0)};
    if (opt_lobby_start.has_value()) {
      env.str_map_path = opt_lobby_start->str_map_path;
      env.int4_seed = opt_lobby_start->int4_seed;
      opro::LobbyModel lobby_model;
      lobby_model.vec_playable_factions = opt_lobby_start->vec_factions;
      lobby_model.int4_starting_cash = 5000;
      lobby_model.str_techlevel = "unrestricted";
      lobby_model.b_build_radius = true;
      env.opt_lobby = std::move(lobby_model);
    }
    auto res_session{lch::session_setup(env)};
    if (!res_session.has_value()) {
      std::println("ora: setup failed: {}", res_session.error());
      return GameRoundExit::kSetupFailed;
    }
    lch::GameSession& s{*res_session};
    ora::game::support::MersenneTwister rng_cosmetic{
        ora::game::support::make_mersenne_twister(static_cast<std::int32_t>(SDL_GetTicks()))};
    ora::game::sfx_sound_rules sfx_rules{};
    ora::game::sfx_context sfx{.ptr_facade = &sound,
                               .ptr_rules = &sfx_rules,
                               .ptr_rng_cosmetic = &rng_cosmetic};
    s.w.ptr_sfx = &sfx;
    ora::platforms::engine_set_listener_position(
        sound.engine,
        ora::platforms::wpos{.int4_x = s.w.econ.map_move.int4_width * 1024 / 2,
                             .int4_y = s.w.econ.map_move.int4_height * 1024 / 2,
                             .int4_z = 2133});
    // 对局 BGM（T20 music_player 接线）：PostWorldLoaded 自动播放（无存档装载恒真）；
    // 无内容包=曲库全缺（probe 后 0 playable）→ 播放决策发起但引擎层静默
    //（上游 MusicController.Play 对 !Exists 曲静默 return 的等价，D070/D074 口径）
    std::optional<ora::game::music_player> opt_music;
    if (!vec_music_library.empty()) {
      opt_music = ora::game::make_music_player(sound, sm.sound, vec_music_library, rng_cosmetic,
                                               ora::game::music_playlist_info{});
      ora::game::music_player_play(*opt_music);
      if (opt_music->str_current_song.has_value())
        std::println("music: autoplay '{}' ({} tracks)",
                     *opt_music->str_current_song, vec_music_library.size());
      else
        std::println("music: autoplay silent ({} tracks, 0 installed content)", 
                     vec_music_library.size());
    }
    lch::session_tick(s);

    UiState ui;
    ui.panel.str_current_type = "Building";
    DemoState demo;
    bool b_window_closed{false};
    std::size_t sz_printed{0};
    flush_events(s, sz_printed, ptr_log);

    // ORA_GAME_AUTOTEST 驱动面（T28 验收探针，分号轮次表）：wait<n>/shot<name>/abort
    static int int4_game_round{0};
    const std::vector<std::string> vec_game_steps{
        autotest_round_steps(std::getenv("ORA_GAME_AUTOTEST"), int4_game_round)};
    std::string str_game_shot_dir{"."};
    if (const char* sz_shot{std::getenv("ORA_FLOW_SHOT_DIR")}; sz_shot != nullptr && *sz_shot != '\0')
      str_game_shot_dir = sz_shot;
    std::size_t sz_game_next_step{0};
    std::uint64_t u8_game_step_ready_frame{5};

    std::uint64_t u8_frames{0};
    std::uint32_t u4_next_logic{SDL_GetTicks()};
    std::uint32_t u4_next_render{SDL_GetTicks()};

    while (!ui.b_quit) {
      const std::uint32_t u4_now{SDL_GetTicks()};

      if (g_ctrl_c) {
        ui.b_quit = true;
        b_window_closed = true;
      }

      if (u4_now - u4_next_logic > kLogicBehindDropMs) u4_next_logic = u4_now;
      if (static_cast<std::int32_t>(u4_now - u4_next_logic) >= 0) {
        u4_next_logic += static_cast<std::uint32_t>(args.int4_tick_ms);
        ora::game::sfx_context_set_tick(sfx, s.w.int4_world_tick);
        lch::session_tick(s);
        static_cast<void>(ora::game::sound_facade_tick(sound));
        if (opt_music.has_value()) ora::game::music_player_tick(*opt_music);
        flush_events(s, sz_printed, ptr_log);
      if (!s.om.str_fatal.empty()) {
        std::println("ora: order manager fatal: {}", s.om.str_fatal);
        ui.b_quit = true;
      }
    }

    if (static_cast<std::int32_t>(u4_now - u4_next_render) >= 0) {
      u4_next_render += kRenderFrameMs;

      const std::vector<inp::ProductionQueueView> vec_queues{lch::session_queue_views(s)};

      if (args.b_demo) demo_step(s, ui, demo, vec_queues, ptr_window);

      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          ui.b_quit = true;
          b_window_closed = true;
        }
        if (event.type == SDL_KEYDOWN) {
          const SDL_Keycode key{event.key.keysym.sym};
          // ESC = ABORT_MISSION 折叠（上游 IngameMenu 确认对话框直通，D087 先例）
          if (key == SDLK_ESCAPE) ui.b_quit = true;
          if (key == SDLK_p && (event.key.keysym.mod & KMOD_CTRL)) ui.b_screenshot = true;
          // 控制组经热键注册表（上游 ControlGroupsWidget.HandleKeyPress——T22 消费面，
          // 改键重进生效链；默认 NUMBER_1..9 与原 SDL 硬编码等价）
          inp::KeyInput ki{};
          ki.event = inp::KeyInputEvent::kDown;
          ki.key = sdl_keycode_to_ora(key);
          ki.mods = mods_from_sdl(event.key.keysym.mod);
          ki.int4_tap_count = 1;
          const auto res_group{
              inp::control_groups_handle_key(hotkeys, ui.selection, s.w, s.u4_viewer, ki)};
          static_cast<void>(res_group);  // opt_center_group 视口跳转折叠（观感票）
        }
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
            event.type == SDL_MOUSEMOTION) {
          const bool b_down{event.type == SDL_MOUSEBUTTONDOWN};
          const std::int32_t int4_x{
              event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x};
          const std::int32_t int4_y{
              event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y};
          ui.drag.int4_mouse_x = int4_x;
          ui.drag.int4_mouse_y = int4_y;
          inp::MouseButton button{inp::MouseButton::kNone};
          if (event.type != SDL_MOUSEMOTION) {
            if (event.button.button == SDL_BUTTON_LEFT) button = inp::MouseButton::kLeft;
            if (event.button.button == SDL_BUTTON_RIGHT) button = inp::MouseButton::kRight;
            if (event.button.button == SDL_BUTTON_MIDDLE) button = inp::MouseButton::kMiddle;
          }
          inp::MouseInput mi;
          mi.int4_x = int4_x;
          mi.int4_y = int4_y;
          mi.mods = mods_from_sdl(SDL_GetModState());
          if (event.type == SDL_MOUSEMOTION) {
            mi.event = inp::MouseInputEvent::kMove;
          } else {
            mi.event = b_down ? inp::MouseInputEvent::kDown : inp::MouseInputEvent::kUp;
            mi.button = button;
            mi.int4_tap_count =
                b_down ? tap_count_update(ui.tap, SDL_GetTicks(), int4_x, int4_y) : 1;
          }
          dispatch_mouse(s, ui, vec_queues, mi);
        }
      }

        vec_quads.clear();
        push_quad(0, 0, kViewW, kViewH, 0.05f, 0.07f, 0.1f, 1);
        push_world_draw(s, ui);
        push_panel_draw(s, ui, vec_queues);
        push_text(lch::session_hud_line(s), 8, 8, 2, 1, 1, 0.4f);

        std::vector<GLfloat> vec_verts;
        vec_verts.reserve(vec_quads.size() * 30);
        for (const Quad& quad : vec_quads) {
          const GLfloat fp4_verts[]{quad.fp4_x, quad.fp4_y, quad.fp4_r, quad.fp4_g, quad.fp4_b,
                                    quad.fp4_a,
                                    quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g,
                                    quad.fp4_b, quad.fp4_a,
                                    quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g,
                                    quad.fp4_b, quad.fp4_a,
                                    quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g,
                                    quad.fp4_b, quad.fp4_a,
                                    quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g,
                                    quad.fp4_b, quad.fp4_a,
                                    quad.fp4_x + quad.fp4_w, quad.fp4_y + quad.fp4_h, quad.fp4_r,
                                    quad.fp4_g, quad.fp4_b, quad.fp4_a};
          vec_verts.insert(vec_verts.end(), fp4_verts, fp4_verts + 36);
        }
        gl_clear_color(0, 0, 0, 1);
        gl_clear(GL_COLOR_BUFFER_BIT);
        gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(vec_verts.size() * sizeof(GLfloat)),
                       vec_verts.data(), GL_STATIC_DRAW);
        gl_draw_arrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_quads.size() * 6));
        SDL_GL_SwapWindow(ptr_window);

        if (ui.b_screenshot) {
          save_screenshot(ptr_window, screenshot_path(
                                         std::format("t{}", s.w.int4_world_tick))
                                         .c_str());
          ui.b_screenshot = false;
        }

        ++u8_frames;
        // autotest 步进：wait<n> 定帧，其余每 3 帧一步
        if (sz_game_next_step < vec_game_steps.size()
            && u8_frames >= u8_game_step_ready_frame) {
          const std::string& str_step{vec_game_steps[sz_game_next_step]};
          if (str_step.starts_with("wait")) {
            u8_game_step_ready_frame = u8_frames + std::stoull(str_step.substr(4));
          } else {
            if (str_step.starts_with("shot")) {
              const std::string str_path{str_game_shot_dir + "/" + str_step.substr(4) + ".bmp"};
              save_screenshot(ptr_window, str_path.c_str());
              std::println("game-autotest: shot {}", str_path);
            } else if (str_step == "abort") {
              ui.b_quit = true;
            }
            u8_game_step_ready_frame = u8_frames + 3;
          }
          ++sz_game_next_step;
        }
        if (sz_game_next_step >= vec_game_steps.size() && !vec_game_steps.empty()
            && u8_frames >= u8_game_step_ready_frame + 30)
          ui.b_quit = true;  // 序列放完自动 abort（防呆）
      }

      const std::uint32_t u4_next{std::min(u4_next_logic, u4_next_render)};
      const std::int32_t int4_sleep{static_cast<std::int32_t>(u4_next - SDL_GetTicks())};
      if (int4_sleep > 0) SDL_Delay(static_cast<std::uint32_t>(int4_sleep));
    }

    std::println("game round done frames={} ticks={}", u8_frames, s.w.int4_world_tick);
    flush_events(s, sz_printed, ptr_log);

    // 轮末清理（上游 Game.DisposeWorld 等价：音乐停、世界释放；音频 engine 跨轮存活）
    if (opt_music.has_value()) ora::game::music_player_stop(*opt_music);
    opt_music.reset();
    s.w.ptr_sfx = nullptr;
    s.vec_bridge_pending.clear();
    s.vec_movers.clear();
    s.vec_fighters.clear();
    s.om = net::OrderManagerModel{};
    s.w = ww::WorldModel{};
    return b_window_closed ? GameRoundExit::kWindowClosed : GameRoundExit::kAborted;
  }};

  // 大堂路径 = --lobby 直进或菜单全流程（ptr_flow 非空）；其余 CLI 直进
  //（--map/--demo/--record/--replay 窗口模式）单轮直入沙盒对局
  const bool b_via_lobby{args.b_lobby || ptr_flow != nullptr};
  if (!b_via_lobby) {
    // CLI 直进单轮（T15 保留：--map/--demo/--record/--replay 窗口模式，退出即进程结束）
    game_round(std::nullopt);
  } else {
    // 大堂↔对局循环（--lobby 直进与菜单全流程共用；T28 上游语义：对局 ABORT 后
    // StartSkirmishGame 重连大堂，DISCONNECT 才离场——直进=退出进程、全流程=回菜单）
    bool b_session_done{false};
    while (!b_session_done) {
      const std::string str_maps_dir{args.str_maps_dir.empty() ? "tests/golden/maps"
                                                               : args.str_maps_dir};
      const std::string str_chrome_dir{args.str_chrome_dir.empty()
                                           ? "tests/golden/lobby/fixture"
                                           : args.str_chrome_dir};
      lch::LobbyRuntime lobby;
      const auto res_lobby{lch::lobby_runtime_create(lobby, str_maps_dir, str_chrome_dir)};
      if (!res_lobby.has_value()) {
        std::println("ora: lobby setup failed: {}", res_lobby.error());
        break;
      }
      std::println("lobby: {} maps from {} (tree {})", lobby.vec_entries.size(), str_maps_dir,
                   lobby.b_tree_loaded ? "loaded" : "fallback");
      if (lob::run_lobby_phase(lobby, ptr_window, u_prog) != 1) {
        std::println("lobby: disconnected");
        if (ptr_flow != nullptr)
          lch::flow_dispatch(*ptr_flow, lch::FlowEvent::kLobbyDisconnect);
        break;
      }
      if (ptr_flow != nullptr) lch::flow_dispatch(*ptr_flow, lch::FlowEvent::kLobbyStart);
      std::optional<lch::LobbyStartConfig> opt_lobby_start{lch::lobby_start_config(lobby)};
      std::println("lobby: start game map={} slots={} seed={}", opt_lobby_start->str_map_path,
                   opt_lobby_start->vec_factions.size(), opt_lobby_start->int4_seed);
      const GameRoundExit round{game_round(std::move(opt_lobby_start))};
      if (round == GameRoundExit::kSetupFailed || round == GameRoundExit::kWindowClosed) break;
      // kAborted：上游 OpenMenuBasedOnLastGame(Skirmish) → StartSkirmishGame 重进大堂
      if (ptr_flow != nullptr) lch::flow_dispatch(*ptr_flow, lch::FlowEvent::kGameAbort);
      std::println("game: aborted -> reconnect lobby (StartSkirmishGame)");
    }
  }

  ora::platforms::engine_dispose(sound.engine);
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(ptr_window);
  SDL_Quit();
  return 0;
}

// —— T26 主菜单窗口（手动验收面）：chrome 装载 + MainMenuLogic 导航 + 设置面板 ——

namespace menu_ns = ora::game::ui;
namespace fr = ora::game::render;

constexpr int kMenuWinW{1024};
constexpr int kMenuWinH{768};

// 语料根探查：环境变量 ORA_GOLDEN_DIR，或自 exe 目录向上找 tests/golden（开发布局）
std::optional<std::filesystem::path> find_golden_root(char* sz_argv0) {
  if (const char* sz_env{std::getenv("ORA_GOLDEN_DIR")};
      sz_env != nullptr && *sz_env != '\0') {
    const std::filesystem::path path_env{sz_env};
    std::error_code ec;
    if (std::filesystem::exists(path_env / "mainmenu" / "fixture" / "mainmenu.yaml", ec))
      return path_env;
  }
  std::filesystem::path path_dir{std::filesystem::path{sz_argv0 != nullptr ? sz_argv0 : ""}
                                     .parent_path()};
  for (int int4_up{0}; int4_up < 6; ++int4_up) {
    std::error_code ec;
    if (std::filesystem::exists(path_dir / "tests" / "golden" / "mainmenu" / "fixture"
                                   / "mainmenu.yaml",
                                ec))
      return path_dir / "tests" / "golden";
    const std::filesystem::path path_parent{path_dir.parent_path()};
    if (path_parent == path_dir) break;
    path_dir = path_parent;
  }
  return std::nullopt;
}

// 设置文件路径：上游 SupportDir 形态的独立分支（不与真 OpenRA 混用）
std::string menu_settings_path() {
  const char* sz_appdata{std::getenv("APPDATA")};
  if (sz_appdata == nullptr) return "openracpp-settings.yaml";
  std::error_code ec;
  const std::filesystem::path path_dir{std::string{sz_appdata} + "/OpenRACpp"};
  std::filesystem::create_directories(path_dir, ec);
  return (path_dir / "settings.yaml").string();
}

// —— T28 全流程共用装载面（菜单/大堂/对局三相位同源）——

// 设置装载：落盘文件优先；缺失/坏文件回落全默认（进程不因设置炸——上游低层
// Load 会炸，探针面折叠为提示+默认，D088 口径顺延）
ora::game::SettingsModel flow_settings_load(const char* sz_where) {
  const std::string str_path{menu_settings_path()};
  auto res{ora::game::settings_load(str_path)};
  if (!res.has_value()) {
    std::println("flow[{}]: settings load {} failed ({}), using defaults", sz_where, str_path,
                 res.error());
    return ora::game::SettingsModel{};
  }
  std::println("flow[{}]: settings={} (windowed={}x{} vsync={} mute={} sound={:.3f})",
               sz_where, str_path, res->graphics.windowed_size.int4_x,
               res->graphics.windowed_size.int4_y, res->graphics.b_vsync ? 1 : 0,
               res->sound.b_mute ? 1 : 0, res->sound.fp_sound_volume);
  return *res;
}

// 热键注册表：黄金 9 文件 + settings Keys: 段应用（改键重进生效链——上游
// HotkeyManager ctor 每次装载先 mod 默认再覆盖 Keys 段）
std::optional<hotk::HotkeyRegistryModel> flow_hotkey_registry_load(
    const std::filesystem::path& path_golden, ora::game::SettingsModel& sm) {
  std::vector<std::vector<ora::game::MiniYamlNode>> vec_hotkey_files;
  std::ifstream fs_files{path_golden / "hotkeys" / "files.txt"};
  if (!fs_files.is_open()) {
    std::println("flow: hotkey corpus files.txt missing");
    return std::nullopt;
  }
  std::string str_line;
  while (std::getline(fs_files, str_line)) {
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    if (str_line.empty()) continue;
    std::replace(str_line.begin(), str_line.end(), '|', '-');
    std::replace(str_line.begin(), str_line.end(), '/', '-');
    auto res_nodes{
        ora::game::miniyaml_from_file((path_golden / "hotkeys" / "yaml" / str_line).string())};
    if (!res_nodes.has_value()) {
      std::println("flow: hotkey corpus {}: {}", str_line, res_nodes.error());
      return std::nullopt;
    }
    vec_hotkey_files.push_back(std::move(*res_nodes));
  }
  auto res_reg{hotk::hotkey_registry_load(vec_hotkey_files)};
  if (!res_reg.has_value()) {
    std::println("flow: hotkey registry load failed: {}", res_reg.error());
    return std::nullopt;
  }
  // settings Keys: 段覆盖（坏值=拒绝装载——D059；流程相位按"装载失败用默认注册表"
  // 探针口径降级，console 提示）
  if (ora::game::MiniYamlNode* ptr_keys{ora::game::settings_find_section(sm, "Keys")}) {
    const auto res_apply{hotk::hotkey_registry_apply_settings(*res_reg, ptr_keys->vec_children)};
    if (!res_apply.has_value())
      std::println("flow: hotkey settings apply failed ({}), mod defaults kept", res_apply.error());
    else
      std::println("flow: hotkeys={} keys-section applied", res_reg->vec_definitions.size());
  } else {
    std::println("flow: hotkeys={} (no Keys section, mod defaults)", res_reg->vec_definitions.size());
  }
  return std::move(*res_reg);
}

// 对局 BGM 曲库：上游 mods/ra/audio/music.yaml 真源语料 + 内容包存在性探测
//（.aud 在 mix 内不可平铺探测——全部 b_exists=false = 上游无内容包等价：播放
// 决策照常发起、引擎层静默；D074 AudLoader 缺位顺延）
std::vector<std::pair<std::string, ora::game::music_info>> flow_music_library(
    const std::filesystem::path& path_golden) {
  const std::filesystem::path path_music{path_golden / "music" / "ra-music.yaml"};
  auto res_tracks{ora::game::load_music_infos(path_music.string())};
  if (!res_tracks.has_value()) {
    std::println("flow: music library {}: {}", path_music.string(), res_tracks.error());
    return {};
  }
  const char* sz_appdata{std::getenv("APPDATA")};
  std::string str_probe_dir;
  if (sz_appdata != nullptr)
    str_probe_dir = std::string{sz_appdata} + "/OpenRA/Content/ra/v2/music";
  ora::game::music_infos_probe_exists(*res_tracks, str_probe_dir);
  std::size_t sz_exists{};
  for (const auto& [str_key, info] : *res_tracks)
    if (info.b_exists) ++sz_exists;
  std::println("flow: music library {} tracks ({} playable) from {}", res_tracks->size(),
               sz_exists, path_music.string());
  return *res_tracks;
}

// SDL key → ora KeyInput（printable 直转（Keycode 枚举=ASCII 值），功能键表覆盖）
inp::Keycode sdl_keycode_to_ora(SDL_Keycode key) {
  if (key >= 0x20 && key <= 0x7E) return static_cast<inp::Keycode>(key);
  switch (key) {
    case SDLK_RETURN:
      return inp::Keycode::kReturn;
    case SDLK_KP_ENTER:
      return inp::Keycode::kKpEnter;
    case SDLK_ESCAPE:
      return inp::Keycode::kEscape;
    case SDLK_BACKSPACE:
      return inp::Keycode::kBackspace;
    case SDLK_TAB:
      return inp::Keycode::kTab;
    case SDLK_UP:
      return inp::Keycode::kUp;
    case SDLK_DOWN:
      return inp::Keycode::kDown;
    case SDLK_LEFT:
      return inp::Keycode::kLeft;
    case SDLK_RIGHT:
      return inp::Keycode::kRight;
    default:
      return inp::Keycode::kUnknown;
  }
}

// 菜单相位出口（T28 全流程）：kQuit=QUIT_BUTTON/窗口关闭/语料错误（进程退出），
// kStartSkirmish=SKIRMISH_BUTTON 出口位（上游 StartSkirmishGame → 流程进大堂）
enum class MenuPhaseExit { kQuit, kStartSkirmish };

MenuPhaseExit run_menu_phase(const lch::CliArgs& args, char* sz_argv0,
                             menu_ns::MenuType initial_type) {
  static_cast<void>(args);
  const auto opt_golden{find_golden_root(sz_argv0)};
  if (!opt_golden.has_value()) {
    std::println("ora: main menu corpus not found（ORA_GOLDEN_DIR 或 tests/golden 布局缺失）");
    return MenuPhaseExit::kQuit;
  }
  const std::filesystem::path path_golden{*opt_golden};
  const std::filesystem::path path_fixture{path_golden / "mainmenu" / "fixture"};

  const auto load_yaml_nodes{[](const std::filesystem::path& path_file) {
    auto res{ora::game::miniyaml_from_file(path_file.string())};
    if (!res.has_value()) std::println("ora: corpus parse {}: {}", path_file.string(), res.error());
    return res;
  }};

  const auto vec_menu{load_yaml_nodes(path_fixture / "mainmenu.yaml")};
  const auto vec_settings{load_yaml_nodes(path_fixture / "settings.yaml")};
  const auto vec_display{load_yaml_nodes(path_fixture / "settings-display.yaml")};
  const auto vec_audio{load_yaml_nodes(path_fixture / "settings-audio.yaml")};
  const auto vec_hotkeys{load_yaml_nodes(path_fixture / "settings-hotkeys.yaml")};
  if (!vec_menu.has_value() || !vec_settings.has_value() || !vec_display.has_value()
      || !vec_audio.has_value() || !vec_hotkeys.has_value())
    return MenuPhaseExit::kQuit;
  const auto loader_menu{menu_ns::widget_loader_from_nodes(*vec_menu)};
  const auto loader_settings{menu_ns::widget_loader_from_nodes(*vec_settings)};
  const auto loader_display{menu_ns::widget_loader_from_nodes(*vec_display)};
  const auto loader_audio{menu_ns::widget_loader_from_nodes(*vec_audio)};
  const auto loader_hotkeys{menu_ns::widget_loader_from_nodes(*vec_hotkeys)};
  if (!loader_menu.has_value() || !loader_settings.has_value() || !loader_display.has_value()
      || !loader_audio.has_value() || !loader_hotkeys.has_value()) {
    std::println("ora: menu chrome loader build failed");
    return MenuPhaseExit::kQuit;
  }

  // chrome 集合/指标（按钮九宫格与配色）
  const auto vec_chrome{
      load_yaml_nodes(path_golden / "chrome" / "fixture" / "ra-chrome.yaml")};
  const auto vec_metrics_common{
      load_yaml_nodes(path_golden / "chrome" / "fixture" / "common-metrics.yaml")};
  const auto vec_metrics_ra{
      load_yaml_nodes(path_golden / "chrome" / "fixture" / "ra-metrics.yaml")};
  std::map<std::string, menu_ns::ChromeCollection> map_chrome;
  std::map<std::string, std::string> map_metrics;
  if (vec_chrome.has_value()) {
    if (const auto res{menu_ns::chrome_provider_initialize({*vec_chrome})}; res.has_value())
      map_chrome = std::move(*res);
  }
  if (vec_metrics_common.has_value() && vec_metrics_ra.has_value()) {
    if (const auto res{
            menu_ns::chrome_metrics_initialize({*vec_metrics_common, *vec_metrics_ra})};
        res.has_value())
      map_metrics = std::move(*res);
  }

  // 字体（T23 stb 光栅化：FreeSans 14px 单 sheet）
  std::ifstream fs_ttf{path_golden / "fonts" / "fixture" / "FreeSans.ttf", std::ios::binary};
  if (!fs_ttf.is_open()) {
    std::println("ora: menu font corpus missing");
    return MenuPhaseExit::kQuit;
  }
  const std::string str_ttf{std::istreambuf_iterator<char>{fs_ttf},
                            std::istreambuf_iterator<char>{}};
  auto res_face{fr::font_face_load(
      {reinterpret_cast<const std::uint8_t*>(str_ttf.data()), str_ttf.size()})};
  if (!res_face.has_value()) {
    std::println("ora: font face: {}", res_face.error());
    return MenuPhaseExit::kQuit;
  }
  const std::shared_ptr<const fr::FontFace> ptr_face{
      std::make_shared<fr::FontFace>(std::move(*res_face))};
  fr::SheetBuilder font_builder{};
  font_builder.int4_sheet_size = 512;
  font_builder.int4_margin = 1;
  auto res_font{fr::spritefont_create(ptr_face, font_builder, 14, 11, 1.0f)};
  if (!res_font.has_value()) {
    std::println("ora: spritefont: {}", res_font.error());
    return MenuPhaseExit::kQuit;
  }
  std::map<std::string, fr::SpriteFont> map_fonts;
  // chrome 字体名折叠到同一 face 同一 sheet 0（观感票再分 Regular/Bold 度量；
  // 单 sheet 使字体 id 0 与 chrome 占位 id ≥1 恒不冲突）
  if (res_font.has_value())
    for (const std::string_view str_name :
         {"Regular", "Bold", "Tiny", "TinyBold", "Title", "FreeSans"})
      map_fonts.emplace(std::string{str_name}, *res_font);

  // 占位 ChromeSheetSet：不装载 chrome PNG 像素（HUD 渲染票 D075 回收项），
  // 只为每集合分配独立 sheet id（≥1），使 chrome quad 与字体 sheet 0 分批
  menu_ns::ChromeSheetSet chrome_sheets;
  for (const auto& [str_name, col] : map_chrome)
    chrome_sheets.map_sheets.emplace(
        col.str_image, menu_ns::ChromeSheet{chrome_sheets.u4_next_id++, 1, 1, {0xFFFFFFFFu}});

  // 设置模型（落盘文件优先，重进生效验收路径）+ 音频门面 + 分辨率生效面
  const std::string str_settings_path{menu_settings_path()};
  ora::game::SettingsModel sm{flow_settings_load("menu")};
  int int4_win_w{kMenuWinW};
  int int4_win_h{kMenuWinH};
  if (sm.graphics.windowed_size.int4_x >= 640 && sm.graphics.windowed_size.int4_y >= 480) {
    int4_win_w = sm.graphics.windowed_size.int4_x;
    int4_win_h = sm.graphics.windowed_size.int4_y;
  } else {
    std::println("menu: windowed_size {}x{} out of range, using {}x{}",
                 sm.graphics.windowed_size.int4_x, sm.graphics.windowed_size.int4_y, int4_win_w,
                 int4_win_h);
  }
  // 热键注册表（黄金 9 文件 + settings Keys: 段——改键重进生效链）
  const auto opt_hotkeys{flow_hotkey_registry_load(path_golden, sm)};
  hotk::HotkeyRegistryModel hotkeys{opt_hotkeys.has_value() ? std::move(*opt_hotkeys)
                                                            : hotk::HotkeyRegistryModel{}};
  ora::game::sound_facade sound{
      ora::game::make_sound_facade_settings(sm.sound.str_device, sm.sound.b_mute, sm.sound)};
  std::println("menu: window {}x{} sound={} engine", int4_win_w, int4_win_h,
               ora::platforms::engine_dummy(sound.engine) ? "dummy" : "openal");

  // UI 树 + 菜单状态
  menu_ns::WidgetInitCtx ctx_window{int4_win_w, int4_win_h, nullptr, "default"};
  menu_ns::WidgetModel root;
  root.str_type = "Container";
  root.str_id = "ROOT";
  root.bounds = menu_ns::WidgetBounds{0, 0, int4_win_w, int4_win_h};
  if (const auto res_widget{
          menu_ns::widget_loader_load_widget(*loader_menu, root, ctx_window, "MAINMENU")};
      !res_widget.has_value()) {
    std::println("ora: MAINMENU load: {}", res_widget.error());
    return MenuPhaseExit::kQuit;
  }

  menu_ns::MainMenuEnv env{};
  env.b_has_maps = true;
  env.b_has_campaign = true;
  env.b_has_missions = true;
  env.b_load_panel_enabled = false;
  env.b_has_content_installer = false;
  menu_ns::MainMenuState menu;
  menu_ns::main_menu_setup(menu);
  // T28 相位重入：大堂 DISCONNECT 后回菜单=SwitchMenu(Singleplayer)（上游
  // OpenSkirmishLobbyPanel onExit）；首轮 initial=Main（setup 默认）
  if (initial_type != menu_ns::MenuType::kMain && initial_type != menu_ns::MenuType::kStartupPrompts)
    menu_ns::main_menu_switch(menu, initial_type);
  menu_ns::main_menu_apply(menu, env, root);
  menu_ns::main_menu_apply_disabled(env, root);

  menu_ns::SettingsPanelState panel;
  bool b_panel_open{false};

  // —— 窗口/GL（纹理管线：sheet0=1x1 白供 chrome quad，字体 sheet 1..n）——
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::println("ora: SDL_Init failed: {}", SDL_GetError());
    return MenuPhaseExit::kQuit;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_Window* ptr_window{SDL_CreateWindow("ora main menu", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, int4_win_w, int4_win_h,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI)};
  if (ptr_window == nullptr) {
    std::println("ora: window creation failed: {}", SDL_GetError());
    SDL_Quit();
    return MenuPhaseExit::kQuit;
  }
  SDL_GLContext ctx_gl{SDL_GL_CreateContext(ptr_window)};
  if (ctx_gl == nullptr) {
    std::println("ora: GL context failed: {}", SDL_GetError());
    SDL_DestroyWindow(ptr_window);
    SDL_Quit();
    return MenuPhaseExit::kQuit;
  }
  // vsync 设置生效面（上游 Graphics.VSync → Renderer SwapInterval）
  SDL_GL_SetSwapInterval(sm.graphics.b_vsync ? 1 : 0);

  gl_clear_color = gl_load<decltype(gl_clear_color)>("glClearColor");
  gl_clear = gl_load<decltype(gl_clear)>("glClear");
  gl_get_error = gl_load<decltype(gl_get_error)>("glGetError");
  gl_create_shader = gl_load<decltype(gl_create_shader)>("glCreateShader");
  gl_shader_source = gl_load<decltype(gl_shader_source)>("glShaderSource");
  gl_compile_shader = gl_load<decltype(gl_compile_shader)>("glCompileShader");
  gl_get_shader_iv = gl_load<decltype(gl_get_shader_iv)>("glGetShaderiv");
  gl_create_program = gl_load<decltype(gl_create_program)>("glCreateProgram");
  gl_attach_shader = gl_load<decltype(gl_attach_shader)>("glAttachShader");
  gl_link_program = gl_load<decltype(gl_link_program)>("glLinkProgram");
  gl_use_program = gl_load<decltype(gl_use_program)>("glUseProgram");
  gl_gen_buffers = gl_load<decltype(gl_gen_buffers)>("glGenBuffers");
  gl_bind_buffer = gl_load<decltype(gl_bind_buffer)>("glBindBuffer");
  gl_buffer_data = gl_load<decltype(gl_buffer_data)>("glBufferData");
  gl_get_attrib_location = gl_load<decltype(gl_get_attrib_location)>("glGetAttribLocation");
  gl_enable_vertex_attrib_array =
      gl_load<decltype(gl_enable_vertex_attrib_array)>("glEnableVertexAttribArray");
  gl_vertex_attrib_pointer =
      gl_load<decltype(gl_vertex_attrib_pointer)>("glVertexAttribPointer");
  gl_draw_arrays = gl_load<decltype(gl_draw_arrays)>("glDrawArrays");
  gl_gen_vertex_arrays = gl_load<decltype(gl_gen_vertex_arrays)>("glGenVertexArrays");
  gl_bind_vertex_array = gl_load<decltype(gl_bind_vertex_array)>("glBindVertexArray");
  gl_read_pixels = gl_load<decltype(gl_read_pixels)>("glReadPixels");
  gl_gen_textures = gl_load<decltype(gl_gen_textures)>("glGenTextures");
  gl_bind_texture = gl_load<decltype(gl_bind_texture)>("glBindTexture");
  gl_tex_image_2d = gl_load<decltype(gl_tex_image_2d)>("glTexImage2D");
  gl_tex_parameteri = gl_load<decltype(gl_tex_parameteri)>("glTexParameteri");
  gl_get_uniform_location =
      gl_load<decltype(gl_get_uniform_location)>("glGetUniformLocation");
  gl_uniform4f = gl_load<decltype(gl_uniform4f)>("glUniform4f");

  const auto make_shader = [&](GLenum type, const char* sz_src) {
    const unsigned u_shader{gl_create_shader(type)};
    gl_shader_source(u_shader, 1, &sz_src, nullptr);
    gl_compile_shader(u_shader);
    int int4_ok{0};
    gl_get_shader_iv(u_shader, GL_COMPILE_STATUS, &int4_ok);
    if (!int4_ok) {
      std::println("ora: shader compile failed");
      std::exit(1);
    }
    return u_shader;
  };
  const unsigned u_prog{static_cast<unsigned>(gl_create_program())};
  gl_attach_shader(u_prog, make_shader(GL_VERTEX_SHADER,
                                       "#version 150\n"
                                       "in vec2 aPos;\n"
                                       "in vec2 aUv;\n"
                                       "in vec4 aCol;\n"
                                       "out vec2 vUv;\n"
                                       "out vec4 vCol;\n"
                                       "uniform vec4 uHalf;\n"
                                       "void main() {\n"
                                       "  vUv = aUv;\n"
                                       "  vCol = aCol;\n"
                                       "  gl_Position = vec4(aPos.x / uHalf.x - 1.0, 1.0 - aPos.y / uHalf.y, 0.0, 1.0);\n"
                                       "}\n"));
  gl_attach_shader(u_prog, make_shader(GL_FRAGMENT_SHADER,
                                       "#version 150\n"
                                       "in vec2 vUv;\n"
                                       "in vec4 vCol;\n"
                                       "out vec4 frag;\n"
                                       "uniform sampler2D uTex;\n"
                                       "void main() {\n"
                                       "  vec4 c = texture(uTex, vUv);\n"
                                       "  frag = vec4(c.rgb * vCol.rgb, c.a * vCol.a);\n"
                                       "}\n"));
  gl_link_program(u_prog);
  gl_use_program(u_prog);
  // 视口半宽高（T28 分辨率生效面：任意 windowed_size 下 NDC 归一化按实际尺寸）
  const int idx_uhalf{gl_get_uniform_location(u_prog, "uHalf")};
  gl_uniform4f(idx_uhalf, static_cast<GLfloat>(int4_win_w) / 2.0f,
               static_cast<GLfloat>(int4_win_h) / 2.0f, 0.0f, 0.0f);

  unsigned u_vao{0};
  unsigned u_vbo{0};
  gl_gen_vertex_arrays(1, &u_vao);
  gl_bind_vertex_array(u_vao);
  gl_gen_buffers(1, &u_vbo);
  gl_bind_buffer(GL_ARRAY_BUFFER, u_vbo);
  const int idx_pos{gl_get_attrib_location(u_prog, "aPos")};
  const int idx_uv{gl_get_attrib_location(u_prog, "aUv")};
  const int idx_col{gl_get_attrib_location(u_prog, "aCol")};
  gl_enable_vertex_attrib_array(idx_pos);
  gl_vertex_attrib_pointer(idx_pos, 2, GL_FLOAT, GL_FALSE, 32, nullptr);
  gl_enable_vertex_attrib_array(idx_uv);
  gl_vertex_attrib_pointer(idx_uv, 2, GL_FLOAT, GL_FALSE, 32,
                           reinterpret_cast<const void*>(8));
  gl_enable_vertex_attrib_array(idx_col);
  gl_vertex_attrib_pointer(idx_col, 4, GL_FLOAT, GL_FALSE, 32,
                           reinterpret_cast<const void*>(16));
  // sampler uTex 默认纹理单元 0；quad 按 sheet 分组提交（单 draw call 单纹理）

  // 纹理表：id 0 = 1x1 白（chrome 像素不装载——HUD 渲染票 D075 回收项，sheet=0 的
  // quad 以 tint 色块呈现）；id 1..n = 字体 sheet（A8 → 白 RGBA）
  std::map<std::uint32_t, unsigned> map_textures;
  {
    unsigned tex_white{};
    gl_gen_textures(1, &tex_white);
    gl_bind_texture(GL_TEXTURE_2D, tex_white);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const std::uint8_t u1_white[]{255, 255, 255, 255};
    gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, u1_white);
    map_textures.emplace(0, tex_white);
  }
  const auto upload_font_sheet{[&](std::uint32_t u4_id, const std::vector<std::uint8_t>& vec1u_a8,
                                   int int4_size) {
    std::vector<std::uint8_t> vec1u_rgba(vec1u_a8.size() * 4);
    for (std::size_t sz_i{0}; sz_i < vec1u_a8.size(); ++sz_i) {
      vec1u_rgba[sz_i * 4] = 255;
      vec1u_rgba[sz_i * 4 + 1] = 255;
      vec1u_rgba[sz_i * 4 + 2] = 255;
      vec1u_rgba[sz_i * 4 + 3] = vec1u_a8[sz_i];
    }
    unsigned tex{};
    gl_gen_textures(1, &tex);
    gl_bind_texture(GL_TEXTURE_2D, tex);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8, int4_size, int4_size, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, vec1u_rgba.data());
    map_textures.emplace(u4_id, tex);
  }};
  for (std::size_t sz_sheet{0}; sz_sheet < font_builder.vec_vec1u_sheets.size(); ++sz_sheet)
    upload_font_sheet(static_cast<std::uint32_t>(sz_sheet + 1),
                      font_builder.vec_vec1u_sheets[sz_sheet], font_builder.int4_sheet_size);

  menu_ns::UiState uist;
  bool b_running{true};
  std::uint64_t u8_frames{0};

  // 输入馈送（事件泵与 ORA_MENU_AUTOTEST 驱动共用）
  const auto feed_mouse{[&](inp::MouseInput mi) {
    const std::size_t sz_base{uist.vec_events.size()};
    static_cast<void>(menu_ns::ui_handle_input(root, uist, mi));
    const std::span<const menu_ns::UiEventRecord> vec_new{
        uist.vec_events.data() + sz_base, uist.vec_events.size() - sz_base};

    if (b_panel_open) {
      bool b_closed{false};
      for (const menu_ns::UiEventRecord& rec : vec_new) {
        if (rec.str_kind != "click") continue;
        // C++ 折叠交互面：BACK=关面板保存（上游同）、RESET=直执行（上游确认
        // 对话框直通——偏离登记）、MODE 下拉折叠为三态循环、分辨率文本/
        // 改键输入无键盘捕获面（跳过——偏离登记）
        if (rec.str_id == "BACK_BUTTON") {
          menu_ns::settings_panel_close(panel, &menu);
          // 上游 Ui.CloseWindow：窗口子树移除
          for (auto it{root.vec_children.begin()}; it != root.vec_children.end(); ++it)
            if ((*it)->str_id == "SETTINGS_PANEL") {
              root.vec_children.erase(it);
              break;
            }
          b_panel_open = false;
          b_closed = true;
          std::println("menu: settings saved -> {}", str_settings_path);
          break;
        }
        if (rec.str_id == "RESET_BUTTON")
          menu_ns::settings_panel_reset(panel, panel.active);
        if (rec.str_id == "MODE_DROPDOWN") {
          const auto next{sm.graphics.mode == ora::game::WindowMode::kPseudoFullscreen
                              ? ora::game::WindowMode::kWindowed
                          : sm.graphics.mode == ora::game::WindowMode::kWindowed
                              ? ora::game::WindowMode::kFullscreen
                              : ora::game::WindowMode::kPseudoFullscreen};
          menu_ns::settings_panel_set_window_mode(panel, next);
        }
      }
      if (!b_closed) menu_ns::settings_panel_dispatch(panel, vec_new);
    } else {
      menu_ns::main_menu_dispatch(menu, env, root, vec_new);
      if (menu.b_quit) {
        b_running = false;
        std::println("menu: quit requested");
      }
      // T28 出口位：SKIRMISH_BUTTON → StartSkirmishGame（流程进大堂）
      if (menu.b_start_skirmish) {
        b_running = false;
        std::println("menu: start skirmish requested");
      }
      if (menu.b_settings_open && !b_panel_open) {
        panel = menu_ns::SettingsPanelState{};
        panel.ptr_settings = &sm;
        panel.ptr_sound = &sound;
        panel.ptr_hotkeys = &hotkeys;
        const auto res_open{menu_ns::settings_panel_open(
            panel, root, *loader_settings, *loader_display, *loader_audio,
            *loader_hotkeys, ctx_window, str_settings_path)};
        if (res_open.has_value()) {
          b_panel_open = true;
          std::println("menu: settings panel open (active={})",
                       menu_ns::settings_panel_id_name(panel.active));
        } else {
          std::println("menu: settings panel open failed: {}", res_open.error());
          menu_ns::main_menu_close_settings(menu);
        }
      }
    }
    menu_ns::main_menu_apply(menu, env, root);
  }};
  // 链可见判定：隐藏菜单内的按钮自身 b_visible 仍为 true（可见性路由在容器位），
  // 驱动必须命中当前真正可达的实例（面板 BACK vs 三个菜单同名 BACK）
  const auto chain_visible{[](const menu_ns::WidgetModel* ptr_w) {
    for (const menu_ns::WidgetModel* ptr_p{ptr_w}; ptr_p != nullptr; ptr_p = ptr_p->ptr_parent)
      if (!ptr_p->b_visible) return false;
    return true;
  }};
  const auto find_reachable{[&](std::string_view str_id) -> menu_ns::WidgetModel* {
    std::function<menu_ns::WidgetModel*(menu_ns::WidgetModel&)> visit{
        [&](menu_ns::WidgetModel& w) -> menu_ns::WidgetModel* {
          if (w.str_id == str_id && chain_visible(&w)) return &w;
          for (const auto& ptr_child : w.vec_children)
            if (menu_ns::WidgetModel* ptr_found{visit(*ptr_child)}) return ptr_found;
          return nullptr;
        }};
    return visit(root);
  }};
  const auto click_widget{[&](std::string_view str_id) {
    menu_ns::WidgetModel* ptr_w{find_reachable(str_id)};
    if (ptr_w == nullptr) {
      std::println("menu-autotest: {} not reachable", str_id);
      return;
    }
    const menu_ns::WidgetBounds b{menu_ns::widget_event_bounds(*ptr_w)};
    inp::MouseInput mi{};
    mi.button = inp::MouseButton::kLeft;
    mi.int4_tap_count = 1;
    mi.int4_x = b.int4_x + b.int4_width / 2;
    mi.int4_y = b.int4_y + b.int4_height / 2;
    mi.event = inp::MouseInputEvent::kDown;
    feed_mouse(mi);
    mi.event = inp::MouseInputEvent::kUp;
    feed_mouse(mi);
  }};

  // ORA_MENU_AUTOTEST 驱动面（验收探针，T28 分号轮次表）：每轮逗号分隔步骤
  // wait<n>/shot<name>/click<id>/drag<id>:<frac>/print/quit；截图落 ORA_MENU_SHOT_DIR
  static int int4_menu_round{0};
  const std::vector<std::string> vec_autotest{
      autotest_round_steps(std::getenv("ORA_MENU_AUTOTEST"), int4_menu_round)};
  std::string str_shot_dir{"."};
  if (const char* sz_shot{std::getenv("ORA_MENU_SHOT_DIR")}; sz_shot != nullptr && *sz_shot != '\0')
    str_shot_dir = sz_shot;
  const auto autotest_step{[&](const std::string& str_step) {
    if (str_step.starts_with("click")) {
      click_widget(std::string_view{str_step}.substr(5));
    } else if (str_step.starts_with("shot")) {
      const std::string str_path{str_shot_dir + "/" + str_step.substr(4) + ".bmp"};
      save_screenshot(ptr_window, str_path.c_str());
      std::println("menu-autotest: shot {}", str_path);
    } else if (str_step.starts_with("drag")) {
      const std::size_t sz_colon{str_step.find(':')};
      if (sz_colon == std::string::npos) return;
      menu_ns::WidgetModel* ptr_w{find_reachable(str_step.substr(4, sz_colon - 4))};
      const float fp4_frac{std::stof(str_step.substr(sz_colon + 1))};
      if (ptr_w == nullptr) return;
      const menu_ns::WidgetBounds b{menu_ns::widget_event_bounds(*ptr_w)};
      inp::MouseInput mi{};
      mi.button = inp::MouseButton::kLeft;
      mi.event = inp::MouseInputEvent::kDown;
      mi.int4_tap_count = 1;
      mi.int4_x = b.int4_x + 1;
      mi.int4_y = b.int4_y + b.int4_height / 2;
      feed_mouse(mi);
      mi.event = inp::MouseInputEvent::kMove;
      mi.int4_x = b.int4_x + static_cast<int>(static_cast<float>(b.int4_width) * fp4_frac);
      feed_mouse(mi);
      mi.event = inp::MouseInputEvent::kUp;
      feed_mouse(mi);
    } else if (str_step == "print") {
      std::println("menu-state: active={} vsync={} mode={} mute={} sound={:.3f} music={:.3f}"
                   " video={:.3f}",
                   b_panel_open ? menu_ns::settings_panel_id_name(panel.active) : "menu",
                   sm.graphics.b_vsync ? 1 : 0, static_cast<int>(sm.graphics.mode),
                   sm.sound.b_mute ? 1 : 0, sm.sound.fp_sound_volume, sm.sound.fp_music_volume,
                   sm.sound.fp_video_volume);
    } else if (str_step == "quit") {
      b_running = false;
    }
  }};
  std::size_t sz_next_step{0};
  std::uint64_t u8_step_ready_frame{5};

  std::println("menu: click SETTINGS 调整设置，BACK 保存返回，QUIT 退出（重进验证持久化）");

  while (b_running) {
    if (g_ctrl_c) b_running = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) b_running = false;
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE
          && !b_panel_open)
        b_running = false;
      if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        const bool b_down{event.type == SDL_MOUSEBUTTONDOWN};
        inp::MouseInput mi;
        mi.int4_x = event.button.x;
        mi.int4_y = event.button.y;
        mi.button = event.button.button == SDL_BUTTON_LEFT  ? inp::MouseButton::kLeft
                    : event.button.button == SDL_BUTTON_RIGHT ? inp::MouseButton::kRight
                                                              : inp::MouseButton::kNone;
        mi.event = b_down ? inp::MouseInputEvent::kDown : inp::MouseInputEvent::kUp;
        feed_mouse(mi);
      }
    }

    // 绘制（sheet 分批：字体 sheet 0 → 字形纹理白字；chrome sheet ≥1 → 白纹理
    // 灰 tint 色块呈现面板/按钮结构——chrome 像素不装载，D075 回收项）
    menu_ns::UiRenderCtx rctx{&map_fonts, "Regular", 0xFFFFFFFFu, &map_chrome, &chrome_sheets,
                              &map_metrics, &uist};
    const std::vector<menu_ns::UiQuad> vec_quads{menu_ns::ui_draw_tree(root, rctx)};
    if (u8_frames == 5) {
      std::map<std::uint32_t, std::size_t> map_sheet_count;
      for (const menu_ns::UiQuad& quad : vec_quads) ++map_sheet_count[quad.u4_sheet];
      std::string str_sheets;
      for (const auto& [u4_id, sz_n] : map_sheet_count)
        str_sheets += std::format("{}:{} ", u4_id, sz_n);
      std::println("menu-dbg: quads={} sheets[{}]", vec_quads.size(), str_sheets);
    }
    gl_clear_color(0.02f, 0.02f, 0.04f, 1);
    gl_clear(GL_COLOR_BUFFER_BIT);
    for (const auto& [u4_key, u4_tex] : map_textures) {
      // 键 0 = 白纹理（chrome quad 灰 tint 批）；键 1..n = 字体 sheet n-1（字形批）
      std::vector<GLfloat> vec_verts;
      for (const menu_ns::UiQuad& quad : vec_quads) {
        const bool b_font{static_cast<std::size_t>(quad.u4_sheet)
                          < font_builder.vec_vec1u_sheets.size()};
        const std::uint32_t u4_want{
            b_font ? static_cast<std::uint32_t>(quad.u4_sheet + 1) : 0u};
        if (u4_want != u4_key) continue;
        const bool b_glyph_tex{b_font};
        const float fp4_sheet{static_cast<float>(font_builder.int4_sheet_size)};
        const float fp4_u0{b_glyph_tex ? static_cast<float>(quad.int4_src_x) / fp4_sheet : 0.0f};
        const float fp4_v0{b_glyph_tex ? static_cast<float>(quad.int4_src_y) / fp4_sheet : 0.0f};
        const float fp4_u1{
            b_glyph_tex ? static_cast<float>(quad.int4_src_x + quad.int4_width) / fp4_sheet
                        : 1.0f};
        const float fp4_v1{
            b_glyph_tex ? static_cast<float>(quad.int4_src_y + quad.int4_height) / fp4_sheet
                        : 1.0f};
        const float fp4_r{b_font ? static_cast<float>((quad.u4_rgba >> 16) & 0xFF) / 255.0f
                                 : 0.40f};
        const float fp4_g{b_font ? static_cast<float>((quad.u4_rgba >> 8) & 0xFF) / 255.0f
                                 : 0.40f};
        const float fp4_b{b_font ? static_cast<float>(quad.u4_rgba & 0xFF) / 255.0f : 0.45f};
        const float fp4_a{static_cast<float>((quad.u4_rgba >> 24) & 0xFF) / 255.0f};
        const float fp4_x0{static_cast<float>(quad.int4_x)};
        const float fp4_y0{static_cast<float>(quad.int4_y)};
        const float fp4_x1{static_cast<float>(quad.int4_x + quad.int4_width)};
        const float fp4_y1{static_cast<float>(quad.int4_y + quad.int4_height)};
        const GLfloat fp4_verts[]{
            fp4_x0, fp4_y0, fp4_u0, fp4_v0, fp4_r, fp4_g, fp4_b, fp4_a,
            fp4_x1, fp4_y0, fp4_u1, fp4_v0, fp4_r, fp4_g, fp4_b, fp4_a,
            fp4_x0, fp4_y1, fp4_u0, fp4_v1, fp4_r, fp4_g, fp4_b, fp4_a,
            fp4_x0, fp4_y1, fp4_u0, fp4_v1, fp4_r, fp4_g, fp4_b, fp4_a,
            fp4_x1, fp4_y0, fp4_u1, fp4_v0, fp4_r, fp4_g, fp4_b, fp4_a,
            fp4_x1, fp4_y1, fp4_u1, fp4_v1, fp4_r, fp4_g, fp4_b, fp4_a};
        vec_verts.insert(vec_verts.end(), fp4_verts, fp4_verts + 48);
      }
      if (vec_verts.empty()) continue;
      gl_bind_texture(GL_TEXTURE_2D, u4_tex);
      gl_bind_buffer(GL_ARRAY_BUFFER, u_vbo);
      gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(vec_verts.size() * sizeof(GLfloat)),
                     vec_verts.data(), GL_STATIC_DRAW);
      gl_draw_arrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_verts.size() / 8));
    }
    SDL_GL_SwapWindow(ptr_window);
    ++u8_frames;
    // autotest 步进：wait<n> 定帧，其余步骤每 3 帧一步（画面刷新间隙）
    if (sz_next_step < vec_autotest.size()
        && u8_frames >= u8_step_ready_frame) {
      const std::string& str_step{vec_autotest[sz_next_step]};
      if (str_step.starts_with("wait")) {
        u8_step_ready_frame = u8_frames + std::stoull(str_step.substr(4));
      } else {
        autotest_step(str_step);
        u8_step_ready_frame = u8_frames + 3;
      }
      ++sz_next_step;
    }
    if (sz_next_step >= vec_autotest.size() && !vec_autotest.empty()
        && u8_frames >= u8_step_ready_frame + 30)
      b_running = false;  // 序列放完自动退出（防呆：quit 步缺失时也收尾）
    SDL_Delay(10);
  }

  std::println("menu: exiting clean frames={}", u8_frames);
  ora::platforms::engine_dispose(sound.engine);
  SDL_GL_DeleteContext(ctx_gl);
  SDL_DestroyWindow(ptr_window);
  SDL_Quit();
  return menu.b_start_skirmish ? MenuPhaseExit::kStartSkirmish : MenuPhaseExit::kQuit;
}

std::optional<std::string> detect_content_dir(const lch::CliArgs& args) {
  if (!args.str_content_dir.empty()) return args.str_content_dir;
  const char* sz_appdata{std::getenv("APPDATA")};
  if (sz_appdata == nullptr) return std::nullopt;
  std::error_code ec;
  const std::filesystem::path path_content{std::string{sz_appdata} +
                                           "/OpenRA/Content/ra/v2"};
  if (std::filesystem::exists(path_content, ec)) return path_content.string();
  return std::nullopt;
}

// T28 全流程编排：FlowState 驱动菜单↔大堂↔对局相位循环（上游 OpenSkirmishLobby
// /IngameMenu/ConnectionLogic 回调网的串联折叠；模式互斥与切换序列由 flow_state
// 状态机保证，无头断言见 ora_full_flow）
int run_flow(const lch::CliArgs& args, std::ofstream* ptr_log, char* sz_argv0) {
  lch::FlowState flow;
  lch::flow_setup(flow);
  for (;;) {
    if (flow.mode == lch::FlowMode::kExit) return 0;
    if (flow.mode == lch::FlowMode::kMainMenu) {
      const MenuPhaseExit res{run_menu_phase(args, sz_argv0, flow.menu_type)};
      if (res == MenuPhaseExit::kQuit) {
        lch::flow_dispatch(flow, lch::FlowEvent::kMenuQuit);
        std::println("flow: exit");
        return 0;
      }
      // SKIRMISH 出口（菜单内 Singleplayer 语境点击；首轮从 Main 补 switch，
      // disconnect 重入时菜单态已是 Singleplayer 直入）
      if (flow.menu_type == menu_ns::MenuType::kMain)
        lch::flow_dispatch(flow, lch::FlowEvent::kMenuSingleplayer);
      lch::flow_dispatch(flow, lch::FlowEvent::kSkirmish);
      std::println("flow: menu -> skirmish lobby");
      continue;
    }
    // kLobby：大堂+对局会话窗口（DISCONNECT 上报后回菜单相位继续循环）
    run_window(args, ptr_log, sz_argv0, &flow);
    if (flow.mode == lch::FlowMode::kMainMenu) {
      std::println("flow: lobby disconnected -> back to menu (Singleplayer)");
      continue;
    }
    // 会话硬终止（窗口关闭/Ctrl+C/装载失败，未走 disconnect 流转）→ 进程退出
    std::println("flow: session terminated (mode={})", static_cast<int>(flow.mode));
    return 0;
  }
}

}

int main(int argc, char** argv) {
  SDL_SetMainReady();
  std::vector<std::string_view> vec_args;
  for (int idx{1}; idx < argc; ++idx) vec_args.push_back(argv[idx]);
  const lch::CliParseResult parsed{lch::cli_args_parse(vec_args)};

  if (parsed.b_help) {
    std::print("{}", lch::cli_help_text());
    return 0;
  }
  if (parsed.b_version) {
    std::println("ora (OpenRACpp M1) sandbox launcher");
    return 0;
  }
  if (!parsed.b_ok) {
    for (const std::string& str_err : parsed.vec_errors)
      std::println("ora: {}", str_err);
    std::print("{}", lch::cli_help_text());
    return 2;
  }

  std::ofstream stream_log;
  std::ofstream* ptr_log{nullptr};
  if (!parsed.args.str_log_file.empty()) {
    stream_log.open(parsed.args.str_log_file, std::ios::trunc);
    if (stream_log) ptr_log = &stream_log;
  }

  if (!parsed.args.str_map.empty() && !std::filesystem::exists(parsed.args.str_map)) {
    std::println("ora: map not found: {}", parsed.args.str_map);
    return 2;
  }

  const auto opt_content{detect_content_dir(parsed.args)};
  if (opt_content.has_value())
    std::println("content: {} (EA assets available for future real-map rendering)",
                 *opt_content);
  else
    std::println(
        "content: EA ra/v2 package not found (install via OpenRA.Utility.exe ra "
        "--install-content); running in color-block fallback mode");

  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

  // T26：窗口模式无参（无 --map/--demo/--record/--replay/--lobby）进主菜单；
  // T28：无参 = 全流程编排（菜单→遭遇战大堂→对局→退出，FlowState 驱动）；
  // 沙盒演示/装载图/大堂直进路径不变（--map、--demo、--lobby、录制回放族）
  const bool b_menu_mode{!parsed.args.b_headless && parsed.args.str_map.empty()
                         && !parsed.args.b_demo && parsed.args.str_record.empty()
                         && parsed.args.str_replay.empty() && !parsed.args.b_lobby};
  const int int4_result{parsed.args.b_headless ? run_headless(parsed.args, ptr_log, argv[0])
                          : b_menu_mode        ? run_flow(parsed.args, ptr_log, argv[0])
                                               : run_window(parsed.args, ptr_log, argv[0],
                                                            nullptr)};
  if (ptr_log != nullptr) stream_log.close();
  return int4_result;
}
