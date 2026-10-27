#include "mods/lobby_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "game/mod_data.hpp"
#include "game/web_services.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map_cache.hpp"
#include "mods/browser_logics.hpp"
#include "mods/connection_logics.hpp"
#include "mods/server_list_logic.hpp"
#include "net/game_save.hpp"
#include "net/network_connection.hpp"
#include "net/order.hpp"
#include "net/order_manager.hpp"
#include "net/session.hpp"
#include "net/unit_orders.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/client_tooltip_region_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/progress_bar_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 std::string str_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name},
           fluent::FluentValue{std::move(str_value)}}});
}

std::string MsgInt(std::string_view str_key, std::string_view str_arg_name,
                   std::int64_t int8_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name}, fluent::FluentValue{int8_value}}});
}

bool IsNullOrWhiteSpace(std::string_view sv_text) {
  for (char c_char : sv_text)
    if (c_char != ' ' && c_char != '\t' && c_char != '\r' && c_char != '\n')
      return false;
  return true;
}

char FoldAscii(char c_char) {
  if (c_char >= 'A' && c_char <= 'Z')
    return static_cast<char>(c_char - 'A' + 'a');
  return c_char;
}

bool StartsWithIgnoreCase(std::string_view sv_text,
                          std::string_view sv_prefix) {
  if (sv_text.size() < sv_prefix.size())
    return false;
  for (std::size_t sz_index = 0; sz_index < sv_prefix.size(); ++sz_index)
    if (FoldAscii(sv_text[sz_index]) != FoldAscii(sv_prefix[sz_index]))
      return false;
  return true;
}

std::string SpawnLetter(int int4_spawn) {
  return int4_spawn <= 0
             ? std::string{}
             : std::string(1, static_cast<char>('A' - 1 + int4_spawn));
}

}

std::string CompleteTabCompletion(TabCompletionState& state_tab,
                                  const std::string& str_text) {
  if (IsNullOrWhiteSpace(str_text))
    return str_text;

  if (state_tab.str_last_completed == str_text &&
      !state_tab.vec_candidates.empty()) {
    state_tab.int4_current_candidate_index =
        (state_tab.int4_current_candidate_index + 1) %
        static_cast<int>(state_tab.vec_candidates.size());
    state_tab.str_last_completed =
        state_tab.str_prefix +
        state_tab.vec_candidates[state_tab.int4_current_candidate_index] +
        state_tab.str_suffix;
    return state_tab.str_last_completed;
  }

  std::string str_to_complete;
  if (!str_text.empty() && str_text[0] == '/' &&
      state_tab.opt_commands.has_value()) {
    state_tab.str_prefix = "/";
    state_tab.str_suffix = "";
    str_to_complete = str_text.substr(1);
    state_tab.vec_candidates.clear();
    for (const auto& str_command : *state_tab.opt_commands)
      if (StartsWithIgnoreCase(str_command, str_to_complete))
        state_tab.vec_candidates.push_back(str_command);
  } else if (state_tab.opt_names.has_value()) {
    const std::size_t sz_space = str_text.rfind(' ');
    if (sz_space != std::string::npos) {
      state_tab.str_prefix = str_text.substr(0, sz_space + 1);
      state_tab.str_suffix = "";
      str_to_complete = str_text.substr(state_tab.str_prefix.size());
    } else {
      state_tab.str_prefix = "";
      state_tab.str_suffix = " ";
      str_to_complete = str_text;
    }

    state_tab.vec_candidates.clear();
    for (const auto& str_name : *state_tab.opt_names)
      if (StartsWithIgnoreCase(str_name, str_to_complete))
        state_tab.vec_candidates.push_back(str_name);
  } else {
    return str_text;
  }

  state_tab.int4_current_candidate_index = 0;

  if (state_tab.vec_candidates.empty())
    return str_text;

  state_tab.str_last_completed =
      state_tab.str_prefix + state_tab.vec_candidates[0] +
      state_tab.str_suffix;
  return state_tab.str_last_completed;
}

void SetupLatencyTooltip(ora::ui::Widget& widget,
                         ora::ui::TooltipContainerWidget& tooltip_container,
                         net::SessionClient* ptr_client) {
  auto& label_latency_prefix = *widget.Get<ora::ui::LabelWidget>(
      "LATENCY_PREFIX");
  ora::gfx::SpriteFont& font_prefix =
      ora::ui::RequireWidgetFont(label_latency_prefix.str_font);
  auto& label_latency = *widget.Get<ora::ui::LabelWidget>("LATENCY");
  ora::gfx::SpriteFont& font_latency =
      ora::ui::RequireWidgetFont(label_latency.str_font);
  const int int4_right_margin = widget.bounds_widget.int4_width;

  label_latency.bounds_widget.int4_x =
      label_latency_prefix.bounds_widget.int4_x +
      font_prefix.Measure(label_latency_prefix.fn_get_text() + " ").X;

  widget.fn_is_visible = [ptr_client] { return ptr_client != nullptr; };
  tooltip_container.fn_before_render =
      [&widget, &label_latency, &font_latency, int4_right_margin] {
        if (widget.fn_is_visible())
          widget.bounds_widget.int4_width =
              label_latency.bounds_widget.int4_x +
              font_latency.Measure(label_latency.fn_get_text()).X +
              int4_right_margin;
      };

  label_latency.fn_get_text = [ptr_client] {
    return LatencyDescription(ptr_client);
  };
  label_latency.fn_get_color = [ptr_client] {
    return LatencyColor(ptr_client);
  };
}

void SetupSpawnSelectorTooltip(
    ora::ui::Widget& widget, game::ModData& mod_data,
    ora::ui::TooltipContainerWidget& tooltip_container,
    ora::ui::MapPreviewWidget& preview, bool b_show_unoccupied_spawnpoints) {
  (void)mod_data;

  auto ptr_b_show = std::make_shared<bool>(b_show_unoccupied_spawnpoints);
  auto ptr_b_show_tooltip = std::make_shared<bool>(true);

  widget.fn_is_visible = [&preview, ptr_b_show_tooltip] {
    return preview.int4_tooltip_spawn_index != -1 && *ptr_b_show_tooltip;
  };

  auto& label = *widget.Get<ora::ui::LabelWidget>("LABEL");
  auto& flag = *widget.Get<ora::ui::ImageWidget>("FLAG");
  auto& team = *widget.Get<ora::ui::LabelWidget>("TEAM");
  const int int4_single_height = widget.Get("SINGLE_HEIGHT")
                                     ->bounds_widget.int4_height;
  const int int4_double_height = widget.Get("DOUBLE_HEIGHT")
                                     ->bounds_widget.int4_height;
  ora::gfx::SpriteFont& font_owner =
      ora::ui::RequireWidgetFont(label.str_font);
  ora::gfx::SpriteFont& font_team = ora::ui::RequireWidgetFont(team.str_font);

  const int int4_label_margin = widget.bounds_widget.int4_width;

  auto ptr_label_text = std::make_shared<std::string>("");
  auto ptr_player_faction = std::make_shared<std::optional<std::string>>();
  auto ptr_player_team = std::make_shared<int>(-1);
  auto ptr_player_spawn = std::make_shared<std::string>("");
  auto ptr_occupied = std::make_shared<bool>(false);

  auto ptr_transform_disabled = std::make_shared<
      core::CachedTransform<std::string, std::string>>(
      [](const std::string& str_spawn) {
        return Msg1("label-disabled-spawn", "spawn", str_spawn);
      });
  auto ptr_transform_available =
      std::make_shared<core::CachedTransform<std::string, std::string>>(
          [](const std::string& str_spawn) {
            return Msg1("label-available-spawn", "spawn", str_spawn);
          });
  auto ptr_transform_spawn =
      std::make_shared<core::CachedTransform<std::string, std::string>>(
          [](const std::string& str_spawn) {
            return Msg1("label-spawn-name", "spawn", str_spawn);
          });
  auto ptr_transform_team = std::make_shared<
      core::CachedTransform<std::pair<int, std::string>, std::string>>(
      [](const std::pair<int, std::string>& pair_team_spawn) {
        return MsgInt("label-team-spawn", "team", pair_team_spawn.first);
      });

  tooltip_container.fn_before_render =
      [&widget, &preview, &label, &team, &flag, &font_owner, &font_team,
       ptr_b_show, ptr_b_show_tooltip, ptr_label_text,
       ptr_player_faction, ptr_player_team, ptr_player_spawn, ptr_occupied,
       ptr_transform_disabled, ptr_transform_available,
       ptr_transform_spawn, ptr_transform_team, int4_label_margin,
       int4_single_height, int4_double_height]() mutable {
        *ptr_b_show_tooltip = true;

        int int4_team_width = 0;
        *ptr_player_spawn = SpawnLetter(preview.int4_tooltip_spawn_index);
        const auto map_occupants = preview.fn_spawn_occupants();
        const auto it_occupant = map_occupants.find(
            preview.int4_tooltip_spawn_index);
        if (it_occupant != map_occupants.end()) {
          *ptr_label_text = it_occupant->second.str_player_name;
          *ptr_player_faction = it_occupant->second.str_faction;
          *ptr_player_team = it_occupant->second.int4_team;
          *ptr_occupied = true;
          widget.bounds_widget.int4_height = int4_double_height;
          int4_team_width = font_team.Measure(team.fn_get_text()).X;
        } else {
          if (!*ptr_b_show) {
            *ptr_b_show_tooltip = false;
            return;
          }

          bool b_disabled = false;
          for (int int4_disabled : preview.fn_disabled_spawn_points())
            if (int4_disabled == preview.int4_tooltip_spawn_index) {
              b_disabled = true;
              break;
            }

          *ptr_label_text =
              b_disabled
                  ? ptr_transform_disabled->Update(*ptr_player_spawn)
                  : ptr_transform_available->Update(*ptr_player_spawn);

          ptr_player_faction->reset();
          *ptr_player_team = 0;
          *ptr_occupied = false;
          widget.bounds_widget.int4_height = int4_single_height;
        }

        label.bounds_widget.int4_x =
            ptr_player_faction->has_value()
                ? flag.bounds_widget.Right() + int4_label_margin
                : int4_label_margin;
        label.bounds_widget.int4_width =
            font_owner.Measure(*ptr_label_text).X;

        widget.bounds_widget.int4_width =
            std::max(int4_team_width + 2 * int4_label_margin,
                     label.bounds_widget.Right() + int4_label_margin);
        team.bounds_widget.int4_width = widget.bounds_widget.int4_width;
      };

  label.fn_get_text = [ptr_label_text] { return *ptr_label_text; };
  flag.fn_is_visible = [ptr_player_faction] {
    return ptr_player_faction->has_value();
  };
  flag.fn_get_image_collection = [] { return "flags"; };
  flag.fn_get_image_name = [ptr_player_faction] {
    return ptr_player_faction->value_or(std::string{});
  };
  team.fn_get_text = [ptr_occupied, ptr_player_team, ptr_player_spawn,
                      ptr_transform_team, ptr_transform_spawn] {
    if (!*ptr_occupied)
      return std::string{};

    return *ptr_player_team > 0
               ? ptr_transform_team->Update(
                     std::make_pair(*ptr_player_team, *ptr_player_spawn))
               : ptr_transform_spawn->Update(*ptr_player_spawn);
  };
  team.fn_is_visible = [ptr_occupied] { return *ptr_occupied; };
}

void SetupKickClient(ora::ui::Widget& widget, std::string str_client_name,
                     std::function<void(bool)> fn_ok_pressed,
                     std::function<void()> fn_cancel_pressed) {
  const std::string str_kick_message =
      Msg1("dialog-kick-client.prompt", "player", str_client_name);
  widget.Get<ora::ui::LabelWidget>("TITLE")->fn_get_text =
      [str_kick_message] { return str_kick_message; };

  auto ptr_temp_ban = std::make_shared<bool>(false);
  auto& checkbox_prevent = *widget.Get<ora::ui::CheckboxWidget>(
      "PREVENT_REJOINING_CHECKBOX");
  checkbox_prevent.fn_is_checked = [ptr_temp_ban] { return *ptr_temp_ban; };
  checkbox_prevent.fn_on_click = [ptr_temp_ban]() mutable {
    *ptr_temp_ban = !*ptr_temp_ban;
  };

  ora::ui::Widget* ptr_widget = &widget;
  widget.Get<ora::ui::ButtonWidget>("OK_BUTTON")->fn_on_click =
      [ptr_widget, ptr_temp_ban, fn_ok_pressed]() mutable {
        if (ora::ui::Widget* ptr_parent = ptr_widget->ptr_parent)
          ptr_parent->RemoveChild(*ptr_widget);
        fn_ok_pressed(*ptr_temp_ban);
      };
  widget.Get<ora::ui::ButtonWidget>("CANCEL_BUTTON")->fn_on_click =
      [ptr_widget, fn_cancel_pressed]() mutable {
        if (ora::ui::Widget* ptr_parent = ptr_widget->ptr_parent)
          ptr_parent->RemoveChild(*ptr_widget);
        fn_cancel_pressed();
      };
}

