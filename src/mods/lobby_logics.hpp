#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "map/map_cache.hpp"
#include "mods/lobby_utils.hpp"
#include "net/session.hpp"
#include "net/text_notifications.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::map {
class MapPreview;
}
namespace ora::net {
class NetworkConnection;
class OrderManager;
struct SessionClient;
}
namespace ora::sim {
class World;
}
namespace ora::ui {
class ButtonWidget;
class CheckboxWidget;
class DropDownButtonWidget;
class LabelWidget;
class MapPreviewWidget;
class ScrollPanelWidget;
class TextFieldWidget;
class TooltipContainerWidget;
}

namespace ora::mods {

struct TabCompletionState {
  std::optional<std::vector<std::string>> opt_commands;
  std::optional<std::vector<std::string>> opt_names;
  std::vector<std::string> vec_candidates;
  int int4_current_candidate_index = 0;
  std::string str_last_completed;
  std::string str_prefix;
  std::string str_suffix;
};

std::string CompleteTabCompletion(TabCompletionState& state_tab,
                                  const std::string& str_text);

void SetupLatencyTooltip(ora::ui::Widget& widget,
                         ora::ui::TooltipContainerWidget& tooltip_container,
                         ora::net::SessionClient* ptr_client);

void SetupSpawnSelectorTooltip(
    ora::ui::Widget& widget, ora::game::ModData& mod_data,
    ora::ui::TooltipContainerWidget& tooltip_container,
    ora::ui::MapPreviewWidget& preview, bool b_show_unoccupied_spawnpoints);

void SetupKickClient(ora::ui::Widget& widget, std::string str_client_name,
                     std::function<void(bool)> fn_ok_pressed,
                     std::function<void()> fn_cancel_pressed);

void SetupKickSpectators(ora::ui::Widget& widget, int int4_client_count,
                         std::function<void()> fn_ok_pressed,
                         std::function<void()> fn_cancel_pressed);

class MapPreviewLogic final : public ora::ui::ChromeLogic {
 public:
  enum class PreviewStatus {
    Unknown,
    Playable,
    Incompatible,
    Validating,
    Generating,
    GenerationError,
    DownloadAvailable,
    Searching,
    Downloading,
    DownloadError,
    Unavailable,
    UpdateAvailable,
    UpdateDownloadAvailable,
  };

  MapPreviewLogic(ora::ui::Widget& widget, ora::game::ModData& mod_data,
                  std::function<std::pair<ora::map::MapPreview*, int>()>
                      fn_get_map,
                  std::function<void(ora::ui::MapPreviewWidget*,
                                     ora::map::MapPreview*,
                                     const MouseInput&)>
                      fn_on_mouse_down,
                  std::function<std::map<int, ora::ui::SpawnOccupant>()>
                      fn_get_spawn_occupants,
                  bool b_map_updates_enabled,
                  std::function<void(const std::string&)> fn_on_map_update,
                  std::function<std::vector<int>()>
                      fn_get_disabled_spawn_points,
                  bool b_show_unoccupied_spawnpoints);

  void Tick() override;

  PreviewStatus StatusForTest() const { return kind_current_status_; }
  bool RetryTriggeredForTest() const { return b_retry_triggered_; }

 private:
  void UpdateVisibility();

  std::function<std::pair<ora::map::MapPreview*, int>()> fn_get_map_;
  std::function<void(const std::string&)> fn_on_map_update_;
  bool b_map_updates_enabled_ = false;

  std::map<PreviewStatus, std::vector<ora::ui::Widget*>> map_preview_widgets_;
  std::vector<ora::ui::Widget*> vec_visible_widgets_;
  PreviewStatus kind_current_status_ = PreviewStatus::Unknown;
  bool b_map_update_available_ = false;
  bool b_retry_triggered_ = false;
  bool b_blink_ = false;
  int int4_blink_tick_ = 0;
  const int int4_blink_tick_length_ = 10;
  ora::ui::ButtonWidget* ptr_retry_button_ = nullptr;
};

class LobbyOptionsLogic final : public ora::ui::ChromeLogic {
 public:
  LobbyOptionsLogic(ora::ui::Widget& widget,
                    ora::net::OrderManager& order_manager,
                    std::function<ora::map::MapPreview*()> fn_get_map,
                    std::function<bool()> fn_configuration_disabled);

  void Tick() override;

  std::size_t RowCountForTest() const;

 private:
  void RebuildOptions();

  ora::ui::ScrollPanelWidget* ptr_panel_ = nullptr;
  ora::ui::Widget* ptr_options_container_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_checkbox_row_template_;
  std::unique_ptr<ora::ui::Widget> up_dropdown_row_template_;
  int int4_y_margin_ = 0;

