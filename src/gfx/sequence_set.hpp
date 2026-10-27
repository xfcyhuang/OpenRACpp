#pragma once
import std;

#include "core/wangle.hpp"
#include "fs/file_system.hpp"
#include "gfx/sprite_cache.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

class SpriteCache;

struct ISpriteSequence {
  virtual std::string_view Name() const = 0;
  virtual std::int32_t Length() const = 0;
  virtual std::int32_t Facings() const = 0;
  virtual std::int32_t Tick() const = 0;
  virtual std::int32_t ZOffset() const = 0;
  virtual std::int32_t ShadowZOffset() const = 0;
  virtual Rectangle Bounds() const = 0;
  virtual bool IgnoreWorldTint() const = 0;
  virtual float Scale() const = 0;
  virtual void ResolveSprites(SpriteCache& cache_sprites) = 0;
  virtual Sprite GetSprite(std::int32_t int4_frame) = 0;
  virtual Sprite GetSprite(std::int32_t int4_frame, WAngle wangle_facing) = 0;
  virtual std::pair<Sprite, WAngle> GetSpriteWithRotation(std::int32_t int4_frame,
                                                          WAngle wangle_facing) = 0;
  virtual Sprite GetShadow(std::int32_t int4_frame, WAngle wangle_facing) = 0;
  virtual float GetAlpha(std::int32_t int4_frame) = 0;
  virtual ~ISpriteSequence() = default;
};

struct ISpriteSequenceLoader {

  virtual std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> ParseSequences(
      SpriteCache& cache_sprites, const std::string& str_tile_set, const yaml::MiniYamlNode& node_image) = 0;
  virtual ~ISpriteSequenceLoader() = default;
};

class SequenceSet {
 public:

  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    std::span<const SpriteLoaderFn> vec_sprite_loaders;
    const std::vector<std::string>* vec_sequence_files = nullptr;
    std::int32_t int4_bgra_sheet_size = 2048;
    std::int32_t int4_indexed_sheet_size = 2048;
    RenderThread* ptr_render = nullptr;
  };

  SequenceSet(Deps deps, ISpriteSequenceLoader& loader_sequences, std::string str_tile_set,
              const yaml::MiniYaml* yaml_additional_sequences = nullptr);

  ISpriteSequence& GetSequence(const std::string& str_image,
                               const std::string& str_sequence) const;

  std::vector<std::string> Images() const;

  bool HasSequence(const std::string& str_image, const std::string& str_sequence) const;

  std::vector<std::string> Sequences(const std::string& str_image) const;

  void LoadSprites();

  SpriteCache& CacheRef() { return *ptr_sprite_cache_; }
  const SpriteCache& CacheRef() const { return *ptr_sprite_cache_; }
  const std::string& TileSet() const { return str_tile_set_; }

 private:

  const std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>>* FindImagesEntry(
      const std::string& str_image) const;

  std::string str_tile_set_;
  ISpriteSequenceLoader* ptr_loader_;
  yaml::StringPool pool_yaml_;
  std::vector<yaml::MiniYamlNode> vec_nodes_;

  std::vector<std::pair<std::string,
                        std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>>>>
      vec_images_;
  std::unique_ptr<SpriteCache> ptr_sprite_cache_;
};

}
