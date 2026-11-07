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

#include "game/input/input_events.hpp"
#include "game/input/order_generator.hpp"
#include "game/input/production_panel.hpp"
#include "game/input/screen_map.hpp"
#include "game/input/selection.hpp"
#include "game/input/selection_logic.hpp"
#include "game/network/order_packet.hpp"
#include "game/replay/replay_format.hpp"
#include "game/replay/replay_player.hpp"
#include "game/replay/replay_record.hpp"
#include "launcher/cli_args.hpp"
#include "launcher/game_session.hpp"

namespace replay_ns = ora::game::replay;

namespace {

namespace inp = ora::game::input;
namespace lch = ora::launcher;
namespace net = ora::game::network;
namespace ww = ora::game::world;

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

struct SoundEngineStub {
  bool b_dummy{true};
};

void sound_tick(SoundEngineStub&) {}

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

int run_window(const lch::CliArgs& args, std::ofstream* ptr_log, char* sz_argv0) {
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

  auto res_session{lch::session_setup(make_session_env(args, sz_argv0))};
  if (!res_session.has_value()) {
    std::println("ora: setup failed: {}", res_session.error());
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(ptr_window);
    SDL_Quit();
    return 1;
  }
  lch::GameSession& s{*res_session};
  SoundEngineStub sound;
  std::println("sound: dummy engine stub (muted, M1)");
  lch::session_tick(s);

  UiState ui;
  ui.panel.str_current_type = "Building";
  DemoState demo;
  std::size_t sz_printed{0};
  flush_events(s, sz_printed, ptr_log);

  std::uint64_t u8_frames{0};
  std::uint32_t u4_next_logic{SDL_GetTicks()};
  std::uint32_t u4_next_render{SDL_GetTicks()};

  while (!ui.b_quit) {
    const std::uint32_t u4_now{SDL_GetTicks()};

    if (g_ctrl_c) ui.b_quit = true;

    if (u4_now - u4_next_logic > kLogicBehindDropMs) u4_next_logic = u4_now;
    if (static_cast<std::int32_t>(u4_now - u4_next_logic) >= 0) {
      u4_next_logic += static_cast<std::uint32_t>(args.int4_tick_ms);
      lch::session_tick(s);
      sound_tick(sound);
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
        if (event.type == SDL_QUIT) ui.b_quit = true;
        if (event.type == SDL_KEYDOWN) {
          const SDL_Keycode key{event.key.keysym.sym};
          if (key == SDLK_ESCAPE) ui.b_quit = true;
          if (key == SDLK_p && (event.key.keysym.mod & KMOD_CTRL)) ui.b_screenshot = true;
          if (key >= SDLK_1 && key <= SDLK_9) {
            const std::size_t idx_group{static_cast<std::size_t>(key - SDLK_1)};
            if (event.key.keysym.mod & KMOD_CTRL)
              inp::control_group_create(ui.selection, s.w, s.u4_viewer, idx_group);
            else
              inp::control_group_select(ui.selection, idx_group);
          }
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
    }

    const std::uint32_t u4_next{std::min(u4_next_logic, u4_next_render)};
    const std::int32_t int4_sleep{static_cast<std::int32_t>(u4_next - SDL_GetTicks())};
    if (int4_sleep > 0) SDL_Delay(static_cast<std::uint32_t>(int4_sleep));
  }

  std::println("exiting clean frames={} ticks={}", u8_frames, s.w.int4_world_tick);
  flush_events(s, sz_printed, ptr_log);

  s.vec_bridge_pending.clear();
  s.vec_movers.clear();
  s.vec_fighters.clear();
  s.om = net::OrderManagerModel{};
  s.w = ww::WorldModel{};
  sound_tick(sound);

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(ptr_window);
  SDL_Quit();
  return 0;
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

  const int int4_result{parsed.args.b_headless
                            ? run_headless(parsed.args, ptr_log, argv[0])
                            : run_window(parsed.args, ptr_log, argv[0])};
  if (ptr_log != nullptr) stream_log.close();
  return int4_result;
}
