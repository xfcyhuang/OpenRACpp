#include "mods/radar_widget.hpp"

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "mods/actor_exts.hpp"
#include "mods/world_interaction_controller.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

void WriteCellColor(std::span<std::byte> vec_data, int int4_stride,
                    MPos uv, std::uint32_t uint4_color,
                    bool b_isometric, int int4_column_offset) {
  auto* colors =
      reinterpret_cast<std::uint32_t*>(vec_data.data());
  if (b_isometric) {
    const int dx = uv.V & 1;
    if (uv.U + dx > 0)
      colors[uv.V * int4_stride + 2 * uv.U + dx - 1 + int4_column_offset] =
          uint4_color;

    if (2 * uv.U + dx < int4_stride)
      colors[uv.V * int4_stride + 2 * uv.U + dx + int4_column_offset] =
          uint4_color;
  } else {
    colors[uv.V * int4_stride + uv.U + int4_column_offset] = uint4_color;
  }
}

}

RadarWidget::RadarWidget(ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_md = std::get_if<game::ModData*>(ptr_value))
      ptr_mod_data_ = *ptr_md;
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world_arg;
  if (const ora::ui::WidgetArgValue* ptr_value =
          args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_wr_arg;

  if (ptr_world_ != nullptr) {
    ptr_radar_pings_ = ptr_world_->WorldActor()->TraitOrDefault<RadarPings>();
    for (auto* rtl :
         ptr_world_->WorldActor()->TraitsImplementing<sim::IRadarTerrainLayer>())
      vec_radar_terrain_layers_.push_back(rtl);
    b_is_rectangular_isometric_ =
        ptr_world_->Map().Grid().Type == MapGridType::RectangularIsometric;
    int4_cell_width_ = b_is_rectangular_isometric_ ? 2 : 1;
    int4_preview_width_ = ptr_world_->Map().MapSize().Width;
    int4_preview_height_ = ptr_world_->Map().MapSize().Height;
    if (b_is_rectangular_isometric_)
      int4_preview_width_ = 2 * int4_preview_width_ - 1;
  }

  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr) {
    str_world_select_cursor_ = ptr_metrics->Get("WorldSelectCursor");
    str_world_default_cursor_ = ptr_metrics->Get("WorldDefaultCursor");
  }

  game_settings_.mouse_control_style =
      ui::Ui::HasInstance()
          ? ui::Ui::Instance().GameSettingsInput().kind_mouse_control_style
          : MouseControlStyle::Modern;
}

RadarWidget::RadarWidget(const RadarWidget& widget_other)
    : ora::ui::Widget(widget_other),
      uint4_color_fog(widget_other.uint4_color_fog),
      uint4_color_shroud(widget_other.uint4_color_shroud),
      str_world_interaction_controller(
          widget_other.str_world_interaction_controller),
      int4_animation_length(widget_other.int4_animation_length),
      str_radar_online_sound(widget_other.str_radar_online_sound),
      str_radar_offline_sound(widget_other.str_radar_offline_sound),
      str_sound_up(widget_other.str_sound_up),
      str_sound_down(widget_other.str_sound_down),
      fn_is_enabled(widget_other.fn_is_enabled),
      fn_after_open(widget_other.fn_after_open),
      fn_after_close(widget_other.fn_after_close),
      fn_animating(widget_other.fn_animating),
      ptr_mod_data_(widget_other.ptr_mod_data_),
      ptr_world_(widget_other.ptr_world_),
      ptr_world_renderer_(widget_other.ptr_world_renderer_),
      ptr_radar_pings_(widget_other.ptr_radar_pings_),
      vec_radar_terrain_layers_(widget_other.vec_radar_terrain_layers_),
      b_is_rectangular_isometric_(widget_other.b_is_rectangular_isometric_),
      int4_cell_width_(widget_other.int4_cell_width_),
      int4_preview_width_(widget_other.int4_preview_width_),
      int4_preview_height_(widget_other.int4_preview_height_),
      str_world_select_cursor_(widget_other.str_world_select_cursor_),
      str_world_default_cursor_(widget_other.str_world_default_cursor_),
      game_settings_(widget_other.game_settings_) {}

RadarWidget::~RadarWidget() {
  if (ptr_radar_sheet_ != nullptr)
    ptr_radar_sheet_.reset();
}

