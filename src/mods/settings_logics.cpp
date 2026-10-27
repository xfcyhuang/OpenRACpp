#include "mods/settings_logics.hpp"

#include "net/sanitize.hpp"

#include "core/cached_transform.hpp"
#include "fluent/fluent_provider.hpp"
#include "game/local_player_profile.hpp"
#include "game/settings.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "mods/music_playlist.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/hotkey_entry_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/password_field_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::mods {

namespace {

using fluent::FluentValue;

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string MsgArg(std::string_view str_key, std::string_view str_arg_name,
                   FluentValue val_arg) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, FluentValue>>{
          {std::string{str_arg_name}, std::move(val_arg)}});
}

std::string MsgArg2(std::string_view str_key, std::string_view str_arg_a,
                    FluentValue val_a, std::string_view str_arg_b,
                    FluentValue val_b) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, FluentValue>>{
          {std::string{str_arg_a}, std::move(val_a)},
          {std::string{str_arg_b}, std::move(val_b)}});
}

constexpr std::string_view kSettingsSaveTitle = "dialog-settings-save.title";
constexpr std::string_view kSettingsSavePrompt = "dialog-settings-save.prompt";
constexpr std::string_view kSettingsSaveCancel = "dialog-settings-save.cancel";
constexpr std::string_view kRestartTitle = "dialog-settings-restart.title";
constexpr std::string_view kRestartPrompt = "dialog-settings-restart.prompt";
constexpr std::string_view kRestartAccept = "dialog-settings-restart.confirm";
constexpr std::string_view kRestartCancel = "dialog-settings-restart.cancel";
constexpr std::string_view kResetTitle = "dialog-settings-reset.title";
constexpr std::string_view kResetPrompt = "dialog-settings-reset.prompt";
constexpr std::string_view kResetAccept = "dialog-settings-reset.confirm";
constexpr std::string_view kResetCancel = "dialog-settings-reset.cancel";

constexpr std::string_view kClose = "options-camera.close";
constexpr std::string_view kMedium = "options-camera.medium";
constexpr std::string_view kFar = "options-camera.far";
constexpr std::string_view kFurthest = "options-camera.furthest";
constexpr std::string_view kWindowed = "options-display-mode.windowed";
constexpr std::string_view kLegacyFullscreen =
    "options-display-mode.legacy-fullscreen";
constexpr std::string_view kFullscreen = "options-display-mode.fullscreen";
constexpr std::string_view kDisplayIndex = "label-video-display-index";
constexpr std::string_view kSelectPreset =
    "dropdownbutton-resolution-select-preset";
constexpr std::string_view kStandard = "options-status-bars.standard";
constexpr std::string_view kShowOnDamage = "options-status-bars.show-on-damage";
constexpr std::string_view kAlwaysShow = "options-status-bars.always-show";
constexpr std::string_view kAutomatic = "options-target-lines.automatic";
constexpr std::string_view kManual = "options-target-lines.manual";
constexpr std::string_view kDisabled = "options-target-lines.disabled";
constexpr std::string_view kFrameLimiter = "checkbox-frame-limiter";
constexpr std::string_view kClassic = "options-control-scheme.classic";
constexpr std::string_view kModern = "options-control-scheme.modern";
constexpr std::string_view kOtherRTS = "options-control-scheme.otherrts";
constexpr std::string_view kScrollDisabled =
    "options-mouse-scroll-type.disabled";
constexpr std::string_view kScrollStandard =
    "options-mouse-scroll-type.standard";
constexpr std::string_view kScrollInverted =
    "options-mouse-scroll-type.inverted";
constexpr std::string_view kScrollJoystick = "options-mouse-scroll-type.joystick";

constexpr std::string_view kButtonForumProfileLink =
    "button-forum-profile-link";
constexpr std::string_view kButtonForumProfileUnlink =
    "button-forum-profile-unlink";
constexpr std::string_view kLabelForumProfileResultUnlinked =
    "label-forum-profile-result-unlinked";
constexpr std::string_view kLabelForumProfileResultLinked =
    "label-forum-profile-result-linked";
constexpr std::string_view kLabelForumProfileResultAuthFailure =
    "label-forum-profile-result-auth-failure";
constexpr std::string_view kLabelForumProfileResultLoginAttempts =
    "label-forum-profile-result-login-attempts";
constexpr std::string_view kLabelForumProfileResultBanned =
    "label-forum-profile-result-banned";
constexpr std::string_view kLabelForumProfileResultConnectionFailed =
    "label-forum-profile-result-connection-failed";
constexpr std::string_view kLabelForumProfileResultError =
    "label-forum-profile-result-error";
constexpr std::string_view kAutoSaveIntervalOptions =
    "auto-save-interval.options";
constexpr std::string_view kAutoSaveIntervalDisabled =
    "auto-save-interval.disabled";
constexpr std::string_view kAutoSaveIntervalMinuteOptions =
    "auto-save-interval.minute-options";
constexpr std::string_view kAutoSaveMaxFileNumber =
    "auto-save-max-file-number";

constexpr std::string_view kOriginalNotice = "label-original-notice";
constexpr std::string_view kDuplicateNotice = "label-duplicate-notice";

constexpr std::array<std::pair<int, int>, 9> kCommonResolutions{{
    {1024, 720},
    {1024, 768},
    {1280, 720},
    {1366, 768},
    {1440, 900},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
}};

constexpr std::array<int, 9> kAutoSaveSeconds{0, 10, 30, 45, 60, 120, 180,
                                              300, 600};
constexpr std::array<int, 6> kAutoSaveFileNumbers{3, 5, 10, 20, 50, 100};

meta::GeneratedRecord& OriginalSoundSnapshot() {
  static meta::GeneratedRecord rec_original =
      game::Settings::Instance().Module("Sound")->CloneRecord();
  return rec_original;
}

meta::GeneratedRecord& OriginalGraphicSnapshot() {
  static meta::GeneratedRecord rec_original =
      game::Settings::Instance().Module("Graphics")->CloneRecord();
  return rec_original;
}

std::optional<bool>& OriginalServerDiscoverNat() {
  static std::optional<bool> b_original;
  return b_original;
}

std::string GetTypeName(const game::SettingsModule& module_settings) {
  return std::string{
      module_settings.Record().record_desc().str_name};
}

std::size_t SlotOf(const game::SettingsModule& module_settings,
                   std::string_view str_pref) {
  const std::optional<std::size_t> opt_slot =
      module_settings.SlotIndexOf(str_pref);
  if (!opt_slot)
    throw std::runtime_error(
        std::format("{} does not contain a preference type {}",
                    GetTypeName(module_settings), str_pref));
  return *opt_slot;
}

std::string NumberToText(int int4_value) {
  return std::to_string(int4_value);
}

std::optional<int> TextToNumber(const std::string& str_text) {
  if (str_text.empty())
    return std::nullopt;
  int int4_value = 0;
  const auto result_parse = std::from_chars(
      str_text.data(), str_text.data() + str_text.size(), int4_value);
  if (result_parse.ec != std::errc{} || result_parse.ptr != str_text.data() + str_text.size())
    return std::nullopt;
  return int4_value;
}

void AppendSliderOnChange(ora::ui::SliderWidget& slider_target,
                          std::function<void(float)> fn_extra) {
  std::function<void(float)> fn_old = slider_target.fn_on_change;
  slider_target.fn_on_change =
      [fn_old, fn_extra](float fp4_value) {
        if (fn_old)
          fn_old(fp4_value);
        fn_extra(fp4_value);
      };
}

const std::string& SnapshotString(const meta::GeneratedRecord& rec_snapshot,
                                  std::size_t sz_slot) {
  return std::get<std::string>(rec_snapshot.Slot(sz_slot).val);
}

std::int64_t SnapshotInt(const meta::GeneratedRecord& rec_snapshot,
                         std::size_t sz_slot) {
  return std::get<std::int64_t>(rec_snapshot.Slot(sz_slot).val);
}

bool SnapshotBool(const meta::GeneratedRecord& rec_snapshot,
                  std::size_t sz_slot) {
  return std::get<bool>(rec_snapshot.Slot(sz_slot).val);
}

std::string GetAutoSaveIntervalText(int int4_value) {
  if (int4_value == 0)
    return Msg(kAutoSaveIntervalDisabled);
  if (int4_value < 60)
    return MsgArg(kAutoSaveIntervalOptions, "seconds",
                  FluentValue{static_cast<std::int64_t>(int4_value)});
  return MsgArg(kAutoSaveIntervalMinuteOptions, "minutes",
                FluentValue{static_cast<std::int64_t>(int4_value / 60)});
}

bool IsNullOrSpace(const std::string& str_text) {
  for (const char c_char : str_text)
    if (c_char != ' ' && c_char != '\t' && c_char != '\r' && c_char != '\n')
      return false;
  return true;
}

bool ContainsIgnoreCase(const std::string& str_haystack,
                        const std::string& str_needle) {
  if (str_needle.empty())
    return true;
  if (str_haystack.size() < str_needle.size())
    return false;
  const auto fn_fold = [](char c_char) {
    return c_char >= 'A' && c_char <= 'Z'
               ? static_cast<char>(c_char - 'A' + 'a')
               : c_char;
  };
  for (std::size_t sz_index = 0;
       sz_index + str_needle.size() <= str_haystack.size(); ++sz_index) {
    bool b_match = true;
    for (std::size_t sz_offset = 0; sz_offset < str_needle.size();
         ++sz_offset)
      if (fn_fold(str_haystack[sz_index + sz_offset]) !=
          fn_fold(str_needle[sz_offset])) {
        b_match = false;
        break;
      }
    if (b_match)
      return true;
  }
  return false;
}

bool OverlapsAny(const std::vector<std::string>& vec_a,
                 const std::vector<std::string>& vec_b) {
  for (const std::string& str_a : vec_a)
    if (std::find(vec_b.begin(), vec_b.end(), str_a) != vec_b.end())
      return true;
  return false;
}

std::vector<std::string_view> SplitCommaList(std::string_view sv_text) {
  std::vector<std::string_view> vec_parts;
  std::size_t sz_start = 0;
  while (sz_start <= sv_text.size()) {
    const std::size_t sz_comma = sv_text.find(',', sz_start);
    const std::size_t sz_end =
        sz_comma == std::string_view::npos ? sv_text.size() : sz_comma;
    std::string_view sv_part = sv_text.substr(sz_start, sz_end - sz_start);
    while (!sv_part.empty() &&
           (sv_part.front() == ' ' || sv_part.front() == '\t'))
      sv_part.remove_prefix(1);
    while (!sv_part.empty() &&
           (sv_part.back() == ' ' || sv_part.back() == '\t'))
      sv_part.remove_suffix(1);
    if (!sv_part.empty())
      vec_parts.push_back(sv_part);
    if (sz_comma == std::string_view::npos)
      break;
    sz_start = sz_comma + 1;
  }
  return vec_parts;
}

std::string GetContextDisplayName(const std::string& str_context) {
  if (str_context.empty())
    return Msg(std::string{ora::ui::HotkeyDefinition::kContextFluentPrefix} +
               "-any");
  return Msg(str_context);
}

void RecalculateWidgetLayout(ora::ui::Widget& widget_target,
                             bool b_inside_scroll_panel) {
  if (widget_target.str_id == "BADGES_CONTAINER" ||
      widget_target.str_id == "SETTINGS_TAB_CONTAINER")
    return;

  ora::ui::Widget* ptr_widget_parent = widget_target.ptr_parent;

  int2 int2_resolution{0, 0};
  if (ora::ui::Ui::HasInstance()) {
    if (gfx::Renderer* ptr_renderer_face =
            ora::ui::Ui::Instance().RendererDeps())
      int2_resolution = ptr_renderer_face->Resolution();
  }

  ora::ui::WidgetBounds bounds_parent;
  if (ptr_widget_parent == nullptr)
    bounds_parent = ora::ui::WidgetBounds{
        0, 0, int2_resolution.X, int2_resolution.Y};
  else
    bounds_parent = ptr_widget_parent->bounds_widget;

  std::vector<std::pair<std::string, std::int32_t>> vec_symbols{
      {"WINDOW_WIDTH", int2_resolution.X},
      {"WINDOW_HEIGHT", int2_resolution.Y},
      {"PARENT_WIDTH", bounds_parent.int4_width},
      {"PARENT_HEIGHT", bounds_parent.int4_height},
  };

  const auto fn_evaluate =
      [&vec_symbols](const std::optional<expr::IntegerExpression>& expr_optional) {
        return expr_optional.has_value()
                   ? expr_optional->Evaluate(vec_symbols)
                   : 0;
      };

  const std::int32_t int4_width = fn_evaluate(widget_target.expr_width);
  const std::int32_t int4_height = fn_evaluate(widget_target.expr_height);

  vec_symbols.emplace_back("WIDTH", int4_width);
  vec_symbols.emplace_back("HEIGHT", int4_height);

  if (b_inside_scroll_panel)
    widget_target.bounds_widget = ora::ui::WidgetBounds{
        widget_target.bounds_widget.int4_x, widget_target.bounds_widget.int4_y,
        int4_width, widget_target.bounds_widget.int4_height};
  else
    widget_target.bounds_widget = ora::ui::WidgetBounds{
        fn_evaluate(widget_target.expr_x), fn_evaluate(widget_target.expr_y),
        int4_width, int4_height};

  for (ora::ui::Widget* ptr_child : widget_target.Children())
    RecalculateWidgetLayout(
        *ptr_child,
        b_inside_scroll_panel ||
            dynamic_cast<ora::ui::ScrollPanelWidget*>(ptr_child) != nullptr);
}

void RecalculateWidgetLayoutRoot() {
  if (!ora::ui::Ui::HasInstance())
    return;
  ora::ui::Widget* ptr_root = ora::ui::Ui::Instance().Root();
  if (ptr_root != nullptr)
    RecalculateWidgetLayout(*ptr_root, false);
}

struct WindowModeNames {
  std::string str_windowed;
  std::string str_legacy_fullscreen;
  std::string str_fullscreen;
};

std::string MouseScrollName(std::int64_t int8_value) {
  switch (int8_value) {
    case 0:
      return "Disabled";
    case 1:
      return "Standard";
    case 2:
      return "Inverted";
    default:
      return "Joystick";
  }
}

std::string ModifierName(std::int64_t int8_value) {
  switch (int8_value) {
    case 1:
      return "Shift";
    case 2:
      return "Alt";
    case 4:
      return "Ctrl";
    case 8:
      return "Meta";
    default:
      return "None";
  }
}

std::string GLProfileName(std::int64_t int8_value) {
  switch (int8_value) {
    case 1:
      return "ANGLE";
    case 2:
      return "Modern";
    case 3:
      return "Embedded";
    default:
      return "Automatic";
  }
}

