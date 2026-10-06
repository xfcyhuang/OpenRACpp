// UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L36-481 + SoundInfo.cs L56-86
//           的验收测试(Phase 4 第十四批)
// 声音门面:格式链分派/源缓存/Play 族/通知池打断策略/音乐与视频面;
// Dummy 引擎(空实现)+ 上游 mods 散装 .aud 实资产。
// The acceptance test of the sound facade (Phase 4 batch 14): the
// format-chain dispatch / the source cache / the Play family / the
// notification-pool interrupt policies / the music and video faces; the
// Dummy engine (no-op) + real loose .aud assets from upstream mods.

#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form
import std;

#include "game/game_records.hpp"
#include "game/ruleset.hpp"
#include "platform/dummy_sound_engine.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sound/sound.hpp"
#include "sound/sound_loader.hpp"
#include "yaml/mini_yaml.hpp"

int int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      int4_failures++;                                                         \
      std::println(stderr, "FAIL {}:{}: {}", __FILE__, __LINE__, #cond);       \
    }                                                                          \
  } while (0)

#define ORA_CHECK_THROWS_TEXT(expr, text_sub)                                  \
  do {                                                                         \
    bool b_caught = false;                                                     \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (const std::exception& e) {                                        \
      b_caught = true;                                                         \
      if (std::string_view{e.what()}.find(text_sub) == std::string_view::npos) { \
        int4_failures++;                                                       \
        std::println(stderr, "FAIL {}:{}: 抛文不含 `{}`: {}", __FILE__, __LINE__, \
                     text_sub, e.what());                                      \
      }                                                                        \
    }                                                                          \
    if (!b_caught) {                                                           \
      int4_failures++;                                                         \
      std::println(stderr, "FAIL {}:{}: 未抛(期望含 `{}`)", __FILE__, __LINE__, \
                   text_sub);                                                  \
    }                                                                          \
  } while (0)

