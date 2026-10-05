// UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @b6fc03f L53-114(ISpriteLoader/
//          FrameCache/FrameLoader)+
//          OpenRA.Game/ModData.cs @b6fc03f L107(ObjectCreator.GetLoaders&lt;ISpriteLoader&gt;)
//          OpenRA.Game/ObjectCreator.cs @b6fc03f L155(loader 未找到的异常文本)
// 精灵加载链:按 Manifest.SpriteFormats 名序逐个尝试(首个命中即用)。
// C++ 形态:
//   - ISpriteLoader 接口 → 函数指针 bool(*)(span, vector<unique_ptr<
//     ISpriteFrame>>&)(各格式的 TryParse* 已有,签名一致);
//   - ObjectCreator.GetLoaders(名 → 实例)→ MakeSpriteLoaders 名字分派表
//     (与 MakePackageLoaders 同形,D73 族);未知名消息逐字("Unable to find
//     a sprite loader for type '{name}'.");
//   - FrameLoader 的 TypeDictionary metadata 出参未接:引擎消费面
//     (SpriteCache/FrameCache)对 metadata 仅透传不读,PngSheet 的
//     EmbeddedSpritePalette 随 mods 序列批(COVERAGE 登记);
//   - **帧与文件字节共持有**:上游帧可引用解码缓冲(ShpTD 的 TrimmedFrame
//     借底层 byte[],靠 GC 存活);C++ 以 FramesSource 同时持有文件字节与
//     帧表,调用方保证两者同寿命(形态适配,COVERAGE 登记)。
// The sprite loader chain: try each loader in Manifest.SpriteFormats name
// order, first hit wins. C++ shape:
//   - the ISpriteLoader interface → the function-pointer signature
//     bool(*)(span, vector<unique_ptr<ISpriteFrame>>&) (the existing
//     per-format TryParse* functions already match it);
//   - ObjectCreator.GetLoaders (name → instance) → the MakeSpriteLoaders
//     name-dispatch table (the same shape as MakePackageLoaders, the D73
//     family); the unknown-name message is verbatim ("Unable to find a
//     sprite loader for type '{name}'.");
//   - FrameLoader's TypeDictionary metadata output is not wired: the engine
//     consumers (SpriteCache/FrameCache) only pass metadata through without
//     reading it; PngSheet's EmbeddedSpritePalette lands with the mods
//     sequences batch (registered in COVERAGE);
//   - **frames co-own the file bytes**: upstream frames may reference the
//     decode buffer (ShpTD's TrimmedFrame borrows the underlying byte[],
//     kept alive by the GC); C++ carries the file bytes and the frame table
//     together in FramesSource, with the caller keeping both alive (a shape
//     adaptation, registered in COVERAGE).
#pragma once
import std;

#include "fs/file_system.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::gfx {

/// 单个精灵格式加载器(各 formats 的 TryParse* 同签名)。
/// One sprite-format loader (the formats' TryParse* share the signature).
using SpriteLoaderFn = bool (*)(std::span<const std::byte> vec_file,
                                std::vector<std::unique_ptr<ISpriteFrame>>& vec_frames);

/// 帧与所引用的文件字节(上游靠 GC 的显式等价物)。
/// The frames plus the file bytes they reference (the explicit equivalent
/// of upstream's GC).
struct FramesSource {
  std::vector<std::byte> vec_file;
  std::vector<std::unique_ptr<ISpriteFrame>> vec_frames;
};

/// ObjectCreator.GetLoaders&lt;ISpriteLoader&gt;(Manifest.SpriteFormats)(ModData.cs
/// L107):名字 → 加载器链。未知名抛 InvalidOperationException 文本逐字。
/// ObjectCreator.GetLoaders<ISpriteLoader>(Manifest.SpriteFormats)
/// (ModData.cs L107): names → the loader chain. An unknown name throws the
/// InvalidOperationException text verbatim.
std::vector<SpriteLoaderFn> MakeSpriteLoaders(const std::vector<std::string>& vec_format_names);

/// FrameLoader.GetFrames(stream, loaders, filename, out _)(L104-113):链内
/// 首个命中;全不命中 = 帧表空(上游 null)。
/// FrameLoader.GetFrames (stream, loaders, filename, out _) (L104-113): the
/// first chain hit; no hit = the empty frame table (upstream's null).
std::optional<FramesSource> GetFramesFromBytes(std::span<const std::byte> vec_file,
                                              std::span<const SpriteLoaderFn> vec_loaders);

/// SpriteCache.GetFrames / FrameLoader.GetFrames(fileSystem, …)(L62-75/L92-
/// 102):打开文件 → 链;文件缺失 = 帧表空(SpriteCache 面)/抛(FileLoader
/// 面,经 fileSystem.Open)。合法但无加载器命中 → "X is not a valid sprite
/// file!" 逐字(FrameLoader 面)。
/// SpriteCache.GetFrames / FrameLoader.GetFrames(fileSystem, …) (L62-75/L92-
/// 102): open then chain; a missing file = the empty frame table (the
/// SpriteCache face) / throws (the FrameLoader face, through
/// fileSystem.Open); a valid file no loader hits → "X is not a valid sprite
/// file!" verbatim (the FrameLoader face).
std::optional<FramesSource> TryGetFrames(fs::FileSystem& file_system, const std::string& str_filename,
                                        std::span<const SpriteLoaderFn> vec_loaders);
FramesSource GetFramesOrThrow(fs::FileSystem& file_system, const std::string& str_filename,
                              std::span<const SpriteLoaderFn> vec_loaders);

/// FrameCache(L78-88):文件名 → 帧的一次性缓存(Cache&lt;string,…&gt; 的
/// memo 语义;重复索引导向首次结果,字节随帧共持)。
/// FrameCache (L78-88): the filename → frames memo (Cache<string,…>
/// semantics; repeat indexing serves the first result, the bytes co-held
/// with the frames).
class FrameCache {
 public:
  FrameCache(fs::FileSystem& file_system, std::span<const SpriteLoaderFn> vec_loaders)
      : file_system_{file_system}, vec_loaders_{vec_loaders.begin(), vec_loaders.end()} {}

  /// this[filename](L87):解析失败抛(经 GetFramesOrThrow)。
  /// this[filename] (L87): throws on parse failure (through
  /// GetFramesOrThrow).
  const std::vector<std::unique_ptr<ISpriteFrame>>& operator[](const std::string& str_filename);

 private:
  fs::FileSystem& file_system_;
  std::vector<SpriteLoaderFn> vec_loaders_;
  std::vector<std::pair<std::string, FramesSource>> vec_cache_;
};

}  // namespace ora::gfx