bool RadarWidget::LoadFieldOrProperty(std::string_view str_key,
                                      std::string_view str_value) {
  if (str_key == "WorldInteractionController") {
    str_world_interaction_controller = std::string{str_value};
    return true;
  }
  if (str_key == "AnimationLength") {
    int4_animation_length = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "RadarOnlineSound") {
    str_radar_online_sound = std::string{str_value};
    return true;
  }
  if (str_key == "RadarOfflineSound") {
    str_radar_offline_sound = std::string{str_value};
    return true;
  }
  if (str_key == "SoundUp") {
    str_sound_up = std::string{str_value};
    return true;
  }
  if (str_key == "SoundDown") {
    str_sound_down = std::string{str_value};
    return true;
  }
  if (str_key == "ColorFog") {
    uint4_color_fog =
        meta::GetColorValue("ColorFog", str_value).ToArgb();
    return true;
  }
  if (str_key == "ColorShroud") {
    uint4_color_shroud =
        meta::GetColorValue("ColorShroud", str_value).ToArgb();
    return true;
  }
  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void RadarWidget::Initialize(ora::ui::WidgetArgs& args_args) {
  ora::ui::Widget::Initialize(args_args);

  ptr_radar_sheet_ = std::make_unique<gfx::Sheet>(
      gfx::SheetType::BGRA,
      int2{gfx::NextPowerOf2(2 * int4_preview_width_),
           gfx::NextPowerOf2(2 * int4_preview_height_)});
  vec_radar_data_ = ptr_radar_sheet_->GetData();

  auto* colors =
      reinterpret_cast<std::uint32_t*>(vec_radar_data_.data());
  std::fill(colors, colors + vec_radar_data_.size() / 4, uint4_color_shroud);

  MapBoundsChanged();

  sim::Player* ptr_player = nullptr;
  if (ptr_world_->Type() == sim::WorldType::Regular)
    ptr_player = ptr_world_->LocalPlayer() != nullptr
                     ? ptr_world_->LocalPlayer()
                     : ptr_world_->RenderPlayer();
  SetPlayer(ptr_player, true);

  if (ptr_player == nullptr)
    for (const MPos& uv : ptr_world_->Map().AllCells().MapCoords())
      UpdateTerrainColor(uv);

  ptr_world_->AddRenderPlayerChangedHandler(
      [this](sim::Player* p) { WorldOnRenderPlayerChanged(p); });
}

void RadarWidget::WorldOnRenderPlayerChanged(sim::Player* ptr_player) {
  SetPlayer(ptr_player);

  for (const MPos& uv : ptr_world_->Map().AllCells().MapCoords())
    UpdateTerrainColor(uv);
}

void RadarWidget::SetPlayer(sim::Player* ptr_player, bool b_force_update) {
  ptr_current_player_ = ptr_player;

  sim::Shroud* ptr_new_shroud =
      ptr_player != nullptr ? ptr_player->GetShroud() : nullptr;

  if (ptr_new_shroud != ptr_shroud_) {
    if (ptr_shroud_ != nullptr && n_shroud_token_.has_value())
      ptr_shroud_->RemoveShroudChangedCallback(*n_shroud_token_);

    if (ptr_new_shroud != nullptr) {
      n_shroud_token_ = ptr_new_shroud->SubscribeShroudChanged(
          [this](PPos puv) { UpdateShroudCell(puv); });
      for (const PPos puv : ptr_world_->Map().ProjectedCells())
        UpdateShroudCell(puv);
    } else {
      n_shroud_token_.reset();
    }

    ptr_shroud_ = ptr_new_shroud;
  }

  PlayerRadarTerrain* ptr_new_player_radar_terrain =
      ptr_current_player_ != nullptr
          ? ptr_current_player_->PlayerActor()
                ->TraitOrDefault<PlayerRadarTerrain>()
          : nullptr;

  if (b_force_update || ptr_new_player_radar_terrain != ptr_player_radar_) {
    if (ptr_player_radar_ != nullptr &&
        n_player_radar_terrain_token_.has_value()) {
      ptr_player_radar_->RemoveCellTerrainColorChangedCallback(
          *n_player_radar_terrain_token_);
      n_player_radar_terrain_token_.reset();
    } else if (n_map_tiles_token_.has_value()) {
      ptr_world_->Map().TilesRef().RemoveCellEntryChangedListener(
          *n_map_tiles_token_);
      n_map_tiles_token_.reset();
      for (std::size_t i = 0; i < vec_radar_terrain_layers_.size(); i++)
        vec_radar_terrain_layers_[i]->RemoveCellEntryChangedListener(
            vec_rtl_tokens_[i]);
      vec_rtl_tokens_.clear();
    }

    if (ptr_new_player_radar_terrain != nullptr) {
      n_player_radar_terrain_token_ =
          ptr_new_player_radar_terrain->SubscribeCellTerrainColorChanged(
              [this](MPos uv) { UpdateTerrainColor(uv); });
    } else {
      n_map_tiles_token_ =
          ptr_world_->Map().TilesRef().SubscribeCellEntryChanged(
              [this](CPos cell) {
                UpdateTerrainColor(
                    cell.ToMPos(ptr_world_->Map().Grid().Type));
              });
      for (auto* rtl : vec_radar_terrain_layers_)
        vec_rtl_tokens_.push_back(rtl->SubscribeCellEntryChanged(
            [this](CPos cell) {
              UpdateTerrainColor(
                  cell.ToMPos(ptr_world_->Map().Grid().Type));
            }));
    }

    ptr_player_radar_ = ptr_new_player_radar_terrain;
  }
}

void RadarWidget::MapBoundsChanged() {
  map::Map& map = ptr_world_->Map();

  const int int4_projected_left = map.Bounds().Left();
  const int int4_projected_right = map.Bounds().Right();
  const int int4_projected_top = map.Bounds().Top();
  const int int4_projected_bottom = map.Bounds().Bottom();
  int int4_top = std::numeric_limits<int>::max();
  int int4_bottom = std::numeric_limits<int>::min();
  const int int4_left = int4_projected_left * int4_cell_width_;
  const int int4_right = int4_projected_right * int4_cell_width_;

  for (int x = int4_projected_left; x < int4_projected_right; x++) {
    const std::vector<MPos> vec_all_top =
        map.Unproject(PPos{x, int4_projected_top});
    const std::vector<MPos> vec_all_bottom =
        map.Unproject(PPos{x, int4_projected_bottom});

    if (!vec_all_top.empty()) {
      const MPos* best = &vec_all_top.front();
      for (const MPos& uv : vec_all_top)
        if (uv.V < best->V)
          best = &uv;
      int4_top = std::min(int4_top, best->V);
    } else {
      int4_top = map.Bounds().Top();
    }

    if (!vec_all_bottom.empty()) {
      const MPos* best = &vec_all_bottom.front();
      for (const MPos& uv : vec_all_bottom)
        if (uv.V > best->V)
          best = &uv;
      int4_bottom = std::max(int4_bottom, best->V);
    } else {
      int4_bottom = map.Bounds().Bottom();
    }
  }

  const Rectangle rect_b = Rectangle::FromLTRB(int4_left, int4_top,
                                               int4_right, int4_bottom);
  const Rectangle rect_rb = RenderBounds();
  fp4_preview_scale_ =
      std::min(rect_rb.Width * 1.0f / rect_b.Width,
               rect_rb.Height * 1.0f / rect_b.Height);
  int2_preview_origin_ = int2{
      static_cast<int>((rect_rb.Width - fp4_preview_scale_ * rect_b.Width) / 2),
      static_cast<int>((rect_rb.Height -
                        fp4_preview_scale_ * rect_b.Height) /
                       2)};
  rect_map_ = Rectangle{
      int2_preview_origin_.X, int2_preview_origin_.Y,
      static_cast<int>(fp4_preview_scale_ * rect_b.Width),
      static_cast<int>(fp4_preview_scale_ * rect_b.Height)};

  sprite_terrain_ = gfx::Sprite(*ptr_radar_sheet_, rect_b,
                                gfx::TextureChannel::RGBA);
  sprite_shroud_ = gfx::Sprite(
      *ptr_radar_sheet_,
      Rectangle{rect_b.Left() + int4_preview_width_, rect_b.Top(),
                rect_b.Width, rect_b.Height},
      gfx::TextureChannel::RGBA);
  sprite_actor_ = gfx::Sprite(
      *ptr_radar_sheet_,
      Rectangle{rect_b.Left(), rect_b.Top() + int4_preview_height_,
                rect_b.Width, rect_b.Height},
      gfx::TextureChannel::RGBA);
}

void RadarWidget::UpdateTerrainColor(MPos uv) {
  const std::pair<std::uint32_t, std::uint32_t> pair_colors =
      ptr_player_radar_ != nullptr && ptr_player_radar_->IsInitialized()
          ? ptr_player_radar_->At(uv)
          : PlayerRadarTerrain::GetColor(
                ptr_world_->Map(),
                std::span<sim::IRadarTerrainLayer* const>(
                    vec_radar_terrain_layers_.data(),
                    vec_radar_terrain_layers_.size()),
                uv);

  const int int4_stride = ptr_radar_sheet_->Size().X;
  WriteCellColor(vec_radar_data_, int4_stride, uv, pair_colors.first,
                 b_is_rectangular_isometric_, 0);
}

void RadarWidget::UpdateShroudCell(PPos puv) {
  std::uint32_t uint4_color = 0;
  const sim::CellVisibility cv =
      ptr_current_player_->GetShroud()->GetVisibility(puv);
  if (!sim::HasCellVisibility(cv, sim::CellVisibility::Explored))
    uint4_color = uint4_color_shroud;
  else if (!sim::HasCellVisibility(cv, sim::CellVisibility::Visible))
    uint4_color = uint4_color_fog;

  const int int4_stride = ptr_radar_sheet_->Size().X;
  for (const MPos& iuv : ptr_world_->Map().Unproject(puv))
    WriteCellColor(vec_radar_data_, int4_stride, iuv, uint4_color,
                   b_is_rectangular_isometric_, int4_preview_width_);
}

const std::string* RadarWidget::GetCursor(int2 pt_pos) const {
  if (ptr_world_ == nullptr || !b_has_radar_)
    return nullptr;

  static thread_local std::string str_cursor_out;

  const core::Vector2 vec_world_pos = MinimapPixelToWorldCoords(pt_pos);
  const WPos wpos{static_cast<int>(vec_world_pos.X),
                  static_cast<int>(vec_world_pos.Y), 0};
  const CPos cell = ptr_world_->Map().CellContaining(wpos);

  const int2 int2_world_pixel = ptr_world_renderer_->ScreenPxPosition(wpos);
  const int2 int2_location =
      ptr_world_renderer_->Viewport()->WorldToViewPx(int2_world_pixel);
  MouseInput mi_input{};
  mi_input.Location = int2_location;
  mi_input.Button = ptr_world_->OrderGenerator()->ActionButton();
  mi_input.ModifierFlags = ui::Ui::Instance().GetModifierKeys();

  std::string str_cursor = ptr_world_->OrderGenerator()->GetCursor(
      *ptr_world_, cell, int2_world_pixel, mi_input);

  if (str_cursor.empty() ||
      (game_settings_.mouse_control_style != MouseControlStyle::Classic &&
       str_cursor == str_world_select_cursor_))
    str_cursor = str_world_default_cursor_;

  if (ui::Ui::Instance().CursorExists(str_cursor + "-minimap"))
    str_cursor = str_cursor + "-minimap";

  str_cursor_out = str_cursor;
  return &str_cursor_out;
}

bool RadarWidget::HandleMouseInput(const ora::MouseInput& mi_input) {
  if (!rect_map_.Contains(mi_input.Location))
    return false;

  if (!b_has_radar_)
    return true;

  const core::Vector2 vec_world_coords =
      MinimapPixelToWorldCoords(mi_input.Location);
  if ((mi_input.Event == MouseInputEvent::Down &&
       mi_input.Button !=
           ptr_world_->OrderGenerator()->ActionButton()) ||
      (mi_input.Event == MouseInputEvent::Move && b_is_minimap_moving_ &&
       mi_input.Button == kind_minimap_move_button_)) {
    ptr_world_renderer_->Viewport()->Center(
        WPos{static_cast<int>(vec_world_coords.X),
             static_cast<int>(vec_world_coords.Y), 0});
    b_is_minimap_moving_ = true;
    kind_minimap_move_button_ = mi_input.Button;
  } else if (mi_input.Event == MouseInputEvent::Down &&
             !str_world_interaction_controller.empty()) {
    const WPos wpos{static_cast<int>(vec_world_coords.X),
                    static_cast<int>(vec_world_coords.Y), 0};

    const int2 int2_location =
        ptr_world_renderer_->Viewport()->WorldToViewPx(
            ptr_world_renderer_->ScreenPxPosition(wpos));
    MouseInput fake_mi{};
    fake_mi.Event = MouseInputEvent::Down;
    fake_mi.Button = mi_input.Button;
    fake_mi.ModifierFlags = mi_input.ModifierFlags;
    fake_mi.Location = int2_location;

    auto* ptr_controller = ui::Ui::Instance().Root()->Get<WorldInteractionControllerWidget>(
        str_world_interaction_controller);
    ptr_controller->HandleMouseInput(fake_mi);
    fake_mi.Event = MouseInputEvent::Up;
    ptr_controller->HandleMouseInput(fake_mi);
  } else if (mi_input.Event == MouseInputEvent::Up &&
             mi_input.Button == kind_minimap_move_button_) {
    b_is_minimap_moving_ = false;
    kind_minimap_move_button_ = MouseButton::None;
  }

  return true;
}

void RadarWidget::Draw() {
  if (ptr_world_ == nullptr)
    return;

  ptr_radar_sheet_->CommitBufferedData();

  const core::Vector2 vec_o{
      static_cast<float>(rect_map_.Left()),
      static_cast<float>(rect_map_.Top()) +
          ptr_world_->Map().Bounds().Height * fp4_preview_scale_ *
              (1 - fp4_radar_minimap_height_) / 2};
  const core::Vector2 vec_s{
      static_cast<float>(rect_map_.Width),
      static_cast<float>(rect_map_.Height) * fp4_radar_minimap_height_};

  ui::DrawSprite(sprite_terrain_, vec_o, vec_s);
  ui::DrawSprite(sprite_actor_, vec_o, vec_s);

  if (ptr_shroud_ != nullptr)
    ui::DrawSprite(sprite_shroud_, vec_o, vec_s);

  if (b_has_radar_) {
    const int2 int2_tl = CellToMinimapPixel(
        ptr_world_->Map().CellContaining(ptr_world_renderer_->ProjectedPosition(
            ptr_world_renderer_->Viewport()->TopLeft())));
    const int2 int2_br = CellToMinimapPixel(
        ptr_world_->Map().CellContaining(ptr_world_renderer_->ProjectedPosition(
            ptr_world_renderer_->Viewport()->BottomRight())));

    if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps()) {
      ptr_renderer->EnableScissor(rect_map_);
      DrawRadarPings();
      ptr_renderer->UIRgbaColorRenderer().DrawRect(
          core::Vector3{static_cast<float>(int2_tl.X),
                        static_cast<float>(int2_tl.Y), 0},
          core::Vector3{static_cast<float>(int2_br.X),
                        static_cast<float>(int2_br.Y), 0},
          1, core::Color::FromArgb(255, 255, 255, 255));
      ptr_renderer->DisableScissor();
    }
  }
}

