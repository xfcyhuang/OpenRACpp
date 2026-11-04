import std;

#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "game/input/input_events.hpp"
#include "game/input/order_generator.hpp"
#include "game/input/production_panel.hpp"
#include "game/input/screen_map.hpp"
#include "game/input/selection.hpp"
#include "game/input/selection_logic.hpp"
#include "game/network/order_manager.hpp"
#include "game/world/world.hpp"

namespace {

namespace wig = ora::game::input;
namespace net = ora::game::network;
namespace ww = ora::game::world;

constexpr int kSkip{77};

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

void push_rect_outline(const wig::PxRect& rect, GLfloat fp4_thick, GLfloat fp4_r, GLfloat fp4_g,
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

constexpr const char* kFontGlyphs{"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -:.!"};

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
      {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
      {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}, {0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00},
      {0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15}};
  const char ch_upper{ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch};
  const char* ptr_found{std::strchr(kFontGlyphs, ch_upper)};
  if (ptr_found == nullptr || ch_upper == '\0') return arr_font[32];
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
    std::println("input_probe: screenshot failed: {}", SDL_GetError());
  else
    std::println("input_probe: screenshot {}", sz_path);
  SDL_FreeSurface(surf_shot);
}

constexpr int kViewW{960};
constexpr int kViewH{600};
constexpr int kLogicStepMs{40};
constexpr int kMaxFrames{900};

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
    "\tAttackProbe:\n"
    "\tOrderCounterProbe:\n"
    "probe.wall:\n"
    "\tSelectable:\n"
    "\t\tBounds: 1024,1024\n"
    "\tTargetable:\n"};

constexpr std::string_view kMap{
    "Title: input-probe\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@P1:\n"
    "\t\tName: P1\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"
    "\ttanka: probe.tank\n"
    "\t\tLocation: 10,10\n"
    "\t\tOwner: Neutral\n"
    "\ttankb: probe.tank\n"
    "\t\tLocation: 13,10\n"
    "\t\tOwner: Neutral\n"
    "\te1: probe.tank\n"
    "\t\tLocation: 20,10\n"
    "\t\tOwner: P1\n"};

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

wig::ModifierKeys mods_from_sdl(std::uint16_t u2_mods) {
  wig::ModifierKeys mods{wig::ModifierKeys::kNone};
  if (u2_mods & KMOD_CTRL) mods |= wig::ModifierKeys::kCtrl;
  if (u2_mods & KMOD_ALT) mods |= wig::ModifierKeys::kAlt;
  if (u2_mods & KMOD_SHIFT) mods |= wig::ModifierKeys::kShift;
  return mods;
}

struct ProbeSession {
  ww::WorldModel w;
  net::OrderManagerModel om;
  wig::SelectionModel selection;
  wig::DragState drag;
  wig::ProductionPanelModel panel;
  std::vector<wig::ProductionQueueView> vec_queues;
  MultiTapState tap;
  std::uint32_t u4_orders_total{};
  std::uint32_t u4_orders_move{};
  std::uint32_t u4_orders_attack{};
  std::uint32_t u4_orders_production{};
};

void issue_orders(ProbeSession& session, std::span<const net::OrderModel> vec_orders) {
  for (const net::OrderModel& order : vec_orders) {
    ++session.u4_orders_total;
    if (order.str_order == "Move") ++session.u4_orders_move;
    if (order.str_order == "Attack") ++session.u4_orders_attack;
    if (order.str_order == "StartProduction" || order.str_order == "PauseProduction" ||
        order.str_order == "CancelProduction")
      ++session.u4_orders_production;
    net::issue_order(session.om, order);
  }
}

