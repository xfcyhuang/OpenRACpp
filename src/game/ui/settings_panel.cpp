#include "game/ui/settings_panel.hpp"

namespace ora::game::ui {

std::string_view settings_panel_id_name(SettingsPanelId id) {
  switch (id) {
    case SettingsPanelId::kDisplay:
      return "DISPLAY_PANEL";
    case SettingsPanelId::kAudio:
      return "AUDIO_PANEL";
    case SettingsPanelId::kHotkeys:
      return "HOTKEYS_PANEL";
  }
  return "DISPLAY_PANEL";
}

namespace {

// 上游 Widget.Clone（Widget.cs:244-268）：全字段拷贝 + 子树递归克隆 + 父指针重接
// （Bounds 为已求值快照，克隆不再重求值；WidgetModel 含 unique_ptr 子树，逐字段拷贝）。
std::unique_ptr<WidgetModel> widget_clone(const WidgetModel& w, WidgetModel* ptr_parent) {
  auto ptr_out{std::make_unique<WidgetModel>()};
  ptr_out->str_type = w.str_type;
  ptr_out->str_id = w.str_id;
  ptr_out->str_x = w.str_x;
  ptr_out->str_y = w.str_y;
  ptr_out->str_width = w.str_width;
  ptr_out->str_height = w.str_height;
  ptr_out->b_visible = w.b_visible;
  ptr_out->b_ignore_mouse_over = w.b_ignore_mouse_over;
  ptr_out->b_ignore_child_mouse_over = w.b_ignore_child_mouse_over;
  ptr_out->b_click_through = w.b_click_through;
  ptr_out->b_disabled = w.b_disabled;
  ptr_out->b_depressed = w.b_depressed;
  ptr_out->b_moving = w.b_moving;
  ptr_out->b_checked = w.b_checked;
  ptr_out->b_has_double_click = w.b_has_double_click;
  ptr_out->b_record_mouse_down = w.b_record_mouse_down;
  ptr_out->fp4_value = w.fp4_value;
  ptr_out->fp4_min_value = w.fp4_min_value;
  ptr_out->fp4_max_value = w.fp4_max_value;
  ptr_out->int4_ticks = w.int4_ticks;
  ptr_out->int4_track_height = w.int4_track_height;
  ptr_out->vec_logic = w.vec_logic;
  ptr_out->str_default_cursor = w.str_default_cursor;
  ptr_out->vec_fields = w.vec_fields;
  ptr_out->ptr_parent = ptr_parent;
  ptr_out->bounds = w.bounds;
  for (const auto& ptr_child : w.vec_children)
    ptr_out->vec_children.push_back(widget_clone(*ptr_child, ptr_out.get()));
  return ptr_out;
}

WidgetModel* widget_remove_by_id(WidgetModel& parent, std::string_view str_id) {
  for (auto it{parent.vec_children.begin()}; it != parent.vec_children.end(); ++it)
    if ((*it)->str_id == str_id) {
      WidgetModel* ptr_removed{it->get()};
      parent.vec_children.erase(it);
      return ptr_removed;
    }
  return nullptr;
}

// 上游 FieldLoader int2："0, 35" → (0,35)（逗号分段，段内允许前后空白）
std::expected<std::pair<int, int>, std::string> parse_int2(std::string_view str_value) {
  const auto trim{[](std::string_view str_in) {
    while (!str_in.empty() && (str_in.front() == ' ' || str_in.front() == '\t'))
      str_in.remove_prefix(1);
    while (!str_in.empty() && (str_in.back() == ' ' || str_in.back() == '\t'))
      str_in.remove_suffix(1);
    return str_in;
  }};
  const std::size_t sz_comma{str_value.find(',')};
  if (sz_comma == std::string_view::npos)
    return std::unexpected(std::format("FieldLoader: Cannot parse `{}` into field `int2`",
                                       str_value));
  const std::string_view str_x{trim(str_value.substr(0, sz_comma))};
  const std::string_view str_y{trim(str_value.substr(sz_comma + 1))};
  int int4_x{};
  int int4_y{};
  const auto res_x{std::from_chars(str_x.data(), str_x.data() + str_x.size(), int4_x)};
  const auto res_y{std::from_chars(str_y.data(), str_y.data() + str_y.size(), int4_y)};
  if (res_x.ec != std::errc{} || res_y.ec != std::errc{} || res_x.ptr != str_x.data() + str_x.size()
      || res_y.ptr != str_y.data() + str_y.size())
    return std::unexpected(std::format("FieldLoader: Cannot parse `{}` into field `int2`",
                                       str_value));
  return std::pair<int, int>{int4_x, int4_y};
}

// MODE_DROPDOWN GetText（DisplaySettingsLogic.cs:171-173）：Windowed/Fullscreen/
// PseudoFullscreen → fluent key（M2 无 fluent 宿主，用 key 原文——偏离登记）。
std::string_view window_mode_text(WindowMode mode) {
  switch (mode) {
    case WindowMode::kWindowed:
      return "options-display-mode.windowed";
    case WindowMode::kFullscreen:
      return "options-display-mode.legacy-fullscreen";
    case WindowMode::kPseudoFullscreen:
      return "options-display-mode.fullscreen";
  }
  return "options-display-mode.fullscreen";
}

// 上游 int.TryParse 失败 → 0（DisplaySettingsLogic.cs:276-278）
int parse_int_or_zero(std::string_view str_text) {
  int int4_out{};
  const auto res{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_out)};
  if (res.ec != std::errc{} || res.ptr != str_text.data() + str_text.size()) return 0;
  return int4_out;
}

std::string window_mode_get_text(SettingsPanelState& st) {
  return std::string{window_mode_text(st.ptr_settings->graphics.mode)};
}

// 上游 ListLayout.AdjustChildren（Mods.Common/Widgets/ListLayout.cs:32-45）最小折叠：
// child.Y = ContentHeight；ContentHeight += child.Height + ItemSpacing（CollapseHiddenChildren
// 且不可见则不计）；尾差 TopBottomSpacing - ItemSpacing。chrome 字段缺省 = 上游
// ScrollPanelWidget 默认（TopBottomSpacing=2, ItemSpacing=0）。
int scrollpanel_layout_impl(WidgetModel& scroll) {
  auto field_value{[&scroll](std::string_view str_key) -> std::optional<std::string> {
    for (const auto& [str_k, str_v] : scroll.vec_fields)
      if (str_k == str_key) return str_v;
    return std::nullopt;
  }};
  int int4_top_bottom{2};
  int int4_item_spacing{0};
  bool b_collapse{false};
  if (const auto opt{field_value("TopBottomSpacing")})
    std::from_chars(opt->data(), opt->data() + opt->size(), int4_top_bottom);
  if (const auto opt{field_value("ItemSpacing")})
    std::from_chars(opt->data(), opt->data() + opt->size(), int4_item_spacing);
  if (const auto opt{field_value("CollapseHiddenChildren")}) {
    std::string str_lower{*opt};
    for (char& c : str_lower)
      c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    b_collapse = str_lower == "true";
  }
  int int4_content{int4_top_bottom};
  for (const auto& ptr_child : scroll.vec_children) {
    ptr_child->bounds.int4_y = int4_content;
    if (!b_collapse || ptr_child->b_visible)
      int4_content += ptr_child->bounds.int4_height + int4_item_spacing;
  }
  return int4_content + int4_top_bottom - int4_item_spacing;
}

}  // namespace

