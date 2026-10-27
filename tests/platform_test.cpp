import std;
#include <cstdio>

#include "gfx/gfx_command.hpp"
#include "meta/dump_format.hpp"
#include "platform/al_loader.hpp"
#include "platform/dummy_sound_engine.hpp"
#include "platform/freetype_font.hpp"
#include "platform/openal_sound_engine.hpp"
#include "platform/sdl2_input.hpp"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define ORA_HAS_DESKTOP_GL 1
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "gfx/render_thread.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"
#include "platform/sdl2_hardware_cursor.hpp"
#include "platform/sdl2_window.hpp"
#endif

namespace {

std::int32_t int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::println(stderr, "FAIL {}:{} {}", __FILE__, __LINE__, #cond);       \
      ++int4_failures;                                                         \
    }                                                                          \
  } while (0)

const ora::gfx::GfxCmd* ManualFetch(ora::gfx::GfxCommandQueue& queue) {
  std::uint32_t uint4_record = 0;
  const ora::gfx::GfxCmd* ptr_cmd = queue.FetchNext(uint4_record);
  if (ptr_cmd != nullptr)
    queue.Skip(uint4_record);
  return ptr_cmd;
}

void TestSpscSingleThread() {

  ora::gfx::GfxCommandQueue queue_test(256);

  for (std::uint32_t i = 0; i < 200; ++i) {
    auto* ptr_cmd = queue_test.Reserve(ora::gfx::GfxCmdKind::Clear, 16);
    ORA_CHECK(ptr_cmd != nullptr);
    ptr_cmd->float_a = static_cast<float>(i);
    std::byte bytes_payload[16]{};
    queue_test.Commit(bytes_payload);

    const auto* ptr_cmd_read = ManualFetch(queue_test);
    ORA_CHECK(ptr_cmd_read != nullptr);
    ORA_CHECK(ptr_cmd_read->kind == ora::gfx::GfxCmdKind::Clear);
    ORA_CHECK(ptr_cmd_read->float_a == static_cast<float>(i));
  }
  ORA_CHECK(queue_test.size_consumed() == queue_test.size_published());
  queue_test.Stop();
  ORA_CHECK(ManualFetch(queue_test) == nullptr);
}

void TestSpscTwoThreads() {
  constexpr std::uint32_t kCount = 200000;
  ora::gfx::GfxCommandQueue queue_test(1024);

  std::jthread thread_consumer([&] {
    std::uint32_t uint4_expected = 0;
    while (uint4_expected != kCount) {
      std::uint32_t uint4_record = 0;
      const ora::gfx::GfxCmd* ptr_cmd = queue_test.FetchNext(uint4_record);
      if (ptr_cmd == nullptr)
        break;
      if (ptr_cmd->kind == ora::gfx::GfxCmdKind::Shutdown)
        break;
      const bool b_ok = ptr_cmd->kind == ora::gfx::GfxCmdKind::Clear &&
                        ptr_cmd->uint4_a == uint4_expected;
      if (!b_ok)
        std::println(stderr, "序号错位 | out-of-sequence: got {} expect {}",
                     ptr_cmd->uint4_a, uint4_expected);
      queue_test.Skip(uint4_record);
      if (!b_ok) {
        ++int4_failures;
        break;
      }
      ++uint4_expected;
    }
    if (uint4_expected != kCount) {
      std::println(stderr, "消费不足 | under-consumed: {} / {}", uint4_expected, kCount);
      ++int4_failures;
    }
  });

  for (std::uint32_t i = 0; i < kCount; ++i) {
    auto* ptr_cmd = queue_test.Reserve(ora::gfx::GfxCmdKind::Clear, 0);
    ORA_CHECK(ptr_cmd != nullptr);
    ptr_cmd->uint4_a = i;
    queue_test.CommitBare();
  }

  if (auto* ptr_cmd = queue_test.Reserve(ora::gfx::GfxCmdKind::Shutdown, 0); ptr_cmd != nullptr)
    queue_test.CommitBare();
  thread_consumer.join();
}

#ifdef ORA_HAS_DESKTOP_GL

