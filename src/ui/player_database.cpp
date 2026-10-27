import std;

#include "ui/player_database.hpp"

#include "formats/png.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "net/http_sync.hpp"
#include "ui/ui.hpp"

namespace ora::ui {

namespace {

std::tuple<std::string, std::string, std::string, std::string, int> BadgeCacheKey(
    const PlayerBadge& badge, int int4_density) {
  return {badge.str_label, badge.opt_icon.value_or(""), badge.opt_icon2x.value_or(""),
          badge.opt_icon3x.value_or(""), int4_density};
}

}

PlayerDatabase& PlayerDatabase::Instance() {
  static PlayerDatabase instance_player_database;
  return instance_player_database;
}

std::unique_ptr<gfx::Sheet> PlayerDatabase::CreateSheet() {
  auto up_sheet = std::make_unique<gfx::Sheet>(gfx::SheetType::BGRA, int2{512, 512});

  up_sheet->CreateBuffer();
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  if (ptr_renderer != nullptr)
    up_sheet->GetTexture().SetScaleFilter(gfx::TextureScaleFilter::Linear);

  return up_sheet;
}

gfx::Sprite PlayerDatabase::LoadSprite(const std::string& str_url, int int4_density) {
  const int int4_sprite_size = kIconSize * int4_density;
  gfx::Sprite sprite_allocated =
      up_sheet_builder_->Allocate(int2{int4_sprite_size, int4_sprite_size},
                                  1.0f / static_cast<float>(int4_density));

  std::thread([str_url, int4_sprite_size,
               sprite_allocated]() mutable {
    const net::HttpResult result_http = net::HttpGetSync(str_url);
    if (!result_http.b_ok)
      return;

    const std::span<const std::byte> vec_body{
        reinterpret_cast<const std::byte*>(result_http.str_body.data()),
        result_http.str_body.size()};
    std::optional<fmt::Png> opt_icon;
    try {
      opt_icon.emplace(vec_body);
    } catch (...) {
      return;
    }

    if (opt_icon->Width() == int4_sprite_size && opt_icon->Height() == int4_sprite_size)
      Ui::Instance().RunAfterTickDefer([sprite_allocated, png_icon = std::move(*opt_icon)]() mutable {
        gfx::FastCopyIntoSprite(sprite_allocated, png_icon);
        if (sprite_allocated.ptr_sheet != nullptr)
          sprite_allocated.ptr_sheet->CommitBufferedData(sprite_allocated.Bounds);
      });
  }).detach();

  return sprite_allocated;
}

std::optional<PlayerBadge> PlayerDatabase::LoadBadge(const yaml::MiniYaml& yaml_badge) {
  if (up_sheet_builder_ == nullptr) {
    up_sheet_builder_ = std::make_unique<gfx::SheetBuilder>(
        gfx::SheetType::BGRA, &PlayerDatabase::CreateSheet);

    map_icon_cache_.clear();
  }

  const yaml::MiniYamlNode* ptr_label_node = yaml_badge.NodeWithKeyOrDefault("Label");
  const yaml::MiniYamlNode* ptr_icon24_node = yaml_badge.NodeWithKeyOrDefault("Icon24");
  const yaml::MiniYamlNode* ptr_icon48_node = yaml_badge.NodeWithKeyOrDefault("Icon48");
  const yaml::MiniYamlNode* ptr_icon72_node = yaml_badge.NodeWithKeyOrDefault("Icon72");
  if (ptr_label_node == nullptr || ptr_label_node->Value.Value == nullptr)
    return std::nullopt;

  PlayerBadge badge_new;
  badge_new.str_label = *ptr_label_node->Value.Value;
  if (ptr_icon24_node != nullptr && ptr_icon24_node->Value.Value != nullptr)
    badge_new.opt_icon = *ptr_icon24_node->Value.Value;
  if (ptr_icon48_node != nullptr && ptr_icon48_node->Value.Value != nullptr)
    badge_new.opt_icon2x = *ptr_icon48_node->Value.Value;
  if (ptr_icon72_node != nullptr && ptr_icon72_node->Value.Value != nullptr)
    badge_new.opt_icon3x = *ptr_icon72_node->Value.Value;
  return badge_new;
}

gfx::Sprite PlayerDatabase::GetIcon(const PlayerBadge& badge) {
  if (up_sheet_builder_ == nullptr) {
    up_sheet_builder_ = std::make_unique<gfx::SheetBuilder>(
        gfx::SheetType::BGRA, &PlayerDatabase::CreateSheet);
    map_icon_cache_.clear();
  }

  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  const float fp4_window_scale =
      ptr_renderer != nullptr ? ptr_renderer->EffectiveWindowScale() : 1.0f;
  const int int4_density = fp4_window_scale > 2 ? 3 : fp4_window_scale > 1 ? 2 : 1;

  const auto key_cache = BadgeCacheKey(badge, int4_density);
  const auto it_found = map_icon_cache_.find(key_cache);
  if (it_found != map_icon_cache_.end())
    return it_found->second;

  gfx::Sprite sprite_icon{};
  if (int4_density > 2 && badge.opt_icon3x.has_value() && !badge.opt_icon3x->empty())
    sprite_icon = LoadSprite(*badge.opt_icon3x, 3);
  else if (int4_density > 1 && badge.opt_icon2x.has_value() && !badge.opt_icon2x->empty())
    sprite_icon = LoadSprite(*badge.opt_icon2x, 2);
  else
    sprite_icon = LoadSprite(badge.opt_icon.value_or(""), 1);

  map_icon_cache_.emplace(key_cache, sprite_icon);
  return sprite_icon;
}

}
