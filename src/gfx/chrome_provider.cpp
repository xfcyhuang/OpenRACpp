// UPSTREAM: OpenRA.Game/Graphics/ChromeProvider.cs @b6fc03f L22-305
//          (chrome_provider.hpp 的实现;头注的形态适配说明适用)
//          Implementation of chrome_provider.hpp; the shape-adaptation notes
//          of the hpp header apply.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "gfx/chrome_provider.hpp"
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::gfx {

// ———— Collection 的 FieldLoader 描述表 ————

namespace {

/// 上游字段子集(C# 字段名 = yaml 键):Image/Image2x/Image3x(string,可空=
/// 缺省)/PanelRegion(ImmutableArray&lt;int&gt;)/PanelSides(枚举)/Regions(
/// FrozenDictionary&lt;string, Rectangle&gt;)。
/// The upstream field subset (C# field names = yaml keys): Image/Image2x/
/// Image3x (string, nullable = default), PanelRegion (ImmutableArray<int>),
/// PanelSides (enum), Regions (FrozenDictionary<string, Rectangle>).
constexpr meta::FieldDesc kElem_Int32 = meta::ElemOf(meta::FieldType::Int32, "System.Int32");
constexpr meta::FieldDesc kElem_String = meta::ElemOf(meta::FieldType::String, "System.String");
constexpr meta::FieldDesc kElem_Rectangle =
    meta::ElemOf(meta::FieldType::Rectangle, "OpenRA.Primitives.Rectangle");

constexpr std::array<meta::FieldDesc, 6> kFields_Collection{{
    {.str_name = "Image", .type = meta::FieldType::String, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection, str_image),
     .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Image2x", .type = meta::FieldType::String, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection, str_image2x),
     .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Image3x", .type = meta::FieldType::String, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection, str_image3x),
     .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "PanelRegion", .type = meta::FieldType::ImmutableArray, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection,
                                                       vec_panel_region),
     .elem = &kElem_Int32, .key = nullptr, .value = nullptr,
     .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.Int32]"},
    {.str_name = "PanelSides", .type = meta::FieldType::Enum, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection,
                                                       int4_panel_sides),
     .elem = nullptr, .key = nullptr, .value = nullptr,
     .str_type_name = "OpenRA.Game.Graphics.PanelSides"},
    {.str_name = "Regions", .type = meta::FieldType::FrozenDictionary, .b_required = false,
     .str_loader = {}, .off_offset = __builtin_offsetof(ChromeProvider::Collection, vec_regions),
     .elem = nullptr, .key = &kElem_String, .value = &kElem_Rectangle,
     .str_type_name = "System.Collections.Frozen.FrozenDictionary`2[System.String,"
                      "OpenRA.Primitives.Rectangle]"},
}};

constexpr meta::RecordDesc kDesc_Collection{
    .str_name = "Collection",
    .str_full_name = "OpenRA.Game.Graphics.ChromeProvider.Collection",
    .str_base = {},
    .fields = kFields_Collection,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

/// PanelSides 的枚举注册(名字 → 值;Enum.TryParse ignoreCase 的名字域)。
/// The PanelSides enum registration (names → values; the name domain of
/// Enum.TryParse ignoreCase).
const bool kPanelSidesRegistered = [] {
  constexpr std::array<meta::EnumMemberDesc, 8> kMembers{{
      {.str_name = "Left", .int4_value = 1},
      {.str_name = "Top", .int4_value = 2},
      {.str_name = "Right", .int4_value = 4},
      {.str_name = "Bottom", .int4_value = 8},
      {.str_name = "Center", .int4_value = 16},
      {.str_name = "Edges", .int4_value = 1 | 2 | 4 | 8},
      {.str_name = "All", .int4_value = 1 | 2 | 4 | 8 | 16},
  }};
  meta::RegisterEnum("OpenRA.Game.Graphics.PanelSides", kMembers);
  return true;
}();

}  // namespace

const meta::RecordDesc& ChromeProvider::Collection::record_desc() const {
  return kDesc_Collection;
}

// ———— Initialize / Deinitialize ————

