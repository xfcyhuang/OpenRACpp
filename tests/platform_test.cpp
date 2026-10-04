// platform_test — Phase 4 第一批验收:SPSC 命令队列(纯逻辑,无 GL)+ 桌面 GL 集成
// (OPT-A5 命令缓冲渲染 / OPT-A6 状态 diff / OPT-B1 NPOT FBO / OPT-B5 原子快照)。
// Phase 4 第二批增补:输入层(多击/修饰符/坐标舍入/键码对照 + 合成事件泵)与
// Shader/Texture 封装(BGRA 往返 / 子区域 / {VERSION} / uniform 枚举 / sampler)。
// GL 集成在无桌面/无 GPU 环境自动跳过(SDL 初始化失败即 SKIP,退出码 0)。
// platform_test — the Phase 4 first-batch acceptance: the SPSC command queue
// (pure logic, no GL) plus desktop-GL integration (the OPT-A5 command-buffer
// render path / OPT-A6 state diff / OPT-B1 NPOT FBO / OPT-B5 atomic snapshot).
// Phase 4 second-batch additions: the input layer (multi-tap/modifiers/
// coordinate rounding/keycode cross-checks plus the synthetic event pump) and
// the Shader/Texture wrappers (BGRA round-trip / sub-rectangle / {VERSION} /
// uniform enumeration / samplers). The GL integration auto-skips (exit 0) on
// desktop-less/GPU-less hosts when SDL initialization fails.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "gfx/gfx_command.hpp"
#include "platform/sdl2_input.hpp"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define ORA_HAS_DESKTOP_GL 1
#define SDL_MAIN_HANDLED  // 阻止 SDL 劫持 main(测试自带入口)| keep SDL from hijacking main
#include <SDL2/SDL.h>  // 键码对照与合成事件(纯常量/事件推入,无初始化依赖)| keycode cross-checks and synthetic events (constants/pushing only)
#include "gfx/render_thread.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"
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

// ———— SPSC 队列:单线程顺序往返(容量受迫 + pad 触发)————
// ———— SPSC queue: single-thread sequential round-trips (forced small capacity + pad triggering) ————

/// 手动读者(不经渲染线程):取一条命令并消费。
/// A manual reader (no render thread): fetch and consume one command.
const ora::gfx::GfxCmd* ManualFetch(ora::gfx::GfxCommandQueue& queue) {
  std::uint32_t uint4_record = 0;
  const ora::gfx::GfxCmd* ptr_cmd = queue.FetchNext(uint4_record);
  if (ptr_cmd != nullptr)
    queue.Skip(uint4_record);
  return ptr_cmd;
}

