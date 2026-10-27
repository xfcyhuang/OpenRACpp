#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "gfx/sheet.hpp"
#include "ui/chrome_metrics.hpp"

namespace ora::gfx {
class SpriteFont;
class PaletteReference;
class Renderer;
class ChromeProvider;
}

namespace ora::game {
class ModData;
}

namespace ora::net {
class TextNotification;
enum class ClientState : int;
}

namespace ora::sim {
class Player;
enum class WinState : std::int32_t;
}

namespace ora::ui {

class LabelWithTooltipWidget;

class ButtonWidget;
class ImageWidget;
class Widget;

ChromeMetrics& RequireChromeMetrics();
game::ModData* ModDataOrNull();
gfx::Renderer& RequireRenderer();
gfx::ChromeProvider* ChromeProviderOrNull();

gfx::SpriteFont& RequireWidgetFont(const std::string& str_font);

std::string GetStatefulImageName(std::string_view str_base_name, bool b_disabled = false,
                                 bool b_pressed = false, bool b_hover = false,
                                 bool b_focused = false);

using StatefulImageKey = std::tuple<bool, bool, bool, bool, bool>;
using StatefulImageCache = core::CachedTransform<StatefulImageKey, gfx::Sprite>;
using StatefulPanelCache = core::CachedTransform<StatefulImageKey, std::vector<std::optional<gfx::Sprite>>>;

StatefulImageCache GetCachedStatefulImage(std::string str_collection, std::string str_image_name);

StatefulPanelCache GetCachedStatefulPanelImages(std::string str_collection);

void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos);
void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, int2 int2_size);
void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, core::Vector2 vec_size);

void DrawSpriteCentered(const gfx::Sprite& sprite_s, const gfx::PaletteReference* ptr_palette,
                        core::Vector2 vec_pos, float fp4_scale = 1.0f);

void DrawPanel(std::string_view str_collection, Rectangle rect_bounds);

void FillRectWithSprite(Rectangle rect_r, const gfx::Sprite& sprite_s);

void FillRectWithColor(Rectangle rect_r, core::Color color_c);

void FillRectWithColor(Rectangle rect_r, core::Color color_top_left, core::Color color_top_right,
                       core::Color color_bottom_right, core::Color color_bottom_left);

void FillEllipseWithColor(Rectangle rect_r, core::Color color_c);

Rectangle InflateBy(const Rectangle& rect_input, int int4_l, int int4_t, int int4_r, int int4_b);

void DrawPanel(Rectangle rect_bounds, const std::vector<std::optional<gfx::Sprite>>& vec_sprites);

std::string FormatTime(int int4_ticks, int int4_timestep);
std::string FormatTime(int int4_ticks, bool b_leading_minute_zero, int int4_timestep);
std::string FormatTimeSeconds(int int4_seconds);
std::string FormatTimeSeconds(int int4_seconds, bool b_leading_minute_zero);

std::string WrapText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text);

std::string TruncateText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text);

void TruncateLabelToTooltip(LabelWithTooltipWidget& label_target,
                            const std::string& str_text);

void TruncateButtonToTooltip(ButtonWidget& button_target, const std::string& str_text);

void BindButtonIcon(ButtonWidget& button_target);

std::string FormatCurrent(std::string_view str_format, double fp8_value);

void SetupTextNotification(Widget& widget_notification,
                           const net::TextNotification& notification,
                           int int4_box_width, bool b_with_timestamp);

std::string WithSuffix(const std::string& str_name, sim::WinState win_state,
                       net::ClientState client_state);

void BindPlayerNameAndStatus(LabelWithTooltipWidget& label_target,
                             sim::Player& player);

}
