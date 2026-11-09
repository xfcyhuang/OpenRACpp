import std;

#include <SDL.h>
#include <cstdlib>
#include <cstring>

#include "game/render/font.hpp"
#include "game/ui/widget.hpp"
#include "game/ui/widget_render.hpp"

#ifndef ORA_FONTS_GOLDEN_DIR
#define ORA_FONTS_GOLDEN_DIR "tests/golden/fonts"
#endif

namespace {

namespace fr = ora::game::render;
namespace ui = ora::game::ui;

constexpr int kSkip{77};
constexpr int kWinW{256};
constexpr int kWinH{128};

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
static int (*gl_get_uniform_location)(unsigned, const char*);
static void (*gl_uniform4f)(int, GLfloat, GLfloat, GLfloat, GLfloat);
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

int int4_failures{0};

void probe_check(bool b_ok, std::string_view str_what) {
  if (!b_ok) {
    ++int4_failures;
    std::println("ui_probe: {}", str_what);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  static_cast<void>(argc);
  static_cast<void>(argv);
  if (const char* sz_headless{std::getenv("ORA_UI_HEADLESS")}; sz_headless != nullptr && *sz_headless != '\0') {
    std::println("ui_probe: SKIP（ORA_UI_HEADLESS 置位）");
    return kSkip;
  }

  std::ifstream fs_ttf{std::string{ORA_FONTS_GOLDEN_DIR} + "/fixture/FreeSans.ttf", std::ios::binary};
  if (!fs_ttf.is_open()) {
    std::println("ui_probe: SKIP（golden 字体 fixture 缺失）");
    return kSkip;
  }
  const std::string str_ttf{std::istreambuf_iterator<char>{fs_ttf}, std::istreambuf_iterator<char>{}};

  auto res_face{fr::font_face_load({reinterpret_cast<const std::uint8_t*>(str_ttf.data()),
      str_ttf.size()})};
  if (!res_face.has_value()) {
    std::println("ui_probe: FAIL（字体装载失败：{}）", res_face.error());
    return 1;
  }
  const std::shared_ptr<const fr::FontFace> ptr_face{
      std::make_shared<fr::FontFace>(std::move(*res_face))};
  fr::SheetBuilder builder{};
  builder.int4_sheet_size = 512;
  builder.int4_margin = 1;
  auto res_font{fr::spritefont_create(ptr_face, builder, 14, 11, 1.0f)};
  if (!res_font.has_value()) {
    std::println("ui_probe: FAIL（spritefont 创建失败）");
    return 1;
  }
  std::map<std::string, fr::SpriteFont> map_fonts;
  map_fonts.emplace("FreeSans", std::move(*res_font));

  ui::WidgetModel root{};
  root.str_type = "Container";
  root.str_x = "0";
  root.str_y = "0";
  root.str_width = "256";
  root.str_height = "128";
  ui::WidgetInitCtx wctx{256, 128, nullptr, "default"};
  ui::widget_initialize(root, wctx);

  auto ptr_label{std::make_unique<ui::WidgetModel>()};
  ptr_label->str_type = "Label";
  ptr_label->str_x = "28";
  ptr_label->str_y = "50";
  ptr_label->str_width = "200";
  ptr_label->str_height = "28";
  ptr_label->vec_fields.emplace_back("Text", std::string{"Hello World"});
  ptr_label->vec_fields.emplace_back("Font", std::string{"FreeSans"});
  ptr_label->vec_fields.emplace_back("Align", std::string{"Center"});
  ptr_label->vec_fields.emplace_back("VAlign", std::string{"Middle"});
  ptr_label->ptr_parent = &root;
  root.vec_children.push_back(std::move(ptr_label));
  ui::widget_initialize(*root.vec_children.back(), wctx);

  ui::UiRenderCtx rctx{&map_fonts, "FreeSans", 0xFFFFFFFFu};
  const std::vector<ui::UiQuad> vec_quads{ui::ui_draw_tree(root, rctx)};
  if (vec_quads.size() != 10) {
    std::println("ui_probe: FAIL（label quad 数 {} != 10）", vec_quads.size());
    return 1;
  }
  const std::uint32_t u4_sheet_size{static_cast<std::uint32_t>(builder.int4_sheet_size)};
  const std::uint32_t u4_sheet{vec_quads[0].u4_sheet};
  if (u4_sheet >= builder.vec_vec1u_sheets.size()) {
    std::println("ui_probe: FAIL（sheet 索引越界）");
    return 1;
  }
  const std::vector<std::uint8_t>& vec1u_sheet{builder.vec_vec1u_sheets[u4_sheet]};

  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::println("ui_probe: FAIL（SDL_Init 失败：{}）", SDL_GetError());
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_Window* window{SDL_CreateWindow("ora_ui_probe (label)",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWinW, kWinH,
      SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI)};
  if (window == nullptr) {
    std::println("ui_probe: SKIP（窗口创建失败：{}）", SDL_GetError());
    SDL_Quit();
    return kSkip;
  }
  SDL_GLContext ctx_gl{SDL_GL_CreateContext(window)};
  if (ctx_gl == nullptr) {
    std::println("ui_probe: SKIP（GL context 创建失败：{}）", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return kSkip;
  }
  SDL_GL_SetSwapInterval(0);

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
  gl_get_uniform_location = gl_load<int (*)(unsigned, const char*)>("glGetUniformLocation");
  gl_uniform4f = gl_load<void (*)(int, GLfloat, GLfloat, GLfloat, GLfloat)>("glUniform4f");
  gl_enable_vertex_attrib_array = gl_load<void (*)(unsigned)>("glEnableVertexAttribArray");
  gl_vertex_attrib_pointer = gl_load<void (*)(unsigned, GLint, GLenum, GLboolean, GLsizei, const void*)>("glVertexAttribPointer");
  gl_draw_elements = gl_load<void (*)(GLenum, GLsizei, GLenum, const void*)>("glDrawElements");
  gl_gen_vertex_arrays = gl_load<void (*)(GLsizei, unsigned*)>("glGenVertexArrays");
  gl_bind_vertex_array = gl_load<void (*)(unsigned)>("glBindVertexArray");
  gl_read_buffer = gl_load<void (*)(GLenum)>("glReadBuffer");
  gl_read_pixels = gl_load<void (*)(int, int, GLsizei, GLsizei, GLenum, GLenum, void*)>("glReadPixels");

  std::vector<std::uint8_t> vec1u_rgba(vec1u_sheet.size() * 4);
  for (std::size_t sz_i{0}; sz_i < vec1u_sheet.size(); ++sz_i) {
    const std::uint8_t u1_a{vec1u_sheet[sz_i]};
    vec1u_rgba[sz_i * 4] = u1_a;
    vec1u_rgba[sz_i * 4 + 1] = u1_a;
    vec1u_rgba[sz_i * 4 + 2] = u1_a;
    vec1u_rgba[sz_i * 4 + 3] = u1_a;
  }

  unsigned tex_sheet{};
  gl_gen_textures(1, &tex_sheet);
  gl_bind_texture(GL_TEXTURE_2D, tex_sheet);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(u4_sheet_size),
      static_cast<GLsizei>(u4_sheet_size), 0, GL_BGRA, GL_UNSIGNED_BYTE, vec1u_rgba.data());

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
      "uniform vec4 uTint;\n"
      "void main() { vec4 c = texture(uTex, vUv); fragColor = vec4(c.rgb * uTint.rgb, c.a * uTint.a); }\n"};
  const auto fn_compile{[&](GLenum en_type, const char* sz_src) -> unsigned {
    const unsigned u1_shader{gl_create_shader(en_type)};
    const char* sz_srcs[]{sz_src};
    int int4_len{static_cast<int>(std::strlen(sz_src))};
    gl_shader_source(u1_shader, 1, sz_srcs, &int4_len);
    gl_compile_shader(u1_shader);
    int int4_ok{0};
    gl_get_shader_iv(u1_shader, GL_COMPILE_STATUS, &int4_ok);
    probe_check(int4_ok != 0, "shader 编译失败");
    return u1_shader;
  }};
  const unsigned u1_prog{static_cast<unsigned>(gl_create_program())};
  gl_attach_shader(u1_prog, fn_compile(GL_VERTEX_SHADER, sz_vs));
  gl_attach_shader(u1_prog, fn_compile(GL_FRAGMENT_SHADER, sz_fs));
  gl_link_program(u1_prog);
  gl_use_program(u1_prog);
  const int int4_tint{gl_get_uniform_location(u1_prog, "uTint")};
  gl_uniform4f(int4_tint, 1.0f, 1.0f, 1.0f, 1.0f);

  struct Vtx {
    float fp4_x{};
    float fp4_y{};
    float fp4_u{};
    float fp4_v{};
  };
  std::vector<Vtx> vec_verts;
  std::vector<unsigned short> vec_idx;
  const float fp4_w{static_cast<float>(kWinW)};
  const float fp4_h{static_cast<float>(kWinH)};
  for (const ui::UiQuad& quad : vec_quads) {
    const float fp4_x0{static_cast<float>(quad.int4_x) / fp4_w * 2.0f - 1.0f};
    const float fp4_y0{1.0f - static_cast<float>(quad.int4_y) / fp4_h * 2.0f};
    const float fp4_x1{static_cast<float>(quad.int4_x + quad.int4_width) / fp4_w * 2.0f - 1.0f};
    const float fp4_y1{1.0f - static_cast<float>(quad.int4_y + quad.int4_height) / fp4_h * 2.0f};
    const float fp4_u0{static_cast<float>(quad.int4_src_x) / static_cast<float>(u4_sheet_size)};
    const float fp4_v0{static_cast<float>(quad.int4_src_y) / static_cast<float>(u4_sheet_size)};
    const float fp4_u1{static_cast<float>(quad.int4_src_x + quad.int4_width)
        / static_cast<float>(u4_sheet_size)};
    const float fp4_v1{static_cast<float>(quad.int4_src_y + quad.int4_height)
        / static_cast<float>(u4_sheet_size)};
    vec_verts.push_back(Vtx{fp4_x0, fp4_y0, fp4_u0, fp4_v0});
    vec_verts.push_back(Vtx{fp4_x1, fp4_y0, fp4_u1, fp4_v0});
    vec_verts.push_back(Vtx{fp4_x1, fp4_y1, fp4_u1, fp4_v1});
    vec_verts.push_back(Vtx{fp4_x0, fp4_y1, fp4_u0, fp4_v1});
    const unsigned short u2_b{static_cast<unsigned short>(vec_verts.size() - 4)};
    vec_idx.push_back(u2_b);
    vec_idx.push_back(static_cast<unsigned short>(u2_b + 1));
    vec_idx.push_back(static_cast<unsigned short>(u2_b + 2));
    vec_idx.push_back(u2_b);
    vec_idx.push_back(static_cast<unsigned short>(u2_b + 2));
    vec_idx.push_back(static_cast<unsigned short>(u2_b + 3));
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
  gl_buffer_data(GL_ELEMENT_ARRAY_BUFFER, static_cast<long>(vec_idx.size() * 2), vec_idx.data(),
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

  gl_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
  gl_clear(GL_COLOR_BUFFER_BIT);
  gl_draw_elements(GL_TRIANGLES, static_cast<GLsizei>(vec_idx.size()), GL_UNSIGNED_SHORT, nullptr);
  SDL_GL_SwapWindow(window);
  SDL_PumpEvents();
  SDL_Delay(50);

  gl_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
  gl_clear(GL_COLOR_BUFFER_BIT);
  gl_draw_elements(GL_TRIANGLES, static_cast<GLsizei>(vec_idx.size()), GL_UNSIGNED_SHORT, nullptr);

  int int4_draw_w{};
  int int4_draw_h{};
  SDL_GL_GetDrawableSize(window, &int4_draw_w, &int4_draw_h);
  std::vector<unsigned char> vec1u_px(static_cast<std::size_t>(int4_draw_w) * int4_draw_h * 3);
  gl_read_buffer(GL_BACK);
  gl_read_pixels(0, 0, int4_draw_w, int4_draw_h, GL_RGB, GL_UNSIGNED_BYTE, vec1u_px.data());
  probe_check(gl_get_error() == 0, "glGetError 非零");

  std::size_t sz_lit{0};
  int int4_lit_min_x{kWinW};
  int int4_lit_max_x{-1};
  int int4_lit_min_y{kWinH};
  int int4_lit_max_y{-1};
  for (int int4_y{0}; int4_y < int4_draw_h; ++int4_y)
    for (int int4_x{0}; int4_x < int4_draw_w; ++int4_x) {
      const std::size_t sz_o{static_cast<std::size_t>(int4_y) * int4_draw_w * 3
          + static_cast<std::size_t>(int4_x) * 3};
      if (vec1u_px[sz_o] > 96 || vec1u_px[sz_o + 1] > 96 || vec1u_px[sz_o + 2] > 96) {
        ++sz_lit;
        int4_lit_min_x = std::min(int4_lit_min_x, int4_x);
        int4_lit_max_x = std::max(int4_lit_max_x, int4_x);
        int4_lit_min_y = std::min(int4_lit_min_y, int4_y);
        int4_lit_max_y = std::max(int4_lit_max_y, int4_y);
      }
    }

  probe_check(sz_lit > 150, "label 文字像素存在");
  probe_check(sz_lit < 4000, "label 像素不泛滥");
  if (int4_lit_min_x <= int4_lit_max_x) {
    probe_check(int4_lit_min_x >= 28 && int4_lit_max_x < 228, "文字 x 落在 label bounds 内");
    probe_check(int4_lit_min_y >= 50 && int4_lit_max_y < 78, "文字 y 落在 label bounds 内");
    probe_check(int4_lit_max_x - int4_lit_min_x > 40, "文字横向跨度合理");
    probe_check(int4_lit_max_y - int4_lit_min_y >= 8, "文字纵向跨度合理");
  }

  const bool b_corner_dark{vec1u_px[0] == 0 && vec1u_px[1] == 0 && vec1u_px[2] == 0};
  probe_check(b_corner_dark, "label 外背景保持黑色");

  SDL_GL_DeleteContext(ctx_gl);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (int4_failures != 0) {
    std::println("ui_probe: {} checks failed", int4_failures);
    return 1;
  }
  std::println("ui_probe: label 渲染像素面通过（lit={} bbox=({}..{},{}..{})）", sz_lit,
      int4_lit_min_x, int4_lit_max_x, int4_lit_min_y, int4_lit_max_y);
  return 0;
}
