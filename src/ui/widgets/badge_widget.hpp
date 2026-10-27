#pragma once
import std;

#include "ui/player_database.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class BadgeWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "BadgeWidget";

  BadgeWidget() = default;
  explicit BadgeWidget(WidgetArgs& args_args) {
    (void)args_args;
  }
  explicit BadgeWidget(const BadgeWidget& widget_other)
      : Widget(widget_other),
        opt_badge(widget_other.opt_badge),
        ptr_player_database_(widget_other.ptr_player_database_) {}
  ~BadgeWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new BadgeWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Draw() override;

  std::optional<PlayerBadge> opt_badge;

 private:
  PlayerDatabase* ptr_player_database_ = nullptr;

  PlayerDatabase& Database() {
    if (ptr_player_database_ == nullptr)
      ptr_player_database_ = &PlayerDatabase::Instance();
    return *ptr_player_database_;
  }
};

}
