// UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @b6fc03f L20-669(实现)
// 逐语句移植;浮点表达式(衰减公式、lengthInSecs)的运算序与字面量逐字照抄。
// 上游 Log.Write("sound",…) 形态偏离为 stderr 前缀打印(日志不入任何对拍面)。
// Statement-by-statement port; the float expressions (the attenuation
// formula, lengthInSecs) keep their evaluation order and literals verbatim.
// Upstream's Log.Write("sound", …) deviates as prefixed stderr prints (the
// logs never enter any differential surface).
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "platform/openal_sound_engine.hpp"

namespace ora::platform {

namespace {

/// 上游 Log.Write("sound", …) 的等价物(偏离登记:日志面形态)。
/// The equivalent of upstream Log.Write("sound", …) (registered deviation:
/// the logging surface's shape).
void LogSound(std::string_view str_msg) {
  std::println(stderr, "[sound] {}", str_msg);
}

/// QueryDevices(OpenAlSoundEngine.cs L62-95):清错误位 → alcGetString →
/// 解析;失败返回空表。
/// QueryDevices (OpenAlSoundEngine.cs L62-95): clear the error bit →
/// alcGetString → parse; an empty list on failure.
std::vector<std::string> QueryDevices(const char* str_label, ALCenum int4_type) {
  al::alGetError();  // 清错误位 | clear the error bit

  const ALCchar* ptr_devices = al::alcGetString(nullptr, int4_type);
  if (ptr_devices == nullptr || al::alGetError() != AL_NO_ERROR) {
    LogSound(std::format("Failed to query OpenAL device list using {}", str_label));
    return {};
  }

  return ParseAlDeviceList(reinterpret_cast<const char*>(ptr_devices));
}

/// PhysicalDevices(OpenAlSoundEngine.cs L97-107):扩展探测序照抄。
/// PhysicalDevices (OpenAlSoundEngine.cs L97-107): the extension probe order
/// kept.
std::vector<std::string> PhysicalDevices() {
  // Windows Vista 起返回全部设备 | Returns all devices under Windows Vista and newer
  if (al::alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
    return QueryDevices("ALC_ENUMERATE_ALL_EXT", ALC_ALL_DEVICES_SPECIFIER);

  if (al::alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
    return QueryDevices("ALC_ENUMERATION_EXT", ALC_DEVICE_SPECIFIER);

  return {};
}

}  // namespace

// ———— 纯逻辑提取(OpenAlSoundEngine.cs L109-115 / L62-95 / L211-243)————
// ———— Pure-logic extractions (OpenAlSoundEngine.cs L109-115 / L62-95 / L211-243) ————

ALenum MakeAlFormat(int int4_channels, int int4_sample_bits) {
  if (int4_channels == 1)
    return int4_sample_bits == 16 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
  return int4_sample_bits == 16 ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
}

std::vector<std::string> ParseAlDeviceList(const char* ptr_devices) {
  std::vector<std::string> vec_devices;
  std::string str_buffer;
  std::size_t int4_offset = 0;

  do {
    const char ch = ptr_devices[int4_offset++];
    if (ch != '\0') {
      str_buffer.push_back(ch);
      continue;
    }

    // NUL = 一条设备名结束(UTF-8 字节序收集,无转义)。
    // A NUL terminates one device name (collected as UTF-8 bytes, no escapes).
    vec_devices.push_back(str_buffer);
    str_buffer.clear();
  } while (ptr_devices[int4_offset] != '\0');  // 连续两个 NUL = 表尾 | two successive NULs end the list

  return vec_devices;
}

PlayGateResult EvaluatePlayGate(std::span<const GateProbeSlot> span_slots, bool b_relative,
                                std::uintptr_t uintp_source_id, WPos pos_position,
                                std::int32_t int4_curr_frame) {
  int int4_instances = 0;
  int int4_active_count = 0;
  for (const GateProbeSlot& slot : span_slots) {
    if (!slot.b_is_active)
      continue;
    if (slot.b_is_relative != b_relative)
      continue;

    ++int4_active_count;
    if (slot.uintp_source_id != uintp_source_id)
      continue;
    if (int4_curr_frame - slot.int4_frame_started >= 5)
      continue;

    // 太远不计入? | Too far away to count?
    const std::int64_t int8_lensqr = (slot.pos - pos_position).LengthSquared();
    if (int8_lensqr >= kAlGroupDistanceSqr)
      continue;

    // 短时间内同源实例过多 → 拒播 | too many same-source instances in a short window → deny
    if (++int4_instances == kAlMaxInstancesPerFrame)
      return {true, 1.0f};
  }

  // 按活跃声数轻微衰减 | attenuate a little based on the number of active sounds
  return {false, 0.66f * ((kAlPoolSize - int4_active_count * 0.5f) / kAlPoolSize)};
}

// ———— OpenAlSoundSource(L394-428)———— | OpenAlSoundSource (L394-428) ————

OpenAlSoundSource::OpenAlSoundSource(std::span<const std::uint8_t> span_data, int int4_channels,
                                     int int4_sample_bits, int int4_sample_rate)
    : int4_sample_rate_{int4_sample_rate} {
  al::alGenBuffers(1, &uint4_buffer_);
  al::alBufferData(uint4_buffer_, MakeAlFormat(int4_channels, int4_sample_bits), span_data.data(),
                   static_cast<ALsizei>(span_data.size()), int4_sample_rate);
}

OpenAlSoundSource::~OpenAlSoundSource() {
  if (!b_disposed_) {
    al::alDeleteBuffers(1, &uint4_buffer_);
    b_disposed_ = true;
  }
}

// ———— OpenAlSound(L430-544)———— | OpenAlSound (L430-544) ————

OpenAlSound::OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping,
                         bool b_relative, WPos pos_position, float fp4_volume,
                         int int4_sample_rate, ALuint uint4_buffer)
    : OpenAlSound{ptr_engine, uint4_source, b_looping, b_relative, pos_position, fp4_volume,
                  int4_sample_rate} {
  al::alSourcei(uint4_source_, AL_BUFFER, static_cast<ALint>(uint4_buffer));
  al::alSourcePlay(uint4_source_);
}

OpenAlSound::OpenAlSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source, bool b_looping,
                         bool b_relative, WPos pos_position, float fp4_volume,
                         int int4_sample_rate)
    : ptr_engine_{ptr_engine},
      uint4_source_{uint4_source},
      fp4_sample_rate_{static_cast<float>(int4_sample_rate)} {
  SetVolume(fp4_volume);

  al::alSourcef(uint4_source_, AL_PITCH, 1.0f);
  al::alSource3f(uint4_source_, AL_POSITION, static_cast<ALfloat>(pos_position.X),
                 static_cast<ALfloat>(pos_position.Y), static_cast<ALfloat>(pos_position.Z));
  al::alSource3f(uint4_source_, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
  al::alSourcei(uint4_source_, AL_LOOPING, b_looping ? 1 : 0);
  al::alSourcei(uint4_source_, AL_SOURCE_RELATIVE, b_relative ? 1 : 0);

  al::alSourcef(uint4_source_, AL_REFERENCE_DISTANCE, 6826);
  al::alSourcef(uint4_source_, AL_MAX_DISTANCE, 136533);
}

OpenAlSound::~OpenAlSound() {
  if (ptr_engine_ != nullptr)
    ptr_engine_->ForgetSound(this);
}

void OpenAlSound::UnbindSource() {
  b_done_ = true;
  uint4_source_ = std::numeric_limits<ALuint>::max();
}

float OpenAlSound::GetVolume() const {
  if (b_done_)
    return std::numeric_limits<float>::quiet_NaN();

  ALfloat fp4_volume = 0.0f;
  al::alGetSourcef(uint4_source_, AL_GAIN, &fp4_volume);
  return fp4_volume;
}

void OpenAlSound::SetVolume(float fp4_volume) {
  if (b_done_)
    return;

  al::alSourcef(uint4_source_, AL_GAIN, fp4_volume);
}

float OpenAlSound::SeekPosition() const {
  if (b_done_)
    return std::numeric_limits<float>::quiet_NaN();

  ALint int4_sample_offset = 0;
  al::alGetSourcei(uint4_source_, AL_SAMPLE_OFFSET, &int4_sample_offset);
  return int4_sample_offset / fp4_sample_rate_;
}

bool OpenAlSound::Complete() const {
  if (b_done_)
    return true;

  ALint int4_state = 0;
  al::alGetSourcei(uint4_source_, AL_SOURCE_STATE, &int4_state);
  return int4_state == AL_STOPPED;
}

void OpenAlSound::SetPosition(WPos pos_position) {
  if (b_done_)
    return;

  al::alSource3f(uint4_source_, AL_POSITION, static_cast<ALfloat>(pos_position.X),
                 static_cast<ALfloat>(pos_position.Y), static_cast<ALfloat>(pos_position.Z));
}

void OpenAlSound::StopSource() {
  if (b_done_)
    return;

  ALint int4_state = 0;
  al::alGetSourcei(uint4_source_, AL_SOURCE_STATE, &int4_state);
  if (int4_state == AL_PLAYING || int4_state == AL_PAUSED)
    al::alSourceStop(uint4_source_);
}

void OpenAlSound::Stop() {
  if (b_done_)
    return;

  StopSource();
  al::alSourcei(uint4_source_, AL_BUFFER, 0);
}

void OpenAlSound::SetLooping(bool b_looping) {
  if (b_done_)
    return;

  al::alSourcei(uint4_source_, AL_LOOPING, b_looping ? AL_TRUE : AL_FALSE);
}

// ———— OpenAlAsyncLoadSound(L546-668)———— | OpenAlAsyncLoadSound (L546-668) ————

OpenAlAsyncLoadSound::OpenAlAsyncLoadSound(OpenAlSoundEngine* ptr_engine, ALuint uint4_source,
                                           bool b_looping, bool b_relative, WPos pos_position,
                                           float fp4_volume, int int4_channels,
                                           int int4_sample_bits, int int4_sample_rate,
                                           std::unique_ptr<IPcmStream>&& up_stream)
    : OpenAlSound{ptr_engine, uint4_source, b_looping, b_relative, pos_position, fp4_volume,
                  int4_sample_rate} {
  // 挂静音 buffer:否则部分系统上改源状态(play/pause)会失败
  // (上游注释照抄)。
  // A silent buffer first: without it, changing the source's state
  // (play/pause) fails on some systems (upstream comment).
  static constexpr std::uint8_t arr_silent_data[2] = {0, 0};
  OpenAlSoundSource silent_source{arr_silent_data, int4_channels, int4_sample_bits,
                                  int4_sample_rate};
  al::alSourcei(uint4_source_, AL_BUFFER, static_cast<ALint>(silent_source.Buffer()));

  thread_play_ = std::jthread([this, int4_channels, int4_sample_bits, int4_sample_rate,
                               ptr_stream = std::move(up_stream)]() mutable {
    std::vector<std::uint8_t> vec_data;
    const std::optional<std::size_t> opt_length = ptr_stream->Length();
    if (opt_length.has_value())
      vec_data.reserve(*opt_length);  // 已知长度 → 预留(上游 MemoryStream 容量)| length known → reserve (the MemoryStream capacity)

    bool b_cancelled_during_copy = false;
    {
      std::array<std::uint8_t, 81920> arr_chunk{};  // CopyToAsync 的 81920 缓冲 | the CopyToAsync 81920 buffer
      for (;;) {
        // 取消检查 = 上游 cts.Token 传播(CopyToAsync 抛 TaskCanceled)
        // The cancel check = upstream's cts.Token propagation (CopyToAsync
        // throwing TaskCanceled).
        if (b_cancel_.load(std::memory_order_acquire)) {
          b_cancelled_during_copy = true;
          break;
        }

        const std::size_t int4_read = ptr_stream->Read(arr_chunk);
        if (int4_read == 0)
          break;
        vec_data.insert(vec_data.end(), arr_chunk.data(), arr_chunk.data() + int4_read);
      }
    }

    if (b_cancelled_during_copy) {
      // 提前停止:清理未用 buffer 后退出
      // Stopped early: clean up the unused buffer and exit.
      al::alSourceStop(uint4_source_);
      al::alSourcei(uint4_source_, AL_BUFFER, 0);
      return;
    }

    const float fp4_bytes_per_sample = int4_sample_bits / 8.0f;
    const float fp4_length_in_secs =
        static_cast<float>(vec_data.size()) /
        (int4_channels * fp4_bytes_per_sample * int4_sample_rate);
    {
      OpenAlSoundSource real_source{vec_data, int4_channels, int4_sample_bits, int4_sample_rate};

      // 挂真输入前须先停源、再删静音源
      // The source must stop before attaching the real input and deleting the
      // silent one.
      al::alSourceStop(uint4_source_);
      al::alSourcei(uint4_source_, AL_BUFFER, static_cast<ALint>(real_source.Buffer()));

      {
        std::lock_guard<std::mutex> lock{mtx_};
        if (!b_cancel_.load(std::memory_order_acquire)) {
          // TODO(上游照抄):状态检查与 play/rewind 之间在用户恰好暂停/恢复时存在
          // 竞态窗口;窗口极小、后果轻微,上游选择忽略。
          // TODO (verbatim from upstream): a race window exists between the
          // state check and the play/rewind if the user pauses/resumes at the
          // right moment; the window is tiny and the consequences minor, so
          // upstream ignores it.
          ALint int4_state = 0;
          al::alGetSourcei(uint4_source_, AL_SOURCE_STATE, &int4_state);
          if (int4_state != AL_STOPPED)
            al::alSourcePlay(uint4_source_);
          else {
            // 停止态 = 装载完成前被暂停;rewind 使下次从头发
            // Stopped = paused before loading finished; rewind so the next
            // start plays from the beginning.
            al::alSourceRewind(uint4_source_);
          }
        }
      }

      for (;;) {
        if (b_cancel_.load(std::memory_order_acquire))
          break;

        // 须先查 seek 再查状态:否则音乐可能在状态检查后停止,seek 归零,
        // 白等整轨长度(上游注释照抄)。
        // Seek must be checked before state: otherwise the music may stop
        // right after the state check, seek reads zero, and we wait the full
        // track length (verbatim upstream comment).
        const float fp4_current_seek = SeekPosition();

        ALint int4_state = 0;
        al::alGetSourcei(uint4_source_, AL_SOURCE_STATE, &int4_state);
        if (int4_state == AL_STOPPED)
          break;

        // 等到轨道将尽,至多每秒 60 次防忙等
        // Wait until the track is due to complete, at most 60 times a second
        // to prevent a busy-wait.
        const float fp4_delay_secs =
            std::max(fp4_length_in_secs - fp4_current_seek, 1.0f / 60.0f);
        std::unique_lock<std::mutex> lock{mtx_};
        cv_.wait_for(lock, std::chrono::duration<float>{fp4_delay_secs},
                     [this] { return b_cancel_.load(std::memory_order_acquire); });
      }

      al::alSourcei(uint4_source_, AL_BUFFER, 0);
    }

    b_finished_.store(true, std::memory_order_release);
  });
}

OpenAlAsyncLoadSound::~OpenAlAsyncLoadSound() {
  Stop();
}

void OpenAlAsyncLoadSound::Stop() {
  {
    std::lock_guard<std::mutex> lock{mtx_};
    StopSource();
    b_cancel_.store(true, std::memory_order_release);
  }
  cv_.notify_all();

  if (thread_play_.joinable())
    thread_play_.join();
}

bool OpenAlAsyncLoadSound::Complete() const {
  return b_finished_.load(std::memory_order_acquire);
}

// ———— OpenAlSoundEngine(L24-392)———— | OpenAlSoundEngine (L24-392) ————

OpenAlSoundEngine::OpenAlSoundEngine(std::optional<std::string> str_device_name) {
  if (str_device_name.has_value())
    std::println("Using sound device `{}`", *str_device_name);
  else
    std::println("Using default sound device");

  ptr_device_ = al::alcOpenDevice(str_device_name.has_value() ? str_device_name->c_str() : nullptr);
  if (ptr_device_ == nullptr) {
    std::println("Failed to open device. Falling back to default");
    ptr_device_ = al::alcOpenDevice(nullptr);
    if (ptr_device_ == nullptr)
      throw std::runtime_error{"Can't create OpenAL device"};
  }

  ptr_context_ = al::alcCreateContext(ptr_device_, nullptr);
  if (ptr_context_ == nullptr)
    throw std::runtime_error{"Can't create OpenAL context"};
  al::alcMakeContextCurrent(ptr_context_);

  for (int int4_i = 0; int4_i < kAlPoolSize; int4_i++) {
    ALuint uint4_source = 0;
    al::alGenSources(1, &uint4_source);
    if (al::alGetError() != AL_NO_ERROR) {
      LogSound(std::format("Failed generating OpenAL source {}", int4_i));
      return;
    }

    PoolSlot slot;
    slot.uint4_source = uint4_source;
    vec_slots_.push_back(slot);
  }
}

OpenAlSoundEngine::~OpenAlSoundEngine() {
  StopAllSounds();

  // 先拆后台线程引用,再回收源(D84:调用方声音对象可能长于引擎,回指在此置空)
  // Tear down the background threads' references before recycling the sources
  // (D84: caller-owned sounds may outlive the engine; their back-pointers are
  // nulled here).
  for (OpenAlSound* ptr_sound : vec_live_sounds_) {
    ptr_sound->ptr_engine_ = nullptr;
    ptr_sound->b_done_ = true;
    ptr_sound->uint4_source_ = std::numeric_limits<ALuint>::max();
  }
  vec_live_sounds_.clear();

  if (!vec_slots_.empty()) {
    std::vector<ALuint> vec_ids;
    vec_ids.reserve(vec_slots_.size());
    for (const PoolSlot& slot : vec_slots_)
      vec_ids.push_back(slot.uint4_source);
    al::alDeleteSources(static_cast<ALsizei>(vec_ids.size()), vec_ids.data());
  }
  vec_slots_.clear();

  if (ptr_context_ != nullptr) {
    al::alcMakeContextCurrent(nullptr);
    al::alcDestroyContext(ptr_context_);
    ptr_context_ = nullptr;
  }

  if (ptr_device_ != nullptr) {
    al::alcCloseDevice(ptr_device_);
    ptr_device_ = nullptr;
  }
}

namespace {

std::vector<SoundDevice> AvailableDevicesImpl() {
  std::vector<SoundDevice> vec_devices;
  vec_devices.push_back(SoundDevice{std::nullopt, "Default Output"});
  for (const std::string& str_device : PhysicalDevices())
    vec_devices.push_back(SoundDevice{str_device, str_device});
  return vec_devices;
}

}  // namespace

std::vector<SoundDevice> OpenAlSoundEngine::AvailableDevices() {
  return AvailableDevicesImpl();
}

void OpenAlSoundEngine::SetVolume(float fp4_volume) {
  al::alListenerf(AL_GAIN, fp4_volume);
  fp4_volume_ = fp4_volume;
}

ISoundSource* OpenAlSoundEngine::AddSoundSourceFromMemory(std::span<const std::uint8_t> span_data,
                                                          int int4_channels, int int4_sample_bits,
                                                          int int4_sample_rate) {
  return new OpenAlSoundSource{span_data, int4_channels, int4_sample_bits, int4_sample_rate};
}

bool OpenAlSoundEngine::TryGetSourceFromPool(ALuint& uint4_source) {
  for (PoolSlot& slot : vec_slots_) {
    if (!slot.b_is_active) {
      slot.b_is_active = true;
      uint4_source = slot.uint4_source;
      return true;
    }
  }

  std::vector<ALuint> vec_free_sources;
  for (PoolSlot& slot : vec_slots_) {
    OpenAlSound* ptr_sound = slot.ptr_sound;
    if (ptr_sound != nullptr && ptr_sound->Complete()) {
      const ALuint uint4_free = slot.uint4_source;
      vec_free_sources.push_back(uint4_free);
      al::alSourceRewind(uint4_free);
      al::alSourcei(uint4_free, AL_BUFFER, 0);

      // 源立即复用时也能准确判定原声结束
      // Make sure we can accurately determine the end of the original
      // sound even if the source is immediately reused.
      ptr_sound->UnbindSource();

      slot.ptr_sound_source = nullptr;
      slot.ptr_sound = nullptr;
      slot.b_is_active = false;
    }
  }

  if (vec_free_sources.empty()) {
    uint4_source = 0;
    return false;
  }

  uint4_source = vec_free_sources[0];
  for (PoolSlot& slot : vec_slots_) {
    if (slot.uint4_source == uint4_source) {
      slot.b_is_active = true;
      break;
    }
  }
  return true;
}

std::unique_ptr<ISound> OpenAlSoundEngine::Play2D(ISoundSource* ptr_source, bool b_loop,
                                                  bool b_relative, WPos pos_position,
                                                  float fp4_volume, bool b_attenuate_volume) {
  if (ptr_source == nullptr) {
    LogSound("Attempt to Play2D a null `ISoundSource`");
    return nullptr;
  }

  auto* const ptr_al_source = static_cast<OpenAlSoundSource*>(ptr_source);

  const std::int32_t int4_curr_frame = int4_local_tick_;
  float fp4_atten = 1.0f;

  // 同源/同帧实例上限检查(b_attenuate_volume 分支,判定 = EvaluatePlayGate):
  if (b_attenuate_volume) {
    std::vector<GateProbeSlot> vec_probe;
    vec_probe.reserve(vec_slots_.size());
    for (const PoolSlot& slot : vec_slots_) {
      GateProbeSlot probe;
      probe.b_is_active = slot.b_is_active;
      probe.b_is_relative = slot.b_is_relative;
      probe.uintp_source_id = reinterpret_cast<std::uintptr_t>(slot.ptr_sound_source);
      probe.int4_frame_started = slot.int4_frame_started;
      probe.pos = slot.pos;
      vec_probe.push_back(probe);
    }

    const PlayGateResult gate = EvaluatePlayGate(
        vec_probe, b_relative, reinterpret_cast<std::uintptr_t>(ptr_al_source), pos_position,
        int4_curr_frame);
    if (gate.b_deny)
      return nullptr;
    fp4_atten = gate.fp4_atten;
  }

  ALuint uint4_source = 0;
  if (!TryGetSourceFromPool(uint4_source))
    return nullptr;

  PoolSlot* ptr_slot = nullptr;
  for (PoolSlot& slot : vec_slots_) {
    if (slot.uint4_source == uint4_source) {
      ptr_slot = &slot;
      break;
    }
  }
  ptr_slot->pos = pos_position;
  ptr_slot->int4_frame_started = int4_curr_frame;
  ptr_slot->b_is_relative = b_relative;
  ptr_slot->ptr_sound_source = ptr_al_source;
  auto up_sound = std::make_unique<OpenAlSound>(this, uint4_source, b_loop, b_relative,
                                                pos_position, fp4_volume * fp4_atten,
                                                ptr_al_source->SampleRate(),
                                                ptr_al_source->Buffer());
  ptr_slot->ptr_sound = up_sound.get();
  vec_live_sounds_.push_back(up_sound.get());
  return up_sound;
}

std::unique_ptr<ISound> OpenAlSoundEngine::Play2DStream(std::unique_ptr<IPcmStream>&& up_stream,
                                                        int int4_channels, int int4_sample_bits,
                                                        int int4_sample_rate, bool b_loop,
                                                        bool b_relative, WPos pos_position,
                                                        float fp4_volume) {
  const std::int32_t int4_curr_frame = int4_local_tick_;

  ALuint uint4_source = 0;
  if (!TryGetSourceFromPool(uint4_source))
    return nullptr;

  PoolSlot* ptr_slot = nullptr;
  for (PoolSlot& slot : vec_slots_) {
    if (slot.uint4_source == uint4_source) {
      ptr_slot = &slot;
      break;
    }
  }
  ptr_slot->pos = pos_position;
  ptr_slot->int4_frame_started = int4_curr_frame;
  ptr_slot->b_is_relative = b_relative;
  ptr_slot->ptr_sound_source = nullptr;
  auto up_sound = std::make_unique<OpenAlAsyncLoadSound>(
      this, uint4_source, b_loop, b_relative, pos_position, fp4_volume, int4_channels,
      int4_sample_bits, int4_sample_rate, std::move(up_stream));
  ptr_slot->ptr_sound = up_sound.get();
  vec_live_sounds_.push_back(up_sound.get());
  return up_sound;
}

void OpenAlSoundEngine::PauseSound(ISound* ptr_sound, bool b_paused) {
  if (ptr_sound == nullptr || ptr_sound->Complete())
    return;

  PauseSource(static_cast<OpenAlSound*>(ptr_sound)->Source(), b_paused);
}

void OpenAlSoundEngine::SetAllSoundsPaused(bool b_paused) {
  for (const PoolSlot& slot : vec_slots_)
    PauseSource(slot.uint4_source, b_paused);
}

void OpenAlSoundEngine::PauseSource(ALuint uint4_source, bool b_paused) {
  ALint int4_state = 0;
  al::alGetSourcei(uint4_source, AL_SOURCE_STATE, &int4_state);
  if (b_paused) {
    if (int4_state == AL_PLAYING)
      al::alSourcePause(uint4_source);
    else if (int4_state == AL_INITIAL) {
      // 未开始的声:PLAY→STOP 转入停止态,表示"不要播放"
      // A sound not started yet: PLAY→STOP transitions it to the stopped
      // state, meaning "do not play".
      al::alSourcePlay(uint4_source);
      al::alSourceStop(uint4_source);
    }
  } else if (!b_paused && int4_state != AL_PLAYING)
    al::alSourcePlay(uint4_source);
}

void OpenAlSoundEngine::SetSoundVolume(float fp4_volume, ISound* ptr_music, ISound* ptr_video) {
  const ALuint uint4_music =
      ptr_music != nullptr ? static_cast<OpenAlSound*>(ptr_music)->Source() : 0;
  const ALuint uint4_video =
      ptr_video != nullptr ? static_cast<OpenAlSound*>(ptr_video)->Source() : 0;

  for (const PoolSlot& slot : vec_slots_) {
    ALint int4_state = 0;
    al::alGetSourcei(slot.uint4_source, AL_SOURCE_STATE, &int4_state);
    const bool b_selected = (int4_state == AL_PLAYING || int4_state == AL_PAUSED) &&
                            (ptr_music == nullptr || slot.uint4_source != uint4_music) &&
                            (ptr_video == nullptr || slot.uint4_source != uint4_video);
    if (b_selected)
      al::alSourcef(slot.uint4_source, AL_GAIN, fp4_volume);
  }
}

void OpenAlSoundEngine::StopSound(ISound* ptr_sound) {
  if (ptr_sound != nullptr)
    static_cast<OpenAlSound*>(ptr_sound)->Stop();
}

void OpenAlSoundEngine::StopAllSounds() {
  for (PoolSlot& slot : vec_slots_) {
    if (slot.ptr_sound != nullptr)
      slot.ptr_sound->Stop();
  }
}

void OpenAlSoundEngine::SetListenerPosition(WPos pos_position) {
  // 监听者抬出平面,屏幕中部的声不至于过分定位化(上游注释照抄)
  // Move the listener out of the plane so that sounds near the middle of the
  // screen aren't too positional (verbatim upstream comment).
  al::alListener3f(AL_POSITION, static_cast<ALfloat>(pos_position.X),
                   static_cast<ALfloat>(pos_position.Y),
                   static_cast<ALfloat>(pos_position.Z + 2133));

  const ALfloat arr_orientation[6] = {0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f};
  al::alListenerfv(AL_ORIENTATION, arr_orientation);
  al::alListenerf(AL_METERS_PER_UNIT, 0.01f);
}

void OpenAlSoundEngine::SetSoundLooping(bool b_looping, ISound* ptr_sound) {
  if (ptr_sound != nullptr)
    static_cast<OpenAlSound*>(ptr_sound)->SetLooping(b_looping);
}

void OpenAlSoundEngine::SetSoundPosition(ISound* ptr_sound, WPos pos_position) {
  if (ptr_sound != nullptr)
    static_cast<OpenAlSound*>(ptr_sound)->SetPosition(pos_position);
}

void OpenAlSoundEngine::ForgetSound(OpenAlSound* ptr_sound) {
  for (PoolSlot& slot : vec_slots_) {
    if (slot.ptr_sound == ptr_sound)
      slot.ptr_sound = nullptr;
  }
  const auto iter = std::ranges::find(vec_live_sounds_, ptr_sound);
  if (iter != vec_live_sounds_.end())
    vec_live_sounds_.erase(iter);
}

}  // namespace ora::platform
