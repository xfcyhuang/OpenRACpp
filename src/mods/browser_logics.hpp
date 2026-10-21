// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ReplayBrowserLogic.cs
//          @e180e3225 L29-883 全文 +
//          OpenRA.Mods.Common/Widgets/Logic/ReplayUtils.cs L17-79 全文 +
//          GameSaveUtils.cs L19-82(值面;GameSaveBrowserLogic 随下一批)。
//          The whole of ReplayBrowserLogic.cs L29-883 + ReplayUtils.cs
//          L17-79 + GameSaveUtils.cs L19-82 (the value face;
//          GameSaveBrowserLogic rides the next batch).
#pragma once
import std;

#include "server/game_information.hpp"
#include "ui/widget.hpp"

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
namespace ora::ui {
class ButtonWidget;
class ScrollItemWidget;
class ScrollPanelWidget;
}

namespace ora::mods {

/// GameInformation.GetMapPreview(GameInformation.cs L59-67)的等价面
/// (MapGenerationArgs 分支随地图生成批;COVERAGE 登记)
map::MapPreview& GameInfoMapPreview(const server::GameInformation& info,
                                     game::ModData& mod_data);

/// GameInformation.ResolvedPlayerName(GameInformation.cs L171-181)
std::string GameInfoResolvedPlayerName(const server::GameInformation& info,
                                       const server::GameInformationPlayer&
                                           player);

/// ReplayUtils.PromptConfirmReplayCompatibility(ReplayUtils.cs L45-70);
/// meta 的 GameInfo 载荷不可解析 = 上游 replayMeta == null 的对话框路径
bool PromptConfirmReplayCompatibility(const net::ReplayMetadata& meta,
                                      game::ModData& mod_data,
                                      std::function<void()> fn_on_cancel = {});

/// GameSaveUtils.GetGameDuration(GameSaveUtils.cs L33-39)的秒域承载;
/// 无 GameSave → nullopt
std::optional<double> GetGameDurationSeconds(const net::GameSave* save);

/// GameSaveUtils.FormatGameDuration(L41-48)
std::string FormatGameDuration(std::optional<double> fp8_seconds);

/// GameSaveUtils.IsValidNewSaveName(L65-80)
bool IsValidNewSaveName(const std::string& str_new_name,
                        const std::string& str_initial_name,
                        const std::string& str_directory);

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

  /// EvaluateReplayVisibility(L601-687):filter 静态域消费(测试面)
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

void RegisterBrowserLogics();

}  // namespace ora::mods
