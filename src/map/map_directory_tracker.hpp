#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::map {

class MapCache;
enum class MapClassification : std::int32_t;
enum class MapStatus : std::int32_t;

enum class MapActionKind : std::int32_t { Add, Delete, Update };

class MapDirectoryTracker final {
 public:
  MapDirectoryTracker(const fs::IReadOnlyPackage* ptr_package,
                      MapClassification classification);
  ~MapDirectoryTracker();

  MapDirectoryTracker(const MapDirectoryTracker&) = delete;
  MapDirectoryTracker& operator=(const MapDirectoryTracker&) = delete;

  void UpdateMaps(MapCache& mapcache);

 private:
  struct FileStamp {
    std::string str_name;
    std::int64_t int8_stamp;
  };

  void AddMapAction(MapActionKind kind, const std::string& str_full_path,
                    const std::string* str_old_full_path);
  void EnqueueAction(MapActionKind kind, const std::string& str_full_path,
                     const std::string* str_old_full_path);
  std::optional<std::string> RemoveSubDirs(const std::string& str_path) const;
  std::vector<FileStamp> ScanDirectory() const;

  const fs::IReadOnlyPackage* ptr_package_ = nullptr;
  MapClassification kind_classification_{};
  std::string str_root_;

  std::mutex mtx_;
  std::vector<std::pair<std::string, MapActionKind>> vec_action_queue_;
  bool b_dirty = false;
  bool b_first_scan_ = true;
  std::vector<FileStamp> vec_last_scan_;
};

}  // namespace ora::map