void TestSpscSingleThread() {
  // 容量 256B,一条带 16B payload 的记录 72B:第 4 条触发环尾 pad;
  // 单线程测试须边写边读(全量写必然满载等待一个不存在的读者)。
  // A 256-byte capacity with 72-byte records (16-byte payload): the 4th
  // record triggers the ring-end pad; the single-thread test must interleave
  // writes and reads (writing everything first would deadlock waiting for a
  // reader that does not exist).
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

// ———— SPSC 队列:双线程压测(往返序号完整性 + 环绕)————
// ———— SPSC queue: two-thread stress (round-trip sequence integrity + wraparound) ————

void TestSpscTwoThreads() {
  constexpr std::uint32_t kCount = 200000;
  ora::gfx::GfxCommandQueue queue_test(1024);  // 小容量迫使高频阻塞/唤醒与 pad
                                                // small capacity forces frequent blocking/wakeups and pads

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
  // 收尾哨兵 + 停止
  // Tail sentinel + stop.
  if (auto* ptr_cmd = queue_test.Reserve(ora::gfx::GfxCmdKind::Shutdown, 0); ptr_cmd != nullptr)
    queue_test.CommitBare();
  thread_consumer.join();
}

#ifdef ORA_HAS_DESKTOP_GL

// ———— 桌面 GL 集成:OPT-A5 命令缓冲 + OPT-B1 NPOT FBO 渲染/读回断言 ————
// ———— Desktop-GL integration: OPT-A5 command-buffer + OPT-B1 NPOT FBO render/readback assertions ————

void TestGlIntegration(int& int4_argc, char** argv_ptr) {
  // 无头环境跳过(环境变量显式控制;SDL 失败亦跳过)
  // Skip on headless hosts (explicit env control; SDL failure also skips).
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;

  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 320, .int4_height = 240});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(无桌面/GPU 环境)| SKIP: no window (headless/GPU-less host)");
    return;
  }
  auto& window = *window_opt;

  // OPT-B5:原子快照读(尺寸与缩放)
  // OPT-B5: atomic snapshot reads (size and scale).
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

  // —— NPOT FBO(OPT-B1:上游强制 pow2,此处 333×257 直接创建)——
  // —— NPOT FBO (OPT-B1: upstream forces pow2; 333×257 created directly here) ——
  constexpr std::int32_t kW = 333, kH = 257;
  std::uint32_t uint4_tex = 0, uint4_fbo = 0;
  render.GenNames(ora::gfx::GfxCmdKind::GenTextures, 1, &uint4_tex);
  render.GenNames(ora::gfx::GfxCmdKind::GenFramebuffers, 1, &uint4_fbo);
  ORA_CHECK(uint4_tex != 0 && uint4_fbo != 0);

  std::vector<std::uint8_t> vec_pixels_init(kW * kH * 4, 0);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindTexture, 0);
    ptr_cmd->uint4_a = 0;        // unit 0
    ptr_cmd->uint4_b = 0x0DE1;   // GL_TEXTURE_2D
    ptr_cmd->uint4_c = uint4_tex;
    render.queue().CommitBare();
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::TexImage2D, 0);
    ptr_cmd->uint4_a = 0x0DE1;  // GL_TEXTURE_2D
    ptr_cmd->uint4_b = kW;
    ptr_cmd->uint4_c = kH;
    ptr_cmd->uint4_d = 0x8058;  // GL_RGBA8
    render.queue().Commit(vec_pixels_init.data());
  }
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::FramebufferTexture2D, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    ptr_cmd->uint4_b = 0x8CE0;  // GL_COLOR_ATTACHMENT0
    ptr_cmd->uint4_c = uint4_tex;
    render.queue().CommitBare();
  }
  render.FlushAndWait();
  ORA_CHECK(render.QueryFboStatus() == 0x8CD5);  // GL_FRAMEBUFFER_COMPLETE(NPOT 直过 = OPT-B1)

  // 绑定 FBO + 视口 + 清屏绿
  // Bind the FBO, set the viewport, clear green.
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

  // 读回验证清屏色(左下角)
  // Read back and verify the clear color (bottom-left).
  std::vector<std::uint8_t> vec_pixels(kW * kH * 4, 0);
  render.ReadPixels(0, 0, kW, kH, vec_pixels.data());
  ORA_CHECK(vec_pixels[0] == 0 && vec_pixels[1] == 255 && vec_pixels[2] == 0 && vec_pixels[3] == 255);

  // —— 三角形:VBO/VAO/program 全命令链 ——
  // —— Triangle: the full VBO/VAO/program command chain ——
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

  // 下半屏标准三角形 (-1,-1)/(1,-1)/(-1,1):斜边 y=-x,内部为 x+y<0
  // The standard lower-left-half triangle (-1,-1)/(1,-1)/(-1,1): hypotenuse
  // y=-x, interior x+y<0.
  const float vec_verts[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
  {  // BindVertexArray + BindBuffer + BufferData
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindVertexArray, 0);
    ptr_cmd->uint4_a = uint4_vao;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindBuffer, 0);
    ptr_cmd->uint4_a = 0x8892;  // GL_ARRAY_BUFFER
    ptr_cmd->uint4_b = uint4_vbo;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BufferData, sizeof(vec_verts));
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = sizeof(vec_verts);
    ptr_cmd->uint4_c = 0x88E4;  // GL_STATIC_DRAW
    render.queue().Commit(vec_verts);
  }
  {  // BindAttribLocation(链接前需要,此处程序已链接 → 改用attrib 0 默认绑定,跳过)
    // attribute 0 是 aPos(shader 只有一个 in,驱动分配 0)
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::VertexAttribPointer, 0);
    ptr_cmd->uint4_a = 0;         // index 0
    ptr_cmd->uint4_b = 2;         // size 2
    ptr_cmd->uint4_c = 0x1406;    // GL_FLOAT
    ptr_cmd->b_depth = 0;         // 未归一化 | not normalized
    ptr_cmd->uint4_d = 16;        // stride 2×float
    ptr_cmd->float_a = 0.0f;      // 偏移 0 | offset 0
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::EnableVertexAttribArray, 0);
    ptr_cmd->uint4_a = 0;
    render.queue().CommitBare();
  }

  // 查 uColor 位置(A6:整数位置,无字符串进渲染热路径)
  // Query the uColor location (A6: integer location, no strings on the hot path).
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

  {  // program + 红色 uniform + 绘制
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindProgram, 0);
    ptr_cmd->uint4_a = uint4_program;
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Uniform4fv, 16);
    ptr_cmd->uint4_a = uint4_location;
    ptr_cmd->uint4_b = 1;
    const float vec_red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    render.queue().Commit(vec_red);
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::DrawArrays, 0);
    ptr_cmd->uint4_a = 0x0004;  // GL_TRIANGLES
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = 3;
    render.queue().CommitBare();
  }
  render.FlushAndWait();

  render.ReadPixels(0, 0, kW, kH, vec_pixels.data());
  // 左下角在三角形内(红,clip(-0.98,-0.98):和 < 0),右上/右下在外(绿,和 > 0)
  // Bottom-left inside the triangle (red, clip(-0.98,-0.98): sum < 0);
  // top-right and bottom-right outside (green, sum > 0).
  const auto PixelAt = [&](std::int32_t x, std::int32_t y) {
    const std::size_t i = (static_cast<std::size_t>(y) * kW + x) * 4;
    return std::pair{vec_pixels[i], vec_pixels[i + 1]};
  };
  // 注:glReadPixels 原点在左下;FBO 坐标同源
  // NOTE: glReadPixels has its origin at the bottom-left; FBO coordinates match.
  auto [r1, g1] = PixelAt(4, 4);          // 左下 | bottom-left
  ORA_CHECK(r1 == 255 && g1 == 0);
  auto [r2, g2] = PixelAt(kW - 5, kH - 5);  // 右上 | top-right
  ORA_CHECK(r2 == 0 && g2 == 255);
  auto [r3, g3] = PixelAt(kW - 5, kH / 2);  // 右中:远离斜边的明确外部点(clip 和 ≈ +0.97)
                                             // mid-right: clearly outside, far from the hypotenuse
  ORA_CHECK(r3 == 0 && g3 == 255);

  (void)int4_argc;
  (void)argv_ptr;
}

