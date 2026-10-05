// UPSTREAM: OpenRA.Game/Graphics/ShaderBindings.cs @b6fc03f L17-34 +
// OpenRA.Platforms.Default/Shader.cs @b6fc03f L20-260(逐方法)
// 语义面:{VERSION} 占位替换(Embedded→"300 es",否则→"140",全量替换)、
// 属性循环(EnableVertexAttribArray + BindAttribLocation)、Modern 档
// BindFragDataLocation(program, 0, "fragColor")、链接后 active uniform 枚举
// (uniformCache 建表 + sampler 依次分配纹理单元并 Uniform1i 固定)、
// PrepareRender(use + 纹理表绑定)、Bind(属性指针重播)。
// OPT-A6(docs/OPTIMIZATION_TRACKER.md)落地点:
//   - uniform 位置构造期枚举为整数表(SetVec/SetBool 走整型 location 发射,
//     字符串只在查表时出现 —— 与上游 Dictionary<string,int> 缓存等价,但
//     Location() 整数 API 供热路径完全绕开字符串);
//   - 每次 PrepareRender/Set* 前的 UseProgram 由消费端绑定 diff 吞掉
//     (上游每次显式 glUseProgram,Shader.cs L147-172);
//   - 上游 PrepareRender 的 glIsTexture 逐帧驱逐(OPT-A6 批判点)以生命
//     周期契约替代:SetTexture 传入的 Texture 须存活至被替换或 Shader 析构
//     (引擎内 Sheet 纹理由 SpriteCache 持有,天然满足)—— COVERAGE 登记。
//   - VAO 缓存(每 (顶点格式, shader) 一 VAO)随 SpriteRenderer 批次落地,
//     本批 Bind() 保持上游的全局 VAO 属性重播语义。
// 偏离:链接成功后即 DeleteShader 两个编译产物(上游不删;已链接 program
// 的行为不受影响,GL 规范允许)。
// The semantic plane of Shader.cs plus the ShaderBindings.cs data plane:
// the {VERSION} placeholder substitution (Embedded→"300 es", else→"140", all
// occurrences), the attribute loop (EnableVertexAttribArray plus
// BindAttribLocation), BindFragDataLocation(program, 0, "fragColor") on the
// Modern profile, the post-link active-uniform enumeration (uniformCache
// built, samplers assigned texture units one by one and pinned via
// Uniform1i), PrepareRender (use + texture-table binding), and Bind
// (attribute-pointer replay). OPT-A6 landing points:
//   - uniform locations enumerate into an integer table at construction
//     (SetVec/SetBool post with integer locations; strings appear only on
//     table lookup — equivalent to upstream's Dictionary<string,int> cache,
//     while the Location() integer API lets hot paths bypass strings
//     entirely);
//   - the UseProgram before each PrepareRender/Set* is absorbed by the
//     consumer-side binding diff (upstream calls glUseProgram explicitly
//     every time, Shader.cs L147-172);
//   - upstream's per-flush glIsTexture eviction in PrepareRender (the OPT-A6
//     critique) is replaced by a lifetime contract: a Texture passed to
//     SetTexture must outlive its replacement or the Shader (Sheet textures
//     are held by SpriteCache in-engine, satisfying this naturally) —
//     registered in COVERAGE;
//   - VAO caching (one VAO per (vertex format, shader)) lands with the
//     SpriteRenderer batch; this batch's Bind() keeps upstream's global-VAO
//     attribute-replay semantics.
// Deviation: the two compiled shader objects are DeleteShader'd right after a
// successful link (upstream keeps them; linked programs are unaffected, as
// the GL spec allows).
#pragma once
import std;

#include "gfx/render_thread.hpp"
#include "gfx/texture.hpp"
#include "platform/gl_types.hpp"

