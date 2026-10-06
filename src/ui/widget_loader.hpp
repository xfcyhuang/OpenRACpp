// UPSTREAM: OpenRA.Game/Widgets/WidgetLoader.cs @b6fc03f(chrome layout 加载
// 逐语义:首 '@' 后键索引/重复键抛/Id 取 Split('@')[1]/字段加载/Initialize/
// Children 递归/logicArgs 出入参/PostInit)。ObjectCreator.CreateObject →
// WidgetTypeRegistry 名字分派(键 = "{Type}Widget";未知名 "Cannot locate
// type: {name}" 逐字);根 widget 之外的依赖(FileSystem/ChromeLayout)注入。
// Verbatim-semantics rewrite of the chrome-layout loader (the after-first-
// '@' key index / the duplicate-key throw / Id from Split('@')[1] / field
// loading / Initialize / the Children recursion / the logicArgs set-remove
// / PostInit). ObjectCreator.CreateObject becomes the WidgetTypeRegistry
// name dispatch (keyed "{Type}Widget"; unknown names throw "Cannot locate
// type: {name}" verbatim); the FileSystem/ChromeLayout dependencies are
// injected.
#pragma once
import std;

#include "fs/file_system.hpp"
#include "ui/widget.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {

/// widget 类型分派注册表(ObjectCreator 等价) | the widget-type dispatch
/// registry (the ObjectCreator equivalent).
class WidgetTypeRegistry {
 public:
  using FactoryFn = std::function<std::unique_ptr<Widget>(WidgetArgs&)>;

  static WidgetTypeRegistry& Instance();

  void Register(std::string str_name, FactoryFn fn_factory);

  /// 未知名抛 "Cannot locate type: {name}"(ObjectCreator.cs L92 逐字)
  /// an unknown name throws "Cannot locate type: {name}" (ObjectCreator.cs
  /// L92 verbatim).
  std::unique_ptr<Widget> Create(const std::string& str_name, WidgetArgs& args) const;

 private:
  std::vector<std::pair<std::string, FactoryFn>> vec_entries_;
};

/// widget 自注册宏(名字 = "{Cls}") | the widget self-registration macro
/// (the name = "{Cls}").
#define ORA_REGISTER_WIDGET(Cls)                                            \
  const bool kWidgetRegistered_##Cls = [] {                                 \
    ::ora::ui::WidgetTypeRegistry::Instance().Register(                     \
        #Cls, [](::ora::ui::WidgetArgs& args_args)                          \
                  -> std::unique_ptr<::ora::ui::Widget> {                   \
          return std::make_unique<Cls>(args_args);                          \
        });                                                                 \
    return true;                                                            \
  }()

class WidgetLoader {
 public:
  /// L24-36:ChromeLayout 全部解析,键 = 首 '@' 后缀(无 '@' = 全键),
  /// 重复键抛 "Widget has duplicate Key `{key}` at {location}" 逐字
  /// L24-36: parses every ChromeLayout file, keyed by the after-first-'@'
  /// suffix (the whole key when none), duplicate keys throwing
  /// "Widget has duplicate Key `{key}` at {location}" verbatim.
  WidgetLoader(const std::vector<std::string>& vec_chrome_layout,
               fs::FileSystem& file_system_host);

  /// L38-44:未命中抛 "Cannot find widget with Id `{w}`" 逐字
  /// L38-44: a miss throws "Cannot find widget with Id `{w}`" verbatim.
  Widget* LoadWidget(WidgetArgs& args_args, Widget* ptr_parent, const std::string& str_id);

 private:
  /// L46-75;返回值由 parent(或调用方经 up_widget)持有
  /// L46-75; the return value is held by parent (or the caller via
  /// up_widget).
  Widget* LoadWidgetNode(WidgetArgs& args_args, Widget* ptr_parent,
                         const yaml::MiniYamlNode& node_widget,
                         std::unique_ptr<Widget>& up_widget);

  std::vector<std::vector<yaml::MiniYamlNode>> vec_files_;  // 节点存活期锚 | the node lifetime anchor
  std::vector<std::unique_ptr<Widget>> vec_owned_;  // 无 parent 根的所有权锚 | the ownership anchor for parentless roots
  std::map<std::string, const yaml::MiniYamlNode*> map_widgets_;
  yaml::StringPool pool_yaml_;
};

}  // namespace ora::ui
