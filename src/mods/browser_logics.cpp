// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ReplayBrowserLogic.cs +
//          ReplayUtils.cs + GameSaveUtils.cs(see browser_logics.hpp)。
#include "mods/browser_logics.hpp"

#include <ctime>

#include "core/mersenne_twister.hpp"
#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "gfx/sprite_font.hpp"
#include "sim/trait_registry.hpp"
#include "sound/sound.hpp"
#include "meta/generic_record.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "game/web_services.hpp"
#include "map/map_cache.hpp"
#include "map/map.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "mods/music_playlist.hpp"
#include "mods/video_player_widget.hpp"
#include "net/game_save.hpp"
#include "net/replay_recorder.hpp"
#include "net/text_notifications.hpp"
#include "server/game_information.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 fluent::FluentValue val_arg) {
  return Fluent().GetMessage(
      str_key, std::vector<std::pair<std::string, fluent::FluentValue>>{
                   {std::string{str_arg_name}, std::move(val_arg)}});
}

std::string Msg2(std::string_view str_key, std::string_view str_arg_a,
                 const fluent::FluentValue& val_a,
                 std::string_view str_arg_b,
                 const fluent::FluentValue& val_b) {
  return Fluent().GetMessage(
      str_key, std::vector<std::pair<std::string, fluent::FluentValue>>{
                   {std::string{str_arg_a}, val_a},
                   {std::string{str_arg_b}, val_b}});
}

char FoldChar(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (FoldChar(a[i]) != FoldChar(b[i]))
      return false;
  return true;
}

bool LessIgnoreCase(std::string_view a, std::string_view b) {
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = FoldChar(a[i]);
    const char cb = FoldChar(b[i]);
    if (ca != cb)
      return ca < cb;
  }
  return a.size() < b.size();
}

FluentArgs MakeArgs(
    std::initializer_list<std::pair<std::string_view, std::string>> vec_pairs) {
  FluentArgs vec_out;
  for (const auto& [str_key, str_value] : vec_pairs)
    vec_out.emplace_back(
        std::string{str_key},
        fluent::FluentValue{std::string{str_value}});
  return vec_out;
}

std::string ButtonText(const ora::ui::ButtonWidget& button) {
  if (button.fn_get_text)
    return button.fn_get_text();
  return button.str_text;
}

std::string BaseDirectory(const char* str_kind,
                          const game::ModData& mod_data) {
  return std::string{str_kind} + "/" + mod_data.ManifestRef().Id() + "/" +
         mod_data.ManifestRef().Metadata().str_version;
}

std::vector<std::string> FilesByExtension(const std::string& str_dir,
                                          std::string_view str_extension) {
  std::vector<std::string> vec_files;
  std::error_code ec{};
  for (auto it = std::filesystem::recursive_directory_iterator{str_dir, ec};
       it != std::filesystem::recursive_directory_iterator{};) {
    if (ec != std::errc{})
      break;
    if (it->is_regular_file(ec) &&
        it->path().extension().generic_string().ends_with(str_extension))
      vec_files.push_back(it->path().generic_string());
    it.increment(ec);
  }
  return vec_files;
}

std::string FilenameWithoutExtension(const std::string& str_path) {
  return std::filesystem::path{str_path}.stem().generic_string();
}

std::optional<std::int64_t> FileWriteUnixSeconds(
    const std::string& str_path) {
  std::error_code ec{};
  const std::filesystem::file_time_type tm_stamp =
      std::filesystem::last_write_time(std::filesystem::path{str_path}, ec);
  if (ec != std::errc{})
    return std::nullopt;
  const auto t8_file_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(
          tm_stamp.time_since_epoch())
          .count();
  return t8_file_seconds - 11644473600LL;
}

std::tm LocalTmOf(std::int64_t t8_seconds) {
  const std::time_t t_time = static_cast<std::time_t>(t8_seconds);
  std::tm tm_out{};
#ifdef _WIN32
  localtime_s(&tm_out, &t_time);
#else
  localtime_r(&t_time, &tm_out);
#endif
  return tm_out;
}

std::string FormatFileDateTime(std::int64_t t8_seconds) {
  const std::tm tm_local = LocalTmOf(t8_seconds);
  return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                     tm_local.tm_year + 1900, tm_local.tm_mon + 1,
                     tm_local.tm_mday, tm_local.tm_hour, tm_local.tm_min,
                     tm_local.tm_sec);
}

std::string FormatFileDate(std::int64_t t8_seconds) {
  const std::tm tm_local = LocalTmOf(t8_seconds);
  return std::format("{:04}-{:02}-{:02}", tm_local.tm_year + 1900,
                     tm_local.tm_mon + 1, tm_local.tm_mday);
}

std::string FormatFileClock(std::int64_t t8_seconds) {
  const std::tm tm_local = LocalTmOf(t8_seconds);
  return std::format("{:02}:{:02}:{:02}", tm_local.tm_hour, tm_local.tm_min,
                     tm_local.tm_sec);
}

MersenneTwister& CosmeticRandomFace() {
  if (MersenneTwister* ptr_random =
          ora::ui::Ui::Instance().CosmeticRandomDeps())
    return *ptr_random;
  static MersenneTwister mt_fallback{0};
  return mt_fallback;
}

bool ContainsIgnoreCase(std::string_view str_haystack,
                        std::string_view str_needle) {
  if (str_needle.empty())
    return true;
  if (str_haystack.size() < str_needle.size())
    return false;
  for (std::size_t i = 0; i + str_needle.size() <= str_haystack.size(); i++)
    if (EqualsIgnoreCase(str_haystack.substr(i, str_needle.size()),
                         str_needle))
      return true;
  return false;
}

using MapOrderByFn = std::function<long(const map::MapPreview&)>;

bool StaticOrderByEquals(const MapOrderByFn& fn_a, const MapOrderByFn& fn_b) {
  const auto ptr_target_a = fn_a.template target<long (*)(const map::MapPreview&)>();
  const auto ptr_target_b = fn_b.template target<long (*)(const map::MapPreview&)>();
  if (ptr_target_a != nullptr || ptr_target_b != nullptr)
    return ptr_target_a == ptr_target_b;
  return !fn_a && !fn_b;
}

template <class T>
T RandomOf(const std::vector<T>& vec_items, MersenneTwister& rng) {
  if (vec_items.empty())
    return T{};
  return vec_items[static_cast<std::size_t>(
      rng.Next(static_cast<std::int32_t>(vec_items.size())))];
}

}  // namespace

map::MapPreview& GameInfoMapPreview(const server::GameInformation& info,
                                     game::ModData& /*mod_data*/) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache == nullptr)
    return map::MapCache::UnknownMap();
  return ptr_cache->At(info.MapUid);
}

std::string GameInfoResolvedPlayerName(
    const server::GameInformation& info,
    const server::GameInformationPlayer& player) {
  if (player.IsBot) {
    std::int64_t n_number = 1;
    for (const auto& p : info.vec_players) {
      if (p.BotType != player.BotType)
        continue;
      if (&p == &player)
        break;
      ++n_number;
    }
    return Msg2("enumerated-bot-name", "name",
                fluent::FluentValue{Msg(player.Name)}, "number",
                fluent::FluentValue{n_number});
  }
  return player.Name;
}

bool PromptConfirmReplayCompatibility(const net::ReplayMetadata& meta,
                                      game::ModData& mod_data,
                                      std::function<void()> fn_on_cancel) {
  const auto incompatible =
      [&](const std::string& str_text, const FluentArgs& vec_args = {}) {
        ButtonPrompt("dialog-incompatible-replay.title", str_text, nullptr,
                     "dialog-incompatible-replay.confirm",
                     std::move(fn_on_cancel),
                     "dialog-incompatible-replay.confirm", nullptr, "",
                     vec_args);
        return false;
      };

  const std::optional<server::GameInformation> info =
      server::GameInformation::Deserialize(meta.GameInfoData(),
                                           meta.FilePath());
  if (!info.has_value())
    return incompatible("dialog-incompatible-replay.prompt");
  if (info->Version.empty())
    return incompatible("dialog-incompatible-replay.prompt-unknown-version");
  if (info->Mod.empty())
    return incompatible("dialog-incompatible-replay.prompt-unknown-mod");

  // Game.Mods 已安装模子表 = 当前模子(ExternalMods 面随其批;COVERAGE)
  if (!EqualsIgnoreCase(info->Mod, mod_data.ManifestRef().Id()))
    return incompatible("dialog-incompatible-replay.prompt-unavailable-mod",
                        MakeArgs({{"mod", info->Mod}}));
  if (info->Version != mod_data.ManifestRef().Metadata().str_version)
    return incompatible(
        "dialog-incompatible-replay.prompt-incompatible-version",
        MakeArgs({{"version", info->Version}}));

  if (GameInfoMapPreview(*info, mod_data).Status() != map::MapStatus::Available)
    return incompatible("dialog-incompatible-replay.prompt-unavailable-map",
                        MakeArgs({{"map", info->MapUid}}));

  return true;
}

std::optional<double> GetGameDurationSeconds(const net::GameSave* save) {
  if (save == nullptr || !save->opt_global_settings.has_value() ||
      save->opt_global_settings->GameTimestep <= 0 ||
      save->LastOrdersFrame < 0)
    return std::nullopt;
  return static_cast<double>(
             static_cast<std::int64_t>(save->LastOrdersFrame) *
             save->opt_global_settings->GameTimestep) /
         1000.0;
}

std::string FormatGameDuration(std::optional<double> fp8_seconds) {
  if (!fp8_seconds.has_value())
    return "?";
  const std::int64_t t8_ms = static_cast<std::int64_t>(*fp8_seconds * 1000.0);
  return std::format("{:02}:{:02}:{:02}", t8_ms / 3600000,
                     (t8_ms / 60000) % 60, (t8_ms / 1000) % 60);
}

bool IsValidNewSaveName(const std::string& str_new_name,
                        const std::string& str_initial_name,
                        const std::string& str_directory) {
  if (str_new_name == str_initial_name)
    return false;

  bool b_has_content = false;
  for (const char c : str_new_name)
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      b_has_content = true;
      break;
    }
  if (!b_has_content)
    return false;

  const auto is_invalid = [](char c) {
    return c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
           c == '\\' || c == '|' || c == '?' || c == '*' || c == '\0' ||
           (static_cast<unsigned char>(c) < 0x20);
  };
  for (const char c : str_new_name)
    if (is_invalid(c))
      return false;

  std::error_code ec{};
  if (std::filesystem::exists(std::filesystem::path{str_directory} /
                                  str_new_name,
                              ec))
    return false;

  return true;
}

// ———— ReplayBrowserLogic ————

ReplayBrowserLogic::Filter ReplayBrowserLogic::filter_{};

ReplayBrowserLogic::ReplayBrowserLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    std::function<void()> fn_on_exit, std::function<void()> fn_on_start)
    : fn_on_start_{std::move(fn_on_start)}, ptr_mod_data_{&mod_data} {
  ptr_map_ = &map::MapCache::UnknownMap();
  ptr_panel_ = &widget;
  ptr_alive_ = std::make_shared<bool>(true);

  // MAP_PREVIEW 子树装载(MapPreviewLogic)随大厅批;COVERAGE 登记

  fn_unsubscribe_game_start_ = ora::ui::Ui::Instance().SubscribeBeforeGameStart(
      [this] {
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_start_();
      });

  ptr_player_list_ = widget.Get<ora::ui::ScrollPanelWidget>("PLAYER_LIST");
  up_player_header_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("HEADER"));
  up_player_template_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("TEMPLATE"));
  ptr_player_list_->RemoveChildren();

  widget.Get<ora::ui::ButtonWidget>("CANCEL_BUTTON")->fn_on_click =
      [this, fn_on_exit = std::move(fn_on_exit)]() mutable {
        b_cancel_loading_ = true;
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_exit();
      };

  ptr_replay_list_ = widget.Get<ora::ui::ScrollPanelWidget>("REPLAY_LIST");
  up_replay_template_ = ptr_replay_list_->RemoveChild(
      *ptr_replay_list_->Get<ora::ui::ScrollItemWidget>("REPLAY_TEMPLATE"));

  const std::string str_dir = BaseDirectory("Replays", mod_data);
  std::error_code ec{};
  if (std::filesystem::exists(str_dir, ec))
    LoadReplaysAsync(str_dir);

  auto* ptr_watch = widget.Get<ora::ui::ButtonWidget>("WATCH_BUTTON");
  ptr_watch->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1) ||
           ptr_map_->Status() != map::MapStatus::Available;
  };
  ptr_watch->fn_on_click = [this] { WatchReplay(); };

  widget.Get("MAP_PREVIEW_ROOT")->fn_is_visible = [this] {
    return sz_selected_ != static_cast<std::size_t>(-1);
  };
  widget.Get("REPLAY_INFO")->fn_is_visible = [this] {
    return sz_selected_ != static_cast<std::size_t>(-1);
  };

  auto transform_duration = std::make_shared<
      core::CachedTransform<std::string, std::string>>(
      [](const std::string&) { return std::string{}; });
  widget.Get<ora::ui::LabelWidget>("DURATION")->fn_get_text =
      [this, transform_duration]() {
        if (sz_selected_ == static_cast<std::size_t>(-1) ||
            !vec_replays_[sz_selected_].info_game.has_value())
          return transform_duration->Update({});
        const auto& info = *vec_replays_[sz_selected_].info_game;
        return transform_duration->Update(Msg1(
            "label-duration", "time",
            fluent::FluentValue{static_cast<std::int64_t>(
                info.DurationSeconds())}));
      };

  SetupFilters();
  SetupManagement();
}

ReplayBrowserLogic::~ReplayBrowserLogic() {
  *ptr_alive_ = false;
  if (fn_unsubscribe_game_start_) {
    fn_unsubscribe_game_start_();
    fn_unsubscribe_game_start_ = nullptr;
  }
}

void ReplayBrowserLogic::LoadReplaysAsync(const std::string& str_dir) {
  std::thread([this, str_dir]() {
    struct Loaded {
      std::unique_ptr<net::ReplayMetadata> up_metadata;
      std::optional<server::GameInformation> info;
    };
    std::vector<Loaded> vec_loaded;
    for (const std::string& str_file : FilesByExtension(str_dir, ".orarep")) {
      if (b_cancel_loading_.load(std::memory_order_relaxed))
        return;
      auto up_meta = net::ReplayMetadata::ReadFile(str_file);
      if (up_meta == nullptr)
        continue;
      const std::string str_payload = up_meta->GameInfoData();
      const std::string str_path = up_meta->FilePath();
      vec_loaded.push_back(
          Loaded{std::move(up_meta),
                 server::GameInformation::Deserialize(str_payload,
                                                      str_path)});
    }

    if (b_cancel_loading_.load(std::memory_order_relaxed))
      return;

    std::stable_sort(
        vec_loaded.begin(), vec_loaded.end(),
        [](const Loaded& a, const Loaded& b) {
          const std::int64_t t_a =
              a.info.has_value() ? a.info->StartTimeUtc.ToUnixSeconds() : 0;
          const std::int64_t t_b =
              b.info.has_value() ? b.info->StartTimeUtc.ToUnixSeconds() : 0;
          return t_a > t_b;
        });

    const auto ptr_loaded =
        std::make_shared<std::vector<Loaded>>(std::move(vec_loaded));
    const std::weak_ptr<bool> weak_alive = ptr_alive_;
    ora::ui::Ui::Instance().RunAfterTickDefer([this, ptr_loaded,
                                               weak_alive]() {
      const auto ptr_alive = weak_alive.lock();
      if (ptr_alive == nullptr || !*ptr_alive)
        return;
      ptr_replay_list_->RemoveChildren();
      vec_files_for_test_.clear();
      for (auto& loaded : *ptr_loaded) {
        vec_files_for_test_.push_back(loaded.up_metadata->FilePath());
        AddReplay(std::move(loaded.up_metadata), std::move(loaded.info));
      }
      SetupReplayDependentFilters();
      ApplyFilter();
    });
  }).detach();
}

