// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpRemasteredLoader.cs @7d57605
// L24-121(实现半;头文件为契约半)。两条正则的等价手写解析:
//   - FilenameRegex 惰性 .+?[\-_]:自左向右找首个使"前缀 + 恰 4 数字 +
//     .tga 恰尽"成立的 -/_ 位置(前缀含分隔符);
//   - MetaRegex:字面前缀 {"size":[ + 数字对 + ],"crop":[ + 四数字 +
//     字尾 ]},全串恰匹配。
// 实现半(契约半见头文件)。FilenameRegex 的惰性 .+?[\-_] 自左向右找
// 首个使"前缀 + 恰 4 数字 + .tga 恰尽"成立的 -/_ 位置(前缀含分隔符);
// MetaRegex 为字面前缀 {"size":[ + 数字对 + ],"crop":[ + 四数字 + 字尾
// ]} 的全串恰匹配。容器实现:ora::fs::ReadOnlyZipFile(条目序 = 中央目
// 录序)。
// The implementation half (the contract half is in the header).
// FilenameRegex's lazy .+?[\-_] scans left to right for the first -/_
// position where "prefix + exactly four digits + .tga exactly to the end"
// holds (the prefix includes the separator); MetaRegex is an exact
// full-string match of the literal prefix {"size":[ + a digit pair +
// ],"crop":[ + four digits + the literal ]} tail. The container:
// ora::fs::ReadOnlyZipFile (entry order = central directory order).
#include "formats/shp_remastered.hpp"

#include "fs/zip_file.hpp"

