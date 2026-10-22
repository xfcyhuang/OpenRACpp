// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/AssetBrowserLogic.cs
//          @e180e3225(实现;头注见 asset_browser_logic.hpp)
//          Implementation; the header notes live in asset_browser_logic.hpp.
#include "mods/asset_browser_logic.hpp"

#include "fluent/fluent_provider.hpp"
#include "formats/video.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "meta/parse.hpp"
#include "mods/video_player_widget.hpp"
#include "sound/sound.hpp"
#include "sound/sound_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/rgba_sprite_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::mods {
namespace {

constexpr int kAssetTypeSprite = 1;
constexpr int kAssetTypeModel = 2;
constexpr int kAssetTypeAudio = 4;
constexpr int kAssetTypeVideo = 8;
constexpr int kAssetTypeUnknown = 16;

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string LowerInvariant(std::string_view sv_input) {
  std::string str_out{sv_input};
  for (char& c : str_out)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return str_out;
}

std::string ExtensionOf(std::string_view sv_path) {
  const std::size_t sz_dot = sv_path.rfind('.');
  if (sz_dot == std::string_view::npos)
    return {};
  return LowerInvariant(sv_path.substr(sz_dot));
}

std::vector<std::string> SplitCommaList(const yaml::MiniYamlNode& node_item) {
  std::vector<std::string> vec_values;
  if (node_item.Value.Value == nullptr)
    return vec_values;
  for (const std::string_view part :
       meta::SplitComma(*node_item.Value.Value)) {
    std::string str_part{part};
    while (!str_part.empty() &&
           (str_part.front() == ' ' || str_part.front() == '\t'))
      str_part.erase(str_part.begin());
    while (!str_part.empty() &&
           (str_part.back() == ' ' || str_part.back() == '\t'))
      str_part.pop_back();
    if (!str_part.empty())
      vec_values.push_back(LowerInvariant(str_part));
  }
  return vec_values;
}

}  // namespace

AssetBrowserData AssetBrowserData::FromManifest(const game::ModData& mod_data) {
  AssetBrowserData data_browser;
  if (const yaml::MiniYaml* yaml_node =
          mod_data.ManifestRef().GlobalModDataNode("AssetBrowser")) {
    for (const yaml::MiniYamlNode& node : yaml_node->Nodes) {
      if (node.Key == nullptr)
        continue;
      if (*node.Key == "SpriteExtensions")
        data_browser.vec_sprite_extensions = SplitCommaList(node);
      else if (*node.Key == "ModelExtensions")
        data_browser.vec_model_extensions = SplitCommaList(node);
      else if (*node.Key == "AudioExtensions")
        data_browser.vec_audio_extensions = SplitCommaList(node);
      else if (*node.Key == "VideoExtensions")
        data_browser.vec_video_extensions = SplitCommaList(node);
    }
  }
  return data_browser;
}

