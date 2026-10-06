// UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L36-481(sound.hpp 的实现)
//          Implementation of sound.hpp.
// 形态偏离(COVERAGE 登记):文件缺失日志(Log.Write "sound")不落地(同
// D70 的日志面);sounds[name] 为 null 源时上游 Play2D(null) 即 NRE,此处
// 以同文本 runtime_error 等价抛;LoadSound 的 "not a valid sound file!"
// 逐字。
// Registered shape deviations: the missing-file log (Log.Write "sound") is
// not ported (D70's log face); a null sounds[name] hits upstream's
// Play2D(null) NRE, thrown here as a runtime_error with the same text;
// LoadSound's "not a valid sound file!" verbatim.
import std;
#include "sound/sound.hpp"

namespace ora::sound {

namespace {

/// 内存 PCM → IPcmStream(Play2DStream 的 Stream 参数形态)。
/// In-memory PCM → IPcmStream (the Stream-parameter shape of Play2DStream).
class VectorPcmStream final : public platform::IPcmStream {
 public:
  explicit VectorPcmStream(std::vector<std::byte> vec_pcm) : vec_pcm_{std::move(vec_pcm)} {}

  std::optional<std::size_t> Length() const override { return vec_pcm_.size(); }

  std::size_t Read(std::span<std::uint8_t> span_dst) override {
    const std::size_t st_take = std::min(span_dst.size(), vec_pcm_.size() - st_pos_);
    std::memcpy(span_dst.data(), vec_pcm_.data() + st_pos_, st_take);
    st_pos_ += st_take;
    return st_take;
  }