void TestGlIntegration(int& int4_argc, char** argv_ptr) {

  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;

  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 320, .int4_height = 240});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(无桌面/GPU 环境)| SKIP: no window (headless/GPU-less host)");
    return;
  }
  auto& window = *window_opt;

  const auto geom = window.Geom();
  ORA_CHECK(geom.int4_width == 320);
  ORA_CHECK(geom.int4_height == 240);
  ORA_CHECK(geom.float_scale >= 1.0f);

  ora::gfx::RenderThread render(window);
  render.FlushAndWait();
  if (render.b_thread_failed()) {
    std::println("SKIP: 渲染线程 GL 初始化失败(无 GPU/驱动环境)| SKIP: render-thread GL init failed");
    return;
  }

  constexpr std::int32_t kW = 333, kH = 257;
  std::uint32_t uint4_tex = 0, uint4_fbo = 0;
  render.GenNames(ora::gfx::GfxCmdKind::GenTextures, 1, &uint4_tex);
  render.GenNames(ora::gfx::GfxCmdKind::GenFramebuffers, 1, &uint4_fbo);
  ORA_CHECK(uint4_tex != 0 && uint4_fbo != 0);

  std::vector<std::uint8_t> vec_pixels_init(kW * kH * 4, 0);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindTexture, 0);
    ptr_cmd->uint4_a = 0;
    ptr_cmd->uint4_b = 0x0DE1;
    ptr_cmd->uint4_c = uint4_tex;
    render.queue().CommitBare();
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::TexImage2D, 0);
    ptr_cmd->uint4_a = 0x0DE1;
    ptr_cmd->uint4_b = kW;
    ptr_cmd->uint4_c = kH;
    ptr_cmd->uint4_d = 0x8058;
    render.queue().Commit(vec_pixels_init.data());
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::FramebufferTexture2D, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    ptr_cmd->uint4_b = 0x8CE0;
    ptr_cmd->uint4_c = uint4_tex;
    render.queue().CommitBare();
  }
  render.FlushAndWait();
  ORA_CHECK(render.QueryFboStatus() == 0x8CD5);

  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindFramebuffer, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    render.queue().CommitBare();
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::SetViewport, 0);
    ptr_cmd->uint4_a = 0;
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = kW;
    ptr_cmd->uint4_d = kH;
    render.queue().CommitBare();
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Clear, 0);
    ptr_cmd->float_a = 0.0f;
    ptr_cmd->float_b = 1.0f;
    ptr_cmd->float_c = 0.0f;
    ptr_cmd->float_d = 1.0f;
    render.queue().CommitBare();
  }
  render.FlushAndWait();

  std::vector<std::uint8_t> vec_pixels(kW * kH * 4, 0);
  render.ReadPixels(0, 0, kW, kH, vec_pixels.data());
  ORA_CHECK(vec_pixels[0] == 0 && vec_pixels[1] == 255 && vec_pixels[2] == 0 && vec_pixels[3] == 255);

  const std::uint32_t uint4_program = render.CreateProgramFromSources(
      "#version 150\n"
      "in vec2 aPos;\n"
      "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n",
      "#version 150\n"
      "uniform vec4 uColor;\n"
      "void main() { gl_FragColor = uColor; }\n");
  ORA_CHECK(uint4_program != 0);

  std::uint32_t uint4_ids[2] = {};
  render.GenNames(ora::gfx::GfxCmdKind::GenBuffers, 1, uint4_ids);
  render.GenNames(ora::gfx::GfxCmdKind::GenVertexArrays, 1, uint4_ids + 1);
  const std::uint32_t uint4_vbo = uint4_ids[0], uint4_vao = uint4_ids[1];
  ORA_CHECK(uint4_vbo != 0 && uint4_vao != 0);

  const float vec_verts[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindVertexArray, 0);
    ptr_cmd->uint4_a = uint4_vao;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindBuffer, 0);
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = uint4_vbo;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BufferData, sizeof(vec_verts));
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = sizeof(vec_verts);
    ptr_cmd->uint4_c = 0x88E4;
    render.queue().Commit(vec_verts);
  }
  {

    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::VertexAttribPointer, 0);
    ptr_cmd->uint4_a = 0;
    ptr_cmd->uint4_b = 2;
    ptr_cmd->uint4_c = 0x1406;
    ptr_cmd->b_depth = 0;
    ptr_cmd->uint4_d = 16;
    ptr_cmd->float_a = 0.0f;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::EnableVertexAttribArray, 0);
    ptr_cmd->uint4_a = 0;
    render.queue().CommitBare();
  }

  std::uint32_t uint4_location = 0;
  {
    ora::gfx::GfxScalarRequest request;
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::GetUniformLocation,
                                           sizeof("uColor"));
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->ptr_sync = &request;
    char str_name[8];
    std::memcpy(str_name, "uColor", sizeof("uColor"));
    render.queue().Commit(str_name);
    request.sync.sem_done.acquire();
    uint4_location = request.uint4_value;
  }
  ORA_CHECK(static_cast<std::int32_t>(uint4_location) >= 0);

  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindProgram, 0);
    ptr_cmd->uint4_a = uint4_program;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Uniform4fv, 16);
    ptr_cmd->uint4_a = uint4_location;
    ptr_cmd->uint4_b = 1;
    const float vec_red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    render.queue().Commit(vec_red);
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::DrawArrays, 0);
    ptr_cmd->uint4_a = 0x0004;
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = 3;
    render.queue().CommitBare();
  }
  render.FlushAndWait();

  render.ReadPixels(0, 0, kW, kH, vec_pixels.data());

  const auto PixelAt = [&](std::int32_t x, std::int32_t y) {
    const std::size_t i = (static_cast<std::size_t>(y) * kW + x) * 4;
    return std::pair{vec_pixels[i], vec_pixels[i + 1]};
  };

  auto [r1, g1] = PixelAt(4, 4);
  ORA_CHECK(r1 == 255 && g1 == 0);
  auto [r2, g2] = PixelAt(kW - 5, kH - 5);
  ORA_CHECK(r2 == 0 && g2 == 255);
  auto [r3, g3] = PixelAt(kW - 5, kH / 2);

  ORA_CHECK(r3 == 0 && g3 == 255);

  (void)int4_argc;
  (void)argv_ptr;
}

#endif

void TestTapHistorySequences() {

  ora::TapHistory history(1000);
  ORA_CHECK(history.GetTapCount(ora::int2{5, 5}, 1100) == 1);
  ORA_CHECK(history.GetTapCount(ora::int2{5, 6}, 1150) == 2);
  ORA_CHECK(history.GetTapCount(ora::int2{8, 5}, 1200) == 3);
  ORA_CHECK(history.LastTapCount() == 3);
  ORA_CHECK(history.GetTapCount(ora::int2{8, 5}, 1500) == 1);
  ORA_CHECK(history.GetTapCount(ora::int2{20, 5}, 1550) == 1);
  ORA_CHECK(history.GetTapCount(ora::int2{20, 5}, 1560) == 2);

  ora::TapHistory history_dist(0);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{0, 0}, 0) == 1);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{3, 0}, 10) == 2);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{7, 0}, 20) == 1);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{9, 0}, 30) == 2);
}

void TestMultiTapDetectionCaches() {

  ora::MultiTapDetection detect;
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::None, 0) == 1);
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::None, 100) == 2);
  ORA_CHECK(detect.InfoFromKeyboard(ora::Keycode::A, ora::Modifiers::None) == 2);
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::Shift, 100) == 1);
  ORA_CHECK(detect.InfoFromKeyboard(ora::Keycode::A, ora::Modifiers::Shift) == 1);

  ORA_CHECK(detect.DetectFromMouse(1, ora::int2{0, 0}, 0) == 1);
  ORA_CHECK(detect.DetectFromMouse(1, ora::int2{1, 1}, 10) == 2);
  ORA_CHECK(detect.InfoFromMouse(1) == 2);
  ORA_CHECK(detect.InfoFromMouse(3) == 1);
}

void TestMakeButtonAndModifiers() {

  ORA_CHECK(ora::Sdl2Input::MakeButton(1) == ora::MouseButton::Left);
  ORA_CHECK(ora::Sdl2Input::MakeButton(2) == ora::MouseButton::Middle);
  ORA_CHECK(ora::Sdl2Input::MakeButton(3) == ora::MouseButton::Right);
  ORA_CHECK(ora::Sdl2Input::MakeButton(8) == ora::MouseButton::None);
  ORA_CHECK(ora::Sdl2Input::MakeButton(0) == ora::MouseButton::None);

  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0000) == ora::Modifiers::None);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0001) == ora::Modifiers::Shift);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0002) == ora::Modifiers::Shift);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0040) == ora::Modifiers::Ctrl);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0100) == ora::Modifiers::Alt);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0400) == ora::Modifiers::Meta);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0800) == ora::Modifiers::Meta);

  const auto mods_combo = ora::Sdl2Input::MakeModifiers(0x0400 | 0x0001 | 0x0100);
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Meta));
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Shift));
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Alt));
  ORA_CHECK(!ora::HasModifier(mods_combo, ora::Modifiers::Ctrl));
}