void ChromeProvider::Initialize(Deps deps) {
  Deinitialize();

  // HiDPI 显示器上加载更高分辨率图(上游注释)| Load higher resolution
  // images if available on HiDPI displays (the upstream comment).
  fp4_dpi_scale_ = deps.fp4_dpi_scale;
  ptr_file_system_ = deps.ptr_file_system;
  ptr_render_ = deps.ptr_render;

  // 单池贯穿(上游 stringPool:YAML 公共串复用)
  // One pool throughout (upstream's stringPool: reusing common YAML
  // strings).
  std::vector<std::vector<yaml::MiniYamlNode>> vec_sources;
  vec_sources.reserve(deps.vec_chrome_files->size());
  for (const std::string& str_file : *deps.vec_chrome_files) {
    const std::vector<char> vec_bytes = ptr_file_system_->Open(str_file);
    vec_sources.push_back(yaml::MiniYaml::FromStream(
        std::string_view{vec_bytes.data(), vec_bytes.size()}, str_file, true, pool_yaml_));
  }

  const std::vector<yaml::MiniYamlNode> vec_chrome = yaml::MiniYaml::Merge(std::move(vec_sources));
  for (const yaml::MiniYamlNode& node_c : vec_chrome) {
    const std::string str_key = node_c.Key != nullptr ? *node_c.Key : std::string{};
    if (!str_key.starts_with('^'))
      vec_collections_.emplace_back(str_key, [&] {
        // LoadCollection(L104-109):FieldLoader.Load&lt;Collection&gt;(yaml)
        // LoadCollection (L104-109): FieldLoader.Load<Collection>(yaml).
        auto collection_next = std::make_unique<Collection>();
        meta::Load(collection_next.get(), node_c.Value);
        return collection_next;
      }());
  }
}

void ChromeProvider::Deinitialize() {
  for (auto& [str_name_ignored, ptr_sheet] : vec_cached_sheets_)
    if (ptr_sheet != nullptr)
      ptr_sheet.reset();

  vec_collections_.clear();
  vec_cached_sheets_.clear();
  vec_cached_sprites_.clear();
  vec_cached_panel_sprites_.clear();
  vec_cached_collection_sheets_.clear();
}

ChromeProvider::Collection* ChromeProvider::FindCollection(const std::string& str_name) {
  for (auto& [str_key, collection_c] : vec_collections_)
    if (str_key == str_name)
      return collection_c.get();
  return nullptr;
}

// ———— sheet/DPI 面 ————

ChromeProvider::SheetDensity ChromeProvider::SheetForCollection(Collection& collection_c) {
  // 外层缓存免重算图名 | The outer cache avoids recalculating image names.
  for (auto& [ptr_cached, density_cached] : vec_cached_collection_sheets_)
    if (ptr_cached == &collection_c)
      return density_cached;

  std::string str_image = collection_c.str_image;
  std::int32_t int4_density = 1;
  if (fp4_dpi_scale_ > 2 && !collection_c.str_image3x.empty()) {
    str_image = collection_c.str_image3x;
    int4_density = 3;
  } else if (fp4_dpi_scale_ > 1 && !collection_c.str_image2x.empty()) {
    str_image = collection_c.str_image2x;
    int4_density = 2;
  }

  // 内层缓存:集合间共享 sheet | The inner cache shares sheets between
  // collections.
  SheetDensity density_sheet{nullptr, 1};
  for (auto& [str_key, ptr_sheet] : vec_cached_sheets_) {
    if (str_key == str_image) {
      density_sheet = SheetDensity{ptr_sheet.get(), int4_density};
      break;
    }
  }

  if (density_sheet.ptr_sheet == nullptr) {
    const std::vector<char> vec_png = ptr_file_system_->Open(str_image);
    auto ptr_sheet = std::make_unique<Sheet>(
        SheetType::BGRA,
        std::span<const std::byte>{reinterpret_cast<const std::byte*>(vec_png.data()),
                                   vec_png.size()},
        ptr_render_);
    // 线性过滤(上游);数据面(无渲染线程)无纹理可设
    // Linear filtering (upstream); the data-only face (no render thread)
    // has no texture to configure.
    if (ptr_render_ != nullptr)
      ptr_sheet->GetTexture().SetScaleFilter(TextureScaleFilter::Linear);

    density_sheet = SheetDensity{ptr_sheet.get(), int4_density};
    vec_cached_sheets_.emplace_back(str_image, std::move(ptr_sheet));
  }

  vec_cached_collection_sheets_.emplace_back(&collection_c, density_sheet);
  return density_sheet;
}