void ShowWindowModeDropdown(ora::ui::DropDownButtonWidget& dropdown,
                            game::SettingsModule& module_graphics,
                            ora::ui::ScrollPanelWidget& scroll_panel) {
  const std::size_t sz_slot = SlotOf(module_graphics, "Mode");
  const std::vector<std::pair<std::string, int>> vec_options{
      {Msg(kFullscreen), 2},
      {Msg(kLegacyFullscreen), 1},
      {Msg(kWindowed), 0}};

  dropdown.ShowDropDown<std::pair<std::string, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_options,
      [&module_graphics, sz_slot, &scroll_panel](
          const std::pair<std::string, int>& pair_option,
          ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_graphics, sz_slot, &pair_option] {
              return module_graphics.GetInt(sz_slot) == pair_option.second;
            },
            [&module_graphics, sz_slot, &pair_option, &scroll_panel] {
              module_graphics.SetInt(sz_slot, pair_option.second);
              AdjustSettingsScrollPanelLayout(scroll_panel);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.first;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowDisplaySelectionDropdown(
    ora::ui::DropDownButtonWidget& dropdown,
    game::SettingsModule& module_graphics) {
  const std::size_t sz_slot = SlotOf(module_graphics, "VideoDisplay");
  const int int4_display_count =
      ora::ui::Ui::HasInstance() &&
              ora::ui::Ui::Instance().RendererDeps() != nullptr
          ? ora::ui::Ui::Instance().RendererDeps()->DisplayCount()
          : 0;

  std::vector<int> vec_displays;
  for (int int4_index = 0; int4_index < int4_display_count; ++int4_index)
    vec_displays.push_back(int4_index);

  dropdown.ShowDropDown<int>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_displays,
      [&module_graphics, sz_slot](const int& int4_option,
                                  ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_graphics, sz_slot, int4_option] {
              return module_graphics.GetInt(sz_slot) == int4_option;
            },
            [&module_graphics, sz_slot, int4_option] {
              module_graphics.SetInt(sz_slot, int4_option);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label =
            std::format("Display {}", int4_option + 1);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowGLProfileDropdown(ora::ui::DropDownButtonWidget& dropdown,
                           game::SettingsModule& module_graphics) {
  const std::size_t sz_slot = SlotOf(module_graphics, "GLProfile");

  std::vector<int> vec_profiles{0};
  if (ora::ui::Ui::HasInstance()) {
    if (gfx::Renderer* ptr_renderer = ora::ui::Ui::Instance().RendererDeps()) {
      for (const platform::GLProfileKind kind_profile :
           ptr_renderer->SupportedGLProfiles())
        vec_profiles.push_back(
            kind_profile == platform::GLProfileKind::Modern ? 2 : 3);
    }
  }

  dropdown.ShowDropDown<int>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_profiles,
      [&module_graphics, sz_slot](const int& int4_option,
                                  ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_graphics, sz_slot, int4_option] {
              return module_graphics.GetInt(sz_slot) == int4_option;
            },
            [&module_graphics, sz_slot, int4_option] {
              module_graphics.SetInt(sz_slot, int4_option);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = GLProfileName(int4_option);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowStatusBarsDropdown(ora::ui::DropDownButtonWidget& dropdown,
                            game::SettingsModule& module_game) {
  const std::size_t sz_slot = SlotOf(module_game, "StatusBars");
  const std::vector<std::pair<std::string, int>> vec_options{
      {Msg(kStandard), 0},
      {Msg(kShowOnDamage), 1},
      {Msg(kAlwaysShow), 2}};

  dropdown.ShowDropDown<std::pair<std::string, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_options,
      [&module_game, sz_slot](const std::pair<std::string, int>& pair_option,
                              ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_game, sz_slot, &pair_option] {
              return module_game.GetInt(sz_slot) == pair_option.second;
            },
            [&module_game, sz_slot, &pair_option] {
              module_game.SetInt(sz_slot, pair_option.second);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.first;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowTargetLinesDropdown(ora::ui::DropDownButtonWidget& dropdown,
                             game::SettingsModule& module_game) {
  const std::size_t sz_slot = SlotOf(module_game, "TargetLines");
  const std::vector<std::pair<std::string, int>> vec_options{
      {Msg(kAutomatic), 2},
      {Msg(kManual), 1},
      {Msg(kDisabled), 0}};

  dropdown.ShowDropDown<std::pair<std::string, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_options,
      [&module_game, sz_slot](const std::pair<std::string, int>& pair_option,
                              ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_game, sz_slot, &pair_option] {
              return module_game.GetInt(sz_slot) == pair_option.second;
            },
            [&module_game, sz_slot, &pair_option] {
              module_game.SetInt(sz_slot, pair_option.second);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.first;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowResolutionPresetDropdown(
    ora::ui::DropDownButtonWidget& dropdown,
    ora::ui::TextFieldWidget* ptr_window_width,
    ora::ui::TextFieldWidget* ptr_window_height) {
  std::vector<std::pair<int, int>> vec_sorted_modes(
      kCommonResolutions.begin(), kCommonResolutions.end());
  std::sort(vec_sorted_modes.begin(), vec_sorted_modes.end(),
            [](const std::pair<int, int>& a_pair,
               const std::pair<int, int>& b_pair) {
              if (a_pair.first != b_pair.first)
                return a_pair.first < b_pair.first;
              return a_pair.second < b_pair.second;
            });

  dropdown.ShowDropDown<std::pair<int, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 300, vec_sorted_modes,
      [ptr_window_width, ptr_window_height](
          const std::pair<int, int>& pair_resolution,
          ora::ui::ScrollItemWidget& item_template) {
        const auto opt_width =
            ptr_window_width != nullptr
                ? TextToNumber(ptr_window_width->Text())
                : std::optional<int>{};
        const auto opt_height =
            ptr_window_height != nullptr
                ? TextToNumber(ptr_window_height->Text())
                : std::optional<int>{};
        const int int4_current_width = opt_width.value_or(0);
        const int int4_current_height = opt_height.value_or(0);

        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [int4_current_width, int4_current_height, &pair_resolution] {
              return int4_current_width == pair_resolution.first &&
                     int4_current_height == pair_resolution.second;
            },
            [ptr_window_width, ptr_window_height, &pair_resolution] {
              if (ptr_window_width != nullptr)
                ptr_window_width->SetText(
                    NumberToText(pair_resolution.first));
              if (ptr_window_height != nullptr)
                ptr_window_height->SetText(
                    NumberToText(pair_resolution.second));
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = std::format(
            "{}x{}", pair_resolution.first, pair_resolution.second);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void MakeMouseFocusSettingsLive(game::SettingsModule& module_game) {
  gfx::Renderer* ptr_renderer = ora::ui::Ui::Instance().RendererDeps();
  if (ptr_renderer == nullptr)
    return;
  if (module_game.GetBool(SlotOf(module_game, "LockMouseWindow")))
    ptr_renderer->GrabWindowMouseFocus();
  else
    ptr_renderer->ReleaseWindowMouseFocus();
}

void ShowMouseScrollDropdown(ora::ui::Widget& dropdown,
                             game::SettingsModule& module_game) {
  auto* ptr_dropdown = static_cast<ora::ui::DropDownButtonWidget*>(&dropdown);
  const std::size_t sz_slot = SlotOf(module_game, "MouseScroll");
  const std::vector<std::pair<std::string, int>> vec_options{
      {Msg(kScrollDisabled), 0},
      {Msg(kScrollStandard), 1},
      {Msg(kScrollInverted), 2},
      {Msg(kScrollJoystick), 3}};

  ptr_dropdown->ShowDropDown<std::pair<std::string, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_options,
      [&module_game, sz_slot](const std::pair<std::string, int>& pair_option,
                              ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_game, sz_slot, &pair_option] {
              return module_game.GetInt(sz_slot) == pair_option.second;
            },
            [&module_game, sz_slot, &pair_option] {
              module_game.SetInt(sz_slot, pair_option.second);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.first;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowZoomModifierDropdown(ora::ui::Widget& dropdown,
                              game::SettingsModule& module_game) {
  auto* ptr_dropdown = static_cast<ora::ui::DropDownButtonWidget*>(&dropdown);
  const std::size_t sz_slot = SlotOf(module_game, "ZoomModifier");
  const std::vector<std::pair<std::string, int>> vec_options{
      {ModifierName(2), 2},  {ModifierName(4), 4}, {ModifierName(8), 8},
      {ModifierName(1), 1},  {ModifierName(0), 0}};

  ptr_dropdown->ShowDropDown<std::pair<std::string, int>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_options,
      [&module_game, sz_slot](const std::pair<std::string, int>& pair_option,
                              ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_game, sz_slot, &pair_option] {
              return module_game.GetInt(sz_slot) == pair_option.second;
            },
            [&module_game, sz_slot, &pair_option] {
              module_game.SetInt(sz_slot, pair_option.second);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.first;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

}

void ResetSettingsPanelSnapshotsForTest() {
  game::Settings& settings_instance = game::Settings::Instance();
  if (game::SettingsModule* ptr_module = settings_instance.Module("Sound"))
    OriginalSoundSnapshot() = ptr_module->CloneRecord();
  if (game::SettingsModule* ptr_module = settings_instance.Module("Graphics"))
    OriginalGraphicSnapshot() = ptr_module->CloneRecord();
  OriginalServerDiscoverNat().reset();
}

void BindCheckboxPref(ora::ui::Widget& parent, std::string_view str_id,
                      game::SettingsModule& module_settings,
                      std::string_view str_pref) {
  const std::size_t sz_slot = SlotOf(module_settings, str_pref);
  auto* ptr_checkbox = parent.Get<ora::ui::CheckboxWidget>(str_id);
  ptr_checkbox->fn_is_checked = [&module_settings, sz_slot] {
    return module_settings.GetBool(sz_slot);
  };
  ptr_checkbox->fn_on_click = [&module_settings, sz_slot, ptr_checkbox] {
    module_settings.SetBool(sz_slot, !ptr_checkbox->fn_is_checked());
  };
}

void BindSliderPref(ora::ui::Widget& parent, std::string_view str_id,
                    game::SettingsModule& module_settings,
                    std::string_view str_pref) {
  const std::size_t sz_slot = SlotOf(module_settings, str_pref);
  auto* ptr_slider = parent.Get<ora::ui::SliderWidget>(str_id);
  ptr_slider->fp4_value = module_settings.GetFloat(sz_slot);
  ptr_slider->fn_on_change = [&module_settings, sz_slot](float fp4_value) {
    module_settings.SetFloat(sz_slot, fp4_value);
  };
}

void BindIntSliderPref(ora::ui::Widget& parent, std::string_view str_id,
                       game::SettingsModule& module_settings,
                       std::string_view str_pref) {
  const std::size_t sz_slot = SlotOf(module_settings, str_pref);
  auto* ptr_slider = parent.Get<ora::ui::SliderWidget>(str_id);
  ptr_slider->fp4_value =
      static_cast<float>(module_settings.GetInt(sz_slot));
  ptr_slider->fn_on_change = [&module_settings, sz_slot](float fp4_value) {
    module_settings.SetInt(sz_slot, static_cast<std::int64_t>(fp4_value));
  };
}

void AdjustSettingsScrollPanelLayout(ora::ui::ScrollPanelWidget& scroll_panel) {
  for (ora::ui::Widget* ptr_row : scroll_panel.Children()) {
    if (ptr_row->Children().empty())
      continue;

    bool b_has_visible_children = false;
    for (ora::ui::Widget* ptr_container : ptr_row->Children()) {
      if (ptr_container->fn_is_visible()) {
        b_has_visible_children = true;
        break;
      }
    }

    if (!b_has_visible_children)
      ptr_row->b_visible = false;
  }

  if (scroll_panel.ptr_layout != nullptr)
    scroll_panel.ptr_layout->AdjustChildren();
}

std::string GetViewportSizeName(game::ModData& mod_data, int int4_viewport) {
  (void)&mod_data;
  switch (int4_viewport) {
    case 1:
      return Msg(kClose);
    case 2:
      return Msg(kMedium);
    case 3:
      return Msg(kFar);
    case 0:
      return Msg(kFurthest);
    default:
      return "";
  }
}

void BindTextNotificationPoolFilterSettings(ora::ui::Widget& panel,
                                            game::SettingsModule& module_game) {
  const std::size_t sz_slot = SlotOf(module_game, "TextNotificationPoolFilters");
  const auto fn_toggle = [&module_game, sz_slot](std::int64_t int8_flag) {
    module_game.SetInt(sz_slot, module_game.GetInt(sz_slot) ^ int8_flag);
    ora::ui::Ui::Instance().SettingsSaveDeps();
  };

  if (auto* ptr_feedback =
          panel.GetOrNull<ora::ui::CheckboxWidget>("UI_FEEDBACK_CHECKBOX")) {
    ptr_feedback->fn_is_checked = [&module_game, sz_slot] {
      return (module_game.GetInt(sz_slot) & 1) != 0;
    };
    ptr_feedback->fn_on_click = [fn_toggle] { fn_toggle(1); };
  }

  if (auto* ptr_transients =
          panel.GetOrNull<ora::ui::CheckboxWidget>("TRANSIENTS_CHECKBOX")) {
    ptr_transients->fn_is_checked = [&module_game, sz_slot] {
      return (module_game.GetInt(sz_slot) & 2) != 0;
    };
    ptr_transients->fn_on_click = [fn_toggle] { fn_toggle(2); };
  }
}

void ShowBattlefieldCameraDropdown(game::ModData& mod_data,
                                   ora::ui::Widget& dropdown,
                                   game::SettingsModule& module_graphics) {
  auto* ptr_dropdown =
      static_cast<ora::ui::DropDownButtonWidget*>(&dropdown);
  const std::size_t sz_slot = SlotOf(module_graphics, "ViewportDistance");
  gfx::WorldViewportSizes& viewport_sizes = gfx::GlobalViewportSizes();

  int int4_window_height = 0;
  if (gfx::Renderer* ptr_renderer = ora::ui::Ui::Instance().RendererDeps())
    int4_window_height = ptr_renderer->NativeResolution().Y;

  std::vector<int> vec_valid_sizes{1};
  if (viewport_sizes.GetSizeRange(gfx::WorldViewport::Medium).X <
      int4_window_height)
    vec_valid_sizes.push_back(2);

  const int2 int2_far_range =
      viewport_sizes.GetSizeRange(gfx::WorldViewport::Far);
  if (int2_far_range.X < int4_window_height)
    vec_valid_sizes.push_back(3);

  if (viewport_sizes.b_allow_native_zoom && int2_far_range.Y < int4_window_height)
    vec_valid_sizes.push_back(0);

  ptr_dropdown->ShowDropDown<int>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_valid_sizes,
      [sz_slot, &module_graphics,
       &mod_data](const int& int4_option,
                  ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_graphics, sz_slot, int4_option] {
              return module_graphics.GetInt(sz_slot) == int4_option;
            },
            [&module_graphics, sz_slot, int4_option] {
              module_graphics.SetInt(sz_slot, int4_option);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label =
            GetViewportSizeName(mod_data, int4_option);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowUIScaleDropdown(ora::ui::Widget& dropdown,
                         game::SettingsModule& module_graphics) {
  auto* ptr_dropdown =
      static_cast<ora::ui::DropDownButtonWidget*>(&dropdown);
  const std::size_t sz_slot = SlotOf(module_graphics, "UIScale");

  int2 int2_native{0, 0};
  if (gfx::Renderer* ptr_renderer = ora::ui::Ui::Instance().RendererDeps())
    int2_native = ptr_renderer->NativeResolution();
  const int2 int2_min = gfx::GlobalViewportSizes().int2_min_effective_resolution;
  const float fp4_max_scale = std::min(
      int2_min.X > 0 ? static_cast<float>(int2_native.X) / int2_min.X : 0.0f,
      int2_min.Y > 0 ? static_cast<float>(int2_native.Y) / int2_min.Y : 0.0f);

  std::vector<float> vec_valid_scales;
  for (const float fp4_scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f})
    if (fp4_scale <= fp4_max_scale)
      vec_valid_scales.push_back(fp4_scale);

  ptr_dropdown->ShowDropDown<float>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_valid_scales,
      [sz_slot, &module_graphics](const float& fp4_option,
                                  ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_graphics, sz_slot, fp4_option] {
              return module_graphics.GetFloat(sz_slot) == fp4_option;
            },
            [fp4_option] {
              ora::ui::Ui::Instance().RunAfterTickDefer([fp4_option] {
                game::SettingsModule& module_graphics_inner =
                    *game::Settings::Instance().Module("Graphics");
                const std::size_t sz_slot_inner =
                    module_graphics_inner.SlotIndexOf("UIScale").value_or(0);
                const float fp4_old = module_graphics_inner.GetFloat(sz_slot_inner);
                module_graphics_inner.SetFloat(sz_slot_inner, fp4_option);

                if (gfx::Renderer* ptr_renderer =
                        ora::ui::Ui::Instance().RendererDeps())
                  ptr_renderer->SetUIScale(fp4_option);
                RecalculateWidgetLayoutRoot();
                gfx::Viewport::int2_last_mouse_pos = int2{
                    static_cast<std::int32_t>(
                        static_cast<float>(gfx::Viewport::int2_last_mouse_pos.X) *
                        fp4_old / fp4_option),
                    static_cast<std::int32_t>(
                        static_cast<float>(gfx::Viewport::int2_last_mouse_pos.Y) *
                        fp4_old / fp4_option)};
              });
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label =
            std::format("{}%", static_cast<int>(100 * fp4_option));
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void ShowMouseControlDropdown(
    ora::ui::Widget& dropdown,
    const std::vector<std::pair<int, std::string>>& vec_control_types,
    game::SettingsModule& module_game) {
  auto* ptr_dropdown =
      static_cast<ora::ui::DropDownButtonWidget*>(&dropdown);
  const std::size_t sz_slot = SlotOf(module_game, "MouseControlStyle");

  ptr_dropdown->ShowDropDown<std::pair<int, std::string>>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_control_types,
      [sz_slot, &module_game](const std::pair<int, std::string>& pair_option,
                              ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&module_game, sz_slot, &pair_option] {
              return module_game.GetInt(sz_slot) == pair_option.first;
            },
            [&module_game, sz_slot, &pair_option] {
              module_game.SetInt(sz_slot, pair_option.first);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = pair_option.second;
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

SettingsLogic::SettingsLogic(
    ora::ui::Widget& widget, std::function<void()> fn_on_exit,
    ora::gfx::WorldRenderer* ptr_world_renderer, game::ModData& mod_data,
    const ora::ui::YamlDictionary* ptr_logic_args)
    : fn_on_exit_(std::move(fn_on_exit)),
      ptr_world_renderer_(ptr_world_renderer),
      ptr_mod_data_(&mod_data) {
  ptr_panel_container_ = widget.Get("PANEL_CONTAINER");
  if (ora::ui::Widget* ptr_panel_template =
          ptr_panel_container_->GetOrNull("PANEL_TEMPLATE"))
    up_panel_template_ = ptr_panel_container_->RemoveChild(*ptr_panel_template);

  ptr_tab_container_ = widget.Get("SETTINGS_TAB_CONTAINER");
  if (ora::ui::ButtonWidget* ptr_tab_button_template =
          ptr_tab_container_->GetOrNull<ora::ui::ButtonWidget>(
              "BUTTON_TEMPLATE"))
    up_tab_template_ = ptr_tab_container_->RemoveChild(*ptr_tab_button_template);

  const yaml::MiniYaml* ptr_args_button_stride = nullptr;
  if (ptr_logic_args != nullptr)
    for (const auto& [str_key, ptr_yaml] : *ptr_logic_args)
      if (str_key == "ButtonStride")
        ptr_args_button_stride = ptr_yaml;

  if (ptr_args_button_stride != nullptr &&
      ptr_args_button_stride->Value != nullptr) {
    const std::string_view sv_value = *ptr_args_button_stride->Value;
    const std::size_t sz_comma = sv_value.find(',');
    if (sz_comma != std::string_view::npos) {
      int2_button_stride_ = int2{
          std::stoi(std::string{sv_value.substr(0, sz_comma)}),
          std::stoi(std::string{sv_value.substr(sz_comma + 1)})};
    }
  }

  const yaml::MiniYaml* ptr_args_panels = nullptr;
  if (ptr_logic_args != nullptr)
    for (const auto& [str_key, ptr_yaml] : *ptr_logic_args)
      if (str_key == "Panels")
        ptr_args_panels = ptr_yaml;

  if (ptr_args_panels != nullptr && up_panel_template_ != nullptr) {
    for (const auto& node_panel : ptr_args_panels->Nodes) {
      if (node_panel.Key == nullptr)
        continue;
      const std::string str_panel_id = *node_panel.Key;
      const std::string str_fluent_key =
          node_panel.Value.Value != nullptr
              ? std::string{*node_panel.Value.Value}
              : std::string{};
      vec_panels_.emplace_back(str_panel_id, str_fluent_key);

      std::unique_ptr<ora::ui::Widget> up_container =
          up_panel_template_->Clone();
      up_container->str_id = str_panel_id;
      ptr_panel_container_->AddChild(std::move(up_container));

      ora::ui::WidgetArgs args_panel;
      args_panel.Add("settingsLogic", static_cast<ISettingsLogic*>(this));
      args_panel.Add("panelID", str_panel_id);
      args_panel.Add("label", Msg(str_fluent_key));
      args_panel.Add("modData", ptr_mod_data_);
      if (ptr_world_renderer_ != nullptr) {
        args_panel.Add("worldRenderer", ptr_world_renderer_);
        args_panel.Add("world", &ptr_world_renderer_->World());
      }
      ora::ui::Ui::Instance().LoadWidget(str_panel_id,
                                         ptr_panel_container_->Get(str_panel_id),
                                         std::move(args_panel));
    }
  }

  if (auto* ptr_back_button = widget.GetOrNull<ora::ui::ButtonWidget>(
          "BACK_BUTTON")) {
    ptr_back_button->fn_on_click = [this] {
      for (const auto& [str_key, fn_leave] : vec_leave_panel_actions_)
        if (str_key == str_active_panel_ && fn_leave)
          b_needs_restart_ = b_needs_restart_ || fn_leave();
      ora::ui::Ui::Instance().SettingsSaveDeps();

      const auto fn_close_and_exit = [this] {
        ora::ui::Ui::Instance().CloseWindow();
        if (fn_on_exit_)
          fn_on_exit_();
      };
      if (b_needs_restart_) {
        const auto fn_no_restart = [this, fn_close_and_exit] {
          ButtonPrompt(Msg(kSettingsSaveTitle), Msg(kSettingsSavePrompt),
                       nullptr, {}, fn_close_and_exit,
                       Msg(kSettingsSaveCancel));
        };
        fn_no_restart();
      } else {
        fn_close_and_exit();
      }
    };
  }

  if (auto* ptr_reset_button = widget.GetOrNull<ora::ui::ButtonWidget>(
          "RESET_BUTTON")) {
    ptr_reset_button->fn_on_click = [this] {
      const auto fn_reset = [this] {
        for (const auto& [str_key, fn_reset_panel] : vec_reset_panel_actions_)
          if (str_key == str_active_panel_ && fn_reset_panel)
            fn_reset_panel();
        ora::ui::Ui::Instance().SettingsSaveDeps();
      };

      std::string str_panel_title;
      for (const auto& [str_key, str_fluent] : vec_panels_)
        if (str_key == str_active_panel_)
          str_panel_title = str_fluent;
      std::string str_converted;
      if (Fluent().TryGetMessage(str_panel_title, {}, str_converted))
        str_panel_title = str_converted;

      ButtonPrompt(
          Msg(kResetTitle), Msg(kResetPrompt), fn_reset, Msg(kResetAccept),
          nullptr, Msg(kResetCancel), nullptr, {}, {},
          FluentArgs{{"panel", FluentValue{str_panel_title}}});
    };
  }
}

void SettingsLogic::RegisterSettingsPanel(
    const std::string& str_panel_id, const std::string& str_label,
    std::function<std::function<bool()>(ora::ui::Widget&)> fn_init,
    std::function<std::function<void()>(ora::ui::Widget&)> fn_reset) {
  ora::ui::Widget* ptr_panel = ptr_panel_container_->Get(str_panel_id);

  if (str_active_panel_.empty())
    str_active_panel_ = str_panel_id;

  ptr_panel->fn_is_visible = [this,
                              str_id = str_panel_id] {
    return str_active_panel_ == str_id;
  };

  vec_leave_panel_actions_.emplace_back(str_panel_id, fn_init(*ptr_panel));
  vec_reset_panel_actions_.emplace_back(str_panel_id, fn_reset(*ptr_panel));

  std::unique_ptr<ora::ui::Widget> up_tab = up_tab_template_->Clone();
  ora::ui::Widget* ptr_tab = up_tab.get();
  if (!vec_buttons_.empty()) {
    ora::ui::Widget* ptr_last_button = vec_buttons_.back();
    ptr_tab->bounds_widget.int4_x =
        ptr_last_button->bounds_widget.int4_x + int2_button_stride_.X;
    ptr_tab->bounds_widget.int4_y =
        ptr_last_button->bounds_widget.int4_y + int2_button_stride_.Y;
  }

  ptr_tab->str_id = str_panel_id;
  if (auto* ptr_tab_button =
          dynamic_cast<ora::ui::ButtonWidget*>(ptr_tab)) {
    ptr_tab_button->fn_get_text = [str_label] { return str_label; };
    ptr_tab_button->fn_is_highlighted = [this, str_id = str_panel_id] {
      return str_active_panel_ == str_id;
    };
    ptr_tab_button->fn_on_click = [this, str_id = str_panel_id] {
      for (const auto& [str_key, fn_leave] : vec_leave_panel_actions_)
        if (str_key == str_active_panel_ && fn_leave)
          b_needs_restart_ = b_needs_restart_ || fn_leave();
      ora::ui::Ui::Instance().SettingsSaveDeps();
      str_active_panel_ = str_id;
    };
  }

  ptr_tab_container_->AddChild(std::move(up_tab));
  vec_buttons_.push_back(ptr_tab);
}

AdvancedSettingsLogic::AdvancedSettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label) {
  game::Settings& settings_instance = game::Settings::Instance();
  ptr_debug_settings_ = settings_instance.Module("Debug");
  ptr_game_settings_ = settings_instance.Module("Game");
  ptr_server_settings_ = settings_instance.Module("Server");

  if (ptr_server_settings_ != nullptr && !OriginalServerDiscoverNat().has_value())
    OriginalServerDiscoverNat() =
        ptr_server_settings_->GetBool(SlotOf(*ptr_server_settings_, "DiscoverNatDevices"));

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });
  (void)&mod_data;
}

std::function<bool()> AdvancedSettingsLogic::InitPanel(ora::ui::Widget& panel) {
  auto* ptr_scroll_panel =
      panel.Get<ora::ui::ScrollPanelWidget>("SETTINGS_SCROLLPANEL");

  BindCheckboxPref(panel, "NAT_DISCOVERY", *ptr_server_settings_,
                   "DiscoverNatDevices");
  BindCheckboxPref(panel, "PERFTEXT_CHECKBOX", *ptr_debug_settings_,
                   "PerfText");
  BindCheckboxPref(panel, "PERFGRAPH_CHECKBOX", *ptr_debug_settings_,
                   "PerfGraph");
  BindCheckboxPref(panel, "FETCH_NEWS_CHECKBOX", *ptr_game_settings_,
                   "FetchNews");
  BindCheckboxPref(panel, "SENDSYSINFO_CHECKBOX", *ptr_debug_settings_,
                   "SendSystemInformation");
  BindCheckboxPref(panel, "CHECK_VERSION_CHECKBOX", *ptr_debug_settings_,
                   "CheckVersion");
  BindCheckboxPref(panel, "ENABLE_DISCORD_SERVICE", *ptr_game_settings_,
                   "EnableDiscordService");

  if (auto* ptr_ssi = panel.GetOrNull<ora::ui::CheckboxWidget>(
          "SENDSYSINFO_CHECKBOX")) {
    ptr_ssi->fn_is_disabled = [this] {
      return !ptr_game_settings_->GetBool(
          SlotOf(*ptr_game_settings_, "FetchNews"));
    };
  }

  BindCheckboxPref(panel, "BOTDEBUG_CHECKBOX", *ptr_debug_settings_,
                   "BotDebug");
  BindCheckboxPref(panel, "LUADEBUG_CHECKBOX", *ptr_debug_settings_,
                   "LuaDebug");
  BindCheckboxPref(panel, "REPLAY_COMMANDS_CHECKBOX", *ptr_debug_settings_,
                   "EnableDebugCommandsInReplays");
  BindCheckboxPref(panel, "CHECKUNSYNCED_CHECKBOX", *ptr_debug_settings_,
                   "SyncCheckUnsyncedCode");
  BindCheckboxPref(panel, "CHECKBOTSYNC_CHECKBOX", *ptr_debug_settings_,
                   "SyncCheckBotModuleCode");
  BindCheckboxPref(panel, "PERFLOGGING_CHECKBOX", *ptr_debug_settings_,
                   "EnableSimulationPerfLogging");

  const auto fn_visible_when_developer = [this] {
    return ptr_debug_settings_->GetBool(
        SlotOf(*ptr_debug_settings_, "DisplayDeveloperSettings"));
  };
  for (const std::string_view sv_id :
       {"BOTDEBUG_CHECKBOX_CONTAINER", "CHECKUNSYNCED_CHECKBOX_CONTAINER",
        "CHECKBOTSYNC_CHECKBOX_CONTAINER", "LUADEBUG_CHECKBOX_CONTAINER",
        "REPLAY_COMMANDS_CHECKBOX_CONTAINER",
        "PERFLOGGING_CHECKBOX_CONTAINER"}) {
    if (ora::ui::Widget* ptr_container = panel.GetOrNull(sv_id))
      ptr_container->fn_is_visible = fn_visible_when_developer;
  }
  if (ora::ui::Widget* ptr_hidden_container =
          panel.GetOrNull("DEBUG_HIDDEN_CONTAINER"))
    ptr_hidden_container->fn_is_visible = [this] {
      return !ptr_debug_settings_->GetBool(
          SlotOf(*ptr_debug_settings_, "DisplayDeveloperSettings"));
    };

  AdjustSettingsScrollPanelLayout(*ptr_scroll_panel);

  const std::size_t sz_slot =
      SlotOf(*ptr_server_settings_, "DiscoverNatDevices");
  return [this, sz_slot] {
    return ptr_server_settings_->GetBool(sz_slot) !=
           OriginalServerDiscoverNat().value_or(false);
  };
}

std::function<void()> AdvancedSettingsLogic::ResetPanel(ora::ui::Widget& panel) {
  (void)panel;
  const auto fn_reset = [this] {
    const auto fn_set_bool = [](game::SettingsModule& module_settings,
                                std::string_view str_pref, bool b_value) {
      module_settings.SetBool(SlotOf(module_settings, str_pref), b_value);
    };
    fn_set_bool(*ptr_server_settings_, "DiscoverNatDevices", false);
    fn_set_bool(*ptr_debug_settings_, "PerfText", false);
    fn_set_bool(*ptr_debug_settings_, "PerfGraph", false);
    fn_set_bool(*ptr_debug_settings_, "SyncCheckUnsyncedCode", false);
    fn_set_bool(*ptr_debug_settings_, "SyncCheckBotModuleCode", false);
    fn_set_bool(*ptr_debug_settings_, "BotDebug", false);
    fn_set_bool(*ptr_debug_settings_, "LuaDebug", false);
    fn_set_bool(*ptr_debug_settings_, "SendSystemInformation", true);
    fn_set_bool(*ptr_debug_settings_, "CheckVersion", true);
    fn_set_bool(*ptr_debug_settings_, "EnableDebugCommandsInReplays", false);
    fn_set_bool(*ptr_debug_settings_, "EnableSimulationPerfLogging", false);
    fn_set_bool(*ptr_game_settings_, "EnableDiscordService", true);
  };
  return fn_reset;
}

InputSettingsLogic::InputSettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label) {
  vec_control_types_ = {{0, Msg(kClassic)},
                        {1, Msg(kModern)},
                        {2, Msg(kOtherRTS)}};
  ptr_game_settings_ = game::Settings::Instance().Module("Game");

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });
  (void)&mod_data;
}

std::function<bool()> InputSettingsLogic::InitPanel(ora::ui::Widget& panel) {
  auto* ptr_scroll_panel =
      panel.Get<ora::ui::ScrollPanelWidget>("SETTINGS_SCROLLPANEL");

  BindCheckboxPref(panel, "ALTERNATE_SCROLL_CHECKBOX", *ptr_game_settings_,
                   "UseAlternateScrollButton");
  BindCheckboxPref(panel, "EDGESCROLL_CHECKBOX", *ptr_game_settings_,
                   "ViewportEdgeScroll");
  BindCheckboxPref(panel, "LOCKMOUSE_CHECKBOX", *ptr_game_settings_,
                   "LockMouseWindow");
  BindSliderPref(panel, "ZOOMSPEED_SLIDER", *ptr_game_settings_, "ZoomSpeed");
  BindSliderPref(panel, "SCROLLSPEED_SLIDER", *ptr_game_settings_,
                 "ViewportEdgeScrollStep");
  BindSliderPref(panel, "UI_SCROLLSPEED_SLIDER", *ptr_game_settings_,
                 "UIScrollSpeed");

  const std::size_t sz_slot_style =
      SlotOf(*ptr_game_settings_, "MouseControlStyle");
  const std::size_t sz_slot_scroll =
      SlotOf(*ptr_game_settings_, "MouseScroll");
  const std::size_t sz_slot_alternate =
      SlotOf(*ptr_game_settings_, "UseAlternateScrollButton");
  const std::size_t sz_slot_zoom_modifier =
      SlotOf(*ptr_game_settings_, "ZoomModifier");
  const std::size_t sz_slot_edge_scroll =
      SlotOf(*ptr_game_settings_, "ViewportEdgeScroll");

  if (auto* ptr_mouse_control_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "MOUSE_CONTROL_DROPDOWN")) {
    ptr_mouse_control_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowMouseControlDropdown(*ptr_game_settings_dropdown_, vec_control_types_,
                                   *ptr_game_settings_);
        };
    ptr_mouse_control_dropdown->fn_get_text = [this, sz_slot_style] {
      const std::int64_t int8_style = ptr_game_settings_->GetInt(sz_slot_style);
      for (const auto& [int4_value, str_label] : vec_control_types_)
        if (int4_value == int8_style)
          return str_label;
      return std::string{};
    };
    ptr_game_settings_dropdown_ = ptr_mouse_control_dropdown;
  }

  if (auto* ptr_mouse_scroll_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "MOUSE_SCROLL_TYPE_DROPDOWN")) {
    ptr_mouse_scroll_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowMouseScrollDropdown(*ptr_mouse_scroll_dropdown_, *ptr_game_settings_);
        };
    ptr_mouse_scroll_dropdown->fn_get_text = [this, sz_slot_scroll] {
      return MouseScrollName(ptr_game_settings_->GetInt(sz_slot_scroll));
    };
    ptr_mouse_scroll_dropdown_ = ptr_mouse_scroll_dropdown;
  }

  for (const auto& [str_id, int4_style] :
       std::initializer_list<std::pair<std::string_view, int>>{
           {"MOUSE_CONTROL_DESC_CLASSIC", 0},
           {"MOUSE_CONTROL_DESC_MODERN", 1},
           {"MOUSE_CONTROL_DESC_OTHERRTS", 2}}) {
    ora::ui::Widget* ptr_container = panel.GetOrNull(str_id);
    if (ptr_container == nullptr)
      continue;
    ptr_container->fn_is_visible = [this, sz_slot_style, int4_style] {
      return ptr_game_settings_->GetInt(sz_slot_style) == int4_style;
    };

    if (ora::ui::Widget* ptr_scroll_right =
            ptr_container->GetOrNull("DESC_SCROLL_RIGHT"))
      ptr_scroll_right->fn_is_visible = [this, sz_slot_style,
                                         sz_slot_alternate] {
        return (ptr_game_settings_->GetInt(sz_slot_style) == 0) ^
               ptr_game_settings_->GetBool(sz_slot_alternate);
      };
    if (ora::ui::Widget* ptr_scroll_middle =
            ptr_container->GetOrNull("DESC_SCROLL_MIDDLE"))
      ptr_scroll_middle->fn_is_visible = [this, sz_slot_style,
                                          sz_slot_alternate] {
        return (ptr_game_settings_->GetInt(sz_slot_style) != 0) ^
               ptr_game_settings_->GetBool(sz_slot_alternate);
      };
    if (ora::ui::Widget* ptr_zoom_desc =
            ptr_container->GetOrNull("DESC_ZOOM"))
      ptr_zoom_desc->fn_is_visible = [this, sz_slot_zoom_modifier] {
        return ptr_game_settings_->GetInt(sz_slot_zoom_modifier) == 0;
      };
    if (auto* ptr_zoom_desc_modifier =
            ptr_container->GetOrNull<ora::ui::LabelWidget>(
                "DESC_ZOOM_MODIFIER")) {
      ptr_zoom_desc_modifier->fn_is_visible = [this, sz_slot_zoom_modifier] {
        return ptr_game_settings_->GetInt(sz_slot_zoom_modifier) != 0;
      };
      const std::string str_template = ptr_zoom_desc_modifier->fn_get_text
                                           ? ptr_zoom_desc_modifier->fn_get_text()
                                           : std::string{};
      auto cache_modifier_label =
          std::make_shared<core::CachedTransform<std::int64_t, std::string>>(
              [str_template](const std::int64_t& int8_mod) {
                std::string str_text = str_template;
                const std::size_t sz_pos = str_text.find("MODIFIER");
                if (sz_pos != std::string::npos)
                  str_text.replace(
                      sz_pos, std::string_view{"MODIFIER"}.size(),
                      ModifierName(int8_mod));
                return str_text;
              });
      ptr_zoom_desc_modifier->fn_get_text = [this, sz_slot_zoom_modifier,
                                             cache_modifier_label] {
        return cache_modifier_label->Update(
            ptr_game_settings_->GetInt(sz_slot_zoom_modifier));
      };
    }
    if (auto* ptr_edgescroll_desc =
            ptr_container->GetOrNull<ora::ui::LabelWidget>(
                "DESC_EDGESCROLL"))
      ptr_edgescroll_desc->fn_is_visible = [this, sz_slot_edge_scroll] {
        return ptr_game_settings_->GetBool(sz_slot_edge_scroll);
      };
  }

  if (auto* ptr_lock_mouse_checkbox =
          panel.GetOrNull<ora::ui::CheckboxWidget>("LOCKMOUSE_CHECKBOX")) {
    std::function<void()> fn_old_on_click = ptr_lock_mouse_checkbox->fn_on_click;
    ptr_lock_mouse_checkbox->fn_on_click =
        [this, fn_old_on_click]() {
          if (fn_old_on_click)
            fn_old_on_click();
          MakeMouseFocusSettingsLive(*ptr_game_settings_);
        };
  }

  if (auto* ptr_zoom_modifier_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>("ZOOM_MODIFIER")) {
    ptr_zoom_modifier_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowZoomModifierDropdown(*ptr_zoom_modifier_dropdown_, *ptr_game_settings_);
        };
    ptr_zoom_modifier_dropdown->fn_get_text = [this, sz_slot_zoom_modifier] {
      return ModifierName(ptr_game_settings_->GetInt(sz_slot_zoom_modifier));
    };
    ptr_zoom_modifier_dropdown_ = ptr_zoom_modifier_dropdown;
  }

  AdjustSettingsScrollPanelLayout(*ptr_scroll_panel);

  return [] { return false; };
}

std::function<void()> InputSettingsLogic::ResetPanel(ora::ui::Widget& panel) {
  const auto fn_reset = [this, &panel] {
    const auto fn_set_bool = [this](std::string_view str_pref, bool b_value) {
      ptr_game_settings_->SetBool(SlotOf(*ptr_game_settings_, str_pref), b_value);
    };
    const auto fn_set_int = [this](std::string_view str_pref,
                                   std::int64_t int8_value) {
      ptr_game_settings_->SetInt(SlotOf(*ptr_game_settings_, str_pref), int8_value);
    };
    const auto fn_set_float = [this](std::string_view str_pref,
                                     float fp4_value) {
      ptr_game_settings_->SetFloat(SlotOf(*ptr_game_settings_, str_pref), fp4_value);
    };

    fn_set_int("MouseControlStyle", 1);
    fn_set_int("MouseScroll", 3);
    fn_set_bool("UseAlternateScrollButton", false);
    fn_set_bool("LockMouseWindow", false);
    fn_set_bool("ViewportEdgeScroll", true);
    fn_set_float("ViewportEdgeScrollStep", 30.0f);
    fn_set_float("UIScrollSpeed", 50.0f);
    fn_set_float("ZoomSpeed", 0.04f);
    fn_set_int("ZoomModifier", 0);

    if (auto* ptr_scroll_slider =
            panel.GetOrNull<ora::ui::SliderWidget>("SCROLLSPEED_SLIDER"))
      ptr_scroll_slider->fp4_value = ptr_game_settings_->GetFloat(
          SlotOf(*ptr_game_settings_, "ViewportEdgeScrollStep"));
    if (auto* ptr_ui_scroll_slider =
            panel.GetOrNull<ora::ui::SliderWidget>("UI_SCROLLSPEED_SLIDER"))
      ptr_ui_scroll_slider->fp4_value = ptr_game_settings_->GetFloat(
          SlotOf(*ptr_game_settings_, "UIScrollSpeed"));

    MakeMouseFocusSettingsLive(*ptr_game_settings_);
  };
  return fn_reset;
}

AudioSettingsLogic::AudioSettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label,
    ora::gfx::WorldRenderer* ptr_world_renderer) {
  ptr_sound_settings_ = game::Settings::Instance().Module("Sound");
  if (ptr_world_renderer != nullptr) {
    sim::World& world_game = ptr_world_renderer->World();
    if (world_game.WorldActor() != nullptr)
      ptr_music_playlist_ = world_game.WorldActor()->Trait<MusicPlaylist>();
  }

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });
  (void)&mod_data;
}

std::string AudioSettingsLogic::DeviceLabel(
    const std::string& str_device) const {
  for (const auto& [str_key, str_label] : vec_devices_)
    if (str_key == str_device)
      return str_label;
  return std::string{};
}

std::function<bool()> AudioSettingsLogic::InitPanel(ora::ui::Widget& panel) {
  auto* ptr_scroll_panel =
      panel.Get<ora::ui::ScrollPanelWidget>("SETTINGS_SCROLLPANEL");

  BindCheckboxPref(panel, "CASH_TICKS", *ptr_sound_settings_, "CashTicks");
  BindCheckboxPref(panel, "MUTE_SOUND", *ptr_sound_settings_, "Mute");
  BindCheckboxPref(panel, "MUTE_BACKGROUND_MUSIC",
                   *ptr_sound_settings_, "MuteBackgroundMusic");

  BindSliderPref(panel, "SOUND_VOLUME", *ptr_sound_settings_, "SoundVolume");
  BindSliderPref(panel, "MUSIC_VOLUME", *ptr_sound_settings_, "MusicVolume");
  BindSliderPref(panel, "VIDEO_VOLUME", *ptr_sound_settings_, "VideoVolume");

  sound::Sound* ptr_sound =
      ora::ui::Ui::HasInstance() ? ora::ui::Ui::Instance().SoundDeps()
                                 : nullptr;
  const bool b_dummy_engine = ptr_sound != nullptr && ptr_sound->DummyEngine();

  if (auto* ptr_mute_checkbox =
          panel.GetOrNull<ora::ui::CheckboxWidget>("MUTE_SOUND")) {
    const std::function<bool()> fn_is_checked = ptr_mute_checkbox->fn_is_checked;
    const std::function<void()> fn_on_click = ptr_mute_checkbox->fn_on_click;
    const std::size_t sz_slot = SlotOf(*ptr_sound_settings_, "Mute");
    ptr_mute_checkbox->fn_is_checked = [fn_is_checked, b_dummy_engine] {
      return fn_is_checked() || b_dummy_engine;
    };
    ptr_mute_checkbox->fn_is_disabled = [b_dummy_engine] {
      return b_dummy_engine;
    };
    ptr_mute_checkbox->fn_on_click =
        [this, fn_on_click, ptr_sound, sz_slot]() {
          if (fn_on_click)
            fn_on_click();
          if (ptr_sound != nullptr) {
            if (ptr_sound_settings_->GetBool(sz_slot))
              ptr_sound->MuteAudio();
            else
              ptr_sound->UnmuteAudio();
          }
        };
  }

  if (auto* ptr_mute_background_checkbox =
          panel.GetOrNull<ora::ui::CheckboxWidget>(
              "MUTE_BACKGROUND_MUSIC")) {
    const std::function<void()> fn_on_click =
        ptr_mute_background_checkbox->fn_on_click;
    MusicPlaylist* ptr_playlist = ptr_music_playlist_;
    ptr_mute_background_checkbox->fn_on_click = [fn_on_click,
                                                 ptr_playlist]() {
      if (fn_on_click)
        fn_on_click();
      if (ptr_playlist == nullptr)
        return;
      if (!ptr_playlist->AllowMuteBackgroundMusic())
        return;
      if (ptr_playlist->CurrentSongIsBackground())
        ptr_playlist->Stop();
    };
  }

  if (ora::ui::Widget* ptr_no_device_label =
          panel.GetOrNull("NO_AUDIO_DEVICE_CONTAINER"))
    ptr_no_device_label->b_visible = b_dummy_engine;

  for (const std::string_view sv_id :
       {"CASH_TICKS_CONTAINER", "MUTE_SOUND_CONTAINER",
        "MUTE_BACKGROUND_MUSIC_CONTAINER", "SOUND_VOLUME_CONTAINER",
        "MUSIC_VOLUME_CONTAINER", "VIDEO_VOLUME_CONTAINER"}) {
    if (ora::ui::Widget* ptr_container = panel.GetOrNull(sv_id))
      ptr_container->b_visible = !b_dummy_engine;
  }

  if (auto* ptr_sound_volume_slider =
          panel.GetOrNull<ora::ui::SliderWidget>("SOUND_VOLUME"))
    AppendSliderOnChange(*ptr_sound_volume_slider,
                         [ptr_sound](float fp4_value) {
                           if (ptr_sound != nullptr)
                             ptr_sound->SetSoundVolume(fp4_value);
                         });
  if (auto* ptr_music_volume_slider =
          panel.GetOrNull<ora::ui::SliderWidget>("MUSIC_VOLUME"))
    AppendSliderOnChange(*ptr_music_volume_slider,
                         [ptr_sound](float fp4_value) {
                           if (ptr_sound != nullptr)
                             ptr_sound->SetMusicVolume(fp4_value);
                         });
  if (auto* ptr_video_volume_slider =
          panel.GetOrNull<ora::ui::SliderWidget>("VIDEO_VOLUME"))
    AppendSliderOnChange(*ptr_video_volume_slider,
                         [ptr_sound](float fp4_value) {
                           if (ptr_sound != nullptr)
                             ptr_sound->SetVideoVolume(fp4_value);
                         });

  const std::string str_settings_device =
      ptr_sound_settings_->GetString(SlotOf(*ptr_sound_settings_, "Device"));
  std::vector<platform::SoundDevice> vec_devices;
  if (ptr_sound != nullptr)
    vec_devices = ptr_sound->AvailableDevices();
  vec_devices_.clear();
  for (const platform::SoundDevice& device_sound : vec_devices)
    vec_devices_.emplace_back(device_sound.str_device.value_or(""),
                              device_sound.str_label);

  const auto it_device = std::find_if(
      vec_devices.begin(), vec_devices.end(),
      [&](const platform::SoundDevice& device_sound) {
        return device_sound.str_device.value_or("") == str_settings_device;
      });
  if (it_device != vec_devices.end())
    str_sound_device_ = it_device->str_device.value_or("");
  else if (!vec_devices.empty())
    str_sound_device_ = vec_devices.front().str_device.value_or("");
  else
    str_sound_device_ = str_settings_device;

  if (auto* ptr_audio_device_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "AUDIO_DEVICE")) {
    ptr_audio_device_dropdown_ = ptr_audio_device_dropdown;
    ptr_audio_device_dropdown->fn_on_mouse_down =
        [this, ptr_scroll_panel](const MouseInput&) {
          ShowAudioDeviceDropdown(*ptr_scroll_panel);
        };

    const std::string str_font_key = ptr_audio_device_dropdown->str_font;
    const int int4_usable_width = ptr_audio_device_dropdown->UsableWidth();
    auto ptr_dropdown_capture = ptr_audio_device_dropdown_;
    auto cache_device_label =
        std::make_shared<core::CachedTransform<std::string, std::string>>(
            [this, int4_usable_width, str_font_key](const std::string& str_device) {
              return ora::ui::TruncateText(
                  DeviceLabel(str_device), int4_usable_width,
                  ora::ui::RequireWidgetFont(str_font_key));
            });
    ptr_audio_device_dropdown->fn_get_text = [this, cache_device_label,
                                              ptr_dropdown_capture] {
      (void)ptr_dropdown_capture;
      return cache_device_label->Update(str_sound_device_);
    };
  }

  const std::size_t sz_slot_device = SlotOf(*ptr_sound_settings_, "Device");
  if (ora::ui::Widget* ptr_restart_desc =
          panel.GetOrNull("AUDIO_RESTART_REQUIRED_DESC"))
    ptr_restart_desc->fn_is_visible = [this, sz_slot_device] {
      return str_sound_device_ !=
             SnapshotString(OriginalSoundSnapshot(), sz_slot_device);
    };

  AdjustSettingsScrollPanelLayout(*ptr_scroll_panel);

  return [this, sz_slot_device] {
    ptr_sound_settings_->SetString(sz_slot_device, str_sound_device_);
    return ptr_sound_settings_->GetString(sz_slot_device) !=
           SnapshotString(OriginalSoundSnapshot(), sz_slot_device);
  };
}

std::function<void()> AudioSettingsLogic::ResetPanel(ora::ui::Widget& panel) {  const auto fn_reset = [this, &panel] {
    ptr_sound_settings_->SetFloat(
        SlotOf(*ptr_sound_settings_, "SoundVolume"),
        ptr_sound_settings_->DefaultFloat(SlotOf(*ptr_sound_settings_, "SoundVolume")));
    ptr_sound_settings_->SetFloat(
        SlotOf(*ptr_sound_settings_, "MusicVolume"),
        ptr_sound_settings_->DefaultFloat(SlotOf(*ptr_sound_settings_, "MusicVolume")));
    ptr_sound_settings_->SetFloat(
        SlotOf(*ptr_sound_settings_, "VideoVolume"),
        ptr_sound_settings_->DefaultFloat(SlotOf(*ptr_sound_settings_, "VideoVolume")));
    ptr_sound_settings_->SetBool(
        SlotOf(*ptr_sound_settings_, "CashTicks"),
        ptr_sound_settings_->DefaultBool(SlotOf(*ptr_sound_settings_, "CashTicks")));
    ptr_sound_settings_->SetBool(
        SlotOf(*ptr_sound_settings_, "Mute"),
        ptr_sound_settings_->DefaultBool(SlotOf(*ptr_sound_settings_, "Mute")));
    ptr_sound_settings_->SetBool(
        SlotOf(*ptr_sound_settings_, "MuteBackgroundMusic"),
        ptr_sound_settings_->DefaultBool(
            SlotOf(*ptr_sound_settings_, "MuteBackgroundMusic")));
    ptr_sound_settings_->SetString(
        SlotOf(*ptr_sound_settings_, "Device"),
        ptr_sound_settings_->DefaultString(SlotOf(*ptr_sound_settings_, "Device")));

    sound::Sound* ptr_sound =
        ora::ui::Ui::HasInstance() ? ora::ui::Ui::Instance().SoundDeps()
                                   : nullptr;
    const std::size_t sz_slot_sound_volume =
        SlotOf(*ptr_sound_settings_, "SoundVolume");
    const std::size_t sz_slot_music_volume =
        SlotOf(*ptr_sound_settings_, "MusicVolume");
    const std::size_t sz_slot_video_volume =
        SlotOf(*ptr_sound_settings_, "VideoVolume");

    if (auto* ptr_slider = panel.GetOrNull<ora::ui::SliderWidget>("SOUND_VOLUME"))
      ptr_slider->fp4_value = ptr_sound_settings_->GetFloat(sz_slot_sound_volume);
    if (ptr_sound != nullptr)
      ptr_sound->SetSoundVolume(ptr_sound_settings_->GetFloat(sz_slot_sound_volume));
    if (auto* ptr_slider = panel.GetOrNull<ora::ui::SliderWidget>("MUSIC_VOLUME"))
      ptr_slider->fp4_value = ptr_sound_settings_->GetFloat(sz_slot_music_volume);
    if (ptr_sound != nullptr)
      ptr_sound->SetMusicVolume(ptr_sound_settings_->GetFloat(sz_slot_music_volume));
    if (auto* ptr_slider = panel.GetOrNull<ora::ui::SliderWidget>("VIDEO_VOLUME"))
      ptr_slider->fp4_value = ptr_sound_settings_->GetFloat(sz_slot_video_volume);
    if (ptr_sound != nullptr)
      ptr_sound->SetVideoVolume(ptr_sound_settings_->GetFloat(sz_slot_video_volume));
    if (ptr_sound != nullptr)
      ptr_sound->UnmuteAudio();

    if (ptr_sound != nullptr) {
      const std::vector<platform::SoundDevice> vec_devices =
          ptr_sound->AvailableDevices();
      if (!vec_devices.empty())
        str_sound_device_ = vec_devices.front().str_device.value_or("");
    }
  };
  return fn_reset;
}

void AudioSettingsLogic::ShowAudioDeviceDropdown(
    ora::ui::ScrollPanelWidget& scroll_panel) {
  if (ptr_audio_device_dropdown_ == nullptr)
    return;

  std::vector<std::string> vec_keys;
  for (const auto& [str_key, str_label] : vec_devices_)
    vec_keys.push_back(str_key);

  ptr_audio_device_dropdown_->ShowDropDown<std::string>(
      "LABEL_DROPDOWN_TEMPLATE", 500, vec_keys,
      [this, &scroll_panel](const std::string& str_option,
                            ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [this, str_option] { return str_sound_device_ == str_option; },
            [this, str_option, &scroll_panel] {
              str_sound_device_ = str_option;
              AdjustSettingsScrollPanelLayout(scroll_panel);
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const int int4_label_width = ptr_label->bounds_widget.int4_width;
        const std::string str_font_key = ptr_label->str_font;
        const std::string str_label = ora::ui::TruncateText(
            DeviceLabel(str_option), int4_label_width,
            ora::ui::RequireWidgetFont(str_font_key));
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

DisplaySettingsLogic::DisplaySettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label,
    ora::gfx::WorldRenderer* ptr_world_renderer) {
  ptr_mod_data_ = &mod_data;
  ptr_world_renderer_ = ptr_world_renderer;
  game::Settings& settings_instance = game::Settings::Instance();
  ptr_game_settings_ = settings_instance.Module("Game");
  ptr_graphic_settings_ = settings_instance.Module("Graphics");

  str_legacy_fullscreen_ = Msg(kLegacyFullscreen);
  str_fullscreen_ = Msg(kFullscreen);
  str_select_preset_ = Msg(kSelectPreset);

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });

  str_show_on_damage_ = Msg(kShowOnDamage);
  str_always_show_ = Msg(kAlwaysShow);
  str_automatic_ = Msg(kAutomatic);
  str_manual_ = Msg(kManual);
  str_disabled_ = Msg(kDisabled);
}

std::function<bool()> DisplaySettingsLogic::InitPanel(ora::ui::Widget& panel) {
  auto* ptr_scroll_panel =
      panel.Get<ora::ui::ScrollPanelWidget>("SETTINGS_SCROLLPANEL");

  BindCheckboxPref(panel, "CURSORDOUBLE_CHECKBOX", *ptr_graphic_settings_,
                   "CursorDouble");
  BindCheckboxPref(panel, "VSYNC_CHECKBOX", *ptr_graphic_settings_, "VSync");
  BindCheckboxPref(panel, "FRAME_LIMIT_CHECKBOX", *ptr_graphic_settings_,
                   "CapFramerate");
  BindCheckboxPref(panel, "FRAME_LIMIT_GAMESPEED_CHECKBOX",
                   *ptr_graphic_settings_, "CapFramerateToGameFps");
  BindIntSliderPref(panel, "FRAME_LIMIT_SLIDER", *ptr_graphic_settings_,
                    "MaxFramerate");
  BindCheckboxPref(panel, "PLAYER_STANCE_COLORS_CHECKBOX", *ptr_game_settings_,
                   "UsePlayerStanceColors");

  if (panel.GetOrNull<ora::ui::CheckboxWidget>("PAUSE_SHELLMAP_CHECKBOX") !=
      nullptr)
    BindCheckboxPref(panel, "PAUSE_SHELLMAP_CHECKBOX", *ptr_game_settings_,
                     "PauseShellmap");

  const std::size_t sz_slot_mode = SlotOf(*ptr_graphic_settings_, "Mode");
  const std::size_t sz_slot_display =
      SlotOf(*ptr_graphic_settings_, "VideoDisplay");
  const std::size_t sz_slot_gl_profile =
      SlotOf(*ptr_graphic_settings_, "GLProfile");
  const std::size_t sz_slot_status_bars =
      SlotOf(*ptr_game_settings_, "StatusBars");
  const std::size_t sz_slot_target_lines =
      SlotOf(*ptr_game_settings_, "TargetLines");
  const std::size_t sz_slot_viewport_distance =
      SlotOf(*ptr_graphic_settings_, "ViewportDistance");
  const std::size_t sz_slot_ui_scale = SlotOf(*ptr_graphic_settings_, "UIScale");
  const std::size_t sz_slot_windowed_size =
      SlotOf(*ptr_graphic_settings_, "WindowedSize");
  const std::size_t sz_slot_max_framerate =
      SlotOf(*ptr_graphic_settings_, "MaxFramerate");
  const std::size_t sz_slot_vsync = SlotOf(*ptr_graphic_settings_, "VSync");

  gfx::Renderer* ptr_renderer =
      ora::ui::Ui::HasInstance() ? ora::ui::Ui::Instance().RendererDeps()
                                 : nullptr;

  if (auto* ptr_window_mode_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>("MODE_DROPDOWN")) {
    ptr_window_mode_dropdown_ = ptr_window_mode_dropdown;
    ptr_window_mode_dropdown->fn_on_mouse_down =
        [this, ptr_scroll_panel](const MouseInput&) {
          ShowWindowModeDropdown(*ptr_window_mode_dropdown_,
                                 *ptr_graphic_settings_, *ptr_scroll_panel);
        };
    ptr_window_mode_dropdown->fn_get_text = [this, sz_slot_mode] {
      const std::int64_t int8_mode = ptr_graphic_settings_->GetInt(sz_slot_mode);
      if (int8_mode == 0)
        return Msg(kWindowed);
      return int8_mode == 1 ? str_legacy_fullscreen_ : str_fullscreen_;
    };
  }

  if (auto* ptr_display_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "DISPLAY_SELECTION_DROPDOWN")) {
    ptr_display_selection_dropdown_ = ptr_display_dropdown;
    ptr_display_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowDisplaySelectionDropdown(*ptr_display_selection_dropdown_,
                                       *ptr_graphic_settings_);
        };
    ptr_display_dropdown->fn_get_text = [this, sz_slot_display] {
      return MsgArg(kDisplayIndex, "number",
                    FluentValue{ptr_graphic_settings_->GetInt(sz_slot_display) + 1});
    };
    ptr_display_dropdown->fn_is_disabled = [ptr_renderer] {
      return ptr_renderer == nullptr || ptr_renderer->DisplayCount() < 2;
    };
  }

  if (auto* ptr_gl_profile_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "GL_PROFILE_DROPDOWN")) {
    const bool b_disable_profile =
        (ptr_renderer == nullptr ||
         ptr_renderer->SupportedGLProfiles().size() < 2) &&
        ptr_graphic_settings_->GetInt(sz_slot_gl_profile) == 0;
    ptr_gl_profile_dropdown_ = ptr_gl_profile_dropdown;
    ptr_gl_profile_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowGLProfileDropdown(*ptr_gl_profile_dropdown_,
                               *ptr_graphic_settings_);
        };
    ptr_gl_profile_dropdown->fn_get_text = [this, sz_slot_gl_profile] {
      return GLProfileName(ptr_graphic_settings_->GetInt(sz_slot_gl_profile));
    };
    ptr_gl_profile_dropdown->fn_is_disabled = [b_disable_profile] {
      return b_disable_profile;
    };
  }

  if (auto* ptr_status_bars_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "STATUS_BAR_DROPDOWN")) {
    ptr_status_bars_dropdown_ = ptr_status_bars_dropdown;
    ptr_status_bars_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowStatusBarsDropdown(*ptr_status_bars_dropdown_,
                                 *ptr_game_settings_);
        };
    ptr_status_bars_dropdown->fn_get_text = [this, sz_slot_status_bars] {
      const std::int64_t int8_status =
          ptr_game_settings_->GetInt(sz_slot_status_bars);
      if (int8_status == 0)
        return Msg(kStandard);
      return int8_status == 1 ? str_show_on_damage_ : str_always_show_;
    };
  }

  if (auto* ptr_target_lines_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "TARGET_LINES_DROPDOWN")) {
    ptr_target_lines_dropdown_ = ptr_target_lines_dropdown;
    ptr_target_lines_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowTargetLinesDropdown(*ptr_target_lines_dropdown_,
                                  *ptr_game_settings_);
        };
    ptr_target_lines_dropdown->fn_get_text = [this, sz_slot_target_lines] {
      const std::int64_t int8_target =
          ptr_game_settings_->GetInt(sz_slot_target_lines);
      if (int8_target == 2)
        return str_automatic_;
      return int8_target == 1 ? str_manual_ : str_disabled_;
    };
  }

  if (auto* ptr_battlefield_camera_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "BATTLEFIELD_CAMERA_DROPDOWN")) {
    ptr_battlefield_camera_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowBattlefieldCameraDropdown(
              *ptr_mod_data_, *ptr_battlefield_camera_dropdown_,
              *ptr_graphic_settings_);
        };
    auto cache_camera_label =
        std::make_shared<core::CachedTransform<std::int64_t, std::string>>(
            [this](const std::int64_t& int8_viewport) {
              return GetViewportSizeName(*ptr_mod_data_,
                                         static_cast<int>(int8_viewport));
            });
    ptr_battlefield_camera_dropdown->fn_get_text =
        [this, sz_slot_viewport_distance, cache_camera_label] {
          return cache_camera_label->Update(
              ptr_graphic_settings_->GetInt(sz_slot_viewport_distance));
        };
  }

  BindTextNotificationPoolFilterSettings(panel, *ptr_game_settings_);

  if (auto* ptr_vsync_checkbox =
          panel.GetOrNull<ora::ui::CheckboxWidget>("VSYNC_CHECKBOX")) {
    const std::function<void()> fn_old_on_click =
        ptr_vsync_checkbox->fn_on_click;
    ptr_vsync_checkbox->fn_on_click =
        [this, fn_old_on_click, ptr_renderer, sz_slot_vsync]() {
          if (fn_old_on_click)
            fn_old_on_click();
          if (ptr_renderer != nullptr)
            ptr_renderer->SetVSyncEnabled(
                ptr_graphic_settings_->GetBool(sz_slot_vsync));
        };
  }

  if (auto* ptr_ui_scale_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "UI_SCALE_DROPDOWN")) {
    ptr_ui_scale_dropdown_ = ptr_ui_scale_dropdown;
    ptr_ui_scale_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowUIScaleDropdown(*ptr_ui_scale_dropdown_, *ptr_graphic_settings_);
        };
    auto cache_ui_scale_label =
        std::make_shared<core::CachedTransform<float, std::string>>(
            [](const float& fp4_scale) {
              return std::format("{}%", static_cast<int>(100 * fp4_scale));
            });
    ptr_ui_scale_dropdown->fn_get_text =
        [this, sz_slot_ui_scale, cache_ui_scale_label] {
          return cache_ui_scale_label->Update(
              ptr_graphic_settings_->GetFloat(sz_slot_ui_scale));
        };

    const gfx::WorldViewportSizes& viewport_sizes =
        gfx::GlobalViewportSizes();
    const int2 int2_min = viewport_sizes.int2_min_effective_resolution;
    int2 int2_resolution{0, 0};
    if (ptr_renderer != nullptr)
      int2_resolution = ptr_renderer->Resolution();
    const float fp4_scale = ptr_graphic_settings_->GetFloat(sz_slot_ui_scale);
    const bool b_is_shellmap =
        ptr_world_renderer_ != nullptr &&
        ptr_world_renderer_->World().Type() == sim::WorldType::Shellmap;
    const bool b_disable_ui_scale =
        !b_is_shellmap ||
        int2_resolution.X * fp4_scale < 1.25f * int2_min.X ||
        int2_resolution.Y * fp4_scale < 1.25f * int2_min.Y;
    ptr_ui_scale_dropdown->fn_is_disabled = [b_disable_ui_scale] {
      return b_disable_ui_scale;
    };
  }

  if (ora::ui::Widget* ptr_display_container =
          panel.GetOrNull("DISPLAY_SELECTION_CONTAINER"))
    ptr_display_container->fn_is_visible = [this, sz_slot_mode] {
      return ptr_graphic_settings_->GetInt(sz_slot_mode) != 0;
    };
  if (ora::ui::Widget* ptr_resolution_container =
          panel.GetOrNull("WINDOW_RESOLUTION_CONTAINER"))
    ptr_resolution_container->fn_is_visible = [this, sz_slot_mode] {
      return ptr_graphic_settings_->GetInt(sz_slot_mode) == 0;
    };

  auto* ptr_window_width =
      panel.GetOrNull<ora::ui::TextFieldWidget>("WINDOW_WIDTH");
  auto* ptr_window_height =
      panel.GetOrNull<ora::ui::TextFieldWidget>("WINDOW_HEIGHT");
  std::string str_orig_width_text;
  std::string str_orig_height_text;
  if (ptr_window_width != nullptr) {
    const int2 int2_windowed =
        ptr_graphic_settings_->GetInt2(sz_slot_windowed_size);
    str_orig_width_text = NumberToText(int2_windowed.X);
    ptr_window_width->SetText(str_orig_width_text);
  }
  if (ptr_window_height != nullptr) {
    const int2 int2_windowed =
        ptr_graphic_settings_->GetInt2(sz_slot_windowed_size);
    str_orig_height_text = NumberToText(int2_windowed.Y);
    ptr_window_height->SetText(str_orig_height_text);
  }

  if (auto* ptr_resolution_preset_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "RESOLUTION_PRESET_DROPDOWN")) {
    ptr_resolution_preset_dropdown_ = ptr_resolution_preset_dropdown;
    const std::string str_select_preset_text = str_select_preset_;
    ptr_resolution_preset_dropdown->fn_get_text =
        [ptr_window_width, ptr_window_height, str_select_preset_text]() {
          if (ptr_window_width != nullptr && ptr_window_height != nullptr) {
            const auto opt_width = TextToNumber(ptr_window_width->Text());
            const auto opt_height = TextToNumber(ptr_window_height->Text());
            if (opt_width.has_value() && opt_height.has_value()) {
              for (const auto& [int4_w, int4_h] : kCommonResolutions)
                if (int4_w == *opt_width && int4_h == *opt_height)
                  return std::format("{}x{}", *opt_width, *opt_height);
            }
          }
          return str_select_preset_text;
        };
    ptr_resolution_preset_dropdown->fn_on_mouse_down =
        [this, ptr_window_width, ptr_window_height](const MouseInput&) {
          ShowResolutionPresetDropdown(*ptr_resolution_preset_dropdown_,
                                       ptr_window_width, ptr_window_height);
        };
  }

  if (ora::ui::Widget* ptr_restart_desc =
          panel.GetOrNull("VIDEO_RESTART_REQUIRED_DESC"))
    ptr_restart_desc->fn_is_visible = [this, sz_slot_mode, sz_slot_display,
                                       sz_slot_gl_profile, str_orig_width_text,
                                       str_orig_height_text, ptr_window_width,
                                       ptr_window_height]() {
      if (ptr_graphic_settings_->GetInt(sz_slot_mode) !=
              SnapshotInt(OriginalGraphicSnapshot(), sz_slot_mode) ||
          ptr_graphic_settings_->GetInt(sz_slot_display) !=
              SnapshotInt(OriginalGraphicSnapshot(), sz_slot_display) ||
          ptr_graphic_settings_->GetInt(sz_slot_gl_profile) !=
              SnapshotInt(OriginalGraphicSnapshot(), sz_slot_gl_profile))
        return true;
      return ptr_graphic_settings_->GetInt(sz_slot_mode) == 0 &&
             ((ptr_window_width != nullptr &&
               str_orig_width_text != ptr_window_width->Text()) ||
              (ptr_window_height != nullptr &&
               str_orig_height_text != ptr_window_height->Text()));
    };

  auto* ptr_frame_limit_gamespeed_checkbox =
      panel.GetOrNull<ora::ui::CheckboxWidget>(
          "FRAME_LIMIT_GAMESPEED_CHECKBOX");
  auto* ptr_frame_limit_checkbox =
      panel.GetOrNull<ora::ui::CheckboxWidget>("FRAME_LIMIT_CHECKBOX");
  if (ptr_frame_limit_checkbox != nullptr) {
    auto cache_frame_limit_label =
        std::make_shared<core::CachedTransform<std::int64_t, std::string>>(
            [](const std::int64_t& int8_fps) {
              return MsgArg(kFrameLimiter, "fps", FluentValue{int8_fps});
            });
    ptr_frame_limit_checkbox->fn_get_text =
        [this, sz_slot_max_framerate, cache_frame_limit_label] {
          return cache_frame_limit_label->Update(
              ptr_graphic_settings_->GetInt(sz_slot_max_framerate));
        };
    ptr_frame_limit_checkbox->fn_is_disabled = [this] {
      return ptr_graphic_settings_->GetBool(
          SlotOf(*ptr_graphic_settings_, "CapFramerateToGameFps"));
    };
  }

  if (auto* ptr_frame_limit_slider =
          panel.GetOrNull<ora::ui::SliderWidget>("FRAME_LIMIT_SLIDER"))
    ptr_frame_limit_slider->fn_is_disabled =
        [ptr_frame_limit_checkbox, ptr_frame_limit_gamespeed_checkbox]() {
          const bool b_cap_disabled =
              ptr_frame_limit_checkbox == nullptr ||
              !ptr_frame_limit_checkbox->fn_is_checked();
          const bool b_gamespeed_cap =
              ptr_frame_limit_gamespeed_checkbox != nullptr &&
              ptr_frame_limit_gamespeed_checkbox->fn_is_checked();
          return b_cap_disabled || b_gamespeed_cap;
        };

  AdjustSettingsScrollPanelLayout(*ptr_scroll_panel);

  return [this, sz_slot_mode, sz_slot_display, sz_slot_gl_profile,
          sz_slot_windowed_size, ptr_window_width, ptr_window_height]() {
    if (ptr_window_width != nullptr) {
      if (const auto opt_x = TextToNumber(ptr_window_width->Text())) {
        ptr_graphic_settings_->SetInt2(
            sz_slot_windowed_size,
            int2{*opt_x,
                 ptr_graphic_settings_->GetInt2(sz_slot_windowed_size).Y});
      }
    }
    if (ptr_window_height != nullptr) {
      if (const auto opt_y = TextToNumber(ptr_window_height->Text())) {
        ptr_graphic_settings_->SetInt2(
            sz_slot_windowed_size,
            int2{ptr_graphic_settings_->GetInt2(sz_slot_windowed_size).X,
                 *opt_y});
      }
    }

    const meta::GenericTuple& tuple_windowed =
        std::get<meta::GenericTuple>(
            ptr_graphic_settings_->Record().Slot(sz_slot_windowed_size).val);
    const meta::GenericTuple& tuple_windowed_orig = std::get<meta::GenericTuple>(
        OriginalGraphicSnapshot().Slot(sz_slot_windowed_size).val);
    const meta::GenericTuple& tuple_fullscreen = std::get<meta::GenericTuple>(
        ptr_graphic_settings_->Record().Slot(sz_slot_windowed_size - 1).val);
    const meta::GenericTuple& tuple_fullscreen_orig = std::get<meta::GenericTuple>(
        OriginalGraphicSnapshot().Slot(sz_slot_windowed_size - 1).val);
    const bool b_windowed_same =
        tuple_windowed.arr_ints[0] == tuple_windowed_orig.arr_ints[0] &&
        tuple_windowed.arr_ints[1] == tuple_windowed_orig.arr_ints[1];
    const bool b_fullscreen_same =
        tuple_fullscreen.arr_ints[0] == tuple_fullscreen_orig.arr_ints[0] &&
        tuple_fullscreen.arr_ints[1] == tuple_fullscreen_orig.arr_ints[1];
    return ptr_graphic_settings_->GetInt(sz_slot_mode) !=
               SnapshotInt(OriginalGraphicSnapshot(), sz_slot_mode) ||
           ptr_graphic_settings_->GetInt(sz_slot_display) !=
               SnapshotInt(OriginalGraphicSnapshot(), sz_slot_display) ||
           !b_windowed_same || !b_fullscreen_same ||
           ptr_graphic_settings_->GetInt(sz_slot_gl_profile) !=
               SnapshotInt(OriginalGraphicSnapshot(), sz_slot_gl_profile);
  };
}

