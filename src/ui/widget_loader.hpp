#pragma once
import std;

#include "fs/file_system.hpp"
#include "ui/widget.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {

class WidgetTypeRegistry {
 public:
  using FactoryFn = std::function<std::unique_ptr<Widget>(WidgetArgs&)>;

  static WidgetTypeRegistry& Instance();

  void Register(std::string str_name, FactoryFn fn_factory);

  std::unique_ptr<Widget> Create(const std::string& str_name, WidgetArgs& args) const;

 private:
  std::vector<std::pair<std::string, FactoryFn>> vec_entries_;
};

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

  WidgetLoader(const std::vector<std::string>& vec_chrome_layout,
               fs::FileSystem& file_system_host);

  Widget* LoadWidget(WidgetArgs& args_args, Widget* ptr_parent, const std::string& str_id);

  std::unique_ptr<Widget> ReleaseRoot(Widget& widget_root);

 private:

  Widget* LoadWidgetNode(WidgetArgs& args_args, Widget* ptr_parent,
                         const yaml::MiniYamlNode& node_widget,
                         std::unique_ptr<Widget>& up_widget);

  std::vector<std::vector<yaml::MiniYamlNode>> vec_files_;
  std::vector<std::unique_ptr<Widget>> vec_owned_;
  std::map<std::string, const yaml::MiniYamlNode*> map_widgets_;
  yaml::StringPool pool_yaml_;
};

}
