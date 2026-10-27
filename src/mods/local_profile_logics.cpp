#include "mods/local_profile_logics.hpp"

#include "game/local_player_profile.hpp"

#include "gfx/sprite_font.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/badge_widget.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::mods {

LoadLocalPlayerProfileLogic::LoadLocalPlayerProfileLogic(
    ora::ui::Widget& widget_root, sim::World& world) {
  auto& ui_face = ui::Ui::Instance();
  std::function<bool()> fn_minimal_profile = [&ui_face] {
    return ui_face.CurrentWindow() != nullptr;
  };

  ui::WidgetArgs args_panel{};
  args_panel.Add("minimalProfile", fn_minimal_profile);
  const auto& deps_ui = ui_face.DepsForTest();
  if (deps_ui.fn_game_load_widget) {
    deps_ui.fn_game_load_widget(world, "LOCAL_PROFILE_PANEL", &widget_root,
                                args_panel);
  } else if (deps_ui.fn_load_widget) {
    ui_face.LoadWidget("LOCAL_PROFILE_PANEL", &widget_root,
                       std::move(args_panel));
  } else {
    auto up_panel = std::make_unique<ui::Widget>();
    up_panel->str_id = "LOCAL_PROFILE_PANEL";
    widget_root.AddChild(std::move(up_panel));
  }
}

LocalProfileLogic::LocalProfileLogic(
    ora::ui::Widget& widget_root, gfx::WorldRenderer* ptr_world_renderer,
    std::function<bool()> fn_minimal_profile)
    : widget_root_{widget_root},
      ptr_world_renderer_{ptr_world_renderer},
      up_alive_{std::make_shared<bool>(true)} {
  auto& profile = game::LocalPlayerProfileInstance();

  widget_root.Get("CHECKING_FINGERPRINT")->fn_is_visible =
      [&profile, fn_minimal_profile] {
        return profile.kind_state ==
                   game::LinkState::CheckingLink &&
               !fn_minimal_profile();
      };
  widget_root.Get("LINK_ACCOUNT")->fn_is_visible =
      [&profile, fn_minimal_profile] {
        return profile.kind_state < game::LinkState::ConnectionFailed &&
               !fn_minimal_profile();
      };
  widget_root.Get("CONNECTION_ERROR")->fn_is_visible =
      [&profile, fn_minimal_profile] {
        return profile.kind_state ==
                   game::LinkState::ConnectionFailed &&
               !fn_minimal_profile();
      };

  if (auto* ptr_settings_button =
          ui::Ui::Instance().Root()->GetOrNull<ui::ButtonWidget>(
              "SETTINGS_BUTTON"))
    widget_root.Get<ui::ButtonWidget>("LINK_BUTTON")->fn_on_click =
        ptr_settings_button->fn_on_click;
  widget_root.Get<ui::ButtonWidget>("RETRY_BUTTON")->fn_on_click =
      [&profile] { game::ProfileRefreshPlayerData(profile); };

  widget_root.Get("PROFILE_HEADER")->fn_is_visible = [&profile] {
    return profile.kind_state == game::LinkState::Linked;
  };
  widget_root.Get<ui::LabelWidget>("PROFILE_NAME")->fn_get_text =
      [&profile] { return profile.profile_data.str_profile_name; };
  widget_root.Get<ui::LabelWidget>("PROFILE_RANK")->fn_get_text =
      [&profile] { return profile.profile_data.str_profile_rank; };

  ui::ButtonWidget& button_destroy =
      *widget_root.Get<ui::ButtonWidget>("DESTROY_KEY");
  button_destroy.fn_on_click = [&profile] {
    game::ProfileDeleteKeypair(profile);
  };
  button_destroy.fn_is_disabled = fn_minimal_profile;

  ptr_badge_container_ = widget_root.Get("BADGES_CONTAINER");
  ptr_badge_container_->fn_is_visible = [this, fn_minimal_profile, &profile] {
    return b_badges_visible_ && !fn_minimal_profile() &&
           profile.kind_state == game::LinkState::Linked;
  };

  fn_unsubscribe_ = game::ProfileAddStateChangedHandler(
      profile, [this] { RefreshBadges(); });
  RefreshBadges();
}

LocalProfileLogic::~LocalProfileLogic() {
  *up_alive_ = false;
  if (fn_unsubscribe_)
    fn_unsubscribe_();
}