  std::function<ora::map::MapPreview*()> fn_get_map_;
  ora::net::OrderManager* ptr_order_manager_ = nullptr;
  std::function<bool()> fn_configuration_disabled_;
  ora::map::MapPreview* ptr_map_preview_ = nullptr;
  ora::map::MapStatus kind_map_status_ = ora::map::MapStatus::Unavailable;
};

class LobbyLogic final
    : public ora::ui::NotificationHandlerSet<net::TextNotification> {
 public:
  enum class PanelType { Players, Options, Music, Servers, Kick, ForceStart };

  LobbyLogic(ora::ui::Widget& widget, ora::game::ModData& mod_data,
             ora::gfx::WorldRenderer* ptr_world_renderer,
             ora::net::OrderManager& order_manager,
             std::function<void()> fn_on_exit,
             std::function<void()> fn_on_start, bool b_skirmish_mode,
             const ora::ui::YamlDictionary* ptr_logic_args);
  ~LobbyLogic() override;

  void Tick() override;
  void Handle(const net::TextNotification& notification) override;

  PanelType PanelForTest() const { return kind_panel_; }
  void TickForTest() { Tick(); }
  void HandleForTest(const net::TextNotification& notification) {
    Handle(notification);
  }
  const std::map<int, ora::ui::SpawnOccupant>& SpawnOccupantsForTest() const {
    return map_spawn_occupants_;
  }
  bool InsufficientPlayerSpawnsForTest() const {
    return b_insufficient_player_spawns_;
  }
  bool ChatEnabledForTest() const { return b_chat_enabled_; }
  TabCompletionState& TabCompletionForTest() { return state_tab_; }
  void UpdatePlayerListForTest() { UpdatePlayerList(); }
  void UpdateOptionsForTest() { UpdateOptions(); }

 private:
  void ConnectionStateChanged(ora::net::OrderManager* ptr_order_manager,
                              const std::string& str_password,
                              ora::net::NetworkConnection* ptr_connection);
  bool MapIsPlayable() const;
  void CurrentMapBecameAvailable();
  void UpdateCurrentMap();
  void UpdatePlayerList();
  void UpdateSpawnOccupants();
  void UpdateOptions();
  void OnGameStart();
  bool OptionsTabDisabled() const;
  bool StartDisabled() const;
  void StartGameAction();

  game::ModData* ptr_mod_data_ = nullptr;
  std::function<void()> fn_on_start_;
  std::function<void()> fn_on_exit_;
  ora::net::OrderManager* ptr_order_manager_ = nullptr;
  ora::gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  bool b_skirmish_mode_ = false;
  const game::Ruleset* ptr_mod_rules_ = nullptr;

  PanelType kind_panel_ = PanelType::Players;

  ora::ui::Widget* ptr_lobby_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_editable_player_template_;
  std::unique_ptr<ora::ui::Widget> up_non_editable_player_template_;
  std::unique_ptr<ora::ui::Widget> up_empty_slot_template_;
  std::unique_ptr<ora::ui::Widget> up_editable_spectator_template_;
  std::unique_ptr<ora::ui::Widget> up_non_editable_spectator_template_;
  std::unique_ptr<ora::ui::Widget> up_new_spectator_template_;

  ora::ui::ScrollPanelWidget* ptr_lobby_chat_panel_ = nullptr;
  std::map<net::TextNotificationPool, std::unique_ptr<ora::ui::Widget>>
      map_chat_templates_;
  ora::ui::TextFieldWidget* ptr_chat_text_field_ = nullptr;
  std::optional<core::CachedTransform<int, std::string>>
      transform_chat_available_in_;
  std::string str_chat_disabled_;

  ora::ui::ScrollPanelWidget* ptr_players_ = nullptr;

  std::vector<std::pair<std::string, LobbyFaction>> vec_factions_;

  TabCompletionState state_tab_;

  ora::map::MapPreview* ptr_map_ = nullptr;
  int int4_map_status_ = 0;
  std::optional<map::MapGenerationArgs> opt_last_generated_map_;
  bool b_game_starting_ = false;

  bool b_chat_enabled_ = false;
  bool b_disable_team_chat_ = false;
  bool b_insufficient_player_spawns_ = false;
  bool b_team_chat_ = false;
  bool b_update_discord_status_ = true;
  bool b_reset_options_button_enabled_ = false;
  bool b_map_available_ = false;
  std::map<int, ora::ui::SpawnOccupant> map_spawn_occupants_;
  std::map<std::string, net::SessionLobbyOption> map_lobby_options_;

  std::string str_chat_line_sound_;
  std::string str_player_joined_sound_;
  std::string str_player_left_sound_;
  std::string str_lobby_option_changed_sound_;

  std::vector<std::function<void()>> vec_unsubscribers_;
};

void DiscordServiceNoop();

void RegisterLobbyLogics();

}
