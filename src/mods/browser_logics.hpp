#pragma once
import std;

#include "core/size.hpp"
#include "map/map_cache.hpp"
#include "net/game_save.hpp"
#include "server/game_information.hpp"
#include "server/lobby_option.hpp"
#include "ui/widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::map {
class MapPreview;
}
namespace ora::net {
class ReplayMetadata;
class GameSave;
}
namespace ora::sim {
class World;
}
namespace ora::sound {
class Sound;
}
namespace ora::ui {
class ButtonWidget;
class DropDownButtonWidget;
class LabelWidget;
class ScrollItemWidget;
class ScrollPanelWidget;
class SpawnOccupant;
class TextFieldWidget;
}
namespace ora::gfx {
class SpriteFont;
}

namespace ora::mods {

class MusicPlaylist;
class VideoPlayerWidget;

map::MapPreview& GameInfoMapPreview(const server::GameInformation& info,
                                     game::ModData& mod_data);

std::string GameInfoResolvedPlayerName(const server::GameInformation& info,
                                       const server::GameInformationPlayer&
                                           player);

bool PromptConfirmReplayCompatibility(const net::ReplayMetadata& meta,
                                      game::ModData& mod_data,
                                      std::function<void()> fn_on_cancel = {});

std::optional<double> GetGameDurationSeconds(const net::GameSave* save);

std::string FormatGameDuration(std::optional<double> fp8_seconds);

bool IsValidNewSaveName(const std::string& str_new_name,
                        const std::string& str_initial_name,
                        const std::string& str_directory);

std::string BuildSaveTooltipText(const std::string& str_save_path,
                                 const net::GameSave* ptr_save,
                                 game::ModData& mod_data);

std::pair<std::optional<std::string>, std::optional<std::string>>
SplitOnFirstToken(const std::string& str_input,
                  const std::string& str_token = "\n");

std::string MapChooserMapSizeLabel(const Size& size);

std::vector<server::LobbyOptionData> CollectMapLobbyOptions(
    const map::MapPreview& preview);
void SetMapLobbyOptionsCollectorForTest(
    std::function<std::vector<server::LobbyOptionData>(
        const map::MapPreview&)>* fn_collector);

class ReplayBrowserLogic final : public ora::ui::ChromeLogic {
 public:
  enum class GameType { Any, Singleplayer, Multiplayer };
  enum class DateType { Any, Today, LastWeek, LastFortnight, LastMonth };
  enum class DurationType { Any, VeryShort, Short, Medium, Long };

  struct Filter {
    GameType kind_type = GameType::Any;
    DateType kind_date = DateType::Any;
    DurationType kind_duration = DurationType::Any;
    server::WinState kind_outcome = server::WinState::Undefined;
    std::string str_player_name;
    std::string str_map_name;
    std::string str_faction;

    bool IsEmpty() const {
      return kind_type == GameType::Any && kind_date == DateType::Any &&
             kind_duration == DurationType::Any &&
             kind_outcome == server::WinState::Undefined &&
             str_player_name.empty() && str_map_name.empty() &&
             str_faction.empty();
    }
  };

  ReplayBrowserLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                     std::function<void()> fn_on_exit,
                     std::function<void()> fn_on_start);
  ~ReplayBrowserLogic() override;

  static bool EvaluateReplayVisibility(const server::GameInformation& info);

  void ApplyFilter();
  void SelectReplay(std::size_t sz_index);

  static const Filter& StaticFilter() { return filter_; }
  static void SetStaticFilterForTest(const Filter& filter_next) {
    filter_ = filter_next;
  }
  const std::vector<std::string>& VisibleMapsForTest() const {
    return vec_visible_uids_;
  }
  std::size_t SelectedIndexForTest() const { return sz_selected_; }
  const std::vector<std::string>& ReplayFilesForTest() const {
    return vec_files_for_test_;
  }

 private:
  struct ReplayState {
    std::unique_ptr<net::ReplayMetadata> up_metadata;
    std::optional<server::GameInformation> info_game;
    ora::ui::ScrollItemWidget* ptr_item = nullptr;
    bool b_visible = true;
  };

