// UPSTREAM: OpenRA.Mods.Common/Traits/World/ControlGroups.cs @b6fc03f L19-150
//          (逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - List<Actor>[] 组表 + Contains/RemoveAll 的线性语义照抄(组内成员数以
//    个位数为主,线性即上游形态);IsInWorld/Disposed/Owner 过滤面原样
//    The List<Actor>[] group table with the linear Contains/RemoveAll
//    semantics kept (groups hold single-digit member counts — linear is the
//    upstream shape); the IsInWorld/Disposed/Owner filters verbatim.
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

/// ControlGroups(ControlGroups.cs L29) | ControlGroups (ControlGroups.cs
/// L29).
class ControlGroups final : public TraitBase,
                            public IControlGroups,
                            public ITick,
                            public IGameSaveTraitData {
 public:
  static constexpr int kDefaultGroupCount = 10;  // Info.Groups 默认

  explicit ControlGroups(World& world,
                         std::vector<std::string> groups =
                             {"1", "2", "3", "4", "5", "6", "7", "8", "9",
                              "0"});  // ctor(L36-41)

  ORA_TRAIT_INTERFACES(ControlGroups, OpenRA_Mods_Common_Traits_ControlGroups,
                       IControlGroups, ITick, IGameSaveTraitData)

  const std::vector<std::string>& Groups() const override { return groups_; }  // L32

  void SelectControlGroup(int group) override;               // L43-46
  void CreateControlGroup(int group) override;               // L48-58
  void AddSelectionToControlGroup(int group) override;       // L60-68
  void CombineSelectionWithControlGroup(int group) override; // L70-73
  void AddToControlGroup(Actor* a, int group) override;      // L75-79
  void RemoveFromControlGroup(Actor* a) override;            // L81-86
  std::optional<int> GetControlGroupForActor(Actor* a) override;  // L88-95
  std::vector<Actor*> GetActorsInControlGroup(int group) override;  // L103-106

  void Tick(Actor& self) override;  // L108-115

  std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) override;      // L117-134
  void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) override;   // L136-148

 private:
  /// RemoveActorsFromAllControlGroups(L97-101) | (L97-101).
  void RemoveActorsFromAllControlGroups(std::span<Actor* const> actors);

  World& world_;
  std::vector<std::string> groups_;                    // L32
  std::vector<std::vector<Actor*>> vec_control_groups_;  // L34
};

}  // namespace ora::sim
