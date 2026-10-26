// UPSTREAM: OpenRA.Mods.Common/Widgets/MapPreviewWidget.cs @1f594e320 L22-234(全文;
// SpawnOccupant 三 ctor + 缩放布局/出生点覆盖/禁用面)。minimap 柄 =
// map::MapPreview::GetMinimap 的 uintptr(Sprite* 重释;D 注册)。
import std;

#include "ui/widgets/map_preview_widget.hpp"

#include "fluent/fluent_provider.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/viewport.hpp"
#include "map/map_cache.hpp"
#include "meta/field_loader.hpp"
#include "net/session.hpp"
#include "server/game_information.hpp"
#include "server/game_server.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

SpawnOccupant::SpawnOccupant(const net::SessionClient& client)
    : color_player{client.Color},
      str_player_name{client.IsBot()
                          ? fluent::FluentProvider::Instance().GetMessage(client.Name)
                          : client.Name},
      int4_team{client.Team},
      str_faction{client.Faction},
      int4_spawn_point{client.SpawnPoint} {}

SpawnOccupant::SpawnOccupant(const server::GameInformationPlayer& player)
    : color_player{player.Color},
      str_player_name{player.IsBot
                          ? fluent::FluentProvider::Instance().GetMessage(player.Name)
                          : player.Name},
      int4_team{player.Team},
      str_faction{player.FactionId},
      int4_spawn_point{player.SpawnPoint} {}

SpawnOccupant::SpawnOccupant(const server::GameClientData& player, bool b_suppress_faction)
    : color_player{player.Color},
      str_player_name{player.IsBot
                          ? fluent::FluentProvider::Instance().GetMessage(player.Name)
                          : player.Name},
      int4_team{player.Team},
      str_faction{!b_suppress_faction ? player.Faction : std::string{}},
      int4_spawn_point{player.SpawnPoint} {}

MapPreviewWidget::MapPreviewWidget() {
  ChromeMetrics& metrics_chrome = RequireChromeMetrics();
  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();

  sprite_spawn_claimed_ = ptr_chrome->GetImage("lobby-bits", "spawn-claimed");
  sprite_spawn_unclaimed_ = ptr_chrome->GetImage("lobby-bits", "spawn-unclaimed");
  sprite_spawn_disabled_ = ptr_chrome->GetImage("lobby-bits", "spawn-disabled");
  ptr_spawn_font_ = &RequireWidgetFont(metrics_chrome.Get("SpawnFont"));
  color_spawn_ = metrics_chrome.GetColor("SpawnColor");
  color_spawn_contrast = metrics_chrome.GetColor("SpawnContrastColor");
  int2_spawn_label_offset = metrics_chrome.GetInt2("SpawnLabelOffset");
}

MapPreviewWidget::MapPreviewWidget(const MapPreviewWidget& widget_other)
    : Widget(widget_other),
      b_ignore_mouse_input(widget_other.b_ignore_mouse_input),
      b_show_spawn_points(widget_other.b_show_spawn_points),
      str_tooltip_container(widget_other.str_tooltip_container),
      str_tooltip_template(widget_other.str_tooltip_template),
      fn_preview(widget_other.fn_preview),
      fn_spawn_occupants(widget_other.fn_spawn_occupants),
      fn_disabled_spawn_points(widget_other.fn_disabled_spawn_points),
      int4_tooltip_spawn_index(widget_other.int4_tooltip_spawn_index),
      b_show_unoccupied_spawnpoints(widget_other.b_show_unoccupied_spawnpoints) {
  ChromeMetrics& metrics_chrome = RequireChromeMetrics();
  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();

  sprite_spawn_claimed_ = ptr_chrome->GetImage("lobby-bits", "spawn-claimed");
  sprite_spawn_unclaimed_ = ptr_chrome->GetImage("lobby-bits", "spawn-unclaimed");
  ptr_spawn_font_ = &RequireWidgetFont(metrics_chrome.Get("SpawnFont"));
  color_spawn_ = metrics_chrome.GetColor("SpawnColor");
  color_spawn_contrast = metrics_chrome.GetColor("SpawnContrastColor");
  int2_spawn_label_offset = metrics_chrome.GetInt2("SpawnLabelOffset");
}

bool MapPreviewWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (b_ignore_mouse_input)
    return Widget::HandleMouseInput(mi_input);

  if (mi_input.Event != MouseInputEvent::Down)
    return false;

  fn_on_mouse_down(mi_input);
  return true;
}

TooltipContainerWidget& MapPreviewWidget::TooltipContainerValue() {
  if (!b_tooltip_container_created_) {
    ptr_tooltip_container_ =
        Ui::Instance().Root()->Get<TooltipContainerWidget>(str_tooltip_container);
    b_tooltip_container_created_ = true;
  }
  return *ptr_tooltip_container_;
}

void MapPreviewWidget::MouseEntered() {
  if (!str_tooltip_container.empty()) {
    WidgetArgs args_tooltip;
    args_tooltip.Add("preview", static_cast<Widget*>(this));
    args_tooltip.Add("showUnoccupiedSpawnpoints", b_show_unoccupied_spawnpoints);
    TooltipContainerValue().SetTooltip(str_tooltip_template, std::move(args_tooltip));
  }
}

void MapPreviewWidget::MouseExited() {
  if (!str_tooltip_container.empty())
    TooltipContainerValue().RemoveTooltip();
}

