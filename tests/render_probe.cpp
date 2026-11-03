import std;

#include <SDL.h>
#include <cstdlib>
#include <cstring>

#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/render/palette.hpp"
#include "game/render/shp_td.hpp"
#include "game/render/tmp_ra.hpp"
#include "game/tileset.hpp"

#ifndef ORA_CONTENT_TEMPERAT_DIR
#define ORA_CONTENT_TEMPERAT_DIR ""
#endif

#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

#ifndef ORA_RENDER_GOLDEN_DIR
#define ORA_RENDER_GOLDEN_DIR "tests/golden/render"
#endif

namespace {

namespace ogf = ora::game::filesystem;
namespace ogr = ora::game::render;

constexpr int kSkip{77};
constexpr int kTile{24};
constexpr int kAtlas{1024};

typedef unsigned int GLenum;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef int GLint;
typedef int GLsizei;

static void (*gl_clear_color)(GLfloat, GLfloat, GLfloat, GLfloat);
static void (*gl_clear)(GLenum);
static unsigned (*gl_get_error)();
static void (*gl_enable)(GLenum);
static void (*gl_blend_func)(GLenum, GLenum);
static unsigned (*gl_create_shader)(GLenum);
static void (*gl_shader_source)(unsigned, GLsizei, const char* const*, const int*);
static void (*gl_compile_shader)(unsigned);
static void (*gl_get_shader_iv)(unsigned, GLenum, int*);
static int (*gl_create_program)();
static void (*gl_attach_shader)(unsigned, unsigned);
static void (*gl_link_program)(unsigned);
static void (*gl_use_program)(unsigned);
static void (*gl_gen_textures)(GLsizei, unsigned*);
static void (*gl_bind_texture)(GLenum, unsigned);
static void (*gl_tex_image_2d)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
static void (*gl_tex_parameteri)(GLenum, GLenum, GLint);
static void (*gl_gen_buffers)(GLsizei, unsigned*);
static void (*gl_bind_buffer)(GLenum, unsigned);
static void (*gl_buffer_data)(GLenum, long, const void*, GLenum);
static int (*gl_get_attrib_location)(unsigned, const char*);
static void (*gl_enable_vertex_attrib_array)(unsigned);
static void (*gl_vertex_attrib_pointer)(unsigned, GLint, GLenum, GLboolean, GLsizei, const void*);
static void (*gl_draw_elements)(GLenum, GLsizei, GLenum, const void*);
static void (*gl_gen_vertex_arrays)(GLsizei, unsigned*);
static void (*gl_bind_vertex_array)(unsigned);
static void (*gl_read_buffer)(GLenum);
static void (*gl_read_pixels)(int, int, GLsizei, GLsizei, GLenum, GLenum, void*);

constexpr GLenum GL_COLOR_BUFFER_BIT{0x4000};
constexpr GLenum GL_TRIANGLES{0x0004};
constexpr GLenum GL_FLOAT{0x1406};
constexpr GLenum GL_UNSIGNED_SHORT{0x1403};
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER{0x8893};
constexpr GLboolean GL_FALSE{0};
constexpr GLenum GL_VERTEX_SHADER{0x8B31};
constexpr GLenum GL_FRAGMENT_SHADER{0x8B30};
constexpr GLenum GL_COMPILE_STATUS{0x8B81};
constexpr GLenum GL_TEXTURE_2D{0x0DE1};
constexpr GLenum GL_TEXTURE_MIN_FILTER{0x2801};
constexpr GLenum GL_TEXTURE_MAG_FILTER{0x2800};
constexpr GLint GL_NEAREST{0x2600};
constexpr GLint GL_RGBA8{0x8058};
constexpr GLenum GL_BGRA{0x80E1};
constexpr GLenum GL_UNSIGNED_BYTE{0x1401};
constexpr GLenum GL_ARRAY_BUFFER{0x8892};
constexpr GLenum GL_STATIC_DRAW{0x88E4};
constexpr GLenum GL_BACK{0x0405};
constexpr GLenum GL_RGB{0x1907};
constexpr GLenum GL_BLEND{0x0BE2};
constexpr GLenum GL_ONE{1};
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA{0x0303};

template <typename T>
T gl_load(const char* sz_name) {
  return reinterpret_cast<T>(SDL_GL_GetProcAddress(sz_name));
}

struct Bgra {
  std::uint8_t u1_b{};
  std::uint8_t u1_g{};
  std::uint8_t u1_r{};
  std::uint8_t u1_a{};
};

struct SpriteRect {
  int int4_x{};
  int int4_y{};
  int int4_w{};
  int int4_h{};
  int int4_off_x{};
  int int4_off_y{};
};

struct AtlasAllocator {
  int int4_row_h{kTile + 1};
  int int4_x{1};
  int int4_y{1};
  int int4_used_h{1};