std::function<void()> DisplaySettingsLogic::ResetPanel(ora::ui::Widget& panel) {
  (void)panel;
  const auto fn_reset = [this] {
    const auto fn_set_bool = [this](std::string_view str_pref, bool b_value) {
      ptr_graphic_settings_->SetBool(SlotOf(*ptr_graphic_settings_, str_pref),
                                     b_value);
    };
    const auto fn_set_int = [this](std::string_view str_pref,
                                   std::int64_t int8_value) {
      ptr_graphic_settings_->SetInt(SlotOf(*ptr_graphic_settings_, str_pref),
                                    int8_value);
    };

    fn_set_bool("CapFramerate", false);
    fn_set_int("MaxFramerate", 60);
    fn_set_bool("CapFramerateToGameFps", false);
    fn_set_int("GLProfile", 0);
    fn_set_int("Mode", 2);
    fn_set_int("VideoDisplay", 0);
    ptr_graphic_settings_->SetInt2(
        SlotOf(*ptr_graphic_settings_, "WindowedSize"), int2{1024, 768});
    fn_set_bool("CursorDouble", false);
    fn_set_int("ViewportDistance", 2);

    const std::size_t sz_slot_ui_scale =
        SlotOf(*ptr_graphic_settings_, "UIScale");
    if (ptr_graphic_settings_->GetFloat(sz_slot_ui_scale) != 1.0f) {
      const float fp4_old_scale =
          ptr_graphic_settings_->GetFloat(sz_slot_ui_scale);
      ptr_graphic_settings_->SetFloat(sz_slot_ui_scale, 1.0f);
      if (gfx::Renderer* ptr_renderer =
              ora::ui::Ui::Instance().RendererDeps())
        ptr_renderer->SetUIScale(1.0f);
      RecalculateWidgetLayoutRoot();
      gfx::Viewport::int2_last_mouse_pos = int2{
          static_cast<std::int32_t>(
              static_cast<float>(gfx::Viewport::int2_last_mouse_pos.X) *
              fp4_old_scale),
          static_cast<std::int32_t>(
              static_cast<float>(gfx::Viewport::int2_last_mouse_pos.Y) *
              fp4_old_scale)};
    }

    ptr_game_settings_->SetInt(
        SlotOf(*ptr_game_settings_, "TextNotificationPoolFilters"), 3);
  };
  return fn_reset;
}

