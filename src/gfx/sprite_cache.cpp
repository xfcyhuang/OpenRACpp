// UPSTREAM: OpenRA.Game/Graphics/SpriteCache.cs @b6fc03f L21-183
//          (sprite_cache.hpp 的实现;头注的形态适配说明适用)
//          Implementation of sprite_cache.hpp; the shape-adaptation notes of
//          the hpp header apply.
import std;
#include "gfx/sprite_cache.hpp"

namespace ora::gfx {

SpriteCache::SpriteCache(fs::FileSystem& file_system, std::span<const SpriteLoaderFn> vec_loaders,
                         std::int32_t int4_bgra_sheet_size, std::int32_t int4_indexed_sheet_size,
                         std::int32_t int4_bgra_margin, std::int32_t int4_indexed_margin,
                         RenderThread* ptr_render)
    : file_system_{file_system},
      vec_loaders_{vec_loaders.begin(), vec_loaders.end()},
      builder_indexed_{SheetType::Indexed, int4_indexed_sheet_size, int4_indexed_margin, ptr_render},
      builder_bgra_{SheetType::BGRA, int4_bgra_sheet_size, int4_bgra_margin, ptr_render} {}

std::int32_t SpriteCache::ReserveSprites(const std::string& str_filename,
                                         std::optional<std::vector<std::int32_t>> vec_frames,
                                         yaml::SourceLocation location_source,
                                         AdjustFrameFn fn_adjust_frame, bool b_premultiplied) {
  const std::int32_t int4_token = int4_next_reservation_token_++;
  vec_sprite_reservations_.emplace_back(
      int4_token, Reservation{std::move(vec_frames), location_source, std::move(fn_adjust_frame),
                              b_premultiplied});

  for (auto& [str_key, vec_tokens] : vec_reservations_by_filename_) {
    if (str_key == str_filename) {
      vec_tokens.push_back(int4_token);
      return int4_token;
    }
  }
  vec_reservations_by_filename_.emplace_back(str_filename, std::vector<std::int32_t>{int4_token});
  return int4_token;
}

std::optional<FramesSource> SpriteCache::LoadFramesUncached(const std::string& str_filename) {
  return TryGetFrames(file_system_, str_filename, vec_loaders_);
}

void SpriteCache::LoadReservations() {
  struct PendingResolve {
    const std::string* ptr_filename;
    std::int32_t int4_frame_index;
    bool b_premultiplied;
    const AdjustFrameFn* ptr_adjust_frame;  // null = 无 | absent
    const ISpriteFrame* ptr_frame;
    std::vector<Sprite>* ptr_sprites_for_token;
  };

  std::vector<PendingResolve> vec_pending_resolve;
  // 帧引用文件字节 —— 逐文件持有至本函数结束(上游靠 GC;见 sprite_loader.hpp)
  // Frames reference the file bytes — held per file until this function ends
  // (upstream leans on the GC; see sprite_loader.hpp).
  std::vector<std::pair<std::string, FramesSource>> vec_loaded;
  for (const auto& [str_filename, vec_tokens] : vec_reservations_by_filename_) {
    // modData.LoadScreen?.Display()(L93)—— Phase 6 进度面 no-op
    // modData.LoadScreen?.Display() (L93) — the Phase 6 progress face, no-op.
    auto opt_loaded_frames = TryGetFrames(file_system_, str_filename, vec_loaders_);
    const FramesSource* ptr_loaded = nullptr;
    if (opt_loaded_frames.has_value()) {
      vec_loaded.emplace_back(str_filename, std::move(*opt_loaded_frames));
      ptr_loaded = &vec_loaded.back().second;
    }
    for (const std::int32_t int4_token : vec_tokens) {
      const auto it_reservation =
          std::ranges::find_if(vec_sprite_reservations_,
                               [&](const auto& kv) { return kv.first == int4_token; });
      if (it_reservation == vec_sprite_reservations_.end())
        continue;
      const Reservation& rs = it_reservation->second;

      if (ptr_loaded != nullptr) {
        const auto& vec_loaded_frames = ptr_loaded->vec_frames;
        auto vec_resolved = std::vector<Sprite>(vec_loaded_frames.size());
        const auto it_inserted =
            map_resolved_sprites_.emplace(int4_token, std::move(vec_resolved));
        auto& vec_target = *it_inserted.first->second;

        if (rs.vec_frames.has_value()) {
          for (const std::int32_t int4_f : *rs.vec_frames) {
            if (int4_f >= static_cast<std::int32_t>(vec_loaded_frames.size())) {
              // "{Location}: {filename} does not contain frames: 1,2"(越界
              // 帧号列表,逗号连接;L103-105 逐字)
              // "{Location}: {filename} does not contain frames: 1,2" (the
              // out-of-range list, comma-joined; L103-105 verbatim).
              std::string str_list;
              for (const std::int32_t int4_bad : *rs.vec_frames) {
                if (int4_bad < static_cast<std::int32_t>(vec_loaded_frames.size()))
                  continue;
                if (!str_list.empty())
                  str_list += ',';
                str_list += std::to_string(int4_bad);
              }
              throw std::runtime_error(std::format("{}: {} does not contain frames: {}",
                                                   rs.location_source.ToString(),
                                                   str_filename, str_list));
            }
          }
        }

        // frames 为 null 时 = 0..len-1 全帧(L107-108)
        // A null frames list = every frame 0..len-1 (L107-108).
        const std::vector<std::int32_t> vec_all_frames = [&vec_loaded_frames, &rs]() {
          if (rs.vec_frames.has_value())
            return *rs.vec_frames;
          std::vector<std::int32_t> vec_range(vec_loaded_frames.size());
          std::iota(vec_range.begin(), vec_range.end(), 0);
          return vec_range;
        }();
        const std::int32_t int4_total = rs.vec_frames.has_value()
                                             ? static_cast<std::int32_t>(rs.vec_frames->size())
                                             : static_cast<std::int32_t>(vec_loaded_frames.size());

        std::int32_t int4_j = 0;
        for (const std::int32_t int4_i : vec_all_frames) {
          const ISpriteFrame* ptr_frame = vec_loaded_frames[static_cast<std::size_t>(int4_i)].get();
          if (rs.fn_adjust_frame != nullptr)
            ptr_frame = rs.fn_adjust_frame(*ptr_frame, int4_j++, int4_total);
          vec_pending_resolve.push_back(PendingResolve{&str_filename, int4_i, rs.b_premultiplied,
                                                       rs.fn_adjust_frame ? &rs.fn_adjust_frame
                                                                          : nullptr,
                                                       ptr_frame, &vec_target});
        }
      } else {
        map_resolved_sprites_.emplace(int4_token, std::nullopt);
        map_missing_files_.emplace(int4_token,
                                   std::make_pair(str_filename, rs.location_source));
      }
    }
  }

  vec_sprite_reservations_.clear();
  vec_reservations_by_filename_.clear();

  // SheetBuilder 加精灵时按行保留最高精灵的高度 —— 同高归拢可改善打包
  // (上游注释 L133-134 逐句)。OrderBy 稳定排序。
  // The sheet builder reserves the tallest sprite's height per row —
  // grouping similar heights packs better (upstream comment L133-134). The
  // OrderBy is a stable sort.
  std::stable_sort(vec_pending_resolve.begin(), vec_pending_resolve.end(),
                   [](const PendingResolve& pending_a, const PendingResolve& pending_b) {
                     return pending_a.ptr_frame->Size().Y < pending_b.ptr_frame->Size().Y;
                   });

  // (文件名, 帧索引, 预乘, AdjustFrame) 去重 —— 同图的预乘/非预乘两版本
  // 需各持一份(L145-146 上游注释)
  // Deduplicated by (filename, frame index, premultiplied, AdjustFrame) —
  // the same image in both premultiply flavors needs its own copy (the
  // L145-146 upstream comment).
  std::vector<std::tuple<const std::string*, std::int32_t, bool, const AdjustFrameFn*,
                         std::optional<Sprite>>>
      vec_sprite_cache;
  const auto sprite_cache_get_or_add = [&](const std::string* ptr_filename,
                                           std::int32_t int4_frame_index, bool b_premultiplied,
                                           const AdjustFrameFn* ptr_adjust,
                                           const ISpriteFrame& frame_input) -> Sprite {
    for (auto& tuple_entry : vec_sprite_cache) {
      if (std::get<0>(tuple_entry) == ptr_filename &&
          std::get<1>(tuple_entry) == int4_frame_index &&
          std::get<2>(tuple_entry) == b_premultiplied && std::get<3>(tuple_entry) == ptr_adjust)
        return *std::get<4>(tuple_entry);
    }
    SheetBuilder& builder = BuilderOf(SheetBuilder::FrameTypeToSheetType(frame_input.Type()));
    const Sprite sprite_added = builder.Add(frame_input, b_premultiplied);
    vec_sprite_cache.emplace_back(ptr_filename, int4_frame_index, b_premultiplied, ptr_adjust,
                                  sprite_added);
    return sprite_added;
  };

  for (const PendingResolve& pending : vec_pending_resolve) {
    (*pending.ptr_sprites_for_token)[static_cast<std::size_t>(pending.int4_frame_index)] =
        sprite_cache_get_or_add(pending.ptr_filename, pending.int4_frame_index,
                                pending.b_premultiplied, pending.ptr_adjust_frame,
                                *pending.ptr_frame);
  }

  if (builder_indexed_.Current() != nullptr)
    builder_indexed_.Current()->ReleaseBuffer();
  if (builder_bgra_.Current() != nullptr)
    builder_bgra_.Current()->ReleaseBuffer();
}

std::vector<Sprite> SpriteCache::ResolveSprites(std::int32_t int4_token) {
  const auto it_resolved = map_resolved_sprites_.find(int4_token);
  if (it_resolved == map_resolved_sprites_.end())
    throw std::runtime_error(std::format(
        "token {} has either already been resolved, or was never reserved via ReserveSprites",
        int4_token));

  std::optional<std::vector<Sprite>> opt_resolved = std::move(it_resolved->second);
  map_resolved_sprites_.erase(it_resolved);

  if (const auto it_missing = map_missing_files_.find(int4_token); it_missing != map_missing_files_.end()) {
    // FileNotFoundException("{Location}: {Filename} not found", Filename)
    // FileNotFoundException("{Location}: {Filename} not found", Filename).
    throw std::runtime_error(std::format("{}: {} not found",
                                         it_missing->second.second.ToString(),
                                         it_missing->second.first));
  }

  return std::move(*opt_resolved);
}

std::vector<std::pair<std::string, yaml::SourceLocation>> SpriteCache::MissingFiles() const {
  // ToHashSet 的元组去重(顺序保持首见)
  // The tuple dedup of ToHashSet (first-seen order kept).
  std::vector<std::pair<std::string, yaml::SourceLocation>> vec_out;
  for (const auto& [int4_token, pair_file] : map_missing_files_) {
    if (std::ranges::find(vec_out, pair_file) == vec_out.end())
      vec_out.push_back(pair_file);
  }
  return vec_out;
}

const SheetBuilder& SpriteCache::BuilderOf(SheetType kind_type) const {
  return kind_type == SheetType::Indexed ? builder_indexed_ : builder_bgra_;
}

SheetBuilder& SpriteCache::BuilderOf(SheetType kind_type) {
  return kind_type == SheetType::Indexed ? builder_indexed_ : builder_bgra_;
}

}  // namespace ora::gfx