int settings_panel_scrollpanel_layout(WidgetModel& scroll) { return scrollpanel_layout_impl(scroll); }

std::expected<void, std::string> settings_panel_open(
    SettingsPanelState& st, WidgetModel& root, const WidgetLoaderModel& loader_settings,
    const WidgetLoaderModel& loader_display, const WidgetLoaderModel& loader_audio,
    const WidgetLoaderModel& loader_hotkeys, const WidgetInitCtx& ctx,
    std::string_view str_settings_path) {
  if (st.ptr_settings == nullptr) return std::unexpected("settings panel: no settings model");

  // SETTINGS_PANEL 窗口装载（Ui.OpenWindow 形态：挂 root）
  const auto res_window{widget_loader_load_widget(loader_settings, root, ctx, "SETTINGS_PANEL")};
  if (!res_window) return std::unexpected(res_window.error());
  WidgetModel* ptr_window{*res_window};

  // Logic 参数（ButtonStride / Panels 注册序）来自 SETTINGS_PANEL 节点
  std::pair<int, int> pr_stride{0, 0};
  std::vector<std::string> vec_panels_order;
  if (const auto it{loader_settings.map_widgets.find("SETTINGS_PANEL")};
      it != loader_settings.map_widgets.end()) {
    for (const MiniYamlNode& node_child : it->second->vec_children) {
      if (node_child.str_key == "Logic") {
        for (const MiniYamlNode& node_arg : node_child.vec_children) {
          if (node_arg.str_key == "ButtonStride") {
            const auto res_stride{parse_int2(node_arg.str_value)};
            if (!res_stride) return std::unexpected(res_stride.error());
            pr_stride = *res_stride;
          } else if (node_arg.str_key == "Panels") {
            for (const MiniYamlNode& node_panel : node_arg.vec_children)
              vec_panels_order.push_back(node_panel.str_key);
          }
        }
      }
    }
  }
  if (vec_panels_order.empty()) return std::unexpected("settings panel: no Panels logic args");

  // 上游 Panels 注册序的子集校验：三页相对序必须与注册序一致
  const std::vector<SettingsPanelId> vec_pages{SettingsPanelId::kDisplay, SettingsPanelId::kAudio,
                                             SettingsPanelId::kHotkeys};
  std::size_t sz_last_order{0};
  for (const SettingsPanelId id : vec_pages) {
    const auto it_found{std::find(vec_panels_order.begin(), vec_panels_order.end(),
                                  std::string{settings_panel_id_name(id)})};
    if (it_found == vec_panels_order.end())
      return std::unexpected(
          std::format("settings panel: {} missing from Panels args", settings_panel_id_name(id)));
    const std::size_t sz_order{
        static_cast<std::size_t>(it_found - vec_panels_order.begin())};
    if (sz_order < sz_last_order)
      return std::unexpected("settings panel: page order diverges from Panels registration");
    sz_last_order = sz_order;
  }

  // PANEL_TEMPLATE 克隆三份（Id=面板名），模板移除（SettingsLogic.cs:83-110）
  WidgetModel* ptr_panel_container{widget_find_by_id(*ptr_window, "PANEL_CONTAINER")};
  if (ptr_panel_container == nullptr) return std::unexpected("PANEL_CONTAINER missing");
  const WidgetModel* ptr_template{
      widget_find_by_id(*ptr_panel_container, "PANEL_TEMPLATE")};
  if (ptr_template == nullptr) return std::unexpected("PANEL_TEMPLATE missing");

  std::vector<WidgetModel*> vec_page_containers;
  for (const SettingsPanelId id : vec_pages) {
    auto ptr_clone{widget_clone(*ptr_template, ptr_panel_container)};
    ptr_clone->str_id = std::string{settings_panel_id_name(id)};
    ptr_clone->b_visible = id == vec_pages.front();  // activePanel ??= 首个注册
    WidgetModel* ptr_clone_raw{ptr_clone.get()};
    ptr_panel_container->vec_children.push_back(std::move(ptr_clone));
    vec_page_containers.push_back(ptr_clone_raw);
  }
  static_cast<void>(widget_remove_by_id(*ptr_panel_container, "PANEL_TEMPLATE"));

  // 三页 chrome 装载进克隆（Game.LoadWidget(world, panelKey, container, ...)）
  for (std::size_t sz_i{0}; sz_i < vec_pages.size(); ++sz_i) {
    const WidgetLoaderModel* ptr_loader{sz_i == 0     ? &loader_display
                                        : sz_i == 1   ? &loader_audio
                                                      : &loader_hotkeys};
    const auto res_page{widget_loader_load_widget(*ptr_loader, *vec_page_containers[sz_i], ctx,
                                                  settings_panel_id_name(vec_pages[sz_i]))};
    if (!res_page) return std::unexpected(res_page.error());
    // 设置面板页 ScrollPanel ROW 堆叠（上游布局器最小折叠——偏离登记）
    if (WidgetModel* ptr_scroll{
            widget_find_by_id(*vec_page_containers[sz_i], "SETTINGS_SCROLLPANEL")})
      static_cast<void>(scrollpanel_layout_impl(*ptr_scroll));
  }

  // SETTINGS_TAB_CONTAINER：BUTTON_TEMPLATE 克隆按 ButtonStride 布局（:179-198）
  WidgetModel* ptr_tab_container{widget_find_by_id(*ptr_window, "SETTINGS_TAB_CONTAINER")};
  if (ptr_tab_container == nullptr) return std::unexpected("SETTINGS_TAB_CONTAINER missing");
  const WidgetModel* ptr_tab_template{widget_find_by_id(*ptr_tab_container, "BUTTON_TEMPLATE")};
  if (ptr_tab_template == nullptr) return std::unexpected("BUTTON_TEMPLATE missing");
  for (std::size_t sz_i{0}; sz_i < vec_pages.size(); ++sz_i) {
    auto ptr_tab{widget_clone(*ptr_tab_template, ptr_tab_container)};
    ptr_tab->str_id = std::string{settings_panel_id_name(vec_pages[sz_i])};
    ptr_tab->bounds.int4_x += static_cast<int>(sz_i) * pr_stride.first;
    ptr_tab->bounds.int4_y += static_cast<int>(sz_i) * pr_stride.second;
    ptr_tab->b_checked = vec_pages[sz_i] == vec_pages.front();  // IsHighlighted 折叠
    ptr_tab_container->vec_children.push_back(std::move(ptr_tab));
  }
  static_cast<void>(widget_remove_by_id(*ptr_tab_container, "BUTTON_TEMPLATE"));

  // 首开快照（上游 static originalGraphicSettings/originalSoundSettings ??= 语义）
  st.ptr_window = ptr_window;
  st.graphics_original = st.ptr_settings->graphics;
  st.sound_original = st.ptr_settings->sound;
  st.str_settings_path = std::string{str_settings_path};
  st.active = vec_pages.front();

  // 热键持久化面初始化：装载自文件的 Keys 节点保留进 vec_settings（上游
  // hotkeySettings.Yaml 原样持有，Set 时才 RemoveAll+条件追加）
  if (st.ptr_hotkeys != nullptr) {
    if (const MiniYamlNode* ptr_keys{settings_find_section(*st.ptr_settings, "Keys")}) {
      for (const MiniYamlNode& node_key : ptr_keys->vec_children) {
        const auto it_found{std::find_if(
            st.ptr_hotkeys->vec_settings.begin(), st.ptr_hotkeys->vec_settings.end(),
            [&node_key](const std::pair<std::string, std::string>& pr) {
              return pr.first == node_key.str_key;
            })};
        if (it_found == st.ptr_hotkeys->vec_settings.end())
          st.ptr_hotkeys->vec_settings.emplace_back(node_key.str_key, node_key.str_value);
      }
    }
  }

  // 三页控件初值（InitPanel 等价）
  st.str_window_width_text = std::format("{}", st.ptr_settings->graphics.windowed_size.int4_x);
  st.str_window_height_text = std::format("{}", st.ptr_settings->graphics.windowed_size.int4_y);
  st.str_mode_text = window_mode_get_text(st);
  if (WidgetModel* ptr_slider{widget_find_by_id(root, "SOUND_VOLUME")})
    ptr_slider->fp4_value = st.ptr_settings->sound.fp_sound_volume;
  if (WidgetModel* ptr_slider{widget_find_by_id(root, "MUSIC_VOLUME")})
    ptr_slider->fp4_value = st.ptr_settings->sound.fp_music_volume;
  if (WidgetModel* ptr_slider{widget_find_by_id(root, "VIDEO_VOLUME")})
    ptr_slider->fp4_value = st.ptr_settings->sound.fp_video_volume;

  st.vec_events.push_back("panel open");
  return {};
}