  SpriteRect alloc(int int4_w, int int4_h) {
    if (int4_x + int4_w + 1 > kAtlas) {
      int4_x = 1;
      int4_y = int4_used_h;
    }
    SpriteRect rect{int4_x, int4_y, int4_w, int4_h, 0, 0};
    int4_x += int4_w + 1;
    const int int4_need{int4_y + int4_h + 1};
    if (int4_need > int4_used_h) int4_used_h = int4_need;
    return rect;
  }
};

std::optional<std::string> read_file_bytes(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string{str_path}}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::optional<std::string> content_dir() {
  std::string str_dir{ORA_CONTENT_TEMPERAT_DIR};
  if (const char* sz_env{std::getenv("ORA_CONTENT_TEMPERAT")}; sz_env != nullptr && *sz_env != '\0')
    str_dir = sz_env;
  if (str_dir.empty()) return std::nullopt;
  if (!std::filesystem::exists(std::filesystem::path{str_dir + "/clear1.tem"}))
    return std::nullopt;
  return str_dir;
}

std::vector<Bgra> palette_to_bgra(const ogr::PaletteTable& pal_table) {
  std::vector<Bgra> vec_bgra(256);
  for (int int4_i{0}; int4_i < 256; ++int4_i) {
    const std::uint32_t u4_c{pal_table.arr4u_colors[static_cast<std::size_t>(int4_i)]};
    vec_bgra[static_cast<std::size_t>(int4_i)] = Bgra{
        static_cast<std::uint8_t>(u4_c & 0xFF), static_cast<std::uint8_t>((u4_c >> 8) & 0xFF),
        static_cast<std::uint8_t>((u4_c >> 16) & 0xFF), static_cast<std::uint8_t>((u4_c >> 24) & 0xFF)};
  }
  return vec_bgra;
}

void blit_premultiplied(std::vector<Bgra>& vec_bgra_atlas, const SpriteRect& rect_atlas,
    std::span<const std::uint8_t> span1u_indices, const std::vector<Bgra>& vec_bgra_pal) {
  for (int int4_y{0}; int4_y < rect_atlas.int4_h; ++int4_y)
    for (int int4_x{0}; int4_x < rect_atlas.int4_w; ++int4_x) {
      const Bgra bgra_c{vec_bgra_pal[span1u_indices[static_cast<std::size_t>(int4_y)
              * static_cast<std::size_t>(rect_atlas.int4_w)
          + static_cast<std::size_t>(int4_x)]]};
      const std::uint32_t u4_a{bgra_c.u1_a};
      vec_bgra_atlas[static_cast<std::size_t>(rect_atlas.int4_y + int4_y) * kAtlas
          + static_cast<std::size_t>(rect_atlas.int4_x + int4_x)]
          = Bgra{static_cast<std::uint8_t>(bgra_c.u1_b * u4_a / 255),
              static_cast<std::uint8_t>(bgra_c.u1_g * u4_a / 255),
              static_cast<std::uint8_t>(bgra_c.u1_r * u4_a / 255), bgra_c.u1_a};
    }
}

int int4_failures{0};

void probe_check(bool b_ok, std::string_view str_what) {
  if (!b_ok) {
    ++int4_failures;
    std::println("render_probe: {}", str_what);
  }
}

}

