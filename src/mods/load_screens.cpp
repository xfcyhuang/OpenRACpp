import std;

#include "mods/load_screens.hpp"

#include "fluent/fluent_provider.hpp"
#include "fs/file_system.hpp"
#include "game/game.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "game/settings.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/viewport.hpp"
#include "mods/browser_logics.hpp"
#include "net/replay_recorder.hpp"
#include "platform/sdl2_input.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::mods {

namespace {

std::optional<std::string> ExtractGameInfoMod(const std::string& str_game_info) {
  const std::string_view sv{str_game_info};
  const std::size_t pos = sv.find("Mod:");
  if (pos == std::string_view::npos)
    return std::nullopt;
  std::size_t cursor = pos + std::string_view{"Mod:"}.size();
  while (cursor < sv.size() && (sv[cursor] == ' ' || sv[cursor] == '\t'))
    ++cursor;
  std::size_t end = cursor;
  while (end < sv.size() && sv[end] != ' ' && sv[end] != '\t' && sv[end] != '\r' &&
         sv[end] != '\n')
    ++end;
  if (end == cursor)
    return std::nullopt;
  return std::string{sv.substr(cursor, end - cursor)};
}

std::vector<std::uint8_t> ReadAllBytes(const std::string& str_path) {
  std::ifstream stream_file{str_path, std::ios::binary};
  if (!stream_file)
    return {};
  return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{stream_file},
                                   std::istreambuf_iterator<char>{}};
}

}

void BlankLoadScreen::Init(const game::Manifest&,
                           const fs::FileSystem& file_system) {
  ptr_file_system_ = &file_system;
}

void BlankLoadScreen::Display() {
  if (deps_load_.ptr_renderer == nullptr || b_initialized_)
    return;

  deps_load_.ptr_renderer->BeginUI();
  NullInputHandler handler_null{};
  deps_load_.ptr_renderer->EndFrame(&handler_null);

  b_initialized_ = true;
}

bool BlankLoadScreen::BeforeLoad(game::ModData&) {
  game::SettingsModule* ptr_graphics =
      game::Settings::Instance().Module("Graphics");
  if (deps_load_.ptr_renderer == nullptr || ptr_graphics == nullptr)
    return true;

  const int2 int2_min =
      gfx::GlobalViewportSizes().int2_min_effective_resolution;
  const int2 int2_resolution = deps_load_.ptr_renderer->Resolution();

  if (const std::optional<std::size_t> opt_slot =
          ptr_graphics->SlotIndexOf("UIScale");
      opt_slot.has_value() && ptr_graphics->GetFloat(*opt_slot) > 1.0f &&
      (int2_resolution.X < int2_min.X || int2_resolution.Y < int2_min.Y)) {
    ptr_graphics->SetFloat(*opt_slot, 1.0f);
    deps_load_.ptr_renderer->SetUIScale(1.0f);
  }

  if (const std::optional<std::size_t> opt_slot =
          ptr_graphics->SlotIndexOf("VideoDisplay");
      opt_slot.has_value())
    ptr_graphics->SetInt(*opt_slot, deps_load_.ptr_renderer->CurrentDisplay());

  if (const std::optional<std::size_t> opt_slot =
          ptr_graphics->SlotIndexOf("GLProfile");
      opt_slot.has_value()) {
    const std::int64_t int8_saved = ptr_graphics->GetInt(*opt_slot);
    const std::int64_t int8_running =
        deps_load_.ptr_renderer->window().GLProfileUsed() ==
                platform::GLProfileKind::Embedded
            ? 3
            : 2;
    if (int8_saved != 0 && int8_saved != int8_running)
      ptr_graphics->SetInt(*opt_slot, 0);
  }

  return true;
}