void ReplayBrowserLogic::SetupFilters() {
  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_GAMETYPE_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<GameType, std::string>> vec_options{
          {GameType::Any, ButtonText(*ptr_ddb)},
          {GameType::Singleplayer, Msg("options-replay-type.singleplayer")},
          {GameType::Multiplayer, Msg("options-replay-type.multiplayer")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_type)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<GameType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<GameType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_type == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_type = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_DATE_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<DateType, std::string>> vec_options{
          {DateType::Any, ButtonText(*ptr_ddb)},
          {DateType::Today, Msg("options-replay-date.today")},
          {DateType::LastWeek, Msg("options-replay-date.last-week")},
          {DateType::LastFortnight, Msg("options-replay-date.last-fortnight")},
          {DateType::LastMonth, Msg("options-replay-date.last-month")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_date)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<DateType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<DateType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_date == kind;
                      },
                      [kind = option.first] { filter_.kind_date = kind; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_DURATION_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<DurationType, std::string>> vec_options{
          {DurationType::Any, ButtonText(*ptr_ddb)},
          {DurationType::VeryShort,
           Msg("options-replay-duration.very-short")},
          {DurationType::Short, Msg("options-replay-duration.short")},
          {DurationType::Medium, Msg("options-replay-duration.medium")},
          {DurationType::Long, Msg("options-replay-duration.long")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_duration)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<DurationType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<DurationType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_duration == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_duration = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_OUTCOME_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      ptr_ddb->fn_is_disabled = [] { return filter_.str_player_name.empty(); };
      const std::vector<std::pair<server::WinState, std::string>>
          vec_options{
              {server::WinState::Undefined, ButtonText(*ptr_ddb)},
              {server::WinState::Lost, Msg("options-winstate.defeat")},
              {server::WinState::Won, Msg("options-winstate.victory")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_outcome)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<server::WinState, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<server::WinState, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_outcome == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_outcome = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_button = ptr_panel_->Get<ora::ui::ButtonWidget>(
        "FLT_RESET_BUTTON");
    ptr_button->fn_is_disabled = [] { return filter_.IsEmpty(); };
    ptr_button->fn_on_click = [] {
      filter_ = Filter{};
    };
  }
}

void ReplayBrowserLogic::SetupReplayDependentFilters() {
  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_MAPNAME_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value()) {
          bool b_found = false;
          for (const std::string& str_existing : vec_options)
            if (EqualsIgnoreCase(str_existing,
                                 state.info_game->MapTitle)) {
              b_found = true;
              break;
            }
          if (!b_found)
            vec_options.push_back(state.info_game->MapTitle);
        }
      std::sort(vec_options.begin(), vec_options.end(),
                LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_map_name.empty() ? str_any_text
                                            : filter_.str_map_name;
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_map_name,
                                                str_option);
                      },
                      [str_option] { filter_.str_map_name = str_option; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty() ? str_any_text
                                                  : str_option;
                      };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_PLAYER_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value())
          for (const auto& p : state.info_game->vec_players) {
            const std::string str_name = GameInfoResolvedPlayerName(
                *state.info_game, p);
            bool b_found = false;
            for (const std::string& str_existing : vec_options)
              if (EqualsIgnoreCase(str_existing, str_name)) {
                b_found = true;
                break;
              }
            if (!b_found)
              vec_options.push_back(str_name);
          }
      std::sort(vec_options.begin(), vec_options.end(), LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_player_name.empty() ? str_any_text
                                               : filter_.str_player_name;
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_player_name,
                                                str_option);
                      },
                      [str_option] {
                        filter_.str_player_name = str_option;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty() ? str_any_text
                                                  : str_option;
                      };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_FACTION_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      ptr_ddb->fn_is_disabled = [] { return filter_.str_player_name.empty(); };
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value())
          for (const auto& p : state.info_game->vec_players) {
            if (p.FactionName.empty())
              continue;
            bool b_found = false;
            for (const std::string& str_existing : vec_options)
              if (EqualsIgnoreCase(str_existing, p.FactionName)) {
                b_found = true;
                break;
              }
            if (!b_found)
              vec_options.push_back(p.FactionName);
          }
      std::sort(vec_options.begin(), vec_options.end(), LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_faction.empty()
                   ? str_any_text
                   : Msg(filter_.str_faction);
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_faction,
                                                str_option);
                      },
                      [str_option] { filter_.str_faction = str_option; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty()
                                   ? str_any_text
                                   : Msg(str_option);
                      };
                  return up_item;
                });
          };
    }
  }
}

void ReplayBrowserLogic::SetupManagement() {
  auto* ptr_rename =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_RENSEL_BUTTON");
  ptr_rename->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1);
  };
  ptr_rename->fn_on_click = [this] {
    if (sz_selected_ == static_cast<std::size_t>(-1))
      return;
    net::ReplayMetadata& meta = *vec_replays_[sz_selected_].up_metadata;
    const std::string str_initial_name =
        FilenameWithoutExtension(meta.FilePath());
    const std::string str_directory =
        std::filesystem::path{meta.FilePath()}
            .parent_path()
            .generic_string();

    TextInputPrompt(
        "dialog-rename-replay.title", "dialog-rename-replay.prompt",
        str_initial_name,
        [this, &meta](const std::string& str_new_name) {
          ora::ui::ScrollItemWidget* ptr_item =
              vec_replays_[sz_selected_].ptr_item;
          if (meta.RenameFile(str_new_name)) {
            ptr_item->fn_get_text = [str_new_name] { return str_new_name; };
            if (auto* ptr_label =
                    ptr_item->GetOrNull<ora::ui::LabelWithTooltipWidget>(
                        "TITLE"))
              ptr_label->fn_get_text = [str_new_name] {
                return str_new_name;
              };
          }
        },
        nullptr, "dialog-rename-replay.confirm", "",
        [str_initial_name, str_directory](const std::string& str_new_name) {
          if (str_new_name == str_initial_name)
            return false;
          if (!IsValidNewSaveName(str_new_name, "", str_directory))
            return false;
          return true;
        });
  };

  auto* ptr_delete =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_DELSEL_BUTTON");
  ptr_delete->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1);
  };
  ptr_delete->fn_on_click = [this] {
    if (sz_selected_ == static_cast<std::size_t>(-1))
      return;
    const std::size_t sz_target = sz_selected_;
    ButtonPrompt(
        "dialog-delete-replay.title", "dialog-delete-replay.prompt",
        [this, sz_target] {
          std::error_code ec{};
          std::filesystem::remove(
              vec_replays_[sz_target].up_metadata->FilePath(), ec);
          if (sz_target == sz_selected_)
            SelectReplay(static_cast<std::size_t>(-1));
          ptr_replay_list_->RemoveChild(
              *vec_replays_[sz_target].ptr_item);
          vec_replays_.erase(vec_replays_.begin() +
                             static_cast<std::ptrdiff_t>(sz_target));
          if (sz_selected_ == static_cast<std::size_t>(-1))
            SelectFirstVisibleReplay();
        },
        "dialog-delete-replay.confirm", nullptr, "",
        nullptr, "",
        MakeArgs({{"replay", FilenameWithoutExtension(
                                vec_replays_[sz_target]
                                    .up_metadata->FilePath())}}));
  };

  auto* ptr_delete_all =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_DELALL_BUTTON");
  ptr_delete_all->fn_is_disabled = [this] {
    for (const auto& state : vec_replays_)
      if (state.b_visible)
        return false;
    return true;
  };
  ptr_delete_all->fn_on_click = [this] {
    std::vector<std::size_t> vec_list;
    for (std::size_t i = 0; i < vec_replays_.size(); ++i)
      if (vec_replays_[i].b_visible)
        vec_list.push_back(i);
    if (vec_list.empty())
      return;

    const auto delete_all = [this, vec_list] {
      for (std::size_t i = vec_list.size(); i-- > 0;) {
        std::error_code ec{};
        std::filesystem::remove(
            vec_replays_[vec_list[i]].up_metadata->FilePath(), ec);
        ptr_replay_list_->RemoveChild(*vec_replays_[vec_list[i]].ptr_item);
        vec_replays_.erase(vec_replays_.begin() +
                           static_cast<std::ptrdiff_t>(vec_list[i]));
      }
      sz_selected_ = static_cast<std::size_t>(-1);
      SelectFirstVisibleReplay();
    };

    if (vec_list.size() == 1) {
      const std::size_t sz_target = vec_list[0];
      ButtonPrompt(
          "dialog-delete-replay.title", "dialog-delete-replay.prompt",
          [this, sz_target, delete_all] {
            std::error_code ec{};
            std::filesystem::remove(
                vec_replays_[sz_target].up_metadata->FilePath(), ec);
            ptr_replay_list_->RemoveChild(
                *vec_replays_[sz_target].ptr_item);
            vec_replays_.erase(vec_replays_.begin() +
                               static_cast<std::ptrdiff_t>(sz_target));
            sz_selected_ = static_cast<std::size_t>(-1);
            SelectFirstVisibleReplay();
          },
          "dialog-delete-replay.confirm", nullptr, "", nullptr, "",
          MakeArgs({{"replay", FilenameWithoutExtension(
                                  vec_replays_[sz_target]
                                      .up_metadata->FilePath())}}));
      return;
    }

    ButtonPrompt(
        "dialog-delete-all-replays.title",
        "dialog-delete-all-replays.prompt", delete_all,
        "dialog-delete-all-replays.confirm", nullptr, "", nullptr, "",
        MakeArgs({{"count", std::to_string(vec_list.size())}}));
  };
}

void ReplayBrowserLogic::SelectFirstVisibleReplay() {
  for (std::size_t i = 0; i < vec_replays_.size(); ++i)
    if (vec_replays_[i].b_visible) {
      SelectReplay(i);
      return;
    }
  SelectReplay(static_cast<std::size_t>(-1));
}

void ReplayBrowserLogic::SelectReplay(std::size_t sz_index) {
  sz_selected_ = sz_index;
  ptr_map_ = sz_index != static_cast<std::size_t>(-1) &&
                     vec_replays_[sz_index].info_game.has_value()
                 ? &GameInfoMapPreview(*vec_replays_[sz_index].info_game,
                                       *ptr_mod_data_)
                 : &map::MapCache::UnknownMap();
  if (sz_index == static_cast<std::size_t>(-1))
    return;

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_map_->Status() == map::MapStatus::Unavailable &&
      ptr_cache != nullptr && ptr_cache->AllowDownloading())
    ptr_cache->QueryRemoteMapDetails(
        game::WebServices{}.MapRepository, {ptr_map_->Uid()});

  const server::GameInformation& info =
      *vec_replays_[sz_index].info_game;

  std::vector<std::pair<int, std::vector<const server::GameInformationPlayer*>>>
      vec_players_by_team;
  for (const auto& p : info.vec_players) {
    bool b_found = false;
    for (auto& [team, vec_members] : vec_players_by_team)
      if (team == p.Team) {
        vec_members.push_back(&p);
        b_found = true;
        break;
      }
    if (!b_found)
      vec_players_by_team.emplace_back(
          p.Team, std::vector<const server::GameInformationPlayer*>{&p});
  }
  std::sort(vec_players_by_team.begin(), vec_players_by_team.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  const bool b_no_teams = vec_players_by_team.size() == 1;
  ptr_player_list_->RemoveChildren();
  for (const auto& [int4_team, vec_members] : vec_players_by_team) {
    const std::string str_label =
        b_no_teams
            ? Msg("label-players")
            : (int4_team > 0
                   ? Msg1("label-team-name", "team",
                          fluent::FluentValue{
                              static_cast<std::int64_t>(int4_team)})
                   : Msg("label-no-team"));
    if (!str_label.empty()) {
      auto up_header = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(up_player_header_.get()),
          [] { return false; }, [] {});
      up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_label] { return str_label; };
      ptr_player_list_->AddChild(std::move(up_header));
    }

    for (const server::GameInformationPlayer* ptr_player : vec_members) {
      const core::Color color = ptr_player->Color;
      const std::string str_name =
          GameInfoResolvedPlayerName(info, *ptr_player);
      auto up_item = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(up_player_template_.get()),
          [] { return false; }, [] {});
      up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_name] { return str_name; };
      up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_color =
          [color] { return color; };
      auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FLAG");
      ptr_flag->fn_get_image_collection = [] { return "flags"; };
      ptr_flag->fn_get_image_name =
          [faction_id = ptr_player->FactionId] { return faction_id; };
      ptr_player_list_->AddChild(std::move(up_item));
    }
  }
}

void ReplayBrowserLogic::WatchReplay() {
  if (sz_selected_ != static_cast<std::size_t>(-1) &&
      PromptConfirmReplayCompatibility(
          *vec_replays_[sz_selected_].up_metadata, *ptr_mod_data_)) {
    b_cancel_loading_ = true;
    ora::ui::Ui::Instance().JoinReplayDeps(
        vec_replays_[sz_selected_].up_metadata->FilePath());
  }
}

void ReplayBrowserLogic::AddReplay(
    std::unique_ptr<net::ReplayMetadata> up_meta,
    std::optional<server::GameInformation> info) {
  ReplayState state{};
  state.up_metadata = std::move(up_meta);
  state.info_game = std::move(info);

  const std::string str_item_text =
      FilenameWithoutExtension(state.up_metadata->FilePath());
  const std::size_t sz_index = vec_replays_.size();
  auto up_item = ora::ui::ScrollItemWidget::Setup(
      *static_cast<ora::ui::ScrollItemWidget*>(up_replay_template_.get()),
      [this, sz_index] { return sz_selected_ == sz_index; },
      [this, sz_index] { SelectReplay(sz_index); },
      [this] { WatchReplay(); });
  up_item->fn_get_text = [str_item_text] { return str_item_text; };
  if (auto* ptr_label =
          up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("TITLE"))
    ptr_label->fn_get_text = [str_item_text] { return str_item_text; };

  state.ptr_item = up_item.get();
  up_item->fn_is_visible = [this, sz_index] {
    return vec_replays_[sz_index].b_visible;
  };
  ptr_replay_list_->AddChild(std::move(up_item));
  vec_replays_.push_back(std::move(state));
}

void ReplayBrowserLogic::ApplyFilter() {
  for (auto& state : vec_replays_)
    state.b_visible = !state.info_game.has_value() ||
                      EvaluateReplayVisibility(*state.info_game);

  if (sz_selected_ == static_cast<std::size_t>(-1) ||
      !vec_replays_[sz_selected_].b_visible)
    SelectFirstVisibleReplay();

  if (ptr_replay_list_->ptr_layout != nullptr)
    ptr_replay_list_->ptr_layout->AdjustChildren();
  ptr_replay_list_->ScrollToSelectedItem();
}

bool ReplayBrowserLogic::EvaluateReplayVisibility(
    const server::GameInformation& info) {
  if ((filter_.kind_type == GameType::Multiplayer && info.IsSinglePlayer()) ||
      (filter_.kind_type == GameType::Singleplayer &&
       !info.IsSinglePlayer()))
    return false;

  if (filter_.kind_date != DateType::Any) {
    double fp_days = 30.0;
    switch (filter_.kind_date) {
      case DateType::Today:
        fp_days = 1.0;
        break;
      case DateType::LastWeek:
        fp_days = 7.0;
        break;
      case DateType::LastFortnight:
        fp_days = 14.0;
        break;
      case DateType::LastMonth:
      case DateType::Any:
        fp_days = 30.0;
        break;
    }
    const std::int64_t t8_cutoff =
        server::GameDateTime::UtcNow().ToUnixSeconds() -
        static_cast<std::int64_t>(fp_days * 86400.0);
    if (info.StartTimeUtc.ToUnixSeconds() < t8_cutoff)
      return false;
  }

  if (filter_.kind_duration != DurationType::Any) {
    const double fp_minutes = info.DurationSeconds() / 60.0;
    switch (filter_.kind_duration) {
      case DurationType::VeryShort:
        if (fp_minutes >= 5)
          return false;
        break;
      case DurationType::Short:
        if (fp_minutes < 5 || fp_minutes >= 20)
          return false;
        break;
      case DurationType::Medium:
        if (fp_minutes < 20 || fp_minutes >= 60)
          return false;
        break;
      case DurationType::Long:
        if (fp_minutes < 60)
          return false;
        break;
      case DurationType::Any:
        break;
    }
  }

  if (!filter_.str_map_name.empty() &&
      !EqualsIgnoreCase(filter_.str_map_name, info.MapTitle))
    return false;

  if (!filter_.str_player_name.empty()) {
    const server::GameInformationPlayer* ptr_player = nullptr;
    for (const auto& p : info.vec_players)
      if (EqualsIgnoreCase(filter_.str_player_name,
                           GameInfoResolvedPlayerName(info, p))) {
        ptr_player = &p;
        break;
      }
    if (ptr_player == nullptr)
      return false;

    if (filter_.kind_outcome != server::WinState::Undefined &&
        filter_.kind_outcome != ptr_player->Outcome)
      return false;

    if (!filter_.str_faction.empty() &&
        !EqualsIgnoreCase(filter_.str_faction, ptr_player->FactionName))
      return false;
  }

  return true;
}

