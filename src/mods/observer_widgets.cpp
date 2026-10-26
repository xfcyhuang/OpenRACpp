// UPSTREAM: OpenRA.Mods.Common/Widgets/ObserverArmyIconsWidget.cs @1f594e320
//          L24-211 + ObserverProductionIconsWidget.cs L25-281 +
//          ObserverSupportPowerIconsWidget.cs L23-199 的实现面
//          The implementation face of the three observer icon strips.
#include "mods/observer_widgets.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "mods/production.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

core::Vector2 Int2ToVector2(int2 v) {
  return core::Vector2{static_cast<float>(v.X), static_cast<float>(v.Y)};
}

void AdjustWidthHack(ora::ui::Widget& widget_self, int int4_new_width,
                     int int4_min_width, int int4_icons_extent) {
  const bool b_was_in_bounds = widget_self.EventBounds().Contains(
      gfx::Viewport::int2_last_mouse_pos);
  widget_self.bounds_widget.int4_width = int4_new_width;
  const bool b_is_in_bounds = widget_self.EventBounds().Contains(
      gfx::Viewport::int2_last_mouse_pos);

  if (b_was_in_bounds != b_is_in_bounds)
    ui::Ui::Instance().RunAfterTickDefer([] {
      if (ui::Ui::HasInstance())
        ui::Ui::Instance().ResetTooltips();
    });

  ora::ui::Widget* ptr_parent = widget_self.ptr_parent;
  if (ptr_parent == nullptr)
    return;

  ptr_parent->bounds_widget.int4_width =
      widget_self.bounds_widget.int4_x + widget_self.bounds_widget.int4_width;

  auto* ptr_gradient =
      ptr_parent->Get<ora::ui::GradientColorBlockWidget>("PLAYER_GRADIENT");

  const int int4_offset =
      ptr_gradient->bounds_widget.int4_x - widget_self.bounds_widget.int4_x;
  ptr_gradient->bounds_widget.int4_width =
      std::max(int4_min_width - int4_offset, int4_icons_extent);

  ora::ui::Widget* ptr_grandparent = ptr_parent->ptr_parent;
  if (ptr_grandparent == nullptr)
    return;

  int int4_widest_child_width = 0;
  for (const ora::ui::Widget* child : ptr_grandparent->Children())
    int4_widest_child_width =
        std::max(int4_widest_child_width,
                 child->bounds_widget.int4_width);

  ptr_grandparent->bounds_widget.int4_width =
      std::max(25 + int4_widest_child_width,
               widget_self.bounds_widget.Left() + int4_min_width);
}

}  // namespace

// ———— ObserverArmyIconsWidget ————
// ———— ObserverArmyIconsWidget ————

ObserverArmyIconsWidget::ObserverArmyIconsWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr_arg;

  fn_get_tooltip_unit = [this] { return ptr_tooltip_unit_; };
}

ObserverArmyIconsWidget::ObserverArmyIconsWidget(
    const ObserverArmyIconsWidget& widget_other)
    : ora::ui::Widget(widget_other),
      fn_get_player(widget_other.fn_get_player),
      ptr_world_(widget_other.ptr_world_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      int4_icon_width(widget_other.int4_icon_width),
      int4_icon_height(widget_other.int4_icon_height),
      int4_icon_spacing(widget_other.int4_icon_spacing),
      int4_min_width(widget_other.int4_min_width),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container) {
  fn_get_tooltip_unit = [this] { return ptr_tooltip_unit_; };
}

