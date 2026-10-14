// UPSTREAM: OpenRA.Game/Widgets/WidgetLoader.cs @e180e3225(widget_loader.hpp 的
// 实现;基型 Container/Input 就地注册)
// The implementation of widget_loader.hpp (the Container/Input base types
// registered in place).
import std;

#include "ui/widget_loader.hpp"

#include "ui/logic/simple_tooltip_logic.hpp"
#include "ui/widgets/background_widget.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/progress_bar_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {
namespace {

// 基型注册(ObjectCreator 装配域的编译期等价) | the base-type
// registrations (the compile-time equivalent of the ObjectCreator assembly).
const bool kBaseWidgetsRegistered = [] {
  auto& registry_widgets = WidgetTypeRegistry::Instance();
  registry_widgets.Register(
      "ContainerWidget",
      [](WidgetArgs&) -> std::unique_ptr<Widget> { return std::make_unique<ContainerWidget>(); });
  registry_widgets.Register(
      "InputWidget",
      [](WidgetArgs&) -> std::unique_ptr<Widget> { return std::make_unique<InputWidget>(); });
  return true;
}();

}  // namespace

WidgetTypeRegistry& WidgetTypeRegistry::Instance() {
  static WidgetTypeRegistry instance_registry;
  return instance_registry;
}

void WidgetTypeRegistry::Register(std::string str_name, FactoryFn fn_factory) {
  vec_entries_.emplace_back(std::move(str_name), std::move(fn_factory));
}

std::unique_ptr<Widget> WidgetTypeRegistry::Create(const std::string& str_name,
                                                   WidgetArgs& args) const {
  for (const auto& [str_entry_name, fn_entry_factory] : vec_entries_)
    if (str_entry_name == str_name)
      return fn_entry_factory(args);

  // ObjectCreator.cs L92 | verbatim
  throw std::runtime_error("Cannot locate type: " + str_name);
}

WidgetLoader::WidgetLoader(const std::vector<std::string>& vec_chrome_layout,
                           fs::FileSystem& file_system_host) {
  // Reuse common strings in YAML(上游注释) | the upstream comment
  vec_files_.reserve(vec_chrome_layout.size());
  for (const std::string& str_file : vec_chrome_layout) {
    const std::vector<char> vec_bytes = file_system_host.Open(str_file);
    vec_files_.push_back(yaml::MiniYaml::FromStream(
        std::string_view{vec_bytes.data(), vec_bytes.size()}, str_file, true, pool_yaml_));
  }

  for (const std::vector<yaml::MiniYamlNode>& vec_nodes : vec_files_)
    for (const yaml::MiniYamlNode& node_widget : vec_nodes) {
      const std::string_view str_key =
          node_widget.Key != nullptr ? std::string_view{*node_widget.Key} : std::string_view{};
      // C# w.Key[(IndexOf('@') + 1)..]:无 '@' 时 IndexOf = -1 → 全键
      // C#'s w.Key[(IndexOf('@') + 1)..]: with no '@', IndexOf = -1 → the
      // whole key
      const std::size_t int4_at = str_key.find('@');
      const std::string str_map_key(int4_at == std::string_view::npos
                                        ? str_key
                                        : str_key.substr(int4_at + 1));
      if (map_widgets_.contains(str_map_key))
        throw std::runtime_error("Widget has duplicate Key `" + std::string(str_key) +
                                 "` at " + node_widget.Location.ToString());
      map_widgets_.emplace(std::move(str_map_key), &node_widget);
    }
}

Widget* WidgetLoader::LoadWidget(WidgetArgs& args_args, Widget* ptr_parent,
                                 const std::string& str_id) {
  const auto it_widget = map_widgets_.find(str_id);
  if (it_widget == map_widgets_.end())
    throw std::runtime_error("Cannot find widget with Id `" + str_id + "`");

  std::unique_ptr<Widget> up_widget;
  Widget* ptr_widget = LoadWidgetNode(args_args, ptr_parent, *it_widget->second, up_widget);
  if (ptr_parent == nullptr) {
    // 无 parent 的根锚定到加载器(上游 GC 生命域等价)
    // a parentless root is anchored to the loader (the upstream GC
    // lifetime-domain equivalent).
    vec_owned_.push_back(std::move(up_widget));
  }
  return ptr_widget;
}