std::string BuildSaveTooltipText(const std::string& str_save_path,
                                 const net::GameSave* ptr_save,
                                 game::ModData& mod_data) {
  std::string str_map_title = "?";
  if (ptr_save != nullptr && ptr_save->opt_global_settings.has_value()) {
    map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
    if (ptr_cache != nullptr)
      str_map_title = ptr_cache->At(
          ptr_save->opt_global_settings->Map).Title();
  }

  std::optional<std::int64_t> t8_created = FileWriteUnixSeconds(str_save_path);
  const std::string str_creation_date =
      t8_created.has_value() ? FormatFileDateTime(*t8_created) : std::string{"?"};
  const std::string str_duration_text =
      FormatGameDuration(GetGameDurationSeconds(ptr_save));
  const std::string str_player_count = std::to_string(
      ptr_save != nullptr ? ptr_save->vec_slot_clients.size() : 0);

  std::string str_out;
  str_out += Msg("tooltip-savegame-date-created") + ": " + str_creation_date;
  str_out += "\n" + Msg("tooltip-savegame-map") + ": " + str_map_title;
  str_out += "\n" + Msg("tooltip-savegame-duration") + ": " + str_duration_text;
  str_out += "\n" + Msg("tooltip-savegame-players") + ": " + str_player_count;
  return str_out;
}

std::pair<std::optional<std::string>, std::optional<std::string>>
SplitOnFirstToken(const std::string& str_input,
                  const std::string& str_token) {
  if (str_input.empty())
    return {std::nullopt, std::nullopt};

  const std::size_t pos_split = str_input.find(str_token);
  if (pos_split == std::string::npos || pos_split == 0)
    return {str_input, std::nullopt};

  return {str_input.substr(0, pos_split),
          str_input.substr(pos_split + str_token.size())};
}

std::string MapChooserMapSizeLabel(const Size& size) {
  const int int4_area = size.Width * size.Height;
  const std::string_view str_label =
      int4_area >= 120 * 120
          ? std::string_view{"label-map-size-huge"}
          : int4_area >= 90 * 90
                ? std::string_view{"label-map-size-large"}
                : int4_area >= 60 * 60
                      ? std::string_view{"label-map-size-medium"}
                      : std::string_view{"label-map-size-small"};
  return std::format("{}x{} ({})", size.Width, size.Height,
                     Msg(str_label));
}

std::function<std::vector<server::LobbyOptionData>(
    const map::MapPreview&)>* ptr_lobby_options_collector_ = nullptr;

std::vector<server::LobbyOptionData> CollectMapLobbyOptions(
    const map::MapPreview& preview) {
  if (ptr_lobby_options_collector_ != nullptr && *ptr_lobby_options_collector_)
    return (*ptr_lobby_options_collector_)(preview);
  return {};
}

void SetMapLobbyOptionsCollectorForTest(
    std::function<std::vector<server::LobbyOptionData>(
        const map::MapPreview&)>* fn_collector) {
  ptr_lobby_options_collector_ = fn_collector;
}

// ———— GameSaveBrowserLogic ————

GameSaveBrowserLogic::GameSaveBrowserLogic(
    ora::ui::Widget& widget, game::ModData& mod_data, sim::World& world,
    std::function<void()> fn_on_exit, std::function<void()> fn_on_start)
    : fn_on_exit_{std::move(fn_on_exit)},
      ptr_mod_data_{&mod_data},
      ptr_world_{&world} {
  ptr_panel_ = &widget;
  ptr_map_ = &map::MapCache::UnknownMap();
  (void)fn_on_start;

  fn_unsubscribe_game_start_ = ora::ui::Ui::Instance().SubscribeBeforeGameStart(
      [this] {
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_exit_();
      });

  auto* ptr_cancel_button = widget.Get<ora::ui::ButtonWidget>("CANCEL_BUTTON");
  ptr_cancel_button->fn_on_click = [this, fn_on_exit = fn_on_exit_]() mutable {
    ora::ui::Ui::Instance().CloseWindow();
    fn_on_exit();
  };

  ptr_game_list_ = widget.Get<ora::ui::ScrollPanelWidget>("GAME_LIST");
  up_game_template_ = ptr_game_list_->RemoveChild(
      *ptr_game_list_->Get<ora::ui::ScrollItemWidget>("GAME_TEMPLATE"));
  up_date_header_template_ = ptr_game_list_->RemoveChild(
      *ptr_game_list_->Get<ora::ui::ScrollItemWidget>("DATE_HEADER"));

  str_base_save_path_ = BaseDirectory("Saves", mod_data);

  widget.Get("SAVE_TITLE")->fn_is_visible = [] { return true; };

  str_default_save_filename_ = world.Map().Title();
  {
    std::error_code ec{};
    int int4_attempt = 0;
    while (std::filesystem::exists(
        std::filesystem::path{str_base_save_path_} /
            (str_default_save_filename_ + ".orasav"),
        ec)) {
      ++int4_attempt;
      str_default_save_filename_ =
          world.Map().Title() + " (" + std::to_string(int4_attempt) + ")";
    }
  }

  auto* ptr_save_button = widget.Get<ora::ui::ButtonWidget>("SAVE_BUTTON");
  ptr_save_button->fn_is_disabled = [this] {
    const std::string& str_text = ptr_save_text_field_->Text();
    bool b_has_content = false;
    for (const char c : str_text)
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
        b_has_content = true;
        break;
      }
    return !b_has_content;
  };
  ptr_save_button->fn_on_click = [this] { SaveGame(); };
  ptr_save_button->fn_is_visible = [] { return true; };

  auto* ptr_save_widgets = widget.Get("SAVE_WIDGETS");
  ptr_game_list_->bounds_widget.int4_height -=
      ptr_save_widgets->bounds_widget.int4_height;
  ptr_save_widgets->fn_is_visible = [] { return true; };

  ptr_save_text_field_ =
      ptr_save_widgets->Get<ora::ui::TextFieldWidget>("SAVE_TEXTFIELD");
  ptr_save_text_field_->fn_on_enter_key =
      [ptr_save_button](const KeyInput& e_input) {
        return ptr_save_button->HandleKeyPress(e_input);
      };
  ptr_save_text_field_->fn_on_esc_key =
      [ptr_cancel_button](const KeyInput& e_input) {
        return ptr_cancel_button->HandleKeyPress(e_input);
      };
  ptr_save_text_field_->fn_on_text_edited = [this] {
    if (ptr_save_text_field_->Text().empty()) {
      Select(nullptr);
      SelectFirstVisible();
    }
  };

  std::error_code ec{};
  if (std::filesystem::exists(str_base_save_path_, ec))
    LoadGames(
        *static_cast<ora::ui::ScrollItemWidget*>(up_game_template_.get()),
        *static_cast<ora::ui::ScrollItemWidget*>(up_date_header_template_.get()));

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache != nullptr)
    ptr_map_ = &ptr_cache->At(world.Map().Uid());

  auto* ptr_save_info = widget.Get("SAVE_INFO");

  auto* ptr_no_save_label =
      ptr_save_info->Get<ora::ui::LabelWidget>("NO_SAVE_SELECTED_LABEL");
  ptr_no_save_label->fn_get_text = [] {
    return Msg("label-gamesave-browser-panel-no-save-selected");
  };
  ptr_no_save_label->fn_is_visible = [] { return false; };

  for (const std::string_view str_id :
       {"INCOMPATIBLE_TITLE_LABEL", "INCOMPATIBLE_LABEL_A",
        "INCOMPATIBLE_LABEL_B"}) {
    auto* ptr_label =
        ptr_save_info->GetOrNull<ora::ui::LabelWidget>(str_id);
    if (ptr_label != nullptr)
      ptr_label->fn_is_visible = [this] {
        return b_has_selected_path_ && !opt_selected_save_.has_value();
      };
  }

  if (auto* ptr_date =
          ptr_save_info->GetOrNull<ora::ui::LabelWidget>(
              "SAVEGAME_INFO_DATE")) {
    ptr_date->fn_get_text = [this] {
      if (!opt_selected_save_.has_value() || !b_has_selected_path_)
        return std::string{};
      const std::optional<std::int64_t> t8_created =
          FileWriteUnixSeconds(str_selected_path_);
      return t8_created.has_value()
                 ? "Date created: " + FormatFileDateTime(*t8_created)
                 : std::string{"Date created: ?"};
    };
    ptr_date->fn_is_visible = [this] {
      return opt_selected_save_.has_value();
    };
  }

  if (auto* ptr_duration =
          ptr_save_info->GetOrNull<ora::ui::LabelWidget>(
              "SAVEGAME_INFO_DURATION")) {
    ptr_duration->fn_get_text = [this] {
      if (opt_selected_save_.has_value() && b_has_selected_path_ &&
          opt_selected_save_->opt_global_settings.has_value() &&
          opt_selected_save_->opt_global_settings->GameTimestep > 0 &&
          opt_selected_save_->LastOrdersFrame >= 0) {
        const std::int64_t t8_ms =
            static_cast<std::int64_t>(opt_selected_save_->LastOrdersFrame) *
            opt_selected_save_->opt_global_settings->GameTimestep;
        return std::format("Duration: {:02}:{:02}:{:02}", t8_ms / 3600000,
                           (t8_ms / 60000) % 60, (t8_ms / 1000) % 60);
      }
      return std::string{"Duration: ?"};
    };
    ptr_duration->fn_is_visible = [this] {
      return opt_selected_save_.has_value();
    };
  }

  ptr_player_list_ = ptr_save_info->Get<ora::ui::ScrollPanelWidget>(
      "PLAYER_LIST");
  up_player_header_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("HEADER"));
  up_player_template_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("TEMPLATE"));
  ptr_player_list_->RemoveChildren();

  auto* ptr_rename_button = widget.Get<ora::ui::ButtonWidget>("RENAME_BUTTON");
  ptr_rename_button->fn_is_disabled = [this] {
    return !opt_selected_save_.has_value();
  };
  ptr_rename_button->fn_on_click = [this] {
    const std::string str_initial_name =
        FilenameWithoutExtension(str_selected_path_);
    TextInputPrompt(
        "dialog-rename-save.title", "dialog-rename-save.prompt",
        str_initial_name,
        [this, str_initial_name](const std::string& str_new_name) {
          Rename(str_initial_name, str_new_name);
        },
        nullptr, "dialog-rename-save.confirm", {},
        [this, str_initial_name](const std::string& str_new_name) {
          return IsValidNewSaveName(str_new_name, str_initial_name,
                                    str_base_save_path_);
        });
  };

  auto* ptr_delete_button = widget.Get<ora::ui::ButtonWidget>("DELETE_BUTTON");
  ptr_delete_button->fn_is_disabled = [this] {
    return !opt_selected_save_.has_value();
  };
  ptr_delete_button->fn_on_click = [this] {
    ButtonPrompt(
        "dialog-delete-save.title", "dialog-delete-save.prompt",
        [this] {
          Delete(str_selected_path_);
          SelectFirstVisible();
        },
        "dialog-delete-save.confirm", nullptr, {}, nullptr, "",
        MakeArgs({{"save", FilenameWithoutExtension(str_selected_path_)}}));
  };

  auto* ptr_delete_all_button =
      widget.Get<ora::ui::ButtonWidget>("DELETE_ALL_BUTTON");
  ptr_delete_all_button->fn_is_disabled =
      [this] { return vec_games_.empty(); };
  ptr_delete_all_button->fn_on_click = [this] {
    ButtonPrompt(
        "dialog-delete-all-saves.title", "dialog-delete-all-saves.prompt",
        [this] {
          for (const std::string& str_save : vec_games_)
            Delete(str_save);
          ora::ui::Ui::Instance().CloseWindow();
          fn_on_exit_();
        },
        "dialog-delete-all-saves.confirm", nullptr, {}, nullptr, "",
        MakeArgs({{"count", std::to_string(vec_games_.size())}}));
  };

  SelectFirstVisible();
}

GameSaveBrowserLogic::~GameSaveBrowserLogic() {
  if (fn_unsubscribe_game_start_) {
    fn_unsubscribe_game_start_();
    fn_unsubscribe_game_start_ = nullptr;
  }
}

void GameSaveBrowserLogic::LoadGames(
    ora::ui::ScrollItemWidget& tpl_game,
    ora::ui::ScrollItemWidget& tpl_date_header) {
  ptr_game_list_->RemoveChildren();

  std::vector<std::string> vec_save_paths =
      FilesByExtension(str_base_save_path_, ".orasav");
  std::stable_sort(vec_save_paths.begin(), vec_save_paths.end(),
                   [](const std::string& a, const std::string& b) {
                     const std::optional<std::int64_t> t_a =
                         FileWriteUnixSeconds(a);
                     const std::optional<std::int64_t> t_b =
                         FileWriteUnixSeconds(b);
                     const std::int64_t v_a = t_a.value_or(0);
                     const std::int64_t v_b = t_b.value_or(0);
                     if (v_a != v_b)
                       return v_a > v_b;
                     return a < b;
                   });

  std::vector<std::pair<std::string, std::vector<std::string>>>
      vec_by_date;
  for (const std::string& str_save_path : vec_save_paths) {
    const std::optional<std::int64_t> t8_stamp =
        FileWriteUnixSeconds(str_save_path);
    const std::string str_date =
        t8_stamp.has_value() ? FormatFileDate(*t8_stamp) : std::string{"?"};
    if (!vec_by_date.empty() && vec_by_date.back().first == str_date)
      vec_by_date.back().second.push_back(str_save_path);
    else
      vec_by_date.emplace_back(
          str_date, std::vector<std::string>{str_save_path});
  }

  for (const auto& [str_date_label, vec_group] : vec_by_date) {
    auto up_header = ora::ui::ScrollItemWidget::Setup(
        tpl_date_header, [] { return false; }, [] {});
    up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
        [str_date_label] { return str_date_label; };
    up_header->fn_is_visible = [] { return true; };
    ptr_game_list_->AddChild(std::move(up_header));

    for (const std::string& str_save_path : vec_group) {
      vec_games_.push_back(str_save_path);

      std::optional<net::GameSave> opt_save =
          net::GameSave::Load(str_save_path);

      auto up_game_item =
          std::unique_ptr<ora::ui::ScrollItemWidget>(
              static_cast<ora::ui::ScrollItemWidget*>(tpl_game.Clone().release()));
      up_game_item->str_item_key = str_save_path;
      up_game_item->fn_is_visible = [] { return true; };
      up_game_item->fn_is_selected = [ptr_item = up_game_item.get(), this] {
        return str_selected_path_ == ptr_item->str_item_key;
      };
      up_game_item->fn_on_click = [ptr_item = up_game_item.get(), this] {
        const std::string str_path = ptr_item->str_item_key;
        Select(&str_path);
      };
      up_game_item->fn_on_double_click = [this] { SaveGame(); };

      const std::string str_title =
          FilenameWithoutExtension(str_save_path);
      auto* ptr_title_label =
          up_game_item->Get<ora::ui::LabelWithTooltipWidget>("TITLE");
      const std::string str_tooltip =
          BuildSaveTooltipText(str_save_path,
                               opt_save.has_value() ? &*opt_save : nullptr,
                               *ptr_mod_data_);
      ora::ui::TruncateLabelToTooltip(*ptr_title_label, str_title);
      ptr_title_label->fn_get_tooltip_text = [str_tooltip] {
        return str_tooltip;
      };

      if (auto* ptr_creation_label = up_game_item->GetOrNull<
              ora::ui::LabelWidget>("CREATION_TIME")) {
        const std::optional<std::int64_t> t8_created =
            FileWriteUnixSeconds(str_save_path);
        const std::string str_clock =
            t8_created.has_value() ? FormatFileClock(*t8_created)
                                   : std::string{"?"};
        ptr_creation_label->fn_get_text = [str_clock] { return str_clock; };
        ptr_creation_label->fn_is_visible = [ptr_item = up_game_item.get(),
                                             this] {
          return str_selected_path_ == ptr_item->str_item_key;
        };
      }

      ptr_game_list_->AddChild(std::move(up_game_item));
    }
  }
}