GameplaySettingsLogic::GameplaySettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label,
    ora::gfx::WorldRenderer* ptr_world_renderer) {
  ptr_mod_data_ = &mod_data;
  ptr_world_renderer_ = ptr_world_renderer;

  game::Settings& settings_instance = game::Settings::Instance();
  ptr_auto_save_settings_ = settings_instance.ModuleOrCreate("AutoSave");
  ptr_game_settings_ = settings_instance.Module("Game");
  ptr_player_settings_ = settings_instance.Module("Player");

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });
}

GameplaySettingsLogic::~GameplaySettingsLogic() {
  if (fn_unsubscribe_state_changed_)
    fn_unsubscribe_state_changed_();
}

void GameplaySettingsLogic::UpdateForumAuthFields() {
  game::LocalPlayerProfileState& profile_state =
      game::LocalPlayerProfileInstance();
  const game::LinkState kind_state = profile_state.kind_state;
  if (kind_state == game::LinkState::Linked) {
    if (ptr_profile_username_textfield_ != nullptr)
      ptr_profile_username_textfield_->SetText(
          profile_state.profile_data.str_profile_name);
    if (ptr_profile_password_textfield_ != nullptr)
      ptr_profile_password_textfield_->SetText("");
  } else if (kind_last_state_ == game::LinkState::Linked) {
    if (ptr_profile_username_textfield_ != nullptr)
      ptr_profile_username_textfield_->SetText("");
  }

  kind_last_state_ = kind_state;
}