 private:
  std::vector<std::byte> vec_pcm_;
  std::size_t st_pos_ = 0;
};

std::span<const std::byte> AsBytes(const std::vector<char>& vec_chars) {
  return std::span<const std::byte>{reinterpret_cast<const std::byte*>(vec_chars.data()),
                                    vec_chars.size()};
}

/// 缺源 = 上游 Play2D(null, …) 的 NRE 等价抛。
/// A missing source = the equivalent throw of upstream's Play2D(null, ...)
/// NRE.
[[noreturn]] void ThrowNullSource() {
  throw std::runtime_error(
      "System.NullReferenceException: Object reference not set to an instance of an object.");
}

/// Ruleset 声音表按名查找(上游 ruleset.Voices[type] 的 null-语义)。
/// The by-name lookup of a Ruleset sound table (upstream's
/// ruleset.Voices[type] null semantics).
const game::SoundInfo* FindSoundInfo(
    const std::vector<std::pair<std::string, std::unique_ptr<game::SoundInfo>>>& vec_table,
    std::string_view str_key) {
  for (const auto& [str_k, up_info] : vec_table)
    if (str_k == str_key)
      return up_info.get();
  return nullptr;
}

const game::SoundPool* FindPool(
    const std::vector<std::pair<std::string, game::SoundPool>>& vec_pools,
    std::string_view str_key) {
  for (const auto& [str_k, pool_p] : vec_pools)
    if (str_k == str_key)
      return &pool_p;
  return nullptr;
}

}  // namespace

Sound::Sound(std::unique_ptr<platform::ISoundEngine> up_engine, SoundSettingsFace& settings_sound)
    : up_engine_{std::move(up_engine)},
      settings_{settings_sound},
      // Game.CosmeticRandom = new MersenneTwister()(TickCount 种子)
      // Game.CosmeticRandom = new MersenneTwister() (the TickCount seed).
      mt_cosmetic_{static_cast<std::int32_t>(
          std::chrono::steady_clock::now().time_since_epoch().count())} {
  b_dummy_engine_ = up_engine_->Dummy();

  if (settings_.b_mute)
    MuteAudio();
}

Sound::~Sound() {
  // Dispose(L471-479)
  StopAudio();
  for (const auto& [str_name_ignored, ptr_source] : vec_sources_)
    delete ptr_source;
  // 引擎 unique_ptr 析构 = 上游 soundEngine.Dispose()
  // The engine's unique_ptr destructor = upstream's soundEngine.Dispose().
}

void Sound::Initialize(std::span<const SoundLoaderFn> vec_loaders, fs::FileSystem& file_system) {
  StopMusic();
  up_engine_->StopAllSounds();

  for (const auto& [str_name_ignored, ptr_source] : vec_sources_)
    delete ptr_source;
  vec_sources_.clear();

  vec_loaders_.assign(vec_loaders.begin(), vec_loaders.end());
  ptr_file_system_ = &file_system;
  vec_current_sounds_.clear();
  vec_current_notifications_.clear();
  up_video_.reset();
}

std::vector<platform::SoundDevice> Sound::AvailableDevices() {
  return up_engine_->AvailableDevices();
}

void Sound::SetListenerPosition(WPos pos_position) {
  up_engine_->SetListenerPosition(pos_position);
}

void Sound::StopAudio() { up_engine_->StopAllSounds(); }

void Sound::SetLooped(platform::ISound* ptr_sound, bool b_looped) {
  up_engine_->SetSoundLooping(b_looped, ptr_sound);
}

void Sound::SetPosition(platform::ISound* ptr_sound, WPos pos_position) {
  up_engine_->SetSoundPosition(ptr_sound, pos_position);
}

void Sound::MuteAudio() { up_engine_->SetVolume(0.0f); }

void Sound::UnmuteAudio() { up_engine_->SetVolume(1.0f); }

void Sound::SetMusicLooped(bool b_loop) {
  settings_.b_repeat = b_loop;
  up_engine_->SetSoundLooping(b_loop, up_music_.get());
}

platform::ISoundSource* Sound::GetOrLoadSource(const std::string& str_filename) {
  for (const auto& [str_k, ptr_source] : vec_sources_)
    if (str_k == str_filename)
      return ptr_source;

  // LoadSound(L60-83):缺文件 → null 源(上游日志面不落地);文件在而无
  // 加载器命中 → "not a valid sound file!" 逐字
  // LoadSound (L60-83): a missing file → the null source (upstream's log
  // face not ported); a present file with no loader hit → the "not a valid
  // sound file!" verbatim.
  platform::ISoundSource* ptr_source = nullptr;
  if (ptr_file_system_->Exists(str_filename)) {
    const std::vector<char> vec_chars = ptr_file_system_->Open(str_filename);
    const std::span<const std::byte> vec_file = AsBytes(vec_chars);

    // 链内首查(stream.Position = 0 每轮重放 = 同一字节视图)
    // The first chain hit (stream.Position = 0 replays the same byte view
    // per round).
    for (const SoundLoaderFn fn_loader : vec_loaders_) {
      SoundFormatInfo info_format{};
      std::vector<std::byte> vec_pcm;
      if (fn_loader(vec_file, info_format, vec_pcm)) {
        ptr_source = up_engine_->AddSoundSourceFromMemory(
            std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(vec_pcm.data()),
                                          vec_pcm.size()},
            info_format.int4_channels, info_format.int4_sample_bits,
            info_format.int4_sample_rate);
        break;
      }
    }

    if (ptr_source == nullptr)
      throw std::runtime_error(str_filename + " is not a valid sound file!");
  }

  vec_sources_.emplace_back(str_filename, ptr_source);
  return ptr_source;
}

platform::ISound* Sound::PlayInternal(SoundType kind_type, const sim::Player* ptr_player,
                                      const std::string& str_name, bool b_head_relative,
                                      WPos pos_position, float fp4_volume_modifier, bool b_loop) {
  if (str_name.empty() || b_disable_all_sounds_ ||
      (b_disable_world_sounds_ && kind_type == SoundType::World))
    return nullptr;

  // player != player.World.LocalPlayer 静默(null player 恒放)
  // player != player.World.LocalPlayer stays silent (a null player always
  // plays).
  if (ptr_player != nullptr && ptr_player != ptr_local_player_)
    return nullptr;

  platform::ISoundSource* ptr_source = GetOrLoadSource(str_name);
  if (ptr_source == nullptr)
    ThrowNullSource();

  // 产物入所有权表,返回非拥有引用(上游 GC 等价)
  // The product joins the ownership list; the caller gets a non-owning
  // reference (the GC equivalent).
  vec_playing_.push_back(up_engine_->Play2D(ptr_source, b_loop, b_head_relative, pos_position,
                                            InternalSoundVolume() * fp4_volume_modifier, true));
  return vec_playing_.back().get();
}

platform::ISound* Sound::Play(SoundType kind_type, const std::string& str_name) {
  return PlayInternal(kind_type, nullptr, str_name, true, WPos::Zero());
}