void GameSaveBrowserLogic::Rename(const std::string& str_old_name,
                                  const std::string& str_new_name) {
  const std::filesystem::path path_old =
      std::filesystem::path{str_base_save_path_} / (str_old_name + ".orasav");

  std::string str_unique_name = str_new_name;
  int int4_attempt = 1;
  std::error_code ec{};
  while (std::filesystem::exists(
      std::filesystem::path{str_base_save_path_} /
          (str_unique_name + ".orasav"),
      ec)) {
    str_unique_name = str_new_name + " (" + std::to_string(int4_attempt) + ")";
    ++int4_attempt;
  }

  const std::filesystem::path path_new =
      std::filesystem::path{str_base_save_path_} /
      (str_unique_name + ".orasav");

  std::filesystem::rename(path_old, path_new, ec);
  if (ec != std::errc{})
    return;

  const std::string str_new_path = path_new.generic_string();
  for (std::string& str_game : vec_games_)
    if (str_game == path_old.generic_string())
      str_game = str_new_path;
  for (ora::ui::Widget* ptr_child : ptr_game_list_->Children()) {
    auto* ptr_item =
        dynamic_cast<ora::ui::ScrollItemWidget*>(ptr_child);
    if (ptr_item == nullptr ||
        ptr_item->str_item_key != path_old.generic_string())
      continue;

    ptr_item->str_item_key = str_new_path;
    ptr_item->Get<ora::ui::LabelWidget>("TITLE")->fn_get_text =
        [str_unique_name] { return str_unique_name; };
  }

  if (str_selected_path_ == path_old.generic_string())
    str_selected_path_ = str_new_path;
}

void GameSaveBrowserLogic::Delete(const std::string& str_save_path) {
  std::error_code ec{};
  if (!std::filesystem::remove(std::filesystem::path{str_save_path}, ec) ||
      ec != std::errc{}) {
    net::TextNotificationsManager::Instance().Debug(
        Msg1("notification-save-deletion-failed", "savePath",
             fluent::FluentValue{str_save_path}));
    return;
  }

  if (str_save_path == str_selected_path_)
    Select(nullptr);

  for (ora::ui::Widget* ptr_child : ptr_game_list_->Children()) {
    auto* ptr_item =
        dynamic_cast<ora::ui::ScrollItemWidget*>(ptr_child);
    if (ptr_item != nullptr && ptr_item->str_item_key == str_save_path) {
      ptr_game_list_->RemoveChild(*ptr_item);
      break;
    }
  }
  vec_games_.erase(
      std::remove(vec_games_.begin(), vec_games_.end(), str_save_path),
      vec_games_.end());
}

void GameSaveBrowserLogic::SelectFirstVisible() {
  Select(nullptr);
  ptr_save_text_field_->TakeKeyboardFocus();
  ptr_save_text_field_->int4_cursor_position =
      static_cast<int>(ptr_save_text_field_->Text().size());
}

void GameSaveBrowserLogic::Select(const std::string* ptr_save_path) {
  if (ptr_save_path != nullptr) {
    b_has_selected_path_ = true;
    str_selected_path_ = *ptr_save_path;
    opt_selected_save_ = net::GameSave::Load(str_selected_path_);
    if (opt_selected_save_.has_value()) {
      map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
      if (ptr_cache != nullptr) {
        map::MapPreview& preview = ptr_cache->At(
            opt_selected_save_->opt_global_settings.has_value()
                ? opt_selected_save_->opt_global_settings->Map
                : std::string{});
        if (preview.Status() != map::MapStatus::Available &&
            opt_selected_save_->opt_map_generation_args.has_value()) {
          preview.UpdateFromGenerationArgs(
              *opt_selected_save_->opt_map_generation_args);
          preview.Generate();
        }
        ptr_map_ = &preview;
      }
    } else {
      ptr_map_ = &map::MapCache::UnknownMap();
    }
  } else {
    b_has_selected_path_ = false;
    str_selected_path_.clear();
    opt_selected_save_.reset();
    map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
    ptr_map_ = ptr_cache != nullptr
                   ? &ptr_cache->At(ptr_world_->Map().Uid())
                   : &map::MapCache::UnknownMap();
  }

  UpdatePlayerList();

  const std::string str_text =
      ptr_save_path == nullptr
          ? str_default_save_filename_
          : FilenameWithoutExtension(str_selected_path_);
  ptr_save_text_field_->SetText(str_text);
  ptr_save_text_field_->int4_cursor_position =
      static_cast<int>(ptr_save_text_field_->Text().size());
}

std::map<std::string, net::GameSaveSlotClient>
GameSaveBrowserLogic::GetWorldSlotClients() const {
  std::map<std::string, net::GameSaveSlotClient> map_out;
  for (const net::SessionClient& client : ptr_world_->LobbyInfo().vec_clients)
    if (!client.Slot.empty())
      map_out.emplace(client.Slot, net::GameSaveSlotClient{client});
  return map_out;
}

std::map<int, ora::ui::SpawnOccupant>
GameSaveBrowserLogic::GetSpawnOccupants() const {
  std::map<int, ora::ui::SpawnOccupant> map_occupants;

  const std::map<std::string, net::GameSaveSlotClient>* ptr_slot_clients =
      nullptr;
  std::map<std::string, net::GameSaveSlotClient> map_world_clients;
  if (opt_selected_save_.has_value()) {
    for (const auto& [str_key, slot_client] :
         opt_selected_save_->vec_slot_clients)
      map_world_clients.emplace(str_key, slot_client);
    ptr_slot_clients = &map_world_clients;
  } else if (!b_has_selected_path_) {
    map_world_clients = GetWorldSlotClients();
    ptr_slot_clients = &map_world_clients;
  }
  if (ptr_slot_clients == nullptr)
    return map_occupants;

  for (const auto& [str_key, slot_client] : *ptr_slot_clients) {
    if (slot_client.int4_spawn_point == 0)
      continue;

    net::SessionClient client{};
    client.Color = slot_client.color;
    client.Faction = slot_client.str_faction;
    client.SpawnPoint = slot_client.int4_spawn_point;
    client.Team = slot_client.int4_team;
    client.Bot = slot_client.str_bot;
    client.Name = !slot_client.str_bot.empty()
                      ? Msg(slot_client.str_bot_name)
                      : std::string{};
    map_occupants.emplace(slot_client.int4_spawn_point,
                          ora::ui::SpawnOccupant{client});
  }
  return map_occupants;
}

void GameSaveBrowserLogic::UpdatePlayerList() {
  ptr_player_list_->RemoveChildren();
  vec_player_labels_for_test_.clear();

  std::optional<std::map<std::string, net::GameSaveSlotClient>>
      opt_slot_clients;
  if (opt_selected_save_.has_value()) {
    std::map<std::string, net::GameSaveSlotClient> map_save_clients;
    for (const auto& [str_key, slot_client] :
         opt_selected_save_->vec_slot_clients)
      map_save_clients.emplace(str_key, slot_client);
    opt_slot_clients = std::move(map_save_clients);
  } else if (!b_has_selected_path_) {
    opt_slot_clients = GetWorldSlotClients();
  } else {
    return;
  }

  std::map<std::string, int> map_bot_ordinals;
  {
    std::map<std::string, int> map_next;
    for (const auto& [str_key, slot_client] : *opt_slot_clients)
      if (!slot_client.str_bot.empty())
        map_bot_ordinals[str_key] = ++map_next[slot_client.str_bot];
  }

  std::vector<std::pair<int, std::vector<std::pair<
                                std::string,
                                const net::GameSaveSlotClient*>>>>
      vec_by_team;
  for (const auto& [str_key, slot_client] : *opt_slot_clients) {
    bool b_found = false;
    for (auto& [int4_team, vec_members] : vec_by_team)
      if (int4_team == slot_client.int4_team) {
        vec_members.emplace_back(str_key, &slot_client);
        b_found = true;
        break;
      }
    if (!b_found)
      vec_by_team.emplace_back(
          slot_client.int4_team,
          std::vector<std::pair<std::string,
                                const net::GameSaveSlotClient*>>{
              {str_key, &slot_client}});
  }
  std::sort(vec_by_team.begin(), vec_by_team.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  const bool b_no_teams = vec_by_team.size() == 1;

  std::vector<std::string> vec_known_factions;
  if (const game::ActorInfo* ptr_world_actor_info =
          ptr_mod_data_->DefaultRules().FindActor("world"))
    for (const meta::RecordObject* rec_faction :
         ptr_world_actor_info->TraitInfosByInterface("FactionInfo"))
      if (const auto v =
              sim::RecordFieldString(*rec_faction, "InternalName"))
        vec_known_factions.emplace_back(*v);

  for (const auto& [int4_team, vec_members] : vec_by_team) {
    const std::string str_label =
        b_no_teams
            ? Msg("label-players")
            : (int4_team > 0
                   ? Msg1("label-team-name", "team",
                          fluent::FluentValue{
                              static_cast<std::int64_t>(int4_team)})
                   : Msg("label-no-team"));

    if (!str_label.empty()) {
      vec_player_labels_for_test_.push_back(str_label);
      auto up_header = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(
              up_player_header_.get()),
          [] { return false; }, [] {});
      up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_label] { return str_label; };
      ptr_player_list_->AddChild(std::move(up_header));
    }

    for (const auto& [str_key, ptr_slot_client] : vec_members) {
      const std::string str_display_name =
          !ptr_slot_client->str_bot.empty()
              ? Msg2("enumerated-bot-name", "name",
                     fluent::FluentValue{
                         Msg(ptr_slot_client->str_bot_name)},
                     "number",
                     fluent::FluentValue{
                         static_cast<std::int64_t>(
                             map_bot_ordinals.at(str_key))})
              : Msg("label-load-game-browser-panel-human-player");
      vec_player_labels_for_test_.push_back(str_display_name);

      const core::Color color = ptr_slot_client->color;
      auto up_item = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(
              up_player_template_.get()),
          [] { return false; }, [] {});

      auto* ptr_name_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
      ptr_name_label->fn_get_text = [str_display_name] {
        return str_display_name;
      };
      ptr_name_label->fn_get_color = [color] { return color; };

      auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FLAG");
      ptr_flag->fn_get_image_collection = [] { return "flags"; };
      const std::string str_faction = ptr_slot_client->str_faction;
      const bool b_faction_known = std::find(vec_known_factions.begin(),
                                             vec_known_factions.end(),
                                             str_faction) !=
                                   vec_known_factions.end();
      ptr_flag->fn_get_image_name = [str_faction, b_faction_known] {
        return b_faction_known ? str_faction : std::string{"Random"};
      };

      ptr_player_list_->AddChild(std::move(up_item));
    }
  }
}

void GameSaveBrowserLogic::SaveGame() {
  const std::string str_filename = ptr_save_text_field_->Text() + ".orasav";
  const std::filesystem::path path_test =
      std::filesystem::path{BaseDirectory("Saves", *ptr_mod_data_)} /
      str_filename;

  const auto fn_inner = [this, str_filename]() {
    ptr_world_->RequestGameSave(str_filename, false);
    ora::ui::Ui::Instance().CloseWindow();
    fn_on_exit_();
  };

  std::error_code ec{};
  if (std::filesystem::exists(path_test, ec)) {
    ButtonPrompt(
        "dialog-overwrite-save.title", "dialog-overwrite-save.prompt",
        fn_inner, "dialog-overwrite-save.confirm", nullptr, {}, nullptr,
        "", MakeArgs({{"file", ptr_save_text_field_->Text()}}));
  } else {
    fn_inner();
  }
}

// ——— MissionBrowserLogic ———


