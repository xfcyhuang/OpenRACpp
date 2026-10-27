import std;

#include "ui/logic/version_label_logic.hpp"

#include "game/mod_data.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

VersionLabelLogic::VersionLabelLogic(WidgetArgs& args_args) {
  LabelWidget* ptr_widget =
      static_cast<LabelWidget*>(std::get<Widget*>(args_args.At("widget")));
  game::ModData* ptr_mod_data = std::get<game::ModData*>(args_args.At("modData"));
  const std::string str_version = ptr_mod_data->ManifestRef().Metadata().str_version;
  ptr_widget->fn_get_text = [str_version] { return str_version; };
}

}
