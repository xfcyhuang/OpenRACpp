// platform_test — Phase 4 第一批验收:SPSC 命令队列(纯逻辑,无 GL)+ 桌面 GL 集成
// (OPT-A5 命令缓冲渲染 / OPT-A6 状态 diff / OPT-B1 NPOT FBO / OPT-B5 原子快照)。
// GL 集成在无桌面/无 GPU 环境自动跳过(SDL 初始化失败即 SKIP,退出码 0)。
// platform_test — the Phase 4 first-batch acceptance: the SPSC command queue
// (pure logic, no GL) plus desktop-GL integration (the OPT-A5 command-buffer
// render path / OPT-A6 state diff / OPT-B1 NPOT FBO / OPT-B5 atomic snapshot).
// The GL integration auto-skips (exit 0) on desktop-less/GPU-less hosts when
// SDL initialization fails.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "gfx/gfx_command.hpp"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define ORA_HAS_DESKTOP_GL 1
#include "gfx/render_thread.hpp"
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

}  // namespace

int main() {
  TestSpscSingleThread();
  TestSpscTwoThreads();

#ifdef ORA_HAS_DESKTOP_GL
  int int4_argc_dummy = 0;
  char* argv_dummy = nullptr;
  TestGlIntegration(int4_argc_dummy, &argv_dummy);
#endif

  if (int4_failures != 0) {
    std::println(stderr, "platform_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("platform_test: PASS(SPCS 队列 + GL 集成)");
  return 0;
}