void dispatch_mouse(ProbeSession& session, const wig::MouseInput& mi) {
  const std::uint32_t u4_viewer{0};

  const wig::PanelDispatchResult res_panel{wig::panel_handle_mouse(
      session.panel, wig::PanelGeometry{kViewW, kViewH}, session.vec_queues, 1, mi)};
  if (res_panel.b_handled) {
    issue_orders(session, res_panel.vec_orders);
    return;
  }

  wig::selection_handle_mouse(session.w, u4_viewer, session.selection, session.drag, mi);
  const std::vector<net::OrderModel> vec_orders{wig::order_generator_handle_mouse(
      session.w, u4_viewer, session.selection, session.drag, mi)};
  issue_orders(session, vec_orders);
}

void queue_stub_advance(ProbeSession& session) {
  for (wig::ProductionQueueView& queue : session.vec_queues) {
    bool b_advanced{};
    for (wig::ProductionItemView& item : queue.vec_queue) {
      if (item.b_done || item.b_paused) continue;
      if (item.int4_remaining_time > 0) {
        --item.int4_remaining_time;
        if (item.int4_remaining_time == 0) item.b_done = true;
      }
      b_advanced = true;
      break;
    }
    static_cast<void>(b_advanced);
  }
}

void push_world_draw(ProbeSession& session) {
  for (const std::uint32_t u4_id : session.w.vec_in_world) {
    const ww::ActorModel& actor{session.w.vec_actors_all[u4_id]};
    if (actor.str_type == "player" || actor.str_type == "world") continue;
    const auto opt_bounds{wig::actor_mouse_bounds(session.w, actor)};
    if (!opt_bounds.has_value()) continue;

    const bool b_neutral_owner{actor.u4_owner < session.w.vec_players.size() &&
                               actor.u4_owner != 0 &&
                               session.w.vec_players[actor.u4_owner].b_non_combatant};
    GLfloat fp4_r{0.9f}, fp4_g{0.2f}, fp4_b{0.2f};
    if (actor.u4_owner == 0) {
      fp4_r = 0.2f;
      fp4_g = 0.9f;
      fp4_b = 0.2f;
    } else if (b_neutral_owner) {
      fp4_r = 0.8f;
      fp4_g = 0.8f;
      fp4_b = 0.2f;
    }
    push_quad(static_cast<GLfloat>(opt_bounds->int4_x),
              static_cast<GLfloat>(opt_bounds->int4_y),
              static_cast<GLfloat>(opt_bounds->int4_w),
              static_cast<GLfloat>(opt_bounds->int4_h), fp4_r, fp4_g, fp4_b, 0.9f);

    bool b_selected{false};
    for (const std::uint32_t u4_sel : session.selection.vec_actor_ids)
      if (u4_sel == u4_id) b_selected = true;
    if (b_selected) push_rect_outline(*opt_bounds, 2, 1, 1, 1);
  }

  if (session.drag.b_dragging)
    push_rect_outline(
        wig::PxRect{std::min(session.drag.int4_start_x, session.drag.int4_mouse_x),
                    std::min(session.drag.int4_start_y, session.drag.int4_mouse_y),
                    std::abs(session.drag.int4_start_x - session.drag.int4_mouse_x),
                    std::abs(session.drag.int4_start_y - session.drag.int4_mouse_y)},
        1, 1, 1, 0.6f);
}