void RadarWidget::DrawRadarPings() {
  if (ptr_radar_pings_ == nullptr)
    return;

  if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps()) {
    for (const auto& radar_ping : ptr_radar_pings_->Pings()) {
      if (radar_ping->fn_is_visible && !radar_ping->fn_is_visible())
        continue;

      const CPos ping_cell =
          ptr_world_->Map().CellContaining(radar_ping->wpos_position);
      const std::array<core::Vector2, 3> vec_points =
          radar_ping->Points(CellToMinimapPixel(ping_cell));
      ptr_renderer->UIRgbaColorRenderer().DrawPolygon(
          std::span<const core::Vector2>(vec_points.data(), vec_points.size()),
          2, radar_ping->color_color);
    }
  }
}

void RadarWidget::Tick() {
  const bool b_enabled = fn_is_enabled();
  b_cached_enabled_ = b_enabled;

  if (b_enabled) {
    const int int4_stride = ptr_radar_sheet_->Size().X;
    const int int4_offset_bytes =
        4 * sprite_actor_.Bounds.Top() * int4_stride;
    const int int4_length_bytes =
        4 * sprite_actor_.Bounds.Height * int4_stride;
    std::fill(vec_radar_data_.data() + int4_offset_bytes,
              vec_radar_data_.data() + int4_offset_bytes + int4_length_bytes,
              std::byte{0});

    auto* colors = reinterpret_cast<std::uint32_t*>(vec_radar_data_.data());
    for (const auto& [actor, trait] :
         ptr_world_->ActorsWithTrait<sim::IRadarSignature>()) {
      if (!actor->IsInWorld() || ptr_world_->FogObscures(*actor))
        continue;

      std::vector<std::pair<CPos, core::Color>> vec_signature_cells;
      trait->PopulateRadarSignatureCells(*actor, vec_signature_cells);

      for (const auto& [cell, color] : vec_signature_cells) {
        if (!ptr_world_->Map().Contains(cell))
          continue;

        const MPos uv = cell.ToMPos(ptr_world_->Map().Grid().Type);
        const std::uint32_t uint4_color = color.ToArgb();

        const int int4_row_offset =
            (uv.V + int4_preview_height_) * int4_stride;
        if (b_is_rectangular_isometric_) {
          const int dx = uv.V & 1;
          const int u2 = 2 * uv.U;

          if (uv.U + dx > 0)
            colors[int4_row_offset + u2 + dx - 1] = uint4_color;

          if (u2 + dx < int4_stride)
            colors[int4_row_offset + u2 + dx] = uint4_color;
        } else {
          colors[int4_row_offset + uv.U] = uint4_color;
        }
      }
    }
  }

  const int int4_target_frame = b_enabled ? int4_animation_length : 0;
  b_has_radar_ = b_enabled && int4_frame_ == int4_animation_length;
  if (int4_frame_ == int4_target_frame)
    return;

  int4_frame_ += b_enabled ? 1 : -1;

  const float fp4_animation_progress =
      static_cast<float>(int4_frame_) / int4_animation_length;
  fp4_radar_minimap_height_ = fp4_animation_progress;
  fn_animating(fp4_animation_progress);

  const int2 int2_ro = RenderOrigin();
  rect_map_ = Rectangle{int2_preview_origin_.X + int2_ro.X,
                        int2_preview_origin_.Y + int2_ro.Y,
                        rect_map_.Width, rect_map_.Height};

  if (int4_frame_ == int4_target_frame) {
    if (b_enabled)
      fn_after_open();
    else
      fn_after_close();
  }
}