platform::ISound* Sound::Play(SoundType kind_type, const std::string& str_name, WPos pos_position) {
  return PlayInternal(kind_type, nullptr, str_name, false, pos_position);
}

platform::ISound* Sound::Play(SoundType kind_type, const std::string& str_name,
                              float fp4_volume_modifier) {
  return PlayInternal(kind_type, nullptr, str_name, true, WPos::Zero(), fp4_volume_modifier);
}

platform::ISound* Sound::Play(SoundType kind_type, const std::string& str_name, WPos pos_position,
                              float fp4_volume_modifier) {
  return PlayInternal(kind_type, nullptr, str_name, false, pos_position, fp4_volume_modifier);
}

platform::ISound* Sound::PlayToPlayer(SoundType kind_type, const sim::Player* ptr_player,
                                      const std::string& str_name) {
  return PlayInternal(kind_type, ptr_player, str_name, true, WPos::Zero());
}

platform::ISound* Sound::PlayToPlayer(SoundType kind_type, const sim::Player* ptr_player,
                                      const std::string& str_name, WPos pos_position) {
  return PlayInternal(kind_type, ptr_player, str_name, false, pos_position);
}

platform::ISound* Sound::PlayLooped(SoundType kind_type, const std::string& str_name) {
  return PlayInternal(kind_type, nullptr, str_name, true, WPos::Zero(), 1.0f, true);
}

platform::ISound* Sound::PlayLooped(SoundType kind_type, const std::string& str_name,
                                    WPos pos_position) {
  return PlayInternal(kind_type, nullptr, str_name, false, pos_position, 1.0f, true);
}

platform::ISound* Sound::Play(SoundType kind_type, std::span<const std::string> vec_names,
                              sim::World& world_sim, const sim::Player* ptr_player,
                              float fp4_volume_modifier) {
  // names.Random(world.LocalRandom):空表 = 上游 Random 的 ArgumentException
  // names.Random(world.LocalRandom): an empty list = upstream Random's
  // ArgumentException.
  if (vec_names.empty())
    throw std::runtime_error("System.ArgumentException: Collection must not be empty. (Parameter 'names')");
  const std::string& str_name =
      vec_names[static_cast<std::size_t>(
          world_sim.LocalRandom().Next(static_cast<std::int32_t>(vec_names.size())))];
  return PlayInternal(kind_type, ptr_player, str_name, true, WPos::Zero(), fp4_volume_modifier);
}

platform::ISound* Sound::Play(SoundType kind_type, std::span<const std::string> vec_names,
                              sim::World& world_sim, WPos pos_position,
                              const sim::Player* ptr_player, float fp4_volume_modifier) {
  if (vec_names.empty())
    throw std::runtime_error("System.ArgumentException: Collection must not be empty. (Parameter 'names')");
  const std::string& str_name =
      vec_names[static_cast<std::size_t>(
          world_sim.LocalRandom().Next(static_cast<std::int32_t>(vec_names.size())))];
  return PlayInternal(kind_type, ptr_player, str_name, false, pos_position, fp4_volume_modifier);
}

platform::ISound* Sound::PlayFromPcm(const SoundFormatInfo& info_format,
                                     std::span<const std::byte> vec_pcm, float fp4_volume) {
  std::vector<std::byte> vec_owned{vec_pcm.begin(), vec_pcm.end()};
  vec_playing_.push_back(up_engine_->Play2DStream(
      std::make_unique<VectorPcmStream>(std::move(vec_owned)), info_format.int4_channels,
      info_format.int4_sample_bits, info_format.int4_sample_rate, false, true, WPos::Zero(),
      fp4_volume));
  return vec_playing_.back().get();
}

void Sound::PlayVideo(std::span<const std::uint8_t> vec_raw, int int4_channels, int int4_sample_bits,
                      int int4_sample_rate) {
  StopVideo();
  ptr_video_source_ =
      up_engine_->AddSoundSourceFromMemory(vec_raw, int4_channels, int4_sample_bits, int4_sample_rate);
  up_video_ = up_engine_->Play2D(ptr_video_source_, false, true, WPos::Zero(),
                                 InternalSoundVolume(), false);
}

void Sound::PlayVideo() {
  if (up_video_ != nullptr)
    up_engine_->PauseSound(up_video_.get(), false);
}

