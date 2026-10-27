#pragma once
import std;

#include "core/rectangle.hpp"
#include "fs/file_system.hpp"
#include "gfx/sheet.hpp"
#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

enum class PanelSides : std::int32_t {
  Left = 1,
  Top = 2,
  Right = 4,
  Bottom = 8,
  Center = 16,

  Edges = Left | Top | Right | Bottom,
  All = Edges | Center,
};

constexpr bool HasSide(PanelSides kind_self, PanelSides kind_m) {
  return (static_cast<std::int32_t>(kind_self) & static_cast<std::int32_t>(kind_m)) ==
         static_cast<std::int32_t>(kind_m);
}

constexpr PanelSides operator|(PanelSides kind_a, PanelSides kind_b) {
  return static_cast<PanelSides>(static_cast<std::int32_t>(kind_a) |
                                 static_cast<std::int32_t>(kind_b));
}

class ChromeProvider {
 public:

  class Collection final : public meta::RecordObject {
   public:
    std::string str_image;
    std::string str_image2x;
    std::string str_image3x;
    std::vector<std::int32_t> vec_panel_region;
    std::int32_t int4_panel_sides = static_cast<std::int32_t>(PanelSides::All);
    std::vector<std::pair<std::string, Rectangle>> vec_regions;

    const meta::RecordDesc& record_desc() const override;

    static constexpr std::string_view kTypeName = "OpenRA.Game.Graphics.ChromeProvider.Collection";
  };

  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    const std::vector<std::string>* vec_chrome_files = nullptr;
    float fp4_dpi_scale = 1.0f;
    RenderThread* ptr_render = nullptr;
  };

  void Initialize(Deps deps);

  void Deinitialize();

  const std::vector<std::pair<std::string, std::unique_ptr<Collection>>>& Collections() const {
    return vec_collections_;
  }

  Sprite GetImage(const std::string& str_collection_name, const std::string& str_image_name);

  Sprite TryGetImage(const std::string& str_collection_name, const std::string& str_image_name);

  std::vector<std::optional<Sprite>> GetPanelImages(const std::string& str_collection_name);

  std::vector<std::optional<Sprite>> TryGetPanelImages(const std::string& str_collection_name);

  int2 GetMinimumPanelSize(const std::string& str_collection_name);

  void SetDPIScale(float fp4_scale);

 private:
  struct SheetDensity {
    Sheet* ptr_sheet = nullptr;
    std::int32_t int4_density = 1;
  };

  SheetDensity SheetForCollection(Collection& collection_c);
  Collection* FindCollection(const std::string& str_name);

  fs::FileSystem* ptr_file_system_ = nullptr;
  RenderThread* ptr_render_ = nullptr;
  float fp4_dpi_scale_ = 1.0f;

  std::vector<std::pair<std::string, std::unique_ptr<Collection>>> vec_collections_;
  std::vector<std::pair<std::string, std::unique_ptr<Sheet>>> vec_cached_sheets_;
  std::vector<std::tuple<std::string, std::string, Sprite>> vec_cached_sprites_;
  std::vector<std::pair<std::string, std::vector<std::optional<Sprite>>>> vec_cached_panel_sprites_;
  std::vector<std::pair<Collection*, SheetDensity>> vec_cached_collection_sheets_;

  yaml::StringPool pool_yaml_;
};

}
