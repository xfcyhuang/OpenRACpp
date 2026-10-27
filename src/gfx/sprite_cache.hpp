#pragma once
import std;

#include "fs/file_system.hpp"
#include "gfx/sheet.hpp"
#include "gfx/sprite_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

using AdjustFrameFn = std::function<const ISpriteFrame*(const ISpriteFrame& frame_input,
                                                         std::int32_t int4_index,
                                                         std::int32_t int4_total)>;

class SpriteCache {
 public:

  SpriteCache(fs::FileSystem& file_system, std::span<const SpriteLoaderFn> vec_loaders,
              std::int32_t int4_bgra_sheet_size, std::int32_t int4_indexed_sheet_size,
              std::int32_t int4_bgra_margin = 1, std::int32_t int4_indexed_margin = 1,
              RenderThread* ptr_render = nullptr);

  std::int32_t ReserveSprites(const std::string& str_filename,
                              std::optional<std::vector<std::int32_t>> vec_frames,
                              yaml::SourceLocation location_source,
                              AdjustFrameFn fn_adjust_frame = nullptr, bool b_premultiplied = false);

  std::optional<FramesSource> LoadFramesUncached(const std::string& str_filename);

  void LoadReservations();

  std::vector<Sprite> ResolveSprites(std::int32_t int4_token);

  std::vector<std::pair<std::string, yaml::SourceLocation>> MissingFiles() const;

  const SheetBuilder& BuilderOf(SheetType kind_type) const;
  SheetBuilder& BuilderOf(SheetType kind_type);

 private:
  struct Reservation {
    std::optional<std::vector<std::int32_t>> vec_frames;
    yaml::SourceLocation location_source{};
    AdjustFrameFn fn_adjust_frame;
    bool b_premultiplied = false;
  };

  fs::FileSystem& file_system_;
  std::vector<SpriteLoaderFn> vec_loaders_;
  SheetBuilder builder_indexed_;
  SheetBuilder builder_bgra_;

  std::vector<std::pair<std::int32_t, Reservation>> vec_sprite_reservations_;
  std::vector<std::pair<std::string, std::vector<std::int32_t>>> vec_reservations_by_filename_;

  std::map<std::int32_t, std::optional<std::vector<Sprite>>> map_resolved_sprites_;
  std::map<std::int32_t, std::pair<std::string, yaml::SourceLocation>> map_missing_files_;

  std::int32_t int4_next_reservation_token_ = 1;
};

}