Sprite ChromeProvider::GetImage(const std::string& str_collection_name,
                                const std::string& str_image_name) {
  const Sprite sprite_image = TryGetImage(str_collection_name, str_image_name);
  if (sprite_image.ptr_sheet == nullptr)
    throw std::runtime_error(
        std::format("Sprite `{}/{}` was not found.", str_collection_name, str_image_name));

  return sprite_image;
}

Sprite ChromeProvider::TryGetImage(const std::string& str_collection_name,
                                   const std::string& str_image_name) {
  if (str_collection_name.empty())
    return Sprite{};

  // 缓存命中 | The cached sprite.
  for (const auto& [str_collection_cached, str_image_cached, sprite_cached] : vec_cached_sprites_)
    if (str_collection_cached == str_collection_name && str_image_cached == str_image_name)
      return sprite_cached;

  Collection* ptr_collection = FindCollection(str_collection_name);
  if (ptr_collection == nullptr)
    return Sprite{};

  const Rectangle* ptr_mi = nullptr;
  for (const auto& [str_region_key, rect_region] : ptr_collection->vec_regions)
    if (str_region_key == str_image_name) {
      ptr_mi = &rect_region;
      break;
    }
  if (ptr_mi == nullptr)
    return Sprite{};

  // 缓存 sprite | Cache the sprite.
  const SheetDensity density_sheet = SheetForCollection(*ptr_collection);
  const Rectangle rect_dense = density_sheet.int4_density * (*ptr_mi);
  Sprite sprite_image{*density_sheet.ptr_sheet, rect_dense, TextureChannel::RGBA,
                      1.0f / static_cast<float>(density_sheet.int4_density)};
  vec_cached_sprites_.emplace_back(str_collection_name, str_image_name, sprite_image);

  return sprite_image;
}

std::vector<std::optional<Sprite>> ChromeProvider::GetPanelImages(
    const std::string& str_collection_name) {
  auto vec_panel = TryGetPanelImages(str_collection_name);
  if (vec_panel.empty())
    throw std::runtime_error(std::format("Panel `{}` was not found.", str_collection_name));

  return vec_panel;
}

