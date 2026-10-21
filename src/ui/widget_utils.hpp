// UPSTREAM: OpenRA.Mods.Common/Widgets/WidgetUtils.cs @e180e3225 L20-352(基础
// 渲染/格式化子集;WithSuffix/BindPlayerNameAndStatus/SetupTextNotification
// 的会话与通知面板绑定面随其消费批)。Game.Renderer/ChromeProvider 静态 →
// Ui::Instance() 注入承载(D 注册);CachedTransform → core/cached_transform。
// The base rendering/formatting subset of WidgetUtils.cs L20-352 (the
// session and notification-panel binding faces — WithSuffix/
// BindPlayerNameAndStatus/SetupTextNotification — await their consumer
// batches). Game.Renderer/ChromeProvider statics become the Ui::Instance()
// injected carrier (registered); CachedTransform becomes
// core/cached_transform.
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
}  // namespace ora::gfx

namespace ora::game {
class ModData;
}  // namespace ora::game

namespace ora::net {
class TextNotification;
}  // namespace ora::net

namespace ora::ui {

class LabelWithTooltipWidget;

class ButtonWidget;
class ImageWidget;
class Widget;

/// widget 族的 Game.* 静态注入访问(widget_utils.cpp 定义;未注入面抛上游
/// KeyNotFoundException 等价/回落 null)
/// The widget family's Game.* injected access (defined in widget_utils.cpp;
/// the uninjected faces throw the KeyNotFoundException equivalent / fall
/// back to null).
ChromeMetrics& RequireChromeMetrics();
game::ModData* ModDataOrNull();
gfx::Renderer& RequireRenderer();
gfx::ChromeProvider* ChromeProviderOrNull();

/// Game.Renderer.Fonts[name] 的 widget 族统一访问(Label/Button/TextField
/// /SimpleTooltip 的 RequireFont 合流;缺名抛 ArgumentException 文本逐字;
/// 生产 = ptr_renderer,测试回落 = Ui Deps fn_try_get_font)
/// The widget family's unified Game.Renderer.Fonts[name] access (the merged
/// RequireFont of Label/Button/TextField/SimpleTooltip; a miss throws the
/// ArgumentException text verbatim; production = ptr_renderer, the test
/// fallback = Ui Deps fn_try_get_font).
gfx::SpriteFont& RequireWidgetFont(const std::string& str_font);

/// 状态图名后缀(WidgetUtils.cs L33-42)。
/// The stateful image-name suffix (WidgetUtils.cs L33-42).
std::string GetStatefulImageName(std::string_view str_base_name, bool b_disabled = false,
                                 bool b_pressed = false, bool b_hover = false,
                                 bool b_focused = false);

using StatefulImageKey = std::tuple<bool, bool, bool, bool, bool>;
using StatefulImageCache = core::CachedTransform<StatefulImageKey, gfx::Sprite>;
using StatefulPanelCache = core::CachedTransform<StatefulImageKey, std::vector<std::optional<gfx::Sprite>>>;

/// GetCachedStatefulImage(L44-56)。
/// GetCachedStatefulImage (L44-56).
StatefulImageCache GetCachedStatefulImage(std::string str_collection, std::string str_image_name);

/// GetCachedStatefulPanelImages(L59-70)。
/// GetCachedStatefulPanelImages (L59-70).
StatefulPanelCache GetCachedStatefulPanelImages(std::string str_collection);

/// DrawSprite(L72-91 三形态)。
/// DrawSprite (L72-91, the three shapes).
void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos);
void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, int2 int2_size);
void DrawSprite(const gfx::Sprite& sprite_s, core::Vector2 vec_pos, core::Vector2 vec_size);

/// DrawSpriteCentered(L93-96;调色板路径)。
/// DrawSpriteCentered (L93-96; the palette path).
void DrawSpriteCentered(const gfx::Sprite& sprite_s, const gfx::PaletteReference* ptr_palette,
                        core::Vector2 vec_pos, float fp4_scale = 1.0f);

/// DrawPanel(collection, bounds)(L98-103)。
/// DrawPanel(collection, bounds) (L98-103).
void DrawPanel(std::string_view str_collection, Rectangle rect_bounds);

/// FillRectWithSprite(L105-131)。
/// FillRectWithSprite (L105-131).
void FillRectWithSprite(Rectangle rect_r, const gfx::Sprite& sprite_s);

/// FillRectWithColor(L133-139)。
/// FillRectWithColor (L133-139).
void FillRectWithColor(Rectangle rect_r, core::Color color_c);

/// FillRectWithColor 四角色(L141-152)。
/// The four-color FillRectWithColor (L141-152).
void FillRectWithColor(Rectangle rect_r, core::Color color_top_left, core::Color color_top_right,
                       core::Color color_bottom_right, core::Color color_bottom_left);

/// FillEllipseWithColor(L154-159)。
/// FillEllipseWithColor (L154-159).
void FillEllipseWithColor(Rectangle rect_r, core::Color color_c);

/// InflateBy(L161-165)。
/// InflateBy (L161-165).
Rectangle InflateBy(const Rectangle& rect_input, int int4_l, int int4_t, int int4_r, int int4_b);

/// 九宫格面板(L176-243;sprites 顺序 TL,T,TR,L,C,R,BL,B,BR;非 9 = no-op)。
/// The nine-slice panel (L176-243; the sprites ordered TL,T,TR,L,C,R,BL,B,BR;
/// a non-9 array is a no-op).
void DrawPanel(Rectangle rect_bounds, const std::vector<std::optional<gfx::Sprite>>& vec_sprites);

/// FormatTime 族(L245-270)。
/// The FormatTime family (L245-270).
std::string FormatTime(int int4_ticks, int int4_timestep);
std::string FormatTime(int int4_ticks, bool b_leading_minute_zero, int int4_timestep);
std::string FormatTimeSeconds(int int4_seconds);
std::string FormatTimeSeconds(int int4_seconds, bool b_leading_minute_zero);

/// WrapText(L272-306):逐词前扫;超宽整行原样。
/// WrapText (L272-306): the forward word scan; an over-wide line stays put.
std::string WrapText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text);

/// TruncateText(L308-322)。
/// TruncateText (L308-322).
std::string TruncateText(const std::string& str_text, int int4_width, gfx::SpriteFont& font_text);

/// TruncateLabelToTooltip(L301-311)。
/// TruncateLabelToTooltip (L301-311).
void TruncateLabelToTooltip(LabelWithTooltipWidget& label_target,
                            const std::string& str_text);

/// TruncateButtonToTooltip(L330-339)。
/// TruncateButtonToTooltip (L330-339).
void TruncateButtonToTooltip(ButtonWidget& button_target, const std::string& str_text);

/// BindButtonIcon(L341-347)。
/// BindButtonIcon (L341-347).
void BindButtonIcon(ButtonWidget& button_target);

/// FormatCurrent(Exts.cs L44-46;消费域 = 图表 widget 的 "{0}" 单占位值
/// 模板;数值走最短十进制表示 —— 复合格式说明域随其消费批)。
/// FormatCurrent (Exts.cs L44-46; the consumer domain = the graph widgets'
/// "{0}" single-placeholder value templates; numbers take the shortest
/// decimal form — the composite-format-specifier domain rides its consumer
/// batch).
std::string FormatCurrent(std::string_view str_format, double fp8_value);

void SetupTextNotification(Widget& widget_notification,
                           const net::TextNotification& notification,
                           int int4_box_width, bool b_with_timestamp);

}  // namespace ora::ui