int2 MapPreviewWidget::ConvertToPreview(CPos cell, MapGridType grid_type) {
  map::MapPreview* ptr_preview = fn_preview();
  const MPos point = cell.ToMPos(grid_type);
  const int int4_cell_width = grid_type == MapGridType::RectangularIsometric ? 2 : 1;
  const int int4_dx = static_cast<int>(fp4_preview_scale_ * int4_cell_width *
                                       (point.U - ptr_preview->Bounds().Left()));
  const int int4_dy =
      static_cast<int>(fp4_preview_scale_ * (point.V - ptr_preview->Bounds().Top()));

  if ((point.V & 1) == 1)
    return int2{rect_map_.X + int4_dx + 1, rect_map_.Y + int4_dy};

  return int2{rect_map_.X + int4_dx, rect_map_.Y + int4_dy};
}

void MapPreviewWidget::UpdateMinimap(const gfx::Sprite& sprite_minimap) {
  sprite_minimap_ = sprite_minimap;

  const Rectangle rect_render = RenderBounds();
  fp4_preview_scale_ = std::min(rect_render.Width / sprite_minimap_.vec_size.X,
                                rect_render.Height / sprite_minimap_.vec_size.Y);
  const int int4_w = static_cast<int>(fp4_preview_scale_ * sprite_minimap_.vec_size.X);
  const int int4_h = static_cast<int>(fp4_preview_scale_ * sprite_minimap_.vec_size.Y);
  const int int4_x = rect_render.X + (rect_render.Width - int4_w) / 2;
  const int int4_y = rect_render.Y + (rect_render.Height - int4_h) / 2;
  rect_map_ = Rectangle{int4_x, int4_y, int4_w, int4_h};
}

void MapPreviewWidget::Draw() {
  map::MapPreview* ptr_preview = fn_preview();
  if (ptr_preview == nullptr)
    return;

  const gfx::Sprite* ptr_minimap =
      reinterpret_cast<const gfx::Sprite*>(ptr_preview->GetMinimap());
  if (ptr_minimap == nullptr)
    return;

  UpdateMinimap(*ptr_minimap);

  DrawSprite(sprite_minimap_, int2{rect_map_.X, rect_map_.Y}.ToVector2(),
             int2{rect_map_.Width, rect_map_.Height});

  int4_tooltip_spawn_index = -1;
  if (b_show_spawn_points && !ptr_preview->HideSpawnPreviews()) {
    const std::vector<CPos>& vec_spawn_points = ptr_preview->SpawnPoints();
    const std::map<int, SpawnOccupant> map_occupants = fn_spawn_occupants();
    const std::vector<int> vec_disabled = fn_disabled_spawn_points();
    const std::optional<MapGridType> opt_grid_type = ptr_preview->GridType();
    const auto fn_index_of = [&vec_spawn_points](CPos p_find) {
      for (std::size_t sz_j = 0; sz_j < vec_spawn_points.size(); sz_j++)
        if (vec_spawn_points[sz_j] == p_find)
          return static_cast<int>(sz_j);
      return -1;
    };
    for (std::size_t sz_i = 0; sz_i < vec_spawn_points.size(); sz_i++) {
      const CPos p = vec_spawn_points[sz_i];

      const auto it_occupant = map_occupants.find(static_cast<int>(sz_i) + 1);
      const bool b_occupied = it_occupant != map_occupants.end();
      const SpawnOccupant* ptr_occupant = b_occupied ? &it_occupant->second : nullptr;
      const bool b_disabled = std::find(vec_disabled.begin(), vec_disabled.end(),
                                        static_cast<int>(sz_i) + 1) != vec_disabled.end();
      const int2 int2_pos = ConvertToPreview(p, opt_grid_type.value_or(MapGridType::Rectangular));

      const gfx::Sprite& sprite = b_disabled ? sprite_spawn_disabled_
                                : b_occupied ? sprite_spawn_claimed_
                                             : sprite_spawn_unclaimed_;
      const int2 int2_offset{static_cast<int>(sprite.vec_size.X) / 2,
                             static_cast<int>(sprite.vec_size.Y) / 2};

      const float fp4_dist_x =
          static_cast<float>(int2_pos.X - gfx::Viewport::int2_last_mouse_pos.X) /
          static_cast<float>(int2_offset.X);
      const float fp4_dist_y =
          static_cast<float>(int2_pos.Y - gfx::Viewport::int2_last_mouse_pos.Y) /
          static_cast<float>(int2_offset.Y);
      if (fp4_dist_x * fp4_dist_x + fp4_dist_y * fp4_dist_y <= 1.0f)
        int4_tooltip_spawn_index = fn_index_of(p) + 1;

      if (b_disabled) {
        DrawSprite(sprite_spawn_disabled_, (int2_pos - int2_offset).ToVector2());
        continue;
      }

      if (b_occupied)
        FillEllipseWithColor(
            Rectangle{int2_pos.X - int2_offset.X + 1, int2_pos.Y - int2_offset.Y + 1,
                      static_cast<int>(sprite.vec_size.X) - 2,
                      static_cast<int>(sprite.vec_size.Y) - 2},
            ptr_occupant->color_player);

      DrawSprite(sprite, (int2_pos - int2_offset).ToVector2());

      const std::string str_spawn_label =
          b_occupied && ptr_occupant->int4_team > 0
              ? std::to_string(ptr_occupant->int4_team)
              : std::string{1, static_cast<char>('A' + fn_index_of(p))};

      const int2 int2_text_offset =
          ptr_spawn_font_->Measure(str_spawn_label) / 2 + int2_spawn_label_offset;
      ptr_spawn_font_->DrawTextWithContrast(
          str_spawn_label, (int2_pos - int2_text_offset).ToVector2(), color_spawn_,
          color_spawn_contrast, 1);
    }
  }
}

bool MapPreviewWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "IgnoreMouseInput") {
    b_ignore_mouse_input = meta::GetBoolValue(str_key, str_value);
    return true;
  }
  if (str_key == "ShowSpawnPoints") {
    b_show_spawn_points = meta::GetBoolValue(str_key, str_value);
    return true;
  }
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
