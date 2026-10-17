// UPSTREAM: OpenRA.Mods.Common/Widgets/VideoPlayerWidget.cs @e180e3225
//          L22-313 的实现面
//          The implementation face of L22-313.
#include "mods/video_player_widget.hpp"


#include "formats/video.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "meta/field_loader.hpp"
#include "sound/sound.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"

namespace ora::mods {

using gfx::operator+;
using gfx::operator-;
using gfx::operator*;

namespace {

std::int64_t ElapsedMsOf(
    const std::chrono::steady_clock::time_point& time_start, bool b_running,
    std::int64_t int8_accumulated) {
  if (!b_running)
    return int8_accumulated;
  return int8_accumulated +
         std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - time_start)
             .count();
}

}  // namespace

VideoPlayerWidget::VideoPlayerWidget(ora::ui::WidgetArgs&) {
  ptr_shared_cached_name_ = std::make_shared<std::string>();
  ptr_lifetime_ = std::make_shared<int>();
}

VideoPlayerWidget::VideoPlayerWidget(const VideoPlayerWidget& widget_other)
    : ora::ui::Widget(widget_other),
      hk_cancel_key(widget_other.hk_cancel_key),
      fp4_aspect_ratio(widget_other.fp4_aspect_ratio),
      b_draw_overlay(widget_other.b_draw_overlay),
      b_skippable(widget_other.b_skippable) {
  ptr_shared_cached_name_ = std::make_shared<std::string>();
  ptr_lifetime_ = std::make_shared<int>();
}

bool VideoPlayerWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "CancelKey") {
    ui::Hotkey hk_parsed{};
    if (ui::Hotkey::TryParse(str_value, hk_parsed))
      hk_cancel_key = hk_parsed;
    return true;
  }
  if (str_key == "AspectRatio") {
    fp4_aspect_ratio = std::stof(std::string{str_value});
    return true;
  }
  if (str_key == "DrawOverlay") {
    b_draw_overlay = str_value == "True";
    return true;
  }
  if (str_key == "Skippable") {
    b_skippable = str_value == "True";
    return true;
  }
  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void VideoPlayerWidget::LoadAndPlay(const std::string& str_filename) {
  if (str_filename == str_cached_video_filename_)
    return;

  str_cached_video_filename_ = str_filename;
  *ptr_shared_cached_name_ = str_filename;
  if (auto fn_load = ui::Ui::Instance().LoadVideoDeps())
    Play(std::shared_ptr<ora::fmt::IVideo>{fn_load(str_filename, true).release()});
  else
    Play(nullptr);
}

void VideoPlayerWidget::LoadAndPlayAsync(
    const std::string& str_filename, std::function<void()> fn_after) {
  if (str_filename == str_cached_video_filename_)
    return;

  str_cached_video_filename_ = str_filename;
  *ptr_shared_cached_name_ = str_filename;

  if (sp_video_ != nullptr)
    CloseVideo();

  auto fn_load = ui::Ui::Instance().LoadVideoDeps();
  if (!fn_load) {
    fn_after();
    return;
  }

  auto ptr_guard = ptr_shared_cached_name_;
  std::weak_ptr<int> wp_lifetime = ptr_lifetime_;
  std::thread([this, ptr_guard, wp_lifetime, str_filename, fn_after,
               fn_load]() mutable {
    std::shared_ptr<ora::fmt::IVideo> sp_video;
    try {
      sp_video.reset(fn_load(str_filename, true).release());
    } catch (...) {
      fn_after();
      return;
    }

    if (*ptr_guard != str_filename) {
      fn_after();
      return;
    }

    ui::Ui::Instance().RunAfterTickDefer(
        [this, ptr_guard, wp_lifetime, str_filename, fn_after, sp_video]() {
          if (wp_lifetime.expired()) {
            fn_after();
            return;
          }

          if (*ptr_guard != str_filename) {
            fn_after();
            return;
          }

          Play(sp_video);
          PlayThen([fn_after]() {
            fn_after();
          });
          CloseVideo();
        });
  }).detach();
}