AssetBrowserLogic::AssetBrowserLogic(
    ora::ui::Widget& widget, std::function<void()> fn_on_exit,
    game::ModData& mod_data, gfx::WorldRenderer* ptr_world_renderer)
    : ptr_mod_data_(&mod_data), ptr_panel_(&widget) {
  static_cast<void>(ptr_world_renderer);
  static_cast<void>(fn_on_exit);

  auto& ui_singleton = ora::ui::Ui::Instance();

  const game::Manifest& manifest_mod = mod_data.ManifestRef();
  std::int32_t int4_indexed_sheet_size = 2048;
  std::int32_t int4_bgra_sheet_size = 2048;
  if (const yaml::MiniYaml* yaml_rc =
          manifest_mod.MergedNode("RendererConstants")) {
    for (const yaml::MiniYamlNode& node : yaml_rc->Nodes) {
      if (node.Key == nullptr || node.Value.Value == nullptr)
        continue;
      std::int32_t int4_value = 0;
      if (*node.Key == "SequenceIndexedSheetSize" &&
          meta::TryParseInt32Invariant(*node.Value.Value, int4_value))
        int4_indexed_sheet_size = int4_value;
      else if (*node.Key == "SequenceBgraSheetSize" &&
               meta::TryParseInt32Invariant(*node.Value.Value, int4_value))
        int4_bgra_sheet_size = int4_value;
    }
  }
  ptr_sheet_builder_indexed_ =
      std::make_unique<gfx::SheetBuilder>(gfx::SheetType::Indexed,
                                          int4_indexed_sheet_size);
  ptr_sheet_builder_bgra_ = std::make_unique<gfx::SheetBuilder>(
      gfx::SheetType::BGRA, int4_bgra_sheet_size);

  vec_sprite_loaders_ =
      gfx::MakeSpriteLoaders(manifest_mod.SpriteFormats());

  str_all_packages_ = Fluent().GetMessage("label-all-packages");

  vec_color_picker_palettes_ =
      ui_singleton.AssetBrowserColorPickerPalettesDeps();
  vec_palettes_ = ui_singleton.AssetBrowserPalettesDeps();
  vec_palettes_.insert(vec_palettes_.end(),
                       vec_color_picker_palettes_.begin(),
                       vec_color_picker_palettes_.end());

  if (auto* ptr_ticker =
          widget.GetOrNull<ora::ui::LogicTickerWidget>("ANIMATION_TICKER")) {
    ptr_ticker->fn_on_tick = [this] {
      if (b_animate_frames_ && !vec_current_sprites_.empty())
        SelectNextFrame();
    };
  }

  if (auto* ptr_source_dropdown =
          widget.GetOrNull<ora::ui::DropDownButtonWidget>(
              "SOURCE_SELECTOR")) {
    ptr_source_dropdown->fn_on_mouse_down =
        [this, ptr_source_dropdown](const ora::MouseInput&) {
          ShowSourceDropdown(*ptr_source_dropdown);
          return true;
        };
    auto sp_source_name = std::make_shared<
        core::CachedTransform<fs::IReadOnlyPackage*, std::string>>(
        [this](const fs::IReadOnlyPackage* ptr_package) {
          return GetSourceDisplayName(ptr_package);
        });
    ptr_source_dropdown->fn_get_text =
        [this, cache_name = std::move(sp_source_name)] {
          return cache_name->Update(ptr_asset_source_);
        };
  }

  if (auto* ptr_asset_type_dropdown =
          widget.GetOrNull<ora::ui::DropDownButtonWidget>(
              "ASSET_TYPES_DROPDOWN")) {
    ptr_asset_type_dropdown->fn_on_mouse_down =
        [this, ptr_asset_type_dropdown](const ora::MouseInput&) {
          ptr_asset_type_dropdown->RemovePanel();
          ora::ui::Widget* ptr_panel_types = CreateAssetTypesPanel();
          std::unique_ptr<ora::ui::Widget> up_panel_types =
              ora::ui::Ui::Instance().ReleaseWidget(ptr_panel_types);
          if (!up_panel_types)
            up_panel_types.reset(ptr_panel_types);
          ptr_asset_type_dropdown->AttachPanel(std::move(up_panel_types));
          return true;
        };
  }

  if (auto* ptr_sprite_widget =
          widget.GetOrNull<ora::ui::SpriteWidget>("SPRITE")) {
    str_current_palette_ = ptr_sprite_widget->str_palette;
    fp4_sprite_scale_ = ptr_sprite_widget->fp4_scale;
    ptr_sprite_widget->fn_get_sprite = [this]() -> gfx::Sprite {
      if (vec_current_sprites_.empty())
        return gfx::Sprite{};
      return vec_current_sprites_[static_cast<std::size_t>(
          std::clamp(int4_current_frame_, 0,
                     static_cast<int>(vec_current_sprites_.size()) - 1))];
    };
    ptr_sprite_widget->fn_get_palette = [this] {
      return str_current_palette_;
    };
    ptr_sprite_widget->fn_is_visible = [this] {
      return !b_is_video_loaded_ && !b_is_load_error_ &&
             !vec_current_sprites_.empty();
    };
    ptr_sprite_widget->fn_get_scale = [this] { return fp4_sprite_scale_; };
  }

  if (auto* ptr_player_widget =
          widget.GetOrNull<mods::VideoPlayerWidget>("PLAYER"))
    ptr_player_widget->fn_is_visible = [this] {
      return b_is_video_loaded_ && !b_is_load_error_;
    };

  if (auto* ptr_error_label = widget.GetOrNull<ora::ui::Widget>("ERROR"))
    ptr_error_label->fn_is_visible = [this] { return b_is_load_error_; };

  if (auto* ptr_palette_dropdown =
          widget.GetOrNull<ora::ui::DropDownButtonWidget>(
              "PALETTE_SELECTOR")) {
    ptr_palette_dropdown->fn_on_mouse_down =
        [this, ptr_palette_dropdown](const ora::MouseInput&) {
          ShowPaletteDropdown(*ptr_palette_dropdown);
          return true;
        };
    ptr_palette_dropdown->fn_get_text = [this] {
      return str_current_palette_;
    };
    ptr_palette_dropdown->fn_is_visible = [this] {
      return !vec_current_sprites_.empty();
    };
    if (auto* ptr_palette_desc =
            widget.GetOrNull<ora::ui::Widget>("PALETTE_DESC"))
      ptr_palette_desc->fn_is_visible = [this] {
        return !vec_current_sprites_.empty();
      };
  }

  if (auto* ptr_color_dropdown =
          widget.GetOrNull<ora::ui::DropDownButtonWidget>("COLOR")) {
    core::Color color_current = ui_singleton.PlayerColorGetDeps();
    ptr_color_dropdown->fn_is_disabled = [this] {
      return std::find(vec_color_picker_palettes_.begin(),
                       vec_color_picker_palettes_.end(),
                       str_current_palette_) ==
             vec_color_picker_palettes_.end();
    };
    ptr_color_dropdown->fn_on_mouse_down =
        [ptr_color_dropdown, ptr_world_renderer,
         color_current](const ora::MouseInput&) mutable {
          ora::ui::Ui::Instance().ShowColorDropDownDeps(
              *ptr_color_dropdown, color_current, nullptr,
              ptr_world_renderer,
              [&color_current](core::Color color_next) {
                color_current = color_next;
                ora::ui::Ui::Instance().PlayerColorSetDeps(color_next);
              });
          return true;
        };
    ptr_color_dropdown->fn_is_visible = [this] {
      return !vec_current_sprites_.empty();
    };

    if (auto* ptr_color_block =
            widget.GetOrNull<ora::ui::ColorBlockWidget>("COLORBLOCK"))
      ptr_color_block->fn_get_color =
          [color_current]() mutable { return color_current; };
  }

  ptr_filename_input_ = widget.Get<ora::ui::TextFieldWidget>("FILENAME_INPUT");
  ptr_filename_input_->fn_on_text_edited = [this] { ApplyFilter(); };
  ptr_filename_input_->fn_on_esc_key =
      [this](const ora::KeyInput&) {
        if (ptr_filename_input_->Text().empty()) {
          ptr_filename_input_->YieldKeyboardFocus();
        } else {
          ptr_filename_input_->SetText("");
          ApplyFilter();
        }
        return true;
      };

  if (auto* ptr_frame_container =
          widget.GetOrNull<ora::ui::Widget>("FRAME_SELECTOR"))
    ptr_frame_container->fn_is_visible = [this] {
      return (vec_current_sprites_.size() > 1) || b_is_video_loaded_ ||
             opt_current_sound_format_.has_value();
    };

  ptr_frame_slider_ = widget.GetOrNull<ora::ui::SliderWidget>("FRAME_SLIDER");
  if (ptr_frame_slider_ != nullptr) {
    ptr_frame_slider_->fn_on_change = [this](float fp4_new_value) {
      if (!b_is_video_loaded_)
        int4_current_frame_ =
            static_cast<int>(std::lround(fp4_new_value));
    };

    ptr_frame_slider_->fn_get_value = [this] {
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        if (ptr_player != nullptr && ptr_player->Video() != nullptr)
          return static_cast<float>(
              ptr_player->Video()->CurrentFrameIndex());
        return 0.0f;
      }

      if (ptr_current_sound_ != nullptr)
        return ptr_current_sound_->SeekPosition() *
               static_cast<float>(
                   opt_current_sound_format_->int4_sample_rate);

      return static_cast<float>(int4_current_frame_);
    };

    ptr_frame_slider_->fn_is_disabled = [this] {
      return b_is_video_loaded_ || opt_current_sound_format_.has_value();
    };
  }

  if (auto* ptr_frame_text =
          widget.GetOrNull<ora::ui::LabelWidget>("FRAME_COUNT")) {
    auto sp_sound_length =
        std::make_shared<core::CachedTransform<double, std::string>>(
            [](const double& fp8_seconds) {
              const double fp8_rounded =
                  std::round(fp8_seconds * 1000.0) / 1000.0;
              return Fluent().GetMessage(
                  "label-length-in-seconds",
                  std::vector<std::pair<std::string, fluent::FluentValue>>{
                      {std::string{"length"},
                       fluent::FluentValue{
                           std::format("{}", fp8_rounded)}}});
            });
    ptr_frame_text->fn_get_text =
        [this, cache_length = std::move(sp_sound_length)] {
          if (b_is_video_loaded_) {
            auto* ptr_player =
                ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
            if (ptr_player != nullptr && ptr_player->Video() != nullptr)
              return std::format(
                  "{} / {}", ptr_player->Video()->CurrentFrameIndex() + 1,
                  ptr_player->Video()->FrameCount());
            return std::string{};
          }

          if (opt_current_sound_format_.has_value())
            return cache_length->Update(
                opt_current_sound_format_->fp4_length_in_seconds);

          return std::format("{} / {}", int4_current_frame_,
                             static_cast<int>(vec_current_sprites_.size()) -
                                 1);
        };
  }

  if (auto* ptr_play_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BUTTON_PLAY")) {
    ptr_play_button->fn_on_click = [this] {
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        if (ptr_player != nullptr)
          ptr_player->Play();
      } else if (opt_current_sound_format_.has_value()) {
        auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
        if (ptr_sound_engine != nullptr) {
          if (ptr_current_sound_ != nullptr)
            ptr_sound_engine->StopSound(ptr_current_sound_);
          const sound::SoundFormatInfo info_copy =
              *opt_current_sound_format_;
          ptr_current_sound_ = ptr_sound_engine->PlayFromPcm(
              info_copy, vec_current_pcm_,
              ptr_sound_engine->SoundVolume());
        }
      } else {
        b_animate_frames_ = true;
      }
    };

    ptr_play_button->fn_is_visible = [this] {
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        return ptr_player == nullptr || ptr_player->Paused();
      }
      return !b_animate_frames_ || opt_current_sound_format_.has_value();
    };
  }

  if (auto* ptr_pause_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BUTTON_PAUSE")) {
    ptr_pause_button->fn_on_click = [this] {
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        if (ptr_player != nullptr)
          ptr_player->Pause();
      } else {
        b_animate_frames_ = false;
      }
    };

    ptr_pause_button->fn_is_visible = [this] {
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        return ptr_player == nullptr || !ptr_player->Paused();
      }
      return b_animate_frames_ && !opt_current_sound_format_.has_value();
    };
  }

  if (auto* ptr_stop_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BUTTON_STOP")) {
    ptr_stop_button->fn_on_click = [this] {
      auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
      if (b_is_video_loaded_) {
        auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER");
        if (ptr_player != nullptr)
          ptr_player->Stop();
      } else if (ptr_current_sound_ != nullptr) {
        if (ptr_sound_engine != nullptr)
          ptr_sound_engine->StopSound(ptr_current_sound_);
        ptr_current_sound_ = nullptr;
      } else {
        int4_current_frame_ = 0;
        b_animate_frames_ = false;
      }

      if (ptr_frame_slider_ != nullptr)
        ptr_frame_slider_->fp4_value = 0.0f;
    };
  }

  if (auto* ptr_next_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BUTTON_NEXT")) {
    ptr_next_button->fn_on_click = [this] {
      if (!b_is_video_loaded_)
        SelectNextFrame();
    };

    ptr_next_button->fn_is_visible = [this] {
      return !b_is_video_loaded_ && !opt_current_sound_format_.has_value();
    };
  }

  if (auto* ptr_prev_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("BUTTON_PREV")) {
    ptr_prev_button->fn_on_click = [this] {
      if (!b_is_video_loaded_)
        SelectPreviousFrame();
    };

    ptr_prev_button->fn_is_visible = [this] {
      return !b_is_video_loaded_ && !opt_current_sound_format_.has_value();
    };
  }

  if (auto* ptr_sprite_scale_slider =
          widget.GetOrNull<ora::ui::SliderWidget>("SPRITE_SCALE_SLIDER")) {
    ptr_sprite_scale_slider->fn_on_change =
        [this](float fp4_new_value) { fp4_sprite_scale_ = fp4_new_value; };
    ptr_sprite_scale_slider->fn_get_value = [this] {
      return fp4_sprite_scale_;
    };
    ptr_sprite_scale_slider->fn_is_visible = [this] {
      return !vec_current_sprites_.empty();
    };
    if (auto* ptr_scale_label =
            widget.GetOrNull<ora::ui::Widget>("SPRITE_SCALE"))
      ptr_scale_label->fn_is_visible = [this] {
        return !vec_current_sprites_.empty();
      };
  }

  if (auto* ptr_voxel_container =
          widget.GetOrNull<ora::ui::Widget>("VOXEL_SELECTOR"))
    ptr_voxel_container->fn_is_visible = [] { return false; };

  if (auto* ptr_roll_slider =
          widget.GetOrNull<ora::ui::SliderWidget>("ROLL_SLIDER")) {
    ptr_roll_slider->fn_on_change = [this](float fp4_new_value) {
      rot_model_orientation_ = rot_model_orientation_.WithRoll(
          WAngle{static_cast<int>(fp4_new_value)});
    };

    ptr_roll_slider->fn_get_value = [this] {
      return static_cast<float>(rot_model_orientation_.Roll.Angle);
    };
  }

  if (auto* ptr_pitch_slider =
          widget.GetOrNull<ora::ui::SliderWidget>("PITCH_SLIDER")) {
    ptr_pitch_slider->fn_on_change = [this](float fp4_new_value) {
      rot_model_orientation_ = rot_model_orientation_.WithPitch(
          WAngle{static_cast<int>(fp4_new_value)});
    };

    ptr_pitch_slider->fn_get_value = [this] {
      return static_cast<float>(rot_model_orientation_.Pitch.Angle);
    };
  }

  if (auto* ptr_yaw_slider =
          widget.GetOrNull<ora::ui::SliderWidget>("YAW_SLIDER")) {
    ptr_yaw_slider->fn_on_change = [this](float fp4_new_value) {
      rot_model_orientation_ = rot_model_orientation_.WithYaw(
          WAngle{static_cast<int>(fp4_new_value)});
    };

    ptr_yaw_slider->fn_get_value = [this] {
      return static_cast<float>(rot_model_orientation_.Yaw.Angle);
    };
  }

  if (auto* ptr_model_scale_slider =
          widget.GetOrNull<ora::ui::SliderWidget>("MODEL_SCALE_SLIDER")) {
    ptr_model_scale_slider->fn_on_change =
        [this](float fp4_new_value) { fp4_model_scale_ = fp4_new_value; };
    ptr_model_scale_slider->fn_get_value = [this] {
      return fp4_model_scale_;
    };
    ptr_model_scale_slider->fn_is_visible = [] { return false; };
    if (auto* ptr_scale_label =
            widget.GetOrNull<ora::ui::Widget>("MODEL_SCALE"))
      ptr_scale_label->fn_is_visible = [] { return false; };
  }

  data_browser_ = AssetBrowserData::FromManifest(mod_data);

  std::set<std::string> set_allowed;
  for (const std::string& str_ext : data_browser_.vec_sprite_extensions)
    set_allowed.insert(str_ext);
  for (const std::string& str_ext : data_browser_.vec_model_extensions)
    set_allowed.insert(str_ext);
  for (const std::string& str_ext : data_browser_.vec_audio_extensions)
    set_allowed.insert(str_ext);
  for (const std::string& str_ext : data_browser_.vec_video_extensions)
    set_allowed.insert(str_ext);
  vec_allowed_extensions_.assign(set_allowed.begin(), set_allowed.end());

  for (fs::IReadOnlyPackage* ptr_package :
       mod_data.ModFiles().MountedPackages()) {
    for (const std::string& str_content : ptr_package->Contents()) {
      if (set_allowed.count(ExtensionOf(str_content)) != 0) {
        vec_acceptable_packages_.push_back(ptr_package);
        break;
      }
    }
  }

  ptr_asset_list_ = widget.Get<ora::ui::ScrollPanelWidget>("ASSET_LIST");
  {
    std::unique_ptr<ora::ui::Widget> up_template_taken =
        ptr_asset_list_->RemoveChild(
            *ptr_asset_list_->Get<ora::ui::Widget>("ASSET_TEMPLATE"));
    up_template_.reset(
        static_cast<ora::ui::ScrollItemWidget*>(up_template_taken.release()));
  }
  PopulateAssetList();

  if (auto* ptr_close_button =
          widget.GetOrNull<ora::ui::ButtonWidget>("CLOSE_BUTTON")) {
    ptr_close_button->fn_on_click =
        [this, fn_on_exit = std::move(fn_on_exit)] {
          ClearLoadedAssets();
          ora::ui::Ui::Instance().CloseWindow();
          if (fn_on_exit)
            fn_on_exit();
        };
  }
}