void SetupKickSpectators(ora::ui::Widget& widget, int int4_client_count,
                         std::function<void()> fn_ok_pressed,
                         std::function<void()> fn_cancel_pressed) {
  const std::string str_kick_message =
      MsgInt("dialog-kick-spectators.prompt", "count", int4_client_count);
  widget.Get<ora::ui::LabelWidget>("TEXT")->fn_get_text =
      [str_kick_message] { return str_kick_message; };

  ora::ui::Widget* ptr_widget = &widget;
  widget.Get<ora::ui::ButtonWidget>("OK_BUTTON")->fn_on_click =
      [ptr_widget, fn_ok_pressed]() mutable {
        if (ora::ui::Widget* ptr_parent = ptr_widget->ptr_parent)
          ptr_parent->RemoveChild(*ptr_widget);
        fn_ok_pressed();
      };
  widget.Get<ora::ui::ButtonWidget>("CANCEL_BUTTON")->fn_on_click =
      [ptr_widget, fn_cancel_pressed]() mutable {
        if (ora::ui::Widget* ptr_parent = ptr_widget->ptr_parent)
          ptr_parent->RemoveChild(*ptr_widget);
        fn_cancel_pressed();
      };
}

MapPreviewLogic::MapPreviewLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    std::function<std::pair<map::MapPreview*, int>()> fn_get_map,
    std::function<void(ora::ui::MapPreviewWidget*, map::MapPreview*,
                       const MouseInput&)> fn_on_mouse_down,
    std::function<std::map<int, ora::ui::SpawnOccupant>()>
        fn_get_spawn_occupants,
    bool b_map_updates_enabled,
    std::function<void(const std::string&)> fn_on_map_update,
    std::function<std::vector<int>()> fn_get_disabled_spawn_points,
    bool b_show_unoccupied_spawnpoints)
    : fn_get_map_(std::move(fn_get_map)),
      fn_on_map_update_(std::move(fn_on_map_update)),
      b_map_updates_enabled_(b_map_updates_enabled) {
  auto fn_setup_map_preview = [&](ora::ui::Widget& parent) {
    auto& preview = *parent.Get<ora::ui::MapPreviewWidget>("MAP_PREVIEW");
    preview.fn_preview = [this] { return fn_get_map_().first; };
    if (fn_on_mouse_down)
      preview.fn_on_mouse_down = [this, &preview,
                                  fn_on_mouse_down](const MouseInput& mi) {
        fn_on_mouse_down(&preview, fn_get_map_().first, mi);
      };

    if (fn_get_spawn_occupants)
      preview.fn_spawn_occupants = std::move(fn_get_spawn_occupants);

    if (fn_get_disabled_spawn_points)
      preview.fn_disabled_spawn_points = std::move(
          fn_get_disabled_spawn_points);

    preview.b_show_unoccupied_spawnpoints = b_show_unoccupied_spawnpoints;

    auto& label_title = *parent.Get<ora::ui::LabelWithTooltipWidget>(
        "MAP_TITLE");
    label_title.fn_is_visible = [this] {
      return fn_get_map_().first != &map::MapCache::UnknownMap();
    };
    ora::gfx::SpriteFont& font_title =
        ora::ui::RequireWidgetFont(label_title.str_font);
    auto cache_title = std::make_shared<
        core::CachedTransform<std::string, std::string>>(
        [&label_title, &font_title](const std::string& str_title) {
          const std::string str_truncated = ora::ui::TruncateText(
              str_title, label_title.bounds_widget.int4_width, font_title);

          if (str_title != str_truncated)
            label_title.fn_get_tooltip_text = [str_title] {
              return str_title;
            };
          else
            label_title.fn_get_tooltip_text = nullptr;

          return str_truncated;
        });

    label_title.fn_get_text = [this, cache_title] {
      return cache_title->Update(fn_get_map_().first->Title());
    };

    return &parent;
  };

  auto cache_author = std::make_shared<
      core::CachedTransform<std::string, std::string>>(
      [](const std::string& str_author) {
        return Msg1("label-created-by", "author", str_author);
      });

  auto fn_setup_author_and_map_type = [&](ora::ui::Widget& parent) {
    auto& label_type = *parent.Get<ora::ui::LabelWidget>("MAP_TYPE");
    auto cache_type = std::make_shared<
        core::CachedTransform<std::vector<std::string>, std::string>>(
        [](const std::vector<std::string>& vec_categories) {
          return !vec_categories.empty() ? vec_categories.front()
                                         : std::string{};
        });

    label_type.fn_get_text = [this, cache_type] {
      return cache_type->Update(fn_get_map_().first->Categories());
    };

    auto& label_author = *parent.Get<ora::ui::LabelWidget>("MAP_AUTHOR");
    ora::gfx::SpriteFont& font_author =
        ora::ui::RequireWidgetFont(label_author.str_font);
    auto cache_truncate = std::make_shared<
        core::CachedTransform<std::string, std::string>>(
        [&label_author, &font_author,
         cache_author](const std::string& str_author) {
          return ora::ui::TruncateText(
              cache_author->Update(str_author),
              label_author.bounds_widget.int4_width, font_author);
        });

    label_author.fn_get_text = [this, cache_truncate] {
      return cache_truncate->Update(fn_get_map_().first->Author());
    };

    return &parent;
  };

  const std::string str_map_repository =
      game::WebServices{}.MapRepository;

  auto fn_set_up_install_button = [&](ora::ui::Widget& parent) {
    auto& button_install = *parent.Get<ora::ui::ButtonWidget>(
        "MAP_INSTALL");
    button_install.fn_is_highlighted = [this] { return b_blink_; };
    button_install.fn_on_click = [this, str_map_repository] {
      fn_get_map_().first->Install(str_map_repository);
    };
    return &parent;
  };

  auto& button_update = *widget.Get<ora::ui::ButtonWidget>("MAP_UPDATE");
  button_update.fn_is_highlighted = [this] { return b_blink_; };
  button_update.fn_on_click = [this](void) {
    const std::string str_uid = fn_get_map_().first->Uid();
    std::string str_new_uid;
    if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps())
      str_new_uid = ptr_cache->GetUpdatedMap(str_uid);
    if (!str_new_uid.empty() && str_new_uid != str_uid && fn_on_map_update_)
      fn_on_map_update_(str_new_uid);
  };

  auto fn_set_up_download_progress = [&](ora::ui::Widget& parent) {
    auto& progressbar = *parent.Get<ora::ui::ProgressBarWidget>(
        "MAP_PROGRESSBAR");
    progressbar.fn_is_indeterminate = [this] {
      return fn_get_map_().first->DownloadPercentage() == 0;
    };
    progressbar.fn_get_percentage = [this] {
      return fn_get_map_().first->DownloadPercentage();
    };

    parent.Get<ora::ui::LabelWidget>("MAP_STATUS_DOWNLOADING")
        ->fn_get_text = [this] {
      map::MapPreview* ptr_map = fn_get_map_().first;
      if (ptr_map->DownloadBytes() == 0)
        return Msg("label-connecting");

      if (ptr_map->DownloadPercentage() == 0)
        return Msg1("label-downloading-map", "size",
                    std::to_string(ptr_map->DownloadBytes() / 1024));

      return Fluent().GetMessage(
          "label-downloading-map-progress",
          std::vector<std::pair<std::string, fluent::FluentValue>>{
              {"size",
               fluent::FluentValue{ptr_map->DownloadBytes() / 1024}},
              {"progress",
               fluent::FluentValue{ptr_map->DownloadPercentage()}}});
    };

    return &parent;
  };

  ptr_retry_button_ = widget.Get<ora::ui::ButtonWidget>("MAP_RETRY");
  ptr_retry_button_->fn_on_click = [this, str_map_repository] {
    b_retry_triggered_ = true;
    map::MapPreview* ptr_map = fn_get_map_().first;

    std::string str_updated_uid;
    if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps())
      str_updated_uid = ptr_cache->GetUpdatedMap(ptr_map->Uid());
    b_map_update_available_ = b_map_updates_enabled_ &&
                              !str_updated_uid.empty() &&
                              ptr_map->Uid() != str_updated_uid;

    if (ptr_map->Status() == map::MapStatus::DownloadError)
      ptr_map->Install(str_map_repository);
    else if (ptr_map->Status() == map::MapStatus::Unavailable)
      if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps())
        ptr_cache->QueryRemoteMapDetails(
            str_map_repository, std::vector<std::string>{ptr_map->Uid()});
  };

  const std::string str_retry_install = Msg("button-retry-install");
  const std::string str_retry_search = Msg("button-retry-search");
  ptr_retry_button_->fn_get_text = [this, str_retry_install,
                                    str_retry_search] {
    return fn_get_map_().first->Status() ==
                   map::MapStatus::DownloadError
               ? str_retry_install
               : str_retry_search;
  };

  ora::ui::Widget* ptr_preview_large =
      fn_setup_map_preview(*widget.Get("MAP_LARGE"));
  ora::ui::Widget* ptr_preview_small =
      fn_setup_map_preview(*widget.Get("MAP_SMALL"));

  map_preview_widgets_[PreviewStatus::Unknown] = {ptr_preview_large};
  map_preview_widgets_[PreviewStatus::Playable] = {
      ptr_preview_large,
      fn_setup_author_and_map_type(*widget.Get("MAP_AVAILABLE"))};
  map_preview_widgets_[PreviewStatus::Incompatible] = {
      ptr_preview_large, widget.Get("MAP_INCOMPATIBLE")};
  map_preview_widgets_[PreviewStatus::Validating] = {
      ptr_preview_small, widget.Get("MAP_VALIDATING")};
  map_preview_widgets_[PreviewStatus::Generating] = {
      ptr_preview_small, widget.Get("MAP_GENERATING")};
  map_preview_widgets_[PreviewStatus::GenerationError] = {
      ptr_preview_small, widget.Get("MAP_GENERATION_ERROR")};
  map_preview_widgets_[PreviewStatus::UpdateAvailable] = {
      ptr_preview_small, widget.Get("MAP_UPDATE_AVAILABLE"), &button_update};
  map_preview_widgets_[PreviewStatus::DownloadAvailable] = {
      ptr_preview_small,
      fn_set_up_install_button(
          *fn_setup_author_and_map_type(*widget.Get("MAP_DOWNLOAD_AVAILABLE")))};
  map_preview_widgets_[PreviewStatus::UpdateDownloadAvailable] = {
      ptr_preview_small,
      fn_set_up_install_button(*widget.Get("MAP_UPDATE_DOWNLOAD_AVAILABLE")),
      &button_update};
  map_preview_widgets_[PreviewStatus::Searching] = {
      ptr_preview_small, widget.Get("MAP_SEARCHING")};
  map_preview_widgets_[PreviewStatus::Downloading] = {
      ptr_preview_small,
      fn_set_up_download_progress(*widget.Get("MAP_DOWNLOADING"))};
  map_preview_widgets_[PreviewStatus::Unavailable] = {
      ptr_preview_small, widget.Get("MAP_UNAVAILABLE"), ptr_retry_button_};
  map_preview_widgets_[PreviewStatus::DownloadError] = {
      ptr_preview_small, widget.Get("MAP_ERROR"), ptr_retry_button_};

  for (const auto& [kind_status, vec_widgets] : map_preview_widgets_)
    for (ora::ui::Widget* ptr_panel : vec_widgets)
      ptr_panel->fn_is_visible = [] { return false; };

  UpdateVisibility();
  (void)mod_data;
}

