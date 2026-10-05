// UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @b6fc03f L20-669(全文:
// OpenAlSoundEngine/OpenAlSoundSource/OpenAlSound/OpenAlAsyncLoadSound)
// 形态映射(偏离登记 D83~D85,docs/COVERAGE.md):
//  - OpenRA-OpenAL-CS 包装层消失,al_loader 直连(OPT-A6 形态);
//  - sourcePool:Dictionary<uint,PoolSlot> → vector<PoolSlot>(保插入序 =
//    C# Dictionary 无删除时的枚举序;alGenSources 名非单调时行为等价,槽等价);
//  - Game.LocalTick → SetLocalTick 注入面(Phase 5/6 的 Game 接线点,上游为
//    静态全局读取);
//  - Play2D/Play2DStream 返回 unique_ptr<ISound>(所有权给调用方;对象析构经
//    回指针把槽内裸指针清空 —— 上游 GC 生命期的等价物);流式后台任务
//    Task.Run/Task.Delay → jthread + condition_variable(PORTING_PLAN §4.8);
//  - 字节参数 → span 直传(README 有意优化表)。
// 纯逻辑提取(可无设备单测):MakeAlFormat / ParseAlDeviceList / EvaluatePlayGate。
// [UPSTREAM continued] the whole of OpenAlSoundEngine.cs L20-669
// (OpenAlSoundEngine/OpenAlSoundSource/OpenAlSound/OpenAlAsyncLoadSound).
// Shape mapping (deviations D83–D85, docs/COVERAGE.md):
//  - the OpenRA-OpenAL-CS wrapper disappears in favor of the al_loader direct
//    connection (the OPT-A6 shape);
//  - sourcePool: Dictionary<uint,PoolSlot> becomes vector<PoolSlot> (insertion
//    order preserved = the enumeration order of a removal-free C# Dictionary;
//    behavior equivalent when alGenSources names are non-monotonic — slots are
//    interchangeable);
//  - Game.LocalTick becomes the SetLocalTick injection point (wired by Game in
//    Phase 5/6; upstream reads a static global);
//  - Play2D/Play2DStream return unique_ptr<ISound> (ownership to the caller;
//    the object's destructor clears its slot's raw pointer through a back-
//    pointer — the equivalent of upstream's GC lifetime); the streaming
//    background task's Task.Run/Task.Delay become a jthread + condition
//    variable (PORTING_PLAN §4.8);
//  - byte parameters pass as spans (the README deliberate-optimization table).
// Pure-logic extractions (unit-testable without a device): MakeAlFormat /
// ParseAlDeviceList / EvaluatePlayGate.
#pragma once
import std;

#include "platform/al_loader.hpp"
#include "platform/sound_engine.hpp"

namespace ora::platform {

class OpenAlSoundEngine;  // 前置:声音对象持有引擎回指 | forward: sound objects hold an engine back-pointer

/// 池常量(OpenAlSoundEngine.cs L49-55;上游类内 const,此处命名空间级以供纯
/// 逻辑提取共用)。
/// Pool constants (OpenAlSoundEngine.cs L49-55; upstream class-level consts,
/// hoisted to namespace scope for the pure-logic extraction).
inline constexpr int kAlMaxInstancesPerFrame = 3;  // 同源实例上限 | per-source instance cap
inline constexpr int kAlGroupDistance = 2730;      // 分组距离(世界单位)| grouping distance (world units)
inline constexpr std::int64_t kAlGroupDistanceSqr =
    static_cast<std::int64_t>(kAlGroupDistance) * kAlGroupDistance;
inline constexpr int kAlPoolSize = 256;  // 音源池(上游引 openal-soft #580)| the source pool (upstream cites openal-soft #580)

/// Play2D 的 AL 格式码(OpenAlSoundEngine.cs L109-115 的 internal static)。
/// The Play2D AL format code (the internal static of OpenAlSoundEngine.cs
/// L109-115).
ALenum MakeAlFormat(int int4_channels, int int4_sample_bits);

/// alcGetString 返回的双 NUL 结尾 UTF-8 设备表解析(OpenAlSoundEngine.cs
/// L62-95 的 QueryDevices 内层循环,提出为可注入合成数据的纯函数)。
/// 怪癖照抄:C# do-while 的 continue 跳到条件判定,读完名字末字节后 offset
/// 恰落其 NUL,循环在 flush 前退出 —— 非空名表恒得空结果(上游实际行为:
/// QueryDevices 恒 [],AvailableDevices 只剩 Default Output;bug 兼容)。
/// Parses the double-NUL-terminated UTF-8 device list returned by alcGetString
/// (the inner loop of QueryDevices, OpenAlSoundEngine.cs L62-95, extracted as
/// a pure function feedable with synthetic data). Quirk kept: a C# do-while's
/// continue jumps to the condition, so after a name's last byte the offset
/// lands on its NUL and the loop exits before flushing — any non-empty-name
/// list yields the empty result (the actual upstream behavior: QueryDevices
/// always [], AvailableDevices left with just the Default Output; bug
/// compatible).
std::vector<std::string> ParseAlDeviceList(const char* ptr_devices);

/// EvaluatePlayGate 的槽快照输入(纯逻辑测试面;字段 = PoolSlot 的判定子集)。
/// The slot snapshot taken by EvaluatePlayGate (the pure-logic test face;
/// fields are the PoolSlot subset the gate reads).
struct GateProbeSlot {
  bool b_is_active = false;
  bool b_is_relative = false;
  std::uintptr_t uintp_source_id = 0;  // 比较用的源身份 | source identity for comparison
  std::int32_t int4_frame_started = 0;
  WPos pos{};
};

/// Play2D 衰减/实例限制门(OpenAlSoundEngine.cs L211-243 的判定,逐语句提取;
/// 浮点表达式与运算序逐字照抄 —— 衰减值进发声音量,黄金关涉)。
/// The Play2D attenuation/instance gate (the decision of OpenAlSoundEngine.cs
/// L211-243 extracted statement by statement; the float expression and its
/// evaluation order copied verbatim — the attenuation feeds the played volume).
struct PlayGateResult {
  bool b_deny = false;  // 同源同帧内实例达 3 → 拒播 | 3 same-source instances in-window → deny
  float fp4_atten = 1.0f;
};

/// 判定(b_attenuate_volume 分支;非衰减路径恒 {false, 1f})。
/// The decision (the b_attenuate_volume branch; the non-attenuated path is
/// constantly {false, 1f}).
PlayGateResult EvaluatePlayGate(std::span<const GateProbeSlot> span_slots, bool b_relative,
                                 std::uintptr_t uintp_source_id, WPos pos_position,
                                 std::int32_t int4_curr_frame);

/// 音频缓冲(OpenAlSoundSource.cs 段,RAII = 上游 Dispose)。
/// An audio buffer (the OpenAlSoundSource.cs section; RAII = upstream
/// Dispose).
class OpenAlSoundSource final : public ISoundSource {
 public:
  OpenAlSoundSource(std::span<const std::uint8_t> span_data, int int4_channels,
                    int int4_sample_bits, int int4_sample_rate);
  ~OpenAlSoundSource() override;