AssetBrowserLogic::~AssetBrowserLogic() { ClearLoadedAssets(); }

void AssetBrowserLogic::SelectNextFrame() {
  ++int4_current_frame_;
  if (int4_current_frame_ >= static_cast<int>(vec_current_sprites_.size()))
    int4_current_frame_ = 0;
}

void AssetBrowserLogic::SelectPreviousFrame() {
  --int4_current_frame_;
  if (int4_current_frame_ < 0)
    int4_current_frame_ =
        static_cast<int>(vec_current_sprites_.size()) - 1;
}

bool AssetBrowserLogic::FilterAsset(const std::string& str_filename) const {
  const std::string& str_filter = ptr_filename_input_->Text();

  bool b_only_whitespace = true;
  for (const char c : str_filter)
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      b_only_whitespace = false;
      break;
    }
  if (str_filter.empty() || b_only_whitespace)
    return true;

  if (str_filename.find(str_filter) != std::string::npos)
    return true;

  return false;
}

void AssetBrowserLogic::ApplyFilter() {
  vec_asset_visibility_.clear();
  if (ptr_asset_list_->ptr_layout != nullptr)
    ptr_asset_list_->ptr_layout->AdjustChildren();
  ptr_asset_list_->ScrollToTop();

  for (const auto& [str_path, b_visible] : vec_asset_visibility_) {
    if (!b_visible)
      continue;
    fs::IReadOnlyPackage* ptr_package = nullptr;
    std::string str_actual;
    if (ptr_mod_data_->ModFiles().TryGetPackageContaining(
            str_path, ptr_package, str_actual)) {
      LoadAsset(ptr_package, str_actual);
      return;
    }
  }
}