int main(int argc, char* argv[]) {
  const std::string str_out{argc > 1 ? argv[1] : "t09_screenshot.bmp"};

  if (const char* sz_headless{std::getenv("ORA_RENDER_HEADLESS")};
      sz_headless != nullptr && *sz_headless != '\0') {
    std::println("render_probe: SKIP（ORA_RENDER_HEADLESS 置位）");
    return kSkip;
  }

  const auto opt_dir{content_dir()};
  if (!opt_dir.has_value()) {
    std::println("render_probe: SKIP（无 EA 内容包抽取素材；设 ORA_CONTENT_TEMPERAT 指向抽取目录）");
    return kSkip;
  }

  const auto opt_map{read_file_bytes(std::string{ORA_MAPS_GOLDEN_DIR} + "/ore-lord.oramap")};
  const auto opt_tileset{read_file_bytes(std::string{ORA_MAPS_GOLDEN_DIR} + "/tilesets/temperat.yaml")};
  const auto opt_pal{read_file_bytes(std::string{ORA_RENDER_GOLDEN_DIR} + "/temperat.pal")};
  if (!opt_map.has_value() || !opt_tileset.has_value() || !opt_pal.has_value()) {
    std::println("render_probe: SKIP（golden 地图/tileset/调色板缺失）");
    return kSkip;
  }

  const auto res_zip{ogf::zip_read(std::span<const std::uint8_t>{
      reinterpret_cast<const std::uint8_t*>(opt_map->data()), opt_map->size()})};
  if (!res_zip) {
    std::println("render_probe: ore-lord.oramap 解包失败：{}", res_zip.error());
    return 1;
  }
  const auto res_yaml{ogf::zip_entry_data(*res_zip, "map.yaml")};
  const auto res_bin{ogf::zip_entry_data(*res_zip, "map.bin")};
  if (!res_yaml || !res_bin) {
    std::println("render_probe: oramap 条目缺失");
    return 1;
  }
  const auto res_header{ora::game::map_header_parse(*res_yaml, "map.yaml")};
  if (!res_header) {
    std::println("render_probe: map.yaml 头解析失败：{}", res_header.error());
    return 1;
  }
  const auto res_mapbin{ora::game::map_bin_parse(*res_bin, res_header->int4_size_x,
      res_header->int4_size_y)};
  if (!res_mapbin) {
    std::println("render_probe: map.bin 解析失败：{}", res_mapbin.error());
    return 1;
  }
  const auto res_tileset{ora::game::tileset_load(*opt_tileset, "temperat.yaml")};
  if (!res_tileset) {
    std::println("render_probe: tileset 加载失败：{}", res_tileset.error());
    return 1;
  }
  const int int4_map_w{res_header->int4_size_x};
  const int int4_map_h{res_header->int4_size_y};

  const std::span<const std::uint8_t> span1u_pal{
      reinterpret_cast<const std::uint8_t*>(opt_pal->data()), opt_pal->size()};
  const auto res_pal{ogr::palette_from_pal_bytes(span1u_pal, "temperat.pal")};
  if (!res_pal) {
    std::println("render_probe: 调色板加载失败：{}", res_pal.error());
    return 1;
  }
  const int vec4i_transparent[]{0};
  auto pal_terrain{res_pal.value()};
  const int vec4i_terrain_shadow[]{3, 4};
  ogr::palette_remap_indexes(pal_terrain, vec4i_transparent, vec4i_terrain_shadow);
  auto pal_player{res_pal.value()};
  const int vec4i_player_shadow[]{4};
  ogr::palette_remap_indexes(pal_player, vec4i_transparent, vec4i_player_shadow);
  const std::vector<Bgra> vec_bgra_terrain{palette_to_bgra(pal_terrain)};
  const std::vector<Bgra> vec_bgra_player{palette_to_bgra(pal_player)};

  std::vector<Bgra> vec_bgra_atlas(static_cast<std::size_t>(kAtlas) * kAtlas, Bgra{0, 0, 0, 0});
  AtlasAllocator alloc_tiles{};
  std::map<std::uint16_t, std::vector<SpriteRect>> map_template_rects;
  std::set<std::uint16_t> set_types_needed;
  for (const ora::game::TerrainTile& tile : res_mapbin->vec_tiles)
    set_types_needed.insert(tile.u2_type);

  for (const ora::game::TerrainTemplate& tpl : res_tileset->vec_templates) {
    if (set_types_needed.count(tpl.u2_id) == 0 || tpl.vec_str_images.empty()) continue;
    const auto opt_tem{read_file_bytes(std::string{*opt_dir} + "/" + tpl.vec_str_images[0])};
    if (!opt_tem.has_value()) {
      std::println("render_probe: 素材 {} 缺失（模板 {}）", tpl.vec_str_images[0], tpl.u2_id);
      return 1;
    }
    const auto res_tem{ogr::tmp_ra_load(std::span<const std::uint8_t>{
                          reinterpret_cast<const std::uint8_t*>(opt_tem->data()), opt_tem->size()},
        tpl.vec_str_images[0])};
    if (!res_tem) {
      std::println("render_probe: {} 解码失败：{}", tpl.vec_str_images[0], res_tem.error());
      return 1;
    }
    std::vector<SpriteRect> vec_rects;
    vec_rects.reserve(res_tem->vec_frames.size());
    for (const ogr::TmpRaFrame& frm : res_tem->vec_frames) {
      if (frm.vec1u_data.empty()) {
        vec_rects.push_back(SpriteRect{});
        continue;
      }
      const SpriteRect rect{alloc_tiles.alloc(frm.int4_width, frm.int4_height)};
      blit_premultiplied(vec_bgra_atlas, rect, frm.vec1u_data, vec_bgra_terrain);
      vec_rects.push_back(rect);
    }
    map_template_rects.emplace(tpl.u2_id, std::move(vec_rects));
  }

  const auto opt_tnk{read_file_bytes(std::string{*opt_dir} + "/1tnk.shp")};
  if (!opt_tnk.has_value()) {
    std::println("render_probe: 1tnk.shp 缺失");
    return 1;
  }
  const auto res_tnk{ogr::shp_td_load(std::span<const std::uint8_t>{
                         reinterpret_cast<const std::uint8_t*>(opt_tnk->data()), opt_tnk->size()},
      "1tnk.shp")};
  if (!res_tnk) {
    std::println("render_probe: 1tnk.shp 解码失败：{}", res_tnk.error());
    return 1;
  }
  std::vector<SpriteRect> vec_tnk_rects;
  for (int int4_f{0}; int4_f < 32; int4_f += 4) {
    const ogr::ShpTdFrame& frm{res_tnk->vec_frames[static_cast<std::size_t>(int4_f)]};
    SpriteRect rect{alloc_tiles.alloc(frm.int4_width, frm.int4_height)};
    blit_premultiplied(vec_bgra_atlas, rect, frm.vec1u_data, vec_bgra_player);
    rect.int4_off_x = frm.int4_off_x;
    rect.int4_off_y = frm.int4_off_y;
    vec_tnk_rects.push_back(rect);
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::println("render_probe: SKIP（SDL_Init 失败：{}）", SDL_GetError());
    return kSkip;
  }
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  int int4_view_w{kTile * int4_map_w};
  int int4_view_h{kTile * int4_map_h};
  if (const char* sz_view{std::getenv("ORA_RENDER_VIEW")}; sz_view != nullptr && *sz_view != '\0')
    int4_view_w = int4_view_h = kTile * std::atoi(sz_view);
  SDL_Window* window{SDL_CreateWindow("ora_render_probe (ore-lord TEMPERAT)",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, int4_view_w, int4_view_h,
      SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI)};
  if (window == nullptr) {
    std::println("render_probe: SKIP（窗口创建失败：{}）", SDL_GetError());
    SDL_Quit();
    return kSkip;
  }
  SDL_GLContext ctx{SDL_GL_CreateContext(window)};
  if (ctx == nullptr) {
    std::println("render_probe: SKIP（GL context 创建失败：{}）", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return kSkip;
  }

  gl_clear_color = gl_load<void (*)(GLfloat, GLfloat, GLfloat, GLfloat)>("glClearColor");
  gl_clear = gl_load<void (*)(GLenum)>("glClear");
  gl_get_error = gl_load<unsigned (*)()>("glGetError");
  gl_enable = gl_load<void (*)(GLenum)>("glEnable");
  gl_blend_func = gl_load<void (*)(GLenum, GLenum)>("glBlendFunc");
  gl_create_shader = gl_load<unsigned (*)(GLenum)>("glCreateShader");
  gl_shader_source = gl_load<void (*)(unsigned, GLsizei, const char* const*, const int*)>("glShaderSource");
  gl_compile_shader = gl_load<void (*)(unsigned)>("glCompileShader");
  gl_get_shader_iv = gl_load<void (*)(unsigned, GLenum, int*)>("glGetShaderiv");
  gl_create_program = gl_load<int (*)()>("glCreateProgram");
  gl_attach_shader = gl_load<void (*)(unsigned, unsigned)>("glAttachShader");
  gl_link_program = gl_load<void (*)(unsigned)>("glLinkProgram");
  gl_use_program = gl_load<void (*)(unsigned)>("glUseProgram");
  gl_gen_textures = gl_load<void (*)(GLsizei, unsigned*)>("glGenTextures");
  gl_bind_texture = gl_load<void (*)(GLenum, unsigned)>("glBindTexture");
  gl_tex_image_2d = gl_load<void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)>("glTexImage2D");
  gl_tex_parameteri = gl_load<void (*)(GLenum, GLenum, GLint)>("glTexParameteri");
  gl_gen_buffers = gl_load<void (*)(GLsizei, unsigned*)>("glGenBuffers");
  gl_bind_buffer = gl_load<void (*)(GLenum, unsigned)>("glBindBuffer");
  gl_buffer_data = gl_load<void (*)(GLenum, long, const void*, GLenum)>("glBufferData");
  gl_get_attrib_location = gl_load<int (*)(unsigned, const char*)>("glGetAttribLocation");
  gl_enable_vertex_attrib_array = gl_load<void (*)(unsigned)>("glEnableVertexAttribArray");
  gl_vertex_attrib_pointer = gl_load<void (*)(unsigned, GLint, GLenum, GLboolean, GLsizei, const void*)>("glVertexAttribPointer");
  gl_draw_elements = gl_load<void (*)(GLenum, GLsizei, GLenum, const void*)>("glDrawElements");
  gl_gen_vertex_arrays = gl_load<void (*)(GLsizei, unsigned*)>("glGenVertexArrays");
  gl_bind_vertex_array = gl_load<void (*)(unsigned)>("glBindVertexArray");
  gl_read_buffer = gl_load<void (*)(GLenum)>("glReadBuffer");
  gl_read_pixels = gl_load<void (*)(int, int, GLsizei, GLsizei, GLenum, GLenum, void*)>("glReadPixels");

  unsigned tex_atlas{};
  gl_gen_textures(1, &tex_atlas);
  gl_bind_texture(GL_TEXTURE_2D, tex_atlas);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlas, kAtlas, 0, GL_BGRA, GL_UNSIGNED_BYTE,
      vec_bgra_atlas.data());

  const char* sz_vs{
      "#version 150\n"
      "in vec2 aPos;\n"
      "in vec2 aUv;\n"
      "out vec2 vUv;\n"
      "void main() { gl_Position = vec4(aPos, 0.0, 1.0); vUv = aUv; }\n"};
  const char* sz_fs{
      "#version 150\n"
      "in vec2 vUv;\n"
      "out vec4 fragColor;\n"
      "uniform sampler2D uTex;\n"
      "void main() { fragColor = texture(uTex, vUv); }\n"};
  const auto fn_compile{[&](GLenum en_type, const char* sz_src) -> unsigned {
    const unsigned u1_shader{gl_create_shader(en_type)};
    const char* sz_srcs[]{sz_src};
    int int4_len{static_cast<int>(std::strlen(sz_src))};
    gl_shader_source(u1_shader, 1, sz_srcs, &int4_len);
    gl_compile_shader(u1_shader);
    int int4_ok{0};
    gl_get_shader_iv(u1_shader, GL_COMPILE_STATUS, &int4_ok);
    if (int4_ok == 0) std::println("render_probe: shader 编译失败");
    return u1_shader;
  }};
  const unsigned u1_prog{static_cast<unsigned>(gl_create_program())};
  gl_attach_shader(u1_prog, fn_compile(GL_VERTEX_SHADER, sz_vs));
  gl_attach_shader(u1_prog, fn_compile(GL_FRAGMENT_SHADER, sz_fs));
  gl_link_program(u1_prog);
  gl_use_program(u1_prog);

  struct Vtx {
    float fp4_x{};
    float fp4_y{};
    float fp4_u{};
    float fp4_v{};
  };
  std::vector<Vtx> vec_verts;
  std::vector<unsigned short> idx;
  const auto fn_quad{[&](float fp4_x0, float fp4_y0, float fp4_x1, float fp4_y1, float fp4_u0,
                         float fp4_v0, float fp4_u1, float fp4_v1) {
    vec_verts.push_back(Vtx{fp4_x0, fp4_y0, fp4_u0, fp4_v0});
    vec_verts.push_back(Vtx{fp4_x1, fp4_y0, fp4_u1, fp4_v0});
    vec_verts.push_back(Vtx{fp4_x1, fp4_y1, fp4_u1, fp4_v1});
    vec_verts.push_back(Vtx{fp4_x0, fp4_y1, fp4_u0, fp4_v1});
    const unsigned short u2_b{static_cast<unsigned short>(vec_verts.size() - 4)};
    idx.push_back(u2_b);
    idx.push_back(static_cast<unsigned short>(u2_b + 1));
    idx.push_back(static_cast<unsigned short>(u2_b + 2));
    idx.push_back(u2_b);
    idx.push_back(static_cast<unsigned short>(u2_b + 2));
    idx.push_back(static_cast<unsigned short>(u2_b + 3));
  }};
  const float fp4_w{static_cast<float>(int4_view_w)};
  const float fp4_h{static_cast<float>(int4_view_h)};
  std::size_t sz_missing_tiles{0};
  for (int int4_j{0}; int4_j < int4_map_h; ++int4_j)
    for (int int4_i{0}; int4_i < int4_map_w; ++int4_i) {
      const ora::game::TerrainTile tile{res_mapbin->tile_at(int4_i, int4_j)};
      const auto it_rects{map_template_rects.find(tile.u2_type)};
      if (it_rects == map_template_rects.end()
          || tile.u1_index >= it_rects->second.size()
          || it_rects->second[tile.u1_index].int4_w == 0) {
        ++sz_missing_tiles;
        continue;
      }
      const SpriteRect& rect{it_rects->second[tile.u1_index]};
      const float fp4_px{static_cast<float>(int4_i * kTile)};
      const float fp4_py{static_cast<float>(int4_j * kTile)};
      fn_quad(fp4_px / fp4_w * 2.0f - 1.0f, 1.0f - fp4_py / fp4_h * 2.0f,
          (fp4_px + kTile) / fp4_w * 2.0f - 1.0f, 1.0f - (fp4_py + kTile) / fp4_h * 2.0f,
          static_cast<float>(rect.int4_x) / kAtlas, static_cast<float>(rect.int4_y) / kAtlas,
          static_cast<float>(rect.int4_x + rect.int4_w) / kAtlas,
          static_cast<float>(rect.int4_y + rect.int4_h) / kAtlas);
    }

  const int int4_tnk_row{int4_view_h >= kTile * 32 ? 60 : 10};
  std::vector<std::pair<int, int>> vec_tnk_screen;
  for (std::size_t sz_k{0}; sz_k < vec_tnk_rects.size(); ++sz_k) {
    const SpriteRect& rect{vec_tnk_rects[sz_k]};
    const int int4_cx{(6 + static_cast<int>(sz_k)) * kTile + kTile / 2};
    const int int4_cy{int4_tnk_row * kTile + kTile / 2};
    const int int4_px{int4_cx + rect.int4_off_x - rect.int4_w / 2};
    const int int4_py{int4_cy + rect.int4_off_y - rect.int4_h / 2};
    vec_tnk_screen.emplace_back(int4_px, int4_py);
    fn_quad(static_cast<float>(int4_px) / fp4_w * 2.0f - 1.0f,
        1.0f - static_cast<float>(int4_py) / fp4_h * 2.0f,
        static_cast<float>(int4_px + rect.int4_w) / fp4_w * 2.0f - 1.0f,
        1.0f - static_cast<float>(int4_py + rect.int4_h) / fp4_h * 2.0f,
        static_cast<float>(rect.int4_x) / kAtlas, static_cast<float>(rect.int4_y) / kAtlas,
        static_cast<float>(rect.int4_x + rect.int4_w) / kAtlas,
        static_cast<float>(rect.int4_y + rect.int4_h) / kAtlas);
  }

  unsigned vao{};
  gl_gen_vertex_arrays(1, &vao);
  gl_bind_vertex_array(vao);
  unsigned vbo{};
  gl_gen_buffers(1, &vbo);
  gl_bind_buffer(GL_ARRAY_BUFFER, vbo);
  gl_buffer_data(GL_ARRAY_BUFFER, static_cast<long>(vec_verts.size() * sizeof(Vtx)),
      vec_verts.data(), GL_STATIC_DRAW);
  unsigned ibo{};
  gl_gen_buffers(1, &ibo);
  gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  gl_buffer_data(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(idx.size() * 2), idx.data(),
      GL_STATIC_DRAW);

  const int int4_pos{gl_get_attrib_location(u1_prog, "aPos")};
  const int int4_uv{gl_get_attrib_location(u1_prog, "aUv")};
  gl_enable_vertex_attrib_array(static_cast<unsigned>(int4_pos));
  gl_vertex_attrib_pointer(static_cast<unsigned>(int4_pos), 2, GL_FLOAT, GL_FALSE, sizeof(Vtx),
      reinterpret_cast<void*>(0));
  gl_enable_vertex_attrib_array(static_cast<unsigned>(int4_uv));
  gl_vertex_attrib_pointer(static_cast<unsigned>(int4_uv), 2, GL_FLOAT, GL_FALSE, sizeof(Vtx),
      reinterpret_cast<void*>(8));

  gl_enable(GL_BLEND);
  gl_blend_func(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  int int4_frames{600};
  if (const char* sz_frames{std::getenv("ORA_RENDER_FRAMES")};
      sz_frames != nullptr && *sz_frames != '\0')
    int4_frames = std::atoi(sz_frames);
  if (int4_frames < 1) int4_frames = 1;
  const std::uint32_t u4_ms_start{SDL_GetTicks()};
  for (int int4_f{0}; int4_f < int4_frames; ++int4_f) {
    gl_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    gl_clear(GL_COLOR_BUFFER_BIT);
    GLsizei n_draw{static_cast<GLsizei>(idx.size())};
  if (const char* sz_max{std::getenv("ORA_RENDER_MAXQUADS")}; sz_max != nullptr && *sz_max != 0)
    n_draw = std::min(n_draw, static_cast<GLsizei>(std::atoi(sz_max)) * 6);
  gl_draw_elements(GL_TRIANGLES, n_draw, GL_UNSIGNED_SHORT, nullptr);
    SDL_GL_SwapWindow(window);
    SDL_PumpEvents();
    SDL_Event ev_event;
    while (SDL_PeepEvents(&ev_event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1) {
      if (ev_event.type == SDL_QUIT) int4_frames = int4_f + 1;
    }
    const std::uint32_t u4_target{u4_ms_start
        + static_cast<std::uint32_t>(int4_f + 1) * 1000u / 60u};
    while (SDL_GetTicks() < u4_target) SDL_Delay(1);
  }

  gl_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
  gl_clear(GL_COLOR_BUFFER_BIT);
  GLsizei n_draw{static_cast<GLsizei>(idx.size())};
  if (const char* sz_max{std::getenv("ORA_RENDER_MAXQUADS")}; sz_max != nullptr && *sz_max != 0)
    n_draw = std::min(n_draw, static_cast<GLsizei>(std::atoi(sz_max)) * 6);
  gl_draw_elements(GL_TRIANGLES, n_draw, GL_UNSIGNED_SHORT, nullptr);

  int int4_w{};
  int int4_h{};
  SDL_GL_GetDrawableSize(window, &int4_w, &int4_h);
  std::vector<unsigned char> vec1u_px(static_cast<std::size_t>(int4_w) * int4_h * 3);
  gl_read_buffer(GL_BACK);
  gl_read_pixels(0, 0, int4_w, int4_h, GL_RGB, GL_UNSIGNED_BYTE, vec1u_px.data());
  probe_check(gl_get_error() == 0, "glGetError 非零");

  SDL_Surface* surf_shot{SDL_CreateRGBSurfaceWithFormat(0, int4_w, int4_h, 24,
      SDL_PIXELFORMAT_RGB24)};
  auto* p1u_dst{static_cast<unsigned char*>(surf_shot->pixels)};
  const int int4_pitch{surf_shot->pitch};
  for (int int4_y{0}; int4_y < int4_h; ++int4_y)
    for (int int4_x{0}; int4_x < int4_w * 3; ++int4_x)
      p1u_dst[int4_y * int4_pitch + int4_x] =
          vec1u_px[static_cast<std::size_t>(int4_h - 1 - int4_y) * int4_w * 3
              + static_cast<std::size_t>(int4_x)];
  const int int4_saved{SDL_SaveBMP(surf_shot, str_out.c_str())};
  SDL_FreeSurface(surf_shot);
  probe_check(int4_saved == 0, std::format("SDL_SaveBMP 失败：{}", SDL_GetError()));
  std::println("render_probe: screenshot {} ({}x{}, {} frames, {} missing tiles)", str_out,
      int4_w, int4_h, int4_frames, sz_missing_tiles);

  probe_check(int4_w == int4_view_w && int4_h == int4_view_h,
      std::format("drawable {}x{} != {}x{}", int4_w, int4_h, int4_view_w, int4_view_h));

  const auto fn_expected_terrain{[&](int int4_i, int int4_j, int int4_tx, int int4_ty) {
    const ora::game::TerrainTile tile{res_mapbin->tile_at(int4_i, int4_j)};
    const auto it_rects{map_template_rects.find(tile.u2_type)};
    const SpriteRect& rect{it_rects->second[tile.u1_index]};
    const Bgra bgra_c{vec_bgra_atlas[static_cast<std::size_t>(rect.int4_y + int4_ty) * kAtlas
        + static_cast<std::size_t>(rect.int4_x + int4_tx)]};
    return bgra_c;
  }};

  const std::vector<std::pair<int, int>> vec_anchor_tiles{{2, 3}, {40, 20}, {60, 55}};
  for (const auto& [int4_i, int4_j] : vec_anchor_tiles) {
    if ((int4_i + 1) * kTile > int4_w || (int4_j + 1) * kTile > int4_h) continue;
    const Bgra bgra_expect{fn_expected_terrain(int4_i, int4_j, 12, 12)};
    if (bgra_expect.u1_a != 255) continue;
    const std::size_t sz_off{static_cast<std::size_t>(int4_h - 1 - (int4_j * kTile + 12))
            * static_cast<std::size_t>(int4_w) * 3
        + static_cast<std::size_t>(int4_i * kTile + 12) * 3};
    probe_check(vec1u_px[sz_off] == bgra_expect.u1_r && vec1u_px[sz_off + 1] == bgra_expect.u1_g
            && vec1u_px[sz_off + 2] == bgra_expect.u1_b,
        std::format("地形锚点 ({},{}): got {} {} {} want {} {} {}", int4_i, int4_j,
            vec1u_px[sz_off], vec1u_px[sz_off + 1], vec1u_px[sz_off + 2], bgra_expect.u1_r,
            bgra_expect.u1_g, bgra_expect.u1_b));
  }

  {
    const SpriteRect& rect{vec_tnk_rects[0]};
    const ogr::ShpTdFrame& frm{res_tnk->vec_frames[0]};
    std::optional<std::pair<int, int>> opt_hit{};
    for (int int4_y{0}; int4_y < rect.int4_h && !opt_hit.has_value(); ++int4_y)
      for (int int4_x{0}; int4_x < rect.int4_w; ++int4_x)
        if (frm.vec1u_data[static_cast<std::size_t>(int4_y) * rect.int4_w + int4_x] == 85) {
          opt_hit = std::pair{int4_x, int4_y};
          break;
        }
    probe_check(opt_hit.has_value(), "1tnk frame0 找不到索引 85 像素");
    if (opt_hit.has_value()) {
      const int int4_sx{vec_tnk_screen[0].first + opt_hit->first};
      const int int4_sy{vec_tnk_screen[0].second + opt_hit->second};
      const std::uint32_t u4_c{pal_player.arr4u_colors[85]};
      const unsigned char u1_b{static_cast<unsigned char>(u4_c & 0xFF)};
      const unsigned char u1_g{static_cast<unsigned char>((u4_c >> 8) & 0xFF)};
      const unsigned char u1_r{static_cast<unsigned char>((u4_c >> 16) & 0xFF)};
      const std::size_t sz_off{static_cast<std::size_t>(int4_h - 1 - int4_sy)
              * static_cast<std::size_t>(int4_w) * 3
          + static_cast<std::size_t>(int4_sx) * 3};
      probe_check(vec1u_px[sz_off] == u1_r && vec1u_px[sz_off + 1] == u1_g
              && vec1u_px[sz_off + 2] == u1_b,
          std::format("1tnk 锚点: got {} {} {} want {} {} {}", vec1u_px[sz_off],
              vec1u_px[sz_off + 1], vec1u_px[sz_off + 2], u1_r, u1_g, u1_b));
    }
  }

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (int4_failures != 0) {
    std::println("render_probe: {} checks failed", int4_failures);
    return 1;
  }
  std::println("render_probe: all checks passed");
  return 0;
}
