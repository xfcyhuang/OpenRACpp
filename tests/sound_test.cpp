#include <cstdio>
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

namespace game = ora::game;

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

  ora::platform::ISound* ptr_played = snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud");
  ORA_CHECK(ptr_played != nullptr);

  snd_sound.SetDisableAllSounds(true);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud") == nullptr);
  snd_sound.SetDisableAllSounds(false);
  snd_sound.SetDisableWorldSounds(true);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::World, "gun5.aud") == nullptr);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "gun5.aud") != nullptr);
  snd_sound.SetDisableWorldSounds(false);
  ORA_CHECK(snd_sound.Play(ora::sound::SoundType::UI, "") == nullptr);

  ora::sim::Player player_local{"Local", "Local", 0};
  ora::sim::Player player_other{"Other", "Other", 1};
  snd_sound.SetLocalPlayer(&player_local);
  ORA_CHECK(snd_sound.PlayToPlayer(ora::sound::SoundType::UI, &player_local, "gun5.aud") != nullptr);
  ORA_CHECK(snd_sound.PlayToPlayer(ora::sound::SoundType::UI, &player_other, "gun5.aud") == nullptr);
  snd_sound.SetLocalPlayer(nullptr);

  ORA_CHECK_THROWS_TEXT(snd_sound.Play(ora::sound::SoundType::UI, "missing.aud"),
                        "System.NullReferenceException");

  std::array<std::uint8_t, 64> arr_pcm{};
  snd_sound.PlayVideo(arr_pcm, 1, 8, 22050);
  snd_sound.PauseVideo();
  snd_sound.PlayVideo();
  snd_sound.StopVideo();
  ORA_CHECK(snd_sound.VideoSeekPosition() == 0.0f);

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

  ORA_CHECK_THROWS_TEXT(
      snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                               "notifications", "nosuch", "", true, ora::WPos::Zero(), 1.0f, false),
      "Can't find nosuch in notification pool.");

  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "hold", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(!snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                      "notifications", "hold", "", true, ora::WPos::Zero(), 1.0f,
                                      false));

  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "beep", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "beep", "", true, ora::WPos::Zero(), 1.0f,
                                     false));
  ORA_CHECK(snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                     "notifications", "over", "", true, ora::WPos::Zero(), 1.0f,
                                     false));

  ORA_CHECK(!snd_sound.PlayPredefined(ora::sound::SoundType::UI, ruleset, nullptr, nullptr,
                                      "notifications", "empty", "", true, ora::WPos::Zero(), 1.0f,
                                      false));

  ORA_CHECK(snd_sound.PlayNotification(ruleset, nullptr, "Notifications", "beep", ""));
  ORA_CHECK(!snd_sound.PlayNotification(ruleset, nullptr, "", "beep", ""));
  ORA_CHECK(!snd_sound.PlayNotification(ruleset, nullptr, "notifications", "", ""));

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

  const std::vector<ora::yaml::MiniYamlNode> vec_nodes = ora::yaml::MiniYaml::FromString(
      "valhalla: Valhalla\n    VolumeModifier: 0.8\n", "music", true,
      ora::yaml::MiniYaml::GlobalPool());
  game::MusicInfo info_music{"valhalla", vec_nodes.front().Value};
  ORA_CHECK(info_music.Filename() == "valhalla.aud");
  ORA_CHECK(info_music.Title() == "Valhalla");
  ORA_CHECK(std::abs(info_music.VolumeModifier() - 0.8f) < 1e-6f);
  ORA_CHECK(!info_music.Exists());
  snd_sound.LoadMusicInfo(info_music);
  ORA_CHECK(!info_music.Exists());

  game::MusicInfo info_gun{"gun5", ora::yaml::MiniYaml::FromString("gun5:\n", "music", true,
                                                                   ora::yaml::MiniYaml::GlobalPool())
                                         .front()
                                         .Value};
  ORA_CHECK(info_gun.Filename() == "gun5.aud");
  snd_sound.LoadMusicInfo(info_gun);
  ORA_CHECK(info_gun.Exists());
  ORA_CHECK(info_gun.Length() > 0);

  bool b_then_ran = false;
  snd_sound.PlayMusicThen(info_gun, [&] { b_then_ran = true; });
  ORA_CHECK(!snd_sound.MusicPlaying());
  ORA_CHECK(snd_sound.CurrentMusic() == nullptr);
  ORA_CHECK(!b_then_ran);
  snd_sound.Tick();
  ORA_CHECK(!b_then_ran);

  snd_sound.PlayMusic();
  snd_sound.PauseMusic();
  snd_sound.StopMusic();
  ORA_CHECK(snd_sound.MusicSeekPosition() == 0.0f);
  snd_sound.SetMusicLooped(true);
  ORA_CHECK(settings_sound.b_repeat);
}

void TestSoundPoolDraw() {

  ora::MersenneTwister mt{42};
  game::SoundPool pool_a{1.0f, game::SoundPool::InterruptType::Overlap, {"x", "y"}};
  ORA_CHECK(pool_a.VolumeModifier() == 1.0f);
  ORA_CHECK(pool_a.Type() == game::SoundPool::InterruptType::Overlap);
  std::array<std::string, 2> arr_draws{pool_a.GetNext(mt), pool_a.GetNext(mt)};
  ORA_CHECK((arr_draws[0] == "x" || arr_draws[0] == "y"));
  ORA_CHECK((arr_draws[1] == "x" || arr_draws[1] == "y"));
  ORA_CHECK(arr_draws[0] != arr_draws[1]);
  const std::string str_third = pool_a.GetNext(mt);
  ORA_CHECK(!str_third.empty());

  game::SoundPool pool_empty{1.0f, game::SoundPool::kDefaultInterruptType, {}};
  ORA_CHECK(pool_empty.GetNext(mt).empty());
}

}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
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