  void LoadReplaysAsync(const std::string& str_dir);
  void SetupFilters();
  void SetupReplayDependentFilters();
  void SetupManagement();
  void SelectFirstVisibleReplay();
  void WatchReplay();
  void AddReplay(std::unique_ptr<net::ReplayMetadata> up_meta,
                 std::optional<server::GameInformation> info);

  static Filter filter_;

  ora::ui::Widget* ptr_panel_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_replay_list_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_player_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_player_header_;
  std::unique_ptr<ora::ui::Widget> up_player_template_;
  std::unique_ptr<ora::ui::Widget> up_replay_template_;
  std::vector<ReplayState> vec_replays_;
  std::vector<std::string> vec_visible_uids_;
  std::vector<std::string> vec_files_for_test_;
  std::function<void()> fn_on_start_;
  game::ModData* ptr_mod_data_ = nullptr;
  map::MapPreview* ptr_map_ = nullptr;
  std::size_t sz_selected_ = static_cast<std::size_t>(-1);
  std::atomic<bool> b_cancel_loading_{false};
  std::shared_ptr<bool> ptr_alive_;
  std::function<void()> fn_unsubscribe_game_start_;
};

class GameSaveBrowserLogic final : public ora::ui::ChromeLogic {
 public:
  GameSaveBrowserLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                       sim::World& world,
                       std::function<void()> fn_on_exit,
                       std::function<void()> fn_on_start);
  ~GameSaveBrowserLogic() override;

  std::map<int, ora::ui::SpawnOccupant> GetSpawnOccupantsForTest() const {
    return GetSpawnOccupants();
  }
  void SelectForTest(const std::string* ptr_save_path) {
    Select(ptr_save_path);
  }
  void DeleteForTest(const std::string& str_save_path) {
    Delete(str_save_path);
  }
  const std::vector<std::string>& GamesForTest() const { return vec_games_; }
  const std::string& SelectedPathForTest() const { return str_selected_path_; }
  const std::string& DefaultSaveFilenameForTest() const {
    return str_default_save_filename_;
  }
  const std::vector<std::string>& PlayerListLabelsForTest() const {
    return vec_player_labels_for_test_;
  }

 private:
  void LoadGames(ora::ui::ScrollItemWidget& tpl_game,
                 ora::ui::ScrollItemWidget& tpl_date_header);
  void Rename(const std::string& str_old_name, const std::string& str_new_name);
  void Delete(const std::string& str_save_path);
  void SelectFirstVisible();
  void Select(const std::string* ptr_save_path);
  std::map<std::string, net::GameSaveSlotClient> GetWorldSlotClients() const;
  std::map<int, ora::ui::SpawnOccupant> GetSpawnOccupants() const;
  void UpdatePlayerList();
  void SaveGame();

  ora::ui::Widget* ptr_panel_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_game_list_ = nullptr;
  ora::ui::TextFieldWidget* ptr_save_text_field_ = nullptr;
  std::vector<std::string> vec_games_;
  std::function<void()> fn_on_exit_;
  game::ModData* ptr_mod_data_ = nullptr;
  std::string str_base_save_path_;
  ora::ui::ScrollPanelWidget* ptr_player_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_player_header_;
  std::unique_ptr<ora::ui::Widget> up_player_template_;
  std::unique_ptr<ora::ui::Widget> up_game_template_;
  std::unique_ptr<ora::ui::Widget> up_date_header_template_;
  map::MapPreview* ptr_map_ = nullptr;
  std::string str_default_save_filename_;
  std::string str_selected_path_;
  bool b_has_selected_path_ = false;
  std::optional<net::GameSave> opt_selected_save_;
  sim::World* ptr_world_ = nullptr;
  std::vector<std::string> vec_player_labels_for_test_;
  std::function<void()> fn_unsubscribe_game_start_;
};