  OpenAlSoundSource(const OpenAlSoundSource&) = delete;
  OpenAlSoundSource& operator=(const OpenAlSoundSource&) = delete;

  [[nodiscard]] ALuint Buffer() const { return uint4_buffer_; }
  [[nodiscard]] int SampleRate() const { return int4_sample_rate_; }

 private:
  ALuint uint4_buffer_ = 0;
  bool b_disposed_ = false;
  int int4_sample_rate_ = 0;
};

/// 单次发声(OpenAlSound.cs 段)。源解绑后 done 语义照抄:Volume/Seek 读 NaN、
/// Complete 恒真、全部操作 no-op。
/// One playing sound (the OpenAlSound.cs section). The post-unbind done
/// semantics copied verbatim: Volume/Seek read NaN, Complete stays true, every
/// operation no-ops.
class OpenAlSound : public ISound {
 public:
  friend class OpenAlSoundEngine;  // 析构序置空回指/解绑(引擎先亡路径)| nulls back-pointers in teardown order (engine-dies-first path)
  /// 缓冲直挂形态(Play2D:挂 buffer + 立即播放)。
  /// The buffer-attached form (Play2D: attach the buffer and play at once).
  OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping, bool b_relative,
              WPos pos_position, float fp4_volume, int int4_sample_rate, ALuint uint4_buffer);

  ~OpenAlSound() override;

  float GetVolume() const override;
  void SetVolume(float fp4_volume) override;
  [[nodiscard]] float SeekPosition() const override;
  [[nodiscard]] virtual bool Complete() const override;
  virtual void Stop();
  void SetPosition(WPos pos_position) override;
  void SetLooping(bool b_looping);

  [[nodiscard]] ALuint Source() const { return uint4_source_; }
  void UnbindSource();

 protected:
  /// 流式形态基构(不挂 buffer;OpenAlAsyncLoadSound 用)。
  /// The streaming-form base construction (no buffer; used by
  /// OpenAlAsyncLoadSound).
  OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping, bool b_relative,
              WPos pos_position, float fp4_volume, int int4_sample_rate);
  void StopSource();

  OpenAlSoundEngine* ptr_engine_;  // 析构回指清槽;引擎析构时置空 | destructor back-pointer clearing the slot; nulled at engine teardown
  ALuint uint4_source_;
  float fp4_sample_rate_;
  bool b_done_ = false;
};

