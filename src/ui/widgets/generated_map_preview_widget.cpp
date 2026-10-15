// UPSTREAM: OpenRA.Mods.Common/Widgets/GeneratedMapPreviewWidget.cs @e180e3225
// L24-168(全文;自有 Sheet 的预览位图 + 出生点标签对)。
import std;

#include "ui/widgets/generated_map_preview_widget.hpp"

#include "gfx/chrome_provider.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/sprite_font.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "meta/field_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

GeneratedMapPreviewWidget::GeneratedMapPreviewWidget() {
  ChromeMetrics& metrics_chrome = RequireChromeMetrics();
  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();

  sprite_spawn_unclaimed_ = ptr_chrome->GetImage("lobby-bits", "spawn-unclaimed");
  ptr_spawn_font_ = &RequireWidgetFont(metrics_chrome.Get("SpawnFont"));
  color_spawn_ = metrics_chrome.GetColor("SpawnColor");
  color_spawn_contrast = metrics_chrome.GetColor("SpawnContrastColor");
  int2_spawn_label_offset = metrics_chrome.GetInt2("SpawnLabelOffset");
}

GeneratedMapPreviewWidget::GeneratedMapPreviewWidget(
    const GeneratedMapPreviewWidget& widget_other)
    : Widget(widget_other),
      b_show_spawn_points(widget_other.b_show_spawn_points),
      sprite_spawn_unclaimed_(widget_other.sprite_spawn_unclaimed_),
      ptr_spawn_font_(widget_other.ptr_spawn_font_),
      color_spawn_(widget_other.color_spawn_),
      color_spawn_contrast(widget_other.color_spawn_contrast),
      int2_spawn_label_offset(widget_other.int2_spawn_label_offset) {}

GeneratedMapPreviewWidget::~GeneratedMapPreviewWidget() {
  up_map_sheet_.reset();
}

void GeneratedMapPreviewWidget::Update(map::MapPreview& map_preview) {
  std::vector<CPos> vec_spawn_points;
  if (!map_preview.HideSpawnPreviews())
    vec_spawn_points = map_preview.SpawnPoints();

  const fmt::Png* ptr_preview = map_preview.Preview();
  if (ptr_preview != nullptr)
    Update(std::move(vec_spawn_points), map_preview.Bounds(),
           map_preview.GridType().value_or(MapGridType::Rectangular), *ptr_preview);
}

void GeneratedMapPreviewWidget::Update(map::Map& map_world, bool b_force_preview) {
  std::vector<CPos> vec_spawn_points;
  if (b_force_preview || !map_world.HideSpawnPreviews()) {
    for (const yaml::MiniYamlNode& node_actor : map_world.ActorDefinitions()) {
      const std::string_view sv_value =
          node_actor.Value.Value != nullptr ? std::string_view{*node_actor.Value.Value}
                                            : std::string_view{};
      if (sv_value != "mpspawn")
        continue;

      const yaml::MiniYamlNode* node_location =
          node_actor.Value.NodeWithKeyOrDefault("Location");
      if (node_location != nullptr && node_location->Value.Value != nullptr) {
        const int2 uv =
            meta::GetSizeValue("Location", *node_location->Value.Value);
        vec_spawn_points.emplace_back(uv.X, uv.Y);
      }
    }
  }

  const std::optional<std::vector<char>> opt_stream =
      map_world.Package() != nullptr
          ? map_world.Package()->GetStream("map.png")
          : std::nullopt;

  if (b_force_preview) {
    const std::vector<std::byte> vec_bytes = map_world.SavePreview();
    const fmt::Png png_preview{std::span<const std::byte>{vec_bytes.data(),
                                                          vec_bytes.size()}};
    Update(std::move(vec_spawn_points), map_world.Bounds(), map_world.Grid().Type,
           png_preview);
    return;
  }

  if (!opt_stream.has_value()) {
    Update(std::move(vec_spawn_points), map_world.Bounds(), map_world.Grid().Type,
           fmt::Png{std::span<const std::byte>{}});
    return;
  }

  const std::span<const std::byte> sv_png{
      reinterpret_cast<const std::byte*>(opt_stream->data()), opt_stream->size()};
  const fmt::Png png_preview{sv_png};
  Update(std::move(vec_spawn_points), map_world.Bounds(), map_world.Grid().Type,
         png_preview);
}