void AssetBrowserLogic::AddAsset(
    ora::ui::ScrollPanelWidget& widget_list, const std::string& str_filepath,
    fs::IReadOnlyPackage* ptr_package) {
  auto up_item = ora::ui::ScrollItemWidget::Setup(
      *up_template_,
      [this, str_filepath, ptr_package] {
        return str_current_filename_ == str_filepath &&
               ptr_current_package_ == ptr_package;
      },
      [this, str_filepath, ptr_package] {
        LoadAsset(ptr_package, str_filepath);
      });

  auto* ptr_label =
      up_item->Get<ora::ui::LabelWithTooltipWidget>("TITLE");
  ora::ui::TruncateLabelToTooltip(*ptr_label, str_filepath);
  ptr_label->fn_get_tooltip_text = [str_filepath, ptr_package] {
    return str_filepath + "\n" + ptr_package->Name();
  };

  const std::string str_extension = ExtensionOf(str_filepath);
  up_item->fn_is_visible = [this, str_filepath, str_extension] {
    const bool b_allowed =
        ((int4_asset_types_to_display_ & kAssetTypeSprite) != 0 &&
         std::find(data_browser_.vec_sprite_extensions.begin(),
                   data_browser_.vec_sprite_extensions.end(),
                   str_extension) !=
             data_browser_.vec_sprite_extensions.end()) ||
        ((int4_asset_types_to_display_ & kAssetTypeModel) != 0 &&
         std::find(data_browser_.vec_model_extensions.begin(),
                   data_browser_.vec_model_extensions.end(),
                   str_extension) !=
             data_browser_.vec_model_extensions.end()) ||
        ((int4_asset_types_to_display_ & kAssetTypeAudio) != 0 &&
         std::find(data_browser_.vec_audio_extensions.begin(),
                   data_browser_.vec_audio_extensions.end(),
                   str_extension) !=
             data_browser_.vec_audio_extensions.end()) ||
        ((int4_asset_types_to_display_ & kAssetTypeVideo) != 0 &&
         std::find(data_browser_.vec_video_extensions.begin(),
                   data_browser_.vec_video_extensions.end(),
                   str_extension) !=
             data_browser_.vec_video_extensions.end()) ||
        ((int4_asset_types_to_display_ & kAssetTypeUnknown) != 0 &&
         std::find(vec_allowed_extensions_.begin(),
                   vec_allowed_extensions_.end(),
                   str_extension) == vec_allowed_extensions_.end());

    const auto it_visible = std::find_if(
        vec_asset_visibility_.begin(), vec_asset_visibility_.end(),
        [&str_filepath](
            const std::pair<std::string, bool>& pair_visible) {
          return pair_visible.first == str_filepath;
        });
    if (it_visible != vec_asset_visibility_.end())
      return it_visible->second && b_allowed;

    const bool b_visible = FilterAsset(str_filepath);
    vec_asset_visibility_.emplace_back(str_filepath, b_visible);
    return b_visible && b_allowed;
  };

  widget_list.AddChild(std::move(up_item));
}