#endif  // ORA_HAS_DESKTOP_GL

// ———— Phase 4 第二批:输入层纯逻辑(MultiTapDetection / 修饰符 / 舍入)————
// ———— Phase 4 second batch: input-layer pure logic (MultiTapDetection / modifiers / rounding) ————

void TestTapHistorySequences() {
  // 三槽历史:预填 1 秒前 → 首击必 1;250ms 内且位移 <4 → 递增;超时/超距重启。
  // The three-slot history: back-filled one second ago so the first tap is
  // 1; within 250ms and displacement <4 the count climbs; timeouts or big
  // jumps restart it.
  ora::TapHistory history(1000);
  ORA_CHECK(history.GetTapCount(ora::int2{5, 5}, 1100) == 1);
  ORA_CHECK(history.GetTapCount(ora::int2{5, 6}, 1150) == 2);   // 位移 1 | displacement 1
  ORA_CHECK(history.GetTapCount(ora::int2{8, 5}, 1200) == 3);   // 位移 3(<4)| displacement 3 (<4)
  ORA_CHECK(history.LastTapCount() == 3);
  ORA_CHECK(history.GetTapCount(ora::int2{8, 5}, 1500) == 1);   // 间隔 300ms 重启 | 300ms gap restarts
  ORA_CHECK(history.GetTapCount(ora::int2{20, 5}, 1550) == 1);  // 位移 12 重启 | displacement 12 restarts
  ORA_CHECK(history.GetTapCount(ora::int2{20, 5}, 1560) == 2);

  // 距离边界:ISqrt 语义 —— 位移 (3,0)=3 接近,(4,0)=4 不接近
  // Distance boundary: the ISqrt semantics — displacement (3,0)=3 is close,
  // (4,0)=4 is not.
  ora::TapHistory history_dist(0);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{0, 0}, 0) == 1);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{3, 0}, 10) == 2);
  ORA_CHECK(history_dist.GetTapCount(ora::int2{7, 0}, 20) == 1);  // 位移 4 断链 | displacement 4 breaks the chain
  ORA_CHECK(history_dist.GetTapCount(ora::int2{9, 0}, 30) == 2);  // 位移 2 续链 | displacement 2 chains on
}

