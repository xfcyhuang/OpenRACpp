// UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @b6fc03f L53-114 +
//          OpenRA.Game/ObjectCreator.cs @b6fc03f L155
//          (sprite_loader.hpp 的实现)
//          Implementation of sprite_loader.hpp.
import std;
#include "gfx/sprite_loader.hpp"

#include "formats/dds.hpp"
#include "formats/png_sheet_loader.hpp"
#include "formats/r8_loader.hpp"
#include "formats/shp_d2.hpp"
#include "formats/shp_remastered.hpp"
#include "formats/shp_td.hpp"
#include "formats/shp_ts.hpp"
#include "formats/targa.hpp"
#include "formats/tmp_ra.hpp"
#include "formats/tmp_td.hpp"
#include "formats/tmp_ts.hpp"

namespace ora::gfx {

namespace {

/// 字节视图(vector&lt;char&gt; → span&lt;const byte&gt;,零拷贝)。
/// A byte view (vector<char> → span<const byte>, zero-copy).
std::span<const std::byte> AsBytes(const std::vector<char>& vec_chars) {
  return std::span<const std::byte>{reinterpret_cast<const std::byte*>(vec_chars.data()),
                                    vec_chars.size()};
}

}  // namespace

std::vector<SpriteLoaderFn> MakeSpriteLoaders(const std::vector<std::string>& vec_format_names) {
  std::vector<SpriteLoaderFn> vec_loaders;
  for (const std::string& str_format : vec_format_names) {
    SpriteLoaderFn fn_loader = nullptr;
    if (str_format == "ShpTD")
      fn_loader = &fmt::TryParseShpTD;
    else if (str_format == "ShpD2")
      fn_loader = &fmt::TryParseShpD2;
    else if (str_format == "ShpTS")
      fn_loader = &fmt::TryParseShpTS;
    else if (str_format == "TmpTD")
      fn_loader = &fmt::TryParseTmpTD;
    else if (str_format == "TmpRA")
      fn_loader = &fmt::TryParseTmpRA;
    else if (str_format == "TmpTS")
      fn_loader = &fmt::TryParseTmpTS;
    else if (str_format == "PngSheet")
      fn_loader = &fmt::TryParsePngSheet;
    else if (str_format == "Tga")
      fn_loader = &fmt::TryParseTga;
    else if (str_format == "Dds")
      fn_loader = &fmt::TryParseDds;
    else if (str_format == "ShpRemastered")
      fn_loader = &fmt::TryParseShpRemastered;
    else if (str_format == "R8")
      fn_loader = &fmt::TryParseR8;

    if (fn_loader == nullptr)
      throw std::runtime_error(std::format("Unable to find a sprite loader for type '{}'.", str_format));
    vec_loaders.push_back(fn_loader);
  }
  return vec_loaders;
}

std::optional<FramesSource> GetFramesFromBytes(std::span<const std::byte> vec_file,
                                              std::span<const SpriteLoaderFn> vec_loaders) {
  for (const SpriteLoaderFn fn_loader : vec_loaders) {
    FramesSource source_next;
    source_next.vec_file.assign(vec_file.begin(), vec_file.end());
    if (fn_loader(source_next.vec_file, source_next.vec_frames))
      return source_next;
  }
  return std::nullopt;
}

std::optional<FramesSource> TryGetFrames(fs::FileSystem& file_system,
                                        const std::string& str_filename,
                                        std::span<const SpriteLoaderFn> vec_loaders) {
  std::vector<char> vec_bytes;
  if (!file_system.TryOpen(str_filename, vec_bytes))
    return std::nullopt;
  return GetFramesFromBytes(AsBytes(vec_bytes), vec_loaders);
}

FramesSource GetFramesOrThrow(fs::FileSystem& file_system, const std::string& str_filename,
                              std::span<const SpriteLoaderFn> vec_loaders) {
  std::vector<char> vec_bytes = file_system.Open(str_filename);
  auto source_frames = GetFramesFromBytes(AsBytes(vec_bytes), vec_loaders);
  if (!source_frames.has_value())
    throw std::runtime_error(str_filename + " is not a valid sprite file!");
  return std::move(*source_frames);
}

const std::vector<std::unique_ptr<ISpriteFrame>>& FrameCache::operator[](
    const std::string& str_filename) {
  for (const auto& [str_key, source_frames] : vec_cache_)
    if (str_key == str_filename)
      return source_frames.vec_frames;
  vec_cache_.emplace_back(str_filename, GetFramesOrThrow(file_system_, str_filename, vec_loaders_));
  return vec_cache_.back().second.vec_frames;
}

}  // namespace ora::gfx
