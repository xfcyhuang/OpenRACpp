import std;

#include "ui/widgets/control_groups_widget.hpp"

#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/actor.hpp"
#include "sim/control_groups.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"

namespace ora::ui {

namespace {

void CenterOnActors(gfx::WorldRenderer& world_renderer,
                    const std::vector<sim::Actor*>& vec_actors) {
  if (vec_actors.empty())
    return;

  WVec vec_sum{};
  for (const sim::Actor* ptr_actor : vec_actors) {
    const WPos pos_center = ptr_actor->CenterPosition();
    vec_sum = vec_sum + WVec{pos_center.X, pos_center.Y, pos_center.Z};
  }
  const std::int32_t int4_count = static_cast<std::int32_t>(vec_actors.size());
  world_renderer.Viewport()->Center(
      WPos{vec_sum.X / int4_count, vec_sum.Y / int4_count, vec_sum.Z / int4_count});
}

}  // namespace

ControlGroupsWidget::ControlGroupsWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world;
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_world_renderer = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_world_renderer;

  if (ptr_world_ != nullptr && ptr_world_->ControlGroups() != nullptr)
    int4_hotkey_count = static_cast<int>(ptr_world_->ControlGroups()->Groups().size());
}

bool ControlGroupsWidget::LoadFieldOrProperty(std::string_view str_key,
                                              std::string_view str_value) {
  if (str_key == "SelectGroupKeyPrefix")
    str_select_group_key_prefix = str_value;
  else if (str_key == "CreateGroupKeyPrefix")
    str_create_group_key_prefix = str_value;
  else if (str_key == "AddToGroupKeyPrefix")
    str_add_to_group_key_prefix = str_value;
  else if (str_key == "CombineWithGroupKeyPrefix")
    str_combine_with_group_key_prefix = str_value;
  else if (str_key == "JumpToGroupKeyPrefix")
    str_jump_to_group_key_prefix = str_value;
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void ControlGroupsWidget::Initialize(WidgetArgs& args_args) {
  Widget::Initialize(args_args);

  const auto fn_make = [](const std::string& str_prefix) {
    return [&str_prefix](int int4_i) {
      return Ui::Instance().Hotkey(str_prefix + std::format("{:02}", int4_i + 1));
    };
  };

  vec_select_group_hotkeys_.reserve(static_cast<std::size_t>(int4_hotkey_count));
  vec_create_group_hotkeys_.reserve(static_cast<std::size_t>(int4_hotkey_count));
  vec_add_to_group_hotkeys_.reserve(static_cast<std::size_t>(int4_hotkey_count));
  vec_combine_with_group_hotkeys_.reserve(static_cast<std::size_t>(int4_hotkey_count));
  vec_jump_to_group_hotkeys_.reserve(static_cast<std::size_t>(int4_hotkey_count));
  for (int int4_i = 0; int4_i < int4_hotkey_count; ++int4_i) {
    vec_select_group_hotkeys_.push_back(fn_make(str_select_group_key_prefix)(int4_i));
    vec_create_group_hotkeys_.push_back(fn_make(str_create_group_key_prefix)(int4_i));
    vec_add_to_group_hotkeys_.push_back(fn_make(str_add_to_group_key_prefix)(int4_i));
    vec_combine_with_group_hotkeys_.push_back(
        fn_make(str_combine_with_group_key_prefix)(int4_i));
    vec_jump_to_group_hotkeys_.push_back(fn_make(str_jump_to_group_key_prefix)(int4_i));
  }
}

bool ControlGroupsWidget::HandleKeyPress(const KeyInput& e_input) {
  if (e_input.Event != KeyInputEvent::Down)
    return false;

  for (int int4_i = 0; int4_i < int4_hotkey_count; ++int4_i) {
    if (vec_select_group_hotkeys_[static_cast<std::size_t>(int4_i)].IsActivatedBy(
            e_input)) {
      ptr_world_->ControlGroups()->SelectControlGroup(int4_i);

      if (e_input.MultiTapCount >= 2)
        CenterOnActors(*ptr_world_renderer_,
                       ptr_world_->ControlGroups()->GetActorsInControlGroup(int4_i));

      return true;
    }

    if (vec_create_group_hotkeys_[static_cast<std::size_t>(int4_i)].IsActivatedBy(
            e_input)) {
      ptr_world_->ControlGroups()->CreateControlGroup(int4_i);
      return true;
    }

    if (vec_add_to_group_hotkeys_[static_cast<std::size_t>(int4_i)].IsActivatedBy(
            e_input)) {
      ptr_world_->ControlGroups()->AddSelectionToControlGroup(int4_i);
      return true;
    }

    if (vec_combine_with_group_hotkeys_[static_cast<std::size_t>(int4_i)].IsActivatedBy(
            e_input)) {
      ptr_world_->ControlGroups()->CombineSelectionWithControlGroup(int4_i);
      return true;
    }

    if (vec_jump_to_group_hotkeys_[static_cast<std::size_t>(int4_i)].IsActivatedBy(
            e_input)) {
      CenterOnActors(*ptr_world_renderer_,
                     ptr_world_->ControlGroups()->GetActorsInControlGroup(int4_i));
      return true;
    }
  }

  return false;
}

}  // namespace ora::ui