void LocalProfileLogic::RefreshBadges() {
  auto& ui_face = ui::Ui::Instance();
  const std::shared_ptr<bool> up_alive = up_alive_;
  ui_face.RunAfterTickDefer([this, up_alive, &ui_face] {
    if (!*up_alive)
      return;
    auto& profile = game::LocalPlayerProfileInstance();
    b_badges_visible_ = false;

    ptr_badge_container_->RemoveChildren();

    if (profile.kind_state == game::LinkState::Linked &&
        !profile.profile_data.vec_badges.empty()) {
      std::function<int(int)> fn_negotiate_width =
          [this](int) {
            return widget_root_.Get("PROFILE_HEADER")
                ->bounds_widget.int4_width;
          };

      ui::WidgetArgs args_badges{};
      args_badges.Add("worldRenderer", ptr_world_renderer_);
      args_badges.Add("profile", &profile.profile_data);
      args_badges.Add("negotiateWidth", fn_negotiate_width);
      ui::Widget* ptr_badges = ui_face.LoadWidget(
          "PLAYER_PROFILE_BADGES_INSERT", ptr_badge_container_,
          std::move(args_badges));

      if (ptr_badges != nullptr &&
          ptr_badges->bounds_widget.int4_height > 0) {
        ptr_badge_container_->bounds_widget.int4_height =
            ptr_badges->bounds_widget.int4_height;
        b_badges_visible_ = true;
      }
    }

    ui_face.ResetTooltips();
  });
}

PlayerProfileBadgesLogic::PlayerProfileBadgesLogic(
    ora::ui::Widget& widget_root, const game::PlayerProfile* ptr_profile,
    std::function<int(int)> fn_negotiate_width) {
  const bool b_show_badges = !ptr_profile->vec_badges.empty();
  widget_root.fn_is_visible = [b_show_badges] { return b_show_badges; };

  up_badge_template_ =
      widget_root.RemoveChild(*widget_root.Get("BADGE_TEMPLATE"));

  int int4_max_label_width = 0;
  const ui::Widget* ptr_template_icon = up_badge_template_->Get("ICON");
  const ui::LabelWidget* ptr_template_label =
      up_badge_template_->Get<ui::LabelWidget>("LABEL");
  const ui::LabelWidget& label_template = *ptr_template_label;
  gfx::SpriteFont& font_template =
      ui::RequireWidgetFont(label_template.str_font);
  for (const auto& badge : ptr_profile->vec_badges)
    int4_max_label_width = std::max(
        int4_max_label_width,
        font_template.Measure(badge.str_label).X);

  widget_root.bounds_widget.int4_width = fn_negotiate_width(
      2 * ptr_template_label->bounds_widget.Left() -
          ptr_template_icon->bounds_widget.Right() + int4_max_label_width);

  int int4_badge_offset = up_badge_template_->bounds_widget.Top();
  if (!ptr_profile->vec_badges.empty())
    int4_badge_offset += 3;

  for (const auto& badge : ptr_profile->vec_badges) {
    auto up_clone = up_badge_template_->Clone();
    ui::BadgeWidget* ptr_icon = up_clone->Get<ui::BadgeWidget>("ICON");
    ptr_icon->opt_badge = ui::PlayerBadge{
        badge.str_label, badge.str_icon, badge.str_icon2x, badge.str_icon3x};

    ui::LabelWidget* ptr_label = up_clone->Get<ui::LabelWidget>("LABEL");
    gfx::SpriteFont& font_label =
        ui::RequireWidgetFont(ptr_label->str_font);

    const std::string str_label_text = ui::TruncateText(
        badge.str_label,
        widget_root.bounds_widget.int4_width -
            ptr_label->bounds_widget.Left() - ptr_icon->bounds_widget.Left(),
        font_label);
    ptr_label->fn_get_text = [str_label_text] { return str_label_text; };

    up_clone->bounds_widget.int4_y = int4_badge_offset;
    widget_root.AddChild(std::move(up_clone));

    int4_badge_offset += up_badge_template_->bounds_widget.int4_height;
  }

  if (int4_badge_offset > up_badge_template_->bounds_widget.Top())
    int4_badge_offset += 5;

  widget_root.bounds_widget.int4_height = int4_badge_offset;
}

void RegisterLocalProfileLogics() {
  auto& registry = ui::ChromeLogicRegistry::Instance();

  registry.Register(
      "LoadLocalPlayerProfileLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        return std::make_unique<LoadLocalPlayerProfileLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });

  registry.Register(
      "LocalProfileLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const ui::WidgetArgValue* ptr_value =
                args_args.TryGet("worldRenderer"))
          if (const auto* ptr_wr = std::get_if<gfx::WorldRenderer*>(ptr_value))
            ptr_world_renderer = *ptr_wr;
        std::function<bool()> fn_minimal_profile = [] { return false; };
        if (const ui::WidgetArgValue* ptr_value =
                args_args.TryGet("minimalProfile"))
          if (const auto* ptr_fn =
                  std::get_if<std::function<bool()>>(ptr_value))
            fn_minimal_profile = *ptr_fn;
        return std::make_unique<LocalProfileLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")), ptr_world_renderer,
            std::move(fn_minimal_profile));
      });

  registry.Register(
      "PlayerProfileBadgesLogic",
      [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::ChromeLogic> {
        const auto* ptr_profile =
            std::get<game::PlayerProfile*>(args_args.At("profile"));
        auto fn_negotiate_width =
            std::get<std::function<int(int)>>(args_args.At("negotiateWidth"));
        return std::make_unique<PlayerProfileBadgesLogic>(
            *std::get<ui::Widget*>(args_args.At("widget")), ptr_profile,
            std::move(fn_negotiate_width));
      });
}

}  // namespace ora::mods
