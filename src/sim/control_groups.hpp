#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class World;

class ControlGroups final : public TraitBase,
                            public IControlGroups,
                            public ITick,
                            public IGameSaveTraitData {
 public:
  static constexpr int kDefaultGroupCount = 10;

  explicit ControlGroups(World& world,
                         std::vector<std::string> groups =
                             {"1", "2", "3", "4", "5", "6", "7", "8", "9",
                              "0"});

  ORA_TRAIT_INTERFACES(ControlGroups, OpenRA_Mods_Common_Traits_ControlGroups,
                       IControlGroups, ITick, IGameSaveTraitData)

  const std::vector<std::string>& Groups() const override { return groups_; }

  void SelectControlGroup(int group) override;
  void CreateControlGroup(int group) override;
  void AddSelectionToControlGroup(int group) override;
  void CombineSelectionWithControlGroup(int group) override;
  void AddToControlGroup(Actor* a, int group) override;
  void RemoveFromControlGroup(Actor* a) override;
  std::optional<int> GetControlGroupForActor(Actor* a) override;
  std::vector<Actor*> GetActorsInControlGroup(int group) override;

  void Tick(Actor& self) override;

  std::vector<yaml::MiniYamlNode> IssueTraitData(Actor& self) override;
  void ResolveTraitData(Actor& self, const yaml::MiniYaml& data) override;

 private:

  void RemoveActorsFromAllControlGroups(std::span<Actor* const> actors);

  World& world_;
  std::vector<std::string> groups_;
  std::vector<std::vector<Actor*>> vec_control_groups_;
};

}
