#pragma once
import std;

#include "gfx/sheet.hpp"
#include "gfx/sprite.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {

struct PlayerBadge {
  std::string str_label;
  std::optional<std::string> opt_icon;
  std::optional<std::string> opt_icon2x;
  std::optional<std::string> opt_icon3x;
};

class PlayerDatabase {
 public:
  static PlayerDatabase& Instance();

  static constexpr int kIconSize = 24;

  std::string str_forum = "https://forum.openra.net/";
  std::string str_profile = "https://forum.openra.net/openra/info/";
  std::string str_link = "https://forum.openra.net/openra/link";

  std::optional<PlayerBadge> LoadBadge(const yaml::MiniYaml& yaml_badge);

  gfx::Sprite GetIcon(const PlayerBadge& badge);

 private:
  PlayerDatabase() = default;

  gfx::Sprite LoadSprite(const std::string& str_url, int int4_density);
  static std::unique_ptr<gfx::Sheet> CreateSheet();

  std::unique_ptr<gfx::SheetBuilder> up_sheet_builder_;
  std::map<std::tuple<std::string, std::string, std::string, std::string, int>,
           gfx::Sprite>
      map_icon_cache_;
};

}  // namespace ora::ui