bool settings_panel_leave(SettingsPanelState& st, SettingsPanelId id) {
  switch (id) {
    case SettingsPanelId::kDisplay: {
      // DisplaySettingsLogic.cs:274-285：TryParse 文本 → WindowedSize；返回五字段比较
      auto& gfx{st.ptr_settings->graphics};
      gfx.windowed_size.int4_x = parse_int_or_zero(st.str_window_width_text);
      gfx.windowed_size.int4_y = parse_int_or_zero(st.str_window_height_text);
      const GraphicSettingsModel& orig{st.graphics_original};
      return gfx.mode != orig.mode || gfx.int4_video_display != orig.int4_video_display
          || gfx.windowed_size.int4_x != orig.windowed_size.int4_x
          || gfx.windowed_size.int4_y != orig.windowed_size.int4_y
          || gfx.fullscreen_size.int4_x != orig.fullscreen_size.int4_x
          || gfx.fullscreen_size.int4_y != orig.fullscreen_size.int4_y
          || gfx.gl_profile != orig.gl_profile;
    }
    case SettingsPanelId::kAudio:
      // AudioSettingsLogic.cs:117-122：Device 赋值 + 与原文比较（M2 无设备选择面）
      return st.ptr_settings->sound.str_device != st.sound_original.str_device;
    case SettingsPanelId::kHotkeys:
      // HotkeysSettingsLogic.cs:162-172：yield focus，恒 false
      st.opt_selected_hotkey.reset();
      return false;
  }
  return false;
}