std::unique_ptr<Widget> WidgetLoader::ReleaseRoot(Widget& widget_root) {
  for (auto it_owned = vec_owned_.begin(); it_owned != vec_owned_.end(); ++it_owned)
    if (it_owned->get() == &widget_root) {
      std::unique_ptr<Widget> up_root = std::move(*it_owned);
      vec_owned_.erase(it_owned);
      return up_root;
    }
  return nullptr;
}

Widget* WidgetLoader::LoadWidgetNode(WidgetArgs& args_args, Widget* ptr_parent,
                                     const yaml::MiniYamlNode& node_widget,
                                     std::unique_ptr<Widget>& up_widget) {
  // NewWidget(L77-81):类型 = 首 '@' 前段 + "Widget"
  // NewWidget (L77-81): the type = the pre-first-'@' segment + "Widget".
  const std::string_view str_key =
      node_widget.Key != nullptr ? std::string_view{*node_widget.Key} : std::string_view{};
  const std::size_t int4_at = str_key.find('@');
  const std::string str_type_name = std::string(int4_at == std::string_view::npos
                                                    ? str_key
                                                    : str_key.substr(0, int4_at)) +
                                    "Widget";
  up_widget = WidgetTypeRegistry::Instance().Create(str_type_name, args_args);
  Widget* ptr_widget = up_widget.get();

  if (ptr_parent != nullptr)
    ptr_parent->AddChild(std::move(up_widget));

  if (int4_at != std::string_view::npos) {
    // C# Split('@')[1] = 首二 '@' 之间段(非"首个 '@' 后全部")
    // C#'s Split('@')[1] = the segment between the first two '@'s (not
    // everything after the first).
    const std::size_t int4_at_second = str_key.find('@', int4_at + 1);
    const std::string_view str_id_part =
        int4_at_second == std::string_view::npos ? str_key.substr(int4_at + 1)
                                                 : str_key.substr(int4_at + 1,
                                                                  int4_at_second - int4_at - 1);
    ptr_widget->LoadFieldOrProperty("Id", str_id_part);
  }

  for (const yaml::MiniYamlNode& node_field : node_widget.Value.Nodes) {
    if (node_field.Key == nullptr || *node_field.Key == "Children")
      continue;

    // 有效 chrome yaml 的字段值不为 null;null 值按空串承载(上游传 null,
    // 仅 string 字段可达)
    // valid chrome-yaml field values are never null; a null value is
    // carried as empty (upstream passes null, reachable only for string
    // fields).
    const std::string_view str_value =
        node_field.Value.Value != nullptr ? std::string_view{*node_field.Value.Value}
                                          : std::string_view{};
    if (!ptr_widget->LoadFieldOrProperty(*node_field.Key, str_value))
      // FieldLoader.UnknownFieldAction 默认实现逐字 | the default
      // FieldLoader.UnknownFieldAction verbatim
      throw std::runtime_error("FieldLoader: Missing field `" + *node_field.Key + "`");
  }

  ptr_widget->Initialize(args_args);

  for (const yaml::MiniYamlNode& node_field : node_widget.Value.Nodes) {
    if (node_field.Key == nullptr || *node_field.Key != "Children")
      continue;

    for (const yaml::MiniYamlNode& node_child : node_field.Value.Nodes) {
      std::unique_ptr<Widget> up_child;
      LoadWidgetNode(args_args, ptr_widget, node_child, up_child);
      // up_child 已由 AddChild 移入父级(有 parent 时);无 parent 的根分支
      // 不在此路径(Children 递归恒有 parent)
      // up_child has moved into the parent via AddChild (when parented);
      // the parentless root never takes this path (Children recursion is
      // always parented).
      (void)up_child;
    }
  }

  // logicArgs 出入参(L66-72):Add 抛重复键 —— 子级递归在外层 Add 前已
  // Remove,同构上游
  // the logicArgs set-remove (L66-72): Add throws on duplicates — the
  // children recursion removed theirs before the outer add, isomorphic to
  // upstream.
  const yaml::MiniYamlNode* ptr_logic_node = node_widget.Value.NodeWithKeyOrDefault("Logic");
  const std::unique_ptr<YamlDictionary> up_logic =
      ptr_logic_node != nullptr
          ? std::make_unique<YamlDictionary>(ptr_logic_node->Value.ToDictionary())
          : nullptr;
  args_args.Add("logicArgs", up_logic.get());

  ptr_widget->PostInit(args_args);

  args_args.Remove("logicArgs");

  return ptr_widget;
}