void VideoPlayerWidget::Play(std::shared_ptr<ora::fmt::IVideo> sp_video) {
  sp_video_ = std::move(sp_video);

  if (sp_video_ == nullptr)
    return;
  ora::fmt::IVideo* ptr_video = sp_video_.get();

  int8_play_elapsed_ms_ = 0;
  b_play_time_running_ = false;
  b_play_time_ever_started_ = false;
  if (sound::Sound* ptr_sound = ui::Ui::Instance().SoundDeps())
    ptr_sound->StopVideo();
  fn_on_complete_ = [] {};

  fp4_inv_length_ =
      ptr_video->Framerate() * 1.0f / ptr_video->FrameCount();

  int4_texture_width_ = gfx::NextPowerOf2(ptr_video->Width());
  int4_texture_height_ = gfx::NextPowerOf2(ptr_video->Height());
  ptr_video_sheet_ = std::make_unique<gfx::Sheet>(
      gfx::SheetType::BGRA, int2{int4_texture_width_, int4_texture_height_});

  // 无头路径(Utility/测试)无纹理面:仅保留几何与帧数据面
  // The headless path (Utility/tests) has no texture face: only the
  // geometry and frame-data faces stay.
  if (ui::Ui::Instance().RendererDeps() != nullptr) {
    ptr_video_sheet_->GetTexture().SetScaleFilter(
        gfx::TextureScaleFilter::Linear);
    ptr_video_sheet_->GetTexture().SetData(
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(
                ptr_video->CurrentFrameData().data()),
            ptr_video->CurrentFrameData().size()),
        int4_texture_width_, int4_texture_height_);
  }

  sprite_video_ =
      gfx::Sprite(*ptr_video_sheet_,
                  Rectangle{0, 0, ptr_video->Width(), ptr_video->Height()},
                  gfx::TextureChannel::RGBA);

  const float fp4_scale =
      std::min(static_cast<float>(RenderBounds().Width) / ptr_video->Width(),
               RenderBounds().Height /
                   (ptr_video->Height() * fp4_aspect_ratio));
  vec_video_origin_ = core::Vector2{
      RenderBounds().X + (RenderBounds().Width - fp4_scale * ptr_video->Width()) / 2.0f,
      RenderBounds().Y +
          (RenderBounds().Height -
           fp4_scale * ptr_video->Height() * fp4_aspect_ratio) /
              2.0f};

  vec_video_size_ = core::Vector2{
      static_cast<float>(static_cast<int>(
          std::ceil(ptr_video->Width() * fp4_scale))),
      static_cast<float>(static_cast<int>(
          std::ceil(ptr_video->Height() * fp4_aspect_ratio * fp4_scale)))};
}

void VideoPlayerWidget::Draw() {
  if (video() == nullptr)
    return;

  if (!Paused()) {
    sound::Sound* ptr_sound = ui::Ui::Instance().SoundDeps();
    int int4_next_frame = 0;
    if (ptr_sound != nullptr && video()->HasAudio() &&
        !ptr_sound->DummyEngine())
      int4_next_frame = static_cast<int>(
          gfx::Lerp(0.0f, static_cast<float>(video()->FrameCount()),
                    ptr_sound->VideoSeekPosition() * fp4_inv_length_));
    else
      int4_next_frame = static_cast<int>(
          gfx::Lerp(0.0f, static_cast<float>(video()->FrameCount()),
                    ElapsedMsOf(time_play_start_, b_play_time_running_,
                                int8_play_elapsed_ms_) / 1000.0f *
                        fp4_inv_length_));

    if (int4_next_frame > video()->FrameCount() ||
        int4_next_frame < video()->CurrentFrameIndex()) {
      Stop();
      return;
    }

    int int4_skipped_frames = 0;
    while (int4_next_frame > video()->CurrentFrameIndex()) {
      video()->AdvanceFrame();
      int4_skipped_frames++;
    }

    if (int4_skipped_frames > 0) {
      const auto vec_frame = video()->CurrentFrameData();
      ptr_video_sheet_->GetTexture().SetData(
          std::span<const std::byte>(
              reinterpret_cast<const std::byte*>(vec_frame.data()),
              vec_frame.size()),
          int4_texture_width_, int4_texture_height_);
    }
  }

  ui::DrawSprite(sprite_video_, vec_video_origin_,
                              vec_video_size_);

  if (!b_draw_overlay)
    return;

  gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps();
  if (ptr_renderer == nullptr)
    return;

  const float fp4_scale = ptr_renderer->EffectiveWindowScale();
  if (ptr_overlay_sheet_ == nullptr || fp4_overlay_scale_ != fp4_scale) {
    ptr_overlay_sheet_.reset();

    const float fp4_video_scale =
        std::min(static_cast<float>(RenderBounds().Width) /
                     video()->Width(),
                 RenderBounds().Height /
                     (video()->Height() * fp4_aspect_ratio));
    const int int4_half_row_height =
        static_cast<int>(fp4_video_scale * fp4_scale / 2 + 0.5f);

    if (int4_half_row_height == 0) {
      b_draw_overlay = false;
      return;
    }

    const int int4_overlay_height = static_cast<int>(
        RenderBounds().Height * fp4_scale / int4_half_row_height);
    const int int4_sheet_height = gfx::NextPowerOf2(int4_overlay_height);
    std::vector<std::byte> vec_overlay(4 * int4_sheet_height);
    for (int i = 3; i < 4 * int4_overlay_height; i += 8)
      vec_overlay[i] = std::byte{128};

    ptr_overlay_sheet_ = std::make_unique<gfx::Sheet>(
        gfx::SheetType::BGRA, int2{1, int4_sheet_height});
    ptr_overlay_sheet_->GetTexture().SetData(
        std::span<const std::byte>(vec_overlay.data(),
                                   vec_overlay.size()),
        1, int4_sheet_height);
    sprite_overlay_ =
        gfx::Sprite(*ptr_overlay_sheet_,
                    Rectangle{0, 0, 1, int4_overlay_height},
                    gfx::TextureChannel::RGBA);

    const int int4_ox =
        static_cast<int>(RenderBounds().X * fp4_scale + 0.5f);
    const int int4_oy =
        static_cast<int>(RenderBounds().Y * fp4_scale + 0.5f);
    vec_overlay_origin_ =
        core::Vector2{static_cast<float>(int4_ox) / fp4_scale,
                      static_cast<float>(int4_oy) / fp4_scale};
    vec_overlay_size_ = core::Vector2{
        static_cast<float>(RenderBounds().Width),
        int4_overlay_height * int4_half_row_height / fp4_scale};
    fp4_overlay_scale_ = fp4_scale;
  }

  ui::DrawSprite(sprite_overlay_, vec_overlay_origin_,
                              vec_overlay_size_);
}