void TestScaleAwayFromZero() {

  using ora::Sdl2Input;
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(0, 0.5f) == 0);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(1, 0.5f) == 1);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(2, 0.5f) == 1);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(3, 0.5f) == 2);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-1, 0.5f) == -1);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-2, 0.5f) == -1);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-3, 0.5f) == -2);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(7, 0.25f) == 2);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-7, 0.25f) == -2);

  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(41, 1.0f) == 41);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-41, 1.0f) == -41);
}

#ifdef ORA_HAS_DESKTOP_GL

void TestKeycodeAgainstSdl() {
  const auto AsInt = [](ora::Keycode key) { return static_cast<std::int32_t>(key); };

  ORA_CHECK(AsInt(ora::Keycode::RETURN) == SDLK_RETURN);
  ORA_CHECK(AsInt(ora::Keycode::ESCAPE) == SDLK_ESCAPE);
  ORA_CHECK(AsInt(ora::Keycode::BACKSPACE) == SDLK_BACKSPACE);
  ORA_CHECK(AsInt(ora::Keycode::TAB) == SDLK_TAB);
  ORA_CHECK(AsInt(ora::Keycode::SPACE) == SDLK_SPACE);
  ORA_CHECK(AsInt(ora::Keycode::EXCLAIM) == SDLK_EXCLAIM);
  ORA_CHECK(AsInt(ora::Keycode::NUMBER_0) == SDLK_0);
  ORA_CHECK(AsInt(ora::Keycode::NUMBER_9) == SDLK_9);
  ORA_CHECK(AsInt(ora::Keycode::A) == SDLK_a);
  ORA_CHECK(AsInt(ora::Keycode::Z) == SDLK_z);
  ORA_CHECK(AsInt(ora::Keycode::DELETE) == SDLK_DELETE);

  ORA_CHECK(AsInt(ora::Keycode::CAPSLOCK) == SDLK_CAPSLOCK);
  ORA_CHECK(AsInt(ora::Keycode::F1) == SDLK_F1);
  ORA_CHECK(AsInt(ora::Keycode::F12) == SDLK_F12);
  ORA_CHECK(AsInt(ora::Keycode::PRINTSCREEN) == SDLK_PRINTSCREEN);
  ORA_CHECK(AsInt(ora::Keycode::INSERT) == SDLK_INSERT);
  ORA_CHECK(AsInt(ora::Keycode::HOME) == SDLK_HOME);
  ORA_CHECK(AsInt(ora::Keycode::PAGEUP) == SDLK_PAGEUP);
  ORA_CHECK(AsInt(ora::Keycode::END) == SDLK_END);
  ORA_CHECK(AsInt(ora::Keycode::PAGEDOWN) == SDLK_PAGEDOWN);
  ORA_CHECK(AsInt(ora::Keycode::RIGHT) == SDLK_RIGHT);
  ORA_CHECK(AsInt(ora::Keycode::LEFT) == SDLK_LEFT);
  ORA_CHECK(AsInt(ora::Keycode::DOWN) == SDLK_DOWN);
  ORA_CHECK(AsInt(ora::Keycode::UP) == SDLK_UP);

  ORA_CHECK(AsInt(ora::Keycode::NUMLOCKCLEAR) == SDLK_NUMLOCKCLEAR);
  ORA_CHECK(AsInt(ora::Keycode::KP_DIVIDE) == SDLK_KP_DIVIDE);
  ORA_CHECK(AsInt(ora::Keycode::KP_ENTER) == SDLK_KP_ENTER);
  ORA_CHECK(AsInt(ora::Keycode::KP_0) == SDLK_KP_0);
  ORA_CHECK(AsInt(ora::Keycode::KP_9) == SDLK_KP_9);
  ORA_CHECK(AsInt(ora::Keycode::KP_PERIOD) == SDLK_KP_PERIOD);

  ORA_CHECK(AsInt(ora::Keycode::LCTRL) == SDLK_LCTRL);
  ORA_CHECK(AsInt(ora::Keycode::RCTRL) == SDLK_RCTRL);
  ORA_CHECK(AsInt(ora::Keycode::LSHIFT) == SDLK_LSHIFT);
  ORA_CHECK(AsInt(ora::Keycode::RSHIFT) == SDLK_RSHIFT);
  ORA_CHECK(AsInt(ora::Keycode::LALT) == SDLK_LALT);
  ORA_CHECK(AsInt(ora::Keycode::RALT) == SDLK_RALT);
  ORA_CHECK(AsInt(ora::Keycode::LGUI) == SDLK_LGUI);
  ORA_CHECK(AsInt(ora::Keycode::RGUI) == SDLK_RGUI);

  ORA_CHECK(AsInt(ora::Keycode::MOUSE4) == (283 | (1 << 30)));
  ORA_CHECK(AsInt(ora::Keycode::MOUSE5) == (284 | (1 << 30)));

  ORA_CHECK(ora::ToUnicodeChar(ora::Keycode::A) == 'a');
}

class RecordingHandler final : public ora::IInputHandler {
 public:
  void ModifierKeys(ora::Modifiers mods) override {
    ++int4_mod_calls;
    mods_last = mods;
  }
  void OnKeyInput(const ora::KeyInput& input) override { vec_keys.push_back(input); }
  void OnMouseInput(const ora::MouseInput& input) override { vec_mice.push_back(input); }
  void OnTextInput(std::string_view str_text) override { vec_texts.emplace_back(str_text); }

  std::int32_t int4_mod_calls = 0;
  ora::Modifiers mods_last = ora::Modifiers::None;
  std::vector<ora::KeyInput> vec_keys;
  std::vector<ora::MouseInput> vec_mice;
  std::vector<std::string> vec_texts;
};

std::int64_t FakeNowMsZero() { return 0; }

