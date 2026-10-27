#pragma once
import std;

#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::sim {
class World;
}

namespace ora::ui {

class ControlGroupsWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ControlGroupsWidget";

  ControlGroupsWidget() = default;
  explicit ControlGroupsWidget(WidgetArgs& args_args);
  ~ControlGroupsWidget() override = default;

  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Initialize(WidgetArgs& args_args) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  bool HandleKeyPress(const KeyInput& e_input) override;

  std::string str_select_group_key_prefix;
  std::string str_create_group_key_prefix;
  std::string str_add_to_group_key_prefix;
  std::string str_combine_with_group_key_prefix;
  std::string str_jump_to_group_key_prefix;

 private:
  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  int int4_hotkey_count = 0;

  std::vector<HotkeyReference> vec_select_group_hotkeys_;
  std::vector<HotkeyReference> vec_create_group_hotkeys_;
  std::vector<HotkeyReference> vec_add_to_group_hotkeys_;
  std::vector<HotkeyReference> vec_combine_with_group_hotkeys_;
  std::vector<HotkeyReference> vec_jump_to_group_hotkeys_;
};

}