std::vector<std::optional<Sprite>> ChromeProvider::TryGetPanelImages(
    const std::string& str_collection_name) {
  if (str_collection_name.empty())
    return {};

  // 缓存命中 | The cached sprites.
  for (const auto& [str_key, vec_cached] : vec_cached_panel_sprites_)
    if (str_key == str_collection_name)
      return vec_cached;

  Collection* ptr_collection = FindCollection(str_collection_name);
  if (ptr_collection == nullptr)
    return {};

  std::vector<std::optional<Sprite>> vec_sprites;
  if (!ptr_collection->vec_panel_region.empty()) {
    if (ptr_collection->vec_panel_region.size() != 8) {
      std::println(stderr, "Collection '{}' does not define a valid PanelRegion",
                   str_collection_name);
      return {};
    }

    // 缓存 sprites | Cache the sprites.
    const SheetDensity density_sheet = SheetForCollection(*ptr_collection);
    const std::vector<std::int32_t>& vec_pr = ptr_collection->vec_panel_region;
    const PanelSides kind_ps = static_cast<PanelSides>(ptr_collection->int4_panel_sides);

    const auto make_sprite = [&](PanelSides kind_sides, Rectangle rect_bounds)
        -> std::optional<Sprite> {
      if (!HasSide(kind_ps, kind_sides))
        return std::nullopt;
      return Sprite{*density_sheet.ptr_sheet, density_sheet.int4_density * rect_bounds,
                    TextureChannel::RGBA, 1.0f / static_cast<float>(density_sheet.int4_density)};
    };

    vec_sprites = {
        make_sprite(PanelSides::Top | PanelSides::Left,
                    Rectangle{vec_pr[0], vec_pr[1], vec_pr[2], vec_pr[3]}),
        make_sprite(PanelSides::Top, Rectangle{vec_pr[0] + vec_pr[2], vec_pr[1], vec_pr[4], vec_pr[3]}),
        make_sprite(PanelSides::Top | PanelSides::Right,
                    Rectangle{vec_pr[0] + vec_pr[2] + vec_pr[4], vec_pr[1], vec_pr[6], vec_pr[3]}),
        make_sprite(PanelSides::Left,
                    Rectangle{vec_pr[0], vec_pr[1] + vec_pr[3], vec_pr[2], vec_pr[5]}),
        make_sprite(PanelSides::Center,
                    Rectangle{vec_pr[0] + vec_pr[2], vec_pr[1] + vec_pr[3], vec_pr[4], vec_pr[5]}),
        make_sprite(PanelSides::Right,
                    Rectangle{vec_pr[0] + vec_pr[2] + vec_pr[4], vec_pr[1] + vec_pr[3], vec_pr[6],
                              vec_pr[5]}),
        make_sprite(PanelSides::Bottom | PanelSides::Left,
                    Rectangle{vec_pr[0], vec_pr[1] + vec_pr[3] + vec_pr[5], vec_pr[2], vec_pr[7]}),
        make_sprite(PanelSides::Bottom,
                    Rectangle{vec_pr[0] + vec_pr[2], vec_pr[1] + vec_pr[3] + vec_pr[5], vec_pr[4],
                              vec_pr[7]}),
        make_sprite(PanelSides::Bottom | PanelSides::Right,
                    Rectangle{vec_pr[0] + vec_pr[2] + vec_pr[4], vec_pr[1] + vec_pr[3] + vec_pr[5],
                              vec_pr[6], vec_pr[7]}),
    };
  } else {
    // PERF: 无定义时不必搜图 | PERF: We don't need to search for images if
    // there are no definitions.
    // PERF: 空数组比 9 个 null 更省 | PERF: It's more efficient to send an
    // empty array rather than an array of 9 nulls.
    if (ptr_collection->vec_regions.empty())
      return {};

    // 非常规对话框布局的手工定义 | Support manual definitions for unusual
    // dialog layouts.
    const auto try_image = [&](const char* chr_name) -> std::optional<Sprite> {
      const Sprite sprite_try = TryGetImage(str_collection_name, chr_name);
      if (sprite_try.ptr_sheet == nullptr)
        return std::nullopt;
      return sprite_try;
    };
    vec_sprites = {
        try_image("corner-tl"), try_image("border-t"), try_image("corner-tr"),
        try_image("border-l"),  try_image("background"), try_image("border-r"),
        try_image("corner-bl"), try_image("border-b"), try_image("corner-br"),
    };
  }

  vec_cached_panel_sprites_.emplace_back(str_collection_name, vec_sprites);
  return vec_sprites;
}

int2 ChromeProvider::GetMinimumPanelSize(const std::string& str_collection_name) {
  if (str_collection_name.empty())
    return int2{0, 0};

  Collection* ptr_collection = FindCollection(str_collection_name);
  if (ptr_collection == nullptr) {
    std::println(stderr, "Could not find collection '{}'", str_collection_name);
    return int2{0, 0};
  }

  if (ptr_collection->vec_panel_region.size() != 8) {
    std::println(stderr, "Collection '{}' does not define a valid PanelRegion",
                 str_collection_name);
    return int2{0, 0};
  }

  const std::vector<std::int32_t>& vec_pr = ptr_collection->vec_panel_region;
  return int2{vec_pr[2] + vec_pr[6], vec_pr[3] + vec_pr[7]};
}

void ChromeProvider::SetDPIScale(float fp4_scale) {
  if (fp4_dpi_scale_ == fp4_scale)
    return;

  fp4_dpi_scale_ = fp4_scale;

  // 清 sprite 缓存以载入新档素材。sheet 不清:同一 sheet 多 DPI 驻留的内存
  // 开销优于全部释放重载(上游注释照抄 —— DPI 切换罕见,但一旦发生本会话
  // 再发生的概率不低)
  // Clear the sprite caches so the new artwork can be loaded. Sheets are
  // not cleared: we assume the extra memory overhead of the same sheet in
  // multiple DPIs beats disposing and reloading everything (upstream comment
  // verbatim — DPI changes are rare, but if one happens another this
  // session is reasonably likely).
  vec_cached_sprites_.clear();
  vec_cached_panel_sprites_.clear();
  vec_cached_collection_sheets_.clear();
}

}  // namespace ora::gfx
