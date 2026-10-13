// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpRemasteredLoader.cs @e180e3225
// L24-121(实现半;头文件为契约半)。两条上游正则 boost::regex 消费点
// 直调(D235;原等价手写复刻 D67 由本批取代):
//   - FilenameRegex ^(?<prefix>.+?[\-_])(?<frame>\d{4})\.tga$ 的惰性
//     .+? = boost perl 语义的最左匹配(前缀含分隔符);
//   - MetaRegex 的 {"size":[w,h],"crop":[l,t,r,b]} 六命名组捕获,
//     regex_match 全串恰匹配。
// boost::regex 替代 std/手写的选型理由:其匹配引擎性能优于 std::regex
// (../cpp26.md §二 std 优先规则的性能豁免)。容器实现:ora::fs::
// ReadOnlyZipFile(条目序 = 中央目录序)。
// The implementation half (the contract half is in the header). The two
// upstream regexes are consumed by direct boost::regex calls at the use
// sites (D235; superseding the former equivalent hand-written parsers of
// D67): FilenameRegex ^(?<prefix>.+?[\-_])(?<frame>\d{4})\.tga$ with its
// lazy .+? = the leftmost match of boost's perl semantics (the prefix
// includes the separator); MetaRegex's six named capture groups over
// {"size":[w,h],"crop":[l,t,r,b]} with regex_match as the exact
// full-string match. boost::regex over std/hand-written per the
// performance exemption of ../cpp26.md §2: its matching engine outperforms
// std::regex. The container: ora::fs::ReadOnlyZipFile (entry order =
// central directory order).
#include "formats/shp_remastered.hpp"

#include "fs/zip_file.hpp"
#include <boost/regex.hpp>  // 上游两条正则直调(性能选型;白名单头)| the two upstream regexes by direct call (a performance selection; a whitelisted header)

namespace ora::fmt {
namespace {

/// FilenameRegex(L51)直调:boost::regex 的惰性 .+?[\-_] 最左匹配。返回
/// false = 不匹配;匹配时 str_prefix 含分隔符,int4_frame = 4 位数字值。
/// The direct FilenameRegex (L51) call: boost::regex's leftmost lazy
/// .+?[\-_] match. False means no match; on a match str_prefix includes
/// the separator and int4_frame is the four-digit value.
bool MatchFrameName(const std::string& str_name, std::string& str_prefix, std::int32_t& int4_frame) {
  static const boost::regex re_filename("^(?<prefix>.+?[-_])(?<frame>\\d{4})\\.tga$");
  boost::smatch m;
  if (!boost::regex_search(str_name, m, re_filename))
    return false;
  str_prefix = m["prefix"].str();
  const std::string str_frame = m["frame"].str();
  std::from_chars(str_frame.data(), str_frame.data() + str_frame.size(), int4_frame);
  return true;
}

/// MetaRegex(L52)的承载结构(失配 = 上游 ParseGroup 的 FormatException
/// 等价抛点,D67)。
/// The carrier struct of MetaRegex (L52) (a mismatch hits the equivalent
/// of upstream ParseGroup's FormatException, D67).
struct MetaInfo {
  int2 int2_size;
  Rectangle rect_crop;
};

/// MetaRegex(L52)直调:六命名组捕获,regex_match 全串恰匹配(手写的
/// st_pos==size 恰尽语义)。
/// The direct MetaRegex (L52) call: six named capture groups with
/// regex_match as the exact full-string match (the hand-written
/// st_pos==size exact-consumption semantics).
bool MatchMetaText(const std::string& str_text, MetaInfo& meta_out) {
  static const boost::regex re_meta(
      "\\{\"size\":\\[(?<w>\\d+),(?<h>\\d+)\\],"
      "\"crop\":\\[(?<l>\\d+),(?<t>\\d+),(?<r>\\d+),(?<b>\\d+)\\]\\}");
  boost::smatch m;
  if (!boost::regex_match(str_text, m, re_meta))
    return false;

  const auto parse_group = [&m](const char* str_group, std::int32_t& int4_out) {
    const std::string str_value = m[str_group].str();
    std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_out);
  };

  std::int32_t int4_width = 0, int4_height = 0;
  std::int32_t int4_left = 0, int4_top = 0, int4_right = 0, int4_bottom = 0;
  parse_group("w", int4_width);
  parse_group("h", int4_height);
  parse_group("l", int4_left);
  parse_group("t", int4_top);
  parse_group("r", int4_right);
  parse_group("b", int4_bottom);

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