void TestPumpInputSynthetic() {
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;
  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 128, .int4_height = 96});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(输入泵合成事件)| SKIP: no window (input pump)");
    return;
  }
  auto& window = *window_opt;

  SDL_Event event_drain;
  while (SDL_PollEvent(&event_drain) != 0) {}

  ora::Sdl2Input input_pump(window, &FakeNowMsZero);
  RecordingHandler handler;

  const auto PushKey = [](SDL_Keycode sym) {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_KEYDOWN;
    event_sdl.key.keysym.sym = sym;
    SDL_PushEvent(&event_sdl);
  };
  const auto PushMotion = [](std::int32_t x, std::int32_t y, std::int32_t xrel, std::int32_t yrel) {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_MOUSEMOTION;
    event_sdl.motion.x = x;
    event_sdl.motion.y = y;
    event_sdl.motion.xrel = xrel;
    event_sdl.motion.yrel = yrel;
    SDL_PushEvent(&event_sdl);
  };
  const auto PushButton = [](std::uint8_t button, std::int32_t x, std::int32_t y) {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_MOUSEBUTTONDOWN;
    event_sdl.button.button = button;
    event_sdl.button.x = x;
    event_sdl.button.y = y;
    SDL_PushEvent(&event_sdl);
  };

  PushKey(SDLK_a);
  PushKey(SDLK_a);
  PushMotion(10, 10, 10, 10);
  PushMotion(20, 20, 10, 10);
  PushMotion(30, 30, 10, 10);
  PushButton(SDL_BUTTON_LEFT, 100, 100);
  PushButton(SDL_BUTTON_LEFT, 100, 100);
  {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_MOUSEWHEEL;
    event_sdl.wheel.y = 1;
    SDL_PushEvent(&event_sdl);
  }
  {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_TEXTINPUT;
    std::memcpy(event_sdl.text.text, "hi", 3);
    SDL_PushEvent(&event_sdl);
  }
  {
    SDL_Event event_sdl{};
    event_sdl.type = SDL_QUIT;
    SDL_PushEvent(&event_sdl);
  }

  const bool b_quit = input_pump.PumpInput(handler);

  ORA_CHECK(handler.int4_mod_calls == 1);
  ORA_CHECK(b_quit);

  ORA_CHECK(handler.vec_keys.size() == 2);
  if (handler.vec_keys.size() == 2) {
    ORA_CHECK(handler.vec_keys[0].Event == ora::KeyInputEvent::Down);
    ORA_CHECK(handler.vec_keys[0].Key == ora::Keycode::A);
    ORA_CHECK(handler.vec_keys[0].MultiTapCount == 1);
    ORA_CHECK(handler.vec_keys[0].UnicodeChar == u'a');
    ORA_CHECK(handler.vec_keys[1].MultiTapCount == 2);
  }

  std::int32_t int4_moves = 0;
  for (const auto& mouse : handler.vec_mice)
    if (mouse.Event == ora::MouseInputEvent::Move)
      ++int4_moves;
  ORA_CHECK(int4_moves == 1);
  if (int4_moves == 1) {
    for (const auto& mouse : handler.vec_mice) {
      if (mouse.Event != ora::MouseInputEvent::Move)
        continue;
      ORA_CHECK(mouse.Location == ora::Sdl2Input::EventPosition(window, 30, 30));
      ORA_CHECK(mouse.Delta == ora::Sdl2Input::EventPosition(window, 10, 10));
    }
  }

  std::int32_t int4_left_downs = 0;
  for (const auto& mouse : handler.vec_mice) {
    if (mouse.Event == ora::MouseInputEvent::Down && mouse.Button == ora::MouseButton::Left) {
      ++int4_left_downs;
      ORA_CHECK(mouse.Location == ora::Sdl2Input::EventPosition(window, 100, 100));
      ORA_CHECK(mouse.MultiTapCount == int4_left_downs);
    }
  }
  ORA_CHECK(int4_left_downs == 2);

  bool b_scroll_seen = false;
  for (const auto& mouse : handler.vec_mice) {
    if (mouse.Event != ora::MouseInputEvent::Scroll)
      continue;
    b_scroll_seen = true;
    const ora::int2 vec_wheel_delta{0, 1};
    ORA_CHECK(mouse.Delta == vec_wheel_delta);
    ORA_CHECK(mouse.Button == ora::MouseButton::None);
  }
  ORA_CHECK(b_scroll_seen);

  ORA_CHECK(handler.vec_texts.size() == 1);
  ORA_CHECK(handler.vec_texts.size() == 1 && handler.vec_texts[0] == "hi");
}