// ———— 基础 widget 族注册(Phase 6 第二批;集中本 TU —— 静态库的注册
// 对象随 WidgetLoader 的引用链保活)————
// The base widget-family registrations (the Phase 6 batch two; centralized
// in this TU — a static library's registration objects stay alive through
// WidgetLoader's reference chain).
namespace {
[[maybe_unused]] const bool kBaseWidgetsBatchRegistered = [] {
  auto& registry_widgets = WidgetTypeRegistry::Instance();
  registry_widgets.Register(
      "BackgroundWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<BackgroundWidget>();
      });
  registry_widgets.Register(
      "ButtonWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<ButtonWidget>(args_args);
      });
  registry_widgets.Register(
      "CheckboxWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<CheckboxWidget>(args_args);
      });
  registry_widgets.Register(
      "ColorBlockWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<ColorBlockWidget>(args_args);
      });
  registry_widgets.Register(
      "DropDownButtonWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<DropDownButtonWidget>(args_args);
      });
  registry_widgets.Register(
      "GradientColorBlockWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<GradientColorBlockWidget>();
      });
  registry_widgets.Register(
      "ImageWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<ImageWidget>();
      });
  registry_widgets.Register(
      "LabelWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<LabelWidget>(args_args);
      });
  registry_widgets.Register(
      "LabelWithTooltipWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<LabelWithTooltipWidget>(args_args);
      });
  registry_widgets.Register(
      "MaskWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<MaskWidget>();
      });
  registry_widgets.Register(
      "ProgressBarWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<ProgressBarWidget>();
      });
  registry_widgets.Register(
      "ScrollItemWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<ScrollItemWidget>(args_args);
      });
  registry_widgets.Register(
      "ScrollPanelWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<ScrollPanelWidget>(args_args);
      });
  registry_widgets.Register(
      "SliderWidget", [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<SliderWidget>(args_args);
      });
  registry_widgets.Register(
      "TextFieldWidget",
      [](WidgetArgs& args_args) -> std::unique_ptr<Widget> {
        return std::make_unique<TextFieldWidget>(args_args);
      });
  registry_widgets.Register(
      "TooltipContainerWidget", [](WidgetArgs&) -> std::unique_ptr<Widget> {
        return std::make_unique<TooltipContainerWidget>();
      });
  return true;
}();

// ChromeLogic 注册(Phase 6 第三批;集中本 TU —— 静态库的注册对象随
// WidgetLoader 的引用链保活)
// The ChromeLogic registrations (the Phase 6 batch three; centralized in
// this TU — a static library's registration objects stay alive through
// WidgetLoader's reference chain).
[[maybe_unused]] const bool kBaseLogicBatchRegistered = [] {
  ChromeLogicRegistry::Instance().Register(
      "SimpleTooltipLogic",
      [](WidgetArgs& args_args) -> std::unique_ptr<ChromeLogic> {
        return std::make_unique<SimpleTooltipLogic>(args_args);
      });
  return true;
}();
}  // namespace
}  // namespace ora::ui