void push_panel_draw(ProbeSession& session) {
  constexpr wig::PanelGeometry kGeo{kViewW, kViewH};
  const wig::PxRect rect_panel{wig::panel_rect(kGeo)};
  push_quad(static_cast<GLfloat>(rect_panel.int4_x), static_cast<GLfloat>(rect_panel.int4_y),
            static_cast<GLfloat>(rect_panel.int4_w), static_cast<GLfloat>(rect_panel.int4_h),
            0.16f, 0.16f, 0.2f, 1);

  for (std::size_t idx{0}; idx < wig::kPanelTabTypes.size(); ++idx) {
    const wig::PxRect rect_tab{wig::panel_tab_rect(kGeo, idx)};
    const bool b_current{session.panel.str_current_type == wig::kPanelTabTypes[idx]};
    bool b_enabled{false};
    for (const wig::ProductionQueueView& queue : session.vec_queues)
      if (queue.str_type == wig::kPanelTabTypes[idx] && queue.b_enabled) b_enabled = true;
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
    push_text(std::string_view{wig::kPanelTabTypes[idx]}.substr(0, 1),
              static_cast<GLfloat>(rect_tab.int4_x + 11),
              static_cast<GLfloat>(rect_tab.int4_y + 10), 2, 1, 1, 1);
  }

  const wig::ProductionQueueView* ptr_queue{nullptr};
  for (const wig::ProductionQueueView& queue : session.vec_queues)
    if (queue.str_type == session.panel.str_current_type) ptr_queue = &queue;
  if (ptr_queue == nullptr) return;

  for (std::size_t idx{0}; idx < ptr_queue->vec_items.size() && idx < 24; ++idx) {
    const wig::PxRect rect_icon{wig::panel_icon_rect(kGeo, idx)};
    push_quad(static_cast<GLfloat>(rect_icon.int4_x), static_cast<GLfloat>(rect_icon.int4_y),
              static_cast<GLfloat>(rect_icon.int4_w), static_cast<GLfloat>(rect_icon.int4_h),
              0.35f, 0.35f, 0.5f, 1);
    push_text(ptr_queue->vec_items[idx], static_cast<GLfloat>(rect_icon.int4_x + 4),
              static_cast<GLfloat>(rect_icon.int4_y + 4), 2, 1, 1, 1);

    for (const wig::ProductionItemView& item : ptr_queue->vec_queue) {
      if (item.str_item != ptr_queue->vec_items[idx]) continue;
      const std::int32_t int4_progress{wig::panel_item_progress_percent(item)};
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
}

struct ScriptedEvent {
  std::uint32_t u4_frame;
  wig::MouseInput mi;
};

constexpr std::initializer_list<ScriptedEvent> kScript{
    ScriptedEvent{40, wig::MouseInput{.event = wig::MouseInputEvent::kDown,
                                      .button = wig::MouseButton::kLeft,
                                      .int4_x = 252,
                                      .int4_y = 252,
                                      .int4_tap_count = 1}},
    ScriptedEvent{44, wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                                      .button = wig::MouseButton::kLeft,
                                      .int4_x = 252,
                                      .int4_y = 252,
                                      .int4_tap_count = 1}},
    ScriptedEvent{60, wig::MouseInput{.event = wig::MouseInputEvent::kDown,
                                      .button = wig::MouseButton::kRight,
                                      .int4_x = 500,
                                      .int4_y = 500,
                                      .int4_tap_count = 1}},
    ScriptedEvent{62, wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                                      .button = wig::MouseButton::kRight,
                                      .int4_x = 500,
                                      .int4_y = 500,
                                      .int4_tap_count = 1}},
    ScriptedEvent{70, wig::MouseInput{.event = wig::MouseInputEvent::kDown,
                                      .button = wig::MouseButton::kRight,
                                      .int4_x = 492,
                                      .int4_y = 252,
                                      .int4_tap_count = 1}},
    ScriptedEvent{72, wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                                      .button = wig::MouseButton::kRight,
                                      .int4_x = 492,
                                      .int4_y = 252,
                                      .int4_tap_count = 1}},
    ScriptedEvent{80, wig::MouseInput{.event = wig::MouseInputEvent::kDown,
                                      .button = wig::MouseButton::kLeft,
                                      .int4_x = 740,
                                      .int4_y = 112,
                                      .int4_tap_count = 1}},
    ScriptedEvent{90, wig::MouseInput{.event = wig::MouseInputEvent::kDown,
                                      .button = wig::MouseButton::kLeft,
                                      .int4_x = 770,
                                      .int4_y = 20,
                                      .int4_tap_count = 1}},
};