void TestGlResourceWrappers() {
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;
  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 64, .int4_height = 48});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(封装集成)| SKIP: no window (wrapper integration)");
    return;
  }
  auto& window = *window_opt;
  ora::gfx::RenderThread render(window);
  render.FlushAndWait();
  if (render.b_thread_failed()) {
    std::println("SKIP: 渲染线程 GL 初始化失败(封装集成)| SKIP: render-thread GL init failed");
    return;
  }

  constexpr std::int32_t kW = 5, kH = 3;
  std::vector<std::byte> vec_bgra(std::size_t(4) * kW * kH);
  for (std::size_t i = 0; i < vec_bgra.size() / 4; ++i) {
    vec_bgra[i * 4 + 0] = static_cast<std::byte>(i * 7);
    vec_bgra[i * 4 + 1] = static_cast<std::byte>(i * 13 + 1);
    vec_bgra[i * 4 + 2] = static_cast<std::byte>(i * 29 + 2);
    vec_bgra[i * 4 + 3] = static_cast<std::byte>(0xFF);
  }
  ora::gfx::Texture texture(render);
  texture.SetData(vec_bgra, kW, kH);
  render.FlushAndWait();
  ORA_CHECK(texture.GlId() != 0);
  ORA_CHECK(texture.Width() == kW && texture.Height() == kH);
  ORA_CHECK(texture.GetData() == vec_bgra);

  std::vector<std::byte> vec_src(std::size_t(4) * kW * 2, static_cast<std::byte>(0xCD));
  for (const std::int32_t x : {1, 2})
    for (int c = 0; c < 4; ++c)
      vec_src[(static_cast<std::size_t>(kW) + x) * 4 + c] = static_cast<std::byte>(0xAB);
  texture.SetSubData(vec_src, 1, 1, 2, 1);
  render.FlushAndWait();
  const std::vector<std::byte> vec_after_patch = texture.GetData();
  ORA_CHECK(vec_after_patch != vec_bgra);
  bool b_patch_ok = true;
  for (std::int32_t y = 0; y < kH && b_patch_ok; ++y)
    for (std::int32_t x = 0; x < kW && b_patch_ok; ++x) {
      const std::size_t i = (static_cast<std::size_t>(y) * kW + x) * 4;
      const bool b_in_patch = y == 1 && (x == 1 || x == 2);
      for (int c = 0; c < 4; ++c) {
        const std::byte byte_expect = b_in_patch ? static_cast<std::byte>(0xAB) : vec_bgra[i + c];
        if (vec_after_patch[i + c] != byte_expect) {
          b_patch_ok = false;
          break;
        }
      }
    }
  ORA_CHECK(b_patch_ok);

  texture.SetScaleFilter(ora::gfx::TextureScaleFilter::Linear);
  texture.SetScaleFilter(ora::gfx::TextureScaleFilter::Linear);
  texture.SetData(vec_bgra, kW, kH);
  render.FlushAndWait();
  ORA_CHECK(texture.GetData() == vec_bgra);

  const ora::gfx::ShaderVertexAttribute vec_attributes[] = {
      ora::gfx::ShaderVertexAttribute{.str_name = "aPos",
                                      .kind_type = ora::gfx::ShaderVertexAttributeType::Float,
                                      .int4_components = 2,
                                      .int4_offset = 0},
  };
  const ora::gfx::ShaderBindingsDesc desc_bindings{
      .str_vertex_shader_name = "test.vert",
      .str_vertex_shader_code =
          "#version {VERSION}\n"
          "in vec2 aPos;\n"
          "out vec2 vUV;\n"
          "void main() { vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n",
      .str_fragment_shader_name = "test.frag",
      .str_fragment_shader_code =
          "#version {VERSION}\n"
          "in vec2 vUV;\n"
          "uniform vec4 uColor;\n"
          "uniform sampler2D uTex;\n"
          "out vec4 fragColor;\n"
          "void main() { fragColor = texture(uTex, vUV) * uColor; }\n",
      .int4_stride = 8,
      .vec_attributes = vec_attributes,
  };
  auto shader_opt = ora::gfx::Shader::Create(render, desc_bindings);
  ORA_CHECK(shader_opt.has_value());
  if (!shader_opt.has_value())
    return;
  auto& shader = *shader_opt;
  ORA_CHECK(shader.GlId() != 0);
  ORA_CHECK(shader.Location("uColor") >= 0);
  ORA_CHECK(shader.Location("uTex") >= 0);
  ORA_CHECK(shader.Location("nonexistent") == -1);

  ora::gfx::Texture texture_white(render);
  const std::byte vec_white[4] = {static_cast<std::byte>(255), static_cast<std::byte>(255),
                                  static_cast<std::byte>(255), static_cast<std::byte>(255)};
  texture_white.SetData(vec_white, 1, 1);
  shader.SetTexture("uTex", texture_white);

  constexpr std::int32_t kRW = 32, kRH = 24;
  ora::gfx::Texture texture_rt(render);
  texture_rt.SetEmpty(kRW, kRH);
  std::uint32_t uint4_fbo = 0;
  render.GenNames(ora::gfx::GfxCmdKind::GenFramebuffers, 1, &uint4_fbo);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::FramebufferTexture2D, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    ptr_cmd->uint4_b = 0x8CE0;
    ptr_cmd->uint4_c = texture_rt.GlId();
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindFramebuffer, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::SetViewport, 0);
    ptr_cmd->uint4_a = ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = kRW;
    ptr_cmd->uint4_d = kRH;
    render.queue().CommitBare();
  }
  render.FlushAndWait();
  ORA_CHECK(render.QueryFboStatus() == 0x8CD5);

  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Clear, 0);
    ptr_cmd->float_a = 0.0f;
    ptr_cmd->float_b = 1.0f;
    ptr_cmd->float_c = 0.0f;
    ptr_cmd->float_d = 1.0f;
    render.queue().CommitBare();
  }

  std::uint32_t uint4_ids[2] = {};
  render.GenNames(ora::gfx::GfxCmdKind::GenBuffers, 1, uint4_ids);
  render.GenNames(ora::gfx::GfxCmdKind::GenVertexArrays, 1, uint4_ids + 1);
  const float vec_verts[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindVertexArray, 0);
    ptr_cmd->uint4_a = uint4_ids[1];
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindBuffer, 0);
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = uint4_ids[0];
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BufferData, sizeof(vec_verts));
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = sizeof(vec_verts);
    ptr_cmd->uint4_c = 0x88E4;
    render.queue().Commit(vec_verts);
  }
  shader.Bind();
  shader.PrepareRender();
  const float vec_red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  shader.SetVec("uColor", vec_red, 4);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::DrawArrays, 0);
    ptr_cmd->uint4_a = 0x0004;
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = 3;
    render.queue().CommitBare();
  }
  render.FlushAndWait();

  std::vector<std::uint8_t> vec_pixels(std::size_t(4) * kRW * kRH, 0);
  render.ReadPixels(0, 0, kRW, kRH, vec_pixels.data());
  const auto PixelPairAt = [&](std::int32_t x, std::int32_t y) {
    const std::size_t i = (static_cast<std::size_t>(y) * kRW + x) * 4;
    return std::pair{vec_pixels[i], vec_pixels[i + 1]};
  };
  auto [r_in, g_in] = PixelPairAt(3, 3);
  ORA_CHECK(r_in == 255 && g_in == 0);
  auto [r_out, g_out] = PixelPairAt(kRW - 4, kRH - 4);
  ORA_CHECK(r_out == 0 && g_out == 255);

  {
    auto shader_scoped = std::move(shader);
    (void)shader_scoped;
  }
  render.FlushAndWait();
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Clear, 0);
    ptr_cmd->float_a = 0.0f;
    ptr_cmd->float_b = 0.0f;
    ptr_cmd->float_c = 1.0f;
    ptr_cmd->float_d = 1.0f;
    render.queue().CommitBare();
  }
  render.FlushAndWait();
  render.ReadPixels(0, 0, kRW, kRH, vec_pixels.data());
  ORA_CHECK(vec_pixels[0] == 0 && vec_pixels[1] == 0 && vec_pixels[2] == 255);
}