MissionBrowserLogic::MissionBrowserLogic(
    ora::ui::Widget& widget, game::ModData& mod_data, sim::World& world,
    std::function<void()> fn_on_start, std::function<void()> fn_on_exit,
    const std::string& str_initial_map)
    : ptr_mod_data_{&mod_data},
      fn_on_start_{std::move(fn_on_start)},
      fn_on_exit_{std::move(fn_on_exit)} {
  ptr_alive_ = std::make_shared<bool>(true);

  fn_unsubscribe_game_start_ = ora::ui::Ui::Instance().SubscribeBeforeGameStart(
      [this] {
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_start_();
      });

  ptr_mission_list_ = widget.Get<ora::ui::ScrollPanelWidget>("MISSION_LIST");
  up_header_template_ = ptr_mission_list_->RemoveChild(
      *ptr_mission_list_->Get<ora::ui::ScrollItemWidget>("HEADER"));
  up_template_ = ptr_mission_list_->RemoveChild(
      *ptr_mission_list_->Get<ora::ui::ScrollItemWidget>("TEMPLATE"));
  ptr_mission_list_->RemoveChildren();

  if (auto* ptr_title =
          widget.GetOrNull<ora::ui::LabelWidget>("MISSIONBROWSER_TITLE")) {
    const std::string str_title_text =
        ptr_title->fn_get_text ? ptr_title->fn_get_text()
                               : std::string{};
    ptr_title->fn_get_text = [this, str_title_text] {
      return kind_playing_video_ != PlayingVideo::None
                 ? (ptr_selected_map_ != nullptr ? ptr_selected_map_->Title()
                                                 : str_title_text)
                 : str_title_text;
    };
  }

  widget.Get("MISSION_INFO")->fn_is_visible = [this] {
    return ptr_selected_map_ != nullptr;
  };

  auto* ptr_preview_widget = widget.Get<ora::ui::MapPreviewWidget>(
      "MISSION_PREVIEW");
  ptr_preview_widget->fn_preview = [this] { return ptr_selected_map_; };
  ptr_preview_widget->fn_is_visible = [this] {
    return kind_playing_video_ == PlayingVideo::None;
  };

  ptr_video_player_ = widget.Get<VideoPlayerWidget>("MISSION_VIDEO");
  widget.Get("MISSION_BIN")->fn_is_visible = [this] {
    return kind_playing_video_ != PlayingVideo::None;
  };
  {
    ora::ui::WidgetArgs args_fullscreen{};
    args_fullscreen.Set("world", &world);
    ptr_fullscreen_video_player_ = ora::ui::Ui::Instance().LoadWidget(
        "FULLSCREEN_PLAYER", ora::ui::Ui::Instance().Root(),
        std::move(args_fullscreen));
  }

  ptr_mission_detail_ = widget.Get("MISSION_DETAIL");

  ptr_description_panel_ =
      ptr_mission_detail_->Get<ora::ui::ScrollPanelWidget>(
          "MISSION_DESCRIPTION_PANEL");
  ptr_description_panel_->fn_is_visible = [this] {
    return kind_panel_ == PanelType::MissionInfo;
  };

  ptr_description_ =
      ptr_description_panel_->Get<ora::ui::LabelWidget>(
          "MISSION_DESCRIPTION");
  ptr_description_font_ =
      &ora::ui::RequireWidgetFont(ptr_description_->str_font);

  ptr_options_container_ = ptr_mission_detail_->Get("MISSION_OPTIONS");
  ptr_options_container_->fn_is_visible = [this] {
    return kind_panel_ == PanelType::Options;
  };
  up_checkbox_row_template_ = ptr_options_container_->RemoveChild(
      *ptr_options_container_->Get("CHECKBOX_ROW_TEMPLATE"));
  up_dropdown_row_template_ = ptr_options_container_->RemoveChild(
      *ptr_options_container_->Get("DROPDOWN_ROW_TEMPLATE"));

  ptr_start_briefing_video_button_ =
      widget.Get<ora::ui::ButtonWidget>("START_BRIEFING_VIDEO_BUTTON");
  ptr_stop_briefing_video_button_ =
      widget.Get<ora::ui::ButtonWidget>("STOP_BRIEFING_VIDEO_BUTTON");
  ptr_stop_briefing_video_button_->fn_is_visible = [this] {
    return kind_playing_video_ == PlayingVideo::Briefing;
  };
  ptr_stop_briefing_video_button_->fn_on_click = [this] {
    StopVideo(*ptr_video_player_);
  };

  ptr_start_info_video_button_ =
      widget.Get<ora::ui::ButtonWidget>("START_INFO_VIDEO_BUTTON");
  ptr_stop_info_video_button_ =
      widget.Get<ora::ui::ButtonWidget>("STOP_INFO_VIDEO_BUTTON");
  ptr_stop_info_video_button_->fn_is_visible = [this] {
    return kind_playing_video_ == PlayingVideo::Info;
  };
  ptr_stop_info_video_button_->fn_on_click = [this] {
    StopVideo(*ptr_video_player_);
  };

  ptr_mini_options_ = widget.GetOrNull("MISSION_MINIFIED_OPTIONS");
  if (ptr_mini_options_ != nullptr) {
    ptr_mini_options_->fn_is_visible = [this] { return b_minified_options_; };
    ptr_difficulty_button_ =
        ptr_mini_options_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "DIFFICULTY");
    ptr_game_speed_button_ =
        ptr_mini_options_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "GAMESPEED");
    if (ptr_difficulty_button_ != nullptr) {
      str_unset_difficulty_ =
          ptr_difficulty_button_->fn_get_text
              ? ptr_difficulty_button_->fn_get_text()
              : std::string{};
      str_default_tooltip_ =
          ptr_difficulty_button_->fn_get_tooltip_text
              ? ptr_difficulty_button_->fn_get_tooltip_text()
              : std::string{};
    }
  }

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();

  if (ptr_cache != nullptr && !mod_data.ManifestRef().Missions().empty()) {
    std::vector<std::vector<yaml::MiniYamlNode>> vec_sources;
    vec_sources.reserve(mod_data.ManifestRef().Missions().size());
    for (const std::string& str_mission : mod_data.ManifestRef().Missions()) {
      std::vector<char> vec_bytes{};
      if (!mod_data.ModFiles().TryOpen(str_mission, vec_bytes))
        continue;
      vec_sources.push_back(yaml::MiniYaml::FromStream(
          std::string_view{vec_bytes.data(), vec_bytes.size()}, str_mission));
    }
    const std::vector<yaml::MiniYamlNode> vec_yaml =
        yaml::MiniYaml::Merge(std::move(vec_sources));

    for (const yaml::MiniYamlNode& kv : vec_yaml) {
      std::vector<const std::string*> vec_mission_map_paths;
      for (const yaml::MiniYamlNode& node : kv.Value.Nodes)
        vec_mission_map_paths.push_back(node.Key);

      std::vector<map::MapPreview*> vec_previews;
      for (map::MapPreview* ptr_preview : ptr_cache->Previews()) {
        if (ptr_preview->Class() != map::MapClassification::System ||
            ptr_preview->Status() != map::MapStatus::Available)
          continue;
        const std::string str_filename =
            std::filesystem::path{ptr_preview->Path()}
                .filename()
                .generic_string();
        for (const std::string* ptr_path : vec_mission_map_paths)
          if (*ptr_path == str_filename) {
            vec_previews.push_back(ptr_preview);
            break;
          }
      }
      std::stable_sort(
          vec_previews.begin(), vec_previews.end(),
          [&vec_mission_map_paths](const map::MapPreview* ptr_a,
                                   const map::MapPreview* ptr_b) {
            const auto index_of = [&vec_mission_map_paths](
                                      const map::MapPreview* ptr_p) {
              const std::string str_filename =
                  std::filesystem::path{ptr_p->Path()}
                      .filename()
                      .generic_string();
              for (std::size_t i = 0; i < vec_mission_map_paths.size(); i++)
                if (*vec_mission_map_paths[i] == str_filename)
                  return static_cast<int>(i);
              return -1;
            };
            return index_of(ptr_a) < index_of(ptr_b);
          });

      if (!vec_previews.empty()) {
        CreateMissionGroup(*kv.Key, vec_previews);
        vec_all_previews_.insert(vec_all_previews_.end(), vec_previews.begin(),
                                 vec_previews.end());
      }
    }
  }

  if (ptr_cache != nullptr) {
    std::vector<map::MapPreview*> vec_loose;
    for (map::MapPreview* ptr_preview : ptr_cache->Previews()) {
      if (ptr_preview->Status() != map::MapStatus::Available ||
          (static_cast<std::int32_t>(ptr_preview->Visibility()) &
           static_cast<std::int32_t>(map::MapVisibility::MissionSelector)) ==
              0)
        continue;
      bool b_in_group = false;
      for (const map::MapPreview* ptr_group : vec_all_previews_)
        if (ptr_group->Uid() == ptr_preview->Uid()) {
          b_in_group = true;
          break;
        }
      if (!b_in_group)
        vec_loose.push_back(ptr_preview);
    }

    if (!vec_loose.empty()) {
      CreateMissionGroup("Missions", vec_loose);
      vec_all_previews_.insert(vec_all_previews_.end(), vec_loose.begin(),
                               vec_loose.end());
    }

    if (!vec_all_previews_.empty()) {
      const std::string str_uid = ptr_cache->GetUpdatedMap(str_initial_map);
      map::MapPreview* ptr_map =
          str_uid.empty() ? nullptr : &ptr_cache->At(str_uid);
      const bool b_visible =
          ptr_map != nullptr &&
          (static_cast<std::int32_t>(ptr_map->Visibility()) &
           static_cast<std::int32_t>(map::MapVisibility::MissionSelector)) !=
              0;
      if (b_visible) {
        SelectMap(*ptr_map);
        ptr_mission_list_->ScrollToSelectedItem();
      } else {
        SelectMap(*vec_all_previews_.front());
      }
    }
  }

  {
    const std::weak_ptr<bool> weak_alive = ptr_alive_;
    std::thread([weak_alive, vec_previews = vec_all_previews_]() {
      const auto ptr_alive = weak_alive.lock();
      if (ptr_alive == nullptr || !*ptr_alive)
        return;
      for (map::MapPreview* ptr_preview : vec_previews)
        ptr_preview->GetMinimap();
    }).detach();
  }

  auto* ptr_start_button = widget.Get<ora::ui::ButtonWidget>(
      "STARTGAME_BUTTON");
  ptr_start_button->fn_on_click = [this] { StartMissionClicked(); };
  ptr_start_button->fn_is_disabled = [this] {
    return ptr_selected_map_ == nullptr;
  };

  widget.Get<ora::ui::ButtonWidget>("BACK_BUTTON")->fn_on_click = [this] {
    StopVideo(*ptr_video_player_);
    ora::ui::Ui::Instance().CloseWindow();
    fn_on_exit_();
  };

  auto* ptr_tab_container = widget.Get("MISSION_TABS");
  ptr_tab_container->fn_is_visible = [this] { return !b_minified_options_; };

  auto* ptr_options_tab =
      ptr_tab_container->Get<ora::ui::ButtonWidget>("OPTIONS_TAB");
  ptr_options_tab->fn_is_highlighted = [this] {
    return kind_panel_ == PanelType::Options;
  };
  ptr_options_tab->fn_on_click = [this] { kind_panel_ = PanelType::Options; };

  auto* ptr_mission_tab =
      ptr_tab_container->Get<ora::ui::ButtonWidget>("MISSIONINFO_TAB");
  ptr_mission_tab->fn_is_highlighted = [this] {
    return kind_panel_ == PanelType::MissionInfo;
  };
  ptr_mission_tab->fn_on_click = [this] {
    kind_panel_ = PanelType::MissionInfo;
  };
}

MissionBrowserLogic::~MissionBrowserLogic() {
  *ptr_alive_ = false;
  if (fn_unsubscribe_game_start_) {
    fn_unsubscribe_game_start_();
    fn_unsubscribe_game_start_ = nullptr;
  }
}

void MissionBrowserLogic::CreateMissionGroup(
    const std::string& str_title,
    const std::vector<map::MapPreview*>& vec_previews) {
  auto up_header = ora::ui::ScrollItemWidget::Setup(
      *static_cast<ora::ui::ScrollItemWidget*>(up_header_template_.get()),
      [] { return false; }, [] {});
  up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
      [str_title] { return str_title; };
  ptr_mission_list_->AddChild(std::move(up_header));

  for (map::MapPreview* ptr_preview : vec_previews) {
    auto up_item = ora::ui::ScrollItemWidget::Setup(
        *static_cast<ora::ui::ScrollItemWidget*>(up_template_.get()),
        [this, ptr_preview] {
          return ptr_selected_map_ != nullptr &&
                 ptr_selected_map_->Uid() == ptr_preview->Uid();
        },
        [this, ptr_preview] { SelectMap(*ptr_preview); },
        [this] { StartMissionClicked(); });

    ora::ui::TruncateLabelToTooltip(
        *up_item->Get<ora::ui::LabelWithTooltipWidget>("TITLE"),
        ptr_preview->Title());

    ptr_mission_list_->AddChild(std::move(up_item));
  }
}


void MissionBrowserLogic::SelectMap(map::MapPreview& preview) {
  ptr_selected_map_ = &preview;

  str_briefing_video_.clear();
  b_briefing_video_visible_ = false;
  str_info_video_.clear();
  b_info_video_visible_ = false;
  std::optional<std::string> opt_briefing;

  if (preview.WorldActorInfo() != nullptr)
    for (const meta::RecordObject* rec :
         preview.WorldActorInfo()->TraitInfosByInterface(
             "MissionDataInfo")) {
      if (const auto v = sim::RecordFieldString(*rec, "BriefingVideo"))
        str_briefing_video_ = std::string{*v};
      if (const auto v = sim::RecordFieldString(*rec, "BackgroundVideo"))
        str_info_video_ = std::string{*v};
      if (const auto v = sim::RecordFieldString(*rec, "Briefing"))
        opt_briefing = std::string{*v};
      break;
    }
  b_briefing_video_visible_ = !str_briefing_video_.empty();
  b_info_video_visible_ = !str_info_video_.empty();

  if (opt_briefing.has_value()) {
    std::string str_briefing_text = *opt_briefing;
    std::string str_translation{};
    if (preview.TryGetMessage(*opt_briefing, str_translation))
      str_briefing_text = str_translation;
    const std::string str_wrapped = ora::ui::WrapText(
        str_briefing_text, ptr_description_->bounds_widget.int4_width,
        *ptr_description_font_);
    const int int4_height = ptr_description_font_->Measure(str_wrapped).Y;
    const map::MapPreview* ptr_preview = &preview;
    const std::weak_ptr<bool> weak_alive = ptr_alive_;
    ora::ui::Ui::Instance().RunAfterTickDefer(
        [this, weak_alive, ptr_preview, str_wrapped, int4_height] {
          const auto ptr_alive_tick = weak_alive.lock();
          if (ptr_alive_tick == nullptr || !*ptr_alive_tick)
            return;
          if (ptr_preview == ptr_selected_map_) {
            ptr_description_->fn_get_text = [str_wrapped] {
              return str_wrapped;
            };
            ptr_description_->bounds_widget.int4_height = int4_height;
            if (ptr_description_panel_->ptr_layout != nullptr)
              ptr_description_panel_->ptr_layout->AdjustChildren();
            kind_panel_ = PanelType::MissionInfo;
          }
        });
  }

  ptr_start_briefing_video_button_->fn_is_visible = [this] {
    return b_briefing_video_visible_ &&
           kind_playing_video_ != PlayingVideo::Briefing;
  };
  ptr_start_briefing_video_button_->fn_on_click = [this] {
    PlayVideo(*ptr_video_player_, str_briefing_video_,
              PlayingVideo::Briefing);
  };

  ptr_start_info_video_button_->fn_is_visible = [this] {
    return b_info_video_visible_ && kind_playing_video_ != PlayingVideo::Info;
  };
  ptr_start_info_video_button_->fn_on_click = [this] {
    PlayVideo(*ptr_video_player_, str_info_video_, PlayingVideo::Info);
  };

  ptr_description_panel_->ScrollToTop();

  RebuildOptions();
}

void MissionBrowserLogic::RebuildOptions() {
  if (ptr_selected_map_ == nullptr ||
      ptr_selected_map_->WorldActorInfo() == nullptr)
    return;

  vec_mission_options_.clear();
  ptr_options_container_->RemoveChildren();

  std::vector<server::LobbyOptionData> vec_all_options =
      CollectMapLobbyOptions(*ptr_selected_map_);
  std::stable_sort(vec_all_options.begin(), vec_all_options.end(),
                   [](const server::LobbyOptionData& a,
                      const server::LobbyOptionData& b) {
                     return a.DisplayOrder < b.DisplayOrder;
                   });

  b_minified_options_ = true;
  for (const server::LobbyOptionData& option : vec_all_options)
    if (option.Id != "difficulty" && option.Id != "gamespeed")
      b_minified_options_ = false;
  if (b_minified_options_)
    BuildMinifiedOptions(vec_all_options);
  else
    BuildOptions(vec_all_options);
}

void MissionBrowserLogic::SetMapDifficulty(
    const server::LobbyOptionData& option) {
  if (!opt_selected_difficulty_.has_value())
    opt_selected_difficulty_ = option.DefaultValue;
  if (option.HasValue(*opt_selected_difficulty_))
    vec_mission_options_.emplace_back(option.Id, *opt_selected_difficulty_);
  else
    vec_mission_options_.emplace_back(option.Id, option.DefaultValue);
}

void MissionBrowserLogic::SetMapSpeed(
    const server::LobbyOptionData& option) {
  if (!opt_selected_game_speed_.has_value())
    opt_selected_game_speed_ = option.DefaultValue;
  if (option.HasValue(*opt_selected_game_speed_))
    vec_mission_options_.emplace_back(option.Id, *opt_selected_game_speed_);
  else
    vec_mission_options_.emplace_back(option.Id, option.DefaultValue);
}

void MissionBrowserLogic::OnOptionSelected(const std::string& str_option_id,
                                           const std::string& str_value) {
  if (str_option_id == "difficulty")
    opt_selected_difficulty_ = str_value;
  else if (str_option_id == "gamespeed")
    opt_selected_game_speed_ = str_value;

  vec_mission_options_.emplace_back(str_option_id, str_value);
}