void MapPreviewLogic::UpdateVisibility() {
  for (ora::ui::Widget* ptr_widget : vec_visible_widgets_)
    ptr_widget->fn_is_visible = [] { return false; };

  vec_visible_widgets_ = map_preview_widgets_[kind_current_status_];
  for (ora::ui::Widget* ptr_widget : vec_visible_widgets_)
    ptr_widget->fn_is_visible = [] { return true; };
}

void MapPreviewLogic::Tick() {
  if (++int4_blink_tick_ >= int4_blink_tick_length_) {
    b_blink_ = !b_blink_;
    int4_blink_tick_ = 0;
  }

  auto [ptr_map, int4_server_status] = fn_get_map_();

  std::optional<PreviewStatus> opt_status;
  if (ptr_map == &map::MapCache::UnknownMap()) {
    opt_status = PreviewStatus::Unknown;
  } else {
    switch (ptr_map->Status()) {
      case map::MapStatus::Available:
        if (net::HasMapStatusFlag(int4_server_status,
                                  net::SessionMapStatus::Playable))
          opt_status = PreviewStatus::Playable;
        else if (net::HasMapStatusFlag(int4_server_status,
                                       net::SessionMapStatus::Incompatible))
          opt_status = PreviewStatus::Incompatible;
        else if (net::HasMapStatusFlag(int4_server_status,
                                       net::SessionMapStatus::Validating))
          opt_status = PreviewStatus::Validating;
        else
          return;
        break;
      case map::MapStatus::DownloadAvailable:
        opt_status = b_map_update_available_
                         ? PreviewStatus::UpdateDownloadAvailable
                         : PreviewStatus::DownloadAvailable;
        break;
      case map::MapStatus::Searching:
        opt_status = PreviewStatus::Searching;
        break;
      case map::MapStatus::Generating:
        opt_status = PreviewStatus::Generating;
        break;
      case map::MapStatus::Unavailable:
        if (b_map_update_available_)
          opt_status = PreviewStatus::UpdateAvailable;
        else if (ptr_map->Class() == map::MapClassification::Generated)
          opt_status = PreviewStatus::GenerationError;
        else
          opt_status = PreviewStatus::Unavailable;
        break;
      case map::MapStatus::DownloadError:
        opt_status = PreviewStatus::DownloadError;
        break;
      case map::MapStatus::Downloading:
        opt_status = PreviewStatus::Downloading;
        break;
      default:
        break;
    }
  }

  if (opt_status.has_value() && *opt_status != kind_current_status_) {
    PreviewStatus kind_status = *opt_status;
    if (kind_status == PreviewStatus::Unavailable ||
        kind_status == PreviewStatus::UpdateAvailable) {
      if (b_retry_triggered_) {
        b_retry_triggered_ = false;
      } else {
        ptr_retry_button_->fn_on_click();
        if (ptr_map->Status() == map::MapStatus::Searching)
          kind_status = PreviewStatus::Searching;
        else if (b_map_update_available_)
          kind_status = PreviewStatus::UpdateAvailable;
        else
          kind_status = PreviewStatus::Unavailable;
      }
    } else if (kind_status != PreviewStatus::Searching) {
      b_retry_triggered_ = false;
    }

    kind_current_status_ = kind_status;
    UpdateVisibility();
  }
}

LobbyOptionsLogic::LobbyOptionsLogic(
    ora::ui::Widget& widget, net::OrderManager& order_manager,
    std::function<map::MapPreview*()> fn_get_map,
    std::function<bool()> fn_configuration_disabled)
    : fn_get_map_(std::move(fn_get_map)),
      ptr_order_manager_(&order_manager),
      fn_configuration_disabled_(std::move(fn_configuration_disabled)) {
  ptr_panel_ = dynamic_cast<ora::ui::ScrollPanelWidget*>(&widget);
  ptr_options_container_ = widget.Get("LOBBY_OPTIONS");
  int4_y_margin_ = ptr_options_container_->bounds_widget.int4_y;
  up_checkbox_row_template_.reset(
      ptr_options_container_->RemoveChild(
          *ptr_options_container_->Get("CHECKBOX_ROW_TEMPLATE"))
          .release());
  up_dropdown_row_template_.reset(
      ptr_options_container_->RemoveChild(
          *ptr_options_container_->Get("DROPDOWN_ROW_TEMPLATE"))
          .release());

  ptr_map_preview_ = fn_get_map_();
  if (ptr_map_preview_ != nullptr)
    kind_map_status_ = ptr_map_preview_->Status();
  RebuildOptions();
}

void LobbyOptionsLogic::Tick() {
  map::MapPreview* ptr_new_map_preview = fn_get_map_();
  if (ptr_new_map_preview == ptr_map_preview_ && ptr_map_preview_ != nullptr &&
      kind_map_status_ == ptr_map_preview_->Status())
    return;

  ora::ui::Ui::Instance().RunAfterTickDefer([this, ptr_new_map_preview] {
    ptr_map_preview_ = ptr_new_map_preview;
    if (ptr_map_preview_ != nullptr)
      kind_map_status_ = ptr_map_preview_->Status();
    RebuildOptions();
  });
}

std::size_t LobbyOptionsLogic::RowCountForTest() const {
  return ptr_options_container_ != nullptr
             ? ptr_options_container_->Children().size()
             : std::size_t{0};
}

void LobbyOptionsLogic::RebuildOptions() {
  if (ptr_map_preview_ == nullptr ||
      ptr_map_preview_->WorldActorInfo() == nullptr)
    return;

  ptr_options_container_->RemoveChildren();
  ptr_options_container_->bounds_widget.int4_height = 0;

  auto vec_all_options = CollectMapLobbyOptions(*ptr_map_preview_);
  std::vector<server::LobbyOptionData*> vec_options;
  for (auto& option : vec_all_options)
    if (option.IsVisible)
      vec_options.push_back(&option);
  std::stable_sort(vec_options.begin(), vec_options.end(),
                   [](const server::LobbyOptionData* ptr_a,
                      const server::LobbyOptionData* ptr_b) {
                     return ptr_a->DisplayOrder < ptr_b->DisplayOrder;
                   });

  std::deque<ora::ui::CheckboxWidget*> deque_checkbox_columns;
  std::deque<ora::ui::DropDownButtonWidget*> deque_dropdown_columns;
  ora::ui::Widget* ptr_row = nullptr;

  for (auto* ptr_option : vec_options) {
    if (!ptr_option->IsBooleanOption)
      continue;

    if (deque_checkbox_columns.empty()) {
      auto up_row = up_checkbox_row_template_->Clone();
      ptr_row = up_row.get();
      ptr_row->bounds_widget.int4_y =
          ptr_options_container_->bounds_widget.int4_height;
      ptr_options_container_->bounds_widget.int4_height +=
          ptr_row->bounds_widget.int4_height;
      for (ora::ui::Widget* ptr_child : ptr_row->Children())
        if (auto* ptr_child_checkbox =
                dynamic_cast<ora::ui::CheckboxWidget*>(ptr_child))
          deque_checkbox_columns.push_back(ptr_child_checkbox);

      ptr_options_container_->AddChild(std::move(up_row));
    }

    ora::ui::CheckboxWidget& checkbox = *deque_checkbox_columns.front();
    deque_checkbox_columns.pop_front();

    auto ptr_option_enabled =
        std::make_shared<core::PredictedCachedTransform<
            const net::SessionGlobalSettings*, bool>>(
            [str_id = ptr_option->Id](
                const net::SessionGlobalSettings* ptr_global) {
              const auto it_option =
                  ptr_global->map_lobby_options.find(str_id);
              return it_option != ptr_global->map_lobby_options.end() &&
                     it_option->second.IsEnabled();
            });

    auto ptr_option_locked = std::make_shared<
        core::CachedTransform<const net::SessionGlobalSettings*, bool>>(
        [str_id = ptr_option->Id](
            const net::SessionGlobalSettings* ptr_global) {
          const auto it_option = ptr_global->map_lobby_options.find(str_id);
          return it_option != ptr_global->map_lobby_options.end() &&
                 it_option->second.IsLocked;
        });

    checkbox.fn_get_text = [str_name = ptr_option->Name] {
      return str_name;
    };
    if (!ptr_option->Description.empty()) {
      const auto [opt_text, opt_desc] =
          SplitOnFirstToken(ptr_option->Description);
      checkbox.fn_get_tooltip_text = [opt_text] {
        return opt_text.value_or(std::string{});
      };
      checkbox.fn_get_tooltip_desc = [opt_desc] {
        return opt_desc.value_or(std::string{});
      };
    }

    checkbox.fn_is_visible = [] { return true; };
    checkbox.fn_is_checked = [this, ptr_option_enabled] {
      return ptr_option_enabled->Update(
          &ptr_order_manager_->LobbyInfo().global_settings);
    };
    checkbox.fn_is_disabled = [this, ptr_option_locked] {
      return fn_configuration_disabled_() ||
             ptr_option_locked->Update(
                 &ptr_order_manager_->LobbyInfo().global_settings);
    };
    checkbox.fn_on_click = [this, ptr_option_enabled,
                            str_id = ptr_option->Id]() mutable {
      const bool b_state = !ptr_option_enabled->Update(
          &ptr_order_manager_->LobbyInfo().global_settings);
      ptr_order_manager_->IssueOrder(net::Order::Command(
          std::format("option {} {}", str_id,
                      b_state ? "True" : "False")));
      ptr_option_enabled->Predict(b_state);
    };
  }

  for (auto* ptr_option : vec_options) {
    if (ptr_option->IsBooleanOption)
      continue;

    if (deque_dropdown_columns.empty()) {
      auto up_row = up_dropdown_row_template_->Clone();
      ptr_row = up_row.get();
      ptr_row->bounds_widget.int4_y =
          ptr_options_container_->bounds_widget.int4_height;
      ptr_options_container_->bounds_widget.int4_height +=
          ptr_row->bounds_widget.int4_height;
      for (ora::ui::Widget* ptr_child : ptr_row->Children())
        if (auto* ptr_child_dropdown =
                dynamic_cast<ora::ui::DropDownButtonWidget*>(ptr_child))
          deque_dropdown_columns.push_back(ptr_child_dropdown);

      ptr_options_container_->AddChild(std::move(up_row));
    }

    ora::ui::DropDownButtonWidget& dropdown = *deque_dropdown_columns.front();
    deque_dropdown_columns.pop_front();

    auto ptr_option_value = std::make_shared<
        core::CachedTransform<const net::SessionGlobalSettings*,
                              const net::SessionLobbyOption*>>(
        [str_id = ptr_option->Id](
            const net::SessionGlobalSettings* ptr_global) {
          const auto it_option = ptr_global->map_lobby_options.find(str_id);
          return it_option != ptr_global->map_lobby_options.end()
                     ? &it_option->second
                     : nullptr;
        });

    auto ptr_option_data =
        std::make_shared<server::LobbyOptionData>(*ptr_option);

    auto ptr_get_option_label = std::make_shared<
        core::CachedTransform<std::string, std::string>>(
        [ptr_option_data](const std::string& str_id) {
          if (str_id.empty())
            return Msg("label-not-available");

          const std::string* ptr_value_label =
              ptr_option_data->FindValueLabel(str_id);
          if (ptr_value_label == nullptr)
            return Msg("label-not-available");

          return *ptr_value_label;
        });

    dropdown.fn_get_text = [this, ptr_option_value,
                            ptr_get_option_label] {
      const net::SessionLobbyOption* ptr_state = ptr_option_value->Update(
          &ptr_order_manager_->LobbyInfo().global_settings);
      return ptr_get_option_label->Update(
          ptr_state != nullptr ? ptr_state->Value : std::string{});
    };
    if (!ptr_option->Description.empty()) {
      const auto [opt_text, opt_desc] =
          SplitOnFirstToken(ptr_option->Description);
      dropdown.fn_get_tooltip_text = [opt_text] {
        return opt_text.value_or(std::string{});
      };
      dropdown.fn_get_tooltip_desc = [opt_desc] {
        return opt_desc.value_or(std::string{});
      };
    }

    dropdown.fn_is_visible = [] { return true; };
    dropdown.fn_is_disabled = [this, ptr_option_value] {
      if (fn_configuration_disabled_())
        return true;

      const net::SessionLobbyOption* ptr_state = ptr_option_value->Update(
          &ptr_order_manager_->LobbyInfo().global_settings);
      return ptr_state != nullptr && ptr_state->IsLocked;
    };

    dropdown.fn_on_mouse_down =
        [this, &dropdown, ptr_option_data,
         ptr_option_value](const MouseInput&) {
          dropdown.ShowDropDown<std::pair<std::string, std::string>>(
              "LABEL_DROPDOWN_TEMPLATE",
              static_cast<int>(ptr_option_data->vec_values.size()) * 30,
              ptr_option_data->vec_values,
              [this, ptr_option_data, ptr_option_value](
                  const std::pair<std::string, std::string>& pair_value,
                  ora::ui::ScrollItemWidget& item_template) {
                auto up_item = ora::ui::ScrollItemWidget::Setup(
                    item_template,
                    [this, ptr_option_value, str_key = pair_value.first] {
                      const net::SessionLobbyOption* ptr_state =
                          ptr_option_value->Update(
                              &ptr_order_manager_->LobbyInfo()
                                   .global_settings);
                      return ptr_state != nullptr &&
                             ptr_state->Value == str_key;
                    },
                    [this, str_id = ptr_option_data->Id,
                     str_key = pair_value.first] {
                      ptr_order_manager_->IssueOrder(net::Order::Command(
                          std::format("option {} {}", str_id, str_key)));
                    });
                up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                    [str_label = pair_value.second] { return str_label; };
                return up_item;
              });
        };

    if (ptr_row != nullptr) {
      if (auto* ptr_label = ptr_row->GetOrNull<ora::ui::LabelWidget>(
              dropdown.str_id + "_DESC")) {
        ptr_label->fn_get_text = [str_name = ptr_option->Name] {
          return str_name + ":";
        };
        ptr_label->fn_is_visible = [] { return true; };
      }
    }
  }

  ptr_panel_->int4_content_height =
      int4_y_margin_ + ptr_options_container_->bounds_widget.int4_height;
  ptr_options_container_->bounds_widget.int4_y = int4_y_margin_;

  ptr_panel_->ScrollToTop();
}