void ObserverArmyIconsWidget::Draw() {
  vec_army_icons_.clear();

  if (!fn_get_player)
    return;
  sim::Player* ptr_player = fn_get_player();
  if (ptr_player == nullptr)
    return;

  if (ptr_stats_player_ != ptr_player) {
    ptr_stats_player_ = ptr_player;
    ptr_stats_ =
        ptr_player->PlayerActor()->TraitOrDefault<PlayerStatistics>();
  }

  if (ptr_stats_ == nullptr)
    return;

  std::vector<ArmyUnit*> vec_items;
  for (const auto& [str_name, unit] : ptr_stats_->Units())
    if (unit->Count > 0 && unit->Icon() != nullptr)
      vec_items.push_back(unit.get());
  std::stable_sort(vec_items.begin(), vec_items.end(),
                   [](const ArmyUnit* a, const ArmyUnit* b) {
                     if (a->ProductionQueueOrder() !=
                         b->ProductionQueueOrder())
                       return a->ProductionQueueOrder() <
                              b->ProductionQueueOrder();
                     return a->BuildPaletteOrder() < b->BuildPaletteOrder();
                   });

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->EnableAntialiasingFilter();

  const core::Vector2 vec_icon_size{
      static_cast<float>(int4_icon_width),
      static_cast<float>(int4_icon_height)};
  int int4_queue_col = 0;
  for (ArmyUnit* unit : vec_items) {
    gfx::Animation* ptr_icon = const_cast<gfx::Animation*>(unit->Icon());
    const int2 int2_top_left_offset{
        int4_queue_col * (int4_icon_width + int4_icon_spacing), 0};
    const int2 int2_icon_top_left = RenderOrigin() + int2_top_left_offset;

    const std::string str_palette =
        unit->IconPaletteIsPlayerPalette()
            ? unit->IconPalette() + ptr_player->InternalName()
            : unit->IconPalette();
    ui::DrawSpriteCentered(
        ptr_icon->Image(), ptr_world_renderer_->Palette(str_palette),
        Int2ToVector2(int2_icon_top_left) + 0.5f * vec_icon_size, 0.5f);

    ArmyIcon army_icon;
    army_icon.rect_bounds =
        Rectangle{int2_icon_top_left.X, int2_icon_top_left.Y,
                  static_cast<int>(vec_icon_size.X),
                  static_cast<int>(vec_icon_size.Y)};
    army_icon.ptr_unit = unit;
    vec_army_icons_.push_back(army_icon);

    int4_queue_col++;
  }

  const int int4_new_width =
      std::max(int4_queue_col * (int4_icon_width + int4_icon_spacing),
               int4_min_width);
  if (int4_new_width != bounds_widget.int4_width)
    AdjustWidthHack(*this, int4_new_width, int4_min_width,
                    int4_queue_col * (int4_icon_width + int4_icon_spacing));

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->DisableAntialiasingFilter();

  if (gfx::SpriteFont* ptr_bold =
          ui::Ui::Instance().TryGetFontDeps("TinyBold")) {
    for (const ArmyIcon& army_icon : vec_army_icons_) {
      const std::string str_text =
          std::to_string(army_icon.ptr_unit->Count);
      ptr_bold->DrawTextWithContrast(
          str_text,
          core::Vector2{static_cast<float>(army_icon.rect_bounds.Left()), static_cast<float>(army_icon.rect_bounds.Top())} +
              core::Vector2{vec_icon_size.X, 0} -
              core::Vector2{
                  static_cast<float>(ptr_bold->Measure(str_text).X),
                  static_cast<float>(ptr_bold->TopOffset())},
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    }
  }
}

void ObserverArmyIconsWidget::Tick() {
  if (str_tooltip_container.empty())
    return;

  if (!b_tooltip_container_materialized_) {
    if (ui::Ui::Instance().Root() == nullptr)
      return;
    b_tooltip_container_materialized_ = true;
    ptr_tooltip_container_widget_ =
        ui::Ui::Instance().Root()->Get<ora::ui::TooltipContainerWidget>(
            str_tooltip_container);
  }

  if (ui::Ui::Instance().MouseOverWidget() != this) {
    if (ptr_tooltip_unit_ != nullptr) {
      ptr_tooltip_container_widget_->RemoveTooltip(
          int4_current_tooltip_token_);
      int4_last_icon_idx_ = 0;
      ptr_tooltip_unit_ = nullptr;
    }

    return;
  }

  if (ptr_tooltip_unit_ != nullptr &&
      int4_last_icon_idx_ < static_cast<int>(vec_army_icons_.size())) {
    const ArmyIcon& army_icon = vec_army_icons_[int4_last_icon_idx_];
    if (army_icon.ptr_unit->ActorInfo() ==
            ptr_tooltip_unit_->ActorInfo() &&
        army_icon.rect_bounds.Contains(gfx::Viewport::int2_last_mouse_pos))
      return;
  }

  for (int i = 0; i < static_cast<int>(vec_army_icons_.size()); i++) {
    if (!vec_army_icons_[i].rect_bounds.Contains(
            gfx::Viewport::int2_last_mouse_pos))
      continue;

    int4_last_icon_idx_ = i;
    ptr_tooltip_unit_ = vec_army_icons_[i].ptr_unit;
    int4_current_tooltip_token_ =
        ptr_tooltip_container_widget_->SetTooltip(
            str_tooltip_template, ora::ui::WidgetArgs{{"player", fn_get_player ? fn_get_player() : nullptr}});

    return;
  }

  ptr_tooltip_unit_ = nullptr;
}

// ———— ObserverProductionIconsWidget ————
// ———— ObserverProductionIconsWidget ————

ObserverProductionIconsWidget::ObserverProductionIconsWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr_arg;

  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };
}

