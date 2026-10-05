// UPSTREAM: OpenRA.Game/Graphics/ChromeProvider.cs @b6fc03f L22-305(全文逐语义)
// UI 皮肤(chrome.yaml):Collection(Image/Image2x/Image3x + PanelRegion 八元 +
// PanelSides 标志 + Regions 名→矩形表)。HiDPI 三档(dpiScale > 2 → 3x,> 1 →
// 2x,缺省原图);sheet 按图片名共享缓存,Sprite 按 density 缩放(1/density 的
// scale + density × 矩形)。面板九宫格:PanelRegion 存在时按 3×3 切角/边/心
// (PanelSides 缺失侧 → null 槽),否则九个具名 region 查询("corner-tl"…);
// 均无定义 = 空数组(上游 PERF 注释照抄)。
// 形态适配:
//   - 上游 static 类 + Game.ModData 全局 → 实例类(测试/多实例可注入;Phase 5
//     的 Ui.Initialize 持单例 —— 形态等价,COVERAGE 登记);
//   - FrozenDictionary/Dictionary → 插入序 vector&lt;pair&gt;(错误文本与查询语
//     义不变);
//   - FieldLoader.Load&lt;Collection&gt; → meta 描述表(ORA_FIELD;Rectangle/Size
//     解析随本批 meta 补齐);
//   - 上游 Log.Write("debug") 的三条诊断 → stderr(形态偏离,D96 族)。
// The UI skin (chrome.yaml): Collection (Image/Image2x/Image3x + the
// eight-element PanelRegion + the PanelSides flags + the Regions name →
// rectangle table). Three HiDPI tiers (dpiScale > 2 → 3x, > 1 → 2x, else the
// base image); sheets share a per-image cache, Sprites scale by density
// (a 1/density scale + density × the rectangle). The panel nine-slice: with a
// PanelRegion, the 3×3 corners/edges/center (missing PanelSides → null
// slots); otherwise nine named-region queries ("corner-tl"...); with no
// definitions at all, the empty array (the upstream PERF comment kept).
// Shape adaptations:
//   - upstream's static class + the Game.ModData global → an instance class
//     (injectable for tests/multiple instances; Phase 5's Ui.Initialize holds
//     the singleton — shape-equivalent, registered in COVERAGE);
//   - FrozenDictionary/Dictionary → insertion-ordered vector<pair> (the error
//     texts and query semantics unchanged);
//   - FieldLoader.Load<Collection> → the meta descriptor tables (ORA_FIELD;
//     Rectangle/Size parsing landed with this batch's meta additions);
//   - the three Log.Write("debug") diagnostics → stderr (a shape deviation,
//     the D96 family).
#pragma once
import std;

#include "core/rectangle.hpp"
#include "fs/file_system.hpp"
#include "gfx/sheet.hpp"
#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

/// PanelSides(ChromeProvider.cs L22-33;[Flags])。
/// PanelSides (ChromeProvider.cs L22-33; [Flags]).
enum class PanelSides : std::int32_t {
  Left = 1,
  Top = 2,
  Right = 4,
  Bottom = 8,
  Center = 16,

  Edges = Left | Top | Right | Bottom,
  All = Edges | Center,
};

/// PanelSidesExts.HasSide(L35-41):(self & m) == m。
/// PanelSidesExts.HasSide (L35-41): (self & m) == m.
constexpr bool HasSide(PanelSides kind_self, PanelSides kind_m) {
  return (static_cast<std::int32_t>(kind_self) & static_cast<std::int32_t>(kind_m)) ==
         static_cast<std::int32_t>(kind_m);
}

/// C# [Flags] 的 | 运算(九宫格构造用)。
/// The | arithmetic of C# [Flags] (for the nine-slice construction).
constexpr PanelSides operator|(PanelSides kind_a, PanelSides kind_b) {
  return static_cast<PanelSides>(static_cast<std::int32_t>(kind_a) |
                                 static_cast<std::int32_t>(kind_b));
}