namespace {

net::TextNotificationPool ParseTextNotificationPool(
    const std::string& str_key) {
  if (str_key == "System")
    return net::TextNotificationPool::System;
  if (str_key == "Join")
    return net::TextNotificationPool::Join;
  if (str_key == "Leave")
    return net::TextNotificationPool::Leave;
  if (str_key == "Chat")
    return net::TextNotificationPool::Chat;
  if (str_key == "Mission")
    return net::TextNotificationPool::Mission;
  if (str_key == "Feedback")
    return net::TextNotificationPool::Feedback;
  if (str_key == "Transients")
    return net::TextNotificationPool::Transients;
  throw std::runtime_error(
      "FieldLoader: Invalid value for field of type TextNotificationPool: " +
      str_key);
}

std::vector<std::pair<std::string, std::string>> CollectBotTypes(
    const map::MapPreview* ptr_map) {
  std::vector<std::pair<std::string, std::string>> vec_bots;
  if (ptr_map == nullptr)
    return vec_bots;

  const auto ptr_player_actor_info = ptr_map->PlayerActorInfo();
  if (!ptr_player_actor_info)
    return vec_bots;

  for (const meta::RecordObject* rec_bot :
       ptr_player_actor_info->TraitInfosByInterface("IBotInfo")) {
    const auto sv_name = sim::RecordFieldString(*rec_bot, "Name");
    const auto sv_type = sim::RecordFieldString(*rec_bot, "Type");
    if (sv_name && sv_type)
      vec_bots.emplace_back(*sv_name, *sv_type);
  }
  return vec_bots;
}

}

void DiscordServiceNoop() {}

void LobbyLogic::ConnectionStateChanged(
    net::OrderManager* ptr_order_manager_notify,
    const std::string& str_password,
    net::NetworkConnection* ptr_connection) {
  if (ptr_connection == nullptr ||
      ptr_connection->State() != net::ConnectionState::NotConnected)
    return;

  ora::ui::Ui::Instance().CloseWindow();

  auto fn_on_connect = [fn_on_exit = fn_on_exit_,
                        fn_on_start = fn_on_start_]() mutable {
    ora::ui::WidgetArgs args_lobby;
    args_lobby.Add("onExit", fn_on_exit);
    args_lobby.Add("onStart", fn_on_start);
    args_lobby.Add("skirmishMode", false);
    ora::ui::Ui::Instance().OpenWindow("SERVER_LOBBY",
                                       std::move(args_lobby));
  };

  auto fn_on_retry =
      [target_retry = ptr_connection->Target(), fn_on_connect,
       fn_on_exit = fn_on_exit_](const std::string& str_pass) mutable {
        Connect(std::move(target_retry), str_pass, fn_on_connect,
                fn_on_exit);
      };

  const std::string str_switch_panel =
      CurrentServerExternalMod() != nullptr
          ? std::string{"CONNECTION_SWITCHMOD_PANEL"}
          : std::string{"CONNECTIONFAILED_PANEL"};
  ora::ui::WidgetArgs args_failed;
  args_failed.Add("orderManager", ptr_order_manager_notify);
  args_failed.Add("connection", ptr_connection);
  args_failed.Add("password", str_password);
  args_failed.Add("onAbort", fn_on_exit_);
  args_failed.Add("onRetry", std::function<void(const std::string&)>{
                                 fn_on_retry});
  ora::ui::Ui::Instance().OpenWindow(str_switch_panel,
                                     std::move(args_failed));
}

bool LobbyLogic::MapIsPlayable() const {
  return net::HasMapStatusFlag(int4_map_status_,
                               net::SessionMapStatus::Playable);
}