#endif

}

void TestMakeAlFormat() {
  using ora::platform::MakeAlFormat;
  ORA_CHECK(MakeAlFormat(1, 16) == AL_FORMAT_MONO16);
  ORA_CHECK(MakeAlFormat(1, 8) == AL_FORMAT_MONO8);
  ORA_CHECK(MakeAlFormat(2, 16) == AL_FORMAT_STEREO16);
  ORA_CHECK(MakeAlFormat(2, 8) == AL_FORMAT_STEREO8);
}

void TestParseAlDeviceList() {

  const std::array<char, 13> arr_two{'A', 'l', 'p', 'h', 'a', '\0', 'B', 'e', 't', 'a', '\0', '\0', '\0'};
  const std::vector<std::string> vec_two = ora::platform::ParseAlDeviceList(arr_two.data());
  ORA_CHECK(vec_two.empty());

  const std::array<char, 8> arr_one{'s', 'o', 'l', 'o', '\0', '\0', '\0', '\0'};
  const std::vector<std::string> vec_one = ora::platform::ParseAlDeviceList(arr_one.data());
  ORA_CHECK(vec_one.empty());

  const std::array<char, 4> arr_empty{'\0', '\0', '\0', '\0'};
  const std::vector<std::string> vec_empty = ora::platform::ParseAlDeviceList(arr_empty.data());
  ORA_CHECK(vec_empty.size() == 1);
  ORA_CHECK(vec_empty[0].empty());
}

void TestEvaluatePlayGate() {
  using ora::platform::EvaluatePlayGate;
  using ora::platform::GateProbeSlot;
  using ora::WPos;

  {
    const auto gate = EvaluatePlayGate({}, true, 7, WPos{}, 0);
    ORA_CHECK(!gate.b_deny);
    ORA_CHECK(gate.fp4_atten == 0.66f * ((256 - 0 * 0.5f) / 256));
  }

  {
    std::vector<GateProbeSlot> vec_slots(3);
    for (GateProbeSlot& slot : vec_slots) {
      slot.b_is_active = true;
      slot.b_is_relative = true;
      slot.uintp_source_id = 7;
      slot.int4_frame_started = 0;
      slot.pos = WPos{};
    }
    ORA_CHECK(EvaluatePlayGate(vec_slots, true, 7, WPos{}, 0).b_deny);

    ORA_CHECK(!EvaluatePlayGate(vec_slots, true, 8, WPos{}, 0).b_deny);
    ORA_CHECK(!EvaluatePlayGate(vec_slots, true, 7, WPos{}, 5).b_deny);
    ORA_CHECK(!EvaluatePlayGate(vec_slots, true, 7, WPos{2730, 0, 0}, 0).b_deny);

    ORA_CHECK(EvaluatePlayGate(vec_slots, true, 7, WPos{2729, 0, 0}, 0).b_deny);
  }

  {
    GateProbeSlot slot_other;
    slot_other.b_is_active = true;
    slot_other.b_is_relative = false;
    slot_other.uintp_source_id = 7;
    slot_other.pos = WPos{};
    const GateProbeSlot arr[] = {slot_other};
    const auto gate = EvaluatePlayGate(arr, true, 9, WPos{}, 0);
    ORA_CHECK(!gate.b_deny);
    ORA_CHECK(gate.fp4_atten == 0.66f);
  }

  {
    std::vector<GateProbeSlot> vec_slots(10);
    for (std::size_t int4_i = 0; int4_i < vec_slots.size(); int4_i++) {
      vec_slots[int4_i].b_is_active = true;
      vec_slots[int4_i].b_is_relative = true;
      vec_slots[int4_i].uintp_source_id = 100 + int4_i;
      vec_slots[int4_i].pos = WPos{};
    }
    const auto gate = EvaluatePlayGate(vec_slots, true, 42, WPos{}, 100);
    ORA_CHECK(!gate.b_deny);
    ORA_CHECK(gate.fp4_atten == 0.66f * ((256 - 10 * 0.5f) / 256));
  }
}

class VectorPcmStream final : public ora::platform::IPcmStream {
 public:
  explicit VectorPcmStream(std::vector<std::uint8_t> vec_data)
      : vec_data_{std::move(vec_data)} {}

  std::optional<std::size_t> Length() const override { return vec_data_.size(); }
  std::size_t Read(std::span<std::uint8_t> span_dst) override {
    const std::size_t int4_take = std::min(span_dst.size(), vec_data_.size() - int4_pos_);
    std::copy_n(vec_data_.data() + int4_pos_, int4_take, span_dst.data());
    int4_pos_ += int4_take;
    return int4_take;
  }

 private:
  std::vector<std::uint8_t> vec_data_;
  std::size_t int4_pos_ = 0;
};