void settings_panel_save(SettingsPanelState& st) {
  if (st.ptr_hotkeys != nullptr && !st.ptr_hotkeys->vec_settings.empty())
    settings_keys_assign(*st.ptr_settings, st.ptr_hotkeys->vec_settings);
  if (!st.str_settings_path.empty())
    static_cast<void>(settings_save(*st.ptr_settings, st.str_settings_path));
  st.vec_events.push_back("save");
}

void settings_panel_close(SettingsPanelState& st, MainMenuState* ptr_menu) {
  st.b_needs_restart |= settings_panel_leave(st, st.active);
  settings_panel_save(st);
  // CloseAndExit：Ui.CloseWindow + onExit（重启确认对话框不在最小面——偏离登记）
  if (ptr_menu != nullptr) main_menu_close_settings(*ptr_menu);
  st.vec_events.push_back("panel close");
}

void settings_panel_tab_click(SettingsPanelState& st, SettingsPanelId id) {
  // 上游 tab.OnClick 无条件 leave(active)+Save+activePanel 赋值（含点击已激活 tab）
  st.b_needs_restart |= settings_panel_leave(st, st.active);
  settings_panel_save(st);
  st.active = id;
  // 可见性/高亮 lambda 折叠：panel.IsVisible = activePanel==panelID、tab.IsHighlighted 同
  // （tab 与 panel 克隆同名，按各自容器子树限定查找）
  if (st.ptr_window != nullptr) {
    WidgetModel* ptr_panels{widget_find_by_id(*st.ptr_window, "PANEL_CONTAINER")};
    WidgetModel* ptr_tabs{widget_find_by_id(*st.ptr_window, "SETTINGS_TAB_CONTAINER")};
    for (const SettingsPanelId page :
         {SettingsPanelId::kDisplay, SettingsPanelId::kAudio, SettingsPanelId::kHotkeys}) {
      if (ptr_panels != nullptr)
        if (WidgetModel* ptr_panel{widget_find_by_id(*ptr_panels, settings_panel_id_name(page))})
          ptr_panel->b_visible = page == st.active;
      if (ptr_tabs != nullptr)
        if (WidgetModel* ptr_tab{widget_find_by_id(*ptr_tabs, settings_panel_id_name(page))})
          ptr_tab->b_checked = page == st.active;
    }
  }
  st.vec_events.push_back(std::format("tab {}", settings_panel_id_name(id)));
}