bool AssetBrowserLogic::LoadAsset(fs::IReadOnlyPackage* ptr_package,
                                  const std::string& str_filename) {
  ClearLoadedAssets();

  if (str_filename.empty())
    return false;

  if (!ptr_package->Contains(str_filename))
    return false;

  b_is_load_error_ = false;

  ptr_current_package_ = ptr_package;
  str_current_filename_ = str_filename;
  std::string str_prefix;
  if (const auto opt_prefix =
          ptr_mod_data_->ModFiles().GetPrefix(ptr_package))
    str_prefix = *opt_prefix + "|";

  const std::string str_file_extension = ExtensionOf(str_filename);
  const auto fn_in = [&str_file_extension](const std::vector<std::string>& vec) {
    return std::find(vec.begin(), vec.end(), str_file_extension) != vec.end();
  };

  if (fn_in(data_browser_.vec_sprite_extensions)) {
    const std::string str_key = str_prefix + str_filename;
    const auto it_cached = std::find_if(
        vec_sprite_cache_.begin(), vec_sprite_cache_.end(),
        [&str_key](const std::pair<std::string, std::vector<gfx::Sprite>>&
                       pair_cached) { return pair_cached.first == str_key; });
    if (it_cached != vec_sprite_cache_.end()) {
      vec_current_sprites_ = it_cached->second;
    } else {
      vec_current_sprites_.clear();
      if (auto opt_frames = gfx::TryGetFrames(
              ptr_mod_data_->ModFiles(), str_key, vec_sprite_loaders_)) {
        for (const std::unique_ptr<gfx::ISpriteFrame>& ptr_frame :
             opt_frames->vec_frames) {
          gfx::SheetBuilder& builder =
              gfx::SheetBuilder::FrameTypeToSheetType(ptr_frame->Type()) ==
                      gfx::SheetType::BGRA
                  ? *ptr_sheet_builder_bgra_
                  : *ptr_sheet_builder_indexed_;
          vec_current_sprites_.push_back(builder.Add(*ptr_frame));
        }
      }
      vec_sprite_cache_.emplace_back(str_key, vec_current_sprites_);
    }
    int4_current_frame_ = 0;

    if (ptr_frame_slider_ != nullptr && !vec_current_sprites_.empty()) {
      ptr_frame_slider_->fp4_maximum_value =
          static_cast<float>(vec_current_sprites_.size()) - 1.0f;
      ptr_frame_slider_->int4_ticks =
          static_cast<int>(vec_current_sprites_.size());
    }
  } else if (fn_in(data_browser_.vec_model_extensions)) {
    vec_current_sprites_.clear();
  } else if (fn_in(data_browser_.vec_audio_extensions)) {
    MuteSounds();

    std::vector<char> vec_bytes;
    if (!ptr_mod_data_->ModFiles().TryOpen(str_prefix + str_filename,
                                           vec_bytes))
      return false;
    const std::span<const std::byte> span_bytes{
        reinterpret_cast<const std::byte*>(vec_bytes.data()),
        vec_bytes.size()};

    for (const sound::SoundLoaderFn fn_loader : sound::MakeSoundLoaders(
             ptr_mod_data_->ManifestRef().SoundFormats())) {
      sound::SoundFormatInfo info_format;
      std::vector<std::byte> vec_pcm;
      if (fn_loader(span_bytes, info_format, vec_pcm)) {
        opt_current_sound_format_ = info_format;
        if (ptr_frame_slider_ != nullptr) {
          ptr_frame_slider_->fp4_maximum_value =
              info_format.fp4_length_in_seconds *
              static_cast<float>(info_format.int4_sample_rate);
          ptr_frame_slider_->int4_ticks = 0;
        }

        auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
        if (ptr_sound_engine != nullptr) {
          vec_current_pcm_ = std::move(vec_pcm);
          const sound::SoundFormatInfo info_copy = info_format;
          ptr_current_sound_ = ptr_sound_engine->PlayFromPcm(
              info_copy, vec_current_pcm_,
              ptr_sound_engine->SoundVolume());
        }
        break;
      }
    }
  } else if (fn_in(data_browser_.vec_video_extensions)) {
    MuteSounds();

    auto fn_load_video = ora::ui::Ui::Instance().LoadVideoDeps();
    if (fn_load_video) {
      if (auto up_video =
              fn_load_video(str_prefix + str_filename, true)) {
        auto* ptr_player =
            ptr_panel_->Get<mods::VideoPlayerWidget>("PLAYER");
        ptr_player->LoadAndPlay(str_prefix + str_filename);
        ptr_player->b_draw_overlay = false;
        b_is_video_loaded_ = true;

        if (ptr_frame_slider_ != nullptr && ptr_player->Video() != nullptr) {
          ptr_frame_slider_->fp4_maximum_value =
              static_cast<float>(ptr_player->Video()->FrameCount()) - 1.0f;
          ptr_frame_slider_->int4_ticks = 0;
        }
      }
    }
  } else {
    return false;
  }

  return true;
}