void TestMultiTapDetectionCaches() {
  // 键盘历史按 (键, 修饰符) 独立;鼠标历史按 SDL 按钮号;未记录槽 Info = 1。
  // Keyboard histories are per (key, modifiers); mouse histories are per SDL
  // button id; an unrecorded slot's Info is 1.
  ora::MultiTapDetection detect;
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::None, 0) == 1);
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::None, 100) == 2);
  ORA_CHECK(detect.InfoFromKeyboard(ora::Keycode::A, ora::Modifiers::None) == 2);
  ORA_CHECK(detect.DetectFromKeyboard(ora::Keycode::A, ora::Modifiers::Shift, 100) == 1);
  ORA_CHECK(detect.InfoFromKeyboard(ora::Keycode::A, ora::Modifiers::Shift) == 1);

  ORA_CHECK(detect.DetectFromMouse(1, ora::int2{0, 0}, 0) == 1);
  ORA_CHECK(detect.DetectFromMouse(1, ora::int2{1, 1}, 10) == 2);
  ORA_CHECK(detect.InfoFromMouse(1) == 2);
  ORA_CHECK(detect.InfoFromMouse(3) == 1);  // 未记录按钮 | unrecorded button
}

void TestMakeButtonAndModifiers() {
  // Sdl2Input.cs L26-41 的换算(值 = SDL 头常量)。
  // The conversions of Sdl2Input.cs L26-41 (values from the SDL headers).
  ORA_CHECK(ora::Sdl2Input::MakeButton(1) == ora::MouseButton::Left);
  ORA_CHECK(ora::Sdl2Input::MakeButton(2) == ora::MouseButton::Middle);
  ORA_CHECK(ora::Sdl2Input::MakeButton(3) == ora::MouseButton::Right);
  ORA_CHECK(ora::Sdl2Input::MakeButton(8) == ora::MouseButton::None);
  ORA_CHECK(ora::Sdl2Input::MakeButton(0) == ora::MouseButton::None);

  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0000) == ora::Modifiers::None);
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0001) == ora::Modifiers::Shift);  // KMOD_LSHIFT
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0002) == ora::Modifiers::Shift);  // KMOD_RSHIFT
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0040) == ora::Modifiers::Ctrl);   // KMOD_LCTRL
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0100) == ora::Modifiers::Alt);    // KMOD_LALT
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0400) == ora::Modifiers::Meta);   // KMOD_LGUI
  ORA_CHECK(ora::Sdl2Input::MakeModifiers(0x0800) == ora::Modifiers::Meta);   // KMOD_RGUI
  // 组合:LGUI|LSHIFT|LALT(0x0501)→ Meta|Shift|Alt
  // Combination: LGUI|LSHIFT|LALT (0x0501) → Meta|Shift|Alt.
  const auto mods_combo = ora::Sdl2Input::MakeModifiers(0x0400 | 0x0001 | 0x0100);
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Meta));
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Shift));
  ORA_CHECK(ora::HasModifier(mods_combo, ora::Modifiers::Alt));
  ORA_CHECK(!ora::HasModifier(mods_combo, ora::Modifiers::Ctrl));
}

void TestScaleAwayFromZero() {
  // C# (int)(Math.Sign(v)/2f + v*s) 的截断语义:±0.5 前置 + 向零截断。
  // The truncation semantics of C# (int)(Math.Sign(v)/2f + v*s): the ±0.5
  // prepend plus truncation towards zero.
  using ora::Sdl2Input;
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(0, 0.5f) == 0);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(1, 0.5f) == 1);    // 0.5+0.5 = 1.0
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(2, 0.5f) == 1);    // 0.5+1.0 = 1.5 → 1(向零)| towards zero
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(3, 0.5f) == 2);    // 0.5+1.5 = 2.0
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-1, 0.5f) == -1);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-2, 0.5f) == -1);  // -0.5-1.0 = -1.5 → -1(向零)| towards zero
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-3, 0.5f) == -2);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(7, 0.25f) == 2);   // 0.5+1.75 = 2.25 → 2
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-7, 0.25f) == -2);
  // s=1 恒等(即使 scale==1 分支不走此函数,语义上应保恒等)
  // s=1 identity (the scale==1 branch bypasses this function, but the
  // semantics should stay identical anyway).
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(41, 1.0f) == 41);
  ORA_CHECK(Sdl2Input::ScaleAwayFromZero(-41, 1.0f) == -41);
}