void settings_panel_reset(SettingsPanelState& st, SettingsPanelId id) {
  switch (id) {
    case SettingsPanelId::kDisplay: {
      // DisplaySettingsLogic.ResetPanel（:288-315；UIScale/renderer 面不在最小面）
      const GraphicSettingsModel def{};
      auto& gfx{st.ptr_settings->graphics};
      gfx.b_cap_framerate = def.b_cap_framerate;
      gfx.int4_max_framerate = def.int4_max_framerate;
      gfx.b_cap_framerate_to_game_fps = def.b_cap_framerate_to_game_fps;
      gfx.gl_profile = def.gl_profile;
      gfx.mode = def.mode;
      gfx.int4_video_display = def.int4_video_display;
      gfx.windowed_size = def.windowed_size;
      gfx.b_cursor_double = def.b_cursor_double;
      gfx.viewport_distance = def.viewport_distance;
      st.ptr_settings->game.text_notification_pool_filters =
          GameSettingsModel{}.text_notification_pool_filters;
      st.str_window_width_text = std::format("{}", gfx.windowed_size.int4_x);
      st.str_window_height_text = std::format("{}", gfx.windowed_size.int4_y);
      st.str_mode_text = window_mode_get_text(st);
      break;
    }
    case SettingsPanelId::kAudio: {
      // AudioSettingsLogic.ResetPanel（:124-146）
      const SoundSettingsModel def{};
      auto& snd{st.ptr_settings->sound};
      snd.fp_sound_volume = def.fp_sound_volume;
      snd.fp_music_volume = def.fp_music_volume;
      snd.fp_video_volume = def.fp_video_volume;
      snd.b_cash_ticks = def.b_cash_ticks;
      snd.b_mute = def.b_mute;
      snd.b_mute_background_music = def.b_mute_background_music;
      snd.str_device = def.str_device;
      if (st.ptr_sound != nullptr) {
        sound_facade_set_sound_volume(*st.ptr_sound, snd.fp_sound_volume);
        sound_facade_set_music_volume(*st.ptr_sound, snd.fp_music_volume);
        sound_facade_set_video_volume(*st.ptr_sound, snd.fp_video_volume);
        ora::platforms::engine_volume_set(st.ptr_sound->engine, 1.0f);
      }
      break;
    }
    case SettingsPanelId::kHotkeys: {
      // HotkeysSettingsLogic.ResetPanel（:175-187）：全部 Set(name, default)
      if (st.ptr_hotkeys != nullptr) {
        input::HotkeyRegistryModel& reg{*st.ptr_hotkeys};
        for (const input::HotkeyDefinitionModel& def : reg.vec_definitions)
          static_cast<void>(input::hotkey_set(reg, def.str_name, def.hk_default));
      }
      break;
    }
  }
  settings_panel_save(st);
  st.vec_events.push_back(std::format("reset {}", settings_panel_id_name(id)));
}