namespace {

namespace game = ora::game;  // 上游 OpenRA.GameRules 命名 | the upstream OpenRA.GameRules names

/// 上游 mods/cnc/bits 挂载(散装 .aud)。
/// Mount upstream mods/cnc/bits (the loose .aud files).
ora::fs::FileSystem MakeBitsFs(const std::string& str_upstream_root) {
  ora::fs::FileSystem file_system{};
  file_system.Mount((std::filesystem::path{str_upstream_root} / "mods" / "cnc" / "bits")
                        .generic_string());
  return file_system;
}

game::SoundInfo MakeSoundInfoFromYaml(std::string_view sv_yaml) {
  const std::vector<ora::yaml::MiniYamlNode> vec_nodes =
      ora::yaml::MiniYaml::FromString(sv_yaml, "sound_test", true,
                                      ora::yaml::MiniYaml::GlobalPool());
  return game::SoundInfo{vec_nodes.front().Value};
}

game::Ruleset MakeMinimalRuleset(std::unique_ptr<game::SoundInfo> up_notifications) {
  std::vector<std::pair<std::string, std::unique_ptr<game::ActorInfo>>> vec_actors;
  std::vector<std::pair<std::string, std::unique_ptr<game::WeaponInfo>>> vec_weapons;
  std::vector<std::pair<std::string, std::unique_ptr<game::SoundInfo>>> vec_voices;
  std::vector<std::pair<std::string, std::unique_ptr<game::SoundInfo>>> vec_notifications;
  vec_notifications.emplace_back("notifications", std::move(up_notifications));
  std::vector<std::pair<std::string, std::unique_ptr<game::MusicInfo>>> vec_music;
  std::vector<std::pair<std::string, ora::yaml::MiniYamlNode>> vec_model_sequences;
  return game::Ruleset{std::move(vec_actors),    std::move(vec_weapons),
                       std::move(vec_voices),    std::move(vec_notifications),
                       std::move(vec_music),     std::move(vec_model_sequences)};
}

void TestLoaderChain() {
  const std::vector<ora::sound::SoundLoaderFn> vec_loaders =
      ora::sound::MakeSoundLoaders({"Aud", "Wav"});
  ORA_CHECK(vec_loaders.size() == 2);
  ORA_CHECK_THROWS_TEXT(ora::sound::MakeSoundLoaders({"Nope"}),
                        "Unable to find a sound loader for type 'Nope'.");

  // 全五名链构造不抛(名字分派表覆盖)
  // The full five-name chain constructs without throwing (the name table
  // covers it).
  const std::vector<ora::sound::SoundLoaderFn> vec_all =
      ora::sound::MakeSoundLoaders({"Ogg", "Mp3", "Voc", "Aud", "Wav"});
  ORA_CHECK(vec_all.size() == 5);
}

void TestSoundFacade(const std::string& str_upstream_root) {
  auto up_engine = std::make_unique<ora::platform::DummySoundEngine>();
  ora::sound::SoundSettingsFace settings_sound{};
  ora::sound::Sound snd_sound{std::move(up_engine), settings_sound};
  ORA_CHECK(snd_sound.DummyEngine());

  ora::fs::FileSystem file_system = MakeBitsFs(str_upstream_root);
  const std::vector<ora::sound::SoundLoaderFn> vec_loaders =
      ora::sound::MakeSoundLoaders({"Aud", "Wav"});
  snd_sound.Initialize(vec_loaders, file_system);

  // 静音构造面 + 音量写读(Dummy 无副作用,断言不崩 + 设置面回读)
  // The mute construction face + volume writes (Dummy has no side effects;
  // assert no crash + the settings face reads back).
  settings_sound.b_mute = true;
  ora::sound::Sound snd_muted{std::make_unique<ora::platform::DummySoundEngine>(),
                              settings_sound};
  snd_muted.Initialize(vec_loaders, file_system);
  ORA_CHECK(snd_muted.DummyEngine());
  settings_sound.b_mute = false;

  snd_sound.SetSoundVolume(0.75f);
  ORA_CHECK(snd_sound.SoundVolume() == 0.75f);
  snd_sound.SetSoundVolumeModifier(2.0f);
  ORA_CHECK(snd_sound.SoundVolumeModifier() == 2.0f);
  snd_sound.SetMusicVolume(0.6f);
  ORA_CHECK(snd_sound.MusicVolume() == 0.6f);
  snd_sound.SetVideoVolume(0.4f);
  ORA_CHECK(snd_sound.VideoVolume() == 0.4f);

  // Play 族:真实 .aud 命中链 → Dummy 返回 NullSound(非空)
  // The Play family: a real .aud hits the chain → the Dummy returns a
  // NullSound (non-null).
  ora::platform::ISound* ptr_played = snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud");
  ORA_CHECK(ptr_played != nullptr);
  // 禁总开关 / 禁世界声 / 空名 → null
  // All-disabled / world-disabled / empty name → null.
  snd_sound.SetDisableAllSounds(true);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud") == nullptr);
  snd_sound.SetDisableAllSounds(false);
  snd_sound.SetDisableWorldSounds(true);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::World, "gun5.aud") == nullptr);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud") != nullptr);
  snd_sound.SetDisableWorldSounds(false);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "") == nullptr);

  // 本地玩家过滤:非本地静默
  // The local-player filter: non-local stays silent.
  ora::sim::Player player_local{"Local", "Local", 0};
  ora::sim::Player player_other{"Other", "Other", 1};
  snd_sound.SetLocalPlayer(&player_local);
  ORA_CHECK(snd_sound.PlayToPlayer(ora::sound::SoundType::UI, &player_local, "gun5.aud") != nullptr);
  ORA_CHECK(snd_sound.PlayToPlayer(ora::sound::SoundType::UI, &player_other, "gun5.aud") == nullptr);
  snd_sound.SetLocalPlayer(nullptr);

  // 缺文件 → 缓存 null 源 → 上游 NRE 等价抛
  // A missing file → the cached null source → upstream's NRE-equivalent
  // throw.
  ORA_CHECK_THROWS_TEXT(snd_sound.Play(ora::sound::SoundType::UI, "missing.aud"),
                        "System.NullReferenceException");

  // 视频声道(Dummy 的 Play2D 产物非空)
  // The video channel (the Dummy's Play2D product is non-null).
  std::array<std::uint8_t, 64> arr_pcm{};
  snd_sound.PlayVideo(arr_pcm, 1, 8, 22050);
  snd_sound.PauseVideo();
  snd_sound.PlayVideo();
  snd_sound.StopVideo();
  ORA_CHECK(snd_sound.VideoSeekPosition() == 0.0f);

  // 循环/位置/停止面(Dummy 无副作用)
  // The loop/position/stop faces (Dummy no-ops).
  ora::platform::ISound* ptr_loop =
      snd_sound.PlayLooped(ora::sound::SoundType::UI, "gun5.aud");
  ORA_CHECK(ptr_loop != nullptr);
  snd_sound.SetLooped(ptr_loop, false);
  snd_sound.SetPosition(ptr_loop, ora::WPos{1, 2, 3});
  snd_sound.StopSound(ptr_loop);
  snd_sound.SetListenerPosition(ora::WPos{0, 0, 0});
  snd_sound.StopAudio();
  const std::vector<ora::platform::SoundDevice> vec_devices = snd_sound.AvailableDevices();
  ORA_CHECK(vec_devices.size() == 1);
}