class MissionBrowserLogic final : public ora::ui::ChromeLogic {
 public:
  MissionBrowserLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                      sim::World& world, std::function<void()> fn_on_start,
                      std::function<void()> fn_on_exit,
                      const std::string& str_initial_map);
  ~MissionBrowserLogic() override;

  void SelectMapForTest(map::MapPreview& preview) { SelectMap(preview); }
  void StartMissionClickedForTest() { StartMissionClicked(); }
  const std::vector<std::string>& StartedOrdersForTest() const {
    return vec_started_orders_for_test_;
  }
  std::string StartedServerUidForTest() const { return str_started_uid_for_test_; }
  bool DifficultyDisabledForTest() const { return b_difficulty_disabled_; }
  bool MinifiedOptionsForTest() const { return b_minified_options_; }
  const std::vector<map::MapPreview*>& MissionPreviewsForTest() const {
    return vec_all_previews_;
  }

 private:
  enum class PlayingVideo { None, Info, Briefing, GameStart };
  enum class PanelType { MissionInfo, Options };

  void CreateMissionGroup(const std::string& str_title,
                          const std::vector<map::MapPreview*>& vec_previews);
  void SelectMap(map::MapPreview& preview);
  void RebuildOptions();
  void SetMapDifficulty(const server::LobbyOptionData& option);
  void SetMapSpeed(const server::LobbyOptionData& option);
  void OnOptionSelected(const std::string& str_option_id,
                        const std::string& str_value);
  void BuildOptions(const std::vector<server::LobbyOptionData>& vec_options);
  void BuildMinifiedOptions(
      const std::vector<server::LobbyOptionData>& vec_options);
  void SetupDropdown(ora::ui::DropDownButtonWidget& dropdown,
                     const server::LobbyOptionData& option);
  void PlayVideo(VideoPlayerWidget& player,
                 const std::string& str_video, PlayingVideo kind_playing,
                 std::function<void()> fn_on_complete = nullptr);
  void StopVideo(VideoPlayerWidget& player);
  void StartMissionClicked();

  game::ModData* ptr_mod_data_ = nullptr;
  std::function<void()> fn_on_start_;
  std::function<void()> fn_on_exit_;
  ora::ui::Widget* ptr_mission_detail_ = nullptr;
  ora::ui::Widget* ptr_options_container_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_checkbox_row_template_;
  std::unique_ptr<ora::ui::Widget> up_dropdown_row_template_;
  ora::ui::ScrollPanelWidget* ptr_description_panel_ = nullptr;
  ora::ui::LabelWidget* ptr_description_ = nullptr;
  gfx::SpriteFont* ptr_description_font_ = nullptr;
  ora::ui::ButtonWidget* ptr_start_briefing_video_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_stop_briefing_video_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_start_info_video_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_stop_info_video_button_ = nullptr;
  VideoPlayerWidget* ptr_video_player_ = nullptr;
  ora::ui::Widget* ptr_fullscreen_video_player_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_mission_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_header_template_;
  std::unique_ptr<ora::ui::Widget> up_template_;
  ora::ui::Widget* ptr_mini_options_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_difficulty_button_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_game_speed_button_ = nullptr;
  std::string str_unset_difficulty_;
  std::string str_default_tooltip_;
  std::string str_briefing_video_;
  bool b_briefing_video_visible_ = false;
  std::string str_info_video_;
  bool b_info_video_visible_ = false;
  std::optional<std::string> opt_selected_difficulty_;
  std::optional<std::string> opt_selected_game_speed_;
  bool b_minified_options_ = true;
  map::MapPreview* ptr_selected_map_ = nullptr;
  PlayingVideo kind_playing_video_ = PlayingVideo::None;
  std::vector<std::pair<std::string, std::string>> vec_mission_options_;
  PanelType kind_panel_ = PanelType::MissionInfo;
  std::vector<map::MapPreview*> vec_all_previews_;
  std::vector<std::string> vec_started_orders_for_test_;
  std::string str_started_uid_for_test_;
  bool b_difficulty_disabled_ = false;
  float fp4_cached_sound_volume_ = 0;
  float fp4_cached_music_volume_ = 0;
  std::shared_ptr<bool> ptr_alive_;
  std::function<void()> fn_unsubscribe_game_start_;
};