bool AssetBrowserLogic::ShowSourceDropdown(
    ora::ui::DropDownButtonWidget& widget_dropdown) {
  auto sp_source_name = std::make_shared<
      core::CachedTransform<fs::IReadOnlyPackage*, std::string>>(
      [this](const fs::IReadOnlyPackage* ptr_package) {
        return GetSourceDisplayName(ptr_package);
      });

  const auto fn_setup_item =
      [this, sp_source_name](
          fs::IReadOnlyPackage* ptr_source,
          ora::ui::ScrollItemWidget& widget_item_template)
      -> std::unique_ptr<ora::ui::ScrollItemWidget> {
    auto up_item = ora::ui::ScrollItemWidget::Setup(
        widget_item_template,
        [this, ptr_source] { return ptr_asset_source_ == ptr_source; },
        [this, ptr_source] {
          ptr_asset_source_ = ptr_source;
          PopulateAssetList();
        });

    up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
        [sp_source_name, ptr_source]() {
          return sp_source_name->Update(ptr_source);
        };
    return up_item;
  };

  std::vector<fs::IReadOnlyPackage*> vec_sources;
  vec_sources.push_back(nullptr);
  vec_sources.insert(vec_sources.end(), vec_acceptable_packages_.begin(),
                     vec_acceptable_packages_.end());
  widget_dropdown.ShowDropDown<fs::IReadOnlyPackage*>(
      "LABEL_DROPDOWN_TEMPLATE", 280, vec_sources,
      std::function<std::unique_ptr<ora::ui::ScrollItemWidget>(
          fs::IReadOnlyPackage* const&, ora::ui::ScrollItemWidget&)>(
          fn_setup_item));
  return true;
}