LobbyLogic::LobbyLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    gfx::WorldRenderer* ptr_world_renderer, net::OrderManager& order_manager,
    std::function<void()> fn_on_exit, std::function<void()> fn_on_start,
    bool b_skirmish_mode, const ora::ui::YamlDictionary* ptr_logic_args)
    : fn_on_start_(std::move(fn_on_start)),
      fn_on_exit_(std::move(fn_on_exit)),
      ptr_order_manager_(&order_manager),
      ptr_world_renderer_(ptr_world_renderer),
      b_skirmish_mode_(b_skirmish_mode) {
  ptr_map_ = &map::MapCache::UnknownMap();
  ptr_lobby_ = &widget;
  ptr_mod_data_ = &mod_data;
  ptr_mod_rules_ = &mod_data.DefaultRules();

  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeLobbyInfoChanged(
          [this] { UpdateCurrentMap(); }));
  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeLobbyInfoChanged(
          [this] { UpdatePlayerList(); }));
  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeLobbyInfoChanged(
          [this] { UpdateSpawnOccupants(); }));
  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeLobbyInfoChanged(
          [this] { UpdateOptions(); }));
  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeBeforeGameStart(
          [this] { OnGameStart(); }));
  vec_unsubscribers_.push_back(
      ora::ui::Ui::Instance().SubscribeConnectionStateChanged(
          [this](net::OrderManager* ptr_om, const std::string& str_password,
                 net::NetworkConnection* ptr_conn) {
            ConnectionStateChanged(ptr_om, str_password, ptr_conn);
          }));

  if (auto* ptr_metrics = ora::ui::Ui::Instance().ChromeMetricsDeps()) {
    ptr_metrics->TryGet("ChatLineSound", str_chat_line_sound_);
    ptr_metrics->TryGet("PlayerJoinedSound", str_player_joined_sound_);
    ptr_metrics->TryGet("PlayerLeftSound", str_player_left_sound_);
    ptr_metrics->TryGet("LobbyOptionChangedSound",
                        str_lobby_option_changed_sound_);
  }

  if (auto* ptr_name =
          widget.GetOrNull<ora::ui::LabelWidget>("SERVER_NAME"))
    ptr_name->fn_get_text = [this] {
      return ptr_order_manager_->LobbyInfo().global_settings.ServerName;
    };

  ora::ui::WidgetArgs args_map_preview;
  args_map_preview.Add("orderManager", ptr_order_manager_);
  args_map_preview.Add(
      "getMap",
      std::function<std::pair<map::MapPreview*, int>()>(
          [this] { return std::make_pair(ptr_map_, int4_map_status_); }));
  args_map_preview.Add(
      "onMouseDown",
      std::function<void(ora::ui::MapPreviewWidget*, map::MapPreview*,
                         const MouseInput&)>(
          [this](ora::ui::MapPreviewWidget* ptr_preview_widget,
                 map::MapPreview* ptr_map_event, const MouseInput& mi_input) {
            SelectSpawnPoint(*ptr_order_manager_, *ptr_preview_widget,
                             *ptr_map_event, mi_input);
          }));
  args_map_preview.Add(
      "getSpawnOccupants",
      std::function<std::map<int, ora::ui::SpawnOccupant>()>(
          [this] { return map_spawn_occupants_; }));
  args_map_preview.Add(
      "getDisabledSpawnPoints",
      std::function<std::vector<int>()>([this] {
        return ptr_order_manager_->LobbyInfo().vec_disabled_spawn_points;
      }));
  args_map_preview.Add("showUnoccupiedSpawnpoints", true);
  args_map_preview.Add("mapUpdatesEnabled", true);
  args_map_preview.Add(
      "onMapUpdate",
      std::function<void(const std::string&)>(
          [this](const std::string& str_uid) {
            ptr_order_manager_->IssueOrder(
                net::Order::Command("map " + str_uid));
            ora::ui::Ui::Instance().ServerMapSetDeps(str_uid);
            ora::ui::Ui::Instance().SettingsSaveDeps();
          }));

  ora::ui::Widget* ptr_map_container = ora::ui::Ui::Instance().LoadWidget(
      "MAP_PREVIEW", widget.Get("MAP_PREVIEW_ROOT"),
      std::move(args_map_preview));
  ptr_map_container->fn_is_visible = [this] {
    return kind_panel_ != PanelType::Servers;
  };

  UpdateCurrentMap();

  ora::ui::Widget* ptr_player_bin = ora::ui::Ui::Instance().LoadWidget(
      "LOBBY_PLAYER_BIN", widget.Get("TOP_PANELS_ROOT"),
      ora::ui::WidgetArgs{});
  ptr_player_bin->fn_is_visible = [this] {
    return kind_panel_ == PanelType::Players;
  };

  ptr_players_ =
      ptr_player_bin->Get<ora::ui::ScrollPanelWidget>("LOBBY_PLAYERS");
  up_editable_player_template_.reset(
      ptr_players_
          ->RemoveChild(*ptr_players_->Get("TEMPLATE_EDITABLE_PLAYER"))
          .release());
  up_non_editable_player_template_.reset(
      ptr_players_
          ->RemoveChild(*ptr_players_->Get("TEMPLATE_NONEDITABLE_PLAYER"))
          .release());
  up_empty_slot_template_.reset(
      ptr_players_->RemoveChild(*ptr_players_->Get("TEMPLATE_EMPTY"))
          .release());
  up_editable_spectator_template_.reset(
      ptr_players_
          ->RemoveChild(*ptr_players_->Get("TEMPLATE_EDITABLE_SPECTATOR"))
          .release());
  up_non_editable_spectator_template_.reset(
      ptr_players_
          ->RemoveChild(*ptr_players_->Get("TEMPLATE_NONEDITABLE_SPECTATOR"))
          .release());
  up_new_spectator_template_.reset(
      ptr_players_
          ->RemoveChild(*ptr_players_->Get("TEMPLATE_NEW_SPECTATOR"))
          .release());

  if (const game::ActorInfo* ptr_world_actor_info =
          ptr_mod_rules_->FindActor("world")) {
    for (const meta::RecordObject* rec_faction :
         ptr_world_actor_info->TraitInfosByInterface("FactionInfo")) {
      LobbyFaction faction{};
      if (const auto sv_name =
              sim::RecordFieldString(*rec_faction, "Name"))
        faction.Name = std::string{*sv_name};
      if (const auto sv_side =
              sim::RecordFieldString(*rec_faction, "Side"))
        faction.Side = std::string{*sv_side};
      if (const auto sv_description =
              sim::RecordFieldString(*rec_faction, "Description"))
        faction.Description = std::string{*sv_description};
      if (const auto val_selectable =
              sim::RecordFieldInt(*rec_faction, "Selectable"))
        faction.Selectable = *val_selectable != 0;
      if (const auto sv_internal =
              sim::RecordFieldString(*rec_faction, "InternalName"))
        vec_factions_.emplace_back(*sv_internal, std::move(faction));
    }
  }

  auto fn_configuration_disabled = [this] {
    return !ora::ui::Ui::Instance().IsHostDeps() || b_game_starting_ ||
           kind_panel_ == PanelType::Kick ||
           kind_panel_ == PanelType::ForceStart || !MapIsPlayable() ||
           ptr_order_manager_->LocalClient() == nullptr ||
           ptr_order_manager_->LocalClient()->IsReady();
  };

  if (auto* ptr_map_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("CHANGEMAP_BUTTON")) {
    ptr_map_button->fn_is_visible = [this] {
      return kind_panel_ != PanelType::Servers;
    };
    ptr_map_button->fn_is_disabled = [this] {
      return b_game_starting_ || kind_panel_ == PanelType::Kick ||
             kind_panel_ == PanelType::ForceStart ||
             ptr_order_manager_->LocalClient() == nullptr ||
             ptr_order_manager_->LocalClient()->IsReady();
    };
    ptr_map_button->fn_on_click = [this]() {
      auto fn_on_select =
          std::function<void(const std::string&)>(
              [this](const std::string& str_uid) {
                map::MapStatus kind_status = map::MapStatus::Unavailable;
                if (auto* ptr_cache =
                        ora::ui::Ui::Instance().MapCacheDeps())
                  kind_status = ptr_cache->At(str_uid).Status();
                if (str_uid == ptr_map_->Uid() ||
                    (kind_status != map::MapStatus::Available &&
                     kind_status != map::MapStatus::DownloadAvailable))
                  return;

                ptr_order_manager_->IssueOrder(
                    net::Order::Command("map " + str_uid));
                ora::ui::Ui::Instance().ServerMapSetDeps(str_uid);
                ora::ui::Ui::Instance().SettingsSaveDeps();
              });

      auto fn_on_select_generated =
          std::function<void(const map::MapGenerationArgs&)>(
              [this](const map::MapGenerationArgs& args_generated) {
                if (args_generated.str_uid == ptr_map_->Uid())
                  return;

                opt_last_generated_map_ = args_generated;
                yaml::StringPool pool_serialized;
                ptr_order_manager_->IssueOrder(
                    net::Order::FromTargetString(
                        "GenerateMap",
                        net::SerializeGameSaveMapGenerationArgs(
                            pool_serialized, args_generated)
                            .Value.Value != nullptr
                            ? *net::SerializeGameSaveMapGenerationArgs(
                                  pool_serialized, args_generated)
                                  .Value.Value
                            : std::string{},
                        true));
                ptr_order_manager_->IssueOrder(net::Order::Command(
                    "map " + args_generated.str_uid));
                ora::ui::Ui::Instance().ServerMapSetDeps(
                    args_generated.str_uid);
                ora::ui::Ui::Instance().SettingsSaveDeps();
              });

      if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps())
        ptr_cache->UpdateMaps();

      const bool b_enable_generator =
          ora::ui::Ui::Instance().IsHostDeps() &&
          ptr_order_manager_->LobbyInfo().global_settings
              .EnableMapGeneration;

      std::string str_initial_map = ptr_map_->Uid();
      if (auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps()) {
        const std::string str_last_modified =
            ptr_cache->PickLastModifiedMap(map::MapVisibility::Lobby);
        if (!str_last_modified.empty())
          str_initial_map = str_last_modified;
      }

      ora::ui::WidgetArgs args_chooser;
      args_chooser.Add("initialMap", str_initial_map);
      if (opt_last_generated_map_.has_value())
        args_chooser.Add("initialGeneratedMap",
                         &opt_last_generated_map_.value());
      static const std::vector<std::string> vec_pool_empty{};
      auto vec_pool = std::make_shared<std::vector<std::string>>();
      if (const auto& opt_pool =
              ptr_order_manager_->ServerMapPool())
        vec_pool->assign(opt_pool->begin(), opt_pool->end());
      args_chooser.Add("remoteMapPool",
                       vec_pool->empty()
                           ? &vec_pool_empty
                           : vec_pool.get());
      args_chooser.Add(
          "initialTab",
          static_cast<std::int32_t>(map::MapClassification::System));
      args_chooser.Add(
          "onExit",
          std::function<void()>{[] {
            if (auto* ptr_cache =
                    ora::ui::Ui::Instance().MapCacheDeps())
              ptr_cache->UpdateMaps();
          }});
      if (ora::ui::Ui::Instance().IsHostDeps())
        args_chooser.Add("onSelect", fn_on_select);
      if (b_enable_generator)
        args_chooser.Add("onSelectGenerated", fn_on_select_generated);
      args_chooser.Add(
          "filter",
          static_cast<std::int32_t>(map::MapVisibility::Lobby));
      ora::ui::Ui::Instance().OpenWindow("MAPCHOOSER_PANEL",
                                         std::move(args_chooser));
    };
  }

  if (auto* ptr_slots_button = widget.GetOrNull<ora::ui::DropDownButtonWidget>(
          "SLOTS_DROPDOWNBUTTON")) {
    ptr_slots_button->fn_is_visible = [this] {
      return kind_panel_ != PanelType::Servers &&
             kind_panel_ != PanelType::Options;
    };
    ptr_slots_button->fn_is_disabled =
        [this, fn_configuration_disabled]() mutable {
          if (fn_configuration_disabled() ||
              kind_panel_ != PanelType::Players)
            return true;

          if (ptr_order_manager_->LobbyInfo().vec_clients.size() == 1 &&
              CollectBotTypes(ptr_map_).empty())
            return true;

          const auto& session_lobby = ptr_order_manager_->LobbyInfo();
          bool b_any_allow_bots = false;
          for (const auto& [str_key, slot] : session_lobby.vec_slots)
            if (slot.AllowBots) {
              b_any_allow_bots = true;
              break;
            }
          if (!b_any_allow_bots) {
            bool b_any_open = false;
            for (const auto& [str_key, slot] : session_lobby.vec_slots)
              if (!slot.LockTeam &&
                  session_lobby.ClientInSlot(str_key) != nullptr) {
                b_any_open = true;
                break;
              }
            if (!b_any_open)
              return true;
          }
          return false;
        };

    ptr_slots_button->fn_on_mouse_down = [this,
                                          ptr_slots_button](
                                             const MouseInput&) {
      struct SlotsOption {
        std::string str_title;
        std::function<void()> fn_on_click;
      };

      const auto vec_bot_types = CollectBotTypes(ptr_map_);
      const auto& session_lobby = ptr_order_manager_->LobbyInfo();

      std::vector<std::pair<std::string, std::vector<SlotsOption>>>
          vec_groups;

      bool b_any_allow_bots = false;
      for (const auto& [str_key, slot] : session_lobby.vec_slots)
        if (slot.AllowBots) {
          b_any_allow_bots = true;
          break;
        }

      const net::SessionClient* ptr_bot_controller = nullptr;
      for (const auto& client : session_lobby.vec_clients)
        if (client.IsAdmin) {
          ptr_bot_controller =
              session_lobby.ClientWithIndex(client.Index);
          break;
        }

      if (!vec_bot_types.empty() && b_any_allow_bots) {
        std::vector<SlotsOption> vec_bot_options;
        vec_bot_options.push_back(SlotsOption{
            Msg("options-slot-admin.add-bots"),
            [this, vec_bot_types, ptr_bot_controller] {
              for (const auto& [str_key, slot] :
                   ptr_order_manager_->LobbyInfo().vec_slots) {
                std::size_t sz_bot = 0;
                if (auto* ptr_random =
                        ora::ui::Ui::Instance().CosmeticRandomDeps())
                  sz_bot = static_cast<std::size_t>(ptr_random->Next(
                      static_cast<int>(vec_bot_types.size())));
                const std::string str_bot = vec_bot_types[sz_bot].second;
                net::SessionClient* ptr_client_in_slot =
                    ptr_order_manager_->LobbyInfo().ClientInSlot(
                        str_key);
                if (slot.AllowBots &&
                    (ptr_client_in_slot == nullptr ||
                     !ptr_client_in_slot->Bot.empty()))
                  ptr_order_manager_->IssueOrder(
                      net::Order::Command(std::format(
                          "slot_bot {} {} {}", str_key,
                          ptr_bot_controller != nullptr
                              ? ptr_bot_controller->Index
                              : 0,
                          str_bot)));
              }
            }});

        bool b_any_bot_clients = false;
        for (const auto& client :
             ptr_order_manager_->LobbyInfo().vec_clients)
          if (!client.Bot.empty()) {
            b_any_bot_clients = true;
            break;
          }

        if (b_any_bot_clients) {
          vec_bot_options.push_back(SlotsOption{
              Msg("options-slot-admin.remove-bots"),
              [this] {
                for (const auto& [str_key, slot] :
                     ptr_order_manager_->LobbyInfo().vec_slots) {
                  net::SessionClient* ptr_client_in_slot =
                      ptr_order_manager_->LobbyInfo().ClientInSlot(
                          str_key);
                  if (ptr_client_in_slot != nullptr &&
                      !ptr_client_in_slot->Bot.empty())
                    ptr_order_manager_->IssueOrder(
                        net::Order::Command("slot_open " +
                                            slot.PlayerReference));
                }
              }});
        }

        vec_groups.emplace_back(Msg("options-slot-admin.configure-bots"),
                                std::move(vec_bot_options));
      }

      int int4_open_slots = 0;
      for (const auto& [str_key, slot] : session_lobby.vec_slots)
        if (!slot.LockTeam &&
            session_lobby.ClientInSlot(str_key) != nullptr)
          ++int4_open_slots;
      const int int4_team_count = (int4_open_slots + 1) / 2;

      if (int4_team_count >= 1) {
        std::vector<SlotsOption> vec_team_options;
        for (int int4_teams = int4_team_count; int4_teams >= 2;
             --int4_teams)
          vec_team_options.push_back(SlotsOption{
              MsgInt("options-slot-admin.teams-count", "count",
                     int4_teams),
              [this, int4_teams] {
                ptr_order_manager_->IssueOrder(net::Order::Command(
                    std::format("assignteams {}", int4_teams)));
              }});

        if (!vec_bot_types.empty() && b_any_allow_bots)
          vec_team_options.push_back(SlotsOption{
              Msg("options-slot-admin.humans-vs-bots"),
              [this] {
                ptr_order_manager_->IssueOrder(
                    net::Order::Command("assignteams 1"));
              }});

        vec_team_options.push_back(SlotsOption{
            Msg("options-slot-admin.free-for-all"),
            [this] {
              ptr_order_manager_->IssueOrder(
                  net::Order::Command("assignteams 0"));
            }});

        vec_groups.emplace_back(
            Msg("options-slot-admin.configure-teams"),
            std::move(vec_team_options));
      }

      ptr_slots_button->ShowDropDown<SlotsOption>(
          "LABEL_DROPDOWN_TEMPLATE", 175, vec_groups,
          [](const SlotsOption& option,
             ora::ui::ScrollItemWidget& item_template) {
            auto up_item = ora::ui::ScrollItemWidget::Setup(
                item_template, [] { return false; }, option.fn_on_click);
            up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                [str_title = option.str_title] { return str_title; };
            return up_item;
          });
    };
  }

  if (auto* ptr_reset_button = widget.GetOrNull<ora::ui::ButtonWidget>(
          "RESET_OPTIONS_BUTTON")) {
    ptr_reset_button->fn_is_visible = [this] {
      return kind_panel_ == PanelType::Options;
    };
    ptr_reset_button->fn_is_disabled =
        [this, fn_configuration_disabled]() mutable {
          return fn_configuration_disabled() ||
                 !b_reset_options_button_enabled_;
        };
    ptr_reset_button->fn_on_mouse_down = [this](const MouseInput&) {
      ptr_order_manager_->IssueOrder(
          net::Order::Command("reset_options"));
    };
  }

  ora::ui::WidgetArgs args_options;
  args_options.Add("orderManager", ptr_order_manager_);
  args_options.Add(
      "getMap",
      std::function<map::MapPreview*()>([this] { return ptr_map_; }));
  args_options.Add("configurationDisabled",
                   std::function<bool()>{fn_configuration_disabled});
  ora::ui::Widget* ptr_options_bin =
      ora::ui::Ui::Instance().LoadWidget(
          "LOBBY_OPTIONS_BIN", widget.Get("TOP_PANELS_ROOT"),
          std::move(args_options));
  ptr_options_bin->fn_is_visible = [this] {
    return kind_panel_ == PanelType::Options;
  };

  ora::ui::WidgetArgs args_music;
  args_music.Add("onExit", std::function<void()>{[] {}});
  if (ptr_world_renderer_ != nullptr)
    args_music.Add("world", &ptr_world_renderer_->World());
  ora::ui::Widget* ptr_music_bin = ora::ui::Ui::Instance().LoadWidget(
      "LOBBY_MUSIC_BIN", widget.Get("TOP_PANELS_ROOT"),
      std::move(args_music));
  ptr_music_bin->fn_is_visible = [this] {
    return kind_panel_ == PanelType::Music;
  };

  ServerListLogic* ptr_server_list_logic = nullptr;
  if (!b_skirmish_mode_) {
    ora::ui::WidgetArgs args_servers;
    args_servers.Add(
        "onJoin",
        std::function<void(server::GameServerData&)>{
            [](server::GameServerData&) {}});
    ora::ui::Widget* ptr_servers_bin =
        ora::ui::Ui::Instance().LoadWidget(
            "LOBBY_SERVERS_BIN", widget.Get("TOP_PANELS_ROOT"),
            std::move(args_servers));
    ptr_servers_bin->fn_is_visible = [this] {
      return kind_panel_ == PanelType::Servers;
    };

    for (const auto& up_logic : ptr_servers_bin->LogicObjects())
      if (auto* ptr_logic = dynamic_cast<ServerListLogic*>(up_logic.get()))
        ptr_server_list_logic = ptr_logic;
  }

  ora::ui::Widget* ptr_tab_container = widget.Get(
      b_skirmish_mode_ ? "SKIRMISH_TABS" : "MULTIPLAYER_TABS");
  ptr_tab_container->fn_is_visible = [] { return true; };

  auto& button_options_tab =
      *ptr_tab_container->Get<ora::ui::ButtonWidget>("OPTIONS_TAB");
  button_options_tab.fn_is_highlighted = [this] {
    return kind_panel_ == PanelType::Options;
  };
  button_options_tab.fn_is_disabled = [this] {
    return OptionsTabDisabled();
  };
  button_options_tab.fn_on_click = [this] {
    kind_panel_ = PanelType::Options;
  };

  auto& button_players_tab =
      *ptr_tab_container->Get<ora::ui::ButtonWidget>("PLAYERS_TAB");
  button_players_tab.fn_is_highlighted = [this] {
    return kind_panel_ == PanelType::Players;
  };
  button_players_tab.fn_is_disabled = [this] {
    return kind_panel_ == PanelType::Kick ||
           kind_panel_ == PanelType::ForceStart;
  };
  button_players_tab.fn_on_click = [this] {
    kind_panel_ = PanelType::Players;
  };

  auto& button_music_tab =
      *ptr_tab_container->Get<ora::ui::ButtonWidget>("MUSIC_TAB");
  button_music_tab.fn_is_highlighted = [this] {
    return kind_panel_ == PanelType::Music;
  };
  button_music_tab.fn_is_disabled = [this] {
    return kind_panel_ == PanelType::Kick ||
           kind_panel_ == PanelType::ForceStart;
  };
  button_music_tab.fn_on_click = [this] { kind_panel_ = PanelType::Music; };

  if (auto* ptr_servers_tab =
          ptr_tab_container->GetOrNull<ora::ui::ButtonWidget>(
              "SERVERS_TAB")) {
    ptr_servers_tab->fn_is_highlighted = [this] {
      return kind_panel_ == PanelType::Servers;
    };
    ptr_servers_tab->fn_is_disabled = [this] {
      return kind_panel_ == PanelType::Kick ||
             kind_panel_ == PanelType::ForceStart;
    };
    ptr_servers_tab->fn_on_click =
        [this, ptr_server_list_logic]() mutable {
          if (ptr_server_list_logic != nullptr &&
              kind_panel_ != PanelType::Servers)
            ptr_server_list_logic->RefreshServerList();

          kind_panel_ = PanelType::Servers;
        };
  }

  auto fn_start_game_action = [this]() {
    auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
    if (ptr_cache == nullptr)
      return;

    if (ptr_cache->At(ptr_map_->Uid()).Status() ==
        map::MapStatus::Available) {
      b_game_starting_ = true;
      ptr_order_manager_->IssueOrder(net::Order::Command("startgame"));
    } else {
      ptr_cache->UpdateMaps();
    }
  };

  if (auto* ptr_start_button = widget.GetOrNull<ora::ui::ButtonWidget>(
          "START_GAME_BUTTON")) {
    ptr_start_button->fn_is_disabled =
        [this, fn_configuration_disabled]() mutable {
          return fn_configuration_disabled() || StartDisabled();
        };

    ptr_start_button->fn_on_click =
        [this, fn_start_game_action]() mutable {
          bool b_any_unready = false;
          for (const auto& client :
               ptr_order_manager_->LobbyInfo().vec_clients)
            if (!client.Slot.empty() && !client.IsAdmin &&
                client.Bot.empty() && !client.IsReady()) {
              b_any_unready = true;
              break;
            }

          if (b_any_unready)
            kind_panel_ = PanelType::ForceStart;
          else
            fn_start_game_action();
        };
  }

  ora::ui::Widget* ptr_force_start_bin =
      ora::ui::Ui::Instance().LoadWidget(
          "FORCE_START_DIALOG", widget.Get("TOP_PANELS_ROOT"),
          ora::ui::WidgetArgs{});
  ptr_force_start_bin->fn_is_visible = [this] {
    return kind_panel_ == PanelType::ForceStart;
  };
  ptr_force_start_bin->Get("KICK_WARNING")->fn_is_visible = [this] {
    for (const auto& client :
         ptr_order_manager_->LobbyInfo().vec_clients)
      if (client.State == net::ClientState::Invalid)
        return true;
    return false;
  };
  auto& button_force_start =
      *ptr_force_start_bin->Get<ora::ui::ButtonWidget>("OK_BUTTON");
  button_force_start.fn_on_click = fn_start_game_action;
  button_force_start.fn_is_disabled = [this] { return StartDisabled(); };

  ptr_force_start_bin->Get<ora::ui::ButtonWidget>("CANCEL_BUTTON")
      ->fn_on_click = [this] { kind_panel_ = PanelType::Players; };

  auto& button_disconnect =
      *widget.Get<ora::ui::ButtonWidget>("DISCONNECT_BUTTON");
  button_disconnect.fn_on_click =
      [this, str_player_left_sound = str_player_left_sound_]() mutable {
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_exit_();
        ora::ui::Ui::Instance().PlayNotification(ptr_mod_rules_,
                                                  str_player_left_sound);
      };

  if (b_skirmish_mode_) {
    const std::string str_back = Msg("button-back");
    button_disconnect.fn_get_text = [str_back] { return str_back; };
  }

  if (ptr_logic_args != nullptr) {
    for (const auto& [str_key, ptr_yaml] : *ptr_logic_args) {
      if (str_key != "ChatTemplates" || ptr_yaml == nullptr)
        continue;
      for (const auto& node_item : ptr_yaml->Nodes) {
        const net::TextNotificationPool kind_pool =
            ParseTextNotificationPool(std::string{*node_item.Key});
        ora::ui::Widget* ptr_template =
            ora::ui::Ui::Instance().LoadWidget(
                node_item.Value.Value != nullptr
                    ? *node_item.Value.Value
                    : std::string{},
                nullptr, ora::ui::WidgetArgs{});
        map_chat_templates_[kind_pool].reset(ptr_template);
      }
    }
  }

  auto& button_chat_mode = *widget.Get<ora::ui::ButtonWidget>("CHAT_MODE");
  const std::string str_team = Msg("button-team-chat");
  const std::string str_all = Msg("button-general-chat");
  button_chat_mode.fn_get_text = [this, str_team, str_all] {
    return b_team_chat_ ? str_team : str_all;
  };
  button_chat_mode.fn_on_click = [this] { b_team_chat_ = !b_team_chat_; };
  button_chat_mode.fn_is_disabled = [this] {
    return b_disable_team_chat_ || !b_chat_enabled_;
  };

  ptr_chat_text_field_ =
      widget.Get<ora::ui::TextFieldWidget>("CHAT_TEXTFIELD");
  ptr_chat_text_field_->fn_is_disabled = [this] {
    return !b_chat_enabled_;
  };
  ptr_chat_text_field_->int4_max_length = net::kChatMessageMaxLength;

  ptr_chat_text_field_->fn_on_enter_key = [this](const KeyInput&) {
    if (ptr_chat_text_field_->Text().empty())
      return true;

    ptr_lobby_chat_panel_->ScrollToBottom();

    std::uint32_t uint4_team_number = 0;
    const net::SessionClient* ptr_local =
        ptr_order_manager_->LocalClient();
    if (b_team_chat_ && ptr_local != nullptr)
      uint4_team_number =
          ptr_local->IsObserver()
              ? std::numeric_limits<std::uint32_t>::max()
              : static_cast<std::uint32_t>(ptr_local->Team);

    ptr_order_manager_->IssueOrder(net::Order::Chat(
        ptr_chat_text_field_->Text(), uint4_team_number));
    ptr_chat_text_field_->SetText("");
    return true;
  };

  ptr_chat_text_field_->fn_on_tab_key =
      [this, &button_chat_mode](const KeyInput& e_input) {
        if (!button_chat_mode.key_hotkey.IsActivatedBy(e_input) ||
            button_chat_mode.fn_is_disabled()) {
          const std::string str_completed = CompleteTabCompletion(
              state_tab_, ptr_chat_text_field_->Text());
          ptr_chat_text_field_->SetText(str_completed);
          ptr_chat_text_field_->int4_cursor_position =
              static_cast<int>(ptr_chat_text_field_->Text().size());
        } else {
          button_chat_mode.fn_on_key_press(e_input);
        }

        return true;
      };

  ptr_chat_text_field_->fn_on_esc_key = [this](const KeyInput&) {
    ptr_chat_text_field_->YieldKeyboardFocus();
    return true;
  };

  transform_chat_available_in_.emplace([](int int4_seconds) {
    return MsgInt("label-chat-availability", "seconds", int4_seconds);
  });
  str_chat_disabled_ = Msg("label-chat-disabled");

  ptr_lobby_chat_panel_ =
      widget.Get<ora::ui::ScrollPanelWidget>("CHAT_DISPLAY");
  ptr_lobby_chat_panel_->RemoveChildren();

  if (auto* ptr_settings_button = widget.GetOrNull<ora::ui::ButtonWidget>(
          "SETTINGS_BUTTON")) {
    ptr_settings_button->fn_on_click = [this]() {
      ora::ui::WidgetArgs args_settings;
      args_settings.Add("onExit", std::function<void()>{[] {}});
      args_settings.Add("worldRenderer", ptr_world_renderer_);
      ora::ui::Ui::Instance().OpenWindow("SETTINGS_PANEL",
                                         std::move(args_settings));
    };
  }

  if (ptr_logic_args != nullptr) {
    const auto fn_override_sound = [](const ora::ui::YamlDictionary&
                                          dict_args,
                                      std::string_view str_key,
                                      std::string& str_out) {
      for (const auto& [str_arg_key, ptr_yaml] : dict_args) {
        if (str_arg_key != str_key || ptr_yaml == nullptr)
          continue;
        if (ptr_yaml->Value != nullptr)
          str_out = *ptr_yaml->Value;
      }
    };
    fn_override_sound(*ptr_logic_args, "ChatLineSound",
                      str_chat_line_sound_);
    fn_override_sound(*ptr_logic_args, "PlayerJoinedSound",
                      str_player_joined_sound_);
    fn_override_sound(*ptr_logic_args, "PlayerLeftSound",
                      str_player_left_sound_);
    fn_override_sound(*ptr_logic_args, "LobbyOptionChangedSound",
                      str_lobby_option_changed_sound_);
  }

  ora::ui::Ui::Instance().PlayNotification(ptr_mod_rules_,
                                           str_player_joined_sound_);
}