class MapChooserLogic final : public ora::ui::ChromeLogic {
 public:
  MapChooserLogic(
      ora::ui::Widget& widget, game::ModData& mod_data,
      const std::string* ptr_initial_map,
      const map::MapGenerationArgs* ptr_initial_generated_map,
      const std::vector<std::string>* ptr_remote_map_pool,
      map::MapClassification kind_initial_tab, std::function<void()> fn_on_exit,
      std::function<void(const std::string&)> fn_on_select,
      std::function<void(const map::MapGenerationArgs&)>
          fn_on_select_generated,
      map::MapVisibility kind_filter);
  ~MapChooserLogic() override;

  const std::vector<std::string>& VisibleMapsForTest() const {
    return vec_visible_maps_;
  }
  const std::string& SelectedUidForTest() const { return str_selected_uid_; }
  map::MapClassification CurrentTabForTest() const { return kind_current_tab_; }

 private:
  void SwitchTab(map::MapClassification kind_tab);
  void SwitchTabOfMap(const std::string& str_uid);
  void RefreshMaps(map::MapClassification kind_tab);
  void SetupMapTabs();
  void SetupMapPanel(map::MapClassification kind_tab,
                     const std::string& str_tab_container_name);
  void SetupGameModeDropdown(map::MapClassification kind_tab,
                             ora::ui::DropDownButtonWidget* ptr_dropdown);
  void SetupOrderByDropdown();
  void EnumerateMaps(map::MapClassification kind_tab);
  std::string DeleteMap(const std::string& str_map);
  void DeleteOneMap(const std::string& str_map,
                    std::function<void(const std::string&)> fn_after);
  void DeleteAllMaps(const std::vector<std::string>& vec_maps,
                     std::function<void(const std::string&)> fn_after);

  std::string str_all_maps_;
  ora::ui::Widget* ptr_widget_ = nullptr;
  ora::ui::DropDownButtonWidget* ptr_game_mode_dropdown_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  const std::vector<std::string>* ptr_remote_map_pool_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_item_template_;
  map::MapVisibility kind_filter_{};
  map::MapClassification kind_current_tab_{};
  bool b_disposed_ = false;
  int int4_remote_searching_ = 0;
  int int4_remote_unavailable_ = 0;
  std::map<map::MapClassification, ora::ui::ScrollPanelWidget*>
      map_scrollpanels_;
  std::map<map::MapClassification, std::vector<map::MapPreview*>> map_tab_maps_;
  std::map<map::MapClassification, std::string> map_tab_labels_;
  std::vector<std::string> vec_visible_maps_;
  std::string str_selected_uid_;
  std::function<void(const std::string&)> fn_on_select_;
  std::optional<std::string> opt_category_;
  std::optional<std::string> opt_map_filter_;
  std::function<long(const map::MapPreview&)> fn_order_by_;
  std::vector<std::pair<std::string,
                        std::function<long(const map::MapPreview&)>>>
      vec_order_by_dict_;
};

class MusicPlayerLogic final : public ora::ui::ChromeLogic {
 public:
  MusicPlayerLogic(ora::ui::Widget& widget, sim::World& world,
                   game::ModData& mod_data, std::function<void()> fn_on_exit);

  void BuildMusicTable();
  const std::vector<std::string>& SongTitlesForTest() const {
    return vec_song_titles_for_test_;
  }
  const std::string& CurrentSongTitleForTest() const {
    return str_current_song_title_for_test_;
  }

 private:
  void Play();
  static std::string SongLengthLabel(const game::MusicInfo& song);

  ora::ui::ScrollPanelWidget* ptr_music_list_ = nullptr;
  std::unique_ptr<ora::ui::Widget> up_item_template_;
  MusicPlaylist* ptr_music_playlist_ = nullptr;
  const game::MusicInfo* ptr_current_song_ = nullptr;
  std::vector<std::string> vec_song_titles_for_test_;
  std::string str_current_song_title_for_test_;
};

void RegisterBrowserLogics();

}