void Sound::PauseVideo() {
  if (up_video_ != nullptr)
    up_engine_->PauseSound(up_video_.get(), true);
}

void Sound::StopVideo() {
  if (up_video_ != nullptr) {
    up_engine_->StopSound(up_video_.get());
    delete ptr_video_source_;
    ptr_video_source_ = nullptr;
    up_video_.reset();
  }
}

void Sound::Tick() {
  // 歌毕(onMusicComplete 在 StopMusic 前捕获)
  // Song finished (onMusicComplete captured before StopMusic clears it).
  if (b_music_playing_ && up_music_ != nullptr && up_music_->Complete()) {
    std::function<void()> fn_then = fn_on_music_complete_;
    StopMusic();
    fn_then();
  }
}

void Sound::PlayMusicThen(game::MusicInfo& info_music, std::function<void()> fn_then) {
  if (!info_music.Exists())
    return;

  fn_on_music_complete_ = std::move(fn_then);

  if (&info_music == ptr_current_music_ && up_music_ != nullptr) {
    up_engine_->PauseSound(up_music_.get(), false);
    b_music_playing_ = true;
    return;
  }

  PlayMusic(info_music, settings_.b_repeat);
}

void Sound::PlayMusic(game::MusicInfo& info_music, bool b_looped) {
  if (!info_music.Exists())
    return;

  StopMusic();

  // Stream 委托:文件 → 首查格式 → Play2DStream(音量 = Music × 曲目修正)
  // The Stream delegate: file → the first format hit → Play2DStream (the
  // volume = Music × the track's modifier).
  platform::ISound* ptr_music = nullptr;
  bool b_loader_hit = false;
  if (ptr_file_system_->Exists(info_music.Filename())) {
    const std::vector<char> vec_chars = ptr_file_system_->Open(info_music.Filename());
    const std::span<const std::byte> vec_file = AsBytes(vec_chars);
    for (const SoundLoaderFn fn_loader : vec_loaders_) {
      SoundFormatInfo info_format{};
      std::vector<std::byte> vec_pcm;
      if (fn_loader(vec_file, info_format, vec_pcm)) {
        b_loader_hit = true;
        // 引擎可返回 null(Dummy 的 Play2DStream 恒 null)= 上游 music=null
        // An engine may return null (the Dummy's Play2DStream is always
        // null) = upstream's music == null.
        ptr_music = up_engine_
                        ->Play2DStream(std::make_unique<VectorPcmStream>(std::move(vec_pcm)),
                                       info_format.int4_channels, info_format.int4_sample_bits,
                                       info_format.int4_sample_rate, b_looped, true, WPos::Zero(),
                                       settings_.fp4_music_volume *
                                           info_music.VolumeModifier())
                        .release();
        break;
      }
    }
    if (!b_loader_hit)
      throw std::runtime_error(info_music.Filename() + " is not a valid sound file!");
  } else {
    // 缺文件 = LoadSound 的 null 分支 → music == null → onMusicComplete 置空
    // A missing file = LoadSound's null branch → music == null →
    // onMusicComplete cleared.
  }

  if (ptr_music == nullptr) {
    fn_on_music_complete_ = nullptr;
    return;
  }

  up_music_.reset(ptr_music);
  ptr_current_music_ = &info_music;
  b_music_playing_ = true;
}

void Sound::PlayMusic() {
  if (up_music_ == nullptr)
    return;

  b_music_playing_ = true;
  up_engine_->PauseSound(up_music_.get(), false);
}

void Sound::StopSound(platform::ISound* ptr_sound) {
  if (ptr_sound != nullptr)
    up_engine_->StopSound(ptr_sound);
}

void Sound::StopMusic() {
  if (up_music_ != nullptr) {
    up_engine_->StopSound(up_music_.get());
    up_music_.reset();
  }

  ptr_current_music_ = nullptr;
  b_music_playing_ = false;
}

void Sound::PauseMusic() {
  if (up_music_ == nullptr)
    return;

  b_music_playing_ = false;
  up_engine_->PauseSound(up_music_.get(), true);
}

void Sound::SetSoundVolumeModifier(float fp4_value) {
  fp4_sound_volume_modifier_ = fp4_value;
  up_engine_->SetSoundVolume(InternalSoundVolume(), up_music_.get(), up_video_.get());
}