LobbyLogic::~LobbyLogic() {
  for (auto it_unsub = vec_unsubscribers_.rbegin();
       it_unsub != vec_unsubscribers_.rend(); ++it_unsub) {
    if (*it_unsub)
      (*it_unsub)();
  }
}

bool LobbyLogic::OptionsTabDisabled() const {
  return ptr_map_->Status() == map::MapStatus::Unavailable ||
         ptr_map_->Status() == map::MapStatus::Searching ||
         !MapIsPlayable() || kind_panel_ == PanelType::Kick ||
         kind_panel_ == PanelType::ForceStart;
}

bool LobbyLogic::StartDisabled() const {
  if (ptr_map_->Status() != map::MapStatus::Available)
    return true;

  const auto& session_lobby = ptr_order_manager_->LobbyInfo();
  for (const auto& [str_key, slot] : session_lobby.vec_slots)
    if (slot.Required && session_lobby.ClientInSlot(str_key) == nullptr)
      return true;

  bool b_any_client = false;
  for (const auto& [str_key, slot] : session_lobby.vec_slots)
    if (session_lobby.ClientInSlot(str_key) != nullptr) {
      b_any_client = true;
      break;
    }
  if (!b_any_client)
    return true;

  if (!session_lobby.global_settings.EnableSingleplayer) {
    std::size_t sz_non_bot_players = 0;
    for (const auto& client : session_lobby.vec_clients)
      if (!client.IsBot() && !client.Slot.empty())
        ++sz_non_bot_players;
    if (sz_non_bot_players < 2)
      return true;
  }

  return b_insufficient_player_spawns_;
}