namespace ora::fmt {
namespace {

/// FilenameRegex(L51)的等价手写解析。返回 false = 不匹配;匹配时
/// str_prefix 含分隔符,int4_frame = 4 位数字值。
/// The equivalent hand-written parse of FilenameRegex (L51). False means
/// no match; on a match str_prefix includes the separator and
/// int4_frame is the four-digit value.
bool MatchFrameName(const std::string& str_name, std::string& str_prefix, std::int32_t& int4_frame) {
  constexpr std::string_view kSuffixTga = ".tga";
  if (str_name.size() < 1 + 1 + 4 + 4)  // .+? 至少 1 字符 + 分隔符 + 4 数字 + .tga
    return false;
  if (!str_name.ends_with(kSuffixTga))
    return false;

  // 惰性 .+?:自左向右的首个可行分隔位 = 正则最左匹配。
  // The lazy .+?: the first feasible separator from the left = the
  // regex's leftmost match.
  for (std::size_t st_k = 1; st_k + 1 + 4 + 4 <= str_name.size(); st_k++) {
    const auto char_sep = str_name[st_k];
    if (char_sep != '-' && char_sep != '_')
      continue;

    const auto st_digits = st_k + 1;
    auto b_all_digits = true;
    auto int4_value = 0;
    for (auto int4_i = 0; int4_i < 4; int4_i++) {
      const auto char_d = str_name[st_digits + static_cast<std::size_t>(int4_i)];
      if (char_d < '0' || char_d > '9') {
        b_all_digits = false;
        break;
      }
      int4_value = int4_value * 10 + (char_d - '0');
    }
    if (!b_all_digits || st_digits + 4 + 4 != str_name.size())
      continue;

    str_prefix = str_name.substr(0, st_k + 1);
    int4_frame = int4_value;
    return true;
  }
  return false;
}

/// MetaRegex(L52)的等价手写解析(全串恰匹配;失配 = 上游 ParseGroup 的
/// FormatException 等价抛点,D67)。
/// The equivalent hand-written parse of MetaRegex (L52) (an exact
/// full-string match; a mismatch hits the equivalent of upstream
/// ParseGroup's FormatException, D67).
struct MetaInfo {
  int2 int2_size;
  Rectangle rect_crop;
};

bool ParseDigits(std::string_view str_text, std::size_t& st_pos, std::int32_t& int4_out) {
  auto int4_value = 0;
  auto st_start = st_pos;
  while (st_pos < str_text.size() && str_text[st_pos] >= '0' && str_text[st_pos] <= '9') {
    int4_value = int4_value * 10 + (str_text[st_pos] - '0');
    st_pos++;
  }
  if (st_pos == st_start)
    return false;
  int4_out = int4_value;
  return true;
}

bool MatchMetaText(const std::string& str_text, MetaInfo& meta_out) {
  const std::string_view str_view = str_text;
  std::size_t st_pos = 0;

  const auto eat = [&](std::string_view str_literal) {
    if (str_view.substr(st_pos, str_literal.size()) != str_literal)
      return false;
    st_pos += str_literal.size();
    return true;
  };

  std::int32_t int4_width = 0, int4_height = 0, int4_left = 0, int4_top = 0, int4_right = 0, int4_bottom = 0;
  if (!eat("{\"size\":[") || !ParseDigits(str_view, st_pos, int4_width) || !eat(",") ||
      !ParseDigits(str_view, st_pos, int4_height) || !eat("],\"crop\":[") || !ParseDigits(str_view, st_pos, int4_left) ||
      !eat(",") || !ParseDigits(str_view, st_pos, int4_top) || !eat(",") ||
      !ParseDigits(str_view, st_pos, int4_right) || !eat(",") || !ParseDigits(str_view, st_pos, int4_bottom) ||
      !eat("]}") || st_pos != str_view.size())
    return false;

  meta_out = MetaInfo{int2{int4_width, int4_height},
                      Rectangle::FromLTRB(int4_left, int4_top, int4_right, int4_bottom)};
  return true;
}

std::span<const std::byte> CharSpanToByte(std::span<const char> vec_chars) {
  return std::span<const std::byte>{reinterpret_cast<const std::byte*>(vec_chars.data()), vec_chars.size()};
}

}  // namespace

bool IsShpRemastered(std::span<const std::byte> vec_file) {
  if (vec_file.size() < 4)
    return false;
  const auto uint4_le = std::to_integer<std::uint32_t>(vec_file[0]) | (std::to_integer<std::uint32_t>(vec_file[1]) << 8) |
                        (std::to_integer<std::uint32_t>(vec_file[2]) << 16) |
                        (std::to_integer<std::uint32_t>(vec_file[3]) << 24);
  return uint4_le == 0x04034B50u;
}

ShpRemasteredSprite::ShpRemasteredSprite(std::span<const std::byte> vec_file) {
  auto vec_zip_bytes = std::vector<char>{};
  vec_zip_bytes.resize(vec_file.size());
  std::memcpy(vec_zip_bytes.data(), vec_file.data(), vec_file.size());
  const auto container = fs::ReadOnlyZipFile{std::move(vec_zip_bytes), "shp-remastered"};

  auto str_frame_prefix = std::string{};
  auto int4_frame_count = 0;
  for (const auto& str_name : container.Contents()) {
    auto str_prefix = std::string{};
    auto int4_frame = 0;
    if (!MatchFrameName(str_name, str_prefix, int4_frame))
      continue;

    if (str_frame_prefix.empty())
      str_frame_prefix = str_prefix;  // framePrefix ??= prefix(L74)

    if (str_prefix != str_frame_prefix)
      throw std::runtime_error("Frame prefix mismatch: `" + str_prefix + "` != `" + str_frame_prefix + "`");

    int4_frame_count = std::max(int4_frame_count, int4_frame + 1);
  }

  vec_frames_.resize(static_cast<std::size_t>(int4_frame_count));
  for (auto int4_i = 0; int4_i < int4_frame_count; int4_i++) {
    const auto str_tga_name = std::format("{}{:04}.tga", str_frame_prefix, int4_i);
    const auto vec_tga = container.GetStream(str_tga_name);
    if (!vec_tga.has_value()) {
      // Blank frame(L88-91)。| The blank frame (L88-91).
      vec_frames_[static_cast<std::size_t>(int4_i)] = std::make_unique<TgaFrame>();
      continue;
    }

    const auto str_meta_name = std::format("{}{:04}.meta", str_frame_prefix, int4_i);
    const auto vec_meta = container.GetStream(str_meta_name);
    if (vec_meta.has_value()) {
      auto meta = MetaInfo{};
      if (!MatchMetaText(std::string{vec_meta->data(), vec_meta->size()}, meta))
        throw std::runtime_error("FormatException: failed to parse remastered meta");  // ParseGroup 空捕获的等价抛点(D67)| the equivalent of ParseGroup's empty-capture throw (D67)

      vec_frames_[static_cast<std::size_t>(int4_i)] = std::make_unique<TgaFrame>(
          CharSpanToByte(std::span<const char>{vec_tga->data(), vec_tga->size()}), meta.int2_size, meta.rect_crop);
    } else {
      vec_frames_[static_cast<std::size_t>(int4_i)] =
          std::make_unique<TgaFrame>(CharSpanToByte(std::span<const char>{vec_tga->data(), vec_tga->size()}));
    }
  }
}

bool TryParseShpRemastered(std::span<const std::byte> vec_file,
                           std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsShpRemastered(vec_file)) {
    vec_frames.clear();
    return false;
  }

  vec_frames.clear();
  auto sprite = ShpRemasteredSprite{vec_file};
  vec_frames = std::move(sprite).TakeFrames();
  return true;
}

}  // namespace ora::fmt
