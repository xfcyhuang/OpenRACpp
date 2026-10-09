import std;

#include "map/map_directory_tracker.hpp"

#include "map/map_cache.hpp"

namespace ora::map {

namespace {

std::string NormalizeSep(std::string str_path) {
  for (char& ch : str_path)
    if (ch == '\\')
      ch = '/';
  return str_path;
}

}  // namespace

MapDirectoryTracker::MapDirectoryTracker(
    const fs::IReadOnlyPackage* ptr_package, MapClassification classification)
    : ptr_package_{ptr_package},
      kind_classification_{classification},
      str_root_{ptr_package != nullptr ? NormalizeSep(ptr_package->Name())
                                       : std::string{}} {}

MapDirectoryTracker::~MapDirectoryTracker() = default;

std::vector<MapDirectoryTracker::FileStamp>
MapDirectoryTracker::ScanDirectory() const {
  namespace fsstd = std::filesystem;
  std::vector<FileStamp> vec_out;
  std::error_code ec;
  if (!fsstd::exists(str_root_, ec) || !fsstd::is_directory(str_root_, ec))
    return vec_out;
  for (const fsstd::directory_entry& entry : fsstd::recursive_directory_iterator(
           str_root_, fsstd::directory_options::skip_permission_denied, ec)) {
    std::error_code ec_file;
    if (!entry.is_regular_file(ec_file))
      continue;
    const std::string str_full = NormalizeSep(entry.path().generic_string());
    std::int64_t int8_stamp = 0;
    if (auto tm = entry.last_write_time(ec_file); !ec_file)
      int8_stamp = tm.time_since_epoch().count();
    vec_out.push_back(FileStamp{str_full, int8_stamp});
  }
  return vec_out;
}

std::optional<std::string> MapDirectoryTracker::RemoveSubDirs(
    const std::string& str_path) const {
  const std::string str_prefix = str_root_ + "/";
  if (!str_path.starts_with(str_prefix))
    return std::nullopt;
  std::string str_end = str_path.substr(str_prefix.size());
  const std::size_t n_slash = str_end.find('/');
  if (n_slash != std::string::npos)
    str_end.resize(n_slash);
  return str_root_ + "/" + str_end;
}

void MapDirectoryTracker::AddMapAction(
    MapActionKind kind, const std::string& str_full_path,
    const std::string* str_old_full_path) {
  const std::lock_guard<std::mutex> lock{mtx_};
  EnqueueAction(kind, str_full_path, str_old_full_path);
}

void MapDirectoryTracker::EnqueueAction(
    MapActionKind kind, const std::string& str_full_path,
    const std::string* str_old_full_path) {
  b_dirty = true;

  const std::optional<std::string> opt_path = RemoveSubDirs(str_full_path);
  if (!opt_path.has_value() || str_full_path == *opt_path)
    vec_action_queue_.emplace_back(str_full_path, kind);
  else
    vec_action_queue_.emplace_back(*opt_path, MapActionKind::Update);

  if (str_old_full_path != nullptr) {
    const std::optional<std::string> opt_old_path =
        RemoveSubDirs(*str_old_full_path);
    if (opt_old_path.has_value()) {
      if (*str_old_full_path == *opt_old_path)
        vec_action_queue_.emplace_back(*opt_old_path, MapActionKind::Delete);
      else
        vec_action_queue_.emplace_back(*opt_old_path, MapActionKind::Update);
    }
  }
}

void MapDirectoryTracker::UpdateMaps(MapCache& mapcache) {
  const std::vector<FileStamp> vec_scan = ScanDirectory();

  const std::lock_guard<std::mutex> lock{mtx_};

  if (b_first_scan_) {
    b_first_scan_ = false;
    vec_last_scan_ = vec_scan;
    return;
  }

  for (const FileStamp& stamp : vec_scan) {
    const auto it_old = std::find_if(
        vec_last_scan_.begin(), vec_last_scan_.end(),
        [&](const FileStamp& s) { return s.str_name == stamp.str_name; });
    if (it_old == vec_last_scan_.end())
      EnqueueAction(MapActionKind::Add, stamp.str_name, nullptr);
    else if (it_old->int8_stamp != stamp.int8_stamp)
      EnqueueAction(MapActionKind::Update, stamp.str_name, nullptr);
  }
  for (const FileStamp& stamp : vec_last_scan_) {
    if (std::find_if(vec_scan.begin(), vec_scan.end(),
                     [&](const FileStamp& s) {
                       return s.str_name == stamp.str_name;
                     }) == vec_scan.end())
      EnqueueAction(MapActionKind::Delete, stamp.str_name, nullptr);
  }
  vec_last_scan_ = vec_scan;

  if (!b_dirty)
    return;
  b_dirty = false;

  std::vector<MapPreview*> vec_previews = mapcache.Previews();
  for (const auto& [str_key, kind] : vec_action_queue_) {
    const auto it_map = std::find_if(
        vec_previews.begin(), vec_previews.end(), [&](MapPreview* p) {
          return p->Path() == str_key && p->Status() == MapStatus::Available;
        });
    if (it_map != vec_previews.end()) {
      MapPreview* p = *it_map;
      if (kind == MapActionKind::Delete) {
        std::println("{} was deleted", str_key);
        p->Invalidate();
      } else {
        std::println("{} was updated", str_key);
        p->Invalidate();
        const std::string str_prefix =
            (ptr_package_ != nullptr ? NormalizeSep(ptr_package_->Name())
                                     : std::string{}) +
            "/";
        std::string str_rel = str_key;
        if (str_rel.starts_with(str_prefix))
          str_rel = str_rel.substr(str_prefix.size());
        mapcache.LoadMap(str_rel, *ptr_package_, kind_classification_, p->Uid());
      }
    } else {
      if (kind != MapActionKind::Delete) {
        std::println("{} was added", str_key);
        const std::string str_prefix =
            (ptr_package_ != nullptr ? NormalizeSep(ptr_package_->Name())
                                     : std::string{}) +
            "/";
        std::string str_rel = str_key;
        if (str_rel.starts_with(str_prefix))
          str_rel = str_rel.substr(str_prefix.size());
        mapcache.LoadMap(str_rel, *ptr_package_, kind_classification_,
                         std::string{});
      }
    }
  }

  vec_action_queue_.clear();
}

}  // namespace ora::map