void LobbyLogic::StartGameAction() {
  auto* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache == nullptr)
    return;

  if (ptr_cache->At(ptr_map_->Uid()).Status() ==
      map::MapStatus::Available) {
    b_game_starting_ = true;
    ptr_order_manager_->IssueOrder(net::Order::Command("startgame"));
  } else {
    ptr_cache->UpdateMaps();
  }
}

void LobbyLogic::Tick() {
  if (!b_map_available_ &&
      ptr_map_->Status() == map::MapStatus::Available) {
    b_map_available_ = true;
    CurrentMapBecameAvailable();
  }

  if (kind_panel_ == PanelType::Options && OptionsTabDisabled())
    kind_panel_ = PanelType::Players;

  const bool b_chat_was_enabled = b_chat_enabled_;
  const std::int64_t int8_disabled_until =
      net::TextNotificationsManager::Instance().ChatDisabledUntil();
  const bool b_world_replay =
      ptr_world_renderer_ != nullptr &&
      ptr_world_renderer_->World().IsReplay();
  b_chat_enabled_ =
      b_world_replay ||
      (ora::ui::Ui::Instance().RunTime() >= int8_disabled_until &&
       int8_disabled_until !=
           std::numeric_limits<std::int64_t>::max());

  if (b_chat_enabled_ && !b_chat_was_enabled) {
    ptr_chat_text_field_->SetText("");
    if (ora::ui::Ui::Instance().KeyboardFocusWidget() == nullptr)
      ptr_chat_text_field_->TakeKeyboardFocus();
  } else if (!b_chat_enabled_) {
    int int4_remaining = 0;
    if (int8_disabled_until !=
        std::numeric_limits<std::int64_t>::max())
      int4_remaining = static_cast<int>(
          (int8_disabled_until -
           ora::ui::Ui::Instance().RunTime() + 999) /
          1000);

    ptr_chat_text_field_->SetText(
        int4_remaining == 0
            ? str_chat_disabled_
            : transform_chat_available_in_->Update(int4_remaining));
  }
}

void LobbyLogic::Handle(const net::TextNotification& notification) {
  const auto it_template = map_chat_templates_.find(notification.Pool());
  if (it_template == map_chat_templates_.end())
    return;

  auto up_chat_line = it_template->second->Clone();
  ora::ui::SetupTextNotification(
      *up_chat_line, notification,
      ptr_lobby_chat_panel_->bounds_widget.int4_width -
          ptr_lobby_chat_panel_->int4_scrollbar_width,
      true);

  const bool b_scrolled_to_bottom =
      ptr_lobby_chat_panel_->ScrolledToBottom();
  ora::ui::Widget* ptr_chat_line = up_chat_line.get();
  ptr_lobby_chat_panel_->AddChild(std::move(up_chat_line));
  if (b_scrolled_to_bottom)
    ptr_lobby_chat_panel_->ScrollToBottom(true);

  switch (notification.Pool()) {
    case net::TextNotificationPool::Chat: {
      auto* ptr_profile_tooltip =
          ptr_chat_line
              ->GetOrNull<ora::ui::ClientTooltipRegionWidget>(
                  "PROFILE_TOOLTIP");
      auto* ptr_prefix = ptr_chat_line->GetOrNull("PREFIX");
      auto* ptr_client = ptr_order_manager_->LobbyInfo().ClientWithIndex(
          notification.ClientId());
      if (ptr_profile_tooltip != nullptr && ptr_prefix != nullptr &&
          ptr_client != nullptr) {
        ptr_profile_tooltip->bounds_widget = ptr_prefix->bounds_widget;
        if (!ptr_client->Fingerprint.empty())
          ptr_profile_tooltip->str_template =
              "REGISTERED_PLAYER_TOOLTIP";

        ptr_profile_tooltip->fn_is_visible = [] { return true; };
        ptr_profile_tooltip->Bind(ptr_order_manager_,
                                  ptr_world_renderer_, ptr_client);
      }

      ora::ui::Ui::Instance().PlayNotification(ptr_mod_rules_,
                                               str_chat_line_sound_);
      break;
    }

    case net::TextNotificationPool::System:
      ora::ui::Ui::Instance().PlayNotification(
          ptr_mod_rules_, str_lobby_option_changed_sound_);
      break;
    case net::TextNotificationPool::Join:
      ora::ui::Ui::Instance().PlayNotification(ptr_mod_rules_,
                                               str_player_joined_sound_);
      break;
    case net::TextNotificationPool::Leave:
      ora::ui::Ui::Instance().PlayNotification(ptr_mod_rules_,
                                               str_player_left_sound_);
      break;
    default:
      break;
  }
}

void LobbyLogic::CurrentMapBecameAvailable() {
  ptr_order_manager_->IssueOrder(net::Order::Command(
      std::format("state {}",
                  static_cast<int>(net::ClientState::NotReady))));

  if (const auto ptr_world_actor_info = ptr_map_->WorldActorInfo()) {
    const meta::RecordObject* rec_mission = nullptr;
    for (const meta::RecordObject* rec :
         ptr_world_actor_info->TraitInfosByInterface("MissionDataInfo"))
      if (rec_mission == nullptr)
        rec_mission = rec;
    if (rec_mission != nullptr) {
      const auto sv_briefing =
          sim::RecordFieldString(*rec_mission, "Briefing");
      if (sv_briefing.has_value() && !sv_briefing->empty()) {
        std::string str_briefing;
        if (!ptr_map_->TryGetMessage(*sv_briefing, str_briefing))
          str_briefing = std::string{*sv_briefing};
        net::TextNotificationsManager::Instance().AddSystemLine(
            str_briefing);
      }
    }
  }

  const auto vec_all_options = CollectMapLobbyOptions(*ptr_map_);
  const auto& map_new_options =
      ptr_order_manager_->LobbyInfo().global_settings.map_lobby_options;
  for (const auto& option : vec_all_options) {
    const auto it_new = map_new_options.find(option.Id);
    if (it_new == map_new_options.end())
      continue;

    const auto it_old = map_lobby_options_.find(option.Id);
    const bool b_changed =
        it_old != map_lobby_options_.end()
            ? it_new->second.Value != it_old->second.Value
            : it_new->second.Value != option.DefaultValue;
    if (b_changed)
      net::TextNotificationsManager::Instance().AddSystemLine(
          "notification-lobby-option",
          std::vector<net::FluentArgument>{
              {"name", std::string{option.Name}},
              {"value", option.Label(it_new->second.Value)}});
  }

  bool b_all_no_bots = true;
  for (const auto& [str_key, player_reference] :
       ptr_map_->Players().Players)
    if (player_reference.Playable && player_reference.AllowBots) {
      b_all_no_bots = false;
      break;
    }
  if (b_all_no_bots)
    net::TextNotificationsManager::Instance().AddSystemLine(
        "notification-map-bots-disabled");

  map_lobby_options_ = map_new_options;
}

void LobbyLogic::UpdateCurrentMap() {
  int4_map_status_ =
      ptr_order_manager_->LobbyInfo().global_settings.MapStatusFlags;
  const std::string& str_uid =
      ptr_order_manager_->LobbyInfo().global_settings.Map;
  if (ptr_map_->Uid() == str_uid)
    return;

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  ptr_map_ = ptr_cache != nullptr ? &ptr_cache->At(str_uid)
                                  : &map::MapCache::UnknownMap();
  if (ptr_map_->GenerationArgs().has_value())
    opt_last_generated_map_ = ptr_map_->GenerationArgs();

  b_map_available_ = ptr_map_->Status() == map::MapStatus::Available;
  if (b_map_available_) {
    CurrentMapBecameAvailable();
  } else if (ptr_map_->Status() != map::MapStatus::DownloadAvailable &&
             ora::ui::Ui::Instance().AllowDownloadingDeps() &&
             ptr_cache != nullptr) {
    ptr_cache->QueryRemoteMapDetails(
        game::WebServices{}.MapRepository,
        std::vector<std::string>{str_uid});
  }
}