void GeneratedMapPreviewWidget::Update(std::vector<CPos> vec_spawn_points,
                                       Rectangle rect_bounds, MapGridType grid_type,
                                       const fmt::Png& png_preview) {
  const int2 int2_preview_size{png_preview.Width(), png_preview.Height()};
  if (up_map_sheet_ == nullptr || up_map_sheet_->Size().X < int2_preview_size.X ||
      up_map_sheet_->Size().Y < int2_preview_size.Y) {
    up_map_sheet_.reset();
    up_map_sheet_ = std::make_unique<gfx::Sheet>(
        gfx::SheetType::BGRA,
        int2{gfx::NextPowerOf2(int2_preview_size.X),
             gfx::NextPowerOf2(int2_preview_size.Y)});
  }

  const Rectangle rect_sprite{0, 0, png_preview.Width(), png_preview.Height()};
  sprite_map_ = gfx::Sprite{*up_map_sheet_, rect_sprite, gfx::TextureChannel::RGBA};
  gfx::FastCopyIntoSprite(sprite_map_, png_preview);
  up_map_sheet_->CommitBufferedData(sprite_map_.Bounds);

  const Rectangle rect_render = RenderBounds();
  const float fp4_preview_scale =
      std::min(rect_render.Width * 1.0f / rect_sprite.Width,
               rect_render.Height * 1.0f / rect_sprite.Height);
  const int int4_w = static_cast<int>(fp4_preview_scale * rect_sprite.Width);
  const int int4_h = static_cast<int>(fp4_preview_scale * rect_sprite.Height);
  const int int4_x = rect_render.X + (rect_render.Width - int4_w) / 2;
  const int int4_y = rect_render.Y + (rect_render.Height - int4_h) / 2;
  rect_map_ = Rectangle{int4_x, int4_y, int4_w, int4_h};

  if (b_show_spawn_points) {
    std::vector<std::tuple<int2, std::string, int2>> vec_labels;
    for (const CPos p : vec_spawn_points) {
      const int2 int2_pos =
          ConvertToPreview(p, rect_bounds, grid_type, fp4_preview_scale);

      const gfx::Sprite& sprite = sprite_spawn_unclaimed_;
      const int2 int2_offset{static_cast<int>(sprite.vec_size.X) / 2,
                             static_cast<int>(sprite.vec_size.Y) / 2};
      DrawSprite(sprite, (int2_pos - int2_offset).ToVector2());

      const std::string str_number{1,
                                   static_cast<char>('A' + vec_labels.size())};
      const int2 int2_text_offset =
          ptr_spawn_font_->Measure(str_number) / 2 + int2_spawn_label_offset;
      vec_labels.emplace_back(int2_pos, str_number, int2_text_offset);
    }

    vec_spawns_ = std::move(vec_labels);
  }
}

int2 GeneratedMapPreviewWidget::ConvertToPreview(CPos cell, Rectangle rect_bounds,
                                                 MapGridType grid_type,
                                                 float fp4_preview_scale) {
  const MPos point = cell.ToMPos(grid_type);
  const int int4_cell_width = grid_type == MapGridType::RectangularIsometric ? 2 : 1;
  const int int4_dx = static_cast<int>(fp4_preview_scale * int4_cell_width *
                                       (point.U - rect_bounds.Left()));
  const int int4_dy =
      static_cast<int>(fp4_preview_scale * (point.V - rect_bounds.Top()));

  if ((point.V & 1) == 1)
    return int2{rect_map_.X + int4_dx + 1, rect_map_.Y + int4_dy};

  return int2{rect_map_.X + int4_dx, rect_map_.Y + int4_dy};
}

void GeneratedMapPreviewWidget::Draw() {
  if (sprite_map_.ptr_sheet == nullptr)
    return;

  DrawSprite(sprite_map_, int2{rect_map_.X, rect_map_.Y}.ToVector2(),
             int2{rect_map_.Width, rect_map_.Height});
  const int2 int2_offset{static_cast<int>(sprite_spawn_unclaimed_.vec_size.X) / 2,
                         static_cast<int>(sprite_spawn_unclaimed_.vec_size.Y) / 2};
  for (const auto& [int2_pos, str_label, int2_label_offset] : vec_spawns_) {
    DrawSprite(sprite_spawn_unclaimed_, (int2_pos - int2_offset).ToVector2());
    ptr_spawn_font_->DrawTextWithContrast(str_label,
                                          (int2_pos - int2_label_offset).ToVector2(),
                                          color_spawn_, color_spawn_contrast, 1);
  }
}

void GeneratedMapPreviewWidget::Removed() {
  Widget::Removed();
  up_map_sheet_.reset();
}

bool GeneratedMapPreviewWidget::LoadFieldOrProperty(std::string_view str_key,
                                                    std::string_view str_value) {
  if (str_key == "ShowSpawnPoints") {
    b_show_spawn_points = meta::GetBoolValue(str_key, str_value);
    return true;
  }

  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}  // namespace ora::ui