ObserverProductionIconsWidget::ObserverProductionIconsWidget(
    const ObserverProductionIconsWidget& widget_other)
    : ora::ui::Widget(widget_other),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container),
      ptr_world_(widget_other.ptr_world_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      int4_icon_width(widget_other.int4_icon_width),
      int4_icon_height(widget_other.int4_icon_height),
      int4_icon_spacing(widget_other.int4_icon_spacing),
      int4_min_width(widget_other.int4_min_width),
      str_clock_animation(widget_other.str_clock_animation),
      str_clock_sequence(widget_other.str_clock_sequence),
      str_clock_palette(widget_other.str_clock_palette) {
  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };
}

void ObserverProductionIconsWidget::Draw() {
  vec_production_icons_.clear();
  vec_production_icons_bounds_.clear();

  if (!fn_get_player)
    return;
  sim::Player* ptr_player = fn_get_player();
  if (ptr_player == nullptr)
    return;

  std::vector<ProductionQueue*> vec_queues;
  for (const auto& [actor, trait] :
       ptr_world_->ActorsWithTrait<ProductionQueue>())
    if (actor->Owner() == ptr_player)
      vec_queues.push_back(trait);

  for (ProductionQueue* queue : vec_queues)
    if (map_clocks_.find(queue) == map_clocks_.end())
      map_clocks_.emplace(queue, std::make_unique<gfx::Animation>(
                                     RenderSprites::MakeAnimationDeps(*ptr_world_),
                                     str_clock_animation));

  std::vector<ProductionItemLike*> vec_current_items;
  for (ProductionQueue* q : vec_queues)
    if (ProductionItemLike* pi = q->CurrentItem())
      vec_current_items.push_back(pi);

  std::stable_sort(vec_current_items.begin(), vec_current_items.end(),
                   [](const ProductionItemLike* a,
                      const ProductionItemLike* b) {
                     if (a->Queue->Info().int4_display_order !=
                         b->Queue->Info().int4_display_order)
                       return a->Queue->Info().int4_display_order <
                              b->Queue->Info().int4_display_order;
                     return a->BuildPaletteOrder() < b->BuildPaletteOrder();
                   });

  std::vector<std::vector<ProductionItemLike*>> vec_grouped_items;
  for (ProductionItemLike* pi : vec_current_items) {
    if (!vec_grouped_items.empty() &&
        vec_grouped_items.back().front()->Item == pi->Item) {
      vec_grouped_items.back().push_back(pi);
      continue;
    }
    vec_grouped_items.push_back({pi});
  }

  int int4_group_count = 0;
  for (auto& vec_queued : vec_grouped_items) {
    std::stable_sort(vec_queued.begin(), vec_queued.end(),
                     [](const ProductionItemLike* a,
                        const ProductionItemLike* b) {
                       const int ra = a->Done() ? 0 : (a->Paused() ? 2 : 1);
                       const int rb = b->Done() ? 0 : (b->Paused() ? 2 : 1);
                       if (ra != rb)
                         return ra < rb;
                       return a->Queue->RemainingTimeActual(*a) <
                              b->Queue->RemainingTimeActual(*b);
                     });
    int4_group_count++;
  }

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->EnableAntialiasingFilter();

  const core::Vector2 vec_icon_size{
      static_cast<float>(int4_icon_width),
      static_cast<float>(int4_icon_height)};
  int int4_queue_col = 0;
  for (const std::vector<ProductionItemLike*>& vec_queued :
       vec_grouped_items) {
    ProductionItemLike* current = vec_queued.front();
    ProductionQueue* queue = current->Queue;
    const std::string str_faction =
        queue->Actor()->Owner()->Faction().InternalName;

    const game::ActorInfo* ptr_actor = nullptr;
    for (const game::ActorInfo* a : queue->AllItems())
      if (a->Name() == current->Item) {
        ptr_actor = a;
        break;
      }
    if (ptr_actor == nullptr)
      continue;

    std::optional<BuildableInfoData> opt_bi;
    const meta::RecordObject* rec_rsi = nullptr;
    for (const meta::RecordObject* rec_trait :
         ptr_actor->TraitsInConstructOrder()) {
      const std::string_view str_full =
          rec_trait->record_desc().str_full_name;
      if (str_full ==
          std::string_view{"OpenRA.Mods.Common.Traits.BuildableInfo"})
        opt_bi = BuildableInfoData::Parse(*rec_trait);
      else if (str_full ==
               std::string_view{
                   "OpenRA.Mods.Common.Traits.Render.RenderSpritesInfo"})
        rec_rsi = rec_trait;
    }
    if (!opt_bi.has_value() || rec_rsi == nullptr)
      continue;
    const BuildableInfoData& bi = *opt_bi;

    gfx::Animation icon(RenderSprites::MakeAnimationDeps(*ptr_world_),
                        RenderSpritesInfoData::Parse(*rec_rsi).GetImage(
                            *ptr_actor, str_faction));
    icon.Play(bi.str_icon);
    const core::Vector2 vec_icon_top_left =
        Int2ToVector2(RenderOrigin()) +
        core::Vector2{
            static_cast<float>(
                int4_queue_col * (int4_icon_width + int4_icon_spacing)),
            0};
    const core::Vector2 vec_center = vec_icon_top_left + 0.5f * vec_icon_size;

    const std::string str_palette =
        bi.b_icon_palette_is_player_palette
            ? bi.str_icon_palette + ptr_player->InternalName()
            : bi.str_icon_palette;
    ui::DrawSpriteCentered(
        icon.Image(), ptr_world_renderer_->Palette(str_palette),
        vec_center, 0.5f);

    const Rectangle rect{
        static_cast<int>(vec_icon_top_left.X),
        static_cast<int>(vec_icon_top_left.Y),
        static_cast<int>(vec_icon_size.X),
        static_cast<int>(vec_icon_size.Y)};
    ProductionIcon production_icon;
    production_icon.actor = ptr_actor;
    production_icon.name = current->Item;
    production_icon.pos = core::Vector2{static_cast<float>(rect.Left()), static_cast<float>(rect.Top())};
    production_icon.vec_queued = vec_queued;
    production_icon.production_queue = queue;
    vec_production_icons_.push_back(std::move(production_icon));
    vec_production_icons_bounds_.push_back(rect);

    for (auto* pio : queue->Actor()->Owner()
                         ->PlayerActor()
                         ->TraitsImplementing<sim::IProductionIconOverlay>())
      if (pio->IsOverlayActive(*ptr_actor)) {
        const gfx::Sprite* ptr_overlay_sprite = pio->OverlaySprite();
        if (ptr_overlay_sprite != nullptr)
          ui::DrawSpriteCentered(
              *ptr_overlay_sprite,
              ptr_world_renderer_->Palette(
                  std::string{pio->OverlayPalette()}),
              vec_center + pio->OverlayOffset(vec_icon_size), 0.5f);
      }

    gfx::Animation& clock = *map_clocks_[queue];
    const int int4_total = current->TotalTime();
    clock.PlayFetchIndex(str_clock_sequence, [&clock, current, int4_total]() {
      return int4_total == 0
                 ? 0
                 : (int4_total - current->RemainingTime()) *
                       (clock.CurrentSequence()->Length() - 1) / int4_total;
    });

    clock.Tick();
    ui::DrawSpriteCentered(
        clock.Image(), ptr_world_renderer_->Palette(str_clock_palette),
        vec_center, 0.5f);

    int4_queue_col++;
  }

  const int int4_new_width =
      std::max(int4_queue_col * (int4_icon_width + int4_icon_spacing),
               int4_min_width);
  if (int4_new_width != bounds_widget.int4_width)
    AdjustWidthHack(*this, int4_new_width, int4_min_width,
                    int4_group_count * (int4_icon_width + int4_icon_spacing));

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->DisableAntialiasingFilter();

  gfx::SpriteFont* ptr_tiny =
      ui::Ui::Instance().TryGetFontDeps("Tiny");
  gfx::SpriteFont* ptr_bold =
      ui::Ui::Instance().TryGetFontDeps("Small");
  if (ptr_tiny == nullptr || ptr_bold == nullptr)
    return;

  for (const ProductionIcon& icon : vec_production_icons_) {
    const ProductionItemLike* current = icon.vec_queued.front();
    std::string str_text;
    if (current->Paused())
      str_text = "ON HOLD";
    else if (current->Done())
      str_text = "READY";
    else
      str_text = ui::FormatTime(
          icon.production_queue->RemainingTimeActual(*current),
          ptr_world_->Timestep());

    ptr_tiny->DrawTextWithContrast(
        str_text,
        icon.pos + core::Vector2{16, 12} -
            core::Vector2{
                static_cast<float>(ptr_tiny->Measure(str_text).X / 2), 0},
        core::Color::FromArgb(255, 255, 255, 255),
        core::Color::FromArgb(255, 0, 0, 0), 1);

    if (icon.vec_queued.size() > 1) {
      const std::string str_count =
          std::to_string(icon.vec_queued.size());
      ptr_bold->DrawTextWithContrast(
          str_count,
          icon.pos + core::Vector2{16, 0} -
              core::Vector2{
                  static_cast<float>(ptr_bold->Measure(str_count).X / 2),
                  0},
          core::Color::FromArgb(255, 255, 255, 255),
          core::Color::FromArgb(255, 0, 0, 0), 1);
    }
  }
}