void settings_panel_set_window_mode(SettingsPanelState& st, WindowMode mode) {
  st.ptr_settings->graphics.mode = mode;
  st.str_mode_text = window_mode_get_text(st);
  st.vec_events.push_back(std::format("mode {}", window_mode_text(mode)));
}

void settings_panel_set_window_text(SettingsPanelState& st, bool b_width,
                                    std::string_view str_text) {
  (b_width ? st.str_window_width_text : st.str_window_height_text) = std::string{str_text};
}

void settings_panel_toggle_vsync(SettingsPanelState& st) {
  st.ptr_settings->graphics.b_vsync = !st.ptr_settings->graphics.b_vsync;
  st.vec_events.push_back("vsync toggle");
}

void settings_panel_set_volume(SettingsPanelState& st, std::size_t sz_channel,
                               float fp4_value) {
  const float fp4_clamped{std::clamp(fp4_value, 0.0f, 1.0f)};
  SoundSettingsModel& snd{st.ptr_settings->sound};
  const char* sz_names[]{"SOUND_VOLUME", "MUSIC_VOLUME", "VIDEO_VOLUME"};
  if (sz_channel > 2) return;
  if (sz_channel == 0) snd.fp_sound_volume = fp4_clamped;
  if (sz_channel == 1) snd.fp_music_volume = fp4_clamped;
  if (sz_channel == 2) snd.fp_video_volume = fp4_clamped;
  if (st.ptr_sound != nullptr) {
    if (sz_channel == 0) sound_facade_set_sound_volume(*st.ptr_sound, fp4_clamped);
    if (sz_channel == 1) sound_facade_set_music_volume(*st.ptr_sound, fp4_clamped);
    if (sz_channel == 2) sound_facade_set_video_volume(*st.ptr_sound, fp4_clamped);
  }
  // 树滑条值同步（上游滑条自身 Value 即状态源；M2 折叠为状态+树双写）
  if (st.ptr_window != nullptr) {
    const char* sz_ids[]{"SOUND_VOLUME", "MUSIC_VOLUME", "VIDEO_VOLUME"};
    if (WidgetModel* ptr_slider{widget_find_by_id(*st.ptr_window, sz_ids[sz_channel])})
      ptr_slider->fp4_value = fp4_clamped;
  }
  st.vec_events.push_back(std::format("volume {} {:.6f}", sz_names[sz_channel], fp4_clamped));
}