std::function<bool()> GameplaySettingsLogic::InitPanel(ora::ui::Widget& panel) {
  auto* ptr_scroll_panel =
      panel.Get<ora::ui::ScrollPanelWidget>("SETTINGS_SCROLLPANEL");
  const bool b_is_shellmap =
      ptr_world_renderer_ != nullptr &&
      ptr_world_renderer_->World().Type() == sim::WorldType::Shellmap;

  const std::size_t sz_slot_name = SlotOf(*ptr_player_settings_, "Name");
  const std::size_t sz_slot_color = SlotOf(*ptr_player_settings_, "Color");

  auto ptr_esc_pressed = std::make_shared<bool>(false);
  ptr_name_textfield_ = panel.Get<ora::ui::TextFieldWidget>("PLAYERNAME");
  ptr_name_textfield_->fn_is_disabled = [b_is_shellmap] {
    return !b_is_shellmap;
  };
  ptr_name_textfield_->SetText(net::SanitizedPlayerName(
      ptr_player_settings_->GetString(sz_slot_name), "Commander"));
  ptr_name_textfield_->fn_on_lose_focus =
      [this, ptr_esc_pressed, sz_slot_name]() {
        if (*ptr_esc_pressed) {
          *ptr_esc_pressed = false;
          return;
        }

        std::string str_text = ptr_name_textfield_->Text();
        str_text.erase(0, str_text.find_first_not_of(" \t\r\n"));
        const std::size_t sz_end = str_text.find_last_not_of(" \t\r\n");
        str_text.erase(sz_end == std::string::npos ? 0 : sz_end + 1);
        ptr_name_textfield_->SetText(str_text);
        if (str_text.empty()) {
          ptr_name_textfield_->SetText(net::SanitizedPlayerName(
              ptr_player_settings_->GetString(sz_slot_name), "Commander"));
        } else {
          ptr_name_textfield_->SetText(
              net::SanitizedPlayerName(str_text, "Commander"));
          ptr_player_settings_->SetString(
              sz_slot_name, ptr_name_textfield_->Text());
        }
      };
  ptr_name_textfield_->fn_on_enter_key = [this](const KeyInput&) {
    ptr_name_textfield_->YieldKeyboardFocus();
    return true;
  };
  ptr_name_textfield_->fn_on_esc_key =
      [this, ptr_esc_pressed, sz_slot_name](const KeyInput&) {
        ptr_name_textfield_->SetText(net::SanitizedPlayerName(
            ptr_player_settings_->GetString(sz_slot_name), "Commander"));
        *ptr_esc_pressed = true;
        ptr_name_textfield_->YieldKeyboardFocus();
        return true;
      };

  if (auto* ptr_color_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>("PLAYERCOLOR")) {
    ptr_color_dropdown->fn_is_disabled = [b_is_shellmap] {
      return !b_is_shellmap;
    };
    ptr_color_dropdown->fn_on_mouse_down = [this, sz_slot_color,
                                            ptr_color_dropdown](
                                                   const MouseInput&) {
      const core::Color color_current =
          core::Color::FromArgbRaw(static_cast<std::uint32_t>(
              ptr_player_settings_->GetInt(sz_slot_color)));
      ora::ui::Ui::Instance().ShowColorDropDownDeps(
          *ptr_color_dropdown, color_current, nullptr, ptr_world_renderer_,
          [this, sz_slot_color](core::Color color_new) {
            ptr_player_settings_->SetInt(
                sz_slot_color,
                static_cast<std::int64_t>(color_new.ToArgb()));
            ora::ui::Ui::Instance().SettingsSaveDeps();
          });
    };
    if (auto* ptr_color_block =
            ptr_color_dropdown->GetOrNull<ora::ui::ColorBlockWidget>(
                "COLORBLOCK"))
      ptr_color_block->fn_get_color = [this, sz_slot_color] {
        return core::Color::FromArgbRaw(static_cast<std::uint32_t>(
            ptr_player_settings_->GetInt(sz_slot_color)));
      };
  }

  BindCheckboxPref(panel, "HIDE_REPLAY_CHAT_CHECKBOX", *ptr_game_settings_,
                   "HideReplayChat");

  game::LocalPlayerProfileState& profile_state =
      game::LocalPlayerProfileInstance();
  ptr_profile_username_textfield_ =
      panel.Get<ora::ui::TextFieldWidget>("USERNAME");
  ptr_profile_username_textfield_->fn_is_disabled = [] {
    return game::LocalPlayerProfileInstance().kind_state !=
           game::LinkState::Unlinked;
  };

  ptr_profile_password_textfield_ =
      panel.GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD");
  if (ptr_profile_password_textfield_ != nullptr)
    ptr_profile_password_textfield_->fn_is_disabled =
        ptr_profile_username_textfield_->fn_is_disabled;

  fn_unsubscribe_state_changed_ = game::ProfileAddStateChangedHandler(
      profile_state, [this] { UpdateForumAuthFields(); });
  UpdateForumAuthFields();

  if (auto* ptr_checking_fingerprint =
          panel.GetOrNull<ora::ui::LabelWidget>("CHECKING_FINGERPRINT"))
    ptr_checking_fingerprint->fn_is_visible = [] {
      return game::LocalPlayerProfileInstance().kind_state ==
             game::LinkState::CheckingLink;
    };

  if (auto* ptr_connection_error =
          panel.GetOrNull<ora::ui::LabelWidget>("CONNECTION_ERROR"))
    ptr_connection_error->fn_is_visible = [] {
      return game::LocalPlayerProfileInstance().kind_state ==
             game::LinkState::ConnectionFailed;
    };

  const std::vector<std::pair<game::LinkResult, std::string>> vec_result_labels{
      {game::LinkResult::AuthFailure, Msg(kLabelForumProfileResultAuthFailure)},
      {game::LinkResult::LoginAttempts,
       Msg(kLabelForumProfileResultLoginAttempts)},
      {game::LinkResult::Banned, Msg(kLabelForumProfileResultBanned)},
      {game::LinkResult::Error, Msg(kLabelForumProfileResultError)},
      {game::LinkResult::ConnectionFailed,
       Msg(kLabelForumProfileResultConnectionFailed)},
  };
  const std::string str_result_linked = Msg(kLabelForumProfileResultLinked);
  const std::string str_result_unlinked =
      Msg(kLabelForumProfileResultUnlinked);
  auto ptr_link_result =
      std::make_shared<game::LinkResult>(game::LinkResult::Success);

  if (auto* ptr_profile_status = panel.GetOrNull<ora::ui::LabelWidget>(
          "PROFILE_STATUS")) {
    ptr_profile_status->fn_is_visible = [] {
      const game::LinkState kind_state =
          game::LocalPlayerProfileInstance().kind_state;
      return kind_state != game::LinkState::ConnectionFailed &&
             kind_state != game::LinkState::CheckingLink;
    };
    ptr_profile_status->fn_get_text =
        [ptr_link_result, str_result_linked, str_result_unlinked,
         vec_result_labels]() {
          for (const auto& [kind_result, str_label] : vec_result_labels)
            if (kind_result == *ptr_link_result)
              return str_label;
          return game::LocalPlayerProfileInstance().kind_state ==
                         game::LinkState::Linked
                     ? str_result_linked
                     : str_result_unlinked;
        };
  }

  const std::string str_link_label = Msg(kButtonForumProfileLink);
  const std::string str_unlink_label = Msg(kButtonForumProfileUnlink);
  if (auto* ptr_link_button =
          panel.GetOrNull<ora::ui::ButtonWidget>("LINK_BUTTON")) {
    ptr_link_button->fn_is_visible = [] {
      return game::LocalPlayerProfileInstance().kind_state !=
             game::LinkState::ConnectionFailed;
    };
    ptr_link_button->fn_is_disabled = [this]() {
      const game::LinkState kind_state =
          game::LocalPlayerProfileInstance().kind_state;
      if (kind_state == game::LinkState::Unlinked)
        return IsNullOrSpace(ptr_profile_username_textfield_->Text()) ||
               (ptr_profile_password_textfield_ != nullptr &&
                IsNullOrSpace(ptr_profile_password_textfield_->Text()));
      return kind_state != game::LinkState::Linked;
    };
    ptr_link_button->fn_get_text = [str_link_label, str_unlink_label] {
      return game::LocalPlayerProfileInstance().kind_state ==
                     game::LinkState::Unlinked
                 ? str_link_label
                 : str_unlink_label;
    };
    ptr_link_button->fn_on_click = [this, ptr_link_result]() {
      game::LocalPlayerProfileState& state_profile =
          game::LocalPlayerProfileInstance();
      if (state_profile.kind_state == game::LinkState::Linked) {
        game::ProfileDeleteKeypair(state_profile);
      } else if (state_profile.kind_state == game::LinkState::Unlinked) {
        game::ProfileLinkForumAccount(
            state_profile, ptr_profile_username_textfield_->Text(),
            ptr_profile_password_textfield_ != nullptr
                ? ptr_profile_password_textfield_->Text()
                : std::string{},
            [ptr_link_result](game::LinkResult kind_result) {
              *ptr_link_result = kind_result;
            });
      }
    };
  }

  if (auto* ptr_retry_button =
          panel.GetOrNull<ora::ui::ButtonWidget>("RETRY_BUTTON")) {
    ptr_retry_button->fn_is_visible = [] {
      return game::LocalPlayerProfileInstance().kind_state ==
             game::LinkState::ConnectionFailed;
    };
    ptr_retry_button->fn_on_click = [] {
      game::ProfileRefreshPlayerData(game::LocalPlayerProfileInstance());
    };
  }

  if (auto* ptr_forum_button =
          panel.GetOrNull<ora::ui::ButtonWidget>("FORUM_BUTTON")) {
    const std::string str_forum_url = ptr_mod_data_->GetOrCreatePlayerDatabase().str_forum;
    ptr_forum_button->fn_on_click = [str_forum_url]() {
      if (ora::ui::Ui::HasInstance()) {
        if (gfx::Renderer* ptr_renderer =
                ora::ui::Ui::Instance().RendererDeps())
          ptr_renderer->TryOpenUrl(str_forum_url);
      }
    };
  }

  const std::size_t sz_slot_interval =
      SlotOf(*ptr_auto_save_settings_, "AutoSaveInterval");
  const std::size_t sz_slot_file_count =
      SlotOf(*ptr_auto_save_settings_, "AutoSaveMaxFileCount");

  if (auto* ptr_interval_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "AUTO_SAVE_INTERVAL_DROP_DOWN")) {
    ptr_interval_dropdown_ = ptr_interval_dropdown;
    ptr_interval_dropdown->fn_on_click = [this]() {
      ShowAutoSaveIntervalDropdown();
    };
    ptr_interval_dropdown->fn_get_text = [this, sz_slot_interval] {
      return GetAutoSaveIntervalText(
          static_cast<int>(ptr_auto_save_settings_->GetInt(sz_slot_interval)));
    };
  }

  if (auto* ptr_file_number_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "AUTO_SAVE_FILE_NUMBER_DROP_DOWN")) {
    ptr_file_number_dropdown_ = ptr_file_number_dropdown;
    ptr_file_number_dropdown->fn_on_mouse_down =
        [this](const MouseInput&) {
          ShowAutoSaveFileNumberDropdown(*ptr_file_number_dropdown_);
        };
    ptr_file_number_dropdown->fn_get_text =
        [this, sz_slot_file_count] {
          return MsgArg(kAutoSaveMaxFileNumber, "saves",
                        FluentValue{
                            ptr_auto_save_settings_->GetInt(sz_slot_file_count)});
        };
    ptr_file_number_dropdown->fn_is_disabled = [this, sz_slot_interval] {
      return ptr_auto_save_settings_->GetInt(sz_slot_interval) <= 0;
    };
  }

  AdjustSettingsScrollPanelLayout(*ptr_scroll_panel);

  return [this]() {
    if (ptr_name_textfield_ != nullptr)
      ptr_name_textfield_->YieldKeyboardFocus();
    return false;
  };
}

