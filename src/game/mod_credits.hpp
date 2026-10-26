// UPSTREAM: OpenRA.Mods.Common/ModCredits.cs @1f594e320 L12-23(全文)+
//          ModData.cs L71-95 的 GlobalModData 装载链对 ModCredits 的投影
//          (FieldLoader.Load 三字段;缺省 = CreateBasic)。
//          The whole ModCredits value face + the ModCredits projection of
//          ModData.cs L71-95's GlobalModData loading chain
//          (FieldLoader.Load over the three fields; the default is the
//          CreateBasic form).
#pragma once
import std;

#include "yaml/mini_yaml.hpp"

namespace ora::game {

struct ModCredits {
  std::string str_mod_tab_title = "Game";
  std::string str_mod_credits_file;   // null 的空串承载 | empty = null
  std::string str_engine_credits_file = "^EngineDir|AUTHORS";

  bool HasModCreditsFile() const { return !str_mod_credits_file.empty(); }
};

/// mod.yaml "ModCredits" 节点装载(GlobalModData 链的 FieldLoader 面;节点
/// 缺失 = 全默认)。
/// Loads the mod.yaml "ModCredits" node (the GlobalModData chain's
/// FieldLoader face; a missing node = all defaults).
ModCredits ModCreditsFromYaml(const yaml::MiniYaml* ptr_node);

}  // namespace ora::game