void Sound::SetSoundVolume(float fp4_value) {
  settings_.fp4_sound_volume = fp4_value;
  up_engine_->SetSoundVolume(InternalSoundVolume(), up_music_.get(), up_video_.get());
}

void Sound::SetMusicVolume(float fp4_value) {
  settings_.fp4_music_volume = fp4_value;
  if (up_music_ != nullptr)
    up_music_->SetVolume(fp4_value);
}

void Sound::SetVideoVolume(float fp4_value) {
  settings_.fp4_video_volume = fp4_value;
  if (up_video_ != nullptr)
    up_video_->SetVolume(fp4_value);
}

float Sound::MusicSeekPosition() const {
  return up_music_ != nullptr ? up_music_->SeekPosition() : 0.0f;
}

float Sound::VideoSeekPosition() const {
  return up_video_ != nullptr ? up_video_->SeekPosition() : 0.0f;
}

bool Sound::PlayPredefined(SoundType kind_type, const game::Ruleset& ruleset,
                           const sim::Player* ptr_player, const sim::Actor* actor_voiced,
                           std::string_view str_type, std::string_view str_definition,
                           std::string_view str_variant, bool b_relative, WPos pos_position,
                           float fp4_volume_modifier, bool b_attenuate_volume) {
  // ArgumentNullException.ThrowIfNull(ruleset):C++ 引用不可空,天然满足
  // ArgumentNullException.ThrowIfNull(ruleset): a C++ reference is never
  // null, satisfied by construction.
  if (str_definition.empty() || b_disable_all_sounds_ ||
      (b_disable_world_sounds_ && kind_type == SoundType::World))
    return false;

  // ruleset.Voices/Notifications null 检查 = 上游死分支(构造恒非空)
  // The ruleset.Voices/Notifications null check = upstream's dead branch
  // (construction never leaves them null).

  const game::SoundInfo* info_rules =
      actor_voiced != nullptr
          ? FindSoundInfo(ruleset.Voices(), str_type)
          : FindSoundInfo(ruleset.Notifications(), str_type);
  if (info_rules == nullptr)
    return false;

  const std::uint32_t uint4_id = actor_voiced != nullptr ? actor_voiced->ActorID() : 0;

  const game::SoundPool* pool_p = nullptr;

  std::string str_suffix = info_rules->str_defaultVariant;
  std::string str_prefix = info_rules->str_defaultPrefix;

  if (actor_voiced != nullptr) {
    const game::SoundPool* pool_found = FindPool(info_rules->VoicePools(), str_definition);
    if (pool_found == nullptr)
      throw std::runtime_error(std::format("Can't find {} in voice pool.", str_definition));

    pool_p = pool_found;
  } else {
    const game::SoundPool* pool_found =
        FindPool(info_rules->NotificationsPools(), str_definition);
    if (pool_found == nullptr)
      throw std::runtime_error(
          std::format("Can't find {} in notification pool.", str_definition));

    pool_p = pool_found;
  }

  const std::string str_clip = pool_p->GetNext(mt_cosmetic_);
  if (str_clip.empty())
    return false;

  if (!str_variant.empty()) {
    if (const std::vector<std::string>* vec_v = info_rules->FindVariants(str_variant);
        vec_v != nullptr && !info_rules->DisableVariantsContains(str_definition))
      str_suffix = (*vec_v)[static_cast<std::size_t>(uint4_id % vec_v->size())];
    if (const std::vector<std::string>* vec_p = info_rules->FindPrefixes(str_variant);
        vec_p != nullptr && !info_rules->DisablePrefixesContains(str_definition))
      str_prefix = (*vec_p)[static_cast<std::size_t>(uint4_id % vec_p->size())];
  }

  const std::string str_name = str_prefix + str_clip + str_suffix;
  const std::uint32_t uint4_actor_id =
      actor_voiced != nullptr && fn_selection_contains_ != nullptr &&
              fn_selection_contains_(*actor_voiced)
          ? 0
          : uint4_id;

  if (!str_name.empty() && (ptr_player == nullptr || ptr_player == ptr_local_player_)) {
    const auto fn_play_sound = [&]() -> platform::ISound* {
      const float fp4_volume =
          InternalSoundVolume() * fp4_volume_modifier * pool_p->VolumeModifier();
      platform::ISoundSource* ptr_source = GetOrLoadSource(str_name);
      if (ptr_source == nullptr)
        ThrowNullSource();
      vec_playing_.push_back(up_engine_->Play2D(ptr_source, false, b_relative, pos_position,
                                                fp4_volume, b_attenuate_volume));
      return vec_playing_.back().get();
    };

    if (pool_p->Type() == game::SoundPool::InterruptType::Overlap) {
      if (fn_play_sound() == nullptr)
        return false;
    } else if (actor_voiced == nullptr) {
      platform::ISound* ptr_current = nullptr;
      for (const auto& [str_k, ptr_s] : vec_current_notifications_)
        if (str_k == str_name) {
          ptr_current = ptr_s;
          break;
        }

      if (ptr_current != nullptr && !ptr_current->Complete()) {
        if (pool_p->Type() == game::SoundPool::InterruptType::Interrupt)
          up_engine_->StopSound(ptr_current);
        else if (pool_p->Type() == game::SoundPool::InterruptType::DoNotPlay)
          return false;
      }

      platform::ISound* ptr_sound = fn_play_sound();
      if (ptr_sound == nullptr)
        return false;

      // currentNotifications[name] = sound(旧项让位)
      // currentNotifications[name] = sound (the old entry yields).
      bool b_replaced = false;
      for (auto& [str_k, ptr_s] : vec_current_notifications_)
        if (str_k == str_name) {
          ptr_s = ptr_sound;
          b_replaced = true;
          break;
        }
      if (!b_replaced)
        vec_current_notifications_.emplace_back(str_name, ptr_sound);
    } else {
      platform::ISound* ptr_current = nullptr;
      for (const auto& [uint4_k, ptr_s] : vec_current_sounds_)
        if (uint4_k == uint4_actor_id) {
          ptr_current = ptr_s;
          break;
        }

      if (ptr_current != nullptr && !ptr_current->Complete()) {
        if (pool_p->Type() == game::SoundPool::InterruptType::Interrupt)
          up_engine_->StopSound(ptr_current);
        else if (pool_p->Type() == game::SoundPool::InterruptType::DoNotPlay)
          return false;
      }

      platform::ISound* ptr_sound = fn_play_sound();
      if (ptr_sound == nullptr)
        return false;

      bool b_replaced = false;
      for (auto& [uint4_k, ptr_s] : vec_current_sounds_)
        if (uint4_k == uint4_actor_id) {
          ptr_s = ptr_sound;
          b_replaced = true;
          break;
        }
      if (!b_replaced)
        vec_current_sounds_.emplace_back(uint4_actor_id, ptr_sound);
    }
  }

  return true;
}