void ObserverProductionIconsWidget::Tick() {
  if (str_tooltip_container.empty())
    return;

  if (!b_tooltip_container_materialized_) {
    if (ui::Ui::Instance().Root() == nullptr)
      return;
    b_tooltip_container_materialized_ = true;
    ptr_tooltip_container_widget_ =
        ui::Ui::Instance().Root()->Get<ora::ui::TooltipContainerWidget>(
            str_tooltip_container);
  }

  if (ui::Ui::Instance().MouseOverWidget() != this) {
    if (ptr_tooltip_icon_ != nullptr) {
      ptr_tooltip_container_widget_->RemoveTooltip(
          int4_current_tooltip_token_);
      int4_last_icon_idx_ = 0;
      ptr_tooltip_icon_ = nullptr;
    }

    return;
  }

  if (ptr_tooltip_icon_ != nullptr &&
      static_cast<int>(vec_production_icons_bounds_.size()) >
          int4_last_icon_idx_ &&
      vec_production_icons_[int4_last_icon_idx_].actor ==
          ptr_tooltip_icon_->actor &&
      vec_production_icons_bounds_[int4_last_icon_idx_].Contains(
          gfx::Viewport::int2_last_mouse_pos))
    return;

  for (int i = 0;
       i < static_cast<int>(vec_production_icons_bounds_.size()); i++) {
    if (!vec_production_icons_bounds_[i].Contains(
            gfx::Viewport::int2_last_mouse_pos))
      continue;

    int4_last_icon_idx_ = i;
    ptr_tooltip_icon_ = &vec_production_icons_[i];
    int4_current_tooltip_token_ =
        ptr_tooltip_container_widget_->SetTooltip(
            str_tooltip_template,
            ora::ui::WidgetArgs{
                {"player", fn_get_player ? fn_get_player() : nullptr}});
    return;
  }

  ptr_tooltip_icon_ = nullptr;
}

