#include "mods/menu_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/mission_objectives.hpp"
#include "mods/order_generators.hpp"
#include "mods/support_powers_widget.hpp"
#include "net/session.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "sound/sound.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/background_widget.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/menu_button_widget.hpp"
#include "mods/production_tabs_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

namespace ora::mods {

namespace {

sim::IOrderGenerator* OrderGeneratorOf(sim::World& world,
                                       const std::type_info& info_type) {
  sim::IOrderGenerator* generator = world.OrderGenerator();
  if (generator == nullptr)
    return nullptr;
  return typeid(*generator) == info_type ? generator : nullptr;
}

}

AddFactionSuffixLogic::AddFactionSuffixLogic(ora::ui::Widget& widget,
                                             sim::World& world) {
  const sim::Player* ptr_local = world.LocalPlayer();
  if (ptr_local == nullptr || ptr_local->Spectating())
    return;

  std::string str_faction;
  ora::ui::ChromeMetrics* ptr_metrics =
      ora::ui::Ui::Instance().ChromeMetricsDeps();
  if (ptr_metrics == nullptr ||
      !ptr_metrics->TryGet("FactionSuffix-" + ptr_local->Faction().InternalName,
                           str_faction))
    str_faction = ptr_local->Faction().InternalName;
  const std::string str_suffix = "-" + str_faction;

  if (auto* button = dynamic_cast<ora::ui::ButtonWidget*>(&widget)) {
    button->str_background += str_suffix;
    return;
  }
  if (auto* image = dynamic_cast<ora::ui::ImageWidget*>(&widget)) {
    image->str_image_collection += str_suffix;
    return;
  }
  if (auto* background =
          dynamic_cast<ora::ui::BackgroundWidget*>(&widget)) {
    background->str_background += str_suffix;
    return;
  }
  if (auto* text_field =
          dynamic_cast<ora::ui::TextFieldWidget*>(&widget)) {
    text_field->str_background += str_suffix;
    return;
  }
  if (auto* scroll_panel =
          dynamic_cast<ora::ui::ScrollPanelWidget*>(&widget)) {
    scroll_panel->str_button += str_suffix;
    scroll_panel->str_background += str_suffix;
    scroll_panel->str_scrollbar_background += str_suffix;
    scroll_panel->str_decorations += str_suffix;
    return;
  }
  if (auto* tabs = dynamic_cast<ProductionTabsWidget*>(&widget)) {
    tabs->str_arrow_button += str_suffix;
    tabs->str_tab_button += str_suffix;
    tabs->str_decorations += str_suffix;
    tabs->str_background += str_suffix;
    return;
  }

  throw std::runtime_error(
      "AddFactionSuffixLogic only supports ButtonWidget, ImageWidget, "
      "BackgroundWidget, TextFieldWidget, ScrollPanelWidget and "
      "ProductionTabsWidget");
}

SupportPowerBinLogic::SupportPowerBinLogic(ora::ui::Widget& widget) {
  auto* ptr_palette = widget.Get<SupportPowersWidget>("SUPPORT_PALETTE");

  ora::ui::Widget* ptr_background = widget.GetOrNull("PALETTE_BACKGROUND");
  ora::ui::Widget* ptr_foreground = widget.GetOrNull("PALETTE_FOREGROUND");
  if (ptr_background == nullptr && ptr_foreground == nullptr)
    return;

  auto up_background_template = std::make_shared<
      std::unique_ptr<ora::ui::Widget>>();
  auto up_foreground_template = std::make_shared<
      std::unique_ptr<ora::ui::Widget>>();
  if (ptr_background != nullptr)
    *up_background_template = ptr_background->RemoveChild(
        *ptr_background->Get("ICON_TEMPLATE"));
  if (ptr_foreground != nullptr)
    *up_foreground_template = ptr_foreground->RemoveChild(
        *ptr_foreground->Get("ICON_TEMPLATE"));

  auto fn_update = [ptr_palette, ptr_background, ptr_foreground,
                    up_background_template = std::move(up_background_template),
                    up_foreground_template =
                        std::move(up_foreground_template)](int, int int4_icons) {
    const int int4_row_height =
        ptr_palette->int2_icon_size.Y + ptr_palette->int4_icon_margin;
    const int int4_row_width =
        ptr_palette->int2_icon_size.X + ptr_palette->int4_icon_margin;

    if (ptr_background != nullptr) {
      ptr_background->RemoveChildren();
      for (int i = 0; i < int4_icons; i++) {
        std::unique_ptr<ora::ui::Widget> up_row =
            (*up_background_template)->Clone();
        if (ptr_palette->b_horizontal)
          up_row->bounds_widget.int4_x += i * int4_row_width;
        else
          up_row->bounds_widget.int4_y += i * int4_row_height;
        ptr_background->AddChild(std::move(up_row));
      }
    }

    if (ptr_foreground != nullptr) {
      ptr_foreground->RemoveChildren();
      for (int i = 0; i < int4_icons; i++) {
        std::unique_ptr<ora::ui::Widget> up_row =
            (*up_foreground_template)->Clone();
        if (ptr_palette->b_horizontal)
          up_row->bounds_widget.int4_x += i * int4_row_width;
        else
          up_row->bounds_widget.int4_y += i * int4_row_height;
        ptr_foreground->AddChild(std::move(up_row));
      }
    }
  };

  ptr_palette->fn_on_icon_count_changed = std::move(fn_update);
  ptr_palette->fn_on_icon_count_changed(0, 0);
}

namespace {

template <class GeneratorT>
void SetupChromeOrderButton(ora::ui::Widget& w, sim::World& world,
                            const std::string& str_icon) {
  auto* widget = dynamic_cast<ora::ui::ButtonWidget*>(&w);
  if (widget == nullptr)
    return;

  widget->fn_on_click = [&world] {
    if (OrderGeneratorOf(world, typeid(GeneratorT)) != nullptr) {
      world.CancelInputMode();
    } else {
      world.AdoptOrderGenerator(std::make_unique<GeneratorT>(world));
    }
  };

  widget->fn_is_highlighted = [&world] {
    return OrderGeneratorOf(world, typeid(GeneratorT)) != nullptr;
  };

  if (auto* icon = widget->template GetOrNull<ora::ui::ImageWidget>("ICON"))
    icon->fn_get_image_name = [&world, str_icon]() -> std::string {
      return OrderGeneratorOf(world, typeid(GeneratorT)) != nullptr
                 ? str_icon + "-active"
                 : str_icon;
    };
}

}

SellOrderButtonLogic::SellOrderButtonLogic(ora::ui::Widget& widget,
                                           sim::World& world) {
  SetupChromeOrderButton<SellOrderGenerator>(
      widget, world, "sell");
}

RepairOrderButtonLogic::RepairOrderButtonLogic(ora::ui::Widget& widget,
                                               sim::World& world) {
  SetupChromeOrderButton<RepairOrderGenerator>(
      widget, world, "repair");
}

PowerdownOrderButtonLogic::PowerdownOrderButtonLogic(ora::ui::Widget& widget,
                                                     sim::World& world) {
  SetupChromeOrderButton<PowerDownOrderGenerator>(
      widget, world, "power");
}

BeaconOrderButtonLogic::BeaconOrderButtonLogic(ora::ui::Widget& widget,
                                               sim::World& world) {
  SetupChromeOrderButton<BeaconOrderGenerator>(
      widget, world, "beacon");
}

MenuButtonsChromeLogic::MenuButtonsChromeLogic(ora::ui::Widget& widget,
                                               sim::World& world)
    : ptr_world_{&world} {
  ptr_world_root_ = ora::ui::Ui::Instance().Root()->Get("WORLD_ROOT");
  ptr_menu_root_ = ora::ui::Ui::Instance().Root()->Get("MENU_ROOT");

  if (auto* options =
          widget.GetOrNull<ora::ui::MenuButtonWidget>("OPTIONS_BUTTON")) {
    auto b_blinking = std::make_shared<bool>(false);
    sim::Player* ptr_lp = world.LocalPlayer();
    options->fn_is_disabled = [this] { return b_disable_system_buttons_; };
    options->fn_on_click =
        [this, options, b_blinking]() mutable {
          *b_blinking = false;
          OpenMenuPanel(*options, ora::ui::WidgetArgs{}
                                      .With("initialPanel",
                                            static_cast<std::int32_t>(
                                                IngameInfoPanel::AutoSelect)));
        };
    options->fn_is_highlighted = [b_blinking] {
      return *b_blinking &&
             ora::ui::Ui::Instance().LocalTick() % 50 < 25;
    };

    if (ptr_lp != nullptr) {
      if (auto* mo =
              ptr_lp->PlayerActor()->TraitOrDefault<MissionObjectives>()) {
        auto b_blinking_handler = b_blinking;
        n_objective_added_token_ = mo->AddObjectiveAddedHandler(
            [b_blinking_handler, &world, ptr_lp](sim::Player& player,
                                                 bool b_inhibit) {
              if (!b_inhibit && &player == world.LocalPlayer() &&
                  world.LocalPlayer() == ptr_lp)
                *b_blinking_handler = true;
            });
        ptr_mission_objectives_ = mo;
      }
    }
  }

  if (auto* debug =
          widget.GetOrNull<ora::ui::MenuButtonWidget>("DEBUG_BUTTON")) {
    bool b_enabled = false;
    if (const game::ActorInfo* ptr_player_actor =
            world.Map().Rules().FindActor("player")) {
      for (const meta::RecordObject* rec :
           ptr_player_actor->TraitsInConstructOrder()) {
        if (rec->record_desc().str_name !=
            std::string_view{"DeveloperModeInfo"})
          continue;
        if (const auto* gv =
                sim::RecordFieldValue(*rec, "CheckboxEnabled"))
          if (const auto* b = std::get_if<bool>(&gv->val))
            b_enabled = *b;
      }
    }

    b_enabled = world.LobbyInfo().global_settings.OptionOrDefault(
        "cheats", b_enabled);
    debug->fn_is_visible = [b_enabled] { return b_enabled; };
    debug->fn_is_disabled = [this] { return b_disable_system_buttons_; };
    debug->fn_on_click = [this, debug] {
      OpenMenuPanel(*debug, ora::ui::WidgetArgs{}
                                 .With("initialPanel",
                                 static_cast<std::int32_t>(
                                     IngameInfoPanel::Debug)));
    };
  }
}

MenuButtonsChromeLogic::~MenuButtonsChromeLogic() {
  if (ptr_mission_objectives_ != nullptr && n_objective_added_token_ != 0)
    ptr_mission_objectives_->RemoveObjectiveAddedHandler(
        n_objective_added_token_);
}

void MenuButtonsChromeLogic::OpenMenuPanel(
    ora::ui::MenuButtonWidget& button, ora::ui::WidgetArgs args_widget) {
  b_disable_system_buttons_ = true;
  const bool b_cached_pause = ptr_world_->PredictedPaused();

  if (button.b_hide_ingame_ui) {
    ptr_world_->CancelInputMode();
    ptr_world_root_->fn_is_visible = [] { return false; };
  }

  if (button.b_pause && ptr_world_->LobbyInfo().NonBotClients().size() == 1)
    ptr_world_->SetPauseState(true);

  const bool b_cached_disable_world_sounds =
      ora::ui::Ui::Instance().SoundDeps() != nullptr &&
      ora::ui::Ui::Instance().SoundDeps()->DisableWorldSounds();
  if (button.b_disable_world_sounds && ora::ui::Ui::Instance().SoundDeps())
    ora::ui::Ui::Instance().SoundDeps()->SetDisableWorldSounds(true);

  args_widget = args_widget.With(
      "onExit", [this, &button, b_cached_pause,
                 b_cached_disable_world_sounds]() {
        if (button.b_hide_ingame_ui)
          ptr_world_root_->fn_is_visible = [] { return true; };

        if (button.b_disable_world_sounds &&
            ora::ui::Ui::Instance().SoundDeps())
          ora::ui::Ui::Instance().SoundDeps()->SetDisableWorldSounds(
              b_cached_disable_world_sounds);

        if (button.b_pause &&
            ptr_world_->LobbyInfo().NonBotClients().size() == 1)
          ptr_world_->SetPauseState(b_cached_pause);

        ptr_menu_root_->RemoveChild(*ptr_current_widget_);
        b_disable_system_buttons_ = false;
      });

  ora::ui::WidgetArgs args_load = args_widget;
  args_load = args_load.With("world", ptr_world_);
  ptr_current_widget_ = ora::ui::Ui::Instance().LoadWidget(
      button.str_menu_container, ptr_menu_root_, std::move(args_load));
  ora::ui::Ui::Instance().RunAfterTickDefer(
      [] { ora::ui::Ui::Instance().ResetTooltips(); });
}

void RegisterMenuLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();
  registry_logics.Register(
      "AddFactionSuffixLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<AddFactionSuffixLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "SupportPowerBinLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<SupportPowerBinLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")));
      });
  registry_logics.Register(
      "SellOrderButtonLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<SellOrderButtonLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "RepairOrderButtonLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<RepairOrderButtonLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "PowerdownOrderButtonLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<PowerdownOrderButtonLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "BeaconOrderButtonLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<BeaconOrderButtonLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "MenuButtonsChromeLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<MenuButtonsChromeLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
}

}