void LobbyLogic::UpdatePlayerList() {
  const net::SessionClient* ptr_local =
      ptr_order_manager_->LocalClient();
  if (ptr_local == nullptr)
    return;

  if (ptr_local->Team == 0 && !ptr_local->IsObserver())
    b_disable_team_chat_ = true;
  else if (ptr_local->IsObserver()) {
    b_disable_team_chat_ = true;
    for (const auto& client : ptr_order_manager_->LobbyInfo().vec_clients)
      if (&client != ptr_local && client.IsObserver()) {
        b_disable_team_chat_ = false;
        break;
      }
  } else {
    b_disable_team_chat_ = true;
    for (const auto& client :
         ptr_order_manager_->LobbyInfo().vec_clients)
      if (&client != ptr_local && client.Bot.empty() &&
          client.Team == ptr_local->Team) {
        b_disable_team_chat_ = false;
        break;
      }
  }

  b_insufficient_player_spawns_ =
      InsufficientEnabledSpawnPoints(*ptr_map_,
                                     ptr_order_manager_->LobbyInfo());

  if (b_disable_team_chat_)
    b_team_chat_ = false;

  const bool b_is_host = ora::ui::Ui::Instance().IsHostDeps();
  std::size_t sz_idx = 0;

  auto fn_take_template =
      [this, &sz_idx](
          const std::unique_ptr<ora::ui::Widget>& up_template)
      -> std::unique_ptr<ora::ui::Widget> {
    const auto vec_children = ptr_players_->Children();
    ora::ui::Widget* ptr_existing =
        sz_idx < vec_children.size() ? vec_children[sz_idx] : nullptr;
    if (ptr_existing != nullptr &&
        ptr_existing->str_id == up_template->str_id)
      return nullptr;
    return up_template->Clone();
  };

  auto fn_place_row = [this, &sz_idx](std::unique_ptr<ora::ui::Widget>
                                          up_row) {
    if (up_row != nullptr) {
      if (sz_idx >= ptr_players_->Children().size()) {
        ptr_players_->AddChild(std::move(up_row));
      } else {
        const std::vector<ora::ui::Widget*> vec_children =
            ptr_players_->Children();
        if (vec_children[sz_idx]->str_id != up_row->str_id)
          ptr_players_->ReplaceChild(*vec_children[sz_idx],
                                     std::move(up_row));
      }
    }
    ++sz_idx;
  };

  for (const auto& [str_key, slot] :
       ptr_order_manager_->LobbyInfo().vec_slots) {
    net::SessionClient* ptr_client =
        ptr_order_manager_->LobbyInfo().ClientInSlot(str_key);

    std::unique_ptr<ora::ui::Widget> up_template;

    if (ptr_client == nullptr) {
      up_template = fn_take_template(up_empty_slot_template_);
      ora::ui::Widget& row =
          up_template != nullptr ? *up_template
                                 : *ptr_players_->Children()[sz_idx];

      if (b_is_host)
        SetupEditableSlotWidget(row, slot, ptr_client,
                                *ptr_order_manager_, *ptr_map_,
                                *ptr_mod_data_);
      else
        SetupSlotWidget(row, *ptr_mod_data_, slot, ptr_client);

      auto* ptr_button_join = row.Get<ora::ui::ButtonWidget>("JOIN");
      ptr_button_join->fn_is_visible = [&slot] { return !slot.Closed; };
      ptr_button_join->fn_is_disabled = [this] {
        return ptr_order_manager_->LocalClient()->IsReady();
      };
      ptr_button_join->fn_on_click = [this, str_key] {
        ptr_order_manager_->IssueOrder(
            net::Order::Command("slot " + str_key));
      };

      row.fn_is_visible = [] { return true; };
      fn_place_row(std::move(up_template));
      continue;
    }

    const bool b_editable =
        ptr_client->Index == ptr_local->Index ||
        (!ptr_client->Bot.empty() && b_is_host);
    if (b_editable) {
      up_template = fn_take_template(up_editable_player_template_);
      ora::ui::Widget& row =
          up_template != nullptr ? *up_template
                                 : *ptr_players_->Children()[sz_idx];

      SetupLatencyWidget(row, ptr_client, *ptr_order_manager_);

      if (!ptr_client->Bot.empty())
        SetupEditableSlotWidget(row, slot, ptr_client,
                                *ptr_order_manager_, *ptr_map_,
                                *ptr_mod_data_);
      else
        SetupEditableNameWidget(row, *ptr_client, *ptr_order_manager_,
                                ptr_world_renderer_);

      SetupEditableColorWidget(row, &slot, *ptr_client,
                               *ptr_order_manager_, ptr_world_renderer_);
      SetupEditableFactionWidget(row, slot, *ptr_client,
                                 *ptr_order_manager_, vec_factions_);
      SetupEditableTeamWidget(row, slot, *ptr_client,
                              *ptr_order_manager_, *ptr_map_);
      SetupEditableHandicapWidget(row, slot, *ptr_client,
                                  *ptr_order_manager_);
      SetupEditableSpawnWidget(row, slot, *ptr_client,
                               *ptr_order_manager_, *ptr_map_);
      SetupEditableReadyWidget(row, *ptr_client, *ptr_order_manager_,
                               *ptr_map_, MapIsPlayable());

      row.fn_is_visible = [] { return true; };
      fn_place_row(std::move(up_template));
      continue;
    }

    up_template = fn_take_template(up_non_editable_player_template_);
    ora::ui::Widget& row =
        up_template != nullptr ? *up_template
                               : *ptr_players_->Children()[sz_idx];

    SetupLatencyWidget(row, ptr_client, *ptr_order_manager_);
    SetupColorWidget(row, *ptr_client);
    SetupFactionWidget(row, *ptr_client, vec_factions_);

    if (b_is_host) {
      SetupEditableTeamWidget(row, slot, *ptr_client,
                              *ptr_order_manager_, *ptr_map_);
      SetupEditableHandicapWidget(row, slot, *ptr_client,
                                  *ptr_order_manager_);
      SetupEditableSpawnWidget(row, slot, *ptr_client,
                               *ptr_order_manager_, *ptr_map_);
      SetupPlayerActionWidget(
          row, *ptr_client, *ptr_order_manager_, ptr_world_renderer_,
          *ptr_lobby_, [this] { kind_panel_ = PanelType::Kick; },
          [this] { kind_panel_ = PanelType::Players; });
    } else {
      SetupNameWidget(row, *ptr_client, *ptr_order_manager_,
                      ptr_world_renderer_, *ptr_map_);
      SetupTeamWidget(row, *ptr_client);
      SetupHandicapWidget(row, *ptr_client);
      SetupSpawnWidget(row, *ptr_client);
    }

    SetupReadyWidget(row, *ptr_client);

    row.fn_is_visible = [] { return true; };
    fn_place_row(std::move(up_template));
  }

  for (auto& client : ptr_order_manager_->LobbyInfo().vec_clients) {
    if (!client.Slot.empty())
      continue;

    std::unique_ptr<ora::ui::Widget> up_template;
    if (client.Index == ptr_local->Index)
      up_template = fn_take_template(up_editable_spectator_template_);
    else
      up_template =
          fn_take_template(up_non_editable_spectator_template_);

    ora::ui::Widget& row =
        up_template != nullptr
            ? *up_template
            : *ptr_players_->Children()[sz_idx];

    if (client.Index == ptr_local->Index) {
      SetupEditableNameWidget(row, client, *ptr_order_manager_,
                              ptr_world_renderer_);

      if (client.IsAdmin)
        SetupEditableReadyWidget(row, client, *ptr_order_manager_,
                                 *ptr_map_, MapIsPlayable());
      else
        HideReadyWidgets(row);
    } else {
      if (b_is_host)
        SetupPlayerActionWidget(
            row, client, *ptr_order_manager_, ptr_world_renderer_,
            *ptr_lobby_, [this] { kind_panel_ = PanelType::Kick; },
            [this] { kind_panel_ = PanelType::Players; });
      else
        SetupNameWidget(row, client, *ptr_order_manager_,
                        ptr_world_renderer_, *ptr_map_);

      if (client.IsAdmin)
        SetupReadyWidget(row, client);
      else
        HideReadyWidgets(row);
    }

    SetupLatencyWidget(row, &client, *ptr_order_manager_);
    row.fn_is_visible = [] { return true; };
    fn_place_row(std::move(up_template));
  }

  if (!ptr_order_manager_->LocalClient()->Slot.empty()) {
    auto up_spec = fn_take_template(up_new_spectator_template_);
    ora::ui::Widget& row =
        up_spec != nullptr ? *up_spec
                           : *ptr_players_->Children()[sz_idx];

    SetupKickSpectatorsWidget(
        row, *ptr_order_manager_, *ptr_lobby_,
        [this] { kind_panel_ = PanelType::Kick; },
        [this] { kind_panel_ = PanelType::Players; }, b_skirmish_mode_);

    auto* ptr_button_spectate = row.Get<ora::ui::ButtonWidget>("SPECTATE");
    ptr_button_spectate->fn_on_click = [this] {
      ptr_order_manager_->IssueOrder(net::Order::Command("spectate"));
    };
    ptr_button_spectate->fn_is_disabled = [this] {
      return ptr_order_manager_->LocalClient()->IsReady();
    };
    ptr_button_spectate->fn_is_visible = [this] {
      return ptr_order_manager_->LobbyInfo().global_settings
                 .AllowSpectators ||
             ptr_order_manager_->LocalClient()->IsAdmin;
    };

    row.fn_is_visible = [] { return true; };
    fn_place_row(std::move(up_spec));
  }

  while (ptr_players_->Children().size() > sz_idx) {
    std::unique_ptr<ora::ui::Widget> up_removed =
        ptr_players_->RemoveChild(*ptr_players_->Children()[sz_idx]);
  }

  state_tab_.opt_names = std::vector<std::string>{};
  for (const auto& client : ptr_order_manager_->LobbyInfo().vec_clients)
    if (!client.IsBot()) {
      bool b_duplicate = false;
      for (const auto& str_name : *state_tab_.opt_names)
        if (str_name == client.Name) {
          b_duplicate = true;
          break;
        }
      if (!b_duplicate)
        state_tab_.opt_names->push_back(client.Name);
    }
}

void LobbyLogic::UpdateSpawnOccupants() {
  map_spawn_occupants_.clear();
  for (const auto& client : ptr_order_manager_->LobbyInfo().vec_clients)
    if (client.SpawnPoint != 0)
      map_spawn_occupants_.emplace(
          client.SpawnPoint, ora::ui::SpawnOccupant{client});
}

void LobbyLogic::UpdateOptions() {
  b_game_starting_ = false;
  if (ptr_map_ == nullptr ||
      ptr_map_->WorldActorInfo() == nullptr)
    return;

  const auto& map_server_options =
      ptr_order_manager_->LobbyInfo().global_settings.map_lobby_options;

  auto vec_all_options = CollectMapLobbyOptions(*ptr_map_);
  std::vector<server::LobbyOptionData*> vec_options;
  for (auto& option : vec_all_options)
    if (option.IsVisible)
      vec_options.push_back(&option);
  std::stable_sort(vec_options.begin(), vec_options.end(),
                   [](const server::LobbyOptionData* ptr_a,
                      const server::LobbyOptionData* ptr_b) {
                     return ptr_a->DisplayOrder < ptr_b->DisplayOrder;
                   });

  for (const auto* ptr_option : vec_options) {
    std::string str_value = ptr_option->DefaultValue;
    const auto it_old = map_lobby_options_.find(ptr_option->Id);
    if (it_old != map_lobby_options_.end())
      str_value = it_old->second.Value;

    std::string str_updated_value = ptr_option->DefaultValue;
    const auto it_updated = map_server_options.find(ptr_option->Id);
    if (it_updated != map_server_options.end())
      str_updated_value = it_updated->second.Value;

    if (str_updated_value != str_value)
      net::TextNotificationsManager::Instance().AddSystemLine(
          "notification-lobby-option-changed",
          std::vector<net::FluentArgument>{
              {"name", std::string{ptr_option->Name}},
              {"value", ptr_option->Label(str_updated_value)}});
  }

  map_lobby_options_ = map_server_options;

  b_reset_options_button_enabled_ = false;
  for (const auto* ptr_option : vec_options) {
    const auto it_server = map_server_options.find(ptr_option->Id);
    if (it_server != map_server_options.end() &&
        ptr_option->DefaultValue != it_server->second.Value) {
      b_reset_options_button_enabled_ = true;
      break;
    }
  }
}

void LobbyLogic::OnGameStart() {
  ora::ui::Ui::Instance().CloseWindow();
  fn_on_start_();
}

void RegisterLobbyLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();

  registry_logics.Register(
      "MapPreviewLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        auto fn_get_map = std::get<
            std::function<std::pair<map::MapPreview*, int>()>>(
            args_args.At("getMap"));

        std::function<void(ora::ui::MapPreviewWidget*, map::MapPreview*,
                           const MouseInput&)>
            fn_on_mouse_down;
        if (const auto* ptr_value = std::get_if<
                std::function<void(ora::ui::MapPreviewWidget*,
                                   map::MapPreview*, const MouseInput&)>>(
                &args_args.At("onMouseDown")))
          fn_on_mouse_down = *ptr_value;

        std::function<std::map<int, ora::ui::SpawnOccupant>()>
            fn_get_spawn_occupants;
        if (const auto* ptr_value = std::get_if<
                std::function<std::map<int, ora::ui::SpawnOccupant>()>>(
                &args_args.At("getSpawnOccupants")))
          fn_get_spawn_occupants = *ptr_value;

        bool b_map_updates_enabled = false;
        if (const auto* ptr_value =
                std::get_if<bool>(&args_args.At("mapUpdatesEnabled")))
          b_map_updates_enabled = *ptr_value;

        std::function<void(const std::string&)> fn_on_map_update;
        if (const auto* ptr_value =
                std::get_if<std::function<void(const std::string&)>>(
                    &args_args.At("onMapUpdate")))
          fn_on_map_update = *ptr_value;

        std::function<std::vector<int>()> fn_get_disabled_spawn_points;
        if (const auto* ptr_value = std::get_if<
                std::function<std::vector<int>()>>(
                &args_args.At("getDisabledSpawnPoints")))
          fn_get_disabled_spawn_points = *ptr_value;

        bool b_show_unoccupied = false;
        if (const auto* ptr_value = std::get_if<bool>(
                &args_args.At("showUnoccupiedSpawnpoints")))
          b_show_unoccupied = *ptr_value;

        return std::make_unique<MapPreviewLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::move(fn_get_map), std::move(fn_on_mouse_down),
            std::move(fn_get_spawn_occupants), b_map_updates_enabled,
            std::move(fn_on_map_update),
            std::move(fn_get_disabled_spawn_points),
            b_show_unoccupied);
      });

  registry_logics.Register(
      "LobbyOptionsLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<LobbyOptionsLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<net::OrderManager*>(args_args.At("orderManager")),
            std::get<ora::ui::GetMapFn>(args_args.At("getMap")),
            std::get<std::function<bool()>>(
                args_args.At("configurationDisabled")));
      });

  registry_logics.Register(
      "LobbyLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<LobbyLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::get<gfx::WorldRenderer*>(args_args.At("worldRenderer")),
            *std::get<net::OrderManager*>(args_args.At("orderManager")),
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::get<std::function<void()>>(args_args.At("onStart")),
            std::get<bool>(args_args.At("skirmishMode")),
            std::get<const ora::ui::YamlDictionary*>(
                args_args.At("logicArgs")));
      });
}

}
