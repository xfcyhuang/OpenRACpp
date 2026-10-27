#pragma once
import std;

#include "fs/file_system.hpp"
#include "gfx/sprite_frame.hpp"

namespace ora::gfx {

using SpriteLoaderFn = bool (*)(std::span<const std::byte> vec_file,
                                std::vector<std::unique_ptr<ISpriteFrame>>& vec_frames);

struct FramesSource {
  std::vector<std::byte> vec_file;
  std::vector<std::unique_ptr<ISpriteFrame>> vec_frames;
};

std::vector<SpriteLoaderFn> MakeSpriteLoaders(const std::vector<std::string>& vec_format_names);

std::optional<FramesSource> GetFramesFromBytes(std::span<const std::byte> vec_file,
                                              std::span<const SpriteLoaderFn> vec_loaders);

std::optional<FramesSource> TryGetFrames(fs::FileSystem& file_system, const std::string& str_filename,
                                        std::span<const SpriteLoaderFn> vec_loaders);
FramesSource GetFramesOrThrow(fs::FileSystem& file_system, const std::string& str_filename,
                              std::span<const SpriteLoaderFn> vec_loaders);

class FrameCache {
 public:
  FrameCache(fs::FileSystem& file_system, std::span<const SpriteLoaderFn> vec_loaders)
      : file_system_{file_system}, vec_loaders_{vec_loaders.begin(), vec_loaders.end()} {}

  const std::vector<std::unique_ptr<ISpriteFrame>>& operator[](const std::string& str_filename);

 private:
  fs::FileSystem& file_system_;
  std::vector<SpriteLoaderFn> vec_loaders_;
  std::vector<std::pair<std::string, FramesSource>> vec_cache_;
};

}