#ifdef ORA_HAS_DESKTOP_GL

// ———— 键码对照:Keycode 枚举 vs SDL 头常量(值域克隆验证)————
// ———— Keycode cross-check: the Keycode enum vs the SDL header constants (value-clone verification) ————

void TestKeycodeAgainstSdl() {
  const auto AsInt = [](ora::Keycode key) { return static_cast<std::int32_t>(key); };
  // 可打印区(C# 字符常量)| printable range (the C# character constants)
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
  // 功能/编辑/导航区(|1<<30 段)| the function/edit/navigation block (the |1<<30 range)
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
  // 小键盘 | keypad
  ORA_CHECK(AsInt(ora::Keycode::NUMLOCKCLEAR) == SDLK_NUMLOCKCLEAR);
  ORA_CHECK(AsInt(ora::Keycode::KP_DIVIDE) == SDLK_KP_DIVIDE);
  ORA_CHECK(AsInt(ora::Keycode::KP_ENTER) == SDLK_KP_ENTER);
  ORA_CHECK(AsInt(ora::Keycode::KP_0) == SDLK_KP_0);
  ORA_CHECK(AsInt(ora::Keycode::KP_9) == SDLK_KP_9);
  ORA_CHECK(AsInt(ora::Keycode::KP_PERIOD) == SDLK_KP_PERIOD);
  // 修饰键(多击缓存键成分)| modifiers (multi-tap cache-key components)
  ORA_CHECK(AsInt(ora::Keycode::LCTRL) == SDLK_LCTRL);
  ORA_CHECK(AsInt(ora::Keycode::RCTRL) == SDLK_RCTRL);
  ORA_CHECK(AsInt(ora::Keycode::LSHIFT) == SDLK_LSHIFT);
  ORA_CHECK(AsInt(ora::Keycode::RSHIFT) == SDLK_RSHIFT);
  ORA_CHECK(AsInt(ora::Keycode::LALT) == SDLK_LALT);
  ORA_CHECK(AsInt(ora::Keycode::RALT) == SDLK_RALT);
  ORA_CHECK(AsInt(ora::Keycode::LGUI) == SDLK_LGUI);
  ORA_CHECK(AsInt(ora::Keycode::RGUI) == SDLK_RGUI);
  // OpenRA 扩展(SDL 无对应)| the OpenRA extensions (no SDL counterparts)
  ORA_CHECK(AsInt(ora::Keycode::MOUSE4) == (283 | (1 << 30)));
  ORA_CHECK(AsInt(ora::Keycode::MOUSE5) == (284 | (1 << 30)));
  // UnicodeChar 截断面 | the UnicodeChar truncation face
  ORA_CHECK(ora::ToUnicodeChar(ora::Keycode::A) == 'a');
}

// ———— 合成事件泵:SDL_PushEvent → PumpInput 分发(多击/motion 合并/文本)————
// ———— The synthetic event pump: SDL_PushEvent → PumpInput dispatch (multi-tap/motion coalescing/text) ————

/// 记录汇:全事件收集。
/// The recording sink: collects everything.
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

  // 冲刷泵内残留(窗口创建期的焦点/尺寸事件)
  // Drain leftovers inside the pump (focus/size events from window creation).
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
  PushKey(SDLK_a);  // 固定时钟 0 → 250ms 内 → MultiTapCount 2 | fixed clock 0 → within 250ms → MultiTapCount 2
  PushMotion(10, 10, 10, 10);
  PushMotion(20, 20, 10, 10);
  PushMotion(30, 30, 10, 10);  // 三条 motion 合并为最后一条 | three motions coalesce to the last
  PushButton(SDL_BUTTON_LEFT, 100, 100);
  PushButton(SDL_BUTTON_LEFT, 100, 100);  // 双击 → 2 | double-click → 2
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

  ORA_CHECK(handler.int4_mod_calls == 1);  // 泵前一次 | once before the pump
  ORA_CHECK(b_quit);                       // SDL_QUIT 上报 | SDL_QUIT reported

  ORA_CHECK(handler.vec_keys.size() == 2);
  if (handler.vec_keys.size() == 2) {
    ORA_CHECK(handler.vec_keys[0].Event == ora::KeyInputEvent::Down);
    ORA_CHECK(handler.vec_keys[0].Key == ora::Keycode::A);
    ORA_CHECK(handler.vec_keys[0].MultiTapCount == 1);
    ORA_CHECK(handler.vec_keys[0].UnicodeChar == u'a');
    ORA_CHECK(handler.vec_keys[1].MultiTapCount == 2);
  }

  // motion 合并:恰一条 Move,位置 = 最后事件,delta = 最后相对量(经坐标换算)
  // Motion coalescing: exactly one Move at the last position with the last
  // relative delta (through the coordinate conversion).
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

  // 左键双击:两条 Down,MultiTapCount 1 → 2;位置经换算
  // Left double-click: two Downs with MultiTapCount 1 → 2; position converted.
  std::int32_t int4_left_downs = 0;
  for (const auto& mouse : handler.vec_mice) {
    if (mouse.Event == ora::MouseInputEvent::Down && mouse.Button == ora::MouseButton::Left) {
      ++int4_left_downs;
      ORA_CHECK(mouse.Location == ora::Sdl2Input::EventPosition(window, 100, 100));
      ORA_CHECK(mouse.MultiTapCount == int4_left_downs);
    }
  }
  ORA_CHECK(int4_left_downs == 2);

  // 滚轮:Scroll,Delta = (0, +1)
  // Wheel: Scroll with Delta = (0, +1).
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