void AssetBrowserLogic::PopulateAssetList() {
  ptr_asset_list_->RemoveChildren();
  vec_asset_file_paths_for_test_.clear();

  std::map<std::string, std::vector<fs::IReadOnlyPackage*>> map_files;

  if (ptr_asset_source_ != nullptr) {
    for (const std::string& str_content : ptr_asset_source_->Contents())
      map_files[str_content].push_back(ptr_asset_source_);
  } else {
    for (fs::IReadOnlyPackage* ptr_package :
         ptr_mod_data_->ModFiles().MountedPackages()) {
      for (const std::string& str_content : ptr_package->Contents())
        map_files[str_content].push_back(ptr_package);
    }
  }

  for (const auto& [str_file, vec_packages] : map_files) {
    for (fs::IReadOnlyPackage* ptr_package : vec_packages) {
      const std::string str_file_extension = ExtensionOf(str_file);
      if (str_file_extension.empty())
        continue;

      vec_asset_file_paths_for_test_.push_back(str_file);
      AddAsset(*ptr_asset_list_, str_file, ptr_package);
    }
  }
}

bool AssetBrowserLogic::ShowPaletteDropdown(
    ora::ui::DropDownButtonWidget& widget_dropdown) {
  const auto fn_setup_item =
      [this](const std::string& str_name,
             ora::ui::ScrollItemWidget& widget_item_template)
      -> std::unique_ptr<ora::ui::ScrollItemWidget> {
    auto up_item = ora::ui::ScrollItemWidget::Setup(
        widget_item_template, [this, str_name] {
          return str_current_palette_ == str_name;
        },
        [this, str_name] { str_current_palette_ = str_name; });

    up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
        [str_name] { return str_name; };
    return up_item;
  };

  widget_dropdown.ShowDropDown<std::string>(
      "LABEL_DROPDOWN_TEMPLATE", 280, vec_palettes_,
      std::function<std::unique_ptr<ora::ui::ScrollItemWidget>(
          const std::string&, ora::ui::ScrollItemWidget&)>(fn_setup_item));
  return true;
}