/// ChromeProvider(ChromeProvider.cs L44-305;实例化形态见头注)。
/// ChromeProvider (ChromeProvider.cs L44-305; the instance shape per the
/// header note).
class ChromeProvider {
 public:
  /// Collection(L46-55;FieldLoader 按 yaml 键加载)。
  /// Collection (L46-55; loaded by FieldLoader per the yaml keys).
  class Collection final : public meta::RecordObject {
   public:
    std::string str_image;    // Image(可空 = C# null)| Image (nullable)
    std::string str_image2x;  // Image2x
    std::string str_image3x;  // Image3x
    std::vector<std::int32_t> vec_panel_region;  // PanelRegion(空 = null)
    std::int32_t int4_panel_sides = static_cast<std::int32_t>(PanelSides::All);  // PanelSides
    std::vector<std::pair<std::string, Rectangle>> vec_regions;                  // Regions(插入序)

    const meta::RecordDesc& record_desc() const override;

    static constexpr std::string_view kTypeName = "OpenRA.Game.Graphics.ChromeProvider.Collection";
  };

  /// Initialize 的依赖(上游 Game.Renderer/ModData 的注入等价)。
  /// The Initialize dependencies (the injected equivalents of upstream's
  /// Game.Renderer/ModData).
  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    const std::vector<std::string>* vec_chrome_files = nullptr;  // Manifest.Chrome
    float fp4_dpi_scale = 1.0f;                                  // Game.Renderer.WindowScale
    RenderThread* ptr_render = nullptr;
  };

  /// Initialize(L67-89):Merge 全部 chrome.yaml → 逐集合加载(^ 前缀跳过)。
  /// Initialize (L67-89): merges every chrome.yaml → loads each collection
  /// (^-prefixed keys skip).
  void Initialize(Deps deps);

  /// Deinitialize(L91-102):sheet 释放 + 表清空。
  /// Deinitialize (L91-102): releases the sheets + clears the tables.
  void Deinitialize();

  /// Collections(L57)。
  const std::vector<std::pair<std::string, std::unique_ptr<Collection>>>& Collections() const {
    return vec_collections_;
  }

  /// GetImage(L147-154):未命中抛 ArgumentException 文本逐字。
  /// GetImage (L147-154): a miss throws the ArgumentException text
  /// verbatim.
  Sprite GetImage(const std::string& str_collection_name, const std::string& str_image_name);

  /// TryGetImage(L156-183)。
  Sprite TryGetImage(const std::string& str_collection_name, const std::string& str_image_name);

  /// GetPanelImages(L185-192)。
  std::vector<std::optional<Sprite>> GetPanelImages(const std::string& str_collection_name);

  /// TryGetPanelImages(L194-264)。
  std::vector<std::optional<Sprite>> TryGetPanelImages(const std::string& str_collection_name);

  /// GetMinimumPanelSize(L266-285)。
  int2 GetMinimumPanelSize(const std::string& str_collection_name);

  /// SetDPIScale(L287-303):sprite 缓存清空,sheet 保留(上游内存取舍注释照抄)。
  /// SetDPIScale (L287-303): clears the sprite caches, keeps the sheets (the
  /// upstream memory-tradeoff comment verbatim).
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
  std::vector<std::pair<std::string, std::unique_ptr<Sheet>>> vec_cached_sheets_;  // 图名 → sheet
  std::vector<std::tuple<std::string, std::string, Sprite>> vec_cached_sprites_;  // (集合,图,Sprite)
  std::vector<std::pair<std::string, std::vector<std::optional<Sprite>>>> vec_cached_panel_sprites_;
  std::vector<std::pair<Collection*, SheetDensity>> vec_cached_collection_sheets_;

  yaml::StringPool pool_yaml_;
};

}  // namespace ora::gfx
