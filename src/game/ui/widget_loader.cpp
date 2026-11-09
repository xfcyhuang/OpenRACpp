#include "game/ui/widget_loader.hpp"

namespace ora::game::ui {

namespace {

constexpr WidgetTypeSpec kTypeSpecs[]{
    {"Container", true, true, false, true},
    {"InputWidget", false, false, false, false},
    {"Image", true, false, false, false},
    {"Label", false, false, false, false},
    {"LabelWithTooltip", false, false, false, false},
    {"LabelForInput", false, false, false, false},
    {"Button", false, false, true, false},
    {"MenuButton", false, false, true, false},
    {"ProductionTypeButton", false, false, true, false},
    {"Checkbox", false, false, true, false},
    {"DropDownButton", false, false, true, false},
    {"Slider", false, false, false, false},
    {"ExponentialSlider", false, false, false, false},
    {"ColorBlock", false, false, false, false},
    {"PerfGraph", false, false, false, false},
    {"Radar", false, false, false, false},
    {"VideoPlayer", false, false, false, false},
    {"LogicKeyListener", false, false, false, false},
    {"LogicTicker", false, false, false, false},
    {"ControlGroups", false, false, false, false},
    {"SupportPowers", false, false, false, false},
    {"SupportPowerTimer", false, false, false, false},
    {"ProductionPalette", false, false, false, false},
    {"Background", false, false, false, false},
    {"MouseAttachment", true, false, false, false},
    {"ScrollPanel", false, false, false, false},
    {"TextField", false, false, false, false},
    {"HotkeyEntry", false, false, false, false},
    {"TooltipContainer", false, false, false, false},
    {"ClientTooltipRegion", false, false, false, false},
    {"MapPreview", false, false, false, false},
    {"ProgressBar", false, false, false, false},
};

constexpr bool is_ascii_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

constexpr char to_ascii_lower(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::expected<bool, std::string> parse_bool_field(std::string_view str_value, std::string_view str_key) {
  while (!str_value.empty() && is_ascii_space(str_value.front())) str_value.remove_prefix(1);
  while (!str_value.empty() && is_ascii_space(str_value.back())) str_value.remove_suffix(1);
  std::string str_core{str_value};
  for (char& c : str_core) c = to_ascii_lower(c);
  if (str_core == "true") return true;
  if (str_core == "false") return false;
  return std::unexpected(std::format("FieldLoader: Cannot parse `{}` into field `{}` of type `Boolean`",
      str_value, str_key));
}

std::vector<std::string> split_comma_trim(std::string_view str_value) {
  std::vector<std::string> vec_out;
  std::size_t sz_begin{0};
  while (sz_begin <= str_value.size()) {
    const std::size_t sz_comma{str_value.find(',', sz_begin)};
    std::string_view str_part{str_value.substr(
        sz_begin, sz_comma == std::string_view::npos ? std::string_view::npos : sz_comma - sz_begin)};
    while (!str_part.empty() && (str_part.front() == ' ' || str_part.front() == '\t'))
      str_part.remove_prefix(1);
    while (!str_part.empty() && (str_part.back() == ' ' || str_part.back() == '\t'))
      str_part.remove_suffix(1);
    vec_out.emplace_back(str_part);
    if (sz_comma == std::string_view::npos) break;
    sz_begin = sz_comma + 1;
  }
  return vec_out;
}

std::expected<void, std::string> widget_apply_field(WidgetModel& w, std::string_view str_key,
                                                    std::string_view str_value) {
  if (str_key == "X") {
    w.str_x = std::string{str_value};
    return {};
  }
  if (str_key == "Y") {
    w.str_y = std::string{str_value};
    return {};
  }
  if (str_key == "Width") {
    w.str_width = std::string{str_value};
    return {};
  }
  if (str_key == "Height") {
    w.str_height = std::string{str_value};
    return {};
  }
  if (str_key == "Id") {
    w.str_id = std::string{str_value};
    return {};
  }
  if (str_key == "Logic") {
    w.vec_logic = split_comma_trim(str_value);
    return {};
  }
  if (str_key == "Visible" || str_key == "IgnoreMouseOver" || str_key == "IgnoreChildMouseOver"
      || str_key == "ClickThrough" || str_key == "Disabled" || str_key == "Depressed") {
    const auto res_bool{parse_bool_field(str_value, str_key)};
    if (!res_bool) return std::unexpected(res_bool.error());
    if (str_key == "Visible") w.b_visible = *res_bool;
    else if (str_key == "IgnoreMouseOver") w.b_ignore_mouse_over = *res_bool;
    else if (str_key == "IgnoreChildMouseOver") w.b_ignore_child_mouse_over = *res_bool;
    else if (str_key == "ClickThrough") w.b_click_through = *res_bool;
    else if (str_key == "Depressed") w.b_depressed = *res_bool;
    else w.b_disabled = *res_bool;
    return {};
  }
  if (str_key == "MinimumValue" || str_key == "MaximumValue" || str_key == "Value") {
    float fp4_value{};
    const auto res{std::from_chars(str_value.data(), str_value.data() + str_value.size(), fp4_value)};
    if (res.ec != std::errc{} || res.ptr != str_value.data() + str_value.size())
      return std::unexpected(std::format(
          "FieldLoader: Cannot parse `{}` into field `{}` of type `Single`", str_value, str_key));
    if (str_key == "MinimumValue") w.fp4_min_value = fp4_value;
    else if (str_key == "MaximumValue") w.fp4_max_value = fp4_value;
    else w.fp4_value = fp4_value;
    return {};
  }
  if (str_key == "Ticks" || str_key == "TrackHeight") {
    int int4_value{};
    const auto res{std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_value)};
    if (res.ec != std::errc{} || res.ptr != str_value.data() + str_value.size())
      return std::unexpected(std::format(
          "FieldLoader: Cannot parse `{}` into field `{}` of type `Int32`", str_value, str_key));
    if (str_key == "Ticks") w.int4_ticks = int4_value;
    else w.int4_track_height = int4_value;
    return {};
  }
  w.vec_fields.emplace_back(std::string{str_key}, std::string{str_value});
  return {};
}

std::expected<WidgetModel*, std::string> widget_loader_load_node(const WidgetLoaderModel& loader,
                                                                 WidgetModel& parent,
                                                                 const WidgetInitCtx& ctx,
                                                                 const MiniYamlNode& node) {
  const std::string_view str_type{widget_key_type_part(node.str_key)};
  const WidgetTypeSpec* ptr_spec{widget_type_spec_find(str_type)};
  if (ptr_spec == nullptr)
    return std::unexpected(
        std::format("Unknown widget type `{}` at {}", str_type, node.loc_location.str_name));

  auto ptr_widget{std::make_unique<WidgetModel>()};
  WidgetModel& w{*ptr_widget};
  w.str_type = std::string{str_type};
  w.b_click_through = ptr_spec->b_click_through;
  w.b_ignore_mouse_over = ptr_spec->b_ignore_mouse_over;
  w.ptr_parent = &parent;
  parent.vec_children.push_back(std::move(ptr_widget));

  if (const std::size_t sz_at{node.str_key.find('@')}; sz_at != std::string::npos)
    w.str_id = node.str_key.substr(sz_at + 1);

  for (const MiniYamlNode& child : node.vec_children) {
    if (child.str_key == "Children") continue;
    if (const auto res{widget_apply_field(w, child.str_key, child.str_value)}; !res)
      return std::unexpected(res.error());
  }

  widget_initialize(w, ctx);

  for (const MiniYamlNode& child : node.vec_children) {
    if (child.str_key != "Children") continue;
    for (const MiniYamlNode& c : child.vec_children) {
      if (const auto res{widget_loader_load_node(loader, w, ctx, c)}; !res) return res;
    }
  }

  return &w;
}

}  // namespace

const std::vector<WidgetTypeSpec>& widget_type_registry() {
  static const std::vector<WidgetTypeSpec> vec_registry{kTypeSpecs, kTypeSpecs + std::size(kTypeSpecs)};
  return vec_registry;
}

const WidgetTypeSpec* widget_type_spec_find(std::string_view str_type) {
  for (const WidgetTypeSpec& spec : widget_type_registry())
    if (spec.str_type == str_type) return &spec;
  return nullptr;
}

std::string_view widget_key_type_part(std::string_view str_key) {
  const std::size_t sz_at{str_key.find('@')};
  return sz_at == std::string_view::npos ? str_key : str_key.substr(0, sz_at);
}

std::string_view widget_key_id_part(std::string_view str_key) {
  const std::size_t sz_at{str_key.find('@')};
  return sz_at == std::string_view::npos ? std::string_view{} : str_key.substr(sz_at + 1);
}

std::expected<WidgetLoaderModel, std::string> widget_loader_from_nodes(
    const std::vector<MiniYamlNode>& vec_nodes) {
  WidgetLoaderModel loader{};
  for (const MiniYamlNode& node : vec_nodes) {
    const std::string str_key{widget_key_id_part(node.str_key)};
    if (loader.map_widgets.contains(str_key))
      return std::unexpected(
          std::format("Widget has duplicate Key `{}` at {}", node.str_key, node.loc_location.str_name));
    loader.map_widgets.emplace(str_key, &node);
  }
  return loader;
}

std::expected<WidgetModel*, std::string> widget_loader_load_widget(
    const WidgetLoaderModel& loader, WidgetModel& parent, const WidgetInitCtx& ctx,
    std::string_view str_id) {
  if (const auto it{loader.map_widgets.find(std::string{str_id})}; it != loader.map_widgets.end())
    return widget_loader_load_node(loader, parent, ctx, *it->second);
  return std::unexpected(std::format("Cannot find widget with Id `{}`", str_id));
}

}