// ———— Shader/Texture 封装集成(OPT-A6/OPT-B1)————
// ———— Shader/Texture wrapper integration (OPT-A6/OPT-B1) ————

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

  // —— Texture:NPOT BGRA 全量往返(OPT-B1:5×3 直接建)——
  // —— Texture: NPOT BGRA full round-trip (OPT-B1: 5×3 built directly) ——
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
  ORA_CHECK(texture.GetData() == vec_bgra);  // BGRA 逐字节往返 | BGRA byte-exact round-trip

  // —— SetSubData:行打包(UNPACK_ROW_LENGTH = 纹理宽;上游契约 = 源为
  // 行距整行宽的位图,SKIP_PIXELS/SKIP_ROWS 定位子窗,如 HardwarePalette
  // 的脏行上传)——
  // —— SetSubData: row packing (UNPACK_ROW_LENGTH = texture width; the
  // upstream contract — the source is a bitmap pitched at the full texture
  // width, SKIP_PIXELS/SKIP_ROWS locate the sub-window, as in
  // HardwarePalette's dirty-row uploads) ——
  // 源 = 2 行 × 5 像素(40B):第 1 行的 x=1..2 为 0xAB,其余填充 0xCD。
  // Source = 2 rows × 5 pixels (40B): x=1..2 of row 1 are 0xAB, the rest 0xCD.
  std::vector<std::byte> vec_src(std::size_t(4) * kW * 2, static_cast<std::byte>(0xCD));
  for (const std::int32_t x : {1, 2})
    for (int c = 0; c < 4; ++c)
      vec_src[(static_cast<std::size_t>(kW) + x) * 4 + c] = static_cast<std::byte>(0xAB);
  texture.SetSubData(vec_src, 1, 1, 2, 1);  // 子窗 (x=1,y=1) 2×1 | the 2×1 window at (1,1)
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
  ORA_CHECK(b_patch_ok);  // 仅子区域变化,其余行/列原样 | only the patch changed

  // —— 过滤切换 + 再次全量(参数序列幂等)——
  // —— Filter switch + full re-upload (idempotent parameter sequence) ——
  texture.SetScaleFilter(ora::gfx::TextureScaleFilter::Linear);
  texture.SetScaleFilter(ora::gfx::TextureScaleFilter::Linear);  // 同值不再重发 | same value skips re-issue
  texture.SetData(vec_bgra, kW, kH);
  render.FlushAndWait();
  ORA_CHECK(texture.GetData() == vec_bgra);

  // —— Shader:{VERSION} 替换 + uniform 枚举 + sampler 绑定 ——
  // —— Shader: {VERSION} substitution + uniform enumeration + sampler binding ——
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
  ORA_CHECK(shader.Location("uColor") >= 0);        // active uniform 枚举入表 | enumerated into the table
  ORA_CHECK(shader.Location("uTex") >= 0);          // sampler 亦在 uniform 表 | samplers are uniforms too
  ORA_CHECK(shader.Location("nonexistent") == -1);  // 未激活名 = -1 | inactive name = -1

  // 1×1 白纹理(采样源)
  // The 1×1 white texture (the sampling source).
  ora::gfx::Texture texture_white(render);
  const std::byte vec_white[4] = {static_cast<std::byte>(255), static_cast<std::byte>(255),
                                  static_cast<std::byte>(255), static_cast<std::byte>(255)};
  texture_white.SetData(vec_white, 1, 1);
  shader.SetTexture("uTex", texture_white);

  // FBO 渲染目标(32×24 NPOT,Texture 封装作颜色附件)
  // The FBO render target (32×24 NPOT; the Texture wrapper as color attachment).
  constexpr std::int32_t kRW = 32, kRH = 24;
  ora::gfx::Texture texture_rt(render);
  texture_rt.SetEmpty(kRW, kRH);
  std::uint32_t uint4_fbo = 0;
  render.GenNames(ora::gfx::GfxCmdKind::GenFramebuffers, 1, &uint4_fbo);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::FramebufferTexture2D, 0);
    ptr_cmd->uint4_a = uint4_fbo;
    ptr_cmd->uint4_b = 0x8CE0;  // GL_COLOR_ATTACHMENT0
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
  ORA_CHECK(render.QueryFboStatus() == 0x8CD5);  // FRAMEBUFFER_COMPLETE

  // 清绿底
  // Clear green.
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::Clear, 0);
    ptr_cmd->float_a = 0.0f;
    ptr_cmd->float_b = 1.0f;
    ptr_cmd->float_c = 0.0f;
    ptr_cmd->float_d = 1.0f;
    render.queue().CommitBare();
  }

  // VAO/VBO + Shader::Bind 属性重播 + PrepareRender + uColor
  // VAO/VBO + Shader::Bind attribute replay + PrepareRender + uColor.
  std::uint32_t uint4_ids[2] = {};
  render.GenNames(ora::gfx::GfxCmdKind::GenBuffers, 1, uint4_ids);
  render.GenNames(ora::gfx::GfxCmdKind::GenVertexArrays, 1, uint4_ids + 1);
  const float vec_verts[6] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindVertexArray, 0);
    ptr_cmd->uint4_a = uint4_ids[1];
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BindBuffer, 0);
    ptr_cmd->uint4_a = 0x8892;  // GL_ARRAY_BUFFER
    ptr_cmd->uint4_b = uint4_ids[0];
    render.queue().CommitBare();
    ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::BufferData, sizeof(vec_verts));
    ptr_cmd->uint4_a = 0x8892;
    ptr_cmd->uint4_b = sizeof(vec_verts);
    ptr_cmd->uint4_c = 0x88E4;  // GL_STATIC_DRAW
    render.queue().Commit(vec_verts);
  }
  shader.Bind();               // aPos 指针重播(全局 VAO 上)| aPos pointer replay (on the bound VAO)
  shader.PrepareRender();      // use + uTex → unit 0 绑定 | use + uTex bound to unit 0
  const float vec_red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  shader.SetVec("uColor", vec_red, 4);
  {
    auto* ptr_cmd = render.queue().Reserve(ora::gfx::GfxCmdKind::DrawArrays, 0);
    ptr_cmd->uint4_a = 0x0004;  // GL_TRIANGLES
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = 3;
    render.queue().CommitBare();
  }
  render.FlushAndWait();

  // 采样链断言:白纹理 × 红 uColor → 三角形内红、外绿
  // Sampling-chain assertion: white texture × red uColor → red inside the
  // triangle, green outside.
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

  // RAII:shader/纹理析构(DeleteProgram/DeleteTextures 异步)后管线照常
  // RAII: after the shader/texture destructors (asynchronous
  // DeleteProgram/DeleteTextures) the pipeline still works.
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

#endif  // ORA_HAS_DESKTOP_GL

}  // namespace

int main() {
  TestSpscSingleThread();
  TestSpscTwoThreads();

  TestTapHistorySequences();
  TestMultiTapDetectionCaches();
  TestMakeButtonAndModifiers();
  TestScaleAwayFromZero();

#ifdef ORA_HAS_DESKTOP_GL
  TestKeycodeAgainstSdl();
  TestPumpInputSynthetic();
  int int4_argc_dummy = 0;
  char* argv_dummy = nullptr;
  TestGlIntegration(int4_argc_dummy, &argv_dummy);
  TestGlResourceWrappers();
#endif

  if (int4_failures != 0) {
    std::println(stderr, "platform_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("platform_test: PASS(SPCS 队列 + 输入层 + GL 集成与资源封装)");
  return 0;
}
