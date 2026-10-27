#pragma once
import std;

#include "core/vector_n.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite.hpp"
#include "platform/sdl2_input.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::fmt {
struct IVideo;
}

namespace ora::mods {

class VideoPlayerWidget final : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "VideoPlayerWidget";

  VideoPlayerWidget(ora::ui::WidgetArgs& args_args);
  VideoPlayerWidget(const VideoPlayerWidget& widget_other);
  ~VideoPlayerWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new VideoPlayerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  void Draw() override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  const std::string* GetCursor(int2) const override { return nullptr; }
  void Removed() override;

  void LoadAndPlay(const std::string& str_filename);
  void LoadAndPlayAsync(const std::string& str_filename,
                        std::function<void()> fn_after);
  void Play(std::shared_ptr<ora::fmt::IVideo> sp_video);
  void PlayThen(std::function<void()> fn_after);
  void Play() { PlayThen([] {}); }
  void Pause();
  void Stop();
  void CloseVideo();

  bool Paused() const { return !b_play_time_running_; }
  ora::fmt::IVideo* Video() const { return sp_video_.get(); }

  ui::Hotkey hk_cancel_key{ora::Keycode::ESCAPE, ora::Modifiers::None};
  float fp4_aspect_ratio = 1.2f;
  bool b_draw_overlay = true;
  bool b_skippable = true;

  const core::Vector2& VideoSizeForTest() const { return vec_video_size_; }
  const core::Vector2& VideoOriginForTest() const { return vec_video_origin_; }

 private:
  std::shared_ptr<ora::fmt::IVideo> sp_video_;
  ora::fmt::IVideo* video() const { return sp_video_.get(); }
  gfx::Sprite sprite_video_;
  gfx::Sprite sprite_overlay_;
  std::unique_ptr<gfx::Sheet> ptr_overlay_sheet_;
  std::string str_cached_video_filename_;
  float fp4_inv_length_ = 0.0f;
  core::Vector2 vec_video_origin_;
  core::Vector2 vec_video_size_;
  core::Vector2 vec_overlay_origin_;
  core::Vector2 vec_overlay_size_;
  float fp4_overlay_scale_ = 0.0f;
  std::chrono::steady_clock::time_point time_play_start_;
  bool b_play_time_running_ = false;
  bool b_play_time_ever_started_ = false;
  std::int64_t int8_play_elapsed_ms_ = 0;
  int int4_texture_width_ = 0;
  int int4_texture_height_ = 0;
  std::unique_ptr<gfx::Sheet> ptr_video_sheet_;
  std::shared_ptr<std::string> ptr_shared_cached_name_;
  std::shared_ptr<int> ptr_lifetime_;
  std::function<void()> fn_on_complete_;
};

void RegisterVideoPlayerWidget();

}