void MissionBrowserLogic::BuildOptions(
    const std::vector<server::LobbyOptionData>& vec_options) {
  ora::ui::Widget* ptr_row = nullptr;
  std::vector<ora::ui::CheckboxWidget*> vec_checkbox_columns;
  std::vector<ora::ui::DropDownButtonWidget*> vec_dropdown_columns;

  int int4_y_offset = 0;
  for (const server::LobbyOptionData& option : vec_options) {
    const bool b_is_boolean = [&option] {
      for (const auto& [str_key, str_value] : option.vec_values)
        if (str_key == "True" || str_key == "False")
          return true;
      return false;
    }();
    if (!b_is_boolean)
      continue;

    for (auto it = vec_mission_options_.begin();
         it != vec_mission_options_.end();)
      it = it->first == option.Id ? vec_mission_options_.erase(it)
                                  : std::next(it);
    vec_mission_options_.emplace_back(option.Id, option.DefaultValue);

    if (vec_checkbox_columns.empty()) {
      ptr_row = up_checkbox_row_template_->Clone().release();
      ptr_row->bounds_widget.int4_y = int4_y_offset;
      int4_y_offset += ptr_row->bounds_widget.int4_height;
      for (ora::ui::Widget* ptr_child : ptr_row->Children())
        if (auto* ptr_checkbox =
                dynamic_cast<ora::ui::CheckboxWidget*>(ptr_child))
          vec_checkbox_columns.push_back(ptr_checkbox);
      ptr_options_container_->AddChild(
          std::unique_ptr<ora::ui::Widget>(ptr_row));
    }

    ora::ui::CheckboxWidget* ptr_checkbox = vec_checkbox_columns.front();
    vec_checkbox_columns.erase(vec_checkbox_columns.begin());

    ptr_checkbox->fn_get_text = [str_name = option.Name] {
      return str_name;
    };
    if (!option.Description.empty()) {
      const auto pair_split = SplitOnFirstToken(option.Description);
      ptr_checkbox->fn_get_tooltip_text = [str_text = pair_split.first] {
        return str_text.value_or(std::string{});
      };
      ptr_checkbox->fn_get_tooltip_desc = [str_desc = pair_split.second] {
        return str_desc.value_or(std::string{});
      };
    }

    ptr_checkbox->fn_is_visible = [] { return true; };
    ptr_checkbox->fn_is_checked = [this, str_id = option.Id] {
      for (const auto& [str_key, str_value] : vec_mission_options_)
        if (str_key == str_id)
          return str_value == "True";
      return false;
    };
    ptr_checkbox->fn_is_disabled = [b_locked = option.IsLocked] {
      return b_locked;
    };
    ptr_checkbox->fn_on_click = [this, str_id = option.Id] {
      for (auto& [str_key, str_value] : vec_mission_options_)
        if (str_key == str_id) {
          str_value = str_value == "True" ? "False" : "True";
          break;
        }
    };
  }

  for (const server::LobbyOptionData& option : vec_options) {
    const bool b_is_boolean = [&option] {
      for (const auto& [str_key, str_value] : option.vec_values)
        if (str_key == "True" || str_key == "False")
          return true;
      return false;
    }();
    if (b_is_boolean)
      continue;

    if (option.Id == "difficulty")
      SetMapDifficulty(option);
    else if (option.Id == "gamespeed")
      SetMapSpeed(option);
    else
      vec_mission_options_.emplace_back(option.Id, option.DefaultValue);

    if (vec_dropdown_columns.empty()) {
      ptr_row = up_dropdown_row_template_->Clone().release();
      ptr_row->bounds_widget.int4_y = int4_y_offset;
      int4_y_offset += ptr_row->bounds_widget.int4_height;
      for (ora::ui::Widget* ptr_child : ptr_row->Children())
        if (auto* ptr_dropdown =
                dynamic_cast<ora::ui::DropDownButtonWidget*>(ptr_child))
          vec_dropdown_columns.push_back(ptr_dropdown);
      ptr_options_container_->AddChild(
          std::unique_ptr<ora::ui::Widget>(ptr_row));
    }

    ora::ui::DropDownButtonWidget* ptr_val = vec_dropdown_columns.front();
    vec_dropdown_columns.erase(vec_dropdown_columns.begin());
    SetupDropdown(*ptr_val, option);

    if (auto* ptr_label = ptr_row->GetOrNull<ora::ui::LabelWidget>(
            ptr_val->str_id + "_DESC")) {
      ptr_label->fn_get_text = [str_name = option.Name] {
        return str_name + ":";
      };
      ptr_label->fn_is_visible = [] { return true; };
    }
  }
}

void MissionBrowserLogic::BuildMinifiedOptions(
    const std::vector<server::LobbyOptionData>& vec_options) {
  const server::LobbyOptionData* ptr_map_difficulty = nullptr;
  const server::LobbyOptionData* ptr_game_speed = nullptr;
  for (const server::LobbyOptionData& option : vec_options) {
    if (option.Id == "difficulty" && ptr_map_difficulty == nullptr)
      ptr_map_difficulty = &option;
    else if (option.Id == "gamespeed" && ptr_game_speed == nullptr)
      ptr_game_speed = &option;
  }

  if (ptr_difficulty_button_ != nullptr) {
    if (ptr_map_difficulty != nullptr) {
      SetMapDifficulty(*ptr_map_difficulty);
      SetupDropdown(*ptr_difficulty_button_, *ptr_map_difficulty);
    } else {
      b_difficulty_disabled_ = true;
      ptr_difficulty_button_->fn_is_disabled = [] { return true; };
      ptr_difficulty_button_->fn_get_text = [this] {
        return str_unset_difficulty_;
      };
      ptr_difficulty_button_->fn_get_tooltip_text = [this] {
        return str_default_tooltip_;
      };
    }
  }

  if (ptr_game_speed_button_ != nullptr) {
    if (ptr_game_speed != nullptr) {
      SetMapSpeed(*ptr_game_speed);
      SetupDropdown(*ptr_game_speed_button_, *ptr_game_speed);
    } else {
      ptr_game_speed_button_->fn_is_disabled = [] { return true; };
      ptr_game_speed_button_->fn_get_text = [] {
        return Msg("label-not-available");
      };
    }
  }
}


void MissionBrowserLogic::SetupDropdown(
    ora::ui::DropDownButtonWidget& dropdown,
    const server::LobbyOptionData& option) {
  dropdown.fn_get_text = [this, option] {
    std::string str_selected;
    for (const auto& [str_key, str_value] : vec_mission_options_)
      if (str_key == option.Id) {
        str_selected = str_value;
        break;
      }
    const std::string* ptr_label = option.FindValueLabel(str_selected);
    return ptr_label != nullptr ? *ptr_label : Msg("label-not-available");
  };

  if (!option.Description.empty()) {
    const auto pair_split = SplitOnFirstToken(option.Description);
    dropdown.fn_get_tooltip_text = [str_text = pair_split.first] {
      return str_text.value_or(std::string{});
    };
    dropdown.fn_get_tooltip_desc = [str_desc = pair_split.second] {
      return str_desc.value_or(std::string{});
    };
  }

  dropdown.fn_is_visible = [] { return true; };
  dropdown.fn_is_disabled = [b_locked = option.IsLocked] {
    return b_locked;
  };

  dropdown.fn_on_mouse_down = [this, &dropdown,
                               option](const MouseInput&) {
    dropdown.ShowDropDown<std::pair<std::string, std::string>>(
        "LABEL_DROPDOWN_TEMPLATE",
        static_cast<int>(option.vec_values.size()) * 30, option.vec_values,
        [this, option](const std::pair<std::string, std::string>& pair_value,
                       ora::ui::ScrollItemWidget& tpl_item) {
          auto up_item = ora::ui::ScrollItemWidget::Setup(
              tpl_item,
              [this, str_id = option.Id, str_key = pair_value.first] {
                for (const auto& [str_k, str_v] : vec_mission_options_)
                  if (str_k == str_id)
                    return str_v == str_key;
                return false;
              },
              [this, str_id = option.Id, str_key = pair_value.first] {
                OnOptionSelected(str_id, str_key);
              });
          up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
              [str_value = pair_value.second] { return str_value; };
          return up_item;
        });
  };
}

void MissionBrowserLogic::PlayVideo(
    VideoPlayerWidget& player, const std::string& str_video,
    PlayingVideo kind_playing, std::function<void()> fn_on_complete) {
  if (!ptr_mod_data_->ModFiles().Exists(str_video)) {
    ButtonPrompt("dialog-no-video.title", "dialog-no-video.prompt", nullptr,
                 "", nullptr, "dialog-no-video.cancel");
    return;
  }

  StopVideo(player);

  kind_playing_video_ = kind_playing;
  player.LoadAndPlay(str_video);

  if (player.Video() == nullptr) {
    StopVideo(player);
    ButtonPrompt("dialog-cant-play-video.title",
                 "dialog-cant-play-video.prompt", nullptr, "", nullptr,
                 "dialog-cant-play-video.cancel");
    return;
  }

  player.PlayThen([this, player_ptr = &player,
                   fn_on_complete = std::move(fn_on_complete)]() mutable {
    StopVideo(*player_ptr);
    if (fn_on_complete)
      fn_on_complete();
  });

  if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps()) {
    fp4_cached_sound_volume_ = ptr_sound->SoundVolume();
    fp4_cached_music_volume_ = ptr_sound->MusicVolume();
    ptr_sound->SetSoundVolume(0);
    ptr_sound->SetMusicVolume(0);
  }
}

void MissionBrowserLogic::StopVideo(VideoPlayerWidget& player) {
  if (kind_playing_video_ == PlayingVideo::None)
    return;

  if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps()) {
    if (fp4_cached_sound_volume_ > 0)
      ptr_sound->SetSoundVolume(fp4_cached_sound_volume_);
    if (fp4_cached_music_volume_ > 0)
      ptr_sound->SetMusicVolume(fp4_cached_music_volume_);
  }
  player.CloseVideo();
  kind_playing_video_ = PlayingVideo::None;
}

void MissionBrowserLogic::StartMissionClicked() {
  StopVideo(*ptr_video_player_);

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  const std::string str_map =
      ptr_cache != nullptr && ptr_selected_map_ != nullptr
          ? ptr_cache->GetUpdatedMap(ptr_selected_map_->Uid())
          : std::string{};
  if (str_map.empty()) {
    ora::ui::Ui::Instance().DisconnectDeps();
    ora::ui::Ui::Instance().CloseWindow();
    fn_on_exit_();
    return;
  }

  ptr_selected_map_ = &ptr_cache->At(str_map);

  vec_started_orders_for_test_.clear();
  std::vector<std::string> vec_orders;
  for (const auto& [str_key, str_value] : vec_mission_options_) {
    const std::string str_order = "option " + str_key + " " + str_value;
    vec_orders.push_back(str_order);
    vec_started_orders_for_test_.push_back(str_order);
  }
  vec_orders.push_back("state 2");
  vec_started_orders_for_test_.push_back("state 2");

  str_started_uid_for_test_ = ptr_selected_map_->Uid();

  std::string str_start_video;
  if (ptr_selected_map_->WorldActorInfo() != nullptr)
    for (const meta::RecordObject* rec :
         ptr_selected_map_->WorldActorInfo()->TraitInfosByInterface(
             "MissionDataInfo")) {
      if (const auto v = sim::RecordFieldString(*rec, "StartVideo"))
        str_start_video = std::string{*v};
      break;
    }

  if (!str_start_video.empty() &&
      ptr_mod_data_->ModFiles().Exists(str_start_video)) {
    auto* ptr_fs_player =
        ptr_fullscreen_video_player_->Get<VideoPlayerWidget>("PLAYER");
    ptr_fullscreen_video_player_->b_visible = true;
    PlayVideo(*ptr_fs_player, str_start_video, PlayingVideo::GameStart,
              [this, str_uid = ptr_selected_map_->Uid(), vec_orders] {
                ora::ui::Ui::Instance().StartMissionServerDeps(str_uid,
                                                               vec_orders);
              });
  } else {
    ora::ui::Ui::Instance().StartMissionServerDeps(ptr_selected_map_->Uid(),
                                                   vec_orders);
  }
}

// ——— MapChooserLogic + MusicPlayerLogic ———


MapChooserLogic::MapChooserLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    const std::string* ptr_initial_map,
    const map::MapGenerationArgs* ptr_initial_generated_map,
    const std::vector<std::string>* ptr_remote_map_pool,
    map::MapClassification kind_initial_tab, std::function<void()> fn_on_exit,
    std::function<void(const std::string&)> fn_on_select,
    std::function<void(const map::MapGenerationArgs&)> fn_on_select_generated,
    map::MapVisibility kind_filter)
    : ptr_widget_{&widget},
      ptr_mod_data_{&mod_data},
      ptr_remote_map_pool_{ptr_remote_map_pool},
      kind_filter_{kind_filter},
      fn_on_select_{std::move(fn_on_select)} {
  (void)ptr_initial_generated_map;
  (void)fn_on_select_generated;

  str_all_maps_ = Msg("label-all-maps");

  const auto fn_canceling = [this, fn_on_exit = std::move(fn_on_exit)]() {
    ora::ui::Ui::Instance().CloseWindow();
    fn_on_exit();
  };

  auto* ptr_ok_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_OK");
  if (fn_on_select_)
    ptr_ok_button->fn_is_disabled = [] { return false; };
  else
    ptr_ok_button->b_disabled = true;

  ptr_ok_button->fn_on_click = [this] {
    ora::ui::Ui::Instance().CloseWindow();
    if (fn_on_select_)
      fn_on_select_(str_selected_uid_);
  };
  widget.Get<ora::ui::ButtonWidget>("BUTTON_CANCEL")->fn_on_click =
      fn_canceling;

  ptr_game_mode_dropdown_ =
      widget.GetOrNull<ora::ui::DropDownButtonWidget>("GAMEMODE_FILTER");

  up_item_template_ = widget.RemoveChild(
      *widget.Get<ora::ui::ScrollItemWidget>("MAP_TEMPLATE"));

  SetupOrderByDropdown();

  if (auto* ptr_filter_container = widget.GetOrNull("FILTER_ORDER_CONTROLS"))
    ptr_filter_container->fn_is_visible = [] { return true; };

  if (auto* ptr_map_filter_input =
          widget.GetOrNull<ora::ui::TextFieldWidget>("MAPFILTER_INPUT")) {
    ptr_map_filter_input->TakeKeyboardFocus();
    ptr_map_filter_input->fn_on_esc_key =
        [this, ptr_map_filter_input, fn_canceling](const KeyInput&) {
          if (ptr_map_filter_input->Text().empty()) {
            fn_canceling();
          } else {
            ptr_map_filter_input->SetText(std::string{});
            opt_map_filter_.reset();
            EnumerateMaps(kind_current_tab_);
          }
          return true;
        };
    ptr_map_filter_input->fn_on_enter_key =
        [ptr_ok_button](const KeyInput&) {
          if (ptr_ok_button->fn_on_click)
            ptr_ok_button->fn_on_click();
          return true;
        };
    ptr_map_filter_input->fn_on_text_edited = [this, ptr_map_filter_input] {
      opt_map_filter_ = ptr_map_filter_input->Text();
      EnumerateMaps(kind_current_tab_);
    };
  }

  if (auto* ptr_random_map_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("RANDOMMAP_BUTTON")) {
    ptr_random_map_button->fn_on_click = [this] {
      const std::string str_uid = RandomOf(vec_visible_maps_,
                                            CosmeticRandomFace());
      str_selected_uid_ = str_uid;
      const auto it_panel = map_scrollpanels_.find(kind_current_tab_);
      if (it_panel != map_scrollpanels_.end())
        it_panel->second->ScrollToItem(str_uid, true);
    };
    ptr_random_map_button->fn_is_disabled = [this] {
      return vec_visible_maps_.empty();
    };
    ptr_random_map_button->fn_is_visible = [] { return true; };
  }

  auto* ptr_delete_map_button =
      widget.Get<ora::ui::ButtonWidget>("DELETE_MAP_BUTTON");
  ptr_delete_map_button->fn_is_disabled = [this] {
    return kind_current_tab_ != map::MapClassification::User;
  };
  ptr_delete_map_button->fn_is_visible = [this] {
    return kind_current_tab_ == map::MapClassification::User;
  };
  ptr_delete_map_button->fn_on_click = [this] {
    DeleteOneMap(str_selected_uid_,
                 [this](const std::string& str_new_uid) {
                   RefreshMaps(kind_current_tab_);
                   EnumerateMaps(kind_current_tab_);
                   SetupMapTabs();
                   if (map_tab_maps_[kind_current_tab_].empty())
                    SwitchTabOfMap(str_new_uid);
                 });
  };

  auto* ptr_delete_all_maps_button =
      widget.Get<ora::ui::ButtonWidget>("DELETE_ALL_MAPS_BUTTON");
  ptr_delete_all_maps_button->fn_is_visible = [this] {
    return kind_current_tab_ == map::MapClassification::User;
  };
  ptr_delete_all_maps_button->fn_on_click = [this] {
    DeleteAllMaps(vec_visible_maps_,
                  [this](const std::string& str_new_uid) {
                    RefreshMaps(kind_current_tab_);
                    EnumerateMaps(kind_current_tab_);
                    SetupMapTabs();
                    SwitchTabOfMap(str_new_uid);
                  });
  };

  auto* ptr_remote_map_label = widget.Get<ora::ui::LabelWidget>(
      "REMOTE_MAP_LABEL");
  ptr_remote_map_label->fn_is_visible = [this] {
    return ptr_remote_map_pool_ != nullptr &&
           (int4_remote_searching_ > 0 || int4_remote_unavailable_ > 0);
  };
  ptr_remote_map_label->fn_get_text = [this] {
    if (int4_remote_searching_ > 0)
      return Msg1("label-map-searching-count", "count",
                  fluent::FluentValue{
                      static_cast<std::int64_t>(int4_remote_searching_)});
    return Msg1("label-map-unavailable-count", "count",
                fluent::FluentValue{
                    static_cast<std::int64_t>(int4_remote_unavailable_)});
  };

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache != nullptr && ptr_remote_map_pool_ != nullptr &&
      ptr_cache->AllowDownloading()) {
    ptr_cache->QueryRemoteMapDetails(
        game::WebServices{}.MapRepository, *ptr_remote_map_pool_);
  }

  SetupMapPanel(map::MapClassification::User, "USER_MAPS_TAB");
  SetupMapPanel(map::MapClassification::System, "SYSTEM_MAPS_TAB");
  SetupMapPanel(map::MapClassification::Remote, "REMOTE_MAPS_TAB");

  if (ptr_remote_map_pool_ != nullptr) {
    map_tab_labels_[map::MapClassification::Remote] =
        "button-mapchooser-remote-maps-tab";
    kind_current_tab_ = map::MapClassification::Remote;
    str_selected_uid_ = ptr_initial_map != nullptr ? *ptr_initial_map
                                                   : std::string{};
  } else {
    map_tab_labels_[map::MapClassification::System] =
        "button-mapchooser-system-maps-tab";
    map_tab_labels_[map::MapClassification::User] =
        "button-mapchooser-user-maps-tab";

    const auto it_initial = map_tab_maps_.find(kind_initial_tab);
    if (ptr_initial_map != nullptr && ptr_cache != nullptr &&
        ptr_cache->At(*ptr_initial_map).Class() ==
            map::MapClassification::Generated) {
      kind_current_tab_ = map::MapClassification::Generated;
      str_selected_uid_ =
          ptr_cache != nullptr
              ? ptr_cache->ChooseInitialMap(std::string{}, CosmeticRandomFace())
              : std::string{};
    } else if (ptr_initial_map == nullptr && it_initial != map_tab_maps_.end() &&
               !it_initial->second.empty()) {
      const std::string str_uid = it_initial->second.front()->Uid();
      str_selected_uid_ =
          ptr_cache != nullptr
              ? ptr_cache->ChooseInitialMap(str_uid, CosmeticRandomFace())
              : str_uid;
      kind_current_tab_ = kind_initial_tab;
    } else {
      str_selected_uid_ =
          ptr_cache != nullptr && ptr_initial_map != nullptr
              ? ptr_cache->ChooseInitialMap(*ptr_initial_map,
                                            CosmeticRandomFace())
              : std::string{};
      for (const auto& [kind_tab, vec_maps] : map_tab_maps_)
        for (const map::MapPreview* ptr_map : vec_maps)
          if (ptr_map->Uid() == str_selected_uid_) {
            kind_current_tab_ = kind_tab;
            break;
          }
    }
  }

  EnumerateMaps(kind_current_tab_);
  SetupMapTabs();
}