void TestOpenAlEngineLive() {
  if (std::getenv("ORA_SKIP_AL") != nullptr)
    return;

  const ora::al::AlLoadResult load = ora::al::LoadAlDefault();
  if (!load.b_ok) {
    std::println("SKIP: OpenAL32.dll 不可用(缺 {}) | SKIP: OpenAL32.dll unavailable (missing {})",
                 load.str_missing, load.str_missing);
    return;
  }

  std::optional<ora::platform::OpenAlSoundEngine> opt_engine;
  try {
    opt_engine.emplace(std::string{"Null"});
  } catch (const std::exception& ex) {
    std::println("SKIP: OpenAL Null 设备打开失败({}) | SKIP: OpenAL Null device failed ({})",
                 ex.what(), ex.what());
    return;
  }

  auto& engine = *opt_engine;
  ORA_CHECK(!engine.Dummy());

  const std::vector<ora::platform::SoundDevice> vec_devices = engine.AvailableDevices();
  ORA_CHECK(!vec_devices.empty());
  ORA_CHECK(!vec_devices[0].str_device.has_value());
  ORA_CHECK(vec_devices[0].str_label == "Default Output");

  engine.SetVolume(0.5f);
  ORA_CHECK(engine.GetVolume() == 0.5f);

  std::vector<std::uint8_t> vec_pcm;
  for (int int4_i = 0; int4_i < 4410; int4_i++) {
    const double fp8_sample = std::sin(2.0 * 3.14159265358979323846 * 440.0 * int4_i / 44100.0) * 8000.0;
    const auto int4_s = static_cast<std::int16_t>(fp8_sample);
    vec_pcm.push_back(static_cast<std::uint8_t>(int4_s & 0xFF));
    vec_pcm.push_back(static_cast<std::uint8_t>((int4_s >> 8) & 0xFF));
  }

  auto* const ptr_source = engine.AddSoundSourceFromMemory(vec_pcm, 1, 16, 44100);
  ORA_CHECK(ptr_source != nullptr);

  {
    std::vector<std::unique_ptr<ora::platform::ISound>> vec_holding;
    for (int int4_i = 0; int4_i < 3; int4_i++) {
      auto up_play = engine.Play2D(ptr_source, true, true, ora::WPos::Zero(), 1.0f, true);
      ORA_CHECK(up_play != nullptr);
      vec_holding.push_back(std::move(up_play));
    }
    ORA_CHECK(engine.Play2D(ptr_source, false, true, ora::WPos::Zero(), 1.0f, true) == nullptr);
    engine.StopAllSounds();
  }

  auto up_sound = engine.Play2D(ptr_source, false, true, ora::WPos::Zero(), 1.0f, false);
  ORA_CHECK(up_sound != nullptr);
  ORA_CHECK(up_sound->GetVolume() == 1.0f);
  up_sound->SetVolume(0.25f);
  ORA_CHECK(up_sound->GetVolume() == 0.25f);
  ORA_CHECK(up_sound->SeekPosition() >= 0.0f);
  engine.StopSound(up_sound.get());
  ORA_CHECK(up_sound->Complete());
  up_sound.reset();

  {
    auto up_stream = engine.Play2DStream(
        std::make_unique<VectorPcmStream>(vec_pcm), 1, 16, 44100, false, true,
        ora::WPos::Zero(), 0.75f);
    ORA_CHECK(up_stream != nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    engine.StopSound(up_stream.get());

    ORA_CHECK(up_stream->Complete());
  }

  {
    std::vector<std::unique_ptr<ora::platform::ISound>> vec_holding;
    int int4_played = 0;
    for (;;) {
      auto up_play = engine.Play2D(ptr_source, true, true, ora::WPos::Zero(), 1.0f, false);
      if (up_play == nullptr)
        break;
      ++int4_played;
      vec_holding.push_back(std::move(up_play));
      if (int4_played > 300)
        break;
    }
    ORA_CHECK(int4_played >= 16);
    ORA_CHECK(int4_played <= ora::platform::kAlPoolSize);
    engine.StopAllSounds();

    auto up_again = engine.Play2D(ptr_source, false, true, ora::WPos::Zero(), 1.0f, false);
    ORA_CHECK(up_again != nullptr);
  }

  engine.SetListenerPosition(ora::WPos{1024, 1024, 0});
  engine.SetSoundVolume(0.5f, nullptr, nullptr);
  engine.SetAllSoundsPaused(true);
  engine.SetAllSoundsPaused(false);
}

void TestDummySoundEngine() {
  ora::platform::DummySoundEngine engine;
  ORA_CHECK(engine.Dummy());
  const std::vector<ora::platform::SoundDevice> vec_devices = engine.AvailableDevices();
  ORA_CHECK(vec_devices.size() == 1);
  ORA_CHECK(vec_devices[0].str_label == "No Sound Output");
  ORA_CHECK(engine.GetVolume() == 0.0f);

  std::array<std::uint8_t, 4> arr_bytes{};
  auto* const ptr_source = engine.AddSoundSourceFromMemory(arr_bytes, 1, 16, 22050);
  auto up_sound = engine.Play2D(ptr_source, false, true, ora::WPos::Zero(), 1.0f, true);
  ORA_CHECK(up_sound != nullptr);
  ORA_CHECK(!up_sound->Complete());
  ORA_CHECK(engine.Play2DStream(nullptr, 1, 16, 22050, false, true, ora::WPos::Zero(), 1.0f) ==
            nullptr);
  delete ptr_source;
}

void TestFreetypeFont(const char* ptr_upstream_root, const char* ptr_golden_path) {
  if (std::getenv("ORA_SKIP_FT") != nullptr)
    return;

  const ora::platform::FreetypeLoadResult load = ora::platform::LoadFreetypeDefault();
  if (!load.b_ok) {
    std::println("SKIP: freetype6.dll 不可用(缺 {}) | SKIP: freetype6.dll unavailable (missing {})",
                 load.str_missing, load.str_missing);
    return;
  }

  const std::array<std::uint8_t, 16> arr_garbage{};
  bool b_threw = false;
  try {
    const ora::platform::FreeTypeFont font_bad{arr_garbage};
  } catch (const std::runtime_error& ex) {
    b_threw = std::string_view{ex.what()} == "Failed to initialize font";
  }
  ORA_CHECK(b_threw);

  if (ptr_upstream_root == nullptr || ptr_golden_path == nullptr)
    return;

  std::ifstream stream_golden{std::filesystem::path{ptr_golden_path}};
  if (!stream_golden.is_open()) {
    std::println(stderr, "FAIL golden_fonts 打不开 | cannot open golden_fonts: {}", ptr_golden_path);
    ++int4_failures;
    return;
  }

  const std::array<const char*, 4> arr_fonts = {
      "mods/common/FreeSans.ttf", "mods/common/FreeSansBold.ttf",
      "mods/d2k/Dune2k.ttf", "mods/ra/ZoodRangmah.ttf"};
  const std::array<int, 3> arr_sizes = {7, 12, 16};
  const std::array<float, 2> arr_scales = {1.0f, 1.25f};
  const std::array<std::uint32_t, 9> arr_chars = {'A', 'a', 'Z', '0', ' ', '~', 0x00E9,
                                                  0x4E2D, 0xD800};

  std::int64_t int8_lines = 0;
  std::int64_t int8_glyphs = 0;
  std::string str_line;
  for (const char* str_rel : arr_fonts) {
    std::ifstream stream_font{std::filesystem::path{ptr_upstream_root} / str_rel,
                              std::ios::binary};
    if (!stream_font.is_open()) {
      std::println(stderr, "FAIL 字体打不开 | cannot open font: {}", str_rel);
      ++int4_failures;
      continue;
    }
    const std::vector<std::uint8_t> vec_font{std::istreambuf_iterator<char>{stream_font},
                                             std::istreambuf_iterator<char>{}};
    ora::platform::FreeTypeFont font{vec_font};

    if (!std::getline(stream_golden, str_line) || str_line != std::format("G {}", str_rel)) {
      std::println(stderr, "FAIL golden_fonts 段头(期望 G {})| golden_fonts header (expected G {})",
                   str_rel, str_rel);
      ++int4_failures;
      return;
    }
    int8_lines++;

    for (const int int4_size : arr_sizes) {
      for (const float fp4_scale : arr_scales) {
        for (const std::uint32_t uint4_cp : arr_chars) {
          const ora::platform::FontGlyph glyph = font.CreateGlyph(uint4_cp, int4_size, fp4_scale);
          std::string str_hex = "-";
          if (!glyph.vec_data.empty()) {
            str_hex.clear();
            for (const std::uint8_t uint1_byte : glyph.vec_data)
              str_hex += std::format("{:02X}", uint1_byte);
          }
          const std::string str_expect = std::format(
              "GL {} {} u{:04X} ADV {} OFF {} {} W {} H {} X {}", int4_size,
              ora::meta::FormatFloatNet(fp4_scale), uint4_cp,
              ora::meta::FormatFloatNet(glyph.fp4_advance), glyph.offset.X, glyph.offset.Y,
              glyph.int4_width, glyph.int4_height, str_hex);

          if (!std::getline(stream_golden, str_line)) {
            std::println(stderr, "FAIL golden_fonts 提前尽 | golden_fonts ended early");
            ++int4_failures;
            return;
          }
          int8_lines++;
          int8_glyphs++;
          if (str_line != str_expect) {
            std::println(stderr, "FAIL 字形不一致({} size={} scale={}):\n  golden: {}\n  cpp:   {}",
                         str_rel, int4_size, fp4_scale, str_line, str_expect);
            ++int4_failures;
            return;
          }
        }
      }
    }
  }

  ORA_CHECK(!std::getline(stream_golden, str_line));
  std::println("OpenAL/FreeType 黄金: {} 行 / {} 字形一致 | golden: {} lines / {} glyphs identical",
               int8_lines, int8_glyphs, int8_lines, int8_glyphs);
}

#ifdef ORA_HAS_DESKTOP_GL

void TestDoublePixelData() {

  const std::array<std::uint8_t, 16> arr_src{0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23,
                                             0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x43};
  const std::vector<std::uint8_t> vec_scaled = ora::platform::DoublePixelData(arr_src, 2, 2);
  ORA_CHECK(vec_scaled.size() == 64);

  std::vector<std::uint8_t> vec_expect(64, 0);
  for (int int4_y = 0; int4_y < 4; int4_y++) {
    for (int int4_x = 0; int4_x < 4; int4_x++) {
      for (int int4_i = 0; int4_i < 4; int4_i++)
        vec_expect[4 * (static_cast<std::size_t>(int4_y) * 4 + int4_x) + int4_i] =
            arr_src[4 * (static_cast<std::size_t>(int4_y / 2) * 2 + int4_x / 2) + int4_i];
    }
  }
  ORA_CHECK(vec_scaled == vec_expect);
}

void TestHardwareCursorLive() {
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;
  if (!ora::platform::InitSdl2Video())
    return;

  std::vector<std::uint8_t> vec_pixels(2 * 2 * 4, 0);
  for (std::size_t int4_i = 0; int4_i < 4; int4_i++) {
    vec_pixels[4 * int4_i + 0] = 0xFF;
    vec_pixels[4 * int4_i + 1] = 0xFF;
    vec_pixels[4 * int4_i + 2] = 0xFF;
    vec_pixels[4 * int4_i + 3] = 0xFF;
  }

  bool b_ok = true;
  try {
    const ora::platform::Sdl2HardwareCursor cursor{2, 2, vec_pixels, ora::int2{1, 1}};
    b_ok = cursor.Cursor() != nullptr;
  } catch (const std::exception& ex) {
    std::println("光标直建抛错({})| direct cursor build threw ({})", ex.what(), ex.what());
    b_ok = false;
  }
  ORA_CHECK(b_ok);

  ora::platform::Sdl2Window::Desc desc;
  desc.int4_width = 64;
  desc.int4_height = 48;
  auto opt_window = ora::platform::Sdl2Window::Create(desc);
  if (!opt_window.has_value()) {
    std::println("SKIP: 窗口不可用(光标集成)| SKIP: no window (cursor integration)");
    return;
  }

  auto opt_cursor = opt_window->CreateHardwareCursor("test", 2, 2, vec_pixels, ora::int2{1, 1},
                                                     false);
  const bool b_scale_single = opt_window->Geom().float_scale <= 1.5f;
  ORA_CHECK(opt_cursor.has_value() || !b_scale_single);
  if (opt_cursor.has_value()) {
    opt_window->SetHardwareCursor(&*opt_cursor);
    opt_window->SetHardwareCursor(nullptr);
  }

  auto opt_cursor_doubled = opt_window->CreateHardwareCursor("test2x", 2, 2, vec_pixels,
                                                             ora::int2{0, 0}, true);
  ORA_CHECK(opt_cursor_doubled.has_value() || !b_scale_single);
}

#endif

int main(int int4_argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  TestSpscSingleThread();
  TestSpscTwoThreads();

  TestTapHistorySequences();
  TestMultiTapDetectionCaches();
  TestMakeButtonAndModifiers();
  TestScaleAwayFromZero();

  TestMakeAlFormat();
  TestParseAlDeviceList();
  TestEvaluatePlayGate();
  TestOpenAlEngineLive();
  TestDummySoundEngine();
  const char* ptr_upstream_root = int4_argc > 1 ? argv[1] : nullptr;
  const char* ptr_golden_fonts = int4_argc > 2 ? argv[2] : nullptr;
  TestFreetypeFont(ptr_upstream_root, ptr_golden_fonts);

#ifdef ORA_HAS_DESKTOP_GL
  TestKeycodeAgainstSdl();
  TestPumpInputSynthetic();
  TestDoublePixelData();
  TestHardwareCursorLive();
  int int4_argc_dummy = 0;
  char* argv_dummy = nullptr;
  TestGlIntegration(int4_argc_dummy, &argv_dummy);
  TestGlResourceWrappers();
#endif

  if (int4_failures != 0) {
    std::println(stderr, "platform_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("platform_test: PASS(SPCS 队列 + 输入层 + OpenAL/FreeType/光标 + GL 集成与资源封装)");
  return 0;
}