std::function<void()> GameplaySettingsLogic::ResetPanel(ora::ui::Widget& panel) {
  const auto fn_reset = [this, &panel] {
    const std::size_t sz_slot_name = SlotOf(*ptr_player_settings_, "Name");
    const std::size_t sz_slot_color = SlotOf(*ptr_player_settings_, "Color");
    const std::size_t sz_slot_interval =
        SlotOf(*ptr_auto_save_settings_, "AutoSaveInterval");
    const std::size_t sz_slot_file_count =
        SlotOf(*ptr_auto_save_settings_, "AutoSaveMaxFileCount");
    const std::size_t sz_slot_hide_chat =
        SlotOf(*ptr_game_settings_, "HideReplayChat");

    ptr_player_settings_->SetString(
        sz_slot_name, ptr_player_settings_->DefaultString(sz_slot_name));
    if (ptr_name_textfield_ != nullptr)
      ptr_name_textfield_->SetText(ptr_player_settings_->GetString(sz_slot_name));
    ptr_player_settings_->SetInt(
        sz_slot_color, ptr_player_settings_->DefaultInt(sz_slot_color));
    ptr_auto_save_settings_->SetInt(
        sz_slot_interval, ptr_auto_save_settings_->DefaultInt(sz_slot_interval));
    ptr_auto_save_settings_->SetInt(
        sz_slot_file_count,
        ptr_auto_save_settings_->DefaultInt(sz_slot_file_count));
    ptr_game_settings_->SetBool(
        sz_slot_hide_chat, ptr_game_settings_->DefaultBool(sz_slot_hide_chat));
    (void)panel;
  };
  return fn_reset;
}