int run_probe(bool b_manual) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::println("input_probe: SDL_Init failed: {}", SDL_GetError());
    return kSkip;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_Window* ptr_window{SDL_CreateWindow("ora_input_probe", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, kViewW, kViewH,
                                          SDL_WINDOW_OPENGL)};
  if (ptr_window == nullptr) {
    std::println("input_probe: window creation failed: {}", SDL_GetError());
    SDL_Quit();
    return kSkip;
  }
  SDL_GLContext ctx{SDL_GL_CreateContext(ptr_window)};
  if (ctx == nullptr) {
    std::println("input_probe: GL context failed: {}", SDL_GetError());
    SDL_DestroyWindow(ptr_window);
    SDL_Quit();
    return kSkip;
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
  gl_enable_vertex_attrib_array = gl_load<decltype(gl_enable_vertex_attrib_array)>("glEnableVertexAttribArray");
  gl_vertex_attrib_pointer = gl_load<decltype(gl_vertex_attrib_pointer)>("glVertexAttribPointer");
  gl_draw_arrays = gl_load<decltype(gl_draw_arrays)>("glDrawArrays");
  gl_gen_vertex_arrays = gl_load<decltype(gl_gen_vertex_arrays)>("glGenVertexArrays");
  gl_bind_vertex_array = gl_load<decltype(gl_bind_vertex_array)>("glBindVertexArray");
  gl_read_pixels = gl_load<decltype(gl_read_pixels)>("glReadPixels");

  const char* sz_vert{
      "#version 150\n"
      "in vec2 pos;\n"
      "in vec4 col;\n"
      "out vec4 vcol;\n"
      "void main() {\n"
      "  vcol = col;\n"
      "  gl_Position = vec4(pos.x / 480.0 - 1.0, 1.0 - pos.y / 300.0, 0.0, 1.0);\n"
      "}\n"};
  const char* sz_frag{
      "#version 150\n"
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
      std::println("input_probe: shader compile failed");
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

  auto res_map{ww::synthetic_map_from_yaml(kRules, kMap)};
  if (!res_map.has_value()) {
    std::println("input_probe: map error: {}", res_map.error());
    return 1;
  }
  auto res_world{ww::world_create(*res_map, 0)};
  if (!res_world.has_value()) {
    std::println("input_probe: world error: {}", res_world.error());
    return 1;
  }

  ProbeSession session{};
  session.w = std::move(*res_world);
  const std::int32_t arr_clients[]{1};
  net::order_manager_start_game(session.om, session.w, arr_clients);

  wig::ProductionQueueView queue;
  queue.str_type = "Vehicle";
  queue.b_enabled = true;
  queue.vec_items = {"jeep", "1tnk"};
  session.vec_queues.push_back(std::move(queue));

  std::uint32_t u4_frame{0};
  std::uint32_t u4_ticks_last_logic{SDL_GetTicks()};
  bool b_quit{false};
  int int4_failures{0};

  while (!b_quit) {
    if (!b_manual && u4_frame >= kMaxFrames) break;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) b_quit = true;
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) b_quit = true;
      if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
          event.type == SDL_MOUSEMOTION) {
        const bool b_down{event.type == SDL_MOUSEBUTTONDOWN};
        const std::int32_t int4_x{
            event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x};
        const std::int32_t int4_y{
            event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y};
        wig::MouseButton button{wig::MouseButton::kNone};
        if (event.type != SDL_MOUSEMOTION) {
          if (event.button.button == SDL_BUTTON_LEFT) button = wig::MouseButton::kLeft;
          if (event.button.button == SDL_BUTTON_RIGHT) button = wig::MouseButton::kRight;
          if (event.button.button == SDL_BUTTON_MIDDLE) button = wig::MouseButton::kMiddle;
        }
        wig::MouseInput mi;
        mi.int4_x = int4_x;
        mi.int4_y = int4_y;
        mi.mods = mods_from_sdl(SDL_GetModState());
        if (event.type == SDL_MOUSEMOTION) {
          mi.event = wig::MouseInputEvent::kMove;
        } else {
          mi.event = b_down ? wig::MouseInputEvent::kDown : wig::MouseInputEvent::kUp;
          mi.button = button;
          mi.int4_tap_count =
              b_down ? tap_count_update(session.tap, SDL_GetTicks(), int4_x, int4_y) : 1;
        }
        dispatch_mouse(session, mi);
      }
    }

    if (!b_manual) {
      for (const ScriptedEvent& step : kScript) {
        if (step.u4_frame != u4_frame) continue;
        dispatch_mouse(session, step.mi);
      }
    }

    const std::uint32_t u4_now{SDL_GetTicks()};
    if (u4_now - u4_ticks_last_logic >= static_cast<std::uint32_t>(kLogicStepMs)) {
      u4_ticks_last_logic = u4_now;
      net::run_frame(session.om, session.w);
      queue_stub_advance(session);
    }

    vec_quads.clear();
    push_quad(0, 0, kViewW, kViewH, 0.05f, 0.07f, 0.1f, 1);
    push_world_draw(session);
    push_panel_draw(session);
    push_text(std::format("SEL {} ORD {} MOVE {} ATK {} PROD {}",
                          session.selection.vec_actor_ids.size(), session.u4_orders_total,
                          session.u4_orders_move, session.u4_orders_attack,
                          session.u4_orders_production),
              8, 8, 2, 1, 1, 0.4f);

    std::vector<GLfloat> vec_verts;
    vec_verts.reserve(vec_quads.size() * 30);
    for (const Quad& quad : vec_quads) {
      const GLfloat fp4_verts[]{quad.fp4_x, quad.fp4_y, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a,
                                quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a,
                                quad.fp4_x, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a,
                                quad.fp4_x + quad.fp4_w, quad.fp4_y + quad.fp4_h, quad.fp4_r, quad.fp4_g, quad.fp4_b, quad.fp4_a};
      vec_verts.insert(vec_verts.end(), fp4_verts, fp4_verts + 36);
    }
    gl_clear_color(0, 0, 0, 1);
    gl_clear(GL_COLOR_BUFFER_BIT);
    gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(vec_verts.size() * sizeof(GLfloat)),
                   vec_verts.data(), GL_STATIC_DRAW);
    gl_draw_arrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vec_quads.size() * 6));
    SDL_GL_SwapWindow(ptr_window);

    if (!b_manual && u4_frame == 200) save_screenshot(ptr_window, "t14-input-probe.bmp");

    ++u4_frame;
  }

  if (!b_manual) {
    if (session.u4_orders_total == 0) {
      std::println("input_probe: no orders produced by scripted events");
      ++int4_failures;
    }
    if (session.u4_orders_move == 0 || session.u4_orders_attack == 0) {
      std::println("input_probe: missing move or attack orders");
      ++int4_failures;
    }
    if (session.u4_orders_production == 0) {
      std::println("input_probe: missing production orders");
      ++int4_failures;
    }
    if (!session.selection.vec_actor_ids.empty() && session.drag.b_dragging) {
      std::println("input_probe: drag state stuck");
      ++int4_failures;
    }
    if (!session.om.str_fatal.empty()) {
      std::println("input_probe: order manager fatal: {}", session.om.str_fatal);
      ++int4_failures;
    }
    std::println("input_probe: {} frames, orders total={} move={} attack={} production={}",
                 u4_frame, session.u4_orders_total, session.u4_orders_move,
                 session.u4_orders_attack, session.u4_orders_production);
  }

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(ptr_window);
  SDL_Quit();
  return int4_failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  bool b_manual{false};
  for (int idx{1}; idx < argc; ++idx)
    if (std::string_view(argv[idx]) == "--manual") b_manual = true;
  return run_probe(b_manual);
}