std::string AssetBrowserLogic::GetSourceDisplayName(
    const fs::IReadOnlyPackage* ptr_package) const {
  if (ptr_package == nullptr)
    return str_all_packages_;

  std::string str_name;
  if (const auto opt_prefix =
          ptr_mod_data_->ModFiles().GetPrefix(ptr_package))
    str_name = *opt_prefix;

  if (str_name.empty())
    str_name = ptr_package->Name();

  if (str_name.size() > 18)
    str_name = "..." + str_name.substr(str_name.size() - 15);

  return str_name;
}

void AssetBrowserLogic::MuteSounds() {
  auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
  if (ptr_sound_engine == nullptr)
    return;
  if (ptr_sound_engine->MusicVolume() > 0) {
    fp4_cached_music_volume_ = ptr_sound_engine->MusicVolume();
    b_music_muted_ = true;
    ptr_sound_engine->SetMusicVolume(0.0f);
  }
}

void AssetBrowserLogic::UnMuteSounds() {
  auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
  if (ptr_sound_engine == nullptr)
    return;
  if (b_music_muted_ && fp4_cached_music_volume_ > 0)
    ptr_sound_engine->SetMusicVolume(fp4_cached_music_volume_);
  b_music_muted_ = false;
}

void AssetBrowserLogic::ClearLoadedAssets() {
  auto* ptr_sound_engine = ora::ui::Ui::Instance().SoundDeps();
  if (ptr_current_sound_ != nullptr && ptr_sound_engine != nullptr) {
    ptr_sound_engine->StopSound(ptr_current_sound_);
    ptr_current_sound_ = nullptr;
  }

  vec_current_sprites_.clear();
  int4_current_frame_ = 0;

  opt_current_sound_format_.reset();
  vec_current_pcm_.clear();

  if (b_is_video_loaded_) {
    if (auto* ptr_player =
            ptr_panel_->GetOrNull<mods::VideoPlayerWidget>("PLAYER"))
      ptr_player->Stop();
  }
  b_is_video_loaded_ = false;

  UnMuteSounds();
}

ora::ui::Widget* AssetBrowserLogic::CreateAssetTypesPanel() {
  ora::ui::Widget* ptr_asset_types_panel =
      ora::ui::Ui::Instance().LoadWidget("ASSET_TYPES_PANEL", nullptr,
                                         ora::ui::WidgetArgs{});
  auto* ptr_asset_type_template =
      ptr_asset_types_panel->Get<ora::ui::CheckboxWidget>(
          "ASSET_TYPE_TEMPLATE");

  const std::pair<int, std::string> vec_all_asset_types[] = {
      {kAssetTypeSprite, "Sprite"},
      {kAssetTypeModel, "Model"},
      {kAssetTypeAudio, "Audio"},
      {kAssetTypeVideo, "Video"},
      {kAssetTypeUnknown, "Unknown"}};
  for (const auto& [int4_type, str_text] : vec_all_asset_types) {
    auto up_asset_type = ptr_asset_type_template->Clone();
    if (auto* ptr_checkbox =
            dynamic_cast<ora::ui::CheckboxWidget*>(up_asset_type.get())) {
      ptr_checkbox->fn_get_text = [str_text] { return str_text; };
      ptr_checkbox->fn_is_checked = [this, int4_type] {
        return (int4_asset_types_to_display_ & int4_type) != 0;
      };
      ptr_checkbox->fn_is_visible = [] { return true; };
      ptr_checkbox->fn_on_click = [this, int4_type] {
        int4_asset_types_to_display_ ^= int4_type;
        PopulateAssetList();
      };
    }

    ptr_asset_types_panel->AddChild(std::move(up_asset_type));
  }

  return ptr_asset_types_panel;
}

void RegisterAssetBrowserLogic() {
  ora::ui::ChromeLogicRegistry::Instance().Register(
      "AssetBrowserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void()> fn_on_exit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onExit")))
          fn_on_exit = *ptr_value;

        gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value = std::get_if<gfx::WorldRenderer*>(
                &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;

        return std::make_unique<AssetBrowserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            std::move(fn_on_exit),
            *std::get<game::ModData*>(args_args.At("modData")),
            ptr_world_renderer);
      });
}

}  // namespace ora::mods