MapChooserLogic::~MapChooserLogic() { b_disposed_ = true; }

void MapChooserLogic::SwitchTabOfMap(const std::string& str_uid) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache != nullptr)
    SwitchTab(ptr_cache->At(str_uid).Class());
}

void MapChooserLogic::SwitchTab(map::MapClassification kind_tab) {
  kind_current_tab_ = kind_tab;
  EnumerateMaps(kind_tab);
}

void MapChooserLogic::RefreshMaps(map::MapClassification kind_tab) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (kind_tab == map::MapClassification::System ||
      kind_tab == map::MapClassification::User) {
    std::vector<map::MapPreview*> vec_maps;
    if (ptr_cache != nullptr)
      for (map::MapPreview* ptr_map : ptr_cache->Previews())
        if (ptr_map->Status() == map::MapStatus::Available &&
            ptr_map->Class() == kind_tab &&
            (static_cast<std::int32_t>(ptr_map->Visibility()) &
             static_cast<std::int32_t>(kind_filter_)) != 0)
          vec_maps.push_back(ptr_map);
    map_tab_maps_[kind_tab] = std::move(vec_maps);
  } else if (ptr_remote_map_pool_ != nullptr) {
    std::vector<map::MapPreview*> vec_loaded;
    int4_remote_searching_ = 0;
    int4_remote_unavailable_ = 0;
    for (const std::string& str_uid : *ptr_remote_map_pool_) {
      map::MapPreview& preview = ptr_cache->At(str_uid);
      const map::MapStatus kind_status = preview.Status();
      if (kind_status == map::MapStatus::Searching)
        int4_remote_searching_++;
      else if (kind_status == map::MapStatus::Unavailable)
        int4_remote_unavailable_++;
      else
        vec_loaded.push_back(&preview);
    }
    map_tab_maps_[kind_tab] = std::move(vec_loaded);

    if (int4_remote_searching_ > 0) {
      ora::ui::Ui::Instance().RunAfterDelayDefer(
          1000, [this] {
            if (b_disposed_)
              return;
            const int int4_missing_before =
                int4_remote_searching_ + int4_remote_unavailable_;
            RefreshMaps(map::MapClassification::Remote);
            const int int4_missing_after =
                int4_remote_searching_ + int4_remote_unavailable_;
            if (kind_current_tab_ == map::MapClassification::Remote &&
                int4_missing_before != int4_missing_after)
              EnumerateMaps(map::MapClassification::Remote);
          });
    }
  } else {
    map_tab_maps_[kind_tab] = {};
  }
}

void MapChooserLogic::SetupMapTabs() {
  for (int int4_index = 1; int4_index <= 3; int4_index++)
    ptr_widget_->Get<ora::ui::ButtonWidget>("BUTTON" +
                                            std::to_string(int4_index))
        ->b_visible = false;

  int int4_tab_count = 0;
  for (const auto& [kind_tab, str_label] : map_tab_labels_) {
    if (kind_tab == map::MapClassification::User &&
        map_tab_maps_[kind_tab].empty())
      continue;

    ++int4_tab_count;
    auto* ptr_tab_button = ptr_widget_->Get<ora::ui::ButtonWidget>(
        "BUTTON" + std::to_string(int4_tab_count));
    ptr_tab_button->fn_is_highlighted = [this, kind_tab] {
      return kind_current_tab_ == kind_tab;
    };
    ptr_tab_button->fn_on_click = [this, kind_tab] { SwitchTab(kind_tab); };
    ptr_tab_button->b_visible = true;
    ptr_tab_button->str_text = str_label;
  }
}

void MapChooserLogic::SetupMapPanel(map::MapClassification kind_tab,
                                    const std::string& str_tab_container_name) {
  auto* ptr_tab_container =
      ptr_widget_->Get<ora::ui::ContainerWidget>(str_tab_container_name);
  ptr_tab_container->fn_is_visible = [this, kind_tab] {
    return kind_current_tab_ == kind_tab;
  };
  auto* ptr_tab_scrollpanel =
      ptr_tab_container->Get<ora::ui::ScrollPanelWidget>("MAP_LIST");
  ptr_tab_scrollpanel->ptr_layout =
      std::make_unique<ora::ui::GridLayout>(*ptr_tab_scrollpanel);
  map_scrollpanels_.emplace(kind_tab, ptr_tab_scrollpanel);

  RefreshMaps(kind_tab);
}


void MapChooserLogic::SetupGameModeDropdown(
    map::MapClassification kind_tab,
    ora::ui::DropDownButtonWidget* ptr_dropdown) {
  if (ptr_dropdown == nullptr)
    return;

  std::vector<std::pair<std::string, int>> vec_categories;
  for (const map::MapPreview* ptr_map : map_tab_maps_[kind_tab])
    for (const std::string& str_category : ptr_map->Categories()) {
      bool b_found = false;
      for (auto& [str_key, int4_count] : vec_categories)
        if (str_key == str_category) {
          ++int4_count;
          b_found = true;
          break;
        }
      if (!b_found)
        vec_categories.emplace_back(str_category, 1);
    }
  std::sort(vec_categories.begin(), vec_categories.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  vec_categories.insert(
      vec_categories.begin(),
      std::pair<std::string, int>{std::string{},
                                  static_cast<int>(
                                      map_tab_maps_[kind_tab].size())});

  ptr_dropdown->fn_on_click =
      [this, ptr_dropdown, kind_tab, vec_categories]() mutable {
        ptr_dropdown->ShowDropDown<std::pair<std::string, int>>(
            "LABEL_DROPDOWN_TEMPLATE", 210, vec_categories,
            [this, kind_tab,
             vec_categories](const std::pair<std::string, int>& pair_item,
                             ora::ui::ScrollItemWidget& tpl_item) {
              auto up_item = ora::ui::ScrollItemWidget::Setup(
                  tpl_item,
                  [this, str_cat = pair_item.first] {
                    return opt_category_.has_value() &&
                           *opt_category_ == str_cat;
                  },
                  [this, kind_tab, str_cat = pair_item.first] {
                    if (str_cat.empty())
                      opt_category_.reset();
                    else
                      opt_category_ = str_cat;
                    EnumerateMaps(kind_tab);
                  });
              up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                  [this, pair_item] {
                    return (pair_item.first.empty() ? str_all_maps_
                                                    : pair_item.first) +
                           " (" + std::to_string(pair_item.second) + ")";
                  };
              return up_item;
            });
      };

  ptr_dropdown->fn_get_text = [this, vec_categories] {
    for (const auto& [str_cat, int4_count] : vec_categories)
      if (opt_category_.has_value() && *opt_category_ == str_cat)
        return (str_cat.empty() ? str_all_maps_ : str_cat) + " (" +
               std::to_string(int4_count) + ")";
    return std::string{Msg("label-no-matches")};
  };
}

void MapChooserLogic::SetupOrderByDropdown() {
  auto* ptr_order_by_dropdown =
      ptr_widget_->GetOrNull<ora::ui::DropDownButtonWidget>("ORDERBY");
  if (ptr_order_by_dropdown == nullptr)
    return;

  vec_order_by_dict_.emplace_back(
      Msg("options-order-maps.player-count"),
      [](const map::MapPreview& m) { return static_cast<long>(m.PlayerCount()); });
  vec_order_by_dict_.emplace_back(Msg("options-order-maps.title"),
                                   std::function<long(const map::MapPreview&)>{});
  vec_order_by_dict_.emplace_back(
      Msg("options-order-maps.date"), [](const map::MapPreview& m) {
        return -static_cast<long>(m.ModifiedDate());
      });
  vec_order_by_dict_.emplace_back(
      Msg("options-order-maps.size"), [](const map::MapPreview& m) {
        return static_cast<long>(m.Bounds().Width) *
               static_cast<long>(m.Bounds().Height);
      });

  fn_order_by_ = vec_order_by_dict_.front().second;

  ptr_order_by_dropdown->fn_on_click =
      [this, ptr_order_by_dropdown]() mutable {
        ptr_order_by_dropdown->ShowDropDown<std::pair<
            std::string, std::function<long(const map::MapPreview&)>>>(
            "LABEL_DROPDOWN_TEMPLATE", 500, vec_order_by_dict_,
            [this](const std::pair<std::string,
                                   std::function<long(const map::MapPreview&)>>&
                       pair_entry,
                   ora::ui::ScrollItemWidget& tpl_item) {
              auto up_item = ora::ui::ScrollItemWidget::Setup(
                  tpl_item,
                  [this, pair_entry] {
                    return StaticOrderByEquals(fn_order_by_, pair_entry.second);
                  },
                  [this, pair_entry] {
                    fn_order_by_ = pair_entry.second;
                    EnumerateMaps(kind_current_tab_);
                  });
              up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                  [str_key = pair_entry.first] { return str_key; };
              return up_item;
            });
      };

  ptr_order_by_dropdown->fn_get_text = [this] {
    for (const auto& [str_key, fn_order] : vec_order_by_dict_)
      if (StaticOrderByEquals(fn_order_by_, fn_order))
        return str_key;
    return std::string{};
  };
}


void MapChooserLogic::EnumerateMaps(map::MapClassification kind_tab) {
  if (kind_tab == map::MapClassification::Generated)
    return;

  int int4_player_count_filter = -1;
  if (opt_map_filter_.has_value() && !opt_map_filter_->empty()) {
    int int4_parsed = 0;
    const auto [ptr_end, ec] = std::from_chars(
        opt_map_filter_->data(), opt_map_filter_->data() + opt_map_filter_->size(),
        int4_parsed);
    if (ec == std::errc{} && ptr_end == opt_map_filter_->data() + opt_map_filter_->size())
      int4_player_count_filter = int4_parsed;
  }

  std::vector<map::MapPreview*> vec_maps;
  for (map::MapPreview* ptr_map : map_tab_maps_[kind_tab]) {
    if (opt_category_.has_value() && !opt_category_->empty()) {
      bool b_in_category = false;
      for (const std::string& str_category : ptr_map->Categories())
        if (str_category == *opt_category_) {
          b_in_category = true;
          break;
        }
      if (!b_in_category)
        continue;
    }

    if (opt_map_filter_.has_value() && !opt_map_filter_->empty()) {
      bool b_matches = EqualsIgnoreCase(ptr_map->Title(), *opt_map_filter_) ||
                       EqualsIgnoreCase(ptr_map->Author(), *opt_map_filter_) ||
                       ptr_map->PlayerCount() == int4_player_count_filter;
      bool b_contains = ContainsIgnoreCase(ptr_map->Title(), *opt_map_filter_) ||
                        ContainsIgnoreCase(ptr_map->Author(), *opt_map_filter_);
      if (!b_matches && !b_contains)
        continue;
    }

    vec_maps.push_back(ptr_map);
  }

  if (!fn_order_by_) {
    std::stable_sort(vec_maps.begin(), vec_maps.end(),
                     [](const map::MapPreview* a, const map::MapPreview* b) {
                       return LessIgnoreCase(a->Title(), b->Title());
                     });
  } else {
    const auto fn_order = fn_order_by_;
    std::stable_sort(
        vec_maps.begin(), vec_maps.end(),
        [fn_order](const map::MapPreview* a, const map::MapPreview* b) {
          const long l_a = fn_order(*a);
          const long l_b = fn_order(*b);
          if (l_a != l_b)
            return l_a < l_b;
          return LessIgnoreCase(a->Title(), b->Title());
        });
  }

  map_scrollpanels_[kind_tab]->RemoveChildren();
  for (map::MapPreview* ptr_preview : vec_maps) {
    ptr_preview->GetMinimap();

    auto up_item = ora::ui::ScrollItemWidget::Setup(
        ptr_preview->Uid(),
        *static_cast<ora::ui::ScrollItemWidget*>(up_item_template_.get()),
        [this, ptr_preview] { return str_selected_uid_ == ptr_preview->Uid(); },
        [this, ptr_preview] { str_selected_uid_ = ptr_preview->Uid(); },
        [this, ptr_preview] {
          if (fn_on_select_) {
            ora::ui::Ui::Instance().CloseWindow();
            fn_on_select_(ptr_preview->Uid());
          }
        });
    up_item->fn_is_visible = [ptr_item = up_item.get(), this, kind_tab] {
      return ptr_item->RenderBounds().IntersectsWith(
          map_scrollpanels_[kind_tab]->RenderBounds());
    };

    if (auto* ptr_title_label =
            up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("TITLE"))
      ora::ui::TruncateLabelToTooltip(*ptr_title_label,
                                      ptr_preview->Title());

    if (auto* ptr_preview_widget =
            up_item->GetOrNull<ora::ui::MapPreviewWidget>("PREVIEW"))
      ptr_preview_widget->fn_preview =
          [ptr_preview] { return const_cast<map::MapPreview*>(ptr_preview); };

    if (auto* ptr_details_label =
            up_item->GetOrNull<ora::ui::LabelWidget>("DETAILS")) {
      std::string str_details;
      if (!ptr_preview->Categories().empty())
        str_details = ptr_preview->Categories().front() + " ";
      str_details += Msg1("label-player-count", "players",
                          fluent::FluentValue{
                              static_cast<std::int64_t>(
                                  ptr_preview->PlayerCount())});
      ptr_details_label->fn_get_text = [str_details] { return str_details; };
    }

    if (auto* ptr_author_label =
            up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("AUTHOR")) {
      if (!ptr_preview->Author().empty())
        ora::ui::TruncateLabelToTooltip(
            *ptr_author_label,
            Msg1("label-created-by", "author",
                 fluent::FluentValue{ptr_preview->Author()}));
    }

    if (auto* ptr_size_label = up_item->GetOrNull<ora::ui::LabelWidget>(
            "SIZE")) {
      const std::string str_size = MapChooserMapSizeLabel(
          Size{ptr_preview->Bounds().Width,
               ptr_preview->Bounds().Height});
      ptr_size_label->fn_get_text = [str_size] { return str_size; };
    }

    map_scrollpanels_[kind_tab]->AddChild(std::move(up_item));
  }

  if (kind_tab == kind_current_tab_) {
    vec_visible_maps_.clear();
    for (const map::MapPreview* ptr_map : vec_maps)
      vec_visible_maps_.push_back(ptr_map->Uid());
    SetupGameModeDropdown(kind_current_tab_, ptr_game_mode_dropdown_);
  }

  for (const std::string& str_uid : vec_visible_maps_)
    if (str_uid == str_selected_uid_) {
      map_scrollpanels_[kind_tab]->ScrollToItem(str_selected_uid_);
      break;
    }
}

std::string MapChooserLogic::DeleteMap(const std::string& str_map) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache != nullptr) {
    ptr_cache->At(str_map).Delete();
    if (str_selected_uid_ == str_map) {
      std::string str_first;
      if (!map_tab_maps_[kind_current_tab_].empty())
        str_first = map_tab_maps_[kind_current_tab_].front()->Uid();
      str_selected_uid_ =
          ptr_cache->ChooseInitialMap(str_first, CosmeticRandomFace());
    }
  }

  return str_selected_uid_;
}

