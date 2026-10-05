// UPSTREAM: OpenRA.Game/Platform.cs @b6fc03f L288-312(ResolvePath)+
//          OpenRA.Game/Game.cs L98-110(EngineDir/BinDir/SupportDir 常量来源)
//          ResolvePath + the EngineDir/BinDir/SupportDir constants.
//
// '^EngineDir|^SupportDir|^BinDir' 前缀替换;引擎目录由宿主注入(上游 = 编译期
// 常量目录,工具/测试以仓库根/工作目录代入)。
// The '^EngineDir|^SupportDir|^BinDir' prefix rewriting; the engine directory
// is injected by the host (upstream = compile-time constant directories;
// tools/tests substitute the repository root / working directory).
#pragma once
import std;

namespace ora::game {

/// Platform.ResolvePath(Platform.cs L288-312):TrimEnd(' ','\t') + 特殊目录
/// 前缀替换(整值或 '前缀|' 形态)
/// Platform.ResolvePath (Platform.cs L288-312): TrimEnd(' ','\t') + special
/// directory prefix rewriting (whole-value or 'prefix|' forms).
class PathResolver final {
 public:
  /// str_engine_dir:'^EngineDir'/'^BinDir' 的展开值(仓库根/可执行目录)
  /// str_engine_dir: the '^EngineDir'/'^BinDir' expansion (repository root /
  /// executable directory).
  /// str_support_dir:'^SupportDir' 的展开值(用户内容目录)
  /// str_support_dir: the '^SupportDir' expansion (the user content dir).
  PathResolver(std::string str_engine_dir, std::string str_support_dir);

  std::string operator()(const std::string& str_path) const;

 private:
  std::string str_engine_dir_;
  std::string str_support_dir_;
};

}  // namespace ora::game
