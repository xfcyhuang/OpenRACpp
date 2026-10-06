// UPSTREAM: OpenRA.Mods.Common/Graphics/DefaultSpriteSequence.cs @b6fc03f L24-605
//          (default_sprite_sequence.hpp 的实现)
//          Implementation of default_sprite_sequence.hpp.
// 形态偏离(COVERAGE 登记):resolved[f] 的越界 = 上游数组
// IndexOutOfRangeException,C++ 以同文本 runtime_error 等价抛;ParseSequences
// 包装异常的内层 {e} 为 C# ToString(),此处取 e.what()(信息等价,少类型前
// 缀);PerfTimer 计时面不落地(名字保留)。
// Registered shape deviations: resolved[f] out of range = upstream's array
// IndexOutOfRangeException, thrown here as a runtime_error with the same
// text; the inner {e} of ParseSequences' wrapper is C# ToString(), here
// e.what() (information-equal, minus the type prefix); the PerfTimer timing
// face is not ported (the name stays).
import std;
#include "mods/default_sprite_sequence.hpp"

namespace ora::mods {

// core::Vector3 的渲染运算在 ora::gfx(ADL 不达;显式引入)
// core::Vector3's rendering arithmetic lives in ora::gfx (beyond ADL;
// imported explicitly).
using gfx::operator+;

namespace {
constexpr std::array<std::int32_t, 1> kFirstFrame{0};
const yaml::MiniYaml& NoDataInstance() {
  static const yaml::MiniYaml yaml_no_data{static_cast<const std::string*>(nullptr), {}};
  return yaml_no_data;
}
}  // namespace

const yaml::MiniYaml& DefaultSpriteSequence::NoData() { return NoDataInstance(); }

// ———— DefaultSpriteSequenceLoader(L34-60)————
// ———— DefaultSpriteSequenceLoader (L34-60) ————

std::unique_ptr<gfx::ISpriteSequence> DefaultSpriteSequenceLoader::CreateSequence(
    gfx::SpriteCache& cache_sprites, const std::string& str_image, const std::string& str_sequence,
    const yaml::MiniYaml& yaml_data, const yaml::MiniYaml& yaml_defaults) {
  return std::make_unique<DefaultSpriteSequence>(cache_sprites, *this, str_image, str_sequence,
                                                 yaml_data, yaml_defaults);
}

std::vector<std::pair<std::string, std::unique_ptr<gfx::ISpriteSequence>>>
DefaultSpriteSequenceLoader::ParseSequences(gfx::SpriteCache& cache_sprites,
                                            const std::string& str_tile_set,
                                            const yaml::MiniYamlNode& node_image) {
  std::vector<std::pair<std::string, std::unique_ptr<gfx::ISpriteSequence>>> vec_sequences;

  const yaml::MiniYamlNode* node_defaults = node_image.Value.NodeWithKeyOrDefault("Defaults");
  const yaml::MiniYaml& yaml_defaults =
      node_defaults != nullptr ? node_defaults->Value : NoDataInstance();

  // 剥离 Defaults 节点后的序列节点表(上游 Nodes.Remove)
  // The sequence node table with the Defaults node removed (upstream's
  // Nodes.Remove).
  std::vector<yaml::MiniYamlNode> vec_nodes;
  vec_nodes.reserve(node_image.Value.Nodes.size());
  for (const yaml::MiniYamlNode& node : node_image.Value.Nodes)
    if (&node != node_defaults)
      vec_nodes.push_back(node);

  const std::string str_image = node_image.Key != nullptr ? *node_image.Key : std::string{};
  for (const yaml::MiniYamlNode& node_sequence : vec_nodes) {
    const std::string str_sequence =
        node_sequence.Key != nullptr ? *node_sequence.Key : std::string{};
    try {
      auto seq_sequence = CreateSequence(cache_sprites, str_image, str_sequence,
                                         node_sequence.Value, yaml_defaults);
      static_cast<DefaultSpriteSequence*>(seq_sequence.get())
          ->ReserveSprites(str_tile_set, node_sequence.Value, yaml_defaults, cache_sprites);
      vec_sequences.emplace_back(str_sequence, std::move(seq_sequence));
    } catch (const std::exception& e) {
      // Nodes[0] = 剥离 Defaults 后的首节点(上游照抄)
      // Nodes[0] = the first node after the Defaults strip (upstream kept
      // verbatim).
      throw std::runtime_error(std::format(
          "Failed to parse sequences for {}.{} at {}:\n{}", str_image, str_sequence,
          vec_nodes.empty() ? yaml::SourceLocation{}.ToString() : vec_nodes.front().Location.ToString(),
          e.what()));
    }
  }

  return vec_sequences;
}

// ———— LoadField 族(L245-270)————
// ———— The LoadField family (L245-270) ————

std::string DefaultSpriteSequence::LoadString(std::string_view sv_key, const std::string* str_fallback,
                                              const yaml::MiniYaml& yaml_data,
                                              const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return str_fallback != nullptr ? *str_fallback : std::string{};
  // GetValue&lt;string&gt;:null/"" 合流(field_loader.hpp 登记偏离)
  // GetValue<string>: null and "" conflate (the registered deviation in
  // field_loader.hpp).
  return node->Value.Value != nullptr ? *node->Value.Value : std::string{};
}

std::int32_t DefaultSpriteSequence::LoadInt32(std::string_view sv_key, std::int32_t int4_fallback,
                                              const yaml::MiniYaml& yaml_data,
                                              const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return int4_fallback;
  return meta::GetInt32Value(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

bool DefaultSpriteSequence::LoadBool(std::string_view sv_key, bool b_fallback,
                                     const yaml::MiniYaml& yaml_data,
                                     const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return b_fallback;
  return meta::GetBoolValue(sv_key,
                            node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                         : std::string_view{});
}

float DefaultSpriteSequence::LoadFloat(std::string_view sv_key, float fp4_fallback,
                                       const yaml::MiniYaml& yaml_data,
                                       const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return fp4_fallback;
  return meta::GetFloatValue(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

std::optional<std::int32_t> DefaultSpriteSequence::LoadNullableInt32(
    std::string_view sv_key, const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults,
    yaml::SourceLocation& location_out) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr) {
    location_out = yaml::SourceLocation{};
    return std::nullopt;
  }

  location_out = node->Location;
  return meta::GetInt32Value(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

std::int32_t DefaultSpriteSequence::LoadInt32Located(std::string_view sv_key,
                                                     std::int32_t int4_fallback,
                                                     const yaml::MiniYaml& yaml_data,
                                                     const yaml::MiniYaml* yaml_defaults,
                                                     yaml::SourceLocation& location_out) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr) {
    location_out = yaml::SourceLocation{};
    return int4_fallback;
  }

  location_out = node->Location;
  return meta::GetInt32Value(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

std::optional<std::vector<std::int32_t>> DefaultSpriteSequence::LoadFrames(
    std::string_view sv_key, const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return std::nullopt;  // 上游 ImmutableArray 默认 = null | upstream's default ImmutableArray = null
  return meta::GetInt32ArrayValue(
      sv_key,
      node->Value.Value != nullptr ? std::string_view{*node->Value.Value} : std::string_view{});
}

std::vector<float> DefaultSpriteSequence::LoadFloatArray(std::string_view sv_key,
                                                         const yaml::MiniYaml& yaml_data,
                                                         const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return {};
  return meta::GetFloatArrayValue(
      sv_key,
      node->Value.Value != nullptr ? std::string_view{*node->Value.Value} : std::string_view{});
}

core::Vector3 DefaultSpriteSequence::LoadVector3(std::string_view sv_key, core::Vector3 vec_fallback,
                                                 const yaml::MiniYaml& yaml_data,
                                                 const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return vec_fallback;
  return meta::GetVector3Value(sv_key,
                               node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                            : std::string_view{});
}

core::Vector2 DefaultSpriteSequence::LoadVector2(std::string_view sv_key, core::Vector2 vec_fallback,
                                                 const yaml::MiniYaml& yaml_data,
                                                 const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return vec_fallback;
  return meta::GetVector2Value(sv_key,
                               node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                            : std::string_view{});
}

core::Color DefaultSpriteSequence::LoadColor(std::string_view sv_key, core::Color color_fallback,
                                             const yaml::MiniYaml& yaml_data,
                                             const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return color_fallback;
  return meta::GetColorValue(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

gfx::BlendMode DefaultSpriteSequence::LoadBlendMode(std::string_view sv_key,
                                                    gfx::BlendMode kind_fallback,
                                                    const yaml::MiniYaml& yaml_data,
                                                    const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return kind_fallback;
  return static_cast<gfx::BlendMode>(meta::GetEnumValue(
      sv_key, node->Value.Value != nullptr ? std::string_view{*node->Value.Value} : std::string_view{},
      "OpenRA.BlendMode"));
}

std::vector<std::pair<std::string, std::string>> DefaultSpriteSequence::LoadStringDictionary(
    std::string_view sv_key, const yaml::MiniYaml& yaml_data, const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return {};
  return meta::GetStringDictionaryValue(node->Value, sv_key);
}

std::int32_t DefaultSpriteSequence::LoadWDist(std::string_view sv_key, std::int32_t int4_fallback,
                                              const yaml::MiniYaml& yaml_data,
                                              const yaml::MiniYaml* yaml_defaults) {
  const yaml::MiniYamlNode* node = FindFieldNode(sv_key, yaml_data, yaml_defaults);
  if (node == nullptr)
    return int4_fallback;
  return meta::GetWDistValue(sv_key,
                             node->Value.Value != nullptr ? std::string_view{*node->Value.Value}
                                                          : std::string_view{});
}

Rectangle DefaultSpriteSequence::FlipRectangle(Rectangle rect_in, bool b_flip_x, bool b_flip_y) {
  const std::int32_t int4_left = b_flip_x ? rect_in.Right() : rect_in.Left();
  const std::int32_t int4_top = b_flip_y ? rect_in.Bottom() : rect_in.Top();
  const std::int32_t int4_right = b_flip_x ? rect_in.Left() : rect_in.Right();
  const std::int32_t int4_bottom = b_flip_y ? rect_in.Top() : rect_in.Bottom();

  return Rectangle::FromLTRB(int4_left, int4_top, int4_right, int4_bottom);
}

std::optional<std::vector<std::int32_t>> DefaultSpriteSequence::CalculateFrameIndices(
    std::int32_t int4_start, const std::optional<std::int32_t>& opt_int4_length, std::int32_t int4_stride,
    std::int32_t int4_facings, const std::optional<std::vector<std::int32_t>>& opt_vec_frames,
    bool b_transpose, bool b_reverse_facings, std::int32_t int4_shadow_start) {
  // length == null = 请求全部帧(调用方语义)
  // length == null requests all frames (the caller's semantics).
  if (!opt_int4_length.has_value())
    return std::nullopt;

  // 只请求实际需要的帧子集 —— 上游注释逐句
  // Only request the subset of frames we actually need — the upstream
  // comment verbatim.
  std::vector<std::int32_t> vec_used_frames;
  for (std::int32_t int4_facing{}; int4_facing < int4_facings; int4_facing++) {
    const std::int32_t int4_facing_inner =
        b_reverse_facings ? (int4_facings - int4_facing) % int4_facings : int4_facing;
    for (std::int32_t int4_frame{}; int4_frame < *opt_int4_length; int4_frame++) {
      const std::int32_t int4_i = b_transpose ? int4_frame * int4_facings + int4_facing_inner
                                              : int4_facing_inner * int4_stride + int4_frame;

      vec_used_frames.push_back(opt_vec_frames.has_value()
                                    ? (*opt_vec_frames)[static_cast<std::size_t>(int4_i)]
                                    : int4_start + int4_i);
    }
  }

  if (int4_shadow_start >= 0) {
    const std::int32_t int4_shadow_offset = int4_shadow_start - int4_start;
    const std::size_t st_frame_count = vec_used_frames.size();
    for (std::size_t int4_i{}; int4_i < st_frame_count; int4_i++)
      vec_used_frames.push_back(vec_used_frames[int4_i] + int4_shadow_offset);
  }

  return vec_used_frames;
}

// ———— 构造(L346-412)————
// ———— Construction (L346-412) ————

DefaultSpriteSequence::DefaultSpriteSequence(gfx::SpriteCache& cache_sprites,
                                             gfx::ISpriteSequenceLoader& loader, std::string str_image,
                                             std::string str_sequence, const yaml::MiniYaml& yaml_data,
                                             const yaml::MiniYaml& yaml_defaults)
    : str_image_{std::move(str_image)}, str_name_{std::move(str_sequence)}, ptr_loader_{&loader} {
  int4_start_ = LoadInt32("Start", 0, yaml_data, &yaml_defaults);

  // Length 三态:键值 "*" → null(全帧);缺键 → 默认 1 —— 上游两段读的
  // 怪癖照抄(L355-357)
  // Length's three states: the value "*" → null (all frames); a missing key
  // → the default 1 — upstream's two-read quirk kept (L355-357).
  opt_int4_length_ = std::nullopt;
  yaml::SourceLocation location_length{};
  const yaml::MiniYamlNode* node_length = FindFieldNode("Length", yaml_data, &yaml_defaults);
  const bool b_length_star =
      node_length != nullptr && node_length->Value.Value != nullptr &&
      std::string_view{*node_length->Value.Value} == "*";
  if (!b_length_star)
    opt_int4_length_ =
        std::int32_t{LoadInt32Located("Length", 1, yaml_data, &yaml_defaults, location_length)};

  // Stride 的回退 = length(int? 可空传递;键在时覆盖)
  // Stride falls back to length (the nullable passes through; a present key
  // overrides).
  yaml::SourceLocation location_stride_ignored{};
  if (FindFieldNode("Stride", yaml_data, &yaml_defaults) != nullptr)
    opt_int4_stride_ =
        LoadNullableInt32("Stride", yaml_data, &yaml_defaults, location_stride_ignored);
  else
    opt_int4_stride_ = opt_int4_length_;

  yaml::SourceLocation location_facings{};
  int4_facings_ = LoadInt32Located("Facings", 1, yaml_data, &yaml_defaults, location_facings);
  yaml::SourceLocation location_interpolated{};
  opt_interpolated_facings_ =
      LoadNullableInt32("InterpolatedFacings", yaml_data, &yaml_defaults, location_interpolated);

  int4_tick_ = LoadInt32("Tick", 40, yaml_data, &yaml_defaults);
  int4_z_offset_ = LoadWDist("ZOffset", 0, yaml_data, &yaml_defaults);

  int4_shadow_start_ = LoadInt32("ShadowStart", -1, yaml_data, &yaml_defaults);
  int4_shadow_z_offset_ = LoadWDist("ShadowZOffset", -5, yaml_data, &yaml_defaults);

  b_ignore_world_tint_ = LoadBool("IgnoreWorldTint", false, yaml_data, &yaml_defaults);
  fp4_scale_ = LoadFloat("Scale", 1, yaml_data, &yaml_defaults);

  b_reverses_ = LoadBool("Reverses", false, yaml_data, &yaml_defaults);
  b_transpose_ = LoadBool("Transpose", false, yaml_data, &yaml_defaults);
  if (const yaml::MiniYamlNode* node_alpha = FindFieldNode("Alpha", yaml_data, &yaml_defaults))
    opt_vec_alpha_ = LoadFloatArray("Alpha", yaml_data, &yaml_defaults);
  yaml::SourceLocation location_alpha_fade{};
  if (const yaml::MiniYamlNode* node_alpha_fade =
          FindFieldNode("AlphaFade", yaml_data, &yaml_defaults))
    location_alpha_fade = node_alpha_fade->Location;
  b_alpha_fade_ = LoadBool("AlphaFade", false, yaml_data, &yaml_defaults);

  yaml::SourceLocation location_depth_sprite{};
  const std::string str_depth_sprite = LoadString("DepthSprite", nullptr, yaml_data, &yaml_defaults);
  if (!str_depth_sprite.empty()) {
    if (const yaml::MiniYamlNode* node_depth =
            FindFieldNode("DepthSprite", yaml_data, &yaml_defaults))
      location_depth_sprite = node_depth->Location;
    opt_int4_depth_sprite_reservation_ = cache_sprites.ReserveSprites(
        str_depth_sprite, std::vector<std::int32_t>{
                              LoadInt32("DepthSpriteFrame", 0, yaml_data, &yaml_defaults)},
        location_depth_sprite);
  }

  vec_depth_sprite_offset_ = LoadVector2("DepthSpriteOffset", core::Vector2{}, yaml_data,
                                         &yaml_defaults);

  if (int4_facings_ < 0) {
    b_reverse_facings_ = true;
    int4_facings_ = -int4_facings_;
  }

  // Facings 须为 1024 的整因子(即幂二)使帧均匀映射整圈 —— 上游注释逐句
  // Facings must be an integer factor of 1024 (i.e. a power of 2) for the
  // frames to map uniformly over the full rotation — the upstream comment
  // verbatim.
  if (int4_facings_ == 0 || int4_facings_ > 1024 || !gfx::IsPowerOf2(int4_facings_))
    throw yaml::YamlException(std::format(
        "{}: Facings must be within the (positive or negative) range of 1 to 1024, and a power of 2.",
        location_facings.ToString()));

  if (opt_interpolated_facings_.has_value() &&
      (*opt_interpolated_facings_ < 2 || *opt_interpolated_facings_ <= int4_facings_ ||
       *opt_interpolated_facings_ > 1024 ||
       !gfx::IsPowerOf2(*opt_interpolated_facings_)))
    throw yaml::YamlException(std::format(
        "{}: InterpolatedFacings must be greater than Facings, within the range of 2 to 1024, and a "
        "power of 2.",
        location_interpolated.ToString()));

  if (opt_int4_length_.has_value() && *opt_int4_length_ <= 0)
    throw yaml::YamlException(
        std::format("{}: Length must be positive.", location_length.ToString()));

  if (!opt_int4_length_.has_value() && int4_facings_ > 1)
    throw yaml::YamlException(std::format(
        "{}: Facings cannot be used with Length: *.", location_facings.ToString()));

  if (b_alpha_fade_ && opt_vec_alpha_.has_value())
    throw yaml::YamlException(std::format(
        "{}: AlphaFade cannot be used with Alpha.", location_alpha_fade.ToString()));
}

void DefaultSpriteSequence::ReserveSprites(const std::string& str_tile_set,
                                           const yaml::MiniYaml& yaml_data,
                                           const yaml::MiniYaml& yaml_defaults,
                                           gfx::SpriteCache& cache_sprites) {
  str_tile_set_ = str_tile_set;
  const std::optional<std::vector<std::int32_t>> opt_vec_frames =
      LoadFrames("Frames", yaml_data, &yaml_defaults);
  const bool b_flip_x = LoadBool("FlipX", false, yaml_data, &yaml_defaults);
  const bool b_flip_y = LoadBool("FlipY", false, yaml_data, &yaml_defaults);
  const std::int32_t int4_z_ramp = LoadInt32("ZRamp", 0, yaml_data, &yaml_defaults);
  const core::Vector3 vec_offset = LoadVector3("Offset", core::Vector3{}, yaml_data, &yaml_defaults);
  const gfx::BlendMode kind_blend_mode =
      LoadBlendMode("BlendMode", gfx::BlendMode::Alpha, yaml_data, &yaml_defaults);

  const yaml::MiniYamlNode* node_combine = yaml_data.NodeWithKeyOrDefault("Combine");
  if (node_combine != nullptr) {
    for (const yaml::MiniYamlNode& node_sub : node_combine->Value.Nodes) {
      const yaml::MiniYaml& yaml_sub_data = node_sub.Value;
      const core::Vector3 vec_sub_offset = LoadVector3("Offset", core::Vector3{}, yaml_sub_data,
                                                       &NoDataInstance());
      const bool b_sub_flip_x = LoadBool("FlipX", false, yaml_sub_data, &NoDataInstance());
      const bool b_sub_flip_y = LoadBool("FlipY", false, yaml_sub_data, &NoDataInstance());
      const std::optional<std::vector<std::int32_t>> opt_vec_sub_frames =
          LoadFrames("Frames", yaml_sub_data, nullptr);

      for (const ReservationInfo& info_f :
           ParseCombineFilenames(str_tile_set, opt_vec_sub_frames, yaml_sub_data)) {
        SpriteReservation reservation;
        reservation.int4_token = cache_sprites.ReserveSprites(info_f.str_filename,
                                                             info_f.opt_vec_load_frames,
                                                             info_f.location_source);
        reservation.vec_offset = vec_sub_offset + vec_offset;
        reservation.b_flip_x = b_sub_flip_x ^ b_flip_x;
        reservation.b_flip_y = b_sub_flip_y ^ b_flip_y;
        reservation.kind_blend_mode = kind_blend_mode;
        reservation.fp4_z_ramp = static_cast<float>(int4_z_ramp);
        reservation.opt_vec_frames = info_f.opt_vec_frames;
        vec_sprites_to_load_.push_back(std::move(reservation));
      }
    }
  } else {
    for (const ReservationInfo& info_f :
         ParseFilenames(str_tile_set, opt_vec_frames, yaml_data, yaml_defaults)) {
      SpriteReservation reservation;
      reservation.int4_token =
          cache_sprites.ReserveSprites(info_f.str_filename, info_f.opt_vec_load_frames,
                                       info_f.location_source);
      reservation.vec_offset = vec_offset;
      reservation.b_flip_x = b_flip_x;
      reservation.b_flip_y = b_flip_y;
      reservation.kind_blend_mode = kind_blend_mode;
      reservation.fp4_z_ramp = static_cast<float>(int4_z_ramp);
      reservation.opt_vec_frames = info_f.opt_vec_frames;
      vec_sprites_to_load_.push_back(std::move(reservation));
    }
  }
}

std::vector<DefaultSpriteSequence::ReservationInfo> DefaultSpriteSequence::ParseFilenames(
    const std::string& /*str_tile_set*/,
    const std::optional<std::vector<std::int32_t>>& opt_vec_frames, const yaml::MiniYaml& yaml_data,
    const yaml::MiniYaml& yaml_defaults) {
  std::vector<ReservationInfo> vec_infos;

  const yaml::MiniYamlNode* node_pattern =
      FindFieldNode("FilenamePattern", yaml_data, &yaml_defaults);
  if (node_pattern != nullptr && node_pattern->Value.Value != nullptr &&
      !node_pattern->Value.Value->empty()) {
    const std::int32_t int4_pattern_start = LoadInt32("Start", 0, node_pattern->Value, nullptr);
    const std::int32_t int4_pattern_count = LoadInt32("Count", 1, node_pattern->Value, nullptr);

    // FormatInvariant(i) = 不变文化十进制(pattern 为 "{0}" 形)
    // FormatInvariant(i) = the invariant-culture decimal (the pattern uses
    // the "{0}" form).
    for (std::int32_t int4_i = int4_pattern_start;
         int4_i < int4_pattern_start + int4_pattern_count; int4_i++) {
      ReservationInfo info;
      info.str_filename = std::vformat(*node_pattern->Value.Value,
                                       std::make_format_args(int4_i));
      info.opt_vec_load_frames = std::vector<std::int32_t>{kFirstFrame.begin(), kFirstFrame.end()};
      info.opt_vec_frames = std::vector<std::int32_t>{kFirstFrame.begin(), kFirstFrame.end()};
      info.location_source = node_pattern->Location;
      vec_infos.push_back(std::move(info));
    }
    return vec_infos;
  }

  yaml::SourceLocation location_filename{};
  const std::string str_filename =
      LoadString("Filename", nullptr, yaml_data, &yaml_defaults);

  // stride ?? length ?? 0 —— 两级可空回退
  // stride ?? length ?? 0 — the two-level nullable fallback.
  const std::int32_t int4_stride =
      opt_int4_stride_.has_value() ? *opt_int4_stride_
                                   : opt_int4_length_.value_or(0);
  const std::optional<std::vector<std::int32_t>> opt_vec_load_frames = CalculateFrameIndices(
      int4_start_, opt_int4_length_, int4_stride, int4_facings_, opt_vec_frames, b_transpose_,
      b_reverse_facings_, int4_shadow_start_);

  ReservationInfo info;
  info.str_filename = str_filename;
  info.opt_vec_load_frames = opt_vec_load_frames;
  info.opt_vec_frames = opt_vec_frames;
  if (const yaml::MiniYamlNode* node_filename = FindFieldNode("Filename", yaml_data, &yaml_defaults))
    info.location_source = node_filename->Location;
  vec_infos.push_back(std::move(info));
  return vec_infos;
}

std::vector<DefaultSpriteSequence::ReservationInfo> DefaultSpriteSequence::ParseCombineFilenames(
    const std::string& /*str_tile_set*/,
    const std::optional<std::vector<std::int32_t>>& opt_vec_frames, const yaml::MiniYaml& yaml_data) {
  std::vector<ReservationInfo> vec_infos;

  ReservationInfo info;
  info.str_filename = LoadString("Filename", nullptr, yaml_data, nullptr);
  if (const yaml::MiniYamlNode* node_filename = FindFieldNode("Filename", yaml_data, nullptr))
    info.location_source = node_filename->Location;

  std::optional<std::vector<std::int32_t>> opt_vec_frames_local = opt_vec_frames;
  // Length 值 "*" 时保持 null(全帧);键缺省走默认 1
  // The value "*" keeps null (all frames); a missing key takes the default 1.
  const yaml::MiniYamlNode* node_length = FindFieldNode("Length", yaml_data, nullptr);
  const bool b_length_star = node_length != nullptr && node_length->Value.Value != nullptr &&
                             std::string_view{*node_length->Value.Value} == "*";
  if (!opt_vec_frames_local.has_value() && !b_length_star) {
    const std::int32_t int4_sub_start = LoadInt32("Start", 0, yaml_data, nullptr);
    const std::int32_t int4_sub_length = LoadInt32("Length", 1, yaml_data, nullptr);
    std::vector<std::int32_t> vec_frames;
    for (std::int32_t int4_i{}; int4_i < int4_sub_length; int4_i++)
      vec_frames.push_back(int4_sub_start + int4_i);
    opt_vec_frames_local = std::move(vec_frames);
  }

  info.opt_vec_load_frames = opt_vec_frames_local;
  info.opt_vec_frames = opt_vec_frames_local;
  vec_infos.push_back(std::move(info));
  return vec_infos;
}

void DefaultSpriteSequence::ResolveSprites(gfx::SpriteCache& cache_sprites) {
  if (opt_rect_bounds_.has_value())
    return;

  gfx::Sprite sprite_depth{};
  if (opt_int4_depth_sprite_reservation_.has_value()) {
    // First(s != null):全空/未命中 → First 的等价抛
    // First(s != null): all-null/no hit → First's equivalent throw.
    const std::vector<gfx::Sprite> vec_depth =
        cache_sprites.ResolveSprites(*opt_int4_depth_sprite_reservation_);
    const auto it_hit = std::ranges::find_if(
        vec_depth, [](const gfx::Sprite& sprite_s) { return sprite_s.ptr_sheet != nullptr; });
    if (it_hit == vec_depth.end())
      throw std::runtime_error("Sequence contains no elements");
    sprite_depth = *it_hit;
  }

  std::vector<gfx::Sprite> vec_all_sprites;
  for (const SpriteReservation& reservation_r : vec_sprites_to_load_) {
    std::vector<gfx::Sprite> vec_resolved = cache_sprites.ResolveSprites(reservation_r.int4_token);
    if (reservation_r.opt_vec_frames.has_value()) {
      std::vector<gfx::Sprite> vec_reindexed;
      for (const std::int32_t int4_f : *reservation_r.opt_vec_frames) {
        // resolved[f] 的越界 = 上游数组索引越界(IndexOutOfRange)
        // An out-of-range resolved[f] = upstream's array index overflow
        // (IndexOutOfRange).
        if (int4_f < 0 || static_cast<std::size_t>(int4_f) >= vec_resolved.size())
          throw std::runtime_error("Index was outside the bounds of the array.");
        vec_reindexed.push_back(vec_resolved[static_cast<std::size_t>(int4_f)]);
      }
      vec_resolved = std::move(vec_reindexed);
    }

    for (const gfx::Sprite& sprite_s : vec_resolved) {
      if (sprite_s.ptr_sheet == nullptr) {
        vec_all_sprites.push_back(gfx::Sprite{});
        continue;
      }

      const float fp4_dx =
          reservation_r.vec_offset.X +
          (reservation_r.b_flip_x ? -sprite_s.vec_offset.X : sprite_s.vec_offset.X);
      const float fp4_dy =
          reservation_r.vec_offset.Y +
          (reservation_r.b_flip_y ? -sprite_s.vec_offset.Y : sprite_s.vec_offset.Y);
      const float fp4_dz = reservation_r.vec_offset.Z + sprite_s.vec_offset.Z +
                           reservation_r.fp4_z_ramp * fp4_dy;
      gfx::Sprite sprite_new{*sprite_s.ptr_sheet,
                             FlipRectangle(sprite_s.Bounds, reservation_r.b_flip_x,
                                           reservation_r.b_flip_y),
                             reservation_r.fp4_z_ramp,
                             core::Vector3{fp4_dx, fp4_dy, fp4_dz},
                             sprite_s.kind_channel, reservation_r.kind_blend_mode};
      if (sprite_depth.ptr_sheet == nullptr) {
        vec_all_sprites.push_back(std::move(sprite_new));
        continue;
      }

      const std::int32_t int4_cw = (sprite_depth.Bounds.Left() + sprite_depth.Bounds.Right()) / 2 +
                                   static_cast<std::int32_t>(
                                       sprite_s.vec_offset.X + vec_depth_sprite_offset_.X);
      const std::int32_t int4_ch = (sprite_depth.Bounds.Top() + sprite_depth.Bounds.Bottom()) / 2 +
                                   static_cast<std::int32_t>(
                                       sprite_s.vec_offset.Y + vec_depth_sprite_offset_.Y);
      const std::int32_t int4_w = sprite_s.Bounds.Width / 2;
      const std::int32_t int4_h = sprite_s.Bounds.Height / 2;

      gfx::SpriteWithSecondaryData sprite_wrapped{
          sprite_new, *sprite_depth.ptr_sheet,
          Rectangle::FromLTRB(int4_cw - int4_w, int4_ch - int4_h, int4_cw + int4_w, int4_ch + int4_h),
          sprite_depth.kind_channel};
      vec_all_sprites.push_back(std::move(sprite_wrapped));
    }
  }

  if (!opt_int4_length_.has_value())
    opt_int4_length_ = static_cast<std::int32_t>(vec_all_sprites.size()) - int4_start_;

  if (opt_vec_alpha_.has_value()) {
    if (opt_vec_alpha_->size() == 1) {
      std::vector<float> vec_expanded(*opt_int4_length_, (*opt_vec_alpha_)[0]);
      opt_vec_alpha_ = std::move(vec_expanded);
    } else if (opt_vec_alpha_->size() != static_cast<std::size_t>(*opt_int4_length_)) {
      throw yaml::YamlException(std::format(
          "Sequence {}.{} must define either 1 or {} Alpha values.", str_image_, str_name_,
          *opt_int4_length_));
    }
  } else if (b_alpha_fade_) {
    std::vector<float> vec_faded;
    for (std::int32_t int4_i{}; int4_i < *opt_int4_length_; int4_i++)
      vec_faded.push_back(
          gfx::Lerp(1.0f, 0.0f, static_cast<float>(int4_i) /
                                    static_cast<float>(*opt_int4_length_ - 1)));
    opt_vec_alpha_ = std::move(vec_faded);
  }

  // 重索引:面向逆时针排序 + 去未用帧 —— 上游注释逐句
  // Reindexing: order facings anti-clockwise and drop unused frames — the
  // upstream comment verbatim.
  std::optional<std::vector<std::int32_t>> opt_vec_index = CalculateFrameIndices(
      int4_start_, opt_int4_length_, opt_int4_stride_.value_or(*opt_int4_length_), int4_facings_,
      std::nullopt, b_transpose_, b_reverse_facings_, -1);
  if (b_reverses_) {
    // index + index.Skip(1).Take(length-2).Reverse()
    std::vector<std::int32_t> vec_index = *opt_vec_index;
    const std::size_t st_take = static_cast<std::size_t>(*opt_int4_length_) - 2;
    std::vector<std::int32_t> vec_extra;
    for (std::size_t int4_i = 1; int4_i < 1 + st_take; int4_i++)
      vec_extra.push_back(vec_index[int4_i]);
    for (auto it = vec_extra.rbegin(); it != vec_extra.rend(); ++it)
      vec_index.push_back(*it);
    opt_vec_index = std::move(vec_index);

    if (opt_vec_alpha_.has_value()) {
      std::vector<float> vec_alpha = *opt_vec_alpha_;
      std::vector<float> vec_alpha_extra;
      for (std::size_t int4_i = 1; int4_i < 1 + st_take; int4_i++)
        vec_alpha_extra.push_back(vec_alpha[int4_i]);
      for (auto it = vec_alpha_extra.rbegin(); it != vec_alpha_extra.rend(); ++it)
        vec_alpha.push_back(*it);
      opt_vec_alpha_ = std::move(vec_alpha);
    }

    opt_int4_length_ = 2 * *opt_int4_length_ - 2;
  }

  if (opt_vec_index->empty())
    throw yaml::YamlException(
        std::format("Sequence {}.{} does not define any frames.", str_image_, str_name_));

  const auto [int4_min_index, int4_max_index] = std::ranges::minmax(*opt_vec_index);
  if (int4_min_index < 0 ||
      int4_max_index >= static_cast<std::int32_t>(vec_all_sprites.size()))
    throw yaml::YamlException(std::format(
        "Sequence {}.{} uses frames between {}..{}, but only 0..{} exist.", str_image_, str_name_,
        int4_min_index, int4_max_index, static_cast<std::int32_t>(vec_all_sprites.size()) - 1));

  for (const std::int32_t int4_f : *opt_vec_index)
    vec_sprites_.push_back(vec_all_sprites[static_cast<std::size_t>(int4_f)]);
  if (int4_shadow_start_ >= 0) {
    for (const std::int32_t int4_f : *opt_vec_index)
      vec_shadow_sprites_.push_back(
          vec_all_sprites[static_cast<std::size_t>(int4_f - int4_start_ + int4_shadow_start_)]);
    b_has_shadow_sprites_ = true;
  }

  // sprites.Concat(shadowSprites).Select(OffsetSpriteBounds).Union() —— 空
  // bounds 帧贡献 Empty(上游 OffsetSpriteBounds L541-550)
  // sprites.Concat(shadowSprites).Select(OffsetSpriteBounds).Union() — an
  // empty-bounds frame contributes Empty (upstream OffsetSpriteBounds
  // L541-550).
  const auto offset_bounds = [](const gfx::Sprite& sprite_in) -> Rectangle {
    if (sprite_in.ptr_sheet == nullptr || sprite_in.Bounds.IsEmpty())
      return Rectangle::Empty();

    return Rectangle{
        static_cast<std::int32_t>(sprite_in.vec_offset.X - sprite_in.vec_size.X / 2),
        static_cast<std::int32_t>(sprite_in.vec_offset.Y - sprite_in.vec_size.Y / 2),
        sprite_in.Bounds.Width, sprite_in.Bounds.Height};
  };

  Rectangle rect_bounds = offset_bounds(vec_sprites_.front());
  for (std::size_t int4_i = 1; int4_i < vec_sprites_.size(); int4_i++)
    rect_bounds = Rectangle::Union(rect_bounds, offset_bounds(vec_sprites_[int4_i]));
  for (const gfx::Sprite& sprite_in : vec_shadow_sprites_)
    rect_bounds = Rectangle::Union(rect_bounds, offset_bounds(sprite_in));
  opt_rect_bounds_ = rect_bounds;
}

// ———— 查询面(L219-242/552-604)————
// ———— The query faces (L219-242/552-604) ————

void DefaultSpriteSequence::ThrowIfUnresolved() const {
  if (!opt_rect_bounds_.has_value())
    throw std::runtime_error(
        std::format("Unable to query unresolved sequence {}.{}.", str_image_, str_name_));
}

std::int32_t DefaultSpriteSequence::Length() const {
  ThrowIfUnresolved();
  return *opt_int4_length_;
}

Rectangle DefaultSpriteSequence::Bounds() const {
  ThrowIfUnresolved();
  return *opt_rect_bounds_;
}

gfx::Sprite DefaultSpriteSequence::GetSprite(std::int32_t int4_frame) {
  return GetSprite(int4_frame, WAngle::Zero());
}

gfx::Sprite DefaultSpriteSequence::GetSprite(std::int32_t int4_frame, WAngle wangle_facing) {
  ThrowIfUnresolved();
  const std::int32_t int4_index =
      GetFacingFrameOffset(wangle_facing) * *opt_int4_length_ + int4_frame % *opt_int4_length_;
  const gfx::Sprite sprite_s =
      vec_sprites_[static_cast<std::size_t>(int4_index)];
  if (sprite_s.ptr_sheet == nullptr)
    throw std::runtime_error(std::format(
        "Attempted to query unloaded sprite from {}.{} frame={} facing={}.", str_image_, str_name_,
        int4_frame, wangle_facing.Angle));

  return sprite_s;
}

std::pair<gfx::Sprite, WAngle> DefaultSpriteSequence::GetSpriteWithRotation(
    std::int32_t int4_frame, WAngle wangle_facing) {
  WAngle wangle_rotation = WAngle::Zero();
  if (opt_interpolated_facings_.has_value())
    wangle_rotation = gfx::GetInterpolatedFacingRotation(wangle_facing, std::abs(int4_facings_),
                                                         *opt_interpolated_facings_);

  return {GetSprite(int4_frame, wangle_facing), wangle_rotation};
}

gfx::Sprite DefaultSpriteSequence::GetShadow(std::int32_t int4_frame, WAngle wangle_facing) {
  if (!b_has_shadow_sprites_)
    return gfx::Sprite{};

  const std::int32_t int4_index =
      GetFacingFrameOffset(wangle_facing) * *opt_int4_length_ + int4_frame % *opt_int4_length_;
  const gfx::Sprite sprite_s =
      vec_shadow_sprites_[static_cast<std::size_t>(int4_index)];
  if (sprite_s.ptr_sheet == nullptr)
    throw std::runtime_error(std::format(
        "Attempted to query unloaded shadow sprite from {}.{} frame={} facing={}.", str_image_,
        str_name_, int4_frame, wangle_facing.Angle));

  return sprite_s;
}

float DefaultSpriteSequence::GetAlpha(std::int32_t int4_frame) {
  return opt_vec_alpha_.has_value() ? (*opt_vec_alpha_)[static_cast<std::size_t>(int4_frame)] : 1.0f;
}

std::int32_t DefaultSpriteSequence::GetFacingFrameOffset(WAngle wangle_facing) {
  return gfx::IndexFacing(wangle_facing, int4_facings_);
}

}  // namespace ora::mods
