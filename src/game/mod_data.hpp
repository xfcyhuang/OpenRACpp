// UPSTREAM: OpenRA.Game/ModData.cs @7d57605 L29-233(Phase 2 最小集)+
//          OpenRA.Mods.Common/FileSystem/{Default,ContentInstallerFileSystem}Loader.cs
//          (FileSystem 节点的挂载语义)
//          The Phase 2 minimal ModData + the mount semantics of the
//          FileSystem-section loaders.
//
// Phase 2 范围裁剪(其余成员随各阶段落地)/ Phase 2 scope (the remaining
// members land with their phases):WidgetLoader/MapCache/各格式 Loader/
// Hotkeys/Cursors/Fluent/LoadScreen/GlobalModData 属 Phase 4-6。
// WidgetLoader/MapCache/format loaders/Hotkeys/Cursors/Fluent/LoadScreen/
// GlobalModData belong to Phases 4-6.
#pragma once
import std;

#include "fs/file_system.hpp"
#include "fs/folder.hpp"
#include "game/manifest.hpp"
#include "game/ruleset.hpp"

namespace ora::game {

/// InstalledMods 的最小集:mods/ 下每个含 mod.yaml 的子目录(上游
/// InstalledMods 的发现逻辑含 SupportsMapsFrom/依赖链,Phase 6 随主菜单;
/// Phase 2 直接目录枚举,发现序 = 目录名排序)
/// The minimal InstalledMods: every mods/ subdirectory carrying a mod.yaml
// (upstream discovery handles SupportsMapsFrom/dependency chains with the
// Phase 6 main menu; Phase 2 enumerates directories directly, in name
// order).
class InstalledMods final {
 public:
  /// str_mods_root:上游仓库的 mods/ 目录绝对路径
  /// str_mods_root: absolute path of the upstream mods/ directory.
  explicit InstalledMods(std::string str_mods_root);

  /// Game.Mods[id] 的等价:mod id → 已解析 Manifest(构造时全量解析)
  /// The Game.Mods[id] equivalent: mod id → parsed Manifest (all parsed at
  /// construction).
  const Manifest* Find(std::string_view str_id) const;

  /// '$mod' 挂载解析器安装到 FileSystem(ModData 构造器 L61-64 的
  /// ObjectCreator/installedMods 接线等价)
  /// Installs the '$mod' mount resolver onto a FileSystem (the
  /// ObjectCreator/installedMods wiring equivalent of the ModData ctor
  /// L61-64).
  void InstallResolver(fs::FileSystem& fileSystem) const;

 private:
  std::string str_modsRoot_;
  std::vector<std::pair<std::string, std::unique_ptr<Manifest>>> vec_mods_;
  std::vector<std::unique_ptr<fs::Folder>> vec_packages_;  // mod 目录包(Manifest.Package 的所有权)
                                                           // mod directory packages (the
                                                           // ownership of Manifest.Package).
};

/// ModData 的 Phase 2 最小集(ModData.cs L29)
class ModData final {
 public:
  /// str_engine_dir:'^EngineDir' 展开值(上游仓库根)
  /// str_engine_dir: the '^EngineDir' expansion (the upstream repo root).
  ModData(const Manifest& manifest_source, const InstalledMods& installedMods,
          std::string str_engine_dir);

  const Manifest& ManifestRef() const { return *manifest_; }
  fs::FileSystem& ModFiles() { return fs_modFiles_; }
  Ruleset& DefaultRules();

 private:
  std::unique_ptr<Manifest> manifest_;
  fs::FileSystem fs_modFiles_;
  std::unique_ptr<Ruleset> ruleset_default_;  // Lazy<Ruleset> 的等价 / the Lazy<Ruleset> equivalent.
};

}  // namespace ora::game