namespace ora::gfx {

/// 顶点属性分量类型(ShaderBindings.cs L19-26;枚举值 = GL 常量)。
/// The vertex-attribute component type (ShaderBindings.cs L19-26; the enum
/// values are the GL constants).
enum class ShaderVertexAttributeType : std::uint16_t {
  Float = 0x1406,  // GL_FLOAT
  Int = 0x1404,    // GL_INT
  UInt = 0x1405,   // GL_UNSIGNED_INT
};

/// 顶点属性描述(ShaderBindings.cs L28)。
/// One vertex-attribute descriptor (ShaderBindings.cs L28).
struct ShaderVertexAttribute {
  std::string str_name;  // 属性名(拥有;BindAttribLocation/重播用)| owned (BindAttribLocation/replay)
  ShaderVertexAttributeType kind_type = ShaderVertexAttributeType::Float;
  std::int32_t int4_components = 0;
  std::int32_t int4_offset = 0;  // 顶点内字节偏移 | byte offset within the vertex
};

/// 着色器对描述(IShaderBindings;源码为调用方持有,string_view 传入,
/// 属性表在 Shader 内拷贝拥有)。
/// The shader-pair descriptor (IShaderBindings; sources are caller-owned and
/// passed as string_view, the attribute table copied and owned by Shader).
struct ShaderBindingsDesc {
  std::string_view str_vertex_shader_name;
  std::string_view str_vertex_shader_code;
  std::string_view str_fragment_shader_name;
  std::string_view str_fragment_shader_code;
  std::int32_t int4_stride = 0;
  std::span<const ShaderVertexAttribute> vec_attributes;
};

/// 着色器程序封装(主线程侧命令发射器;program 生命周期 RAII)。
/// The shader-program wrapper (a main-thread command emitter; RAII program
/// lifetime).
class Shader {
 public:
  /// 编译 + 链接 + uniform 枚举(Shader.cs L29-132;同步往返)。失败打印
  /// info log 并返回 std::nullopt。
  /// Compile + link + uniform enumeration (Shader.cs L29-132; synchronous
  /// round-trips). Failures print the info log and return std::nullopt.
  static std::optional<Shader> Create(RenderThread& render, const ShaderBindingsDesc& desc);

  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;
  Shader(Shader&& other) noexcept;
  Shader& operator=(Shader&&) = delete;

  /// 属性指针重播(Shader.cs L134-145;须在对应 VB 绑定后调用)。
  /// Attribute-pointer replay (Shader.cs L134-145; call after binding the
  /// corresponding VB).
  void Bind();

  /// 渲染前绑定(Shader.cs L147-172):use program + 绑定纹理表。
  /// Pre-render binding (Shader.cs L147-172): use the program and bind the
  /// texture table.
  void PrepareRender();

  /// 绑定采样器纹理(Shader.cs L174-182;名字须在 samplers 表内,否则忽略)。
  /// Binds a sampler texture (Shader.cs L174-182; the name must exist in the
  /// samplers table, else ignored).
  void SetTexture(std::string_view str_name, const Texture& texture);

  /// uniform 位置查询(未激活/不存在 = -1)。热路径应缓存返回值直发。
  /// Uniform-location lookup (inactive/absent = -1). Hot paths should cache
  /// the return and post directly.
  std::int32_t Location(std::string_view str_name) const;

  // —— 上游字符串接口(Shader.cs L184-259;内部查表后按整数位置发射)——
  // —— Upstream's string interfaces (Shader.cs L184-259; table lookup then
  // integer-location posting) ——

  void SetBool(std::string_view str_name, bool b_value);
  void SetVec(std::string_view str_name, float float_x);
  void SetVec(std::string_view str_name, float float_x, float float_y);
  void SetVec(std::string_view str_name, float float_x, float float_y, float float_z);
  void SetVec(std::string_view str_name, std::span<const float> vec_values, std::int32_t int4_length);
  void SetMatrix(std::string_view str_name, std::span<const float> vec_matrix16);

  // —— 整数位置直发(OPT-A6 热路径;语义同上)——
  // —— Integer-location direct posting (the OPT-A6 hot path; same semantics) ——

  void SetBoolAt(std::int32_t int4_location, bool b_value);
  void SetVecAt(std::int32_t int4_location, float float_x);
  void SetVecAt(std::int32_t int4_location, float float_x, float float_y);
  void SetVecAt(std::int32_t int4_location, float float_x, float float_y, float float_z);
  void SetVecAt(std::int32_t int4_location, std::span<const float> vec_values, std::int32_t int4_length);
  void SetMatrixAt(std::int32_t int4_location, std::span<const float> vec_matrix16);

  /// GL 程序名(0 = 已移动出/未创建)。
  /// The GL program name (0 = moved-from or uncreated).
  std::uint32_t GlId() const { return uint4_program_; }

 private:
  Shader() = default;
  void PostUseProgram();

  RenderThread* ptr_render_ = nullptr;
  std::uint32_t uint4_program_ = 0;
  std::vector<ShaderVertexAttribute> vec_attributes_;
  std::int32_t int4_stride_ = 0;
  std::map<std::string, std::int32_t, std::less<>> map_uniform_cache_;  // 名 → location | name → location
  std::map<std::string, std::uint32_t, std::less<>> map_samplers_;      // 名 → 纹理单元 | name → texture unit
  std::map<std::uint32_t, std::uint32_t> map_bound_textures_;           // 单元 → 纹理 GL 名 | unit → GL texture name
};

}  // namespace ora::gfx
