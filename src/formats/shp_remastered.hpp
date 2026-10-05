// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpRemasteredLoader.cs @7d57605
// L24-121(全文逐语义)
// Remastered SHP = zip 容器:条目名正则 ^(?<prefix>.+?[\-_])(?<frame>
// \d{4})\.tga$ 的惰性前缀(最短 .+? 后随 -/_ 分隔)、帧数 = max(帧号+1)、
// 缺号帧 = 空白 TgaFrame、meta 条目 {"size":[w,h],"crop":[l,t,r,b]} 的
// 画框/裁剪窗口、前缀不一致即抛。SharpZipLib ZipFile → ora::fs::
// ReadOnlyZipFile(miniz;条目枚举序 = 中央目录序,同 SharpZipLib 事实序);
// 两条正则以等价的手写最左匹配解析复刻(COVERAGE 登记 D67)。
// The Remastered SHP is a zip container: the entry-name regex ^(?<prefix>.
// +?[\-_])(?<frame>\d{4})\.tga$ with its lazy prefix (the shortest .+?
// followed by a -/_ separator), frame count = max(frame+1), gap frames as
// blank TgaFrames, the meta entries' {"size":[w,h],"crop":[l,t,r,b]}
// canvas/crop window, and the prefix-mismatch throw. SharpZipLib ZipFile →
// ora::fs::ReadOnlyZipFile (miniz; the entry enumeration order = the
// central-directory order, the same de-facto order as SharpZipLib); the
// two regexes are reproduced as equivalent hand-written leftmost-match
// parsers (registered as D67).
#pragma once
import std;

#include "formats/targa.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::fmt {

/// IsShpRemastered(L26-33):首 u32 小端 == 0x04034B50(zip 本地头签名)。
/// IsShpRemastered (L26-33): the leading little-endian u32 == 0x04034B50
/// (the zip local-file-header signature).
bool IsShpRemastered(std::span<const std::byte> vec_file);

/// TryParseSprite(L35-46):判定 + 解析一体。
/// TryParseSprite (L35-46): probe + parse in one.
bool TryParseShpRemastered(std::span<const std::byte> vec_file,
                           std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

/// ShpRemasteredSprite(L49-120):容器枚举 + 前缀链 + 逐帧取流。
/// ShpRemasteredSprite (L49-120): the container enumeration + the prefix
/// chain + the per-frame stream fetch.
class ShpRemasteredSprite {
 public:
  explicit ShpRemasteredSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }

  /// TryParseSprite 的帧数组移交形态(out 参数物化)。
  /// The frame-array handover for TryParseSprite (materializing the out
  /// parameter).
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> TakeFrames() && { return std::move(vec_frames_); }

 private:
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}  // namespace ora::fmt
