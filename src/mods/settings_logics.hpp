#pragma once
import std;

#include "game/mod_data.hpp"
#include "game/settings.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::ui {
class ButtonWidget;
class CheckboxWidget;
class DropDownButtonWidget;
class HotkeyEntryWidget;
class LabelWidget;
class PasswordFieldWidget;
class ScrollPanelWidget;
class TextFieldWidget;
}  // namespace ora::ui

namespace ora::mods {

class MusicPlaylist;

class ISettingsLogic {
 public:
  virtual ~ISettingsLogic() = default;
  virtual void RegisterSettingsPanel(
      const std::string& str_panel_id, const std::string& str_label,
      std::function<std::function<bool()>(ora::ui::Widget&)> fn_init,
      std::function<std::function<void()>(ora::ui::Widget&)> fn_reset) = 0;
};

void BindCheckboxPref(ora::ui::Widget& parent, std::string_view str_id,
                      game::SettingsModule& module_settings,
                      std::string_view str_pref);
void BindSliderPref(ora::ui::Widget& parent, std::string_view str_id,
                    game::SettingsModule& module_settings,
                    std::string_view str_pref);
void BindIntSliderPref(ora::ui::Widget& parent, std::string_view str_id,
                       game::SettingsModule& module_settings,
                       std::string_view str_pref);
void AdjustSettingsScrollPanelLayout(ora::ui::ScrollPanelWidget& scroll_panel);

std::string GetViewportSizeName(game::ModData& mod_data, int int4_viewport);
void BindTextNotificationPoolFilterSettings(ora::ui::Widget& panel,
                                            game::SettingsModule& module_game);
void ShowBattlefieldCameraDropdown(game::ModData& mod_data,
                                   ora::ui::Widget& dropdown,
                                   game::SettingsModule& module_graphics);
void ShowUIScaleDropdown(ora::ui::Widget& dropdown,
                         game::SettingsModule& module_graphics);
void ShowMouseControlDropdown(
    ora::ui::Widget& dropdown,
    const std::vector<std::pair<int, std::string>>& vec_control_types,
    game::SettingsModule& module_game);

class SettingsLogic final : public ora::ui::ChromeLogic,
                            public ISettingsLogic {
 public:
  SettingsLogic(ora::ui::Widget& widget, std::function<void()> fn_on_exit,
                ora::gfx::WorldRenderer* ptr_world_renderer,
                game::ModData& mod_data, const ora::ui::YamlDictionary* ptr_logic_args);

  void RegisterSettingsPanel(
      const std::string& str_panel_id, const std::string& str_label,
      std::function<std::function<bool()>(ora::ui::Widget&)> fn_init,
      std::function<std::function<void()>(ora::ui::Widget&)> fn_reset) override;

 private:
  ora::ui::Widget* ptr_panel_container_ = nullptr;
  ora::ui::Widget* ptr_tab_container_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_tab_template_;
  std::unique_ptr<ora::ui::Widget> up_panel_template_;
  int2 int2_button_stride_{};
  std::vector<ora::ui::Widget*> vec_buttons_;
  std::vector<std::pair<std::string, std::string>> vec_panels_;
  std::vector<std::pair<std::string, std::function<bool()>>> vec_leave_panel_actions_;
  std::vector<std::pair<std::string, std::function<void()>>> vec_reset_panel_actions_;
  std::string str_active_panel_;
  bool b_needs_restart_ = false;
  std::function<void()> fn_on_exit_;
  ora::gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
};

class AudioSettingsLogic final : public ora::ui::ChromeLogic {
 public:
  AudioSettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                     const std::string& str_panel_id,
                     const std::string& str_label,
                     ora::gfx::WorldRenderer* ptr_world_renderer);

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);

  game::SettingsModule* ptr_sound_settings_ = nullptr;
  std::string str_sound_device_;
  std::vector<std::pair<std::string, std::string>> vec_devices_;
  ora::ui::DropDownButtonWidget* ptr_audio_device_dropdown_ = nullptr;
  MusicPlaylist* ptr_music_playlist_ = nullptr;

  void ShowAudioDeviceDropdown(ora::ui::ScrollPanelWidget& scroll_panel);
  std::string DeviceLabel(const std::string& str_device) const;
};

void ResetSettingsPanelSnapshotsForTest();

class DisplaySettingsLogic final : public ora::ui::ChromeLogic {
 public:
  DisplaySettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                       const std::string& str_panel_id,
                       const std::string& str_label,
                       ora::gfx::WorldRenderer* ptr_world_renderer);

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);

  game::ModData* ptr_mod_data_ = nullptr;
  ora::gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  game::SettingsModule* ptr_game_settings_ = nullptr;
  game::SettingsModule* ptr_graphic_settings_ = nullptr;
  std::string str_show_on_damage_;
  std::string str_always_show_;
  std::string str_automatic_;
  std::string str_manual_;
  std::string str_disabled_;
  std::string str_legacy_fullscreen_;
  std::string str_fullscreen_;
  std::string str_select_preset_;
  ora::ui::DropDownButtonWidget* ptr_window_mode_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_display_selection_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_gl_profile_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_status_bars_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_target_lines_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_battlefield_camera_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_ui_scale_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_resolution_preset_dropdown_ = nullptr;
};

