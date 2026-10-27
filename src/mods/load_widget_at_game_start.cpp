#include "mods/load_widget_at_game_start.hpp"

#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/menu_button_widget.hpp"

namespace ora::mods {

LoadWidgetAtGameStartInfoData LoadWidgetAtGameStartInfoData::Parse(
    const meta::RecordObject& rec_info) {
  LoadWidgetAtGameStartInfoData data{};
  if (const auto v = sim::RecordFieldString(rec_info, "ShellmapRoot"))
    data.str_shellmap_root = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "IngameRoot"))
    data.str_ingame_root = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "EditorRoot"))
    data.str_editor_root = std::string{*v};
  if (const auto v =
          sim::RecordFieldString(rec_info, "GameSaveLoadingRoot"))
    data.str_game_save_loading_root = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "ClearRoot"))
    data.b_clear_root = *v != 0;
  return data;
}

LoadWidgetAtGameStart::LoadWidgetAtGameStart(
    LoadWidgetAtGameStartInfoData&& info)
    : info_{std::move(info)} {}

void LoadWidgetAtGameStart::GameLoading(sim::World& world) {
  auto& ui_face = ui::Ui::Instance();
  if (info_.b_clear_root)
    ui_face.ResetAll();

  ui::WidgetArgs args_panel{};
  args_panel.Add("world", &world);
  ui_face.OpenWindow(info_.str_game_save_loading_root, std::move(args_panel));
}

ui::Widget* LoadWidgetAtGameStart::LoadRootWidget(sim::World& world) {
  const std::string& str_widget =
      world.Type() == sim::WorldType::Shellmap
          ? info_.str_shellmap_root
          : world.Type() == sim::WorldType::Editor ? info_.str_editor_root
                                                   : info_.str_ingame_root;
  auto& ui_face = ui::Ui::Instance();
  ui::WidgetArgs args_empty{};
  const auto& deps_ui = ui_face.DepsForTest();
  if (deps_ui.fn_game_load_widget)
    return deps_ui.fn_game_load_widget(world, str_widget, ui_face.Root(),
                                       args_empty);
  if (deps_ui.fn_load_widget)
    return ui_face.LoadWidget(str_widget, ui_face.Root(),
                              std::move(args_empty));
  auto up_root = std::make_unique<ui::Widget>();
  up_root->str_id = str_widget;
  ui::Widget* ptr_root = up_root.get();
  ui_face.Root()->AddChild(std::move(up_root));
  return ptr_root;
}

void LoadWidgetAtGameStart::WorldLoaded(
    sim::World& world, gfx::WorldRenderer* ptr_world_renderer) {
  (void)ptr_world_renderer;
  if (!world.IsLoadingGameSave() && info_.b_clear_root)
    ui::Ui::Instance().ResetAll();

  ptr_root_ = LoadRootWidget(world);

  if (world.IsLoadingGameSave())
    ptr_root_->fn_is_visible = [] { return false; };
}

void LoadWidgetAtGameStart::GameLoaded(sim::World& world) {
  auto& ui_face = ui::Ui::Instance();
  ui_face.CloseWindow();
  if (ptr_root_ != nullptr)
    ptr_root_->fn_is_visible = [] { return true; };

  if (!world.IsReplay()) {
    ui::MenuButtonWidget* ptr_options_button =
        ptr_root_ != nullptr
            ? ptr_root_->GetOrNull<ui::MenuButtonWidget>("OPTIONS_BUTTON")
            : nullptr;
    world.SetPauseState(false);
    if (ptr_options_button != nullptr) {
      auto fn_on_click = ptr_options_button->fn_on_click;
      sim::RunUnsynced(true, &world, [&fn_on_click] { fn_on_click(); });
    }
  }
}

void RegisterLoadWidgetAtGameStart() {
  sim::TraitRegistry::Instance().Register(
      "LoadWidgetAtGameStartInfo",
      [](const meta::RecordObject& rec_info, sim::ActorInitializer& init,
         ora::WorldArena& arena) -> sim::TraitBase* {
        return arena.Create<LoadWidgetAtGameStart>(
            LoadWidgetAtGameStartInfoData::Parse(rec_info));
      });
}

}
