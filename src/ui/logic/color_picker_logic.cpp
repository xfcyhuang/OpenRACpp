// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ColorPickerLogic.cs @1f594e320
// (color_picker_logic.hpp 的实现)
// The implementation of color_picker_logic.hpp.
import std;

#include "ui/logic/color_picker_logic.hpp"

#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "meta/parse.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/color_mixer_widget.hpp"
#include "ui/widgets/hue_slider_widget.hpp"

namespace ora::ui {
namespace {

bool b_palette_tab_opened_last = false;

const meta::GeneratedRecord* WorldColorManagerRecord(game::ModData& mod_data) {
  const game::ActorInfo* ptr_actor_world =
      mod_data.DefaultRules().FindActor("world");
  if (ptr_actor_world == nullptr)
    throw std::runtime_error(
        "Actor does not have a trait of type IColorPickerManagerInfo");

  for (const meta::RecordObject* rec_trait : ptr_actor_world->TraitInfosByInterface(
           "OpenRA.Mods.Common.Traits.IColorPickerManagerInfo"))
    return static_cast<const meta::GeneratedRecord*>(rec_trait);

  throw std::runtime_error(
      "Actor does not have a trait of type IColorPickerManagerInfo");
}

std::size_t FieldIndexOf(const meta::RecordDesc& desc, std::string_view str_name) {
  const std::vector<const meta::FieldDesc*> vec_fields = meta::CollectFields(desc);
  for (std::size_t sz_i = 0; sz_i < vec_fields.size(); sz_i++)
    if (vec_fields[sz_i]->str_name == str_name)
      return sz_i;
  throw std::runtime_error("FieldLoader: Missing field `" + std::string(str_name) +
                           "`");
}

}  // namespace

ColorPickerLogic::ColorPickerLogic(WidgetArgs& args_args) {
  Widget* ptr_widget = std::get<Widget*>(args_args.At("widget"));
  game::ModData* ptr_mod_data =
      std::get<game::ModData*>(args_args.At("modData"));
  sim::World* ptr_world = std::get<sim::World*>(args_args.At("world"));
  const core::Color color_initial = std::get<core::Color>(args_args.At("initialColor"));
  const auto fn_on_change =
      std::get<std::function<void(core::Color)>>(args_args.At("onChange"));
  const auto fn_extra_logic =
      std::get<std::function<void(Widget*)>>(args_args.At("extraLogic"));

  ColorMixerWidget* ptr_mixer = ptr_widget->Get<ColorMixerWidget>("MIXER");
  HueSliderWidget* ptr_hue_slider = ptr_widget->Get<HueSliderWidget>("HUE_SLIDER");

  const meta::GeneratedRecord* rec_manager = WorldColorManagerRecord(*ptr_mod_data);
  const std::vector<meta::GenericValue>& vec_values = rec_manager->Values();
  const auto& vec_sat_range =
      std::get<std::vector<meta::GenericValue>>(vec_values[FieldIndexOf(
          rec_manager->record_desc(), "HsvSaturationRange")].val);
  const auto& vec_val_range =
      std::get<std::vector<meta::GenericValue>>(vec_values[FieldIndexOf(
          rec_manager->record_desc(), "HsvValueRange")].val);
  data_manager_.arr_hsv_saturation_range = {
      std::get<float>(vec_sat_range[0].val), std::get<float>(vec_sat_range[1].val)};
  data_manager_.arr_hsv_value_range = {
      std::get<float>(vec_val_range[0].val), std::get<float>(vec_val_range[1].val)};
  data_manager_.SimilarityThreshold = static_cast<int>(
      std::get<std::int64_t>(vec_values[FieldIndexOf(
          rec_manager->record_desc(), "SimilarityThreshold")].val));
  for (const meta::GenericValue& val_color :
       std::get<std::vector<meta::GenericValue>>(
           vec_values[FieldIndexOf(rec_manager->record_desc(), "PresetColors")].val))
    data_manager_.vec_preset_colors.push_back(
        core::Color::FromArgb(static_cast<std::uint32_t>(
            std::get<std::int64_t>(val_color.val))));

  const float fp4_s_min = data_manager_.arr_hsv_saturation_range[0];
  const float fp4_s_max = data_manager_.arr_hsv_saturation_range[1];
  const float fp4_v_min = data_manager_.arr_hsv_value_range[0];
  const float fp4_v_max = data_manager_.arr_hsv_value_range[1];
  ptr_mixer->SetColorLimits(fp4_s_min, fp4_s_max, fp4_v_min, fp4_v_max);
  ptr_mixer->OnChangeAdd([ptr_mixer, fn_on_change] {
    fn_on_change(ptr_mixer->ColorValue());
  });
  ptr_mixer->Set(color_initial);

  ptr_hue_slider->fn_on_change =
      [ptr_mixer, fn_on_change, fp4_s_min, fp4_s_max, fp4_v_min,
       fp4_v_max](float fp4_h_new) {
        ptr_mixer->SetColorLimits(fp4_s_min, fp4_s_max, fp4_v_min, fp4_v_max,
                                  fp4_h_new);
        const auto [int4_a, int4_h_ignored, fp4_s, fp4_v] =
            ptr_mixer->ColorValue().ToAhsv();
        (void)int4_a;
        (void)int4_h_ignored;
        ptr_mixer->Set(core::Color::FromAhsv(fp4_h_new, fp4_s, fp4_v));
        fn_on_change(ptr_mixer->ColorValue());
      };

  const auto [int4_a_init, fp4_h_init, int4_s_init_ignored, int4_v_init_ignored] =
      color_initial.ToAhsv();
  (void)int4_a_init;
  (void)int4_s_init_ignored;
  (void)int4_v_init_ignored;
  ptr_hue_slider->UpdateValue(fp4_h_init);

  if (ButtonWidget* ptr_random_button =
          ptr_widget->GetOrNull<ButtonWidget>("RANDOM_BUTTON")) {
    for (const auto& [str_key_ignored, up_info_terrain] : ptr_mod_data->TerrainInfos()) {
      (void)str_key_ignored;
      for (const core::Color color_terrain : up_info_terrain->RestrictedPlayerColors())
        if (std::find(vec_terrain_colors_.begin(), vec_terrain_colors_.end(),
                      color_terrain) == vec_terrain_colors_.end())
          vec_terrain_colors_.push_back(color_terrain);
    }

    ptr_random_button->fn_on_click = [this, ptr_mixer, ptr_hue_slider, ptr_world] {
      const core::Color color_random = server::ColorRandomValidColor(
          data_manager_, ptr_world->LocalRandom(), vec_terrain_colors_, {});
      ptr_mixer->Set(color_random);
      const auto [int4_a, fp4_h, int4_s_ignored, int4_v_ignored] =
          color_random.ToAhsv();
      (void)int4_a;
      (void)int4_s_ignored;
      (void)int4_v_ignored;
      ptr_hue_slider->UpdateValue(fp4_h);
    };
  }

  fn_on_change(color_initial);

  Widget* ptr_mixer_tab = ptr_widget->Get("MIXER_TAB");
  Widget* ptr_palette_tab = ptr_widget->Get("PALETTE_TAB");
  Widget* ptr_palette_tab_panel = ptr_widget->Get("PALETTE_TAB_PANEL");
  ButtonWidget* ptr_mixer_tab_button = ptr_widget->Get<ButtonWidget>("MIXER_TAB_BUTTON");
  ButtonWidget* ptr_palette_tab_button =
      ptr_widget->Get<ButtonWidget>("PALETTE_TAB_BUTTON");
  ContainerWidget* ptr_preset_area =
      ptr_palette_tab_panel->Get<ContainerWidget>("PRESET_AREA");
  ContainerWidget* ptr_custom_area =
      ptr_palette_tab_panel->Get<ContainerWidget>("CUSTOM_AREA");
  ColorBlockWidget* ptr_preset_color_template =
      ptr_palette_tab_panel->Get<ColorBlockWidget>("COLORPRESET");
  ColorBlockWidget* ptr_custom_color_template =
      ptr_palette_tab_panel->Get<ColorBlockWidget>("COLORCUSTOM");

  ptr_mixer_tab->fn_is_visible = [] { return !b_palette_tab_opened_last; };
  ptr_mixer_tab_button->fn_on_click = [] { b_palette_tab_opened_last = false; };
  ptr_mixer_tab_button->fn_is_highlighted = [ptr_mixer_tab] {
    return ptr_mixer_tab->fn_is_visible();
  };

  ptr_palette_tab->fn_is_visible = [] { return b_palette_tab_opened_last; };
  ptr_palette_tab_button->fn_on_click = [] { b_palette_tab_opened_last = true; };
  ptr_palette_tab_button->fn_is_highlighted = [this, ptr_palette_tab] {
    return ptr_palette_tab->fn_is_visible() || int4_palette_tab_highlighted > 0;
  };

  int int4_palette_cols = 8;
  int int4_palette_preset_rows = 2;
  int int4_palette_custom_rows = 1;

  const YamlDictionary* vec_logic_args = nullptr;
  if (const WidgetArgValue* ptr_value = args_args.TryGet("logicArgs"))
    if (const auto* ptr_args = std::get_if<const YamlDictionary*>(ptr_value))
      vec_logic_args = *ptr_args;
  const auto fn_parse_logic_arg =
      [&vec_logic_args](std::string_view str_key, int& int4_out) {
        if (vec_logic_args == nullptr)
          return;
        for (const auto& [str_entry_key, ptr_node] : *vec_logic_args) {
          if (str_entry_key != str_key)
            continue;
          const std::string_view str_value =
              ptr_node->Value != nullptr
                  ? std::string_view{*ptr_node->Value}
                  : std::string_view{};
          if (!meta::TryParseInt32Invariant(str_value, int4_out))
            throw std::runtime_error("Invalid value for " +
                                     std::string(str_key) + ": " +
                                     std::string(str_value));
          return;
        }
      };
  fn_parse_logic_arg("PaletteColumns", int4_palette_cols);
  fn_parse_logic_arg("PalettePresetRows", int4_palette_preset_rows);
  fn_parse_logic_arg("PaletteCustomRows", int4_palette_custom_rows);

  for (int int4_j = 0; int4_j < int4_palette_preset_rows; int4_j++) {
    for (int int4_i = 0; int4_i < int4_palette_cols; int4_i++) {
      const int int4_color_index = int4_j * int4_palette_cols + int4_i;
      if (static_cast<std::size_t>(int4_color_index) >=
          data_manager_.vec_preset_colors.size())
        break;

      const core::Color color_preset =
          data_manager_.vec_preset_colors[int4_color_index];

      std::unique_ptr<Widget> up_swatch = ptr_preset_color_template->Clone();
      ColorBlockWidget* ptr_swatch =
          static_cast<ColorBlockWidget*>(up_swatch.get());
      ptr_swatch->fn_get_color = [color_preset] { return color_preset; };
      ptr_swatch->fn_is_visible = [] { return true; };
      ptr_swatch->bounds_widget.int4_x =
          int4_i * ptr_swatch->bounds_widget.int4_width;
      ptr_swatch->bounds_widget.int4_y =
          int4_j * ptr_swatch->bounds_widget.int4_height;
      ptr_swatch->fn_on_mouse_up =
          [ptr_mixer, ptr_hue_slider, color_preset](const MouseInput&) {
            ptr_mixer->Set(color_preset);
            const auto [int4_a, fp4_h, int4_s_ignored, int4_v_ignored] =
                color_preset.ToAhsv();
            (void)int4_a;
            (void)int4_s_ignored;
            (void)int4_v_ignored;
            ptr_hue_slider->UpdateValue(fp4_h);
          };

      ptr_preset_area->AddChild(std::move(up_swatch));
    }
  }

  for (int int4_j = 0; int4_j < int4_palette_custom_rows; int4_j++) {
    for (int int4_i = 0; int4_i < int4_palette_cols; int4_i++) {
      const int int4_color_index = int4_j * int4_palette_cols + int4_i;

      std::unique_ptr<Widget> up_swatch = ptr_custom_color_template->Clone();
      ColorBlockWidget* ptr_swatch =
          static_cast<ColorBlockWidget*>(up_swatch.get());
      const auto up_get_color =
          std::make_shared<core::CachedTransform<core::Color, core::Color>>(
              [this, ptr_world](core::Color color_custom) {
                return server::ColorMakeValid(
                    data_manager_, color_custom, ptr_world->LocalRandom(), {},
                    {}, nullptr);
              });

      ptr_swatch->fn_get_color = [up_get_color, int4_color_index] {
        const std::vector<core::Color> vec_custom =
            Ui::Instance().CustomColorsDeps();
        return up_get_color->Update(vec_custom[int4_color_index]);
      };
      ptr_swatch->fn_is_visible = [int4_color_index] {
        return Ui::Instance().CustomColorsDeps().size() >
               static_cast<std::size_t>(int4_color_index);
      };
      ptr_swatch->bounds_widget.int4_x =
          int4_i * ptr_swatch->bounds_widget.int4_width;
      ptr_swatch->bounds_widget.int4_y =
          int4_j * ptr_swatch->bounds_widget.int4_height;
      ptr_swatch->fn_on_mouse_up =
          [ptr_swatch, ptr_mixer, ptr_hue_slider](const MouseInput&) {
            const core::Color color_custom = ptr_swatch->fn_get_color();
            ptr_mixer->Set(color_custom);
            const auto [int4_a, fp4_h, int4_s_ignored, int4_v_ignored] =
                color_custom.ToAhsv();
            (void)int4_a;
            (void)int4_s_ignored;
            (void)int4_v_ignored;
            ptr_hue_slider->UpdateValue(fp4_h);
          };

      ptr_custom_area->AddChild(std::move(up_swatch));
    }
  }

  ButtonWidget* ptr_store_button = ptr_widget->Get<ButtonWidget>("STORE_BUTTON");
  ptr_store_button->fn_on_click = [this, ptr_mixer, int4_palette_custom_rows,
                                   int4_palette_cols]() {
    const core::Color color_mixer = ptr_mixer->ColorValue();
    std::vector<core::Color> vec_custom = Ui::Instance().CustomColorsDeps();
    std::erase(vec_custom, color_mixer);
    vec_custom.push_back(color_mixer);
    const std::size_t sz_keep =
        static_cast<std::size_t>(int4_palette_custom_rows * int4_palette_cols);
    if (vec_custom.size() > sz_keep)
      vec_custom.erase(vec_custom.begin(),
                       vec_custom.end() - static_cast<std::ptrdiff_t>(sz_keep));
    Ui::Instance().StoreCustomColorsDeps(vec_custom);

    if (!b_palette_tab_opened_last)
      int4_palette_tab_highlighted = 4;
  };

  fn_extra_logic(ptr_widget);
}

void ColorPickerLogic::Tick() {
  if (int4_palette_tab_highlighted > 0)
    int4_palette_tab_highlighted--;
}

}  // namespace ora::ui