void TestPlayPredefinedPolicies(const std::string& str_upstream_root) {
  auto up_engine = std::make_unique<ora::platform::DummySoundEngine>();
  ora::sound::SoundSettingsFace settings_sound{};
  ora::sound::Sound snd_sound{std::move(up_engine), settings_sound};
  ora::fs::FileSystem file_system = MakeBitsFs(str_upstream_root);
  snd_sound.Initialize(ora::sound::MakeSoundLoaders({"Aud", "Wav"}), file_system);

  // 单 clip 池(确定性,不依赖随机器取值)。池条目 = 标量 clip + 可选嵌套
  // 选项(上游 notifications.yaml 同形)。
  // Single-clip pools (deterministic, independent of the random draw). A
  // pool entry = the scalar clip + optional nested options (the shape of
  // upstream's notifications.yaml).
  auto up_info = std::make_unique<game::SoundInfo>(MakeSoundInfoFromYaml(
      "notifications:\n"
      "    DefaultVariant: .aud\n"
      "    Notifications:\n"
      "        beep: gun5\n"
      "            InterruptType: Interrupt\n"
      "        hold: gun5\n"
      "            InterruptType: DoNotPlay\n"
      "        over: gun5\n"
      "            InterruptType: Overlap\n"
      "        empty:\n"));
  game::Ruleset ruleset = MakeMinimalRuleset(std::move(up_info));

  // UnknownDefinitions 键缺池 → 文本逐字
  // A definition with no pool → the text verbatim.
  ORA_CHECK_THROWS_TEXT(
      snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                               "notifications", "nosuch", "", true, ora::WPos::Zero(), 1.0f, false),
      "Can't find nosuch in notification pool.");

  // DoNotPlay:首播后 NullSound.Complete 恒假 → 第二次拒绝
  // DoNotPlay: NullSound.Complete stays false → the second call declines.
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "hold", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(!snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                      "notifications", "hold", "", true, ora::WPos::Zero(), 1.0f,
                                      false));

  // Interrupt/Overlap:恒放行
  // Interrupt/Overlap: always admitted.
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "beep", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "beep", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "over", "", true, ora::WPos::Zero(), 1.0f,
                                     false));

  // 空 clip 池(Value 空)→ 拒绝且不抛
  // An empty-clip pool (an empty Value) → declined without throwing.
  ORA_CHECK(!snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                      "notifications", "empty", "", true, ora::WPos::Zero(), 1.0f,
                                      false));

  // PlayNotification:type 小写化后命中
  // PlayNotification: the lower-cased type hits.
  ORA_CHECK(snd_sound.PlayNotification(ruleset, nullptr, "Notifications", "beep", ""));
  ORA_CHECK(!snd_sound.PlayNotification(ruleset, nullptr, "", "beep", ""));
  ORA_CHECK(!snd_sound.PlayNotification(ruleset, nullptr, "notifications", "", ""));

  // DisableAllSounds 前置否决
  // DisableAllSounds vetoes first.
  snd_sound.SetDisableAllSounds(true);
  ORA_CHECK(!snd_sound.PlayNotification(ruleset, nullptr, "notifications", "beep", ""));
  snd_sound.SetDisableAllSounds(false);
}

