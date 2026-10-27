#include "mods/ingame_hud_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "mods/player_resources.hpp"
#include "mods/power.hpp"
#include "mods/production.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/production_support.hpp"
#include "mods/production_tabs_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "mods/radar.hpp"
#include "mods/radar_widget.hpp"
#include "net/order_manager.hpp"
#include "net/replay_recorder.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/resource_bar_widget.hpp"

namespace ora::mods {

namespace {

std::string ToLowerInvariant(std::string str_value) {
  for (char& c : str_value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return str_value;
}

std::vector<std::pair<std::string, fluent::FluentValue>> FluentArgs(
    std::initializer_list<
        std::pair<std::string_view, fluent::FluentValue>> list_args) {
  std::vector<std::pair<std::string, fluent::FluentValue>> vec_args;
  for (const auto& [str_key, value] : list_args)
    vec_args.emplace_back(std::string{str_key}, value);
  return vec_args;
}

fluent::FluentValue FluentInt(std::int64_t int8_value) {
  return fluent::FluentValue{int8_value};
}

fluent::FluentValue FluentString(const std::string& str_value) {
  return fluent::FluentValue{str_value};
}

}

IngameRadarDisplayLogic::IngameRadarDisplayLogic(ora::ui::Widget& widget,
                                                 sim::World& world) {
  color_block_ = core::Color::FromArgb(0, 0, 0, 0);
  auto* ptr_radar = widget.Get<RadarWidget>("RADAR_MINIMAP");
  ptr_radar->fn_is_enabled = [this] { return b_radar_enabled_; };

  auto* ptr_dev_mode =
      world.LocalPlayer()->PlayerActor()->Trait<DeveloperMode>();

  auto* ptr_ticker = widget.Get<ora::ui::LogicTickerWidget>("RADAR_TICKER");
  ptr_ticker->fn_on_tick = [this, ptr_radar, ptr_dev_mode, &world] {
    b_radar_enabled_ = ptr_dev_mode->DisableShroud;
    if (!b_radar_enabled_) {
      for (Actor* ptr_actor :
           world.ActorsHavingTrait<ProvidesRadar>(
               [](ProvidesRadar* ptr_trait) {
                 return !ptr_trait->IsTraitDisabled();
               }))
        if (ptr_actor->Owner() == world.LocalPlayer()) {
          b_radar_enabled_ = true;
          break;
        }
    }

    if (b_radar_enabled_ != b_cached_radar_enabled_ &&
        ora::ui::Ui::HasInstance())
      ora::ui::Ui::Instance().PlayNotification(
          &world.Map().Rules(),
          b_radar_enabled_ ? ptr_radar->str_sound_up
                           : ptr_radar->str_sound_down);
    b_cached_radar_enabled_ = b_radar_enabled_;
  };

  auto* ptr_block =
      widget.GetOrNull<ora::ui::ColorBlockWidget>("RADAR_FADETOBLACK");
  if (ptr_block != nullptr) {
    ptr_radar->fn_animating = [this](float fp4_x) {
      color_block_ =
          core::Color::FromArgb(static_cast<int>(255 * fp4_x), 0, 0, 0);
    };
    ptr_block->fn_is_visible = [this] { return color_block_.A() != 0; };
    ptr_block->fn_get_color = [this] { return color_block_; };
  }
}

ClassicProductionLogic::ClassicProductionLogic(ora::ui::Widget& widget,
                                               sim::World& world)
    : ptr_world_{&world} {
  ptr_palette_ = widget.Get<ProductionPaletteWidget>("PRODUCTION_PALETTE");

  auto* ptr_background = widget.GetOrNull("PALETTE_BACKGROUND");
  auto* ptr_foreground = widget.GetOrNull("PALETTE_FOREGROUND");
  ptr_background_ = ptr_background;
  ptr_foreground_ = ptr_foreground;
  if (ptr_background != nullptr || ptr_foreground != nullptr) {
    if (ptr_background != nullptr) {
      if (auto* ptr_row = ptr_background->GetOrNull("ROW_TEMPLATE"))
        up_background_row_template_ = ptr_background->RemoveChild(*ptr_row);
      if (auto* ptr_cap = ptr_background->GetOrNull("BOTTOM_CAP"))
        ptr_background_bottom_ = ptr_cap;
    }

    if (ptr_foreground != nullptr)
      if (auto* ptr_row = ptr_foreground->GetOrNull("ROW_TEMPLATE"))
        up_foreground_row_template_ = ptr_foreground->RemoveChild(*ptr_row);

    ptr_palette_->fn_on_icon_count_changed =
        [this](int int4_old_count, int int4_icons) {
          UpdateBackground(int4_old_count, int4_icons);
        };
    UpdateBackground(0, 0);
  }

  auto* ptr_types_container = widget.Get("PRODUCTION_TYPES");
  for (ora::ui::Widget* ptr_child : ptr_types_container->Children())
    SetupProductionGroupButton(
        dynamic_cast<ProductionTypeButtonWidget*>(ptr_child));

  auto* ptr_ticker =
      widget.Get<ora::ui::LogicTickerWidget>("PRODUCTION_TICKER");
  ptr_ticker->fn_on_tick = [this, ptr_types_container] {
    if (ptr_palette_->ptr_current_queue == nullptr ||
        ptr_palette_->int4_displayed_icon_count == 0) {
      for (ora::ui::Widget* ptr_child : ptr_types_container->Children()) {
        auto* ptr_button =
            dynamic_cast<ProductionTypeButtonWidget*>(ptr_child);
        if (ptr_button == nullptr || ptr_button->fn_is_disabled())
          continue;

        ptr_button->fn_on_click();
        break;
      }
    }
  };

  auto* ptr_scroll_down =
      widget.GetOrNull<ora::ui::ButtonWidget>("SCROLL_DOWN_BUTTON");
  if (ptr_scroll_down != nullptr) {
    ptr_scroll_down->fn_on_click = [this] { ptr_palette_->ScrollDown(); };
    ptr_scroll_down->fn_is_visible = [this] {
      return ptr_palette_->int4_total_icon_count >
             ptr_palette_->int4_max_icon_row_offset *
                 ptr_palette_->int4_columns;
    };
    ptr_scroll_down->fn_is_disabled = [this] {
      return !ptr_palette_->CanScrollDown();
    };
  }

  auto* ptr_scroll_up =
      widget.GetOrNull<ora::ui::ButtonWidget>("SCROLL_UP_BUTTON");
  if (ptr_scroll_up != nullptr) {
    ptr_scroll_up->fn_on_click = [this] { ptr_palette_->ScrollUp(); };
    ptr_scroll_up->fn_is_visible = [this] {
      return ptr_palette_->int4_total_icon_count >
             ptr_palette_->int4_max_icon_row_offset *
                 ptr_palette_->int4_columns;
    };
    ptr_scroll_up->fn_is_disabled = [this] {
      return !ptr_palette_->CanScrollUp();
    };
  }

  SetMaximumVisibleRows(*ptr_palette_);
}

void ClassicProductionLogic::SetupProductionGroupButton(
    ProductionTypeButtonWidget* ptr_button) {
  if (ptr_button == nullptr)
    return;

  std::vector<ProductionQueue*> vec_queues;
  for (ProductionQueue* ptr_queue :
       ptr_world_->LocalPlayer()->PlayerActor()
           ->TraitsImplementing<ProductionQueue>()) {
    const ProductionQueueInfoData& info_queue = ptr_queue->Info();
    const std::string& str_group = info_queue.str_group.empty()
                                       ? info_queue.str_type
                                       : info_queue.str_group;
    if (str_group == ptr_button->ProductionGroup())
      vec_queues.push_back(ptr_queue);
  }

  const auto fn_select_tab = [this, vec_queues](bool) {
    ProductionQueue* ptr_first = nullptr;
    for (ProductionQueue* ptr_queue : vec_queues)
      if (ptr_queue->Enabled()) {
        ptr_first = ptr_queue;
        break;
      }
    ptr_palette_->SetCurrentQueue(ptr_first);
    ptr_palette_->ScrollToTop();
    ptr_palette_->PickUpCompletedBuilding();
  };

  ptr_button->fn_is_disabled = [vec_queues] {
    for (ProductionQueue* ptr_queue : vec_queues)
      if (ptr_queue->AnyItemsToBuild())
        return false;
    return true;
  };
  ptr_button->fn_on_mouse_up =
      [fn_select_tab](const MouseInput& mi_input) {
        fn_select_tab(
            HasModifier(mi_input.ModifierFlags, Modifiers::Shift));
      };
  ptr_button->fn_on_key_press =
      [fn_select_tab](const KeyInput& e_input) {
        fn_select_tab(HasModifier(e_input.ModifierFlags, Modifiers::Shift));
      };
  ptr_button->fn_on_click = [fn_select_tab] { fn_select_tab(false); };
  ptr_button->fn_is_highlighted = [this, vec_queues] {
    return std::find(vec_queues.begin(), vec_queues.end(),
                     ptr_palette_->ptr_current_queue) != vec_queues.end();
  };

  const std::string str_chrome_name =
      ToLowerInvariant(ptr_button->ProductionGroup());
  auto* ptr_icon = ptr_button->Get<ora::ui::ImageWidget>("ICON");
  ptr_icon->fn_get_image_name =
      [ptr_button, vec_queues, str_chrome_name]() -> std::string {
        if (ptr_button->fn_is_disabled())
          return str_chrome_name + "-disabled";
        for (ProductionQueue* ptr_queue : vec_queues)
          for (const auto& up_item : ptr_queue->AllQueued())
            if (up_item->Done())
              return str_chrome_name + "-alert";
        return str_chrome_name;
      };
}

void ClassicProductionLogic::UpdateBackground(int, int int4_icons) {
  int int4_rows = std::max(
      ptr_palette_->int4_minimum_rows,
      (int4_icons + ptr_palette_->int4_columns - 1) /
          ptr_palette_->int4_columns);
  int4_rows = std::min(int4_rows, ptr_palette_->int4_maximum_rows);

  if (ptr_background_ != nullptr) {
    std::unique_ptr<ora::ui::Widget> up_bottom;
    if (ptr_background_bottom_ != nullptr)
      up_bottom = ptr_background_->RemoveChild(*ptr_background_bottom_);

    ptr_background_->RemoveChildren();

    const int int4_row_height =
        up_background_row_template_->bounds_widget.int4_height;
    for (int int4_i = 0; int4_i < int4_rows; ++int4_i) {
      auto up_row = up_background_row_template_->Clone();
      up_row->bounds_widget.int4_y = int4_i * int4_row_height;
      ptr_background_->AddChild(std::move(up_row));
    }

    if (ptr_background_bottom_ == nullptr)
      return;

    ptr_background_bottom_->bounds_widget.int4_y =
        int4_rows * int4_row_height;
    ptr_background_->AddChild(std::move(up_bottom));
  }

  if (ptr_foreground_ != nullptr) {
    ptr_foreground_->RemoveChildren();

    const int int4_row_height =
        up_foreground_row_template_->bounds_widget.int4_height;
    for (int int4_i = 0; int4_i < int4_rows; ++int4_i) {
      auto up_row = up_foreground_row_template_->Clone();
      up_row->bounds_widget.int4_y = int4_i * int4_row_height;
      ptr_foreground_->AddChild(std::move(up_row));
    }
  }
}

void ClassicProductionLogic::SetMaximumVisibleRows(
    ProductionPaletteWidget& palette) {
  const int2 int2_resolution = ora::ui::Ui::Instance().Resolution();

  ora::ui::Widget* ptr_container = nullptr;
  if (ora::ui::Ui::Instance().Root() != nullptr)
    ptr_container =
        ora::ui::Ui::Instance().Root()->GetOrNull("SIDEBAR_PRODUCTION");
  if (ptr_container == nullptr)
    return;

  const int int4_max_items_height =
      int2_resolution.Y - ptr_container->bounds_widget.int4_y;
  const int int4_max_icon_row_offset =
      int4_max_items_height / palette.int2_icon_size.Y - 1;
  palette.int4_max_icon_row_offset =
      std::min(int4_max_icon_row_offset, palette.int4_maximum_rows);
}

ProductionTabsLogic::ProductionTabsLogic(ora::ui::Widget& widget,
                                         sim::World& world)
    : ptr_world_{&world} {
  ptr_tabs_ = widget.Get<ProductionTabsWidget>("PRODUCTION_TABS");

  uint8_added_token_ = world.AddActorAddedHandler(
      [this](Actor& actor_a) { ptr_tabs_->ActorChanged(actor_a); });
  uint8_removed_token_ = world.AddActorRemovedHandler(
      [this](Actor& actor_a) { ptr_tabs_->ActorChanged(actor_a); });

  auto* ptr_types_container =
      ora::ui::Ui::Instance().Root()->Get(ptr_tabs_->str_types_container);
  for (ora::ui::Widget* ptr_child : ptr_types_container->Children())
    SetupProductionGroupButton(
        dynamic_cast<ProductionTypeButtonWidget*>(ptr_child));

  auto* ptr_background =
      ora::ui::Ui::Instance().Root()->GetOrNull(
          ptr_tabs_->str_background_container);
  if (ptr_background != nullptr) {
    auto* ptr_palette = static_cast<ProductionPaletteWidget*>(
        ptr_tabs_->ptr_parent->GetOrNull(ptr_tabs_->str_palette_widget));
    if (auto* ptr_template = ptr_background->GetOrNull("ICON_TEMPLATE"))
      up_icon_template_ = ptr_background->RemoveChild(*ptr_template);

    ptr_palette->fn_on_icon_count_changed =
        [this, ptr_background, ptr_palette](int, int int4_new_count) {
          ptr_background->RemoveChildren();

          for (int int4_i = 0; int4_i < int4_new_count; ++int4_i) {
            const int int4_x = int4_i % ptr_palette->int4_columns;
            const int int4_y = int4_i / ptr_palette->int4_columns;

            auto up_bg = up_icon_template_->Clone();
            up_bg->bounds_widget.int4_x =
                ptr_palette->int2_icon_size.X * int4_x;
            up_bg->bounds_widget.int4_y =
                ptr_palette->int2_icon_size.Y * int4_y;
            ptr_background->AddChild(std::move(up_bg));
          }
        };
    ptr_palette->fn_on_icon_count_changed(0, 0);
  }
}

ProductionTabsLogic::~ProductionTabsLogic() {
  if (ptr_world_ != nullptr) {
    ptr_world_->RemoveActorAddedHandler(uint8_added_token_);
    ptr_world_->RemoveActorRemovedHandler(uint8_removed_token_);
  }
}

void ProductionTabsLogic::SetupProductionGroupButton(
    ProductionTypeButtonWidget* ptr_button) {
  if (ptr_button == nullptr)
    return;

  const auto fn_select_tab = [this, ptr_button](bool b_reverse) {
    if (ptr_tabs_->QueueGroupSet() &&
        ptr_tabs_->QueueGroup() == ptr_button->ProductionGroup())
      ptr_tabs_->SelectNextTab(b_reverse);
    else
      ptr_tabs_->SetQueueGroup(ptr_button->ProductionGroup());

    ptr_tabs_->PickUpCompletedBuilding();
  };

  ptr_button->fn_is_disabled = [this, ptr_button] {
    const ProductionTabGroup& group_tabs =
        ptr_tabs_->Groups().at(ptr_button->ProductionGroup());
    for (const ProductionTab& tab : group_tabs.vec_tabs)
      if (!tab.queue->BuildableItems().empty())
        return false;
    return true;
  };
  ptr_button->fn_on_mouse_up =
      [fn_select_tab](const MouseInput& mi_input) {
        fn_select_tab(
            HasModifier(mi_input.ModifierFlags, Modifiers::Shift));
      };
  ptr_button->fn_on_key_press =
      [fn_select_tab](const KeyInput& e_input) {
        fn_select_tab(HasModifier(e_input.ModifierFlags, Modifiers::Shift));
      };
  ptr_button->fn_is_highlighted = [this, ptr_button] {
    return ptr_tabs_->QueueGroupSet() &&
           ptr_tabs_->QueueGroup() == ptr_button->ProductionGroup();
  };

  const std::string str_chrome_name =
      ToLowerInvariant(ptr_button->ProductionGroup());
  auto* ptr_icon = ptr_button->Get<ora::ui::ImageWidget>("ICON");
  ptr_icon->fn_get_image_name =
      [this, ptr_button, str_chrome_name]() -> std::string {
        if (ptr_button->fn_is_disabled())
          return str_chrome_name + "-disabled";
        if (ptr_tabs_->Groups().at(ptr_button->ProductionGroup()).Alert())
          return str_chrome_name + "-alert";
        return str_chrome_name;
      };
}

IngameCashCounterLogic::IngameCashCounterLogic(ora::ui::Widget& widget,
                                               sim::World& world)
    : ptr_world_{&world},
      cache_silo_usage_tooltip_{
          [](const std::tuple<int, int>& values_usage) {
            return fluent::FluentProvider::Instance().GetMessage(
                "label-silo-usage",
                FluentArgs({{"usage", FluentInt(std::get<0>(values_usage))},
                            {"capacity", FluentInt(std::get<1>(values_usage))}}));
          }} {
  ptr_player_ = world.LocalPlayer();
  ptr_player_resources_ = ptr_player_->PlayerActor()->Trait<PlayerResources>();
  int4_display_resources_ = ptr_player_resources_->GetCashAndResources();
  ptr_cash_label_ = widget.Get<ora::ui::LabelWithTooltipWidget>("CASH");
  ptr_cash_label_->fn_get_tooltip_text = [this] {
    return str_silo_usage_tooltip_;
  };
}

void IngameCashCounterLogic::Tick() {
  if (int4_next_cash_tick_time_ > 0)
    int4_next_cash_tick_time_--;

  const int int4_actual = ptr_player_resources_->GetCashAndResources();
  const int int4_diff = std::abs(int4_actual - int4_display_resources_);
  const int int4_move = std::min(
      std::max(static_cast<int>(int4_diff * kDisplayFracPerFrame),
               kDisplayDeltaPerFrame),
      int4_diff);
  if (int4_display_resources_ < int4_actual) {
    int4_display_resources_ += int4_move;
    if (ora::ui::Ui::Instance().CashTicks())
      ora::ui::Ui::Instance().PlayNotification(
          &ptr_world_->Map().Rules(),
          ptr_player_resources_->Info().str_cash_tick_up_notification);
  } else if (int4_display_resources_ > int4_actual) {
    int4_display_resources_ -= int4_move;
    if (ora::ui::Ui::Instance().CashTicks() &&
        int4_next_cash_tick_time_ == 0) {
      ora::ui::Ui::Instance().PlayNotification(
          &ptr_world_->Map().Rules(),
          ptr_player_resources_->Info().str_cash_tick_down_notification);
      int4_next_cash_tick_time_ = 2;
    }
  }

  str_silo_usage_tooltip_ = cache_silo_usage_tooltip_.Update(
      {ptr_player_resources_->Resources,
       ptr_player_resources_->ResourceCapacity});
  const std::string str_display_resources_text =
      std::to_string(int4_display_resources_);
  ptr_cash_label_->fn_get_text =
      [str_display_resources_text] { return str_display_resources_text; };
}

IngamePowerCounterLogic::IngamePowerCounterLogic(ora::ui::Widget& widget,
                                                 sim::World& world) {
  auto* ptr_developer_mode =
      world.LocalPlayer()->PlayerActor()->Trait<DeveloperMode>();
  auto* ptr_power_manager =
      world.LocalPlayer()->PlayerActor()->Trait<PowerManager>();
  auto* ptr_power = widget.Get<ora::ui::LabelWithTooltipWidget>("POWER");
  auto* ptr_power_icon = widget.Get<ora::ui::ImageWidget>("POWER_ICON");
  const std::string str_unlimited_capacity =
      fluent::FluentProvider::Instance().GetMessage("label-infinite-power");

  ptr_power_icon->fn_get_image_name =
      [ptr_power_manager]() -> std::string {
        return ptr_power_manager->ExcessPower() < 0 ? "power-critical"
                                                    : "power-normal";
      };
  ptr_power->fn_get_color = [ptr_power_manager] {
    return ptr_power_manager->ExcessPower() < 0
               ? core::Color::FromArgb(255, 255, 0, 0)
               : core::Color::FromArgb(255, 255, 255, 255);
  };
  ptr_power->fn_get_text =
      [ptr_developer_mode, ptr_power_manager,
       str_unlimited_capacity]() -> std::string {
        return ptr_developer_mode->UnlimitedPower
                   ? str_unlimited_capacity
                   : std::to_string(ptr_power_manager->ExcessPower());
      };

  auto cache_tooltip_text =
      core::CachedTransform<std::tuple<int, std::optional<int>>,
                            std::string>(
          [str_unlimited_capacity](
              const std::tuple<int, std::optional<int>>& values_usage) {
            const std::string str_capacity =
                std::get<1>(values_usage).has_value()
                    ? std::to_string(*std::get<1>(values_usage))
                    : str_unlimited_capacity;
            return fluent::FluentProvider::Instance().GetMessage(
                "label-power-usage",
                FluentArgs(
                    {{"usage", FluentString(std::to_string(
                                   std::get<0>(values_usage)))},
                     {"capacity", FluentString(str_capacity)}}));
          });
  ptr_power->fn_get_tooltip_text =
      [ptr_developer_mode, ptr_power_manager,
       cache_tooltip_text = std::move(cache_tooltip_text)]() mutable
      -> std::string {
        const std::optional<int> opt_capacity =
            ptr_developer_mode->UnlimitedPower
                ? std::nullopt
                : std::optional<int>{ptr_power_manager->PowerProvided};
        return cache_tooltip_text.Update(
            {ptr_power_manager->PowerDrained, opt_capacity});
      };
}

IngameSiloBarLogic::IngameSiloBarLogic(ora::ui::Widget& widget,
                                       sim::World& world) {
  auto* ptr_player_resources =
      world.LocalPlayer()->PlayerActor()->Trait<PlayerResources>();
  auto* ptr_silo_bar = widget.Get<ora::ui::ResourceBarWidget>("SILOBAR");

  ptr_silo_bar->fn_get_provided = [ptr_player_resources] {
    return static_cast<float>(ptr_player_resources->ResourceCapacity);
  };
  ptr_silo_bar->fn_get_used = [ptr_player_resources] {
    return static_cast<float>(ptr_player_resources->Resources);
  };
  ptr_silo_bar->cache_tooltip_text =
      core::CachedTransform<std::tuple<float, float>, std::string>(
          [](const std::tuple<float, float>& values_usage) {
            return fluent::FluentProvider::Instance().GetMessage(
                "label-silo-usage",
                FluentArgs(
                    {{"usage",
                      FluentInt(static_cast<std::int64_t>(
                          std::get<0>(values_usage)))},
                     {"capacity",
                      FluentInt(static_cast<std::int64_t>(
                          std::get<1>(values_usage)))}}));
          });
  ptr_silo_bar->fn_get_bar_color = [ptr_player_resources] {
    if (ptr_player_resources->Resources ==
        ptr_player_resources->ResourceCapacity)
      return core::Color::FromArgb(255, 255, 0, 0);

    if (ptr_player_resources->Resources >=
        0.8 * ptr_player_resources->ResourceCapacity)
      return core::Color::FromArgb(255, 255, 165, 0);

    return core::Color::FromArgb(255, 50, 205, 50);
  };
}

GameTimerLogic::GameTimerLogic(ora::ui::Widget& widget,
                               net::OrderManager& order_manager,
                               sim::World& world) {
  auto* ptr_timer = widget.GetOrNull<ora::ui::LabelWidget>("GAME_TIMER");
  auto* ptr_status =
      widget.GetOrNull<ora::ui::LabelWidget>("GAME_TIMER_STATUS");
  const int int4_start_tick = ora::ui::Ui::Instance().LocalTick();

  const auto fn_should_show_status = [&world,
                                      int4_start_tick]() -> bool {
    return (world.Paused() || world.ReplayTimestep() != world.Timestep()) &&
           (ora::ui::Ui::Instance().LocalTick() - int4_start_tick) / 1000 %
                   2 ==
               0;
  };
  const auto fn_paused = [&world]() -> bool {
    return world.Paused() || world.ReplayTimestep() == 0;
  };
  const std::string str_paused_text =
      fluent::FluentProvider::Instance().GetMessage("label-paused");
  const std::string str_max_speed_text =
      fluent::FluentProvider::Instance().GetMessage("label-max-speed");
  auto cache_speed_text = core::CachedTransform<int, std::string>(
      [](int int4_percentage) {
        return fluent::FluentProvider::Instance().GetMessage(
            "label-replay-speed",
            FluentArgs({{"percentage", FluentInt(int4_percentage)}}));
      });

  if (ptr_timer != nullptr) {
    ptr_timer->fn_get_text =
        [ptr_status, fn_should_show_status, fn_paused, str_paused_text,
         &world]() -> std::string {
          if (ptr_status == nullptr && fn_paused() &&
              fn_should_show_status())
            return str_paused_text;

          return ora::ui::FormatTime(std::max(0, world.WorldTick()),
                                     world.Timestep());
        };
  }

  if (ptr_status != nullptr) {
    ptr_status->fn_is_visible = fn_should_show_status;
    ptr_status->fn_get_text =
        [fn_paused, str_paused_text, str_max_speed_text,
         cache_speed_text = std::move(cache_speed_text),
         &world]() mutable -> std::string {
          if (fn_paused())
            return str_paused_text;
          if (world.ReplayTimestep() == 1)
            return str_max_speed_text;
          return cache_speed_text.Update(world.Timestep() * 100 /
                                         world.ReplayTimestep());
        };
  }

  auto cache_complete_text = core::CachedTransform<int, std::string>(
      [](int int4_percentage) {
        return fluent::FluentProvider::Instance().GetMessage(
            "label-replay-complete",
            FluentArgs({{"percentage", FluentInt(int4_percentage)}}));
      });
  if (auto* ptr_timer_tooltip =
          dynamic_cast<ora::ui::LabelWithTooltipWidget*>(ptr_timer)) {
    auto* ptr_connection =
        dynamic_cast<net::ReplayConnection*>(&order_manager.Connection());
    if (ptr_connection != nullptr &&
        ptr_connection->FinalGameTick() != 0) {
      const int int4_final_tick = ptr_connection->FinalGameTick();
      ptr_timer_tooltip->fn_get_tooltip_text =
          [cache_complete_text = std::move(cache_complete_text),
           &world, int4_final_tick]() mutable -> std::string {
            return cache_complete_text.Update(world.WorldTick() * 100 /
                                              int4_final_tick);
          };
    } else if (ptr_connection != nullptr &&
               ptr_connection->ReplayTickCount() != 0) {
      const int int4_tick_count = ptr_connection->ReplayTickCount();
      ptr_timer_tooltip->fn_get_tooltip_text =
          [cache_complete_text = std::move(cache_complete_text),
           &order_manager, int4_tick_count]() mutable -> std::string {
            return cache_complete_text.Update(
                order_manager.NetFrameNumber() * 100 / int4_tick_count);
          };
    } else {
      ptr_timer_tooltip->fn_get_tooltip_text = nullptr;
    }
  }
}

void RegisterIngameHudLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();
  registry_logics.Register(
      "IngameRadarDisplayLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<IngameRadarDisplayLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "ClassicProductionLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<ClassicProductionLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "ProductionTabsLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<ProductionTabsLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "IngameCashCounterLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<IngameCashCounterLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "IngamePowerCounterLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<IngamePowerCounterLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "IngameSiloBarLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<IngameSiloBarLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });
  registry_logics.Register(
      "GameTimerLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<GameTimerLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<net::OrderManager*>(args_args.At("orderManager")),
            *std::get<sim::World*>(args_args.At("world")));
      });
}

}
