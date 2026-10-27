#include "mods/hotkey_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "game/settings.hpp"
#include "mods/music_playlist.hpp"
#include "net/text_notifications.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/logic_key_listener_widget.hpp"

namespace ora::mods {

namespace {

ui::HotkeyReference NamedKeyOrInvalid(
    game::ModData& mod_data, const ui::YamlDictionary* ptr_logic_args,
    std::string_view str_key) {
  if (ptr_logic_args != nullptr)
    for (const auto& [str_entry, ptr_yaml] : *ptr_logic_args) {
      if (str_entry != str_key)
        continue;
      if (ptr_yaml != nullptr && ptr_yaml->Value != nullptr)
        return mod_data.Hotkeys().Get(*ptr_yaml->Value);
      break;
    }
  return ui::HotkeyReference{};
}

}  // namespace

SingleHotkeyBaseLogic::SingleHotkeyBaseLogic(
    ora::ui::Widget& widget_root, game::ModData& mod_data,
    const std::string& str_arg_name, const std::string& str_parent_name,
    const ora::ui::YamlDictionary* ptr_logic_args) {
  const ui::HotkeyReference hk_named =
      NamedKeyOrInvalid(mod_data, ptr_logic_args, str_arg_name);

  auto* ptr_keyhandler =
      widget_root.Get<ui::LogicKeyListenerWidget>(str_parent_name);
  ptr_keyhandler->AddHandler([this, hk_named](const KeyInput& e_input) {
    if (e_input.Event == KeyInputEvent::Down &&
        hk_named.IsActivatedBy(e_input))
      return OnHotkeyActivated(e_input);
    return false;
  });
}

MusicHotkeyLogic::MusicHotkeyLogic(ora::ui::Widget& widget_root,
                                   game::ModData& mod_data,
                                   sim::World& world,
                                   const ora::ui::YamlDictionary* ptr_logic_args) {
  const auto vec_playlists =
      world.WorldActor()->TraitsImplementing<MusicPlaylist>();
  ptr_playlist_ = vec_playlists.empty() ? nullptr : vec_playlists.front();

  const ui::HotkeyReference hk_stop =
      NamedKeyOrInvalid(mod_data, ptr_logic_args, "StopMusicKey");
  const ui::HotkeyReference hk_pause =
      NamedKeyOrInvalid(mod_data, ptr_logic_args, "PauseMusicKey");
  const ui::HotkeyReference hk_prev =
      NamedKeyOrInvalid(mod_data, ptr_logic_args, "PrevMusicKey");
  const ui::HotkeyReference hk_next =
      NamedKeyOrInvalid(mod_data, ptr_logic_args, "NextMusicKey");

  auto* ptr_keyhandler =
      widget_root.Get<ui::LogicKeyListenerWidget>("GLOBAL_KEYHANDLER");
  ptr_keyhandler->AddHandler([this, hk_stop, hk_pause, hk_prev,
                         hk_next](const KeyInput& e_input) {
    if (e_input.Event == KeyInputEvent::Down) {
      if (hk_next.IsActivatedBy(e_input)) {
        if (ptr_playlist_ != nullptr) {
          if (auto* ptr_song = ptr_playlist_->GetNextSong())
            ptr_playlist_->Play(ptr_song);
        }
      } else if (hk_prev.IsActivatedBy(e_input)) {
        if (ptr_playlist_ != nullptr) {
          if (auto* ptr_song = ptr_playlist_->GetPrevSong())
            ptr_playlist_->Play(ptr_song);
        }
      } else if (hk_stop.IsActivatedBy(e_input)) {
        StopMusic();
      } else if (hk_pause.IsActivatedBy(e_input)) {
        PauseOrResumeMusic();
      }
    }

    return false;
  });
}

void MusicHotkeyLogic::StopMusic() {
  if (ptr_playlist_ == nullptr)
    return;
  if (!ptr_playlist_->CurrentSongIsBackground())
    ptr_playlist_->Stop();
  else if (auto* ptr_song = ptr_playlist_->GetNextSong())
    ptr_playlist_->Play(ptr_song);
}

void MusicHotkeyLogic::PauseOrResumeMusic() {
  auto* ptr_sound =
      ui::Ui::HasInstance() ? ui::Ui::Instance().SoundDeps() : nullptr;
  if (ptr_sound == nullptr)
    return;
  if (ptr_sound->MusicPlaying()) {
    ptr_sound->PauseMusic();
  } else if (ptr_sound->CurrentMusic() != nullptr) {
    ptr_sound->PlayMusic();
  } else if (ptr_playlist_ != nullptr) {
    if (auto* ptr_song = ptr_playlist_->GetNextSong())
      ptr_playlist_->Play(ptr_song);
  }
}

ScreenshotHotkeyLogic::ScreenshotHotkeyLogic(
    ora::ui::Widget& widget_root, game::ModData& mod_data,
    const ora::ui::YamlDictionary* ptr_logic_args)
    : SingleHotkeyBaseLogic(widget_root, mod_data, "TakeScreenshotKey",
                            "GLOBAL_KEYHANDLER", ptr_logic_args) {}

bool ScreenshotHotkeyLogic::OnHotkeyActivated(const KeyInput& e_input) {
  (void)e_input;
  if (ui::Ui::HasInstance())
    ui::Ui::Instance().DepsForTest().fn_take_screenshot();
  return true;
}

MuteHotkeyLogic::MuteHotkeyLogic(
    ora::ui::Widget& widget_root, game::ModData& mod_data,
    const ora::ui::YamlDictionary* ptr_logic_args)
    : SingleHotkeyBaseLogic(widget_root, mod_data, "MuteAudioKey",
                            "GLOBAL_KEYHANDLER", ptr_logic_args) {}

bool MuteHotkeyLogic::OnHotkeyActivated(const KeyInput& e_input) {
  (void)e_input;
  auto& settings = game::Settings::Instance();
  game::SettingsModule* ptr_module = settings.Module("Sound");
  if (ptr_module == nullptr)
    return true;
  const auto opt_slot = ptr_module->SlotIndexOf("Mute");
  if (!opt_slot.has_value())
    return true;
  const bool b_muted = !ptr_module->GetBool(*opt_slot);
  ptr_module->SetBool(*opt_slot, b_muted);

  auto* ptr_sound =
      ui::Ui::HasInstance() ? ui::Ui::Instance().SoundDeps() : nullptr;
  if (ptr_sound != nullptr) {
    if (b_muted) {
      ptr_sound->MuteAudio();
      net::TextNotificationsManager::Instance().AddFeedbackLine(
          fluent::FluentProvider::Instance().GetMessage("label-audio-muted"));
    } else {
      ptr_sound->UnmuteAudio();
      net::TextNotificationsManager::Instance().AddFeedbackLine(
          fluent::FluentProvider::Instance().GetMessage(
              "label-audio-unmuted"));
    }
  }

  return true;
}

void RegisterHotkeyLogics() {
  auto& registry = ui::ChromeLogicRegistry::Instance();

  registry.Register(
      "MusicHotkeyLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        const ui::YamlDictionary* ptr_logic_args = nullptr;
        if (const ui::WidgetArgValue* ptr_value =
                args_args.TryGet("logicArgs"))
          if (const auto* ptr_dict =
                  std::get_if<const ui::YamlDictionary*>(ptr_value))
            ptr_logic_args = *ptr_dict;
        return std::make_unique<MusicHotkeyLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")), ptr_logic_args);
      });

  registry.Register(
      "ScreenshotHotkeyLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        const ui::YamlDictionary* ptr_logic_args = nullptr;
        if (const ui::WidgetArgValue* ptr_value =
                args_args.TryGet("logicArgs"))
          if (const auto* ptr_dict =
                  std::get_if<const ui::YamlDictionary*>(ptr_value))
            ptr_logic_args = *ptr_dict;
        return std::make_unique<ScreenshotHotkeyLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            ptr_logic_args);
      });

  registry.Register(
      "MuteHotkeyLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        const ui::YamlDictionary* ptr_logic_args = nullptr;
        if (const ui::WidgetArgValue* ptr_value =
                args_args.TryGet("logicArgs"))
          if (const auto* ptr_dict =
                  std::get_if<const ui::YamlDictionary*>(ptr_value))
            ptr_logic_args = *ptr_dict;
        return std::make_unique<MuteHotkeyLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            ptr_logic_args);
      });
}

}  // namespace ora::mods