/// 后台装流的发声(OpenAlAsyncLoadSound.cs 段):挂静音 buffer → jthread 读流
/// → 换真 buffer → 轮询到自然结束或取消(时序与锁序照抄)。
/// The background-loading sound (the OpenAlAsyncLoadSound.cs section): a
/// silent buffer attached first, a jthread fills from the stream, then swaps
/// in the real buffer and polls to natural completion or cancellation (timing
/// and lock order kept).
class OpenAlAsyncLoadSound final : public OpenAlSound {
 public:
  OpenAlAsyncLoadSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping,
                       bool b_relative, WPos pos_position, float fp4_volume, int int4_channels,
                       int int4_sample_bits, int int4_sample_rate,
                       std::unique_ptr<IPcmStream>&& up_stream);
  ~OpenAlAsyncLoadSound() override;

  void Stop() override;
  [[nodiscard]] bool Complete() const override;

 private:
  void RunPlayTask();

  std::jthread thread_play_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::atomic<bool> b_cancel_{false};  // 上游 CancellationToken 的等价物 | the equivalent of upstream's CancellationToken
  std::atomic<bool> b_finished_{false};
};

/// OpenAL 引擎(OpenAlSoundEngine.cs 主类)。
/// The OpenAL engine (the OpenAlSoundEngine.cs main class).
class OpenAlSoundEngine final : public ISoundEngine {
 public:
  /// device_name 为 nullopt = 默认设备;打开失败先回落默认再抛
  /// ("Can't create OpenAL device",消息逐字)。
  /// A device_name of nullopt means the default device; on open failure the
  /// engine falls back to the default device before throwing ("Can't create
  /// OpenAL device", message verbatim).
  explicit OpenAlSoundEngine(std::optional<std::string> str_device_name);
  ~OpenAlSoundEngine() override;

  OpenAlSoundEngine(const OpenAlSoundEngine&) = delete;
  OpenAlSoundEngine& operator=(const OpenAlSoundEngine&) = delete;

  [[nodiscard]] std::vector<SoundDevice> AvailableDevices() override;
  [[nodiscard]] bool Dummy() const override { return false; }
  [[nodiscard]] float GetVolume() const override { return fp4_volume_; }
  void SetVolume(float fp4_volume) override;

  ISoundSource* AddSoundSourceFromMemory(std::span<const std::uint8_t> span_data,
                                         int int4_channels, int int4_sample_bits,
                                         int int4_sample_rate) override;
  std::unique_ptr<ISound> Play2D(ISoundSource* ptr_source, bool b_loop, bool b_relative,
                                 WPos pos_position, float fp4_volume,
                                 bool b_attenuate_volume) override;
  std::unique_ptr<ISound> Play2DStream(std::unique_ptr<IPcmStream>&& up_stream, int int4_channels,
                                       int int4_sample_bits, int int4_sample_rate, bool b_loop,
                                       bool b_relative, WPos pos_position,
                                       float fp4_volume) override;

  void PauseSound(ISound* ptr_sound, bool b_paused) override;
  void StopSound(ISound* ptr_sound) override;
  void SetAllSoundsPaused(bool b_paused) override;
  void StopAllSounds() override;
  void SetListenerPosition(WPos pos_position) override;
  void SetSoundVolume(float fp4_volume, ISound* ptr_music, ISound* ptr_video) override;
  void SetSoundLooping(bool b_looping, ISound* ptr_sound) override;
  void SetSoundPosition(ISound* ptr_sound, WPos pos_position) override;

  /// Game.LocalTick 注入面(D84:上游静态读;Game 主循环每 tick 调用)。
  /// The Game.LocalTick injection point (D84: upstream reads a static; the
  /// Game main loop calls this every tick).
  void SetLocalTick(std::int32_t int4_tick) { int4_local_tick_ = int4_tick; }

 private:
  friend class OpenAlSound;
  friend class OpenAlAsyncLoadSound;

  struct PoolSlot {
    bool b_is_active = false;
    std::int32_t int4_frame_started = 0;
    WPos pos{};
    bool b_is_relative = false;
    OpenAlSoundSource* ptr_sound_source = nullptr;  // 借用(Sound 门面所有)| borrowed (owned by the Sound facade)
    OpenAlSound* ptr_sound = nullptr;               // 借用(调用方所有;回收时解绑)| borrowed (caller-owned; unbound on recycle)
    ALuint uint4_source = 0;
  };

  bool TryGetSourceFromPool(ALuint& uint4_source);
  static void PauseSource(ALuint uint4_source, bool b_paused);
  /// 声音对象析构回指:清槽 + 摘除登记(D84 生命期等价物)。
  /// The sound-object destructor back-call: clears the slot and deregisters
  /// (the D84 lifetime equivalent).
  void ForgetSound(OpenAlSound* ptr_sound);

  std::vector<PoolSlot> vec_slots_;          // 插入序 = 上游 Dictionary 枚举序 | insertion order = the upstream Dictionary enumeration order
  float fp4_volume_ = 1.0f;
  ALCdevice* ptr_device_ = nullptr;
  ALCcontext* ptr_context_ = nullptr;
  std::vector<OpenAlSound*> vec_live_sounds_;  // 析构序登记(引擎先亡时置空回指)| destructor-order registry (back-pointers nulled if the engine dies first)
  std::int32_t int4_local_tick_ = 0;           // D84 注入面默认 0(= Game.LocalTick 初值)| the D84 injection defaulting to 0 (= Game.LocalTick's initial value)
};

}  // namespace ora::platform