void settings_panel_toggle_mute(SettingsPanelState& st) {
  SoundSettingsModel& snd{st.ptr_settings->sound};
  snd.b_mute = !snd.b_mute;
  if (st.ptr_sound != nullptr)
    ora::platforms::engine_volume_set(st.ptr_sound->engine, snd.b_mute ? 0.0f : 1.0f);
  st.vec_events.push_back(std::format("mute {}", snd.b_mute ? 1 : 0));
}

bool settings_panel_rekey(SettingsPanelState& st, std::string_view str_name,
                          const input::HotkeyModel& hk) {
  if (st.ptr_hotkeys == nullptr) return false;
  st.opt_selected_hotkey = std::string{str_name};
  const bool b_set{input::hotkey_set(*st.ptr_hotkeys, str_name, hk)};
  if (b_set) settings_panel_save(st);
  st.vec_events.push_back(std::format("rekey {} {}", str_name, b_set ? "ok" : "rejected"));
  return b_set;
}

std::vector<HotkeyRow> settings_panel_hotkey_rows(const input::HotkeyRegistryModel& reg) {
  std::vector<HotkeyRow> vec_rows;
  vec_rows.reserve(reg.vec_definitions.size());
  for (std::size_t sz_i{0}; sz_i < reg.vec_definitions.size(); ++sz_i) {
    const input::HotkeyDefinitionModel& def{reg.vec_definitions[sz_i]};
    vec_rows.push_back(HotkeyRow{.str_name = def.str_name,
                                 .str_key = input::hotkey_to_string(reg.vec_keys[sz_i]),
                                 .str_default = input::hotkey_to_string(def.hk_default),
                                 .b_readonly = def.b_readonly,
                                 .b_has_duplicates = def.b_has_duplicates});
  }
  return vec_rows;
}

void settings_panel_dispatch(SettingsPanelState& st, std::span<const UiEventRecord> vec_events) {
  for (const UiEventRecord& rec : vec_events) {
    if (rec.str_kind == "click") {
      if (rec.str_id == "DISPLAY_PANEL") {
        settings_panel_tab_click(st, SettingsPanelId::kDisplay);
      } else if (rec.str_id == "AUDIO_PANEL") {
        settings_panel_tab_click(st, SettingsPanelId::kAudio);
      } else if (rec.str_id == "HOTKEYS_PANEL") {
        settings_panel_tab_click(st, SettingsPanelId::kHotkeys);
      } else if (rec.str_id == "VSYNC_CHECKBOX") {
        settings_panel_toggle_vsync(st);
      } else if (rec.str_id == "MUTE_SOUND") {
        settings_panel_toggle_mute(st);
      }
    } else if (rec.str_kind.starts_with("change ")
               && (rec.str_id == "SOUND_VOLUME" || rec.str_id == "MUSIC_VOLUME"
                   || rec.str_id == "VIDEO_VOLUME")) {
      const std::size_t sz_channel{
          static_cast<std::size_t>(rec.str_id == "SOUND_VOLUME"   ? 0
                                   : rec.str_id == "MUSIC_VOLUME" ? 1
                                                                  : 2)};
      float fp4_value{};
      const auto res{std::from_chars(rec.str_kind.data() + 7, rec.str_kind.data() + rec.str_kind.size(), fp4_value)};
      if (res.ec == std::errc{}) settings_panel_set_volume(st, sz_channel, fp4_value);
    }
  }
}

}  // namespace ora::game::ui