void TestMusicFaces(const std::string& str_upstream_root) {
  auto up_engine = std::make_unique<ora::platform::DummySoundEngine>();
  ora::sound::SoundSettingsFace settings_sound{};
  ora::sound::Sound snd_sound{std::move(up_engine), settings_sound};
  ora::fs::FileSystem file_system = MakeBitsFs(str_upstream_root);
  const std::vector<ora::sound::SoundLoaderFn> vec_loaders =
      ora::sound::MakeSoundLoaders({"Aud", "Wav"});
  snd_sound.Initialize(vec_loaders, file_system);

  // MusicInfo:手写 yaml(默认扩展 aud → gun5.aud)+ Load
  // MusicInfo: hand-written yaml (the default aud extension → gun5.aud) +
  // Load.
  const std::vector<ora::yaml::MiniYamlNode> vec_nodes = ora::yaml::MiniYaml::FromString(
      "valhalla: Valhalla\n    VolumeModifier: 0.8\n", "music", true,
      ora::yaml::MiniYaml::GlobalPool());
  game::MusicInfo info_music{"valhalla", vec_nodes.front().Value};
  ORA_CHECK(info_music.Filename() == "valhalla.aud");
  ORA_CHECK(info_music.Title() == "Valhalla");
  ORA_CHECK(std::abs(info_music.VolumeModifier() - 0.8f) < 1e-6f);
  ORA_CHECK(!info_music.Exists());
  snd_sound.LoadMusicInfo(info_music);  // 缺文件:Exists 保持假 | missing file: Exists stays false
  ORA_CHECK(!info_music.Exists());

  game::MusicInfo info_gun{"gun5", ora::yaml::MiniYaml::FromString("gun5:\n", "music", true,
                                                                   ora::yaml::MiniYaml::GlobalPool())
                                         .front()
                                         .Value};
  ORA_CHECK(info_gun.Filename() == "gun5.aud");
  snd_sound.LoadMusicInfo(info_gun);
  ORA_CHECK(info_gun.Exists());
  ORA_CHECK(info_gun.Length() > 0);

  // Dummy 的 Play2DStream 恒 null → PlayMusic 走 onMusicComplete 置空分支
  // The Dummy's Play2DStream is always null → PlayMusic takes the
  // onMusicComplete-cleared branch.
  bool b_then_ran = false;
  snd_sound.PlayMusicThen(info_gun, [&] { b_then_ran = true; });
  ORA_CHECK(!snd_sound.MusicPlaying());
  ORA_CHECK(snd_sound.CurrentMusic() == nullptr);
  ORA_CHECK(!b_then_ran);
  snd_sound.Tick();  // music == null:歌毕分支不触 | music == null: the finished branch stays cold.
  ORA_CHECK(!b_then_ran);

  // PlayMusic/Stop/Pause 在 null music 上的幂等面
  // The idempotent faces over null music.
  snd_sound.PlayMusic();
  snd_sound.PauseMusic();
  snd_sound.StopMusic();
  ORA_CHECK(snd_sound.MusicSeekPosition() == 0.0f);
  snd_sound.SetMusicLooped(true);
  ORA_CHECK(settings_sound.b_repeat);
}

void TestSoundPoolDraw() {
  // SoundPool 抽干回填:单种子随机器下连续 GetNext 覆盖全部 clip 后回填
  // SoundPool's refill-on-drain: with one seeded twister, successive
  // GetNext draws cover every clip then refill.
  ora::MersenneTwister mt{42};
  game::SoundPool pool_a{1.0f, game::SoundPool::InterruptType::Overlap, {"x", "y"}};
  ORA_CHECK(pool_a.VolumeModifier() == 1.0f);
  ORA_CHECK(pool_a.Type() == game::SoundPool::InterruptType::Overlap);
  std::array<std::string, 2> arr_draws{pool_a.GetNext(mt), pool_a.GetNext(mt)};
  ORA_CHECK((arr_draws[0] == "x" || arr_draws[0] == "y"));
  ORA_CHECK((arr_draws[1] == "x" || arr_draws[1] == "y"));
  ORA_CHECK(arr_draws[0] != arr_draws[1]);
  const std::string str_third = pool_a.GetNext(mt);  // 回填后再抽 | a draw after the refill
  ORA_CHECK(!str_third.empty());

  game::SoundPool pool_empty{1.0f, game::SoundPool::kDefaultInterruptType, {}};
  ORA_CHECK(pool_empty.GetNext(mt).empty());  // 零 clips 防崩 | zero clips: no crash
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::println(stderr, "用法 | usage: sound_test <upstream-root>");
    return 2;
  }
  const std::string str_upstream_root = argv[1];

  TestLoaderChain();
  TestSoundFacade(str_upstream_root);
  TestPlayPredefinedPolicies(str_upstream_root);
  TestMusicFaces(str_upstream_root);
  TestSoundPoolDraw();

  if (int4_failures != 0) {
    std::println(stderr, "sound_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("sound_test: 全部通过 | all passed");
  return 0;
}