void BlankLoadScreen::StartGame(const core::Arguments& args) {
  launch = net::LaunchArguments{args};

  ui::Ui::Instance().ResetAll();
  (void)deps_load_.fn_save_settings();

  if (!launch.str_benchmark.empty())
    std::println("Saving benchmark data into Logs");

  if (const std::optional<net::ConnectionTarget> opt_connect =
          launch.GetConnectEndPoint();
      opt_connect.has_value()) {
    deps_load_.ptr_game->LoadShellMap();
    deps_load_.ptr_game->RemoteDirectConnect(*opt_connect);
    return;
  }

  if (!launch.str_map.empty()) {
    deps_load_.ptr_game->LoadMap(launch.str_map);
    return;
  }

  if (!launch.str_replay.empty()) {
    std::unique_ptr<net::ReplayMetadata> up_meta =
        net::ReplayMetadata::ReadFile(launch.str_replay);
    bool b_compat = false;
    if (up_meta != nullptr && deps_load_.ptr_game->ModDataFace() != nullptr)
      b_compat = PromptConfirmReplayCompatibility(
          *up_meta, *deps_load_.ptr_game->ModDataFace(),
          [ptr_game = deps_load_.ptr_game] { ptr_game->LoadShellMap(); });
    if (b_compat)
      deps_load_.ptr_game->JoinReplay(ReadAllBytes(launch.str_replay));
    if (up_meta != nullptr && deps_load_.ptr_game->ModDataFace() != nullptr) {
      if (const std::optional<std::string> opt_mod =
              ExtractGameInfoMod(up_meta->GameInfoData());
          opt_mod.has_value() &&
          *opt_mod !=
              deps_load_.ptr_game->ModDataFace()->ManifestRef().Id())
        deps_load_.fn_switch_mod(*opt_mod, args);
    }    return;
  }

  deps_load_.ptr_game->LoadShellMap();
  (void)deps_load_.fn_save_settings();
}

void SheetLoadScreen::Init(const game::Manifest& manifest,
                           const fs::FileSystem& file_system) {
  BlankLoadScreen::Init(manifest, file_system);

  if (const yaml::MiniYaml* yaml_node = manifest.MergedNode("LoadScreen"))
    for (const yaml::MiniYamlNode& node : yaml_node->Nodes)
      if (node.Value.Value != nullptr)
        map_info_[node.Key != nullptr ? *node.Key : std::string{}] =
            *node.Value.Value;
}

void SheetLoadScreen::Display() {
  if (deps_load_.ptr_renderer == nullptr)
    return;
  const auto tp_now = std::chrono::steady_clock::now();
  if (opt_last_update_.has_value() &&
      std::chrono::duration<double>(tp_now - *opt_last_update_).count() < 0.2)
    return;
  if (!opt_last_update_.has_value())
    opt_last_update_ = tp_now;

  const float fp4_scale = deps_load_.ptr_renderer->EffectiveWindowScale();
  if (fp4_dpi_scale_ != fp4_scale) {
    fp4_dpi_scale_ = fp4_scale;
    up_sheet_.reset();
  }

  if (up_sheet_ == nullptr && map_info_.contains("Image") &&
      ptr_file_system_ != nullptr) {
    std::string str_image = map_info_.at("Image");
    int4_density_ = 1;
    if (fp4_dpi_scale_ > 2 && map_info_.contains("Image3x")) {
      str_image = map_info_.at("Image3x");
      int4_density_ = 3;
    } else if (fp4_dpi_scale_ > 1 && map_info_.contains("Image2x")) {
      str_image = map_info_.at("Image2x");
      int4_density_ = 2;
    }

    const std::vector<char> vec_png = ptr_file_system_->Open(str_image);
    up_sheet_ = std::make_unique<gfx::Sheet>(
        gfx::SheetType::BGRA,
        std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(vec_png.data()),
            vec_png.size()},
        &deps_load_.ptr_renderer->render());
    if (up_sheet_ != nullptr)
      up_sheet_->GetTexture().SetScaleFilter(
          gfx::TextureScaleFilter::Linear);
  }

  deps_load_.ptr_renderer->BeginUI();
  DisplayInner(*deps_load_.ptr_renderer, up_sheet_.get(), int4_density_);
  NullInputHandler handler_null{};
  deps_load_.ptr_renderer->EndFrame(&handler_null);

  *opt_last_update_ = std::chrono::steady_clock::now();
}