class GameplaySettingsLogic final : public ora::ui::ChromeLogic {
 public:
  GameplaySettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                        const std::string& str_panel_id,
                        const std::string& str_label,
                        ora::gfx::WorldRenderer* ptr_world_renderer);
  ~GameplaySettingsLogic() override;

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);
  void UpdateForumAuthFields();

  game::SettingsModule* ptr_auto_save_settings_ = nullptr;
  game::SettingsModule* ptr_game_settings_ = nullptr;
  game::SettingsModule* ptr_player_settings_ = nullptr;
  ora::gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  ora::ui::TextFieldWidget* ptr_name_textfield_ = nullptr;
  ora::ui::TextFieldWidget* ptr_profile_username_textfield_ = nullptr;
  ora::ui::PasswordFieldWidget* ptr_profile_password_textfield_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_interval_dropdown_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_file_number_dropdown_ = nullptr;
  game::LinkState kind_last_state_ = game::LinkState::Unlinked;
  std::function<void()> fn_unsubscribe_state_changed_;

  void ShowAutoSaveIntervalDropdown();
  void ShowAutoSaveFileNumberDropdown(ora::ui::DropDownButtonWidget& dropdown);
};

class InputSettingsLogic final : public ora::ui::ChromeLogic {
 public:
  InputSettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                     const std::string& str_panel_id,
                     const std::string& str_label);

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);

  game::SettingsModule* ptr_game_settings_ = nullptr;
  std::vector<std::pair<int, std::string>> vec_control_types_;
  ora::ui::Widget* ptr_game_settings_dropdown_ = nullptr;
  ora::ui::Widget* ptr_mouse_scroll_dropdown_ = nullptr;
  ora::ui::Widget* ptr_zoom_modifier_dropdown_ = nullptr;
};

class AdvancedSettingsLogic final : public ora::ui::ChromeLogic {
 public:
  AdvancedSettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                        const std::string& str_panel_id,
                        const std::string& str_label);

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);

  game::SettingsModule* ptr_debug_settings_ = nullptr;
  game::SettingsModule* ptr_game_settings_ = nullptr;
  game::SettingsModule* ptr_server_settings_ = nullptr;
};

class HotkeysSettingsLogic final : public ora::ui::ChromeLogic {
 public:
  HotkeysSettingsLogic(game::ModData& mod_data, ISettingsLogic& settings_logic,
                       const std::string& str_panel_id,
                       const std::string& str_label,
                       const ora::ui::YamlDictionary* ptr_logic_args);

 private:
  std::function<bool()> InitPanel(ora::ui::Widget& panel);
  std::function<void()> ResetPanel(ora::ui::Widget& panel);
  void InitHotkeyList();
  void InitHotkeyRemapDialog(ora::ui::Widget& panel);
  void BindHotkeyPref(const ora::ui::HotkeyDefinition& hd,
                      ora::ui::Widget& template_widget);
  void ValidateHotkey();
  void SaveHotkey();
  bool IsHotkeyVisibleInFilter(const ora::ui::HotkeyDefinition& hd) const;
  void ShowContextDropdown();

  game::ModData* ptr_mod_data_ = nullptr;
  const ora::ui::YamlDictionary* ptr_logic_args_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_hotkey_list_ = nullptr;
  ora::ui::ButtonWidget* ptr_selected_hotkey_button_ = nullptr;
  ora::ui::HotkeyEntryWidget* ptr_hotkey_entry_widget_ = nullptr;
  const ora::ui::HotkeyDefinition* ptr_selected_hotkey_definition_ = nullptr;
  const ora::ui::HotkeyDefinition* ptr_duplicate_hotkey_definition_ = nullptr;
  int int4_valid_hotkey_entry_width_ = 0;
  int int4_invalid_hotkey_entry_width_ = 0;
  bool b_is_hotkey_valid_ = false;
  bool b_is_hotkey_default_ = false;
  std::string str_current_context_;
  std::vector<std::string> vec_contexts_;
  std::vector<std::pair<std::string, std::vector<std::string>>> vec_hotkey_groups_;
  ora::ui::TextFieldWidget* ptr_filter_input_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_header_template_;
  std::unique_ptr<ora::ui::Widget> up_template_;
  ora::ui::Widget* ptr_empty_list_message_ = nullptr;
  ora::ui::Widget* ptr_remap_dialog_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_context_dropdown_ = nullptr;
};

void RegisterSettingsLogics();

}  // namespace ora::mods