bool VideoPlayerWidget::HandleKeyPress(const ora::KeyInput& e_input) {
  if (!(ui::Hotkey::FromKeyInput(e_input) == hk_cancel_key) ||
      e_input.Event != KeyInputEvent::Down || !b_skippable)
    return false;

  Stop();
  return true;
}

bool VideoPlayerWidget::HandleMouseInput(const ora::MouseInput& mi_input) {
  return RenderBounds().Contains(mi_input.Location) && b_skippable;
}

void VideoPlayerWidget::PlayThen(std::function<void()> fn_after) {
  if (video() == nullptr)
    return;

  fn_on_complete_ = std::move(fn_after);
  if (sound::Sound* ptr_sound = ui::Ui::Instance().SoundDeps()) {
    if (!b_play_time_ever_started_ && video()->HasAudio())
      ptr_sound->PlayVideo(
          std::span<const std::uint8_t>(
              reinterpret_cast<const std::uint8_t*>(
                  video()->AudioData().data()),
              video()->AudioData().size()),
          video()->AudioChannels(), video()->SampleBits(),
          video()->SampleRate());
    else
      ptr_sound->PlayVideo();
  }

  time_play_start_ = std::chrono::steady_clock::now();
  b_play_time_running_ = true;
  b_play_time_ever_started_ = true;
}

void VideoPlayerWidget::Pause() {
  if (Paused() || video() == nullptr)
    return;

  int8_play_elapsed_ms_ =
      ElapsedMsOf(time_play_start_, b_play_time_running_,
                  int8_play_elapsed_ms_);
  b_play_time_running_ = false;
  if (sound::Sound* ptr_sound = ui::Ui::Instance().SoundDeps())
    ptr_sound->PauseVideo();
}

void VideoPlayerWidget::Stop() {
  if (video() == nullptr)
    return;

  int8_play_elapsed_ms_ = 0;
  b_play_time_running_ = false;
  b_play_time_ever_started_ = false;
  if (sound::Sound* ptr_sound = ui::Ui::Instance().SoundDeps())
    ptr_sound->StopVideo();
  video()->Reset();
  if (ui::Ui::Instance().RendererDeps() != nullptr) {
    const auto vec_frame = video()->CurrentFrameData();
    ptr_video_sheet_->GetTexture().SetData(
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(vec_frame.data()),
            vec_frame.size()),
        int4_texture_width_, int4_texture_height_);
  }
  ui::Ui::Instance().RunAfterTickDefer([this]() {
    if (fn_on_complete_) {
      auto fn = fn_on_complete_;
      fn_on_complete_ = nullptr;
      fn();
    }
  });
}

void VideoPlayerWidget::CloseVideo() {
  Stop();
  sp_video_.reset();
}

void VideoPlayerWidget::Removed() {
  ptr_video_sheet_.reset();
  ptr_overlay_sheet_.reset();
}

void RegisterVideoPlayerWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "VideoPlayerWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<VideoPlayerWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