void MapChooserLogic::DeleteOneMap(
    const std::string& str_map,
    std::function<void(const std::string&)> fn_after) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  const std::string str_title =
      ptr_cache != nullptr ? ptr_cache->At(str_map).Title() : std::string{};
  ButtonPrompt(
      "dialog-delete-map.title", "dialog-delete-map.prompt",
      [this, str_map, fn_after = std::move(fn_after)]() {
        const std::string str_new_uid = DeleteMap(str_map);
        if (fn_after)
          fn_after(str_new_uid);
      },
      "dialog-delete-map.confirm", nullptr, {}, nullptr, "",
      MakeArgs({{"title", str_title}}));
}

void MapChooserLogic::DeleteAllMaps(
    const std::vector<std::string>& vec_maps,
    std::function<void(const std::string&)> fn_after) {
  ButtonPrompt(
      "dialog-delete-all-maps.title", "dialog-delete-all-maps.prompt",
      [this, vec_maps, fn_after = std::move(fn_after)]() {
        for (const std::string& str_map : vec_maps)
          DeleteMap(str_map);
        map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
        if (fn_after)
          fn_after(ptr_cache != nullptr
                       ? ptr_cache->ChooseInitialMap(std::string{},
                                                     CosmeticRandomFace())
                       : std::string{});
      },
      "dialog-delete-all-maps.confirm");
}


// ——— MusicPlayerLogic ———

MusicPlayerLogic::MusicPlayerLogic(ora::ui::Widget& widget,
                                   sim::World& world,
                                   game::ModData& /*mod_data*/,
                                   std::function<void()> fn_on_exit) {
  ptr_music_list_ = widget.Get<ora::ui::ScrollPanelWidget>("MUSIC_LIST");
  up_item_template_ = ptr_music_list_->RemoveChild(
      *ptr_music_list_->Get<ora::ui::ScrollItemWidget>("MUSIC_TEMPLATE"));
  ptr_music_playlist_ = world.WorldActor()->Trait<MusicPlaylist>();

  BuildMusicTable();

  const auto fn_no_music = [this] {
    return ptr_music_playlist_ == nullptr ||
           !ptr_music_playlist_->IsMusicAvailable() ||
           ptr_music_playlist_->CurrentSongIsBackground() ||
           ptr_current_song_ == nullptr;
  };
  widget.Get("NO_MUSIC_LABEL")->fn_is_visible = [this] {
    return ptr_music_playlist_ != nullptr &&
           !ptr_music_playlist_->IsMusicAvailable();
  };

  if (ptr_music_playlist_ != nullptr &&
      ptr_music_playlist_->IsMusicAvailable()) {
    widget.Get<ora::ui::LabelWidget>("MUTE_LABEL")->fn_get_text = [] {
      if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps();
          ptr_sound != nullptr && ptr_sound->SettingsSound().b_mute)
        return Msg("label-sound-muted");
      return std::string{};
    };
  }

  auto* ptr_play_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_PLAY");
  ptr_play_button->fn_on_click = [this] { Play(); };
  ptr_play_button->fn_is_disabled = fn_no_music;
  ptr_play_button->fn_is_visible = [this] {
    sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps();
    return ptr_sound == nullptr || !ptr_sound->MusicPlaying();
  };

  auto* ptr_pause_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_PAUSE");
  ptr_pause_button->fn_on_click = [] {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      ptr_sound->PauseMusic();
  };
  ptr_pause_button->fn_is_disabled = fn_no_music;
  ptr_pause_button->fn_is_visible = [this] {
    sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps();
    return ptr_sound != nullptr && ptr_sound->MusicPlaying();
  };

  auto* ptr_stop_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_STOP");
  ptr_stop_button->fn_on_click = [this] {
    if (ptr_music_playlist_ != nullptr)
      ptr_music_playlist_->Stop();
  };
  ptr_stop_button->fn_is_disabled = fn_no_music;

  auto* ptr_next_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_NEXT");
  ptr_next_button->fn_on_click = [this] {
    if (ptr_music_playlist_ != nullptr)
      ptr_current_song_ = ptr_music_playlist_->GetNextSong();
    Play();
  };
  ptr_next_button->fn_is_disabled = fn_no_music;

  auto* ptr_prev_button = widget.Get<ora::ui::ButtonWidget>("BUTTON_PREV");
  ptr_prev_button->fn_on_click = [this] {
    if (ptr_music_playlist_ != nullptr)
      ptr_current_song_ = ptr_music_playlist_->GetPrevSong();
    Play();
  };
  ptr_prev_button->fn_is_disabled = fn_no_music;

  auto* ptr_shuffle_checkbox =
      widget.Get<ora::ui::CheckboxWidget>("SHUFFLE");
  ptr_shuffle_checkbox->fn_is_checked = [] {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      return ptr_sound->SettingsSound().b_shuffle;
    return false;
  };
  ptr_shuffle_checkbox->fn_on_click = [] {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      ptr_sound->MutableSettingsSound().b_shuffle =
          !ptr_sound->SettingsSound().b_shuffle;
  };
  ptr_shuffle_checkbox->fn_is_disabled = [this] {
    return ptr_music_playlist_ != nullptr &&
           ptr_music_playlist_->CurrentSongIsBackground();
  };

  auto* ptr_repeat_checkbox = widget.Get<ora::ui::CheckboxWidget>("REPEAT");
  ptr_repeat_checkbox->fn_is_checked = [] {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      return ptr_sound->SettingsSound().b_repeat;
    return false;
  };
  ptr_repeat_checkbox->fn_on_click = [] {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      ptr_sound->SetMusicLooped(!ptr_sound->SettingsSound().b_repeat);
  };
  ptr_repeat_checkbox->fn_is_disabled = [this] {
    return ptr_music_playlist_ != nullptr &&
           ptr_music_playlist_->CurrentSongIsBackground();
  };

  widget.Get<ora::ui::LabelWidget>("TIME_LABEL")->fn_get_text = [this] {
    if (ptr_current_song_ == nullptr ||
        (ptr_music_playlist_ != nullptr &&
         ptr_music_playlist_->CurrentSongIsBackground()))
      return std::string{};

    sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps();
    const int int4_seek =
        ptr_sound != nullptr
            ? static_cast<int>(ptr_sound->MusicSeekPosition())
            : 0;
    const int int4_minutes = int4_seek / 60;
    const int int4_seconds = int4_seek % 60;
    const int int4_total_minutes = ptr_current_song_->Length() / 60;
    const int int4_total_seconds = ptr_current_song_->Length() % 60;

    return std::format("{:02}:{:02} / {:02}:{:02}", int4_minutes,
                       int4_seconds, int4_total_minutes, int4_total_seconds);
  };

  const std::string str_no_song_playing = Msg("label-no-song-playing");
  if (auto* ptr_music_title =
          widget.GetOrNull<ora::ui::LabelWidget>("TITLE_LABEL"))
    ptr_music_title->fn_get_text = [this, str_no_song_playing] {
      return ptr_current_song_ != nullptr ? ptr_current_song_->Title()
                                          : str_no_song_playing;
    };

  auto* ptr_music_slider = widget.Get<ora::ui::SliderWidget>("MUSIC_SLIDER");
  ptr_music_slider->fn_on_change = [](float fp4_value) {
    if (sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps())
      ptr_sound->SetMusicVolume(fp4_value);
  };
  {
    sound::Sound* ptr_sound = ora::ui::Ui::Instance().SoundDeps();
    ptr_music_slider->fp4_value =
        ptr_sound != nullptr ? ptr_sound->MusicVolume() : 0.5f;
  }

  if (auto* ptr_song_watcher =
          widget.GetOrNull<ora::ui::LogicTickerWidget>("SONG_WATCHER"))
    ptr_song_watcher->fn_on_tick = [this] {
      if (ptr_music_playlist_ != nullptr &&
          ptr_music_playlist_->CurrentSongIsBackground() &&
          ptr_current_song_ != nullptr)
        ptr_current_song_ = nullptr;

      const game::MusicInfo* ptr_current =
          ora::ui::Ui::Instance().SoundDeps() != nullptr
              ? ora::ui::Ui::Instance().SoundDeps()->CurrentMusic()
              : nullptr;
      if (ptr_current == nullptr || ptr_current_song_ == ptr_current ||
          (ptr_music_playlist_ != nullptr &&
           ptr_music_playlist_->CurrentSongIsBackground()))
        return;

      ptr_current_song_ = ptr_current;
    };

  if (auto* ptr_back_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BACK_BUTTON"))
    ptr_back_button->fn_on_click = [fn_on_exit = std::move(fn_on_exit)]() {
      ora::ui::Ui::Instance().SettingsSaveDeps();
      ora::ui::Ui::Instance().CloseWindow();
      fn_on_exit();
    };
}

void MusicPlayerLogic::BuildMusicTable() {
  if (ptr_music_playlist_ == nullptr ||
      !ptr_music_playlist_->IsMusicAvailable())
    return;

  const std::vector<game::MusicInfo*>& vec_music =
      ptr_music_playlist_->AvailablePlaylist();
  ptr_current_song_ = ptr_music_playlist_->CurrentSong();

  ptr_music_list_->RemoveChildren();
  vec_song_titles_for_test_.clear();
  for (const game::MusicInfo* ptr_song : vec_music) {
    auto up_item = ora::ui::ScrollItemWidget::Setup(
        ptr_song->Filename(),
        *static_cast<ora::ui::ScrollItemWidget*>(up_item_template_.get()),
        [this, ptr_song] { return ptr_current_song_ == ptr_song; },
        [this, ptr_song] {
          ptr_current_song_ = ptr_song;
          Play();
        },
        [] {});
    vec_song_titles_for_test_.push_back(ptr_song->Title());
    ora::ui::TruncateLabelToTooltip(
        *up_item->Get<ora::ui::LabelWithTooltipWidget>("TITLE"),
        ptr_song->Title());

    up_item->Get<ora::ui::LabelWidget>("LENGTH")->fn_get_text =
        [ptr_song] { return SongLengthLabel(*ptr_song); };
    ptr_music_list_->AddChild(std::move(up_item));
  }

  if (str_current_song_title_for_test_.empty() && ptr_current_song_ != nullptr)
    str_current_song_title_for_test_ = ptr_current_song_->Title();
}

void MusicPlayerLogic::Play() {
  if (ptr_current_song_ == nullptr)
    return;

  ptr_music_list_->ScrollToItem(ptr_current_song_->Filename());
  if (ptr_music_playlist_ != nullptr)
    ptr_music_playlist_->Play(ptr_current_song_);
  str_current_song_title_for_test_ = ptr_current_song_->Title();
}

std::string MusicPlayerLogic::SongLengthLabel(const game::MusicInfo& song) {
  return std::format("{}:{:02}", song.Length() / 60, song.Length() % 60);
}

void RegisterBrowserLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();
  registry_logics.Register(
      "ReplayBrowserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<ReplayBrowserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::get<std::function<void()>>(args_args.At("onStart")));
      });

  registry_logics.Register(
      "GameSaveBrowserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<GameSaveBrowserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")),
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::get<std::function<void()>>(args_args.At("onStart")));
      });

  registry_logics.Register(
      "MissionBrowserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<MissionBrowserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")),
            std::get<std::function<void()>>(args_args.At("onStart")),
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::get<std::string>(args_args.At("initialMap")));
      });

  registry_logics.Register(
      "MapChooserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        static const std::string str_empty_initial{};
        const std::string* ptr_initial_map = &str_empty_initial;
        if (const auto* ptr_value =
                std::get_if<std::string>(&args_args.At("initialMap")))
          ptr_initial_map = ptr_value;

        const std::vector<std::string>* ptr_remote_pool = nullptr;
        if (const auto* ptr_value =
                std::get_if<const std::vector<std::string>*>(
                    &args_args.At("remoteMapPool")))
          ptr_remote_pool = *ptr_value;

        std::function<void(const std::string&)> fn_on_select;
        if (const auto* ptr_value =
                std::get_if<std::function<void(const std::string&)>>(
                    &args_args.At("onSelect")))
          fn_on_select = *ptr_value;

        map::MapClassification kind_initial_tab =
            map::MapClassification::System;
        if (const auto* ptr_value =
                std::get_if<std::int32_t>(&args_args.At("initialTab")))
          kind_initial_tab =
              static_cast<map::MapClassification>(*ptr_value);
        map::MapVisibility kind_filter = map::MapVisibility::Lobby;
        if (const auto* ptr_value =
                std::get_if<std::int32_t>(&args_args.At("filter")))
          kind_filter = static_cast<map::MapVisibility>(*ptr_value);

        const map::MapGenerationArgs* ptr_initial_generated = nullptr;
        if (const auto* ptr_value =
                std::get_if<const map::MapGenerationArgs*>(
                    &args_args.At("initialGeneratedMap")))
          ptr_initial_generated = *ptr_value;

        std::function<void(const map::MapGenerationArgs&)>
            fn_on_select_generated;
        if (const auto* ptr_value =
                std::get_if<
                    std::function<void(const map::MapGenerationArgs&)>>(
                    &args_args.At("onSelectGenerated")))
          fn_on_select_generated = *ptr_value;

        return std::make_unique<MapChooserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            ptr_initial_map, ptr_initial_generated, ptr_remote_pool,
            kind_initial_tab,
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::move(fn_on_select), std::move(fn_on_select_generated),
            kind_filter);
      });

  registry_logics.Register(
      "MusicPlayerLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<MusicPlayerLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::get<std::function<void()>>(args_args.At("onExit")));
      });
}

}  // namespace ora::mods
