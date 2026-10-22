// UPSTREAM: OpenRA.Mods.Common/AssetBrowser.cs @e180e3225 L16-22(IGlobal
//          ModData 的四扩展名表;mod.yaml "AssetBrowser" 节点装载)+
//          OpenRA.Mods.Common/Widgets/Logic/AssetBrowserLogic.cs L26-770
//          全文(源包下拉/五类型过滤/帧滑条与播放三键/sprite-voxel-
//          audio-video 四装载路径/调色板下拉/静音对)。
//          AssetBrowser.cs L16-22 (the IGlobalModData four-extension table;
//          the mod.yaml "AssetBrowser" node load) + the whole of
//          AssetBrowserLogic.cs L26-770 (the source-package dropdown / the
//          five type filters / the frame slider and the play trio / the
//          sprite-voxel-audio-video load paths / the palette dropdown / the
//          mute pair).
#pragma once
import std;

#include "core/wrot.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_loader.hpp"
#include "sound/sound_loader.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::fs {
class IReadOnlyPackage;
}
namespace ora::platform {
class ISound;
}
namespace ora::ui {
class DropDownButtonWidget;
class ScrollItemWidget;
class ScrollPanelWidget;
class SliderWidget;
class TextFieldWidget;
}

namespace ora::mods {

/// AssetBrowser.cs L16-22(Manifest.GlobalModData["AssetBrowser"] 的装载
/// 产物)
/// AssetBrowser.cs L16-22 (the loaded product of
/// Manifest.GlobalModData["AssetBrowser"]).
struct AssetBrowserData {
  std::vector<std::string> vec_sprite_extensions;
  std::vector<std::string> vec_model_extensions;
  std::vector<std::string> vec_audio_extensions;
  std::vector<std::string> vec_video_extensions;

  static AssetBrowserData FromManifest(const game::ModData& mod_data);
};

class AssetBrowserLogic final : public ora::ui::ChromeLogic {
 public:
  AssetBrowserLogic(ora::ui::Widget& widget, std::function<void()> fn_on_exit,
                    game::ModData& mod_data, gfx::WorldRenderer* ptr_world_renderer);
  ~AssetBrowserLogic() override;

  bool LoadAssetForTest(fs::IReadOnlyPackage* ptr_package,
                        const std::string& str_filename) {
    return LoadAsset(ptr_package, str_filename);
  }
  void ApplyFilterForTest() { ApplyFilter(); }
  void PopulateAssetListForTest() { PopulateAssetList(); }
  void SetFrameForTest(int int4_frame) { int4_current_frame_ = int4_frame; }
  void NextFrameForTest() { SelectNextFrame(); }
  void PrevFrameForTest() { SelectPreviousFrame(); }
  bool AnimateFramesForTest() const { return b_animate_frames_; }
  void SetAnimateFramesForTest(bool b_value) { b_animate_frames_ = b_value; }
  bool IsLoadErrorForTest() const { return b_is_load_error_; }
  bool IsVideoLoadedForTest() const { return b_is_video_loaded_; }
  int CurrentFrameForTest() const { return int4_current_frame_; }
  std::size_t CurrentSpriteCountForTest() const {
    return vec_current_sprites_.size();
  }
  const std::string& CurrentFilenameForTest() const {
    return str_current_filename_;
  }
  const AssetBrowserData& ExtensionsForTest() const { return data_browser_; }
  std::string SourceDisplayNameForTest(
      const fs::IReadOnlyPackage* ptr_package) const {
    return GetSourceDisplayName(ptr_package);
  }
  bool FilterAssetForTest(const std::string& str_filename) const {
    return FilterAsset(str_filename);
  }
  const std::vector<std::string>& AssetFilePathsForTest() const {
    return vec_asset_file_paths_for_test_;
  }

 private:
  void SelectNextFrame();
  void SelectPreviousFrame();
  bool FilterAsset(const std::string& str_filename) const;
  void ApplyFilter();
  void AddAsset(ora::ui::ScrollPanelWidget& widget_list,
                const std::string& str_filepath,
                fs::IReadOnlyPackage* ptr_package);
  bool LoadAsset(fs::IReadOnlyPackage* ptr_package,
                 const std::string& str_filename);
  bool ShowSourceDropdown(ora::ui::DropDownButtonWidget& widget_dropdown);
  void PopulateAssetList();
  bool ShowPaletteDropdown(ora::ui::DropDownButtonWidget& widget_dropdown);
  std::string GetSourceDisplayName(const fs::IReadOnlyPackage* ptr_package) const;
  void MuteSounds();
  void UnMuteSounds();
  void ClearLoadedAssets();
  ora::ui::Widget* CreateAssetTypesPanel();

  game::ModData* ptr_mod_data_ = nullptr;
  ora::ui::Widget* ptr_panel_ = nullptr;

  ora::ui::TextFieldWidget* ptr_filename_input_ = nullptr;
  ora::ui::SliderWidget* ptr_frame_slider_ = nullptr;
  ora::ui::ScrollPanelWidget* ptr_asset_list_ = nullptr;
  std::unique_ptr<ora::ui::ScrollItemWidget> up_template_;

  std::unique_ptr<gfx::SheetBuilder> ptr_sheet_builder_indexed_;
  std::unique_ptr<gfx::SheetBuilder> ptr_sheet_builder_bgra_;
  std::vector<std::pair<std::string, std::vector<gfx::Sprite>>>
      vec_sprite_cache_;
  std::vector<gfx::SpriteLoaderFn> vec_sprite_loaders_;

  AssetBrowserData data_browser_;
  std::vector<std::string> vec_allowed_extensions_;
  std::vector<fs::IReadOnlyPackage*> vec_acceptable_packages_;
  std::vector<std::string> vec_palettes_;
  std::vector<std::string> vec_color_picker_palettes_;

  fs::IReadOnlyPackage* ptr_asset_source_ = nullptr;
  bool b_animate_frames_ = false;

  std::string str_current_palette_;
  std::string str_current_filename_;
  fs::IReadOnlyPackage* ptr_current_package_ = nullptr;
  std::vector<gfx::Sprite> vec_current_sprites_;
  platform::ISound* ptr_current_sound_ = nullptr;
  std::optional<sound::SoundFormatInfo> opt_current_sound_format_;
  std::vector<std::byte> vec_current_pcm_;
  bool b_is_video_loaded_ = false;
  bool b_is_load_error_ = false;
  int int4_current_frame_ = 0;
  WRot rot_model_orientation_{WAngle{0}, WAngle{0}, WAngle{0}};
  float fp4_sprite_scale_ = 1.0f;
  float fp4_model_scale_ = 1.0f;
  int int4_asset_types_to_display_ = 1 | 2 | 4 | 8;

  std::string str_all_packages_;
  float fp4_cached_music_volume_ = 0.0f;
  bool b_music_muted_ = false;
  std::vector<std::pair<std::string, bool>> vec_asset_visibility_;
  std::vector<std::string> vec_asset_file_paths_for_test_;
};

void RegisterAssetBrowserLogic();

}  // namespace ora::mods
