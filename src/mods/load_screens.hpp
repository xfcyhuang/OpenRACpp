// UPSTREAM: OpenRA.Mods.Common/LoadScreens/LogoStripeLoadScreen.cs @1f594e320
#pragma once
import std;

#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "game/load_screen.hpp"
#include "gfx/sprite.hpp"
#include "net/launch_arguments.hpp"

namespace ora::fs {
class FileSystem;
}
namespace ora::game {
class Game;
class Manifest;
class ModData;
}
namespace ora::gfx {
class Renderer;
class Sheet;
}

namespace ora::mods {

struct LoadScreenDeps {
  game::Game* ptr_game = nullptr;
  gfx::Renderer* ptr_renderer = nullptr;
  std::function<bool()> fn_save_settings = [] { return true; };
  std::function<void(const std::string&, const core::Arguments&)>
      fn_switch_mod = [](const std::string&, const core::Arguments&) {};
};

class BlankLoadScreen : public game::ILoadScreen {
 public:
  explicit BlankLoadScreen(LoadScreenDeps deps_load)
      : deps_load_(std::move(deps_load)) {}
  ~BlankLoadScreen() override = default;

  void Init(const game::Manifest& manifest,
            const fs::FileSystem& file_system) override;
  void Display() override;
  bool BeforeLoad(game::ModData& mod_data) override;
  void StartGame(const core::Arguments& args) override;

  net::LaunchArguments launch{};

 protected:
  const fs::FileSystem* ptr_file_system_ = nullptr;
  bool b_initialized_ = false;
  LoadScreenDeps deps_load_{};
};

class SheetLoadScreen : public BlankLoadScreen {
 public:
  using BlankLoadScreen::BlankLoadScreen;
  ~SheetLoadScreen() override = default;

  void Init(const game::Manifest& manifest,
            const fs::FileSystem& file_system) override;
  void Display() override;

  virtual void DisplayInner(gfx::Renderer& r_renderer, gfx::Sheet* ptr_sheet,
                            int int4_density) = 0;

 protected:
  std::map<std::string, std::string> map_info_{};
  float fp4_dpi_scale_ = 1.0f;
  std::unique_ptr<gfx::Sheet> up_sheet_;
  int int4_density_ = 1;
  std::optional<std::chrono::steady_clock::time_point> opt_last_update_{};
};

class LogoStripeLoadScreen final : public SheetLoadScreen {
 public:
  using SheetLoadScreen::SheetLoadScreen;

  void Init(const game::Manifest& manifest,
            const fs::FileSystem& file_system) override;
  void DisplayInner(gfx::Renderer& r_renderer, gfx::Sheet* ptr_sheet,
                    int int4_density) override;

 private:
  Rectangle rect_stripe_{};
  core::Vector2 vec_logo_pos_{};
  gfx::Sprite sprite_stripe_{};
  gfx::Sprite sprite_logo_{};
  gfx::Sheet* ptr_last_sheet_ = nullptr;
  int int4_last_density_ = 0;
  int2 int2_last_resolution_{};
  std::vector<std::string> vec_messages_{};
};

std::unique_ptr<game::ILoadScreen> CreateLoadScreen(const std::string& str_name,
                                                    LoadScreenDeps deps_load);

}  // namespace ora::mods