// ———— ObserverSupportPowerIconsWidget ————
// ———— ObserverSupportPowerIconsWidget ————

ObserverSupportPowerIconsWidget::ObserverSupportPowerIconsWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr_arg;

  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };
}

ObserverSupportPowerIconsWidget::ObserverSupportPowerIconsWidget(
    const ObserverSupportPowerIconsWidget& widget_other)
    : ora::ui::Widget(widget_other),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container),
      ptr_world_(widget_other.ptr_world_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      int4_icon_width(widget_other.int4_icon_width),
      int4_icon_height(widget_other.int4_icon_height),
      int4_icon_spacing(widget_other.int4_icon_spacing),
      str_clock_animation(widget_other.str_clock_animation),
      str_clock_sequence(widget_other.str_clock_sequence),
      str_clock_palette(widget_other.str_clock_palette) {
  fn_get_tooltip_icon = [this] { return ptr_tooltip_icon_; };
}

void ObserverSupportPowerIconsWidget::Draw() {
  vec_support_power_icons_.clear();
  vec_support_power_icons_bounds_.clear();

  if (!fn_get_player)
    return;
  sim::Player* ptr_player = fn_get_player();
  if (ptr_player == nullptr)
    return;

  SupportPowerManager* ptr_spm =
      ptr_player->PlayerActor()->TraitOrDefault<SupportPowerManager>();
  if (ptr_spm == nullptr)
    return;

  std::vector<std::pair<const std::string*, SupportPowerInstance*>> vec_powers;
  for (const auto& [str_key, power] : ptr_spm->Powers())
    if (!power->Disabled())
      vec_powers.emplace_back(&str_key, power.get());
  std::stable_sort(vec_powers.begin(), vec_powers.end(),
                   [](const auto& a, const auto& b) {
                     return a.second->Info()->int4_support_power_palette_order <
                            b.second->Info()->int4_support_power_palette_order;
                   });

  for (const auto& [str_key, power] : vec_powers)
    if (map_clocks_.find(*str_key) == map_clocks_.end())
      map_clocks_.emplace(*str_key,
                          std::make_unique<gfx::Animation>(
                              RenderSprites::MakeAnimationDeps(*ptr_world_),
                              str_clock_animation));

  bounds_widget.int4_width =
      static_cast<int>(vec_powers.size()) *
      (int4_icon_width + int4_icon_spacing);

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->EnableAntialiasingFilter();

  const core::Vector2 vec_icon_size{
      static_cast<float>(int4_icon_width),
      static_cast<float>(int4_icon_height)};
  for (int i = 0; i < static_cast<int>(vec_powers.size()); i++) {
    const auto& [str_key, item] = vec_powers[i];
    if (item->Info() == nullptr || item->Info()->str_icon.empty())
      continue;

    gfx::Animation icon(RenderSprites::MakeAnimationDeps(*ptr_world_),
                        item->Info()->str_icon_image);
    icon.Play(item->Info()->str_icon);
    const core::Vector2 vec_location =
        core::Vector2{static_cast<float>(RenderBounds().Left()), static_cast<float>(RenderBounds().Top())} +
        core::Vector2{
            static_cast<float>(i * (int4_icon_width + int4_icon_spacing)),
            0};

    SupportPowerIcon power_icon;
    power_icon.ptr_power = item;
    power_icon.pos = vec_location;
    vec_support_power_icons_.push_back(std::move(power_icon));
    vec_support_power_icons_bounds_.push_back(Rectangle{
        static_cast<int>(vec_location.X), static_cast<int>(vec_location.Y),
        static_cast<int>(vec_icon_size.X),
        static_cast<int>(vec_icon_size.Y)});

    ui::DrawSpriteCentered(
        icon.Image(),
        ptr_world_renderer_->Palette(item->Info()->str_icon_palette),
        vec_location + 0.5f * vec_icon_size, 0.5f);

    gfx::Animation& clock = *map_clocks_[*str_key];
    const SupportPowerInstance* sp = item;
    clock.PlayFetchIndex(str_clock_sequence,
                         [&clock, sp]() -> std::int32_t {
                           if (sp->TotalTicks() == 0)
                             return 0;
                           return (sp->TotalTicks() - sp->RemainingTicks()) *
                                  (clock.CurrentSequence()->Length() - 1) /
                                  sp->TotalTicks();
                         });

    clock.Tick();
    ui::DrawSpriteCentered(
        clock.Image(), ptr_world_renderer_->Palette(str_clock_palette),
        vec_location + 0.5f * vec_icon_size, 0.5f);
  }

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
    ptr_renderer->DisableAntialiasingFilter();

  gfx::SpriteFont* ptr_tiny =
      ui::Ui::Instance().TryGetFontDeps("Tiny");
  if (ptr_tiny == nullptr)
    return;

  for (const SupportPowerIcon& icon : vec_support_power_icons_) {
    const SupportPowerInstance* item = icon.ptr_power;
    std::string str_text;
    if (item->Disabled())
      str_text = "ON HOLD";
    else if (item->Ready())
      str_text = "READY";
    else
      str_text = ui::FormatTime(item->RemainingTicks(),
                                             ptr_world_->Timestep());

    ptr_tiny->DrawTextWithContrast(
        str_text,
        icon.pos + core::Vector2{16, 12} -
            core::Vector2{
                static_cast<float>(ptr_tiny->Measure(str_text).X / 2), 0},
        core::Color::FromArgb(255, 255, 255, 255),
        core::Color::FromArgb(255, 0, 0, 0), 1);
  }
}

void ObserverSupportPowerIconsWidget::Tick() {
  if (str_tooltip_container.empty())
    return;

  if (!b_tooltip_container_materialized_) {
    if (ui::Ui::Instance().Root() == nullptr)
      return;
    b_tooltip_container_materialized_ = true;
    ptr_tooltip_container_widget_ =
        ui::Ui::Instance().Root()->Get<ora::ui::TooltipContainerWidget>(
            str_tooltip_container);
  }

  if (ui::Ui::Instance().MouseOverWidget() != this) {
    if (ptr_tooltip_icon_ != nullptr) {
      ptr_tooltip_container_widget_->RemoveTooltip(
          int4_current_tooltip_token_);
      int4_last_icon_idx_ = 0;
      ptr_tooltip_icon_ = nullptr;
    }

    return;
  }

  if (ptr_tooltip_icon_ != nullptr &&
      int4_last_icon_idx_ <
          static_cast<int>(vec_support_power_icons_bounds_.size()) &&
      vec_support_power_icons_[int4_last_icon_idx_].ptr_power ==
          ptr_tooltip_icon_->ptr_power &&
      vec_support_power_icons_bounds_[int4_last_icon_idx_].Contains(
          gfx::Viewport::int2_last_mouse_pos))
    return;

  for (int i = 0;
       i < static_cast<int>(vec_support_power_icons_bounds_.size()); i++) {
    if (!vec_support_power_icons_bounds_[i].Contains(
            gfx::Viewport::int2_last_mouse_pos))
      continue;

    int4_last_icon_idx_ = i;
    ptr_tooltip_icon_ = &vec_support_power_icons_[i];
    int4_current_tooltip_token_ =
        ptr_tooltip_container_widget_->SetTooltip(
            str_tooltip_template,
            ora::ui::WidgetArgs{
                {"world", static_cast<sim::World*>(ptr_world_)},
                {"player", fn_get_player ? fn_get_player() : nullptr}});
    return;
  }

  ptr_tooltip_icon_ = nullptr;
}

void RegisterObserverWidgets() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "ObserverArmyIconsWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ObserverArmyIconsWidget>(args_args);
        });
    ui::WidgetTypeRegistry::Instance().Register(
        "ObserverProductionIconsWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ObserverProductionIconsWidget>(args_args);
        });
    ui::WidgetTypeRegistry::Instance().Register(
        "ObserverSupportPowerIconsWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ObserverSupportPowerIconsWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