bool Sound::PlayNotification(const game::Ruleset& ruleset, const sim::Player* ptr_player,
                             std::string_view str_type, std::string_view str_notification,
                             std::string_view str_variant) {
  if (str_type.empty() || str_notification.empty())
    return false;

  // type.ToLowerInvariant(ASCII 域)
  // type.ToLowerInvariant (the ASCII domain).
  std::string str_type_lower{str_type};
  std::ranges::transform(str_type_lower, str_type_lower.begin(),
                         [](unsigned char chr_c) { return static_cast<char>(std::tolower(chr_c)); });

  return PlayPredefined(SoundType::UI, ruleset, ptr_player, nullptr, str_type_lower,
                        str_notification, str_variant, true, WPos::Zero(), 1.0f, false);
}

void Sound::LoadMusicInfo(game::MusicInfo& info_music) {
  info_music.Load(
      [&](const std::string& str_filename) {
        return ptr_file_system_ != nullptr && ptr_file_system_->Exists(str_filename);
      },
      [&](game::MusicInfo& info_target) {
        const std::vector<char> vec_chars = ptr_file_system_->Open(info_target.Filename());
        const std::span<const std::byte> vec_file = AsBytes(vec_chars);
        for (const SoundLoaderFn fn_loader : vec_loaders_) {
          SoundFormatInfo info_format{};
          std::vector<std::byte> vec_pcm_ignored;
          if (fn_loader(vec_file, info_format, vec_pcm_ignored)) {
            // Length = (int)LengthInSeconds(向零截断)
            // Length = (int)LengthInSeconds (truncation towards zero).
            info_target.SetLength(
                static_cast<std::int32_t>(info_format.fp4_length_in_seconds));
            break;
          }
        }
      });
}

}  // namespace ora::sound