int2 RadarWidget::CellToMinimapPixel(CPos p) const {
  const MPos uv = p.ToMPos(ptr_world_->Map().Grid().Type);
  int dx = static_cast<int>(fp4_preview_scale_ * int4_cell_width_ *
                            (uv.U - ptr_world_->Map().Bounds().Left()));
  const int dy = static_cast<int>(
      fp4_preview_scale_ * (uv.V - ptr_world_->Map().Bounds().Top()));

  if (b_is_rectangular_isometric_ && (uv.V & 1) == 1)
    dx++;

  return int2{rect_map_.X + dx, rect_map_.Y + dy};
}

core::Vector2 RadarWidget::MinimapPixelToWorldCoords(int2 pixel) const {
  const float u = (pixel.X - rect_map_.X) /
                      (fp4_preview_scale_ * int4_cell_width_) +
                  ptr_world_->Map().Bounds().Left();
  const float v = (pixel.Y - rect_map_.Y) / fp4_preview_scale_ +
                  ptr_world_->Map().Bounds().Top();

  if (ptr_world_->Map().Grid().Type == MapGridType::Rectangular)
    return core::Vector2{1024 * u + 512, 1024 * v + 512};

  const float y = v / 2.0f - u;
  const float x = v - y;
  return core::Vector2{724 * (x - y), 724 * (x + y)};
}

void RadarWidget::Removed() {
  ora::ui::Widget::Removed();

  if (ptr_player_radar_ != nullptr &&
      n_player_radar_terrain_token_.has_value())
    ptr_player_radar_->RemoveCellTerrainColorChangedCallback(
        *n_player_radar_terrain_token_);

  ptr_radar_sheet_.reset();
}

void RegisterRadarWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "RadarWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<RadarWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}