void LogoStripeLoadScreen::Init(const game::Manifest& manifest,
                                const fs::FileSystem& file_system) {
  SheetLoadScreen::Init(manifest, file_system);

  std::string str_loading =
      fluent::FluentProvider::Instance().GetMessage("loadscreen-loading");
  const std::string_view sv{str_loading};
  std::size_t pos_prev = 0;
  while (pos_prev <= sv.size()) {
    const std::size_t pos_comma = sv.find(',', pos_prev);
    const std::string_view sv_token =
        pos_comma == std::string_view::npos ? sv.substr(pos_prev)
                                            : sv.substr(pos_prev, pos_comma - pos_prev);
    std::size_t pos_begin = 0;
    std::size_t pos_end = sv_token.size();
    while (pos_begin < pos_end &&
           (sv_token[pos_begin] == ' ' || sv_token[pos_begin] == '\t'))
      ++pos_begin;
    while (pos_end > pos_begin &&
           (sv_token[pos_end - 1] == ' ' || sv_token[pos_end - 1] == '\t'))
      --pos_end;
    if (pos_end > pos_begin)
      vec_messages_.emplace_back(sv_token.substr(pos_begin, pos_end - pos_begin));
    if (pos_comma == std::string_view::npos)
      break;
    pos_prev = pos_comma + 1;
  }
}

void LogoStripeLoadScreen::DisplayInner(gfx::Renderer& r_renderer,
                                        gfx::Sheet* ptr_sheet,
                                        int int4_density) {
  if (ptr_sheet != ptr_last_sheet_ || int4_density != int4_last_density_) {
    ptr_last_sheet_ = ptr_sheet;
    int4_last_density_ = int4_density;
    if (ptr_sheet != nullptr) {
      sprite_logo_ = gfx::Sprite(
          *ptr_sheet,
          Rectangle{0, 0, 256 * int4_density, 256 * int4_density},
          gfx::TextureChannel::RGBA, 1.0f / int4_density);
      sprite_stripe_ = gfx::Sprite(
          *ptr_sheet,
          Rectangle{258 * int4_density, 0, 253 * int4_density, 256 * int4_density},
          gfx::TextureChannel::RGBA, 1.0f / int4_density);
    }
  }

  if (r_renderer.Resolution() != int2_last_resolution_) {
    int2_last_resolution_ = r_renderer.Resolution();
    rect_stripe_ = Rectangle{0, int2_last_resolution_.Y / 2 - 128,
                             int2_last_resolution_.X, 256};
    vec_logo_pos_ = core::Vector2{
        static_cast<float>(int2_last_resolution_.X / 2 - 128),
        static_cast<float>(int2_last_resolution_.Y / 2 - 128)};
  }

  if (ptr_sheet != nullptr)
    ui::FillRectWithSprite(rect_stripe_, sprite_stripe_);

  if (ptr_sheet != nullptr)
    r_renderer.UIRgbaSpriteRenderer().DrawSprite(
        sprite_logo_, core::Vector3{vec_logo_pos_.X, vec_logo_pos_.Y, 0.0f});

  if (gfx::SpriteFont* ptr_font = r_renderer.TryGetFont("Bold");
      ptr_font != nullptr && !vec_messages_.empty() &&
      deps_load_.ptr_game != nullptr) {
    const std::int32_t int4_index = deps_load_.ptr_game->CosmeticRandom().Next(
        0, static_cast<std::int32_t>(vec_messages_.size()));
    if (int4_index >= 0) {
      const std::string& str_text =
          vec_messages_[static_cast<std::size_t>(int4_index)];
      const int2 int2_size = ptr_font->Measure(str_text);
      ptr_font->DrawText(
          str_text,
          core::Vector2{
              static_cast<float>(int2_last_resolution_.X - int2_size.X - 20),
              static_cast<float>(int2_last_resolution_.Y - int2_size.Y - 20)},
          core::Color::FromArgb(255, 255, 255));
    }
  }
}

std::unique_ptr<game::ILoadScreen> CreateLoadScreen(
    const std::string& str_name, LoadScreenDeps deps_load) {
  if (str_name == "BlankLoadScreen")
    return std::make_unique<BlankLoadScreen>(std::move(deps_load));
  if (str_name == "LogoStripeLoadScreen")
    return std::make_unique<LogoStripeLoadScreen>(std::move(deps_load));
  throw std::runtime_error("Unknown LoadScreen type: " + str_name);
}

}