void GameplaySettingsLogic::ShowAutoSaveIntervalDropdown() {
  if (ptr_interval_dropdown_ == nullptr)
    return;
  const std::size_t sz_slot =
      SlotOf(*ptr_auto_save_settings_, "AutoSaveInterval");

  ptr_interval_dropdown_->ShowDropDown<int>(
      "LABEL_DROPDOWN_TEMPLATE", 500,
      std::vector<int>(kAutoSaveSeconds.begin(), kAutoSaveSeconds.end()),
      [this, sz_slot](const int& int4_option,
                      ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [this, sz_slot, int4_option] {
              return ptr_auto_save_settings_->GetInt(sz_slot) == int4_option;
            },
            [this, sz_slot, int4_option] {
              ptr_auto_save_settings_->SetInt(sz_slot, int4_option);
              ora::ui::Ui::Instance().SettingsSaveDeps();
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = GetAutoSaveIntervalText(int4_option);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void GameplaySettingsLogic::ShowAutoSaveFileNumberDropdown(
    ora::ui::DropDownButtonWidget& dropdown) {
  const std::size_t sz_slot =
      SlotOf(*ptr_auto_save_settings_, "AutoSaveMaxFileCount");

  dropdown.ShowDropDown<int>(
      "LABEL_DROPDOWN_TEMPLATE", 500,
      std::vector<int>(kAutoSaveFileNumbers.begin(), kAutoSaveFileNumbers.end()),
      [this, sz_slot](const int& int4_option,
                      ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [this, sz_slot, int4_option] {
              return ptr_auto_save_settings_->GetInt(sz_slot) == int4_option;
            },
            [this, sz_slot, int4_option] {
              ptr_auto_save_settings_->SetInt(sz_slot, int4_option);
              ora::ui::Ui::Instance().SettingsSaveDeps();
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label =
            MsgArg(kAutoSaveMaxFileNumber, "saves",
                   FluentValue{static_cast<std::int64_t>(int4_option)});
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

HotkeysSettingsLogic::HotkeysSettingsLogic(
    game::ModData& mod_data, ISettingsLogic& settings_logic,
    const std::string& str_panel_id, const std::string& str_label,
    const ora::ui::YamlDictionary* ptr_logic_args) {
  ptr_mod_data_ = &mod_data;
  ptr_logic_args_ = ptr_logic_args;
  str_current_context_ = std::string{
      ora::ui::HotkeyDefinition::kContextFluentPrefix} +
      "-any";
  vec_contexts_.push_back(str_current_context_);

  settings_logic.RegisterSettingsPanel(str_panel_id, str_label,
                                        [this](ora::ui::Widget& panel) {
                                          return InitPanel(panel);
                                        },
                                        [this](ora::ui::Widget& panel) {
                                          return ResetPanel(panel);
                                        });
}

void HotkeysSettingsLogic::BindHotkeyPref(
    const ora::ui::HotkeyDefinition& hd, ora::ui::Widget& template_widget) {
  std::unique_ptr<ora::ui::Widget> up_key = template_widget.Clone();
  up_key->str_id = hd.str_name;
  up_key->fn_is_visible = [] { return true; };

  const std::string str_desc = Msg(hd.str_description) + ":";
  if (auto* ptr_function_label = up_key->GetOrNull<ora::ui::LabelWidget>(
          "FUNCTION"))
    ptr_function_label->fn_get_text = [str_desc] { return str_desc; };

  auto* ptr_remap_button = up_key->Get<ora::ui::ButtonWidget>("HOTKEY");
  ora::ui::TruncateButtonToTooltip(
      *ptr_remap_button,
      ptr_mod_data_->Hotkeys().Get(hd.str_name).GetValue().DisplayString());

  ptr_remap_button->fn_is_highlighted =
      [this, &hd] { return ptr_selected_hotkey_definition_ == &hd; };

  ora::ui::ChromeMetrics* ptr_metrics =
      ora::ui::Ui::HasInstance()
          ? ora::ui::Ui::Instance().ChromeMetricsDeps()
          : nullptr;
  core::Color color_valid{0xFFFFFFFF};
  core::Color color_invalid{0xFFFFFFFF};
  if (ptr_metrics != nullptr) {
    color_valid = ptr_metrics->GetColor("HotkeyColor");
    color_invalid = ptr_metrics->GetColor("HotkeyColorInvalid");
  }
  ptr_remap_button->fn_get_color = [&hd, color_valid, color_invalid] {
    return hd.b_has_duplicates ? color_invalid : color_valid;
  };

  if (ptr_selected_hotkey_definition_ == &hd) {
    ptr_selected_hotkey_button_ = ptr_remap_button;
    ptr_hotkey_entry_widget_->key_hotkey =
        ptr_mod_data_->Hotkeys().Get(hd.str_name).GetValue();
    ValidateHotkey();
  }

  const std::string str_name = hd.str_name;
  ptr_remap_button->fn_on_click = [this, str_name, ptr_remap_button]() {
    const ora::ui::HotkeyDefinition* ptr_definition = nullptr;
    for (const auto& [str_key, definition] :
         ptr_mod_data_->Hotkeys().Definitions())
      if (str_key == str_name)
        ptr_definition = &definition;
    if (ptr_definition == nullptr)
      return;
    ptr_selected_hotkey_definition_ = ptr_definition;
    ptr_selected_hotkey_button_ = ptr_remap_button;
    ptr_hotkey_entry_widget_->key_hotkey =
        ptr_mod_data_->Hotkeys().Get(str_name).GetValue();
    ValidateHotkey();

    if (ptr_definition->b_readonly)
      ptr_hotkey_entry_widget_->YieldKeyboardFocus();
    else
      ptr_hotkey_entry_widget_->TakeKeyboardFocus();
  };

  ptr_hotkey_list_->AddChild(std::move(up_key));
}

std::function<bool()> HotkeysSettingsLogic::InitPanel(ora::ui::Widget& panel) {
  ptr_hotkey_list_ = panel.Get<ora::ui::ScrollPanelWidget>("HOTKEY_LIST");
  ptr_hotkey_list_->ptr_layout =
      std::make_unique<ora::ui::GridLayout>(*ptr_hotkey_list_);
  if (ora::ui::Widget* ptr_header =
          ptr_hotkey_list_->GetOrNull("HEADER"))
    up_header_template_ = ptr_hotkey_list_->RemoveChild(*ptr_header);
  if (ora::ui::Widget* ptr_item = ptr_hotkey_list_->GetOrNull("TEMPLATE"))
    up_template_ = ptr_hotkey_list_->RemoveChild(*ptr_item);
  ptr_empty_list_message_ = panel.GetOrNull("HOTKEY_EMPTY_LIST");
  ptr_remap_dialog_ = panel.GetOrNull("HOTKEY_REMAP_DIALOG");

  for (const auto& [str_key, definition] :
       ptr_mod_data_->Hotkeys().Definitions())
    for (const std::string& str_context : definition.vec_contexts)
      if (std::find(vec_contexts_.begin(), vec_contexts_.end(), str_context) ==
          vec_contexts_.end())
        vec_contexts_.push_back(str_context);

  ptr_filter_input_ = panel.Get<ora::ui::TextFieldWidget>("FILTER_INPUT");
  ptr_filter_input_->fn_on_text_edited = [this] { InitHotkeyList(); };
  ptr_filter_input_->fn_on_esc_key = [this](const KeyInput&) {
    if (ptr_filter_input_->Text().empty()) {
      ptr_filter_input_->YieldKeyboardFocus();
    } else {
      ptr_filter_input_->SetText("");
      InitHotkeyList();
    }
    return true;
  };

  if (auto* ptr_context_dropdown =
          panel.GetOrNull<ora::ui::DropDownButtonWidget>(
              "CONTEXT_DROPDOWN")) {
    ptr_context_dropdown_ = ptr_context_dropdown;
    ptr_context_dropdown->fn_on_mouse_down = [this](const MouseInput&) {
      ShowContextDropdown();
    };
    auto cache_context_name =
        std::make_shared<core::CachedTransform<std::string, std::string>>(
            [](const std::string& str_context) {
              return GetContextDisplayName(str_context);
            });
    ptr_context_dropdown->fn_get_text = [this, cache_context_name] {
      return cache_context_name->Update(str_current_context_);
    };
  }

  const yaml::MiniYaml* ptr_args_groups = nullptr;
  if (ptr_logic_args_ != nullptr)
    for (const auto& [str_key, ptr_yaml] : *ptr_logic_args_)
      if (str_key == "HotkeyGroups")
        ptr_args_groups = ptr_yaml;

  if (ptr_args_groups != nullptr) {
    for (const auto& node_group : ptr_args_groups->Nodes) {
      if (node_group.Key == nullptr)
        continue;
      const yaml::MiniYamlNode* ptr_types_node =
          node_group.Value.NodeWithKeyOrDefault("Types");
      if (ptr_types_node == nullptr ||
          ptr_types_node->Value.Value == nullptr)
        continue;
      std::vector<std::string> vec_types;
      for (const std::string_view sv_part :
           SplitCommaList(std::string_view{*ptr_types_node->Value.Value}))
        vec_types.emplace_back(sv_part);
      vec_hotkey_groups_.emplace_back(*node_group.Key,
                                      std::move(vec_types));
    }

    InitHotkeyRemapDialog(panel);
    InitHotkeyList();
  }

  return [this]() {
    if (ptr_hotkey_entry_widget_ != nullptr) {
      ptr_hotkey_entry_widget_->key_hotkey =
          ptr_selected_hotkey_definition_ != nullptr
              ? ptr_mod_data_->Hotkeys()
                    .Get(ptr_selected_hotkey_definition_->str_name)
                    .GetValue()
              : ora::ui::Hotkey::Invalid();
      ptr_hotkey_entry_widget_->ForceYieldKeyboardFocus();
    }
    return false;
  };
}

std::function<void()> HotkeysSettingsLogic::ResetPanel(ora::ui::Widget& panel) {
  const auto fn_reset = [this, &panel]() {
    for (const auto& [str_key, definition] :
         ptr_mod_data_->Hotkeys().Definitions()) {
      ptr_mod_data_->Hotkeys().Set(str_key, definition.hk_default);
      if (ora::ui::Widget* ptr_hotkey_widget = panel.GetOrNull(str_key))
        if (auto* ptr_hotkey_button =
                ptr_hotkey_widget->GetOrNull<ora::ui::ButtonWidget>("HOTKEY"))
          ora::ui::TruncateButtonToTooltip(
              *ptr_hotkey_button, definition.hk_default.DisplayString());
    }
  };
  return fn_reset;
}

void HotkeysSettingsLogic::InitHotkeyList() {
  ptr_hotkey_list_->RemoveChildren();
  ptr_selected_hotkey_definition_ = nullptr;

  for (const auto& [str_group_name, vec_types_in_group] : vec_hotkey_groups_) {
    std::vector<const ora::ui::HotkeyDefinition*> vec_keys_in_group;
    for (const auto& [str_key, definition] :
         ptr_mod_data_->Hotkeys().Definitions())
      if (IsHotkeyVisibleInFilter(definition) &&
          OverlapsAny(definition.vec_types, vec_types_in_group))
        vec_keys_in_group.push_back(&definition);

    if (vec_keys_in_group.empty())
      continue;

    if (up_header_template_ != nullptr) {
      std::unique_ptr<ora::ui::Widget> up_header =
          up_header_template_->Clone();
      if (auto* ptr_label = up_header->GetOrNull<ora::ui::LabelWidget>("LABEL")) {
        const std::string str_group_label = Msg(str_group_name);
        ptr_label->fn_get_text = [str_group_label] {
          return str_group_label;
        };
      }
      ptr_hotkey_list_->AddChild(std::move(up_header));
    }

    std::vector<const ora::ui::HotkeyDefinition*> vec_added;
    for (const std::string& str_type : vec_types_in_group) {
      for (const ora::ui::HotkeyDefinition* ptr_definition :
           vec_keys_in_group) {
        if (std::find(ptr_definition->vec_types.begin(),
                      ptr_definition->vec_types.end(),
                      str_type) == ptr_definition->vec_types.end())
          continue;
        if (std::find(vec_added.begin(), vec_added.end(), ptr_definition) !=
            vec_added.end())
          continue;
        vec_added.push_back(ptr_definition);
        if (ptr_selected_hotkey_definition_ == nullptr)
          ptr_selected_hotkey_definition_ = ptr_definition;

        if (up_template_ != nullptr)
          BindHotkeyPref(*ptr_definition, *up_template_);
      }
    }
  }

  if (ptr_empty_list_message_ != nullptr)
    ptr_empty_list_message_->b_visible =
        ptr_selected_hotkey_definition_ == nullptr;
  if (ptr_remap_dialog_ != nullptr)
    ptr_remap_dialog_->b_visible =
        ptr_selected_hotkey_definition_ != nullptr;

  ptr_hotkey_list_->ScrollToTop();
}

void HotkeysSettingsLogic::InitHotkeyRemapDialog(ora::ui::Widget& panel) {
  auto* ptr_label = panel.Get<ora::ui::LabelWidget>("HOTKEY_LABEL");
  auto cache_label_text =
      std::make_shared<core::CachedTransform<const ora::ui::HotkeyDefinition*,
                                              std::string>>(
          [](const ora::ui::HotkeyDefinition* const& ptr_hd) {
            return (ptr_hd != nullptr ? Msg(ptr_hd->str_description)
                                      : std::string{}) +
                   ":";
          });
  ptr_label->fn_is_visible = [this] {
    return ptr_selected_hotkey_definition_ != nullptr;
  };
  ptr_label->fn_get_text = [this, cache_label_text] {
    return cache_label_text->Update(ptr_selected_hotkey_definition_);
  };

  auto* ptr_duplicate_notice =
      panel.Get<ora::ui::LabelWidget>("DUPLICATE_NOTICE");
  if (ora::ui::ChromeMetrics* ptr_metrics_notice =
          ora::ui::Ui::Instance().ChromeMetricsDeps())
    ptr_duplicate_notice->color_text =
        ptr_metrics_notice->GetColor("NoticeErrorColor");
  ptr_duplicate_notice->fn_is_visible = [this] { return !b_is_hotkey_valid_; };
  auto cache_duplicate_text =
      std::make_shared<core::CachedTransform<
          const ora::ui::HotkeyDefinition*, std::string>>(
          [this](const ora::ui::HotkeyDefinition* const& ptr_hd) {
            if (ptr_hd == nullptr)
              return std::string{};
            std::string str_context;
            for (const std::string& str_candidate :
                 ptr_hd->vec_contexts) {
              if (ptr_selected_hotkey_definition_ != nullptr &&
                  std::find(ptr_selected_hotkey_definition_->vec_contexts
                                .begin(),
                            ptr_selected_hotkey_definition_->vec_contexts.end(),
                            str_candidate) !=
                      ptr_selected_hotkey_definition_->vec_contexts.end()) {
                str_context = str_candidate;
                break;
              }
            }
            return MsgArg2(kDuplicateNotice, "key",
                           FluentValue{Msg(ptr_hd->str_description)},
                           "context", FluentValue{Msg(str_context)});
          });
  ptr_duplicate_notice->fn_get_text = [this, cache_duplicate_text] {
    return cache_duplicate_text->Update(ptr_duplicate_hotkey_definition_);
  };

  auto* ptr_original_notice =
      panel.Get<ora::ui::LabelWidget>("ORIGINAL_NOTICE");
  if (ora::ui::ChromeMetrics* ptr_metrics_notice =
          ora::ui::Ui::Instance().ChromeMetricsDeps())
    ptr_original_notice->color_text =
        ptr_metrics_notice->GetColor("NoticeInfoColor");
  ptr_original_notice->fn_is_visible = [this] {
    return b_is_hotkey_valid_ && !b_is_hotkey_default_;
  };
  auto cache_original_text =
      std::make_shared<core::CachedTransform<
          const ora::ui::HotkeyDefinition*, std::string>>(
          [](const ora::ui::HotkeyDefinition* const& ptr_hd) {
            return MsgArg(kOriginalNotice, "key",
                          FluentValue{ptr_hd != nullptr
                                          ? ptr_hd->hk_default.DisplayString()
                                          : std::string{}});
          });
  ptr_original_notice->fn_get_text = [this, cache_original_text] {
    return cache_original_text->Update(ptr_selected_hotkey_definition_);
  };

  auto* ptr_readonly_notice =
      panel.Get<ora::ui::LabelWidget>("READONLY_NOTICE");
  if (ora::ui::ChromeMetrics* ptr_metrics_notice =
          ora::ui::Ui::Instance().ChromeMetricsDeps())
    ptr_readonly_notice->color_text =
        ptr_metrics_notice->GetColor("NoticeInfoColor");
  ptr_readonly_notice->fn_is_visible = [this] {
    return ptr_selected_hotkey_definition_ != nullptr &&
           ptr_selected_hotkey_definition_->b_readonly;
  };

  auto* ptr_reset_button = panel.Get<ora::ui::ButtonWidget>("RESET_HOTKEY_BUTTON");
  ptr_reset_button->fn_is_disabled = [this] {
    return b_is_hotkey_default_ ||
           (ptr_selected_hotkey_definition_ != nullptr &&
            ptr_selected_hotkey_definition_->b_readonly);
  };
  ptr_reset_button->fn_on_click = [this]() {
    if (ptr_selected_hotkey_definition_ == nullptr)
      return;
    ptr_hotkey_entry_widget_->key_hotkey =
        ptr_selected_hotkey_definition_->hk_default;
    ptr_hotkey_entry_widget_->YieldKeyboardFocus();
  };

  auto* ptr_clear_button = panel.Get<ora::ui::ButtonWidget>("CLEAR_HOTKEY_BUTTON");
  ptr_clear_button->fn_is_disabled = [this] {
    return (ptr_selected_hotkey_definition_ != nullptr &&
            ptr_selected_hotkey_definition_->b_readonly) ||
           (ptr_hotkey_entry_widget_ != nullptr &&
            !ptr_hotkey_entry_widget_->key_hotkey.IsValid());
  };
  ptr_clear_button->fn_on_click = [this]() {
    if (ptr_hotkey_entry_widget_ == nullptr)
      return;
    ptr_hotkey_entry_widget_->key_hotkey = ora::ui::Hotkey::Invalid();
    ptr_hotkey_entry_widget_->YieldKeyboardFocus();
  };

  auto* ptr_override_button =
      panel.Get<ora::ui::ButtonWidget>("OVERRIDE_HOTKEY_BUTTON");
  ptr_override_button->fn_is_disabled = [this] { return b_is_hotkey_valid_; };
  ptr_override_button->fn_is_visible = [this] {
    return !b_is_hotkey_valid_ && ptr_duplicate_hotkey_definition_ != nullptr &&
           !ptr_duplicate_hotkey_definition_->b_readonly;
  };
  ptr_override_button->fn_on_click = [this]() {
    if (ptr_duplicate_hotkey_definition_ == nullptr)
      return;
    if (ora::ui::Widget* ptr_duplicate_widget =
            ptr_hotkey_list_->GetOrNull(
                ptr_duplicate_hotkey_definition_->str_name))
      if (auto* ptr_duplicate_button =
              ptr_duplicate_widget->GetOrNull<ora::ui::ButtonWidget>("HOTKEY"))
        ora::ui::TruncateButtonToTooltip(
            *ptr_duplicate_button,
            ora::ui::Hotkey::Invalid().DisplayString());
    ptr_mod_data_->Hotkeys().Set(
        ptr_duplicate_hotkey_definition_->str_name,
        ora::ui::Hotkey::Invalid());
    ptr_mod_data_->Hotkeys().Save();
    ptr_hotkey_entry_widget_->YieldKeyboardFocus();
  };

  ptr_hotkey_entry_widget_ =
      panel.Get<ora::ui::HotkeyEntryWidget>("HOTKEY_ENTRY");
  ptr_hotkey_entry_widget_->fn_is_valid = [this] { return b_is_hotkey_valid_; };
  ptr_hotkey_entry_widget_->fn_on_lose_focus = [this] { ValidateHotkey(); };
  ptr_hotkey_entry_widget_->fn_on_esc_key =
      [this](const KeyInput&) {
        if (ptr_selected_hotkey_definition_ != nullptr)
          ptr_hotkey_entry_widget_->key_hotkey =
              ptr_mod_data_->Hotkeys()
                  .Get(ptr_selected_hotkey_definition_->str_name)
                  .GetValue();
      };
  ptr_hotkey_entry_widget_->fn_is_disabled = [this] {
    return ptr_selected_hotkey_definition_ == nullptr ||
           ptr_selected_hotkey_definition_->b_readonly;
  };

  int4_valid_hotkey_entry_width_ =
      ptr_hotkey_entry_widget_->bounds_widget.int4_width;
  int4_invalid_hotkey_entry_width_ =
      int4_valid_hotkey_entry_width_ -
      (ptr_clear_button->bounds_widget.int4_x -
       ptr_override_button->bounds_widget.int4_x);
}

void HotkeysSettingsLogic::ValidateHotkey() {
  if (ptr_selected_hotkey_definition_ == nullptr)
    return;

  ptr_duplicate_hotkey_definition_ = ptr_mod_data_->Hotkeys().GetFirstDuplicate(
      *ptr_selected_hotkey_definition_, ptr_hotkey_entry_widget_->key_hotkey);
  b_is_hotkey_valid_ = ptr_duplicate_hotkey_definition_ == nullptr ||
                       ptr_selected_hotkey_definition_->b_readonly;
  b_is_hotkey_default_ =
      ptr_hotkey_entry_widget_->key_hotkey ==
          ptr_selected_hotkey_definition_->hk_default ||
      (!ptr_hotkey_entry_widget_->key_hotkey.IsValid() &&
       !ptr_selected_hotkey_definition_->hk_default.IsValid());

  if (b_is_hotkey_valid_) {
    ptr_hotkey_entry_widget_->bounds_widget.int4_width =
        int4_valid_hotkey_entry_width_;
    SaveHotkey();
  } else {
    ptr_hotkey_entry_widget_->bounds_widget.int4_width =
        ptr_duplicate_hotkey_definition_->b_readonly
            ? int4_valid_hotkey_entry_width_
            : int4_invalid_hotkey_entry_width_;
    ptr_hotkey_entry_widget_->TakeKeyboardFocus();
  }
}

void HotkeysSettingsLogic::SaveHotkey() {
  if (ptr_selected_hotkey_definition_->b_readonly)
    return;

  if (ptr_selected_hotkey_button_ != nullptr)
    ora::ui::TruncateButtonToTooltip(
        *ptr_selected_hotkey_button_,
        ptr_hotkey_entry_widget_->key_hotkey.DisplayString());
  ptr_mod_data_->Hotkeys().Set(ptr_selected_hotkey_definition_->str_name,
                               ptr_hotkey_entry_widget_->key_hotkey);
  ptr_mod_data_->Hotkeys().Save();
}

bool HotkeysSettingsLogic::IsHotkeyVisibleInFilter(
    const ora::ui::HotkeyDefinition& hd) const {
  const std::string& str_filter = ptr_filter_input_->Text();
  const bool b_filtered_by_name =
      str_filter.empty() || IsNullOrSpace(str_filter) ||
      ContainsIgnoreCase(Msg(hd.str_description), str_filter);
  const bool b_filtered_by_context =
      str_current_context_.empty() ||
      str_current_context_ ==
          std::string{ora::ui::HotkeyDefinition::kContextFluentPrefix} +
              "-any" ||
      std::find(hd.vec_contexts.begin(), hd.vec_contexts.end(),
                str_current_context_) != hd.vec_contexts.end();

  return b_filtered_by_name && b_filtered_by_context;
}

void HotkeysSettingsLogic::ShowContextDropdown() {
  if (ptr_hotkey_entry_widget_ != nullptr)
    ptr_hotkey_entry_widget_->YieldKeyboardFocus();

  if (ptr_context_dropdown_ == nullptr)
    return;

  auto cache_context_name =
      std::make_shared<core::CachedTransform<std::string, std::string>>(
          [](const std::string& str_context) {
            return GetContextDisplayName(str_context);
          });
  ptr_context_dropdown_->ShowDropDown<std::string>(
      "LABEL_DROPDOWN_TEMPLATE", 280, vec_contexts_,
      [this, cache_context_name](const std::string& str_context,
                                 ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [this, &str_context] { return str_current_context_ == str_context; },
            [this, &str_context] {
              str_current_context_ = str_context;
              InitHotkeyList();
            });

        auto* ptr_label = up_item->Get<ora::ui::LabelWidget>("LABEL");
        const std::string str_label = cache_context_name->Update(str_context);
        ptr_label->fn_get_text = [str_label] { return str_label; };
        return up_item;
      });
}

void RegisterSettingsLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();

  registry_logics.Register(
      "SettingsLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void()> fn_on_exit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onExit")))
          fn_on_exit = *ptr_value;

        ora::gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value =
                std::get_if<ora::gfx::WorldRenderer*>(
                    &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;

        const ora::ui::YamlDictionary* ptr_logic_args = nullptr;
        if (const auto* ptr_value =
                std::get_if<const ora::ui::YamlDictionary*>(
                    &args_args.At("logicArgs")))
          ptr_logic_args = *ptr_value;

        return std::make_unique<SettingsLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            std::move(fn_on_exit), ptr_world_renderer,
            *std::get<game::ModData*>(args_args.At("modData")),
            ptr_logic_args);
      });

  const auto fn_args_panel_id = [](ora::ui::WidgetArgs& args_args) {
    std::string str_panel_id;
    if (const auto* ptr_value = std::get_if<std::string>(
            &args_args.At("panelID")))
      str_panel_id = *ptr_value;
    std::string str_label;
    if (const auto* ptr_value = std::get_if<std::string>(
            &args_args.At("label")))
      str_label = *ptr_value;
    return std::pair<std::string, std::string>{std::move(str_panel_id),
                                               std::move(str_label)};
  };

  registry_logics.Register(
      "AudioSettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        ora::gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value =
                std::get_if<ora::gfx::WorldRenderer*>(
                    &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;
        return std::make_unique<AudioSettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label, ptr_world_renderer);
      });

  registry_logics.Register(
      "DisplaySettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        ora::gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value =
                std::get_if<ora::gfx::WorldRenderer*>(
                    &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;
        return std::make_unique<DisplaySettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label, ptr_world_renderer);
      });

  registry_logics.Register(
      "GameplaySettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        ora::gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value =
                std::get_if<ora::gfx::WorldRenderer*>(
                    &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;
        return std::make_unique<GameplaySettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label, ptr_world_renderer);
      });

  registry_logics.Register(
      "InputSettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        return std::make_unique<InputSettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label);
      });

  registry_logics.Register(
      "AdvancedSettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        return std::make_unique<AdvancedSettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label);
      });

  registry_logics.Register(
      "HotkeysSettingsLogic",
      [fn_args_panel_id](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto [str_panel_id, str_label] = fn_args_panel_id(args_args);
        const ora::ui::YamlDictionary* ptr_logic_args = nullptr;
        if (const auto* ptr_value =
                std::get_if<const ora::ui::YamlDictionary*>(
                    &args_args.At("logicArgs")))
          ptr_logic_args = *ptr_value;
        return std::make_unique<HotkeysSettingsLogic>(
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<ISettingsLogic*>(args_args.At("settingsLogic")),
            str_panel_id, str_label, ptr_logic_args);
      });
}

}
